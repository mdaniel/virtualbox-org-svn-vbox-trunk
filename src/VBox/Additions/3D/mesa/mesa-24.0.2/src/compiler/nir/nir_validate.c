/*
 * Copyright © 2014 Intel Corporation
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
 *    Connor Abbott (cwabbott0@gmail.com)
 *
 */

#include <assert.h>
#include "c11/threads.h"
#include "util/simple_mtx.h"
#include "nir.h"
#include "nir_xfb_info.h"

/*
 * This file checks for invalid IR indicating a bug somewhere in the compiler.
 */

/* Since this file is just a pile of asserts, don't bother compiling it if
 * we're not building a debug build.
 */
#ifndef NDEBUG

typedef struct {
   void *mem_ctx;

   /* the current shader being validated */
   nir_shader *shader;

   /* the current instruction being validated */
   nir_instr *instr;

   /* the current variable being validated */
   nir_variable *var;

   /* the current basic block being validated */
   nir_block *block;

   /* the current if statement being validated */
   nir_if *if_stmt;

   /* the current loop being visited */
   nir_loop *loop;

   /* weather the loop continue construct is being visited */
   bool in_loop_continue_construct;

   /* the parent of the current cf node being visited */
   nir_cf_node *parent_node;

   /* the current function implementation being validated */
   nir_function_impl *impl;

   /* Set of all blocks in the list */
   struct set *blocks;

   /* Number of tagged nir_src's. This is implicitly the cardinality of the set
    * of pending nir_src's.
    */
   uint32_t nr_tagged_srcs;

   /* bitset of ssa definitions we have found; used to check uniqueness */
   BITSET_WORD *ssa_defs_found;

   /* map of variable -> function implementation where it is defined or NULL
    * if it is a global variable
    */
   struct hash_table *var_defs;

   /* map of instruction/var/etc to failed assert string */
   struct hash_table *errors;
} validate_state;

static void
log_error(validate_state *state, const char *cond, const char *file, int line)
{
   const void *obj;

   if (state->instr)
      obj = state->instr;
   else if (state->var)
      obj = state->var;
   else
      obj = cond;

   char *msg = ralloc_asprintf(state->errors, "error: %s (%s:%d)",
                               cond, file, line);

   _mesa_hash_table_insert(state->errors, obj, msg);
}

static bool
validate_assert_impl(validate_state *state, bool cond, const char *str,
                     const char *file, unsigned line)
{
   if (unlikely(!cond))
      log_error(state, str, file, line);
   return cond;
}

#define validate_assert(state, cond) \
   validate_assert_impl(state, (cond), #cond, __FILE__, __LINE__)

static void
validate_num_components(validate_state *state, unsigned num_components)
{
   validate_assert(state, nir_num_components_valid(num_components));
}

/* Tag used in nir_src::_parent to indicate that a source has been seen. */
#define SRC_TAG_SEEN (0x2)

static_assert(SRC_TAG_SEEN == (~NIR_SRC_PARENT_MASK + 1),
              "Parent pointer tags chosen not to collide");

static void
tag_src(nir_src *src, validate_state *state)
{
   /* nir_src only appears once and only in one SSA def use list, since we
    * mark nir_src's as we go by tagging this pointer.
    */
   if (validate_assert(state, (src->_parent & SRC_TAG_SEEN) == 0)) {
      src->_parent |= SRC_TAG_SEEN;
      state->nr_tagged_srcs++;
   }
}

/* Due to tagging, it's not safe to use nir_src_parent_instr during the main
 * validate loop. This is a tagging-aware version.
 */
static nir_instr *
src_parent_instr_safe(nir_src *src)
{
   uintptr_t untagged = (src->_parent & ~SRC_TAG_SEEN);
   assert(!(untagged & NIR_SRC_PARENT_IS_IF) && "precondition");
   return (nir_instr *)untagged;
}

/*
 * As we walk SSA defs, we mark every use as seen by tagging the parent pointer.
 * We need to make sure our use is seen in a use list.
 *
 * Then we unmark when we hit the source. This will let us prove that we've
 * seen all the sources.
 */
static void
validate_src_tag(nir_src *src, validate_state *state)
{
   if (validate_assert(state, src->_parent & SRC_TAG_SEEN)) {
      src->_parent &= ~SRC_TAG_SEEN;
      state->nr_tagged_srcs--;
   }
}

static void
validate_if_src(nir_src *src, validate_state *state)
{
   validate_src_tag(src, state);
   validate_assert(state, nir_src_parent_if(src) == state->if_stmt);
   validate_assert(state, src->ssa != NULL);
   validate_assert(state, src->ssa->num_components == 1);
}

static void
validate_src(nir_src *src, validate_state *state)
{
   /* Validate the tag first, so that nir_src_parent_instr is valid */
   validate_src_tag(src, state);

   /* Source assumed to be instruction, use validate_if_src for if */
   validate_assert(state, nir_src_parent_instr(src) == state->instr);

   validate_assert(state, src->ssa != NULL);
}

static void
validate_sized_src(nir_src *src, validate_state *state,
                   unsigned bit_sizes, unsigned num_components)
{
   validate_src(src, state);

   if (bit_sizes)
      validate_assert(state, src->ssa->bit_size & bit_sizes);
   if (num_components)
      validate_assert(state, src->ssa->num_components == num_components);
}

static void
validate_alu_src(nir_alu_instr *instr, unsigned index, validate_state *state)
{
   nir_alu_src *src = &instr->src[index];

   unsigned num_instr_channels = nir_ssa_alu_instr_src_components(instr, index);
   unsigned num_components = nir_src_num_components(src->src);

   for (unsigned i = 0; i < num_instr_channels; i++) {
      validate_assert(state, src->swizzle[i] < num_components);
   }

   validate_src(&src->src, state);
}

static void
validate_def(nir_def *def, validate_state *state)
{
   validate_assert(state, def->index < state->impl->ssa_alloc);
   validate_assert(state, !BITSET_TEST(state->ssa_defs_found, def->index));
   BITSET_SET(state->ssa_defs_found, def->index);

   validate_assert(state, def->parent_instr == state->instr);
   validate_num_components(state, def->num_components);

   list_validate(&def->uses);
   nir_foreach_use_including_if(src, def) {
      /* Check that the def matches. */
      validate_assert(state, src->ssa == def);

      /* Check that nir_src's are unique */
      tag_src(src, state);
   }
}

static void
validate_alu_instr(nir_alu_instr *instr, validate_state *state)
{
   validate_assert(state, instr->op < nir_num_opcodes);

   unsigned instr_bit_size = 0;
   for (unsigned i = 0; i < nir_op_infos[instr->op].num_inputs; i++) {
      nir_alu_type src_type = nir_op_infos[instr->op].input_types[i];
      unsigned src_bit_size = nir_src_bit_size(instr->src[i].src);
      if (nir_alu_type_get_type_size(src_type)) {
         validate_assert(state, src_bit_size == nir_alu_type_get_type_size(src_type));
      } else if (instr_bit_size) {
         validate_assert(state, src_bit_size == instr_bit_size);
      } else {
         instr_bit_size = src_bit_size;
      }

      if (nir_alu_type_get_base_type(src_type) == nir_type_float) {
         /* 8-bit float isn't a thing */
         validate_assert(state, src_bit_size == 16 || src_bit_size == 32 ||
                                   src_bit_size == 64);
      }

      /* In nir_opcodes.py, these are defined to take general uint or int
       * sources.  However, they're really only defined for 32-bit or 64-bit
       * sources.  This seems to be the only place to enforce this
       * restriction.
       */
      switch (instr->op) {
      case nir_op_ufind_msb:
      case nir_op_ufind_msb_rev:
         validate_assert(state, src_bit_size == 32 || src_bit_size == 64);
         break;

      default:
         break;
      }

      validate_alu_src(instr, i, state);
   }

   nir_alu_type dest_type = nir_op_infos[instr->op].output_type;
   unsigned dest_bit_size = instr->def.bit_size;
   if (nir_alu_type_get_type_size(dest_type)) {
      validate_assert(state, dest_bit_size == nir_alu_type_get_type_size(dest_type));
   } else if (instr_bit_size) {
      validate_assert(state, dest_bit_size == instr_bit_size);
   } else {
      /* The only unsized thing is the destination so it's vacuously valid */
   }

   if (nir_alu_type_get_base_type(dest_type) == nir_type_float) {
      /* 8-bit float isn't a thing */
      validate_assert(state, dest_bit_size == 16 || dest_bit_size == 32 ||
                                dest_bit_size == 64);
   }

   validate_def(&instr->def, state);
}

static void
validate_var_use(nir_variable *var, validate_state *state)
{
   struct hash_entry *entry = _mesa_hash_table_search(state->var_defs, var);
   validate_assert(state, entry);
   if (entry && var->data.mode == nir_var_function_temp)
      validate_assert(state, (nir_function_impl *)entry->data == state->impl);
}

static void
validate_deref_instr(nir_deref_instr *instr, validate_state *state)
{
   if (instr->deref_type == nir_deref_type_var) {
      /* Variable dereferences are stupid simple. */
      validate_assert(state, instr->modes == instr->var->data.mode);
      validate_assert(state, instr->type == instr->var->type);
      validate_var_use(instr->var, state);
   } else if (instr->deref_type == nir_deref_type_cast) {
      /* For cast, we simply have to trust the instruction.  It's up to
       * lowering passes and front/back-ends to make them sane.
       */
      validate_src(&instr->parent, state);

      /* Most variable modes in NIR can only exist by themselves. */
      if (instr->modes & ~nir_var_mem_generic)
         validate_assert(state, util_bitcount(instr->modes) == 1);

      nir_deref_instr *parent = nir_src_as_deref(instr->parent);
      if (parent) {
         /* Casts can change the mode but it can't change completely.  The new
          * mode must have some bits in common with the old.
          */
         validate_assert(state, instr->modes & parent->modes);
      } else {
         /* If our parent isn't a deref, just assert the mode is there */
         validate_assert(state, instr->modes != 0);
      }

      /* We just validate that the type is there */
      validate_assert(state, instr->type);
      if (instr->cast.align_mul > 0) {
         validate_assert(state, util_is_power_of_two_nonzero(instr->cast.align_mul));
         validate_assert(state, instr->cast.align_offset < instr->cast.align_mul);
      } else {
         validate_assert(state, instr->cast.align_offset == 0);
      }
   } else {
      /* The parent pointer value must have the same number of components
       * as the destination.
       */
      validate_sized_src(&instr->parent, state, instr->def.bit_size,
                         instr->def.num_components);

      nir_instr *parent_instr = instr->parent.ssa->parent_instr;

      /* The parent must come from another deref instruction */
      validate_assert(state, parent_instr->type == nir_instr_type_deref);

      nir_deref_instr *parent = nir_instr_as_deref(parent_instr);

      validate_assert(state, instr->modes == parent->modes);

      switch (instr->deref_type) {
      case nir_deref_type_struct:
         validate_assert(state, glsl_type_is_struct_or_ifc(parent->type));
         validate_assert(state,
                         instr->strct.index < glsl_get_length(parent->type));
         validate_assert(state, instr->type ==
                                   glsl_get_struct_field(parent->type, instr->strct.index));
         break;

      case nir_deref_type_array:
      case nir_deref_type_array_wildcard:
         if (instr->modes & nir_var_vec_indexable_modes) {
            /* Shared variables and UBO/SSBOs have a bit more relaxed rules
             * because we need to be able to handle array derefs on vectors.
             * Fortunately, nir_lower_io handles these just fine.
             */
            validate_assert(state, glsl_type_is_array(parent->type) ||
                                      glsl_type_is_matrix(parent->type) ||
                                      glsl_type_is_vector(parent->type));
         } else {
            /* Most of NIR cannot handle array derefs on vectors */
            validate_assert(state, glsl_type_is_array(parent->type) ||
                                      glsl_type_is_matrix(parent->type));
         }
         validate_assert(state,
                         instr->type == glsl_get_array_element(parent->type));

         if (instr->deref_type == nir_deref_type_array) {
            validate_sized_src(&instr->arr.index, state,
                               instr->def.bit_size, 1);
         }
         break;

      case nir_deref_type_ptr_as_array:
         /* ptr_as_array derefs must have a parent that is either an array,
          * ptr_as_array, or cast.  If the parent is a cast, we get the stride
          * information (if any) from the cast deref.
          */
         validate_assert(state,
                         parent->deref_type == nir_deref_type_array ||
                            parent->deref_type == nir_deref_type_ptr_as_array ||
                            parent->deref_type == nir_deref_type_cast);
         validate_sized_src(&instr->arr.index, state,
                            instr->def.bit_size, 1);
         break;

      default:
         unreachable("Invalid deref instruction type");
      }
   }

   /* We intentionally don't validate the size of the destination because we
    * want to let other compiler components such as SPIR-V decide how big
    * pointers should be.
    */
   validate_def(&instr->def, state);

   /* Certain modes cannot be used as sources for phi instructions because
    * way too many passes assume that they can always chase deref chains.
    */
   nir_foreach_use_including_if(use, &instr->def) {
      /* Deref instructions as if conditions don't make sense because if
       * conditions expect well-formed Booleans.  If you want to compare with
       * NULL, an explicit comparison operation should be used.
       */
      if (!validate_assert(state, !nir_src_is_if(use)))
         continue;

      if (src_parent_instr_safe(use)->type == nir_instr_type_phi) {
         validate_assert(state, !(instr->modes & (nir_var_shader_in |
                                                  nir_var_shader_out |
                                                  nir_var_shader_out |
                                                  nir_var_uniform)));
      }
   }
}

static bool
vectorized_intrinsic(nir_intrinsic_instr *intr)
{
   const nir_intrinsic_info *info = &nir_intrinsic_infos[intr->intrinsic];

   if (info->dest_components == 0)
      return true;

   for (unsigned i = 0; i < info->num_srcs; i++)
      if (info->src_components[i] == 0)
         return true;

   return false;
}

/** Returns the image format or PIPE_FORMAT_COUNT for incomplete derefs
 *
 * We use PIPE_FORMAT_COUNT for incomplete derefs because PIPE_FORMAT_NONE
 * indicates that we found the variable but it has no format specified.
 */
static enum pipe_format
image_intrin_format(nir_intrinsic_instr *instr)
{
   if (nir_intrinsic_format(instr) != PIPE_FORMAT_NONE)
      return nir_intrinsic_format(instr);

   /* If this not a deref intrinsic, PIPE_FORMAT_NONE is the best we can do */
   if (nir_intrinsic_infos[instr->intrinsic].src_components[0] != -1)
      return PIPE_FORMAT_NONE;

   nir_variable *var = nir_intrinsic_get_var(instr, 0);
   if (var == NULL)
      return PIPE_FORMAT_COUNT;

   return var->data.image.format;
}

static void
validate_register_handle(nir_src handle_src,
                         unsigned num_components,
                         unsigned bit_size,
                         validate_state *state)
{
   nir_def *handle = handle_src.ssa;
   nir_instr *parent = handle->parent_instr;

   if (!validate_assert(state, parent->type == nir_instr_type_intrinsic))
      return;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(parent);
   if (!validate_assert(state, intr->intrinsic == nir_intrinsic_decl_reg))
      return;

   validate_assert(state, nir_intrinsic_num_components(intr) == num_components);
   validate_assert(state, nir_intrinsic_bit_size(intr) == bit_size);
}

static void
validate_intrinsic_instr(nir_intrinsic_instr *instr, validate_state *state)
{
   unsigned dest_bit_size = 0;
   unsigned src_bit_sizes[NIR_INTRINSIC_MAX_INPUTS] = {
      0,
   };
   switch (instr->intrinsic) {
   case nir_intrinsic_decl_reg:
      assert(state->block == nir_start_block(state->impl));
      break;

   case nir_intrinsic_load_reg:
   case nir_intrinsic_load_reg_indirect:
      validate_register_handle(instr->src[0],
                               instr->def.num_components,
                               instr->def.bit_size, state);
      break;

   case nir_intrinsic_store_reg:
   case nir_intrinsic_store_reg_indirect:
      validate_register_handle(instr->src[1],
                               nir_src_num_components(instr->src[0]),
                               nir_src_bit_size(instr->src[0]), state);
      break;

   case nir_intrinsic_convert_alu_types: {
      nir_alu_type src_type = nir_intrinsic_src_type(instr);
      nir_alu_type dest_type = nir_intrinsic_dest_type(instr);
      dest_bit_size = nir_alu_type_get_type_size(dest_type);
      src_bit_sizes[0] = nir_alu_type_get_type_size(src_type);
      validate_assert(state, dest_bit_size != 0);
      validate_assert(state, src_bit_sizes[0] != 0);
      break;
   }

   case nir_intrinsic_load_param: {
      unsigned param_idx = nir_intrinsic_param_idx(instr);
      validate_assert(state, param_idx < state->impl->function->num_params);
      nir_parameter *param = &state->impl->function->params[param_idx];
      validate_assert(state, instr->num_components == param->num_components);
      dest_bit_size = param->bit_size;
      break;
   }

   case nir_intrinsic_load_deref: {
      nir_deref_instr *src = nir_src_as_deref(instr->src[0]);
      assert(src);
      validate_assert(state, glsl_type_is_vector_or_scalar(src->type) ||
                                (src->modes == nir_var_uniform &&
                                 glsl_get_base_type(src->type) == GLSL_TYPE_SUBROUTINE));
      validate_assert(state, instr->num_components ==
                                glsl_get_vector_elements(src->type));
      dest_bit_size = glsl_get_bit_size(src->type);
      /* Also allow 32-bit boolean load operations */
      if (glsl_type_is_boolean(src->type))
         dest_bit_size |= 32;
      break;
   }

   case nir_intrinsic_store_deref: {
      nir_deref_instr *dst = nir_src_as_deref(instr->src[0]);
      assert(dst);
      validate_assert(state, glsl_type_is_vector_or_scalar(dst->type));
      validate_assert(state, instr->num_components ==
                                glsl_get_vector_elements(dst->type));
      src_bit_sizes[1] = glsl_get_bit_size(dst->type);
      /* Also allow 32-bit boolean store operations */
      if (glsl_type_is_boolean(dst->type))
         src_bit_sizes[1] |= 32;
      validate_assert(state, !nir_deref_mode_may_be(dst, nir_var_read_only_modes));
      break;
   }

   case nir_intrinsic_copy_deref: {
      nir_deref_instr *dst = nir_src_as_deref(instr->src[0]);
      nir_deref_instr *src = nir_src_as_deref(instr->src[1]);
      validate_assert(state, glsl_get_bare_type(dst->type) ==
                                glsl_get_bare_type(src->type));
      validate_assert(state, !nir_deref_mode_may_be(dst, nir_var_read_only_modes));
      /* FIXME: now that we track if the var copies were lowered, it would be
       * good to validate here that no new copy derefs were added. Right now
       * we can't as there are some specific cases where copies are added even
       * after the lowering. One example is the Intel compiler, that calls
       * nir_lower_io_to_temporaries when linking some shader stages.
       */
      break;
   }

   case nir_intrinsic_load_ubo_vec4: {
      int bit_size = instr->def.bit_size;
      validate_assert(state, bit_size >= 8);
      validate_assert(state, (nir_intrinsic_component(instr) +
                              instr->num_components) *
                                   (bit_size / 8) <=
                                16);
      break;
   }

   case nir_intrinsic_load_ubo:
      /* Make sure that the creator didn't forget to set the range_base+range. */
      validate_assert(state, nir_intrinsic_range(instr) != 0);
      FALLTHROUGH;
   case nir_intrinsic_load_ssbo:
   case nir_intrinsic_load_shared:
   case nir_intrinsic_load_global:
   case nir_intrinsic_load_global_constant:
   case nir_intrinsic_load_scratch:
   case nir_intrinsic_load_constant:
      /* These memory load operations must have alignments */
      validate_assert(state,
                      util_is_power_of_two_nonzero(nir_intrinsic_align_mul(instr)));
      validate_assert(state, nir_intrinsic_align_offset(instr) <
                                nir_intrinsic_align_mul(instr));
      FALLTHROUGH;

   case nir_intrinsic_load_uniform:
   case nir_intrinsic_load_input:
   case nir_intrinsic_load_per_vertex_input:
   case nir_intrinsic_load_interpolated_input:
   case nir_intrinsic_load_output:
   case nir_intrinsic_load_per_vertex_output:
   case nir_intrinsic_load_per_primitive_output:
   case nir_intrinsic_load_push_constant:
      /* All memory load operations must load at least a byte */
      validate_assert(state, instr->def.bit_size >= 8);
      break;

   case nir_intrinsic_store_ssbo:
   case nir_intrinsic_store_shared:
   case nir_intrinsic_store_global:
   case nir_intrinsic_store_scratch:
      /* These memory store operations must also have alignments */
      validate_assert(state,
                      util_is_power_of_two_nonzero(nir_intrinsic_align_mul(instr)));
      validate_assert(state, nir_intrinsic_align_offset(instr) <
                                nir_intrinsic_align_mul(instr));
      FALLTHROUGH;

   case nir_intrinsic_store_output:
   case nir_intrinsic_store_per_vertex_output:
      /* All memory store operations must store at least a byte */
      validate_assert(state, nir_src_bit_size(instr->src[0]) >= 8);
      break;

   case nir_intrinsic_deref_mode_is:
   case nir_intrinsic_addr_mode_is:
      validate_assert(state,
                      util_bitcount(nir_intrinsic_memory_modes(instr)) == 1);
      break;

   case nir_intrinsic_image_deref_atomic:
   case nir_intrinsic_image_deref_atomic_swap:
   case nir_intrinsic_bindless_image_atomic:
   case nir_intrinsic_bindless_image_atomic_swap:
   case nir_intrinsic_image_atomic:
   case nir_intrinsic_image_atomic_swap: {
      nir_atomic_op op = nir_intrinsic_atomic_op(instr);

      enum pipe_format format = image_intrin_format(instr);
      if (format != PIPE_FORMAT_COUNT) {
         bool allowed = false;
         bool is_float = (nir_atomic_op_type(op) == nir_type_float);

         switch (format) {
         case PIPE_FORMAT_R32_FLOAT:
            allowed = is_float || op == nir_atomic_op_xchg;
            break;
         case PIPE_FORMAT_R16_FLOAT:
         case PIPE_FORMAT_R64_FLOAT:
            allowed = op == nir_atomic_op_fmin || op == nir_atomic_op_fmax;
            break;
         case PIPE_FORMAT_R32_UINT:
         case PIPE_FORMAT_R32_SINT:
         case PIPE_FORMAT_R64_UINT:
         case PIPE_FORMAT_R64_SINT:
            allowed = !is_float;
            break;
         default:
            break;
         }

         validate_assert(state, allowed);
         validate_assert(state, instr->def.bit_size ==
                                   util_format_get_blocksizebits(format));
      }
      break;
   }

   case nir_intrinsic_store_buffer_amd:
      if (nir_intrinsic_access(instr) & ACCESS_USES_FORMAT_AMD) {
         unsigned writemask = nir_intrinsic_write_mask(instr);

         /* Make sure the writemask is derived from the component count. */
         validate_assert(state,
                         writemask ==
                            BITFIELD_MASK(nir_src_num_components(instr->src[0])));
      }
      break;

   default:
      break;
   }

   if (instr->num_components > 0)
      validate_num_components(state, instr->num_components);

   const nir_intrinsic_info *info = &nir_intrinsic_infos[instr->intrinsic];
   unsigned num_srcs = info->num_srcs;
   for (unsigned i = 0; i < num_srcs; i++) {
      unsigned components_read = nir_intrinsic_src_components(instr, i);

      validate_num_components(state, components_read);

      validate_sized_src(&instr->src[i], state, src_bit_sizes[i], components_read);
   }

   if (nir_intrinsic_infos[instr->intrinsic].has_dest) {
      unsigned components_written = nir_intrinsic_dest_components(instr);
      unsigned bit_sizes = info->dest_bit_sizes;
      if (!bit_sizes && info->bit_size_src >= 0)
         bit_sizes = nir_src_bit_size(instr->src[info->bit_size_src]);

      validate_num_components(state, components_written);
      if (dest_bit_size && bit_sizes)
         validate_assert(state, dest_bit_size & bit_sizes);
      else
         dest_bit_size = dest_bit_size ? dest_bit_size : bit_sizes;

      validate_def(&instr->def, state);
      validate_assert(state, instr->def.num_components == components_written);

      if (dest_bit_size)
         validate_assert(state, instr->def.bit_size & dest_bit_size);
   }

   if (!vectorized_intrinsic(instr))
      validate_assert(state, instr->num_components == 0);

   if (nir_intrinsic_has_write_mask(instr)) {
      unsigned component_mask = BITFIELD_MASK(instr->num_components);
      validate_assert(state, (nir_intrinsic_write_mask(instr) & ~component_mask) == 0);
   }

   if (nir_intrinsic_has_io_xfb(instr)) {
      unsigned used_mask = 0;

      for (unsigned i = 0; i < 4; i++) {
         nir_io_xfb xfb = i < 2 ? nir_intrinsic_io_xfb(instr) : nir_intrinsic_io_xfb2(instr);
         unsigned xfb_mask = BITFIELD_RANGE(i, xfb.out[i % 2].num_components);

         /* Each component can be used only once by transform feedback info. */
         validate_assert(state, (xfb_mask & used_mask) == 0);
         used_mask |= xfb_mask;
      }
   }

   if (nir_intrinsic_has_io_semantics(instr) &&
       !nir_intrinsic_infos[instr->intrinsic].has_dest) {
      nir_io_semantics sem = nir_intrinsic_io_semantics(instr);

      /* An output that has no effect shouldn't be present in the IR. */
      validate_assert(state,
                      (nir_slot_is_sysval_output(sem.location, MESA_SHADER_NONE) &&
                       !sem.no_sysval_output) ||
                         (nir_slot_is_varying(sem.location) && !sem.no_varying) ||
                         nir_instr_xfb_write_mask(instr));
   }
}

static void
validate_tex_instr(nir_tex_instr *instr, validate_state *state)
{
   bool src_type_seen[nir_num_tex_src_types];
   for (unsigned i = 0; i < nir_num_tex_src_types; i++)
      src_type_seen[i] = false;

   for (unsigned i = 0; i < instr->num_srcs; i++) {
      validate_assert(state, !src_type_seen[instr->src[i].src_type]);
      src_type_seen[instr->src[i].src_type] = true;
      validate_sized_src(&instr->src[i].src, state,
                         0, nir_tex_instr_src_size(instr, i));

      switch (instr->src[i].src_type) {

      case nir_tex_src_comparator:
         validate_assert(state, instr->is_shadow);
         break;

      case nir_tex_src_bias:
         validate_assert(state, instr->op == nir_texop_txb ||
                                   instr->op == nir_texop_tg4 ||
                                   instr->op == nir_texop_lod);
         break;

      case nir_tex_src_lod:
         validate_assert(state, instr->op != nir_texop_tex &&
                                   instr->op != nir_texop_txb &&
                                   instr->op != nir_texop_txd &&
                                   instr->op != nir_texop_lod);
         break;

      case nir_tex_src_ddx:
      case nir_tex_src_ddy:
         validate_assert(state, instr->op == nir_texop_txd);
         break;

      case nir_tex_src_texture_deref: {
         nir_deref_instr *deref = nir_src_as_deref(instr->src[i].src);
         if (!validate_assert(state, deref))
            break;

         validate_assert(state, glsl_type_is_image(deref->type) ||
                                   glsl_type_is_texture(deref->type) ||
                                   glsl_type_is_sampler(deref->type));
         switch (instr->op) {
         case nir_texop_descriptor_amd:
         case nir_texop_sampler_descriptor_amd:
            break;
         case nir_texop_lod:
         case nir_texop_lod_bias_agx:
            validate_assert(state, nir_alu_type_get_base_type(instr->dest_type) == nir_type_float);
            break;
         case nir_texop_samples_identical:
            validate_assert(state, nir_alu_type_get_base_type(instr->dest_type) == nir_type_bool);
            break;
         case nir_texop_txs:
         case nir_texop_texture_samples:
         case nir_texop_query_levels:
         case nir_texop_fragment_mask_fetch_amd:
         case nir_texop_txf_ms_mcs_intel:
            validate_assert(state, nir_alu_type_get_base_type(instr->dest_type) == nir_type_int ||
                                      nir_alu_type_get_base_type(instr->dest_type) == nir_type_uint);

            break;
         default:
            validate_assert(state,
                            glsl_get_sampler_result_type(deref->type) == GLSL_TYPE_VOID ||
                               glsl_base_type_is_integer(glsl_get_sampler_result_type(deref->type)) ==
                                  (nir_alu_type_get_base_type(instr->dest_type) == nir_type_int ||
                                   nir_alu_type_get_base_type(instr->dest_type) == nir_type_uint));
         }
         break;
      }

      case nir_tex_src_sampler_deref: {
         nir_deref_instr *deref = nir_src_as_deref(instr->src[i].src);
         if (!validate_assert(state, deref))
            break;

         validate_assert(state, glsl_type_is_sampler(deref->type));
         break;
      }

      case nir_tex_src_coord:
      case nir_tex_src_projector:
      case nir_tex_src_offset:
      case nir_tex_src_min_lod:
      case nir_tex_src_ms_index:
      case nir_tex_src_texture_offset:
      case nir_tex_src_sampler_offset:
      case nir_tex_src_plane:
      case nir_tex_src_texture_handle:
      case nir_tex_src_sampler_handle:
         break;

      default:
         break;
      }
   }

   bool msaa = (instr->sampler_dim == GLSL_SAMPLER_DIM_MS ||
                instr->sampler_dim == GLSL_SAMPLER_DIM_SUBPASS_MS);

   if (msaa)
      validate_assert(state, instr->op != nir_texop_txf);
   else
      validate_assert(state, instr->op != nir_texop_txf_ms);

   if (instr->op != nir_texop_tg4)
      validate_assert(state, instr->component == 0);

   if (nir_tex_instr_has_explicit_tg4_offsets(instr)) {
      validate_assert(state, instr->op == nir_texop_tg4);
      validate_assert(state, !src_type_seen[nir_tex_src_offset]);
   }

   if (instr->is_gather_implicit_lod)
      validate_assert(state, instr->op == nir_texop_tg4);

   validate_def(&instr->def, state);
   validate_assert(state, instr->def.num_components ==
                             nir_tex_instr_dest_size(instr));

   unsigned bit_size = nir_alu_type_get_type_size(instr->dest_type);
   validate_assert(state,
                   (bit_size ? bit_size : 32) ==
                      instr->def.bit_size);
}

static void
validate_call_instr(nir_call_instr *instr, validate_state *state)
{
   validate_assert(state, instr->num_params == instr->callee->num_params);

   for (unsigned i = 0; i < instr->num_params; i++) {
      validate_sized_src(&instr->params[i], state,
                         instr->callee->params[i].bit_size,
                         instr->callee->params[i].num_components);
   }
}

static void
validate_const_value(nir_const_value *val, unsigned bit_size,
                     bool is_null_constant, validate_state *state)
{
   /* In order for block copies to work properly for things like instruction
    * comparisons and [de]serialization, we require the unused bits of the
    * nir_const_value to be zero.
    */
   nir_const_value cmp_val;
   memset(&cmp_val, 0, sizeof(cmp_val));
   if (!is_null_constant) {
      switch (bit_size) {
      case 1:
         cmp_val.b = val->b;
         break;
      case 8:
         cmp_val.u8 = val->u8;
         break;
      case 16:
         cmp_val.u16 = val->u16;
         break;
      case 32:
         cmp_val.u32 = val->u32;
         break;
      case 64:
         cmp_val.u64 = val->u64;
         break;
      default:
         validate_assert(state, !"Invalid load_const bit size");
      }
   }
   validate_assert(state, memcmp(val, &cmp_val, sizeof(cmp_val)) == 0);
}

static void
validate_load_const_instr(nir_load_const_instr *instr, validate_state *state)
{
   validate_def(&instr->def, state);

   for (unsigned i = 0; i < instr->def.num_components; i++)
      validate_const_value(&instr->value[i], instr->def.bit_size, false, state);
}

static void
validate_ssa_undef_instr(nir_undef_instr *instr, validate_state *state)
{
   validate_def(&instr->def, state);
}

static void
validate_phi_instr(nir_phi_instr *instr, validate_state *state)
{
   /*
    * don't validate the sources until we get to them from their predecessor
    * basic blocks, to avoid validating an SSA use before its definition.
    */

   validate_def(&instr->def, state);

   exec_list_validate(&instr->srcs);
   validate_assert(state, exec_list_length(&instr->srcs) ==
                             state->block->predecessors->entries);
}

static void
validate_jump_instr(nir_jump_instr *instr, validate_state *state)
{
   nir_block *block = state->block;
   validate_assert(state, &instr->instr == nir_block_last_instr(block));

   switch (instr->type) {
   case nir_jump_return:
   case nir_jump_halt:
      validate_assert(state, block->successors[0] == state->impl->end_block);
      validate_assert(state, block->successors[1] == NULL);
      validate_assert(state, instr->target == NULL);
      validate_assert(state, instr->else_target == NULL);
      validate_assert(state, !state->in_loop_continue_construct);
      break;

   case nir_jump_break:
      validate_assert(state, state->impl->structured);
      validate_assert(state, state->loop != NULL);
      if (state->loop) {
         nir_block *after =
            nir_cf_node_as_block(nir_cf_node_next(&state->loop->cf_node));
         validate_assert(state, block->successors[0] == after);
      }
      validate_assert(state, block->successors[1] == NULL);
      validate_assert(state, instr->target == NULL);
      validate_assert(state, instr->else_target == NULL);
      break;

   case nir_jump_continue:
      validate_assert(state, state->impl->structured);
      validate_assert(state, state->loop != NULL);
      if (state->loop) {
         nir_block *cont_block = nir_loop_continue_target(state->loop);
         validate_assert(state, block->successors[0] == cont_block);
      }
      validate_assert(state, block->successors[1] == NULL);
      validate_assert(state, instr->target == NULL);
      validate_assert(state, instr->else_target == NULL);
      validate_assert(state, !state->in_loop_continue_construct);
      break;

   case nir_jump_goto:
      validate_assert(state, !state->impl->structured);
      validate_assert(state, instr->target == block->successors[0]);
      validate_assert(state, instr->target != NULL);
      validate_assert(state, instr->else_target == NULL);
      break;

   case nir_jump_goto_if:
      validate_assert(state, !state->impl->structured);
      validate_assert(state, instr->target == block->successors[1]);
      validate_assert(state, instr->else_target == block->successors[0]);
      validate_sized_src(&instr->condition, state, 0, 1);
      validate_assert(state, instr->target != NULL);
      validate_assert(state, instr->else_target != NULL);
      break;

   default:
      validate_assert(state, !"Invalid jump instruction type");
      break;
   }
}

static void
validate_instr(nir_instr *instr, validate_state *state)
{
   validate_assert(state, instr->block == state->block);

   state->instr = instr;

   switch (instr->type) {
   case nir_instr_type_alu:
      validate_alu_instr(nir_instr_as_alu(instr), state);
      break;

   case nir_instr_type_deref:
      validate_deref_instr(nir_instr_as_deref(instr), state);
      break;

   case nir_instr_type_call:
      validate_call_instr(nir_instr_as_call(instr), state);
      break;

   case nir_instr_type_intrinsic:
      validate_intrinsic_instr(nir_instr_as_intrinsic(instr), state);
      break;

   case nir_instr_type_tex:
      validate_tex_instr(nir_instr_as_tex(instr), state);
      break;

   case nir_instr_type_load_const:
      validate_load_const_instr(nir_instr_as_load_const(instr), state);
      break;

   case nir_instr_type_phi:
      validate_phi_instr(nir_instr_as_phi(instr), state);
      break;

   case nir_instr_type_undef:
      validate_ssa_undef_instr(nir_instr_as_undef(instr), state);
      break;

   case nir_instr_type_jump:
      validate_jump_instr(nir_instr_as_jump(instr), state);
      break;

   default:
      validate_assert(state, !"Invalid ALU instruction type");
      break;
   }

   state->instr = NULL;
}

static void
validate_phi_src(nir_phi_instr *instr, nir_block *pred, validate_state *state)
{
   state->instr = &instr->instr;

   exec_list_validate(&instr->srcs);
   nir_foreach_phi_src(src, instr) {
      if (src->pred == pred) {
         validate_sized_src(&src->src, state, instr->def.bit_size,
                            instr->def.num_components);
         state->instr = NULL;
         return;
      }
   }
   validate_assert(state, !"Phi does not have a source corresponding to one "
                           "of its predecessor blocks");
}

static void
validate_phi_srcs(nir_block *block, nir_block *succ, validate_state *state)
{
   nir_foreach_phi(phi, succ) {
      validate_phi_src(phi, block, state);
   }
}

static void
collect_blocks(struct exec_list *cf_list, validate_state *state)
{
   /* We walk the blocks manually here rather than using nir_foreach_block for
    * a few reasons:
    *
    *  1. nir_foreach_block() doesn't work properly for unstructured NIR and
    *     we need to be able to handle all forms of NIR here.
    *
    *  2. We want to call exec_list_validate() on every linked list in the IR
    *     which means we need to touch every linked and just walking blocks
    *     with nir_foreach_block() would make that difficult.  In particular,
    *     we want to validate each list before the first time we walk it so
    *     that we catch broken lists in exec_list_validate() instead of
    *     getting stuck in a hard-to-debug infinite loop in the validator.
    *
    *  3. nir_foreach_block() depends on several invariants of the CF node
    *     hierarchy which nir_validate_shader() is responsible for verifying.
    *     If we used nir_foreach_block() in nir_validate_shader(), we could
    *     end up blowing up on a bad list walk instead of throwing the much
    *     easier to debug validation error.
    */
   exec_list_validate(cf_list);
   foreach_list_typed(nir_cf_node, node, node, cf_list) {
      switch (node->type) {
      case nir_cf_node_block:
         _mesa_set_add(state->blocks, nir_cf_node_as_block(node));
         break;

      case nir_cf_node_if:
         collect_blocks(&nir_cf_node_as_if(node)->then_list, state);
         collect_blocks(&nir_cf_node_as_if(node)->else_list, state);
         break;

      case nir_cf_node_loop:
         collect_blocks(&nir_cf_node_as_loop(node)->body, state);
         collect_blocks(&nir_cf_node_as_loop(node)->continue_list, state);
         break;

      default:
         unreachable("Invalid CF node type");
      }
   }
}

static void validate_cf_node(nir_cf_node *node, validate_state *state);

static void
validate_block_predecessors(nir_block *block, validate_state *state)
{
   for (unsigned i = 0; i < 2; i++) {
      if (block->successors[i] == NULL)
         continue;

      /* The block has to exist in the nir_function_impl */
      validate_assert(state, _mesa_set_search(state->blocks,
                                              block->successors[i]));

      /* And we have to be in our successor's predecessors set */
      validate_assert(state,
                      _mesa_set_search(block->successors[i]->predecessors, block));

      validate_phi_srcs(block, block->successors[i], state);
   }

   /* The start block cannot have any predecessors */
   if (block == nir_start_block(state->impl))
      validate_assert(state, block->predecessors->entries == 0);

   set_foreach(block->predecessors, entry) {
      const nir_block *pred = entry->key;
      validate_assert(state, _mesa_set_search(state->blocks, pred));
      validate_assert(state, pred->successors[0] == block ||
                                pred->successors[1] == block);
   }
}

static void
validate_block(nir_block *block, validate_state *state)
{
   validate_assert(state, block->cf_node.parent == state->parent_node);

   state->block = block;

   exec_list_validate(&block->instr_list);
   nir_foreach_instr(instr, block) {
      if (instr->type == nir_instr_type_phi) {
         validate_assert(state, instr == nir_block_first_instr(block) ||
                                   nir_instr_prev(instr)->type == nir_instr_type_phi);
      }

      validate_instr(instr, state);
   }

   validate_assert(state, block->successors[0] != NULL);
   validate_assert(state, block->successors[0] != block->successors[1]);
   validate_block_predecessors(block, state);

   if (!state->impl->structured) {
      validate_assert(state, nir_block_ends_in_jump(block));
   } else if (!nir_block_ends_in_jump(block)) {
      nir_cf_node *next = nir_cf_node_next(&block->cf_node);
      if (next == NULL) {
         switch (state->parent_node->type) {
         case nir_cf_node_loop: {
            if (block == nir_loop_last_block(state->loop)) {
               nir_block *cont = nir_loop_continue_target(state->loop);
               validate_assert(state, block->successors[0] == cont);
            } else {
               validate_assert(state, nir_loop_has_continue_construct(state->loop) &&
                                         block == nir_loop_last_continue_block(state->loop));
               nir_block *head = nir_loop_first_block(state->loop);
               validate_assert(state, block->successors[0] == head);
            }
            /* due to the hack for infinite loops, block->successors[1] may
             * point to the block after the loop.
             */
            break;
         }

         case nir_cf_node_if: {
            nir_block *after =
               nir_cf_node_as_block(nir_cf_node_next(state->parent_node));
            validate_assert(state, block->successors[0] == after);
            validate_assert(state, block->successors[1] == NULL);
            break;
         }

         case nir_cf_node_function:
            validate_assert(state, block->successors[0] == state->impl->end_block);
            validate_assert(state, block->successors[1] == NULL);
            break;

         default:
            unreachable("unknown control flow node type");
         }
      } else {
         if (next->type == nir_cf_node_if) {
            nir_if *if_stmt = nir_cf_node_as_if(next);
            validate_assert(state, block->successors[0] ==
                                      nir_if_first_then_block(if_stmt));
            validate_assert(state, block->successors[1] ==
                                      nir_if_first_else_block(if_stmt));
         } else if (next->type == nir_cf_node_loop) {
            nir_loop *loop = nir_cf_node_as_loop(next);
            validate_assert(state, block->successors[0] ==
                                      nir_loop_first_block(loop));
            validate_assert(state, block->successors[1] == NULL);
         } else {
            validate_assert(state,
                            !"Structured NIR cannot have consecutive blocks");
         }
      }
   }
}

static void
validate_end_block(nir_block *block, validate_state *state)
{
   validate_assert(state, block->cf_node.parent == &state->impl->cf_node);

   exec_list_validate(&block->instr_list);
   validate_assert(state, exec_list_is_empty(&block->instr_list));

   validate_assert(state, block->successors[0] == NULL);
   validate_assert(state, block->successors[1] == NULL);
   validate_block_predecessors(block, state);
}

static void
validate_if(nir_if *if_stmt, validate_state *state)
{
   validate_assert(state, state->impl->structured);

   state->if_stmt = if_stmt;

   validate_assert(state, !exec_node_is_head_sentinel(if_stmt->cf_node.node.prev));
   nir_cf_node *prev_node = nir_cf_node_prev(&if_stmt->cf_node);
   validate_assert(state, prev_node->type == nir_cf_node_block);

   validate_assert(state, !exec_node_is_tail_sentinel(if_stmt->cf_node.node.next));
   nir_cf_node *next_node = nir_cf_node_next(&if_stmt->cf_node);
   validate_assert(state, next_node->type == nir_cf_node_block);

   validate_assert(state, nir_src_is_if(&if_stmt->condition));
   validate_if_src(&if_stmt->condition, state);

   validate_assert(state, !exec_list_is_empty(&if_stmt->then_list));
   validate_assert(state, !exec_list_is_empty(&if_stmt->else_list));

   nir_cf_node *old_parent = state->parent_node;
   state->parent_node = &if_stmt->cf_node;

   foreach_list_typed(nir_cf_node, cf_node, node, &if_stmt->then_list) {
      validate_cf_node(cf_node, state);
   }

   foreach_list_typed(nir_cf_node, cf_node, node, &if_stmt->else_list) {
      validate_cf_node(cf_node, state);
   }

   state->parent_node = old_parent;
   state->if_stmt = NULL;
}

static void
validate_loop(nir_loop *loop, validate_state *state)
{
   validate_assert(state, state->impl->structured);

   validate_assert(state, !exec_node_is_head_sentinel(loop->cf_node.node.prev));
   nir_cf_node *prev_node = nir_cf_node_prev(&loop->cf_node);
   validate_assert(state, prev_node->type == nir_cf_node_block);

   validate_assert(state, !exec_node_is_tail_sentinel(loop->cf_node.node.next));
   nir_cf_node *next_node = nir_cf_node_next(&loop->cf_node);
   validate_assert(state, next_node->type == nir_cf_node_block);

   validate_assert(state, !exec_list_is_empty(&loop->body));

   nir_cf_node *old_parent = state->parent_node;
   state->parent_node = &loop->cf_node;
   nir_loop *old_loop = state->loop;
   bool old_continue_construct = state->in_loop_continue_construct;
   state->loop = loop;
   state->in_loop_continue_construct = false;

   foreach_list_typed(nir_cf_node, cf_node, node, &loop->body) {
      validate_cf_node(cf_node, state);
   }
   state->in_loop_continue_construct = true;
   foreach_list_typed(nir_cf_node, cf_node, node, &loop->continue_list) {
      validate_cf_node(cf_node, state);
   }
   state->in_loop_continue_construct = false;
   state->parent_node = old_parent;
   state->loop = old_loop;
   state->in_loop_continue_construct = old_continue_construct;
}

static void
validate_cf_node(nir_cf_node *node, validate_state *state)
{
   validate_assert(state, node->parent == state->parent_node);

   switch (node->type) {
   case nir_cf_node_block:
      validate_block(nir_cf_node_as_block(node), state);
      break;

   case nir_cf_node_if:
      validate_if(nir_cf_node_as_if(node), state);
      break;

   case nir_cf_node_loop:
      validate_loop(nir_cf_node_as_loop(node), state);
      break;

   default:
      unreachable("Invalid CF node type");
   }
}

static void
validate_constant(nir_constant *c, const struct glsl_type *type,
                  validate_state *state)
{
   if (glsl_type_is_vector_or_scalar(type)) {
      unsigned num_components = glsl_get_vector_elements(type);
      unsigned bit_size = glsl_get_bit_size(type);
      for (unsigned i = 0; i < num_components; i++)
         validate_const_value(&c->values[i], bit_size, c->is_null_constant, state);
      for (unsigned i = num_components; i < NIR_MAX_VEC_COMPONENTS; i++)
         validate_assert(state, c->values[i].u64 == 0);
   } else {
      validate_assert(state, c->num_elements == glsl_get_length(type));
      if (glsl_type_is_struct_or_ifc(type)) {
         for (unsigned i = 0; i < c->num_elements; i++) {
            const struct glsl_type *elem_type = glsl_get_struct_field(type, i);
            validate_constant(c->elements[i], elem_type, state);
            validate_assert(state, !c->is_null_constant || c->elements[i]->is_null_constant);
         }
      } else if (glsl_type_is_array_or_matrix(type)) {
         const struct glsl_type *elem_type = glsl_get_array_element(type);
         for (unsigned i = 0; i < c->num_elements; i++) {
            validate_constant(c->elements[i], elem_type, state);
            validate_assert(state, !c->is_null_constant || c->elements[i]->is_null_constant);
         }
      } else {
         validate_assert(state, !"Invalid type for nir_constant");
      }
   }
}

static void
validate_var_decl(nir_variable *var, nir_variable_mode valid_modes,
                  validate_state *state)
{
   state->var = var;

   /* Must have exactly one mode set */
   validate_assert(state, util_is_power_of_two_nonzero(var->data.mode));
   validate_assert(state, var->data.mode & valid_modes);

   if (var->data.compact) {
      /* The "compact" flag is only valid on arrays of scalars. */
      assert(glsl_type_is_array(var->type));

      const struct glsl_type *type = glsl_get_array_element(var->type);
      if (nir_is_arrayed_io(var, state->shader->info.stage)) {
         if (var->data.per_view) {
            assert(glsl_type_is_array(type));
            type = glsl_get_array_element(type);
         }
         assert(glsl_type_is_array(type));
         assert(glsl_type_is_scalar(glsl_get_array_element(type)));
      } else {
         assert(glsl_type_is_scalar(type));
      }
   }

   if (var->num_members > 0) {
      const struct glsl_type *without_array = glsl_without_array(var->type);
      validate_assert(state, glsl_type_is_struct_or_ifc(without_array));
      validate_assert(state, var->num_members == glsl_get_length(without_array));
      validate_assert(state, var->members != NULL);
   }

   if (var->data.per_view)
      validate_assert(state, glsl_type_is_array(var->type));

   if (var->constant_initializer)
      validate_constant(var->constant_initializer, var->type, state);

   if (var->data.mode == nir_var_image) {
      validate_assert(state, !var->data.bindless);
      validate_assert(state, glsl_type_is_image(glsl_without_array(var->type)));
   }

   if (var->data.per_vertex)
      validate_assert(state, state->shader->info.stage == MESA_SHADER_FRAGMENT);

   /*
    * TODO validate some things ir_validate.cpp does (requires more GLSL type
    * support)
    */

   _mesa_hash_table_insert(state->var_defs, var,
                           valid_modes == nir_var_function_temp ? state->impl : NULL);

   state->var = NULL;
}

static bool
validate_ssa_def_dominance(nir_def *def, void *_state)
{
   validate_state *state = _state;

   validate_assert(state, def->index < state->impl->ssa_alloc);
   validate_assert(state, !BITSET_TEST(state->ssa_defs_found, def->index));
   BITSET_SET(state->ssa_defs_found, def->index);

   return true;
}

static bool
validate_src_dominance(nir_src *src, void *_state)
{
   validate_state *state = _state;

   if (src->ssa->parent_instr->block == nir_src_parent_instr(src)->block) {
      validate_assert(state, src->ssa->index < state->impl->ssa_alloc);
      validate_assert(state, BITSET_TEST(state->ssa_defs_found,
                                         src->ssa->index));
   } else {
      validate_assert(state, nir_block_dominates(src->ssa->parent_instr->block,
                                                 nir_src_parent_instr(src)->block));
   }
   return true;
}

static void
validate_ssa_dominance(nir_function_impl *impl, validate_state *state)
{
   nir_metadata_require(impl, nir_metadata_dominance);

   nir_foreach_block(block, impl) {
      state->block = block;
      nir_foreach_instr(instr, block) {
         state->instr = instr;
         if (instr->type == nir_instr_type_phi) {
            nir_phi_instr *phi = nir_instr_as_phi(instr);
            nir_foreach_phi_src(src, phi) {
               validate_assert(state,
                               nir_block_dominates(src->src.ssa->parent_instr->block,
                                                   src->pred));
            }
         } else {
            nir_foreach_src(instr, validate_src_dominance, state);
         }
         nir_foreach_def(instr, validate_ssa_def_dominance, state);
      }
   }
}

static void
validate_function_impl(nir_function_impl *impl, validate_state *state)
{
   validate_assert(state, impl->function->impl == impl);
   validate_assert(state, impl->cf_node.parent == NULL);

   if (impl->preamble) {
      validate_assert(state, impl->function->is_entrypoint);
      validate_assert(state, impl->preamble->is_preamble);
   }

   validate_assert(state, exec_list_is_empty(&impl->end_block->instr_list));
   validate_assert(state, impl->end_block->successors[0] == NULL);
   validate_assert(state, impl->end_block->successors[1] == NULL);

   state->impl = impl;
   state->parent_node = &impl->cf_node;

   exec_list_validate(&impl->locals);
   nir_foreach_function_temp_variable(var, impl) {
      validate_var_decl(var, nir_var_function_temp, state);
   }

   state->ssa_defs_found = reralloc(state->mem_ctx, state->ssa_defs_found,
                                    BITSET_WORD, BITSET_WORDS(impl->ssa_alloc));
   memset(state->ssa_defs_found, 0, BITSET_WORDS(impl->ssa_alloc) * sizeof(BITSET_WORD));

   _mesa_set_clear(state->blocks, NULL);
   _mesa_set_resize(state->blocks, impl->num_blocks);
   collect_blocks(&impl->body, state);
   _mesa_set_add(state->blocks, impl->end_block);
   validate_assert(state, !exec_list_is_empty(&impl->body));
   foreach_list_typed(nir_cf_node, node, node, &impl->body) {
      validate_cf_node(node, state);
   }
   validate_end_block(impl->end_block, state);

   /* We must have seen every source by now. This also means that we've untagged
    * every source, so we have valid (unaugmented) NIR once again.
    */
   validate_assert(state, state->nr_tagged_srcs == 0);

   static int validate_dominance = -1;
   if (validate_dominance < 0) {
      validate_dominance =
         NIR_DEBUG(VALIDATE_SSA_DOMINANCE);
   }
   if (validate_dominance) {
      memset(state->ssa_defs_found, 0, BITSET_WORDS(impl->ssa_alloc) * sizeof(BITSET_WORD));
      validate_ssa_dominance(impl, state);
   }
}

static void
validate_function(nir_function *func, validate_state *state)
{
   if (func->impl != NULL) {
      validate_assert(state, func->impl->function == func);
      validate_function_impl(func->impl, state);
   }
}

static void
init_validate_state(validate_state *state)
{
   state->mem_ctx = ralloc_context(NULL);
   state->ssa_defs_found = NULL;
   state->blocks = _mesa_pointer_set_create(state->mem_ctx);
   state->var_defs = _mesa_pointer_hash_table_create(state->mem_ctx);
   state->errors = _mesa_pointer_hash_table_create(state->mem_ctx);
   state->nr_tagged_srcs = 0;

   state->loop = NULL;
   state->in_loop_continue_construct = false;
   state->instr = NULL;
   state->var = NULL;
}

static void
destroy_validate_state(validate_state *state)
{
   ralloc_free(state->mem_ctx);
}

simple_mtx_t fail_dump_mutex = SIMPLE_MTX_INITIALIZER;

static void
dump_errors(validate_state *state, const char *when)
{
   struct hash_table *errors = state->errors;

   /* Lock around dumping so that we get clean dumps in a multi-threaded
    * scenario
    */
   simple_mtx_lock(&fail_dump_mutex);

   if (when) {
      fprintf(stderr, "NIR validation failed %s\n", when);
      fprintf(stderr, "%d errors:\n", _mesa_hash_table_num_entries(errors));
   } else {
      fprintf(stderr, "NIR validation failed with %d errors:\n",
              _mesa_hash_table_num_entries(errors));
   }

   nir_print_shader_annotated(state->shader, stderr, errors);

   if (_mesa_hash_table_num_entries(errors) > 0) {
      fprintf(stderr, "%d additional errors:\n",
              _mesa_hash_table_num_entries(errors));
      hash_table_foreach(errors, entry) {
         fprintf(stderr, "%s\n", (char *)entry->data);
      }
   }

   simple_mtx_unlock(&fail_dump_mutex);

   abort();
}

void
nir_validate_shader(nir_shader *shader, const char *when)
{
   if (NIR_DEBUG(NOVALIDATE))
      return;

   validate_state state;
   init_validate_state(&state);

   state.shader = shader;

   nir_variable_mode valid_modes =
      nir_var_shader_in |
      nir_var_shader_out |
      nir_var_shader_temp |
      nir_var_uniform |
      nir_var_mem_ubo |
      nir_var_system_value |
      nir_var_mem_ssbo |
      nir_var_mem_shared |
      nir_var_mem_global |
      nir_var_mem_push_const |
      nir_var_mem_constant |
      nir_var_image;

   if (gl_shader_stage_is_callable(shader->info.stage))
      valid_modes |= nir_var_shader_call_data;

   if (shader->info.stage == MESA_SHADER_ANY_HIT ||
       shader->info.stage == MESA_SHADER_CLOSEST_HIT ||
       shader->info.stage == MESA_SHADER_INTERSECTION)
      valid_modes |= nir_var_ray_hit_attrib;

   if (shader->info.stage == MESA_SHADER_TASK ||
       shader->info.stage == MESA_SHADER_MESH)
      valid_modes |= nir_var_mem_task_payload;

   if (shader->info.stage == MESA_SHADER_COMPUTE)
      valid_modes |= nir_var_mem_node_payload |
                     nir_var_mem_node_payload_in;

   exec_list_validate(&shader->variables);
   nir_foreach_variable_in_shader(var, shader)
      validate_var_decl(var, valid_modes, &state);

   exec_list_validate(&shader->functions);
   foreach_list_typed(nir_function, func, node, &shader->functions) {
      validate_function(func, &state);
   }

   if (shader->xfb_info != NULL) {
      /* At least validate that, if nir_shader::xfb_info exists, the shader
       * has real transform feedback going on.
       */
      validate_assert(&state, shader->info.stage == MESA_SHADER_VERTEX ||
                                 shader->info.stage == MESA_SHADER_TESS_EVAL ||
                                 shader->info.stage == MESA_SHADER_GEOMETRY);
      validate_assert(&state, shader->xfb_info->buffers_written != 0);
      validate_assert(&state, shader->xfb_info->streams_written != 0);
      validate_assert(&state, shader->xfb_info->output_count > 0);
   }

   if (_mesa_hash_table_num_entries(state.errors) > 0)
      dump_errors(&state, when);

   destroy_validate_state(&state);
}

void
nir_validate_ssa_dominance(nir_shader *shader, const char *when)
{
   if (NIR_DEBUG(NOVALIDATE))
      return;

   validate_state state;
   init_validate_state(&state);

   state.shader = shader;

   nir_foreach_function_impl(impl, shader) {
      state.ssa_defs_found = reralloc(state.mem_ctx, state.ssa_defs_found,
                                      BITSET_WORD,
                                      BITSET_WORDS(impl->ssa_alloc));
      memset(state.ssa_defs_found, 0, BITSET_WORDS(impl->ssa_alloc) * sizeof(BITSET_WORD));

      state.impl = impl;
      validate_ssa_dominance(impl, &state);
   }

   if (_mesa_hash_table_num_entries(state.errors) > 0)
      dump_errors(&state, when);

   destroy_validate_state(&state);
}

#endif /* NDEBUG */
