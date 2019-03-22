/*
 * Copyright 2016 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pipe/p_context.h"
#include "util/u_format.h"
#include "util/u_inlines.h"

#include "main/context.h"
#include "main/macros.h"
#include "main/mtypes.h"
#include "main/teximage.h"
#include "main/texobj.h"
#include "program/prog_instruction.h"

#include "st_context.h"
#include "st_sampler_view.h"
#include "st_texture.h"
#include "st_format.h"
#include "st_cb_bufferobjects.h"
#include "st_cb_texture.h"


/**
 * Try to find a matching sampler view for the given context.
 * If none is found an empty slot is initialized with a
 * template and returned instead.
 */
static struct st_sampler_view *
st_texture_get_sampler_view(struct st_context *st,
                            struct st_texture_object *stObj)
{
   struct st_sampler_view *free = NULL;
   GLuint i;

   for (i = 0; i < stObj->num_sampler_views; ++i) {
      struct st_sampler_view *sv = &stObj->sampler_views[i];
      /* Is the array entry used ? */
      if (sv->view) {
         /* check if the context matches */
         if (sv->view->context == st->pipe) {
            return sv;
         }
      } else {
         /* Found a free slot, remember that */
         free = sv;
      }
   }

   /* Couldn't find a slot for our context, create a new one */

   if (!free) {
      /* Haven't even found a free one, resize the array */
      unsigned new_size = (stObj->num_sampler_views + 1) *
         sizeof(struct st_sampler_view);
      stObj->sampler_views = realloc(stObj->sampler_views, new_size);
      free = &stObj->sampler_views[stObj->num_sampler_views++];
      free->view = NULL;
   }

   assert(free->view == NULL);

   return free;
}


/**
 * For the given texture object, release any sampler views which belong
 * to the calling context.
 */
void
st_texture_release_sampler_view(struct st_context *st,
                                struct st_texture_object *stObj)
{
   GLuint i;

   for (i = 0; i < stObj->num_sampler_views; ++i) {
      struct pipe_sampler_view **sv = &stObj->sampler_views[i].view;

      if (*sv && (*sv)->context == st->pipe) {
         pipe_sampler_view_reference(sv, NULL);
         break;
      }
   }
}


/**
 * Release all sampler views attached to the given texture object, regardless
 * of the context.
 */
void
st_texture_release_all_sampler_views(struct st_context *st,
                                     struct st_texture_object *stObj)
{
   GLuint i;

   /* XXX This should use sampler_views[i]->pipe, not st->pipe */
   for (i = 0; i < stObj->num_sampler_views; ++i)
      pipe_sampler_view_release(st->pipe, &stObj->sampler_views[i].view);
}


void
st_texture_free_sampler_views(struct st_texture_object *stObj)
{
   free(stObj->sampler_views);
   stObj->sampler_views = NULL;
   stObj->num_sampler_views = 0;
}


/**
 * Return swizzle1(swizzle2)
 */
static unsigned
swizzle_swizzle(unsigned swizzle1, unsigned swizzle2)
{
   unsigned i, swz[4];

   if (swizzle1 == SWIZZLE_XYZW) {
      /* identity swizzle, no change to swizzle2 */
      return swizzle2;
   }

   for (i = 0; i < 4; i++) {
      unsigned s = GET_SWZ(swizzle1, i);
      switch (s) {
      case SWIZZLE_X:
      case SWIZZLE_Y:
      case SWIZZLE_Z:
      case SWIZZLE_W:
         swz[i] = GET_SWZ(swizzle2, s);
         break;
      case SWIZZLE_ZERO:
         swz[i] = SWIZZLE_ZERO;
         break;
      case SWIZZLE_ONE:
         swz[i] = SWIZZLE_ONE;
         break;
      default:
         assert(!"Bad swizzle term");
         swz[i] = SWIZZLE_X;
      }
   }

   return MAKE_SWIZZLE4(swz[0], swz[1], swz[2], swz[3]);
}


/**
 * Given a user-specified texture base format, the actual gallium texture
 * format and the current GL_DEPTH_MODE, return a texture swizzle.
 *
 * Consider the case where the user requests a GL_RGB internal texture
 * format the driver actually uses an RGBA format.  The A component should
 * be ignored and sampling from the texture should always return (r,g,b,1).
 * But if we rendered to the texture we might have written A values != 1.
 * By sampling the texture with a ".xyz1" swizzle we'll get the expected A=1.
 * This function computes the texture swizzle needed to get the expected
 * values.
 *
 * In the case of depth textures, the GL_DEPTH_MODE state determines the
 * texture swizzle.
 *
 * This result must be composed with the user-specified swizzle to get
 * the final swizzle.
 */
static unsigned
compute_texture_format_swizzle(GLenum baseFormat, GLenum depthMode,
                               bool glsl130_or_later)
{
   switch (baseFormat) {
   case GL_RGBA:
      return SWIZZLE_XYZW;
   case GL_RGB:
      return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_ONE);
   case GL_RG:
      return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_Y, SWIZZLE_ZERO, SWIZZLE_ONE);
   case GL_RED:
      return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_ZERO,
                           SWIZZLE_ZERO, SWIZZLE_ONE);
   case GL_ALPHA:
      return MAKE_SWIZZLE4(SWIZZLE_ZERO, SWIZZLE_ZERO,
                           SWIZZLE_ZERO, SWIZZLE_W);
   case GL_LUMINANCE:
      return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_ONE);
   case GL_LUMINANCE_ALPHA:
      return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_W);
   case GL_INTENSITY:
      return SWIZZLE_XXXX;
   case GL_STENCIL_INDEX:
   case GL_DEPTH_STENCIL:
   case GL_DEPTH_COMPONENT:
      /* Now examine the depth mode */
      switch (depthMode) {
      case GL_LUMINANCE:
         return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_ONE);
      case GL_INTENSITY:
         return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X);
      case GL_ALPHA:
         /* The texture(sampler*Shadow) functions from GLSL 1.30 ignore
          * the depth mode and return float, while older shadow* functions
          * and ARB_fp instructions return vec4 according to the depth mode.
          *
          * The problem with the GLSL 1.30 functions is that GL_ALPHA forces
          * them to return 0, breaking them completely.
          *
          * A proper fix would increase code complexity and that's not worth
          * it for a rarely used feature such as the GL_ALPHA depth mode
          * in GL3. Therefore, change GL_ALPHA to GL_INTENSITY for all
          * shaders that use GLSL 1.30 or later.
          *
          * BTW, it's required that sampler views are updated when
          * shaders change (check_sampler_swizzle takes care of that).
          */
         if (glsl130_or_later)
            return SWIZZLE_XXXX;
         else
            return MAKE_SWIZZLE4(SWIZZLE_ZERO, SWIZZLE_ZERO,
                                 SWIZZLE_ZERO, SWIZZLE_X);
      case GL_RED:
         return MAKE_SWIZZLE4(SWIZZLE_X, SWIZZLE_ZERO,
                              SWIZZLE_ZERO, SWIZZLE_ONE);
      default:
         assert(!"Unexpected depthMode");
         return SWIZZLE_XYZW;
      }
   default:
      assert(!"Unexpected baseFormat");
      return SWIZZLE_XYZW;
   }
}


static unsigned
get_texture_format_swizzle(const struct st_context *st,
                           const struct st_texture_object *stObj,
                           bool glsl130_or_later)
{
   GLenum baseFormat = _mesa_base_tex_image(&stObj->base)->_BaseFormat;
   unsigned tex_swizzle;
   GLenum depth_mode = stObj->base.DepthMode;

   /* In ES 3.0, DEPTH_TEXTURE_MODE is expected to be GL_RED for textures
    * with depth component data specified with a sized internal format.
    */
   if (_mesa_is_gles3(st->ctx) &&
       (baseFormat == GL_DEPTH_COMPONENT ||
        baseFormat == GL_DEPTH_STENCIL ||
        baseFormat == GL_STENCIL_INDEX)) {
      const struct gl_texture_image *firstImage =
         _mesa_base_tex_image(&stObj->base);
      if (firstImage->InternalFormat != GL_DEPTH_COMPONENT &&
          firstImage->InternalFormat != GL_DEPTH_STENCIL &&
          firstImage->InternalFormat != GL_STENCIL_INDEX)
         depth_mode = GL_RED;
   }
   tex_swizzle = compute_texture_format_swizzle(baseFormat,
                                                depth_mode,
                                                glsl130_or_later);

   /* Combine the texture format swizzle with user's swizzle */
   return swizzle_swizzle(stObj->base._Swizzle, tex_swizzle);
}


/**
 * Return TRUE if the texture's sampler view swizzle is not equal to
 * the texture's swizzle.
 *
 * \param stObj  the st texture object,
 */
MAYBE_UNUSED static boolean
check_sampler_swizzle(const struct st_context *st,
                      const struct st_texture_object *stObj,
                      const struct pipe_sampler_view *sv,
                      bool glsl130_or_later)
{
   unsigned swizzle = get_texture_format_swizzle(st, stObj, glsl130_or_later);

   return ((sv->swizzle_r != GET_SWZ(swizzle, 0)) ||
           (sv->swizzle_g != GET_SWZ(swizzle, 1)) ||
           (sv->swizzle_b != GET_SWZ(swizzle, 2)) ||
           (sv->swizzle_a != GET_SWZ(swizzle, 3)));
}


static unsigned
last_level(const struct st_texture_object *stObj)
{
   unsigned ret = MIN2(stObj->base.MinLevel + stObj->base._MaxLevel,
                       stObj->pt->last_level);
   if (stObj->base.Immutable)
      ret = MIN2(ret, stObj->base.MinLevel + stObj->base.NumLevels - 1);
   return ret;
}


static unsigned
last_layer(const struct st_texture_object *stObj)
{
   if (stObj->base.Immutable && stObj->pt->array_size > 1)
      return MIN2(stObj->base.MinLayer + stObj->base.NumLayers - 1,
                  stObj->pt->array_size - 1);
   return stObj->pt->array_size - 1;
}


/**
 * Determine the format for the texture sampler view.
 */
static enum pipe_format
get_sampler_view_format(struct st_context *st,
                        const struct st_texture_object *stObj,
                        bool srgb_skip_decode)
{
   enum pipe_format format;

   GLenum baseFormat = _mesa_base_tex_image(&stObj->base)->_BaseFormat;
   format = stObj->surface_based ? stObj->surface_format : stObj->pt->format;

   if (baseFormat == GL_DEPTH_COMPONENT ||
       baseFormat == GL_DEPTH_STENCIL ||
       baseFormat == GL_STENCIL_INDEX) {
      if (stObj->base.StencilSampling || baseFormat == GL_STENCIL_INDEX)
         format = util_format_stencil_only(format);

      return format;
   }

   /* If sRGB decoding is off, use the linear format */
   if (srgb_skip_decode)
      format = util_format_linear(format);

   /* Use R8_UNORM for video formats */
   switch (format) {
   case PIPE_FORMAT_NV12:
   case PIPE_FORMAT_IYUV:
      format = PIPE_FORMAT_R8_UNORM;
      break;
   default:
      break;
   }
   return format;
}


static struct pipe_sampler_view *
st_create_texture_sampler_view_from_stobj(struct st_context *st,
					  struct st_texture_object *stObj,
					  enum pipe_format format,
                                          bool glsl130_or_later)
{
   /* There is no need to clear this structure (consider CPU overhead). */
   struct pipe_sampler_view templ;
   unsigned swizzle = get_texture_format_swizzle(st, stObj, glsl130_or_later);

   templ.format = format;

   if (stObj->level_override) {
      templ.u.tex.first_level = templ.u.tex.last_level = stObj->level_override;
   } else {
      templ.u.tex.first_level = stObj->base.MinLevel + stObj->base.BaseLevel;
      templ.u.tex.last_level = last_level(stObj);
   }
   if (stObj->layer_override) {
      templ.u.tex.first_layer = templ.u.tex.last_layer = stObj->layer_override;
   } else {
      templ.u.tex.first_layer = stObj->base.MinLayer;
      templ.u.tex.last_layer = last_layer(stObj);
   }
   assert(templ.u.tex.first_layer <= templ.u.tex.last_layer);
   assert(templ.u.tex.first_level <= templ.u.tex.last_level);
   templ.target = gl_target_to_pipe(stObj->base.Target);

   templ.swizzle_r = GET_SWZ(swizzle, 0);
   templ.swizzle_g = GET_SWZ(swizzle, 1);
   templ.swizzle_b = GET_SWZ(swizzle, 2);
   templ.swizzle_a = GET_SWZ(swizzle, 3);

   return st->pipe->create_sampler_view(st->pipe, stObj->pt, &templ);
}


struct pipe_sampler_view *
st_get_texture_sampler_view_from_stobj(struct st_context *st,
                                       struct st_texture_object *stObj,
                                       const struct gl_sampler_object *samp,
                                       bool glsl130_or_later,
                                       bool ignore_srgb_decode)
{
   struct st_sampler_view *sv;
   struct pipe_sampler_view *view;
   bool srgb_skip_decode = false;

   sv = st_texture_get_sampler_view(st, stObj);
   view = sv->view;

   if (!ignore_srgb_decode && samp->sRGBDecode == GL_SKIP_DECODE_EXT)
      srgb_skip_decode = true;

   if (view &&
       sv->glsl130_or_later == glsl130_or_later &&
       sv->srgb_skip_decode == srgb_skip_decode) {
      /* Debug check: make sure that the sampler view's parameters are
       * what they're supposed to be.
       */
      MAYBE_UNUSED struct pipe_sampler_view *view = sv->view;
      assert(stObj->pt == view->texture);
      assert(!check_sampler_swizzle(st, stObj, view, glsl130_or_later));
      assert(get_sampler_view_format(st, stObj, srgb_skip_decode) == view->format);
      assert(gl_target_to_pipe(stObj->base.Target) == view->target);
      assert(stObj->level_override ||
             stObj->base.MinLevel + stObj->base.BaseLevel == view->u.tex.first_level);
      assert(stObj->level_override || last_level(stObj) == view->u.tex.last_level);
      assert(stObj->layer_override || stObj->base.MinLayer == view->u.tex.first_layer);
      assert(stObj->layer_override || last_layer(stObj) == view->u.tex.last_layer);
      assert(!stObj->layer_override ||
             (stObj->layer_override == view->u.tex.first_layer &&
              stObj->layer_override == view->u.tex.last_layer));
   }
   else {
      /* create new sampler view */
      enum pipe_format format = get_sampler_view_format(st, stObj, srgb_skip_decode);

      sv->glsl130_or_later = glsl130_or_later;
      sv->srgb_skip_decode = srgb_skip_decode;

      pipe_sampler_view_release(st->pipe, &sv->view);
      view = sv->view =
         st_create_texture_sampler_view_from_stobj(st, stObj, format, glsl130_or_later);
   }

   return view;
}


struct pipe_sampler_view *
st_get_buffer_sampler_view_from_stobj(struct st_context *st,
                                      struct st_texture_object *stObj)
{
   struct st_sampler_view *sv;
   struct st_buffer_object *stBuf =
      st_buffer_object(stObj->base.BufferObject);

   if (!stBuf || !stBuf->buffer)
      return NULL;

   sv = st_texture_get_sampler_view(st, stObj);

   struct pipe_resource *buf = stBuf->buffer;
   struct pipe_sampler_view *view = sv->view;

   if (view && view->texture == buf) {
      /* Debug check: make sure that the sampler view's parameters are
       * what they're supposed to be.
       */
      assert(st_mesa_format_to_pipe_format(st, stObj->base._BufferObjectFormat)
             == view->format);
      assert(view->target == PIPE_BUFFER);
      unsigned base = stObj->base.BufferOffset;
      MAYBE_UNUSED unsigned size = MIN2(buf->width0 - base,
                           (unsigned) stObj->base.BufferSize);
      assert(view->u.buf.offset == base);
      assert(view->u.buf.size == size);
   } else {
      unsigned base = stObj->base.BufferOffset;

      if (base >= buf->width0)
         return NULL;

      unsigned size = buf->width0 - base;
      size = MIN2(size, (unsigned)stObj->base.BufferSize);
      if (!size)
         return NULL;

      /* Create a new sampler view. There is no need to clear the entire
       * structure (consider CPU overhead).
       */
      struct pipe_sampler_view templ;

      templ.format =
         st_mesa_format_to_pipe_format(st, stObj->base._BufferObjectFormat);
      templ.target = PIPE_BUFFER;
      templ.swizzle_r = PIPE_SWIZZLE_X;
      templ.swizzle_g = PIPE_SWIZZLE_Y;
      templ.swizzle_b = PIPE_SWIZZLE_Z;
      templ.swizzle_a = PIPE_SWIZZLE_W;
      templ.u.buf.offset = base;
      templ.u.buf.size = size;

      pipe_sampler_view_release(st->pipe, &sv->view);
      view = sv->view = st->pipe->create_sampler_view(st->pipe, buf, &templ);
   }
   return view;
}
