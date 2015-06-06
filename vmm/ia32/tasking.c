#include "vmm/ia32/tasking_common.h"

struct tss_t 
Tss_create ( struct seg_descr_t *tssd, trans_t *laddr_to_raddr )
{
	struct tss_t tss;
	int i;

	ASSERT ( tssd != NULL );
     
	tss.previous_task_link = read_word ( tssd->base + 2, laddr_to_raddr );

	for ( i = 0; i < NUM_OF_TSS_ENTRIES; i++ ) {
		bit32u_t p = tssd->base + i * 8;

		tss.esp[i] = read_dword ( p + 4, laddr_to_raddr );
		tss.ss[i]  = read_word ( p + 8, laddr_to_raddr );
	}

	return tss;
}


struct tss_t
get_tss_of_current_task ( const struct regs_t *regs, trans_t *laddr_to_raddr ) 
{
	struct descr_t descr;
	struct seg_descr_t tssd;
	struct tss_t tss;

	ASSERT ( regs != NULL );

	/* Task Regsiter references a TSS descriptor in the
	 * GDT ( The address specifies the linear
	 * address of byte 0 of the TSS. 
	 * [???] cache を参照すれば十分では？
	 */
	descr = Regs_lookup_descr_table ( regs, &regs->sys.tr.selector, laddr_to_raddr );

	tssd = Descr_to_task_state_seg_descr ( &descr );

	tss = Tss_create ( &tssd, laddr_to_raddr );
	return tss;
}
