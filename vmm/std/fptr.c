#include "vmm/std/print.h"
#include "vmm/std/debug.h"
#include "vmm/std/mem.h"
#include "vmm/std/fptr_common.h"

struct fptr_t
Fptr_create ( const void *base, size_t offset )
{
	struct fptr_t retval;

	retval.base =  ( void * ) base; // to avoid gcc warning
	retval.offset = offset;
	return retval;
}

void 
Fptr_destroy ( struct fptr_t x )
{
	ASSERT ( x.base != NULL );
	ASSERT ( x.offset > 0 );
     
	Free ( x.base );
}

struct fptr_t
Fptr_alloc ( size_t size )
{
	void * base = Malloc ( size );
	return Fptr_create ( base, size );
}

struct fptr_t
Fptr_null ( void )
{
	return Fptr_create ( NULL, 0 );
}

void
Fptr_zero ( struct fptr_t x )
{
	Mzero ( x.base, x.offset );
}

bool_t
Fptr_is_null ( const struct fptr_t x )
{
	return  ( x.base == NULL );
}

void
Fptr_print ( FILE *stream, const struct fptr_t x )
{
	Print_color ( stream, BLUE, "{ " );
	Print ( stream, "base=%p, offset=%d", x.base, x.offset );
	Print_color ( stream, BLUE, " }" );
}

