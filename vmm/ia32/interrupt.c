#include "vmm/ia32/interrupt_common.h"
#include "vmm/ia32/regs.h"
#include "vmm/ia32/maccess.h"
#include "vmm/ia32/tasking.h"

const char *
DeliveryMode_to_string ( delivery_mode_t x )
{
	switch ( x ) {
	case DELIVERY_MODE_FIXED:		return "FIXED";
	case DELIVERY_MODE_LOWEST_PRIORITY:	return "LOWEST_PRIORITY";
	case DELIVERY_MODE_SMI:		 	return "SMI";
	case DELIVERY_MODE_NMI:		 	return "NMI";
	case DELIVERY_MODE_INIT: 		return "INIT";
	case DELIVERY_MODE_STARTUP: 	 	return "STARTUP";
	case DELIVERY_MODE_EXTINT:		return "EXTINT";
	default:		 		Match_failure ( "DeliveryMode_to_string\n" );
	}
	Match_failure ( "DeliveryMode_to_string\n" );
	return "";
}

const char *
DestMode_to_string ( dest_mode_t x )
{
	switch ( x ) {
	case DEST_MODE_PHYSICAL: return "PHYSICAL";
	case DEST_MODE_LOGICAL:  return "LOGICAL";
	default:         	 Match_failure ( "DestMode_to_string\n" );
	}
	Match_failure ( "DestMode_to_string\n" );
	return "";
}

const char *
Level_to_string ( level_t x )
{
	switch ( x ) {
	case LEVEL_DEASSERT: 	return "DEASSERT";
	case LEVEL_ASSERT:  	return "ASSERT";
	default:       		Match_failure ( "Level_to_string\n" );
	}
	Match_failure ( "Level_to_string\n" );
	return "";
}

const char *
TrigMode_to_string ( trig_mode_t x )
{
	switch ( x ) {
	case TRIG_MODE_EDGE:  	return "EDGE";
	case TRIG_MODE_LEVEL: 	return "LEVEL";
	default:       		Match_failure ( "TrigMode_to_string\n" );
	}
	Match_failure ( "TrigMode_to_string\n" );
	return "";
}

const char *
DestShorthand_to_string ( dest_shorthand_t x )
{
	switch ( x ) {
	case DEST_SHORTHAND_NO: 	        return "NO";
	case DEST_SHORTHAND_SELF:       	return "SELF";
	case DEST_SHORTHAND_ALL_INCLUDING_SELF: return "ALL_INCLUDING_SELF";
	case DEST_SHORTHAND_ALL_EXCLUDING_SELF: return "ALL_EXCLUDING_SELF";
	default:                		Match_failure ( "DestShorthand_to_string\n" );
	}
	Match_failure ( "DestShorthand_to_string\n" );
	return "";
}

struct interrupt_command_t
InterruptCommand_of_bit64u ( bit32u_t vals[2] )
{
	struct interrupt_command_t x;

	x.vector         = SUB_BIT ( vals[0], 0, 8 );
	x.delivery_mode  = SUB_BIT ( vals[0], 8, 3 );
	x.dest_mode      = SUB_BIT ( vals[0], 11, 1 );
	x.level          = SUB_BIT ( vals[0], 14, 1 );
	x.trig_mode      = SUB_BIT ( vals[0], 15, 1 );
	x.dest_shorthand = SUB_BIT ( vals[0], 18, 2 );
	x.dest           = SUB_BIT ( vals[1], 24, 8 );
	return x;
}

void
InterruptCommand_print ( FILE *stream, struct interrupt_command_t *x )
{
	ASSERT ( stream != NULL );
	ASSERT ( x != NULL );

	Print ( stream,
		"{ vector=%#x, delivery_mode=%s, dest_mode=%s,\n"
		" level=%s, trig_mode=%s, dest_shorthand=%s,\n"
		" dest=%#x\n",
		x->vector, 
		DeliveryMode_to_string ( x->delivery_mode ), 
		DestMode_to_string ( x->dest_mode ),
		Level_to_string ( x->level ),
		TrigMode_to_string ( x->trig_mode ),
		DestShorthand_to_string ( x->dest_shorthand ),
		x->dest );
}

#ifdef DEBUG

void INTERRUPT_COMMAND_DPRINT ( struct interrupt_command_t *x ) { InterruptCommand_print ( stderr, x ); }

#else

void INTERRUPT_COMMAND_DPRINT ( struct interrupt_command_t *x ) { }

#endif

unsigned int
irq_to_ivector ( unsigned int irq )
{
	return 0x20 + irq;
}

void
enable_interrupt ( struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	FlagReg_set_interrupt_enable_flag ( &regs->eflags );
}

void
disable_interrupt ( struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	FlagReg_clear_interrupt_enable_flag ( &regs->eflags );
}

bool_t
interrupt_is_enabled ( struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	return regs->eflags.interrupt_enable_flag;
}

bool_t
interrupt_is_disabled ( struct regs_t *regs )
{
	ASSERT ( regs != NULL );
	return ( !regs->eflags.interrupt_enable_flag );
}

/* [Reference] IA-32 manual Vol.3 5-3 */
bool_t
interrupt_has_error_code ( int ivector )
{
	bool_t retval = FALSE;

	switch ( ivector ) {
	case IVECTOR_PAGEFAULT: retval = TRUE; break;
	default:		Match_failure ( "interrupt_has_error_code\n" );
	};

	return retval;
}

static void 
create_kernel_level_stack ( struct regs_t *regs,
			 void ( *pushl ) ( struct regs_t *, bit32u_t ),
			 trans_t laddr_to_raddr )
{
	bit16u_t saved_ss;
	bit32u_t saved_esp;
	struct tss_t tss;

	ASSERT ( regs != NULL );

	saved_ss = Regs_get_seg_reg ( regs, SEG_REG_CS );
	saved_esp = regs->user.esp;

	tss = get_tss_of_current_task ( regs, laddr_to_raddr );
	
	Regs_set_seg_reg2 ( regs, SEG_REG_SS, tss.ss[SUPERVISOR_MODE], laddr_to_raddr ); 
	regs->user.esp = tss.esp[SUPERVISOR_MODE];

	pushl ( regs, saved_ss );
	pushl ( regs, saved_esp );
}


/* [Reference] Linux kernel pp.133~135 */
void
raise_interrupt ( struct regs_t *regs, 
		  unsigned int ivector, 
		  void ( *pushl ) ( struct regs_t *, bit32u_t ),
		  trans_t *laddr_to_raddr )
{
	struct descr_t descr;
	struct gate_descr_t gd;

	ASSERT ( regs != NULL );

	/* [TODO] Check the validity of this interrupt */

	if ( cpl_is_user_mode ( regs ) )
		create_kernel_level_stack ( regs, pushl, laddr_to_raddr );

	pushl ( regs, regs->eflags.val ); 
	pushl ( regs, Regs_get_seg_reg ( regs, SEG_REG_CS ) );
	pushl ( regs, regs->user.eip );

	descr = lookup_idt ( regs, ivector, laddr_to_raddr );
	gd = Descr_to_gate_descr ( &descr );

	Regs_set_seg_reg2 ( regs, SEG_REG_CS, gd.selector, laddr_to_raddr ); 
	regs->user.eip = gd.offset;

	/* [TODO] Clear the TF flag.  On calls to exception and
	 * interrupt handlers, the processor also clears the VM, RF,
	 * and NT flags. */

	/* [NOTE] When accessing an exception- or interrupt-handling
	 * procedure through an interrupt gate, the processor clears
	 * the IF flag to prevent other interrupts from interfering
	 * with the current interrupt handler.  Accessing a handler
	 * procedure through a trap gate does not affect the IF
	 * flag */
	if ( gd.type == INTERRUPT_GATE )
		disable_interrupt ( regs );
}
