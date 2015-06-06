#ifndef _VMM_STD_MEM_H
#define _VMM_STD_MEM_H

#include "vmm/std/types.h"

void  Mmove ( void *dest, const void *src, size_t n );
void *Memset ( void *s, int c, size_t n );
void *Memchr ( const void *s, int c, size_t n );
void *Mdup ( const void *s, size_t n );
void  Mzero ( void *s, size_t n );
void *Malloc ( size_t size );
void *Calloc ( size_t nmemb, size_t size );
void *Realloc ( void *ptr, size_t size );
void  Free ( void *ptr );
void *Mmap ( void *start, size_t length, int prot, int flags, int fd, off_t offset );
int   Munmap ( void *start, size_t length );
void  Mprotect ( const void *addr, size_t len, int prot );

/* malloc with cast */
#define Malloct( type ) 		 ( type * ) Malloc ( sizeof ( type ) )
#define Calloct( nmemb,type ) 	 ( type * ) Calloc ( nmemb, sizeof ( type ) )

#endif /* _VMM_STD_MEM_H */

