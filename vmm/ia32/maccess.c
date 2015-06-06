#include "vmm/ia32/maccess_common.h"


static const bit32u_t HARDWARE_ADDR_RANGE[2] = { 0xa0000, 0x100000 };
static const bit32u_t VGA_ADDR_RANGE[2] = { 0xb0000, 0xc0000 };


/* [TODO] consider access length */
static bool_t
addr_is_in_range(bit32u_t addr, const bit32u_t range[2])
{
     return ((addr >= range[0]) && (addr < range[1]));
}

bool_t
is_hardware_reserved_region(bit32u_t paddr)
{
     return (addr_is_in_range(paddr, HARDWARE_ADDR_RANGE));
}

bool_t
is_vga_region(bit32u_t paddr)
{
     return (addr_is_in_range(paddr, VGA_ADDR_RANGE));
}

const char*
MemAccessKind_to_string ( mem_access_kind_t x )
{
	switch ( x ) {
	case MEM_ACCESS_READ: 
		return "Read";
	case MEM_ACCESS_WRITE:
		return "Write";
	default:
		Match_failure ( "MemAccessKind_to_string" );
	}
	return NULL;
}
