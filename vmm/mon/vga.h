#ifndef _VMM_MON_VGA_H
#define _VMM_MON_VGA_H

#include "vmm/common.h"

// #define ENABLE_CURSES

#ifdef ENABLE_CURSES
#include <curses.h>
#endif



struct vga_t {
	int 		fd;
	int		base_y;

#ifdef ENABLE_CURSES
	WINDOW 		*subwin;
#endif
};


void Vga_init ( struct vga_t *x, int cpuid, bit32u_t pmem_base );
void Vga_destroy ( struct vga_t *x );

struct mon_t;

void write_to_vram(struct mon_t *mon, bit32u_t paddr, size_t len);


#endif /*_VMM_MON_VGA_H */

