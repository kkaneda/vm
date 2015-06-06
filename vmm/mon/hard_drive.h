#ifndef _VMM_MON_HARD_DRIVE_H
#define _VMM_MON_HARD_DRIVE_H

#include "vmm/common.h"

/* [TODO] disk image ごとに変更が必要 
          （bochs と同じ設定にする） */
enum {
	LIMIT_CYLINDER	= 1040,
	LIMIT_HEAD	= 16,
	LIMIT_SECTOR	= 63
};

struct controller_status_t {
	bool_t			busy;
	bool_t			drive_ready;
	bool_t			drive_write_fault;
	bool_t			drive_seek_complete;
	bool_t			drive_request;
	bool_t			corrected_data;
	bool_t			index;
	bool_t			error;
};


enum {
	MAX_CNTLER_BUFSIZE = 512,
	MAX_MULT_SECTORS   = 16
//	MAX_MULT_SECTORS   = 1
};

/* cylinder-head-sector */
struct chs_t {
	bit16u_t		cylinder;	/* 0, ... */
	bit4u_t			head;		/* 0, ... */
	bit8u_t			sector; 	/* 1, ... */
};

union sector_addr_t {
	struct chs_t 		chs;	/* CHS addressing */
	bit32u_t		lba;	/* LBA addressing */
};

struct controller_t {
	bit8u_t			current_command;
	bit8u_t			error_register;
	
	struct controller_status_t status;
	
	bit8u_t			features;
	
	bool_t			reset;
	bool_t			disable_irq;
	
	bool_t			lba_mode;
	union sector_addr_t	addr;
	
	bit8u_t			buffer[MAX_CNTLER_BUFSIZE * MAX_MULT_SECTORS*512 + 4];
	bit32u_t		buffer_index, buffer_size; // buffer 中で，次に読み込みの始まる indexを指す
	
	bit32u_t		sector_count; 
	
	/* controller の状態を管理するのに使用する */
	bool_t			reset_in_progress;
	bool_t 			requesting_irq;
};

struct dma_t {
	bit8u_t 		command;
	bit8u_t 		status;
	bit32u_t 		addr;

	bool_t			has_started;
	mem_access_kind_t	ma_kind;
};

enum {
	NUM_OF_ID_DRIVE = 256
};

struct drive_t {
	int			disk_image_fd;     
	struct controller_t	cntler;
	
	struct chs_t		limit;
	bit16u_t		id_drive[NUM_OF_ID_DRIVE];

	struct dma_t		dma;
};

enum drive_select {
	MASTER_DRIVE = 0,
	SLAVE_DRIVE = 1,
};
typedef enum drive_select	drive_select_t;

enum { 
	NUM_OF_DRIVERS = 2
};

struct ide_channel_t {
	struct drive_t 		drives[NUM_OF_DRIVERS];
	drive_select_t		drive_select;
};

struct hard_drive_t {
	struct ide_channel_t	channel;
};


struct controller_t *get_selected_controller(struct hard_drive_t *x);
void HardDrive_init(struct hard_drive_t *x, const char *disk_file);
void HardDrive_pack ( struct hard_drive_t *x, int fd );
void HardDrive_unpack ( struct hard_drive_t *x, int fd );
bit32u_t HardDrive_read(struct hard_drive_t *x, bit16u_t addr, size_t len);
void HardDrive_write(struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len);
int HardDrive_try_get_irq(struct hard_drive_t *x);
bool_t HardDrive_check_irq ( struct hard_drive_t *x );

bit32u_t HardDriveIoMap_read ( struct hard_drive_t *x, bit16u_t addr, size_t len );
void HardDriveIoMap_write ( struct hard_drive_t *x, bit16u_t addr, bit32u_t val, size_t len );

#endif /*_VMM_MON_HARD_DRIVE_H */

