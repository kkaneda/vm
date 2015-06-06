#ifndef _VMM_STD_PTHRD_H
#define _VMM_STD_PTHRD_H

#include "vmm/std/types.h"
#include <pthread.h>

typedef void *pthread_start_routine_t ( void *);


void Pthread_create ( pthread_t *thread, pthread_attr_t *attr, void * ( *start_routine ) ( void * ), void *arg );
void Pthread_join ( pthread_t th, void **thread_return );
void Pthread_detach ( pthread_t th );
void Pthread_attr_init ( pthread_attr_t *attr );
void Pthread_attr_destroy ( pthread_attr_t *attr );
void Pthread_attr_setscope ( pthread_attr_t *attr, int scope );
void Pthread_mutex_init ( pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr );
void Pthread_mutex_lock ( pthread_mutex_t *mutex );
void Pthread_mutex_unlock ( pthread_mutex_t *mutex );
void Pthread_mutex_destroy ( pthread_mutex_t *mutex ); 
void Pthread_cond_init ( pthread_cond_t *cond, pthread_condattr_t *cond_attr );
void Pthread_cond_signal ( pthread_cond_t *cond );
void Pthread_cond_broadcast ( pthread_cond_t *cond );
void Pthread_cond_wait ( pthread_cond_t *cond, pthread_mutex_t *mutex );
void Pthread_cond_timedwait ( pthread_cond_t *cond, pthread_mutex_t *mutex,
			      const struct timespec *abstime );
void Pthread_cond_destroy ( pthread_cond_t *cond ) ;

#endif /* _VMM_STD_PTHRD_H */
