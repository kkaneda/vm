#ifndef _VMM_COMM_MSG_H
#define _VMM_COMM_MSG_H

#include "vmm/comm/msg_common.h"


struct msg_t *Msg_create(msg_kind_t kind, size_t len, void *body);
struct msg_t *Msg_dup ( struct msg_t *msg );
struct msg_t *Msg_create2(struct msg_hdr_t hdr, void *body);
struct msg_t *Msg_create3(msg_kind_t kind, ...);
void          Msg_destroy(struct msg_t *x);
void          Msg_print(FILE *stream, struct msg_t *x);
void          MSG_DPRINT(struct msg_t *x);
void          Msg_send(struct msg_t *msg, int fd);
struct msg_t *Msg_recv(int fd, int src_id);

struct msg_init_t               *Msg_to_msg_init(struct msg_t *msg);
struct msg_apic_logical_id_t    *Msg_to_msg_apic_logical_id(struct msg_t *msg);
struct msg_ipi_t                *Msg_to_msg_ipi(struct msg_t *msg);
struct msg_page_fetch_request_t *Msg_to_msg_page_fetch_request(struct msg_t *msg);
struct msg_page_fetch_ack_t     *Msg_to_msg_page_fetch_ack(struct msg_t *msg);
struct msg_page_invalidate_request_t *Msg_to_msg_page_invalidate_request(struct msg_t *msg);
struct msg_page_fetch_ack_ack_t *Msg_to_msg_page_fetch_ack_ack(struct msg_t *msg);
struct msg_page_seq_update_t *Msg_to_msg_page_seq_update(struct msg_t *msg);
struct msg_input_port_t *Msg_to_msg_input_port ( struct msg_t *msg );
struct msg_input_port_ack_t *Msg_to_msg_input_port_ack ( struct msg_t *msg );
struct msg_output_port_t *Msg_to_msg_output_port ( struct msg_t *msg );
struct msg_output_port_ack_t *Msg_to_msg_output_port_ack ( struct msg_t *msg );
struct msg_stat_request_t *Msg_to_msg_stat_request ( struct msg_t *msg );
struct msg_stat_request_ack_t *Msg_to_msg_stat_request_ack ( struct msg_t *msg );

/****************************************************************/

struct msg_list_t;

struct msg_list_t *MsgList_create(void);
void               MsgList_destroy(struct msg_list_t *l);
void               MsgList_add(struct msg_list_t *l, struct msg_t *msg);
struct msg_t *     MsgList_try_remove(struct msg_list_t *l);
struct msg_t *     MsgList_remove(struct msg_list_t *l);
struct msg_t *     MsgList_remove2 ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) );
struct msg_t *     MsgList_try_remove2 ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ) );
void               MsgList_wait ( struct msg_list_t *l, bool_t (*judge_func) ( struct msg_t * ), int sleep_time );
void               MsgList_pack ( struct msg_list_t *l, int fd );
void               MsgList_unpack ( struct msg_list_t *l, int fd );

#endif /* _VMM_COMM_MSG_H */
