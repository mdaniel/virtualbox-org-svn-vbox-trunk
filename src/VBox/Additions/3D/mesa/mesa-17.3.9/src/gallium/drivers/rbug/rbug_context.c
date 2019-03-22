/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
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


#include "pipe/p_context.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/simple_list.h"

#include "rbug/rbug_context.h"

#include "rbug_context.h"
#include "rbug_objects.h"


static void
rbug_destroy(struct pipe_context *_pipe)
{
   struct rbug_screen *rb_screen = rbug_screen(_pipe->screen);
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   rbug_screen_remove_from_list(rb_screen, contexts, rb_pipe);

   mtx_lock(&rb_pipe->call_mutex);
   pipe->destroy(pipe);
   rb_pipe->pipe = NULL;
   mtx_unlock(&rb_pipe->call_mutex);

   FREE(rb_pipe);
}

static void
rbug_draw_block_locked(struct rbug_context *rb_pipe, int flag)
{

   if (rb_pipe->draw_blocker & flag) {
      rb_pipe->draw_blocked |= flag;
   } else if ((rb_pipe->draw_rule.blocker & flag) &&
              (rb_pipe->draw_blocker & RBUG_BLOCK_RULE)) {
      unsigned k;
      boolean block = FALSE;
      unsigned sh;

      debug_printf("%s (%p %p) (%p %p) (%p %u) (%p %u)\n", __FUNCTION__,
                   (void *) rb_pipe->draw_rule.shader[PIPE_SHADER_FRAGMENT],
                   (void *) rb_pipe->curr.shader[PIPE_SHADER_FRAGMENT],
                   (void *) rb_pipe->draw_rule.shader[PIPE_SHADER_VERTEX],
                   (void *) rb_pipe->curr.shader[PIPE_SHADER_VERTEX],
                   (void *) rb_pipe->draw_rule.surf, 0,
                   (void *) rb_pipe->draw_rule.texture, 0);
      for (sh = 0; sh < PIPE_SHADER_TYPES; sh++) {
         if (rb_pipe->draw_rule.shader[sh] &&
             rb_pipe->draw_rule.shader[sh] == rb_pipe->curr.shader[sh])
            block = TRUE;
      }

      if (rb_pipe->draw_rule.surf &&
          rb_pipe->draw_rule.surf == rb_pipe->curr.zsbuf)
            block = TRUE;
      if (rb_pipe->draw_rule.surf)
         for (k = 0; k < rb_pipe->curr.nr_cbufs; k++)
            if (rb_pipe->draw_rule.surf == rb_pipe->curr.cbufs[k])
               block = TRUE;
      if (rb_pipe->draw_rule.texture) {
         for (sh = 0; sh < ARRAY_SIZE(rb_pipe->curr.num_views); sh++) {
            for (k = 0; k < rb_pipe->curr.num_views[sh]; k++) {
               if (rb_pipe->draw_rule.texture == rb_pipe->curr.texs[sh][k]) {
                  block = TRUE;
                  sh = PIPE_SHADER_TYPES; /* to break out of both loops */
                  break;
               }
            }
         }
      }

      if (block)
         rb_pipe->draw_blocked |= (flag | RBUG_BLOCK_RULE);
   }

   if (rb_pipe->draw_blocked)
      rbug_notify_draw_blocked(rb_pipe);

   /* wait for rbug to clear the blocked flag */
   while (rb_pipe->draw_blocked & flag) {
      rb_pipe->draw_blocked |= flag;
      cnd_wait(&rb_pipe->draw_cond, &rb_pipe->draw_mutex);
   }

}

static void
rbug_draw_vbo(struct pipe_context *_pipe, const struct pipe_draw_info *info)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->draw_mutex);
   rbug_draw_block_locked(rb_pipe, RBUG_BLOCK_BEFORE);

   mtx_lock(&rb_pipe->call_mutex);
   /* XXX loop over PIPE_SHADER_x here */
   if (!(rb_pipe->curr.shader[PIPE_SHADER_FRAGMENT] && rb_pipe->curr.shader[PIPE_SHADER_FRAGMENT]->disabled) &&
       !(rb_pipe->curr.shader[PIPE_SHADER_GEOMETRY] && rb_pipe->curr.shader[PIPE_SHADER_GEOMETRY]->disabled) &&
       !(rb_pipe->curr.shader[PIPE_SHADER_VERTEX] && rb_pipe->curr.shader[PIPE_SHADER_VERTEX]->disabled))
      pipe->draw_vbo(pipe, info);
   mtx_unlock(&rb_pipe->call_mutex);

   rbug_draw_block_locked(rb_pipe, RBUG_BLOCK_AFTER);
   mtx_unlock(&rb_pipe->draw_mutex);
}

static struct pipe_query *
rbug_create_query(struct pipe_context *_pipe,
                  unsigned query_type,
                  unsigned index)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_query *query;

   mtx_lock(&rb_pipe->call_mutex);
   query = pipe->create_query(pipe,
                              query_type,
                              index);
   mtx_unlock(&rb_pipe->call_mutex);
   return query;
}

static void
rbug_destroy_query(struct pipe_context *_pipe,
                   struct pipe_query *query)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->destroy_query(pipe,
                       query);
   mtx_unlock(&rb_pipe->call_mutex);
}

static boolean
rbug_begin_query(struct pipe_context *_pipe,
                 struct pipe_query *query)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   boolean ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->begin_query(pipe, query);
   mtx_unlock(&rb_pipe->call_mutex);
   return ret;
}

static bool
rbug_end_query(struct pipe_context *_pipe,
               struct pipe_query *query)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   bool ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->end_query(pipe,
                         query);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static boolean
rbug_get_query_result(struct pipe_context *_pipe,
                      struct pipe_query *query,
                      boolean wait,
                      union pipe_query_result *result)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   boolean ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->get_query_result(pipe,
                                query,
                                wait,
                                result);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_set_active_query_state(struct pipe_context *_pipe, boolean enable)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_active_query_state(pipe, enable);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_blend_state(struct pipe_context *_pipe,
                        const struct pipe_blend_state *blend)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->create_blend_state(pipe,
                                  blend);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_bind_blend_state(struct pipe_context *_pipe,
                      void *blend)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->bind_blend_state(pipe,
                          blend);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_blend_state(struct pipe_context *_pipe,
                        void *blend)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->delete_blend_state(pipe,
                            blend);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_sampler_state(struct pipe_context *_pipe,
                          const struct pipe_sampler_state *sampler)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->create_sampler_state(pipe,
                                    sampler);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_bind_sampler_states(struct pipe_context *_pipe,
                         enum pipe_shader_type shader,
                         unsigned start, unsigned count,
                         void **samplers)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->bind_sampler_states(pipe, shader, start, count, samplers);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_sampler_state(struct pipe_context *_pipe,
                          void *sampler)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->delete_sampler_state(pipe,
                              sampler);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_rasterizer_state(struct pipe_context *_pipe,
                             const struct pipe_rasterizer_state *rasterizer)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->create_rasterizer_state(pipe,
                                       rasterizer);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_bind_rasterizer_state(struct pipe_context *_pipe,
                           void *rasterizer)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->bind_rasterizer_state(pipe,
                               rasterizer);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_rasterizer_state(struct pipe_context *_pipe,
                             void *rasterizer)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->delete_rasterizer_state(pipe,
                                 rasterizer);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                      const struct pipe_depth_stencil_alpha_state *depth_stencil_alpha)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->create_depth_stencil_alpha_state(pipe,
                                                depth_stencil_alpha);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_bind_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                    void *depth_stencil_alpha)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->bind_depth_stencil_alpha_state(pipe,
                                        depth_stencil_alpha);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                      void *depth_stencil_alpha)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->delete_depth_stencil_alpha_state(pipe,
                                          depth_stencil_alpha);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_fs_state(struct pipe_context *_pipe,
                     const struct pipe_shader_state *state)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *result;

   mtx_lock(&rb_pipe->call_mutex);
   result = pipe->create_fs_state(pipe, state);
   mtx_unlock(&rb_pipe->call_mutex);

   if (!result)
      return NULL;

   return rbug_shader_create(rb_pipe, state, result, RBUG_SHADER_FRAGMENT);
}

static void
rbug_bind_fs_state(struct pipe_context *_pipe,
                   void *_fs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *fs;

   mtx_lock(&rb_pipe->call_mutex);

   fs = rbug_shader_unwrap(_fs);
   rb_pipe->curr.shader[PIPE_SHADER_FRAGMENT] = rbug_shader(_fs);
   pipe->bind_fs_state(pipe,
                       fs);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_fs_state(struct pipe_context *_pipe,
                     void *_fs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_shader *rb_shader = rbug_shader(_fs);

   mtx_lock(&rb_pipe->call_mutex);
   rbug_shader_destroy(rb_pipe, rb_shader);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_vs_state(struct pipe_context *_pipe,
                     const struct pipe_shader_state *state)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *result;

   mtx_lock(&rb_pipe->call_mutex);
   result = pipe->create_vs_state(pipe, state);
   mtx_unlock(&rb_pipe->call_mutex);

   if (!result)
      return NULL;

   return rbug_shader_create(rb_pipe, state, result, RBUG_SHADER_VERTEX);
}

static void
rbug_bind_vs_state(struct pipe_context *_pipe,
                   void *_vs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *vs;

   mtx_lock(&rb_pipe->call_mutex);

   vs = rbug_shader_unwrap(_vs);
   rb_pipe->curr.shader[PIPE_SHADER_VERTEX] = rbug_shader(_vs);
   pipe->bind_vs_state(pipe,
                       vs);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_vs_state(struct pipe_context *_pipe,
                     void *_vs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_shader *rb_shader = rbug_shader(_vs);

   mtx_unlock(&rb_pipe->call_mutex);
   rbug_shader_destroy(rb_pipe, rb_shader);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_gs_state(struct pipe_context *_pipe,
                     const struct pipe_shader_state *state)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *result;

   mtx_lock(&rb_pipe->call_mutex);
   result = pipe->create_gs_state(pipe, state);
   mtx_unlock(&rb_pipe->call_mutex);

   if (!result)
      return NULL;

   return rbug_shader_create(rb_pipe, state, result, RBUG_SHADER_GEOM);
}

static void
rbug_bind_gs_state(struct pipe_context *_pipe,
                   void *_gs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *gs;

   mtx_lock(&rb_pipe->call_mutex);

   gs = rbug_shader_unwrap(_gs);
   rb_pipe->curr.shader[PIPE_SHADER_GEOMETRY] = rbug_shader(_gs);
   pipe->bind_gs_state(pipe,
                       gs);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_gs_state(struct pipe_context *_pipe,
                     void *_gs)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_shader *rb_shader = rbug_shader(_gs);

   mtx_lock(&rb_pipe->call_mutex);
   rbug_shader_destroy(rb_pipe, rb_shader);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void *
rbug_create_vertex_elements_state(struct pipe_context *_pipe,
                                  unsigned num_elements,
                                  const struct pipe_vertex_element *vertex_elements)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   void *ret;

   mtx_lock(&rb_pipe->call_mutex);
   ret = pipe->create_vertex_elements_state(pipe,
                                             num_elements,
                                             vertex_elements);
   mtx_unlock(&rb_pipe->call_mutex);

   return ret;
}

static void
rbug_bind_vertex_elements_state(struct pipe_context *_pipe,
                                void *velems)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->bind_vertex_elements_state(pipe,
                                    velems);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_delete_vertex_elements_state(struct pipe_context *_pipe,
                                  void *velems)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->delete_vertex_elements_state(pipe,
                                      velems);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_blend_color(struct pipe_context *_pipe,
                     const struct pipe_blend_color *blend_color)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_blend_color(pipe,
                         blend_color);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_stencil_ref(struct pipe_context *_pipe,
                     const struct pipe_stencil_ref *stencil_ref)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_stencil_ref(pipe,
                         stencil_ref);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_clip_state(struct pipe_context *_pipe,
                    const struct pipe_clip_state *clip)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_clip_state(pipe,
                        clip);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_constant_buffer(struct pipe_context *_pipe,
                         enum pipe_shader_type shader,
                         uint index,
                         const struct pipe_constant_buffer *_cb)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_constant_buffer cb;

   /* XXX hmm? unwrap the input state */
   if (_cb) {
      cb = *_cb;
      cb.buffer = rbug_resource_unwrap(_cb->buffer);
   }

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_constant_buffer(pipe,
                             shader,
                             index,
                             _cb ? &cb : NULL);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_framebuffer_state(struct pipe_context *_pipe,
                           const struct pipe_framebuffer_state *_state)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_framebuffer_state unwrapped_state;
   struct pipe_framebuffer_state *state = NULL;
   unsigned i;

   /* must protect curr status */
   mtx_lock(&rb_pipe->call_mutex);

   rb_pipe->curr.nr_cbufs = 0;
   memset(rb_pipe->curr.cbufs, 0, sizeof(rb_pipe->curr.cbufs));
   rb_pipe->curr.zsbuf = NULL;

   /* unwrap the input state */
   if (_state) {
      memcpy(&unwrapped_state, _state, sizeof(unwrapped_state));

      rb_pipe->curr.nr_cbufs = _state->nr_cbufs;
      for(i = 0; i < _state->nr_cbufs; i++) {
         unwrapped_state.cbufs[i] = rbug_surface_unwrap(_state->cbufs[i]);
         if (_state->cbufs[i])
            rb_pipe->curr.cbufs[i] = rbug_resource(_state->cbufs[i]->texture);
      }
      unwrapped_state.zsbuf = rbug_surface_unwrap(_state->zsbuf);
      if (_state->zsbuf)
         rb_pipe->curr.zsbuf = rbug_resource(_state->zsbuf->texture);
      state = &unwrapped_state;
   }

   pipe->set_framebuffer_state(pipe,
                               state);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_polygon_stipple(struct pipe_context *_pipe,
                         const struct pipe_poly_stipple *poly_stipple)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_polygon_stipple(pipe,
                             poly_stipple);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_scissor_states(struct pipe_context *_pipe,
                        unsigned start_slot,
                        unsigned num_scissors,
                        const struct pipe_scissor_state *scissor)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_scissor_states(pipe, start_slot, num_scissors, scissor);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_viewport_states(struct pipe_context *_pipe,
                         unsigned start_slot,
                         unsigned num_viewports,
                         const struct pipe_viewport_state *viewport)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_viewport_states(pipe, start_slot, num_viewports, viewport);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_sampler_views(struct pipe_context *_pipe,
                       enum pipe_shader_type shader,
                       unsigned start,
                       unsigned num,
                       struct pipe_sampler_view **_views)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_sampler_view *unwrapped_views[PIPE_MAX_SHADER_SAMPLER_VIEWS];
   struct pipe_sampler_view **views = NULL;
   unsigned i;

   assert(start == 0); /* XXX fix */

   /* must protect curr status */
   mtx_lock(&rb_pipe->call_mutex);

   rb_pipe->curr.num_views[shader] = 0;
   memset(rb_pipe->curr.views[shader], 0, sizeof(rb_pipe->curr.views[shader]));
   memset(rb_pipe->curr.texs[shader], 0, sizeof(rb_pipe->curr.texs[shader]));
   memset(unwrapped_views, 0, sizeof(unwrapped_views));

   if (_views) {
      rb_pipe->curr.num_views[shader] = num;
      for (i = 0; i < num; i++) {
         rb_pipe->curr.views[shader][i] = rbug_sampler_view(_views[i]);
         rb_pipe->curr.texs[shader][i] = rbug_resource(_views[i] ? _views[i]->texture : NULL);
         unwrapped_views[i] = rbug_sampler_view_unwrap(_views[i]);
      }
      views = unwrapped_views;
   }

   pipe->set_sampler_views(pipe, shader, start, num, views);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_vertex_buffers(struct pipe_context *_pipe,
                        unsigned start_slot, unsigned num_buffers,
                        const struct pipe_vertex_buffer *_buffers)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_vertex_buffer unwrapped_buffers[PIPE_MAX_SHADER_INPUTS];
   struct pipe_vertex_buffer *buffers = NULL;
   unsigned i;

   mtx_lock(&rb_pipe->call_mutex);

   if (num_buffers && _buffers) {
      memcpy(unwrapped_buffers, _buffers, num_buffers * sizeof(*_buffers));
      for (i = 0; i < num_buffers; i++) {
         if (!_buffers[i].is_user_buffer)
            unwrapped_buffers[i].buffer.resource =
               rbug_resource_unwrap(_buffers[i].buffer.resource);
      }
      buffers = unwrapped_buffers;
   }

   pipe->set_vertex_buffers(pipe, start_slot,
                            num_buffers,
                            buffers);

   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_sample_mask(struct pipe_context *_pipe,
                     unsigned sample_mask)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_sample_mask(pipe, sample_mask);
   mtx_unlock(&rb_pipe->call_mutex);
}

static struct pipe_stream_output_target *
rbug_create_stream_output_target(struct pipe_context *_pipe,
                                 struct pipe_resource *_res,
                                 unsigned buffer_offset, unsigned buffer_size)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *res = rbug_resource_unwrap(_res);
   struct pipe_stream_output_target *target;

   mtx_lock(&rb_pipe->call_mutex);
   target = pipe->create_stream_output_target(pipe, res, buffer_offset,
                                              buffer_size);
   mtx_unlock(&rb_pipe->call_mutex);
   return target;
}

static void
rbug_stream_output_target_destroy(struct pipe_context *_pipe,
                                  struct pipe_stream_output_target *target)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->stream_output_target_destroy(pipe, target);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_set_stream_output_targets(struct pipe_context *_pipe,
                               unsigned num_targets,
                               struct pipe_stream_output_target **targets,
                               const unsigned *offsets)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->set_stream_output_targets(pipe, num_targets, targets, offsets);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_resource_copy_region(struct pipe_context *_pipe,
                          struct pipe_resource *_dst,
                          unsigned dst_level,
                          unsigned dstx,
                          unsigned dsty,
                          unsigned dstz,
                          struct pipe_resource *_src,
                          unsigned src_level,
                          const struct pipe_box *src_box)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_resource *rb_resource_dst = rbug_resource(_dst);
   struct rbug_resource *rb_resource_src = rbug_resource(_src);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *dst = rb_resource_dst->resource;
   struct pipe_resource *src = rb_resource_src->resource;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->resource_copy_region(pipe,
                              dst,
                              dst_level,
                              dstx,
                              dsty,
                              dstz,
                              src,
                              src_level,
                              src_box);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_blit(struct pipe_context *_pipe, const struct pipe_blit_info *_blit_info)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_resource *rb_resource_dst = rbug_resource(_blit_info->dst.resource);
   struct rbug_resource *rb_resource_src = rbug_resource(_blit_info->src.resource);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *dst = rb_resource_dst->resource;
   struct pipe_resource *src = rb_resource_src->resource;
   struct pipe_blit_info blit_info;

   blit_info = *_blit_info;
   blit_info.dst.resource = dst;
   blit_info.src.resource = src;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->blit(pipe, &blit_info);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_flush_resource(struct pipe_context *_pipe,
                    struct pipe_resource *_res)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_resource *rb_resource_res = rbug_resource(_res);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *res = rb_resource_res->resource;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->flush_resource(pipe, res);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_clear(struct pipe_context *_pipe,
           unsigned buffers,
           const union pipe_color_union *color,
           double depth,
           unsigned stencil)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->clear(pipe,
               buffers,
               color,
               depth,
               stencil);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_clear_render_target(struct pipe_context *_pipe,
                         struct pipe_surface *_dst,
                         const union pipe_color_union *color,
                         unsigned dstx, unsigned dsty,
                         unsigned width, unsigned height,
                         bool render_condition_enabled)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_surface *rb_surface_dst = rbug_surface(_dst);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_surface *dst = rb_surface_dst->surface;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->clear_render_target(pipe,
                             dst,
                             color,
                             dstx,
                             dsty,
                             width,
                             height,
                             render_condition_enabled);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_clear_depth_stencil(struct pipe_context *_pipe,
                         struct pipe_surface *_dst,
                         unsigned clear_flags,
                         double depth,
                         unsigned stencil,
                         unsigned dstx, unsigned dsty,
                         unsigned width, unsigned height,
                         bool render_condition_enabled)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_surface *rb_surface_dst = rbug_surface(_dst);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_surface *dst = rb_surface_dst->surface;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->clear_depth_stencil(pipe,
                             dst,
                             clear_flags,
                             depth,
                             stencil,
                             dstx,
                             dsty,
                             width,
                             height,
                             render_condition_enabled);
   mtx_unlock(&rb_pipe->call_mutex);
}

static void
rbug_flush(struct pipe_context *_pipe,
           struct pipe_fence_handle **fence,
           unsigned flags)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct pipe_context *pipe = rb_pipe->pipe;

   mtx_lock(&rb_pipe->call_mutex);
   pipe->flush(pipe, fence, flags);
   mtx_unlock(&rb_pipe->call_mutex);
}

static struct pipe_sampler_view *
rbug_context_create_sampler_view(struct pipe_context *_pipe,
                                 struct pipe_resource *_resource,
                                 const struct pipe_sampler_view *templ)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_resource *rb_resource = rbug_resource(_resource);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *resource = rb_resource->resource;
   struct pipe_sampler_view *result;

   mtx_lock(&rb_pipe->call_mutex);
   result = pipe->create_sampler_view(pipe,
                                      resource,
                                      templ);
   mtx_unlock(&rb_pipe->call_mutex);

   if (result)
      return rbug_sampler_view_create(rb_pipe, rb_resource, result);
   return NULL;
}

static void
rbug_context_sampler_view_destroy(struct pipe_context *_pipe,
                                  struct pipe_sampler_view *_view)
{
   rbug_sampler_view_destroy(rbug_context(_pipe),
                             rbug_sampler_view(_view));
}

static struct pipe_surface *
rbug_context_create_surface(struct pipe_context *_pipe,
                            struct pipe_resource *_resource,
                            const struct pipe_surface *surf_tmpl)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_resource *rb_resource = rbug_resource(_resource);
   struct pipe_context *pipe = rb_pipe->pipe;
   struct pipe_resource *resource = rb_resource->resource;
   struct pipe_surface *result;

   mtx_lock(&rb_pipe->call_mutex);
   result = pipe->create_surface(pipe,
                                 resource,
                                 surf_tmpl);
   mtx_unlock(&rb_pipe->call_mutex);

   if (result)
      return rbug_surface_create(rb_pipe, rb_resource, result);
   return NULL;
}

static void
rbug_context_surface_destroy(struct pipe_context *_pipe,
                             struct pipe_surface *_surface)
{
   struct rbug_context *rb_pipe = rbug_context(_pipe);
   struct rbug_surface *rb_surface = rbug_surface(_surface);

   mtx_lock(&rb_pipe->call_mutex);
   rbug_surface_destroy(rb_pipe,
                        rb_surface);
   mtx_unlock(&rb_pipe->call_mutex);
}



static void *
rbug_context_transfer_map(struct pipe_context *_context,
                          struct pipe_resource *_resource,
                          unsigned level,
                          unsigned usage,
                          const struct pipe_box *box,
                          struct pipe_transfer **transfer)
{
   struct rbug_context *rb_pipe = rbug_context(_context);
   struct rbug_resource *rb_resource = rbug_resource(_resource);
   struct pipe_context *context = rb_pipe->pipe;
   struct pipe_resource *resource = rb_resource->resource;
   struct pipe_transfer *result;
   void *map;

   mtx_lock(&rb_pipe->call_mutex);
   map = context->transfer_map(context,
                               resource,
                               level,
                               usage,
                               box, &result);
   mtx_unlock(&rb_pipe->call_mutex);

   *transfer = rbug_transfer_create(rb_pipe, rb_resource, result);
   return *transfer ? map : NULL;
}

static void
rbug_context_transfer_flush_region(struct pipe_context *_context,
                                   struct pipe_transfer *_transfer,
                                   const struct pipe_box *box)
{
   struct rbug_context *rb_pipe = rbug_context(_context);
   struct rbug_transfer *rb_transfer = rbug_transfer(_transfer);
   struct pipe_context *context = rb_pipe->pipe;
   struct pipe_transfer *transfer = rb_transfer->transfer;

   mtx_lock(&rb_pipe->call_mutex);
   context->transfer_flush_region(context,
                                  transfer,
                                  box);
   mtx_unlock(&rb_pipe->call_mutex);
}


static void
rbug_context_transfer_unmap(struct pipe_context *_context,
                            struct pipe_transfer *_transfer)
{
   struct rbug_context *rb_pipe = rbug_context(_context);
   struct rbug_transfer *rb_transfer = rbug_transfer(_transfer);
   struct pipe_context *context = rb_pipe->pipe;
   struct pipe_transfer *transfer = rb_transfer->transfer;

   mtx_lock(&rb_pipe->call_mutex);
   context->transfer_unmap(context,
                           transfer);
   rbug_transfer_destroy(rb_pipe,
                         rb_transfer);
   mtx_unlock(&rb_pipe->call_mutex);
}


static void
rbug_context_buffer_subdata(struct pipe_context *_context,
                            struct pipe_resource *_resource,
                            unsigned usage, unsigned offset,
                            unsigned size, const void *data)
{
   struct rbug_context *rb_pipe = rbug_context(_context);
   struct rbug_resource *rb_resource = rbug_resource(_resource);
   struct pipe_context *context = rb_pipe->pipe;
   struct pipe_resource *resource = rb_resource->resource;

   mtx_lock(&rb_pipe->call_mutex);
   context->buffer_subdata(context, resource, usage, offset, size, data);
   mtx_unlock(&rb_pipe->call_mutex);
}


static void
rbug_context_texture_subdata(struct pipe_context *_context,
                             struct pipe_resource *_resource,
                             unsigned level,
                             unsigned usage,
                             const struct pipe_box *box,
                             const void *data,
                             unsigned stride,
                             unsigned layer_stride)
{
   struct rbug_context *rb_pipe = rbug_context(_context);
   struct rbug_resource *rb_resource = rbug_resource(_resource);
   struct pipe_context *context = rb_pipe->pipe;
   struct pipe_resource *resource = rb_resource->resource;

   mtx_lock(&rb_pipe->call_mutex);
   context->texture_subdata(context,
                            resource,
                            level,
                            usage,
                            box,
                            data,
                            stride,
                            layer_stride);
   mtx_unlock(&rb_pipe->call_mutex);
}


struct pipe_context *
rbug_context_create(struct pipe_screen *_screen, struct pipe_context *pipe)
{
   struct rbug_context *rb_pipe;
   struct rbug_screen *rb_screen = rbug_screen(_screen);

   if (!rb_screen)
      return NULL;

   rb_pipe = CALLOC_STRUCT(rbug_context);
   if (!rb_pipe)
      return NULL;

   (void) mtx_init(&rb_pipe->draw_mutex, mtx_plain);
   cnd_init(&rb_pipe->draw_cond);
   (void) mtx_init(&rb_pipe->call_mutex, mtx_plain);
   (void) mtx_init(&rb_pipe->list_mutex, mtx_plain);
   make_empty_list(&rb_pipe->shaders);

   rb_pipe->base.screen = _screen;
   rb_pipe->base.priv = pipe->priv; /* expose wrapped data */
   rb_pipe->base.draw = NULL;
   rb_pipe->base.stream_uploader = pipe->stream_uploader;
   rb_pipe->base.const_uploader = pipe->const_uploader;

   rb_pipe->base.destroy = rbug_destroy;
   rb_pipe->base.draw_vbo = rbug_draw_vbo;
   rb_pipe->base.create_query = rbug_create_query;
   rb_pipe->base.destroy_query = rbug_destroy_query;
   rb_pipe->base.begin_query = rbug_begin_query;
   rb_pipe->base.end_query = rbug_end_query;
   rb_pipe->base.get_query_result = rbug_get_query_result;
   rb_pipe->base.set_active_query_state = rbug_set_active_query_state;
   rb_pipe->base.create_blend_state = rbug_create_blend_state;
   rb_pipe->base.bind_blend_state = rbug_bind_blend_state;
   rb_pipe->base.delete_blend_state = rbug_delete_blend_state;
   rb_pipe->base.create_sampler_state = rbug_create_sampler_state;
   rb_pipe->base.bind_sampler_states = rbug_bind_sampler_states;
   rb_pipe->base.delete_sampler_state = rbug_delete_sampler_state;
   rb_pipe->base.create_rasterizer_state = rbug_create_rasterizer_state;
   rb_pipe->base.bind_rasterizer_state = rbug_bind_rasterizer_state;
   rb_pipe->base.delete_rasterizer_state = rbug_delete_rasterizer_state;
   rb_pipe->base.create_depth_stencil_alpha_state = rbug_create_depth_stencil_alpha_state;
   rb_pipe->base.bind_depth_stencil_alpha_state = rbug_bind_depth_stencil_alpha_state;
   rb_pipe->base.delete_depth_stencil_alpha_state = rbug_delete_depth_stencil_alpha_state;
   rb_pipe->base.create_fs_state = rbug_create_fs_state;
   rb_pipe->base.bind_fs_state = rbug_bind_fs_state;
   rb_pipe->base.delete_fs_state = rbug_delete_fs_state;
   rb_pipe->base.create_vs_state = rbug_create_vs_state;
   rb_pipe->base.bind_vs_state = rbug_bind_vs_state;
   rb_pipe->base.delete_vs_state = rbug_delete_vs_state;
   rb_pipe->base.create_gs_state = rbug_create_gs_state;
   rb_pipe->base.bind_gs_state = rbug_bind_gs_state;
   rb_pipe->base.delete_gs_state = rbug_delete_gs_state;
   rb_pipe->base.create_vertex_elements_state = rbug_create_vertex_elements_state;
   rb_pipe->base.bind_vertex_elements_state = rbug_bind_vertex_elements_state;
   rb_pipe->base.delete_vertex_elements_state = rbug_delete_vertex_elements_state;
   rb_pipe->base.set_blend_color = rbug_set_blend_color;
   rb_pipe->base.set_stencil_ref = rbug_set_stencil_ref;
   rb_pipe->base.set_clip_state = rbug_set_clip_state;
   rb_pipe->base.set_constant_buffer = rbug_set_constant_buffer;
   rb_pipe->base.set_framebuffer_state = rbug_set_framebuffer_state;
   rb_pipe->base.set_polygon_stipple = rbug_set_polygon_stipple;
   rb_pipe->base.set_scissor_states = rbug_set_scissor_states;
   rb_pipe->base.set_viewport_states = rbug_set_viewport_states;
   rb_pipe->base.set_sampler_views = rbug_set_sampler_views;
   rb_pipe->base.set_vertex_buffers = rbug_set_vertex_buffers;
   rb_pipe->base.set_sample_mask = rbug_set_sample_mask;
   rb_pipe->base.create_stream_output_target = rbug_create_stream_output_target;
   rb_pipe->base.stream_output_target_destroy = rbug_stream_output_target_destroy;
   rb_pipe->base.set_stream_output_targets = rbug_set_stream_output_targets;
   rb_pipe->base.resource_copy_region = rbug_resource_copy_region;
   rb_pipe->base.blit = rbug_blit;
   rb_pipe->base.flush_resource = rbug_flush_resource;
   rb_pipe->base.clear = rbug_clear;
   rb_pipe->base.clear_render_target = rbug_clear_render_target;
   rb_pipe->base.clear_depth_stencil = rbug_clear_depth_stencil;
   rb_pipe->base.flush = rbug_flush;
   rb_pipe->base.create_sampler_view = rbug_context_create_sampler_view;
   rb_pipe->base.sampler_view_destroy = rbug_context_sampler_view_destroy;
   rb_pipe->base.create_surface = rbug_context_create_surface;
   rb_pipe->base.surface_destroy = rbug_context_surface_destroy;
   rb_pipe->base.transfer_map = rbug_context_transfer_map;
   rb_pipe->base.transfer_unmap = rbug_context_transfer_unmap;
   rb_pipe->base.transfer_flush_region = rbug_context_transfer_flush_region;
   rb_pipe->base.buffer_subdata = rbug_context_buffer_subdata;
   rb_pipe->base.texture_subdata = rbug_context_texture_subdata;

   rb_pipe->pipe = pipe;

   rbug_screen_add_to_list(rb_screen, contexts, rb_pipe);

   if (debug_get_bool_option("GALLIUM_RBUG_START_BLOCKED", FALSE)) {
      rb_pipe->draw_blocked = RBUG_BLOCK_BEFORE;
   }

   return &rb_pipe->base;
}
