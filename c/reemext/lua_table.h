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
static void luaext_table(lua_State *L)
{
	const luaL_Reg procs[] = {
		// ×Ö·û´®¿ìËÙÌæ»»
		{ "clone", &lua_table_clone },
		// ×Ö·û´®Ö¸¶¨Î»ÖÃ+½áÊøÎ»ÖÃÌæ»»
		{ "extend", &lua_table_extend },

		{ NULL, NULL }
	};


	lua_getglobal(L, "table");

	luaL_register(L, NULL, procs);

	lua_pop(L, 1);
}