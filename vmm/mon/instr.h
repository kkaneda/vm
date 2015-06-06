#ifndef _VMM_MON_INSTR_H
#define _VMM_MON_INSTR_H

#include "vmm/common.h"

enum {
     INVALID_OPCODE = -1
};

struct instruction_t;
struct mon_t;

typedef bit32u_t resolve_t ( struct instruction_t *, struct user_regs_struct * );
typedef void execute_t ( struct mon_t *, struct instruction_t * );
typedef struct mem_access_t *mem_access_func_t ( struct mon_t *, struct instruction_t * );

struct mem_access_t {
     seg_reg_index_t		sreg_index;
     bit32u_t			vaddr;
     size_t			len;
     mem_access_kind_t		kind;
     struct mem_access_t 	*next;
};

void                 MemAccess_destroy ( struct mem_access_t *x );
void                 MemAccess_destroy_all  (  struct mem_access_t *x  );
struct mem_access_t *MemAccess_create_read ( seg_reg_index_t sreg_index, bit32u_t vaddr, size_t len );
struct mem_access_t *MemAccess_create_write ( seg_reg_index_t sreg_index, bit32u_t vaddr, size_t len );
struct mem_access_t *MemAccess_create_read_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len );
struct mem_access_t *MemAccess_create_write_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len );
struct mem_access_t *MemAccess_create_read_write_resolve ( struct mon_t *mon, struct instruction_t *instr, size_t len );

struct mem_access_t *MemAccess_create_read_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len );
struct mem_access_t *MemAccess_create_write_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len );
struct mem_access_t *MemAccess_create_read_write_resolve_if_not_mod3 ( struct mon_t *mon, struct instruction_t *instr, size_t len );

struct mem_access_t *MemAccess_push ( struct mon_t *mon, size_t len );
struct mem_access_t *MemAccess_pop ( struct mon_t *mon, size_t len );
void                MemAccess_print ( struct mem_access_t *x );
struct mem_access_t *MemAccess_add_instruction_fetch ( struct mem_access_t *maccess, size_t ilen, bit32u_t eip );


struct instruction_t {
     int			opcode;
     char			*name;
     bool_t			is_sensitive;

     size_t			len;

     int			mod;
     int			reg;
     int			rm;
     int			sib_scale;
     int			sib_index;
     int 			opcode_reg;
     seg_reg_index_t		sreg_index;	 /* Segment Register */
     bit32u_t			immediate[2];
     bit32u_t			disp;

     /* prefix options */
     bool_t			opsize_override;
     bool_t			is_locked;
     bool_t			rep_repe_repz;
     bool_t			repne_repnz;

     resolve_t			*resolve;
     execute_t 			*execute; /* The pointer to the function that
					   * emulates the instruction */
     mem_access_func_t		*maccess;
};

void Instruction_init ( struct instruction_t *x );
void Instruction_print  (  FILE *stream, struct instruction_t *x  );
void skip_instr ( struct mon_t *mon, struct instruction_t *instr );
void dummy_instr ( struct mon_t *mon, struct instruction_t *instr );
bit32u_t get_rep_count ( struct mon_t *mon, struct instruction_t *instr );

struct mem_access_t *maccess_none ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_rdonly_byte ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_rdonly_word ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_rdonly_dword ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_wronly_byte ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_wronly_word ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_wronly_dword ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_rdwr_byte ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_rdwr_dword ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_push_dword ( struct mon_t *mon, struct instruction_t *instr );
struct mem_access_t *maccess_pop_dword ( struct mon_t *mon, struct instruction_t *instr );

#endif /* _VMM_MON_INSTR_H */
