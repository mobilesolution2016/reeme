#ifndef _REEME_EXTC_JSON_H__
#define _REEME_EXTC_JSON_H__

// ������ASCII�ַ����Ա�1��ʾ���ţ�2��ʾ��Сд��ĸ��3��ʾ���֣�4��ʾ�����������������С���ķ���
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
static uint8_t integer64_valid_bits[256] = { 0 };
static uint8_t string_fmt_valid_fmt[256] = { 0 };
static uint8_t string_template_ctls[256] = { 0 };
static uint8_t string_htmlent_ctls[65536] = { 0 };
static const char string_htmlent_strs[][12] = {
	"&quot;", "&apos;", "&amp;", "&lt;", "&gt;",
	// ISO 8859-1 ����ʵ��
	"&nbsp;", "&iexcl;", "&cent;", "&pound;", "&curren;", "&yen;", "&brvbar;", "&sect;", "&uml;", "&copy;", "&ordf;", "&laquo;", "&not;", "&shy;", "&reg;", "&macr;", "&deg;", "&plusmn;", "&sup2;", "&sup3;", "&acute;", "&micro;", "&para;", "&middot;", "&cedil;", "&sup1;", "&ordm;", "&raquo;", "&frac14;", "&frac12;", "&frac34;", "&iquest;", "&times;", "&divide;",
	// ISO 8859-1 �ַ�ʵ��
	"&Agrave;", "&Aacute;", "&Acirc;", "&Atilde;", "&Auml;", "&Aring;", "&AElig;", "&Ccedil;", "&Egrave;", "&Eacute;", "&Ecirc;", "&Euml;", "&Igrave;", "&Iacute;", "&Icirc;", "&Iuml;", "&ETH;", "&Ntilde;", "&Ograve;", "&Oacute;", "&Ocirc;", "&Otilde;", "&Ouml;", "&Oslash;", "&Ugrave;", "&Uacute;", "&Ucirc;", "&Uuml;", "&Yacute;", "&THORN;", "&szlig;", "&agrave;", "&aacute;", "&acirc;", "&atilde;", "&auml;", "&aring;", "&aelig;", "&ccedil;", "&egrave;", "&eacute;", "&ecirc;", "&euml;", "&igrave;", "&iacute;", "&icirc;", "&iuml;", "&eth;", "&ntilde;", "&ograve;", "&oacute;", "&ocirc;", "&otilde;", "&ouml;", "&oslash;", "&ugrave;", "&uacute;", "&ucirc;", "&uuml;", "&yacute;", "&thorn;", "&yuml;"
};
// ���е�HTMLʵ��
static uint8_t string_htmlent_dblcodes[] = {
	// ISO 8859-1 ����ʵ��
	0x00, 0x20, 0xC2, 0xA1, 0xC2, 0xA2, 0xC2, 0xA3, 0xC2, 0xA4, 0xC2, 0xA5, 0xC2, 0xA6, 0xC2, 0xA7, 0xC2, 0xA8, 0xC2, 0xA9, 0xC2, 0xAA, 0xC2, 0xAB, 0xC2, 0xAC, 0x00, 0xAD, 0xC2, 0xAE, 0xC2, 0xAF, 0xC2, 0xB0,
	0xC2, 0xB1, 0xC2, 0xB2, 0xC2, 0xB3, 0xC2, 0xB4, 0xC2, 0xB5, 0xC2, 0xB6, 0xC2, 0xB7, 0xC2, 0xB8, 0xC2, 0xB9, 0xC2, 0xBA, 0xC2, 0xBB, 0xC2, 0xBC, 0xC2, 0xBD, 0xC2, 0xBE, 0xC2, 0xBF, 0xC3, 0x97, 0xC3, 0xB7,
	// ISO 8859-1 �ַ�ʵ��			
	0xC3, 0x80, 0xC3, 0x81, 0xC3, 0x82, 0xC3, 0x83, 0xC3, 0x84, 0xC3, 0x85, 0xC3, 0x86, 0xC3, 0x87, 0xC3, 0x88, 0xC3, 0x89, 0xC3, 0x8A, 0xC3, 0x8B, 0xC3, 0x8C, 0xC3, 0x8D, 0xC3, 0x8E, 0xC3, 0x8F, 0xC3, 0x90, 
	0xC3, 0x91, 0xC3, 0x92, 0xC3, 0x93, 0xC3, 0x94, 0xC3, 0x95, 0xC3, 0x96, 0xC3, 0x98, 0xC3, 0x99, 0xC3, 0x9A, 0xC3, 0x9B, 0xC3, 0x9C, 0xC3, 0x9D, 0xC3, 0x9E, 0xC3, 0x9F, 0xC3, 0xA0, 0xC3, 0xA1, 0xC3, 0xA2, 
	0xC3, 0xA3, 0xC3, 0xA4, 0xC3, 0xA5, 0xC3, 0xA6, 0xC3, 0xA7, 0xC3, 0xA8, 0xC3, 0xA9, 0xC3, 0xAA, 0xC3, 0xAB, 0xC3, 0xAC, 0xC3, 0xAD, 0xC3, 0xAE, 0xC3, 0xAF, 0xC3, 0xB0, 0xC3, 0xB1, 0xC3, 0xB2, 0xC3, 0xB3,
	0xC3, 0xB4, 0xC3, 0xB5, 0xC3, 0xB6, 0xC3, 0xB8, 0xC3, 0xB9, 0xC3, 0xBA, 0xC3, 0xBB, 0xC3, 0xBC, 0xC3, 0xBD, 0xC3, 0xBE, 0xC3, 0xBF, 
};

typedef MAP_CLASS_NAME<StringPtrKey, uint32_t> HtmlEntStringsMap;
static HtmlEntStringsMap gHtmlEntStrings;

struct initJsonEscapeChars
{
	initJsonEscapeChars()
	{
		uint32_t i;

		json_escape_chars['\\'] = 1;
		//json_escape_chars['/'] = 1;
		json_escape_chars['"'] = 1;
		json_escape_chars['\\'] = 1;
		json_escape_chars['\n'] = 'n';
		json_escape_chars['\r'] = 'r';
		json_escape_chars['\t'] = 't';
		json_escape_chars['\a'] = 'a';
		json_escape_chars['\f'] = 'f';
		json_escape_chars['\v'] = 'v';
		json_escape_chars['\b'] = 'b';
		for(i = 0x80; i < 256; ++ i)
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

		for(i = 0; i < 256; ++ i)
			integer64_valid_bits[i] = 0xFF;
		integer64_valid_bits['U'] = 1;
		integer64_valid_bits['L'] = 2;
		integer64_valid_bits['u'] = 3;
		integer64_valid_bits['l'] = 4;
		for(i = 0; i < 10; ++ i)
			integer64_valid_bits['0' + i] = 0;

		string_fmt_valid_fmt['s'] = string_fmt_valid_fmt['S'] = 1;
		string_fmt_valid_fmt['f'] = string_fmt_valid_fmt['F'] = 2;
		string_fmt_valid_fmt['g'] = string_fmt_valid_fmt['G'] = 2;
		string_fmt_valid_fmt['c'] = string_fmt_valid_fmt['C'] = 3;

		string_fmt_valid_fmt['u'] = string_fmt_valid_fmt['U'] = 4;
		string_fmt_valid_fmt['d'] = string_fmt_valid_fmt['D'] = string_fmt_valid_fmt['i'] = 5;
		string_fmt_valid_fmt['x'] = 6;	string_fmt_valid_fmt['X'] = 7;

		// ���²���֧�ֻ�δ֧��
		string_fmt_valid_fmt['-'] = 0x10;		// ����룬����ո��������Ҷ��룬����ո�
		string_fmt_valid_fmt['+'] = 0x20;		// ����ʱ�����������
		string_fmt_valid_fmt['#'] = 0x40;		// x��Xʱ������0x��С��ʱһ������С���㣬g��Gʱ������β����0
		string_fmt_valid_fmt[' '] = 0x80;		// ����ʱ���գ�����ʱ��-���

		// ���е�ģ����Ʒ�
		string_template_ctls['%'] = string_template_ctls[':'] = string_template_ctls['='] = string_template_ctls['?'] = string_template_ctls['-'] = string_template_ctls['#'] = 1;


		string_htmlent_ctls['"'] = 1;
		string_htmlent_ctls['\''] = 2;
		string_htmlent_ctls['&'] = 3;
		string_htmlent_ctls['<'] = 4;
		string_htmlent_ctls['>'] = 5;
		for (size_t i = 0; i < sizeof(string_htmlent_dblcodes) / sizeof(string_htmlent_dblcodes[0]); i += 2)
		{
			uint32_t v = ((uint32_t)string_htmlent_dblcodes[i] << 8) | (uint32_t)string_htmlent_dblcodes[i + 1];
			string_htmlent_ctls[v] = (i >> 1) + 6;
		}

		assert(sizeof(string_htmlent_dblcodes) / sizeof(string_htmlent_dblcodes[0]) / 2 + 5 == sizeof(string_htmlent_strs) / sizeof(string_htmlent_strs[0]));

		for (size_t i = 0; i < sizeof(string_htmlent_strs) / sizeof(string_htmlent_strs[0]); ++ i)
		{
			assert(strlen(string_htmlent_strs[i]) < 12);
			StringPtrKey key(string_htmlent_strs[i]);
			gHtmlEntStrings.insert(HtmlEntStringsMap::value_type(key, i));
		}
	}
} _g_initJsonEscapeChars;

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
	union {
		double		dbl;
		uint64_t	u64;
	} value;
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
	JVTDecimal,
	JVTHex,
	JVTOctal,
	JVTDouble,
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

	inline void hash()
	{
		nameHash = hashString<size_t>(name.pString);
	}
} ;


//////////////////////////////////////////////////////////////////////////
// ����Json
#define SEARCH_SKIP_WHITES() while(readPtr < srcEnd)\
	{\
		uint8_t ch = readPtr[0];\
		if (ch > 32)\
			break;\
		if (json_invisibles_allowed[ch] != 1)\
			return luaL_error(L, "string.searchjson: Unknown invisible char at %u", readPtr - src + 1);\
		readPtr ++;\
	}

#define SEARCH_SKIP_STRING() while(readPtr < srcEnd)\
	{\
		if (readPtr[0] == '"') { readPtr ++; break; }\
		if (readPtr[0] == 0) return luaL_error(L, "string.searchjson: Error end json at %u", readPtr - src + 1);\
		if (readPtr[0] == '\\') { readPtr += 2; }\
		else readPtr ++;\
	}

class JsonSearcher
{
public:
	JsonSearcher(char* s, size_t srcLen)
		: src(s), readPtr(s), srcEnd(s + srcLen)
		, beginPos(0), endPos(0)
		, haveResult(false)
	{

	}

	JsonSearcher(char* s, char* se)
		: src(s), readPtr(s), srcEnd(se)
		, beginPos(0), endPos(0)
		, haveResult(false)
	{

	}

	~JsonSearcher()
	{

	}

	inline bool isEnded() const { return readPtr >= srcEnd; }
	inline int pushResult(lua_State* L)
	{
		if (beginPos)
			lua_pushlstring(L, beginPos, (endPos ? endPos : srcEnd) - beginPos);
		else if (haveResult)
			lua_pushlstring(L, src, srcEnd - src);
		else
			lua_pushboolean(L, 0);
		return 1;
	}
	inline void reduceJson()
	{
		src = readPtr = beginPos;
		if (endPos)
			srcEnd = endPos;
		beginPos = endPos = 0;
	}

	int skipWhites(lua_State* L)
	{
		SEARCH_SKIP_WHITES();

		return 0;
	}

	int skipTo(lua_State* L, uint8_t toChar1, uint8_t toChar2 = 0, int* piMembersCount = NULL)
	{
		int cc = -1, valueChars = 0;
		while (readPtr < srcEnd)
		{
			SEARCH_SKIP_WHITES();

			uint8_t ch = readPtr[0];
			if (ch == toChar1 || ch == toChar2)
			{
				if (valueChars)
					cc ++;
				if (piMembersCount)
					*piMembersCount = cc + 1;
				return 0;
			}

			readPtr ++;
			valueChars ++;

			if (ch == '"')
			{
				SEARCH_SKIP_STRING();
				SEARCH_SKIP_WHITES();

				if (readPtr[0] == ':')
				{
					readPtr ++;
					SEARCH_SKIP_WHITES();
				}
				else
				{
					assert(readPtr[0] == ',' || readPtr[0] == ']' || readPtr[0] == '}');
				}
			}
			else if (ch == '{')
			{
				int r = skipTo(L, '}');
				if (r) return r;
				readPtr ++;
			}
			else if (ch == '[')
			{
				int r = skipTo(L, ']');
				if (r) return r;
				readPtr ++;
			}
			else if (ch == ',')
			{
				valueChars = 0;
				cc ++;
			}
			else if (!(json_value_char_tbl[ch] || json_escape_chars[ch]))
				break;
		}

		return 0;
	}

	int findEnd(lua_State* L, int maxMembersCount = 1)
	{
		int r;
		for (int i = 0; i < maxMembersCount; ++ i)
		{
			if (i > 0 && readPtr[0] != ',')
				return -1;

			readPtr ++;
			memberCount = 0;
			switch (beginPos[0])
			{
			case '[':
				// ���Key��ֵ��һ������
				r = skipTo(L, ']', 0, &memberCount);
				readPtr ++;
				break;

			case '{':
				// ���Key��ֵ��һ������
				r = skipTo(L, '}', 0, &memberCount);
				readPtr ++;
				break;

			case '"':
				// ���Key��ֵ��һ���ַ����������ַ���β
				SEARCH_SKIP_STRING();
				break;

			default:
				// ���Key��ֵ��һ��ֵ�����ǵ�������������ߵ���һ��ֵʱֹͣ
				r = skipTo(L, '}', ',');
				if (readPtr[0] == '}')
					readPtr ++;
				break;
			}

			if (r) return r;
		}

		if (maxMembersCount > 1)
			memberCount = maxMembersCount;

		// �ҵ��˽���λ��
		endPos = readPtr;
		return 0;
	}

	int testLen(lua_State* L)
	{
		SEARCH_SKIP_WHITES();

		int cc = -1;
		char indexChar = *readPtr ++, endChar;

		if (indexChar == '[')
			endChar = ']';
		else if (indexChar == '{')
			endChar = '}';
		else
			return 0;

		while (readPtr != srcEnd)
		{
			memberCount = 0;
			int r = skipTo(L, endChar, ',', &memberCount);
			if (r) return r;

			if (memberCount)
				cc ++;

			if (readPtr[0] == endChar)
				break;

			if (isEnded() || readPtr[0] != ',')
				return 0;

			readPtr ++;
		}

		lua_pushinteger(L, cc + 1);
		return 1;
	}

	int findIndex(lua_State* L, long index, long indexEnd, bool findTheEnd)
	{
		haveResult = false;
		SEARCH_SKIP_WHITES();

		if (readPtr[0] != '[')
			return 0;

		readPtr ++;
		for (long i = 0; i < index; ++ i)
		{
			int r = skipTo(L, ',', ']');
			if (r) return r;

			if (isEnded())
				return luaL_error(L, "string.searchjson meet not closed json", 0);

			if (readPtr[0] == ']')
			{
				lua_pushboolean(L, 0);
				return 1;
			}

			readPtr ++;
		}

		int r;
		beginPos = readPtr;
		if (findTheEnd)
		{
			// �ҵ�������
			r = findEnd(L);
		}
		else if (indexEnd >= index)
		{
			// �ҵ�ָ��Ԫ�ظ����Ľ�����
			r = findEnd(L, indexEnd - index + 1);
		}

		if (r) return r;

		haveResult = true;
		return 0;
	}

	int findKey(lua_State* L, const std::string& strKey, bool bIsMultiKeys, bool findTheEnd)
	{
		haveResult = false;
		SEARCH_SKIP_WHITES();

		if (readPtr[0] != '{')
			return 0;

		readPtr ++;
		for (int r; readPtr < srcEnd; )
		{
			SEARCH_SKIP_WHITES();
			if (readPtr[0] != '"')
				return luaL_error(L, "string.searchjson meet not a string", 0);

			char* nameEnd = strchr(readPtr + 1, '"');
			if (!nameEnd)
				return luaL_error(L, "string.searchjson meet a string not end", 0);

			char* nameKey = readPtr;
			readPtr = nameEnd + 1;

			strNameKey.clear();
			strNameKey.append(nameKey, readPtr - nameKey);

			SEARCH_SKIP_WHITES();
			if (readPtr[0] != ':')
				return luaL_error(L, "string.searchjson meet a key have not value", 0);

			readPtr ++;
			SEARCH_SKIP_WHITES();

			if ((bIsMultiKeys && strstr(strKey.c_str(), strNameKey.c_str())) ||
				(!bIsMultiKeys && strcmp(strKey.c_str(), strNameKey.c_str()) == 0))
			{
				// ����ֵ�Ŀ�ʼλ�ã��ж�ֵ������
				beginPos = readPtr;
				if (findTheEnd)
				{
					r = findEnd(L);
					if (r) return r;
				}

				haveResult = true;
				break;
			}

			r = skipTo(L, ',', '}');
			if (r) return r;

			if (isEnded())
				return luaL_error(L, "string.searchjson meet not closed json", 0);

			if (readPtr[0] == '}')
			{
				lua_pushboolean(L, 0);
				return 1;
			}

			readPtr ++;
		}

		return 0;
	}

public:
	std::string			strNameKey;
	char				*src, *readPtr;
	char				*srcEnd;
	char				*beginPos, *endPos;
	bool				haveResult;
	int					memberCount;
};


//////////////////////////////////////////////////////////////////////////
// JSON�Ľ����������������ֵ����ᱻ����
#define MAX_PARSE_LEVEL		200
// double����������¿���holdס���������ֵ����ֵ
#define DOUBLE_UINT_MAX		9007199254740992

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

	struct CtlPath
	{
		bool	started;
		char	*path, *readPtr;
	};
	
	typedef std::vector<CtlPath> MultiPaths;

	char				*m_pMemory, *m_pMemEnd;
	char				*m_pLastPos;
	size_t				m_nMemSize;
	int					m_iErr;
	bool				m_bFreeMem, m_bQuoteStart, m_bNegativeVal, m_bDropObjNull;
	bool				m_bNeedSetMarker, m_bHavePath;
	JSONValueType		m_kValType;

	uint32_t			m_nOpens;
	std::string			m_strPath;
	MultiPaths			m_paths;
	size_t				m_opens[MAX_PARSE_LEVEL];
	JSONAttribute		m_commonAttr;

	lua_State*			L;

protected:
	//����/�ӽڵ�ڵ����͵Ľڵ㱻����
	void onNodeEnd(bool bIsArray)
	{
		if (m_nOpens)
			lua_rawset(L, -3);
	}

	//һ�����Խڵ㱻ȷ���������������ӽڵ����ͣ����ʱ����/�ӽڵ����ݻ�δȷ����Ҫ��onNodeEnd��˵������/�ӽڵ���������ַ���/ֵ���͵Ľڵ�������ֵ�Ѿ�����ʱ�������ű�����
	void onAddAttr(const JSONAttribute* attr, unsigned count)
	{
		if (attr->name.nLength)
			lua_pushlstring(L, attr->name.pString, attr->name.nLength);
		else
			lua_pushinteger(L, count + 1);

		double dv;
		uint64_t u64;
		const JSONString& val = attr->value;
		switch(attr->kType)
		{
		case JATValue:
			u64 = attr->value.value.u64;
			switch (m_kValType)
			{
			case JVTDecimal:
			case JVTOctal:
				if (m_bNegativeVal)
				{
					if (u64 < DOUBLE_UINT_MAX)
						lua_pushnumber(L, -(double)u64);
					else
						jsonPushInt64(-(int64_t)u64);
				}
				else if (u64 < DOUBLE_UINT_MAX)
					lua_pushnumber(L, u64);
				else
					jsonPushUInt64(u64);
				break;
			case JVTHex:
				if (u64 < DOUBLE_UINT_MAX)
					lua_pushnumber(L, u64);
				else
					jsonPushUInt64(u64);
				break;
			case JVTDouble:
				dv = *(double*)&u64;
				lua_pushnumber(L, m_bNegativeVal ? -dv : dv);
				break;
			case JVTTrue:
				lua_pushboolean(L, 1);
				break;
			case JVTFalse:
				lua_pushboolean(L, 0);
				break;
			case JVTNull:
				if (m_bDropObjNull)
				{
					lua_pop(L, 1);
					return ;
				}

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

	void jsonPushInt64(int64_t v)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
		lua_pushliteral(L, "int64_t");
		lua_pcall(L, 1, 1, 0);

		int64_t* pt = (int64_t*)const_cast<void*>(lua_topointer(L, -1));
		pt[0] = v;
	}
	void jsonPushUInt64(uint64_t v)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, kLuaRegVal_FFINew);
		lua_pushliteral(L, "uint64_t");
		lua_pcall(L, 1, 1, 0);

		uint64_t* pt = (uint64_t*)const_cast<void*>(lua_topointer(L, -1));
		pt[0] = v;
	}

	inline JSONAttribute* getTmpAttr()
	{
		memset(&m_commonAttr, 0, sizeof(m_commonAttr));
		return &m_commonAttr;
	}

	static inline uint32_t readUnicode(const char* p)
	{
		uint32_t code = 0;
		for (uint32_t i = 0, shift = 12; i < 4; ++ i, shift -= 4)
		{
			uint8_t ch = p[i];
			switch (json_value_char_tbl[ch])
			{
			case 1: code |= (ch - '0') << shift; break;
			case 4: code |= (ch - 'a' + 10) << shift; break;
			case 6: code |= (ch - 'A' + 10) << shift; break;
			}
		}

		return code;
	}

	static inline uint32_t unicode2utf8(uint32_t wchar)
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

	static char* unicode2utf8(uint32_t _wchar, char *_utf8)
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

public:
	inline JSONFile(lua_State* p, int dropObjNull, int needSetMarker)
		: m_pMemory		(NULL)
		, m_nMemSize	(0)
		, m_pLastPos	(NULL)
		, m_iErr		(0)
		, m_bFreeMem	(true)
		, m_bHavePath	(false)
		, m_nOpens		(0)
		, m_bDropObjNull(dropObjNull != 0)
		, m_bNeedSetMarker(needSetMarker != 0)
		, L				(p)
	{
	}
	~JSONFile()
	{
		if (m_bFreeMem && m_pMemory)
			free(m_pMemory);
	}

	bool initPath(const char* pathCtl)
	{
		m_bHavePath = true;

		CtlPath cp;
		std::string strPathTmp = pathCtl;
		m_strPath.reserve(std::max((size_t)256, strPathTmp.length() * 2));

		char* s;
		char* next = strtok_s(const_cast<char*>(strPathTmp.c_str()), "\r\n", &s);

		m_paths.reserve(8);
		while (next)
		{
			// �����е�[]�е�����ȡ����
			size_t nPos = m_strPath.length();

			char* endPtr;
			const char* pathMBracket = strchr(next, '[');
			if (pathMBracket)
			{
				if (pathMBracket > next)
					m_strPath.append(next, pathMBracket - next + 1);

				while (true)
				{
					long v1 = strtol(pathMBracket, &endPtr, 10), v2 = 0;
					if (endPtr[0] == '-')
						v2 = strtol(endPtr + 1, &endPtr, 10);

					if (v1 <= 1 || (v2 && v2 < v1))
						return false;
					if (endPtr[0] != ']')
						return false;

					if (v1)
					{
						uint32_t v = v1;
						m_strPath.append((char*)&v, sizeof(v));
					}
					if (v2)
					{
						uint32_t v = v2;
						m_strPath += '-';
						m_strPath.append((char*)&v, sizeof(v));
					}
					m_strPath += ']';

					next = endPtr + 1;
					pathMBracket = strchr(next, '[');
				}
			}
			else
			{
				m_strPath += next;
			}

			m_strPath += '\0';

			cp.path = const_cast<char*>(m_strPath.c_str()) + nPos;
			cp.readPtr = cp.path;
			cp.started = true;

			m_paths.push_back(cp);
			next = strtok_s(NULL, "\r\n", &s);
		}

		return true;
	}

	size_t parse(const char* mem, size_t nSize, bool copyIt)
	{
		m_iErr = 0;
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
			//δ�����ر�
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
	uint32_t checkPath(bool bCurrentInArray, uint32_t curIndex = 0, JSONString* pCurKey = NULL)
	{
		uint32_t rOk = 0;
		if (bCurrentInArray)
		{
			if (curIndex)
			{
				// �ж��������λ�õ��Ƿ�Ҫ
				for (MultiPaths::iterator ite = m_paths.begin(), iEnd = m_paths.end(); ite != iEnd; ++ ite)
				{
					CtlPath& cp = *ite;
					if (cp.started && cp.readPtr[0] == '[')
					{
						char* ptr = cp.readPtr + 1;
						uint32_t v = *(uint32_t*)ptr;
						ptr += sizeof(v);

						if (ptr[0] == '-')
						{
							uint32_t v2 = *(uint32_t*)(ptr + 1);
							ptr += sizeof(v);

							if (v >= curIndex && curIndex <= v2)
							{
								cp.readPtr = ptr + 1;
								rOk = 1;

								return v2 - v;
							}
						}
						else if (v == curIndex)
						{
							cp.readPtr = ptr + 1;
							rOk = 1;

							break;
						}
					}
				}
			}
			else
			{
				// ���ݿ�ʼ���ж��Ƿ���Path�����鿪ʼ
				for (MultiPaths::iterator ite = m_paths.begin(), iEnd = m_paths.end(); ite != iEnd; ++ ite)
				{
					CtlPath& cp = *ite;
					if (!cp.started && cp.readPtr[0] == '[')
					{
						cp.started = true;
						rOk = 1;
					}
					else if (cp.started)
					{
						if (cp.readPtr[0] == '[')
							rOk = 1;
						else
							ite = m_paths.erase(ite);
					}
				}
			}
		}
		else
		{
			if (pCurKey)
			{

			}
			else
			{

			}
		}

		return rOk;
	}

	char* keepToEnd(char* pReadPos)
	{
		if (m_paths.size() == 0)
			return m_pMemEnd;

		JsonSearcher s(pReadPos, m_pMemEnd);
		s.beginPos = pReadPos;

		int r = s.findEnd(L);
		if (r) return NULL;

		return s.endPos - 1;
	}

	char* parseRoot(char* pReadPos)
	{
		SKIP_WHITES();

		if (pReadPos[0] == '[')
		{
			pReadPos = parseArray(pReadPos + 1);
		}
		else if (pReadPos[0] == '{')
		{
			pReadPos = parseObject(pReadPos + 1);
		}
		else
		{
			m_iErr = kErrorSymbol;
			return 0;
		}

		return pReadPos;
	}

	//��ȡ����
	char* parseArray(char* pReadPos)
	{
		uint32_t cc = 0, maxDecodeCC = UINT_MAX;
		size_t pathCurLen = m_strPath.length();
		bool bHavePath = m_bHavePath;

		if (m_nOpens == MAX_PARSE_LEVEL)
		{
			m_iErr = kErrorMaxDeeps;
			return 0;
		}

		m_pLastPos = pReadPos;
		m_opens[m_nOpens ++] = JATArray | (pathCurLen << 4);
		
		if (bHavePath && !checkPath(true))
			return keepToEnd(pReadPos - 1);

		while (pReadPos != m_pMemEnd)
		{
			SKIP_WHITES();

			if (bHavePath)
			{
				uint32_t keepCC = checkPath(true, cc + 1);
				if (keepCC)
				{
					// ����Ԫ��������·�����δͨ�����޸�����������
					maxDecodeCC = keepCC;
					bHavePath = false;
				}
				else
				{
					// ����Ԫ��������·�����δͨ���������������Ԫ��
					pReadPos = keepToEnd(pReadPos);
					if (!pReadPos)
						break;

					if (pReadPos[0] == ',')
					{
						//����ֵ�����Լ�������
						pReadPos ++;
						continue;
					}
					else if (pReadPos[0] == ']')
					{
						// ��������Ѿ�������
						if (m_nOpens == 0 || (m_opens[m_nOpens - 1] & 0xF) != JATArray)
							return 0;

						m_nOpens --;
						pReadPos ++;
						break;
					}
					else
					{
						// ��Ӧ�ó��ֵķ���
						m_pLastPos = pReadPos;
						m_iErr = kErrorSymbol;
						pReadPos = 0;
						break;
					}
				}
			}

			uint8_t ch = pReadPos[0];
			if (ch == '{')
			{
				//һ���µĽڵ�Ŀ�ʼ
				JSONAttribute* attr = getTmpAttr();
				attr->kType = JATObject;

				onAddAttr(attr, cc);
				pReadPos = parseObject(pReadPos + 1);
			}
			else if (ch == '[')
			{
				//һ���µĽڵ�Ŀ�ʼ
				JSONAttribute* attr = getTmpAttr();
				attr->kType = JATArray;

				onAddAttr(attr, cc);
				pReadPos = parseArray(pReadPos + 1);
			}
			else if (ch == ',')
			{
				//����ֵ
				pReadPos ++;
				continue;
			}
			else if (ch == ']')
			{
				//����
			_end_array:
				if (m_nOpens == 0 || (m_opens[m_nOpens - 1] & 0xF) != JATArray)
					return 0;

				m_nOpens --;
				pReadPos ++;

				onNodeEnd(true);
				break;
			}
			else if (ch == '"' || json_value_char_tbl[ch] <= 2 || json_value_char_tbl[ch] >= 4)
			{
				//��ֵ��
				JSONAttribute* attr = getTmpAttr();

				pReadPos = fetchString(pReadPos, attr->value);
				if (!pReadPos)
					break;

				attr->kType = m_bQuoteStart ? JATString : JATValue;

				onAddAttr(attr, cc);
			}
			else
			{
				m_iErr = kErrorSymbol;
				pReadPos = 0;
			}

			if (!pReadPos)
				break;

			if (-- maxDecodeCC == 0)
			{
				// ����
			}

			cc ++;
		}

		return pReadPos;
	}

	char* parseObject(char* pReadPos)
	{
		if (m_nOpens == MAX_PARSE_LEVEL)
		{
			m_iErr = kErrorMaxDeeps;
			return 0;
		}

		m_opens[m_nOpens ++] = JATObject | (m_strPath.length() << 4);

		// ��������		
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
				pReadPos = fetchString(pReadPos, attr->name);
				if (!pReadPos)
					return 0;

				//ȡ��һ�����ţ�ð��
				SKIP_TO(':');

				pReadPos ++;
				SKIP_WHITES();

				if (m_bHavePath && !checkPath(true, cc + 1))
				{
					// ����Ԫ��������·�����δͨ���������������Ԫ��
					pReadPos = keepToEnd(pReadPos);
					if (!pReadPos)
						return 0;

					if (pReadPos[0] == ',')
						pReadPos ++;
					else if (pReadPos[0] == '}')
						goto _obj_end;
				}
				else
				{
					//�ж������黹�ǵ�ֵ
					if (pReadPos[0] == '[')
					{
						//for (eqSyms = 1; ; ++ eqSyms)
						//{
						//	char ch = pReadPos[eqSyms];
						//	if (ch != '=')
						//		break;
						//	if (!ch)
						//	{
						//		m_iErr = kErrorSymbol;
						//		return 0;
						//	}
						//}

						//if (pReadPos[eqSyms] == '[')
						//{
						//	// Lua�����ַ���
						//	attr->kType = JATString;

						//	pReadPos = fetchLuaString(pReadPos + eqSyms + 1, eqSyms - 1, attr->value);

						//	onAddAttr(attr, 0);
						//}
						//else
						//{
							//��������
							attr->kType = JATArray;

							onAddAttr(attr, 0);

							pReadPos = parseArray(pReadPos + 1);
						//}

						if (!pReadPos)
							return 0;
					}
					else if (pReadPos[0] == '{')
					{
						//�ݹ��ӽڵ�
						attr->kType = JATObject;

						onAddAttr(attr, 0);

						pReadPos = parseObject(pReadPos + 1);
						if (!pReadPos)
							return 0;
					}
					else
					{
						//ȡֵ
						pReadPos = fetchString(pReadPos, attr->value);
						if (!pReadPos)
							return 0;

						attr->kType = m_bQuoteStart ? JATString : JATValue;					
						onAddAttr(attr, 0);
					}

					SKIP_WHITES();
					endChar = pReadPos[0];

					cc ++;
				}
			}
			//else if (pReadPos[0] == '[')
			//{
			//	// һ���µ�����
			//	JSONAttribute* attr = getTmpAttr();
			//	attr->kType = JATArray;

			//	onAddAttr(attr, cc);

			//	pReadPos = parseArray(pReadPos + 1);
			//	if (!pReadPos)
			//		return 0;
			//}
			//else if (pReadPos[0] == '{')
			//{
			//	// һ������
			//	JSONAttribute* attr = getTmpAttr();
			//	attr->kType = JATObject;

			//	onAddAttr(attr, cc);

			//	pReadPos = parseObject(pReadPos + 1);
			//	if (!pReadPos)
			//		return 0;
			//}
			else if (cc == 0 && pReadPos[0] == '}')
			{
				//�յĴ�����
				endChar = '}';

				if (m_bNeedSetMarker)
				{
					lua_pushvalue(L, m_bNeedSetMarker ? 1 : 0);
					lua_pushboolean(L, 1);
					lua_rawset(L, -3);
				}
			}
			else
			{
				m_iErr = kErrorSymbol;
				return 0;
			}

			//ȡ��һ������
			if (endChar == '}')
			{
			_obj_end:
				if (m_nOpens == 0 || (m_opens[m_nOpens - 1] & 0xF) != JATObject)
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

	//�Ӹ���λ�ÿ�ʼȡһ���ַ�����ֱ���ո�������Ϊֹ��֧��˫�����ַ����ͷ�˫�����ַ���
	char* fetchString(char* pReadPos, JSONString& str)
	{
		char* pStart = str.pString = pReadPos, *pEndPos = 0;

		m_bQuoteStart = (pStart[0] == '"');
		m_pLastPos = pReadPos;

		if (m_bQuoteStart)
		{
			// �ַ�����ֵ
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
				// �����ʱchΪ\��˵���ַ����к���ת����ţ���˽������ת����ַ����Ĵ�����̡���������Ĵ����ֱ�ӽ����������ַ���ֵ�Ļ�ȡ����
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
							// ��Unicode��UTF8
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
			// ��ֵ������ֵ���ȴӵ�1�����Ų²�һ�¿�����ʲô���͵�ֵ
			m_kValType = JVTNone;
			m_bNegativeVal = false;

			uint8_t ch = pReadPos[0];
			if (ch == '+')
			{
				pReadPos ++;
				ch = pReadPos[0];
			}
			else if (ch == '-')
			{				
				pReadPos ++;
				ch = pReadPos[0];
				m_bNegativeVal = true;
			}

			uint8_t ctl = json_value_char_tbl[ch];
			if (ch == '0')
			{
				// 8���ƻ�16���ƿ���
				pReadPos ++;
				ch = pReadPos[0];
				if (ch == 'x' || ch == 'X')
				{
					// ʮ����������
					pReadPos ++;
					m_kValType = JVTHex;
					if (m_bNegativeVal)
					{
						m_iErr = kErrorValue;
						return 0;
					}
				}
				else if (ch == '.')
				{
					// С��
					m_kValType = JVTDouble;
				}
				else
				{
					// �˽�������
					m_kValType = JVTOctal;
				}
			}
			else if (ctl == 1)
			{
				// ʮ����������С��
				m_kValType = memchr(pReadPos, '.', m_pMemEnd - pReadPos) ? JVTDouble : JVTDecimal;
			}
			else if (ctl >= 4 && ctl <= 7)
			{
				// ����ֵ
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
					return 0;
				}
			}
			else if (ch == '.')
			{
				// ������
				m_kValType = JVTDouble;
			}

			// ת������ֵ
			switch (m_kValType)
			{
			case JVTDecimal:
				str.value.u64 = strtoull(pReadPos, &pReadPos, 10);
				break;
			case JVTHex:
				str.value.u64 = strtoull(pReadPos, &pReadPos, 16);
				break;
			case JVTOctal:
				str.value.u64 = strtoull(pReadPos, &pReadPos, 8);
				break;
			case JVTDouble:
				str.value.dbl = strtod(pReadPos, &pReadPos);
				break;
			}

			if (pReadPos >= m_pMemEnd)
			{
				m_iErr = kErrorEnd;
				return 0;
			}			
		}

		str.nLength = (pEndPos ? pEndPos : pReadPos) - pStart;
		if (m_bQuoteStart)
		{
			str.nLength --;
			pReadPos ++;
		}

		return pReadPos;
	}

	//��ȡLua�����ַ���
	char* fetchLuaString(char* pReadPos, uint32_t eqSyms, JSONString& str)
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
} ;

#endif