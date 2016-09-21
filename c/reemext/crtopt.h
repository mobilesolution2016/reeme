#ifndef _REEME_CRTOPT_H__
#define _REEME_CRTOPT_H__

// itoa函数优化
REEME_API size_t opt_u32toa(uint32_t value, char* buffer);
REEME_API size_t opt_u64toa(uint64_t value, char* buffer);

REEME_API size_t opt_i32toa(int32_t value, char* buffer);
REEME_API size_t opt_i64toa(int64_t value, char* buffer);

// 16进制整数转字符串
REEME_API size_t opt_u32toa_hex(uint32_t value, char* dst, bool useUpperCase = true);
REEME_API size_t opt_u64toa_hex(uint64_t value, char* dst, bool useUpperCase = true);

//////////////////////////////////////////////////////////////////////////
// dtoa函数优化
REEME_API size_t opt_dtoa(double value, char* buffer);

//////////////////////////////////////////////////////////////////////////
REEME_API size_t opt_stristr(const char* str, size_t strLeng, const char* find, size_t findLeng);

#endif