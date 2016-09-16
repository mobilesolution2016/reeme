static void doTableClone(lua_State* L, int src, int dst, int deepClone)
{
	if (dst == -1)
	{
		lua_newtable(L);
		dst = lua_gettop(L);
	}

	lua_pushnil(L);
	while (lua_next(L, src))
	{
		lua_pushvalue(L, -2);
		if (deepClone && lua_istable(L, -2))
			doTableClone(L, lua_gettop(L) - 1, -1, 1);
		else
			lua_pushvalue(L, -2);
		lua_rawset(L, dst);
		lua_pop(L, 1);
	}
}
static int lua_table_clone(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);	

	if (lua_istable(L, 2))
	{
		doTableClone(L, 2, 1, lua_isboolean(L, 3) ? lua_toboolean(L, 3) : 0);
		lua_pushvalue(L, 2);		
	}
	else
	{
		doTableClone(L, 1, -1, lua_isboolean(L, 2) ? lua_toboolean(L, 2) : 0);
	}

	return 1;
}

static void doTableExtend(lua_State* L, int src, int dst)
{
	lua_pushnil(L);
	while (lua_next(L, src))
	{
		if (lua_istable(L, -1))
		{
			int newsrc = lua_gettop(L);

			lua_pushvalue(L, -2);
			lua_gettable(L, 1);
			if (!lua_istable(L, -1))
			{
				lua_pushvalue(L, -3);
				lua_createtable(L, lua_objlen(L, newsrc), 4);

				doTableExtend(L, newsrc, newsrc + 3);
				lua_settable(L, dst);				
			}
			else
			{
				doTableExtend(L, newsrc, newsrc + 1);
			}
			lua_pop(L, 2);
		}
		else
		{
			lua_pushvalue(L, -2);
			lua_pushvalue(L, -2);
			lua_settable(L, dst);
			lua_pop(L, 1);
		}		
	}
}
static int lua_table_extend(lua_State* L)
{
	int n = lua_gettop(L);
	luaL_checktype(L, 1, LUA_TTABLE);

	for (int i = 2; i <= n; ++ i)
	{
		if (!lua_istable(L, i))
			continue;

		doTableExtend(L, i, 1);
	}

	lua_pushvalue(L, 1);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_table_filter(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	int tp = lua_type(L, 2), top = -1;

	if (tp == LUA_TSTRING && (!lua_isboolean(L, 3) || !lua_toboolean(L, 3)))
	{
		lua_newtable(L);
		top = lua_gettop(L);
	}

	if (tp == LUA_TSTRING)
	{
		// 将字符串按照 , 分解，以每一个名称做为key去查找
		size_t len = 0;
		const char* keys = lua_tolstring(L, 2, &len);
		if (!keys || len < 1)
			return 1;

		int cc = 0;
		const char* endp, *ptr = keys, *ptrend = keys + len;
		for(;;)
		{
			while (ptr < ptrend)
			{
				if ((uint8_t)ptr[0] > 32)
					break;
				ptr ++;
			}

			endp = (const char*)memchr(ptr, ',', len - (ptr - keys));
			if (!endp)
				break;

			lua_pushlstring(L, ptr, endp - ptr);
			if (top != -1)
			{				
				lua_pushvalue(L, -1);
				lua_gettable(L, 1);
				lua_rawset(L, top);
			}
			else
			{
				lua_gettable(L, 1);
			}
			ptr = endp + 1;
			cc ++;
		}

		if (ptr < ptrend)
		{
			// 最后一组
			lua_pushlstring(L, ptr, ptrend - ptr);
			if (top != -1)
			{
				lua_pushvalue(L, -1);
				lua_gettable(L, 1);
				lua_rawset(L, top);
			}
			else
			{
				lua_gettable(L, 1);
			}
			cc ++;
		}

		if (top == -1)
			return cc;
	}
	else if (tp == LUA_TTABLE)
	{
		// table中的每一个key或值用于去查找
		lua_pushnil(L);
		while (lua_next(L, 2))
		{
			if (lua_isstring(L, -1) && lua_isnumber(L, -2))
			{
				// 值是字符串，用值去查找
				lua_pushvalue(L, -1);
				lua_pushvalue(L, -1);
				lua_gettable(L, 1);
				lua_rawset(L, top);
			}
			else if (lua_isstring(L, -2))
			{
				// 键是字符串，用键去查找
				lua_pushvalue(L, -2);
				lua_pushvalue(L, -1);
				lua_rawget(L, 1);
				lua_rawset(L, top);
			}

			lua_pop(L, 1);
		}
	}
	else if (tp == LUA_TFUNCTION)
	{
		// 对源table中的每一个值都调用函数，当函数返回true时保留这个值
		lua_pushnil(L);
		int pop = lua_gettop(L);

		while (lua_next(L, 1))
		{
			lua_pushvalue(L, 2);
			lua_pushvalue(L, -3);
			lua_pushvalue(L, -3);
			if (lua_pcall(L, 2, 1, 0) == 0 && lua_isboolean(L, -1))
			{
				lua_pushvalue(L, -3);
				lua_pushvalue(L, -3);
				lua_rawset(L, top);
			}

			lua_settop(L, pop);
		}
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_table_val2key(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_createtable(L, 0, lua_objlen(L, 1));
	int idx = lua_gettop(L);

	lua_pushnil(L);
	while (lua_next(L, 1))
	{
		lua_pushvalue(L, -1);
		lua_pushvalue(L, -3);
		lua_rawset(L, idx);
		lua_pop(L, 1);
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_table_new(lua_State* L)
{
	int t = lua_gettop(L), narr = 4, nrec = 0;

	if (t == 2)
	{
		narr = luaL_optinteger(L, 1, 4);
		nrec = luaL_optinteger(L, 2, 0);
	}
	else if (t == 1)
	{
		narr = luaL_optinteger(L, 1, 4);
	}

	lua_createtable(L, narr, nrec);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_table_in(lua_State* L)
{
	int r = 0;
	if (lua_gettop(L) >= 2)
	{
		luaL_checktype(L, 1, LUA_TTABLE);

		lua_pushnil(L);
		while (lua_next(L, 1))
		{
			if (lua_equal(L, -1, 2))
			{
				r = 1;
				break;
			}
			lua_pop(L, 1);
		}
	}

	lua_pushboolean(L, r);
	return 1;
}

static int lua_table_rawin(lua_State* L)
{
	int r = 0;
	if (lua_gettop(L) >= 2)
	{
		luaL_checktype(L, 1, LUA_TTABLE);

		lua_pushnil(L);
		while (lua_next(L, 1))
		{
			if (lua_rawequal(L, -1, 2))
			{
				r = 1;
				break;
			}
			lua_pop(L, 1);
		}
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
typedef MemoryBlockRW<MemoryAlignCheck4> LuaValueMemoryBlock;

inline size_t _keepTextalignbytes(size_t leng)
{
	if (leng & 3)
		return ((leng >> 2) + 1) << 2;	// 本来就不是4的整数倍，所以后面有至少1个字节以上的NULL结尾符空间，因此没有必要多加
	return leng + 4;		// 4的整数倍结尾，因此需要留出一个4字节的空间，以保证后面的数据还是4字节对齐的
}
static size_t luaValueStatSize(lua_State* L, int idx, bool bIsTable, size_t* pnCount)
{	
	size_t len;
	int tp, n = idx;
	size_t s = 0, cc = 0, tt;

	if (bIsTable)
		lua_pushnil(L);

	for( ; ; ++ n)
	{
		if (bIsTable)
		{
			if (!lua_next(L, idx))
				break;
			tp = lua_type(L, n = -1);

			switch (lua_type(L, -2))
			{
			case LUA_TNUMBER:
				s += sizeof(double) + sizeof(int);
				break;
			case LUA_TSTRING:
				lua_tolstring(L, -2, &len);
				s += alignbytes<size_t>(len) + sizeof(size_t) + sizeof(int);
				break;
			}
		}
		else
		{
			tp = lua_type(L, n);
			if (tp == LUA_TNONE)
				break;
		}

		switch(tp)
		{
		case LUA_TNIL: s += sizeof(int); cc ++; break;
		case LUA_TBOOLEAN: s += sizeof(int) * 2; cc ++; break;
		case LUA_TNUMBER: s += sizeof(double) + sizeof(int); cc ++; break;
		case LUA_TSTRING: lua_tolstring(L, n, &len); s += alignbytes<size_t>(len) + sizeof(size_t) + sizeof(int); cc ++; break;
		case LUA_TLIGHTUSERDATA: s += sizeof(void*) + sizeof(int); cc ++; break;
		case LUA_TTABLE:
			cc ++;
			tt = luaValueStatSize(L, bIsTable ? lua_gettop(L) : n, true, 0) + sizeof(int) * 2;
			if (tt == -1)
				return -1;
			s += tt;
			break;
		}

		if (bIsTable)
			lua_pop(L, 1);
	}

	if (pnCount)
		*pnCount = cc;
	return s;
}
static void luaValueCopyToBuf_String(lua_State* L, int n, LuaValueMemoryBlock& block)
{
	size_t len;
	const char* s = lua_tolstring(L, n, &len);

	block.writeval((size_t)len);
	block.write(s, len);
	block.fillzero(alignbytes<size_t>(len) -len);
}
static size_t luaValueCopyToBuf(lua_State* L, int idx, bool bIsTable, LuaValueMemoryBlock& block, size_t cc = 0xFFFFFFFF)
{	
	size_t i, k;
	size_t len, curpos;
	int tp, n = idx;

	if (bIsTable)
		lua_pushnil(L);

	for (i = 0; i < cc; ++ n, ++ i)
	{
		if (bIsTable)
		{
			if (!lua_next(L, idx))
				break;
			tp = lua_type(L, n = -1);

			curpos = block.size();
			block.move(sizeof(int));

			switch (lua_type(L, -2))
			{
			case LUA_TNUMBER:
				block.writeval(tp, curpos);
				block.writeval(lua_tonumber(L, -2));
				break;
			case LUA_TSTRING:
				block.writeval(tp | 0x8000, curpos);
				luaValueCopyToBuf_String(L, -2, block);
				break;
			default:
				luaL_error(L, "the key must be number or string when copy table parameter between thread");
				return -1;
			}
		}
		else
		{
			tp = lua_type(L, n);
			if (tp == LUA_TNONE)
				break;

			block.writeval(tp);
		}

		switch (tp)
		{
		case LUA_TNUMBER:
			block.writeval(lua_tonumber(L, n));
			break;

		case LUA_TBOOLEAN:
			block.writeval(lua_toboolean(L, n));
			break;

		case LUA_TCDATA:
			break;

		case LUA_TSTRING:
			luaValueCopyToBuf_String(L, n, block);
			break;

		case LUA_TTABLE:
			curpos = block.size();
			block.move(sizeof(size_t));

			k = luaValueCopyToBuf(L, bIsTable ? lua_gettop(L) : n, true, block);
			if (k == -1)
				return -1;

			block.writeval(k, curpos);
			break;

		case LUA_TLIGHTUSERDATA:
			block.writeval(lua_touserdata(L, n));
			break;
		}

		if (bIsTable)
			lua_pop(L, 1);
	}

	return i;
}
static void luaValueFromBuf(lua_State* L, LuaValueMemoryBlock& block, bool bIsTable, size_t cc, std::list<void*>* pAllUserdatas = 0)
{
	void* p;
	size_t len, realcc = 0;

	for(size_t i = 0; i < cc; ++ i)
	{
		int tp = block.readval<int>();
		if (bIsTable)
		{
			if (tp & 0x8000)
			{
				len = block.readval<int>();
				lua_pushlstring(L, block, len);
				block.move(alignbytes<size_t>(len));
			}
			else
			{
				lua_pushnumber(L, block.readval<double>());
			}
		}

		switch (tp & 0xFF)
		{
		default: lua_pushnil(L); realcc++; break;
		case LUA_TNUMBER: lua_pushnumber(L, block.readval<double>()); realcc ++; break;
		case LUA_TBOOLEAN: lua_pushboolean(L, block.readval<int>()); realcc ++; break;
		case LUA_TSTRING:
			len = block.readval<size_t>();
			lua_pushlstring(L, block, len);
			block.move(alignbytes<size_t>(len));
			realcc ++;
			break;

		case LUA_TCDATA:
			break;

		case LUA_TLIGHTUSERDATA: lua_pushlightuserdata(L, block.readval<void*>()); realcc ++; break;

		case LUA_TTABLE:
			realcc ++;
			lua_newtable(L);

			len = block.readval<size_t>();
			luaValueFromBuf(L, block, true, len, pAllUserdatas);
			break;
		}

		if (bIsTable)
			lua_rawset(L, -3);
	}

	assert(realcc == cc);
}

static int lua_table_serialize(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	size_t count = 0;
	size_t total = luaValueStatSize(L, 1, true, &count);
	size_t needMemSize = total +  + sizeof(size_t) * 2;

	int t = lua_type(L, 2);
	bool doserialize = false, compressit = false, asCData = false, asString = false;

	if (t == LUA_TSTRING)
	{
		// 参数2为标志
		size_t len = 0;
		const char* rtype = lua_tolstring(L, 2, &len);
		if (rtype && len < 32)
		{
			char tmpstr[40] = { 0 }, *s = 0;
			memcpy(tmpstr, rtype, len);

			const char* token = strtok_s(tmpstr, ",", &s);
			while (token)
			{
				if (strcmp(token, "string") == 0)
					doserialize = asString = true;
				else if (strcmp(token, "cdata") == 0)
					doserialize = asCData = true;
				else if (strcmp(token, "compress") == 0)
					compressit = true;
				token = strtok_s(tmpstr, ",", &s);
			}
		}
	}

	if (doserialize)
	{
		// 如果需要序列化，则在这里分配内存然后处理
		char* mem = (char*)malloc(compressit ? needMemSize : needMemSize * 2);
		size_t* dst = (size_t*)mem;
		char* rmem = mem;

		LuaValueMemoryBlock block(dst + 23, total);
		dst[0] = luaValueCopyToBuf(L, 1, true, block);
		
		if (compressit)
		{
			// 如果需要压缩，则使用ZLib库进行压缩
			rmem = mem + needMemSize;
			dst = (size_t*)rmem;

			dst[1] = ZLibCompress(mem + sizeof(size_t) * 3, total, (char*)(dst + 3), total, 9);
			needMemSize = dst[1] + sizeof(size_t) *  2;
		}
		else
		{
			// 否则标记压缩后的尺寸为0
			dst[1] = 0;
		}
		
		if (asString)
		{
			// 以字符串返回压缩后的数据
			lua_pushlstring(L, rmem, needMemSize);
			return 1;
		}
		
		if (asCData)
		{
			// 以cdata的uint8_t类型返回压缩后的数据
			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
			lua_pushlstring(L, "uint8_t[?]", 10);
			lua_pushinteger(L, needMemSize);
			lua_pcall(L, 2, 1, 0);

			memcpy(const_cast<void*>(lua_topointer(L, -1)), rmem, needMemSize);
			return 1;
		}
	}
	else
	{
		// 仅返回压缩需要用掉的字节数
		lua_pushinteger(L, needMemSize);
		return 1;
	}
	return 0;
}

static int lua_table_unserialize(lua_State* L)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////
static void luaext_table(lua_State *L)
{
	const luaL_Reg procs[] = {
		// 字符串快速替换
		{ "clone", &lua_table_clone },
		// 字符串指定位置+结束位置替换
		{ "extend", &lua_table_extend },
		// 过滤
		{ "filter", &lua_table_filter },
		// value和key互转
		{ "val2key", &lua_table_val2key },
		// 判断值是否在数组中出现
		{ "in", &lua_table_in },
		// 判断值是否在数组中出现
		{ "rawin", &lua_table_rawin },
		// 序列化Table数据
		{ "serialize", &lua_table_serialize },
		// 反序列化
		{ "unserialize", &lua_table_unserialize },

		{ NULL, NULL }
	};

	lua_getglobal(L, "table");

	luaL_register(L, NULL, procs);

	lua_pushliteral(L, "pack");
	lua_rawget(L, -2);
	int t = lua_isnil(L, -1);
	lua_pop(L, 1);

	if (t)
	{
		lua_pushliteral(L, "pack");
		lua_getglobal(L, "pack");
		lua_rawset(L, -3);

		lua_pushliteral(L, "unpack");
		lua_getglobal(L, "unpack");
		lua_rawset(L, -3);
	}

	lua_pushliteral(L, "new");
	lua_rawget(L, -2);
	t = lua_isnil(L, -1);
	lua_pop(L, 1);

	if (t)
	{
		lua_pushliteral(L, "new");
		lua_pushcfunction(L, &lua_table_new);
		lua_rawset(L, -3);
	}

	lua_pop(L, 1);
}