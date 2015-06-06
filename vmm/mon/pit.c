#include "vmm/mon/mon.h"
#include <unistd.h>

enum {
	CLOCK_ID   = 0,
	SPEAKER_ID = 2
};


enum {
	/* Time interrupt interval = 10 mili sec */
	TIMER_INTERVAL_SEC = 0, 
	TIMER_INTERVAL_USEC = 10000
};

enum {
	/* PIT input frequency */
	PIT_FREQ = 1193182 
};

#define PIT_POLLING_THREAD


static struct pit_channel_t *
get_speaker ( struct pit_t *pit )
{
	return &pit->channels[SPEAKER_ID];
}

static bit64u_t
get_delta ( struct pit_channel_t *x, bit64u_t current_time )
{
	return muldiv64 ( current_time - x->count_load_time, PIT_FREQ, ticks_per_sec );
}

static bit64u_t
get_delta2 ( struct pit_channel_t *x )
{
	bit64u_t current_time;

	rdtsc ( current_time );
	return get_delta ( x, current_time );
}

static bool_t
__channel_get_out ( struct pit_channel_t *x, bit64u_t d )
{
	bool_t out = FALSE;

	switch ( x->mode ) {
	case PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT:
		out = ( d >= x->count ); 
		break;
	case PIT_MODE_PROGRAMMABLE_ONE_SHOT:	
		out = ( d < x->count ); 
		break;
	case PIT_MODE_RATE_GENERATOR:	
		out = ( ( ( d % x->count ) == 0 ) && ( d != 0 ) ); 
		break;
	case PIT_MODE_SQUARE_WAVE_RATE_GENERATOR:	
		out = ( d % x->count ) < ( ( x->count + 1 ) >> 1 ); 
		break;
	case PIT_MODE_SOFTWARE_TRIGGERED_STROBE:
	case PIT_MODE_HARDWARE_TRIGGERED_STROBE:	
		out = ( d == x->count ); 
		break;
	default:
		Match_failure ( "channel_get_out\n" );
	}
	return out;	
}

/* get pit output bit */
static bool_t
channel_get_out ( struct pit_channel_t *x, bit64u_t current_time )
{
	bit64u_t d;

	d = get_delta ( x, current_time );
	return __channel_get_out ( x, d );
}

static bool_t
channel_get_out2 ( struct pit_channel_t *x )
{
	bit64u_t current_time;

	rdtsc ( current_time );
	return channel_get_out ( x, current_time );
}

static bit16u_t
channel_get_count ( struct pit_channel_t *x )
{
	bit64u_t d;
	bit16u_t ret = 0;

	d = get_delta2 ( x );

	switch ( x->mode ) {
	case PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT:
	case PIT_MODE_PROGRAMMABLE_ONE_SHOT:
	case PIT_MODE_SOFTWARE_TRIGGERED_STROBE:
	case PIT_MODE_HARDWARE_TRIGGERED_STROBE:
		ret = SUB_BIT ( ( x->count - d ), 0, 16 );
		break;

	case PIT_MODE_RATE_GENERATOR:
		ret = x->count - ( d % x->count );
		break;

	case PIT_MODE_SQUARE_WAVE_RATE_GENERATOR:
		ret = x->count - ( ( 2 * d ) % x->count );
		break;

	default:
		Match_failure ( "channel_get_count\n" );
	}
	
	return ret;
}

static bit64u_t
get_next_transition_count ( struct pit_channel_t *x, bit64u_t current_count )
{
	bit64u_t d, next_time = 0, base;
	int period2, v;
	
	// d = get_delta ( x, current_time );
	d = current_count;

	switch ( x->mode ) {
	case PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT:
	case PIT_MODE_PROGRAMMABLE_ONE_SHOT:	
		if ( d >= x->count ) {
			return -1; 
		}
		next_time = x->count;
		break;

	case PIT_MODE_RATE_GENERATOR:	
		base = (d / x->count) * x->count;
#if 0
		v = ( ( ( d == base ) && ( d != 0 ) ) ? 0 :  1 );
		next_time = base + x->count + v;
#else
		next_time = base + x->count ;
#endif
		break;

	case PIT_MODE_SQUARE_WAVE_RATE_GENERATOR:	
		base = (d / x->count) * x->count;
		period2 = ((x->count + 1) >> 1);
		v = ((d - base) < period2) ? period2 : x->count;
		next_time = base + v;
		break;
		
	case PIT_MODE_SOFTWARE_TRIGGERED_STROBE:
	case PIT_MODE_HARDWARE_TRIGGERED_STROBE:
		if ( d >= x->count ) {
			return -1; 
		}
		v = ( d == x->count ) ? 1 : 0;
		next_time = x->count + v;
		break;

	default:
		Match_failure ( "get_next_transition_time\n" );
	}
	
	return next_time;
}

static bit64u_t 
pit_count_to_time ( bit64u_t n )
{
	return muldiv64 ( n, ticks_per_sec, PIT_FREQ );	
}

/* return -1 if no transition will occur.  */
static bit64u_t
get_next_transition_time ( struct pit_channel_t *x, bit64u_t current_time, bit64u_t current_count )
{
	static bit64u_t next_time, n;

	n = get_next_transition_count ( x, current_count );
	x->next_transition_count = n;
	next_time = x->count_load_time + pit_count_to_time ( n );

	/* fix potential rounding problems */
	/* XXX: better solution: use a clock at PIT_FREQ Hz */
	if ( next_time <= current_time ) {
		next_time = current_time + 1;
	}

	return next_time;
}

static void
channel_timer_update ( struct pit_channel_t *x, bit64u_t current_time )
{
	static bit64u_t old_delta = 0LL;

	if ( x->id != CLOCK_ID ) {
		return;
	}

	bit64u_t v = x->next_transition_count;

	x->irq_level = __channel_get_out ( x, x->next_transition_count );
	x->next_transition_time = get_next_transition_time ( x, current_time, 
							     x->next_transition_count );
	bit64u_t vv = x->next_transition_count - v;

	if ( x->next_transition_time < 0LL ) {
		return;
	}

	if ( vv != old_delta ) {
		bit64u_t d = x->next_transition_time - current_time;
		struct itimerval itv; 
	
		itv.it_interval.tv_sec = d / ticks_per_sec;

		itv.it_interval.tv_usec = muldiv64 ( d, 1000000, ticks_per_sec );
//		itv.it_interval.tv_usec = muldiv64 ( d, 5000000, ticks_per_sec ); [DEBUG] 
		itv.it_value = itv.it_interval;
	
		assert ( ( itv.it_interval.tv_sec != 0 ) ||
			 ( itv.it_interval.tv_usec != 0 ) );
		
#ifdef PIT_POLLING_THREAD
		x->sleep_time = ( itv.it_interval.tv_sec * 1000000 + itv.it_interval.tv_usec );
#else /* ! PIT_POLLING_THREAD */
		Setitimer ( ITIMER_REAL, &itv, NULL );	
#endif /* ! PIT_POLLING_THREAD */
		old_delta = vv;
	}
}

static void
channel_timer_update_a ( struct pit_channel_t *x, bit64u_t current_time )
{
	Pthread_mutex_lock ( &x->mp ); 
	channel_timer_update ( x, current_time );
	Pthread_mutex_unlock ( &x->mp );
}

static void
channel_timer_update2 ( struct pit_channel_t *x )
{
	bit64u_t current_time;

	rdtsc ( current_time );
	x->count_load_time = current_time;

	channel_timer_update ( x, x->count_load_time );
}

static void
channel_set_gate ( struct pit_channel_t *x, bool_t val )
{
	bool_t saved;

	saved = x->gate;
	x->gate = val;
	
	switch ( x->mode ) {
	case PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT:
	case PIT_MODE_SOFTWARE_TRIGGERED_STROBE:
		break;

	case PIT_MODE_PROGRAMMABLE_ONE_SHOT:
	case PIT_MODE_RATE_GENERATOR:
	case PIT_MODE_SQUARE_WAVE_RATE_GENERATOR:
	case PIT_MODE_HARDWARE_TRIGGERED_STROBE:
		if ( val && ( ! saved ) ) {
			channel_timer_update2 ( x );
		}
		break;

	default:
		Match_failure ( "channel_set_gate\n" );
	}
}

static void
channel_set_gate_a ( struct pit_channel_t *x, bool_t val )
{
	Pthread_mutex_lock ( &x->mp );
	channel_set_gate ( x, val );
	Pthread_mutex_unlock ( &x->mp );		
}

static void
channel_load_count ( struct pit_channel_t *x, int val )
{
	x->count = ( ( val > 0 ) ? val : 0xffff );
	channel_timer_update2 ( x );
}

static void
channel_latch_count ( struct pit_channel_t *x )
{
	if ( x->count_latched == RW_STATE_LATCHING ) {
                /* if already latched, do not latch again */
		return;
	}

	x->latched_count = channel_get_count ( x );
	x->count_latched = x->rw_mode;
}

/****************************************/

static void
channel_init ( struct pit_channel_t *x, int id )
{
	x->id = id;
	x->mode = PIT_MODE_SQUARE_WAVE_RATE_GENERATOR;
	x->gate = ( id != SPEAKER_ID );

	x->rw_mode = x->count_latched = RW_STATE_LATCHING;
	x->read_mode_msb = x->write_mode_msb = FALSE;
	
	x->count = x->latched_count = 0;
	x->gate = FALSE;

	x->status = 0;
	x->status_latched = FALSE;

	x->bcd = FALSE;

	x->irq_level = FALSE;
	rdtsc ( x->count_load_time );	

	channel_load_count ( x, 0 );
	x->next_transition_time = 0LL;
	x->next_transition_count = 0LL;

	x->sleep_time = TIMER_INTERVAL_SEC * 1000000 + TIMER_INTERVAL_USEC;
	Pthread_mutex_init ( &x->mp, NULL );
}

static void
channel_pack ( struct pit_channel_t *x, int fd )
{
	Bit32u_pack ( ( bit32u_t ) x->id, fd );
	Bit32u_pack ( ( bit32u_t ) x->mode, fd );
	Bit32u_pack ( ( bit32u_t ) x->rw_mode, fd );
	Bit32u_pack ( ( bit32u_t ) x->count_latched, fd );
	Bool_pack ( x->read_mode_msb, fd );
	Bool_pack ( x->write_mode_msb, fd );
	Bit8u_pack ( x->write_latch, fd );
	Bit16u_pack ( x->count, fd );
	Bit16u_pack ( x->latched_count, fd );
	Bool_pack ( x->gate, fd );
	Bit8u_pack ( x->status, fd );
	Bool_pack ( x->status_latched, fd );
	Bool_pack ( x->bcd, fd );
	Bool_pack ( x->irq_level, fd );
	Bit64u_pack ( x->count_load_time, fd );
	Bit64u_pack ( x->next_transition_time, fd );
	Bit64u_pack ( x->next_transition_count, fd );
	Bit32u_pack ( x->sleep_time, fd );
}

static void
channel_unpack ( struct pit_channel_t *x, int fd )
{
	x->id = ( int ) Bit32u_unpack ( fd );
	x->mode = ( pit_mode_t ) Bit32u_unpack ( fd );
	x->rw_mode = ( pit_rw_state_t ) Bit32u_unpack ( fd );
	x->count_latched = ( pit_rw_state_t ) Bit32u_unpack ( fd );
	x->read_mode_msb = Bool_unpack ( fd );
	x->write_mode_msb = Bool_unpack ( fd );
	x->write_latch = Bit8u_unpack ( fd );
	x->count = Bit16u_unpack ( fd );
	x->latched_count = Bit16u_unpack ( fd );
	x->gate = Bool_unpack ( fd );
	x->status = Bit8u_unpack ( fd );
	x->status_latched = Bool_unpack ( fd );
	x->bcd = Bool_unpack ( fd );
	x->irq_level = Bool_unpack ( fd );
	x->count_load_time = Bit64u_unpack ( fd );
	x->next_transition_time = Bit64u_unpack ( fd );
	x->next_transition_count = Bit64u_unpack ( fd );
	x->sleep_time = Bit32u_unpack ( fd );
}

/****************************************/

static bit32u_t
channel_read_latched_count ( struct pit_channel_t *x )
{
	bit32u_t ret = 0;

	switch ( x->count_latched ) {
	case RW_STATE_LSB_ONLY:
		ret = SUB_BIT ( x->latched_count, 0, 8 );
		x->count_latched = RW_STATE_LATCHING;
		break;

	case RW_STATE_MSB_ONLY:
		ret = SUB_BIT ( ( x->latched_count >> 8 ), 0, 8 );
		x->count_latched = RW_STATE_LATCHING;
		break;

	case RW_STATE_LSB_MSB:
		ret = SUB_BIT ( x->latched_count, 0, 8 );
		x->count_latched = RW_STATE_MSB_ONLY;
		break;

	default:
		Match_failure ( "__channel_read1\n" );
	}

	return ret;
}

static bit32u_t
channel_read_count ( struct pit_channel_t *x )
{
	bit16u_t count;
	bit32u_t ret = 0;

	count = channel_get_count ( x );

	switch ( x->rw_mode ) {
	case RW_STATE_LSB_ONLY:
		ret = SUB_BIT ( count, 0, 8 );
	        break;

	case RW_STATE_MSB_ONLY:
		ret = SUB_BIT ( ( count >> 8 ), 0, 8 );
		break;

	case RW_STATE_LSB_MSB:
		ret = ( ( x->read_mode_msb ) ?
			SUB_BIT ( ( count >> 8 ), 0, 8 ) :
			SUB_BIT ( count, 0, 8 ) );
		x->read_mode_msb = ! x->read_mode_msb;
		break;

	default:
		Match_failure ( "channel_read_count\n" );
	}

	return ret;
}

static bit32u_t
channel_read ( struct pit_channel_t *x )
{
	bit32u_t ret = 0;

	if ( x->status_latched ) {
		ret = x->status;
		x->status_latched = FALSE;
	} else if ( x->count_latched != RW_STATE_LATCHING ) {
		ret = channel_read_latched_count ( x );
	} else {
		ret = channel_read_count ( x );
	}

	return ret;
}

static bit32u_t
channel_read_a ( struct pit_channel_t *x )
{
	bit32u_t ret = 0;

	Pthread_mutex_lock ( &x->mp );	
	ret = channel_read ( x );
	Pthread_mutex_unlock ( &x->mp );

	return ret;
}

/****************************************/

static void
channel_write_count ( struct pit_channel_t *x, bit32u_t val )
{
	switch ( x->rw_mode ) {
	case RW_STATE_LSB_ONLY:
		channel_load_count ( x, val );
		break;
	case RW_STATE_MSB_ONLY:
		channel_load_count ( x, val << 8 );
		break;
	case RW_STATE_LSB_MSB:
		if ( x->write_mode_msb ) {
			channel_load_count ( x, x->write_latch | ( val << 8 ) );
		} else {
			x->write_latch = val;			
		}
		x->write_mode_msb = ! x->write_mode_msb;
		break;
	default:
		Match_failure ( "channel_write1\n" );
	}
}

static void
channel_write_count_a ( struct pit_channel_t *x, bit32u_t val )
{
	Pthread_mutex_lock ( &x->mp );
	channel_write_count ( x, val );
	Pthread_mutex_unlock ( &x->mp );	
}

static void 
channel_set_modes ( struct pit_channel_t *x, bit32u_t val )
{
	int n;

	n = SUB_BIT ( val, 4, 2 );
	
	if ( n == RW_STATE_LATCHING ) {
		channel_latch_count ( x );
	} else {
		x->rw_mode = n;
		if ( x->rw_mode == RW_STATE_LSB_MSB ) {
			x->read_mode_msb = FALSE;
			x->write_mode_msb = FALSE;
		}
		
		x->mode = SUB_BIT ( val, 1, 3 );
		x->bcd  = TEST_BIT ( val, 0 );
	}	
}

static void 
channel_set_modes_a ( struct pit_channel_t *x, bit32u_t val )
{
	Pthread_mutex_lock ( &x->mp );
	channel_set_modes ( x, val );
	Pthread_mutex_unlock ( &x->mp );
}

static void 
channel_write_status ( struct pit_channel_t *x, bit32u_t val )
{
	if ( ! ( TEST_BIT ( val, 5 ) ) ) {
		channel_latch_count ( x );
	}
	
	if ( ! ( TEST_BIT ( val, 4 ) ) && ( ! x->status_latched ) ) {
		x->status = ( ( channel_get_out2 ( x ) << 7 ) | 
			      ( x->rw_mode << 4 ) | 
			      ( x->mode << 1 ) |
			      ( x->bcd ) );
		x->status_latched = TRUE;
	}
}

static void 
channel_write_status_a ( struct pit_channel_t *x, bit32u_t val )
{
	Pthread_mutex_lock ( &x->mp );
	if ( TEST_BIT ( val, ( x->id + 1 ) ) ) {
		channel_write_status ( x, val );
	}
	Pthread_mutex_unlock ( &x->mp );	
}

/****************************************/
/****************************************/
/****************************************/

static void set_pit_poll ( struct mon_t *mon );

void
Pit_init ( struct pit_t *x, struct mon_t *mon )
{
	int i;

	ASSERT ( x != NULL );

	x->speaker_data_on = TRUE;
	x->dummy_refresh_clock = TRUE;

	for ( i = 0; i < NR_PIT_CHANNELS; i++ ) {
		channel_init ( &x->channels[i], i );
	}

	set_pit_poll ( mon );       
}

/*****************************************/

void 
Pit_pack ( struct pit_t *x, int fd )
{
	int i;

	Bool_pack ( x->speaker_data_on, fd );
	Bool_pack ( x->dummy_refresh_clock, fd );

	for ( i = 0; i < NR_PIT_CHANNELS; i++ ) {
		channel_pack ( &x->channels[i], fd );
	}
}

void 
Pit_unpack ( struct pit_t *x, int fd )
{
	int i;

	x->speaker_data_on = Bool_unpack ( fd );
	x->dummy_refresh_clock = Bool_unpack ( fd );

	for ( i = 0; i < NR_PIT_CHANNELS; i++ ) {
		channel_unpack ( &x->channels[i], fd );
	}
}

/*****************************************/

static bit32u_t
speaker_read ( struct pit_t *pit )
{
	struct pit_channel_t *x = get_speaker ( pit );
	
	pit->dummy_refresh_clock ^= 1;

	return ( ( pit->speaker_data_on << 1 ) |
		 ( x->gate ) |
		 ( pit->dummy_refresh_clock << 4 ) |
		 ( channel_get_out2 ( x ) << 5 ) );
}

static void
speaker_write_a ( struct pit_t *pit, bit32u_t val )
{
	struct pit_channel_t *x = get_speaker ( pit );

	pit->speaker_data_on = TEST_BIT ( val, 1 );
	channel_set_gate_a ( x, TEST_BIT ( val, 0 ) );
}

/*****************************************/

bit32u_t
Pit_read ( struct pit_t *pit, bit16u_t addr, size_t len )
{
	int n;
	bit32u_t ret = 0;

	ASSERT ( pit != NULL );

//	Print_color ( stdout, CYAN, "Pit_read: addr=%#x\n", addr );

	switch ( addr ) {
	case PIT_PORT ( 0 ):
	case PIT_PORT ( 1 ):
	case PIT_PORT ( 2 ):
		n = SUB_BIT ( addr, 0, 2 );
		ret = channel_read_a ( &pit->channels[n] );	
		break;

	case SYSTEM_CONTROL_PORT:
		ret = speaker_read ( pit ); 
		break;

	default:
		Match_failure ( "Pit_read: addr=%#x\n", addr );
	}

	return ret;
}

/*****************************************/

static void 
__pit_write ( struct pit_t *pit, bit32u_t val )
{
	int n;
	int i;
	
	n = val >> 6;

	if ( n < 3 ) {
		channel_set_modes_a ( &pit->channels[n], val );
		return;
	}

	/* read back command */
	for ( i = 0; i < NR_PIT_CHANNELS; i++ ) {
		channel_write_status_a ( &pit->channels[i], val );
	}	
}

void
Pit_write ( struct pit_t *pit, bit16u_t addr, bit32u_t val, size_t len )
{
	int n;

	ASSERT ( pit != NULL );

	//Print_color ( stdout, CYAN, "Pit_write: addr=%#x, val=%#x\n", addr, val );
	
	switch ( addr ) {
	case PIT_PORT ( 0 ):
	case PIT_PORT ( 1 ):
	case PIT_PORT ( 2 ):
		n = SUB_BIT ( addr, 0, 2 );
		channel_write_count_a ( &pit->channels[n], val );	
		break;

	case PIT_PORT ( 3 ):
		__pit_write ( pit, val );		
		break;

	case SYSTEM_CONTROL_PORT:
		speaker_write_a ( pit, val );
		break;

	default:
		Match_failure ( "Pit_write: addr=%#x\n", addr );
	}
}

/*****************************************/

/* [TODO] guaranntee of atomic access to variables with mutex lock */
static void
__pit_poll ( struct mon_t *mon )
{
	struct pit_t *pit;
	struct pit_channel_t *x;

	ASSERT ( mon != NULL );

	pit = &mon->devs.pit;
	x = &pit->channels [ CLOCK_ID ];

	if ( is_bootstrap_proc ( mon ) ) {
		channel_timer_update_a ( x, x->next_transition_time );
	}

	if ( ( is_native_mode ( mon ) ) && ( Pit_check_irq ( pit ) ) ) {
		Kill ( mon->pid, SIGALRM );
	}
}

#ifdef PIT_POLLING_THREAD

static void *
Pit_polling_thread ( void *arg )
{
	struct mon_t *mon = ( struct mon_t *) arg;
	struct pit_t *pit = &mon->devs.pit;
	struct pit_channel_t *x = &pit->channels [ CLOCK_ID ];

	while ( TRUE ) {
		__pit_poll ( mon ); 
		mon->stat.nr_pit_interrupts++;
#if 0
		usleep ( x->sleep_time ); 
#else
		usleep ( x->sleep_time * 5.0 ); // [DEBUG] 
#endif
	}

	return NULL;
}

static void
set_pit_poll ( struct mon_t *mon )
{
	pthread_t tid;
	Pthread_create ( &tid, NULL, &Pit_polling_thread, ( void * ) mon );
}

#else /* ! PIT_POLLING_THREAD */

static struct mon_t *static_mon = NULL;

static void
Pit_poll ( int sig )
{
	__pit_poll ( static_mon );
}

static void
set_pit_poll ( struct mon_t *mon )
{
	struct itimerval itv; 
	
	static_mon = mon;

	Signal ( SIGALRM, Pit_poll );

	itv.it_interval.tv_sec = TIMER_INTERVAL_SEC;
	itv.it_interval.tv_usec = TIMER_INTERVAL_USEC;
	itv.it_value = itv.it_interval;

	Setitimer ( ITIMER_REAL, &itv, NULL );	
}

#endif /* PIT_POLLING_THREAD */

void
Pit_stop_timer ( struct pit_t *pit )
{
	ASSERT ( pit != NULL );
//	pit->emulation_start_time = Timespec_current ( );
}

void
Pit_restart_timer ( struct pit_t *pit )
{
	ASSERT ( pit != NULL );
//	pit->emulation_time += Timespec_elapsed ( pit->emulation_start_time );
}

/*****************************************/

int 
Pit_try_get_irq ( struct pit_t *pit )
{
	struct pit_channel_t *x;

	ASSERT ( pit != NULL );

	x = &pit->channels[CLOCK_ID];

	Pthread_mutex_lock ( &x->mp );

	if ( ! x->irq_level ) {
		Pthread_mutex_unlock ( &x->mp );
		return IRQ_INVALID;
	}

	/* [TODO] */
	if ( x->mode == PIT_MODE_RATE_GENERATOR ) {
		 x->irq_level = FALSE;
	}

	Pthread_mutex_unlock ( &x->mp );

	// Print ( stdout, "Pit_try_get_irq: %#x\n", IRQ_TIMER );

	return IRQ_TIMER;
}

int 
Pit_check_irq ( struct pit_t *pit )
{
	struct pit_channel_t *x = &pit->channels[CLOCK_ID];
	int ret;
	
	ASSERT ( pit != NULL );
	
	Pthread_mutex_lock ( &x->mp );
	ret = x->irq_level;
	Pthread_mutex_unlock ( &x->mp );

	return ret;
}
