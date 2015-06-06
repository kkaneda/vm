#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "vmm/std.h"
#include "vmm/ia32.h"
#include "vmm/comm.h"


char *
Ttyname ( int fd )
{
	char *ret;
	
	ret = ttyname ( fd );
	
	if ( ret == NULL )
		Sys_failure ( "ttyname" );
	
	return ret;
}

void
Tcgetattr ( int fd, struct termios *termios_p )
{
	int ret;

	ASSERT ( termios_p != NULL );

	ret = tcgetattr ( fd, termios_p );
	
	if ( ret == -1 )
		Sys_failure ( "tcgetattr" );
}

void
Tcflush ( int fd, int queue_selector )
{
	int ret;

	ret = tcflush ( fd, queue_selector );
	
	if ( ret == -1 )
		Sys_failure ( "tcflush" );	
}

void
Tcsetattr ( int fd, int optional_actions, const struct termios *termios_p )
{
	int ret;

	ASSERT ( termios_p != NULL );

	ret = tcsetattr ( fd, optional_actions, termios_p );
	
	if ( ret == -1 )
		Sys_failure ( "tcsetattr" );

	/* [TODO] 
	 * tcsetattr() returns success if any of the requested changes
	 * could be successfully carried out.  Therefore, when making
	 * multiple changes it may be necessary to follow this call with
	 * a further call to tcgetattr() to check that all changes have
	 * been performed successfully.
	 */
}

static struct termios term_orig;
int tty_fd = -1;

static void
term_exit(void)
{
	Tcsetattr ( tty_fd, TCSANOW, &term_orig );
}

void
init_termios ( void )
{
	struct termios term_new;

#if 0 
	const char *tty_name;
	tty_name = Ttyname ( 0 );
	tty_fd = Open2 ( tty_name, O_RDWR, 600 );
#else
	tty_fd = 0;
#endif
	Tcgetattr ( tty_fd, &term_orig );

	Mmove( (caddr_t) &term_new, (caddr_t) &term_orig, sizeof(struct termios));

	term_new.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
			      |INLCR|IGNCR|ICRNL|IXON);
	term_new.c_oflag |= OPOST;

	term_new.c_lflag &= ~( ECHO | ECHONL | ICANON | IEXTEN );
#if 0
	/* handle Cntl-C */
	term_new.c_lflag &= ~ISIG;
#endif

	term_new.c_cflag &= ~( CSIZE | PARENB );
	term_new.c_cflag |= CS8;
	
	term_new.c_cc[VMIN] = 1;
	term_new.c_cc[VTIME] = 0;

	Tcsetattr ( tty_fd, TCSANOW, &term_new );

	atexit(term_exit);
}

static pid_t pids[NUM_OF_PROCS];

static void
kill_proc ( int cpuid, char *prog_name )
{
	enum { BUF_SIZE = 1024 };
	char host[BUF_SIZE];
//	char *argv[] = { "ssh", "-xf", host, 
//	                 "pkill", "-9", prog_name, NULL };
	char *argv[] = { "pkill", "-9", prog_name, NULL };
	int ret;

	snprintf ( host, BUF_SIZE, "sheep%02d", cpuid + 1 );

	ret = execvp ( argv[0], argv );
	assert ( ret == 0 );
	exit ( 1 );	
}

static void
__kill_all_procs ( char *prog_name )
{
	int i;

	Print_color ( stderr, GREEN, "KILL %s\n", prog_name );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		pid_t pid;

		pid= Fork ( );

		if ( pid == 0 ) {
			kill_proc ( i, prog_name );
		} 
		Waitpid ( pid, NULL, 0 );
	}
}

static void
kill_all_procs ( void )
{
	Print_color ( stderr, RED, "FINISH\n" );

	__kill_all_procs ( "vm" );
	__kill_all_procs ( "mon" );
}

static void
handle_sigint ( int sig )
{
	kill_all_procs ( );
}

static void
fork_proc ( int cpuid )
{
	enum { BUF_SIZE = 1024 };
	char ssh_option[BUF_SIZE];
	char id[BUF_SIZE];
	char host[BUF_SIZE];
	char *MON_PROGRAM = "/home/kaneda/vm/vmm/mon/mon";
	char *CONFIG_FILE = "/home/kaneda/vm/vmm/mon/config.txt";
	int ret;
	char *argv[9];
	int i = 0;

	if ( cpuid != BSP_CPUID ) {
	  argv[0] = "ssh";

	  snprintf ( ssh_option, BUF_SIZE, "-xf" );
	  argv[1] = ssh_option;

	  snprintf ( host, BUF_SIZE, "sheep%02d", cpuid + 1 );
	  argv[2] = host;
	  
	  i = 3;
	}

	argv[i] = MON_PROGRAM;

	argv[i+1] = "--config";
	argv[i+2] = CONFIG_FILE;

	snprintf ( id, BUF_SIZE, "%d", cpuid );
	argv[i+3] = "--id";
	argv[i+4] = id;

	argv[i+5] = NULL;

	ret = execvp ( argv[0], argv );
	assert ( ret == 0 );
	exit ( 1 );
}

int
main ( int argc, char *argv[] )
{
	int i;

	init_termios ( );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		pids[i] = Fork ( );

		if ( pids[i] == 0 ) {
			fork_proc ( i );
		}
	}

	atexit ( &kill_all_procs );

	Signal ( SIGINT, &handle_sigint );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		Waitpid ( pids[i], NULL, 0 );
	}

	return 0;
}

