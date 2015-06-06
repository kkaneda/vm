#include "vmm/std/debug.h"
#include "vmm/std/sig_common.h"
#include <signal.h>

bool_t
is_sigsegv ( int signo )
{
	return signo == SIGSEGV;
}

bool_t
is_sigstop ( int signo )
{
	return signo == SIGSTOP;
}

bool_t
is_sigalrm ( int signo )
{
	return signo == SIGALRM;
}

bool_t
is_sigusr2 ( int signo )
{
	return signo == SIGUSR2;
}

const char *
Signo_to_string ( int signo )
{
	switch  ( signo ) {
	case 0:		return " ( NULL )";
	case SIGHUP: 	return "SIGHUP"; 	/* 1 */
	case SIGINT: 	return "SIGINT";	/* 2 */
	case SIGQUIT: 	return "SIGQUIT";	/* 3 */
	case SIGILL: 	return "SIGILL";	/* 4 */
	case SIGTRAP:	return "SIGTRAP";	/* 5 */
	case SIGABRT: 	return "SIGABRT";	/* 6 */
	case SIGBUS: 	return "SIGBUS";	/* 7 */
	case SIGFPE: 	return "SIGFPE";	/* 8 */
	case SIGKILL: 	return "SIGKILL";	/* 9 */
	case SIGUSR1: 	return "SIGUSR1";	/* 10 */
	case SIGSEGV: 	return "SIGSEGV";	/* 11 */
	case SIGUSR2: 	return "SIGUSR2";	/* 12 */
	case SIGPIPE: 	return "SIGPIPE";	/* 13 */
	case SIGALRM: 	return "SIGALRM";	/* 14 */
	case SIGTERM: 	return "SIGTERM";	/* 15 */

	case SIGCHLD: 	return "SIGCHLD";	/* 17 */
	case SIGCONT: 	return "SIGCONT";	/* 18 */
	case SIGSTOP: 	return "SIGSTOP";	/* 19 */
	case SIGWINCH: 	return "SIGWINCH";	/* 28 */
	case SIGIO: 	return "SIGIO";		/* 29 */
	default: 	Warning ( "Signo_to_string: signo=%d does not match\n", signo ); return "UNKNOWN_SIGNAL";
	}
	Match_failure ( "Signo_to_string: signo=%d\n", signo );
	return "";
}

void
Kill ( pid_t pid, int sig )
{
	int retval;

	retval = kill ( pid, sig );

	if  ( retval == -1 ) 
		Sys_failure ( "kill" ); 
}

void
Raise ( int sig )
{
	int retval;

	retval = raise ( sig );

	if  ( retval != 0 ) 
		Sys_failure ( "raise" );
}

sigfunc_t * 
Signal ( int signo, sigfunc_t *func )
{
	int retval;
	struct sigaction  act, oact;

	ASSERT ( func != NULL );
  
	act.sa_handler = func;
	sigemptyset ( &act.sa_mask );
	act.sa_flags = 0;
	retval = sigaction ( signo, &act, &oact );

	return  ( retval == -1 ) ? SIG_ERR : oact.sa_handler;
}

void
Sigaction ( int signum, const struct sigaction *act, struct sigaction *oldact )
{
	int retval;

	ASSERT ( act != NULL );

	retval = sigaction ( signum, act, oldact );

	if  ( retval == -1 ) 
		Sys_failure ( "sigaction" );
}

void
Sigemptyset ( sigset_t *set )
{
	int retval;

	retval = sigemptyset ( set );

	if  ( retval == -1 ) 
		Sys_failure ( "sigemptyset" );
}
