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

#include "main/macros.h"
#include "intel_batchbuffer.h"
#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"

#include "common/gen_l3_config.h"

/**
 * The following diagram shows how we partition the URB:
 *
 *        16kB or 32kB               Rest of the URB space
 *   __________-__________   _________________-_________________
 *  /                     \ /                                   \
 * +-------------------------------------------------------------+
 * |  VS/HS/DS/GS/FS Push  |           VS/HS/DS/GS URB           |
 * |       Constants       |               Entries               |
 * +-------------------------------------------------------------+
 *
 * Notably, push constants must be stored at the beginning of the URB
 * space, while entries can be stored anywhere.  Ivybridge and Haswell
 * GT1/GT2 have a maximum constant buffer size of 16kB, while Haswell GT3
 * doubles this (32kB).
 *
 * Ivybridge and Haswell GT1/GT2 allow push constants to be located (and
 * sized) in increments of 1kB.  Haswell GT3 requires them to be located and
 * sized in increments of 2kB.
 *
 * Currently we split the constant buffer space evenly among whatever stages
 * are active.  This is probably not ideal, but simple.
 *
 * Ivybridge GT1 and Haswell GT1 have 128kB of URB space.
 * Ivybridge GT2 and Haswell GT2 have 256kB of URB space.
 * Haswell GT3 has 512kB of URB space.
 *
 * See "Volume 2a: 3D Pipeline," section 1.8, "Volume 1b: Configurations",
 * and the documentation for 3DSTATE_PUSH_CONSTANT_ALLOC_xS.
 */
static void
gen7_allocate_push_constants(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   /* BRW_NEW_GEOMETRY_PROGRAM */
   bool gs_present = brw->programs[MESA_SHADER_GEOMETRY];

   /* BRW_NEW_TESS_PROGRAMS */
   bool tess_present = brw->programs[MESA_SHADER_TESS_EVAL];

   unsigned avail_size = 16;
   unsigned multiplier =
      (devinfo->gen >= 8 || (devinfo->is_haswell && devinfo->gt == 3)) ? 2 : 1;

   int stages = 2 + gs_present + 2 * tess_present;

   /* Divide up the available space equally between stages.  Because we
    * round down (using floor division), there may be some left over
    * space.  We allocate that to the pixel shader stage.
    */
   unsigned size_per_stage = avail_size / stages;

   unsigned vs_size = size_per_stage;
   unsigned hs_size = tess_present ? size_per_stage : 0;
   unsigned ds_size = tess_present ? size_per_stage : 0;
   unsigned gs_size = gs_present ? size_per_stage : 0;
   unsigned fs_size = avail_size - size_per_stage * (stages - 1);

   gen7_emit_push_constant_state(brw, multiplier * vs_size,
                                 multiplier * hs_size, multiplier * ds_size,
                                 multiplier * gs_size, multiplier * fs_size);

   /* From p115 of the Ivy Bridge PRM (3.2.1.4 3DSTATE_PUSH_CONSTANT_ALLOC_VS):
    *
    *     Programming Restriction:
    *
    *     The 3DSTATE_CONSTANT_VS must be reprogrammed prior to the next
    *     3DPRIMITIVE command after programming the
    *     3DSTATE_PUSH_CONSTANT_ALLOC_VS.
    *
    * Similar text exists for the other 3DSTATE_PUSH_CONSTANT_ALLOC_*
    * commands.
    */
   brw->vs.base.push_constants_dirty = true;
   brw->tcs.base.push_constants_dirty = true;
   brw->tes.base.push_constants_dirty = true;
   brw->gs.base.push_constants_dirty = true;
   brw->wm.base.push_constants_dirty = true;
}

void
gen7_emit_push_constant_state(struct brw_context *brw, unsigned vs_size,
                              unsigned hs_size, unsigned ds_size,
                              unsigned gs_size, unsigned fs_size)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   unsigned offset = 0;

   BEGIN_BATCH(10);
   OUT_BATCH(_3DSTATE_PUSH_CONSTANT_ALLOC_VS << 16 | (2 - 2));
   OUT_BATCH(vs_size | offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT);
   offset += vs_size;

   OUT_BATCH(_3DSTATE_PUSH_CONSTANT_ALLOC_HS << 16 | (2 - 2));
   OUT_BATCH(hs_size | offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT);
   offset += hs_size;

   OUT_BATCH(_3DSTATE_PUSH_CONSTANT_ALLOC_DS << 16 | (2 - 2));
   OUT_BATCH(ds_size | offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT);
   offset += ds_size;

   OUT_BATCH(_3DSTATE_PUSH_CONSTANT_ALLOC_GS << 16 | (2 - 2));
   OUT_BATCH(gs_size | offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT);
   offset += gs_size;

   OUT_BATCH(_3DSTATE_PUSH_CONSTANT_ALLOC_PS << 16 | (2 - 2));
   OUT_BATCH(fs_size | offset << GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT);
   ADVANCE_BATCH();

   /* From p292 of the Ivy Bridge PRM (11.2.4 3DSTATE_PUSH_CONSTANT_ALLOC_PS):
    *
    *     A PIPE_CONTROL command with the CS Stall bit set must be programmed
    *     in the ring after this instruction.
    *
    * No such restriction exists for Haswell or Baytrail.
    */
   if (devinfo->gen < 8 && !devinfo->is_haswell && !devinfo->is_baytrail)
      gen7_emit_cs_stall_flush(brw);
}

const struct brw_tracked_state gen7_push_constant_space = {
   .dirty = {
      .mesa = 0,
      .brw = BRW_NEW_CONTEXT |
             BRW_NEW_GEOMETRY_PROGRAM |
             BRW_NEW_TESS_PROGRAMS,
   },
   .emit = gen7_allocate_push_constants,
};

static void
upload_urb(struct brw_context *brw)
{
   /* BRW_NEW_VS_PROG_DATA */
   const struct brw_vue_prog_data *vs_vue_prog_data =
      brw_vue_prog_data(brw->vs.base.prog_data);
   const unsigned vs_size = MAX2(vs_vue_prog_data->urb_entry_size, 1);
   /* BRW_NEW_GS_PROG_DATA */
   const bool gs_present = brw->gs.base.prog_data;
   /* BRW_NEW_TES_PROG_DATA */
   const bool tess_present = brw->tes.base.prog_data;

   gen7_upload_urb(brw, vs_size, gs_present, tess_present);
}

void
gen7_upload_urb(struct brw_context *brw, unsigned vs_size,
                bool gs_present, bool tess_present)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   const int push_size_kB =
      (devinfo->gen >= 8 || (devinfo->is_haswell && devinfo->gt == 3)) ? 32 : 16;

   /* BRW_NEW_{VS,TCS,TES,GS}_PROG_DATA */
   struct brw_vue_prog_data *prog_data[4] = {
      [MESA_SHADER_VERTEX] =
         brw_vue_prog_data(brw->vs.base.prog_data),
      [MESA_SHADER_TESS_CTRL] =
         tess_present ? brw_vue_prog_data(brw->tcs.base.prog_data) : NULL,
      [MESA_SHADER_TESS_EVAL] =
         tess_present ? brw_vue_prog_data(brw->tes.base.prog_data) : NULL,
      [MESA_SHADER_GEOMETRY] =
         gs_present ? brw_vue_prog_data(brw->gs.base.prog_data) : NULL,
   };

   unsigned entry_size[4];
   entry_size[MESA_SHADER_VERTEX] = vs_size;
   for (int i = MESA_SHADER_TESS_CTRL; i <= MESA_SHADER_GEOMETRY; i++) {
      entry_size[i] = prog_data[i] ? prog_data[i]->urb_entry_size : 1;
   }

   /* If we're just switching between programs with the same URB requirements,
    * skip the rest of the logic.
    */
   if (brw->urb.vsize == entry_size[MESA_SHADER_VERTEX] &&
       brw->urb.gs_present == gs_present &&
       brw->urb.gsize == entry_size[MESA_SHADER_GEOMETRY] &&
       brw->urb.tess_present == tess_present &&
       brw->urb.hsize == entry_size[MESA_SHADER_TESS_CTRL] &&
       brw->urb.dsize == entry_size[MESA_SHADER_TESS_EVAL]) {
      return;
   }
   brw->urb.vsize = entry_size[MESA_SHADER_VERTEX];
   brw->urb.gs_present = gs_present;
   brw->urb.gsize = entry_size[MESA_SHADER_GEOMETRY];
   brw->urb.tess_present = tess_present;
   brw->urb.hsize = entry_size[MESA_SHADER_TESS_CTRL];
   brw->urb.dsize = entry_size[MESA_SHADER_TESS_EVAL];

   unsigned entries[4];
   unsigned start[4];
   gen_get_urb_config(devinfo, 1024 * push_size_kB, 1024 * brw->urb.size,
                      tess_present, gs_present, entry_size, entries, start);

   if (devinfo->gen == 7 && !devinfo->is_haswell && !devinfo->is_baytrail)
      gen7_emit_vs_workaround_flush(brw);

   BEGIN_BATCH(8);
   for (int i = MESA_SHADER_VERTEX; i <= MESA_SHADER_GEOMETRY; i++) {
      assert(devinfo->gen != 10 || entry_size[i] % 3);
      OUT_BATCH((_3DSTATE_URB_VS + i) << 16 | (2 - 2));
      OUT_BATCH(entries[i] |
                ((entry_size[i] - 1) << GEN7_URB_ENTRY_SIZE_SHIFT) |
                (start[i] << GEN7_URB_STARTING_ADDRESS_SHIFT));
   }
   ADVANCE_BATCH();
}

const struct brw_tracked_state gen7_urb = {
   .dirty = {
      .mesa = 0,
      .brw = BRW_NEW_BLORP |
             BRW_NEW_CONTEXT |
             BRW_NEW_URB_SIZE |
             BRW_NEW_GS_PROG_DATA |
             BRW_NEW_TCS_PROG_DATA |
             BRW_NEW_TES_PROG_DATA |
             BRW_NEW_VS_PROG_DATA,
   },
   .emit = upload_urb,
};
