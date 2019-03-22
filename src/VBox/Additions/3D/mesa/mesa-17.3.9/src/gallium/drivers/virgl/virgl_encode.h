/*
 * Copyright 2014, 2015 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef VIRGL_ENCODE_H
#define VIRGL_ENCODE_H

#include "pipe/p_defines.h"
#include "pipe/p_state.h"

#include "virgl_winsys.h"

struct tgsi_token;

struct virgl_context;
struct virgl_resource;
struct virgl_sampler_view;

struct virgl_surface {
   struct pipe_surface base;
   uint32_t handle;
};

struct virgl_indexbuf {
   unsigned offset;
   unsigned index_size;  /**< size of an index, in bytes */
   struct pipe_resource *buffer; /**< the actual buffer */
   const void *user_buffer;  /**< pointer to a user buffer if buffer == NULL */
};

static inline struct virgl_surface *virgl_surface(struct pipe_surface *surf)
{
   return (struct virgl_surface *)surf;
}

static inline void virgl_encoder_write_dword(struct virgl_cmd_buf *state,
                                            uint32_t dword)
{
   state->buf[state->cdw++] = dword;
}

static inline void virgl_encoder_write_qword(struct virgl_cmd_buf *state,
                                            uint64_t qword)
{
   memcpy(state->buf + state->cdw, &qword, sizeof(uint64_t));
   state->cdw += 2;
}

static inline void virgl_encoder_write_block(struct virgl_cmd_buf *state,
                                            const uint8_t *ptr, uint32_t len)
{
   int x;
   memcpy(state->buf + state->cdw, ptr, len);
   x = (len % 4);
//   fprintf(stderr, "[%d] block %d x is %d\n", state->cdw, len, x);
   if (x) {
      uint8_t *mp = (uint8_t *)(state->buf + state->cdw);
      mp += len;
      memset(mp, 0, x);
   }
   state->cdw += (len + 3) / 4;
}

extern int virgl_encode_blend_state(struct virgl_context *ctx,
                                   uint32_t handle,
                                   const struct pipe_blend_state *blend_state);
extern int virgl_encode_rasterizer_state(struct virgl_context *ctx,
                                         uint32_t handle,
                                         const struct pipe_rasterizer_state *state);

extern int virgl_encode_shader_state(struct virgl_context *ctx,
                                     uint32_t handle,
                                     uint32_t type,
                                     const struct pipe_stream_output_info *so_info,
                                     const struct tgsi_token *tokens);

int virgl_encode_stream_output_info(struct virgl_context *ctx,
                                   uint32_t handle,
                                   uint32_t type,
                                   const struct pipe_shader_state *shader);

int virgl_encoder_set_so_targets(struct virgl_context *ctx,
                                unsigned num_targets,
                                struct pipe_stream_output_target **targets,
                                unsigned append_bitmask);

int virgl_encoder_create_so_target(struct virgl_context *ctx,
                                  uint32_t handle,
                                  struct virgl_resource *res,
                                  unsigned buffer_offset,
                                  unsigned buffer_size);

int virgl_encode_clear(struct virgl_context *ctx,
                      unsigned buffers,
                      const union pipe_color_union *color,
                      double depth, unsigned stencil);

int virgl_encode_bind_object(struct virgl_context *ctx,
                            uint32_t handle, uint32_t object);
int virgl_encode_delete_object(struct virgl_context *ctx,
                              uint32_t handle, uint32_t object);

int virgl_encoder_set_framebuffer_state(struct virgl_context *ctx,
                                       const struct pipe_framebuffer_state *state);
int virgl_encoder_set_viewport_states(struct virgl_context *ctx,
                                      int start_slot,
                                      int num_viewports,
                                      const struct pipe_viewport_state *states);

int virgl_encoder_draw_vbo(struct virgl_context *ctx,
                          const struct pipe_draw_info *info);


int virgl_encoder_create_surface(struct virgl_context *ctx,
                                uint32_t handle,
                                struct virgl_resource *res,
                                const struct pipe_surface *templat);

int virgl_encoder_flush_frontbuffer(struct virgl_context *ctx,
                                   struct virgl_resource *res);

int virgl_encoder_create_vertex_elements(struct virgl_context *ctx,
                                        uint32_t handle,
                                        unsigned num_elements,
                                        const struct pipe_vertex_element *element);

int virgl_encoder_set_vertex_buffers(struct virgl_context *ctx,
                                    unsigned num_buffers,
                                    const struct pipe_vertex_buffer *buffers);


int virgl_encoder_inline_write(struct virgl_context *ctx,
                              struct virgl_resource *res,
                              unsigned level, unsigned usage,
                              const struct pipe_box *box,
                              const void *data, unsigned stride,
                              unsigned layer_stride);
int virgl_encode_sampler_state(struct virgl_context *ctx,
                              uint32_t handle,
                              const struct pipe_sampler_state *state);
int virgl_encode_sampler_view(struct virgl_context *ctx,
                             uint32_t handle,
                             struct virgl_resource *res,
                             const struct pipe_sampler_view *state);

int virgl_encode_set_sampler_views(struct virgl_context *ctx,
                                  uint32_t shader_type,
                                  uint32_t start_slot,
                                  uint32_t num_views,
                                  struct virgl_sampler_view **views);

int virgl_encode_bind_sampler_states(struct virgl_context *ctx,
                                    uint32_t shader_type,
                                    uint32_t start_slot,
                                    uint32_t num_handles,
                                    uint32_t *handles);

int virgl_encoder_set_index_buffer(struct virgl_context *ctx,
                                  const struct virgl_indexbuf *ib);

uint32_t virgl_object_assign_handle(void);

int virgl_encoder_write_constant_buffer(struct virgl_context *ctx,
                                       uint32_t shader,
                                       uint32_t index,
                                       uint32_t size,
                                       const void *data);

int virgl_encoder_set_uniform_buffer(struct virgl_context *ctx,
                                     uint32_t shader,
                                     uint32_t index,
                                     uint32_t offset,
                                     uint32_t length,
                                     struct virgl_resource *res);
int virgl_encode_dsa_state(struct virgl_context *ctx,
                          uint32_t handle,
                          const struct pipe_depth_stencil_alpha_state *dsa_state);

int virgl_encoder_set_stencil_ref(struct virgl_context *ctx,
                                 const struct pipe_stencil_ref *ref);

int virgl_encoder_set_blend_color(struct virgl_context *ctx,
                                 const struct pipe_blend_color *color);

int virgl_encoder_set_scissor_state(struct virgl_context *ctx,
                                    unsigned start_slot,
                                    int num_scissors,
                                    const struct pipe_scissor_state *ss);

void virgl_encoder_set_polygon_stipple(struct virgl_context *ctx,
                                      const struct pipe_poly_stipple *ps);

void virgl_encoder_set_sample_mask(struct virgl_context *ctx,
                                  unsigned sample_mask);

void virgl_encoder_set_clip_state(struct virgl_context *ctx,
                                 const struct pipe_clip_state *clip);

int virgl_encode_resource_copy_region(struct virgl_context *ctx,
                                     struct virgl_resource *dst_res,
                                     unsigned dst_level,
                                     unsigned dstx, unsigned dsty, unsigned dstz,
                                     struct virgl_resource *src_res,
                                     unsigned src_level,
                                     const struct pipe_box *src_box);

int virgl_encode_blit(struct virgl_context *ctx,
                     struct virgl_resource *dst_res,
                     struct virgl_resource *src_res,
                     const struct pipe_blit_info *blit);

int virgl_encoder_create_query(struct virgl_context *ctx,
                              uint32_t handle,
                              uint query_type,
                              uint query_index,
                              struct virgl_resource *res,
                              uint32_t offset);

int virgl_encoder_begin_query(struct virgl_context *ctx,
                             uint32_t handle);
int virgl_encoder_end_query(struct virgl_context *ctx,
                           uint32_t handle);
int virgl_encoder_get_query_result(struct virgl_context *ctx,
                                  uint32_t handle, boolean wait);

int virgl_encoder_render_condition(struct virgl_context *ctx,
                                  uint32_t handle, boolean condition,
                                  enum pipe_render_cond_flag mode);

int virgl_encoder_set_sub_ctx(struct virgl_context *ctx, uint32_t sub_ctx_id);
int virgl_encoder_create_sub_ctx(struct virgl_context *ctx, uint32_t sub_ctx_id);
int virgl_encoder_destroy_sub_ctx(struct virgl_context *ctx, uint32_t sub_ctx_id);

int virgl_encode_bind_shader(struct virgl_context *ctx,
                             uint32_t handle, uint32_t type);
#endif
