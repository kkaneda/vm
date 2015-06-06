#ifndef _VMM_IA32_REGS_COMMON_H
#define _VMM_IA32_REGS_COMMON_H


#include "vmm/std.h"
#include "vmm/ia32/descr.h"
#include "vmm/ia32/maccess.h"


enum {
	NUM_OF_GEN_REGS		= 6,
	NUM_OF_DEBUG_REGS	= 8
};

/* General-purpose register Index */
enum gen_reg_index {
	GEN_REG_EAX = 0,
	GEN_REG_ECX = 1,
	GEN_REG_EDX = 2,
	GEN_REG_EBX = 3,
	GEN_REG_ESP = 4,
	GEN_REG_EBP = 5, 
	GEN_REG_ESI = 6,
	GEN_REG_EDI = 7
};
typedef enum gen_reg_index	gen_reg_index_t;

enum {
	NUM_OF_SEG_REGS	= 6
};

/* Seg registers */
enum seg_reg_index {
	SEG_REG_NULL	= -1,
	SEG_REG_ES 	= 0,
	SEG_REG_CS	= 1,
	SEG_REG_SS 	= 2,
	SEG_REG_DS	= 3,
	SEG_REG_FS	= 4,
	SEG_REG_GS	= 5,

	SEG_REG_LDTR	= 6,
	SEG_REG_TR	= 7,
};
typedef enum seg_reg_index	seg_reg_index_t;


/* Flag Register */
struct flag_reg_t {
	bit32u_t 		val;
	bool_t			interrupt_enable_flag;
	bool_t			direction_flag;
	privilege_level_t	iopl;
	bool_t			nested_task;
	bool_t			virtual_8086_mode;
	bool_t 			virtual_interrupt_pending;
};

enum tbl_indicator {
	TBL_INDICATOR_GDT = 0,
	TBL_INDICATOR_LDT = 1
};
typedef enum tbl_indicator	tbl_indicator_t;


/* Segment Selector 
 * [Refernece] IA-32 manual. Vol.3 3-7 */
struct seg_selector_t {
	bit13u_t 		index;   // bits 3 throught 15 
	tbl_indicator_t		tbl_indicator;     // bit 2
	privilege_level_t	rpl; // requested privilege level 
};

/* Segment Register */
struct seg_reg_t {
	struct seg_selector_t	selector;
	struct descr_t		cache;
	bit16u_t 		val;
};

/* Global Segment Register */
struct global_seg_reg_t {
	bit32u_t 		base;
	bit16u_t		limit;
};

/* CR0 register 
 * [Refernece] IA-32 manual. Vol.3 2-12 */
struct cr0_t {
	bit32u_t		val;
	bool_t			protection_enable; 
	bool_t			task_switched;
	bool_t			paging; 
};

struct cr3_t {
	bit32u_t		val;
	bit12u_t		base;
};

struct cr4_t {
	bit32u_t		val;
	bool_t			page_global_enable;
	bool_t			physical_address_extension;
	bool_t			page_size_extension;
};

/* System Registers */
struct sys_regs_t {
	struct cr0_t		cr0;	 /* control registers */
	bit32u_t		cr1;
	bit32u_t		cr2;
	struct cr3_t		cr3;
	struct cr4_t		cr4;

	struct global_seg_reg_t gdtr;	/* global desctptor table register */
	struct global_seg_reg_t idtr;	/* interrupt desctptor table register */

	struct seg_reg_t 	ldtr;	/* local desctptor table register */
	struct seg_reg_t 	tr;	/* task register */
};

/* Register Map */
struct regs_t {
	struct user_regs_struct user; /* general-purpose registers and 
				       * instruction pointer register (eip)
				       * [Note] Do not use eflags in this variable */
	struct flag_reg_t	eflags;	/* flag register */     
	struct seg_reg_t	segs[NUM_OF_SEG_REGS]; /* segment register */
	bit32u_t		debugs[NUM_OF_DEBUG_REGS];
	struct sys_regs_t	sys;
};

#endif /* _VMM_IA32_REGS_COMMON_H */
