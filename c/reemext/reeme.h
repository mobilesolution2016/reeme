#ifndef _REEME_EXTC_REEME_H__
#define _REEME_EXTC_REEME_H__

#include "preheader.h"

#define LUA_TCDATA 10

enum LuaRegistryFixedValue {
	kLuaRegVal_FFINew = -99990,
	kLuaRegVal_FFISizeof,
	kLuaRegVal_tostring,
	kLuaRegVal_ngx_re_match,
};

//////////////////////////////////////////////////////////////////////////
template <typename T> static inline T hashString(const char *str)
{
	const T seed = 131;

	char ch;
	T hash = 0;
	while ((ch = *str ++) != 0)
		hash = hash * seed + ch;

	return hash;
}

template <typename T> static inline T hashString(const char *str, size_t len)
{
	const T seed = 131;

	T hash = 0;
	for(size_t i = 0; i < len; ++ i)
		hash = hash * seed + str[i];

	return hash;
}

template <typename T> static inline T alignbytes(T v)
{
#ifdef REEME_64
	return v + 7 >> 3 << 3;
#else
	return v + 3 >> 2 << 2;
#endif
}

union double2int
{
	double	dval;
	int32_t	i32;
} ;

static inline int32_t dtoi(double val)
{
	double2int u;
	u.dval = val + 6755399441055744.0;
	return u.i32;
}

// 内存池
template <typename T, size_t BlockSize = 4096> class TMemoryPool
{
public:
	typedef T               value_type;
	typedef T*              pointer;
	typedef T&              reference;
	typedef const T*        const_pointer;
	typedef const T&        const_reference;
	typedef size_t          size_type;
	typedef intptr_t        difference_type;

	template <typename U> struct rebind {
		typedef TMemoryPool<U> other;
	};

	TMemoryPool() throw()
	{
		fixedBlock_ = 0;
		currentBlock_ = 0;
		currentSlot_ = 0;
		lastSlot_ = 0;
		freeSlots_ = 0;
		slotsCount_ = 0;
	}
	TMemoryPool(const TMemoryPool& memoryPool) throw()
	{
		TMemoryPool();
	}
	template <typename U, size_t N> TMemoryPool(const TMemoryPool<U, N>& memoryPool) throw()
	{
		TMemoryPool();
	}

	inline ~TMemoryPool() throw()
	{
		freeall();
	}

	void reset()
	{
		freeall();

		if (fixedBlock_)
			allocateBlock(fixedBlock_);
	}

	void initFixed(void* fixed)
	{
		fixedBlock_ = (data_pointer_)fixed;
		allocateBlock(fixedBlock_);
	}

	pointer address(reference x) const throw() { return &x; }
	const_pointer address(const_reference x) const throw() { return &x; }

	void construct(pointer _p, const T& _val)
	{
		::new((void *)_p) T(_val);
	}
	void destroy(pointer _p)
	{
		_p->~T();
	}

	inline pointer allocate(size_t cc = 1)
	{
		assert(cc == 1);

		slotsCount_ ++;
		if (freeSlots_ != 0) {
			pointer result = reinterpret_cast<pointer>(freeSlots_);
			freeSlots_ = freeSlots_->next;
			return result;
		}

		if (currentSlot_ >= lastSlot_)
			allocateBlock();
		return reinterpret_cast<pointer>(currentSlot_++);
	}
	inline void deallocate(pointer p, size_type cc = 0)
	{
		if (p != 0) {
			slotsCount_ --;
			reinterpret_cast<slot_pointer_>(p)->next = freeSlots_;
			freeSlots_ = reinterpret_cast<slot_pointer_>(p);
		}
	}

	inline size_type count() const throw() { return slotsCount_; }
	inline size_type max_size() const throw()
	{
		size_type maxBlocks = -1 / BlockSize;
		return (BlockSize - sizeof(data_pointer_)) / sizeof(slot_type_) * maxBlocks;
	}

	inline pointer newElement()
	{
		pointer result = allocate();
		new (result)value_type();
		return result;
	}
	inline void deleteElement(pointer p)
	{
		if (p != 0) {
			p->~value_type();
			deallocate(p);
		}
	}

private:
	union Slot_ {
		char objspace[sizeof(value_type)];
		Slot_* next;
	};

	typedef char* data_pointer_;
	typedef Slot_ slot_type_;
	typedef Slot_* slot_pointer_;

	data_pointer_ fixedBlock_;
	slot_pointer_ currentBlock_;
	slot_pointer_ currentSlot_;
	slot_pointer_ lastSlot_;
	slot_pointer_ freeSlots_;
	size_type	  slotsCount_;

	size_type padPointer(data_pointer_ p, size_type align) const throw()
	{
		size_t result = reinterpret_cast<size_t>(p);
		return ((align - result) % align);
	}

	void allocateBlock(data_pointer_ newBlock = 0)
	{
		if (!newBlock)
			newBlock = reinterpret_cast<data_pointer_>(operator new(BlockSize));

		reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
		currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);

		data_pointer_ body = newBlock + sizeof(slot_pointer_);
		size_type bodyPadding = padPointer(body, sizeof(slot_type_));
		currentSlot_ = reinterpret_cast<slot_pointer_>(body + bodyPadding);
		lastSlot_ = reinterpret_cast<slot_pointer_>(newBlock + BlockSize - sizeof(slot_type_) + 1);
	}

	void freeall()
	{
		slot_pointer_ curr = currentBlock_;
		while (curr != 0) {
			slot_pointer_ prev = curr->next;
			if (curr != reinterpret_cast<slot_pointer_>(fixedBlock_))
				operator delete(reinterpret_cast<void*>(curr));
			curr = prev;
		}

		currentBlock_ = 0;
		currentSlot_ = 0;
		lastSlot_ = 0;
		freeSlots_ = 0;
		slotsCount_ = 0;
	}
};

template <class T1, class T2> bool operator == (const TMemoryPool<T1>&, const TMemoryPool<T2>&) throw()
{
	return true;
}

template <class T1, class T2> bool operator != (const TMemoryPool<T1>&, const TMemoryPool<T2>&) throw()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
// 指针字符串Key
struct StringPtrKey
{
	const char*		pString;
	size_t			nHashID;

	inline StringPtrKey()
		: pString(0)
		, nHashID(0)
	{
	}
	inline StringPtrKey(const char* name)
		: pString(name)
		, nHashID(hashString<size_t>(name))
	{
	}
	inline StringPtrKey(const std::string& name)
	{
		pString = name.c_str();
		nHashID = hashString<size_t>(pString);
	}
	inline StringPtrKey(const char* name, size_t len)
		: pString(name)
		, nHashID(hashString<size_t>(name, len))
	{
	}

	inline operator size_t () const { return nHashID; }
	inline operator const char* () const { return pString; }
	inline void reset() { pString = 0; nHashID = 0; }
	inline const StringPtrKey& operator = (const char* s) { pString = s; nHashID = 0; return *this; }
	inline bool operator == (const StringPtrKey& key) const { return nHashID == key.nHashID && strcmp(pString, key.pString) == 0; }
	inline bool operator == (const std::string& str) const { return strcmp(pString, str.c_str()) == 0; }

	inline void copyto(std::string& str) const
	{
		str = pString;
	}
};
inline size_t hash_value(const StringPtrKey& k)
{
	if (k.nHashID == 0)
		return hashString<size_t>(k.pString);
	return k.nHashID;
}
static inline bool operator < (const StringPtrKey& s1, const StringPtrKey& s2)
{
	return strcmp(s1.pString, s2.pString) < 0;
}
static inline bool operator > (const StringPtrKey& s1, const StringPtrKey& s2)
{
	return strcmp(s1.pString, s2.pString) > 0;
}
static inline int compare(const StringPtrKey& s1, const StringPtrKey& s2)
{
	return strcmp(s1.pString, s2.pString);
}

//////////////////////////////////////////////////////////////////////////
struct StringPtrKeyL
{
	const char*		pString;
	size_t			nHashID, nLength;

	inline StringPtrKeyL()
		: pString(0)
		, nHashID(0)
	{
	}
	inline StringPtrKeyL(const char* name, size_t leng)
		: pString(name)
		, nHashID(hashString<size_t>(name))
		, nLength(leng)
	{
	}
	inline StringPtrKeyL(const std::string& name)
	{
		pString = name.c_str();
		nLength = name.length();
		nHashID = hashString<size_t>(pString);
	}
	inline StringPtrKeyL(const char* name, size_t len, size_t leng)
		: pString(name)
		, nLength(leng)
		, nHashID(hashString<size_t>(name, len))
	{
	}

	inline operator size_t () const { return nHashID; }
	inline operator const char* () const { return pString; }
	inline void reset() { pString = 0; nHashID = nLength = 0; }
	inline size_t length() const { return nLength; }
	inline const StringPtrKeyL& operator = (const char* s) { pString = s; nHashID = hashString<size_t>(s); nLength = strlen(s); return *this; }
	inline bool operator == (const StringPtrKeyL& key) const { return nHashID == key.nHashID && strncmp(pString, key.pString, nLength) == 0; }
	inline bool operator == (const std::string& str) const { return nLength == str.length() && strncmp(pString, str.c_str(), nLength) == 0; }

	inline void copyto(std::string& str)
	{
		str.append(pString, nLength);
	}
};
inline size_t hash_value(const StringPtrKeyL& k)
{
	if (k.nHashID == 0)
		return hashString<size_t>(k.pString, k.nLength);
	return k.nHashID;
}
static inline bool operator < (const StringPtrKeyL& s1, const StringPtrKeyL& s2)
{
	return s1.nLength < s2.nLength || strncmp(s1.pString, s2.pString, s1.nLength) < 0;
}
static inline bool operator > (const StringPtrKeyL& s1, const StringPtrKeyL& s2)
{
	return s1.nLength > s2.nLength || strncmp(s1.pString, s2.pString, s1.nLength) > 0;
}
static inline int compare(const StringPtrKeyL& s1, const StringPtrKeyL& s2)
{
	if (s1.nLength < s2.nLength) return -1;
	if (s1.nLength > s2.nLength) return 1;
	return strncmp(s1.pString, s2.pString, s1.nLength);
}


//////////////////////////////////////////////////////////////////////////
template <typename T> class TList;

template <typename T> class TListNode
{
	friend class TList<T>;
protected:
	typedef TListNode<T> Node;
	typedef TList<T> List;

	List			*m_pOwningList;
	Node			*m_pNext, *m_pPrevious;

public:
	inline TListNode()
		: m_pNext(NULL), m_pPrevious(NULL), m_pOwningList(NULL)
	{}

	inline T* next() const { return static_cast<T*>(m_pNext); }
	inline T* previous() const { return static_cast<T*>(m_pPrevious); }
} ;

template <typename T> class TList
{
protected:
	typedef TListNode<T> Node;

	Node			*m_pFirstNode, *m_pLastNode;
	size_t			m_nodesCount;

public:
	inline TList()
		: m_pFirstNode(NULL), m_pLastNode(NULL), m_nodesCount(0)
	{}

	void prepend(T* p)
	{
		assert(p);

		Node* n = static_cast<Node*>(p);
		assert(n->m_pOwningList == 0);

		if (m_pFirstNode)
		{
			m_pFirstNode->m_pPrevious = n;
			n->m_pNext = m_pFirstNode;
			m_pFirstNode = n;
		}
		else
		{
			m_pFirstNode = m_pLastNode = n;
			n->m_pNext = NULL;
		}

		n->m_pPrevious = NULL;
		n->m_pOwningList = this;
		m_nodesCount ++;
	}

	bool append(T* node)
	{
		Node* n = static_cast<Node*>(node);
		if (!n || n->m_pOwningList)
			return false;

		if (m_pLastNode)
		{
			m_pLastNode->m_pNext = n;
			n->m_pPrevious = m_pLastNode;
			m_pLastNode = n;
		}
		else
		{
			m_pFirstNode = m_pLastNode = n;
			n->m_pPrevious = NULL;
		}

		n->m_pNext = NULL;
		n->m_pOwningList = this;

		m_nodesCount ++;
		return true;
	}

	bool append(TList<T>& list)
	{
		m_nodesCount += list.size();

		Node* n = list.m_pFirstNode, *nn;
		while(n)
		{
			nn = n->m_pNext;

			if (m_pLastNode)
			{
				m_pLastNode->m_pNext = n;
				n->m_pPrevious = m_pLastNode;
				m_pLastNode = n;
			}
			else
			{
				m_pFirstNode = m_pLastNode = n;
				n->m_pPrevious = NULL;
			}

			n->m_pNext = NULL;
			n->m_pOwningList = this;

			n = nn;
		}

		list.m_pFirstNode = list.m_pLastNode = NULL;
		list.m_nodesCount = 0;

		return true;
	}

	bool remove(T* node)
	{
		Node* n = static_cast<Node*>(node);
		if (!n || n->m_pOwningList != this)
			return false;

		Node* pprev = n->m_pPrevious, *nnext = n->m_pNext;

		if (pprev)
			pprev->m_pNext = nnext;
		if (nnext)
			nnext->m_pPrevious = pprev;

		if (n == m_pFirstNode)
			m_pFirstNode = nnext;
		if (n == m_pLastNode)
			m_pLastNode = pprev;

		n->m_pPrevious = n->m_pNext = NULL;
		n->m_pOwningList = NULL;
		m_nodesCount --;

		return true;
	}

	void clear()
	{
		m_pFirstNode = m_pLastNode = 0;
		m_nodesCount = 0;
	}

	T* popFirst()
	{
		Node* n = m_pFirstNode;
		if (n)
		{
			m_pFirstNode = n->m_pNext;
			if (m_pFirstNode)
				m_pFirstNode->m_pPrevious = 0;
			if (m_pLastNode == n)
				m_pLastNode = m_pFirstNode;

			n->m_pPrevious = n->m_pNext = NULL;
			n->m_pOwningList = NULL;
			m_nodesCount --;
		}

		return static_cast<T*>(n);
	}

	T* popLast()
	{
		Node* pNode = m_pLastNode;
		if (pNode)
		{
			m_pLastNode = pNode->m_pPrevious;
			if (m_pLastNode)
				m_pLastNode->m_pNext = 0;
			if (m_pFirstNode == pNode)
				m_pFirstNode = m_pLastNode;

			pNode->m_pPrevious = pNode->m_pNext = NULL;
			pNode->m_pOwningList = NULL;

			m_nodesCount --;
		}

		return static_cast<T*>(pNode);
	}

	void insertBefore(T *p, T *before)
	{
		assert(p);

		Node* n = static_cast<Node*>(p);
		assert(n->m_pOwningList == 0);

		if (before)
		{
			assert(before->m_pOwningList == this);

			n->m_pNext = before;
			Node* after = ((Node*)before)->m_pPrevious;
			n->m_pPrevious = after;
			if (after) after->m_pNext = n;
			else m_pFirstNode = n;
			((Node*)before)->m_pPrevious = n;
			n->m_pOwningList = this;

			m_nodesCount ++;
		}
		else
		{
			prepend(p);
		}
	}

	void insertAfter(T *p, T *after)
	{
		assert(p);

		Node* n = static_cast<Node*>(p);
		assert(n->m_pOwningList == 0);

		if (after)
		{
			assert(after->m_pOwningList == this);

			n->m_pPrevious = after;
			Node* before = ((Node*)after)->m_pNext;
			n->m_pNext = before;
			if (before) before->m_pPrevious = n;
			else m_pLastNode = n;
			((Node*)after)->m_pNext = n;
			n->m_pOwningList = this;

			m_nodesCount ++;
		}
		else
		{
			append(p);
		}
	}

	inline T* first() const { return static_cast<T*>(m_pFirstNode); }
	inline T* last() const { return static_cast<T*>(m_pLastNode); }

	inline size_t size() const { return m_nodesCount; }
};

//////////////////////////////////////////////////////////////////////////
class TMemNode : public TListNode<TMemNode>
{
public:
	size_t		used;
	size_t		total;

	inline operator char* () { return (char*)(this + 1); }
	inline operator const char* () { return (const char*)(this + 1); }

	inline operator void* () { return (void*)(this + 1); }
	inline operator const void* () { return (const void*)(this + 1); }
};

const size_t TMEMNODESIZE = 8192 - sizeof(TMemNode);

class TMemList : public TList<TMemNode>
{
public:
	inline TMemList() {}
	~TMemList()
	{
		TMemNode* n;
		while((n = popFirst()) != 0)
			free(n);
	}

	TMemNode* newNode(size_t size = TMEMNODESIZE)
	{
		TMemNode* n = (TMemNode*)malloc(size + sizeof(TMemNode));
		new (n) TMemNode();
		n->total = size;
		n->used = 0;

		append(n);
		return n;
	}
	TMemNode* wrapNode(char* buf, size_t fixedBufSize)
	{
		assert(fixedBufSize >= sizeof(TMemNode) + 16);

		TMemNode* n = (TMemNode*)buf;
		new (n) TMemNode();
		n->total = fixedBufSize - sizeof(TMemNode);
		n->used = 0;

		append(n);
		return n;
	}
	void addChar(char ch)
	{
		TMemNode* n = (TMemNode*)m_pLastNode;
		if (n->used >= n->total)
			n = newNode();

		char* ptr = (char*)(n + 1);
		ptr[n->used ++] = ch;
	}
	void addChar2(char ch1, char ch2)
	{
		TMemNode* n = (TMemNode*)m_pLastNode;
		if (n->used + 1 >= n->total)
			n = newNode();

		char* ptr = (char*)(n + 1);
		size_t used = n->used;
		ptr[used] = ch1;
		ptr[used + 1] = ch2;
		n->used += 2;
	}
	void addString(const char* s, size_t len)
	{
		TMemNode* n = (TMemNode*)m_pLastNode;
		char* ptr = (char*)(n + 1);

		size_t copy = std::min(n->total - n->used, len);
		memcpy(ptr + n->used, s, copy);
		len -= copy;
		n->used += copy;

		if (len > 0)
		{
			// 剩下的直接一次分配够
			n = newNode(std::max(TMEMNODESIZE, len));
			ptr = (char*)(n + 1);

			memcpy(ptr + n->used, s + copy, len);
			n->used += len;
		}
	}
	char* reserve(size_t len)
	{
		char* ptr;
		TMemNode* n = (TMemNode*)m_pLastNode;

		// 最后一个节点剩下的空间不够需要保留的话，就直接分配一个全新的至少有len这么大的
		if (n->used + len < n->total)
		{
			ptr = (char*)(n + 1);
			ptr += n->used;
		}
		else
		{
			n = newNode(std::max(TMEMNODESIZE, len));
			ptr = (char*)(n + 1);
		}

		n->used += len;
		return ptr;
	}

	char* joinAll(size_t* pnTotal = 0)
	{
		size_t total = 0;
		TMemNode* n = first();

		while (n)
		{
			total += n->used;
			n = n->next();
		}

		char* dst = (char*)malloc(total), *ptr = dst;
		while ((n = popFirst()) != NULL)
		{
			memcpy(ptr, (char*)(n + 1), n->used);
			ptr += n->used;
			free(n);
		}

		if (pnTotal)
			*pnTotal = total;
		return dst;
	}
} ;

//////////////////////////////////////////////////////////////////////////
//自动处理对齐的内存块读写器
template <typename AlignCheck> class MemoryBlockRW
{
public:
	enum AlignBytes
	{
		kAlign1Bytes = 1,
		kAlign2Bytes = 2,
		kAlign4Bytes = 4,
		kAlign8Bytes = 8
	};

public:
	MemoryBlockRW()
		: pMemory(0), pMemoryBegin(0), nTotal(0)
	{}
	MemoryBlockRW(void* mem, size_t size)
		: pMemory((char*)mem), pMemoryBegin((char*)mem), nTotal(size)
	{}

	//!写入
	template <typename T> void writeval(const T& val)
	{
		AlignCheck chk;
		size_t offset = pMemory - pMemoryBegin;
		assert(offset + sizeof(T) <= nTotal);

		if (chk(offset, sizeof(T)))
			*(T*)pMemory = val;
		else
			memcpy(pMemory, &val, sizeof(T));
		pMemory += sizeof(T);
	}
	template <typename T> void writeval(const T& val, size_t pos)
	{
		AlignCheck chk;
		assert(pos + sizeof(T) <= nTotal);

		if (chk(pos, sizeof(T)))
			*(T*)(pMemoryBegin + pos) = val;
		else
			memcpy(pMemoryBegin + pos, &val, sizeof(T));
	}

	//!写入任意字节数
	inline void write(const void* src, size_t n)
	{
		assert(pMemory - pMemoryBegin + n <= nTotal);
		memcpy(pMemory, src, n);
		pMemory += n;
	}
	inline void write(const void* src, size_t n, size_t pos)
	{
		assert(pos + n <= nTotal);
		memcpy(pMemoryBegin + pos, src, n);
	}
	//!填充0字节
	inline void fillzero(size_t n)
	{
		if (n)
		{
			assert(pMemory - pMemoryBegin + n <= nTotal);
			memset(pMemory, 0, n);
			pMemory += n;
		}
	}

	//!读出
	template <typename T> void readval(T& val)
	{
		AlignCheck chk;
		size_t offset = pMemory - pMemoryBegin;
		assert(offset + sizeof(T) <= nTotal);

		if (chk(offset, sizeof(T)))
			val = *(T*)pMemory;
		else
			memcpy(&val, pMemory, sizeof(T));
		pMemory += sizeof(T);
	}
	template <typename T> void readval(T& val, size_t pos)
	{
		AlignCheck chk;
		assert(pos + sizeof(T) <= nTotal);

		if (chk(pos, sizeof(T)))
			val = *(T*)(pMemoryBegin + pos);
		else
			memcpy(&val, pMemoryBegin + pos, sizeof(T));
	}
	template <typename T> T readval()
	{
		T v;
		readval(v);
		return v;
	}
	template <typename T> T readval(size_t pos)
	{
		T v;
		readval(v, pos);
		return v;
	}

	//!读出做生意字节数
	inline void read(void* dst, size_t n)
	{
		assert(pMemory - pMemoryBegin + n <= nTotal);
		memcpy(dst, pMemory, n);
		pMemory += n;
	}
	inline void read(void* dst, size_t n, size_t pos)
	{
		assert(pos + n <= nTotal);
		memcpy(dst, pMemoryBegin + pos, n);
	}

	//!获取当前位置
	inline char* get() { return pMemory; }
	inline operator void* () { return pMemory; }
	inline operator char* () { return pMemory; }
	inline operator unsigned char* () { return pMemory; }
	inline operator const void* () { return pMemory; }
	inline operator const char* () { return pMemory; }
	inline operator const unsigned char* () { return pMemory; }

	//!获取已写入的字节数
	inline size_t size() const { return pMemory - pMemoryBegin; }

	//!移动位置
	inline void move(size_t n) { pMemory += n; }

public:
	char	*pMemory, *pMemoryBegin;
	size_t	nTotal;
};

struct MemoryAlignCheck1 {
	inline bool operator ()(size_t i, size_t n)
	{
		return false;
	}
};
struct MemoryAlignCheck2 {
	inline bool operator ()(size_t i, size_t n)
	{
		return ((i & 1) == 0 && n == 2);
	}
};
struct MemoryAlignCheck4 {
	inline bool operator ()(size_t i, size_t n)
	{
		return ((i & 3) == 0 && n == 4);
	}
};
struct MemoryAlignCheck8 {
	inline bool operator ()(size_t i, size_t n)
	{
		return ((i & 7) == 0 && n == 8);
	}
};

//////////////////////////////////////////////////////////////////////////
#define TSORT_CUTOFF 8

template <typename T> struct defswap
{
	void operator () (char *a, char *b)
	{
		char tmp;
		if (a != b)
		{
			uint32_t width = sizeof(T);
			while (width -- > 0)
			{
				tmp = *a;
				*a ++ = *b;
				*b ++ = tmp;
			}
		}
	}
};

template <typename T> struct structswap
{
	char		tmp[sizeof(T)];

	void operator () (char *a, char *b)
	{
		if (a != b)
		{
			memcpy(tmp, a, sizeof(T));
			memcpy(a, b, sizeof(T));
			memcpy(b, tmp, sizeof(T));
		}
	}
};

template <typename T> struct greater
{
	int32_t operator () (const T& a, const T& b) { return a - b; }
};
template <typename T> struct less
{
	int32_t operator () (const T& a, const T& b) { return -(a - b); }
};

template <typename T, class fnComper, class fnSwap>
void tsort(T *base, uint32_t num, fnComper comp, fnSwap s)
{
	char *lo, *hi;
	char *mid;
	char *l, *h;
	uint32_t size;
	char *lostk[30], *histk[30];
	int stkptr;

	if (num < 2)
		return;

	stkptr = 0;

	lo = (char*)base;
	hi = (char *)base + sizeof(T) * (num - 1);
recurse:
	size = (hi - lo) / sizeof(T) + 1;

	if (size <= TSORT_CUTOFF)
	{
		char *p, *max;
		char *phi = hi, *plo = lo;

		while (phi > plo)
		{
			max = plo;
			for (p = plo + sizeof(T); p <= phi; p += sizeof(T))
			{
				if (comp(*((const T*)p), *((const T*)max)) > 0)
					max = p;
			}
			s(max, phi);
			phi -= sizeof(T);
		}
	}
	else
	{
		mid = lo + (size / 2) * sizeof(T);
		s(mid, lo);

		l = lo;
		h = hi + sizeof(T);

		for (;;)
		{
			do { l += sizeof(T); } while (l <= hi && comp(*((const T*)l), *((const T*)lo)) <= 0);
			do { h -= sizeof(T); } while (h > lo && comp(*((const T*)h), *((const T*)lo)) >= 0);
			if (h < l) break;
			s(l, h);
		}

		s(lo, h);

		if (h - 1 - lo >= hi - l)
		{
			if (lo + sizeof(T) < h)
			{
				lostk[stkptr] = lo;
				histk[stkptr] = h - sizeof(T);
				++ stkptr;
			}

			if (l < hi)
			{
				lo = l;
				goto recurse;
			}
		}
		else
		{
			if (l < hi)
			{
				lostk[stkptr] = l;
				histk[stkptr] = hi;
				++ stkptr;
			}

			if (lo + sizeof(T) < h)
			{
				hi = h - sizeof(T);
				goto recurse;
			}
		}
	}

	-- stkptr;
	if (stkptr >= 0)
	{
		lo = lostk[stkptr];
		hi = histk[stkptr];
		goto recurse;
	}
}

//////////////////////////////////////////////////////////////////////////

extern size_t ZLibCompress(const void* data, size_t size, char* outbuf, size_t outbufSize, int32_t level);
extern size_t ZLibDecompress(const void* data, size_t size, void* outmem, size_t outsize);
extern uint32_t CRC32Check(const void* data, size_t size);

#ifndef _MSC_VER
namespace std {
	template <> struct hash<StringPtrKey>
	{
		size_t operator()(StringPtrKey const& k) const
		{
			if (k.nHashID == 0)
				return hashString<size_t>(k.pString);
			return k.nHashID;
		}
	};

	template <> struct hash<StringPtrKeyL>
	{
		size_t operator()(StringPtrKeyL const& k) const
		{
			if (k.nHashID == 0)
				return hashString<size_t>(k.pString);
			return k.nHashID;
		}
	};
}
#endif

#endif
