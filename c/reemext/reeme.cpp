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
		// �ȵȴ�100ms���ң��ȴ��������ɳ�ʼ���������ظ����������˳������ೢ��5��
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
		// �Ѿ�������
		lua_pushboolean(L, 1);
		return 1;
	}

	// taskdeamonִ���ļ���·��
	size_t len = 0, pathLen = 0;
	const char* deamonPath = lua_isstring(L, 1) ? luaL_checklstring(L, 1, &pathLen) : 0;
	const char* args = 0, *host = "127.0.0.1";
	unsigned short port = 5918;

	// ��������
	int t = lua_type(L, 2), v = 0;
	if (t == LUA_TTABLE)
	{
		// �����Ǹ�table��������Ϊjson�ַ���
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
		// �����Ǹ�json�ַ�����������Ϊtable����Ϊ��Ҫ���л�ȡ����
		lua_getglobal(L, "string");
		lua_getfield(L, -1, "json");
		lua_pushvalue(L, 2);
		lua_pcall(L, 1, 1, 0);

		if (lua_istable(L, -1))
			v = lua_gettop(L);
	}
	else if (t == LUA_TNIL)
	{
		// ����Ϊnil����ȫ��ʹ��Ĭ�ϲ���
		t = LUA_TSTRING;
		args = "{}";
		len = 2;
	}
	else
		return 0;

	if (v)
	{
		// ��ȡ��Ҫ����
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
	// ��Windows��ʹ��shellexecute������taskdeamon.exe
	std::wstring strExe, strCmd;

	int cchPath = MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, 0, 0);
	int cchCmd = MultiByteToWideChar(CP_UTF8, 0, args, len, 0, 0);

	strExe.resize(cchPath);
	strCmd.resize(cchCmd + 5);

	// �����ǲ���2�������ַ����Ļ������п�����һ���ļ���
	if (t == LUA_TSTRING && GetFileAttributesA(args))
		strCmd = L"file:";
	else
		strCmd = L"json:";

	if (cchPath)
		MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, const_cast<wchar_t*>(strExe.c_str()), cchPath + 1);
	MultiByteToWideChar(CP_UTF8, 0, args, len, const_cast<wchar_t*>(strCmd.c_str()) + 5, cchCmd + 1);

	if (pathLen == 0)
	{
		// û��ָ��·������ô���õ�ǰģ�����ڵ�·�����൱��Ĭ��taskdeamon.exe��reemext.dll����ͬһ��Ŀ¼�µ�
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
		// �ȷ�ͷ
		PckHeader hd;
		hd.bodyLeng = len;
		hd.crc32 = CRC32Check(posts, len);
		hd.decompLeng = 0;
		hd.cmdId = kCmdRequest;
		hd.flags = flags;

		if (send(gDeamonSock, (const char*)&hd, sizeof(hd), 0) == SOCKET_ERROR)
		{
			// ���ӿ��ܶ��ˣ���������
			socketClose();

			if (!socketConnect())
			{
				lua_pushboolean(L, 0);
				return 1;
			}
		}

		// �ٷ���body����
		if (send(gDeamonSock, posts, len, 0) == SOCKET_ERROR)
			socketClose();
		else
			r = 1;
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
REEME_API int luaopen_reemext(lua_State* L)
{
	initCommonLib(L);

	// ��������չ����ע��Ϊmeta table���ṩmeta table���Һ���
	static luaL_Reg cExtProcs[] = {
		{ "sql_expression_parse", &lua_sql_expression_parse },
		{ "start_deamon", &lua_start_deamon },
		{ "connect_deamon", &lua_connect_deamon },
		{ "request_deamon", &lua_request_deamon },

		// ��ָ����λ�ÿ�ʼȡһ����
		{ "find_token", lua_sql_findtoken },

		// ��boxed int64ֱ��ת��void*�ͣ��Ա�֤��ͬcdata int64��ֵΨһ
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
