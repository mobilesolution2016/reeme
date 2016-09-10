#ifndef _REEME_CRTOPT_H__
#define _REEME_CRTOPT_H__

// itoa函数优化
extern void opt_u32toa(uint32_t value, char* buffer);
extern void opt_u64toa(uint64_t value, char* buffer);

static inline void opt_i32toa(uint32_t value, char* buffer)
{
	uint32_t u = static_cast<uint32_t>(value);
	if (value < 0)
	{
		*buffer ++ = '-';
		u = ~u + 1;
	}
	opt_u32toa(u, buffer);
}

static inline void opt_i64toa(int64_t value, char* buffer)
{
	uint64_t u = static_cast<uint64_t>(value);
	if (value < 0)
	{
		*buffer ++ = '-';
		u = ~u + 1;
	}

	opt_u64toa(u, buffer);
}

//////////////////////////////////////////////////////////////////////////
// dtoa函数优化
extern void opt_dtoa(double value, char* buffer);

#endif