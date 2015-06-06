#include "vmm/vm/vm.h"
#include <asm/ucontext.h>
#include <sys/mman.h>

/* [NOTE]
 * The workspace must be initialized by the workspace_init() function
 * before this function is called. */
inline struct vm_t *
Vm_get ( void )
{
	return ( struct vm_t * )WORKSPACE_BASE;
}

inline bool_t
is_bootstrap_proc ( struct vm_t* vm )
{
	proc_class_t x;   

	ASSERT ( vm != NULL );

	x = ProcClass_of_cpuid ( vm->cpuid );

	return ( x == BOOTSTRAP_PROC );
}

inline bool_t
is_application_proc ( struct vm_t* vm )
{
	proc_class_t x;   

	ASSERT ( vm != NULL );

	x = ProcClass_of_cpuid ( vm->cpuid );

	return ( x == APPLICATION_PROC );
}

/*************************************************************************/

static void
Vm_raise_interrupt ( struct vm_t *vm, unsigned int ivector )
{
	ASSERT ( vm != NULL );

	Print ( stderr, "Vm_raise_interrupt: ivector=%#x\n", ivector );
	Print ( stdout, "Vm_raise_interrupt: ivector=%#x\n", ivector );

	raise_interrupt ( vm->regs, ivector, &Vm_pushl, NULL );
}

/*************************************************************************/

static void
emulate_pagefault ( struct vm_t *vm, bit32u_t cr2 )
{
	bit32u_t laddr, paddr;
	bool_t is_ok, read_write;
	bool_t b;

	ASSERT ( vm != NULL );

	/* [Note] cr2 register contains the linear address where the page fault occures 
	 * [Reference] O'REILLY pp.316 */ 
	laddr = BIT_ALIGN ( cr2, 12 ); 
	paddr = Vm_try_laddr_to_paddr2 ( laddr, &is_ok, &read_write );

	assert ( is_ok );
	ASSERT ( paddr < vm->pmem.ram_offset );

	/* There exists a correspoing physical memory
	 * region that contains the virtual address of which memory
	 * access causes a page fault. */

	b = try_add_new_mem_map ( vm, laddr, paddr, read_write );
	assert ( b );
}

static void
set_cntl_reg ( struct vm_t *vm, int sig )
{
	int index;
	bit32u_t val;

	ASSERT ( vm != NULL );
	ASSERT ( sig == SIGUSR1 );

	index = vm->shi->args.set_cntl_reg.index;
	val = vm->shi->args.set_cntl_reg.val;

	DPRINT ( " ( vm )\t" "set_cntl_reg: index=%d, val=%#x\n", index, val );

	switch ( index ) {
	case 0: set_cr0 ( vm, val ); break;
	case 3: set_cr3 ( vm, val ); break;
	case 4: set_cr4 ( vm, val ); break;
	default: Match_failure ( "set_cntl_reg\n" );
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
start_emulation ( struct vm_t *vm, int sig, struct sigcontext *sc )
{
	ASSERT ( vm != NULL );
	ASSERT ( sc != NULL );

	/* restore the esp register */
	sc->esp = vm->shi->saved_esp;
		
	switch ( vm->shi->kind ) {
	case HANDLE_PAGEFAULT: 	emulate_pagefault ( vm, vm->shi->args.cr2 );break;
	case SET_CNTL_REG:      set_cntl_reg ( vm, sig ); break;
	case INVALIDATE_TLB:    invalidate_tlb ( vm ); break;
	case CHANGE_PAGE_PROT:  change_page_prot ( vm, vm->shi->args.page_no ); break;
	case UNMAP_ALL:		Vm_unmap_all ( vm ); break;
	case RAISE_INT:		Vm_raise_interrupt ( vm, sc->trapno ); break;
	case PRINT_CR2:		fprintf ( stderr, "cr2 = %#x\n", sc->cr2 ); break;
	default:	        Match_failure ( "handle_signal\n" );
	};
}

static void
quit_emulation ( struct vm_t *vm, struct sigcontext *sc )
{
	ASSERT ( vm != NULL );
	ASSERT ( sc != NULL );

	vm->shi->retval = *sc;
	Raise ( SIGSTOP );
	assert ( 0 );
}

/* 
 * The entry point for the hardware emulation. 
 * This function is called whenever a signal is delivered to this process. 
 */
void
handle_signal ( int sig, struct sigcontext x )
{
	struct vm_t *vm = Vm_get ( );
	struct sigcontext sc = x;

	ASSERT ( vm != NULL );

	start_emulation ( vm, sig, &sc );
	quit_emulation ( vm, &sc );
	assert ( 0 ); 
}

/*************************************************************************/

int
main ( int argc, char *argv[], char *envp[] )
{
	struct vm_t *vm;

	vm = Vm_init ( argc, argv );

	/* Give the control to the monitor process */
	Ptrace_traceme ( );
	Raise ( SIGSTOP );

	/* [Note] The BSP never reaches here */
	assert ( is_application_proc ( vm ) );

	sleep_forever ( );

	return 0;
}
