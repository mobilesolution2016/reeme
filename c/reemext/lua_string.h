// ������ASCII�ַ����Ա�1��ʾ���ţ�2��ʾ��Сд��ĸ��3��ʾ���֣�4��ʾ��������ķ��ţ�5��6�ǵ�ʽ����
static uint8_t sql_where_splits[128] = 
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	1,		// 0~32
	6, 1, 1, 1, 1, 1, 4, 1, 1, 4, 1, 1, 2, 2, 1,	// 33~47
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	// 48~57
	1, 1, 6, 5, 6, 4, 1,	// 58~64
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
	kJsonDropNull = 0x08000000,
	kJsonSimpleEscape = 0x04000000,
	kJsonEmptyToObject = 0x02000000
};

enum StringHtmlEntFlags {
	kHtmlEntReservedOnly = 0x1,
};

//////////////////////////////////////////////////////////////////////////
static int lua_string_addbuf(luaL_Buffer* buf, const char* str, size_t len)
{
	int addBuf = 0;
	size_t lenleft = len, copy;
	while(lenleft > 0)
	{
		copy = std::min(lenleft, (size_t)(LUAL_BUFFERSIZE - (buf->p - buf->buffer)));
		if (!copy)
		{
			addBuf ++;
			luaL_prepbuffer(buf);
			copy = std::min(lenleft, (size_t)LUAL_BUFFERSIZE);
		}

		memcpy(buf->p, str, copy);
		lenleft -= copy;
		buf->p += copy;
		str += copy;
	}

	return addBuf;
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

	// ����3��ʾΪunsigned int64��2��ʾsigned int64
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
// ������1�ַ������ղ���2�ַ����г��ֵ�ÿһ���ַ������з֣��з�ʱ�����ݲ���3�����õı�־������Ӧ�Ĵ����������4������Ϊtrue���зֵĽ�����Զ෵��ֵ���أ�������table�����зֵĽ��
// ��ʹ����1�ַ�����ȫ�����в���2�ַ����е��κ�һ���ַ���Ҳ�����һ���з֣�ֻ�Ƿ���Ϊ��������1�ַ���
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

	// ���ñ��
	size_t i, endpos;
	for(i = 0; i < byLen; ++ i)
		checker[by[i]] = 1;
	
	// ����ַ��ļ��	
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
		if (tblVal)
		{
			lua_pushinteger(L, 0);
			return 2;
		}
		
		return 0;
	}

	if (retAs == LUA_TTABLE)
	{
		lua_pushinteger(L, cc)
		return 2;
	}
	
	return cc;
}

// ����ĳ�ַ��ŷָ����ַ�����ʾ��ID��������з֣�
static int lua_string_idarray(lua_State* L)
{
	size_t len, splitByLen = 1;
	const char* src = luaL_optlstring(L, 1, NULL, &len);
	const char* splitBySrc = luaL_optlstring(L, 2, ",", &splitByLen);
	lua_Integer asInteger = luaL_optinteger(L, 3, 0);

	if (src == NULL)
		return 0;
	
	if (splitByLen != 1)
	{
		luaL_error(L, "the 2-th parameter for string.idarray must be a char");
		return 0;
	}
	if (len < 1)
	{
		lua_newtable(L);
		return 1;
	}

	int cc = 0;
	char* endPtr = NULL;
	const char* readPtr = src, *readEnd = src + len;

	lua_createtable(L, 4, 0);
	for( ; ; )
	{
		while((uint8_t)readPtr[0] <= 32 && readPtr < readEnd)
			readPtr ++;	// skip invisible chars

		if (asInteger)
			lua_pushinteger(L, strtoll(readPtr, &endPtr, 10));
		else
			lua_pushnumber(L, strtod(readPtr, &endPtr));

		if (endPtr == readPtr)
			return 0;	// error formats

		lua_rawseti(L, -2, ++ cc);

		if (endPtr[0] == splitBySrc[0])
			readPtr = endPtr + 1;
		else
			break;
	}

	if (endPtr - src == len)
		return 1;

	// error formats
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// ���ַ����������ҷǿɼ�����ȥ��������2��3���������Ϊtrue|false�ֱ��ʾ�Ƿ�Ҫ�������|�ұߡ����û�в���2��3��Ĭ�����Ҷ�����
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
// �ַ����������ַ�������ң���֧�ֶ��ַ����в��Ҳ�֧���ַ���
static int lua_string_rfindchar(lua_State* L)
{
	size_t len = 0, len2 = 0;
	const char* s = luaL_checklstring(L, 1, &len);
	const char* f = luaL_checklstring(L, 2, &len2);

	if (len && len2 == 1)
	{
		long t = luaL_optinteger(L, 3, 0);
		if (t > 1 && t <= len)
			f = strrchr(s + len - t, f[0]);
		else
			f = strrchr(s, f[0]);

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
struct StringReplacePosGreater
{
	int32_t operator () (const StringReplacePos& a, const StringReplacePos& b) { return (ptrdiff_t)a.offset - (ptrdiff_t)b.offset; }
};

struct StringReplacePosAlloc
{
	StringReplacePosAlloc()
	{
		cc = 0;
		upper = 80;
		all = fixedBuf;
	}
	~StringReplacePosAlloc()
	{
		if (all != fixedBuf)
			free(all);
	}

	StringReplacePos* alloc()
	{
		if (cc >= upper)
		{
			upper <<= 2;
			StringReplacePos* n = (StringReplacePos*)malloc(sizeof(StringReplacePos) * upper);
			if (cc)
				memcpy(n, all, sizeof(StringReplacePos) * cc);
			if (all != fixedBuf)
				free(all);
			all = n;
		}

		return &all[cc ++];
	}

	uint32_t			cc, upper;
	StringReplacePos	fixedBuf[80];
	StringReplacePos	*all;
};

// ���ֶ���ģʽ���ַ�/�ַ����滻
static int lua_string_replace(lua_State* L)
{
	int top = lua_gettop(L);
	if (top < 2)
		return 0;

	luaL_Buffer buf;	
	StringReplacePosAlloc posAlloc;
	size_t srcLen = 0, fromLen = 0, toLen = 0, offset;
	const char* src = luaL_checklstring(L, 1, &srcLen), *srcptr, *foundPos, *from, *to;

	luaL_buffinit(L, &buf);

	int tp1 = lua_type(L, 2), tp2 = lua_type(L, 3);	
	if (tp2 == LUA_TTABLE)
	{
		int i;
		if (tp1 == LUA_TTABLE)
		{
			// �ַ���������ַ�������
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
				while((foundPos = fromLen == 1 ? strchr(srcptr, from[0]) : strstr(srcptr, from)) != 0)
				{
					StringReplacePos* newrep = posAlloc.alloc();
					newrep->offset = foundPos - src;
					newrep->fromLen = fromLen;
					newrep->toLen = toLen;
					newrep->from = from;
					newrep->to = to;

					srcptr = foundPos + fromLen;
				}
			}
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM�ַ������ַ�������
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
					StringReplacePos* newrep = posAlloc.alloc();
					newrep->offset = offset + fromLen;
					newrep->fromLen = bms->m_subLen;
					newrep->toLen = toLen;
					newrep->from = (const char*)(bms + 1);
					newrep->to = to;

					fromLen += offset + newrep->fromLen;
				}

			} while ((bms = bms->getNext()) != 0);
		}
		else
			return luaL_error(L, "error type for the 2-th parameter of string.replcae", 0);
	}
	else if (tp2 == LUA_TSTRING)
	{
		int repcc = 0;
		to = lua_tolstring(L, 3, &toLen);

		if (tp1 == LUA_TSTRING)
		{
			// �ַ������ַ����滻
			from = lua_tolstring(L, 2, &fromLen);
			if (fromLen > 0)
			{				
				srcptr = src;
				for(;;)
				{
					foundPos = fromLen == 1 ? strchr(srcptr, from[0]) : strstr(srcptr, from);
					if (!foundPos)
						break;

					offset = foundPos - srcptr;
					lua_string_addbuf(&buf, srcptr, offset);
					lua_string_addbuf(&buf, to, toLen);	

					srcptr = foundPos + fromLen;
					repcc ++;
				}
			
				if (repcc)
				{
					lua_string_addbuf(&buf, srcptr, srcLen - (srcptr - src));
					luaL_pushresult(&buf);
					return 1;
				}
			}
		}
		else if (tp1 == LUA_TUSERDATA)
		{
			// BM�ַ�������ͨ�ַ���
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
				repcc ++;
			}

			if (repcc)
			{
				if (fromLen < srcLen)
					lua_string_addbuf(&buf, src + fromLen, srcLen - fromLen);

				luaL_pushresult(&buf);
				return 1;
			}
		}

		lua_pushvalue(L, 1);
		return 1;
	}
	else if (top == 2 && tp1 == LUA_TTABLE)
	{
		// keyy=>value���滻������ֻ��Ҫ����������
		lua_pushnil(L);
		while(lua_next(L, 2))
		{
			from = lua_tolstring(L, -2, &fromLen);
			if (!from || !fromLen)
				return luaL_error(L, "cannot replace from empty string by string.replace", 0);

			to = lua_tolstring(L, -1, &toLen);
			if (!to)
				to = "";

			srcptr = src;
			while((foundPos = fromLen == 1 ? strchr(srcptr, from[0]) : strstr(srcptr, from)) != 0)
			{
				StringReplacePos* newrep = posAlloc.alloc();
				newrep->offset = foundPos - src;
				newrep->fromLen = fromLen;
				newrep->toLen = toLen;
				newrep->from = from;
				newrep->to = to;

				srcptr = foundPos + fromLen;
			}

			lua_pop(L, 1);
		}
	}

	if (posAlloc.cc)
	{
		// ����֮��˳���滻
		offset = 0;
		uint32_t cc = posAlloc.cc;
		StringReplacePos* allPoses = posAlloc.all;

		tsort<StringReplacePos>(allPoses, cc, StringReplacePosGreater(), structswap<StringReplacePos>());
		for(uint32_t i = 0; i < cc; ++ i)
		{
			StringReplacePos& rep = allPoses[i];
			assert(i == 0 || rep.offset >= offset);

			lua_string_addbuf(&buf, src + offset, rep.offset - offset);
			lua_string_addbuf(&buf, rep.to, rep.toLen);

			offset = rep.offset + rep.fromLen;
		}

		if (offset < srcLen)
			lua_string_addbuf(&buf, src + offset, srcLen - offset);

		if (cc)
			luaL_pushresult(&buf);
		else
			lua_pushvalue(L, 1);
	}
	else
		lua_pushvalue(L, 1);

	return 1;
}

// ������1�ַ������ɲ���3ָ����λ�ÿ�ʼ������λ�ã����Բ�ָ�����ò���4ָ�������ַ������ò���2�������滻
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

// ������1�ַ������ɲ���3ָ����λ�ÿ�ʼ��ָ�����ȣ����Բ�ָ�����ò���4ָ�������ַ������ò���2�������滻
// ��������������subreplaceto��Ψһ�������һ���õ��ǿ�ʼ+λ�ý�����һ���õ��ǿ�ʼλ��+����
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

// ����2ָ����ʼλ�ã�Ȼ���ò���3ָ���Ľ���λ�û��ַ������в����Եõ�һ������λ�ã�����ʼ�����λ��֮����ַ���ȡ���󷵻�
// �÷�1��string.subto('abcdefghijklmn', 'hijkl') => abcdefg   hijkl�ǽ���λ���ַ���������sub������ǿ�ʼ��h֮ǰ
// �÷�2��string.subto('abcdefghijklmn', 4, 'hijkl') => defg   ͬ��һ����ֻ����ָ���˿�ʼλ��Ϊ��4���ַ������ǰ���3���ַ����ӵ���
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
		const char* pos = toLen == 1 ? strchr(findStart, to[0]) : strstr(findStart, to);
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
// �������2�ַ����е�ÿһ���ַ��ڲ���1�ַ������ܹ������˶��ٴΣ�ע��ͳ�Ƶ��ǲ���2��ÿһ���ַ����ڲ���1���ܹ����ֵĴ���
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

// �������2�ַ�����ÿһ���ַ��ֱ��ڲ���1�ַ����г����˶��ٴΣ�����Ϊһ��table
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
// ����Ƿ�����ֵ��������ʽ��ʾ��������ֵ��������ϼ���������򷵻�ת�������ֵ�����򷵻�nil�����2������в���2�Ļ���
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

static int lua_string_hasnumeric(lua_State* L)
{
	double d = 0;
	size_t len = 0;
	char *endp = 0;
	const char* s = 0;
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
			s = (const char*)lua_tolstring(L, 1, &len);
			if (len > 0)
			{
				d = strtod(s, &endp);
				if (endp > s)
					r = 1;
			}
		}
	}

	if (r)
	{
		lua_pushnumber(L, d);
		if (endp)
		{
			lua_pushinteger(L, endp - s + 1);
			return 2;
		}

		return 1;
	}
	if (t >= 2)
	{
		lua_pushvalue(L, 2);
		return 1;
	}

	return 0;
}

// ����Ƿ���������������ʽ��ʾ������ֵ��������ϼ���������򷵻�ת��������������򷵻�nil�����2������в���2�Ļ���
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
				switch(s[0])
				{
				case '0':
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
					break;

				case '-':
					negative = true;
					break;
				
				case '+':
					s ++;
					len --;
					break;
				}

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
#if defined(REEME_64) && defined(DOUBLE_EXCEED_CHEDK)
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
// ����ַ����Ƿ������֣������������ֿ�ͷ�ַ������ַ���������֮������������������ν��������ǣ��������ֱ���number���ͣ��Լ��ַ����з����ֿ�ʼ��λ��
// ����������ֻ򲻺����֣��򷵻�Ϊnil������в���2������ʧ��ʱ���ز���2��
static int lua_string_hasinteger(lua_State* L)
{	
	size_t len = 0;
	long long v = 0;
	char *endp = 0;
	const char* s = 0;
	bool negative = false;
	int r = 0, t = lua_gettop(L), tp = 0;	

	if (t >= 1)
	{
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
				switch(s[0])
				{
				case '0':				
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
					break;
				case '-':
					negative = true;
					break;
				case '+':
					s ++;
					len --;
					break;
				}

				v = strtoll(s, &endp, digits);
				if (endp > s)
					r = 2;
			}
		}
	}

	if (r)
	{
#if defined(REEME_64) && defined(DOUBLE_EXCEED_CHEDK)
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
			if (endp)
			{
				lua_pushinteger(L, endp - s + 1);
				return 2;
			}
			return 1;
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

// ����Ƿ��ǲ���ֵ��������ʽ��ʾ�Ĳ���ֵ��������ϼ���������򷵻�ת����Ĳ���ֵ�����򷵻�nil�����2������в���2�Ļ���
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

// ����Ƿ����ַ�����������ϼ���������򷵻ظ��ַ��������򷵻�nil
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
			// �ַ�����С����󳤶ȷ�Χ
			minl = lua_tointeger(L, n);
			if (lua_isnumber(L, n + 1))
			{
				// �����һ������Ҳ����ֵ�͵Ļ�����ô��Ĭ��Ϊ�뱾����һ�������С����󳤶�����Χ
				maxl = lua_tointeger(L, 3);
				n ++;
			}

			if (utf8)
			{
				// ����UTF8���ַ����ĳ���
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
			// ������ʽ����������ƥ�䣬���û�а���google re2��Ļ����͵���ngx.re.match����ɹ���
#ifdef RE2_RE2_H_
			if (!RE2::FullMatch(s, lua_tostring(L, n)))
			{
				flags = 0;
				break;
			}
#else
			size_t matchedl;

			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_ngx_re_match);
			lua_pushvalue(L, 1);

			const char* re = lua_tolstring(L, n, &matchedl);
			const char* rer = re + matchedl - 1;

			if (re[0] == '/')
			{				
				re ++;
				while(rer > re)
				{
					if (rer[0] == '/')
						break;
					rer --;
				}

				if (rer > re)
				{
					lua_pushlstring(L, re, rer - re);
					lua_pushstring(L, rer + 1);
					lua_pcall(L, 3, 1, 0);
					rer = NULL;
				}
			}

			if (rer)
			{
				lua_pushvalue(L, n);
				lua_pcall(L, 2, 1, 0);
			}
			
			if (lua_istable(L, -1))
			{
				matchedl = 0;
				lua_rawgeti(L, -1, 0);
				if (!lua_tolstring(L, -1, &matchedl) || matchedl != len)
				{
					flags = 0;
					break;
				}
			}
			else
			{
				flags = 0;
				break;
			}
#endif
		}
		else if (t == LUA_TBOOLEAN)
		{
			// �Ƿ�utf8������ַ���
			utf8 = lua_toboolean(L, n);
		}
		else if (t == LUA_TFUNCTION)
		{
			// ʹ�ü�⺯�����м�⣬����true��ʾ���ͨ��
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
// ���õ�ģʽ�У�
// %s���ַ���
// %u���޷�������
// %d���з�������
// %x��ʮ����������
// %f %g��������
// %c������ansi��ת�ַ�
// ֧��������С�����ַ������п���ƣ�С����λ������
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
		const char* foundpos = strchr(src + start, '%');
		if (!foundpos)
			break;

		size_t pos = foundpos - src;
		if (start < pos)
			lua_string_addbuf(&buf, src + start, pos - start);

		start = pos + 1;
		if (start >= srcLen)
			break;

		// ��������%Ϊת��
		foundpos ++;
		char ctl = foundpos[0];
		if (ctl == '%')
		{
			start ++; 
			luaL_addchar(pBuf, '%');
			continue;
		}

		// ��־λ
		flags = 0;
		flag = string_fmt_valid_fmt[ctl];
		while((flag >> 4) > 0)
		{
			flags |= flag;
			foundpos ++;
			ctl = foundpos[0];
			flag = string_fmt_valid_fmt[ctl];
		}

		// ��/���
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

		// С��λ��
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
			luaL_error(L, "string.fmt #%d have no parameter", cc - 1);
			return 0;
		}

		switch(flag = string_fmt_valid_fmt[ctl])
		{
		case 1: // �ַ���
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
		
		case 2: // ������
			val = 0;
			if (tp == LUA_TNUMBER)
			{				
				dv = lua_tonumber(L, cc);
			}
			else if (tp == LUA_TSTRING)
			{
				val = lua_tolstring(L, cc, &valLen);
				dv = strtod(val, &endpos);
				if (std::isnan(dv) || endpos - val != valLen)
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
				// ����С��λ��������digits�����0�����˾Ͳõ�
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

		case 3:	// ansi�뵽�ַ�
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

		case 0:	// ����
			return luaL_error(L, "the %u-ith char '%c' invalid", foundpos - src + 1, ctl);
		
		default: // ����
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

			// λ�������Ȳ�0
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
#define TP_FIXED		16384
#define ERR_START_MAX	128

struct StringIndex {
	int				index;
	const char		*val;
	size_t			leng;

	inline StringIndex() {}
	inline StringIndex(int a, const char* b, size_t c)
		: index(a), val(b), leng(c)
	{
	}
};
typedef MAP_CLASS_NAME<StringPtrKeyL, StringIndex> StringsPckMap;

static const char templReturnCode[] = { "\nreturn __segcc__ > 0 and __segs__ or table.concat(__ret__, '')\nend" };
static const char tenplSetvarCode[] = { "__ret__[#__ret__+1]=" };
static const char templSubtemplCode[] = { "subtemplate(self, __env__, " };
static const char templErrTipCode[] = { "the vffull template parsed result: <br/><br/>\r\n\r\n" };
static const char templSegDecl[] = { "__ret__, __segcc__, __segs__['" };
static const char templSegAdd[] = { "'] = table.new(32, 0), __segcc__ + 1, table.concat(__ret__, '')\n" };
static const char templInitCode[] = { 
	"return function(self, __env__)\n"
	"local __ret__, __segs__, __segcc__, __cachesecs__ = table.new(32, 0), table.new(0, 4), 0, {}\n"
	"local function fetchsection()\n"
	"	return table.remove(__ret__)\n"
	"end\n"
	"local function echo(...)\n"
	"	__ret__[#__ret__+1] = string.merge(...)\n"
	"end\n"
};

class TemplateParser
{
public:
	typedef std::list<const char*> ErorrStarts;

	std::string			buf, errorMsg;
	StringsPckMap		*pck;
	size_t				offset, srcLen, wrote, mlsLength;
	
	uint32_t			expLines;
	char				expQuoted;
	const char			*expSub;

	const char			*src;
	const char			*savedPos;
	ErorrStarts			errorStart;

	char				mlsBegin[32], mlsEnd[32];
	bool				bracketOpened, hasSegments, isSafeMode;

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

	if (strncmp(revFind - 3, " end", 3))
	{
		revFind -= revLeng[r];
		if (r == KEND_THEN && strncmp(revFind, " then", 5))
			return KEND_THEN;
		if (r == KEND_DO && strncmp(revFind, " do", 3))
			return KEND_DO;
	}
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

	const char* findExpEnd(const char* expStart, const char* totalEnd)
	{
		int brackets = 0, bbrackets = 1;
		const char* expEnd = expStart;

		expSub = 0;
		expLines = 0;
		expQuoted = 0;
		errorStart.clear();

		while (expEnd < totalEnd)
		{
			char ch = expEnd[0];
			if (expQuoted)
			{
				if (ch == '\\')
					expEnd ++;
				else if (ch == '\n')
					expLines ++;
				else if (ch == expQuoted)
					expQuoted = 0;
			}
			else
			{
				switch (ch)
				{
				case '\'':
				case '"':
					expQuoted = ch;
					break;

				case '\\':
					expSub = expEnd;
					break;

				case '(':
					errorStart.push_back(expEnd);
					brackets ++;
					break;

				case '{':
					errorStart.push_back(expEnd);
					bbrackets ++;
					break;

				case ')':
					if (brackets == 0 || errorStart.size() == 0 || errorStart.back()[0] != '(')
					{
						errorMsg = "')' error paired";
						return 0;
					}

					errorStart.pop_back();
					brackets --;
					break;

				case '}':
					if (bbrackets == 1)
						goto _end;

					if (errorStart.size() ==  0 || errorStart.back()[0] != '{')
					{
						errorMsg = "'}' error paired";
						return 0;
					}

					errorStart.pop_back();
					bbrackets --;
					break;

				case '\n':
					expLines ++;
					break;
				}
			}

			expEnd ++;
		}

_end:
		if (expEnd >= totalEnd || brackets)
			return 0;

		return expEnd;
	}

	bool more()
	{
		uint8_t ch;
		size_t add;
		StringPtrKeyL key;

		if (offset)
		{
			buf.clear();
			wrote = 0;
		}

		if (offset >= srcLen)
			return false;

		for(const char* foundPos = 0;;)
		{			
			if (savedPos)
				foundPos = savedPos;
			else if (src[offset] == '{')
				foundPos = src + offset;
			else
				foundPos = (const char*)memchr(src + offset, '{', srcLen - offset);
			savedPos = 0;

			if (!foundPos)
				return false;

			size_t pos = foundPos - src;
			if (pos > srcLen - 3)
				break;

			foundPos ++;
			ch = foundPos[0];

			if (ch == '\\' && string_template_ctls[foundPos[1]] == 1)
			{
				// ���Ʒ�ת��
				add = pos - offset + 1;
				if (pos >= offset && append(add) != add)
				{
					savedPos = foundPos;
					return true;
				}

				offset = pos + 2;
				continue;
			}

			if (string_template_ctls[ch] != 1)
			{
				append(foundPos - src - offset);
				goto _lastcheck;
			}

			// �����֮ǰδ����Ĳ���
			add = pos - offset;
			if (pos >= offset && append(add) != add)
			{
				savedPos = foundPos - 1;
				return true;
			}

			// ������ַ������Ļ�ֱ���滻
			if (ch == '#')
			{
				const char* nameBegin = foundPos + 1;
				while((uint8_t)nameBegin[0] <= 32)
					nameBegin ++;

				const char* nameEnd = nameBegin + 1;
				while (nameEnd < src + srcLen)
				{
					if (nameEnd[0] == '}' || (uint8_t)nameEnd[0] <= 32)
						break;
					nameEnd ++;
				}

				if (pck)
				{
					// �ַ�����
					key.pString = nameBegin;
					key.nLength = nameEnd - nameBegin;
					key.nHashID = hashString<size_t>(nameBegin, key.nLength);

					StringsPckMap::iterator ite = pck->find(key);
					if (ite != pck->end())
					{
						StringIndex& stridx = ite->second;
						buf.append(stridx.val, stridx.leng);
					}
				}

				while(nameEnd[0] != '}')
					nameEnd ++;

				wrote = buf.size();

				foundPos = nameEnd + 1;
				offset = foundPos - src;

				goto _lastcheck;
			}

			// �ر���������ݿ��Ʒ������������д���
			close();

			if (ch == '=' || ch == ':')
			{
				// Ѱ�ұ��ʽ�Ľ���
				const char* varBegin = foundPos + 1;
				while((uint8_t)varBegin[0] <= 32)
					varBegin ++;
				
				const char* varEnd = findExpEnd(varBegin, src + srcLen);				
				if (!varEnd)
					return false;

				if (ch == '=')
				{					
					// ʹ��ֵ�������2���Ǹ�@���ţ���ʹ��htmlentities����ת��
					if (varBegin[0] == '@')
					{
						buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
						buf.append("string.htmlentitiesenc(tostring(", 9);
						buf.append(varBegin + 1, varEnd - varBegin - 1);
						buf += ')';
						buf += ')';
					}
					else
					{
						buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
						if (isSafeMode) buf.append("tostring(", 9);
						buf.append(varBegin, varEnd - varBegin);
						if (isSafeMode) buf += ')';
					}
				}
				else
				{
					// ������ģ��
					if (expSub)
					{
						// ���ģ�岻ֱ�����ã����Ǹ���һ��������
						buf.append("local ", 6);
						buf.append(varBegin, expSub - varBegin);
						buf += '=';
						buf.append(templSubtemplCode, sizeof(templSubtemplCode) - 1);
						buf += '\'';

						expSub ++;
						while((uint8_t)expSub[0] <= 32)
							expSub ++;
						buf.append(expSub, varEnd - expSub);
					}
					else
					{
						// ֱ������ģ��
						buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
						buf.append(templSubtemplCode, sizeof(templSubtemplCode) - 1);
						buf += '\'';
						buf.append(varBegin, varEnd - varBegin);
					}

					buf.append("\')", 2);					
				}

				buf += '\n';
				wrote = buf.size();
				
				foundPos = varEnd + 1;
				offset = foundPos - src;

				open(offset);
				goto _lastcheck;
			}
			else if (ch == '%')
			{
				// �ҵ����ʽ�Ľ���λ��
				const char* expStart = foundPos + 1, *expEnd;
				const char* totalEnd = src + srcLen;

				while(expStart[0] <= 32)
					expStart ++;

				expEnd = findExpEnd(expStart, totalEnd);
				if (!expEnd)
					return false;
				
				add = expEnd - expStart;
				if (add >= 14 && memcmp(expStart, "subtemplate", 11) == 0 && sql_where_splits[expStart[11]] == 1)
				{
					// ���⴦��subtemplate
					buf.append(tenplSetvarCode, sizeof(tenplSetvarCode) - 1);
					buf.append(templSubtemplCode, sizeof(templSubtemplCode) - 1);

					expStart += 11;
					while(expStart[0] <= 32)
						expStart ++;

					if (expStart[0] == '(')
						expStart ++;

					buf.append(expStart, expEnd - expStart);

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
					// �����ı��ʽ���ж�һ���Ƿ���ĳЩ�ؼ��֣�����ǵĻ��������ж���û����Ӧ��then do��䡣����ط��Ǻ����ױ����ǵģ�����ɳ������Զ����
					buf.append(expStart, add);

					if (expLines == 0 && add >= 4)
					{
						// ֻ���ڱ��ʽ�����е�ʱ��Żᴦ���Զ���ӣ����{%����ı��ʽ�ж��еĻ����ǲ���ȥ��������Զ���ӵ�
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

				foundPos = expEnd + 1;
				offset = foundPos - src;

				open(offset);
				goto _lastcheck;
			}
			else if (ch == '?')
			{
				// �ж��ǿ�ʼ���ǽ���
				const char* cmdStart = foundPos + 1;
				const char* totalEnd = src + srcLen;

				while(cmdStart[0] <= 32)
					cmdStart ++;

				const char* cmdEnd = findExpEnd(cmdStart, totalEnd);
				if (!cmdEnd)
					return false;

				const char dstbuf[4] = { 'e', 'n', 'd', 0 };
				char chkbuf[8] = { cmdStart[0], cmdStart[1], cmdStart[2] };				

				size_t add = cmdEnd - cmdStart;
				if (cmdStart[0] == '=' && add > 1)
				{
					// ������ֱ�ӽ���һ��ģ���ֵ���ڱ���������������cmdStart->cmdEnd֮����ַ���
					buf += "cachesection(self, false, __ret__, __cachesecs__)\n";
					buf.append(cmdStart + 1, add - 1);
					buf += "=table.remove(__ret__)\nend";
				}
				else if (*(uint32_t*)chkbuf == *(const uint32_t*)dstbuf && (cmdStart[3] == '}' || (uint8_t)cmdStart[3] <= 32))
				{
					// ����
					buf += "cachesection(self, false, __ret__, __cachesecs__)\nend";
				}
				else
				{
					// ��ʼ
					buf += "if cachesection(self, true, __ret__, __cachesecs__";
					if (add)
					{
						buf += ',';
						buf.append(cmdStart, add);
					}

					buf.append(") then", 6);
				}

				buf += '\n';
				wrote = buf.size();

				foundPos = cmdEnd + 1;
				offset = foundPos - src;

				open(offset);
				goto _lastcheck;
			}
			else
			{
				// ������ģ��ֶζ���������Ҫ��4�����ӵ�-�ţ�����4��������
				int bkchk = 1;
				for ( ; ; ++ bkchk)
				{
					ch = foundPos[bkchk];
					if (ch != '-')
						break;
				}

				if (bkchk >= 4)
				{
					const char* segnameStart = foundPos + bkchk;
					while((uint8_t)foundPos[0] <= 32)
						segnameStart ++;

					// �ҵ��ո��}ʱֹͣ�����Ƕ�����
					const char* segnameEnd = segnameStart + 1;
					const char* totalEnd = src + srcLen;

					while ((ch = segnameEnd[0]) != 0 && segnameEnd < totalEnd)
					{
						if (ch == ' ' || ch == '}')
							break;
						if (ch == '\'' || ch == '"')
							return false;
						if (ch == '\n')
						{
							// �����в�������
							segnameEnd = 0;
							break;
						}

						segnameEnd ++;
					}

					if (segnameEnd)
					{
						// ��Ӵ���
						add = pos - offset;
						if (pos >= offset && append(add) != add)
						{
							savedPos = foundPos - 1;
							return true;
						}

						close();

						buf.append(templSegDecl, sizeof(templSegDecl) - 1);
						buf.append(segnameStart, segnameEnd - segnameStart);
						buf.append(templSegAdd, sizeof(templSegAdd) - 1);
						wrote = buf.size();

						while (ch != '}' && segnameEnd < totalEnd)
						{
							segnameEnd ++;
							ch = segnameEnd[0];
						}

						foundPos = segnameEnd + 1;
						offset = foundPos - src;
						hasSegments = true;

						open(offset);
						goto _lastcheck;
					}
				}
			}

			append(foundPos - src - offset);			

_lastcheck:
			if (buf.size() + 400 >= TP_FIXED)
				break;	// ����ֻ��400�ֽڵĿ��У��Ͳ�ʹ����
		}

		return true;
	}

	void open(size_t skipWhitesToNextOpenOffset = -1)
	{
		if (!bracketOpened)
		{			
			if (skipWhitesToNextOpenOffset != -1)
			{
				// �����skipWhitesToNextOpenOffset��ʼ����һ��������֮�����ȫ�����ǿհ��ַ���ֻ��1�����еĻ����Ǿ�ȫ�����Ե�
				int endl = 0;
				const char* chk = src + skipWhitesToNextOpenOffset;
				while((uint8_t)chk[0] <= 32 && chk < src + srcLen)
				{
					if (chk[0] == '\n')
						endl ++;
					chk ++;
				}

				if (endl <= 1 && chk[0] == '{')
				{
					offset = chk - src;
					return ;
				}
			}

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
	parser.src = src;
	parser.srcLen = srcLen;
	parser.offset = 0;
	parser.pck = 0;
	parser.hasSegments = false;
	parser.bracketOpened = false;
	parser.isSafeMode = false;
	parser.wrote = sizeof(templInitCode) - 1;

	parser.buf.reserve(TP_FIXED);
	parser.buf.append(templInitCode, parser.wrote);
}
static bool _replaceErrLine(luaL_Buffer* buf, const char* err, size_t len)
{
	if (strncmp(err, "[string ", 8))
		return false;

	const char* linepos = strstr(err, "]:");
	if (!linepos)
		return false;

	char* pszContinue, lineBuf[32];
	lua_string_addbuf(buf, err, linepos - err + 2);
	long line = strtol(linepos + 2, &pszContinue, 10);

	size_t lineLeng = sprintf(lineBuf, "%u", line + 2);
	lua_string_addbuf(buf, lineBuf, lineLeng);

	lua_string_addbuf(buf, pszContinue, len - (pszContinue - err));

	return true;
}
static int lua_string_parsetemplate(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	size_t srcLen = 0;	
	TemplateParser parser;
	const char* src = luaL_checklstring(L, 2, &srcLen);
	const char* chunkName = "__templ_tempr__";	

	int envType = lua_type(L, 3);

	_init_TemplateParser(parser, src, srcLen);
	if (lua_isstring(L, 4))
		chunkName = lua_tostring(L, 4);
	
	lua_pushlstring(L, "__safemode", 10);
	lua_rawget(L, 1);
	if (lua_isboolean(L, -1) && lua_toboolean(L, -1))
		parser.isSafeMode = true;

	// ȷ��Ҫ�õĶ����ַ�����ʾ��û���ڴ����б�ʹ�ù���������õ��ˣ����������=�ŵ�����
	memset(parser.mlsBegin, 0, sizeof(parser.mlsBegin) * 2);

	strcpy(parser.mlsBegin, "[==[");
	strcpy(parser.mlsEnd, "]==]");
	parser.mlsLength = 4;

	for(int i = 0; ; ++ i)
	{
		if (!strstr(src, parser.mlsEnd))
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

	if (envType >= LUA_TNIL)
	{
		// env[0]������һ���ַ�����
		if (envType == LUA_TTABLE)
		{
			int pops = 1;
			lua_rawgeti(L, 3, 0);
			if (lua_istable(L, -1))
			{
				lua_rawgeti(L, -1, 0);
				parser.pck = (StringsPckMap*)lua_touserdata(L, -1);
				pops = 2;
			}
			lua_pop(L, pops);
		}

		// ���������Ĵ���
		int r = lua_load(L, &lua_tpl_loader, &parser, chunkName);
		if (parser.errorMsg.length())
		{
			assert(parser.errorStart.size() > 0);

			std::string& msg = parser.errorMsg;
			msg += " start at : ";
		
			const char* errs = parser.errorStart.back();
			size_t left = std::min(srcLen - (errs - parser.src), (size_t)60);
			msg.append(errs, left);

			lua_pushlstring(L, "", 0);
			lua_pushlstring(L, msg.c_str(), msg.length());
		}
		else if (r)
		{
			// ������������Ϣ�������
_run_error:			
			luaL_Buffer buf;
			TemplateParser fullcode;
			size_t len = 0, templateLen = 0;
			const char* templateCode = NULL, *err = NULL;

			luaL_buffinit(L, &buf);
			_init_TemplateParser(fullcode, src, srcLen);

			strcpy(fullcode.mlsBegin, parser.mlsBegin);
			strcpy(fullcode.mlsEnd, parser.mlsEnd);
			fullcode.mlsLength = parser.mlsLength;

			if (envType == LUA_TTABLE)
			{
				// ��������ģ�屨���Ĵ���
				lua_pushliteral(L, "__sub_err_msg");
				lua_rawget(L, 3);
				err = lua_tolstring(L, -1, &len);
				if (err)
				{
					lua_pushliteral(L, "__sub_err_fullcode");
					lua_rawget(L, 3);
					templateCode = lua_tolstring(L, -1, &templateLen);
				}
				else
				{
					lua_pop(L, 1);
				}
			}

			if (!err)
				err = lua_tolstring(L, -1, &len);
			
			if (len)
			{
				if (!_replaceErrLine(&buf, err, len))
					lua_string_addbuf(&buf, err, len);

				lua_string_addbuf(&buf, ", ", 2);
			}

			lua_string_addbuf(&buf, templErrTipCode, sizeof(templErrTipCode) - 1);

			if (templateCode)
			{ 
				lua_string_addbuf(&buf, templateCode, templateLen);
			}
			else
			{
				for (;;)
				{
					const char* ss = lua_tpl_loader(L, &fullcode, &len);
					if (!ss || len == 0)
						break;

					lua_string_addbuf(&buf, ss, len);
				}
			}

			luaL_pushresult(&buf);

			lua_pushlstring(L, "", 0);			
			lua_pushvalue(L, -2);
		}
		else if (lua_isfunction(L, -1))
		{
			r = lua_pcall(L, 0, 1, 0);
			if (r)
				goto _run_error;

			assert(lua_isfunction(L, -1));
			if (envType == LUA_TTABLE)
			{
				int top = lua_gettop(L);

				lua_pushvalue(L, 3);
				lua_setfenv(L, top);

				lua_pushvalue(L, 1);
				lua_pushvalue(L, 3);
				if (lua_pcall(L, 2, 1, 0))
					goto _run_error;

				lua_pushnil(L);
			}
			else
			{
				lua_pushboolean(L, parser.hasSegments ? 1 : 0);
			}
		}
		else
		{
			assert(0 && "not possible");
		}

		return 2;
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
static int stringLangPckGC(lua_State* L)
{
	StringsPckMap* n = (StringsPckMap*)lua_touserdata(L, 1);
	if (n)
		n->~StringsPckMap();
	return 0;
}
static int stringLangRecurTable(lua_State* L, StringsPckMap* pck, int src, int tbl, int index, std::string& strPrefix)
{
	StringPtrKeyL key;
	StringIndex stridx;
	std::string strKeyname;

	lua_pushnil(L);

	const char* keys;
	int top = lua_gettop(L);
	size_t keylen, leng = strPrefix.length();

	while(lua_next(L, src))
	{
		if (lua_istable(L, -1))
		{			
			keys = lua_tolstring(L, -2, &keylen);

			if (leng)
				strPrefix += '.';
			strPrefix.append(keys, keylen);

			index = stringLangRecurTable(L, pck, top + 1, tbl, index, strPrefix);
			strPrefix.erase(leng);
		}
		else
		{
			lua_rawseti(L, tbl, -index);

			if (strPrefix.length())
			{
				keys = lua_tolstring(L, -1, &keylen);

				strKeyname = strPrefix;
				strKeyname += '.';
				strKeyname.append(keys, keylen);
				lua_pushlstring(L, strKeyname.c_str(), strKeyname.length());
			}
			else
			{
				lua_pushvalue(L, -1);
			}

			lua_rawseti(L, tbl, index);
			lua_rawgeti(L, tbl, index);

			key.pString = lua_tolstring(L, -1, &key.nLength);
			key.nHashID = hashString<size_t>(key.pString, key.nLength);

			lua_rawgeti(L, tbl, -index);

			stridx.index = -index;
			stridx.val = lua_tolstring(L, -1, &stridx.leng);

			pck->insert(StringsPckMap::value_type(key, stridx));
			index ++;
		}

		lua_settop(L, top);
	}

	return index;
}
static int stringLangFind(lua_State* L)
{
	StringPtrKeyL key;
	key.pString = luaL_checklstring(L, 2, &key.nLength);
	key.nHashID = hashString<size_t>(key.pString, key.nLength);

	lua_rawgeti(L, 1, 0);
	StringsPckMap* pck = (StringsPckMap*)lua_touserdata(L, -1);

	StringsPckMap::iterator ite = pck->find(key);
	if (ite == pck->end())
		return 0;

	lua_rawgeti(L, 1, ite->second.index);
	return 1;
}
static int lua_string_makestrpackage(lua_State* L)
{
	StringPtrKeyL key;
	StringIndex stridx;
	std::string strPrefix;
	StringsPckMap* pck = 0;
	int top = lua_gettop(L), tbl, index = 2;

	if (top < 2)
		return luaL_error(L, "string.makestrpackage must be called with at least two parameter");

	tbl = lua_type(L, 1);
	if (tbl == LUA_TTABLE)
	{
		tbl = 1;
		lua_rawgeti(L, 1, 1);
		index = lua_tointeger(L, -1);
		if (index >= 2)
		{
			lua_rawgeti(L, 1, 0);
			pck = (StringsPckMap*)lua_touserdata(L, -1);
		}
		else
			index = 2;

		lua_pushvalue(L, 1);
	}
	else if (tbl != LUA_TNIL)
		return luaL_error(L, "string.makestrpackage #1 not a nil or table", 0);
	else
		tbl = 0;

	if (!pck)
	{
		if (!tbl)
		{
			tbl = top + 1;
			lua_newtable(L);
		}

		pck = (StringsPckMap*)lua_newuserdata(L, sizeof(StringsPckMap));
		new (pck) StringsPckMap();

		if (luaL_newmetatable(L, "_LANG_PACK_FOR_PRASETEMPLATE"))
		{
			// ���meta���������ͷ�map
			lua_pushliteral(L, "__gc");
			lua_pushcfunction(L, &stringLangPckGC);
			lua_rawset(L, -3);
		}
		lua_setmetatable(L, -2);

		// ��map���ڷ���table��0��λ��
		lua_rawseti(L, tbl, 0);

		if (luaL_newmetatable(L, "_LANG_PACK_SIM_CALL"))
		{
			// ���meta���������÷��ص�table��������һ�������Բ���key��Ӧ��ֵ
			lua_pushliteral(L, "__call");
			lua_pushcfunction(L, &stringLangFind);
			lua_rawset(L, -3);
		}
		lua_setmetatable(L, tbl);
	}

	// ����ÿһ��������ֵ	
	for (int n = 2; n <= top; ++ n)
	{
		int t = lua_type(L, n);
		if (t == LUA_TSTRING)
		{
			if (!lua_isstring(L, n + 1))
				return luaL_error(L, "string.makestrpackage #%d expect a string got not string", n + 1);			

			lua_pushvalue(L, n);
			lua_rawseti(L, tbl, index);

			lua_pushvalue(L, n + 1);
			lua_rawseti(L, tbl, -index);

			lua_rawgeti(L, tbl, index);

			key.pString = lua_tolstring(L, -1, &key.nLength);
			key.nHashID = hashString<size_t>(key.pString, key.nLength);

			lua_rawgeti(L, tbl, -index);
			
			stridx.index = -index;
			stridx.val = lua_tolstring(L, -1, &stridx.leng);

			pck->insert(StringsPckMap::value_type(key, stridx));
			index ++;

			lua_pop(L, 2);
		}
		else if (t == LUA_TTABLE)
		{
			strPrefix.clear();
			index = stringLangRecurTable(L, pck, n, tbl, index, strPrefix);
		}
		else
		{
			return luaL_error(L, "string.makestrpackage #%d not a string or table", n);
		}
	}

	// ��ǰ������ֵ����1��λ�á�����key=>value��ֵ�Ǵ�2��ʼ�洢��
	lua_pushinteger(L, index);
	lua_rawseti(L, tbl, 1);

	return 1;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_htmlentitiesenc(lua_State* L)
{
	char ch;	
	size_t i, prevpos = 0;
	luaL_Buffer buf, *pBuf = &buf;
	size_t len = 0, skips = 0, addlen;
	const char* s = luaL_optlstring(L, 1, NULL, &len), *src;
	uint32_t flags = luaL_optinteger(L, 2, 0);

	if (!s || len == 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	uint8_t v, pos = 0;
	luaL_buffinit(L, &buf);

	for(i = 0; i < len; i += skips)
	{
		v = s[i];
		if (v & 0x80)
		{
			if ((v & 0xE0) == 0xC0)
			{
				skips = 2;
				pos = string_htmlent_ctls[((uint32_t)v << 8) | (uint8_t)s[i + 1]];
			}
			else if ((v & 0xF0) == 0xE0)
				skips = 3;
			else if ((v & 0xF8) == 0xF0)
				skips = 4;
			else if ((v & 0xFC) == 0xF8)
				skips = 5;
			else if ((v & 0xFE) == 0xFC)
				skips = 6;
			else
				return luaL_error(L, "Unknown code '0x%02x' in string.htmlentitiesenc", v);
		}
		else
		{
			skips = 1;
			pos = string_htmlent_ctls[v];
		}

		if (pos)
		{
			addlen = i - prevpos;
			if (addlen)
				lua_string_addbuf(pBuf, s + prevpos, addlen);
			prevpos = i + skips;

			if (pos <= 5)
			{
				src = string_htmlent_strs[pos - 1];
				while((ch = *src ++) != 0)
					luaL_addchar(pBuf, ch);

				pos = 0;
				continue;
			}
			else if (!(flags & kHtmlEntReservedOnly))
			{
				src = string_htmlent_strs[pos - 1];
				while((ch = *src ++) != 0)
					luaL_addchar(pBuf, ch);

				pos = 0;
				continue;
			}			
		}
	}

	lua_string_addbuf(pBuf, s + prevpos, i - prevpos);
	luaL_pushresult(pBuf);
	return 1;
}

static int lua_string_htmlentitiesdec(lua_State* L)
{
	StringPtrKey key;
	char tmpBuf[12], ch;
	bool foundEnd = false;
	luaL_Buffer buf, *pBuf = &buf;
	size_t i, len, bufLen, v, prevpos = 0;
	HtmlEntStringsMap::iterator ite, iEnd = gHtmlEntStrings.end();
	const char* s = luaL_checklstring(L, 1, &len);

	if (!s || len == 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	luaL_buffinit(L, pBuf);
	for(i = 0; i < len; ++ i)
	{
		ch = s[i];
		if (ch == '&')
		{
		_refind:
			tmpBuf[0] = '&';
			foundEnd = false;

			for(bufLen = 1; bufLen < 12 && i + bufLen < len; )
			{
				ch = s[i + bufLen];
				tmpBuf[bufLen ++] = ch;
				if (ch == ';')
				{
					foundEnd = true;
					break;
				}
				if (ch == '&')
				{
					i += bufLen;
					goto _refind;
				}
			}

			if (foundEnd)
			{
				tmpBuf[bufLen] = 0;
				key.pString = tmpBuf;
				key.nHashID = hashString<size_t>(tmpBuf, bufLen);

				ite = gHtmlEntStrings.find(key);
				if (ite != iEnd)
				{
					size_t addlen = i - prevpos;
					if (addlen)
						lua_string_addbuf(pBuf, s + prevpos, addlen);
					prevpos = i + bufLen;
					i = prevpos - 1;

					v = ite->second;
					switch (v)
					{
					case 0: luaL_addchar(pBuf, '"'); break;
					case 1: luaL_addchar(pBuf, '\''); break;
					case 2: luaL_addchar(pBuf, '&'); break;
					case 3: luaL_addchar(pBuf, '<'); break;
					case 4: luaL_addchar(pBuf, '>'); break;
					case 5: luaL_addchar(pBuf, ' '); break;

					default:
						v = (v - 5) * 2;
						luaL_addchar(pBuf, string_htmlent_dblcodes[v]);
						luaL_addchar(pBuf, string_htmlent_dblcodes[v + 1]);
						break;
					}
				}
			}
		}
	}

	lua_string_addbuf(pBuf, s + prevpos, i - prevpos);
	luaL_pushresult(pBuf);
	return 1;
}

static int lua_string_addslashes(lua_State* L)
{
	char ch;	
	size_t i, prevpos = 0;
	luaL_Buffer buf, *pBuf = &buf;
	size_t len = 0, skips = 0, addlen;
	const char* s = luaL_optlstring(L, 1, NULL, &len), *src;
	int nsq = lua_toboolean(L, 2), ndq = lua_toboolean(L, 3);

	if (!s || len == 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	uint8_t v, pos = 0;
	luaL_buffinit(L, &buf);

	for(i = 0; i < len; ++ i)
	{
		ch = s[i];
		if ((ch == '\'' && !nsq) || (ch == '"' && !ndq) || ch == '\\')
		{
			addlen = i - prevpos;
			if (addlen)
				lua_string_addbuf(pBuf, s + prevpos, addlen);
			prevpos = i + 1;

			luaL_addchar(pBuf, '\\');
			luaL_addchar(pBuf, ch);
		}
	}

	lua_string_addbuf(pBuf, s + prevpos, i - prevpos);
	luaL_pushresult(pBuf);
	return 1;
}

static int lua_string_stripslashes(lua_State* L)
{
	char ch;	
	size_t i, prevpos = 0;
	luaL_Buffer buf, *pBuf = &buf;
	size_t len = 0, skips = 0, addlen;
	const char* s = luaL_optlstring(L, 1, NULL, &len), *src;
	int nsq = lua_toboolean(L, 2), ndq = lua_toboolean(L, 3);

	if (!s || len == 0)
	{
		lua_pushlstring(L, "", 0);
		return 1;
	}

	uint8_t v, pos = 0;
	luaL_buffinit(L, &buf);

	for(i = 0; i < len; ++ i)
	{
		ch = s[i];
		if (ch = '\\')
		{
			ch = s[i + 1];
			if ((ch == '\'' && !nsq) || (ch == '"' && !ndq) || ch == '\\')
			{
				addlen = i - prevpos;
				if (addlen)
					lua_string_addbuf(pBuf, s + prevpos, addlen);
				prevpos = i + 2;
				i = prevpos;

				luaL_addchar(pBuf, ch);
			}
		}
	}

	lua_string_addbuf(pBuf, s + prevpos, i - prevpos);
	luaL_pushresult(pBuf);
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
typedef TMemNode JsonMemNode;

class JsonMemList : public TMemList
{
public:
	lua_State		*L;

	void escapeJsonString(const char* src, size_t len, uint32_t flags)
	{		
		uint8_t ch, v;
		uint32_t unicode;
		size_t i = 0, spos = 0;
		const char upperChars[] = { "0123456789ABCDEF" };

		while (i < len)
		{
			uint8_t ch = src[i];
			if (ch == 0)
			{
				// Ϊʲô�����NULL�ַ���????
				if (i > spos)
					addString(src + spos, i - spos);
				spos = ++ i;
				continue;
			}

			if (!(flags & kJsonSimpleEscape))
				v = json_escape_chars[ch];
			else if (ch == '\\' || ch == '"')
				v = 1;
			else
				v = 0;

			if (v == 0)
			{
				// defered
				++ i;
				continue;
			}

			if (v == 1)
			{
				// escape some chars
				if (i > spos)
					addString(src + spos, i - spos);
				addChar2('\\', ch);
				spos = ++ i;
			}
			else if (v == 2)
			{
				if (flags & kJsonUnicodes)
				{
					// defered
					if ((ch & 0xE0) == 0xC0)		//2 bit count
						i += 2;
					else if ((ch & 0xF0) == 0xE0)	//3 bit count
						i += 3;
					else if ((ch & 0xF8) == 0xF0)	//4 bit count
						i += 4;
					else
						i ++;

					continue;
				}

				if (i > spos)
					addString(src + spos, i - spos);

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
					unicode = '?';
					i ++;
					assert(0);
				}

				char* utf8dst = reserve(6);
				utf8dst[0] = '\\';
				utf8dst[1] = 'u';

				utf8dst[2] = upperChars[(unicode >> 12) & 0xF];
				utf8dst[3] = upperChars[(unicode >> 8) & 0xF];
				utf8dst[4] = upperChars[(unicode >> 4) & 0xF];
				utf8dst[5] = upperChars[unicode & 0xF];

				spos = i;
			}
			else
			{
				// invisible(s) to visibled
				if (i > spos)
					addString(src + spos, i - spos);
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
};

#define JSON_MAX_DEEP_LEVELS	1024
#define jsonConvValue()\
	switch(vtype) {\
	case LUA_TTABLE:\
		if (recursionJsonEncode(L, mem, base + 1, flags, funcsIdx) == -1)\
			return -1;\
		break;\
	case LUA_TNUMBER:\
		v = lua_tonumber(L, -1);\
		ival = static_cast<int64_t>(v);\
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
			mem->escapeJsonString(ptr, len, flags);\
			mem->addChar('"');\
		}\
		break;\
	case LUA_TCDATA:\
		ival = *(const int64_t*)lua_topointer(L, -1);\
		len = opt_i64toa(ival, buf);\
		mem->addString(buf, len);\
		break;\
	case LUA_TBOOLEAN:\
		if (lua_toboolean(L, -1))\
			mem->addString("true", 4);\
		else\
			mem->addString("false", 5);\
		break;\
	case LUA_TLIGHTUSERDATA:\
	case LUA_TUSERDATA:\
		if (!(flags & kJsonDropNull) && lua_touserdata(L, -1) == NULL)\
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

	if (funcsIdx[3] ++ >= JSON_MAX_DEEP_LEVELS)
		return -1;

	size_t arr = lua_objlen(L, tblIdx), cc = 0;
	if (arr == 0)
	{
		int base = lua_gettop(L) + 1;

		lua_pushnil(L);
		while(lua_next(L, tblIdx))
		{
			int vtype = lua_type(L, -1);
			if (vtype < LUA_TFUNCTION)
			{
				ptr = lua_tolstring(L, -2, &len);

				mem->addChar2(cc ? ',' : '{', '"');
				mem->addString(ptr, len);
				mem->addChar2('"', ':');

				jsonConvValue();
				cc ++;
			}

			lua_settop(L, base);
		}

		if (cc)
			mem->addChar('}');
		else if (flags & kJsonEmptyToObject)
			mem->addChar2('{', ']');
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

			int vtype = lua_type(L, -1);
			if (vtype < LUA_TFUNCTION)
			{
				if (n > 1)
					mem->addChar(',');

				jsonConvValue();
				cc ++;
			}

			lua_settop(L, base);
		}

		mem->addChar(']');
	}

	funcsIdx[3] --;

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
		size_t used = n->used;
		memcpy(ptr, (char*)(n + 1), n->used);
		if (ptr != dst)
			free(n);	// ��һ������Ҫ�ͷţ���Ϊ������malloc�����
		ptr += used;
	}

	lua_pushlstring(L, dst, total);
	return 1;
}

// ����1��JSON�ַ���������ֵ��������һ���ǽ������Table������һ�����õ����ַ����ĳ��ȡ������2������ֵΪnil���ʾ����JSON��ʱ�������
// ����2�����ڵ�{}��[]Ϊ�յ�ʱ�򣬱�ʶ������һ��Object����һ��Array
// ����õ���cdata���ͣ�����boxed 64bit integer������ô�����ڵ���֮ǰrequire('ffi')
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
		lua_Integer flags = 0;
		int needSetMarker = 0;
		
		if (tp == LUA_TSTRING)
		{
			str = luaL_checklstring(L, 1, &len);
		}
		else
		{
			// ʹ��sizeof�������ַ�������
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
			flags = luaL_optinteger(L, 3, 0);
			if (flags & kJsonNoCopy)
				copy = false;
		}

		lua_newtable(L);
		top = lua_gettop(L);

		JSONFile f(L, flags & kJsonDropNull);
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
		char fixedBuf[4096];

		int32_t funcs[4] = { 0 };
		uint32_t flags = 0;

		memList.L = L;
		memList.wrapNode(fixedBuf, sizeof(fixedBuf));

		if (top >= 2)
			flags = luaL_optinteger(L, 2, 0);

		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);

		funcs[1] = lua_gettop(L);
		funcs[0] = funcs[1] - 1;

		if (recursionJsonEncode(L, &memList, 1, flags, funcs) != -1)
		{
			// ������û���κ����⣬���Ƿ��ؽ�����������2�ı�־λ�к���kJsonRetCData����ô������һ��uint8_t�͵�cdata�ڴ�飬���򽫷���һ��lua string
			int r = 0;
			JsonMemNode* n = memList.first();
			if (memList.size() == 1)
			{
				r = pushJsonString(L, (char*)(n + 1), n->used, flags & kJsonRetCData, funcs);
				memList.clear();
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
		else if (memList.size() >= 1)
		{
			memList.popFirst();
		}
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
static int lua_string_merge(lua_State* L)
{
	int i, n = lua_gettop(L);
	if (n < 1)
	{
		lua_pushlstring(L, "", 0);
		return 0;
	}

	char fmtbuf[64];
	luaL_Buffer buf;

	luaL_buffinit(L, &buf);

	for (i = 1; i <= n; i ++)
	{
		size_t len = 0;
		const char *s = 0;

		switch(lua_type(L, i))
		{
		case LUA_TNUMBER:
			s = lua_tolstring(L, i, &len);
			if (n == 1)
			{
				lua_pushlstring(L, s, len);
				return 1;
			}
			break;

		case LUA_TSTRING:
			if (n == 1)
			{
				lua_pushvalue(L, 1);
				return 1;
			}

			s = lua_tolstring(L, i, &len);
			break;

		case LUA_TBOOLEAN:
			if (lua_toboolean(L, i))
			{
				s = "true";
				len = 4;
			}
			else
			{
				s = "false";
				len = 5;
			}

			if (n == 1)
			{
				lua_pushlstring(L, s, len);
				return 1;
			}
			break;

		case LUA_TTABLE:
			luaL_prepbuffer(&buf);

			lua_pushcfunction(L, &lua_string_json);
			lua_pushvalue(L, i);
			lua_pushinteger(L, kJsonUnicodes);
			lua_pcall(L, 2, 1, 0);

			if (!lua_isstring(L, -1))
				return luaL_error(L, "string.merge #%d type is table cannot encode to a json string or the table has a self-loop", i);

			buf.lvl ++;
			break;

		case LUA_TCDATA:
			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFISizeof);
			lua_pushvalue(L, i);
			lua_pcall(L, 1, 1, 0);
			len = lua_tointeger(L, -1);
			lua_pop(L, 1);

			s = (const char*)lua_topointer(L, i);
			break;

		case LUA_TLIGHTUSERDATA:
		case LUA_TUSERDATA:
			s = (const char*)lua_touserdata(L, i);
			if (s == NULL)
			{
				s = "(null)";
				len = 6;
			}
			else
			{
#if REEME_64
				len = sprintf(fmtbuf, "0x%llx", s);
#else
				len = sprintf(fmtbuf, "0x%x", s);
#endif
				s = fmtbuf;
			}

			if (n == 1)
			{
				lua_pushlstring(L, s, len);
				return 1;
			}
			break;

		case LUA_TNIL:
			s = "nil";
			len = 3;

			if (n == 1)
			{
				lua_pushlstring(L, s, len);
				return 1;
			}
			break;

		default:
			lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_tostring);
			lua_pushvalue(L, n);
			lua_pcall(L, 1, 1, 0);
			luaL_addvalue(&buf);
			len = 0;
			break;
		}

		if (len > 0)
			lua_string_addbuf(&buf, s, len);
	}

	luaL_pushresult(&buf);
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static inline bool _isTagEnd(const uint8_t* code, const uint8_t* codeEnd, const uint8_t** outPtr)
{
	if (code[0] == '>')
	{
		*outPtr = code + 1;
		return true;
	}
	if (code[0] == '/')
	{
		code ++;
		while (code < codeEnd)
		{
			if (code[0] == '>')
			{
				*outPtr = code + 1;
				return true;
			}
			else if (code[0] > 32)
				break;
			code ++;
		}
	}
	return false;
}
static inline bool _isCommentEnd(const uint8_t* code, const uint8_t* codeEnd, const uint8_t** outPtr)
{
	if (code[0] == '-' && code[1] == '-' && code[2] == '>')
	{
		*outPtr = code + 4;
		return true;
	}
	return false;
}

static bool _findEnd(const uint8_t* code, const uint8_t* codeEnd, const uint8_t** outPtr)
{
	uint8_t quoteIn = 0;
	while (code < codeEnd)
	{
		if (quoteIn)
		{
			if (code[0] == quoteIn)
				quoteIn = 0;
			else if (code[0] == '\\')
				code ++;
		}
		else if (code[0] == '\'' || code[0] == '"')
		{
			quoteIn = code[0];
		}
		else if (_isTagEnd(code, codeEnd, outPtr))
		{

			return true;
		}

		code ++;
	}

	return false;
}
static bool _findCommentEnd(const uint8_t* code, const uint8_t* codeEnd, const uint8_t** outPtr)
{
	uint8_t quoteIn = 0;
	while (code < codeEnd)
	{
		if (quoteIn)
		{
			if (code[0] == quoteIn)
				quoteIn = 0;
			else if (code[0] == '\\')
				code ++;
		}
		else if (code[0] == '\'' || code[0] == '"')
		{
			quoteIn = code[0];
		}
		else if (_isCommentEnd(code, codeEnd, outPtr))
		{
			return true;
		}

		code ++;
	}

	return false;
}

static int lua_string_removemarkups(lua_State* L)
{
	size_t len = 0;
	std::string strResult;
	const uint8_t* code = (const uint8_t*)luaL_checklstring(L, 1, &len);
	const uint8_t* codeEnd = code + len, *testPtr;
	uint8_t flag = 0;

	strResult.reserve(len / 2);
	while (code < codeEnd)
	{
		if (code[0] == '<')
		{
			testPtr = code + 1;
			if (testPtr[0] == '!' && testPtr[1] == '-' && testPtr[2] == '-')
			{
				// ע��
				if (_findCommentEnd(testPtr + 3, codeEnd, &code))
					continue;
			}
			else
			{
				while (testPtr < codeEnd)
				{
					flag = json_value_char_tbl[testPtr[0]];
					if (flag) break;
					testPtr ++;
				}

				if (testPtr[0] == '/' || (flag >= 4 && flag <= 7))
				{
					// ��ǩ��ʼ������������
					testPtr ++;
					while (testPtr < codeEnd)
					{
						flag = json_value_char_tbl[testPtr[0]];
						if (flag < 4 || flag > 7)
							break;
						testPtr ++;
					}

					if (flag == 0)
					{
						// �ҵ�����λ��						
						if (_findEnd(testPtr, codeEnd, &code))
							continue;
					}
					else if (_isTagEnd(testPtr, codeEnd, &code))
					{
						// �����ǩ������
						continue;
					}
				}
			}
		}

		strResult += code[0];
		code ++;
	}

	lua_pushlstring(L, strResult.c_str(), strResult.length());
	return 1;
}

//////////////////////////////////////////////////////////////////////////
static void luaext_string(lua_State *L)
{
	const luaL_Reg procs[] = {
		// �ַ����з�
		{ "split", &lua_string_split },
		// ID�����з�
		{ "idarray", &lua_string_idarray },
		// trim����
		{ "trim", &lua_string_trim },

		// �����ַ�������ң���֧���ַ�������
		{ "rfindchar", &lua_string_rfindchar },
		// ���ַ������������ַ����ִ������ܼ���
		{ "countchars", &lua_string_countchars },
		// ���ַ�������ÿһ���ַ����ִ����ķֱ����
		{ "counteachchars", &lua_string_counteachchars },

		// �ַ�����ģʽ�����滻��֧�ֵ��Ե�����������飬bm�ַ��������ͨ�ַ��������
		{ "replace", &lua_string_replace },
		// �ַ���ָ��λ��+����λ���滻
		{ "subreplaceto", &lua_string_subreplaceto },
		// �ַ���ָ��λ��+�����滻
		{ "subreplace", &lua_string_subreplace },
		// �ַ������Ҵ���ȡ
		{ "subto", &lua_string_subto },

		// ��ֵ+�������ַ������
		{ "checknumeric", &lua_string_checknumeric },
		{ "hasnumeric", &lua_string_hasnumeric },
		// �����ַ�����⣬֧��boxed int64
		{ "checkinteger", &lua_string_checkinteger },
		{ "checkinteger32", &lua_string_checkinteger32 },
		{ "hasinteger", &lua_string_hasinteger },
		// �����ͼ�⣨������ֵ���ַ�����boolean���ͣ�
		{ "checkboolean", &lua_string_checkboolean },
		// �ַ�����⣬���Լ����С��󳤶ȡ�ʹ������(google re2)ƥ�䡢ʹ�ú�������
		{ "checkstring", &lua_string_checkstring },

		// �ַ�����ʽ�����󲿷�ʱ���string.format����ֱ���滻ʹ�ã�ִ���������ۺϱ�string.format��΢����һ�㣬���ؼ���֧��boxed int64���ͣ���ʽ��Ϊ�ַ��������־���(string.format��֧��)
		{ "fmt", &lua_string_fmt },

		// ģ�������ʹ��{% sentense } {= value } {: sub_template }�����﷨���ȼ򵥵�Lua���template�и�ǿ���ݴ����Ӧ������������дthen/do��ʱ������Զ����ϣ�ǰ���Ǳ���д����{% while true do if xxx }�ͻ��޷�������
		{ "parseTemplate", &lua_string_parsetemplate },
		// Ϊģ��������������ַ�����
		{ "makestrpackage", &lua_string_makestrpackage },

		// HTMLʵ��ת��
		{ "htmlentitiesenc", &lua_string_htmlentitiesenc },
		{ "htmlentitiesdec", &lua_string_htmlentitiesdec },

		// Ϊ��˫������ӷ�б��/ȥ����б��
		{ "addslashes", &lua_string_addslashes },
		{ "stripslashes", &lua_string_stripslashes },

		// ����Boyer-Moore���ַ������ڲ���
		{ "bmcompile", &lua_string_bmcompile },
		// �ñ���õ�BM�ַ������в��ң��ʺ���һ�α��룬Ȼ���ڴ������ı��п��ٵĲ���һ���Ӵ����Ӵ���Դ�ַ���Խ������Խ�ţ�
		{ "bmfind", &lua_string_bmfind },

		// Json���루decʱ2~4��������ngx�����õ�cjson��encʱ��table�ĸ��Ӷ�1.5~4.5��������cjson�������ⲻ�ǹؼ����ؼ��Ǳ�json encode֧��boxed 64bit integer�Լ�cdata�Զ�����Ϊbase64�ȱ�����е���չ������
		{ "json", &lua_string_json },

		// �����������ϳ��ַ���
		{ "merge", &lua_string_merge },

		// �Ƴ�MarkupLanguageʽ�ı�ǩ����
		{ "removemarkups", &lua_string_removemarkups },

		{ NULL, NULL }
	};


	lua_getglobal(L, "string");

	// �ַ����з�ʱ���õı�־λ
	lua_pushliteral(L, "SPLIT_ASKEY");		// ���г�����ֵ��Ϊkey
	lua_pushinteger(L, kSplitAsKey);
	lua_rawset(L, -3);

	lua_pushliteral(L, "SPLIT_TRIM");		// ÿһ���г�����ֵ��������trim
	lua_pushinteger(L, kSplitTrim);
	lua_rawset(L, -3);

	// JSON����ʱ���õı�־λ
	lua_pushliteral(L, "JSON_NOCOPY");				// ����ʱֱ����ԭ�ַ����ϲ�����ԭ�ַ����ڴ潫�⵽�ƻ��������֮�󽫲�������ʹ�õĻ����ƻ�Ҳû��ϵ�ˣ��ֿ�����һ��copy��
	lua_pushinteger(L, kJsonNoCopy);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_RETCDATA");			// ����ʱ����uint8_t�͵�cdata���ݶ�����string
	lua_pushinteger(L, kJsonRetCData);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_LUASTRING");			// �����ַ���ʱֱ��ʹ�ñ���֧�ֵ���չ [[ ]] ����ʾ����ʹ�ñ�׼��json�ַ�����ʾ�������Ҳ��û��escape��ת��utf8/unicode�Ĺ���
	lua_pushinteger(L, kJsonLuaString);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_UNICODES");			// �ַ����е�utf8/unicode��Ҫת�壬ֱ�ӱ���ʹ�á���һ����־JSON_LUASTRING����ʱ������־������
	lua_pushinteger(L, kJsonUnicodes);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_SIMPLE_ESCAPE");		// ����\��"����ת�壬�����Ķ�����
	lua_pushinteger(L, kJsonSimpleEscape);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_EMPTY_TOOBJECT");		// ��tableΪ��ʱ�����������Ĭ����{}������[]
	lua_pushinteger(L, kJsonEmptyToObject);
	lua_rawset(L, -3);

	lua_pushliteral(L, "JSON_DROPNULL");			// ����־������ڣ�����ʱ���������ĳ��ֵΪngx.null������Ե����������nullֵ��������ʱ�������nullֵ�������lua ngx.null
	lua_pushinteger(L, kJsonDropNull);
	lua_rawset(L, -3);

	lua_pushliteral(L, "HTML_RESERVED_ONLY");		// htmlentitiesenc����������HTML��Ԥ��ʵ����ţ�������ISO-XXXXϵ�еķ���
	lua_pushinteger(L, kHtmlEntReservedOnly);
	lua_rawset(L, -3);

	// ������չ�ĺ���
	luaL_register(L, NULL, procs);

	lua_pop(L, 1);
}
