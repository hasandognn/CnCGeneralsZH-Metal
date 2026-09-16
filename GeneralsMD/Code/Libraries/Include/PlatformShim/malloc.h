/* malloc.h - MSVC put malloc and alloca here; POSIX splits them. */
#ifndef WIN32COMPAT_SHIM_MALLOC_H
#define WIN32COMPAT_SHIM_MALLOC_H
#include <cstdlib>
#include <alloca.h>
#define _alloca alloca
#endif
