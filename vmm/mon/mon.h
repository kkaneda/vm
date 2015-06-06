#ifndef _VMM_MON_MON_H
#define _VMM_MON_MON_H

#include "vmm/common.h"
#include "vmm/mon/instr.h"
#include "vmm/mon/dev.h"
#include "vmm/mon/apic.h"
#include "vmm/mon/guest.h"
#include "vmm/mon/stat.h"


/* DEBUG */
extern bool_t get_snapshot_at_next_trap;


enum mon_mode {
     NATIVE_MODE,
     EMULATION_MODE
};
typedef enum mon_mode 	mon_mode_t;

struct mon_t {
	pid_t			pid;	/* pid of the guest process */
	mon_mode_t		mode;
	
	int			cpuid;
	struct pmem_t		pmem; 	/* physical memory */
	struct regs_t 		*regs; 	/* general register file */
	struct devices_t	devs;
	
	struct local_apic_t	*local_apic;
	struct io_apic_t	*io_apic;
	
	struct shared_info_t	*shi; 	/* shared memory space between the
					 * guest process and the monitor
					 * process */
	
	size_t			num_of_pages; /* = PMEM_SIZE / PAGE_SIZE_4K */
#ifdef ENABLE_MP
	struct comm_t		*comm;
#endif /* ENABLE_MP */
	struct page_descr_t	*page_descrs;
	
	struct guest_state_t	guest_state; /* current status of guest OS */
	struct stat_t		stat;

	bool_t 			wait_ipi;
	bit32u_t 		emu_stack_base;
};

/*** init.c ***/
void wait_for_sipi(struct mon_t *mon);
struct mon_t *Monitor_create(const struct config_t *config);


/*** mem_access.c ***/
void     Monitor_init_mem(struct mon_t *mon);

inline bit8u_t  Monitor_read_byte_with_paddr(bit32u_t paddr);
inline bit16u_t Monitor_read_word_with_paddr(bit32u_t paddr);
inline bit32u_t Monitor_read_dword_with_paddr(bit32u_t paddr);
inline void     Monitor_write_byte_with_paddr(bit32u_t paddr, bit8u_t value);
inline void     Monitor_write_word_with_paddr(bit32u_t paddr, bit16u_t value);
inline void     Monitor_write_dword_with_paddr(bit32u_t paddr, bit32u_t value);
inline void     Monitor_write_with_paddr(bit32u_t paddr, bit32u_t value, size_t len);

inline bit32u_t Monitor_laddr_to_paddr ( bit32u_t laddr );
inline bit32u_t Monitor_paddr_to_raddr ( bit32u_t paddr );
inline bit32u_t Monitor_laddr_to_raddr ( bit32u_t laddr );
inline bit32u_t Monitor_vaddr_to_raddr ( seg_reg_index_t i, bit32u_t vaddr );
inline bit32u_t Monitor_vaddr_to_laddr ( seg_reg_index_t index, bit32u_t vaddr );
inline bit8u_t  Monitor_try_read_byte_with_laddr(bit32u_t laddr, bool_t *is_ok);
inline bit8u_t  Monitor_read_byte_with_laddr(bit32u_t laddr);
inline bit16u_t Monitor_read_word_with_laddr(bit32u_t laddr);
inline bit32u_t Monitor_read_dword_with_laddr(bit32u_t laddr);
inline void     Monitor_write_byte_with_laddr(bit32u_t laddr, bit8u_t value);
inline void     Monitor_write_word_with_laddr(bit32u_t laddr, bit16u_t value);
inline void     Monitor_write_dword_with_laddr(bit32u_t laddr, bit32u_t value);

inline bit32u_t Monitor_try_vaddr_to_paddr(seg_reg_index_t index, bit32u_t vaddr, bool_t *is_ok);
inline bit32u_t Monitor_try_vaddr_to_paddr2(seg_reg_index_t index, bit32u_t vaddr, bool_t *is_ok, bool_t *read_write);
inline bit32u_t Monitor_vaddr_to_paddr ( seg_reg_index_t index, bit32u_t vaddr );
inline bit8u_t  Monitor_try_read_byte_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bool_t *is_ok);
inline bit32u_t Monitor_try_read_dword_with_vaddr ( seg_reg_index_t i, bit32u_t vaddr, bool_t *is_ok );
inline bit8u_t  Monitor_read_byte_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline bit16u_t Monitor_read_word_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline bit32u_t Monitor_read_dword_with_vaddr(seg_reg_index_t i, bit32u_t vaddr);
inline bit32u_t Monitor_read_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, size_t len);
inline bit32u_t Monitor_try_read_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, size_t len, bool_t *is_ok);
inline bit32u_t Monitor_read_with_resolve(struct mon_t *mon, struct instruction_t *instr, size_t len);
inline bit32u_t Monitor_read_reg_or_mem(struct mon_t *mon, struct instruction_t *instr, size_t len);
inline void     Monitor_write_byte_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit8u_t value);
inline void     Monitor_write_word_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit16u_t value);
inline void     Monitor_write_dword_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit32u_t value);
inline void     Monitor_write_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit32u_t value, size_t len);
inline void     Monitor_try_write_with_vaddr(seg_reg_index_t i, bit32u_t vaddr, bit32u_t value, size_t len, bool_t *is_ok);
inline void     Monitor_write_with_resolve(struct mon_t *mon, struct instruction_t *instr, bit32u_t value, size_t len);
inline void     Monitor_write_reg_or_mem(struct mon_t *mon, struct instruction_t *instr, bit32u_t value, size_t len);

void     Monitor_pushl(struct regs_t *regs, bit32u_t value);
void     Monitor_push(struct mon_t *mon, bit32u_t val, size_t len);
bit32u_t Monitor_pop(struct mon_t *mon, size_t len);
void     Monitor_set_seg_reg(struct regs_t *regs, seg_reg_index_t index, struct seg_selector_t *selector);
void     Monitor_set_seg_reg2(struct regs_t *regs, seg_reg_index_t index, bit16u_t val);

bool_t              Monitor_check_mem_access_with_laddr(const struct regs_t *regs, bit32u_t laddr);
bool_t              Monitor_check_mem_access_with_vaddr(const struct regs_t *regs, seg_reg_index_t i, bit32u_t vaddr);
struct pdir_entry_t Monitor_lookup_page_directory(const struct regs_t *regs, int index);
struct ptbl_entry_t Monitor_lookup_page_table(const struct pdir_entry_t *pde, int index);
struct descr_t      Monitor_lookup_descr_table(const struct regs_t *regs, const struct seg_selector_t *selector);
void                Monitor_update_descr_table(const struct regs_t *regs, const struct seg_selector_t *selector, const struct descr_t *descr);
struct tss_t Monitor_get_tss_of_current_task(const struct regs_t *regs);
void Monitor_print_page_directory(FILE *stream, const struct regs_t *regs);

void Monitor_mmove_rd ( bit32u_t paddr, void *to_addr, size_t len );
void Monitor_mmove_wr ( bit32u_t paddr, void *from_addr, size_t len );
void mem_check_for_dma_access ( bit32u_t paddr );

/*** decode.c ***/
struct instruction_t decode_instruction(bit32u_t eip);

/* arith.c */
enum arith_kind {
     ARITH_ADD, 
     ARITH_AND,
     ARITH_SUB,
     ARITH_OR,
     ARITH_XOR,
     ARITH_NOT,
     ARITH_NEG
};
typedef enum arith_kind		arith_kind_t;

void inc_eb(struct mon_t *mon, struct instruction_t *instr);
void inc_ed(struct mon_t *mon, struct instruction_t *instr);
void dec_eb(struct mon_t *mon, struct instruction_t *instr);
void dec_ed(struct mon_t *mon, struct instruction_t *instr);
void arith(struct mon_t *mon, struct instruction_t *instr, size_t len, arith_kind_t kind, bool_t use_immediate);
void arith3(struct mon_t *mon, struct instruction_t *instr, size_t len, arith_kind_t kind);
void add_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void add_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void add_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void add_ed_id(struct mon_t *mon, struct instruction_t *instr);
void add_gb_eb(struct mon_t *mon, struct instruction_t *instr);
void add_gd_ed(struct mon_t *mon, struct instruction_t *instr);
void sub_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void sub_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void sub_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void sub_ed_id(struct mon_t *mon, struct instruction_t *instr);
void sub_gb_eb(struct mon_t *mon, struct instruction_t *instr);
void sub_gd_ed(struct mon_t *mon, struct instruction_t *instr);
void sbb_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void sbb_ed_id(struct mon_t *mon, struct instruction_t *instr);
void adc_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void adc_ed_id(struct mon_t *mon, struct instruction_t *instr);
void cmp_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void cmp_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void cmp_gb_eb(struct mon_t *mon, struct instruction_t *instr);
void cmp_gd_ed(struct mon_t *mon, struct instruction_t *instr);
void cmp_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void cmp_ed_id(struct mon_t *mon, struct instruction_t *instr);
void xadd_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void xadd_ed_gd(struct mon_t *mon, struct instruction_t *instr);

void mul_al_eb(struct mon_t *mon, struct instruction_t *instr);
void mul_eax_ed(struct mon_t *mon, struct instruction_t *instr);
void imul_al_eb(struct mon_t *mon, struct instruction_t *instr);
void imul_eax_ed(struct mon_t *mon, struct instruction_t *instr);
void div_al_eb(struct mon_t *mon, struct instruction_t *instr);
void div_eax_ed(struct mon_t *mon, struct instruction_t *instr);
void idiv_al_eb(struct mon_t *mon, struct instruction_t *instr);
void idiv_eax_ed(struct mon_t *mon, struct instruction_t *instr);
void neg_eb(struct mon_t *mon, struct instruction_t *instr);
void neg_ed(struct mon_t *mon, struct instruction_t *instr);
void cmpxchg_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void cmpxchg_ed_gd(struct mon_t *mon, struct instruction_t *instr);

struct mem_access_t *cmpxchg_eb_gb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *cmpxchg_ed_gd_mem(struct mon_t *mon, struct instruction_t *instr);


/*** proc_ctrl.c ***/
void hlt(struct mon_t *mon, struct instruction_t *instr);
void wrmsr(struct mon_t *mon, struct instruction_t *instr);
void rdmsr(struct mon_t *mon, struct instruction_t *instr);
void cpuid(struct mon_t *mon, struct instruction_t *instr);
void smsw_ew(struct mon_t *mon, struct instruction_t *instr);
void lmsw_ew(struct mon_t *mon, struct instruction_t *instr);

void mov_rd_cd(struct mon_t *mon, struct instruction_t *instr);
void mov_cd_rd(struct mon_t *mon, struct instruction_t *instr);
void mov_dd_rd(struct mon_t *mon, struct instruction_t *instr);


/*** flag_ctrl.c ***/
void cli(struct mon_t *mon, struct instruction_t *instr);
void sti(struct mon_t *mon, struct instruction_t *instr);
void cld(struct mon_t *mon, struct instruction_t *instr);
void std(struct mon_t *mon, struct instruction_t *instr);
void clts(struct mon_t *mon, struct instruction_t *instr);
void pushf_fv(struct mon_t *mon, struct instruction_t *instr);
void popf_fv(struct mon_t *mon, struct instruction_t *instr);


/*** protect_ctrl.c ***/
void sgdt_ms(struct mon_t *mon, struct instruction_t *instr);
void sidt_ms(struct mon_t *mon, struct instruction_t *instr);
void lgdt_ms(struct mon_t *mon, struct instruction_t *instr);
void lidt_ms(struct mon_t *mon, struct instruction_t *instr);
void sldt_ew(struct mon_t *mon, struct instruction_t *instr);
void str_ew(struct mon_t *mon, struct instruction_t *instr);
void lldt_ew(struct mon_t *mon, struct instruction_t *instr);
void ltr_ew(struct mon_t *mon, struct instruction_t *instr);
void verr_ew(struct mon_t *mon, struct instruction_t *instr);
void verw_ew(struct mon_t *mon, struct instruction_t *instr);
void invlpg(struct mon_t *mon, struct instruction_t *instr);

struct mem_access_t *sgdt_ms_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *sidt_ms_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lgdt_ms_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lidt_ms_mem(struct mon_t *mon, struct instruction_t *instr);


/*** segment_ctrl.c ***/
void lss_gv_mp(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lss_gv_mp_mem(struct mon_t *mon, struct instruction_t *instr);


/*** io.c ***/
void in_al_ib(struct mon_t *mon, struct instruction_t *instr);
void in_al_dx(struct mon_t *mon, struct instruction_t *instr);
void in_eax_dx(struct mon_t *mon, struct instruction_t *instr);
void out_ib_al(struct mon_t *mon, struct instruction_t *instr);
void out_dx_al(struct mon_t *mon, struct instruction_t *instr);
void out_dx_eax(struct mon_t *mon, struct instruction_t *instr);
void insb_yb_dx(struct mon_t *mon, struct instruction_t *instr);
void insw_yv_dx(struct mon_t *mon, struct instruction_t *instr);
void outsb_dx_xb(struct mon_t *mon, struct instruction_t *instr);
void outsw_dx_xv(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *insb_yb_dx_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *insw_yv_dx_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *outsb_dx_xb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *outsw_dx_xv_mem(struct mon_t *mon, struct instruction_t *instr);

/*** logical.c ***/
void test_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void test_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void test_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void test_ed_id(struct mon_t *mon, struct instruction_t *instr); 
void xor_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void xor_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void xor_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void xor_ed_id(struct mon_t *mon, struct instruction_t *instr);
void xor_al_ib(struct mon_t *mon, struct instruction_t *instr);
void xor_eax_id( struct mon_t *mon, struct instruction_t *instr);
void or_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void or_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void or_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void or_ed_id(struct mon_t *mon, struct instruction_t *instr);
void and_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void and_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void and_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void and_ed_id(struct mon_t *mon, struct instruction_t *instr);
void not_eb(struct mon_t *mon, struct instruction_t *instr);
void not_ed(struct mon_t *mon, struct instruction_t *instr);

/*** shift.c ***/
void rol_eb(struct mon_t *mon, struct instruction_t *instr);
void rol_ed(struct mon_t *mon, struct instruction_t *instr);
void ror_eb(struct mon_t *mon, struct instruction_t *instr);
void ror_ed(struct mon_t *mon, struct instruction_t *instr);
void rcl_eb(struct mon_t *mon, struct instruction_t *instr);
void rcl_ed(struct mon_t *mon, struct instruction_t *instr);
void rcr_eb(struct mon_t *mon, struct instruction_t *instr);
void rcr_ed(struct mon_t *mon, struct instruction_t *instr);
void shl_eb(struct mon_t *mon, struct instruction_t *instr);
void shl_ed(struct mon_t *mon, struct instruction_t *instr);
void shr_eb(struct mon_t *mon, struct instruction_t *instr);
void shr_ed(struct mon_t *mon, struct instruction_t *instr);
void sar_eb(struct mon_t *mon, struct instruction_t *instr);
void sar_ed(struct mon_t *mon, struct instruction_t *instr);


/*** stack.c ***/
void push_ds(struct mon_t *mon, struct instruction_t *instr);
void pop_ds(struct mon_t *mon, struct instruction_t *instr);
void push_es(struct mon_t *mon, struct instruction_t *instr);
void pop_es(struct mon_t *mon, struct instruction_t *instr);
void push_id(struct mon_t *mon, struct instruction_t *instr);
void push_erx(struct mon_t *mon, struct instruction_t *instr);
void pop_erx(struct mon_t *mon, struct instruction_t *instr);
void push_ed(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *push_ed_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *pop_ed_mem(struct mon_t *mon, struct instruction_t *instr);

/*** bit.h ***/
void bt_ev_gv(struct mon_t *mon, struct instruction_t *instr);
void bts_ev_gv(struct mon_t *mon, struct instruction_t *instr);
void btr_ev_gv(struct mon_t *mon, struct instruction_t *instr);
void bts_ev_ib(struct mon_t *mon, struct instruction_t *instr);
void btr_ev_ib(struct mon_t *mon, struct instruction_t *instr);
void bt_ev_ib(struct mon_t *mon, struct instruction_t *instr);
void btc_ev_ib(struct mon_t *mon, struct instruction_t *instr);
void setnz_eb(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *bit_mem(struct mon_t *mon, struct instruction_t *instr);


/*** ctrl_xfer.c ***/
void jmp_ed(struct mon_t *mon, struct instruction_t *instr);
void jmp_ap(struct mon_t *mon, struct instruction_t *instr);
void jmp32_ep(struct mon_t *mon, struct instruction_t *instr);
void call_ed(struct mon_t *mon, struct instruction_t *instr);
void call32_ep(struct mon_t *mon, struct instruction_t *instr);
void call_ad(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *call_ad_mem(struct mon_t *mon, struct instruction_t *instr);

void ret_far16(struct mon_t *mon, struct instruction_t *instr);
void ret_far32(struct mon_t *mon, struct instruction_t *instr);
void iret16(struct mon_t *mon, struct instruction_t *instr);
void iret32(struct mon_t *mon, struct instruction_t *instr);

struct mem_access_t *jmp_ap_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *jmp_ed_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *jmp32_ep_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *call_ed_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *call32_ep_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *ret_far16_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *ret_far32_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *leave_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *iret16_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *iret32_mem(struct mon_t *mon, struct instruction_t *instr);


/*** data_xfer.c ***/
void mov_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void mov_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void mov_eb_ib(struct mon_t *mon, struct instruction_t *instr);
void mov_ed_id(struct mon_t *mon, struct instruction_t *instr);
void mov_gw_ew(struct mon_t *mon, struct instruction_t *instr);
void mov_gb_eb(struct mon_t *mon, struct instruction_t *instr);
void mov_gd_ed(struct mon_t *mon, struct instruction_t *instr);
void movzx_gd_eb(struct mon_t *mon, struct instruction_t *instr);
void movzx_gd_ew(struct mon_t *mon, struct instruction_t *instr);
void mov_od_eax(struct mon_t *mon, struct instruction_t *instr);
void mov_ob_al(struct mon_t *mon, struct instruction_t *instr);
void mov_al_ob(struct mon_t *mon, struct instruction_t *instr);
void mov_ax_ow(struct mon_t *mon, struct instruction_t *instr);
void mov_eax_od(struct mon_t *mon, struct instruction_t *instr);
void mov_erx_id(struct mon_t *mon, struct instruction_t *instr);
void mov_ew_sw(struct mon_t *mon, struct instruction_t *instr);
void mov_sw_ew(struct mon_t *mon, struct instruction_t *instr);
void xchg_eb_gb(struct mon_t *mon, struct instruction_t *instr);
void xchg_ed_gd(struct mon_t *mon, struct instruction_t *instr);
void cmov_gd_ed(struct mon_t *mon, struct instruction_t *instr);
void lea_gdm(struct mon_t *mon, struct instruction_t *instr);
void movsx_gd_eb(struct mon_t *mon, struct instruction_t *instr);
void movsx_gd_ew(struct mon_t *mon, struct instruction_t *instr);

struct mem_access_t *mov_od_eax_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *mov_ob_al_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *mov_al_ob_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *mov_ax_ow_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *mov_eax_od_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lea_gdm_mem(struct mon_t *mon, struct instruction_t *instr);



/*** string.c ***/
void movsb_xb_yb(struct mon_t *mon, struct instruction_t *instr); 
void movsw_xv_yv(struct mon_t *mon, struct instruction_t *instr);
void stosb_yb_al(struct mon_t *mon, struct instruction_t *instr);
void stosw_yv_eax(struct mon_t *mon, struct instruction_t *instr);
void scasb_al_xb(struct mon_t *mon, struct instruction_t *instr);
void scasw_eax_xv(struct mon_t *mon, struct instruction_t *instr);
void cmpsb_xb_yb(struct mon_t *mon, struct instruction_t *instr);
void cmpsw_xv_yv(struct mon_t *mon, struct instruction_t *instr);
void lodsb_al_xb(struct mon_t *mon, struct instruction_t *instr);
void lodsw_eax_xv(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *movsb_xb_yb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *movsw_xv_yv_mem(struct mon_t *mon, struct instruction_t *instr); 
struct mem_access_t *stosb_yb_al_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *stosw_yv_eax_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *scasb_al_xb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *scasw_eax_xv_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *cmpsb_xb_yb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *cmpsw_xv_yv_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lodsb_al_xb_mem(struct mon_t *mon, struct instruction_t *instr);
struct mem_access_t *lodsw_eax_xv_mem(struct mon_t *mon, struct instruction_t *instr);

void start_or_stop_profile ( struct mon_t *mon );
void start_or_stop_profile2 ( struct mon_t *mon, bool_t start_flag );


/*** mon_print.c ***/
void Monitor_print_simple(FILE *stream, struct mon_t *mon);
void Monitor_print_detail(FILE *stream, struct mon_t *mon);
void Monitor_print(FILE *stream, struct mon_t *mon);
void MONITOR_DPRINT(struct mon_t *mon);

void Monitor_create_snapshot ( struct mon_t *mon );
void Monitor_resume ( struct mon_t *mon, const char *filename );
void Monitor_migrate ( struct mon_t *mon, struct node_t dest, char *new_config );

/*** shmem.c ***/
bool_t emulate_shared_memory_with_vaddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr, bit32u_t vaddr );
bool_t emulate_shared_memory_with_paddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr );
void sync_shared_memory(struct mon_t *mon, struct instruction_t *i);
void try_handle_pending_fetch_requests ( struct mon_t *mon );

void handle_fetch_ack_ack ( struct mon_t *mon, struct msg_t *msg );
void handle_fetch_ack ( struct mon_t *mon, struct msg_t *msg );
void handle_invalidate_request ( struct mon_t *mon, struct msg_t *msg );
void handle_fetch_request ( struct mon_t *mon, struct msg_t *msg );

void wait_recvable_msg ( struct mon_t *mon, int sleep_time );


/*** main.c ***/
inline bool_t is_native_mode(struct mon_t *mon);
inline bool_t is_emulation_mode(struct mon_t* mon);
inline bool_t is_bootstrap_proc(struct mon_t* mon);
inline bool_t is_application_proc(struct mon_t* mon);
inline void cancel_signal_delivery(struct mon_t *mon);

void check_pgtable_permission ( struct mon_t *mon, bit32u_t cr3 );

void run_emulation_code_of_vm(struct mon_t *mon, vm_handler_kind_t kind, ...);
void emulate_fault(struct mon_t *mon);
int  trap_vm(struct mon_t *mon);
void handle_msg(struct mon_t *mon, struct msg_t *msg);
void restart_vm(struct mon_t *mon, int signo );
void restart_vm_with_no_signal ( struct mon_t *mon );
void try_handle_msgs(struct mon_t *mon);
void try_handle_msgs_with_stat ( struct mon_t *mon );
void Monitor_finalize ( int status, void *arg );

#endif /* _VM_MON_MON_H */
