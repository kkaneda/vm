#include "vmm/std.h"
#include "vmm/comm/msg.h"
#include "vmm/comm/conf.h"
#include <sys/socket.h>

#ifdef ENABLE_MP

int
connect_to_node ( const struct node_t *node )
{
	in_addr_t in_addr;
	int sockfd;
	struct sockaddr_in sin;

	ASSERT ( node != NULL );

	sockfd = Socket ( AF_INET, SOCK_STREAM, 0 );

	in_addr = InAddr_resolve ( node->hostname );	 
	sin = Inet_sockaddr_create ( in_addr, node->port );
   
	DPRINT ( "connecting to %s:%d\t", node->hostname, node->port );
	for ( ; ; ) {
		int err;

		err = connect ( sockfd, ( struct sockaddr * )&sin, sizeof ( sin ) );
		if ( err == 0 )
			break;
		Sleep ( 1 );
	}

	DPRINT ( "succeeded: sockfd=%d\n", sockfd );
	Setsockflag_nonblock ( sockfd );
	Setsockopt_nodelay ( sockfd );

	return sockfd;
}

int
listen_at_port ( int port )
{
	enum { LISTENQ = 64 };
	int sockfd;
	struct sockaddr_in sin;

	sockfd = Socket ( AF_INET, SOCK_STREAM, 0 );
	Setsockopt_reuseaddr ( sockfd );

	sin = Inet_sockaddr_create ( htonl ( INADDR_ANY ), port );
	Bind ( sockfd, ( struct sockaddr * )&sin, sizeof ( sin ) );
	Listen ( sockfd, LISTENQ );

	return sockfd;
}

/****************************************************************/

struct conn_t {
	int 			sockfd;
	struct msg_t *		recving_msg;
	size_t			recv_offset;
	bool_t			recving_header;
	bool_t			is_sending;
	bool_t			is_active;
	
	pthread_mutex_t		mp;
	pthread_cond_t		cond;
};


static void
init_recv_info ( struct conn_t *conn )
{
	conn->recving_header = TRUE;
	conn->recving_msg = Msg_create ( MSG_KIND_INVALID, 0, NULL );
	conn->recv_offset = 0;
}

static void
init_conn ( struct conn_t *x ) 
{
	x->is_active = FALSE;
	x->is_sending = FALSE;
	x->sockfd = -1;

	init_recv_info ( x );

	Pthread_mutex_init ( &x->mp, NULL );
	Pthread_cond_init ( &x->cond, NULL );
}

void
conn_set_active ( struct conn_t *x, int sockfd )
{
	Pthread_mutex_lock ( &x->mp );
	x->sockfd = sockfd;
	x->is_active = TRUE;
	Pthread_cond_broadcast ( &x->cond );
	Pthread_mutex_unlock ( &x->mp );
}

void
conn_set_inactive ( struct conn_t *x )
{
	Pthread_mutex_lock ( &x->mp );
	x->is_active = FALSE;
	Pthread_cond_broadcast ( &x->cond );
	Pthread_mutex_unlock ( &x->mp );
}

/* Do not send any messages when the opposite endpoint is migrating. */
static void
wait_until_conn_becomes_ready_to_send ( struct conn_t *x )
{
	Pthread_mutex_lock ( &x->mp );
	while ( ! x->is_active ) {
		Pthread_cond_wait ( &x->cond, &x->mp );
	}
	x->is_sending = TRUE;
	Pthread_mutex_unlock ( &x->mp );
}

static void
notify_send_completion ( struct conn_t *x )
{
	Pthread_mutex_lock ( &x->mp );
	x->is_sending = FALSE;
	Pthread_cond_broadcast ( &x->cond );
	Pthread_mutex_unlock ( &x->mp );	
}

static void
wait_until_conn_becomes_ready_to_close ( struct conn_t *x )
{
	Pthread_mutex_lock ( &x->mp );
	x->is_active = FALSE;
	
	while ( x->is_sending ) {
		Pthread_cond_wait ( &x->cond, &x->mp );
	}
	Pthread_mutex_unlock ( &x->mp );
}

static void
close_conn ( struct conn_t *x )
{
	/* Wait until no message is in transmit */	
	wait_until_conn_becomes_ready_to_close ( x );

	/* Close the old connection */
	Shutdown ( x->sockfd, SHUT_RDWR );
	Close ( x->sockfd ); 
	x->sockfd = -1;

	assert ( x->recving_msg != NULL );
	Msg_destroy ( x->recving_msg );
	x->recving_msg = NULL;

	conn_set_inactive ( x );
}

static void
wait_until_conn_becomes_inactive ( struct conn_t *x )
{
	Pthread_mutex_lock ( &x->mp );
	while ( x->is_active ) {
		Pthread_cond_wait ( &x->cond, &x->mp );
	}
	Pthread_mutex_unlock ( &x->mp );
}

/******************************************************/

struct comm_t {
	int			cpuid;
	struct msg_list_t 	*msgs;

	pid_t			pid;	/* Process ID of which the process receives the SIGUSR2
					   when a new message is arrived. */
	pthread_t		tid;
	int 			lsockfd;
	struct conn_t		conns[NUM_OF_PROCS];
};

/* The prototype declaration */
static void *recv_loop ( void *x );
void Comm_send ( struct comm_t *comm, struct msg_t *msg, int dest_cpuid );
void Comm_bcast ( struct comm_t *comm, struct msg_t *msg );
struct msg_t *Comm_recv ( struct comm_t *comm );


static void
__init_socks_connect ( struct comm_t *comm, const struct node_t *node, int cpuid )
{
	struct msg_t *msg;
   
	conn_set_active ( &comm->conns[cpuid], connect_to_node ( node ) );
   
	msg = Msg_create3 ( MSG_KIND_INIT, comm->cpuid );
	Comm_send ( comm, msg, cpuid );
	Msg_destroy ( msg );   
}

static void
init_socks_connect ( struct comm_t *comm, const struct config_t *config, bool_t is_resuming )
{
	int i;

	if ( is_resuming ) {
		/* connect to all other processes */
		for ( i = 0; i < NUM_OF_PROCS; i++ ) {
			if ( i != comm->cpuid ) {
				__init_socks_connect ( comm, &config->nodes[i], i );
			}
		}
	} else {
		for ( i = comm->cpuid + 1; i < NUM_OF_PROCS; i++ ) {
			__init_socks_connect ( comm, &config->nodes[i], i );
		}
	}
}

static void
__init_socks_accept ( struct comm_t *comm )
{
	enum { UNKNOWN_SRC = -1 };
	int asockfd;
	struct msg_t *msg;
	struct msg_init_t *x;
	int cpuid;

	asockfd = Accept ( comm->lsockfd, NULL, NULL );
	Setsockflag_nonblock ( asockfd );
	Setsockopt_nodelay ( asockfd );
   
	msg = Msg_recv ( asockfd, UNKNOWN_SRC );
	x = Msg_to_msg_init ( msg );
	cpuid = x->cpuid;
	conn_set_active ( &comm->conns[cpuid], asockfd );
	Msg_destroy ( msg );

	DPRINT ( "[CPU%d]\t" "accepted: sockfd=%d ( from cpuid=%d )\n", 
		 comm->cpuid, comm->conns[cpuid].sockfd, cpuid );   
}

static void
init_socks_accept ( struct comm_t *comm, bool_t is_resuming )
{
	int i;

	if ( is_resuming ) {
		return;
	}

	for ( i = 0; i < comm->cpuid; i++ ) {
		__init_socks_accept ( comm );
	}
}

static void
init_socks ( struct comm_t *comm, const struct config_t *config, bool_t is_resuming )
{
	ASSERT ( comm != NULL );
	ASSERT ( config != NULL );

	comm->lsockfd = listen_at_port ( config->nodes[comm->cpuid].port );
	init_socks_connect ( comm, config, is_resuming );
	init_socks_accept ( comm, is_resuming );
}

struct comm_t *
Comm_create ( int cpuid, int pid, const struct config_t *config, bool_t is_resuming )
{
	struct comm_t * comm;
	int i;
	pthread_attr_t attr;

	ASSERT ( config != NULL );
   
	comm = Malloct ( struct comm_t );
	comm->cpuid = cpuid;
	comm->pid = pid;
	comm->msgs = MsgList_create ( );
   
	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		init_conn ( &comm->conns[i] );
	}
	init_socks ( comm, config, is_resuming );

	Pthread_attr_init ( &attr );
#if 0
	Pthread_attr_setscope ( &attr, PTHREAD_SCOPE_PROCESS );
#else
	Pthread_attr_setscope ( &attr, PTHREAD_SCOPE_SYSTEM );
#endif
	Pthread_create ( &comm->tid, &attr, &recv_loop, ( void * )comm );
	Pthread_attr_destroy ( &attr );

	return comm;
}

void
Comm_destroy ( struct comm_t *comm )
{
	ASSERT ( comm != NULL );

	MsgList_destroy ( comm->msgs );

	/* TODO: close and shutdown the sockets. */

	Pthread_join ( comm->tid, NULL );
	Free ( comm );
}

void
Comm_send ( struct comm_t *comm, struct msg_t *msg, int dest_cpuid )
{
	ASSERT ( comm != NULL );
	ASSERT ( msg != NULL );
	ASSERT ( comm->cpuid != dest_cpuid );

	static long long msg_id = 0LL;
	msg->hdr.msg_id = msg_id;
	msg_id++;

	// Print ( stderr, "Comm_send: " ); Msg_print ( stderr, msg );

	struct conn_t *conn = &comm->conns[dest_cpuid];

	wait_until_conn_becomes_ready_to_send ( conn );

	Msg_send ( msg, conn->sockfd );

	notify_send_completion ( conn );
}

void
Comm_bcast ( struct comm_t *comm, struct msg_t *msg )
{
	int i;

	ASSERT ( comm != NULL );
	ASSERT ( msg != NULL );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( i == comm->cpuid ) {
			continue;
		}

		Comm_send ( comm, msg, i );
	}
}

/************************************************/

static int 
get_maxfds ( struct comm_t *comm )
{
	int max_fds = 0;
	int i;	

  	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( ( i != comm->cpuid ) && ( comm->conns[i].sockfd + 1 > max_fds ) ) {
			max_fds = comm->conns[i].sockfd + 1;
		}
	}
	
	return max_fds;
}

static void
init_fds ( struct comm_t *comm, fd_set *fds )
{
	int i;

	FD_ZERO ( fds );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( ( i != comm->cpuid ) && ( comm->conns[i].sockfd != -1 ) ) { 
			FD_SET ( comm->conns[i].sockfd, fds );
		}
	}
}

static bool_t
try_recv ( struct conn_t *conn, char *base, size_t len )
{
	ssize_t n;
	
	assert ( base != NULL );
	assert ( base + conn->recv_offset != NULL );

	n = Recv ( conn->sockfd, base + conn->recv_offset, len - conn->recv_offset, 0 );
	if ( n == -1 ) {
		/* No resource temporarily available */
		return TRUE;
	}

	if ( n == 0 ) {
		Warning ( "try_recv: failed to receive a message: n = %d\n", n );
		close_conn ( conn );
		return FALSE;
	}

	conn->recv_offset += n;
	return TRUE;
}

static struct msg_t *
try_recv_msg_body ( struct comm_t *comm, int src_id )
{
	struct conn_t *conn = &comm->conns[src_id];
	struct msg_t *m = conn->recving_msg;
	bool_t b;

	assert ( m->body != NULL );
	b  = try_recv ( conn, m->body, m->hdr.len );
	if ( ! b ) {
		return NULL;
	}
	
	if ( conn->recv_offset < m->hdr.len ) {
		return NULL;
	}
		
	init_recv_info ( conn );
	return m;
}

static struct msg_t *
try_recv_msg_header ( struct comm_t *comm, int src_id )
{
	struct conn_t *conn = &comm->conns[src_id];
	struct msg_t *m = conn->recving_msg;
	bool_t b;
	
	assert ( ( void * )&(m->hdr) != NULL );

	b = try_recv ( conn, ( void * )&(m->hdr), sizeof ( struct msg_hdr_t ) );
	if ( ! b ) {	       
		return NULL;
	}

	if ( conn->recv_offset < sizeof ( struct msg_hdr_t ) ) {
		return NULL;
	}

	assert ( m->hdr.kind != MSG_KIND_INVALID );
	m->hdr.src_id = src_id;
	
	if ( m->hdr.len == 0 ) {
		init_recv_info ( conn );
		return m;
	} 
	
	m->body = Malloc ( m->hdr.len );
	conn->recving_header = FALSE;
	conn->recv_offset = 0;
	return try_recv_msg_body ( comm, src_id );		
}

static struct msg_t *
try_recv_msg ( struct comm_t *comm, int src_id )
{
	if ( comm->conns[src_id].recving_header ) {
		return  try_recv_msg_header ( comm, src_id );
	} else {
		return try_recv_msg_body ( comm, src_id );
	}
}

static struct msg_t *
__recv_loop ( struct comm_t *comm, fd_set *fds, int max_fds )
{
	struct msg_t *msg = NULL;

	ASSERT ( comm != NULL );
	ASSERT ( fds != NULL );

	while ( msg == NULL ) {
		int i;

		select ( max_fds, fds, NULL /* write */, NULL /* error */, NULL /* timeout */ );

		for ( i = 0; i < NUM_OF_PROCS; i++ ) {
			if ( ( i == comm->cpuid ) || ( comm->conns[i].sockfd == -1 ) ) { 
				continue;
			}

			if ( ! FD_ISSET ( comm->conns[i].sockfd, fds ) ) {
				FD_SET ( comm->conns[i].sockfd, fds );
				continue;
			}

			msg = try_recv_msg ( comm, i );
			if ( msg != NULL ) {
				break;
			}
		}
	}

	return msg;
}

static void 
accept_new_conn ( struct comm_t *comm, int src_id )
{
	struct conn_t *conn = &comm->conns[src_id];

	close_conn ( conn );

	/* Accept a new connection from a host to which CPU|src_id| moves  */
	__init_socks_accept ( comm );
	init_recv_info ( conn );

	assert ( conn->sockfd != -1 );
}

static void *
recv_loop ( void *x )
{
	struct comm_t *comm = ( struct comm_t * )x;
	fd_set fds;
	int max_fds;

	ASSERT ( comm != NULL );

	max_fds = get_maxfds ( comm );

	for ( ; ; ) {	
		struct msg_t *msg;

		init_fds ( comm, &fds );
		msg = __recv_loop ( comm, &fds, max_fds );

		// Print ( stderr, "Comm_recv: " ); Msg_print ( stderr, msg );

		if ( msg->hdr.kind == MSG_KIND_SHUTDOWN ) {
			accept_new_conn ( comm, msg->hdr.src_id );
			max_fds = get_maxfds ( comm );	
			/* Do not deliver the message to the monitor process */
			Msg_destroy ( msg );
		} else {
			MsgList_add ( comm->msgs, msg );
			Kill ( comm->pid, SIGUSR2 );
		}
	}
}

void
Comm_add_msg ( struct comm_t *comm, struct msg_t *msg, int src_id )
{
	assert ( comm != NULL );
	assert ( msg != NULL );

	msg->hdr.src_id = src_id;
	MsgList_add ( comm->msgs, msg );
}

struct msg_t *
Comm_try_remove_msg ( struct comm_t *comm )
{
	ASSERT ( comm != NULL );

	return MsgList_try_remove ( comm->msgs );
}

struct msg_t *
Comm_remove_msg ( struct comm_t *comm )
{
	ASSERT ( comm != NULL );

	return MsgList_remove ( comm->msgs );
}

struct msg_t *
Comm_remove_msg2 ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ) )
{
	ASSERT ( comm != NULL );

	return MsgList_remove2 ( comm->msgs, judge_func );
}

struct msg_t *
Comm_try_remove_msg2 ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ) )
{
	ASSERT ( comm != NULL );

	return MsgList_try_remove2 ( comm->msgs, judge_func );
}

void
Comm_wait_msg ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ), int sleep_time )
{
	ASSERT ( comm != NULL );

	MsgList_wait ( comm->msgs, judge_func, sleep_time );
}

void
Comm_shutdown ( struct comm_t *comm )
{
	ASSERT ( comm != NULL );

	{
		struct msg_t *msg = Msg_create3 ( MSG_KIND_SHUTDOWN );
		Comm_bcast ( comm, msg );
		Msg_destroy ( msg );
	}

	int i;
	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( i == comm->cpuid ) {
			continue;
		}
		wait_until_conn_becomes_inactive ( &comm->conns[i] );
	}
}

void
Comm_pack_msgs ( struct comm_t *comm, int fd )
{
	ASSERT ( comm != NULL );

	MsgList_pack ( comm->msgs, fd );
}

void
Comm_unpack_msgs ( struct comm_t *comm, int fd )
{
	ASSERT ( comm != NULL );

	MsgList_unpack ( comm->msgs, fd );
}

#else /* ! ENBLE_MP */

struct comm_t *
Comm_create ( int cpuid, int pid, const struct config_t *config, bool_t is_resuming )
{
	return NULL;
}

#endif /* ENABLE_MP */
