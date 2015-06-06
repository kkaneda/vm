#include "vmm/comm/msg_common.h"


const char *
MsgKind_to_string ( msg_kind_t x )
{
	switch ( x ) {
	case MSG_KIND_INVALID:    	  	return "INVALID";
	case MSG_KIND_INIT:        		return "INIT";
	case MSG_KIND_APIC_LOGICAL_ID:  	return "APIC_LOGICAL_ID";
	case MSG_KIND_IPI:        		return "IPI";
	case MSG_KIND_MEM_IMAGE_REQUEST: 	return "MEM_IMAGE_REQUEST";
	case MSG_KIND_MEM_IMAGE_RESPONSE: 	return "MEM_IMAGE_RESPONSE";
	case MSG_KIND_IOAPIC_DUMP: 		return "IOAPIC_DUMP";
	case MSG_KIND_PAGE_FETCH_REQUEST: 	return "PAGE_FETCH_REQUEST";
	case MSG_KIND_PAGE_FETCH_ACK:   	return "PAGE_FETCH_ACK";
	case MSG_KIND_PAGE_INVALIDATE_REQUEST: 	return "PAGE_INVALIDATE_REQUEST";
	case MSG_KIND_PAGE_FETCH_ACK_ACK:   	return "PAGE_FETCH_ACK_ACK";
		
	case MSG_KIND_INPUT_PORT:		return "INPUT_PORT";
	case MSG_KIND_INPUT_PORT_ACK:		return "INPUT_PORT_ACK";
	case MSG_KIND_OUTPUT_PORT:		return "OUTPUT_PORT";
	case MSG_KIND_OUTPUT_PORT_ACK:		return "OUTPUT_PORT_ACK";

	case MSG_KIND_STAT_REQUEST:		return "STAT_REQUEST";
	case MSG_KIND_STAT_REQUEST_ACK:		return "STAT_REQUEST_ACK";

	case MSG_KIND_SHUTDOWN :		return "SHUTDOWN";

	default: 		        	Match_failure ( "MsgKind_to_string: %d\n", x );
	}
	Match_failure ( "MsgKind_to_string\n" );
	return "";
};

struct msg_t *
Msg_create ( msg_kind_t kind, size_t len, void *body )
{   
	struct msg_t *x;
   
	x = Malloct ( struct msg_t );
	x->hdr.kind = kind;
	x->hdr.len = len;
	if ( body != NULL ) {
		x->body = Malloc ( len );
		Mmove ( x->body, body, len );
	} else {
		x->body = NULL;
	}
	return x;
}

struct msg_t *
Msg_dup ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );

	return Msg_create ( msg->hdr.kind, msg->hdr.len, msg->body );
}

/* Msg_create2 ( ) diffes from Msg_create ( ) in that
  Msg_create2 ( ) does not allocate a new memory for <body> */
struct msg_t *
Msg_create2 ( struct msg_hdr_t hdr, void *body )
{   
	struct msg_t *x;
	x = Malloct ( struct msg_t );
	x->hdr = hdr;
	x->body = body;
	return x;
}

static struct fptr_t
Msg_create3_sub_init ( va_list ap )
{
	struct msg_init_t *x;
	const size_t LEN = sizeof ( struct msg_init_t );

	x = Malloct ( struct msg_init_t ); 
	x->cpuid = ( int )va_arg ( ap, int );

	return Fptr_create ( ( void * )x, LEN );
}

static struct fptr_t
Msg_create3_sub_apic_logical_id ( va_list ap )
{
	struct msg_apic_logical_id_t *x;
	size_t LEN = sizeof ( struct msg_apic_logical_id_t );

	x = Malloct ( struct msg_apic_logical_id_t );
	x->src_apic_id = ( int )va_arg ( ap, int );
	x->logical_id = ( int )va_arg ( ap, int );

	return Fptr_create ( ( void * )x, LEN );
}

static struct fptr_t
Msg_create3_sub_ipi ( va_list ap )
{
	struct msg_ipi_t *x;
	struct interrupt_command_t *p;
	const size_t LEN = sizeof ( struct msg_ipi_t );

	x = Malloct ( struct msg_ipi_t );
	p = ( struct interrupt_command_t * )va_arg ( ap, struct interrupt_command_t * );
	ASSERT ( p != NULL );
	x->ic = *p;
   
	return Fptr_create ( ( void * )x, LEN );
} 

static struct fptr_t
Msg_create3_sub_fetch_request ( va_list ap )
{
	struct msg_page_fetch_request_t *x;
	const size_t LEN = sizeof ( struct msg_page_fetch_request_t );
	
	x = Malloct ( struct msg_page_fetch_request_t );
	x->page_no = ( int )va_arg ( ap, int );
	x->kind = ( mem_access_kind_t )va_arg ( ap, mem_access_kind_t );
	x->src_id = ( int )va_arg ( ap, int );

	return Fptr_create ( ( void * )x, LEN );
}

static struct fptr_t
Msg_create3_sub_fetch_ack ( va_list ap )
{
	struct msg_page_fetch_ack_t *x;
	bit8u_t *p;
	const size_t LEN = sizeof ( struct msg_page_fetch_ack_t );

	x = Malloct ( struct msg_page_fetch_ack_t );
	x->page_no = ( int )va_arg ( ap, int );
	x->kind = ( mem_access_kind_t )va_arg ( ap, mem_access_kind_t );
	p = ( bit8u_t * )va_arg ( ap, bit8u_t * );
	Mmove ( x->data, p, PAGE_SIZE_4K );
	x->seq = ( long long )va_arg ( ap, long long );

	return Fptr_create ( ( void * )x, LEN );
}

static struct fptr_t
Msg_create3_sub_invalidate_request ( va_list ap )
{
	struct msg_page_invalidate_request_t *x;
	const size_t LEN = sizeof ( struct msg_page_invalidate_request_t );

	x = Malloct ( struct msg_page_invalidate_request_t );
	x->page_no = ( int )va_arg ( ap, int );
	x->kind = ( mem_access_kind_t )va_arg ( ap, mem_access_kind_t );
	x->src_id = ( int )va_arg ( ap, int );
	x->seq = ( long long )va_arg ( ap, long long );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_fetch_ack_ack ( va_list ap )
{
	struct msg_page_fetch_ack_ack_t *x;
	const size_t LEN = sizeof ( struct msg_page_fetch_ack_ack_t );

	x = Malloct ( struct msg_page_fetch_ack_ack_t );
	x->page_no = ( int )va_arg ( ap, int );
	x->kind = ( mem_access_kind_t )va_arg ( ap, mem_access_kind_t );
	x->src_id = ( int )va_arg ( ap, int );
	x->seq = ( long long )va_arg ( ap, long long );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_input_port ( va_list ap )
{
	struct msg_input_port_t *x;
	const size_t LEN = sizeof ( struct msg_input_port_t );

	x = Malloct ( struct msg_input_port_t );
	x->addr = ( int )va_arg ( ap, int );
	x->len = ( size_t )va_arg ( ap, size_t );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_input_port_ack ( va_list ap )
{
	struct msg_input_port_ack_t *x;
	const size_t LEN = sizeof ( struct msg_input_port_ack_t );

	x = Malloct ( struct msg_input_port_ack_t );
	x->addr = ( int )va_arg ( ap, int );
	x->val = ( int )va_arg ( ap, int );
	x->len = ( size_t )va_arg ( ap, size_t );
	x->irq = ( int )va_arg ( ap, int );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_output_port ( va_list ap )
{
	struct msg_output_port_t *x;
	const size_t LEN = sizeof ( struct msg_output_port_t );

	x = Malloct ( struct msg_output_port_t );
	x->addr = ( int )va_arg ( ap, int );
	x->val = ( int )va_arg ( ap, int );
	x->len = ( size_t )va_arg ( ap, size_t );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_output_port_ack ( va_list ap )
{
	struct msg_output_port_ack_t *x;
	const size_t LEN = sizeof ( struct msg_output_port_ack_t );

	x = Malloct ( struct msg_output_port_ack_t );
	x->addr = ( int )va_arg ( ap, int );
	x->len = ( size_t )va_arg ( ap, size_t );
	x->irq = ( int )va_arg ( ap, int );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_stat_request ( va_list ap )
{
	struct msg_stat_request_t *x;
	const size_t LEN = sizeof ( struct msg_stat_request_t );

	x = Malloct ( struct msg_stat_request_t );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub_stat_request_ack ( va_list ap )
{
	struct msg_stat_request_ack_t *x;
	const size_t LEN = sizeof ( struct msg_stat_request_ack_t );

	x = Malloct ( struct msg_stat_request_ack_t );
	x->start_flag = ( bool_t )va_arg ( ap, bool_t );

	return Fptr_create ( ( void* )x, LEN );
}

static struct fptr_t
Msg_create3_sub ( msg_kind_t kind, va_list ap )
{
	struct fptr_t body = Fptr_null ( );

	switch ( kind ) {
	case MSG_KIND_INIT: 			body = Msg_create3_sub_init ( ap ); break;
	case MSG_KIND_APIC_LOGICAL_ID: 		body = Msg_create3_sub_apic_logical_id ( ap ); break;
	case MSG_KIND_IPI: 			body = Msg_create3_sub_ipi ( ap ); break;
		
	case MSG_KIND_PAGE_FETCH_REQUEST: 	body = Msg_create3_sub_fetch_request ( ap ); break;
	case MSG_KIND_PAGE_FETCH_ACK: 		body = Msg_create3_sub_fetch_ack ( ap ); break;
	case MSG_KIND_PAGE_INVALIDATE_REQUEST: 	body = Msg_create3_sub_invalidate_request ( ap ); break;
	case MSG_KIND_PAGE_FETCH_ACK_ACK:	body = Msg_create3_sub_fetch_ack_ack ( ap ); break;
		
	case MSG_KIND_INPUT_PORT:		body = Msg_create3_sub_input_port ( ap ); break;
	case MSG_KIND_INPUT_PORT_ACK:		body = Msg_create3_sub_input_port_ack ( ap ); break;
	case MSG_KIND_OUTPUT_PORT:		body = Msg_create3_sub_output_port ( ap ); break;
	case MSG_KIND_OUTPUT_PORT_ACK:		body = Msg_create3_sub_output_port_ack ( ap ); break;
		
	case MSG_KIND_STAT_REQUEST:		body = Msg_create3_sub_stat_request ( ap ); break;
	case MSG_KIND_STAT_REQUEST_ACK:		body = Msg_create3_sub_stat_request_ack ( ap ); break;

	case MSG_KIND_SHUTDOWN:			body = Fptr_null ( ); break;

	default:				Match_failure ( "Msg_create3_sub: kind=%#x", kind );
	}
	return body;
}

struct msg_t *
Msg_create3 ( msg_kind_t kind, ... )
{
	va_list ap;
	struct msg_hdr_t hdr;
	struct fptr_t body;
   
	va_start ( ap, kind );
	body = Msg_create3_sub ( kind, ap );
	// assert ( ! Fptr_is_null ( body ) );
	va_end ( ap );

	hdr.kind = kind;
	hdr.len = body.offset;
	return Msg_create2 ( hdr, body.base );
}

void 
Msg_destroy ( struct msg_t *x )
{
	ASSERT ( x != NULL );
	
	if ( x->body != NULL ) {
		Free ( x->body );
	}
	
	Free ( x );
}

struct msg_init_t *
Msg_to_msg_init ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->body != NULL );
	return ( struct msg_init_t * ) ( msg->body );
}

struct msg_apic_logical_id_t *
Msg_to_msg_apic_logical_id ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_APIC_LOGICAL_ID );
	return ( struct msg_apic_logical_id_t * ) ( msg->body );
}

struct msg_ipi_t *
Msg_to_msg_ipi ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_IPI );
	return ( struct msg_ipi_t * ) ( msg->body );
}

struct msg_page_fetch_request_t *
Msg_to_msg_page_fetch_request ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_PAGE_FETCH_REQUEST );
	return ( struct msg_page_fetch_request_t * ) ( msg->body );
}

struct msg_page_fetch_ack_t *
Msg_to_msg_page_fetch_ack ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_PAGE_FETCH_ACK );
	return ( struct msg_page_fetch_ack_t * ) ( msg->body );
}

struct msg_page_invalidate_request_t *
Msg_to_msg_page_invalidate_request ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_PAGE_INVALIDATE_REQUEST );
	return ( struct msg_page_invalidate_request_t * ) ( msg->body );
}

struct msg_page_fetch_ack_ack_t *
Msg_to_msg_page_fetch_ack_ack ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_PAGE_FETCH_ACK_ACK );
	return ( struct msg_page_fetch_ack_ack_t * ) ( msg->body );	
}

struct msg_input_port_t *
Msg_to_msg_input_port ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_INPUT_PORT );
	return ( struct msg_input_port_t * ) ( msg->body );
}

struct msg_input_port_ack_t *
Msg_to_msg_input_port_ack ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_INPUT_PORT_ACK );
	return ( struct msg_input_port_ack_t * ) ( msg->body );
}

struct msg_output_port_t *
Msg_to_msg_output_port ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_OUTPUT_PORT );
	return ( struct msg_output_port_t * ) ( msg->body );
}

struct msg_output_port_ack_t *
Msg_to_msg_output_port_ack ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_OUTPUT_PORT_ACK );
	return ( struct msg_output_port_ack_t * ) ( msg->body );
}

struct msg_stat_request_t *
Msg_to_msg_stat_request ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_STAT_REQUEST );
	return ( struct msg_stat_request_t * ) ( msg->body );
}

struct msg_stat_request_ack_t *
Msg_to_msg_stat_request_ack ( struct msg_t *msg )
{
	ASSERT ( msg != NULL );
	ASSERT ( msg->hdr.kind == MSG_KIND_STAT_REQUEST_ACK );
	return ( struct msg_stat_request_ack_t * ) ( msg->body );
}

/************************************/

static void
Msg_print_fetch_request ( FILE *stream, struct msg_page_fetch_request_t *x )
{
	Print ( stream, "page_no=%#x, kind=%s", x->page_no, MemAccessKind_to_string ( x->kind ) );
}

static void
Msg_print_fetch_ack ( FILE *stream, struct msg_page_fetch_ack_t *x )
{
	Print ( stream, "page_no=%#x, kind=%s", x->page_no, MemAccessKind_to_string ( x->kind ) );
}

static void
Msg_print_invalidate_request ( FILE *stream, struct msg_page_invalidate_request_t *x )
{
	Print ( stream, "page_no=%#x, kind=%s, src_id=%#x",
		x->page_no, MemAccessKind_to_string ( x->kind ), x->src_id );
}

static void
Msg_print_fetch_ack_ack ( FILE *stream, struct msg_page_fetch_ack_ack_t *x )
{
	Print ( stream, "page_no=%#x, kind=%s, src_id=%#x",
		x->page_no, MemAccessKind_to_string ( x->kind ), x->src_id );
}

void
Msg_print ( FILE *stream, struct msg_t *msg )
{
	ASSERT ( stream != NULL );
	ASSERT ( msg != NULL );

	Print ( stream, "{ kind=%s, mid=%lld, ",
		MsgKind_to_string ( msg->hdr.kind ), msg->hdr.msg_id ); 

	switch ( msg->hdr.kind ) {
	case MSG_KIND_INVALID:
	case MSG_KIND_INIT:
	case MSG_KIND_APIC_LOGICAL_ID:
	case MSG_KIND_IPI:
	case MSG_KIND_MEM_IMAGE_REQUEST:
	case MSG_KIND_MEM_IMAGE_RESPONSE:
	case MSG_KIND_IOAPIC_DUMP:
		break;

	case MSG_KIND_PAGE_FETCH_REQUEST:
		Msg_print_fetch_request ( stream, ( struct msg_page_fetch_request_t * ) ( msg->body ) );
		break;
	case MSG_KIND_PAGE_FETCH_ACK:
		Msg_print_fetch_ack ( stream, ( struct msg_page_fetch_ack_t * ) ( msg->body ) );
		break;
	case MSG_KIND_PAGE_INVALIDATE_REQUEST:
		Msg_print_invalidate_request ( stream, ( struct msg_page_invalidate_request_t * ) ( msg->body ) );
		break;
	case MSG_KIND_PAGE_FETCH_ACK_ACK:
		Msg_print_fetch_ack_ack ( stream, ( struct msg_page_fetch_ack_ack_t * ) ( msg->body ) );
		break;

	case MSG_KIND_INPUT_PORT:
	case MSG_KIND_INPUT_PORT_ACK:
	case MSG_KIND_OUTPUT_PORT:
	case MSG_KIND_OUTPUT_PORT_ACK:
		break;

	case MSG_KIND_STAT_REQUEST:
	case MSG_KIND_STAT_REQUEST_ACK:
		break;

	case MSG_KIND_SHUTDOWN:
		break;

	default:
		Match_failure ( "Msg_print" );
	}

	Print ( stream, " }\n" );
}

#ifdef DEBUG

void MSG_DPRINT ( struct msg_t *msg ) { Msg_print ( stderr, msg ); }

#else /* DEBUG */

void MSG_DPRINT ( struct msg_t *msg ) { }

#endif /* DEBUG */

void
Msg_pack ( struct msg_t *msg, int fd )
{
	ASSERT ( msg != NULL );

	Pack ( &msg->hdr, sizeof ( struct msg_hdr_t ), fd );
	if ( msg->hdr.len > 0 ) {
		Pack ( msg->body, msg->hdr.len, fd );
	}
}

struct msg_t *
Msg_unpack ( int fd )
{
	struct msg_hdr_t hdr;
	void *body;

	Unpack ( &hdr, sizeof ( struct msg_hdr_t ), fd );

	if ( hdr.len > 0 ) {
		body = Malloc ( hdr.len );
		Unpack ( body, hdr.len, fd );
	} else {
		body = NULL; 
	}

	return Msg_create2 ( hdr, body );
}

/************************************/

void
Msg_send ( struct msg_t *msg, int fd )
{
	ASSERT ( msg != NULL );

	Sendn ( fd, &msg->hdr, sizeof ( struct msg_hdr_t ), 0 );

	if ( msg->hdr.len > 0 ) {
		Sendn ( fd, msg->body, msg->hdr.len, 0 );
	}
}

struct msg_t *
Msg_recv ( int fd, int src_id )
{
	struct msg_hdr_t hdr;
	void *body;

	Recvn ( fd, &hdr, sizeof ( struct msg_hdr_t ), 0 );

	hdr.src_id = src_id;
	DPRINT ( " ( comm )\t" "Msg_recv: src_id=%#x, len=%#x\n", hdr.src_id, hdr.len );

	if ( hdr.len > 0 ) {
		body = Malloc ( hdr.len );
		Recvn ( fd, body, hdr.len, 0 );
	} else {
		body = NULL;
	}

	return Msg_create2 ( hdr, body );
}

/****************************************************************/

struct msg_list_elem_t {
	struct msg_t		*msg;
	struct msg_list_elem_t 	*next;
};

struct msg_list_elem_t *
MsgListElem_create ( struct msg_t *msg, struct msg_list_elem_t *next )
{
	struct msg_list_elem_t *x;
	
	ASSERT ( msg == NULL );

	x = Malloct ( struct msg_list_elem_t );
	x->msg = msg;
	x->next = next;
	
	return x;
}

void
MsgListElem_destroy ( struct msg_list_elem_t *x )
{
	ASSERT ( x != NULL );

	if ( x->msg != NULL ) {
		Msg_destroy ( x->msg );
	}

	Free ( x );
}

void
MsgListElem_destroy2 ( struct msg_list_elem_t *x )
{
	ASSERT ( x != NULL );
	Free ( x );
}

/****************************************************************/

struct msg_list_t {
	struct msg_list_elem_t	*head, *tail;
	pthread_mutex_t		mp;
	pthread_cond_t		cond;
};

struct msg_list_t *
MsgList_create ( void )
{
	struct msg_list_t *l;

	l = Malloct ( struct msg_list_t );
	l->head = NULL;
	l->tail = NULL;
	
	Pthread_mutex_init ( &l->mp, NULL );
	Pthread_cond_init ( &l->cond, NULL );

	return l;
}

void
MsgList_destroy ( struct msg_list_t *l )
{
	struct msg_list_elem_t *p, *next;
	ASSERT ( l != NULL );

	p = l->head;
	next = NULL;
	while ( p != NULL ) {
		next = p->next;
		MsgListElem_destroy ( p );
		p = next;
	}
   
	Pthread_mutex_destroy ( &l->mp );
	Pthread_cond_destroy ( &l->cond );
	Free ( l );
}

void
MsgList_add ( struct msg_list_t *l, struct msg_t *msg )
{
	struct msg_list_elem_t *x;

	ASSERT ( l != NULL );
	ASSERT ( msg != NULL );

	x = Malloct ( struct msg_list_elem_t );
	x->msg = msg;
	x->next = NULL;

	Pthread_mutex_lock ( &l->mp );
	
	if ( l->head == NULL ) {
		l->head = x;
	}
	if ( l->tail != NULL ) {
		l->tail->next = x;
	}
	l->tail = x; 

	Pthread_cond_broadcast ( &l->cond );
	Pthread_mutex_unlock ( &l->mp );
}

struct msg_t *
__msg_list_remove ( struct msg_list_t *l )
{
	struct msg_t *ret;
	struct msg_list_elem_t *next;

	ret = l->head->msg;
	ASSERT ( ret != NULL );
	next = l->head->next;

	MsgListElem_destroy2 ( l->head );
   
	l->head = next;
	if ( l->head == NULL ) {
		l->tail = NULL;
	}

	return ret;
}

struct msg_t *
MsgList_try_remove ( struct msg_list_t *l )
{
	struct msg_t *ret;
	
	ASSERT ( l != NULL );

	Pthread_mutex_lock ( &l->mp );
	ret = ( l->head != NULL ) ? __msg_list_remove ( l ) : NULL;
	Pthread_mutex_unlock ( &l->mp );

	return ret;
}

struct msg_t *
MsgList_remove ( struct msg_list_t *l )
{
	struct msg_t *ret;

	ASSERT ( l != NULL );

	Pthread_mutex_lock ( &l->mp );

	while ( l->head == NULL ) {
		Pthread_cond_wait ( &l->cond, &l->mp );
	}

	ret = __msg_list_remove ( l );

	Pthread_mutex_unlock ( &l->mp );

	return ret;
}

static bool_t
exists_recvable_msgs ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) )
{
	struct msg_list_elem_t *p;

	assert ( l != NULL );
	assert ( judge_func != NULL );

	p = l->head;

	while ( p != NULL ) {
		assert ( p->msg != NULL );
		if ( (*judge_func) ( p->msg ) ) {
			return TRUE;
		}
		p = p->next;
	}

	return FALSE;
}

void
MsgList_wait ( struct msg_list_t *l,
	       bool_t (*judge_func) ( struct msg_t * ),
	       int sleep_time )
{
	ASSERT ( l != NULL );

	Pthread_mutex_lock ( &l->mp );

	if ( exists_recvable_msgs ( l, judge_func ) ) {	
		Pthread_mutex_unlock ( &l->mp );
		return;
	}

	struct timespec ts;
	ts = Timespec_current ( );
#if 0
	ts = Timespec_add3 ( ts, sleep_time );
#else
	/* [DEBUG] */
	ts.tv_nsec += sleep_time;
	/*
	if ( ts.tv_nsec >= 1000000000 ) {
		ts.tv_sec += 1;
		ts.tv_nsec -= 1000000000;
	}
	*/
#endif
	
	Pthread_cond_timedwait ( &l->cond, &l->mp, &ts );

	Pthread_mutex_unlock ( &l->mp );
}

static struct msg_t *
__msg_list_remove2 ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) )
{
	struct msg_list_elem_t *p, *prev;
	struct msg_t *ret = NULL;

	p = l->head;
	prev = NULL;
	while ( p != NULL ) {
		assert ( p->msg != NULL );

		if ( (*judge_func) ( p->msg ) ) {
 
			if ( prev == NULL ) {
				l->head = p->next;
			} else {
				prev->next = p->next;
			}
			if ( p == l->tail ) {
				l->tail = prev;
			}		

			ret = p->msg;
			MsgListElem_destroy2 ( p );			
			break;
		}

		prev = p;
		p = p->next;
	}
	
	return ret;
}

struct msg_t *
MsgList_remove2 ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) )
{
	struct msg_t *ret;

	ASSERT ( l != NULL );
	ASSERT ( judge_func != NULL );

	Pthread_mutex_lock ( &l->mp );

	while ( ! exists_recvable_msgs ( l, judge_func ) ) {
		Pthread_cond_wait ( &l->cond, &l->mp );
	}

	ret = __msg_list_remove2 ( l, judge_func );
	assert ( ret != NULL );

	Pthread_mutex_unlock ( &l->mp );

	return ret;
}

struct msg_t *
MsgList_try_remove2 ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) )
{
	struct msg_t *ret;
	
	ASSERT ( l != NULL );

	Pthread_mutex_lock ( &l->mp );
	ret = __msg_list_remove2 ( l, judge_func );
	Pthread_mutex_unlock ( &l->mp );

	return ret;
}

/* [Note] no lock acquired */
size_t
__msg_list_nr_elems ( struct msg_list_t *l )
{
	size_t ret = 0;

	struct msg_list_elem_t *p;

	for ( p = l->head; p != NULL; p = p->next ) {
		ret++;
	}
	return ret;
}

size_t
MsgList_nr_elems ( struct msg_list_t *l )
{
	size_t ret;

	Pthread_mutex_lock ( &l->mp );	
	ret = __msg_list_nr_elems ( l );
	Pthread_mutex_unlock ( &l->mp );	
	return ret;
}

void
MsgList_pack ( struct msg_list_t *l, int fd )
{
	struct msg_list_elem_t *p;

	ASSERT ( l != NULL );

	Pthread_mutex_lock ( &l->mp );
	
	Bit32u_pack ( ( bit32u_t ) __msg_list_nr_elems ( l ), fd );

	p = l->head;
	for ( p = l->head; p != NULL; p = p->next ) {
		assert ( p->msg != NULL );
		Msg_pack ( p->msg, fd );
	}

	Pthread_mutex_unlock ( &l->mp );
}

void
MsgList_unpack ( struct msg_list_t *l, int fd )
{
	int nelms, i;

	ASSERT ( l != NULL );

	nelms = ( int ) Bit32u_unpack ( fd );

	for ( i = 0; i < nelms; i++ ) {
		struct msg_t *msg;
		msg = Msg_unpack ( fd ); 
		MsgList_add ( l, msg );
	}
}
