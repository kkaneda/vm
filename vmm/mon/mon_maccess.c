#include "vmm/mon/mon.h"
#include <fcntl.h>
#include <sys/mman.h>


static struct mon_t *static_mon = NULL;

static struct pmem_t 
pmem_init ( int cpuid )
{
	struct pmem_t x;

	x.fd = open_private_file ( cpuid, PMEM_FILENAME, O_RDWR );
	x.ram_offset = PMEM_SIZE;
	x.offset = MAX_PMEM_SIZE;

	x.base = ( bit32u_t ) Mmap ( NULL,
				     x.offset, 
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED, 
				     x.fd, 
				     0 );
	return x;
}

static bit32u_t
get_workspace_size ( struct mon_t *mon )
{
	bit32u_t s;

	ASSERT ( mon != NULL );
	
	s = ( sizeof ( struct regs_t ) + 
	      sizeof ( struct shared_info_t ) +
	      mon->num_of_pages * sizeof ( struct page_descr_t ) );

	s = BIT_ALIGN ( s, 12 ) + PAGE_SIZE;

	return s;
}

static void
workspace_init ( struct mon_t *mon )
{
	int fd;
	size_t offset;
	bit32u_t p;

	mon->num_of_pages = PMEM_SIZE / PAGE_SIZE_4K;

	offset = get_workspace_size ( mon );

	fd = open_private_file ( mon->cpuid, REGS_FILENAME, O_RDWR );
	p = ( bit32u_t ) Mmap ( NULL,
				offset,
				PROT_READ | PROT_WRITE,
				MAP_SHARED,
				fd,
				0 );
	Close ( fd );
     
	mon->regs = ( struct regs_t * )p;
	p += sizeof ( struct regs_t );

	mon->shi = ( struct shared_info_t * )p;
	p += sizeof ( struct shared_info_t );

	mon->page_descrs = ( struct page_descr_t * )p;
}

void
Monitor_init_mem ( struct mon_t *mon )
{
	static_mon = mon;

	mon->pmem = pmem_init ( mon->cpuid );
	workspace_init ( mon );
}

inline bit32u_t
Monitor_paddr_to_raddr ( bit32u_t paddr )
{
	struct mon_t *mon = static_mon;
	ASSERT ( mon != NULL );

	return paddr + mon->pmem.base;
}

#ifdef ENABLE_MP

static void
check_read ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
	int page_no;
	struct page_descr_t *pdescr;

	ASSERT ( mon != NULL );
	
	if ( paddr >= mon->pmem.ram_offset )
		return;

	page_no = paddr_to_page_no ( paddr );
	pdescr = &( mon->page_descrs[page_no] );
	
	if ( pdescr->state == PAGE_STATE_INVALID ) {
		Print ( stderr,
			"Failed on check_read: paddr=%#x, page_no=%#x, len=%#x, eip=%#x\n",
			paddr,
			paddr_to_page_no ( paddr ),
			len,
			mon->regs->user.eip );
		assert ( 0 );
	}
}

#else /* ! ENABLE_MP */

static void
check_read ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
}

#endif /* ENABLE_MP */

static bit32u_t 
read_from_non_ram_region ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
	ASSERT ( mon != NULL );

	if ( LocalApic_is_selected ( mon->local_apic, paddr, len ) )
		return LocalApic_read ( mon->local_apic, paddr, len );

	if ( IoApic_is_selected ( mon->io_apic, paddr, len ) )
		return IoApic_read ( mon->io_apic, paddr, len );

	Fatal_failure ( "read_from_non_ram_region: paddr = %#lx\n", paddr );
	return 0;
}

#define DEFINE_MONITOR_READ_PADDR_FUNC(unit, ret_type, len) \
inline ret_type \
Monitor_read_ ## unit ## _with_paddr ( bit32u_t paddr ) \
{ \
	struct mon_t *mon = static_mon; \
\
	ASSERT ( mon != NULL ); \
\
	check_read ( mon, paddr, len ); \
\
	return ( ( paddr < mon->pmem.ram_offset ) ? \
		 read_ ## unit ( paddr, &Monitor_paddr_to_raddr ) : \
		 read_from_non_ram_region ( mon, paddr, len ) ); \
}

DEFINE_MONITOR_READ_PADDR_FUNC(byte, bit8u_t, 1)
DEFINE_MONITOR_READ_PADDR_FUNC(word, bit16u_t, 2)
DEFINE_MONITOR_READ_PADDR_FUNC(dword, bit32u_t, 4)

#ifdef ENABLE_MP

static inline void
check_write ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
	int page_no;
	struct page_descr_t *pdescr;

	ASSERT ( mon != NULL );
	
	page_no = paddr_to_page_no ( paddr );
	pdescr = &( mon->page_descrs[page_no] );

	if ( pdescr->state != PAGE_STATE_EXCLUSIVELY_SHARED ) {
		Fatal_failure ( "check_write: paddr = %#x, len = %#x, eip=%#x\n",
				paddr, len, mon->regs->user.eip );
	}
}

#else /* ! ENABLE_MP */

static inline void
check_write ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
}	

#endif /* ENABLE_MP */

static inline void
write_to_hardware_reserved_region ( struct mon_t *mon, bit32u_t paddr, bit32u_t value, size_t len )
{
	ASSERT ( mon != NULL );

	switch ( len ) {
	case 1:  write_byte ( paddr, value, &Monitor_paddr_to_raddr ); break;
	case 2:  write_word ( paddr, value, &Monitor_paddr_to_raddr ); break;
	case 4:  write_dword ( paddr, value, &Monitor_paddr_to_raddr ); break;
	default: Match_failure ( "write_to_hardware_reserved_region\n" );
	}

     	if ( is_vga_region ( paddr ) )
		write_to_vram ( mon, paddr, len );
}

static inline void
write_to_non_ram_region ( struct mon_t *mon, bit32u_t paddr, bit32u_t value, size_t len )
{
	ASSERT ( mon != NULL );
        
	if ( LocalApic_is_selected ( mon->local_apic, paddr, len ) ) {
		LocalApic_write ( mon->local_apic, paddr, value, len );
		return;
	}

	if ( IoApic_is_selected ( mon->io_apic, paddr, len ) ) {
		IoApic_write ( mon->io_apic, paddr, value, len );
		return;
	}

	Fatal_failure ( "write_to_non_ram_region: paddr = %#lx\n", paddr );
}

#define DEFINE_MONITOR_WRITE_PADDR_FUNC(unit, value_type, len) \
inline void \
Monitor_write_ ## unit ## _with_paddr ( bit32u_t paddr, value_type value ) \
{ \
	struct mon_t *mon = static_mon; \
\
	ASSERT ( mon != NULL ); \
\
	if ( is_hardware_reserved_region ( paddr ) ) { \
		write_to_hardware_reserved_region ( mon, paddr, value, len ); \
		return; \
	} \
\
	if ( paddr < mon->pmem.ram_offset ) { \
		check_write ( mon, paddr, len ); \
		write_ ## unit ( paddr, value, &Monitor_paddr_to_raddr ); \
		return; \
	} \
\
	write_to_non_ram_region ( mon, paddr, value, len ); \
}

DEFINE_MONITOR_WRITE_PADDR_FUNC(byte, bit8u_t, 1)
DEFINE_MONITOR_WRITE_PADDR_FUNC(word, bit16u_t, 2)
DEFINE_MONITOR_WRITE_PADDR_FUNC(dword, bit32u_t, 4)

/***********************************/

inline bit32u_t
Monitor_laddr_to_paddr ( bit32u_t laddr )
{
	struct mon_t *mon = static_mon;

	ASSERT ( mon != NULL );
	return translate_laddr_to_paddr ( mon->regs, laddr, &Monitor_paddr_to_raddr );
}

inline bit32u_t
Monitor_laddr_to_raddr ( bit32u_t laddr )
{
	bit32u_t paddr = Monitor_laddr_to_paddr ( laddr );
	return Monitor_paddr_to_raddr ( paddr );
}

inline bit8u_t
Monitor_try_read_byte_with_laddr ( bit32u_t laddr, bool_t *is_ok )
{
	struct mon_t *mon = static_mon;
	bit32u_t paddr;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );

	*is_ok = Monitor_check_mem_access_with_laddr ( mon->regs, laddr );
	if  ( *is_ok == FALSE ) 
		return 0;

	paddr = Monitor_laddr_to_paddr ( laddr );
	return Monitor_read_byte_with_paddr ( paddr );
}

inline bit32u_t
Monitor_try_read_dword_with_laddr ( bit32u_t laddr, bool_t *is_ok )
{
	struct mon_t *mon = static_mon;
	bit32u_t paddr;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );

	*is_ok = Monitor_check_mem_access_with_laddr ( mon->regs, laddr );
	if  ( *is_ok == FALSE ) 
		return 0;

	paddr = Monitor_laddr_to_paddr ( laddr );
	return Monitor_read_dword_with_paddr ( paddr );
}


static inline bool_t
is_access_to_same_page ( bit32u_t laddr, size_t len )
{
	return ( laddr >> 12 ) == ( ( laddr + len - 1 ) >> 12 );
}

#define DEFINE_MONITOR_READ_LADDR_FUNC(unit, ret_type, len)  \
inline ret_type \
Monitor_read_ ## unit ## _with_laddr ( bit32u_t laddr ) \
{ \
        bit32u_t paddr; \
	ret_type v[2]; \
	size_t l[2]; \
\
        if ( is_access_to_same_page ( laddr, len ) ) { \
		paddr = Monitor_laddr_to_paddr ( laddr ); \
		return Monitor_read_ ## unit ## _with_paddr ( paddr ); \
	} \
        /* access to multiple pages */ \
	l[1] = SUB_BIT ( laddr + len, 0, 12 ); \
	l[0] = len - l[1]; \
\
	paddr = Monitor_laddr_to_paddr ( laddr ); \
	v[0] = Monitor_read_ ## unit ## _with_paddr ( paddr ); \
	v[0] = SUB_BIT ( v[0], 0, (l[0]*8) ); \
\
	paddr = Monitor_laddr_to_paddr ( laddr + l[0] ); \
	v[1] = Monitor_read_ ## unit ## _with_paddr ( paddr ); \
	v[1] = SUB_BIT ( v[1], 0, (l[1]*8) ); \
\
	return v[0] | ( v[1] << (l[0]*8) ); \
}

DEFINE_MONITOR_READ_LADDR_FUNC(byte, bit8u_t, 1)
DEFINE_MONITOR_READ_LADDR_FUNC(word, bit16u_t, 2)
DEFINE_MONITOR_READ_LADDR_FUNC(dword, bit32u_t, 4)

#define DEFINE_MONITOR_WRITE_LADDR_FUNC(unit, value_type, len) \
inline void \
Monitor_write_ ## unit ## _with_laddr ( bit32u_t laddr, value_type value ) \
{ \
        bit32u_t paddr; \
\
        if ( is_access_to_same_page ( laddr, len ) ) { \
		paddr = Monitor_laddr_to_paddr ( laddr ); \
		Monitor_write_## unit ## _with_paddr ( paddr, value ); \
		return; \
	} \
\
	assert ( 0 ); \
}

DEFINE_MONITOR_WRITE_LADDR_FUNC(byte, bit8u_t, 1)
DEFINE_MONITOR_WRITE_LADDR_FUNC(word, bit16u_t, 2)
DEFINE_MONITOR_WRITE_LADDR_FUNC(dword, bit32u_t, 4)

/***********************************/

inline bit32u_t
Monitor_vaddr_to_laddr ( seg_reg_index_t index, bit32u_t vaddr )
{
	struct mon_t *mon = static_mon;     
	bit32u_t base;

	ASSERT ( mon != NULL );
	ASSERT ( ( index >= 0 ) && ( index < NUM_OF_SEG_REGS ) );

	base = Descr_base ( &mon->regs->segs[index].cache );
	if ( base != 0 ) {
		//Fatal_failure ( "Monitor_vaddr_to_laddr: index=%#x, base is not zero\n", index );
		Warning ( "Monitor_vaddr_to_laddr: eip=%#x, index=%#x, base is not zero\n", mon->regs->user.eip, index );
	}
	return base + vaddr;
}

inline bit32u_t
Monitor_try_vaddr_to_paddr ( seg_reg_index_t index, bit32u_t vaddr, bool_t *is_ok )
{
	struct mon_t *mon = static_mon;     
	bit32u_t laddr;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );

	laddr = Monitor_vaddr_to_laddr ( index, vaddr );
	return try_translate_laddr_to_paddr ( mon->regs, laddr, &Monitor_paddr_to_raddr, is_ok );
}

inline bit32u_t
Monitor_try_vaddr_to_paddr2 ( seg_reg_index_t index, bit32u_t vaddr, bool_t *is_ok, bool_t *read_write )
{
	struct mon_t *mon = static_mon;     
	bit32u_t laddr;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );
	ASSERT ( read_write != NULL );

	laddr = Monitor_vaddr_to_laddr ( index, vaddr );
	return try_translate_laddr_to_paddr2 ( mon->regs, laddr, &Monitor_paddr_to_raddr, is_ok, read_write );
}


inline bit32u_t
Monitor_vaddr_to_paddr ( seg_reg_index_t index, bit32u_t vaddr )
{
	struct mon_t *mon = static_mon;     
	bit32u_t laddr;

	ASSERT ( mon != NULL );

	laddr = Monitor_vaddr_to_laddr ( index, vaddr );
	return translate_laddr_to_paddr ( mon->regs, laddr, &Monitor_paddr_to_raddr );
}

static seg_reg_index_t 	trans_arg_index;

static inline bit32u_t
Monitor_vaddr_to_raddr_sub ( bit32u_t vaddr )
{
	bit32u_t laddr, raddr;

	ASSERT ( trans_arg_index != SEG_REG_NULL );

	laddr = Monitor_vaddr_to_laddr ( trans_arg_index, vaddr );
	raddr = Monitor_laddr_to_raddr ( laddr );
	trans_arg_index = SEG_REG_NULL;
	return raddr;
}

inline bit32u_t
Monitor_vaddr_to_raddr ( seg_reg_index_t i, bit32u_t vaddr )
{
	bit32u_t laddr, raddr;

	laddr = Monitor_vaddr_to_laddr ( i, vaddr );
	raddr = Monitor_laddr_to_raddr ( laddr );
	return raddr;     
}

inline bit8u_t
Monitor_try_read_byte_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, bool_t *is_ok )
{
	bit32u_t laddr;
	ASSERT ( is_ok != NULL );

	laddr = Monitor_vaddr_to_laddr ( i, vaddr );
	return Monitor_try_read_byte_with_laddr ( laddr, is_ok );
}

inline bit32u_t
Monitor_try_read_dword_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, bool_t *is_ok )
{
	bit32u_t laddr;
	ASSERT ( is_ok != NULL );

	laddr = Monitor_vaddr_to_laddr ( i, vaddr );
	return Monitor_try_read_dword_with_laddr ( laddr, is_ok );
}


#define DEFINE_MONITOR_READ_VADDR_FUNC(unit, ret_type) \
inline ret_type \
Monitor_read_ ## unit ## _with_vaddr ( seg_reg_index_t i, bit32u_t vaddr ) \
{ \
	bit32u_t laddr = Monitor_vaddr_to_laddr ( i, vaddr ); \
	return Monitor_read_ ## unit ## _with_laddr ( laddr ); \
}

DEFINE_MONITOR_READ_VADDR_FUNC(byte, bit8u_t)
DEFINE_MONITOR_READ_VADDR_FUNC(word, bit16u_t)
DEFINE_MONITOR_READ_VADDR_FUNC(dword, bit32u_t)

#define DEFINE_MONITOR_WRITE_VADDR_FUNC(unit, value_type) \
inline void \
Monitor_write_ ## unit ## _with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, value_type value ) \
{ \
	bit32u_t laddr = Monitor_vaddr_to_laddr ( i, vaddr ); \
	Monitor_write_## unit ## _with_laddr ( laddr, value ); \
}

DEFINE_MONITOR_WRITE_VADDR_FUNC(byte, bit8u_t)
DEFINE_MONITOR_WRITE_VADDR_FUNC(word, bit16u_t)
DEFINE_MONITOR_WRITE_VADDR_FUNC(dword, bit32u_t)

inline 
bit32u_t
Monitor_try_read_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, size_t len, bool_t *is_ok )
{
	struct mon_t *mon = static_mon;
	bit32u_t retval;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );

	*is_ok = Monitor_check_mem_access_with_vaddr ( mon->regs, i, vaddr );
	if ( *is_ok == FALSE )
		return 0;

	retval = Monitor_read_with_vaddr ( i, vaddr, len );
	*is_ok = TRUE;
	return retval;
}

bit32u_t 
Monitor_read_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, size_t len )
{
	bit32u_t ret = 0;

	switch ( len ) {
	case 1:  ret = Monitor_read_byte_with_vaddr ( i, vaddr ); break;
	case 2:  ret = Monitor_read_word_with_vaddr ( i, vaddr ); break;
	case 4:  ret = Monitor_read_dword_with_vaddr ( i, vaddr ); break;
	default: Match_failure ( "Monitor_read_with_vaddr\n" );
	}

	return ret;
}


inline bit32u_t 
Monitor_read_with_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	bit32u_t vaddr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	vaddr = instr->resolve ( instr, &mon->regs->user );
	return Monitor_read_with_vaddr ( instr->sreg_index, vaddr, len );
}

inline bit32u_t 
Monitor_read_reg_or_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	
	return ( ( instr->mod == 3 ) 
		 ? UserRegs_get2 ( &mon->regs->user, instr->rm, len )
		 : Monitor_read_with_resolve ( mon, instr, len ) );
}

inline void
Monitor_write_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, bit32u_t value, size_t len )
{
	switch ( len ) {
	case 1:  Monitor_write_byte_with_vaddr ( i, vaddr, value ); break;
	case 2:  Monitor_write_word_with_vaddr ( i, vaddr, value ); break;
	case 4:  Monitor_write_dword_with_vaddr ( i, vaddr, value ); break;
	default: Match_failure ( "Monitor_write_with_vaddr\n" );
	}
}

inline void
Monitor_try_write_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, bit32u_t value, size_t len, bool_t *is_ok )
{
	struct mon_t *mon = static_mon;

	ASSERT ( mon != NULL );
	ASSERT ( is_ok != NULL );

	*is_ok = Monitor_check_mem_access_with_vaddr ( mon->regs, i, vaddr );
	if ( *is_ok )
		Monitor_write_with_vaddr ( i, vaddr, value, len );
}

inline void
Monitor_write_with_resolve ( struct mon_t *mon, struct instruction_t *instr, bit32u_t value, size_t len )
{
	bit32u_t vaddr;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	vaddr = instr->resolve ( instr, &mon->regs->user );
	Monitor_write_with_vaddr ( instr->sreg_index, vaddr, value, len );
}

inline void
Monitor_write_reg_or_mem ( struct mon_t *mon, struct instruction_t *instr, bit32u_t value, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	if ( instr->mod == 3 ) {
		UserRegs_set2 ( &mon->regs->user, instr->rm, value, len );
	} else {
		Monitor_write_with_resolve ( mon, instr, value, len );
	}
}

void
Monitor_pushl ( struct regs_t *regs, bit32u_t value )
{
	ASSERT ( regs != NULL );

	trans_arg_index = SEG_REG_SS;
	pushl ( regs, value, &Monitor_vaddr_to_raddr_sub );
}

void
Monitor_push ( struct mon_t *mon, bit32u_t val, size_t len )
{
	ASSERT ( mon != NULL );

	/* [TODO] check the boundary of the stack */

	mon->regs->user.esp -= len;
	Monitor_write_with_vaddr ( SEG_REG_SS, mon->regs->user.esp, val, len );
}

bit32u_t
Monitor_pop ( struct mon_t *mon, size_t len )
{
	bit32u_t retval = 0;

	ASSERT ( mon != NULL );

	retval = Monitor_read_with_vaddr ( SEG_REG_SS, mon->regs->user.esp, len );
	mon->regs->user.esp += len;

	return retval;
}

void
Monitor_set_seg_reg ( struct regs_t *regs, seg_reg_index_t index, struct seg_selector_t *selector )
{
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );

	Regs_set_seg_reg ( regs, index, selector, &Monitor_laddr_to_raddr ); 
}

void
Monitor_set_seg_reg2 ( struct regs_t *regs, seg_reg_index_t index, bit16u_t val )
{
	ASSERT ( regs != NULL );

	Regs_set_seg_reg2 ( regs, index, val, &Monitor_laddr_to_raddr ); 
}

bool_t
Monitor_check_mem_access_with_laddr ( const struct regs_t *regs, bit32u_t laddr )
{
	ASSERT ( regs != NULL );
	return check_mem_access ( regs, laddr, &Monitor_paddr_to_raddr );
}

bool_t
Monitor_check_mem_access_with_vaddr ( const struct regs_t *regs, seg_reg_index_t i, bit32u_t vaddr )
{
	bit32u_t laddr = Monitor_vaddr_to_laddr ( i, vaddr );

	return Monitor_check_mem_access_with_laddr ( regs, laddr );
}

struct pdir_entry_t
Monitor_lookup_page_directory ( const struct regs_t *regs, int index )
{
	ASSERT ( regs != NULL );

	return lookup_page_directory ( regs, index, &Monitor_paddr_to_raddr );
}

struct ptbl_entry_t
Monitor_lookup_page_table ( const struct pdir_entry_t *pde, int index )
{
	ASSERT ( pde != NULL );

	return lookup_page_table ( pde, index, &Monitor_paddr_to_raddr );
}

struct descr_t
Monitor_lookup_descr_table ( const struct regs_t *regs, const struct seg_selector_t *selector )
{
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );

	return Regs_lookup_descr_table ( regs, selector, &Monitor_laddr_to_raddr );
}

void
Monitor_update_descr_table ( const struct regs_t *regs, const struct seg_selector_t *selector, const struct descr_t *descr )
{
	ASSERT ( regs != NULL );
	ASSERT ( selector != NULL );
	ASSERT ( descr != NULL );

	Regs_update_descr_table ( regs, selector, descr, &Monitor_laddr_to_raddr );
}

struct tss_t
Monitor_get_tss_of_current_task ( const struct regs_t *regs )
{
	ASSERT ( regs != NULL );

	return get_tss_of_current_task ( regs, &Monitor_laddr_to_raddr );
}

void
Monitor_print_page_directory ( FILE *stream, const struct regs_t *regs )
{
	ASSERT ( stream != NULL );
	ASSERT ( regs != NULL );

	return print_page_directory ( stream, regs, &Monitor_paddr_to_raddr );
}

#ifdef ENABLE_MP

void
Monitor_mmove ( bit32u_t paddr, void *addr, size_t len, mem_access_kind_t kind )
{
	struct mon_t *mon = static_mon;
	size_t offset = 0;
	
	while ( offset < len ) {
		bit32u_t p = paddr + offset;
		bit32u_t n;
		
		n = len - offset;
		if ( paddr_to_page_no ( p ) != paddr_to_page_no ( p + n ) ) {
			n = PAGE_SIZE_4K - SUB_BIT ( p, 0, 12 );
		}

		emulate_shared_memory_with_paddr ( mon, kind, p );
		switch ( kind ) {
		case MEM_ACCESS_READ:
			Mmove ( addr + offset, (void *)(mon->pmem.base + p ), n );
			break;
		case MEM_ACCESS_WRITE:
			Mmove ( (void *)(mon->pmem.base + p ), addr + offset, n );
			break;
		default:
			Match_failure ( "Monitor_mmove" );
		}
		offset += n;
	}
}

void
Monitor_mmove_rd ( bit32u_t paddr, void *to_addr, size_t len )
{
	Monitor_mmove ( paddr, to_addr, len, MEM_ACCESS_READ );
}

void
Monitor_mmove_wr ( bit32u_t paddr, void *from_addr, size_t len )
{
	Monitor_mmove ( paddr, from_addr, len, MEM_ACCESS_WRITE );
}

#else /* ! ENABLE_MP */

void
Monitor_mmove_rd ( bit32u_t paddr, void *to_addr, size_t len )
{
	struct mon_t *mon = static_mon;
	Mmove ( to_addr, (void *)(mon->pmem.base + paddr), len );
}

void
Monitor_mmove_wr ( bit32u_t paddr, void *from_addr, size_t len )
{
	struct mon_t *mon = static_mon;	
	Mmove ( (void *)(mon->pmem.base + paddr ), from_addr, len );
}

#endif /* ENABLE_MP */

void
mem_check_for_dma_access ( bit32u_t paddr )
{
	struct mon_t *mon = static_mon;	

	emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr );
	emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr + 4 );
}

/* [DEBUG]
void
Print_eip ( void )
{
	struct mon_t *mon = static_mon;	
	fprintf ( stderr, "Eip = %#x\n", mon->regs->user.eip );
}
*/
