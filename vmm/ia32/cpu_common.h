#ifndef _VMM_IA32_CPU_COMMON_H
#define _VMM_IA32_CPU_COMMON_H

#include "vmm/std.h"

enum {
#ifdef ENABLE_MP
//	BSP_CPUID = 2
	BSP_CPUID = 0
#else
	BSP_CPUID = 0
#endif
};

enum proc_class {
	BOOTSTRAP_PROC,
	APPLICATION_PROC 
};
typedef enum proc_class		proc_class_t;

#endif /* _VMM_IA32_CPU_COMMON_H */
