#ifndef _VMM_IA32_MACCESS_COMMON_H
#define _VMM_IA32_MACCESS_COMMON_H

#include "vmm/std.h"

typedef bit32u_t trans_t(bit32u_t);

enum mem_access_kind {
     MEM_ACCESS_READ,
     MEM_ACCESS_WRITE
};
typedef enum mem_access_kind	mem_access_kind_t;

#endif /* _VMM_IA32_MACCESS_COMMON_H */
