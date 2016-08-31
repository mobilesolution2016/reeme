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
		doTableClone(L, 1, 2, lua_isboolean(L, 3) ? lua_toboolean(L, 3) : 0);
		lua_pushvalue(L, 2);		
	}
	else
	{
		doTableClone(L, 1, -1, lua_isboolean(L, 2) ? lua_toboolean(L, 2) : 0);
	}

	return 1;
}

static int lua_table_extend(lua_State* L)
{
	int n = lua_gettop(L);

	for (int i = 2; i < n; ++ i)
	{
		if (!lua_istable(L, i))
			continue;

		lua_pushnil(L);
		while (lua_next(L, i))
		{
			lua_pushvalue(L, -2);
			lua_rawget(L, 1);
			if (lua_isnil(L, -1))
			{
				lua_pushvalue(L, -3);
				lua_pushvalue(L, -3);
				lua_rawset(L, 1);
			}
			lua_pop(L, 2);
		}
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

		{ NULL, NULL }
	};


	lua_getglobal(L, "table");

	luaL_register(L, NULL, procs);

	lua_pushliteral(L, "pack");
	lua_rawget(L, -2);
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);

		lua_pushliteral(L, "pack");
		lua_getglobal(L, "pack");
		lua_rawset(L, -3);

		lua_pushliteral(L, "unpack");
		lua_getglobal(L, "unpack");
		lua_rawset(L, -3);
	}

	lua_pop(L, 1);
}