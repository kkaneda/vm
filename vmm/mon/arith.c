#include "vmm/mon/mon.h"

enum inc_or_dec {
     INC, DEC
};
typedef enum inc_or_dec		inc_or_dec_t;

static bit32u_t
eval_inc_or_dec(inc_or_dec_t kind, bit32u_t value)
{
     bit32u_t retval = 0;
     switch (kind) {
     case INC: retval = value + 1; break;
     case DEC: retval = value - 1; break;
     default:  Match_failure("inc_or_dec_value\n");
     }
     return retval;
}

static void
inc_or_dec(struct mon_t *mon, struct instruction_t *instr, size_t len, inc_or_dec_t kind)
{
     bit32u_t value;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     value = Monitor_read_reg_or_mem(mon, instr, len);
     value = eval_inc_or_dec(kind, value);
     Monitor_write_reg_or_mem(mon, instr, value, len);     
     skip_instr(mon, instr);     
}

static void inc(struct mon_t *mon, struct instruction_t *instr, size_t len) { inc_or_dec(mon, instr, len, INC); }
static void dec(struct mon_t *mon, struct instruction_t *instr, size_t len) { inc_or_dec(mon, instr, len, DEC); }

void inc_eb(struct mon_t *mon, struct instruction_t *instr) { inc(mon, instr, 1); }
void inc_ed(struct mon_t *mon, struct instruction_t *instr) { inc(mon, instr, 4); }
void dec_eb(struct mon_t *mon, struct instruction_t *instr) { dec(mon, instr, 1); }
void dec_ed(struct mon_t *mon, struct instruction_t *instr) { dec(mon, instr, 4); }

/****************************************************************/

static bit32u_t
eval(arith_kind_t kind, bit32u_t values[2])
{
     bit32u_t retval = 0;
     switch (kind) {
     case ARITH_ADD: retval = values[0] + values[1]; break;
     case ARITH_AND: retval = values[0] & values[1]; break;
     case ARITH_SUB: retval = values[0] - values[1]; break;
     case ARITH_OR:  retval = values[0] | values[1]; break;
     case ARITH_XOR: retval = values[0] ^ values[1]; break;
     default:        Match_failure("eval\n");
     }
     return retval;
}

static bit32u_t 
eval2(arith_kind_t kind, bit32u_t value)
{
     bit32u_t retval = 0;
     switch (kind) {
     case ARITH_NOT: retval = ~value; break;
     case ARITH_NEG: retval = 0 - value; break;
     default:        Match_failure("eval2\n");
     }
     return retval;
}

void
arith(struct mon_t *mon, struct instruction_t *instr, size_t len, arith_kind_t kind, bool_t use_immediate)
{
     bit32u_t values[2], retval;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     values[0] = Monitor_read_reg_or_mem(mon, instr, len);
     values[1] = ((use_immediate) 
		  ? instr->immediate[0]
		  : UserRegs_get2(&mon->regs->user, instr->reg, len));
     retval = eval(kind, values);
     Monitor_write_reg_or_mem(mon, instr, retval, len);

     skip_instr(mon, instr);
}
	   
struct mem_access_t *
arith_mem(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     return MemAccess_create_read_write_resolve_if_not_mod3(mon, instr, len);
}

static void
arith2(struct mon_t *mon, struct instruction_t *instr, size_t len, arith_kind_t kind)
{
     bit32u_t values[2], retval;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     values[0] = UserRegs_get2(&mon->regs->user, instr->reg, len);
     values[1] = Monitor_read_reg_or_mem(mon, instr, len);
     retval = eval(kind, values);
     UserRegs_set2(&mon->regs->user, instr->reg, retval, len);
     skip_instr(mon, instr);
}
	   
void
arith3(struct mon_t *mon, struct instruction_t *instr, size_t len, arith_kind_t kind)
{
     bit32u_t value, retval;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     value = Monitor_read_reg_or_mem(mon, instr, len);
     retval = eval2(kind, value);
     Monitor_write_reg_or_mem(mon, instr, retval, len);

     skip_instr(mon, instr);
}

static void add(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate) { arith(mon, instr, len, ARITH_ADD, use_immediate); }
static void sub(struct mon_t *mon, struct instruction_t *instr, size_t len, bool_t use_immediate) { arith(mon, instr, len, ARITH_SUB, use_immediate); }
static void add2(struct mon_t *mon, struct instruction_t *instr, size_t len) { arith2(mon, instr, len, ARITH_ADD); }
static void sub2(struct mon_t *mon, struct instruction_t *instr, size_t len) { arith2(mon, instr, len, ARITH_SUB); }
static void neg(struct mon_t *mon, struct instruction_t *instr, size_t len) { arith3(mon, instr, len, ARITH_NEG); }


/* Add */
void add_eb_gb(struct mon_t *mon, struct instruction_t *instr) { add(mon, instr, 1, FALSE); }
void add_ed_gd(struct mon_t *mon, struct instruction_t *instr) { add(mon, instr, 4, FALSE); }
void add_eb_ib(struct mon_t *mon, struct instruction_t *instr) { add(mon, instr, 1, TRUE); }
void add_ed_id(struct mon_t *mon, struct instruction_t *instr) { add(mon, instr, 4, TRUE); }
void add_gb_eb(struct mon_t *mon, struct instruction_t *instr) { add2(mon, instr, 1); }
void add_gd_ed(struct mon_t *mon, struct instruction_t *instr) { add2(mon, instr, 4); }

/* Subtract */
void sub_eb_gb(struct mon_t *mon, struct instruction_t *instr) { sub(mon, instr, 1, FALSE); }
void sub_ed_gd(struct mon_t *mon, struct instruction_t *instr) { sub(mon, instr, 4, FALSE); }
void sub_eb_ib(struct mon_t *mon, struct instruction_t *instr) { sub(mon, instr, 1, TRUE); }
void sub_ed_id(struct mon_t *mon, struct instruction_t *instr) { sub(mon, instr, 4, TRUE); }
void sub_gb_eb(struct mon_t *mon, struct instruction_t *instr) { sub2(mon, instr, 1); }
void sub_gd_ed(struct mon_t *mon, struct instruction_t *instr) { sub2(mon, instr, 4); }

void neg_eb(struct mon_t *mon, struct instruction_t *instr) { neg(mon, instr, 1); }
void neg_ed(struct mon_t *mon, struct instruction_t *instr) { neg(mon, instr, 4); }

/* Integer Subtraction with Borrow */
void sbb_eb_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void sbb_ed_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Add with Carry */
void adc_eb_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void adc_ed_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Compare Two Operands */
void cmp_eb_gb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void cmp_ed_gd(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

void cmp_gb_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void cmp_gd_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void cmp_eb_ib(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void cmp_ed_id(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Exchange and Add */
void xadd_eb_gb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void xadd_ed_gd(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Unsigned Multiply */
void mul_al_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void mul_eax_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Signed Multiply */
void imul_al_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void imul_eax_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Unsigned Divide */
void div_al_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void div_eax_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/* Signed Divide */
void idiv_al_eb(struct mon_t *mon, struct instruction_t *instr) { assert(0); }
void idiv_eax_ed(struct mon_t *mon, struct instruction_t *instr) { assert(0); }

/****************************************************************/

void
cmpxchg(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     bit32u_t dest;
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     dest = Monitor_read_reg_or_mem(mon, instr, len);
     
     if (dest == UserRegs_get2(&mon->regs->user, GEN_REG_EAX, len)) {
	  bit32u_t src = UserRegs_get2(&mon->regs->user, instr->reg, len);
	  Monitor_write_reg_or_mem(mon, instr, src, len);
     } else {
	  UserRegs_set2(&mon->regs->user, GEN_REG_EAX, dest, len);	  
     }

     /* [TODO] update flags */

     skip_instr(mon, instr);
}

struct mem_access_t *
cmpxchg_mem(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     struct mem_access_t *maccess;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
     
     if (instr->mod == 3) {
	     Print ( stderr, "cmpxchg_mem: none\n" );
	     return NULL;
     }

     maccess = MemAccess_create_read_resolve(mon, instr, len);
     maccess->next = MemAccess_create_write_resolve(mon, instr, len);

     return maccess;     
}

void cmpxchg_eb_gb(struct mon_t *mon, struct instruction_t *instr) { cmpxchg(mon, instr, 1); }
void cmpxchg_ed_gd(struct mon_t *mon, struct instruction_t *instr) { cmpxchg(mon, instr, 4); }

struct mem_access_t *
cmpxchg_eb_gb_mem(struct mon_t *mon, struct instruction_t *instr)
{
	return cmpxchg_mem(mon, instr, 1);
}

struct mem_access_t *
cmpxchg_ed_gd_mem(struct mon_t *mon, struct instruction_t *instr)
{
	return cmpxchg_mem(mon, instr, 4);
}
