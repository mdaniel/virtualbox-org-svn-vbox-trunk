/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
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

/* Authors:
 *  Brian Paul
 */

#include "util/u_inlines.h"
#include "util/u_memory.h"

#include "draw/draw_context.h"

#include "lp_context.h"
#include "lp_screen.h"
#include "lp_state.h"
#include "lp_debug.h"
#include "state_tracker/sw_winsys.h"


static void *
llvmpipe_create_sampler_state(struct pipe_context *pipe,
                              const struct pipe_sampler_state *sampler)
{
   struct pipe_sampler_state *state = mem_dup(sampler, sizeof *sampler);

   if (LP_PERF & PERF_NO_MIP_LINEAR) {
      if (state->min_mip_filter == PIPE_TEX_MIPFILTER_LINEAR)
	 state->min_mip_filter = PIPE_TEX_MIPFILTER_NEAREST;
   }

   if (LP_PERF & PERF_NO_MIPMAPS)
      state->min_mip_filter = PIPE_TEX_MIPFILTER_NONE;

   if (LP_PERF & PERF_NO_LINEAR) {
      state->mag_img_filter = PIPE_TEX_FILTER_NEAREST;
      state->min_img_filter = PIPE_TEX_FILTER_NEAREST;
   }

   return state;
}


static void
llvmpipe_bind_sampler_states(struct pipe_context *pipe,
                             enum pipe_shader_type shader,
                             unsigned start,
                             unsigned num,
                             void **samplers)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   unsigned i;

   assert(shader < PIPE_SHADER_TYPES);
   assert(start + num <= ARRAY_SIZE(llvmpipe->samplers[shader]));

   draw_flush(llvmpipe->draw);

   /* set the new samplers */
   for (i = 0; i < num; i++) {
      llvmpipe->samplers[shader][start + i] = samplers[i];
   }

   /* find highest non-null samplers[] entry */
   {
      unsigned j = MAX2(llvmpipe->num_samplers[shader], start + num);
      while (j > 0 && llvmpipe->samplers[shader][j - 1] == NULL)
         j--;
      llvmpipe->num_samplers[shader] = j;
   }

   if (shader == PIPE_SHADER_VERTEX || shader == PIPE_SHADER_GEOMETRY) {
      draw_set_samplers(llvmpipe->draw,
                        shader,
                        llvmpipe->samplers[shader],
                        llvmpipe->num_samplers[shader]);
   }
   else {
      llvmpipe->dirty |= LP_NEW_SAMPLER;
   }
}


static void
llvmpipe_set_sampler_views(struct pipe_context *pipe,
                           enum pipe_shader_type shader,
                           unsigned start,
                           unsigned num,
                           struct pipe_sampler_view **views)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   uint i;

   assert(num <= PIPE_MAX_SHADER_SAMPLER_VIEWS);

   assert(shader < PIPE_SHADER_TYPES);
   assert(start + num <= ARRAY_SIZE(llvmpipe->sampler_views[shader]));

   draw_flush(llvmpipe->draw);

   /* set the new sampler views */
   for (i = 0; i < num; i++) {
      /* Note: we're using pipe_sampler_view_release() here to work around
       * a possible crash when the old view belongs to another context that
       * was already destroyed.
       */
      pipe_sampler_view_release(pipe,
                                &llvmpipe->sampler_views[shader][start + i]);
      /*
       * Warn if someone tries to set a view created in a different context
       * (which is why we need the hack above in the first place).
       * An assert would be better but st/mesa relies on it...
       */
      if (views[i] && views[i]->context != pipe) {
         debug_printf("Illegal setting of sampler_view %d created in another "
                      "context\n", i);
      }
      pipe_sampler_view_reference(&llvmpipe->sampler_views[shader][start + i],
                                  views[i]);
   }

   /* find highest non-null sampler_views[] entry */
   {
      unsigned j = MAX2(llvmpipe->num_sampler_views[shader], start + num);
      while (j > 0 && llvmpipe->sampler_views[shader][j - 1] == NULL)
         j--;
      llvmpipe->num_sampler_views[shader] = j;
   }

   if (shader == PIPE_SHADER_VERTEX || shader == PIPE_SHADER_GEOMETRY) {
      draw_set_sampler_views(llvmpipe->draw,
                             shader,
                             llvmpipe->sampler_views[shader],
                             llvmpipe->num_sampler_views[shader]);
   }
   else {
      llvmpipe->dirty |= LP_NEW_SAMPLER_VIEW;
   }
}


static struct pipe_sampler_view *
llvmpipe_create_sampler_view(struct pipe_context *pipe,
                            struct pipe_resource *texture,
                            const struct pipe_sampler_view *templ)
{
   struct pipe_sampler_view *view = CALLOC_STRUCT(pipe_sampler_view);
   /*
    * XXX: bind flags from OpenGL state tracker are notoriously unreliable.
    * This looks unfixable, so fix the bind flags instead when it happens.
    */
   if (!(texture->bind & PIPE_BIND_SAMPLER_VIEW)) {
      debug_printf("Illegal sampler view creation without bind flag\n");
      texture->bind |= PIPE_BIND_SAMPLER_VIEW;
   }

   if (view) {
      *view = *templ;
      view->reference.count = 1;
      view->texture = NULL;
      pipe_resource_reference(&view->texture, texture);
      view->context = pipe;

#ifdef DEBUG
     /*
      * This is possibly too lenient, but the primary reason is just
      * to catch state trackers which forget to initialize this, so
      * it only catches clearly impossible view targets.
      */
      if (view->target != texture->target) {
         if (view->target == PIPE_TEXTURE_1D)
            assert(texture->target == PIPE_TEXTURE_1D_ARRAY);
         else if (view->target == PIPE_TEXTURE_1D_ARRAY)
            assert(texture->target == PIPE_TEXTURE_1D);
         else if (view->target == PIPE_TEXTURE_2D)
            assert(texture->target == PIPE_TEXTURE_2D_ARRAY ||
                   texture->target == PIPE_TEXTURE_CUBE ||
                   texture->target == PIPE_TEXTURE_CUBE_ARRAY);
         else if (view->target == PIPE_TEXTURE_2D_ARRAY)
            assert(texture->target == PIPE_TEXTURE_2D ||
                   texture->target == PIPE_TEXTURE_CUBE ||
                   texture->target == PIPE_TEXTURE_CUBE_ARRAY);
         else if (view->target == PIPE_TEXTURE_CUBE)
            assert(texture->target == PIPE_TEXTURE_CUBE_ARRAY ||
                   texture->target == PIPE_TEXTURE_2D_ARRAY);
         else if (view->target == PIPE_TEXTURE_CUBE_ARRAY)
            assert(texture->target == PIPE_TEXTURE_CUBE ||
                   texture->target == PIPE_TEXTURE_2D_ARRAY);
         else
            assert(0);
      }
#endif
   }

   return view;
}


static void
llvmpipe_sampler_view_destroy(struct pipe_context *pipe,
                              struct pipe_sampler_view *view)
{
   pipe_resource_reference(&view->texture, NULL);
   FREE(view);
}


static void
llvmpipe_delete_sampler_state(struct pipe_context *pipe,
                              void *sampler)
{
   FREE( sampler );
}


static void
prepare_shader_sampling(
   struct llvmpipe_context *lp,
   unsigned num,
   struct pipe_sampler_view **views,
   enum pipe_shader_type shader_type)
{

   unsigned i;
   uint32_t row_stride[PIPE_MAX_TEXTURE_LEVELS];
   uint32_t img_stride[PIPE_MAX_TEXTURE_LEVELS];
   uint32_t mip_offsets[PIPE_MAX_TEXTURE_LEVELS];
   const void *addr;

   assert(num <= PIPE_MAX_SHADER_SAMPLER_VIEWS);
   if (!num)
      return;

   for (i = 0; i < num; i++) {
      struct pipe_sampler_view *view = i < num ? views[i] : NULL;

      if (view) {
         struct pipe_resource *tex = view->texture;
         struct llvmpipe_resource *lp_tex = llvmpipe_resource(tex);
         unsigned width0 = tex->width0;
         unsigned num_layers = tex->depth0;
         unsigned first_level = 0;
         unsigned last_level = 0;

         if (!lp_tex->dt) {
            /* regular texture - setup array of mipmap level offsets */
            struct pipe_resource *res = view->texture;
            int j;

            if (llvmpipe_resource_is_texture(res)) {
               first_level = view->u.tex.first_level;
               last_level = view->u.tex.last_level;
               assert(first_level <= last_level);
               assert(last_level <= res->last_level);
               addr = lp_tex->tex_data;

               for (j = first_level; j <= last_level; j++) {
                  mip_offsets[j] = lp_tex->mip_offsets[j];
                  row_stride[j] = lp_tex->row_stride[j];
                  img_stride[j] = lp_tex->img_stride[j];
               }
               if (tex->target == PIPE_TEXTURE_1D_ARRAY ||
                   tex->target == PIPE_TEXTURE_2D_ARRAY ||
                   tex->target == PIPE_TEXTURE_CUBE ||
                   tex->target == PIPE_TEXTURE_CUBE_ARRAY) {
                  num_layers = view->u.tex.last_layer - view->u.tex.first_layer + 1;
                  for (j = first_level; j <= last_level; j++) {
                     mip_offsets[j] += view->u.tex.first_layer *
                                       lp_tex->img_stride[j];
                  }
                  if (view->target == PIPE_TEXTURE_CUBE ||
                      view->target == PIPE_TEXTURE_CUBE_ARRAY) {
                     assert(num_layers % 6 == 0);
                  }
                  assert(view->u.tex.first_layer <= view->u.tex.last_layer);
                  assert(view->u.tex.last_layer < res->array_size);
               }
            }
            else {
               unsigned view_blocksize = util_format_get_blocksize(view->format);
               addr = lp_tex->data;
               /* probably don't really need to fill that out */
               mip_offsets[0] = 0;
               row_stride[0] = 0;
               img_stride[0] = 0;

               /* everything specified in number of elements here. */
               width0 = view->u.buf.size / view_blocksize;
               addr = (uint8_t *)addr + view->u.buf.offset;
               assert(view->u.buf.offset + view->u.buf.size <= res->width0);
            }
         }
         else {
            /* display target texture/surface */
            /*
             * XXX: Where should this be unmapped?
             */
            struct llvmpipe_screen *screen = llvmpipe_screen(tex->screen);
            struct sw_winsys *winsys = screen->winsys;
            addr = winsys->displaytarget_map(winsys, lp_tex->dt,
                                                PIPE_TRANSFER_READ);
            row_stride[0] = lp_tex->row_stride[0];
            img_stride[0] = lp_tex->img_stride[0];
            mip_offsets[0] = 0;
            assert(addr);
         }
         draw_set_mapped_texture(lp->draw,
                                 shader_type,
                                 i,
                                 width0, tex->height0, num_layers,
                                 first_level, last_level,
                                 addr,
                                 row_stride, img_stride, mip_offsets);
      }
   }
}


/**
 * Called whenever we're about to draw (no dirty flag, FIXME?).
 */
void
llvmpipe_prepare_vertex_sampling(struct llvmpipe_context *lp,
                                 unsigned num,
                                 struct pipe_sampler_view **views)
{
   prepare_shader_sampling(lp, num, views, PIPE_SHADER_VERTEX);
}


/**
 * Called whenever we're about to draw (no dirty flag, FIXME?).
 */
void
llvmpipe_prepare_geometry_sampling(struct llvmpipe_context *lp,
                                   unsigned num,
                                   struct pipe_sampler_view **views)
{
   prepare_shader_sampling(lp, num, views, PIPE_SHADER_GEOMETRY);
}


void
llvmpipe_init_sampler_funcs(struct llvmpipe_context *llvmpipe)
{
   llvmpipe->pipe.create_sampler_state = llvmpipe_create_sampler_state;

   llvmpipe->pipe.bind_sampler_states = llvmpipe_bind_sampler_states;
   llvmpipe->pipe.create_sampler_view = llvmpipe_create_sampler_view;
   llvmpipe->pipe.set_sampler_views = llvmpipe_set_sampler_views;
   llvmpipe->pipe.sampler_view_destroy = llvmpipe_sampler_view_destroy;
   llvmpipe->pipe.delete_sampler_state = llvmpipe_delete_sampler_state;
}
