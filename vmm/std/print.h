#ifndef _VMM_STD_PRINT_H
#define _VMM_STD_PRINT_H

#include "vmm/std/print_common.h"
#include <stdarg.h>

void Print ( FILE *stream, const char *fmt, ... );
void Printv ( FILE *stream, const char *fmt, va_list ap );
void Print_newline ( FILE *stream );
void Print_color ( FILE *stream, color_t color, const char *fmt, ... );
void Printv_color ( FILE *stream, color_t color, const char *fmt, va_list ap );

#endif /* _VMM_STD_PRINT_H */
