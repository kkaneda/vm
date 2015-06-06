#include "vmm/mon/mon.h"

struct mem_access_t *
bit_mem(struct mon_t *mon, struct instruction_t *instr)
{
     bit32u_t op2_32, disp32, vaddr;
     struct mem_access_t *maccess;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     if (instr->mod == 3) { return NULL; }
         
     op2_32 = UserRegs_get(&mon->regs->user, instr->reg);
     disp32 = (op2_32 >> 5);

     vaddr = instr->resolve(instr, &mon->regs->user) + (disp32 << 2);
     maccess = MemAccess_create_read(instr->sreg_index, vaddr, 4);
     maccess->next = MemAccess_create_write(instr->sreg_index, vaddr, 4);
     return maccess;     
}

/* Bit Test */
void bt_ev_gv(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Bit Test and Set */
void bts_ev_gv(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Bit Test and Reset */
void btr_ev_gv(struct mon_t *mon, struct instruction_t *instr) { assert(0); }


/****************************************************************/

/* Bit Test */
void bt_ev_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Bit Test and Set */
void bts_ev_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Bit Test and Reset */
void btr_ev_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Bit Test and Complement */
void btc_ev_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

void setnz_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
