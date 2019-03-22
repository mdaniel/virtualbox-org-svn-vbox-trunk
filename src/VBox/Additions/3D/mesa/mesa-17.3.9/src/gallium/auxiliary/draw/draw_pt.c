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
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */

#include "draw/draw_context.h"
#include "draw/draw_gs.h"
#include "draw/draw_private.h"
#include "draw/draw_pt.h"
#include "draw/draw_vbuf.h"
#include "draw/draw_vs.h"
#include "tgsi/tgsi_dump.h"
#include "util/u_math.h"
#include "util/u_prim.h"
#include "util/u_format.h"
#include "util/u_draw.h"


DEBUG_GET_ONCE_BOOL_OPTION(draw_fse, "DRAW_FSE", FALSE)
DEBUG_GET_ONCE_BOOL_OPTION(draw_no_fse, "DRAW_NO_FSE", FALSE)

/* Overall we split things into:
 *     - frontend -- prepare fetch_elts, draw_elts - eg vsplit
 *     - middle   -- fetch, shade, cliptest, viewport
 *     - pipeline -- the prim pipeline: clipping, wide lines, etc 
 *     - backend  -- the vbuf_render provided by the driver.
 */
static boolean
draw_pt_arrays(struct draw_context *draw,
               unsigned prim,
               unsigned start, 
               unsigned count)
{
   struct draw_pt_front_end *frontend = NULL;
   struct draw_pt_middle_end *middle = NULL;
   unsigned opt = 0;

   /* Sanitize primitive length:
    */
   {
      unsigned first, incr;
      draw_pt_split_prim(prim, &first, &incr);
      count = draw_pt_trim_count(count, first, incr);
      if (count < first)
         return TRUE;
   }

   if (!draw->force_passthrough) {
      unsigned gs_out_prim = (draw->gs.geometry_shader ? 
                              draw->gs.geometry_shader->output_primitive :
                              prim);

      if (!draw->render) {
         opt |= PT_PIPELINE;
      }

      if (draw_need_pipeline(draw,
                             draw->rasterizer,
                             gs_out_prim)) {
         opt |= PT_PIPELINE;
      }

      if ((draw->clip_xy ||
           draw->clip_z ||
           draw->clip_user) && !draw->pt.test_fse) {
         opt |= PT_CLIPTEST;
      }

      opt |= PT_SHADE;
   }

   if (draw->pt.middle.llvm) {
      middle = draw->pt.middle.llvm;
   } else {
      if (opt == 0)
         middle = draw->pt.middle.fetch_emit;
      else if (opt == PT_SHADE && !draw->pt.no_fse)
         middle = draw->pt.middle.fetch_shade_emit;
      else
         middle = draw->pt.middle.general;
   }

   frontend = draw->pt.frontend;

   if (frontend) {
      if (draw->pt.prim != prim || draw->pt.opt != opt) {
         /* In certain conditions switching primitives requires us to flush
          * and validate the different stages. One example is when smooth
          * lines are active but first drawn with triangles and then with
          * lines.
          */
         draw_do_flush( draw, DRAW_FLUSH_STATE_CHANGE );
         frontend = NULL;
      } else if (draw->pt.eltSize != draw->pt.user.eltSize) {
         /* Flush draw state if eltSize changed.
          * This could be improved so only the frontend is flushed since it
          * converts all indices to ushorts and the fetch part of the middle
          * always prepares both linear and indexed.
          */
         frontend->flush( frontend, DRAW_FLUSH_STATE_CHANGE );
         frontend = NULL;
      }
   }

   if (!frontend) {
      frontend = draw->pt.front.vsplit;

      frontend->prepare( frontend, prim, middle, opt );

      draw->pt.frontend = frontend;
      draw->pt.eltSize = draw->pt.user.eltSize;
      draw->pt.prim = prim;
      draw->pt.opt = opt;
   }

   if (draw->pt.rebind_parameters) {
      /* update constants, viewport dims, clip planes, etc */
      middle->bind_parameters(middle);
      draw->pt.rebind_parameters = FALSE;
   }

   frontend->run( frontend, start, count );

   return TRUE;
}

void draw_pt_flush( struct draw_context *draw, unsigned flags )
{
   assert(flags);

   if (draw->pt.frontend) {
      draw->pt.frontend->flush( draw->pt.frontend, flags );

      /* don't prepare if we only are flushing the backend */
      if (flags & DRAW_FLUSH_STATE_CHANGE)
         draw->pt.frontend = NULL;
   }

   if (flags & DRAW_FLUSH_PARAMETER_CHANGE) {
      draw->pt.rebind_parameters = TRUE;
   }
}



boolean draw_pt_init( struct draw_context *draw )
{
   draw->pt.test_fse = debug_get_option_draw_fse();
   draw->pt.no_fse = debug_get_option_draw_no_fse();

   draw->pt.front.vsplit = draw_pt_vsplit(draw);
   if (!draw->pt.front.vsplit)
      return FALSE;

   draw->pt.middle.fetch_emit = draw_pt_fetch_emit( draw );
   if (!draw->pt.middle.fetch_emit)
      return FALSE;

   draw->pt.middle.fetch_shade_emit = draw_pt_middle_fse( draw );
   if (!draw->pt.middle.fetch_shade_emit)
      return FALSE;

   draw->pt.middle.general = draw_pt_fetch_pipeline_or_emit( draw );
   if (!draw->pt.middle.general)
      return FALSE;

#if HAVE_LLVM
   if (draw->llvm)
      draw->pt.middle.llvm = draw_pt_fetch_pipeline_or_emit_llvm( draw );
#endif

   return TRUE;
}


void draw_pt_destroy( struct draw_context *draw )
{
   if (draw->pt.middle.llvm) {
      draw->pt.middle.llvm->destroy( draw->pt.middle.llvm );
      draw->pt.middle.llvm = NULL;
   }

   if (draw->pt.middle.general) {
      draw->pt.middle.general->destroy( draw->pt.middle.general );
      draw->pt.middle.general = NULL;
   }

   if (draw->pt.middle.fetch_emit) {
      draw->pt.middle.fetch_emit->destroy( draw->pt.middle.fetch_emit );
      draw->pt.middle.fetch_emit = NULL;
   }

   if (draw->pt.middle.fetch_shade_emit) {
      draw->pt.middle.fetch_shade_emit->destroy( draw->pt.middle.fetch_shade_emit );
      draw->pt.middle.fetch_shade_emit = NULL;
   }

   if (draw->pt.front.vsplit) {
      draw->pt.front.vsplit->destroy( draw->pt.front.vsplit );
      draw->pt.front.vsplit = NULL;
   }
}


/**
 * Debug- print the first 'count' vertices.
 */
static void
draw_print_arrays(struct draw_context *draw, uint prim, int start, uint count)
{
   uint i;

   debug_printf("Draw arrays(prim = %u, start = %u, count = %u)\n",
                prim, start, count);

   for (i = 0; i < count; i++) {
      uint ii = 0;
      uint j;

      if (draw->pt.user.eltSize) {
         /* indexed arrays */

         switch (draw->pt.user.eltSize) {
         case 1:
            {
               const ubyte *elem = (const ubyte *) draw->pt.user.elts;
               ii = elem[start + i];
            }
            break;
         case 2:
            {
               const ushort *elem = (const ushort *) draw->pt.user.elts;
               ii = elem[start + i];
            }
            break;
         case 4:
            {
               const uint *elem = (const uint *) draw->pt.user.elts;
               ii = elem[start + i];
            }
            break;
         default:
            assert(0);
            return;
         }
         ii += draw->pt.user.eltBias;
         debug_printf("Element[%u + %u] + %i -> Vertex %u:\n", start, i,
                      draw->pt.user.eltBias, ii);
      }
      else {
         /* non-indexed arrays */
         ii = start + i;
         debug_printf("Vertex %u:\n", ii);
      }

      for (j = 0; j < draw->pt.nr_vertex_elements; j++) {
         uint buf = draw->pt.vertex_element[j].vertex_buffer_index;
         ubyte *ptr = (ubyte *) draw->pt.user.vbuffer[buf].map;

         if (draw->pt.vertex_element[j].instance_divisor) {
            ii = draw->instance_id / draw->pt.vertex_element[j].instance_divisor;
         }

         ptr += draw->pt.vertex_buffer[buf].buffer_offset;
         ptr += draw->pt.vertex_buffer[buf].stride * ii;
         ptr += draw->pt.vertex_element[j].src_offset;

         debug_printf("  Attr %u: ", j);
         switch (draw->pt.vertex_element[j].src_format) {
         case PIPE_FORMAT_R32_FLOAT:
            {
               float *v = (float *) ptr;
               debug_printf("R %f  @ %p\n", v[0], (void *) v);
            }
            break;
         case PIPE_FORMAT_R32G32_FLOAT:
            {
               float *v = (float *) ptr;
               debug_printf("RG %f %f  @ %p\n", v[0], v[1], (void *) v);
            }
            break;
         case PIPE_FORMAT_R32G32B32_FLOAT:
            {
               float *v = (float *) ptr;
               debug_printf("RGB %f %f %f  @ %p\n", v[0], v[1], v[2], (void *) v);
            }
            break;
         case PIPE_FORMAT_R32G32B32A32_FLOAT:
            {
               float *v = (float *) ptr;
               debug_printf("RGBA %f %f %f %f  @ %p\n", v[0], v[1], v[2], v[3],
                            (void *) v);
            }
            break;
         case PIPE_FORMAT_B8G8R8A8_UNORM:
            {
               ubyte *u = (ubyte *) ptr;
               debug_printf("BGRA %d %d %d %d  @ %p\n", u[0], u[1], u[2], u[3],
                            (void *) u);
            }
            break;
         case PIPE_FORMAT_A8R8G8B8_UNORM:
            {
               ubyte *u = (ubyte *) ptr;
               debug_printf("ARGB %d %d %d %d  @ %p\n", u[0], u[1], u[2], u[3],
                            (void *) u);
            }
            break;
         default:
            debug_printf("other format %s (fix me)\n",
                     util_format_name(draw->pt.vertex_element[j].src_format));
         }
      }
   }
}


/** Helper code for below */
#define PRIM_RESTART_LOOP(elements) \
   do { \
      for (j = 0; j < count; j++) {               \
         i = draw_overflow_uadd(start, j, MAX_LOOP_IDX);  \
         if (i < elt_max && elements[i] == info->restart_index) { \
            if (cur_count > 0) { \
               /* draw elts up to prev pos */ \
               draw_pt_arrays(draw, prim, cur_start, cur_count); \
            } \
            /* begin new prim at next elt */ \
            cur_start = i + 1; \
            cur_count = 0; \
         } \
         else { \
            cur_count++; \
         } \
      } \
      if (cur_count > 0) { \
         draw_pt_arrays(draw, prim, cur_start, cur_count); \
      } \
   } while (0)


/**
 * For drawing prims with primitive restart enabled.
 * Scan for restart indexes and draw the runs of elements/vertices between
 * the restarts.
 */
static void
draw_pt_arrays_restart(struct draw_context *draw,
                       const struct pipe_draw_info *info)
{
   const unsigned prim = info->mode;
   const unsigned start = info->start;
   const unsigned count = info->count;
   const unsigned elt_max = draw->pt.user.eltMax;
   unsigned i, j, cur_start, cur_count;
   /* The largest index within a loop using the i variable as the index.
    * Used for overflow detection */
   const unsigned MAX_LOOP_IDX = 0xffffffff;

   assert(info->primitive_restart);

   if (draw->pt.user.eltSize) {
      /* indexed prims (draw_elements) */
      cur_start = start;
      cur_count = 0;

      switch (draw->pt.user.eltSize) {
      case 1:
         {
            const ubyte *elt_ub = (const ubyte *) draw->pt.user.elts;
            PRIM_RESTART_LOOP(elt_ub);
         }
         break;
      case 2:
         {
            const ushort *elt_us = (const ushort *) draw->pt.user.elts;
            PRIM_RESTART_LOOP(elt_us);
         }
         break;
      case 4:
         {
            const uint *elt_ui = (const uint *) draw->pt.user.elts;
            PRIM_RESTART_LOOP(elt_ui);
         }
         break;
      default:
         assert(0 && "bad eltSize in draw_arrays()");
      }
   }
   else {
      /* Non-indexed prims (draw_arrays).
       * Primitive restart should have been handled in the state tracker.
       */
      draw_pt_arrays(draw, prim, start, count);
   }
}


/**
 * Resolve true values within pipe_draw_info.
 * If we're rendering from transform feedback/stream output
 * buffers both the count and max_index need to be computed
 * from the attached stream output target. 
 */
static void
resolve_draw_info(const struct pipe_draw_info *raw_info,
                  struct pipe_draw_info *info,
                  struct pipe_vertex_buffer *vertex_buffer)
{
   memcpy(info, raw_info, sizeof(struct pipe_draw_info));

   if (raw_info->count_from_stream_output) {
      struct draw_so_target *target =
         (struct draw_so_target *)info->count_from_stream_output;
      assert(vertex_buffer != NULL);
      info->count = target->internal_offset / vertex_buffer->stride;

      /* Stream output draw can not be indexed */
      debug_assert(!info->index_size);
      info->max_index = info->count - 1;
   }
}

/**
 * Draw vertex arrays.
 * This is the main entrypoint into the drawing module.  If drawing an indexed
 * primitive, the draw_set_indexes() function should have already been called
 * to specify the element/index buffer information.
 */
void
draw_vbo(struct draw_context *draw,
         const struct pipe_draw_info *info)
{
   unsigned instance;
   unsigned index_limit;
   unsigned count;
   unsigned fpstate = util_fpstate_get();
   struct pipe_draw_info resolved_info;

   /* Make sure that denorms are treated like zeros. This is 
    * the behavior required by D3D10. OpenGL doesn't care.
    */
   util_fpstate_set_denorms_to_zero(fpstate);

   resolve_draw_info(info, &resolved_info, &(draw->pt.vertex_buffer[0]));
   info = &resolved_info;

   assert(info->instance_count > 0);
   if (info->index_size)
      assert(draw->pt.user.elts);

   count = info->count;

   draw->pt.user.eltBias = info->index_bias;
   draw->pt.user.min_index = info->min_index;
   draw->pt.user.max_index = info->max_index;
   draw->pt.user.eltSize = info->index_size ? draw->pt.user.eltSizeIB : 0;

   if (0)
      debug_printf("draw_vbo(mode=%u start=%u count=%u):\n",
                   info->mode, info->start, count);

   if (0)
      tgsi_dump(draw->vs.vertex_shader->state.tokens, 0);

   if (0) {
      unsigned int i;
      debug_printf("Elements:\n");
      for (i = 0; i < draw->pt.nr_vertex_elements; i++) {
         debug_printf("  %u: src_offset=%u  inst_div=%u   vbuf=%u  format=%s\n",
                      i,
                      draw->pt.vertex_element[i].src_offset,
                      draw->pt.vertex_element[i].instance_divisor,
                      draw->pt.vertex_element[i].vertex_buffer_index,
                      util_format_name(draw->pt.vertex_element[i].src_format));
      }
      debug_printf("Buffers:\n");
      for (i = 0; i < draw->pt.nr_vertex_buffers; i++) {
         debug_printf("  %u: stride=%u offset=%u size=%d ptr=%p\n",
                      i,
                      draw->pt.vertex_buffer[i].stride,
                      draw->pt.vertex_buffer[i].buffer_offset,
                      (int) draw->pt.user.vbuffer[i].size,
                      draw->pt.user.vbuffer[i].map);
      }
   }

   if (0)
      draw_print_arrays(draw, info->mode, info->start, MIN2(count, 20));

   index_limit = util_draw_max_index(draw->pt.vertex_buffer,
                                     draw->pt.vertex_element,
                                     draw->pt.nr_vertex_elements,
                                     info);
#if HAVE_LLVM
   if (!draw->llvm)
#endif
   {
      if (index_limit == 0) {
         /* one of the buffers is too small to do any valid drawing */
         debug_warning("draw: VBO too small to draw anything\n");
         util_fpstate_set(fpstate);
         return;
      }
   }

   /* If we're collecting stats then make sure we start from scratch */
   if (draw->collect_statistics) {
      memset(&draw->statistics, 0, sizeof(draw->statistics));
   }

   draw->pt.max_index = index_limit - 1;
   draw->start_index = info->start;

   /*
    * TODO: We could use draw->pt.max_index to further narrow
    * the min_index/max_index hints given by the state tracker.
    */

   for (instance = 0; instance < info->instance_count; instance++) {
      unsigned instance_idx = instance + info->start_instance;
      draw->start_instance = info->start_instance;
      draw->instance_id = instance;
      /* check for overflow */
      if (instance_idx < instance ||
          instance_idx < draw->start_instance) {
         /* if we overflown just set the instance id to the max */
         draw->instance_id = 0xffffffff;
      }

      draw_new_instance(draw);

      if (info->primitive_restart) {
         draw_pt_arrays_restart(draw, info);
      }
      else {
         draw_pt_arrays(draw, info->mode, info->start, count);
      }
   }

   /* If requested emit the pipeline statistics for this run */
   if (draw->collect_statistics) {
      draw->render->pipeline_statistics(draw->render, &draw->statistics);
   }
   util_fpstate_set(fpstate);
}
