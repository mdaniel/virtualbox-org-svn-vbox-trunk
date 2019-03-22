/*
 * Copyright 2003 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <drm_fourcc.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "main/context.h"
#include "main/framebuffer.h"
#include "main/renderbuffer.h"
#include "main/texobj.h"
#include "main/hash.h"
#include "main/fbobject.h"
#include "main/version.h"
#include "swrast/s_renderbuffer.h"
#include "util/ralloc.h"
#include "brw_defines.h"
#include "brw_state.h"
#include "compiler/nir/nir.h"

#include "utils.h"
#include "util/xmlpool.h"

static const __DRIconfigOptionsExtension brw_config_options = {
   .base = { __DRI_CONFIG_OPTIONS, 1 },
   .xml =
DRI_CONF_BEGIN
   DRI_CONF_SECTION_PERFORMANCE
      /* Options correspond to DRI_CONF_BO_REUSE_DISABLED,
       * DRI_CONF_BO_REUSE_ALL
       */
      DRI_CONF_OPT_BEGIN_V(bo_reuse, enum, 1, "0:1")
	 DRI_CONF_DESC_BEGIN(en, "Buffer object reuse")
	    DRI_CONF_ENUM(0, "Disable buffer object reuse")
	    DRI_CONF_ENUM(1, "Enable reuse of all sizes of buffer objects")
	 DRI_CONF_DESC_END
      DRI_CONF_OPT_END
      DRI_CONF_MESA_NO_ERROR("false")
   DRI_CONF_SECTION_END

   DRI_CONF_SECTION_QUALITY
      DRI_CONF_PRECISE_TRIG("false")

      DRI_CONF_OPT_BEGIN(clamp_max_samples, int, -1)
              DRI_CONF_DESC(en, "Clamp the value of GL_MAX_SAMPLES to the "
                            "given integer. If negative, then do not clamp.")
      DRI_CONF_OPT_END
   DRI_CONF_SECTION_END

   DRI_CONF_SECTION_DEBUG
      DRI_CONF_NO_RAST("false")
      DRI_CONF_ALWAYS_FLUSH_BATCH("false")
      DRI_CONF_ALWAYS_FLUSH_CACHE("false")
      DRI_CONF_DISABLE_THROTTLING("false")
      DRI_CONF_FORCE_GLSL_EXTENSIONS_WARN("false")
      DRI_CONF_FORCE_GLSL_VERSION(0)
      DRI_CONF_DISABLE_GLSL_LINE_CONTINUATIONS("false")
      DRI_CONF_DISABLE_BLEND_FUNC_EXTENDED("false")
      DRI_CONF_DUAL_COLOR_BLEND_BY_LOCATION("false")
      DRI_CONF_ALLOW_GLSL_EXTENSION_DIRECTIVE_MIDSHADER("false")
      DRI_CONF_ALLOW_GLSL_BUILTIN_VARIABLE_REDECLARATION("false")
      DRI_CONF_ALLOW_GLSL_CROSS_STAGE_INTERPOLATION_MISMATCH("false")
      DRI_CONF_ALLOW_HIGHER_COMPAT_VERSION("false")
      DRI_CONF_FORCE_GLSL_ABS_SQRT("false")

      DRI_CONF_OPT_BEGIN_B(shader_precompile, "true")
	 DRI_CONF_DESC(en, "Perform code generation at shader link time.")
      DRI_CONF_OPT_END
   DRI_CONF_SECTION_END

   DRI_CONF_SECTION_MISCELLANEOUS
      DRI_CONF_GLSL_ZERO_INIT("false")
   DRI_CONF_SECTION_END
DRI_CONF_END
};

#include "intel_batchbuffer.h"
#include "intel_buffers.h"
#include "brw_bufmgr.h"
#include "intel_fbo.h"
#include "intel_mipmap_tree.h"
#include "intel_screen.h"
#include "intel_tex.h"
#include "intel_image.h"

#include "brw_context.h"

#include "i915_drm.h"

/**
 * For debugging purposes, this returns a time in seconds.
 */
double
get_time(void)
{
   struct timespec tp;

   clock_gettime(CLOCK_MONOTONIC, &tp);

   return tp.tv_sec + tp.tv_nsec / 1000000000.0;
}

static const __DRItexBufferExtension intelTexBufferExtension = {
   .base = { __DRI_TEX_BUFFER, 3 },

   .setTexBuffer        = intelSetTexBuffer,
   .setTexBuffer2       = intelSetTexBuffer2,
   .releaseTexBuffer    = NULL,
};

static void
intel_dri2_flush_with_flags(__DRIcontext *cPriv,
                            __DRIdrawable *dPriv,
                            unsigned flags,
                            enum __DRI2throttleReason reason)
{
   struct brw_context *brw = cPriv->driverPrivate;

   if (!brw)
      return;

   struct gl_context *ctx = &brw->ctx;

   FLUSH_VERTICES(ctx, 0);

   if (flags & __DRI2_FLUSH_DRAWABLE)
      intel_resolve_for_dri2_flush(brw, dPriv);

   if (reason == __DRI2_THROTTLE_SWAPBUFFER)
      brw->need_swap_throttle = true;
   if (reason == __DRI2_THROTTLE_FLUSHFRONT)
      brw->need_flush_throttle = true;

   intel_batchbuffer_flush(brw);
}

/**
 * Provides compatibility with loaders that only support the older (version
 * 1-3) flush interface.
 *
 * That includes libGL up to Mesa 9.0, and the X Server at least up to 1.13.
 */
static void
intel_dri2_flush(__DRIdrawable *drawable)
{
   intel_dri2_flush_with_flags(drawable->driContextPriv, drawable,
                               __DRI2_FLUSH_DRAWABLE,
                               __DRI2_THROTTLE_SWAPBUFFER);
}

static const struct __DRI2flushExtensionRec intelFlushExtension = {
    .base = { __DRI2_FLUSH, 4 },

    .flush              = intel_dri2_flush,
    .invalidate         = dri2InvalidateDrawable,
    .flush_with_flags   = intel_dri2_flush_with_flags,
};

static const struct intel_image_format intel_image_formats[] = {
   { __DRI_IMAGE_FOURCC_ARGB8888, __DRI_IMAGE_COMPONENTS_RGBA, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_ARGB8888, 4 } } },

   { __DRI_IMAGE_FOURCC_ABGR8888, __DRI_IMAGE_COMPONENTS_RGBA, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_ABGR8888, 4 } } },

   { __DRI_IMAGE_FOURCC_SARGB8888, __DRI_IMAGE_COMPONENTS_RGBA, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_SARGB8, 4 } } },

   { __DRI_IMAGE_FOURCC_XRGB8888, __DRI_IMAGE_COMPONENTS_RGB, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_XRGB8888, 4 }, } },

   { __DRI_IMAGE_FOURCC_XBGR8888, __DRI_IMAGE_COMPONENTS_RGB, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_XBGR8888, 4 }, } },

   { __DRI_IMAGE_FOURCC_ARGB1555, __DRI_IMAGE_COMPONENTS_RGBA, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_ARGB1555, 2 } } },

   { __DRI_IMAGE_FOURCC_RGB565, __DRI_IMAGE_COMPONENTS_RGB, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_RGB565, 2 } } },

   { __DRI_IMAGE_FOURCC_R8, __DRI_IMAGE_COMPONENTS_R, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 }, } },

   { __DRI_IMAGE_FOURCC_R16, __DRI_IMAGE_COMPONENTS_R, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R16, 1 }, } },

   { __DRI_IMAGE_FOURCC_GR88, __DRI_IMAGE_COMPONENTS_RG, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_GR88, 2 }, } },

   { __DRI_IMAGE_FOURCC_GR1616, __DRI_IMAGE_COMPONENTS_RG, 1,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_GR1616, 2 }, } },

   { __DRI_IMAGE_FOURCC_YUV410, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 2, 2, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 2, 2, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YUV411, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 2, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 2, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YUV420, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 1, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 1, 1, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YUV422, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 1, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YUV444, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YVU410, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 2, 2, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 2, 2, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YVU411, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 2, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 2, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YVU420, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 1, 1, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 1, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YVU422, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 1, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_YVU444, __DRI_IMAGE_COMPONENTS_Y_U_V, 3,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 2, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 } } },

   { __DRI_IMAGE_FOURCC_NV12, __DRI_IMAGE_COMPONENTS_Y_UV, 2,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 1, __DRI_IMAGE_FORMAT_GR88, 2 } } },

   { __DRI_IMAGE_FOURCC_NV16, __DRI_IMAGE_COMPONENTS_Y_UV, 2,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_R8, 1 },
       { 1, 1, 0, __DRI_IMAGE_FORMAT_GR88, 2 } } },

   /* For YUYV and UYVY buffers, we set up two overlapping DRI images
    * and treat them as planar buffers in the compositors.
    * Plane 0 is GR88 and samples YU or YV pairs and places Y into
    * the R component, while plane 1 is ARGB/ABGR and samples YUYV/UYVY
    * clusters and places pairs and places U into the G component and
    * V into A.  This lets the texture sampler interpolate the Y
    * components correctly when sampling from plane 0, and interpolate
    * U and V correctly when sampling from plane 1. */
   { __DRI_IMAGE_FOURCC_YUYV, __DRI_IMAGE_COMPONENTS_Y_XUXV, 2,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_GR88, 2 },
       { 0, 1, 0, __DRI_IMAGE_FORMAT_ARGB8888, 4 } } },
   { __DRI_IMAGE_FOURCC_UYVY, __DRI_IMAGE_COMPONENTS_Y_UXVX, 2,
     { { 0, 0, 0, __DRI_IMAGE_FORMAT_GR88, 2 },
       { 0, 1, 0, __DRI_IMAGE_FORMAT_ABGR8888, 4 } } }
};

static const struct {
   uint64_t modifier;
   unsigned since_gen;
} supported_modifiers[] = {
   { .modifier = DRM_FORMAT_MOD_LINEAR       , .since_gen = 1 },
   { .modifier = I915_FORMAT_MOD_X_TILED     , .since_gen = 1 },
   { .modifier = I915_FORMAT_MOD_Y_TILED     , .since_gen = 6 },
   { .modifier = I915_FORMAT_MOD_Y_TILED_CCS , .since_gen = 9 },
};

static bool
modifier_is_supported(const struct gen_device_info *devinfo,
                      const struct intel_image_format *fmt, int dri_format,
                      uint64_t modifier)
{
   const struct isl_drm_modifier_info *modinfo =
      isl_drm_modifier_get_info(modifier);
   int i;

   /* ISL had better know about the modifier */
   if (!modinfo)
      return false;

   if (modinfo->aux_usage == ISL_AUX_USAGE_CCS_E) {
      /* If INTEL_DEBUG=norbc is set, don't support any CCS_E modifiers */
      if (unlikely(INTEL_DEBUG & DEBUG_NO_RBC))
         return false;

      /* CCS_E is not supported for planar images */
      if (fmt && fmt->nplanes > 1)
         return false;

      if (fmt) {
         assert(dri_format == 0);
         dri_format = fmt->planes[0].dri_format;
      }

      mesa_format format = driImageFormatToGLFormat(dri_format);
      format = _mesa_get_srgb_format_linear(format);
      if (!isl_format_supports_ccs_e(devinfo,
                                     brw_isl_format_for_mesa_format(format)))
         return false;
   }

   for (i = 0; i < ARRAY_SIZE(supported_modifiers); i++) {
      if (supported_modifiers[i].modifier != modifier)
         continue;

      return supported_modifiers[i].since_gen <= devinfo->gen;
   }

   return false;
}

static uint64_t
tiling_to_modifier(uint32_t tiling)
{
   static const uint64_t map[] = {
      [I915_TILING_NONE]   = DRM_FORMAT_MOD_LINEAR,
      [I915_TILING_X]      = I915_FORMAT_MOD_X_TILED,
      [I915_TILING_Y]      = I915_FORMAT_MOD_Y_TILED,
   };

   assert(tiling < ARRAY_SIZE(map));

   return map[tiling];
}

static void
intel_image_warn_if_unaligned(__DRIimage *image, const char *func)
{
   uint32_t tiling, swizzle;
   brw_bo_get_tiling(image->bo, &tiling, &swizzle);

   if (tiling != I915_TILING_NONE && (image->offset & 0xfff)) {
      _mesa_warning(NULL, "%s: offset 0x%08x not on tile boundary",
                    func, image->offset);
   }
}

static const struct intel_image_format *
intel_image_format_lookup(int fourcc)
{
   for (unsigned i = 0; i < ARRAY_SIZE(intel_image_formats); i++) {
      if (intel_image_formats[i].fourcc == fourcc)
         return &intel_image_formats[i];
   }

   return NULL;
}

static boolean
intel_image_get_fourcc(__DRIimage *image, int *fourcc)
{
   if (image->planar_format) {
      *fourcc = image->planar_format->fourcc;
      return true;
   }

   for (unsigned i = 0; i < ARRAY_SIZE(intel_image_formats); i++) {
      if (intel_image_formats[i].planes[0].dri_format == image->dri_format) {
         *fourcc = intel_image_formats[i].fourcc;
         return true;
      }
   }
   return false;
}

static __DRIimage *
intel_allocate_image(struct intel_screen *screen, int dri_format,
                     void *loaderPrivate)
{
    __DRIimage *image;

    image = calloc(1, sizeof *image);
    if (image == NULL)
	return NULL;

    image->screen = screen;
    image->dri_format = dri_format;
    image->offset = 0;

    image->format = driImageFormatToGLFormat(dri_format);
    if (dri_format != __DRI_IMAGE_FORMAT_NONE &&
        image->format == MESA_FORMAT_NONE) {
       free(image);
       return NULL;
    }

    image->internal_format = _mesa_get_format_base_format(image->format);
    image->data = loaderPrivate;

    return image;
}

/**
 * Sets up a DRIImage structure to point to a slice out of a miptree.
 */
static void
intel_setup_image_from_mipmap_tree(struct brw_context *brw, __DRIimage *image,
                                   struct intel_mipmap_tree *mt, GLuint level,
                                   GLuint zoffset)
{
   intel_miptree_make_shareable(brw, mt);

   intel_miptree_check_level_layer(mt, level, zoffset);

   image->width = minify(mt->surf.phys_level0_sa.width,
                         level - mt->first_level);
   image->height = minify(mt->surf.phys_level0_sa.height,
                          level - mt->first_level);
   image->pitch = mt->surf.row_pitch;

   image->offset = intel_miptree_get_tile_offsets(mt, level, zoffset,
                                                  &image->tile_x,
                                                  &image->tile_y);

   brw_bo_unreference(image->bo);
   image->bo = mt->bo;
   brw_bo_reference(mt->bo);
}

static __DRIimage *
intel_create_image_from_name(__DRIscreen *dri_screen,
			     int width, int height, int format,
			     int name, int pitch, void *loaderPrivate)
{
    struct intel_screen *screen = dri_screen->driverPrivate;
    __DRIimage *image;
    int cpp;

    image = intel_allocate_image(screen, format, loaderPrivate);
    if (image == NULL)
       return NULL;

    if (image->format == MESA_FORMAT_NONE)
       cpp = 1;
    else
       cpp = _mesa_get_format_bytes(image->format);

    image->width = width;
    image->height = height;
    image->pitch = pitch * cpp;
    image->bo = brw_bo_gem_create_from_name(screen->bufmgr, "image",
                                                  name);
    if (!image->bo) {
       free(image);
       return NULL;
    }
    image->modifier = tiling_to_modifier(image->bo->tiling_mode);

    return image;
}

static __DRIimage *
intel_create_image_from_renderbuffer(__DRIcontext *context,
				     int renderbuffer, void *loaderPrivate)
{
   __DRIimage *image;
   struct brw_context *brw = context->driverPrivate;
   struct gl_context *ctx = &brw->ctx;
   struct gl_renderbuffer *rb;
   struct intel_renderbuffer *irb;

   rb = _mesa_lookup_renderbuffer(ctx, renderbuffer);
   if (!rb) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "glRenderbufferExternalMESA");
      return NULL;
   }

   irb = intel_renderbuffer(rb);
   intel_miptree_make_shareable(brw, irb->mt);
   image = calloc(1, sizeof *image);
   if (image == NULL)
      return NULL;

   image->internal_format = rb->InternalFormat;
   image->format = rb->Format;
   image->modifier = tiling_to_modifier(
                        isl_tiling_to_i915_tiling(irb->mt->surf.tiling));
   image->offset = 0;
   image->data = loaderPrivate;
   brw_bo_unreference(image->bo);
   image->bo = irb->mt->bo;
   brw_bo_reference(irb->mt->bo);
   image->width = rb->Width;
   image->height = rb->Height;
   image->pitch = irb->mt->surf.row_pitch;
   image->dri_format = driGLFormatToImageFormat(image->format);
   image->has_depthstencil = irb->mt->stencil_mt? true : false;

   rb->NeedsFinishRenderTexture = true;
   return image;
}

static __DRIimage *
intel_create_image_from_texture(__DRIcontext *context, int target,
                                unsigned texture, int zoffset,
                                int level,
                                unsigned *error,
                                void *loaderPrivate)
{
   __DRIimage *image;
   struct brw_context *brw = context->driverPrivate;
   struct gl_texture_object *obj;
   struct intel_texture_object *iobj;
   GLuint face = 0;

   obj = _mesa_lookup_texture(&brw->ctx, texture);
   if (!obj || obj->Target != target) {
      *error = __DRI_IMAGE_ERROR_BAD_PARAMETER;
      return NULL;
   }

   if (target == GL_TEXTURE_CUBE_MAP)
      face = zoffset;

   _mesa_test_texobj_completeness(&brw->ctx, obj);
   iobj = intel_texture_object(obj);
   if (!obj->_BaseComplete || (level > 0 && !obj->_MipmapComplete)) {
      *error = __DRI_IMAGE_ERROR_BAD_PARAMETER;
      return NULL;
   }

   if (level < obj->BaseLevel || level > obj->_MaxLevel) {
      *error = __DRI_IMAGE_ERROR_BAD_MATCH;
      return NULL;
   }

   if (target == GL_TEXTURE_3D && obj->Image[face][level]->Depth < zoffset) {
      *error = __DRI_IMAGE_ERROR_BAD_MATCH;
      return NULL;
   }
   image = calloc(1, sizeof *image);
   if (image == NULL) {
      *error = __DRI_IMAGE_ERROR_BAD_ALLOC;
      return NULL;
   }

   image->internal_format = obj->Image[face][level]->InternalFormat;
   image->format = obj->Image[face][level]->TexFormat;
   image->modifier = tiling_to_modifier(
                        isl_tiling_to_i915_tiling(iobj->mt->surf.tiling));
   image->data = loaderPrivate;
   intel_setup_image_from_mipmap_tree(brw, image, iobj->mt, level, zoffset);
   image->dri_format = driGLFormatToImageFormat(image->format);
   image->has_depthstencil = iobj->mt->stencil_mt? true : false;
   image->planar_format = iobj->planar_format;
   if (image->dri_format == MESA_FORMAT_NONE) {
      *error = __DRI_IMAGE_ERROR_BAD_PARAMETER;
      free(image);
      return NULL;
   }

   *error = __DRI_IMAGE_ERROR_SUCCESS;
   return image;
}

static void
intel_destroy_image(__DRIimage *image)
{
   brw_bo_unreference(image->bo);
   free(image);
}

enum modifier_priority {
   MODIFIER_PRIORITY_INVALID = 0,
   MODIFIER_PRIORITY_LINEAR,
   MODIFIER_PRIORITY_X,
   MODIFIER_PRIORITY_Y,
   MODIFIER_PRIORITY_Y_CCS,
};

const uint64_t priority_to_modifier[] = {
   [MODIFIER_PRIORITY_INVALID] = DRM_FORMAT_MOD_INVALID,
   [MODIFIER_PRIORITY_LINEAR] = DRM_FORMAT_MOD_LINEAR,
   [MODIFIER_PRIORITY_X] = I915_FORMAT_MOD_X_TILED,
   [MODIFIER_PRIORITY_Y] = I915_FORMAT_MOD_Y_TILED,
   [MODIFIER_PRIORITY_Y_CCS] = I915_FORMAT_MOD_Y_TILED_CCS,
};

static uint64_t
select_best_modifier(struct gen_device_info *devinfo,
                     int dri_format,
                     const uint64_t *modifiers,
                     const unsigned count)
{
   enum modifier_priority prio = MODIFIER_PRIORITY_INVALID;

   for (int i = 0; i < count; i++) {
      if (!modifier_is_supported(devinfo, NULL, dri_format, modifiers[i]))
         continue;

      switch (modifiers[i]) {
      case I915_FORMAT_MOD_Y_TILED_CCS:
         prio = MAX2(prio, MODIFIER_PRIORITY_Y_CCS);
         break;
      case I915_FORMAT_MOD_Y_TILED:
         prio = MAX2(prio, MODIFIER_PRIORITY_Y);
         break;
      case I915_FORMAT_MOD_X_TILED:
         prio = MAX2(prio, MODIFIER_PRIORITY_X);
         break;
      case DRM_FORMAT_MOD_LINEAR:
         prio = MAX2(prio, MODIFIER_PRIORITY_LINEAR);
         break;
      case DRM_FORMAT_MOD_INVALID:
      default:
         break;
      }
   }

   return priority_to_modifier[prio];
}

static __DRIimage *
intel_create_image_common(__DRIscreen *dri_screen,
                          int width, int height, int format,
                          unsigned int use,
                          const uint64_t *modifiers,
                          unsigned count,
                          void *loaderPrivate)
{
   __DRIimage *image;
   struct intel_screen *screen = dri_screen->driverPrivate;
   uint64_t modifier = DRM_FORMAT_MOD_INVALID;
   bool ok;

   /* Callers of this may specify a modifier, or a dri usage, but not both. The
    * newer modifier interface deprecates the older usage flags newer modifier
    * interface deprecates the older usage flags.
    */
   assert(!(use && count));

   if (use & __DRI_IMAGE_USE_CURSOR) {
      if (width != 64 || height != 64)
	 return NULL;
      modifier = DRM_FORMAT_MOD_LINEAR;
   }

   if (use & __DRI_IMAGE_USE_LINEAR)
      modifier = DRM_FORMAT_MOD_LINEAR;

   if (modifier == DRM_FORMAT_MOD_INVALID) {
      if (modifiers) {
         /* User requested specific modifiers */
         modifier = select_best_modifier(&screen->devinfo, format,
                                         modifiers, count);
         if (modifier == DRM_FORMAT_MOD_INVALID)
            return NULL;
      } else {
         /* Historically, X-tiled was the default, and so lack of modifier means
          * X-tiled.
          */
         modifier = I915_FORMAT_MOD_X_TILED;
      }
   }

   image = intel_allocate_image(screen, format, loaderPrivate);
   if (image == NULL)
      return NULL;

   const struct isl_drm_modifier_info *mod_info =
      isl_drm_modifier_get_info(modifier);

   struct isl_surf surf;
   ok = isl_surf_init(&screen->isl_dev, &surf,
                      .dim = ISL_SURF_DIM_2D,
                      .format = brw_isl_format_for_mesa_format(image->format),
                      .width = width,
                      .height = height,
                      .depth = 1,
                      .levels = 1,
                      .array_len = 1,
                      .samples = 1,
                      .usage = ISL_SURF_USAGE_RENDER_TARGET_BIT |
                               ISL_SURF_USAGE_TEXTURE_BIT |
                               ISL_SURF_USAGE_STORAGE_BIT,
                      .tiling_flags = (1 << mod_info->tiling));
   assert(ok);
   if (!ok) {
      free(image);
      return NULL;
   }

   struct isl_surf aux_surf;
   if (mod_info->aux_usage == ISL_AUX_USAGE_CCS_E) {
      ok = isl_surf_get_ccs_surf(&screen->isl_dev, &surf, &aux_surf, 0);
      if (!ok) {
         free(image);
         return NULL;
      }
   } else {
      assert(mod_info->aux_usage == ISL_AUX_USAGE_NONE);
      aux_surf.size = 0;
   }

   /* We request that the bufmgr zero the buffer for us for two reasons:
    *
    *  1) If a buffer gets re-used from the pool, we don't want to leak random
    *     garbage from our process to some other.
    *
    *  2) For images with CCS_E, we want to ensure that the CCS starts off in
    *     a valid state.  A CCS value of 0 indicates that the given block is
    *     in the pass-through state which is what we want.
    */
   image->bo = brw_bo_alloc_tiled(screen->bufmgr, "image",
                                  surf.size + aux_surf.size,
                                  isl_tiling_to_i915_tiling(mod_info->tiling),
                                  surf.row_pitch, BO_ALLOC_ZEROED);
   if (image->bo == NULL) {
      free(image);
      return NULL;
   }
   image->width = width;
   image->height = height;
   image->pitch = surf.row_pitch;
   image->modifier = modifier;

   if (aux_surf.size) {
      image->aux_offset = surf.size;
      image->aux_pitch = aux_surf.row_pitch;
      image->aux_size = aux_surf.size;
   }

   return image;
}

static __DRIimage *
intel_create_image(__DRIscreen *dri_screen,
		   int width, int height, int format,
		   unsigned int use,
		   void *loaderPrivate)
{
   return intel_create_image_common(dri_screen, width, height, format, use, NULL, 0,
                               loaderPrivate);
}

static __DRIimage *
intel_create_image_with_modifiers(__DRIscreen *dri_screen,
                                  int width, int height, int format,
                                  const uint64_t *modifiers,
                                  const unsigned count,
                                  void *loaderPrivate)
{
   return intel_create_image_common(dri_screen, width, height, format, 0,
                                    modifiers, count, loaderPrivate);
}

static GLboolean
intel_query_image(__DRIimage *image, int attrib, int *value)
{
   switch (attrib) {
   case __DRI_IMAGE_ATTRIB_STRIDE:
      *value = image->pitch;
      return true;
   case __DRI_IMAGE_ATTRIB_HANDLE:
      *value = brw_bo_export_gem_handle(image->bo);
      return true;
   case __DRI_IMAGE_ATTRIB_NAME:
      return !brw_bo_flink(image->bo, (uint32_t *) value);
   case __DRI_IMAGE_ATTRIB_FORMAT:
      *value = image->dri_format;
      return true;
   case __DRI_IMAGE_ATTRIB_WIDTH:
      *value = image->width;
      return true;
   case __DRI_IMAGE_ATTRIB_HEIGHT:
      *value = image->height;
      return true;
   case __DRI_IMAGE_ATTRIB_COMPONENTS:
      if (image->planar_format == NULL)
         return false;
      *value = image->planar_format->components;
      return true;
   case __DRI_IMAGE_ATTRIB_FD:
      return !brw_bo_gem_export_to_prime(image->bo, value);
   case __DRI_IMAGE_ATTRIB_FOURCC:
      return intel_image_get_fourcc(image, value);
   case __DRI_IMAGE_ATTRIB_NUM_PLANES:
      if (isl_drm_modifier_has_aux(image->modifier)) {
         assert(!image->planar_format || image->planar_format->nplanes == 1);
         *value = 2;
      } else if (image->planar_format) {
         *value = image->planar_format->nplanes;
      } else {
         *value = 1;
      }
      return true;
   case __DRI_IMAGE_ATTRIB_OFFSET:
      *value = image->offset;
      return true;
   case __DRI_IMAGE_ATTRIB_MODIFIER_LOWER:
      *value = (image->modifier & 0xffffffff);
      return true;
   case __DRI_IMAGE_ATTRIB_MODIFIER_UPPER:
      *value = ((image->modifier >> 32) & 0xffffffff);
      return true;

  default:
      return false;
   }
}

static GLboolean
intel_query_format_modifier_attribs(__DRIscreen *dri_screen,
                                    uint32_t fourcc, uint64_t modifier,
                                    int attrib, uint64_t *value)
{
   struct intel_screen *screen = dri_screen->driverPrivate;
   const struct intel_image_format *f = intel_image_format_lookup(fourcc);

   if (!modifier_is_supported(&screen->devinfo, f, 0, modifier))
      return false;

   switch (attrib) {
   case __DRI_IMAGE_FORMAT_MODIFIER_ATTRIB_PLANE_COUNT:
      *value = isl_drm_modifier_has_aux(modifier) ? 2 : f->nplanes;
      return true;

   default:
      return false;
   }
}

static __DRIimage *
intel_dup_image(__DRIimage *orig_image, void *loaderPrivate)
{
   __DRIimage *image;

   image = calloc(1, sizeof *image);
   if (image == NULL)
      return NULL;

   brw_bo_reference(orig_image->bo);
   image->bo              = orig_image->bo;
   image->internal_format = orig_image->internal_format;
   image->planar_format   = orig_image->planar_format;
   image->dri_format      = orig_image->dri_format;
   image->format          = orig_image->format;
   image->modifier        = orig_image->modifier;
   image->offset          = orig_image->offset;
   image->width           = orig_image->width;
   image->height          = orig_image->height;
   image->pitch           = orig_image->pitch;
   image->tile_x          = orig_image->tile_x;
   image->tile_y          = orig_image->tile_y;
   image->has_depthstencil = orig_image->has_depthstencil;
   image->data            = loaderPrivate;
   image->dma_buf_imported = orig_image->dma_buf_imported;
   image->aux_offset      = orig_image->aux_offset;
   image->aux_pitch       = orig_image->aux_pitch;

   memcpy(image->strides, orig_image->strides, sizeof(image->strides));
   memcpy(image->offsets, orig_image->offsets, sizeof(image->offsets));

   return image;
}

static GLboolean
intel_validate_usage(__DRIimage *image, unsigned int use)
{
   if (use & __DRI_IMAGE_USE_CURSOR) {
      if (image->width != 64 || image->height != 64)
	 return GL_FALSE;
   }

   return GL_TRUE;
}

static __DRIimage *
intel_create_image_from_names(__DRIscreen *dri_screen,
                              int width, int height, int fourcc,
                              int *names, int num_names,
                              int *strides, int *offsets,
                              void *loaderPrivate)
{
    const struct intel_image_format *f = NULL;
    __DRIimage *image;
    int i, index;

    if (dri_screen == NULL || names == NULL || num_names != 1)
        return NULL;

    f = intel_image_format_lookup(fourcc);
    if (f == NULL)
        return NULL;

    image = intel_create_image_from_name(dri_screen, width, height,
                                         __DRI_IMAGE_FORMAT_NONE,
                                         names[0], strides[0],
                                         loaderPrivate);

   if (image == NULL)
      return NULL;

    image->planar_format = f;
    for (i = 0; i < f->nplanes; i++) {
        index = f->planes[i].buffer_index;
        image->offsets[index] = offsets[index];
        image->strides[index] = strides[index];
    }

    return image;
}

static __DRIimage *
intel_create_image_from_fds_common(__DRIscreen *dri_screen,
                                   int width, int height, int fourcc,
                                   uint64_t modifier, int *fds, int num_fds,
                                   int *strides, int *offsets,
                                   void *loaderPrivate)
{
   struct intel_screen *screen = dri_screen->driverPrivate;
   const struct intel_image_format *f;
   __DRIimage *image;
   int i, index;
   bool ok;

   if (fds == NULL || num_fds < 1)
      return NULL;

   f = intel_image_format_lookup(fourcc);
   if (f == NULL)
      return NULL;

   if (modifier != DRM_FORMAT_MOD_INVALID &&
       !modifier_is_supported(&screen->devinfo, f, 0, modifier))
      return NULL;

   if (f->nplanes == 1)
      image = intel_allocate_image(screen, f->planes[0].dri_format,
                                   loaderPrivate);
   else
      image = intel_allocate_image(screen, __DRI_IMAGE_FORMAT_NONE,
                                   loaderPrivate);

   if (image == NULL)
      return NULL;

   image->width = width;
   image->height = height;
   image->pitch = strides[0];

   image->planar_format = f;

   if (modifier != DRM_FORMAT_MOD_INVALID) {
      const struct isl_drm_modifier_info *mod_info =
         isl_drm_modifier_get_info(modifier);
      uint32_t tiling = isl_tiling_to_i915_tiling(mod_info->tiling);
      image->bo = brw_bo_gem_create_from_prime_tiled(screen->bufmgr, fds[0],
                                                     tiling, strides[0]);
   } else {
      image->bo = brw_bo_gem_create_from_prime(screen->bufmgr, fds[0]);
   }

   if (image->bo == NULL) {
      free(image);
      return NULL;
   }

   /* We only support all planes from the same bo.
    * brw_bo_gem_create_from_prime() should return the same pointer for all
    * fds received here */
   for (i = 1; i < num_fds; i++) {
      struct brw_bo *aux = brw_bo_gem_create_from_prime(screen->bufmgr, fds[i]);
      brw_bo_unreference(aux);
      if (aux != image->bo) {
         brw_bo_unreference(image->bo);
         free(image);
         return NULL;
      }
   }

   if (modifier != DRM_FORMAT_MOD_INVALID)
      image->modifier = modifier;
   else
      image->modifier = tiling_to_modifier(image->bo->tiling_mode);

   const struct isl_drm_modifier_info *mod_info =
      isl_drm_modifier_get_info(image->modifier);

   int size = 0;
   struct isl_surf surf;
   for (i = 0; i < f->nplanes; i++) {
      index = f->planes[i].buffer_index;
      image->offsets[index] = offsets[index];
      image->strides[index] = strides[index];

      mesa_format format = driImageFormatToGLFormat(f->planes[i].dri_format);

      ok = isl_surf_init(&screen->isl_dev, &surf,
                         .dim = ISL_SURF_DIM_2D,
                         .format = brw_isl_format_for_mesa_format(format),
                         .width = image->width >> f->planes[i].width_shift,
                         .height = image->height >> f->planes[i].height_shift,
                         .depth = 1,
                         .levels = 1,
                         .array_len = 1,
                         .samples = 1,
                         .row_pitch = strides[index],
                         .usage = ISL_SURF_USAGE_RENDER_TARGET_BIT |
                                  ISL_SURF_USAGE_TEXTURE_BIT |
                                  ISL_SURF_USAGE_STORAGE_BIT,
                         .tiling_flags = (1 << mod_info->tiling));
      if (!ok) {
         brw_bo_unreference(image->bo);
         free(image);
         return NULL;
      }

      const int end = offsets[index] + surf.size;
      if (size < end)
         size = end;
   }

   if (mod_info->aux_usage == ISL_AUX_USAGE_CCS_E) {
      /* Even though we initialize surf in the loop above, we know that
       * anything with CCS_E will have exactly one plane so surf is properly
       * initialized when we get here.
       */
      assert(f->nplanes == 1);

      image->aux_offset = offsets[1];
      image->aux_pitch = strides[1];

      /* Scanout hardware requires that the CCS be placed after the main
       * surface in memory.  We consider any CCS that is placed any earlier in
       * memory to be invalid and reject it.
       *
       * At some point in the future, this restriction may be relaxed if the
       * hardware becomes less strict but we may need a new modifier for that.
       */
      assert(size > 0);
      if (image->aux_offset < size) {
         brw_bo_unreference(image->bo);
         free(image);
         return NULL;
      }

      struct isl_surf aux_surf;
      ok = isl_surf_get_ccs_surf(&screen->isl_dev, &surf, &aux_surf,
                                 image->aux_pitch);
      if (!ok) {
         brw_bo_unreference(image->bo);
         free(image);
         return NULL;
      }

      image->aux_size = aux_surf.size;

      const int end = image->aux_offset + aux_surf.size;
      if (size < end)
         size = end;
   } else {
      assert(mod_info->aux_usage == ISL_AUX_USAGE_NONE);
   }

   /* Check that the requested image actually fits within the BO. 'size'
    * is already relative to the offsets, so we don't need to add that. */
   if (image->bo->size == 0) {
      image->bo->size = size;
   } else if (size > image->bo->size) {
      brw_bo_unreference(image->bo);
      free(image);
      return NULL;
   }

   if (f->nplanes == 1) {
      image->offset = image->offsets[0];
      intel_image_warn_if_unaligned(image, __func__);
   }

   return image;
}

static __DRIimage *
intel_create_image_from_fds(__DRIscreen *dri_screen,
                            int width, int height, int fourcc,
                            int *fds, int num_fds, int *strides, int *offsets,
                            void *loaderPrivate)
{
   return intel_create_image_from_fds_common(dri_screen, width, height, fourcc,
                                             DRM_FORMAT_MOD_INVALID,
                                             fds, num_fds, strides, offsets,
                                             loaderPrivate);
}

static __DRIimage *
intel_create_image_from_dma_bufs2(__DRIscreen *dri_screen,
                                  int width, int height,
                                  int fourcc, uint64_t modifier,
                                  int *fds, int num_fds,
                                  int *strides, int *offsets,
                                  enum __DRIYUVColorSpace yuv_color_space,
                                  enum __DRISampleRange sample_range,
                                  enum __DRIChromaSiting horizontal_siting,
                                  enum __DRIChromaSiting vertical_siting,
                                  unsigned *error,
                                  void *loaderPrivate)
{
   __DRIimage *image;
   const struct intel_image_format *f = intel_image_format_lookup(fourcc);

   if (!f) {
      *error = __DRI_IMAGE_ERROR_BAD_MATCH;
      return NULL;
   }

   image = intel_create_image_from_fds_common(dri_screen, width, height,
                                              fourcc, modifier,
                                              fds, num_fds, strides, offsets,
                                              loaderPrivate);

   /*
    * Invalid parameters and any inconsistencies between are assumed to be
    * checked by the caller. Therefore besides unsupported formats one can fail
    * only in allocation.
    */
   if (!image) {
      *error = __DRI_IMAGE_ERROR_BAD_ALLOC;
      return NULL;
   }

   image->dma_buf_imported = true;
   image->yuv_color_space = yuv_color_space;
   image->sample_range = sample_range;
   image->horizontal_siting = horizontal_siting;
   image->vertical_siting = vertical_siting;

   *error = __DRI_IMAGE_ERROR_SUCCESS;
   return image;
}

static __DRIimage *
intel_create_image_from_dma_bufs(__DRIscreen *dri_screen,
                                 int width, int height, int fourcc,
                                 int *fds, int num_fds,
                                 int *strides, int *offsets,
                                 enum __DRIYUVColorSpace yuv_color_space,
                                 enum __DRISampleRange sample_range,
                                 enum __DRIChromaSiting horizontal_siting,
                                 enum __DRIChromaSiting vertical_siting,
                                 unsigned *error,
                                 void *loaderPrivate)
{
   return intel_create_image_from_dma_bufs2(dri_screen, width, height,
                                            fourcc, DRM_FORMAT_MOD_INVALID,
                                            fds, num_fds, strides, offsets,
                                            yuv_color_space,
                                            sample_range,
                                            horizontal_siting,
                                            vertical_siting,
                                            error,
                                            loaderPrivate);
}

static GLboolean
intel_query_dma_buf_formats(__DRIscreen *screen, int max,
                            int *formats, int *count)
{
   int i, j = 0;

   if (max == 0) {
      *count = ARRAY_SIZE(intel_image_formats) - 1; /* not SARGB */
      return true;
   }

   for (i = 0; i < (ARRAY_SIZE(intel_image_formats)) && j < max; i++) {
     if (intel_image_formats[i].fourcc == __DRI_IMAGE_FOURCC_SARGB8888)
       continue;
     formats[j++] = intel_image_formats[i].fourcc;
   }

   *count = j;
   return true;
}

static GLboolean
intel_query_dma_buf_modifiers(__DRIscreen *_screen, int fourcc, int max,
                              uint64_t *modifiers,
                              unsigned int *external_only,
                              int *count)
{
   struct intel_screen *screen = _screen->driverPrivate;
   const struct intel_image_format *f;
   int num_mods = 0, i;

   f = intel_image_format_lookup(fourcc);
   if (f == NULL)
      return false;

   for (i = 0; i < ARRAY_SIZE(supported_modifiers); i++) {
      uint64_t modifier = supported_modifiers[i].modifier;
      if (!modifier_is_supported(&screen->devinfo, f, 0, modifier))
         continue;

      num_mods++;
      if (max == 0)
         continue;

      modifiers[num_mods - 1] = modifier;
      if (num_mods >= max)
        break;
   }

   if (external_only != NULL) {
      for (i = 0; i < num_mods && i < max; i++) {
         if (f->components == __DRI_IMAGE_COMPONENTS_Y_U_V ||
             f->components == __DRI_IMAGE_COMPONENTS_Y_UV ||
             f->components == __DRI_IMAGE_COMPONENTS_Y_XUXV) {
            external_only[i] = GL_TRUE;
         }
         else {
            external_only[i] = GL_FALSE;
         }
      }
   }

   *count = num_mods;
   return true;
}

static __DRIimage *
intel_from_planar(__DRIimage *parent, int plane, void *loaderPrivate)
{
    int width, height, offset, stride, size, dri_format;
    __DRIimage *image;

    if (parent == NULL)
       return NULL;

    width = parent->width;
    height = parent->height;

    const struct intel_image_format *f = parent->planar_format;

    if (f && plane < f->nplanes) {
       /* Use the planar format definition. */
       width >>= f->planes[plane].width_shift;
       height >>= f->planes[plane].height_shift;
       dri_format = f->planes[plane].dri_format;
       int index = f->planes[plane].buffer_index;
       offset = parent->offsets[index];
       stride = parent->strides[index];
       size = height * stride;
    } else if (plane == 0) {
       /* The only plane of a non-planar image: copy the parent definition
        * directly. */
       dri_format = parent->dri_format;
       offset = parent->offset;
       stride = parent->pitch;
       size = height * stride;
    } else if (plane == 1 && parent->modifier != DRM_FORMAT_MOD_INVALID &&
               isl_drm_modifier_has_aux(parent->modifier)) {
       /* Auxiliary plane */
       dri_format = parent->dri_format;
       offset = parent->aux_offset;
       stride = parent->aux_pitch;
       size = parent->aux_size;
    } else {
       return NULL;
    }

    if (offset + size > parent->bo->size) {
       _mesa_warning(NULL, "intel_from_planar: subimage out of bounds");
       return NULL;
    }

    image = intel_allocate_image(parent->screen, dri_format, loaderPrivate);
    if (image == NULL)
       return NULL;

    image->bo = parent->bo;
    brw_bo_reference(parent->bo);
    image->modifier = parent->modifier;

    image->width = width;
    image->height = height;
    image->pitch = stride;
    image->offset = offset;

    intel_image_warn_if_unaligned(image, __func__);

    return image;
}

static const __DRIimageExtension intelImageExtension = {
    .base = { __DRI_IMAGE, 16 },

    .createImageFromName                = intel_create_image_from_name,
    .createImageFromRenderbuffer        = intel_create_image_from_renderbuffer,
    .destroyImage                       = intel_destroy_image,
    .createImage                        = intel_create_image,
    .queryImage                         = intel_query_image,
    .dupImage                           = intel_dup_image,
    .validateUsage                      = intel_validate_usage,
    .createImageFromNames               = intel_create_image_from_names,
    .fromPlanar                         = intel_from_planar,
    .createImageFromTexture             = intel_create_image_from_texture,
    .createImageFromFds                 = intel_create_image_from_fds,
    .createImageFromDmaBufs             = intel_create_image_from_dma_bufs,
    .blitImage                          = NULL,
    .getCapabilities                    = NULL,
    .mapImage                           = NULL,
    .unmapImage                         = NULL,
    .createImageWithModifiers           = intel_create_image_with_modifiers,
    .createImageFromDmaBufs2            = intel_create_image_from_dma_bufs2,
    .queryDmaBufFormats                 = intel_query_dma_buf_formats,
    .queryDmaBufModifiers               = intel_query_dma_buf_modifiers,
    .queryDmaBufFormatModifierAttribs   = intel_query_format_modifier_attribs,
};

static uint64_t
get_aperture_size(int fd)
{
   struct drm_i915_gem_get_aperture aperture;

   if (drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture) != 0)
      return 0;

   return aperture.aper_size;
}

static int
brw_query_renderer_integer(__DRIscreen *dri_screen,
                           int param, unsigned int *value)
{
   const struct intel_screen *const screen =
      (struct intel_screen *) dri_screen->driverPrivate;

   switch (param) {
   case __DRI2_RENDERER_VENDOR_ID:
      value[0] = 0x8086;
      return 0;
   case __DRI2_RENDERER_DEVICE_ID:
      value[0] = screen->deviceID;
      return 0;
   case __DRI2_RENDERER_ACCELERATED:
      value[0] = 1;
      return 0;
   case __DRI2_RENDERER_VIDEO_MEMORY: {
      /* Once a batch uses more than 75% of the maximum mappable size, we
       * assume that there's some fragmentation, and we start doing extra
       * flushing, etc.  That's the big cliff apps will care about.
       */
      const unsigned gpu_mappable_megabytes =
         screen->aperture_threshold / (1024 * 1024);

      const long system_memory_pages = sysconf(_SC_PHYS_PAGES);
      const long system_page_size = sysconf(_SC_PAGE_SIZE);

      if (system_memory_pages <= 0 || system_page_size <= 0)
         return -1;

      const uint64_t system_memory_bytes = (uint64_t) system_memory_pages
         * (uint64_t) system_page_size;

      const unsigned system_memory_megabytes =
         (unsigned) (system_memory_bytes / (1024 * 1024));

      value[0] = MIN2(system_memory_megabytes, gpu_mappable_megabytes);
      return 0;
   }
   case __DRI2_RENDERER_UNIFIED_MEMORY_ARCHITECTURE:
      value[0] = 1;
      return 0;
   case __DRI2_RENDERER_HAS_TEXTURE_3D:
      value[0] = 1;
      return 0;
   case __DRI2_RENDERER_HAS_CONTEXT_PRIORITY:
      value[0] = 0;
      if (brw_hw_context_set_priority(screen->bufmgr,
				      0, BRW_CONTEXT_HIGH_PRIORITY) == 0)
         value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_HIGH;
      if (brw_hw_context_set_priority(screen->bufmgr,
				      0, BRW_CONTEXT_LOW_PRIORITY) == 0)
         value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_LOW;
      /* reset to default last, just in case */
      if (brw_hw_context_set_priority(screen->bufmgr,
				      0, BRW_CONTEXT_MEDIUM_PRIORITY) == 0)
         value[0] |= __DRI2_RENDERER_HAS_CONTEXT_PRIORITY_MEDIUM;
      return 0;
   default:
      return driQueryRendererIntegerCommon(dri_screen, param, value);
   }

   return -1;
}

static int
brw_query_renderer_string(__DRIscreen *dri_screen,
                          int param, const char **value)
{
   const struct intel_screen *screen =
      (struct intel_screen *) dri_screen->driverPrivate;

   switch (param) {
   case __DRI2_RENDERER_VENDOR_ID:
      value[0] = brw_vendor_string;
      return 0;
   case __DRI2_RENDERER_DEVICE_ID:
      value[0] = brw_get_renderer_string(screen);
      return 0;
   default:
      break;
   }

   return -1;
}

static const __DRI2rendererQueryExtension intelRendererQueryExtension = {
   .base = { __DRI2_RENDERER_QUERY, 1 },

   .queryInteger = brw_query_renderer_integer,
   .queryString = brw_query_renderer_string
};

static const __DRIrobustnessExtension dri2Robustness = {
   .base = { __DRI2_ROBUSTNESS, 1 }
};

static const __DRIextension *screenExtensions[] = {
    &intelTexBufferExtension.base,
    &intelFenceExtension.base,
    &intelFlushExtension.base,
    &intelImageExtension.base,
    &intelRendererQueryExtension.base,
    &dri2ConfigQueryExtension.base,
    &dri2NoErrorExtension.base,
    NULL
};

static const __DRIextension *intelRobustScreenExtensions[] = {
    &intelTexBufferExtension.base,
    &intelFenceExtension.base,
    &intelFlushExtension.base,
    &intelImageExtension.base,
    &intelRendererQueryExtension.base,
    &dri2ConfigQueryExtension.base,
    &dri2Robustness.base,
    &dri2NoErrorExtension.base,
    NULL
};

static int
intel_get_param(struct intel_screen *screen, int param, int *value)
{
   int ret = 0;
   struct drm_i915_getparam gp;

   memset(&gp, 0, sizeof(gp));
   gp.param = param;
   gp.value = value;

   if (drmIoctl(screen->driScrnPriv->fd, DRM_IOCTL_I915_GETPARAM, &gp) == -1) {
      ret = -errno;
      if (ret != -EINVAL)
         _mesa_warning(NULL, "drm_i915_getparam: %d", ret);
   }

   return ret;
}

static bool
intel_get_boolean(struct intel_screen *screen, int param)
{
   int value = 0;
   return (intel_get_param(screen, param, &value) == 0) && value;
}

static int
intel_get_integer(struct intel_screen *screen, int param)
{
   int value = -1;

   if (intel_get_param(screen, param, &value) == 0)
      return value;

   return -1;
}

static void
intelDestroyScreen(__DRIscreen * sPriv)
{
   struct intel_screen *screen = sPriv->driverPrivate;

   brw_bufmgr_destroy(screen->bufmgr);
   driDestroyOptionInfo(&screen->optionCache);

   ralloc_free(screen);
   sPriv->driverPrivate = NULL;
}


/**
 * Create a gl_framebuffer and attach it to __DRIdrawable::driverPrivate.
 *
 *_This implements driDriverAPI::createNewDrawable, which the DRI layer calls
 * when creating a EGLSurface, GLXDrawable, or GLXPixmap. Despite the name,
 * this does not allocate GPU memory.
 */
static GLboolean
intelCreateBuffer(__DRIscreen *dri_screen,
                  __DRIdrawable * driDrawPriv,
                  const struct gl_config * mesaVis, GLboolean isPixmap)
{
   struct intel_renderbuffer *rb;
   struct intel_screen *screen = (struct intel_screen *)
      dri_screen->driverPrivate;
   mesa_format rgbFormat;
   unsigned num_samples =
      intel_quantize_num_samples(screen, mesaVis->samples);

   if (isPixmap)
      return false;

   struct gl_framebuffer *fb = CALLOC_STRUCT(gl_framebuffer);
   if (!fb)
      return false;

   _mesa_initialize_window_framebuffer(fb, mesaVis);

   if (screen->winsys_msaa_samples_override != -1) {
      num_samples = screen->winsys_msaa_samples_override;
      fb->Visual.samples = num_samples;
   }

   if (mesaVis->redBits == 5) {
      rgbFormat = mesaVis->redMask == 0x1f ? MESA_FORMAT_R5G6B5_UNORM
                                           : MESA_FORMAT_B5G6R5_UNORM;
   } else if (mesaVis->sRGBCapable) {
      rgbFormat = mesaVis->redMask == 0xff ? MESA_FORMAT_R8G8B8A8_SRGB
                                           : MESA_FORMAT_B8G8R8A8_SRGB;
   } else if (mesaVis->alphaBits == 0) {
      rgbFormat = mesaVis->redMask == 0xff ? MESA_FORMAT_R8G8B8X8_UNORM
                                           : MESA_FORMAT_B8G8R8X8_UNORM;
   } else {
      rgbFormat = mesaVis->redMask == 0xff ? MESA_FORMAT_R8G8B8A8_SRGB
                                           : MESA_FORMAT_B8G8R8A8_SRGB;
      fb->Visual.sRGBCapable = true;
   }

   /* setup the hardware-based renderbuffers */
   rb = intel_create_winsys_renderbuffer(screen, rgbFormat, num_samples);
   _mesa_attach_and_own_rb(fb, BUFFER_FRONT_LEFT, &rb->Base.Base);

   if (mesaVis->doubleBufferMode) {
      rb = intel_create_winsys_renderbuffer(screen, rgbFormat, num_samples);
      _mesa_attach_and_own_rb(fb, BUFFER_BACK_LEFT, &rb->Base.Base);
   }

   /*
    * Assert here that the gl_config has an expected depth/stencil bit
    * combination: one of d24/s8, d16/s0, d0/s0. (See intelInitScreen2(),
    * which constructs the advertised configs.)
    */
   if (mesaVis->depthBits == 24) {
      assert(mesaVis->stencilBits == 8);

      if (screen->devinfo.has_hiz_and_separate_stencil) {
         rb = intel_create_private_renderbuffer(screen,
                                                MESA_FORMAT_Z24_UNORM_X8_UINT,
                                                num_samples);
         _mesa_attach_and_own_rb(fb, BUFFER_DEPTH, &rb->Base.Base);
         rb = intel_create_private_renderbuffer(screen, MESA_FORMAT_S_UINT8,
                                                num_samples);
         _mesa_attach_and_own_rb(fb, BUFFER_STENCIL, &rb->Base.Base);
      } else {
         /*
          * Use combined depth/stencil. Note that the renderbuffer is
          * attached to two attachment points.
          */
         rb = intel_create_private_renderbuffer(screen,
                                                MESA_FORMAT_Z24_UNORM_S8_UINT,
                                                num_samples);
         _mesa_attach_and_own_rb(fb, BUFFER_DEPTH, &rb->Base.Base);
         _mesa_attach_and_reference_rb(fb, BUFFER_STENCIL, &rb->Base.Base);
      }
   }
   else if (mesaVis->depthBits == 16) {
      assert(mesaVis->stencilBits == 0);
      rb = intel_create_private_renderbuffer(screen, MESA_FORMAT_Z_UNORM16,
                                             num_samples);
      _mesa_attach_and_own_rb(fb, BUFFER_DEPTH, &rb->Base.Base);
   }
   else {
      assert(mesaVis->depthBits == 0);
      assert(mesaVis->stencilBits == 0);
   }

   /* now add any/all software-based renderbuffers we may need */
   _swrast_add_soft_renderbuffers(fb,
                                  false, /* never sw color */
                                  false, /* never sw depth */
                                  false, /* never sw stencil */
                                  mesaVis->accumRedBits > 0,
                                  false, /* never sw alpha */
                                  false  /* never sw aux */ );
   driDrawPriv->driverPrivate = fb;

   return true;
}

static void
intelDestroyBuffer(__DRIdrawable * driDrawPriv)
{
    struct gl_framebuffer *fb = driDrawPriv->driverPrivate;

    _mesa_reference_framebuffer(&fb, NULL);
}

static void
intel_detect_sseu(struct intel_screen *screen)
{
   assert(screen->devinfo.gen >= 8);
   int ret;

   screen->subslice_total = -1;
   screen->eu_total = -1;

   ret = intel_get_param(screen, I915_PARAM_SUBSLICE_TOTAL,
                         &screen->subslice_total);
   if (ret < 0 && ret != -EINVAL)
      goto err_out;

   ret = intel_get_param(screen,
                         I915_PARAM_EU_TOTAL, &screen->eu_total);
   if (ret < 0 && ret != -EINVAL)
      goto err_out;

   /* Without this information, we cannot get the right Braswell brandstrings,
    * and we have to use conservative numbers for GPGPU on many platforms, but
    * otherwise, things will just work.
    */
   if (screen->subslice_total < 1 || screen->eu_total < 1)
      _mesa_warning(NULL,
                    "Kernel 4.1 required to properly query GPU properties.\n");

   return;

err_out:
   screen->subslice_total = -1;
   screen->eu_total = -1;
   _mesa_warning(NULL, "Failed to query GPU properties (%s).\n", strerror(-ret));
}

static bool
intel_init_bufmgr(struct intel_screen *screen)
{
   __DRIscreen *dri_screen = screen->driScrnPriv;

   if (getenv("INTEL_NO_HW") != NULL)
      screen->no_hw = true;

   screen->bufmgr = brw_bufmgr_init(&screen->devinfo, dri_screen->fd);
   if (screen->bufmgr == NULL) {
      fprintf(stderr, "[%s:%u] Error initializing buffer manager.\n",
	      __func__, __LINE__);
      return false;
   }

   if (!intel_get_boolean(screen, I915_PARAM_HAS_EXEC_NO_RELOC)) {
      fprintf(stderr, "[%s: %u] Kernel 3.9 required.\n", __func__, __LINE__);
      return false;
   }

   return true;
}

static bool
intel_detect_swizzling(struct intel_screen *screen)
{
   struct brw_bo *buffer;
   unsigned flags = 0;
   uint32_t aligned_pitch;
   uint32_t tiling = I915_TILING_X;
   uint32_t swizzle_mode = 0;

   buffer = brw_bo_alloc_tiled_2d(screen->bufmgr, "swizzle test",
                                  64, 64, 4, tiling, &aligned_pitch, flags);
   if (buffer == NULL)
      return false;

   brw_bo_get_tiling(buffer, &tiling, &swizzle_mode);
   brw_bo_unreference(buffer);

   if (swizzle_mode == I915_BIT_6_SWIZZLE_NONE)
      return false;
   else
      return true;
}

static int
intel_detect_timestamp(struct intel_screen *screen)
{
   uint64_t dummy = 0, last = 0;
   int upper, lower, loops;

   /* On 64bit systems, some old kernels trigger a hw bug resulting in the
    * TIMESTAMP register being shifted and the low 32bits always zero.
    *
    * More recent kernels offer an interface to read the full 36bits
    * everywhere.
    */
   if (brw_reg_read(screen->bufmgr, TIMESTAMP | 1, &dummy) == 0)
      return 3;

   /* Determine if we have a 32bit or 64bit kernel by inspecting the
    * upper 32bits for a rapidly changing timestamp.
    */
   if (brw_reg_read(screen->bufmgr, TIMESTAMP, &last))
      return 0;

   upper = lower = 0;
   for (loops = 0; loops < 10; loops++) {
      /* The TIMESTAMP should change every 80ns, so several round trips
       * through the kernel should be enough to advance it.
       */
      if (brw_reg_read(screen->bufmgr, TIMESTAMP, &dummy))
         return 0;

      upper += (dummy >> 32) != (last >> 32);
      if (upper > 1) /* beware 32bit counter overflow */
         return 2; /* upper dword holds the low 32bits of the timestamp */

      lower += (dummy & 0xffffffff) != (last & 0xffffffff);
      if (lower > 1)
         return 1; /* timestamp is unshifted */

      last = dummy;
   }

   /* No advancement? No timestamp! */
   return 0;
}

 /**
 * Test if we can use MI_LOAD_REGISTER_MEM from an untrusted batchbuffer.
 *
 * Some combinations of hardware and kernel versions allow this feature,
 * while others don't.  Instead of trying to enumerate every case, just
 * try and write a register and see if works.
 */
static bool
intel_detect_pipelined_register(struct intel_screen *screen,
                                int reg, uint32_t expected_value, bool reset)
{
   if (screen->no_hw)
      return false;

   struct brw_bo *results, *bo;
   uint32_t *batch;
   uint32_t offset = 0;
   void *map;
   bool success = false;

   /* Create a zero'ed temporary buffer for reading our results */
   results = brw_bo_alloc(screen->bufmgr, "registers", 4096, 0);
   if (results == NULL)
      goto err;

   bo = brw_bo_alloc(screen->bufmgr, "batchbuffer", 4096, 0);
   if (bo == NULL)
      goto err_results;

   map = brw_bo_map(NULL, bo, MAP_WRITE);
   if (!map)
      goto err_batch;

   batch = map;

   /* Write the register. */
   *batch++ = MI_LOAD_REGISTER_IMM | (3 - 2);
   *batch++ = reg;
   *batch++ = expected_value;

   /* Save the register's value back to the buffer. */
   *batch++ = MI_STORE_REGISTER_MEM | (3 - 2);
   *batch++ = reg;
   struct drm_i915_gem_relocation_entry reloc = {
      .offset = (char *) batch - (char *) map,
      .delta = offset * sizeof(uint32_t),
      .target_handle = results->gem_handle,
      .read_domains = I915_GEM_DOMAIN_INSTRUCTION,
      .write_domain = I915_GEM_DOMAIN_INSTRUCTION,
   };
   *batch++ = reloc.presumed_offset + reloc.delta;

   /* And afterwards clear the register */
   if (reset) {
      *batch++ = MI_LOAD_REGISTER_IMM | (3 - 2);
      *batch++ = reg;
      *batch++ = 0;
   }

   *batch++ = MI_BATCH_BUFFER_END;

   struct drm_i915_gem_exec_object2 exec_objects[2] = {
      {
         .handle = results->gem_handle,
      },
      {
         .handle = bo->gem_handle,
         .relocation_count = 1,
         .relocs_ptr = (uintptr_t) &reloc,
      }
   };

   struct drm_i915_gem_execbuffer2 execbuf = {
      .buffers_ptr = (uintptr_t) exec_objects,
      .buffer_count = 2,
      .batch_len = ALIGN((char *) batch - (char *) map, 8),
      .flags = I915_EXEC_RENDER,
   };

   /* Don't bother with error checking - if the execbuf fails, the
    * value won't be written and we'll just report that there's no access.
    */
   __DRIscreen *dri_screen = screen->driScrnPriv;
   drmIoctl(dri_screen->fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);

   /* Check whether the value got written. */
   void *results_map = brw_bo_map(NULL, results, MAP_READ);
   if (results_map) {
      success = *((uint32_t *)results_map + offset) == expected_value;
      brw_bo_unmap(results);
   }

err_batch:
   brw_bo_unreference(bo);
err_results:
   brw_bo_unreference(results);
err:
   return success;
}

static bool
intel_detect_pipelined_so(struct intel_screen *screen)
{
   const struct gen_device_info *devinfo = &screen->devinfo;

   /* Supposedly, Broadwell just works. */
   if (devinfo->gen >= 8)
      return true;

   if (devinfo->gen <= 6)
      return false;

   /* See the big explanation about command parser versions below */
   if (screen->cmd_parser_version >= (devinfo->is_haswell ? 7 : 2))
      return true;

   /* We use SO_WRITE_OFFSET0 since you're supposed to write it (unlike the
    * statistics registers), and we already reset it to zero before using it.
    */
   return intel_detect_pipelined_register(screen,
                                          GEN7_SO_WRITE_OFFSET(0),
                                          0x1337d0d0,
                                          false);
}

/**
 * Return array of MSAA modes supported by the hardware. The array is
 * zero-terminated and sorted in decreasing order.
 */
const int*
intel_supported_msaa_modes(const struct intel_screen  *screen)
{
   static const int gen9_modes[] = {16, 8, 4, 2, 0, -1};
   static const int gen8_modes[] = {8, 4, 2, 0, -1};
   static const int gen7_modes[] = {8, 4, 0, -1};
   static const int gen6_modes[] = {4, 0, -1};
   static const int gen4_modes[] = {0, -1};

   if (screen->devinfo.gen >= 9) {
      return gen9_modes;
   } else if (screen->devinfo.gen >= 8) {
      return gen8_modes;
   } else if (screen->devinfo.gen >= 7) {
      return gen7_modes;
   } else if (screen->devinfo.gen == 6) {
      return gen6_modes;
   } else {
      return gen4_modes;
   }
}

static unsigned
intel_loader_get_cap(const __DRIscreen *dri_screen, enum dri_loader_cap cap)
{
   if (dri_screen->dri2.loader && dri_screen->dri2.loader->base.version >= 4 &&
       dri_screen->dri2.loader->getCapability)
      return dri_screen->dri2.loader->getCapability(dri_screen->loaderPrivate, cap);

   if (dri_screen->image.loader && dri_screen->image.loader->base.version >= 2 &&
       dri_screen->image.loader->getCapability)
      return dri_screen->image.loader->getCapability(dri_screen->loaderPrivate, cap);

   return 0;
}

static __DRIconfig**
intel_screen_make_configs(__DRIscreen *dri_screen)
{
   static const mesa_format formats[] = {
      MESA_FORMAT_B5G6R5_UNORM,
      MESA_FORMAT_B8G8R8A8_UNORM,
      MESA_FORMAT_B8G8R8X8_UNORM,

      /* The 32-bit RGBA format must not precede the 32-bit BGRA format.
       * Likewise for RGBX and BGRX.  Otherwise, the GLX client and the GLX
       * server may disagree on which format the GLXFBConfig represents,
       * resulting in swapped color channels.
       *
       * The problem, as of 2017-05-30:
       * When matching a GLXFBConfig to a __DRIconfig, GLX ignores the channel
       * order and chooses the first __DRIconfig with the expected channel
       * sizes. Specifically, GLX compares the GLXFBConfig's and __DRIconfig's
       * __DRI_ATTRIB_{CHANNEL}_SIZE but ignores __DRI_ATTRIB_{CHANNEL}_MASK.
       *
       * EGL does not suffer from this problem. It correctly compares the
       * channel masks when matching EGLConfig to __DRIconfig.
       */

      /* Required by Android, for HAL_PIXEL_FORMAT_RGBA_8888. */
      MESA_FORMAT_R8G8B8A8_UNORM,

      /* Required by Android, for HAL_PIXEL_FORMAT_RGBX_8888. */
      MESA_FORMAT_R8G8B8X8_UNORM,
   };

   /* GLX_SWAP_COPY_OML is not supported due to page flipping. */
   static const GLenum back_buffer_modes[] = {
      __DRI_ATTRIB_SWAP_UNDEFINED, __DRI_ATTRIB_SWAP_NONE
   };

   static const uint8_t singlesample_samples[1] = {0};

   struct intel_screen *screen = dri_screen->driverPrivate;
   const struct gen_device_info *devinfo = &screen->devinfo;
   uint8_t depth_bits[4], stencil_bits[4];
   __DRIconfig **configs = NULL;

   /* Expose only BGRA ordering if the loader doesn't support RGBA ordering. */
   unsigned num_formats;
   if (intel_loader_get_cap(dri_screen, DRI_LOADER_CAP_RGBA_ORDERING))
      num_formats = ARRAY_SIZE(formats);
   else
      num_formats = 3;

   /* Generate singlesample configs without accumulation buffer. */
   for (unsigned i = 0; i < num_formats; i++) {
      __DRIconfig **new_configs;
      int num_depth_stencil_bits = 2;

      /* Starting with DRI2 protocol version 1.1 we can request a depth/stencil
       * buffer that has a different number of bits per pixel than the color
       * buffer, gen >= 6 supports this.
       */
      depth_bits[0] = 0;
      stencil_bits[0] = 0;

      if (formats[i] == MESA_FORMAT_B5G6R5_UNORM) {
         depth_bits[1] = 16;
         stencil_bits[1] = 0;
         if (devinfo->gen >= 6) {
             depth_bits[2] = 24;
             stencil_bits[2] = 8;
             num_depth_stencil_bits = 3;
         }
      } else {
         depth_bits[1] = 24;
         stencil_bits[1] = 8;
      }

      new_configs = driCreateConfigs(formats[i],
                                     depth_bits,
                                     stencil_bits,
                                     num_depth_stencil_bits,
                                     back_buffer_modes, 2,
                                     singlesample_samples, 1,
                                     false, false);
      configs = driConcatConfigs(configs, new_configs);
   }

   /* Generate the minimum possible set of configs that include an
    * accumulation buffer.
    */
   for (unsigned i = 0; i < num_formats; i++) {
      __DRIconfig **new_configs;

      if (formats[i] == MESA_FORMAT_B5G6R5_UNORM) {
         depth_bits[0] = 16;
         stencil_bits[0] = 0;
      } else {
         depth_bits[0] = 24;
         stencil_bits[0] = 8;
      }

      new_configs = driCreateConfigs(formats[i],
                                     depth_bits, stencil_bits, 1,
                                     back_buffer_modes, 1,
                                     singlesample_samples, 1,
                                     true, false);
      configs = driConcatConfigs(configs, new_configs);
   }

   /* Generate multisample configs.
    *
    * This loop breaks early, and hence is a no-op, on gen < 6.
    *
    * Multisample configs must follow the singlesample configs in order to
    * work around an X server bug present in 1.12. The X server chooses to
    * associate the first listed RGBA888-Z24S8 config, regardless of its
    * sample count, with the 32-bit depth visual used for compositing.
    *
    * Only doublebuffer configs with GLX_SWAP_UNDEFINED_OML behavior are
    * supported.  Singlebuffer configs are not supported because no one wants
    * them.
    */
   for (unsigned i = 0; i < num_formats; i++) {
      if (devinfo->gen < 6)
         break;

      __DRIconfig **new_configs;
      const int num_depth_stencil_bits = 2;
      int num_msaa_modes = 0;
      const uint8_t *multisample_samples = NULL;

      depth_bits[0] = 0;
      stencil_bits[0] = 0;

      if (formats[i] == MESA_FORMAT_B5G6R5_UNORM) {
         depth_bits[1] = 16;
         stencil_bits[1] = 0;
      } else {
         depth_bits[1] = 24;
         stencil_bits[1] = 8;
      }

      if (devinfo->gen >= 9) {
         static const uint8_t multisample_samples_gen9[] = {2, 4, 8, 16};
         multisample_samples = multisample_samples_gen9;
         num_msaa_modes = ARRAY_SIZE(multisample_samples_gen9);
      } else if (devinfo->gen == 8) {
         static const uint8_t multisample_samples_gen8[] = {2, 4, 8};
         multisample_samples = multisample_samples_gen8;
         num_msaa_modes = ARRAY_SIZE(multisample_samples_gen8);
      } else if (devinfo->gen == 7) {
         static const uint8_t multisample_samples_gen7[] = {4, 8};
         multisample_samples = multisample_samples_gen7;
         num_msaa_modes = ARRAY_SIZE(multisample_samples_gen7);
      } else if (devinfo->gen == 6) {
         static const uint8_t multisample_samples_gen6[] = {4};
         multisample_samples = multisample_samples_gen6;
         num_msaa_modes = ARRAY_SIZE(multisample_samples_gen6);
      }

      new_configs = driCreateConfigs(formats[i],
                                     depth_bits,
                                     stencil_bits,
                                     num_depth_stencil_bits,
                                     back_buffer_modes, 1,
                                     multisample_samples,
                                     num_msaa_modes,
                                     false, false);
      configs = driConcatConfigs(configs, new_configs);
   }

   if (configs == NULL) {
      fprintf(stderr, "[%s:%u] Error creating FBConfig!\n", __func__,
              __LINE__);
      return NULL;
   }

   return configs;
}

static void
set_max_gl_versions(struct intel_screen *screen)
{
   __DRIscreen *dri_screen = screen->driScrnPriv;
   const bool has_astc = screen->devinfo.gen >= 9;

   switch (screen->devinfo.gen) {
   case 10:
   case 9:
   case 8:
      dri_screen->max_gl_core_version = 45;
      dri_screen->max_gl_compat_version = 30;
      dri_screen->max_gl_es1_version = 11;
      dri_screen->max_gl_es2_version = has_astc ? 32 : 31;
      break;
   case 7:
      dri_screen->max_gl_core_version = 33;
      if (can_do_pipelined_register_writes(screen)) {
         dri_screen->max_gl_core_version = 42;
         if (screen->devinfo.is_haswell && can_do_compute_dispatch(screen))
            dri_screen->max_gl_core_version = 43;
         if (screen->devinfo.is_haswell && can_do_mi_math_and_lrr(screen))
            dri_screen->max_gl_core_version = 45;
      }
      dri_screen->max_gl_compat_version = 30;
      dri_screen->max_gl_es1_version = 11;
      dri_screen->max_gl_es2_version = screen->devinfo.is_haswell ? 31 : 30;
      break;
   case 6:
      dri_screen->max_gl_core_version = 33;
      dri_screen->max_gl_compat_version = 30;
      dri_screen->max_gl_es1_version = 11;
      dri_screen->max_gl_es2_version = 30;
      break;
   case 5:
   case 4:
      dri_screen->max_gl_core_version = 0;
      dri_screen->max_gl_compat_version = 21;
      dri_screen->max_gl_es1_version = 11;
      dri_screen->max_gl_es2_version = 20;
      break;
   default:
      unreachable("unrecognized intel_screen::gen");
   }
}

/**
 * Return the revision (generally the revid field of the PCI header) of the
 * graphics device.
 *
 * XXX: This function is useful to keep around even if it is not currently in
 * use. It is necessary for new platforms and revision specific workarounds or
 * features. Please don't remove it so that we know it at least continues to
 * build.
 */
static __attribute__((__unused__)) int
brw_get_revision(int fd)
{
   struct drm_i915_getparam gp;
   int revision;
   int ret;

   memset(&gp, 0, sizeof(gp));
   gp.param = I915_PARAM_REVISION;
   gp.value = &revision;

   ret = drmCommandWriteRead(fd, DRM_I915_GETPARAM, &gp, sizeof(gp));
   if (ret)
      revision = -1;

   return revision;
}

static void
shader_debug_log_mesa(void *data, const char *fmt, ...)
{
   struct brw_context *brw = (struct brw_context *)data;
   va_list args;

   va_start(args, fmt);
   GLuint msg_id = 0;
   _mesa_gl_vdebug(&brw->ctx, &msg_id,
                   MESA_DEBUG_SOURCE_SHADER_COMPILER,
                   MESA_DEBUG_TYPE_OTHER,
                   MESA_DEBUG_SEVERITY_NOTIFICATION, fmt, args);
   va_end(args);
}

static void
shader_perf_log_mesa(void *data, const char *fmt, ...)
{
   struct brw_context *brw = (struct brw_context *)data;

   va_list args;
   va_start(args, fmt);

   if (unlikely(INTEL_DEBUG & DEBUG_PERF)) {
      va_list args_copy;
      va_copy(args_copy, args);
      vfprintf(stderr, fmt, args_copy);
      va_end(args_copy);
   }

   if (brw->perf_debug) {
      GLuint msg_id = 0;
      _mesa_gl_vdebug(&brw->ctx, &msg_id,
                      MESA_DEBUG_SOURCE_SHADER_COMPILER,
                      MESA_DEBUG_TYPE_PERFORMANCE,
                      MESA_DEBUG_SEVERITY_MEDIUM, fmt, args);
   }
   va_end(args);
}

static int
parse_devid_override(const char *devid_override)
{
   static const struct {
      const char *name;
      int pci_id;
   } name_map[] = {
      { "brw", 0x2a02 },
      { "g4x", 0x2a42 },
      { "ilk", 0x0042 },
      { "snb", 0x0126 },
      { "ivb", 0x016a },
      { "hsw", 0x0d2e },
      { "byt", 0x0f33 },
      { "bdw", 0x162e },
      { "chv", 0x22B3 },
      { "skl", 0x1912 },
      { "bxt", 0x5A85 },
      { "kbl", 0x5912 },
      { "glk", 0x3185 },
      { "cnl", 0x5a52 },
   };

   for (unsigned i = 0; i < ARRAY_SIZE(name_map); i++) {
      if (!strcmp(name_map[i].name, devid_override))
         return name_map[i].pci_id;
   }

   return strtol(devid_override, NULL, 0);
}

/**
 * Get the PCI ID for the device.  This can be overridden by setting the
 * INTEL_DEVID_OVERRIDE environment variable to the desired ID.
 *
 * Returns -1 on ioctl failure.
 */
static int
get_pci_device_id(struct intel_screen *screen)
{
   if (geteuid() == getuid()) {
      char *devid_override = getenv("INTEL_DEVID_OVERRIDE");
      if (devid_override) {
         screen->no_hw = true;
         return parse_devid_override(devid_override);
      }
   }

   return intel_get_integer(screen, I915_PARAM_CHIPSET_ID);
}

/**
 * This is the driver specific part of the createNewScreen entry point.
 * Called when using DRI2.
 *
 * \return the struct gl_config supported by this driver
 */
static const
__DRIconfig **intelInitScreen2(__DRIscreen *dri_screen)
{
   struct intel_screen *screen;

   if (dri_screen->image.loader) {
   } else if (dri_screen->dri2.loader->base.version <= 2 ||
       dri_screen->dri2.loader->getBuffersWithFormat == NULL) {
      fprintf(stderr,
	      "\nERROR!  DRI2 loader with getBuffersWithFormat() "
	      "support required\n");
      return NULL;
   }

   /* Allocate the private area */
   screen = rzalloc(NULL, struct intel_screen);
   if (!screen) {
      fprintf(stderr, "\nERROR!  Allocating private area failed\n");
      return NULL;
   }
   /* parse information in __driConfigOptions */
   driParseOptionInfo(&screen->optionCache, brw_config_options.xml);

   screen->driScrnPriv = dri_screen;
   dri_screen->driverPrivate = (void *) screen;

   screen->deviceID = get_pci_device_id(screen);

   if (!gen_get_device_info(screen->deviceID, &screen->devinfo))
      return NULL;

   if (!intel_init_bufmgr(screen))
       return NULL;

   const struct gen_device_info *devinfo = &screen->devinfo;

   brw_process_intel_debug_variable();

   if ((INTEL_DEBUG & DEBUG_SHADER_TIME) && devinfo->gen < 7) {
      fprintf(stderr,
              "shader_time debugging requires gen7 (Ivybridge) or better.\n");
      INTEL_DEBUG &= ~DEBUG_SHADER_TIME;
   }

   if (intel_get_integer(screen, I915_PARAM_MMAP_GTT_VERSION) >= 1) {
      /* Theorectically unlimited! At least for individual objects...
       *
       * Currently the entire (global) address space for all GTT maps is
       * limited to 64bits. That is all objects on the system that are
       * setup for GTT mmapping must fit within 64bits. An attempt to use
       * one that exceeds the limit with fail in brw_bo_map_gtt().
       *
       * Long before we hit that limit, we will be practically limited by
       * that any single object must fit in physical memory (RAM). The upper
       * limit on the CPU's address space is currently 48bits (Skylake), of
       * which only 39bits can be physical memory. (The GPU itself also has
       * a 48bit addressable virtual space.) We can fit over 32 million
       * objects of the current maximum allocable size before running out
       * of mmap space.
       */
      screen->max_gtt_map_object_size = UINT64_MAX;
   } else {
      /* Estimate the size of the mappable aperture into the GTT.  There's an
       * ioctl to get the whole GTT size, but not one to get the mappable subset.
       * It turns out it's basically always 256MB, though some ancient hardware
       * was smaller.
       */
      uint32_t gtt_size = 256 * 1024 * 1024;

      /* We don't want to map two objects such that a memcpy between them would
       * just fault one mapping in and then the other over and over forever.  So
       * we would need to divide the GTT size by 2.  Additionally, some GTT is
       * taken up by things like the framebuffer and the ringbuffer and such, so
       * be more conservative.
       */
      screen->max_gtt_map_object_size = gtt_size / 4;
   }

   screen->aperture_threshold = get_aperture_size(dri_screen->fd) * 3 / 4;

   screen->hw_has_swizzling = intel_detect_swizzling(screen);
   screen->hw_has_timestamp = intel_detect_timestamp(screen);

   isl_device_init(&screen->isl_dev, &screen->devinfo,
                   screen->hw_has_swizzling);

   /* GENs prior to 8 do not support EU/Subslice info */
   if (devinfo->gen >= 8) {
      intel_detect_sseu(screen);
   } else if (devinfo->gen == 7) {
      screen->subslice_total = 1 << (devinfo->gt - 1);
   }

   /* Gen7-7.5 kernel requirements / command parser saga:
    *
    * - pre-v3.16:
    *   Haswell and Baytrail cannot use any privileged batchbuffer features.
    *
    *   Ivybridge has aliasing PPGTT on by default, which accidentally marks
    *   all batches secure, allowing them to use any feature with no checking.
    *   This is effectively equivalent to a command parser version of
    *   \infinity - everything is possible.
    *
    *   The command parser does not exist, and querying the version will
    *   return -EINVAL.
    *
    * - v3.16:
    *   The kernel enables the command parser by default, for systems with
    *   aliasing PPGTT enabled (Ivybridge and Haswell).  However, the
    *   hardware checker is still enabled, so Haswell and Baytrail cannot
    *   do anything.
    *
    *   Ivybridge goes from "everything is possible" to "only what the
    *   command parser allows" (if the user boots with i915.cmd_parser=0,
    *   then everything is possible again).  We can only safely use features
    *   allowed by the supported command parser version.
    *
    *   Annoyingly, I915_PARAM_CMD_PARSER_VERSION reports the static version
    *   implemented by the kernel, even if it's turned off.  So, checking
    *   for version > 0 does not mean that you can write registers.  We have
    *   to try it and see.  The version does, however, indicate the age of
    *   the kernel.
    *
    *   Instead of matching the hardware checker's behavior of converting
    *   privileged commands to MI_NOOP, it makes execbuf2 start returning
    *   -EINVAL, making it dangerous to try and use privileged features.
    *
    *   Effective command parser versions:
    *   - Haswell:   0 (reporting 1, writes don't work)
    *   - Baytrail:  0 (reporting 1, writes don't work)
    *   - Ivybridge: 1 (enabled) or infinite (disabled)
    *
    * - v3.17:
    *   Baytrail aliasing PPGTT is enabled, making it like Ivybridge:
    *   effectively version 1 (enabled) or infinite (disabled).
    *
    * - v3.19: f1f55cc0556031c8ee3fe99dae7251e78b9b653b
    *   Command parser v2 supports predicate writes.
    *
    *   - Haswell:   0 (reporting 1, writes don't work)
    *   - Baytrail:  2 (enabled) or infinite (disabled)
    *   - Ivybridge: 2 (enabled) or infinite (disabled)
    *
    *   So version >= 2 is enough to know that Ivybridge and Baytrail
    *   will work.  Haswell still can't do anything.
    *
    * - v4.0: Version 3 happened.  Largely not relevant.
    *
    * - v4.1: 6702cf16e0ba8b0129f5aa1b6609d4e9c70bc13b
    *   L3 config registers are properly saved and restored as part
    *   of the hardware context.  We can approximately detect this point
    *   in time by checking if I915_PARAM_REVISION is recognized - it
    *   landed in a later commit, but in the same release cycle.
    *
    * - v4.2: 245054a1fe33c06ad233e0d58a27ec7b64db9284
    *   Command parser finally gains secure batch promotion.  On Haswell,
    *   the hardware checker gets disabled, which finally allows it to do
    *   privileged commands.
    *
    *   I915_PARAM_CMD_PARSER_VERSION reports 3.  Effective versions:
    *   - Haswell:   3 (enabled) or 0 (disabled)
    *   - Baytrail:  3 (enabled) or infinite (disabled)
    *   - Ivybridge: 3 (enabled) or infinite (disabled)
    *
    *   Unfortunately, detecting this point in time is tricky, because
    *   no version bump happened when this important change occurred.
    *   On Haswell, if we can write any register, then the kernel is at
    *   least this new, and we can start trusting the version number.
    *
    * - v4.4: 2bbe6bbb0dc94fd4ce287bdac9e1bd184e23057b and
    *   Command parser reaches version 4, allowing access to Haswell
    *   atomic scratch and chicken3 registers.  If version >= 4, we know
    *   the kernel is new enough to support privileged features on all
    *   hardware.  However, the user might have disabled it...and the
    *   kernel will still report version 4.  So we still have to guess
    *   and check.
    *
    * - v4.4: 7b9748cb513a6bef4af87b79f0da3ff7e8b56cd8
    *   Command parser v5 whitelists indirect compute shader dispatch
    *   registers, needed for OpenGL 4.3 and later.
    *
    * - v4.8:
    *   Command parser v7 lets us use MI_MATH on Haswell.
    *
    *   Additionally, the kernel begins reporting version 0 when
    *   the command parser is disabled, allowing us to skip the
    *   guess-and-check step on Haswell.  Unfortunately, this also
    *   means that we can no longer use it as an indicator of the
    *   age of the kernel.
    */
   if (intel_get_param(screen, I915_PARAM_CMD_PARSER_VERSION,
                       &screen->cmd_parser_version) < 0) {
      /* Command parser does not exist - getparam is unrecognized */
      screen->cmd_parser_version = 0;
   }

   /* Kernel 4.13 retuired for exec object capture */
   if (intel_get_boolean(screen, I915_PARAM_HAS_EXEC_CAPTURE)) {
      screen->kernel_features |= KERNEL_ALLOWS_EXEC_CAPTURE;
   }

   if (intel_get_boolean(screen, I915_PARAM_HAS_EXEC_BATCH_FIRST)) {
      screen->kernel_features |= KERNEL_ALLOWS_EXEC_BATCH_FIRST;
   }

   if (!intel_detect_pipelined_so(screen)) {
      /* We can't do anything, so the effective version is 0. */
      screen->cmd_parser_version = 0;
   } else {
      screen->kernel_features |= KERNEL_ALLOWS_SOL_OFFSET_WRITES;
   }

   if (devinfo->gen >= 8 || screen->cmd_parser_version >= 2)
      screen->kernel_features |= KERNEL_ALLOWS_PREDICATE_WRITES;

   /* Haswell requires command parser version 4 in order to have L3
    * atomic scratch1 and chicken3 bits
    */
   if (devinfo->is_haswell && screen->cmd_parser_version >= 4) {
      screen->kernel_features |=
         KERNEL_ALLOWS_HSW_SCRATCH1_AND_ROW_CHICKEN3;
   }

   /* Haswell requires command parser version 6 in order to write to the
    * MI_MATH GPR registers, and version 7 in order to use
    * MI_LOAD_REGISTER_REG (which all users of MI_MATH use).
    */
   if (devinfo->gen >= 8 ||
       (devinfo->is_haswell && screen->cmd_parser_version >= 7)) {
      screen->kernel_features |= KERNEL_ALLOWS_MI_MATH_AND_LRR;
   }

   /* Gen7 needs at least command parser version 5 to support compute */
   if (devinfo->gen >= 8 || screen->cmd_parser_version >= 5)
      screen->kernel_features |= KERNEL_ALLOWS_COMPUTE_DISPATCH;

   const char *force_msaa = getenv("INTEL_FORCE_MSAA");
   if (force_msaa) {
      screen->winsys_msaa_samples_override =
         intel_quantize_num_samples(screen, atoi(force_msaa));
      printf("Forcing winsys sample count to %d\n",
             screen->winsys_msaa_samples_override);
   } else {
      screen->winsys_msaa_samples_override = -1;
   }

   set_max_gl_versions(screen);

   /* Notification of GPU resets requires hardware contexts and a kernel new
    * enough to support DRM_IOCTL_I915_GET_RESET_STATS.  If the ioctl is
    * supported, calling it with a context of 0 will either generate EPERM or
    * no error.  If the ioctl is not supported, it always generate EINVAL.
    * Use this to determine whether to advertise the __DRI2_ROBUSTNESS
    * extension to the loader.
    *
    * Don't even try on pre-Gen6, since we don't attempt to use contexts there.
    */
   if (devinfo->gen >= 6) {
      struct drm_i915_reset_stats stats;
      memset(&stats, 0, sizeof(stats));

      const int ret = drmIoctl(dri_screen->fd, DRM_IOCTL_I915_GET_RESET_STATS, &stats);

      screen->has_context_reset_notification =
         (ret != -1 || errno != EINVAL);
   }

   dri_screen->extensions = !screen->has_context_reset_notification
      ? screenExtensions : intelRobustScreenExtensions;

   screen->compiler = brw_compiler_create(screen, devinfo);
   screen->compiler->shader_debug_log = shader_debug_log_mesa;
   screen->compiler->shader_perf_log = shader_perf_log_mesa;
   screen->compiler->constant_buffer_0_is_relative = true;
   screen->compiler->supports_pull_constants = true;

   screen->has_exec_fence =
     intel_get_boolean(screen, I915_PARAM_HAS_EXEC_FENCE);

   intel_screen_init_surface_formats(screen);

   if (INTEL_DEBUG & (DEBUG_BATCH | DEBUG_SUBMIT)) {
      unsigned int caps = intel_get_integer(screen, I915_PARAM_HAS_SCHEDULER);
      if (caps) {
         fprintf(stderr, "Kernel scheduler detected: %08x\n", caps);
         if (caps & I915_SCHEDULER_CAP_PRIORITY)
            fprintf(stderr, "  - User priority sorting enabled\n");
         if (caps & I915_SCHEDULER_CAP_PREEMPTION)
            fprintf(stderr, "  - Preemption enabled\n");
      }
   }

   return (const __DRIconfig**) intel_screen_make_configs(dri_screen);
}

struct intel_buffer {
   __DRIbuffer base;
   struct brw_bo *bo;
};

static __DRIbuffer *
intelAllocateBuffer(__DRIscreen *dri_screen,
		    unsigned attachment, unsigned format,
		    int width, int height)
{
   struct intel_buffer *intelBuffer;
   struct intel_screen *screen = dri_screen->driverPrivate;

   assert(attachment == __DRI_BUFFER_FRONT_LEFT ||
          attachment == __DRI_BUFFER_BACK_LEFT);

   intelBuffer = calloc(1, sizeof *intelBuffer);
   if (intelBuffer == NULL)
      return NULL;

   /* The front and back buffers are color buffers, which are X tiled. GEN9+
    * supports Y tiled and compressed buffers, but there is no way to plumb that
    * through to here. */
   uint32_t pitch;
   int cpp = format / 8;
   intelBuffer->bo = brw_bo_alloc_tiled_2d(screen->bufmgr,
                                           "intelAllocateBuffer",
                                           width,
                                           height,
                                           cpp,
                                           I915_TILING_X, &pitch,
                                           BO_ALLOC_BUSY);

   if (intelBuffer->bo == NULL) {
	   free(intelBuffer);
	   return NULL;
   }

   brw_bo_flink(intelBuffer->bo, &intelBuffer->base.name);

   intelBuffer->base.attachment = attachment;
   intelBuffer->base.cpp = cpp;
   intelBuffer->base.pitch = pitch;

   return &intelBuffer->base;
}

static void
intelReleaseBuffer(__DRIscreen *dri_screen, __DRIbuffer *buffer)
{
   struct intel_buffer *intelBuffer = (struct intel_buffer *) buffer;

   brw_bo_unreference(intelBuffer->bo);
   free(intelBuffer);
}

static const struct __DriverAPIRec brw_driver_api = {
   .InitScreen		 = intelInitScreen2,
   .DestroyScreen	 = intelDestroyScreen,
   .CreateContext	 = brwCreateContext,
   .DestroyContext	 = intelDestroyContext,
   .CreateBuffer	 = intelCreateBuffer,
   .DestroyBuffer	 = intelDestroyBuffer,
   .MakeCurrent		 = intelMakeCurrent,
   .UnbindContext	 = intelUnbindContext,
   .AllocateBuffer       = intelAllocateBuffer,
   .ReleaseBuffer        = intelReleaseBuffer
};

static const struct __DRIDriverVtableExtensionRec brw_vtable = {
   .base = { __DRI_DRIVER_VTABLE, 1 },
   .vtable = &brw_driver_api,
};

static const __DRIextension *brw_driver_extensions[] = {
    &driCoreExtension.base,
    &driImageDriverExtension.base,
    &driDRI2Extension.base,
    &brw_vtable.base,
    &brw_config_options.base,
    NULL
};

PUBLIC const __DRIextension **__driDriverGetExtensions_i965(void)
{
   globalDriverAPI = &brw_driver_api;

   return brw_driver_extensions;
}
