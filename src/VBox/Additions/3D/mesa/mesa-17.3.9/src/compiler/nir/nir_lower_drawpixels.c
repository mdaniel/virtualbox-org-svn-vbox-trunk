/*
 * Copyright © 2015 Red Hat
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nir.h"
#include "nir_builder.h"

/* Lower glDrawPixels().
 *
 * This is based on the logic in st_get_drawpix_shader() in TGSI compiler.
 *
 * Run before nir_lower_io.
 */

typedef struct {
   const nir_lower_drawpixels_options *options;
   nir_shader   *shader;
   nir_builder   b;
   nir_variable *texcoord, *scale, *bias;
} lower_drawpixels_state;

static nir_ssa_def *
get_texcoord(lower_drawpixels_state *state)
{
   if (state->texcoord == NULL) {
      nir_variable *texcoord = NULL;

      /* find gl_TexCoord, if it exists: */
      nir_foreach_variable(var, &state->shader->inputs) {
         if (var->data.location == VARYING_SLOT_TEX0) {
            texcoord = var;
            break;
         }
      }

      /* otherwise create it: */
      if (texcoord == NULL) {
         texcoord = nir_variable_create(state->shader,
                                        nir_var_shader_in,
                                        glsl_vec4_type(),
                                        "gl_TexCoord");
         texcoord->data.location = VARYING_SLOT_TEX0;
      }

      state->texcoord = texcoord;
   }
   return nir_load_var(&state->b, state->texcoord);
}

static nir_variable *
create_uniform(nir_shader *shader, const char *name, const int state_tokens[5])
{
   nir_variable *var = nir_variable_create(shader,
                                           nir_var_uniform,
                                           glsl_vec4_type(),
                                           name);
   var->num_state_slots = 1;
   var->state_slots = ralloc_array(var, nir_state_slot, 1);
   memcpy(var->state_slots[0].tokens, state_tokens,
          sizeof(var->state_slots[0].tokens));
   return var;
}

static nir_ssa_def *
get_scale(lower_drawpixels_state *state)
{
   if (state->scale == NULL) {
      state->scale = create_uniform(state->shader, "gl_PTscale",
                                    state->options->scale_state_tokens);
   }
   return nir_load_var(&state->b, state->scale);
}

static nir_ssa_def *
get_bias(lower_drawpixels_state *state)
{
   if (state->bias == NULL) {
      state->bias = create_uniform(state->shader, "gl_PTbias",
                                   state->options->bias_state_tokens);
   }
   return nir_load_var(&state->b, state->bias);
}

static nir_ssa_def *
get_texcoord_const(lower_drawpixels_state *state)
{
   if (state->bias == NULL) {
      state->bias = create_uniform(state->shader, "gl_MultiTexCoord0",
                                   state->options->texcoord_state_tokens);
   }
   return nir_load_var(&state->b, state->bias);
}

static void
lower_color(lower_drawpixels_state *state, nir_intrinsic_instr *intr)
{
   nir_builder *b = &state->b;
   nir_ssa_def *texcoord;
   nir_tex_instr *tex;
   nir_ssa_def *def;

   assert(intr->dest.is_ssa);

   b->cursor = nir_before_instr(&intr->instr);

   texcoord = get_texcoord(state);

   /* replace load_var(gl_Color) w/ texture sample:
    *   TEX def, texcoord, drawpix_sampler, 2D
    */
   tex = nir_tex_instr_create(state->shader, 1);
   tex->op = nir_texop_tex;
   tex->sampler_dim = GLSL_SAMPLER_DIM_2D;
   tex->coord_components = 2;
   tex->sampler_index = state->options->drawpix_sampler;
   tex->texture_index = state->options->drawpix_sampler;
   tex->dest_type = nir_type_float;
   tex->src[0].src_type = nir_tex_src_coord;
   tex->src[0].src =
      nir_src_for_ssa(nir_channels(b, texcoord,
                                   (1 << tex->coord_components) - 1));

   nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
   nir_builder_instr_insert(b, &tex->instr);
   def = &tex->dest.ssa;

   /* Apply the scale and bias. */
   if (state->options->scale_and_bias) {
      /* MAD def, def, scale, bias; */
      def = nir_ffma(b, def, get_scale(state), get_bias(state));
   }

   if (state->options->pixel_maps) {
      static const unsigned swiz_xy[4] = {0,1};
      static const unsigned swiz_zw[4] = {2,3};

      /* do four pixel map look-ups with two TEX instructions: */
      nir_ssa_def *def_xy, *def_zw;

      /* TEX def.xy, def.xyyy, pixelmap_sampler, 2D; */
      tex = nir_tex_instr_create(state->shader, 1);
      tex->op = nir_texop_tex;
      tex->sampler_dim = GLSL_SAMPLER_DIM_2D;
      tex->coord_components = 2;
      tex->sampler_index = state->options->pixelmap_sampler;
      tex->texture_index = state->options->pixelmap_sampler;
      tex->dest_type = nir_type_float;
      tex->src[0].src_type = nir_tex_src_coord;
      tex->src[0].src = nir_src_for_ssa(nir_swizzle(b, def, swiz_xy, 2, true));

      nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
      nir_builder_instr_insert(b, &tex->instr);
      def_xy = &tex->dest.ssa;

      /* TEX def.zw, def.zwww, pixelmap_sampler, 2D; */
      tex = nir_tex_instr_create(state->shader, 1);
      tex->op = nir_texop_tex;
      tex->sampler_dim = GLSL_SAMPLER_DIM_2D;
      tex->coord_components = 2;
      tex->sampler_index = state->options->pixelmap_sampler;
      tex->dest_type = nir_type_float;
      tex->src[0].src_type = nir_tex_src_coord;
      tex->src[0].src = nir_src_for_ssa(nir_swizzle(b, def, swiz_zw, 2, true));

      nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, NULL);
      nir_builder_instr_insert(b, &tex->instr);
      def_zw = &tex->dest.ssa;

      /* def = vec4(def.xy, def.zw); */
      def = nir_vec4(b,
                     nir_channel(b, def_xy, 0),
                     nir_channel(b, def_xy, 1),
                     nir_channel(b, def_zw, 0),
                     nir_channel(b, def_zw, 1));
   }

   nir_ssa_def_rewrite_uses(&intr->dest.ssa, nir_src_for_ssa(def));
}

static void
lower_texcoord(lower_drawpixels_state *state, nir_intrinsic_instr *intr)
{
   state->b.cursor = nir_before_instr(&intr->instr);

   nir_ssa_def *texcoord_const = get_texcoord_const(state);
   nir_ssa_def_rewrite_uses(&intr->dest.ssa, nir_src_for_ssa(texcoord_const));
}

static bool
lower_drawpixels_block(lower_drawpixels_state *state, nir_block *block)
{
   nir_foreach_instr_safe(instr, block) {
      if (instr->type == nir_instr_type_intrinsic) {
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic == nir_intrinsic_load_var) {
            nir_deref_var *dvar = intr->variables[0];
            nir_variable *var = dvar->var;

            if (var->data.location == VARYING_SLOT_COL0) {
               /* gl_Color should not have array/struct derefs: */
               assert(dvar->deref.child == NULL);
               lower_color(state, intr);
            } else if (var->data.location == VARYING_SLOT_TEX0) {
               /* gl_TexCoord should not have array/struct derefs: */
               assert(dvar->deref.child == NULL);
               lower_texcoord(state, intr);
            }
         }
      }
   }

   return true;
}

static void
lower_drawpixels_impl(lower_drawpixels_state *state, nir_function_impl *impl)
{
   nir_builder_init(&state->b, impl);

   nir_foreach_block(block, impl) {
      lower_drawpixels_block(state, block);
   }
   nir_metadata_preserve(impl, nir_metadata_block_index |
                               nir_metadata_dominance);
}

void
nir_lower_drawpixels(nir_shader *shader,
                     const nir_lower_drawpixels_options *options)
{
   lower_drawpixels_state state = {
      .options = options,
      .shader = shader,
   };

   assert(shader->info.stage == MESA_SHADER_FRAGMENT);

   nir_foreach_function(function, shader) {
      if (function->impl)
         lower_drawpixels_impl(&state, function->impl);
   }
}
