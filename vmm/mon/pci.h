#ifndef _VMM_MON_PCI_H
#define _VMM_MON_PCI_H

#include "vmm/common.h"

enum {
	MAX_PCI_DEVS = 256,
	NR_PCI_DEV_CONFIG = 256
};


#if 0
typedef struct PCIIORegion {
    uint32_t addr; /* current PCI mapping address. -1 means not mapped */
    uint32_t size;
    uint8_t type;
    PCIMapIORegionFunc *map_func;
} PCIIORegion;
#endif


struct pci_dev_t {
	bit8u_t		config[NR_PCI_DEV_CONFIG];
	char 		*name;
};


/* PCI Bus */
struct pci_t {
	bit32u_t 	config_reg;
	struct pci_dev_t *devs[MAX_PCI_DEVS];
};


void     Pci_init ( struct pci_t *x );
void     Pci_pack ( struct pci_t *x, int fd );
void     Pci_unpack ( struct pci_t *x, int fd );
bit32u_t Pci_read ( struct pci_t *x, bit16u_t addr, size_t len );
void     Pci_write ( struct pci_t *x, bit16u_t addr, bit32u_t val, size_t len );


#endif /*_VMM_MON_PCI_H */

