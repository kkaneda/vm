#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


int
main ( int argc, char *argv[] )
{
	char *access_kind;

	if ( argc != 2 ) {
		fprintf ( stderr, "Usage: %s {read|write}", argv[0] );
		exit ( 1 );
	}

	access_kind = argv[1];

	fprintf ( stderr, "start.\n" );

	while ( 1 ) {
		int ret;
		float v;
		char s[1024], a[1024];
		unsigned int n;
		
		ret = scanf ( "%f%s%s", &v, a, s );

		if ( ret == 0 )
			break;
		
		if ( ret == EOF )
			break;

		if ( strcmp ( a, access_kind ) == 0 ) {
			sscanf ( s + 2, "%x", &n );

			printf ( "%f %d\n", v, n >> 12 );
		}
	}

	fprintf ( stderr, "finish.\n" );	
	
	return 0;
}
