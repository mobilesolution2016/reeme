static int lua_sql_expression_parse(lua_State* L)
{
	enum TokenChecker {
		TNone,
		TName,
		TString,
	};

	uint8_t m;
	int cc = 0, slashes = 0;
	TokenChecker kToken = TNone;
	size_t i, len = 0, prevpos = 0;
	const char* sql = luaL_checklstring(L, 1, &len);

	lua_newtable(L);
	int r1 = lua_gettop(L);

	lua_newtable(L);
	int r2 = r1 + 1;

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
			break;

		case TName:
			m = sql_where_splits[ch];
			if (m != 2 && m != 3 && ch != '.')
			{
				lua_pushlstring(L, sql + prevpos, i - prevpos);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, prevpos + 1);
				lua_rawseti(L, r2, cc);

				kToken = TNone;
			}
			break;

		case TString:
			if (slashes)
				slashes = 0;
			else if (ch == '\\')
				slashes = 1;
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

	return 2;
}