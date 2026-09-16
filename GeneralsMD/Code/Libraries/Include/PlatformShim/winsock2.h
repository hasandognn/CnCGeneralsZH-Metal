/* winsock2.h - Berkeley sockets under Microsoft's names.
**
** Winsock is the Berkeley API with a different spelling for four things: the handle type, the
** startup and cleanup calls, the way you close a socket, and where the error code lives.  The rest
** - socket, bind, connect, send, recv, select, and every sockaddr - is the same and comes from the
** system headers below.
*/
#ifndef WIN32COMPAT_SHIM_WINSOCK2_H
#define WIN32COMPAT_SHIM_WINSOCK2_H

#include "Platform/Win32Compat.h"

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

typedef int SOCKET;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)

typedef struct sockaddr     SOCKADDR;
typedef struct sockaddr_in  SOCKADDR_IN;
typedef struct sockaddr *   LPSOCKADDR;
typedef struct hostent      HOSTENT;
typedef struct hostent *    LPHOSTENT;

struct WSADATA {
	WORD  wVersion, wHighVersion;
	char  szDescription[257];
	char  szSystemStatus[129];
	unsigned short iMaxSockets, iMaxUdpDg;
	char *lpVendorInfo;
};
typedef WSADATA * LPWSADATA;

#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | (((WORD)((BYTE)(b))) << 8)))

// There is no library to start or stop; sockets are always available.
inline int WSAStartup(WORD, LPWSADATA data) { if (data) *data = WSADATA{}; return 0; }
inline int WSACleanup(void)                 { return 0; }

inline int closesocket(SOCKET s)            { return ::close(s); }
inline int WSAGetLastError(void)            { return errno; }
inline void WSASetLastError(int e)          { errno = e; }

// ioctlsocket's one use is putting a socket in non-blocking mode.
#define FIONBIO_WIN 0x8004667E
inline int ioctlsocket(SOCKET s, long cmd, unsigned long * argp)
{
	if (argp == nullptr) return -1;
	int flags = ::fcntl(s, F_GETFL, 0);
	if (flags < 0) return -1;
	if (*argp != 0) flags |= O_NONBLOCK; else flags &= ~O_NONBLOCK;
	return ::fcntl(s, F_SETFL, flags);
}

#define WSAEWOULDBLOCK   EWOULDBLOCK
#define WSAEINPROGRESS   EINPROGRESS
#define WSAECONNRESET    ECONNRESET
#define WSAECONNREFUSED  ECONNREFUSED
#define WSAETIMEDOUT     ETIMEDOUT
#define WSAEADDRINUSE    EADDRINUSE
#define WSAEINTR         EINTR

#endif
