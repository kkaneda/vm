#include "vmm/ia32/paging_common.h"
#include "vmm/ia32/cpu.h"


struct linear_addr_t
LinearAddr_of_bit32u ( bit32u_t laddr )
{
	struct linear_addr_t retval;

	retval.dir = SUB_BIT ( laddr, 22, 10 );
	retval.table = SUB_BIT ( laddr, 12, 10 );
	retval.offset = SUB_BIT ( laddr, 0, 12 );

	return retval;
}

bit32u_t
LinearAddr_to_bit32u ( struct linear_addr_t x )
{
	return ( ( x.dir << 22 ) | ( x.table << 12 ) | x.offset );
}

struct pdir_entry_t
PdirEntry_create ( bit32u_t paddr, bit32u_t val ) 
{
	struct pdir_entry_t x;

	x.paddr = paddr;
	x.val = val;

	x.present = SUB_BIT ( val, 0, 1 ); 
	x.read_write = SUB_BIT ( val, 1, 1 ); 
	x.accessed = SUB_BIT ( val, 5, 1 ); 
	x.dirty = SUB_BIT ( val, 6, 1 ); 
	x.page_size = SUB_BIT ( val, 7, 1 ); 

	if ( x.page_size ) {
		x.base.page = ( SUB_BIT ( val, 22, 10 ) | LSHIFTED_SUB_BIT ( val, 13, 4, 10 ) );
	} else {
		x.base.ptbl = SUB_BIT ( val, 12, 20 );
	}
	return x;
}

void
PdirEntry_set_accessed_flag ( struct pdir_entry_t *pde, trans_t *paddr_to_raddr ) 
{
	ASSERT ( pde != NULL );

	SET_BIT ( pde->val, 5 );
	write_dword ( pde->paddr, pde->val, paddr_to_raddr );
}

void
PdirEntry_set_dirty_flag ( struct pdir_entry_t *pde, trans_t *paddr_to_raddr ) 
{
	ASSERT ( pde != NULL );

	SET_BIT ( pde->val, 6 );
	write_dword ( pde->paddr, pde->val, paddr_to_raddr );
}

struct ptbl_entry_t
PtblEntry_create ( bit32u_t paddr, bit32u_t val ) 
{
	struct ptbl_entry_t x;

	x.paddr = paddr;
	x.val = val;
	x.present = SUB_BIT ( val, 0, 1 );
	x.read_write = SUB_BIT ( val, 1, 1 ); 
	x.accessed = SUB_BIT ( val, 5, 1 ); 
	x.dirty = SUB_BIT ( val, 6, 1 ); 
	x.base = SUB_BIT ( val, 12, 20 );

	return x;   
}

void
PtblEntry_set_accessed_flag ( struct ptbl_entry_t *pte, trans_t *paddr_to_raddr ) 
{
	ASSERT ( pte != NULL );

	SET_BIT ( pte->val, 5 );
	write_dword ( pte->paddr, pte->val, paddr_to_raddr );
}

void
PtblEntry_set_dirty_flag ( struct ptbl_entry_t *pte, trans_t *paddr_to_raddr ) 
{
	ASSERT ( pte != NULL );

	SET_BIT ( pte->val, 6 );
	write_dword ( pte->paddr, pte->val, paddr_to_raddr );
}

bool_t
paging_is_enabled ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	return ( regs->sys.cr0.paging );
}


bool_t
paging_is_disabled ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	return ( !regs->sys.cr0.paging );
}


struct pdir_entry_t 
lookup_page_directory ( const struct regs_t *regs, int index, trans_t *paddr_to_raddr )
{
	bit32u_t paddr;
	bit32u_t val;

	ASSERT ( regs != NULL );
	paddr = ( regs->sys.cr3.base << 12 ) + index * PDIR_ENTRY_SIZE;
	val = read_dword ( paddr, paddr_to_raddr );

	return PdirEntry_create ( paddr, val );
}

struct ptbl_entry_t
lookup_page_table ( const struct pdir_entry_t *pde, int index, trans_t *paddr_to_raddr )
{
	bit32u_t paddr;
	bit32u_t val;

	ASSERT ( pde != NULL );
   
	paddr = ( pde->base.ptbl << 12 ) + index * PTBL_ENTRY_SIZE;
	val = read_dword ( paddr, paddr_to_raddr );

	return PtblEntry_create ( paddr, val );
}

bit32u_t
try_translate_laddr_to_paddr2 ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr, bool_t *is_ok,
			       bool_t *read_write )
{
	bit32u_t paddr;
	struct linear_addr_t p;
	struct pdir_entry_t pde;
	struct ptbl_entry_t pte;

	ASSERT ( regs != NULL );
	ASSERT ( is_ok != NULL );
	ASSERT ( read_write != NULL );

	*read_write = TRUE;
	*is_ok = TRUE;

	if ( paging_is_disabled ( regs ) ) 
		return laddr;
	
	p = LinearAddr_of_bit32u ( laddr );
	pde = lookup_page_directory ( regs, p.dir, paddr_to_raddr );
	if ( ! pde.present ) {
		*is_ok = FALSE; 
		*read_write = FALSE;
		return 0; 
	}

	if ( pde.page_size ) {
		/* [TODO] 
		 * The current implementation does not support 36-bit physical addressing.
		 * The 4 most significant bits of an address is simply ignored. 
		 */
		bit10u_t base;
		
		ASSERT ( regs->sys.cr4.page_size_extension );
		
		base = SUB_BIT ( pde.base.page, 0, 10 );
		paddr = ( base << 22 ) | ( p.table << 12 ) | p.offset;
		*read_write = pde.read_write;
	} else {
		pte = lookup_page_table ( &pde, p.table, paddr_to_raddr );
		
		if ( ! pte.present ) { *is_ok = FALSE; return 0; }
		paddr = ( pte.base << 12 ) | p.offset;
		*read_write = pte.read_write;
	}

	return paddr;   
}

bit32u_t
try_translate_laddr_to_paddr ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr, bool_t *is_ok )
{
	bool_t read_write;

	return try_translate_laddr_to_paddr2 ( regs, laddr, paddr_to_raddr, is_ok, &read_write );
}

bit32u_t
translate_laddr_to_paddr ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr )
{
	bit32u_t paddr;
	bool_t is_ok;

	ASSERT ( regs != NULL );

	paddr = try_translate_laddr_to_paddr ( regs, laddr, paddr_to_raddr, &is_ok );

	if ( ! is_ok )
		Fatal_failure ( "translate_laddr_to_paddr: "
				"pde/pte is not present: laddr=%#x\n", laddr );

	return paddr;
}

/* This function returns TRUE if the accessed address is in a memory
 * region */
bool_t
check_mem_access ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr )
{
	bit32u_t paddr;
	bool_t is_ok;

	ASSERT ( regs != NULL );

	paddr = try_translate_laddr_to_paddr ( regs, laddr, paddr_to_raddr, &is_ok );

	/* [TODO] Check permission/priviledge and deliver a fault if necessary. */

	return is_ok;
}

void
print_page_directory ( FILE *stream, const struct regs_t *regs, trans_t *paddr_to_raddr )
{
	struct linear_addr_t addr;

	assert ( stream != NULL );

	addr.offset = 0;

	Print ( stream, "PAGE_DIRECTORY:\n" );

	for ( addr.dir = 0; addr.dir < NUM_OF_PDIR_ENTRIES; addr.dir++ ) {
		struct pdir_entry_t pde;
		bit32u_t laddr, paddr;

		pde = lookup_page_directory ( regs, addr.dir, paddr_to_raddr );
		if ( ! pde.present )
			continue; 

		if ( pde.page_size ) {
			laddr = addr.dir << 22;
			paddr = ( SUB_BIT ( pde.base.page, 0, 10 ) ) << 22;
			Print ( stream,
				"\t" "0x%08x <===> 0x%08x ( size=%x ) %s\n",
				laddr,
				paddr,
				PAGE_SIZE_4M,
				pde.read_write ? "RW" : "R" ); 
		} else {
			for ( addr.table = 0; addr.table < NUM_OF_PTBL_ENTRIES; addr.table++ ) {
				struct ptbl_entry_t pte;

				pte = lookup_page_table ( &pde, addr.table, paddr_to_raddr );
				if ( ! pte.present ) 
					continue;

				laddr = LinearAddr_to_bit32u ( addr );
				paddr = pte.base << 12;

				Print ( stream, "\t" "0x%08x <===> 0x%08x ( size=%x ) %s\n",
					laddr,
					paddr,
					PAGE_SIZE_4K,
					pte.read_write ? "RW" : "R" ); 
			}
		}
	}
}

bit32u_t
page_no_to_paddr ( int page_no )
{
	return page_no << 12;
}

int
paddr_to_page_no ( bit32u_t paddr )
{
	return paddr >> 12;
}

void
PageDescr_init ( struct page_descr_t *x, int cpuid )
{
	ASSERT ( x != NULL );

	x->state = ( cpuid == BSP_CPUID ) ? PAGE_STATE_EXCLUSIVELY_SHARED : PAGE_STATE_INVALID;
	x->owner = BSP_CPUID;
	x->copyset = 1 << BSP_CPUID;
	x->num_of_laddrs = 0;

	x->requesting = FALSE;

	x->seq = 0LL;
}

void
PageDescr_pack ( struct page_descr_t *x, int fd )
{
	ASSERT ( x != NULL );

	/* [DEBUG] */
#if 0
	Bit32uArray_pack ( x->laddrs, MAX_OF_PDESCR_LADDRS, fd );
	BoolArray_pack ( x->read_write, MAX_OF_PDESCR_LADDRS, fd );
	Bit32u_pack ( ( bit32u_t ) x->num_of_laddrs, fd );
#endif

	Bit32u_pack ( ( bit32u_t ) x->state, fd );
	Bit32u_pack ( ( bit32u_t ) x->copyset, fd );
	Bit32u_pack ( ( bit32u_t ) x->owner, fd );
	Bit64u_pack ( ( bit64u_t ) x->seq, fd );
	Bool_pack ( x->requesting, fd );
}

void
PageDescr_unpack ( struct page_descr_t *x, int fd )
{
	ASSERT ( x != NULL );
	
	/* [DEBUG] */
#if 0
	Bit32uArray_unpack ( x->laddrs, MAX_OF_PDESCR_LADDRS, fd );
	BoolArray_unpack ( x->read_write, MAX_OF_PDESCR_LADDRS, fd );
	x->num_of_laddrs = ( bit32u_t ) Bit32u_unpack ( fd );
#else
	x->num_of_laddrs = 0;
#endif

	x->state = ( page_state_t ) Bit32u_unpack ( fd );
	x->copyset = ( bit32u_t ) Bit32u_unpack ( fd );
	x->owner = ( bit32u_t ) Bit32u_unpack ( fd );
	x->seq   = ( bit64u_t ) Bit64u_unpack ( fd );
	x->requesting = Bool_unpack ( fd );
}

const char *
PageState_to_string ( page_state_t x )
{
	switch ( x ) {
	case PAGE_STATE_INVALID:     	    return "INVALID";
	case PAGE_STATE_READ_ONLY_SHARED:   return "RD_SHARED";
	case PAGE_STATE_EXCLUSIVELY_SHARED: return "EX_SHARED";
	default:		 	     Match_failure ( "PageState_to_string\n" );
	}
	Match_failure ( "PageState_to_string\n" );
	return "";
}

void
PageDescr_print ( FILE *stream, struct page_descr_t *x )
{
	int i;

	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream,
		" { state=%s, owner=%#x, copyset=%#x",
		PageState_to_string ( x->state ),
		x->owner,
		x->copyset );
	
	Print ( stream, ", laddrs=" );
	
	for ( i = 0; i < x->num_of_laddrs; i++ ) 
		Print ( stream, "%#x, ", x->laddrs[i] );

	Print ( stream, " }\n" );
}

#ifdef DEBUG
void PAGE_DESCR_DPRINT ( struct page_descr_t *x ) { PageDescr_print ( stderr, x ); }
#else
void PAGE_DESCR_DPRINT ( struct page_descr_t *x ) { }
#endif

void
PageDescr_add_laddr ( struct page_descr_t *x, bit32u_t laddr, bool_t read_write )
{
	int i;

	ASSERT ( x != NULL );

	for ( i = 0; i < x->num_of_laddrs; i++ ) {
		if ( x->laddrs[i] == laddr ) {
			/* Do nothing */
			x->read_write[i] = read_write;
			return;
		}
	}

	ASSERT ( x->num_of_laddrs < MAX_OF_PDESCR_LADDRS );

	x->laddrs[x->num_of_laddrs] = laddr;
	x->read_write[x->num_of_laddrs] = read_write;

	x->num_of_laddrs++;
}

void
PageDescr_remove_laddr ( struct page_descr_t *x, bit32u_t laddr )
{
	int i, n;

	ASSERT ( x != NULL );

	n = -1;
	
	for ( i = 0; i < x->num_of_laddrs; i++ ) {
		if ( x->laddrs[i] == laddr ) {
			n = i;
			break;
		}
	}

	/* [DEBUG] */
	if ( n < 0 ) {
		Print ( stderr, "PageDescr_remove_laddr: laddr=%#x, x->num_of_laddrs=%d\n", 
			laddr,
			x->num_of_laddrs );
		
		for ( i = 0; i < x->num_of_laddrs; i++ ) {
			Print ( stderr, "\t" "x->laddrs[%d]=%#x\n", i, x->laddrs[i] );
		}

	}
	assert ( n >= 0 );

	for ( i = n + 1; i < x->num_of_laddrs; i++ ) {
		x->laddrs[i-1] = x->laddrs[i];
		x->read_write[i-1] = x->read_write[i];
	}

	x->num_of_laddrs--;

	{
		for ( i = 0; i < x->num_of_laddrs; i++ ) {
			assert ( x->laddrs[i] != laddr );
		}
	}
}

bool_t
PageDescr_has_same_laddr ( struct page_descr_t *x, bit32u_t laddr, bool_t read_write )
{
	int i;

	ASSERT ( x != NULL );

	for ( i = 0; i < x->num_of_laddrs; i++ ) {
		if ( ( x->laddrs[i] == laddr ) && ( x->read_write[i] == read_write ) ) {
			return TRUE;
		}
	}

	return FALSE;
}

bool_t
PageDescr_get_read_write ( const struct page_descr_t *x, bit32u_t laddr )
{
	int i;

	ASSERT ( x != NULL );

	for ( i = 0; i < x->num_of_laddrs; i++ ) {
		if ( x->laddrs[i] == laddr ) {
			return x->read_write[i];
		}
	}

	assert ( 0 );
	return FALSE;
}

bool_t
PageDescr_cpuid_is_in_copyset ( const struct page_descr_t *x, int cpuid )
{
	ASSERT ( x != NULL );
	return TEST_BIT ( x->copyset, cpuid );
}
