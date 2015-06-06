#include "vmm/mon/mon.h"

static void
load_cs ( struct mon_t *mon, const struct seg_selector_t *selector, privilege_level_t cpl )
{
	struct seg_selector_t x;

	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
   
	x = *selector;
	x.rpl = cpl;
	Monitor_set_seg_reg ( mon->regs, SEG_REG_CS, &x );
}

static void
load_ss ( struct mon_t *mon, const struct seg_selector_t *selector, privilege_level_t cpl )
{
	struct seg_selector_t x;

	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
   
	x = *selector;
	x.rpl = cpl;
	Monitor_set_seg_reg ( mon->regs, SEG_REG_CS, &x );
}

/****************************************************************/

void
call_ed ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t op1_32;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	/* jump する直前に eip を increment する */
	skip_instr ( mon, instr ); 

	op1_32 = Monitor_read_reg_or_mem ( mon, instr, 4 );
	Monitor_push ( mon, mon->regs->user.eip, 4 );
	mon->regs->user.eip = op1_32;
}

struct mem_access_t *
call_ed_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	struct mem_access_t *maccess;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	if ( instr->mod == 3 ) {
		maccess = MemAccess_push ( mon, 4 );
	} else {
		maccess = MemAccess_create_read_resolve ( mon, instr, 4 );
		maccess->next = MemAccess_push ( mon, 4 );
		return maccess;
	}

	return maccess;
}

/****************************************************************/

void
jmp_ed ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t op1_32;
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	op1_32 = ( ( instr->mod == 3 )
		 ? UserRegs_get ( &mon->regs->user, instr->rm ) 
		 : Monitor_read_with_resolve ( mon, instr, 4 ) );
	mon->regs->user.eip = op1_32;
}

struct mem_access_t *
jmp_ed_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return ( instr->mod == 3 ) ? NULL : MemAccess_create_read_resolve ( mon, instr, 4 );
}

/****************************************************************/

static void
jump_protected_cd_seg_conforming ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector, 
				 struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
	assert ( descr->seg.dpl <= cpl ( mon->regs ) );
	assert ( descr->seg.present );
	/* [TODO] check limit */

	DPRINT ( "jump_protected_cd_seg_conforming\n" );

	load_cs ( mon, selector, cpl ( mon->regs ) );
	mon->regs->user.eip = addr;
}

static void
jump_protected_cd_seg_non_conforming ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector,
				   struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	assert ( selector->rpl <= cpl ( mon->regs ) );
	assert ( descr->seg.dpl == cpl ( mon->regs ) );
	assert ( descr->seg.present );
	/* [TODO] check limit */

	DPRINT ( "jump_protected_cd_seg_non_conforming: addr=%#x\n", addr );

	load_cs ( mon, selector, cpl ( mon->regs ) );
	mon->regs->user.eip = addr;
}

static void
jump_protected_cd_seg ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector, 
		   struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	assert ( descr->executable );

	if ( descr->c_ed ) {
		jump_protected_cd_seg_conforming ( mon, addr, selector, descr );
	} else {
		jump_protected_cd_seg_non_conforming ( mon, addr, selector, descr );
	}
}

static void
jump_protected_available_tss ( struct mon_t *mon, struct seg_selector_t *selector, struct seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	DPRINT ( "jump_protected_available_tss\n" );

#if 0
	// SWITCH_TASKS _without_ nesting to TSS
	task_switch ( &selector, &descriptor,
		  BX_TASK_FROM_JUMP, 
		  dword1, 
		  dword2 );
#else
	Fatal_failure ( "jump_protected_sys_available_tss: not implemented." );
#endif
   
	/* [TODO] check eip is within the sgment limit */
}

/* [NOTE] 
 * This function is never called because Linux does not use the
 * task gate descriptor. */
static void
jump_protected_task_gate ( struct mon_t *mon, struct seg_selector_t *selector, struct gate_descr_t *descr )
{
	struct seg_selector_t tss_selector;
	struct descr_t descr2;
	struct seg_descr_t tssd;
   
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	assert ( ( SUB_BIT ( descr->selector, 2, 14 ) ) != 0 ); /* [???] from bochs */

	DPRINT ( "jump_protected_task_gate\n" );

	tss_selector = SegSelector_of_bit16u ( descr->selector );
	descr2 = Monitor_lookup_descr_table ( mon->regs, &tss_selector );
	tssd = Descr_to_available_task_state_seg_descr ( &descr2 );
	assert ( tssd.present );

#if 0
	// SWITCH_TASKS _without_ nesting to TSS
	task_switch ( &tss_selector, &tss_descriptor,
		  BX_TASK_FROM_JUMP, dword1, dword2 ); // この dwordは descr2 を fetchしてきたときの値
#else
	Fatal_failure ( "jump_protected_sys_task_gate: not implemented" );
#endif

	/* [TODO] check eip is within the segment limit */
}

static void
jump_protected_call_gate ( struct mon_t *mon, struct seg_selector_t *selector, struct gate_descr_t *descr )
{
	struct seg_selector_t gate_cs_selector;
	struct descr_t gate_cs_descr;
	struct cd_seg_descr_t cd_seg;
   
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	assert ( ( SUB_BIT ( descr->selector, 2, 14 ) ) != 0 ); /* [???] from bochs */

	DPRINT ( "jump_protected_call_gate\n" );

	gate_cs_selector = SegSelector_of_bit16u ( descr->selector );
	gate_cs_descr = Monitor_lookup_descr_table ( mon->regs, &gate_cs_selector );
	cd_seg = Descr_to_cd_seg_descr ( &gate_cs_descr );
	assert ( cd_seg.executable );

	assert ( ( ( cd_seg.c_ed ) && ( cd_seg.seg.dpl <= cpl ( mon->regs ) ) ) ||
	    ( ( cd_seg.c_ed == FALSE ) && ( cd_seg.seg.dpl == cpl ( mon->regs ) ) ) );
	assert ( cd_seg.seg.present );

	/* [TODO] offset must be smaller than limit. */

	load_cs ( mon, &gate_cs_selector, cpl ( mon->regs ) ); 
	mon->regs->user.eip = descr->offset;
}

static void
jump_protected_sys ( struct mon_t *mon, struct seg_selector_t *selector, struct sys_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	switch ( descr->type ) {
	case AVAILABLE_TASK_STATE_SEG: 	jump_protected_available_tss ( mon, selector, &descr->u.seg ); break;
	case TASK_GATE:       	jump_protected_task_gate ( mon, selector, &descr->u.gate ); break;
	case CALL_GATE:  	  	jump_protected_call_gate ( mon, selector, &descr->u.gate ); break;
	default: 		 	Match_failure ( "jump_protected_sys: descr->type=%#x\n", descr->type );
	}
}

static void
jump_protected ( struct mon_t *mon, struct instruction_t *instr, bit16u_t cs_raw, bit32u_t addr )
{
	struct seg_selector_t selector;
	struct descr_t descr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	/* [NOTE]
	 * Raise #GP ( 0 ) if an effective address in the CS, DS, ES, FS, GS
	 * or SS segment is illegal or segment selector in target operand
	 * null. */
	assert ( ( SUB_BIT ( cs_raw, 2, 14 ) ) != 0 ); /* [???] from Bochs */

	selector = SegSelector_of_bit16u ( cs_raw );

	/* [TODO] 
	 * Raise #GP ( new selector ) if segment selector index is not
	 * within descriptor table limit. */

	descr = Monitor_lookup_descr_table ( mon->regs, &selector );
   
	switch ( descr.type ) {
	case CD_SEG_DESCR: 	jump_protected_cd_seg ( mon, addr, &selector, &descr.u.cd_seg ); break;
	case SYSTEM_DESCR: 	jump_protected_sys ( mon, &selector, &descr.u.sys ); break;
	default:		Match_failure ( "jump_protected\n" );
	}
}

/* Jump */
void
jmp_ap ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t disp32;
	bit16u_t cs_raw;
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	disp32 = instr->immediate[0];
	cs_raw = instr->immediate[1];
	DPRINT ( "jmp_ap: disp32=%#x, cs_raw=%#x\n", disp32, cs_raw );
	jump_protected ( mon, instr, cs_raw, disp32 );
}

struct mem_access_t *jmp_ap_mem ( struct mon_t *mon, struct instruction_t *instr ) { return NULL; } /* TODO */

/****************************************************************/

static void
call_protected_cd_seg_conforming ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector, struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	assert ( descr->seg.dpl <= cpl ( mon->regs ) );
	assert ( descr->seg.present );
	/* [TODO] check limit */

	DPRINT ( "call_protected_cd_seg_conforming\n" );

	Monitor_push ( mon, SegReg_to_bit16u ( &mon->regs->segs[SEG_REG_CS] ), 4 );
	Monitor_push ( mon, mon->regs->user.eip, 4 );
	load_cs ( mon, selector, cpl ( mon->regs ) );
	mon->regs->user.eip = addr;
}

static void
call_protected_cd_seg_non_conforming ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector, struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	assert ( selector->rpl <= cpl ( mon->regs ) );
	assert ( descr->seg.dpl == cpl ( mon->regs ) );
	assert ( descr->seg.present );
	/* [TODO] check limit */

	DPRINT ( "call_protected_cd_seg_non_conforming: addr=%#x\n", addr );

	Monitor_push ( mon, SegReg_to_bit16u ( &mon->regs->segs[SEG_REG_CS] ), 4 );
	Monitor_push ( mon, mon->regs->user.eip, 4 );
	load_cs ( mon, selector, cpl ( mon->regs ) );
	mon->regs->user.eip = addr;
}

static void
call_protected_cd_seg ( struct mon_t *mon, bit32u_t addr, struct seg_selector_t *selector, struct cd_seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	DPRINT ( "call_protected_cd_seg\n" );

	assert ( descr->executable );
	if ( descr->c_ed ) {
		call_protected_cd_seg_conforming ( mon, addr, selector, descr );
	} else {
		call_protected_cd_seg_non_conforming ( mon, addr, selector, descr );
	}
}

static void
call_protected_available_tss ( struct mon_t *mon, struct seg_selector_t *selector, struct seg_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	DPRINT ( "call_protected_available_tss\n" );

	assert ( 0 );
/*
 task_switch ( &selector, &descriptor,
 BX_TASK_FROM_JUMP, 
 dword1, 
 dword2 );
*/

}

void
call_protected_task_gate ( struct mon_t *mon, struct seg_selector_t *selector, struct gate_descr_t *descr )
{
	struct seg_selector_t tss_selector;
	struct descr_t descr2;
	struct seg_descr_t tssd;
   
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );
   
	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	assert ( ( SUB_BIT ( descr->selector, 2, 14 ) ) != 0 ); /* [???] from bochs */

	DPRINT ( "call_protected_task_gate\n" );

	tss_selector = SegSelector_of_bit16u ( descr->selector );
	descr2 = Monitor_lookup_descr_table ( mon->regs, &tss_selector );
	tssd = Descr_to_available_task_state_seg_descr ( &descr2 );
	assert ( tssd.present );

	assert ( 0 );
/*
 task_switch ( &tss_selector, &tss_descriptor,
 BX_TASK_FROM_JUMP, dword1, dword2 ); // この dwordは descr2 を fetchしてきたときの値
*/
}

static void
call_protected_call_gate_more_privilege ( struct mon_t *mon, struct gate_descr_t *gate_descr, 
					struct seg_selector_t *cs_selector, struct descr_t *cs_descr, 
					bit32u_t new_eip )
{
	enum { MAX_PARAM = 31 };
	struct seg_selector_t ss_selector;
	struct descr_t ss_descr;
	struct cd_seg_descr_t ss_cd_seg, cs_cd_seg;
	struct tss_t tss;
	bit16u_t ss_raw, old_ss, old_cs;
	bit32u_t new_esp, old_esp, old_eip;
	int i;
	bit32u_t params[MAX_PARAM];

	ASSERT ( mon != NULL );
	ASSERT ( gate_descr != NULL );
	ASSERT ( cs_selector != NULL );
	ASSERT ( cs_descr != NULL );

	DPRINT ( "call_protected_call_gate_more_privilege\n" );

	cs_cd_seg = Descr_to_cd_seg_descr ( cs_descr );

	tss = Monitor_get_tss_of_current_task ( mon->regs );
	ss_raw = tss.ss[cs_cd_seg.seg. dpl];
	new_esp = tss.esp[cs_cd_seg.seg. dpl];

	assert ( ss_raw & 0xfffc );
	ss_selector = SegSelector_of_bit16u ( ss_raw );
	ss_descr = Monitor_lookup_descr_table ( mon->regs, &ss_selector );
	ss_cd_seg = Descr_to_cd_seg_descr ( &ss_descr );

	assert ( ss_selector.rpl == cs_cd_seg.seg.dpl );
	assert ( ss_cd_seg.seg.dpl == cs_cd_seg.seg.dpl );

	assert ( ss_cd_seg.executable == FALSE );
	assert ( ss_cd_seg.read_write );
	assert ( ss_cd_seg.seg.present );
	assert ( gate_descr->param_count <= MAX_PARAM );

	/* [TODO] check the boundary of the stack */

	for ( i = 0; i < gate_descr->param_count; i++ ) {
		params[i] = Monitor_read_dword_with_vaddr ( SEG_REG_SS, mon->regs->user.esp + i*4 );
	}

	old_ss = SegReg_to_bit16u ( &mon->regs->segs[SEG_REG_SS] );
	old_esp = mon->regs->user.esp;
	//old_ss_base = Descr_base ( &mon->regs->segs[SEG_REG_SS].cache );

	old_cs = SegReg_to_bit16u ( &mon->regs->segs[SEG_REG_CS] );
	old_eip = mon->regs->user.eip;

	load_ss ( mon, &ss_selector, ss_cd_seg.seg.dpl );
	mon->regs->user.esp = new_esp;

	load_cs ( mon, cs_selector, cs_cd_seg.seg.dpl );
	mon->regs->user.eip = new_eip;

	Monitor_push ( mon, old_ss, 4 );
	Monitor_push ( mon, old_esp, 4 );

	for ( i = gate_descr->param_count - 1; i >= 0; i-- ) {
		Monitor_push ( mon, params[i], 4 );
	}

	Monitor_push ( mon, old_cs, 4 );
	Monitor_push ( mon, old_eip, 4 );
}

static void
call_protected_call_gate_same_privilege ( struct mon_t *mon, struct seg_selector_t *gate_cs_selector, 
					bit32u_t new_eip )
{
	ASSERT ( mon != NULL );
	ASSERT ( gate_cs_selector != NULL );
   
	DPRINT ( "call_protected_call_gate_same_privilege\n" );
	/* [TODO] check the boundary of the stack */
	/* [TODO] check the segment limit */
   
	Monitor_push ( mon, SegReg_to_bit16u ( &mon->regs->segs[SEG_REG_CS] ), 4 );
	Monitor_push ( mon, mon->regs->user.eip, 4 );
	load_cs ( mon, gate_cs_selector, cpl ( mon->regs ) ); 
	mon->regs->user.eip = new_eip;
}

static void
call_protected_call_gate ( struct mon_t *mon, struct seg_selector_t *selector, struct gate_descr_t *descr )
{
	struct seg_selector_t gate_cs_selector;
	struct descr_t gate_cs_descr;
	struct cd_seg_descr_t cd_seg;
	bit32u_t new_eip;
   
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	DPRINT ( "call_protected_call_gate\n" );

	assert ( descr->dpl >= cpl ( mon->regs ) );
	assert ( descr->dpl >= selector->rpl );
	assert ( descr->present );

	assert ( descr->selector & 0xfffc );
	gate_cs_selector = SegSelector_of_bit16u ( descr->selector );
	new_eip = descr->offset;

	gate_cs_descr = Monitor_lookup_descr_table ( mon->regs, &gate_cs_selector );

	cd_seg = Descr_to_cd_seg_descr ( &gate_cs_descr );
	assert ( cd_seg.executable );
	assert ( cd_seg.seg.dpl <= cpl ( mon->regs ) );

	if ( ( cd_seg.c_ed == FALSE ) && ( cd_seg.seg.dpl < cpl ( mon->regs ) ) ) {
		call_protected_call_gate_more_privilege ( mon, descr, &gate_cs_selector, &gate_cs_descr, 
							new_eip );
	} else {
		call_protected_call_gate_same_privilege ( mon, &gate_cs_selector, new_eip );
	}
}

static void
call_protected_sys ( struct mon_t *mon, struct seg_selector_t *selector, struct sys_descr_t *descr )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	DPRINT ( "call_protected_sys\n" );
	switch ( descr->type ) {
	case AVAILABLE_TASK_STATE_SEG: 	call_protected_available_tss ( mon, selector, &descr->u.seg ); break;
	case TASK_GATE:			call_protected_task_gate ( mon, selector, &descr->u.gate ); break;
	case CALL_GATE:			call_protected_call_gate ( mon, selector, &descr->u.gate ); break;
	default: 			Match_failure ( "call_protected_sys" );
	}  
}

static void
call_protected ( struct mon_t *mon, bit16u_t cs_raw, bit32u_t addr )
{
	struct seg_selector_t selector;
	struct descr_t descr;
   
	DPRINT ( "call_protected\n" );
	assert ( cs_raw & 0xfffc );

	selector = SegSelector_of_bit16u ( cs_raw );
	descr = Monitor_lookup_descr_table ( mon->regs, &selector );

	assert ( descr.is_valid );
	switch ( descr.type ) {
	case CD_SEG_DESCR: call_protected_cd_seg ( mon, addr, &selector, &descr.u.cd_seg ); break;
	case SYSTEM_DESCR: call_protected_sys ( mon, &selector, &descr.u.sys ); break;
	default:	   Match_failure ( "call_protected\n" );
	}
}

void
call32_ep ( struct mon_t *mon, struct instruction_t *instr )
{
	bit16u_t cs_raw;
	bit32u_t op1_32;
	bit32u_t vaddr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	/* jump する直前に eip を increment する */
	skip_instr ( mon, instr ); 

	vaddr = instr->resolve ( instr, &mon->regs->user ); 

	op1_32 = Monitor_read_dword_with_vaddr ( instr->sreg_index, vaddr );
	cs_raw = Monitor_read_word_with_vaddr ( instr->sreg_index, vaddr + 4 );

	call_protected ( mon, cs_raw, op1_32 );
}

struct mem_access_t *
call32_ep_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	struct mem_access_t *maccess;
	bit32u_t vaddr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	vaddr = instr->resolve ( instr, &mon->regs->user );
	maccess = MemAccess_create_read ( instr->sreg_index, vaddr, 4 );
	maccess->next = MemAccess_create_read ( instr->sreg_index, vaddr + 4, 2 );
	return maccess;
}

void
call_ad ( struct mon_t *mon, struct instruction_t *instr )
{
	bit32u_t new_eip;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

#if 1
	/* jump する直前に eip を increment する */
	skip_instr ( mon, instr ); 
#endif

	new_eip = mon->regs->user.eip + instr->immediate[0];
	Monitor_push ( mon, mon->regs->user.eip, 4 );
	mon->regs->user.eip = new_eip;
}

struct mem_access_t *
call_ad_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return MemAccess_push ( mon, 4 );
}

/****************************************************************/

void
jmp32_ep ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( 0 );
}

/* TODO */
struct mem_access_t *jmp32_ep_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return NULL;
}

/****************************************************************/

static void
return_to_same_privilege_level ( struct mon_t *mon, bit16u_t pop_bytes, struct seg_selector_t *cs_selector )
{
	ASSERT ( mon != NULL );
	ASSERT ( cs_selector != NULL );
   
	DPRINT ( "return_to_same_privilege_level:\n" );

	mon->regs->user.eip = Monitor_pop ( mon, 4 );
	 ( void )Monitor_pop ( mon, 4 );
	mon->regs->user.esp += pop_bytes;

	/* [TODO] check the boundary of the stack */

	load_cs ( mon, cs_selector, cpl ( mon->regs ) );
}
 
static void
return_to_outer_privilege_level ( struct mon_t *mon, bit16u_t pop_bytes, struct seg_selector_t *cs_selector )
{
	bit16u_t ss_raw;
	struct seg_selector_t ss_selector;
	struct descr_t ss_descr;
	struct cd_seg_descr_t ss_cd_seg;
	bit32u_t temp_esp;

	ASSERT ( mon != NULL );
	ASSERT ( cs_selector != NULL );

	DPRINT ( "return_to_outer_privilege_level:\n" );

	mon->regs->user.eip = Monitor_pop ( mon, 4 );
	 ( void )Monitor_pop ( mon, 4 );
	mon->regs->user.esp += pop_bytes;

	temp_esp = Monitor_pop ( mon, 4 );
	ss_raw = Monitor_pop ( mon, 4 );
	assert ( ( ss_raw & 0xfffc ) );

	ss_selector = SegSelector_of_bit16u ( ss_raw );
	assert ( ss_selector.rpl == cs_selector->rpl );

	ss_descr = Monitor_lookup_descr_table ( mon->regs, &ss_selector );
	ss_cd_seg = Descr_to_cd_seg_descr ( &ss_descr );

	assert ( ss_cd_seg.executable == FALSE );
	assert ( ss_cd_seg.read_write );
	assert ( ss_cd_seg.seg.dpl == cs_selector->rpl );
	assert ( ss_cd_seg.seg.present );

	mon->regs->user.esp = temp_esp;
   
	load_ss ( mon, &ss_selector, cs_selector->rpl );
}
  
static void
return_protected ( struct mon_t *mon, bit16u_t pop_bytes )
{
	ASSERT ( mon != NULL );
	bit16u_t cs_raw;
	struct seg_selector_t selector;
	struct descr_t descr;
	struct cd_seg_descr_t cd_seg;

	DPRINT ( "return_protected:\n" );

	cs_raw = Monitor_read_word_with_vaddr ( SEG_REG_SS, mon->regs->user.esp + 4 );
	assert ( cs_raw & 0xfffc );
	selector = SegSelector_of_bit16u ( cs_raw );
	assert ( selector.rpl >= cpl ( mon->regs ) );

	descr = Monitor_lookup_descr_table ( mon->regs, &selector );
	cd_seg = Descr_to_cd_seg_descr ( &descr );

	assert ( cd_seg.executable );
	assert ( ( ( cd_seg.c_ed ) && ( cd_seg.seg.dpl <= selector.rpl ) ) ||
	    ( ( cd_seg.c_ed == FALSE ) && ( cd_seg.seg.dpl == selector.rpl ) ) );
	assert ( cd_seg.seg.present );

	if ( selector.rpl == cpl ( mon->regs ) ) {
		return_to_same_privilege_level ( mon, pop_bytes, &selector );
	} else {
		return_to_outer_privilege_level ( mon, pop_bytes, &selector );
	}
}

void
ret_far16 ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return_protected ( mon, 0 );
}

void
ret_far32 ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return_protected ( mon, 0 );
}

struct mem_access_t *ret_far16_mem ( struct mon_t *mon, struct instruction_t *instr ) { return NULL; }
struct mem_access_t *ret_far32_mem ( struct mon_t *mon, struct instruction_t *instr ) { return NULL; }

struct mem_access_t *
leave_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	const size_t LEN = 4;
	return MemAccess_create_read ( SEG_REG_SS, mon->regs->user.ebp, LEN );
}


/****************************************************************/

static void
iret_task_return ( struct mon_t *mon )
{
	struct tss_t tss;
	struct seg_selector_t selector;
	struct descr_t descr;
	struct seg_descr_t tssd;

	DPRINT ( "iret_task_return: nested_task\n" );
	DPRINT2 ( "iret_task_return: nested_task\n" );

	ASSERT ( mon != NULL );
	assert ( mon->regs->eflags.virtual_8086_mode == FALSE );

	tss = Monitor_get_tss_of_current_task ( mon->regs );
	selector = SegSelector_of_bit16u ( tss.previous_task_link );

	descr = Monitor_lookup_descr_table ( mon->regs, &selector );
	tssd = Descr_to_busy_task_state_seg_descr ( &descr );
	assert ( tssd.present );
   
	assert ( 0 );
/* [DEBUG]
  task_switch ( &selector, &tssd,
  BX_TASK_FROM_IRET, dword1, dword2 );
*/
}

static void
iret_set_eflags ( struct mon_t *mon, bit32u_t eflags )
{
	bit32u_t mask = 0x254dd5; // 0010 0101 0100 1101 1101 0101
	bit32u_t val;

	ASSERT ( mon != NULL );

	if ( cpl ( mon->regs ) <= mon->regs->eflags.iopl )
		mask |= ( 1 << 9 ); // IF

	if ( cpl ( mon->regs ) == 0 ) {
		mask |= ( ( 1 << 12 ) | ( 1 << 13 ) ); // IOPL
		mask |= ( ( 1 << 20 ) | ( 1 << 19 ) | ( 1 << 17 ) ); // VM, VIF, VIP
	}

	val = ( ( mon->regs->eflags.val & ~mask ) | ( eflags & mask ) );
	mon->regs->eflags = FlagReg_of_bit32u ( val );   
}


static void
iret_return_to_same_privilege_level ( struct mon_t *mon, 
				      struct seg_selector_t *selector, 
				      bit32u_t eip, 
				      bit32u_t eflags )
{
	ASSERT ( mon != NULL );
	ASSERT ( selector != NULL );

	DPRINT ( "iret_return_to_same_privilege_level: eip = %#x\n", eip );

	iret_set_eflags ( mon, eflags );
	load_cs ( mon, selector, cpl ( mon->regs ) );
	mon->regs->user.eip = eip;
}

static void
iret_return_to_outer_privilege_level ( struct mon_t *mon, 
				       struct seg_selector_t *cs_selector, 
				       bit32u_t eip, 
				       bit32u_t eflags )
{
	bit16u_t ss_raw;
	bit32u_t esp;
	struct seg_selector_t ss_selector;
	struct descr_t descr;
	struct cd_seg_descr_t cd_seg;

	ASSERT ( mon != NULL );
	ASSERT ( cs_selector != NULL );

	DPRINT ( "iret_return_to_outer_privilege_level: eip = %#x\n", eip );
   
	esp = Monitor_pop ( mon, 4 );
	ss_raw = Monitor_pop ( mon, 4 );
   
	assert ( ss_raw & 0xfffc );
   
	ss_selector = SegSelector_of_bit16u ( ss_raw );
	assert ( ss_selector.rpl == cs_selector->rpl );

	descr = Monitor_lookup_descr_table ( mon->regs, &ss_selector );
	cd_seg = Descr_to_cd_seg_descr ( &descr );

	assert ( ! cd_seg.executable );
	assert ( cd_seg.read_write );
	assert ( cd_seg.seg.dpl == cs_selector->rpl );
	assert ( cd_seg.seg.present );

	iret_set_eflags ( mon, eflags );
	load_cs ( mon, cs_selector, cs_selector->rpl );
	load_ss ( mon, &ss_selector, ss_selector.rpl );
	mon->regs->user.eip = eip;
	mon->regs->user.esp = esp;
}

static void
__iret_protected ( struct mon_t *mon )
{
	bit32u_t eip, eflags;
	bit16u_t cs_raw;
	struct seg_selector_t selector;
	struct descr_t descr;
	struct cd_seg_descr_t cd_seg;

	ASSERT ( mon != NULL );
	/* [TODO] check the boundary of the stack */
   
	DPRINT ( "iret_protected:\n" );

	eip    = Monitor_pop ( mon, 4 );
	cs_raw = Monitor_pop ( mon, 4 );
	eflags = Monitor_pop ( mon, 4 );

	assert ( eip <= VM_PMEM_BASE );

	if ( TEST_BIT ( eflags, 17 ) ) { /* check VM ( Virtual-8086 Mode ) flag */
		/* [TODO] return to v86 */
		assert ( 0 );
		return;
	}
	assert ( cs_raw & 0xfffc );
   
	selector = SegSelector_of_bit16u ( cs_raw );
	descr    = Monitor_lookup_descr_table ( mon->regs, &selector );
	cd_seg   = Descr_to_cd_seg_descr ( &descr );

	assert ( selector.rpl >= cpl ( mon->regs ) );
	assert ( ( ( cd_seg.c_ed ) && ( cd_seg.seg.dpl <= selector.rpl ) ) ||
		 ( ( cd_seg.c_ed == FALSE ) && ( cd_seg.seg.dpl == selector.rpl ) ) );
	assert ( cd_seg.seg.present );

	if ( selector.rpl == cpl ( mon->regs ) ) {
		iret_return_to_same_privilege_level ( mon, &selector, eip, eflags );
	} else {
		iret_return_to_outer_privilege_level ( mon, &selector, eip, eflags );
	}
}

static void
iret_protected ( struct mon_t *mon )
{
	struct process_entry_t *current;

	current = get_guest_current_process ( &mon->guest_state, mon );

	if ( mon->regs->eflags.nested_task ) {
		iret_task_return ( mon );
	} else {
		__iret_protected ( mon );
	}

	if ( is_iret_from_syscall ( &mon->guest_state, mon, current ) )
		emulate_syscall ( &mon->guest_state, mon, current );
}

/* Interrput Return */
void
iret16 ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	iret_protected ( mon );
}

/* Interrput Return */
void
iret32 ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	iret_protected ( mon );
}

struct mem_access_t *
iret16_mem ( struct mon_t *mon, struct instruction_t *instr )
{ 
	return NULL; 
}

struct mem_access_t *
iret32_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{
	return MemAccess_pop ( mon, 12 );
}


/****************************************************************/


