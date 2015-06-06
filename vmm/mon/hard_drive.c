#include "vmm/mon/mon.h"
#include <string.h>
#include <fcntl.h>
#include <linux/hdreg.h>

/*
 * {Reference] 
 * IDE - Hardware Reference & Information Document
 * ( http://www.repairfaq.org/filipg/LINK/F_IDE-tech.html )

 * AT Attachment-3 Interface ( ATA-3 )
 * ( http://www.t13.org )
 */

/* PIO data in commands
 * 
 * IDENTIFY DEVICE
 * READ SECTOR ( S )
 */

/* Non-data commands 
 *
 * RECALIBRATE
 * INITIALIZE DEVICE PARAMETERS
 */


#ifndef WIN_READ_ONCE
# define WIN_READ_ONCE          0x21 /* 28-Bit without retries */
#endif 

#ifndef WIN_READDMA_ONCE
# define WIN_READDMA_ONCE          0xc9
#endif 

#ifndef WIN_WRITEDMA_ONCE
# define WIN_WRITEDMA_ONCE          0xcb
#endif 

#ifndef WIN_RECAL
# define WIN_RECAL            0x10
#endif 

enum hard_drive_access_kind {
	HD_ACCESS_PRIMARY_CMD_REGS, 
	HD_ACCESS_PRIMARY_CNTL_REGS, 
	HD_ACCESS_SECOND_CMD_REGS, 
	HD_ACCESS_SECOND_CNTL_REGS
};
typedef enum hard_drive_access_kind	hard_drive_access_kind_t;

struct hard_drive_access_entry_t {
	bit16u_t			addrs[2]; /* addrs[0] ==> lower, addrs[1] ==> upper */ 
	hard_drive_access_kind_t	kind;
};

static struct hard_drive_access_entry_t hard_drive_access_map[] =
{ { { 0x0170, 0x0178 }, HD_ACCESS_SECOND_CMD_REGS },
 { { 0x01f0, 0x01f8 }, HD_ACCESS_PRIMARY_CMD_REGS },
 { { 0x0370, 0x0378 }, HD_ACCESS_SECOND_CNTL_REGS },
 { { 0x03f0, 0x03f8 }, HD_ACCESS_PRIMARY_CNTL_REGS }
};

static size_t
nr_hard_drive_access_entries ( void )
{
	return sizeof ( hard_drive_access_map ) / sizeof ( struct hard_drive_access_entry_t );
}

static hard_drive_access_kind_t
addr_to_hard_drive_access_kind ( bit16u_t addr )
{
	int i;
	for ( i = 0; i < nr_hard_drive_access_entries ( ); i++ ) {
		struct hard_drive_access_entry_t *x = &hard_drive_access_map[i];
	 
		if ( ( addr >= x->addrs[0] ) && ( addr < x->addrs[1] ) )
			return x->kind;
	}
	assert ( 0 );
	return -1;
}

/****************************************************************/

static void
Chs_init ( struct chs_t *x )
{
	ASSERT ( x != NULL );
	x->cylinder = 0;   
	x->head = 0;
	x->sector = 1;
}

/****************************************************************/

static void
ControllerStatus_init ( struct controller_status_t *x )
{
	ASSERT ( x != NULL );

	x->busy = FALSE;
	x->drive_ready = TRUE;
	x->drive_write_fault = FALSE;
	x->drive_seek_complete = TRUE;
	x->drive_request = FALSE;
	x->corrected_data = FALSE;
	x->index = FALSE;
	x->error = TRUE;
	// BX_CONTROLLER ( channel,device ).status.index_pulse  = 0;
	// BX_CONTROLLER ( channel,device ).status.index_pulse_count = 0;     
}

static bit8u_t
ControllerStatus_to_bit8u ( const struct controller_status_t *x )
{
	ASSERT ( x != NULL );

	return ( ( x->busy << 7 ) |
		 ( x->drive_ready << 6 ) |
		 ( x->drive_write_fault << 5 ) |
		 ( x->drive_seek_complete << 4 ) |
		 ( x->drive_request << 3 ) |
		 ( x->corrected_data << 2 ) |
		 ( x->index << 1 ) |
		 ( x->error ) );
}

void
ControllerStatus_print ( FILE *stream, const struct controller_status_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );
   
	Print ( stream, 
	   "{ BSY=%d, DRDY=%d, DF=%d, DSC=%d, DRQ=%d, CORR=%d, IDX=%d, ERR=%d }\n",
	   x->busy,
	   x->drive_ready,
	   x->drive_write_fault,
	   x->drive_seek_complete,
	   x->drive_request,
	   x->corrected_data,
	   x->index,
	   x->error );
}

/****************************************************************/

static void
Controller_init ( struct controller_t *x )
{
	ASSERT ( x != NULL );

	Chs_init ( &x->addr.chs );

	x->current_command = 0;
	x->error_register = 0;

	x->reset = FALSE;
	x->reset_in_progress = FALSE;
	x->disable_irq = FALSE;

	x->lba_mode = FALSE;

	x->sector_count = 1;
	// x->sectors_per_block = 0x80;

	x->features = 0;

	x->requesting_irq = FALSE;

	ControllerStatus_init ( &x->status );
}

static void
Controller_raise_interrupt ( struct controller_t *x )
{
	ASSERT ( x != NULL );
	
	if ( ! x->disable_irq ) {
		x->requesting_irq = TRUE;
	}
}

static void
Controller_clear_pending_interrupt ( struct controller_t *x )
{
	ASSERT ( x != NULL );
	x->requesting_irq = FALSE;
}

// buffer_index + len >= MAX_COUNT_BUFSIZE となるとき，動作がおかしくなるような気がする
bit32u_t
Controller_read_from_buffer ( struct controller_t *x, size_t len )
{
	bit32u_t retval = 0;

	ASSERT ( x != NULL );

	switch ( len ) {
	case 4: retval = * ( ( bit32u_t * ) ( x->buffer + x->buffer_index ) ); break;
	case 2: retval = * ( ( bit16u_t * ) ( x->buffer + x->buffer_index ) ); break;
	default: Match_failure ( "Controller_read_from_buffer: len=%#x\n", len );
	}
	x->buffer_index += len;

	return retval;
}

static bool_t
cmd_regs_are_ready ( struct controller_t *x )
{
	struct controller_status_t *status = &x->status;

	DPRINT ( "busy=%d, drive_request=%d\n", status->busy, status->drive_request );

	return ( ( status->busy == FALSE ) && ( status->drive_request == FALSE ) );
}

/****************************************************************/

enum {
	BM_STATUS_DMAING = 0x01,
	BM_STATUS_ERROR  = 0x02,
	BM_STATUS_INT    = 0x04,
	
	BM_CMD_START	 = 0x01,
	BM_CMD_READ      = 0x08
};

static void
Dma_init ( struct dma_t *x )
{
	ASSERT ( x != NULL );

	x->status = 0x60; /* ??? */
	x->command = 0;
	x->addr = 0;

	x->has_started = FALSE;
	x->ma_kind = MEM_ACCESS_READ;
}

static void
Dma_write_command ( struct dma_t *x, bit32u_t val )
{
	ASSERT ( x != NULL );

	x->command = val & 0x09;

	if ( ! ( val & BM_CMD_START ) ) {
		/* XXX: do it better */
		x->status &= ~BM_STATUS_DMAING;
	} else {
		x->status |= BM_STATUS_DMAING;
	}
}

static void
Dma_write_status ( struct dma_t *x, bit32u_t val )
{
	ASSERT ( x != NULL );

	x->status = ( val & 0x60 ) | ( x->status & 1 ) | ( x->status & ~val & 0x06 );
}

static void
Dma_write_addr ( struct dma_t *x, bit32u_t val )
{
	ASSERT ( x != NULL );

	x->addr = BIT_ALIGN ( val, 2 );
}

/****************************************************************/

static void
lba_to_chs ( union sector_addr_t *x ) {
	bit32u_t v;

	ASSERT ( x != NULL );

	v = ( ( x->chs.head << 24 ) |
	      ( x->chs.cylinder << 8 ) |
	      ( x->chs.sector ) );
	x->lba = v;
}

static void
chs_to_lba ( union sector_addr_t *x ) {
	struct chs_t v;

	ASSERT ( x != NULL );

	v.sector = SUB_BIT ( x->lba, 0, 8 );
	v.cylinder = SUB_BIT ( x->lba, 8, 16 );
	v.head = SUB_BIT ( x->lba, 24, 4 );
	x->chs = v;
}

static void
update_lba ( union sector_addr_t *x, bit32u_t val, bit32u_t start, bit32u_t len )
{
	bit32u_t orig_lba = x->lba;
	bit32u_t n = 32 - ( start + len );

	x->lba = ( ( BIT_ALIGN ( orig_lba, n ) ) |
		   ( LSHIFTED_SUB_BIT ( val, 0, len, start ) ) |
		   ( SUB_BIT ( orig_lba, 0, start ) ) );
}

static size_t
get_logical_sector_addr_with_lba ( union sector_addr_t *x )
{
	ASSERT ( x != NULL );
	return x->lba;
}

static size_t
get_logical_sector_addr_with_no_lba ( union sector_addr_t *x, struct chs_t *limit )
{
	size_t retval;
   
	ASSERT ( x != NULL );
	ASSERT ( limit != NULL );
	
	retval = ( ( x->chs.cylinder * ( limit->head * limit->sector ) ) +
		   ( x->chs.head * limit->sector ) +
		   x->chs.sector - 1 );
   
	ASSERT ( retval < limit->cylinder * limit->head * limit->sector );
	return retval;
}

static size_t
get_logical_sector_addr ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;

	ASSERT ( x->disk_image_fd != -1 );

	return ( ( cntler->lba_mode ) ?
		get_logical_sector_addr_with_lba ( &cntler->addr ) :
		get_logical_sector_addr_with_no_lba ( &cntler->addr, &x->limit ) );
}

static void
increment_logical_sector_addr_with_lba ( union sector_addr_t *x )
{
	ASSERT ( x != NULL );
	x->lba++;
}

static void
increment_logical_sector_addr_with_no_lba ( struct controller_t *x, struct chs_t *limit )
{
	struct chs_t *addr = &x->addr.chs;;
   
	addr->sector++;

	if ( addr->sector <= limit->sector ) { return; }
   
	addr->sector = 1;
	addr->head++;
   
	if ( addr->head < limit->head ) { return; }
   
	addr->head = 0;
	addr->cylinder++;
   
	if ( addr->cylinder < limit->cylinder ) { return; }
   
	addr->cylinder = limit->cylinder - 1;
}

static void
increment_logical_sector_addr ( struct controller_t *x, struct chs_t *limit )
{
	x->sector_count--; // sector_count を減らす 

	if ( x->lba_mode ) {
		increment_logical_sector_addr_with_lba ( &x->addr );
	} else {
		increment_logical_sector_addr_with_no_lba ( x, limit );
	}
}

/****************************************************************/

static void
Drive_init ( struct drive_t *x, drive_select_t s, const char *disk_file )
{
	ASSERT ( x != NULL );
	ASSERT ( disk_file != NULL );

	switch ( s ) {
	case MASTER_DRIVE:
		x->disk_image_fd = Open ( disk_file, O_RDWR );

		/* The follwing values are from .bochsrc */
		x->limit.cylinder = LIMIT_CYLINDER;
		x->limit.head = LIMIT_HEAD;
		x->limit.sector = LIMIT_SECTOR;

		Controller_init ( &x->cntler );
		break;

	case SLAVE_DRIVE:
		x->disk_image_fd = -1;

		x->limit.cylinder = 0;
		x->limit.head = 0;
		x->limit.sector = 0;

		Controller_init ( &x->cntler );
		break;

	default:
		Match_failure ( "Drive_init\n" );
	}

	Dma_init ( &x->dma );
}

static void
Drive_lseek_set ( struct drive_t *x )
{
	size_t logical_sector;

	ASSERT ( x != NULL );

	logical_sector = get_logical_sector_addr ( x );
	Lseek ( x->disk_image_fd, logical_sector * MAX_CNTLER_BUFSIZE, SEEK_SET );
}

static void
Drive_read ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;
	Readn ( x->disk_image_fd, cntler->buffer, MAX_CNTLER_BUFSIZE );
}

static void
Drive_read2 ( struct drive_t *x, int n )
{
	struct controller_t *cntler = &x->cntler;
	Readn ( x->disk_image_fd, cntler->buffer, MAX_CNTLER_BUFSIZE * n );
}

static void
Drive_write ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;
	Writen ( x->disk_image_fd, cntler->buffer, MAX_CNTLER_BUFSIZE );
}

static void
Drive_write2 ( struct drive_t *x, int n )
{
	struct controller_t *cntler = &x->cntler;
	Writen ( x->disk_image_fd, cntler->buffer, MAX_CNTLER_BUFSIZE * n );
}

/****************************************************************/

static void
IdeChannel_init ( struct ide_channel_t *x, const char *disk_file )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( disk_file != NULL );

	x->drive_select = 0;
	for ( i = 0; i < NUM_OF_DRIVERS; i++ )
		Drive_init ( &x->drives[i], i, disk_file );
}

/****************************************************************/

void
HardDrive_init ( struct hard_drive_t *x, const char *disk_file )
{
	ASSERT ( x != NULL );
	ASSERT ( disk_file != NULL );

	IdeChannel_init ( &x->channel, disk_file );
}

static struct drive_t *
get_selected_drive ( struct hard_drive_t *x )
{
	ASSERT ( x != NULL );
	return &x->channel.drives[x->channel.drive_select];
}

struct controller_t *
get_selected_controller ( struct hard_drive_t *x )
{
	struct drive_t *drive;
	ASSERT ( x != NULL );

	drive = get_selected_drive ( x );
	return &drive->cntler;
}

/****************************************************************/

void
HardDrive_pack ( struct hard_drive_t *x, int fd )
{
	int i;

	Pack ( x, sizeof ( struct hard_drive_t ), fd );

	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		Bit32u_pack ( ( bit32u_t ) x->channel.drives[i].disk_image_fd, fd );
	}
}

void
HardDrive_unpack ( struct hard_drive_t *x, int fd )
{
	int i;

	Unpack ( x, sizeof ( struct hard_drive_t ), fd );

	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		x->channel.drives[i].disk_image_fd = ( int ) Bit32u_unpack ( fd );
	}
}

/****************************************************************/

/* 追加でデータを読み込む */
static void
read_next_buffer ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;
	struct controller_status_t *status = &cntler->status;

	// update sector count, sector number, cylinder,
	// drive, head, status
	// if there are more sectors, read next one in...
	//
	cntler->buffer_index = 0;
   
	increment_logical_sector_addr ( cntler, &x->limit );
   
	status->busy = FALSE;
	status->drive_ready = TRUE;
	status->drive_write_fault = FALSE;
	status->drive_seek_complete = TRUE; // ??? Onew
	status->corrected_data = FALSE;
	status->error = FALSE;
   
	/* 全てのデータを読み終った場合 */
	if ( cntler->sector_count == 0 ) {
		status->drive_request = FALSE;
		return;
	}

	/* read next one into controller buffer */
	status->drive_request = TRUE;
	status->drive_seek_complete = TRUE;
   
	Drive_lseek_set ( x );
	Drive_read ( x );

	Controller_raise_interrupt ( cntler );
}

static bit32u_t
read_data_read_sectors ( struct drive_t *x, size_t len )
{
	struct controller_t *cntler = &x->cntler;
	bit32u_t val;

	ASSERT ( x->disk_image_fd != -1 );
	ASSERT ( cntler->buffer_index < MAX_CNTLER_BUFSIZE );
      
	// [???] buffer_index + len >= MAX_CNTLER_BUFSIZE の場合，
	// buffer を overflow する
   
	// buffer_index の値が len だけ増える
	val = Controller_read_from_buffer ( cntler, len );

	// if buffer is completely read   
	if ( cntler->buffer_index >= MAX_CNTLER_BUFSIZE ) {
		read_next_buffer ( x );
	}
	return val;
}

static bit32u_t
read_data_identify_device ( struct controller_t *x, size_t len )
{
	struct controller_status_t *status = &x->status;
	bit32u_t retval;

	status->busy = FALSE;
	status->drive_ready = TRUE;
	status->drive_write_fault = FALSE;
	status->drive_seek_complete = TRUE;
	status->corrected_data = FALSE;
	status->error = FALSE;
   
	retval = Controller_read_from_buffer ( x, len );

	if ( x->buffer_index >= MAX_CNTLER_BUFSIZE ) {
		status->drive_request = FALSE; 
	}

	/* 追加で読み込む必要はない？ */

	return retval;
}

/* page 19 */
static bit32u_t
read_data ( struct drive_t *x, size_t len )
{
	struct controller_t *cntler = &x->cntler;
	struct controller_status_t *status = &cntler->status;
	bit32u_t retval = 0;

	//DPRINT2 ( " + DATA PORT REGISTER\n" );

	ASSERT ( status->drive_request );

	switch ( cntler->current_command ) {
	case WIN_READ: 	 /* 0x20 */
	case WIN_READ_ONCE: /* 0x21 */
		retval = read_data_read_sectors ( x, len ); 
		break;

	case WIN_IDENTIFY: /* 0xec */
	case WIN_PIDENTIFY: /* 0xa1 */ 
		retval = read_data_identify_device ( cntler, len ); 
		break;

	case WIN_READDMA:  	/* 0xc8 */
	case WIN_READDMA_ONCE: 	/* 0xc9 */ 
		assert ( 0 );
		break;

	case WIN_PACKETCMD: /* 0xa0 */ 
	default:
		Match_failure ( "read_data: current_command = %#x\n",
			   cntler->current_command );
	}
	return retval;
}

/*******************/

static bit32u_t
read_error ( struct controller_t *x, size_t len )
{
	struct controller_status_t *status = &x->status;

	ASSERT ( cmd_regs_are_ready ( x ) ); /* p. 13 */
	ASSERT ( len == 1 );
	ASSERT ( status->error == TRUE ); // ???

	//DPRINT2 ( " + ERROR\n" );

	status->error= FALSE; // ???

	return x->error_register;
}

/*******************/

static bit32u_t
read_nsector ( struct controller_t *x, size_t len )
{
	ASSERT ( x != NULL );
	ASSERT ( cmd_regs_are_ready ( x ) ); /* p. 13 */
	ASSERT ( len == 1 );

	//DPRINT2 ( " + NSECTOR\n" ); 
	return x->sector_count; 
}

/*******************/

static bit32u_t
read_sector ( struct controller_t *x, size_t len )
{
	ASSERT ( x != NULL );
	ASSERT ( cmd_regs_are_ready ( x ) ); /* p. 13 */
	ASSERT ( len == 1 );

	//DPRINT2 ( " + SECTOR\n" );

	return ( ( x->lba_mode ) ?
		SUB_BIT ( x->addr.lba, 0, 8 ) : 
		x->addr.chs.sector );
}

/*******************/

static bit32u_t
read_lcyl ( struct controller_t *x, size_t len )
{
	ASSERT ( x != NULL );
	ASSERT ( cmd_regs_are_ready ( x ) ); /* p. 13 */
	ASSERT ( len == 1 );

	//DPRINT2 ( " + LCYL\n" );
	return ( ( x->lba_mode ) ?
		SUB_BIT ( x->addr.lba, 8, 8 ) : 
		SUB_BIT ( x->addr.chs.cylinder, 0, 8 ) );
}

/*******************/

static bit32u_t
read_hcyl ( struct controller_t *x, size_t len )
{
	ASSERT ( x != NULL );
	ASSERT ( cmd_regs_are_ready ( x ) ); /* p. 13 */
	ASSERT ( len == 1 );

	//DPRINT2 ( " + HCYL\n" );
	return ( ( x->lba_mode ) ?
		SUB_BIT ( x->addr.lba, 16, 8 ) : 
		SUB_BIT ( x->addr.chs.cylinder, 8, 8 ) );
}

/*******************/

static bit32u_t
read_current ( struct hard_drive_t *x, size_t len )
{
	struct controller_t *cntler = get_selected_controller ( x );
	bit4u_t val;

	ASSERT ( cmd_regs_are_ready ( cntler ) ); /* p. 13 */
	ASSERT ( len == 1 );

	//DPRINT2 ( " + DRIVE/HEAD REGISTER: drive_select=%d\n", x->channel.drive_select );

	val = ( ( cntler->lba_mode ) ?
	    SUB_BIT ( cntler->addr.lba, 24, 4 ) :
	    cntler->addr.chs.head );

	return ( ( 1 << 7 ) |
		 ( cntler->lba_mode << 6 ) |
		 ( 1 << 5 ) |
		 ( x->channel.drive_select << 4 ) |
		val );
}

/*******************/

static bit8u_t
read_status ( struct controller_t *x, bit16u_t addr, size_t len )
{
	struct controller_status_t *status = &x->status;
	bit8u_t retval;

//   DPRINT2 ( " + STATUS\n" );
//   ControllerStatus_print ( stdout, status ); /* [DEBUG] */

	ASSERT ( len == 1 );

	/*
	 BX_SELECTED_CONTROLLER ( channel ).status.index_pulse_count++;
	 BX_SELECTED_CONTROLLER ( channel ).status.index_pulse = 0;

	 if ( BX_SELECTED_CONTROLLER ( channel ).status.index_pulse_count >= INDEX_PULSE_CYCLE ) {
	 BX_SELECTED_CONTROLLER ( channel ).status.index_pulse = 1;
	 BX_SELECTED_CONTROLLER ( channel ).status.index_pulse_count = 0;
	 }
	*/

	retval = ControllerStatus_to_bit8u ( status );

	if ( addr == 0x1f7 )
		Controller_clear_pending_interrupt ( x );


	return retval;
}

static bit32u_t
read_cmd_regs ( struct hard_drive_t *x, bit16u_t addr, size_t len )
{
	struct drive_t *drive = get_selected_drive ( x );
	struct controller_t *cntler = &drive->cntler;
	bit32u_t retval = 0;

	switch ( addr ) {
	case HD_DATA:    /* 0x1f0 */ retval = read_data ( drive, len ); break;
	case HD_ERROR:   /* 0x1f1 */ retval = read_error ( cntler, len ); break;
	case HD_NSECTOR: /* 0x1f2 */ retval = read_nsector ( cntler, len ); break;
	case HD_SECTOR:  /* 0x1f3 */ retval = read_sector ( cntler, len ); break;
	case HD_LCYL:    /* 0x1f4 */ retval = read_lcyl ( cntler, len ); break;
	case HD_HCYL:    /* 0x1f5 */ retval = read_hcyl ( cntler, len ); break;
	case HD_CURRENT: /* 0x1f6 */ retval = read_current ( x, len ); break;
	case HD_STATUS:  /* 0x1f7 */ retval = read_status ( cntler, addr, len ); break;
	default:	  	     Match_failure ( "read_cmd_regs\n" ); break;
	}

	return retval;
}

/**********************************/

static bit32u_t
read_cntl_regs ( struct hard_drive_t *x, bit16u_t addr, size_t len )
{
	struct controller_t *cntler = get_selected_controller ( x );
	bit32u_t retval = 0;

	switch ( addr ) {
	case HD_ALTSTATUS: /* 0x3f6 */ retval = read_status ( cntler, addr, len ); break;
	case 0x3f7:	 	    ASSERT ( 0 ); break;
	default:		    Match_failure ( "read_cntl_regs\n" );
	}
	return retval;
}

bit32u_t
HardDrive_read ( struct hard_drive_t *x, bit16u_t addr, size_t len )
{
	hard_drive_access_kind_t kind;
	bit32u_t retval = 0;

	ASSERT ( x != NULL );

	/*
	DPRINT ( "[HardDrive_read: addr=%#x, BUSY=%d,DREADY=%d,WFAULT=%d, SEEK=%d, DRQ=%d, COR=%d, INDEX=%d, ERROR=%d\n", 
	    addr, 
	    get_selected_controller ( x )->status.busy,
	    get_selected_controller ( x )->status.drive_ready,
	    get_selected_controller ( x )->status.drive_write_fault,
	    get_selected_controller ( x )->status.drive_seek_complete,
	    get_selected_controller ( x )->status.drive_request,
	    get_selected_controller ( x )->status.corrected_data,
	    get_selected_controller ( x )->status.index,
	    get_selected_controller ( x )->status.error );
	*/


	kind = addr_to_hard_drive_access_kind ( addr );
	switch ( kind ) {
	case HD_ACCESS_PRIMARY_CMD_REGS:	retval = read_cmd_regs ( x, addr, len ); break;
	case HD_ACCESS_PRIMARY_CNTL_REGS:	retval = read_cntl_regs ( x, addr, len ); break;
	case HD_ACCESS_SECOND_CMD_REGS:
	case HD_ACCESS_SECOND_CNTL_REGS:	retval = 0; break; /* [TODO] */
	default:				Match_failure ( "HardDrive_read\n" );
	}

//	Print ( stderr, "HardDrive_read: addr=%#x, val=%#x\n", addr, retval );

/*
	DPRINT ( "[HardDrive_read: addr=%#x, BUSY=%d,DREADY=%d,WFAULT=%d, SEEK=%d, DRQ=%d, COR=%d, INDEX=%d, ERROR=%d\n", 
	    addr, 
	    get_selected_controller ( x )->status.busy,
	    get_selected_controller ( x )->status.drive_ready,
	    get_selected_controller ( x )->status.drive_write_fault,
	    get_selected_controller ( x )->status.drive_seek_complete,
	    get_selected_controller ( x )->status.drive_request,
	    get_selected_controller ( x )->status.corrected_data,
	    get_selected_controller ( x )->status.index,
	    get_selected_controller ( x )->status.error );
*/

	return retval;
}

/****************************************************************/

static void
write_data_write_sectors ( struct hard_drive_t *x, bit32u_t val, size_t len )
{
	struct drive_t *drive = get_selected_drive ( x );
	struct controller_t *cntler = &drive->cntler;
	int i;
	
	assert ( cntler->buffer_index < 512 );
	
	for ( i = 0; i < len; i++ )
		cntler->buffer[cntler->buffer_index + i] = SUB_BIT ( val, i * 8, 8 );
	
	cntler->buffer_index += len;
	
	/* if buffer completely writtten */
	if ( cntler->buffer_index < 512 )
		return;

	Drive_lseek_set ( drive );
	Drive_write ( drive );

	cntler->buffer_index = 0;
		
	/* update sector count, sector number, cylinder,
	 * drive, head, status
	 * if there are more sectors, read next one in...
	 */
	increment_logical_sector_addr ( cntler, &drive->limit );
		
	/* When the write is complete, controller clears the DRQ bit and
	 * sets the BSY bit.
	 * If at least one more sector is to be written, controller sets DRQ bit,
	 * clears BSY bit, and issues IRQ 
	 */

	cntler->status.busy = FALSE;
	cntler->status.drive_ready = TRUE;
	cntler->status.error = FALSE;
	cntler->status.corrected_data = FALSE;
	cntler->status.drive_request = ( cntler->sector_count > 0 );

	Controller_raise_interrupt ( cntler );
}

/*******************/


/* page 19 */
static void
write_data ( struct hard_drive_t *x, bit32u_t val, size_t len )
{
	struct drive_t *drive = get_selected_drive ( x );
	struct controller_t *cntler = &drive->cntler;

	DPRINT ( " + DATA PORT REGISTER: current_coomand = %#x\n",
	    cntler->current_command );

	switch ( cntler->current_command ) {
	case WIN_WRITE: /* 0x30 */
		write_data_write_sectors ( x, val, len ); 
		break;

	case WIN_RECAL: /* 0x10 */
	case WIN_READ: /* 0x20 */
	case WIN_READ_ONCE: /* 0x21 */
	case WIN_SPECIFY: /* 0x91 */
	case WIN_PIDENTIFY: /* 0xa1 */
	case WIN_IDENTIFY: /* 0xe3 */
	default:
		Match_failure ( "write_data: cntler->current_command = %#x\n", cntler->current_command );
	}
}

/************************/

static void
write_precomp ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	//DPRINT2 ( " + PRECOMP\n" );
	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		cntler->features = val;   
	}   
}

/************************/

static void
write_nsector ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i, v;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	v = ( val == 0 ) ? 256 : val;

	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		cntler->sector_count = v;
	}   
}

/************************/

static void
write_sector ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	//DPRINT2 ( " + SECTOR\n" );
	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;

		if ( cntler->lba_mode ) {
			update_lba ( &cntler->addr, val, 0, 8 );
		} else {
			cntler->addr.chs.sector = val;
		} 
	}
}

/************************/

static void
write_lcyl ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	//DPRINT2 ( " + LCYL\n" );
	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;

		if ( cntler->lba_mode ) {
			update_lba ( &cntler->addr, val, 8, 8 );
		} else {
			bit16u_t old = cntler->addr.chs.cylinder;
			cntler->addr.chs.cylinder = ( ( BIT_ALIGN ( old, 8 ) ) | val );
		} 
	}
}

/************************/

static void
write_hcyl ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	//DPRINT2 ( " + HCYL\n" );
	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		if ( cntler->lba_mode ) {
			update_lba ( &cntler->addr, val, 16, 8 );
		} else {
			bit16u_t old = cntler->addr.chs.cylinder;
			cntler->addr.chs.cylinder = ( ( val << 8 ) | ( SUB_BIT ( old, 0, 8 ) ) );
		} 
	}   

	//busy = TRUE;
	// request = TRUE;
	// ready = FALSE;
}

/************************/

static void
write_current ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	int i;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	/*
	 DPRINT2 ( " + DRIVE/HEAD REGISTER: dsel %d -> %d: lba ( %d,%d ) -> %d\n",
	 x->channel.drive_select,
	 SUB_BIT ( val, 4, 1 ),
	 x->channel.drives[0].cntler.lba_mode,
	 x->channel.drives[1].cntler.lba_mode,
	 SUB_BIT ( val, 6, 1 ) );
	*/

	ASSERT ( ( TEST_BIT ( val, 7 ) ) && ( TEST_BIT ( val, 5 ) ) );
   
	x->channel.drive_select = SUB_BIT ( val, 4, 1 );
   
	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		bool_t old = cntler->lba_mode;

		cntler->lba_mode = SUB_BIT ( val, 6, 1 );
		if ( cntler->lba_mode ) {
			if ( old != cntler->lba_mode ) { chs_to_lba ( &cntler->addr ); }
			update_lba ( &cntler->addr, val, 24, 4 );
		} else {
			if ( old != cntler->lba_mode ) { lba_to_chs ( &cntler->addr ); }
			cntler->addr.chs.head = SUB_BIT ( val, 0, 4 );
		}
	} 
}

/************************/

static void
command_aborted ( struct controller_t *x )
{
	DPRINT ( "  + ABORT\n" );
	x ->current_command = 0;
	x->error_register = ABRT_ERR; // 0x04
}

/*******************/

/* RECALIBRATE.
 * This function performed by this command is vendor specific.
 * [Reference] p. 71
 */
static void
command_recalibrate ( struct controller_t *x )
{
	/* cylinder だけを動かすのか，それとも head や sector も動かすのか不明 */
	DPRINT ( "  + RECALIBRATE\n" );
	if ( x->lba_mode ) {
		x->addr.lba = 0;
	} else {
		Chs_init ( &x->addr.chs );
	}
}

/*******************/

/* READ SECTOR ( S ) ( with retries and without retires ). 
 * This command reads from 1 to 256 sectors as specified in the Sector
 * Count register. A sector count of 0 requests 256 sectors. The
 * transfer begins at the sector specified in Sector Number register.
 * [Reference] p. 69
 */
static void
command_read_sectors ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;

	DPRINT ( "  + READ SECTORS\n" );
	/*
	 ASSERT ( ( cntler->lba_mode ) ||
	  ( cntler->addr.chs.cylinder > 0 ) ||
	  ( cntler->addr.chs.head > 0 ) ||
	  ( cntler->addr.chs.sector > 0 ) );
	*/
   
	Drive_lseek_set ( x );

	// マニュアルには，sector count だけ read するとある
	Drive_read ( x );

	cntler->buffer_index = 0;
}

/*******************/

/* WRITE SECTOR ( S )
 * This command writes from 1 to 256 sectors as specified in the
 * Sector Count register. A sector count of 0 requests 256 sectors.
 * [Reference] p. 104 */
static void
command_write_sectors ( struct controller_t *x )
{
	ASSERT ( x != NULL );
	x->buffer_index = 0;	
}

/*******************/

/* INITIALIZE DEVICE PARAMETERS.
 * This command enables the host to set the number of logical sectors
 * per track and the number of logical heads minus 1 per logical
 * cylinder for the current CHS translation mode. 
 * [Reference] p. 61 */
static void
command_initialize_device_parameters ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;

	DPRINT ( "  + INITIALIZE DEVICE PARAMETERS\n" );

	ASSERT ( cntler->sector_count == x->limit.sector );
	//ASSERT ( cntler->addr.chs.head == x->limit.head - 1 ); chs mode の時のみ有効

	// エミュレーションしていない？
}

/*******************/

static void
command_idle_immediate ( struct controller_t *x )
{
	ASSERT ( x != NULL );
	Fatal_failure ( "command_idle_immediate\n" );
}

/*******************/

static void
command_identify_packet_device ( struct controller_t *x )
{
	ASSERT ( x != NULL );
	DPRINT ( "  + IDENTIFY PACKET DEVICE\n" );
	command_aborted ( x ); 
}

/*******************/

/* p. 49 - */
static void
identify_drive ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;
	bit16u_t *id_drive = x->id_drive;
	bit32u_t n;
	int i;

	for ( i = 0; i < NUM_OF_ID_DRIVE; i++ ) {
		id_drive[i] = 0;
	}
#if 0
	id_drive[0] = 0x0040;
	id_drive[1] = x->limit.cylinder;
	id_drive[3] = x->limit.head;
	id_drive[4] = x->limit.sector * MAX_CNTLER_BUFSIZE;
	id_drive[5] = MAX_CNTLER_BUFSIZE;
	id_drive[6] = x->limit.sector;

	id_drive[20] = 3;
	id_drive[21] = MAX_CNTLER_BUFSIZE;
	id_drive[22] = 4;

	for ( i = 0; i < 40; i++ ) {
		int j = 27 + i / 2;
		char k;
		const char *model_no = "ata1-master: type=disk, mode=flat";

		k = ( i < strlen ( model_no ) ) ? model_no[i] : 0;
		id_drive[j] = ( i % 2 == 0 ) ? ( k << 8 ) : k;
	}

#if MAX_MULT_SECTORS > 1    
	id_drive[47] = 0x8000 | MAX_MULT_SECTORS;
#endif

	id_drive[48] = 1;
	put_le16(p + 49, 1 << 9 | 1 << 8); /* DMA and LBA supported */

	id_drive[51] = 0x200;
	id_drive[52] = 0x200;
	put_le16(p + 53, 1 | 1 << 2); /* words 54-58,88 are valid */

	id_drive[54] = x->limit.cylinder;
	id_drive[55] = x->limit.head;
	id_drive[56] = x->limit.sector;

	n = ( x->limit.cylinder * x->limit.head * x->limit.sector );
	id_drive[57] = SUB_BIT ( n, 0, 16 );
	id_drive[58] = SUB_BIT ( n, 16, 16 );
#if MAX_MULT_SECTORS > 1    
	id_drive[59] = 0x100 | MAX_MULT_SECTORS;
#endif
	id_drive[60] = SUB_BIT ( n, 0, 16 );
	id_drive[61] = SUB_BIT ( n, 16, 16 );

	id_drive[80] = ( 1 << 2 ) | ( 1 << 1 );

	for ( i = 82; i < 88; i++ ) {
		id_drive[i] = 1 << 14;
	}
#else 
	id_drive[0] = 0x0040;
	id_drive[1] = x->limit.cylinder;
	id_drive[3] = x->limit.head;
	id_drive[4] = x->limit.sector * MAX_CNTLER_BUFSIZE;
	id_drive[5] = MAX_CNTLER_BUFSIZE;
	id_drive[6] = x->limit.sector;

	snprintf ( (char *)(id_drive + 10), 10, "VMP" ); /* serial number */

	id_drive[20] = 3;
	id_drive[21] = MAX_CNTLER_BUFSIZE; /* cache size in sectors */
	id_drive[22] = 4; /* ecc bytes */

	for ( i = 0; i < 40; i++ ) {
		int j = 27 + i / 2;
		char k;
		const char *model_no = "ata1-master: type=disk, mode=flat";

		k = ( i < strlen ( model_no ) ) ? model_no[i] : 0;
		id_drive[j] = ( i % 2 == 0 ) ? ( k << 8 ) : k;
	}

#if MAX_MULT_SECTORS > 1    
	id_drive[47] = 0x8000 | MAX_MULT_SECTORS;
#endif

	id_drive[48] = 1; /* dword I/O */
	id_drive[49] = (1 << 9 | 1 << 8); /* DMA and LBA supported */
	id_drive[51] = 0x200; /* PIO transfer cycle */
	id_drive[52] = 0x200; /* DMA transfer cycle */
	id_drive[53] = (1 | 1 << 2); /* words 54-58,88 are valid */
	id_drive[54] = x->limit.cylinder;
	id_drive[55] = x->limit.head;
	id_drive[56] = x->limit.sector;

	n = ( x->limit.cylinder * x->limit.head * x->limit.sector );
	id_drive[57] = SUB_BIT ( n, 0, 16 );
	id_drive[58] = SUB_BIT ( n, 16, 16 );
#if MAX_MULT_SECTORS > 1    
	id_drive[59] = 0x100 | MAX_MULT_SECTORS;
#endif
	id_drive[60] = SUB_BIT ( n, 0, 16 );
	id_drive[61] = SUB_BIT ( n, 16, 16 );

	id_drive[80] = ( 1 << 2 ) | ( 1 << 1 );
	id_drive[82] = (1 << 14);
	id_drive[83] = (1 << 14);
	id_drive[84] = (1 << 14);
	id_drive[85] = (1 << 14);
	id_drive[86] = 0;
	id_drive[87] = (1 << 14);
	id_drive[88] = 0x1f | (1 << 13);
	id_drive[93] =  1 | (1 << 14) | 0x2000 | 0x4000;

#endif


	// now convert the id_drive array ( native 256 word format ) to
	// the controller buffer ( 512 bytes )
	for ( i = 0; i < NUM_OF_ID_DRIVE; i++ ) {
		bit16u_t n = id_drive[i];

		cntler->buffer[i*2] = SUB_BIT ( n, 0, 8 );
		cntler->buffer[i*2 + 1] = SUB_BIT ( n, 8, 8 );
	}
}

/* IDENTIFY DEVICE. 
 * This command enables the host to receive parameter information from the device.
 * [Reference] pp. 48-57
 */
static void
command_identify_device ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;

	DPRINT ( "  + IDENTIFY DEVICE\n" );

	identify_drive ( x );

	// 以下の値は manual には記述されていない．

	cntler->buffer_index = 0;  
}

/*******************/

static bit32u_t
__dma_loop_read ( struct drive_t *x, bit32u_t phys_addr, size_t transfer_size )
{
	struct controller_t *cntler = &x->cntler;
	int offset;

	offset = 0;
	while ( offset < transfer_size ) {
		int len;
		bit32u_t addr, rest;

		addr = phys_addr + offset;
		rest = transfer_size - offset;
		len = cntler->buffer_size - cntler->buffer_index;

		if (len <= 0) {
			int n, i;

			/* transfert next data */
			n = cntler->sector_count;
			
			if (n == 0) { break; }
			if (n > MAX_MULT_SECTORS) { n = MAX_MULT_SECTORS; }

			/* transfert next data */			
			Drive_lseek_set ( x );
			Drive_read2 ( x, n );
			
			cntler->buffer_index = 0;
			cntler->buffer_size = MAX_CNTLER_BUFSIZE * n;
			len = cntler->buffer_size;
			
			/* [TODO] */
			for ( i = 0; i < n; i++ ) {
				increment_logical_sector_addr ( cntler, &x->limit );
			}
		}

		if (len > rest) { len = rest; }

		Monitor_mmove_wr ( addr, cntler->buffer + cntler->buffer_index, len );

		cntler->buffer_index += len;
		offset += len;
	}

	if ( ( cntler->buffer_index >= cntler->buffer_size ) && ( cntler->sector_count == 0 ) ) {
		cntler->status.drive_request = FALSE;
		cntler->status.drive_seek_complete = TRUE;
		Controller_raise_interrupt ( cntler );
/*
		Print_color ( stdout, GREEN, 
			      "zero return: index=%#x, size=%#x, offset=%#x, trans=%#x, sec_count=%#x.\n",
			      cntler->buffer_index, cntler->buffer_size,
			      offset, transfer_size,
			      cntler->sector_count );
*/
		return 0;
	}

	return offset;
}

static bit32u_t
__dma_loop_write ( struct drive_t *x, bit32u_t phys_addr, size_t transfer_size1 )
{
	int transfer_size;
	struct controller_t *cntler = &x->cntler;

	transfer_size = transfer_size1;
	for ( ; ; ) {
		int len;

		len = cntler->buffer_size - cntler->buffer_index;
		if ( len == 0 ) {
			int i, n;
			
			n = cntler->buffer_size >> 9;

			Drive_lseek_set ( x );
			Drive_write2 ( x, n );

			for ( i = 0; i < n; i++ ) {
				increment_logical_sector_addr ( cntler, &x->limit );
			}
			
			n = cntler->sector_count;
			if ( n == 0 ) {
				cntler->status.drive_request = FALSE;
				cntler->status.drive_seek_complete = TRUE;
				Controller_raise_interrupt ( cntler );
				return 0;
			}

			if ( n > MAX_MULT_SECTORS ) { n = MAX_MULT_SECTORS; }
			cntler->buffer_index = 0;
			cntler->buffer_size = n * 512;
			len = cntler->buffer_size;
		}

		if ( transfer_size <= 0 ) { break; }

		if ( len > transfer_size ) { len = transfer_size; }

		Monitor_mmove_rd ( phys_addr, cntler->buffer + cntler->buffer_index, len );

		cntler->buffer_index += len;
		transfer_size -= len;
		phys_addr += len;
	}

	return transfer_size1 - transfer_size;
}

static bool_t
__dma_loop ( struct drive_t *x, int i )
{
	struct dma_t *dma = &x->dma;
	bit32u_t addr = dma->addr + i * 8;
	struct {
		bit32u_t addr, size;
	} prd;
	int len;

	mem_check_for_dma_access ( addr );
	prd.addr = Monitor_read_dword_with_paddr ( addr );
	prd.size = Monitor_read_dword_with_paddr ( addr + 4 );

	len = prd.size & 0xfffe;
	if ( len == 0 ) { len = 0x10000; }
/*
	Print_color ( stdout, GREEN,
		      "addr=%#x, len=%#x(%#x), kind=%s\n",
		      prd.addr, len, prd.size,
		      (dma->ma_kind == MEM_ACCESS_READ) ? "read" : "write" );
*/
	while ( len > 0 ) {
		int n = 0;
		switch ( dma->ma_kind ) {
		case MEM_ACCESS_READ:  n = __dma_loop_read ( x, prd.addr, len ); break;
		case MEM_ACCESS_WRITE: n = __dma_loop_write ( x, prd.addr, len ); break;
		default:               Match_failure ( "__dma_loop\n" );
		}

		if ( n == 0 ) {
			return TRUE;
		}

		prd.addr += n;
		len -= n;
	}
	
	/* end of transfer */
	return ( prd.size & 0x80000000 );
}

static void
dma_loop ( struct drive_t *x )
{
	struct dma_t *dma = &x->dma;
	int i;

//	Print_color ( stdout, GREEN, "dma_loop: start\n" );

	/* at most one page to avoid hanging if erroneous parameters */
	for ( i = 0; i < 512; i++ ) {
		bool_t b;

		b = __dma_loop ( x, i );
		if ( b ) { break; }
	}

	/* end of transfer */
	dma->status &= ~BM_STATUS_DMAING;
	dma->status |= BM_STATUS_INT;

	dma->has_started = FALSE;

//	Print_color ( stdout, GREEN, "dma_loop: end\n" );
}

static void
dma_start ( struct drive_t *x, mem_access_kind_t ma_kind )
{
	struct controller_status_t *status = &x->cntler.status;
	struct dma_t *dma = &x->dma;

	status->drive_request = TRUE;
	status->drive_seek_complete = TRUE;
	
	dma->has_started = TRUE;
	dma->ma_kind = ma_kind;

	if ( dma->status & BM_STATUS_DMAING ) {
		dma_loop ( x );
	}
}

static void
command_read_dma ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;

	cntler->buffer_index = 0;
	cntler->buffer_size = 0;
	dma_start ( x, MEM_ACCESS_READ );
}

static void
command_write_dma ( struct drive_t *x )
{
	struct controller_t *cntler = &x->cntler;
	int n;

	n = cntler->sector_count;
	if ( n > MAX_MULT_SECTORS ) { n = MAX_MULT_SECTORS; }

	cntler->buffer_index = 0;
	cntler->buffer_size = n * MAX_CNTLER_BUFSIZE;
	dma_start ( x, MEM_ACCESS_WRITE );
}

/*******************/

enum protocol {
	PROTOCOL_PIO_DATA,
	PROTOCOL_NON_DATA,
	PROTOCOL_DMA,
	PROTOCOL_UNKNOWN
};
typedef enum protocol		protocol_t;

static void
write_command_pre_process ( struct controller_t *x, bit8u_t val )
{
	struct controller_status_t *status = &x->status;
  
	Controller_clear_pending_interrupt ( x ); // 無意味？

	x->current_command = val;
	x->error_register = 0; 

	ASSERT ( status->busy == FALSE );
	ASSERT ( status->drive_ready );
}

static protocol_t
get_protocol ( bit8u_t val )
{
	protocol_t retval = PROTOCOL_UNKNOWN;

	switch ( val ) {
	case WIN_READ: 
	case WIN_READ_ONCE:
	case WIN_WRITE:
	case WIN_IDENTIFY:
		retval = PROTOCOL_PIO_DATA;
		break;

	case WIN_RECAL:
	case WIN_SPECIFY:
		retval = PROTOCOL_NON_DATA;
		break;

	case WIN_READDMA:
	case WIN_READDMA_ONCE:
	case WIN_WRITEDMA:
	case WIN_WRITEDMA_ONCE:
		retval = PROTOCOL_DMA;
		break;

	case WIN_PIDENTIFY:
		retval = PROTOCOL_UNKNOWN;
		break;

	default:
		Match_failure ( "get_protocol\n" );
	}
	return retval;
}

static void
write_command_post_process ( struct controller_t *x, bit8u_t val )
{
	struct controller_status_t *status = &x->status;
	protocol_t kind;

	ASSERT ( status->drive_ready );
   
	status->busy = FALSE;

	if ( x->error_register > 0 ) {
		status->error = TRUE; 

#if 1
		status->drive_write_fault = TRUE; // device fault の場合

		// 本当に必要かよく分からない

		// stauts->drive_write_fault = FALSE
		status->drive_seek_complete = FALSE;

		status->corrected_data = FALSE;
		status->index = FALSE;
#endif

		return;
	}

	status->error = FALSE;
	kind = get_protocol ( val );
	switch ( kind ) {
	case PROTOCOL_PIO_DATA:
		ASSERT ( ! status->drive_request );

		/* the device is ready to transfer a word or byte of data
		 * between the host and device ( p. 28 ) */
		status->drive_request = TRUE;	 

		status->drive_write_fault = FALSE;

		status->drive_seek_complete = TRUE;
		status->corrected_data = FALSE;

		break;
	case PROTOCOL_NON_DATA:
		ASSERT ( ! status->drive_request );
#if 1
		status->drive_seek_complete = TRUE;
#endif
		break;

	case PROTOCOL_DMA:
		break;
		
	case PROTOCOL_UNKNOWN:
		assert ( 0 );
		break;
	default:
		Match_failure ( "write_command_post_process\n" );
	}

	Controller_raise_interrupt ( x );
}

static void
write_command ( struct hard_drive_t *x, bit8u_t val, size_t len )
{
	struct drive_t *drive = get_selected_drive ( x );
	struct controller_t *cntler = &drive->cntler;

	ASSERT ( x != NULL );
	ASSERT ( len == 1 );

	//DPRINT2 ( " + COMMAND: drive_select=%d\n", x->channel.drive_select );

	if ( drive->disk_image_fd == -1 )
		return;
   

	if ( ( val & 0xf0 ) == WIN_RECAL ) // ???
		val = WIN_RECAL;

	write_command_pre_process ( cntler, val );
   
	switch ( val ) {
	case WIN_RECAL:   	/* 0x10 */ command_recalibrate ( cntler ); break;
	case WIN_READ:   	/* 0x20 */
	case WIN_READ_ONCE: 	/* 0x21 */ command_read_sectors ( drive ); break;
	case WIN_WRITE:	  	/* 0x30 */ command_write_sectors ( cntler ); break;
	case WIN_SPECIFY: 	/* 0x91 */ command_initialize_device_parameters ( drive ); break;
	case WIN_PIDENTIFY: 	/* 0xa1 */ command_identify_packet_device ( cntler ); break;
	case WIN_IDLEIMMEDIATE: /* 0xe1 */ command_idle_immediate ( cntler ); break;
	case WIN_IDENTIFY: 	/* 0xe3 */ command_identify_device ( drive ); break;

	case WIN_READDMA:  	/* 0xc8 */
	case WIN_READDMA_ONCE: 	/* 0xc9 */ command_read_dma ( drive ); break;
		
	case WIN_WRITEDMA:	/* 0xca */
	case WIN_WRITEDMA_ONCE: /* 0xcb */ command_write_dma ( drive ); break;



	case WIN_STANDBYNOW1: 	/* 0xe0 */
	case WIN_STANDBYNOW2: 	/* 0x94 */
		command_aborted ( cntler ); /* TODO */ 
		break; 

	default:	 	    Match_failure ( "write_command: %#x\n", val );
	}

	write_command_post_process ( cntler, val );
}

static void
write_cmd_regs ( struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	ASSERT ( x != NULL );

#if 0
	/* [???] write data のときは， ( drive_request == TRUE ) でなければいけない */
	ASSERT ( cmd_regs_are_ready ( get_selected_controller ( x ) ) ); /* p. 13 */
#endif

	switch ( addr ) {
	case HD_DATA:  /* 0x1f0 */ write_data ( x, val, len ); break;
	case HD_PRECOMP: /* 0x1f1 */ write_precomp ( x, val, len ); break;
	case HD_NSECTOR: /* 0x1f2 */ write_nsector ( x, val, len ); break;
	case HD_SECTOR: /* 0x1f3 */ write_sector ( x, val, len ); break;
	case HD_LCYL:  /* 0x1f4 */ write_lcyl ( x, val, len ); break;
	case HD_HCYL:  /* 0x1f5 */ write_hcyl ( x, val, len ); break;
	case HD_CURRENT: /* 0x1f6 */ write_current ( x, val, len ); break;
	case HD_COMMAND: /* 0x1f7 */ write_command ( x, val, len ); break;
	default:	  	   Match_failure ( "write_cmd_regs\n" );
	}
}

/**********************************/

static void
write_device_contrl_reset_sub ( struct controller_t *x )
{
	struct controller_status_t *status = &x->status;

	x->reset_in_progress = TRUE;
	status->busy = FALSE;


	x->disable_irq = FALSE;

	x->current_command = 0;
	x->error_register = MARK_ERR; // 0x01: diagnostic code: no error
   
	x->buffer_index = 0;
	// x->sectors_per_block = 0x80;

	x->lba_mode = FALSE;
	    
	status->drive_ready = TRUE;
	status->drive_write_fault = FALSE;
	status->drive_seek_complete = TRUE;
	status->drive_request = FALSE;
	status->corrected_data = FALSE;
	//status->index = FALSE;
	status->error = FALSE;
   
	Controller_clear_pending_interrupt ( x );
}

static void
write_device_contrl_reset ( struct hard_drive_t *x )
{
	int i;
	ASSERT ( x != NULL );

	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		write_device_contrl_reset_sub ( cntler );
	}
}

/***************/

static void
write_device_contrl_reset_in_progress_sub ( struct controller_t *x )
{
	struct controller_status_t *status = &x->status;

	status->busy = FALSE;
	status->drive_ready = TRUE;   

	x->reset_in_progress = FALSE;

	// この初期化は，ここでやる？
	Chs_init ( &x->addr.chs );
	x->sector_count = 1; 
}

static void
write_device_contrl_reset_in_progress ( struct hard_drive_t *x )
{
	int i;

	ASSERT ( x != NULL );

	for ( i = 0; i < NUM_OF_DRIVERS; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;
		write_device_contrl_reset_in_progress_sub ( cntler );
	}
}

static void
write_device_contrl ( struct hard_drive_t *x, bit16u_t addr, bit8u_t val, size_t len )
{
	struct controller_t *cntler = get_selected_controller ( x );
	bool_t prev_reset;

	//DPRINT2 ( " + DEVICE_CONTROL\n" );

	ASSERT ( len == 1 );

	prev_reset = cntler->reset;

	cntler->reset = SUB_BIT ( val, 2, 1 );
	cntler->disable_irq = SUB_BIT ( val, 1, 1 );
	ASSERT ( TEST_BIT ( val, 0 ) == FALSE );

	if ( cntler->reset ) {
		if ( prev_reset == FALSE ) { write_device_contrl_reset ( x ); }
	} else {
		if ( cntler->reset_in_progress ) { write_device_contrl_reset_in_progress ( x ); }
	}
}

static void
write_cntl_regs ( struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	ASSERT ( x != NULL );

	switch ( addr ) {
	case HD_CMD: /* 0x3f6 */ write_device_contrl ( x, addr, val, len ); break;
	case 0x3f7:	   ASSERT ( 0 ); break;
	default:		   Match_failure ( "HardDrive_write_cntl_regs\n" );
	}
}

/**********************************/

void
HardDrive_write ( struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	hard_drive_access_kind_t kind;

	ASSERT ( x != NULL );

//	Print ( stderr, "HardDrive_write: addr=%#x, val=%#x\n", addr, val );

	/*
	DPRINT2 ( "HardDrive_write: addr=%#x, val=%#x]\n", addr, val );
	DPRINT ( "HardDrive_write: addr=%#x, BUSY=%d,DREADY=%d,WFAULT=%d, SEEK=%d, DRQ=%d, COR=%d, INDEX=%d, ERROR=%d\n", 
	    addr, 
	    get_selected_controller ( x )->status.busy,
	    get_selected_controller ( x )->status.drive_ready,
	    get_selected_controller ( x )->status.drive_write_fault,
	    get_selected_controller ( x )->status.drive_seek_complete,
	    get_selected_controller ( x )->status.drive_request,
	    get_selected_controller ( x )->status.corrected_data,
	    get_selected_controller ( x )->status.index,
	    get_selected_controller ( x )->status.error );
	*/

	kind = addr_to_hard_drive_access_kind ( addr );
	switch ( kind ) {
	case HD_ACCESS_PRIMARY_CMD_REGS:	write_cmd_regs ( x, addr, val, len ); break;
	case HD_ACCESS_PRIMARY_CNTL_REGS:	write_cntl_regs ( x, addr, val, len ); break;
	case HD_ACCESS_SECOND_CMD_REGS:
	case HD_ACCESS_SECOND_CNTL_REGS:	break; /* [TODO] */
	default:				Match_failure ( "HardDrive_write\n" );
	}

	/*
	DPRINT ( "[HardDrive_write: addr=%#x, BUSY=%d,DREADY=%d,WFAULT=%d, SEEK=%d, DRQ=%d, COR=%d, INDEX=%d, ERROR=%d\n", 
	    addr, 
	    get_selected_controller ( x )->status.busy,
	    get_selected_controller ( x )->status.drive_ready,
	    get_selected_controller ( x )->status.drive_write_fault,
	    get_selected_controller ( x )->status.drive_seek_complete,
	    get_selected_controller ( x )->status.drive_request,
	    get_selected_controller ( x )->status.corrected_data,
	    get_selected_controller ( x )->status.index,
	    get_selected_controller ( x )->status.error );
	*/
}

/****************************************************************/

int
HardDrive_try_get_irq ( struct hard_drive_t *x )
{
	int i;

	ASSERT ( x != NULL );
   
	for ( i = 0; i < 2; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;

		if ( cntler->requesting_irq ) {
			cntler->requesting_irq = FALSE;
			return IRQ_EIDE ( 0 );
		}
	}

	return IRQ_INVALID;
}

/*************************************************************/

bool_t
HardDrive_check_irq ( struct hard_drive_t *x )
{
	int i;

	ASSERT ( x != NULL );
   
	for ( i = 0; i < 2; i++ ) {
		struct controller_t *cntler = &x->channel.drives[i].cntler;

		if ( cntler->requesting_irq ) {
			return TRUE;
		}
	}

	return FALSE;
}


/*************************************************************/

bit32u_t
HardDriveIoMap_read ( struct hard_drive_t *x, bit16u_t addr, size_t len )
{
	bit32u_t ret = 0;
	bit16u_t n;
	int kind;
	int offset;
	struct dma_t *dma;

	ASSERT ( x != NULL );

	n = addr - 0xc000;
	kind = TEST_BIT ( n, 3 );
	offset = SUB_BIT ( n, 0, 3 );
	dma = & x->channel.drives[kind].dma;

	switch ( offset ) {
	case 0x0: assert ( len == 1 ); ret = dma->command; break;
	case 0x2: assert ( len == 1 ); ret = dma->status; break;
	case 0x4: assert ( len == 4 ); ret = dma->addr; break;
	default:  Match_failure ( "HardDriveIoMap_read: addr=%#x, kind=%#x, offset=%#x\n", addr, kind, offset );
	}

/*	Print_color ( stdout, CYAN,
		      "read: addr=%#x(%#x,%#x), val=%#x, len=%#x\n",
		      addr, kind, offset, ret, len );
*/

	return ret;
}

void
HardDriveIoMap_write ( struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len )
{
	bit16u_t n;
	int kind;
	int offset;
	struct drive_t *drive;
	struct dma_t *dma;

	ASSERT ( x != NULL );

	n = addr - 0xc000;
	kind = TEST_BIT ( n, 3 );
	offset = SUB_BIT ( n, 0, 3 );

	drive = &x->channel.drives[kind];
	dma = & drive->dma;
/*
	Print_color ( stdout, CYAN,
		      "write: addr=%#x(%#x,%#x), val=%#x, len=%#x\n",
		      addr, kind, offset, val, len );
*/
	switch ( offset ) {
	case 0x0: Dma_write_command ( dma, val ); break;
	case 0x2: Dma_write_status ( dma, val ); break;
	case 0x4: Dma_write_addr ( dma, val ); break;		
	default:  Match_failure ( "HardDriveIoMap_write: addr=%#x, kind=%#x, offset=%#x\n", addr, kind, offset );
	}

	if ( offset == 0 ) {
		if ( ( dma->has_started ) && ( dma->status & BM_STATUS_DMAING ) ) {
			dma_loop ( drive );
		}
	}

}
