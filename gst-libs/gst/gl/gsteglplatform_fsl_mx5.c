#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _LINUX
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglfslext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2amdext.h>
#undef  _LINUX
#undef  EGL_EGLEXT_PROTOTYPES
#undef  GL_GLEXT_PROTOTYPES

#include <string.h>

#include <gst/gstutils.h>
#include "gsteglplatform.h"
#include "gstgldisplay.h"
#include "gsteglbuffer.h"
#include "gst/fsl/gstbufmeta.h"

#define FSL_FRAGMENT_SOURCE(sampler) 				\
      "precision mediump float;                            \n"  \
      "varying vec2 v_texCoord;                            \n"  \
      "uniform " sampler " s_texture;                      \n"  \
      "void main()                                         \n"  \
      "{                                                   \n"  \
      "  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"  \
      "}                                                   \n"

#define IS_PLANAR_YUV420(fmt)                                          \
      (fmt == GST_VIDEO_FORMAT_I420 || fmt == GST_VIDEO_FORMAT_YV12)

#define IS_RGB32(fmt)                                                  \
      (fmt == GST_VIDEO_FORMAT_RGBA || fmt == GST_VIDEO_FORMAT_BGRA || \
       fmt == GST_VIDEO_FORMAT_RGBx || fmt == GST_VIDEO_FORMAT_BGRx)

static EGLenum
get_fsl_format(GstVideoFormat *format)
{
  switch(*format)
  {
    case GST_VIDEO_FORMAT_RGBx:
      *format = GST_VIDEO_FORMAT_RGBA;
      //fall through
    case GST_VIDEO_FORMAT_RGBA:
      return EGL_FORMAT_RGBA_8888_FSL;
    case GST_VIDEO_FORMAT_BGRx:
      *format = GST_VIDEO_FORMAT_BGRA;
      //fall through
    case GST_VIDEO_FORMAT_BGRA:
      return EGL_FORMAT_BGRA_8888_FSL;
    case GST_VIDEO_FORMAT_I420:
      *format = GST_VIDEO_FORMAT_YV12;
      //fall through
    case GST_VIDEO_FORMAT_YV12:
      return EGL_FORMAT_YUV_YV12_FSL;
    case GST_VIDEO_FORMAT_NV12:
      return EGL_FORMAT_YUV_NV21_FSL;
    case GST_VIDEO_FORMAT_UYVY:
      return EGL_FORMAT_YUV_UYVY_FSL;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", format);
      return 0;
  }
}

//////////////////////// global /////////////////////

gchar *
gst_egl_platform_get_vertex_source(GstVideoFormat format)
{
  return g_strdup(
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n"
  ); 
}

gchar *
gst_egl_platform_get_fragment_source(GstVideoFormat format)
{
  const gchar *fragment_base;
  switch(format)
  {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      fragment_base = FSL_FRAGMENT_SOURCE("sampler2D");
      break;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      fragment_base = FSL_FRAGMENT_SOURCE("YV12 samplerExternalOES");
      break;
    case GST_VIDEO_FORMAT_NV12:
      fragment_base = FSL_FRAGMENT_SOURCE("NV21 samplerExternalOES");
      break;
    case GST_VIDEO_FORMAT_UYVY:
      fragment_base = FSL_FRAGMENT_SOURCE("UYVY samplerExternalOES");
      break;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", format);
      return g_strdup("");
  }
  return g_strdup(fragment_base);
}

gint
gst_egl_platform_get_alignment_h(GstVideoFormat format)
{
  switch(format)
  {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      return 32;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
      return 64;
    case GST_VIDEO_FORMAT_UYVY:
	  return 32;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", format);
      return 1;
  }
}

gint
gst_egl_platform_get_alignment_v(GstVideoFormat format)
{
  switch(format)
  {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      return 1;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
	  return 64;
    case GST_VIDEO_FORMAT_UYVY:
	  return 1;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", format);
      return 1;
  }
}

void
gst_egl_platform_get_format_info(GstVideoFormat videoformat, GLenum *internalformat,
        GLenum *format, GLenum *type)
{
  switch(videoformat)
  {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
        *internalformat = GL_RGBA;
        *format = GL_RGBA;
        *type = GL_UNSIGNED_BYTE;
        break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
        *internalformat = GL_BGRA_EXT;
        *format = GL_BGRA_EXT;
        *type = GL_UNSIGNED_BYTE;
        break;
    case GST_VIDEO_FORMAT_YV12:
        *internalformat = GL_YUV_AMD;
        *format = GL_YUV_AMD;
        *type = GL_YV12_AMD;
        break;
    case GST_VIDEO_FORMAT_NV12:
        *internalformat = GL_YUV_AMD;
        *format = GL_YUV_AMD;
        *type = GL_NV21_AMD;
        break;
    case GST_VIDEO_FORMAT_UYVY:
        *internalformat = GL_YUV_AMD;
        *format = GL_YUV_AMD;
        *type = GL_UYVY_AMD;
        break;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", videoformat);
  }
}

void
gst_egl_platform_alloc_image(EGLDisplay display, GstEGLTexture *info)
{
  EGLImageKHR image;
  GstVideoFormat real_format = info->format;
  EGLint attribs[] =
  {
    EGL_WIDTH, info->width,
    EGL_HEIGHT, info->height,
    EGL_IMAGE_FORMAT_FSL, get_fsl_format(&real_format),
    EGL_NONE
  };
  struct EGLImageInfoFSL meta;
  info->real_format = real_format;
  image = eglCreateImageKHR(display, (EGLContext)0, EGL_NEW_IMAGE_FSL, (EGLClientBuffer)0, attribs);
  info->image = image;
  if(image)
  {
    GstBufferMeta *hw_meta = gst_buffer_meta_new();
    eglQueryImageFSL(display, image, EGL_CLIENTBUFFER_TYPE_FSL, (EGLint *)&meta);
    GST_INFO("Query Image FSL: virtual  address [%p, %p, %p]", meta.mem_virt[0], meta.mem_virt[1], meta.mem_virt[2]);
    GST_INFO("Query Image FSL: physical address [%p, %p, %p]", meta.mem_phy[0], meta.mem_phy[1], meta.mem_phy[2]);
    GST_INFO("Query Image FSL: stride %d", meta.stride);
    hw_meta->physical_data = (gpointer)meta.mem_phy[0];
    info->data = meta.mem_virt[0];
    info->hw_meta = hw_meta;
    if(real_format == GST_VIDEO_FORMAT_RGBA || real_format == GST_VIDEO_FORMAT_BGRA)
      info->stride = GST_ROUND_UP_32(info->width) * 4;
    else
      info->stride = meta.stride;
  }
  else
    GST_ERROR("Cannot alloc image [%d, %d] with format %d", info->width, info->height, info->format);
  return;
}

void
gst_egl_platform_free_image(EGLDisplay display, GstEGLTexture *info)
{
  eglDestroyImageKHR(display, info->image);
  gst_buffer_meta_free(info->hw_meta);
}

GLenum
gst_egl_platform_get_target(GstVideoFormat videoformat)
{
  switch(videoformat)
  {
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      return GL_TEXTURE_2D;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_UYVY:
      return GL_TEXTURE_EXTERNAL_OES;
    default:
      GST_ERROR("GstVideoFormat %d not supported by fsl", videoformat);
      return 0;
  }
}

gpointer
gst_egl_platform_dup_buffer_meta(gpointer meta)
{
  return gst_buffer_meta_copy(meta);
}

gboolean
gst_egl_platform_accept_caps(GstVideoFormat format, gint width, gint height)
{
  gchar *videoformat=NULL;
  //GST_INFO("format %d, width, height [%d, %d]", format, width, height);
  switch(format)
  {
    case GST_VIDEO_FORMAT_RGBA:
      if((width & 31) == 0)
        return TRUE;
      videoformat = "RGBA";
      break;
    case GST_VIDEO_FORMAT_BGRA:
      if((width & 31) == 0)
        return TRUE;
      videoformat = "BGRA";
      break;
    case GST_VIDEO_FORMAT_YV12:
      if((width & 63) == 0 && (height & 63) == 0)
        return TRUE;
      videoformat = "YV12";
      break;
    case GST_VIDEO_FORMAT_NV12:
      if((width & 63) == 0 && (height & 63) == 0)
        return TRUE;
      videoformat = "NV12";
      break;
    case GST_VIDEO_FORMAT_UYVY:
      if((width & 31) == 0)
        return TRUE;
      videoformat = "UYVY";
      break;
    default:
      GST_WARNING("GstVideoFormat %d not supported by fsl", format);
  }
  if(videoformat)
    GST_WARNING("caps width,height [%d, %d] doesn't meet %s alignment "
        "requirement, won't use direct rendering", width, height, videoformat);
  return FALSE;
}

static void
copy_planar_yuv420(gpointer src, gpointer dst, gint width,
        gint height, gint dst_stride)
{
  gchar *dstline, *srcline;
  gint i;
  gint src_stride = GST_ROUND_UP_4(width);
  gint src_height = GST_ROUND_UP_2(height);
  gint uv_src_stride = GST_ROUND_UP_8(width)/2;
  gint uv_dst_stride = GST_ROUND_UP_32(dst_stride/2);
  gint uv_src_height = src_height/2;
  gint uv_dst_height = GST_ROUND_UP_32(uv_src_height);
  gint y_dst_height = GST_ROUND_UP_32(height);
  gint uv_src_size = uv_src_stride * uv_src_height;
  gint uv_dst_size = uv_dst_stride * uv_dst_height;
  gint y_src_size = src_stride * src_height;
  gint y_dst_size = dst_stride * y_dst_height;
  gchar *src_u = (gchar*)src + y_src_size;
  gchar *src_v = src_u + uv_src_size;
  gchar *dst_u = (gchar*)(((guint32)dst + y_dst_size + 4095)&(~4095));
  gchar *dst_v = (gchar*)(((guint32)dst_u + uv_dst_size + 4095)&(~4095));

  GST_INFO("==== copy planar yuv420: [%d, %d], dst_stride %d\n", width, height, dst_stride);
  GST_INFO("==== copy planar yuv420: src yuv virtual addr:[%p, %p, %p]", src, src_u, src_v);
  GST_INFO("==== copy planar yuv420: dst yuv virtual addr:[%p, %p, %p]", dst, dst_u, dst_v);

  srcline = src;
  dstline = dst;
  for(i=0; i<height; i++) 	//Y
  {
	memcpy(dstline, srcline, src_stride);
	dstline += dst_stride;
	srcline += src_stride;
  }
  srcline = src_u;
  dstline = dst_u;
  for(i=0; i<uv_src_height; i++) //U
  {
    memcpy(dstline, srcline, uv_src_stride);
	dstline += uv_dst_stride;
	srcline += uv_src_stride;
  }
  srcline = src_v;
  dstline = dst_v;
  for(i=0; i<uv_src_height; i++) //V
  {
    memcpy(dstline, srcline, uv_src_stride);
	dstline += uv_dst_stride;
	srcline += uv_src_stride;
  }
}

static void
copy_rgba8888(gpointer src, gpointer dst, gint width,
        gint height, gint stride)
{
  gchar *dstline, *srcline;
  gint src_stride = width*4;
  gint i;
  dstline = dst;
  srcline = src;
  GST_INFO("==== copy rgb32: [%d, %d], stride %d\n", width, height, stride);
  for(i=0; i<height; i++) 	//Y
  {
    memcpy(dstline, srcline, src_stride);
    dstline += stride;
    srcline += src_stride;
  }
}

static void
convert_i420_yv12(gpointer src, gpointer dst, gint width,
		gint height, gint dst_stride)
{
  gchar *dstline, *srcline;
  gint i;
  gint src_stride = GST_ROUND_UP_4(width);
  gint src_height = GST_ROUND_UP_2(height);
  gint uv_src_stride = GST_ROUND_UP_8(width)/2;
  gint uv_dst_stride = GST_ROUND_UP_32(dst_stride/2);
  gint uv_src_height = src_height/2;
  gint uv_dst_height = GST_ROUND_UP_32(uv_src_height);
  gint y_dst_height = GST_ROUND_UP_32(height);
  gint uv_src_size = uv_src_stride * uv_src_height;
  gint uv_dst_size = uv_dst_stride * uv_dst_height;
  gint y_src_size = src_stride * src_height;
  gint y_dst_size = dst_stride * y_dst_height;
  gchar *src_u = (gchar*)src + y_src_size;
  gchar *src_v = src_u + uv_src_size;
  gchar *dst_v = (gchar*)(((guint32)dst + y_dst_size + 4095)&(~4095));
  gchar *dst_u = (gchar*)(((guint32)dst_v + uv_dst_size + 4095)&(~4095));

  GST_INFO("==== convert_i420_yv12: [%d, %d], src_stride %d, dst_stride %d", width, height, src_stride, dst_stride);
  GST_INFO("==== convert_i420_yv12: src yuv virtual addr:[%p, %p, %p]", src, src_u, src_v);
  GST_INFO("==== convert_i420_yv12: dst yuv virtual addr:[%p, %p, %p]", dst, dst_v, dst_u);

  srcline = src;
  dstline = dst;
  for(i=0; i<height; i++) 	//Y
  {
    memcpy(dstline, srcline, src_stride);
    dstline += dst_stride;
    srcline += src_stride;
  }
  srcline = src_u;
  dstline = dst_u;
  for(i=0; i<uv_src_height; i++) //U
  {
    memcpy(dstline, srcline, uv_src_stride);
    dstline += uv_dst_stride;
    srcline += uv_src_stride;
  }
  srcline = src_v;
  dstline = dst_v;
  for(i=0; i<uv_src_height; i++) //V
  {
    memcpy(dstline, srcline, uv_src_stride);
    dstline += uv_dst_stride;
    srcline += uv_src_stride;
  }
}

static void
convert_rgbx_rgba(gpointer src, gpointer dst, gint width,
		gint height, gint dst_stride)
{
  gchar *dstline, *srcline;
  gint src_stride = width*4;
  gint i, j;
  dstline = dst;
  srcline = src;
  GST_INFO("==== convert_rgbx_rgba: [%d, %d], stride %d\n", width, height, dst_stride);
  for(i=0; i<height; i++) 	//Y
  {
    gchar *x = dstline;
    memcpy(dstline, srcline, src_stride);
    for(j=0; j<width; j++) {
      *(x+3) = 0xFF;
      x+=4;
    }
    dstline += dst_stride;
    srcline += src_stride;
  }
}

gboolean
gst_egl_platform_convert_color_space(gpointer src, GstVideoFormat srcfmt, gpointer dst,
		GstVideoFormat dstfmt, gint width, gint height, gint stride)
{
  gboolean ret = TRUE;
  if((srcfmt == GST_VIDEO_FORMAT_I420 && dstfmt == GST_VIDEO_FORMAT_YV12) ||
  	 (srcfmt == GST_VIDEO_FORMAT_YV12 && dstfmt == GST_VIDEO_FORMAT_I420))
    convert_i420_yv12(src, dst, width, height, stride);
  else if(srcfmt == dstfmt)
  {
    if(IS_PLANAR_YUV420(srcfmt))
      copy_planar_yuv420(src, dst, width, height, stride);
    else if(IS_RGB32(srcfmt))
      copy_rgba8888(src, dst, width, height, stride);
    else
    {
      GST_ERROR("Cannot copy format %d", srcfmt);
      ret = FALSE;
    }
  }
  else if((srcfmt == GST_VIDEO_FORMAT_RGBx && dstfmt == GST_VIDEO_FORMAT_RGBA) ||
          (srcfmt == GST_VIDEO_FORMAT_BGRx && dstfmt == GST_VIDEO_FORMAT_BGRA))
    convert_rgbx_rgba(src, dst, width, height, stride);
  else
  {
    GST_ERROR("Cannot convert color space from %d to %d", srcfmt, dstfmt);
    ret = FALSE;
  }
  return ret;
}


