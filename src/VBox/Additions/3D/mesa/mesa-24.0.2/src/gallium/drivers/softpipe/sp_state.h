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

/* Authors:  Keith Whitwell <keithw@vmware.com>
 */

#ifndef SP_STATE_H
#define SP_STATE_H

#include "pipe/p_state.h"
#include "tgsi/tgsi_scan.h"


#define SP_NEW_VIEWPORT      0x1
#define SP_NEW_RASTERIZER    0x2
#define SP_NEW_FS            0x4
#define SP_NEW_BLEND         0x8
#define SP_NEW_CLIP          0x10
#define SP_NEW_SCISSOR       0x20

#define SP_NEW_FRAMEBUFFER   0x80
#define SP_NEW_DEPTH_STENCIL_ALPHA 0x100
#define SP_NEW_CONSTANTS     0x200
#define SP_NEW_SAMPLER       0x400
#define SP_NEW_TEXTURE       0x800
#define SP_NEW_VERTEX        0x1000
#define SP_NEW_VS            0x2000
#define SP_NEW_QUERY         0x4000
#define SP_NEW_GS            0x8000
#define SP_NEW_SO            0x10000
#define SP_NEW_SO_BUFFERS    0x20000


struct tgsi_sampler;
struct tgsi_image;
struct tgsi_buffer;
struct tgsi_exec_machine;
struct vertex_info;


struct sp_fragment_shader_variant_key
{
   /* Standard C doesn't allow 0-size structs */
   int dummy;
};


struct sp_fragment_shader_variant
{
   const struct tgsi_token *tokens;
   struct sp_fragment_shader_variant_key key;
   struct tgsi_shader_info info;

   /* See comments about this elsewhere */
#if 0
   struct draw_fragment_shader *draw_shader;
#endif

   void (*prepare)(const struct sp_fragment_shader_variant *shader,
		   struct tgsi_exec_machine *machine,
		   struct tgsi_sampler *sampler,
		   struct tgsi_image *image,
		   struct tgsi_buffer *buffer);

   unsigned (*run)(const struct sp_fragment_shader_variant *shader,
		   struct tgsi_exec_machine *machine,
		   struct quad_header *quad,
		   bool early_depth_test);

   /* Deletes this instance of the object */
   void (*delete)(struct sp_fragment_shader_variant *shader,
                  struct tgsi_exec_machine *machine);

   struct sp_fragment_shader_variant *next;
};


/** Subclass of pipe_shader_state */
struct sp_fragment_shader {
   struct pipe_shader_state shader;
   struct sp_fragment_shader_variant *variants;
   struct draw_fragment_shader *draw_shader;
};


/** Subclass of pipe_shader_state */
struct sp_vertex_shader {
   struct pipe_shader_state shader;
   struct draw_vertex_shader *draw_data;
   int max_sampler;             /* -1 if no samplers */
};

/** Subclass of pipe_shader_state */
struct sp_geometry_shader {
   struct pipe_shader_state shader;
   struct draw_geometry_shader *draw_data;
   int max_sampler;
};

struct sp_velems_state {
   unsigned count;
   struct pipe_vertex_element velem[PIPE_MAX_ATTRIBS];
};

struct sp_so_state {
   struct pipe_stream_output_info base;
};

/** Subclass of pipe_compute_state */
struct sp_compute_shader {
   struct pipe_compute_state shader;
   struct tgsi_token *tokens;
   struct tgsi_shader_info info;
   int max_sampler;             /* -1 if no samplers */
};

void
softpipe_init_blend_funcs(struct pipe_context *pipe);

void
softpipe_init_clip_funcs(struct pipe_context *pipe);

void
softpipe_init_sampler_funcs(struct pipe_context *pipe);

void
softpipe_init_rasterizer_funcs(struct pipe_context *pipe);

void
softpipe_init_shader_funcs(struct pipe_context *pipe);

void
softpipe_init_streamout_funcs(struct pipe_context *pipe);

void
softpipe_init_vertex_funcs(struct pipe_context *pipe);

void
softpipe_init_image_funcs(struct pipe_context *pipe);

void
softpipe_set_framebuffer_state(struct pipe_context *,
                               const struct pipe_framebuffer_state *);

void
softpipe_update_derived(struct softpipe_context *softpipe, unsigned prim);

void
softpipe_set_sampler_views(struct pipe_context *pipe,
                           enum pipe_shader_type shader,
                           unsigned start,
                           unsigned num,
                           unsigned unbind_num_trailing_slots,
                           bool take_ownership,
                           struct pipe_sampler_view **views);


void
softpipe_draw_vbo(struct pipe_context *pipe,
                  const struct pipe_draw_info *info,
                  unsigned drawid_offset,
                  const struct pipe_draw_indirect_info *indirect,
                  const struct pipe_draw_start_count_bias *draws,
                  unsigned num_draws);

void
softpipe_map_texture_surfaces(struct softpipe_context *sp);

void
softpipe_unmap_texture_surfaces(struct softpipe_context *sp);


struct vertex_info *
softpipe_get_vbuf_vertex_info(struct softpipe_context *softpipe);


struct sp_fragment_shader_variant *
softpipe_find_fs_variant(struct softpipe_context *softpipe,
                         struct sp_fragment_shader *fs,
                         const struct sp_fragment_shader_variant_key *key);

void
softpipe_prepare_vertex_sampling(struct softpipe_context *ctx,
                                 unsigned num,
                                 struct pipe_sampler_view **views);
void
softpipe_cleanup_vertex_sampling(struct softpipe_context *ctx);


void
softpipe_prepare_geometry_sampling(struct softpipe_context *ctx,
                                   unsigned num,
                                   struct pipe_sampler_view **views);
void
softpipe_cleanup_geometry_sampling(struct softpipe_context *ctx);


void
softpipe_launch_grid(struct pipe_context *context,
                     const struct pipe_grid_info *info);

void
softpipe_update_compute_samplers(struct softpipe_context *softpipe);
#endif
