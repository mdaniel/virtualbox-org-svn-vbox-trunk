
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

/**
 * \brief  Public interface into the drawing module.
 */

/* Authors:  Keith Whitwell <keithw@vmware.com>
 */


#ifndef DRAW_CONTEXT_H
#define DRAW_CONTEXT_H


#include "pipe/p_state.h"

struct pipe_context;
struct draw_context;
struct draw_stage;
struct draw_vertex_shader;
struct draw_geometry_shader;
struct draw_fragment_shader;
struct tgsi_sampler;
struct tgsi_image;
struct tgsi_buffer;

/*
 * structure to contain driver internal information 
 * for stream out support. mapping stores the pointer
 * to the buffer contents, and internal offset stores
 * an internal counter to how much of the stream
 * out buffer is used (in bytes).
 */
struct draw_so_target {
   struct pipe_stream_output_target target;
   void *mapping;
   int internal_offset;
};

struct draw_context *draw_create( struct pipe_context *pipe );

#if HAVE_LLVM
struct draw_context *draw_create_with_llvm_context(struct pipe_context *pipe,
                                                   void *context);
#endif

struct draw_context *draw_create_no_llvm(struct pipe_context *pipe);

void draw_destroy( struct draw_context *draw );

void draw_flush(struct draw_context *draw);

void draw_set_viewport_states( struct draw_context *draw,
                               unsigned start_slot,
                               unsigned num_viewports,
                               const struct pipe_viewport_state *viewports );

void draw_set_clip_state( struct draw_context *pipe,
                          const struct pipe_clip_state *clip );

/**
 * Sets the rasterization state used by the draw module.
 * The rast_handle is used to pass the driver specific representation
 * of the rasterization state. It's going to be used when the
 * draw module sets the state back on the driver itself using the
 * pipe::bind_rasterizer_state method.
 *
 * NOTE: if you're calling this function from within the pipe's
 * bind_rasterizer_state you should always call it before binding
 * the actual state - that's because the draw module can try to
 * bind its own rasterizer state which would reset your newly
 * set state. i.e. always do
 * draw_set_rasterizer_state(driver->draw, state->pipe_state, state);
 * driver->state.raster = state;
 */
void draw_set_rasterizer_state( struct draw_context *draw,
                                const struct pipe_rasterizer_state *raster,
                                void *rast_handle );

void draw_set_rasterize_stage( struct draw_context *draw,
                               struct draw_stage *stage );

void draw_wide_point_threshold(struct draw_context *draw, float threshold);

void draw_wide_point_sprites(struct draw_context *draw, boolean draw_sprite);

void draw_wide_line_threshold(struct draw_context *draw, float threshold);

void draw_enable_line_stipple(struct draw_context *draw, boolean enable);

void draw_enable_point_sprites(struct draw_context *draw, boolean enable);

void draw_set_zs_format(struct draw_context *draw, enum pipe_format format);

boolean
draw_install_aaline_stage(struct draw_context *draw, struct pipe_context *pipe);

boolean
draw_install_aapoint_stage(struct draw_context *draw, struct pipe_context *pipe);

boolean
draw_install_pstipple_stage(struct draw_context *draw, struct pipe_context *pipe);


struct tgsi_shader_info *
draw_get_shader_info(const struct draw_context *draw);

void
draw_prepare_shader_outputs(struct draw_context *draw);

int
draw_find_shader_output(const struct draw_context *draw,
                        uint semantic_name, uint semantic_index);

boolean
draw_will_inject_frontface(const struct draw_context *draw);

uint
draw_num_shader_outputs(const struct draw_context *draw);

uint
draw_total_vs_outputs(const struct draw_context *draw);

uint
draw_total_gs_outputs(const struct draw_context *draw);

void
draw_texture_sampler(struct draw_context *draw,
                     enum pipe_shader_type shader_type,
                     struct tgsi_sampler *sampler);

void
draw_image(struct draw_context *draw,
           enum pipe_shader_type shader_type,
           struct tgsi_image *image);

void
draw_buffer(struct draw_context *draw,
           enum pipe_shader_type shader_type,
           struct tgsi_buffer *buffer);

void
draw_set_sampler_views(struct draw_context *draw,
                       enum pipe_shader_type shader_stage,
                       struct pipe_sampler_view **views,
                       unsigned num);
void
draw_set_samplers(struct draw_context *draw,
                  enum pipe_shader_type shader_stage,
                  struct pipe_sampler_state **samplers,
                  unsigned num);

void
draw_set_mapped_texture(struct draw_context *draw,
                        enum pipe_shader_type shader_stage,
                        unsigned sview_idx,
                        uint32_t width, uint32_t height, uint32_t depth,
                        uint32_t first_level, uint32_t last_level,
                        const void *base,
                        uint32_t row_stride[PIPE_MAX_TEXTURE_LEVELS],
                        uint32_t img_stride[PIPE_MAX_TEXTURE_LEVELS],
                        uint32_t mip_offsets[PIPE_MAX_TEXTURE_LEVELS]);


/*
 * Vertex shader functions
 */

struct draw_vertex_shader *
draw_create_vertex_shader(struct draw_context *draw,
                          const struct pipe_shader_state *shader);
void draw_bind_vertex_shader(struct draw_context *draw,
                             struct draw_vertex_shader *dvs);
void draw_delete_vertex_shader(struct draw_context *draw,
                               struct draw_vertex_shader *dvs);
void draw_vs_attach_so(struct draw_vertex_shader *dvs,
                       const struct pipe_stream_output_info *info);
void draw_vs_reset_so(struct draw_vertex_shader *dvs);


/*
 * Fragment shader functions
 */
struct draw_fragment_shader *
draw_create_fragment_shader(struct draw_context *draw,
                            const struct pipe_shader_state *shader);
void draw_bind_fragment_shader(struct draw_context *draw,
                               struct draw_fragment_shader *dvs);
void draw_delete_fragment_shader(struct draw_context *draw,
                                 struct draw_fragment_shader *dvs);

/*
 * Geometry shader functions
 */
struct draw_geometry_shader *
draw_create_geometry_shader(struct draw_context *draw,
                            const struct pipe_shader_state *shader);
void draw_bind_geometry_shader(struct draw_context *draw,
                               struct draw_geometry_shader *dvs);
void draw_delete_geometry_shader(struct draw_context *draw,
                                 struct draw_geometry_shader *dvs);


/*
 * Vertex data functions
 */

void draw_set_vertex_buffers(struct draw_context *draw,
                             unsigned start_slot, unsigned count,
                             const struct pipe_vertex_buffer *buffers);

void draw_set_vertex_elements(struct draw_context *draw,
			      unsigned count,
                              const struct pipe_vertex_element *elements);

void draw_set_indexes(struct draw_context *draw,
                      const void *elements, unsigned elem_size,
                      unsigned available_space);

void draw_set_mapped_vertex_buffer(struct draw_context *draw,
                                   unsigned attr, const void *buffer,
                                   size_t size);

void
draw_set_mapped_constant_buffer(struct draw_context *draw,
                                enum pipe_shader_type shader_type,
                                unsigned slot,
                                const void *buffer,
                                unsigned size);

void
draw_set_mapped_so_targets(struct draw_context *draw,
                           int num_targets,
                           struct draw_so_target *targets[PIPE_MAX_SO_BUFFERS]);


/***********************************************************************
 * draw_pt.c 
 */

void draw_vbo(struct draw_context *draw,
              const struct pipe_draw_info *info);


/*******************************************************************************
 * Driver backend interface 
 */
struct vbuf_render;
void draw_set_render( struct draw_context *draw, 
		      struct vbuf_render *render );

void draw_set_driver_clipping( struct draw_context *draw,
                               boolean bypass_clip_xy,
                               boolean bypass_clip_z,
                               boolean guard_band_xy,
                               boolean bypass_clip_points);

void draw_set_force_passthrough( struct draw_context *draw, 
                                 boolean enable );


/*******************************************************************************
 * Draw statistics
 */
void draw_collect_pipeline_statistics(struct draw_context *draw,
                                      boolean enable);

/*******************************************************************************
 * Draw pipeline 
 */
boolean draw_need_pipeline(const struct draw_context *draw,
                           const struct pipe_rasterizer_state *rasterizer,
                           unsigned prim );

int
draw_get_shader_param(enum pipe_shader_type shader, enum pipe_shader_cap param);

int
draw_get_shader_param_no_llvm(enum pipe_shader_type shader,
                              enum pipe_shader_cap param);

boolean
draw_get_option_use_llvm(void);

#endif /* DRAW_CONTEXT_H */
