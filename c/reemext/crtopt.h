#ifndef _REEME_CRTOPT_H__
#define _REEME_CRTOPT_H__

// itoa函数优化
extern size_t opt_u32toa(uint32_t value, char* buffer);
extern size_t opt_u64toa(uint64_t value, char* buffer);

static inline size_t opt_i32toa(int32_t value, char* buffer)
{
	uint32_t u = static_cast<uint32_t>(value);
	if (value < 0)
	{
		*buffer ++ = '-';
		u = ~u + 1;
	}
	return opt_u32toa(u, buffer);
}

static inline size_t opt_i64toa(int64_t value, char* buffer)
{
	uint64_t u = static_cast<uint64_t>(value);
	if (value < 0)
	{
		*buffer ++ = '-';
		u = ~u + 1;
	}

	return opt_u64toa(u, buffer);
}

static size_t opt_u32toa_hex(uint32_t value, char* dst, bool useUpperCase = 1)
{	
	const char upperChars[] = { "0123456789ABCDEF" };
	const char lowerChars[] = { "0123456789abcdef" };
		
	char* buffer = dst;
	uint32_t endt = 32, i;
	uint32_t mask = 0xF0000000;

	do
	{
		if (value & mask)
			break;
		mask >>= 4;
		endt -= 4;
	} while (mask != 0);

	if (useUpperCase)
	{
		for (i = 0; i < endt; i += 4)
			*buffer ++ = upperChars[value >> i & 0xF];
	}
	else
	{
		for (i = 0; i < endt; i += 4)
			*buffer ++ = lowerChars[value >> i & 0xF];
	}

	return buffer - dst;
}

static size_t opt_u64toa_hex(uint64_t value, char* dst, bool useUpperCase = 1)
{
	const char upperChars[] = { "0123456789ABCDEF" };
	const char lowerChars[] = { "0123456789abcdef" };

	char* buffer = dst;
	uint32_t endt = 32, i;
	uint64_t mask = 0xF000000000000000;

	do
	{
		if (value & mask)
			break;
		mask >>= 4;
		endt -= 4;
	} while (mask != 0);

	if (useUpperCase)
	{
		for (i = 0; i < endt; i += 4)
			*buffer ++ = upperChars[value >> i & 0xF];
	}
	else
	{
		for (i = 0; i < endt; i += 4)
			*buffer ++ = lowerChars[value >> i & 0xF];
	}

	return buffer - dst;
}

//////////////////////////////////////////////////////////////////////////
// dtoa函数优化
extern size_t opt_dtoa(double value, char* buffer);

#endif