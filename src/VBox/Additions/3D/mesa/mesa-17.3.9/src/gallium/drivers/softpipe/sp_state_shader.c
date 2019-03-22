/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "sp_context.h"
#include "sp_state.h"
#include "sp_fs.h"
#include "sp_texture.h"

#include "pipe/p_defines.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_pstipple.h"
#include "draw/draw_context.h"
#include "draw/draw_vs.h"
#include "draw/draw_gs.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_parse.h"


/**
 * Create a new fragment shader variant.
 */
static struct sp_fragment_shader_variant *
create_fs_variant(struct softpipe_context *softpipe,
                  struct sp_fragment_shader *fs,
                  const struct sp_fragment_shader_variant_key *key)
{
   struct sp_fragment_shader_variant *var;
   struct pipe_shader_state *curfs = &fs->shader;

   /* codegen, create variant object */
   var = softpipe_create_fs_variant_exec(softpipe);

   if (var) {
      var->key = *key;

#if DO_PSTIPPLE_IN_HELPER_MODULE
      if (key->polygon_stipple) {
         /* get new shader that implements polygon stippling */
         var->tokens = 
            util_pstipple_create_fragment_shader(curfs->tokens,
                                                 &var->stipple_sampler_unit, 0,
                                                 TGSI_FILE_INPUT);
      }
      else
#endif
      {
         var->tokens = tgsi_dup_tokens(curfs->tokens);
         var->stipple_sampler_unit = 0;
      }

      tgsi_scan_shader(var->tokens, &var->info);

      /* See comments elsewhere about draw fragment shaders */
#if 0
      /* draw's fs state */
      var->draw_shader = draw_create_fragment_shader(softpipe->draw,
                                                     &fs->shader);
      if (!var->draw_shader) {
         var->delete(var);
         FREE((void *) var->tokens);
         return NULL;
      }
#endif

      /* insert variant into linked list */
      var->next = fs->variants;
      fs->variants = var;
   }

   return var;
}


struct sp_fragment_shader_variant *
softpipe_find_fs_variant(struct softpipe_context *sp,
                         struct sp_fragment_shader *fs,
                         const struct sp_fragment_shader_variant_key *key)
{
   struct sp_fragment_shader_variant *var;

   for (var = fs->variants; var; var = var->next) {
      if (memcmp(&var->key, key, sizeof(*key)) == 0) {
         /* found it */
         return var;
      }
   }

   return create_fs_variant(sp, fs, key);
}


static void *
softpipe_create_fs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_fragment_shader *state = CALLOC_STRUCT(sp_fragment_shader);

   /* debug */
   if (softpipe->dump_fs) 
      tgsi_dump(templ->tokens, 0);

   /* we need to keep a local copy of the tokens */
   state->shader.tokens = tgsi_dup_tokens(templ->tokens);

   /* draw's fs state */
   state->draw_shader = draw_create_fragment_shader(softpipe->draw,
                                                    &state->shader);
   if (!state->draw_shader) {
      tgsi_free_tokens(state->shader.tokens);
      FREE(state);
      return NULL;
   }

   return state;
}


static void
softpipe_bind_fs_state(struct pipe_context *pipe, void *fs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_fragment_shader *state = (struct sp_fragment_shader *) fs;

   if (softpipe->fs == fs)
      return;

   draw_flush(softpipe->draw);

   softpipe->fs = fs;

   /* This depends on the current fragment shader and must always be
    * re-validated before use.
    */
   softpipe->fs_variant = NULL;

   if (state)
      draw_bind_fragment_shader(softpipe->draw,
                                state->draw_shader);
   else
      draw_bind_fragment_shader(softpipe->draw, NULL);

   softpipe->dirty |= SP_NEW_FS;
}


static void
softpipe_delete_fs_state(struct pipe_context *pipe, void *fs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_fragment_shader *state = fs;
   struct sp_fragment_shader_variant *var, *next_var;

   assert(fs != softpipe->fs);

   /* delete variants */
   for (var = state->variants; var; var = next_var) {
      next_var = var->next;

      assert(var != softpipe->fs_variant);

      /* See comments elsewhere about draw fragment shaders */
#if 0
      draw_delete_fragment_shader(softpipe->draw, var->draw_shader);
#endif

      var->delete(var, softpipe->fs_machine);
   }

   draw_delete_fragment_shader(softpipe->draw, state->draw_shader);

   tgsi_free_tokens(state->shader.tokens);
   FREE(state);
}


static void *
softpipe_create_vs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_vertex_shader *state;

   state = CALLOC_STRUCT(sp_vertex_shader);
   if (!state)
      goto fail;

   /* copy shader tokens, the ones passed in will go away.
    */
   state->shader.tokens = tgsi_dup_tokens(templ->tokens);
   if (state->shader.tokens == NULL)
      goto fail;

   state->draw_data = draw_create_vertex_shader(softpipe->draw, templ);
   if (state->draw_data == NULL) 
      goto fail;

   state->max_sampler = state->draw_data->info.file_max[TGSI_FILE_SAMPLER];

   return state;

fail:
   if (state) {
      tgsi_free_tokens(state->shader.tokens);
      FREE( state->draw_data );
      FREE( state );
   }
   return NULL;
}


static void
softpipe_bind_vs_state(struct pipe_context *pipe, void *vs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   softpipe->vs = (struct sp_vertex_shader *) vs;

   draw_bind_vertex_shader(softpipe->draw,
                           (softpipe->vs ? softpipe->vs->draw_data : NULL));

   softpipe->dirty |= SP_NEW_VS;
}


static void
softpipe_delete_vs_state(struct pipe_context *pipe, void *vs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   struct sp_vertex_shader *state = (struct sp_vertex_shader *) vs;

   draw_delete_vertex_shader(softpipe->draw, state->draw_data);
   tgsi_free_tokens(state->shader.tokens);
   FREE( state );
}


static void *
softpipe_create_gs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_geometry_shader *state;

   state = CALLOC_STRUCT(sp_geometry_shader);
   if (!state)
      goto fail;

   state->shader = *templ;

   if (templ->tokens) {
      /* debug */
      if (softpipe->dump_gs)
         tgsi_dump(templ->tokens, 0);

      /* copy shader tokens, the ones passed in will go away.
       */
      state->shader.tokens = tgsi_dup_tokens(templ->tokens);
      if (state->shader.tokens == NULL)
         goto fail;

      state->draw_data = draw_create_geometry_shader(softpipe->draw, templ);
      if (state->draw_data == NULL)
         goto fail;

      state->max_sampler = state->draw_data->info.file_max[TGSI_FILE_SAMPLER];
   }

   return state;

fail:
   if (state) {
      tgsi_free_tokens(state->shader.tokens);
      FREE( state->draw_data );
      FREE( state );
   }
   return NULL;
}


static void
softpipe_bind_gs_state(struct pipe_context *pipe, void *gs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   softpipe->gs = (struct sp_geometry_shader *)gs;

   draw_bind_geometry_shader(softpipe->draw,
                             (softpipe->gs ? softpipe->gs->draw_data : NULL));

   softpipe->dirty |= SP_NEW_GS;
}


static void
softpipe_delete_gs_state(struct pipe_context *pipe, void *gs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);

   struct sp_geometry_shader *state =
      (struct sp_geometry_shader *)gs;

   draw_delete_geometry_shader(softpipe->draw,
                               (state) ? state->draw_data : 0);

   tgsi_free_tokens(state->shader.tokens);
   FREE(state);
}


static void
softpipe_set_constant_buffer(struct pipe_context *pipe,
                             enum pipe_shader_type shader, uint index,
                             const struct pipe_constant_buffer *cb)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct pipe_resource *constants = cb ? cb->buffer : NULL;
   unsigned size;
   const void *data;

   assert(shader < PIPE_SHADER_TYPES);

   if (cb && cb->user_buffer) {
      constants = softpipe_user_buffer_create(pipe->screen,
                                              (void *) cb->user_buffer,
                                              cb->buffer_size,
                                              PIPE_BIND_CONSTANT_BUFFER);
   }

   size = cb ? cb->buffer_size : 0;
   data = constants ? softpipe_resource_data(constants) : NULL;
   if (data)
      data = (const char *) data + cb->buffer_offset;

   draw_flush(softpipe->draw);

   /* note: reference counting */
   pipe_resource_reference(&softpipe->constants[shader][index], constants);

   if (shader == PIPE_SHADER_VERTEX || shader == PIPE_SHADER_GEOMETRY) {
      draw_set_mapped_constant_buffer(softpipe->draw, shader, index, data, size);
   }

   softpipe->mapped_constants[shader][index] = data;
   softpipe->const_buffer_size[shader][index] = size;

   softpipe->dirty |= SP_NEW_CONSTANTS;

   if (cb && cb->user_buffer) {
      pipe_resource_reference(&constants, NULL);
   }
}

static void *
softpipe_create_compute_state(struct pipe_context *pipe,
                              const struct pipe_compute_state *templ)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   const struct tgsi_token *tokens;
   struct sp_compute_shader *state;
   if (templ->ir_type != PIPE_SHADER_IR_TGSI)
      return NULL;

   tokens = templ->prog;
   /* debug */
   if (softpipe->dump_cs)
      tgsi_dump(tokens, 0);

   state = CALLOC_STRUCT(sp_compute_shader);

   state->shader = *templ;
   state->tokens = tgsi_dup_tokens(tokens);
   tgsi_scan_shader(state->tokens, &state->info);

   state->max_sampler = state->info.file_max[TGSI_FILE_SAMPLER];

   return state;
}

static void
softpipe_bind_compute_state(struct pipe_context *pipe,
                            void *cs)
{
   struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_compute_shader *state = (struct sp_compute_shader *)cs;
   if (softpipe->cs == state)
      return;

   softpipe->cs = state;
}

static void
softpipe_delete_compute_state(struct pipe_context *pipe,
                              void *cs)
{
   MAYBE_UNUSED struct softpipe_context *softpipe = softpipe_context(pipe);
   struct sp_compute_shader *state = (struct sp_compute_shader *)cs;

   assert(softpipe->cs != state);
   tgsi_free_tokens(state->tokens);
   FREE(state);
}

void
softpipe_init_shader_funcs(struct pipe_context *pipe)
{
   pipe->create_fs_state = softpipe_create_fs_state;
   pipe->bind_fs_state   = softpipe_bind_fs_state;
   pipe->delete_fs_state = softpipe_delete_fs_state;

   pipe->create_vs_state = softpipe_create_vs_state;
   pipe->bind_vs_state   = softpipe_bind_vs_state;
   pipe->delete_vs_state = softpipe_delete_vs_state;

   pipe->create_gs_state = softpipe_create_gs_state;
   pipe->bind_gs_state   = softpipe_bind_gs_state;
   pipe->delete_gs_state = softpipe_delete_gs_state;

   pipe->set_constant_buffer = softpipe_set_constant_buffer;

   pipe->create_compute_state = softpipe_create_compute_state;
   pipe->bind_compute_state = softpipe_bind_compute_state;
   pipe->delete_compute_state = softpipe_delete_compute_state;
}
