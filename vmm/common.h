#ifndef _VMM_COMMON_H
#define _VMM_COMMON_H

#include "vmm/std.h"
#include "vmm/ia32.h"
#include "vmm/comm.h"

/* 
   memory layout ( virtual address space of the guest process)

  +----------------------+ 0x0000000
  |                      |
  | User process         |
  |                      |
  |         ....         |
  |                      |
  +----------------------+ 0x90000000 (= GUEST_KERNEL_BASE)
  | Guest OS kernel      |
  +----------------------+ 0xa8000000 (= VM_PMEM_BASE)
  | Guest Phy. memory    |  ( SIZE: 256MB )
  +----------------------+ 0xb8000000 (= WORKSPACE_BASE)
  | workspace for emu    |
  +----------------------+ 0xc0000000 (= HOST_KERNEL_BASE)
  |                      |
  | Host OS kernel       |
  |                      |
  +----------------------+ 0xffffffff 


  the internal structure of workspace for emulation
   
  +----------------------+ 0xb8000000
  | struct vm_t          |
  +----------------------+
  | (reserverd)          |
  +----------------------+ 0xb8001000
  | struct regs_t        | 
  +----------------------+
  | struct shared_info_t |
  +----------------------+
  | struct page_descr_t  |
  +----------------------+
  | struct page_descr_t  |
  +----------------------+
  | (reserverd)          |
  +----------------------+ 0xb8400000
  | emulation code       |
  |                      |
  |                      |
  |         ....         |
  |                      |
  +----------------------+ 
  | stack                |
  |                      |
  |                      |
  |                      |
  +----------------------+ EMU_STACK_BASE
  |                      |
  +----------------------+ 0xc0000000 
*/


/* [DEBUG] */
#define TMP_DIRNAME	"/tmp/vm_"
#define PMEM_FILENAME	"mem"
#define REGS_FILENAME	"regs"

#define get_private_dirname( s, n )  Snprintf (s, n, "%s%s", TMP_DIRNAME, Getenv ( "USER" ) )

#define open_private_file( cpuid, filename, flags ) \
               Open_fmt ( flags, "%s%s/%s%d", TMP_DIRNAME, Getenv ( "USER" ), filename, cpuid )

#define create_private_file( cpuid, filename ) \
               Creat_fmt ( S_IRUSR | S_IWUSR, "%s%s/%s%d", TMP_DIRNAME, Getenv ( "USER" ), filename, cpuid )

enum {
	GUEST_KERNEL_BASE 	= 0x90000000,
	VM_PMEM_BASE		= 0xa8000000,
	WORKSPACE_BASE		= 0xb8000000,

//	PMEM_SIZE		= 0x10000000, // 2^{28} = 256 MB
//	PMEM_SIZE		= 0x0c000000, // 192 MB
	PMEM_SIZE		= 0x08000000, // 2^{27} = 128 MB
//	PMEM_SIZE		= 0x02000000, // 2^{25} =  32 MB

	MAX_PMEM_SIZE		= 0x10000000, 
	MAX_SIZE_OF_WORKSPACE 	= 0x00700000 /* [Caution] If you change this value,
					 * you need to change linker_script. */
};


struct pmem_t {
     int		fd;
     bit32u_t		base;
     bit32u_t		offset;
     bit32u_t		ram_offset;
};

enum vm_handler_kind {
     HANDLE_PAGEFAULT	= 0,
     SET_CNTL_REG	= 1,
     INVALIDATE_TLB	= 2,
     CHANGE_PAGE_PROT	= 3,
     UNMAP_ALL		= 4,
     RAISE_INT		= 5,
     PRINT_CR2		= 6
};
typedef enum vm_handler_kind 	vm_handler_kind_t;

enum {
	NR_HANDLER_KINDS	= 5
};

struct set_cntl_reg_t {
     int		index;
     bit32u_t		val;
};

/* [TODO] 変数名とかがいい加減なので整理する */
struct shared_info_t {
     vm_handler_kind_t	kind;
     bit32u_t		saved_esp;

     union {
	     struct set_cntl_reg_t 	set_cntl_reg; /* for SET_CNTL_REG */
	     int			page_no; /* for CHANGE_PAGE_PROT */
	     bit32u_t			cr2; /* for HANDLE_PAGEFAULT */
     } args;

     struct sigcontext retval;
};

#endif /* _VMM_COMMON_H */
