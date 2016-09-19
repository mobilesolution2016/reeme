// 下面是ASCII字符属性表，1表示符号，2表示大小写字母，3表示数字，4表示可以用于组合整数或小数的符号
static uint8_t sql_where_splits[128] = 
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	1,		// 0~32
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 4, 1,	// 33~47
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	// 48~57
	1, 1, 1, 1, 1, 1, 1,	// 58~64
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// 65~92
	1, 1, 1, 1, 2, 1,	// 91~96
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// 97~122
	1, 1, 1, 1, 1,
};

enum StringSplitFlags {
	kSplitAsKey = 0x40000000,
	kSplitTrim = 0x20000000,
};

enum StringJsonFlags {
	kJsonNoCopy = 0x80000000,
	kJsonRetCData = 0x40000000,
	kJsonLuaString = 0x20000000,
	kJsonUnicodes = 0x10000000,
};

//////////////////////////////////////////////////////////////////////////
static void lua_string_addbuf(luaL_Buffer* buf, const char* str, size_t len)
{
	size_t lenleft = len, copy;
	while(lenleft > 0)
	{
		copy = std::min(lenleft, (size_t)(LUAL_BUFFERSIZE - (buf->p - buf->buffer)));
		if (!copy)
		{
			luaL_prepbuffer(buf);
			copy = std::min(lenleft, (size_t)LUAL_BUFFERSIZE);
		}

		memcpy(buf->p, str, copy);
		lenleft -= copy;
		buf->p += copy;
		str += copy;
	}
}

static uint32_t cdataValueIsInt64(const uint8_t* ptr, size_t len, size_t* lenout, uint32_t minDigits = 1)
{
	size_t i = 0;
	uint32_t postfix = 2, backc = 0;
	uint8_t ch, chExpet = 0, digits = 0;	

	if (ptr[0] == '-')
	{
		postfix = 3;
		i ++;
	}

	for (; i < len; ++ i)
	{
		ch = integer64_valid_bits[ptr[i]];
		if (ch == 0xFF)
			return 0;

		if (ch != chExpet)
		{
			if (chExpet)
				return 0;

			switch (ch)
			{
			case 1:
				postfix = 3;	// ULL
			case 2:
				chExpet = 2;	// LL
				break;
			case 3:
				postfix = 3;	// ull
			case 4:
				chExpet = 2;	// ll
				break;
			default:
				return 0;
			}

			backc = postfix;
		}
		else if (chExpet)
		{
			if (backc <= 1)
				return 0;
			backc --;
		}
		else
			digits ++;
	}

	if (digits < minDigits)
		return 0;

	*lenout = len - postfix;

	// 返回3表示为unsigned int64，2表示signed int64
	return postfix;
}

//////////////////////////////////////////////////////////////////////////

struct BMString
{
	uint32_t	m_flags;
	uint32_t	m_hasNext;
	size_t		m_subLen;
	size_t		m_subMaxLen;	

	void setSub(const char* src, size_t len)
	{
#ifdef REEME_64
		m_subMaxLen = len + 7 >> 3 << 3;
#else
		m_subMaxLen = len + 3 >> 2 << 2;
#endif
		m_subLen = len;
		memcpy(this + 1, src, len);
	}

	void makePreTable()
	{
		size_t pos, i;
		uint8_t* sub = (uint8_t*)(this + 1);
		size_t* pSkips = (size_t*)(sub + m_subMaxLen);
		size_t* pShifts = pSkips + 256;

		memset(pShifts, 0, sizeof(size_t) * m_subLen);

		for (i = 0; i < 256; ++ i)
			pSkips[i] = m_subLen;

		for (pos = m_subLen - 1; ; -- pos)
		{
			uint8_t *good = sub + pos + 1;
			size_t goodLen = m_subLen - pos - 1;

			while (goodLen > 0)
			{
				uint8_t *p = sub + m_subLen - 1 - goodLen;
				while (p >= sub)
				{
					if (memcmp(p, good, goodLen) == 0)
					{
						pShifts[pos] = (m_subLen - pos) + (good - p) - 1;
						break;
					}
					p --;
				}

				if (pShifts[pos] != 0)
					break;

				good ++;
				goodLen --;
			}

			if (pShifts[pos] == 0)
				pShifts[pos] = m_subLen - pos;

			if (pos == 0)
				break;
		}

		i = m_subLen;
		while (i)
			pSkips[*sub ++] = -- i;
	}

	BMString* getNext() const
	{
		if (!m_hasNext)
			return 0;

		uint8_t* sub = (uint8_t*)(this + 1);
		size_t* pTbl = (size_t*)(sub + m_subMaxLen);

		return (BMString*)(pTbl + 256 + m_subLen);
	}

	size_t find(const uint8_t* src, size_t srcLen) const
	{
		const uint8_t* sub = (const uint8_t*)(this + 1);
		const size_t* pSkips = (const size_t*)(sub + m_subMaxLen);
		const size_t* pShifts = pSkips + 256;

		size_t lenSub1 = m_subLen - 1;
		size_t strEnd = lenSub1, subEnd;
		while (strEnd <= srcLen)
		{
			subEnd = lenSub1;
			while (src[strEnd] == sub[subEnd])
			{
				if (subEnd == 0)
					return strEnd;

				strEnd --;
				subEnd --;
			}

			strEnd += std::max(pSkips[src[strEnd]], pShifts[subEnd]);
		}

		return -1;
	}

	static size_t calcAllocSize(size_t subLen)
	{
		size_t sub;
#ifdef REEME_64
		sub = subLen + 7 >> 3 << 3;
#else
		sub = subLen + 3 >> 2 << 2;
#endif
		return (subLen + 256) * sizeof(size_t) + sizeof(BMString) + sub;
	}
};


//////////////////////////////////////////////////////////////////////////
// 按参数1字符串按照参数2字符串中出现的每一个字符进行切分，切分时将根据参数3中设置的标志进行相应的处理，如果参数4存在且为true则切分的结果会以多返回值返回，否则将以table返回切分的结果
// 即使参数1字符串完全不含有参数2字符串中的任何一个字符，也会进行一次切分，只是返回为整个参数1字符串
static int lua_string_split(lua_State* L)
{
	bool exists = false;
	uint8_t checker[256] = { 0 }, ch;
	size_t byLen = 0, srcLen = 0, start = 0;
	uint32_t nFlags = 0, maxSplits = 0, cc = 0;
	int top = lua_gettop(L), retAs = LUA_TTABLE, tblVal = 3;

	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);
	const uint8_t* by = (const uint8_t*)luaL_checklstring(L, 2, &byLen);	

	retAs = lua_type(L, 3);
	if (retAs == LUA_TNUMBER)
	{
		nFlags = luaL_optinteger(L, 3, 0);
		maxSplits = nFlags & 0x0FFFFFFF;

		if (top >= 4)
			retAs = lua_type(L, tblVal = 4);
	}
	
	if (retAs == LUA_TBOOLEAN)
	{
		// is true return plained string(s)
		if (lua_toboolean(L, tblVal))
			retAs = LUA_TNIL;
	}
	else if (retAs == LUA_TTABLE)
	{
		// is table then use it directly
		cc = lua_objlen(L, tblVal);
		exists = true;
	}
	else
	{
		retAs = LUA_TTABLE;
	}

	if (retAs == LUA_TTABLE)
	{
		if (!exists)
		{
			int n1 = std::max(maxSplits, 4U), n2 = 0;

			if (nFlags & kSplitAsKey)
				std::swap(n1, n2);

			lua_createtable(L, n1, n2);
			tblVal = lua_gettop(L);
		}
	}
	else
		tblVal = 0;

	if (maxSplits == 0)
		maxSplits = 0x0FFFFFFF;

	// 设置标记
	size_t i, endpos;
	for(i = 0; i < byLen; ++ i)
		checker[by[i]] = 1;
	
	// 逐个字符的检测	
	for (i = endpos = 0; i < srcLen; ++ i)
	{
		ch = src[i];
		if (!checker[ch])
			continue;

		endpos = i;
		if (nFlags & kSplitTrim)
		{
			// trim
			while(start < endpos && src[start] <= 32)
				start ++;
			while(start < endpos && src[endpos - 1] <= 32)
				endpos --;
		}

		if (start < endpos)
		{
_lastseg:
			// push result
			if (tblVal)
			{
				lua_pushlstring(L, (const char*)src + start, endpos - start);
				if (nFlags & kSplitAsKey)
				{
					// as key
					lua_pushboolean(L, 1);
					lua_rawset(L, tblVal);
				}
				else
				{
					// as array element
					lua_rawseti(L, tblVal, cc + 1);
				}
			}
			else
			{
				lua_pushlstring(L, (const char*)src + start, endpos - start);
			}

			cc ++;
			if (cc >= maxSplits)
				break;
		}

		start = i + 1;
	}

	if (maxSplits && start < srcLen)
	{
		endpos = srcLen;
		maxSplits = 0;
		goto _lastseg;
	}

	if (cc == 0)
	{
		// no one
		lua_pushvalue(L, 1);
		if (tblVal)
		{
			if (nFlags & kSplitAsKey)
			{
				// as key
				lua_pushboolean(L, 1);
				lua_rawset(L, tblVal);
			}
			else
			{
				// as array element
				lua_rawseti(L, tblVal, cc + 1);
			}
		}

		return 1;
	}

	return retAs == LUA_TTABLE ? 1 : cc;
}

//////////////////////////////////////////////////////////////////////////
// 对字符串进行左右非可见符号去除，参数2和3如果存在且为true|false分别表示是否要处理左边|右边。如果没有参数2或3则默认左右都处理
static int lua_string_trim(lua_State* L)
{
	size_t len = 0;
	int triml = 1, trimr = 1;
	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &len);
	
	if (lua_isboolean(L, 2))
		triml = lua_toboolean(L, 2);
	if (lua_isboolean(L, 3))
		trimr = lua_toboolean(L, 3);

	const uint8_t* left = src;
	const uint8_t* right = src + len;
	if (triml)
	{
		while(left < right)
		{
			if (left[0] > 32)
				break;
			left ++;
		}
	}
	if (trimr)
	{
		while (left < right)
		{
			if (*(right - 1) > 32)
				break;
			right --;
		}
	}

	if (right - left == len)
		lua_pushvalue(L, 1);
	else
		lua_pushlstring(L, (const char*)left, right - left);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// 对参数1和2的字符串进行比对，参数3可以为一个整数表示要比对的字符串长度，参数3可以是一个布尔值表示是否要忽略大小写比对。参数3如果没有，参数3可以成为参数3
static int lua_string_cmp(lua_State* L)
{	
	int ignoreCase = 0, r = 0;
	size_t alen = 0, blen = 0, cmplen = -1;
	const char* a = luaL_checklstring(L, 1, &alen);
	const char* b = luaL_checklstring(L, 2, &blen);

	if (!a || !b)
	{
		lua_pushboolean(L, 0);
		return 1;
	}

	if (lua_isnumber(L, 3))
	{
		cmplen = luaL_checklong(L, 3);
		if (lua_isboolean(L, 4))
			ignoreCase = lua_toboolean(L, 4);
	}
	else if (lua_isboolean(L, 3))
	{
		ignoreCase = lua_toboolean(L, 3);
	}

	if (ignoreCase)
	{
		if (cmplen == -1)
			r = alen == blen ? stricmp(a, b) == 0 : 0;
		else if (cmplen > alen || cmplen > blen)
			r = 0;
		else
			r = strnicmp(a, b, cmplen) == 0;
	}
	else
	{
		if (cmplen == -1)
			r = alen == blen ? strcmp(a, b) == 0 : 0;
		else if (cmplen > alen || cmplen > blen)
			r = 0;
		else
			r = strncmp(a, b, cmplen) == 0;
	}

	lua_pushboolean(L, r);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
// 字符串中所含字符/串的查找，直接使用STL函数，比string.find(xx, xx, 1, true)快
static int lua_string_plainfind(lua_State* L)
{
	size_t len = 0, len2 = 0;
	const char* s = luaL_checklstring(L, 1, &len);
	const char* f = luaL_checklstring(L, 2, &len2);

	if (len2 && len2 <= len)
	{
		long t = luaL_optinteger(L, 3, 0);
		if (t > 0 && t <= len)
		{
			t --;
			s += t;
		}

		f = len2 > 1 ? std::strstr(s, f) : std::strchr(s, f[0]);
		if (f)
		{
			lua_pushinteger(L, f - s + t + 1);
			return 1;
		}
	}

	return 0;
}

// 字符串中所含字符倒叙查找，仅支持对字符进行查找不支持字符串
static int lua_string_rfindchar(lua_State* L)
{
	size_t len = 0, len2 = 0;
	const char* s = luaL_checklstring(L, 1, &len);
	const char* f = luaL_checklstring(L, 2, &len2);

	if (len && len2 == 1)
	{
		long t = luaL_optinteger(L, 3, 0);
		if (t > 0 && t <= len)
		{
			t --;
			s += t;
		}

		f = std::strrchr(s, f[0]);
		if (f)
		{
			lua_pushinteger(L, f - s + t + 1);
			return 1;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
struct StringReplacePos
{
	size_t		offset;
	size_t		fromLen, toLen;
	const char	*from, *to;
};
struct ReplacePosGreater : public std::binary_function <StringReplacePos&, StringReplacePos&, bool>
{
	bool operator()(const StringReplacePos& a, const StringReplacePos& b) const
	{
		return a.offset < b.offset;
	}
};

// 各种对照模式的字符/字符串替换
static int lua_string_replace(lua_State* L)
{
	int top = lua_gettop(L);
	if (top < 3)
		return 0;

	luaL_Buffer buf;
	size_t srcLen = 0, fromLen = 0, toLen = 0, offset;
	const char* src = luaL_checklstring(L, 1, &srcLen), *srcptr, *foundPos, *from, *to;

	luaL_buffinit(L, &buf);

	int tp1 = lua_type(L, 2), tp2 = lua_type(L, 3);
	if (tp2 == LUA_TTABLE)
	{
		int i;
		StringReplacePos newrep;
		std::list<StringReplacePos> replacePoses;

		if (tp1 == LUA_TTABLE)
		{
			// 字符串数组对字符串数组
			int fromTbLen = lua_objlen(L, 2);
			if (fromTbLen < 1 || fromTbLen != lua_objlen(L, 3))
				return luaL_error(L, "string.replace with two table which length not equal", 0);
			
			for (i = 1; i <= fromTbLen; ++ i)
			{
				lua_rawgeti(L, 2, i);
				from = lua_tolstring(L, -1, &fromLen);
				if (!fromLen)
					return luaL_error(L, "cannot replace from empty string at table(%d) by string.replace", i);

				lua_rawgeti(L, 3, i);
				to = lua_tolstring(L, -1, &toLen);

				srcptr = src;
				while((foundPos = fromLen == 1 ? std::strchr(srcptr, from[0]) : std::strstr(srcptr, from)) != 0)
				{			
					newrep.offset = foundPos - src;
					newrep.fromLen = fromLen;
					newrep.toLen = toLen;
					newrep.from = from;
					newrep.to = to;
					replacePoses.push_back(newrep);
					srcptr = foundPos + fromLen;
				}
			}
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM字符串对字符串数组
			BMString* bms = (BMString*)lua_touserdata(L, 2);
			if (bms->m_flags != 'BMST')
				return luaL_error(L, "not a string returned by string.bmcompile", 0);

			int toTbLen = lua_objlen(L, 3);
			if (toTbLen != bms->m_hasNext + 1)
				return luaL_error(L, "string.replace with two table which length not equal", 0);

			i = 1;
			do 
			{
				lua_rawgeti(L, 3, i ++);
				to = lua_tolstring(L, -1, &toLen);

				offset = fromLen = 0;
				while((offset = bms->find((const uint8_t*)src + fromLen, srcLen - fromLen)) != -1)
				{
					newrep.offset = offset + fromLen;
					newrep.fromLen = bms->m_subLen;
					newrep.toLen = toLen;
					newrep.from = (const char*)(bms + 1);
					newrep.to = to;
					replacePoses.push_back(newrep);

					fromLen += offset + newrep.fromLen;
				}

			} while ((bms = bms->getNext()) != 0);
		}
		else
			return luaL_error(L, "error type for the 2-th parameter of string.replcae", 0);

		// 排序之后按顺序替换
		i = 0;
		offset = 0;
		if (replacePoses.size())
		{
			replacePoses.sort(ReplacePosGreater());
			for (std::list<StringReplacePos>::iterator ite = replacePoses.begin(), iend = replacePoses.end(); ite != iend; ++ ite)
			{
				StringReplacePos& rep = *ite;

				if (i && rep.offset < offset)
				{
					std::string strfrom;
					strfrom.append(rep.from, rep.fromLen);
					return luaL_error(L, "string.replace from '%s' have appeared multiple times", strfrom.c_str());
				}

				lua_string_addbuf(&buf, src + offset, rep.offset - offset);
				lua_string_addbuf(&buf, rep.to, rep.toLen);

				offset = rep.offset + rep.fromLen;
				++ i;
			}

			if (offset < srcLen)
				lua_string_addbuf(&buf, src + offset, srcLen - offset);
		}

		if (i)
			luaL_pushresult(&buf);
		else
			lua_pushvalue(L, 1);

		return 1;
	}
	else if (tp2 == LUA_TSTRING)
	{
		to = lua_tolstring(L, 3, &toLen);

		if (tp1 == LUA_TSTRING)
		{
			// 字符串对字符串替换
			from = lua_tolstring(L, 2, &fromLen);
			if (fromLen > 0)
			{
				srcptr = src;
				for(;;)
				{
					foundPos = fromLen == 1 ? std::strchr(srcptr, from[0]) : std::strstr(srcptr, from);
					if (!foundPos)
						break;

					offset = foundPos - srcptr;
					lua_string_addbuf(&buf, srcptr, offset);
					lua_string_addbuf(&buf, to, toLen);	

					srcptr = foundPos + fromLen;
				}
			
				lua_string_addbuf(&buf, srcptr, srcLen - (srcptr - src));
				luaL_pushresult(&buf);
				return 1;
			}
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM字符串对普通字符串
			BMString* bms = (BMString*)lua_touserdata(L, 2);
			if (bms->m_flags != 'BMST')
				return luaL_error(L, "not a string returned by string.bmcompile", 0);

			offset = fromLen = 0;
			while((offset = bms->find((const uint8_t*)src + fromLen, srcLen - fromLen)) != -1)
			{
				if (offset > fromLen)
					lua_string_addbuf(&buf, src + fromLen, offset - fromLen);
				lua_string_addbuf(&buf, to, toLen);

				fromLen += offset + bms->m_subLen;
			}

			if (fromLen < srcLen)
				lua_string_addbuf(&buf, src + fromLen, srcLen - fromLen);
		}
	}

	lua_pushvalue(L, 1);
	return 1;
}

// 将参数1字符串中由参数3指定的位置开始到结束位置（可以不指定或用参数4指定）的字符串，用参数2来进行替换
static int lua_string_subreplaceto(lua_State* L)
{
	size_t srcLen = 0, repLen = 0;
	const char* src = (const char*)luaL_checklstring(L, 1, &srcLen);
	const char* rep = (const char*)luaL_checklstring(L, 2, &repLen);

	if (srcLen < 1)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	ptrdiff_t startp = luaL_checklong(L, 3) - 1, endp = srcLen - 1;
	if (lua_isnumber(L, 4))
		endp = lua_tointeger(L, 4);

	if (endp < 0)
		endp = srcLen + endp;
	else if (endp >= srcLen)
		endp = srcLen - 1;
	else
		endp --;

	if (endp < startp)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	if (startp > 1)
		lua_string_addbuf(&buf, src, startp - 1);
	lua_string_addbuf(&buf, rep, repLen);
	lua_string_addbuf(&buf, src + endp + 1, srcLen - endp - 1);

	luaL_pushresult(&buf);
	return 1;
}

// 将参数1字符串中由参数3指定的位置开始到指定长度（可以不指定或用参数4指定）的字符串，用参数2来进行替换
// 这个函数和上面的subreplaceto的唯一区别就是一个用的是开始+位置结束，一个用的是开始位置+长度
static int lua_string_subreplace(lua_State* L)
{
	size_t srcLen = 0, repLen = 0;
	const char* src = (const char*)luaL_checklstring(L, 1, &srcLen);
	const char* rep = (const char*)luaL_checklstring(L, 2, &repLen);

	if (srcLen < 1)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	ptrdiff_t startp = luaL_checklong(L, 3) - 1, leng = LONG_MAX;
	if (lua_isnumber(L, 4))
		leng = lua_tointeger(L, 4);

	leng = std::min(leng, (ptrdiff_t)(srcLen - startp));
	if (leng < 1)
	{
		lua_pushvalue(L, 1);
		return 0;
	}

	luaL_Buffer buf;
	luaL_buffinit(L, &buf);

	if (startp > 0)
		lua_string_addbuf(&buf, src, startp);
	lua_string_addbuf(&buf, rep, repLen);
	if (leng)
		lua_string_addbuf(&buf, src + startp + leng, srcLen - startp - leng);

	luaL_pushresult(&buf);
	return 1;
}

// 参数2指定开始位置，然后用参数3指定的结束位置或字符串进行查找以得到一个结束位置，将开始与结束位置之间的字符串取出后返回
// 用法1：string.subto('abcdefghijklmn', 'hijkl') => abcdefg   hijkl是结束位置字符串，所以sub的起点是开始到h之前
// 用法2：string.subto('abcdefghijklmn', 4, 'hijkl') => defg   同上一例，只不过指定了开始位置为第4个字符，因此前面的3个字符被扔掉了
static int lua_string_subto(lua_State* L)
{
	size_t srcLen, toLen = 0;
	int top = std::min(3, lua_gettop(L));
	const char* src = luaL_checklstring(L, 1, &srcLen), *to = 0;
	long start = 0, endp = srcLen;	

	if (top == 3)
		start = luaL_checklong(L, 2) - 1;
	if (start < 0 || srcLen < 1)
		return 0;

	if (lua_isnumber(L, top))
	{
		endp = luaL_checklong(L, top);
		if (endp < 0)
			endp = (long)srcLen + endp;
	}
	else if (lua_isstring(L, top))
	{
		to = luaL_checklstring(L, top, &toLen);
		if (!to || toLen < 1)
			return 0;

		const char* findStart = src + start;
		const char* pos = toLen == 1 ? std::strchr(findStart, to[0]) : std::strstr(findStart, to);
		if (!pos)
			return 0;
		
		if (pos == findStart)
		{
			lua_pushlstring(L, "", 0);
			return 1;
		}

		endp = pos - src - 1;
	}
	else if (lua_isnil(L, top))
	{
		endp = (long)srcLen - 1;
	}
	else
		return 0;

	if (endp < start)
		return 0;

	lua_pushlstring(L, src + start, endp - start + 1);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_findvarname(lua_State* L)
{
	size_t i, len = 0, addonLen = 0;
	const char* s = luaL_checklstring(L, 1, &len);
	ptrdiff_t off = luaL_optinteger(L, 2, 1) - 1;

	if (len < 1 || off < 0 || off >= len)
		return 0;

	uint8_t addons[128] = { 0 };
	const char* addon = luaL_optlstring(L, 3, "", &addonLen);
	for(i = 0; i < addonLen; ++ i)
		addons[(uint8_t)addon[i]] = 1;

	for(i = off; i < len; ++ i)
	{
		uint8_t ch = s[i];
		if (ch <= 32)
		{
			if (i > off)
				goto _return;
			off = i + 1;
			continue;
		}
		if (ch >= 128)
		{
			if (i > off)
				goto _return;
			return 0;
		}

		uint8_t flag = sql_where_splits[ch];
		if (flag != 2 && flag != 3 && addons[ch] == 0)
		{
			if (i > off)
				goto _return;
			off = i + 1;
		}
	}

	if (i > off)
	{
_return:
		lua_pushlstring(L, s + off, i - off);
		lua_pushinteger(L, i + 1);
		return 2;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// 计算参数2字符串中的每一个字符在参数1字符串中总共出现了多少次，注意统计的是参数2中每一个字符串在参数1中总共出现的次数
static int lua_string_countchars(lua_State* L)
{	
	size_t srcLen = 0, byLen = 0;
	uint8_t checker[256] = { 0 }, ch;

	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);
	const uint8_t* by = (const uint8_t*)luaL_checklstring(L, 2, &byLen);

	while ((ch = *by) != 0)
	{
		checker[ch] = 1;
		by ++;
	}

	int cc = 0;
	for(size_t i = 0; i < srcLen; ++ i)
	{
		if (checker[src[i]])
			cc ++;
	}

	lua_pushinteger(L, cc);
	return 1;
}

// 计算参数2字符串中每一个字符分别在参数1字符串中出现了多少次，返回为一个table
static int lua_string_counteachchars(lua_State* L)
{	
	size_t srcLen = 0, byLen = 0, i;
	uint32_t counts[256] = { 0 };
	uint8_t checker[256] = { 0 }, ch;	

	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);
	const uint8_t* by = (const uint8_t*)luaL_checklstring(L, 2, &byLen);

	for(i = 0; i < byLen; ++ i)
	{
		checker[by[i]] = 1;
		by ++;
	}

	for(i = 0; i < srcLen; ++ i)
	{
		uint8_t ch = src[i];
		if (checker[ch])
			counts[ch] ++;
	}

	int cc = 1;
	lua_createtable(L, byLen, 0);
	for(i = 0; i < byLen; ++ i)
	{
		lua_pushinteger(L, counts[by[i]]);
		lua_rawseti(L, -2, cc ++);
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
// 检测是否是数值或其它方式表示整数的数值，如果符合检测条件，则返回转换后的数值，否则返回nil或参数2（如果有参数2的话）
static int lua_string_checknumeric(lua_State* L)
{
	double d = 0;
	int r = 0, t = lua_gettop(L);

	if (t >= 1)
	{
		if (lua_isnumber(L, 1))
		{
			r = 1;
			d = lua_tonumber(L, 1);
		}
		else
		{
			size_t len = 0;
			char *endp = 0;
			const char* s = (const char*)lua_tolstring(L, 1, &len);

			if (len > 0)
			{
				d = strtod(s, &endp);
				if (endp && endp - s == len)
					r = 1;
			}
		}
	}

	if (r)
	{
		lua_pushnumber(L, d);
		return 1;
	}
	if (t >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}

	return 0;
}

// 检测是否是整数或其它方式表示整数的值，如果符合检测条件，则返回转换后的整数，否则返回nil或参数2（如果有参数2的话）
static int lua_string_checkinteger(lua_State* L)
{	
	size_t len = 0;
	long long v = 0;
	const char* s = 0;
	bool negative = false;
	int r = 0, t = lua_gettop(L), tp = 0;	

	if (t >= 1)
	{
		char *endp = 0;

		tp = lua_type(L, 1);
		if (tp == LUA_TNUMBER)
		{
			r = 1;
			v = lua_tointeger(L, 1);
		}
		else if (tp == LUA_TSTRING)
		{						
			s = (const char*)lua_tolstring(L, 1, &len);

			if (len > 0)
			{
				int digits = 10;
				if (s[0] == '0')
				{
					if (s[1] == 'x')
					{
						digits = 16;
						len -= 2;
						s += 2;						
					}
					else
					{
						digits = 8;
						len --;
						s ++;						
					}
				}
				else if (s[0] == '-')
					negative = true;

				v = strtoll(s, &endp, digits);
				if (endp && endp - s == len)
					r = 1;
			}
		}
		else if (tp == LUA_TCDATA)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);
			lua_pushvalue(L, 1);
			lua_pcall(L, 1, 1, 0);

			s = lua_tolstring(L, -1, &len);
			if (len >= 3 && cdataValueIsInt64((const uint8_t*)s, len, &len))
				r = 1;
		}
	}

	if (r)
	{
		if (tp != LUA_TCDATA)
		{
#ifdef REEME_64
			if (v > DOUBLE_UINT_MAX)
			{
				lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
				if (negative)
				{
					lua_pushliteral(L, "int64_t");
					lua_pcall(L, 1, 1, 0);

					int64_t* p64t = (int64_t*)const_cast<void*>(lua_topointer(L, -1));
					p64t[0] = (int64_t)v;
				}
				else
				{
					lua_pushliteral(L, "uint64_t");
					lua_pcall(L, 1, 1, 0);

					uint64_t* p64t = (uint64_t*)const_cast<void*>(lua_topointer(L, -1));
					p64t[0] = (uint64_t)v;
				}
			}
			else
				lua_pushnumber(L, (double)v);
#else
			lua_pushnumber(L, (double)v);
#endif
			return 1;
		}
		else
		{
			lua_pushvalue(L, 1);
			lua_pushlstring(L, s, len);
			return 2;
		}
	}
	if (t >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}

	return 0;
}

static int lua_string_checkinteger32(lua_State* L)
{
	lua_Integer resulti = 0;
	int r = 0, t = lua_gettop(L), tp = 0;

	if (t >= 1)
	{
		size_t len = 0;
		const char* s;
		char *endp = 0;

		tp = lua_type(L, 1);
		if (tp == LUA_TNUMBER)
		{
			double d = lua_tonumber(L, 1);
			if (d <= INT_MAX && d >= INT_MIN)
			{
				r = 1;
				resulti = dtoi(d);
			}
		}
		else if (tp == LUA_TSTRING)
		{
			s = (const char*)lua_tolstring(L, 1, &len);

			if (len > 0)
			{
				int digits = 10;
				if (s[0] == '0')
				{
					if (s[1] == 'x')
					{
						digits = 16;
						len -= 2;
						s += 2;
					}
					else
					{
						digits = 8;
						len --;
						s ++;
					}
				}

				long long v = strtoll(s, &endp, digits);
				if (endp && endp - s == len && v <= INT_MAX && v >= INT_MIN)
				{
					r = 1;
					resulti = v;
				}
			}
		}
	}

	if (r)
	{
		lua_pushinteger(L, resulti);
		return 1;
	}
	if (t >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}

	return 0;
}

// 检测是否是布尔值或其它方式表示的布尔值，如果符合检测条件，则返回转换后的布尔值，否则返回nil或参数2（如果有参数2的话）
static int lua_string_checkboolean(lua_State* L)
{
	int top = lua_gettop(L), r = 0, cc = 0;
	if (top >= 1)
	{
		switch (lua_type(L, 1))
		{
		case LUA_TNUMBER:
		if ((lua_isnumber(L, 1) && lua_tonumber(L, 1) != 0) || lua_tointeger(L, 1) != 0)
			r = 1;
		cc = 1;
		break;

		case LUA_TBOOLEAN:
			r = lua_toboolean(L, 1);
			cc = 1;
			break;

		case LUA_TSTRING:
		{
			size_t len = 0;
			const char* s = lua_tolstring(L, 1, &len);

			if (len == 4 && stricmp(s, "true") == 0)
				r = cc = 1;
			else if (len == 5 && stricmp(s, "false") == 0)
				cc = 1;
			else if (len == 1)
			{
				if (s[0] == '0') cc = 1;
				else if (s[0] == '1') r = cc = 1;
			}
		}
		break;
		}
	}

	if (cc)
	{
		lua_pushboolean(L, r);
		return 1;
	}
	if (top >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}
	return 0;
}

// 检测是否是字符串，如果符合检测条件，则返回该字符串，否则返回nil
static int lua_string_checkstring(lua_State* L)
{
	if (!lua_isstring(L, 1))
		return 0;

	size_t len = 0;
	int top = lua_gettop(L);
	const char* s = lua_tolstring(L, 1, &len);
	if (!s)
	{
		if (top >= 2)
		{
			lua_pushvalue(L, 2);
			return 1;
		}
		return 0;
	}

	int utf8 = 0;
	uint32_t flags = 0;
	ptrdiff_t minl = 0, maxl = len;	

	for(int n = 2; n <= top; n ++)
	{
		int t = lua_type(L, n);
		if (flags & (1 << t))
			return luaL_error(L, "error type of parameter(#%d) for string.checkstring", n);

		flags |= 1 << t;
		if (t == LUA_TNUMBER)
		{
			// 字符串最小和最大长度范围
			minl = lua_tointeger(L, n);
			if (lua_isnumber(L, n + 1))
			{
				// 如果下一个参数也是数值型的话，那么就默认为与本参数一起组成最小到最大长度允许范围
				maxl = lua_tointeger(L, 3);
				n ++;
			}

			if (utf8)
			{
				// 计算UTF8下字符串的长度
				ptrdiff_t cc = 0;
				const char* ptr = s;
				while (ptr - s < len)
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
						return luaL_error(L, "illegal char in string.checkstring(#1) with utf-8 mode", 0);

					cc ++;
				}

				if (cc < minl || cc > maxl)
				{
					flags = 0;
					break;
				}
			}
			else if (len < minl || len > maxl)
			{
				flags = 0;
				break;
			}
		}
		else if (t == LUA_TSTRING)
		{
			// 正则表达式进行匹配
#ifdef RE2_RE2_H_
			if (!RE2::FullMatch(s, lua_tostring(L, n)))
			{
				flags = 0;
				break;
			}
#else
			lua_getglobal(L, "string");
			lua_getfield(L, "match");
			lua_pushvalue(L, 1);
			lua_pushvalue(L, n);
			lua_pcall(L, 2, 1, 0);
			if (lua_isnil(L, -1))
			{
				flags = 0;
				break;
			}
#endif
		}
		else if (t == LUA_TBOOLEAN)
		{
			// 是否utf8编码的字符串
			utf8 = lua_toboolean(L, n);
		}
		else if (t == LUA_TFUNCTION)
		{
			// 使用检测函数进行检测，返回true表示检测通过
			lua_pushvalue(L, n);
			lua_pushvalue(L, 1);
			if (lua_pcall(L, 1, 1, 0) || lua_toboolean(L, -1) == 0)
			{
				flags = 0;
				break;
			}
		}
	}

	if (flags || top == 1)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
// 可用的模式有：
// %s：字符串
// %u：无符号数字
// %d：有符号数字
// %x：十六进制数字
// %f %g：浮点数
// %c：单个ansi码转字符
// 支持整数、小数、字符串的列宽控制，小数点位数控制
static int lua_string_fmt(lua_State* L)
{	
	luaL_Buffer buf;
	size_t srcLen = 0;
	char *endpos = 0, tmpbuf[64];
	const char* src = luaL_checklstring(L, 1, &srcLen), *val;

	if (srcLen < 2)
	{
		lua_pushvalue(L, 1);
		return 1;
	}

	luaL_Buffer* pBuf = &buf;
	luaL_buffinit(L, pBuf);

	double dv;
	int64_t lli;
	uint64_t lln;
	uint32_t flags, flag;
	bool hasLen = false, hasDigits = false;
	size_t start = 0, valLen, len, digits, carry;
	for (int cc = 2, tp; ; )
	{
		const char* foundpos = std::strchr(src + start, '%');
		if (!foundpos)
			break;

		size_t pos = foundpos - src;
		if (start < pos)
			lua_string_addbuf(&buf, src + start, pos - start);

		start = pos + 1;
		if (start >= srcLen)
			break;

		// 连续两个%为转义
		foundpos ++;
		char ctl = foundpos[0];
		if (ctl == '%')
		{
			start ++; 
			luaL_addchar(pBuf, '%');
			continue;
		}

		// 标志位
		flags = 0;
		flag = string_fmt_valid_fmt[ctl];
		while((flag >> 4) > 0)
		{
			flags |= flag;
			foundpos ++;
			ctl = foundpos[0];
			flag = string_fmt_valid_fmt[ctl];
		}

		// 长/宽度
		carry = 1;
		len = digits = 0;
		while (ctl >= '0' && ctl <= '9')
		{
			hasLen = true;
			len *= carry;
			len += ctl - '0';
			foundpos ++;
			carry *= 10;
			ctl = foundpos[0];
		}

		// 小数位数
		if (ctl == '.')
		{
			carry = 1;
			foundpos ++;
			hasDigits = true;
			ctl = foundpos[0];	
			while (ctl >= '0' && ctl <= '9')
			{
				digits *= carry;
				digits += ctl - '0';
				foundpos ++;
				carry *= 10;
				ctl = foundpos[0];
			}
		}

		tp = lua_type(L, cc);
		if (tp <= LUA_TNIL)
		{
			luaL_checkany(L, cc);
			return 0;
		}

		switch(flag = string_fmt_valid_fmt[ctl])
		{
		case 1: // 字符串
			if (tp != LUA_TSTRING)
			{
				lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);
				lua_pushvalue(L, cc);
				lua_pcall(L, 1, 1, 0);
				val = lua_tolstring(L, -1, &valLen);
			}
			else
				val = lua_tolstring(L, cc, &valLen);
			
			if (!val)
				return luaL_error(L, "string.fmt #%d expet string but got not string", cc - 1);

			if (tp == LUA_TCDATA)
			{
				cdataValueIsInt64((const uint8_t*)val, valLen, &valLen);
				lua_string_addbuf(pBuf, val, valLen);
			}
			else if (len > 0)
			{
				lua_string_addbuf(pBuf, val, std::min(len, valLen));
				while(len -- > valLen)
					luaL_addchar(pBuf, ' ');
			}
			else
				lua_string_addbuf(pBuf, val, valLen);

			if (tp != LUA_TSTRING)
				lua_pop(L, 1);
			break;
		
		case 2: // 浮点数
			val = 0;
			if (tp == LUA_TNUMBER)
			{				
				dv = lua_tonumber(L, cc);
			}
			else if (tp == LUA_TSTRING)
			{
				val = lua_tolstring(L, cc, &valLen);
				dv = strtod(val, &endpos);
				if (isnan(dv) || endpos - val != valLen)
					return luaL_error(L, "string.fmt #%d expet number but got not number", cc - 1);
			}
			else if (tp == LUA_TBOOLEAN)
			{
				tmpbuf[0] = lua_toboolean(L, cc);
				valLen = 1;
			}
			else
				return luaL_error(L, "string.fmt #%d expet number but got not number", cc - 1);

			if (hasDigits)
			{
				// 处理小数位数，不够digits就填充0，多了就裁掉
				valLen = opt_dtoa(dv, tmpbuf);
				val = (const char*)memchr(tmpbuf, '.', valLen);
				if (val)
				{
					len = valLen - (val - tmpbuf) - 1;
					if (digits < len)
						valLen -= len - digits;
					if (digits == 0 && !(flag & 0x40))
						valLen --;	// remove point
				}
				else
				{
					len = 0;
					if (digits)
						tmpbuf[valLen ++] = '.';
				}

				while (len ++ < digits)
					tmpbuf[valLen ++] = '0';

				val = tmpbuf;
			}
			else if (!val)
			{
				val = lua_tolstring(L, cc, &valLen);
			}

			lua_string_addbuf(pBuf, val, valLen);
			break;

		case 3:	// ansi码到字符
			if (tp == LUA_TSTRING)
			{
				val = lua_tolstring(L, cc, &valLen);
			}
			else if (tp == LUA_TNUMBER)
			{
				tmpbuf[0] = lua_tointeger(L, cc);
				val = tmpbuf;
			}
			else
				return luaL_error(L, "string.fmt #%d expet ansi code but got ansi code", cc - 1);

			luaL_addchar(pBuf, val[0]);
			break;

		case 0:	// 错误
			return luaL_error(L, "the %u-ith char '%c' invalid", foundpos - src + 1, ctl);
		
		default: // 整数
			if (tp == LUA_TNUMBER)
			{
				lli = lua_tointeger(L, cc);
			}
			else if (tp == LUA_TSTRING)
			{
				val = lua_tolstring(L, cc, &valLen);
				if (valLen >= 3 && val[0] == '0' && val[1] == 'x')
				{
					val += 2;
					valLen -= 2;
				}

				lli = strtoll(val, &endpos, 10);
				if (endpos - val != valLen)
					return luaL_error(L, "string.fmt #%d expet integer but got not integer", cc - 1);
			}
			else if (tp == LUA_TCDATA)
			{
				lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);
				lua_pushvalue(L, cc);
				lua_pcall(L, 1, 1, 0);

				val = lua_tolstring(L, -1, &valLen);
				if (!cdataValueIsInt64((const uint8_t*)val, valLen, &valLen))
					return luaL_error(L, "string.fmt #%d expet integer but got not integer", cc - 1);

				lli = strtoll(val, &endpos, 10);
				lua_pop(L, 1);
			}
			else if (tp == LUA_TBOOLEAN)
			{
				lli = lua_toboolean(L, cc);
			}
			else
				return luaL_error(L, "string.fmt #%d expet integer but got not integer", cc - 1);

			lln = lli;
			switch(flag)			
			{
			case 4:
				if (lln > UINT_MAX)
					valLen = opt_u64toa(lln, tmpbuf);
				else
					valLen = opt_u32toa(lln, tmpbuf);
				break;

			case 5:
				if (lli >= INT_MIN || lli <= INT_MAX)
					valLen = opt_i32toa(lli, tmpbuf);
				else
					valLen = opt_i64toa(lli, tmpbuf);
				break;

			case 6:
				if (lln > UINT_MAX)
					valLen = opt_u64toa_hex(lln, tmpbuf, false);
				else
					valLen = opt_u32toa_hex(lln, tmpbuf, false);
				break;

			case 7:
				if (lln > UINT_MAX)
					valLen = opt_u64toa_hex(lln, tmpbuf, true);
				else
					valLen = opt_u32toa_hex(lln, tmpbuf, true);
				break;
			}

			// 位数不足先补0
			while(len -- > valLen)
				luaL_addchar(pBuf, '0');

			lua_string_addbuf(pBuf, tmpbuf, valLen);
			break;
		}

		start = foundpos - src + 1;
		++ cc;
	}

	if (start < srcLen)
		lua_string_addbuf(pBuf, src + start, srcLen - start);

	luaL_pushresult(pBuf);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
#define TP_FIXED	16384

static const char templInitCode[] = { "return function(self, __env__)\nlocal __ret__ = {}\n" };
static const char templReturnCode[] = { "\nreturn table.concat(__ret__, '')\nend" };
static const char tenplSetvarCode[] = { "__ret__[#__ret__+1]=" };
static const char templSubtemplCode[] = { "subtemplate(self, __env__, " };
static const char templErrTipCode[] = { ", the full template parsed code: <br/><br/>\r\n\r\n" };

class TemplateParser
{
public:
	std::string	buf, errorMsg;
	size_t		offset, srcLen, wrote, mlsLength;
	const char	*src;
	const char	*savedPos;
	const char	*errorStart;
	char		mlsBegin[32], mlsEnd[32];
	bool		bracketOpened;

	enum KeywordEndType {
		KEND_NONE,
		KEND_THEN,
		KEND_DO,
	};
	static KeywordEndType checkTemplateKeywords(const char* exp, const char* expEnd, size_t leng)
	{
		char buf[8] = { 0 };
		const size_t revLeng[] = { 0, 4, 2 };
		const char cmps[][8] = { { "if" }, { "elseif" }, { "while" }, { "for" } };

		for (int i = 0; i < 8; ++ i)
		{
			char ch = exp[i];
			if (ch <= 32) break;
			buf[i] = ch;
		}

		KeywordEndType r = KEND_NONE;
		uint64_t a = *(const uint64_t*)buf;

		if (a == *(uint64_t*)cmps[0] || a == *(uint64_t*)cmps[1])
			r = KEND_THEN;
		else if (a == *(uint64_t*)cmps[2] || a == *(uint64_t*)cmps[3])
			r = KEND_DO;

		if (r != KEND_NONE)
		{
			const char* revFind = expEnd - 1;
			while (revFind > exp)
			{
				if ((uint8_t)revFind[0] <= 32 || revFind[0] == '}')
					revFind --;
				else
					break;
			}			

			revFind -= revLeng[r];
			if (r == KEND_THEN && strncmp(revFind, " then", 5))
				return KEND_THEN;
			if (r == KEND_DO && strncmp(revFind, " do", 3))
				return KEND_DO;
		}

		return KEND_NONE;
	}

	size_t append(size_t s)
	{
		if (s == 0)
			return 0;

		size_t used = buf.size() + 400;
		if (used >= TP_FIXED)
			return 0;

		open();

		s = std::min(s, (size_t)(TP_FIXED - used));
		buf.append(src + offset, s);
		offset += s;

		wrote = buf.size();
		return s;
	}

	const char* findExpEnd(const char* expStart, const char* totalEnd, uint32_t *pLinesCC = 0, uint32_t *pQuoted = 0)
	{
		uint8_t quoted = 0, ln = 0;
		const char* expEnd = expStart;

		while (expEnd < totalEnd)
		{
			char ch = *expEnd ++;
			if (quoted)
			{
				expEnd ++;
				if (ch == '\\')
					expEnd ++;
				else if (ch == '\'' || ch == '"')
					quoted = 0;
				else if (ch == '\n')
					ln = 1;
			}
			else if (ch == '\'' || ch == '"')
			{
				expEnd ++;
				if (quoted)
				{
					errorStart = expStart;
					errorMsg = "not closed string";
					expEnd = 0;
					break;
				}
				quoted = ch;
			}
			else if (ch == '\n')
				ln = 1;
			else if (ch == '}')
				break;
		}

		if (pLinesCC)
			*pLinesCC = ln;
		if (pQuoted)
			*pQuoted = quoted;

		return expEnd;
	}

	bool more()
	{
		uint8_t ch;
		size_t add;

		if (offset)
		{
			buf.clear();
			wrote = 0;
		}

		if (offset >= srcLen)
			return false;

		for(;;)
		{
			const char* foundPos = savedPos ? savedPos : (const char*)std::memchr(src + offset, '{', srcLen - offset);
			savedPos = 0;

			if (!foundPos)
				return false;

			size_t pos = foundPos - src;
			if (pos > srcLen - 3)
				break;

			foundPos ++;
			ch = foundPos[0];

			if (ch <= 32)
			{
				append(foundPos - src - offset);
				goto _lastcheck;
			}

			if (ch == '\\')
			{
				ch = foundPos[1];
				if (ch == '=' || ch == ':' | ch == '%')
				{
					// 大括号转义
					add = pos - offset + 1;
					if (pos >= offset && append(add) != add)
					{
						savedPos = foundPos;
						return true;
					}
				}

				offset = pos + 2;
				continue;
			}
			else if (ch == '=' || ch == ':')
			{
				add = pos - offset;
				if (pos >= offset && append(add) != add)
				{
					savedPos = foundPos;
					return true;
				}

				const char* varBegin = foundPos + 1;
				while((uint8_t)varBegin[0] <= 32)
					varBegin ++;

				const char* varEnd = varBegin + 1;
				for ( ; ; varEnd ++)
				{
					uint8_t ch2 = varEnd[0];
					if (ch2 == '}')
						break;
				}

				close();

				// 引用子模板
				if (ch == ':')
				{
					buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
					buf.append(templSubtemplCode, sizeof(templSubtemplCode) - 1);
					buf += '\'';
					buf.append(varBegin, varEnd - varBegin);
					buf.append("\')", 2);					
				}
				else
				{
					buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
					buf.append(varBegin, varEnd - varBegin);
				}

				buf += '\n';
				wrote = buf.size();

				open();

				foundPos = varEnd + 1;
				offset = foundPos - src;
				goto _lastcheck;
			}
			else if (ch == '%')
			{
				// 先关闭之前的输出，因为表达式不可能与字符串也不可能与其它的表达式位于同一行
				add = pos - offset;
				if (pos >= offset && append(add) != add)
				{
					savedPos = foundPos - 1;
					return true;
				}

				close();

				// 找到表达式的结束位置
				uint32_t ln = 0, quoted = 0;
				const char* expStart = foundPos + 1, *expEnd;
				const char* totalEnd = src + srcLen;

				while(expStart[0] <= 32)
					expStart ++;

				expEnd = findExpEnd(expStart, totalEnd, &ln, &quoted);
				if (!expEnd)
					return false;
				if (expEnd <= totalEnd)
				{					
					add = expEnd - expStart - 1;
					if (add >= 14 && memcmp(expStart, "subtemplate", 11) == 0 && sql_where_splits[expStart[11]] == 1)
					{
						// 特殊处理subtemplate
						buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
						buf.append(templSubtemplCode, sizeof(templSubtemplCode) - 1);

						expStart += 11;
						while(expStart[0] <= 32)
							expStart ++;

						if (expStart[0] == '(')
							expStart ++;

						add = expEnd - expStart - 1;
						buf.append(expStart, add);

						const char* revFindBracket = expEnd - 1;
						while (revFindBracket > expStart)
						{
							if ((uint8_t)revFindBracket[0] <= 32)
								revFindBracket --;
							else if (revFindBracket[0] == ')')
								revFindBracket = 0;
							else
								break;
						}

						if (revFindBracket)
							buf += ')';
					}
					else
					{
						// 其它的表达式，判断一下是否是某些关键字，如果是的话，则再判断有没有相应的then do语句。这个地方是很容易被忘记的，因此由程序来自动添加
						buf.append(expStart, add);

						if (ln == 0 && add >= 4)
						{
							switch (checkTemplateKeywords(expStart, expEnd, add))
							{
							case KEND_THEN:
								buf.append(" then", 5);
								break;
							case KEND_DO:
								buf.append(" do", 3);
								break;
							}
						}
					}

					buf += '\n';
					wrote = buf.size();

					foundPos = expEnd;
					offset = foundPos - src;
					goto _lastcheck;
				}
			}
			else if (ch == '?')
			{
				// 关闭之前的输出，模板部分缓存功能不可能与其它位于同一行
				add = pos - offset;
				if (pos >= offset && append(add) != add)
				{
					savedPos = foundPos - 1;
					return true;
				}

				close();

				// 判断是开始还是结束
				uint32_t quoted = 0;
				const char* cmdStart = foundPos + 1;
				const char* totalEnd = src + srcLen;

				while(cmdStart[0] <= 32)
					cmdStart ++;

				const char* cmdEnd = findExpEnd(cmdStart, totalEnd, 0, &quoted);
				if (!cmdEnd)
					return false;
				if (cmdEnd <= totalEnd)
				{
					size_t add = cmdEnd - cmdStart - 1;
					if (add == 0)
					{
						// 结束
						buf.append("cachesection(false, __ret__, __cachesecs__)\nend");
					}
					else
					{
						// 开始
						buf.append("if cachesection(true, __ret__, __cachesecs__");
						if (add)
						{
							buf += ',';
							buf.append(cmdStart, add);
						}

						buf.append(") then", 6);
					}
				}

				buf += '\n';
				wrote = buf.size();

				foundPos = cmdEnd;
				offset = foundPos - src;
				goto _lastcheck;
			}

			append(foundPos - src - offset);

_lastcheck:
			if (buf.size() + 400 >= TP_FIXED)
				break;	// 缓存只有400字节的空闲，就不使用了
		}

		return true;
	}

	void open()
	{
		if (!bracketOpened)
		{
			bracketOpened = true;
			buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
			buf.append(mlsBegin, mlsLength);
			wrote = buf.size();
		}
	}
	void close()
	{
		if (bracketOpened)
		{
			bracketOpened = false;
			buf.append(mlsEnd, mlsLength);
			buf += '\n';
			wrote = buf.size();
		}
	}
};
static const char *lua_tpl_loader(lua_State *L, void *ud, size_t *size)
{
	TemplateParser* ctx = (TemplateParser*)ud;
	
	if (!ctx->more())
	{
		size_t left = ctx->srcLen - ctx->offset;
		if (left)
		{
			left = std::min(left, (size_t)TP_FIXED - ctx->buf.size());

			ctx->open();
			ctx->buf.append(ctx->src + ctx->offset, left);

			ctx->offset = ctx->srcLen;
		}

		if (ctx->wrote)
		{
			ctx->close();
			*size = ctx->wrote;
			return ctx->buf.c_str();
		}

		if (ctx->src)
		{
			ctx->src = 0;
			*size = sizeof(templReturnCode) - 1;
			return templReturnCode;
		}

		*size = 0;
		return 0;
	}

	ctx->close();

	*size = ctx->wrote;
	ctx->wrote = 0;
	
	return ctx->buf.c_str();
}
static void _init_TemplateParser(TemplateParser& parser, const char* src, size_t srcLen)
{
	parser.savedPos = 0;
	parser.errorStart = 0;
	parser.src = src;
	parser.srcLen = srcLen;
	parser.offset = 0;
	parser.bracketOpened = false;
	parser.wrote = sizeof(templInitCode) - 1;

	parser.buf.reserve(TP_FIXED);
	parser.buf.append(templInitCode, parser.wrote);
}
static int lua_string_parsetemplate(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	size_t srcLen = 0;	
	const char* src = luaL_checklstring(L, 2, &srcLen);
	const char* chunkName = "__templ_tempr__";	

	int hasEnv = lua_istable(L, 3);
	if (lua_isstring(L, 4))
		chunkName = lua_tostring(L, 3);

	TemplateParser parser;	
	_init_TemplateParser(parser, src, srcLen);

	// 确认要用的多行字符串表示法没有在代码中被使用过，如果有用到了，则继续增加=号的数量
	memset(parser.mlsBegin, 0, sizeof(parser.mlsBegin) * 2);

	strcpy(parser.mlsBegin, "[===[");
	strcpy(parser.mlsEnd, "]===]");
	parser.mlsLength = 5;

	for(int i = 0; ; ++ i)
	{
		if (!std::strstr(src, parser.mlsEnd))
			break;
		if (i == 27)
		{
			// it is possible?
			lua_pushliteral(L, "multi-line string split find failed");
			return 1;
		}

		size_t l = parser.mlsLength - 1;
		parser.mlsBegin[l] = parser.mlsEnd[l] = '=';
		parser.mlsBegin[l + 1] = '[';
		parser.mlsEnd[l + 1] = ']';
		parser.mlsLength ++;
	}

	if (hasEnv)
	{
		int r = lua_load(L, &lua_tpl_loader, &parser, chunkName);
		if (parser.errorMsg.length())
		{
			std::string& msg = parser.errorMsg;
			msg += " start at : ";
		
			size_t left = std::min(srcLen - (parser.errorStart - parser.src), (size_t)60);
			msg.append(parser.errorStart, left);

			lua_pushlstring(L, msg.c_str(), msg.length());
		}
		else if (r)
		{
			// 出错，将错误信息组合起来
			size_t len = 0;
			luaL_Buffer buf;
			TemplateParser fullcode;
			const char* err = lua_tolstring(L, -1, &len);

			_init_TemplateParser(fullcode, src, srcLen);					
			
			luaL_buffinit(L, &buf);
			lua_string_addbuf(&buf, err, len);
			lua_string_addbuf(&buf, templErrTipCode, sizeof(templErrTipCode) - 1);

			for (;;)
			{
				const char* ss = lua_tpl_loader(L, &fullcode, &len);
				if (!ss || len == 0)
					break;

				lua_string_addbuf(&buf, ss, len);
			}

			luaL_pushresult(&buf);
		}
		else if (lua_isfunction(L, -1))
		{
			r = lua_pcall(L, 0, 1, 0);
			if (r == 0)
			{
				assert(lua_isfunction(L, -1));

				int top = lua_gettop(L);

				lua_pushvalue(L, 3);
				lua_setfenv(L, top);

				lua_pushvalue(L, 1);
				lua_pushvalue(L, 3);
				lua_pcall(L, 2, 1, 0);
			}
		}
	}
	else
	{
		luaL_Buffer buf;
		luaL_buffinit(L, &buf);

		for (;;)
		{
			size_t s = 0;
			const char* ss = lua_tpl_loader(L, &parser, &s);
			if (!ss || s == 0)
				break;
			
			lua_string_addbuf(&buf, ss, s);
		}

		luaL_pushresult(&buf);
	}


	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_bmcompile(lua_State* L)
{
	size_t len;
	const char* src;
	BMString* bms;
	
	if (lua_istable(L, 1))
	{
		luaL_checkstack(L, 1, "string.bmcompile only build from one table");
		
		size_t totals = 0, cc = 0;
		int i, t = lua_objlen(L, 1);
		
		for(i = 1; i <= t; ++ i)
		{
			lua_rawgeti(L, 1, i);

			len = 0;
			src = lua_tolstring(L, -1, &len);
			if (!src || len == 0)
				return luaL_error(L, "string.bmcompile cannot compile a empty string", 0);

			len =  BMString::calcAllocSize(len);
			if (len)
			{
				totals += len;
				cc ++;
			}
		}

		if (cc)
		{			
			char* mem = (char*)lua_newuserdata(L, totals);
			for (i = 2, ++ t; i <= t; ++ i)
			{
				src = lua_tolstring(L, i, &len);
				if (src)
				{
					bms = (BMString*)mem;
					bms->setSub(src, len);
					bms->m_flags = 'BMST';
					bms->m_hasNext = -- cc;
					bms->makePreTable();

					mem += BMString::calcAllocSize(len);
				}
			}

			return 1;
		}
	}
	else
	{
		int n = 1, t = lua_gettop(L);
		for( ; n <= t; ++ n)
		{
			len = 0;
			src = luaL_checklstring(L, 1, &len);
			if (!src || len == 0)
				return luaL_error(L, "string.bmcompile cannot compile a empty string", 0);

			bms = (BMString*)lua_newuserdata(L, BMString::calcAllocSize(len));
			bms->setSub(src, len);
			bms->m_flags = 'BMST';
			bms->m_hasNext = 0;
			bms->makePreTable();
		}

		return t;
	}

	return 0;
}

static int lua_string_bmfind(lua_State* L)
{
	size_t srcLen;
	const uint8_t* src = (const uint8_t*)luaL_checklstring(L, 1, &srcLen);

	BMString* bms = (BMString*)lua_touserdata(L, 2);
	if (!bms || bms->m_flags != 'BMST')
		return luaL_error(L, "not a string returned by string.bmcompile", 0);

	size_t pos = bms->find(src, srcLen);
	if (pos != -1)
	{
		lua_pushinteger(L, pos + 1);
		return 1;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
#define NODESIZE 8192 - sizeof(JsonMemNode)

class JsonMemNode : public TListNode<JsonMemNode>
{
public:
	size_t		used;
};

class JsonMemList : public TList<JsonMemNode>
{
public:
	lua_State		*L;

	JsonMemNode* newNode(size_t size = NODESIZE)
	{
		JsonMemNode* n = (JsonMemNode*)malloc(size + sizeof(JsonMemNode));
		new (n) JsonMemNode();
		n->used = 0;

		append(n);
		return n;
	}
	void addChar(char ch)
	{
		JsonMemNode* n = (JsonMemNode*)m_pLastNode;
		if (n->used >= NODESIZE)
			n = newNode();

		char* ptr = (char*)(n + 1);
		ptr[n->used ++] = ch;
	}
	void addChar2(char ch1, char ch2)
	{
		JsonMemNode* n = (JsonMemNode*)m_pLastNode;
		if (n->used + 1 >= NODESIZE)
			n = newNode();

		char* ptr = (char*)(n + 1);
		size_t used = n->used;
		ptr[used] = ch1;
		ptr[used + 1] = ch2;
		n->used += 2;
	}
	void addString(const char* s, size_t len)
	{
		JsonMemNode* n = (JsonMemNode*)m_pLastNode;
		char* ptr = (char*)(n + 1);

		size_t copy = std::min(NODESIZE - n->used, len);
		memcpy(ptr + n->used, s, copy);		
		len -= copy;
		n->used += copy;

		if (len > 0)
		{
			// 剩下的直接一次分配够
			n = newNode(std::max(NODESIZE, len));
			ptr = (char*)(n + 1);

			memcpy(ptr + n->used, s + copy, len);
			n->used += len;
		}
	}
	char* reserve(size_t len)
	{
		char* ptr;
		JsonMemNode* n = (JsonMemNode*)m_pLastNode;
		if (n->used + len < NODESIZE)
		{
			ptr = (char*)(n + 1);
			ptr += n->used;
		}
		else
		{
			n = newNode(std::max(NODESIZE, len));
			ptr = (char*)(n + 1);
		}

		n->used += len;
		return ptr;
	}
	void escapeJsonString(const char* src, size_t len)
	{		
		uint8_t ch, v;
		uint32_t unicode;
		size_t i = 0, spos = 0;
		while (i < len)
		{
			uint8_t ch = src[i];
			v = json_escape_chars[ch];

			if (v == 0)
			{
				// defered
				++ i;
				continue;
			}

			if (i > spos)
				addString(src + spos, i - spos);

			if (v == 1)
			{
				// escape some chars
				addChar2('\\', ch);
				spos = ++ i;
			}
			else if (v == 2)
			{
				// check utf8
				uint8_t* utf8src = (uint8_t*)src + i;
				if ((ch & 0xE0) == 0xC0)
				{
					//2 bit count
					unicode = ch & 0x1F;
					unicode = (unicode << 6) | (utf8src[1] & 0x3F);
					i += 2;
				}
				else if ((ch & 0xF0) == 0xE0)
				{
					//3 bit count
					unicode = ch & 0xF;
					unicode = (unicode << 6) | (utf8src[1] & 0x3F);
					unicode = (unicode << 6) | (utf8src[2] & 0x3F);
					i += 3;
				}
				else if ((ch & 0xF8) == 0xF0)
				{
					//4 bit count
					unicode = ch & 0x7;
					unicode = (unicode << 6) | (utf8src[1] & 0x3F);
					unicode = (unicode << 6) | (utf8src[2] & 0x3F);
					unicode = (unicode << 6) | (utf8src[3] & 0x3F);
					i += 4;
				}
				else
				{
					assert(0);
				}

				char* utf8dst = reserve(6);
				utf8dst[0] = '\\';
				utf8dst[1] = 'u';
				opt_u32toa_hex(unicode, utf8dst + 2, false);

				spos = i;
			}
			else
			{
				// invisible(s) to visibled
				addChar2('\\', v);
				spos = ++ i;
			}
		}

		if (i > spos)
			addString(src + spos, i - spos);
	}
	void copyLuaString(const char* src, size_t len, uint32_t eqSymbols)
	{
		char* dst;
		uint32_t i;

		dst = reserve(eqSymbols + 2);
		*dst ++ = '[';
		for (i = 0; i < eqSymbols; ++ i)
			dst[i] = '=';
		dst[i] = '[';

		addString(src, len);

		dst = reserve(eqSymbols + 2);
		*dst ++ = ']';
		for (i = 0; i < eqSymbols; ++ i)
			dst[i] = '=';
		dst[i] = ']';
	}
	void base64EncodeCData(const char* ptr, size_t len)
	{

	}
};

#define jsonConvValue()\
	switch(lua_type(L, -1)) {\
	case LUA_TTABLE:\
		if (recursionJsonEncode(L, mem, base + 1, flags, funcsIdx) == -1)\
			return -1;\
		break;\
	case LUA_TNUMBER:\
		v = lua_tonumber(L, -1);\
		ival = (int64_t)v;\
		if (v == ival) {\
			if (v < 0) {\
				if (ival < INT_MIN)\
					len = opt_i64toa(ival, buf);\
				else\
					len = opt_i32toa(ival, buf);\
			} else if (ival <= UINT_MAX)\
				len = opt_u32toa(ival, buf);\
			else\
				len = opt_u64toa(ival, buf);\
		} else {\
			len = opt_dtoa(v, buf);\
		}\
		mem->addString(buf, len);\
		break;\
	case LUA_TSTRING:\
		ptr = lua_tolstring(L, -1, &len);\
		if (flags & kJsonLuaString) {\
			mem->copyLuaString(ptr, len, flags & 0xFFFFFF);\
		} else {\
			mem->addChar('"');\
			mem->escapeJsonString(ptr, len);\
			mem->addChar('"');\
		}\
		break;\
	case LUA_TCDATA:\
		len = lua_objlen(L, -1);\
		lua_pushvalue(L, funcsIdx[1]);\
		lua_pushvalue(L, -2);\
		lua_pcall(L, 1, 1, 0);\
		ptr = lua_tolstring(L, -1, &len);\
		if (cdataValueIsInt64((const uint8_t*)ptr, len, &len)) {\
			mem->addString(ptr, len);\
		} else {\
			ptr = (const char*)lua_topointer(L, -1);\
			mem->base64EncodeCData(ptr, len);\
		}\
		break;\
	case LUA_TBOOLEAN:\
		if (lua_toboolean(L, -1))\
			mem->addString("true", 4);\
		else\
			mem->addString("false", 5);\
		break;\
	case LUA_TLIGHTUSERDATA:\
	case LUA_TUSERDATA:\
		if (lua_touserdata(L, -1) == NULL)\
			mem->addString("null", 4);\
		break;\
	}

static int recursionJsonEncode(lua_State* L, JsonMemList* mem, int tblIdx, uint32_t flags, int* funcsIdx)
{
	double v;
	size_t len;	
	char buf[64];
	int64_t ival;
	const char* ptr;	

	size_t arr = lua_objlen(L, tblIdx), cc = 0;
	if (arr == 0)
	{
		int base = lua_gettop(L) + 1;

		lua_pushnil(L);
		while(lua_next(L, tblIdx))
		{
			ptr = lua_tolstring(L, -2, &len);

			mem->addChar2(cc ? ',' : '{', '"');
			mem->addString(ptr, len);
			mem->addChar2('"', ':');

			jsonConvValue();

			lua_settop(L, base);
			cc ++;
		}

		if (cc)
			mem->addChar('}');
		else
			mem->addChar2('[', ']');
	}
	else
	{
		int base = lua_gettop(L);

		mem->addChar('[');

		for (size_t n = 1; n <= arr; ++ n)
		{
			lua_rawgeti(L, tblIdx, n);

			if (n > 1)
				mem->addChar(',');

			jsonConvValue();

			lua_settop(L, base);
			cc ++;
		}

		mem->addChar(']');
	}

	return cc;
}

static int pushJsonString(lua_State* L, const char* v, size_t len, uint32_t retCData, int32_t* funcs)
{
	if (retCData)
	{
		lua_pushvalue(L, funcs[0]);
		lua_pushliteral(L, "uint8_t[?]");
		lua_pushinteger(L, len);
		lua_pcall(L, 2, 1, 0);

		void* dst = const_cast<void*>(lua_topointer(L, -1));
		if (dst)
		{
			memcpy(dst, v, len);
			return 1;
		}

		return 0;
	}

	lua_pushlstring(L, v, len);
	return 1;
}
static int pushJsonString(lua_State* L, JsonMemList& mems, size_t total, uint32_t retCData, int32_t* funcs)
{
	JsonMemNode* n;
	if (retCData)
	{
		lua_pushvalue(L, funcs[0]);
		lua_pushliteral(L, "uint8_t[?]");
		lua_pushinteger(L, total);
		lua_pcall(L, 2, 1, 0);

		char* dst = (char*)const_cast<void*>(lua_topointer(L, -1));
		if (dst)
		{
			while ((n = mems.popFirst()) != NULL)
			{
				memcpy(dst, (char*)(n + 1), n->used);
				dst += n->used;
				free(n);
			}

			return 1;
		}

		return 0;
	}

	char* dst = (char*)malloc(total), *ptr = dst;
	while ((n = mems.popFirst()) != NULL)
	{
		memcpy(ptr, (char*)(n + 1), n->used);
		ptr += n->used;
		free(n);
	}

	lua_pushlstring(L, dst, total);
	return 1;
}

// 参数1是JSON字符串，返回值有两个，一个是解出来的Table，另外一个是用掉的字符串的长度。如果第2个返回值为nil则表示解析JSON的时候出错了
// 参数2可以在当{}或[]为空的时候，标识出这是一个Object还是一个Array
// 如果用到了cdata类型（包括boxed 64bit integer），那么必须在调用之前require('ffi')
static int lua_string_json(lua_State* L)
{
	int top = lua_gettop(L);
	int tp = lua_type(L, 1);

	if (tp == LUA_TSTRING || tp == LUA_TCDATA)
	{
		// json string to lua table
		size_t len = 0;
		bool copy = true;
		const char* str;
		int needSetMarker = 0;
		
		if (tp == LUA_TSTRING)
		{
			str = luaL_checklstring(L, 1, &len);
		}
		else
		{
			// 使用sizeof函数求字符串长度
			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFISizeof);
			lua_pushvalue(L, 1);
			lua_pcall(L, 1, 1, 0);

			len = lua_tointeger(L, -1);
			str = (const char*)lua_topointer(L, 1);
		}

		if (!str || len < 2)
			return 0;

		if (top >= 2 && !lua_isnil(L, 2))
			needSetMarker = 2;
		if (top >= 3)
		{
			lua_Integer flags = luaL_optinteger(L, 3, 0);
			if (flags & kJsonNoCopy)
				copy = false;
		}

		lua_newtable(L);
		top = lua_gettop(L);

		JSONFile f(L);
		size_t readlen = f.parse(str, len, copy, needSetMarker);
		if (readlen == 0)
		{
			// error
			char err[512], summary[64] = { 0 };
		
			f.summary(summary, 63);
			size_t errl = snprintf(err, 512, "JSON parse error: %s, position is approximately at: %s", f.getError(), summary);
		
			lua_settop(L, top - 1);
			lua_pushlstring(L, err, errl);
			return 1;
		}

		lua_settop(L, top);
		lua_pushinteger(L, readlen);
		return 2;
	}
	else if (tp == LUA_TTABLE)
	{
		// lua table to json string
		JsonMemList memList;
		int32_t funcs[2] = { 0 };
		uint32_t flags = 0;

		memList.L = L;
		memList.newNode();

		if (top >= 2)
			flags = luaL_optinteger(L, 2, 0);

		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);

		funcs[1] = lua_gettop(L);
		funcs[0] = funcs[1] - 1;

		if (recursionJsonEncode(L, &memList, 1, flags, funcs) != -1)
		{
			// 过程中没有任何问题，于是返回结果。如果参数2的标志位中含有kJsonRetCData，那么将返回一个uint8_t型的cdata内存块，否则将返回一个lua string
			int r = 0;
			JsonMemNode* n = memList.first();
			if (memList.size() == 1)
			{
				r = pushJsonString(L, (char*)(n + 1), n->used, flags & kJsonRetCData, funcs);
				free(n);
			}
			else
			{
				size_t total = 0;
				while (n)
				{
					total += n->used;
					n = n->next();
				}

				r = pushJsonString(L, memList, total, flags & kJsonRetCData, funcs);
			}

			return r;
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
static void luaext_string(lua_State *L)
{
	const luaL_Reg procs[] = {
		// 字符串切分
		{ "split", &lua_string_split },
		// trim函数
		{ "trim", &lua_string_trim },
		// 字符串比较
		{ "cmp", &lua_string_cmp },

		// 单个字符/字符串正向查找，不需要用正则时的常规查找（3~5倍性能于string.find(str, by, 1, true)）
		{ "plainfind", &lua_string_plainfind },
		// 单个字符反向查找（不支持字符串反向）
		{ "rfindchar", &lua_string_rfindchar },
		// 对字符串进行所有字符出现次数的总计数
		{ "countchars", &lua_string_countchars },
		// 对字符串进行每一个字符出现次数的分别计数
		{ "counteachchars", &lua_string_counteachchars },

		// 字符串多模式快速替换，支持单对单、数组对数组，bm字符串组对普通字符串数组等
		{ "replace", &lua_string_replace },
		// 字符串指定位置+结束位置替换
		{ "subreplaceto", &lua_string_subreplaceto },
		// 字符串指定位置+长度替换
		{ "subreplace", &lua_string_subreplace },
		// 字符串查找带截取
		{ "subto", &lua_string_subto },

		// 从指定的位置开始取一个标准变量名长度的字符串
		{ "findvarname", lua_string_findvarname },

		// 数值+浮点数字符串检测
		{ "checknumeric", &lua_string_checknumeric },
		// 整数字符串检测，支持boxed int64
		{ "checkinteger", &lua_string_checkinteger },
		{ "checkinteger32", &lua_string_checkinteger32 },
		// 布尔型检测（允许数值、字符串和boolean类型）
		{ "checkboolean", &lua_string_checkboolean },
		// 字符串检测，可以检测最小最大长度、使用正则(google re2)匹配、使用函数检测等
		{ "checkstring", &lua_string_checkstring },

		// 字符串格式化，大部分时候和string.format可以直接替换使用，执行性能上综合比string.format稍微慢了一点，但关键是支持boxed int64类型，格式化为字符串和数字均可(string.format不支持)
		{ "fmt", &lua_string_fmt },
		// 模板解析，使用{% sentense } {= value } {: sub_template }三种语法，比简单的Lua版的template有更强的容错和适应能力，如忘了写then/do的时候可以自动补上（前提是别连写，如{% while true do if xxx }就会无法纠正）
		{ "parseTemplate", &lua_string_parsetemplate },

		// 编译Boyer-Moore子字符串用于查找
		{ "bmcompile", &lua_string_bmcompile },
		// 用编译好的BM字符串进行查找（适合于一次编译，然后在大量的文本中快速的查找一个子串，子串越长性能越优）
		{ "bmfind", &lua_string_bmfind },

		// Json解码（dec时2~4倍性能于ngx所采用的cjson，enc时随table的复杂度1.5~4.5倍性能于cjson，不过这不是关键，关键是本json encode支持boxed 64bit integer以及cdata自动编码为base64等本库才有的扩展能力）
		{ "json", &lua_string_json },

		{ NULL, NULL }
	};


	lua_getglobal(L, "string");

	// 字符串切分时可用的标志位
	lua_pushliteral(L, "SPLIT_ASKEY");		// 用切出来的值做为key
	lua_pushinteger(L, kSplitAsKey);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_TRIM");		// 每一个切出来的值先做左右trim
	lua_pushinteger(L, kSplitTrim);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_NOCOPY");		// 解码时直接在原字符串上操作（原字符串内存将遭到破坏，但如果之后将不可能再使用的话，破坏也没关系了，又可以少一次copy）
	lua_pushinteger(L, kJsonNoCopy);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_RETCDATA");	// 编码时返回uint8_t型的cdata数据而不是string
	lua_pushinteger(L, kJsonRetCData);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_LUASTRING");	// 遇到字符串时直接使用本库支持的扩展 [[ ]] 来表示而不使用标准的json字符串表示法，因此也就没有escape和转义utf8/unicode的过程
	lua_pushinteger(L, kJsonLuaString);
	lua_rawset(L, -3);

	lua_pushliteral(L, "LUA_UNICODES");		// 字符串中的utf8/unicode不要转义，直接保留使用。上一个标志JSON_LUASTRING存在时，本标志被忽略
	lua_pushinteger(L, kJsonUnicodes);
	lua_rawset(L, -3);

	// 所有扩展的函数
	luaL_register(L, NULL, procs);

	lua_pop(L, 1);
}