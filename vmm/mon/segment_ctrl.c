#include "vmm/mon/mon.h"

/* Load Far Pointer */
void
lss_gv_mp(struct mon_t *mon, struct instruction_t *instr)
{
     bit32u_t reg_32, vaddr;
     bit16u_t ss_raw;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     assert(instr->mod != 3);

     vaddr = instr->resolve(instr, &mon->regs->user);
     reg_32 = Monitor_read_dword_with_vaddr(instr->sreg_index, vaddr);

     ss_raw = Monitor_read_word_with_vaddr(instr->sreg_index, vaddr + 4);
     Monitor_set_seg_reg2(mon->regs, SEG_REG_SS, ss_raw);
     
     UserRegs_set(&mon->regs->user, instr->reg, reg_32);

     skip_instr(mon, instr);
}

struct mem_access_t *
lss_gv_mp_mem(struct mon_t *mon, struct instruction_t *instr)
{
     struct mem_access_t *maccess;
     bit32u_t vaddr;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     vaddr = instr->resolve(instr, &mon->regs->user);
     maccess = MemAccess_create_read(instr->sreg_index, vaddr, 4);
     maccess->next = MemAccess_create_read(instr->sreg_index, vaddr + 4, 2);
     return maccess;
}
