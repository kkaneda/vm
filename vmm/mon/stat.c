#include "vmm/mon/stat.h"

void
Stat_init ( struct stat_t *x )
{
	int i;

	ASSERT ( x != NULL );

	x->exec_start_time = Timespec_current ();
	init_time_counter ( &x->exec_start_counter );
	start_time_counter( &x->exec_start_counter );

	x->nr_traps = 0LL;
	x->nr_syscalls = 0LL;
	x->nr_io = 0LL;

	x->nr_dev_rd = 0LL;
	x->nr_dev_wr = 0LL;

	x->min_dev_rd_count = x->max_dev_rd_count = 0LL;
	x->min_dev_wr_count = x->max_dev_wr_count = 0LL;

	init_time_counter ( &x->guest_counter );
	init_time_counter ( &x->guest_user_counter );
	init_time_counter ( &x->guest_kernel_counter );

	init_time_counter ( &x->vmm_counter );
	init_time_counter ( &x->emu_counter );
	init_time_counter ( &x->dev_rd_counter );
	init_time_counter ( &x->dev_wr_counter );
	init_time_counter ( &x->comm_counter );

	init_time_counter ( &x->pit_counter ); /* [DEBUG] */
	init_time_counter ( &x->apic_timer_counter ); /* [DEBUG] */

	init_time_counter ( &x->shandler_counter );
	init_time_counter ( &x->ihandler_counter );
	init_time_counter ( &x->mhandler_counter );
	init_time_counter ( &x->halt_counter );

	for ( i = 0; i < NR_HANDLER_KINDS; i++ ) {
		init_time_counter ( &x->emu_counter_sub[i] );		
		x->nr_emulation_enter[i] = 0LL;
	}

	x->comm_counter_flag = FALSE;
	x->halt_counter_flag = FALSE;

	x->nr_fetch_requests = 0LL;
	for ( i = 0; i < FETCH_HISOTRY_SIZE; i++ ) {
		struct fetch_history_t *h = &(x->fetch_history[i]);

		h->elapsed_time_count = 0LL;
		h->comm_time_count = 0LL;
		h->kind = -1;
		h->vaddr = h->eip = h->esp = 0;
		h->is_paddr_access = FALSE;
	}

	x->nr_pit_interrupts = 0LL;
	x->nr_apic_timer_interrupts = 0LL;
	x->nr_other_sigs = 0LL;
	x->nr_instr_emu = 0LL;
	x->nr_traps_at_kernel = 0LL;
	x->nr_hlts = 0LL;
	x->nr_irets = 0LL;

	x->kernel_state = -1;
	for ( i = 0; i < 256; i++ ) {
		x->nr_interrupts [ i ] = 0LL;
		init_time_counter ( &x->interrupt_counters [ i ]  );
	}

	for ( i = 0; i < 0x1000; i ++ ) {
		x->nr_emulated_instr [ i ] = 0; 
		x->nr_emulated_instr_name [i] = NULL;
	}
}

static void
print_execution_time ( FILE *stream, struct stat_t *stat )
{
	int i;

	Print ( stream, "  (Total) = %f\n", Timespec_elapsed ( stat->exec_start_time ) );
	Print ( stream, "     |  \n" );
    	Print ( stream, "     +-- (Guest) = %f\n", time_counter_to_sec ( &stat->guest_counter ) );
	Print ( stream, "     |     |  \n" );
    	Print ( stream, "     |     +-- (User)   = %f\n", time_counter_to_sec ( &stat->guest_user_counter ) );
    	Print ( stream, "     |     +-- (Kernel) = %f (# of traps = %lld)\n", 
		time_counter_to_sec ( &stat->guest_kernel_counter ), stat->nr_traps_at_kernel );
//	Print ( stream, "     |           |  \n" );
//	Print ( stream, "     |           +-- (pg fault) = %f\n", time_counter_to_sec ( &stat->pg_fault_counter ) );
//	Print ( stream, "     |           +-- (syscall) = %f\n", time_counter_to_sec ( &stat->syscall_counter ) );
	Print ( stream, "     | \n" );
    	Print ( stream, "     +-- (VMM)   = %f (# of traps = %lld)\n", 
		time_counter_to_sec ( &stat->vmm_counter ), stat->nr_traps );
	Print ( stream, "           |  \n" );
    	Print ( stream, "           +-- (Msg) = %f\n", time_counter_to_sec ( &stat->mhandler_counter ) );
    	Print ( stream, "           +-- (Int) = %f\n", time_counter_to_sec ( &stat->ihandler_counter ) );
    	Print ( stream, "           +-- (Sig) = %f\n", time_counter_to_sec ( &stat->shandler_counter ) );
	Print ( stream, "                 |  \n" );

	long long n = ( stat->nr_emulation_enter[HANDLE_PAGEFAULT] +
			stat->nr_emulation_enter[SET_CNTL_REG] +
			stat->nr_emulation_enter[INVALIDATE_TLB] +
			stat->nr_emulation_enter[CHANGE_PAGE_PROT] );

    	Print ( stream, "                 +-- (Emu)    = %f (%lld x %f)\n",
		time_counter_to_sec ( &stat->emu_counter ),
		n,
		time_counter_to_sec ( &stat->emu_counter ) / ( ( double ) n ) );

    	Print ( stream, "                 +-- (Dev RD) = %f (%lld x %f)\n",
		time_counter_to_sec ( &stat->dev_rd_counter ),
		stat->nr_dev_rd,
		time_counter_to_sec ( &stat->dev_rd_counter ) / ( ( double ) stat->nr_dev_rd ) );

    	Print ( stream, "                 +-- (Dev WR) = %f (%lld x %f)\n",
		time_counter_to_sec ( &stat->dev_wr_counter ),
		stat->nr_dev_wr,
		time_counter_to_sec ( &stat->dev_wr_counter ) / ( ( double ) stat->nr_dev_wr ) );

    	Print ( stream, "                 +-- (Comm)   = %f (%lld x %f)\n", 
		time_counter_to_sec ( &stat->comm_counter ),
		stat->nr_fetch_requests,
		time_counter_to_sec ( &stat->comm_counter ) / ( ( double ) stat->nr_fetch_requests ) );

    	Print ( stream, "                 +-- (Halt)   = %f (%lld)\n", 
		time_counter_to_sec ( &stat->halt_counter ),
		stat->nr_hlts );
	Print ( stream, "\n" );

	Print ( stream, "Device Read Time: min = %f, max = %f\n",
		count_to_sec ( stat->min_dev_rd_count ),
		count_to_sec ( stat->max_dev_rd_count ) );

	Print ( stream, "Device Write Time: min = %f, max = %f",
		count_to_sec ( stat->min_dev_wr_count ),
		count_to_sec ( stat->max_dev_wr_count ) );

	Print ( stream, "\n" );

	for ( i = 0; i < 256; i++ ) {
		if ( stat->nr_interrupts [ i ] > 0 ) {
			Print ( stream, "Ivector %#x: num = %lld, time = %f",
				i, 
				stat->nr_interrupts [i],
				time_counter_to_sec ( &stat->interrupt_counters[i] ) );

			switch ( i ) {
			case IVECTOR_PAGEFAULT:
				Print ( stream, " (page fault)\n" );
				break;
			case IVECTOR_SYSCALL:
				Print ( stream, " (syscall)\n" );
				break;
			case 0x20:
				Print ( stream, " (pit via pic)\n" );
				break;
			case 0x31:
				Print ( stream, " (pit via apic)\n" );
				break;
			case 0x99:
				Print ( stream, " (io: irq=0xe)\n" );
				break;
			case 0xef:
				Print ( stream, " (local apic timer)\n" );
				break;
			default:
				Print ( stream, "\n" );
			}
		}
	}

	for ( i = 0; i < 0x1000; i++ ) {
		if ( stat->nr_emulated_instr [ i ] > 0 ) {
			Print ( stream, "# of exec = %d, opcode = %#x (%s) \n", 
				stat->nr_emulated_instr [ i ], 
				i, 
				stat->nr_emulated_instr_name [ i ] );
		}
	}
}

void
Stat_print ( FILE *stream, struct stat_t *x )
{
	ASSERT ( x != NULL );
	
	Print ( stream, "\n" );
	print_execution_time ( stream, x );
	Print ( stream, "\n" );
}

