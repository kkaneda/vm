#include "vmm/mon/mon.h"

static void
mov(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate)
{
     bit32u_t value;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     
     value = ((use_immediate) 
	      ? instr->immediate[0]
	      : UserRegs_get2(&mon->regs->user, instr->reg, len));

     Monitor_write_reg_or_mem(mon, instr, value, len);
     skip_instr(mon, instr);
}

void mov_eb_gb(struct mon_t *mon, struct instruction_t *instr) { mov(mon, instr, 1, FALSE); }
void mov_ed_gd(struct mon_t *mon, struct instruction_t *instr) { mov(mon, instr, ((instr->opsize_override) ? 2 : 4), FALSE); }
void mov_eb_ib(struct mon_t *mon, struct instruction_t *instr) { mov(mon, instr, 1, TRUE); }
void mov_ed_id(struct mon_t *mon, struct instruction_t *instr) { mov(mon, instr, ((instr->opsize_override) ? 2 : 4), TRUE); }

/****************************************************************/

static void
mov2(struct mon_t *mon, struct instruction_t *instr, size_t len1, size_t len2)
{
     bit32u_t value;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     
     value = Monitor_read_reg_or_mem(mon, instr, len1);
     UserRegs_set2(&mon->regs->user, instr->reg, value, len2);
     skip_instr(mon, instr);
}

void mov_gb_eb(struct mon_t *mon, struct instruction_t *instr) { mov2(mon, instr, 1, 1); }
void mov_gd_ed(struct mon_t *mon, struct instruction_t *instr) { mov2(mon, instr, 4, 4); }
void movzx_gd_eb(struct mon_t *mon, struct instruction_t *instr) { mov2(mon, instr, 1, ((instr->opsize_override) ? 2 : 4)); }
void movzx_gd_ew(struct mon_t *mon, struct instruction_t *instr) { mov2(mon, instr, 2, 4); }

/****************************************************************/

static void
mov3(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     seg_reg_index_t index;
     bit32u_t vaddr;
     bit32u_t value;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     index = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;     
     vaddr = instr->immediate[0];
     value = UserRegs_get2(&mon->regs->user, GEN_REG_EAX, len);
     Monitor_write_with_vaddr(index, vaddr, value, len);
     skip_instr(mon, instr);
}

static struct mem_access_t *
mov3_mem(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     seg_reg_index_t index;
     bit32u_t vaddr;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     
     index = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;
     vaddr = instr->immediate[0];
     return MemAccess_create_write(index, vaddr, len);
}

void mov_od_eax(struct mon_t *mon, struct instruction_t *instr) { mov3(mon, instr, 4); }
void mov_ob_al(struct mon_t *mon, struct instruction_t *instr) { mov3(mon, instr, 1); }

struct mem_access_t *mov_od_eax_mem(struct mon_t *mon, struct instruction_t *instr) { return mov3_mem(mon, instr, ((instr->opsize_override) ? 2 : 4)); }
struct mem_access_t *mov_ob_al_mem(struct mon_t *mon, struct instruction_t *instr) { return mov3_mem(mon, instr, 1); }

/****************************************************************/

static void
mov4(struct mon_t *mon, struct instruction_t *instr, size_t len) 
{
     seg_reg_index_t index;
     bit32u_t vaddr;
     bit32u_t value;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     index = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;     
     vaddr = instr->immediate[0];
     value = Monitor_read_with_vaddr(index, vaddr, len);
     UserRegs_set2(&mon->regs->user, GEN_REG_EAX, value, len);
     skip_instr(mon, instr);
}

static struct mem_access_t *
mov4_mem(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     seg_reg_index_t index;
     bit32u_t vaddr;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     index = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;
     vaddr = instr->immediate[0];
     return MemAccess_create_read(index, vaddr, len);
}

void mov_al_ob(struct mon_t *mon, struct instruction_t *instr) { mov4(mon, instr, 1); }
void mov_ax_ow(struct mon_t *mon, struct instruction_t *instr) { mov4(mon, instr, 2); }
void mov_eax_od(struct mon_t *mon, struct instruction_t *instr) { mov4(mon, instr, 4); }

struct mem_access_t *mov_al_ob_mem(struct mon_t *mon, struct instruction_t *instr) { return mov4_mem(mon, instr, 1); }
struct mem_access_t *mov_ax_ow_mem(struct mon_t *mon, struct instruction_t *instr) { return mov4_mem(mon, instr, 2); }
struct mem_access_t *mov_eax_od_mem(struct mon_t *mon, struct instruction_t *instr) { return mov4_mem(mon, instr, 4); }

/****************************************************************/

void mov_erx_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

void
mov_ew_sw(struct mon_t *mon, struct instruction_t *instr)
{
     bit16u_t seg_reg;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     
     seg_reg = SegReg_to_bit16u(&mon->regs->segs[instr->reg]);
     Monitor_write_reg_or_mem(mon, instr, seg_reg, 4);
     skip_instr(mon, instr);
}

void
mov_sw_ew(struct mon_t *mon, struct instruction_t *instr)
{
     bit16u_t seg_reg;
     
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     ASSERT(instr->reg != SEG_REG_CS);

     seg_reg = Monitor_read_reg_or_mem(mon, instr, 2);
     Monitor_set_seg_reg2(mon->regs, instr->reg, seg_reg);
     skip_instr(mon, instr);
}

/****************************************************************/

static void
xchg(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     bit32u_t values[2];
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     values[0] = Monitor_read_reg_or_mem(mon, instr, len);
     values[1] = UserRegs_get2(&mon->regs->user, instr->reg, len);

     Monitor_write_reg_or_mem(mon, instr, values[1], len);
     UserRegs_set2(&mon->regs->user, instr->reg, values[0], len);
     skip_instr(mon, instr);
}

void xchg_eb_gb(struct mon_t *mon, struct instruction_t *instr) { xchg(mon, instr, 1); }
void xchg_ed_gd(struct mon_t *mon, struct instruction_t *instr) { xchg(mon, instr, 4); }

/****************************************************************/

void cmov_gd_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

void lea_gdm(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

struct mem_access_t *
lea_gdm_mem(struct mon_t *mon, struct instruction_t *instr)
{
     ASSERT(instr->mod != 3);
     return NULL;
}

/****************************************************************/

void movsx_gd_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void movsx_gd_ew(struct mon_t *mon, struct instruction_t *instr) { assert(0); }



