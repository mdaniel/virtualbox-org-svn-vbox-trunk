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

#include "compiler/nir/nir_builder.h"
#include "brw_nir.h"

/**
 * Prior to Haswell, the hardware can't natively support GL_FIXED or
 * 2_10_10_10_REV vertex formats.  This pass inserts extra shader code
 * to produce the correct values.
 */

struct attr_wa_state {
   nir_builder builder;
   bool impl_progress;
   bool use_legacy_snorm_formula;
   const uint8_t *wa_flags;
};

static bool
apply_attr_wa_block(nir_block *block, struct attr_wa_state *state)
{
   nir_builder *b = &state->builder;

   nir_foreach_instr_safe(instr, block) {
      if (instr->type != nir_instr_type_intrinsic)
         continue;

      nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
      if (intrin->intrinsic != nir_intrinsic_load_input)
         continue;

      uint8_t wa_flags = state->wa_flags[intrin->const_index[0]];
      if (wa_flags == 0)
         continue;

      b->cursor = nir_after_instr(instr);

      nir_ssa_def *val = &intrin->dest.ssa;

      /* Do GL_FIXED rescaling for GLES2.0.  Our GL_FIXED attributes
       * come in as floating point conversions of the integer values.
       */
      if (wa_flags & BRW_ATTRIB_WA_COMPONENT_MASK) {
         nir_ssa_def *scaled =
            nir_fmul(b, val, nir_imm_float(b, 1.0f / 65536.0f));
         nir_ssa_def *comps[4];
         for (int i = 0; i < val->num_components; i++) {
            bool rescale = i < (wa_flags & BRW_ATTRIB_WA_COMPONENT_MASK);
            comps[i] = nir_channel(b, rescale ? scaled : val, i);
         }
         val = nir_vec(b, comps, val->num_components);
      }

      /* Do sign recovery for 2101010 formats if required. */
      if (wa_flags & BRW_ATTRIB_WA_SIGN) {
         /* sign recovery shift: <22, 22, 22, 30> */
         nir_ssa_def *shift = nir_imm_ivec4(b, 22, 22, 22, 30);
         val = nir_ishr(b, nir_ishl(b, val, shift), shift);
      }

      /* Apply BGRA swizzle if required. */
      if (wa_flags & BRW_ATTRIB_WA_BGRA) {
         val = nir_swizzle(b, val, (unsigned[4]){2,1,0,3}, 4, true);
      }

      if (wa_flags & BRW_ATTRIB_WA_NORMALIZE) {
         /* ES 3.0 has different rules for converting signed normalized
          * fixed-point numbers than desktop GL.
          */
         if ((wa_flags & BRW_ATTRIB_WA_SIGN) &&
             !state->use_legacy_snorm_formula) {
            /* According to equation 2.2 of the ES 3.0 specification,
             * signed normalization conversion is done by:
             *
             * f = c / (2^(b-1)-1)
             */
            nir_ssa_def *es3_normalize_factor =
               nir_imm_vec4(b, 1.0f / ((1 << 9) - 1), 1.0f / ((1 << 9) - 1),
                               1.0f / ((1 << 9) - 1), 1.0f / ((1 << 1) - 1));
            val = nir_fmax(b,
                           nir_fmul(b, nir_i2f32(b, val), es3_normalize_factor),
                           nir_imm_float(b, -1.0f));
         } else {
            /* The following equations are from the OpenGL 3.2 specification:
             *
             * 2.1 unsigned normalization
             * f = c/(2^n-1)
             *
             * 2.2 signed normalization
             * f = (2c+1)/(2^n-1)
             *
             * Both of these share a common divisor, which we handle by
             * multiplying by 1 / (2^b - 1) for b = <10, 10, 10, 2>.
             */
            nir_ssa_def *normalize_factor =
               nir_imm_vec4(b, 1.0f / ((1 << 10) - 1), 1.0f / ((1 << 10) - 1),
                               1.0f / ((1 << 10) - 1), 1.0f / ((1 << 2)  - 1));

            if (wa_flags & BRW_ATTRIB_WA_SIGN) {
               /* For signed normalization, the numerator is 2c+1. */
               nir_ssa_def *two = nir_imm_float(b, 2.0f);
               nir_ssa_def *one = nir_imm_float(b, 1.0f);
               val = nir_fadd(b, nir_fmul(b, nir_i2f32(b, val), two), one);
            } else {
               /* For unsigned normalization, the numerator is just c. */
               val = nir_u2f32(b, val);
            }
            val = nir_fmul(b, val, normalize_factor);
         }
      }

      if (wa_flags & BRW_ATTRIB_WA_SCALE) {
         val = (wa_flags & BRW_ATTRIB_WA_SIGN) ? nir_i2f32(b, val)
                                               : nir_u2f32(b, val);
      }

      nir_ssa_def_rewrite_uses_after(&intrin->dest.ssa, nir_src_for_ssa(val),
                                     val->parent_instr);
      state->impl_progress = true;
   }

   return true;
}

bool
brw_nir_apply_attribute_workarounds(nir_shader *shader,
                                    bool use_legacy_snorm_formula,
                                    const uint8_t *attrib_wa_flags)
{
   bool progress = false;
   struct attr_wa_state state = {
      .use_legacy_snorm_formula = use_legacy_snorm_formula,
      .wa_flags = attrib_wa_flags,
   };

   nir_foreach_function(func, shader) {
      if (!func->impl)
         continue;

      nir_builder_init(&state.builder, func->impl);
      state.impl_progress = false;

      nir_foreach_block(block, func->impl) {
         apply_attr_wa_block(block, &state);
      }

      if (state.impl_progress) {
         nir_metadata_preserve(func->impl, nir_metadata_block_index |
                                           nir_metadata_dominance);
         progress = true;
      }
   }

   return progress;
}
