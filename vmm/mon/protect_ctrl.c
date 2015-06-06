#include "vmm/mon/mon.h"

static void
sgdt_or_sidt_ms(struct mon_t *mon, struct instruction_t *instr, struct global_seg_reg_t *gsreg)
{
     bit16u_t limit_16;
     bit32u_t base_32;
     bit32u_t vaddr;
    
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(gsreg != NULL);
     assert(instr->mod != 3);

     limit_16 = gsreg->limit;
     base_32 = gsreg->base;
     
     vaddr = instr->resolve(instr, &mon->regs->user);
     Monitor_write_word_with_vaddr(instr->sreg_index, vaddr, limit_16);
     Monitor_write_dword_with_vaddr(instr->sreg_index, vaddr + 2, base_32);

     skip_instr(mon, instr); 
}

static struct mem_access_t *
sgdt_or_sidt_ms_mem(struct mon_t *mon, struct instruction_t *instr, struct global_seg_reg_t *gsreg)
{
     struct mem_access_t *maccess;
     bit32u_t vaddr;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(gsreg != NULL);
     assert(instr->mod != 3);

     vaddr = instr->resolve(instr, &mon->regs->user);
     maccess = MemAccess_create_write(instr->sreg_index, vaddr, 2);
     maccess->next = MemAccess_create_write(instr->sreg_index, vaddr + 2, 4);
     return maccess;
}

static void
lgdt_or_lidt_ms(struct mon_t *mon, struct instruction_t *instr, struct global_seg_reg_t *gsreg)
{
     bit32u_t vaddr;
     
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(gsreg != NULL);
     assert(cpl_is_supervisor_mode(mon->regs));
     assert(instr->mod != 3);

     vaddr = instr->resolve(instr, &mon->regs->user);
     gsreg->limit = Monitor_read_word_with_vaddr(instr->sreg_index, vaddr);
     gsreg->base = Monitor_read_dword_with_vaddr(instr->sreg_index, vaddr + 2);
     /*
     Print_color ( stdout, CYAN, "[CPU%d] (%#x) lgdt_or_lidt = (base = %#x, limit = %#x)\n", 
		   mon->cpuid, 
		   mon->regs->user.eip, 
		   gsreg->base, gsreg->limit );
     */
     skip_instr(mon, instr);      
}

static struct mem_access_t *
lgdt_or_lidt_ms_mem(struct mon_t *mon, struct instruction_t *instr)
{
     struct mem_access_t *maccess;
     bit32u_t vaddr;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     assert(instr->mod != 3);

     vaddr = instr->resolve(instr, &mon->regs->user);
     maccess = MemAccess_create_read(instr->sreg_index, vaddr, 2);
     maccess->next = MemAccess_create_read(instr->sreg_index, vaddr + 2, 4);
     return maccess;
}

/* Store Global Descriptor Table Register */
void sgdt_ms(struct mon_t *mon, struct instruction_t *instr) { sgdt_or_sidt_ms(mon, instr, &mon->regs->sys.gdtr); }

/* Store Interrupt Descriptor Table Register */
void sidt_ms(struct mon_t *mon, struct instruction_t *instr) { sgdt_or_sidt_ms(mon, instr, &mon->regs->sys.idtr); }

/* Load Global Descriptor Table Register */
void lgdt_ms(struct mon_t *mon, struct instruction_t *instr) { lgdt_or_lidt_ms(mon, instr, &mon->regs->sys.gdtr); }

/* Load Interrupt Descriptor Table Register */
void lidt_ms(struct mon_t *mon, struct instruction_t *instr) { lgdt_or_lidt_ms(mon, instr, &mon->regs->sys.idtr); }

struct mem_access_t *sgdt_ms_mem(struct mon_t *mon, struct instruction_t *instr) { return sgdt_or_sidt_ms_mem(mon, instr, &mon->regs->sys.gdtr); } 
struct mem_access_t *sidt_ms_mem(struct mon_t *mon, struct instruction_t *instr) { return sgdt_or_sidt_ms_mem(mon, instr, &mon->regs->sys.idtr); } 
struct mem_access_t *lgdt_ms_mem(struct mon_t *mon, struct instruction_t *instr) { return lgdt_or_lidt_ms_mem(mon, instr); } 
struct mem_access_t *lidt_ms_mem(struct mon_t *mon, struct instruction_t *instr) { return lgdt_or_lidt_ms_mem(mon, instr); } 

/****************************************************************/

static void
sldt_or_str_ew(struct mon_t *mon, struct instruction_t *instr, struct seg_reg_t *sreg)
{
     bit16u_t val;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(sreg != NULL);

     val = SegSelector_to_bit16u(&sreg->selector);
     Monitor_write_reg_or_mem(mon, instr, val, 2);
     skip_instr(mon, instr);      
}

static void
lldt_or_ltr_ew_sub(struct mon_t *mon, struct instruction_t *instr, seg_reg_index_t sreg_index, struct seg_selector_t *selector)
{
     struct descr_t descr;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(selector != NULL);

     descr = Monitor_lookup_descr_table(mon->regs, selector);

     switch (sreg_index) {
     case SEG_REG_LDTR: {
	  struct seg_descr_t ldt;
	  ldt = Descr_to_ldt_seg_descr(&descr);
	  assert(ldt.present);

	  Descr_set_sys_descr_type(&descr, LDT_SEG);
	  Monitor_update_descr_table(mon->regs, selector, &descr);
	  break;
     }
     case SEG_REG_TR: {
	  struct seg_descr_t tssd;
	  tssd = Descr_to_available_task_state_seg_descr(&descr);
	  assert(tssd.present);
     
	  Descr_set_sys_descr_type(&descr, BUSY_TASK_STATE_SEG);
	  Monitor_update_descr_table(mon->regs, selector, &descr);
	  break;
     }
     default: 
	  Match_failure("lldt_or_ltr_ew_sub\n");
     }
}

static void
lldt_or_ltr_ew(struct mon_t *mon, struct instruction_t *instr, seg_reg_index_t sreg_index)
{
     bit16u_t raw_selector;
     struct seg_selector_t selector;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     assert(cpl_is_supervisor_mode(mon->regs));

     raw_selector = Monitor_read_reg_or_mem(mon, instr, 2);
     selector = SegSelector_of_bit16u(raw_selector);

     SEG_SELECTOR_DPRINT(&selector);

     /* [TODO] if selector is NULL, invalidate and done */
     if ((raw_selector & 0xfffc) != 0) {
	  lldt_or_ltr_ew_sub(mon, instr, sreg_index, &selector);
     }

     Monitor_set_seg_reg(mon->regs, sreg_index, &selector);

     skip_instr(mon, instr); 
}

/* Store Local Descriptor Table Register */
void sldt_ew(struct mon_t *mon, struct instruction_t *instr) { sldt_or_str_ew(mon, instr, &mon->regs->sys.ldtr); }

/* Store Task Register */
void str_ew(struct mon_t *mon, struct instruction_t *instr) { sldt_or_str_ew(mon, instr, &mon->regs->sys.tr); }

/* Load Local Descriptor Table Register */
void lldt_ew(struct mon_t *mon, struct instruction_t *instr) { lldt_or_ltr_ew(mon, instr, SEG_REG_LDTR); }

/* Load Task Register */
void ltr_ew(struct mon_t *mon, struct instruction_t *instr) { lldt_or_ltr_ew(mon, instr, SEG_REG_TR); }

struct mem_access_t *sldt_ew_mem(struct mon_t *mon, struct instruction_t *instr) { return MemAccess_create_write_resolve_if_not_mod3(mon, instr, 2); }
struct mem_access_t *str_ew_mem(struct mon_t *mon, struct instruction_t *instr) { return MemAccess_create_write_resolve_if_not_mod3(mon, instr, 2); }
struct mem_access_t *lldt_ew_mem(struct mon_t *mon, struct instruction_t *instr) { return MemAccess_create_read_resolve_if_not_mod3(mon, instr, 2); }
struct mem_access_t *ltr_ew_mem(struct mon_t *mon, struct instruction_t *instr) { return MemAccess_create_read_resolve_if_not_mod3(mon, instr, 2); }

/****************************************************************/


/* Verify a Segment for Reading or Writing */
void verr_ew(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Verify a Segment for Reading or Writing */
void verw_ew(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Invalidate TLB Entry */
void
invlpg(struct mon_t *mon, struct instruction_t *instr)
{
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     check_pgtable_permission ( mon, mon->regs->sys.cr3.val );
     run_emulation_code_of_vm(mon, INVALIDATE_TLB);
     skip_instr(mon, instr); 
}

