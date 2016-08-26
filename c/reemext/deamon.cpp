#include "reeme.h"
#include "lua.hpp"
#include "deamon.h"
#include "json.h"
#include "lua_string.h"
#include "lua_table.h"
#include "lua_utf8str.h"
#include "lua_deamon.h"
#include "threadpool.hpp"

bool gbRunning = true;

#ifdef _WINDOWS
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(int argc, char* argv[])
#endif
{
	lua_State* L = luaL_newstate();	

	luaext_string(L);
	luaext_table(L);
	luaext_utf8str(L);

	while (gbRunning)
	{

	}

	lua_close(L);
	return 0;
}