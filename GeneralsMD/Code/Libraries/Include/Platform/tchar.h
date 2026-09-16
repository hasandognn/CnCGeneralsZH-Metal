/* tchar.h - the MSVC TCHAR mapping, for the ten files in this tree that use it.
**
** TCHAR existed so one source could build as either 8-bit or UTF-16.  Nothing here builds both
** ways: every caller is the narrow build, which is what _MBCS/_UNICODE being undefined meant on
** Windows too.  So this maps the handful of spellings onto their narrow originals and stops.
*/

#ifndef WIN32COMPAT_TCHAR_H
#define WIN32COMPAT_TCHAR_H

#include <cstring>
#include <cstdio>

typedef char TCHAR;
typedef char _TCHAR;

#ifndef _T
#define _T(x)    x
#endif
#ifndef _TEXT
#define _TEXT(x) x
#endif

#define _tcslen   strlen
#define _tcscpy   strcpy
#define _tcsncpy  strncpy
#define _tcscat   strcat
#define _tcscmp   strcmp
#define _tcsicmp  strcasecmp
#define _tcsncmp  strncmp
#define _tcsnicmp strncasecmp
#define _tcschr   strchr
#define _tcsrchr  strrchr
#define _tcsstr   strstr
#define _stprintf sprintf
#define _tprintf  printf
#define _vstprintf vsprintf
#define _ftprintf fprintf

#define _tcsclen  strlen

/* WCHAR is UTF-16 (see Win32Compat.h), and wchar_t on this platform is 32 bits, so the C library's
** wcslen and friends are the wrong functions for it - they would walk the string at the wrong
** stride.  These overloads take the 16-bit spelling, so the call sites keep the names they had. */

#include "Platform/Win32Compat.h"

inline size_t wcslen(const WCHAR * s)
{
	size_t n = 0;
	while (s != nullptr && s[n] != 0) ++n;
	return n;
}

inline WCHAR * wcscpy(WCHAR * dst, const WCHAR * src)
{
	WCHAR * out = dst;
	while ((*out++ = *src++) != 0) { }
	return dst;
}

inline int wcscmp(const WCHAR * a, const WCHAR * b)
{
	while (*a != 0 && *a == *b) { ++a; ++b; }
	return (int)*a - (int)*b;
}

#endif // WIN32COMPAT_TCHAR_H
