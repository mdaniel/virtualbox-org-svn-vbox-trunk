/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#include "util/u_inlines.h"
#include "pipe/p_defines.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_bitmask.h"

#include "svga_context.h"
#include "svga_hw_reg.h"
#include "svga_cmd.h"


static inline unsigned
svga_translate_blend_factor(const struct svga_context *svga, unsigned factor)
{
   /* Note: there is no SVGA3D_BLENDOP_[INV]BLENDFACTORALPHA so
    * we can't translate PIPE_BLENDFACTOR_[INV_]CONST_ALPHA properly.
    */
   switch (factor) {
   case PIPE_BLENDFACTOR_ZERO:            return SVGA3D_BLENDOP_ZERO;
   case PIPE_BLENDFACTOR_SRC_ALPHA:       return SVGA3D_BLENDOP_SRCALPHA;
   case PIPE_BLENDFACTOR_ONE:             return SVGA3D_BLENDOP_ONE;
   case PIPE_BLENDFACTOR_SRC_COLOR:       return SVGA3D_BLENDOP_SRCCOLOR;
   case PIPE_BLENDFACTOR_INV_SRC_COLOR:   return SVGA3D_BLENDOP_INVSRCCOLOR;
   case PIPE_BLENDFACTOR_DST_COLOR:       return SVGA3D_BLENDOP_DESTCOLOR;
   case PIPE_BLENDFACTOR_INV_DST_COLOR:   return SVGA3D_BLENDOP_INVDESTCOLOR;
   case PIPE_BLENDFACTOR_INV_SRC_ALPHA:   return SVGA3D_BLENDOP_INVSRCALPHA;
   case PIPE_BLENDFACTOR_DST_ALPHA:       return SVGA3D_BLENDOP_DESTALPHA;
   case PIPE_BLENDFACTOR_INV_DST_ALPHA:   return SVGA3D_BLENDOP_INVDESTALPHA;
   case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE: return SVGA3D_BLENDOP_SRCALPHASAT;
   case PIPE_BLENDFACTOR_CONST_COLOR:     return SVGA3D_BLENDOP_BLENDFACTOR;
   case PIPE_BLENDFACTOR_INV_CONST_COLOR: return SVGA3D_BLENDOP_INVBLENDFACTOR;
   case PIPE_BLENDFACTOR_CONST_ALPHA:
      if (svga_have_vgpu10(svga))
         return SVGA3D_BLENDOP_BLENDFACTORALPHA;
      else
         return SVGA3D_BLENDOP_BLENDFACTOR; /* as close as we can get */
   case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
      if (svga_have_vgpu10(svga))
         return SVGA3D_BLENDOP_INVBLENDFACTORALPHA;
      else
         return SVGA3D_BLENDOP_INVBLENDFACTOR; /* as close as we can get */
   case PIPE_BLENDFACTOR_SRC1_COLOR:      return SVGA3D_BLENDOP_SRC1COLOR;
   case PIPE_BLENDFACTOR_INV_SRC1_COLOR:  return SVGA3D_BLENDOP_INVSRC1COLOR;
   case PIPE_BLENDFACTOR_SRC1_ALPHA:      return SVGA3D_BLENDOP_SRC1ALPHA;
   case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:  return SVGA3D_BLENDOP_INVSRC1ALPHA;
   case 0:                                return SVGA3D_BLENDOP_ONE;
   default:
      assert(0);
      return SVGA3D_BLENDOP_ZERO;
   }
}

static inline unsigned
svga_translate_blend_func(unsigned mode)
{
   switch (mode) {
   case PIPE_BLEND_ADD:              return SVGA3D_BLENDEQ_ADD;
   case PIPE_BLEND_SUBTRACT:         return SVGA3D_BLENDEQ_SUBTRACT;
   case PIPE_BLEND_REVERSE_SUBTRACT: return SVGA3D_BLENDEQ_REVSUBTRACT;
   case PIPE_BLEND_MIN:              return SVGA3D_BLENDEQ_MINIMUM;
   case PIPE_BLEND_MAX:              return SVGA3D_BLENDEQ_MAXIMUM;
   default:
      assert(0);
      return SVGA3D_BLENDEQ_ADD;
   }
}


/**
 * Define a vgpu10 blend state object for the given
 * svga blend state.
 */
static void
define_blend_state_object(struct svga_context *svga,
                          struct svga_blend_state *bs)
{
   SVGA3dDXBlendStatePerRT perRT[SVGA3D_MAX_RENDER_TARGETS];
   unsigned try;
   int i;

   assert(svga_have_vgpu10(svga));

   bs->id = util_bitmask_add(svga->blend_object_id_bm);

   for (i = 0; i < SVGA3D_DX_MAX_RENDER_TARGETS; i++) {
      perRT[i].blendEnable = bs->rt[i].blend_enable;
      perRT[i].srcBlend = bs->rt[i].srcblend;
      perRT[i].destBlend = bs->rt[i].dstblend;
      perRT[i].blendOp = bs->rt[i].blendeq;
      perRT[i].srcBlendAlpha = bs->rt[i].srcblend_alpha;
      perRT[i].destBlendAlpha = bs->rt[i].dstblend_alpha;
      perRT[i].blendOpAlpha = bs->rt[i].blendeq_alpha;
      perRT[i].renderTargetWriteMask = bs->rt[i].writemask;
      perRT[i].logicOpEnable = 0;
      perRT[i].logicOp = SVGA3D_LOGICOP_COPY;
      assert(perRT[i].srcBlend == perRT[0].srcBlend);
   }

   /* Loop in case command buffer is full and we need to flush and retry */
   for (try = 0; try < 2; try++) {
      enum pipe_error ret;

      ret = SVGA3D_vgpu10_DefineBlendState(svga->swc,
                                           bs->id,
                                           bs->alpha_to_coverage,
                                           bs->independent_blend_enable,
                                           perRT);
      if (ret == PIPE_OK)
         return;
      svga_context_flush(svga, NULL);
   }
}


static void *
svga_create_blend_state(struct pipe_context *pipe,
                        const struct pipe_blend_state *templ)
{
   struct svga_context *svga = svga_context(pipe);
   struct svga_blend_state *blend = CALLOC_STRUCT( svga_blend_state );
   unsigned i;

   if (!blend)
      return NULL;

   /* Fill in the per-rendertarget blend state.  We currently only
    * support independent blend enable and colormask per render target.
    */
   for (i = 0; i < PIPE_MAX_COLOR_BUFS; i++) {
      /* No way to set this in SVGA3D, and no way to correctly implement it on
       * top of D3D9 API.  Instead we try to simulate with various blend modes.
       */
      if (templ->logicop_enable) {
         switch (templ->logicop_func) {
         case PIPE_LOGICOP_XOR:
         case PIPE_LOGICOP_INVERT:
            blend->need_white_fragments = TRUE;
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_SUBTRACT;
            break;
         case PIPE_LOGICOP_CLEAR:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MINIMUM;
            break;
         case PIPE_LOGICOP_COPY:
            blend->rt[i].blend_enable = FALSE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_ADD;
            break;
         case PIPE_LOGICOP_COPY_INVERTED:
            blend->rt[i].blend_enable   = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_INVSRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_ADD;
            break;
         case PIPE_LOGICOP_NOOP:
            blend->rt[i].blend_enable   = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_DESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_ADD;
            break;
         case PIPE_LOGICOP_SET:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MAXIMUM;
            break;
         case PIPE_LOGICOP_AND:
            /* Approximate with minimum - works for the 0 & anything case: */
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_SRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_DESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MINIMUM;
            break;
         case PIPE_LOGICOP_AND_REVERSE:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_SRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_INVDESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MINIMUM;
            break;
         case PIPE_LOGICOP_AND_INVERTED:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_INVSRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_DESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MINIMUM;
            break;
         case PIPE_LOGICOP_OR:
            /* Approximate with maximum - works for the 1 | anything case: */
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_SRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_DESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MAXIMUM;
            break;
         case PIPE_LOGICOP_OR_REVERSE:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_SRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_INVDESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MAXIMUM;
            break;
         case PIPE_LOGICOP_OR_INVERTED:
            blend->rt[i].blend_enable = TRUE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_INVSRCCOLOR;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_DESTCOLOR;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_MAXIMUM;
            break;
         case PIPE_LOGICOP_NAND:
         case PIPE_LOGICOP_NOR:
         case PIPE_LOGICOP_EQUIV:
            /* Fill these in with plausible values */
            blend->rt[i].blend_enable = FALSE;
            blend->rt[i].srcblend       = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend       = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq        = SVGA3D_BLENDEQ_ADD;
            break;
         default:
            assert(0);
            break;
         }
         blend->rt[i].srcblend_alpha = blend->rt[i].srcblend;
         blend->rt[i].dstblend_alpha = blend->rt[i].dstblend;
         blend->rt[i].blendeq_alpha = blend->rt[i].blendeq;

         if (templ->logicop_func == PIPE_LOGICOP_XOR) {
            pipe_debug_message(&svga->debug.callback, CONFORMANCE,
                               "XOR logicop mode has limited support");
         }
         else if (templ->logicop_func != PIPE_LOGICOP_COPY) {
            pipe_debug_message(&svga->debug.callback, CONFORMANCE,
                               "general logicops are not supported");
         }
      }
      else {
         /* Note: the vgpu10 device does not yet support independent
          * blend terms per render target.  Target[0] always specifies the
          * blending terms.
          */
         if (templ->independent_blend_enable || templ->rt[0].blend_enable) {
            /* always use the 0th target's blending terms for now */
            blend->rt[i].srcblend =
               svga_translate_blend_factor(svga, templ->rt[0].rgb_src_factor);
            blend->rt[i].dstblend =
               svga_translate_blend_factor(svga, templ->rt[0].rgb_dst_factor);
            blend->rt[i].blendeq =
               svga_translate_blend_func(templ->rt[0].rgb_func);
            blend->rt[i].srcblend_alpha =
               svga_translate_blend_factor(svga, templ->rt[0].alpha_src_factor);
            blend->rt[i].dstblend_alpha =
               svga_translate_blend_factor(svga, templ->rt[0].alpha_dst_factor);
            blend->rt[i].blendeq_alpha =
               svga_translate_blend_func(templ->rt[0].alpha_func);

            if (blend->rt[i].srcblend_alpha != blend->rt[i].srcblend ||
                blend->rt[i].dstblend_alpha != blend->rt[i].dstblend ||
                blend->rt[i].blendeq_alpha  != blend->rt[i].blendeq) {
               blend->rt[i].separate_alpha_blend_enable = TRUE;
            }
         }
         else {
            /* disabled - default blend terms */
            blend->rt[i].srcblend = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq = SVGA3D_BLENDEQ_ADD;
            blend->rt[i].srcblend_alpha = SVGA3D_BLENDOP_ONE;
            blend->rt[i].dstblend_alpha = SVGA3D_BLENDOP_ZERO;
            blend->rt[i].blendeq_alpha = SVGA3D_BLENDEQ_ADD;
         }

         if (templ->independent_blend_enable) {
            blend->rt[i].blend_enable = templ->rt[i].blend_enable;
         }
         else {
            blend->rt[i].blend_enable = templ->rt[0].blend_enable;
         }
      }

      /* Some GL blend modes are not supported by the VGPU9 device (there's
       * no equivalent of PIPE_BLENDFACTOR_[INV_]CONST_ALPHA).
       * When we set this flag, we copy the constant blend alpha value
       * to the R, G, B components.
       * This works as long as the src/dst RGB blend factors doesn't use
       * PIPE_BLENDFACTOR_CONST_COLOR and PIPE_BLENDFACTOR_CONST_ALPHA
       * at the same time.  There's no work-around for that.
       */
      if (!svga_have_vgpu10(svga)) {
         if (templ->rt[0].rgb_src_factor == PIPE_BLENDFACTOR_CONST_ALPHA ||
             templ->rt[0].rgb_dst_factor == PIPE_BLENDFACTOR_CONST_ALPHA ||
             templ->rt[0].rgb_src_factor == PIPE_BLENDFACTOR_INV_CONST_ALPHA ||
             templ->rt[0].rgb_dst_factor == PIPE_BLENDFACTOR_INV_CONST_ALPHA) {
            blend->blend_color_alpha = TRUE;
         }
      }

      if (templ->independent_blend_enable) {
         blend->rt[i].writemask = templ->rt[i].colormask;
      }
      else {
         blend->rt[i].writemask = templ->rt[0].colormask;
      }
   }

   blend->independent_blend_enable = templ->independent_blend_enable;

   blend->alpha_to_coverage = templ->alpha_to_coverage;
   blend->alpha_to_one = templ->alpha_to_one;

   if (svga_have_vgpu10(svga)) {
      define_blend_state_object(svga, blend);
   }

   svga->hud.num_blend_objects++;
   SVGA_STATS_COUNT_INC(svga_screen(svga->pipe.screen)->sws,
                        SVGA_STATS_COUNT_BLENDSTATE);

   return blend;
}


static void svga_bind_blend_state(struct pipe_context *pipe,
                                  void *blend)
{
   struct svga_context *svga = svga_context(pipe);

   svga->curr.blend = (struct svga_blend_state*)blend;
   svga->dirty |= SVGA_NEW_BLEND;
}

static void svga_delete_blend_state(struct pipe_context *pipe,
                                    void *blend)
{
   struct svga_context *svga = svga_context(pipe);
   struct svga_blend_state *bs =
      (struct svga_blend_state *) blend;

   if (bs->id != SVGA3D_INVALID_ID) {
      enum pipe_error ret;

      ret = SVGA3D_vgpu10_DestroyBlendState(svga->swc, bs->id);
      if (ret != PIPE_OK) {
         svga_context_flush(svga, NULL);
         ret = SVGA3D_vgpu10_DestroyBlendState(svga->swc, bs->id);
         assert(ret == PIPE_OK);
      }

      if (bs->id == svga->state.hw_draw.blend_id)
         svga->state.hw_draw.blend_id = SVGA3D_INVALID_ID;

      util_bitmask_clear(svga->blend_object_id_bm, bs->id);
      bs->id = SVGA3D_INVALID_ID;
   }

   FREE(blend);
   svga->hud.num_blend_objects--;
}

static void svga_set_blend_color( struct pipe_context *pipe,
                                  const struct pipe_blend_color *blend_color )
{
   struct svga_context *svga = svga_context(pipe);

   svga->curr.blend_color = *blend_color;

   svga->dirty |= SVGA_NEW_BLEND_COLOR;
}


void svga_init_blend_functions( struct svga_context *svga )
{
   svga->pipe.create_blend_state = svga_create_blend_state;
   svga->pipe.bind_blend_state = svga_bind_blend_state;
   svga->pipe.delete_blend_state = svga_delete_blend_state;

   svga->pipe.set_blend_color = svga_set_blend_color;
}
