#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include "vmm/std/io.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>

pid_t 
Fork ( void )
{
	pid_t retval;

	retval = fork ( );

	if ( retval == -1 ) 
		Sys_failure ( "fork" );

	return retval;
}

pid_t 
Wait ( int *status )
{
	pid_t retval;

	retval = wait ( status );

	if ( retval == -1 ) 
		Sys_failure ( "wait" ); 

	return retval;
}

pid_t
Waitpid ( pid_t pid, int *status, int options )
{
	pid_t retval = -1;

	do {
		retval = waitpid ( pid, status, options );

		if ( ( retval == -1 ) && ( errno != EINTR ) )
			Sys_failure ( "waitpid" ); 

	} while  ( retval == -1 );

	return retval;
}

pid_t
Getpid ( void )
{
	return getpid ( );
}

unsigned int
Sleep ( unsigned int seconds )
{
	return sleep ( seconds );
}

char *
Getenv ( const char *name )
{
	char *retval;

	ASSERT ( name != NULL );

	retval = getenv ( name );

	if ( retval == NULL ) 
		Sys_failure ( "getenv" );

	return retval;	
}

void
Atexit ( void ( *function ) ( void ) )
{
	int retval;

	retval = atexit ( function );

	if ( retval != 0 ) 
		Sys_failure ( "atexit\n" );
}

void
On_exit ( void ( *function )( int , void * ), void *arg )
{
	int retval;

	retval = on_exit ( function, arg );

	if ( retval != 0 ) 
		Sys_failure ( "on_exit\n" );	
}

void 
Get_utime_and_stime ( pid_t pid, unsigned long int *utime, unsigned long int *stime )
{
	enum { BUFSIZE = 8192 };
	char  s [ BUFSIZE ], *p;
	int i, fd;
	
	ASSERT ( utime != NULL );
	ASSERT ( stime != NULL );

	fd = Open_fmt ( O_RDONLY , "/proc/%d/stat", pid );
	Read ( fd, s, BUFSIZE );
	Close ( fd );

	for ( i = 0; i < 13; i++ ) {
		p = strtok ( ( i == 0 ) ? s : NULL, " " );
		ASSERT ( s != NULL );
	}

	p = strtok ( NULL, " " );
	sscanf ( p, "%lu", utime );

	p = strtok ( NULL, " " );
	sscanf ( p, "%lu", stime );
}
