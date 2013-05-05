#ifndef __GST_EGL_TYPES_H__
#define __GST_EGL_TYPES_H__

#include <gst/video/video.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

G_BEGIN_DECLS

typedef struct _GstEGLBuffer GstEGLBuffer;
typedef struct _GstGLDisplay GstGLDisplay;

typedef struct {
  GstVideoFormat format;
  GstVideoFormat real_format;
  gint           width;
  gint           height;
  gint           stride;
  GLuint         texture;
  EGLImageKHR    image;
  gpointer       data;    /* virtual */
  gpointer       hw_meta;
} GstEGLTexture;

typedef gboolean      (*EGLSetCapsCB)   (GstCaps *caps, gpointer data);
typedef GstBuffer*    (*EGLGetBufferCB) (GstCaps *caps, guint size, gpointer data);
typedef GstFlowReturn (*EGLDrawCB)      (GstBuffer *buf, gpointer data);

G_END_DECLS

#endif
