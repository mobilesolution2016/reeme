#ifndef _REEME_EXTC_JSON_H__
#define _REEME_EXTC_JSON_H__

// 下面是ASCII字符属性表，1表示符号，2表示大小写字母，3表示数字，4表示可以用于组合整数或小数的符号
// 1 = 0~9
// 2 = + - .
// 3 = , ] }
// 4&5 = a-z
// 6&7 = A-Z
static uint8_t json_value_char_tbl[128] = 
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0,		// 0~32
	0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 2, 3, 2, 2, 0,	// 33~47
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 48~57
	0, 0, 0, 0, 0, 0, 0,	// 58~64
	6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,	// 65~92
	0, 0, 3, 0, 0, 0,	// 91~96
	4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,	// 97~122
	0, 0, 3, 0, 0,
};
static uint8_t json_invisibles_allowed[33] = 
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

static uint8_t json_escape_chars[256] = { 0 };
static uint8_t json_unescape_chars[256] = { 0 };

struct initJsonEscapeChars
{
	initJsonEscapeChars()
	{
		json_escape_chars['\\'] = 1;
		json_escape_chars['/'] = 1;
		json_escape_chars['"'] = 1;
		json_escape_chars['\\'] = 1;
		json_escape_chars['\n'] = 'n';
		json_escape_chars['\r'] = 'r';
		json_escape_chars['\t'] = 't';
		json_escape_chars['\a'] = 'a';
		json_escape_chars['\f'] = 'f';
		json_escape_chars['\v'] = 'v';
		json_escape_chars['\b'] = 'b';
		for(uint32_t i = 0x80; i < 256; ++ i)
			json_escape_chars[i] = 2;

		json_unescape_chars['n'] = '\n';
		json_unescape_chars['r'] = '\r';
		json_unescape_chars['t'] = '\t';
		json_unescape_chars['a'] = '\a';
		json_unescape_chars['f'] = '\f';
		json_unescape_chars['v'] = '\v';
		json_unescape_chars['b'] = '\b';

		json_unescape_chars['u'] = 'u';
		json_unescape_chars['/'] = '/';
		json_unescape_chars['\\'] = '\\';
		json_unescape_chars['\''] = '\'';
		json_unescape_chars['"'] = '"';
	}
};

class JSONString
{
public:
	inline JSONString()
		: pString(0), nLength(0)
	{
	}
	inline JSONString(char* p, int i)
		: pString(p), nLength(i)
	{
	}
	inline JSONString(char* p, char* pEnd)
		: pString(p), nLength((int)(pEnd - p))
	{
	}

	inline void empty()
	{
		pString = 0;
		nLength = 0;
	}

	char		*pString;
	size_t		nLength;
} ;

enum JSONAttrType
{
	JATValue,
	JATString,
	JATObject,
	JATArray
} ;
enum JSONValueType
{
	JVTNone,
	JVTTrue,
	JVTFalse,
	JVTNull,
};

class JSONAttribute
{
public:
	JSONString		name;
	JSONString		value;
	size_t			nameHash;
	JSONAttrType	kType;
	JSONValueType	kValType;

	inline void hash()
	{
		nameHash = hashString<size_t>(name.pString);
	}
} ;

//////////////////////////////////////////////////////////////////////////
#define MAX_PARSE_LEVEL		200	// JSON的解析深度如果超过这个值，则会被报错
#define SKIP_WHITES()\
	while(pReadPos != m_pMemEnd)\
	{\
		uint8_t ch = pReadPos[0];\
		if (ch > 32)\
			break;\
		if (json_invisibles_allowed[ch] != 1)\
		{\
			m_iErr = kErrorSymbol;\
			return 0;\
		}\
		pReadPos ++;\
	}\
	if (pReadPos >= m_pMemEnd) return 0;

#define SKIP_TO(dst)\
	m_pLastPos = pReadPos;\
	while(pReadPos != m_pMemEnd)\
	{\
		uint8_t ch = pReadPos[0];\
		if (ch > 32 || ch == dst)\
			break;\
		if (json_invisibles_allowed[ch] != 1)\
		{\
			m_iErr = kErrorSymbol;\
			return 0;\
		}\
		pReadPos ++;\
	}\
	if (pReadPos >= m_pMemEnd) return 0;

class JSONFile
{
public:
	enum
	{
		kErrorNotClosed = 1,
		kErrorEnd,
		kErrorAttribute,
		kErrorCloseNode,
		kErrorName,
		kErrorSymbol,
		kErrorMaxDeeps,
		kErrorValue
	} ;

	char				*m_pMemory, *m_pMemEnd;
	char				*m_pLastPos;
	size_t				m_nMemSize;
	int					m_iErr, m_iNeedSetMarker, m_iTableIndex;
	bool				m_bFreeMem, m_bQuoteStart;
	JSONValueType		m_kValType;

	uint32_t			m_nOpens;
	JSONAttrType		m_opens[MAX_PARSE_LEVEL];
	JSONAttribute		m_commonAttr;

	lua_State*			L;

protected:
	//数组/子节点节点类型的节点被结束
	void onNodeEnd(bool bIsArray)
	{
		if (m_nOpens)
			lua_settable(L, -3);
	}

	//一个属性节点被确定。如果是数组或子节点类型，则此时数组/子节点数据还未确定，要到onNodeEnd才说明数组/子节点结束。而字符串/值类型的节点则是其值已经读完时本函数才被调用
	void onAddAttr(const JSONAttribute* attr, unsigned count)
	{
		if (attr->name.nLength)
			lua_pushlstring(L, attr->name.pString, attr->name.nLength);
		else
			lua_pushinteger(L, count + 1);

		const JSONString& val = attr->value;
		switch(attr->kType)
		{
		case JATValue:
			switch (attr->kValType)
			{
			case JVTNone:
				if (strchr(val.pString, '.') == 0)
#ifdef REEME_64
					lua_pushinteger(L, strtoll(val.pString, 0, 10));
#else
					lua_pushinteger(L, strtol(val.pString, 0, 10));
#endif
				else
					lua_pushnumber(L, strtod(val.pString, 0));
				break;
			case JVTTrue:
				lua_pushboolean(L, 1);
				break;
			case JVTFalse:
				lua_pushboolean(L, 0);
				break;
			case JVTNull:
				lua_pushlightuserdata(L, NULL);
				break;
			}
			lua_rawset(L, -3);
			break;

		case JATString:
			lua_pushlstring(L, val.pString, val.nLength);
			lua_rawset(L, -3);
			break;

		case JATArray:
			lua_newtable(L);
			break;

		case JATObject:
			lua_newtable(L);
			break;
		}
	}

	JSONAttribute* getTmpAttr()
	{
		memset(&m_commonAttr, 0, sizeof(m_commonAttr));
		return &m_commonAttr;
	}

public:
	inline JSONFile(lua_State* p)
		: m_pMemory		(0)
		, m_nMemSize	(0)
		, m_pLastPos	(0)
		, m_iErr		(0)
		, m_bFreeMem	(true)
		, m_nOpens		(0)
		, L				(p)
	{
	}
	~JSONFile()
	{
		if (m_bFreeMem && m_pMemory)
			free(m_pMemory);
	}

	size_t parse(const char* mem, size_t nSize, bool copyIt, int needSetMarker)
	{
		m_iErr = 0;
		m_iTableIndex = lua_gettop(L);
		m_iNeedSetMarker = needSetMarker;

		if (copyIt)
		{
			m_pMemory = (char*)malloc(nSize + 1);
			memcpy(m_pMemory, mem, nSize);
			m_pMemory[nSize] = 0;

			m_nMemSize = nSize;
			m_bFreeMem = true;
		}
		else
		{
			m_pMemory = const_cast<char*>(mem);
			m_nMemSize = nSize;
			m_bFreeMem = false;
		}
		m_pMemEnd = m_pMemory + nSize;

		char* pReadPos = parseRoot(m_pLastPos = m_pMemory);
		if (!pReadPos)
			return 0;

		if (m_nOpens)
		{
			//未正常关闭
			m_iErr = kErrorNotClosed;
			return 0;
		}

		return pReadPos - m_pMemory;
	}

	const char* getError()
	{
		switch(m_iErr)
		{
		case kErrorNotClosed:
		{
			static char msg[] = { "node not closed" };
			return msg;
		}
		break;

		case kErrorEnd:
		{
			static char msg[] = { "error end" };
			return msg;
		}
		break;

		case kErrorAttribute:
		{
			static char msg[] = { "error attribute or value" };
			return msg;
		}
		break;

		case kErrorCloseNode:
		{
			static char msg[] = { "incorrected close node" };
			return msg;
		}
		break;

		case kErrorName:
		{
			static char msg[] = { "name with no data" };
			return msg;
		}
		break;

		case kErrorSymbol:
		{
			static char msg[] = { "unrecognize/invalid symbol" };
			return msg;
		}
		break;

		case kErrorMaxDeeps:
		{
			static char msg[] = { "exceed max parse deepths" };
			return msg;
		}
		break;

		case kErrorValue:
		{
			static char msg[] = { "special value must be one of true|false|null" };
			return msg;
		}
		break;		
		}

		return "";
	}

	size_t summary(char* outs, size_t maxl)
	{
		size_t bytes = std::min((size_t)(m_pMemEnd - m_pLastPos), maxl);

		memcpy(outs, m_pLastPos, bytes);
		return bytes;
	}

private:
	char* parseRoot(char* pReadPos)
	{
		SKIP_WHITES();

		if (pReadPos[0] == '[')
		{
			pReadPos = parseArray(pReadPos + 1);
		}
		else if (pReadPos[0] == '{')
		{
			pReadPos = parseReadNode(pReadPos + 1, JATObject);
		}
		else
		{
			m_iErr = kErrorSymbol;
			return 0;
		}

		return pReadPos;
	}

	char* parseReadNode(char* pReadPos, JSONAttrType kParentType)
	{		
		if (m_nOpens == MAX_PARSE_LEVEL)
		{
			m_iErr = kErrorMaxDeeps;
			return 0;
		}

		m_opens[m_nOpens ++] = kParentType;

		SKIP_WHITES();

		// 解析属性		
		char endChar = 0;
		uint32_t cc = 0, eqSyms = 0;
		JSONValueType kValType = JVTNone;

		m_pLastPos = pReadPos;
		while(pReadPos != m_pMemEnd)
		{
			SKIP_WHITES();

			if (pReadPos[0] == '"')
			{
				JSONAttribute* attr = getTmpAttr();
				pReadPos = parseFetchString(pReadPos, attr->name);
				if (!pReadPos)
					return 0;

				//取下一个符号：冒号
				SKIP_TO(':');

				pReadPos ++;
				SKIP_WHITES();

				//判断是数组还是单值
				if (pReadPos[0] == '[')
				{
					for (eqSyms = 1; ; ++ eqSyms)
					{
						char ch = pReadPos[eqSyms];
						if (ch != '=')
							break;
						if (!ch)
						{
							m_iErr = kErrorSymbol;
							return 0;
						}
					}

					if (pReadPos[eqSyms] == '[')
					{
						// Lua多行字符串
						attr->kType = JATString;

						pReadPos = parseFetchLuaString(pReadPos + eqSyms + 1, eqSyms - 1, attr->value);

						onAddAttr(attr, 0);
					}
					else
					{
						//处理数组
						attr->kType = JATArray;

						onAddAttr(attr, 0);

						pReadPos = parseArray(pReadPos + 1);
					}

					if (!pReadPos)
						return 0;
					endChar = pReadPos[0];
				}
				else if (pReadPos[0] == '{')
				{
					//递归子节点
					attr->kType = JATObject;

					onAddAttr(attr, 0);

					pReadPos = parseReadNode(pReadPos + 1, JATObject);
					if (!pReadPos)
						return 0;

					endChar = pReadPos[0];
				}
				else
				{
					//取值
					pReadPos = parseFetchString(pReadPos, attr->value);
					if (!pReadPos)
						return 0;

					if (m_bQuoteStart)
						attr->kType = JATString;
					else
						attr->kValType = m_kValType;

					endChar = pReadPos[0];
					onAddAttr(attr, 0);
				}

				cc ++;
			}
			else if (cc == 0 && pReadPos[0] == '}')
			{
				//空的大括号
				endChar = pReadPos[0];

				if (m_iNeedSetMarker)
				{
					lua_pushvalue(L, m_iNeedSetMarker);
					lua_pushboolean(L, 1);
					lua_rawset(L, -3);
				}
			}
			else
			{
				m_iErr = kErrorAttribute;
				return 0;
			}

			//取下一个符号，只能是逗号或者}号
			if (endChar == '}')
			{
				if (m_nOpens == 0 || m_opens[m_nOpens - 1] != JATObject)
					return 0;

				m_nOpens --;
				onNodeEnd(false);

				return pReadPos + 1;
			}
			else if (endChar == ',' || endChar == ']')
			{
				pReadPos ++;
			}
			else
			{
				return 0;
			}
		}

		m_iErr = kErrorNotClosed;
		return pReadPos;
	}

	//从给定位置开始取一个字符串，直到空格或结束符为止。支持双引号字符串和非双引号字符串
	char* parseFetchString(char* pReadPos, JSONString& str)
	{
		char* pStart = str.pString = pReadPos, *pEndPos = 0;

		m_bQuoteStart = (pStart[0] == '"');
		m_pLastPos = pReadPos;

		if (m_bQuoteStart)
		{
			// 字符串型值
			pReadPos ++;
			str.pString = pEndPos = pReadPos;

			size_t i;
			uint8_t ch;
			for(i = 0; i < m_pMemEnd - pReadPos; ++ i)
			{
				ch = pReadPos[i];
				if (ch == 0)
				{
					m_iErr = kErrorEnd;
					return 0;
				}

				if (ch == '"' || ch == '\\')
					break;
			}

			pReadPos += i;
			pEndPos = pReadPos;

			if (ch == '\\')
			{
				// 如果此时ch为\则说明字符串中含有转义符号，因此进入带有转义的字符串的处理过程。否则，上面的处理就直接结束了整个字符串值的获取过程
				while(pReadPos < m_pMemEnd)
				{
					if (ch == '\\')
					{
						uint8_t next = json_unescape_chars[pReadPos[1]];
						if (next == 0)
						{
							m_iErr = kErrorSymbol;
							return 0;
						}

						if (next == 'u')
						{
							// 解Unicode到UTF8
							uint32_t unicode = readUnicode(pReadPos + 2);
							pEndPos = unicode2utf8(unicode, pEndPos);
							pReadPos += 6;
						}
						else 
						{
							*pEndPos ++ = next;
							pReadPos += 2;
						}						
					}
					else if (ch != '"')
					{
						*pEndPos ++ = ch;
						pReadPos ++;
					}
					else
						break;

					ch = pReadPos[0];
				}
			}

			if (pReadPos >= m_pMemEnd)
			{
				m_iErr = kErrorEnd;
				return 0;
			}
		}
		else
		{
			// 数值或特殊值
			m_kValType = JVTNone;
			while(pReadPos != m_pMemEnd)
			{
				uint8_t ch = *pReadPos;
				if (ch <= 32)
				{
					if (!json_invisibles_allowed[ch])
					{
						m_iErr = kErrorSymbol;
						return 0;
					}

					if (!pEndPos)
						pEndPos = pReadPos;

					pReadPos ++;
					continue;
				}
				if (ch >= 127)
				{
					m_iErr = kErrorSymbol;
					return 0;
				}

				uint8_t flag = json_value_char_tbl[ch];
				if (flag == 3)
					break;

				if (pEndPos)
				{
					m_iErr = m_kValType != JVTNone ? kErrorValue : kErrorEnd;
					return 0;
				}

				//数字表达式是可以的
				if (flag <= 2 || ch == 'e')
				{
					pReadPos ++;
					continue;
				}

				//或者一些固定单词表达式也是可以的，如true、false、null
				if (flag >= 4)
				{
					if (strncmp(pReadPos, "true", 4) == 0)
					{
						m_kValType = JVTTrue;
						pReadPos += 4;
					}
					else if (strncmp(pReadPos, "false", 5) == 0)
					{
						m_kValType = JVTFalse;
						pReadPos += 5;
					}
					else if (strncmp(pReadPos, "null", 4) == 0)
					{
						m_kValType = JVTNull;
						pReadPos += 4;
					}
					else
					{
						m_iErr = kErrorValue;
						break;
					}

					pEndPos = pReadPos;
				}
			}
		}

		if (pEndPos)
			str.nLength = pEndPos - pStart;
		else
			str.nLength = pReadPos - pStart;
		if (m_bQuoteStart)
		{
			str.nLength --;
			pReadPos ++;
		}

		return pReadPos;
	}

	//读取Lua多行字符串
	char* parseFetchLuaString(char* pReadPos, uint32_t eqSyms, JSONString& str)
	{
		m_pLastPos = pReadPos;

		char* src = pReadPos;
		size_t i, k, ik, iend = m_pMemEnd - pReadPos - 2;
		for(i = 0; i < iend; ++ i)
		{
			if (src[i] == ']')
			{
				for (k = 1; k < eqSyms; ++ k)
				{
					if (src[i + k] != '=')
						break;
				}

				ik = i + k;
				if (ik < iend && k - 1 == eqSyms && src[ik] == ']')
				{
					str.pString = src;
					str.nLength = i;
					return src + ik;
				}
			}
		}

		m_iErr = kErrorEnd;
		return 0;
	}

	//读取数组
	char* parseArray(char* pReadPos)
	{
		uint32_t cc = 0;

		if (m_nOpens == MAX_PARSE_LEVEL)
		{
			m_iErr = kErrorMaxDeeps;
			return 0;
		}

		m_pLastPos = pReadPos;
		m_opens[m_nOpens ++] = JATArray;

		while(pReadPos != m_pMemEnd)
		{
			SKIP_WHITES();

			uint8_t ch = pReadPos[0];
			if (ch == '{')
			{
				//一个新的节点的开始
				JSONAttribute* attr = getTmpAttr();
				attr->kType = JATObject;

				onAddAttr(attr, cc);
				pReadPos = parseReadNode(pReadPos + 1, JATObject);
			}
			else if (ch == '[')
			{
				//一个新的节点的开始
				JSONAttribute* attr = getTmpAttr();
				attr->kType = JATArray;

				onAddAttr(attr, cc);
				pReadPos = parseReadNode(pReadPos + 1, JATArray);
			}
			else if (ch == ',')
			{
				//还有值
				pReadPos ++;
				continue;
			}
			else if (ch == ']')
			{
				//结束
				if (m_nOpens == 0 || m_opens[m_nOpens - 1] != JATArray)
					return 0;

				m_nOpens --;
				pReadPos ++;

				onNodeEnd(true);				
				break;
			}
			else if (ch == '"' || json_value_char_tbl[ch] <= 2 || json_value_char_tbl[ch] >= 4)
			{
				//数值类
				JSONAttribute* attr = getTmpAttr();

				pReadPos = parseFetchString(pReadPos, attr->value);
				if (!pReadPos)
					break;

				if (m_bQuoteStart)
					attr->kType = JATString;

				onAddAttr(attr, cc);
			}
			else
			{
				m_iErr = kErrorSymbol;
				pReadPos = 0;
			}

			if (!pReadPos)
				break;

			cc ++;
		}

		return pReadPos;
	}

	inline uint32_t readUnicode(const char* p)
	{
		uint32_t code = 0;
		for(uint32_t i = 0, shift = 12; i < 4; ++ i, shift -= 4)
		{
			uint8_t ch = p[i];
			uint8_t flag = json_value_char_tbl[ch];

			switch(flag)
			{
			case 1: code |= (ch - '0') << shift; break;
			case 4: code |= (ch - 'a' + 10) << shift; break;
			case 6: code |= (ch - 'A' + 10) << shift; break;
			}
		}

		return code;
	}

	inline uint32_t unicode2utf8(uint32_t wchar)
	{
		if (wchar < 0xC0)
			return 1;
		if (wchar < 0x800)
			return 2;
		if (wchar < 0x10000)
			return 3;
		if (wchar < 0x200000)
			return 4;
		//if (wchar < 0x4000000)
		//	return 5;
		//if (wchar < 0x80000000)
		//	return 6;

		return 1;
	}
	char* unicode2utf8(uint32_t _wchar, char *_utf8)
	{	
		char *utf8 = _utf8;
		uint32_t wchar = _wchar, len = 0;

		if (wchar < 0xC0)
		{ 
			utf8[0] = (char)wchar;
			len = 1;
		}
		else if (wchar < 0x800)
		{
			utf8[0] = 0xc0 | (wchar >> 6);
			utf8[1] = 0x80 | (wchar & 0x3f);
			len = 2;
		}
		else if (wchar < 0x10000)
		{
			utf8[0] = 0xe0 | (wchar >> 12);
			utf8[1] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[2] = 0x80 | (wchar & 0x3f);
			len = 3;
		}
		else if (wchar < 0x200000) 
		{
			utf8[0] = 0xf0 | ((int)wchar >> 18);
			utf8[1] = 0x80 | ((wchar >> 12) & 0x3f);
			utf8[2] = 0x80 | ((wchar >> 6) & 0x3f);
			utf8[3] = 0x80 | (wchar & 0x3f);
			len = 4;
		}
		//else if (wchar < 0x4000000)
		//{
		//	utf8[len ++] = 0xf8 | ((int)wchar >> 24);
		//	utf8[len ++] = 0x80 | ((wchar >> 18) & 0x3f);
		//	utf8[len ++] = 0x80 | ((wchar >> 12) & 0x3f);
		//	utf8[len ++] = 0x80 | ((wchar >> 6) & 0x3f);
		//	utf8[len ++] = 0x80 | (wchar & 0x3f);
		//}
		//else if (wchar < 0x80000000)
		//{
		//	utf8[len ++] = 0xfc | ((int)wchar >> 30);
		//	utf8[len ++] = 0x80 | ((wchar >> 24) & 0x3f);
		//	utf8[len ++] = 0x80 | ((wchar >> 18) & 0x3f);
		//	utf8[len ++] = 0x80 | ((wchar >> 12) & 0x3f);
		//	utf8[len ++] = 0x80 | ((wchar >> 6) & 0x3f);
		//	utf8[len ++] = 0x80 | (wchar & 0x3f);
		//}

		return utf8 + len;
	}
} ;

#endif