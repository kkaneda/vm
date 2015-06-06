#ifndef _VMM_MON_STAT_H
#define _VMM_MON_STAT_H

#include "vmm/common.h"

enum {
//	FETCH_HISOTRY_SIZE = 4096
	FETCH_HISOTRY_SIZE = 0x10000
};

struct fetch_history_t {
	long long		elapsed_time_count;
	long long		comm_time_count;
	mem_access_kind_t	kind;
	int			vaddr;
	int			eip, esp;
	bool_t			is_paddr_access;
};

struct stat_t {
	struct timespec		exec_start_time;
	struct time_counter_t	exec_start_counter;

	struct time_counter_t	guest_counter, guest_kernel_counter, guest_user_counter;
	struct time_counter_t	vmm_counter;
	struct time_counter_t	emu_counter;
	struct time_counter_t	emu_counter_sub[NR_HANDLER_KINDS];
	struct time_counter_t	dev_rd_counter;
	struct time_counter_t	dev_wr_counter;
	struct time_counter_t	comm_counter;

	struct time_counter_t	mhandler_counter;
	struct time_counter_t	shandler_counter;
	struct time_counter_t	ihandler_counter;
	struct time_counter_t	halt_counter;

	struct time_counter_t	pit_counter, apic_timer_counter; /* [DEBUG] */

	unsigned long long	nr_traps;
	unsigned long long	nr_syscalls;
	unsigned long long	nr_io;
	unsigned long long	nr_dev_rd, nr_dev_wr;

	long long		min_dev_rd_count, max_dev_rd_count;
	long long		min_dev_wr_count, max_dev_wr_count;

	unsigned long long	nr_emulation_enter[NR_HANDLER_KINDS];

	bool_t			comm_counter_flag;
	bool_t			halt_counter_flag;

	unsigned long long	nr_fetch_requests;
	struct fetch_history_t	fetch_history[FETCH_HISOTRY_SIZE];

	unsigned long long	nr_interrupts[256];
	struct time_counter_t	interrupt_counters[256];
	int			nr_emulated_instr[0x10000];
	char *			nr_emulated_instr_name[0x10000];

	unsigned long long	nr_pit_interrupts, nr_apic_timer_interrupts, nr_other_sigs, 
		nr_instr_emu, nr_traps_at_kernel, nr_hlts, nr_irets;
	int kernel_state;	
};

void Stat_init ( struct stat_t *x );
void Stat_print ( FILE *stream, struct stat_t *x );


#endif /* _VMM_MON_STAT_H */
