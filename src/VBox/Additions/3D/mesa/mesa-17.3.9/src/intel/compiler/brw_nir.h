/*
 * Copyright © 2015 Intel Corporation
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

#ifndef BRW_NIR_H
#define BRW_NIR_H

#include "brw_reg.h"
#include "compiler/nir/nir.h"
#include "brw_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

int type_size_scalar(const struct glsl_type *type);
int type_size_vec4(const struct glsl_type *type);
int type_size_dvec4(const struct glsl_type *type);

static inline int
type_size_scalar_bytes(const struct glsl_type *type)
{
   return type_size_scalar(type) * 4;
}

static inline int
type_size_vec4_bytes(const struct glsl_type *type)
{
   return type_size_vec4(type) * 16;
}

/* Flags set in the instr->pass_flags field by i965 analysis passes */
enum {
   BRW_NIR_NON_BOOLEAN           = 0x0,

   /* Indicates that the given instruction's destination is a boolean
    * value but that it needs to be resolved before it can be used.
    * On Gen <= 5, CMP instructions return a 32-bit value where the bottom
    * bit represents the actual true/false value of the compare and the top
    * 31 bits are undefined.  In order to use this value, we have to do a
    * "resolve" operation by replacing the value of the CMP with -(x & 1)
    * to sign-extend the bottom bit to 0/~0.
    */
   BRW_NIR_BOOLEAN_NEEDS_RESOLVE = 0x1,

   /* Indicates that the given instruction's destination is a boolean
    * value that has intentionally been left unresolved.  Not all boolean
    * values need to be resolved immediately.  For instance, if we have
    *
    *    CMP r1 r2 r3
    *    CMP r4 r5 r6
    *    AND r7 r1 r4
    *
    * We don't have to resolve the result of the two CMP instructions
    * immediately because the AND still does an AND of the bottom bits.
    * Instead, we can save ourselves instructions by delaying the resolve
    * until after the AND.  The result of the two CMP instructions is left
    * as BRW_NIR_BOOLEAN_UNRESOLVED.
    */
   BRW_NIR_BOOLEAN_UNRESOLVED    = 0x2,

   /* Indicates a that the given instruction's destination is a boolean
    * value that does not need a resolve.  For instance, if you AND two
    * values that are BRW_NIR_BOOLEAN_NEEDS_RESOLVE then we know that both
    * values will be 0/~0 before we get them and the result of the AND is
    * also guaranteed to be 0/~0 and does not need a resolve.
    */
   BRW_NIR_BOOLEAN_NO_RESOLVE    = 0x3,

   /* A mask to mask the boolean status values off of instr->pass_flags */
   BRW_NIR_BOOLEAN_MASK          = 0x3,
};

void brw_nir_analyze_boolean_resolves(nir_shader *nir);

nir_shader *brw_preprocess_nir(const struct brw_compiler *compiler,
                               nir_shader *nir);

void
brw_nir_link_shaders(const struct brw_compiler *compiler,
                     nir_shader **producer, nir_shader **consumer);

bool brw_nir_lower_cs_intrinsics(nir_shader *nir,
                                 struct brw_cs_prog_data *prog_data);
void brw_nir_lower_vs_inputs(nir_shader *nir,
                             bool use_legacy_snorm_formula,
                             const uint8_t *vs_attrib_wa_flags);
void brw_nir_lower_vue_inputs(nir_shader *nir,
                              const struct brw_vue_map *vue_map);
void brw_nir_lower_tes_inputs(nir_shader *nir, const struct brw_vue_map *vue);
void brw_nir_lower_fs_inputs(nir_shader *nir,
                             const struct gen_device_info *devinfo,
                             const struct brw_wm_prog_key *key);
void brw_nir_lower_vue_outputs(nir_shader *nir, bool is_scalar);
void brw_nir_lower_tcs_outputs(nir_shader *nir, const struct brw_vue_map *vue,
                               GLenum tes_primitive_mode);
void brw_nir_lower_fs_outputs(nir_shader *nir);
void brw_nir_lower_cs_shared(nir_shader *nir);

nir_shader *brw_postprocess_nir(nir_shader *nir,
                                const struct brw_compiler *compiler,
                                bool is_scalar);

bool brw_nir_apply_attribute_workarounds(nir_shader *nir,
                                         bool use_legacy_snorm_formula,
                                         const uint8_t *attrib_wa_flags);

bool brw_nir_apply_trig_workarounds(nir_shader *nir);

void brw_nir_apply_tcs_quads_workaround(nir_shader *nir);

nir_shader *brw_nir_apply_sampler_key(nir_shader *nir,
                                      const struct brw_compiler *compiler,
                                      const struct brw_sampler_prog_key_data *key,
                                      bool is_scalar);

enum brw_reg_type brw_type_for_nir_type(const struct gen_device_info *devinfo,
                                        nir_alu_type type);

enum glsl_base_type brw_glsl_base_type_for_nir_type(nir_alu_type type);

void brw_nir_setup_glsl_uniforms(void *mem_ctx, nir_shader *shader,
                                 const struct gl_program *prog,
                                 struct brw_stage_prog_data *stage_prog_data,
                                 bool is_scalar);

void brw_nir_setup_arb_uniforms(void *mem_ctx, nir_shader *shader,
                                struct gl_program *prog,
                                struct brw_stage_prog_data *stage_prog_data);

void brw_nir_analyze_ubo_ranges(const struct brw_compiler *compiler,
                                nir_shader *nir,
                                struct brw_ubo_range out_ranges[4]);

bool brw_nir_opt_peephole_ffma(nir_shader *shader);

nir_shader *brw_nir_optimize(nir_shader *nir,
                             const struct brw_compiler *compiler,
                             bool is_scalar);

#define BRW_NIR_FRAG_OUTPUT_INDEX_SHIFT 0
#define BRW_NIR_FRAG_OUTPUT_INDEX_MASK INTEL_MASK(0, 0)
#define BRW_NIR_FRAG_OUTPUT_LOCATION_SHIFT 1
#define BRW_NIR_FRAG_OUTPUT_LOCATION_MASK INTEL_MASK(31, 1)

#ifdef __cplusplus
}
#endif

#endif /* BRW_NIR_H */
