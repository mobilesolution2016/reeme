#ifndef _REEME_EXTC_PREHEADER_H__
#define _REEME_EXTC_PREHEADER_H__

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <memory.h>

#include <algorithm>
#include <string>
#include <vector>
#include <list>

#ifdef _WINDOWS
#	include <windows.h>
#	include <boost/unordered_map.hpp>
#	include <boost/unordered_set.hpp>

#	undef min
#	undef max

#	define MAP_CLASS_NAME boost::unordered_map
#	define SET_CLASS_NAME boost::unordered_set

#	define REEME_API extern "C" __declspec(dllexport)

// ÐÞ¸´VS2015±àÒëÎÊÌâ
#if _MSC_VER >= 1900
#	define STDC99
#else
#	define snprintf _snprintf
#endif

#elif defined(__GUNC__) || defined(__unix__)
#   include <sys/time.h>
#	include <unordered_map>
#	include <unordered_set>

#	define MAP_CLASS_NAME std::unordered_map
#	define SET_CLASS_NAME std::unordered_set

#	define REEME_API extern "C" __attribute__ ((visibility("default")))

#	ifndef offsetof
#		define offsetof(s, m) ((size_t)(&reinterpret_cast<s*>(100000)->m) - 100000)
#	endif

#	define stricmp strcasecmp
#	define strnicmp strncasecmp
#	define strtok_s strtok_r

#	define BOOL int32_t
#	define TRUE 1
#	define FALSE 0

#endif	// _WINDOWS

#if defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#	define REEME_64 1
#endif

#endif