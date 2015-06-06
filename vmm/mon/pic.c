#include "vmm/mon/mon.h"

/* 8259A Programmable Interrupt Controller 
 * [Reference] Intel 8259A PIC Dataseat */

enum {
	SLAVE_IRQ    = 2,
	SPURIOUS_IRQ = 7
};

static void Pic_update_irq ( struct pic_t *pic ) ;

/***********************************************************/

static void
set_priority_add ( struct pic_state_t *x, int irq )
{
	x->priority_add = SUB_BIT ( ( irq + 1 ), 0, 3 );
}

static int 
get_adjusted_priority ( struct pic_state_t *x, int priority )
{	
	return SUB_BIT ( ( x->priority_add + priority ), 0, 3 );
}

/* return the highest priority found in mask (highest = smallest
   number). Return 8 if no irq */
static int
get_priority ( struct pic_state_t *x, int mask )
{
	int priority;

	if ( mask == 0 ) { 
		return 8; 
	}

	priority = 0;
	while ( ( mask & ( 1 << ( get_adjusted_priority ( x, priority ) ) ) ) == 0 ) {
		priority++;
	}

	return priority;
}

static int
get_irr_priority ( struct pic_state_t *x )
{
	int mask;
	
	mask = ( x->irr ) & ( ~x->imr );
	return get_priority ( x, mask );
}

static int
get_isr_priority ( struct pic_state_t *x )
{
	int mask;

	/* compute current priority. If special fully nested mode on the
	   master, the IRQ coming from the slave is not taken into account
	   for the priority computation. */
	mask = x->isr;
	if ( ( x->special_fully_nested_mode ) && ( x->kind == PIC_KIND_MASTER ) ) {
		CLEAR_BIT ( mask, SLAVE_IRQ );
	}

	return get_priority ( x, mask );
}

/* return the pic wanted interrupt. return IRQ_INVALID if none */
static int 
state_get_irq ( struct pic_state_t *x )
{
	int irr_priority;

	irr_priority = get_irr_priority ( x );
	if ( irr_priority == 8 ) {
		return IRQ_INVALID;
	}

	if ( irr_priority >= get_isr_priority ( x ) ) {
		return IRQ_INVALID;
	}

	/* higher priority found: an irq should be generated */
	return get_adjusted_priority ( x, irr_priority );
}

/* set irq level. If an edge is detected, then the IRR is set to 1 */
static void
state_set_irq ( struct pic_state_t *x, int irq, bool_t level )
{
	/* edge triggered */
	
	if ( ! level ) {
		CLEAR_BIT ( x->last_irr, irq );
		return;
	}

	if ( ! TEST_BIT ( x->last_irr, irq ) ) {
		SET_BIT ( x->irr, irq );
	}
	SET_BIT ( x->last_irr, irq );
}

/***********************************************************/

static void
state_reset ( struct pic_state_t *x )
{
	x->irr = 0;
	x->isr = 0;
#if 0
	x->imr = 0;
#else
	x->imr = 0xff; // debug
#endif
	x->read_reg_select = FALSE;
	x->poll = FALSE;
	x->init_state = 0;

	/* TODO */
}

/***********************************************************/

void
Pic_init ( struct pic_t *pic )
{
	int i;
	
	for ( i = 0; i < NR_PIC_STATES; i++ ) {
		struct pic_state_t *x = &pic->states[i];

		state_reset ( x );
		x->kind = i;
	}
}

/***********************************************************/

void 
Pic_pack ( struct pic_t *x, int fd )
{
	Pack ( x, sizeof ( struct pic_t ), fd ); 
}

void 
Pic_unpack ( struct pic_t *x, int fd )
{
	Unpack ( x, sizeof ( struct pic_t ), fd ); 
}

/***********************************************************/

static bit32u_t
state_poll_read ( struct pic_t *pic, pic_kind_t kind, bit16u_t addr )
{
	struct pic_state_t *x = &pic->states[kind];
	int irq;
	
	irq = state_get_irq ( x );
	if ( irq == IRQ_INVALID ) {
		irq = SPURIOUS_IRQ;
		Pic_update_irq ( pic );
		return irq;
	}

	int b = ( addr >> 7 );
	if ( b ) {
		struct pic_state_t *master = &pic->states[PIC_KIND_MASTER];

		CLEAR_BIT ( master->irr, SLAVE_IRQ );
		CLEAR_BIT ( master->isr, SLAVE_IRQ );
	}
	
	CLEAR_BIT ( x->irr, irq );
	CLEAR_BIT ( x->isr, irq );
	
	if ( ( b ) || ( irq != SLAVE_IRQ ) ) {
		Pic_update_irq ( pic );
	}
	
	return irq;
}

static bit32u_t
state_read ( struct pic_t *pic, pic_kind_t kind, bit16u_t addr )
{
	struct pic_state_t *x = &pic->states[kind];

	if ( x->poll ) {
		x->poll = FALSE;
		return state_poll_read ( pic, kind, addr );
	} 

	if ( TEST_BIT ( addr, 0 ) ) {
		return x->imr; 
	} 
		
	return ( x->read_reg_select ) ? x->isr : x->irr;
}

bit32u_t
Pic_read ( struct pic_t *pic, bit16u_t addr, size_t len )
{
	bit32u_t ret = 0;

	ASSERT ( pic != NULL );

	switch ( addr ) {
	case INTERRUPT_CONTROLLER1_PORT ( 0 ):
	case INTERRUPT_CONTROLLER1_PORT ( 1 ):
 		ret = state_read ( pic, PIC_KIND_MASTER, addr );
		break;

	case INTERRUPT_CONTROLLER2_PORT ( 0 ): 
	case INTERRUPT_CONTROLLER2_PORT ( 1 ): 
 		ret = state_read ( pic, PIC_KIND_SLAVE, addr );
		break;

	default:
		Match_failure ( "Pic_read\n" );
	}

	return ret;
}

/***********************************************************/

/* [Reference] p.9 */
static void
state_write_icw1 ( struct pic_state_t *x, bit32u_t val )
{
	state_reset ( x );

	/* deassert a pending interrupt */
	// cpu_reset_interrupt ( cpu_single_env, CPU_INTERRUPT_HARD );
		
	x->init_state = 1;
	x->init4 = TEST_BIT ( val, 0 );

	assert ( ! ( TEST_BIT ( val, 2 ) ) );
	assert ( ! ( TEST_BIT ( val, 3 ) ) );
}

static void
state_write_ocw2_with_eoi_bit ( struct pic_t *pic, pic_kind_t kind, bit32u_t val )
{
	struct pic_state_t *x = &pic->states[kind];
	int irq;

	if ( TEST_BIT ( val, 6 ) ) { /* SL bit */
		/* specific */
		irq = SUB_BIT ( val, 0, 3 );
	} else {
		/* non-specific */
		int priority = get_priority ( x, x->isr );
		if ( priority == 8 ) { return; }
		irq = get_adjusted_priority ( x , priority );
	}

	CLEAR_BIT ( x->isr, irq );
	
	if ( TEST_BIT ( val, 7 ) ) { /* R bit */
		/* rotate on non-specific eoicommand */
		set_priority_add ( x, irq );
	}
	
	Pic_update_irq ( pic );
}

static void
state_write_ocw2_with_no_eoi_bit ( struct pic_t *pic, pic_kind_t kind, bit32u_t val )
{
	struct pic_state_t *x = &pic->states[kind];

	if ( TEST_BIT ( val, 6 ) ) { /* SL bit */
		/* rotate in automatic eoi mode */
		x->rotate_on_auto_eoi = TEST_BIT ( val, 7 ); /* R bit */
		return;
	}

	if ( TEST_BIT ( val, 7 ) ) { /* R bit */
		/* set priority command */
		set_priority_add ( x, val );
		Pic_update_irq ( pic );
	}
}

/* [Reference] p.13 */
static void
state_write_ocw2 ( struct pic_t *pic, pic_kind_t kind, bit32u_t val )
{
	if ( TEST_BIT ( val, 5 ) ) { /* EOI bit */
		state_write_ocw2_with_eoi_bit ( pic, kind, val );
	} else {
		state_write_ocw2_with_no_eoi_bit ( pic, kind, val );
	}
}

/* [Reference] p.14 */
static void
state_write_ocw3 ( struct pic_state_t *x, bit32u_t val )
{
	if ( TEST_BIT ( val, 1 ) ) {
		x->read_reg_select = TEST_BIT ( val, 0 );
	}
	if ( TEST_BIT ( val, 2 ) ) {
		x->poll = TRUE;
	}
	if ( TEST_BIT ( val, 6 ) ) {
		x->special_mask = TEST_BIT ( val, 5 );
	}	
}

static void
state_write_icw1_ocw23 ( struct pic_t *pic, pic_kind_t kind, bit32u_t val )
{
	struct pic_state_t *x = &pic->states[kind];
	int v = SUB_BIT ( val, 3, 2 );
	
	switch ( v ) {
	case 0:  state_write_ocw2 ( pic, kind, val ); break;
	case 1:  state_write_ocw3 ( x, val ); break;
	case 2: 
	case 3:  state_write_icw1 ( x, val ); break;
	default: Match_failure ( "state_write1\n" );
	}
}

static void
state_write_icw234_ocw1 ( struct pic_t *pic, pic_kind_t kind, bit32u_t val )
{
	struct pic_state_t *x = &pic->states[kind];

	switch ( x->init_state ) {
	case 0:
		x->imr = val;
//		Print_color ( stdout, CYAN, "pic [%s] mask: %#x\n", ( ( kind == PIC_KIND_MASTER ) ? "master" : "slave" ), x->imr );
		Pic_update_irq ( pic );
		break;
	case 1:
		// x->irq_base = SUB_BIT ( val, 3, 5 );
		x->irq_base = val & 0xf8;
		x->init_state = 2;
		break;
	case 2:
		x->init_state = ( x->init4 ) ? 3 : 0;
		break;
	case 3:
		x->special_fully_nested_mode = TEST_BIT ( val, 4 );
		x->auto_eoi = TEST_BIT ( val, 1 );
		x->init_state = 0;
		break;
	default:
		Match_failure ( "state_write2\n" );
	}
}

static void
state_write ( struct pic_t *pic, pic_kind_t kind, bit16u_t addr, bit32u_t val )
{
	if ( TEST_BIT ( addr, 0 ) ) {
		state_write_icw234_ocw1 ( pic, kind, val );
	} else {
		state_write_icw1_ocw23 ( pic, kind, val );
	}
}

void
Pic_write ( struct pic_t *pic, bit16u_t addr, bit32u_t val, size_t len )
{
	ASSERT ( pic != NULL );

	switch ( addr ) {
	case INTERRUPT_CONTROLLER1_PORT ( 0 ):
	case INTERRUPT_CONTROLLER1_PORT ( 1 ): 
		state_write ( pic, PIC_KIND_MASTER, addr, val );
		break;

	case INTERRUPT_CONTROLLER2_PORT ( 0 ):
	case INTERRUPT_CONTROLLER2_PORT ( 1 ):
		state_write ( pic, PIC_KIND_SLAVE, addr, val );
		break;

	default:
		Match_failure ( "Pic_write\n" );
	}
}

/***********************************************************/

/* raise irq to CPU if necessary. must be called every time the active
   irq may change */
static void
Pic_update_irq ( struct pic_t *pic ) 
{
	struct pic_state_t *master = &pic->states[PIC_KIND_MASTER];
	struct pic_state_t *slave = &pic->states[PIC_KIND_SLAVE];
	int irq2;

	/* first look at slave pic */
	irq2 = state_get_irq ( slave );
	if ( irq2 >= 0 ) {
		/* if irq request by slave pic, signal master PIC */
		state_set_irq ( master, SLAVE_IRQ, TRUE );
		state_set_irq ( master, SLAVE_IRQ, FALSE );
	}

#if 0
	int irq;
	/* look at requested irq */
	irq = state_get_irq ( master );

	if ( irq >= 0 ) {
		cpu_interrupt(cpu_single_env, CPU_INTERRUPT_HARD);
	}
#endif
}

/***********************************************************/

bool_t
Pic_trigger ( struct pic_t *pic, int irq )
{
	pic_kind_t kind = TEST_BIT ( irq, 3 );
	int i = SUB_BIT ( irq, 0, 3 );

	if (  i && pic->states[ kind ].imr ) {
		/* IRQ |i| is disabled */
		return FALSE;
	}

	state_set_irq ( &pic->states[ kind ], i, TRUE );
	state_set_irq ( &pic->states[ kind ], i, FALSE );
	Pic_update_irq ( pic );

//	Print_color ( stdout, CYAN, "Pic_trigger: irq=%#x\n", irq );

	     return TRUE;
}

/***********************************************************/

static void
state_acknowledge_interrupt ( struct pic_state_t *x, int irq )
{
	if ( x->auto_eoi ) {
		if ( x->rotate_on_auto_eoi ) {
			set_priority_add ( x, irq );
		}
	} else {
		SET_BIT ( x->isr, irq );
	}

	CLEAR_BIT ( x->irr, irq );
}

static int
__pic_try_acknowledge_interrupt ( struct pic_t *pic, pic_kind_t kind )
{
	struct pic_state_t *x = &pic->states[kind];	
	int irq;

	irq = state_get_irq ( x );
	if ( irq == IRQ_INVALID ) { 
		return -1;
	}

	state_acknowledge_interrupt ( x, irq );
 	
	if ( ( irq == SLAVE_IRQ ) && ( kind == PIC_KIND_MASTER ) ) {
		return __pic_try_acknowledge_interrupt ( pic, PIC_KIND_SLAVE );
	}

        // [DEBUG]		
	if ( x->irq_base + irq == 153 ) {
		Print ( stderr, 
			"pic: irq_base = %d, irq = %d\n",
			x->irq_base, irq );
			
	}

	return x->irq_base + irq; 
}

int
Pic_try_acknowledge_interrupt ( struct pic_t *pic )
{
	int ivec;

	ivec = __pic_try_acknowledge_interrupt ( pic, PIC_KIND_MASTER );
	Pic_update_irq ( pic );
	return ivec;
}

/***********************************************************/

static bool_t
__pic_check_interrupt ( struct pic_t *pic, pic_kind_t kind )
{
	struct pic_state_t *x = &pic->states[kind];	
	int irq;

	irq = state_get_irq ( x );
	if ( irq == IRQ_INVALID ) { 
		return FALSE;
	}

	if ( ( irq == SLAVE_IRQ ) && ( kind == PIC_KIND_MASTER ) ) {
		irq = __pic_check_interrupt ( pic, PIC_KIND_SLAVE );
		return ( irq != IRQ_INVALID );
	}

	return TRUE;
}

bool_t
Pic_check_interrupt ( struct pic_t *pic )
{
	return __pic_check_interrupt ( pic, PIC_KIND_MASTER );
}
