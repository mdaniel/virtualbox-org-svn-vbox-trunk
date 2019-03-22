/*
 * Copyright © 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file gen7_sol_state.c
 *
 * Controls the stream output logic (SOL) stage of the gen7 hardware, which is
 * used to implement GL_EXT_transform_feedback.
 */

#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"
#include "intel_batchbuffer.h"
#include "intel_buffer_objects.h"
#include "main/transformfeedback.h"

void
gen7_begin_transform_feedback(struct gl_context *ctx, GLenum mode,
                              struct gl_transform_feedback_object *obj)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_transform_feedback_object *brw_obj =
      (struct brw_transform_feedback_object *) obj;
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen == 7);

   /* We're about to lose the information needed to compute the number of
    * vertices written during the last Begin/EndTransformFeedback section,
    * so we can't delay it any further.
    */
   brw_compute_xfb_vertices_written(brw, brw_obj);

   /* No primitives have been generated yet. */
   for (int i = 0; i < BRW_MAX_XFB_STREAMS; i++) {
      brw_obj->prims_generated[i] = 0;
   }

   /* Store the starting value of the SO_NUM_PRIMS_WRITTEN counters. */
   brw_save_primitives_written_counters(brw, brw_obj);

   /* Reset the SO buffer offsets to 0. */
   if (!can_do_pipelined_register_writes(brw->screen)) {
      intel_batchbuffer_flush(brw);
      brw->batch.needs_sol_reset = true;
   } else {
      for (int i = 0; i < 4; i++) {
         BEGIN_BATCH(3);
         OUT_BATCH(MI_LOAD_REGISTER_IMM | (3 - 2));
         OUT_BATCH(GEN7_SO_WRITE_OFFSET(i));
         OUT_BATCH(0);
         ADVANCE_BATCH();
      }
   }

   brw_obj->primitive_mode = mode;
}

void
gen7_end_transform_feedback(struct gl_context *ctx,
			    struct gl_transform_feedback_object *obj)
{
   /* After EndTransformFeedback, it's likely that the client program will try
    * to draw using the contents of the transform feedback buffer as vertex
    * input.  In order for this to work, we need to flush the data through at
    * least the GS stage of the pipeline, and flush out the render cache.  For
    * simplicity, just do a full flush.
    */
   struct brw_context *brw = brw_context(ctx);
   struct brw_transform_feedback_object *brw_obj =
      (struct brw_transform_feedback_object *) obj;

   /* Store the ending value of the SO_NUM_PRIMS_WRITTEN counters. */
   if (!obj->Paused)
      brw_save_primitives_written_counters(brw, brw_obj);

   /* EndTransformFeedback() means that we need to update the number of
    * vertices written.  Since it's only necessary if DrawTransformFeedback()
    * is called and it means mapping a buffer object, we delay computing it
    * until it's absolutely necessary to try and avoid stalls.
    */
   brw_obj->vertices_written_valid = false;
}

void
gen7_pause_transform_feedback(struct gl_context *ctx,
                              struct gl_transform_feedback_object *obj)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_transform_feedback_object *brw_obj =
      (struct brw_transform_feedback_object *) obj;
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   /* Flush any drawing so that the counters have the right values. */
   brw_emit_mi_flush(brw);

   assert(devinfo->gen == 7);

   /* Save the SOL buffer offset register values. */
   for (int i = 0; i < 4; i++) {
      BEGIN_BATCH(3);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (3 - 2));
      OUT_BATCH(GEN7_SO_WRITE_OFFSET(i));
      OUT_RELOC(brw_obj->offset_bo, RELOC_WRITE, i * sizeof(uint32_t));
      ADVANCE_BATCH();
   }

   /* Store the temporary ending value of the SO_NUM_PRIMS_WRITTEN counters.
    * While this operation is paused, other transform feedback actions may
    * occur, which will contribute to the counters.  We need to exclude that
    * from our counts.
    */
   brw_save_primitives_written_counters(brw, brw_obj);
}

void
gen7_resume_transform_feedback(struct gl_context *ctx,
                               struct gl_transform_feedback_object *obj)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_transform_feedback_object *brw_obj =
      (struct brw_transform_feedback_object *) obj;
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen == 7);

   /* Reload the SOL buffer offset registers. */
   for (int i = 0; i < 4; i++) {
      BEGIN_BATCH(3);
      OUT_BATCH(GEN7_MI_LOAD_REGISTER_MEM | (3 - 2));
      OUT_BATCH(GEN7_SO_WRITE_OFFSET(i));
      OUT_RELOC(brw_obj->offset_bo, RELOC_WRITE, i * sizeof(uint32_t));
      ADVANCE_BATCH();
   }

   /* Store the new starting value of the SO_NUM_PRIMS_WRITTEN counters. */
   brw_save_primitives_written_counters(brw, brw_obj);
}
