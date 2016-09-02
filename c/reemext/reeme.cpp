#include "reeme.h"
#include "lua.hpp"
#include "json.h"
#include "lua_string.h"
#include "lua_table.h"
#include "lua_utf8str.h"
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

static int lua_toboolean(lua_State* L)
{
	int strict = lua_toboolean(L, 2);
	int t = lua_type(L, 1), r = 0, cc = 0;

	switch (t)
	{
	case LUA_TNUMBER:
	{
		double v = lua_tonumber(L, 1);
		if (v == 0)
			r = 0;
		else if (v == 1 || !strict)
			r = 1;
		cc = 1;
	}
	break;

	case LUA_TSTRING:
	{
		size_t len = 0;
		const char* s = lua_tolstring(L, 1, &len);

		if (len == 4 && stricmp(s, "true") == 0)
			r = cc = 1;
		else if (len = 5 && stricmp(s, "false") == 0)
			cc = 1;
		else if (len == 1)
		{
			if (s[0] == '0') cc = 1;
			else if (s[0] == '1') r = cc = 1;
		}
	}
		break;

	case LUA_TBOOLEAN:
		lua_pushvalue(L, 1);
		return 1;

	default:
		if (!strict)
			cc = 1;
		break;
	}

	lua_pushboolean(L, r);
	return cc;
}

static int lua_checknull(lua_State* L)
{
	int t = lua_type(L, 1);
	if ((t == LUA_TUSERDATA || t == LUA_TLIGHTUSERDATA) && lua_touserdata(L, 1) == NULL)
		lua_pushvalue(L, 2);
	else
		lua_pushvalue(L, 1);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
const char initcodes[] = {
	"table.unique = function(tbl)\n"
	"	local cc = #tbl\n"
	"	local s, r = table.new(0, cc), table.new(cc, 0)\n"
	"	for i = 1, cc do\n"
	"		s[tbl[i]] = true\n"
	"	end\n"
	"	local i = 1\n"
	"	for k,_ in pairs(s) do\n"
	"		r[i] = k\n"
	"		i = i + 1\n"
	"	end\n"
	"	return r\n"
	"end\n"
};

//////////////////////////////////////////////////////////////////////////
REEME_API int luaopen_reemext(lua_State* L)
{
	luaext_string(L);
	luaext_table(L);
	luaext_utf8str(L);

	lua_pushcfunction(L, &lua_findmetatable);
	lua_setglobal(L, "findmetatable");

	lua_pushcfunction(L, &lua_toboolean);
	lua_setglobal(L, "toboolean");

	lua_pushcfunction(L, &lua_checknull);
	lua_setglobal(L, "checknull");

	int r = luaL_dostring(L, initcodes);
	assert(r == 0);

	luaL_newmetatable(L, "REEME_C_EXTLIB");
	luaL_register(L, NULL, cExtProcs);
	lua_pop(L, 1);

	return 1;
}