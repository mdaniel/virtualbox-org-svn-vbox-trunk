/**************************************************************************
 * 
 * Copyright 2003 VMware, Inc.
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
 * 
 **************************************************************************/

#include "util/u_math.h"
#include "util/u_memory.h"
#include "pipe/p_shader_tokens.h"
#include "draw/draw_context.h"
#include "draw/draw_vertex.h"
#include "draw/draw_private.h"
#include "lp_context.h"
#include "lp_screen.h"
#include "lp_setup.h"
#include "lp_state.h"



/**
 * The vertex info describes how to convert the post-transformed vertices
 * (simple float[][4]) used by the 'draw' module into vertices for
 * rasterization.
 *
 * This function validates the vertex layout.
 */
static void
compute_vertex_info(struct llvmpipe_context *llvmpipe)
{
   const struct tgsi_shader_info *fsInfo = &llvmpipe->fs->info.base;
   struct vertex_info *vinfo = &llvmpipe->vertex_info;
   int vs_index;
   uint i;

   draw_prepare_shader_outputs(llvmpipe->draw);

   /*
    * Those can't actually be 0 (because pos is always at 0).
    * But use ints anyway to avoid confusion (in vs outputs, they
    * can very well be at pos 0).
    */
   llvmpipe->color_slot[0] = -1;
   llvmpipe->color_slot[1] = -1;
   llvmpipe->bcolor_slot[0] = -1;
   llvmpipe->bcolor_slot[1] = -1;
   llvmpipe->viewport_index_slot = -1;
   llvmpipe->layer_slot = -1;
   llvmpipe->face_slot = -1;
   llvmpipe->psize_slot = -1;

   /*
    * Match FS inputs against VS outputs, emitting the necessary
    * attributes.  Could cache these structs and look them up with a
    * combination of fragment shader, vertex shader ids.
    */

   vinfo->num_attribs = 0;

   vs_index = draw_find_shader_output(llvmpipe->draw,
                                      TGSI_SEMANTIC_POSITION, 0);

   draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);

   for (i = 0; i < fsInfo->num_inputs; i++) {
      /*
       * Search for each input in current vs output:
       */
      vs_index = draw_find_shader_output(llvmpipe->draw,
                                         fsInfo->input_semantic_name[i],
                                         fsInfo->input_semantic_index[i]);

      if (fsInfo->input_semantic_name[i] == TGSI_SEMANTIC_COLOR &&
          fsInfo->input_semantic_index[i] < 2) {
         int idx = fsInfo->input_semantic_index[i];
         llvmpipe->color_slot[idx] = (int)vinfo->num_attribs;
      }

      if (fsInfo->input_semantic_name[i] == TGSI_SEMANTIC_FACE) {
         llvmpipe->face_slot = (int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      /*
       * For vp index and layer, if the fs requires them but the vs doesn't
       * provide them, draw (vbuf) will give us the required 0 (slot -1).
       * (This means in this case we'll also use those slots in setup, which
       * isn't necessary but they'll contain the correct (0) value.)
       */
      } else if (fsInfo->input_semantic_name[i] ==
                 TGSI_SEMANTIC_VIEWPORT_INDEX) {
         llvmpipe->viewport_index_slot = (int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      } else if (fsInfo->input_semantic_name[i] == TGSI_SEMANTIC_LAYER) {
         llvmpipe->layer_slot = (int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      } else {
         /*
          * Note that we'd actually want to skip position (as we won't use
          * the attribute in the fs) but can't. The reason is that we don't
          * actually have an input/output map for setup (even though it looks
          * like we do...). Could adjust for this though even without a map
          * (in llvmpipe_create_fs_state()).
          */
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      }
   }

   /* Figure out if we need bcolor as well.
    */
   for (i = 0; i < 2; i++) {
      vs_index = draw_find_shader_output(llvmpipe->draw,
                                         TGSI_SEMANTIC_BCOLOR, i);

      if (vs_index >= 0) {
         llvmpipe->bcolor_slot[i] = (int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      }
   }

   /* Figure out if we need pointsize as well.
    */
   vs_index = draw_find_shader_output(llvmpipe->draw,
                                      TGSI_SEMANTIC_PSIZE, 0);

   if (vs_index >= 0) {
      llvmpipe->psize_slot = (int)vinfo->num_attribs;
      draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
   }

   /* Figure out if we need viewport index (if it wasn't already in fs input) */
   if (llvmpipe->viewport_index_slot < 0) {
      vs_index = draw_find_shader_output(llvmpipe->draw,
                                         TGSI_SEMANTIC_VIEWPORT_INDEX,
                                         0);
      if (vs_index >= 0) {
         llvmpipe->viewport_index_slot =(int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      }
   }

   /* Figure out if we need layer (if it wasn't already in fs input) */
   if (llvmpipe->layer_slot < 0) {
      vs_index = draw_find_shader_output(llvmpipe->draw,
                                         TGSI_SEMANTIC_LAYER,
                                         0);
      if (vs_index >= 0) {
         llvmpipe->layer_slot = (int)vinfo->num_attribs;
         draw_emit_vertex_attr(vinfo, EMIT_4F, vs_index);
      }
   }

   draw_compute_vertex_size(vinfo);
   lp_setup_set_vertex_info(llvmpipe->setup, vinfo);
}


/**
 * Handle state changes.
 * Called just prior to drawing anything (pipe::draw_arrays(), etc).
 *
 * Hopefully this will remain quite simple, otherwise need to pull in
 * something like the state tracker mechanism.
 */
void llvmpipe_update_derived( struct llvmpipe_context *llvmpipe )
{
   struct llvmpipe_screen *lp_screen = llvmpipe_screen(llvmpipe->pipe.screen);

   /* Check for updated textures.
    */
   if (llvmpipe->tex_timestamp != lp_screen->timestamp) {
      llvmpipe->tex_timestamp = lp_screen->timestamp;
      llvmpipe->dirty |= LP_NEW_SAMPLER_VIEW;
   }

   /* This needs LP_NEW_RASTERIZER because of draw_prepare_shader_outputs(). */
   if (llvmpipe->dirty & (LP_NEW_RASTERIZER |
                          LP_NEW_FS |
                          LP_NEW_GS |
                          LP_NEW_VS))
      compute_vertex_info(llvmpipe);

   if (llvmpipe->dirty & (LP_NEW_FS |
                          LP_NEW_FRAMEBUFFER |
                          LP_NEW_BLEND |
                          LP_NEW_SCISSOR |
                          LP_NEW_DEPTH_STENCIL_ALPHA |
                          LP_NEW_RASTERIZER |
                          LP_NEW_SAMPLER |
                          LP_NEW_SAMPLER_VIEW |
                          LP_NEW_OCCLUSION_QUERY))
      llvmpipe_update_fs( llvmpipe );

   if (llvmpipe->dirty & (LP_NEW_RASTERIZER)) {
      boolean discard =
         (llvmpipe->sample_mask & 1) == 0 ||
         (llvmpipe->rasterizer ? llvmpipe->rasterizer->rasterizer_discard : FALSE);

      lp_setup_set_rasterizer_discard(llvmpipe->setup, discard);
   }

   if (llvmpipe->dirty & (LP_NEW_FS |
                          LP_NEW_FRAMEBUFFER |
                          LP_NEW_RASTERIZER))
      llvmpipe_update_setup( llvmpipe );

   if (llvmpipe->dirty & LP_NEW_BLEND_COLOR)
      lp_setup_set_blend_color(llvmpipe->setup,
                               &llvmpipe->blend_color);

   if (llvmpipe->dirty & LP_NEW_SCISSOR)
      lp_setup_set_scissors(llvmpipe->setup, llvmpipe->scissors);

   if (llvmpipe->dirty & LP_NEW_DEPTH_STENCIL_ALPHA) {
      lp_setup_set_alpha_ref_value(llvmpipe->setup, 
                                   llvmpipe->depth_stencil->alpha.ref_value);
      lp_setup_set_stencil_ref_values(llvmpipe->setup,
                                      llvmpipe->stencil_ref.ref_value);
   }

   if (llvmpipe->dirty & LP_NEW_FS_CONSTANTS)
      lp_setup_set_fs_constants(llvmpipe->setup,
                                ARRAY_SIZE(llvmpipe->constants[PIPE_SHADER_FRAGMENT]),
                                llvmpipe->constants[PIPE_SHADER_FRAGMENT]);

   if (llvmpipe->dirty & (LP_NEW_SAMPLER_VIEW))
      lp_setup_set_fragment_sampler_views(llvmpipe->setup,
                                          llvmpipe->num_sampler_views[PIPE_SHADER_FRAGMENT],
                                          llvmpipe->sampler_views[PIPE_SHADER_FRAGMENT]);

   if (llvmpipe->dirty & (LP_NEW_SAMPLER))
      lp_setup_set_fragment_sampler_state(llvmpipe->setup,
                                          llvmpipe->num_samplers[PIPE_SHADER_FRAGMENT],
                                          llvmpipe->samplers[PIPE_SHADER_FRAGMENT]);

   if (llvmpipe->dirty & LP_NEW_VIEWPORT) {
      /*
       * Update setup and fragment's view of the active viewport state.
       *
       * XXX TODO: It is possible to only loop over the active viewports
       *           instead of all viewports (PIPE_MAX_VIEWPORTS).
       */
      lp_setup_set_viewports(llvmpipe->setup,
                             PIPE_MAX_VIEWPORTS,
                             llvmpipe->viewports);
   }

   llvmpipe->dirty = 0;
}

