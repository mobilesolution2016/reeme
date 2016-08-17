#include "reeme.h"
#include "lua.hpp"
#include "lua_string.h"
#include "sql.h"

static luaL_Reg cExtProcs[] = {
	{ "sql_expression_parse", &lua_sql_expression_parse },
	{ NULL, NULL }
};

static int lua_findmetatable(lua_State* L)
{
	int r = 0;
	const char* name = luaL_checkstring(L, 1);
	if (name && luaL_newmetatable(L, name) == 0)
		r = 1;
	return r;
}

//////////////////////////////////////////////////////////////////////////
REEME_API int luaopen_reemext(lua_State* L)
{
	luaext_string(L);

	lua_pushcfunction(L, &lua_findmetatable);
	lua_setglobal(L, "findmetatable");

	luaL_newmetatable(L, "REEME_C_EXTLIB");
	luaL_register(L, NULL, cExtProcs);
	lua_pop(L, 1);

	return 1;
}