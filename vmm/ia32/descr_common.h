#ifndef _VMM_IA32_DESCR_COMMON_H
#define _VMM_IA32_DESCR_COMMON_H

#include "vmm/std.h"

enum privilege_level {
     SUPERVISOR_MODE = 0,
     USER_MODE = 3 
};
typedef enum privilege_level 	privilege_level_t;


enum {
     SIZE_OF_DESCR = 8
};

enum descr_type {
     CD_SEG_DESCR = 1,
     SYSTEM_DESCR = 0
};
typedef enum descr_type  descr_type_t;


enum sys_descr_type {
     INVALID_SEG	 	 	= 0,
     /* 16BIT_AVAILABLE_TSS		= 1, */
     LDT_SEG			 	= 2,
     /* 16BIT_BUSY_TASK_STATE_SEG	= 3, */
     /* 16BIT_CALL_GATE			= 4, */
     TASK_GATE 			 	= 5,
     /* 16BIT_INTERRUPT_GATE	 	= 6, */
     /* 16BIT_TRAP_GATE 		= 7, */
     /* ( reserved  )                   = 8, */
     AVAILABLE_TASK_STATE_SEG 		= 9,
     /* ( reserved )			= 10, */
     BUSY_TASK_STATE_SEG 		= 11,
     CALL_GATE		 		= 12,
     /* ( reserved ) 			= 13, */
     INTERRUPT_GATE		 	= 14,
     TRAP_GATE 				= 15
};
typedef enum sys_descr_type  	sys_descr_type_t;


struct seg_descr_t {
     bit32u_t  			base;          
     bit20u_t  			limit;
     
     bool_t 			present;
     privilege_level_t  	dpl;
     bool_t			granuality;
};

/* Code- or Data-segment Descriptor 
 * [Reference] IA-32 manual: Vol.3 3-10, 3-12
 */
struct cd_seg_descr_t {
     struct seg_descr_t		seg;

     bool_t 			executable;    /* 1=code, 0=data or stack segment */
     bool_t 			c_ed;          /* for code: 1=conforming,
					        * for data/stack: 1=expand down */
     bool_t 			read_write;    /* for code: readable?, 
						* for data/stack: writeable? */
     bool_t 			accessed;      /* accessed? */
};

/* [Reference] IA-32 manual: Vol.3 4-17 (call gate)
 * [Reference] IA-32 manual: Vol.3 5-14 (IDT gate) */
struct gate_descr_t {
     bool_t 			present;
     sys_descr_type_t 		type;
     privilege_level_t		dpl;

     bit16u_t			selector;
     bit32u_t			offset; /* except task_gate */
     bit5u_t			param_count;
};

struct sys_descr_t {
     sys_descr_type_t 		type;
     union {
	  struct seg_descr_t  	seg;  /* for LDT- or TS-segment */
	  struct gate_descr_t 	gate; /* for TASK-, TRAP-, CALL-, or INTERRPUT-GATE */
     } u;
};

struct descr_t {
     bit32u_t			vals[2];
     bool_t			is_valid;
     descr_type_t		type;
     union {
	  struct cd_seg_descr_t cd_seg;
	  struct sys_descr_t	sys;
     } u; 
};


#endif /* _VMM_IA32_DESCR_COMMON_H */
