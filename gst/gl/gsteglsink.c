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

/**
 * SECTION:element-eglsink
 *
 * eglsink renders video frames to a drawable on a local or remote
 * display using OpenGL. This element can receive a Window ID from the
 * application through the XOverlay interface and will then render video
 * frames in this drawable.
 * If no Window ID was provided by the application, the element will
 * create its own internal window and render into it.
 *
 * <refsect2>
 * <title>Scaling</title>
 * <para>
 * Depends on the driver, OpenGL handles hardware accelerated
 * scaling of video frames. This means that the element will just accept
 * incoming video frames no matter their geometry and will then put them to the
 * drawable scaling them on the fly. Using the #GstEGLSink:force-aspect-ratio
 * property it is possible to enforce scaling with a constant aspect ratio,
 * which means drawing black borders around the video frame.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Events</title>
 * <para>
 * Through the gl thread, eglsink handle some events coming from the drawable
 * to manage its appearance even when the data is not flowing (GST_STATE_PAUSED).
 * That means that even when the element is paused, it will receive expose events
 * from the drawable and draw the latest frame with correct borders/aspect-ratio.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Examples</title>
 * |[
 * gst-launch -v videotestsrc ! "video/x-raw-rgb" ! eglsink
 * ]| A pipeline to test hardware scaling.
 * No special opengl extension is used in this pipeline, that's why it should work
 * with OpenGL >= 1.1. That's the case if you are using the MESA3D driver v1.3.
 * |[
 * gst-launch -v videotestsrc ! "video/x-raw-yuv, format=(fourcc)I420" ! eglsink
 * ]| A pipeline to test hardware scaling and hardware colorspace conversion.
 * When your driver supports GLSL (OpenGL Shading Language needs OpenGL >= 2.1),
 * the 4 following format YUY2, UYVY, I420, YV12 and AYUV are converted to RGB32
 * through some fragment shaders and using one framebuffer (FBO extension OpenGL >= 1.4).
 * If your driver does not support GLSL but supports MESA_YCbCr extension then
 * the you can use YUY2 and UYVY. In this case the colorspace conversion is automatically
 * made when loading the texture and therefore no framebuffer is used.
 * |[
 * gst-launch -v gltestsrc ! eglsink
 * ]| A pipeline 100% OpenGL.
 * No special opengl extension is used in this pipeline, that's why it should work
 * with OpenGL >= 1.1. That's the case if you are using the MESA3D driver v1.3.
 * |[
 * gst-plugins-egl/tests/examples/generic/cube
 * ]| The graphic FPS scene can be greater than the input video FPS.
 * The graphic scene can be written from a client code through the
 * two glfilterapp properties.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/interfaces/xoverlay.h>

#include "gsteglsink.h"
#include "gsteglplatform.h"
#include "gsteglbuffer.h"
#include "gstgldisplay.h"

GST_DEBUG_CATEGORY (gst_debug_egl_sink);
#define GST_CAT_DEFAULT gst_debug_egl_sink

static void gst_egl_sink_init_interfaces (GType type);

static void gst_egl_sink_finalize (GObject * object);
static void gst_egl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * param_spec);
static void gst_egl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * param_spec);

static gboolean gst_egl_sink_query (GstElement * element, GstQuery * query);

static GstStateChangeReturn
gst_egl_sink_change_state (GstElement * element, GstStateChange transition);

static GstFlowReturn gst_egl_sink_buffer_alloc (GstBaseSink * sink,
	guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);
static void gst_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_egl_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static GstFlowReturn gst_egl_sink_show_frame (GstVideoSink *video_sink,
    GstBuffer * buf);

static void gst_egl_sink_xoverlay_init (GstXOverlayClass * iface);
static void gst_egl_sink_set_xwindow_id (GstXOverlay * overlay,
    gulong window_id);
static void gst_egl_sink_expose (GstXOverlay * overlay);
static gboolean gst_egl_sink_interface_supported (GstImplementsInterface *
    iface, GType type);
static void gst_egl_sink_implements_init (GstImplementsInterfaceClass *
    klass);

enum
{
  ARG_0,
  ARG_DISPLAY,
  PROP_CLIENT_SET_CAPS_CALLBACK,
  PROP_CLIENT_GET_BUFFER_CALLBACK,
  PROP_CLIENT_DRAW_CALLBACK,
  PROP_CLIENT_DATA,
  PROP_FORCE_ASPECT_RATIO,
  PROP_PIXEL_ASPECT_RATIO
};

/*
static GstStaticPadTemplate gst_egl_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
	GST_PAD_SINK,
    GST_PAD_ALWAYS,
	GST_STATIC_CAPS (
		GST_VIDEO_CAPS_RGBA ";"
		GST_VIDEO_CAPS_BGRA ";"
		GST_VIDEO_CAPS_YUV ("I420") ";"
		GST_VIDEO_CAPS_YUV ("YV12") ";"
		GST_VIDEO_CAPS_YUV ("NV12") ";"
		GST_VIDEO_CAPS_YUV ("UYVY"))
	);
*/
GST_BOILERPLATE_FULL (GstEGLSink, gst_egl_sink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_egl_sink_init_interfaces);

static void
gst_egl_sink_init_interfaces (GType type)
{

  static const GInterfaceInfo implements_info = {
    (GInterfaceInitFunc) gst_egl_sink_implements_init,
    NULL,
    NULL
  };

  static const GInterfaceInfo xoverlay_info = {
    (GInterfaceInitFunc) gst_egl_sink_xoverlay_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type, GST_TYPE_IMPLEMENTS_INTERFACE,
      &implements_info);

  g_type_add_interface_static (type, GST_TYPE_X_OVERLAY, &xoverlay_info);

  GST_DEBUG_CATEGORY_INIT (gst_debug_egl_sink, "eglsink", 0,
      "OpenGL Video Sink");
}

static void
gst_egl_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstCaps *capslist;
  GstPadTemplate *sink_template = NULL;
  gint i;
  guint32 formats[][2] = {
    {GST_MAKE_FOURCC ('N', 'V', '1', '2'), GST_VIDEO_FORMAT_NV12},
    {GST_MAKE_FOURCC ('Y', 'V', '1', '2'), GST_VIDEO_FORMAT_YV12},
    {GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y'), GST_VIDEO_FORMAT_UYVY},
    {GST_MAKE_FOURCC ('I', '4', '2', '0'), GST_VIDEO_FORMAT_I420}
  };

  gst_element_class_set_details_simple (element_class, "OpenGL video sink",
      "Sink/Video", "A videosink based on OpenGL",
      "Julien Isorce <julien.isorce@gmail.com>");
  /* make a list of all available caps */
  capslist = gst_caps_new_empty ();
  for (i = 0; i < G_N_ELEMENTS (formats); i++) {
    gst_caps_append_structure (capslist,
			gst_structure_new ("video/x-raw-yuv",
				"format",
				GST_TYPE_FOURCC,
				formats[i][0], "width",
				GST_TYPE_INT_RANGE, 1,
				G_MAXINT, "height",
				GST_TYPE_INT_RANGE, 1,
				G_MAXINT, "framerate",
				GST_TYPE_FRACTION_RANGE,
				0, 1, G_MAXINT, 1,
				"width_align", G_TYPE_INT, gst_egl_platform_get_alignment_h(formats[i][1]),
				"height_align", G_TYPE_INT, gst_egl_platform_get_alignment_v(formats[i][1]),
				NULL));
  }
  gst_caps_append_structure (capslist,
		  gst_structure_new ("video/x-raw-rgb",
			  "bpp", G_TYPE_INT, 32,
			  "depth", GST_TYPE_INT_RANGE, 24, 32,
			  "width_align", G_TYPE_INT, gst_egl_platform_get_alignment_h(GST_VIDEO_FORMAT_RGBA),
			  "height_align", G_TYPE_INT, gst_egl_platform_get_alignment_v(GST_VIDEO_FORMAT_RGBA),
			  NULL));

  sink_template = gst_pad_template_new ("sink",
		  GST_PAD_SINK, GST_PAD_ALWAYS,
		  capslist);

  gst_element_class_add_pad_template (element_class, sink_template);
}

static void
gst_egl_sink_class_init (GstEGLSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *gstvideosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstvideosink_class = (GstVideoSinkClass *) klass;

  gobject_class->set_property = gst_egl_sink_set_property;
  gobject_class->get_property = gst_egl_sink_get_property;

  g_object_class_install_property (gobject_class, ARG_DISPLAY,
      g_param_spec_string ("display", "Display", "Display name",
          NULL, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CLIENT_SET_CAPS_CALLBACK,
      g_param_spec_pointer ("set-caps-callback", "Client set caps callback",
		  "Define a custom set caps callback in a client code", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_CLIENT_GET_BUFFER_CALLBACK,
      g_param_spec_pointer ("get-buffer-callback", "Client get buffer callback",
		  "Define a custom get buffer callback in a client code", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_CLIENT_DRAW_CALLBACK,
      g_param_spec_pointer ("draw-callback", "Client draw callback",
		  "Define a custom draw callback in a client code", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_CLIENT_DATA,
      g_param_spec_pointer ("client-data", "Client data",
		  "Pass data to the draw and reshape callbacks", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio",
          "Force aspect ratio",
          "When enabled, scaling will respect original aspect ratio", FALSE,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gobject_class->finalize = gst_egl_sink_finalize;

  gstelement_class->change_state = gst_egl_sink_change_state;
  gstelement_class->query = GST_DEBUG_FUNCPTR (gst_egl_sink_query);

  gstbasesink_class->buffer_alloc = gst_egl_sink_buffer_alloc;
  gstbasesink_class->set_caps = gst_egl_sink_set_caps;
  gstbasesink_class->get_times = gst_egl_sink_get_times;
  
  gstvideosink_class->show_frame = gst_egl_sink_show_frame;
}

#define COLORFUL_STR(color, format, ...)  \
    "\33[1;" color "m" format "\33[0m", ##__VA_ARGS__

static void
gst_egl_sink_init (GstEGLSink * egl_sink,
    GstEGLSinkClass * egl_sink_class)
{
  egl_sink->display_name = NULL;
  egl_sink->window_id = 0;
  egl_sink->new_window_id = 0;
  egl_sink->display = NULL;
  egl_sink->set_caps_callback = NULL;
  egl_sink->get_buffer_callback = NULL;
  egl_sink->draw_callback = NULL;
  egl_sink->client_data = NULL;
  egl_sink->keep_aspect_ratio = FALSE;
  egl_sink->par = NULL;
  egl_sink->show_count = 0;
  g_print(COLORFUL_STR("32", "%s %s build on %s %s.\n", "EGLSink", VERSION, __DATE__, __TIME__));
}

static void
gst_egl_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEGLSink *egl_sink;

  g_return_if_fail (GST_IS_EGL_SINK (object));

  egl_sink = GST_EGL_SINK (object);

  switch (prop_id) {
    case ARG_DISPLAY:
    {
      g_free (egl_sink->display_name);
      egl_sink->display_name = g_strdup (g_value_get_string (value));
      break;
    }
	case PROP_CLIENT_SET_CAPS_CALLBACK:
	{
	  egl_sink->set_caps_callback = g_value_get_pointer (value);
	  break;
	}
	case PROP_CLIENT_GET_BUFFER_CALLBACK:
	{
	  egl_sink->get_buffer_callback = g_value_get_pointer (value);
	  break;
	}
	case PROP_CLIENT_DRAW_CALLBACK:
	{
	  egl_sink->draw_callback = g_value_get_pointer (value);
	  break;
	}
    case PROP_CLIENT_DATA:
	{
	  egl_sink->client_data = g_value_get_pointer (value);
	  break;
	}
    case PROP_FORCE_ASPECT_RATIO:
    {
      egl_sink->keep_aspect_ratio = g_value_get_boolean (value);
      break;
    }
    case PROP_PIXEL_ASPECT_RATIO:
    {
      g_free (egl_sink->par);
      egl_sink->par = g_new0 (GValue, 1);
      g_value_init (egl_sink->par, GST_TYPE_FRACTION);
      if (!g_value_transform (value, egl_sink->par)) {
        g_warning ("Could not transform string to aspect ratio");
        gst_value_set_fraction (egl_sink->par, 1, 1);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_egl_sink_finalize (GObject * object)
{
  GstEGLSink *egl_sink;

  g_return_if_fail (GST_IS_EGL_SINK (object));

  egl_sink = GST_EGL_SINK (object);

  if (egl_sink->par) {
    g_free (egl_sink->par);
    egl_sink->par = NULL;
  }

  if (egl_sink->caps)
    gst_caps_unref (egl_sink->caps);

  g_free (egl_sink->display_name);

  GST_DEBUG ("finalized");
}

static void
gst_egl_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstEGLSink *egl_sink;

  g_return_if_fail (GST_IS_EGL_SINK (object));

  egl_sink = GST_EGL_SINK (object);

  switch (prop_id) {
    case ARG_DISPLAY:
      g_value_set_string (value, egl_sink->display_name);
      break;
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, egl_sink->keep_aspect_ratio);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
      if (egl_sink->par)
        g_value_transform (egl_sink->par, value);
      else
        g_value_set_static_string(value, "1/1");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_egl_sink_query (GstElement * element, GstQuery * query)
{
  GstEGLSink *egl_sink = GST_EGL_SINK (element);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CUSTOM:
    {
      GstStructure *structure = gst_query_get_structure (query);
      gst_structure_set (structure, "gstgldisplay", G_TYPE_POINTER,
          egl_sink->display, NULL);
      res = GST_ELEMENT_CLASS (parent_class)->query (element, query);
      break;
    }
    default:
      res = GST_ELEMENT_CLASS (parent_class)->query (element, query);
      break;
  }

  return res;
}


/*
 * GstElement methods
 */

static GstStateChangeReturn
gst_egl_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstEGLSink *egl_sink;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  GST_DEBUG ("change state");

  egl_sink = GST_EGL_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (!egl_sink->display && !egl_sink->draw_callback) {
        GST_INFO("Create GLDisplay");
        egl_sink->display = gst_gl_display_new ();
        egl_sink->display->keep_aspect_ratio = egl_sink->keep_aspect_ratio;
        /* init opengl context */
        gst_gl_display_create_context (egl_sink->display, 0);
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      if (egl_sink->display) {
        gst_gl_display_destroy_context(egl_sink->display);
        g_object_unref (egl_sink->display);
        egl_sink->display = NULL;
      }

      egl_sink->window_id = 0;
      //but do not reset egl_sink->new_window_id

      egl_sink->fps_n = 0;
      egl_sink->fps_d = 1;
      egl_sink->par_n = 1;
      egl_sink->par_d = 1;
      GST_VIDEO_SINK_WIDTH (egl_sink) = 0;
      GST_VIDEO_SINK_HEIGHT (egl_sink) = 0;
    }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static GstFlowReturn
gst_egl_sink_buffer_alloc (GstBaseSink * sink, 
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf)
{
  GstBuffer *buffer=NULL;
  GstEGLSink *egl_sink = GST_EGL_SINK (sink);
  if(egl_sink->get_buffer_callback)
    buffer = egl_sink->get_buffer_callback (caps, size, egl_sink->client_data);
  else if(egl_sink->display)
    buffer = GST_BUFFER_CAST(gst_gl_display_get_free_buffer(egl_sink->display, caps, size, TRUE));
  if(buffer)
    GST_BUFFER_OFFSET(buffer) = offset;
  *buf = buffer;
  GST_INFO("Alloc buffer return %p", *buf);
  return GST_FLOW_OK;
}

static void
gst_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstEGLSink *eglsink;

  eglsink = GST_EGL_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf))
      *end = *start + GST_BUFFER_DURATION (buf);
    else {
      if (eglsink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, eglsink->fps_d,
            eglsink->fps_n);
      }
    }
  }
}

static gboolean
gst_egl_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  GstEGLSink *egl_sink;
  gint width;
  gint height;
  gint bufcount;
  gboolean ok;
  gint fps_n, fps_d;
  gint par_n, par_d;
  gint display_par_n, display_par_d;
  guint display_ratio_num, display_ratio_den;
  GstVideoFormat format;
  GstStructure *s;

  GST_DEBUG ("set caps with %" GST_PTR_FORMAT, caps);

  egl_sink = GST_EGL_SINK (bsink);

  if(egl_sink->set_caps_callback)
    return egl_sink->set_caps_callback(caps, egl_sink->client_data);
  
  s = gst_caps_get_structure (caps, 0);
  if(gst_structure_get_int (s, "num-buffers-required", &bufcount) &&
      bufcount > GST_GL_DISPLAY_MAX_BUFFER_COUNT) {
    GST_WARNING("num-buffers-required %d exceed max eglsink buffer count %d", bufcount, GST_GL_DISPLAY_MAX_BUFFER_COUNT);
    return FALSE;
  }
  
  ok = gst_video_format_parse_caps (caps, &format, &width, &height);
  if (!ok)
    return FALSE;

  ok &= gst_video_parse_caps_framerate (caps, &fps_n, &fps_d);
  ok &= gst_video_parse_caps_pixel_aspect_ratio (caps, &par_n, &par_d);

  if (!ok)
    return FALSE;

  /* get display's PAR */
  if (egl_sink->par) {
    display_par_n = gst_value_get_fraction_numerator (egl_sink->par);
    display_par_d = gst_value_get_fraction_denominator (egl_sink->par);
  } else {
    display_par_n = 1;
    display_par_d = 1;
  }

  ok = gst_video_calculate_display_ratio (&display_ratio_num,
      &display_ratio_den, width, height, par_n, par_d, display_par_n,
      display_par_d);

  if (!ok)
    return FALSE;

  if (height % display_ratio_den == 0) {
    GST_DEBUG ("keeping video height");
    egl_sink->window_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    egl_sink->window_height = height;
  } else if (width % display_ratio_num == 0) {
    GST_DEBUG ("keeping video width");
    egl_sink->window_width = width;
    egl_sink->window_height = (guint)
        gst_util_uint64_scale_int (width, display_ratio_den, display_ratio_num);
  } else {
    GST_DEBUG ("approximating while keeping video height");
    egl_sink->window_width = (guint)
        gst_util_uint64_scale_int (height, display_ratio_num,
        display_ratio_den);
    egl_sink->window_height = height;
  }
  GST_DEBUG ("scaling to %dx%d",
      egl_sink->window_width, egl_sink->window_height);

  GST_VIDEO_SINK_WIDTH (egl_sink) = width;
  GST_VIDEO_SINK_HEIGHT (egl_sink) = height;
  egl_sink->fps_n = fps_n;
  egl_sink->fps_d = fps_d;
  egl_sink->par_n = par_n;
  egl_sink->par_d = par_d;

  if (!egl_sink->window_id && !egl_sink->new_window_id)
    gst_x_overlay_prepare_xwindow_id (GST_X_OVERLAY (egl_sink));

  return TRUE;
}

static GstFlowReturn
gst_egl_sink_show_frame (GstVideoSink *video_sink, GstBuffer *buf)
{
  GstEGLSink *egl_sink;
  GstEGLBuffer *egl_buffer;
  GstFlowReturn ret;

  egl_sink = GST_EGL_SINK (video_sink);

  GST_DEBUG("eglsink show_frame count %d\n", ++egl_sink->show_count);
  GST_INFO ("buffer %p, buffer size: %d", buf, GST_BUFFER_SIZE (buf));

  if(egl_sink->draw_callback)
    return egl_sink->draw_callback(buf, egl_sink->client_data);

  //is egl
  if (GST_IS_EGL_BUFFER(buf)) {
    GST_DEBUG("Direct rendering");
    //increment gl buffer ref before storage
    egl_buffer = GST_EGL_BUFFER (gst_buffer_ref(buf));
  }
  //is not egl
  else {
    egl_buffer = gst_gl_display_get_free_buffer (egl_sink->display,
        GST_BUFFER_CAPS(buf), -1, FALSE);
    if(egl_buffer)
      gst_gl_display_do_upload (egl_sink->display, egl_buffer, buf);
    else
      return GST_FLOW_UNEXPECTED;
  }

  if (egl_sink->window_id != egl_sink->new_window_id) {
    egl_sink->window_id = egl_sink->new_window_id;
    gst_gl_display_set_window_id (egl_sink->display,
        egl_sink->window_id);
  }

  GST_INFO("redisplay texture %d, width %d, height %d", egl_buffer->texinfo->texture,
      egl_sink->window_width, egl_sink->window_height);
  //redisplay opengl scene
  if (gst_gl_display_redisplay (egl_sink->display,
          egl_buffer,
          egl_sink->window_width, egl_sink->window_height,
          egl_sink->keep_aspect_ratio))
    ret = GST_FLOW_OK;
  else
    ret = GST_FLOW_UNEXPECTED;
  gst_egl_buffer_unref(egl_buffer);
  return ret;
}


static void
gst_egl_sink_xoverlay_init (GstXOverlayClass * iface)
{
  iface->set_xwindow_id = gst_egl_sink_set_xwindow_id;
  iface->expose = gst_egl_sink_expose;
}


static void
gst_egl_sink_set_xwindow_id (GstXOverlay * overlay, gulong window_id)
{
  GstEGLSink *egl_sink = GST_EGL_SINK (overlay);

  g_return_if_fail (GST_IS_EGL_SINK (overlay));

  GST_DEBUG ("set_xwindow_id %ld", window_id);

  egl_sink->new_window_id = window_id;
}


static void
gst_egl_sink_expose (GstXOverlay * overlay)
{
  GstEGLSink *egl_sink = GST_EGL_SINK (overlay);

  //redisplay opengl scene
  if (egl_sink->display && egl_sink->window_id) {

    if (egl_sink->window_id != egl_sink->new_window_id) {
      egl_sink->window_id = egl_sink->new_window_id;
      gst_gl_display_set_window_id (egl_sink->display,
          egl_sink->window_id);
    }

    gst_gl_display_redisplay (egl_sink->display, NULL, 0, 0,
        egl_sink->keep_aspect_ratio);
  }
}


static gboolean
gst_egl_sink_interface_supported (GstImplementsInterface * iface,
    GType type)
{
  return TRUE;
}


static void
gst_egl_sink_implements_init (GstImplementsInterfaceClass * klass)
{
  klass->supported = gst_egl_sink_interface_supported;
}
