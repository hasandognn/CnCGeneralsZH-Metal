/* direct.h - MSVC's directory calls, which POSIX spells without the underscore. */
#ifndef WIN32COMPAT_SHIM_DIRECT_H
#define WIN32COMPAT_SHIM_DIRECT_H
#include <unistd.h>
#include <sys/stat.h>

inline int _mkdir(const char * path)              { return ::mkdir(path, 0755); }
inline int _rmdir(const char * path)              { return ::rmdir(path); }
inline int _chdir(const char * path)              { return ::chdir(path); }
inline char * _getcwd(char * buffer, int length)  { return ::getcwd(buffer, (size_t)length); }
#endif
