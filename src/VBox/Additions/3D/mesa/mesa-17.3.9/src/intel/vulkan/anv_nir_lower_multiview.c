/*
 * Copyright © 2016 Intel Corporation
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

#include "anv_nir.h"
#include "nir/nir_builder.h"

/**
 * This file implements the lowering required for VK_KHR_multiview.  We
 * implement multiview using instanced rendering.  The number of instances in
 * each draw call is multiplied by the number of views in the subpass.  Then,
 * in the shader, we divide gl_InstanceId by the number of views and use
 * gl_InstanceId % view_count to compute the actual ViewIndex.
 */

struct lower_multiview_state {
   nir_builder builder;

   uint32_t view_mask;

   nir_ssa_def *instance_id;
   nir_ssa_def *view_index;
};

static nir_ssa_def *
build_instance_id(struct lower_multiview_state *state)
{
   assert(state->builder.shader->info.stage == MESA_SHADER_VERTEX);

   if (state->instance_id == NULL) {
      nir_builder *b = &state->builder;

      b->cursor = nir_before_block(nir_start_block(b->impl));

      /* We use instancing for implementing multiview.  The actual instance id
       * is given by dividing instance_id by the number of views in this
       * subpass.
       */
      state->instance_id =
         nir_idiv(b, nir_load_instance_id(b),
                     nir_imm_int(b, _mesa_bitcount(state->view_mask)));
   }

   return state->instance_id;
}

static nir_ssa_def *
build_view_index(struct lower_multiview_state *state)
{
   if (state->view_index == NULL) {
      nir_builder *b = &state->builder;

      b->cursor = nir_before_block(nir_start_block(b->impl));

      assert(state->view_mask != 0);
      if (0 && _mesa_bitcount(state->view_mask) == 1) {
         state->view_index = nir_imm_int(b, ffs(state->view_mask) - 1);
      } else if (state->builder.shader->info.stage == MESA_SHADER_VERTEX) {
         /* We only support 16 viewports */
         assert((state->view_mask & 0xffff0000) == 0);

         /* We use instancing for implementing multiview.  The compacted view
          * id is given by instance_id % view_count.  We then have to convert
          * that to an actual view id.
          */
         nir_ssa_def *compacted =
            nir_umod(b, nir_load_instance_id(b),
                        nir_imm_int(b, _mesa_bitcount(state->view_mask)));

         if (0 && util_is_power_of_two(state->view_mask + 1)) {
            /* If we have a full view mask, then compacted is what we want */
            state->view_index = compacted;
         } else {
            /* Now we define a map from compacted view index to the actual
             * view index that's based on the view_mask.  The map is given by
             * 16 nibbles, each of which is a value from 0 to 15.
             */
            uint64_t remap = 0;
            uint32_t bit, i = 0;
            for_each_bit(bit, state->view_mask) {
               assert(bit < 16);
               remap |= (uint64_t)bit << (i++ * 4);
            }

            nir_ssa_def *shift = nir_imul(b, compacted, nir_imm_int(b, 4));

            /* One of these days, when we have int64 everywhere, this will be
             * easier.
             */
            nir_ssa_def *shifted;
            if (remap <= UINT32_MAX) {
               shifted = nir_ushr(b, nir_imm_int(b, remap), shift);
            } else {
               nir_ssa_def *shifted_low =
                  nir_ushr(b, nir_imm_int(b, remap), shift);
               nir_ssa_def *shifted_high =
                  nir_ushr(b, nir_imm_int(b, remap >> 32),
                              nir_isub(b, shift, nir_imm_int(b, 32)));
               shifted = nir_bcsel(b, nir_ilt(b, shift, nir_imm_int(b, 32)),
                                      shifted_low, shifted_high);
            }
            state->view_index = nir_iand(b, shifted, nir_imm_int(b, 0xf));
         }
      } else {
         const struct glsl_type *type = glsl_int_type();
         if (b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_GEOMETRY)
            type = glsl_array_type(type, 1);

         nir_variable *idx_var =
            nir_variable_create(b->shader, nir_var_shader_in,
                                type, "view index");
         idx_var->data.location = VARYING_SLOT_VIEW_INDEX;
         if (b->shader->info.stage == MESA_SHADER_FRAGMENT)
            idx_var->data.interpolation = INTERP_MODE_FLAT;

         if (glsl_type_is_array(type)) {
            nir_deref_var *deref = nir_deref_var_create(b->shader, idx_var);
            nir_deref_array *arr = nir_deref_array_create(b->shader);
            arr->deref.type = glsl_int_type();
            arr->deref_array_type = nir_deref_array_type_direct;
            arr->base_offset = 0;
            deref->deref.child = &arr->deref;

            state->view_index = nir_load_deref_var(b, deref);
         } else {
            state->view_index = nir_load_var(b, idx_var);
         }
      }
   }

   return state->view_index;
}

bool
anv_nir_lower_multiview(nir_shader *shader, uint32_t view_mask)
{
   assert(shader->info.stage != MESA_SHADER_COMPUTE);

   /* If multiview isn't enabled, we have nothing to do. */
   if (view_mask == 0)
      return false;

   struct lower_multiview_state state = {
      .view_mask = view_mask,
   };

   /* This pass assumes a single entrypoint */
   nir_function_impl *entrypoint = nir_shader_get_entrypoint(shader);

   nir_builder_init(&state.builder, entrypoint);

   bool progress = false;
   nir_foreach_block(block, entrypoint) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *load = nir_instr_as_intrinsic(instr);

         if (load->intrinsic != nir_intrinsic_load_instance_id &&
             load->intrinsic != nir_intrinsic_load_view_index)
            continue;

         assert(load->dest.is_ssa);

         nir_ssa_def *value;
         if (load->intrinsic == nir_intrinsic_load_instance_id) {
            value = build_instance_id(&state);
         } else {
            assert(load->intrinsic == nir_intrinsic_load_view_index);
            value = build_view_index(&state);
         }

         nir_ssa_def_rewrite_uses(&load->dest.ssa, nir_src_for_ssa(value));

         nir_instr_remove(&load->instr);
         progress = true;
      }
   }

   /* The view index is available in all stages but the instance id is only
    * available in the VS.  If it's not a fragment shader, we need to pass
    * the view index on to the next stage.
    */
   if (shader->info.stage != MESA_SHADER_FRAGMENT) {
      nir_ssa_def *view_index = build_view_index(&state);

      nir_builder *b = &state.builder;

      assert(view_index->parent_instr->block == nir_start_block(entrypoint));
      b->cursor = nir_after_instr(view_index->parent_instr);

      nir_variable *view_index_out =
         nir_variable_create(shader, nir_var_shader_out,
                             glsl_int_type(), "view index");
      view_index_out->data.location = VARYING_SLOT_VIEW_INDEX;
      nir_store_var(b, view_index_out, view_index, 0x1);

      nir_variable *layer_id_out =
         nir_variable_create(shader, nir_var_shader_out,
                             glsl_int_type(), "layer ID");
      layer_id_out->data.location = VARYING_SLOT_LAYER;
      nir_store_var(b, layer_id_out, view_index, 0x1);

      progress = true;
   }

   if (progress) {
      nir_metadata_preserve(entrypoint, nir_metadata_block_index |
                                        nir_metadata_dominance);
   }

   return progress;
}
