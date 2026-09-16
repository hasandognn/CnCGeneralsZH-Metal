/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* Win32Compat.h - the part of windows.h this game actually uses, on top of POSIX.
 *
 * 620 files in this tree name a Win32 type or call a Win32 function.  Rewriting them to call
 * POSIX directly would be 620 diffs against upstream and a merge conflict in every one of them.
 * Counting what they actually reach for gives a much smaller number: about forty functions, and
 * the heavy users are clocks.  timeGetTime is called from 284 places and QueryPerformanceCounter
 * from 132; after those come threads, events, critical sections, file i/o, module loading and the
 * registry, none of them above eighty.
 *
 * So this is a shim and not a rewrite.  It declares those forty functions and the types they need,
 * Win32Compat.cpp implements them on POSIX, and the files that call them are left alone.
 *
 * What it is not is a general Windows emulation.  Everything here is narrowed to the way this game
 * calls it: CreateFile takes the flags the game passes, the registry is backed by a plist because
 * the game only ever reads its own install path out of it, and anything outside that is missing on
 * purpose rather than guessed at.  Calls that cannot be answered honestly fail loudly through
 * Win32Compat_Unimplemented rather than returning a plausible zero, because a silent wrong answer
 * in a lockstep simulation is a desync an hour later.
 *
 * Wide characters are UTF-16 as Windows means them, so wchar_t is not used: it is 32 bits on this
 * platform and the game serialises these to disk.
 */

#ifndef WIN32COMPAT_H
#define WIN32COMPAT_H

#ifdef _WIN32
#error "Win32Compat.h is the off-Windows shim; on Windows include windows.h."
#endif

#include <cstddef>
#include <cstdint>
#include <cwchar>

// ---------------------------------------------------------------------------
// Integer and pointer types.
//
// LONG and DWORD stay 32 bits, as they are on Win64: the game writes them to save games and sends
// them over the wire, so their width is a file format and not a machine property.  The handle and
// pointer-sized types follow the machine.
// ---------------------------------------------------------------------------

typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef uint32_t            DWORD;
typedef int32_t             LONG;
typedef uint32_t            ULONG;
typedef int32_t             INT;
typedef uint32_t            UINT;
typedef int                 BOOL;
typedef unsigned char       UCHAR;
typedef char                CHAR;
typedef uint16_t            WCHAR;      // UTF-16, not wchar_t: wchar_t is 32 bits here.
typedef float               FLOAT;

typedef void *              LPVOID;
typedef const void *        LPCVOID;
typedef char *              LPSTR;
typedef const char *        LPCSTR;
typedef WCHAR *             LPWSTR;
typedef const WCHAR *       LPCWSTR;
typedef DWORD *             LPDWORD;
typedef BYTE *              LPBYTE;
typedef BOOL *              LPBOOL;

typedef intptr_t            INT_PTR;
typedef uintptr_t           UINT_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           ULONG_PTR;
typedef ULONG_PTR           DWORD_PTR;
typedef ULONG_PTR           SIZE_T;

typedef void *              HANDLE;
typedef HANDLE              HMODULE;
typedef HANDLE              HINSTANCE;
typedef HANDLE              HWND;
typedef HANDLE              HKEY;
typedef HKEY *              PHKEY;

typedef LONG_PTR            LPARAM;
typedef UINT_PTR            WPARAM;
typedef LONG_PTR            LRESULT;

typedef LONG                HRESULT;

/* MSVC keyword spellings.  __int64 is a macro rather than a typedef because the tree writes both
** "__int64" and "unsigned __int64", and a typedef cannot carry the second.  The single-underscore
** calling conventions appear in wwstring.h and a few others. */
#define __int64             long long
#ifndef _cdecl
#define _cdecl
#endif
#ifndef _stdcall
#define _stdcall
#endif
#ifndef _fastcall
#define _fastcall
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

#define VOID                void
#define CONST               const
#define FAR
#define NEAR
#define WINAPI
#define APIENTRY
#define CALLBACK
#define WINBASEAPI

#ifndef TRUE
#define TRUE                1
#endif
#ifndef FALSE
#define FALSE               0
#endif
#ifndef MAX_PATH
#define MAX_PATH            260
#endif

#define INVALID_HANDLE_VALUE  ((HANDLE)(intptr_t)-1)
#define INFINITE              0xFFFFFFFFu

#define S_OK                  ((HRESULT)0)
#define S_FALSE               ((HRESULT)1)
#define E_FAIL                ((HRESULT)0x80004005L)
#define E_INVALIDARG          ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY         ((HRESULT)0x8007000EL)
#define E_NOTIMPL             ((HRESULT)0x80004001L)
#define SUCCEEDED(hr)         (((HRESULT)(hr)) >= 0)
#define FAILED(hr)            (((HRESULT)(hr)) < 0)

union LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; } u;
	struct { DWORD LowPart; LONG HighPart; };
	int64_t QuadPart;
};
typedef LARGE_INTEGER * PLARGE_INTEGER;

union ULARGE_INTEGER {
	struct { DWORD LowPart; DWORD HighPart; } u;
	struct { DWORD LowPart; DWORD HighPart; };
	uint64_t QuadPart;
};

struct POINT { LONG x, y; };
struct RECT  { LONG left, top, right, bottom; };
typedef POINT * LPPOINT;
typedef RECT * LPRECT;

struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; };

struct SYSTEMTIME {
	WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};

// ---------------------------------------------------------------------------
// Error reporting.  GetLastError is per-thread, as it is on Windows.
// ---------------------------------------------------------------------------

#define ERROR_SUCCESS             0L
#define ERROR_FILE_NOT_FOUND      2L
#define ERROR_PATH_NOT_FOUND      3L
#define ERROR_ACCESS_DENIED       5L
#define ERROR_INVALID_HANDLE      6L
#define ERROR_NOT_ENOUGH_MEMORY   8L
#define ERROR_NO_MORE_FILES       18L
#define ERROR_ALREADY_EXISTS      183L
#define ERROR_CALL_NOT_IMPLEMENTED 120L

DWORD GetLastError(void);
void  SetLastError(DWORD code);

/* A call this shim does not answer.  Prints what was asked for and aborts: a plausible zero here
** becomes a desync or a silently empty asset an hour later, which is far more expensive to find
** than a stop at the call site. */
[[noreturn]] void Win32Compat_Unimplemented(const char * what);

// ---------------------------------------------------------------------------
// Clocks.  The two heaviest users in the tree.
// ---------------------------------------------------------------------------

DWORD timeGetTime(void);
DWORD GetTickCount(void);
BOOL  QueryPerformanceCounter(LARGE_INTEGER * count);
BOOL  QueryPerformanceFrequency(LARGE_INTEGER * frequency);
void  Sleep(DWORD milliseconds);

// ---------------------------------------------------------------------------
// Critical sections.  Recursive, because Win32's are and the game relies on it.
// ---------------------------------------------------------------------------

struct CRITICAL_SECTION {
	// Opaque, sized for a pthread_mutex_t on this platform; Win32Compat.cpp asserts the fit.
	alignas(16) unsigned char Opaque[64];
	int Initialised;
};
typedef CRITICAL_SECTION * LPCRITICAL_SECTION;

void InitializeCriticalSection(LPCRITICAL_SECTION cs);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);

// ---------------------------------------------------------------------------
// Interlocked operations.
// ---------------------------------------------------------------------------

LONG InterlockedIncrement(LONG volatile * addend);
LONG InterlockedDecrement(LONG volatile * addend);
LONG InterlockedExchange(LONG volatile * target, LONG value);
LONG InterlockedExchangeAdd(LONG volatile * addend, LONG value);
LONG InterlockedCompareExchange(LONG volatile * dest, LONG exchange, LONG comparand);

// ---------------------------------------------------------------------------
// Threads and events.
// ---------------------------------------------------------------------------

#define WAIT_OBJECT_0   0x00000000L
#define WAIT_TIMEOUT    0x00000102L
#define WAIT_FAILED     0xFFFFFFFFL

typedef DWORD (WINAPI * LPTHREAD_START_ROUTINE)(LPVOID);

HANDLE CreateThread(LPVOID attributes, SIZE_T stack_size, LPTHREAD_START_ROUTINE start,
                    LPVOID parameter, DWORD flags, LPDWORD thread_id);
DWORD  GetCurrentThreadId(void);
HANDLE GetCurrentThread(void);
BOOL   SetThreadPriority(HANDLE thread, int priority);

HANDLE CreateEventA(LPVOID attributes, BOOL manual_reset, BOOL initial_state, LPCSTR name);
#define CreateEvent CreateEventA
BOOL   SetEvent(HANDLE event);
BOOL   ResetEvent(HANDLE event);
DWORD  WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL   CloseHandle(HANDLE handle);

// ---------------------------------------------------------------------------
// Files.
// ---------------------------------------------------------------------------

#define GENERIC_READ            0x80000000u
#define GENERIC_WRITE           0x40000000u
#define FILE_SHARE_READ         0x00000001u
#define FILE_SHARE_WRITE        0x00000002u
#define CREATE_NEW              1
#define CREATE_ALWAYS           2
#define OPEN_EXISTING           3
#define OPEN_ALWAYS             4
#define TRUNCATE_EXISTING       5
#define FILE_ATTRIBUTE_NORMAL       0x00000080u
#define FILE_ATTRIBUTE_DIRECTORY    0x00000010u
#define FILE_ATTRIBUTE_READONLY     0x00000001u
#define INVALID_FILE_ATTRIBUTES     0xFFFFFFFFu
#define INVALID_FILE_SIZE           0xFFFFFFFFu
#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPVOID security,
                   DWORD creation, DWORD flags, HANDLE templ);
#define CreateFile CreateFileA
BOOL   ReadFile(HANDLE file, LPVOID buffer, DWORD to_read, LPDWORD read, LPVOID overlapped);
BOOL   WriteFile(HANDLE file, LPCVOID buffer, DWORD to_write, LPDWORD written, LPVOID overlapped);
DWORD  SetFilePointer(HANDLE file, LONG distance, LONG * distance_high, DWORD method);
DWORD  GetFileSize(HANDLE file, LPDWORD size_high);
BOOL   DeleteFileA(LPCSTR name);
#define DeleteFile DeleteFileA
BOOL   CreateDirectoryA(LPCSTR path, LPVOID security);
#define CreateDirectory CreateDirectoryA
DWORD  GetFileAttributesA(LPCSTR name);
#define GetFileAttributes GetFileAttributesA
DWORD  GetCurrentDirectoryA(DWORD length, LPSTR buffer);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL   SetCurrentDirectoryA(LPCSTR path);
#define SetCurrentDirectory SetCurrentDirectoryA

struct WIN32_FIND_DATAA {
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
	DWORD    nFileSizeHigh, nFileSizeLow;
	DWORD    dwReserved0, dwReserved1;
	CHAR     cFileName[MAX_PATH];
	CHAR     cAlternateFileName[14];
};
typedef WIN32_FIND_DATAA * LPWIN32_FIND_DATAA;
#define WIN32_FIND_DATA WIN32_FIND_DATAA

HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data);
#define FindFirstFile FindFirstFileA
BOOL   FindNextFileA(HANDLE find, LPWIN32_FIND_DATAA data);
#define FindNextFile FindNextFileA
BOOL   FindClose(HANDLE find);

// ---------------------------------------------------------------------------
// Modules.
// ---------------------------------------------------------------------------

typedef INT_PTR (*FARPROC)();

HMODULE  LoadLibraryA(LPCSTR name);
#define  LoadLibrary LoadLibraryA
BOOL     FreeLibrary(HMODULE module);
FARPROC  GetProcAddress(HMODULE module, LPCSTR name);
HMODULE  GetModuleHandleA(LPCSTR name);
#define  GetModuleHandle GetModuleHandleA
DWORD    GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size);
#define  GetModuleFileName GetModuleFileNameA

// ---------------------------------------------------------------------------
// System information and diagnostics.
// ---------------------------------------------------------------------------

struct MEMORYSTATUS {
	DWORD     dwLength, dwMemoryLoad;
	SIZE_T    dwTotalPhys, dwAvailPhys;
	SIZE_T    dwTotalPageFile, dwAvailPageFile;
	SIZE_T    dwTotalVirtual, dwAvailVirtual;
};
typedef MEMORYSTATUS * LPMEMORYSTATUS;
void GlobalMemoryStatus(LPMEMORYSTATUS status);

struct OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
	CHAR  szCSDVersion[128];
};
typedef OSVERSIONINFOA * LPOSVERSIONINFOA;
#define OSVERSIONINFO OSVERSIONINFOA
BOOL GetVersionExA(LPOSVERSIONINFOA info);
#define GetVersionEx GetVersionExA

void OutputDebugStringA(LPCSTR text);
#define OutputDebugString OutputDebugStringA

void GetSystemTime(SYSTEMTIME * time);
void GetLocalTime(SYSTEMTIME * time);

// ---------------------------------------------------------------------------
// Text conversion.  UTF-8 <-> UTF-16, which is all the game asks for.
// ---------------------------------------------------------------------------

#define CP_ACP    0
#define CP_UTF8   65001

int MultiByteToWideChar(UINT code_page, DWORD flags, LPCSTR in, int in_len,
                        LPWSTR out, int out_len);
int WideCharToMultiByte(UINT code_page, DWORD flags, LPCWSTR in, int in_len,
                        LPSTR out, int out_len, LPCSTR default_char, LPBOOL used_default);

// ---------------------------------------------------------------------------
// Message boxes.  Routed to stderr here; the platform layer replaces this with a real alert
// once there is a window to parent one to.
// ---------------------------------------------------------------------------

#define MB_OK                0x00000000u
#define MB_OKCANCEL          0x00000001u
#define MB_ABORTRETRYIGNORE  0x00000002u
#define MB_YESNO             0x00000004u
#define MB_ICONERROR         0x00000010u
#define MB_ICONHAND          0x00000010u
#define MB_ICONQUESTION      0x00000020u
#define MB_ICONWARNING       0x00000030u
#define MB_ICONINFORMATION   0x00000040u
#define MB_TASKMODAL         0x00002000u
#define MB_SETFOREGROUND     0x00010000u
#define IDOK      1
#define IDCANCEL  2
#define IDABORT   3
#define IDRETRY   4
#define IDIGNORE  5
#define IDYES     6
#define IDNO      7

int MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA

// ---------------------------------------------------------------------------
// Registry.  The game reads its install path and a handful of settings out of HKLM/HKCU and
// writes options back.  Backed by a plist under Application Support; see Win32Compat.cpp.
// ---------------------------------------------------------------------------

#define HKEY_CLASSES_ROOT    ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER    ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE   ((HKEY)(uintptr_t)0x80000002)
#define KEY_READ             0x20019u
#define KEY_WRITE            0x20006u
#define KEY_ALL_ACCESS       0xF003Fu
#define REG_SZ               1u
#define REG_BINARY           3u
#define REG_DWORD            4u

LONG RegOpenKeyExA(HKEY key, LPCSTR subkey, DWORD options, DWORD desired, PHKEY result);
#define RegOpenKeyEx RegOpenKeyExA
LONG RegCreateKeyExA(HKEY key, LPCSTR subkey, DWORD reserved, LPSTR cls, DWORD options,
                     DWORD desired, LPVOID security, PHKEY result, LPDWORD disposition);
#define RegCreateKeyEx RegCreateKeyExA
LONG RegQueryValueExA(HKEY key, LPCSTR value, LPDWORD reserved, LPDWORD type,
                      LPBYTE data, LPDWORD size);
#define RegQueryValueEx RegQueryValueExA
LONG RegSetValueExA(HKEY key, LPCSTR value, DWORD reserved, DWORD type,
                    const BYTE * data, DWORD size);
#define RegSetValueEx RegSetValueExA
LONG RegCloseKey(HKEY key);

// ---------------------------------------------------------------------------
// CRT spellings that are Microsoft's rather than POSIX's.
// ---------------------------------------------------------------------------

#include <cstring>
#include <cstdio>
#include <cctype>

#define stricmp    strcasecmp
#define _stricmp   strcasecmp
#define strnicmp   strncasecmp
#define _strnicmp  strncasecmp

inline char * strupr(char * s)
{
	for (char * p = s; p != nullptr && *p != 0; ++p) *p = (char)toupper((unsigned char)*p);
	return s;
}

inline char * strlwr(char * s)
{
	for (char * p = s; p != nullptr && *p != 0; ++p) *p = (char)tolower((unsigned char)*p);
	return s;
}

#define _strupr strupr
#define _strlwr strlwr

inline char * itoa(int value, char * buffer, int radix)
{
	// Only the radices the tree passes; anything else is a caller bug rather than a missing case.
	char scratch[24];   // widest is octal of a 32-bit value, 11 digits and a sign
	if      (radix == 10) std::snprintf(scratch, sizeof(scratch), "%d", value);
	else if (radix == 16) std::snprintf(scratch, sizeof(scratch), "%x", (unsigned)value);
	else if (radix == 8)  std::snprintf(scratch, sizeof(scratch), "%o", (unsigned)value);
	else                  Win32Compat_Unimplemented("itoa with a radix other than 8, 10 or 16");
	std::strcpy(buffer, scratch);
	return buffer;
}

#define _itoa itoa

// ---------------------------------------------------------------------------
// The PE COFF file header.
//
// verchk.cpp reads the TimeDateStamp out of an executable to build a version string.  There is no
// PE header on a Mach-O image, so the reader off Windows reports failure and the stamp stays zero;
// the layout is declared here because the callers hold one by value.
// ---------------------------------------------------------------------------

struct IMAGE_FILE_HEADER {
	WORD  Machine;
	WORD  NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD  SizeOfOptionalHeader;
	WORD  Characteristics;
};
typedef IMAGE_FILE_HEADER * PIMAGE_FILE_HEADER;

// ---------------------------------------------------------------------------
// Rotates.  MSVC intrinsics; clang has builtins for the same thing.
// ---------------------------------------------------------------------------

inline unsigned int  _rotl (unsigned int v, int n)       { return __builtin_rotateleft32(v, (unsigned)n); }
inline unsigned int  _rotr (unsigned int v, int n)       { return __builtin_rotateright32(v, (unsigned)n); }
inline unsigned long _lrotl(unsigned long v, int n)
{
	// unsigned long is 64 bits here and 32 on Win32.  The callers (crc.h) feed it 32-bit values
	// and expect a 32-bit rotate, so that is what this does; rotating at 64 would fold different
	// bits in and change every checksum.
	return (unsigned long)__builtin_rotateleft32((uint32_t)v, (unsigned)n);
}
inline unsigned long _lrotr(unsigned long v, int n)
{
	return (unsigned long)__builtin_rotateright32((uint32_t)v, (unsigned)n);
}

// ---------------------------------------------------------------------------
// Odds and ends the tree reaches for by their Microsoft names.
// ---------------------------------------------------------------------------

#define _vsnprintf  vsnprintf
#define _snprintf   snprintf

#define _MAX_DRIVE  3
#define _MAX_DIR    256
#define _MAX_FNAME  256
#define _MAX_EXT    256
#define _MAX_PATH   MAX_PATH

inline int _wcsicmp(const WCHAR * a, const WCHAR * b)
{
	// UTF-16, and ASCII-only folding, which is what the callers compare: file names and tags.
	while (*a != 0 && *b != 0) {
		WCHAR ca = (*a >= 'A' && *a <= 'Z') ? (WCHAR)(*a + 32) : *a;
		WCHAR cb = (*b >= 'A' && *b <= 'Z') ? (WCHAR)(*b + 32) : *b;
		if (ca != cb) return (int)ca - (int)cb;
		++a; ++b;
	}
	return (int)*a - (int)*b;
}

inline int _wcsicmp(const wchar_t * a, const wchar_t * b)
{
	// The wchar_t spelling, for UnicodeString.  ASCII folding, as above.
	while (*a != 0 && *b != 0) {
		wchar_t ca = (*a >= L'A' && *a <= L'Z') ? (wchar_t)(*a + 32) : *a;
		wchar_t cb = (*b >= L'A' && *b <= L'Z') ? (wchar_t)(*b + 32) : *b;
		if (ca != cb) return (int)ca - (int)cb;
		++a; ++b;
	}
	return (int)*a - (int)*b;
}

inline int _wcsnicmp(const wchar_t * a, const wchar_t * b, size_t n)
{
	for (size_t i = 0; i < n; ++i) {
		wchar_t ca = (a[i] >= L'A' && a[i] <= L'Z') ? (wchar_t)(a[i] + 32) : a[i];
		wchar_t cb = (b[i] >= L'A' && b[i] <= L'Z') ? (wchar_t)(b[i] + 32) : b[i];
		if (ca != cb) return (int)ca - (int)cb;
		if (a[i] == 0) break;
	}
	return 0;
}

#define _vsnwprintf vswprintf

inline void DebugBreak(void) { __builtin_debugtrap(); }

// The multimedia timer resolution calls.  Mach timers do not have a resolution to raise, and
// timeGetTime above is already nanosecond-sourced, so there is nothing for these to do.
inline DWORD timeBeginPeriod(UINT) { return 0; }
inline DWORD timeEndPeriod(UINT)   { return 0; }

// ---------------------------------------------------------------------------
// Handle types for the Windows-only subsystems.  Declared so the files that hold one compile;
// the functions that would produce one are not here, and the callers are guarded or unused.
// ---------------------------------------------------------------------------

typedef HANDLE HRSRC;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef HANDLE HACCEL;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HMENU;
typedef HANDLE HDC;
typedef HANDLE HBITMAP;
typedef HANDLE HPALETTE;
typedef HANDLE HFONT;

struct VS_FIXEDFILEINFO {
	DWORD dwSignature, dwStrucVersion;
	DWORD dwFileVersionMS, dwFileVersionLS;
	DWORD dwProductVersionMS, dwProductVersionLS;
	DWORD dwFileFlagsMask, dwFileFlags, dwFileOS;
	DWORD dwFileType, dwFileSubtype;
	DWORD dwFileDateMS, dwFileDateLS;
};

// ---------------------------------------------------------------------------
// The remaining Microsoft spellings and the small calls this tree makes.
// ---------------------------------------------------------------------------

#define _int64    long long
#define strcmpi   strcasecmp
#define _strcmpi  strcasecmp
#define stricoll  strcasecmp

// Windows platform ids, tested by cpudetect.cpp.
#define VER_PLATFORM_WIN32s         0
#define VER_PLATFORM_WIN32_WINDOWS  1
#define VER_PLATFORM_WIN32_NT       2

// FormatMessage, which wwdebug.cpp uses to turn GetLastError into text.
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000u
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200u
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100u

DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD message_id, DWORD language_id,
                     LPSTR buffer, DWORD size, void * arguments);
#define FormatMessage FormatMessageA

// Named mutexes.  mutex.cpp wants one to keep a second copy of the game off the same install; the
// shim gives it a process-local one, so the check passes and a second copy is not prevented.
HANDLE CreateMutexA(LPVOID attributes, BOOL initial_owner, LPCSTR name);
#define CreateMutex CreateMutexA
BOOL   ReleaseMutex(HANDLE mutex);

// srandom.cpp stirs free disk space into its seed.
BOOL GetDiskFreeSpaceA(LPCSTR root, LPDWORD sectors_per_cluster, LPDWORD bytes_per_sector,
                       LPDWORD free_clusters, LPDWORD total_clusters);
#define GetDiskFreeSpace GetDiskFreeSpaceA

/* The version-resource calls.  A Mach-O image carries no VERSIONINFO resource, so these report
** that there is none and the callers take the branch they already have for a file without one. */
DWORD GetFileVersionInfoSizeA(LPCSTR filename, LPDWORD handle);
#define GetFileVersionInfoSize GetFileVersionInfoSizeA
BOOL  GetFileVersionInfoA(LPCSTR filename, DWORD handle, DWORD length, LPVOID data);
#define GetFileVersionInfo GetFileVersionInfoA
BOOL  VerQueryValueA(LPCVOID block, LPCSTR sub_block, LPVOID * buffer, UINT * length);
#define VerQueryValue VerQueryValueA

// The path splitter, which MSVC keeps in <stdlib.h>.
void _splitpath(const char * path, char * drive, char * dir, char * fname, char * ext);
void _makepath(char * path, const char * drive, const char * dir,
               const char * fname, const char * ext);

// ---------------------------------------------------------------------------
// UTF-16 string calls.
//
// WCHAR is 16 bits (see the typedef above) and wchar_t is 32 on this platform, so the C library's
// wcslen and friends would walk these strings at twice the stride.  These overloads take the
// 16-bit spelling, which leaves every call site naming the function it already named.
// ---------------------------------------------------------------------------

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

inline WCHAR * wcsncpy(WCHAR * dst, const WCHAR * src, size_t n)
{
	size_t i = 0;
	for (; i < n && src[i] != 0; ++i) dst[i] = src[i];
	for (; i < n; ++i) dst[i] = 0;
	return dst;
}

inline WCHAR * wcscat(WCHAR * dst, const WCHAR * src)
{
	WCHAR * out = dst;
	while (*out != 0) ++out;
	while ((*out++ = *src++) != 0) { }
	return dst;
}

inline int wcscmp(const WCHAR * a, const WCHAR * b)
{
	while (*a != 0 && *a == *b) { ++a; ++b; }
	return (int)*a - (int)*b;
}

inline int wcsncmp(const WCHAR * a, const WCHAR * b, size_t n)
{
	for (size_t i = 0; i < n; ++i) {
		if (a[i] != b[i]) return (int)a[i] - (int)b[i];
		if (a[i] == 0) break;
	}
	return 0;
}

inline const WCHAR * wcschr(const WCHAR * s, WCHAR c)
{
	for (; *s != 0; ++s) if (*s == c) return s;
	return (c == 0) ? s : nullptr;
}
inline WCHAR * wcschr(WCHAR * s, WCHAR c)
{
	return const_cast<WCHAR *>(wcschr(const_cast<const WCHAR *>(s), c));
}

inline const WCHAR * wcsrchr(const WCHAR * s, WCHAR c)
{
	const WCHAR * found = nullptr;
	for (; *s != 0; ++s) if (*s == c) found = s;
	return found;
}
inline WCHAR * wcsrchr(WCHAR * s, WCHAR c)
{
	return const_cast<WCHAR *>(wcsrchr(const_cast<const WCHAR *>(s), c));
}

inline const WCHAR * wcsstr(const WCHAR * haystack, const WCHAR * needle)
{
	if (needle == nullptr || *needle == 0) return haystack;
	for (; *haystack != 0; ++haystack) {
		const WCHAR * h = haystack;
		const WCHAR * n = needle;
		while (*h != 0 && *n != 0 && *h == *n) { ++h; ++n; }
		if (*n == 0) return haystack;
	}
	return nullptr;
}
inline WCHAR * wcsstr(WCHAR * haystack, const WCHAR * needle)
{
	return const_cast<WCHAR *>(wcsstr(const_cast<const WCHAR *>(haystack), needle));
}

#endif // WIN32COMPAT_H
