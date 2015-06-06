#ifndef _VMM_MON_APIC_H
#define _VMM_MON_APIC_H

#include "vmm/common.h"

struct generic_apic_t {
     bit8u_t			id;    
     bit32u_t			base_addr;
#ifdef ENABLE_MP
     struct comm_t		*comm;
#endif
};

enum model {
     MODEL_CLUSTER		= 0x0, /* 0000B */
     MODEL_FLAT			= 0xf  /* 1111B */
};
typedef enum model		model_t;

struct local_vector_table_t {
     bit32u_t 			timer;
     bit32u_t			lint0;
     bit32u_t			lint1;
     bit32u_t			error;
     bit32u_t			perf_mon_counter;
     bit32u_t	 		thermal_sensor;
};

struct timer_t {
	bit32u_t		initial_count;
	bit32u_t		current_count;
	bool_t			active;
	bit4u_t			div_conf;
	bit32u_t		div_factor;
	bit64u_t		ticks_initial;
};

struct startup_info_t {
     int			num;
     int 			vector;
     bool_t			need_handle;
};

struct local_apic_t {
     struct generic_apic_t	*gapic;

     bit8u_t			task_priority;
     bit8u_t			arb_priority;
     bit8u_t			logical_id_map[NUM_OF_PROCS];
     model_t			model;		/* 0000B ==> flat, 1111B ==> cluster */
     bit32u_t			spurious_vector;
     bit32u_t			error_status;
     bit32u_t			interrupt_command[2];	/* low ==> ic[0], high ==> ic[1] */
     struct local_vector_table_t local_vector_table;
     struct timer_t		timer;
     bool_t			interrupt_request[MAX_OF_IVECTOR];
     bool_t			in_service[MAX_OF_IVECTOR];
     bool_t			trigger_mode[MAX_OF_IVECTOR];

     bool_t			INTR;

     struct startup_info_t	startup_info;

     pthread_mutex_t		mp;
     pthread_cond_t		cond;
     bit32u_t 			sleep_time;
     pid_t pid;
};


struct local_apic_t *LocalApic_create(int id, struct comm_t *comm, pid_t pid);
void                 LocalApic_pack ( struct local_apic_t *x, int fd );
void                 LocalApic_unpack ( struct local_apic_t *x, int fd );
void                 LocalApic_print(FILE *stream, struct local_apic_t *apic);
bool_t               LocalApic_is_selected(struct local_apic_t *apic, bit32u_t paddr, size_t len);
bit32u_t             LocalApic_read(struct local_apic_t *apic, bit32u_t paddr, size_t len);
void                 LocalApic_write(struct local_apic_t *apic, bit32u_t paddr, bit32u_t val, size_t len);
void                 LocalApic_handle_request_a(struct local_apic_t *apic, struct interrupt_command_t *ic);
int                  LocalApic_try_acknowledge_interrupt(struct local_apic_t *apic);
bool_t               LocalApic_check_interrupt ( struct local_apic_t *apic );

enum polarity {
     POLARITY_HIGH 		= 0,
     POLARITY_LOW 		= 1
};
typedef enum polarity		polarity_t;

/* [Reference] Intel IOAPIC pp.11-13 */
struct iored_entry_t {
     bit32u_t		vals[2];

     bit8u_t		vector;    
     delivery_mode_t	delivery_mode;
     dest_mode_t	dest_mode;
     bool_t		delivery_status;
     polarity_t		polarity;
     bool_t		remote_irr;
     trig_mode_t	trig_mode;
     bool_t		interrupt_mask;
     bit8u_t		dest;    
};

enum {
     NUM_OF_IORED_ENTRIES 	= 0x18
};

struct io_apic_t {    
     struct generic_apic_t	*gapic;
     struct local_apic_t	*local_apic;
     bit8u_t			ioregsel; /* I/O register selector register 
					   * [Refence] Intel IOAPIC p.9 */
     struct iored_entry_t 	ioredtbl[NUM_OF_IORED_ENTRIES]; /* I/O redirecgtion table registers 
								 * [Reference] Intel IOAPIC p.11 */

     bool_t			interrupt_request[32];
};

struct io_apic_t *IoApic_create(int id, struct comm_t *comm, struct local_apic_t *local_apic);
void              IoApic_pack ( struct io_apic_t *x, int fd );
void              IoApic_unpack ( struct io_apic_t *x, int fd );
bool_t            IoApic_is_selected(const struct io_apic_t *apic, bit32u_t paddr, size_t len);
void              IoApic_trigger(struct io_apic_t *apic, int irq);
bit32u_t          IoApic_read(struct io_apic_t *apic, bit32u_t paddr, size_t len);
void              IoApic_write(struct io_apic_t *apic, bit32u_t paddr, bit32u_t val, size_t len);


#endif /* _VMM_MON_APIC_H */
