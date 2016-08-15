#include "reeme.h"
#include "lua.hpp"
#include "lua_string.h"

//////////////////////////////////////////////////////////////////////////
REEME_API int luaopen_reemext(lua_State* L)
{
	luaext_string(L);
	return 1;
}