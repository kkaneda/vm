#ifndef _VMM_IA32_MACCESS_H
#define _VMM_IA32_MACCESS_H

#include "vmm/std.h"
#include "vmm/ia32/maccess_common.h"

/*
 * [Terminology]
 * vaddr = virtual address = logical address
 * laddr = linear  adddress 
 * paddr = physical address
 * raddr = real address = the address where the guest/monitor process really accesses
 */

#define DEFINE_MEM_READ_FUNC(unit, ret_type) \
static inline ret_type \
read_ ## unit ( bit32u_t addr, trans_t *trans ) \
{ \
	bit32u_t raddr; \
\
	raddr = ( trans != NULL ) ? trans ( addr ) : addr; \
        return *((ret_type *)raddr); \
}

DEFINE_MEM_READ_FUNC(byte, bit8u_t)
DEFINE_MEM_READ_FUNC(word, bit16u_t)
DEFINE_MEM_READ_FUNC(dword, bit32u_t)


#define DEFINE_MEM_WRITE_FUNC(unit, value_type) \
static inline void \
write_ ## unit ( bit32u_t addr, value_type value, trans_t *trans ) \
{ \
	bit32u_t raddr; \
\
	raddr = ( trans != NULL ) ? trans ( addr ) : addr; \
        *((value_type *)raddr) = value; \
}

DEFINE_MEM_WRITE_FUNC(byte, bit8u_t)
DEFINE_MEM_WRITE_FUNC(word, bit16u_t)
DEFINE_MEM_WRITE_FUNC(dword, bit32u_t)

bool_t is_hardware_reserved_region(bit32u_t paddr);
bool_t is_vga_region(bit32u_t paddr);
const char*MemAccessKind_to_string ( mem_access_kind_t x );

#endif /* _VMM_IA32_MACCESS_H */
