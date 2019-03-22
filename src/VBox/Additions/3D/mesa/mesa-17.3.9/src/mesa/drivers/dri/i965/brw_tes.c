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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file brw_tes.c
 *
 * Tessellation evaluation shader state upload code.
 */

#include "brw_context.h"
#include "compiler/brw_nir.h"
#include "brw_program.h"
#include "brw_state.h"
#include "program/prog_parameter.h"

static void
brw_tes_debug_recompile(struct brw_context *brw, struct gl_program *prog,
                       const struct brw_tes_prog_key *key)
{
   perf_debug("Recompiling tessellation evaluation shader for program %d\n",
              prog->Id);

   bool found = false;
   const struct brw_tes_prog_key *old_key =
      brw_find_previous_compile(&brw->cache, BRW_CACHE_TES_PROG,
                                key->program_string_id);

   if (!old_key) {
      perf_debug("  Didn't find previous compile in the shader cache for "
                 "debug\n");
      return;
   }

   found |= brw_debug_recompile_sampler_key(brw, &old_key->tex, &key->tex);
   found |= key_debug(brw, "inputs read", old_key->inputs_read,
                      key->inputs_read);
   found |= key_debug(brw, "patch inputs read", old_key->patch_inputs_read,
                      key->patch_inputs_read);

   if (!found) {
      perf_debug("  Something else\n");
   }
}

static bool
brw_codegen_tes_prog(struct brw_context *brw,
                     struct brw_program *tep,
                     struct brw_tes_prog_key *key)
{
   const struct brw_compiler *compiler = brw->screen->compiler;
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   struct brw_stage_state *stage_state = &brw->tes.base;
   nir_shader *nir = tep->program.nir;
   struct brw_tes_prog_data prog_data;
   bool start_busy = false;
   double start_time = 0;

   memset(&prog_data, 0, sizeof(prog_data));

   void *mem_ctx = ralloc_context(NULL);

   brw_assign_common_binding_table_offsets(devinfo, &tep->program,
                                           &prog_data.base.base, 0);

   brw_nir_setup_glsl_uniforms(mem_ctx, nir, &tep->program,
                               &prog_data.base.base,
                               compiler->scalar_stage[MESA_SHADER_TESS_EVAL]);
   brw_nir_analyze_ubo_ranges(compiler, tep->program.nir,
                              prog_data.base.base.ubo_ranges);

   int st_index = -1;
   if (unlikely(INTEL_DEBUG & DEBUG_SHADER_TIME))
      st_index = brw_get_shader_time_index(brw, &tep->program, ST_TES, true);

   if (unlikely(brw->perf_debug)) {
      start_busy = brw->batch.last_bo && brw_bo_busy(brw->batch.last_bo);
      start_time = get_time();
   }

   struct brw_vue_map input_vue_map;
   brw_compute_tess_vue_map(&input_vue_map, key->inputs_read,
                            key->patch_inputs_read);

   unsigned program_size;
   char *error_str;
   const unsigned *program =
      brw_compile_tes(compiler, brw, mem_ctx, key, &input_vue_map, &prog_data,
                      nir, &tep->program, st_index, &program_size, &error_str);
   if (program == NULL) {
      tep->program.sh.data->LinkStatus = linking_failure;
      ralloc_strcat(&tep->program.sh.data->InfoLog, error_str);

      _mesa_problem(NULL, "Failed to compile tessellation evaluation shader: "
                    "%s\n", error_str);

      ralloc_free(mem_ctx);
      return false;
   }

   if (unlikely(brw->perf_debug)) {
      if (tep->compiled_once) {
         brw_tes_debug_recompile(brw, &tep->program, key);
      }
      if (start_busy && !brw_bo_busy(brw->batch.last_bo)) {
         perf_debug("TES compile took %.03f ms and stalled the GPU\n",
                    (get_time() - start_time) * 1000);
      }
      tep->compiled_once = true;
   }

   /* Scratch space is used for register spilling */
   brw_alloc_stage_scratch(brw, stage_state,
                           prog_data.base.base.total_scratch);

   /* The param and pull_param arrays will be freed by the shader cache. */
   ralloc_steal(NULL, prog_data.base.base.param);
   ralloc_steal(NULL, prog_data.base.base.pull_param);
   brw_upload_cache(&brw->cache, BRW_CACHE_TES_PROG,
                    key, sizeof(*key),
                    program, program_size,
                    &prog_data, sizeof(prog_data),
                    &stage_state->prog_offset, &brw->tes.base.prog_data);
   ralloc_free(mem_ctx);

   return true;
}

void
brw_tes_populate_key(struct brw_context *brw,
                     struct brw_tes_prog_key *key)
{
   struct brw_program *tcp =
      (struct brw_program *) brw->programs[MESA_SHADER_TESS_CTRL];
   struct brw_program *tep =
      (struct brw_program *) brw->programs[MESA_SHADER_TESS_EVAL];
   struct gl_program *prog = &tep->program;

   uint64_t per_vertex_slots = prog->info.inputs_read;
   uint32_t per_patch_slots = prog->info.patch_inputs_read;

   memset(key, 0, sizeof(*key));

   key->program_string_id = tep->id;

   /* The TCS may have additional outputs which aren't read by the
    * TES (possibly for cross-thread communication).  These need to
    * be stored in the Patch URB Entry as well.
    */
   if (tcp) {
      struct gl_program *tcp_prog = &tcp->program;
      per_vertex_slots |= tcp_prog->info.outputs_written &
         ~(VARYING_BIT_TESS_LEVEL_INNER | VARYING_BIT_TESS_LEVEL_OUTER);
      per_patch_slots |= tcp_prog->info.patch_outputs_written;
   }

   key->inputs_read = per_vertex_slots;
   key->patch_inputs_read = per_patch_slots;

   /* _NEW_TEXTURE */
   brw_populate_sampler_prog_key_data(&brw->ctx, prog, &key->tex);
}

void
brw_upload_tes_prog(struct brw_context *brw)
{
   struct brw_stage_state *stage_state = &brw->tes.base;
   struct brw_tes_prog_key key;
   /* BRW_NEW_TESS_PROGRAMS */
   struct brw_program *tep =
      (struct brw_program *) brw->programs[MESA_SHADER_TESS_EVAL];

   if (!brw_state_dirty(brw,
                        _NEW_TEXTURE,
                        BRW_NEW_TESS_PROGRAMS))
      return;

   brw_tes_populate_key(brw, &key);

   if (!brw_search_cache(&brw->cache, BRW_CACHE_TES_PROG,
                         &key, sizeof(key),
                         &stage_state->prog_offset,
                         &brw->tes.base.prog_data)) {
      bool success = brw_codegen_tes_prog(brw, tep, &key);
      assert(success);
      (void)success;
   }
}


bool
brw_tes_precompile(struct gl_context *ctx,
                   struct gl_shader_program *shader_prog,
                   struct gl_program *prog)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_tes_prog_key key;
   uint32_t old_prog_offset = brw->tes.base.prog_offset;
   struct brw_stage_prog_data *old_prog_data = brw->tes.base.prog_data;
   bool success;

   struct brw_program *btep = brw_program(prog);

   memset(&key, 0, sizeof(key));

   key.program_string_id = btep->id;
   key.inputs_read = prog->nir->info.inputs_read;
   key.patch_inputs_read = prog->nir->info.patch_inputs_read;

   if (shader_prog->_LinkedShaders[MESA_SHADER_TESS_CTRL]) {
      struct gl_program *tcp =
         shader_prog->_LinkedShaders[MESA_SHADER_TESS_CTRL]->Program;
      key.inputs_read |= tcp->nir->info.outputs_written &
         ~(VARYING_BIT_TESS_LEVEL_INNER | VARYING_BIT_TESS_LEVEL_OUTER);
      key.patch_inputs_read |= tcp->nir->info.patch_outputs_written;
   }

   brw_setup_tex_for_precompile(brw, &key.tex, prog);

   success = brw_codegen_tes_prog(brw, btep, &key);

   brw->tes.base.prog_offset = old_prog_offset;
   brw->tes.base.prog_data = old_prog_data;

   return success;
}
