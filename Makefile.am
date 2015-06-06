AUX_DIST                = $(ac_aux_dir)/config.guess \
			$(ac_aux_dir)/config.sub \
			$(ac_aux_dir)/install-sh \
			$(ac_aux_dir)/ltconfig \
			$(ac_aux_dir)/ltmain.sh \
			$(ac_aux_dir)/mdate-sh \
			$(ac_aux_dir)/missing \
			$(ac_aux_dir)/mkinstalldirs

EXTRA_DIST		= bootstrap

SUBDIRS 		= vmm 

MAINTAINERCLEANFILES 	= Makefile.in aclocal.m4 configure config-h.in \
			stamp-h.in $(AUX_DIST)

ACLOCAL			= aclocal -I $(ac_aux_dir)

docdir			= $(top_builddir)/doc
