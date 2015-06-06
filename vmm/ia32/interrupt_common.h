#ifndef _VMM_IA32_INTERRUPT_COMMON_H
#define _VMM_IA32_INTERRUPT_COMMON_H

#include "vmm/std.h"

enum {
     IVECTOR_BREAKPOINT			= 0x03,
     IVECTOR_DEVICE_NOT_AVAILABLE 	= 0x07,
     IVECTOR_PAGEFAULT 			= 0x0e,
     IVECTOR_SYSCALL 			= 0x80
};

enum {
     ERROR_ILLEGAL_REGISTER_ADDR   	= 0x80,
     ERROR_RECEIVED_ILLEGAL_VEC  	= 0x40,
     ERROR_SEND_ILLEGAL_VEC  		= 0x20,
     ERROR_RECEIVED_ACCEPT   		= 0x08,
     ERROR_SEND_ACCEPT   		= 0x04,
     ERROR_RECEIVED_CHECKSUM     	= 0x02,
     ERROR_SEND_CHECKSUM     		= 0x01
};

enum delivery_mode {
     DELIVERY_MODE_FIXED		= 0,
     DELIVERY_MODE_LOWEST_PRIORITY	= 1,
     DELIVERY_MODE_SMI			= 2,
     DELIVERY_MODE_NMI			= 4,
     DELIVERY_MODE_INIT			= 5,
     DELIVERY_MODE_STARTUP		= 6,
     DELIVERY_MODE_EXTINT		= 7
};
typedef enum delivery_mode	delivery_mode_t;

enum dest_mode {
     DEST_MODE_PHYSICAL		= 0,
     DEST_MODE_LOGICAL		= 1,
};
typedef enum dest_mode		dest_mode_t;

enum level {
     LEVEL_DEASSERT		= 0,
     LEVEL_ASSERT		= 1
};
typedef enum level		level_t;

enum trig_mode {
     TRIG_MODE_EDGE 		= 0,
     TRIG_MODE_LEVEL 		= 1
};
typedef enum trig_mode		trig_mode_t;

enum dest_shorthand {
     DEST_SHORTHAND_NO			= 0,
     DEST_SHORTHAND_SELF		= 1,
     DEST_SHORTHAND_ALL_INCLUDING_SELF	= 2,
     DEST_SHORTHAND_ALL_EXCLUDING_SELF	= 3
};
typedef enum dest_shorthand	dest_shorthand_t;

struct interrupt_command_t {
     bit8u_t		vector;
     delivery_mode_t	delivery_mode;
     dest_mode_t	dest_mode;
     level_t		level;
     trig_mode_t	trig_mode;
     dest_shorthand_t	dest_shorthand;
     bit8u_t		dest;
};

#endif /* _VMM_IA32_INTERRUPT_COMMON_H */

