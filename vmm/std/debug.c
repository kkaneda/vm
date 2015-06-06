#include "vmm/std/print.h"
#include "vmm/std/debug_common.h"
#include <stdarg.h>

void
Warning ( const char *fmt, ... )
{
	va_list ap;

	ASSERT ( fmt != NULL );

	Print_color ( stderr, RED, "Warning" );
	Print ( stderr, " on " );

	va_start ( ap, fmt );
	Printv ( stderr, fmt, ap );
	va_end ( ap );
}

void
Fatal_failure ( const char *fmt, ... )
{
	va_list ap;

	ASSERT ( fmt != NULL );

	Print_color ( stderr, RED, "Failed" );
	Print ( stderr, " on " );

	va_start ( ap, fmt );
	Printv ( stderr, fmt, ap );
	va_end ( ap );
	exit ( 1 );
}

void
Sys_failure ( const char *s )
{
	ASSERT ( s != NULL );

	Print_color ( stderr, RED, "Failed" );
	Print ( stderr, " on " );

	perror ( s ); 
	exit ( errno ); 
}

void
Match_failure ( const char *fmt, ... )
{
	va_list ap;

	ASSERT ( fmt != NULL );

	Print_color ( stderr, RED, "Failed on pattarn matching" );
	Print ( stderr, " on " );

	va_start ( ap, fmt );
	Printv ( stderr, fmt, ap );
	va_end ( ap );

	exit ( 1 );
}

#ifdef DEBUG

inline void
DPRINT ( const char *fmt, ... )
{
	va_list ap;

	ASSERT ( fmt != NULL );

	va_start ( ap, fmt ); 
	Printv ( stderr, fmt, ap );
	va_end ( ap );
}

inline void
DPRINT2 ( const char *fmt, ... )
{
	va_list ap;

	ASSERT ( fmt != NULL );

	va_start ( ap, fmt ); 
	Printv ( stdout, fmt, ap );
	va_end ( ap );
}

#else /* !DEBUG */

inline void DPRINT ( const char * fmt, ... )  { }
inline void DPRINT2 ( const char * fmt, ... ) { }    

#endif /* DEBUG */
