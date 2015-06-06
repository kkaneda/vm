#ifndef _VMM_COMM_H
#define _VMM_COMM_H

#include "vmm/std.h"
#include "vmm/comm/msg.h"
#include "vmm/comm/conf.h"


struct comm_t;

int connect_to_node ( const struct node_t *node );
int listen_at_port ( int port );

struct comm_t *Comm_create ( int cpuid, int pid, const struct config_t *config, bool_t is_resuming );
void           Comm_destroy ( struct comm_t *comm );
void           Comm_send ( struct comm_t *comm, struct msg_t *msg, int dest_cpuid );
void           Comm_bcast ( struct comm_t *comm, struct msg_t *msg );
void           Comm_add_msg ( struct comm_t *comm, struct msg_t *msg, int src_id );
struct msg_t  *Comm_remove_msg ( struct comm_t *comm );
struct msg_t  *Comm_try_remove_msg ( struct comm_t *comm );
struct msg_t  *Comm_remove_msg2 ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ) );
struct msg_t  *Comm_try_remove_msg2 ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ) );
void           Comm_wait_msg ( struct comm_t *comm, bool_t (*judge_func) ( struct msg_t * ), int sleep_time );
void           Comm_shutdown ( struct comm_t *comm );
void           Comm_pack_msgs ( struct comm_t *comm, int fd );
void           Comm_unpack_msgs ( struct comm_t *comm, int fd );

#endif /* _VMM_COMM_H */
