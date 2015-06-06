#ifndef _VMM_STD_UNIX_H
#define _VMM_STD_UNIX_H

#include "vmm/std/types.h"

pid_t        Fork ( void );
pid_t        Wait ( int *status );
pid_t        Waitpid ( pid_t pid, int *status, int options );
pid_t        Getpid ( void );
unsigned int Sleep ( unsigned int seconds );
char        *Getenv ( const char *name );

void Atexit ( void ( *function ) ( void ) );
void On_exit ( void ( *function )( int , void * ), void *arg );

void Get_utime_and_stime ( pid_t pid, unsigned long int *utime, unsigned long int *stime );

#endif /* _VMM_STD_UNIX_H */
