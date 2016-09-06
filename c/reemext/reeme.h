#ifndef _REEME_EXTC_REEME_H__
#define _REEME_EXTC_REEME_H__

#include "preheader.h"

#define LUA_TCDATA 10

class DBuffer;

template <typename T> static inline T hashString(const char *str)
{
	const T seed = 131;

	char ch;
	T hash = 0;
	while ((ch = *str) ++ != 0)
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
	int32_t	ivals[2];
} ;

static inline int32_t dtoi(double val)
{
	double2int u;
	u.dval = val + 6755399441055744.0;
	return u.ivals[0];
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
	template <class U> TMemoryPool(const TMemoryPool<U>& memoryPool) throw()
	{
		TMemoryPool();
	}

	inline ~TMemoryPool() throw()
	{
		freeall();
	}

	void freeall()
	{
		slot_pointer_ curr = currentBlock_;
		while (curr != 0) {
			slot_pointer_ prev = curr->next;
			operator delete(reinterpret_cast<void*>(curr));
			curr = prev;
		}

		currentBlock_ = 0;
		currentSlot_ = 0;
		lastSlot_ = 0;
		freeSlots_ = 0;
		slotsCount_ = 0;
	}

	pointer address(reference x) const throw() { return &x; }
	const_pointer address(const_reference x) const throw() { return &x; }

	inline pointer allocate(size_type n = 1, const_pointer hint = 0)
	{
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
	inline void deallocate(pointer p, size_type n = 1)
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
	void allocateBlock()
	{
		data_pointer_ newBlock = reinterpret_cast<data_pointer_>(operator new(BlockSize));
		reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
		currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);

		data_pointer_ body = newBlock + sizeof(slot_pointer_);
		size_type bodyPadding = padPointer(body, sizeof(slot_type_));
		currentSlot_ = reinterpret_cast<slot_pointer_>(body + bodyPadding);
		lastSlot_ = reinterpret_cast<slot_pointer_>(newBlock + BlockSize - sizeof(slot_type_) + 1);
	}
};

//////////////////////////////////////////////////////////////////////////
// 指针字符串Key
struct StringPtrKey
{
	const char*		pString;
	size_t			nHashID, nLength;

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
	inline void reset() { pString = 0; nHashID = nLength = 0; }
	inline const StringPtrKey& operator = (const char* s) { pString = s; nHashID = 0; return *this; }
	inline bool operator == (const StringPtrKey& key) const { return nHashID == key.nHashID && strcmp(pString, key.pString) == 0; }
	inline bool operator == (const std::string& str) const { return strcmp(pString, str.c_str()) == 0; }

	inline void copyto(std::string& str) const
	{
		str.append(pString, nLength);
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
template <typename T> class TList;

template <typename T> class TListNode
{
	friend class TList<T>;
private:
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
private:
	typedef TListNode<T> Node;

	Node			*m_pFirstNode, *m_pLastNode;
	size_t			m_nodesCount;

public:
	inline TList()
		: m_pFirstNode(NULL), m_pLastNode(NULL), m_nodesCount(0)
	{}

	void append(T* node)
	{
		Node* n = static_cast<Node*>(node);
		if (!n || n->m_pOwningList)
			return ;

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

		m_nodesCount ++;
	}

	void remove(T* node)
	{
		Node* n = static_cast<Node*>(node);
		if (!n || n->m_pOwningList != this)
			return ;

		Node* pprev = n->m_pPrevious, *nnext = n->m_pNext;

		pprev->m_pNext = nnext;
		nnext->m_pPrevious = pprev;

		if (pprev == m_pFirstNode)
			m_pFirstNode = nnext;
		if (nnext == m_pLastNode)
			m_pLastNode = pprev;

		n->m_pPrevious = n->m_pNext = NULL;
		n->m_pOwningList = NULL;
		m_nodesCount --;
	}

	T* popFirst()
	{
		Node* r = 0;
		if (m_pFirstNode)
		{
			r = m_pFirstNode;

			Node* nnext = r->m_pNext;
			m_pFirstNode = nnext;
			if (nnext)
				nnext->m_pPrevious = NULL;
			else
				m_pLastNode = 0;

			r->m_pPrevious = r->m_pNext = NULL;
			r->m_pOwningList = NULL;
			m_nodesCount --;
		}

		return static_cast<T*>(r);
	}

	inline T* first() const { return static_cast<T*>(m_pFirstNode); }
	inline T* last() const { return static_cast<T*>(m_pLastNode); }

	inline size_t size() const { return m_nodesCount; }
};

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


extern size_t ZLibCompress(const void* data, size_t size, char* outbuf, size_t outbufSize, int32_t level);
extern size_t ZLibDecompress(const void* data, size_t size, void* outmem, size_t outsize);


#ifndef _MSC_VER
namespace std {
	//ΪStringPtrKey��std::tr1�ṩHash����
	template <> struct hash<StringPtrKey>
	{
		size_t operator()(StringPtrKey const& k) const
		{
			if (k.nHashID == 0)
				return hashString<size_t>(k.pString);
			return k.nHashID;
		}
	};
}
#endif

#endif
