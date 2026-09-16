/* crtdbg.h - the MSVC debug heap.  There is none here; the macros go quiet. */
#ifndef WIN32COMPAT_SHIM_CRTDBG_H
#define WIN32COMPAT_SHIM_CRTDBG_H
#define _ASSERT(x)       ((void)0)
#define _ASSERTE(x)      ((void)0)
#define _CrtCheckMemory()            1
#define _CrtSetDbgFlag(f)            0
#define _CrtDumpMemoryLeaks()        0
#define _CRTDBG_ALLOC_MEM_DF         0
#define _CRTDBG_LEAK_CHECK_DF        0
#define _NORMAL_BLOCK                1
#endif
