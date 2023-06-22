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

#include "mg-dbx.h"
#include "mg-net.h"

#if !defined(_WIN32)
extern int errno;
#endif

static NETXSOCK      netx_so        = {0, 0, 0, 0, 0, 0, 0, {'\0'}};


int netx_load_winsock(DBXCON *pcon, int context)
{
#if defined(_WIN32)
   int result, mem_locked;
   char buffer[1024];

   result = 0;
   mem_locked = 0;
   *buffer = '\0';
   netx_so.version_requested = 0;

   if (netx_so.load_attempted) {
      return result;
   }

   if (netx_so.load_attempted) {
      goto netx_load_winsock_no_so;
   }

   netx_so.sock = 0;

   /* Try to Load the Winsock 2 library */

   netx_so.winsock = 2;
   strcpy(netx_so.libnam, "WS2_32.DLL");

   netx_so.plibrary = dbx_dso_load(netx_so.libnam);

   if (!netx_so.plibrary) {
      netx_so.winsock = 1;
      strcpy(netx_so.libnam, "WSOCK32.DLL");
      netx_so.plibrary = dbx_dso_load(netx_so.libnam);

      if (!netx_so.plibrary) {
         goto netx_load_winsock_no_so;
      }
   }

   netx_so.p_WSASocket             = (MG_LPFN_WSASOCKET)              dbx_dso_sym(netx_so.plibrary, "WSASocketA");
   netx_so.p_WSAGetLastError       = (MG_LPFN_WSAGETLASTERROR)        dbx_dso_sym(netx_so.plibrary, "WSAGetLastError");
   netx_so.p_WSAStartup            = (MG_LPFN_WSASTARTUP)             dbx_dso_sym(netx_so.plibrary, "WSAStartup");
   netx_so.p_WSACleanup            = (MG_LPFN_WSACLEANUP)             dbx_dso_sym(netx_so.plibrary, "WSACleanup");
   netx_so.p_WSAFDIsSet            = (MG_LPFN_WSAFDISSET)             dbx_dso_sym(netx_so.plibrary, "__WSAFDIsSet");
   netx_so.p_WSARecv               = (MG_LPFN_WSARECV)                dbx_dso_sym(netx_so.plibrary, "WSARecv");
   netx_so.p_WSASend               = (MG_LPFN_WSASEND)                dbx_dso_sym(netx_so.plibrary, "WSASend");

#if defined(NETX_IPV6)
   netx_so.p_WSAStringToAddress    = (MG_LPFN_WSASTRINGTOADDRESS)     dbx_dso_sym(netx_so.plibrary, "WSAStringToAddressA");
   netx_so.p_WSAAddressToString    = (MG_LPFN_WSAADDRESSTOSTRING)     dbx_dso_sym(netx_so.plibrary, "WSAAddressToStringA");
   netx_so.p_getaddrinfo           = (MG_LPFN_GETADDRINFO)            dbx_dso_sym(netx_so.plibrary, "getaddrinfo");
   netx_so.p_freeaddrinfo          = (MG_LPFN_FREEADDRINFO)           dbx_dso_sym(netx_so.plibrary, "freeaddrinfo");
   netx_so.p_getnameinfo           = (MG_LPFN_GETNAMEINFO)            dbx_dso_sym(netx_so.plibrary, "getnameinfo");
   netx_so.p_getpeername           = (MG_LPFN_GETPEERNAME)            dbx_dso_sym(netx_so.plibrary, "getpeername");
   netx_so.p_inet_ntop             = (MG_LPFN_INET_NTOP)              dbx_dso_sym(netx_so.plibrary, "InetNtop");
   netx_so.p_inet_pton             = (MG_LPFN_INET_PTON)              dbx_dso_sym(netx_so.plibrary, "InetPton");
#else
   netx_so.p_WSAStringToAddress    = NULL;
   netx_so.p_WSAAddressToString    = NULL;
   netx_so.p_getaddrinfo           = NULL;
   netx_so.p_freeaddrinfo          = NULL;
   netx_so.p_getnameinfo           = NULL;
   netx_so.p_getpeername           = NULL;
   netx_so.p_inet_ntop             = NULL;
   netx_so.p_inet_pton             = NULL;
#endif

   netx_so.p_closesocket           = (MG_LPFN_CLOSESOCKET)            dbx_dso_sym(netx_so.plibrary, "closesocket");
   netx_so.p_gethostname           = (MG_LPFN_GETHOSTNAME)            dbx_dso_sym(netx_so.plibrary, "gethostname");
   netx_so.p_gethostbyname         = (MG_LPFN_GETHOSTBYNAME)          dbx_dso_sym(netx_so.plibrary, "gethostbyname");
   netx_so.p_getservbyname         = (MG_LPFN_GETSERVBYNAME)          dbx_dso_sym(netx_so.plibrary, "getservbyname");
   netx_so.p_gethostbyaddr         = (MG_LPFN_GETHOSTBYADDR)          dbx_dso_sym(netx_so.plibrary, "gethostbyaddr");
   netx_so.p_htons                 = (MG_LPFN_HTONS)                  dbx_dso_sym(netx_so.plibrary, "htons");
   netx_so.p_htonl                 = (MG_LPFN_HTONL)                  dbx_dso_sym(netx_so.plibrary, "htonl");
   netx_so.p_ntohl                 = (MG_LPFN_NTOHL)                  dbx_dso_sym(netx_so.plibrary, "ntohl");
   netx_so.p_ntohs                 = (MG_LPFN_NTOHS)                  dbx_dso_sym(netx_so.plibrary, "ntohs");
   netx_so.p_connect               = (MG_LPFN_CONNECT)                dbx_dso_sym(netx_so.plibrary, "connect");
   netx_so.p_inet_addr             = (MG_LPFN_INET_ADDR)              dbx_dso_sym(netx_so.plibrary, "inet_addr");
   netx_so.p_inet_ntoa             = (MG_LPFN_INET_NTOA)              dbx_dso_sym(netx_so.plibrary, "inet_ntoa");

   netx_so.p_socket                = (MG_LPFN_SOCKET)                 dbx_dso_sym(netx_so.plibrary, "socket");
   netx_so.p_setsockopt            = (MG_LPFN_SETSOCKOPT)             dbx_dso_sym(netx_so.plibrary, "setsockopt");
   netx_so.p_getsockopt            = (MG_LPFN_GETSOCKOPT)             dbx_dso_sym(netx_so.plibrary, "getsockopt");
   netx_so.p_getsockname           = (MG_LPFN_GETSOCKNAME)            dbx_dso_sym(netx_so.plibrary, "getsockname");

   netx_so.p_select                = (MG_LPFN_SELECT)                 dbx_dso_sym(netx_so.plibrary, "select");
   netx_so.p_recv                  = (MG_LPFN_RECV)                   dbx_dso_sym(netx_so.plibrary, "recv");
   netx_so.p_send                  = (MG_LPFN_SEND)                   dbx_dso_sym(netx_so.plibrary, "send");
   netx_so.p_shutdown              = (MG_LPFN_SHUTDOWN)               dbx_dso_sym(netx_so.plibrary, "shutdown");
   netx_so.p_bind                  = (MG_LPFN_BIND)                   dbx_dso_sym(netx_so.plibrary, "bind");
   netx_so.p_listen                = (MG_LPFN_LISTEN)                 dbx_dso_sym(netx_so.plibrary, "listen");
   netx_so.p_accept                = (MG_LPFN_ACCEPT)                 dbx_dso_sym(netx_so.plibrary, "accept");

   if (   (netx_so.p_WSASocket              == NULL && netx_so.winsock == 2)
       ||  netx_so.p_WSAGetLastError        == NULL
       ||  netx_so.p_WSAStartup             == NULL
       ||  netx_so.p_WSACleanup             == NULL
       ||  netx_so.p_WSAFDIsSet             == NULL
       || (netx_so.p_WSARecv                == NULL && netx_so.winsock == 2)
       || (netx_so.p_WSASend                == NULL && netx_so.winsock == 2)

#if defined(NETX_IPV6)
       || (netx_so.p_WSAStringToAddress     == NULL && netx_so.winsock == 2)
       || (netx_so.p_WSAAddressToString     == NULL && netx_so.winsock == 2)
       ||  netx_so.p_getpeername            == NULL
#endif

       ||  netx_so.p_closesocket            == NULL
       ||  netx_so.p_gethostname            == NULL
       ||  netx_so.p_gethostbyname          == NULL
       ||  netx_so.p_getservbyname          == NULL
       ||  netx_so.p_gethostbyaddr          == NULL
       ||  netx_so.p_htons                  == NULL
       ||  netx_so.p_htonl                  == NULL
       ||  netx_so.p_ntohl                  == NULL
       ||  netx_so.p_ntohs                  == NULL
       ||  netx_so.p_connect                == NULL
       ||  netx_so.p_inet_addr              == NULL
       ||  netx_so.p_inet_ntoa              == NULL
       ||  netx_so.p_socket                 == NULL
       ||  netx_so.p_setsockopt             == NULL
       ||  netx_so.p_getsockopt             == NULL
       ||  netx_so.p_getsockname            == NULL
       ||  netx_so.p_select                 == NULL
       ||  netx_so.p_recv                   == NULL
       ||  netx_so.p_send                   == NULL
       ||  netx_so.p_shutdown               == NULL
       ||  netx_so.p_bind                   == NULL
       ||  netx_so.p_listen                 == NULL
       ||  netx_so.p_accept                 == NULL
      ) {

      sprintf(buffer, "Cannot use Winsock library (WSASocket=%p; WSAGetLastError=%p; WSAStartup=%p; WSACleanup=%p; WSAFDIsSet=%p; WSARecv=%p; WSASend=%p; WSAStringToAddress=%p; WSAAddressToString=%p; closesocket=%p; gethostname=%p; gethostbyname=%p; getservbyname=%p; gethostbyaddr=%p; getaddrinfo=%p; freeaddrinfo=%p; getnameinfo=%p; getpeername=%p; htons=%p; htonl=%p; ntohl=%p; ntohs=%p; connect=%p; inet_addr=%p; inet_ntoa=%p; socket=%p; setsockopt=%p; getsockopt=%p; getsockname=%p; select=%p; recv=%p; p_send=%p; shutdown=%p; bind=%p; listen=%p; accept=%p;)",
            netx_so.p_WSASocket,
            netx_so.p_WSAGetLastError,
            netx_so.p_WSAStartup,
            netx_so.p_WSACleanup,
            netx_so.p_WSAFDIsSet,
            netx_so.p_WSARecv,
            netx_so.p_WSASend,

            netx_so.p_WSAStringToAddress,
            netx_so.p_WSAAddressToString,

            netx_so.p_closesocket,
            netx_so.p_gethostname,
            netx_so.p_gethostbyname,
            netx_so.p_getservbyname,
            netx_so.p_gethostbyaddr,

            netx_so.p_getaddrinfo,
            netx_so.p_freeaddrinfo,
            netx_so.p_getnameinfo,
            netx_so.p_getpeername,

            netx_so.p_htons,
            netx_so.p_htonl,
            netx_so.p_ntohl,
            netx_so.p_ntohs,
            netx_so.p_connect,
            netx_so.p_inet_addr,
            netx_so.p_inet_ntoa,
            netx_so.p_socket,
            netx_so.p_setsockopt,
            netx_so.p_getsockopt,
            netx_so.p_getsockname,
            netx_so.p_select,
            netx_so.p_recv,
            netx_so.p_send,
            netx_so.p_shutdown,
            netx_so.p_bind,
            netx_so.p_listen,
            netx_so.p_accept
            );
      dbx_dso_unload((DBXPLIB) netx_so.plibrary);
   }
   else {
      netx_so.sock = 1;
   }

   if (netx_so.sock)
      result = 0;
   else
      result = -1;

   netx_so.load_attempted = 1;

   if (netx_so.p_getaddrinfo == NULL ||  netx_so.p_freeaddrinfo == NULL ||  netx_so.p_getnameinfo == NULL)
      netx_so.ipv6 = 0;

netx_load_winsock_no_so:

   if (result == 0) {

      if (netx_so.winsock == 2)
         netx_so.version_requested = MAKEWORD(2, 2);
      else
         netx_so.version_requested = MAKEWORD(1, 1);

      netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));

      if (netx_so.wsastartup != 0 && netx_so.winsock == 2) {
         netx_so.version_requested = MAKEWORD(2, 0);
         netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));
         if (netx_so.wsastartup != 0) {
            netx_so.winsock = 1;
            netx_so.version_requested = MAKEWORD(1, 1);
            netx_so.wsastartup = NETX_WSASTARTUP(netx_so.version_requested, &(netx_so.wsadata));
         }
      }
      if (netx_so.wsastartup == 0) {
         if ((netx_so.winsock == 2 && LOBYTE(netx_so.wsadata.wVersion) != 2)
               || (netx_so.winsock == 1 && (LOBYTE(netx_so.wsadata.wVersion) != 1 || HIBYTE(netx_so.wsadata.wVersion) != 1))) {
  
            sprintf(pcon->error, "Initialization Error: Wrong version of Winsock library (%s) (%d.%d)", netx_so.libnam, LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            NETX_WSACLEANUP();
            netx_so.wsastartup = -1;
         }
         else {
            if (strlen(netx_so.libnam))
               sprintf(pcon->info, "Initialization: Windows Sockets library loaded (%s) Version: %d.%d", netx_so.libnam, LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            else
               sprintf(pcon->info, "Initialization: Windows Sockets library Version: %d.%d", LOBYTE(netx_so.wsadata.wVersion), HIBYTE(netx_so.wsadata.wVersion));
            netx_so.winsock_ready = 1;
         }
      }
      else {
         strcpy(pcon->error, "Initialization Error: Unusable Winsock library");
      }
   }

   return result;

#else

   return 1;

#endif /* #if defined(_WIN32) */

}


int netx_tcp_connect(DBXCON *pcon, int context)
{
   short physical_ip, ipv6, connected, getaddrinfo_ok;
   int n, errorno;
   unsigned long inetaddr;
   DWORD spin_count;
   char net_host[64];
   struct sockaddr_in srv_addr, cli_addr;
   struct hostent *hp;
   struct in_addr **pptr;

   pcon->open = 0;
   pcon->error_code = 0;
   connected = 0;
   getaddrinfo_ok = 0;
   spin_count = 0;

   ipv6 = 1;
#if !defined(NETX_IPV6)
   ipv6 = 0;
#endif

   strcpy(net_host, (char *) pcon->net_host);

#if defined(_WIN32)

   if (!netx_so.load_attempted) {
      n = netx_load_winsock(pcon, 0);
      if (n != 0) {
         return CACHE_NOCON;
      }
   }
   if (!netx_so.winsock_ready) {
      strcpy(pcon->error, (char *) "DLL Load Error: Unusable Winsock Library");
      return CACHE_NOCON;
   }

   n = netx_so.wsastartup;
   if (n != 0) {
      strcpy(pcon->error, (char *) "DLL Load Error: Unusable Winsock Library");
      return n;
   }

#endif /* #if defined(_WIN32) */

#if defined(NETX_IPV6)

   if (ipv6) {
      short mode;
      struct addrinfo hints, *res;
      struct addrinfo *ai;
      char port_str[32];

      res = NULL;
      sprintf(port_str, "%d", pcon->tcp_port);
      connected = 0;
      pcon->error_code = 0;

      for (mode = 0; mode < 3; mode ++) {

         if (res) {
            NETX_FREEADDRINFO(res);
            res = NULL;
         }

         memset(&hints, 0, sizeof hints);
         hints.ai_family = AF_UNSPEC;     /* Use IPv4 or IPv6 */
         hints.ai_socktype = SOCK_STREAM;
         /* hints.ai_flags = AI_PASSIVE; */
         if (mode == 0)
            hints.ai_flags = AI_NUMERICHOST | AI_CANONNAME;
         else if (mode == 1)
            hints.ai_flags = AI_CANONNAME;
         else if (mode == 2) {
            /* Apparently an error can occur with AF_UNSPEC (See RJW1564) */
            /* This iteration will return IPV6 addresses if any */
            hints.ai_flags = AI_CANONNAME;
            hints.ai_family = AF_INET6;
         }
         else
            break;

         n = NETX_GETADDRINFO(net_host, port_str, &hints, &res);

         if (n != 0) {
            continue;
         }

         getaddrinfo_ok = 1;
         spin_count = 0;
         for (ai = res; ai != NULL; ai = ai->ai_next) {

            spin_count ++;

	         if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) {
               continue;
            }

	         /* Open a socket with the correct address family for this address. */
	         pcon->cli_socket = NETX_SOCKET(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

            /* NETX_BIND(pcon->cli_socket, ai->ai_addr, (int) (ai->ai_addrlen)); */
            /* NETX_CONNECT(pcon->cli_socket, ai->ai_addr, (int) (ai->ai_addrlen)); */

            if (netx_so.nagle_algorithm == 0) {

               int flag = 1;
               int result;

               result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));

               if (result < 0) {
                  strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");
               }

            }

            pcon->error_code = 0;
            n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) ai->ai_addr, (socklen_netx) (ai->ai_addrlen), pcon->timeout);
            if (n == -2) {
               pcon->error_code = n;
               n = -737;
               continue;
            }
            if (SOCK_ERROR(n)) {
               errorno = (int) netx_get_last_error(0);
               pcon->error_code = errorno;
               netx_tcp_disconnect(pcon, 0);
               continue;
            }
            else {
               connected = 1;
               break;
            }
         }
         if (connected)
            break;
      }

      if (pcon->error_code) {
         char message[256];
         netx_get_error_message(pcon->error_code, message, 250, 0);
         sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->net_host, pcon->tcp_port, pcon->error_code, message);
         n = -5;
      }

      if (res) {
         NETX_FREEADDRINFO(res);
         res = NULL;
      }
   }
#endif

   if (ipv6) {
      if (connected) {
         return 0;
      }
      else {
         if (getaddrinfo_ok) {
            netx_tcp_disconnect(pcon, 0);
            return -5;
         }
         else {
            char message[256];

            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Cannot identify Server: Error Code: %d (%s)", errorno, message);
            netx_tcp_disconnect(pcon, 0);
            return -5;
         }
      }
   }

   ipv6 = 0;
   inetaddr = NETX_INET_ADDR(net_host);

   physical_ip = 0;
   if (isdigit(net_host[0])) {
      char *p;

      if ((p = strstr(net_host, "."))) {
         if (isdigit(*(++ p))) {
            if ((p = strstr(p, "."))) {
               if (isdigit(*(++ p))) {
                  if ((p = strstr(p, "."))) {
                     if (isdigit(*(++ p))) {
                        physical_ip = 1;
                     }
                  }
               }
            }
         }
      }
   }

   if (inetaddr == INADDR_NONE || !physical_ip) {

      hp = NETX_GETHOSTBYNAME((const char *) net_host);

      if (hp == NULL) {
         n = -2;
         strcpy(pcon->error, "Connection Error: Invalid Host");
         return n;
      }

      pptr = (struct in_addr **) hp->h_addr_list;
      connected = 0;

      spin_count = 0;

      for (; *pptr != NULL; pptr ++) {

         spin_count ++;

         pcon->cli_socket = NETX_SOCKET(AF_INET, SOCK_STREAM, 0);

         if (INVALID_SOCK(pcon->cli_socket)) {
            char message[256];

            n = -2;
            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Invalid Socket: Context=1: Error Code: %d (%s)", errorno, message);
            break;
         }

#if !defined(_WIN32)
         BZERO((char *) &cli_addr, sizeof(cli_addr));
         BZERO((char *) &srv_addr, sizeof(srv_addr));
#endif

         cli_addr.sin_family = AF_INET;
         srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->tcp_port);

         cli_addr.sin_addr.s_addr = NETX_HTONL(INADDR_ANY);
         cli_addr.sin_port = NETX_HTONS(0);

         n = NETX_BIND(pcon->cli_socket, (xLPSOCKADDR) &cli_addr, sizeof(cli_addr));

         if (SOCK_ERROR(n)) {
            char message[256];

            n = -3;
            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "Connection Error: Cannot bind to Socket: Error Code: %d (%s)", errorno, message);

            break;
         }

         if (netx_so.nagle_algorithm == 0) {

            int flag = 1;
            int result;

            result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));
            if (result < 0) {
               strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");
            }
         }

         srv_addr.sin_family = AF_INET;
         srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->tcp_port);

         NETX_MEMCPY(&srv_addr.sin_addr, *pptr, sizeof(struct in_addr));

         n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) &srv_addr, sizeof(srv_addr), pcon->timeout);

         if (n == -2) {
            pcon->error_code = n;
            n = -737;

            continue;
         }

         if (SOCK_ERROR(n)) {
            char message[256];

            errorno = (int) netx_get_last_error(0);
            netx_get_error_message(errorno, message, 250, 0);

            pcon->error_code = errorno;
            sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->net_host, pcon->tcp_port, errorno, message);
            n = -5;
            netx_tcp_disconnect(pcon, 0);
            continue;
         }
         else {
            connected = 1;
            break;
         }
      }
      if (!connected) {

         netx_tcp_disconnect(pcon, 0);

         strcpy(pcon->error, "Connection Error: Failed to find the Server via a DNS Lookup");

         return n;
      }
   }
   else {

      pcon->cli_socket = NETX_SOCKET(AF_INET, SOCK_STREAM, 0);

      if (INVALID_SOCK(pcon->cli_socket)) {
         char message[256];

         n = -2;
         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);
         sprintf(pcon->error, "Connection Error: Invalid Socket: Context=2: Error Code: %d (%s)", errorno, message);

         return n;
      }

#if !defined(_WIN32)
      BZERO((char *) &cli_addr, sizeof(cli_addr));
      BZERO((char *) &srv_addr, sizeof(srv_addr));
#endif

      cli_addr.sin_family = AF_INET;
      cli_addr.sin_addr.s_addr = NETX_HTONL(INADDR_ANY);
      cli_addr.sin_port = NETX_HTONS(0);

      n = NETX_BIND(pcon->cli_socket, (xLPSOCKADDR) &cli_addr, sizeof(cli_addr));

      if (SOCK_ERROR(n)) {
         char message[256];

         n = -3;

         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);

         sprintf(pcon->error, "Connection Error: Cannot bind to Socket: Error Code: %d (%s)", errorno, message);

         netx_tcp_disconnect(pcon, 0);

         return n;
      }

      if (netx_so.nagle_algorithm == 0) {

         int flag = 1;
         int result;

         result = NETX_SETSOCKOPT(pcon->cli_socket, IPPROTO_TCP, TCP_NODELAY, (const char *) &flag, sizeof(int));

         if (result < 0) {
            strcpy(pcon->error, "Connection Error: Unable to disable the Nagle Algorithm");

         }
      }

      srv_addr.sin_port = NETX_HTONS((unsigned short) pcon->tcp_port);
      srv_addr.sin_family = AF_INET;
      srv_addr.sin_addr.s_addr = NETX_INET_ADDR(net_host);

      n = netx_tcp_connect_ex(pcon, (xLPSOCKADDR) &srv_addr, sizeof(srv_addr), pcon->timeout);
      if (n == -2) {
         pcon->error_code = n;
         n = -737;

         netx_tcp_disconnect(pcon, 0);

         return n;
      }

      if (SOCK_ERROR(n)) {
         char message[256];

         errorno = (int) netx_get_last_error(0);
         netx_get_error_message(errorno, message, 250, 0);
         pcon->error_code = errorno;
         sprintf(pcon->error, "Connection Error: Cannot Connect to Server (%s:%d): Error Code: %d (%s)", (char *) pcon->net_host, pcon->tcp_port, errorno, message);
         n = -5;
         netx_tcp_disconnect(pcon, 0);
         return n;
      }
   }

   return 0;
}


int netx_tcp_handshake(DBXCON *pcon, int context)
{
   int len;
   char buffer[256];

   sprintf(buffer, "dbx1~%s\n", pcon->nspace);
   len = (int) strlen(buffer);

   netx_tcp_write(pcon, (unsigned char *) buffer, len);
   len = netx_tcp_read(pcon, (unsigned char *) buffer, 5, DBX_DEFAULT_TIMEOUT, 1);

   len = dbx_get_size((unsigned char *) buffer);
 
   netx_tcp_read(pcon, (unsigned char *) buffer, len, DBX_DEFAULT_TIMEOUT, 1);
   if (pcon->dbtype != DBX_DBTYPE_YOTTADB) {
      isc_parse_zv(buffer, pcon->p_zv);
      T_SPRINTF(pcon->p_zv->version, _dbxso(pcon->p_zv->version), "%d.%d build %d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->dbx_build);
   }
   else {
      ydb_parse_zv(buffer, pcon->p_zv);
      if (pcon->p_zv->dbx_build)
         sprintf(pcon->p_zv->version, "%d.%d.b%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion, pcon->p_zv->dbx_build);
      else
         sprintf(pcon->p_zv->version, "%d.%d", pcon->p_zv->majorversion, pcon->p_zv->minorversion);
   }

   return 0;
}

int netx_tcp_command(DBXMETH *pmeth, int command, int context)
{
   int len, rc;
   unsigned int netbuf_used;
   unsigned char *netbuf;
   char *p;
   DBXCON *pcon = pmeth->pcon;

   rc = CACHE_SUCCESS;
   pcon->error[0] = '\0';

   dbx_add_block_size(pmeth->ibuffer, pmeth->ibuffer_used, 0,  DBX_DSORT_EOD, DBX_DTYPE_STR8);
   pmeth->ibuffer_used += 5;

   netbuf = (pmeth->ibuffer - DBX_IBUFFER_OFFSET);
   netbuf_used = (pmeth->ibuffer_used + DBX_IBUFFER_OFFSET);
   dbx_add_block_size(netbuf, 0, netbuf_used,  0, command);

   /* v2.4.26 */
   if (pcon->utf8 == 2)
      netbuf[9] = 255;
   else
      netbuf[9] = 0;


/*
   {
      char buffer[256];
      sprintf(buffer, "netx_tcp_command SEND cmnd=%d; size=%d; netbuf_used=%d;", command, pmeth->ibuffer_used, netbuf_used);
      dbx_log_buffer(pcon, (char *) netbuf, (int) netbuf_used, buffer, 0);
   }
*/

   rc = netx_tcp_write(pcon, (unsigned char *) netbuf, netbuf_used);
   if (rc < 0) { /* v2.2.21 */
      netx_tcp_disconnect(pcon, 0);
      return rc;
   }

   rc = netx_tcp_read(pcon, (unsigned char *) pmeth->output_val.svalue.buf_addr, 5, pcon->timeout, 1);
   pmeth->output_val.svalue.buf_addr[5] = '\0';

   if (rc < 0) { /* v2.2.21 */
      netx_tcp_disconnect(pcon, 0);
      return rc;
   }

   len = dbx_get_block_size((unsigned char *) pmeth->output_val.svalue.buf_addr, 0, &(pmeth->output_val.sort), &(pmeth->output_val.type));

   /* v2.2.22 */
   if (len >= (int) pmeth->output_val.svalue.len_alloc) {
      p = (char *) dbx_malloc(sizeof(char) * (len + 2), 301);

      if (p) {
         if (pmeth->output_val.svalue.buf_addr) {
            dbx_free((void *) pmeth->output_val.svalue.buf_addr, 301);
         }
         pmeth->output_val.svalue.buf_addr = (char *) p;
         pmeth->output_val.svalue.len_alloc = len;
      }
   }

   if (len > 0) {
      rc = netx_tcp_read(pcon, (unsigned char *) pmeth->output_val.svalue.buf_addr, len, pcon->timeout, 1);
      if (rc < 0) { /* v2.2.21 */
         netx_tcp_disconnect(pcon, 0);
         return rc;
      }
   }

   if (pmeth->output_val.type == DBX_DTYPE_OREF) {
      pmeth->output_val.svalue.buf_addr[len] = '\0';
      pmeth->output_val.num.oref = (int) strtol(pmeth->output_val.svalue.buf_addr, NULL, 10);
      pmeth->output_val.num.int32 = pmeth->output_val.num.oref;
   }

   rc = CACHE_SUCCESS;
   if (pmeth->output_val.sort == DBX_DSORT_ERROR) {
      rc = CACHE_FAILURE;
      if (len > 0) {
         strncpy(pcon->error, pmeth->output_val.svalue.buf_addr, len);
         pcon->error[len] = '\0';
         len = 0;
      }
   }
/*
   {
      char buffer[256];
      sprintf(buffer, "netx_tcp_command RECV cmnd=%d; len=%d; sort=%d; type=%d; oref=%d; rc=%d; error=%s;", command, len, pmeth->output_val.sort, pmeth->output_val.type, pmeth->output_val.num.oref, rc, pcon->error);
      dbx_log_buffer(pcon, pmeth->output_val.svalue.buf_addr, len, buffer, 0);
   }
*/
   pmeth->output_val.svalue.len_used = len;

   return rc;
}


int netx_tcp_connect_ex(DBXCON *pcon, xLPSOCKADDR p_srv_addr, socklen_netx srv_addr_len, int timeout)
{
#if defined(_WIN32)
   int n;
#else
   int flags, n, error;
   socklen_netx len;
   fd_set rset, wset;
   struct timeval tval;
#endif

#if defined(SOLARIS) && BIT64PLAT
   timeout = 0;
#endif

   /* It seems that BIT64PLAT is set to 0 for 64-bit Solaris:  So, to be safe .... */

#if defined(SOLARIS)
   timeout = 0;
#endif

   if (timeout != 0) {

#if defined(_WIN32)

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      return n;

#else
      flags = fcntl(pcon->cli_socket, F_GETFL, 0);
      n = fcntl(pcon->cli_socket, F_SETFL, flags | O_NONBLOCK);

      error = 0;

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      if (n < 0) {

         if (errno != EINPROGRESS) {

#if defined(SOLARIS)

            if (errno != 2 && errno != 146) {
               sprintf((char *) pcon->error, "Diagnostic: Solaris: Initial Connection Error errno=%d; EINPROGRESS=%d", errno, EINPROGRESS);
               return -1;
            }
#else
            return -1;
#endif

         }
      }

      if (n != 0) {

         FD_ZERO(&rset);
         FD_SET(pcon->cli_socket, &rset);

         wset = rset;
         tval.tv_sec = timeout;
         tval.tv_usec = timeout;

         n = NETX_SELECT((int) (pcon->cli_socket + 1), &rset, &wset, NULL, &tval);

         if (n == 0) {
            close(pcon->cli_socket);
            errno = ETIMEDOUT;

            return (-2);
         }
         if (NETX_FD_ISSET(pcon->cli_socket, &rset) || NETX_FD_ISSET(pcon->cli_socket, &wset)) {

            len = sizeof(error);
            if (NETX_GETSOCKOPT(pcon->cli_socket, SOL_SOCKET, SO_ERROR, (void *) &error, &len) < 0) {

               sprintf((char *) pcon->error, "Diagnostic: Solaris: Pending Error %d", errno);

               return (-1);   /* Solaris pending error */
            }
         }
         else {
            ;
         }
      }

      fcntl(pcon->cli_socket, F_SETFL, flags);      /* Restore file status flags */

      if (error) {

         close(pcon->cli_socket);
         errno = error;
         return (-1);
      }

      return 1;

#endif

   }
   else {

      n = NETX_CONNECT(pcon->cli_socket, (xLPSOCKADDR) p_srv_addr, (socklen_netx) srv_addr_len);

      return n;
   }

}


int netx_tcp_disconnect(DBXCON *pcon, int context)
{

   if (!pcon) {
      return 0;
   }

   if (pcon->cli_socket != (SOCKET) 0) {

#if defined(_WIN32)
      NETX_CLOSESOCKET(pcon->cli_socket);
/*
      NETX_WSACLEANUP();
*/

#else
      close(pcon->cli_socket);
#endif

   }

   pcon->open = 0;

   return 0;

}


int netx_tcp_write(DBXCON *pcon, unsigned char *data, int size)
{
   int n = 0, errorno = 0, char_sent = 0;
   int total;
   char errormessage[512];

   *errormessage = '\0';

   if (pcon->open == 0) {
      strcpy(pcon->error, "TCP Write Error: Socket is Closed");
      return -1;
   }

   total = 0;
   for (;;) {
      n = NETX_SEND(pcon->cli_socket, (xLPSENDBUF) (data + total), size - total, 0);

      if (SOCK_ERROR(n)) {

         errorno = (int) netx_get_last_error(0);

         if (NOT_BLOCKING(errorno) && errorno != 0) {

            char message[256];

            netx_get_error_message(errorno, message, 250, 0);
            sprintf(pcon->error, "TCP Write Error: Cannot Write Data: Error Code: %d (%s)", errorno, message);

            char_sent = -1;
            break;
         }
      }
      else {

         total += n;
         if (total == size) {
            break;
         }
      }
   }

   if (char_sent < 0)
      return char_sent;
   else
      return size;

}



int netx_tcp_read(DBXCON *pcon, unsigned char *data, int size, int timeout, int context)
{
   int result, n;
   int len;
   fd_set rset, eset;
   struct timeval tval;
   unsigned long spin_count;


   if (!pcon) {
      return NETX_READ_ERROR;
   }

   result = 0;

   tval.tv_sec = timeout;
   tval.tv_usec = 0;

   spin_count = 0;
   len = 0;
   for (;;) {
      spin_count ++;

      FD_ZERO(&rset);
      FD_ZERO(&eset);
      FD_SET(pcon->cli_socket, &rset);
      FD_SET(pcon->cli_socket, &eset);

      n = NETX_SELECT((int) (pcon->cli_socket + 1), &rset, NULL, &eset, &tval);

      if (n == 0) {
         sprintf(pcon->error, "TCP Read Error: Server did not respond within the timeout period (%d seconds)", timeout);
         result = NETX_READ_TIMEOUT;
         break;
      }

      if (n < 0 || !NETX_FD_ISSET(pcon->cli_socket, &rset)) {
          strcpy(pcon->error, "TCP Read Error: Server closed the connection without having returned any data");
          result = NETX_READ_ERROR;
         break;
      }

      n = NETX_RECV(pcon->cli_socket, (char *) data + len, size - len, 0);

      if (n < 1) {
         if (n == 0) {
            result = NETX_READ_EOF;
            pcon->open = 0;
            pcon->eof = 1;
         }
         else {
            result = NETX_READ_ERROR;
            len = 0;
            pcon->open = 0;
         }
         break;
      }

      len += n;
      if (context) { /* Must read length requested */
         if (len == size) {
            break;
         }
      }
      else {
         break;
      }
   }

   if (len) {
      result = len;
   }
   return result;
}



int netx_get_last_error(int context)
{
   int error_code;

#if defined(_WIN32)
   if (context)
      error_code = (int) GetLastError();
   else
      error_code = (int) NETX_WSAGETLASTERROR();
#else
   error_code = (int) errno;
#endif

   return error_code;
}


int netx_get_error_message(int error_code, char *message, int size, int context)
{
   *message = '\0';

#if defined(_WIN32)

   if (context == 0) {
      short ok;
      int len;
      char *p;
      LPVOID lpMsgBuf;

      ok = 0;
      lpMsgBuf = NULL;
      len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL,
                           error_code,
                           /* MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), */
                           MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                           (LPTSTR) &lpMsgBuf,
                           0,
                           NULL 
                           );
      if (len && lpMsgBuf) {
         strncpy(message, (const char *) lpMsgBuf, size);
         p = strstr(message, "\r\n");
         if (p)
            *p = '\0';
         ok = 1;
      }
      if (lpMsgBuf)
         LocalFree(lpMsgBuf);

      if (!ok) {
         switch (error_code) {
            case EXCEPTION_ACCESS_VIOLATION:
               strncpy(message, "The thread attempted to read from or write to a virtual address for which it does not have the appropriate access.", size);
               break;
            case EXCEPTION_BREAKPOINT:
               strncpy(message, "A breakpoint was encountered.", size); 
               break;
            case EXCEPTION_DATATYPE_MISALIGNMENT:
               strncpy(message, "The thread attempted to read or write data that is misaligned on hardware that does not provide alignment. For example, 16-bit values must be aligned on 2-byte boundaries, 32-bit values on 4-byte boundaries, and so on.", size);
               break;
            case EXCEPTION_SINGLE_STEP:
               strncpy(message, "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.", size);
               break;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
               strncpy(message, "The thread attempted to access an array element that is out of bounds, and the underlying hardware supports bounds checking.", size);
               break;
            case EXCEPTION_FLT_DENORMAL_OPERAND:
               strncpy(message, "One of the operands in a floating-point operation is denormal. A denormal value is one that is too small to represent as a standard floating-point value.", size);
               break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
               strncpy(message, "The thread attempted to divide a floating-point value by a floating-point divisor of zero.", size);
               break;
            case EXCEPTION_FLT_INEXACT_RESULT:
               strncpy(message, "The result of a floating-point operation cannot be represented exactly as a decimal fraction.", size);
               break;
            case EXCEPTION_FLT_INVALID_OPERATION:
               strncpy(message, "This exception represents any floating-point exception not included in this list.", size);
               break;
            case EXCEPTION_FLT_OVERFLOW:
               strncpy(message, "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.", size);
               break;
            case EXCEPTION_FLT_STACK_CHECK:
               strncpy(message, "The stack overflowed or underflowed as the result of a floating-point operation.", size);
               break;
            case EXCEPTION_FLT_UNDERFLOW:
               strncpy(message, "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.", size);
               break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
               strncpy(message, "The thread attempted to divide an integer value by an integer divisor of zero.", size);
               break;
            case EXCEPTION_INT_OVERFLOW:
               strncpy(message, "The result of an integer operation caused a carry out of the most significant bit of the result.", size);
               break;
            case EXCEPTION_PRIV_INSTRUCTION:
               strncpy(message, "The thread attempted to execute an instruction whose operation is not allowed in the current machine mode.", size);
               break;
            case EXCEPTION_NONCONTINUABLE_EXCEPTION:
               strncpy(message, "The thread attempted to continue execution after a noncontinuable exception occurred.", size);
               break;
            default:
               strncpy(message, "Unrecognised system or hardware error.", size);
            break;
         }
      }
   }

#else

   if (context == 0) {
#if defined(_GNU_SOURCE)
      char *p;
#endif
      strcpy(message, "");
#if defined(LINUX) || defined(AIX) || defined(OSF1) || defined(MACOSX)
#if defined(_GNU_SOURCE)
      p = strerror_r(error_code, message, (size_t) size);
      if (p && p != message) {
         strncpy(message, p, size - 1);
         message[size - 1] = '\0';
      }
#else
      strerror_r(error_code, message, (size_t) size);
#endif
      size = (int) strlen(message);
#else
      netx_get_std_error_message(error_code, message, size, context);
      size = (int) strlen(message);
#endif
   }

#endif

   message[size - 1] = '\0';

   return (int) strlen(message);
}


int netx_get_std_error_message(int error_code, char *message, int size, int context)
{

   strcpy(message, "");

#if !defined(_WIN32)
   switch (error_code) {
      case E2BIG:
         strncpy(message, "Argument list too long.", size);
         break;
      case EACCES:
         strncpy(message, "Permission denied.", size);
         break;
      case EADDRINUSE:
         strncpy(message, "Address in use.", size);
         break;
      case EADDRNOTAVAIL:
         strncpy(message, "Address not available.", size);
         break;
      case EAFNOSUPPORT:
         strncpy(message, "Address family not supported.", size);
         break;
      case EAGAIN:
         strncpy(message, "Resource unavailable, try again.", size);
         break;
      case EALREADY:
         strncpy(message, "Connection already in progress.", size);
         break;
      case EBADF:
         strncpy(message, "Bad file descriptor.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case EBADMSG:
         strncpy(message, "Bad message.", size);
         break;
#endif
      case EBUSY:
         strncpy(message, "Device or resource busy.", size);
         break;
      case ECANCELED:
         strncpy(message, "Operation canceled.", size);
         break;
      case ECHILD:
         strncpy(message, "No child processes.", size);
         break;
      case ECONNABORTED:
         strncpy(message, "Connection aborted.", size);
         break;
      case ECONNREFUSED:
         strncpy(message, "Connection refused.", size);
         break;
      case ECONNRESET:
         strncpy(message, "Connection reset.", size);
         break;
      case EDEADLK:
         strncpy(message, "Resource deadlock would occur.", size);
         break;
      case EDESTADDRREQ:
         strncpy(message, "Destination address required.", size);
         break;
      case EDOM:
         strncpy(message, "Mathematics argument out of domain of function.", size);
         break;
      case EDQUOT:
         strncpy(message, "Reserved.", size);
         break;
      case EEXIST:
         strncpy(message, "File exists.", size);
         break;
      case EFAULT:
         strncpy(message, "Bad address.", size);
         break;
      case EFBIG:
         strncpy(message, "File too large.", size);
         break;
      case EHOSTUNREACH:
         strncpy(message, "Host is unreachable.", size);
         break;
      case EIDRM:
         strncpy(message, "Identifier removed.", size);
         break;
      case EILSEQ:
         strncpy(message, "Illegal byte sequence.", size);
         break;
      case EINPROGRESS:
         strncpy(message, "Operation in progress.", size);
         break;
      case EINTR:
         strncpy(message, "Interrupted function.", size);
         break;
      case EINVAL:
         strncpy(message, "Invalid argument.", size);
         break;
      case EIO:
         strncpy(message, "I/O error.", size);
         break;
      case EISCONN:
         strncpy(message, "Socket is connected.", size);
         break;
      case EISDIR:
         strncpy(message, "Is a directory.", size);
         break;
      case ELOOP:
         strncpy(message, "Too many levels of symbolic links.", size);
         break;
      case EMFILE:
         strncpy(message, "Too many open files.", size);
         break;
      case EMLINK:
         strncpy(message, "Too many links.", size);
         break;
      case EMSGSIZE:
         strncpy(message, "Message too large.", size);
         break;
#if !defined(MACOSX) && !defined(OSF1) && !defined(FREEBSD)
      case EMULTIHOP:
         strncpy(message, "Reserved.", size);
         break;
#endif
      case ENAMETOOLONG:
         strncpy(message, "Filename too long.", size);
         break;
      case ENETDOWN:
         strncpy(message, "Network is down.", size);
         break;
      case ENETRESET:
         strncpy(message, "Connection aborted by network.", size);
         break;
      case ENETUNREACH:
         strncpy(message, "Network unreachable.", size);
         break;
      case ENFILE:
         strncpy(message, "Too many files open in system.", size);
         break;
      case ENOBUFS:
         strncpy(message, "No buffer space available.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENODATA:
         strncpy(message, "[XSR] [Option Start] No message is available on the STREAM head read queue. [Option End]", size);
         break;
#endif
      case ENODEV:
         strncpy(message, "No such device.", size);
         break;
      case ENOENT:
         strncpy(message, "No such file or directory.", size);
         break;
      case ENOEXEC:
         strncpy(message, "Executable file format error.", size);
         break;
      case ENOLCK:
         strncpy(message, "No locks available.", size);
         break;
#if !defined(MACOSX) && !defined(OSF1) && !defined(FREEBSD)
      case ENOLINK:
         strncpy(message, "Reserved.", size);
         break;
#endif
      case ENOMEM:
         strncpy(message, "Not enough space.", size);
         break;
      case ENOMSG:
         strncpy(message, "No message of the desired type.", size);
         break;
      case ENOPROTOOPT:
         strncpy(message, "Protocol not available.", size);
         break;
      case ENOSPC:
         strncpy(message, "No space left on device.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENOSR:
         strncpy(message, "[XSR] [Option Start] No STREAM resources. [Option End]", size);
         break;
#endif
#if !defined(MACOSX) && !defined(FREEBSD)
      case ENOSTR:
         strncpy(message, "[XSR] [Option Start] Not a STREAM. [Option End]", size);
         break;
#endif
      case ENOSYS:
         strncpy(message, "Function not supported.", size);
         break;
      case ENOTCONN:
         strncpy(message, "The socket is not connected.", size);
         break;
      case ENOTDIR:
         strncpy(message, "Not a directory.", size);
         break;
#if !defined(AIX) && !defined(AIX5)
      case ENOTEMPTY:
         strncpy(message, "Directory not empty.", size);
         break;
#endif
      case ENOTSOCK:
         strncpy(message, "Not a socket.", size);
         break;
      case ENOTSUP:
         strncpy(message, "Not supported.", size);
         break;
      case ENOTTY:
         strncpy(message, "Inappropriate I/O control operation.", size);
         break;
      case ENXIO:
         strncpy(message, "No such device or address.", size);
         break;
#if !defined(LINUX) && !defined(MACOSX) && !defined(FREEBSD)
      case EOPNOTSUPP:
         strncpy(message, "Operation not supported on socket.", size);
         break;
#endif
#if !defined(OSF1)
      case EOVERFLOW:
         strncpy(message, "Value too large to be stored in data type.", size);
         break;
#endif
      case EPERM:
         strncpy(message, "Operation not permitted.", size);
         break;
      case EPIPE:
         strncpy(message, "Broken pipe.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case EPROTO:
         strncpy(message, "Protocol error.", size);
         break;
#endif
      case EPROTONOSUPPORT:
         strncpy(message, "Protocol not supported.", size);
         break;
      case EPROTOTYPE:
         strncpy(message, "Protocol wrong type for socket.", size);
         break;
      case ERANGE:
         strncpy(message, "Result too large.", size);
         break;
      case EROFS:
         strncpy(message, "Read-only file system.", size);
         break;
      case ESPIPE:
         strncpy(message, "Invalid seek.", size);
         break;
      case ESRCH:
         strncpy(message, "No such process.", size);
         break;
      case ESTALE:
         strncpy(message, "Reserved.", size);
         break;
#if !defined(MACOSX) && !defined(FREEBSD)
      case ETIME:
         strncpy(message, "[XSR] [Option Start] Stream ioctl() timeout. [Option End]", size);
         break;
#endif
      case ETIMEDOUT:
         strncpy(message, "Connection timed out.", size);
         break;
      case ETXTBSY:
         strncpy(message, "Text file busy.", size);
         break;
#if !defined(LINUX) && !defined(AIX) && !defined(AIX5) && !defined(MACOSX) && !defined(OSF1) && !defined(SOLARIS) && !defined(FREEBSD)
      case EWOULDBLOCK:
         strncpy(message, "Operation would block.", size);
         break;
#endif
      case EXDEV:
         strncpy(message, "Cross-device link.", size);
         break;
      default:
         strcpy(message, "");
      break;
   }
#endif

   return (int) strlen(message);
}




