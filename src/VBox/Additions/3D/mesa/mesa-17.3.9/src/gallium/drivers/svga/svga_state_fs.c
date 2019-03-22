/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#include "util/u_inlines.h"
#include "pipe/p_defines.h"
#include "util/u_format.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_bitmask.h"
#include "tgsi/tgsi_ureg.h"

#include "svga_context.h"
#include "svga_state.h"
#include "svga_cmd.h"
#include "svga_shader.h"
#include "svga_resource_texture.h"
#include "svga_tgsi.h"
#include "svga_format.h"

#include "svga_hw_reg.h"



/**
 * If we fail to compile a fragment shader (because it uses too many
 * registers, for example) we'll use a dummy/fallback shader that
 * simply emits a constant color (red for debug, black for release).
 * We hit this with the Unigine/Heaven demo when Shaders = High.
 * With black, the demo still looks good.
 */
static const struct tgsi_token *
get_dummy_fragment_shader(void)
{
#ifdef DEBUG
   static const float color[4] = { 1.0, 0.0, 0.0, 0.0 }; /* red */
#else
   static const float color[4] = { 0.0, 0.0, 0.0, 0.0 }; /* black */
#endif
   struct ureg_program *ureg;
   const struct tgsi_token *tokens;
   struct ureg_src src;
   struct ureg_dst dst;

   ureg = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!ureg)
      return NULL;

   dst = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
   src = ureg_DECL_immediate(ureg, color, 4);
   ureg_MOV(ureg, dst, src);
   ureg_END(ureg);

   tokens = ureg_get_tokens(ureg, NULL);

   ureg_destroy(ureg);

   return tokens;
}


static struct svga_shader_variant *
translate_fragment_program(struct svga_context *svga,
                           const struct svga_fragment_shader *fs,
                           const struct svga_compile_key *key)
{
   if (svga_have_vgpu10(svga)) {
      return svga_tgsi_vgpu10_translate(svga, &fs->base, key,
                                        PIPE_SHADER_FRAGMENT);
   }
   else {
      return svga_tgsi_vgpu9_translate(svga, &fs->base, key,
                                       PIPE_SHADER_FRAGMENT);
   }
}


/**
 * Replace the given shader's instruction with a simple constant-color
 * shader.  We use this when normal shader translation fails.
 */
static struct svga_shader_variant *
get_compiled_dummy_shader(struct svga_context *svga,
                          struct svga_fragment_shader *fs,
                          const struct svga_compile_key *key)
{
   const struct tgsi_token *dummy = get_dummy_fragment_shader();
   struct svga_shader_variant *variant;

   if (!dummy) {
      return NULL;
   }

   FREE((void *) fs->base.tokens);
   fs->base.tokens = dummy;

   variant = translate_fragment_program(svga, fs, key);
   return variant;
}


/**
 * Translate TGSI shader into an svga shader variant.
 */
static enum pipe_error
compile_fs(struct svga_context *svga,
           struct svga_fragment_shader *fs,
           const struct svga_compile_key *key,
           struct svga_shader_variant **out_variant)
{
   struct svga_shader_variant *variant;
   enum pipe_error ret = PIPE_ERROR;

   variant = translate_fragment_program(svga, fs, key);
   if (variant == NULL) {
      debug_printf("Failed to compile fragment shader,"
                   " using dummy shader instead.\n");
      variant = get_compiled_dummy_shader(svga, fs, key);
   }
   else if (svga_shader_too_large(svga, variant)) {
      /* too big, use dummy shader */
      debug_printf("Shader too large (%u bytes),"
                   " using dummy shader instead.\n",
                   (unsigned) (variant->nr_tokens
                               * sizeof(variant->tokens[0])));
      /* Free the too-large variant */
      svga_destroy_shader_variant(svga, SVGA3D_SHADERTYPE_PS, variant);
      /* Use simple pass-through shader instead */
      variant = get_compiled_dummy_shader(svga, fs, key);
   }

   if (!variant) {
      return PIPE_ERROR;
   }

   ret = svga_define_shader(svga, SVGA3D_SHADERTYPE_PS, variant);
   if (ret != PIPE_OK) {
      svga_destroy_shader_variant(svga, SVGA3D_SHADERTYPE_PS, variant);
      return ret;
   }

   *out_variant = variant;

   /* insert variant at head of linked list */
   variant->next = fs->base.variants;
   fs->base.variants = variant;

   return PIPE_OK;
}


/* SVGA_NEW_TEXTURE_BINDING
 * SVGA_NEW_RAST
 * SVGA_NEW_NEED_SWTNL
 * SVGA_NEW_SAMPLER
 */
static enum pipe_error
make_fs_key(const struct svga_context *svga,
            struct svga_fragment_shader *fs,
            struct svga_compile_key *key)
{
   const enum pipe_shader_type shader = PIPE_SHADER_FRAGMENT;
   unsigned i;

   memset(key, 0, sizeof *key);

   memcpy(key->generic_remap_table, fs->generic_remap_table,
          sizeof(fs->generic_remap_table));

   /* SVGA_NEW_GS, SVGA_NEW_VS
    */
   if (svga->curr.gs) {
      key->fs.gs_generic_outputs = svga->curr.gs->generic_outputs;
   } else {
      key->fs.vs_generic_outputs = svga->curr.vs->generic_outputs;
   }

   /* Only need fragment shader fixup for twoside lighting if doing
    * hwtnl.  Otherwise the draw module does the whole job for us.
    *
    * SVGA_NEW_SWTNL
    */
   if (!svga->state.sw.need_swtnl) {
      /* SVGA_NEW_RAST, SVGA_NEW_REDUCED_PRIMITIVE
       */
      key->fs.light_twoside = svga->curr.rast->templ.light_twoside;
      key->fs.front_ccw = svga->curr.rast->templ.front_ccw;
      key->fs.pstipple = (svga->curr.rast->templ.poly_stipple_enable &&
                          svga->curr.reduced_prim == PIPE_PRIM_TRIANGLES);
      key->fs.aa_point = (svga->curr.rast->templ.point_smooth &&
                          svga->curr.reduced_prim == PIPE_PRIM_POINTS &&
                          (svga->curr.rast->pointsize > 1.0 ||
                           svga->curr.vs->base.info.writes_psize));
      if (key->fs.aa_point) {
         assert(svga->curr.gs != NULL);
         assert(svga->curr.gs->aa_point_coord_index != -1);
         key->fs.aa_point_coord_index = svga->curr.gs->aa_point_coord_index;
      }
   }

   /* The blend workaround for simulating logicop xor behaviour
    * requires that the incoming fragment color be white.  This change
    * achieves that by creating a variant of the current fragment
    * shader that overrides all output colors with 1,1,1,1
    *   
    * This will work for most shaders, including those containing
    * TEXKIL and/or depth-write.  However, it will break on the
    * combination of xor-logicop plus alphatest.
    *
    * Ultimately, we could implement alphatest in the shader using
    * texkil prior to overriding the outgoing fragment color.
    *   
    * SVGA_NEW_BLEND
    */
   key->fs.white_fragments = svga->curr.blend->need_white_fragments;

   key->fs.alpha_to_one = svga->curr.blend->alpha_to_one;

#ifdef DEBUG
   /*
    * We expect a consistent set of samplers and sampler views.
    * Do some debug checks/warnings here.
    */
   {
      static boolean warned = FALSE;
      unsigned i, n = MAX2(svga->curr.num_sampler_views[shader],
                           svga->curr.num_samplers[shader]);
      /* Only warn once to prevent too much debug output */
      if (!warned) {
         if (svga->curr.num_sampler_views[shader] !=
             svga->curr.num_samplers[shader]) {
            debug_printf("svga: mismatched number of sampler views (%u) "
                         "vs. samplers (%u)\n",
                         svga->curr.num_sampler_views[shader],
                         svga->curr.num_samplers[shader]);
         }
         for (i = 0; i < n; i++) {
            if ((svga->curr.sampler_views[shader][i] == NULL) !=
                (svga->curr.sampler[shader][i] == NULL))
               debug_printf("sampler_view[%u] = %p but sampler[%u] = %p\n",
                            i, svga->curr.sampler_views[shader][i],
                            i, svga->curr.sampler[shader][i]);
         }
         warned = TRUE;
      }
   }
#endif

   /* XXX: want to limit this to the textures that the shader actually
    * refers to.
    *
    * SVGA_NEW_TEXTURE_BINDING | SVGA_NEW_SAMPLER
    */
   svga_init_shader_key_common(svga, shader, key);

   for (i = 0; i < svga->curr.num_samplers[shader]; ++i) {
      struct pipe_sampler_view *view = svga->curr.sampler_views[shader][i];
      const struct svga_sampler_state *sampler = svga->curr.sampler[shader][i];
      if (view) {
         struct pipe_resource *tex = view->texture;
         if (tex->target != PIPE_BUFFER) {
            struct svga_texture *stex = svga_texture(tex);
            SVGA3dSurfaceFormat format = stex->key.format;

            if (!svga_have_vgpu10(svga) &&
                (format == SVGA3D_Z_D16 ||
                 format == SVGA3D_Z_D24X8 ||
                 format == SVGA3D_Z_D24S8)) {
               /* If we're sampling from a SVGA3D_Z_D16, SVGA3D_Z_D24X8,
                * or SVGA3D_Z_D24S8 surface, we'll automatically get
                * shadow comparison.  But we only get LEQUAL mode.
                * Set TEX_COMPARE_NONE here so we don't emit the extra FS
                * code for shadow comparison.
                */
               key->tex[i].compare_mode = PIPE_TEX_COMPARE_NONE;
               key->tex[i].compare_func = PIPE_FUNC_NEVER;
               /* These depth formats _only_ support comparison mode and
                * not ordinary sampling so warn if the later is expected.
                */
               if (sampler->compare_mode != PIPE_TEX_COMPARE_R_TO_TEXTURE) {
                  debug_warn_once("Unsupported shadow compare mode");
               }
               /* The shader translation code can emit code to
                * handle ALWAYS and NEVER compare functions
                */
               else if (sampler->compare_func == PIPE_FUNC_ALWAYS ||
                        sampler->compare_func == PIPE_FUNC_NEVER) {
                  key->tex[i].compare_mode = sampler->compare_mode;
                  key->tex[i].compare_func = sampler->compare_func;
               }
               else if (sampler->compare_func != PIPE_FUNC_LEQUAL) {
                  debug_warn_once("Unsupported shadow compare function");
               }
            }
            else {
               /* For other texture formats, just use the compare func/mode
                * as-is.  Should be no-ops for color textures.  For depth
                * textures, we do not get automatic depth compare.  We have
                * to do it ourselves in the shader.  And we don't get PCF.
                */
               key->tex[i].compare_mode = sampler->compare_mode;
               key->tex[i].compare_func = sampler->compare_func;
            }
         }
      }
   }

   /* sprite coord gen state */
   for (i = 0; i < svga->curr.num_samplers[shader]; ++i) {
      key->tex[i].sprite_texgen =
         svga->curr.rast->templ.sprite_coord_enable & (1 << i);
   }

   key->sprite_origin_lower_left = (svga->curr.rast->templ.sprite_coord_mode
                                    == PIPE_SPRITE_COORD_LOWER_LEFT);

   key->fs.flatshade = svga->curr.rast->templ.flatshade;

   /* SVGA_NEW_DEPTH_STENCIL_ALPHA */
   if (svga_have_vgpu10(svga)) {
      /* Alpha testing is not supported in integer-valued render targets. */
      if (svga_has_any_integer_cbufs(svga)) {
         key->fs.alpha_func = SVGA3D_CMP_ALWAYS;
         key->fs.alpha_ref = 0;
      }
      else {
         key->fs.alpha_func = svga->curr.depth->alphafunc;
         key->fs.alpha_ref = svga->curr.depth->alpharef;
      }
   }

   /* SVGA_NEW_FRAME_BUFFER | SVGA_NEW_BLEND */
   if (fs->base.info.properties[TGSI_PROPERTY_FS_COLOR0_WRITES_ALL_CBUFS] ||
       svga->curr.blend->need_white_fragments) {
      /* Replicate color0 output (or white) to N colorbuffers */
      key->fs.write_color0_to_n_cbufs = svga->curr.framebuffer.nr_cbufs;
   }

   return PIPE_OK;
}


/**
 * svga_reemit_fs_bindings - Reemit the fragment shader bindings
 */
enum pipe_error
svga_reemit_fs_bindings(struct svga_context *svga)
{
   enum pipe_error ret;

   assert(svga->rebind.flags.fs);
   assert(svga_have_gb_objects(svga));

   if (!svga->state.hw_draw.fs)
      return PIPE_OK;

   if (!svga_need_to_rebind_resources(svga)) {
      ret =  svga->swc->resource_rebind(svga->swc, NULL,
                                        svga->state.hw_draw.fs->gb_shader,
                                        SVGA_RELOC_READ);
      goto out;
   }

   if (svga_have_vgpu10(svga))
      ret = SVGA3D_vgpu10_SetShader(svga->swc, SVGA3D_SHADERTYPE_PS,
                                    svga->state.hw_draw.fs->gb_shader,
                                    svga->state.hw_draw.fs->id);
   else
      ret = SVGA3D_SetGBShader(svga->swc, SVGA3D_SHADERTYPE_PS,
                               svga->state.hw_draw.fs->gb_shader);

 out:
   if (ret != PIPE_OK)
      return ret;

   svga->rebind.flags.fs = FALSE;
   return PIPE_OK;
}



static enum pipe_error
emit_hw_fs(struct svga_context *svga, unsigned dirty)
{
   struct svga_shader_variant *variant = NULL;
   enum pipe_error ret = PIPE_OK;
   struct svga_fragment_shader *fs = svga->curr.fs;
   struct svga_compile_key key;

   SVGA_STATS_TIME_PUSH(svga_sws(svga), SVGA_STATS_TIME_EMITFS);

   /* Disable rasterization if rasterizer_discard flag is set or
    * vs/gs does not output position.
    */
   svga->disable_rasterizer =
      svga->curr.rast->templ.rasterizer_discard ||
      (svga->curr.gs && !svga->curr.gs->base.info.writes_position) ||
      (!svga->curr.gs && !svga->curr.vs->base.info.writes_position);

   /* Set FS to NULL when rasterization is to be disabled */
   if (svga->disable_rasterizer) {
      /* Set FS to NULL if it has not been done */
      if (svga->state.hw_draw.fs) {
         ret = svga_set_shader(svga, SVGA3D_SHADERTYPE_PS, NULL);
         if (ret != PIPE_OK)
            goto done;
      }
      svga->rebind.flags.fs = FALSE;
      svga->state.hw_draw.fs = NULL;
      goto done;
   }

   /* SVGA_NEW_BLEND
    * SVGA_NEW_TEXTURE_BINDING
    * SVGA_NEW_RAST
    * SVGA_NEW_NEED_SWTNL
    * SVGA_NEW_SAMPLER
    * SVGA_NEW_FRAME_BUFFER
    * SVGA_NEW_DEPTH_STENCIL_ALPHA
    * SVGA_NEW_VS
    */
   ret = make_fs_key(svga, fs, &key);
   if (ret != PIPE_OK)
      goto done;

   variant = svga_search_shader_key(&fs->base, &key);
   if (!variant) {
      ret = compile_fs(svga, fs, &key, &variant);
      if (ret != PIPE_OK)
         goto done;
   }

   assert(variant);

   if (variant != svga->state.hw_draw.fs) {
      ret = svga_set_shader(svga, SVGA3D_SHADERTYPE_PS, variant);
      if (ret != PIPE_OK)
         goto done;

      svga->rebind.flags.fs = FALSE;

      svga->dirty |= SVGA_NEW_FS_VARIANT;
      svga->state.hw_draw.fs = variant;
   }

done:
   SVGA_STATS_TIME_POP(svga_sws(svga));
   return ret;
}

struct svga_tracked_state svga_hw_fs = 
{
   "fragment shader (hwtnl)",
   (SVGA_NEW_FS |
    SVGA_NEW_GS |
    SVGA_NEW_VS |
    SVGA_NEW_TEXTURE_BINDING |
    SVGA_NEW_NEED_SWTNL |
    SVGA_NEW_RAST |
    SVGA_NEW_STIPPLE |
    SVGA_NEW_REDUCED_PRIMITIVE |
    SVGA_NEW_SAMPLER |
    SVGA_NEW_FRAME_BUFFER |
    SVGA_NEW_DEPTH_STENCIL_ALPHA |
    SVGA_NEW_BLEND),
   emit_hw_fs
};



