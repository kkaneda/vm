#ifndef _VMM_IA32_TASKING_H
#define _VMM_IA32_TASKING_H

#include "vmm/ia32/tasking_common.h"

struct tss_t Tss_create(struct seg_descr_t *tssd, trans_t *laddr_to_raddr);
struct tss_t get_tss_of_current_task(const struct regs_t *regs, trans_t *laddr_to_raddr);

#endif /* _VMM_IA32_TASKING_H */
