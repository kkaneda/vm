#ifndef _VMM_STD_SIGNAL_H
#define _VMM_STD_SIGNAL_H

#include "vmm/std/sig_common.h"
#include <signal.h>

bool_t is_sigsegv ( int signo );
bool_t is_sigstop ( int signo );
bool_t is_sigalrm ( int signo );
bool_t is_sigusr2 ( int signo );

const char *Signo_to_string ( int signo );
void        Kill ( pid_t pid, int sig );
void        Raise ( int sig );
sigfunc_t   *Signal ( int signo, sigfunc_t *func );
void Sigaction ( int signum, const struct sigaction *act, struct sigaction *oldact );
void Sigemptyset ( sigset_t *set );

#endif /* _VMM_STD_SIGNAL_H */
