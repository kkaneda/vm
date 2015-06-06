#ifndef _VMM_IA32_PAGING_COMMON_H
#define _VMM_IA32_PAGING_COMMON_H

#include "vmm/std.h"
#include "vmm/ia32/maccess.h"
#include "vmm/ia32/regs.h"


enum {
	NUM_OF_PDIR_ENTRIES	 = 1024,
	NUM_OF_PTBL_ENTRIES 	= 1024
};

enum {
	PDIR_ENTRY_SIZE 	= 4,
	PTBL_ENTRY_SIZE 	= 4,
	PAGE_SIZE_4K		= 0x1000,	/* = 4096 = (1 << 12) */
	PAGE_SIZE_4M		= 0x400000	/* = (1 << 22) */
};

/* Linear Address */
struct linear_addr_t {
	bit10u_t		dir; 		/* directory address (bits 22 through 31) */
	bit10u_t		table; 		/* table address (bits 12 through 21) */
	bit12u_t		offset;	 	/* offset (bits 0 through 11) */
};

/* Page-Directory Entry 
 * [Reference] IA-32 manual. Vol.3 3-24. */
struct pdir_entry_t {
	bit32u_t		paddr;		/* physical address where this entry is located */
	bit32u_t		val;

	union {
		bit20u_t	ptbl;	/* page table base address (when PSE is clear) */
		bit14u_t	page;	/* page base address (when PSE is set) */
	} base;

	bool_t 			present;
	bool_t 			read_write; 	/* clear --> read only, 
						 * set   --> can be read and written into */
	bool_t			accessed;
	bool_t			dirty;
	bool_t			page_size;	/* clear --> 4-KByte page 
						 * set   --> 4-MByte page */
};

/* Page-table Entry 
 * [Reference] IA-32 manual. Vol.3 3-24. */
struct ptbl_entry_t {
	bit32u_t		paddr;		/* physical address where this entry is located */
	bit32u_t		val;

	bit20u_t		base;

	bool_t 			present;
	bool_t 			read_write; /* clear --> read only, 
					     * set   --> can be read and written into */
	bool_t			accessed;
	bool_t			dirty;
};

enum page_state {
	PAGE_STATE_INVALID 		= 0,
	PAGE_STATE_READ_ONLY_SHARED 	= 1,
	PAGE_STATE_EXCLUSIVELY_SHARED 	= 2
};
typedef enum page_state		page_state_t;

enum {
	MAX_OF_PDESCR_LADDRS	= 10
};

struct page_descr_t {
	bit32u_t		laddrs[MAX_OF_PDESCR_LADDRS];
	bool_t			read_write[MAX_OF_PDESCR_LADDRS];

	int			num_of_laddrs;
	page_state_t	 	state;
	bit32u_t		copyset;
	bit32u_t		owner;
	
	long long		seq;

	bool_t			requesting;
};

#endif /* _VMM_IA32_PAGING_COMMON_H */
