#ifndef _VMM_IA32_REGS_H
#define _VMM_IA32_REGS_H

#include "vmm/ia32/regs_common.h"

const char *GenRegIndex_to_string ( gen_reg_index_t index );

const char *SegRegIndex_to_string ( seg_reg_index_t index );

const char * TblIndicator_to_string ( tbl_indicator_t x );

struct seg_selector_t SegSelector_of_bit16u ( bit16u_t val );
bit16u_t              SegSelector_to_bit16u ( const struct seg_selector_t *x );
void                  SegSelector_print ( FILE *stream, const struct seg_selector_t *x );
void                  SEG_SELECTOR_DPRINT ( const struct seg_selector_t *x );

struct seg_reg_t SegReg_create ( const struct seg_selector_t *selector, 
				 const struct descr_t *cache );
bit16u_t         SegReg_to_bit16u ( const struct seg_reg_t *x );
void             SegReg_print ( FILE *stream, const struct seg_reg_t *x );

struct global_seg_reg_t GlobalSegReg_of_bit48u ( bit48u_t val );
void                    GlobalSegReg_print ( FILE *stream, const struct global_seg_reg_t *x );

bit32u_t UserRegs_get ( const struct user_regs_struct *uregs, gen_reg_index_t index );
bit32u_t UserRegs_get2 ( struct user_regs_struct *uregs, gen_reg_index_t index, size_t len );
void     UserRegs_set ( struct user_regs_struct *uregs, gen_reg_index_t index, bit32u_t vale );
void     UserRegs_set2 ( struct user_regs_struct *uregs,
			 gen_reg_index_t index, bit32u_t value, size_t len );
void     UserRegs_set_from_sigcontext ( struct user_regs_struct *uregs, struct sigcontext *sc );
void     UserRegs_print ( FILE *stream, const struct user_regs_struct *x );

struct flag_reg_t FlagReg_of_bit32u ( bit32u_t val );
void              FlagReg_set_direction_flag ( struct flag_reg_t *x );
void              FlagReg_clear_direction_flag ( struct flag_reg_t *x );
void              FlagReg_set_interrupt_enable_flag ( struct flag_reg_t *x );
void              FlagReg_clear_interrupt_enable_flag ( struct flag_reg_t *x );
void              FlagReg_set_virtual_interrupt_flag ( struct flag_reg_t *x );
void              FlagReg_clear_virtual_interrupt_flag ( struct flag_reg_t *x );
void              FlagReg_print ( FILE *stream, const struct flag_reg_t *x );
void              FlagReg_merge1 ( struct flag_reg_t *x, bit32u_t val );
bit32u_t          FlagReg_merge2 ( const struct flag_reg_t *x, bit32u_t val );
bit32u_t          FlagReg_get_delta ( const struct flag_reg_t *x, size_t len );

struct cr0_t Cr0_of_bit32u ( bit32u_t val );
void         Cr0_set_paging ( struct cr0_t *x );
void         Cr0_clear_paging ( struct cr0_t *x );
void         Cr0_set_task_switched ( struct cr0_t *x );
void         Cr0_clear_task_switched ( struct cr0_t *x );

struct cr3_t Cr3_of_bit32u ( bit32u_t val );

struct cr4_t Cr4_of_bit32u ( bit32u_t val );
void         Cr4_set_page_size_extension ( struct cr4_t *x );
void         Cr4_clear_page_size_extension ( struct cr4_t *x );
void         Cr4_set_page_global_enable ( struct cr4_t *x );
void         Cr4_clear_page_global_enable ( struct cr4_t *x );

void SysRegs_print ( FILE *stream, const struct sys_regs_t *x );


struct descr_t Regs_lookup_descr_table ( const struct regs_t *regs,
				       const struct seg_selector_t *selector,
				       trans_t *laddr_to_raddr );
void           Regs_update_descr_table ( const struct regs_t *regs, 
				       const struct seg_selector_t *selector, 
				       const struct descr_t *descr, 
				       trans_t *laddr_to_raddr );
bit32u_t       Regs_get_gen_reg ( struct regs_t *regs, gen_reg_index_t index );
void           Regs_set_gen_reg ( struct regs_t *regs, gen_reg_index_t index, bit32u_t val );
bit16u_t       Regs_get_seg_reg ( struct regs_t *regs, seg_reg_index_t index );
void           Regs_set_seg_reg ( struct regs_t *regs, 
				  seg_reg_index_t index, struct seg_selector_t *selector, 
				  trans_t *laddr_to_raddr );
void           Regs_set_seg_reg2 ( struct regs_t *regs, seg_reg_index_t index, bit16u_t val, 
				   trans_t *laddr_to_raddr );
void           Regs_print ( FILE *stream, const struct regs_t *x );
void           Regs_pack ( const struct regs_t *x, int fd );
void           Regs_unpack ( const struct regs_t *x, int fd );


struct descr_t lookup_gdt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr );
struct descr_t lookup_ldt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr );
struct descr_t lookup_idt ( const struct regs_t *regs, int index, trans_t *laddr_to_raddr );
void           update_gdt ( const struct regs_t *regs, int index, const struct descr_t *descr, 
			    trans_t *laddr_to_raddr );
void           update_ldt ( const struct regs_t *regs, int index, const struct descr_t *descr, 
			    trans_t *laddr_to_raddr );
void           update_idt ( const struct regs_t *regs, int index, const struct descr_t *descr, 
			    trans_t *laddr_to_raddr );

privilege_level_t  cpl ( const struct regs_t *regs );
bool_t             cpl_is_user_mode ( const struct regs_t *regs );
bool_t             cpl_is_supervisor_mode ( const struct regs_t *regs );
void               pushl ( struct regs_t *regs, bit32u_t value, trans_t *vaddr_to_raddr );


#endif /* _VMM_IA32_REGS_H */
