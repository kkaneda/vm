#ifndef _VMM_IA32_DESCR_H
#define _VMM_IA32_DESCR_H

#include "vmm/ia32/descr_common.h"


const char *        PrivilegeLevel_to_string(privilege_level_t x);

struct descr_t      Descr_of_bit64(bit32u_t vals[2]);
struct cd_seg_descr_t Descr_to_cd_seg_descr(struct descr_t *x);
struct gate_descr_t Descr_to_gate_descr(struct descr_t *x);
struct gate_descr_t Descr_to_interrupt_gate_descr(struct descr_t *x);
struct seg_descr_t  Descr_to_task_state_seg_descr(struct descr_t *x);
struct seg_descr_t  Descr_to_ldt_seg_descr(struct descr_t *x);
struct seg_descr_t  Descr_to_available_task_state_seg_descr(struct descr_t *x);
struct seg_descr_t  Descr_to_busy_task_state_seg_descr(struct descr_t *x);

void                Descr_print(FILE *stream, const struct descr_t *x);
void                DESCR_DPRINT(const struct descr_t *x);

bit32u_t            Descr_base(const struct descr_t *x);
bit16u_t            Descr_limit(const struct descr_t *x);
void                Descr_set_sys_descr_type(struct descr_t *x, sys_descr_type_t type);


#endif /* _VMM_IA32_DESCR_H */


