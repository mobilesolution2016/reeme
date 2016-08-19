// 下面是ASCII字符属性表，1表示符号，2表示大小写字母，3表示数字，4表示可以用于组合整数或小数的符号
static uint8_t sql_where_splits[128] = 
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	1,		// 0~32
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 1,	// 33~47
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	// 48~57
	1, 1, 1, 1, 1, 1, 1,	// 58~64
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// 65~92
	1, 1, 1, 1, 2, 1,	// 91~96
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// 97~122
	1, 1, 1, 1, 1,
};


//////////////////////////////////////////////////////////////////////////
static int lua_string_split(lua_State* L)
{	
	bool retAsTable = false;
	int top = lua_gettop(L);
	uint8_t checker[256] = { 0 }, ch;
	size_t byLen = 0, srcLen = 0, start = 0;

	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);
	const uint8_t* by = (const uint8_t*)luaL_checklstring(L, 2, &byLen);

	uint32_t nFlags = luaL_checklong(L, 3);
	uint32_t maxSplits = nFlags & 0x0FFFFFFF, cc = 0;

	if (top >= 4 && lua_toboolean(L, 4))
	{
		retAsTable = true;
		lua_newtable(L);
	}

	if (byLen > srcLen)
		return retAsTable ? 1 : 0;

	while ((ch = *by) != 0)
	{
		checker[ch] = 1;
		by ++;
	}

	if (maxSplits == 0)
		maxSplits = 0x0FFFFFFF;
	
	size_t i, endpos;
	for (i = endpos = 0; i < srcLen; ++ i)
	{
		ch = src[i];
		if (!checker[ch])
			continue;

		endpos = i;
		if (nFlags & 0x40000000)
		{
			// trim
			while(start < endpos)
			{
				if (src[start] > 32)
					break;
				start ++;
			}
			while(start < endpos)
			{
				if (src[endpos - 1] > 32)
					break;
				endpos --;
			}
		}

		if (start < endpos)
		{
_lastseg:
			// push result
			if (retAsTable)
			{
				lua_pushlstring(L, (const char*)src + start, endpos - start);
				if (nFlags & 0x40000000)
				{
					// as key
					lua_pushlstring(L, "", 0);
					lua_rawset(L, top + 1);
				}
				else
				{
					// as array element
					lua_rawseti(L, top + 1, cc);
				}
			}
			else
			{
				lua_pushlstring(L, (const char*)src + start, endpos - start);
			}

			cc ++;
			if (cc >= maxSplits)
				break;
		}

		start = i + 1;
	}

	if (maxSplits && start < srcLen)
	{
		endpos = srcLen;
		maxSplits = 0;
		goto _lastseg;
	}

	if (cc == 0)
	{
		// no one
		lua_pushvalue(L, 1);
		if (retAsTable)
		{
			if (nFlags & 0x40000000)
			{
				// as key
				lua_pushlstring(L, "", 0);
				lua_rawset(L, top + 1);
			}
			else
			{
				// as array element
				lua_rawseti(L, top + 1, cc);
			}
		}

		return 1;
	}

	return retAsTable ? 1 : cc;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_trim(lua_State* L)
{
	size_t len = 0;
	int triml = 1, trimr = 1;
	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &len);
	
	if (lua_isboolean(L, 2))
		triml = lua_toboolean(L, 2);
	if (lua_isboolean(L, 3))
		trimr = lua_toboolean(L, 3);

	const uint8_t* left = src;
	const uint8_t* right = src + len;
	if (triml)
	{
		while(left < right)
		{
			if (left[0] > 32)
				break;
			left ++;
		}
	}
	if (trimr)
	{
		while (left < right)
		{
			if (*(right - 1) > 32)
				break;
			right --;
		}
	}

	if (right - left == len)
		lua_pushvalue(L, 1);
	else
		lua_pushlstring(L, (const char*)left, right - left);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_cmp(lua_State* L)
{	
	int ignoreCase = 0, r = 0;
	size_t alen = 0, blen = 0, cmplen = -1;
	const char* a = luaL_checklstring(L, 1, &alen);
	const char* b = luaL_checklstring(L, 2, &blen);

	if (lua_isnumber(L, 3))
	{
		cmplen = luaL_checklong(L, 3);
		if (lua_isboolean(L, 4))
			ignoreCase = lua_toboolean(L, 4);
	}
	else if (lua_isboolean(L, 3))
	{
		ignoreCase = lua_toboolean(L, 3);
	}

	if (ignoreCase)
	{
		if (cmplen == -1)
			r = alen == blen ? stricmp(a, b) == 0 : 0;
		else if (cmplen > alen || cmplen > blen)
			r = 0;
		else
			r = strnicmp(a, b, cmplen) == 0;
	}
	else
	{
		if (cmplen == -1)
			r = alen == blen ? strcmp(a, b) == 0 : 0;
		else if (cmplen > alen || cmplen > blen)
			r = 0;
		else
			r = strncmp(a, b, cmplen) == 0;
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static void lua_string_addbuf(luaL_Buffer* buf, const char* str, size_t len)
{
	size_t lenleft = len, copy;
	while(lenleft > 0)
	{
		copy = std::min(lenleft, (size_t)(LUAL_BUFFERSIZE - (buf->p - buf->buffer)));
		if (!copy)
		{
			luaL_prepbuffer(buf);
			copy = std::min(lenleft, (size_t)LUAL_BUFFERSIZE);
		}

		memcpy(buf->p, str, copy);
		lenleft -= copy;
		buf->p += copy;
		str += copy;
	}
}
static int lua_string_replace(lua_State* L)
{
	if (lua_gettop(L) < 3)
		return 0;

	luaL_Buffer buf;
	size_t srcLen = 0, fromLen = 0, toLen = 0, offset;
	const char* src = luaL_checklstring(L, 1, &srcLen), *from, *to, *foundPos;

	luaL_buffinit(L, &buf);

	int tp1 = lua_type(L, 2), tp2 = lua_type(L, 3);
	if (tp2 == LUA_TTABLE)
	{
		if (tp1 == LUA_TTABLE)
		{
			// 字符串数组对字符串数组
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM字符串对字符串数组
		}
	}
	else if (tp2 == LUA_TSTRING)
	{
		to = lua_tolstring(L, 3, &toLen);

		if (tp1 == LUA_TSTRING)
		{
			// 字符串对字符串替换
			from = lua_tolstring(L, 2, &fromLen);
			if (fromLen > 0)
			{
				const char* srcptr = src;
				for(;;)
				{
					foundPos = fromLen == 1 ? strchr(srcptr, from[0]) : strstr(srcptr, from);
					if (!foundPos)
						break;

					offset = foundPos - srcptr;
					lua_string_addbuf(&buf, srcptr, offset);
					lua_string_addbuf(&buf, to, toLen);	

					srcptr = foundPos + fromLen;
				}
			
				lua_string_addbuf(&buf, srcptr, srcLen - (srcptr - src));
				luaL_pushresult(&buf);
				return 1;
			}
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM字符串对普通字符串
		}
	}

	lua_pushvalue(L, 1);
	return 1;
}

static int lua_string_subreplaceto(lua_State* L)
{
	size_t srcLen = 0, repLen = 0;
	const char* src = (const char*)luaL_checklstring(L, 1, &srcLen);
	const char* rep = (const char*)luaL_checklstring(L, 2, &repLen);

	if (srcLen < 1)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	long startp = luaL_checklong(L, 3) - 1, endp = srcLen - 1;
	if (lua_isnumber(L, 4))
		endp = lua_tointeger(L, 4);

	if (endp < 0)
		endp = srcLen + endp;
	else if (endp >= srcLen)
		endp = srcLen - 1;
	else
		endp --;

	if (endp < startp)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	if (startp > 1)
		lua_string_addbuf(&buf, src, startp - 1);
	lua_string_addbuf(&buf, rep, repLen);
	lua_string_addbuf(&buf, src + endp + 1, srcLen - endp - 1);

	luaL_pushresult(&buf);
	return 1;
}

static int lua_string_subreplace(lua_State* L)
{
	size_t srcLen = 0, repLen = 0;
	const char* src = (const char*)luaL_checklstring(L, 1, &srcLen);
	const char* rep = (const char*)luaL_checklstring(L, 2, &repLen);

	if (srcLen < 1)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	long startp = luaL_checklong(L, 3) - 1, leng = LONG_MAX;
	if (lua_isnumber(L, 4))
		leng = lua_tointeger(L, 4);

	leng = std::min(leng, (long)(srcLen - startp));
	if (leng < 1)
	{
		lua_pushvalue(L, 1);
		return 0;
	}

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	if (startp > 0)
		lua_string_addbuf(&buf, src, startp);
	lua_string_addbuf(&buf, rep, repLen);
	if (leng)
		lua_string_addbuf(&buf, src + startp + leng, srcLen - startp - leng);

	luaL_pushresult(&buf);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_checknumeric(lua_State* L)
{
	int tp = lua_type(L, 1), r = 0;
	if (tp == LUA_TNUMBER)
	{
		r = 1;
	}
	else if (tp == LUA_TSTRING)
	{
		size_t len;
		char *endp = 0;
		const char* s = (const char*)lua_tolstring(L, 1, &len);

		strtod(s, &endp);
		if (endp && endp - s == len)
			r = 1;
	}

	lua_pushboolean(L, r);
	return 1;
}

static int lua_string_checkinteger(lua_State* L)
{
	int tp = lua_type(L, 1), r = 0;
	if (tp == LUA_TNUMBER)
	{
		r = 1;
	}
	else if (tp == LUA_TSTRING)
	{
		size_t len;
		char *endp = 0;
		const char* s = (const char*)lua_tolstring(L, 1, &len);

		strtoll(s, &endp, 10);
		if (endp && endp - s == len)
			r = 1;
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_template(lua_State* L)
{	
	luaL_Buffer buf;
	luaL_Buffer* pBuf = &buf;
	size_t srcLen = 0;
	char ch, *endpos;
	int idx, n = lua_gettop(L);
	int hasTable = lua_istable(L, 2), tostringIdx = 0;
	const char* src = luaL_checklstring(L, 1, &srcLen), *val;

	if (srcLen < 3)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	luaL_buffinit(L, pBuf);

	size_t i = 0, pos = 0, len, nums = 0, chars = 0, bracketOpened = -1;
	for (i = 0, pos = 0; i < srcLen; ++ i)
	{
		ch = src[i];
		if (bracketOpened != -1)
		{
			if (ch != '}')
			{
				chars ++;
				if (ch >= '0' && ch <= '9')
					nums ++;

				continue;
			}

			bool getVal = false;

			val = 0;
			if (chars == 0)
			{
				// 空引用
			}
			else if (chars == nums)
			{
				// 纯数字，引用后面相应位置的变量
				idx = strtol(src + bracketOpened, &endpos, 10);
				assert(endpos == src + i);

				if (hasTable)
				{
					lua_rawgeti(L, 2, idx);
					idx = -2;
				}

				val = lua_tolstring(L, idx + 1, &chars);
				getVal = true;
			}
			else if (hasTable)
			{
				// 按照变量名来引用
				lua_pushlstring(L, src + bracketOpened, i - bracketOpened);
				lua_rawget(L, 2);

				val = lua_tolstring(L, -1, &chars);
				getVal = true;
			}

			if (getVal)
			{
				if (!val)
				{
					// 非字符串类型的值，使用tostring函数来做转换，然后再获取转换后的值
					if (!tostringIdx)
					{
						idx = lua_gettop(L);
						lua_getglobal(L, "tostring");
						tostringIdx = idx + 1;	
					}
					else
					{
						idx = -3;
					}

					lua_pushvalue(L, tostringIdx);
					lua_pushvalue(L, idx);
					lua_call(L, 1, 1);
					val = lua_tolstring(L, -1, &chars);
				}

				if (chars)
					lua_string_addbuf(pBuf, val, chars);
			}

			pos = i + 1;
			continue;
		}

		if (ch == '{')
		{
			_found:
			len = i - pos;
			if (len)
				lua_string_addbuf(pBuf, src + pos, len);

			if (src[i + 1] == '{')
			{
				// 转义，非变量或关键字	
				luaL_addchar(pBuf, '{');

				++ i;
				pos = i + 1;

				continue;
			}

			bracketOpened = i + 1;
		}
	}

	if (pos < srcLen)
		lua_string_addbuf(pBuf, src + pos, srcLen - pos);

	luaL_pushresult(pBuf);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static void luaext_string(lua_State *L)
{
	const luaL_Reg procs[] = {
		// 字符串切分
		{ "split", &lua_string_split },
		// trim函数
		{ "trim", &lua_string_trim },
		// 字符串比较
		{ "cmp", &lua_string_cmp },
		// 字符串快速替换
		{ "replace", &lua_string_replace },
		// 字符串指定位置+结束位置替换
		{ "subreplaceto", &lua_string_subreplaceto },
		// 字符串指定位置+长度替换
		{ "subreplace", &lua_string_subreplace },
		// 数值+浮点数字符串检测
		{ "checknumeric", &lua_string_checknumeric },
		// 整数字符串检测
		{ "checkinteger", &lua_string_checkinteger },
		// 模板解析（不支持语法和关键字，只能按照变量名来进行替换，速度快）
		{ "template", &lua_string_template },

		{ NULL, NULL }
	};


	lua_getglobal(L, "string");

	// 字符串切分时可用的标志位
	lua_pushliteral(L, "SPLIT_ASKEY");
	lua_pushinteger(L, 0x40000000);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_TRIM");
	lua_pushinteger(L, 0x20000000);
	lua_rawset(L, -3);

	// 所有扩展的函数
	luaL_register(L, NULL, procs);

	lua_pop(L, 1);
}