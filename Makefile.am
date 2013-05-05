DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc

SUBDIRS = 			\
	gst-libs 		\
	gst		 	\
	common 			\
	m4 			\
	pkgconfig

DIST_SUBDIRS = 			\
	gst-libs		\
	gst			\
	common 			\
	m4 			\
	pkgconfig

EXTRA_DIST = \
	gst-plugins-egl.spec depcomp \
	AUTHORS COPYING NEWS README RELEASE REQUIREMENTS \
	ChangeLog gst-plugins-egl.doap autogen.sh 

DISTCLEANFILES = _stdint.h

ACLOCAL_AMFLAGS = -I m4 -I common/m4

include $(top_srcdir)/common/release.mak

check-valgrind:
	cd tests/check && make check-valgrind

if HAVE_GST_CHECK
check-torture:
	cd tests/check && make torture
else
check-torture:
	true
endif

include $(top_srcdir)/common/coverage/lcov.mak

check: check-exports

# cruft: plugins that have been merged or moved or renamed

CRUFT_FILES = \
	$(top_builddir)/common/shave \
	$(top_builddir)/common/shave-libtool

include $(top_srcdir)/common/cruft.mak

all-local: check-cruft
