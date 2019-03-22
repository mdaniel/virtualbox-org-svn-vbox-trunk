/*
 * Copyright © 2009 Intel Corporation
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
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#include "main/macros.h"
#include "intel_batchbuffer.h"
#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"

/**
 * When the GS is not in use, we assign the entire URB space to the VS.  When
 * the GS is in use, we split the URB space evenly between the VS and the GS.
 * This is not ideal, but it's simple.
 *
 *           URB size / 2                   URB size / 2
 *   _____________-______________   _____________-______________
 *  /                            \ /                            \
 * +-------------------------------------------------------------+
 * | Vertex Shader Entries        | Geometry Shader Entries      |
 * +-------------------------------------------------------------+
 *
 * Sandybridge GT1 has 32kB of URB space, while GT2 has 64kB.
 * (See the Sandybridge PRM, Volume 2, Part 1, Section 1.4.7: 3DSTATE_URB.)
 */
void
gen6_upload_urb(struct brw_context *brw, unsigned vs_size,
                bool gs_present, unsigned gs_size)
{
   int nr_vs_entries, nr_gs_entries;
   int total_urb_size = brw->urb.size * 1024; /* in bytes */
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   /* Calculate how many entries fit in each stage's section of the URB */
   if (gs_present) {
      nr_vs_entries = (total_urb_size/2) / (vs_size * 128);
      nr_gs_entries = (total_urb_size/2) / (gs_size * 128);
   } else {
      nr_vs_entries = total_urb_size / (vs_size * 128);
      nr_gs_entries = 0;
   }

   /* Then clamp to the maximum allowed by the hardware */
   if (nr_vs_entries > devinfo->urb.max_entries[MESA_SHADER_VERTEX])
      nr_vs_entries = devinfo->urb.max_entries[MESA_SHADER_VERTEX];

   if (nr_gs_entries > devinfo->urb.max_entries[MESA_SHADER_GEOMETRY])
      nr_gs_entries = devinfo->urb.max_entries[MESA_SHADER_GEOMETRY];

   /* Finally, both must be a multiple of 4 (see 3DSTATE_URB in the PRM). */
   brw->urb.nr_vs_entries = ROUND_DOWN_TO(nr_vs_entries, 4);
   brw->urb.nr_gs_entries = ROUND_DOWN_TO(nr_gs_entries, 4);

   assert(brw->urb.nr_vs_entries >=
          devinfo->urb.min_entries[MESA_SHADER_VERTEX]);
   assert(brw->urb.nr_vs_entries % 4 == 0);
   assert(brw->urb.nr_gs_entries % 4 == 0);
   assert(vs_size <= 5);
   assert(gs_size <= 5);

   BEGIN_BATCH(3);
   OUT_BATCH(_3DSTATE_URB << 16 | (3 - 2));
   OUT_BATCH(((vs_size - 1) << GEN6_URB_VS_SIZE_SHIFT) |
	     ((brw->urb.nr_vs_entries) << GEN6_URB_VS_ENTRIES_SHIFT));
   OUT_BATCH(((gs_size - 1) << GEN6_URB_GS_SIZE_SHIFT) |
	     ((brw->urb.nr_gs_entries) << GEN6_URB_GS_ENTRIES_SHIFT));
   ADVANCE_BATCH();

   /* From the PRM Volume 2 part 1, section 1.4.7:
    *
    *   Because of a urb corruption caused by allocating a previous gsunit’s
    *   urb entry to vsunit software is required to send a "GS NULL
    *   Fence"(Send URB fence with VS URB size == 1 and GS URB size == 0) plus
    *   a dummy DRAW call before any case where VS will be taking over GS URB
    *   space.
    *
    * It is not clear exactly what this means ("URB fence" is a command that
    * doesn't exist on Gen6).  So for now we just do a full pipeline flush as
    * a workaround.
    */
   if (brw->urb.gs_present && !gs_present)
      brw_emit_mi_flush(brw);
   brw->urb.gs_present = gs_present;
}

static void
upload_urb(struct brw_context *brw)
{
   /* BRW_NEW_VS_PROG_DATA */
   const struct brw_vue_prog_data *vs_vue_prog_data =
      brw_vue_prog_data(brw->vs.base.prog_data);
   const unsigned vs_size = MAX2(vs_vue_prog_data->urb_entry_size, 1);

   /* BRW_NEW_GEOMETRY_PROGRAM, BRW_NEW_GS_PROG_DATA */
   const bool gs_present =
      brw->ff_gs.prog_active || brw->programs[MESA_SHADER_GEOMETRY];

   /* Whe using GS to do transform feedback only we use the same VUE layout for
    * VS outputs and GS outputs (as it's what the SF and Clipper expect), so we
    * can simply make the GS URB entry size the same as for the VS.  This may
    * technically be too large in cases where we have few vertex attributes and
    * a lot of varyings, since the VS size is determined by the larger of the
    * two. For now, it's safe.
    *
    * For user-provided GS the assumption above does not hold since the GS
    * outputs can be different from the VS outputs.
    */
   unsigned gs_size = vs_size;
   if (brw->programs[MESA_SHADER_GEOMETRY]) {
      const struct brw_vue_prog_data *gs_vue_prog_data =
         brw_vue_prog_data(brw->gs.base.prog_data);
      gs_size = gs_vue_prog_data->urb_entry_size;
      assert(gs_size >= 1);
   }

   gen6_upload_urb(brw, vs_size, gs_present, gs_size);
}

const struct brw_tracked_state gen6_urb = {
   .dirty = {
      .mesa = 0,
      .brw = BRW_NEW_BLORP |
             BRW_NEW_CONTEXT |
             BRW_NEW_FF_GS_PROG_DATA |
             BRW_NEW_GEOMETRY_PROGRAM |
             BRW_NEW_GS_PROG_DATA |
             BRW_NEW_VS_PROG_DATA,
   },
   .emit = upload_urb,
};
