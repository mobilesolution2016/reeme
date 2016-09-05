#include "reeme.h"

#include "lua.hpp"
#include "re2/re2.h"
#include "re2/regexp.h"

#include "json.h"
#include "lua_utf8str.h"
#include "lua_string.h"
#include "lua_table.h"
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

// 将参数1转换为布尔值，参数2可以为true表示检测严格，如果不设置或为false表示不严格检测，在不严格检测中，所有的数值型字符串或只要是非0的值或任何非nil的值都被认为是true
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
		char *endp = 0;
		size_t len = 0;
		const char* s = lua_tolstring(L, 1, &len);

		if (len == 4 && stricmp(s, "true") == 0)
			r = cc = 1;
		else if (len = 5 && stricmp(s, "false") == 0)
			cc = 1;
		else 
		{
			long v = strtol(s, &endp, 10);
			if (endp - s == len)
			{
				cc = 1;
				r = v ? 1 : 0;
			}
		}
	}
		break;

	case LUA_TBOOLEAN:
		lua_pushvalue(L, 1);
		return 1;

	case LUA_TNIL:
		cc = 1;
		break;

	default:
		if (!strict)
			cc = 1;
		break;
	}

	lua_pushboolean(L, r);
	return cc;
}

// 检测是否是userdata的NULL，如果是，则返回参数2或true(当没有参数2时)，或不是，直接返回false
static int lua_checknull(lua_State* L)
{
	int t = lua_type(L, 1);
	int top = lua_gettop(L);

	if ((t == LUA_TUSERDATA || t == LUA_TLIGHTUSERDATA) && lua_touserdata(L, 1) == NULL)
	{
		if (top >= 2)
			lua_pushvalue(L, 2);
		else
			lua_pushboolean(L, 1);
	}
	else
	{
		lua_pushboolean(L, 0);
	}
	return 1;
}

// 判断参数1的值是否在接下来的所有参数中有一个相等的，如果有则返回相等的那个位置的参数的序号(最小序号是2，因为用于比对的参数从第2个开始)，如果没有，则返回nil
static int lua_hasequal(lua_State* L)
{
	int n = 2, top = lua_gettop(L);
	while (n <= top)
	{
		if (lua_equal(L, 1, n))
		{
			lua_pushinteger(L, n);
			return 1;
		}
		++ n;
	}

	return 0;
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

	lua_pushcfunction(L, &lua_hasequal);
	lua_setglobal(L, "hasequal");

	int r = luaL_dostring(L, initcodes);
	assert(r == 0);

	luaL_newmetatable(L, "REEME_C_EXTLIB");
	luaL_register(L, NULL, cExtProcs);
	lua_pop(L, 1);

	return 1;
}