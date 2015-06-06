#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include <ctype.h>


int
Atoi ( const char *nptr )
{
	int i;
	
	ASSERT(nptr != NULL);

	for ( i = 0; nptr[i] != '\0'; i++ ) {
		if ( isdigit ( nptr[i] ) == 0 ) 
			Fatal_failure("Atoi error: char '%c' (in \"%s\") is not digit\n", nptr[i],	nptr );	}

	return atoi ( nptr );
}

/* compute with 96 bit intermediate result: (a*b)/c */
bit64u_t
muldiv64 ( bit64u_t a, bit32u_t b, bit32u_t c )
{
	union {
		bit64u_t ll;
		struct {
			bit32u_t low, high;
		} l;
	} u, res;
	bit64u_t rl, rh;
	
	u.ll = a;
	rl = (bit64u_t)u.l.low * (bit64u_t)b;
	rh = (bit64u_t)u.l.high * (bit64u_t)b;
	rh += (rl >> 32);
	res.l.high = rh / c;
	res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
	return res.ll;
}
