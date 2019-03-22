/*
 * Copyright © 2010 Intel Corporation
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

#include "brw_context.h"
#include "brw_defines.h"
#include "intel_batchbuffer.h"
#include "intel_fbo.h"

/**
 * According to the latest documentation, any PIPE_CONTROL with the
 * "Command Streamer Stall" bit set must also have another bit set,
 * with five different options:
 *
 *  - Render Target Cache Flush
 *  - Depth Cache Flush
 *  - Stall at Pixel Scoreboard
 *  - Post-Sync Operation
 *  - Depth Stall
 *  - DC Flush Enable
 *
 * I chose "Stall at Pixel Scoreboard" since we've used it effectively
 * in the past, but the choice is fairly arbitrary.
 */
static void
gen8_add_cs_stall_workaround_bits(uint32_t *flags)
{
   uint32_t wa_bits = PIPE_CONTROL_RENDER_TARGET_FLUSH |
                      PIPE_CONTROL_DEPTH_CACHE_FLUSH |
                      PIPE_CONTROL_WRITE_IMMEDIATE |
                      PIPE_CONTROL_WRITE_DEPTH_COUNT |
                      PIPE_CONTROL_WRITE_TIMESTAMP |
                      PIPE_CONTROL_STALL_AT_SCOREBOARD |
                      PIPE_CONTROL_DEPTH_STALL |
                      PIPE_CONTROL_DATA_CACHE_FLUSH;

   /* If we're doing a CS stall, and don't already have one of the
    * workaround bits set, add "Stall at Pixel Scoreboard."
    */
   if ((*flags & PIPE_CONTROL_CS_STALL) != 0 && (*flags & wa_bits) == 0)
      *flags |= PIPE_CONTROL_STALL_AT_SCOREBOARD;
}

/* Implement the WaCsStallAtEveryFourthPipecontrol workaround on IVB, BYT:
 *
 * "Every 4th PIPE_CONTROL command, not counting the PIPE_CONTROL with
 *  only read-cache-invalidate bit(s) set, must have a CS_STALL bit set."
 *
 * Note that the kernel does CS stalls between batches, so we only need
 * to count them within a batch.
 */
static uint32_t
gen7_cs_stall_every_four_pipe_controls(struct brw_context *brw, uint32_t flags)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   if (devinfo->gen == 7 && !devinfo->is_haswell) {
      if (flags & PIPE_CONTROL_CS_STALL) {
         /* If we're doing a CS stall, reset the counter and carry on. */
         brw->pipe_controls_since_last_cs_stall = 0;
         return 0;
      }

      /* If this is the fourth pipe control without a CS stall, do one now. */
      if (++brw->pipe_controls_since_last_cs_stall == 4) {
         brw->pipe_controls_since_last_cs_stall = 0;
         return PIPE_CONTROL_CS_STALL;
      }
   }
   return 0;
}

static void
brw_emit_pipe_control(struct brw_context *brw, uint32_t flags,
                      struct brw_bo *bo, uint32_t offset, uint64_t imm)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   if (devinfo->gen >= 8) {
      if (devinfo->gen == 8)
         gen8_add_cs_stall_workaround_bits(&flags);

      if (flags & PIPE_CONTROL_VF_CACHE_INVALIDATE) {
         if (devinfo->gen == 9) {
            /* The PIPE_CONTROL "VF Cache Invalidation Enable" bit description
             * lists several workarounds:
             *
             *    "Project: SKL, KBL, BXT
             *
             *     If the VF Cache Invalidation Enable is set to a 1 in a
             *     PIPE_CONTROL, a separate Null PIPE_CONTROL, all bitfields
             *     sets to 0, with the VF Cache Invalidation Enable set to 0
             *     needs to be sent prior to the PIPE_CONTROL with VF Cache
             *     Invalidation Enable set to a 1."
             */
            brw_emit_pipe_control_flush(brw, 0);
         }

         if (devinfo->gen >= 9) {
            /* THE PIPE_CONTROL "VF Cache Invalidation Enable" docs continue:
             *
             *    "Project: BDW+
             *
             *     When VF Cache Invalidate is set “Post Sync Operation” must
             *     be enabled to “Write Immediate Data” or “Write PS Depth
             *     Count” or “Write Timestamp”."
             *
             * If there's a BO, we're already doing some kind of write.
             * If not, add a write to the workaround BO.
             *
             * XXX: This causes GPU hangs on Broadwell, so restrict it to
             *      Gen9+ for now...see this bug for more information:
             *      https://bugs.freedesktop.org/show_bug.cgi?id=103787
             */
            if (!bo) {
               flags |= PIPE_CONTROL_WRITE_IMMEDIATE;
               bo = brw->workaround_bo;
            }
         }
      }

      BEGIN_BATCH(6);
      OUT_BATCH(_3DSTATE_PIPE_CONTROL | (6 - 2));
      OUT_BATCH(flags);
      if (bo) {
         OUT_RELOC64(bo, RELOC_WRITE, offset);
      } else {
         OUT_BATCH(0);
         OUT_BATCH(0);
      }
      OUT_BATCH(imm);
      OUT_BATCH(imm >> 32);
      ADVANCE_BATCH();
   } else if (devinfo->gen >= 6) {
      if (devinfo->gen == 6 &&
          (flags & PIPE_CONTROL_RENDER_TARGET_FLUSH)) {
         /* Hardware workaround: SNB B-Spec says:
          *
          *   [Dev-SNB{W/A}]: Before a PIPE_CONTROL with Write Cache Flush
          *   Enable = 1, a PIPE_CONTROL with any non-zero post-sync-op is
          *   required.
          */
         brw_emit_post_sync_nonzero_flush(brw);
      }

      flags |= gen7_cs_stall_every_four_pipe_controls(brw, flags);

      /* PPGTT/GGTT is selected by DW2 bit 2 on Sandybridge, but DW1 bit 24
       * on later platforms.  We always use PPGTT on Gen7+.
       */
      unsigned gen6_gtt = devinfo->gen == 6 ? PIPE_CONTROL_GLOBAL_GTT_WRITE : 0;

      BEGIN_BATCH(5);
      OUT_BATCH(_3DSTATE_PIPE_CONTROL | (5 - 2));
      OUT_BATCH(flags);
      if (bo) {
         OUT_RELOC(bo, RELOC_WRITE | RELOC_NEEDS_GGTT, gen6_gtt | offset);
      } else {
         OUT_BATCH(0);
      }
      OUT_BATCH(imm);
      OUT_BATCH(imm >> 32);
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(4);
      OUT_BATCH(_3DSTATE_PIPE_CONTROL | flags | (4 - 2));
      if (bo) {
         OUT_RELOC(bo, RELOC_WRITE, PIPE_CONTROL_GLOBAL_GTT_WRITE | offset);
      } else {
         OUT_BATCH(0);
      }
      OUT_BATCH(imm);
      OUT_BATCH(imm >> 32);
      ADVANCE_BATCH();
   }
}

/**
 * Emit a PIPE_CONTROL with various flushing flags.
 *
 * The caller is responsible for deciding what flags are appropriate for the
 * given generation.
 */
void
brw_emit_pipe_control_flush(struct brw_context *brw, uint32_t flags)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   if (devinfo->gen >= 6 &&
       (flags & PIPE_CONTROL_CACHE_FLUSH_BITS) &&
       (flags & PIPE_CONTROL_CACHE_INVALIDATE_BITS)) {
      /* A pipe control command with flush and invalidate bits set
       * simultaneously is an inherently racy operation on Gen6+ if the
       * contents of the flushed caches were intended to become visible from
       * any of the invalidated caches.  Split it in two PIPE_CONTROLs, the
       * first one should stall the pipeline to make sure that the flushed R/W
       * caches are coherent with memory once the specified R/O caches are
       * invalidated.  On pre-Gen6 hardware the (implicit) R/O cache
       * invalidation seems to happen at the bottom of the pipeline together
       * with any write cache flush, so this shouldn't be a concern.  In order
       * to ensure a full stall, we do an end-of-pipe sync.
       */
      brw_emit_end_of_pipe_sync(brw, (flags & PIPE_CONTROL_CACHE_FLUSH_BITS));
      flags &= ~(PIPE_CONTROL_CACHE_FLUSH_BITS | PIPE_CONTROL_CS_STALL);
   }

   brw_emit_pipe_control(brw, flags, NULL, 0, 0);
}

/**
 * Emit a PIPE_CONTROL that writes to a buffer object.
 *
 * \p flags should contain one of the following items:
 *  - PIPE_CONTROL_WRITE_IMMEDIATE
 *  - PIPE_CONTROL_WRITE_TIMESTAMP
 *  - PIPE_CONTROL_WRITE_DEPTH_COUNT
 */
void
brw_emit_pipe_control_write(struct brw_context *brw, uint32_t flags,
                            struct brw_bo *bo, uint32_t offset,
                            uint64_t imm)
{
   brw_emit_pipe_control(brw, flags, bo, offset, imm);
}

/**
 * Restriction [DevSNB, DevIVB]:
 *
 * Prior to changing Depth/Stencil Buffer state (i.e. any combination of
 * 3DSTATE_DEPTH_BUFFER, 3DSTATE_CLEAR_PARAMS, 3DSTATE_STENCIL_BUFFER,
 * 3DSTATE_HIER_DEPTH_BUFFER) SW must first issue a pipelined depth stall
 * (PIPE_CONTROL with Depth Stall bit set), followed by a pipelined depth
 * cache flush (PIPE_CONTROL with Depth Flush Bit set), followed by
 * another pipelined depth stall (PIPE_CONTROL with Depth Stall bit set),
 * unless SW can otherwise guarantee that the pipeline from WM onwards is
 * already flushed (e.g., via a preceding MI_FLUSH).
 */
void
brw_emit_depth_stall_flushes(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   /* Starting on BDW, these pipe controls are unnecessary.
    *
    *   WM HW will internally manage the draining pipe and flushing of the caches
    *   when this command is issued. The PIPE_CONTROL restrictions are removed.
    */
   if (devinfo->gen >= 8)
      return;

   brw_emit_pipe_control_flush(brw, PIPE_CONTROL_DEPTH_STALL);
   brw_emit_pipe_control_flush(brw, PIPE_CONTROL_DEPTH_CACHE_FLUSH);
   brw_emit_pipe_control_flush(brw, PIPE_CONTROL_DEPTH_STALL);
}

/**
 * From the Ivybridge PRM, Volume 2 Part 1, Section 3.2 (VS Stage Input):
 * "A PIPE_CONTROL with Post-Sync Operation set to 1h and a depth
 *  stall needs to be sent just prior to any 3DSTATE_VS, 3DSTATE_URB_VS,
 *  3DSTATE_CONSTANT_VS, 3DSTATE_BINDING_TABLE_POINTER_VS,
 *  3DSTATE_SAMPLER_STATE_POINTER_VS command.  Only one PIPE_CONTROL needs
 *  to be sent before any combination of VS associated 3DSTATE."
 */
void
gen7_emit_vs_workaround_flush(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen == 7);
   brw_emit_pipe_control_write(brw,
                               PIPE_CONTROL_WRITE_IMMEDIATE
                               | PIPE_CONTROL_DEPTH_STALL,
                               brw->workaround_bo, 0, 0);
}


/**
 * Emit a PIPE_CONTROL command for gen7 with the CS Stall bit set.
 */
void
gen7_emit_cs_stall_flush(struct brw_context *brw)
{
   brw_emit_pipe_control_write(brw,
                               PIPE_CONTROL_CS_STALL
                               | PIPE_CONTROL_WRITE_IMMEDIATE,
                               brw->workaround_bo, 0, 0);
}

/**
 * Emits a PIPE_CONTROL with a non-zero post-sync operation, for
 * implementing two workarounds on gen6.  From section 1.4.7.1
 * "PIPE_CONTROL" of the Sandy Bridge PRM volume 2 part 1:
 *
 * [DevSNB-C+{W/A}] Before any depth stall flush (including those
 * produced by non-pipelined state commands), software needs to first
 * send a PIPE_CONTROL with no bits set except Post-Sync Operation !=
 * 0.
 *
 * [Dev-SNB{W/A}]: Before a PIPE_CONTROL with Write Cache Flush Enable
 * =1, a PIPE_CONTROL with any non-zero post-sync-op is required.
 *
 * And the workaround for these two requires this workaround first:
 *
 * [Dev-SNB{W/A}]: Pipe-control with CS-stall bit set must be sent
 * BEFORE the pipe-control with a post-sync op and no write-cache
 * flushes.
 *
 * And this last workaround is tricky because of the requirements on
 * that bit.  From section 1.4.7.2.3 "Stall" of the Sandy Bridge PRM
 * volume 2 part 1:
 *
 *     "1 of the following must also be set:
 *      - Render Target Cache Flush Enable ([12] of DW1)
 *      - Depth Cache Flush Enable ([0] of DW1)
 *      - Stall at Pixel Scoreboard ([1] of DW1)
 *      - Depth Stall ([13] of DW1)
 *      - Post-Sync Operation ([13] of DW1)
 *      - Notify Enable ([8] of DW1)"
 *
 * The cache flushes require the workaround flush that triggered this
 * one, so we can't use it.  Depth stall would trigger the same.
 * Post-sync nonzero is what triggered this second workaround, so we
 * can't use that one either.  Notify enable is IRQs, which aren't
 * really our business.  That leaves only stall at scoreboard.
 */
void
brw_emit_post_sync_nonzero_flush(struct brw_context *brw)
{
   brw_emit_pipe_control_flush(brw,
                               PIPE_CONTROL_CS_STALL |
                               PIPE_CONTROL_STALL_AT_SCOREBOARD);

   brw_emit_pipe_control_write(brw, PIPE_CONTROL_WRITE_IMMEDIATE,
                               brw->workaround_bo, 0, 0);
}

/*
 * From Sandybridge PRM, volume 2, "1.7.2 End-of-Pipe Synchronization":
 *
 *  Write synchronization is a special case of end-of-pipe
 *  synchronization that requires that the render cache and/or depth
 *  related caches are flushed to memory, where the data will become
 *  globally visible. This type of synchronization is required prior to
 *  SW (CPU) actually reading the result data from memory, or initiating
 *  an operation that will use as a read surface (such as a texture
 *  surface) a previous render target and/or depth/stencil buffer
 *
 *
 * From Haswell PRM, volume 2, part 1, "End-of-Pipe Synchronization":
 *
 *  Exercising the write cache flush bits (Render Target Cache Flush
 *  Enable, Depth Cache Flush Enable, DC Flush) in PIPE_CONTROL only
 *  ensures the write caches are flushed and doesn't guarantee the data
 *  is globally visible.
 *
 *  SW can track the completion of the end-of-pipe-synchronization by
 *  using "Notify Enable" and "PostSync Operation - Write Immediate
 *  Data" in the PIPE_CONTROL command.
 */
void
brw_emit_end_of_pipe_sync(struct brw_context *brw, uint32_t flags)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   if (devinfo->gen >= 6) {
      /* From Sandybridge PRM, volume 2, "1.7.3.1 Writing a Value to Memory":
       *
       *    "The most common action to perform upon reaching a synchronization
       *    point is to write a value out to memory. An immediate value
       *    (included with the synchronization command) may be written."
       *
       *
       * From Broadwell PRM, volume 7, "End-of-Pipe Synchronization":
       *
       *    "In case the data flushed out by the render engine is to be read
       *    back in to the render engine in coherent manner, then the render
       *    engine has to wait for the fence completion before accessing the
       *    flushed data. This can be achieved by following means on various
       *    products: PIPE_CONTROL command with CS Stall and the required
       *    write caches flushed with Post-Sync-Operation as Write Immediate
       *    Data.
       *
       *    Example:
       *       - Workload-1 (3D/GPGPU/MEDIA)
       *       - PIPE_CONTROL (CS Stall, Post-Sync-Operation Write Immediate
       *         Data, Required Write Cache Flush bits set)
       *       - Workload-2 (Can use the data produce or output by Workload-1)
       */
      brw_emit_pipe_control_write(brw,
                                  flags | PIPE_CONTROL_CS_STALL |
                                  PIPE_CONTROL_WRITE_IMMEDIATE,
                                  brw->workaround_bo, 0, 0);

      if (devinfo->is_haswell) {
         /* Haswell needs addition work-arounds:
          *
          * From Haswell PRM, volume 2, part 1, "End-of-Pipe Synchronization":
          *
          *    Option 1:
          *    PIPE_CONTROL command with the CS Stall and the required write
          *    caches flushed with Post-SyncOperation as Write Immediate Data
          *    followed by eight dummy MI_STORE_DATA_IMM (write to scratch
          *    spce) commands.
          *
          *    Example:
          *       - Workload-1
          *       - PIPE_CONTROL (CS Stall, Post-Sync-Operation Write
          *         Immediate Data, Required Write Cache Flush bits set)
          *       - MI_STORE_DATA_IMM (8 times) (Dummy data, Scratch Address)
          *       - Workload-2 (Can use the data produce or output by
          *         Workload-1)
          *
          * Unfortunately, both the PRMs and the internal docs are a bit
          * out-of-date in this regard.  What the windows driver does (and
          * this appears to actually work) is to emit a register read from the
          * memory address written by the pipe control above.
          *
          * What register we load into doesn't matter.  We choose an indirect
          * rendering register because we know it always exists and it's one
          * of the first registers the command parser allows us to write.  If
          * you don't have command parser support in your kernel (pre-4.2),
          * this will get turned into MI_NOOP and you won't get the
          * workaround.  Unfortunately, there's just not much we can do in
          * that case.  This register is perfectly safe to write since we
          * always re-load all of the indirect draw registers right before
          * 3DPRIMITIVE when needed anyway.
          */
         brw_load_register_mem(brw, GEN7_3DPRIM_START_INSTANCE,
                               brw->workaround_bo, 0);
      }
   } else {
      /* On gen4-5, a regular pipe control seems to suffice. */
      brw_emit_pipe_control_flush(brw, flags);
   }
}

/* Emit a pipelined flush to either flush render and texture cache for
 * reading from a FBO-drawn texture, or flush so that frontbuffer
 * render appears on the screen in DRI1.
 *
 * This is also used for the always_flush_cache driconf debug option.
 */
void
brw_emit_mi_flush(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   if (brw->batch.ring == BLT_RING && devinfo->gen >= 6) {
      const unsigned n_dwords = devinfo->gen >= 8 ? 5 : 4;
      BEGIN_BATCH_BLT(n_dwords);
      OUT_BATCH(MI_FLUSH_DW | (n_dwords - 2));
      OUT_BATCH(0);
      OUT_BATCH(0);
      OUT_BATCH(0);
      if (n_dwords == 5)
         OUT_BATCH(0);
      ADVANCE_BATCH();
   } else {
      int flags = PIPE_CONTROL_NO_WRITE | PIPE_CONTROL_RENDER_TARGET_FLUSH;
      if (devinfo->gen >= 6) {
         flags |= PIPE_CONTROL_INSTRUCTION_INVALIDATE |
                  PIPE_CONTROL_CONST_CACHE_INVALIDATE |
                  PIPE_CONTROL_DATA_CACHE_FLUSH |
                  PIPE_CONTROL_DEPTH_CACHE_FLUSH |
                  PIPE_CONTROL_VF_CACHE_INVALIDATE |
                  PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE |
                  PIPE_CONTROL_CS_STALL;
      }
      brw_emit_pipe_control_flush(brw, flags);
   }
}

int
brw_init_pipe_control(struct brw_context *brw,
                      const struct gen_device_info *devinfo)
{
   if (devinfo->gen < 6)
      return 0;

   /* We can't just use brw_state_batch to get a chunk of space for
    * the gen6 workaround because it involves actually writing to
    * the buffer, and the kernel doesn't let us write to the batch.
    */
   brw->workaround_bo = brw_bo_alloc(brw->bufmgr,
                                     "pipe_control workaround",
                                     4096, 4096);
   if (brw->workaround_bo == NULL)
      return -ENOMEM;

   brw->pipe_controls_since_last_cs_stall = 0;

   return 0;
}

void
brw_fini_pipe_control(struct brw_context *brw)
{
   brw_bo_unreference(brw->workaround_bo);
}
