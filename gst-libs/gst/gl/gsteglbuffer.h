/* 
 * GStreame
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GST_EGL_BUFFER_H_
#define _GST_EGL_BUFFER_H_

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstegltypes.h"

G_BEGIN_DECLS

#define GST_TYPE_EGL_BUFFER (gst_egl_buffer_get_type())

#define GST_IS_EGL_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_EGL_BUFFER))
#define GST_EGL_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_EGL_BUFFER, GstEGLBuffer))

typedef struct _GstEGLBufferPrivate  GstEGLBufferPrivate;
typedef GstEGLTexture* (*GstEGLBufferGenTexture) (gpointer data);
typedef void           (*GstEGLBufferDelTexture) (gpointer data, GstEGLTexture* texinfo);


//A egl buffer has only one texture.

struct _GstEGLBuffer {
  GstBuffer buffer;
  GstGLDisplay *display;
  GstBuffer *attach;
  GstVideoFormat format;
  gint width;
  gint height;
  GstEGLTexture *texinfo;
  GstEGLBufferGenTexture gen;
  GstEGLBufferDelTexture del;
  gpointer client_data;
};

GType gst_egl_buffer_get_type (void);

#define gst_egl_buffer_ref(x) ((GstEGLBuffer *)(gst_buffer_ref((GstBuffer *)(x))))
#define gst_egl_buffer_unref(x) (gst_buffer_unref((GstBuffer *)(x)))

GstEGLBuffer* gst_egl_buffer_new (GstGLDisplay* display, GstEGLBufferGenTexture gen,
        GstEGLBufferDelTexture del, gpointer data, GstVideoFormat format, gint gl_width, gint gl_height);
gboolean gst_egl_buffer_parse_caps (GstCaps* caps, gint* width, gint* height);

/* used by gstgldisplay */
void gst_egl_buffer_attach(GstEGLBuffer *buffer, GstBuffer *attach);

G_END_DECLS

#endif

