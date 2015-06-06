#ifndef _VMM_STD_TIMESPEC_COMMON_H
#define _VMM_STD_TIMESPEC_COMMON_H

#include "vmm/std/types.h"


struct time_counter_t {
	long long start;
	long long end;
	long long sum;
};

extern bit64u_t ticks_per_sec;


#define rdtsc( t ) \
{ \
	register unsigned long low, high; \
	__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high));  \
	t = (long long)low + ( ( (long long)high) << 32);  \
}

#define init_time_counter( c ) { (c)->start = (c)->end = (c)->sum = 0LL; }
#define start_time_counter( c ) { rdtsc ( (c)->start ); (c)->end = (c)->start; }
#define stop_time_counter( c ) { rdtsc ( (c)->end ); (c)->sum += ( (c)->end - (c)->start ); }
#define time_counter_to_sec( c ) ( ( ( double ) (c)->sum ) / ticks_per_sec )

#define count_to_sec( c ) ( ( ( double ) c ) / ticks_per_sec )


#endif /* _VMM_STD_TIMESPEC_COMMON_H */
