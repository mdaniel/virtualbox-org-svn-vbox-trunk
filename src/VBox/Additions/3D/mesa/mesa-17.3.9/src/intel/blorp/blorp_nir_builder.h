/*
 * Copyright © 2017 Intel Corporation
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

static inline nir_ssa_def *
blorp_nir_frag_coord(nir_builder *b)
{
   nir_variable *frag_coord =
      nir_variable_create(b->shader, nir_var_shader_in,
                          glsl_vec4_type(), "gl_FragCoord");

   frag_coord->data.location = VARYING_SLOT_POS;
   frag_coord->data.origin_upper_left = true;

   return nir_load_var(b, frag_coord);
}

static inline nir_ssa_def *
blorp_nir_txf_ms_mcs(nir_builder *b, nir_ssa_def *xy_pos, nir_ssa_def *layer)
{
   nir_tex_instr *tex = nir_tex_instr_create(b->shader, 1);
   tex->op = nir_texop_txf_ms_mcs;
   tex->sampler_dim = GLSL_SAMPLER_DIM_MS;
   tex->dest_type = nir_type_int;

   nir_ssa_def *coord;
   if (layer) {
      tex->is_array = true;
      tex->coord_components = 3;
      coord = nir_vec3(b, nir_channel(b, xy_pos, 0),
                          nir_channel(b, xy_pos, 1),
                          layer);
   } else {
      tex->is_array = false;
      tex->coord_components = 2;
      coord = nir_channels(b, xy_pos, 0x3);
   }
   tex->src[0].src_type = nir_tex_src_coord;
   tex->src[0].src = nir_src_for_ssa(coord);

   /* Blorp only has one texture and it's bound at unit 0 */
   tex->texture_index = 0;
   tex->sampler_index = 0;

   nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
   nir_builder_instr_insert(b, &tex->instr);

   return &tex->dest.ssa;
}

static inline nir_ssa_def *
blorp_nir_mcs_is_clear_color(nir_builder *b,
                             nir_ssa_def *mcs,
                             uint32_t samples)
{
   switch (samples) {
   case 2:
      /* Empirical evidence suggests that the value returned from the
       * sampler is not always 0x3 for clear color so we need to mask it.
       */
      return nir_ieq(b, nir_iand(b, nir_channel(b, mcs, 0),
                                    nir_imm_int(b, 0x3)),
                    nir_imm_int(b, 0x3));

   case 4:
      return nir_ieq(b, nir_channel(b, mcs, 0), nir_imm_int(b, 0xff));

   case 8:
      return nir_ieq(b, nir_channel(b, mcs, 0), nir_imm_int(b, ~0));

   case 16:
      /* For 16x MSAA, the MCS is actually an ivec2 */
      return nir_iand(b, nir_ieq(b, nir_channel(b, mcs, 0),
                                    nir_imm_int(b, ~0)),
                         nir_ieq(b, nir_channel(b, mcs, 1),
                                    nir_imm_int(b, ~0)));

   default:
      unreachable("Invalid sample count");
   }
}
