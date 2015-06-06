#ifndef _VMM_IA32_INTERRUPT_H
#define _VMM_IA32_INTERRUPT_H

#include "vmm/ia32/interrupt_common.h"

enum {
     INVALID_SYSCALL_NO = 0xffffffff
};

enum {
     OPCODE_INT		= 0xcd
};

enum {
     MAX_OF_IVECTOR	= 256
};

enum {
     IRQ_INVALID	= -1,
     IRQ_TIMER		= 0x00,
     IRQ_SYSTEM1	= 0x05, // free (SOUND_CARD?)
     IRQ_SYSTEM2	= 0x09,  // free (NIC?)
};

#define IRQ_EIDE(x)		(0x0e + (x))
#define IRQ_COM(x)		(0x03 + (x) % 2)


const char *DeliveryMode_to_string(delivery_mode_t x);
void InterruptCommand_print(FILE *stream, struct interrupt_command_t *x);
void INTERRUPT_COMMAND_DPRINT(struct interrupt_command_t *x);
struct interrupt_command_t InterruptCommand_of_bit64u(bit32u_t vals[2]);

unsigned int irq_to_ivector(unsigned int irq);
void   enable_interrupt(struct regs_t *regs);
void   disable_interrupt(struct regs_t *regs);
bool_t interrupt_is_enabled(struct regs_t *regs);
bool_t interrupt_is_disabled(struct regs_t *regs);
bool_t interrupt_has_error_code(int ivector);
void   raise_interrupt(struct regs_t *regs, 
		       unsigned int ivector, 
		       void (*pushl)(struct regs_t *, bit32u_t),
		       trans_t *laddr_to_raddr);


void check_interrupt_raise2 ( struct regs_t *regs, bit32u_t eip, bit32u_t cs_raw, bit32u_t eflags );

#endif /* _VMM_IA32_INTERRUPT_H */
