#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include "vmm/std/print.h"
#include "vmm/std/mem.h"
#include "vmm/std/net.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

in_addr_t
InAddr_of_string ( const char *s )
{
	struct in_addr x;

	ASSERT ( s != NULL );

	Inet_pton ( AF_INET, s, &x );

	return x.s_addr;
}

char *
InAddr_to_string ( in_addr_t x )
{
	struct in_addr y;
	char * retval = ( char * ) Malloc ( INET_ADDRSTRLEN );

	y.s_addr = x;
	Inet_ntop ( AF_INET, ( const void* ) &y, retval, INET_ADDRSTRLEN );

	return retval;;
}

in_addr_t 
InAddr_resolve ( const char *name )
{
	ASSERT ( name != NULL );

	{
		struct in_addr x;
		int retval = inet_pton ( AF_INET, name, &x );
		if  ( retval > 0 )
			return x.s_addr;
	}

	{
		struct hostent hp;    
		bool_t success;
		in_addr_t retval;

		success = Gethostbyname ( name, &hp );
		if  ( ! success )
			return INADDR_NONE;

		Mmove ( &retval, *hp.h_addr_list, sizeof ( in_addr_t ) );
		return retval;
	}
}

void 
InAddr_print ( FILE *stream, in_addr_t x )
{
	char * s = InAddr_to_string ( x );

	ASSERT ( stream != NULL );

	Print ( stream, "%s", s );
	Free ( s );
}

void 
InAddr_println ( FILE *stream, in_addr_t x )
{
	InAddr_print ( stream, x );
	Print_newline ( stream );
}
