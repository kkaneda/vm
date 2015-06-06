#ifndef _VMM_STD_IN_ADDR_H
#define _VMM_STD_IN_ADDR_H

#include "vmm/std/types.h"
#include <netinet/in.h>

in_addr_t InAddr_of_string ( const char *s );
char     *InAddr_to_string ( in_addr_t x );
in_addr_t InAddr_resolve ( const char *name );
void      InAddr_print ( FILE *stream, in_addr_t x );
void      InAddr_println ( FILE *stream, in_addr_t x );

#endif /* _VMM_STD_IN_ADDR_H */
