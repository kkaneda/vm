#ifndef _VMM_STD_TIMESPEC_H
#define _VMM_STD_TIMESPEC_H

#include "vmm/std/types.h"
#include "vmm/std/timespec_common.h"
#include <time.h>
#include <sys/time.h>


time_t          Time ( time_t *t );

void		Gettimeofday ( struct timeval *tv, void *foo );
double          Timeval_elapsed ( const struct timeval start_time );
struct timeval  Timeval_add ( const struct timeval x, long long usec );

void            Setitimer ( int which, const struct itimerval *value, struct itimerval *ovalue );

struct timeval	Timeval_of_second ( double sec );
long long       Timespec_to_msec ( struct timespec x );
struct timespec Timespec_current ( void );
double 		Timespec_diff ( const struct timespec x, const struct timespec y );
double 		Timespec_elapsed ( const struct timespec start_time );
struct timespec Timespec_add ( const struct timespec x, double sec );
struct timespec Timespec_add2 ( const struct timespec x, long long usec );
struct timespec Timespec_add3 ( const struct timespec x, long long nsec );

extern bit64u_t ticks_per_sec;
void cpu_calibrate_ticks ( void );

void sleep_forever ( void );

#endif /* _VMM_STD_TIMESPEC_H */
