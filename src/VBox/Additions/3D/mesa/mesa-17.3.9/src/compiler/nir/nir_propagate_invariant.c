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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "nir.h"

static void
add_src(nir_src *src, struct set *invariants)
{
   if (src->is_ssa) {
      _mesa_set_add(invariants, src->ssa);
   } else {
      _mesa_set_add(invariants, src->reg.reg);
   }
}

static bool
add_src_cb(nir_src *src, void *state)
{
   add_src(src, state);
   return true;
}

static bool
dest_is_invariant(nir_dest *dest, struct set *invariants)
{
   if (dest->is_ssa) {
      return _mesa_set_search(invariants, &dest->ssa);
   } else {
      return _mesa_set_search(invariants, dest->reg.reg);
   }
}

static void
add_cf_node(nir_cf_node *cf, struct set *invariants)
{
   if (cf->type == nir_cf_node_if) {
      nir_if *if_stmt = nir_cf_node_as_if(cf);
      add_src(&if_stmt->condition, invariants);
   }

   if (cf->parent)
      add_cf_node(cf->parent, invariants);
}

static void
add_var(nir_variable *var, struct set *invariants)
{
   _mesa_set_add(invariants, var);
}

static bool
var_is_invariant(nir_variable *var, struct set * invariants)
{
   return var->data.invariant || _mesa_set_search(invariants, var);
}

static void
propagate_invariant_instr(nir_instr *instr, struct set *invariants)
{
   switch (instr->type) {
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      if (!dest_is_invariant(&alu->dest.dest, invariants))
         break;

      alu->exact = true;
      nir_foreach_src(instr, add_src_cb, invariants);
      break;
   }

   case nir_instr_type_tex: {
      nir_tex_instr *tex = nir_instr_as_tex(instr);
      if (dest_is_invariant(&tex->dest, invariants))
         nir_foreach_src(instr, add_src_cb, invariants);
      break;
   }

   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
      switch (intrin->intrinsic) {
      case nir_intrinsic_copy_var:
         /* If the destination is invariant then so is the source */
         if (var_is_invariant(intrin->variables[0]->var, invariants))
            add_var(intrin->variables[1]->var, invariants);
         break;

      case nir_intrinsic_load_var:
         if (dest_is_invariant(&intrin->dest, invariants))
            add_var(intrin->variables[0]->var, invariants);
         break;

      case nir_intrinsic_store_var:
         if (var_is_invariant(intrin->variables[0]->var, invariants))
            add_src(&intrin->src[0], invariants);
         break;

      default:
         /* Nothing to do */
         break;
      }
   }

   case nir_instr_type_jump:
   case nir_instr_type_ssa_undef:
   case nir_instr_type_load_const:
      break; /* Nothing to do */

   case nir_instr_type_phi: {
      nir_phi_instr *phi = nir_instr_as_phi(instr);
      if (!dest_is_invariant(&phi->dest, invariants))
         break;

      nir_foreach_phi_src(src, phi) {
         add_src(&src->src, invariants);
         add_cf_node(&src->pred->cf_node, invariants);
      }
      break;
   }

   case nir_instr_type_call:
      unreachable("This pass must be run after function inlining");

   case nir_instr_type_parallel_copy:
   default:
      unreachable("Cannot have this instruction type");
   }
}

static bool
propagate_invariant_impl(nir_function_impl *impl, struct set *invariants)
{
   bool progress = false;

   while (true) {
      uint32_t prev_entries = invariants->entries;

      nir_foreach_block_reverse(block, impl) {
         nir_foreach_instr_reverse(instr, block)
            propagate_invariant_instr(instr, invariants);
      }

      /* Keep running until we make no more progress. */
      if (invariants->entries > prev_entries) {
         progress = true;
         continue;
      } else {
         break;
      }
   }

   if (progress) {
      nir_metadata_preserve(impl, nir_metadata_block_index |
                                  nir_metadata_dominance |
                                  nir_metadata_live_ssa_defs);
   }

   return progress;
}

bool
nir_propagate_invariant(nir_shader *shader)
{
   /* Hash set of invariant things */
   struct set *invariants = _mesa_set_create(NULL, _mesa_hash_pointer,
                                             _mesa_key_pointer_equal);

   bool progress = false;
   nir_foreach_function(function, shader) {
      if (function->impl && propagate_invariant_impl(function->impl, invariants))
         progress = true;
   }

   _mesa_set_destroy(invariants, NULL);

   return progress;
}
