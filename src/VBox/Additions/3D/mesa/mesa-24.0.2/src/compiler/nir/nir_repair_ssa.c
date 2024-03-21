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

#include "nir.h"
#include "nir_phi_builder.h"

struct repair_ssa_state {
   nir_function_impl *impl;

   BITSET_WORD *def_set;
   struct nir_phi_builder *phi_builder;

   bool progress;
};

/* Get ready to build a phi and return the builder */
static struct nir_phi_builder *
prep_build_phi(struct repair_ssa_state *state)
{
   const unsigned num_words = BITSET_WORDS(state->impl->num_blocks);

   /* We create the phi builder on-demand. */
   if (state->phi_builder == NULL) {
      state->phi_builder = nir_phi_builder_create(state->impl);
      state->def_set = ralloc_array(NULL, BITSET_WORD, num_words);
   }

   /* We're going to build a phi.  That's progress. */
   state->progress = true;

   /* Set the defs set to empty */
   memset(state->def_set, 0, num_words * sizeof(*state->def_set));

   return state->phi_builder;
}

static nir_block *
get_src_block(nir_src *src)
{
   if (nir_src_is_if(src)) {
      return nir_cf_node_as_block(nir_cf_node_prev(&nir_src_parent_if(src)->cf_node));
   } else if (nir_src_parent_instr(src)->type == nir_instr_type_phi) {
      return exec_node_data(nir_phi_src, src, src)->pred;
   } else {
      return nir_src_parent_instr(src)->block;
   }
}

static bool
repair_ssa_def(nir_def *def, void *void_state)
{
   struct repair_ssa_state *state = void_state;

   bool is_valid = true;
   nir_foreach_use_including_if(src, def) {
      nir_block *src_block = get_src_block(src);

      if (nir_block_is_unreachable(src_block) ||
          !nir_block_dominates(def->parent_instr->block, src_block)) {
         is_valid = false;
         break;
      }
   }

   if (is_valid)
      return true;

   struct nir_phi_builder *pb = prep_build_phi(state);

   BITSET_SET(state->def_set, def->parent_instr->block->index);

   struct nir_phi_builder_value *val =
      nir_phi_builder_add_value(pb, def->num_components, def->bit_size,
                                state->def_set);

   nir_phi_builder_value_set_block_def(val, def->parent_instr->block, def);

   nir_foreach_use_including_if_safe(src, def) {
      nir_block *block = get_src_block(src);

      if (block == def->parent_instr->block) {
         assert(nir_phi_builder_value_get_block_def(val, block) == def);
         continue;
      }

      nir_def *block_def =
         nir_phi_builder_value_get_block_def(val, block);
      if (block_def == def)
         continue;

      /* If def was a deref and the use we're looking at is a deref that
       * isn't a cast, we need to wrap it in a cast so we don't loose any
       * deref information.
       */
      if (!nir_src_is_if(src) &&
          def->parent_instr->type == nir_instr_type_deref &&
          nir_src_parent_instr(src)->type == nir_instr_type_deref &&
          nir_instr_as_deref(nir_src_parent_instr(src))->deref_type != nir_deref_type_cast) {
         nir_deref_instr *cast =
            nir_deref_instr_create(state->impl->function->shader,
                                   nir_deref_type_cast);

         nir_deref_instr *deref = nir_instr_as_deref(def->parent_instr);
         cast->modes = deref->modes;
         cast->type = deref->type;
         cast->parent = nir_src_for_ssa(block_def);
         cast->cast.ptr_stride = nir_deref_instr_array_stride(deref);

         nir_def_init(&cast->instr, &cast->def, def->num_components,
                      def->bit_size);
         nir_instr_insert(nir_before_instr(nir_src_parent_instr(src)),
                          &cast->instr);
         block_def = &cast->def;
      }

      if (nir_src_is_if(src))
         nir_src_rewrite(&nir_src_parent_if(src)->condition, block_def);
      else
         nir_src_rewrite(src, block_def);
   }

   return true;
}

bool
nir_repair_ssa_impl(nir_function_impl *impl)
{
   struct repair_ssa_state state;

   state.impl = impl;
   state.phi_builder = NULL;
   state.progress = false;

   nir_metadata_require(impl, nir_metadata_block_index |
                                 nir_metadata_dominance);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         nir_foreach_def(instr, repair_ssa_def, &state);
      }
   }

   if (state.progress)
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                     nir_metadata_dominance);

   if (state.phi_builder) {
      nir_phi_builder_finish(state.phi_builder);
      ralloc_free(state.def_set);
   }

   return state.progress;
}

/** This pass can be used to repair SSA form in a shader.
 *
 * Sometimes a transformation (such as return lowering) will have to make
 * changes to a shader which, while still correct, break some of NIR's SSA
 * invariants.  This pass will insert ssa_undefs and phi nodes as needed to
 * get the shader back into SSA that the validator will like.
 */
bool
nir_repair_ssa(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function_impl(impl, shader) {
      progress = nir_repair_ssa_impl(impl) || progress;
   }

   return progress;
}
