/**************************************************************************
 * 
 * Copyright 2009 VMware, Inc.
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

/**
 * @file
 * Code generate the whole fragment pipeline.
 *
 * The fragment pipeline consists of the following stages:
 * - early depth test
 * - fragment shader
 * - alpha test
 * - depth/stencil test
 * - blending
 *
 * This file has only the glue to assemble the fragment pipeline.  The actual
 * plumbing of converting Gallium state into LLVM IR is done elsewhere, in the
 * lp_bld_*.[ch] files, and in a complete generic and reusable way. Here we
 * muster the LLVM JIT execution engine to create a function that follows an
 * established binary interface and that can be called from C directly.
 *
 * A big source of complexity here is that we often want to run different
 * stages with different precisions and data types and precisions. For example,
 * the fragment shader needs typically to be done in floats, but the
 * depth/stencil test and blending is better done in the type that most closely
 * matches the depth/stencil and color buffer respectively.
 *
 * Since the width of a SIMD vector register stays the same regardless of the
 * element type, different types imply different number of elements, so we must
 * code generate more instances of the stages with larger types to be able to
 * feed/consume the stages with smaller types.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 */

#include <limits.h>
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_pointer.h"
#include "util/u_format.h"
#include "util/u_dump.h"
#include "util/u_string.h"
#include "util/simple_list.h"
#include "util/u_dual_blend.h"
#include "os/os_time.h"
#include "pipe/p_shader_tokens.h"
#include "draw/draw_context.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_parse.h"
#include "gallivm/lp_bld_type.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_conv.h"
#include "gallivm/lp_bld_init.h"
#include "gallivm/lp_bld_intr.h"
#include "gallivm/lp_bld_logic.h"
#include "gallivm/lp_bld_tgsi.h"
#include "gallivm/lp_bld_swizzle.h"
#include "gallivm/lp_bld_flow.h"
#include "gallivm/lp_bld_debug.h"
#include "gallivm/lp_bld_arit.h"
#include "gallivm/lp_bld_bitarit.h"
#include "gallivm/lp_bld_pack.h"
#include "gallivm/lp_bld_format.h"
#include "gallivm/lp_bld_quad.h"

#include "lp_bld_alpha.h"
#include "lp_bld_blend.h"
#include "lp_bld_depth.h"
#include "lp_bld_interp.h"
#include "lp_context.h"
#include "lp_debug.h"
#include "lp_perf.h"
#include "lp_setup.h"
#include "lp_state.h"
#include "lp_tex_sample.h"
#include "lp_flush.h"
#include "lp_state_fs.h"
#include "lp_rast.h"


/** Fragment shader number (for debugging) */
static unsigned fs_no = 0;


/**
 * Expand the relevant bits of mask_input to a n*4-dword mask for the
 * n*four pixels in n 2x2 quads.  This will set the n*four elements of the
 * quad mask vector to 0 or ~0.
 * Grouping is 01, 23 for 2 quad mode hence only 0 and 2 are valid
 * quad arguments with fs length 8.
 *
 * \param first_quad  which quad(s) of the quad group to test, in [0,3]
 * \param mask_input  bitwise mask for the whole 4x4 stamp
 */
static LLVMValueRef
generate_quad_mask(struct gallivm_state *gallivm,
                   struct lp_type fs_type,
                   unsigned first_quad,
                   LLVMValueRef mask_input) /* int32 */
{
   LLVMBuilderRef builder = gallivm->builder;
   struct lp_type mask_type;
   LLVMTypeRef i32t = LLVMInt32TypeInContext(gallivm->context);
   LLVMValueRef bits[16];
   LLVMValueRef mask, bits_vec;
   int shift, i;

   /*
    * XXX: We'll need a different path for 16 x u8
    */
   assert(fs_type.width == 32);
   assert(fs_type.length <= ARRAY_SIZE(bits));
   mask_type = lp_int_type(fs_type);

   /*
    * mask_input >>= (quad * 4)
    */
   switch (first_quad) {
   case 0:
      shift = 0;
      break;
   case 1:
      assert(fs_type.length == 4);
      shift = 2;
      break;
   case 2:
      shift = 8;
      break;
   case 3:
      assert(fs_type.length == 4);
      shift = 10;
      break;
   default:
      assert(0);
      shift = 0;
   }

   mask_input = LLVMBuildLShr(builder,
                              mask_input,
                              LLVMConstInt(i32t, shift, 0),
                              "");

   /*
    * mask = { mask_input & (1 << i), for i in [0,3] }
    */
   mask = lp_build_broadcast(gallivm,
                             lp_build_vec_type(gallivm, mask_type),
                             mask_input);

   for (i = 0; i < fs_type.length / 4; i++) {
      unsigned j = 2 * (i % 2) + (i / 2) * 8;
      bits[4*i + 0] = LLVMConstInt(i32t, 1ULL << (j + 0), 0);
      bits[4*i + 1] = LLVMConstInt(i32t, 1ULL << (j + 1), 0);
      bits[4*i + 2] = LLVMConstInt(i32t, 1ULL << (j + 4), 0);
      bits[4*i + 3] = LLVMConstInt(i32t, 1ULL << (j + 5), 0);
   }
   bits_vec = LLVMConstVector(bits, fs_type.length);
   mask = LLVMBuildAnd(builder, mask, bits_vec, "");

   /*
    * mask = mask == bits ? ~0 : 0
    */
   mask = lp_build_compare(gallivm,
                           mask_type, PIPE_FUNC_EQUAL,
                           mask, bits_vec);

   return mask;
}


#define EARLY_DEPTH_TEST  0x1
#define LATE_DEPTH_TEST   0x2
#define EARLY_DEPTH_WRITE 0x4
#define LATE_DEPTH_WRITE  0x8

static int
find_output_by_semantic( const struct tgsi_shader_info *info,
			 unsigned semantic,
			 unsigned index )
{
   int i;

   for (i = 0; i < info->num_outputs; i++)
      if (info->output_semantic_name[i] == semantic &&
	  info->output_semantic_index[i] == index)
	 return i;

   return -1;
}


/**
 * Fetch the specified lp_jit_viewport structure for a given viewport_index.
 */
static LLVMValueRef
lp_llvm_viewport(LLVMValueRef context_ptr,
                 struct gallivm_state *gallivm,
                 LLVMValueRef viewport_index)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef ptr;
   LLVMValueRef res;
   struct lp_type viewport_type =
      lp_type_float_vec(32, 32 * LP_JIT_VIEWPORT_NUM_FIELDS);

   ptr = lp_jit_context_viewports(gallivm, context_ptr);
   ptr = LLVMBuildPointerCast(builder, ptr,
            LLVMPointerType(lp_build_vec_type(gallivm, viewport_type), 0), "");

   res = lp_build_pointer_get(builder, ptr, viewport_index);

   return res;
}


static LLVMValueRef
lp_build_depth_clamp(struct gallivm_state *gallivm,
                     LLVMBuilderRef builder,
                     struct lp_type type,
                     LLVMValueRef context_ptr,
                     LLVMValueRef thread_data_ptr,
                     LLVMValueRef z)
{
   LLVMValueRef viewport, min_depth, max_depth;
   LLVMValueRef viewport_index;
   struct lp_build_context f32_bld;

   assert(type.floating);
   lp_build_context_init(&f32_bld, gallivm, type);

   /*
    * Assumes clamping of the viewport index will occur in setup/gs. Value
    * is passed through the rasterization stage via lp_rast_shader_inputs.
    *
    * See: draw_clamp_viewport_idx and lp_clamp_viewport_idx for clamping
    *      semantics.
    */
   viewport_index = lp_jit_thread_data_raster_state_viewport_index(gallivm,
                       thread_data_ptr);

   /*
    * Load the min and max depth from the lp_jit_context.viewports
    * array of lp_jit_viewport structures.
    */
   viewport = lp_llvm_viewport(context_ptr, gallivm, viewport_index);

   /* viewports[viewport_index].min_depth */
   min_depth = LLVMBuildExtractElement(builder, viewport,
                  lp_build_const_int32(gallivm, LP_JIT_VIEWPORT_MIN_DEPTH), "");
   min_depth = lp_build_broadcast_scalar(&f32_bld, min_depth);

   /* viewports[viewport_index].max_depth */
   max_depth = LLVMBuildExtractElement(builder, viewport,
                  lp_build_const_int32(gallivm, LP_JIT_VIEWPORT_MAX_DEPTH), "");
   max_depth = lp_build_broadcast_scalar(&f32_bld, max_depth);

   /*
    * Clamp to the min and max depth values for the given viewport.
    */
   return lp_build_clamp(&f32_bld, z, min_depth, max_depth);
}


/**
 * Generate the fragment shader, depth/stencil test, and alpha tests.
 */
static void
generate_fs_loop(struct gallivm_state *gallivm,
                 struct lp_fragment_shader *shader,
                 const struct lp_fragment_shader_variant_key *key,
                 LLVMBuilderRef builder,
                 struct lp_type type,
                 LLVMValueRef context_ptr,
                 LLVMValueRef num_loop,
                 struct lp_build_interp_soa_context *interp,
                 struct lp_build_sampler_soa *sampler,
                 LLVMValueRef mask_store,
                 LLVMValueRef (*out_color)[4],
                 LLVMValueRef depth_ptr,
                 LLVMValueRef depth_stride,
                 LLVMValueRef facing,
                 LLVMValueRef thread_data_ptr)
{
   const struct util_format_description *zs_format_desc = NULL;
   const struct tgsi_token *tokens = shader->base.tokens;
   struct lp_type int_type = lp_int_type(type);
   LLVMTypeRef vec_type, int_vec_type;
   LLVMValueRef mask_ptr, mask_val;
   LLVMValueRef consts_ptr, num_consts_ptr;
   LLVMValueRef z;
   LLVMValueRef z_value, s_value;
   LLVMValueRef z_fb, s_fb;
   LLVMValueRef stencil_refs[2];
   LLVMValueRef outputs[PIPE_MAX_SHADER_OUTPUTS][TGSI_NUM_CHANNELS];
   struct lp_build_for_loop_state loop_state;
   struct lp_build_mask_context mask;
   /*
    * TODO: figure out if simple_shader optimization is really worthwile to
    * keep. Disabled because it may hide some real bugs in the (depth/stencil)
    * code since tests tend to take another codepath than real shaders.
    */
   boolean simple_shader = (shader->info.base.file_count[TGSI_FILE_SAMPLER] == 0 &&
                            shader->info.base.num_inputs < 3 &&
                            shader->info.base.num_instructions < 8) && 0;
   const boolean dual_source_blend = key->blend.rt[0].blend_enable &&
                                     util_blend_state_is_dual(&key->blend, 0);
   unsigned attrib;
   unsigned chan;
   unsigned cbuf;
   unsigned depth_mode;

   struct lp_bld_tgsi_system_values system_values;

   memset(&system_values, 0, sizeof(system_values));

   if (key->depth.enabled ||
       key->stencil[0].enabled) {

      zs_format_desc = util_format_description(key->zsbuf_format);
      assert(zs_format_desc);

      if (!shader->info.base.writes_z && !shader->info.base.writes_stencil) {
         if (key->alpha.enabled ||
             key->blend.alpha_to_coverage ||
             shader->info.base.uses_kill ||
             shader->info.base.writes_samplemask) {
            /* With alpha test and kill, can do the depth test early
             * and hopefully eliminate some quads.  But need to do a
             * special deferred depth write once the final mask value
             * is known. This only works though if there's either no
             * stencil test or the stencil value isn't written.
             */
            if (key->stencil[0].enabled && (key->stencil[0].writemask ||
                                            (key->stencil[1].enabled &&
                                             key->stencil[1].writemask)))
               depth_mode = LATE_DEPTH_TEST | LATE_DEPTH_WRITE;
            else
               depth_mode = EARLY_DEPTH_TEST | LATE_DEPTH_WRITE;
         }
         else
            depth_mode = EARLY_DEPTH_TEST | EARLY_DEPTH_WRITE;
      }
      else {
         depth_mode = LATE_DEPTH_TEST | LATE_DEPTH_WRITE;
      }

      if (!(key->depth.enabled && key->depth.writemask) &&
          !(key->stencil[0].enabled && (key->stencil[0].writemask ||
                                        (key->stencil[1].enabled &&
                                         key->stencil[1].writemask))))
         depth_mode &= ~(LATE_DEPTH_WRITE | EARLY_DEPTH_WRITE);
   }
   else {
      depth_mode = 0;
   }

   vec_type = lp_build_vec_type(gallivm, type);
   int_vec_type = lp_build_vec_type(gallivm, int_type);

   stencil_refs[0] = lp_jit_context_stencil_ref_front_value(gallivm, context_ptr);
   stencil_refs[1] = lp_jit_context_stencil_ref_back_value(gallivm, context_ptr);
   /* convert scalar stencil refs into vectors */
   stencil_refs[0] = lp_build_broadcast(gallivm, int_vec_type, stencil_refs[0]);
   stencil_refs[1] = lp_build_broadcast(gallivm, int_vec_type, stencil_refs[1]);

   consts_ptr = lp_jit_context_constants(gallivm, context_ptr);
   num_consts_ptr = lp_jit_context_num_constants(gallivm, context_ptr);

   lp_build_for_loop_begin(&loop_state, gallivm,
                           lp_build_const_int32(gallivm, 0),
                           LLVMIntULT,
                           num_loop,
                           lp_build_const_int32(gallivm, 1));

   mask_ptr = LLVMBuildGEP(builder, mask_store,
                           &loop_state.counter, 1, "mask_ptr");
   mask_val = LLVMBuildLoad(builder, mask_ptr, "");

   memset(outputs, 0, sizeof outputs);

   for(cbuf = 0; cbuf < key->nr_cbufs; cbuf++) {
      for(chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
         out_color[cbuf][chan] = lp_build_array_alloca(gallivm,
                                                       lp_build_vec_type(gallivm,
                                                                         type),
                                                       num_loop, "color");
      }
   }
   if (dual_source_blend) {
      assert(key->nr_cbufs <= 1);
      for(chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
         out_color[1][chan] = lp_build_array_alloca(gallivm,
                                                    lp_build_vec_type(gallivm,
                                                                      type),
                                                    num_loop, "color1");
      }
   }


   /* 'mask' will control execution based on quad's pixel alive/killed state */
   lp_build_mask_begin(&mask, gallivm, type, mask_val);

   if (!(depth_mode & EARLY_DEPTH_TEST) && !simple_shader)
      lp_build_mask_check(&mask);

   lp_build_interp_soa_update_pos_dyn(interp, gallivm, loop_state.counter);
   z = interp->pos[2];

   if (depth_mode & EARLY_DEPTH_TEST) {
      /*
       * Clamp according to ARB_depth_clamp semantics.
       */
      if (key->depth_clamp) {
         z = lp_build_depth_clamp(gallivm, builder, type, context_ptr,
                                  thread_data_ptr, z);
      }
      lp_build_depth_stencil_load_swizzled(gallivm, type,
                                           zs_format_desc, key->resource_1d,
                                           depth_ptr, depth_stride,
                                           &z_fb, &s_fb, loop_state.counter);
      lp_build_depth_stencil_test(gallivm,
                                  &key->depth,
                                  key->stencil,
                                  type,
                                  zs_format_desc,
                                  &mask,
                                  stencil_refs,
                                  z, z_fb, s_fb,
                                  facing,
                                  &z_value, &s_value,
                                  !simple_shader);

      if (depth_mode & EARLY_DEPTH_WRITE) {
         lp_build_depth_stencil_write_swizzled(gallivm, type,
                                               zs_format_desc, key->resource_1d,
                                               NULL, NULL, NULL, loop_state.counter,
                                               depth_ptr, depth_stride,
                                               z_value, s_value);
      }
      /*
       * Note mask check if stencil is enabled must be after ds write not after
       * stencil test otherwise new stencil values may not get written if all
       * fragments got killed by depth/stencil test.
       */
      if (!simple_shader && key->stencil[0].enabled)
         lp_build_mask_check(&mask);
   }

   lp_build_interp_soa_update_inputs_dyn(interp, gallivm, loop_state.counter);

   /* Build the actual shader */
   lp_build_tgsi_soa(gallivm, tokens, type, &mask,
                     consts_ptr, num_consts_ptr, &system_values,
                     interp->inputs,
                     outputs, context_ptr, thread_data_ptr,
                     sampler, &shader->info.base, NULL);

   /* Alpha test */
   if (key->alpha.enabled) {
      int color0 = find_output_by_semantic(&shader->info.base,
                                           TGSI_SEMANTIC_COLOR,
                                           0);

      if (color0 != -1 && outputs[color0][3]) {
         const struct util_format_description *cbuf_format_desc;
         LLVMValueRef alpha = LLVMBuildLoad(builder, outputs[color0][3], "alpha");
         LLVMValueRef alpha_ref_value;

         alpha_ref_value = lp_jit_context_alpha_ref_value(gallivm, context_ptr);
         alpha_ref_value = lp_build_broadcast(gallivm, vec_type, alpha_ref_value);

         cbuf_format_desc = util_format_description(key->cbuf_format[0]);

         lp_build_alpha_test(gallivm, key->alpha.func, type, cbuf_format_desc,
                             &mask, alpha, alpha_ref_value,
                             (depth_mode & LATE_DEPTH_TEST) != 0);
      }
   }

   /* Emulate Alpha to Coverage with Alpha test */
   if (key->blend.alpha_to_coverage) {
      int color0 = find_output_by_semantic(&shader->info.base,
                                           TGSI_SEMANTIC_COLOR,
                                           0);

      if (color0 != -1 && outputs[color0][3]) {
         LLVMValueRef alpha = LLVMBuildLoad(builder, outputs[color0][3], "alpha");

         lp_build_alpha_to_coverage(gallivm, type,
                                    &mask, alpha,
                                    (depth_mode & LATE_DEPTH_TEST) != 0);
      }
   }

   if (shader->info.base.writes_samplemask) {
      int smaski = find_output_by_semantic(&shader->info.base,
                                           TGSI_SEMANTIC_SAMPLEMASK,
                                           0);
      LLVMValueRef smask;
      struct lp_build_context smask_bld;
      lp_build_context_init(&smask_bld, gallivm, int_type);

      assert(smaski >= 0);
      smask = LLVMBuildLoad(builder, outputs[smaski][0], "smask");
      /*
       * Pixel is alive according to the first sample in the mask.
       */
      smask = LLVMBuildBitCast(builder, smask, smask_bld.vec_type, "");
      smask = lp_build_and(&smask_bld, smask, smask_bld.one);
      smask = lp_build_cmp(&smask_bld, PIPE_FUNC_NOTEQUAL, smask, smask_bld.zero);
      lp_build_mask_update(&mask, smask);
   }

   /* Late Z test */
   if (depth_mode & LATE_DEPTH_TEST) {
      int pos0 = find_output_by_semantic(&shader->info.base,
                                         TGSI_SEMANTIC_POSITION,
                                         0);
      int s_out = find_output_by_semantic(&shader->info.base,
                                          TGSI_SEMANTIC_STENCIL,
                                          0);
      if (pos0 != -1 && outputs[pos0][2]) {
         z = LLVMBuildLoad(builder, outputs[pos0][2], "output.z");
      }
      /*
       * Clamp according to ARB_depth_clamp semantics.
       */
      if (key->depth_clamp) {
         z = lp_build_depth_clamp(gallivm, builder, type, context_ptr,
                                  thread_data_ptr, z);
      }

      if (s_out != -1 && outputs[s_out][1]) {
         /* there's only one value, and spec says to discard additional bits */
         LLVMValueRef s_max_mask = lp_build_const_int_vec(gallivm, int_type, 255);
         stencil_refs[0] = LLVMBuildLoad(builder, outputs[s_out][1], "output.s");
         stencil_refs[0] = LLVMBuildBitCast(builder, stencil_refs[0], int_vec_type, "");
         stencil_refs[0] = LLVMBuildAnd(builder, stencil_refs[0], s_max_mask, "");
         stencil_refs[1] = stencil_refs[0];
      }

      lp_build_depth_stencil_load_swizzled(gallivm, type,
                                           zs_format_desc, key->resource_1d,
                                           depth_ptr, depth_stride,
                                           &z_fb, &s_fb, loop_state.counter);

      lp_build_depth_stencil_test(gallivm,
                                  &key->depth,
                                  key->stencil,
                                  type,
                                  zs_format_desc,
                                  &mask,
                                  stencil_refs,
                                  z, z_fb, s_fb,
                                  facing,
                                  &z_value, &s_value,
                                  !simple_shader);
      /* Late Z write */
      if (depth_mode & LATE_DEPTH_WRITE) {
         lp_build_depth_stencil_write_swizzled(gallivm, type,
                                               zs_format_desc, key->resource_1d,
                                               NULL, NULL, NULL, loop_state.counter,
                                               depth_ptr, depth_stride,
                                               z_value, s_value);
      }
   }
   else if ((depth_mode & EARLY_DEPTH_TEST) &&
            (depth_mode & LATE_DEPTH_WRITE))
   {
      /* Need to apply a reduced mask to the depth write.  Reload the
       * depth value, update from zs_value with the new mask value and
       * write that out.
       */
      lp_build_depth_stencil_write_swizzled(gallivm, type,
                                            zs_format_desc, key->resource_1d,
                                            &mask, z_fb, s_fb, loop_state.counter,
                                            depth_ptr, depth_stride,
                                            z_value, s_value);
   }


   /* Color write  */
   for (attrib = 0; attrib < shader->info.base.num_outputs; ++attrib)
   {
      unsigned cbuf = shader->info.base.output_semantic_index[attrib];
      if ((shader->info.base.output_semantic_name[attrib] == TGSI_SEMANTIC_COLOR) &&
           ((cbuf < key->nr_cbufs) || (cbuf == 1 && dual_source_blend)))
      {
         for(chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
            if(outputs[attrib][chan]) {
               /* XXX: just initialize outputs to point at colors[] and
                * skip this.
                */
               LLVMValueRef out = LLVMBuildLoad(builder, outputs[attrib][chan], "");
               LLVMValueRef color_ptr;
               color_ptr = LLVMBuildGEP(builder, out_color[cbuf][chan],
                                        &loop_state.counter, 1, "");
               lp_build_name(out, "color%u.%c", attrib, "rgba"[chan]);
               LLVMBuildStore(builder, out, color_ptr);
            }
         }
      }
   }

   if (key->occlusion_count) {
      LLVMValueRef counter = lp_jit_thread_data_counter(gallivm, thread_data_ptr);
      lp_build_name(counter, "counter");
      lp_build_occlusion_count(gallivm, type,
                               lp_build_mask_value(&mask), counter);
   }

   mask_val = lp_build_mask_end(&mask);
   LLVMBuildStore(builder, mask_val, mask_ptr);
   lp_build_for_loop_end(&loop_state);
}


/**
 * This function will reorder pixels from the fragment shader SoA to memory layout AoS
 *
 * Fragment Shader outputs pixels in small 2x2 blocks
 *  e.g. (0, 0), (1, 0), (0, 1), (1, 1) ; (2, 0) ...
 *
 * However in memory pixels are stored in rows
 *  e.g. (0, 0), (1, 0), (2, 0), (3, 0) ; (0, 1) ...
 *
 * @param type            fragment shader type (4x or 8x float)
 * @param num_fs          number of fs_src
 * @param is_1d           whether we're outputting to a 1d resource
 * @param dst_channels    number of output channels
 * @param fs_src          output from fragment shader
 * @param dst             pointer to store result
 * @param pad_inline      is channel padding inline or at end of row
 * @return                the number of dsts
 */
static int
generate_fs_twiddle(struct gallivm_state *gallivm,
                    struct lp_type type,
                    unsigned num_fs,
                    unsigned dst_channels,
                    LLVMValueRef fs_src[][4],
                    LLVMValueRef* dst,
                    bool pad_inline)
{
   LLVMValueRef src[16];

   bool swizzle_pad;
   bool twiddle;
   bool split;

   unsigned pixels = type.length / 4;
   unsigned reorder_group;
   unsigned src_channels;
   unsigned src_count;
   unsigned i;

   src_channels = dst_channels < 3 ? dst_channels : 4;
   src_count = num_fs * src_channels;

   assert(pixels == 2 || pixels == 1);
   assert(num_fs * src_channels <= ARRAY_SIZE(src));

   /*
    * Transpose from SoA -> AoS
    */
   for (i = 0; i < num_fs; ++i) {
      lp_build_transpose_aos_n(gallivm, type, &fs_src[i][0], src_channels, &src[i * src_channels]);
   }

   /*
    * Pick transformation options
    */
   swizzle_pad = false;
   twiddle = false;
   split = false;
   reorder_group = 0;

   if (dst_channels == 1) {
      twiddle = true;

      if (pixels == 2) {
         split = true;
      }
   } else if (dst_channels == 2) {
      if (pixels == 1) {
         reorder_group = 1;
      }
   } else if (dst_channels > 2) {
      if (pixels == 1) {
         reorder_group = 2;
      } else {
         twiddle = true;
      }

      if (!pad_inline && dst_channels == 3 && pixels > 1) {
         swizzle_pad = true;
      }
   }

   /*
    * Split the src in half
    */
   if (split) {
      for (i = num_fs; i > 0; --i) {
         src[(i - 1)*2 + 1] = lp_build_extract_range(gallivm, src[i - 1], 4, 4);
         src[(i - 1)*2 + 0] = lp_build_extract_range(gallivm, src[i - 1], 0, 4);
      }

      src_count *= 2;
      type.length = 4;
   }

   /*
    * Ensure pixels are in memory order
    */
   if (reorder_group) {
      /* Twiddle pixels by reordering the array, e.g.:
       *
       * src_count =  8 -> 0 2 1 3 4 6 5 7
       * src_count = 16 -> 0 1 4 5 2 3 6 7 8 9 12 13 10 11 14 15
       */
      const unsigned reorder_sw[] = { 0, 2, 1, 3 };

      for (i = 0; i < src_count; ++i) {
         unsigned group = i / reorder_group;
         unsigned block = (group / 4) * 4 * reorder_group;
         unsigned j = block + (reorder_sw[group % 4] * reorder_group) + (i % reorder_group);
         dst[i] = src[j];
      }
   } else if (twiddle) {
      /* Twiddle pixels across elements of array */
      /*
       * XXX: we should avoid this in some cases, but would need to tell
       * lp_build_conv to reorder (or deal with it ourselves).
       */
      lp_bld_quad_twiddle(gallivm, type, src, src_count, dst);
   } else {
      /* Do nothing */
      memcpy(dst, src, sizeof(LLVMValueRef) * src_count);
   }

   /*
    * Moves any padding between pixels to the end
    * e.g. RGBXRGBX -> RGBRGBXX
    */
   if (swizzle_pad) {
      unsigned char swizzles[16];
      unsigned elems = pixels * dst_channels;

      for (i = 0; i < type.length; ++i) {
         if (i < elems)
            swizzles[i] = i % dst_channels + (i / dst_channels) * 4;
         else
            swizzles[i] = LP_BLD_SWIZZLE_DONTCARE;
      }

      for (i = 0; i < src_count; ++i) {
         dst[i] = lp_build_swizzle_aos_n(gallivm, dst[i], swizzles, type.length, type.length);
      }
   }

   return src_count;
}


/*
 * Untwiddle and transpose, much like the above.
 * However, this is after conversion, so we get packed vectors.
 * At this time only handle 4x16i8 rgba / 2x16i8 rg / 1x16i8 r data,
 * the vectors will look like:
 * r0r1r4r5r2r3r6r7r8r9r12... (albeit color channels may
 * be swizzled here). Extending to 16bit should be trivial.
 * Should also be extended to handle twice wide vectors with AVX2...
 */
static void
fs_twiddle_transpose(struct gallivm_state *gallivm,
                     struct lp_type type,
                     LLVMValueRef *src,
                     unsigned src_count,
                     LLVMValueRef *dst)
{
   unsigned i, j;
   struct lp_type type64, type16, type32;
   LLVMTypeRef type64_t, type8_t, type16_t, type32_t;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef tmp[4], shuf[8];
   for (j = 0; j < 2; j++) {
      shuf[j*4 + 0] = lp_build_const_int32(gallivm, j*4 + 0);
      shuf[j*4 + 1] = lp_build_const_int32(gallivm, j*4 + 2);
      shuf[j*4 + 2] = lp_build_const_int32(gallivm, j*4 + 1);
      shuf[j*4 + 3] = lp_build_const_int32(gallivm, j*4 + 3);
   }

   assert(src_count == 4 || src_count == 2 || src_count == 1);
   assert(type.width == 8);
   assert(type.length == 16);

   type8_t = lp_build_vec_type(gallivm, type);

   type64 = type;
   type64.length /= 8;
   type64.width *= 8;
   type64_t = lp_build_vec_type(gallivm, type64);

   type16 = type;
   type16.length /= 2;
   type16.width *= 2;
   type16_t = lp_build_vec_type(gallivm, type16);

   type32 = type;
   type32.length /= 4;
   type32.width *= 4;
   type32_t = lp_build_vec_type(gallivm, type32);

   lp_build_transpose_aos_n(gallivm, type, src, src_count, tmp);

   if (src_count == 1) {
      /* transpose was no-op, just untwiddle */
      LLVMValueRef shuf_vec;
      shuf_vec = LLVMConstVector(shuf, 8);
      tmp[0] = LLVMBuildBitCast(builder, src[0], type16_t, "");
      tmp[0] = LLVMBuildShuffleVector(builder, tmp[0], tmp[0], shuf_vec, "");
      dst[0] = LLVMBuildBitCast(builder, tmp[0], type8_t, "");
   } else if (src_count == 2) {
      LLVMValueRef shuf_vec;
      shuf_vec = LLVMConstVector(shuf, 4);

      for (i = 0; i < 2; i++) {
         tmp[i] = LLVMBuildBitCast(builder, tmp[i], type32_t, "");
         tmp[i] = LLVMBuildShuffleVector(builder, tmp[i], tmp[i], shuf_vec, "");
         dst[i] = LLVMBuildBitCast(builder, tmp[i], type8_t, "");
      }
   } else {
      for (j = 0; j < 2; j++) {
         LLVMValueRef lo, hi, lo2, hi2;
          /*
          * Note that if we only really have 3 valid channels (rgb)
          * and we don't need alpha we could substitute a undef here
          * for the respective channel (causing llvm to drop conversion
          * for alpha).
          */
         /* we now have rgba0rgba1rgba4rgba5 etc, untwiddle */
         lo2 = LLVMBuildBitCast(builder, tmp[j*2], type64_t, "");
         hi2 = LLVMBuildBitCast(builder, tmp[j*2 + 1], type64_t, "");
         lo = lp_build_interleave2(gallivm, type64, lo2, hi2, 0);
         hi = lp_build_interleave2(gallivm, type64, lo2, hi2, 1);
         dst[j*2] = LLVMBuildBitCast(builder, lo, type8_t, "");
         dst[j*2 + 1] = LLVMBuildBitCast(builder, hi, type8_t, "");
      }
   }
}


/**
 * Load an unswizzled block of pixels from memory
 */
static void
load_unswizzled_block(struct gallivm_state *gallivm,
                      LLVMValueRef base_ptr,
                      LLVMValueRef stride,
                      unsigned block_width,
                      unsigned block_height,
                      LLVMValueRef* dst,
                      struct lp_type dst_type,
                      unsigned dst_count,
                      unsigned dst_alignment)
{
   LLVMBuilderRef builder = gallivm->builder;
   unsigned row_size = dst_count / block_height;
   unsigned i;

   /* Ensure block exactly fits into dst */
   assert((block_width * block_height) % dst_count == 0);

   for (i = 0; i < dst_count; ++i) {
      unsigned x = i % row_size;
      unsigned y = i / row_size;

      LLVMValueRef bx = lp_build_const_int32(gallivm, x * (dst_type.width / 8) * dst_type.length);
      LLVMValueRef by = LLVMBuildMul(builder, lp_build_const_int32(gallivm, y), stride, "");

      LLVMValueRef gep[2];
      LLVMValueRef dst_ptr;

      gep[0] = lp_build_const_int32(gallivm, 0);
      gep[1] = LLVMBuildAdd(builder, bx, by, "");

      dst_ptr = LLVMBuildGEP(builder, base_ptr, gep, 2, "");
      dst_ptr = LLVMBuildBitCast(builder, dst_ptr,
                                 LLVMPointerType(lp_build_vec_type(gallivm, dst_type), 0), "");

      dst[i] = LLVMBuildLoad(builder, dst_ptr, "");

      LLVMSetAlignment(dst[i], dst_alignment);
   }
}


/**
 * Store an unswizzled block of pixels to memory
 */
static void
store_unswizzled_block(struct gallivm_state *gallivm,
                       LLVMValueRef base_ptr,
                       LLVMValueRef stride,
                       unsigned block_width,
                       unsigned block_height,
                       LLVMValueRef* src,
                       struct lp_type src_type,
                       unsigned src_count,
                       unsigned src_alignment)
{
   LLVMBuilderRef builder = gallivm->builder;
   unsigned row_size = src_count / block_height;
   unsigned i;

   /* Ensure src exactly fits into block */
   assert((block_width * block_height) % src_count == 0);

   for (i = 0; i < src_count; ++i) {
      unsigned x = i % row_size;
      unsigned y = i / row_size;

      LLVMValueRef bx = lp_build_const_int32(gallivm, x * (src_type.width / 8) * src_type.length);
      LLVMValueRef by = LLVMBuildMul(builder, lp_build_const_int32(gallivm, y), stride, "");

      LLVMValueRef gep[2];
      LLVMValueRef src_ptr;

      gep[0] = lp_build_const_int32(gallivm, 0);
      gep[1] = LLVMBuildAdd(builder, bx, by, "");

      src_ptr = LLVMBuildGEP(builder, base_ptr, gep, 2, "");
      src_ptr = LLVMBuildBitCast(builder, src_ptr,
                                 LLVMPointerType(lp_build_vec_type(gallivm, src_type), 0), "");

      src_ptr = LLVMBuildStore(builder, src[i], src_ptr);

      LLVMSetAlignment(src_ptr, src_alignment);
   }
}


/**
 * Checks if a format description is an arithmetic format
 *
 * A format which has irregular channel sizes such as R3_G3_B2 or R5_G6_B5.
 */
static inline boolean
is_arithmetic_format(const struct util_format_description *format_desc)
{
   boolean arith = false;
   unsigned i;

   for (i = 0; i < format_desc->nr_channels; ++i) {
      arith |= format_desc->channel[i].size != format_desc->channel[0].size;
      arith |= (format_desc->channel[i].size % 8) != 0;
   }

   return arith;
}


/**
 * Checks if this format requires special handling due to required expansion
 * to floats for blending, and furthermore has "natural" packed AoS -> unpacked
 * SoA conversion.
 */
static inline boolean
format_expands_to_float_soa(const struct util_format_description *format_desc)
{
   if (format_desc->format == PIPE_FORMAT_R11G11B10_FLOAT ||
       format_desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
      return true;
   }
   return false;
}


/**
 * Retrieves the type representing the memory layout for a format
 *
 * e.g. RGBA16F = 4x half-float and R3G3B2 = 1x byte
 */
static inline void
lp_mem_type_from_format_desc(const struct util_format_description *format_desc,
                             struct lp_type* type)
{
   unsigned i;
   unsigned chan;

   if (format_expands_to_float_soa(format_desc)) {
      /* just make this a uint with width of block */
      type->floating = false;
      type->fixed = false;
      type->sign = false;
      type->norm = false;
      type->width = format_desc->block.bits;
      type->length = 1;
      return;
   }

   for (i = 0; i < 4; i++)
      if (format_desc->channel[i].type != UTIL_FORMAT_TYPE_VOID)
         break;
   chan = i;

   memset(type, 0, sizeof(struct lp_type));
   type->floating = format_desc->channel[chan].type == UTIL_FORMAT_TYPE_FLOAT;
   type->fixed    = format_desc->channel[chan].type == UTIL_FORMAT_TYPE_FIXED;
   type->sign     = format_desc->channel[chan].type != UTIL_FORMAT_TYPE_UNSIGNED;
   type->norm     = format_desc->channel[chan].normalized;

   if (is_arithmetic_format(format_desc)) {
      type->width = 0;
      type->length = 1;

      for (i = 0; i < format_desc->nr_channels; ++i) {
         type->width += format_desc->channel[i].size;
      }
   } else {
      type->width = format_desc->channel[chan].size;
      type->length = format_desc->nr_channels;
   }
}


/**
 * Retrieves the type for a format which is usable in the blending code.
 *
 * e.g. RGBA16F = 4x float, R3G3B2 = 3x byte
 */
static inline void
lp_blend_type_from_format_desc(const struct util_format_description *format_desc,
                               struct lp_type* type)
{
   unsigned i;
   unsigned chan;

   if (format_expands_to_float_soa(format_desc)) {
      /* always use ordinary floats for blending */
      type->floating = true;
      type->fixed = false;
      type->sign = true;
      type->norm = false;
      type->width = 32;
      type->length = 4;
      return;
   }

   for (i = 0; i < 4; i++)
      if (format_desc->channel[i].type != UTIL_FORMAT_TYPE_VOID)
         break;
   chan = i;

   memset(type, 0, sizeof(struct lp_type));
   type->floating = format_desc->channel[chan].type == UTIL_FORMAT_TYPE_FLOAT;
   type->fixed    = format_desc->channel[chan].type == UTIL_FORMAT_TYPE_FIXED;
   type->sign     = format_desc->channel[chan].type != UTIL_FORMAT_TYPE_UNSIGNED;
   type->norm     = format_desc->channel[chan].normalized;
   type->width    = format_desc->channel[chan].size;
   type->length   = format_desc->nr_channels;

   for (i = 1; i < format_desc->nr_channels; ++i) {
      if (format_desc->channel[i].size > type->width)
         type->width = format_desc->channel[i].size;
   }

   if (type->floating) {
      type->width = 32;
   } else {
      if (type->width <= 8) {
         type->width = 8;
      } else if (type->width <= 16) {
         type->width = 16;
      } else {
         type->width = 32;
      }
   }

   if (is_arithmetic_format(format_desc) && type->length == 3) {
      type->length = 4;
   }
}


/**
 * Scale a normalized value from src_bits to dst_bits.
 *
 * The exact calculation is
 *
 *    dst = iround(src * dst_mask / src_mask)
 *
 *  or with integer rounding
 *
 *    dst = src * (2*dst_mask + sign(src)*src_mask) / (2*src_mask)
 *
 *  where
 *
 *    src_mask = (1 << src_bits) - 1
 *    dst_mask = (1 << dst_bits) - 1
 *
 * but we try to avoid division and multiplication through shifts.
 */
static inline LLVMValueRef
scale_bits(struct gallivm_state *gallivm,
           int src_bits,
           int dst_bits,
           LLVMValueRef src,
           struct lp_type src_type)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef result = src;

   if (dst_bits < src_bits) {
      int delta_bits = src_bits - dst_bits;

      if (delta_bits <= dst_bits) {
         /*
          * Approximate the rescaling with a single shift.
          *
          * This gives the wrong rounding.
          */

         result = LLVMBuildLShr(builder,
                                src,
                                lp_build_const_int_vec(gallivm, src_type, delta_bits),
                                "");

      } else {
         /*
          * Try more accurate rescaling.
          */

         /*
          * Drop the least significant bits to make space for the multiplication.
          *
          * XXX: A better approach would be to use a wider integer type as intermediate.  But
          * this is enough to convert alpha from 16bits -> 2 when rendering to
          * PIPE_FORMAT_R10G10B10A2_UNORM.
          */
         result = LLVMBuildLShr(builder,
                                src,
                                lp_build_const_int_vec(gallivm, src_type, dst_bits),
                                "");


         result = LLVMBuildMul(builder,
                               result,
                               lp_build_const_int_vec(gallivm, src_type, (1LL << dst_bits) - 1),
                               "");

         /*
          * Add a rounding term before the division.
          *
          * TODO: Handle signed integers too.
          */
         if (!src_type.sign) {
            result = LLVMBuildAdd(builder,
                                  result,
                                  lp_build_const_int_vec(gallivm, src_type, (1LL << (delta_bits - 1))),
                                  "");
         }

         /*
          * Approximate the division by src_mask with a src_bits shift.
          *
          * Given the src has already been shifted by dst_bits, all we need
          * to do is to shift by the difference.
          */

         result = LLVMBuildLShr(builder,
                                result,
                                lp_build_const_int_vec(gallivm, src_type, delta_bits),
                                "");
      }

   } else if (dst_bits > src_bits) {
      /* Scale up bits */
      int db = dst_bits - src_bits;

      /* Shift left by difference in bits */
      result = LLVMBuildShl(builder,
                            src,
                            lp_build_const_int_vec(gallivm, src_type, db),
                            "");

      if (db <= src_bits) {
         /* Enough bits in src to fill the remainder */
         LLVMValueRef lower = LLVMBuildLShr(builder,
                                            src,
                                            lp_build_const_int_vec(gallivm, src_type, src_bits - db),
                                            "");

         result = LLVMBuildOr(builder, result, lower, "");
      } else if (db > src_bits) {
         /* Need to repeatedly copy src bits to fill remainder in dst */
         unsigned n;

         for (n = src_bits; n < dst_bits; n *= 2) {
            LLVMValueRef shuv = lp_build_const_int_vec(gallivm, src_type, n);

            result = LLVMBuildOr(builder,
                                 result,
                                 LLVMBuildLShr(builder, result, shuv, ""),
                                 "");
         }
      }
   }

   return result;
}

/**
 * If RT is a smallfloat (needing denorms) format
 */
static inline int
have_smallfloat_format(struct lp_type dst_type,
                       enum pipe_format format)
{
   return ((dst_type.floating && dst_type.width != 32) ||
    /* due to format handling hacks this format doesn't have floating set
     * here (and actually has width set to 32 too) so special case this. */
    (format == PIPE_FORMAT_R11G11B10_FLOAT));
}


/**
 * Convert from memory format to blending format
 *
 * e.g. GL_R3G3B2 is 1 byte in memory but 3 bytes for blending
 */
static void
convert_to_blend_type(struct gallivm_state *gallivm,
                      unsigned block_size,
                      const struct util_format_description *src_fmt,
                      struct lp_type src_type,
                      struct lp_type dst_type,
                      LLVMValueRef* src, // and dst
                      unsigned num_srcs)
{
   LLVMValueRef *dst = src;
   LLVMBuilderRef builder = gallivm->builder;
   struct lp_type blend_type;
   struct lp_type mem_type;
   unsigned i, j;
   unsigned pixels = block_size / num_srcs;
   bool is_arith;

   /*
    * full custom path for packed floats and srgb formats - none of the later
    * functions would do anything useful, and given the lp_type representation they
    * can't be fixed. Should really have some SoA blend path for these kind of
    * formats rather than hacking them in here.
    */
   if (format_expands_to_float_soa(src_fmt)) {
      LLVMValueRef tmpsrc[4];
      /*
       * This is pretty suboptimal for this case blending in SoA would be much
       * better, since conversion gets us SoA values so need to convert back.
       */
      assert(src_type.width == 32 || src_type.width == 16);
      assert(dst_type.floating);
      assert(dst_type.width == 32);
      assert(dst_type.length % 4 == 0);
      assert(num_srcs % 4 == 0);

      if (src_type.width == 16) {
         /* expand 4x16bit values to 4x32bit */
         struct lp_type type32x4 = src_type;
         LLVMTypeRef ltype32x4;
         unsigned num_fetch = dst_type.length == 8 ? num_srcs / 2 : num_srcs / 4;
         type32x4.width = 32;
         ltype32x4 = lp_build_vec_type(gallivm, type32x4);
         for (i = 0; i < num_fetch; i++) {
            src[i] = LLVMBuildZExt(builder, src[i], ltype32x4, "");
         }
         src_type.width = 32;
      }
      for (i = 0; i < 4; i++) {
         tmpsrc[i] = src[i];
      }
      for (i = 0; i < num_srcs / 4; i++) {
         LLVMValueRef tmpsoa[4];
         LLVMValueRef tmps = tmpsrc[i];
         if (dst_type.length == 8) {
            LLVMValueRef shuffles[8];
            unsigned j;
            /* fetch was 4 values but need 8-wide output values */
            tmps = lp_build_concat(gallivm, &tmpsrc[i * 2], src_type, 2);
            /*
             * for 8-wide aos transpose would give us wrong order not matching
             * incoming converted fs values and mask. ARGH.
             */
            for (j = 0; j < 4; j++) {
               shuffles[j] = lp_build_const_int32(gallivm, j * 2);
               shuffles[j + 4] = lp_build_const_int32(gallivm, j * 2 + 1);
            }
            tmps = LLVMBuildShuffleVector(builder, tmps, tmps,
                                          LLVMConstVector(shuffles, 8), "");
         }
         if (src_fmt->format == PIPE_FORMAT_R11G11B10_FLOAT) {
            lp_build_r11g11b10_to_float(gallivm, tmps, tmpsoa);
         }
         else {
            lp_build_unpack_rgba_soa(gallivm, src_fmt, dst_type, tmps, tmpsoa);
         }
         lp_build_transpose_aos(gallivm, dst_type, tmpsoa, &src[i * 4]);
      }
      return;
   }

   lp_mem_type_from_format_desc(src_fmt, &mem_type);
   lp_blend_type_from_format_desc(src_fmt, &blend_type);

   /* Is the format arithmetic */
   is_arith = blend_type.length * blend_type.width != mem_type.width * mem_type.length;
   is_arith &= !(mem_type.width == 16 && mem_type.floating);

   /* Pad if necessary */
   if (!is_arith && src_type.length < dst_type.length) {
      for (i = 0; i < num_srcs; ++i) {
         dst[i] = lp_build_pad_vector(gallivm, src[i], dst_type.length);
      }

      src_type.length = dst_type.length;
   }

   /* Special case for half-floats */
   if (mem_type.width == 16 && mem_type.floating) {
      assert(blend_type.width == 32 && blend_type.floating);
      lp_build_conv_auto(gallivm, src_type, &dst_type, dst, num_srcs, dst);
      is_arith = false;
   }

   if (!is_arith) {
      return;
   }

   src_type.width = blend_type.width * blend_type.length;
   blend_type.length *= pixels;
   src_type.length *= pixels / (src_type.length / mem_type.length);

   for (i = 0; i < num_srcs; ++i) {
      LLVMValueRef chans[4];
      LLVMValueRef res = NULL;

      dst[i] = LLVMBuildZExt(builder, src[i], lp_build_vec_type(gallivm, src_type), "");

      for (j = 0; j < src_fmt->nr_channels; ++j) {
         unsigned mask = 0;
         unsigned sa = src_fmt->channel[j].shift;
#ifdef PIPE_ARCH_LITTLE_ENDIAN
         unsigned from_lsb = j;
#else
         unsigned from_lsb = src_fmt->nr_channels - j - 1;
#endif

         mask = (1 << src_fmt->channel[j].size) - 1;

         /* Extract bits from source */
         chans[j] = LLVMBuildLShr(builder,
                                  dst[i],
                                  lp_build_const_int_vec(gallivm, src_type, sa),
                                  "");

         chans[j] = LLVMBuildAnd(builder,
                                 chans[j],
                                 lp_build_const_int_vec(gallivm, src_type, mask),
                                 "");

         /* Scale bits */
         if (src_type.norm) {
            chans[j] = scale_bits(gallivm, src_fmt->channel[j].size,
                                  blend_type.width, chans[j], src_type);
         }

         /* Insert bits into correct position */
         chans[j] = LLVMBuildShl(builder,
                                 chans[j],
                                 lp_build_const_int_vec(gallivm, src_type, from_lsb * blend_type.width),
                                 "");

         if (j == 0) {
            res = chans[j];
         } else {
            res = LLVMBuildOr(builder, res, chans[j], "");
         }
      }

      dst[i] = LLVMBuildBitCast(builder, res, lp_build_vec_type(gallivm, blend_type), "");
   }
}


/**
 * Convert from blending format to memory format
 *
 * e.g. GL_R3G3B2 is 3 bytes for blending but 1 byte in memory
 */
static void
convert_from_blend_type(struct gallivm_state *gallivm,
                        unsigned block_size,
                        const struct util_format_description *src_fmt,
                        struct lp_type src_type,
                        struct lp_type dst_type,
                        LLVMValueRef* src, // and dst
                        unsigned num_srcs)
{
   LLVMValueRef* dst = src;
   unsigned i, j, k;
   struct lp_type mem_type;
   struct lp_type blend_type;
   LLVMBuilderRef builder = gallivm->builder;
   unsigned pixels = block_size / num_srcs;
   bool is_arith;

   /*
    * full custom path for packed floats and srgb formats - none of the later
    * functions would do anything useful, and given the lp_type representation they
    * can't be fixed. Should really have some SoA blend path for these kind of
    * formats rather than hacking them in here.
    */
   if (format_expands_to_float_soa(src_fmt)) {
      /*
       * This is pretty suboptimal for this case blending in SoA would be much
       * better - we need to transpose the AoS values back to SoA values for
       * conversion/packing.
       */
      assert(src_type.floating);
      assert(src_type.width == 32);
      assert(src_type.length % 4 == 0);
      assert(dst_type.width == 32 || dst_type.width == 16);

      for (i = 0; i < num_srcs / 4; i++) {
         LLVMValueRef tmpsoa[4], tmpdst;
         lp_build_transpose_aos(gallivm, src_type, &src[i * 4], tmpsoa);
         /* really really need SoA here */

         if (src_fmt->format == PIPE_FORMAT_R11G11B10_FLOAT) {
            tmpdst = lp_build_float_to_r11g11b10(gallivm, tmpsoa);
         }
         else {
            tmpdst = lp_build_float_to_srgb_packed(gallivm, src_fmt,
                                                   src_type, tmpsoa);
         }

         if (src_type.length == 8) {
            LLVMValueRef tmpaos, shuffles[8];
            unsigned j;
            /*
             * for 8-wide aos transpose has given us wrong order not matching
             * output order. HMPF. Also need to split the output values manually.
             */
            for (j = 0; j < 4; j++) {
               shuffles[j * 2] = lp_build_const_int32(gallivm, j);
               shuffles[j * 2 + 1] = lp_build_const_int32(gallivm, j + 4);
            }
            tmpaos = LLVMBuildShuffleVector(builder, tmpdst, tmpdst,
                                            LLVMConstVector(shuffles, 8), "");
            src[i * 2] = lp_build_extract_range(gallivm, tmpaos, 0, 4);
            src[i * 2 + 1] = lp_build_extract_range(gallivm, tmpaos, 4, 4);
         }
         else {
            src[i] = tmpdst;
         }
      }
      if (dst_type.width == 16) {
         struct lp_type type16x8 = dst_type;
         struct lp_type type32x4 = dst_type;
         LLVMTypeRef ltype16x4, ltypei64, ltypei128;
         unsigned num_fetch = src_type.length == 8 ? num_srcs / 2 : num_srcs / 4;
         type16x8.length = 8;
         type32x4.width = 32;
         ltypei128 = LLVMIntTypeInContext(gallivm->context, 128);
         ltypei64 = LLVMIntTypeInContext(gallivm->context, 64);
         ltype16x4 = lp_build_vec_type(gallivm, dst_type);
         /* We could do vector truncation but it doesn't generate very good code */
         for (i = 0; i < num_fetch; i++) {
            src[i] = lp_build_pack2(gallivm, type32x4, type16x8,
                                    src[i], lp_build_zero(gallivm, type32x4));
            src[i] = LLVMBuildBitCast(builder, src[i], ltypei128, "");
            src[i] = LLVMBuildTrunc(builder, src[i], ltypei64, "");
            src[i] = LLVMBuildBitCast(builder, src[i], ltype16x4, "");
         }
      }
      return;
   }

   lp_mem_type_from_format_desc(src_fmt, &mem_type);
   lp_blend_type_from_format_desc(src_fmt, &blend_type);

   is_arith = (blend_type.length * blend_type.width != mem_type.width * mem_type.length);

   /* Special case for half-floats */
   if (mem_type.width == 16 && mem_type.floating) {
      int length = dst_type.length;
      assert(blend_type.width == 32 && blend_type.floating);

      dst_type.length = src_type.length;

      lp_build_conv_auto(gallivm, src_type, &dst_type, dst, num_srcs, dst);

      dst_type.length = length;
      is_arith = false;
   }

   /* Remove any padding */
   if (!is_arith && (src_type.length % mem_type.length)) {
      src_type.length -= (src_type.length % mem_type.length);

      for (i = 0; i < num_srcs; ++i) {
         dst[i] = lp_build_extract_range(gallivm, dst[i], 0, src_type.length);
      }
   }

   /* No bit arithmetic to do */
   if (!is_arith) {
      return;
   }

   src_type.length = pixels;
   src_type.width = blend_type.length * blend_type.width;
   dst_type.length = pixels;

   for (i = 0; i < num_srcs; ++i) {
      LLVMValueRef chans[4];
      LLVMValueRef res = NULL;

      dst[i] = LLVMBuildBitCast(builder, src[i], lp_build_vec_type(gallivm, src_type), "");

      for (j = 0; j < src_fmt->nr_channels; ++j) {
         unsigned mask = 0;
         unsigned sa = src_fmt->channel[j].shift;
#ifdef PIPE_ARCH_LITTLE_ENDIAN
         unsigned from_lsb = j;
#else
         unsigned from_lsb = src_fmt->nr_channels - j - 1;
#endif

         assert(blend_type.width > src_fmt->channel[j].size);

         for (k = 0; k < blend_type.width; ++k) {
            mask |= 1 << k;
         }

         /* Extract bits */
         chans[j] = LLVMBuildLShr(builder,
                                  dst[i],
                                  lp_build_const_int_vec(gallivm, src_type,
                                                         from_lsb * blend_type.width),
                                  "");

         chans[j] = LLVMBuildAnd(builder,
                                 chans[j],
                                 lp_build_const_int_vec(gallivm, src_type, mask),
                                 "");

         /* Scale down bits */
         if (src_type.norm) {
            chans[j] = scale_bits(gallivm, blend_type.width,
                                  src_fmt->channel[j].size, chans[j], src_type);
         }

         /* Insert bits */
         chans[j] = LLVMBuildShl(builder,
                                 chans[j],
                                 lp_build_const_int_vec(gallivm, src_type, sa),
                                 "");

         sa += src_fmt->channel[j].size;

         if (j == 0) {
            res = chans[j];
         } else {
            res = LLVMBuildOr(builder, res, chans[j], "");
         }
      }

      assert (dst_type.width != 24);

      dst[i] = LLVMBuildTrunc(builder, res, lp_build_vec_type(gallivm, dst_type), "");
   }
}


/**
 * Convert alpha to same blend type as src
 */
static void
convert_alpha(struct gallivm_state *gallivm,
              struct lp_type row_type,
              struct lp_type alpha_type,
              const unsigned block_size,
              const unsigned block_height,
              const unsigned src_count,
              const unsigned dst_channels,
              const bool pad_inline,
              LLVMValueRef* src_alpha)
{
   LLVMBuilderRef builder = gallivm->builder;
   unsigned i, j;
   unsigned length = row_type.length;
   row_type.length = alpha_type.length;

   /* Twiddle the alpha to match pixels */
   lp_bld_quad_twiddle(gallivm, alpha_type, src_alpha, block_height, src_alpha);

   /*
    * TODO this should use single lp_build_conv call for
    * src_count == 1 && dst_channels == 1 case (dropping the concat below)
    */
   for (i = 0; i < block_height; ++i) {
      lp_build_conv(gallivm, alpha_type, row_type, &src_alpha[i], 1, &src_alpha[i], 1);
   }

   alpha_type = row_type;
   row_type.length = length;

   /* If only one channel we can only need the single alpha value per pixel */
   if (src_count == 1 && dst_channels == 1) {

      lp_build_concat_n(gallivm, alpha_type, src_alpha, block_height, src_alpha, src_count);
   } else {
      /* If there are more srcs than rows then we need to split alpha up */
      if (src_count > block_height) {
         for (i = src_count; i > 0; --i) {
            unsigned pixels = block_size / src_count;
            unsigned idx = i - 1;

            src_alpha[idx] = lp_build_extract_range(gallivm, src_alpha[(idx * pixels) / 4],
                                                    (idx * pixels) % 4, pixels);
         }
      }

      /* If there is a src for each pixel broadcast the alpha across whole row */
      if (src_count == block_size) {
         for (i = 0; i < src_count; ++i) {
            src_alpha[i] = lp_build_broadcast(gallivm,
                              lp_build_vec_type(gallivm, row_type), src_alpha[i]);
         }
      } else {
         unsigned pixels = block_size / src_count;
         unsigned channels = pad_inline ? TGSI_NUM_CHANNELS : dst_channels;
         unsigned alpha_span = 1;
         LLVMValueRef shuffles[LP_MAX_VECTOR_LENGTH];

         /* Check if we need 2 src_alphas for our shuffles */
         if (pixels > alpha_type.length) {
            alpha_span = 2;
         }

         /* Broadcast alpha across all channels, e.g. a1a2 to a1a1a1a1a2a2a2a2 */
         for (j = 0; j < row_type.length; ++j) {
            if (j < pixels * channels) {
               shuffles[j] = lp_build_const_int32(gallivm, j / channels);
            } else {
               shuffles[j] = LLVMGetUndef(LLVMInt32TypeInContext(gallivm->context));
            }
         }

         for (i = 0; i < src_count; ++i) {
            unsigned idx1 = i, idx2 = i;

            if (alpha_span > 1){
               idx1 *= alpha_span;
               idx2 = idx1 + 1;
            }

            src_alpha[i] = LLVMBuildShuffleVector(builder,
                                                  src_alpha[idx1],
                                                  src_alpha[idx2],
                                                  LLVMConstVector(shuffles, row_type.length),
                                                  "");
         }
      }
   }
}


/**
 * Generates the blend function for unswizzled colour buffers
 * Also generates the read & write from colour buffer
 */
static void
generate_unswizzled_blend(struct gallivm_state *gallivm,
                          unsigned rt,
                          struct lp_fragment_shader_variant *variant,
                          enum pipe_format out_format,
                          unsigned int num_fs,
                          struct lp_type fs_type,
                          LLVMValueRef* fs_mask,
                          LLVMValueRef fs_out_color[PIPE_MAX_COLOR_BUFS][TGSI_NUM_CHANNELS][4],
                          LLVMValueRef context_ptr,
                          LLVMValueRef color_ptr,
                          LLVMValueRef stride,
                          unsigned partial_mask,
                          boolean do_branch)
{
   const unsigned alpha_channel = 3;
   const unsigned block_width = LP_RASTER_BLOCK_SIZE;
   const unsigned block_height = LP_RASTER_BLOCK_SIZE;
   const unsigned block_size = block_width * block_height;
   const unsigned lp_integer_vector_width = 128;

   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef fs_src[4][TGSI_NUM_CHANNELS];
   LLVMValueRef fs_src1[4][TGSI_NUM_CHANNELS];
   LLVMValueRef src_alpha[4 * 4];
   LLVMValueRef src1_alpha[4 * 4] = { NULL };
   LLVMValueRef src_mask[4 * 4];
   LLVMValueRef src[4 * 4];
   LLVMValueRef src1[4 * 4];
   LLVMValueRef dst[4 * 4];
   LLVMValueRef blend_color;
   LLVMValueRef blend_alpha;
   LLVMValueRef i32_zero;
   LLVMValueRef check_mask;
   LLVMValueRef undef_src_val;

   struct lp_build_mask_context mask_ctx;
   struct lp_type mask_type;
   struct lp_type blend_type;
   struct lp_type row_type;
   struct lp_type dst_type;
   struct lp_type ls_type;

   unsigned char swizzle[TGSI_NUM_CHANNELS];
   unsigned vector_width;
   unsigned src_channels = TGSI_NUM_CHANNELS;
   unsigned dst_channels;
   unsigned dst_count;
   unsigned src_count;
   unsigned i, j;

   const struct util_format_description* out_format_desc = util_format_description(out_format);

   unsigned dst_alignment;

   bool pad_inline = is_arithmetic_format(out_format_desc);
   bool has_alpha = false;
   const boolean dual_source_blend = variant->key.blend.rt[0].blend_enable &&
                                     util_blend_state_is_dual(&variant->key.blend, 0);

   const boolean is_1d = variant->key.resource_1d;
   boolean twiddle_after_convert = FALSE;
   unsigned num_fullblock_fs = is_1d ? 2 * num_fs : num_fs;
   LLVMValueRef fpstate = 0;

   /* Get type from output format */
   lp_blend_type_from_format_desc(out_format_desc, &row_type);
   lp_mem_type_from_format_desc(out_format_desc, &dst_type);

   /*
    * Technically this code should go into lp_build_smallfloat_to_float
    * and lp_build_float_to_smallfloat but due to the
    * http://llvm.org/bugs/show_bug.cgi?id=6393
    * llvm reorders the mxcsr intrinsics in a way that breaks the code.
    * So the ordering is important here and there shouldn't be any
    * llvm ir instrunctions in this function before
    * this, otherwise half-float format conversions won't work
    * (again due to llvm bug #6393).
    */
   if (have_smallfloat_format(dst_type, out_format)) {
      /* We need to make sure that denorms are ok for half float
         conversions */
      fpstate = lp_build_fpstate_get(gallivm);
      lp_build_fpstate_set_denorms_zero(gallivm, FALSE);
   }

   mask_type = lp_int32_vec4_type();
   mask_type.length = fs_type.length;

   for (i = num_fs; i < num_fullblock_fs; i++) {
      fs_mask[i] = lp_build_zero(gallivm, mask_type);
   }

   /* Do not bother executing code when mask is empty.. */
   if (do_branch) {
      check_mask = LLVMConstNull(lp_build_int_vec_type(gallivm, mask_type));

      for (i = 0; i < num_fullblock_fs; ++i) {
         check_mask = LLVMBuildOr(builder, check_mask, fs_mask[i], "");
      }

      lp_build_mask_begin(&mask_ctx, gallivm, mask_type, check_mask);
      lp_build_mask_check(&mask_ctx);
   }

   partial_mask |= !variant->opaque;
   i32_zero = lp_build_const_int32(gallivm, 0);

   undef_src_val = lp_build_undef(gallivm, fs_type);

   row_type.length = fs_type.length;
   vector_width    = dst_type.floating ? lp_native_vector_width : lp_integer_vector_width;

   /* Compute correct swizzle and count channels */
   memset(swizzle, LP_BLD_SWIZZLE_DONTCARE, TGSI_NUM_CHANNELS);
   dst_channels = 0;

   for (i = 0; i < TGSI_NUM_CHANNELS; ++i) {
      /* Ensure channel is used */
      if (out_format_desc->swizzle[i] >= TGSI_NUM_CHANNELS) {
         continue;
      }

      /* Ensure not already written to (happens in case with GL_ALPHA) */
      if (swizzle[out_format_desc->swizzle[i]] < TGSI_NUM_CHANNELS) {
         continue;
      }

      /* Ensure we havn't already found all channels */
      if (dst_channels >= out_format_desc->nr_channels) {
         continue;
      }

      swizzle[out_format_desc->swizzle[i]] = i;
      ++dst_channels;

      if (i == alpha_channel) {
         has_alpha = true;
      }
   }

   if (format_expands_to_float_soa(out_format_desc)) {
      /*
       * the code above can't work for layout_other
       * for srgb it would sort of work but we short-circuit swizzles, etc.
       * as that is done as part of unpack / pack.
       */
      dst_channels = 4; /* HACK: this is fake 4 really but need it due to transpose stuff later */
      has_alpha = true;
      swizzle[0] = 0;
      swizzle[1] = 1;
      swizzle[2] = 2;
      swizzle[3] = 3;
      pad_inline = true; /* HACK: prevent rgbxrgbx->rgbrgbxx conversion later */
   }

   /* If 3 channels then pad to include alpha for 4 element transpose */
   if (dst_channels == 3) {
      assert (!has_alpha);
      for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
         if (swizzle[i] > TGSI_NUM_CHANNELS)
            swizzle[i] = 3;
      }
      if (out_format_desc->nr_channels == 4) {
         dst_channels = 4;
         /*
          * We use alpha from the color conversion, not separate one.
          * We had to include it for transpose, hence it will get converted
          * too (albeit when doing transpose after conversion, that would
          * no longer be the case necessarily).
          * (It works only with 4 channel dsts, e.g. rgbx formats, because
          * otherwise we really have padding, not alpha, included.)
          */
         has_alpha = true;
      }
   }

   /*
    * Load shader output
    */
   for (i = 0; i < num_fullblock_fs; ++i) {
      /* Always load alpha for use in blending */
      LLVMValueRef alpha;
      if (i < num_fs) {
         alpha = LLVMBuildLoad(builder, fs_out_color[rt][alpha_channel][i], "");
      }
      else {
         alpha = undef_src_val;
      }

      /* Load each channel */
      for (j = 0; j < dst_channels; ++j) {
         assert(swizzle[j] < 4);
         if (i < num_fs) {
            fs_src[i][j] = LLVMBuildLoad(builder, fs_out_color[rt][swizzle[j]][i], "");
         }
         else {
            fs_src[i][j] = undef_src_val;
         }
      }

      /* If 3 channels then pad to include alpha for 4 element transpose */
      /*
       * XXX If we include that here maybe could actually use it instead of
       * separate alpha for blending?
       * (Difficult though we actually convert pad channels, not alpha.)
       */
      if (dst_channels == 3 && !has_alpha) {
         fs_src[i][3] = alpha;
      }

      /* We split the row_mask and row_alpha as we want 128bit interleave */
      if (fs_type.length == 8) {
         src_mask[i*2 + 0]  = lp_build_extract_range(gallivm, fs_mask[i],
                                                     0, src_channels);
         src_mask[i*2 + 1]  = lp_build_extract_range(gallivm, fs_mask[i],
                                                     src_channels, src_channels);

         src_alpha[i*2 + 0] = lp_build_extract_range(gallivm, alpha, 0, src_channels);
         src_alpha[i*2 + 1] = lp_build_extract_range(gallivm, alpha,
                                                     src_channels, src_channels);
      } else {
         src_mask[i] = fs_mask[i];
         src_alpha[i] = alpha;
      }
   }
   if (dual_source_blend) {
      /* same as above except different src/dst, skip masks and comments... */
      for (i = 0; i < num_fullblock_fs; ++i) {
         LLVMValueRef alpha;
         if (i < num_fs) {
            alpha = LLVMBuildLoad(builder, fs_out_color[1][alpha_channel][i], "");
         }
         else {
            alpha = undef_src_val;
         }

         for (j = 0; j < dst_channels; ++j) {
            assert(swizzle[j] < 4);
            if (i < num_fs) {
               fs_src1[i][j] = LLVMBuildLoad(builder, fs_out_color[1][swizzle[j]][i], "");
            }
            else {
               fs_src1[i][j] = undef_src_val;
            }
         }
         if (dst_channels == 3 && !has_alpha) {
            fs_src1[i][3] = alpha;
         }
         if (fs_type.length == 8) {
            src1_alpha[i*2 + 0] = lp_build_extract_range(gallivm, alpha, 0, src_channels);
            src1_alpha[i*2 + 1] = lp_build_extract_range(gallivm, alpha,
                                                         src_channels, src_channels);
         } else {
            src1_alpha[i] = alpha;
         }
      }
   }

   if (util_format_is_pure_integer(out_format)) {
      /*
       * In this case fs_type was really ints or uints disguised as floats,
       * fix that up now.
       */
      fs_type.floating = 0;
      fs_type.sign = dst_type.sign;
      for (i = 0; i < num_fullblock_fs; ++i) {
         for (j = 0; j < dst_channels; ++j) {
            fs_src[i][j] = LLVMBuildBitCast(builder, fs_src[i][j],
                                            lp_build_vec_type(gallivm, fs_type), "");
         }
         if (dst_channels == 3 && !has_alpha) {
            fs_src[i][3] = LLVMBuildBitCast(builder, fs_src[i][3],
                                            lp_build_vec_type(gallivm, fs_type), "");
         }
      }
   }

   /*
    * We actually should generally do conversion first (for non-1d cases)
    * when the blend format is 8 or 16 bits. The reason is obvious,
    * there's 2 or 4 times less vectors to deal with for the interleave...
    * Albeit for the AVX (not AVX2) case there's no benefit with 16 bit
    * vectors (as it can do 32bit unpack with 256bit vectors, but 8/16bit
    * unpack only with 128bit vectors).
    * Note: for 16bit sizes really need matching pack conversion code
    */
   if (!is_1d && dst_channels != 3 && dst_type.width == 8) {
      twiddle_after_convert = TRUE;
   }

   /*
    * Pixel twiddle from fragment shader order to memory order
    */
   if (!twiddle_after_convert) {
      src_count = generate_fs_twiddle(gallivm, fs_type, num_fullblock_fs,
                                      dst_channels, fs_src, src, pad_inline);
      if (dual_source_blend) {
         generate_fs_twiddle(gallivm, fs_type, num_fullblock_fs, dst_channels,
                             fs_src1, src1, pad_inline);
      }
   } else {
      src_count = num_fullblock_fs * dst_channels;
      /*
       * We reorder things a bit here, so the cases for 4-wide and 8-wide
       * (AVX) turn out the same later when untwiddling/transpose (albeit
       * for true AVX2 path untwiddle needs to be different).
       * For now just order by colors first (so we can use unpack later).
       */
      for (j = 0; j < num_fullblock_fs; j++) {
         for (i = 0; i < dst_channels; i++) {
            src[i*num_fullblock_fs + j] = fs_src[j][i];
            if (dual_source_blend) {
               src1[i*num_fullblock_fs + j] = fs_src1[j][i];
            }
         }
      }
   }

   src_channels = dst_channels < 3 ? dst_channels : 4;
   if (src_count != num_fullblock_fs * src_channels) {
      unsigned ds = src_count / (num_fullblock_fs * src_channels);
      row_type.length /= ds;
      fs_type.length = row_type.length;
   }

   blend_type = row_type;
   mask_type.length = 4;

   /* Convert src to row_type */
   if (dual_source_blend) {
      struct lp_type old_row_type = row_type;
      lp_build_conv_auto(gallivm, fs_type, &row_type, src, src_count, src);
      src_count = lp_build_conv_auto(gallivm, fs_type, &old_row_type, src1, src_count, src1);
   }
   else {
      src_count = lp_build_conv_auto(gallivm, fs_type, &row_type, src, src_count, src);
   }

   /* If the rows are not an SSE vector, combine them to become SSE size! */
   if ((row_type.width * row_type.length) % 128) {
      unsigned bits = row_type.width * row_type.length;
      unsigned combined;

      assert(src_count >= (vector_width / bits));

      dst_count = src_count / (vector_width / bits);

      combined = lp_build_concat_n(gallivm, row_type, src, src_count, src, dst_count);
      if (dual_source_blend) {
         lp_build_concat_n(gallivm, row_type, src1, src_count, src1, dst_count);
      }

      row_type.length *= combined;
      src_count /= combined;

      bits = row_type.width * row_type.length;
      assert(bits == 128 || bits == 256);
   }

   if (twiddle_after_convert) {
      fs_twiddle_transpose(gallivm, row_type, src, src_count, src);
      if (dual_source_blend) {
         fs_twiddle_transpose(gallivm, row_type, src1, src_count, src1);
      }
   }

   /*
    * Blend Colour conversion
    */
   blend_color = lp_jit_context_f_blend_color(gallivm, context_ptr);
   blend_color = LLVMBuildPointerCast(builder, blend_color,
                    LLVMPointerType(lp_build_vec_type(gallivm, fs_type), 0), "");
   blend_color = LLVMBuildLoad(builder, LLVMBuildGEP(builder, blend_color,
                               &i32_zero, 1, ""), "");

   /* Convert */
   lp_build_conv(gallivm, fs_type, blend_type, &blend_color, 1, &blend_color, 1);

   if (out_format_desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
      /*
       * since blending is done with floats, there was no conversion.
       * However, the rules according to fixed point renderbuffers still
       * apply, that is we must clamp inputs to 0.0/1.0.
       * (This would apply to separate alpha conversion too but we currently
       * force has_alpha to be true.)
       * TODO: should skip this with "fake" blend, since post-blend conversion
       * will clamp anyway.
       * TODO: could also skip this if fragment color clamping is enabled. We
       * don't support it natively so it gets baked into the shader however, so
       * can't really tell here.
       */
      struct lp_build_context f32_bld;
      assert(row_type.floating);
      lp_build_context_init(&f32_bld, gallivm, row_type);
      for (i = 0; i < src_count; i++) {
         src[i] = lp_build_clamp_zero_one_nanzero(&f32_bld, src[i]);
      }
      if (dual_source_blend) {
         for (i = 0; i < src_count; i++) {
            src1[i] = lp_build_clamp_zero_one_nanzero(&f32_bld, src1[i]);
         }
      }
      /* probably can't be different than row_type but better safe than sorry... */
      lp_build_context_init(&f32_bld, gallivm, blend_type);
      blend_color = lp_build_clamp(&f32_bld, blend_color, f32_bld.zero, f32_bld.one);
   }

   /* Extract alpha */
   blend_alpha = lp_build_extract_broadcast(gallivm, blend_type, row_type, blend_color, lp_build_const_int32(gallivm, 3));

   /* Swizzle to appropriate channels, e.g. from RGBA to BGRA BGRA */
   pad_inline &= (dst_channels * (block_size / src_count) * row_type.width) != vector_width;
   if (pad_inline) {
      /* Use all 4 channels e.g. from RGBA RGBA to RGxx RGxx */
      blend_color = lp_build_swizzle_aos_n(gallivm, blend_color, swizzle, TGSI_NUM_CHANNELS, row_type.length);
   } else {
      /* Only use dst_channels e.g. RGBA RGBA to RG RG xxxx */
      blend_color = lp_build_swizzle_aos_n(gallivm, blend_color, swizzle, dst_channels, row_type.length);
   }

   /*
    * Mask conversion
    */
   lp_bld_quad_twiddle(gallivm, mask_type, &src_mask[0], block_height, &src_mask[0]);

   if (src_count < block_height) {
      lp_build_concat_n(gallivm, mask_type, src_mask, 4, src_mask, src_count);
   } else if (src_count > block_height) {
      for (i = src_count; i > 0; --i) {
         unsigned pixels = block_size / src_count;
         unsigned idx = i - 1;

         src_mask[idx] = lp_build_extract_range(gallivm, src_mask[(idx * pixels) / 4],
                                                (idx * pixels) % 4, pixels);
      }
   }

   assert(mask_type.width == 32);

   for (i = 0; i < src_count; ++i) {
      unsigned pixels = block_size / src_count;
      unsigned pixel_width = row_type.width * dst_channels;

      if (pixel_width == 24) {
         mask_type.width = 8;
         mask_type.length = vector_width / mask_type.width;
      } else {
         mask_type.length = pixels;
         mask_type.width = row_type.width * dst_channels;

         /*
          * If mask_type width is smaller than 32bit, this doesn't quite
          * generate the most efficient code (could use some pack).
          */
         src_mask[i] = LLVMBuildIntCast(builder, src_mask[i],
                                        lp_build_int_vec_type(gallivm, mask_type), "");

         mask_type.length *= dst_channels;
         mask_type.width /= dst_channels;
      }

      src_mask[i] = LLVMBuildBitCast(builder, src_mask[i],
                                     lp_build_int_vec_type(gallivm, mask_type), "");
      src_mask[i] = lp_build_pad_vector(gallivm, src_mask[i], row_type.length);
   }

   /*
    * Alpha conversion
    */
   if (!has_alpha) {
      struct lp_type alpha_type = fs_type;
      alpha_type.length = 4;
      convert_alpha(gallivm, row_type, alpha_type,
                    block_size, block_height,
                    src_count, dst_channels,
                    pad_inline, src_alpha);
      if (dual_source_blend) {
         convert_alpha(gallivm, row_type, alpha_type,
                       block_size, block_height,
                       src_count, dst_channels,
                       pad_inline, src1_alpha);
      }
   }


   /*
    * Load dst from memory
    */
   if (src_count < block_height) {
      dst_count = block_height;
   } else {
      dst_count = src_count;
   }

   dst_type.length *= block_size / dst_count;

   if (format_expands_to_float_soa(out_format_desc)) {
      /*
       * we need multiple values at once for the conversion, so can as well
       * load them vectorized here too instead of concatenating later.
       * (Still need concatenation later for 8-wide vectors).
       */
      dst_count = block_height;
      dst_type.length = block_width;
   }

   /*
    * Compute the alignment of the destination pointer in bytes
    * We fetch 1-4 pixels, if the format has pot alignment then those fetches
    * are always aligned by MIN2(16, fetch_width) except for buffers (not
    * 1d tex but can't distinguish here) so need to stick with per-pixel
    * alignment in this case.
    */
   if (is_1d) {
      dst_alignment = (out_format_desc->block.bits + 7)/(out_format_desc->block.width * 8);
   }
   else {
      dst_alignment = dst_type.length * dst_type.width / 8;
   }
   /* Force power-of-two alignment by extracting only the least-significant-bit */
   dst_alignment = 1 << (ffs(dst_alignment) - 1);
   /*
    * Resource base and stride pointers are aligned to 16 bytes, so that's
    * the maximum alignment we can guarantee
    */
   dst_alignment = MIN2(16, dst_alignment);

   ls_type = dst_type;

   if (dst_count > src_count) {
      if ((dst_type.width == 8 || dst_type.width == 16) &&
          util_is_power_of_two(dst_type.length) &&
          dst_type.length * dst_type.width < 128) {
         /*
          * Never try to load values as 4xi8 which we will then
          * concatenate to larger vectors. This gives llvm a real
          * headache (the problem is the type legalizer (?) will
          * try to load that as 4xi8 zext to 4xi32 to fill the vector,
          * then the shuffles to concatenate are more or less impossible
          * - llvm is easily capable of generating a sequence of 32
          * pextrb/pinsrb instructions for that. Albeit it appears to
          * be fixed in llvm 4.0. So, load and concatenate with 32bit
          * width to avoid the trouble (16bit seems not as bad, llvm
          * probably recognizes the load+shuffle as only one shuffle
          * is necessary, but we can do just the same anyway).
          */
         ls_type.length = dst_type.length * dst_type.width / 32;
         ls_type.width = 32;
      }
   }

   if (is_1d) {
      load_unswizzled_block(gallivm, color_ptr, stride, block_width, 1,
                            dst, ls_type, dst_count / 4, dst_alignment);
      for (i = dst_count / 4; i < dst_count; i++) {
         dst[i] = lp_build_undef(gallivm, ls_type);
      }

   }
   else {
      load_unswizzled_block(gallivm, color_ptr, stride, block_width, block_height,
                            dst, ls_type, dst_count, dst_alignment);
   }


   /*
    * Convert from dst/output format to src/blending format.
    *
    * This is necessary as we can only read 1 row from memory at a time,
    * so the minimum dst_count will ever be at this point is 4.
    *
    * With, for example, R8 format you can have all 16 pixels in a 128 bit vector,
    * this will take the 4 dsts and combine them into 1 src so we can perform blending
    * on all 16 pixels in that single vector at once.
    */
   if (dst_count > src_count) {
      if (ls_type.length != dst_type.length && ls_type.length == 1) {
         LLVMTypeRef elem_type = lp_build_elem_type(gallivm, ls_type);
         LLVMTypeRef ls_vec_type = LLVMVectorType(elem_type, 1);
         for (i = 0; i < dst_count; i++) {
            dst[i] = LLVMBuildBitCast(builder, dst[i], ls_vec_type, "");
         }
      }

      lp_build_concat_n(gallivm, ls_type, dst, 4, dst, src_count);

      if (ls_type.length != dst_type.length) {
         struct lp_type tmp_type = dst_type;
         tmp_type.length = dst_type.length * 4 / src_count;
         for (i = 0; i < src_count; i++) {
            dst[i] = LLVMBuildBitCast(builder, dst[i],
                                      lp_build_vec_type(gallivm, tmp_type), "");
         }
      }
   }

   /*
    * Blending
    */
   /* XXX this is broken for RGB8 formats -
    * they get expanded from 12 to 16 elements (to include alpha)
    * by convert_to_blend_type then reduced to 15 instead of 12
    * by convert_from_blend_type (a simple fix though breaks A8...).
    * R16G16B16 also crashes differently however something going wrong
    * inside llvm handling npot vector sizes seemingly.
    * It seems some cleanup could be done here (like skipping conversion/blend
    * when not needed).
    */
   convert_to_blend_type(gallivm, block_size, out_format_desc, dst_type,
                         row_type, dst, src_count);

   /*
    * FIXME: Really should get logic ops / masks out of generic blend / row
    * format. Logic ops will definitely not work on the blend float format
    * used for SRGB here and I think OpenGL expects this to work as expected
    * (that is incoming values converted to srgb then logic op applied).
    */
   for (i = 0; i < src_count; ++i) {
      dst[i] = lp_build_blend_aos(gallivm,
                                  &variant->key.blend,
                                  out_format,
                                  row_type,
                                  rt,
                                  src[i],
                                  has_alpha ? NULL : src_alpha[i],
                                  src1[i],
                                  has_alpha ? NULL : src1_alpha[i],
                                  dst[i],
                                  partial_mask ? src_mask[i] : NULL,
                                  blend_color,
                                  has_alpha ? NULL : blend_alpha,
                                  swizzle,
                                  pad_inline ? 4 : dst_channels);
   }

   convert_from_blend_type(gallivm, block_size, out_format_desc,
                           row_type, dst_type, dst, src_count);

   /* Split the blend rows back to memory rows */
   if (dst_count > src_count) {
      row_type.length = dst_type.length * (dst_count / src_count);

      if (src_count == 1) {
         dst[1] = lp_build_extract_range(gallivm, dst[0], row_type.length / 2, row_type.length / 2);
         dst[0] = lp_build_extract_range(gallivm, dst[0], 0, row_type.length / 2);

         row_type.length /= 2;
         src_count *= 2;
      }

      dst[3] = lp_build_extract_range(gallivm, dst[1], row_type.length / 2, row_type.length / 2);
      dst[2] = lp_build_extract_range(gallivm, dst[1], 0, row_type.length / 2);
      dst[1] = lp_build_extract_range(gallivm, dst[0], row_type.length / 2, row_type.length / 2);
      dst[0] = lp_build_extract_range(gallivm, dst[0], 0, row_type.length / 2);

      row_type.length /= 2;
      src_count *= 2;
   }

   /*
    * Store blend result to memory
    */
   if (is_1d) {
      store_unswizzled_block(gallivm, color_ptr, stride, block_width, 1,
                             dst, dst_type, dst_count / 4, dst_alignment);
   }
   else {
      store_unswizzled_block(gallivm, color_ptr, stride, block_width, block_height,
                             dst, dst_type, dst_count, dst_alignment);
   }

   if (have_smallfloat_format(dst_type, out_format)) {
      lp_build_fpstate_set(gallivm, fpstate);
   }

   if (do_branch) {
      lp_build_mask_end(&mask_ctx);
   }
}


/**
 * Generate the runtime callable function for the whole fragment pipeline.
 * Note that the function which we generate operates on a block of 16
 * pixels at at time.  The block contains 2x2 quads.  Each quad contains
 * 2x2 pixels.
 */
static void
generate_fragment(struct llvmpipe_context *lp,
                  struct lp_fragment_shader *shader,
                  struct lp_fragment_shader_variant *variant,
                  unsigned partial_mask)
{
   struct gallivm_state *gallivm = variant->gallivm;
   const struct lp_fragment_shader_variant_key *key = &variant->key;
   struct lp_shader_input inputs[PIPE_MAX_SHADER_INPUTS];
   char func_name[64];
   struct lp_type fs_type;
   struct lp_type blend_type;
   LLVMTypeRef fs_elem_type;
   LLVMTypeRef blend_vec_type;
   LLVMTypeRef arg_types[13];
   LLVMTypeRef func_type;
   LLVMTypeRef int32_type = LLVMInt32TypeInContext(gallivm->context);
   LLVMTypeRef int8_type = LLVMInt8TypeInContext(gallivm->context);
   LLVMValueRef context_ptr;
   LLVMValueRef x;
   LLVMValueRef y;
   LLVMValueRef a0_ptr;
   LLVMValueRef dadx_ptr;
   LLVMValueRef dady_ptr;
   LLVMValueRef color_ptr_ptr;
   LLVMValueRef stride_ptr;
   LLVMValueRef depth_ptr;
   LLVMValueRef depth_stride;
   LLVMValueRef mask_input;
   LLVMValueRef thread_data_ptr;
   LLVMBasicBlockRef block;
   LLVMBuilderRef builder;
   struct lp_build_sampler_soa *sampler;
   struct lp_build_interp_soa_context interp;
   LLVMValueRef fs_mask[16 / 4];
   LLVMValueRef fs_out_color[PIPE_MAX_COLOR_BUFS][TGSI_NUM_CHANNELS][16 / 4];
   LLVMValueRef function;
   LLVMValueRef facing;
   unsigned num_fs;
   unsigned i;
   unsigned chan;
   unsigned cbuf;
   boolean cbuf0_write_all;
   const boolean dual_source_blend = key->blend.rt[0].blend_enable &&
                                     util_blend_state_is_dual(&key->blend, 0);

   assert(lp_native_vector_width / 32 >= 4);

   /* Adjust color input interpolation according to flatshade state:
    */
   memcpy(inputs, shader->inputs, shader->info.base.num_inputs * sizeof inputs[0]);
   for (i = 0; i < shader->info.base.num_inputs; i++) {
      if (inputs[i].interp == LP_INTERP_COLOR) {
	 if (key->flatshade)
	    inputs[i].interp = LP_INTERP_CONSTANT;
	 else
	    inputs[i].interp = LP_INTERP_PERSPECTIVE;
      }
   }

   /* check if writes to cbuf[0] are to be copied to all cbufs */
   cbuf0_write_all =
     shader->info.base.properties[TGSI_PROPERTY_FS_COLOR0_WRITES_ALL_CBUFS];

   /* TODO: actually pick these based on the fs and color buffer
    * characteristics. */

   memset(&fs_type, 0, sizeof fs_type);
   fs_type.floating = TRUE;      /* floating point values */
   fs_type.sign = TRUE;          /* values are signed */
   fs_type.norm = FALSE;         /* values are not limited to [0,1] or [-1,1] */
   fs_type.width = 32;           /* 32-bit float */
   fs_type.length = MIN2(lp_native_vector_width / 32, 16); /* n*4 elements per vector */

   memset(&blend_type, 0, sizeof blend_type);
   blend_type.floating = FALSE; /* values are integers */
   blend_type.sign = FALSE;     /* values are unsigned */
   blend_type.norm = TRUE;      /* values are in [0,1] or [-1,1] */
   blend_type.width = 8;        /* 8-bit ubyte values */
   blend_type.length = 16;      /* 16 elements per vector */

   /* 
    * Generate the function prototype. Any change here must be reflected in
    * lp_jit.h's lp_jit_frag_func function pointer type, and vice-versa.
    */

   fs_elem_type = lp_build_elem_type(gallivm, fs_type);

   blend_vec_type = lp_build_vec_type(gallivm, blend_type);

   util_snprintf(func_name, sizeof(func_name), "fs%u_variant%u_%s",
                 shader->no, variant->no, partial_mask ? "partial" : "whole");

   arg_types[0] = variant->jit_context_ptr_type;       /* context */
   arg_types[1] = int32_type;                          /* x */
   arg_types[2] = int32_type;                          /* y */
   arg_types[3] = int32_type;                          /* facing */
   arg_types[4] = LLVMPointerType(fs_elem_type, 0);    /* a0 */
   arg_types[5] = LLVMPointerType(fs_elem_type, 0);    /* dadx */
   arg_types[6] = LLVMPointerType(fs_elem_type, 0);    /* dady */
   arg_types[7] = LLVMPointerType(LLVMPointerType(blend_vec_type, 0), 0);  /* color */
   arg_types[8] = LLVMPointerType(int8_type, 0);       /* depth */
   arg_types[9] = int32_type;                          /* mask_input */
   arg_types[10] = variant->jit_thread_data_ptr_type;  /* per thread data */
   arg_types[11] = LLVMPointerType(int32_type, 0);     /* stride */
   arg_types[12] = int32_type;                         /* depth_stride */

   func_type = LLVMFunctionType(LLVMVoidTypeInContext(gallivm->context),
                                arg_types, ARRAY_SIZE(arg_types), 0);

   function = LLVMAddFunction(gallivm->module, func_name, func_type);
   LLVMSetFunctionCallConv(function, LLVMCCallConv);

   variant->function[partial_mask] = function;

   /* XXX: need to propagate noalias down into color param now we are
    * passing a pointer-to-pointer?
    */
   for(i = 0; i < ARRAY_SIZE(arg_types); ++i)
      if(LLVMGetTypeKind(arg_types[i]) == LLVMPointerTypeKind)
         lp_add_function_attr(function, i + 1, LP_FUNC_ATTR_NOALIAS);

   context_ptr  = LLVMGetParam(function, 0);
   x            = LLVMGetParam(function, 1);
   y            = LLVMGetParam(function, 2);
   facing       = LLVMGetParam(function, 3);
   a0_ptr       = LLVMGetParam(function, 4);
   dadx_ptr     = LLVMGetParam(function, 5);
   dady_ptr     = LLVMGetParam(function, 6);
   color_ptr_ptr = LLVMGetParam(function, 7);
   depth_ptr    = LLVMGetParam(function, 8);
   mask_input   = LLVMGetParam(function, 9);
   thread_data_ptr  = LLVMGetParam(function, 10);
   stride_ptr   = LLVMGetParam(function, 11);
   depth_stride = LLVMGetParam(function, 12);

   lp_build_name(context_ptr, "context");
   lp_build_name(x, "x");
   lp_build_name(y, "y");
   lp_build_name(a0_ptr, "a0");
   lp_build_name(dadx_ptr, "dadx");
   lp_build_name(dady_ptr, "dady");
   lp_build_name(color_ptr_ptr, "color_ptr_ptr");
   lp_build_name(depth_ptr, "depth");
   lp_build_name(mask_input, "mask_input");
   lp_build_name(thread_data_ptr, "thread_data");
   lp_build_name(stride_ptr, "stride_ptr");
   lp_build_name(depth_stride, "depth_stride");

   /*
    * Function body
    */

   block = LLVMAppendBasicBlockInContext(gallivm->context, function, "entry");
   builder = gallivm->builder;
   assert(builder);
   LLVMPositionBuilderAtEnd(builder, block);

   /* code generated texture sampling */
   sampler = lp_llvm_sampler_soa_create(key->state);

   num_fs = 16 / fs_type.length; /* number of loops per 4x4 stamp */
   /* for 1d resources only run "upper half" of stamp */
   if (key->resource_1d)
      num_fs /= 2;

   {
      LLVMValueRef num_loop = lp_build_const_int32(gallivm, num_fs);
      LLVMTypeRef mask_type = lp_build_int_vec_type(gallivm, fs_type);
      LLVMValueRef mask_store = lp_build_array_alloca(gallivm, mask_type,
                                                      num_loop, "mask_store");
      LLVMValueRef color_store[PIPE_MAX_COLOR_BUFS][TGSI_NUM_CHANNELS];
      boolean pixel_center_integer =
         shader->info.base.properties[TGSI_PROPERTY_FS_COORD_PIXEL_CENTER];

      /*
       * The shader input interpolation info is not explicitely baked in the
       * shader key, but everything it derives from (TGSI, and flatshade) is
       * already included in the shader key.
       */
      lp_build_interp_soa_init(&interp,
                               gallivm,
                               shader->info.base.num_inputs,
                               inputs,
                               pixel_center_integer,
                               key->depth_clamp,
                               builder, fs_type,
                               a0_ptr, dadx_ptr, dady_ptr,
                               x, y);

      for (i = 0; i < num_fs; i++) {
         LLVMValueRef mask;
         LLVMValueRef indexi = lp_build_const_int32(gallivm, i);
         LLVMValueRef mask_ptr = LLVMBuildGEP(builder, mask_store,
                                              &indexi, 1, "mask_ptr");

         if (partial_mask) {
            mask = generate_quad_mask(gallivm, fs_type,
                                      i*fs_type.length/4, mask_input);
         }
         else {
            mask = lp_build_const_int_vec(gallivm, fs_type, ~0);
         }
         LLVMBuildStore(builder, mask, mask_ptr);
      }

      generate_fs_loop(gallivm,
                       shader, key,
                       builder,
                       fs_type,
                       context_ptr,
                       num_loop,
                       &interp,
                       sampler,
                       mask_store, /* output */
                       color_store,
                       depth_ptr,
                       depth_stride,
                       facing,
                       thread_data_ptr);

      for (i = 0; i < num_fs; i++) {
         LLVMValueRef indexi = lp_build_const_int32(gallivm, i);
         LLVMValueRef ptr = LLVMBuildGEP(builder, mask_store,
                                         &indexi, 1, "");
         fs_mask[i] = LLVMBuildLoad(builder, ptr, "mask");
         /* This is fucked up need to reorganize things */
         for (cbuf = 0; cbuf < key->nr_cbufs; cbuf++) {
            for (chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
               ptr = LLVMBuildGEP(builder,
                                  color_store[cbuf * !cbuf0_write_all][chan],
                                  &indexi, 1, "");
               fs_out_color[cbuf][chan][i] = ptr;
            }
         }
         if (dual_source_blend) {
            /* only support one dual source blend target hence always use output 1 */
            for (chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
               ptr = LLVMBuildGEP(builder,
                                  color_store[1][chan],
                                  &indexi, 1, "");
               fs_out_color[1][chan][i] = ptr;
            }
         }
      }
   }

   sampler->destroy(sampler);

   /* Loop over color outputs / color buffers to do blending.
    */
   for(cbuf = 0; cbuf < key->nr_cbufs; cbuf++) {
      if (key->cbuf_format[cbuf] != PIPE_FORMAT_NONE) {
         LLVMValueRef color_ptr;
         LLVMValueRef stride;
         LLVMValueRef index = lp_build_const_int32(gallivm, cbuf);

         boolean do_branch = ((key->depth.enabled
                               || key->stencil[0].enabled
                               || key->alpha.enabled)
                              && !shader->info.base.uses_kill);

         color_ptr = LLVMBuildLoad(builder,
                                   LLVMBuildGEP(builder, color_ptr_ptr,
                                                &index, 1, ""),
                                   "");

         lp_build_name(color_ptr, "color_ptr%d", cbuf);

         stride = LLVMBuildLoad(builder,
                                LLVMBuildGEP(builder, stride_ptr, &index, 1, ""),
                                "");

         generate_unswizzled_blend(gallivm, cbuf, variant,
                                   key->cbuf_format[cbuf],
                                   num_fs, fs_type, fs_mask, fs_out_color,
                                   context_ptr, color_ptr, stride,
                                   partial_mask, do_branch);
      }
   }

   LLVMBuildRetVoid(builder);

   gallivm_verify_function(gallivm, function);
}


static void
dump_fs_variant_key(const struct lp_fragment_shader_variant_key *key)
{
   unsigned i;

   debug_printf("fs variant %p:\n", (void *) key);

   if (key->flatshade) {
      debug_printf("flatshade = 1\n");
   }
   for (i = 0; i < key->nr_cbufs; ++i) {
      debug_printf("cbuf_format[%u] = %s\n", i, util_format_name(key->cbuf_format[i]));
   }
   if (key->depth.enabled || key->stencil[0].enabled) {
      debug_printf("depth.format = %s\n", util_format_name(key->zsbuf_format));
   }
   if (key->depth.enabled) {
      debug_printf("depth.func = %s\n", util_str_func(key->depth.func, TRUE));
      debug_printf("depth.writemask = %u\n", key->depth.writemask);
   }

   for (i = 0; i < 2; ++i) {
      if (key->stencil[i].enabled) {
         debug_printf("stencil[%u].func = %s\n", i, util_str_func(key->stencil[i].func, TRUE));
         debug_printf("stencil[%u].fail_op = %s\n", i, util_str_stencil_op(key->stencil[i].fail_op, TRUE));
         debug_printf("stencil[%u].zpass_op = %s\n", i, util_str_stencil_op(key->stencil[i].zpass_op, TRUE));
         debug_printf("stencil[%u].zfail_op = %s\n", i, util_str_stencil_op(key->stencil[i].zfail_op, TRUE));
         debug_printf("stencil[%u].valuemask = 0x%x\n", i, key->stencil[i].valuemask);
         debug_printf("stencil[%u].writemask = 0x%x\n", i, key->stencil[i].writemask);
      }
   }

   if (key->alpha.enabled) {
      debug_printf("alpha.func = %s\n", util_str_func(key->alpha.func, TRUE));
   }

   if (key->occlusion_count) {
      debug_printf("occlusion_count = 1\n");
   }

   if (key->blend.logicop_enable) {
      debug_printf("blend.logicop_func = %s\n", util_str_logicop(key->blend.logicop_func, TRUE));
   }
   else if (key->blend.rt[0].blend_enable) {
      debug_printf("blend.rgb_func = %s\n",   util_str_blend_func  (key->blend.rt[0].rgb_func, TRUE));
      debug_printf("blend.rgb_src_factor = %s\n",   util_str_blend_factor(key->blend.rt[0].rgb_src_factor, TRUE));
      debug_printf("blend.rgb_dst_factor = %s\n",   util_str_blend_factor(key->blend.rt[0].rgb_dst_factor, TRUE));
      debug_printf("blend.alpha_func = %s\n",       util_str_blend_func  (key->blend.rt[0].alpha_func, TRUE));
      debug_printf("blend.alpha_src_factor = %s\n", util_str_blend_factor(key->blend.rt[0].alpha_src_factor, TRUE));
      debug_printf("blend.alpha_dst_factor = %s\n", util_str_blend_factor(key->blend.rt[0].alpha_dst_factor, TRUE));
   }
   debug_printf("blend.colormask = 0x%x\n", key->blend.rt[0].colormask);
   if (key->blend.alpha_to_coverage) {
      debug_printf("blend.alpha_to_coverage is enabled\n");
   }
   for (i = 0; i < key->nr_samplers; ++i) {
      const struct lp_static_sampler_state *sampler = &key->state[i].sampler_state;
      debug_printf("sampler[%u] = \n", i);
      debug_printf("  .wrap = %s %s %s\n",
                   util_str_tex_wrap(sampler->wrap_s, TRUE),
                   util_str_tex_wrap(sampler->wrap_t, TRUE),
                   util_str_tex_wrap(sampler->wrap_r, TRUE));
      debug_printf("  .min_img_filter = %s\n",
                   util_str_tex_filter(sampler->min_img_filter, TRUE));
      debug_printf("  .min_mip_filter = %s\n",
                   util_str_tex_mipfilter(sampler->min_mip_filter, TRUE));
      debug_printf("  .mag_img_filter = %s\n",
                   util_str_tex_filter(sampler->mag_img_filter, TRUE));
      if (sampler->compare_mode != PIPE_TEX_COMPARE_NONE)
         debug_printf("  .compare_func = %s\n", util_str_func(sampler->compare_func, TRUE));
      debug_printf("  .normalized_coords = %u\n", sampler->normalized_coords);
      debug_printf("  .min_max_lod_equal = %u\n", sampler->min_max_lod_equal);
      debug_printf("  .lod_bias_non_zero = %u\n", sampler->lod_bias_non_zero);
      debug_printf("  .apply_min_lod = %u\n", sampler->apply_min_lod);
      debug_printf("  .apply_max_lod = %u\n", sampler->apply_max_lod);
   }
   for (i = 0; i < key->nr_sampler_views; ++i) {
      const struct lp_static_texture_state *texture = &key->state[i].texture_state;
      debug_printf("texture[%u] = \n", i);
      debug_printf("  .format = %s\n",
                   util_format_name(texture->format));
      debug_printf("  .target = %s\n",
                   util_str_tex_target(texture->target, TRUE));
      debug_printf("  .level_zero_only = %u\n",
                   texture->level_zero_only);
      debug_printf("  .pot = %u %u %u\n",
                   texture->pot_width,
                   texture->pot_height,
                   texture->pot_depth);
   }
}


void
lp_debug_fs_variant(const struct lp_fragment_shader_variant *variant)
{
   debug_printf("llvmpipe: Fragment shader #%u variant #%u:\n", 
                variant->shader->no, variant->no);
   tgsi_dump(variant->shader->base.tokens, 0);
   dump_fs_variant_key(&variant->key);
   debug_printf("variant->opaque = %u\n", variant->opaque);
   debug_printf("\n");
}


/**
 * Generate a new fragment shader variant from the shader code and
 * other state indicated by the key.
 */
static struct lp_fragment_shader_variant *
generate_variant(struct llvmpipe_context *lp,
                 struct lp_fragment_shader *shader,
                 const struct lp_fragment_shader_variant_key *key)
{
   struct lp_fragment_shader_variant *variant;
   const struct util_format_description *cbuf0_format_desc = NULL;
   boolean fullcolormask;
   char module_name[64];

   variant = CALLOC_STRUCT(lp_fragment_shader_variant);
   if (!variant)
      return NULL;

   util_snprintf(module_name, sizeof(module_name), "fs%u_variant%u",
                 shader->no, shader->variants_created);

   variant->gallivm = gallivm_create(module_name, lp->context);
   if (!variant->gallivm) {
      FREE(variant);
      return NULL;
   }

   variant->shader = shader;
   variant->list_item_global.base = variant;
   variant->list_item_local.base = variant;
   variant->no = shader->variants_created++;

   memcpy(&variant->key, key, shader->variant_key_size);

   /*
    * Determine whether we are touching all channels in the color buffer.
    */
   fullcolormask = FALSE;
   if (key->nr_cbufs == 1) {
      cbuf0_format_desc = util_format_description(key->cbuf_format[0]);
      fullcolormask = util_format_colormask_full(cbuf0_format_desc, key->blend.rt[0].colormask);
   }

   variant->opaque =
         !key->blend.logicop_enable &&
         !key->blend.rt[0].blend_enable &&
         fullcolormask &&
         !key->stencil[0].enabled &&
         !key->alpha.enabled &&
         !key->blend.alpha_to_coverage &&
         !key->depth.enabled &&
         !shader->info.base.uses_kill &&
         !shader->info.base.writes_samplemask
      ? TRUE : FALSE;

   if ((shader->info.base.num_tokens <= 1) &&
       !key->depth.enabled && !key->stencil[0].enabled) {
      variant->ps_inv_multiplier = 0;
   } else {
      variant->ps_inv_multiplier = 1;
   }

   if ((LP_DEBUG & DEBUG_FS) || (gallivm_debug & GALLIVM_DEBUG_IR)) {
      lp_debug_fs_variant(variant);
   }

   lp_jit_init_types(variant);
   
   if (variant->jit_function[RAST_EDGE_TEST] == NULL)
      generate_fragment(lp, shader, variant, RAST_EDGE_TEST);

   if (variant->jit_function[RAST_WHOLE] == NULL) {
      if (variant->opaque) {
         /* Specialized shader, which doesn't need to read the color buffer. */
         generate_fragment(lp, shader, variant, RAST_WHOLE);
      }
   }

   /*
    * Compile everything
    */

   gallivm_compile_module(variant->gallivm);

   variant->nr_instrs += lp_build_count_ir_module(variant->gallivm->module);

   if (variant->function[RAST_EDGE_TEST]) {
      variant->jit_function[RAST_EDGE_TEST] = (lp_jit_frag_func)
            gallivm_jit_function(variant->gallivm,
                                 variant->function[RAST_EDGE_TEST]);
   }

   if (variant->function[RAST_WHOLE]) {
         variant->jit_function[RAST_WHOLE] = (lp_jit_frag_func)
               gallivm_jit_function(variant->gallivm,
                                    variant->function[RAST_WHOLE]);
   } else if (!variant->jit_function[RAST_WHOLE]) {
      variant->jit_function[RAST_WHOLE] = variant->jit_function[RAST_EDGE_TEST];
   }

   gallivm_free_ir(variant->gallivm);

   return variant;
}


static void *
llvmpipe_create_fs_state(struct pipe_context *pipe,
                         const struct pipe_shader_state *templ)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   struct lp_fragment_shader *shader;
   int nr_samplers;
   int nr_sampler_views;
   int i;

   shader = CALLOC_STRUCT(lp_fragment_shader);
   if (!shader)
      return NULL;

   shader->no = fs_no++;
   make_empty_list(&shader->variants);

   /* get/save the summary info for this shader */
   lp_build_tgsi_info(templ->tokens, &shader->info);

   /* we need to keep a local copy of the tokens */
   shader->base.tokens = tgsi_dup_tokens(templ->tokens);

   shader->draw_data = draw_create_fragment_shader(llvmpipe->draw, templ);
   if (shader->draw_data == NULL) {
      FREE((void *) shader->base.tokens);
      FREE(shader);
      return NULL;
   }

   nr_samplers = shader->info.base.file_max[TGSI_FILE_SAMPLER] + 1;
   nr_sampler_views = shader->info.base.file_max[TGSI_FILE_SAMPLER_VIEW] + 1;

   shader->variant_key_size = Offset(struct lp_fragment_shader_variant_key,
                                     state[MAX2(nr_samplers, nr_sampler_views)]);

   for (i = 0; i < shader->info.base.num_inputs; i++) {
      shader->inputs[i].usage_mask = shader->info.base.input_usage_mask[i];
      shader->inputs[i].cyl_wrap = shader->info.base.input_cylindrical_wrap[i];

      switch (shader->info.base.input_interpolate[i]) {
      case TGSI_INTERPOLATE_CONSTANT:
         shader->inputs[i].interp = LP_INTERP_CONSTANT;
         break;
      case TGSI_INTERPOLATE_LINEAR:
         shader->inputs[i].interp = LP_INTERP_LINEAR;
         break;
      case TGSI_INTERPOLATE_PERSPECTIVE:
         shader->inputs[i].interp = LP_INTERP_PERSPECTIVE;
         break;
      case TGSI_INTERPOLATE_COLOR:
         shader->inputs[i].interp = LP_INTERP_COLOR;
         break;
      default:
         assert(0);
         break;
      }

      switch (shader->info.base.input_semantic_name[i]) {
      case TGSI_SEMANTIC_FACE:
         shader->inputs[i].interp = LP_INTERP_FACING;
         break;
      case TGSI_SEMANTIC_POSITION:
         /* Position was already emitted above
          */
         shader->inputs[i].interp = LP_INTERP_POSITION;
         shader->inputs[i].src_index = 0;
         continue;
      }

      /* XXX this is a completely pointless index map... */
      shader->inputs[i].src_index = i+1;
   }

   if (LP_DEBUG & DEBUG_TGSI) {
      unsigned attrib;
      debug_printf("llvmpipe: Create fragment shader #%u %p:\n",
                   shader->no, (void *) shader);
      tgsi_dump(templ->tokens, 0);
      debug_printf("usage masks:\n");
      for (attrib = 0; attrib < shader->info.base.num_inputs; ++attrib) {
         unsigned usage_mask = shader->info.base.input_usage_mask[attrib];
         debug_printf("  IN[%u].%s%s%s%s\n",
                      attrib,
                      usage_mask & TGSI_WRITEMASK_X ? "x" : "",
                      usage_mask & TGSI_WRITEMASK_Y ? "y" : "",
                      usage_mask & TGSI_WRITEMASK_Z ? "z" : "",
                      usage_mask & TGSI_WRITEMASK_W ? "w" : "");
      }
      debug_printf("\n");
   }

   return shader;
}


static void
llvmpipe_bind_fs_state(struct pipe_context *pipe, void *fs)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);

   if (llvmpipe->fs == fs)
      return;

   llvmpipe->fs = (struct lp_fragment_shader *) fs;

   draw_bind_fragment_shader(llvmpipe->draw,
                             (llvmpipe->fs ? llvmpipe->fs->draw_data : NULL));

   llvmpipe->dirty |= LP_NEW_FS;
}


/**
 * Remove shader variant from two lists: the shader's variant list
 * and the context's variant list.
 */
void
llvmpipe_remove_shader_variant(struct llvmpipe_context *lp,
                               struct lp_fragment_shader_variant *variant)
{
   if ((LP_DEBUG & DEBUG_FS) || (gallivm_debug & GALLIVM_DEBUG_IR)) {
      debug_printf("llvmpipe: del fs #%u var %u v created %u v cached %u "
                   "v total cached %u inst %u total inst %u\n",
                   variant->shader->no, variant->no,
                   variant->shader->variants_created,
                   variant->shader->variants_cached,
                   lp->nr_fs_variants, variant->nr_instrs, lp->nr_fs_instrs);
   }

   gallivm_destroy(variant->gallivm);

   /* remove from shader's list */
   remove_from_list(&variant->list_item_local);
   variant->shader->variants_cached--;

   /* remove from context's list */
   remove_from_list(&variant->list_item_global);
   lp->nr_fs_variants--;
   lp->nr_fs_instrs -= variant->nr_instrs;

   FREE(variant);
}


static void
llvmpipe_delete_fs_state(struct pipe_context *pipe, void *fs)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   struct lp_fragment_shader *shader = fs;
   struct lp_fs_variant_list_item *li;

   assert(fs != llvmpipe->fs);

   /*
    * XXX: we need to flush the context until we have some sort of reference
    * counting in fragment shaders as they may still be binned
    * Flushing alone might not sufficient we need to wait on it too.
    */
   llvmpipe_finish(pipe, __FUNCTION__);

   /* Delete all the variants */
   li = first_elem(&shader->variants);
   while(!at_end(&shader->variants, li)) {
      struct lp_fs_variant_list_item *next = next_elem(li);
      llvmpipe_remove_shader_variant(llvmpipe, li->base);
      li = next;
   }

   /* Delete draw module's data */
   draw_delete_fragment_shader(llvmpipe->draw, shader->draw_data);

   assert(shader->variants_cached == 0);
   FREE((void *) shader->base.tokens);
   FREE(shader);
}



static void
llvmpipe_set_constant_buffer(struct pipe_context *pipe,
                             enum pipe_shader_type shader, uint index,
                             const struct pipe_constant_buffer *cb)
{
   struct llvmpipe_context *llvmpipe = llvmpipe_context(pipe);
   struct pipe_resource *constants = cb ? cb->buffer : NULL;

   assert(shader < PIPE_SHADER_TYPES);
   assert(index < ARRAY_SIZE(llvmpipe->constants[shader]));

   /* note: reference counting */
   util_copy_constant_buffer(&llvmpipe->constants[shader][index], cb);

   if (constants) {
       if (!(constants->bind & PIPE_BIND_CONSTANT_BUFFER)) {
         debug_printf("Illegal set constant without bind flag\n");
         constants->bind |= PIPE_BIND_CONSTANT_BUFFER;
      }
   }

   if (shader == PIPE_SHADER_VERTEX ||
       shader == PIPE_SHADER_GEOMETRY) {
      /* Pass the constants to the 'draw' module */
      const unsigned size = cb ? cb->buffer_size : 0;
      const ubyte *data;

      if (constants) {
         data = (ubyte *) llvmpipe_resource_data(constants);
      }
      else if (cb && cb->user_buffer) {
         data = (ubyte *) cb->user_buffer;
      }
      else {
         data = NULL;
      }

      if (data)
         data += cb->buffer_offset;

      draw_set_mapped_constant_buffer(llvmpipe->draw, shader,
                                      index, data, size);
   }
   else {
      llvmpipe->dirty |= LP_NEW_FS_CONSTANTS;
   }

   if (cb && cb->user_buffer) {
      pipe_resource_reference(&constants, NULL);
   }
}


/**
 * Return the blend factor equivalent to a destination alpha of one.
 */
static inline unsigned
force_dst_alpha_one(unsigned factor, boolean clamped_zero)
{
   switch(factor) {
   case PIPE_BLENDFACTOR_DST_ALPHA:
      return PIPE_BLENDFACTOR_ONE;
   case PIPE_BLENDFACTOR_INV_DST_ALPHA:
      return PIPE_BLENDFACTOR_ZERO;
   case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE:
      if (clamped_zero)
         return PIPE_BLENDFACTOR_ZERO;
      else
         return PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE;
   }

   return factor;
}


/**
 * We need to generate several variants of the fragment pipeline to match
 * all the combinations of the contributing state atoms.
 *
 * TODO: there is actually no reason to tie this to context state -- the
 * generated code could be cached globally in the screen.
 */
static void
make_variant_key(struct llvmpipe_context *lp,
                 struct lp_fragment_shader *shader,
                 struct lp_fragment_shader_variant_key *key)
{
   unsigned i;

   memset(key, 0, shader->variant_key_size);

   if (lp->framebuffer.zsbuf) {
      enum pipe_format zsbuf_format = lp->framebuffer.zsbuf->format;
      const struct util_format_description *zsbuf_desc =
         util_format_description(zsbuf_format);

      if (lp->depth_stencil->depth.enabled &&
          util_format_has_depth(zsbuf_desc)) {
         key->zsbuf_format = zsbuf_format;
         memcpy(&key->depth, &lp->depth_stencil->depth, sizeof key->depth);
      }
      if (lp->depth_stencil->stencil[0].enabled &&
          util_format_has_stencil(zsbuf_desc)) {
         key->zsbuf_format = zsbuf_format;
         memcpy(&key->stencil, &lp->depth_stencil->stencil, sizeof key->stencil);
      }
      if (llvmpipe_resource_is_1d(lp->framebuffer.zsbuf->texture)) {
         key->resource_1d = TRUE;
      }
   }

   /*
    * Propagate the depth clamp setting from the rasterizer state.
    * depth_clip == 0 implies depth clamping is enabled.
    *
    * When clip_halfz is enabled, then always clamp the depth values.
    *
    * XXX: This is incorrect for GL, but correct for d3d10 (depth
    * clamp is always active in d3d10, regardless if depth clip is
    * enabled or not).
    * (GL has an always-on [0,1] clamp on fs depth output instead
    * to ensure the depth values stay in range. Doesn't look like
    * we do that, though...)
    */
   if (lp->rasterizer->clip_halfz) {
      key->depth_clamp = 1;
   } else {
      key->depth_clamp = (lp->rasterizer->depth_clip == 0) ? 1 : 0;
   }

   /* alpha test only applies if render buffer 0 is non-integer (or does not exist) */
   if (!lp->framebuffer.nr_cbufs ||
       !lp->framebuffer.cbufs[0] ||
       !util_format_is_pure_integer(lp->framebuffer.cbufs[0]->format)) {
      key->alpha.enabled = lp->depth_stencil->alpha.enabled;
   }
   if(key->alpha.enabled)
      key->alpha.func = lp->depth_stencil->alpha.func;
   /* alpha.ref_value is passed in jit_context */

   key->flatshade = lp->rasterizer->flatshade;
   if (lp->active_occlusion_queries) {
      key->occlusion_count = TRUE;
   }

   if (lp->framebuffer.nr_cbufs) {
      memcpy(&key->blend, lp->blend, sizeof key->blend);
   }

   key->nr_cbufs = lp->framebuffer.nr_cbufs;

   if (!key->blend.independent_blend_enable) {
      /* we always need independent blend otherwise the fixups below won't work */
      for (i = 1; i < key->nr_cbufs; i++) {
         memcpy(&key->blend.rt[i], &key->blend.rt[0], sizeof(key->blend.rt[0]));
      }
      key->blend.independent_blend_enable = 1;
   }

   for (i = 0; i < lp->framebuffer.nr_cbufs; i++) {
      struct pipe_rt_blend_state *blend_rt = &key->blend.rt[i];

      if (lp->framebuffer.cbufs[i]) {
         enum pipe_format format = lp->framebuffer.cbufs[i]->format;
         const struct util_format_description *format_desc;

         key->cbuf_format[i] = format;

         /*
          * Figure out if this is a 1d resource. Note that OpenGL allows crazy
          * mixing of 2d textures with height 1 and 1d textures, so make sure
          * we pick 1d if any cbuf or zsbuf is 1d.
          */
         if (llvmpipe_resource_is_1d(lp->framebuffer.cbufs[i]->texture)) {
            key->resource_1d = TRUE;
         }

         format_desc = util_format_description(format);
         assert(format_desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB ||
                format_desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB);

         /*
          * Mask out color channels not present in the color buffer.
          */
         blend_rt->colormask &= util_format_colormask(format_desc);

         /*
          * Disable blend for integer formats.
          */
         if (util_format_is_pure_integer(format)) {
            blend_rt->blend_enable = 0;
         }

         /*
          * Our swizzled render tiles always have an alpha channel, but the
          * linear render target format often does not, so force here the dst
          * alpha to be one.
          *
          * This is not a mere optimization. Wrong results will be produced if
          * the dst alpha is used, the dst format does not have alpha, and the
          * previous rendering was not flushed from the swizzled to linear
          * buffer. For example, NonPowTwo DCT.
          *
          * TODO: This should be generalized to all channels for better
          * performance, but only alpha causes correctness issues.
          *
          * Also, force rgb/alpha func/factors match, to make AoS blending
          * easier.
          */
         if (format_desc->swizzle[3] > PIPE_SWIZZLE_W ||
             format_desc->swizzle[3] == format_desc->swizzle[0]) {
            /* Doesn't cover mixed snorm/unorm but can't render to them anyway */
            boolean clamped_zero = !util_format_is_float(format) &&
                                   !util_format_is_snorm(format);
            blend_rt->rgb_src_factor =
               force_dst_alpha_one(blend_rt->rgb_src_factor, clamped_zero);
            blend_rt->rgb_dst_factor =
               force_dst_alpha_one(blend_rt->rgb_dst_factor, clamped_zero);
            blend_rt->alpha_func       = blend_rt->rgb_func;
            blend_rt->alpha_src_factor = blend_rt->rgb_src_factor;
            blend_rt->alpha_dst_factor = blend_rt->rgb_dst_factor;
         }
      }
      else {
         /* no color buffer for this fragment output */
         key->cbuf_format[i] = PIPE_FORMAT_NONE;
         blend_rt->colormask = 0x0;
         blend_rt->blend_enable = 0;
      }
   }

   /* This value will be the same for all the variants of a given shader:
    */
   key->nr_samplers = shader->info.base.file_max[TGSI_FILE_SAMPLER] + 1;

   for(i = 0; i < key->nr_samplers; ++i) {
      if(shader->info.base.file_mask[TGSI_FILE_SAMPLER] & (1 << i)) {
         lp_sampler_static_sampler_state(&key->state[i].sampler_state,
                                         lp->samplers[PIPE_SHADER_FRAGMENT][i]);
      }
   }

   /*
    * XXX If TGSI_FILE_SAMPLER_VIEW exists assume all texture opcodes
    * are dx10-style? Can't really have mixed opcodes, at least not
    * if we want to skip the holes here (without rescanning tgsi).
    */
   if (shader->info.base.file_max[TGSI_FILE_SAMPLER_VIEW] != -1) {
      key->nr_sampler_views = shader->info.base.file_max[TGSI_FILE_SAMPLER_VIEW] + 1;
      for(i = 0; i < key->nr_sampler_views; ++i) {
         if(shader->info.base.file_mask[TGSI_FILE_SAMPLER_VIEW] & (1 << i)) {
            lp_sampler_static_texture_state(&key->state[i].texture_state,
                                            lp->sampler_views[PIPE_SHADER_FRAGMENT][i]);
         }
      }
   }
   else {
      key->nr_sampler_views = key->nr_samplers;
      for(i = 0; i < key->nr_sampler_views; ++i) {
         if(shader->info.base.file_mask[TGSI_FILE_SAMPLER] & (1 << i)) {
            lp_sampler_static_texture_state(&key->state[i].texture_state,
                                            lp->sampler_views[PIPE_SHADER_FRAGMENT][i]);
         }
      }
   }
}



/**
 * Update fragment shader state.  This is called just prior to drawing
 * something when some fragment-related state has changed.
 */
void 
llvmpipe_update_fs(struct llvmpipe_context *lp)
{
   struct lp_fragment_shader *shader = lp->fs;
   struct lp_fragment_shader_variant_key key;
   struct lp_fragment_shader_variant *variant = NULL;
   struct lp_fs_variant_list_item *li;

   make_variant_key(lp, shader, &key);

   /* Search the variants for one which matches the key */
   li = first_elem(&shader->variants);
   while(!at_end(&shader->variants, li)) {
      if(memcmp(&li->base->key, &key, shader->variant_key_size) == 0) {
         variant = li->base;
         break;
      }
      li = next_elem(li);
   }

   if (variant) {
      /* Move this variant to the head of the list to implement LRU
       * deletion of shader's when we have too many.
       */
      move_to_head(&lp->fs_variants_list, &variant->list_item_global);
   }
   else {
      /* variant not found, create it now */
      int64_t t0, t1, dt;
      unsigned i;
      unsigned variants_to_cull;

      if (LP_DEBUG & DEBUG_FS) {
         debug_printf("%u variants,\t%u instrs,\t%u instrs/variant\n",
                      lp->nr_fs_variants,
                      lp->nr_fs_instrs,
                      lp->nr_fs_variants ? lp->nr_fs_instrs / lp->nr_fs_variants : 0);
      }

      /* First, check if we've exceeded the max number of shader variants.
       * If so, free 6.25% of them (the least recently used ones).
       */
      variants_to_cull = lp->nr_fs_variants >= LP_MAX_SHADER_VARIANTS ? LP_MAX_SHADER_VARIANTS / 16 : 0;

      if (variants_to_cull ||
          lp->nr_fs_instrs >= LP_MAX_SHADER_INSTRUCTIONS) {
         struct pipe_context *pipe = &lp->pipe;

         if (gallivm_debug & GALLIVM_DEBUG_PERF) {
            debug_printf("Evicting FS: %u fs variants,\t%u total variants,"
                         "\t%u instrs,\t%u instrs/variant\n",
                         shader->variants_cached,
                         lp->nr_fs_variants, lp->nr_fs_instrs,
                         lp->nr_fs_instrs / lp->nr_fs_variants);
         }

         /*
          * XXX: we need to flush the context until we have some sort of
          * reference counting in fragment shaders as they may still be binned
          * Flushing alone might not be sufficient we need to wait on it too.
          */
         llvmpipe_finish(pipe, __FUNCTION__);

         /*
          * We need to re-check lp->nr_fs_variants because an arbitrarliy large
          * number of shader variants (potentially all of them) could be
          * pending for destruction on flush.
          */

         for (i = 0; i < variants_to_cull || lp->nr_fs_instrs >= LP_MAX_SHADER_INSTRUCTIONS; i++) {
            struct lp_fs_variant_list_item *item;
            if (is_empty_list(&lp->fs_variants_list)) {
               break;
            }
            item = last_elem(&lp->fs_variants_list);
            assert(item);
            assert(item->base);
            llvmpipe_remove_shader_variant(lp, item->base);
         }
      }

      /*
       * Generate the new variant.
       */
      t0 = os_time_get();
      variant = generate_variant(lp, shader, &key);
      t1 = os_time_get();
      dt = t1 - t0;
      LP_COUNT_ADD(llvm_compile_time, dt);
      LP_COUNT_ADD(nr_llvm_compiles, 2);  /* emit vs. omit in/out test */

      /* Put the new variant into the list */
      if (variant) {
         insert_at_head(&shader->variants, &variant->list_item_local);
         insert_at_head(&lp->fs_variants_list, &variant->list_item_global);
         lp->nr_fs_variants++;
         lp->nr_fs_instrs += variant->nr_instrs;
         shader->variants_cached++;
      }
   }

   /* Bind this variant */
   lp_setup_set_fs_variant(lp->setup, variant);
}





void
llvmpipe_init_fs_funcs(struct llvmpipe_context *llvmpipe)
{
   llvmpipe->pipe.create_fs_state = llvmpipe_create_fs_state;
   llvmpipe->pipe.bind_fs_state   = llvmpipe_bind_fs_state;
   llvmpipe->pipe.delete_fs_state = llvmpipe_delete_fs_state;

   llvmpipe->pipe.set_constant_buffer = llvmpipe_set_constant_buffer;
}

/*
 * Rasterization is disabled if there is no pixel shader and
 * both depth and stencil testing are disabled:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/bb205125
 */
boolean
llvmpipe_rasterization_disabled(struct llvmpipe_context *lp)
{
   boolean null_fs = !lp->fs || lp->fs->info.base.num_tokens <= 1;

   return (null_fs &&
           !lp->depth_stencil->depth.enabled &&
           !lp->depth_stencil->stencil[0].enabled);
}
