#ifndef _VMM_MON_PIT_H
#define _VMM_MON_PIT_H

#include "vmm/common.h"


#ifndef PIT_PORT
# define PIT_PORT( x )		 ( 0x40 + ( x ) )
#endif

#ifndef SYSTEM_CONTROL_PORT
# define SYSTEM_CONTROL_PORT 	0x61
#endif


enum pit_rw_state {
	RW_STATE_LATCHING = 0,
	RW_STATE_LSB_ONLY = 1,
	RW_STATE_MSB_ONLY = 2,
	RW_STATE_LSB_MSB  = 3
};
typedef enum pit_rw_state 	pit_rw_state_t;

enum pit_mode {
	PIT_MODE_INTERRUPT_ON_TERMINAL_COUNT = 0,
	PIT_MODE_PROGRAMMABLE_ONE_SHOT	     = 1,
	PIT_MODE_RATE_GENERATOR		     = 2,
	PIT_MODE_SQUARE_WAVE_RATE_GENERATOR  = 3,
	PIT_MODE_SOFTWARE_TRIGGERED_STROBE   = 4,
	PIT_MODE_HARDWARE_TRIGGERED_STROBE   = 5
};
typedef enum pit_mode	 	pit_mode_t;

struct pit_channel_t {
	int		id;

	pit_mode_t 	mode;

	pit_rw_state_t	rw_mode, count_latched;
	bool_t		read_mode_msb, write_mode_msb; /* used when rw_mode = RW_STATE_LSB_MSB */
	bit8u_t		write_latch; /* for temporaliry saving the write value */

	bit16u_t 	count, latched_count;

	bool_t 		gate;

	bit8u_t		status;
	bool_t		status_latched;
	
	bool_t 		bcd; /* not supported */


	bool_t		irq_level;
	bit64u_t 	count_load_time, next_transition_time, next_transition_count;
	bit32u_t 	sleep_time;

	pthread_mutex_t mp;
};
	
enum {
	NR_PIT_CHANNELS = 3
};

struct pit_t {
	bool_t			speaker_data_on;
	bool_t			dummy_refresh_clock;
	struct pit_channel_t 	channels[NR_PIT_CHANNELS];
};


void     Pit_init ( struct pit_t *x, struct mon_t *mon );
void     Pit_pack ( struct pit_t *x, int fd );
void     Pit_unpack ( struct pit_t *x, int fd );
bit32u_t Pit_read ( struct pit_t *pit, bit16u_t addr, size_t len );
void     Pit_write ( struct pit_t *pit, bit16u_t addr, bit32u_t val, size_t len );
int      Pit_try_get_irq ( struct pit_t *pit );
int      Pit_check_irq ( struct pit_t *pit );
void     Pit_stop_timer ( struct pit_t *pit );
void     Pit_restart_timer ( struct pit_t *pit );

#endif /*_VMM_MON_PIT_H */

