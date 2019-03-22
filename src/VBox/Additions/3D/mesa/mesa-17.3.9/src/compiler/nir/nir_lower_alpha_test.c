/*
 * Copyright © 2017 Broadcom
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

/**
 * @file
 *
 * Implements GL alpha testing by comparing the output color's alpha to the
 * alpha_ref intrinsic and emitting a discard based on it.
 *
 * The alpha_to_one value overrides the source alpha to 1.0 to implement
 * GL_SAMPLE_ALPHA_TO_ONE, which applies before the alpha test (and would be
 * rather silly to use with alpha test, but the spec permits).
 */

#include "nir/nir.h"
#include "nir/nir_builder.h"

void
nir_lower_alpha_test(nir_shader *shader, enum compare_func func,
                     bool alpha_to_one)
{
   assert(shader->info.stage == MESA_SHADER_FRAGMENT);

   nir_foreach_function(function, shader) {
      nir_function_impl *impl = function->impl;
      nir_builder b;
      nir_builder_init(&b, impl);
      b.cursor = nir_before_cf_list(&impl->body);

      nir_foreach_block(block, impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type == nir_instr_type_intrinsic) {
               nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

               nir_variable *out = NULL;

               switch (intr->intrinsic) {
               case nir_intrinsic_store_var:
                  out = intr->variables[0]->var;
                  break;
               case nir_intrinsic_store_output:
                  /* already had i/o lowered.. lookup the matching output var: */
                  nir_foreach_variable(var, &shader->outputs) {
                     int drvloc = var->data.driver_location;
                     if (nir_intrinsic_base(intr) == drvloc) {
                        out = var;
                        break;
                     }
                  }
                  assume(out);
                  break;
               default:
                  continue;
               }

               if (out->data.mode != nir_var_shader_out)
                  continue;

               if (out->data.location != FRAG_RESULT_COLOR &&
                   out->data.location != FRAG_RESULT_DATA0)
                  continue;

               b.cursor = nir_before_instr(&intr->instr);

               nir_ssa_def *alpha;
               if (alpha_to_one) {
                  alpha = nir_imm_float(&b, 1.0);
               } else {
                  alpha = nir_channel(&b, nir_ssa_for_src(&b, intr->src[0], 4),
                                      3);
               }

               nir_ssa_def *condition =
                  nir_compare_func(&b, func,
                                   alpha, nir_load_alpha_ref_float(&b));

               nir_intrinsic_instr *discard =
                  nir_intrinsic_instr_create(b.shader,
                                             nir_intrinsic_discard_if);
               discard->num_components = 1;
               discard->src[0] = nir_src_for_ssa(nir_inot(&b, condition));
               nir_builder_instr_insert(&b, &discard->instr);
               shader->info.fs.uses_discard = true;
            }
         }
      }

      nir_metadata_preserve(impl, nir_metadata_block_index |
                            nir_metadata_dominance);
   }
}
