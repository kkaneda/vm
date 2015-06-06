#ifndef _VMM_MON_PIC_H
#define _VMM_MON_PIC_H


#include "vmm/common.h"


#ifndef INTERRUPT_CONTROLLER1_PORT
# define INTERRUPT_CONTROLLER1_PORT( x )	 ( 0x20 + ( x ) )
#endif

#ifndef INTERRUPT_CONTROLLER2_PORT
# define INTERRUPT_CONTROLLER2_PORT( x )	 ( 0xa0 + ( x ) )
#endif



enum pic_kind {
	PIC_KIND_MASTER = 0,
	PIC_KIND_SLAVE  = 1
};
typedef enum pic_kind	 pic_kind_t;

struct pic_state_t {
	pic_kind_t	kind;

	bit8u_t 	irr; /* Interrupt Request Register */
	bit8u_t 	isr; /* In-Service Register */
	bit8u_t 	imr; /* Interrupt Mask Register */
	bool_t 		read_reg_select;

	bit8u_t		init_state;
	bool_t		init4; /* true if 4 byte init */

	bool_t		poll;

	bool_t 		last_irr;

	bit8u_t 	irq_base;
	bit8u_t 	priority_add; /* highest irq priority */

	bool_t auto_eoi;
	bool_t special_fully_nested_mode;
	bool_t rotate_on_auto_eoi;
	bool_t special_mask;
};

enum {
	NR_PIC_STATES = 2
};

struct pic_t {
	struct pic_state_t states[NR_PIC_STATES];
};


void     Pic_init ( struct pic_t *x );
void     Pic_pack ( struct pic_t *x, int fd );
void     Pic_unpack ( struct pic_t *x, int fd );
bit32u_t Pic_read ( struct pic_t *x, bit16u_t addr, size_t len );
void     Pic_write ( struct pic_t *x, bit16u_t addr, bit32u_t val, size_t len );
bool_t   Pic_trigger ( struct pic_t *pic, int irq );
int      Pic_try_acknowledge_interrupt ( struct pic_t *pic );
bool_t   Pic_check_interrupt ( struct pic_t *pic );

#endif /*_VMM_MON_PIC_H */

