static int lua_string_split(lua_State* L)
{	
	bool retAsTable = false;
	int top = lua_gettop(L);
	uint8_t maskBits[32] = { 0 }, ch;
	size_t byLen = 0, srcLen = 0, start = 0, pos;

	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);
	const uint8_t* by = (const uint8_t*)luaL_checklstring(L, 2, &byLen);

	uint32_t nFlags = lua_tointeger(L, 2);
	uint32_t maxSplits = nFlags & 0x0FFFFFFF, cc = 0;

	if (top >= 3 && lua_toboolean(L, 3))
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
	
	for (size_t i = 0, endpos; i < srcLen; ++ i)
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
static void luaext_string(lua_State *L)
{
	lua_getglobal(L, "string");

	// Boyer-Moore×Ó´®±àÒë
	lua_pushlstring(L, "split", 5);
	lua_pushcfunction(L, &lua_string_split);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_ASKEY");
	lua_rawseti(L, -2, 0x40000000);

	lua_pushliteral(L, "SPLIT_TRIM");
	lua_rawseti(L, -2, 0x20000000);

	lua_pop(L, 1);
}