#include "vmm/std/types.h"
#include "vmm/std/print.h"
#include "vmm/std/debug.h"
#include "vmm/std/fptr.h"
#include "vmm/std/mem.h"
#include "vmm/std/unix.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <linux/user.h>
#include <asm/unistd.h>

/* make the parent process trace this process */
void
Ptrace_traceme ( void )
{
	long retval;

	retval = ptrace ( PTRACE_TRACEME, 0, NULL, NULL );

	if ( retval != 0 )
		Sys_failure ( "ptrace" );
}

long
Ptrace_peektext ( pid_t pid, void *addr )
{
	long retval;

	retval = ptrace ( PTRACE_PEEKTEXT, pid, addr, NULL );

	if ( retval == -1 ) 
		Sys_failure ( "ptrace: request is PTRACE_PEEKTEXT" );

	return retval;
} 

void
Ptrace_poketext ( pid_t pid, void *addr, void *data )
{
	long retval;

	retval = ptrace ( PTRACE_POKETEXT, pid, addr, data );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" );
}

void
Ptrace_cont ( pid_t pid, int signo )
{
	long retval;

	retval = ptrace ( PTRACE_CONT, pid, NULL, ( void * )signo );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" ); 
}

void
Ptrace_syscall ( pid_t pid, int signo )
{
	long retval;

	retval = ptrace ( PTRACE_SYSCALL, pid, NULL, ( void * )signo );

	if ( retval == -1 ) 
		Sys_failure ( "ptrace: request is PTRACE_SYSCALL" );
}

void
Ptrace_singlestep ( pid_t pid, int signo )
{
	long retval;

	retval = ptrace ( PTRACE_SINGLESTEP, pid, NULL, ( void * )signo );

	if ( retval == -1 ) 
		Sys_failure ( "ptrace error: request is PTRACE_SINGLESTEP" ); 
}

void 
Ptrace_restart ( pid_t pid, enum __ptrace_request request, int signo )
{
	long retval;

	retval = ptrace ( request, pid, NULL, ( void * )signo );
	
	if ( retval == -1 ) 
		Sys_failure ( "ptrace error" ); 
}

void 
Ptrace_getregs ( pid_t pid, struct user_regs_struct *regs )
{
	long retval;

	ASSERT ( regs != NULL );
   
	retval = ptrace ( PTRACE_GETREGS, pid, NULL, regs );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" );
}

/* See <linux/user.h> for information on the format of <data> */
void 
Ptrace_setregs ( pid_t pid, struct user_regs_struct *regs )
{
	long retval;
	ASSERT ( regs != NULL );

	retval = ptrace ( PTRACE_SETREGS, pid, NULL, ( void * )regs );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" ); 
}

void 
Ptrace_getfpregs ( pid_t pid, struct user_i387_struct *regs )
{
	long retval;

	ASSERT ( regs != NULL );
   
	retval = ptrace ( PTRACE_GETFPREGS, pid, NULL, regs );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" );
}

void 
Ptrace_setfpregs ( pid_t pid, struct user_i387_struct *regs )
{
	long retval;
	ASSERT ( regs != NULL );

	retval = ptrace ( PTRACE_SETFPREGS, pid, NULL, ( void * )regs );

	if ( retval != 0 ) 
		Sys_failure ( "ptrace" ); 
}

/* read from the location <addr> in the child's memory,
   and copy it to <src> */
void
Ptrace_getmem ( pid_t pid, void *addr, const struct fptr_t src )
{
	size_t i;

	Fptr_zero ( src );

	for ( i = 0; i < src.offset; i += sizeof ( int ) ) {
		void *p = addr + i;
		void *q = src.base + i;
		int value = Ptrace_peektext ( pid, p );
		Mmove ( q, &value, sizeof ( int ) );
	}
}

/* copy the <data> to location <addr> in the child's memory */
void
Ptrace_setmem ( pid_t pid, void *addr, struct fptr_t dest )
{
	int i;

	for ( i = 0; i < dest.offset; i += sizeof ( int ) ) {
		int value = * ( ( int * ) ( dest.base + i ) ); 
		void *p = addr + i;
		Ptrace_poketext ( pid, p, ( void * )value );
	}
}

int
Ptrace_trap ( pid_t pid )
{
	int status;
   
	Waitpid ( pid, &status, 0 );

	if ( WIFEXITED ( status ) ) {
		Fatal_failure ( "ptrace: the child process terminated normally.\n" );
	}

	if ( WIFSIGNALED ( status ) ) {
		Fatal_failure ( "ptrace: the child process terminated "
				"because of a signal which was not caught: signo=%d.\n",
				WTERMSIG ( status ) );
	}

	assert ( WIFSTOPPED ( status ) );

	return WSTOPSIG ( status );
}

void 
Print_user_regs ( FILE *stream, struct user_regs_struct *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream, "regs = {ebx=%#x, ecx=%#x, edx=%#x, esi=%#x, edi=%#x, ebp=%#x, eax=%#x, "
		"eip=%#x, ds=%#x, fs=%#x, cs=%#x, ds=%#x, es=%#x, ss=%#x, eflags=%#x}\n",
		x->ebx, x->ecx, x->edx, x->esi, x->edi, x->ebp, x->eax,
		x->eip, x->ds, x->fs, x->cs, x->ds, x->es, x->ss, x->eflags );
}


/* See /usr/include/asm/unistd.h */

#ifndef __NR_restart_syscall
#define __NR_restart_syscall   0
#endif /* __NR_restart_syscall */

#ifndef __NR_exit_group
#define __NR_exit_group		252
#endif /* __NR_exit_group */

#ifndef __NR_clock_gettime
#define __NR_clock_gettime	265
#endif /* __NR_clock_gettime */

struct syscall_info_entry_t {
	int		no;
	char 		*name;
};

static struct syscall_info_entry_t syscall_info_map[] =
{
	{ __NR_restart_syscall,	"restart_systemcall" }, /*  0 */
	{ __NR_exit,		"exit" },		/*  1 */
	{ __NR_fork,		"fork"},		/*  2 */
	{ __NR_read,		"read" },		/*  3 */
	{ __NR_write,		"write" },		/*  4 */
	{ __NR_open,		"open" },		/*  5 */
	{ __NR_close,		"close" },		/*  6 */
	{ __NR_waitpid,		"waitpid" },		/*  7 */
	{ __NR_creat,		"creat" },		/*  8 */
	{ __NR_link,		"link" }, 		/*  9 */
	{ __NR_unlink,		"unlink" },		/* 10 */
	{ __NR_execve,		"execve" },		/* 11 */
	{ __NR_chdir,		"chdir" },		/* 12 */
	{ __NR_time,		"time" },		/* 13 */
	{ __NR_mknod,		"mknod" },		/* 14 */
	{ __NR_chmod,		"chmod" }, 		/* 15 */
	{ __NR_lseek,		"lseek" },		/* 19 */
	{ __NR_getpid,		"getpid" },		/* 20 */
	{ __NR_mount,		"mount" },		/* 21 */
	{ __NR_umount,		"umount" },		/* 22 */
	{ __NR_setuid, 		"setuid" },		/* 23 */
	{ __NR_getuid,		"getuid" },		/* 24 */
	{ __NR_alarm,		"alarm" }, 		/* 27 */
	{ __NR_pause,		"pause" }, 		/* 29 */
	{ __NR_utime,		"utime" },		/* 30 */
	{ __NR_access,		"access" },		/* 33 */
	{ __NR_sync,		"sync" },		/* 36 */
	{ __NR_kill,		"kill" },		/* 37 */
	{__NR_rename,		"rename" },		/* 38 */
	{ __NR_dup,		"dup" },		/* 41 */
	{ __NR_pipe,		"pipe" },		/* 42 */
	{ __NR_brk,		"brk" },		/* 45 */
	{ __NR_getgid,		"getgid" },		/* 47 */
	{ __NR_geteuid,		"geteuid" },		/* 49 */
	{ __NR_getegid,		"getegid" },		/* 50 */
	{ __NR_umount2,		"umount2" },		/* 52 */
	{ __NR_ioctl,		"ioctl" },		/* 54 */
	{ __NR_fcntl,		"fcntl" },		/* 55 */
	{ __NR_setpgid,		"setpgid" },		/* 57 */
	{ __NR_umask,		"umask" },		/* 60 */
	{ __NR_dup2,		"dup2" },		/* 63 */
	{ __NR_getppid,		"getppid" },		/* 64 */
	{ __NR_getpgrp,		"getpgrp" },		/* 65 */
	{ __NR_setsid,		"setsid" },		/* 66 */
	{ __NR_sethostname,	"sethostname" },	/* 74 */
	{ __NR_setrlimit,	"setrlimit" },		/* 75 */
	{ __NR_getrlimit,	"getrlimit" }, 		/* 76 */
	{ __NR_getrusage,	"getrusage" },		/* 77 */
	{ __NR_gettimeofday,	"gettimeofday" },	/* 78 */
	{ __NR_settimeofday,	"settimeofday" },	/* 79 */
	{ __NR_setgroups,	"setgroups" },		/* 81 */
	{ __NR_symlink,		"symlink" },		/* 83 */
	{ __NR_readlink,	"readlink" },		/* 85 */
	{ __NR_swapon,		"swapon" },		/* 87 */
	{ __NR_reboot,		"reboot" },		/* 88 */
	{ __NR_mmap,		"mmap" },		/* 90 */
	{ __NR_munmap,		"munmap" },		/* 91 */
	{ __NR_ftruncate,	"ftruncate" },		/* 93 */
	{ __NR_fchmod,		"fchmod" },		/* 94 */
	{ __NR_statfs,		"statfs" },		/* 99 */
	{ __NR_syslog,		"syslog" },		/* 103 */
	{ __NR_setitimer,	"setitimer" },		/* 104 */
	{ __NR_getitimer,	"getitimer" },		/* 105 */
	{ __NR_stat,		"stat" },		/* 106 */
	{ __NR_lstat,		"lstat" },		/* 107 */
	{ __NR_fstat,		"fstat" }, 		/* 108 */
	{ __NR_olduname,	"olduname" }, 		/* 109 */
	{ __NR_iopl,		"iopl" },		/* 110 */
	{ __NR_socketcall,	"socketcall" },		/* 102 */
	{ __NR_wait4,		"wait4" },		/* 114 */
	{ __NR_ipc,		"ipc" },		/* 117 */
	{ __NR_fsync,		"fsync" },		/* 118 */
	{ __NR_sigreturn,	"sigreturn" }, 		/* 119 */
	{ __NR_clone,		"clone" },		/* 120 */
	{ __NR_setdomainname,	"setdomainname" },	/* 121 */
	{ __NR_uname,		"uname" },		/* 122 */
	{ __NR_mprotect,	"mprotect" },		/* 125 */
	{ __NR_get_kernel_syms,	"get_kernel_syms" },	/* 130 */
	{ __NR_fchdir,		"fchdir" },		/* 133 */
	{ __NR_bdflush,		"bdflush" },		/* 134 */
	{ __NR__llseek,		"llseek" },		/* 140 */
	{ __NR_getdents,	"getdents" },		/* 141 */
	{ __NR__newselect,	"newselect" }, 		/* 142 */
	{ __NR_flock,		"flock" },		/* 143 */
	{ __NR_writev,		"writev" },		/* 146 */
	{ __NR_fdatasync,	"fdatasync" }, 		/* 148 */
	{ __NR__sysctl,		"sysctl" }, 		/* 149 */
	{ __NR_sched_setscheduler, "sched_setscheduler" } , /* 156 */
	{ __NR_sched_yield, 	"sched_yield" } , 	/* 158  */
	{ __NR_nanosleep,	"nanosleep" },		/* 162 */
	{ __NR_query_module,	"query_module" },	/* 167 */
	{ __NR_poll,		"poll" },		/* 168 */
	{ __NR_rt_sigaction,	"rt_sigaction" },	/* 174 */
	{ __NR_rt_sigprocmask,	"rt_sigprocmask" },	/* 175 */
	{ __NR_rt_sigsuspend,	"rt_sigsuspend" },	/* 179 */
	{ __NR_chown,		"chown" },		/* 182 */
	{ __NR_getcwd,		"getcwd" },		/* 183 */
	{ __NR_vfork,		"vfokr" },		/* 190 */
	{ __NR_ugetrlimit,	"ugetrlimit" },		/* 191 */
	{ __NR_mmap2,		"mmap2" },		/* 192 */
	{ __NR_ftruncate64,	"ftruncate64" }, 	/* 194 */
	{ __NR_stat64,		"stat64" },		/* 195 */
	{ __NR_lstat64,		"lstat64" },		/* 196 */
	{ __NR_fstat64,		"fstat64" },		/* 197 */
	{ __NR_getuid32,	"getuid32" },		/* 199 */
	{ __NR_getgid32,	"getgid32" },		/* 200 */
	{ __NR_geteuid32,	"geteuid32" },		/* 201 */
	{ __NR_getegid32,	"getegid32" },		/* 202 */
	{ __NR_setreuid32,	"setreuid32" },		/* 203 */
	{ __NR_setregid32,	"setregid32" }, 	/* 204 */
	{ __NR_getgroups32,	"getgroups32" },	/* 205 */
	{ __NR_setgroups32,	"setgroups32" },	/* 206 */
	{ __NR_setresuid32,	"setresuid32" },	/* 208 */
	{ __NR_chown32,		"chown32" },		/* 212 */
	{ __NR_setuid32, 	"setuid32" },		/* 213 */
	{  __NR_setgid32,	"setgid32" },		/* 214 */
	{ __NR_getdents64,	"getdents64" },		/* 220 */
	{ __NR_fcntl64,		"fcntl64" },		/* 221 */
	{ __NR_set_thread_area,	"set_thread_area" },	/* 243 */
	{ __NR_exit_group,	"exit_group" },		/* 252 */
	{ __NR_clock_gettime,	"clock_gettime" },	/* 265 */
};

static size_t
nr_syscall_info_entries ( void )
{
	return sizeof ( syscall_info_map ) / sizeof ( struct syscall_info_entry_t ); 
}

const char *
Sysno_to_string ( int sysno )
{
	int i;

	for ( i = 0; i < nr_syscall_info_entries ( ); i++ ) {
		struct syscall_info_entry_t *x = &syscall_info_map[i];

		if ( x->no == sysno ) 
			return x->name; 
	}

	Warning ( "sysno_to_string: unknown sysno=%d\n", sysno );
	return "";
}
