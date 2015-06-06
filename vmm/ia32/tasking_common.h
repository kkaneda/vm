#ifndef _VMM_IA32_TASKING_COMMON_H
#define _VMM_IA32_TASKING_COMMON_H

#include "vmm/std.h"
#include "vmm/ia32/maccess.h"
#include "vmm/ia32/descr.h"
#include "vmm/ia32/regs.h"


enum {
	NUM_OF_TSS_ENTRIES = 4 /* = NUM_OF_PRIVILEGE_LEVELS */
};

/* Task State Segment (TSS) p
 * [Refernece] IA-32 manual Vol 3. 6-5 */
struct tss_t {
     bit16u_t 		previous_task_link;
     bit32u_t 		esp[NUM_OF_TSS_ENTRIES];
     bit16u_t 		ss[NUM_OF_TSS_ENTRIES];

     /* [TODO] ... */
};

#endif /* _VMM_IA32_TASKING_COMMON_H */
