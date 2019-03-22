/**************************************************************************
 * 
 * Copyright 2009 VMware, Inc.
 * All rights reserved.
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
 * Texture sampling code generation
 *
 * This file is nothing more than ugly glue between three largely independent
 * entities:
 * - TGSI -> LLVM translation (i.e., lp_build_tgsi_soa)
 * - texture sampling code generation (i.e., lp_build_sample_soa)
 * - LLVM pipe driver
 *
 * All interesting code is in the functions mentioned above. There is really
 * nothing to see here.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 */

#include "pipe/p_defines.h"
#include "pipe/p_shader_tokens.h"
#include "gallivm/lp_bld_debug.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_type.h"
#include "gallivm/lp_bld_sample.h"
#include "gallivm/lp_bld_tgsi.h"
#include "lp_jit.h"
#include "lp_tex_sample.h"
#include "lp_state_fs.h"
#include "lp_debug.h"


/**
 * This provides the bridge between the sampler state store in
 * lp_jit_context and lp_jit_texture and the sampler code
 * generator. It provides the texture layout information required by
 * the texture sampler code generator in terms of the state stored in
 * lp_jit_context and lp_jit_texture in runtime.
 */
struct llvmpipe_sampler_dynamic_state
{
   struct lp_sampler_dynamic_state base;

   const struct lp_sampler_static_state *static_state;
};


/**
 * This is the bridge between our sampler and the TGSI translator.
 */
struct lp_llvm_sampler_soa
{
   struct lp_build_sampler_soa base;

   struct llvmpipe_sampler_dynamic_state dynamic_state;
};


/**
 * Fetch the specified member of the lp_jit_texture structure.
 * \param emit_load  if TRUE, emit the LLVM load instruction to actually
 *                   fetch the field's value.  Otherwise, just emit the
 *                   GEP code to address the field.
 *
 * @sa http://llvm.org/docs/GetElementPtr.html
 */
static LLVMValueRef
lp_llvm_texture_member(const struct lp_sampler_dynamic_state *base,
                       struct gallivm_state *gallivm,
                       LLVMValueRef context_ptr,
                       unsigned texture_unit,
                       unsigned member_index,
                       const char *member_name,
                       boolean emit_load)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef indices[4];
   LLVMValueRef ptr;
   LLVMValueRef res;

   assert(texture_unit < PIPE_MAX_SHADER_SAMPLER_VIEWS);

   /* context[0] */
   indices[0] = lp_build_const_int32(gallivm, 0);
   /* context[0].textures */
   indices[1] = lp_build_const_int32(gallivm, LP_JIT_CTX_TEXTURES);
   /* context[0].textures[unit] */
   indices[2] = lp_build_const_int32(gallivm, texture_unit);
   /* context[0].textures[unit].member */
   indices[3] = lp_build_const_int32(gallivm, member_index);

   ptr = LLVMBuildGEP(builder, context_ptr, indices, ARRAY_SIZE(indices), "");

   if (emit_load)
      res = LLVMBuildLoad(builder, ptr, "");
   else
      res = ptr;

   lp_build_name(res, "context.texture%u.%s", texture_unit, member_name);

   return res;
}


/**
 * Helper macro to instantiate the functions that generate the code to
 * fetch the members of lp_jit_texture to fulfill the sampler code
 * generator requests.
 *
 * This complexity is the price we have to pay to keep the texture
 * sampler code generator a reusable module without dependencies to
 * llvmpipe internals.
 */
#define LP_LLVM_TEXTURE_MEMBER(_name, _index, _emit_load)  \
   static LLVMValueRef \
   lp_llvm_texture_##_name( const struct lp_sampler_dynamic_state *base, \
                            struct gallivm_state *gallivm, \
                            LLVMValueRef context_ptr, \
                            unsigned texture_unit) \
   { \
      return lp_llvm_texture_member(base, gallivm, context_ptr, \
                                    texture_unit, _index, #_name, _emit_load ); \
   }


LP_LLVM_TEXTURE_MEMBER(width,      LP_JIT_TEXTURE_WIDTH, TRUE)
LP_LLVM_TEXTURE_MEMBER(height,     LP_JIT_TEXTURE_HEIGHT, TRUE)
LP_LLVM_TEXTURE_MEMBER(depth,      LP_JIT_TEXTURE_DEPTH, TRUE)
LP_LLVM_TEXTURE_MEMBER(first_level, LP_JIT_TEXTURE_FIRST_LEVEL, TRUE)
LP_LLVM_TEXTURE_MEMBER(last_level, LP_JIT_TEXTURE_LAST_LEVEL, TRUE)
LP_LLVM_TEXTURE_MEMBER(base_ptr,   LP_JIT_TEXTURE_BASE, TRUE)
LP_LLVM_TEXTURE_MEMBER(row_stride, LP_JIT_TEXTURE_ROW_STRIDE, FALSE)
LP_LLVM_TEXTURE_MEMBER(img_stride, LP_JIT_TEXTURE_IMG_STRIDE, FALSE)
LP_LLVM_TEXTURE_MEMBER(mip_offsets, LP_JIT_TEXTURE_MIP_OFFSETS, FALSE)


/**
 * Fetch the specified member of the lp_jit_sampler structure.
 * \param emit_load  if TRUE, emit the LLVM load instruction to actually
 *                   fetch the field's value.  Otherwise, just emit the
 *                   GEP code to address the field.
 *
 * @sa http://llvm.org/docs/GetElementPtr.html
 */
static LLVMValueRef
lp_llvm_sampler_member(const struct lp_sampler_dynamic_state *base,
                       struct gallivm_state *gallivm,
                       LLVMValueRef context_ptr,
                       unsigned sampler_unit,
                       unsigned member_index,
                       const char *member_name,
                       boolean emit_load)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef indices[4];
   LLVMValueRef ptr;
   LLVMValueRef res;

   assert(sampler_unit < PIPE_MAX_SAMPLERS);

   /* context[0] */
   indices[0] = lp_build_const_int32(gallivm, 0);
   /* context[0].samplers */
   indices[1] = lp_build_const_int32(gallivm, LP_JIT_CTX_SAMPLERS);
   /* context[0].samplers[unit] */
   indices[2] = lp_build_const_int32(gallivm, sampler_unit);
   /* context[0].samplers[unit].member */
   indices[3] = lp_build_const_int32(gallivm, member_index);

   ptr = LLVMBuildGEP(builder, context_ptr, indices, ARRAY_SIZE(indices), "");

   if (emit_load)
      res = LLVMBuildLoad(builder, ptr, "");
   else
      res = ptr;

   lp_build_name(res, "context.sampler%u.%s", sampler_unit, member_name);

   return res;
}


#define LP_LLVM_SAMPLER_MEMBER(_name, _index, _emit_load)  \
   static LLVMValueRef \
   lp_llvm_sampler_##_name( const struct lp_sampler_dynamic_state *base, \
                            struct gallivm_state *gallivm, \
                            LLVMValueRef context_ptr, \
                            unsigned sampler_unit) \
   { \
      return lp_llvm_sampler_member(base, gallivm, context_ptr, \
                                    sampler_unit, _index, #_name, _emit_load ); \
   }


LP_LLVM_SAMPLER_MEMBER(min_lod,    LP_JIT_SAMPLER_MIN_LOD, TRUE)
LP_LLVM_SAMPLER_MEMBER(max_lod,    LP_JIT_SAMPLER_MAX_LOD, TRUE)
LP_LLVM_SAMPLER_MEMBER(lod_bias,   LP_JIT_SAMPLER_LOD_BIAS, TRUE)
LP_LLVM_SAMPLER_MEMBER(border_color, LP_JIT_SAMPLER_BORDER_COLOR, FALSE)


#if LP_USE_TEXTURE_CACHE
static LLVMValueRef
lp_llvm_texture_cache_ptr(const struct lp_sampler_dynamic_state *base,
                          struct gallivm_state *gallivm,
                          LLVMValueRef thread_data_ptr,
                          unsigned unit)
{
   /* We use the same cache for all units */
   (void)unit;

   return lp_jit_thread_data_cache(gallivm, thread_data_ptr);
}
#endif


static void
lp_llvm_sampler_soa_destroy(struct lp_build_sampler_soa *sampler)
{
   FREE(sampler);
}


/**
 * Fetch filtered values from texture.
 * The 'texel' parameter returns four vectors corresponding to R, G, B, A.
 */
static void
lp_llvm_sampler_soa_emit_fetch_texel(const struct lp_build_sampler_soa *base,
                                     struct gallivm_state *gallivm,
                                     const struct lp_sampler_params *params)
{
   struct lp_llvm_sampler_soa *sampler = (struct lp_llvm_sampler_soa *)base;
   unsigned texture_index = params->texture_index;
   unsigned sampler_index = params->sampler_index;

   assert(sampler_index < PIPE_MAX_SAMPLERS);
   assert(texture_index < PIPE_MAX_SHADER_SAMPLER_VIEWS);
   
   if (LP_PERF & PERF_NO_TEX) {
      lp_build_sample_nop(gallivm, params->type, params->coords, params->texel);
      return;
   }

   lp_build_sample_soa(&sampler->dynamic_state.static_state[texture_index].texture_state,
                       &sampler->dynamic_state.static_state[sampler_index].sampler_state,
                       &sampler->dynamic_state.base,
                       gallivm, params);
}

/**
 * Fetch the texture size.
 */
static void
lp_llvm_sampler_soa_emit_size_query(const struct lp_build_sampler_soa *base,
                                    struct gallivm_state *gallivm,
                                    const struct lp_sampler_size_query_params *params)
{
   struct lp_llvm_sampler_soa *sampler = (struct lp_llvm_sampler_soa *)base;

   assert(params->texture_unit < PIPE_MAX_SHADER_SAMPLER_VIEWS);

   lp_build_size_query_soa(gallivm,
                           &sampler->dynamic_state.static_state[params->texture_unit].texture_state,
                           &sampler->dynamic_state.base,
                           params);
}


struct lp_build_sampler_soa *
lp_llvm_sampler_soa_create(const struct lp_sampler_static_state *static_state)
{
   struct lp_llvm_sampler_soa *sampler;

   sampler = CALLOC_STRUCT(lp_llvm_sampler_soa);
   if (!sampler)
      return NULL;

   sampler->base.destroy = lp_llvm_sampler_soa_destroy;
   sampler->base.emit_tex_sample = lp_llvm_sampler_soa_emit_fetch_texel;
   sampler->base.emit_size_query = lp_llvm_sampler_soa_emit_size_query;
   sampler->dynamic_state.base.width = lp_llvm_texture_width;
   sampler->dynamic_state.base.height = lp_llvm_texture_height;
   sampler->dynamic_state.base.depth = lp_llvm_texture_depth;
   sampler->dynamic_state.base.first_level = lp_llvm_texture_first_level;
   sampler->dynamic_state.base.last_level = lp_llvm_texture_last_level;
   sampler->dynamic_state.base.base_ptr = lp_llvm_texture_base_ptr;
   sampler->dynamic_state.base.row_stride = lp_llvm_texture_row_stride;
   sampler->dynamic_state.base.img_stride = lp_llvm_texture_img_stride;
   sampler->dynamic_state.base.mip_offsets = lp_llvm_texture_mip_offsets;
   sampler->dynamic_state.base.min_lod = lp_llvm_sampler_min_lod;
   sampler->dynamic_state.base.max_lod = lp_llvm_sampler_max_lod;
   sampler->dynamic_state.base.lod_bias = lp_llvm_sampler_lod_bias;
   sampler->dynamic_state.base.border_color = lp_llvm_sampler_border_color;

#if LP_USE_TEXTURE_CACHE
   sampler->dynamic_state.base.cache_ptr = lp_llvm_texture_cache_ptr;
#endif

   sampler->dynamic_state.static_state = static_state;

   return &sampler->base;
}

