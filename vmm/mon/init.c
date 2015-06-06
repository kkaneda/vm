#include "vmm/mon/mon.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* [kernel 変更時に、手動で行わなければいけない作業]

   1) GUEST_OS_KERNEL_VERSION を設定する。

   2) INITIAL_AP_GDTR の値を設定する。
      protest_cntlr.c の lgdt_or_lidt_ms () 中で gdtr の値を取得し、
     起動してから最初に設定された値を INITIAL_AP_GDTR に指定する。

   3) application processor の EIP register の初期値を設定する。
      ラベル startup_32_smp を指すようにする。*/


// #define GUEST_OS_KERNEL_VERSION 0x02060f
#define GUEST_OS_KERNEL_VERSION 0x020608

/* [TODO] Make the following values can be configured by a user. */
#if GUEST_OS_KERNEL_VERSION == 0x020419 /* 2.4.25 */
# define INITIAL_BP_GDTR	( ( 0x00090bb1LL << 16 ) | 0x8000LL ) 
# define INITIAL_AP_GDTR	( ( 0x0032b160LL << 16 ) | 0x045fLL )
#endif

#if GUEST_OS_KERNEL_VERSION == 0x020608 /* 2.6.8 */
// # define INITIAL_BP_GDTR	( ( 0x00090af0LL << 16 ) | 0x001fLL ) 
// # define INITIAL_AP_GDTR	( ( 0x0024a100LL << 16 ) | 0x001fLL )

# define INITIAL_BP_GDTR	( ( 0x00090ac0LL << 16 ) | 0x001fLL ) 
# define INITIAL_AP_GDTR	( ( 0x00278100LL << 16 ) | 0x001fLL )
#endif

#if GUEST_OS_KERNEL_VERSION == 0x02060f /* 2.6.15 */
# define INITIAL_BP_GDTR	( ( 0x000913a0LL << 16 ) | 0x001fLL ) 
# define INITIAL_AP_GDTR	( ( 0x0027f100LL << 16 ) | 0x001fLL )

// # define INITIAL_BP_GDTR	( ( 0x000913c0LL << 16 ) | 0x001fLL ) 
// # define INITIAL_AP_GDTR	( ( 0x002d6100LL << 16 ) | 0x001fLL )
#endif

static void
try_make_private_dir ( void )
{
	int err;
	enum { BUFSIZE = 1024 };
	char s[BUFSIZE];

	get_private_dirname ( s, BUFSIZE );

	err = mkdir ( s, S_IRWXU );

	if ( ( err == 1 ) && ( err != EEXIST ) )
		Sys_failure ( "mkdir" );
}

static void
copy_mem_image ( int src_fd, int dest_fd ) 
{
	enum { BUFSIZE = 1024 };
	char s[BUFSIZE];

	for ( ; ; ) {
		int n;
		
		n = Read ( src_fd, s, BUFSIZE );
		if ( n <= 0 ) 
			break;
		
		Writen ( dest_fd, s, n );
	}	
}

static void
init_mem_file ( const struct config_t *config )
{
	int src_fd, dest_fd; 

	ASSERT ( config != NULL );

	src_fd = Open ( config->memory, O_RDONLY );
	dest_fd = create_private_file ( config->cpuid, PMEM_FILENAME );

	copy_mem_image ( src_fd, dest_fd );

	Close ( src_fd );
	Close ( dest_fd );
}

static void
init_reg_file ( const struct config_t *config )
{
	int fd;

	ASSERT ( config != NULL );

	fd = create_private_file ( config->cpuid, REGS_FILENAME );
	Ftruncate ( fd, PAGE_SIZE );
	Close ( fd );
}

static void
init_private_files ( const struct config_t *config )
{
	ASSERT ( config != NULL );

	try_make_private_dir ( );
	
	init_mem_file ( config );
	init_reg_file ( config );
}

static pid_t
fork_vm ( const struct config_t *config )
{
	char *VM_EXE_NAME = "vm";
	char *RESUME = "-resume";
	enum { BUFSIZE = 128 };
	char exe[BUFSIZE], cpuid[BUFSIZE];
	char *argv [] = { exe, cpuid, NULL, NULL };
	pid_t pid;
	int err;

	ASSERT ( config != NULL );

	pid = Fork (  );
	if ( pid != 0 ) {
		int signo;
	
		signo = Ptrace_trap ( pid );
		assert ( signo == SIGSTOP );

		return pid;
	}

	Snprintf ( exe, BUFSIZE, "%s/%s", config->dirname, VM_EXE_NAME );
	Snprintf ( cpuid, BUFSIZE, "%d", config->cpuid );
	if ( config->snapshot != NULL ) {
		argv[2] = RESUME;
	}


	err = execvp ( argv[0], argv );
	if ( err == -1 ) {
		Sys_failure ( "execvp" );
	}
	assert ( 0 );

	return 0;
}

static void
init_regs_of_bsp ( struct regs_t *regs )
{
	ASSERT ( regs != NULL );

	regs->sys.gdtr = GlobalSegReg_of_bit48u ( INITIAL_BP_GDTR );
	regs->sys.idtr = GlobalSegReg_of_bit48u ( 0x000000000LL );

	Monitor_set_seg_reg2 ( regs, SEG_REG_LDTR, 0x0 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_TR, 0x0 );

	Monitor_set_seg_reg2 ( regs, SEG_REG_ES, 0x20 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_CS, 0x18 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_SS, 0x20 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_DS, 0x20 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_FS, 0x20 );
	Monitor_set_seg_reg2 ( regs, SEG_REG_GS, 0x20 );

	regs->eflags = FlagReg_of_bit32u ( 0x86 );

	regs->sys.cr0 = Cr0_of_bit32u ( 0x60000011 );
	regs->sys.cr1 = 0;
	regs->sys.cr2 = 0;
	regs->sys.cr3 = Cr3_of_bit32u ( 0 );
	regs->sys.cr4 = Cr4_of_bit32u ( 0 );

	regs->user.eip = 0x00100000;
	regs->user.esp = 0x7efe;

	/* [???] */
	regs->user.eax = 0x7efe;
	regs->user.ebx = 0x00100000;
	regs->user.esi = 0x90000;
	regs->user.edi = 0x00000003;
	regs->user.ebp = 0xbffff8e8;
}

static void
init_regs_of_ap ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	/* [Note] 
	 * vectorで指定されたアドレス  ( trampoline code )にjumpするのが
	 * 本当は正しい hardware の emulation だが，
	 * それにはリアルモードのエミュレーションを必要とする．
	 * そこで、最初から 0x100000 にジャンプするようにしている．
	 */

#if 0
	mon->regs->user.eip = mon->apic->startup_info.vector << 12;
	/* ... */

#else
	
	/* entry point of startup_32_smp (according to arch/i386/kernel/trampoline.S) */
#if GUEST_OS_KERNEL_VERSION == 0x020419 /* 2.4.25 */
	mon->regs->user.eip = 0x100074;
#endif
#if GUEST_OS_KERNEL_VERSION == 0x020608 /* 2.6.8 */
	mon->regs->user.eip = 0x100088;
#endif
#if GUEST_OS_KERNEL_VERSION == 0x02060f /* 2.6.15 */
	mon->regs->user.eip = 0x100100;
#endif

	// [TODO] linux の memory imageが変わるたびに変更が必要．
	mon->regs->sys.gdtr = GlobalSegReg_of_bit48u ( INITIAL_AP_GDTR ); 
	mon->regs->sys.idtr = GlobalSegReg_of_bit48u ( 0x0000000LL );

	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_LDTR, 0x0 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_TR, 0x0 );

	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_ES, 0x20 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_CS, 0x10 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_SS, 0x20 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_DS, 0x20 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_FS, 0x20 );
	Monitor_set_seg_reg2 ( mon->regs, SEG_REG_GS, 0x20 );

	mon->regs->eflags = FlagReg_of_bit32u ( 0x2 );

	mon->regs->sys.cr0 = Cr0_of_bit32u ( 0x60000010 );
	mon->regs->sys.cr1 = 0;
	mon->regs->sys.cr2 = 0;
	mon->regs->sys.cr3 = Cr3_of_bit32u ( 0 );
	mon->regs->sys.cr4 = Cr4_of_bit32u ( 0 );

//	mon->regs->user.esp = 0x7efe;
	mon->regs->user.esp = 0x0; // ?? 
#endif
}

#ifdef ENABLE_MP

static void
send_mem_image_request ( struct mon_t *mon )
{
	struct msg_t *msg;
	
	ASSERT ( mon != NULL );
	
	msg = Msg_create ( MSG_KIND_MEM_IMAGE_REQUEST, 0, NULL );
	Comm_send ( mon->comm, msg, BSP_CPUID );
	Msg_destroy ( msg );	
}

static void
recv_mem_image_response_sub ( struct mon_t *mon, struct msg_t *msg, int *offset_p )
{
	int offset = *offset_p;

	ASSERT ( mon != NULL );
	ASSERT ( msg != NULL );	

	if ( msg->hdr.kind != MSG_KIND_MEM_IMAGE_RESPONSE ) {
		handle_msg ( mon, msg );
		return;
	}

	Mmove ( ( void * )mon->pmem.base + offset, msg->body, msg->hdr.len );
	*offset_p += msg->hdr.len;

	Print ( stdout, "%#x / %#lx\r", *offset_p, mon->pmem.ram_offset );
}

static void
recv_mem_image_response ( struct mon_t *mon )
{
	int i;

	ASSERT ( mon != NULL );

	i = 0;
	while ( i < mon->pmem.ram_offset ) {
		struct msg_t *msg;

		msg = Comm_remove_msg ( mon->comm );
		recv_mem_image_response_sub ( mon, msg, &i );
		Msg_destroy ( msg );
	}
	DPRINT2 ( "\n" );
	Print ( stdout, "\n" );
}

static void
init_mem_of_ap ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	send_mem_image_request ( mon );
	recv_mem_image_response ( mon );
}

static void
recv_sipi ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	for ( ; ; ) {
		int signo;

		signo = trap_vm ( mon );

		try_handle_msgs ( mon );

		if ( mon->local_apic->startup_info.need_handle )
			break;

		restart_vm ( mon, 0 );
	}    
}

static void
handle_sipi ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	init_mem_of_ap ( mon ); /* [DEBUG] */
	init_regs_of_ap ( mon );
	mon->local_apic->startup_info.need_handle = TRUE;
	mon->mode = NATIVE_MODE;

	restart_vm ( mon, 0 );     
}

void
wait_for_sipi ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );
//	ASSERT ( is_emulation_mode ( mon ) );

	recv_sipi ( mon );
	handle_sipi ( mon );
}

#else /* !ENABLE_MP */

void
wait_for_sipi ( struct mon_t *mon )
{
	Fatal_failure ( "wait_for_sipi() function is never called from an uni-processor machine.\n" );
}

#endif /* ENABLE_MP */

struct mon_t * static_mon = NULL;

static void
handle_sigint ( int sig )
{
#if 0
	struct mon_t *mon = static_mon;

	assert ( static_mon != NULL );

	Stat_print ( &mon->stat );
#endif
	exit ( 1 );
}

static void
handle_sighup ( int sig )
{
//	struct mon_t *mon = static_mon;

	assert ( static_mon != NULL );
	get_snapshot_at_next_trap = TRUE;
	Print_color ( stdout, RED, "SIGHUP Received.\n" );
}

static void
set_finalization ( struct mon_t *mon )
{
	ASSERT ( mon != NULL );

	static_mon = mon;
	On_exit ( &Monitor_finalize, ( void * ) mon ); 
	
	Signal ( SIGHUP, &handle_sighup );
	Signal ( SIGINT, &handle_sigint );
}

static struct comm_t *
init_comm ( struct mon_t *mon, const struct config_t *config )
{
	struct comm_t *comm;

	comm = Comm_create ( mon->cpuid, mon->pid, config, ( config->snapshot != NULL ) );
#ifdef ENABLE_MP
	mon->comm = comm;
#endif	
	return comm;
}

struct mon_t *
Monitor_create ( const struct config_t *config )
{
	struct mon_t *mon;
	struct comm_t *comm;
     
	ASSERT ( config != NULL );
#ifndef ENABLE_MP
	ASSERT ( ProcClass_of_cpuid ( config->cpuid ) == BOOTSTRAP_PROC );
#endif /* ! ENABLE_MP */

	cpu_calibrate_ticks ( );

	init_private_files ( config );

	mon = Malloct ( struct mon_t );
	mon->cpuid = config->cpuid;
	mon->wait_ipi = TRUE;
	mon->mode = NATIVE_MODE;
	mon->pid  = fork_vm ( config );

	Monitor_init_mem ( mon );

	Ptrace_getregs ( mon->pid, &mon->regs->user );
	mon->emu_stack_base = mon->regs->user.esp;
	if ( is_bootstrap_proc ( mon ) ) {
		init_regs_of_bsp ( mon->regs );
	}

	comm = init_comm ( mon, config );

	mon->local_apic = LocalApic_create ( mon->cpuid, comm, mon->pid );
	mon->io_apic = IoApic_create ( mon->cpuid, comm, mon->local_apic );
	init_devices ( mon, config );

	/* initialization for debug */
	init_guest_state ( &mon->guest_state );
	Stat_init ( &mon->stat );
	set_finalization ( mon );

	if ( config->snapshot != NULL ) {
		assert ( is_bootstrap_proc ( mon ) ); /* [DEBUG] */
		Monitor_resume ( mon, config->snapshot );
	}

	restart_vm_with_no_signal ( mon );

	return mon;
}
