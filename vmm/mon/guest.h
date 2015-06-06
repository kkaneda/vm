#ifndef _VMM_MON_GUEST_H
#define _VMM_MON_GUEST_H

#include "vmm/common.h"


struct file_descr_entry_t {
     int 			fd;
     char 			*filename;
     struct file_descr_entry_t	*next;
};

struct process_entry_t {
     pid_t			pid;
     bool_t			issuing_syscall;
     struct user_regs_struct	uregs;

     char 			*exec_file;
     struct file_descr_entry_t	*fds;
     /* allocated memory region */
     struct process_entry_t	*next;
};

/* state of a guest operating system */
struct guest_state_t {
     struct process_entry_t 	*procs;
};


struct mon_t;

void init_guest_state(struct guest_state_t *x);
void save_syscall_state(struct guest_state_t *x, struct mon_t *mon, struct user_regs_struct *uregs);
bool_t is_iret_from_syscall(struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current);
struct process_entry_t *get_guest_current_process(struct guest_state_t *x, struct mon_t *mon);
void emulate_syscall(struct guest_state_t *x, struct mon_t *mon, struct process_entry_t *current);


#endif /* _VMM_MON_GUEST_H */
