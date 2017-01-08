static int lua_sql_token_parse(lua_State* L)
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
			else if (m == 4)
			{
				if (ch == '\'')
				{
					prevpos = i;
					kToken = TString;
				}
				else if (ch == '?')
				{
					prevpos = i;
					while(sql[i + 1] == 3)
						i ++;

					lua_pushlstring(L, sql + prevpos, i - prevpos + 1);
					lua_rawseti(L, r1, ++ cc);

					lua_pushinteger(L, prevpos + 1);
					lua_rawseti(L, r2, cc);
				}
				else if (ch == '*')
				{
					prevpos = i;

					lua_pushlstring(L, "*", 1);
					lua_rawseti(L, r1, ++ cc);

					lua_pushinteger(L, i + 1);
					lua_rawseti(L, r2, cc);
				}
			}
			else if (m >= 5)
			{
				// < <= > >= = != �⼸����ʽ����
_eq_sym:
				prevpos = i;
				if (m == 6 && sql[i + 1] == '=')
					i ++;

				lua_pushlstring(L, sql + prevpos, i - prevpos + 1);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, prevpos + 1);
				lua_rawseti(L, r2, cc);
			}
			break;

		case TName:
			m = sql_where_splits[ch];
			if (m != 2 && m != 3)
			{
				lua_pushlstring(L, sql + prevpos, i - prevpos);
				lua_rawseti(L, r1, ++ cc);

				lua_pushinteger(L, prevpos + 1);
				lua_rawseti(L, r2, cc);

				kToken = TNone;
				if (m >= 5)
					goto _eq_sym;
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
// Ѱ��һ��token�������ַ����ͳ���.-_֮������з��ţ�
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
			// �������ɼ��ַ��ͷ�ANSI�ַ�
			if (i > off)
				goto _return;
			off = i + 1;
			continue;
		}

		if (ch == '*')
		{
			// *�ŵ�������
			lua_pushlstring(L, "*", 1);
			lua_pushinteger(L, i + 1);
			return 2;
		}

		if (ch == '\'' || ch == '"')
		{
			// ����ַ���
			inString = ch;
			continue;
		}

		if (sql_where_splits[ch] == 1)
		{
			// ������token��Χ�ڵķ���
			if (i <= off)
				off = i + 1;	// token����Ч����Ϊ0
			else if (!addons[ch])
				goto _return;	// ͬʱҲ���Ǹ�����Ч��Χ�ķ��ţ����ʱ����ӵ����Ч���ȣ���ô�Ϳ���ֹͣ��
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