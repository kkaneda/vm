#ifndef _VMM_STD_NET_H
#define _VMM_STD_NET_H

#include "vmm/std/types.h"
#include <netdb.h>

#ifndef INADDR_NONE
#define INADDR_NONE              ( ( in_addr_t ) 0xffffffff )
#endif

int     Socket ( int domain, int type, int protocol );
void    Connect ( int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen );
void    Bind ( int sockfd, struct sockaddr *my_addr, socklen_t addrlen );
void    Listen ( int s, int backlog );
int     Accept ( int fd, struct sockaddr *addr, socklen_t *addrlen );
void    Shutdown ( int s, int how );
ssize_t Send ( int s, const void *buf, size_t len, int flags );
void    Sendn ( int fd, const void *buf, size_t count, int flags );
ssize_t Recv ( int s, void *buf, size_t len, int flags );
void    Recvn ( int fd, void *buf, size_t count, int flags );
int     Recvn2 ( int fd, void *buf, size_t count, int flags );
void    Setsockopt ( int s, int level, int optname, const void *optval, socklen_t optlen );
void    Setsockopt_reuseaddr ( int s );
void    Setsockopt_nodelay ( int s );
void    Setsockflag_nonblock ( int s );
bool_t  Gethostbyname ( const char *name, struct hostent *ret );
void    Inet_pton ( int af, const char *src, void * dst );
void    Inet_ntop ( int af, const void *src, char *dst, socklen_t cnt );
struct sockaddr_in Inet_sockaddr_create ( in_addr_t in_addr, int port );

#endif /* _VMM_STD_NET_H */
