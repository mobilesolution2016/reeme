static uint32_t CRC32Table[256];
struct initCRC32Table
{
	initCRC32Table()
	{
		int i, j;
		uint32_t crc;
		for (i = 0; i < 256; i++)
		{
			crc = i;
			for (j = 0; j < 8; j++)
			{
				if (crc & 1)
					crc = (crc >> 1) ^ 0xEDB88320;
				else
					crc = crc >> 1;
			}
			CRC32Table[i] = crc;
		}
	}
} _g_initCRC32Table;

uint32_t CRC32Check(const void* data, size_t size)
{
	uint32_t ret = 0xFFFFFFFF;
	const uint8_t* buf = (const uint8_t*)data;

	for (int i = 0; i < size; i++)
		ret = CRC32Table[((ret & 0xFF) ^ buf[i])] ^ (ret >> 8);
	return ~ret;
}

//////////////////////////////////////////////////////////////////////////
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
		else if (len == 5 && stricmp(s, "false") == 0)
			cc = 1;
		else if (len)
		{
			long v = strtoul(s, &endp, 10);
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

	case LUA_TNONE:
		break;

	default:
		if (!strict)
			r = cc = 1;
		break;
	}

	lua_pushboolean(L, r);
	return cc;
}

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
		lua_pushvalue(L, top >= 3 ? 3 : 1);
	return 1;
}

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

static int lua_rawhasequal(lua_State* L)
{
	int n = 2, top = lua_gettop(L);
	while (n <= top)
	{
		if (lua_rawequal(L, 1, n))
		{
			lua_pushinteger(L, n);
			return 1;
		}
		++ n;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
REEME_API int64_t str2int64(const char* str);
REEME_API uint64_t str2uint64(const char* str);
REEME_API int64_t double2int64(double dbl);
REEME_API uint64_t double2uint64(double dbl);
REEME_API int64_t ltud2int64(void* p);
REEME_API uint64_t ltud2uint64(void* p);
REEME_API uint32_t cdataisint64(const char* str, size_t len);

REEME_API int deleteDirectory(const char* path);
REEME_API int deleteFile(const char* fname);

REEME_API const char* readdirinfo(void* p, const char* filter);
#ifndef _WINDOWS
REEME_API bool pathisfile(const char* path);
REEME_API bool pathisdir(const char* path);
REEME_API bool createdir(const char* path, int mode);
REEME_API unsigned getpathattrs(const char* path);
REEME_API unsigned pathisexists(const char* path);
#endif
