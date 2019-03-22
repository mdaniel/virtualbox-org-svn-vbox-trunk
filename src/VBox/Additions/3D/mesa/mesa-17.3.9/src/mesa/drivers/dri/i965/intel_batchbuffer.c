/*
 * Copyright 2006 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "intel_batchbuffer.h"
#include "intel_buffer_objects.h"
#include "brw_bufmgr.h"
#include "intel_buffers.h"
#include "intel_fbo.h"
#include "brw_context.h"
#include "brw_defines.h"
#include "brw_state.h"
#include "common/gen_decoder.h"

#include "util/hash_table.h"

#include <xf86drm.h>
#include <i915_drm.h>

#define FILE_DEBUG_FLAG DEBUG_BUFMGR

/**
 * Target sizes of the batch and state buffers.  We create the initial
 * buffers at these sizes, and flush when they're nearly full.  If we
 * underestimate how close we are to the end, and suddenly need more space
 * in the middle of a draw, we can grow the buffers, and finish the draw.
 * At that point, we'll be over our target size, so the next operation
 * should flush.  Each time we flush the batch, we recreate both buffers
 * at the original target size, so it doesn't grow without bound.
 */
#define BATCH_SZ (20 * 1024)
#define STATE_SZ (16 * 1024)

static void
intel_batchbuffer_reset(struct brw_context *brw);

static bool
uint_key_compare(const void *a, const void *b)
{
   return a == b;
}

static uint32_t
uint_key_hash(const void *key)
{
   return (uintptr_t) key;
}

static void
init_reloc_list(struct brw_reloc_list *rlist, int count)
{
   rlist->reloc_count = 0;
   rlist->reloc_array_size = count;
   rlist->relocs = malloc(rlist->reloc_array_size *
                          sizeof(struct drm_i915_gem_relocation_entry));
}

void
intel_batchbuffer_init(struct brw_context *brw)
{
   struct intel_screen *screen = brw->screen;
   struct intel_batchbuffer *batch = &brw->batch;
   const struct gen_device_info *devinfo = &screen->devinfo;

   if (!devinfo->has_llc) {
      batch->batch.cpu_map = malloc(BATCH_SZ);
      batch->batch.map = batch->batch.cpu_map;
      batch->map_next = batch->batch.map;
      batch->state.cpu_map = malloc(STATE_SZ);
      batch->state.map = batch->state.cpu_map;
   }

   init_reloc_list(&batch->batch_relocs, 250);
   init_reloc_list(&batch->state_relocs, 250);

   batch->exec_count = 0;
   batch->exec_array_size = 100;
   batch->exec_bos =
      malloc(batch->exec_array_size * sizeof(batch->exec_bos[0]));
   batch->validation_list =
      malloc(batch->exec_array_size * sizeof(batch->validation_list[0]));

   if (INTEL_DEBUG & DEBUG_BATCH) {
      batch->state_batch_sizes =
         _mesa_hash_table_create(NULL, uint_key_hash, uint_key_compare);
   }

   batch->use_batch_first =
      screen->kernel_features & KERNEL_ALLOWS_EXEC_BATCH_FIRST;

   /* PIPE_CONTROL needs a w/a but only on gen6 */
   batch->valid_reloc_flags = EXEC_OBJECT_WRITE;
   if (devinfo->gen == 6)
      batch->valid_reloc_flags |= EXEC_OBJECT_NEEDS_GTT;

   intel_batchbuffer_reset(brw);
}

#define READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))

static unsigned
add_exec_bo(struct intel_batchbuffer *batch, struct brw_bo *bo)
{
   unsigned index = READ_ONCE(bo->index);

   if (index < batch->exec_count && batch->exec_bos[index] == bo)
      return index;

   /* May have been shared between multiple active batches */
   for (index = 0; index < batch->exec_count; index++) {
      if (batch->exec_bos[index] == bo)
         return index;
   }

   brw_bo_reference(bo);

   if (batch->exec_count == batch->exec_array_size) {
      batch->exec_array_size *= 2;
      batch->exec_bos =
         realloc(batch->exec_bos,
                 batch->exec_array_size * sizeof(batch->exec_bos[0]));
      batch->validation_list =
         realloc(batch->validation_list,
                 batch->exec_array_size * sizeof(batch->validation_list[0]));
   }

   batch->validation_list[batch->exec_count] =
      (struct drm_i915_gem_exec_object2) {
         .handle = bo->gem_handle,
         .alignment = bo->align,
         .offset = bo->gtt_offset,
         .flags = bo->kflags,
      };

   bo->index = batch->exec_count;
   batch->exec_bos[batch->exec_count] = bo;
   batch->aperture_space += bo->size;

   return batch->exec_count++;
}

static void
intel_batchbuffer_reset(struct brw_context *brw)
{
   struct intel_screen *screen = brw->screen;
   struct intel_batchbuffer *batch = &brw->batch;
   struct brw_bufmgr *bufmgr = screen->bufmgr;

   if (batch->last_bo != NULL) {
      brw_bo_unreference(batch->last_bo);
      batch->last_bo = NULL;
   }
   batch->last_bo = batch->batch.bo;

   batch->batch.bo = brw_bo_alloc(bufmgr, "batchbuffer", BATCH_SZ, 4096);
   if (!batch->batch.cpu_map) {
      batch->batch.map =
         brw_bo_map(brw, batch->batch.bo, MAP_READ | MAP_WRITE);
   }
   batch->map_next = batch->batch.map;

   batch->state.bo = brw_bo_alloc(bufmgr, "statebuffer", STATE_SZ, 4096);
   batch->state.bo->kflags =
      can_do_exec_capture(screen) ? EXEC_OBJECT_CAPTURE : 0;
   if (!batch->state.cpu_map) {
      batch->state.map =
         brw_bo_map(brw, batch->state.bo, MAP_READ | MAP_WRITE);
   }

   /* Avoid making 0 a valid state offset - otherwise the decoder will try
    * and decode data when we use offset 0 as a null pointer.
    */
   batch->state_used = 1;

   add_exec_bo(batch, batch->batch.bo);
   assert(batch->batch.bo->index == 0);

   batch->needs_sol_reset = false;
   batch->state_base_address_emitted = false;

   /* We don't know what ring the new batch will be sent to until we see the
    * first BEGIN_BATCH or BEGIN_BATCH_BLT.  Mark it as unknown.
    */
   batch->ring = UNKNOWN_RING;

   if (batch->state_batch_sizes)
      _mesa_hash_table_clear(batch->state_batch_sizes, NULL);
}

static void
intel_batchbuffer_reset_and_clear_render_cache(struct brw_context *brw)
{
   intel_batchbuffer_reset(brw);
   brw_cache_sets_clear(brw);
}

void
intel_batchbuffer_save_state(struct brw_context *brw)
{
   brw->batch.saved.map_next = brw->batch.map_next;
   brw->batch.saved.batch_reloc_count = brw->batch.batch_relocs.reloc_count;
   brw->batch.saved.state_reloc_count = brw->batch.state_relocs.reloc_count;
   brw->batch.saved.exec_count = brw->batch.exec_count;
}

void
intel_batchbuffer_reset_to_saved(struct brw_context *brw)
{
   for (int i = brw->batch.saved.exec_count;
        i < brw->batch.exec_count; i++) {
      brw_bo_unreference(brw->batch.exec_bos[i]);
   }
   brw->batch.batch_relocs.reloc_count = brw->batch.saved.batch_reloc_count;
   brw->batch.state_relocs.reloc_count = brw->batch.saved.state_reloc_count;
   brw->batch.exec_count = brw->batch.saved.exec_count;

   brw->batch.map_next = brw->batch.saved.map_next;
   if (USED_BATCH(brw->batch) == 0)
      brw->batch.ring = UNKNOWN_RING;
}

void
intel_batchbuffer_free(struct intel_batchbuffer *batch)
{
   free(batch->batch.cpu_map);
   free(batch->state.cpu_map);

   for (int i = 0; i < batch->exec_count; i++) {
      brw_bo_unreference(batch->exec_bos[i]);
   }
   free(batch->batch_relocs.relocs);
   free(batch->state_relocs.relocs);
   free(batch->exec_bos);
   free(batch->validation_list);

   brw_bo_unreference(batch->last_bo);
   brw_bo_unreference(batch->batch.bo);
   brw_bo_unreference(batch->state.bo);
   if (batch->state_batch_sizes)
      _mesa_hash_table_destroy(batch->state_batch_sizes, NULL);
}

static void
replace_bo_in_reloc_list(struct brw_reloc_list *rlist,
                         uint32_t old_handle, uint32_t new_handle)
{
   for (int i = 0; i < rlist->reloc_count; i++) {
      if (rlist->relocs[i].target_handle == old_handle)
         rlist->relocs[i].target_handle = new_handle;
   }
}

/**
 * Grow either the batch or state buffer to a new larger size.
 *
 * We can't actually grow buffers, so we allocate a new one, copy over
 * the existing contents, and update our lists to refer to the new one.
 *
 * Note that this is only temporary - each new batch recreates the buffers
 * at their original target size (BATCH_SZ or STATE_SZ).
 */
static void
grow_buffer(struct brw_context *brw,
            struct brw_bo **bo_ptr,
            uint32_t **map_ptr,
            uint32_t **cpu_map_ptr,
            unsigned existing_bytes,
            unsigned new_size)
{
   struct intel_batchbuffer *batch = &brw->batch;
   struct brw_bufmgr *bufmgr = brw->bufmgr;

   uint32_t *old_map = *map_ptr;
   struct brw_bo *old_bo = *bo_ptr;

   struct brw_bo *new_bo =
      brw_bo_alloc(bufmgr, old_bo->name, new_size, old_bo->align);
   uint32_t *new_map;

   perf_debug("Growing %s - ran out of space\n", old_bo->name);

   /* Copy existing data to the new larger buffer */
   if (*cpu_map_ptr) {
      *cpu_map_ptr = new_map = realloc(*cpu_map_ptr, new_size);
   } else {
      new_map = brw_bo_map(brw, new_bo, MAP_READ | MAP_WRITE);
      memcpy(new_map, old_map, existing_bytes);
   }

   /* Try to put the new BO at the same GTT offset as the old BO (which
    * we're throwing away, so it doesn't need to be there).
    *
    * This guarantees that our relocations continue to work: values we've
    * already written into the buffer, values we're going to write into the
    * buffer, and the validation/relocation lists all will match.
    *
    * Also preserve kflags for EXEC_OBJECT_CAPTURE.
    */
   new_bo->gtt_offset = old_bo->gtt_offset;
   new_bo->index = old_bo->index;
   new_bo->kflags = old_bo->kflags;

   /* Batch/state buffers are per-context, and if we've run out of space,
    * we must have actually used them before, so...they will be in the list.
    */
   assert(old_bo->index < batch->exec_count);
   assert(batch->exec_bos[old_bo->index] == old_bo);

   /* Update the validation list to use the new BO. */
   batch->exec_bos[old_bo->index] = new_bo;
   batch->validation_list[old_bo->index].handle = new_bo->gem_handle;
   brw_bo_reference(new_bo);
   brw_bo_unreference(old_bo);

   if (!batch->use_batch_first) {
      /* We're not using I915_EXEC_HANDLE_LUT, which means we need to go
       * update the relocation list entries to point at the new BO as well.
       * (With newer kernels, the "handle" is an offset into the validation
       * list, which remains unchanged, so we can skip this.)
       */
      replace_bo_in_reloc_list(&batch->batch_relocs,
                               old_bo->gem_handle, new_bo->gem_handle);
      replace_bo_in_reloc_list(&batch->state_relocs,
                               old_bo->gem_handle, new_bo->gem_handle);
   }

   /* Drop the *bo_ptr reference.  This should free the old BO. */
   brw_bo_unreference(old_bo);

   *bo_ptr = new_bo;
   *map_ptr = new_map;
}

void
intel_batchbuffer_require_space(struct brw_context *brw, GLuint sz,
                                enum brw_gpu_ring ring)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   struct intel_batchbuffer *batch = &brw->batch;

   /* If we're switching rings, implicitly flush the batch. */
   if (unlikely(ring != brw->batch.ring) && brw->batch.ring != UNKNOWN_RING &&
       devinfo->gen >= 6) {
      intel_batchbuffer_flush(brw);
   }

   const unsigned batch_used = USED_BATCH(*batch) * 4;
   if (batch_used + sz >= BATCH_SZ && !batch->no_wrap) {
      intel_batchbuffer_flush(brw);
   } else if (batch_used + sz >= batch->batch.bo->size) {
      const unsigned new_size =
         MIN2(batch->batch.bo->size + batch->batch.bo->size / 2,
              MAX_BATCH_SIZE);
      grow_buffer(brw, &batch->batch.bo, &batch->batch.map,
                  &batch->batch.cpu_map, batch_used, new_size);
      batch->map_next = (void *) batch->batch.map + batch_used;
      assert(batch_used + sz < batch->batch.bo->size);
   }

   /* The intel_batchbuffer_flush() calls above might have changed
    * brw->batch.ring to UNKNOWN_RING, so we need to set it here at the end.
    */
   brw->batch.ring = ring;
}

#ifdef DEBUG
#define CSI "\e["
#define BLUE_HEADER  CSI "0;44m"
#define NORMAL       CSI "0m"


static void
decode_struct(struct brw_context *brw, struct gen_spec *spec,
              const char *struct_name, uint32_t *data,
              uint32_t gtt_offset, uint32_t offset, bool color)
{
   struct gen_group *group = gen_spec_find_struct(spec, struct_name);
   if (!group)
      return;

   fprintf(stderr, "%s\n", struct_name);
   gen_print_group(stderr, group, gtt_offset + offset,
                   &data[offset / 4], color);
}

static void
decode_structs(struct brw_context *brw, struct gen_spec *spec,
               const char *struct_name,
               uint32_t *data, uint32_t gtt_offset, uint32_t offset,
               int struct_size, bool color)
{
   struct gen_group *group = gen_spec_find_struct(spec, struct_name);
   if (!group)
      return;

   int entries = brw_state_batch_size(brw, offset) / struct_size;
   for (int i = 0; i < entries; i++) {
      fprintf(stderr, "%s %d\n", struct_name, i);
      gen_print_group(stderr, group, gtt_offset + offset,
                      &data[(offset + i * struct_size) / 4], color);
   }
}

static void
do_batch_dump(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   struct intel_batchbuffer *batch = &brw->batch;
   struct gen_spec *spec = gen_spec_load(&brw->screen->devinfo);

   if (batch->ring != RENDER_RING)
      return;

   uint32_t *batch_data = brw_bo_map(brw, batch->batch.bo, MAP_READ);
   uint32_t *state = brw_bo_map(brw, batch->state.bo, MAP_READ);
   if (batch_data == NULL || state == NULL) {
      fprintf(stderr, "WARNING: failed to map batchbuffer/statebuffer\n");
      return;
   }

   uint32_t *end = batch_data + USED_BATCH(*batch);
   uint32_t batch_gtt_offset = batch->batch.bo->gtt_offset;
   uint32_t state_gtt_offset = batch->state.bo->gtt_offset;
   int length;

   bool color = INTEL_DEBUG & DEBUG_COLOR;
   const char *header_color = color ? BLUE_HEADER : "";
   const char *reset_color  = color ? NORMAL : "";

   for (uint32_t *p = batch_data; p < end; p += length) {
      struct gen_group *inst = gen_spec_find_instruction(spec, p);
      length = gen_group_get_length(inst, p);
      assert(inst == NULL || length > 0);
      length = MAX2(1, length);
      if (inst == NULL) {
         fprintf(stderr, "unknown instruction %08x\n", p[0]);
         continue;
      }

      uint64_t offset = batch_gtt_offset + 4 * (p - batch_data);

      fprintf(stderr, "%s0x%08"PRIx64":  0x%08x:  %-80s%s\n", header_color,
              offset, p[0], gen_group_get_name(inst), reset_color);

      gen_print_group(stderr, inst, offset, p, color);

      switch (gen_group_get_opcode(inst) >> 16) {
      case _3DSTATE_PIPELINED_POINTERS:
         /* Note: these Gen4-5 pointers are full relocations rather than
          * offsets from the start of the statebuffer.  So we need to subtract
          * gtt_offset (the start of the statebuffer) to obtain an offset we
          * can add to the map and get at the data.
          */
         decode_struct(brw, spec, "VS_STATE", state, state_gtt_offset,
                       (p[1] & ~0x1fu) - state_gtt_offset, color);
         if (p[2] & 1) {
            decode_struct(brw, spec, "GS_STATE", state, state_gtt_offset,
                          (p[2] & ~0x1fu) - state_gtt_offset, color);
         }
         if (p[3] & 1) {
            decode_struct(brw, spec, "CLIP_STATE", state, state_gtt_offset,
                          (p[3] & ~0x1fu) - state_gtt_offset, color);
         }
         decode_struct(brw, spec, "SF_STATE", state, state_gtt_offset,
                       (p[4] & ~0x1fu) - state_gtt_offset, color);
         decode_struct(brw, spec, "WM_STATE", state, state_gtt_offset,
                       (p[5] & ~0x1fu) - state_gtt_offset, color);
         decode_struct(brw, spec, "COLOR_CALC_STATE", state, state_gtt_offset,
                       (p[6] & ~0x3fu) - state_gtt_offset, color);
         break;
      case _3DSTATE_BINDING_TABLE_POINTERS_VS:
      case _3DSTATE_BINDING_TABLE_POINTERS_HS:
      case _3DSTATE_BINDING_TABLE_POINTERS_DS:
      case _3DSTATE_BINDING_TABLE_POINTERS_GS:
      case _3DSTATE_BINDING_TABLE_POINTERS_PS: {
         struct gen_group *group =
            gen_spec_find_struct(spec, "RENDER_SURFACE_STATE");
         if (!group)
            break;

         uint32_t bt_offset = p[1] & ~0x1fu;
         int bt_entries = brw_state_batch_size(brw, bt_offset) / 4;
         uint32_t *bt_pointers = &state[bt_offset / 4];
         for (int i = 0; i < bt_entries; i++) {
            fprintf(stderr, "SURFACE_STATE - BTI = %d\n", i);
            gen_print_group(stderr, group, state_gtt_offset + bt_pointers[i],
                            &state[bt_pointers[i] / 4], color);
         }
         break;
      }
      case _3DSTATE_SAMPLER_STATE_POINTERS_VS:
      case _3DSTATE_SAMPLER_STATE_POINTERS_HS:
      case _3DSTATE_SAMPLER_STATE_POINTERS_DS:
      case _3DSTATE_SAMPLER_STATE_POINTERS_GS:
      case _3DSTATE_SAMPLER_STATE_POINTERS_PS:
         decode_structs(brw, spec, "SAMPLER_STATE", state,
                        state_gtt_offset, p[1] & ~0x1fu, 4 * 4, color);
         break;
      case _3DSTATE_VIEWPORT_STATE_POINTERS:
         decode_structs(brw, spec, "CLIP_VIEWPORT", state,
                        state_gtt_offset, p[1] & ~0x3fu, 4 * 4, color);
         decode_structs(brw, spec, "SF_VIEWPORT", state,
                        state_gtt_offset, p[1] & ~0x3fu, 8 * 4, color);
         decode_structs(brw, spec, "CC_VIEWPORT", state,
                        state_gtt_offset, p[3] & ~0x3fu, 2 * 4, color);
         break;
      case _3DSTATE_VIEWPORT_STATE_POINTERS_CC:
         decode_structs(brw, spec, "CC_VIEWPORT", state,
                        state_gtt_offset, p[1] & ~0x3fu, 2 * 4, color);
         break;
      case _3DSTATE_VIEWPORT_STATE_POINTERS_SF_CL:
         decode_structs(brw, spec, "SF_CLIP_VIEWPORT", state,
                        state_gtt_offset, p[1] & ~0x3fu, 16 * 4, color);
         break;
      case _3DSTATE_SCISSOR_STATE_POINTERS:
         decode_structs(brw, spec, "SCISSOR_RECT", state,
                        state_gtt_offset, p[1] & ~0x1fu, 2 * 4, color);
         break;
      case _3DSTATE_BLEND_STATE_POINTERS:
         /* TODO: handle Gen8+ extra dword at the beginning */
         decode_structs(brw, spec, "BLEND_STATE", state,
                        state_gtt_offset, p[1] & ~0x3fu, 8 * 4, color);
         break;
      case _3DSTATE_CC_STATE_POINTERS:
         if (devinfo->gen >= 7) {
            decode_struct(brw, spec, "COLOR_CALC_STATE", state,
                          state_gtt_offset, p[1] & ~0x3fu, color);
         } else if (devinfo->gen == 6) {
            decode_structs(brw, spec, "BLEND_STATE", state,
                           state_gtt_offset, p[1] & ~0x3fu, 2 * 4, color);
            decode_struct(brw, spec, "DEPTH_STENCIL_STATE", state,
                          state_gtt_offset, p[2] & ~0x3fu, color);
            decode_struct(brw, spec, "COLOR_CALC_STATE", state,
                          state_gtt_offset, p[3] & ~0x3fu, color);
         }
         break;
      case _3DSTATE_DEPTH_STENCIL_STATE_POINTERS:
         decode_struct(brw, spec, "DEPTH_STENCIL_STATE", state,
                       state_gtt_offset, p[1] & ~0x3fu, color);
         break;
      }
   }

   brw_bo_unmap(batch->batch.bo);
   brw_bo_unmap(batch->state.bo);
}
#else
static void do_batch_dump(struct brw_context *brw) { }
#endif

/**
 * Called when starting a new batch buffer.
 */
static void
brw_new_batch(struct brw_context *brw)
{
   /* Unreference any BOs held by the previous batch, and reset counts. */
   for (int i = 0; i < brw->batch.exec_count; i++) {
      brw_bo_unreference(brw->batch.exec_bos[i]);
      brw->batch.exec_bos[i] = NULL;
   }
   brw->batch.batch_relocs.reloc_count = 0;
   brw->batch.state_relocs.reloc_count = 0;
   brw->batch.exec_count = 0;
   brw->batch.aperture_space = 0;

   brw_bo_unreference(brw->batch.state.bo);

   /* Create a new batchbuffer and reset the associated state: */
   intel_batchbuffer_reset_and_clear_render_cache(brw);

   /* If the kernel supports hardware contexts, then most hardware state is
    * preserved between batches; we only need to re-emit state that is required
    * to be in every batch.  Otherwise we need to re-emit all the state that
    * would otherwise be stored in the context (which for all intents and
    * purposes means everything).
    */
   if (brw->hw_ctx == 0) {
      brw->ctx.NewDriverState |= BRW_NEW_CONTEXT;
      brw_upload_invariant_state(brw);
   }

   brw->ctx.NewDriverState |= BRW_NEW_BATCH;

   brw->ib.index_size = -1;

   /* We need to periodically reap the shader time results, because rollover
    * happens every few seconds.  We also want to see results every once in a
    * while, because many programs won't cleanly destroy our context, so the
    * end-of-run printout may not happen.
    */
   if (INTEL_DEBUG & DEBUG_SHADER_TIME)
      brw_collect_and_report_shader_time(brw);
}

/**
 * Called from intel_batchbuffer_flush before emitting MI_BATCHBUFFER_END and
 * sending it off.
 *
 * This function can emit state (say, to preserve registers that aren't saved
 * between batches).  All of this state MUST fit in the reserved space at the
 * end of the batchbuffer.  If you add more GPU state, increase the reserved
 * space by updating the BATCH_RESERVED macro.
 */
static void
brw_finish_batch(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   brw->batch.no_wrap = true;

   /* Capture the closing pipeline statistics register values necessary to
    * support query objects (in the non-hardware context world).
    */
   brw_emit_query_end(brw);

   if (brw->batch.ring == RENDER_RING) {
      /* Work around L3 state leaks into contexts set MI_RESTORE_INHIBIT which
       * assume that the L3 cache is configured according to the hardware
       * defaults.
       */
      if (devinfo->gen >= 7)
         gen7_restore_default_l3_config(brw);

      if (devinfo->is_haswell) {
         /* From the Haswell PRM, Volume 2b, Command Reference: Instructions,
          * 3DSTATE_CC_STATE_POINTERS > "Note":
          *
          * "SW must program 3DSTATE_CC_STATE_POINTERS command at the end of every
          *  3D batch buffer followed by a PIPE_CONTROL with RC flush and CS stall."
          *
          * From the example in the docs, it seems to expect a regular pipe control
          * flush here as well. We may have done it already, but meh.
          *
          * See also WaAvoidRCZCounterRollover.
          */
         brw_emit_mi_flush(brw);
         BEGIN_BATCH(2);
         OUT_BATCH(_3DSTATE_CC_STATE_POINTERS << 16 | (2 - 2));
         OUT_BATCH(brw->cc.state_offset | 1);
         ADVANCE_BATCH();
         brw_emit_pipe_control_flush(brw, PIPE_CONTROL_RENDER_TARGET_FLUSH |
                                          PIPE_CONTROL_CS_STALL);
      }
   }

   /* Emit MI_BATCH_BUFFER_END to finish our batch.  Note that execbuf2
    * requires our batch size to be QWord aligned, so we pad it out if
    * necessary by emitting an extra MI_NOOP after the end.
    */
   intel_batchbuffer_require_space(brw, 8, brw->batch.ring);
   *brw->batch.map_next++ = MI_BATCH_BUFFER_END;
   if (USED_BATCH(brw->batch) & 1) {
      *brw->batch.map_next++ = MI_NOOP;
   }

   brw->batch.no_wrap = false;
}

static void
throttle(struct brw_context *brw)
{
   /* Wait for the swapbuffers before the one we just emitted, so we
    * don't get too many swaps outstanding for apps that are GPU-heavy
    * but not CPU-heavy.
    *
    * We're using intelDRI2Flush (called from the loader before
    * swapbuffer) and glFlush (for front buffer rendering) as the
    * indicator that a frame is done and then throttle when we get
    * here as we prepare to render the next frame.  At this point for
    * round trips for swap/copy and getting new buffers are done and
    * we'll spend less time waiting on the GPU.
    *
    * Unfortunately, we don't have a handle to the batch containing
    * the swap, and getting our hands on that doesn't seem worth it,
    * so we just use the first batch we emitted after the last swap.
    */
   if (brw->need_swap_throttle && brw->throttle_batch[0]) {
      if (brw->throttle_batch[1]) {
         if (!brw->disable_throttling) {
            /* Pass NULL rather than brw so we avoid perf_debug warnings;
             * stalling is common and expected here...
             */
            brw_bo_wait_rendering(brw->throttle_batch[1]);
         }
         brw_bo_unreference(brw->throttle_batch[1]);
      }
      brw->throttle_batch[1] = brw->throttle_batch[0];
      brw->throttle_batch[0] = NULL;
      brw->need_swap_throttle = false;
      /* Throttling here is more precise than the throttle ioctl, so skip it */
      brw->need_flush_throttle = false;
   }

   if (brw->need_flush_throttle) {
      __DRIscreen *dri_screen = brw->screen->driScrnPriv;
      drmCommandNone(dri_screen->fd, DRM_I915_GEM_THROTTLE);
      brw->need_flush_throttle = false;
   }
}

static int
execbuffer(int fd,
           struct intel_batchbuffer *batch,
           uint32_t ctx_id,
           int used,
           int in_fence,
           int *out_fence,
           int flags)
{
   struct drm_i915_gem_execbuffer2 execbuf = {
      .buffers_ptr = (uintptr_t) batch->validation_list,
      .buffer_count = batch->exec_count,
      .batch_start_offset = 0,
      .batch_len = used,
      .flags = flags,
      .rsvd1 = ctx_id, /* rsvd1 is actually the context ID */
   };

   unsigned long cmd = DRM_IOCTL_I915_GEM_EXECBUFFER2;

   if (in_fence != -1) {
      execbuf.rsvd2 = in_fence;
      execbuf.flags |= I915_EXEC_FENCE_IN;
   }

   if (out_fence != NULL) {
      cmd = DRM_IOCTL_I915_GEM_EXECBUFFER2_WR;
      *out_fence = -1;
      execbuf.flags |= I915_EXEC_FENCE_OUT;
   }

   int ret = drmIoctl(fd, cmd, &execbuf);
   if (ret != 0)
      ret = -errno;

   for (int i = 0; i < batch->exec_count; i++) {
      struct brw_bo *bo = batch->exec_bos[i];

      bo->idle = false;
      bo->index = -1;

      /* Update brw_bo::gtt_offset */
      if (batch->validation_list[i].offset != bo->gtt_offset) {
         DBG("BO %d migrated: 0x%" PRIx64 " -> 0x%llx\n",
             bo->gem_handle, bo->gtt_offset,
             batch->validation_list[i].offset);
         bo->gtt_offset = batch->validation_list[i].offset;
      }
   }

   if (ret == 0 && out_fence != NULL)
      *out_fence = execbuf.rsvd2 >> 32;

   return ret;
}

static int
submit_batch(struct brw_context *brw, int in_fence_fd, int *out_fence_fd)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   __DRIscreen *dri_screen = brw->screen->driScrnPriv;
   struct intel_batchbuffer *batch = &brw->batch;
   int ret = 0;

   if (batch->batch.cpu_map) {
      void *bo_map = brw_bo_map(brw, batch->batch.bo, MAP_WRITE);
      memcpy(bo_map, batch->batch.cpu_map, 4 * USED_BATCH(*batch));
   }

   if (batch->state.cpu_map) {
      void *bo_map = brw_bo_map(brw, batch->state.bo, MAP_WRITE);
      memcpy(bo_map, batch->state.cpu_map, batch->state_used);
   }

   brw_bo_unmap(batch->batch.bo);
   brw_bo_unmap(batch->state.bo);

   if (!brw->screen->no_hw) {
      /* The requirement for using I915_EXEC_NO_RELOC are:
       *
       *   The addresses written in the objects must match the corresponding
       *   reloc.gtt_offset which in turn must match the corresponding
       *   execobject.offset.
       *
       *   Any render targets written to in the batch must be flagged with
       *   EXEC_OBJECT_WRITE.
       *
       *   To avoid stalling, execobject.offset should match the current
       *   address of that object within the active context.
       */
      int flags = I915_EXEC_NO_RELOC;

      if (devinfo->gen >= 6 && batch->ring == BLT_RING) {
         flags |= I915_EXEC_BLT;
      } else {
         flags |= I915_EXEC_RENDER;
      }
      if (batch->needs_sol_reset)
         flags |= I915_EXEC_GEN7_SOL_RESET;

      uint32_t hw_ctx = batch->ring == RENDER_RING ? brw->hw_ctx : 0;

      /* Set statebuffer relocations */
      const unsigned state_index = batch->state.bo->index;
      if (state_index < batch->exec_count &&
          batch->exec_bos[state_index] == batch->state.bo) {
         struct drm_i915_gem_exec_object2 *entry =
            &batch->validation_list[state_index];
         assert(entry->handle == batch->state.bo->gem_handle);
         entry->relocation_count = batch->state_relocs.reloc_count;
         entry->relocs_ptr = (uintptr_t) batch->state_relocs.relocs;
      }

      /* Set batchbuffer relocations */
      struct drm_i915_gem_exec_object2 *entry = &batch->validation_list[0];
      assert(entry->handle == batch->batch.bo->gem_handle);
      entry->relocation_count = batch->batch_relocs.reloc_count;
      entry->relocs_ptr = (uintptr_t) batch->batch_relocs.relocs;

      if (batch->use_batch_first) {
         flags |= I915_EXEC_BATCH_FIRST | I915_EXEC_HANDLE_LUT;
      } else {
         /* Move the batch to the end of the validation list */
         struct drm_i915_gem_exec_object2 tmp;
         const unsigned index = batch->exec_count - 1;

         tmp = *entry;
         *entry = batch->validation_list[index];
         batch->validation_list[index] = tmp;
      }

      ret = execbuffer(dri_screen->fd, batch, hw_ctx,
                       4 * USED_BATCH(*batch),
                       in_fence_fd, out_fence_fd, flags);

      throttle(brw);
   }

   if (unlikely(INTEL_DEBUG & DEBUG_BATCH))
      do_batch_dump(brw);

   if (brw->ctx.Const.ResetStrategy == GL_LOSE_CONTEXT_ON_RESET_ARB)
      brw_check_for_reset(brw);

   if (ret != 0) {
      fprintf(stderr, "i965: Failed to submit batchbuffer: %s\n",
              strerror(-ret));
      exit(1);
   }

   return ret;
}

/**
 * The in_fence_fd is ignored if -1.  Otherwise this function takes ownership
 * of the fd.
 *
 * The out_fence_fd is ignored if NULL. Otherwise, the caller takes ownership
 * of the returned fd.
 */
int
_intel_batchbuffer_flush_fence(struct brw_context *brw,
                               int in_fence_fd, int *out_fence_fd,
                               const char *file, int line)
{
   int ret;

   if (USED_BATCH(brw->batch) == 0)
      return 0;

   /* Check that we didn't just wrap our batchbuffer at a bad time. */
   assert(!brw->batch.no_wrap);

   brw_finish_batch(brw);
   intel_upload_finish(brw);

   if (brw->throttle_batch[0] == NULL) {
      brw->throttle_batch[0] = brw->batch.batch.bo;
      brw_bo_reference(brw->throttle_batch[0]);
   }

   if (unlikely(INTEL_DEBUG & (DEBUG_BATCH | DEBUG_SUBMIT))) {
      int bytes_for_commands = 4 * USED_BATCH(brw->batch);
      int bytes_for_state = brw->batch.state_used;
      fprintf(stderr, "%19s:%-3d: Batchbuffer flush with %5db (%0.1f%%) (pkt),"
              " %5db (%0.1f%%) (state), %4d BOs (%0.1fMb aperture),"
              " %4d batch relocs, %4d state relocs\n", file, line,
              bytes_for_commands, 100.0f * bytes_for_commands / BATCH_SZ,
              bytes_for_state, 100.0f * bytes_for_state / STATE_SZ,
              brw->batch.exec_count,
              (float) brw->batch.aperture_space / (1024 * 1024),
              brw->batch.batch_relocs.reloc_count,
              brw->batch.state_relocs.reloc_count);
   }

   ret = submit_batch(brw, in_fence_fd, out_fence_fd);

   if (unlikely(INTEL_DEBUG & DEBUG_SYNC)) {
      fprintf(stderr, "waiting for idle\n");
      brw_bo_wait_rendering(brw->batch.batch.bo);
   }

   /* Start a new batch buffer. */
   brw_new_batch(brw);

   return ret;
}

bool
brw_batch_has_aperture_space(struct brw_context *brw, unsigned extra_space)
{
   return brw->batch.aperture_space + extra_space <=
          brw->screen->aperture_threshold;
}

bool
brw_batch_references(struct intel_batchbuffer *batch, struct brw_bo *bo)
{
   unsigned index = READ_ONCE(bo->index);
   if (index < batch->exec_count && batch->exec_bos[index] == bo)
      return true;

   for (int i = 0; i < batch->exec_count; i++) {
      if (batch->exec_bos[i] == bo)
         return true;
   }
   return false;
}

/*  This is the only way buffers get added to the validate list.
 */
static uint64_t
emit_reloc(struct intel_batchbuffer *batch,
           struct brw_reloc_list *rlist, uint32_t offset,
           struct brw_bo *target, int32_t target_offset,
           unsigned int reloc_flags)
{
   assert(target != NULL);

   if (rlist->reloc_count == rlist->reloc_array_size) {
      rlist->reloc_array_size *= 2;
      rlist->relocs = realloc(rlist->relocs,
                              rlist->reloc_array_size *
                              sizeof(struct drm_i915_gem_relocation_entry));
   }

   unsigned int index = add_exec_bo(batch, target);
   struct drm_i915_gem_exec_object2 *entry = &batch->validation_list[index];

   if (reloc_flags)
      entry->flags |= reloc_flags & batch->valid_reloc_flags;

   rlist->relocs[rlist->reloc_count++] =
      (struct drm_i915_gem_relocation_entry) {
         .offset = offset,
         .delta = target_offset,
         .target_handle = batch->use_batch_first ? index : target->gem_handle,
         .presumed_offset = entry->offset,
      };

   /* Using the old buffer offset, write in what the right data would be, in
    * case the buffer doesn't move and we can short-circuit the relocation
    * processing in the kernel
    */
   return entry->offset + target_offset;
}

uint64_t
brw_batch_reloc(struct intel_batchbuffer *batch, uint32_t batch_offset,
                struct brw_bo *target, uint32_t target_offset,
                unsigned int reloc_flags)
{
   assert(batch_offset <= batch->batch.bo->size - sizeof(uint32_t));

   return emit_reloc(batch, &batch->batch_relocs, batch_offset,
                     target, target_offset, reloc_flags);
}

uint64_t
brw_state_reloc(struct intel_batchbuffer *batch, uint32_t state_offset,
                struct brw_bo *target, uint32_t target_offset,
                unsigned int reloc_flags)
{
   assert(state_offset <= batch->state.bo->size - sizeof(uint32_t));

   return emit_reloc(batch, &batch->state_relocs, state_offset,
                     target, target_offset, reloc_flags);
}


uint32_t
brw_state_batch_size(struct brw_context *brw, uint32_t offset)
{
   struct hash_entry *entry =
      _mesa_hash_table_search(brw->batch.state_batch_sizes,
                              (void *) (uintptr_t) offset);
   return entry ? (uintptr_t) entry->data : 0;
}

/**
 * Reserve some space in the statebuffer, or flush.
 *
 * This is used to estimate when we're near the end of the batch,
 * so we can flush early.
 */
void
brw_require_statebuffer_space(struct brw_context *brw, int size)
{
   if (brw->batch.state_used + size >= STATE_SZ)
      intel_batchbuffer_flush(brw);
}

/**
 * Allocates a block of space in the batchbuffer for indirect state.
 */
void *
brw_state_batch(struct brw_context *brw,
                int size,
                int alignment,
                uint32_t *out_offset)
{
   struct intel_batchbuffer *batch = &brw->batch;

   assert(size < batch->state.bo->size);

   uint32_t offset = ALIGN(batch->state_used, alignment);

   if (offset + size >= STATE_SZ && !batch->no_wrap) {
      intel_batchbuffer_flush(brw);
      offset = ALIGN(batch->state_used, alignment);
   } else if (offset + size >= batch->state.bo->size) {
      const unsigned new_size =
         MIN2(batch->state.bo->size + batch->state.bo->size / 2,
              MAX_STATE_SIZE);
      grow_buffer(brw, &batch->state.bo, &batch->state.map,
                  &batch->state.cpu_map, batch->state_used, new_size);
      assert(offset + size < batch->state.bo->size);
   }

   if (unlikely(INTEL_DEBUG & DEBUG_BATCH)) {
      _mesa_hash_table_insert(batch->state_batch_sizes,
                              (void *) (uintptr_t) offset,
                              (void *) (uintptr_t) size);
   }

   batch->state_used = offset + size;

   *out_offset = offset;
   return batch->state.map + (offset >> 2);
}

void
intel_batchbuffer_data(struct brw_context *brw,
                       const void *data, GLuint bytes, enum brw_gpu_ring ring)
{
   assert((bytes & 3) == 0);
   intel_batchbuffer_require_space(brw, bytes, ring);
   memcpy(brw->batch.map_next, data, bytes);
   brw->batch.map_next += bytes >> 2;
}

static void
load_sized_register_mem(struct brw_context *brw,
                        uint32_t reg,
                        struct brw_bo *bo,
                        uint32_t offset,
                        int size)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   int i;

   /* MI_LOAD_REGISTER_MEM only exists on Gen7+. */
   assert(devinfo->gen >= 7);

   if (devinfo->gen >= 8) {
      BEGIN_BATCH(4 * size);
      for (i = 0; i < size; i++) {
         OUT_BATCH(GEN7_MI_LOAD_REGISTER_MEM | (4 - 2));
         OUT_BATCH(reg + i * 4);
         OUT_RELOC64(bo, 0, offset + i * 4);
      }
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(3 * size);
      for (i = 0; i < size; i++) {
         OUT_BATCH(GEN7_MI_LOAD_REGISTER_MEM | (3 - 2));
         OUT_BATCH(reg + i * 4);
         OUT_RELOC(bo, 0, offset + i * 4);
      }
      ADVANCE_BATCH();
   }
}

void
brw_load_register_mem(struct brw_context *brw,
                      uint32_t reg,
                      struct brw_bo *bo,
                      uint32_t offset)
{
   load_sized_register_mem(brw, reg, bo, offset, 1);
}

void
brw_load_register_mem64(struct brw_context *brw,
                        uint32_t reg,
                        struct brw_bo *bo,
                        uint32_t offset)
{
   load_sized_register_mem(brw, reg, bo, offset, 2);
}

/*
 * Write an arbitrary 32-bit register to a buffer via MI_STORE_REGISTER_MEM.
 */
void
brw_store_register_mem32(struct brw_context *brw,
                         struct brw_bo *bo, uint32_t reg, uint32_t offset)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   if (devinfo->gen >= 8) {
      BEGIN_BATCH(4);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (4 - 2));
      OUT_BATCH(reg);
      OUT_RELOC64(bo, RELOC_WRITE, offset);
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(3);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (3 - 2));
      OUT_BATCH(reg);
      OUT_RELOC(bo, RELOC_WRITE | RELOC_NEEDS_GGTT, offset);
      ADVANCE_BATCH();
   }
}

/*
 * Write an arbitrary 64-bit register to a buffer via MI_STORE_REGISTER_MEM.
 */
void
brw_store_register_mem64(struct brw_context *brw,
                         struct brw_bo *bo, uint32_t reg, uint32_t offset)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   /* MI_STORE_REGISTER_MEM only stores a single 32-bit value, so to
    * read a full 64-bit register, we need to do two of them.
    */
   if (devinfo->gen >= 8) {
      BEGIN_BATCH(8);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (4 - 2));
      OUT_BATCH(reg);
      OUT_RELOC64(bo, RELOC_WRITE, offset);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (4 - 2));
      OUT_BATCH(reg + sizeof(uint32_t));
      OUT_RELOC64(bo, RELOC_WRITE, offset + sizeof(uint32_t));
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(6);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (3 - 2));
      OUT_BATCH(reg);
      OUT_RELOC(bo, RELOC_WRITE | RELOC_NEEDS_GGTT, offset);
      OUT_BATCH(MI_STORE_REGISTER_MEM | (3 - 2));
      OUT_BATCH(reg + sizeof(uint32_t));
      OUT_RELOC(bo, RELOC_WRITE | RELOC_NEEDS_GGTT, offset + sizeof(uint32_t));
      ADVANCE_BATCH();
   }
}

/*
 * Write a 32-bit register using immediate data.
 */
void
brw_load_register_imm32(struct brw_context *brw, uint32_t reg, uint32_t imm)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   BEGIN_BATCH(3);
   OUT_BATCH(MI_LOAD_REGISTER_IMM | (3 - 2));
   OUT_BATCH(reg);
   OUT_BATCH(imm);
   ADVANCE_BATCH();
}

/*
 * Write a 64-bit register using immediate data.
 */
void
brw_load_register_imm64(struct brw_context *brw, uint32_t reg, uint64_t imm)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   BEGIN_BATCH(5);
   OUT_BATCH(MI_LOAD_REGISTER_IMM | (5 - 2));
   OUT_BATCH(reg);
   OUT_BATCH(imm & 0xffffffff);
   OUT_BATCH(reg + 4);
   OUT_BATCH(imm >> 32);
   ADVANCE_BATCH();
}

/*
 * Copies a 32-bit register.
 */
void
brw_load_register_reg(struct brw_context *brw, uint32_t src, uint32_t dest)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 8 || devinfo->is_haswell);

   BEGIN_BATCH(3);
   OUT_BATCH(MI_LOAD_REGISTER_REG | (3 - 2));
   OUT_BATCH(src);
   OUT_BATCH(dest);
   ADVANCE_BATCH();
}

/*
 * Copies a 64-bit register.
 */
void
brw_load_register_reg64(struct brw_context *brw, uint32_t src, uint32_t dest)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 8 || devinfo->is_haswell);

   BEGIN_BATCH(6);
   OUT_BATCH(MI_LOAD_REGISTER_REG | (3 - 2));
   OUT_BATCH(src);
   OUT_BATCH(dest);
   OUT_BATCH(MI_LOAD_REGISTER_REG | (3 - 2));
   OUT_BATCH(src + sizeof(uint32_t));
   OUT_BATCH(dest + sizeof(uint32_t));
   ADVANCE_BATCH();
}

/*
 * Write 32-bits of immediate data to a GPU memory buffer.
 */
void
brw_store_data_imm32(struct brw_context *brw, struct brw_bo *bo,
                     uint32_t offset, uint32_t imm)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   BEGIN_BATCH(4);
   OUT_BATCH(MI_STORE_DATA_IMM | (4 - 2));
   if (devinfo->gen >= 8)
      OUT_RELOC64(bo, RELOC_WRITE, offset);
   else {
      OUT_BATCH(0); /* MBZ */
      OUT_RELOC(bo, RELOC_WRITE, offset);
   }
   OUT_BATCH(imm);
   ADVANCE_BATCH();
}

/*
 * Write 64-bits of immediate data to a GPU memory buffer.
 */
void
brw_store_data_imm64(struct brw_context *brw, struct brw_bo *bo,
                     uint32_t offset, uint64_t imm)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;

   assert(devinfo->gen >= 6);

   BEGIN_BATCH(5);
   OUT_BATCH(MI_STORE_DATA_IMM | (5 - 2));
   if (devinfo->gen >= 8)
      OUT_RELOC64(bo, RELOC_WRITE, offset);
   else {
      OUT_BATCH(0); /* MBZ */
      OUT_RELOC(bo, RELOC_WRITE, offset);
   }
   OUT_BATCH(imm & 0xffffffffu);
   OUT_BATCH(imm >> 32);
   ADVANCE_BATCH();
}
