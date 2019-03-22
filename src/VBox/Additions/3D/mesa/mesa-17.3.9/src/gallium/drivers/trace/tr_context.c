/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
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

#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/simple_list.h"

#include "pipe/p_format.h"
#include "pipe/p_screen.h"

#include "tr_dump.h"
#include "tr_dump_defines.h"
#include "tr_dump_state.h"
#include "tr_public.h"
#include "tr_screen.h"
#include "tr_texture.h"
#include "tr_context.h"


struct trace_query
{
   unsigned type;

   struct pipe_query *query;
};


static inline struct trace_query *
trace_query(struct pipe_query *query)
{
   return (struct trace_query *)query;
}


static inline struct pipe_query *
trace_query_unwrap(struct pipe_query *query)
{
   if (query) {
      return trace_query(query)->query;
   } else {
      return NULL;
   }
}


static inline struct pipe_surface *
trace_surface_unwrap(struct trace_context *tr_ctx,
                     struct pipe_surface *surface)
{
   struct trace_surface *tr_surf;

   if (!surface)
      return NULL;

   assert(surface->texture);
   if (!surface->texture)
      return surface;

   tr_surf = trace_surface(surface);

   assert(tr_surf->surface);
   return tr_surf->surface;
}


static void
trace_context_draw_vbo(struct pipe_context *_pipe,
                       const struct pipe_draw_info *info)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "draw_vbo");

   trace_dump_arg(ptr,  pipe);
   trace_dump_arg(draw_info, info);

   trace_dump_trace_flush();

   pipe->draw_vbo(pipe, info);

   trace_dump_call_end();
}


static struct pipe_query *
trace_context_create_query(struct pipe_context *_pipe,
                           unsigned query_type,
                           unsigned index)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_query *query;

   trace_dump_call_begin("pipe_context", "create_query");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(query_type, query_type);
   trace_dump_arg(int, index);

   query = pipe->create_query(pipe, query_type, index);

   trace_dump_ret(ptr, query);

   trace_dump_call_end();

   /* Wrap query object. */
   if (query) {
      struct trace_query *tr_query = CALLOC_STRUCT(trace_query);
      if (tr_query) {
         tr_query->type = query_type;
         tr_query->query = query;
         query = (struct pipe_query *)tr_query;
      } else {
         pipe->destroy_query(pipe, query);
         query = NULL;
      }
   }

   return query;
}


static void
trace_context_destroy_query(struct pipe_context *_pipe,
                            struct pipe_query *_query)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct trace_query *tr_query = trace_query(_query);
   struct pipe_query *query = tr_query->query;

   FREE(tr_query);

   trace_dump_call_begin("pipe_context", "destroy_query");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, query);

   pipe->destroy_query(pipe, query);

   trace_dump_call_end();
}


static boolean
trace_context_begin_query(struct pipe_context *_pipe,
                          struct pipe_query *query)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   boolean ret;

   query = trace_query_unwrap(query);

   trace_dump_call_begin("pipe_context", "begin_query");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, query);

   ret = pipe->begin_query(pipe, query);

   trace_dump_call_end();
   return ret;
}


static bool
trace_context_end_query(struct pipe_context *_pipe,
                        struct pipe_query *query)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   bool ret;

   query = trace_query_unwrap(query);

   trace_dump_call_begin("pipe_context", "end_query");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, query);

   ret = pipe->end_query(pipe, query);

   trace_dump_call_end();
   return ret;
}


static boolean
trace_context_get_query_result(struct pipe_context *_pipe,
                               struct pipe_query *_query,
                               boolean wait,
                               union pipe_query_result *result)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct trace_query *tr_query = trace_query(_query);
   struct pipe_query *query = tr_query->query;
   boolean ret;

   trace_dump_call_begin("pipe_context", "get_query_result");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, query);

   ret = pipe->get_query_result(pipe, query, wait, result);

   trace_dump_arg_begin("result");
   if (ret) {
      trace_dump_query_result(tr_query->type, result);
   } else {
      trace_dump_null();
   }
   trace_dump_arg_end();

   trace_dump_ret(bool, ret);

   trace_dump_call_end();

   return ret;
}


static void
trace_context_set_active_query_state(struct pipe_context *_pipe,
                                     boolean enable)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_active_query_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(bool, enable);

   pipe->set_active_query_state(pipe, enable);

   trace_dump_call_end();
}


static void *
trace_context_create_blend_state(struct pipe_context *_pipe,
                                 const struct pipe_blend_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_blend_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(blend_state, state);

   result = pipe->create_blend_state(pipe, state);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_bind_blend_state(struct pipe_context *_pipe,
                               void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "bind_blend_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->bind_blend_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_delete_blend_state(struct pipe_context *_pipe,
                                 void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_blend_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->delete_blend_state(pipe, state);

   trace_dump_call_end();
}


static void *
trace_context_create_sampler_state(struct pipe_context *_pipe,
                                   const struct pipe_sampler_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_sampler_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(sampler_state, state);

   result = pipe->create_sampler_state(pipe, state);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_bind_sampler_states(struct pipe_context *_pipe,
                                  enum pipe_shader_type shader,
                                  unsigned start,
                                  unsigned num_states,
                                  void **states)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   /* remove this when we have pipe->bind_sampler_states(..., start, ...) */
   assert(start == 0);

   trace_dump_call_begin("pipe_context", "bind_sampler_states");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, shader);
   trace_dump_arg(uint, start);
   trace_dump_arg(uint, num_states);
   trace_dump_arg_array(ptr, states, num_states);

   pipe->bind_sampler_states(pipe, shader, start, num_states, states);

   trace_dump_call_end();
}


static void
trace_context_delete_sampler_state(struct pipe_context *_pipe,
                                   void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_sampler_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->delete_sampler_state(pipe, state);

   trace_dump_call_end();
}


static void *
trace_context_create_rasterizer_state(struct pipe_context *_pipe,
                                      const struct pipe_rasterizer_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_rasterizer_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(rasterizer_state, state);

   result = pipe->create_rasterizer_state(pipe, state);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_bind_rasterizer_state(struct pipe_context *_pipe,
                                    void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "bind_rasterizer_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->bind_rasterizer_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_delete_rasterizer_state(struct pipe_context *_pipe,
                                      void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_rasterizer_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->delete_rasterizer_state(pipe, state);

   trace_dump_call_end();
}


static void *
trace_context_create_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                               const struct pipe_depth_stencil_alpha_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_depth_stencil_alpha_state");

   result = pipe->create_depth_stencil_alpha_state(pipe, state);

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(depth_stencil_alpha_state, state);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_bind_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                             void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "bind_depth_stencil_alpha_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->bind_depth_stencil_alpha_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_delete_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                               void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_depth_stencil_alpha_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->delete_depth_stencil_alpha_state(pipe, state);

   trace_dump_call_end();
}


#define TRACE_SHADER_STATE(shader_type) \
   static void * \
   trace_context_create_##shader_type##_state(struct pipe_context *_pipe, \
                                 const struct pipe_shader_state *state) \
   { \
      struct trace_context *tr_ctx = trace_context(_pipe); \
      struct pipe_context *pipe = tr_ctx->pipe; \
      void * result; \
      trace_dump_call_begin("pipe_context", "create_" #shader_type "_state"); \
      trace_dump_arg(ptr, pipe); \
      trace_dump_arg(shader_state, state); \
      result = pipe->create_##shader_type##_state(pipe, state); \
      trace_dump_ret(ptr, result); \
      trace_dump_call_end(); \
      return result; \
   } \
    \
   static void \
   trace_context_bind_##shader_type##_state(struct pipe_context *_pipe, \
                               void *state) \
   { \
      struct trace_context *tr_ctx = trace_context(_pipe); \
      struct pipe_context *pipe = tr_ctx->pipe; \
      trace_dump_call_begin("pipe_context", "bind_" #shader_type "_state"); \
      trace_dump_arg(ptr, pipe); \
      trace_dump_arg(ptr, state); \
      pipe->bind_##shader_type##_state(pipe, state); \
      trace_dump_call_end(); \
   } \
    \
   static void \
   trace_context_delete_##shader_type##_state(struct pipe_context *_pipe, \
                                 void *state) \
   { \
      struct trace_context *tr_ctx = trace_context(_pipe); \
      struct pipe_context *pipe = tr_ctx->pipe; \
      trace_dump_call_begin("pipe_context", "delete_" #shader_type "_state"); \
      trace_dump_arg(ptr, pipe); \
      trace_dump_arg(ptr, state); \
      pipe->delete_##shader_type##_state(pipe, state); \
      trace_dump_call_end(); \
   }

TRACE_SHADER_STATE(fs)
TRACE_SHADER_STATE(vs)
TRACE_SHADER_STATE(gs)
TRACE_SHADER_STATE(tcs)
TRACE_SHADER_STATE(tes)

#undef TRACE_SHADER_STATE


static inline void *
trace_context_create_compute_state(struct pipe_context *_pipe,
                                   const struct pipe_compute_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_compute_state");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(compute_state, state);
   result = pipe->create_compute_state(pipe, state);
   trace_dump_ret(ptr, result);
   trace_dump_call_end();
   return result;
}

static inline void
trace_context_bind_compute_state(struct pipe_context *_pipe,
                                 void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "bind_compute_state");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);
   pipe->bind_compute_state(pipe, state);
   trace_dump_call_end();
}

static inline void
trace_context_delete_compute_state(struct pipe_context *_pipe,
                                   void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_compute_state");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);
   pipe->delete_compute_state(pipe, state);
   trace_dump_call_end();
}

static void *
trace_context_create_vertex_elements_state(struct pipe_context *_pipe,
                                           unsigned num_elements,
                                           const struct  pipe_vertex_element *elements)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   void * result;

   trace_dump_call_begin("pipe_context", "create_vertex_elements_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, num_elements);

   trace_dump_arg_begin("elements");
   trace_dump_struct_array(vertex_element, elements, num_elements);
   trace_dump_arg_end();

   result = pipe->create_vertex_elements_state(pipe, num_elements, elements);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_bind_vertex_elements_state(struct pipe_context *_pipe,
                                         void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "bind_vertex_elements_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->bind_vertex_elements_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_delete_vertex_elements_state(struct pipe_context *_pipe,
                                           void *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_vertex_elements_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, state);

   pipe->delete_vertex_elements_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_set_blend_color(struct pipe_context *_pipe,
                              const struct pipe_blend_color *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_blend_color");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(blend_color, state);

   pipe->set_blend_color(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_set_stencil_ref(struct pipe_context *_pipe,
                              const struct pipe_stencil_ref *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_stencil_ref");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(stencil_ref, state);

   pipe->set_stencil_ref(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_set_clip_state(struct pipe_context *_pipe,
                             const struct pipe_clip_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_clip_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(clip_state, state);

   pipe->set_clip_state(pipe, state);

   trace_dump_call_end();
}

static void
trace_context_set_sample_mask(struct pipe_context *_pipe,
                              unsigned sample_mask)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_sample_mask");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, sample_mask);

   pipe->set_sample_mask(pipe, sample_mask);

   trace_dump_call_end();
}

static void
trace_context_set_constant_buffer(struct pipe_context *_pipe,
                                  enum pipe_shader_type shader, uint index,
                                  const struct pipe_constant_buffer *constant_buffer)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_constant_buffer");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, shader);
   trace_dump_arg(uint, index);
   trace_dump_arg(constant_buffer, constant_buffer);

   pipe->set_constant_buffer(pipe, shader, index, constant_buffer);

   trace_dump_call_end();
}


static void
trace_context_set_framebuffer_state(struct pipe_context *_pipe,
                                    const struct pipe_framebuffer_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_framebuffer_state unwrapped_state;
   unsigned i;


   /* Unwrap the input state */
   memcpy(&unwrapped_state, state, sizeof(unwrapped_state));
   for (i = 0; i < state->nr_cbufs; ++i)
      unwrapped_state.cbufs[i] = trace_surface_unwrap(tr_ctx, state->cbufs[i]);
   for (i = state->nr_cbufs; i < PIPE_MAX_COLOR_BUFS; ++i)
      unwrapped_state.cbufs[i] = NULL;
   unwrapped_state.zsbuf = trace_surface_unwrap(tr_ctx, state->zsbuf);
   state = &unwrapped_state;

   trace_dump_call_begin("pipe_context", "set_framebuffer_state");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(framebuffer_state, state);

   pipe->set_framebuffer_state(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_set_polygon_stipple(struct pipe_context *_pipe,
                                  const struct pipe_poly_stipple *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_polygon_stipple");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(poly_stipple, state);

   pipe->set_polygon_stipple(pipe, state);

   trace_dump_call_end();
}


static void
trace_context_set_scissor_states(struct pipe_context *_pipe,
                                 unsigned start_slot,
                                 unsigned num_scissors,
                                 const struct pipe_scissor_state *states)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_scissor_states");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, start_slot);
   trace_dump_arg(uint, num_scissors);
   trace_dump_arg(scissor_state, states);

   pipe->set_scissor_states(pipe, start_slot, num_scissors, states);

   trace_dump_call_end();
}


static void
trace_context_set_viewport_states(struct pipe_context *_pipe,
                                  unsigned start_slot,
                                  unsigned num_viewports,
                                  const struct pipe_viewport_state *states)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_viewport_states");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, start_slot);
   trace_dump_arg(uint, num_viewports);
   trace_dump_arg(viewport_state, states);

   pipe->set_viewport_states(pipe, start_slot, num_viewports, states);

   trace_dump_call_end();
}


static struct pipe_sampler_view *
trace_context_create_sampler_view(struct pipe_context *_pipe,
                                  struct pipe_resource *resource,
                                  const struct pipe_sampler_view *templ)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_sampler_view *result;
   struct trace_sampler_view *tr_view;

   trace_dump_call_begin("pipe_context", "create_sampler_view");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, resource);

   trace_dump_arg_begin("templ");
   trace_dump_sampler_view_template(templ, resource->target);
   trace_dump_arg_end();

   result = pipe->create_sampler_view(pipe, resource, templ);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   /*
    * Wrap pipe_sampler_view
    */
   tr_view = CALLOC_STRUCT(trace_sampler_view);
   tr_view->base = *templ;
   tr_view->base.reference.count = 1;
   tr_view->base.texture = NULL;
   pipe_resource_reference(&tr_view->base.texture, resource);
   tr_view->base.context = _pipe;
   tr_view->sampler_view = result;
   result = &tr_view->base;

   return result;
}


static void
trace_context_sampler_view_destroy(struct pipe_context *_pipe,
                                   struct pipe_sampler_view *_view)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct trace_sampler_view *tr_view = trace_sampler_view(_view);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_sampler_view *view = tr_view->sampler_view;

   assert(_view->context == _pipe);

   trace_dump_call_begin("pipe_context", "sampler_view_destroy");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, view);

   pipe_sampler_view_reference(&tr_view->sampler_view, NULL);

   trace_dump_call_end();

   pipe_resource_reference(&_view->texture, NULL);
   FREE(_view);
}

/********************************************************************
 * surface
 */


static struct pipe_surface *
trace_context_create_surface(struct pipe_context *_pipe,
                             struct pipe_resource *resource,
                             const struct pipe_surface *surf_tmpl)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_surface *result = NULL;

   trace_dump_call_begin("pipe_context", "create_surface");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, resource);

   trace_dump_arg_begin("surf_tmpl");
   trace_dump_surface_template(surf_tmpl, resource->target);
   trace_dump_arg_end();


   result = pipe->create_surface(pipe, resource, surf_tmpl);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   result = trace_surf_create(tr_ctx, resource, result);

   return result;
}


static void
trace_context_surface_destroy(struct pipe_context *_pipe,
                              struct pipe_surface *_surface)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct trace_surface *tr_surf = trace_surface(_surface);
   struct pipe_surface *surface = tr_surf->surface;

   trace_dump_call_begin("pipe_context", "surface_destroy");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, surface);

   trace_dump_call_end();

   trace_surf_destroy(tr_surf);
}


static void
trace_context_set_sampler_views(struct pipe_context *_pipe,
                                enum pipe_shader_type shader,
                                unsigned start,
                                unsigned num,
                                struct pipe_sampler_view **views)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct trace_sampler_view *tr_view;
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_sampler_view *unwrapped_views[PIPE_MAX_SHADER_SAMPLER_VIEWS];
   unsigned i;

   /* remove this when we have pipe->set_sampler_views(..., start, ...) */
   assert(start == 0);

   for (i = 0; i < num; ++i) {
      tr_view = trace_sampler_view(views[i]);
      unwrapped_views[i] = tr_view ? tr_view->sampler_view : NULL;
   }
   views = unwrapped_views;

   trace_dump_call_begin("pipe_context", "set_sampler_views");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, shader);
   trace_dump_arg(uint, start);
   trace_dump_arg(uint, num);
   trace_dump_arg_array(ptr, views, num);

   pipe->set_sampler_views(pipe, shader, start, num, views);

   trace_dump_call_end();
}


static void
trace_context_set_vertex_buffers(struct pipe_context *_pipe,
                                 unsigned start_slot, unsigned num_buffers,
                                 const struct pipe_vertex_buffer *buffers)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_vertex_buffers");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, start_slot);
   trace_dump_arg(uint, num_buffers);

   trace_dump_arg_begin("buffers");
   trace_dump_struct_array(vertex_buffer, buffers, num_buffers);
   trace_dump_arg_end();

   pipe->set_vertex_buffers(pipe, start_slot, num_buffers, buffers);

   trace_dump_call_end();
}


static struct pipe_stream_output_target *
trace_context_create_stream_output_target(struct pipe_context *_pipe,
                                          struct pipe_resource *res,
                                          unsigned buffer_offset,
                                          unsigned buffer_size)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_stream_output_target *result;

   trace_dump_call_begin("pipe_context", "create_stream_output_target");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, res);
   trace_dump_arg(uint, buffer_offset);
   trace_dump_arg(uint, buffer_size);

   result = pipe->create_stream_output_target(pipe,
                                              res, buffer_offset, buffer_size);

   trace_dump_ret(ptr, result);

   trace_dump_call_end();

   return result;
}


static void
trace_context_stream_output_target_destroy(
   struct pipe_context *_pipe,
   struct pipe_stream_output_target *target)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "stream_output_target_destroy");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, target);

   pipe->stream_output_target_destroy(pipe, target);

   trace_dump_call_end();
}


static void
trace_context_set_stream_output_targets(struct pipe_context *_pipe,
                                        unsigned num_targets,
                                        struct pipe_stream_output_target **tgs,
                                        const unsigned *offsets)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "set_stream_output_targets");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, num_targets);
   trace_dump_arg_array(ptr, tgs, num_targets);
   trace_dump_arg_array(uint, offsets, num_targets);

   pipe->set_stream_output_targets(pipe, num_targets, tgs, offsets);

   trace_dump_call_end();
}


static void
trace_context_resource_copy_region(struct pipe_context *_pipe,
                                   struct pipe_resource *dst,
                                   unsigned dst_level,
                                   unsigned dstx, unsigned dsty, unsigned dstz,
                                   struct pipe_resource *src,
                                   unsigned src_level,
                                   const struct pipe_box *src_box)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "resource_copy_region");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, dst);
   trace_dump_arg(uint, dst_level);
   trace_dump_arg(uint, dstx);
   trace_dump_arg(uint, dsty);
   trace_dump_arg(uint, dstz);
   trace_dump_arg(ptr, src);
   trace_dump_arg(uint, src_level);
   trace_dump_arg(box, src_box);

   pipe->resource_copy_region(pipe,
                              dst, dst_level, dstx, dsty, dstz,
                              src, src_level, src_box);

   trace_dump_call_end();
}


static void
trace_context_blit(struct pipe_context *_pipe,
                   const struct pipe_blit_info *_info)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   struct pipe_blit_info info = *_info;

   trace_dump_call_begin("pipe_context", "blit");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(blit_info, _info);

   pipe->blit(pipe, &info);

   trace_dump_call_end();
}


static void
trace_context_flush_resource(struct pipe_context *_pipe,
                             struct pipe_resource *resource)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "flush_resource");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, resource);

   pipe->flush_resource(pipe, resource);

   trace_dump_call_end();
}


static void
trace_context_clear(struct pipe_context *_pipe,
                    unsigned buffers,
                    const union pipe_color_union *color,
                    double depth,
                    unsigned stencil)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "clear");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, buffers);
   trace_dump_arg_begin("color");
   if (color)
      trace_dump_array(float, color->f, 4);
   else
      trace_dump_null();
   trace_dump_arg_end();
   trace_dump_arg(float, depth);
   trace_dump_arg(uint, stencil);

   pipe->clear(pipe, buffers, color, depth, stencil);

   trace_dump_call_end();
}


static void
trace_context_clear_render_target(struct pipe_context *_pipe,
                                  struct pipe_surface *dst,
                                  const union pipe_color_union *color,
                                  unsigned dstx, unsigned dsty,
                                  unsigned width, unsigned height,
                                  bool render_condition_enabled)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   dst = trace_surface_unwrap(tr_ctx, dst);

   trace_dump_call_begin("pipe_context", "clear_render_target");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, dst);
   trace_dump_arg_array(float, color->f, 4);
   trace_dump_arg(uint, dstx);
   trace_dump_arg(uint, dsty);
   trace_dump_arg(uint, width);
   trace_dump_arg(uint, height);
   trace_dump_arg(bool, render_condition_enabled);

   pipe->clear_render_target(pipe, dst, color, dstx, dsty, width, height,
                             render_condition_enabled);

   trace_dump_call_end();
}

static void
trace_context_clear_depth_stencil(struct pipe_context *_pipe,
                                  struct pipe_surface *dst,
                                  unsigned clear_flags,
                                  double depth,
                                  unsigned stencil,
                                  unsigned dstx, unsigned dsty,
                                  unsigned width, unsigned height,
                                  bool render_condition_enabled)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   dst = trace_surface_unwrap(tr_ctx, dst);

   trace_dump_call_begin("pipe_context", "clear_depth_stencil");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, dst);
   trace_dump_arg(uint, clear_flags);
   trace_dump_arg(float, depth);
   trace_dump_arg(uint, stencil);
   trace_dump_arg(uint, dstx);
   trace_dump_arg(uint, dsty);
   trace_dump_arg(uint, width);
   trace_dump_arg(uint, height);
   trace_dump_arg(bool, render_condition_enabled);

   pipe->clear_depth_stencil(pipe, dst, clear_flags, depth, stencil,
                             dstx, dsty, width, height,
                             render_condition_enabled);

   trace_dump_call_end();
}

static inline void
trace_context_clear_texture(struct pipe_context *_pipe,
                            struct pipe_resource *res,
                            unsigned level,
                            const struct pipe_box *box,
                            const void *data)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;


   trace_dump_call_begin("pipe_context", "clear_texture");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, res);
   trace_dump_arg(uint, level);
   trace_dump_arg_begin("box");
   trace_dump_box(box);
   trace_dump_arg_end();
   trace_dump_arg(ptr, data);

   pipe->clear_texture(pipe, res, level, box, data);

   trace_dump_call_end();
}

static void
trace_context_flush(struct pipe_context *_pipe,
                    struct pipe_fence_handle **fence,
                    unsigned flags)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "flush");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, flags);

   pipe->flush(pipe, fence, flags);

   if (fence)
      trace_dump_ret(ptr, *fence);

   trace_dump_call_end();
}


static inline boolean
trace_context_generate_mipmap(struct pipe_context *_pipe,
                              struct pipe_resource *res,
                              enum pipe_format format,
                              unsigned base_level,
                              unsigned last_level,
                              unsigned first_layer,
                              unsigned last_layer)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   boolean ret;

   trace_dump_call_begin("pipe_context", "generate_mipmap");

   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, res);

   trace_dump_arg(format, format);
   trace_dump_arg(uint, base_level);
   trace_dump_arg(uint, last_level);
   trace_dump_arg(uint, first_layer);
   trace_dump_arg(uint, last_layer);

   ret = pipe->generate_mipmap(pipe, res, format, base_level, last_level,
                               first_layer, last_layer);

   trace_dump_ret(bool, ret);
   trace_dump_call_end();

   return ret;
}


static void
trace_context_destroy(struct pipe_context *_pipe)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "destroy");
   trace_dump_arg(ptr, pipe);
   trace_dump_call_end();

   pipe->destroy(pipe);

   FREE(tr_ctx);
}


/********************************************************************
 * transfer
 */


static void *
trace_context_transfer_map(struct pipe_context *_context,
                           struct pipe_resource *resource,
                           unsigned level,
                           unsigned usage,
                           const struct pipe_box *box,
                           struct pipe_transfer **transfer)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;
   struct pipe_transfer *result = NULL;
   void *map;

   /*
    * Map and transfers can't be serialized so we convert all write transfers
    * to texture/buffer_subdata and ignore read transfers.
    */

   map = context->transfer_map(context, resource, level, usage, box, &result);
   if (!map)
      return NULL;

   *transfer = trace_transfer_create(tr_context, resource, result);

   if (map) {
      if (usage & PIPE_TRANSFER_WRITE) {
         trace_transfer(*transfer)->map = map;
      }
   }

   return *transfer ? map : NULL;
}

static void
trace_context_transfer_flush_region( struct pipe_context *_context,
				     struct pipe_transfer *_transfer,
				     const struct pipe_box *box)
{
   struct trace_context *tr_context = trace_context(_context);
   struct trace_transfer *tr_transfer = trace_transfer(_transfer);
   struct pipe_context *context = tr_context->pipe;
   struct pipe_transfer *transfer = tr_transfer->transfer;

   context->transfer_flush_region(context, transfer, box);
}

static void
trace_context_transfer_unmap(struct pipe_context *_context,
                             struct pipe_transfer *_transfer)
{
   struct trace_context *tr_ctx = trace_context(_context);
   struct trace_transfer *tr_trans = trace_transfer(_transfer);
   struct pipe_context *context = tr_ctx->pipe;
   struct pipe_transfer *transfer = tr_trans->transfer;

   if (tr_trans->map) {
      /*
       * Fake a texture/buffer_subdata
       */

      struct pipe_resource *resource = transfer->resource;
      unsigned level = transfer->level;
      unsigned usage = transfer->usage;
      const struct pipe_box *box = &transfer->box;
      unsigned stride = transfer->stride;
      unsigned layer_stride = transfer->layer_stride;

      if (resource->target == PIPE_BUFFER)
         trace_dump_call_begin("pipe_context", "buffer_subdata");
      else
         trace_dump_call_begin("pipe_context", "texture_subdata");

      trace_dump_arg(ptr, context);
      trace_dump_arg(ptr, resource);
      trace_dump_arg(uint, level);
      trace_dump_arg(uint, usage);
      trace_dump_arg(box, box);

      trace_dump_arg_begin("data");
      trace_dump_box_bytes(tr_trans->map,
                           resource,
                           box,
                           stride,
                           layer_stride);
      trace_dump_arg_end();

      trace_dump_arg(uint, stride);
      trace_dump_arg(uint, layer_stride);

      trace_dump_call_end();

      tr_trans->map = NULL;
   }

   context->transfer_unmap(context, transfer);
   trace_transfer_destroy(tr_ctx, tr_trans);
}


static void
trace_context_buffer_subdata(struct pipe_context *_context,
                             struct pipe_resource *resource,
                             unsigned usage, unsigned offset,
                             unsigned size, const void *data)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;
   struct pipe_box box;

   trace_dump_call_begin("pipe_context", "buffer_subdata");

   trace_dump_arg(ptr, context);
   trace_dump_arg(ptr, resource);
   trace_dump_arg(uint, usage);
   trace_dump_arg(uint, offset);
   trace_dump_arg(uint, size);

   trace_dump_arg_begin("data");
   u_box_1d(offset, size, &box);
   trace_dump_box_bytes(data, resource, &box, 0, 0);
   trace_dump_arg_end();

   trace_dump_call_end();

   context->buffer_subdata(context, resource, usage, offset, size, data);
}


static void
trace_context_texture_subdata(struct pipe_context *_context,
                              struct pipe_resource *resource,
                              unsigned level,
                              unsigned usage,
                              const struct pipe_box *box,
                              const void *data,
                              unsigned stride,
                              unsigned layer_stride)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "texture_subdata");

   trace_dump_arg(ptr, context);
   trace_dump_arg(ptr, resource);
   trace_dump_arg(uint, level);
   trace_dump_arg(uint, usage);
   trace_dump_arg(box, box);

   trace_dump_arg_begin("data");
   trace_dump_box_bytes(data,
                        resource,
                        box,
                        stride,
                        layer_stride);
   trace_dump_arg_end();

   trace_dump_arg(uint, stride);
   trace_dump_arg(uint, layer_stride);

   trace_dump_call_end();

   context->texture_subdata(context, resource, level, usage, box,
                            data, stride, layer_stride);
}

static void
trace_context_invalidate_resource(struct pipe_context *_context,
                                  struct pipe_resource *resource)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "invalidate_resource");

   trace_dump_arg(ptr, context);
   trace_dump_arg(ptr, resource);

   trace_dump_call_end();

   context->invalidate_resource(context, resource);
}

static void
trace_context_render_condition(struct pipe_context *_context,
                               struct pipe_query *query,
                               boolean condition,
                               enum pipe_render_cond_flag mode)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   query = trace_query_unwrap(query);

   trace_dump_call_begin("pipe_context", "render_condition");

   trace_dump_arg(ptr, context);
   trace_dump_arg(ptr, query);
   trace_dump_arg(bool, condition);
   trace_dump_arg(uint, mode);

   trace_dump_call_end();

   context->render_condition(context, query, condition, mode);
}


static void
trace_context_texture_barrier(struct pipe_context *_context, unsigned flags)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "texture_barrier");

   trace_dump_arg(ptr, context);
   trace_dump_arg(uint, flags);

   trace_dump_call_end();

   context->texture_barrier(context, flags);
}


static void
trace_context_memory_barrier(struct pipe_context *_context,
                             unsigned flags)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "memory_barrier");
   trace_dump_arg(ptr, context);
   trace_dump_arg(uint, flags);
   trace_dump_call_end();

   context->memory_barrier(context, flags);
}


static bool
trace_context_resource_commit(struct pipe_context *_context,
                              struct pipe_resource *resource,
                              unsigned level, struct pipe_box *box, bool commit)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "resource_commit");
   trace_dump_arg(ptr, context);
   trace_dump_arg(ptr, resource);
   trace_dump_arg(uint, level);
   trace_dump_arg(box, box);
   trace_dump_arg(bool, commit);
   trace_dump_call_end();

   return context->resource_commit(context, resource, level, box, commit);
}

static void
trace_context_set_tess_state(struct pipe_context *_context,
                             const float default_outer_level[4],
                             const float default_inner_level[2])
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "set_tess_state");
   trace_dump_arg(ptr, context);
   trace_dump_arg_array(float, default_outer_level, 4);
   trace_dump_arg_array(float, default_inner_level, 2);
   trace_dump_call_end();

   context->set_tess_state(context, default_outer_level, default_inner_level);
}


static void trace_context_set_shader_buffers(struct pipe_context *_context,
                                             enum pipe_shader_type shader,
                                             unsigned start, unsigned nr,
                                             const struct pipe_shader_buffer *buffers)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "set_shader_buffers");
   trace_dump_arg(ptr, context);
   trace_dump_arg(uint, shader);
   trace_dump_arg(uint, start);
   trace_dump_arg_begin("buffers");
   trace_dump_struct_array(shader_buffer, buffers, nr);
   trace_dump_arg_end();
   trace_dump_call_end();

   context->set_shader_buffers(context, shader, start, nr, buffers);
}

static void trace_context_set_shader_images(struct pipe_context *_context,
                                            enum pipe_shader_type shader,
                                            unsigned start, unsigned nr,
                                            const struct pipe_image_view *images)
{
   struct trace_context *tr_context = trace_context(_context);
   struct pipe_context *context = tr_context->pipe;

   trace_dump_call_begin("pipe_context", "set_shader_images");
   trace_dump_arg(ptr, context);
   trace_dump_arg(uint, shader);
   trace_dump_arg(uint, start);
   trace_dump_arg_begin("images");
   trace_dump_struct_array(image_view, images, nr);
   trace_dump_arg_end();
   trace_dump_call_end();

   context->set_shader_images(context, shader, start, nr, images);
}

static void trace_context_launch_grid(struct pipe_context *_pipe,
                                      const struct pipe_grid_info *info)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "launch_grid");

   trace_dump_arg(ptr,  pipe);
   trace_dump_arg(grid_info, info);

   trace_dump_trace_flush();

   pipe->launch_grid(pipe, info);

   trace_dump_call_end();
}

static uint64_t trace_context_create_texture_handle(struct pipe_context *_pipe,
                                                    struct pipe_sampler_view *view,
                                                    const struct pipe_sampler_state *state)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   uint64_t handle;

   trace_dump_call_begin("pipe_context", "create_texture_handle");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(ptr, view);
   trace_dump_arg_begin("state");
   trace_dump_arg(sampler_state, state);
   trace_dump_arg_end();

   handle = pipe->create_texture_handle(pipe, view, state);

   trace_dump_ret(uint, handle);
   trace_dump_call_end();

   return handle;
}

static void trace_context_delete_texture_handle(struct pipe_context *_pipe,
                                                uint64_t handle)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_texture_handle");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, handle);
   trace_dump_call_end();

   pipe->delete_texture_handle(pipe, handle);
}

static void trace_context_make_texture_handle_resident(struct pipe_context *_pipe,
                                                       uint64_t handle,
                                                       bool resident)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "make_texture_handle_resident");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, handle);
   trace_dump_arg(bool, resident);
   trace_dump_call_end();

   pipe->make_texture_handle_resident(pipe, handle, resident);
}

static uint64_t trace_context_create_image_handle(struct pipe_context *_pipe,
                                                  const struct pipe_image_view *image)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;
   uint64_t handle;

   trace_dump_call_begin("pipe_context", "create_image_handle");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg_begin("image");
   trace_dump_image_view(image);
   trace_dump_arg_end();

   handle = pipe->create_image_handle(pipe, image);

   trace_dump_ret(uint, handle);
   trace_dump_call_end();

   return handle;
}

static void trace_context_delete_image_handle(struct pipe_context *_pipe,
                                              uint64_t handle)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "delete_image_handle");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, handle);
   trace_dump_call_end();

   pipe->delete_image_handle(pipe, handle);
}

static void trace_context_make_image_handle_resident(struct pipe_context *_pipe,
                                                    uint64_t handle,
                                                    unsigned access,
                                                    bool resident)
{
   struct trace_context *tr_ctx = trace_context(_pipe);
   struct pipe_context *pipe = tr_ctx->pipe;

   trace_dump_call_begin("pipe_context", "make_image_handle_resident");
   trace_dump_arg(ptr, pipe);
   trace_dump_arg(uint, handle);
   trace_dump_arg(uint, access);
   trace_dump_arg(bool, resident);
   trace_dump_call_end();

   pipe->make_image_handle_resident(pipe, handle, access, resident);
}

struct pipe_context *
trace_context_create(struct trace_screen *tr_scr,
                     struct pipe_context *pipe)
{
   struct trace_context *tr_ctx;

   if (!pipe)
      goto error1;

   if (!trace_enabled())
      goto error1;

   tr_ctx = CALLOC_STRUCT(trace_context);
   if (!tr_ctx)
      goto error1;

   tr_ctx->base.priv = pipe->priv; /* expose wrapped priv data */
   tr_ctx->base.screen = &tr_scr->base;
   tr_ctx->base.stream_uploader = pipe->stream_uploader;
   tr_ctx->base.const_uploader = pipe->const_uploader;

   tr_ctx->base.destroy = trace_context_destroy;

#define TR_CTX_INIT(_member) \
   tr_ctx->base . _member = pipe -> _member ? trace_context_ ## _member : NULL

   TR_CTX_INIT(draw_vbo);
   TR_CTX_INIT(render_condition);
   TR_CTX_INIT(create_query);
   TR_CTX_INIT(destroy_query);
   TR_CTX_INIT(begin_query);
   TR_CTX_INIT(end_query);
   TR_CTX_INIT(get_query_result);
   TR_CTX_INIT(set_active_query_state);
   TR_CTX_INIT(create_blend_state);
   TR_CTX_INIT(bind_blend_state);
   TR_CTX_INIT(delete_blend_state);
   TR_CTX_INIT(create_sampler_state);
   TR_CTX_INIT(bind_sampler_states);
   TR_CTX_INIT(delete_sampler_state);
   TR_CTX_INIT(create_rasterizer_state);
   TR_CTX_INIT(bind_rasterizer_state);
   TR_CTX_INIT(delete_rasterizer_state);
   TR_CTX_INIT(create_depth_stencil_alpha_state);
   TR_CTX_INIT(bind_depth_stencil_alpha_state);
   TR_CTX_INIT(delete_depth_stencil_alpha_state);
   TR_CTX_INIT(create_fs_state);
   TR_CTX_INIT(bind_fs_state);
   TR_CTX_INIT(delete_fs_state);
   TR_CTX_INIT(create_vs_state);
   TR_CTX_INIT(bind_vs_state);
   TR_CTX_INIT(delete_vs_state);
   TR_CTX_INIT(create_gs_state);
   TR_CTX_INIT(bind_gs_state);
   TR_CTX_INIT(delete_gs_state);
   TR_CTX_INIT(create_tcs_state);
   TR_CTX_INIT(bind_tcs_state);
   TR_CTX_INIT(delete_tcs_state);
   TR_CTX_INIT(create_tes_state);
   TR_CTX_INIT(bind_tes_state);
   TR_CTX_INIT(delete_tes_state);
   TR_CTX_INIT(create_compute_state);
   TR_CTX_INIT(bind_compute_state);
   TR_CTX_INIT(delete_compute_state);
   TR_CTX_INIT(create_vertex_elements_state);
   TR_CTX_INIT(bind_vertex_elements_state);
   TR_CTX_INIT(delete_vertex_elements_state);
   TR_CTX_INIT(set_blend_color);
   TR_CTX_INIT(set_stencil_ref);
   TR_CTX_INIT(set_clip_state);
   TR_CTX_INIT(set_sample_mask);
   TR_CTX_INIT(set_constant_buffer);
   TR_CTX_INIT(set_framebuffer_state);
   TR_CTX_INIT(set_polygon_stipple);
   TR_CTX_INIT(set_scissor_states);
   TR_CTX_INIT(set_viewport_states);
   TR_CTX_INIT(set_sampler_views);
   TR_CTX_INIT(create_sampler_view);
   TR_CTX_INIT(sampler_view_destroy);
   TR_CTX_INIT(create_surface);
   TR_CTX_INIT(surface_destroy);
   TR_CTX_INIT(set_vertex_buffers);
   TR_CTX_INIT(create_stream_output_target);
   TR_CTX_INIT(stream_output_target_destroy);
   TR_CTX_INIT(set_stream_output_targets);
   TR_CTX_INIT(resource_copy_region);
   TR_CTX_INIT(blit);
   TR_CTX_INIT(flush_resource);
   TR_CTX_INIT(clear);
   TR_CTX_INIT(clear_render_target);
   TR_CTX_INIT(clear_depth_stencil);
   TR_CTX_INIT(clear_texture);
   TR_CTX_INIT(flush);
   TR_CTX_INIT(generate_mipmap);
   TR_CTX_INIT(texture_barrier);
   TR_CTX_INIT(memory_barrier);
   TR_CTX_INIT(resource_commit);
   TR_CTX_INIT(set_tess_state);
   TR_CTX_INIT(set_shader_buffers);
   TR_CTX_INIT(launch_grid);
   TR_CTX_INIT(set_shader_images);
   TR_CTX_INIT(create_texture_handle);
   TR_CTX_INIT(delete_texture_handle);
   TR_CTX_INIT(make_texture_handle_resident);
   TR_CTX_INIT(create_image_handle);
   TR_CTX_INIT(delete_image_handle);
   TR_CTX_INIT(make_image_handle_resident);

   TR_CTX_INIT(transfer_map);
   TR_CTX_INIT(transfer_unmap);
   TR_CTX_INIT(transfer_flush_region);
   TR_CTX_INIT(buffer_subdata);
   TR_CTX_INIT(texture_subdata);
   TR_CTX_INIT(invalidate_resource);

#undef TR_CTX_INIT

   tr_ctx->pipe = pipe;

   return &tr_ctx->base;

error1:
   return pipe;
}


/**
 * Sanity checker: check that the given context really is a
 * trace context (and not the wrapped driver's context).
 */
void
trace_context_check(const struct pipe_context *pipe)
{
   MAYBE_UNUSED struct trace_context *tr_ctx = (struct trace_context *) pipe;
   assert(tr_ctx->base.destroy == trace_context_destroy);
}
