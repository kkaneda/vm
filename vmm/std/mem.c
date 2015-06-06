#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include <string.h>
#include <sys/mman.h>

void *Malloc ( size_t size );

void
Mmove ( void *dest, const void *src, size_t n )
{
	ASSERT ( dest != NULL );
	ASSERT ( src != NULL );
	ASSERT ( n > 0 );

	memmove ( dest, src, n );
}

void *
Memset ( void *s, int c, size_t n )
{
	ASSERT ( s != NULL );
	ASSERT ( n > 0 );

	return memset ( s, c, n );
}

void *
Memchr ( const void *s, int c, size_t n )
{
	ASSERT ( s != NULL );
	ASSERT ( n > 0 );

	return memchr ( s, c, n );
}

void *
Mdup ( const void *s, size_t n )
{
	void * retval;
	ASSERT ( s != NULL );
	ASSERT ( n > 0 );

	retval = Malloc ( n );
	Mmove ( retval, s, n );

	return retval;
}

void
Mzero ( void *s, size_t n )
{
	ASSERT ( s != NULL );
	ASSERT ( n > 0 );

	Memset ( s, 0, n );
}

void *
Malloc ( size_t size )
{
	void * retval;
	
	ASSERT ( size > 0 );
     
	retval = malloc ( size );
	if ( retval == NULL )
		Sys_failure ( "malloc" );

	Mzero ( retval, size );

	return retval;
}

void *
Calloc ( size_t nmemb, size_t size )
{
	void *retval;

	ASSERT ( nmemb > 0 );
	ASSERT ( size > 0 );

	retval = calloc ( nmemb, size );
	if ( retval == NULL )
		Sys_failure ( "calloc" );

	Mzero ( retval, nmemb*size );

	return retval;
}

void *
Realloc ( void *ptr, size_t size )
{
	void *retval;

	retval = realloc ( ptr, size );
	if ( retval == NULL )
		Sys_failure ( "realloc" );
	
	if ( ( ptr == NULL ) && ( size > 0 ) )
		Mzero ( retval, size );

	return retval;
}

void
Free ( void * ptr )
{
	ASSERT ( ptr != NULL );

	free ( ptr );
}

// #define ENABLE_MMAP_LOG

#ifdef ENABLE_MMAP_LOG

#include "io.h"
#include "print.h"
#include "unix.h"
static FILE *mmap_log = NULL;

static void
log_mmap ( void *start, size_t length, int prot, int fd, off_t offset )
{
	if ( mmap_log == NULL ) {
		mmap_log = Fopen_fmt ( "w+", "/tmp/mmap_log%d", Getpid ( ) );
	}

	Print ( mmap_log, "mmap:\t" "%#lx --> %#lx (%#lx) ", start, offset, length );

	if ( prot & PROT_READ )  Print ( mmap_log, "R" );
	if ( prot & PROT_WRITE ) Print ( mmap_log, "W" );
	if ( prot & PROT_EXEC )  Print ( mmap_log, "E" );
	
	Print ( mmap_log, " fd=%d\n", fd );
}

static void
log_unmap ( void *start, size_t length )
{
	assert ( mmap_log != NULL );
	
	Print ( mmap_log, "munmap:\t" "%#lx (%#lx)\n", start, length );
}

#endif /* ENABLE_MMAP_LOG */

void *
Mmap ( void *start, size_t length, int prot, int flags, int fd, off_t offset )
{
	void *retval;

	retval = mmap ( start, length, prot, flags, fd, offset );
	if ( retval == MAP_FAILED )
		Sys_failure ( "mmap" );

#ifdef ENABLE_MMAP_LOG
	log_mmap ( start, length, prot, fd, offset );
#endif /* ENABLE_MMAP_LOG */

	return retval;
}

void
Munmap ( void *start, size_t length )
{
	int retval;

	retval = munmap ( start, length );
	if ( retval == -1 ) {
		Sys_failure ( "munmap" );
	}

#ifdef ENABLE_MMAP_LOG
	log_unmap ( start, length );
#endif /* ENABLE_MMAP_LOG */
}

void
Mprotect ( const void *addr, size_t len, int prot )
{
	int retval;

	/* [???]
	 * The definition of the 1th argument of mprotect (  ) in /usr/include/sysm/mman.h 
	 * differs from the definition in man  ( online manual ).
	 */
	retval = mprotect ( ( void * )addr, len, prot ); 

	if ( retval == -1 ) {
		Sys_failure ( "mprotect" );
	}
}

