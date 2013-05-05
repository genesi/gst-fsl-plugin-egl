#ifndef __GST_EGL_PLATFORM_H__
#define __GST_EGL_PLATFORM_H__

#include "gstegltypes.h"

G_BEGIN_DECLS

gchar *               gst_egl_platform_get_vertex_source(GstVideoFormat format);
gchar *               gst_egl_platform_get_fragment_source(GstVideoFormat format);

gint                  gst_egl_platform_get_alignment_h(GstVideoFormat format);
gint                  gst_egl_platform_get_alignment_v(GstVideoFormat format);

void                  gst_egl_platform_get_format_info(GstVideoFormat videoformat,
                                                       GLenum *internalformat,
                                                       GLenum *format,
                                                       GLenum *type);

void                  gst_egl_platform_alloc_image(EGLDisplay display, GstEGLTexture *info);
void                  gst_egl_platform_free_image(EGLDisplay display, GstEGLTexture *info);

GLenum                gst_egl_platform_get_target(GstVideoFormat videoformat);

gboolean              gst_egl_platform_accept_caps(GstVideoFormat format, gint width, gint height);

gboolean              gst_egl_platform_convert_color_space(gpointer src, GstVideoFormat srcfmt, gpointer dst,
		                      GstVideoFormat dstfmt, gint width, gint height, gint stride);

G_END_DECLS

#endif
