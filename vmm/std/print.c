#include "vmm/std/debug.h"
#include "vmm/std/print_common.h"
#include <stdarg.h>

/* comment this if you want not to print colored messages */
#define ENABLE_COLORING


void
Printv ( FILE *stream, const char *fmt, va_list ap )
{
	ASSERT ( stream != NULL );
	ASSERT ( fmt != NULL );

	vfprintf ( stream, fmt, ap );
	fflush ( stream );
}

void
Print ( FILE *stream, const char *fmt, ... )
{
	va_list ap;

	ASSERT ( stream != NULL );
	ASSERT ( fmt != NULL );

	va_start ( ap, fmt );
	Printv ( stream, fmt, ap );
	va_end ( ap );
}

void
Print_newline ( FILE *stream )
{
	ASSERT ( stream != NULL );
	Print ( stream, "\n" );
}

static void
store_color ( FILE *stream, color_t x )
{
	ASSERT ( stream != NULL );

#ifdef ENABLE_COLORING
	switch ( x ) {
	case RED:	fprintf ( stream, "\033[02;31m" ); break;
	case GREEN:	fprintf ( stream, "\033[02;32m" ); break;
	case YELLOW:	fprintf ( stream, "\033[02;33m" ); break;
	case BLUE:	fprintf ( stream, "\033[02;34m" ); break;
	case MAGENTA:	fprintf ( stream, "\033[02;35m" ); break;
	case CYAN:	fprintf ( stream, "\033[02;36m" ); break;
	case WHITE:	fprintf ( stream, "\033[02;37m" ); break;
	default:	Match_failure ( "in store_color (  )\n" );
	}
#endif /* ENABLE_COLORING */
}

static void
restore_color ( FILE *stream )
{
#ifdef ENABLE_COLORING
	fprintf ( stream, "\033[00m" );
#endif /* ENABLE_COLORING */
}

void
Printv_color ( FILE *stream, color_t color, const char *fmt, va_list ap )
{
	ASSERT ( stream != NULL );
	ASSERT ( fmt != NULL );

	store_color ( stream, color );
	vfprintf ( stream, fmt, ap );
	restore_color ( stream );
	fflush ( stream );
}

void
Print_color ( FILE *stream, color_t color, const char *fmt, ... )
{
	va_list ap;

	ASSERT ( stream != NULL );
	ASSERT ( fmt != NULL );

	va_start ( ap, fmt );
	Printv_color ( stream, color, fmt, ap );
	va_end ( ap );
}
