
lib_LTLIBRARIES = libgstegl-@GST_MAJORMINOR@.la

EXTRA_DIST = \
	gstglwindow_eglx.c \
        gsteglplatform_fsl_mx5.c

libgstegl_@GST_MAJORMINOR@_la_SOURCES = \
	gstgldisplay.c \
	gsteglbuffer.c \
	gstglshader.c

libgstegl_@GST_MAJORMINOR@_la_SOURCES += gstglwindow_eglx.c gsteglplatform_fsl_mx5.c

libgstegl_@GST_MAJORMINOR@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/gl
libgstegl_@GST_MAJORMINOR@include_HEADERS = \
        gstegltypes.h \
        gsteglplatform.h \
	gstglwindow.h \
	gstgldisplay.h \
	gsteglbuffer.h \
	gstgles2.h \
	gstglshader.h

libgstegl_@GST_MAJORMINOR@_la_LIBADD = \
        $(GST_FSL_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstcontroller-$(GST_MAJORMINOR) \
	$(GST_BASE_LIBS) $(GST_LIBS) \
	$(GL_LIBS)

libgstegl_@GST_MAJORMINOR@_la_CFLAGS = \
        $(GST_FSL_BASE_CFLAGS) \
	$(GL_CFLAGS) $(X_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstegl_@GST_MAJORMINOR@_la_OBJCFLAGS = \
        $(GST_FSL_BASE_CFLAGS) \
	$(GL_CFLAGS) $(X_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)

libgstegl_@GST_MAJORMINOR@_la_LDFLAGS = \
	$(GST_LIB_LDFLAGS) $(GST_ALL_LDFLAGS) $(GST_LT_LDFLAGS)

libgstegl_@GST_MAJORMINOR@_la_LIBTOOLFLAGS = --tag=CC
