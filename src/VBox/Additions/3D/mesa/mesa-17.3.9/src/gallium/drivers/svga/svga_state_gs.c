/**********************************************************
 * Copyright 2014 VMware, Inc.  All rights reserved.
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
#include "util/u_memory.h"
#include "util/u_bitmask.h"
#include "translate/translate.h"
#include "tgsi/tgsi_ureg.h"

#include "svga_context.h"
#include "svga_cmd.h"
#include "svga_shader.h"
#include "svga_tgsi.h"
#include "svga_streamout.h"
#include "svga_format.h"

/**
 * If we fail to compile a geometry shader we'll use a dummy/fallback shader
 * that simply emits the incoming vertices.
 */
static const struct tgsi_token *
get_dummy_geometry_shader(void)
{
   //XXX
   return NULL;
}


static struct svga_shader_variant *
translate_geometry_program(struct svga_context *svga,
                           const struct svga_geometry_shader *gs,
                           const struct svga_compile_key *key)
{
   assert(svga_have_vgpu10(svga));
   return svga_tgsi_vgpu10_translate(svga, &gs->base, key,
                                     PIPE_SHADER_GEOMETRY);
}


/**
 * Translate TGSI shader into an svga shader variant.
 */
static enum pipe_error
compile_gs(struct svga_context *svga,
           struct svga_geometry_shader *gs,
           const struct svga_compile_key *key,
           struct svga_shader_variant **out_variant)
{
   struct svga_shader_variant *variant;
   enum pipe_error ret = PIPE_ERROR;

   variant = translate_geometry_program(svga, gs, key);
   if (!variant) {
      /* some problem during translation, try the dummy shader */
      const struct tgsi_token *dummy = get_dummy_geometry_shader();
      if (!dummy) {
         return PIPE_ERROR_OUT_OF_MEMORY;
      }
      debug_printf("Failed to compile geometry shader, using dummy shader instead.\n");
      FREE((void *) gs->base.tokens);
      gs->base.tokens = dummy;
      variant = translate_geometry_program(svga, gs, key);
      if (!variant) {
         return PIPE_ERROR;
      }
   }

   ret = svga_define_shader(svga, SVGA3D_SHADERTYPE_GS, variant);
   if (ret != PIPE_OK) {
      svga_destroy_shader_variant(svga, SVGA3D_SHADERTYPE_GS, variant);
      return ret;
   }

   *out_variant = variant;

   return PIPE_OK;
}


static void
make_gs_key(struct svga_context *svga, struct svga_compile_key *key)
{
   struct svga_geometry_shader *gs = svga->curr.gs;

   memset(key, 0, sizeof *key);

   /*
    * SVGA_NEW_TEXTURE_BINDING | SVGA_NEW_SAMPLER
    */
   svga_init_shader_key_common(svga, PIPE_SHADER_GEOMETRY, key);

   memcpy(key->generic_remap_table, gs->generic_remap_table,
          sizeof(gs->generic_remap_table));

   key->gs.vs_generic_outputs = svga->curr.vs->generic_outputs;

   key->gs.need_prescale = svga->state.hw_clear.prescale.enabled;

   key->gs.writes_psize = gs->base.info.writes_psize;
   key->gs.wide_point = gs->wide_point;
   key->sprite_coord_enable = svga->curr.rast->templ.sprite_coord_enable;
   key->sprite_origin_lower_left = (svga->curr.rast->templ.sprite_coord_mode
                                    == PIPE_SPRITE_COORD_LOWER_LEFT);

   /* SVGA_NEW_RAST */
   key->clip_plane_enable = svga->curr.rast->templ.clip_plane_enable;
}


/**
 * svga_reemit_gs_bindings - Reemit the geometry shader bindings
 */
enum pipe_error
svga_reemit_gs_bindings(struct svga_context *svga)
{
   enum pipe_error ret;
   struct svga_winsys_gb_shader *gbshader = NULL;
   SVGA3dShaderId shaderId = SVGA3D_INVALID_ID;

   assert(svga->rebind.flags.gs);
   assert(svga_have_gb_objects(svga));

   /* Geometry Shader is only supported in vgpu10 */
   assert(svga_have_vgpu10(svga));

   if (svga->state.hw_draw.gs) {
      gbshader = svga->state.hw_draw.gs->gb_shader;
      shaderId = svga->state.hw_draw.gs->id;
   }

   if (!svga_need_to_rebind_resources(svga)) {
      ret =  svga->swc->resource_rebind(svga->swc, NULL, gbshader,
                                        SVGA_RELOC_READ);
      goto out;
   }

   ret = SVGA3D_vgpu10_SetShader(svga->swc, SVGA3D_SHADERTYPE_GS,
                                 gbshader, shaderId);

 out:
   if (ret != PIPE_OK)
      return ret;

   svga->rebind.flags.gs = FALSE;
   return PIPE_OK;
}

static enum pipe_error
emit_hw_gs(struct svga_context *svga, unsigned dirty)
{
   struct svga_shader_variant *variant;
   struct svga_geometry_shader *gs = svga->curr.gs;
   enum pipe_error ret = PIPE_OK;
   struct svga_compile_key key;

   SVGA_STATS_TIME_PUSH(svga_sws(svga), SVGA_STATS_TIME_EMITGS);

   /* If there's a user-defined GS, we should have a pointer to a derived
    * GS.  This should have been resolved in update_tgsi_transform().
    */
   if (svga->curr.user_gs)
      assert(svga->curr.gs);

   if (!gs) {
      if (svga->state.hw_draw.gs != NULL) {

         /** The previous geometry shader is made inactive.
          *  Needs to unbind the geometry shader.
          */
         ret = svga_set_shader(svga, SVGA3D_SHADERTYPE_GS, NULL);
         if (ret != PIPE_OK)
            goto done;
         svga->state.hw_draw.gs = NULL;
      }
      goto done;
   }

   /* If there is stream output info for this geometry shader, then use
    * it instead of the one from the vertex shader.
    */
   if (svga_have_gs_streamout(svga)) {
      ret = svga_set_stream_output(svga, gs->base.stream_output);
      if (ret != PIPE_OK) {
         goto done;
      }
   }
   else if (!svga_have_vs_streamout(svga)) {
      /* turn off stream out */
      ret = svga_set_stream_output(svga, NULL);
      if (ret != PIPE_OK) {
         goto done;
      }
   }

   /* SVGA_NEW_NEED_SWTNL */
   if (svga->state.sw.need_swtnl && !svga_have_vgpu10(svga)) {
      /* No geometry shader is needed */
      variant = NULL;
   }
   else {
      make_gs_key(svga, &key);

      /* See if we already have a GS variant that matches the key */
      variant = svga_search_shader_key(&gs->base, &key);

      if (!variant) {
         ret = compile_gs(svga, gs, &key, &variant);
         if (ret != PIPE_OK)
            goto done;

         /* insert the new variant at head of linked list */
         assert(variant);
         variant->next = gs->base.variants;
         gs->base.variants = variant;
      }
   }

   if (variant != svga->state.hw_draw.gs) {
      /* Bind the new variant */
      ret = svga_set_shader(svga, SVGA3D_SHADERTYPE_GS, variant);
      if (ret != PIPE_OK)
         goto done;

      svga->rebind.flags.gs = FALSE;
      svga->dirty |= SVGA_NEW_GS_VARIANT;
      svga->state.hw_draw.gs = variant;
   }

done:
   SVGA_STATS_TIME_POP(svga_sws(svga));
   return ret;
}

struct svga_tracked_state svga_hw_gs =
{
   "geometry shader (hwtnl)",
   (SVGA_NEW_VS |
    SVGA_NEW_FS |
    SVGA_NEW_GS |
    SVGA_NEW_TEXTURE_BINDING |
    SVGA_NEW_SAMPLER |
    SVGA_NEW_RAST |
    SVGA_NEW_NEED_SWTNL),
   emit_hw_gs
};
