#include "vmm/std/types.h"
#include "vmm/std/print.h"
#include "vmm/std/debug.h"
#include "vmm/std/mem.h"
#include "vmm/std/str.h"
#include "vmm/std/io.h"
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <fcntl.h>


int
Socket ( int domain, int type, int protocol )
{
	int retval;

	retval = socket ( domain, type, protocol );
	if ( retval == -1 )
		Sys_failure ( "socket" );

	return retval;
}

void
Connect ( int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen )
{
	int retval;
     
	retval = connect ( sockfd, serv_addr, addrlen );
	if ( retval == -1 )
		Sys_failure ( "connect" );
}

void
Bind ( int sockfd, struct sockaddr *my_addr, socklen_t addrlen )
{
	int retval;
	ASSERT ( my_addr != NULL );

	retval = bind ( sockfd, my_addr, addrlen );
	if ( retval == -1 )
		Sys_failure ( "bind" );
}

void
Listen ( int s, int backlog )
{
	char * p;
	int retval;
	int n;
  
	p = getenv ( "LISTENQ" );
	n =  ( p != NULL ) ? atoi ( p ) : backlog;
	retval = listen ( s, n );
	if ( retval == -1 )
		Sys_failure ( "listen" );
}

int
Accept ( int fd, struct sockaddr *addr, socklen_t *addrlen )
{
	int retval;
     
	for ( ; ; ) {
		retval = accept ( fd, addr, addrlen );
		if ( retval >= 0 )
			break;

		if ( errno == EINTR )
			continue;

		if ( errno != ECONNABORTED )
			Sys_failure ( "accept" );

	}
	return retval;
}

void
Shutdown ( int s, int how )
{
	int retval;

	retval = shutdown ( s, how );
	if ( retval == -1 )
		Sys_failure ( "shutdown" );
}

ssize_t
Send ( int s, const void *buf, size_t len, int flags )
{
	ssize_t retval;
	ASSERT ( buf != NULL );

	retval = send ( s, buf, len, flags );
	if ( retval == -1 ) 
		Sys_failure ( "send" );

	return retval;
}

void
Sendn ( int fd, const void *buf, size_t count, int flags )
{
	size_t off = 0;
	assert ( buf != NULL );

	while  ( off < count ) {
		ssize_t n;

		n = send ( fd, buf + off, count - off, flags );
		if ( n == -1 ) {
			if ( errno != EAGAIN )
				Sys_failure ( "sendn" );
			continue; 
		}
		off += n;
	}
}

ssize_t
Recv ( int s, void *buf, size_t len, int flags )
{
	ssize_t retval;
	ASSERT ( buf != NULL );

	retval = recv ( s, buf, len, flags );
	if ( retval == -1 ) {
		if ( errno != EAGAIN ) {
			Sys_failure ( "recv" );
		}
	}

	return retval;
}

static ssize_t
__recvn ( int fd, void *buf, size_t count, int flags )
{
	size_t off = 0;
	assert ( buf != NULL );

	while  ( off < count ) {
		ssize_t n;

		n = recv ( fd, buf + off, count - off, flags );
		if ( n == 0 ) {
			return -1;
		}

		if ( n < 0 ) { 
			if ( errno != EAGAIN )
				return -1;
			continue; 
		}

		off += n;
	}
	return 0;
}

void
Recvn ( int fd, void *buf, size_t count, int flags )
{
	ssize_t ret;

	ret = __recvn ( fd, buf, count, flags );
	if ( ret == -1 ) {
		Sys_failure ( "Recvn" );
	}
}

int
Recvn2 ( int fd, void *buf, size_t count, int flags )
{
	return __recvn ( fd, buf, count, flags );
}

void
Setsockopt ( int s, int level, int optname, const void *optval, socklen_t optlen )
{
	int retval;

	retval = setsockopt ( s, level, optname, optval, optlen );
	if ( retval == -1 )
		Sys_failure ( "setsockopt" );
}

void
Setsockopt_reuseaddr ( int s )
{
	const int on = 1;

	Setsockopt ( s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof ( on ) );
}

void
Setsockopt_nodelay ( int s )
{
	const int on = 1;

	Setsockopt ( s, IPPROTO_TCP, TCP_NODELAY, &on, sizeof ( on ) );
}

void
Setsockflag_nonblock ( int s )
{
	int sock_flags;
	sock_flags = Fcntl ( s, F_GETFL, 0 );
	sock_flags = sock_flags | O_NONBLOCK;
	Fcntl ( s, F_SETFL, sock_flags );
}

#ifdef HAVE_FUNC_GETHOSTBYNAME_R_6
static bool_t
Gethostbyname_r_sub ( const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop )
{
	int retval;

	retval = gethostbyname_r ( name, ret, buf, buflen, result, h_errnop );

	return ( ( retval == 0 ) && ( *result != NULL ) );
}
#endif /* HAVE_FUNC_GETHOSTBYNAME_R_6 */

#ifdef HAVE_FUNC_GETHOSTBYNAME_R_5
static bool_t
Gethostbyname_r_sub ( const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop )
{
	struct hostent * retval;

	retval = gethostbyname_r ( name, ret, buf, buflen, h_errnop );
	return ( retval != NULL );
}
#endif /* HAVE_FUNC_GETHOSTBYNAME_R_5 */

#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
static bool_t
Gethostbyname_r_sub ( const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop )
{
	Fatal_failure ( "Gethostbyname_r_sub: not implemented" );

	return TRUE;
}
#endif /* HAVE_FUNC_GETHOSTBYNAME_R_3 */

static bool_t
Gethostbyname_r ( const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop )
{
	bool_t retval;
     
	retval = Gethostbyname_r_sub ( name, ret, buf, buflen, result, h_errnop );
	if ( retval ) 
		return TRUE;

#ifdef DEBUG
	enum { BUFSIZE = 1024 };
	char s1[BUFSIZE];
	herror ( "gethostbyname_r" );
	Snprintf ( s1, BUFSIZE, "gethostbyname_r  ( name=%s )", name );
	DPRINT ( "warning: %s: %s\n", s1, strerror ( errno ) );
#endif /* DEBUG */

	return FALSE;
}

bool_t
Gethostbyname ( const char *name, struct hostent *ret )
{
	enum { BUFSIZE = 1024 };
	char buf[BUFSIZE];
	int h_errnop;
	struct hostent * result;

	ASSERT ( name != NULL );
	ASSERT ( ret != NULL );

	return Gethostbyname_r ( name, ret, buf, BUFSIZE, &result, &h_errnop );
}

void
Inet_pton ( int af, const char * src, void * dst )
{
	int retval;

	ASSERT ( src != NULL );
	ASSERT ( dst != NULL );

	retval = inet_pton ( af, src, dst );
	if ( retval <= 0 )
		Sys_failure ( "inet_pton" );
}

void
Inet_ntop ( int af, const void *src, char *dst, socklen_t cnt )
{
	const char * retval;

	ASSERT ( src != NULL );
	ASSERT ( dst != NULL );

	retval = inet_ntop ( af, src, dst, cnt );
	if ( retval == NULL )
		Sys_failure ( "inet_ntop" );
}

struct sockaddr_in
Inet_sockaddr_create ( in_addr_t in_addr, int port )
{
	struct sockaddr_in retval;
     
	Mzero ( &retval, sizeof ( retval ) );  
	retval.sin_family = AF_INET;
	retval.sin_port = htons ( port );
	Mmove ( &retval.sin_addr.s_addr, &in_addr, sizeof ( retval.sin_addr.s_addr ) );

	return retval;
}
