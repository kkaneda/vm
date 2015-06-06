#include "vmm/mon/mon.h"
#include <unistd.h>
#include <asm/apicdef.h>


/* [Reference] 
 *  [1] IA-32 Vol.3 Chapter 8. 
 *  [2] 82099AA I/O ADVANCED PROGRAMMABLE INTERRUPT CONTROLLER  ( IOAPIC ) 
 */
#ifndef APIC_LVTTHMR
#  define		APIC_LVTTHMR	0x330
#endif


enum {
        /* [Note] The base address of real machines is 0xfee00000 */
	VM_LOCAL_APIC_DEFAULT_PHYS_BASE = 0x0ee00000, 

        /* [Note] The base address of real machines is 0xfec00000 */
	VM_IO_APIC_DEFAULT_PHYS_BASE 	= 0x0ec00000, 

	/* [Bochs] same version as 82093 IOAPIC */
	APIC_VERSION_ID 		= 0x00170011
};

/*************************************************************************/

static bit32u_t
shift_aligned_value ( bit32u_t paddr, bit32u_t val )
{
	int start, n;
	int x;

	x = SUB_BIT ( paddr, 0, 2 );
	start = x * 8;
	n = 32 - start;
	return ( ( n == 32 ) ? 
		 val : 
		 SUB_BIT ( val, start, n ) );
}

/*************************************************************************/

static struct generic_apic_t *
GenericApic_create ( bit8u_t id, bit32u_t base_addr, struct comm_t *comm )
{
	struct generic_apic_t *x;

	x = Malloct ( struct generic_apic_t );
	x->id = id;
	x->base_addr = base_addr;
#ifdef ENABLE_MP
	ASSERT ( comm != NULL );
	x->comm = comm;
#endif /* ENABLE_MP */
	return x;     
}

bool_t
GenericApic_is_selected ( const struct generic_apic_t *x, bit32u_t paddr, size_t len )
{
	ASSERT ( x != NULL );
	return ( ( BIT_ALIGN ( paddr, 12 ) ) == x->base_addr );
}


static void
GenericApic_pack ( const struct generic_apic_t *x, int fd )
{
	Bit8u_pack ( x->id, fd );
	Bit32u_pack ( x->base_addr, fd );
}

static void
GenericApic_unpack ( struct generic_apic_t *x, int fd )
{
	
	x->id = Bit8u_unpack ( fd );
	x->base_addr = Bit32u_unpack ( fd );
}

/*************************************************************************/

static void *LocalApic_timer_update_thread ( void *arg );

/* [Reference] IA-32 manual Vol.3 8-12 */
static void
LocalApic_init ( struct local_apic_t *x )
{
	int i;

	ASSERT ( x != NULL );

	x->task_priority = 0;
	x->arb_priority = 0;
	x->logical_id_map[x->gapic->id] = 0;
	x->model = MODEL_CLUSTER; /* The DFR register is reset to all 1s */
	x->spurious_vector = 0xff;
	x->error_status = 0;
	x->interrupt_command[0] = 0;
	x->interrupt_command[1] = 0;
   
	x->INTR = FALSE;

	/* The LVT register entries are reset to all 0s 
	 * except for the mask bits, which are set to 1s. */
	x->local_vector_table.timer = 1 << 16; 
	x->local_vector_table.lint0 = 1 << 16; 
	x->local_vector_table.lint1 = 1 << 16; 
	x->local_vector_table.error = 1 << 16;
	x->local_vector_table.perf_mon_counter = 1 << 16;
	x->local_vector_table.thermal_sensor = 1 << 16;

	for ( i = 0; i < MAX_OF_IVECTOR; i++ ) {
		x->interrupt_request[i] = FALSE;
		x->in_service[i] = FALSE;
		x->trigger_mode[i] = FALSE;
	}

	x->timer.active = FALSE;
	x->timer.initial_count = 0;
	x->timer.current_count = 0;
	x->timer.div_conf = 0;
	x->timer.div_factor = 1;
	rdtsc ( x->timer.ticks_initial );

	x->startup_info.num = 0;
	x->startup_info.vector = 0;
	x->startup_info.need_handle = FALSE;

	x->sleep_time = 0;
}

struct local_apic_t *
LocalApic_create ( int id, struct comm_t *comm, pid_t pid )
{
	int i;
	struct local_apic_t *x;

	x = Malloct ( struct local_apic_t );

	x->gapic = GenericApic_create ( id, VM_LOCAL_APIC_DEFAULT_PHYS_BASE, comm );
	for ( i = 0; i < NUM_OF_PROCS; i++ )
		x->logical_id_map[i] = 0;
	x->pid = pid;
	LocalApic_init ( x );

	{
		pthread_t tid;
		Pthread_mutex_init ( &x->mp, NULL );
		Pthread_cond_init ( &x->cond, NULL );
		Pthread_create ( &tid, NULL, &LocalApic_timer_update_thread, ( void * ) x );
	} 

	return x;
}

/****************************************************************/

void
LocalApic_pack ( struct local_apic_t *x, int fd )
{
	Pthread_mutex_lock ( &x->mp );

	GenericApic_pack ( x->gapic, fd );
	
	Bit8u_pack ( x->task_priority, fd );
	Bit8u_pack ( x->arb_priority, fd );
	Bit8uArray_pack ( x->logical_id_map, NUM_OF_PROCS, fd );
	Bit32u_pack ( ( bit32u_t ) x->model, fd );
	Bit32u_pack ( x->spurious_vector, fd );
	Bit32u_pack ( x->error_status, fd );
	Bit32uArray_pack ( x->interrupt_command, 2, fd );
	Pack ( &x->local_vector_table, sizeof ( struct local_vector_table_t ), fd );
	Pack ( &x->timer, sizeof ( struct timer_t ), fd );
	BoolArray_pack ( x->interrupt_request, MAX_OF_IVECTOR, fd );
	BoolArray_pack ( x->in_service, MAX_OF_IVECTOR, fd );
	BoolArray_pack ( x->trigger_mode, MAX_OF_IVECTOR, fd );
	Bool_pack ( x->INTR, fd );
	Pack ( &x->startup_info, sizeof ( struct startup_info_t ), fd );
	Bit32u_pack ( x->sleep_time, fd );

	Pthread_mutex_unlock ( &x->mp );
}

void 
LocalApic_unpack ( struct local_apic_t *x, int fd )
{
	Pthread_mutex_lock ( &x->mp );

	GenericApic_unpack ( x->gapic, fd );
	
	x->task_priority = Bit8u_unpack ( fd );
	x->arb_priority = Bit8u_unpack ( fd );
	Bit8uArray_unpack ( x->logical_id_map, NUM_OF_PROCS, fd );
	x->model = ( model_t ) Bit32u_unpack ( fd );
	x->spurious_vector = Bit32u_unpack ( fd );
	x->error_status = Bit32u_unpack ( fd );
	Bit32uArray_unpack ( x->interrupt_command, 2, fd );
	Unpack ( &x->local_vector_table, sizeof ( struct local_vector_table_t ), fd );
	Unpack ( &x->timer, sizeof ( struct timer_t ), fd );
	BoolArray_unpack ( x->interrupt_request, MAX_OF_IVECTOR, fd );
	BoolArray_unpack ( x->in_service, MAX_OF_IVECTOR, fd );
	BoolArray_unpack ( x->trigger_mode, MAX_OF_IVECTOR, fd );
	x->INTR = Bool_unpack ( fd );
	Unpack ( &x->startup_info, sizeof ( struct startup_info_t ), fd );
	x->sleep_time = Bit32u_unpack ( fd );

	Pthread_cond_broadcast ( &x->cond );

	Pthread_mutex_unlock ( &x->mp );
}

/****************************************************************/

bool_t
LocalApic_is_selected ( struct local_apic_t *x, bit32u_t paddr, size_t len )
{
	bool_t ret;

	ASSERT ( x != NULL );

	Pthread_mutex_lock ( &x->mp );
	ret = GenericApic_is_selected ( x->gapic, paddr, len );
	Pthread_mutex_unlock ( &x->mp );

	return ret;
}

static int 
get_highest_priority ( bool_t x[] )
{
	int i;
	ASSERT ( x != NULL );

	for ( i = 0; i < MAX_OF_IVECTOR; i++ ) {
		if ( x[i] ) 
			return i;
	}
	return -1;
}

const char *
Model_to_string ( model_t model )
{
	switch ( model ) {
	case MODEL_CLUSTER: return "CLUSTER";
	case MODEL_FLAT:    return "FLAT"; 
	default: 	    Match_failure ( "Model_to_string\n" );
	}
	Match_failure ( "Model_to_string\n" );
	return "";
}

void
LocalApic_print ( FILE *stream, struct local_apic_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );
	ASSERT ( x->gapic != NULL );

	Pthread_mutex_lock ( &x->mp );

	Print ( stream,
	   "{ id=%#x, task_priority=%#x, arb_priority=%#x,\n" 
	   " logical_id_map={ %#x, %#x }, model=%s,\n"
	   " spurious_vector=%#x, error_status=%#x\n"
	   "}\n"
	   ,
	   x->gapic->id,
	   x->task_priority,
	   x->arb_priority,
	   x->logical_id_map[0], x->logical_id_map[1], 
	   Model_to_string ( x->model ),
	   x->spurious_vector, x->error_status );

	Pthread_mutex_unlock ( &x->mp );
}

/****************************************************************/

static bit32u_t
LocalApic_read_aligned ( struct local_apic_t *apic, bit32u_t paddr, size_t len )
{
	bit32u_t retval = 0;
	bit32u_t p;

	ASSERT ( apic != NULL );
	ASSERT ( len == 4 );

	p = SUB_BIT ( paddr, 0, 12 );  
	p = BIT_ALIGN ( p, 4 );

	DPRINT ( "LocalApic_read_aligned: addr=%#x\n", p );

	switch ( p ) {
	case APIC_ID:
		retval = apic->gapic->id << 24;
		break;

	case APIC_LVR:
		retval = APIC_VERSION_ID;
		break;

	case APIC_TASKPRI:
		retval = apic->task_priority;
		break;

	case APIC_ARBPRI:
		retval = apic->arb_priority;
		break;

	case APIC_PROCPRI:
		Fatal_failure ( "Apic_read_aligned: APIC_PROCPIR is not implemented\n" );
		break;

	case APIC_EOI:
		break;

	case APIC_LDR:
		retval = SET_APIC_LOGICAL_ID ( apic->logical_id_map[apic->gapic->id] );
		break;

	case APIC_DFR:
		retval = BIT_MASK ( 28 ) | ( apic->model << 28 );
		break;

	case APIC_SPIV:
		retval = apic->spurious_vector;
		break;

	case 0x100: case 0x110: case 0x120: case 0x130:
	case 0x140: case 0x150: case 0x160: case 0x170:
	case 0x180: case 0x190: case 0x1a0: case 0x1b0:
	case 0x1c0: case 0x1d0: case 0x1e0: case 0x1f0:
	case 0x200: case 0x210: case 0x220: case 0x230:
	case 0x240: case 0x250: case 0x260: case 0x270:
		retval = 0;
		break;

	case APIC_ESR:
		retval = apic->error_status;
		break;

	case APIC_ICR:
		retval = apic->interrupt_command[0];
		break;

	case APIC_ICR2:
		retval = apic->interrupt_command[1];
		break;

	case APIC_LVTT:
		retval = apic->local_vector_table.timer;
		break;

	case APIC_LVTTHMR:
		retval = apic->local_vector_table.thermal_sensor;
		break;

	case APIC_LVTPC:
		retval = apic->local_vector_table.perf_mon_counter;
		break;

	case APIC_LVT0:
		retval = apic->local_vector_table.lint0;
		break;

	case APIC_LVT1:
		retval = apic->local_vector_table.lint1;
		break;

	case APIC_LVTERR:
		retval = apic->local_vector_table.error;
		break;

	case APIC_TMICT:
		retval = apic->timer.initial_count;
		break;
	case APIC_TMCCT:
		if ( ! apic->timer.active ) {
			retval = apic->timer.current_count;
		} else {
			bit32u_t d;
			bit64u_t current;

			rdtsc ( current );
			d = ( bit32u_t )( ( current - apic->timer.ticks_initial ) / apic->timer.div_factor);
			assert ( d <= apic->timer.initial_count );
			apic->timer.current_count = apic->timer.initial_count - d;

			retval = apic->timer.current_count;
		}
		break;

	case APIC_TDCR:
		retval = apic->timer.div_conf;
		break;
	default:
		Match_failure ( "LocalApic_read_aligned\n" );
	}
	return retval;
}

bit32u_t
LocalApic_read ( struct local_apic_t *apic, bit32u_t paddr, size_t len )
{
	bit32u_t val;

//	Print_color ( stdout, CYAN, "LocalApic_read: paddr=%#x\n", paddr );

	ASSERT ( apic != NULL );
	ASSERT ( BIT_ALIGN ( paddr, 4 ) == BIT_ALIGN ( ( paddr + len - 1 ), 4 ) );

	Pthread_mutex_lock ( &apic->mp );

	val = LocalApic_read_aligned ( apic, paddr, 4 );
	val = shift_aligned_value ( paddr, val );
	
	DPRINT ( "LocalApic_read: paddr=%#lx, val=%#lx\n", paddr, val );

	Pthread_mutex_unlock ( &apic->mp );

	return val;
}

/****************************************************************/

static void
LocalApic_serve ( struct local_apic_t *apic )
{
	int irr, isr;

	ASSERT ( apic != NULL );

	if ( apic->INTR ) 
		return;

	irr = get_highest_priority ( apic->interrupt_request );
	isr = get_highest_priority ( apic->in_service );
	
//	Print_color ( stdout, BLUE, "serve: irr = %#x, isr = %#x\n", irr, isr );
	DPRINT ( "serve: irr = %#x, isr = %#x\n", irr, isr );

	if  ( irr < 0 )
		return;
	
	if ( ( isr >= 0 ) && ( isr <= irr ) )
		return;

	apic->INTR = TRUE;
}

#ifdef ENABLE_MP

static void
LocalApic_notify_logical_id ( struct local_apic_t *apic ) {
	struct msg_t *msg;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );

	msg = Msg_create3 ( MSG_KIND_APIC_LOGICAL_ID, 
			  ( int )apic->gapic->id, 
			  ( int )apic->logical_id_map[apic->gapic->id] );
	Comm_bcast ( apic->gapic->comm, msg );
	Msg_destroy ( msg );
}

#else /* !ENABLE_MP */

static void
LocalApic_notify_logical_id ( struct local_apic_t *apic ) 
{
}

#endif /* ENABLE_MP */


static bit8u_t
get_ipi_dest_with_physical_mode ( bit8u_t dest )
{
	const bit8u_t BCAST_MASK = 0xff;
	return ( dest == BCAST_MASK ) ? BCAST_MASK : ( 1 << dest );
}

static bool_t
match_logical_dest ( struct local_apic_t *apic, bit8u_t id, bit8u_t dest )
{
	bool_t retval = 0;

	ASSERT ( apic != NULL );

	switch ( apic->model ) {
	case MODEL_CLUSTER:
		Fatal_failure ( "match_logical_dest: Not implemented\n" );
		break;

	case MODEL_FLAT:
		// ????
		retval = ( ( dest == 0xff )  ||
			   ( ( dest & apic->logical_id_map[id] ) != 0 ) );  // ???
		break;
	default:
		Match_failure ( "match_logical_dest: model=%#x (%p)\n",
				apic->model, apic );
	}
	return retval;
}

static int
get_ipi_dest_with_logical_mode ( struct local_apic_t *apic, bit8u_t dest )
{
	int retval = 0;
	int i;

	ASSERT ( apic != NULL );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( match_logical_dest ( apic, i, dest ) ) 
			retval |= ( 1 << i );
	}
	return retval;
}

static int
get_ipi_dest_with_no_shorthand ( struct local_apic_t *apic, struct interrupt_command_t *ic )
{
	int retval = 0;

	ASSERT ( apic != NULL );
	ASSERT ( ic != NULL );

	switch ( ic->dest_mode ) {
	case DEST_MODE_PHYSICAL: retval = get_ipi_dest_with_physical_mode ( ic->dest ); break;
	case DEST_MODE_LOGICAL:  retval = get_ipi_dest_with_logical_mode ( apic, ic->dest ); break;
	default: 		 Match_failure ( "get_ipi_dest_with_no_shorthand" );
	}
	return retval;
}

/* [TODO] */
static int 
get_lowest_priority_cpuid ( void )
{
	return BSP_CPUID; 
}

static int
modify_dest_with_delivery_mode ( struct local_apic_t *apic, struct interrupt_command_t *ic, int ipi_dest )
{
	int retval = ipi_dest;

	ASSERT ( apic != NULL );
	ASSERT ( ic != NULL );

	switch ( ic->delivery_mode ) {
	case DELIVERY_MODE_LOWEST_PRIORITY: {
		retval = 1 << get_lowest_priority_cpuid ( );
		break;
	}
	case DELIVERY_MODE_INIT: 
		if ( ( ic->level == LEVEL_DEASSERT ) && ( ic->trig_mode == TRIG_MODE_LEVEL ) ) { 
			/* INIT DEASSERTED */
			retval = BIT_MASK ( NUM_OF_PROCS );
			// 自分を含めるか，そうでないか
		}
		break;
	default: 
		/* do nothing */ 
		break;
	}
	return retval;
}

/* [Reference] IA-32 manual. Vol.3 8-27 */
static int
LocalApic_get_ipi_dest ( struct local_apic_t *apic, struct interrupt_command_t *ic )
{
	const int ALL_MASK = BIT_MASK ( NUM_OF_PROCS );
	int retval = 0;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( ic != NULL );

	switch ( ic->dest_shorthand ) {
	case DEST_SHORTHAND_NO:
		retval = get_ipi_dest_with_no_shorthand ( apic, ic );
		break;

	case DEST_SHORTHAND_SELF:
		retval = ( 1 << apic->gapic->id );
		break;

	case DEST_SHORTHAND_ALL_INCLUDING_SELF:
		retval = ALL_MASK; 
		break;

	case DEST_SHORTHAND_ALL_EXCLUDING_SELF:
		retval = ALL_MASK; 
		CLEAR_BIT ( retval, apic->gapic->id );
		break;

	default:
                Match_failure ( "LocalApic_get_ipi_dest" );
	}
   
	retval = modify_dest_with_delivery_mode ( apic, ic, retval );

	return retval;
}

static void
LocalApic_handle_init_request ( struct local_apic_t *apic, struct interrupt_command_t *ic )
{
	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( ic != NULL );

	DPRINT ( "Init\n" );

	if ( ( ic->level == LEVEL_DEASSERT ) && ( ic->trig_mode == TRIG_MODE_LEVEL ) ) { 
		apic->arb_priority = apic->gapic->id;
	} else {
		LocalApic_init ( apic );
	}
}

/* [Reference] IA-32 manual Vol.3 Section 7.5 */
static void
LocalApic_handle_startup_request ( struct local_apic_t *apic, struct interrupt_command_t *ic )
{
	ASSERT ( apic != NULL );
	ASSERT ( ic != NULL );

	DPRINT ( "Startup\n" );

	apic->startup_info.num++;
	apic->startup_info.vector = ic->vector;
	apic->startup_info.need_handle = TRUE;
}

static void
LocalApic_handle_request ( struct local_apic_t *apic, 
			   struct interrupt_command_t *ic )
{
	ASSERT ( apic != NULL );
	ASSERT ( ic != NULL );

	DPRINT ( "LocalApic_handle_request\n" );
	INTERRUPT_COMMAND_DPRINT ( ic );

	switch ( ic->delivery_mode ) {
	case DELIVERY_MODE_FIXED:
	case DELIVERY_MODE_LOWEST_PRIORITY:
	case DELIVERY_MODE_EXTINT:
		/* [Reference] IA-32 manual Vol.3 8-36 */

		apic->interrupt_request[ic->vector] = TRUE;
		LocalApic_serve ( apic );
		break;

	case DELIVERY_MODE_INIT:
		LocalApic_handle_init_request ( apic, ic );
		break;

	case DELIVERY_MODE_STARTUP:
		LocalApic_handle_startup_request ( apic, ic ); 
		break;

	case DELIVERY_MODE_SMI:
	case DELIVERY_MODE_NMI:
		Fatal_failure ( "LocalApic_handle_request: Not implemented: %d",
				ic->delivery_mode );

	default:
		Match_failure ( "LocalApic_handle_request" );
	}
}

void
LocalApic_handle_request_a ( struct local_apic_t *apic, 
			   struct interrupt_command_t *ic )
{
	Pthread_mutex_lock ( &apic->mp );
	LocalApic_handle_request ( apic, ic );
	Pthread_mutex_unlock ( &apic->mp );
}

#ifdef ENABLE_MP

static void
LocalApic_deliver_to_remote ( struct local_apic_t *apic, struct interrupt_command_t *ic, bit8u_t dest_id )
{
	struct msg_t *msg;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( ic != NULL );

	DPRINT ( "APIC_DELIVER: %d --> %d: delivery_mode=%s, vector=%#x\n", 
		 apic->gapic->id, 
		 dest_id, 
		 DeliveryMode_to_string ( ic->delivery_mode ), 
		 ic->vector );
/*
	Print_color ( stdout, GREEN,
		"APIC_DELIVER: %d --> %d: delivery_mode=%s, vector=%#x\n", 
		 apic->gapic->id, 
		 dest_id, 
		 DeliveryMode_to_string ( ic->delivery_mode ), 
		 ic->vector );
*/

	msg = Msg_create3 ( MSG_KIND_IPI, ( void * )ic );
	Comm_send ( apic->gapic->comm, msg, dest_id );
	Msg_destroy ( msg );
}

#else /* !ENABLE_MP */

static void
LocalApic_deliver_to_remote ( struct local_apic_t *apic, struct interrupt_command_t *ic, bit8u_t dest_id )
{
}

#endif /* ENABLE_MP */

static void
LocalApic_deliver_sub ( struct local_apic_t *apic, struct interrupt_command_t *ic )
{
	int bitmask;
	int i;

	ASSERT ( apic != NULL );
	ASSERT ( ic != NULL );   

	bitmask = LocalApic_get_ipi_dest ( apic, ic );

	if ( bitmask == 0 ) {
		LocalApic_print ( stderr, apic );
		Fatal_failure ( "LocalApic_deliver_sub: failed\n" );
		apic->error_status |= ERROR_SEND_ACCEPT; 
		return; 
	}

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( ! TEST_BIT ( bitmask, i ) ) 
			continue;
	    
		if ( i == apic->gapic->id ) { 
			LocalApic_handle_request ( apic, ic );
		} else {
			LocalApic_deliver_to_remote ( apic, ic, i );
		} 
	}
}

static void
LocalApic_deliver ( struct local_apic_t *apic )
{
	struct interrupt_command_t ic;
  
	DPRINT ( "Apic_deliver\n" );

	ASSERT ( apic != NULL );

	ic = InterruptCommand_of_bit64u ( apic->interrupt_command );

	INTERRUPT_COMMAND_DPRINT ( &ic );
   
	LocalApic_deliver_sub ( apic, &ic );
}

void
LocalApic_write ( struct local_apic_t *apic, bit32u_t paddr, bit32u_t val, size_t len )
{
	bit32u_t p;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( len == 4 );

	p = SUB_BIT ( paddr, 0, 12 );  
	p = BIT_ALIGN ( p, 4 );

//	Print_color ( stdout, CYAN, "LocalApic_write: addr=%#x, val=%#x\n", p, val );

	Pthread_mutex_lock ( &apic->mp );

	DPRINT ( "LocalApic_write: addr=%#x, val=%#x\n", p, val );
	//DPRINT2 ( "LocalApic_write: addr=%#x, val=%#x\n", p, val );

	switch ( p ) {
	case APIC_ID:
		apic->gapic->id = SUB_BIT ( val, 24, 8 );
		break;

	case APIC_LVR:
		break;

	case APIC_TASKPRI:
		apic->task_priority = SUB_BIT ( val, 0, 8 );
		break;

	case APIC_ARBPRI:
		break;

	case APIC_PROCPRI:
		break;

	case APIC_EOI: /* end-of-interrupt */
	{	 /* [Reference] IA-32 manual. Vol 3. 8-38 
		   Upon the receiving and EOI, the APIC clears the highest priority bit in the ISR 
		   and dispatches the next highest priority interrupt to the processor. */
		int vec;
		vec = get_highest_priority ( apic->in_service );
		if ( vec >= 0 ) {
			apic->in_service[vec] = FALSE;
		}
		LocalApic_serve ( apic ); 
		break;
	}

	case APIC_LDR:
		apic->logical_id_map[apic->gapic->id] = GET_APIC_LOGICAL_ID ( val );
		LocalApic_notify_logical_id ( apic );
		break;

	case APIC_DFR:
		apic->model = SUB_BIT ( val, 28, 4 );
		break;

	case APIC_SPIV:
		/* ??? */
		apic->spurious_vector = (( SUB_BIT ( apic->spurious_vector, 0, 4 ) | 
					   LSHIFTED_SUB_BIT ( val, 4, 6, 4 ) ) );
		break;

	case 0x100: case 0x110: case 0x120: case 0x130:
	case 0x140: case 0x150: case 0x160: case 0x170:
	case 0x180: case 0x190: case 0x1a0: case 0x1b0:
	case 0x1c0: case 0x1d0: case 0x1e0: case 0x1f0:
	case 0x200: case 0x210: case 0x220: case 0x230:
	case 0x240: case 0x250: case 0x260: case 0x270:
		break;

	case APIC_ESR:
		/* ??? */ 
		apic->error_status = 0; 
		break;

	case APIC_ICR:
		apic->interrupt_command[0] = val;
		CLEAR_BIT ( apic->interrupt_command[0], 12 ); /* force delivery status bit = 0 ( become idle ) */
		LocalApic_deliver ( apic );
		break;

	case APIC_ICR2:
		apic->interrupt_command[1] = BIT_ALIGN ( val, 24 ); 
		break;

	case APIC_LVTT:
		apic->local_vector_table.timer = ( ( SUB_BIT ( val, 0, 8 ) | 
						     LSHIFTED_SUB_BIT ( val, 12, 1, 12 ) | 
						     LSHIFTED_SUB_BIT ( val, 16, 2, 16 ) ) );
		break;

	case APIC_LVTTHMR:
		apic->local_vector_table.thermal_sensor = ( ( SUB_BIT ( val, 0, 10 ) | 
							      LSHIFTED_SUB_BIT ( val, 12, 1, 12 ) | 
							      LSHIFTED_SUB_BIT ( val, 16, 1, 16 ) ) );
		break;

	case APIC_LVTPC:
		apic->local_vector_table.perf_mon_counter = ( ( SUB_BIT ( val, 0, 10 ) | 
								LSHIFTED_SUB_BIT ( val, 12, 1, 12 ) | 
								LSHIFTED_SUB_BIT ( val, 16, 1, 16 ) ) );
		break;

	case APIC_LVT0:
		apic->local_vector_table.lint0 = ( ( SUB_BIT ( val, 0, 10 ) | 
						     LSHIFTED_SUB_BIT ( val, 12, 5, 12 ) ) );
		break;

	case APIC_LVT1:
		apic->local_vector_table.lint1 = ( ( SUB_BIT ( val, 0, 10 ) | 
						     LSHIFTED_SUB_BIT ( val, 12, 5, 12 ) ) );
		break;

	case APIC_LVTERR:
		apic->local_vector_table.error = ( ( SUB_BIT ( val, 0, 10 ) | 
						     LSHIFTED_SUB_BIT ( val, 12, 1, 12 ) | 
						     LSHIFTED_SUB_BIT ( val, 16, 1, 16 ) ) );
		break;

	case APIC_TMICT: /* 0x380 */
		apic->timer.initial_count = val;
		apic->timer.current_count = apic->timer.initial_count;
		apic->timer.active = TRUE;
		rdtsc ( apic->timer.ticks_initial );

#if 0
		bool_t cont = ( apic->local_vector_table.timer & 0x20000 ) > 0;
		bx_pc_system.activate_timer_ticks(timer_handle,
						  Bit64u(apic->timer.initial_count) * Bit64u(apic->timer.div_factor), 
						  cont );
#endif
		struct itimerval itv; 
		bit64u_t d = ((bit64u_t)apic->timer.initial_count) * ((bit64u_t)apic->timer.div_factor);
		itv.it_interval.tv_sec = d / ticks_per_sec;
		itv.it_interval.tv_usec = muldiv64 ( d, 1000000, ticks_per_sec );
		itv.it_value = itv.it_interval;

		assert ( ( itv.it_interval.tv_sec != 0 ) || ( itv.it_interval.tv_usec != 0 ) );
		apic->sleep_time = ( itv.it_interval.tv_sec * 1000000 + itv.it_interval.tv_usec);
//		Print_color ( stdout, CYAN, "apic->sleep_time = %d\n",			      apic->sleep_time );

		Pthread_cond_broadcast ( &apic->cond );

		break;

	case APIC_TMCCT: /* 0x390 */
		Fatal_failure ( "LocalApic_write: TMCCT has not yet been implemented\n" );
		break;

        case APIC_TDCR: /* 0x3E0 */

		// only bits 3, 1, and 0 are writable
		apic->timer.div_conf = SUB_BIT ( val, 0, 4 );
		CLEAR_BIT ( apic->timer.div_conf, 2 );

		{ /* set divide configuration */
			bit32u_t val;
	
			val = apic->timer.div_conf;
			val = ( ( val & 8 ) >> 1 ) | ( val & 3 );
			assert ( ( 0 <= val ) && ( val <= 7 ) );
			apic->timer.div_factor = ( val == 7 ) ? 1 : ( 2 << val );
		}

		break;

	default:		
		Match_failure ( "LocalApic_write\n" );
	}

	Pthread_mutex_unlock ( &apic->mp );
}

/****************************************************************/

static int
LocalApic_try_acknowledge_interrupt_sub ( struct local_apic_t *apic )
{
	int vec;

	if ( ! apic->INTR ) {
		return -1;
	}

	vec = get_highest_priority ( apic->interrupt_request );
	if ( vec < 0 ) {
		return -1;
	}

	apic->interrupt_request[vec] = FALSE;
	apic->in_service[vec] = TRUE;

//	Print_color ( stdout, BLUE, " INTR = FALSE: vec = %#x\n", vec ); // [DEBUG]		

	apic->INTR = FALSE;

	LocalApic_serve ( apic ); 

	return vec;
}

int
LocalApic_try_acknowledge_interrupt ( struct local_apic_t *apic )
{
	int vec;

	ASSERT ( apic != NULL );

	Pthread_mutex_lock ( &apic->mp );
	vec = LocalApic_try_acknowledge_interrupt_sub ( apic );
	Pthread_mutex_unlock ( &apic->mp );

	return vec;
}

/****************************************************************/

static bool_t
LocalApic_check_interrupt_sub ( struct local_apic_t *apic )
{
	int vec;

	if ( ! apic->INTR ) {
		return FALSE;
	}

	vec = get_highest_priority ( apic->interrupt_request );
	return ( vec >= 0 );
}

bool_t
LocalApic_check_interrupt ( struct local_apic_t *apic )
{
	bool_t ret;

	ASSERT ( apic != NULL );

	Pthread_mutex_lock ( &apic->mp );
	ret = LocalApic_check_interrupt_sub ( apic );
	Pthread_mutex_unlock ( &apic->mp );

	return ret;
}

/****************************************************************/

static void
LocalApic_timer_update ( struct local_apic_t *apic )
{
	bit32u_t v;
	bit64u_t current;
	bit32u_t d;

	/* [???] */
	rdtsc ( current );
	d = ( bit32u_t )( ( current - apic->timer.ticks_initial ) / apic->timer.div_factor);
	if ( d < apic->timer.initial_count ) {
		return;
	}
	
	v = apic->local_vector_table.timer;

	// If timer is not masked, trigger interrupt.
	if ( ! TEST_BIT ( v, 16 ) ) {
		apic->interrupt_request[ SUB_BIT ( v, 0, 8 ) ] = TRUE;
		LocalApic_serve ( apic );

		/* [TODO] */
//		if ( ( is_native_mode ( mon ) ) && ( Pit_check_irq ( pit ) ) ) {
		Kill ( apic->pid, SIGALRM );
	}
	
	if ( TEST_BIT ( v, 17 ) ) {
		/* Periodic mode */

		apic->timer.current_count = apic->timer.initial_count;
		rdtsc ( apic->timer.ticks_initial );
	} else {
		/* one-shot mode */

		apic->timer.current_count = 0;
		apic->timer.active = FALSE;
#if 0
		bx_pc_system.deactivate_timer(timer_handle); // Make sure.
#endif		
	}
}

static void *
LocalApic_timer_update_thread ( void *arg )
{
	struct local_apic_t *apic = ( struct local_apic_t *) arg;

	while ( TRUE ) {
		Pthread_mutex_lock ( &apic->mp );
		while ( ! apic->timer.active ) {
			Pthread_cond_wait ( &apic->cond, &apic->mp );
		}
		LocalApic_timer_update ( apic );
		Pthread_mutex_unlock ( &apic->mp );

		if ( apic->sleep_time > 0 ) {
#if 0
			usleep ( apic->sleep_time );
#else
			usleep ( apic->sleep_time * 5.0 ); // [DEBUG] 
#endif
		}
		/* [TODO] periodic update でない場合の扱い */
	}

	return NULL;
}

/*************************************************************************/

struct iored_entry_t
IoredEntry_of_bit64u ( const bit32u_t vals[2] )
{
	struct iored_entry_t x;
   
	x.vals[0] = vals[0];
	x.vals[1] = vals[1];

	x.vector          = SUB_BIT ( vals[0], 0, 8 );
	x.delivery_mode   = SUB_BIT ( vals[0], 8, 3 );
	x.dest_mode       = SUB_BIT ( vals[0], 11, 1 );
	x.delivery_status = SUB_BIT ( vals[0], 12, 1 );
	x.polarity        = SUB_BIT ( vals[0], 13, 1 );
	x.remote_irr      = SUB_BIT ( vals[0], 14, 1 );
	x.trig_mode       = SUB_BIT ( vals[0], 15, 1 );
	x.interrupt_mask  = SUB_BIT ( vals[0], 16, 1 );
	x.dest            = SUB_BIT ( vals[1], 24, 8 );

	return x;
}

struct interrupt_command_t
IoredEntry_to_interrupt_command ( const struct iored_entry_t *x )
{
	struct interrupt_command_t ic;

	ASSERT ( x != NULL );

	ic.vector         = x->vector;
	ic.delivery_mode  = x->delivery_mode;
	ic.dest_mode      = x->dest_mode;
	ic.level          = LEVEL_ASSERT; /* ??? */
	ic.trig_mode      = x->trig_mode;
	ic.dest_shorthand = DEST_SHORTHAND_NO; /* ??? */
	ic.dest           = x->dest;

	/* [TODO] remote_irr ( p. 12 ) */

	return ic;
}

struct io_apic_t *
IoApic_create ( int id, struct comm_t *comm, struct local_apic_t *local_apic )
{
	struct io_apic_t *x;
	int i;

	ASSERT ( local_apic != NULL );

	x = Malloct ( struct io_apic_t );
	x->gapic = GenericApic_create ( id, VM_IO_APIC_DEFAULT_PHYS_BASE, comm );
	x->local_apic = local_apic;

	x->ioregsel = 0;
	for ( i = 0; i < NUM_OF_IORED_ENTRIES; i++ ) {
		/* [???] default value described in the manual is xxx1 xxxx xxx xxxxh (pp.11) */
		const bit32u_t vals[2] = { 0x00010000, 0x00000000 };

		x->ioredtbl[i] = IoredEntry_of_bit64u ( vals );
		x->interrupt_request[i] = FALSE;
	}

	return x;
}

/****************************************************************/

void
IoApic_pack ( struct io_apic_t *x, int fd )
{
	GenericApic_pack ( x->gapic, fd );
	
	Bit8u_pack ( x->ioregsel, fd );
	Pack ( ( void *) x->ioredtbl, sizeof ( struct iored_entry_t ) * NUM_OF_IORED_ENTRIES, fd );
	BoolArray_pack ( x->interrupt_request, 32, fd );
}

void 
IoApic_unpack ( struct io_apic_t *x, int fd )
{
	GenericApic_unpack ( x->gapic, fd );
	
	x->ioregsel = Bit8u_unpack ( fd );
	Unpack ( ( void *) x->ioredtbl, sizeof ( struct iored_entry_t ) * NUM_OF_IORED_ENTRIES, fd );
	BoolArray_unpack ( x->interrupt_request, 32, fd );
}

/****************************************************************/

bool_t
IoApic_is_selected ( const struct io_apic_t *x, bit32u_t paddr, size_t len )
{
	ASSERT ( x != NULL );
	return GenericApic_is_selected ( x->gapic, paddr, len );
}

static void
IoApic_service ( struct io_apic_t *apic )
{
	int i;

	ASSERT ( apic != NULL );
   
	for ( i = 0; i < NUM_OF_IORED_ENTRIES; i++ ) {
		struct iored_entry_t *x = &apic->ioredtbl[i];
		struct interrupt_command_t ic;

		if ( ( ! apic->interrupt_request[i] ) || ( x->interrupt_mask ) ) 
			continue;
	 
		ic = IoredEntry_to_interrupt_command ( x );
		LocalApic_deliver_sub ( apic->local_apic, &ic );

//		Print_color ( stdout, BLUE, "irq[%#x]= FALSE \n", i );
		apic->interrupt_request[i] = FALSE; 
	}
}

void
IoApic_trigger ( struct io_apic_t *apic, int irq )
{
	ASSERT ( apic != NULL );
	ASSERT ( ( irq >= 0 ) && ( irq < NUM_OF_IORED_ENTRIES ) );
   
	if ( ! apic->interrupt_request[irq] ) {
		apic->interrupt_request[irq] = TRUE;
//		Print_color ( stdout, BLUE, "irq[%#x]= TRUE \n", irq );
		IoApic_service ( apic );
	}
}

/****************************************************************/

static bit32u_t
IoApic_read_id_reg ( const struct io_apic_t *apic )
{
	ASSERT ( apic != NULL );
	return apic->gapic->id << 24;
}

static bit32u_t
IoApic_read_version_reg ( const struct io_apic_t *apic )
{
	bit32u_t entries = NUM_OF_IORED_ENTRIES - 1;
	ASSERT ( apic != NULL );

	return ( ( SUB_BIT ( APIC_VERSION_ID, 0, 8 ) ) |
		 ( LSHIFTED_SUB_BIT ( entries, 0, 8, 16 ) ) ); 
}

/* [TODO] */
static bit32u_t
IoApic_read_arb_reg ( const struct io_apic_t *apic )
{
	return 0;
}

static bit32u_t
IoApic_read_ioredtbl ( struct io_apic_t *apic )
{
	int i;
	struct iored_entry_t *x;

	ASSERT ( apic != NULL );

	i = ( apic->ioregsel - 0x10 ) >> 1;
	ASSERT ( ( i >= 0 ) && ( i < NUM_OF_IORED_ENTRIES ) );

	x = &apic->ioredtbl[i];
	return ( apic->ioregsel % 2 == 1 ) ? x->vals[1] : x->vals[0];
}

static bit32u_t
IoApic_read_iowin ( struct io_apic_t *apic, size_t len )
{
	bit32u_t retval = 0;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( len == 4 );
   
	switch ( apic->ioregsel ) {
	case 0x00: retval = IoApic_read_id_reg ( apic ); 	break;
	case 0x01: retval = IoApic_read_version_reg ( apic );	break;
	case 0x02: retval = IoApic_read_arb_reg ( apic ); 	break; 
	default:   retval = IoApic_read_ioredtbl ( apic );
	}

	//DPRINT2 ( "read from iowin: regsel=%#x, val=%#x\n", apic->ioregsel, retval );

	return retval;
}

static bit32u_t
IoApic_read_aligned ( struct io_apic_t *apic, bit32u_t paddr, size_t len )
{
	bit32u_t retval = 0;
	bit32u_t p;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( len == 4 );

	p = SUB_BIT ( paddr, 0, 8 );
	p = BIT_ALIGN ( p, 4 );

	DPRINT ( "IoApic_read_aligned: addr=%#x\n", p );

	switch ( p ) {
	case 0x00: retval = apic->ioregsel; 			break;
	case 0x10: retval = IoApic_read_iowin ( apic, len ); 	break;
	default:   Match_failure ( "IoApic_read_aligned\n" );
	}

	return retval;
}

bit32u_t
IoApic_read ( struct io_apic_t *apic, bit32u_t paddr, size_t len )
{
	bit32u_t val;

	ASSERT ( apic != NULL );
	ASSERT ( BIT_ALIGN ( paddr, 4 ) == BIT_ALIGN ( ( paddr + len - 1 ), 4 ) );

	val = IoApic_read_aligned ( apic, paddr, 4 );
	val = shift_aligned_value ( paddr, val );

	DPRINT ( "IoApic_read: paddr=%#lx, val=%#lx\n", paddr, val );
	//DPRINT2 ( "IoApic_read: paddr=%#lx, val=%#lx\n", paddr, val );

	return val;
}

/****************************************************************/

static void
IoApic_write_ioregsel ( struct io_apic_t *apic, bit32u_t val )
{
	ASSERT ( apic != NULL );

	apic->ioregsel = SUB_BIT ( val, 0, 8 ); 
	//DPRINT2 ( "set ioregsel to %#x\n", apic->ioregsel );
}

static void
IoApic_write_id_reg ( struct io_apic_t *apic, bit32u_t val )
{
	ASSERT ( apic != NULL );
	apic->gapic->id = SUB_BIT ( val, 24, 4 );

	/* [TODO] 他のCPUへ通知する */
}

#ifdef ENABLE_MP 

static void
update_remote_ioredtable ( struct io_apic_t *apic )
{
	struct msg_t *msg;
	void *body;
	size_t n;
	
	ASSERT ( apic != NULL );

	body = ( void * ) ( apic->ioredtbl );
	n = sizeof ( struct iored_entry_t ) * NUM_OF_IORED_ENTRIES;
	msg = Msg_create ( MSG_KIND_IOAPIC_DUMP, n, body );

	Comm_bcast ( apic->gapic->comm, msg );
	Msg_destroy ( msg );	
}

#else /* ! ENABLE_MP */

static void
update_remote_ioredtable ( struct io_apic_t *apic )
{
}

#endif /* ENABLE_MP */

static void
IoApic_write_ioredtbl ( struct io_apic_t *apic, bit32u_t val )
{
	int i;
	struct iored_entry_t *x;
	bit32u_t vals[2];

	ASSERT ( apic != NULL );
   
	i = ( apic->ioregsel - 0x10 ) >> 1;
	ASSERT ( i < NUM_OF_IORED_ENTRIES );
   
	x = &apic->ioredtbl[i];
   
	if ( apic->ioregsel % 2 == 1 ) {
		vals[0] = x->vals[0];
		vals[1] = val;
	} else {
		vals[0] = val;
		vals[1] = x->vals[1];
	}
	apic->ioredtbl[i] = IoredEntry_of_bit64u ( vals );

//	DPRINT ( "ioredtbl[%#x].vector = %#x\n", i, x->vector );

	update_remote_ioredtable ( apic );

	IoApic_service ( apic );
}

static void
IoApic_write_iowin ( struct io_apic_t *apic, bit32u_t val, size_t len )
{
	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );
	ASSERT ( len == 4 );

	//DPRINT2 ( "write to iowin\n" );

	switch ( apic->ioregsel ) {
	case 0x00: IoApic_write_id_reg ( apic, val ); break;
	case 0x01: 
	case 0x02: /* [Note] version and arb are read only */ ASSERT ( 0 ); break;
	default:   IoApic_write_ioredtbl ( apic, val );
	}
}

void
IoApic_write ( struct io_apic_t *apic, bit32u_t paddr, bit32u_t val, size_t len )
{
	bit32u_t p;

	ASSERT ( apic != NULL );
	ASSERT ( apic->gapic != NULL );

	p = SUB_BIT ( paddr, 0, 8 );  
	p = BIT_ALIGN ( p, 4 );

	DPRINT ( "IoApic_write: paddr=%#x, val=%#x\n", p, val );
	//DPRINT2 ( "IoApic_write: addr=%#x, val=%#x\n", p, val );

	switch ( p ) {
	case 0x00: IoApic_write_ioregsel ( apic, val );  break;
	case 0x10: IoApic_write_iowin ( apic, val, len ); break;
	default:  Match_failure ( "IoApic_write\n" );
	}
}
