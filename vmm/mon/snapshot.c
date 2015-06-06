#include "vmm/mon/mon.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


static void
get_snapshot_name ( struct mon_t *mon, char *s, size_t len )
{
	time_t t;

	t = time ( NULL );
	strftime ( s, len, "/tmp/snapshot.%Y-%m-%d-%H-%M-%S", 
		   localtime ( &t ) );
}

static void
pack_mem ( struct mon_t *mon, int fd )
{
	Pack ( ( void *)mon->pmem.base, mon->pmem.ram_offset, fd );	
}

static void
unpack_mem ( struct mon_t *mon, int fd )
{
	Unpack ( ( void *)mon->pmem.base, mon->pmem.ram_offset, fd );	
}

static void 
pack_page_descrs ( struct mon_t *mon, int fd )
{
	int i;
	for ( i = 0; i < mon->num_of_pages; i ++ ) {
		PageDescr_pack ( & mon->page_descrs [ i ], fd );
	} 
}

static void 
unpack_page_descrs ( struct mon_t *mon, int fd )
{
	int i;
	for ( i = 0; i < mon->num_of_pages; i ++ ) {
		PageDescr_unpack ( & mon->page_descrs [ i ], fd );
		/* [???] change page protection? */
	} 
}

static void
__monitor_create_snapshot ( struct mon_t *mon, const char *filename )
{
	int fd;

	ASSERT ( mon != NULL );

	Print_color ( stdout, GREEN, "CPU[%d] snapshot created: filename=\"%s\".\n", 
		      mon->cpuid, filename );

	fd = Open2 ( filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );

	pack_mem ( mon, fd );
	Regs_pack ( mon->regs, fd );
	LocalApic_pack ( mon->local_apic, fd );
	IoApic_pack ( mon->io_apic, fd );
	pack_devices ( mon, fd );
	Bool_pack ( mon->wait_ipi, fd );

	pack_page_descrs ( mon, fd );
#ifdef ENABLE_MP
	Comm_pack_msgs ( mon->comm, fd );
#endif

	Close ( fd );
}

void
Monitor_create_snapshot ( struct mon_t *mon )
{
	enum { BUFSIZE = 1024 };
	char filename [BUFSIZE];

	ASSERT ( mon != NULL );
	assert ( is_native_mode ( mon ) );

	get_snapshot_name ( mon, filename, BUFSIZE );
	__monitor_create_snapshot ( mon, filename );
	
	exit ( 1 );
}

void
Monitor_resume ( struct mon_t *mon, const char *filename )
{
	int fd;

	ASSERT ( mon != NULL );
	ASSERT ( filename != NULL );
	assert ( is_native_mode ( mon ) );

	fd = Open ( filename, O_RDONLY );

	unpack_mem ( mon, fd );
	Regs_unpack ( mon->regs, fd );
	LocalApic_unpack ( mon->local_apic, fd );
	IoApic_unpack ( mon->io_apic, fd );
	unpack_devices ( mon, fd );
	mon->wait_ipi = Bool_unpack ( fd );

	unpack_page_descrs ( mon, fd ); 

#ifdef ENABLE_MP
	Comm_unpack_msgs ( mon->comm, fd );
#endif

	Print_color ( stdout, GREEN, "CPU[%d] snapshot resumed: filename=\"%s\".\n", 
		      mon->cpuid, filename );

	Close ( fd );
}

#ifdef ENABLE_MP

static void
exec_vm ( struct mon_t *mon, struct node_t dest, char *new_config, char *snapshot )
{
	enum { BUFSIZE = 1024 };
	char *MON_PROGRAM = "/home/users/kaneda/vm/vmm/mon/mon"; // [TODO]
	char cpuid [BUFSIZE];
	char *argv [] = { 
		"ssh", dest.hostname, // [DEBUG]
		MON_PROGRAM,
		"--id",       cpuid,
		"--config",   new_config,
		"--snapshot", snapshot,
		NULL };
	int err;

	Snprintf ( cpuid, BUFSIZE, "%d", mon->cpuid );

	Print_color ( stdout, GREEN, "CPU[%d] exec \"ssh %s %s\".\n", 
		      mon->cpuid, argv[1], argv[2] );
	Print ( stderr, "CPU[%d] exec ssh %s %s %s.\n", 
		mon->cpuid, argv[1], argv[2], argv[3] );

	err  = execvp ( argv [0], argv );
	if ( err == -1 ) 
		Sys_failure ( "execvp" );
	assert ( 0 );	
}

void
Monitor_migrate ( struct mon_t *mon, struct node_t dest, char *new_config )
{
	enum { BUFSIZE = 1024 };
	char filename [BUFSIZE];

	ASSERT ( mon != NULL );
	ASSERT ( new_config != NULL );
	assert ( is_native_mode ( mon ) );

	Print_color ( stdout, GREEN, "CPU[%d] migration started.\n", mon->cpuid );
	Print_color ( stdout, GREEN, "CPU[%d] communication subsystem shut down.\n", mon->cpuid );

	Comm_shutdown ( mon->comm ); // [TODO] Should this function be called after snapshot? 

	get_snapshot_name ( mon, filename, BUFSIZE );
	__monitor_create_snapshot ( mon, filename );

	Kill ( mon->pid, SIGKILL ); // [???] Is it necessary? */
	exec_vm ( mon, dest, new_config, filename ); 

	assert ( 0 );
}

#else /* !ENABLE_MP */

void
Monitor_migrate ( struct mon_t *mon, struct node_t dest, char *new_config )
{
	Fatal_failure ( "Monitor_migrate: "
			"migraiton for single-processor machines is not supported.\n" );
}

#endif /* ENABLE_MP */
