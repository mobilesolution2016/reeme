#ifdef _WINDOWS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#ifndef INVALID_SOCKET
#	define INVALID_SOCKET (SOCKET)(~0)
#endif
#endif

#include "reeme.h"
#include "crtopt.h"

#include "lua.hpp"
//#include "re2/re2.h"
//#include "re2/regexp.h"
#include "zlib/zlib.h"

#include "json.h"
#include "lua_utf8str.h"
#include "lua_string.h"
#include "lua_table.h"
#include "sql.h"

#include "commonlib.h"
#include "packets.h"

static int lua_findmetatable(lua_State* L)
{
	int r = 0;
	const char* name = luaL_checkstring(L, 1);
	if (name && luaL_newmetatable(L, name) == 0)
		r = 1;
	return r;
}

//////////////////////////////////////////////////////////////////////////
static SOCKET gDeamonSock = INVALID_SOCKET;
static char gDeamonHost[128] = { 0 };
static unsigned short gDeamonPort = 0;
static bool gDeamongStarting = false;

#ifdef _WINDOWS
struct _wsastartup
{
	_wsastartup()
	{
		WSADATA wsa;
		WSAStartup(MAKEWORD(2, 0), &wsa);
	}
	~_wsastartup()
	{
		if (gDeamonSock != INVALID_SOCKET)
		{
			shutdown(gDeamonSock, SD_BOTH);
			closesocket(gDeamonSock);
			gDeamonSock = INVALID_SOCKET;
		}
		WSACleanup();
	}
} g_wsastartup;
#endif

static void socketClose()
{
	if (gDeamonSock != INVALID_SOCKET)
	{
		shutdown(gDeamonSock, SD_BOTH);
		closesocket(gDeamonSock);
		gDeamonSock = INVALID_SOCKET;
	}
}
static bool socketConnect()
{
	socketClose();

	SOCKADDR_IN addrSrv;
	gDeamonSock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WINDOWS
	addrSrv.sin_addr.S_un.S_addr = inet_addr(gDeamonHost);
#else
	addrSrv.sin_addr.s_addr = inet_addr(gDeamonHost);
#endif
	addrSrv.sin_port = htons(gDeamonPort);
	addrSrv.sin_family = AF_INET;

	int iSendTimeout = 50, iRecvTimeout = 10 * 1000;
	setsockopt(gDeamonSock, SOL_SOCKET,SO_SNDTIMEO, (char*)&iSendTimeout, sizeof(iSendTimeout));
	setsockopt(gDeamonSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&iRecvTimeout, sizeof(iRecvTimeout));

	if (connect(gDeamonSock, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) != SOCKET_ERROR)
		return true;

	return false;
}

static int connectToDeamon(lua_State* L, void* processHandle, const char* host, unsigned short port)
{
	bool connected = false;

	strcpy(gDeamonHost, host);
	gDeamonPort = port;

	if (processHandle)
	{
		// 先等待100ms左右，等待进程完成初始化，或是重复的启动而退出，最多尝试5次
		for(int i = 0; i < 5; ++ i)
		{
#ifdef _WINDOWS
			DWORD code = 0;
			Sleep(100);
			if (GetExitCodeProcess(processHandle, &code) && code != STILL_ACTIVE)
				break;
#else
#endif

			if (socketConnect())
			{
				connected = true;
				break;
			}
		}
	}
	else if (socketConnect())
	{
		connected = true;
	}

	if (!connected)
		return 0;

	return 1;
}

static int lua_start_deamon(lua_State* L)
{
	if (gDeamonSock != INVALID_SOCKET)
	{
		// 已经连上了
		lua_pushboolean(L, 1);
		return 1;
	}

	// taskdeamon执行文件的路径
	size_t len = 0, pathLen = 0;
	const char* deamonPath = lua_isstring(L, 1) ? luaL_checklstring(L, 1, &pathLen) : 0;
	const char* args = 0, *host = "127.0.0.1";
	unsigned short port = 5918;

	// 启动参数
	int t = lua_type(L, 2), v = 0;
	if (t == LUA_TTABLE)
	{
		// 如果是个table，则编码为json字符串
		lua_getglobal(L, "require");
		lua_pushliteral(L, "cjson.safe");
		lua_pcall(L, 1, 1, 0);
		lua_getfield(L, -1, "encode");
		lua_pushvalue(L, 2);
		lua_pcall(L, 1, 1, 0);

		v = lua_gettop(L);
		if (!lua_isstring(L, v))
			return 0;

		args = lua_tolstring(L, v, &len);
		v = 2;
	}
	else if (t == LUA_TSTRING)
	{
		// 如果是个json字符串，则解码为table，因为需要从中获取参数
		lua_getglobal(L, "string");
		lua_getfield(L, -1, "json");
		lua_pushvalue(L, 2);
		lua_pcall(L, 1, 1, 0);

		if (lua_istable(L, -1))
			v = lua_gettop(L);
	}
	else if (t == LUA_TNIL)
	{
		// 允许为nil，以全部使用默认参数
		t = LUA_TSTRING;
		args = "{}";
		len = 2;
	}
	else
		return 0;

	if (v)
	{
		// 获取必要参数
		lua_getfield(L, v, "listen");
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, "host");
			lua_getfield(L, -2, "port");

			if (lua_isstring(L, -2))
				host = lua_tostring(L, -2);
			if (lua_isnumber(L, -1))
				port = lua_tointeger(L, -1);
		}
	}

	if (gDeamongStarting)
		return 0;
	gDeamongStarting = true;

#ifdef _WINDOWS
	// 在Windows上使用shellexecute来启动taskdeamon.exe
	std::wstring strExe, strCmd;

	int cchPath = MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, 0, 0);
	int cchCmd = MultiByteToWideChar(CP_UTF8, 0, args, len, 0, 0);

	strExe.resize(cchPath);
	strCmd.resize(cchCmd + 5);

	// 如果是参数2输入是字符串的话，则有可能是一个文件名
	if (t == LUA_TSTRING && GetFileAttributesA(args))
		strCmd = L"file:";
	else
		strCmd = L"json:";

	if (cchPath)
		MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, const_cast<wchar_t*>(strExe.c_str()), cchPath + 1);
	MultiByteToWideChar(CP_UTF8, 0, args, len, const_cast<wchar_t*>(strCmd.c_str()) + 5, cchCmd + 1);

	if (pathLen == 0)
	{
		// 没有指定路径，那么就用当前模块所在的路径，相当于默认taskdeamon.exe和reemext.dll是在同一个目录下的
		wchar_t szThisModule[512] = { 0 };
		DWORD cchModule = GetModuleFileNameW(GetModuleHandle(L"reemext"), szThisModule, 512);

		wchar_t* pszModFilename = wcsrchr(szThisModule, L'\\');
		if (pszModFilename)
			pszModFilename[1] = 0;

		strExe = szThisModule;
		std::replace(strExe.begin(), strExe.end(), L'\\', L'/');

		if (strExe[strExe.length() - 1] != '/')
			strExe += '/';
		strExe += L"taskdeamon.exe";
	}

	SHELLEXECUTEINFOW sh = { 0 };
	sh.cbSize = sizeof(sh);
	sh.fMask = SEE_MASK_FLAG_NO_UI;
	sh.lpFile = strExe.c_str();
	sh.lpParameters = strCmd.c_str();
	sh.nShow = SW_SHOWNORMAL;
	BOOL r = ShellExecuteExW(&sh);

	if (r)
	{
		r = connectToDeamon(L, sh.hProcess, host, port);
		if (sh.hProcess)
			CloseHandle(sh.hProcess);
	}
#else
	BOOL r = 0;
#endif

	if (!r)
		socketClose();

	gDeamongStarting = false;
	lua_pushboolean(L, r);
	return 1;
}

static int lua_connect_deamon(lua_State* L)
{
	int r = 0;
	const char* host = luaL_optstring(L, 1, "127.0.0.1");
	lua_Integer port = luaL_optinteger(L, 1, 5918);

	strcpy(gDeamonHost, host);
	gDeamonPort = port;

	if (socketConnect())
		r = 1;

	lua_pushboolean(L, r);
	return 1;
}

static int lua_request_deamon(lua_State* L)
{
	if (gDeamonSock == INVALID_SOCKET)
		return luaL_error(L, "task deamon not started", 0);

	size_t len = 0;
	const char* posts = 0;

	int t = lua_type(L, 1), top = lua_gettop(L);
	uint32_t flags = luaL_optinteger(L, 2, 0);

	if (t == LUA_TTABLE)
	{
		lua_getglobal(L, "require");
		lua_pushliteral(L, "cjson.safe");
		lua_pcall(L, 1, 1, 0);
		lua_getfield(L, -1, "encode");
		lua_pushvalue(L, 1);
		lua_pcall(L, 1, 1, 0);

		if (!lua_isstring(L, -1))
			return 0;

		posts = lua_tolstring(L, -1, &len);
	}
	else if (t == LUA_TSTRING)
	{
		posts = lua_tolstring(L, 1, &len);
	}

	int r = 0;
	if (posts && len)
	{
		// 先发头
		PckHeader hd;
		hd.bodyLeng = len;
		hd.crc32 = CRC32Check(posts, len);
		hd.decompLeng = 0;
		hd.cmdId = kCmdRequest;
		hd.flags = flags;

		if (send(gDeamonSock, (const char*)&hd, sizeof(hd), 0) == SOCKET_ERROR)
		{
			// 连接可能断了，于是重连
			socketClose();

			if (!socketConnect())
			{
				lua_pushboolean(L, 0);
				return 1;
			}
		}

		if (send(gDeamonSock, posts, len, 0) == SOCKET_ERROR)
			socketClose();
		else
			r = 1;
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
size_t ZLibCompress(const void* data, size_t size, char* outbuf, size_t outbufSize, int32_t level)
{
	if (size == 0)
		return 0;

	uLongf destLeng = outbufSize;

	if (level > 9)
		level = 9;
	else if (level < 1)
		level = 1;

	int ret = compress2((Bytef*)outbuf, &destLeng, (const Bytef*)data, size, level);
	if (ret != Z_OK || destLeng > size)
		return 0;

	return destLeng;
}

size_t ZLibDecompress(const void* data, size_t size, void* outmem, size_t outsize)
{
	uLongf destlen = outsize;
	int ret = uncompress((Bytef*)outmem, &destlen, (const Bytef*)data, size);
	if (ret != Z_OK)
	{
		memcpy(outmem, data, std::min(size, outsize));
		return size;
	}

	return destlen;
}

int64_t str2int64(const char* str)
{
	char* endp;
	if (str && str[0])
		return strtoll(str, &endp, 10);
	return 0;
}

uint64_t str2uint64(const char* str)
{
	char* endp;
	if (str && str[0])
	{
		if (str[0] == '0' && str[1] == 'x')
			return strtoull(str + 2, &endp, 16);
		return strtoull(str, &endp, 10);
	}
	return 0;
}

int64_t double2int64(double dbl)
{
	return dbl;
}

uint64_t double2uint64(double dbl)
{
	return dbl;
}

int64_t ltud2int64(void* p)
{
	return (int64_t)p;
}

uint64_t ltud2uint64(void* p)
{
	return (uint64_t)p;
}

uint32_t cdataisint64(const char* str, size_t len)
{
	size_t outl;
	if (str)
	{
		int postfix = cdataValueIsInt64((const uint8_t*)str, len, &outl);
		if (outl + postfix == len)
			return postfix;
	}
	return 0;
}

static int lua_uint64_tolightuserdata(lua_State* L)
{
	const uint64_t* p = (const uint64_t*)lua_topointer(L, 1);
	if (p)
	{
		lua_pushlightuserdata(L, (void*)p[0]);
		return 1;
	}

	luaL_checktype(L, 1, LUA_TCDATA);
	return 0;
}

static void initCommonLib(lua_State* L)
{
	int top = lua_gettop(L);

	luaext_string(L);
	luaext_table(L);
	luaext_utf8str(L);

	// 新增几个全局函数
	lua_pushcfunction(L, &lua_toboolean);
	lua_setglobal(L, "toboolean");

	lua_pushcfunction(L, &lua_checknull);
	lua_setglobal(L, "checknull");

	lua_pushcfunction(L, &lua_hasequal);
	lua_setglobal(L, "hasequal");

	lua_pushcfunction(L, &lua_rawhasequal);
	lua_setglobal(L, "rawhasequal");

	// 引用一部分ffi函数
	lua_getglobal(L, "require");
	lua_pushliteral(L, "ffi");
	lua_pcall(L, 1, 1, 0);

	lua_getfield(L, -1, "new");
	lua_rawseti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);

	lua_getfield(L, -1, "sizeof");
	lua_rawseti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFISizeof);

	lua_getglobal(L, "tostring");
	lua_rawseti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);

	lua_getglobal(L, "ngx");
	lua_getfield(L, -1, "re");
	lua_getfield(L, -1, "match");
	lua_rawseti(L, LUA_REGISTRYINDEX, kLuaRegVal_ngx_re_match);

	lua_settop(L, top);
}

//////////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
REEME_API int luaopen_reemext(lua_State* L)
#else
REEME_API int luaopen_libreemext(lua_State* L)
#endif
{
	initCommonLib(L);

	// 将部分扩展函数注册为meta table并提供meta table查找函数
	static luaL_Reg cExtProcs[] = {
		{ "sql_token_parse", &lua_sql_token_parse },
		{ "start_deamon", &lua_start_deamon },
		{ "connect_deamon", &lua_connect_deamon },
		{ "request_deamon", &lua_request_deamon },

		// 从指定的位置开始取一个词
		{ "find_token", lua_sql_findtoken },

		// 将boxed int64直接转成void*型，以保证不同cdata int64的值唯一
		{ "int64ltud", &lua_uint64_tolightuserdata },

		{ NULL, NULL }
	};

	lua_pushcfunction(L, &lua_findmetatable);
	lua_setglobal(L, "findmetatable");

	luaL_newmetatable(L, "REEME_C_EXTLIB");
	luaL_register(L, NULL, cExtProcs);

	lua_pop(L, 1);

	return 1;
}
