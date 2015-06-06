#ifndef _VMM_STD_DEBUG_H
#define _VMM_STD_DEBUG_H

#include "vmm/std/debug_common.h"

void Warning ( const char *fmt, ... );
void Fatal_failure ( const char *fmt, ...) ;
void Sys_failure ( const char * s );
void Match_failure ( const char *fmt, ... );
void Noimp_failure ( const char *fmt, ... );

inline void DPRINT ( const char * fmt, ... );
inline void DPRINT2 ( const char * fmt, ... );

#endif /* _VMM_DEBUG_H */
