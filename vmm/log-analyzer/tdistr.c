#include <stdlib.h>
#include <stdio.h>
#include <assert.h>


enum {
	LOG_SIZE = 10000,
	RATIO    = 1000000 
};

static void
init_log ( int log[] )
{
	int i;

	for ( i = 0; i < LOG_SIZE; i++ ) {
		log[i] = 0;
	}
}

static void
calc_sum ( int log[] )
{
	while ( 1 ) {
		int ret;
		float v;
		
		ret = scanf ( "%f", &v );
		
		if ( ret == 0 )
			break;
		
		if ( ret == EOF )
			break;
	
		assert ( v * RATIO < LOG_SIZE );
		
		log[(int)(v* RATIO)] += 1;
	}
}

static void
print_result ( int log[] )
{
	int i;

	for ( i = 0; i < LOG_SIZE; i++ ) {
		if (  log[i] > 0 ) {
			printf ( "%d %d\n", i , log[i] );
		}
	}
	fflush ( stdout );
}

int
main ( int argc, char *argv[] )
{
	int log[LOG_SIZE];

	init_log ( log );

	fprintf ( stderr, "start.\n" );
	calc_sum ( log );
	fprintf ( stderr, "finish.\n" );
	
	print_result ( log );
 
	return 0;
}
