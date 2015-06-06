#include "vmm/mon/mon.h"


static void
movs ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	seg_reg_index_t seg;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = ( instr->sreg_index != SEG_REG_NULL ) ? instr->sreg_index : SEG_REG_DS;   

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t value;

		value = Monitor_read_with_vaddr ( seg, mon->regs->user.esi, len );
		Monitor_write_with_vaddr ( SEG_REG_ES, mon->regs->user.edi, value, len );

		mon->regs->user.esi += delta;
		mon->regs->user.edi += delta;
	}

	if ( instr->rep_repe_repz ) 
		mon->regs->user.ecx = 0;
   
	skip_instr ( mon, instr );   
}

void movsb_xb_yb ( struct mon_t *mon, struct instruction_t *instr ) { movs ( mon, instr, 1 ); }
void movsw_xv_yv ( struct mon_t *mon, struct instruction_t *instr ) { movs ( mon, instr, ((instr->opsize_override) ? 2 : 4) ); }
static struct mem_access_t *
movs_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len ) 
{
	struct mem_access_t *maccess = NULL;
	seg_reg_index_t seg;
	bit32u_t src_vaddr, dest_vaddr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = ( instr->sreg_index != SEG_REG_NULL ) ? instr->sreg_index : SEG_REG_DS;   
	src_vaddr = mon->regs->user.esi;
	dest_vaddr = mon->regs->user.edi;

	maccess = MemAccess_create_read ( seg, src_vaddr, len );
	maccess->next = MemAccess_create_write ( SEG_REG_ES, dest_vaddr, len );

	return maccess;   
}

struct mem_access_t *movsb_xb_yb_mem ( struct mon_t *mon, struct instruction_t *instr ) { return movs_mem ( mon, instr, 1 ); }
struct mem_access_t *movsw_xv_yv_mem ( struct mon_t *mon, struct instruction_t *instr ) { return movs_mem ( mon, instr, ((instr->opsize_override) ? 2 : 4) ); }

/****************************************************************/

static void
stos ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t val;

		val = UserRegs_get2 ( &mon->regs->user, GEN_REG_EAX, len );
		Monitor_write_with_vaddr ( SEG_REG_ES, mon->regs->user.edi, val, len );
		mon->regs->user.edi += delta;
	}

	if ( instr->rep_repe_repz ) {
		mon->regs->user.ecx = 0;
	}

	skip_instr ( mon, instr );
}

void stosb_yb_al ( struct mon_t *mon, struct instruction_t *instr )  { stos ( mon, instr, 1 ); }

void stosw_yv_eax ( struct mon_t *mon, struct instruction_t *instr ) { stos ( mon, instr, 4 ); }

static struct mem_access_t *
stos_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	/*
	{
		bit32u_t count, delta;   

		delta = FlagReg_get_delta ( &mon->regs->eflags, len );
		count = get_rep_count ( mon, instr );

		fprintf ( stderr,
			  "stos: len = %d, edi=%#x, count=%#x , delta=%#x, [%#x, ..., %#x]\n",
			  len,
			  mon->regs->user.edi,
			  count,
			  delta,
			  mon->regs->user.edi,
			  mon->regs->user.edi + delta * count );
	}
	*/

	return MemAccess_create_write ( SEG_REG_ES, mon->regs->user.edi, len );
}

struct mem_access_t *stosb_yb_al_mem ( struct mon_t *mon, struct instruction_t *instr ) { return stos_mem ( mon, instr, 1 ); }
struct mem_access_t *stosw_yv_eax_mem ( struct mon_t *mon, struct instruction_t *instr ) { return stos_mem ( mon, instr, 4 ); }


/****************************************************************/

static bool_t
check_rep_condition ( struct instruction_t *instr, bool_t zf )
{
	ASSERT ( instr != NULL );
	return ( ( ( instr->rep_repe_repz ) && ( !zf ) ) ||
		 ( ( instr->repne_repnz ) && ( zf ) ) );
}

#ifdef ENABLE_MP

static bool_t
check_read_permission ( struct mon_t *mon, seg_reg_index_t i, bit32u_t vaddr, size_t len )
{
	int page_no;
	struct page_descr_t *pdescr;
	bit32u_t paddr;
	bool_t is_ok;

	ASSERT ( mon != NULL );
	
	paddr = Monitor_try_vaddr_to_paddr ( i, vaddr, &is_ok );
	ASSERT ( is_ok );

	page_no = paddr_to_page_no ( paddr );
	pdescr = &( mon->page_descrs[page_no] );
	
	return ( pdescr->state != PAGE_STATE_INVALID );
}

#else /* ! ENABLE_MP */

static bool_t
check_read_permission ( struct mon_t *mon, seg_reg_index_t i, bit32u_t vaddr, size_t len )
{
	return TRUE;
}

#endif /* ENABLE_MP */

void scasb_al_xb ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }
void scasw_eax_xv ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }

struct mem_access_t *
__scas_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	seg_reg_index_t seg;
	struct mem_access_t *maccess = NULL;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = ( instr->sreg_index != SEG_REG_NULL ) ? instr->sreg_index : SEG_REG_DS;   

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );

	for ( i = 0; i < count; i++ ) {
		bit32u_t vaddr;
		struct mem_access_t *x;

		vaddr = mon->regs->user.edi + delta * i;

		x = MemAccess_create_read ( SEG_REG_ES, vaddr, len );

		/* add <x> to <maccess> */
		if ( maccess == NULL ) {
			maccess = x;
		} else {
			x->next = maccess;
			maccess = x;
		}

#if 1
		/* [TEST]
		   page fault が発生する原因は 1 つ目の maccess なのでは？
		   そうでなければ、string instruction は、実行が先に進んでいるはずなので
		   （多分正しい）
		*/
		return maccess;
#endif

		{ /* check loop exit condition */
			bit32u_t vals[2];
			bool_t zf;
			bool_t is_ok;

			is_ok = Monitor_check_mem_access_with_vaddr ( mon->regs, SEG_REG_ES, vaddr );
			if ( ! is_ok ) 
				break;

			if ( ! check_read_permission ( mon, SEG_REG_ES, vaddr, len ) )
				break;

			vals[0] = UserRegs_get2 ( &mon->regs->user, GEN_REG_EAX, len );
			vals[1] = Monitor_read_with_vaddr ( SEG_REG_ES, vaddr, len );

			zf = ( vals[0] == vals[1] );
			if ( check_rep_condition ( instr, zf ) )
				break;
		}
	}

	DPRINT ( "scasb: edi=%#x, ecx=%#x, len=%#x\n", 
		 mon->regs->user.edi, mon->regs->user.ecx, delta * i );

	return maccess;
}

struct mem_access_t *
scasb_al_xb_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return __scas_mem ( mon, instr, 1 );
}

struct mem_access_t *
scasw_eax_xv_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return __scas_mem ( mon, instr, 4 );
}

/****************************************************************/

void cmpsb_xb_yb ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }
void cmpsw_xv_yv ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }

struct mem_access_t *
cmps_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	seg_reg_index_t seg;
	struct mem_access_t *maccess = NULL;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = ( instr->sreg_index != SEG_REG_NULL ) ? instr->sreg_index : SEG_REG_DS;   

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t src_vaddr, dest_vaddr;
		struct mem_access_t *x;

		src_vaddr = mon->regs->user.esi + delta * i;
		dest_vaddr = mon->regs->user.edi + delta * i;

		x = MemAccess_create_read ( seg, src_vaddr, len );
		x->next = MemAccess_create_read ( SEG_REG_ES, dest_vaddr, len );

		/* add <x> and <x->next> to <maccess> */
		if ( maccess == NULL ) {
			maccess = x;
		} else {
			x->next->next = maccess;
			maccess = x;
		}


#if 1
		/* [TEST]
		   page fault が発生する原因は 1 つ目の maccess なのでは？
		   そうでなければ、string instruction は、実行が先に進んでいるはずなので
		   （多分正しい）
		*/
		return maccess;
#endif

		{ /* check loop exit condition */
			bit32u_t vals[2];
			bool_t is_oks[2];
			bool_t zf;

			is_oks[0] = Monitor_check_mem_access_with_vaddr ( mon->regs, seg, src_vaddr );
			is_oks[1] = Monitor_check_mem_access_with_vaddr ( mon->regs, SEG_REG_ES, dest_vaddr );

			if ( ( !is_oks[0] ) || ( !is_oks[1] ) ) 
				break;

			if ( ( ! check_read_permission ( mon, seg, src_vaddr, len ) ) ||
			     ( ! check_read_permission ( mon, SEG_REG_ES, dest_vaddr, len ) ) )
				break;

			vals[0] = Monitor_read_with_vaddr ( seg, src_vaddr, len );
			vals[1] = Monitor_read_with_vaddr ( SEG_REG_ES, dest_vaddr, len );
			zf = ( vals[0] == vals[1] );
			if ( check_rep_condition ( instr, zf ) ) 
				break;
		}
	}

	return maccess;
}

struct mem_access_t *
cmpsb_xb_yb_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{ 
	return cmps_mem ( mon, instr, 1 );
}

struct mem_access_t *
cmpsw_xv_yv_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return cmps_mem ( mon, instr, 4 );
}

/****************************************************************/

/* Load String */
void lodsb_al_xb ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }
void lodsw_eax_xv ( struct mon_t *mon, struct instruction_t *instr ) { assert ( 0 ); }


struct mem_access_t *
lods_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	seg_reg_index_t seg;
	struct mem_access_t *maccess = NULL;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = ( instr->sreg_index != SEG_REG_NULL ) ? instr->sreg_index : SEG_REG_DS;   

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t vaddr;
		struct mem_access_t *x;

		vaddr = mon->regs->user.esi + delta * i;
		x = MemAccess_create_read ( seg, vaddr, len );

		/* add <x> to <maccess> */
		if ( maccess == NULL ) {
			maccess = x;
		} else {
			x->next = maccess;
			maccess = x;
		}

#if 1
		/* [TEST]
		   page fault が発生する原因は 1 つ目の maccess なのでは？
		   そうでなければ、string instruction は、実行が先に進んでいるはずなので
		   （多分正しい）
		*/
		return maccess;
#endif
	}

	return maccess;
}

struct mem_access_t *
lodsb_al_xb_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{ 
	return lods_mem ( mon, instr, 1 );
}

struct mem_access_t *
lodsw_eax_xv_mem ( struct mon_t *mon, struct instruction_t *instr )
{
	return lods_mem ( mon, instr, 4 );
}
