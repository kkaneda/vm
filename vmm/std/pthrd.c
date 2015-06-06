#include "vmm/std/types.h"
#include "vmm/std/debug.h"
#include <pthread.h>


void
Pthread_create ( pthread_t *thread, pthread_attr_t *attr, void * ( *start_routine ) ( void * ), void *arg )
{
	int retval;
	ASSERT ( start_routine != NULL );

	retval = pthread_create ( thread, attr, start_routine, arg );

	if ( retval != 0 )
		Sys_failure ( "pthread_create" );
}

void
Pthread_join ( pthread_t th, void **thread_return )
{
	int retval;

	retval = pthread_join ( th, thread_return );

	if ( retval != 0 )
		Sys_failure ( "pthread_join" );
}

void
Pthread_detach ( pthread_t th )
{
	int retval;

	retval = pthread_detach ( th );

	if ( retval != 0 )
		Sys_failure ( "pthread_detach" );
}

void
Pthread_attr_init ( pthread_attr_t *attr )
{
	int retval;

	ASSERT ( attr != NULL );

	retval = pthread_attr_init(attr);

	if ( retval != 0 ) 
		Sys_failure ( "pthread_attr_init" );
}

void
Pthread_attr_destroy ( pthread_attr_t *attr )
{
     int retval;

     ASSERT ( attr != NULL );

     retval = pthread_attr_destroy ( attr );

     if ( retval != 0 )
	     Sys_failure ( "pthread_attr_destroy" );
}

void 
Pthread_attr_setscope ( pthread_attr_t *attr, int scope )
{
	int retval;

	ASSERT ( attr != NULL );

	retval = pthread_attr_setscope ( attr, scope );

	if ( retval != 0 ) 
		Sys_failure ( "pthread_attr_setscope" );
}

void
Pthread_mutex_init ( pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr )
{
	int retval;

	ASSERT ( mutex != NULL );

	retval = pthread_mutex_init ( mutex, mutexattr );

	if ( retval != 0 )
		Sys_failure ( "pthread_mutex_init" );
}

void
Pthread_mutex_lock ( pthread_mutex_t *mutex )
{
	int retval;
	ASSERT ( mutex != NULL );

	retval = pthread_mutex_lock ( mutex );

	if ( retval != 0 )
		Sys_failure ( "pthread_mutex_lock" );
}

void
Pthread_mutex_unlock ( pthread_mutex_t *mutex )
{
	int retval;

	ASSERT ( mutex != NULL );

	retval = pthread_mutex_unlock ( mutex );

	if ( retval != 0 )
		Sys_failure ( "pthread_mutex_unlock" );
}

void
Pthread_mutex_destroy ( pthread_mutex_t *mutex ) 
{
	int retval;

	ASSERT ( mutex != NULL );

	retval = pthread_mutex_destroy ( mutex );
	if ( retval != 0 )
		Sys_failure ( "pthread_mutex_destroy" );
}

void
Pthread_cond_init ( pthread_cond_t *cond, pthread_condattr_t *cond_attr )
{
	int retval;

	ASSERT ( cond != NULL );

	retval = pthread_cond_init ( cond, cond_attr );

	if ( retval != 0 )
		Sys_failure ( "pthread_cond_init" );
}

void
Pthread_cond_signal ( pthread_cond_t *cond )
{
	int retval;

	ASSERT ( cond != NULL );

	retval = pthread_cond_signal ( cond );

	if ( retval != 0 )
		Sys_failure ( "pthread_cond_signal" );
}

void
Pthread_cond_broadcast ( pthread_cond_t *cond )
{
	int retval;

	ASSERT ( cond != NULL );

	retval = pthread_cond_broadcast ( cond );

	if ( retval != 0 ) 
		Sys_failure ( "pthread_cond_broadcast" );
}

void
Pthread_cond_wait ( pthread_cond_t *cond, pthread_mutex_t *mutex )
{
	int retval;

	ASSERT ( cond != NULL );
	ASSERT ( mutex != NULL );

	retval = pthread_cond_wait ( cond, mutex );

	if ( retval != 0 )
		Sys_failure ( "pthread_cond_wait" ); 
}

void
Pthread_cond_timedwait ( pthread_cond_t *cond, pthread_mutex_t *mutex,
			 const struct timespec *abstime )
{
	int retval;

	ASSERT ( cond != NULL );
	ASSERT ( mutex != NULL );
	ASSERT ( abstime != NULL );

	retval = pthread_cond_timedwait ( cond, mutex, abstime );
	if ( ( retval != 0 ) && ( retval != ETIMEDOUT ) )
		Sys_failure ( "pthread_cond_timedwait" );
}

void
Pthread_cond_destroy ( pthread_cond_t *cond ) 
{
	int retval;

	ASSERT ( cond != NULL );

	retval = pthread_cond_destroy ( cond );

	if ( retval != 0 )
		Sys_failure ( "pthread_cond_destroy" );
}
