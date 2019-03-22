/*
 * Copyright © 2015 Intel Corporation
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

#include "main/mtypes.h"
#include "nir.h"

static void
set_io_mask(nir_shader *shader, nir_variable *var, int offset, int len)
{
   for (int i = 0; i < len; i++) {
      assert(var->data.location != -1);

      int idx = var->data.location + offset + i;
      bool is_patch_generic = var->data.patch &&
                              idx != VARYING_SLOT_TESS_LEVEL_INNER &&
                              idx != VARYING_SLOT_TESS_LEVEL_OUTER &&
                              idx != VARYING_SLOT_BOUNDING_BOX0 &&
                              idx != VARYING_SLOT_BOUNDING_BOX1;
      uint64_t bitfield;

      if (is_patch_generic) {
         assert(idx >= VARYING_SLOT_PATCH0 && idx < VARYING_SLOT_TESS_MAX);
         bitfield = BITFIELD64_BIT(idx - VARYING_SLOT_PATCH0);
      }
      else {
         assert(idx < VARYING_SLOT_MAX);
         bitfield = BITFIELD64_BIT(idx);
      }

      if (var->data.mode == nir_var_shader_in) {
         if (is_patch_generic)
            shader->info.patch_inputs_read |= bitfield;
         else
            shader->info.inputs_read |= bitfield;

         if (shader->info.stage == MESA_SHADER_FRAGMENT) {
            shader->info.fs.uses_sample_qualifier |= var->data.sample;
         }
      } else {
         assert(var->data.mode == nir_var_shader_out);
         if (is_patch_generic) {
            shader->info.patch_outputs_written |= bitfield;
         } else if (!var->data.read_only) {
            shader->info.outputs_written |= bitfield;
         }

         if (var->data.fb_fetch_output)
            shader->info.outputs_read |= bitfield;
      }
   }
}

/**
 * Mark an entire variable as used.  Caller must ensure that the variable
 * represents a shader input or output.
 */
static void
mark_whole_variable(nir_shader *shader, nir_variable *var)
{
   const struct glsl_type *type = var->type;

   if (nir_is_per_vertex_io(var, shader->info.stage)) {
      assert(glsl_type_is_array(type));
      type = glsl_get_array_element(type);
   }

   const unsigned slots =
      var->data.compact ? DIV_ROUND_UP(glsl_get_length(type), 4)
                        : glsl_count_attribute_slots(type, false);

   set_io_mask(shader, var, 0, slots);
}

static unsigned
get_io_offset(nir_deref_var *deref)
{
   unsigned offset = 0;

   nir_deref *tail = &deref->deref;
   while (tail->child != NULL) {
      tail = tail->child;

      if (tail->deref_type == nir_deref_type_array) {
         nir_deref_array *deref_array = nir_deref_as_array(tail);

         if (deref_array->deref_array_type == nir_deref_array_type_indirect) {
            return -1;
         }

         offset += glsl_count_attribute_slots(tail->type, false) *
            deref_array->base_offset;
      }
      /* TODO: we can get the offset for structs here see nir_lower_io() */
   }

   return offset;
}

/**
 * Try to mark a portion of the given varying as used.  Caller must ensure
 * that the variable represents a shader input or output.
 *
 * If the index can't be interpreted as a constant, or some other problem
 * occurs, then nothing will be marked and false will be returned.
 */
static bool
try_mask_partial_io(nir_shader *shader, nir_deref_var *deref)
{
   nir_variable *var = deref->var;
   const struct glsl_type *type = var->type;

   if (nir_is_per_vertex_io(var, shader->info.stage)) {
      assert(glsl_type_is_array(type));
      type = glsl_get_array_element(type);
   }

   /* The code below only handles:
    *
    * - Indexing into matrices
    * - Indexing into arrays of (arrays, matrices, vectors, or scalars)
    *
    * For now, we just give up if we see varying structs and arrays of structs
    * here marking the entire variable as used.
    */
   if (!(glsl_type_is_matrix(type) ||
         (glsl_type_is_array(type) && !var->data.compact &&
          (glsl_type_is_numeric(glsl_without_array(type)) ||
           glsl_type_is_boolean(glsl_without_array(type)))))) {

      /* If we don't know how to handle this case, give up and let the
       * caller mark the whole variable as used.
       */
      return false;
   }

   unsigned offset = get_io_offset(deref);
   if (offset == -1)
      return false;

   unsigned num_elems;
   unsigned elem_width = 1;
   unsigned mat_cols = 1;
   if (glsl_type_is_array(type)) {
      num_elems = glsl_get_aoa_size(type);
      if (glsl_type_is_matrix(glsl_without_array(type)))
         mat_cols = glsl_get_matrix_columns(glsl_without_array(type));
   } else {
      num_elems = glsl_get_matrix_columns(type);
   }

   /* double element width for double types that takes two slots */
   if (glsl_type_is_dual_slot(glsl_without_array(type))) {
      elem_width *= 2;
   }

   if (offset >= num_elems * elem_width * mat_cols) {
      /* Constant index outside the bounds of the matrix/array.  This could
       * arise as a result of constant folding of a legal GLSL program.
       *
       * Even though the spec says that indexing outside the bounds of a
       * matrix/array results in undefined behaviour, we don't want to pass
       * out-of-range values to set_io_mask() (since this could result in
       * slots that don't exist being marked as used), so just let the caller
       * mark the whole variable as used.
       */
      return false;
   }

   set_io_mask(shader, var, offset, elem_width);
   return true;
}

static void
gather_intrinsic_info(nir_intrinsic_instr *instr, nir_shader *shader)
{
   switch (instr->intrinsic) {
   case nir_intrinsic_discard:
   case nir_intrinsic_discard_if:
      assert(shader->info.stage == MESA_SHADER_FRAGMENT);
      shader->info.fs.uses_discard = true;
      break;

   case nir_intrinsic_interp_var_at_centroid:
   case nir_intrinsic_interp_var_at_sample:
   case nir_intrinsic_interp_var_at_offset:
   case nir_intrinsic_load_var:
   case nir_intrinsic_store_var: {
      nir_variable *var = instr->variables[0]->var;

      if (var->data.mode == nir_var_shader_in ||
          var->data.mode == nir_var_shader_out) {
         if (!try_mask_partial_io(shader, instr->variables[0]))
            mark_whole_variable(shader, var);

         /* We need to track which input_reads bits correspond to a
          * dvec3/dvec4 input attribute */
         if (shader->info.stage == MESA_SHADER_VERTEX &&
             var->data.mode == nir_var_shader_in &&
             glsl_type_is_dual_slot(glsl_without_array(var->type))) {
            for (uint i = 0; i < glsl_count_attribute_slots(var->type, false); i++) {
               int idx = var->data.location + i;
               shader->info.double_inputs_read |= BITFIELD64_BIT(idx);
            }
         }
      }
      break;
   }

   case nir_intrinsic_load_draw_id:
   case nir_intrinsic_load_frag_coord:
   case nir_intrinsic_load_front_face:
   case nir_intrinsic_load_vertex_id:
   case nir_intrinsic_load_vertex_id_zero_base:
   case nir_intrinsic_load_base_vertex:
   case nir_intrinsic_load_base_instance:
   case nir_intrinsic_load_instance_id:
   case nir_intrinsic_load_sample_id:
   case nir_intrinsic_load_sample_pos:
   case nir_intrinsic_load_sample_mask_in:
   case nir_intrinsic_load_primitive_id:
   case nir_intrinsic_load_invocation_id:
   case nir_intrinsic_load_local_invocation_id:
   case nir_intrinsic_load_local_invocation_index:
   case nir_intrinsic_load_work_group_id:
   case nir_intrinsic_load_num_work_groups:
   case nir_intrinsic_load_tess_coord:
   case nir_intrinsic_load_tess_level_outer:
   case nir_intrinsic_load_tess_level_inner:
      shader->info.system_values_read |=
         (1ull << nir_system_value_from_intrinsic(instr->intrinsic));
      break;

   case nir_intrinsic_end_primitive:
   case nir_intrinsic_end_primitive_with_counter:
      assert(shader->info.stage == MESA_SHADER_GEOMETRY);
      shader->info.gs.uses_end_primitive = 1;
      break;

   default:
      break;
   }
}

static void
gather_tex_info(nir_tex_instr *instr, nir_shader *shader)
{
   switch (instr->op) {
   case nir_texop_tg4:
      shader->info.uses_texture_gather = true;
      break;
   case nir_texop_txf:
   case nir_texop_txf_ms:
   case nir_texop_txf_ms_mcs:
      shader->info.textures_used_by_txf |=
         ((1 << MAX2(instr->texture_array_size, 1)) - 1) <<
         instr->texture_index;
      break;
   default:
      break;
   }
}

static void
gather_info_block(nir_block *block, nir_shader *shader)
{
   nir_foreach_instr(instr, block) {
      switch (instr->type) {
      case nir_instr_type_intrinsic:
         gather_intrinsic_info(nir_instr_as_intrinsic(instr), shader);
         break;
      case nir_instr_type_tex:
         gather_tex_info(nir_instr_as_tex(instr), shader);
         break;
      case nir_instr_type_call:
         assert(!"nir_shader_gather_info only works if functions are inlined");
         break;
      default:
         break;
      }
   }
}

void
nir_shader_gather_info(nir_shader *shader, nir_function_impl *entrypoint)
{
   shader->info.num_textures = 0;
   shader->info.num_images = 0;
   nir_foreach_variable(var, &shader->uniforms) {
      const struct glsl_type *type = var->type;
      unsigned count = 1;
      if (glsl_type_is_array(type)) {
         count = glsl_get_aoa_size(type);
         type = glsl_without_array(type);
      }

      if (glsl_type_is_image(type)) {
         shader->info.num_images += count;
      } else if (glsl_type_is_sampler(type)) {
         shader->info.num_textures += count;
      }
   }

   shader->info.inputs_read = 0;
   shader->info.outputs_written = 0;
   shader->info.outputs_read = 0;
   shader->info.double_inputs_read = 0;
   shader->info.patch_inputs_read = 0;
   shader->info.patch_outputs_written = 0;
   shader->info.system_values_read = 0;
   if (shader->info.stage == MESA_SHADER_FRAGMENT) {
      shader->info.fs.uses_sample_qualifier = false;
   }
   nir_foreach_block(block, entrypoint) {
      gather_info_block(block, shader);
   }
}
