#include "vmm/mon/mon.h"
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* [Reference]
 * http://www.nondot.org/sabre/os/files/Communication/ser_port.txt
 * http://www.beyondlogic.org/serial/serial.htm
 */


/****************************************************************/

static void
Com_receiver_sub ( struct com_t *com )
{
	bit8u_t c;

	ASSERT ( com != NULL );

	Readn ( com->in_fd, &c, 1 );

	if ( c == '\n' )
		c = '\r';

	Pthread_mutex_lock ( &com->mp );

	while ( com->rxdata_is_ready ) {
		Pthread_cond_wait ( &com->cond, &com->mp );
	}

	com->rx_buf = c;
	com->rxdata_is_ready = TRUE;
	com->data_is_receiving = TRUE;
	
	Pthread_mutex_unlock ( &com->mp );

//	Print_color ( stdout, CYAN, "read = \"%c\"\n", c ); // [DEBUG] 
	Kill ( com->pid, SIGUSR2 );
}

static void *
Com_receiver ( void *arg )
{
	struct com_t *com = ( struct com_t * ) arg;

	ASSERT ( arg != NULL );

	for ( ; ; ) {
		Com_receiver_sub ( com );
	}

	return NULL;
}

/****************************************************************/

static void
Com_init ( struct com_t *com, pid_t pid, int i, bool_t is_bootstrap )
{
	ASSERT ( com != NULL );

	/* [DEBUG] Currently only COM1 is supported */
	com->is_enabled = ( ( i == 1 ) && ( is_bootstrap ) );

	com->in_fd = 0;
	com->out_fd = 1;

	com->dl = 0;
	com->ier = 0;
	com->fcr = 0;
	com->lcr = 0;
	com->dlab = FALSE;
	com->mcr = 0;
	com->mcr_out2 = FALSE;

	com->txhold_is_enabled = FALSE;
	com->rxdata_is_enabled = FALSE;

	com->thr_buf = 0;
	com->rx_buf = 0;

	com->thr_is_empty = TRUE;
	com->rxdata_is_ready = FALSE;
	com->data_is_transferring = FALSE;

	com->txhold_interrupt = FALSE;
	com->rxdata_interrupt = FALSE;

	Pthread_mutex_init ( &com->mp, NULL );
	Pthread_cond_init ( &com->cond, NULL );
	com->pid = pid;

	if ( com->is_enabled )
		Pthread_create ( &com->tid, NULL, &Com_receiver, ( void *) com );
}
	
void
Coms_init ( struct com_t coms[], pid_t pid, bool_t is_bootstrap )
{
	int i;

	ASSERT ( coms != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ )
		Com_init ( &coms[i], pid, i, is_bootstrap );
}


/****************************************************************/

static void
Com_pack ( struct com_t *x, int fd )
{
//	Bit32u_pack ( ( bit32u_t ) x->in_fd, fd );
//	Bit32u_pack ( ( bit32u_t ) x->out_fd, fd );

	Bool_pack ( x->is_enabled, fd );
	Bit16u_pack ( x->dl, fd );
	Bit8u_pack ( x->ier, fd );
	Bit8u_pack ( x->fcr, fd );
	Bit8u_pack ( x->lcr, fd );
	Bool_pack ( x->dlab, fd );
	Bit8u_pack ( x->mcr, fd );
	Bool_pack ( x->mcr_out2, fd );
	Bit8u_pack ( x->scr, fd );

	Bool_pack ( x->txhold_is_enabled, fd );
	Bool_pack ( x->rxdata_is_enabled, fd );

	Bit8u_pack ( x->thr_buf, fd );
	Bit8u_pack ( x->rx_buf, fd );

	Bool_pack ( x->thr_is_empty, fd );
	Bool_pack ( x->rxdata_is_ready, fd );
	Bool_pack ( x->data_is_transferring, fd );
	Bool_pack ( x->data_is_receiving, fd );

	Bool_pack ( x->txhold_interrupt, fd );
	Bool_pack ( x->rxdata_interrupt, fd );
}

void
Coms_pack ( struct com_t coms[], int fd )
{
	int i;

	ASSERT ( coms != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ ) {
		Com_pack ( &coms[i], fd );
	}
}

static void
Com_unpack ( struct com_t *x, int fd )
{
//	x->in_fd = ( int ) Bit32u_unpack ( fd );
//	x->out_fd = ( int ) Bit32u_unpack ( fd );

	x->is_enabled = Bool_unpack ( fd );
	x->dl = Bit16u_unpack ( fd );
	x->ier = Bit8u_unpack ( fd );
	x->fcr = Bit8u_unpack ( fd );
	x->lcr = Bit8u_unpack ( fd );
	x->dlab = Bool_unpack ( fd );
	x->mcr = Bit8u_unpack ( fd );
	x->mcr_out2 = Bool_unpack ( fd );
	x->scr = Bit8u_unpack ( fd );

	x->txhold_is_enabled = Bool_unpack ( fd );
	x->rxdata_is_enabled = Bool_unpack ( fd );

	x->thr_buf = Bit8u_unpack ( fd );
	x->rx_buf = Bit8u_unpack ( fd );

	x->thr_is_empty = Bool_unpack ( fd );
	x->rxdata_is_ready = Bool_unpack ( fd );
	x->data_is_transferring = Bool_unpack ( fd );
	x->data_is_receiving = Bool_unpack ( fd );

	x->txhold_interrupt = Bool_unpack ( fd );
	x->rxdata_interrupt = Bool_unpack ( fd );	
}

void
Coms_unpack ( struct com_t coms[], int fd )
{
	int i;

	ASSERT ( coms != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ ) {
		Com_unpack ( &coms[i], fd );
	}
}

/****************************************************************/

static int
com_no_of_addr ( bit16u_t addr, int *offset )
{
	int i;
	const bit32u_t tbl[NUM_OF_COMS][2] = 
		{ { 0x03f8, 0x0400 }, /* COM1 */
		  { 0x02f8, 0x0300 }, /* COM2 */
		  { 0x03e8, 0x03f0 }, /* COM3 */
		  { 0x02e8, 0x02f0 } /* COM4 */ 
		};;

	assert ( offset != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ ) {
		if ( ( addr >= tbl[i][0] ) && ( addr < tbl[i][1] ) ) {
			*offset = addr - tbl[i][0];
			return i + 1;
		}
	}
	Fatal_failure ( "com_no_of_addr\n" );
	return -1;
}

/****************************************************************/

/* RBR ( Receive Buffer Register ) */
static bit8u_t
Com_read_rbr ( struct com_t *com )
{
	bit8u_t ret;

	ASSERT ( com != NULL );

	Pthread_mutex_lock ( &com->mp );

	com->rxdata_is_ready = FALSE;
	com->rxdata_interrupt = FALSE;
	ret = com->rx_buf;

	Pthread_cond_signal ( &com->cond );

	Pthread_mutex_unlock ( &com->mp );

	//DPRINT2 ( "  read_rbr = %c\n", ret );

	return ret;
}

/* IIR ( Interrupt Identification Register ) */
static bit8u_t
Com_read_iir ( struct com_t *com )
{
	int kind;
	bool_t pending;

	/* Bit 0 tells you if the UART has triggered it. */
	/* 0x1 ==> None ( No Interrput Pending ) 
	 * 0x0 ==> Modem ( One of the delta flags in the MSR set. Serviced by reading MSP ) */

	/*
	 * Bit 1 and bit 2 で割り込みの種類が決定する
	 * THRE : Bit2 = 0, Bit1 = 1 
	 * DA   : Bit2 = 1, Bit1 = 0 
	 */
	ASSERT ( com != NULL );

	if ( com->rxdata_interrupt ) {
		kind = 0x2;
		pending = FALSE;
	} else if ( com->txhold_interrupt ) {
		kind = 0x1;
		pending = FALSE;
	} else {
		kind = 0x0;
		pending = TRUE;
	}
	
	com->rxdata_interrupt = FALSE;
	com->txhold_interrupt = FALSE;

	return ( pending | ( kind << 1 ) );
}
 
/* LCR ( Line Control Register ) */
static bit8u_t
Com_read_lcr ( struct com_t *com )
{
	/* bit 0-1 : word length ( 11B => 8 bits )
	 * bit 2   : Stop bits
	 * bit 3-5 : Parity type ( = no parity )
	 * bit 6   : SOUT condition ( = normal operation )
	 * bit 7   : DLAB ( = normal registers )
	 */
	ASSERT ( com != NULL );
	return 3; // ???
}

/* LSR ( Line Status Register ) */
static bit8u_t
Com_read_lsr ( struct com_t *com )
{
	bool_t retval;
	bool_t b;

	/* 
	 * Bit 0: Data Ready ( DR ). 
	 * Bit 1: Overrun Error ( OE ).
	 * Bit 2: Parity Error ( PE ).
	 * Bit 3: Framing Error ( FE ).
	 * Bit 4: Break Indicator ( BI ).
	 * Bit 5: Transmitter Holding Register Empty ( THRE ).
	 * Bit 6: Transmitter Empty ( TEMT ).
	 * Bit 7: ( 16550* only )
	 */
	ASSERT ( com != NULL );

	Pthread_mutex_lock ( &com->mp );
	b = com->rxdata_is_ready;
	Pthread_mutex_unlock ( &com->mp );

	retval = ( b | 
		   ( com->thr_is_empty << 5 )  |
		   ( 1 << 6 ) );

	return retval;
}

static bit8u_t
Com_read_msr ( struct com_t *com )
{
	ASSERT ( com != NULL );

	/* THe only DCD ( Data Carrier Detect ) bit ( = state of OUT2 ) is set */
	return 0x80; 
}

bit8u_t
Coms_read ( struct com_t coms[], bit16u_t addr, size_t len )
{
	bit32u_t retval = 0;
	int com_no;
	int offset;
	struct com_t *com;

	ASSERT ( coms != NULL );

	com_no = com_no_of_addr ( addr, &offset );
	com = &coms[com_no];
	if ( ! com->is_enabled ) 
		return 0;

	switch ( offset ) {
	case 0x00: retval = ( com->dlab ) ? SUB_BIT ( com->dl, 0, 8 ) : Com_read_rbr ( com ); break;
	case 0x01: retval = ( com->dlab ) ? SUB_BIT ( com->dl, 8, 8 ) : com->ier; break;
	case 0x02: retval = Com_read_iir ( com ); break;
	case 0x03: retval = Com_read_lcr ( com ); break;
	case 0x04: retval = com->mcr; break;
	case 0x05: retval = Com_read_lsr ( com ); break;
	case 0x06: retval = Com_read_msr ( com ); break;
	case 0x07: retval = com->scr; break;
	default:  Match_failure ( "Coms_read\n" );
	}


//	Print_color ( stdout, YELLOW, "Coms_read: offset = %#x, val = %#x\n", offset, retval );


	return retval;
}

/****************************************************************/

/* THR ( Transmitter Holding Register ) */
static void
Com_write_thr ( struct com_t *com, bit8u_t val )
{
 	ASSERT ( com != NULL );

	com->thr_buf = val;
	com->data_is_transferring = TRUE;
	com->txhold_interrupt = FALSE;

	Writen ( com->out_fd, &com->thr_buf, 1 );

//	Writen ( 2, &com->thr_buf, 1 );
}

static void
Com_write_offset0 ( struct com_t *com, bit8u_t val )
{
	ASSERT ( com != NULL );
	if ( com->dlab ) {
		com->dl = BIT_ALIGN ( com->dl, 8 ) | val;
	} else {
		Com_write_thr ( com, val );
	}
}

/* IER ( Interrupt Enable Register ) */
static void
Com_write_ier ( struct com_t *com, bit8u_t val )
{
	bool_t b;
	/*
	 * Bit 0: If set, DR ( Data Ready ) interrput is enabled.
	 *    It is generated if data waits to be read by the CPU. 
	 * Bit 1: If set, THRE ( THR Empty ) interrput is enabled.
	 *    It tells the CPU to write characters to the THR.
	 * Bit 2: If set, Status interrput is enabled.
	 *    It informs the CPU of occurred transmission errors 
	 *    during reception.
	 * Bit 3: If set, Modem status interrput is enabled.
	 *    It is triggered whenver one of the delta-bits is set 
	 */
	ASSERT ( com != NULL );

	b = com->txhold_is_enabled;

	com->ier = val;
	com->rxdata_is_enabled = SUB_BIT ( val, 0, 1 );
	com->txhold_is_enabled = SUB_BIT ( val, 1, 1 );

	if ( ( ! b ) && ( com->txhold_is_enabled ) )
		com->data_is_transferring = TRUE;
}

static void
Com_write_offset1 ( struct com_t *com, bit8u_t val )
{
	ASSERT ( com != NULL );
	if ( com->dlab ) {
		com->dl = ( val << 8 ) | SUB_BIT ( com->dl, 0, 8 );
	} else {
		Com_write_ier ( com, val );
	}
}

/* LCR ( Line Control Register ) */
static void
Com_write_lcr ( struct com_t *com, bit8u_t val )
{
	ASSERT ( com != NULL );
	com->lcr = val;
	com->dlab = SUB_BIT ( com->lcr, 7, 1 );
}

/* MCR ( Modem Control Register ) */
static void
Com_write_mcr ( struct com_t *com, bit8u_t val )
{
	/* 
	 * Bit 3: Programs -OUT2.
	 *    If set, interrputs generated by the UART are
	 *    transferred to the ICU ( Interrput Control Unit ).
	 *    If cleared, the interrupt output of the card is set to high impedance.
	 *     ( This is PC only ). 
	 */
	ASSERT ( com != NULL );
	com->mcr = val;
	com->mcr_out2 = SUB_BIT ( com->mcr, 3, 1 );  
}

void
Coms_write ( struct com_t coms[], bit16u_t addr, bit32u_t val, size_t len )
{
	int com_no;
	int offset;
	struct com_t *com;

	ASSERT ( coms != NULL );

	com_no = com_no_of_addr ( addr, &offset );
	com = &coms[com_no];
	if ( ! com->is_enabled ) 
		return;

//	Print_color ( stdout, YELLOW, "Coms_write: offset = %#x, val = %#x\n", offset, val );
	
	switch ( offset ) {
	case 0x00: Com_write_offset0 ( com, val ); break;
	case 0x01: Com_write_offset1 ( com, val ); break;
	case 0x02: com->fcr = val; break;
	case 0x03: Com_write_lcr ( com, val ); break;
	case 0x04: Com_write_mcr ( com, val ); break;
	case 0x05: /* LSR ( Line Status Register ) */ break;
	case 0x06: /* MSR ( Modem Status Register ) */ break;
	case 0x07: com->scr = val; break;
	default:  Match_failure ( "Coms_write\n" );
	}
}

/****************************************************************/

static bool_t
is_txhold_new_interrupt ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->txhold_is_enabled )
		return FALSE;

	if ( ! com->data_is_transferring ) 
		return FALSE;

	com->thr_is_empty = TRUE;
	com->data_is_transferring = FALSE;

	com->txhold_interrupt = TRUE;

//	Print_color ( stdout, GREEN, "txhold interrupt\n" );
//	SINGLESTEP_MODE  =  TRUE;

	return TRUE;
}

static bool_t
need_to_raise_new_rxdata_interrupt ( struct com_t *com )
{
	bool_t ret;
	
	Pthread_mutex_lock ( &com->mp );
	ret = com->data_is_receiving;
	Pthread_mutex_unlock ( &com->mp );

	return ret;
}

static bool_t
is_rxdata_new_interrupt ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->rxdata_is_enabled )
		return FALSE;

	if ( ! need_to_raise_new_rxdata_interrupt ( com ) )
		return FALSE;
	
//	Print_color ( stdout, GREEN, "rxdata interrupt\n" );

	Pthread_mutex_lock ( &com->mp );
	com->data_is_receiving = FALSE;
	Pthread_mutex_unlock ( &com->mp );

	com->rxdata_interrupt = TRUE;

	return TRUE;
}

static bool_t
Com_try_get_irq ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->mcr_out2 ) 
		return FALSE;

	if ( is_txhold_new_interrupt ( com ) )
		return TRUE;

	if ( is_rxdata_new_interrupt ( com ) )
		return TRUE;

	return FALSE;
}

int
Coms_try_get_irq ( struct com_t coms[] )
{
	int i;

	ASSERT ( coms != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ ) {
		struct com_t *com = &coms[i];

		if ( ! com->is_enabled ) 
			continue;

		if (  Com_try_get_irq ( com ) ) {
			return IRQ_COM ( i );
		}
	}

	return IRQ_INVALID; 
}

/****************************************************************/

static bool_t
is_txhold_new_interrupt2 ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->txhold_is_enabled )
		return FALSE;

	if ( ! com->data_is_transferring ) 
		return FALSE;

	return TRUE;
}

static bool_t
is_rxdata_new_interrupt2 ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->rxdata_is_enabled )
		return FALSE;

	if ( ! need_to_raise_new_rxdata_interrupt ( com ) )
		return FALSE;
	
	return TRUE;
}

static bool_t
Com_check_irq ( struct com_t *com )
{
	ASSERT ( com != NULL );

	if ( ! com->mcr_out2 ) 
		return FALSE;

	if ( is_txhold_new_interrupt2 ( com ) )
		return TRUE;

	if ( is_rxdata_new_interrupt2 ( com ) )
		return TRUE;

	return FALSE;
}

int
Coms_check_irq ( struct com_t coms[] )
{
	int i;

	ASSERT ( coms != NULL );

	for ( i = 0; i < NUM_OF_COMS; i++ ) {
		struct com_t *com = &coms[i];

		if ( ! com->is_enabled ) 
			continue;

		if (  Com_check_irq ( com ) ) {
			return TRUE;
		}
	}

	return FALSE;
}
