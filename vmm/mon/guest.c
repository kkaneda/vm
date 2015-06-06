#include "vmm/mon/mon.h"
#include <asm/unistd.h>

/* [TODO] Support of multi-processor */
// #define MONITOR_GUEST_OS 


#ifdef MONITOR_GUEST_OS

void
print_log ( const char *fmt, ... )
{
	static FILE*syslog_fp = NULL;
	va_list ap;

	ASSERT ( fmt != NULL );

	if ( syslog_fp == NULL )
		syslog_fp = Fopen_fmt ( "w+", "%s%d", "/tmp/syscall", 0 );

	va_start ( ap, fmt ); 

	Printv_color ( stdout, CYAN, fmt, ap );
	Printv ( stderr, fmt, ap );

	Printv ( syslog_fp, fmt, ap );
	va_end ( ap );
}

static struct file_descr_entry_t *
FileDescrEntry_create ( int fd, char *filename )
{
	struct file_descr_entry_t *x;

	ASSERT ( filename != NULL );

	x = Malloct ( struct file_descr_entry_t );
	x->fd = fd;
	x->filename = filename;

	return x;
}

static void
FileDescrEntry_destroy ( struct file_descr_entry_t *x )
{
	ASSERT ( x != NULL );
	Free ( x->filename );
	Free ( x );
}

static void
FileDescrEntry_destroy_all ( struct file_descr_entry_t *x )
{
	struct file_descr_entry_t *p;

	p = x;
	while ( p != NULL ) {
		struct file_descr_entry_t *next;
		next = p->next;
		FileDescrEntry_destroy ( p );
		p = next;
	}
}

/****************************************************************/

static void add_new_file_descr ( struct process_entry_t *x, int fd, char *filename );

static struct process_entry_t *
ProcessEntry_create ( pid_t pid )
{
	struct process_entry_t *x;

	x = Malloct ( struct process_entry_t );
	x->pid = pid;
	x->exec_file = NULL;
	x->fds = NULL; /* [TODO] inheritance from a parent process */
	x->next = NULL;
	x->issuing_syscall = FALSE;

	/*
	  add_new_file_descr ( x, 0, "stdin" );
	  add_new_file_descr ( x, 1, "stdout" );
	  add_new_file_descr ( x, 2, "stderr" );
	*/
	return x;
}

static void
ProcessEntry_destroy ( struct process_entry_t *x )
{
	ASSERT ( x != NULL );
	Free ( x->exec_file );
	FileDescrEntry_destroy_all ( x->fds );
	Free ( x );
}

static int
get_sys_no ( struct process_entry_t *x )
{
	ASSERT ( x != NULL );
	return x->uregs.eax;
}

static struct file_descr_entry_t *
try_get_file_descr ( struct process_entry_t *x, int fd )
{
	struct file_descr_entry_t *p;

	ASSERT ( x != NULL );

	for ( p = x->fds; p != NULL; p = p->next ) {
		if ( p->fd == fd )
			return p;
	}
	return NULL;
}

static struct file_descr_entry_t *
get_file_descr ( struct process_entry_t *x, int fd )
{
	static struct file_descr_entry_t *retval;

	retval = try_get_file_descr ( x, fd );
	if ( retval == NULL )
		Fatal_failure ( "get_file_descr: Not found: fd=%d\n", fd );

	return retval;
}

static void
add_new_file_descr ( struct process_entry_t *x, int fd, char *filename )
{
	struct file_descr_entry_t *p, *prev, *n;

	ASSERT ( x != NULL );
     
	/* assert that <fd> has not already been inserted in <x->fds> */
	p = x->fds;
	prev = NULL;
	while ( p != NULL ) {
		ASSERT ( p->fd != fd );
		prev = p;
		p = p->next;
	}

	/* add a new entry to the list */
	n = FileDescrEntry_create ( fd, filename );;
	
	if ( prev == NULL ) {
		x->fds = n;
	} else {
		prev->next = n;
	}
}

static bool_t
try_delete_file_descr ( struct process_entry_t *x, int fd )
{
	struct file_descr_entry_t *p, *prev;

	ASSERT ( x != NULL );
     
	p = x->fds;
	prev = NULL;
	while ( p != NULL ) {
		if ( p->fd == fd )
			break;

		prev = p;
		p = p->next;
	}

	if ( p == NULL )
		return FALSE;

	/* remove <p> from the list */
	if ( prev == NULL ) {
		x->fds = p->next;
	} else {
		prev->next = p->next;
	}
	FileDescrEntry_destroy ( p );

	return TRUE;
}

static void
delete_file_descr ( struct process_entry_t *x, int fd )
{
	bool_t f;

	ASSERT ( x != NULL );

	f = try_delete_file_descr ( x, fd );

	if ( ! f )
		Fatal_failure ( "delete_file_descr: fd=%d\n", fd ); 
}

/* makes <newfd> be the copy of <oldfd>, closing <newfd> first if necessary. */
static void
dup_file_descr ( struct process_entry_t *x, int oldfd, int newfd )
{
	struct file_descr_entry_t *p;

	ASSERT ( x != NULL );

	( void )try_delete_file_descr ( x, newfd );
	p = get_file_descr ( x, oldfd );
	add_new_file_descr ( x, newfd, Strdup ( p->filename ) );
}

/****************************************************************/

void
init_guest_state ( struct guest_state_t *x )
{
	ASSERT ( x != NULL );
	x->procs = ProcessEntry_create ( 0 );;
}

/****************************************************************/

static char *
get_string_from_guest_mem ( struct mon_t *mon, bit32u_t vaddr )
{
	bit32u_t raddr;

	ASSERT ( mon != NULL );

	if ( ! Monitor_check_mem_access_with_vaddr ( mon->regs, SEG_REG_DS, vaddr ) )
		return "???";

	raddr = Monitor_vaddr_to_raddr ( SEG_REG_DS, vaddr );

	return Strdup ( ( char * )raddr );
}

/****************************************************************/

/* This function is called when the guest OS is about to issue a system call. */
void
save_syscall_state ( struct guest_state_t *x, struct mon_t *mon, struct user_regs_struct *uregs )
{
	struct process_entry_t *current;

	ASSERT ( x != NULL );
	ASSERT ( mon != NULL );
	ASSERT ( uregs != NULL );

	current = get_guest_current_process ( x, mon );
	current->uregs = *uregs;
	current->issuing_syscall = TRUE;

#if 1
	print_log ( " ( %d )%#x\t", current->pid, current->uregs.eip );
	print_log ( "call %s ( %#x, %#x )\n",
		  Sysno_to_string ( current->uregs.eax ),
		  current->uregs.ebx,
		  current->uregs.ecx );
#endif
     
	if ( get_sys_no ( current ) != __NR_execve ) {
		//DISABLE_TIMER_INTERRUPT = TRUE; /* [DEBUG] */
		return;
	}

	current->exec_file = get_string_from_guest_mem ( mon, current->uregs.ebx );
     
	print_log ( "[CPU%d] pid=%03d ( %s ) at %#08x\t",
		    mon->cpuid, 
		    current->pid, NULL, current->uregs.eip );
	print_log ( "%s ( \"%s\", ... )\n", 
		    Sysno_to_string ( get_sys_no ( current ) ),
		    current->exec_file );
}

/****************************************************************/

bool_t
is_iret_from_syscall ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current )
{
	ASSERT ( x != NULL );
	ASSERT ( mon != NULL );
	ASSERT ( current != NULL );

	if ( !current->issuing_syscall )
		return FALSE;

	if ( ( get_sys_no ( current ) == __NR_clone ) && ( mon->regs->user.eax == 0x0 ) )
		return TRUE;

	return ( mon->regs->user.esp == current->uregs.esp );
}

/****************************************************************/

static struct process_entry_t *
try_get_process ( struct guest_state_t *x, pid_t pid )
{
	struct process_entry_t *p;

	ASSERT ( x != NULL );

	for ( p = x->procs; p != NULL; p = p->next ) {
		if ( p->pid == pid )
			return p;
	}

	return NULL;     
}

static struct process_entry_t *
get_process ( struct guest_state_t *x, pid_t pid )
{
	struct process_entry_t *p;

	ASSERT ( x != NULL );

	p = try_get_process ( x, pid );
	if ( p == NULL ) 
		Fatal_failure ( "get_current_process: Not found: pid=%d\n", pid );

	return p;
}

static struct process_entry_t *
add_new_process ( struct guest_state_t *x, pid_t pid )
{
	struct process_entry_t *p, *prev, *n;

	ASSERT ( x != NULL );
     
	/* assert that <pid> has not already been inserted in <x->procs> */
	p = x->procs;
	prev = NULL;
	while ( p != NULL ) {
		ASSERT ( p->pid != pid );
		prev = p;
		p = p->next;
	}

	/* add a new entry to the list */
	n = ProcessEntry_create ( pid );;
	
	if ( prev == NULL ) {
		x->procs = n;
	} else {
		prev->next = n;
	}
	return n;
}

static void
delete_process ( struct guest_state_t *x, pid_t pid )
{
	struct process_entry_t *p, *prev;

	ASSERT ( x != NULL );
     
	p = x->procs;
	prev = NULL;
	while ( p != NULL ) {
		if ( p->pid == pid )
			break;

		prev = p;
		p = p->next;
	}

	ASSERT ( p != NULL );

	/* remove <p> from the list */
	if ( prev == NULL ) {
		x->procs = p->next;
	} else {
		prev->next = p->next;
	}
	ProcessEntry_destroy ( p );
}


/* [Note] This function is must be called when the guest OS is in the kernel mode */
pid_t
get_current_pid ( struct guest_state_t *x, struct mon_t *mon )
{
	bit32u_t p;
	const bit32u_t OFFSET = 0x7c; /* See <linux/sched.h> */
	bit32u_t paddr;

	ASSERT ( x != NULL );
	ASSERT ( mon != NULL );

	p = BIT_ALIGN ( mon->regs->user.esp, 13 ); /* See <asm/current.h> */
	
	paddr = Monitor_vaddr_to_paddr( SEG_REG_SS, p + OFFSET );
	return *( pid_t *)( mon->pmem.base + paddr );
}

struct process_entry_t *
get_guest_current_process ( struct guest_state_t *x, struct mon_t *mon )
{
	pid_t pid;
	struct process_entry_t *p;
     
	pid = get_current_pid ( x, mon );

	p = try_get_process ( x, pid );
     
	if ( p != NULL ) 
		return p;

	/* The current process is a newly created process by the clone system call. */

	print_log ( "create a new process: pid=%d\n", pid );
	p = add_new_process ( x, pid );
	p->issuing_syscall = TRUE;
	p->uregs.eax = __NR_clone;

	/*
	 * 親プロセスを特定し，
	 * ファイルデスクリプタを複製する必要がある
	 */

	return p;

}

static void
emulate_sys_clone ( struct guest_state_t *x, struct process_entry_t *current, int ret )
{
	/* Do nothing */
}

static void
emulate_sys_exit ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	pid_t pid = ret; /* ??? */
	struct process_entry_t *p = get_process ( x, pid );

	print_log ( "delete a process: exec_file=\"%s\", pid=%d\n", p->exec_file, pid );
	delete_process ( x, pid );
}

static void
emulate_sys_execve ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	if ( ret < 0 ) 
		return;

	c->exec_file = get_string_from_guest_mem ( mon, c->uregs.ebx );
	print_log ( "execute: file=\"%s\", pid=%d\n", c->exec_file, c->pid );
}

static void
emulate_sys_open ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	int fd = ret;
	char *s;

	if ( ret < 0 ) 
		return;

	s = get_string_from_guest_mem ( mon, c->uregs.ebx );	
	add_new_file_descr ( c, fd, s );
}

static void
emulate_sys_close ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	int fd = c->uregs.ebx;

	if ( ret < 0 )
		return;

	if ( try_get_file_descr ( c, fd ) == NULL ) {
		print_log ( "close: not found: oldfd=%d\n", fd );
		return;
	}

	print_log ( "close: fd=%d, name=\"%s\"\n", fd, get_file_descr ( c, fd )->filename );
	delete_file_descr ( c, fd );
}

static void
emulate_sys_dup ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	int oldfd = c->uregs.ebx;

	if ( ret == -1 ) 
		return; 

	if ( try_get_file_descr ( c, oldfd ) == NULL ) {
		print_log ( "dup: not found: oldfd=%d\n", oldfd );
		return;
	}

	print_log ( "dup: oldfd=%d, newfd=%d\n", oldfd, ret );
	dup_file_descr ( c, oldfd, ret );
}

static void
emulate_sys_dup2 ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	int oldfd = c->uregs.ebx;
	int newfd = c->uregs.ecx;

	if ( ret == -1 )
		return;

	if ( try_get_file_descr ( c, oldfd ) == NULL ) {
		print_log ( "dup: not found: oldfd=%d\n", oldfd );
		return;
	}

	print_log ( "dup: oldfd=%d, newfd=%d\n", oldfd, newfd );
	dup_file_descr ( c, oldfd, newfd );
}

static void
emulate_sys_socketcall ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *c, int ret )
{
	/*
	int fd = ret;
	char *s = Strdup ( "socket" );
	if ( ret < 0 ) 
		return;
	add_new_file_descr ( c, fd, s );
	*/
}

enum syscall_kind {
	SYSCALL_KIND_1ARG_STRING,
	SYSCALL_KIND_1ARG_INT,
	SYSCALL_KIND_2ARG_INT_INT,
	SYSCALL_KIND_3ARG_INT_STRING_INT,
	SYSCALL_KIND_MMAP,
	SYSCALL_KIND_ANY
};
typedef enum syscall_kind		syscall_kind_t;

static syscall_kind_t
syscall_kind_of_sys_no ( int sys_no )
{
	if ( ( sys_no == __NR_execve ) || ( sys_no == __NR_open ) || 
	     ( sys_no == __NR_mknod ) || ( sys_no == __NR_stat ) ) {
		return SYSCALL_KIND_1ARG_STRING;
	}

	if ( ( sys_no == __NR_close ) || ( sys_no == __NR_brk ) || 
	     ( sys_no == __NR_read )  ||  ( sys_no == __NR_fstat ) ||
	     ( sys_no == __NR_waitpid ) || ( sys_no == __NR_wait4 ) ) {
		return SYSCALL_KIND_1ARG_INT;
	}

	if ( sys_no == __NR_mmap ) {
		return SYSCALL_KIND_MMAP;
	}

	if ( ( sys_no == __NR_munmap )  ||
	     ( sys_no == __NR_dup2 ) || ( sys_no == __NR_ioctl ) ) {
		return SYSCALL_KIND_2ARG_INT_INT;
	}

	if ( sys_no == __NR_write ) {
		return SYSCALL_KIND_3ARG_INT_STRING_INT;
	}

	return SYSCALL_KIND_ANY;
}

static void
print_args ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current )
{
	int sys_no;
	syscall_kind_t kind;

	sys_no = get_sys_no ( current );
	kind = syscall_kind_of_sys_no ( sys_no );

	switch ( kind ) {
	case SYSCALL_KIND_1ARG_STRING: {
		bool_t is_ok;
		bit32u_t vaddr, paddr;
		char *args;

		vaddr = current->uregs.ebx;
		paddr = Monitor_try_vaddr_to_paddr ( SEG_REG_DS, vaddr, &is_ok );
		args = ( char * ) ( paddr + mon->pmem.base );

		print_log ( "( \"%s\", ... )",  args );
		break;
	}

	case SYSCALL_KIND_1ARG_INT: 
		print_log ( "( %#x, ... )", current->uregs.ebx );
		break;

	case SYSCALL_KIND_2ARG_INT_INT:
		print_log ( "( %#x, %#x, ... )", current->uregs.ebx, current->uregs.ecx );
		break;

	case SYSCALL_KIND_3ARG_INT_STRING_INT: {
		bit32u_t vaddr = current->uregs.ecx;
		char * s;
		
		s = ( ( ! Monitor_check_mem_access_with_vaddr ( mon->regs, SEG_REG_DS, vaddr ) )
		      ?  "???" 
		      : (char *)Monitor_vaddr_to_raddr ( SEG_REG_DS, vaddr ) );
		
		print_log ( "( %#x, \"%s\", %#x )", current->uregs.ebx, s, current->uregs.edx );
		break;
	}

        case SYSCALL_KIND_MMAP: {
		bool_t is_ok;
		bit32u_t vaddr, paddr;
		bit32u_t v;

		vaddr = current->uregs.ebx;
		paddr = Monitor_try_vaddr_to_paddr ( SEG_REG_DS, vaddr, &is_ok );
		v = *(( bit32u_t * ) ( paddr + mon->pmem.base ));

		print_log ( "( %#x, %#x, ... )", 
			    v, current->uregs.ecx );
		break;
	}

	case SYSCALL_KIND_ANY:
		print_log ( "( ... )" );
		break;

	default:
		Match_failure ( "print_args\n" );
	}
}

/* [NOTE]
 * This function is called from the end of the emulation of 'iret' instruction. 
 */
void
emulate_syscall ( struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current )
{
	int sys_no;
	int ret;

	ASSERT ( x != NULL );
	ASSERT ( mon != NULL );
	ASSERT ( current != NULL );

	sys_no = get_sys_no ( current );
	ret = mon->regs->user.eax;

	print_log ( "[CPU%d] pid=%03d ( %s ) at %#08x\t", 
		    mon->cpuid, 
		    current->pid, current->exec_file, current->uregs.eip );
	print_log ( "%s ", Sysno_to_string ( sys_no ) );

	print_args ( x, mon, current );
	print_log ( " = %#x\n", ret );

	ASSERT ( ( ( sys_no == __NR_clone ) && ( ret == 0x0 ) ) ||
		 ( mon->regs->user.esp == current->uregs.esp ) );

	switch ( sys_no ) {
	case __NR_clone:	emulate_sys_clone ( x, current, ret ); break;
	case __NR_exit:		emulate_sys_exit ( x, mon, current, ret ); break;
	case __NR_execve:	emulate_sys_execve ( x, mon, current, ret ); break;
	case __NR_open:		emulate_sys_open ( x, mon, current, ret ); break;
	case __NR_close:	emulate_sys_close ( x, mon, current, ret ); break;
	case __NR_dup:		emulate_sys_dup ( x, mon, current, ret ); break;
	case __NR_dup2:		emulate_sys_dup2 ( x, mon, current, ret ); break;
	case __NR_socketcall: 	emulate_sys_socketcall ( x, mon, current, ret ); break;
	default:		break;
	}
	current->issuing_syscall = FALSE;
}

#else /* ! MONITOR_GUEST_OS */

void
init_guest_state(struct guest_state_t *x)
{ /* do nothing */ }

void
save_syscall_state(struct guest_state_t *x, struct mon_t *mon, struct user_regs_struct *uregs)
{ /* do nothing */ }

bool_t
is_iret_from_syscall(struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current)
{
	return FALSE;
}

struct process_entry_t *
get_guest_current_process(struct guest_state_t *x, struct mon_t *mon)
{
	return FALSE;
}

void
emulate_syscall(struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current)
{ /* do nothing */ }

#endif /* MONITOR_GUEST_OS */
