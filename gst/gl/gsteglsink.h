/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
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

#ifndef _EGLSINK_H_
#define _EGLSINK_H_

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include "gstegltypes.h"

GST_DEBUG_CATEGORY_EXTERN (gst_debug_egl_sink);

#define GST_TYPE_EGL_SINK \
    (gst_egl_sink_get_type())
#define GST_EGL_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EGL_SINK,GstEGLSink))
#define GST_EGL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EGL_SINK,GstEGLSinkClass))
#define GST_IS_EGL_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EGL_SINK))
#define GST_IS_EGL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EGL_SINK))

typedef struct _GstEGLSink GstEGLSink;
typedef struct _GstEGLSinkClass GstEGLSinkClass;

struct _GstEGLSink
{
    GstVideoSink video_sink;

    //properties
    gchar *display_name;

    gulong window_id;
    gulong new_window_id;

    //caps
    GstCaps *caps;
    gint window_width;
    gint window_height;
    gint fps_n;
    gint fps_d;
    gint par_n;
    gint par_d;

    //callback
	EGLSetCapsCB set_caps_callback;
	EGLGetBufferCB get_buffer_callback;
	EGLDrawCB draw_callback;
	gpointer client_data;

    GstGLDisplay *display;
    gboolean keep_aspect_ratio;
    GValue *par;

    gint show_count;
};

struct _GstEGLSinkClass
{
    GstVideoSinkClass video_sink_class;
};

GType gst_egl_sink_get_type(void);

#endif

