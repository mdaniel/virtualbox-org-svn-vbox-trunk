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

/*
 * This file implements the st_draw_vbo() function which is called from
 * Mesa's VBO module.  All point/line/triangle rendering is done through
 * this function whether the user called glBegin/End, glDrawArrays,
 * glDrawElements, glEvalMesh, or glCalList, etc.
 *
 * Authors:
 *   Keith Whitwell <keithw@vmware.com>
 */


#include "main/imports.h"
#include "main/image.h"
#include "main/bufferobj.h"
#include "main/macros.h"
#include "main/varray.h"

#include "compiler/glsl/ir_uniform.h"

#include "vbo/vbo.h"

#include "st_context.h"
#include "st_atom.h"
#include "st_cb_bitmap.h"
#include "st_cb_bufferobjects.h"
#include "st_cb_xformfb.h"
#include "st_debug.h"
#include "st_draw.h"
#include "st_program.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_format.h"
#include "util/u_prim.h"
#include "util/u_draw.h"
#include "util/u_upload_mgr.h"
#include "draw/draw_context.h"
#include "cso_cache/cso_context.h"


/**
 * Set the restart index.
 */
static void
setup_primitive_restart(struct gl_context *ctx, struct pipe_draw_info *info)
{
   if (ctx->Array._PrimitiveRestart) {
      unsigned index_size = info->index_size;

      info->restart_index =
         _mesa_primitive_restart_index(ctx, index_size);

      /* Enable primitive restart only when the restart index can have an
       * effect. This is required for correctness in radeonsi VI support.
       * Other hardware may also benefit from taking a faster, non-restart path
       * when possible.
       */
      if (index_size == 4 || info->restart_index < (1 << (index_size * 8)))
         info->primitive_restart = true;
   }
}


/**
 * Translate OpenGL primtive type (GL_POINTS, GL_TRIANGLE_STRIP, etc) to
 * the corresponding Gallium type.
 */
static unsigned
translate_prim(const struct gl_context *ctx, unsigned prim)
{
   /* GL prims should match Gallium prims, spot-check a few */
   STATIC_ASSERT(GL_POINTS == PIPE_PRIM_POINTS);
   STATIC_ASSERT(GL_QUADS == PIPE_PRIM_QUADS);
   STATIC_ASSERT(GL_TRIANGLE_STRIP_ADJACENCY == PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY);
   STATIC_ASSERT(GL_PATCHES == PIPE_PRIM_PATCHES);

   return prim;
}

static inline void
prepare_draw(struct st_context *st, struct gl_context *ctx)
{
   /* Mesa core state should have been validated already */
   assert(ctx->NewState == 0x0);

   if (unlikely(!st->bitmap.cache.empty))
      st_flush_bitmap_cache(st);

   st_invalidate_readpix_cache(st);

   /* Validate state. */
   if ((st->dirty | ctx->NewDriverState) & ST_PIPELINE_RENDER_STATE_MASK ||
       st->gfx_shaders_may_be_dirty) {
      st_validate_state(st, ST_PIPELINE_RENDER);
   }
}

/**
 * This function gets plugged into the VBO module and is called when
 * we have something to render.
 * Basically, translate the information into the format expected by gallium.
 */
static void
st_draw_vbo(struct gl_context *ctx,
            const struct _mesa_prim *prims,
            GLuint nr_prims,
            const struct _mesa_index_buffer *ib,
	    GLboolean index_bounds_valid,
            GLuint min_index,
            GLuint max_index,
            struct gl_transform_feedback_object *tfb_vertcount,
            unsigned stream,
            struct gl_buffer_object *indirect)
{
   struct st_context *st = st_context(ctx);
   struct pipe_draw_info info;
   unsigned i;
   unsigned start = 0;

   prepare_draw(st, ctx);

   if (st->vertex_array_out_of_memory)
      return;

   /* Initialize pipe_draw_info. */
   info.primitive_restart = false;
   info.vertices_per_patch = ctx->TessCtrlProgram.patch_vertices;
   info.indirect = NULL;
   info.count_from_stream_output = NULL;

   if (ib) {
      struct gl_buffer_object *bufobj = ib->obj;

      /* Get index bounds for user buffers. */
      if (!index_bounds_valid && st->draw_needs_minmax_index) {
         vbo_get_minmax_indices(ctx, prims, ib, &min_index, &max_index,
                                nr_prims);
      }

      info.index_size = ib->index_size;
      info.min_index = min_index;
      info.max_index = max_index;

      if (_mesa_is_bufferobj(bufobj)) {
         /* indices are in a real VBO */
         info.has_user_indices = false;
         info.index.resource = st_buffer_object(bufobj)->buffer;
         start = pointer_to_offset(ib->ptr) / info.index_size;
      } else {
         /* indices are in user space memory */
         info.has_user_indices = true;
         info.index.user = ib->ptr;
      }

      setup_primitive_restart(ctx, &info);
   }
   else {
      info.index_size = 0;
      info.has_user_indices = false;

      /* Transform feedback drawing is always non-indexed. */
      /* Set info.count_from_stream_output. */
      if (tfb_vertcount) {
         if (!st_transform_feedback_draw_init(tfb_vertcount, stream, &info))
            return;
      }
   }

   assert(!indirect);

   /* do actual drawing */
   for (i = 0; i < nr_prims; i++) {
      info.count = prims[i].count;

      /* Skip no-op draw calls. */
      if (!info.count && !tfb_vertcount)
         continue;

      info.mode = translate_prim(ctx, prims[i].mode);
      info.start = start + prims[i].start;
      info.start_instance = prims[i].base_instance;
      info.instance_count = prims[i].num_instances;
      info.index_bias = prims[i].basevertex;
      info.drawid = prims[i].draw_id;
      if (!ib) {
         info.min_index = info.start;
         info.max_index = info.start + info.count - 1;
      }

      if (ST_DEBUG & DEBUG_DRAW) {
         debug_printf("st/draw: mode %s  start %u  count %u  index_size %d\n",
                      u_prim_name(info.mode),
                      info.start,
                      info.count,
                      info.index_size);
      }

      /* Don't call u_trim_pipe_prim. Drivers should do it if they need it. */
      cso_draw_vbo(st->cso_context, &info);
   }
}

static void
st_indirect_draw_vbo(struct gl_context *ctx,
                     GLuint mode,
                     struct gl_buffer_object *indirect_data,
                     GLsizeiptr indirect_offset,
                     unsigned draw_count,
                     unsigned stride,
                     struct gl_buffer_object *indirect_params,
                     GLsizeiptr indirect_params_offset,
                     const struct _mesa_index_buffer *ib)
{
   struct st_context *st = st_context(ctx);
   struct pipe_draw_info info;
   struct pipe_draw_indirect_info indirect;

   assert(stride);
   prepare_draw(st, ctx);

   if (st->vertex_array_out_of_memory)
      return;

   memset(&indirect, 0, sizeof(indirect));
   util_draw_init_info(&info);
   info.start = 0; /* index offset / index size */

   if (ib) {
      struct gl_buffer_object *bufobj = ib->obj;

      /* indices are always in a real VBO */
      assert(_mesa_is_bufferobj(bufobj));

      info.index_size = ib->index_size;
      info.index.resource = st_buffer_object(bufobj)->buffer;
      info.start = pointer_to_offset(ib->ptr) / info.index_size;

      /* Primitive restart is not handled by the VBO module in this case. */
      setup_primitive_restart(ctx, &info);
   }

   info.mode = translate_prim(ctx, mode);
   info.vertices_per_patch = ctx->TessCtrlProgram.patch_vertices;
   info.indirect = &indirect;
   indirect.buffer = st_buffer_object(indirect_data)->buffer;
   indirect.offset = indirect_offset;

   if (ST_DEBUG & DEBUG_DRAW) {
      debug_printf("st/draw indirect: mode %s drawcount %d index_size %d\n",
                   u_prim_name(info.mode),
                   draw_count,
                   info.index_size);
   }

   if (!st->has_multi_draw_indirect) {
      int i;

      assert(!indirect_params);
      indirect.draw_count = 1;
      for (i = 0; i < draw_count; i++) {
         info.drawid = i;
         cso_draw_vbo(st->cso_context, &info);
         indirect.offset += stride;
      }
   } else {
      indirect.draw_count = draw_count;
      indirect.stride = stride;
      if (indirect_params) {
         indirect.indirect_draw_count = st_buffer_object(indirect_params)->buffer;
         indirect.indirect_draw_count_offset = indirect_params_offset;
      }
      cso_draw_vbo(st->cso_context, &info);
   }
}


void
st_init_draw(struct st_context *st)
{
   struct gl_context *ctx = st->ctx;

   vbo_set_draw_func(ctx, st_draw_vbo);
   vbo_set_indirect_draw_func(ctx, st_indirect_draw_vbo);
}


void
st_destroy_draw(struct st_context *st)
{
   draw_destroy(st->draw);
}

/**
 * Getter for the draw_context, so that initialization of it can happen only
 * when needed (the TGSI exec machines take up quite a bit of memory).
 */
struct draw_context *
st_get_draw_context(struct st_context *st)
{
   if (!st->draw) {
      st->draw = draw_create(st->pipe);
      if (!st->draw) {
         _mesa_error(st->ctx, GL_OUT_OF_MEMORY, "feedback fallback allocation");
         return NULL;
      }
   }

   /* Disable draw options that might convert points/lines to tris, etc.
    * as that would foul-up feedback/selection mode.
    */
   draw_wide_line_threshold(st->draw, 1000.0f);
   draw_wide_point_threshold(st->draw, 1000.0f);
   draw_enable_line_stipple(st->draw, FALSE);
   draw_enable_point_sprites(st->draw, FALSE);

   return st->draw;
}

/**
 * Draw a quad with given position, texcoords and color.
 */
bool
st_draw_quad(struct st_context *st,
             float x0, float y0, float x1, float y1, float z,
             float s0, float t0, float s1, float t1,
             const float *color,
             unsigned num_instances)
{
   struct pipe_vertex_buffer vb = {0};
   struct st_util_vertex *verts;

   vb.stride = sizeof(struct st_util_vertex);

   u_upload_alloc(st->pipe->stream_uploader, 0,
                  4 * sizeof(struct st_util_vertex), 4,
                  &vb.buffer_offset, &vb.buffer.resource, (void **) &verts);
   if (!vb.buffer.resource) {
      return false;
   }

   /* lower-left */
   verts[0].x = x0;
   verts[0].y = y1;
   verts[0].z = z;
   verts[0].r = color[0];
   verts[0].g = color[1];
   verts[0].b = color[2];
   verts[0].a = color[3];
   verts[0].s = s0;
   verts[0].t = t0;

   /* lower-right */
   verts[1].x = x1;
   verts[1].y = y1;
   verts[1].z = z;
   verts[1].r = color[0];
   verts[1].g = color[1];
   verts[1].b = color[2];
   verts[1].a = color[3];
   verts[1].s = s1;
   verts[1].t = t0;

   /* upper-right */
   verts[2].x = x1;
   verts[2].y = y0;
   verts[2].z = z;
   verts[2].r = color[0];
   verts[2].g = color[1];
   verts[2].b = color[2];
   verts[2].a = color[3];
   verts[2].s = s1;
   verts[2].t = t1;

   /* upper-left */
   verts[3].x = x0;
   verts[3].y = y0;
   verts[3].z = z;
   verts[3].r = color[0];
   verts[3].g = color[1];
   verts[3].b = color[2];
   verts[3].a = color[3];
   verts[3].s = s0;
   verts[3].t = t1;

   u_upload_unmap(st->pipe->stream_uploader);

   /* At the time of writing, cso_get_aux_vertex_buffer_slot() always returns
    * zero.  If that ever changes we need to audit the calls to that function
    * and make sure the slot number is used consistently everywhere.
    */
   assert(cso_get_aux_vertex_buffer_slot(st->cso_context) == 0);

   cso_set_vertex_buffers(st->cso_context,
                          cso_get_aux_vertex_buffer_slot(st->cso_context),
                          1, &vb);

   if (num_instances > 1) {
      cso_draw_arrays_instanced(st->cso_context, PIPE_PRIM_TRIANGLE_FAN, 0, 4,
                                0, num_instances);
   } else {
      cso_draw_arrays(st->cso_context, PIPE_PRIM_TRIANGLE_FAN, 0, 4);
   }

   pipe_resource_reference(&vb.buffer.resource, NULL);

   return true;
}
