/*
 * GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gsteglbuffer.h"
#include "gstgldisplay.h"

GST_DEBUG_CATEGORY_STATIC (gst_egl_buffer_debug);
#define GST_CAT_DEFAULT gst_egl_buffer_debug

static GObjectClass *gst_egl_buffer_parent_class;

static void
gst_egl_buffer_finalize (GstEGLBuffer * buffer)
{
  if(buffer->del)
    buffer->del(buffer->client_data, buffer->texinfo);
  else if(buffer->display)
    gst_gl_display_del_texture(buffer->display, buffer);
  if(buffer->display)
    g_object_unref (buffer->display);
  gst_egl_buffer_attach(buffer, NULL);
  GST_MINI_OBJECT_CLASS (gst_egl_buffer_parent_class)->finalize (GST_MINI_OBJECT(buffer));
}

static void
gst_egl_buffer_init (GstEGLBuffer * buffer, gpointer g_class)
{
  buffer->texinfo = NULL;
  buffer->attach = NULL;
}

static void
gst_egl_buffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  gst_egl_buffer_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_egl_buffer_finalize;
  GST_DEBUG_CATEGORY_INIT (gst_egl_buffer_debug, "eglbuffer", 0, "egl buffer");
}


GType
gst_egl_buffer_get_type (void)
{
  static GType _gst_egl_buffer_type;

  if (G_UNLIKELY (_gst_egl_buffer_type == 0)) {
    static const GTypeInfo info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      gst_egl_buffer_class_init,
      NULL,
      NULL,
      sizeof (GstEGLBuffer),
      0,
      (GInstanceInitFunc) gst_egl_buffer_init,
      NULL
    };
    _gst_egl_buffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "GstEGLBuffer", &info, 0);
  }
  return _gst_egl_buffer_type;
}

GstEGLBuffer *
gst_egl_buffer_new (GstGLDisplay * display, GstEGLBufferGenTexture gen, 
		GstEGLBufferDelTexture del, gpointer data, GstVideoFormat format, gint gl_width, gint gl_height)
{
  GstEGLTexture *info;
  GstEGLBuffer *egl_buffer =
      (GstEGLBuffer *) gst_mini_object_new (GST_TYPE_EGL_BUFFER);
  egl_buffer->width = gl_width;
  egl_buffer->height = gl_height;
  egl_buffer->format = format;
  egl_buffer->gen = gen;
  egl_buffer->del = del;
  egl_buffer->client_data = data;
  
  egl_buffer->display = display ? g_object_ref (display) : NULL;

  if(gen)
    egl_buffer->texinfo = gen(data);
  else if(display)
    gst_gl_display_gen_texture(display, egl_buffer);
  
  info = egl_buffer->texinfo;
  if(info)
  {
    gint index;
    index = G_N_ELEMENTS(GST_BUFFER(egl_buffer)->_gst_reserved)-1;
	GST_BUFFER_DATA(egl_buffer) = info->data;
	GST_BUFFER_SIZE(egl_buffer) = gst_video_format_get_size (info->format, info->width, info->height);
	GST_BUFFER(egl_buffer)->_gst_reserved[index] = info->hw_meta;
  }
  return egl_buffer;
}

gboolean
gst_egl_buffer_parse_caps (GstCaps * caps, gint * width, gint * height)
{
  GstStructure *structure = gst_caps_get_structure (caps, 0);
  gboolean ret;

  ret = gst_structure_get_int (structure, "width", width);
  ret &= gst_structure_get_int (structure, "height", height);

  return ret;
}

void
gst_egl_buffer_attach(GstEGLBuffer *buffer, GstBuffer *attach)
{
  if(buffer->attach)
    gst_buffer_unref(buffer->attach);
  buffer->attach = NULL;
  if(attach)
    buffer->attach = gst_buffer_ref(attach);
}
