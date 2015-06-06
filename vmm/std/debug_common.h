#ifndef _VMM_STD_DEBUG_COMMON_H
#define _VMM_STD_DEBUG_COMMON_H

#include "vmm/std/types.h"

#if 0

#ifdef DEBUG
#  define ASSERT(prep)		assert ( prep )
#else /* !DEBUG */
#  define ASSERT(prep)
#endif /* DEBUG*/

#endif /* 0 */


#define ASSERT(prep)		assert ( prep )


#endif /* _VMM_STD_DEBUG_COMMON_H */
