#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include <string.h>
#include <stdarg.h>

char *
Strdup ( const char *s )
{
	char *retval;

	ASSERT ( s != NULL );

	retval = strdup ( s );

	if  ( retval == NULL )
		Sys_failure ( "strdup" );

	return retval;
}

void
Snprintf ( char *str, size_t size, const char *format, ... )
{     
	va_list ap;
	int retval;

	ASSERT ( str != NULL );
	ASSERT ( format != NULL );

	va_start ( ap, format );
	retval = vsnprintf ( str, size, format, ap );
	va_end ( ap );

	if  ( retval < 0 ) 
		Sys_failure ( "snprintf" );

	if  ( retval == size - 1 ) 
		Warning ( "snprintf: the output may be truncated." );
}

void
Vsnprintf ( char *str, size_t size, const char *format, va_list ap )
{     
	int retval;

	ASSERT ( str != NULL );
	ASSERT ( format != NULL );

	retval = vsnprintf ( str, size, format, ap );

	if  ( retval < 0 ) 
		Sys_failure ( "vsnprintf" );

	if  ( retval == size - 1 ) 
		Warning ( "vsnprintf: the output may be truncated." );
}

bool_t
String_equal ( const char *s1, const char *s2 )
{
	size_t n1, n2;

	ASSERT ( s1 != NULL );
	ASSERT ( s2 != NULL );
	
	n1 = strlen ( s1 );
	n2 = strlen ( s2 );

	return ( ( n1 == n2 ) && ( strncmp ( s1, s2, n1 ) ) == 0 );
}
