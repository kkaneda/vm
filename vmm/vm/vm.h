#ifndef _VMM_VM_VM_H
#define _VMM_VM_VM_H

#include "vmm/common.h"


struct mem_map_entry_t {
	bit32u_t			laddr;
	bit32u_t			paddr;
	bool_t				read_write;

	struct mem_map_entry_t 		*next;
};

struct mem_map_list_t {
	struct mem_map_entry_t *head;
};

struct vm_t {
	int			cpuid;
	struct pmem_t		pmem; 	/* physical memory */
	struct regs_t 		*regs; 
	struct shared_info_t	*shi;
	size_t			num_of_pages; /* = PMEM_SIZE / PAGE_SIZE_4K */
	
	struct mem_map_list_t	mem_map_list;
	struct page_descr_t	*page_descrs;
};



void Vm_init_mem ( int cpuid );

inline bit8u_t  Vm_read_byte_with_paddr(bit32u_t paddr);
inline bit16u_t Vm_read_word_with_paddr(bit32u_t paddr);
inline bit32u_t Vm_read_dword_with_paddr(bit32u_t paddr);
inline void     Vm_write_byte_with_paddr(bit32u_t paddr, bit8u_t value);
inline void     Vm_write_word_with_paddr(bit32u_t paddr, bit16u_t value);
inline void     Vm_write_dword_with_paddr(bit32u_t paddr, bit32u_t value);

inline bit8u_t  Vm_read_byte_with_laddr(bit32u_t laddr);
inline bit16u_t Vm_read_word_with_laddr(bit32u_t laddr);
inline bit32u_t Vm_read_dword_with_laddr(bit32u_t laddr);
inline void     Vm_write_byte_with_laddr(bit32u_t laddr, bit8u_t value);
inline void     Vm_write_word_with_laddr(bit32u_t laddr, bit16u_t value);
inline void     Vm_write_dword_with_laddr(bit32u_t laddr, bit32u_t value);

inline bit32u_t Vm_try_laddr_to_paddr(bit32u_t laddr, bool_t *is_ok);
inline bit32u_t Vm_try_laddr_to_paddr2 ( bit32u_t laddr, bool_t *is_ok, bool_t *read_write );
inline bit32u_t Vm_laddr_to_paddr(bit32u_t laddr);

inline bit8u_t  Vm_read_byte_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline bit16u_t Vm_read_word_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline bit32u_t Vm_read_dword_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline void     Vm_write_byte_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit8u_t value);
inline void     Vm_write_word_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit16u_t value);
inline void     Vm_write_dword_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit32u_t value);

void     Vm_pushl(struct regs_t *regs, bit32u_t value);
void     Vm_set_seg_reg(struct regs_t *regs, seg_reg_index_t index, struct seg_selector_t *selector);
void     Vm_set_seg_reg2(struct regs_t *regs, seg_reg_index_t index, bit16u_t val);

struct pdir_entry_t Vm_lookup_page_directory(const struct regs_t *regs, int index);
struct ptbl_entry_t Vm_lookup_page_table(const struct pdir_entry_t *pde, int index);
bool_t              Vm_check_mem_access(const struct regs_t *regs, bit32u_t laddr);

void print_page_prot(FILE *stream, int prot);

void Vm_map_page(struct vm_t *vm, bit32u_t laddr);
void Vm_init_page_mapping(struct vm_t *vm);
void Vm_print_page_directory(FILE *stream, const struct regs_t *regs);

void set_cr0(struct vm_t *vm, bit32u_t val);
void set_cr3(struct vm_t *vm, bit32u_t val);
void set_cr4(struct vm_t *vm, bit32u_t val);
void invalidate_tlb(struct vm_t *vm);
void change_page_prot(struct vm_t *vm, int page_no);
bool_t try_add_new_mem_map ( struct vm_t *vm, bit32u_t laddr, bit32u_t paddr, bool_t read_write );
void allow_access_to_descr_tables ( struct regs_t *regs );
void restore_permission_of_descr_tables ( struct regs_t *regs );

void Vm_print_page_prot(FILE *stream, struct vm_t *vm, bit32u_t laddr);

void handle_signal(int sig, struct sigcontext x);

struct vm_t *Vm_init(int argc, char *argv[]);

void Vm_unmap_all ( struct vm_t *vm );

inline struct vm_t *Vm_get(void);
inline bool_t is_bootstrap_proc(struct vm_t* vm);
inline bool_t is_application_proc(struct vm_t* vm);



#endif /* _VMM_VM_VM_H */

