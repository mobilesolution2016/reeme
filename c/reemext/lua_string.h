static int lua_string_split(lua_State* L)
{	
	bool retAsTable = false;
	int top = lua_gettop(L);
	uint8_t maskBits[32] = { 0 }, ch;
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
		maskBits[ch >> 3] |= 1 << (ch & 7);
		by ++;
	}

	if (maxSplits == 0)
		maxSplits = 0x0FFFFFFF;
	
	size_t i, endpos;
	for (i = endpos = 0; i < srcLen; ++ i)
	{
		ch = src[i];
		if (!(maskBits[ch >> 3] & (1 << (ch & 7))))
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
			// ×Ö·û´®Êý×é¶Ô×Ö·û´®Êý×é
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM×Ö·û´®¶Ô×Ö·û´®Êý×é
		}
	}
	else if (tp2 == LUA_TSTRING)
	{
		to = lua_tolstring(L, 3, &toLen);

		if (tp1 == LUA_TSTRING)
		{
			// ×Ö·û´®¶Ô×Ö·û´®Ìæ»»
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
			// BM×Ö·û´®¶ÔÆÕÍ¨×Ö·û´®
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
static void luaext_string(lua_State *L)
{
	lua_getglobal(L, "string");

	// ×Ö·û´®ÇÐ·Ö
	lua_pushlstring(L, "split", 5);
	lua_pushcfunction(L, &lua_string_split);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_ASKEY");
	lua_pushinteger(L, 0x40000000);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_TRIM");
	lua_pushinteger(L, 0x20000000);
	lua_rawset(L, -3);

	// ×Ö·û´®¿ìËÙÌæ»»
	lua_pushlstring(L, "replace", 7);
	lua_pushcfunction(L, &lua_string_replace);
	lua_rawset(L, -3);

	// ×Ö·û´®Ö¸¶¨Î»ÖÃÌæ»»
	lua_pushlstring(L, "subreplaceto", 12);
	lua_pushcfunction(L, &lua_string_subreplaceto);
	lua_rawset(L, -3);

	lua_pushlstring(L, "subreplace", 10);
	lua_pushcfunction(L, &lua_string_subreplace);
	lua_rawset(L, -3);

	// ×Ö·û´®ÊÇ·ñÊÇÊý×ÖÀàÐÍµÄ¼ì²â
	lua_pushlstring(L, "checknumeric", 12);
	lua_pushcfunction(L, &lua_string_checknumeric);
	lua_rawset(L, -3);

	lua_pushlstring(L, "checkinteger", 12);
	lua_pushcfunction(L, &lua_string_checkinteger);
	lua_rawset(L, -3);

	lua_pop(L, 1);
}