static int lua_sql_expression_parse(lua_State* L)
{
	enum TokenChecker {
		TNone,
		TName,
		TString,
	};

	uint8_t m;	
	TokenChecker kToken = TNone;
	size_t i, len = 0, prevpos = 0;
	const char* sql = luaL_checklstring(L, 1, &len);
	int cc = 0, top = lua_gettop(L), r1 = 2, r2 = 3;
	
	if (top == 1)
	{
		lua_createtable(L, 4, 0);
		r1 = lua_gettop(L);

		lua_createtable(L, 4, 0);
		r2 = r1 + 1;
	}

	for (i = 0; i < len; ++ i)
	{
		char ch = sql[i];
		switch (kToken)
		{
		case TNone:
			m = sql_where_splits[ch];
			if (m == 2)
			{
				prevpos = i;
				kToken = TName;
			}
			else if (ch == '\'')
			{
				prevpos = i;
				kToken = TString;
			}
			else if (ch == '*')
			{
				prevpos = i;

				lua_pushlstring(L, "*", 1);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, i + 1);
				lua_rawseti(L, r2, cc);
			}
			break;

		case TName:
			m = sql_where_splits[ch];
			if (m == 1)
			{
				lua_pushlstring(L, sql + prevpos, i - prevpos);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, prevpos + 1);
				lua_rawseti(L, r2, cc);

				kToken = TNone;
			}
			break;

		case TString:
			if (ch == '\\')
				++ i;
			else if (ch == '\'')
			{
				lua_pushlstring(L, sql + prevpos, i - prevpos + 1);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, prevpos + 1);
				lua_rawseti(L, r2, cc);

				kToken = TNone;
			}
			break;
		}
	}

	if (kToken == TString)
		return 0;	// error

	if (kToken == TName)
	{
		lua_pushlstring(L, sql + prevpos, len - prevpos);
		lua_rawseti(L, r1, ++ cc);

		lua_pushinteger(L, prevpos + 1);
		lua_rawseti(L, r2, cc);
	}

	if (top == 3)
	{
		lua_pushinteger(L, cc);
		return 3;
	}

	return 2;
}

//////////////////////////////////////////////////////////////////////////
// 寻找一个token（忽略字符串和除了.-_之外的所有符号）
static int lua_sql_findtoken(lua_State* L)
{
	size_t i, len = 0, addonLen = 0;
	const char* s = luaL_checklstring(L, 1, &len);
	ptrdiff_t off = luaL_optinteger(L, 2, 1) - 1;

	if (len < 1 || off < 0 || off >= len)
		return 0;

	uint8_t addons[128] = { 0 };
	const char* addon = luaL_optlstring(L, 3, "", &addonLen);
	for(i = 0; i < addonLen; ++ i)
		addons[(uint8_t)addon[i]] = 1;

	char inString = 0;
	for(i = off; i < len; ++ i)
	{
		uint8_t ch = s[i];
		if (inString)
		{
			if (ch == '\\')
				++ i;
			else if (ch == inString)
				off = i + 1;
			continue;
		}

		if (ch <= 32 || ch >= 128)
		{
			// 跳过不可见字符和非ANSI字符
			if (i > off)
				goto _return;
			off = i + 1;
			continue;
		}

		if (ch == '*')
		{
			// *号单独处理
			lua_pushlstring(L, "*", 1);
			lua_pushinteger(L, i + 1);
			return 2;
		}

		if (ch == '\'' || ch == '"')
		{
			// 标记字符串
			inString = ch;
			continue;
		}

		if (sql_where_splits[ch] == 1)
		{
			// 不算在token范围内的符号
			if (i <= off)
				off = i + 1;	// token的有效长度为0
			else if (!addons[ch])
				goto _return;	// 同时也不是附加有效范围的符号，这个时候又拥有有效长度，那么就可以停止了
			continue;
		}
	}

	if (i > off && !inString)
	{
	_return:
		lua_pushlstring(L, s + off, i - off);
		lua_pushinteger(L, i + 1);
		return 2;
	}
	return 0;
}