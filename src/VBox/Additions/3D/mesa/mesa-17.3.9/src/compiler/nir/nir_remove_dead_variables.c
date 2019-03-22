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

#include "nir.h"

static void
add_var_use_intrinsic(nir_intrinsic_instr *instr, struct set *live,
                      nir_variable_mode modes)
{
   unsigned num_vars = nir_intrinsic_infos[instr->intrinsic].num_variables;

   switch (instr->intrinsic) {
   case nir_intrinsic_copy_var:
      _mesa_set_add(live, instr->variables[1]->var);
      /* Fall through */
   case nir_intrinsic_store_var: {
      /* The first source in both copy_var and store_var is the destination.
       * If the variable is a local that never escapes the shader, then we
       * don't mark it as live for just a store.
       */
      nir_variable_mode mode = instr->variables[0]->var->data.mode;
      if (!(mode & (nir_var_local | nir_var_global | nir_var_shared)))
         _mesa_set_add(live, instr->variables[0]->var);
      break;
   }

   /* This pass can't be used on I/O variables after they've been lowered. */
   case nir_intrinsic_load_input:
      assert(!(modes & nir_var_shader_in));
      break;
   case nir_intrinsic_store_output:
      assert(!(modes & nir_var_shader_out));
      break;

   default:
      for (unsigned i = 0; i < num_vars; i++) {
         _mesa_set_add(live, instr->variables[i]->var);
      }
      break;
   }
}

static void
add_var_use_call(nir_call_instr *instr, struct set *live)
{
   if (instr->return_deref != NULL) {
      nir_variable *var = instr->return_deref->var;
      _mesa_set_add(live, var);
   }

   for (unsigned i = 0; i < instr->num_params; i++) {
      nir_variable *var = instr->params[i]->var;
      _mesa_set_add(live, var);
   }
}

static void
add_var_use_tex(nir_tex_instr *instr, struct set *live)
{
   if (instr->texture != NULL) {
      nir_variable *var = instr->texture->var;
      _mesa_set_add(live, var);
   }

   if (instr->sampler != NULL) {
      nir_variable *var = instr->sampler->var;
      _mesa_set_add(live, var);
   }
}

static void
add_var_use_shader(nir_shader *shader, struct set *live, nir_variable_mode modes)
{
   nir_foreach_function(function, shader) {
      if (function->impl) {
         nir_foreach_block(block, function->impl) {
            nir_foreach_instr(instr, block) {
               switch(instr->type) {
               case nir_instr_type_intrinsic:
                  add_var_use_intrinsic(nir_instr_as_intrinsic(instr), live,
                                        modes);
                  break;

               case nir_instr_type_call:
                  add_var_use_call(nir_instr_as_call(instr), live);
                  break;

               case nir_instr_type_tex:
                  add_var_use_tex(nir_instr_as_tex(instr), live);
                  break;

               default:
                  break;
               }
            }
         }
      }
   }
}

static void
remove_dead_var_writes(nir_shader *shader, struct set *live)
{
   nir_foreach_function(function, shader) {
      if (!function->impl)
         continue;

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;

            nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
            if (intrin->intrinsic != nir_intrinsic_copy_var &&
                intrin->intrinsic != nir_intrinsic_store_var)
               continue;

            /* Stores to dead variables need to be removed */
            if (intrin->variables[0]->var->data.mode == 0)
               nir_instr_remove(instr);
         }
      }
   }
}

static bool
remove_dead_vars(struct exec_list *var_list, struct set *live)
{
   bool progress = false;

   foreach_list_typed_safe(nir_variable, var, node, var_list) {
      struct set_entry *entry = _mesa_set_search(live, var);
      if (entry == NULL) {
         /* Mark this variable as used by setting the mode to 0 */
         var->data.mode = 0;
         exec_node_remove(&var->node);
         progress = true;
      }
   }

   return progress;
}

bool
nir_remove_dead_variables(nir_shader *shader, nir_variable_mode modes)
{
   bool progress = false;
   struct set *live =
      _mesa_set_create(NULL, _mesa_hash_pointer, _mesa_key_pointer_equal);

   add_var_use_shader(shader, live, modes);

   if (modes & nir_var_uniform)
      progress = remove_dead_vars(&shader->uniforms, live) || progress;

   if (modes & nir_var_shader_in)
      progress = remove_dead_vars(&shader->inputs, live) || progress;

   if (modes & nir_var_shader_out)
      progress = remove_dead_vars(&shader->outputs, live) || progress;

   if (modes & nir_var_global)
      progress = remove_dead_vars(&shader->globals, live) || progress;

   if (modes & nir_var_system_value)
      progress = remove_dead_vars(&shader->system_values, live) || progress;

   if (modes & nir_var_shared)
      progress = remove_dead_vars(&shader->shared, live) || progress;

   if (modes & nir_var_local) {
      nir_foreach_function(function, shader) {
         if (function->impl) {
            if (remove_dead_vars(&function->impl->locals, live))
               progress = true;
         }
      }
   }

   if (progress) {
      remove_dead_var_writes(shader, live);

      nir_foreach_function(function, shader) {
         if (function->impl) {
            nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                                  nir_metadata_dominance);
         }
      }
   }

   _mesa_set_destroy(live, NULL);
   return progress;
}
