/* io.h - the low-level file calls MSVC keeps here. */
#ifndef WIN32COMPAT_SHIM_IO_H
#define WIN32COMPAT_SHIM_IO_H
#include <unistd.h>
#include <fcntl.h>

#define _access  access
#define _open    open
#define _close   close
#define _read    read
#define _write   write
#define _lseek   lseek
#define _unlink  unlink
#endif
