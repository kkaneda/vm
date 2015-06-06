#include "vmm/mon/mon.h"
#include <signal.h>

/****************************************************************/
/* [DEBUG] */

#define ENABLE_EIP_LOGGING
static bool_t SINGLESTEP_MODE = FALSE;
bool_t get_snapshot_at_next_trap = FALSE;

static void handle_signal ( struct mon_t *mon, int signo );

enum mem_access_emu_kind {
	NO_EMULATION = 0,
	PAGEFAULT_EMULATION = 1,
	SHMEM_EMULATION = 2,
	HDMEM_EMULATION = 3,
	APIC_EMULATION = 4,
};
typedef enum mem_access_emu_kind	mem_access_emu_kind_t;

enum page_fault_kind {
	PAGE_FAULT_KIND_NO_MAPPING = 1,
	PAGE_FAULT_KIND_NO_PAGE = 2,
	PAGE_FAULT_KIND_WRITE_VIOLATION = 3
};
typedef enum page_fault_kind	page_fault_kind_t;

struct segv_info_t {
	mem_access_emu_kind_t 	emu_kind;	
	bit32u_t 		cr2; 
	mem_access_kind_t 	mem_access_kind;
	page_fault_kind_t 	page_fault_kind;
};

/****************************************************************/

inline bool_t
is_native_mode ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	return ( mon->mode == NATIVE_MODE );
}

inline bool_t
is_emulation_mode ( struct mon_t* mon )
{
	ASSERT ( mon != NULL );

	return ( mon->mode == EMULATION_MODE );
}

inline bool_t
is_bootstrap_proc ( struct mon_t* mon )
{
	proc_class_t x;

	ASSERT ( mon != NULL );

	x = ProcClass_of_cpuid ( mon->cpuid );

	return ( x == BOOTSTRAP_PROC );
}

inline bool_t
is_application_proc ( struct mon_t* mon )
{
	proc_class_t x;

	ASSERT ( mon != NULL );

	x = ProcClass_of_cpuid ( mon->cpuid );

	return ( x == APPLICATION_PROC );
}

#ifdef ENABLE_MP

static void
check_stack_permission_for_interrupt ( struct mon_t *mon )
{
	bit32u_t paddr;
	bit32u_t esp;
	bool_t is_ok;

	ASSERT ( mon != NULL );

	if ( cpl_is_user_mode ( mon->regs ) ) {
		struct tss_t tss;
		tss = get_tss_of_current_task ( mon->regs, &Monitor_laddr_to_raddr );

		{ // ????????
			struct descr_t descr;
			struct seg_descr_t tssd;

			descr = Regs_lookup_descr_table ( mon->regs, 
							  &mon->regs->sys.tr.selector, 
							  &Monitor_laddr_to_raddr );
			tssd = Descr_to_task_state_seg_descr ( &descr );
			
			paddr = Monitor_try_vaddr_to_paddr ( SEG_REG_SS, tssd.base, &is_ok );
			if ( ! is_ok ) {
				DPRINT ( "no allocated page\n" );
				return;
			}
			
			emulate_shared_memory_with_vaddr ( mon, MEM_ACCESS_WRITE, paddr, tssd.base );
		}
		esp = tss.esp[SUPERVISOR_MODE];
	} else {
		esp = mon->regs->user.esp;
	}

	DPRINT ( "check stack permission: esp = %#x ( regs->esp = %#x )\n",
		 esp, mon->regs->user.esp );

	paddr = Monitor_try_vaddr_to_paddr ( SEG_REG_SS, esp, &is_ok );
	if ( ! is_ok ) {
		DPRINT ( "no allocated page\n" );
		return;
	}
	
	emulate_shared_memory_with_vaddr ( mon, MEM_ACCESS_WRITE, paddr, esp );
}

static void
check_idt_permission ( struct mon_t *mon, int ivector )
{
	bit32u_t p, paddr;
	
	p = mon->regs->sys.idtr.base + ivector * SIZE_OF_DESCR;

	paddr = Monitor_laddr_to_paddr ( p );
	emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr );
	emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr + 4 );
}

static void
__check_pgtable_permission ( struct mon_t *mon, struct linear_addr_t addr, struct pdir_entry_t pde )
{
	int i;

	ASSERT ( mon != NULL );

	if ( ! pde.present )
		return;

	if ( pde.page_size ) 
		return;

	for ( i = 0; i <= ( NUM_OF_PTBL_ENTRIES * PTBL_ENTRY_SIZE ) >> 12; i++ ) {
		bit32u_t paddr;

		paddr = ( pde.base.ptbl + i ) << 12;		
		emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr );
	}
}

void
check_pgtable_permission ( struct mon_t *mon, bit32u_t cr3 )
{
	struct cr3_t bkup;
	struct linear_addr_t addr;
	int i;

	ASSERT ( mon != NULL );

	bkup = mon->regs->sys.cr3;
	mon->regs->sys.cr3 = Cr3_of_bit32u ( cr3 ); 

	for ( i = 0; i <= ( NUM_OF_PDIR_ENTRIES * PDIR_ENTRY_SIZE ) >> 12; i++ ) {
		bit32u_t paddr;
		
		paddr = ( mon->regs->sys.cr3.base + i ) << 12;		
		emulate_shared_memory_with_paddr ( mon, MEM_ACCESS_READ, paddr );
	}

	addr.offset = 0;
	for ( addr.dir = 0; addr.dir < NUM_OF_PDIR_ENTRIES; addr.dir++ ) {	
		struct pdir_entry_t pde;
		pde = Monitor_lookup_page_directory ( mon->regs, addr.dir );
		__check_pgtable_permission ( mon, addr, pde );
	}
 
	mon->regs->sys.cr3 = bkup;
}

#else /* ! ENABLE_MP */

static void
check_stack_permission_for_interrupt ( struct mon_t *mon )
{ /* do nothing */ }

static void
check_idt_permission ( struct mon_t *mon, int ivector )
{ /* do nothing */ }

void
check_pgtable_permission ( struct mon_t *mon, bit32u_t cr3 )
{ /* do nothing */ }

#endif /* ENABLE_MP */

static void
Monitor_raise_interrupt ( struct mon_t *mon, unsigned int ivector )
{
	struct user_regs_struct saved_uregs;

	ASSERT ( mon != NULL );

	check_stack_permission_for_interrupt ( mon );
	check_idt_permission ( mon, ivector );

	saved_uregs = mon->regs->user; /* [STAT] */

	assert ( mon->regs->user.eip <= VM_PMEM_BASE );

	raise_interrupt ( mon->regs, 
			  ivector, 
			  &Monitor_pushl,
			  &Monitor_laddr_to_raddr );

	/* [STAT] */
	mon->stat.nr_interrupts[ivector]++;
	mon->stat.kernel_state = ivector;
	if ( ivector == IVECTOR_SYSCALL ) {
		save_syscall_state ( &mon->guest_state, mon, &saved_uregs );
	}
}

const char*
VmHandlerKind_to_string ( vm_handler_kind_t kind )
{
	switch ( kind ) {
	case HANDLE_PAGEFAULT:  return "HANDLE_PAGEFAULT";
	case SET_CNTL_REG:      return "SET_CNTL_REG";
	case INVALIDATE_TLB:    return "INVALIDATE_TLB";
	case CHANGE_PAGE_PROT:  return "CHANGE_PAGE_PROT";
	case UNMAP_ALL:         return "UNMAP_ALL";
	case RAISE_INT:         return "RAISE_INT";
	case PRINT_CR2:		return "PRINT_CR2";
	default: 	        Match_failure ( "VmHandlerKind_to_string\n" );
	}
	Match_failure ( "VmHandlerKind_to_string\n" );
	return "";
}

static void
setup_emulation_mode ( struct mon_t *mon, vm_handler_kind_t kind, va_list ap )
{
	ASSERT ( mon != NULL );
	ASSERT ( mon->mode == NATIVE_MODE );

	mon->mode = EMULATION_MODE;

	mon->shi->saved_esp = mon->regs->user.esp;
	mon->regs->user.esp = mon->emu_stack_base;

	mon->shi->kind = kind;

	switch ( kind ) {
	case HANDLE_PAGEFAULT:
		mon->shi->args.cr2 = (bit32u_t) va_arg ( ap, bit32u_t );
		break;
		
	case SET_CNTL_REG: 
		mon->shi->args.set_cntl_reg.index = (int) va_arg ( ap, int );
		mon->shi->args.set_cntl_reg.val = (bit32u_t) va_arg ( ap, bit32u_t );
		break;
		
	case INVALIDATE_TLB:
		break;
		
	case CHANGE_PAGE_PROT:
		mon->shi->args.page_no = (int) va_arg ( ap, int );
		break;

	case UNMAP_ALL:
	case RAISE_INT:
	case PRINT_CR2:
		break;

	default: 
		Match_failure ( "setup_emulation_mode\n" );
	}

	mon->stat.nr_emulation_enter[kind]++;
}

static void
wait_emulation ( struct mon_t *mon, int signo )
{
	ASSERT ( mon != NULL );

	while ( is_emulation_mode ( mon ) ) {
		restart_vm ( mon, signo );
		signo = trap_vm ( mon );

		assert ( mon->regs->user.eip > VM_PMEM_BASE );
		assert ( signo != SIGSEGV );
			
		handle_signal ( mon, signo );
		signo = 0;
	}
}

static
void
__run_emulation_code_of_vm ( struct mon_t *mon, vm_handler_kind_t kind, va_list ap, int signo )
{
	ASSERT ( mon != NULL );

	setup_emulation_mode ( mon, kind, ap );

	DPRINT ( " +---------------------------------+\n" 
		 " | run_emulation_code_of_vm: begin |\n"
		 "   kind=%s\n", 
		 VmHandlerKind_to_string ( kind ) );

	start_time_counter ( &mon->stat.emu_counter );
	start_time_counter ( &mon->stat.emu_counter_sub[kind] );
	
	wait_emulation ( mon, signo );

	stop_time_counter ( &mon->stat.emu_counter_sub[kind] );
	stop_time_counter ( &mon->stat.emu_counter );
     
	DPRINT ( " |                                |\n"
		 " | run_emulation_code_of_vm: end  |\n"
		 " +--------------------------------+\n" );
}

void
run_emulation_code_of_vm ( struct mon_t *mon, vm_handler_kind_t kind, ... )
{
	va_list ap;

	va_start ( ap, kind );
	__run_emulation_code_of_vm ( mon, kind, ap, SIGUSR1 );
	va_end ( ap );
}

void
run_emulation_code_of_vm2 ( struct mon_t *mon, int signo, vm_handler_kind_t kind, ... )
{
	va_list ap;

	va_start ( ap, kind );
	__run_emulation_code_of_vm ( mon, kind, ap, signo );
	va_end ( ap );
}

/****************************************************************/

static bit16u_t saved_seg_reg_ds = 0x20, saved_seg_reg_es = 0x20;
static bit32u_t saved_cr0 = 0x60000011;

static void
get_vm_regs ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	Ptrace_getregs ( mon->pid, &mon->regs->user );

	/* [NOTE]
	 * The flags of EFLGAS register are classified into two categories.
	 * Some flags of the VM's EFLAGS register have the same value
	 * that the real hardware's EFLAGS register.
	 *
	 * The value of the other flags of the VM's EFLAGS register may be different 
	 * from that of the real hardware's EFLAGS register.
	 * The function below updates only the flags of the former case.
	 * The flags of the latter case is not updated. */
	FlagReg_merge1 ( &mon->regs->eflags, mon->regs->user.eflags );
	
	if ( saved_cr0 != mon->regs->sys.cr0.val ) {
		mon->regs->sys.cr0 = Cr0_of_bit32u ( mon->regs->sys.cr0.val );
	}

	/* check whether SEG_REG_DS (or SEG_REG_ES) is modified or not 
	 * and update the caches of the segment register if necessasry */
	if ( saved_seg_reg_ds != mon->regs->segs[SEG_REG_DS].val ) {
		Monitor_set_seg_reg2 ( mon->regs, SEG_REG_DS, mon->regs->segs[SEG_REG_DS].val );
	}

	if ( saved_seg_reg_es != mon->regs->segs[SEG_REG_ES].val ) {
		Monitor_set_seg_reg2 ( mon->regs, SEG_REG_ES, mon->regs->segs[SEG_REG_ES].val );
	}
}

static void
set_vm_regs ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
	
	/* [NOTE] The FlagReg_merge2() is inverse of the FlagReg_merge1() function. */
	mon->regs->user.eflags = FlagReg_merge2 ( &mon->regs->eflags, mon->regs->user.eflags );
	Ptrace_setregs ( mon->pid, &mon->regs->user );

	saved_cr0 = mon->regs->sys.cr0.val;
	
	/* saved the current values of SEG_REG_DS and SEG_REG_ES */
	saved_seg_reg_ds = mon->regs->segs[SEG_REG_DS].val;
	saved_seg_reg_es = mon->regs->segs[SEG_REG_ES].val;
}

static void __restart_vm ( struct mon_t *mon, int signo );

int
trap_vm ( struct mon_t *mon )
{
	int signo;

	ASSERT ( mon != NULL );

	for ( ; ; ) {
		signo = Ptrace_trap ( mon->pid );

		if ( signo != SIGUSR1 ) 
			break;

		/* monitor process が挿入した signal を trap した */
		
		__restart_vm ( mon, signo );
	}

	get_vm_regs ( mon );

	if ( mon->mode == NATIVE_MODE ) {
		Pit_stop_timer ( &mon->devs.pit );
	}

	mon->stat.nr_traps++;	
	if ( cpl_is_supervisor_mode ( mon->regs ) ) {
		mon->stat.nr_traps_at_kernel ++;	
	}

	return signo;
}

static void
__restart_vm ( struct mon_t *mon, int signo )
{
	enum __ptrace_request request = 0;

	switch ( mon->mode ) {
	case NATIVE_MODE:
		request = ( SINGLESTEP_MODE ) ? PTRACE_SINGLESTEP : PTRACE_SYSCALL;
		break;
	case EMULATION_MODE:
		request = PTRACE_CONT; 
		break;
	default:
		Match_failure ( "restart_vm\n" );
	}
	
	Ptrace_restart ( mon->pid, request, signo );	
}

void 
restart_vm ( struct mon_t *mon, int signo )
{
	ASSERT ( mon != NULL );

	set_vm_regs ( mon );

	if ( mon->mode == NATIVE_MODE ) {
		Pit_restart_timer ( &mon->devs.pit ); 
	}

	__restart_vm ( mon, signo );
}

void
restart_vm_with_no_signal ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
	restart_vm ( mon, 0 );
}

/****************************************************************/

static void
emulate_instruction ( struct mon_t *mon, struct instruction_t *i )
{
	ASSERT ( mon != NULL );
	ASSERT ( i != NULL );
	ASSERT ( i->opcode != -1 );
	ASSERT ( i->execute != NULL );

	mon->stat.nr_instr_emu ++;
	mon->stat.nr_emulated_instr [ i->opcode ] ++;
	mon->stat.nr_emulated_instr_name [ i->opcode ] = i->name;

	i->execute ( mon, i );
}

/****************************************************************/

static void
deliver_pagefault_to_guest_process ( struct mon_t *mon, bit32u_t cr2 )
{
	ASSERT ( mon != NULL );

#if 1
	check_stack_permission_for_interrupt ( mon ); 
	check_pgtable_permission ( mon, mon->regs->sys.cr3.val ); // ??? 
#endif

	run_emulation_code_of_vm ( mon, HANDLE_PAGEFAULT, cr2 );
}

static bit32u_t
get_err_code ( struct mon_t *mon, struct segv_info_t *si )
{
	bit32u_t err = 0;

	if ( si->page_fault_kind == PAGE_FAULT_KIND_WRITE_VIOLATION ) 
		SET_BIT ( err, 0 ); /* present or not */

	if ( si->mem_access_kind == MEM_ACCESS_WRITE )
		SET_BIT ( err, 1 ); /* write or not */
		
	if ( cpl_is_user_mode ( mon->regs ) ) 
		SET_BIT ( err, 2 ); 
	
	return err;
}

static void
__raise_pagefault ( struct mon_t *mon, struct segv_info_t *si )
{
	bit32u_t err_code = get_err_code ( mon, si );
	
	mon->regs->sys.cr2 = si->cr2;
	Monitor_raise_interrupt ( mon, IVECTOR_PAGEFAULT );
	Monitor_pushl ( mon->regs, err_code );
}

static void 
raise_pagefault ( struct mon_t *mon, struct segv_info_t *si )
{
	switch ( si->page_fault_kind ) {
	case PAGE_FAULT_KIND_NO_MAPPING:
		deliver_pagefault_to_guest_process ( mon, si->cr2 );
		break;

	case PAGE_FAULT_KIND_NO_PAGE:
	case PAGE_FAULT_KIND_WRITE_VIOLATION: 
		__raise_pagefault ( mon, si );
		break;

	default:
		Match_failure ( "raise_pagefault\n" );
	}
}

/****************************************************************/

enum mem_access_region {
	MEM_ACCESS_TO_HD_RESERVED,
	MEM_ACCESS_TO_RAM,
	MEM_ACCESS_TO_APIC
};
typedef enum mem_access_region		mem_access_region_t;

static bool_t
apic_is_selected ( const struct mon_t *mon, bit32u_t paddr, size_t len )
{
	assert ( mon != NULL );

	return ( ( LocalApic_is_selected ( mon->local_apic, paddr, len ) ) ||
		 ( IoApic_is_selected ( mon->io_apic, paddr, len ) ) );
}

static mem_access_region_t
get_mem_access_region ( const struct mon_t *mon, bit32u_t paddr, size_t len )
{
	ASSERT ( mon != NULL );

	if ( is_hardware_reserved_region ( paddr ) ) 
		return MEM_ACCESS_TO_HD_RESERVED;

	if ( paddr < mon->pmem.ram_offset ) 
		return MEM_ACCESS_TO_RAM;

	if ( apic_is_selected ( mon, paddr, len ) )
		return MEM_ACCESS_TO_APIC;

	ASSERT ( 0 );
	return -1;
}

static bool_t
is_write_privilege_violation ( mem_access_kind_t kind, bool_t read_write )
{
	return ( ( kind == MEM_ACCESS_WRITE ) && ( ! read_write ) );
}

static mem_access_emu_kind_t
to_hd_reserved ( struct mon_t *mon, struct mem_access_t *x, bit32u_t paddr )
{
	bool_t f;

	if ( x->kind == MEM_ACCESS_WRITE )  
		return HDMEM_EMULATION;

	f = emulate_shared_memory_with_paddr ( mon, x->kind, paddr );

	return ( f ) ? SHMEM_EMULATION : NO_EMULATION;
}

static mem_access_emu_kind_t
to_ram ( struct mon_t *mon, struct mem_access_t *x, bit32u_t paddr, bool_t read_write,
	 page_fault_kind_t *page_fault_kind )
{
	int page_no;
	struct page_descr_t *pdescr;
	bit32u_t laddr = Monitor_vaddr_to_laddr ( x->sreg_index, x->vaddr );

	page_no = paddr_to_page_no ( paddr );
	pdescr = & ( mon->page_descrs[page_no] );
	if ( ! PageDescr_has_same_laddr ( pdescr, BIT_ALIGN ( laddr, 12 ), read_write ) ) {
		/* this page has not yet been mapped in the physical machine. */
		*page_fault_kind = PAGE_FAULT_KIND_NO_MAPPING;
		return PAGEFAULT_EMULATION;
	}
		
	bool_t f;
	f = emulate_shared_memory_with_vaddr ( mon, x->kind, paddr, x->vaddr );
	if ( f ) { return SHMEM_EMULATION; }
	
	if ( is_write_privilege_violation ( x->kind, read_write ) ) {
		*page_fault_kind = PAGE_FAULT_KIND_WRITE_VIOLATION;
		return PAGEFAULT_EMULATION;
	}
	
	return NO_EMULATION;
}

static mem_access_emu_kind_t
try_handle_signal_by_mem_access_sub_sub ( struct mon_t *mon, struct mem_access_t *x,
					  page_fault_kind_t *page_fault_kind )
{
	mem_access_emu_kind_t retval = NO_EMULATION;
	bit32u_t paddr;
	bool_t is_ok;
	mem_access_region_t r;
	bool_t read_write;

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );
    
	paddr = Monitor_try_vaddr_to_paddr2 ( x->sreg_index, x->vaddr,
					      &is_ok, &read_write );

	DPRINT ( "check maccess " ); MemAccess_print ( x );     
	DPRINT ( "paddr=%#x, is_ok=%d\n", paddr, is_ok );

	if ( ! is_ok ) {
                /* Deliver this page fault to the guest OS. */
		*page_fault_kind = PAGE_FAULT_KIND_NO_PAGE;
		return PAGEFAULT_EMULATION; 
	}

	r = get_mem_access_region ( mon, paddr, x->len );
	switch  ( r ) {
	case MEM_ACCESS_TO_HD_RESERVED:
		retval = to_hd_reserved ( mon, x, paddr );
		break;

	case MEM_ACCESS_TO_RAM:
    	        /* There exists a correspoing physical memory
		 * region that contains the virtual address to which memory
		 * acncess causes a page fault. */
		retval = to_ram ( mon, x, paddr, read_write, page_fault_kind );
		break;

	case MEM_ACCESS_TO_APIC:
		retval = APIC_EMULATION;
		break;

	default: 
		Monitor_print_detail ( stderr, mon );
		Match_failure ( "try_handle_signal_by_mem_access_sub_sub: eip=%#x, paddr=%#x\n",
				mon->regs->user.eip, paddr );
	}
     
	return retval;     
}

static struct segv_info_t 
try_handle_signal_by_mem_access_sub ( struct mon_t *mon, struct mem_access_t *x )
{
	struct segv_info_t ret;
	size_t len;

	ASSERT ( mon != NULL );
	ASSERT ( x != NULL );

	ret.emu_kind = NO_EMULATION;

	len = x->len;

	while ( len > 0 ) {
		bit32u_t next_vaddr;
		bit32u_t f;

		next_vaddr = BIT_ALIGN ( x->vaddr, 12 ) + PAGE_SIZE_4K;
		x->len = ( ( x->vaddr + x->len > next_vaddr ) ?
			   next_vaddr - x->vaddr :
			   len );

		f = try_handle_signal_by_mem_access_sub_sub ( mon, x, &ret.page_fault_kind );
	
		if ( f != NO_EMULATION ) {
			ret.emu_kind = f;
		}

		if ( f == PAGEFAULT_EMULATION ) {
			ret.cr2 = Monitor_vaddr_to_laddr ( x->sreg_index, x->vaddr );
			ret.mem_access_kind = x->kind;
			break;
		}
		
		x->vaddr = next_vaddr;
		len -= x->len;
	}

	return ret;
}

static struct segv_info_t 
try_handle_signal_by_mem_access ( struct mon_t *mon, struct instruction_t *i )
{
	struct segv_info_t ret;
	struct mem_access_t *p;

	ASSERT ( mon != NULL );
	ASSERT ( i != NULL );
	ASSERT ( i->maccess != NULL );

	ret.emu_kind = NO_EMULATION;

	p = i->maccess ( mon, i );;
	p = MemAccess_add_instruction_fetch ( p, i->len, mon->regs->user.eip ); // いらない？

	while ( p != NULL ) {
		struct mem_access_t *next;
		struct segv_info_t x;

		x = try_handle_signal_by_mem_access_sub ( mon, p );

		if ( x.emu_kind != NO_EMULATION ) 
			ret = x;

		if ( x.emu_kind == PAGEFAULT_EMULATION ) {
			MemAccess_destroy_all ( p );
			break;
		}

		next = p->next;
		MemAccess_destroy ( p );
		p = next;
	}

	return ret;
}

/****************************************************************/

static bool_t
opcode_is_int_syscall ( bit8u_t vals[2] )
{
	return ( ( vals[0] == OPCODE_INT ) && ( vals[1] == IVECTOR_SYSCALL ) );
}

static bool_t
syscall_is_trapped ( struct mon_t *mon )
{
	int i;
	bit8u_t vals[2];
	bool_t flags[2];
	unsigned long int addr;

	ASSERT ( mon != NULL );
	ASSERT ( is_native_mode ( mon ) );

	addr = ( SINGLESTEP_MODE ) ? mon->regs->user.eip : ( mon->regs->user.eip - 2 );
	
	for ( i = 0; i < 2; i++ )
		vals[i] = Monitor_try_read_byte_with_vaddr ( SEG_REG_CS, addr + i, &flags[i] );
	
	return ( ( flags[0] ) && ( flags[1] ) && ( opcode_is_int_syscall ( vals ) ) );
}

/* [Note] The specification of ptrace does not allow to stop issuing a
 * system call with PTRACE_SYSCALL.  For this reason, this function
 * replaces the system call number and makes the physical machine
 * issue some invalid system call. */
static void
ignore_real_syscall ( struct mon_t *mon )
{
	int signo;

	ASSERT ( mon != NULL );

	if ( SINGLESTEP_MODE ) {
		/* When the execution is trapped by PTRACE_SINGLESTEP, 
		 * the trap is done BEFORE the guest process issues a system call. 
		 * As a result, there is no need to issue an illegal system call. */
		mon->regs->user.eip += 2;
		return;
	} 

	/* Copy the value of the orig_eax  ( i.e., syscall no ) to EAX register.
	 * [???] I don't know the details of the specification of the ptrace.
	 *       How do user.eax and user.orig_eax work? */
	mon->regs->user.eax = mon->regs->user.orig_eax;
    
	/* Restart the stopped child process. 
	   The  process issues a dummy system call that only fails. */
	mon->regs->user.orig_eax = INVALID_SYSCALL_NO;
	Ptrace_setregs ( mon->pid, &mon->regs->user );
	Ptrace_syscall ( mon->pid, 0 ); 
	mon->regs->user.orig_eax = mon->regs->user.eax;
     
	/* Trap the exit of the system call. */
	signo = Ptrace_trap ( mon->pid );
	assert ( signo == SIGTRAP ); /* [???] may trap the other kinds of signals? */
}

static int
ivector_of_sigtrap ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
	
	if ( syscall_is_trapped ( mon ) ) {
		ignore_real_syscall ( mon );
	       
		DPRINT ( "issue system call %s (no=%d)\n",
			 Sysno_to_string ( mon->regs->user.eax ),
			 mon->regs->user.eax );

		return IVECTOR_SYSCALL;
	}

	/* [TODO] 
	 * Actually SIGTRAP is invoked by a breakpoint exception as
	 * well as a debug exception.  Thus the above routine is not
	 * sufficient and we have to handle the debug exception.
	 */
	return IVECTOR_BREAKPOINT;
}

static void
handle_sigtrap ( struct mon_t *mon, int signo )
{
	int ivector;
     
	ASSERT ( mon != NULL );

	ivector = ivector_of_sigtrap ( mon );

	/* [DEBUG] */
	if ( ( SINGLESTEP_MODE ) && ( ivector == IVECTOR_BREAKPOINT ) ) {
		/* This SIGTRAP signal is perhaps caused by the single step
		 * execution with the ptrace system call. */
		return;
	}

	Monitor_raise_interrupt ( mon, ivector );
}

/*************************************************/

static void
handle_sigstop ( struct mon_t *mon, int signo )
{
	ASSERT ( mon != NULL );
	ASSERT ( is_emulation_mode ( mon ) );

	DPRINT ( "handle_sigstop\n" );

	mon->mode = NATIVE_MODE;

	/* restore the state of the registers that are stored at <mon->shi->retval>. */
	UserRegs_set_from_sigcontext ( &mon->regs->user, &mon->shi->retval );     
	FlagReg_merge1 ( &mon->regs->eflags, mon->regs->user.eflags );     
}

/*************************************************/

static bool_t
is_undefined_instruction ( bit32u_t eip )
{
	const bit16u_t UD_OPCODE = 0x0b0f;
	bit16u_t val;

	val = Monitor_read_word_with_vaddr ( SEG_REG_CS, eip );
	return ( val == UD_OPCODE );
}

static void 
skip_lock_prefix ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
	mon->regs->user.eip++;
}

static void 
skip_undefined_instruction ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
	DPRINT ( "skip undefined instruction\n" );
	mon->regs->user.eip += 2;
}

/* return TRUE only if the instruction fetch causes a page fault */
static bool_t
check_instr_fetch ( struct mon_t *mon, struct instruction_t *i, bit32u_t saved_eip )
{
	struct segv_info_t si;

	/* fetch pages if necessasry */
	si = try_handle_signal_by_mem_access ( mon, i );

	if ( si.emu_kind != PAGEFAULT_EMULATION ) {
		return FALSE; 
	}

	mon->regs->user.eip = saved_eip; /* roll back the EIP register */
	raise_pagefault ( mon, &si );

	return TRUE;
}

static void
handle_sigill ( struct mon_t *mon, int signo )
{
	struct instruction_t i;     
	bit32u_t saved_eip;
	bool_t b;

	ASSERT ( mon != NULL );
	if ( ! is_undefined_instruction ( mon->regs->user.eip ) ) {
		Monitor_print_detail ( stderr, mon );
	}
	ASSERT ( is_undefined_instruction ( mon->regs->user.eip ) );
     
	saved_eip = mon->regs->user.eip;
	skip_undefined_instruction ( mon );
	i = decode_instruction ( mon->regs->user.eip );

	ASSERT ( ( i.is_sensitive ) || ( i.is_locked ) );

	b = check_instr_fetch ( mon, &i, saved_eip );
	if ( b ) { return; }

	if ( i.is_locked ) {
		/* [TODO] xchg instruction 実行時の処理 */
		sync_shared_memory ( mon, &i );
		skip_lock_prefix ( mon );
	} else {
		emulate_instruction ( mon, &i );
	}
}

/*************************************************/

/*
 *[TODO]
 * page fault の発生した原因を調べている最中に
 * page fault が発生することも考慮しなければいけない．
 */
static bool_t
try_handle_signal_by_instruction_fetch ( struct mon_t *mon )
{
	bool_t ret = FALSE;
	struct mem_access_t *p;
	const size_t MAX_INSTR_LEN = 12;
	struct segv_info_t si;

	p = MemAccess_add_instruction_fetch ( NULL, MAX_INSTR_LEN, mon->regs->user.eip );
	si = try_handle_signal_by_mem_access_sub ( mon, p );
	MemAccess_destroy ( p );
	
	switch ( si.emu_kind ) {
	case PAGEFAULT_EMULATION:
		raise_pagefault ( mon, &si );
		ret = TRUE;
		break;
	
	case SHMEM_EMULATION:
		ret = TRUE;
		break;
		
	default:
		break;
	}

	return ret;
}

static void
__handle_other_signals ( struct mon_t *mon, int signo )
{
	struct instruction_t i;
	struct segv_info_t si;

	i = decode_instruction ( mon->regs->user.eip );

	assert ( i.opcode != INVALID_OPCODE );

	si = try_handle_signal_by_mem_access ( mon, &i );

	switch  ( si.emu_kind ) {
	case NO_EMULATION:
		if ( i.is_sensitive ) {
                        /* A sensitive instruction inserted by a user program is executed. */
			emulate_instruction ( mon, &i );
		} else {
			/* An exception other than page-fault exceptions happend. */
#if 1 
			Print ( stderr, "signo = %s\n", Signo_to_string ( signo ) ); /* [DEBUG] */
			Instruction_print ( stderr, &i );
			exit ( 1 ); 
#endif 
			run_emulation_code_of_vm2 ( mon, signo, RAISE_INT );
		}
		break;

	case PAGEFAULT_EMULATION: 
		raise_pagefault ( mon, &si );
		break;

	case SHMEM_EMULATION:
		/* necessary emulation procedure has been done. */
		break;

	case HDMEM_EMULATION:
	case APIC_EMULATION:
		emulate_instruction ( mon, &i );
		break;

	default:
		Match_failure ( "__handle_other_signals\n" );
	}
}

static void
handle_other_signals ( struct mon_t *mon, int signo )
{
	bool_t b;

	ASSERT ( mon != NULL );
	ASSERT ( is_native_mode ( mon ) );

	mon->stat.nr_other_sigs ++; /* [STAT] */

#if 0 
	// Is this check really necessary?
	check_pgtable_permission ( mon, mon->regs->sys.cr3.val ); 
#endif

	b = try_handle_signal_by_instruction_fetch ( mon );
	if ( b ) { return; }

	/* The signal is not caused by instruction fetch, and 
	 * necessary emulation procedure has not yet been done. */
	return __handle_other_signals ( mon, signo );
}

/*************************************************/

static void
do_nothing ( struct mon_t *mon, int signo ) { }

/*************************************************/

/* 
 * [Note] 
 * The following is the list of signals that the exception handler raises. 
 * SIGFPE  <= Divide error  ( 0 ), 
 *            Coprocessor segment overrun  ( 9 ), 
 *            Floating point error  ( 16 ), 
 *            SIMD floating point  ( 19 )
 * SIGTRAP <= Debug  ( 1 ), 
 *            Breakpoint  ( 3 )
 * SIGSEGV <= Overflow  ( 4 ), 
 *            Bounds check  ( 5 ), 
 *            Device not available  ( 7 ), 
 *            Double fault  ( 8 ), 
 *            Invalid TSS  ( 10 ), 
 *            General protection  ( 13 ), 
 *            Page Fault  ( 14 )
 * SIGILL  <= Invalid opcode  ( 6 )
 * SIGBUS  <= Segment not present  ( 11 ), 
 *            Stack exception  ( 12 ), 
 *            Alignment check  ( 17 )
 * none    <= NMI  ( 2 ), 
 *            Machine check  ( 18 )
 * [Reference] O'REILLY p.132 
 */

typedef void mon_sig_handler_t ( struct mon_t *, int );

struct mon_sig_handler_entry_t {
	int			signo;
	mon_sig_handler_t	*handler;
};

static struct mon_sig_handler_entry_t sig_handler_map[] =
{ { SIGSTOP, 	&handle_sigstop },
  { SIGTRAP, 	&handle_sigtrap },
  { SIGILL, 	&handle_sigill },
  { SIGALRM,	&do_nothing },
  { SIGWINCH,	&do_nothing },
  { SIGUSR1,	&do_nothing },
  { SIGUSR2,	&do_nothing },
  { SIGSEGV,	&handle_other_signals },
  { SIGFPE,	&handle_other_signals },
  { SIGBUS,	&handle_other_signals }
};

static size_t
nr_sig_handler_entries ( void )
{
	return sizeof ( sig_handler_map ) / sizeof ( struct mon_sig_handler_entry_t ); 
}

static mon_sig_handler_t *
get_mon_sig_handler ( int signo )
{
	int i;

	for ( i = 0; i < nr_sig_handler_entries ( ); i++ ) {
		struct mon_sig_handler_entry_t *x = &sig_handler_map[i];
	 
		if ( x->signo == signo ) {
			return x->handler;
		}
	}

	Match_failure ( "handle_signal: signo=%d\n", signo ); 
	return NULL;
}

static void
handle_signal ( struct mon_t *mon, int signo )
{
	mon_sig_handler_t *f;

	f = get_mon_sig_handler ( signo );
	( *f ) ( mon, signo );
}

static void
handle_signal_with_stat ( struct mon_t *mon, int signo )
{
	start_time_counter ( &mon->stat.shandler_counter );
	handle_signal ( mon, signo );
	stop_time_counter ( &mon->stat.shandler_counter );
}

/****************************************************************/

static int
__try_generate_interrupt ( struct mon_t *mon )
{
	int ret, irq;

	ASSERT ( mon != NULL );

	irq = try_generate_external_irq ( mon, FALSE );
	if ( irq != IRQ_INVALID ) { 
		bool_t f;
		f = Pic_trigger ( &mon->devs.pic, irq );  /* PIC mode */
		if ( ! f ) {
			IoApic_trigger ( mon->io_apic, irq ); /* APIC mode */
		}
	}

	ret = Pic_try_acknowledge_interrupt ( &mon->devs.pic );
	if ( ret < 0 ) {
		ret = LocalApic_try_acknowledge_interrupt ( mon->local_apic );
	}
	return ret;
}

static void
try_generate_interrupt ( struct mon_t *mon )
{
	int ivec;

	ASSERT ( mon != NULL );
	ASSERT ( is_native_mode ( mon ) );

	if ( interrupt_is_disabled ( mon->regs ) ) {
		return;
	}

	ivec = __try_generate_interrupt ( mon );

	if ( ivec >= 0 ) {
		Monitor_raise_interrupt ( mon, ivec );
	}
}

static void
try_generate_interrupt_with_stat ( struct mon_t *mon )
{
	start_time_counter ( &mon->stat.ihandler_counter );
	try_generate_interrupt ( mon );		
	stop_time_counter ( &mon->stat.ihandler_counter );
}

/****************************************************************/

void 
Monitor_finalize ( int status, void *arg )
{
	struct mon_t *mon = (struct mon_t *) arg;

	Vga_destroy ( &mon->devs.vga );

#if 0
	Stat_print ( stdout, &mon->stat );
	Stat_print ( stderr, &mon->stat );
#endif

	Print_color ( stdout, RED, "CPU %d quitted\n", mon->cpuid );
	Print_color ( stderr, RED, "CPU %d quitted\n", mon->cpuid );

	kill ( mon->pid, SIGKILL );
	exit ( status );
}

#ifdef ENABLE_EIP_LOGGING

static void
log_eip ( struct mon_t *mon )
{
	static FILE *fp = NULL;

	if ( fp == NULL ) {
		fp = Fopen_fmt ( "w+", "/tmp/eip%d", mon->cpuid );
	}
	
	Print ( fp, "%#lx\n", mon->regs->user.eip );
}

#else /* ! ENABLE_EIP_LOGGING */

static void
log_eip ( struct mon_t *mon )
{
}

#endif /* ENABLE_EIP_LOGGING */


// #define ENABLE_AFFINITY

#ifdef ENABLE_AFFINITY
#include <sched.h>

static void
// Sched_setaffinity ( pid_t pid, unsigned int len, cpu_set_t *mask )
Sched_setaffinity ( pid_t pid, unsigned int len, int *mask )
{
	int ret;

 	ret = sched_setaffinity ( pid, len, mask );
	
	if ( ret == -1 ) {
		Sys_failure ( "sched_setaffinity" );
	}
}
#endif /* ENABLE_AFFINITY */

static void
INIT_FOR_DEBUG ( struct mon_t *mon )
{
#ifdef ENABLE_AFFINITY

#if 1
	cpu_set_t mask;
	const unsigned long LEN = sizeof ( mask );

	__CPU_ZERO ( &mask );
	__CPU_SET ( ( mon->cpuid * 2 ), &mask );

	/* bind individual virtual CPUs to different physical CPUs */

	Sched_setaffinity ( 0, LEN, &mask );
	Sched_setaffinity ( mon->pid, LEN, &mask );
#else
	int mask = 1;
	Sched_setaffinity ( mon->pid, LEN, &mask );
#endif

#endif /* ENABLE_AFFINITY */
}

static void
raise_device_not_available_exception_if_ts_set ( struct mon_t *mon, privilege_level_t saved_cpl ) 
{
	privilege_level_t curr_cpl = cpl ( mon->regs );

	/* [TODO] consider EM flag of cr0 (IA-32 vol3A. 2-19) */
	if ( ( saved_cpl == SUPERVISOR_MODE ) && ( curr_cpl == USER_MODE ) && 
	     ( mon->regs->sys.cr0.task_switched ) ) {
		/* raise device-not-available exception (#NM) for restoring fpu */
		Monitor_raise_interrupt ( mon, IVECTOR_DEVICE_NOT_AVAILABLE ); 
	}
}

static void
____monitor_run ( struct mon_t *mon, int signo ) 
{
	privilege_level_t saved_cpl;

	ASSERT ( mon != NULL );

	saved_cpl = cpl ( mon->regs );

	handle_signal_with_stat ( mon, signo );
	try_handle_msgs_with_stat ( mon );
	try_generate_interrupt_with_stat ( mon );

	/* for restoring the state of FPU */
	raise_device_not_available_exception_if_ts_set ( mon, saved_cpl );
}

static void
check_migration ( struct mon_t *mon ) 
{
	if ( ! get_snapshot_at_next_trap ) {
		return;
	}

#ifdef ENABLE_MP
	struct node_t dest;
	char *new_config = "/home/users/kaneda/vm/vmm/mon/config-mejiro-m.txt"; // [TEST]
	
	dest.hostname = "localhost";
	Monitor_migrate ( mon, dest, new_config );
#else /* ! ENABLE_MP */
	Monitor_create_snapshot ( mon );
#endif /* ENABLE_MP */
	get_snapshot_at_next_trap = FALSE;
}

static void
__monitor_run_stat_pre ( struct mon_t *mon, bool_t is_kernel_mode )
{
	/* [STAT] */
	stop_time_counter ( &mon->stat.guest_counter );
	stop_time_counter ( ( is_kernel_mode ) ? ( &mon->stat.guest_kernel_counter ) : ( &mon->stat.guest_user_counter ) );
	start_time_counter ( &mon->stat.vmm_counter );

	if ( is_kernel_mode ) {
		stop_time_counter ( &mon->stat.interrupt_counters [mon->stat.kernel_state] );
	}

	log_eip ( mon ); /* [DEBUG] */
}

static void
__monitor_run_stat_post ( struct mon_t *mon, bool_t is_kernel_mode )
{
	/* [STAT] */
	start_time_counter ( ( is_kernel_mode ) ? ( &mon->stat.guest_kernel_counter ) : ( &mon->stat.guest_user_counter ) );
	start_time_counter ( &mon->stat.guest_counter );
	stop_time_counter ( &mon->stat.vmm_counter );

	if ( is_kernel_mode ) {
		start_time_counter ( &mon->stat.interrupt_counters [mon->stat.kernel_state ] );
	} else {
		mon->stat.kernel_state = -1;
	}
}

static void
__monitor_run ( struct mon_t *mon )
{
	int signo;
	static bool_t is_kernel_mode = FALSE;
	struct user_i387_struct fp_regs;

	{ 
		signo = trap_vm ( mon );
		ASSERT ( mon->regs->user.eip <= VM_PMEM_BASE );
		
		__monitor_run_stat_pre ( mon, is_kernel_mode ); /* [STAT] */
		
		Ptrace_getfpregs ( mon->pid, &fp_regs );
	}

	____monitor_run ( mon, signo );

	{
		Ptrace_setfpregs ( mon->pid, &fp_regs );
		
		/* [STAT] */
		is_kernel_mode = cpl_is_supervisor_mode ( mon->regs );
		__monitor_run_stat_post ( mon, is_kernel_mode );
		
		restart_vm_with_no_signal ( mon );
	}
}

void
Monitor_run ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	INIT_FOR_DEBUG ( mon );

	if ( is_application_proc ( mon ) ) {
		wait_for_sipi ( mon );
	}

	start_time_counter ( &mon->stat.guest_counter );
	start_time_counter ( &mon->stat.guest_kernel_counter );
	mon->stat.exec_start_time = Timespec_current ();

	for ( ; ; ) {
		__monitor_run ( mon );
	}     
}

int
main ( int argc, char *argv[], char *envp[] )
{
	struct mon_t *mon;
	struct config_t *config;

	config = Config_create ( argc, argv );
	mon = Monitor_create ( config );
	Monitor_run ( mon );

	return 0;
}
