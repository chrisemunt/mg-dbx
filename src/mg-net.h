/*
   ----------------------------------------------------------------------------
   | mg-dbx.node                                                              |
   | Author: Chris Munt cmunt@mgateway.com                                    |
   |                    chris.e.munt@gmail.com                                |
   | Copyright (c) 2019-2023 MGateway Ltd                                     |
   | Surrey UK.                                                               |
   | All rights reserved.                                                     |
   |                                                                          |
   | http://www.mgateway.com                                                  |
   |                                                                          |
   | Licensed under the Apache License, Version 2.0 (the "License"); you may  |
   | not use this file except in compliance with the License.                 |
   | You may obtain a copy of the License at                                  |
   |                                                                          |
   | http://www.apache.org/licenses/LICENSE-2.0                               |
   |                                                                          |
   | Unless required by applicable law or agreed to in writing, software      |
   | distributed under the License is distributed on an "AS IS" BASIS,        |
   | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
   | See the License for the specific language governing permissions and      |
   | limitations under the License.                                           |
   |                                                                          |
   ----------------------------------------------------------------------------
*/

#ifndef MG_NET_H
#define MG_NET_H

#define NETX_TIMEOUT             10
#define NETX_IPV6                1
#define NETX_READ_EOF            0
#define NETX_READ_NOCON          -1
#define NETX_READ_ERROR          -2
#define NETX_READ_TIMEOUT        -3
#define NETX_RECV_BUFFER         32768


#if defined(LINUX)
#define NETX_MEMCPY(a,b,c)       memmove(a,b,c)
#else
#define NETX_MEMCPY(a,b,c)       memcpy(a,b,c)
#endif

/*
typedef void * (* MG_MALLOC)     (unsigned long size);
typedef void * (* MG_REALLOC)    (void *p, unsigned long size);
typedef int    (* MG_FREE)       (void *p);
*/

#if defined(_WIN32)

#if defined(MG_DBA_DSO)
#define DBX_EXTFUN(a)    __declspec(dllexport) a __cdecl
#else
#define DBX_EXTFUN(a)    a
#endif

#define NETX_WSASOCKET               netx_so.p_WSASocket
#define NETX_WSAGETLASTERROR         netx_so.p_WSAGetLastError
#define NETX_WSASTARTUP              netx_so.p_WSAStartup
#define NETX_WSACLEANUP              netx_so.p_WSACleanup
#define NETX_WSAFDISET               netx_so.p_WSAFDIsSet
#define NETX_WSARECV                 netx_so.p_WSARecv
#define NETX_WSASEND                 netx_so.p_WSASend

#define NETX_WSASTRINGTOADDRESS      netx_so.p_WSAStringToAddress
#define NETX_WSAADDRESSTOSTRING      netx_so.p_WSAAddressToString
#define NETX_GETADDRINFO             netx_so.p_getaddrinfo
#define NETX_FREEADDRINFO            netx_so.p_freeaddrinfo
#define NETX_GETNAMEINFO             netx_so.p_getnameinfo
#define NETX_GETPEERNAME             netx_so.p_getpeername
#define NETX_INET_NTOP               netx_so.p_inet_ntop
#define NETX_INET_PTON               netx_so.p_inet_pton

#define NETX_CLOSESOCKET             netx_so.p_closesocket
#define NETX_GETHOSTNAME             netx_so.p_gethostname
#define NETX_GETHOSTBYNAME           netx_so.p_gethostbyname
#define NETX_SETSERVBYNAME           netx_so.p_getservbyname
#define NETX_GETHOSTBYADDR           netx_so.p_gethostbyaddr
#define NETX_HTONS                   netx_so.p_htons
#define NETX_HTONL                   netx_so.p_htonl
#define NETX_NTOHL                   netx_so.p_ntohl
#define NETX_NTOHS                   netx_so.p_ntohs
#define NETX_CONNECT                 netx_so.p_connect
#define NETX_INET_ADDR               netx_so.p_inet_addr
#define NETX_INET_NTOA               netx_so.p_inet_ntoa
#define NETX_SOCKET                  netx_so.p_socket
#define NETX_SETSOCKOPT              netx_so.p_setsockopt
#define NETX_GETSOCKOPT              netx_so.p_getsockopt
#define NETX_GETSOCKNAME             netx_so.p_getsockname
#define NETX_SELECT                  netx_so.p_select
#define NETX_RECV                    netx_so.p_recv
#define NETX_SEND                    netx_so.p_send
#define NETX_SHUTDOWN                netx_so.p_shutdown
#define NETX_BIND                    netx_so.p_bind
#define NETX_LISTEN                  netx_so.p_listen
#define NETX_ACCEPT                  netx_so.p_accept

#define  NETX_FD_ISSET(fd, set)              netx_so.p_WSAFDIsSet((SOCKET)(fd), (fd_set *)(set))

typedef int (WINAPI * MG_LPFN_WSAFDISSET)       (SOCKET, fd_set *);

typedef DWORD           DBXTHID;
typedef HINSTANCE       DBXPLIB;
typedef FARPROC         DBXPROC;

typedef LPSOCKADDR      xLPSOCKADDR;
typedef u_long          *xLPIOCTL;
typedef const char      *xLPSENDBUF;
typedef char            *xLPRECVBUF;

#ifdef _WIN64
typedef int             socklen_netx;
#else
typedef size_t          socklen_netx;
#endif

#define SOCK_ERROR(n)   (n == SOCKET_ERROR)
#define INVALID_SOCK(n) (n == INVALID_SOCKET)
#define NOT_BLOCKING(n) (n != WSAEWOULDBLOCK)

#define BZERO(b,len) (memset((b), '\0', (len)), (void) 0)

#else /* #if defined(_WIN32) */

#define DBX_EXTFUN(a)    a

#define NETX_WSASOCKET               WSASocket
#define NETX_WSAGETLASTERROR         WSAGetLastError
#define NETX_WSASTARTUP              WSAStartup
#define NETX_WSACLEANUP              WSACleanup
#define NETX_WSAFDIsSet              WSAFDIsSet
#define NETX_WSARECV                 WSARecv
#define NETX_WSASEND                 WSASend

#define NETX_WSASTRINGTOADDRESS      WSAStringToAddress
#define NETX_WSAADDRESSTOSTRING      WSAAddressToString
#define NETX_GETADDRINFO             getaddrinfo
#define NETX_FREEADDRINFO            freeaddrinfo
#define NETX_GETNAMEINFO             getnameinfo
#define NETX_GETPEERNAME             getpeername
#define NETX_INET_NTOP               inet_ntop
#define NETX_INET_PTON               inet_pton

#define NETX_CLOSESOCKET             closesocket
#define NETX_GETHOSTNAME             gethostname
#define NETX_GETHOSTBYNAME           gethostbyname
#define NETX_SETSERVBYNAME           getservbyname
#define NETX_GETHOSTBYADDR           gethostbyaddr
#define NETX_HTONS                   htons
#define NETX_HTONL                   htonl
#define NETX_NTOHL                   ntohl
#define NETX_NTOHS                   ntohs
#define NETX_CONNECT                 connect
#define NETX_INET_ADDR               inet_addr
#define NETX_INET_NTOA               inet_ntoa
#define NETX_SOCKET                  socket
#define NETX_SETSOCKOPT              setsockopt
#define NETX_GETSOCKOPT              getsockopt
#define NETX_GETSOCKNAME             getsockname
#define NETX_SELECT                  select
#define NETX_RECV                    recv
#define NETX_SEND                    send
#define NETX_SHUTDOWN                shutdown
#define NETX_BIND                    bind
#define NETX_LISTEN                  listen
#define NETX_ACCEPT                  accept

#define NETX_FD_ISSET(fd, set) FD_ISSET(fd, set)

typedef int             WSADATA;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr * LPSOCKADDR;
typedef struct hostent  HOSTENT;
typedef struct hostent  * LPHOSTENT;
typedef struct servent  SERVENT;
typedef struct servent  * LPSERVENT;

#ifdef NETX_BS_GEN_PTR
typedef const void      * xLPSOCKADDR;
typedef void            * xLPIOCTL;
typedef const void      * xLPSENDBUF;
typedef void            * xLPRECVBUF;
#else
typedef LPSOCKADDR      xLPSOCKADDR;
typedef char            * xLPIOCTL;
typedef const char      * xLPSENDBUF;
typedef char            * xLPRECVBUF;
#endif /* #ifdef NETX_BS_GEN_PTR */

#if defined(OSF1) || defined(HPUX) || defined(HPUX10) || defined(HPUX11)
typedef int             socklen_netx;
#elif defined(LINUX) || defined(AIX) || defined(AIX5) || defined(MACOSX)
typedef socklen_t       socklen_netx;
#else
typedef size_t          socklen_netx;
#endif

#ifndef INADDR_NONE
#define INADDR_NONE     -1
#endif

#define SOCK_ERROR(n)   (n < 0)
#define INVALID_SOCK(n) (n < 0)
#define NOT_BLOCKING(n) (n != EWOULDBLOCK && n != 2)

#define BZERO(b, len)   (bzero(b, len))

#endif /* #if defined(_WIN32) */



#if defined(__MINGW32__)

typedef INT                   (WSAAPI * LPFN_INET_PTONA)             (INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
#define LPFN_INET_PTON        LPFN_INET_PTONA
typedef PCSTR                 (WSAAPI * LPFN_INET_NTOPA)             (INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
#define LPFN_INET_NTOP        LPFN_INET_NTOPA

#endif


#if defined(_WIN32)
#if defined(MG_USE_MS_TYPEDEFS)

typedef LPFN_WSASOCKET                MG_LPFN_WSASOCKET;
typedef LPFN_WSAGETLASTERROR          MG_LPFN_WSAGETLASTERROR; 
typedef LPFN_WSASTARTUP               MG_LPFN_WSASTARTUP;
typedef LPFN_WSACLEANUP               MG_LPFN_WSACLEANUP;
typedef LPFN_WSARECV                  MG_LPFN_WSARECV;
typedef LPFN_WSASEND                  MG_LPFN_WSASEND;

#if defined(NETX_IPV6)
typedef LPFN_WSASTRINGTOADDRESS       MG_LPFN_WSASTRINGTOADDRESS;
typedef LPFN_WSAADDRESSTOSTRING       MG_LPFN_WSAADDRESSTOSTRING;
typedef LPFN_GETADDRINFO              MG_LPFN_GETADDRINFO;
typedef LPFN_FREEADDRINFO             MG_LPFN_FREEADDRINFO;
typedef LPFN_GETNAMEINFO              MG_LPFN_GETNAMEINFO;
typedef LPFN_GETPEERNAME              MG_LPFN_GETPEERNAME;
typedef LPFN_INET_NTOP                MG_LPFN_INET_NTOP;
typedef LPFN_INET_PTON                MG_LPFN_INET_PTON;
#endif

typedef LPFN_CLOSESOCKET              MG_LPFN_CLOSESOCKET;
typedef LPFN_GETHOSTNAME              MG_LPFN_GETHOSTNAME;
typedef LPFN_GETHOSTBYNAME            MG_LPFN_GETHOSTBYNAME;
typedef LPFN_GETHOSTBYADDR            MG_LPFN_GETHOSTBYADDR;
typedef LPFN_GETSERVBYNAME            MG_LPFN_GETSERVBYNAME;

typedef LPFN_HTONS                    MG_LPFN_HTONS;
typedef LPFN_HTONL                    MG_LPFN_HTONL;
typedef LPFN_NTOHL                    MG_LPFN_NTOHL;
typedef LPFN_NTOHS                    MG_LPFN_NTOHS;
typedef LPFN_CONNECT                  MG_LPFN_CONNECT;
typedef LPFN_INET_ADDR                MG_LPFN_INET_ADDR;
typedef LPFN_INET_NTOA                MG_LPFN_INET_NTOA;

typedef LPFN_SOCKET                   MG_LPFN_SOCKET;
typedef LPFN_SETSOCKOPT               MG_LPFN_SETSOCKOPT;
typedef LPFN_GETSOCKOPT               MG_LPFN_GETSOCKOPT;
typedef LPFN_GETSOCKNAME              MG_LPFN_GETSOCKNAME;
typedef LPFN_SELECT                   MG_LPFN_SELECT;
typedef LPFN_RECV                     MG_LPFN_RECV;
typedef LPFN_SEND                     MG_LPFN_SEND;
typedef LPFN_SHUTDOWN                 MG_LPFN_SHUTDOWN;
typedef LPFN_BIND                     MG_LPFN_BIND;
typedef LPFN_LISTEN                   MG_LPFN_LISTEN;
typedef LPFN_ACCEPT                   MG_LPFN_ACCEPT;

#else

typedef int                   (WSAAPI * MG_LPFN_WSASTARTUP)          (WORD wVersionRequested, LPWSADATA lpWSAData);
typedef int                   (WSAAPI * MG_LPFN_WSACLEANUP)          (void);
typedef int                   (WSAAPI * MG_LPFN_WSAGETLASTERROR)     (void);
typedef SOCKET                (WSAAPI * MG_LPFN_WSASOCKET)           (int af, int type, int protocol, LPWSAPROTOCOL_INFOA lpProtocolInfo, GROUP g, DWORD dwFlags);
typedef int                   (WSAAPI * MG_LPFN_WSARECV)             (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int                   (WSAAPI * MG_LPFN_WSASEND)             (SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef INT                   (WSAAPI * MG_LPFN_WSASTRINGTOADDRESS)  (LPSTR AddressString, INT AddressFamily, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSOCKADDR lpAddress, LPINT lpAddressLength);
typedef INT                   (WSAAPI * MG_LPFN_WSAADDRESSTOSTRING)  (LPSOCKADDR lpsaAddress, DWORD dwAddressLength, LPWSAPROTOCOL_INFOA lpProtocolInfo, LPSTR lpszAddressString, LPDWORD lpdwAddressStringLength);
typedef INT                   (WSAAPI * MG_LPFN_GETADDRINFO)         (PCSTR pNodeName, PCSTR pServiceName, const ADDRINFOA * pHints, PADDRINFOA * ppResult);
typedef VOID                  (WSAAPI * MG_LPFN_FREEADDRINFO)        (PADDRINFOA pAddrInfo);
typedef int                   (WSAAPI * MG_LPFN_GETNAMEINFO)         (const SOCKADDR * pSockaddr, socklen_t SockaddrLength, PCHAR pNodeBuffer, DWORD NodeBufferSize, PCHAR pServiceBuffer, DWORD ServiceBufferSize, INT Flags);
typedef int                   (WSAAPI * MG_LPFN_GETPEERNAME)         (SOCKET s, struct sockaddr FAR * name, int FAR * namelen);
typedef PCSTR                 (WSAAPI * MG_LPFN_INET_NTOP)           (INT Family, PVOID pAddr, PSTR pStringBuf, size_t StringBufSize);
typedef INT                   (WSAAPI * MG_LPFN_INET_PTON)           (INT Family, PCSTR pszAddrString, PVOID pAddrBuf);
typedef int                   (WSAAPI * MG_LPFN_CLOSESOCKET)         (SOCKET s);
typedef int                   (WSAAPI * MG_LPFN_GETHOSTNAME)         (char FAR * name, int namelen);
typedef struct hostent FAR *  (WSAAPI * MG_LPFN_GETHOSTBYNAME)       (const char FAR * name);
typedef struct hostent FAR *  (WSAAPI * MG_LPFN_GETHOSTBYADDR)       (const char FAR * addr, int len, int type);
typedef struct servent FAR *  (WSAAPI * MG_LPFN_GETSERVBYNAME)       (const char FAR * name, const char FAR * proto);
typedef u_short               (WSAAPI * MG_LPFN_HTONS)               (u_short hostshort);
typedef u_long                (WSAAPI * MG_LPFN_HTONL)               (u_long hostlong);
typedef u_long                (WSAAPI * MG_LPFN_NTOHL)               (u_long netlong);
typedef u_short               (WSAAPI * MG_LPFN_NTOHS)               (u_short netshort);
typedef char FAR *            (WSAAPI * MG_LPFN_INET_NTOA)           (struct in_addr in);
typedef int                   (WSAAPI * MG_LPFN_CONNECT)             (SOCKET s, const struct sockaddr FAR * name, int namelen);
typedef unsigned long         (WSAAPI * MG_LPFN_INET_ADDR)           (const char FAR * cp);
typedef SOCKET                (WSAAPI * MG_LPFN_SOCKET)              (int af, int type, int protocol);
typedef int                   (WSAAPI * MG_LPFN_SETSOCKOPT)          (SOCKET s, int level, int optname, const char FAR * optval, int optlen);
typedef int                   (WSAAPI * MG_LPFN_GETSOCKOPT)          (SOCKET s, int level, int optname, char FAR * optval, int FAR * optlen);
typedef int                   (WSAAPI * MG_LPFN_GETSOCKNAME)         (SOCKET s, struct sockaddr FAR * name, int FAR * namelen);
typedef int                   (WSAAPI * MG_LPFN_SELECT)              (int nfds, fd_set FAR * readfds, fd_set FAR * writefds, fd_set FAR *exceptfds, const struct timeval FAR * timeout);
typedef int                   (WSAAPI * MG_LPFN_RECV)                (SOCKET s, char FAR * buf, int len, int flags);
typedef int                   (WSAAPI * MG_LPFN_SEND)                (SOCKET s, const char FAR * buf, int len, int flags);
typedef int                   (WSAAPI * MG_LPFN_SHUTDOWN)            (SOCKET s, int how);
typedef int                   (WSAAPI * MG_LPFN_BIND)                (SOCKET s, const struct sockaddr FAR * name, int namelen);
typedef int                   (WSAAPI * MG_LPFN_LISTEN)              (SOCKET s, int backlog);
typedef SOCKET                (WSAAPI * MG_LPFN_ACCEPT)              (SOCKET s, struct sockaddr FAR * addr, int FAR * addrlen);

#endif
#endif /* #if defined(_WIN32) */

typedef struct tagNETXSOCK {

   unsigned char                 winsock_ready;
   short                         sock;
   short                         load_attempted;
   short                         nagle_algorithm;
   short                         winsock;
   short                         ipv6;
   DBXPLIB                       plibrary;

   char                          libnam[256];

#if defined(_WIN32)
   WSADATA                       wsadata;
   int                           wsastartup;
   WORD                          version_requested;
   MG_LPFN_WSASOCKET                p_WSASocket;
   MG_LPFN_WSAGETLASTERROR          p_WSAGetLastError; 
   MG_LPFN_WSASTARTUP               p_WSAStartup;
   MG_LPFN_WSACLEANUP               p_WSACleanup;
   MG_LPFN_WSAFDISSET               p_WSAFDIsSet;
   MG_LPFN_WSARECV                  p_WSARecv;
   MG_LPFN_WSASEND                  p_WSASend;

#if defined(NETX_IPV6)
   MG_LPFN_WSASTRINGTOADDRESS       p_WSAStringToAddress;
   MG_LPFN_WSAADDRESSTOSTRING       p_WSAAddressToString;
   MG_LPFN_GETADDRINFO              p_getaddrinfo;
   MG_LPFN_FREEADDRINFO             p_freeaddrinfo;
   MG_LPFN_GETNAMEINFO              p_getnameinfo;
   MG_LPFN_GETPEERNAME              p_getpeername;
   MG_LPFN_INET_NTOP                p_inet_ntop;
   MG_LPFN_INET_PTON                p_inet_pton;
#else
   LPVOID                        p_WSAStringToAddress;
   LPVOID                        p_WSAAddressToString;
   LPVOID                        p_getaddrinfo;
   LPVOID                        p_freeaddrinfo;
   LPVOID                        p_getnameinfo;
   LPVOID                        p_getpeername;
   LPVOID                        p_inet_ntop;
   LPVOID                        p_inet_pton;
#endif

   MG_LPFN_CLOSESOCKET              p_closesocket;
   MG_LPFN_GETHOSTNAME              p_gethostname;
   MG_LPFN_GETHOSTBYNAME            p_gethostbyname;
   MG_LPFN_GETHOSTBYADDR            p_gethostbyaddr;
   MG_LPFN_GETSERVBYNAME            p_getservbyname;

   MG_LPFN_HTONS                    p_htons;
   MG_LPFN_HTONL                    p_htonl;
   MG_LPFN_NTOHL                    p_ntohl;
   MG_LPFN_NTOHS                    p_ntohs;
   MG_LPFN_CONNECT                  p_connect;
   MG_LPFN_INET_ADDR                p_inet_addr;
   MG_LPFN_INET_NTOA                p_inet_ntoa;

   MG_LPFN_SOCKET                   p_socket;
   MG_LPFN_SETSOCKOPT               p_setsockopt;
   MG_LPFN_GETSOCKOPT               p_getsockopt;
   MG_LPFN_GETSOCKNAME              p_getsockname;
   MG_LPFN_SELECT                   p_select;
   MG_LPFN_RECV                     p_recv;
   MG_LPFN_SEND                     p_send;
   MG_LPFN_SHUTDOWN                 p_shutdown;
   MG_LPFN_BIND                     p_bind;
   MG_LPFN_LISTEN                   p_listen;
   MG_LPFN_ACCEPT                   p_accept;
#endif /* #if defined(_WIN32) */

} NETXSOCK, *PNETXSOCK;


int                     netx_load_winsock             (DBXCON *pcon, int context);
int                     netx_tcp_connect              (DBXCON *pcon, int context);
int                     netx_tcp_handshake            (DBXCON *pcon, int context);
int                     netx_tcp_command              (DBXMETH *pmeth, int command, int context);
int                     netx_tcp_connect_ex           (DBXCON *pcon, xLPSOCKADDR p_srv_addr, socklen_netx srv_addr_len, int timeout);
int                     netx_tcp_disconnect           (DBXCON *pcon, int context);
int                     netx_tcp_write                (DBXCON *pcon, unsigned char *data, int size);
int                     netx_tcp_read                 (DBXCON *pcon, unsigned char *data, int size, int timeout, int context);
int                     netx_get_last_error           (int context);
int                     netx_get_error_message        (int error_code, char *message, int size, int context);
int                     netx_get_std_error_message    (int error_code, char *message, int size, int context);


#endif

