#include "vmm/ia32/cpu_common.h"

proc_class_t
ProcClass_of_cpuid ( int cpuid )
{
	return ( cpuid == BSP_CPUID ) ? BOOTSTRAP_PROC : APPLICATION_PROC;
}
