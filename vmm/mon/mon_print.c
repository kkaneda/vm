#include "vmm/mon/mon.h"

static const char *
bool_to_string(bool_t x)
{
     return (x) ? "T" : "F";
}

static void
print_eip(FILE *stream, struct user_regs_struct *uregs)
{
     ASSERT(stream != NULL);
     ASSERT(uregs != NULL);

     Print(stream, "EIP: %#x\n", uregs->eip);
}

static const char *
MonMode_to_string(mon_mode_t x)
{
     switch (x) {
     case NATIVE_MODE:    return "Native";
     case EMULATION_MODE: return "Emulation";
     default:		  Match_failure("MonMode_to_string\n");
     }
     Match_failure("MonMode_to_string\n");
     return "";
}

static void
print_mode(FILE *stream, mon_mode_t x)
{
     ASSERT(stream != NULL);

     Print(stream, "Mode: %s\n", MonMode_to_string(x));
}

static void
print_eflags(FILE *stream, struct flag_reg_t *x)
{
     ASSERT(stream != NULL);
     ASSERT(x != NULL);

     Print(stream, "EFLAGS: %#x  (if=%s, df=%s, iopl=%s)\n", 
	    x->val, 
	    bool_to_string(x->interrupt_enable_flag),
	    bool_to_string(x->direction_flag),
	    PrivilegeLevel_to_string(x->iopl));
}

static void
print_gen_regs(FILE *stream, struct user_regs_struct *uregs)
{
     ASSERT(stream != NULL);
     ASSERT(uregs != NULL);

     Print(stream, "eax: 0x%08x  ebx: 0x%08x  ecx: 0x%08x  edx: 0x%08x\n"
	    "esi: 0x%08x  edi: 0x%08x  ebp: 0x%08x  esp: 0x%08x\n",
	    uregs->eax, uregs->ebx, uregs->ecx, uregs->edx,
	    uregs->esi, uregs->edi, uregs->ebp, uregs->esp);
}

static void
print_seg_reg(FILE *stream, struct seg_reg_t *seg, seg_reg_index_t index)
{
     ASSERT(stream != NULL);
     ASSERT(seg != NULL);

     Print(stream, "%s: (0x%04x, %s, %s)",
	    SegRegIndex_to_string(index),
	    seg->selector.index,
	    TblIndicator_to_string(seg->selector.tbl_indicator),
	    PrivilegeLevel_to_string(seg->selector.rpl));
}

static void
print_seg_reg2(FILE *stream, struct seg_reg_t segs[], seg_reg_index_t index)
{
     ASSERT(stream != NULL);
     ASSERT(segs != NULL);

     print_seg_reg(stream, &segs[index], index);
}

static void
print_seg_regs(FILE *stream, struct seg_reg_t segs[])
{
     ASSERT(stream != NULL);
     ASSERT(segs != NULL);

     print_seg_reg2(stream, segs, SEG_REG_CS);
     Print(stream, "  ");
     print_seg_reg2(stream, segs, SEG_REG_DS);
     Print(stream, "\n");
     print_seg_reg2(stream, segs, SEG_REG_ES);
     Print(stream, "  ");
     print_seg_reg2(stream, segs, SEG_REG_FS);
     Print(stream, "\n");
}

static void
print_sys_regs(FILE *stream, struct sys_regs_t *x)
{
     ASSERT(stream != NULL);
     ASSERT(x != NULL);

     Print(stream, "gdtr: base=0x%08x, limit=0x%04x\n", x->gdtr.base, x->gdtr.limit);
     Print(stream, "idtr: base=0x%08x, limit=0x%04x\n", x->idtr.base, x->idtr.limit);
     print_seg_reg(stream, &x->ldtr, SEG_REG_LDTR);
     Print(stream, "  ");
     print_seg_reg(stream, &x->tr, SEG_REG_TR);
     Print(stream, "\n"); 
     Print(stream, 
	   "cr0: 0x%08x, cr1: 0x%08x, cr2: 0x%08x\n"
	   "cr3: 0x%08x, cr4: 0x%08x (PSE=%s)\n",
	   x->cr0.val, x->cr1, x->cr2,
	   x->cr3.val, x->cr4.val,
	   bool_to_string(x->cr4.page_size_extension));
}

void
print_stack(FILE *stream, struct mon_t *mon)
{
     const size_t LEN = 16;
     bit32u_t i;
     int base = (mon->regs->user.esp - 4);

     ASSERT(stream != NULL);
     ASSERT(mon != NULL);

     Print(stream, "Stack:");

     /* [DEBUG] */
     Print ( stream, "DEBUGEND\n" ); return;

     if (base >= VM_PMEM_BASE)
	     return;
     
     for (i = 0; i < LEN; i++) {
	  bit8u_t val;
	  bool_t is_ok;

	  val = Monitor_try_read_byte_with_vaddr(SEG_REG_SS, base + i, &is_ok);
	  
	  if (i % 4 == 0) { Print(stream, "\t(0x%08x)\t", (int)(base + i)); }

	  if (is_ok) {
	       Print(stream, "%02x ", val);
	  } else {
	       Print(stream, "__ ");
	  }
	  
	  if (i % 4 == 3) { Print(stream, "\n"); }
     }
}

static void
print_code(FILE *stream, struct mon_t *mon)
{
     const size_t LEN = 16;
     bit32u_t i;
     int base = (mon->regs->user.eip - 4);

     ASSERT(stream != NULL);
     ASSERT(mon != NULL);

     if (mon->mode == EMULATION_MODE) { return; }

     Print(stream, "Code:");
     for (i = 0; i < LEN; i++) {
	  bit8u_t val;
	  bool_t is_ok;

	  val = Monitor_try_read_byte_with_vaddr(SEG_REG_CS, base + i, &is_ok);
	  
	  if (i % 4 == 0) { Print(stream, "\t(0x%08x)\t", (int)(base + i)); }

	  if (is_ok) {
	       Print(stream, "%02x ", val);
	  } else {
	       Print(stream, "__ ");
	  }
	  
	  if (i % 4 == 3) { Print(stream, "\n"); }
     }
}

void
Monitor_print_simple(FILE *stream, struct mon_t *mon)
{
     ASSERT(stream != NULL);
     ASSERT(mon != NULL);

     Print(stream, "---------------------------------------------------------\n");
//     Print(stream, "SIGNAL: %s\n", Signo_to_string(mon->signo));
     print_eip(stream, &mon->regs->user);
     print_mode(stream, mon->mode);
     Print(stream, "gdtr: base=0x%08x, limit=0x%04x\n", mon->regs->sys.gdtr.base, mon->regs->sys.gdtr.limit);
     print_eflags(stream, &mon->regs->eflags);

     Print(stream, "apic->INTR=%d\n", mon->local_apic->INTR );
}

void
Monitor_print_detail(FILE *stream, struct mon_t *mon)
{
     ASSERT(stream != NULL);
     ASSERT(mon != NULL);

     Print(stream, "---------------------------------------------------------\n");
     Print(stream, "CPL: %s\n", PrivilegeLevel_to_string(cpl(mon->regs)));
     Print(stream, "CPUID: %d\n", mon->cpuid);
//     Print(stream, "SIGNAL: %s\n", Signo_to_string(mon->signo));
     print_eip(stream, &mon->regs->user);
     print_mode(stream, mon->mode);
     print_eflags(stream,&mon->regs->eflags);
     // Print(stream, "EFLAGS(GUEST): %#x\n", mon->regs->user.eflags);
     print_gen_regs(stream, &mon->regs->user);
     print_seg_regs(stream, mon->regs->segs);
     print_sys_regs(stream, &mon->regs->sys);
#if 1
     print_code(stream, mon);
#endif
}

void
Monitor_print(FILE *stream, struct mon_t *mon)
{
     ASSERT(stream != NULL);
     ASSERT(mon != NULL);

     switch (ProcClass_of_cpuid(mon->cpuid)) {
     case BOOTSTRAP_PROC:   Monitor_print_simple(stream, mon); break;
     case APPLICATION_PROC: Monitor_print_detail(stream, mon); break;
     default:		    Match_failure("Monitor_print_status\n");
     }
}

#ifdef DEBUG

void MONITOR_DPRINT(struct mon_t *mon) { Monitor_print(stderr, mon); }

#else /* ! DEBUG */

void MONITOR_DPRINT(struct mon_t *mon) {  }

#endif /* DEBUG */

