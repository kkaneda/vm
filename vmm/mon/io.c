#include "vmm/mon/mon.h"

/* Input to Port */
void
in_al_ib(struct mon_t *mon, struct instruction_t *instr)
{
     bit8u_t imm8;
     bit8u_t al;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);
    
     imm8 = instr->immediate[0];
     al = inp(mon, imm8, 1);

     UserRegs_set2(&mon->regs->user, GEN_REG_EAX, al, 1);
     
     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

/* Input to Port */
void
in_al_dx(struct mon_t *mon, struct instruction_t *instr)
{
     bit16u_t dx;
     bit8u_t al;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);
     al = inp(mon, dx, 1);
     UserRegs_set2(&mon->regs->user, GEN_REG_EAX, al, 1);

     /* [TODO] check privilege */

     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

/* Input to Port */
void
in_eax_dx(struct mon_t *mon, struct instruction_t *instr)
{
     bit32u_t val;
     bit16u_t dx;
     size_t len;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     len = (instr->opsize_override) ? 2 : 4;

     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);
     val = inp(mon, dx, len);
     UserRegs_set2(&mon->regs->user, GEN_REG_EAX, val, len);

//     Print ( stderr, "inp: addr=%#x, val=%#x, len=%#x\n", dx, val, len );

     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

/* Output to Port */
void
out_ib_al(struct mon_t *mon, struct instruction_t *instr)
{
     bit8u_t imm8;
     bit8u_t al;
     
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     imm8 = instr->immediate[0];

     al = UserRegs_get2(&mon->regs->user, GEN_REG_EAX, 1);
     
     /* [TODO] check privilege */

     outp(mon, imm8, al, 1);

     skip_instr(mon, instr); 
     mon->stat.nr_io++;
}

/* Output to Port */
void
out_dx_al(struct mon_t *mon, struct instruction_t *instr)
{
     bit16u_t dx;
     bit8u_t al;
     
     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);
     al = UserRegs_get2(&mon->regs->user, GEN_REG_EAX, 1);
     
     /* [TODO] check privilege */

     outp(mon, dx, al, 1);

     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

/* Output to Port */
void
out_dx_eax(struct mon_t *mon, struct instruction_t *instr)
{
     bit16u_t dx;
     bit32u_t val;
     size_t len;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     len = (instr->opsize_override) ? 2 : 4;

     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);
     val = UserRegs_get2(&mon->regs->user, GEN_REG_EAX, len);
//     Print ( stderr, "outp: addr=%#x, val=%#x, len=%#x\n", dx, val, len );
     outp(mon, dx, val, len);

     skip_instr(mon, instr);
}

/* Input from Port to String */
static void
ins(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     bit16u_t dx;
     int i;
     bit32u_t count, delta;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);

     delta = FlagReg_get_delta ( &mon->regs->eflags, len );
     count = get_rep_count(mon, instr);
     for (i = 0; i < count; i++) {
	  bit32u_t val;

	  val = inp(mon, dx, len);
	  Monitor_write_with_vaddr(SEG_REG_ES, mon->regs->user.edi, val, len);

	  mon->regs->user.edi += delta;
     }

     if (instr->rep_repe_repz)
	     mon->regs->user.ecx = 0;

     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

void insb_yb_dx(struct mon_t *mon, struct instruction_t *instr) { ins(mon, instr, 1); }
void insw_yv_dx(struct mon_t *mon, struct instruction_t *instr) { ins(mon, instr, (instr->opsize_override) ? 2 : 4); }

static struct mem_access_t *
ins_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len ) 
{
	struct mem_access_t *maccess = NULL;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t vaddr;
		struct mem_access_t *x;

		vaddr = mon->regs->user.edi + ( delta * i );

		x = MemAccess_create_write ( SEG_REG_ES, vaddr, len );

		/* add <x> to <maccess> */
		if ( maccess == NULL ) {
			maccess = x;
		} else {
			x->next = maccess;
			maccess = x;
		}
	}

	return maccess;   
}

struct mem_access_t *
insb_yb_dx_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{
	return ins_mem ( mon, instr, 1 );
}

struct mem_access_t *
insw_yv_dx_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{
	return ins_mem ( mon, instr, (instr->opsize_override) ? 2 : 4 );
}


/* Output String to Port */
static void
outs(struct mon_t *mon, struct instruction_t *instr, size_t len)
{
     seg_reg_index_t seg;
     bit16u_t dx;
     int i;
     bit32u_t count, delta;

     ASSERT(mon != NULL);
     ASSERT(instr != NULL);

     seg = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;     
     dx = UserRegs_get2(&mon->regs->user, GEN_REG_EDX, 2);

     delta = FlagReg_get_delta ( &mon->regs->eflags, len );
     count = get_rep_count(mon, instr);
     for (i = 0; i < count; i++) {
	  bit32u_t val;

	  val = Monitor_read_with_vaddr(seg, mon->regs->user.esi, len);
	  outp(mon, dx, val, len);
	  mon->regs->user.esi += delta;
     }

     if (instr->rep_repe_repz)
	     mon->regs->user.ecx = 0;

     skip_instr(mon, instr);
     mon->stat.nr_io++;
}

void outsb_dx_xb(struct mon_t *mon, struct instruction_t *instr) { outs(mon, instr, 1); }
void outsw_dx_xv(struct mon_t *mon, struct instruction_t *instr) { outs(mon, instr, (instr->opsize_override) ? 2 : 4); }

static struct mem_access_t *
outs_mem ( struct mon_t *mon, struct instruction_t *instr, size_t len ) 
{
	struct mem_access_t *maccess = NULL;
	seg_reg_index_t seg;
	bit32u_t i, count, delta;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	seg = (instr->sreg_index != SEG_REG_NULL) ? instr->sreg_index : SEG_REG_DS;     
	delta = FlagReg_get_delta ( &mon->regs->eflags, len );
	count = get_rep_count ( mon, instr );
	for ( i = 0; i < count; i++ ) {
		bit32u_t vaddr;
		struct mem_access_t *x;

		vaddr = mon->regs->user.esi + ( delta * i );

		x = MemAccess_create_read ( seg, vaddr, len );

		/* add <x>to <maccess> */
		if ( maccess == NULL ) {
			maccess = x;
		} else {
			x->next = maccess;
			maccess = x;
		}
	}

	return maccess;   
}

struct mem_access_t *
outsb_dx_xb_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{
	return outs_mem ( mon, instr, 1 );
}

struct mem_access_t *
outsw_dx_xv_mem ( struct mon_t *mon, struct instruction_t *instr ) 
{
	return outs_mem ( mon, instr, (instr->opsize_override) ? 2 : 4 );
}
