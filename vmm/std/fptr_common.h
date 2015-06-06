#ifndef _VMM_STD_FPTR_COMMON_H
#define _VMM_STD_FPTR_COMMON_H

#include "vmm/std/types.h"

/* fat pointer */
struct fptr_t {
	void 	*base;
	size_t 	offset;
};

#endif /* _VMM_STD_FPTR_COMMON_H */
