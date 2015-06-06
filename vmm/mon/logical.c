#include "vmm/mon/mon.h"

void test_eb_gb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void test_ed_gd(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void test_eb_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void test_ed_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

static void and(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate) { arith(mon, instr, len, ARITH_AND, use_immediate); }
static void or(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate) { arith(mon, instr, len, ARITH_OR, use_immediate); }
static void xor(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate) { arith(mon, instr, len, ARITH_XOR, use_immediate); }
static void not(struct mon_t *mon, struct instruction_t *instr, size_t len) { arith3(mon, instr, len, ARITH_NOT); }

void and_eb_gb(struct mon_t *mon, struct instruction_t *instr) { and(mon, instr, 1, FALSE); }
void and_ed_gd(struct mon_t *mon, struct instruction_t *instr) { and(mon, instr, 4, FALSE); }
void and_eb_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void and_ed_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

void not_eb(struct mon_t *mon, struct instruction_t *instr) { not(mon, instr, 1); }
void not_ed(struct mon_t *mon, struct instruction_t *instr) { not(mon, instr, 4); }

void or_eb_gb(struct mon_t *mon, struct instruction_t *instr) { or(mon, instr, 1, FALSE); }
void or_ed_gd(struct mon_t *mon, struct instruction_t *instr) { or(mon, instr, 4, FALSE); }
void or_eb_ib(struct mon_t *mon, struct instruction_t *instr) { or(mon, instr, 1, TRUE); }
void or_ed_id(struct mon_t *mon, struct instruction_t *instr) { or(mon, instr, 4, TRUE); }

/* Logical Exclusive OR */
void xor_eb_gb(struct mon_t *mon, struct instruction_t *instr) { xor(mon, instr, 1, FALSE); }
void xor_ed_gd(struct mon_t *mon, struct instruction_t *instr) { xor(mon, instr, 4, FALSE); }
void xor_eb_ib(struct mon_t *mon, struct instruction_t *instr) { xor(mon, instr, 1, TRUE); }
void xor_ed_id(struct mon_t *mon, struct instruction_t *instr) { xor(mon, instr, 4, TRUE); }

/****************************************************************/

static void xor2(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     bit32u_t vals[2], retval;

     vals[0] = UserRegs_get2(&mon->regs->user, GEN_REG_EAX, len);
     vals[1] = instr->immediate[0];
     retval = vals[0] ^ vals[1];
     UserRegs_set2(&mon->regs->user, GEN_REG_EAX, retval, len);
     skip_instr(mon, instr);

     /* [TODO] update flags */ 
}

void xor_al_ib(struct mon_t *mon, struct instruction_t *instr) { xor2(mon, instr, 1); }     
void xor_eax_id(struct mon_t *mon, struct instruction_t *instr) { xor2(mon, instr, 4); }



