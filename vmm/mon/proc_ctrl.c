#include "vmm/mon/mon.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


static bool_t
need_wakeup ( struct mon_t *mon )
{
	return ( ( check_external_irq ( mon ) ) ||
		 ( Pic_check_interrupt ( &mon->devs.pic ) ) ||
		 ( LocalApic_check_interrupt ( mon->local_apic ) ) ||
		 ( ! mon->wait_ipi ) );
}

/* Halt */
void
hlt ( struct mon_t *mon, struct instruction_t *instr )
{ 
	Pit_restart_timer ( &mon->devs.pit );

#ifdef ENABLE_MP

//	Print_color ( stdout, RED, "[%d] hlt: begin\n", mon->cpuid );

	mon->stat.halt_counter_flag = TRUE;
	start_time_counter ( &mon->stat.halt_counter );

	mon->wait_ipi = TRUE;

	while ( ! need_wakeup ( mon ) ) {
#if 0
		/* kernel 2.6 だと止まってしまう */
		/* [???] Should sleep_time be same as timer interrupt interval? */
		enum { SLEEP_TIME = 10000 }; /*  10,000 nano seconds = 10 milli seconds */
		wait_recvable_msg ( mon, SLEEP_TIME );
		try_handle_msgs ( mon );
#else
		/* [DEBUG] */
		usleep ( 1 );
		try_handle_msgs ( mon );
#endif
	}

	stop_time_counter ( &mon->stat.halt_counter );
	mon->stat.halt_counter_flag = FALSE;

//	Print_color ( stdout, RED, "[%d] hlt: end\n", mon->cpuid );

#endif /* ENABLE_MP */

	Pit_stop_timer ( &mon->devs.pit );
	skip_instr ( mon, instr ); 

	mon->stat.nr_hlts ++;
}
  
/*
 enum {
 BX_MSR_P5_MC_ADDR 	= 0x0000,
 BX_MSR_MC_TYPE 	= 0x0001,
 BX_MSR_TSC 		= 0x0010,
 BX_MSR_CESR 		= 0x0011,
 BX_MSR_CTR0 		= 0x0012,
 BX_MSR_CTR1 		= 0x0013,
 BX_MSR_APICBASE 	= 0x001b,
 BX_MSR_EBL_CR_POWERON = 0x002a,
 BX_MSR_TEST_CTL 	= 0x0033,
 BX_MSR_BIOS_UPDT_TRIG = 0x0079,
 BX_MSR_BBL_CR_D0 	= 0x0088,
 BX_MSR_BBL_CR_D1 	= 0x0089,
 BX_MSR_BBL_CR_D2 	= 0x008a,
 BX_MSR_BBL_CR_D3 	= 0x008b,
 BX_MSR_PERFCTR0	= 0x00c1,
 BX_MSR_PERFCTR1	= 0x00c2,
 BX_MSR_MTRRCAP	= 0x00fe,
 BX_MSR_BBL_CR_ADDR	= 0x0116,
 BX_MSR_BBL_DECC	= 0x0118,
 BX_MSR_BBL_CR_CTL	= 0x0119,
 BX_MSR_BBL_CR_TRIG	= 0x011a,
 BX_MSR_BBL_CR_BUSY	= 0x011b,
 BX_MSR_BBL_CR_CTL3	= 0x011e,
 BX_MSR_MCG_CAP	= 0x0179,
 BX_MSR_MCG_STATUS	= 0x017a,
 BX_MSR_MCG_CTL	= 0x017b,
 BX_MSR_EVNTSEL0	= 0x0186,
 BX_MSR_EVNTSEL1	= 0x0187,
 BX_MSR_DEBUGCTLMSR	= 0x01d9,
 BX_MSR_LASTBRANCHFROMIP = 0x01db,
 BX_MSR_LASTBRANCHTOIP	= 0x01dc,
 BX_MSR_LASTINTOIP	= 0x01dd,
 BX_MSR_ROB_CR_BKUPTMPDR6 = 0x01e0,
 BX_MSR_MTRRPHYSBASE0	= 0x0200,
 BX_MSR_MTRRPHYSMASK0	= 0x0201,
 BX_MSR_MTRRPHYSBASE1	= 0x0202
 };
*/

/* Write to Model Specific Register */
void
wrmsr ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	assert ( cpl ( mon->regs ) == SUPERVISOR_MODE );

	skip_instr ( mon, instr ); 
}

/* Read from Model Specific Register */
void
rdmsr ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	assert ( cpl ( mon->regs ) == SUPERVISOR_MODE );

#if 0
	/* We have the requested MSR register in ECX */
	switch ( mon->regs->user.ecx ) {
	case MSR_P5_MC_ADDR:
	case MSR_MC_TYPE: break;
	default: Match_failure ( "rdmsr\n" );

	}
#endif

	skip_instr ( mon, instr ); 
}

/* CPU Identification */
void
cpuid ( struct mon_t *mon, struct instruction_t *instr )
{
	int op = mon->regs->user.eax;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
  
	__asm__ ( "cpuid"
		:
		"=a" ( mon->regs->user.eax ),
		"=b" ( mon->regs->user.ebx ),
		"=c" ( mon->regs->user.ecx ),
		"=d" ( mon->regs->user.edx )
		: "0" ( op ) );

	if ( op == 0x00000001 ) {
		/* Set the APIC On-Chip flag.
		 * [Referenec] IA-32 manual. Vol.2A 3-120, 3-141 */
		SET_BIT ( mon->regs->user.edx, 9 );
	}
  
	skip_instr ( mon, instr ); 
}

/* Store Machine Status Word */
void
smsw_ew ( struct mon_t *mon, struct instruction_t *instr )
{
	assert ( 0 );
}

/* Load Machine Status Word */
void
lmsw_ew ( struct mon_t *mon, struct instruction_t *instr )
{
	assert ( 0 );
}

/* Move from Control Registers */
void
mov_rd_cd ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t val_32 = 0;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( instr->mod == 3 );
	assert ( cpl_is_supervisor_mode ( mon->regs ) );

	switch ( instr->reg ) {
	case 0: val_32 = mon->regs->sys.cr0.val; break;
	case 1: ASSERT ( 0 ); break;
	case 2: val_32 = mon->regs->sys.cr2; break;
	case 3: val_32 = mon->regs->sys.cr3.val; break;
	case 4: val_32 = mon->regs->sys.cr4.val; break;
	default: Match_failure ( "mov_rd_cd\n" );
	}

	UserRegs_set ( &mon->regs->user, instr->rm, val_32 );

	skip_instr ( mon, instr );
}

/* Move to Control Registers */
void
mov_cd_rd ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t val_32;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( instr->mod == 3 );
	assert ( cpl ( mon->regs ) == SUPERVISOR_MODE );

	val_32 = UserRegs_get ( &mon->regs->user, instr->rm );
  
	switch ( instr->reg ) {
	case 0: 
		check_pgtable_permission ( mon, mon->regs->sys.cr3.val );
		run_emulation_code_of_vm ( mon, SET_CNTL_REG, instr->reg, val_32 );
		break;

	case 3: 
		check_pgtable_permission ( mon, val_32 );
		run_emulation_code_of_vm ( mon, SET_CNTL_REG, instr->reg, val_32 );
		break;
	case 1:
		ASSERT ( 0 ); 
		break;
	case 2:
		mon->regs->sys.cr2 = val_32; 
		break;
	case 4:
		mon->regs->sys.cr4 = Cr4_of_bit32u ( val_32 ); 
		break;
	default: 
		Match_failure ( "mon_cd_rd\n" );
	}
	skip_instr ( mon, instr );
}

void
mov_dd_rd ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t val_32;
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( instr->mod == 3 );
	assert ( cpl ( mon->regs ) == SUPERVISOR_MODE );

	val_32 = UserRegs_get ( &mon->regs->user, instr->rm );
  
	switch ( instr->reg ) {
	case 0: 
	case 1:
	case 2:
	case 3:
	case 6:
	case 7:
		mon->regs->debugs[instr->reg] = val_32;
		break;
	default:
		Match_failure ( "mon_dd_rd\n" );
	}
  
	skip_instr ( mon, instr );
}

