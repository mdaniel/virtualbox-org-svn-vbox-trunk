/**************************************************************************
 *
 * Copyright 2007-2009 VMware, Inc.
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
 * The setup code is concerned with point/line/triangle setup and
 * putting commands/data into the bins.
 */


#ifndef LP_SETUP_CONTEXT_H
#define LP_SETUP_CONTEXT_H

#include "lp_setup.h"
#include "lp_rast.h"
#include "lp_scene.h"
#include "lp_bld_interp.h"	/* for struct lp_shader_input */

#include "draw/draw_vbuf.h"
#include "util/u_rect.h"
#include "util/u_pack_color.h"

#define LP_SETUP_NEW_FS          0x01
#define LP_SETUP_NEW_CONSTANTS   0x02
#define LP_SETUP_NEW_BLEND_COLOR 0x04
#define LP_SETUP_NEW_SCISSOR     0x08
#define LP_SETUP_NEW_VIEWPORTS   0x10


struct lp_setup_variant;


/** Max number of scenes */
/* XXX: make multiple scenes per context work, see lp_setup_rasterize_scene */
#define MAX_SCENES 1



/**
 * Point/line/triangle setup context.
 * Note: "stored" below indicates data which is stored in the bins,
 * not arbitrary malloc'd memory.
 *
 *
 * Subclass of vbuf_render, plugged directly into the draw module as
 * the rendering backend.
 */
struct lp_setup_context
{
   struct vbuf_render base;

   struct pipe_context *pipe;
   struct vertex_info *vertex_info;
   uint prim;
   uint vertex_size;
   uint nr_vertices;
   uint sprite_coord_enable, sprite_coord_origin;
   uint vertex_buffer_size;
   void *vertex_buffer;

   /* Final pipeline stage for draw module.  Draw module should
    * create/install this itself now.
    */
   struct draw_stage *vbuf;
   unsigned num_threads;
   unsigned scene_idx;
   struct lp_scene *scenes[MAX_SCENES];  /**< all the scenes */
   struct lp_scene *scene;               /**< current scene being built */

   struct lp_fence *last_fence;
   struct llvmpipe_query *active_queries[LP_MAX_ACTIVE_BINNED_QUERIES];
   unsigned active_binned_queries;

   boolean flatshade_first;
   boolean ccw_is_frontface;
   boolean scissor_test;
   boolean point_size_per_vertex;
   boolean rasterizer_discard;
   unsigned cullmode;
   unsigned bottom_edge_rule;
   float pixel_offset;
   float line_width;
   float point_size;
   int8_t psize_slot;
   int8_t viewport_index_slot;
   int8_t layer_slot;
   int8_t face_slot;

   struct pipe_framebuffer_state fb;
   struct u_rect framebuffer;
   struct u_rect scissors[PIPE_MAX_VIEWPORTS];
   struct u_rect draw_regions[PIPE_MAX_VIEWPORTS];   /* intersection of fb & scissor */
   struct lp_jit_viewport viewports[PIPE_MAX_VIEWPORTS];

   struct {
      unsigned flags;
      union util_color color_val[PIPE_MAX_COLOR_BUFS];
      uint64_t zsmask;
      uint64_t zsvalue;               /**< lp_rast_clear_zstencil() cmd */
   } clear;

   enum setup_state {
      SETUP_FLUSHED,    /**< scene is null */
      SETUP_CLEARED,    /**< scene exists but has only clears */
      SETUP_ACTIVE      /**< scene exists and has at least one draw/query */
   } state;
   
   struct {
      const struct lp_rast_state *stored; /**< what's in the scene */
      struct lp_rast_state current;  /**< currently set state */
      struct pipe_resource *current_tex[PIPE_MAX_SHADER_SAMPLER_VIEWS];
      unsigned current_tex_num;
   } fs;

   /** fragment shader constants */
   struct {
      struct pipe_constant_buffer current;
      unsigned stored_size;
      const void *stored_data;
   } constants[LP_MAX_TGSI_CONST_BUFFERS];

   struct {
      struct pipe_blend_color current;
      uint8_t *stored;
   } blend_color;


   struct {
      const struct lp_setup_variant *variant;
   } setup;

   unsigned dirty;   /**< bitmask of LP_SETUP_NEW_x bits */

   void (*point)( struct lp_setup_context *,
                  const float (*v0)[4]);

   void (*line)( struct lp_setup_context *,
                 const float (*v0)[4],
                 const float (*v1)[4]);

   void (*triangle)( struct lp_setup_context *,
                     const float (*v0)[4],
                     const float (*v1)[4],
                     const float (*v2)[4]);
};

static inline void
scissor_planes_needed(boolean scis_planes[4], const struct u_rect *bbox,
                      const struct u_rect *scissor)
{
   /* left */
   scis_planes[0] = (bbox->x0 < scissor->x0);
   /* right */
   scis_planes[1] = (bbox->x1 > scissor->x1);
   /* top */
   scis_planes[2] = (bbox->y0 < scissor->y0);
   /* bottom */
   scis_planes[3] = (bbox->y1 > scissor->y1);
}


void lp_setup_choose_triangle( struct lp_setup_context *setup );
void lp_setup_choose_line( struct lp_setup_context *setup );
void lp_setup_choose_point( struct lp_setup_context *setup );

void lp_setup_init_vbuf(struct lp_setup_context *setup);

boolean lp_setup_update_state( struct lp_setup_context *setup,
                            boolean update_scene);

void lp_setup_destroy( struct lp_setup_context *setup );

boolean lp_setup_flush_and_restart(struct lp_setup_context *setup);

void
lp_setup_print_triangle(struct lp_setup_context *setup,
                        const float (*v0)[4],
                        const float (*v1)[4],
                        const float (*v2)[4]);

void
lp_setup_print_vertex(struct lp_setup_context *setup,
                      const char *name,
                      const float (*v)[4]);


struct lp_rast_triangle *
lp_setup_alloc_triangle(struct lp_scene *scene,
                        unsigned num_inputs,
                        unsigned nr_planes,
                        unsigned *tri_size);

boolean
lp_setup_bin_triangle(struct lp_setup_context *setup,
                      struct lp_rast_triangle *tri,
                      const struct u_rect *bboxorig,
                      const struct u_rect *bbox,
                      int nr_planes,
                      unsigned scissor_index);

#endif
