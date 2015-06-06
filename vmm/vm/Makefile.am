include $(top_srcdir)/config/Make-rules

LDFLAGS         = $(CFLAGS) -Wl,--script=linker_script --static

bin_PROGRAMS	= vm
vm_SOURCES	= init.c vm_maccess.c main.c
vm_LDADD	= @LIBS@ ../std/libstd.la ../ia32/libia32.la ../comm/libcomm.la
