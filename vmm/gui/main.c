#include "vmm/common.h"
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <curses.h>


WINDOW *
Initscr ( void )
{
	WINDOW *retval;
	
	retval = initscr ( );
	
	if ( retval == NULL )
		Fatal_failure ( "initscr\n" );
	
	return retval;
}

WINDOW *
Subwin ( WINDOW *orig, int nlines, int ncols, int begin_y, int begin_x )
{
	WINDOW *retval;

	retval = subwin ( orig, nlines, ncols, begin_y, begin_x );

	if ( retval == NULL )
		Fatal_failure ( "subwin\n" );

	return retval;
}

void
Endwin ( void )
{
	int retval;

	retval = endwin ( );

	if ( retval == ERR )
		Fatal_failure ( "endwin\n" );
}

void
Keypad ( WINDOW *win, bool bf )
{
	int retval;

	retval = keypad ( win, bf );

	if ( retval == ERR )
		Fatal_failure ( "keypad\n" );
}

void
Scrollok ( WINDOW *win, bool bf )
{
	int retval;
	
	retval = scrollok ( win, bf );
	
	if ( retval == ERR )
		Fatal_failure ( "scrollok\n" );
}

void
Nonl ( void )
{
	int retval;
	
	retval = nonl ( );
	
	if ( retval == ERR )
		Fatal_failure ( "nonl\n" );
}

void
Cbreak ( void )
{
	int retval;

	retval = cbreak ( );

	if ( retval == ERR )
		Fatal_failure ( "cbreak\n" );
}

void
Noecho ( void )
{
     int retval;

     retval = noecho ( );

     if ( retval == ERR )
	     Fatal_failure ( "noecho\n" );
}

void
Refresh ( void )
{
     int retval;

     retval = refresh ( );

     if ( retval == ERR )
	     Fatal_failure ( "refresh\n" );
}

void
Wrefresh ( WINDOW *win )
{
     int retval;

     retval = wrefresh ( win );

     if ( retval == ERR )
	     Fatal_failure ( "wrefresh\n" );
}

void
Mvaddch ( int y, int x, const chtype ch )
{
     int retval;

     retval = mvaddch ( y, x, ch );

     if ( retval == ERR ) 
	     Fatal_failure ( "mvaddch\n" );
}

void
Scroll ( WINDOW *win )
{
	int retval;

	retval = scroll ( win );

	if ( retval == ERR )
		Fatal_failure ( "scroll\n" );
}

void
Scrl ( int n )
{
	int retval;

	retval = scrl ( n );

	if ( retval == ERR )
		Fatal_failure ( "scrl\n" );
}

void
Wscrl ( WINDOW *win, int n )
{
     int retval;

     retval = wscrl (win, n );

     if ( retval == ERR )
	     Fatal_failure ( "wscrl\n" );
}

/****************************************************************/


const char *LOG_FILENAME = "/tmp/vram";


enum {
	VRAM_BASE_ADDR = 0xb0000,
	VRAM_OFFSET    = 0x10000
};

static int
open_vga_file (int cpuid )
{
	int fd;
	
	/* Repeat until the file is opened */
	for ( ; ; ) {
		fd = Open_fmt ( O_RDONLY, "/tmp/vga%d", cpuid );

		if ( fd >= 0 ) 
			break;

		Sleep ( 1 );
	}
	
	return fd;
}

static FILE *
open_log_file ( void )
{
	FILE *fp;
	
	fp = Fopen ( LOG_FILENAME, "w+" );
	
	return fp;
}

static void
finish ( void )
{
	Endwin();
}

static void
init_curses(void)
{
	Atexit ( finish );		/* arrange interrupts to terminate */
 
	Initscr ( ); 			/* initialize the curses library */
	Keypad ( stdscr, TRUE );	/* enable keyborad mapping */
	Scrollok ( stdscr, TRUE );
	Nonl ( );			/* tell curses not to do NL->CR/NL on output */
	Cbreak ( );			/* take input chars one at a time, no wait for \n */
	Noecho ( );			/* don't echo input */
}


enum {
	WINDOW_WIDTH = 80,
	WINDOW_HEIGHT = 25,
	
	SUB_WINDOW_BEGIN_X = 0,
	SUB_WINDOW_BEGIN_Y = 2,
	SUB_WINDOW_WIDTH = 80,
	SUB_WINDOW_HEIGHT = 21
};

static WINDOW *
create_subwin ( void )
{
	WINDOW *w;
	
	w = Subwin ( stdscr, 
		     SUB_WINDOW_HEIGHT, SUB_WINDOW_WIDTH, 
		     SUB_WINDOW_BEGIN_Y, SUB_WINDOW_BEGIN_X );
	return w;
}

static void
print_menu ( void )
{
	mvprintw ( 0, 24, "V I R T U A L    M A C H I N E " );
	mvhline ( 1, 0, ACS_HLINE, WINDOW_WIDTH );
	mvhline ( 23, 0, ACS_HLINE,  WINDOW_WIDTH );
	Refresh ( ) ;
}

static bit32u_t 
read_access_data ( int fd, bit32u_t *paddr, size_t *len )
{
	bit32u_t val;
	
	for ( ; ; ) {
		ssize_t n;

		n = read ( fd, paddr, sizeof(bit32u_t) );
		
		if ( n == -1 ) 
			Sys_failure ( "read" );
		
		if ( n > 0 )
			break;
	}

	Readn ( fd, len, sizeof ( size_t ) );
	Readn ( fd, &val, sizeof ( bit32u_t ) );

	return val;
}

struct point_t {
	int x, y;
};

enum {
	PADDR_LOWER = 0xb8000,
	PADDR_UPPER = 0xc0000
};

static struct point_t
paddr_to_pt ( bit32u_t paddr )
{

	struct point_t pt;
	bit32u_t p;
	
	p = ( paddr - PADDR_LOWER ) / 2;
	pt.x = p % WINDOW_WIDTH;
	pt.y = p / WINDOW_WIDTH;

	return pt;
}

static void
dump_char ( FILE *log_fp, char c, bit32u_t paddr )
{
	Print ( log_fp, "%c", c );
}

static void
print_char ( WINDOW *w, FILE *log_fp, bit32u_t paddr, int base_y, char c )
{
	static int nscr = 0;
	struct point_t pt;
	
	pt = paddr_to_pt ( paddr );
	pt.y += base_y;
	
	if  ( ! isprint ( c ) )
		return;
	
	if  ( pt.y - nscr >= SUB_WINDOW_HEIGHT ) {
		nscr++; 
		Scrollok ( w, TRUE );
		Scroll ( w );
		Scrollok ( w, FALSE );
	}
	
	mvwaddch ( w, pt.y - nscr, pt.x, c );
	Wrefresh ( w );
	
	dump_char ( log_fp, c, paddr );
}

int
main ( int argc, char *argv[], char *envp[] )
{
	WINDOW *subwin;
	bit32u_t prev_paddr = 0;
	int base_y = 0;	
	int cpuid;
	int fd;
	FILE *log_fp;
	
	if ( argc != 2 ) {
		Print ( stdout, "Usage: %s cpuid\n", argv[0] );
		exit ( 1 );
	}

	cpuid = Atoi ( argv[1] );

	fd = open_vga_file ( cpuid );

	log_fp = open_log_file (  );

	init_curses (  );
	print_menu (  );
	subwin = create_subwin (  );

	for ( ; ; ) {
		bit32u_t paddr;
		size_t len;
		int i;
		bit32u_t val;

		val = read_access_data ( fd, &paddr, &len );

		if ( paddr == PADDR_LOWER )
			base_y += ( PADDR_UPPER - PADDR_LOWER ) / WINDOW_WIDTH;
		prev_paddr = paddr;
		
		for ( i = 0; i < len; i += 2 ) {
			char c = SUB_BIT ( val, i * 8, 8 );
			print_char ( subwin, log_fp, paddr + i, base_y, c );
			 /*
			if ( isprint ( c ) ) {
				printf ( " < %#x> '%c' \n", paddr + i, c );
			}
			 */
		}
	}
	
	Close ( fd );
	
	return 0;
}

