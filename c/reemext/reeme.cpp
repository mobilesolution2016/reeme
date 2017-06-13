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

#include "qrcode/qrencode.h"

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
		lua_pushboolean(L, 1);
		return 1;
	}

	size_t len = 0, pathLen = 0;
	const char* deamonPath = lua_isstring(L, 1) ? luaL_checklstring(L, 1, &pathLen) : 0;
	const char* args = 0, *host = "127.0.0.1";
	unsigned short port = 5918;

	int t = lua_type(L, 2), v = 0;
	if (t == LUA_TTABLE)
	{
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
		lua_getglobal(L, "string");
		lua_getfield(L, -1, "json");
		lua_pushvalue(L, 2);
		lua_pcall(L, 1, 1, 0);

		if (lua_istable(L, -1))
			v = lua_gettop(L);
	}
	else if (t == LUA_TNIL)
	{
		t = LUA_TSTRING;
		args = "{}";
		len = 2;
	}
	else
		return 0;

	if (v)
	{
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
	std::wstring strExe, strCmd;

	int cchPath = MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, 0, 0);
	int cchCmd = MultiByteToWideChar(CP_UTF8, 0, args, len, 0, 0);

	strExe.resize(cchPath);
	strCmd.resize(cchCmd + 5);

	if (t == LUA_TSTRING && GetFileAttributesA(args))
		strCmd = L"file:";
	else
		strCmd = L"json:";

	if (cchPath)
		MultiByteToWideChar(CP_UTF8, 0, deamonPath, pathLen, const_cast<wchar_t*>(strExe.c_str()), cchPath + 1);
	MultiByteToWideChar(CP_UTF8, 0, args, len, const_cast<wchar_t*>(strCmd.c_str()) + 5, cchCmd + 1);

	if (pathLen == 0)
	{
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
		PckHeader hd;
		hd.bodyLeng = len;
		hd.crc32 = CRC32Check(posts, len);
		hd.decompLeng = 0;
		hd.cmdId = kCmdRequest;
		hd.flags = flags;

		if (send(gDeamonSock, (const char*)&hd, sizeof(hd), 0) == SOCKET_ERROR)
		{
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

int deleteDirectory(const char* dir)
{
#ifdef _WINDOWS
	int len = strlen(dir) + 2;
	char tempdirFix[512] = { 0 };
	char* tempdir = tempdirFix;

	if (len > 512)
	{
		tempdir = (char*)malloc(len);
		tempdir[len - 1] = 0;
		tempdir[len] = 0;
	}
	memcpy(tempdir, dir, len);

	SHFILEOPSTRUCTA file_op = {
		NULL,
		FO_DELETE,
		tempdir,
		"",
		FOF_NOCONFIRMATION |
		FOF_NOERRORUI |
		FOF_SILENT,
		false,
		0,
		""
	};
	int ret = SHFileOperationA(&file_op);

	if (tempdir != tempdirFix)
		free(tempdir);

	return ret == 0 ? TRUE : FALSE;
#else
	if (!dir || !dir[0])
		return false;

	DIR* dp = NULL;
	DIR* dpin = NULL;

	struct dirent* dirp;
	dp = opendir(dir);
	if (dp == 0)
		return 0;

	std::string strPathname;
	while ((dirp = readdir(dp)) != 0)
	{
		if (strcmp(dirp->d_name, "..") == 0 || strcmp(dirp->d_name, ".") == 0)
			continue;

		strPathname = dir;
		strPathname += '/';
		strPathname += dirp->d_name;
		dpin = opendir(strPathname.c_str());
		if (dpin != 0)
		{
			closedir(dpin);
			dpin = 0;

			if (!deleteDirectory(strPathname.c_str()))
				return 0;
		}
		else if (remove(strPathname.c_str()) != 0)
		{
			closedir(dp);
			return 0;
		}
	}

	rmdir(dir);
	closedir(dp);

	return 1;
#endif
}

int deleteFile(const char* fname)
{
#ifdef _WINDOWS
	SetFileAttributesA(fname, FILE_ATTRIBUTE_ARCHIVE);
	return DeleteFileA(fname);
#else
	if (access(fname, F_OK) == 0)
		return remove(fname) == 0;
	return false;
#endif
}

bool getFileTime(const char* fname, LocalDateTime* create, LocalDateTime* update)
{
#ifdef _WINDOWS
	HANDLE hFile = CreateFileA(fname, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		SYSTEMTIME sys;
		FILETIME c, u, l;

		BOOL ret = GetFileTime(hFile, &c, 0, &u);
		CloseHandle(hFile);

		if (!ret)
			return false;

		if (create)
		{
			FileTimeToLocalFileTime(&c, &l);
			FileTimeToSystemTime(&l, &sys);
			create->year = sys.wYear;
			create->month = sys.wMonth;
			create->day = sys.wDay;			
			create->dayofweek = sys.wDayOfWeek;
			create->hour = sys.wHour;
			create->minute = sys.wMinute;
			create->second = sys.wSecond;
			create->millisecond = sys.wMilliseconds;
		}

		if (update)
		{
			FileTimeToLocalFileTime(&u, &l);
			FileTimeToSystemTime(&l, &sys);
			update->year = sys.wYear;
			update->month = sys.wMonth;
			update->day = sys.wDay;			
			update->dayofweek = sys.wDayOfWeek;
			update->hour = sys.wHour;
			update->minute = sys.wMinute;
			update->second = sys.wSecond;
			update->millisecond = sys.wMilliseconds;
		}

		return true;
	}
#else
	struct stat statbuf;
	if (lstat(fname, &statbuf) == 0)
	{
		time_t c;
#ifdef HAVE_ST_BIRTHTIME
		c = statbuf.st_birthtime;
#else
		c = statbuf.st_ctime;
#endif

		if (create)
		{
			tm* ctm = localtime(&c);
			create->year = 1900 + ctm->tm_year;
			create->month = ctm->tm_mon;
			create->day = ctm->tm_mday;
			create->dayofweek = ctm->tm_wday;
			create->hour = ctm->tm_hour;
			create->minute = ctm->tm_min;
			create->second = ctm->tm_sec;
			create->millisecond = 0;
		}

		if (update)
		{
			tm* utm = localtime((const time_t *)&statbuf.st_mtime);
			update->year = 1900 + utm->tm_year;
			update->month = utm->tm_mon;
			update->day = utm->tm_mday;
			update->dayofweek = utm->tm_wday;
			update->hour = utm->tm_hour;
			update->minute = utm->tm_min;
			update->second = utm->tm_sec;
			update->millisecond = 0;
		}

		return true;
	}
#endif
	return false;
}

double getFileSize(const char* fname)
{
#ifdef _WINDOWS
	HANDLE hFile = CreateFileA(fname, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile && hFile != INVALID_HANDLE_VALUE)
	{
		DWORD hi;
		DWORD lo = GetFileSize(hFile, &hi);
		CloseHandle(hFile);

		return static_cast<double>(((uint64_t)hi << 32) | lo);
	}
#else
	FILE* fp = fopen(fname, "rb");
	if (fp)
	{
		fseek(fp, 0L, SEEK_END);
		long s = ftell(fp);
		fclose(fp);

		return static_cast<double>(s);
	}
#endif

	return -1;
}

#ifndef _WINDOWS
bool fnamematch(const char* needle, const char* haystack)
{
	for(; needle[0] != '\0'; ++ needle)
	{
		switch(needle[0])
		{
			case '?':
				++ haystack;
				break;

			case '*':
			{
				if (needle[1] == '\0' || haystack[0] == '\0')
					return true;
				for(size_t i = 0; ; ++ i)
				{
					if (haystack[i] == '\0')
						return false;
					if (fnamematch(needle + 1, haystack + i))
						return true;
				}
				return false;
			}
			break;

			default:
				if (haystack[0] != needle[0])
					return false;
				++ haystack;
			break;
		}
	}

	return haystack[0] == '\0';
}
const char* readdirinfo(void* p, const char* filter)
{
	for(;;)
	{
		const dirent* d = readdir((DIR*)p);
		if (!d)
			break;

		const char* name = d->d_name;
		if (!filter || strcmp(filter, "*.*") == 0)
			return name;
		if (!filter[1] && (filter[0] == '.' || filter[0] == '*'))
			return name;

		if (fnamematch(filter, name))
			return name;
	}
	return NULL;
}

unsigned getpathattrs(const char* path)
{
	struct stat buf;
	if (stat(path, &buf) == -1)
		return 0;

	unsigned r = 0;
	if (S_ISREG(buf.st_mode)) r |= 1;
	if (S_ISDIR(buf.st_mode)) r |= 2;
	if (buf.st_mode & S_IFLNK) r |= 4;
	if (buf.st_mode & S_IFSOCK) r |= 8;

	const char* lastseg = strrchr(path, '/');
	if (lastseg)
	{
		if (lastseg + 1 <= path && lastseg[1] == '.')
			r |= 0x10;
	}
	else if (path[0])
	{
		r |= 0x10;
	}

	return r | (buf.st_mode & 0x1FF);
}

bool pathisfile(const char* path)
{
	struct stat buf;
	if (stat(path, &buf) != 0)
		return 0;
	return S_ISREG(buf.st_mode);
}
bool pathisdir(const char* path)
{
	struct stat buf = { 0 };
	if (stat(path, &buf) != 0)
		return 0;
	return S_ISDIR(buf.st_mode);
}
unsigned pathisexists(const char* path)
{
	struct stat buf = { 0 };
	if (stat(path, &buf) != 0)
		return 0;

	if (S_ISREG(buf.st_mode))
		return 1;
	if (S_ISDIR(buf.st_mode))
		return 2;
	return 0;
}
bool createdir(const char* path, int mode)
{
	struct stat buf = { 0 };
	if (stat(path, &buf) == -1)
	{
		umask(0);
		return mkdir(path, mode != 0 ? mode : 0666) == 0 || errno == EEXIST;
	}

	return (S_ISDIR(buf.st_mode)) ? true : false;
}
#endif

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

	// 新增一些全局函数
	static luaL_Reg cGlobalProcs[] = {
		{ "toboolean", &lua_toboolean },
		{ "checknull", &lua_checknull },
		{ "hasequal", &lua_hasequal },
		{ "rawhasequal", &lua_rawhasequal },
		{ "findmetatable", &lua_findmetatable },

		{ NULL, NULL }
	};

	lua_getglobal(L, "_G");
	for(;;)
	{
		if (lua_getmetatable(L, -1))
			lua_getfield(L, -1, "__index");
		else
			break;
	}
	luaL_register(L, NULL, cGlobalProcs);
	lua_settop(L, top);

	// 缓存一些函数方便C来调用
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
#ifdef __QRENCODE_H__
int lua_qrcode_gen(lua_State* L)
{
	const char szKeys[][12] =
	{
		{ "code" },
		{ "blocksize" },
		{ "margin" },
		{ "level" },
		{ "fgcolor" },
		{ "bgcolor" },
	};

	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	bool bOk = false;
	const char* code = 0;
	int version = 0, dpi = 0, blockSize = 1, margins = 0, level = QR_ECLEVEL_L;
	uint32_t fgcolor = 0xff000000, bgcolor = 0xffffffff;

	lua_pushnil(L);
	while (lua_next(L, 1))
	{
		const char* name = lua_tostring(L, -2);
		uint32_t nNameId = *(uint32_t*)name;

		if (nNameId == *(const uint32_t*)szKeys[0] && strcmp(name, szKeys[0]) == 0)
			code = lua_tostring(L, -1);
		else if (nNameId == *(const uint32_t*)szKeys[1] && strcmp(name, szKeys[1]) == 0)
			blockSize = std::max(1, (int)lua_tointeger(L, -1));
		else if (nNameId == *(const uint32_t*)szKeys[2] && strcmp(name, szKeys[2]) == 0)
			margins = std::max(0, (int)lua_tointeger(L, -1));
		else if (nNameId == *(const uint32_t*)szKeys[3] && strcmp(name, szKeys[3]) == 0)
			level = std::max(0, (int)lua_tointeger(L, -1));
		else if (nNameId == *(const uint32_t*)szKeys[4] && strcmp(name, szKeys[4]) == 0)
			fgcolor = lua_tointeger(L, -1);
		else if (nNameId == *(const uint32_t*)szKeys[5] && strcmp(name, szKeys[5]) == 0)
			bgcolor = lua_tointeger(L, -1);

		lua_pop(L, 1);
	}

	if (level > QR_ECLEVEL_H)
		level = QR_ECLEVEL_H;

	if (!code)
	{
		luaL_error(L, "Call QRCodeEncode function without set \"code\" string", 0);
		return 0;
	}

	QRcode *qrcode = QRcode_encodeString(code, version, (QRecLevel)level, QR_MODE_8, 1);
	if (qrcode)
	{
		bool bHas = false;		
		uint8_t* p = qrcode->data, *row;
		uint32_t *bmpMem = 0, *bmpMemPtr, *bmpRow, *writePtr, setColor;
		int x = 0, y = 0, rx = 0, ry = 0, r, realwidth = (qrcode->width + margins * 2) * blockSize;

		// 若有函数，则使用该函数创建出Bitmap，bitmap会有一个成员名为data
		lua_pushinteger(L, realwidth);
		lua_pushinteger(L, realwidth);
		lua_pushliteral(L, "bgra8");
		lua_pcall(L, 3, 1, 0);
		r = lua_gettop(L);

		lua_getfield(L, -1, "data");
		bmpMem = const_cast<uint32_t*>((const uint32_t*)lua_topointer(L, -1));
		if (!bmpMem)
		{
			QRcode_free(qrcode);
			return 0;
		}

		// 填充像素数据
		bmpMemPtr = bmpMem + realwidth * margins * blockSize;
		if (blockSize == 1)
		{
			for (y = 0; y < qrcode->width; y ++)
			{
				row = p + y * qrcode->width;

				bmpRow = bmpMemPtr + margins;
				bmpMemPtr += realwidth;

				for (x = 0; x < qrcode->width; x ++)
					bmpRow[x] = (row[x] & 0x1) ? fgcolor : bgcolor;
			}
		}
		else
		{
			for (y = 0; y < qrcode->width; y ++)
			{
				row = p + y * qrcode->width;

				bmpRow = bmpMemPtr + margins * blockSize;
				bmpMemPtr += realwidth * blockSize;

				for (x = 0; x < qrcode->width; x ++)
				{
					writePtr = bmpRow;
					setColor = (row[x] & 0x1) ? fgcolor : bgcolor;
					for (ry = 0; ry < blockSize; ry ++)
					{
						for (rx = 0; rx < blockSize; rx ++)
							writePtr[rx] = setColor;
						writePtr += realwidth;
					}
					bmpRow += blockSize;
				}
			}
		}

		QRcode_free(qrcode);

		lua_pushvalue(L, r);
		lua_pushinteger(L, blockSize);
		return 2;
	}

	return 0;
}
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef _WINDOWS
REEME_API int luaopen_reemext(lua_State* L)
#else
REEME_API int luaopen_libreemext(lua_State* L)
#endif
{
	initCommonLib(L);

	// 一些隐藏的扩展函数
	static luaL_Reg cExtProcs[] = {
		{ "sql_token_parse", &lua_sql_token_parse },
		{ "start_deamon", &lua_start_deamon },
		{ "connect_deamon", &lua_connect_deamon },
		{ "request_deamon", &lua_request_deamon },

		{ "find_token", lua_sql_findtoken },

		{ "int64ltud", &lua_uint64_tolightuserdata },

		{ NULL, NULL }
	};

	luaL_newmetatable(L, "REEME_C_EXTLIB");
	luaL_register(L, NULL, cExtProcs);

	lua_pop(L, 1);

#ifdef __QRENCODE_H__
	lua_pushcfunction(L, &lua_qrcode_gen);
	lua_setglobal(L, "QRCodeEncode");
#endif

	return 1;
}
