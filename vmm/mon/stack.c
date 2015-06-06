#include "vmm/mon/mon.h"

static void
push_sreg(struct mon_t *mon, struct instruction_t *instr, seg_reg_index_t sreg_index)
{
     bit32u_t val;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     val = SegReg_to_bit16u(&mon->regs->segs[sreg_index]);
     
     /*
     Print_color ( stdout, BLUE, "push: [%d] %#x, eip=%#x, esp=%#x\n", 
		   sreg_index,
		   val,
		   mon->regs->user.eip, 
		   mon->regs->user.esp );
     */

     Monitor_push(mon, val, 4);
     skip_instr(mon, instr);          
}

static void
pop_sreg(struct mon_t *mon, struct instruction_t *instr, seg_reg_index_t sreg_index)
{
     bit32u_t val;
     
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     val = Monitor_pop(mon, 4);

     /*
     Print_color ( stdout, BLUE, "popx: [%d] %#x --> %#x\n", 
		   sreg_index,
		   mon->regs->segs[sreg_index].val, 
		   val );
     */

     Monitor_set_seg_reg2(mon->regs, sreg_index, val);
     skip_instr(mon, instr);
}

void push_ds(struct mon_t *mon, struct instruction_t *instr) { push_sreg(mon, instr, SEG_REG_DS); }
void push_es(struct mon_t *mon, struct instruction_t *instr) { push_sreg(mon, instr, SEG_REG_ES); }
void pop_ds(struct mon_t *mon, struct instruction_t *instr) { pop_sreg(mon, instr, SEG_REG_DS); }
void pop_es(struct mon_t *mon, struct instruction_t *instr) { pop_sreg(mon, instr, SEG_REG_ES); }

/****************************************************************/

void
push_id(struct mon_t *mon, struct instruction_t *instr)
{
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     Monitor_push(mon, instr->immediate[0], 4);
     skip_instr(mon, instr);     
}

/****************************************************************/

void push_erx(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void pop_erx(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

void
push_ed(struct mon_t *mon, struct instruction_t *instr) 
{
     bit32u_t val;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     val = Monitor_read_reg_or_mem(mon, instr, 4);     
     Monitor_push(mon, val, 4);
     skip_instr(mon, instr);
}

struct mem_access_t *
push_ed_mem(struct mon_t *mon, struct instruction_t *instr)
{
     struct mem_access_t *maccess;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     if (instr->mod == 3) {
	  maccess = MemAccess_push(mon, 4);
     } else {
	  maccess = MemAccess_create_read_resolve(mon, instr, 4);
	  maccess->next = MemAccess_push(mon, 4);
	  return maccess;
     }

     return maccess;
}


struct mem_access_t *
pop_ed_mem(struct mon_t *mon, struct instruction_t *instr)
{
     struct mem_access_t *maccess;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     maccess = MemAccess_pop(mon, 4);

     if (instr->mod != 3) {
	  maccess->next = MemAccess_create_write_resolve(mon, instr, 4);
     }

     return maccess;
}
