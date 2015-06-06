#ifndef _VMM_IA32_PAGING_H
#define _VMM_IA32_PAGNIG_H

#include "vmm/ia32/paging_common.h"


struct linear_addr_t LinearAddr_of_bit32u ( bit32u_t laddr );
bit32u_t             LinearAddr_to_bit32u ( struct linear_addr_t x );


struct pdir_entry_t  PdirEntry_create ( bit32u_t paddr, bit32u_t val );
void                 PdirEntry_set_accessed_flag ( struct pdir_entry_t *pde, trans_t *paddr_to_raddr );
void                 PdirEntry_set_dirty_flag ( struct pdir_entry_t *pde, trans_t *paddr_to_raddr );

struct ptbl_entry_t  PtblEntry_create ( bit32u_t paddr, bit32u_t val );
void                 PtblEntry_set_accessed_flag ( struct ptbl_entry_t *pte, trans_t *paddr_to_raddr ); 
void                 PtblEntry_set_dirty_flag ( struct ptbl_entry_t *pte, trans_t *paddr_to_raddr ); 

bool_t               paging_is_enabled ( const struct regs_t *regs );
struct pdir_entry_t  lookup_page_directory ( const struct regs_t *regs, int index, trans_t *paddr_to_raddr );
struct ptbl_entry_t  lookup_page_table ( const struct pdir_entry_t *pde, int index, trans_t *paddr_to_raddr );
void                 print_page_directory ( FILE *stream, const struct regs_t *regs, trans_t *paddr_to_raddr );
bit32u_t	     try_translate_laddr_to_paddr ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr, bool_t *is_ok );
bit32u_t	     try_translate_laddr_to_paddr2 ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr, bool_t *is_ok,
						     bool_t *read_write );
bit32u_t             translate_laddr_to_paddr ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr );
bool_t               check_mem_access ( const struct regs_t *regs, bit32u_t laddr, trans_t *paddr_to_raddr );



void                 PageDescr_pack ( struct page_descr_t *x, int fd );
void                 PageDescr_unpack ( struct page_descr_t *x, int fd );
const char *         PageState_to_string ( page_state_t x );

bit32u_t             page_no_to_paddr ( int page_no );
int                  paddr_to_page_no ( bit32u_t paddr );
void                 PageDescr_init ( struct page_descr_t *x, int cpuid );
void                 PageDescr_print ( FILE *stream, struct page_descr_t *x );
void                 PAGE_DESCR_DPRINT ( struct page_descr_t *x );
void                 PageDescr_add_laddr ( struct page_descr_t *x, bit32u_t laddr, bool_t read_write );
void                 PageDescr_remove_laddr ( struct page_descr_t *x, bit32u_t laddr );
bool_t               PageDescr_get_read_write ( const struct page_descr_t *x, bit32u_t laddr );
bool_t               PageDescr_has_same_laddr ( struct page_descr_t *x, bit32u_t laddr, bool_t read_write );
bool_t		     PageDescr_cpuid_is_in_copyset ( const struct page_descr_t *x, int cpuid );


#endif /* _VMM_IA32_PAGING_H */
