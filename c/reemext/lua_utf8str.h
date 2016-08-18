// utf8从前向后跳过完整的字符数
template <typename STR, typename COUNT> STR* lua_utf8str_skip_ht(STR* str, size_t len, COUNT skips)
{
	STR* ptr = str;
	if (skips == 0)
		return ptr;

	while(ptr - str < len)
	{
		STR hiChar = ptr[0];
		if (!(hiChar & 0x80))
			ptr ++;
		else if ((hiChar & 0xE0) == 0xC0)
			ptr += 2;
		else if ((hiChar & 0xF0) == 0xE0)
			ptr += 3;
		else if ((hiChar & 0xF8) == 0xF0)
			ptr += 4;
		else if ((hiChar & 0xFC) == 0xF8)
			ptr += 5;
		else if ((hiChar & 0xFE) == 0xFC)
			ptr += 6;
		else
			return 0;

		if (-- skips == 0)
			return ptr;
	}

	return 0;
}
// utf8从后向前跳过完整的字符数
template <typename STR, typename COUNT> STR* lua_utf8str_skip_th(STR* str, size_t len, COUNT skips)
{
	uint32_t cc = 0;
	STR* ptr = str + len;

	if (skips == 0)
		return ptr;

	ptr --;
	while(ptr >= str)
	{
		STR hiChar = ptr[0];
		if (!(hiChar & 0x80))
		{
			if (-- skips == 0)
				return ptr;

			ptr --;
			cc = 0;
			continue;
		}

		switch (cc)
		{
		case 0:			
			cc ++;
			break;

		case 1:
			if ((hiChar & 0xE0) == 0xC0)
			{
				if (-- skips == 0)
					return ptr;				
				cc = 0;
			}
			else
				cc ++;
			break;

		case 2:
			if ((hiChar & 0xF0) == 0xE0)
			{
				if (-- skips == 0)
					return ptr;				
				cc = 0;
			}
			else
				cc ++;
			break;

		case 3:
			if ((hiChar & 0xF8) == 0xF0)
			{
				if (-- skips == 0)
					return ptr;				
				cc = 0;
			}
			else
				cc ++;
			break;

		case 4:
			if ((hiChar & 0xFC) == 0xF8)
			{
				if (-- skips == 0)
					return ptr;				
				cc = 0;
			}
			else
				cc ++;
			break;

		case 5:
			if ((hiChar & 0xFE) == 0xFC)
			{
				if (-- skips == 0)
					return ptr;	
				cc = 0;
			}
			else
				return 0;
			break;

		default:
			return 0;
		}

		ptr --;
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
static int lua_utf8str_det3(lua_State* L)
{
	int r = 1;
	size_t len = 0;
	const uint8_t* str = (const uint8_t*)luaL_checklstring(L, 1, &len);
	const uint8_t* ptr = str;

	while(ptr - str < len)
	{
		uint8_t hiChar = ptr[0];
		if (!(hiChar & 0x80))
			ptr ++;
		else if ((hiChar & 0xE0) == 0xC0 && (ptr[1] & 0xC0) == 0x80)
			ptr += 2;
		else if ((hiChar & 0xF0) == 0xE0 && (ptr[1] & 0xC0) == 0x80 && (ptr[2] & 0xC0) == 0x80)
			ptr += 3;
		else
		{
			r = 0;
			break;
		}
	}

	if (str + len != ptr)
		r = 0;

	lua_pushboolean(L, r);
	return 1;
}

static int lua_utf8str_len(lua_State* L)
{
	int cc = 0;
	size_t len = 0;
	const uint8_t* str = (const uint8_t*)luaL_checklstring(L, 1, &len);
	const uint8_t* ptr = str;

	if (lua_isnumber(L, 2))
	{
		long n = luaL_checklong(L, 2);
		if (n < len)
			len = n;
	}

	while(ptr - str < len)
	{
		uint8_t hiChar = ptr[0];
		if (!(hiChar & 0x80))
			ptr ++;
		else if ((hiChar & 0xE0) == 0xC0)
			ptr += 2;
		else if ((hiChar & 0xF0) == 0xE0)
			ptr += 3;
		else if ((hiChar & 0xF8) == 0xF0)
			ptr += 4;
		else if ((hiChar & 0xFC) == 0xF8)
			ptr += 5;
		else if ((hiChar & 0xFE) == 0xFC)
			ptr += 6;
		else
			return 0;

		cc ++;
	}

	if (str + len == ptr)
	{
		lua_pushinteger(L, cc);
		return 1;
	}

	return 0;
}

static int lua_utf8str_sub(lua_State* L)
{
	size_t i, len = 0;
	const uint8_t* str = (const uint8_t*)luaL_checklstring(L, 1, &len);
	long start = luaL_checklong(L, 2) - 1;
	if (start < 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	const uint8_t* ptr = lua_utf8str_skip_ht(str, len, start), *endptr = 0;
	if (!ptr)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	if (lua_isnumber(L, 3))
	{
		long end = luaL_checklong(L, 3);
		if (end > 0)
		{
			if (end < start)
			{
				lua_pushlstring(L, "", 0);
				return 1;
			}

			endptr = lua_utf8str_skip_ht(ptr, len - (ptr - str), end - start);
		}
		else if (end < 0)
		{
			endptr = lua_utf8str_skip_th(str, len, -1 - end);
		}

		if (!endptr || endptr <= ptr)
		{
			lua_pushlstring(L, "", 0);
			return 1;
		}
	}
	else
		endptr = ptr + len;

	lua_pushlstring(L, (const char*)ptr, endptr - ptr);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static void luaext_utf8str(lua_State *L)
{
	const luaL_Reg procs[] = {
		// 检测是否是3字节UTF8编码的类型
		{ "det3", &lua_utf8str_det3 },
		// 检测UTF8字符串的长度
		{ "len", &lua_utf8str_len },
		// 获取子串
		{ "sub", &lua_utf8str_sub },

		{ NULL, NULL }
	};

	luaL_register(L, "u8string", procs);
	lua_pop(L, 1);
}