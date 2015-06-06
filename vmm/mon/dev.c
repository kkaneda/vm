#include "vmm/mon/mon.h"
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>


// #include <linux/bcd.h>
// #include <linux/mc146818rtc.h>


static void finish_io_access ( struct mon_t *mon );
static void start_io_access ( struct mon_t *mon );

enum io_kind {
	IO_KIND_UNKNOWN,
	IO_KIND_DMA_CONTROLLER,
	IO_KIND_INTERRUPT_CONTROLLER,
	IO_KIND_SYSTEM_TIMER, 
	IO_KIND_KEYBOARD_MOUSE,
	IO_KIND_SYSTEM_CONTROL_PORT,
	IO_KIND_RTC_CMOS_NMI,
	IO_KIND_DMA_PAGE_REGISTER,
	IO_KIND_FLOATING_POINT_UNIT,
	IO_KIND_IDE,
	IO_KIND_IDE_IOMAP,
	IO_KIND_COM,
	IO_KIND_PCI,
	IO_KIND_LPT,
	IO_KIND_VGA_PLUS
};
typedef enum io_kind	io_kind_t;

struct io_addr_entry_t {
	bit16u_t		addrs[2]; /* addrs[0] ==> lower, addrs[1] ==> upper */ 
	io_kind_t		kind;
	char			*name;
};

static struct io_addr_entry_t io_addr_map[] =
{ { { 0x0000, 0x0010 }, IO_KIND_DMA_CONTROLLER, "DMA_CONTROLLER" },
  { { 0x0020, 0x0022 }, IO_KIND_INTERRUPT_CONTROLLER, "INTERRUPT_CONTROLLER" },
  { { 0x0040, 0x0044 }, IO_KIND_SYSTEM_TIMER, "SYSTEM_TIMER" }, 
  { { 0x0060, 0x0061 }, IO_KIND_KEYBOARD_MOUSE, "KEYBOARD_MOUSE" },
  { { 0x0061, 0x0062 }, IO_KIND_SYSTEM_CONTROL_PORT, "SYSTEM_CONTROL_PORT" },
  { { 0x0064, 0x0065 }, IO_KIND_KEYBOARD_MOUSE, "KEYBOARD_MOUSE" },
  { { 0x0070, 0x0072 }, IO_KIND_RTC_CMOS_NMI, "RTC_CMOS_NMI" },
  { { 0x0080, 0x0090 }, IO_KIND_DMA_PAGE_REGISTER, "DMA_PAGE_REGISTER" },
  { { 0x0092, 0x0093 }, IO_KIND_SYSTEM_CONTROL_PORT, "SYSTEM_CONTROL_PORT" },
  { { 0x00a0, 0x00a2 }, IO_KIND_INTERRUPT_CONTROLLER, "INTERRUPT_CONTROLLER" },
  { { 0x00c0, 0x00e0 }, IO_KIND_DMA_CONTROLLER, "DMA_CONTROLLER" },
  { { 0x00f0, 0x0100 }, IO_KIND_FLOATING_POINT_UNIT, "FLOATING_POINT_UNIT" }, 
  { { 0x0170, 0x0178 }, IO_KIND_IDE, "IDE" },
  { { 0x01f0, 0x01f8 }, IO_KIND_IDE, "IDE" },
  { { 0x02f8, 0x0300 }, IO_KIND_COM, "COM" },
  { { 0x0376, 0x0378 }, IO_KIND_IDE, "IDE" },
  { { 0x0378, 0x0380 }, IO_KIND_LPT, "LPT" },
  { { 0x03c0, 0x03e0 }, IO_KIND_VGA_PLUS, "VGA_PLUS" },
  { { 0x03f6, 0x03f8 }, IO_KIND_IDE, "IDE" },
  { { 0x03f8, 0x0400 }, IO_KIND_COM, "COM" },
  { { 0x0cf8, 0x0d00 }, IO_KIND_PCI, "PCI" },
  { { 0xc000, 0xc010 }, IO_KIND_IDE_IOMAP, "IO_KIND_IDE_IOMAP" },
};

typedef bit32u_t read_func_t( struct mon_t *mon, bit16u_t, size_t );
typedef void write_func_t( struct mon_t *mon, bit16u_t, bit32u_t, size_t );

struct dev_func_entry_t {
	io_kind_t		kind;
	read_func_t		*read_func;
	write_func_t		*write_func;
};

static inline bit32u_t __dma_controller_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __pic_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __pit_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __keyboard_mouse_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __rtc_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __dma_page_register_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __hard_drive_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __hard_drive_iomap_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __coms_read ( struct mon_t *mon, bit16u_t addr, size_t len );
static inline bit32u_t __pci_read ( struct mon_t *mon, bit16u_t addr, size_t len );

static inline void __dma_controller_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __pic_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __pit_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __keyboard_mouse_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __rtc_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __dma_page_register_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __hard_drive_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __hard_drive_iomap_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __coms_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );
static inline void __pci_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len );

static inline bit32u_t
ignore_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	/* do nothing */
	return 0;
}

static inline void
ignore_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	/* do nothing */
}

static struct dev_func_entry_t dev_func_map[] =
{ { IO_KIND_UNKNOWN, 			NULL, NULL },
  { IO_KIND_DMA_CONTROLLER, 	 	&__dma_controller_read, &__dma_controller_write },
  { IO_KIND_INTERRUPT_CONTROLLER,	&__pic_read, &__pic_write },
  { IO_KIND_SYSTEM_TIMER, 		&__pit_read, &__pit_write },
  { IO_KIND_KEYBOARD_MOUSE,		&__keyboard_mouse_read, &__keyboard_mouse_write },
  { IO_KIND_SYSTEM_CONTROL_PORT, 	&__pit_read, &__pit_write },
  { IO_KIND_RTC_CMOS_NMI, 		&__rtc_read, &__rtc_write },
  { IO_KIND_DMA_PAGE_REGISTER, 		&__dma_page_register_read, &__dma_page_register_write },
  { IO_KIND_FLOATING_POINT_UNIT, 	NULL, NULL },
  { IO_KIND_IDE, 			&__hard_drive_read, &__hard_drive_write },
  { IO_KIND_COM, 			&__coms_read, &__coms_write },
  { IO_KIND_PCI, 			&__pci_read, &__pci_write },
  { IO_KIND_LPT, 			NULL, NULL },
  { IO_KIND_VGA_PLUS, 			&ignore_read, &ignore_write },
  { IO_KIND_IDE_IOMAP, 			&__hard_drive_iomap_read, &__hard_drive_iomap_write },
};


#ifndef SLOWDOWN_PORT
# define SLOWDOWN_PORT 		0x80
#endif



static size_t
nr_io_addr_entries ( void )
{
	return sizeof ( io_addr_map ) / sizeof ( struct io_addr_entry_t ); 
}

static io_kind_t
addr_to_io_kind ( bit16u_t addr )
{
	int i;

	for ( i = 0; i < nr_io_addr_entries ( ); i++ ) {
		struct io_addr_entry_t *x = &io_addr_map[i];
	 
		if ( ( addr >= x->addrs[0] ) && ( addr < x->addrs[1] ) )
			return x->kind;
	}

	return IO_KIND_UNKNOWN;
}

const char *
io_kind_to_string ( io_kind_t kind )
{
	int i;

	for ( i = 0; i < nr_io_addr_entries ( ); i++ ) {
		struct io_addr_entry_t *x = &io_addr_map[i];
		if ( x->kind == kind ) 
			return x->name;
	}

	return " ( unknown )";
}

static size_t
nr_dev_func_entries ( void )
{
	return sizeof ( dev_func_map ) / sizeof ( struct dev_func_entry_t ); 
}

static read_func_t *
get_dev_read_func ( io_kind_t kind )
{
	int i;

	for ( i = 0; i < nr_dev_func_entries ( ); i++ ) {
		struct dev_func_entry_t *x = &dev_func_map[i];
	 
		if ( x->kind == kind )
			return x->read_func;
	}

	return NULL;
}

static write_func_t *
get_dev_write_func ( io_kind_t kind )
{
	int i;

	for ( i = 0; i < nr_dev_func_entries ( ); i++ ) {
		struct dev_func_entry_t *x = &dev_func_map[i];
	 
		if ( x->kind == kind )
			return x->write_func;
	}

	return NULL;
}

/****************************************************************/

/* [TODO] */
static bit32u_t
DmaPageRegister_read ( bit16u_t addr, size_t len )
{
	Print_color ( stdout, RED, "DmaPageRegister read\n" );
	return 0;
}

/* [TODO] */
static void
DmaPageRegister_write ( bit16u_t addr )
{
	/* Linux uses this port to slow down the machine
	 * [Reference] kernel source: inclue/asm-i386/io.h */
	ASSERT ( addr == SLOWDOWN_PORT );   
}

/****************************************************************/

/* [TODO] */
static bit32u_t
DmaController_read ( bit16u_t addr, size_t len )
{
	Print_color ( stdout, RED, "DmaController read\n" );
	return 0;
}

static void
DmaController_write ( bit16u_t addr )
{
	ASSERT ( addr = 0x000a ); /* fd_disable_dma */
}

/****************************************************************/

/* [TODO] */
static bit32u_t KeyBoardMouse_read ( void ) 
{ 
	return 0;
}

static void KeyBoardMouse_write ( void ) { }

/****************************************************************/

void
init_devices ( struct mon_t *mon, const struct config_t *config )
{
	struct devices_t *x = &mon->devs;

	ASSERT ( x != NULL );
	ASSERT ( config != NULL );

	Vga_init ( &x->vga, mon->cpuid, mon->pmem.base );
	Pic_init ( &x->pic );
	Rtc_init ( &x->rtc );
	Pit_init ( &x->pit, mon );
	Coms_init ( x->coms, mon->pid, is_bootstrap_proc ( mon ) );
	Pci_init ( &x->pci );

	HardDrive_init ( &x->hard_drive, config->disk );

	/* [TODO] init miscellenous devices */

	Pthread_mutex_init ( &x->mp, NULL );

#ifdef ENABLE_MP
	x->head_rirq = 0;
	x->tail_rirq = 0;
#endif
}

/*************/

#ifdef ENABLE_MP 

/*************/

static void
send_inp_request ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	struct msg_t *msg;

//	Print ( stdout, "[CPU%d] SEND INP REQUEST to %#x\n", mon->cpuid, BSP_CPUID );

	msg = Msg_create3 ( MSG_KIND_INPUT_PORT, ( int ) addr, len );
	Comm_send ( mon->comm, msg, BSP_CPUID );
	Msg_destroy ( msg );
}

static bool_t
recv_inp_response_sub ( struct mon_t *mon, bit16u_t addr, size_t len, struct msg_t *msg, bit32u_t *val )
{
	struct msg_input_port_ack_t *x;

//	Print ( stdout, "[CPU%d] RECV MSG from %#x\n", mon->cpuid, msg->hdr.src_id );

	if ( msg->hdr.kind != MSG_KIND_INPUT_PORT_ACK ) {
		handle_msg ( mon, msg );
		return FALSE;
	}

//	Print ( stdout, "    recv inp response\n" );

	x = Msg_to_msg_input_port_ack ( msg );
	
	assert ( x->addr = addr );
	assert ( x->len = len );
	*val = x->val;				
	
	return TRUE;
}

static bit32u_t
recv_inp_response ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	bit32u_t val;
	bool_t f;

	f = FALSE;
	while ( ! f ) {
		struct msg_t *msg;

		msg = Comm_remove_msg ( mon->comm );
		f = recv_inp_response_sub ( mon, addr, len, msg, &val );
		Msg_destroy ( msg );
	}
	
	return val;	
}

static inline bit32u_t
inp_from_remote ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	send_inp_request ( mon, addr, len );
	return recv_inp_response ( mon, addr, len );
}

#else  /* ! ENABLE_MP */

static inline bit32u_t
inp_from_remote ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return 0;
}

#endif /* ENABLE_MP */

static inline bit32u_t 
__dma_controller_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return DmaController_read ( addr, len );
}

static inline bit32u_t 
__pic_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return Pic_read ( &mon->devs.pic, addr, len );
}

static inline bit32u_t 
__pit_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return Pit_read ( &mon->devs.pit, addr, len );
}

static inline bit32u_t 
__rtc_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return Rtc_read ( &mon->devs.rtc, addr, len );
}

static inline bit32u_t 
__dma_page_register_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
//	return DmaController_read ( &mon->devs.pic,addr, len );
	return DmaPageRegister_read ( addr, len );
}

static inline bit32u_t 
__keyboard_mouse_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return KeyBoardMouse_read ( );
}

static inline bit32u_t 
__hard_drive_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return HardDrive_read ( &mon->devs.hard_drive, addr, len );
}

static inline bit32u_t 
__hard_drive_iomap_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return HardDriveIoMap_read ( &mon->devs.hard_drive, addr, len );
}

static inline bit32u_t 
__coms_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return Coms_read ( mon->devs.coms, addr, len );	
}

static inline bit32u_t 
__pci_read ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	return Pci_read ( &mon->devs.pci, addr, len );
}

static bit32u_t
__inp ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	io_kind_t kind;
	read_func_t *f;
	bit32u_t ret = 0;

	if ( is_application_proc ( mon ) )
		return inp_from_remote ( mon, addr, len );

	kind = addr_to_io_kind ( addr );
	f = get_dev_read_func ( kind );
	if ( f != NULL ) {
		ret = (*f) ( mon, addr, len );
	}

	DPRINT ( "inp: kind=%s, addr=%#x, retval=%#x, len=%#x\n",
		 io_kind_to_string ( kind ), addr, ret, len );

	return ret;
}

bit32u_t
inp ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	bit32u_t ret;

	ASSERT ( mon != NULL );

	start_time_counter ( &mon->stat.dev_rd_counter );
	mon->stat.nr_dev_rd++;

	start_io_access ( mon );
	ret = __inp ( mon, addr, len );
	finish_io_access ( mon );

	stop_time_counter ( &mon->stat.dev_rd_counter );

	long long n = mon->stat.dev_rd_counter.end - mon->stat.dev_rd_counter.start;
	if ( ( n < mon->stat.min_dev_rd_count ) || ( mon->stat.min_dev_rd_count == 0LL ) ) {
		mon->stat.min_dev_rd_count = n;
	}

	if ( ( n > mon->stat.max_dev_rd_count  ) || ( mon->stat.max_dev_rd_count == 0LL ) ) {
		mon->stat.max_dev_rd_count = n;
	}

	return ret;
}

#ifdef ENABLE_MP

static bool_t
recv_outp_response_sub ( struct mon_t *mon, bit16u_t addr, size_t len, struct msg_t *msg )
{
	struct msg_output_port_ack_t *x;
	struct devices_t *devs = &mon->devs;

//	Print ( stdout, "[CPU%d] RECV MSG from %#x\n", mon->cpuid, msg->hdr.src_id );

	if ( msg->hdr.kind != MSG_KIND_OUTPUT_PORT_ACK ) {
		handle_msg ( mon, msg );
		return FALSE;
	}

//	Print ( stdout, "    recv outp response\n" );

	x = Msg_to_msg_output_port_ack ( msg );
	
	assert ( x->addr = addr );
	assert ( x->len = len );

	if ( x->irq != IRQ_INVALID ) {
		devs->remote_irqs[devs->tail_rirq] = x->irq;
		devs->tail_rirq = ( devs->tail_rirq + 1 ) % 1024;
	}
	
	return TRUE;
}

static void
recv_outp_response ( struct mon_t *mon, bit16u_t addr, size_t len )
{
	bool_t f;

	f = FALSE;
	while ( ! f ) {
		struct msg_t *msg;

		msg = Comm_remove_msg ( mon->comm );
		f = recv_outp_response_sub ( mon, addr, len, msg );	
		Msg_destroy ( msg );
	}
}

static void
send_outp_request ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	struct msg_t *msg;

//	Print ( stdout, "[CPU%d] SEND OUTP REQUEST to %#x\n", mon->cpuid, BSP_CPUID );

	msg = Msg_create3 ( MSG_KIND_OUTPUT_PORT, ( int ) addr, ( int ) val, len );
	Comm_send ( mon->comm, msg, BSP_CPUID );
	Msg_destroy ( msg );
}

static inline void
outp_to_remote ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	send_outp_request ( mon, addr, val, len );
	recv_outp_response ( mon, addr, len );
}

#else  /* ! ENABLE_MP */

static inline void
outp_to_remote ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	assert ( 0 );
}

#endif /* ENABLE_MP */

static inline void
__dma_controller_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	DmaController_write ( addr );
}

static inline void
__pic_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	Pic_write ( &mon->devs.pic, addr, val, len );
}

static inline void
__pit_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	Pit_write ( &mon->devs.pit, addr, val, len );
}

static inline void
__keyboard_mouse_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	KeyBoardMouse_write ( );
}

static inline void
__rtc_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	Rtc_write ( &mon->devs.rtc, addr, val, len );
}

static inline void
__dma_page_register_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	DmaPageRegister_write ( addr );
}

static inline void
__hard_drive_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	HardDrive_write ( &mon->devs.hard_drive, addr, val, len );
}

static inline void
__hard_drive_iomap_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	HardDriveIoMap_write ( &mon->devs.hard_drive, addr, val, len );
}

static inline void
__coms_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	Coms_write ( mon->devs.coms, addr, val, len );
}

static inline void
__pci_write ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	Pci_write ( &mon->devs.pci, addr, val, len );
}

static void
__outp ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	io_kind_t kind;
	write_func_t *f;

	if ( is_application_proc ( mon ) ) {
		outp_to_remote ( mon, addr, val, len );
		return;
	}

	kind = addr_to_io_kind ( addr );
	f = get_dev_write_func ( kind );
	if ( f != NULL ) { 
		(*f) ( mon, addr, val, len );
	}

	DPRINT ( "outp: kind=%s, addr=%#x, val=%#x, len=%#x\n",
		 io_kind_to_string ( kind ), addr, val, len );
}

void
outp ( struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len )
{
	ASSERT ( mon != NULL );

	start_time_counter ( &mon->stat.dev_wr_counter );
	mon->stat.nr_dev_wr++;

	start_io_access ( mon );
	__outp ( mon, addr, val, len );
	finish_io_access ( mon );

	stop_time_counter ( &mon->stat.dev_wr_counter );

	long long n = mon->stat.dev_wr_counter.end - mon->stat.dev_wr_counter.start;
	if ( ( n < mon->stat.min_dev_wr_count ) || ( mon->stat.min_dev_wr_count == 0LL ) ) {
		mon->stat.min_dev_wr_count = n;
	}

	if ( ( n > mon->stat.max_dev_wr_count  ) || ( mon->stat.max_dev_wr_count == 0LL ) ) {
		mon->stat.max_dev_wr_count = n;
	}
}

static int
__try_generate_external_irq ( struct mon_t *mon, bool_t ignore_pit )
{
	struct devices_t *devs = &mon->devs;
	int irq;

	ASSERT ( devs != NULL );

#ifdef ENABLE_MP
	if ( is_application_proc ( mon ) ) {
		int ret;

		if ( devs->head_rirq == devs->tail_rirq ) {
			return IRQ_INVALID;
		}

//		Print ( stdout, "handle irq\n" );
		ret = devs->remote_irqs[devs->head_rirq];
		devs->head_rirq = ( devs->head_rirq + 1 ) % 1024;
		return ret;
	}
#endif /* ENABLE_MP */

	irq = HardDrive_try_get_irq ( &devs->hard_drive );
	if ( irq != IRQ_INVALID ) 
		return irq;

	irq = Coms_try_get_irq ( devs->coms );
	if ( irq != IRQ_INVALID )
		return irq;

	if ( ! ignore_pit ) {
		irq = Pit_try_get_irq ( &devs->pit );
		if ( irq != IRQ_INVALID ) {
//			Print_color ( stdout, GREEN, "*" );
			return irq;
		}
	}

	return IRQ_INVALID;
}

int
try_generate_external_irq ( struct mon_t *mon, bool_t ignore_pit )
{
	int irq;

	start_io_access ( mon );
	irq = __try_generate_external_irq ( mon, ignore_pit );
	finish_io_access ( mon );

	return irq;
}


bool_t
check_external_irq ( struct mon_t *mon )
{
	struct devices_t *devs = &mon->devs;
	bool_t ret;

	ASSERT ( devs != NULL );

#ifdef ENABLE_MP
	if ( is_application_proc ( mon ) ) {
		return FALSE;
	}
#endif /* ENABLE_MP */

	start_io_access ( mon );
	ret = ( ( HardDrive_check_irq ( &devs->hard_drive ) ) || 
		 ( Coms_check_irq ( devs->coms ) ) ||
		 ( Pit_check_irq ( &devs->pit ) )  );
	finish_io_access ( mon );

	return ret;
}

bool_t
accessing_io ( struct mon_t *mon )
{
	struct devices_t *devs = &mon->devs;
	bool_t ret;
	
	Pthread_mutex_lock ( &devs->mp );
	ret = devs->is_accessing;
	Pthread_mutex_unlock ( &devs->mp );
	
	return ret;
}

static void
start_io_access ( struct mon_t *mon )
{
	struct devices_t *devs = &mon->devs;

	Pthread_mutex_lock ( &devs->mp );
	assert ( ! devs->is_accessing );
	devs->is_accessing = TRUE;
	Pthread_mutex_unlock ( &devs->mp );
}

static void
finish_io_access ( struct mon_t *mon )
{
	struct devices_t *devs = &mon->devs;

	Pthread_mutex_lock ( &devs->mp );
	assert ( devs->is_accessing );
	devs->is_accessing = FALSE;
	Pthread_mutex_unlock ( &devs->mp );
}

/****************************************************************/

void
pack_devices ( struct mon_t *mon, int fd )
{
	struct devices_t *x = &mon->devs;

	ASSERT ( x != NULL );

	Pic_pack ( &x->pic, fd );
	Rtc_pack ( &x->rtc, fd );
	Pit_pack ( &x->pit, fd );
	Coms_pack ( x->coms, fd );
	Pci_pack ( &x->pci, fd );
	HardDrive_pack ( &x->hard_drive, fd );

	/* [TODO] init miscellenous devices */

#ifdef ENABLE_MP
	Bit32u_pack ( x->head_rirq, fd );
	Bit32u_pack ( x->tail_rirq, fd );
#endif /* ENABLE_MP */
}

void
unpack_devices ( struct mon_t *mon, int fd )
{
	struct devices_t *x = &mon->devs;

	ASSERT ( x != NULL );

	Pic_unpack ( &x->pic, fd );
	Rtc_unpack ( &x->rtc, fd );
	Pit_unpack ( &x->pit, fd );
	Coms_unpack ( x->coms, fd );
	Pci_unpack ( &x->pci, fd );

	HardDrive_unpack ( &x->hard_drive, fd );

	/* [TODO] init miscellenous devices */

#ifdef ENABLE_MP
	x->head_rirq = Bit32u_unpack ( fd );
	x->tail_rirq = Bit32u_unpack ( fd );
#endif /* ENABLE_MP */
}

