/*
 * GStreamer
 * Copyright (C) 2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef __GST_GL_H__
#define __GST_GL_H__

#include <gst/video/video.h>

#include "gstegltypes.h"
#include "gstglwindow.h"
#include "gstglshader.h"

G_BEGIN_DECLS

#define GST_TYPE_GL_DISPLAY			\
  (gst_gl_display_get_type())
#define GST_GL_DISPLAY(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY,GstGLDisplay))
#define GST_GL_DISPLAY_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_DISPLAY,GstGLDisplayClass))
#define GST_IS_GL_DISPLAY(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY))
#define GST_IS_GL_DISPLAY_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_DISPLAY))

#define GST_GL_DISPLAY_MAX_BUFFER_COUNT		(32)

typedef struct _GstGLDisplayClass GstGLDisplayClass;

typedef void (*GstGLDisplayThreadFunc) (GstGLDisplay * display, gpointer data);

struct _GstGLDisplay
{
  GObject object;

  //thread safe
  GMutex *mutex;

  //gl context
  GThread *gl_thread;
  GstGLWindow *gl_window;
  gboolean isAlive;

  //conditions
  GCond *cond_create_context;
  GCond *cond_destroy_context;

  //buffer management
  gint  alloc_count;
  GList *free_textures;
  GMutex *texlock;
  GCond *cond_tex;
  GCond *cond_disp;
  GstEGLBuffer *todraw;
  GstEGLBuffer *drawing;

  //action redisplay
  gboolean keep_aspect_ratio;
  GstVideoFormat redisplay_format;
  GstGLShader *redisplay_shader;
  GLint redisplay_attr_position_loc;
  GLint redisplay_attr_texture_loc;
  gint  window_width;
  gint  window_height;
  gint  tex_width;
  gint  tex_height;
  gint  crop_left;
  gint  crop_right;
  gint  crop_top;
  gint  crop_bottom;
  gfloat sampler_left;
  gfloat sampler_right;
  gfloat sampler_top;
  gfloat sampler_bottom;
  gchar *vertex_src;
  gchar *fragment_src;

  //foreign gl context
  gulong external_gl_context;
};


struct _GstGLDisplayClass
{
  GObjectClass object_class;
};

GType gst_gl_display_get_type (void);


//------------------------------------------------------------
//-------------------- Public declarations ------------------
//------------------------------------------------------------
GstGLDisplay *gst_gl_display_new (void);

void gst_gl_display_create_context (GstGLDisplay * display,
    gulong external_gl_context);
void gst_gl_display_destroy_context (GstGLDisplay * display);
GstEGLBuffer *gst_gl_display_get_free_buffer(GstGLDisplay * display,
    GstCaps *caps, guint size, gboolean check_platform);
gboolean gst_gl_display_redisplay (GstGLDisplay * display, GstEGLBuffer *buffer,
    gint window_width, gint window_height, gboolean keep_aspect_ratio);

void gst_gl_display_gen_texture (GstGLDisplay * display, GstEGLBuffer *buffer);
void gst_gl_display_del_texture (GstGLDisplay * display, GstEGLBuffer *buffer);

gboolean gst_gl_display_do_upload (GstGLDisplay * display, GstEGLBuffer *buffer, GstBuffer *src);

void gst_gl_display_set_window_id (GstGLDisplay * display, gulong window_id);

gulong gst_gl_display_get_internal_gl_context (GstGLDisplay * display);
void gst_gl_display_activate_gl_context (GstGLDisplay * display, gboolean activate);

G_END_DECLS

#endif
