#include "vmm/vm/vm.h"

struct vm_args_t {
	int    cpuid;
	bool_t resume;
};

static struct vm_args_t
parse_args ( int argc, char *argv[] )
{
	struct vm_args_t x;

	ASSERT ( argv != NULL );

	if ( ( argc != 2 ) && ( argc != 3 ) ) {
		Print ( stderr, "Usage : %s <cpuid> [-resume]\n", argv[0] );
		exit ( 1 );
	}

	x.cpuid = Atoi ( argv[1] );
	x.resume = ( argc == 3 );

	return x;
}

static void
set_sighandlers ( void )
{
	int i;
	struct sigaction act;
	int sigs[] = { SIGUSR1, SIGSEGV, SIGALRM, SIGTRAP, SIGFPE, SIGILL, -1 };

	Sigemptyset ( &act.sa_mask );
	act.sa_handler = ( sigfunc_t * )&handle_signal;
	act.sa_flags = SA_NODEFER;
	
	for ( i = 0; sigs[i] != -1; i++ ) {
		Sigaction ( sigs[i], &act, NULL );
	}
}

struct vm_t *
Vm_init ( int argc, char *argv[] )
{
	struct vm_t * vm;
	struct vm_args_t va;

	ASSERT ( argv != NULL );

	va = parse_args ( argc, argv );

#ifndef ENABLE_MP
	ASSERT ( ProcClass_of_cpuid ( va.cpuid ) == BOOTSTRAP_PROC );
#endif /* !ENABLE_MP */

	Vm_init_mem ( va.cpuid );
	set_sighandlers ( );

	vm = Vm_get ( );
	if ( ! va.resume ) {
		Vm_init_page_mapping ( vm );
	}

	return vm;
}
