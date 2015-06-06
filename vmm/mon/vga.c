#include "vmm/mon/mon.h"
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

enum {
	PADDR_LOWER = 0xb8000,
	PADDR_UPPER = 0xc0000,
	WINDOW_WIDTH = 80,
	WINDOW_HEIGHT = 25
};


#ifdef ENABLE_CURSES
#include <curses.h>


enum {
	SUB_WINDOW_BEGIN_X = 0,
	SUB_WINDOW_BEGIN_Y = 2,
	SUB_WINDOW_WIDTH = 80,
	SUB_WINDOW_HEIGHT = 21
};

static WINDOW *
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

static void
init_curses(void)
{
	Initscr ( ); 			/* initialize the curses library */
	Keypad ( stdscr, TRUE );	/* enable keyborad mapping */
	Scrollok ( stdscr, TRUE );
	Nonl ( );			/* tell curses not to do NL->CR/NL on output */
	Cbreak ( );			/* take input chars one at a time, no wait for \n */
	Noecho ( );			/* don't echo input */
}

static void
print_menu ( void )
{
	mvprintw ( 0, 24, "V I R T U A L  M A C H I N E " );
	mvhline ( 1, 0, ACS_HLINE, WINDOW_WIDTH );
	mvhline ( 23, 0, ACS_HLINE, WINDOW_WIDTH );
	Refresh ( ) ;
}

static WINDOW *
create_subwin ( void )
{
	WINDOW *w;
	
	w = Subwin ( stdscr, 
		     SUB_WINDOW_HEIGHT, SUB_WINDOW_WIDTH, 
		     SUB_WINDOW_BEGIN_Y, SUB_WINDOW_BEGIN_X );
	return w;
}

#endif /* ENABLE_CURSES */

void
Vga_init ( struct vga_t *x, int cpuid, bit32u_t pmem_base )
{
	ASSERT ( x != NULL );

	x->fd = Open2_fmt ( O_WRONLY | O_CREAT | O_TRUNC, 
			    S_IRUSR | S_IWUSR,
			    "/tmp/vga%d", cpuid );

	x->base_y = 0;

#ifdef ENABLE_CURSES
	init_curses ( );
	print_menu ( );
	x->subwin = create_subwin ( );
#endif
}

void
Vga_destroy ( struct vga_t *x )
{
#ifdef ENABLE_CURSES
	Endwin();
#endif
}

struct point_t {
	int x, y;
};

#ifdef ENABLE_CURSES

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
print_char ( WINDOW *w, bit32u_t paddr, int base_y )
{
	static int nscr = 0;
	struct point_t pt;
	char c;
	
	pt = paddr_to_pt ( paddr );
	pt.y += base_y;

	c = ( char ) Monitor_read_byte_with_paddr ( paddr );
	if ( ! isprint ( c ) )
		return;
	
	if ( pt.y - nscr >= SUB_WINDOW_HEIGHT ) {
		nscr++; 
		Scrollok ( w, TRUE );
		Scroll ( w );
		Scrollok ( w, FALSE );
	}
	
	mvwaddch ( w, pt.y - nscr, pt.x, c );
	Wrefresh ( w );
}

#endif /* ENABLE_CURSES */

static void
dump ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
	int fd = mon->devs.vga.fd;
	bit32u_t val;

	Writen ( fd, &paddr, sizeof ( bit32u_t ) );
	Writen ( fd, &len, sizeof ( size_t ) );	

	val = *( bit32u_t *) ( mon->pmem.base + paddr );
	Writen ( fd, &val, sizeof ( bit32u_t ) );	
}

void
write_to_vram ( struct mon_t *mon, bit32u_t paddr, size_t len )
{
	struct vga_t *vga;
	
	ASSERT ( mon != NULL );
	vga = &mon->devs.vga;

	if ( paddr == PADDR_LOWER )
		vga->base_y += ( PADDR_UPPER - PADDR_LOWER ) / WINDOW_WIDTH;

#ifdef ENABLE_CURSES
	{
		int i;

		for ( i = 0; i < len; i += 2 )
			print_char ( vga->subwin, paddr + i, vga->base_y );
	}
#endif 

	dump ( mon, paddr, len );
}
