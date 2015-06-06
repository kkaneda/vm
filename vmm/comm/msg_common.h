#ifndef _VMM_COMM_MSG_COMMON_H
#define _VMM_COMM_MSG_COMMON_H

#include "vmm/std.h"
#include "vmm/ia32.h"

enum msg_kind {
	MSG_KIND_INVALID,
	MSG_KIND_INIT, 
	MSG_KIND_APIC_LOGICAL_ID, 
	MSG_KIND_IPI,
	MSG_KIND_MEM_IMAGE_REQUEST,
	MSG_KIND_MEM_IMAGE_RESPONSE,
	MSG_KIND_IOAPIC_DUMP,

	MSG_KIND_PAGE_FETCH_REQUEST,
	MSG_KIND_PAGE_FETCH_ACK,
	MSG_KIND_PAGE_INVALIDATE_REQUEST,
	MSG_KIND_PAGE_FETCH_ACK_ACK,

	MSG_KIND_INPUT_PORT,
	MSG_KIND_INPUT_PORT_ACK,
	MSG_KIND_OUTPUT_PORT,
	MSG_KIND_OUTPUT_PORT_ACK,

	MSG_KIND_STAT_REQUEST,
	MSG_KIND_STAT_REQUEST_ACK,

	MSG_KIND_SHUTDOWN,
};
typedef enum msg_kind	msg_kind_t;

const char *MsgKind_to_string(msg_kind_t x);

struct msg_hdr_t {
	msg_kind_t	 	kind;
	size_t			len;	/* size of message body */
	int			src_id;
	long long		msg_id;
};

struct msg_t {
	struct msg_hdr_t	hdr;
	void 			*body; // union の方が自然？
};

struct msg_init_t {
	int			cpuid;
};

struct msg_apic_logical_id_t {
	int			src_apic_id;
	int			logical_id;
};

struct msg_ipi_t {
	struct interrupt_command_t ic;
};

struct msg_page_fetch_request_t {
	int			page_no;
	mem_access_kind_t	kind;
	int			src_id;
};

struct msg_page_fetch_ack_t {
	int			page_no;
	mem_access_kind_t	kind;
	bit8u_t 		data[PAGE_SIZE_4K];
	long long		seq;
};

struct msg_page_fetch_ack_ack_t {
	int			page_no;
	mem_access_kind_t	kind;
	int			src_id;
	long long		seq;
};

struct msg_page_invalidate_request_t {
	int			page_no;
	mem_access_kind_t	kind;
	int			src_id;
	long long		seq;
};

struct msg_input_port_t {
	int			addr;
	size_t			len;
};

struct msg_input_port_ack_t {
	int			addr;
	int			val;
	size_t			len;
	int			irq;
};

struct msg_output_port_t {
	int			addr;
	int			val;
	size_t			len;
};

struct msg_output_port_ack_t {
	int			addr;
	size_t			len;
	int			irq;
};

struct msg_stat_request_t {
	int			dummy;
};

struct msg_stat_request_ack_t {
	bool_t			start_flag;
};

#endif /* _VMM_COMM_MSG_COMMON_H */
