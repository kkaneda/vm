#include "vmm/mon/mon.h"

#ifdef ENABLE_MP

static void
handle_msg_apic_logical_id ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_apic_logical_id_t *x = Msg_to_msg_apic_logical_id ( msg );  

	ASSERT ( mon != NULL );
	ASSERT ( mon->local_apic != NULL );
	ASSERT ( x != NULL );

	assert ( ( 0 <= x->src_apic_id ) && ( x->src_apic_id < NUM_OF_PROCS ) );
	mon->local_apic->logical_id_map[x->src_apic_id] = x->logical_id;
}

/****************************************************************/

static void
handle_msg_ipi ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_ipi_t *x = Msg_to_msg_ipi ( msg );

	ASSERT ( mon != NULL );
	ASSERT ( mon->local_apic != NULL );
	ASSERT ( x != NULL );

	mon->wait_ipi = FALSE;
	LocalApic_handle_request_a ( mon->local_apic, &x->ic );
}

/****************************************************************/

static void
handle_msg_mem_image_request ( struct mon_t *mon, struct msg_t *msg )
{
	enum { LEN = 0x10000 };
	int i;
	int src_id = msg->hdr.src_id;

	ASSERT ( mon != NULL );

	for ( i = 0; i < mon->pmem.ram_offset; i += LEN ) {
		size_t n;
		struct msg_t *msg;
	 
		n = ( i + LEN < mon->pmem.ram_offset ) ? LEN : mon->pmem.ram_offset - i;
		msg = Msg_create ( MSG_KIND_MEM_IMAGE_RESPONSE, 
				   n, ( void * ) ( mon->pmem.base + i ) );
		Comm_send ( mon->comm, msg, src_id );
		Msg_destroy ( msg );

		DPRINT2 ( "%#x / %#lx\r", i + n, mon->pmem.ram_offset ); 
	}
	DPRINT2 ( "\n" ); 
}

/****************************************************************/

static void
handle_msg_ioapic_dump ( struct mon_t *mon, struct msg_t *msg )
{
	void *body = msg->body;
	ASSERT ( mon != NULL );
	ASSERT ( body != NULL );

	Mmove ( ( void *)mon->io_apic->ioredtbl, 
		body, 
		sizeof ( struct iored_entry_t ) * NUM_OF_IORED_ENTRIES );
}

/****************************************************************/

static void
send_input_port_ack ( struct mon_t *mon, struct msg_input_port_t *x, bit32u_t val, int src_id )
{
	struct msg_t *msg;
	int irq;

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );

//	Print ( stdout, "[CPU%d] SEND INPUT_ACK to %#x\n", mon->cpuid, src_id );

#if 1
	irq = try_generate_external_irq ( mon, TRUE ); 
#else
	irq = IRQ_INVALID;
#endif
	msg = Msg_create3 ( MSG_KIND_INPUT_PORT_ACK, x->addr, ( int )val, x->len, irq );
	Comm_send ( mon->comm, msg, src_id );
	Msg_destroy ( msg );
}

void
handle_msg_input_port ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_input_port_t *x = Msg_to_msg_input_port ( msg );
	bit32u_t val;

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );

//	Print ( stdout, "[CPU%d] handle INPUT from %d\n", mon->cpuid, msg->hdr.src_id );

	val = inp ( mon, x->addr, x->len );
	send_input_port_ack ( mon, x, val, msg->hdr.src_id );
}

static void
send_output_port_ack ( struct mon_t *mon, struct msg_output_port_t *x, int src_id )
{
	struct msg_t *msg;
	int irq;

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );

//	Print ( stdout, "[CPU%d] SEND OUTPUT_ACK to %#x\n", mon->cpuid, src_id );

#if 1
	irq = try_generate_external_irq ( mon, TRUE ); 
        // [???] irq をそのまま送るのではなく、pic や ioapic が trigger したものを送信する必要がある
#else
	irq = IRQ_INVALID;
#endif
	msg = Msg_create3 ( MSG_KIND_OUTPUT_PORT_ACK, x->addr, x->len, irq );
	Comm_send ( mon->comm, msg, src_id );
	Msg_destroy ( msg );
}

void
handle_msg_output_port ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_output_port_t *x = Msg_to_msg_output_port ( msg );

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );

//	Print ( stdout, "[CPU%d] handle OUTPUT\n", mon->cpuid );

	outp ( mon, x->addr, x->val, x->len );

	send_output_port_ack ( mon, x, msg->hdr.src_id );
}

/****************************************************************/

static void
handle_msg_stat_request ( struct mon_t *mon, struct msg_t *msg )
{
	ASSERT ( mon != NULL );

	start_or_stop_profile ( mon );
	start_time_counter ( &mon->stat.mhandler_counter );
}

/****************************************************************/

static void
handle_msg_stat_request_ack ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_stat_request_ack_t *x = Msg_to_msg_stat_request_ack ( msg );

	ASSERT ( mon != NULL );

	start_or_stop_profile2 ( mon, x->start_flag );	
	start_time_counter ( &mon->stat.mhandler_counter );
}

/****************************************************************/

typedef void mhandler_t ( struct mon_t *mon, struct msg_t *msg);

struct mhandler_entry_t {
	msg_kind_t		kind;
	mhandler_t		*func;
};

static struct mhandler_entry_t mhandler_map[] =
{ { MSG_KIND_APIC_LOGICAL_ID, &handle_msg_apic_logical_id },
  { MSG_KIND_IPI, &handle_msg_ipi },
  { MSG_KIND_MEM_IMAGE_REQUEST, &handle_msg_mem_image_request },
  { MSG_KIND_IOAPIC_DUMP, &handle_msg_ioapic_dump },

  { MSG_KIND_PAGE_FETCH_REQUEST, &handle_fetch_request },
  { MSG_KIND_PAGE_FETCH_ACK, &handle_fetch_ack },
  { MSG_KIND_PAGE_INVALIDATE_REQUEST, &handle_invalidate_request },
  { MSG_KIND_PAGE_FETCH_ACK_ACK, &handle_fetch_ack_ack },  

  { MSG_KIND_INPUT_PORT, &handle_msg_input_port },
  { MSG_KIND_OUTPUT_PORT, &handle_msg_output_port },

  { MSG_KIND_STAT_REQUEST, &handle_msg_stat_request },
  { MSG_KIND_STAT_REQUEST_ACK, &handle_msg_stat_request_ack }
};

static size_t
nr_mhandler_entries ( void )
{
	return sizeof ( mhandler_map ) / sizeof ( struct mhandler_entry_t ); 
}

static mhandler_t *
get_mhandler ( msg_kind_t kind )
{
	int i;
	
	for ( i = 0; i < nr_mhandler_entries ( ); i++ ) {
		struct mhandler_entry_t *x = &mhandler_map[i];
		
		if ( x->kind == kind )
			return x->func;
	}
	
	return NULL;
}

void
handle_msg ( struct mon_t *mon, struct msg_t *msg )
{
	mhandler_t *f;
	
	ASSERT ( mon != NULL );
	ASSERT ( msg != NULL );

	/* [DEBUG] */
//	Print_color ( stderr, YELLOW, "handle_msg: " ); 
//	Msg_print ( stderr, msg );
	
	f = get_mhandler ( msg->hdr.kind );

	if ( f == NULL )
		Match_failure ( "handle_msg: kind=%s\n", MsgKind_to_string ( msg->hdr.kind ) );

	(*f) ( mon, msg );

//	Print_color ( stderr, YELLOW, "handle_msg: end\n" );
}

#else /* !ENABLE_MP */


void
try_handle_msgs ( struct mon_t *mon ) 
{ 
}

void
try_handle_msgs_with_stat ( struct mon_t *mon ) 
{ 
}


#endif /* ENABLE_MP */
