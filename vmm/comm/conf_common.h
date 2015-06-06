#ifndef _VMM_COMM_CONF_COMMON_H
#define _VMM_COMM_CONF_COMMON_H


#include "vmm/std.h"


enum {
#ifdef ENABLE_MP 
//       NUM_OF_PROCS = 8
//       NUM_OF_PROCS = 4
       NUM_OF_PROCS = 2
#else
     NUM_OF_PROCS = 1
#endif
};

struct node_t {
	char 		*hostname;
	int		port; 
};

struct config_t {
	int		cpuid;
	char		*config_file;

	char 		*disk;
	char 		*memory;

	char		*dirname;
	char		*snapshot;

	struct node_t 	nodes[NUM_OF_PROCS]; 
};

#endif /* _VMM_COMM_CONF_COMMON_H */
