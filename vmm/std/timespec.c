#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include "vmm/std/timespec_common.h"
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

/*
 struct timeval {
 time_t     tv_sec;  // seconds
 suseconds_t  tv_usec; // microseconds
 };

 struct timespec {
 time_t  tv_sec;    // seconds
 long   tv_nsec;    // nanoseconds
 };
*/

time_t
Time ( time_t *t ) 
{
	time_t ret;

	ret = Time ( t );
	if ( ret == ( time_t) -1 ) {
		Sys_failure ( "time" );
	}

	return ret;
}


static void Clock_gettime ( clockid_t clk_id, struct timespec *tp );
static struct timespec Timespec_of_timeval ( const struct timeval x );
double Timespec_elapsed ( const struct timespec start_time );

void
Gettimeofday ( struct timeval *tv, void *foo )
{
	int retval;

	ASSERT ( tv != NULL );

	retval = gettimeofday ( tv, foo );
	if ( retval == -1 ) 
		Sys_failure ( "gettimeofday" );
}

double
Timeval_elapsed ( const struct timeval start_time )
{
	struct timespec x;

	x = Timespec_of_timeval ( start_time );

	return Timespec_elapsed ( x );
}

struct timeval
Timeval_add ( const struct timeval x, long long usec_delta )
{
	struct timeval retval;
	long long sec_delta, usec;
	const long long K = ( 1000LL * 1000LL );

	usec = usec_delta + ( long long )x.tv_usec;
	sec_delta = usec / K;
	usec %= K;

	retval.tv_sec = x.tv_sec + ( int )sec_delta;
	retval.tv_usec = ( int )usec;

	return retval;
}

#ifdef HAVE_CLOCK_GETTIME

static void
Clock_gettime ( clockid_t clk_id, struct timespec *tp )
{
	int retval;

	ASSERT ( tp != NULL );

	retval = clock_gettime ( CLOCK_REALTIME, tp );
	if ( retval == -1 ) 
		Sys_failure ( "clock_gettime" );
}

#else /* HAVE_CLOCK_GETTIME */

static void 
Clock_gettime ( clockid_t clk_id, struct timespec *tp )
{
	struct timeval timeval;

	ASSERT ( tp != NULL );
	ASSERT ( clk_id == CLOCK_REALTIME );

	Gettimeofday ( &timeval, NULL );
	*tp = Timespec_of_timeval ( timeval );
}

#endif /* HAVE_CLOCK_GETTIME */

void
Setitimer ( int which, const struct itimerval *value, struct itimerval *ovalue )
{
	int retval;

	ASSERT ( value != NULL );

	retval = setitimer ( which, value, ovalue );

	if ( retval == -1 ) 
		Sys_failure ( "setitimer" );
}

struct timeval
Timeval_of_second ( double sec )
{
	struct timeval retval;
	double n;

	n = floor ( sec );
	retval.tv_sec = ( int )n;
	retval.tv_usec = ( int ) ( ( sec - n ) * 1000.0 * 1000.0 );

	return retval;
}

static struct timespec
Timespec_of_timeval ( const struct timeval x )
{
	struct timespec retval;

	retval.tv_sec = x.tv_sec;
	retval.tv_nsec = x.tv_usec * 1000;

	return retval;
}

long long
Timespec_to_msec ( struct timespec x )
{
	return ( ( ( long long int )x.tv_sec ) * 1000  +
		 ( ( long long int )x.tv_nsec ) / 1000 / 1000 );
	    
}

struct timespec
Timespec_current ( void )
{
	struct timespec retval;

	Clock_gettime ( CLOCK_REALTIME, &retval );

	return retval;
}

double
Timespec_diff ( const struct timespec x, const struct timespec y )
{
	return ( ( double ) ( x.tv_sec - y.tv_sec ) + 
		 ( ( double ) ( x.tv_nsec - y.tv_nsec ) ) / ( 1000.0 * 1000.0 * 1000.0 ) );
}

double
Timespec_elapsed ( const struct timespec start_time )
{
	struct timespec current;

	current = Timespec_current ( );

	return Timespec_diff ( current, start_time );
}


struct timespec
Timespec_add ( const struct timespec x, double sec )
{
	struct timespec retval;
	double n, m;
	double K = 1000.0 * 1000.0 * 1000.0;

	n = floor ( sec );
	m = ( double )x.tv_nsec / K + ( sec - n );
	n += floor ( m );
	m = ( m - floor ( m ) ) * K;

	retval.tv_sec = x.tv_sec + ( int )n;
	retval.tv_nsec = ( int )m;

	return retval;
}

struct timespec
Timespec_add2 ( const struct timespec x, long long usec )
{
	struct timespec retval;
	long long n, m;
	const long long K = ( 1000LL * 1000LL );

	n = usec / K;
	m = usec % K + ( ( long long )x.tv_nsec / 1000LL );
	n += m / K;
	m = ( m % K ) * 1000LL;

	retval.tv_sec = x.tv_sec + ( int )n;
	retval.tv_nsec = ( int )m;

	return retval;
}

struct timespec
Timespec_add3 ( const struct timespec x, long long nsec )
{
	struct timespec retval;
	long long n, m;
	const long long K = ( 1000LL * 1000LL * 1000LL );

	m = nsec + ( long long ) x.tv_nsec;
	n = m / K;
	m = m % K;

	retval.tv_sec = x.tv_sec + ( int ) n;
	retval.tv_nsec = ( int ) m;

	return retval;
}

bit64u_t ticks_per_sec = 1601.376 * 1000.0 * 1000.0;

static bit64u_t
get_clock ( void )
{
	struct timeval tv;

	Gettimeofday ( &tv, NULL );

	return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void 
cpu_calibrate_ticks ( void )
{
	struct time_counter_t c;
	bit64u_t usec;
	
	init_time_counter ( &c );

	usec  = get_clock ( );
	start_time_counter ( &c );	

	usleep ( 50 * 1000 );

	usec = get_clock ( ) - usec;
	stop_time_counter ( &c );

	ticks_per_sec = ( ( c.end - c.start ) * 1000000LL + ( usec >> 1 ) ) / usec;
}

void 
sleep_forever ( void )
{
	for ( ; ; )
		Sleep ( 1 );
}
