#ifndef _VMM_STD_FPTR_H
#define _VMM_STD_FPTR_H

#include "vmm/std/fptr_common.h"

struct fptr_t Fptr_create ( const void *base, size_t offset );
void          Fptr_destroy ( struct fptr_t x );
struct fptr_t Fptr_alloc ( size_t size );
struct fptr_t Fptr_null ( void );
void          Fptr_zero ( struct fptr_t x );
bool_t        Fptr_is_null ( const struct fptr_t x );
void          Fptr_print ( FILE *stream, const struct fptr_t x );

#endif /* _VMM_STD_FPTR_H */
