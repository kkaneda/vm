#include "vmm/mon/mon.h"

#ifdef ENABLE_MP

struct shm_arg_t {
	mem_access_kind_t 	kind;
	int 			page_no;
	struct page_descr_t 	*pdescr;
};

#if 1
inline void DP ( FILE *stream, const char * fmt, ... )  { }
inline void DP_color ( FILE *stream, color_t color, const char * fmt, ... ) { }    
#else

inline void
DP ( FILE *stream, const char *fmt, ... )
{
	va_list ap;

	va_start ( ap, fmt ); 
	Printv ( stream, fmt, ap );
	va_end ( ap );
}

inline void
DP_color ( FILE *stream, color_t color, const char *fmt, ... )
{
	va_list ap;
	
	va_start ( ap, fmt ); 
	Printv_color ( stream, color, fmt, ap );
	va_end ( ap );
}
#endif

static bool_t
page_is_accesible ( const struct page_descr_t *pdescr, mem_access_kind_t kind )
{
	bool_t ret = TRUE;

	ASSERT ( pdescr != NULL );
     
	switch ( kind ) {
	case MEM_ACCESS_READ:
		ret = ( pdescr->state != PAGE_STATE_INVALID );
		break;
	case MEM_ACCESS_WRITE:
		ret = ( pdescr->state == PAGE_STATE_EXCLUSIVELY_SHARED );
		break;
	default:
		Match_failure ( "page_is_accesible\n" );
	}
	
	return ret;
}

static int 
get_manager_id ( int page_no )
{
	return page_no % NUM_OF_PROCS;
}

static struct page_descr_t *
get_pdescr ( struct mon_t *mon, int page_no )
{
	return & ( mon->page_descrs[page_no] );
}

bit32u_t
get_page_paddr ( struct mon_t *mon, int page_no )
{
	return mon->pmem.base + page_no_to_paddr ( page_no );
}

static struct mon_t *static_mon = NULL;

static bool_t
is_not_requesting_page ( struct msg_t *msg )
{
	struct mon_t *mon = static_mon;
	struct msg_page_fetch_request_t *x;
	struct page_descr_t *pdescr;

	assert ( static_mon != NULL );
	assert ( msg != NULL );

//	Print ( stderr, "check: mid=%lld\n", msg->hdr.msg_id );

	if ( ( msg->hdr.kind == MSG_KIND_INPUT_PORT ) || 
	     ( msg->hdr.kind == MSG_KIND_OUTPUT_PORT ) ) {
		return ! accessing_io ( mon );
	}

	if ( msg->hdr.kind != MSG_KIND_PAGE_FETCH_REQUEST ) {
		return TRUE;
	}

	x = Msg_to_msg_page_fetch_request ( msg );
	pdescr = get_pdescr ( mon, x->page_no );
	
	return ( ! pdescr->requesting );
}

static void
send_or_handle ( struct mon_t *mon, struct msg_t *msg, int dest_id, 
		 void (*handler_func)( struct mon_t *, struct msg_t *) )
{
	if ( mon->cpuid == dest_id ) {
		(*handler_func) ( mon, msg );
	} else {
		Comm_send ( mon->comm, msg, dest_id );
	}
}

/*******************************************************************/

void
wait_recvable_msg ( struct mon_t *mon, int sleep_time )
{
	Comm_wait_msg ( mon->comm, &is_not_requesting_page, sleep_time );
}

void
try_handle_msgs ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

   	static_mon = mon;

	for ( ; ; ) {
		struct msg_t *msg;

		msg = Comm_try_remove_msg2 ( mon->comm, &is_not_requesting_page );
		if ( msg == NULL ) {
			break;
		}
		
		handle_msg ( mon, msg );
		Msg_destroy ( msg );
	}
}

void
try_handle_msgs_with_stat ( struct mon_t *mon )
{
	start_time_counter ( &mon->stat.mhandler_counter );
	try_handle_msgs ( mon );
	stop_time_counter ( &mon->stat.mhandler_counter );
}

/*******************************************************************/

void
handle_fetch_ack_ack ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_page_fetch_ack_ack_t *x = Msg_to_msg_page_fetch_ack_ack ( msg );
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );

	DP ( stderr, "handle fetch_ack_ack (no=%#x,seq=%lld, copyset=%#x,%s)\n", 
		x->page_no, pdescr->seq, pdescr->copyset,
		MemAccessKind_to_string ( x->kind ) );

	switch ( x->kind ) {
	case MEM_ACCESS_READ:
		DP ( stderr, "update(man)(rd): (no=%#x)  (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			pdescr->owner,
			pdescr->owner,
			pdescr->copyset,
			pdescr->copyset | ( 1 << x->src_id ) );

		SET_BIT ( pdescr->copyset, x->src_id );
		break;
	 
	case MEM_ACCESS_WRITE:
		DP ( stderr, "update(man)(wr): (no=%#x)  (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			pdescr->owner,
			x->src_id,
			pdescr->copyset,
			1 << x->src_id );

		pdescr->owner = x->src_id;
		pdescr->copyset = ( 1 << x->src_id );
		break;
	 
	default:
		Match_failure ( "send_invalidate_request\n" );
	}

	DP ( stderr, "requesting: FALSE %#x (pdescr->seq=%lld, x->seq=%lld)\n", 
		x->page_no, pdescr->seq, x->seq );
	assert ( pdescr->seq == x->seq );
	pdescr->requesting = FALSE;
}

/*************************************/	

static void
update_pdescr_with_fetch_ack ( struct mon_t *mon, struct msg_page_fetch_ack_t *x, int src_id )
{
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );
	
	switch ( x->kind ) {
	case MEM_ACCESS_READ:
		DP ( stderr, "update(rd): (no=%#x) (%s->%s) (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			PageState_to_string ( pdescr->state ),
			PageState_to_string ( PAGE_STATE_READ_ONLY_SHARED ),
			pdescr->owner,
			src_id,
			pdescr->copyset,
			pdescr->copyset );

		pdescr->state = PAGE_STATE_READ_ONLY_SHARED;
		pdescr->owner = src_id;
		/* copyset do not need to hold the correct value since 
		 * the local node is not owner */
		break;
	 
	case MEM_ACCESS_WRITE:
		DP ( stderr, "update(wr): (no=%#x) (%s->%s) (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			PageState_to_string ( pdescr->state ),
			PageState_to_string ( PAGE_STATE_EXCLUSIVELY_SHARED ),
			pdescr->owner,
			mon->cpuid,
			pdescr->copyset,
			( 1 << mon->cpuid  ) );

		pdescr->state = PAGE_STATE_EXCLUSIVELY_SHARED;
		pdescr->owner = mon->cpuid;
		pdescr->copyset = ( 1 << mon->cpuid );
		break;
	 
	default:
		Match_failure ( "update_pdescr_with_fetch_ack\n" );
	}
}

static void
update_mem_image_with_fetch_ack ( struct mon_t *mon, struct msg_page_fetch_ack_t *x )
{
	bit32u_t addr = get_page_paddr ( mon, x->page_no );
	Mmove ( ( void * ) addr, x->data, PAGE_SIZE_4K );
}

static void
send_or_handle_fetch_ack_ack ( struct mon_t *mon, struct msg_page_fetch_ack_t *x )
{
	const int mid = get_manager_id ( x->page_no );
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_PAGE_FETCH_ACK_ACK,
			    x->page_no, x->kind, mon->cpuid, x->seq );
	send_or_handle ( mon, msg, mid, &handle_fetch_ack_ack );
	Msg_destroy ( msg );
}

static void
__handle_fetch_ack ( struct mon_t *mon, struct msg_page_fetch_ack_t *x,
		     int src_id )
{
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );

	DP ( stderr, "handle fetch_ack (no=%#x,seq=%lld)\n", 
		x->page_no, pdescr->seq );

	update_pdescr_with_fetch_ack ( mon, x, src_id );
	if ( mon->cpuid != src_id ) {
		update_mem_image_with_fetch_ack ( mon, x );
	}
	run_emulation_code_of_vm ( mon, CHANGE_PAGE_PROT, x->page_no );

	send_or_handle_fetch_ack_ack ( mon, x );		
}

void
handle_fetch_ack ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_page_fetch_ack_t *x = Msg_to_msg_page_fetch_ack ( msg );
	__handle_fetch_ack ( mon, x, msg->hdr.src_id );
}

static void
handle_fetch_ack_local ( struct mon_t *mon,
			 struct msg_page_invalidate_request_t *x )
{
	struct msg_page_fetch_ack_t xx;

	xx.page_no = x->page_no;
	xx.kind = x->kind;
	xx.seq = x->seq;

	__handle_fetch_ack ( mon, &xx, mon->cpuid );
}

/*************************************/	

static void
update_pdescr_with_invalidate_request ( struct mon_t *mon, struct msg_page_invalidate_request_t *x )
{
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );

	switch ( x->kind ) {
	case MEM_ACCESS_READ:
		DP ( stderr, "update(rd): (no=%#x) (%s->%s) (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			PageState_to_string ( pdescr->state ),
			PageState_to_string ( PAGE_STATE_READ_ONLY_SHARED ),
			pdescr->owner,
			pdescr->owner,
			pdescr->copyset,
			pdescr->copyset | ( 1 << x->src_id ) );

		assert ( pdescr->state != PAGE_STATE_INVALID );
		pdescr->state = PAGE_STATE_READ_ONLY_SHARED;
		SET_BIT ( pdescr->copyset, x->src_id ); 
		break;
	 
	case MEM_ACCESS_WRITE:
		DP ( stderr, "update(wr): (no=%#x) (%s->%s) (%d->%d) (%#x->%#x)\n", 
			x->page_no,
			PageState_to_string ( pdescr->state ),
			PageState_to_string ( PAGE_STATE_INVALID ),
			pdescr->owner,
			x->src_id,
			pdescr->copyset,
			( 1 << x->src_id ) );

		assert ( pdescr->state != PAGE_STATE_INVALID );
		pdescr->state = PAGE_STATE_INVALID;
		pdescr->owner = x->src_id;
		pdescr->copyset = ( 1 << x->src_id );
		break;
	 
	default:
		Match_failure ( "handle_invalidate_request\n" );
	}
}

static void
send_fetch_ack ( struct mon_t *mon, struct msg_page_invalidate_request_t *x )
{
	bit8u_t *addr = ( bit8u_t * ) get_page_paddr ( mon, x->page_no );
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_PAGE_FETCH_ACK,
			    x->page_no,
			    x->kind,
			    addr,
			    x->seq );

	send_or_handle ( mon, msg, x->src_id, &handle_fetch_ack );
	Msg_destroy ( msg );
}

int nr_recv_invalidates[NUM_OF_PROCS];

void
handle_invalidate_request ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_page_invalidate_request_t *x = Msg_to_msg_page_invalidate_request ( msg );
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );

	DP ( stderr, "handle invalidate_request (no=%#x,seq=%lld,owner=%d)\n", 
		x->page_no, pdescr->seq, pdescr->owner );

	if ( ( mon->cpuid == x->src_id ) && ( mon->cpuid == pdescr->owner ) ) { 
		handle_fetch_ack_local ( mon, x );
	} else {
		if ( mon->cpuid == pdescr->owner ) {
			/* send the up-to-date state of the page to the requestor */
			DP ( stderr, "owner = %d, x->src_id = %d\n", pdescr->owner, x->src_id  );
			send_fetch_ack ( mon, x );
		} else {
			DP ( stderr, "skip sending fetch_ack\n" );
		}

		update_pdescr_with_invalidate_request ( mon, x );
		run_emulation_code_of_vm ( mon, CHANGE_PAGE_PROT, x->page_no );
	}
}

/*************************************/

static void
__send_invalidate_request ( struct mon_t *mon, struct msg_t *msg, int dest_id )
{
	send_or_handle ( mon, msg, dest_id, &handle_invalidate_request );
}

static void
send_invalidate_request_to_copyset ( struct mon_t *mon, struct msg_t *msg, 
					   bit32u_t copyset, int src_id, int owner )
{
	int i;
	
	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( TEST_BIT ( copyset, i ) ) {

			if ( ( i == src_id ) && ( i != mon->cpuid ) && ( i != owner ) ) {
				continue;
			}

			__send_invalidate_request ( mon, msg, i );

		} else {
			DP ( stderr,
				"not send invalidate request to %d: src_id=%d, copyset=%#x\n",
				i , src_id, copyset );
		}
	}
}

static void
send_invalidate_request ( struct mon_t *mon, struct msg_page_fetch_request_t *x,
			  struct page_descr_t *pdescr )
{
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_PAGE_INVALIDATE_REQUEST,
			    x->page_no,
			    x->kind,
			    x->src_id,
			    pdescr->seq );
	
	switch ( x->kind ) {
	case MEM_ACCESS_READ:
		assert ( x->src_id != pdescr->owner );
		__send_invalidate_request ( mon, msg, pdescr->owner );
		break;
	 
	case MEM_ACCESS_WRITE:
		send_invalidate_request_to_copyset ( mon, msg, pdescr->copyset, x->src_id, pdescr->owner );
		break;
	 
	default:
		Match_failure ( "send_invalidate_request\n" );
	}

	Msg_destroy ( msg );	
}

void
handle_fetch_request ( struct mon_t *mon, struct msg_t *msg )
{
	struct msg_page_fetch_request_t *x = Msg_to_msg_page_fetch_request ( msg );
	struct page_descr_t *pdescr = get_pdescr ( mon, x->page_no );

	if ( pdescr->requesting ) {
		struct msg_t *m = Msg_dup ( msg );
		Comm_add_msg ( mon->comm, m, x->src_id );

		DP ( stderr, 
		     "save: (no=%#x,seq=%lld,owner=%d,copyset=%#x,%s), from=%d,%d\n",
		     x->page_no, pdescr->seq, pdescr->owner, pdescr->copyset,
		     MemAccessKind_to_string ( x->kind ),
		     x->src_id, m->hdr.src_id );

		return;
	}

	DP ( stderr, "requesting: TRUE %#x (%lld --> %lld)\n", x->page_no, 
		pdescr->seq, pdescr->seq + 1LL );
	pdescr->seq++;
	pdescr->requesting = TRUE;
	
	DP ( stderr, "handle fetch_request: (no=%#x,seq=%lld,owner=%d,copyset=%#x,%s), from=%d,%d\n",
		x->page_no, pdescr->seq, pdescr->owner, pdescr->copyset,
		MemAccessKind_to_string ( x->kind ),
		x->src_id, msg->hdr.src_id );

	send_invalidate_request ( mon, x, pdescr );
}

/*************************************/

static void
send_or_handle_fetch_request ( struct mon_t *mon, struct shm_arg_t *x )
{
	const int mid = get_manager_id ( x->page_no );
	struct msg_t *msg;

	msg = Msg_create3 ( MSG_KIND_PAGE_FETCH_REQUEST,
			    x->page_no,
			    x->kind,
			    mon->cpuid /* requestor's cpuid */ );
	
	send_or_handle ( mon, msg, mid, &handle_fetch_request );

	Msg_destroy ( msg );     	
}

/********************/
 
static void
recv_and_handle_fetch_ack ( struct mon_t *mon, struct shm_arg_t *x )
{
	DP ( stderr, "wait for fetch_ack: begin (no=%#x, kind=%s)\n",
		x->page_no, MemAccessKind_to_string ( x->kind ) );
//	Print ( stderr, "wait for fetch_ack: begin (no=%#x, kind=%s)\n", x->page_no, MemAccessKind_to_string ( x->kind ) );

	static_mon = mon;

	while ( ! page_is_accesible ( x->pdescr, x->kind ) ) {
		struct msg_t *msg;

		msg = Comm_remove_msg2 ( mon->comm, &is_not_requesting_page );

		handle_msg ( mon, msg );
		Msg_destroy ( msg );
	}

	DP ( stderr, "wake up fetch_ack: end (no=%#x, kind=%s)\n",
		x->page_no, MemAccessKind_to_string ( x->kind ) );
//	Print ( stderr, "wake up fetch_ack: end (no=%#x, kind=%s)\n",x->page_no, MemAccessKind_to_string ( x->kind ) );
}

/********************/

static void
fetch_page ( struct mon_t *mon, struct shm_arg_t *x )
{
	DP_color ( stderr, GREEN, "fetch_page: begin (no=%#x, kind=%s)\n",
		   x->page_no, MemAccessKind_to_string ( x->kind ) );
//	Print_color ( stderr, GREEN, "fetch_page: begin (no=%#x, kind=%s)\n",     x->page_no, MemAccessKind_to_string ( x->kind ) );

	send_or_handle_fetch_request ( mon, x );
	recv_and_handle_fetch_ack ( mon, x );

	DP_color ( stderr, GREEN, "fetch_page: end (no=%#x, kind=%s)\n",
		   x->page_no, MemAccessKind_to_string ( x->kind ) );
//	Print_color ( stderr, GREEN, "fetch_page: end (no=%#x, kind=%s)\n",     x->page_no, MemAccessKind_to_string ( x->kind ) );
}

/****************************************************************/

static bool_t
is_access_violation ( struct shm_arg_t *x )
{
	ASSERT ( x != NULL );

	return ( ( x->pdescr->state == PAGE_STATE_INVALID ) 
		 || 
		 ( ( x->pdescr->state == PAGE_STATE_READ_ONLY_SHARED ) && ( x->kind == MEM_ACCESS_WRITE ) ) );
}

static void
update_fetch_history ( struct mon_t *mon, struct shm_arg_t *x,
		       int vaddr, bool_t is_paddr_access )
{
	struct stat_t *stat = &mon->stat;
	struct fetch_history_t *h;

	if ( stat->nr_fetch_requests >= FETCH_HISOTRY_SIZE ) {
		return;
	}

	h = &(stat->fetch_history[stat->nr_fetch_requests]);

	h->elapsed_time_count = stat->comm_counter.end - stat->exec_start_counter.start;
	h->comm_time_count = stat->comm_counter.end - stat->comm_counter.start; 
	h->kind = x->kind;
	h->eip = mon->regs->user.eip;
	h->esp = mon->regs->user.esp;
	h->vaddr = vaddr;
	h->is_paddr_access = is_paddr_access;

	stat->nr_fetch_requests++;
}

/* return TRUE if the cause of pagefault is shared memory emulation. */
static bool_t
emulate_shared_memory ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr,
			bit32u_t vaddr, bool_t is_paddr_access )
{
	struct shm_arg_t x;

	ASSERT ( mon != NULL );

	x.page_no = paddr_to_page_no ( paddr );
	if ( x.page_no >= mon->num_of_pages ) {
		return FALSE;
	}

	x.pdescr = get_pdescr ( mon, x.page_no );
	x.kind = kind;

	if ( ! is_access_violation ( &x ) ) {
		return FALSE;		
	}


	/* [STAT] */
	mon->stat.comm_counter_flag = TRUE;
	start_time_counter ( &mon->stat.comm_counter );

	fetch_page ( mon, &x );

	/* [STAT] */
	stop_time_counter ( &mon->stat.comm_counter );
	mon->stat.comm_counter_flag = FALSE;
	update_fetch_history ( mon, &x, vaddr, is_paddr_access );

	return TRUE;
}

bool_t
emulate_shared_memory_with_vaddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr,
				   bit32u_t vaddr )
{
	return emulate_shared_memory ( mon, kind, paddr, vaddr, FALSE );
}

bool_t
emulate_shared_memory_with_paddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr )
{
	return emulate_shared_memory ( mon, kind, paddr, 0, TRUE );
}

void
sync_shared_memory ( struct mon_t *mon, struct instruction_t *i )
{
	ASSERT ( mon != NULL );
	ASSERT ( i != NULL );
	ASSERT ( i->opcode != -1 );
     
	DPRINT ( "Sync memory!!!\n" );
}

#else /* !ENABLE_MP */

bool_t
emulate_shared_memory_with_vaddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr, bit32u_t vaddr )
{
	/* Do nothing */     
	return FALSE;
}


bool_t
emulate_shared_memory_with_paddr ( struct mon_t *mon, mem_access_kind_t kind, bit32u_t paddr )
{
	/* Do nothing */     
	return FALSE;
}

void
sync_shared_memory ( struct mon_t *mon, struct instruction_t *i )
{
	/* Do nothing */
}

#endif /* ENABLE_MP */

