#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include "vmm/std/fptr.h"
#include "vmm/std/mem.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

int
Open ( const char *pathname, int oflag )
{
	int retval;

	ASSERT ( pathname != NULL );

	retval = open ( pathname, oflag );
	if ( retval == -1 ) {
		Print ( stderr, "open(\"%s\")\n", pathname );
		Sys_failure ( "open" );
	}

	return retval;
}

int
Open_fmt ( int oflag, const char *fmt, ... )
{
	enum { BUFSIZE = 1024 };
	char path[BUFSIZE];
	va_list ap;

	ASSERT ( fmt != NULL );
     
	va_start ( ap, fmt );
	vsnprintf ( path, BUFSIZE, fmt, ap );
	va_end ( ap );

	return Open ( path, oflag );
}

int
Open2 ( const char *pathname, int oflag, mode_t mode )
{
	int retval;
	ASSERT ( pathname != NULL );

	retval = open ( pathname, oflag, mode );
	if ( retval == -1 ) {
		Print ( stderr, "open(\"%s\")\n", pathname );
		Sys_failure ( "open" );
	}

	return retval;
}

int
Open2_fmt ( int oflag, mode_t mode, const char *fmt, ... )
{
	enum { BUFSIZE = 1024 };
	char path[BUFSIZE];
	va_list ap;

	ASSERT ( fmt != NULL );
     
	va_start ( ap, fmt );
	vsnprintf ( path, BUFSIZE, fmt, ap );
	va_end ( ap );

	return Open2 ( path, oflag, mode );
}

int
Creat ( const char *pathname, mode_t mode )
{
	int retval;
 
	ASSERT ( pathname != NULL );
	
	retval = creat ( pathname, mode );
	if ( retval == -1 ) 
		Sys_failure ( "creat" );
	
	return retval;	
}

int
Creat_fmt ( mode_t mode, const char *fmt, ... )
{
	enum { BUFSIZE = 1024 };
	char path[BUFSIZE];
	va_list ap;

	ASSERT ( fmt != NULL );
     
	va_start ( ap, fmt );
	vsnprintf ( path, BUFSIZE, fmt, ap );
	va_end ( ap );

	return Creat ( path, mode );
}

void
Close ( int fd )
{
	int retval;

	retval = close ( fd );

	if ( retval == -1 )
		Sys_failure ( "close" );
}

void
Truncate ( const char *path, off_t length )
{
	int retval;

	ASSERT ( path != NULL );

	retval = truncate ( path, length );

	if ( retval == -1 )
		Sys_failure ( "truncate" );
}

void
Ftruncate ( int fd, off_t length )
{
	int retval;

	retval = ftruncate ( fd, length );

	if ( retval == -1 )
		Sys_failure ( "ftruncate" );
}

off_t
Lseek ( int fildes, off_t offset, int whence )
{
	off_t retval;

	retval = lseek ( fildes, offset, whence );
	if ( retval ==  ( off_t )-1 )
		Sys_failure ( "lseek" );

	return retval;
}

size_t
Get_filesize ( int fd )
{
	return Lseek ( fd, 0, SEEK_END );
}

ssize_t
Read ( int fd, void *buf, size_t count ) 
{
	ssize_t retval;

	assert ( buf != NULL );

	retval = read ( fd, buf, count );
	if ( retval == -1 ) { 
		if ( errno != EINTR )
			Sys_failure ( "read" );

		retval = 0;
	};
	return retval;
}

void
Readn ( int fd, void *buf, size_t count ) 
{
	size_t off = 0;

	assert ( buf != NULL );

	while  ( off < count ) {
		size_t n;
		n = Read ( fd, buf + off, count - off );
		assert ( n != 0 );
		off += n;
	}
}

ssize_t
Write ( int fd, const void *buf, size_t count ) 
{
	ssize_t retval;

	assert ( buf != NULL );

	retval = write ( fd, buf, count );

	if ( retval == -1 ) {
		if ( errno != EINTR )
			Sys_failure ( "read" );
		
		retval = 0;
	}

	return retval;
}

void
Writen ( int fd, const void *buf, size_t count ) {
	size_t off = 0;

	assert ( buf != NULL );

	while  ( off < count ) {
		size_t n;
		n = Write ( fd, buf + off, count - off );
		off += n;
	}
}

int
Fcntl ( int fd, int cmd, long arg )
{
	int retval;

	retval = fcntl ( fd, cmd, arg );

	if ( retval == -1 )
		Sys_failure ( "fcntl" );

	return retval;
}

FILE *
Fopen ( const char *path, const char *mode )
{
	FILE *retval;

	retval = fopen ( path, mode );
	if ( retval == NULL ) {
		Print ( stderr, "fopen(\"%s\")\n", path );
		Sys_failure ( "fopen" );
	}

	return retval;     
}

FILE *
Fopen_fmt ( const char *mode, const char *fmt, ... )
{
	enum { BUFSIZE = 1024 };
	char path[BUFSIZE];
	va_list ap;
     
	ASSERT ( fmt != NULL );

	va_start ( ap, fmt );
	vsnprintf ( path, BUFSIZE, fmt, ap );
	va_end ( ap );

	return Fopen ( path, mode );
}

void
Fclose ( FILE *fp )
{
	int ret;

	assert ( fp != NULL );

	ret = fclose ( fp );

	if ( ret == EOF ) 
		Sys_failure ( "fclose" );
}

int
Select ( int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout )
{
	int retval;

	retval = select ( n, readfds, writefds, exceptfds, timeout );
	if ( retval == -1 ) {
		if ( errno != EINTR ) 
			Sys_failure ( "select" ); 
	  
		retval = 0;
	}

	return retval;
}

bool_t
Fd_is_readable ( int fd )
{
	fd_set fds;
	struct timeval tv;
     
	FD_ZERO  ( &fds );
	FD_SET ( fd, &fds );
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	  
	Select ( fd + 1, &fds, NULL, NULL, &tv );
	return FD_ISSET ( fd, &fds );
}

void
Mkfifo ( const char *pathname, mode_t mode )
{
	int retval;

	retval = mkfifo ( pathname, mode );
	if ( retval == -1 )
		Sys_failure ( "mkfifo" );
}

void
Copy_file ( const char *from, const char *to )
{
	int from_fd, to_fd;

	from_fd = Open ( from, O_RDONLY );
	to_fd = Open2 ( to,
			O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR );

	while ( TRUE ) {
		enum { BUF_SIZE = 1024 };
		char buf[BUF_SIZE];
		int n;

		n = Read ( from_fd, buf, BUF_SIZE );

		if ( n == 0 )
			break;
		
		Writen ( to_fd, buf, n );
	}

	Close ( from_fd );
	Close ( to_fd );
}

void
Remove ( const char *pathname )
{
	int retval;

	retval = remove ( pathname );

	if ( retval == -1 )
		Sys_failure ( "remove" );	
}

void
Pack ( const void *p, size_t len, int fd )
{
	Writen ( fd, &len, sizeof ( int ) );
	Writen ( fd, p, len );
}

void
Unpack ( void *p, size_t len, int fd )
{
	size_t l;

	Read ( fd, &l, sizeof ( int ) );
	assert ( l == len );

	Read ( fd, p, len );
}

struct fptr_t
Unpack_any ( int fd )
{
	struct fptr_t x;

	Read ( fd, &x.offset, sizeof ( int ) );
	x.base = Malloc ( x.offset );
	Read ( fd, x.base, x.offset );

	return x;
}

#define DEFINE_PACK_UNPACK_FUNC(fun_name, type) \
void \
fun_name ## _pack ( type x, int fd ) \
{ \
	Pack ( &x, sizeof ( type ), fd ); \
} \
\
type \
fun_name ## _unpack ( int fd ) \
{ \
	type x; \
	Unpack ( &x, sizeof ( type ), fd ); \
	return x; \
} \
void \
fun_name ## Array_pack ( const type x[], size_t nelem, int fd ) \
{ \
	Pack ( x, sizeof ( type ) * nelem, fd ); \
} \
\
void \
fun_name ## Array_unpack ( type x[], size_t nelem, int fd ) \
{ \
	Unpack ( x, sizeof ( type ) * nelem, fd ); \
} \
\

DEFINE_PACK_UNPACK_FUNC(Bool, bool_t)
DEFINE_PACK_UNPACK_FUNC(Bit8u, bit8u_t)
DEFINE_PACK_UNPACK_FUNC(Bit16u, bit16u_t)
DEFINE_PACK_UNPACK_FUNC(Bit32u, bit32u_t)
DEFINE_PACK_UNPACK_FUNC(Bit64u, bit64u_t)

