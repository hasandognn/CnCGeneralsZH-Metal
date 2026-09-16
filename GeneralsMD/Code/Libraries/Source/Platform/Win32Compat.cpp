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

/* Win32Compat.cpp - the POSIX side of the shim.  See Win32Compat.h for why it exists.
 *
 * Handles are small heap objects carrying a tag, so CloseHandle can tell a file from an event from
 * a thread, and a wrong one is caught rather than reinterpreted.
 */

#include "Platform/Win32Compat.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <string>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <mach/mach_time.h>
#include <mach-o/dyld.h>
#include <crt_externs.h>

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

static thread_local DWORD g_LastError = ERROR_SUCCESS;

DWORD GetLastError(void)       { return g_LastError; }
void  SetLastError(DWORD code) { g_LastError = code; }

static DWORD ErrorFromErrno(int e)
{
	switch (e) {
		case 0:       return ERROR_SUCCESS;
		case ENOENT:  return ERROR_FILE_NOT_FOUND;
		case ENOTDIR: return ERROR_PATH_NOT_FOUND;
		case EACCES:
		case EPERM:   return ERROR_ACCESS_DENIED;
		case EBADF:   return ERROR_INVALID_HANDLE;
		case ENOMEM:  return ERROR_NOT_ENOUGH_MEMORY;
		case EEXIST:  return ERROR_ALREADY_EXISTS;
		default:      return (DWORD)e;
	}
}

static void SetErrnoError(void) { g_LastError = ErrorFromErrno(errno); }

void Win32Compat_Unimplemented(const char * what)
{
	std::fprintf(stderr,
		"\n[Win32Compat] %s is not implemented by the macOS shim.\n"
		"This call has no honest answer here, and a plausible one would surface as a desync or a\n"
		"missing asset much later.  Implement it in Win32Compat.cpp, or change the caller.\n\n",
		what ? what : "(unnamed call)");
	std::abort();
}

// ---------------------------------------------------------------------------
// Handles
// ---------------------------------------------------------------------------

enum HandleTag { TAG_FILE = 1, TAG_EVENT, TAG_THREAD, TAG_FIND };

struct HandleBase {
	HandleTag Tag;
	explicit HandleBase(HandleTag t) : Tag(t) {}
};

struct FileHandle : HandleBase {
	int Fd;
	explicit FileHandle(int fd) : HandleBase(TAG_FILE), Fd(fd) {}
};

struct EventHandle : HandleBase {
	pthread_mutex_t Mutex;
	pthread_cond_t  Cond;
	bool            Signalled;
	bool            ManualReset;
	EventHandle(bool manual, bool initial)
		: HandleBase(TAG_EVENT), Signalled(initial), ManualReset(manual)
	{
		pthread_mutex_init(&Mutex, nullptr);
		pthread_cond_init(&Cond, nullptr);
	}
	~EventHandle() { pthread_cond_destroy(&Cond); pthread_mutex_destroy(&Mutex); }
};

struct ThreadHandle : HandleBase {
	pthread_t Thread;
	bool      Joined;
	ThreadHandle() : HandleBase(TAG_THREAD), Thread(), Joined(false) {}
};

struct FindHandle : HandleBase {
	DIR *       Dir;
	std::string Pattern;
	std::string Directory;
	FindHandle() : HandleBase(TAG_FIND), Dir(nullptr) {}
};

static HandleBase * AsHandle(HANDLE h, HandleTag tag)
{
	if (h == nullptr || h == INVALID_HANDLE_VALUE) return nullptr;
	HandleBase * base = static_cast<HandleBase *>(h);
	return base->Tag == tag ? base : nullptr;
}

// ---------------------------------------------------------------------------
// Clocks
//
// mach_absolute_time is monotonic, which is what frame pacing wants.  The timebase is queried
// once: on Apple silicon it is 125/3 nanoseconds per tick and not 1/1, so it cannot be assumed.
// ---------------------------------------------------------------------------

static mach_timebase_info_data_t g_Timebase;
static uint64_t                  g_StartTicks;

static void InitClock(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, []() {
		mach_timebase_info(&g_Timebase);
		g_StartTicks = mach_absolute_time();
	});
}

static uint64_t NanosSinceStart(void)
{
	InitClock();
	uint64_t ticks = mach_absolute_time() - g_StartTicks;
	return ticks * g_Timebase.numer / g_Timebase.denom;
}

DWORD timeGetTime(void)  { return (DWORD)(NanosSinceStart() / 1000000ull); }
DWORD GetTickCount(void) { return timeGetTime(); }

BOOL QueryPerformanceCounter(LARGE_INTEGER * count)
{
	if (count == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	count->QuadPart = (int64_t)NanosSinceStart();
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER * frequency)
{
	// The counter above is reported in nanoseconds, so the frequency is exactly 1 GHz.  Reporting
	// the raw mach tick rate instead would make every interval the game computes wrong by the
	// timebase ratio.
	if (frequency == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	frequency->QuadPart = 1000000000ll;
	return TRUE;
}

void Sleep(DWORD milliseconds)
{
	if (milliseconds == 0) { sched_yield(); return; }
	struct timespec ts;
	ts.tv_sec  = (time_t)(milliseconds / 1000u);
	ts.tv_nsec = (long)((milliseconds % 1000u) * 1000000ul);
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { }
}

// ---------------------------------------------------------------------------
// Critical sections
// ---------------------------------------------------------------------------

static_assert(sizeof(pthread_mutex_t) <= 64,
              "CRITICAL_SECTION::Opaque is too small for a pthread_mutex_t on this platform");

static pthread_mutex_t * MutexOf(LPCRITICAL_SECTION cs)
{
	return reinterpret_cast<pthread_mutex_t *>(cs->Opaque);
}

void InitializeCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs == nullptr) return;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	// Win32 critical sections are recursive and the game takes them recursively.
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(MutexOf(cs), &attr);
	pthread_mutexattr_destroy(&attr);
	cs->Initialised = 1;
}

void DeleteCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs == nullptr || !cs->Initialised) return;
	pthread_mutex_destroy(MutexOf(cs));
	cs->Initialised = 0;
}

void EnterCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs == nullptr) return;
	if (!cs->Initialised) InitializeCriticalSection(cs);
	pthread_mutex_lock(MutexOf(cs));
}

void LeaveCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs == nullptr || !cs->Initialised) return;
	pthread_mutex_unlock(MutexOf(cs));
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs == nullptr) return FALSE;
	if (!cs->Initialised) InitializeCriticalSection(cs);
	return pthread_mutex_trylock(MutexOf(cs)) == 0 ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Interlocked
// ---------------------------------------------------------------------------

LONG InterlockedIncrement(LONG volatile * addend)
{ return __atomic_add_fetch(addend, 1, __ATOMIC_SEQ_CST); }

LONG InterlockedDecrement(LONG volatile * addend)
{ return __atomic_sub_fetch(addend, 1, __ATOMIC_SEQ_CST); }

LONG InterlockedExchange(LONG volatile * target, LONG value)
{ return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }

LONG InterlockedExchangeAdd(LONG volatile * addend, LONG value)
{ return __atomic_fetch_add(addend, value, __ATOMIC_SEQ_CST); }

LONG InterlockedCompareExchange(LONG volatile * dest, LONG exchange, LONG comparand)
{
	LONG expected = comparand;
	__atomic_compare_exchange_n(dest, &expected, exchange, false,
	                            __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return expected;   // Win32 returns the value that was there, matched or not.
}

// ---------------------------------------------------------------------------
// Threads and events
// ---------------------------------------------------------------------------

namespace {
	struct ThreadStart {
		LPTHREAD_START_ROUTINE Entry;
		LPVOID                 Parameter;
	};
}

static void * ThreadTrampoline(void * raw)
{
	ThreadStart * start = static_cast<ThreadStart *>(raw);
	LPTHREAD_START_ROUTINE entry = start->Entry;
	LPVOID parameter = start->Parameter;
	delete start;
	return reinterpret_cast<void *>((uintptr_t)entry(parameter));
}

HANDLE CreateThread(LPVOID, SIZE_T stack_size, LPTHREAD_START_ROUTINE start,
                    LPVOID parameter, DWORD, LPDWORD thread_id)
{
	ThreadHandle * handle = new ThreadHandle();
	ThreadStart * payload = new ThreadStart{start, parameter};

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	if (stack_size != 0) pthread_attr_setstacksize(&attr, (size_t)stack_size);

	int rc = pthread_create(&handle->Thread, &attr, ThreadTrampoline, payload);
	pthread_attr_destroy(&attr);

	if (rc != 0) {
		delete payload;
		delete handle;
		g_LastError = ErrorFromErrno(rc);
		return nullptr;
	}
	if (thread_id != nullptr) *thread_id = (DWORD)(uintptr_t)handle->Thread;
	return handle;
}

DWORD  GetCurrentThreadId(void) { return (DWORD)(uintptr_t)pthread_self(); }
HANDLE GetCurrentThread(void)   { return (HANDLE)pthread_self(); }

BOOL SetThreadPriority(HANDLE, int)
{
	// Deliberately a no-op.  Win32 thread priorities do not map onto Mach scheduling in a way that
	// preserves what the caller meant, and guessing costs frame pacing.
	return TRUE;
}

HANDLE CreateEventA(LPVOID, BOOL manual_reset, BOOL initial_state, LPCSTR)
{
	return new EventHandle(manual_reset != FALSE, initial_state != FALSE);
}

BOOL SetEvent(HANDLE handle)
{
	EventHandle * event = static_cast<EventHandle *>(AsHandle(handle, TAG_EVENT));
	if (event == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	pthread_mutex_lock(&event->Mutex);
	event->Signalled = true;
	// A manual-reset event releases everyone; an auto-reset one releases a single waiter, which
	// then clears the flag itself.
	if (event->ManualReset) pthread_cond_broadcast(&event->Cond);
	else                    pthread_cond_signal(&event->Cond);
	pthread_mutex_unlock(&event->Mutex);
	return TRUE;
}

BOOL ResetEvent(HANDLE handle)
{
	EventHandle * event = static_cast<EventHandle *>(AsHandle(handle, TAG_EVENT));
	if (event == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	pthread_mutex_lock(&event->Mutex);
	event->Signalled = false;
	pthread_mutex_unlock(&event->Mutex);
	return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
	if (EventHandle * event = static_cast<EventHandle *>(AsHandle(handle, TAG_EVENT))) {
		pthread_mutex_lock(&event->Mutex);
		int rc = 0;
		if (milliseconds == INFINITE) {
			while (!event->Signalled && rc == 0) {
				rc = pthread_cond_wait(&event->Cond, &event->Mutex);
			}
		} else {
			struct timespec deadline;
			clock_gettime(CLOCK_REALTIME, &deadline);
			deadline.tv_sec  += (time_t)(milliseconds / 1000u);
			deadline.tv_nsec += (long)((milliseconds % 1000u) * 1000000ul);
			if (deadline.tv_nsec >= 1000000000l) { deadline.tv_sec += 1; deadline.tv_nsec -= 1000000000l; }
			while (!event->Signalled && rc == 0) {
				rc = pthread_cond_timedwait(&event->Cond, &event->Mutex, &deadline);
			}
		}
		DWORD result = WAIT_OBJECT_0;
		if (!event->Signalled)        result = (rc == ETIMEDOUT) ? WAIT_TIMEOUT : WAIT_FAILED;
		else if (!event->ManualReset) event->Signalled = false;
		pthread_mutex_unlock(&event->Mutex);
		return result;
	}

	if (ThreadHandle * thread = static_cast<ThreadHandle *>(AsHandle(handle, TAG_THREAD))) {
		// Only the infinite wait is honest here: a timed join has no pthread equivalent, and the
		// game only ever waits forever on a thread it is shutting down.
		if (milliseconds != INFINITE) Win32Compat_Unimplemented("WaitForSingleObject(thread, timeout)");
		if (!thread->Joined) { pthread_join(thread->Thread, nullptr); thread->Joined = true; }
		return WAIT_OBJECT_0;
	}

	g_LastError = ERROR_INVALID_HANDLE;
	return WAIT_FAILED;
}

BOOL CloseHandle(HANDLE handle)
{
	if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
		g_LastError = ERROR_INVALID_HANDLE;
		return FALSE;
	}
	HandleBase * base = static_cast<HandleBase *>(handle);
	switch (base->Tag) {
		case TAG_FILE: {
			FileHandle * file = static_cast<FileHandle *>(base);
			if (file->Fd >= 0) close(file->Fd);
			delete file;
			return TRUE;
		}
		case TAG_EVENT:
			delete static_cast<EventHandle *>(base);
			return TRUE;
		case TAG_THREAD: {
			ThreadHandle * thread = static_cast<ThreadHandle *>(base);
			if (!thread->Joined) pthread_detach(thread->Thread);
			delete thread;
			return TRUE;
		}
		case TAG_FIND:
			return FindClose(handle);
	}
	g_LastError = ERROR_INVALID_HANDLE;
	return FALSE;
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD, LPVOID, DWORD creation, DWORD, HANDLE)
{
	if (name == nullptr) { g_LastError = ERROR_FILE_NOT_FOUND; return INVALID_HANDLE_VALUE; }

	int flags = 0;
	const bool reading = (access & GENERIC_READ)  != 0;
	const bool writing = (access & GENERIC_WRITE) != 0;
	if (reading && writing) flags = O_RDWR;
	else if (writing)       flags = O_WRONLY;
	else                    flags = O_RDONLY;

	switch (creation) {
		case CREATE_NEW:        flags |= O_CREAT | O_EXCL;  break;
		case CREATE_ALWAYS:     flags |= O_CREAT | O_TRUNC; break;
		case OPEN_EXISTING:                                 break;
		case OPEN_ALWAYS:       flags |= O_CREAT;           break;
		case TRUNCATE_EXISTING: flags |= O_TRUNC;           break;
		default:                                            break;
	}

	int fd = open(name, flags, 0644);
	if (fd < 0) { SetErrnoError(); return INVALID_HANDLE_VALUE; }
	return new FileHandle(fd);
}

BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD to_read, LPDWORD read_out, LPVOID)
{
	FileHandle * file = static_cast<FileHandle *>(AsHandle(handle, TAG_FILE));
	if (file == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	ssize_t got = read(file->Fd, buffer, (size_t)to_read);
	if (got < 0) { SetErrnoError(); if (read_out) *read_out = 0; return FALSE; }
	if (read_out != nullptr) *read_out = (DWORD)got;
	return TRUE;
}

BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD to_write, LPDWORD written_out, LPVOID)
{
	FileHandle * file = static_cast<FileHandle *>(AsHandle(handle, TAG_FILE));
	if (file == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	ssize_t put = write(file->Fd, buffer, (size_t)to_write);
	if (put < 0) { SetErrnoError(); if (written_out) *written_out = 0; return FALSE; }
	if (written_out != nullptr) *written_out = (DWORD)put;
	return TRUE;
}

DWORD SetFilePointer(HANDLE handle, LONG distance, LONG * distance_high, DWORD method)
{
	FileHandle * file = static_cast<FileHandle *>(AsHandle(handle, TAG_FILE));
	if (file == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return INVALID_FILE_SIZE; }

	int whence = (method == FILE_CURRENT) ? SEEK_CUR : (method == FILE_END) ? SEEK_END : SEEK_SET;
	int64_t offset = distance;
	if (distance_high != nullptr) offset |= ((int64_t)*distance_high) << 32;

	off_t now = lseek(file->Fd, (off_t)offset, whence);
	if (now == (off_t)-1) { SetErrnoError(); return INVALID_FILE_SIZE; }
	if (distance_high != nullptr) *distance_high = (LONG)((int64_t)now >> 32);
	return (DWORD)((uint64_t)now & 0xFFFFFFFFull);
}

DWORD GetFileSize(HANDLE handle, LPDWORD size_high)
{
	FileHandle * file = static_cast<FileHandle *>(AsHandle(handle, TAG_FILE));
	if (file == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return INVALID_FILE_SIZE; }
	struct stat st;
	if (fstat(file->Fd, &st) != 0) { SetErrnoError(); return INVALID_FILE_SIZE; }
	if (size_high != nullptr) *size_high = (DWORD)((uint64_t)st.st_size >> 32);
	return (DWORD)((uint64_t)st.st_size & 0xFFFFFFFFull);
}

BOOL DeleteFileA(LPCSTR name)
{
	if (name == nullptr || unlink(name) != 0) { SetErrnoError(); return FALSE; }
	return TRUE;
}

BOOL CreateDirectoryA(LPCSTR path, LPVOID)
{
	if (path == nullptr || mkdir(path, 0755) != 0) { SetErrnoError(); return FALSE; }
	return TRUE;
}

DWORD GetFileAttributesA(LPCSTR name)
{
	struct stat st;
	if (name == nullptr || stat(name, &st) != 0) { SetErrnoError(); return INVALID_FILE_ATTRIBUTES; }
	DWORD attributes = 0;
	if (S_ISDIR(st.st_mode))         attributes |= FILE_ATTRIBUTE_DIRECTORY;
	if ((st.st_mode & S_IWUSR) == 0) attributes |= FILE_ATTRIBUTE_READONLY;
	if (attributes == 0)             attributes = FILE_ATTRIBUTE_NORMAL;
	return attributes;
}

DWORD GetCurrentDirectoryA(DWORD length, LPSTR buffer)
{
	char path[PATH_MAX];
	if (getcwd(path, sizeof(path)) == nullptr) { SetErrnoError(); return 0; }
	DWORD needed = (DWORD)std::strlen(path);
	if (buffer == nullptr || length <= needed) return needed + 1;   // Win32 counts the NUL here.
	std::strcpy(buffer, path);
	return needed;
}

BOOL SetCurrentDirectoryA(LPCSTR path)
{
	if (path == nullptr || chdir(path) != 0) { SetErrnoError(); return FALSE; }
	return TRUE;
}

/* Directory enumeration.
 *
 * FindFirstFile takes a path with a glob in the last component.  readdir gives entries in
 * directory order rather than Windows' order; nothing in the game depends on the order, but the
 * match has to be case-insensitive, because the asset names on disk and the names in the .big
 * indexes disagree about case throughout. */

static void FillFindData(LPWIN32_FIND_DATAA data, const std::string & directory, const char * name)
{
	std::memset(data, 0, sizeof(*data));
	std::snprintf(data->cFileName, MAX_PATH, "%s", name);

	std::string full = directory + "/" + name;
	struct stat st;
	if (stat(full.c_str(), &st) == 0) {
		data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
		data->nFileSizeLow  = (DWORD)((uint64_t)st.st_size & 0xFFFFFFFFull);
		data->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
	} else {
		data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	}
}

static bool AdvanceFind(FindHandle * find, LPWIN32_FIND_DATAA data)
{
	struct dirent * entry;
	while ((entry = readdir(find->Dir)) != nullptr) {
		if (fnmatch(find->Pattern.c_str(), entry->d_name, FNM_CASEFOLD) == 0) {
			FillFindData(data, find->Directory, entry->d_name);
			return true;
		}
	}
	return false;
}

HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
	if (pattern == nullptr || data == nullptr) {
		g_LastError = ERROR_FILE_NOT_FOUND;
		return INVALID_HANDLE_VALUE;
	}

	std::string spec(pattern);
	for (char & c : spec) if (c == '\\') c = '/';      // the game builds these with backslashes

	std::string directory = ".";
	std::string leaf      = spec;
	std::string::size_type slash = spec.find_last_of('/');
	if (slash != std::string::npos) {
		directory = spec.substr(0, slash);
		leaf      = spec.substr(slash + 1);
		if (directory.empty()) directory = "/";
	}

	FindHandle * find = new FindHandle();
	find->Dir = opendir(directory.c_str());
	if (find->Dir == nullptr) {
		delete find;
		SetErrnoError();
		return INVALID_HANDLE_VALUE;
	}
	find->Pattern   = leaf;
	find->Directory = directory;

	if (!AdvanceFind(find, data)) {
		closedir(find->Dir);
		delete find;
		g_LastError = ERROR_NO_MORE_FILES;
		return INVALID_HANDLE_VALUE;
	}
	return find;
}

BOOL FindNextFileA(HANDLE handle, LPWIN32_FIND_DATAA data)
{
	FindHandle * find = static_cast<FindHandle *>(AsHandle(handle, TAG_FIND));
	if (find == nullptr || data == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	if (!AdvanceFind(find, data)) { g_LastError = ERROR_NO_MORE_FILES; return FALSE; }
	return TRUE;
}

BOOL FindClose(HANDLE handle)
{
	FindHandle * find = static_cast<FindHandle *>(AsHandle(handle, TAG_FIND));
	if (find == nullptr) { g_LastError = ERROR_INVALID_HANDLE; return FALSE; }
	if (find->Dir != nullptr) closedir(find->Dir);
	delete find;
	return TRUE;
}

// ---------------------------------------------------------------------------
// Modules
// ---------------------------------------------------------------------------

HMODULE LoadLibraryA(LPCSTR name)
{
	if (name == nullptr) { g_LastError = ERROR_FILE_NOT_FOUND; return nullptr; }
	void * module = dlopen(name, RTLD_LAZY | RTLD_LOCAL);
	if (module == nullptr) g_LastError = ERROR_FILE_NOT_FOUND;
	return module;
}

BOOL FreeLibrary(HMODULE module)
{
	if (module == nullptr) return FALSE;
	return dlclose(module) == 0 ? TRUE : FALSE;
}

FARPROC GetProcAddress(HMODULE module, LPCSTR name)
{
	if (name == nullptr) return nullptr;
	void * symbol = dlsym(module != nullptr ? module : RTLD_DEFAULT, name);
	return reinterpret_cast<FARPROC>(symbol);
}

HMODULE GetModuleHandleA(LPCSTR name)
{
	// NULL means "the running image", which RTLD_DEFAULT stands in for.
	if (name == nullptr) return (HMODULE)RTLD_DEFAULT;
	return dlopen(name, RTLD_LAZY | RTLD_NOLOAD);
}

DWORD GetModuleFileNameA(HMODULE, LPSTR buffer, DWORD size)
{
	if (buffer == nullptr || size == 0) return 0;
	char path[PATH_MAX];
	uint32_t length = sizeof(path);
	if (_NSGetExecutablePath(path, &length) != 0) { g_LastError = ERROR_FILE_NOT_FOUND; return 0; }

	char resolved[PATH_MAX];
	const char * use = realpath(path, resolved) ? resolved : path;

	std::snprintf(buffer, size, "%s", use);
	return (DWORD)std::strlen(buffer);
}

// ---------------------------------------------------------------------------
// System information and diagnostics
// ---------------------------------------------------------------------------

void GlobalMemoryStatus(LPMEMORYSTATUS status)
{
	if (status == nullptr) return;
	std::memset(status, 0, sizeof(*status));
	status->dwLength = (DWORD)sizeof(*status);

	uint64_t physical = 0;
	size_t   length   = sizeof(physical);
	if (sysctlbyname("hw.memsize", &physical, &length, nullptr, 0) != 0) physical = 0;

	// The callers only compare these against thresholds to pick texture detail, so reporting
	// physical memory for the virtual figures too is the honest simplification: this platform has
	// no fixed page file to report.
	status->dwTotalPhys     = (SIZE_T)physical;
	status->dwAvailPhys     = (SIZE_T)physical;
	status->dwTotalVirtual  = (SIZE_T)physical;
	status->dwAvailVirtual  = (SIZE_T)physical;
	status->dwTotalPageFile = (SIZE_T)physical;
	status->dwAvailPageFile = (SIZE_T)physical;
	status->dwMemoryLoad    = 0;
}

BOOL GetVersionExA(LPOSVERSIONINFOA info)
{
	if (info == nullptr) return FALSE;
	// The callers gate DirectX and shell behaviour on "at least Windows XP".  A version past every
	// such check is the truthful answer to the question they are actually asking, which is whether
	// this machine is too old to run the game.
	info->dwMajorVersion = 10;
	info->dwMinorVersion = 0;
	info->dwBuildNumber  = 19045;
	info->dwPlatformId   = 2;           // VER_PLATFORM_WIN32_NT
	std::snprintf(info->szCSDVersion, sizeof(info->szCSDVersion), "macOS shim");
	return TRUE;
}

void OutputDebugStringA(LPCSTR text)
{
	if (text != nullptr) std::fputs(text, stderr);
}

static void FillSystemTime(SYSTEMTIME * out, const struct tm & t, long microseconds)
{
	out->wYear         = (WORD)(t.tm_year + 1900);
	out->wMonth        = (WORD)(t.tm_mon + 1);
	out->wDayOfWeek    = (WORD)t.tm_wday;
	out->wDay          = (WORD)t.tm_mday;
	out->wHour         = (WORD)t.tm_hour;
	out->wMinute       = (WORD)t.tm_min;
	out->wSecond       = (WORD)t.tm_sec;
	out->wMilliseconds = (WORD)(microseconds / 1000);
}

void GetSystemTime(SYSTEMTIME * time)
{
	if (time == nullptr) return;
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct tm utc;
	gmtime_r(&now.tv_sec, &utc);
	FillSystemTime(time, utc, now.tv_nsec / 1000);
}

void GetLocalTime(SYSTEMTIME * time)
{
	if (time == nullptr) return;
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct tm local;
	localtime_r(&now.tv_sec, &local);
	FillSystemTime(time, local, now.tv_nsec / 1000);
}

/* UTF-8 <-> UTF-16.
 *
 * Written out rather than handed to iconv because the lengths Win32 reports - and the "pass a zero
 * length to ask how much room you need" protocol - are part of the contract the callers rely on. */

int MultiByteToWideChar(UINT, DWORD, LPCSTR in, int in_len, LPWSTR out, int out_len)
{
	if (in == nullptr) return 0;
	const size_t bytes = (in_len < 0) ? std::strlen(in) + 1 : (size_t)in_len;

	int    produced = 0;
	size_t i = 0;
	while (i < bytes) {
		unsigned char c = (unsigned char)in[i];
		uint32_t code;
		size_t   extra;
		if      (c < 0x80)         { code = c;         extra = 0; }
		else if ((c >> 5) == 0x06) { code = c & 0x1Fu; extra = 1; }
		else if ((c >> 4) == 0x0E) { code = c & 0x0Fu; extra = 2; }
		else if ((c >> 3) == 0x1E) { code = c & 0x07u; extra = 3; }
		else                       { code = 0xFFFD;    extra = 0; }   // lone continuation byte

		if (i + extra >= bytes) { code = 0xFFFD; extra = 0; }         // truncated sequence
		for (size_t k = 1; k <= extra; ++k) {
			code = (code << 6) | ((unsigned char)in[i + k] & 0x3Fu);
		}
		i += extra + 1;

		if (code >= 0x10000) {
			if (out != nullptr) {
				if (produced + 2 > out_len) return 0;
				uint32_t v = code - 0x10000;
				out[produced]     = (WCHAR)(0xD800 + (v >> 10));
				out[produced + 1] = (WCHAR)(0xDC00 + (v & 0x3FF));
			}
			produced += 2;
		} else {
			if (out != nullptr) {
				if (produced + 1 > out_len) return 0;
				out[produced] = (WCHAR)code;
			}
			produced += 1;
		}
	}
	return produced;
}

int WideCharToMultiByte(UINT, DWORD, LPCWSTR in, int in_len, LPSTR out, int out_len,
                        LPCSTR, LPBOOL used_default)
{
	if (used_default != nullptr) *used_default = FALSE;
	if (in == nullptr) return 0;

	size_t units = 0;
	if (in_len < 0) { while (in[units] != 0) ++units; ++units; }
	else            { units = (size_t)in_len; }

	int produced = 0;
	for (size_t i = 0; i < units; ++i) {
		uint32_t code = in[i];
		if (code >= 0xD800 && code <= 0xDBFF && i + 1 < units &&
		    in[i + 1] >= 0xDC00 && in[i + 1] <= 0xDFFF) {
			code = 0x10000 + ((code - 0xD800) << 10) + (in[i + 1] - 0xDC00);
			++i;
		}

		char encoded[4];
		int  length;
		if      (code < 0x80)  { length = 1; encoded[0] = (char)code; }
		else if (code < 0x800) { length = 2; encoded[0] = (char)(0xC0 | (code >> 6));
		                                      encoded[1] = (char)(0x80 | (code & 0x3F)); }
		else if (code < 0x10000){ length = 3; encoded[0] = (char)(0xE0 | (code >> 12));
		                                      encoded[1] = (char)(0x80 | ((code >> 6) & 0x3F));
		                                      encoded[2] = (char)(0x80 | (code & 0x3F)); }
		else                   { length = 4; encoded[0] = (char)(0xF0 | (code >> 18));
		                                      encoded[1] = (char)(0x80 | ((code >> 12) & 0x3F));
		                                      encoded[2] = (char)(0x80 | ((code >> 6) & 0x3F));
		                                      encoded[3] = (char)(0x80 | (code & 0x3F)); }

		if (out != nullptr) {
			if (produced + length > out_len) return 0;
			std::memcpy(out + produced, encoded, (size_t)length);
		}
		produced += length;
	}
	return produced;
}

// ---------------------------------------------------------------------------
// Message boxes
// ---------------------------------------------------------------------------

int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
	std::fprintf(stderr, "\n=== %s ===\n%s\n\n",
	             caption ? caption : "Zero Hour", text ? text : "");
	// Without a window there is nobody to answer, so every box takes its default button.  The
	// platform layer replaces this with a real alert once there is a window to parent one to.
	if (type & MB_YESNO)            return IDYES;
	if (type & MB_OKCANCEL)         return IDOK;
	if (type & MB_ABORTRETRYIGNORE) return IDIGNORE;
	return IDOK;
}

/* The registry.
 *
 * The game reads its install path, its language and a few display settings out of HKLM and writes
 * options back to HKCU.  None of that applies here: the install path is the bundle, and the
 * options already have a file of their own in Options.ini.
 *
 * So every open reports "no such key" and every caller takes the default branch it already has for
 * a machine where the game was never installed by the Windows installer - which is every Mac.  That
 * is a real answer rather than a stub: there is no registry here, and saying so is correct.  The
 * couple of settings that genuinely need to persist are the platform layer's job, not this one's.
 */

LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, PHKEY result)
{
	if (result != nullptr) *result = nullptr;
	return ERROR_FILE_NOT_FOUND;
}

LONG RegCreateKeyExA(HKEY, LPCSTR, DWORD, LPSTR, DWORD, DWORD, LPVOID,
                     PHKEY result, LPDWORD disposition)
{
	if (result != nullptr)      *result = nullptr;
	if (disposition != nullptr) *disposition = 0;
	return ERROR_ACCESS_DENIED;
}

LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD type, LPBYTE, LPDWORD size)
{
	if (type != nullptr) *type = 0;
	if (size != nullptr) *size = 0;
	return ERROR_FILE_NOT_FOUND;
}

LONG RegSetValueExA(HKEY, LPCSTR, DWORD, DWORD, const BYTE *, DWORD)
{
	return ERROR_ACCESS_DENIED;
}

LONG RegCloseKey(HKEY)
{
	return ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// The tail: version resources, named mutexes, disk space, path splitting.
// ---------------------------------------------------------------------------

DWORD FormatMessageA(DWORD, LPCVOID, DWORD message_id, DWORD, LPSTR buffer, DWORD size, void *)
{
	if (buffer == nullptr || size == 0) return 0;
	// The callers pass a GetLastError value, and this shim's error codes are errno where they are
	// not one of the ERROR_ constants, so strerror is the right text for most of them.
	const char * text = std::strerror((int)message_id);
	std::snprintf(buffer, size, "%s", text ? text : "unknown error");
	return (DWORD)std::strlen(buffer);
}

/* Named mutexes.
 *
 * The one caller uses this to notice a second copy of the game on the same machine.  A process
 * local mutex cannot notice that, so this never reports ERROR_ALREADY_EXISTS and a second copy is
 * not prevented - which is what the LAN screen wants anyway, since the game supports two copies on
 * one machine playing each other. */

HANDLE CreateMutexA(LPVOID, BOOL initial_owner, LPCSTR)
{
	// Backed by the event handle shape so CloseHandle already knows how to free it.
	EventHandle * handle = new EventHandle(true, initial_owner == FALSE);
	g_LastError = ERROR_SUCCESS;
	return handle;
}

BOOL ReleaseMutex(HANDLE mutex) { return SetEvent(mutex); }

BOOL GetDiskFreeSpaceA(LPCSTR, LPDWORD sectors_per_cluster, LPDWORD bytes_per_sector,
                       LPDWORD free_clusters, LPDWORD total_clusters)
{
	// srandom.cpp stirs these into a seed, so any honest reading of the filesystem will do.
	struct statvfs st;
	if (statvfs(".", &st) != 0) { SetErrnoError(); return FALSE; }
	if (sectors_per_cluster != nullptr) *sectors_per_cluster = 1;
	if (bytes_per_sector    != nullptr) *bytes_per_sector    = (DWORD)st.f_frsize;
	if (free_clusters       != nullptr) *free_clusters       = (DWORD)(st.f_bavail & 0xFFFFFFFFull);
	if (total_clusters      != nullptr) *total_clusters      = (DWORD)(st.f_blocks & 0xFFFFFFFFull);
	return TRUE;
}

DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD handle)
{
	// A Mach-O image carries no VERSIONINFO resource.  Zero is what Windows returns for a file
	// without one, and the callers already branch on it.
	if (handle != nullptr) *handle = 0;
	g_LastError = ERROR_FILE_NOT_FOUND;
	return 0;
}

BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID)
{
	g_LastError = ERROR_FILE_NOT_FOUND;
	return FALSE;
}

BOOL VerQueryValueA(LPCVOID, LPCSTR, LPVOID * buffer, UINT * length)
{
	if (buffer != nullptr) *buffer = nullptr;
	if (length != nullptr) *length = 0;
	return FALSE;
}

/* _splitpath and _makepath.
 *
 * There are no drive letters here, so the drive component always comes back empty.  The separator
 * is either slash, because the tree builds paths with backslashes and reads them back on a
 * filesystem that uses forward ones. */

static const char * LastSeparator(const char * path)
{
	const char * slash     = std::strrchr(path, '/');
	const char * backslash = std::strrchr(path, '\\');
	if (slash == nullptr)     return backslash;
	if (backslash == nullptr) return slash;
	return (slash > backslash) ? slash : backslash;
}

void _splitpath(const char * path, char * drive, char * dir, char * fname, char * ext)
{
	if (drive != nullptr) drive[0] = 0;
	if (dir   != nullptr) dir[0]   = 0;
	if (fname != nullptr) fname[0] = 0;
	if (ext   != nullptr) ext[0]   = 0;
	if (path == nullptr) return;

	const char * sep  = LastSeparator(path);
	const char * leaf = (sep != nullptr) ? sep + 1 : path;

	if (dir != nullptr && sep != nullptr) {
		size_t n = (size_t)(sep - path) + 1;
		std::memcpy(dir, path, n);
		dir[n] = 0;
	}

	const char * dot = std::strrchr(leaf, '.');
	if (dot == nullptr) {
		if (fname != nullptr) std::strcpy(fname, leaf);
	} else {
		if (fname != nullptr) {
			size_t n = (size_t)(dot - leaf);
			std::memcpy(fname, leaf, n);
			fname[n] = 0;
		}
		if (ext != nullptr) std::strcpy(ext, dot);
	}
}

void _makepath(char * path, const char *, const char * dir, const char * fname, const char * ext)
{
	if (path == nullptr) return;
	path[0] = 0;
	if (dir   != nullptr && dir[0]   != 0) std::strcat(path, dir);
	if (fname != nullptr && fname[0] != 0) std::strcat(path, fname);
	if (ext   != nullptr && ext[0]   != 0) std::strcat(path, ext);
}

// ---------------------------------------------------------------------------
// The command line, rebuilt from argv.
// ---------------------------------------------------------------------------

static std::string BuildCommandLine(void)
{
	int     argc = *_NSGetArgc();
	char ** argv = *_NSGetArgv();
	std::string line;
	for (int i = 0; i < argc; ++i) {
		if (i != 0) line += ' ';
		// The shell already split these; an argument carrying a space is quoted again so the
		// callers' tokeniser puts it back together the way Windows would have handed it over.
		const bool spaced = std::strchr(argv[i], ' ') != nullptr;
		if (spaced) line += '"';
		line += argv[i];
		if (spaced) line += '"';
	}
	return line;
}

LPSTR GetCommandLineA(void)
{
	static std::string line = BuildCommandLine();
	return const_cast<LPSTR>(line.c_str());
}

const wchar_t * GetCommandLineW(void)
{
	static std::wstring wide = []() {
		std::string narrow = BuildCommandLine();
		std::wstring out;
		out.reserve(narrow.size());
		// A path and a handful of switches; anything outside ASCII comes through as the bytes it
		// was, which is what the option matching above compares.
		for (unsigned char c : narrow) out.push_back((wchar_t)c);
		return out;
	}();
	return wide.c_str();
}

DWORD FormatMessageW(DWORD, LPCVOID, DWORD message_id, DWORD, WCHAR * buffer, DWORD size, void *)
{
	if (buffer == nullptr || size == 0) return 0;
	const char * text = std::strerror((int)message_id);
	if (text == nullptr) text = "unknown error";
	DWORD i = 0;
	for (; text[i] != 0 && i + 1 < size; ++i) buffer[i] = (WCHAR)(unsigned char)text[i];
	buffer[i] = 0;
	return i;
}
