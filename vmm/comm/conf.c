#include "vmm/comm/conf_common.h"
#include <getopt.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>


static void
init_config ( struct config_t *config, char * const argv[] )
{
	int i;

	ASSERT ( config != NULL );

	config->cpuid = -1;
	config->disk = NULL;
	config->memory = NULL;
	config->snapshot = NULL;
	config->dirname = dirname ( Strdup ( argv[0] ) );

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		config->nodes[i].hostname = NULL;
		config->nodes[i].port = -1;
	}
}

/****************************************************************/

static void
print_config ( const struct config_t *config )
{
	int i;

	ASSERT ( config != NULL );

	Print ( stdout, "------------------ CONFIGRATION ------------------\n" );
	Print ( stdout, 
		"cpuid  = %d\n"
		"disk   = \"%s\"\n"
		"memory = \"%s\"\n",
		config->cpuid, 
		config->disk, 
		config->memory
		);
	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		Print ( stdout,	"cpu[%d] = %s:%d\n", i, config->nodes[i].hostname, config->nodes[i].port );
	}
	
	Print ( stdout, "--------------------------------------------------\n" );
}

/****************************************************************/

static bool_t
config_not_given ( const struct config_t *config )
{
	ASSERT ( config != NULL );
	
	return ( ( config->cpuid == -1) || ( config->config_file == NULL) );
}

static void
print_usage ( char * const argv[] )
{
	Print ( stdout, "Usage: %s --id <number> --config <filename> --snapshot <filename>\n", argv[0] );
}

static bool_t
parse_option_sub ( struct config_t *config, int argc, char * const argv[] )
{
	const char *optstring = "c:d:m:";
	const struct option long_options[] = {
		{ "id",     required_argument, NULL, 'i' },
		{ "config", required_argument, NULL, 'c' },
		{ "snapshot", required_argument, NULL, 's' },
		{ 0, 0, 0, 0 } 
	};
	char c;
	
	ASSERT ( config != NULL );
	
	c = getopt_long ( argc, argv, optstring, long_options, NULL );
	
	if ( c == -1 ) 
		return FALSE;
	
	switch ( c ) {
	case 'i': config->cpuid = Atoi ( optarg ); break;
	case 'c': config->config_file = Strdup ( optarg ); break;
	case 's': config->snapshot = Strdup ( optarg ); break;
	default:  print_usage ( argv ); exit(1);
	}
	return TRUE;
}

static void
parse_option ( struct config_t *config, int argc, char * const argv[] )
{
	bool_t b;
	ASSERT ( config != NULL );
	
	do {
		b = parse_option_sub ( config, argc, argv );
	} while ( b );

	if ( config_not_given ( config ) ) { 
		print_usage ( argv );
		exit ( 1 );
	}
}

/****************************************************************/

static void
print_parse_failure ( const char *filename, int line_no )
{
	Print ( stderr, "Syntax failure: line %d of file \"%s\" is ignored.\n", line_no, filename );
}

struct token_t {
	char		*start;
	size_t		len;
};

static int
get_next_token ( struct fptr_t buf, size_t offset, struct token_t *token )
{
	size_t i;
	size_t sp;
	char *s = ( char * )buf.base;

	i = offset;
	while ( ( i < buf.offset ) && ( isspace ( s [ i ] ) ) )
		i++;
	
	sp = i;

	if ( i == buf.offset ) 
		return -1;
	
	while ( ( i < buf.offset ) && ( ! isspace ( s [ i ] ) ) )
		i++;

	token->start = ( s + sp );
	token->len = i - sp;

	s [ i ] = '\0';

	return i + 1;
}

static int
get_number ( struct fptr_t buf, size_t offset, int *num )
{
 	struct token_t t;
	size_t current;

	current = get_next_token ( buf, offset, &t );
	if ( current == -1 )
		return -1;

	*num = Atoi ( t.start );

	return current;
}

static int 
get_string ( struct fptr_t buf, size_t offset, char **str )
{
 	struct token_t t;
	size_t current;

	current = get_next_token ( buf, offset, &t );
	if ( current == -1 )
		return -1;

	*str = t.start;

	return current;
}


#include <netdb.h>
#include <assert.h>


static void
parse_cpu ( struct config_t *config, struct fptr_t buf, int line_no, int offset )
{
	int i, p;

	p = offset;
	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		char *s;
		char *q;

		p = get_string ( buf, p, &s );
		if ( p == -1 ) {
			print_parse_failure ( config->config_file, line_no );
			break;
		}
		
		q = strchr ( s, ':' );
		if ( q == NULL ) {
			print_parse_failure ( config->config_file, line_no );
			break;
		}
		
		*q = '\0'; /* replace ':' with '\0' */
		
		config->nodes[i].hostname = Strdup ( s );
		config->nodes[i].port = Atoi ( q + 1 ); 
	}
}

static void
parse_memory ( struct config_t *config, struct fptr_t buf, int line_no, int offset )
{
	char *s;

	offset = get_string ( buf, offset, &s );
	if ( offset == -1 ) {
		print_parse_failure ( config->config_file, line_no ); 
		return;
	}
	config->memory = Strdup ( s );
}

static void
parse_disk ( struct config_t *config, struct fptr_t buf, int line_no, int offset )
{
	char *s;

	offset = get_string ( buf, offset, &s );
	if ( offset == -1 ) {
		print_parse_failure ( config->config_file, line_no );
		return;
	}
	config->disk = Strdup ( s );
}

typedef void parse_func_t ( struct config_t *, struct fptr_t, int, int );

struct keyword_t {
	char		*name;
	parse_func_t	*func;
};

static parse_func_t *
get_parse_func ( const char *s )
{
	struct keyword_t keyword_map [] = 
		{ { "cpu:", &parse_cpu },
		  { "memory:", &parse_memory },
		  { "disk:", &parse_disk }
		};
	const size_t N = sizeof ( keyword_map ) / sizeof ( struct keyword_t );
	int i;

	for ( i = 0; i < N; i++ ) {
		struct keyword_t *x = &keyword_map[i];
	 
		if ( String_equal ( s, x->name ) )
		     return x->func;
	}

	return NULL;
}

static bool_t 
parse_config_line ( struct config_t *config, FILE *fp, int line_no )
{
	enum { BUFSIZE = 1024 };
	char s [ BUFSIZE ];
	struct fptr_t buf = Fptr_create ( s, BUFSIZE );

	char *ret;
	size_t offset;
	char *keyword;
	parse_func_t *f;
	
	Fptr_zero ( buf );
	ret = fgets ( buf.base, buf.offset, fp );
	if ( ret == NULL )
		return FALSE;

	offset = 0;
	offset = get_string ( buf, offset, &keyword );
	if ( offset == -1 ) {
		print_parse_failure ( config->config_file, line_no ); 
		return TRUE;
	}

	f = get_parse_func ( keyword );
	assert ( f != NULL );
	if ( f == NULL ) {
		print_parse_failure ( config->config_file, line_no );
		return TRUE;
	}
	
	( *f ) ( config, buf, line_no, offset );
	return TRUE;
}

static void
parse_config_file ( struct config_t *config )
{
	FILE *fp;
	int line_no;
	bool_t b;

	ASSERT ( config != NULL );

	fp = fopen ( config->config_file, "r" );
	if ( fp == NULL ) {
		Print ( stderr, "Error: invalid config file \"%s\": ", config->config_file );
		perror ( "" );
		exit ( 1 );	
	}
	
	line_no = 1;
	
	do {
		b = parse_config_line ( config, fp, line_no );
		line_no++;
	} while ( b );
}

/****************************************************************/

static void
assert_cpuid ( int cpuid )
{
	if ( ( cpuid >= 0 ) && ( cpuid < NUM_OF_PROCS ) )
		return;

	Print ( stderr,
		"Error: invalid cpuid: %d:"
		" cpuid must be in [%d,%d)\n", 
		cpuid,
		0, NUM_OF_PROCS );
	exit ( 1 );
}

static void
assert_filestat ( const char *confname, const char *filename )
{
	int retval;
	struct stat s;

	retval = stat ( filename, &s );
	if ( retval == 0 )
		return;

	Print ( stderr, "Error: invalid %s file \"%s\": ", confname, filename );
	perror ( "" );
	exit ( 1 );
}

#ifdef ENABLE_MP

static bool_t
is_invalid_node ( struct node_t *node )
{
	return ( ( node->hostname == NULL ) || 
		 ( node->port == -1 ) ||
		 ( InAddr_resolve ( node->hostname ) == INADDR_NONE ) );
}

static void
assert_nodes ( struct node_t nodes[NUM_OF_PROCS] )
{
	int i;

	for ( i = 0; i < NUM_OF_PROCS; i++ ) {
		if ( ! is_invalid_node ( &nodes[i] ) )
			continue;

		Print ( stderr, "Error: invalid node name: hostname=%s, port=%d\n", nodes[i].hostname, nodes[i].port );
		exit ( 1 );			
	}
}

#else /* ! ENABLE_MP */

static void
assert_nodes ( struct node_t nodes[NUM_OF_PROCS] )
{
}

#endif /* ENABLE_MP */

static void
assert_config ( struct config_t *config, char * const argv[] )
{
	ASSERT ( config != NULL );

	assert_cpuid ( config->cpuid );
	assert_filestat ( "disk", config->disk );
	assert_filestat ( "memory", config->memory );
	assert_nodes ( config->nodes );
#if 0
	print_config ( config );
#endif
}

/****************************************************************/

struct config_t *
Config_create ( int argc, char * const argv[] )
{
	struct config_t *config;

	ASSERT ( argv != NULL );
   
	config = Malloct ( struct config_t );

	init_config ( config, argv );
	parse_option ( config, argc, argv );
	parse_config_file ( config );
	assert_config ( config, argv );
	
	return config;
}

