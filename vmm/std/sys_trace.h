#ifndef _VMM_STD_SYS_TRACE_H
#define _VMM_STD_SYS_TRACE_H

#include "vmm/std/types.h"
#include "vmm/std/fptr.h"
#include <sys/ptrace.h>
#include <linux/user.h>

void        Ptrace_traceme ( void );
long        Ptrace_peektext ( pid_t pid, void *addr );
void        Ptrace_poketext ( pid_t pid, void *addr, void *data );
void        Ptrace_cont ( pid_t pid, int signo );
void        Ptrace_syscall ( pid_t pid, int signo );
void        Ptrace_singlestep ( pid_t pid, int signo );
void        Ptrace_restart ( pid_t pid, enum __ptrace_request request, int signo );
void        Ptrace_getregs ( pid_t pid, struct user_regs_struct *regs );
void        Ptrace_setregs ( pid_t pid, struct user_regs_struct *regs );
void	    Ptrace_getfpregs ( pid_t pid, struct user_i387_struct *regs );
void        Ptrace_setfpregs ( pid_t pid, struct user_i387_struct *regs );
void        Ptrace_getmem ( pid_t pid, void *addr, const struct fptr_t src );
void        Ptrace_setmem ( pid_t pid, void *addr, struct fptr_t dest );
int         Ptrace_trap ( pid_t pid );
void        Print_user_regs ( FILE *stream, struct user_regs_struct * x );
const char *Sysno_to_string ( int sysno );

#endif /* _VMM_STD_SYS_TRACE_H */
