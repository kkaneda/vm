#ifndef _VMM_STD_STR_H
#define _VMM_STD_STR_H

#include "vmm/std/types.h"

char *Strdup ( const char *s );
void  Snprintf ( char *str, size_t size, const char *format, ... );
void  Vsnprintf ( char *str, size_t size, const char *format, va_list ap );
bool_t String_equal ( const char *s1, const char *s2 );

#endif /* _VMM_STD_STR_H */
