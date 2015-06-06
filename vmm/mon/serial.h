#ifndef _VMM_MON_SERIAL_H
#define _VMM_MON_SERIAL_H

#include "vmm/common.h"


enum {
	NUM_OF_COMS = 4
};

struct com_t {
	bool_t			is_enabled;
	int			in_fd, out_fd;

	bit16u_t		dl;  /* DL (Divisor Latch) */
	bit8u_t			ier; /* IER (Interrupt Enable Register) */
	bit8u_t			fcr; /* FCR (FIFO Control Regiset) */
	bit8u_t			lcr; /* FCR (FIFO Control Regiset) */
	bool_t			dlab;
	bit8u_t			mcr; /* MCR (Modem Control Register */
	bool_t			mcr_out2;
	bit8u_t			scr; /* SCR (Scratch Register) */

	bool_t			txhold_is_enabled;
	bool_t			rxdata_is_enabled;

	bit8u_t			thr_buf;
	bit8u_t			rx_buf;

	bool_t			thr_is_empty;
	bool_t			rxdata_is_ready;
	bool_t			data_is_transferring;
	bool_t			data_is_receiving;

	bool_t			txhold_interrupt;
	bool_t			rxdata_interrupt;

	pthread_t		tid;
	pthread_mutex_t		mp;
	pthread_cond_t		cond;
	pid_t			pid;
};

void    Coms_init ( struct com_t coms[], pid_t pid, bool_t is_bootstrap );
void    Coms_pack ( struct com_t coms[], int fd );
void    Coms_unpack ( struct com_t coms[], int fd );
bit8u_t Coms_read ( struct com_t coms[], bit16u_t addr, size_t len );
void    Coms_write ( struct com_t coms[], bit16u_t addr, bit32u_t val, size_t len );
int     Coms_try_get_irq(struct com_t coms[]);
int     Coms_check_irq ( struct com_t coms[] );


#endif /*_VMM_MON_SERIAL_H */

