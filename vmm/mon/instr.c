#include "vmm/mon/mon.h"

void 
MemAccess_destroy ( struct mem_access_t *x )
{
	ASSERT ( x != NULL );
	Free ( x );
}

void 
MemAccess_destroy_all ( struct mem_access_t *x )
{
	struct mem_access_t *p;

	ASSERT ( x != NULL );

	p = x;
	while ( p != NULL ) {
		struct mem_access_t *next = p->next;
		MemAccess_destroy ( p );
		p = next;
	}
}

struct mem_access_t *
MemAccess_create_read ( seg_reg_index_t sreg_index, bit32u_t vaddr, size_t len )
{
	struct mem_access_t *x;

	x = Malloct ( struct mem_access_t );
	x->sreg_index = sreg_index;
	x->vaddr = vaddr;
	x->len = len;
	x->kind = MEM_ACCESS_READ;
	x->next = NULL;

	return x;
}

struct mem_access_t *
MemAccess_create_write ( seg_reg_index_t sreg_index, bit32u_t vaddr, size_t len )
{
	struct mem_access_t *x;

	x = Malloct ( struct mem_access_t );
	x->sreg_index = sreg_index;
	x->vaddr = vaddr;
	x->len = len;
	x->kind = MEM_ACCESS_WRITE;
	x->next = NULL;

	return x;
}

struct mem_access_t *
MemAccess_create_read_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( instr->mod != 3 );
	return MemAccess_create_read ( instr->sreg_index, 
				   instr->resolve ( instr, &mon->regs->user ), 
				   len );
}

struct mem_access_t *
MemAccess_create_write_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	assert ( instr->mod != 3 );

	return MemAccess_create_write ( instr->sreg_index, 
				   instr->resolve ( instr, &mon->regs->user ), 
				   len );
}

struct mem_access_t *
MemAccess_create_read_write_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	struct mem_access_t *maccess;

	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	if ( instr->mod == 3 ) { return NULL; }

	maccess = MemAccess_create_read_resolve ( mon, instr, len );
	maccess->next = MemAccess_create_write_resolve ( mon, instr, len );
	return maccess;
}

struct mem_access_t *
MemAccess_create_read_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return ( instr->mod == 3 ) ? NULL : MemAccess_create_read_resolve ( mon, instr, len );
}

struct mem_access_t *
MemAccess_create_write_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return ( instr->mod == 3 ) ? NULL : MemAccess_create_write_resolve ( mon, instr, len );
}

struct mem_access_t *
MemAccess_create_read_write_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	return ( instr->mod == 3 ) ? NULL : MemAccess_create_read_write_resolve ( mon, instr, len );
}

struct mem_access_t *
MemAccess_push ( struct mon_t *mon, size_t len )
{
	ASSERT ( mon != NULL );
	return MemAccess_create_write ( SEG_REG_SS, 
					mon->regs->user.esp - len, 
					len );
}

struct mem_access_t *
MemAccess_pop ( struct mon_t *mon, size_t len )
{
	ASSERT ( mon != NULL );
	return MemAccess_create_read ( SEG_REG_SS, mon->regs->user.esp, len );
}

void
MemAccess_print ( struct mem_access_t *x )
{
	ASSERT ( x != NULL );
	DPRINT ( "{ sreg=%s, vaddr=%#x, len=%d, kind=%s }\n",
	    SegRegIndex_to_string ( x->sreg_index ),
	    x->vaddr,
	    x->len,
	    ( x->kind == MEM_ACCESS_READ ) ? "R" : "W" );  
}

struct mem_access_t *
MemAccess_add_instruction_fetch ( struct mem_access_t *maccess, size_t ilen, bit32u_t eip )
{
	struct mem_access_t *x;

	x = MemAccess_create_read ( SEG_REG_CS, eip, ilen );
	x->next = maccess;
	return x;
}

void
Instruction_init ( struct instruction_t *x )
{
	ASSERT ( x != NULL );
	x->opcode = INVALID_OPCODE;
	x->name = "";
	x->resolve = NULL;
	x->execute = NULL;
	x->maccess = NULL;
	x->is_sensitive = FALSE;
	x->mod = -1;
	x->reg = -1;
	x->rm = -1;

	x->sib_scale = -1;
	x->sib_index = -1;

	x->opcode_reg = -1;

	x->immediate[0] = x->immediate[1] = 0;
	x->disp = 0;
	x->sreg_index = SEG_REG_NULL;
	x->opsize_override = FALSE;
	x->rep_repe_repz = FALSE;
	x->repne_repnz = FALSE;

	x->is_locked = FALSE;
	x->len = 0;
}

void
Instruction_print ( FILE *stream, struct instruction_t *x )
{
	ASSERT ( x != NULL );
	Print ( stream, 
		" { name=%s, opcode= %#x, execute=%p, mod=%#x, reg=%#x, rm=%#x, immediate[0]=%#x, immediate[1]=%#x, disp=%#x, sreg_index=%#x, is_locked=%d, len=%#x }\n", 
		x->name, 
		x->opcode, 
		x->execute,
		x->mod, x->reg, x->rm,
		x->immediate[0],
		x->immediate[1],
		x->disp,
		x->sreg_index,
		x->is_locked,
		x->len );
}

void
dummy_instr ( struct mon_t *mon, struct instruction_t *instr )
{
	assert ( 0 );
}

void
skip_instr ( struct mon_t *mon, struct instruction_t *instr )
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );
	mon->regs->user.eip += instr->len;
}

bit32u_t
get_rep_count ( struct mon_t *mon, struct instruction_t *instr ) 
{
	ASSERT ( mon != NULL );
	ASSERT ( instr != NULL );

	return ( ( ( instr->rep_repe_repz ) || ( instr->repne_repnz ) ) ? 
		 mon->regs->user.ecx : /* address_size == 16 の時は cx */
		 1 );
}

struct mem_access_t *
maccess_none ( struct mon_t *mon, struct instruction_t *instr )
{
	return NULL;
}

struct mem_access_t *
maccess_rdonly_byte ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve_if_not_mod3 ( mon, instr, 1 );
}

struct mem_access_t *
maccess_rdonly_word ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_resolve_if_not_mod3 ( mon, instr, 2 );
}

struct mem_access_t *
maccess_rdonly_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	size_t len = ( instr->opsize_override ) ? 2 : 4;
	return MemAccess_create_read_resolve_if_not_mod3 ( mon, instr, len );
}

struct mem_access_t *
maccess_wronly_byte ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve_if_not_mod3 ( mon, instr, 1 );
}
struct mem_access_t *
maccess_wronly_word ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve_if_not_mod3 ( mon, instr, 2 );
}

struct mem_access_t *
maccess_wronly_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_write_resolve_if_not_mod3 ( mon, instr, 4 );
}

struct mem_access_t *
maccess_rdwr_byte ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_write_resolve_if_not_mod3 ( mon, instr, 1 );
}

struct mem_access_t *
maccess_rdwr_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_create_read_write_resolve_if_not_mod3 ( mon, instr, 4 );
}

struct mem_access_t *
maccess_push_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_push ( mon, 4 );
}

struct mem_access_t *
maccess_pop_dword ( struct mon_t *mon, struct instruction_t *instr )
{
	return MemAccess_pop ( mon, 4 );
}



