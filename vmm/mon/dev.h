#ifndef _VMM_MON_DEV_H
#define _VMM_MON_DEV_H

#define ENABLE_VGA

#include "vmm/common.h"
#include "vmm/mon/vga.h"
#include "vmm/mon/serial.h"
#include "vmm/mon/hard_drive.h"
#include "vmm/mon/pci.h"
#include "vmm/mon/pit.h"
#include "vmm/mon/rtc.h"
#include "vmm/mon/pic.h"


struct devices_t {
	struct vga_t		vga;
	struct pic_t		pic;
	struct rtc_t		rtc;
	struct pit_t		pit;
	struct pci_t		pci;
	struct com_t		coms[NUM_OF_COMS];
	struct hard_drive_t	hard_drive;

	pthread_mutex_t		mp;
	bool_t			is_accessing;

#ifdef ENABLE_MP
//	int			sdevs_sockfds[NUM_OF_PROCS];
	int			remote_irqs[1024];
	int			head_rirq, tail_rirq;
#endif
};


struct mon_t;

void     init_devices(struct mon_t *mon, const struct config_t *config);
bit32u_t inp(struct mon_t *mon, bit16u_t addr, size_t len);
void     outp(struct mon_t *mon, bit16u_t addr, bit32u_t val, size_t len);
int      try_generate_external_irq(struct mon_t *mon, bool_t ignore_irq);
bool_t	 accessing_io(struct mon_t *mon);

bool_t   check_external_irq ( struct mon_t *mon );

void     pack_devices ( struct mon_t *mon, int fd );
void     unpack_devices ( struct mon_t *mon, int fd );

#endif /* _VMM_MON_DEV_H */
