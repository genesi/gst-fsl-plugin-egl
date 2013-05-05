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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <gst/video/gstvideosink.h>
#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>
#undef GL_GLEXT_PROTOTYPES

#include "gstgldisplay.h"
#include "gsteglbuffer.h"
#include "gsteglplatform.h"

#ifndef GLEW_VERSION_MAJOR
#define GLEW_VERSION_MAJOR 4
#endif

#ifndef GLEW_VERSION_MINOR
#define GLEW_VERSION_MINOR 0
#endif

/*
 * gst-launch-0.10 --gst-debug=gldisplay:N pipeline
 * N=1: errors
 * N=2: errors warnings
 * N=3: errors warnings infos
 * N=4: errors warnings infos
 * N=5: errors warnings infos logs
 */

GST_DEBUG_CATEGORY_STATIC (gst_gl_display_debug);
#define GST_CAT_DEFAULT gst_gl_display_debug

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_gl_display_debug, "gldisplay", 0, "opengl display");

enum
{
    RESIZE_SIGNAL = 0,
    DRAW_SIGNAL,
    DRAW_FINISH_SIGNAL,
    CLOSE_SIGNAL,
    LAST_SIGNAL
};

static guint display_signals[LAST_SIGNAL] = { 0, };

GST_BOILERPLATE_FULL (GstGLDisplay, gst_gl_display, GObject, G_TYPE_OBJECT,
    DEBUG_INIT);
static void gst_gl_display_finalize (GObject * object);

/* Called in the gl thread, protected by lock and unlock */
gpointer gst_gl_display_thread_create_context (GstGLDisplay * display);
void gst_gl_display_thread_destroy_context (GstGLDisplay * display);
void gst_gl_display_thread_gen_texture (GstEGLBuffer * buffer);
void gst_gl_display_thread_del_textures (GstGLDisplay *display);
void gst_gl_display_thread_init_redisplay (GstGLDisplay * display);
void gst_gl_display_thread_on_resize (GstGLDisplay * display);
void gst_gl_display_thread_do_upload (GstEGLBuffer * buffer);

/* private methods */
void gst_gl_display_lock (GstGLDisplay * display);
void gst_gl_display_unlock (GstGLDisplay * display);
void gst_gl_display_on_resize (GstGLDisplay * display, gint width, gint height);
void gst_gl_display_on_draw (GstGLDisplay * display);
void gst_gl_display_on_draw_finish (GstGLDisplay * display);
void gst_gl_display_on_close (GstGLDisplay * display);
void gst_gl_display_glgen_texture (GstEGLBuffer *buffer);
void gst_gl_display_gldel_texture (gpointer data, gpointer user_data);

void gst_gl_display_thread_do_upload_fill (GstEGLBuffer * buffer);


//------------------------------------------------------------
//---------------------- For klass GstGLDisplay ---------------
//------------------------------------------------------------

static void
gst_gl_display_base_init (gpointer g_class)
{
}

static void
gst_gl_display_class_init (GstGLDisplayClass * klass)
{
  G_OBJECT_CLASS (klass)->finalize = gst_gl_display_finalize;

  display_signals[RESIZE_SIGNAL] =
	  g_signal_new ("resize",
			  GST_TYPE_GL_DISPLAY,
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0);
  display_signals[DRAW_SIGNAL] =
	  g_signal_new ("draw",
			  GST_TYPE_GL_DISPLAY,
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0);
  display_signals[DRAW_FINISH_SIGNAL] =
	  g_signal_new ("draw-finish",
			  GST_TYPE_GL_DISPLAY,
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0);
  display_signals[CLOSE_SIGNAL] =
	  g_signal_new ("close",
			  GST_TYPE_GL_DISPLAY,
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0);
  gst_gl_window_init_platform ();
}


static void
gst_gl_display_init (GstGLDisplay * display, GstGLDisplayClass * klass)
{
  GST_INFO("begin");
  //thread safe
  display->mutex = g_mutex_new ();
  display->texlock = g_mutex_new ();

  //gl context
  display->gl_thread = NULL;
  display->gl_window = NULL;
  display->isAlive = TRUE;

  //conditions
  display->cond_create_context = g_cond_new ();
  display->cond_destroy_context = g_cond_new ();

  //action redisplay
  display->alloc_count = 0;
  display->free_textures = NULL;
  display->todraw = NULL;
  display->drawing = NULL;
  display->cond_tex = g_cond_new();
  display->cond_disp = g_cond_new();
  display->keep_aspect_ratio = FALSE;
  display->redisplay_format = GST_VIDEO_FORMAT_UNKNOWN;
  display->tex_width = 1;
  display->tex_height = 1;
  display->crop_left = 0;
  display->crop_right = 0;
  display->crop_top = 0;
  display->crop_bottom = 0;
  display->sampler_left = 0.0f;
  display->sampler_right = 1.0f;
  display->sampler_top = 1.0f;
  display->sampler_bottom = 0.0f;
  display->vertex_src = NULL;
  display->fragment_src = NULL;
  display->redisplay_shader = NULL;
  display->redisplay_attr_position_loc = 0;
  display->redisplay_attr_texture_loc = 0;

  //foreign gl context
  display->external_gl_context = 0;
  GST_INFO("end");
}

static void
gst_gl_display_finalize (GObject * object)
{
  GstGLDisplay *display = GST_GL_DISPLAY (object);
  GST_INFO("gst_gl_display_finalize begin");
  
  gst_gl_window_send_message (display->gl_window,
      GST_GL_WINDOW_CB (gst_gl_display_thread_del_textures), display);

  if (display->mutex && display->gl_window) {
    gst_gl_window_set_resize_callback (display->gl_window, NULL, NULL);
    gst_gl_window_set_draw_callback (display->gl_window, NULL, NULL, NULL);
    gst_gl_window_set_close_callback (display->gl_window, NULL, NULL);

    GST_INFO ("send quit gl window loop");

    gst_gl_window_quit_loop (display->gl_window,
        GST_GL_WINDOW_CB (gst_gl_display_thread_destroy_context), display);

    GST_INFO ("quit sent to gl window loop");

    gst_gl_display_lock (display);
    if(display->gl_window)
      g_cond_wait (display->cond_destroy_context, display->mutex);
    GST_INFO ("quit received from gl window");
    gst_gl_display_unlock (display);
  }

  if (display->gl_thread) {
    gpointer ret = g_thread_join (display->gl_thread);
    GST_INFO ("gl thread joined");
    if (ret != NULL)
      GST_ERROR ("gl thread returned a not null pointer");
    display->gl_thread = NULL;
  }

  if (display->mutex) {
    g_mutex_free (display->mutex);
    display->mutex = NULL;
  }
  if (display->texlock) {
    g_mutex_free (display->texlock);
    display->texlock = NULL;
  }
  if (display->cond_destroy_context) {
    g_cond_free (display->cond_destroy_context);
    display->cond_destroy_context = NULL;
  }
  if (display->cond_create_context) {
    g_cond_free (display->cond_create_context);
    display->cond_create_context = NULL;
  }
  if (display->cond_disp) {
    g_cond_free (display->cond_disp);
    display->cond_disp = NULL;
  }
  if (display->cond_tex) {
    g_cond_free (display->cond_tex);
    display->cond_tex = NULL;
  }
  g_free(display->vertex_src);
  g_free(display->fragment_src);
  GST_INFO("gst_gl_display_finalize finish");
}


//------------------------------------------------------------
//------------------ BEGIN GL THREAD PROCS -------------------
//------------------------------------------------------------

/* Called in the gl thread */
gpointer
gst_gl_display_thread_create_context (GstGLDisplay * display)
{
  GString *opengl_version;
  gint opengl_version_major = 0;
  gint opengl_version_minor = 0;
  GstGLWindow *window;

  GST_INFO("create context");
  window = gst_gl_window_new (display->external_gl_context);

  if (!window) {
    gst_gl_display_lock(display);
    display->isAlive = FALSE;
    GST_ERROR_OBJECT (display, "Failed to create gl window");
    g_cond_signal (display->cond_create_context);
    gst_gl_display_unlock (display);
    return NULL;
  }

  //setup callbacks
  gst_gl_window_set_resize_callback (window,
      GST_GL_WINDOW_CB2 (gst_gl_display_on_resize), display);
  gst_gl_window_set_draw_callback (window,
      GST_GL_WINDOW_CB (gst_gl_display_on_draw),
      GST_GL_WINDOW_CB (gst_gl_display_on_draw_finish), display);
  gst_gl_window_set_close_callback (window,
      GST_GL_WINDOW_CB (gst_gl_display_on_close), display); 

  gst_gl_display_lock (display);
  display->gl_window = window;

  GST_INFO ("gl window created");

  opengl_version =
	  g_string_truncate (g_string_new ((gchar *) glGetString (GL_VERSION)),
			  3);

  sscanf (opengl_version->str, "%d.%d", &opengl_version_major,
		  &opengl_version_minor);

  GST_INFO ("GL_VERSION: %s", glGetString (GL_VERSION));
  if (glGetString (GL_SHADING_LANGUAGE_VERSION))
	  GST_INFO ("GL_SHADING_LANGUAGE_VERSION: %s",
			  glGetString (GL_SHADING_LANGUAGE_VERSION));
  else
	  GST_INFO ("Your driver does not support GLSL (OpenGL Shading Language)");

  GST_INFO ("GL_VENDOR: %s", glGetString (GL_VENDOR));
  GST_INFO ("GL_RENDERER: %s", glGetString (GL_RENDERER));

  g_string_free (opengl_version, TRUE);

  if (!GL_ES_VERSION_2_0) {
	  GST_WARNING ("Required OpenGL ES > 2.0");
	  display->isAlive = FALSE;
  }

  g_cond_signal (display->cond_create_context);

  gst_gl_display_unlock (display);

  gst_gl_window_run_loop (display->gl_window);

  GST_INFO ("loop exited\n");

  gst_gl_display_lock (display);
  display->isAlive = FALSE;
  display->gl_window = NULL;
  gst_gl_display_unlock (display);
  
  g_object_unref (G_OBJECT (window));

  gst_gl_display_lock (display);
  g_cond_signal (display->cond_destroy_context);
  gst_gl_display_unlock (display);

  return NULL;
}


/* Called in the gl thread */
void
gst_gl_display_thread_destroy_context (GstGLDisplay * display)
{
  if (display->redisplay_shader) {
    g_object_unref (G_OBJECT (display->redisplay_shader));
    display->redisplay_shader = NULL;
  }
  GST_INFO ("Context destroyed");
}

static void
align_buffer_size(GstVideoFormat format, gint *width, gint *height,
    gint *right, gint *bottom)
{
  gint new_width, new_height;
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
      new_width = GST_ROUND_UP_4 (*width);
      new_height = GST_ROUND_UP_2 (*height);
      *right += new_width - *width;
      *bottom += new_height - *height;
      *width = new_width;
      *height = new_height;
      break;
    case GST_VIDEO_FORMAT_YUY2:
    case GST_VIDEO_FORMAT_YVYU:
    case GST_VIDEO_FORMAT_UYVY:
      new_width = GST_ROUND_UP_2(*width);
      *right += new_width - *width;
      *width = new_width;
      break;
    default:
      ;
  }
}

static GstEGLBuffer*
gst_gl_display_alloc_new_buffer(GstGLDisplay * display, GstVideoFormat format,
    gint width, gint height, gint left, gint right, gint top, gint bottom)
{
  GstEGLBuffer *buf;
  align_buffer_size(format, &width, &height, &right, &bottom);  //hack for current gpu driver

  gst_gl_display_lock(display);
  if(format != display->redisplay_format ||  //new video format
      width != display->tex_width || height != display->tex_height ||
      left != display->crop_left || right != display->crop_right ||
      top != display->crop_top || bottom != display->crop_bottom)
  {
    display->redisplay_format = format;
    display->tex_width = width;
    display->tex_height = height;
    display->crop_left = left;
    display->crop_right = right;
    display->crop_top = top;
    display->crop_bottom = bottom;
    display->sampler_left   = ((gfloat)left)/width;
    display->sampler_right  = ((gfloat)(width-right))/width;
    display->sampler_top    = ((gfloat)(height-bottom))/height;
    display->sampler_bottom = ((gfloat)top)/height;
    
    if (display->todraw) {
      gst_egl_buffer_unref(display->todraw);
      display->todraw = NULL;
    }
    if (display->drawing) {
      gst_egl_buffer_unref(display->drawing);
      display->drawing = NULL;
    }
    if(display->redisplay_shader)
    {
      gst_gl_shader_use (NULL);
      g_object_unref (G_OBJECT (display->redisplay_shader));
      display->redisplay_shader = NULL;
    }
    g_cond_signal(display->cond_disp);
  }
  gst_gl_display_unlock(display);

  buf = gst_egl_buffer_new(display, NULL, NULL, NULL, format, width, height);
  
  return buf;
}

GstEGLBuffer*
gst_gl_display_get_free_buffer(GstGLDisplay * display, GstCaps *caps, guint size, gboolean check_platform)
{
  GstEGLBuffer *ret = NULL;
  GstVideoFormat format;
  gint width;
  gint height;
  gint alloc_width;
  gint alloc_height;
  GstStructure *s;
  gint left=0, right=0, top=0, bottom=0;
  s = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (s, "crop-left", &left);
  gst_structure_get_int (s, "crop-top", &top);
  gst_structure_get_int (s, "crop-right", &right);
  gst_structure_get_int (s, "crop-bottom", &bottom);

  gst_video_format_parse_caps (caps, &format, &width, &height);
  alloc_width = width;
  alloc_height = height;

  GST_INFO("get free buffer format %d, width, height [%d, %d], size %d", format, width, height, size);
  if(size != -1 && gst_video_format_get_size(format, alloc_width, alloc_height) != size)
  {
    GST_WARNING("can't allocate buffer format:%d, width, height: [%d, %d] while size %d", format, alloc_width, alloc_height, size);
    return NULL;
  }

  if(check_platform && !gst_egl_platform_accept_caps(format, alloc_width, alloc_height))
    return NULL;
  
  ret = gst_gl_display_alloc_new_buffer(display, format, alloc_width, alloc_height, left, right, top, bottom);
  if(ret->texinfo)
    GST_BUFFER_CAPS(ret) = gst_caps_ref(caps);
  else
  {
    gst_egl_buffer_unref(ret);
    ret = NULL;
  }
  GST_INFO("Got buf %p", ret);
  return ret;
}

/* Called in the gl thread */
void
gst_gl_display_thread_gen_texture (GstEGLBuffer * buffer)
{
  //setup a texture to render to (this one will be in a gl buffer)
  gst_gl_display_glgen_texture (buffer);
}

void
gst_gl_display_thread_del_textures (GstGLDisplay *display)
{
  g_mutex_lock(display->texlock);
  if(display->free_textures)
  {
    g_list_foreach (display->free_textures, gst_gl_display_gldel_texture, display);
    g_list_free(display->free_textures);
    display->free_textures = NULL;
  }
  g_mutex_unlock(display->texlock);
}

/* Called in the gl thread */
void
gst_gl_display_thread_init_redisplay (GstGLDisplay * display)
{
  GError *error = NULL;
  gst_gl_display_lock(display);
  if(display->redisplay_shader)
  { //already initialized
    gst_gl_display_unlock(display);
    return;
  }
  g_free(display->vertex_src);
  g_free(display->fragment_src);
   
  display->redisplay_shader = gst_gl_shader_new ();
  display->vertex_src = gst_egl_platform_get_vertex_source(display->redisplay_format);
  display->fragment_src = gst_egl_platform_get_fragment_source(display->redisplay_format);
 
  GST_INFO("vertex source:\n%s", display->vertex_src);
  GST_INFO("fragment source:\n%s", display->fragment_src);

  gst_gl_shader_set_vertex_source (display->redisplay_shader, display->vertex_src);
  gst_gl_shader_set_fragment_source (display->redisplay_shader, display->fragment_src);

  gst_gl_shader_compile (display->redisplay_shader, &error);
  if (error) {
    GST_ERROR ("%s", error->message);
    g_error_free (error);
    error = NULL;
    gst_gl_shader_use (NULL);
    display->isAlive = FALSE;
  } else {
    display->redisplay_attr_position_loc =
        gst_gl_shader_get_attribute_location (display->redisplay_shader,
        "a_position");
    display->redisplay_attr_texture_loc =
        gst_gl_shader_get_attribute_location (display->redisplay_shader,
        "a_texCoord");
  }
  gst_gl_display_unlock(display);
}

void
gst_gl_display_thread_on_resize (GstGLDisplay * display)
{
  gst_gl_display_on_resize(display, display->window_width, display->window_height);
}

/* Called by the idle function */
void
gst_gl_display_thread_do_upload (GstEGLBuffer * buffer)
{
  gst_gl_display_thread_do_upload_fill (buffer);
}

//------------------------------------------------------------
//------------------ BEGIN GL THREAD ACTIONS -----------------
//------------------------------------------------------------


//------------------------------------------------------------
//---------------------- BEGIN PRIVATE -----------------------
//------------------------------------------------------------


void
gst_gl_display_lock (GstGLDisplay * display)
{
  g_mutex_lock (display->mutex);
}


void
gst_gl_display_unlock (GstGLDisplay * display)
{
  g_mutex_unlock (display->mutex);
}


void
gst_gl_display_on_resize (GstGLDisplay * display, gint width, gint height)
{
    gst_gl_display_lock(display);
    GST_INFO("!!!!!!!!!!keep aspect ratio %d, [%d, %d]", display->keep_aspect_ratio, width, height);
    if (display->keep_aspect_ratio) {
      GstVideoRectangle src, dst, result;
      GstEGLBuffer *buffer = display->todraw ? display->todraw :
          (display->drawing ? display->drawing : NULL);

      src.x = 0;
      src.y = 0;
      src.w = buffer ? buffer->width : 0;
      src.h = buffer ? buffer->height : 0;

      dst.x = 0;
      dst.y = 0;
      dst.w = width;
      dst.h = height;

      gst_video_sink_center_rect (src, dst, &result, TRUE);
      glViewport (result.x, result.y, result.w, result.h);
      GST_INFO("view port [%d, %d, %d, %d]", result.x, result.y, result.w, result.h);
    } else {
      glViewport (0, 0, width, height);
      GST_INFO("view port [%d, %d, %d, %d]", 0, 0, width, height);
    }
    gst_gl_display_unlock(display);
    g_signal_emit (display, display_signals[RESIZE_SIGNAL], 0);
}

void
gst_gl_display_on_draw (GstGLDisplay * display)
{
  GstEGLBuffer *buffer;

  GST_DEBUG("draw begin");
  gst_gl_display_lock(display);
  //check if texture is ready for being drawn
  if (!display->todraw && !display->drawing)
  {
    gst_gl_display_unlock(display);
    return;
  }

  buffer = display->todraw ? display->todraw : display->drawing;

  GST_INFO("------ draw buffer %p", buffer);

  {
    GLenum target = gst_egl_platform_get_target(buffer->format);
    const GLfloat vVertices[] = { 1.0f, 1.0f, 0.0f,
      display->sampler_right, display->sampler_bottom,
      -1.0f, 1.0f, 0.0f,
      display->sampler_left, display->sampler_bottom,
      -1.0f, -1.0f, 0.0f,
      display->sampler_left, display->sampler_top,
      1.0f, -1.0f, 0.0f,
      display->sampler_right, display->sampler_top
    };

    GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

    glClear (GL_COLOR_BUFFER_BIT);

    gst_gl_shader_use (NULL);
    gst_gl_shader_use (display->redisplay_shader);

    //Load the vertex position
    glVertexAttribPointer (display->redisplay_attr_position_loc, 3, GL_FLOAT,
        GL_FALSE, 5 * sizeof (GLfloat), vVertices);

    //Load the texture coordinate
    glVertexAttribPointer (display->redisplay_attr_texture_loc, 2, GL_FLOAT,
        GL_FALSE, 5 * sizeof (GLfloat), &vVertices[3]);

    glEnableVertexAttribArray (display->redisplay_attr_position_loc);
    glEnableVertexAttribArray (display->redisplay_attr_texture_loc);

    glEnable(target);
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (target, buffer->texinfo->texture);
    gst_gl_shader_set_uniform_1i (display->redisplay_shader, "s_texture", 0);

	GST_INFO("Draw Element crop: [%f, %f, %f, %f]", display->sampler_top,
			display->sampler_bottom, display->sampler_left, display->sampler_right);
	GST_INFO("Texture: format %d, size [%d, %d], stride %d", buffer->texinfo->format,
			buffer->texinfo->width, buffer->texinfo->height, buffer->texinfo->stride);

    glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    glBindTexture (target, 0);
    glDisable(target);
  }
  gst_gl_display_unlock(display);
  
  //end default opengl scene
  g_signal_emit (display, display_signals[DRAW_SIGNAL], 0);
  GST_DEBUG("draw finish");
}

void
gst_gl_display_on_draw_finish (GstGLDisplay * display)
{
  gst_gl_display_lock(display);
  GST_INFO("------ draw buffer %p finish, last drawing %p", display->todraw, display->drawing);
  if(display->todraw)
  {
    if(display->drawing)
      gst_egl_buffer_unref(display->drawing);
    display->drawing = display->todraw;
    display->todraw = NULL;
    g_cond_signal(display->cond_disp);
  }
  gst_gl_display_unlock(display);
  g_signal_emit (display, display_signals[DRAW_FINISH_SIGNAL], 0);
}

void
gst_gl_display_on_close (GstGLDisplay * display)
{
  GST_INFO ("on close");
  gst_gl_display_lock(display);
  display->isAlive = FALSE;
  gst_gl_display_unlock(display);
  g_signal_emit (display, display_signals[CLOSE_SIGNAL], 0);
  GST_INFO("on_close finish");
}

static gboolean
caps_match(GstEGLBuffer *buf, GstEGLTexture *info)
{
  return buf->format == info->format &&
         buf->width == info->width &&
         buf->height == info->height;
}

static void
assign_texture(GstEGLBuffer *buf, GstEGLTexture *info)
{
  buf->texinfo = info;
}

static gboolean
reuse_texture(GstEGLBuffer *buf, GstEGLTexture *info)
{
  if(caps_match(buf, info))
  {
    assign_texture(buf, info);
    return TRUE;
  }
  else
    return FALSE;
}

/* Generate a texture if no one is available in the pool
 * Called in the gl thread */
void
gst_gl_display_glgen_texture (GstEGLBuffer *buffer)
{
  GLenum target;
  GList *first;
  GstEGLTexture *info;
  GstGLDisplay *display = buffer->display;
  
  g_mutex_lock(display->texlock);
  while(!buffer->texinfo)
  {
    while(display->free_textures)
    {
      first = g_list_first(display->free_textures);
      info = (GstEGLTexture*)(first->data);
      display->free_textures = g_list_remove_link(display->free_textures, first);
      g_list_free(first);
      if(!reuse_texture(buffer, info))  //cannot reuse
        gst_gl_display_gldel_texture(info, display);
      else
      {
        GST_INFO("====== reuse texture %d", info->texture);
        break;
      }
    }
    if(!buffer->texinfo && display->alloc_count < GST_GL_DISPLAY_MAX_BUFFER_COUNT)
    {
      info = g_slice_new0(GstEGLTexture);
      info->format = buffer->format;
      info->width = buffer->width;
      info->height = buffer->height;
      target = gst_egl_platform_get_target(buffer->format);
      gst_egl_platform_alloc_image(gst_gl_window_get_egl_display(display->gl_window), info);
      if(info->image)
      {
        glGenTextures(1, &info->texture);
  
        glEnable(target);
        glBindTexture(target, info->texture);
        glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glEGLImageTargetTexture2DOES(target, info->image);
        glBindTexture (target, 0);
        glDisable(target);

        assign_texture(buffer, info);
    
        display->alloc_count++;
        GST_INFO("===== create texture %d for buffer %p, target %d", info->texture, buffer, target);
      }
      else
      {
        g_slice_free(GstEGLTexture, info);
        GST_ERROR("gst_egl_platform_alloc_image failed: out of memory");
      }
      break;
    }
    else if(!buffer->texinfo)
    {
      GST_INFO("###### wait for texture release");
      g_cond_wait(display->cond_tex, display->texlock);
    }
  }
  g_mutex_unlock(display->texlock);
}


/* Delete a texture, actually the texture is just added to the pool
 * Called in the gl thread */
void
gst_gl_display_gldel_texture (gpointer data, gpointer user_data)
{
  GstEGLTexture *info = (GstEGLTexture *)data;
  GstGLDisplay *display = GST_GL_DISPLAY(user_data);
  GST_INFO ("deleted texture id:%d", info->texture);
  glDeleteTextures (1, &info->texture);
  gst_egl_platform_free_image(gst_gl_window_get_egl_display(display->gl_window), info);
  g_slice_free(GstEGLTexture, info);
  display->alloc_count--;
  GST_INFO ("deleted texture id:%d done", info->texture);
}

//------------------------------------------------------------
//---------------------  END PRIVATE -------------------------
//------------------------------------------------------------


//------------------------------------------------------------
//---------------------- BEGIN PUBLIC ------------------------
//------------------------------------------------------------


/* Called by the first gl element of a video/x-raw-gl flow */
GstGLDisplay *
gst_gl_display_new (void)
{
  GstGLDisplay *disp = g_object_new (GST_TYPE_GL_DISPLAY, NULL);
  GST_INFO("create display %p", disp);
  return disp;
}


/* Create an opengl context (one context for one GstGLDisplay) */
void
gst_gl_display_create_context (GstGLDisplay * display,
    gulong external_gl_context)
{
  GST_INFO("create context");
  gst_gl_display_lock (display);

  if (!display->gl_window) {
    display->external_gl_context = external_gl_context;

    display->gl_thread = g_thread_create (
        (GThreadFunc) gst_gl_display_thread_create_context, display, TRUE,
        NULL);

    g_cond_wait (display->cond_create_context, display->mutex);

    GST_INFO ("gl thread created");
  }

  gst_gl_display_unlock (display);
}

void
gst_gl_display_destroy_context(GstGLDisplay * display)
{
  GST_INFO("begin");
  gst_gl_display_lock(display);
  display->isAlive = FALSE;
  //unref all reffered buffers, thus no one reffered this display
  if (display->todraw) {
    gst_egl_buffer_unref(display->todraw);
    display->todraw = NULL;
  }
  if (display->drawing) {
    gst_egl_buffer_unref(display->drawing);
    display->drawing = NULL;
  }
  g_cond_signal(display->cond_disp);
  gst_gl_display_unlock (display);
  GST_INFO("end");
}

/* Called by the glimagesink element */
gboolean
gst_gl_display_redisplay (GstGLDisplay * display, GstEGLBuffer *buffer,
    gint window_width, gint window_height, gboolean keep_aspect_ratio)
{
  gboolean isAlive = TRUE;

  GST_INFO("------ redisplay buffer %p", buffer);
  gst_gl_display_lock (display);
  isAlive = display->isAlive;
  if (isAlive) {
    if (!display->redisplay_shader) {
      gst_gl_display_unlock (display);
      gst_gl_window_send_message (display->gl_window,
          GST_GL_WINDOW_CB (gst_gl_display_thread_init_redisplay), display);
      gst_gl_display_lock (display);
    }
    while(isAlive && buffer && display->todraw) {	//wait last buffer display finish
      GST_INFO("###### wait for display finish");
      g_cond_wait(display->cond_disp, display->mutex);
      isAlive = display->isAlive;
    }
    if(isAlive && (!buffer || buffer != display->drawing))
    {
      if(buffer)
        display->todraw = gst_egl_buffer_ref(buffer);
      if(display->keep_aspect_ratio != keep_aspect_ratio)
      {
        display->keep_aspect_ratio = keep_aspect_ratio;
        display->window_width = window_width;
        display->window_height = window_height;
        gst_gl_display_unlock (display);
        gst_gl_window_send_message (display->gl_window,
            GST_GL_WINDOW_CB (gst_gl_display_thread_on_resize), display);
        gst_gl_display_lock (display);
      }
      if (display->gl_window) {
        gst_gl_display_unlock (display);
        gst_gl_window_draw (display->gl_window, window_width, window_height);
        gst_gl_display_lock(display);
      }
    }
  }
  gst_gl_display_unlock (display);
  GST_INFO("------ redisplay buffer %p done", buffer);

  return isAlive;
}

/* Called by gst_gl_buffer_new */
void
gst_gl_display_gen_texture (GstGLDisplay * display, GstEGLBuffer *buffer)
{
  if (display->isAlive) {
    gst_gl_window_send_message (display->gl_window,
        GST_GL_WINDOW_CB (gst_gl_display_thread_gen_texture), buffer);
  }
}

/* Called by gst_gl_buffer_finalize */
void
gst_gl_display_del_texture (GstGLDisplay * display, GstEGLBuffer *buffer)
{
  GST_INFO("Delete texture of buffer %p", buffer);
  if (buffer->texinfo) {
    g_mutex_lock (display->texlock);
    display->free_textures = g_list_append(display->free_textures, buffer->texinfo);
    g_cond_signal(display->cond_tex);
    g_mutex_unlock (display->texlock);
    buffer->texinfo = NULL;
  }
}

/* Called by the first gl element of a video/x-raw-gl flow */
gboolean
gst_gl_display_do_upload (GstGLDisplay * display, GstEGLBuffer *buffer, GstBuffer *src)
{
  gboolean isAlive = display->isAlive;
  g_return_val_if_fail(buffer && src, FALSE);
  if (isAlive) {
    gst_egl_buffer_attach(buffer, src);
    gst_gl_window_send_message (display->gl_window,
        GST_GL_WINDOW_CB (gst_gl_display_thread_do_upload), buffer);
  }

  return isAlive;
}

/* Called by the glimagesink */
void
gst_gl_display_set_window_id (GstGLDisplay * display, gulong window_id)
{
  gst_gl_window_set_external_window_id (display->gl_window, window_id);
}

gulong
gst_gl_display_get_internal_gl_context (GstGLDisplay * display)
{
  gulong external_gl_context = 0;
  gst_gl_display_lock (display);
  external_gl_context =
      gst_gl_window_get_internal_gl_context (display->gl_window);
  gst_gl_display_unlock (display);
  return external_gl_context;
}

void
gst_gl_display_activate_gl_context (GstGLDisplay * display, gboolean activate)
{
  if (!activate)
    gst_gl_display_lock (display);
  gst_gl_window_activate_gl_context (display->gl_window, activate);
  if (activate)
    gst_gl_display_unlock (display);
}


//------------------------------------------------------------
//------------------------ END PUBLIC ------------------------
//------------------------------------------------------------

/* called by gst_gl_display_thread_do_upload (in the gl thread) */
void
gst_gl_display_thread_do_upload_fill (GstEGLBuffer * buffer)
{
  GstBuffer *src = buffer->attach;
  gint width = buffer->width;
  gint height = buffer->height;
  gpointer data = GST_BUFFER_DATA(src);
  GST_INFO("==========do_upload_fill %p, width %d, height %d", buffer, width, height);
  if(buffer->format == buffer->texinfo->real_format)
  {
    GLenum target = gst_egl_platform_get_target(buffer->format);
    GLenum internalformat, format, type;
    glEnable(target);
    glBindTexture (target, buffer->texinfo->texture);

    gst_egl_platform_get_format_info(buffer->format, &internalformat, &format, &type);
    glTexSubImage2D (target, 0, 0, 0, width, height, format, type, data);

    //make sure no texture is in use in our opengl context
    //in case we want to use the upload texture in an other opengl context
    glBindTexture (target, 0);
    glDisable(target);
  }
  else
  {
    gst_egl_platform_convert_color_space(data, buffer->format, GST_BUFFER_DATA(buffer),
			buffer->texinfo->real_format, width, height, buffer->texinfo->stride);
  }
  gst_egl_buffer_attach(buffer, NULL);
}


