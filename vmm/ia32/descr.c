#include "descr_common.h"

/* Prototype Declaration */
bit32u_t Descr_base ( const struct descr_t *x );


const char *
PrivilegeLevel_to_string ( privilege_level_t x )
{
	switch ( x ) {
	case SUPERVISOR_MODE: 	return "Kernel";
	case USER_MODE:	  	return "User";
	default: 		Fatal_failure ( "PrivilegeLevel_to_string\n" );
	}
	Fatal_failure ( "PrivilegeLevel_to_string\n" );
	return NULL;
}

const char *
DescrType_to_string ( descr_type_t x )
{
	switch ( x ) {
	case CD_SEG_DESCR: return "Code/Data seg.";
	case SYSTEM_DESCR: return "System";
	default: 	   Fatal_failure ( "DescrType_to_string\n" );
	}
	Fatal_failure ( "DescrType_to_string\n" );
	return NULL;
}

const char *
SysDescrType_to_string ( sys_descr_type_t x )
{
	switch ( x ) {
	case LDT_SEG: 			return "LDT_SEG";
	case AVAILABLE_TASK_STATE_SEG: 	return "AVAILABLE_TSS";
	case BUSY_TASK_STATE_SEG:	return "BUSY_TSS";
	case TASK_GATE:		  	return "TASK_GATE";
	case TRAP_GATE:	  	  	return "TRAP_GATE";
	case CALL_GATE:	  	  	return "CALL_GATE";
	case INTERRUPT_GATE:	  	return "INTERRUPT_GATE";
	default: 			Match_failure ( "SysDescrType_to_string\n" );
	}
	Match_failure ( "SysDescrType_to_string\n" );
	return NULL;
}

struct seg_descr_t
SegDescr_create ( bit32u_t vals[2] )
{
	struct seg_descr_t x;
   
	x.base       = ( SUB_BIT ( vals[0], 16, 16 ) |
			 LSHIFTED_SUB_BIT ( vals[1], 0, 8, 16 ) |
			 LSHIFTED_SUB_BIT ( vals[1], 24, 8, 24 ) );
	
	x.limit      = ( SUB_BIT ( vals[0], 0, 16 ) | LSHIFTED_SUB_BIT ( vals[0], 16, 4, 16 ) );
	x.dpl        = SUB_BIT ( vals[1], 13, 2 );
	x.present    = SUB_BIT ( vals[1], 15, 1 );
	x.granuality = SUB_BIT ( vals[1], 23, 1 );
	return x;
}

struct cd_seg_descr_t
CdSegDescr_create ( bit32u_t vals[2] )
{
	struct cd_seg_descr_t x;

	x.seg        = SegDescr_create ( vals );
	x.executable = SUB_BIT ( vals[1], 11, 1 );
	x.c_ed       = SUB_BIT ( vals[1], 10, 1 );
	x.read_write = SUB_BIT ( vals[1], 9, 1 );
	x.accessed   = SUB_BIT ( vals[1], 8, 1 );

	return x;
}

struct gate_descr_t
GateDescr_create ( bit32u_t vals[2] )
{
	struct gate_descr_t x;

	x.param_count = SUB_BIT ( vals[1], 0, 5 );
	x.type        = SUB_BIT ( vals[1], 8, 4 );
	x.dpl         = SUB_BIT ( vals[1], 13, 2 );
	x.present     = SUB_BIT ( vals[1], 15, 1 );

	x.selector    = SUB_BIT ( vals[0], 16, 16 );
	x.offset      = ( SUB_BIT ( vals[0], 0, 16 ) | LSHIFTED_SUB_BIT ( vals[1], 16, 16, 16 ) );

	return x;
}

struct sys_descr_t
SysDescr_create ( bit32u_t vals[2], bool_t *is_valid )
{
	struct sys_descr_t x;
	ASSERT ( is_valid != NULL );
   
	x.type = SUB_BIT ( vals[1], 8, 4 );
	switch ( x.type ) {
	case LDT_SEG:
	case AVAILABLE_TASK_STATE_SEG:
	case BUSY_TASK_STATE_SEG:
		x.u.seg = SegDescr_create ( vals );
		break;

	case TASK_GATE:
	case TRAP_GATE: 
	case CALL_GATE:
	case INTERRUPT_GATE:
		x.u.gate = GateDescr_create ( vals );
		break;

	default:
		*is_valid = FALSE; /* Match_failure ( "SysDescr_create: x->type=%#x\n", x.type ); */
	}

	return x;
}

struct descr_t
Descr_of_bit64 ( bit32u_t vals[2] )
{
	int i;
	struct descr_t x;

	for ( i = 0; i < 2; i++ ) 
		x.vals[i] = vals[i];

	x.is_valid = TRUE;
   
	x.type = SUB_BIT ( vals[1], 12, 1 );
	switch ( x.type ) {
	case CD_SEG_DESCR: x.u.cd_seg = CdSegDescr_create ( vals ); break;
	case SYSTEM_DESCR: x.u.sys = SysDescr_create ( vals, &x.is_valid ); break;
	default:	   Match_failure ( "Descr_create\n" );
	}
	//ASSERT ( ( x.is_valid == FALSE ) || ( Descr_base ( &x ) == 0 ) );
	return x;
}

static void
SegDescr_print ( FILE *stream, const struct seg_descr_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream, "{ base=%#x, limit=%#x }", x->base, x->limit );   
}

static void
CdSegDescr_print ( FILE *stream, const struct cd_seg_descr_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );
	SegDescr_print ( stream, &x->seg );
}

static void
GateDescr_print ( FILE *stream, const struct gate_descr_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );
  
	Print ( stream, "{ selector=%#x, offset=%#x }", x->selector, x->offset );
}

static void
SysDescr_print ( FILE *stream, const struct sys_descr_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream, " { type=%s, ", SysDescrType_to_string ( x->type ) );

	switch ( x->type ) {
	case LDT_SEG:
	case AVAILABLE_TASK_STATE_SEG:
	case BUSY_TASK_STATE_SEG:
		SegDescr_print ( stream, &x->u.seg );
		break;
	 
	case TASK_GATE:
	case TRAP_GATE: 
	case CALL_GATE:
	case INTERRUPT_GATE:
		GateDescr_print ( stream, &x->u.gate );
		break;

	default:
		Match_failure ( "SysDescr_print\n" );
	}
	Print ( stream, " }" );
}


void
Descr_print ( FILE *stream, const struct descr_t *x ) 
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	if ( ! x->is_valid ) {
		Print ( stream, "{ INVALID }\n" ); 
		return;
	}

	Print ( stream, "{ type=%s, ", DescrType_to_string ( x->type ) );

	switch ( x->type ) {
	case CD_SEG_DESCR: CdSegDescr_print ( stream, &x->u.cd_seg ); break;
	case SYSTEM_DESCR: SysDescr_print ( stream, &x->u.sys ); break;
	default:  	Match_failure ( "Descr_print\n" );
	}

	Print ( stream, " }\n" );
}

#ifdef DEBUG

void DESCR_DPRINT ( const struct descr_t *x ) { Descr_print ( stderr, x ); }

#else

void DESCR_DPRINT ( const struct descr_t *x ) { }

#endif

static bit32u_t
SegDescr_base ( const struct seg_descr_t *x )
{
	ASSERT ( x != NULL );
	return x->base;
}

static bit32u_t
CdSegDescr_base ( const struct cd_seg_descr_t *x )
{
	ASSERT ( x != NULL );
	return SegDescr_base ( &x->seg );
}

static bit32u_t
SysDescr_base ( const struct sys_descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( ( x->type == LDT_SEG ) ||
		 ( x->type == AVAILABLE_TASK_STATE_SEG ) || 
		 ( x->type == BUSY_TASK_STATE_SEG ) );
	return SegDescr_base ( &x->u.seg );
}

bit32u_t 
Descr_base ( const struct descr_t *x )
{
	bit32u_t retval = 0;

	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );

	switch ( x->type ) {
	case CD_SEG_DESCR: retval = CdSegDescr_base ( &x->u.cd_seg ); break;
	case SYSTEM_DESCR: retval = SysDescr_base ( &x->u.sys ); break;
	default:   	   Match_failure ( "Descr_base\n" );
	}

	/* [Note] Linux only uses the descriptors of which base is 0 */
	if ( retval != 0 ) {
//		Fatal_failure ( "Descr_base: base is not zero but %#x\n", retval );
		Warning ( "Descr_base: base is not zero but %#x\n", retval );
	}
	
	return retval;
}


static bit32u_t
SegDescr_limit ( const struct seg_descr_t *x )
{
	ASSERT ( x != NULL );
	return x->limit;
}

static bit32u_t
CdSegDescr_limit ( const struct cd_seg_descr_t *x )
{
	ASSERT ( x != NULL );
	return SegDescr_limit ( &x->seg );
}

static bit32u_t
SysDescr_limit ( const struct sys_descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( ( x->type == LDT_SEG ) ||
		 ( x->type == AVAILABLE_TASK_STATE_SEG ) || 
		 ( x->type == BUSY_TASK_STATE_SEG ) );
	return SegDescr_limit ( &x->u.seg );
}

bit32u_t 
Descr_limit ( const struct descr_t *x )
{
	bit32u_t retval = 0;

	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );

	switch ( x->type ) {
	case CD_SEG_DESCR: retval = CdSegDescr_limit ( &x->u.cd_seg ); break;
	case SYSTEM_DESCR: retval = SysDescr_limit ( &x->u.sys ); break;
	default:   	   Match_failure ( "Descr_limit\n" );
	}
	
	return retval;
}

void
Descr_set_sys_descr_type ( struct descr_t *x, sys_descr_type_t type )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );

	CLEAR_BITS ( x->vals[1], 8, 4 );
	SET_BITS ( x->vals[1], 8, 4, type );
	*x = Descr_of_bit64 ( x->vals );
}

struct cd_seg_descr_t
Descr_to_cd_seg_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == CD_SEG_DESCR );
	return x->u.cd_seg;
}

struct gate_descr_t
Descr_to_gate_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( ( x->u.sys.type == TASK_GATE ) ||
		 ( x->u.sys.type == TRAP_GATE ) ||
		 ( x->u.sys.type == CALL_GATE ) ||
		 ( x->u.sys.type == INTERRUPT_GATE ) );
	return x->u.sys.u.gate;
}

struct gate_descr_t
Descr_to_interrupt_gate_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( x->u.sys.type == INTERRUPT_GATE );
	return x->u.sys.u.gate;
}

struct seg_descr_t 
Descr_to_task_state_seg_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( ( x->u.sys.type == AVAILABLE_TASK_STATE_SEG ) ||
		 ( x->u.sys.type == BUSY_TASK_STATE_SEG ) );
	return x->u.sys.u.seg;
}

struct seg_descr_t 
Descr_to_ldt_seg_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( x->u.sys.type == LDT_SEG );
	return x->u.sys.u.seg;
}

struct seg_descr_t 
Descr_to_available_task_state_seg_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( x->u.sys.type == AVAILABLE_TASK_STATE_SEG );
	return x->u.sys.u.seg;
}

struct seg_descr_t 
Descr_to_busy_task_state_seg_descr ( struct descr_t *x )
{
	ASSERT ( x != NULL );
	ASSERT ( x->is_valid );
	assert ( x->type == SYSTEM_DESCR );
	assert ( x->u.sys.type == BUSY_TASK_STATE_SEG );
	return x->u.sys.u.seg;
}
