#include "vmm/ia32/regs_common.h"
#include "vmm/ia32/paging.h"

/* Prototype Declaration */
struct descr_t lookup_gdt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr );
struct descr_t lookup_ldt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr );
void update_gdt ( const struct regs_t *regs, int index, const struct descr_t *descr, trans_t *laddr_to_raddr );
void update_ldt ( const struct regs_t *regs, int index, const struct descr_t *descr, trans_t *laddr_to_raddr );


/****************************************************************/

const char *
GenRegIndex_to_string ( gen_reg_index_t index )
{
	switch ( index ) {
	case GEN_REG_EAX: return "eax";
	case GEN_REG_ECX: return "ecx";
	case GEN_REG_EDX: return "edx";
	case GEN_REG_EBX: return "ebx";
	case GEN_REG_ESP: return "esp";
	case GEN_REG_EBP: return "ebp";
	case GEN_REG_ESI: return "esi";
	case GEN_REG_EDI: return "edi";
	default: Match_failure ( "GenRegIndex_to_string\n" );
	}

	Match_failure ( "GenRegIndex_to_string" );
	return NULL;
}

/****************************************************************/

const char *
SegRegIndex_to_string ( seg_reg_index_t index )
{
	switch ( index ) {
	case SEG_REG_ES: return "es";
	case SEG_REG_CS: return "cs";
	case SEG_REG_SS: return "ss";
	case SEG_REG_DS: return "ds";
	case SEG_REG_FS: return "fs";
	case SEG_REG_GS: return "gs";
	case SEG_REG_LDTR: return "ldtr";
	case SEG_REG_TR: return "tr";
	default: Match_failure ( "SegRegIndex_to_string\n" );
	}

	Match_failure ( "SegRegIndex_to_string\n" );
	return NULL;
}

/****************************************************************/

const char *
TblIndicator_to_string ( tbl_indicator_t x )
{
	switch ( x ) {
	case TBL_INDICATOR_GDT: return "Gdt";
	case TBL_INDICATOR_LDT: return "Ldt";
	default: Match_failure ( "TblIndicator_to_string\n" );
	}

	Match_failure ( "TblIndicator_to_string\n" );
	return NULL;
}

struct seg_selector_t 
SegSelector_of_bit16u ( bit16u_t val )
{
	struct seg_selector_t x;
   
	x.index = SUB_BIT ( val, 3, 13 );
	x.tbl_indicator = SUB_BIT ( val, 2, 1 );
	x.rpl = SUB_BIT ( val, 0, 2 );

	return x;
}

bit16u_t
SegSelector_to_bit16u ( const struct seg_selector_t *x )
{
	ASSERT ( x != NULL );

	return ( ( x->rpl ) |
		 ( x->tbl_indicator << 2 ) |
		 ( x->index << 3 ) );
}

void
SegSelector_print ( FILE *stream, const struct seg_selector_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream, "{ index=%#x, ti=%#x, rpl=%#x }\n", x->index, x->tbl_indicator, x->rpl );
}

#ifdef DEBUG

void SEG_SELECTOR_DPRINT ( const struct seg_selector_t *x ) { SegSelector_print ( stderr, x ); }

#else

void SEG_SELECTOR_DPRINT ( const struct seg_selector_t *x ) { }

#endif

/****************************************************************/

struct seg_reg_t
SegReg_create ( const struct seg_selector_t *selector, const struct descr_t *cache )
{
	struct seg_reg_t x;

	ASSERT ( selector != NULL );
	ASSERT ( cache != NULL );

	x.selector = *selector;
	x.cache = *cache;

	return x;
}

bit16u_t
SegReg_to_bit16u ( const struct seg_reg_t *x )
{
	ASSERT ( x != NULL );
	return SegSelector_to_bit16u ( &x->selector );
}

void
SegReg_print ( FILE *stream, const struct seg_reg_t *x )
{
	ASSERT ( x != NULL );

	SegSelector_print ( stream, &x->selector );
	// Descr_print ( FILE *stream, &x->cache );

	/* [TODO] print cache ? */
}

/****************************************************************/

struct global_seg_reg_t 
GlobalSegReg_of_bit48u ( bit48u_t val )
{
	struct global_seg_reg_t x;
   
	x.base = SUB_BIT_LL ( val, 16, 32 );
	x.limit = SUB_BIT_LL ( val, 0, 16 );

	return x;
}

void
GlobalSegReg_print ( FILE *stream, const struct global_seg_reg_t *x )
{
	ASSERT ( x != NULL );
	Print ( stream, "{ base=%#x, limit=%#x }\n", x->base, x->limit );
}

/****************************************************************/

bit32u_t
UserRegs_get ( const struct user_regs_struct *uregs, gen_reg_index_t index )
{
	ASSERT ( uregs != NULL );
   
	switch ( index ) {
	case GEN_REG_EAX: return uregs->eax;
	case GEN_REG_ECX: return uregs->ecx;
	case GEN_REG_EDX: return uregs->edx;
	case GEN_REG_EBX: return uregs->ebx;
	case GEN_REG_ESP: return uregs->esp;
	case GEN_REG_EBP: return uregs->ebp;
	case GEN_REG_ESI: return uregs->esi;
	case GEN_REG_EDI: return uregs->edi;
	default: 	  Match_failure ( "UesrRegs_get: index=%d\n", index );
	}

	Match_failure ( "UesrRegs_get: index=%d\n", index );

	return 0;
}

bit32u_t
UserRegs_get2 ( struct user_regs_struct *uregs, gen_reg_index_t index, size_t len )
{
	bit32u_t value;
	size_t n;

	ASSERT ( uregs != NULL );
   
	value = UserRegs_get ( uregs, index );
	n = len * 8;

	return ( n == 32 ) ? value : SUB_BIT ( value, 0, n );
}

void
UserRegs_set ( struct user_regs_struct *uregs, gen_reg_index_t index, bit32u_t value )
{
	ASSERT ( uregs != NULL );
   
	switch ( index ) {
	case GEN_REG_EAX: uregs->eax = value; break;
	case GEN_REG_ECX: uregs->ecx = value; break;
	case GEN_REG_EDX: uregs->edx = value; break;
	case GEN_REG_EBX: uregs->ebx = value; break;
	case GEN_REG_ESP: uregs->esp = value; break;
	case GEN_REG_EBP: uregs->ebp = value; break;
	case GEN_REG_ESI: uregs->esi = value; break;
	case GEN_REG_EDI: uregs->edi = value; break;
	default: 	  Match_failure ( "UserRegs_set: index=%d\n", index );
	}
}

void
UserRegs_set2 ( struct user_regs_struct *uregs, gen_reg_index_t index, bit32u_t value, size_t len )
{
	bit32u_t orig;
	size_t n;

	ASSERT ( uregs != NULL );

	n = len * 8;
	orig = UserRegs_get ( uregs, index );
	value = ( n == 32 ) ? value : ( ( BIT_ALIGN ( orig, n ) | ( SUB_BIT ( value, 0, n ) ) ) );
	UserRegs_set ( uregs, index, value );
}

void
UserRegs_set_from_sigcontext ( struct user_regs_struct *uregs, struct sigcontext *sc )
{
	ASSERT ( uregs != NULL );
	ASSERT ( sc != NULL );
   
	uregs->eax = sc->eax;
	uregs->ecx = sc->ecx;
	uregs->edx = sc->edx;
	uregs->ebx = sc->ebx;
	uregs->esp = sc->esp;
	uregs->ebp = sc->ebp;
	uregs->esi = sc->esi;
	uregs->edi = sc->edi;

	uregs->eip = sc->eip;
	uregs->eflags = sc->eflags;
}

void 
UserRegs_print ( FILE *stream, const struct user_regs_struct *x )
{
	ASSERT ( x != NULL );
   
	Print ( stream, "uregs = { eax=%#x, ebx=%#x, ecx=%#x, edx=%#x, esi=%#x, edi=%#x\n"
		"     eip=%#x, esp=%#x, ebp=%#x eflags=%#x }\n",
		x->eax, x->ebx, x->ecx, x->edx, x->esi, x->edi,
		x->eip, x->esp, x->ebp, x->eflags );
}

/*
 eflags= 0x210246
 0010 0001 0000 0010 0100 0110


 eflags= 0x210346
 0010 0001 0000 0011 0100 0110

 eflags= 0x210207
 0010 0001 0000 0010 0000 0111
*/

/****************************************************************/

/* System Flags in the EFLAGS Register
 * Bit |
 * -----+-----------------------------
 *  0 | CF ( Carry Flag )
 *  1 | 1
 *  2 | PF ( Parity Flag )
 *  3 | 0
 *
 *  4 | AF ( Auxiliary Carry Flag )
 *  5 | 0 
 *  6 | ZF ( Zero Flag )
 *  7 | SF ( Sign Flag )
 *
 *  8 | TF ( Trap Flag )
 *  9 | IF ( Interrupt Enable Flag ) 
 * 10 | DF ( Direction Flag )
 * 11 | OF ( Overflow Flag )
 *
 * 12 | IOPL ( I/O Privilege Level )
 * 13 | IOPL ( I/O Privilege Level )
 * 14 | NT ( Nested Task )
 * 15 | 0
 *
 * 16 | RF ( Resume Flag )
 * 17 | VM ( Virtual-8086 Mode )
 * 18 | AC ( Alignment Check )
 * 19 | VIF ( Virtual Interrupt Flag )
 *
 * 20 | VIP ( Virtual Interrupt Pending )
 * 21 | ID ( Identification Flag )
 * 22 | Reserved
 *    |
 * .. | ...
 *    |
 * 31 | Reserved
 */
struct flag_reg_t 
FlagReg_of_bit32u ( bit32u_t val )
{

	struct flag_reg_t x;

	x.val = val;

	x.interrupt_enable_flag 	= SUB_BIT ( val, 9, 1 );
	x.direction_flag		= SUB_BIT ( val, 10, 1 );
	x.iopl          		= SUB_BIT ( val, 12, 2 );
	x.nested_task      		= SUB_BIT ( val, 14, 1 );
	x.virtual_8086_mode   		= SUB_BIT ( val, 17, 1 );
	x.virtual_interrupt_pending	= SUB_BIT ( val, 20, 1 );

	return x;
}

void
FlagReg_set_direction_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	SET_BIT ( x->val, 10 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_clear_direction_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	CLEAR_BIT ( x->val, 10 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_set_interrupt_enable_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	SET_BIT ( x->val, 9 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_clear_interrupt_enable_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	CLEAR_BIT ( x->val, 9 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_set_virtual_interrupt_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	SET_BIT ( x->val, 19 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_clear_virtual_interrupt_flag ( struct flag_reg_t *x )
{
	ASSERT ( x != NULL );

	CLEAR_BIT ( x->val, 19 );
	*x = FlagReg_of_bit32u ( x->val );
}

void
FlagReg_print ( FILE *stream, const struct flag_reg_t *x )
{
	ASSERT ( x != NULL );
   
	Print ( stream, " eflags = { val=%#x, if=%d, df=%d, iopl=%d }\n",
		x->val, x->interrupt_enable_flag, x->direction_flag, x->iopl );
}


const bit32u_t EFLAGS_MERGE_MASK 
= ( ( 1 << 0 ) |
  ( 1 << 2 ) |
  ( 1 << 4 ) |
  ( 1 << 6 ) |
  ( 1 << 7 ) |
  ( 1 << 10 ) |
  ( 1 << 11 ) );

/* [NOTE] 
 * The second argument of this function is a value of eflags
 * register that is obtained from the ptrace system call. */
void
FlagReg_merge1 ( struct flag_reg_t *x, bit32u_t val )
{
	bit32u_t v;

	ASSERT ( x != NULL );

	v = ( ( val & EFLAGS_MERGE_MASK ) | ( x->val & ~EFLAGS_MERGE_MASK ) );
	*x = FlagReg_of_bit32u ( v );
}

/* [NOTE] 
 * The second argument of this function is a value of eflags
 * register that is obtained from the ptrace system call. */
bit32u_t
FlagReg_merge2 ( const struct flag_reg_t *x, bit32u_t val )
{
	ASSERT ( x != NULL );

	return ( ( val & ~EFLAGS_MERGE_MASK ) | ( x->val & EFLAGS_MERGE_MASK ) );
}

bit32u_t 
FlagReg_get_delta ( const struct flag_reg_t *x, size_t len )
{
	ASSERT ( x != NULL );

	return ( ( x->direction_flag == FALSE ) ? 
		 len :
		 ( -1 ) * len );
}

/****************************************************************/

struct cr0_t 
Cr0_of_bit32u ( bit32u_t val )
{
	struct cr0_t x;

	x.val = val;
	x.protection_enable = SUB_BIT ( val, 0, 1 );
	x.task_switched = SUB_BIT ( val, 3, 1 );
	x.paging = SUB_BIT ( val, 31, 1 );

	return x;
}

void
Cr0_set_paging ( struct cr0_t *x )
{
	ASSERT ( x != NULL );

	SET_BIT ( x->val, 31 );
	*x = Cr0_of_bit32u ( x->val );
}

void
Cr0_clear_paging ( struct cr0_t *x )
{
	ASSERT ( x != NULL );
	
	CLEAR_BIT ( x->val, 31 );
	*x = Cr0_of_bit32u ( x->val );
}

void
Cr0_set_task_switched ( struct cr0_t *x )
{
	ASSERT ( x != NULL );
	
	SET_BIT ( x->val, 3 );
	*x = Cr0_of_bit32u ( x->val );
}

void
Cr0_clear_task_switched ( struct cr0_t *x )
{
	ASSERT ( x != NULL );
	
	CLEAR_BIT ( x->val, 3 );
	*x = Cr0_of_bit32u ( x->val );
}

/****************************************************************/

struct cr3_t 
Cr3_of_bit32u ( bit32u_t val )
{
	struct cr3_t x;
	
	x.val = val;
	x.base = SUB_BIT ( val, 12, 20 );
	
	return x;
}

/****************************************************************/

struct cr4_t 
Cr4_of_bit32u ( bit32u_t val )
{
	struct cr4_t x;

	x.val = val;
	x.page_size_extension = SUB_BIT ( val, 4, 1 );
	x.physical_address_extension = SUB_BIT ( val, 5, 1 );
	x.page_global_enable = SUB_BIT ( val, 7, 1 );
	
	return x;
}

void
Cr4_set_page_size_extension ( struct cr4_t *x )
{
	ASSERT ( x != NULL );
	
	SET_BIT ( x->val, 4 );
	*x = Cr4_of_bit32u ( x->val );
}

void
Cr4_clear_page_size_extension ( struct cr4_t *x )
{
	ASSERT ( x != NULL );
	
	CLEAR_BIT ( x->val, 4 );
	*x = Cr4_of_bit32u ( x->val );
}

void
Cr4_set_page_global_enable ( struct cr4_t *x )
{
	ASSERT ( x != NULL );
	
	SET_BIT ( x->val, 7 );
	*x = Cr4_of_bit32u ( x->val );
}

void
Cr4_clear_page_global_enable ( struct cr4_t *x )
{
	ASSERT ( x != NULL );
	
	CLEAR_BIT ( x->val, 7 );
	*x = Cr4_of_bit32u ( x->val );
}

/****************************************************************/

void 
SysRegs_print ( FILE *stream, const struct sys_regs_t *x )
{
	ASSERT ( x != NULL );

	Print ( stream, " sys = {\n" );
	Print ( stream, "     cr0=%#x, cr1=%#x, cr2=%#x, cr3=%#x, cr4=%#x\n",
		x->cr0.val, x->cr1, x->cr2, x->cr3, x->cr4 );
	Print ( stream, "     gdtr = " ); GlobalSegReg_print ( stream, &x->gdtr );
	Print ( stream, "     idtr = " ); GlobalSegReg_print ( stream, &x->idtr );
	Print ( stream, "     ldtr = " ); SegReg_print ( stream, &x->ldtr );
	Print ( stream, "     tr  = " ); SegReg_print ( stream, &x->tr );
	Print ( stream, "    }\n" );
}

/****************************************************************/

struct descr_t
Regs_lookup_descr_table ( const struct regs_t *regs, const struct seg_selector_t *selector, trans_t *laddr_to_raddr )
{
	struct descr_t descr;
	
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );

	switch ( selector->tbl_indicator ) {
	case TBL_INDICATOR_GDT: descr = lookup_gdt ( regs, selector->index, laddr_to_raddr ); break;
	case TBL_INDICATOR_LDT: descr = lookup_ldt ( regs, selector->index, laddr_to_raddr ); break;
	default: Fatal_failure ( "Regs_lookup_descr_table" );
	}
	
	return descr;
}

void
Regs_update_descr_table ( const struct regs_t *regs, 
			const struct seg_selector_t *selector, 
			const struct descr_t *descr, 
			trans_t *laddr_to_raddr )
{
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	switch ( selector->tbl_indicator ) {
	case TBL_INDICATOR_GDT: update_gdt ( regs, selector->index, descr, laddr_to_raddr ); break;
	case TBL_INDICATOR_LDT: update_ldt ( regs, selector->index, descr, laddr_to_raddr ); break;
	default: Fatal_failure ( "Regs_update_descr_table" );
	}
}

bit32u_t
Regs_get_gen_reg ( struct regs_t *regs, gen_reg_index_t index )
{
	ASSERT ( regs != NULL );
	return UserRegs_get ( &regs->user, index );
}

void
Regs_set_gen_reg ( struct regs_t *regs, gen_reg_index_t index, bit32u_t val )
{
	ASSERT ( regs != NULL );
	UserRegs_set ( &regs->user, index, val );
}

bit16u_t
Regs_get_seg_reg ( struct regs_t *regs, seg_reg_index_t index )
{
	ASSERT ( regs != NULL );
	ASSERT ( ( index >= 0 ) && ( index < NUM_OF_SEG_REGS ) );
	return SegReg_to_bit16u ( &regs->segs[index] );
}

void 
Regs_set_seg_reg ( struct regs_t *regs, seg_reg_index_t index, struct seg_selector_t *selector, trans_t *laddr_to_raddr )
{
	struct seg_reg_t *sreg;
	
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );

	//DPRINT ( "Regs_set_seg_reg: seg=%s, selector=", SegRegIndex_to_string ( index ) );
	//SEG_SELECTOR_DPRINT ( selector );

	if ( index == SEG_REG_LDTR ) {
		sreg = &regs->sys.ldtr;
	} else if ( index == SEG_REG_TR ) {
		sreg = &regs->sys.tr;
	} else {
		ASSERT ( ( index >= 0 ) && ( index < NUM_OF_SEG_REGS ) );
		sreg = &regs->segs[index];
	}

	sreg->selector = *selector;
	sreg->cache  = Regs_lookup_descr_table ( regs, selector, laddr_to_raddr );

	sreg->val = SegReg_to_bit16u ( sreg ); 

	/* [TOOD] set the accessed flag of the descriptor */

	//DESCR_DPRINT ( &sreg->cache );
}

void
Regs_set_seg_reg2 ( struct regs_t *regs, seg_reg_index_t index, bit16u_t val, trans_t *laddr_to_raddr )
{
	struct seg_selector_t selector;
	
	ASSERT ( regs != NULL );

	selector = SegSelector_of_bit16u ( val );
	Regs_set_seg_reg ( regs, index, &selector, laddr_to_raddr );
}

void 
Regs_print ( FILE *stream, const struct regs_t *x )
{
	int i;
	
	ASSERT ( x != NULL );

	UserRegs_print ( stream, &x->user );

	for ( i = 0; i < NUM_OF_SEG_REGS; i++ ) { 
		Print ( stream, " %s = ", SegRegIndex_to_string ( i ) );
		SegReg_print ( stream, &x->segs[i] ); 
	}

	FlagReg_print ( stream, &x->eflags );

	SysRegs_print ( stream, &x->sys );
}

void
Regs_pack ( const struct regs_t *x, int fd )
{
	const size_t LEN = sizeof ( struct regs_t );
	Pack ( ( void * ) x, LEN, fd );
}

void
Regs_unpack ( const struct regs_t *x, int fd )
{
	const size_t LEN = sizeof ( struct regs_t );
	Unpack (  ( void * ) x, LEN, fd );
}

/****************************************************************/

struct descr_t
lookup_gdt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr )
{
	bit32u_t p;
	bit32u_t vals[2];
	
	ASSERT ( regs != NULL );
	assert ( index >= 0 );

	p = regs->sys.gdtr.base + index * SIZE_OF_DESCR;

	vals[0] = read_dword ( p, laddr_to_raddr );
	vals[1] = read_dword ( p + 4, laddr_to_raddr );
	//DPRINT ( "lookup gdt: gdtr.base=%#x, index=%#x, val= ( %#x,%#x )\n", regs->sys.gdtr.base, index, vals[0], vals[1] );

	return Descr_of_bit64 ( vals );
}

struct descr_t
lookup_ldt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr )
{
	bit32u_t p;
	bit32u_t vals[2];
	bit32u_t base;

	ASSERT ( regs != NULL );
	assert ( index >= 0 );

	base = Descr_base ( &regs->sys.ldtr.cache );
	if ( base != 0 ) {
		Fatal_failure ( "lookup_ldt: index=%#x, base is not zero\n", index );
	}
	p = base + index * SIZE_OF_DESCR;

	vals[0] = read_dword ( p, laddr_to_raddr );
	vals[1] = read_dword ( p + 4, laddr_to_raddr );
	
	return Descr_of_bit64 ( vals );
}

struct descr_t
lookup_idt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr )
{
	bit32u_t p;
	bit32u_t vals[2];

	ASSERT ( regs != NULL );
	assert ( index >= 0 );
	
	p = regs->sys.idtr.base + index * SIZE_OF_DESCR;

	vals[0] = read_dword ( p, laddr_to_raddr );
	vals[1] = read_dword ( p + 4, laddr_to_raddr );
	
	return Descr_of_bit64 ( vals );
}

void
update_gdt ( const struct regs_t *regs, int index, const struct descr_t *descr, trans_t *laddr_to_raddr )
{
	bit32u_t p;

	ASSERT ( regs != NULL );
	ASSERT ( descr != NULL );
	
	p = regs->sys.gdtr.base + index * SIZE_OF_DESCR;

	write_dword ( p, descr->vals[0], laddr_to_raddr );
	write_dword ( p + 4, descr->vals[1], laddr_to_raddr );
}

void
update_ldt ( const struct regs_t *regs, int index, const struct descr_t *descr, trans_t *laddr_to_raddr )
{
	bit32u_t p;
	bit32u_t base;

	ASSERT ( regs != NULL );
	ASSERT ( descr != NULL );

	base = Descr_base ( &regs->sys.ldtr.cache );
	if ( base != 0 ) {
		Fatal_failure ( "update_ldt: index=%#x, base is not zero\n", index );
	}	
	p = base + index * SIZE_OF_DESCR;

	write_dword ( p, descr->vals[0], laddr_to_raddr );
	write_dword ( p + 4, descr->vals[1], laddr_to_raddr );
}

void
update_idt ( const struct regs_t *regs, int index, const struct descr_t *descr, trans_t *laddr_to_raddr )
{
	bit32u_t p;

	ASSERT ( regs != NULL );
	ASSERT ( descr != NULL );
	
	p = regs->sys.idtr.base + index * SIZE_OF_DESCR;

	write_dword ( p, descr->vals[0], laddr_to_raddr );
	write_dword ( p + 4, descr->vals[1], laddr_to_raddr );
}

privilege_level_t
cpl ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	
	return regs->segs[SEG_REG_CS].selector.rpl;
}

bool_t
cpl_is_user_mode ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	
	return ( cpl ( regs ) == USER_MODE );
}

bool_t
cpl_is_supervisor_mode ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	
	return ( cpl ( regs ) == SUPERVISOR_MODE );
}

void
pushl ( struct regs_t *regs, bit32u_t value, trans_t *vaddr_to_raddr )
{
	ASSERT ( regs != NULL );
   
	regs->user.esp -= 4;
	write_dword ( regs->user.esp, value, vaddr_to_raddr );
}


