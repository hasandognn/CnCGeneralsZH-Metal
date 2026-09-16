# PlatformShim

Headers named the way MSVC names them, so the ~170 files that write `#include <windows.h>` or
`#include <process.h>` need no edit at all.  This directory goes on the include path only off
Windows; none of these names collides with a macOS system header, which is why the trick is safe
here (`malloc.h` is `<malloc/malloc.h>` on this platform, and there is no `windows.h`).

The substance lives in `../Platform/Win32Compat.h`.  These files are thin.
