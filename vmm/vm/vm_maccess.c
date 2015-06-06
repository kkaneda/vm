#include "vmm/vm/vm.h"
#include <sys/mman.h>
#include <fcntl.h>

static inline struct page_descr_t *
get_pdescr_by_page_no ( struct vm_t *vm, int page_no )
{
	ASSERT ( vm != NULL );
	if ( page_no >= vm->num_of_pages ) {
		Print ( stderr, "page_no = %#x, num_of_pages = %#x\n",
			page_no, vm->num_of_pages );
	}
	ASSERT ( page_no < vm->num_of_pages );

	return & ( vm->page_descrs[page_no] );
}

static inline struct page_descr_t *
get_pdescr_by_paddr ( struct vm_t *vm, bit32u_t paddr )
{
	int page_no;

	ASSERT ( vm != NULL );
	ASSERT ( paddr < vm->pmem.ram_offset );

	page_no = paddr_to_page_no ( paddr );
	return get_pdescr_by_page_no ( vm, page_no );
}

/********************************************************************************/

struct mem_map_entry_t *
MemMapEntry_create ( bit32u_t laddr, bit32u_t paddr, bool_t read_write )
{
	struct mem_map_entry_t *x;

	x = Malloct ( struct mem_map_entry_t );

	x->laddr = laddr;
	x->paddr = paddr;
	x->read_write = read_write;

	x->next = NULL;
	
	return x;
}

static void
MemMapEntry_destroy ( struct mem_map_entry_t *x )
{
	ASSERT ( x != NULL );

	Free ( x );
}

static void
MemMapList_init ( struct mem_map_list_t *list )
{
	list->head = NULL;
}

static void 
MemMapList_destroy ( struct mem_map_list_t *list )
{
	struct mem_map_entry_t *p;

	p = list->head;
	while ( p != NULL ) {
		struct mem_map_entry_t *next = p->next;
		MemMapEntry_destroy ( p );
		p = next;
	}

	list->head = NULL;
}

static bit32u_t 
MemMapList_add ( struct mem_map_list_t *list, bit32u_t laddr, bit32u_t paddr, bool_t read_write, bool_t *b )
{
	struct mem_map_entry_t **pp, *p;
	bit32u_t ret;

	p = list->head;
	pp = &list->head;
	while ( p != NULL ) {
	
		if ( laddr >= p->laddr ) {
			break;
		}

		pp = &p->next;
		p = *pp;
	}

	if ( ( p != NULL ) &&  ( laddr == p->laddr ) ) {
		*b = ( paddr != p->paddr );
		ret = p->paddr;

		p->paddr = paddr;
	} else {
		struct mem_map_entry_t *e;

		e = MemMapEntry_create ( laddr, paddr, read_write );
		e->next = p;
		*pp = e;

		*b = FALSE;
		ret = 0;
	}

	return ret;
}

static struct mem_map_entry_t *
MemMapList_find ( struct mem_map_list_t *list, bit32u_t laddr )
{
	struct mem_map_entry_t *p;

	for ( p = list->head; p != NULL; p = p->next ) {
		if ( p->laddr == laddr ) {
			return p;
		}
	}

	return NULL;
}

static void
MemMapList_print ( FILE *fp, struct mem_map_list_t *list )
{
	struct mem_map_entry_t *p;

	for ( p = list->head; p != NULL; p = p->next ) {
		Print ( fp, "%#x ---> %#x (%d)\n", p->laddr, p->paddr, p->read_write );
	}
}

/********************************************************************************/

static void
workspace_init_for_vm ( void )
{
	void *p;
	p = Mmap ( ( void * )WORKSPACE_BASE, 
		 PAGE_SIZE, 
		 PROT_READ | PROT_WRITE,
		 MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED, 
		 -1,
		 0 );
	ASSERT ( ( bit32u_t )p == WORKSPACE_BASE );
	Mzero ( p, sizeof ( struct vm_t ) );
}

static void
check_workspace_size ( size_t offset )
{
	if ( offset + PAGE_SIZE <= MAX_SIZE_OF_WORKSPACE ) 
		return;

	Fatal_failure ( "sizeof ( struct regs_t ) + sizeof ( struct shared_info_t ) = "
			"%#x ( > %#x = MAX_SIZE_OF_WORKSPACE )\n",
			offset, MAX_SIZE_OF_WORKSPACE );
}

static bit32u_t
get_workspace_size ( struct vm_t *vm )
{
	bit32u_t s;

	ASSERT ( vm != NULL );
	
	s = ( sizeof ( struct regs_t ) + 
	      sizeof ( struct shared_info_t ) +
	      vm->num_of_pages * sizeof ( struct page_descr_t ) );

	s = BIT_ALIGN ( s, 12 ) + PAGE_SIZE;

	check_workspace_size ( s );

	return s;
}

static void
workspace_init_for_misc ( int cpuid )
{
	void *p;
	int fd;
	size_t offset;
	struct vm_t *vm = Vm_get ( );
	
	vm->num_of_pages = PMEM_SIZE / PAGE_SIZE_4K;
	offset = get_workspace_size ( vm );

	fd = open_private_file ( cpuid, REGS_FILENAME, O_RDWR );
	Ftruncate ( fd, offset );

	p = Mmap ( ( void * ) ( WORKSPACE_BASE + PAGE_SIZE ),
		   offset,
		   PROT_READ | PROT_WRITE,
		   MAP_FIXED | MAP_SHARED,
		   fd,
		   0 );
	ASSERT ( ( bit32u_t ) p == ( WORKSPACE_BASE + PAGE_SIZE ) );
	Close ( fd ); 
	Mzero ( p, offset );
}

static void
workspace_init ( int cpuid )
{
	workspace_init_for_vm ( );
	workspace_init_for_misc ( cpuid );
}

static struct pmem_t
pmem_init ( int cpuid )
{
	struct pmem_t x;

	x.ram_offset = PMEM_SIZE;
	x.offset = MAX_PMEM_SIZE;

	x.fd = open_private_file ( cpuid, PMEM_FILENAME, O_RDWR );
	Ftruncate ( x.fd, MAX_PMEM_SIZE );
	x.base = ( bit32u_t ) Mmap ( ( void * )VM_PMEM_BASE, 
				     x.offset, 
				     PROT_READ | PROT_WRITE | PROT_EXEC,
				     MAP_FIXED | MAP_SHARED,
				     x.fd, 
				     0 );
	ASSERT ( x.base == VM_PMEM_BASE );

	return x;
}

static void
init_page_descrs ( struct vm_t *vm )
{
	int i;

	ASSERT ( vm != NULL );

	for ( i = 0; i < vm->num_of_pages; i++ ) {
		PageDescr_init ( &vm->page_descrs[i], vm->cpuid );
	}
}

void
Vm_init_mem ( int cpuid )
{
	struct vm_t *vm;
	bit32u_t p;

	workspace_init ( cpuid );

	vm = Vm_get ( );
	vm->cpuid = cpuid;
	vm->pmem = pmem_init ( vm->cpuid );

	p = ( WORKSPACE_BASE + PAGE_SIZE );
	vm->regs = ( struct regs_t * )p;
	p += sizeof ( struct regs_t );


	vm->shi = ( struct shared_info_t * ) ( p );
	p += sizeof ( struct shared_info_t );

	vm->page_descrs = ( struct page_descr_t * )p;
	init_page_descrs ( vm );
	p += vm->num_of_pages * ( sizeof ( struct page_descr_t ) );

	MemMapList_init ( &vm->mem_map_list );
}

/********************************************************************************/

static inline bit32u_t
Vm_paddr_to_raddr ( bit32u_t paddr )
{
	struct vm_t *vm = Vm_get ( );
	return paddr + vm->pmem.base;
}

#define DEFINE_VM_READ_PADDR_FUNC(unit, ret_type) \
inline ret_type \
Vm_read_ ## unit ## _with_paddr ( bit32u_t paddr ) \
{ \
	return read_ ## unit ( paddr, &Vm_paddr_to_raddr ); \
}

DEFINE_VM_READ_PADDR_FUNC(byte, bit8u_t)
DEFINE_VM_READ_PADDR_FUNC(word, bit16u_t)
DEFINE_VM_READ_PADDR_FUNC(dword, bit32u_t)

#define DEFINE_VM_WRITE_PADDR_FUNC(unit, value_type) \
inline void \
Vm_write_ ## unit ## _with_paddr ( bit32u_t paddr, value_type value ) \
{ \
	return write_ ## unit ( paddr, value, &Vm_paddr_to_raddr ); \
}

DEFINE_VM_WRITE_PADDR_FUNC(byte, bit8u_t)
DEFINE_VM_WRITE_PADDR_FUNC(word, bit16u_t)
DEFINE_VM_WRITE_PADDR_FUNC(dword, bit32u_t)

/***********************************/

inline bit32u_t
Vm_try_laddr_to_paddr ( bit32u_t laddr, bool_t *is_ok )
{
	struct vm_t *vm = Vm_get ( );

	ASSERT ( vm != NULL );
	ASSERT ( is_ok != NULL );

	return try_translate_laddr_to_paddr ( vm->regs, laddr, &Vm_paddr_to_raddr, is_ok );
}

inline bit32u_t
Vm_try_laddr_to_paddr2 ( bit32u_t laddr, bool_t *is_ok, bool_t *read_write )
{
	struct vm_t *vm = Vm_get ( );

	ASSERT ( vm != NULL );
	ASSERT ( is_ok != NULL );
	ASSERT ( read_write != NULL );

	return try_translate_laddr_to_paddr2 ( vm->regs, laddr, &Vm_paddr_to_raddr, is_ok, read_write );
}

inline bit32u_t
Vm_laddr_to_paddr ( bit32u_t laddr )
{
	struct vm_t *vm = Vm_get ( );

	ASSERT ( vm != NULL );

	return translate_laddr_to_paddr ( vm->regs, laddr, &Vm_paddr_to_raddr );
}

#define DEFINE_VM_READ_LADDR_FUNC(unit, ret_type) \
inline ret_type \
Vm_read_ ## unit ## _with_laddr ( bit32u_t laddr ) \
{ \
	return read_ ## unit ( laddr, NULL ); \
}

DEFINE_VM_READ_LADDR_FUNC(byte, bit8u_t)
DEFINE_VM_READ_LADDR_FUNC(word, bit16u_t)
DEFINE_VM_READ_LADDR_FUNC(dword, bit32u_t)

#define DEFINE_VM_WRITE_LADDR_FUNC(unit, value_type) \
inline void \
Vm_write_ ## unit ## _with_laddr ( bit32u_t laddr, value_type value ) \
{ \
	return write_ ## unit ( laddr, value, NULL ); \
}

DEFINE_VM_WRITE_LADDR_FUNC(byte, bit8u_t)
DEFINE_VM_WRITE_LADDR_FUNC(word, bit16u_t)
DEFINE_VM_WRITE_LADDR_FUNC(dword, bit32u_t)

/***********************************/

static inline bit32u_t
Vm_vaddr_to_laddr ( seg_reg_index_t index, bit32u_t vaddr )
{
	struct vm_t *vm = Vm_get ( );
	bit32u_t base;

	ASSERT ( vm != NULL );
	ASSERT ( ( index >= 0 ) && ( index < NUM_OF_SEG_REGS ) );

	base = Descr_base ( &vm->regs->segs[index].cache );
	if ( base != 0 ) {
		Fatal_failure ( "Vm_vaddr_to_laddr: index=%#x, base is not zero\n", index );		
	}
	return base + vaddr;
}

static seg_reg_index_t 	trans_arg_index;

static inline bit32u_t
Vm_vaddr_to_raddr ( bit32u_t vaddr )
{
	bit32u_t laddr, raddr;

	ASSERT ( trans_arg_index != SEG_REG_NULL );

	laddr = Vm_vaddr_to_laddr ( trans_arg_index, vaddr );
	raddr = laddr; /* Translation from laddr to raddr is not necessary. */
	trans_arg_index = SEG_REG_NULL;

	return raddr;
}

#define DEFINE_VM_READ_VADDR_FUNC(unit, ret_type) \
inline ret_type \
Vm_read_ ## unit ## _with_vaddr ( seg_reg_index_t i, bit32u_t laddr ) \
{ \
	return read_ ## unit ( laddr, &Vm_vaddr_to_raddr ); \
}

DEFINE_VM_READ_VADDR_FUNC(byte, bit8u_t)
DEFINE_VM_READ_VADDR_FUNC(word, bit16u_t)
DEFINE_VM_READ_VADDR_FUNC(dword, bit32u_t)

#define DEFINE_VM_WRITE_VADDR_FUNC(unit, value_type) \
inline void \
Vm_write_ ## unit ## _with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, value_type value ) \
{ \
	trans_arg_index = i; \
	return write_ ## unit ( vaddr, value, &Vm_vaddr_to_raddr ); \
}

DEFINE_VM_WRITE_VADDR_FUNC(byte, bit8u_t)
DEFINE_VM_WRITE_VADDR_FUNC(word, bit16u_t)
DEFINE_VM_WRITE_VADDR_FUNC(dword, bit32u_t)

/***********************************/

void
Vm_pushl ( struct regs_t *regs, bit32u_t value )
{
	ASSERT ( regs != NULL );
	trans_arg_index = SEG_REG_SS;
	pushl ( regs, value, &Vm_vaddr_to_raddr );
}

void
Vm_set_seg_reg ( struct regs_t *regs, seg_reg_index_t index, struct seg_selector_t *selector )
{
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );
	Regs_set_seg_reg ( regs, index, selector, NULL );
}

void
Vm_set_seg_reg2 ( struct regs_t *regs, seg_reg_index_t index, bit16u_t val )
{
	ASSERT ( regs != NULL );
	Regs_set_seg_reg2 ( regs, index, val, NULL );
}

struct pdir_entry_t
Vm_lookup_page_directory ( const struct regs_t *regs, int index )
{
	ASSERT ( regs != NULL );
	return lookup_page_directory ( regs, index, &Vm_paddr_to_raddr );
}

struct ptbl_entry_t
Vm_lookup_page_table ( const struct pdir_entry_t *pde, int index )
{
	ASSERT ( pde != NULL );
	return lookup_page_table ( pde, index, &Vm_paddr_to_raddr );
}

bool_t
Vm_check_mem_access ( const struct regs_t *regs, bit32u_t laddr )
{
	ASSERT ( regs != NULL );
	return check_mem_access ( regs, laddr, &Vm_paddr_to_raddr );
}

/********************************************************************************/

static void
print_prot ( FILE *stream, int prot )
{
	Print ( stream, "{ " );

	if ( prot & PROT_READ )
		Print ( stream, " ( Read )" );

	if ( prot & PROT_WRITE ) 
		Print ( stream, " ( Write )" );

	if ( prot & PROT_EXEC ) 
		Print ( stream, " ( Exec )" );

	Print ( stream, "}\n" );
}

#ifdef ENABLE_MP

static int
get_page_prot_sub ( struct vm_t *vm, bit32u_t paddr, int prot )
{
	struct page_descr_t *pdescr;
   
	pdescr = get_pdescr_by_paddr ( vm, paddr );
   
	switch ( pdescr->state ) {
	case PAGE_STATE_INVALID:            prot = PROT_NONE; break;
	case PAGE_STATE_READ_ONLY_SHARED:   prot &= ~PROT_WRITE; break;
	case PAGE_STATE_EXCLUSIVELY_SHARED: break;
	default:              		    Match_failure ( "get_page_prot: eip=%#x, paddr=%#x, state=%d\n", 
							    vm->regs->user.eip, pdescr, pdescr->state );
	}
	return prot;
}

#else /* !ENABLE_MP */

static int
get_page_prot_sub ( struct vm_t *vm, bit32u_t paddr, int prot )
{
	return prot;
}

#endif /* ENABLE_MP */

static int
get_page_prot ( struct vm_t *vm, bit32u_t paddr, bool_t read_write )
{
	int prot;

	ASSERT ( vm != NULL );
   
	if ( paddr >= vm->pmem.ram_offset )
		return PROT_NONE; /* Prohibit access to the non-RAM region */

	prot = ( PROT_READ | PROT_EXEC );
	if  ( read_write ) 
		prot |= PROT_WRITE;

	if ( is_hardware_reserved_region ( paddr ) ) {
		/* disable write to the hardware reserved region */
		prot &= ~PROT_WRITE;
		return prot;
	} 

	prot = get_page_prot_sub ( vm, paddr, prot );
	return prot;
}

static void
add_laddr_to_page_descr ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr, bool_t read_write )
{
	struct page_descr_t *pdescr;

	ASSERT ( vm != NULL );
 
	if ( paddr >= vm->pmem.ram_offset ) {
		return;
	}
	
	pdescr = get_pdescr_by_paddr ( vm, paddr );
	PageDescr_add_laddr ( pdescr, laddr, read_write );
}

static void
remove_laddr_of_page_descr ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr )
{
	struct page_descr_t *pdescr;

	ASSERT ( vm != NULL );
   	
	if ( paddr >= vm->pmem.ram_offset ) {
		return;
	}

	pdescr = get_pdescr_by_paddr ( vm, paddr );	 
	PageDescr_remove_laddr ( pdescr, laddr );
}

static void
Vm_mmap ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr, size_t size, bool_t read_write )
{
	bit32u_t i;

	ASSERT ( vm != NULL );

	for ( i = 0; i < size; i += PAGE_SIZE_4K ) {
		bit32u_t laddr2 = laddr + i;
		bit32u_t paddr2 = paddr + i;
		int prot;

		if ( paddr2 >= vm->pmem.offset ) { 
//			Print ( stderr, "map: offset over: %#x, %#x\n",	laddr + i , paddr + i );
			continue;
		}

		prot = get_page_prot ( vm, paddr2, read_write );
		Mmap ( ( void * )laddr2, 
		       PAGE_SIZE_4K,
		       prot, 
		       MAP_FIXED | MAP_SHARED, 
		       vm->pmem.fd, 
		       paddr2 );

		add_laddr_to_page_descr ( vm, laddr2, paddr2, read_write );
		
		{
			bit32u_t p;
			bool_t b;
			p = MemMapList_add ( &vm->mem_map_list, laddr2 , paddr2, read_write, &b );
			if ( b ) {
				Print ( stderr, "dup add: laddr = %#x,  paddr = %#x", laddr2 , p  );
				remove_laddr_of_page_descr ( vm, laddr2 , p );
			}
		}
	}
}

static void
Vm_unmap ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr, size_t size )
{
	int i;

	ASSERT ( vm != NULL );

	Munmap ( ( void * )laddr, size );

	for ( i = 0; i < size; i += PAGE_SIZE_4K ) {
		remove_laddr_of_page_descr ( vm, laddr + i , paddr + i );
	}
}

static void
Vm_map_4k_page ( struct vm_t *vm, bit32u_t laddr, struct ptbl_entry_t *pte )
{
	bit32u_t laddr2, paddr;

	ASSERT ( vm != NULL );
	ASSERT ( pte != NULL );

	laddr2 = BIT_ALIGN ( laddr, 12 );
	paddr = pte->base << 12;

	/* [TODO] */
	PtblEntry_set_accessed_flag ( pte, &Vm_paddr_to_raddr );
	if ( pte->read_write ) 
		PtblEntry_set_dirty_flag ( pte, &Vm_paddr_to_raddr ); 

	DPRINT ( "mmap: %#x --> %#x\n", laddr2, paddr );

	Vm_mmap ( vm, laddr2, paddr, PAGE_SIZE_4K, pte->read_write );
}

static void
Vm_map_4m_page ( struct vm_t *vm, bit32u_t laddr, struct pdir_entry_t *pde )
{
	bit32u_t laddr2, paddr;

	ASSERT ( vm != NULL );
	ASSERT ( pde != NULL );

	laddr2 = BIT_ALIGN ( laddr, 22 );
	paddr = ( SUB_BIT ( pde->base.page, 0, 10 ) ) << 22;

	/* [TODO] */
	PdirEntry_set_accessed_flag ( pde, &Vm_paddr_to_raddr );
	if ( pde->read_write ) {
		PdirEntry_set_dirty_flag ( pde, &Vm_paddr_to_raddr );
	}

	DPRINT ( "mmap: %#x --> %#x\n", laddr2, paddr );

	Vm_mmap ( vm, laddr2, paddr, PAGE_SIZE_4M, pde->read_write );
}

void
Vm_map_page ( struct vm_t *vm, bit32u_t laddr )
{
	struct linear_addr_t p;
	struct pdir_entry_t pde;

	ASSERT ( vm != NULL );
  
	p = LinearAddr_of_bit32u ( laddr );
	pde = Vm_lookup_page_directory ( vm->regs, p.dir );
	assert ( pde.present );
  
	if ( pde.page_size ) {
		ASSERT ( vm->regs->sys.cr4.page_size_extension );
		Vm_map_4m_page ( vm, laddr, &pde );
	} else {
		struct ptbl_entry_t pte;

		pte = Vm_lookup_page_table ( &pde, p.table );
		assert ( pte.present );
		Vm_map_4k_page ( vm, laddr, &pte );
	}
}

/* 
 * This function maps 
 *  [VM_PMEM_BASE, ..., VM_PMEM_BASE + vm->pmem.offset]
 *     to
 *  [0x00000000, ..., vm->pmem.offset]. 
 * [Note] Paging is not enabled when this function is called.
 */
void
Vm_init_page_mapping ( struct vm_t *vm )
{
	int prot;
	int i;

	ASSERT ( vm != NULL );
   
	prot = is_bootstrap_proc ( vm ) 
		? ( PROT_READ | PROT_WRITE | PROT_EXEC )
		: PROT_NONE;

	Mmap ( ( void * )0x00000000, 
	       vm->pmem.offset, 
	       prot, 
	       MAP_FIXED | MAP_SHARED,
	       vm->pmem.fd, 
	       0 );

	for ( i = 0; i < vm->pmem.offset; i += PAGE_SIZE_4K ) {
		const bool_t read_write = TRUE;
		bool_t b;

		add_laddr_to_page_descr ( vm, i, i, read_write );
		MemMapList_add ( &vm->mem_map_list, i, i, read_write, &b );
	}
}

static void
Vm_unmap_initial_page_mapping ( struct vm_t *vm )
{
	int i;

	ASSERT ( vm != NULL );

	Munmap ( ( void * )0x00000000, vm->pmem.offset );

	for ( i = 0; i < vm->pmem.offset; i += PAGE_SIZE_4K ) {
		remove_laddr_of_page_descr ( vm, i, i );  
	}

	MemMapList_destroy ( &vm->mem_map_list );
}

static void
Vm_map_all_pages_sub ( struct vm_t *vm, struct linear_addr_t addr, struct pdir_entry_t pde )
{
	ASSERT ( vm != NULL );

	if ( ! pde.present ) {
		return;
	}

	if ( pde.page_size ) {
		Vm_map_4m_page ( vm, addr.dir << 22, &pde );
		return;
	}

	for ( addr.table = 0; addr.table < NUM_OF_PTBL_ENTRIES; addr.table++ ) {
		struct ptbl_entry_t pte;
	 
		pte = Vm_lookup_page_table ( &pde, addr.table );
		if ( ! pte.present ) {
			continue;
		}

		Vm_map_4k_page ( vm, LinearAddr_to_bit32u ( addr ), &pte );
	}
}

static void
Vm_map_all_pages ( struct vm_t *vm )
{
	struct linear_addr_t addr;

	ASSERT ( vm != NULL );

	addr.offset = 0;

	for ( addr.dir = 0; addr.dir < NUM_OF_PDIR_ENTRIES; addr.dir++ ) {
		struct pdir_entry_t pde;

		pde = Vm_lookup_page_directory ( vm->regs, addr.dir );
		Vm_map_all_pages_sub ( vm, addr, pde );
	}
}
 
static bool_t
is_obsolete ( struct vm_t *vm, struct mem_map_entry_t *x )
{
	bit32u_t paddr;
	bool_t is_ok, read_write;

	paddr = Vm_try_laddr_to_paddr2 ( x->laddr, &is_ok, &read_write );
	
	if ( ! is_ok ) {
		return TRUE;
	}

	return ( ( paddr != x->paddr ) || ( read_write != x->read_write ) );
}

/* Unmap a collection of the page mappings that do not exist in the page directory/table. */
static void
Vm_unmap_obsolete ( struct vm_t *vm )
{
	struct mem_map_entry_t **pp;

	ASSERT ( vm != NULL );

	pp = &vm->mem_map_list.head;

	while ( *pp != NULL ) {
		struct mem_map_entry_t *p = *pp;

		if ( is_obsolete ( vm, p ) ) {
			struct mem_map_entry_t *next = p->next;

			Vm_unmap ( vm, p->laddr, p->paddr, PAGE_SIZE_4K );
			MemMapEntry_destroy ( p );
			*pp = next;
		} else {
			pp = &p->next;
		}
	}
}

void
Vm_unmap_all ( struct vm_t *vm )
{
	struct mem_map_entry_t **pp;

	ASSERT ( vm != NULL );

	pp = &vm->mem_map_list.head;

	while ( *pp != NULL ) {
		struct mem_map_entry_t *p = *pp;
		struct mem_map_entry_t *next = p->next;

		Vm_unmap ( vm, p->laddr, p->paddr, PAGE_SIZE_4K );
		MemMapEntry_destroy ( p );
		*pp = next;
	}	
}

void
Vm_print_page_directory ( FILE *stream, const struct regs_t *regs )
{
	ASSERT ( stream != NULL );
	ASSERT ( regs != NULL );

	print_page_directory ( stream, regs, &Vm_paddr_to_raddr );
}

void
set_cr0 ( struct vm_t *vm, bit32u_t val )
{
	ASSERT ( vm != NULL );

	if ( vm->regs->sys.cr0.paging ) {
		/* Pagin has already been enabled. */
		vm->regs->sys.cr0 = Cr0_of_bit32u ( val );
		return;
	}

	vm->regs->sys.cr0 = Cr0_of_bit32u ( val );
	ASSERT ( vm->regs->sys.cr0.paging );

	Vm_unmap_initial_page_mapping ( vm );
	Vm_map_all_pages ( vm );
}

void
set_cr3 ( struct vm_t *vm, bit32u_t val )
{
	ASSERT ( vm != NULL );

	DPRINT ( " ( vm )\t" "set_cr3: val=%#x\n", val );

	if ( ! vm->regs->sys.cr0.paging ) { 
		vm->regs->sys.cr3 = Cr3_of_bit32u ( val ); 
		return; 
	}

	vm->regs->sys.cr3 = Cr3_of_bit32u ( val ); 
	Vm_unmap_obsolete ( vm );
}

void
set_cr4 ( struct vm_t *vm, bit32u_t val )
{
	ASSERT ( vm != NULL );

	DPRINT ( " ( vm )\t" "set_cr4: val=%#x\n", val );

	if ( ! vm->regs->sys.cr0.paging ) { 
		vm->regs->sys.cr4 = Cr4_of_bit32u ( val ); 
		return; 
	}

	vm->regs->sys.cr4 = Cr4_of_bit32u ( val ); 
	Vm_unmap_obsolete ( vm );
}

void
invalidate_tlb ( struct vm_t *vm )
{
	ASSERT ( vm != NULL );

	DPRINT ( " ( vm )\t" "invalidate_tlb\n" );

	if ( ! vm->regs->sys.cr0.paging ) 
		return;
   
	Vm_unmap_obsolete ( vm );
}

#ifdef ENABLE_MP

static void
__change_page_prot ( struct vm_t *vm, int page_no, struct page_descr_t *pdescr, int i )
{
	bit32u_t laddr, paddr;
	bool_t read_write;
	int prot;
	
	laddr = pdescr->laddrs[i];
	paddr = page_no_to_paddr ( page_no );
	read_write = pdescr->read_write[i];
	prot = get_page_prot ( vm, paddr, read_write );
	

	Munmap ( ( void * )laddr, PAGE_SIZE_4K );
	
	Mmap ( ( void * )laddr, 
	       PAGE_SIZE_4K,
	       prot, 
	       MAP_FIXED | MAP_SHARED, 
	       vm->pmem.fd, 
	       paddr );

#if 0
	Print ( stderr,
		" ( vm )\t" "change_page_prot: laddr=%#x, paddr=%#x: ",
		laddr, paddr );
	print_prot ( stderr, prot ); /* [DEBUG] */
#endif
}

void
change_page_prot ( struct vm_t *vm, int page_no )
{
	struct page_descr_t *pdescr;
	int i;

	ASSERT ( vm != NULL );
	ASSERT ( page_no < vm->num_of_pages );

	DPRINT ( " ( vm )\t" "change page prot: page_no=%#x\n", page_no );

	pdescr = get_pdescr_by_page_no ( vm, page_no );

	for ( i = 0; i < pdescr->num_of_laddrs; i++ ) {
		__change_page_prot ( vm, page_no, pdescr, i );
	}
}

#else /* !ENABLE_MP */

void
change_page_prot ( struct vm_t *vm, int page_no )
{
	Fatal_failure ( "The change_page_prot() function must not be called.\n" );
}

#endif /* ENABLE_MP */

void
Vm_print_page_prot ( FILE *stream, struct vm_t *vm, bit32u_t laddr )
{
	struct linear_addr_t p;
	struct pdir_entry_t pde;
	int prot;
	bit32u_t paddr;
	bool_t read_write;

	ASSERT ( vm != NULL );
  
	p = LinearAddr_of_bit32u ( laddr );
	pde = Vm_lookup_page_directory ( vm->regs, p.dir );
	assert ( pde.present );
   
	if ( pde.page_size ) {
		ASSERT ( vm->regs->sys.cr4.page_size_extension );
		paddr = SUB_BIT ( pde.base.page, 0, 10 ) << 22;
		read_write = pde.read_write;
	} else {
		struct ptbl_entry_t pte;
		pte = Vm_lookup_page_table ( &pde, p.table );
		assert ( pte.present );

		paddr = pte.base << 12;
		read_write = pte.read_write;
	}

	prot = get_page_prot ( vm, paddr, read_write );
	print_prot ( stream, prot );
}

bool_t 
try_add_new_mem_map ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr, bool_t read_write )
{
	struct page_descr_t *pdescr;

	ASSERT ( vm != NULL );
	assert ( paddr < vm->pmem.ram_offset );

	pdescr = get_pdescr_by_paddr ( vm, paddr );
	if ( PageDescr_has_same_laddr ( pdescr, laddr, read_write ) ) {
		return FALSE;
	}

	Vm_map_page ( vm, laddr );

	return TRUE;
}


#ifdef ENABLE_MP

static void
allow_access_to_descr_table ( bit32u_t base, bit16u_t limit )
{
	bit32u_t i, pages[2];
	struct vm_t *vm = Vm_get ( );

	ASSERT ( vm != NULL );

	pages[0] = base >> 12;
	pages[1] = ( base + SIZE_OF_DESCR * limit ) >> 12;

	for ( i = pages[0]; i <= pages[1]; i++ ) {
		bool_t is_ok;
		bit32u_t laddr = i << 12;
		bit32u_t paddr = Vm_try_laddr_to_paddr ( laddr, &is_ok );

		if ( ! is_ok )
			continue;

		Mmap ( ( void * ) laddr, 
		       PAGE_SIZE_4K,
		       PROT_READ | PROT_WRITE | PROT_EXEC, 
		       MAP_FIXED | MAP_SHARED, 
		       vm->pmem.fd, 
		       paddr );
	}
}

/* Allow access to the global descriptor table and the interrupt descriptor table. */
void
allow_access_to_descr_tables ( struct regs_t *regs )
{
	allow_access_to_descr_table ( regs->sys.gdtr.base, regs->sys.gdtr.limit );
	allow_access_to_descr_table ( regs->sys.idtr.base, regs->sys.idtr.limit );
#if 0
	check_permission_of_descr_table ( Descr_base ( &regs->sys.ldtr.cache ),
					  Descr_limit ( &regs->sys.ldtr.cache ) );
	check_permission_of_descr_table ( Descr_base ( &regs->sys.tr.cache ),
					  Descr_limit ( &regs->sys.tr.cache ) );
#endif	
}

static void
restore_permission_of_descr_table ( bit32u_t base, bit16u_t limit )
{
	bit32u_t i, pages[2];
	struct vm_t *vm = Vm_get ( );

	ASSERT ( vm != NULL );

	pages[0] = base >> 12;
	pages[1] = ( base + SIZE_OF_DESCR * limit ) >> 12;

	for ( i = pages[0]; i <= pages[1]; i++ ) {
		bit32u_t laddr = i << 12;
		struct mem_map_entry_t *p;
		struct page_descr_t *pdescr;
		bool_t read_write;

		p = MemMapList_find ( &vm->mem_map_list, laddr );
		
		if ( p == NULL ) 
			continue;

		pdescr = get_pdescr_by_paddr ( vm, p->paddr );
		read_write = PageDescr_get_read_write ( pdescr, laddr );

		Mmap ( ( void * ) laddr, 
		       PAGE_SIZE_4K,
		       get_page_prot ( vm, p->paddr, read_write ), 
		       MAP_FIXED | MAP_SHARED, 
		       vm->pmem.fd, 
		       p->paddr );
	}
}

void
restore_permission_of_descr_tables ( struct regs_t *regs )
{
	restore_permission_of_descr_table ( regs->sys.gdtr.base, regs->sys.gdtr.limit );
	restore_permission_of_descr_table ( regs->sys.idtr.base, regs->sys.idtr.limit );
}

#else /* ! ENABLE_MP */

void
allow_access_to_descr_tables ( struct regs_t *regs )
{
	/* do nothing */
}

void
restore_permission_of_descr_tables ( struct regs_t *regs )
{
        /* do nothing */
}

#endif /* ENABLE_MP */
