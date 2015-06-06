include $(top_srcdir)/config/Make-rules

bin_PROGRAMS	= mon
mon_SOURCES	= instr.c stat.c init.c mon_maccess.c mon_print.c decode.c \
		  pci.c vga.c hard_drive.c serial.c pit.c pic.c rtc.c dev.c \
		  guest.c \
		  arith.c bit.c logical.c stack.c shift.c io.c \
		  ctrl_xfer.c data_xfer.c string.c \
		  flag_ctrl.c proc_ctrl.c protect_ctrl.c segment_ctrl.c \
		  shmem.c apic.c mhandler.c snapshot.c main.c
mon_LDADD	= @LIBS@ ../std/libstd.la ../ia32/libia32.la ../comm/libcomm.la
