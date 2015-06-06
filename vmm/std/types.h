#ifndef _VMM_STD_TYPES_H
#define _VMM_STD_TYPES_H

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>

#if HAVE_CONFIG_H
#include "vmm/config.h"
#endif

/* boolean type */
enum bool {
	TRUE 		= 1,
	FALSE 		= 0
};
typedef enum bool 	bool_t;

typedef unsigned long long int	bit64u_t;
typedef unsigned long long int	bit48u_t;
typedef unsigned long int	bit32u_t; /* = uint32_t */
typedef unsigned long int	bit20u_t; /* = uint32_t */
typedef unsigned short int	bit16u_t; /* = uint16_t */
typedef unsigned short int	bit14u_t;
typedef unsigned short int	bit13u_t;
typedef unsigned short int	bit12u_t;
typedef unsigned short int	bit10u_t;
typedef unsigned char		bit8u_t;
typedef unsigned char		bit5u_t;
typedef unsigned char		bit4u_t;


#endif /* _VMM_STD_TYPES_H */
