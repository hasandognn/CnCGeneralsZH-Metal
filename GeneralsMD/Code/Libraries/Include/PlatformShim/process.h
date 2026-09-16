/* process.h - MSVC's thread entry points, on pthreads.
**
** _beginthreadex returns a handle the caller passes to WaitForSingleObject and CloseHandle, so it
** is the shim's CreateThread with the argument order the CRT uses.  The entry point's return type
** differs (unsigned rather than DWORD); both are 32 bits and the value is only ever read back
** through GetExitCodeThread, which nothing here calls.
*/
#ifndef WIN32COMPAT_SHIM_PROCESS_H
#define WIN32COMPAT_SHIM_PROCESS_H

#include "Platform/Win32Compat.h"
#include <pthread.h>

typedef unsigned (__stdcall * _beginthreadex_proc_type)(void *);

inline uintptr_t _beginthreadex(void * security, unsigned stack_size,
                                _beginthreadex_proc_type start, void * arglist,
                                unsigned initflag, unsigned * thrdaddr)
{
	DWORD id = 0;
	HANDLE h = CreateThread(security, (SIZE_T)stack_size,
	                        reinterpret_cast<LPTHREAD_START_ROUTINE>(start),
	                        arglist, (DWORD)initflag, &id);
	if (thrdaddr != nullptr) *thrdaddr = (unsigned)id;
	return (uintptr_t)h;
}

typedef void (__cdecl * _beginthread_proc_type)(void *);

inline uintptr_t _beginthread(_beginthread_proc_type start, unsigned stack_size, void * arglist)
{
	// The void-returning form.  CreateThread wants a DWORD-returning entry; the value is discarded
	// either way, and both return in the same register.
	return (uintptr_t)CreateThread(nullptr, (SIZE_T)stack_size,
	                               reinterpret_cast<LPTHREAD_START_ROUTINE>(start),
	                               arglist, 0, nullptr);
}

inline void _endthread(void)       { pthread_exit(nullptr); }
inline void _endthreadex(unsigned) { pthread_exit(nullptr); }

#endif
