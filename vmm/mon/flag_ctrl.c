#include "vmm/mon/mon.h"

/* Clear Interrupt Flag */
void
cli ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	if ( mon->regs->eflags.iopl >= cpl ( mon->regs ) ) {
		disable_interrupt ( mon->regs );
		skip_instr ( mon, instr );
		return;
	}

	if ( ( mon->regs->eflags.iopl < cpl ( mon->regs ) ) &&
	     ( cpl_is_supervisor_mode ( mon->regs ) ) &&
	     ( ! mon->regs->eflags.virtual_interrupt_pending ) ) {
		FlagReg_clear_virtual_interrupt_flag ( &mon->regs->eflags );
		skip_instr ( mon, instr );
		return;
	}

	assert ( 0 );
}

/* Set Interrupt Flag */
void
sti ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	if ( mon->regs->eflags.iopl >= cpl ( mon->regs ) ) {
		enable_interrupt ( mon->regs );
		skip_instr ( mon, instr );
		return;
	}

	if ( ( mon->regs->eflags.iopl < cpl ( mon->regs ) ) &&
	     ( cpl_is_supervisor_mode ( mon->regs ) ) &&
	     ( !mon->regs->eflags.virtual_interrupt_pending ) ) {
		FlagReg_set_virtual_interrupt_flag ( &mon->regs->eflags );
		skip_instr ( mon, instr );
		return;
	}
	
	assert ( 0 );
}

/* Clear Direction Flag: 
 * Clear the DF flag in the EFLAGS register 
 */
void
cld ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	FlagReg_clear_direction_flag ( &mon->regs->eflags );
	skip_instr ( mon, instr );
}

/* Set Direction Flag */
void
std ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	FlagReg_set_direction_flag ( &mon->regs->eflags );
	skip_instr ( mon, instr );
}


/* Clear Task-Switched Flag in CR0 */ 
void
clts ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	Cr0_clear_task_switched ( &mon->regs->sys.cr0 );

	skip_instr ( mon, instr );
}


/* Push EFLAGS Register onto the Stack */
void
pushf_fv ( struct mon_t *mon, struct instruction_t *instr ) 
{
	bit32u_t val;
	const bit32u_t MASK = 0x00fcffff; /* VM and RF FLAG bits are cleared in
					   * image stored on the stack */
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	val = mon->regs->eflags.val & MASK;
	Monitor_push ( mon, val, 4 );
	skip_instr ( mon, instr );
}


/* Pop Stack into EFLAGS Register */
void
popf_fv ( struct mon_t *mon, struct instruction_t *instr ) 
{
	bit32u_t val, val2;
	bit32u_t mask = 0x003d4dd5; /* ( == 0000 0000 0011 1101 0100 1101 1101 0101 ) 
				     * Flags of which bit is 0 are IF, IOPL, and VM */
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	
	val = Monitor_pop ( mon, 4 );

	/* [TODO] check the boundary of the stack */

	if ( cpl ( mon->regs ) == SUPERVISOR_MODE ) {
		/* IOPL can be modified only if CPL == 0 */
		mask |= ( ( 1 << 12 ) | ( 1 << 13 ) );
	} 

	if ( cpl ( mon->regs ) <= mon->regs->eflags.iopl ) {
		/* The interrupt flag ( IF ) is altered only when executing at
		  a level at least as privileged as the IOPL. */
		mask |= ( 1 << 9 );
	}
   
	/* All non-reserved flags except VIP, VIF, and VM can be modified.
	 * VIP and VIF are cleared; VM is unaffected. */
	val &= ~ (  ( 1 << 19 ) | ( 1 << 20 )  ); /* Clear VIP/VIF */

	val2 = ( ( mon->regs->eflags.val & ~mask ) | ( val & mask ) );

	DPRINT ( "popf_fw: val=%#x, mask=%#x ==> %#x\n", val, mask, val2 );

	mon->regs->eflags = FlagReg_of_bit32u ( val2 );

	skip_instr ( mon, instr );
}
