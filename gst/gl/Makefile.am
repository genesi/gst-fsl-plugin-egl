
plugin_LTLIBRARIES = libgstopengl.la

AM_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
AM_LIBS = $(GST_BASE_LIBS)

libgstopengl_la_SOURCES = \
	gstopengl.c \
	gsteglsink.c \
	gsteglsink.h

# check order of CFLAGS and LIBS, shouldn't the order be the other way around
# (like in AM_CFLAGS)?
libgstopengl_la_CFLAGS = $(GST_CFLAGS) $(X_CFLAGS) $(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) $(GL_CFLAGS) $(LIBPNG_CFLAGS) \
	-I$(top_srcdir)/gst-libs \
	-I$(top_srcdir)/gst-libs/gst/gl

libgstopengl_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/gl/libgstegl-$(GST_MAJORMINOR).la \
	$(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstinterfaces-$(GST_MAJORMINOR) \
	$(GL_LIBS) \
	$(LIBPNG_LIBS) \
	$(LIBM)

libgstopengl_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

