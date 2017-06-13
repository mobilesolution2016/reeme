

#define MAJOR_VERSION 3
#define MINOR_VERSION 4
#define MICRO_VERSION 4
#define VERSION "3.4.4"

#define static static

#if defined(_WIN32) || defined(_WIN64)
  //#define snprintf _snprintf
  //#define vsnprintf _vsnprintf
  //#define strdup _strdup
  #define strcasecmp _stricmp
  #define strncasecmp _strnicmp
#endif