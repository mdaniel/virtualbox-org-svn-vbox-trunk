/**************************************************************************
 *
 * Copyright 2010 VMware, Inc.
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

#include "draw_llvm.h"

#include "draw_context.h"
#include "draw_vs.h"
#include "draw_gs.h"

#include "gallivm/lp_bld_arit.h"
#include "gallivm/lp_bld_arit_overflow.h"
#include "gallivm/lp_bld_bitarit.h"
#include "gallivm/lp_bld_gather.h"
#include "gallivm/lp_bld_logic.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_swizzle.h"
#include "gallivm/lp_bld_struct.h"
#include "gallivm/lp_bld_type.h"
#include "gallivm/lp_bld_flow.h"
#include "gallivm/lp_bld_debug.h"
#include "gallivm/lp_bld_tgsi.h"
#include "gallivm/lp_bld_printf.h"
#include "gallivm/lp_bld_intr.h"
#include "gallivm/lp_bld_init.h"
#include "gallivm/lp_bld_type.h"
#include "gallivm/lp_bld_pack.h"
#include "gallivm/lp_bld_format.h"

#include "tgsi/tgsi_exec.h"
#include "tgsi/tgsi_dump.h"

#include "util/u_math.h"
#include "util/u_pointer.h"
#include "util/u_string.h"
#include "util/simple_list.h"


#define DEBUG_STORE 0


static void
draw_llvm_generate(struct draw_llvm *llvm, struct draw_llvm_variant *var);


struct draw_gs_llvm_iface {
   struct lp_build_tgsi_gs_iface base;

   struct draw_gs_llvm_variant *variant;
   LLVMValueRef input;
};

static inline const struct draw_gs_llvm_iface *
draw_gs_llvm_iface(const struct lp_build_tgsi_gs_iface *iface)
{
   return (const struct draw_gs_llvm_iface *)iface;
}

/**
 * Create LLVM type for draw_vertex_buffer.
 */
static LLVMTypeRef
create_jit_dvbuffer_type(struct gallivm_state *gallivm,
                         const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef dvbuffer_type;
   LLVMTypeRef elem_types[DRAW_JIT_DVBUFFER_NUM_FIELDS];
   LLVMTypeRef int32_type = LLVMInt32TypeInContext(gallivm->context);

   elem_types[DRAW_JIT_DVBUFFER_MAP] =
      LLVMPointerType(LLVMIntTypeInContext(gallivm->context, 8), 0);
   elem_types[DRAW_JIT_DVBUFFER_SIZE] = int32_type;

   dvbuffer_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                           ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct draw_vertex_buffer, map,
                          target, dvbuffer_type,
                          DRAW_JIT_DVBUFFER_MAP);
   LP_CHECK_MEMBER_OFFSET(struct draw_vertex_buffer, size,
                          target, dvbuffer_type,
                          DRAW_JIT_DVBUFFER_SIZE);

   return dvbuffer_type;
}

/**
 * Create LLVM type for struct draw_jit_texture
 */
static LLVMTypeRef
create_jit_texture_type(struct gallivm_state *gallivm, const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef texture_type;
   LLVMTypeRef elem_types[DRAW_JIT_TEXTURE_NUM_FIELDS];
   LLVMTypeRef int32_type = LLVMInt32TypeInContext(gallivm->context);

   elem_types[DRAW_JIT_TEXTURE_WIDTH]  =
   elem_types[DRAW_JIT_TEXTURE_HEIGHT] =
   elem_types[DRAW_JIT_TEXTURE_DEPTH] =
   elem_types[DRAW_JIT_TEXTURE_FIRST_LEVEL] =
   elem_types[DRAW_JIT_TEXTURE_LAST_LEVEL] = int32_type;
   elem_types[DRAW_JIT_TEXTURE_BASE] =
      LLVMPointerType(LLVMInt8TypeInContext(gallivm->context), 0);
   elem_types[DRAW_JIT_TEXTURE_ROW_STRIDE] =
   elem_types[DRAW_JIT_TEXTURE_IMG_STRIDE] =
   elem_types[DRAW_JIT_TEXTURE_MIP_OFFSETS] =
      LLVMArrayType(int32_type, PIPE_MAX_TEXTURE_LEVELS);

   texture_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                          ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, width,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_WIDTH);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, height,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_HEIGHT);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, depth,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_DEPTH);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, first_level,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_FIRST_LEVEL);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, last_level,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_LAST_LEVEL);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, base,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_BASE);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, row_stride,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_ROW_STRIDE);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, img_stride,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_IMG_STRIDE);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_texture, mip_offsets,
                          target, texture_type,
                          DRAW_JIT_TEXTURE_MIP_OFFSETS);

   LP_CHECK_STRUCT_SIZE(struct draw_jit_texture, target, texture_type);

   return texture_type;
}


/**
 * Create LLVM type for struct draw_jit_sampler
 */
static LLVMTypeRef
create_jit_sampler_type(struct gallivm_state *gallivm, const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef sampler_type;
   LLVMTypeRef elem_types[DRAW_JIT_SAMPLER_NUM_FIELDS];

   elem_types[DRAW_JIT_SAMPLER_MIN_LOD] =
   elem_types[DRAW_JIT_SAMPLER_MAX_LOD] =
   elem_types[DRAW_JIT_SAMPLER_LOD_BIAS] = LLVMFloatTypeInContext(gallivm->context);
   elem_types[DRAW_JIT_SAMPLER_BORDER_COLOR] =
      LLVMArrayType(LLVMFloatTypeInContext(gallivm->context), 4);

   sampler_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                          ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_sampler, min_lod,
                          target, sampler_type,
                          DRAW_JIT_SAMPLER_MIN_LOD);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_sampler, max_lod,
                          target, sampler_type,
                          DRAW_JIT_SAMPLER_MAX_LOD);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_sampler, lod_bias,
                          target, sampler_type,
                          DRAW_JIT_SAMPLER_LOD_BIAS);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_sampler, border_color,
                          target, sampler_type,
                          DRAW_JIT_SAMPLER_BORDER_COLOR);

   LP_CHECK_STRUCT_SIZE(struct draw_jit_sampler, target, sampler_type);

   return sampler_type;
}


/**
 * Create LLVM type for struct draw_jit_context
 */
static LLVMTypeRef
create_jit_context_type(struct gallivm_state *gallivm,
                        LLVMTypeRef texture_type, LLVMTypeRef sampler_type,
                        const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef float_type = LLVMFloatTypeInContext(gallivm->context);
   LLVMTypeRef int_type = LLVMInt32TypeInContext(gallivm->context);
   LLVMTypeRef elem_types[DRAW_JIT_CTX_NUM_FIELDS];
   LLVMTypeRef context_type;

   elem_types[0] = LLVMArrayType(LLVMPointerType(float_type, 0), /* vs_constants */
                                 LP_MAX_TGSI_CONST_BUFFERS);
   elem_types[1] = LLVMArrayType(int_type, /* num_vs_constants */
                                 LP_MAX_TGSI_CONST_BUFFERS);
   elem_types[2] = LLVMPointerType(LLVMArrayType(LLVMArrayType(float_type, 4),
                                                 DRAW_TOTAL_CLIP_PLANES), 0);
   elem_types[3] = LLVMPointerType(float_type, 0); /* viewports */
   elem_types[4] = LLVMArrayType(texture_type,
                                 PIPE_MAX_SHADER_SAMPLER_VIEWS); /* textures */
   elem_types[5] = LLVMArrayType(sampler_type,
                                 PIPE_MAX_SAMPLERS); /* samplers */
   context_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                          ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, vs_constants,
                          target, context_type, DRAW_JIT_CTX_CONSTANTS);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, num_vs_constants,
                          target, context_type, DRAW_JIT_CTX_NUM_CONSTANTS);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, planes,
                          target, context_type, DRAW_JIT_CTX_PLANES);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, viewports,
                          target, context_type, DRAW_JIT_CTX_VIEWPORT);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, textures,
                          target, context_type,
                          DRAW_JIT_CTX_TEXTURES);
   LP_CHECK_MEMBER_OFFSET(struct draw_jit_context, samplers,
                          target, context_type,
                          DRAW_JIT_CTX_SAMPLERS);
   LP_CHECK_STRUCT_SIZE(struct draw_jit_context,
                        target, context_type);

   return context_type;
}


/**
 * Create LLVM type for struct draw_gs_jit_context
 */
static LLVMTypeRef
create_gs_jit_context_type(struct gallivm_state *gallivm,
                           unsigned vector_length,
                           LLVMTypeRef texture_type, LLVMTypeRef sampler_type,
                           const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef float_type = LLVMFloatTypeInContext(gallivm->context);
   LLVMTypeRef int_type = LLVMInt32TypeInContext(gallivm->context);
   LLVMTypeRef elem_types[DRAW_GS_JIT_CTX_NUM_FIELDS];
   LLVMTypeRef context_type;

   elem_types[0] = LLVMArrayType(LLVMPointerType(float_type, 0), /* constants */
                                 LP_MAX_TGSI_CONST_BUFFERS);
   elem_types[1] = LLVMArrayType(int_type, /* num_constants */
                                 LP_MAX_TGSI_CONST_BUFFERS);
   elem_types[2] = LLVMPointerType(LLVMArrayType(LLVMArrayType(float_type, 4),
                                                 DRAW_TOTAL_CLIP_PLANES), 0);
   elem_types[3] = LLVMPointerType(float_type, 0); /* viewports */

   elem_types[4] = LLVMArrayType(texture_type,
                                 PIPE_MAX_SHADER_SAMPLER_VIEWS); /* textures */
   elem_types[5] = LLVMArrayType(sampler_type,
                                 PIPE_MAX_SAMPLERS); /* samplers */
   
   elem_types[6] = LLVMPointerType(LLVMPointerType(int_type, 0), 0);
   elem_types[7] = LLVMPointerType(LLVMVectorType(int_type,
                                                  vector_length), 0);
   elem_types[8] = LLVMPointerType(LLVMVectorType(int_type,
                                                  vector_length), 0);

   context_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                          ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, constants,
                          target, context_type, DRAW_GS_JIT_CTX_CONSTANTS);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, num_constants,
                          target, context_type, DRAW_GS_JIT_CTX_NUM_CONSTANTS);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, planes,
                          target, context_type, DRAW_GS_JIT_CTX_PLANES);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, viewports,
                          target, context_type, DRAW_GS_JIT_CTX_VIEWPORT);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, textures,
                          target, context_type,
                          DRAW_GS_JIT_CTX_TEXTURES);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, samplers,
                          target, context_type,
                          DRAW_GS_JIT_CTX_SAMPLERS);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, prim_lengths,
                          target, context_type,
                          DRAW_GS_JIT_CTX_PRIM_LENGTHS);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, emitted_vertices,
                          target, context_type,
                          DRAW_GS_JIT_CTX_EMITTED_VERTICES);
   LP_CHECK_MEMBER_OFFSET(struct draw_gs_jit_context, emitted_prims,
                          target, context_type,
                          DRAW_GS_JIT_CTX_EMITTED_PRIMS);
   LP_CHECK_STRUCT_SIZE(struct draw_gs_jit_context,
                        target, context_type);

   return context_type;
}


static LLVMTypeRef
create_gs_jit_input_type(struct gallivm_state *gallivm)
{
   LLVMTypeRef float_type = LLVMFloatTypeInContext(gallivm->context);
   LLVMTypeRef input_array;

   input_array = LLVMVectorType(float_type, TGSI_NUM_CHANNELS); /* num primitives */
   input_array = LLVMArrayType(input_array, TGSI_NUM_CHANNELS); /* num channels */
   input_array = LLVMArrayType(input_array, PIPE_MAX_SHADER_INPUTS); /* num attrs per vertex */
   input_array = LLVMPointerType(input_array, 0); /* num vertices per prim */

   return input_array;
}

/**
 * Create LLVM type for struct pipe_vertex_buffer
 */
static LLVMTypeRef
create_jit_vertex_buffer_type(struct gallivm_state *gallivm,
                              const char *struct_name)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef elem_types[4];
   LLVMTypeRef vb_type;

   elem_types[0] = LLVMInt16TypeInContext(gallivm->context);
   elem_types[1] = LLVMInt8TypeInContext(gallivm->context);
   elem_types[2] = LLVMInt32TypeInContext(gallivm->context);
   elem_types[3] = LLVMPointerType(LLVMInt8TypeInContext(gallivm->context), 0);

   vb_type = LLVMStructTypeInContext(gallivm->context, elem_types,
                                     ARRAY_SIZE(elem_types), 0);

   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, stride,
                          target, vb_type, 0);
   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, is_user_buffer,
                          target, vb_type, 1);
   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, buffer_offset,
                          target, vb_type, 2);
   LP_CHECK_MEMBER_OFFSET(struct pipe_vertex_buffer, buffer.resource,
                          target, vb_type, 3);

   LP_CHECK_STRUCT_SIZE(struct pipe_vertex_buffer, target, vb_type);

   return vb_type;
}


/**
 * Create LLVM type for struct vertex_header;
 */
static LLVMTypeRef
create_jit_vertex_header(struct gallivm_state *gallivm, int data_elems)
{
   LLVMTargetDataRef target = gallivm->target;
   LLVMTypeRef elem_types[3];
   LLVMTypeRef vertex_header;
   char struct_name[24];

   util_snprintf(struct_name, 23, "vertex_header%d", data_elems);

   elem_types[DRAW_JIT_VERTEX_VERTEX_ID]  = LLVMIntTypeInContext(gallivm->context, 32);
   elem_types[DRAW_JIT_VERTEX_CLIP_POS]  = LLVMArrayType(LLVMFloatTypeInContext(gallivm->context), 4);
   elem_types[DRAW_JIT_VERTEX_DATA]  = LLVMArrayType(elem_types[1], data_elems);

   vertex_header = LLVMStructTypeInContext(gallivm->context, elem_types,
                                           ARRAY_SIZE(elem_types), 0);

   /* these are bit-fields and we can't take address of them
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, clipmask,
      target, vertex_header,
      DRAW_JIT_VERTEX_CLIPMASK);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, edgeflag,
      target, vertex_header,
      DRAW_JIT_VERTEX_EDGEFLAG);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, pad,
      target, vertex_header,
      DRAW_JIT_VERTEX_PAD);
      LP_CHECK_MEMBER_OFFSET(struct vertex_header, vertex_id,
      target, vertex_header,
      DRAW_JIT_VERTEX_VERTEX_ID);
   */
   (void) target; /* silence unused var warning for non-debug build */
   LP_CHECK_MEMBER_OFFSET(struct vertex_header, clip_pos,
                          target, vertex_header,
                          DRAW_JIT_VERTEX_CLIP_POS);
   LP_CHECK_MEMBER_OFFSET(struct vertex_header, data,
                          target, vertex_header,
                          DRAW_JIT_VERTEX_DATA);

   assert(LLVMABISizeOfType(target, vertex_header) ==
          offsetof(struct vertex_header, data[data_elems]));

   return vertex_header;
}


/**
 * Create LLVM types for various structures.
 */
static void
create_jit_types(struct draw_llvm_variant *variant)
{
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMTypeRef texture_type, sampler_type, context_type, buffer_type,
      vb_type;

   texture_type = create_jit_texture_type(gallivm, "texture");
   sampler_type = create_jit_sampler_type(gallivm, "sampler");

   context_type = create_jit_context_type(gallivm, texture_type, sampler_type,
                                          "draw_jit_context");
   variant->context_ptr_type = LLVMPointerType(context_type, 0);

   buffer_type = create_jit_dvbuffer_type(gallivm, "draw_vertex_buffer");
   variant->buffer_ptr_type = LLVMPointerType(buffer_type, 0);
   
   vb_type = create_jit_vertex_buffer_type(gallivm, "pipe_vertex_buffer");
   variant->vb_ptr_type = LLVMPointerType(vb_type, 0);
}


static LLVMTypeRef
get_context_ptr_type(struct draw_llvm_variant *variant)
{
   if (!variant->context_ptr_type)
      create_jit_types(variant);
   return variant->context_ptr_type;
}


static LLVMTypeRef
get_buffer_ptr_type(struct draw_llvm_variant *variant)
{
   if (!variant->buffer_ptr_type)
      create_jit_types(variant);
   return variant->buffer_ptr_type;
}


static LLVMTypeRef
get_vb_ptr_type(struct draw_llvm_variant *variant)
{
   if (!variant->vb_ptr_type)
      create_jit_types(variant);
   return variant->vb_ptr_type;
}

static LLVMTypeRef
get_vertex_header_ptr_type(struct draw_llvm_variant *variant)
{
   if (!variant->vertex_header_ptr_type)
      create_jit_types(variant);
   return variant->vertex_header_ptr_type;
}


/**
 * Create per-context LLVM info.
 */
struct draw_llvm *
draw_llvm_create(struct draw_context *draw, LLVMContextRef context)
{
   struct draw_llvm *llvm;

   if (!lp_build_init())
      return NULL;

   llvm = CALLOC_STRUCT( draw_llvm );
   if (!llvm)
      return NULL;

   llvm->draw = draw;

   llvm->context = context;
   if (!llvm->context) {
      llvm->context = LLVMContextCreate();
      llvm->context_owned = true;
   }
   if (!llvm->context)
      goto fail;

   llvm->nr_variants = 0;
   make_empty_list(&llvm->vs_variants_list);

   llvm->nr_gs_variants = 0;
   make_empty_list(&llvm->gs_variants_list);

   return llvm;

fail:
   draw_llvm_destroy(llvm);
   return NULL;
}


/**
 * Free per-context LLVM info.
 */
void
draw_llvm_destroy(struct draw_llvm *llvm)
{
   if (llvm->context_owned)
      LLVMContextDispose(llvm->context);
   llvm->context = NULL;

   /* XXX free other draw_llvm data? */
   FREE(llvm);
}


/**
 * Create LLVM-generated code for a vertex shader.
 */
struct draw_llvm_variant *
draw_llvm_create_variant(struct draw_llvm *llvm,
                         unsigned num_inputs,
                         const struct draw_llvm_variant_key *key)
{
   struct draw_llvm_variant *variant;
   struct llvm_vertex_shader *shader =
      llvm_vertex_shader(llvm->draw->vs.vertex_shader);
   LLVMTypeRef vertex_header;
   char module_name[64];

   variant = MALLOC(sizeof *variant +
                    shader->variant_key_size -
                    sizeof variant->key);
   if (!variant)
      return NULL;

   variant->llvm = llvm;
   variant->shader = shader;

   util_snprintf(module_name, sizeof(module_name), "draw_llvm_vs_variant%u",
                 variant->shader->variants_cached);

   variant->gallivm = gallivm_create(module_name, llvm->context);

   create_jit_types(variant);

   memcpy(&variant->key, key, shader->variant_key_size);

   if (gallivm_debug & (GALLIVM_DEBUG_TGSI | GALLIVM_DEBUG_IR)) {
      tgsi_dump(llvm->draw->vs.vertex_shader->state.tokens, 0);
      draw_llvm_dump_variant_key(&variant->key);
   }

   vertex_header = create_jit_vertex_header(variant->gallivm, num_inputs);

   variant->vertex_header_ptr_type = LLVMPointerType(vertex_header, 0);

   draw_llvm_generate(llvm, variant);

   gallivm_compile_module(variant->gallivm);

   variant->jit_func = (draw_jit_vert_func)
         gallivm_jit_function(variant->gallivm, variant->function);

   gallivm_free_ir(variant->gallivm);

   variant->list_item_global.base = variant;
   variant->list_item_local.base = variant;
   /*variant->no = */shader->variants_created++;
   variant->list_item_global.base = variant;

   return variant;
}


static void
generate_vs(struct draw_llvm_variant *variant,
            LLVMBuilderRef builder,
            struct lp_type vs_type,
            LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS],
            const LLVMValueRef (*inputs)[TGSI_NUM_CHANNELS],
            const struct lp_bld_tgsi_system_values *system_values,
            LLVMValueRef context_ptr,
            struct lp_build_sampler_soa *draw_sampler,
            boolean clamp_vertex_color)
{
   struct draw_llvm *llvm = variant->llvm;
   const struct tgsi_token *tokens = llvm->draw->vs.vertex_shader->state.tokens;
   LLVMValueRef consts_ptr =
      draw_jit_context_vs_constants(variant->gallivm, context_ptr);
   LLVMValueRef num_consts_ptr =
      draw_jit_context_num_vs_constants(variant->gallivm, context_ptr);

   lp_build_tgsi_soa(variant->gallivm,
                     tokens,
                     vs_type,
                     NULL /*struct lp_build_mask_context *mask*/,
                     consts_ptr,
                     num_consts_ptr,
                     system_values,
                     inputs,
                     outputs,
                     context_ptr,
                     NULL,
                     draw_sampler,
                     &llvm->draw->vs.vertex_shader->info,
                     NULL);

   {
      LLVMValueRef out;
      unsigned chan, attrib;
      struct lp_build_context bld;
      struct tgsi_shader_info* info = &llvm->draw->vs.vertex_shader->info;
      lp_build_context_init(&bld, variant->gallivm, vs_type);

      for (attrib = 0; attrib < info->num_outputs; ++attrib) {
         for (chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
            if (outputs[attrib][chan]) {
               switch (info->output_semantic_name[attrib]) {
               case TGSI_SEMANTIC_COLOR:
               case TGSI_SEMANTIC_BCOLOR:
                  if (clamp_vertex_color) {
                     out = LLVMBuildLoad(builder, outputs[attrib][chan], "");
                     out = lp_build_clamp(&bld, out, bld.zero, bld.one);
                     LLVMBuildStore(builder, out, outputs[attrib][chan]);
                  }
                  break;
               }
            }
         }
      }
   }
}


static void
fetch_instanced(struct gallivm_state *gallivm,
                const struct util_format_description *format_desc,
                struct lp_type vs_type,
                LLVMValueRef vb_stride,
                LLVMValueRef map_ptr,
                LLVMValueRef buffer_size_adj,
                LLVMValueRef *inputs,
                LLVMValueRef index)
{
   LLVMTypeRef i32_t = LLVMInt32TypeInContext(gallivm->context);
   LLVMTypeRef aosf_t, aosi_t;
   LLVMValueRef zero = LLVMConstNull(i32_t);
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef stride, buffer_overflowed, aos, index_valid;
   unsigned i;

   aosf_t = lp_build_vec_type(gallivm, lp_float32_vec4_type());
   aosi_t = lp_build_vec_type(gallivm, lp_int32_vec4_type());

   /* This mul can overflow. Wraparound is ok. */
   stride = LLVMBuildMul(builder, vb_stride, index, "");

   buffer_overflowed = LLVMBuildICmp(builder, LLVMIntUGE,
                                     stride, buffer_size_adj,
                                     "buffer_overflowed");

   if (0) {
      lp_build_print_value(gallivm, "   instance index = ", index);
      lp_build_print_value(gallivm, "   buffer overflowed = ", buffer_overflowed);
   }

   index_valid = LLVMBuildNot(builder, buffer_overflowed, "");
   index_valid = LLVMBuildSExt(builder, index_valid, i32_t, "");
   stride = LLVMBuildAnd(builder, stride, index_valid, "");

   aos = lp_build_fetch_rgba_aos(gallivm,
                                 format_desc,
                                 lp_float32_vec4_type(),
                                 FALSE,
                                 map_ptr,
                                 stride, zero, zero,
                                 NULL);

   index_valid = lp_build_broadcast(gallivm, aosi_t, index_valid);
   aos = LLVMBuildBitCast(builder, aos, aosi_t, "");
   aos = LLVMBuildAnd(builder, aos, index_valid, "");
   aos = LLVMBuildBitCast(builder, aos, aosf_t, "");

   for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
      LLVMValueRef index = lp_build_const_int32(gallivm, i);
      inputs[i] = lp_build_extract_broadcast(gallivm,
                                             lp_float32_vec4_type(),
                                             vs_type, aos, index);
   }
}


static void
fetch_vector(struct gallivm_state *gallivm,
             const struct util_format_description *format_desc,
             struct lp_type vs_type,
             LLVMValueRef vb_stride,
             LLVMValueRef map_ptr,
             LLVMValueRef buffer_size_adj,
             LLVMValueRef *inputs,
             LLVMValueRef indices)
{
   LLVMBuilderRef builder = gallivm->builder;
   struct lp_build_context blduivec;
   struct lp_type fetch_type = vs_type;
   LLVMValueRef offset, valid_mask;
   unsigned i;

   lp_build_context_init(&blduivec, gallivm, lp_uint_type(vs_type));

   vb_stride = lp_build_broadcast_scalar(&blduivec, vb_stride);
   buffer_size_adj = lp_build_broadcast_scalar(&blduivec, buffer_size_adj);

   /* This mul can overflow. Wraparound is ok. */
   offset = lp_build_mul(&blduivec, vb_stride, indices);

   valid_mask = lp_build_compare(gallivm, blduivec.type,
                                 PIPE_FUNC_LESS, offset, buffer_size_adj);

   /* not valid elements use offset 0 */
   offset = LLVMBuildAnd(builder, offset, valid_mask, "");

   if (0) {
      lp_build_print_value(gallivm, "   indices = ", indices);
      lp_build_print_value(gallivm, "   offsets = ", offset);
      lp_build_print_value(gallivm, "   valid_mask = ", valid_mask);
   }

   /*
    * Unlike fetch_instanced, use SoA fetch instead of multiple AoS fetches.
    * This should always produce better code.
    */

   /* The type handling is annoying here... */
   if (format_desc->colorspace == UTIL_FORMAT_COLORSPACE_RGB &&
       format_desc->channel[0].pure_integer) {
      if (format_desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED) {
         fetch_type = lp_type_int_vec(vs_type.width, vs_type.width * vs_type.length);
      }
      else if (format_desc->channel[0].type == UTIL_FORMAT_TYPE_UNSIGNED) {
         fetch_type = lp_type_uint_vec(vs_type.width, vs_type.width * vs_type.length);
      }
   }

   lp_build_fetch_rgba_soa(gallivm, format_desc,
                           fetch_type, FALSE, map_ptr, offset,
                           blduivec.zero, blduivec.zero,
                           NULL, inputs);

   for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
      inputs[i] = LLVMBuildBitCast(builder, inputs[i],
                                   lp_build_vec_type(gallivm, vs_type), "");
   }

   /* out-of-bound fetches return all zeros */
   for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
      inputs[i] = LLVMBuildBitCast(builder, inputs[i], blduivec.vec_type, "");
      inputs[i] = LLVMBuildAnd(builder, inputs[i], valid_mask, "");
      inputs[i] = LLVMBuildBitCast(builder, inputs[i],
                                   lp_build_vec_type(gallivm, vs_type), "");
   }
}


static void
store_aos(struct gallivm_state *gallivm,
          LLVMValueRef io_ptr,
          LLVMValueRef index,
          LLVMValueRef value)
{
   LLVMTypeRef data_ptr_type = LLVMPointerType(lp_build_vec_type(gallivm, lp_float32_vec4_type()), 0);
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef data_ptr = draw_jit_header_data(gallivm, io_ptr);
   LLVMValueRef indices[3];

   indices[0] = lp_build_const_int32(gallivm, 0);
   indices[1] = index;
   indices[2] = lp_build_const_int32(gallivm, 0);

   data_ptr = LLVMBuildGEP(builder, data_ptr, indices, 3, "");
   data_ptr = LLVMBuildPointerCast(builder, data_ptr, data_ptr_type, "");

#if DEBUG_STORE
   lp_build_printf(gallivm, "    ---- %p storing attribute %d (io = %p)\n", data_ptr, index, io_ptr);
#endif

   /* Unaligned store due to the vertex header */
   LLVMSetAlignment(LLVMBuildStore(builder, value, data_ptr), sizeof(float));
}

/**
 * Adjust the mask to architecture endianess. The mask will the store in struct:
 *
 * struct vertex_header {
 *    unsigned clipmask:DRAW_TOTAL_CLIP_PLANES;
 *    unsigned edgeflag:1;
 *    unsigned pad:1;
 *    unsigned vertex_id:16;
 *    [...]
 * }
 *
 * On little-endian machine nothing needs to done, however on bit-endian machine
 * the mask's fields need to be adjusted with the algorithm:
 *
 * uint32_t reverse (uint32_t x)
 * {
 *   return (x >> 16) |              // vertex_id
 *          ((x & 0x3fff) << 18) |   // clipmask
 *          ((x & 0x4000) << 3) |    // pad
 *          ((x & 0x8000) << 1);     // edgeflag
 * }
 */
static LLVMValueRef
adjust_mask(struct gallivm_state *gallivm,
            LLVMValueRef mask)
{
#ifdef PIPE_ARCH_BIG_ENDIAN
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef vertex_id;
   LLVMValueRef clipmask;
   LLVMValueRef pad;
   LLVMValueRef edgeflag;

   vertex_id = LLVMBuildLShr(builder, mask, lp_build_const_int32(gallivm, 16), "");
   clipmask  = LLVMBuildAnd(builder, mask, lp_build_const_int32(gallivm, 0x3fff), "");
   clipmask  = LLVMBuildShl(builder, clipmask, lp_build_const_int32(gallivm, 18), "");
   if (0) {
      pad = LLVMBuildAnd(builder, mask, lp_build_const_int32(gallivm, 0x4000), "");
      pad = LLVMBuildShl(builder, pad, lp_build_const_int32(gallivm, 3), "");
   }
   edgeflag = LLVMBuildAnd(builder, mask, lp_build_const_int32(gallivm, 0x8000), "");
   edgeflag = LLVMBuildShl(builder, edgeflag, lp_build_const_int32(gallivm, 1), "");

   mask = LLVMBuildOr(builder, vertex_id, clipmask, "");
   if (0) {
      mask = LLVMBuildOr(builder, mask, pad, "");
   }
   mask = LLVMBuildOr(builder, mask, edgeflag, "");
#endif
   return mask;
}

static void
store_aos_array(struct gallivm_state *gallivm,
                struct lp_type soa_type,
                LLVMValueRef io_ptr,
                LLVMValueRef *indices,
                LLVMValueRef* aos,
                int attrib,
                int num_outputs,
                LLVMValueRef clipmask,
                boolean need_edgeflag)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef attr_index = lp_build_const_int32(gallivm, attrib);
   LLVMValueRef inds[LP_MAX_VECTOR_WIDTH / 32];
   LLVMValueRef linear_inds[LP_MAX_VECTOR_WIDTH / 32];
   LLVMValueRef io_ptrs[LP_MAX_VECTOR_WIDTH / 32];
   int vector_length = soa_type.length;
   int i;

   debug_assert(TGSI_NUM_CHANNELS == 4);

   for (i = 0; i < vector_length; i++) {
      linear_inds[i] = lp_build_const_int32(gallivm, i);
      if (indices) {
         inds[i] = indices[i];
      } else {
         inds[i] = linear_inds[i];
      }
      io_ptrs[i] = LLVMBuildGEP(builder, io_ptr, &inds[i], 1, "");
   }

   if (attrib == 0) {
      /* store vertex header for each of the n vertices */
      LLVMValueRef val, cliptmp;
      int vertex_id_pad_edgeflag;

      /* If this assertion fails, it means we need to update the bit twidding
       * code here.  See struct vertex_header in draw_private.h.
       */
      assert(DRAW_TOTAL_CLIP_PLANES==14);
      /* initialize vertex id:16 = 0xffff, pad:1 = 0, edgeflag:1 = 1 */
      if (!need_edgeflag) {
         vertex_id_pad_edgeflag = (0xffff << 16) | (1 << DRAW_TOTAL_CLIP_PLANES);
      }
      else {
         vertex_id_pad_edgeflag = (0xffff << 16);
      }
      val = lp_build_const_int_vec(gallivm, lp_int_type(soa_type),
                                   vertex_id_pad_edgeflag);
      /* OR with the clipmask */
      cliptmp = LLVMBuildOr(builder, val, clipmask, "");
      for (i = 0; i < vector_length; i++) {
         LLVMValueRef id_ptr = draw_jit_header_id(gallivm, io_ptrs[i]);
         val = LLVMBuildExtractElement(builder, cliptmp, linear_inds[i], "");
         val = adjust_mask(gallivm, val);
#if DEBUG_STORE
         lp_build_printf(gallivm, "io = %p, index %d, clipmask = %x\n",
                         io_ptrs[i], inds[i], val);
#endif
         LLVMBuildStore(builder, val, id_ptr);
      }
   }

   /* store for each of the n vertices */
   for (i = 0; i < vector_length; i++) {
      store_aos(gallivm, io_ptrs[i], attr_index, aos[i]);
   }
}


static void
convert_to_aos(struct gallivm_state *gallivm,
               LLVMValueRef io,
               LLVMValueRef *indices,
               LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS],
               LLVMValueRef clipmask,
               int num_outputs,
               struct lp_type soa_type,
               boolean need_edgeflag)
{
   LLVMBuilderRef builder = gallivm->builder;
   unsigned chan, attrib, i;

#if DEBUG_STORE
   lp_build_printf(gallivm, "   # storing begin\n");
#endif
   for (attrib = 0; attrib < num_outputs; ++attrib) {
      LLVMValueRef soa[TGSI_NUM_CHANNELS];
      LLVMValueRef aos[LP_MAX_VECTOR_WIDTH / 32];
      for (chan = 0; chan < TGSI_NUM_CHANNELS; ++chan) {
         if (outputs[attrib][chan]) {
            LLVMValueRef out = LLVMBuildLoad(builder, outputs[attrib][chan], "");
            lp_build_name(out, "output%u.%c", attrib, "xyzw"[chan]);
#if DEBUG_STORE
            lp_build_printf(gallivm, "output %d : %d ",
                            LLVMConstInt(LLVMInt32TypeInContext(gallivm->context),
                                         attrib, 0),
                            LLVMConstInt(LLVMInt32TypeInContext(gallivm->context),
                                         chan, 0));
            lp_build_print_value(gallivm, "val = ", out);
            {
               LLVMValueRef iv =
                  LLVMBuildBitCast(builder, out, lp_build_int_vec_type(gallivm, soa_type), "");
               
               lp_build_print_value(gallivm, "  ival = ", iv);
            }
#endif
            soa[chan] = out;
         }
         else {
            soa[chan] = 0;
         }
      }


      if (soa_type.length == TGSI_NUM_CHANNELS) {
         lp_build_transpose_aos(gallivm, soa_type, soa, aos);
      } else {
         lp_build_transpose_aos(gallivm, soa_type, soa, soa);

         for (i = 0; i < soa_type.length; ++i) {
            aos[i] = lp_build_extract_range(gallivm,
                                            soa[i % TGSI_NUM_CHANNELS],
                                            (i / TGSI_NUM_CHANNELS) * TGSI_NUM_CHANNELS,
                                            TGSI_NUM_CHANNELS);
         }
      }

      store_aos_array(gallivm,
                      soa_type,
                      io, indices,
                      aos,
                      attrib,
                      num_outputs,
                      clipmask,
                      need_edgeflag);
   }
#if DEBUG_STORE
   lp_build_printf(gallivm, "   # storing end\n");
#endif
}


/**
 * Stores original vertex positions in clip coordinates
 */
static void
store_clip(struct gallivm_state *gallivm,
           const struct lp_type vs_type,
           LLVMValueRef io_ptr,
           LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS],
           int idx)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef soa[4];
   LLVMValueRef aos[LP_MAX_VECTOR_LENGTH];
   LLVMValueRef indices[2];
   LLVMValueRef io_ptrs[LP_MAX_VECTOR_WIDTH / 32];
   LLVMValueRef inds[LP_MAX_VECTOR_WIDTH / 32];
   LLVMValueRef clip_ptrs[LP_MAX_VECTOR_WIDTH / 32];
   LLVMTypeRef clip_ptr_type =
      LLVMPointerType(LLVMVectorType(LLVMFloatTypeInContext(gallivm->context),
                                     4), 0);
   int i, j;

   indices[0] =
   indices[1] = lp_build_const_int32(gallivm, 0);

   for (i = 0; i < vs_type.length; i++) {
      inds[i] = lp_build_const_int32(gallivm, i);
      io_ptrs[i] = LLVMBuildGEP(builder, io_ptr, &inds[i], 1, "");
   }

   soa[0] = LLVMBuildLoad(builder, outputs[idx][0], ""); /*x0 x1 .. xn*/
   soa[1] = LLVMBuildLoad(builder, outputs[idx][1], ""); /*y0 y1 .. yn*/
   soa[2] = LLVMBuildLoad(builder, outputs[idx][2], ""); /*z0 z1 .. zn*/
   soa[3] = LLVMBuildLoad(builder, outputs[idx][3], ""); /*w0 w1 .. wn*/

   for (i = 0; i < vs_type.length; i++) {
      clip_ptrs[i] = draw_jit_header_clip_pos(gallivm, io_ptrs[i]);
   }

   lp_build_transpose_aos(gallivm, vs_type, soa, soa);
   for (i = 0; i < vs_type.length; ++i) {
      aos[i] = lp_build_extract_range(gallivm,
                                      soa[i % TGSI_NUM_CHANNELS],
                                      (i / TGSI_NUM_CHANNELS) * TGSI_NUM_CHANNELS,
                                      TGSI_NUM_CHANNELS);
   }

   for (j = 0; j < vs_type.length; j++) {
      LLVMValueRef clip_ptr;

      clip_ptr = LLVMBuildGEP(builder, clip_ptrs[j], indices, 2, "clipo");
      clip_ptr = LLVMBuildPointerCast(builder, clip_ptr, clip_ptr_type, "");

      /* Unaligned store */
      LLVMSetAlignment(LLVMBuildStore(builder, aos[j], clip_ptr), sizeof(float));
   }
}


/**
 * Transforms the outputs for viewport mapping
 */
static void
generate_viewport(struct draw_llvm_variant *variant,
                  LLVMBuilderRef builder,
                  struct lp_type vs_type,
                  LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS],
                  LLVMValueRef context_ptr)
{
   int i;
   struct gallivm_state *gallivm = variant->gallivm;
   struct lp_type f32_type = vs_type;
   const unsigned pos = variant->llvm->draw->vs.position_output;
   LLVMTypeRef vs_type_llvm = lp_build_vec_type(gallivm, vs_type);
   LLVMValueRef out3 = LLVMBuildLoad(builder, outputs[pos][3], ""); /*w0 w1 .. wn*/
   LLVMValueRef const1 = lp_build_const_vec(gallivm, f32_type, 1.0);       /*1.0 1.0 1.0 1.0*/
   LLVMValueRef vp_ptr = draw_jit_context_viewports(gallivm, context_ptr);

   /* We treat pipe_viewport_state as a float array */
   const int scale_index_offset = offsetof(struct pipe_viewport_state, scale) / sizeof(float);
   const int trans_index_offset = offsetof(struct pipe_viewport_state, translate) / sizeof(float);

   /* for 1/w convention*/
   out3 = LLVMBuildFDiv(builder, const1, out3, "");
   LLVMBuildStore(builder, out3, outputs[pos][3]);

   /* Viewport Mapping */
   for (i=0; i<3; i++) {
      LLVMValueRef out = LLVMBuildLoad(builder, outputs[pos][i], ""); /*x0 x1 .. xn*/
      LLVMValueRef scale;
      LLVMValueRef trans;
      LLVMValueRef scale_i;
      LLVMValueRef trans_i;
      LLVMValueRef index;

      index = lp_build_const_int32(gallivm, i + scale_index_offset);
      scale_i = LLVMBuildGEP(builder, vp_ptr, &index, 1, "");

      index = lp_build_const_int32(gallivm, i + trans_index_offset);
      trans_i = LLVMBuildGEP(builder, vp_ptr, &index, 1, "");

      scale = lp_build_broadcast(gallivm, vs_type_llvm,
                                 LLVMBuildLoad(builder, scale_i, "scale"));
      trans = lp_build_broadcast(gallivm, vs_type_llvm,
                                 LLVMBuildLoad(builder, trans_i, "trans"));

      /* divide by w */
      out = LLVMBuildFMul(builder, out, out3, "");
      /* mult by scale, add translation */
      out = lp_build_fmuladd(builder, out, scale, trans);

      /* store transformed outputs */
      LLVMBuildStore(builder, out, outputs[pos][i]);
   }

}


/**
 * Returns clipmask as nxi32 bitmask for the n vertices
 */
static LLVMValueRef
generate_clipmask(struct draw_llvm *llvm,
                  struct gallivm_state *gallivm,
                  struct lp_type vs_type,
                  LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS],
                  struct draw_llvm_variant_key *key,
                  LLVMValueRef context_ptr,
                  boolean *have_clipdist)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef mask; /* stores the <nxi32> clipmasks */
   LLVMValueRef test, temp;
   LLVMValueRef zero, shift;
   LLVMValueRef pos_x, pos_y, pos_z, pos_w;
   LLVMValueRef cv_x, cv_y, cv_z, cv_w;
   LLVMValueRef plane1, planes, plane_ptr, sum;
   struct lp_type f32_type = vs_type;
   struct lp_type i32_type = lp_int_type(vs_type);
   const unsigned pos = llvm->draw->vs.position_output;
   const unsigned cv = llvm->draw->vs.clipvertex_output;
   int num_written_clipdistance = llvm->draw->vs.vertex_shader->info.num_written_clipdistance;
   boolean have_cd = false;
   boolean clip_user = key->clip_user;
   unsigned ucp_enable = key->ucp_enable;
   unsigned cd[2];

   cd[0] = llvm->draw->vs.ccdistance_output[0];
   cd[1] = llvm->draw->vs.ccdistance_output[1];

   if (cd[0] != pos || cd[1] != pos)
      have_cd = true;

   if (num_written_clipdistance && !clip_user) {
      clip_user = true;
      ucp_enable = (1 << num_written_clipdistance) - 1;
   }

   mask = lp_build_const_int_vec(gallivm, i32_type, 0);
   temp = lp_build_const_int_vec(gallivm, i32_type, 0);
   zero = lp_build_const_vec(gallivm, f32_type, 0);         /* 0.0f 0.0f 0.0f 0.0f */
   shift = lp_build_const_int_vec(gallivm, i32_type, 1);    /* 1 1 1 1 */

   /*
    * load clipvertex and position from correct locations.
    * if they are the same just load them once.
    */
   pos_x = LLVMBuildLoad(builder, outputs[pos][0], ""); /*x0 x1 .. xn */
   pos_y = LLVMBuildLoad(builder, outputs[pos][1], ""); /*y0 y1 .. yn */
   pos_z = LLVMBuildLoad(builder, outputs[pos][2], ""); /*z0 z1 .. zn */
   pos_w = LLVMBuildLoad(builder, outputs[pos][3], ""); /*w0 w1 .. wn */

   if (clip_user && cv != pos) {
      cv_x = LLVMBuildLoad(builder, outputs[cv][0], ""); /*x0 x1 .. xn */
      cv_y = LLVMBuildLoad(builder, outputs[cv][1], ""); /*y0 y1 .. yn */
      cv_z = LLVMBuildLoad(builder, outputs[cv][2], ""); /*z0 z1 .. zn */
      cv_w = LLVMBuildLoad(builder, outputs[cv][3], ""); /*w0 w1 .. wn */
   } else {
      cv_x = pos_x;
      cv_y = pos_y;
      cv_z = pos_z;
      cv_w = pos_w;
   }

   /*
    * Be careful with the comparisons and NaNs (using llvm's unordered
    * comparisons here).
    */
   /* Cliptest, for hardwired planes */
   /*
    * XXX should take guardband into account (currently not in key).
    * Otherwise might run the draw pipeline stages for nothing.
    */
   if (key->clip_xy) {
      /* plane 1 */
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, pos_x , pos_w);
      temp = shift;
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = test;

      /* plane 2 */
      test = LLVMBuildFAdd(builder, pos_x, pos_w, "");
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, test);
      temp = LLVMBuildShl(builder, temp, shift, "");
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = LLVMBuildOr(builder, mask, test, "");

      /* plane 3 */
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, pos_y, pos_w);
      temp = LLVMBuildShl(builder, temp, shift, "");
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = LLVMBuildOr(builder, mask, test, "");

      /* plane 4 */
      test = LLVMBuildFAdd(builder, pos_y, pos_w, "");
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, test);
      temp = LLVMBuildShl(builder, temp, shift, "");
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = LLVMBuildOr(builder, mask, test, "");
   }

   if (key->clip_z) {
      temp = lp_build_const_int_vec(gallivm, i32_type, 16);
      if (key->clip_halfz) {
         /* plane 5 */
         test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, pos_z);
         test = LLVMBuildAnd(builder, test, temp, "");
         mask = LLVMBuildOr(builder, mask, test, "");
      }
      else {
         /* plane 5 */
         test = LLVMBuildFAdd(builder, pos_z, pos_w, "");
         test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, test);
         test = LLVMBuildAnd(builder, test, temp, "");
         mask = LLVMBuildOr(builder, mask, test, "");
      }
      /* plane 6 */
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, pos_z, pos_w);
      temp = LLVMBuildShl(builder, temp, shift, "");
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = LLVMBuildOr(builder, mask, test, "");
   }

   if (clip_user) {
      LLVMValueRef planes_ptr = draw_jit_context_planes(gallivm, context_ptr);
      LLVMValueRef indices[3];
      LLVMValueRef is_nan_or_inf;

      /* userclip planes */
      while (ucp_enable) {
         unsigned plane_idx = ffs(ucp_enable)-1;
         ucp_enable &= ~(1 << plane_idx);
         plane_idx += 6;

         if (have_cd && num_written_clipdistance) {
            LLVMValueRef clipdist;
            int i;
            i = plane_idx - 6;

            *have_clipdist = TRUE;
            if (i < 4) {
               clipdist = LLVMBuildLoad(builder, outputs[cd[0]][i], "");
            } else {
               clipdist = LLVMBuildLoad(builder, outputs[cd[1]][i-4], "");
            }
            test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, clipdist);
            is_nan_or_inf = lp_build_is_inf_or_nan(gallivm, vs_type, clipdist);
            test = LLVMBuildOr(builder, test, is_nan_or_inf, "");
            temp = lp_build_const_int_vec(gallivm, i32_type, 1LL << plane_idx);
            test = LLVMBuildAnd(builder, test, temp, "");
            mask = LLVMBuildOr(builder, mask, test, "");
         } else {
            LLVMTypeRef vs_type_llvm = lp_build_vec_type(gallivm, vs_type);
            indices[0] = lp_build_const_int32(gallivm, 0);
            indices[1] = lp_build_const_int32(gallivm, plane_idx);

            indices[2] = lp_build_const_int32(gallivm, 0);
            plane_ptr = LLVMBuildGEP(builder, planes_ptr, indices, 3, "");
            plane1 = LLVMBuildLoad(builder, plane_ptr, "plane_x");
            planes = lp_build_broadcast(gallivm, vs_type_llvm, plane1);
            sum = LLVMBuildFMul(builder, planes, cv_x, "");

            indices[2] = lp_build_const_int32(gallivm, 1);
            plane_ptr = LLVMBuildGEP(builder, planes_ptr, indices, 3, "");
            plane1 = LLVMBuildLoad(builder, plane_ptr, "plane_y");
            planes = lp_build_broadcast(gallivm, vs_type_llvm, plane1);
            sum = lp_build_fmuladd(builder, planes, cv_y, sum);

            indices[2] = lp_build_const_int32(gallivm, 2);
            plane_ptr = LLVMBuildGEP(builder, planes_ptr, indices, 3, "");
            plane1 = LLVMBuildLoad(builder, plane_ptr, "plane_z");
            planes = lp_build_broadcast(gallivm, vs_type_llvm, plane1);
            sum = lp_build_fmuladd(builder, planes, cv_z, sum);

            indices[2] = lp_build_const_int32(gallivm, 3);
            plane_ptr = LLVMBuildGEP(builder, planes_ptr, indices, 3, "");
            plane1 = LLVMBuildLoad(builder, plane_ptr, "plane_w");
            planes = lp_build_broadcast(gallivm, vs_type_llvm, plane1);
            sum = lp_build_fmuladd(builder, planes, cv_w, sum);

            test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_GREATER, zero, sum);
            temp = lp_build_const_int_vec(gallivm, i32_type, 1LL << plane_idx);
            test = LLVMBuildAnd(builder, test, temp, "");
            mask = LLVMBuildOr(builder, mask, test, "");
         }
      }
   }
   if (key->need_edgeflags) {
      /*
       * This isn't really part of clipmask but stored the same in vertex
       * header later, so do it here.
       */
      unsigned edge_attr = llvm->draw->vs.edgeflag_output;
      LLVMValueRef one = lp_build_const_vec(gallivm, f32_type, 1.0);
      LLVMValueRef edgeflag = LLVMBuildLoad(builder, outputs[edge_attr][0], "");
      test = lp_build_compare(gallivm, f32_type, PIPE_FUNC_EQUAL, one, edgeflag);
      temp = lp_build_const_int_vec(gallivm, i32_type,
                                    1LL << DRAW_TOTAL_CLIP_PLANES);
      test = LLVMBuildAnd(builder, test, temp, "");
      mask = LLVMBuildOr(builder, mask, test, "");
   }
   return mask;
}


/**
 * Returns boolean if any clipping has occurred
 * Used zero/one i8 value to represent boolean
 */
static LLVMValueRef
clipmask_booli8(struct gallivm_state *gallivm,
                const struct lp_type vs_type,
                LLVMValueRef clipmask_bool_ptr,
                boolean edgeflag_in_clipmask)
{
   LLVMBuilderRef builder = gallivm->builder;
   LLVMTypeRef int8_type = LLVMInt8TypeInContext(gallivm->context);
   LLVMValueRef clipmask_bool = LLVMBuildLoad(builder, clipmask_bool_ptr, "");
   LLVMValueRef ret;
   struct lp_build_context bldivec;

   lp_build_context_init(&bldivec, gallivm, lp_int_type(vs_type));

   /*
    * We need to invert the edgeflag bit from the clipmask here
    * (because the result is really if we want to run the pipeline or not
    * and we (may) need it if edgeflag was 0).
    */
   if (edgeflag_in_clipmask) {
      LLVMValueRef edge = lp_build_const_int_vec(gallivm, bldivec.type,
                                                 1LL << DRAW_TOTAL_CLIP_PLANES);
      clipmask_bool = LLVMBuildXor(builder, clipmask_bool, edge, "");
   }

   /*
    * XXX: probably should mask off bits from the mask which come from
    * vertices which were beyond the count (i.e. indices_valid for
    * linear fetches, for elts ones we don't have the correct mask
    * right now). Otherwise might run the pipeline for nothing,
    * though everything should still work.
    */
   ret = lp_build_any_true_range(&bldivec, vs_type.length, clipmask_bool);
   ret = LLVMBuildZExt(builder, ret, int8_type, "");
   return ret;
}

static LLVMValueRef
draw_gs_llvm_fetch_input(const struct lp_build_tgsi_gs_iface *gs_iface,
                         struct lp_build_tgsi_context * bld_base,
                         boolean is_vindex_indirect,
                         LLVMValueRef vertex_index,
                         boolean is_aindex_indirect,
                         LLVMValueRef attrib_index,
                         LLVMValueRef swizzle_index)
{
   const struct draw_gs_llvm_iface *gs = draw_gs_llvm_iface(gs_iface);
   struct gallivm_state *gallivm = bld_base->base.gallivm;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef indices[3];
   LLVMValueRef res;
   struct lp_type type = bld_base->base.type;

   if (is_vindex_indirect || is_aindex_indirect) {
      int i;
      res = bld_base->base.zero;
      for (i = 0; i < type.length; ++i) {
         LLVMValueRef idx = lp_build_const_int32(gallivm, i);
         LLVMValueRef vert_chan_index = vertex_index;
         LLVMValueRef attr_chan_index = attrib_index;
         LLVMValueRef channel_vec, value;

         if (is_vindex_indirect) {
            vert_chan_index = LLVMBuildExtractElement(builder,
                                                      vertex_index, idx, "");
         }
         if (is_aindex_indirect) {
            attr_chan_index = LLVMBuildExtractElement(builder,
                                                      attrib_index, idx, "");
         }

         indices[0] = vert_chan_index;
         indices[1] = attr_chan_index;
         indices[2] = swizzle_index;

         channel_vec = LLVMBuildGEP(builder, gs->input, indices, 3, "");
         channel_vec = LLVMBuildLoad(builder, channel_vec, "");
         value = LLVMBuildExtractElement(builder, channel_vec, idx, "");

         res = LLVMBuildInsertElement(builder, res, value, idx, "");
      }
   } else {
      indices[0] = vertex_index;
      indices[1] = attrib_index;
      indices[2] = swizzle_index;

      res = LLVMBuildGEP(builder, gs->input, indices, 3, "");
      res = LLVMBuildLoad(builder, res, "");
   }

   return res;
}

static void
draw_gs_llvm_emit_vertex(const struct lp_build_tgsi_gs_iface *gs_base,
                         struct lp_build_tgsi_context * bld_base,
                         LLVMValueRef (*outputs)[4],
                         LLVMValueRef emitted_vertices_vec)
{
   const struct draw_gs_llvm_iface *gs_iface = draw_gs_llvm_iface(gs_base);
   struct draw_gs_llvm_variant *variant = gs_iface->variant;
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMBuilderRef builder = gallivm->builder;
   struct lp_type gs_type = bld_base->base.type;
   LLVMValueRef clipmask = lp_build_const_int_vec(gallivm,
                                                  lp_int_type(gs_type), 0);
   LLVMValueRef indices[LP_MAX_VECTOR_LENGTH];
   LLVMValueRef next_prim_offset =
      lp_build_const_int32(gallivm, variant->shader->base.primitive_boundary);
   LLVMValueRef io = variant->io_ptr;
   unsigned i;
   const struct tgsi_shader_info *gs_info = &variant->shader->base.info;

   for (i = 0; i < gs_type.length; ++i) {
      LLVMValueRef ind = lp_build_const_int32(gallivm, i);
      LLVMValueRef currently_emitted =
         LLVMBuildExtractElement(builder, emitted_vertices_vec, ind, "");
      indices[i] = LLVMBuildMul(builder, ind, next_prim_offset, "");
      indices[i] = LLVMBuildAdd(builder, indices[i], currently_emitted, "");
   }

   convert_to_aos(gallivm, io, indices,
                  outputs, clipmask,
                  gs_info->num_outputs, gs_type,
                  FALSE);
}

static void
draw_gs_llvm_end_primitive(const struct lp_build_tgsi_gs_iface *gs_base,
                           struct lp_build_tgsi_context * bld_base,
                           LLVMValueRef verts_per_prim_vec,
                           LLVMValueRef emitted_prims_vec)
{
   const struct draw_gs_llvm_iface *gs_iface = draw_gs_llvm_iface(gs_base);
   struct draw_gs_llvm_variant *variant = gs_iface->variant;
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef prim_lengts_ptr =
      draw_gs_jit_prim_lengths(variant->gallivm, variant->context_ptr);
   unsigned i;

   for (i = 0; i < bld_base->base.type.length; ++i) {
      LLVMValueRef ind = lp_build_const_int32(gallivm, i);
      LLVMValueRef prims_emitted =
         LLVMBuildExtractElement(builder, emitted_prims_vec, ind, "");
      LLVMValueRef store_ptr;
      LLVMValueRef num_vertices =
         LLVMBuildExtractElement(builder, verts_per_prim_vec, ind, "");

      store_ptr = LLVMBuildGEP(builder, prim_lengts_ptr, &prims_emitted, 1, "");
      store_ptr = LLVMBuildLoad(builder, store_ptr, "");
      store_ptr = LLVMBuildGEP(builder, store_ptr, &ind, 1, "");
      LLVMBuildStore(builder, num_vertices, store_ptr);
   }
}

static void
draw_gs_llvm_epilogue(const struct lp_build_tgsi_gs_iface *gs_base,
                      struct lp_build_tgsi_context * bld_base,
                      LLVMValueRef total_emitted_vertices_vec,
                      LLVMValueRef emitted_prims_vec)
{
   const struct draw_gs_llvm_iface *gs_iface = draw_gs_llvm_iface(gs_base);
   struct draw_gs_llvm_variant *variant = gs_iface->variant;
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMValueRef emitted_verts_ptr =
      draw_gs_jit_emitted_vertices(gallivm, variant->context_ptr);
   LLVMValueRef emitted_prims_ptr =
      draw_gs_jit_emitted_prims(gallivm, variant->context_ptr);
   LLVMValueRef zero = lp_build_const_int32(gallivm, 0);
   
   emitted_verts_ptr = LLVMBuildGEP(builder, emitted_verts_ptr, &zero, 0, "");
   emitted_prims_ptr = LLVMBuildGEP(builder, emitted_prims_ptr, &zero, 0, "");

   LLVMBuildStore(builder, total_emitted_vertices_vec, emitted_verts_ptr);
   LLVMBuildStore(builder, emitted_prims_vec, emitted_prims_ptr);
}

static void
draw_llvm_generate(struct draw_llvm *llvm, struct draw_llvm_variant *variant)
{
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMContextRef context = gallivm->context;
   LLVMTypeRef int32_type = LLVMInt32TypeInContext(context);
   LLVMTypeRef arg_types[11];
   unsigned num_arg_types = ARRAY_SIZE(arg_types);
   LLVMTypeRef func_type;
   LLVMValueRef context_ptr;
   LLVMBasicBlockRef block;
   LLVMBuilderRef builder;
   char func_name[64];
   struct lp_type vs_type;
   LLVMValueRef count, fetch_elts, start_or_maxelt;
   LLVMValueRef vertex_id_offset, start_instance;
   LLVMValueRef stride, step, io_itr;
   LLVMValueRef ind_vec, start_vec, have_elts, fetch_max, tmp;
   LLVMValueRef io_ptr, vbuffers_ptr, vb_ptr;
   LLVMValueRef vb_stride[PIPE_MAX_ATTRIBS];
   LLVMValueRef map_ptr[PIPE_MAX_ATTRIBS];
   LLVMValueRef buffer_size_adj[PIPE_MAX_ATTRIBS];
   LLVMValueRef instance_index[PIPE_MAX_ATTRIBS];
   LLVMValueRef fake_buf_ptr, fake_buf;

   struct draw_context *draw = llvm->draw;
   const struct tgsi_shader_info *vs_info = &draw->vs.vertex_shader->info;
   unsigned i, j;
   struct lp_build_context bld, blduivec;
   struct lp_build_loop_state lp_loop;
   struct lp_build_if_state if_ctx;
   const int vector_length = lp_native_vector_width / 32;
   LLVMValueRef outputs[PIPE_MAX_SHADER_OUTPUTS][TGSI_NUM_CHANNELS];
   struct lp_build_sampler_soa *sampler = 0;
   LLVMValueRef ret, clipmask_bool_ptr;
   struct draw_llvm_variant_key *key = &variant->key;
   /* If geometry shader is present we need to skip both the viewport
    * transformation and clipping otherwise the inputs to the geometry
    * shader will be incorrect.
    * The code can't handle vp transform when vs writes vp index neither
    * (though this would be fixable here, but couldn't just broadcast
    * the values).
    */
   const boolean bypass_viewport = key->has_gs || key->bypass_viewport ||
                                   vs_info->writes_viewport_index;
   const boolean enable_cliptest = !key->has_gs && (key->clip_xy ||
                                                    key->clip_z ||
                                                    key->clip_user ||
                                                    key->need_edgeflags);
   LLVMValueRef variant_func;
   const unsigned pos = draw->vs.position_output;
   const unsigned cv = draw->vs.clipvertex_output;
   boolean have_clipdist = FALSE;
   struct lp_bld_tgsi_system_values system_values;

   memset(&system_values, 0, sizeof(system_values));

   util_snprintf(func_name, sizeof(func_name), "draw_llvm_vs_variant%u",
                 variant->shader->variants_cached);

   i = 0;
   arg_types[i++] = get_context_ptr_type(variant);       /* context */
   arg_types[i++] = get_vertex_header_ptr_type(variant); /* vertex_header */
   arg_types[i++] = get_buffer_ptr_type(variant);        /* vbuffers */
   arg_types[i++] = int32_type;                          /* count */
   arg_types[i++] = int32_type;                          /* start/fetch_elt_max */
   arg_types[i++] = int32_type;                          /* stride */
   arg_types[i++] = get_vb_ptr_type(variant);            /* pipe_vertex_buffer's */
   arg_types[i++] = int32_type;                          /* instance_id */
   arg_types[i++] = int32_type;                          /* vertex_id_offset */
   arg_types[i++] = int32_type;                          /* start_instance */
   arg_types[i++] = LLVMPointerType(int32_type, 0);      /* fetch_elts  */

   func_type = LLVMFunctionType(LLVMInt8TypeInContext(context),
                                arg_types, num_arg_types, 0);

   variant_func = LLVMAddFunction(gallivm->module, func_name, func_type);
   variant->function = variant_func;

   LLVMSetFunctionCallConv(variant_func, LLVMCCallConv);
   for (i = 0; i < num_arg_types; ++i)
      if (LLVMGetTypeKind(arg_types[i]) == LLVMPointerTypeKind)
         lp_add_function_attr(variant_func, i + 1, LP_FUNC_ATTR_NOALIAS);

   context_ptr               = LLVMGetParam(variant_func, 0);
   io_ptr                    = LLVMGetParam(variant_func, 1);
   vbuffers_ptr              = LLVMGetParam(variant_func, 2);
   count                     = LLVMGetParam(variant_func, 3);
   /*
    * XXX: the maxelt part is unused. Not really useful, since we cannot
    * get index buffer overflows due to vsplit (which provides its own
    * elts buffer, with a different size than what's passed in here).
    */
   start_or_maxelt           = LLVMGetParam(variant_func, 4);
   /*
    * XXX: stride is actually unused. The stride we use is strictly calculated
    * from the number of outputs (including the draw_extra outputs).
    * Should probably fix some day (we need a new vs just because of extra
    * outputs which the generated vs won't touch).
    */
   stride                    = LLVMGetParam(variant_func, 5);
   vb_ptr                    = LLVMGetParam(variant_func, 6);
   system_values.instance_id = LLVMGetParam(variant_func, 7);
   vertex_id_offset          = LLVMGetParam(variant_func, 8);
   start_instance            = LLVMGetParam(variant_func, 9);
   fetch_elts                = LLVMGetParam(variant_func, 10);

   lp_build_name(context_ptr, "context");
   lp_build_name(io_ptr, "io");
   lp_build_name(vbuffers_ptr, "vbuffers");
   lp_build_name(count, "count");
   lp_build_name(start_or_maxelt, "start_or_maxelt");
   lp_build_name(stride, "stride");
   lp_build_name(vb_ptr, "vb");
   lp_build_name(system_values.instance_id, "instance_id");
   lp_build_name(vertex_id_offset, "vertex_id_offset");
   lp_build_name(start_instance, "start_instance");
   lp_build_name(fetch_elts, "fetch_elts");

   /*
    * Function body
    */

   block = LLVMAppendBasicBlockInContext(gallivm->context, variant_func, "entry");
   builder = gallivm->builder;
   LLVMPositionBuilderAtEnd(builder, block);

   memset(&vs_type, 0, sizeof vs_type);
   vs_type.floating = TRUE; /* floating point values */
   vs_type.sign = TRUE;     /* values are signed */
   vs_type.norm = FALSE;    /* values are not limited to [0,1] or [-1,1] */
   vs_type.width = 32;      /* 32-bit float */
   vs_type.length = vector_length;

   lp_build_context_init(&bld, gallivm, lp_type_uint(32));
   lp_build_context_init(&blduivec, gallivm, lp_uint_type(vs_type));

   /* hold temporary "bool" clipmask */
   clipmask_bool_ptr = lp_build_alloca(gallivm, blduivec.vec_type, "");

   fake_buf = lp_build_alloca_undef(gallivm,
                 LLVMVectorType(LLVMInt64TypeInContext(context), 4), "");
   fake_buf = LLVMBuildBitCast(builder, fake_buf,
                 LLVMPointerType(LLVMInt8TypeInContext(context), 0), "");
   fake_buf_ptr = LLVMBuildGEP(builder, fake_buf, &bld.zero, 1, "");

   /* code generated texture sampling */
   sampler = draw_llvm_sampler_soa_create(draw_llvm_variant_key_samplers(key));

   step = lp_build_const_int32(gallivm, vector_length);

   ind_vec = blduivec.undef;
   for (i = 0; i < vs_type.length; i++) {
      LLVMValueRef index = lp_build_const_int32(gallivm, i);
      ind_vec = LLVMBuildInsertElement(builder, ind_vec, index, index, "");
   }

   have_elts = LLVMBuildICmp(builder, LLVMIntNE,
                             LLVMConstPointerNull(arg_types[10]), fetch_elts, "");

   fetch_max = LLVMBuildSub(builder, count, bld.one, "fetch_max");
   fetch_max = lp_build_broadcast_scalar(&blduivec, fetch_max);
   /*
    * Only needed for non-indexed path.
    */
   start_vec = lp_build_broadcast_scalar(&blduivec, start_or_maxelt);

   /*
    * Pre-calculate everything which is constant per shader invocation.
    */
   for (j = 0; j < key->nr_vertex_elements; ++j) {
      LLVMValueRef vb_buffer_offset, buffer_size, temp_ptr;
      LLVMValueRef vb_info, vbuffer_ptr, buf_offset, ofbit;
      struct pipe_vertex_element *velem = &key->vertex_element[j];
      LLVMValueRef vb_index =
         lp_build_const_int32(gallivm, velem->vertex_buffer_index);
      LLVMValueRef bsize = lp_build_const_int32(gallivm,
                                                util_format_get_blocksize(velem->src_format));
      LLVMValueRef src_offset = lp_build_const_int32(gallivm,
                                                     velem->src_offset);
      struct lp_build_if_state if_ctx;

      if (velem->src_format != PIPE_FORMAT_NONE) {
         vbuffer_ptr = LLVMBuildGEP(builder, vbuffers_ptr, &vb_index, 1, "");
         vb_info = LLVMBuildGEP(builder, vb_ptr, &vb_index, 1, "");
         vb_stride[j] = draw_jit_vbuffer_stride(gallivm, vb_info);
         vb_stride[j] = LLVMBuildZExt(gallivm->builder, vb_stride[j],
                                      LLVMInt32TypeInContext(context), "");
         vb_buffer_offset = draw_jit_vbuffer_offset(gallivm, vb_info);
         map_ptr[j] = draw_jit_dvbuffer_map(gallivm, vbuffer_ptr);
         buffer_size = draw_jit_dvbuffer_size(gallivm, vbuffer_ptr);

         ofbit = NULL;
         /*
          * We'll set buffer_size_adj to zero if we have of, so it will
          * always overflow later automatically without having to keep ofbit.
          * Overflows (with normal wraparound) doing the actual offset
          * calculation should be ok, just not for the buffer size calc.
          * It would also be possible to detect such overflows and return
          * zeros if that happens, but this would be more complex.
          */
         buf_offset = lp_build_add(&bld, vb_buffer_offset, src_offset);
         tmp = lp_build_sub(&bld, bsize, bld.one);
         buffer_size_adj[j] = lp_build_usub_overflow(gallivm, buffer_size, tmp,
                                                     &ofbit);
         buffer_size_adj[j] = lp_build_usub_overflow(gallivm, buffer_size_adj[j],
                                                     buf_offset, &ofbit);

         /*
          * We can't easily set fake vertex buffers outside the generated code.
          * Hence, set fake vertex buffers here instead basically, so fetch
          * code can always fetch using offset 0, eliminating all control flow
          * inside the main loop.
          * (Alternatively, could have control flow per vector skipping fetch
          * if ofbit is true.)
          */
         if (velem->instance_divisor) {
            /*
             * Index is equal to the start instance plus the number of current
             * instance divided by the divisor. In this case we compute it as:
             * index = start_instance + (instance_id  / divisor).
             * Note we could actually do the fetch here, outside the loop -
             * it's all constant, hopefully llvm recognizes this.
             */
            LLVMValueRef current_instance;
            current_instance = LLVMBuildUDiv(builder, system_values.instance_id,
                                             lp_build_const_int32(gallivm,
                                                                  velem->instance_divisor),
                                             "instance_divisor");
            instance_index[j] = lp_build_uadd_overflow(gallivm, start_instance,
                                                       current_instance, &ofbit);
         }

         buffer_size_adj[j] = LLVMBuildSelect(builder, ofbit, bld.zero,
                                              buffer_size_adj[j], "");

         temp_ptr = lp_build_alloca_undef(gallivm,
                       LLVMPointerType(LLVMInt8TypeInContext(context), 0), "");

         lp_build_if(&if_ctx, gallivm, ofbit);
         {
            LLVMBuildStore(builder, fake_buf_ptr, temp_ptr);
         }
         lp_build_else(&if_ctx);
         {
            map_ptr[j] = LLVMBuildGEP(builder, map_ptr[j], &buf_offset, 1, "");
            LLVMBuildStore(builder, map_ptr[j], temp_ptr);
         }
         lp_build_endif(&if_ctx);
         map_ptr[j] = LLVMBuildLoad(builder, temp_ptr, "map_ptr");

         if (0) {
            lp_build_printf(gallivm, "velem %d, vbuf index = %u, vb_stride = %u\n",
                            lp_build_const_int32(gallivm, j),
                            vb_index, vb_stride[j]);
            lp_build_printf(gallivm,
                            "   vb_buffer_offset = %u, src_offset = %u, buf_offset = %u\n",
                            vb_buffer_offset, src_offset, buf_offset);
            lp_build_printf(gallivm, "   buffer size = %u, blocksize = %u\n",
                            buffer_size, bsize);
            lp_build_printf(gallivm, "   instance_id = %u\n", system_values.instance_id);
         }
      }
   }

   lp_build_loop_begin(&lp_loop, gallivm, bld.zero);
   {
      LLVMValueRef inputs[PIPE_MAX_SHADER_INPUTS][TGSI_NUM_CHANNELS];
      LLVMValueRef io;
      LLVMValueRef clipmask;   /* holds the clipmask value */
      LLVMValueRef true_index_array, index_store;
      const LLVMValueRef (*ptr_aos)[TGSI_NUM_CHANNELS];

      io_itr = lp_loop.counter;

      io = LLVMBuildGEP(builder, io_ptr, &io_itr, 1, "");
#if DEBUG_STORE
      lp_build_printf(gallivm, " --- io %d = %p, loop counter %d\n",
                      io_itr, io, lp_loop.counter);
#endif

      true_index_array = lp_build_broadcast_scalar(&blduivec, lp_loop.counter);
      true_index_array = LLVMBuildAdd(builder, true_index_array, ind_vec, "");

      /*
       * Limit indices to fetch_max, otherwise might try to access indices
       * beyond index buffer (or rather vsplit elt buffer) size.
       * Could probably safely (?) skip this for non-indexed draws and
       * simplify things minimally (by removing it could combine the ind_vec
       * and start_vec adds). I think the only effect for non-indexed draws will
       * be that for the invalid elements they will be all fetched from the
       * same location as the last valid one, but noone should really care.
       */
      true_index_array = lp_build_min(&blduivec, true_index_array, fetch_max);

      index_store = lp_build_alloca_undef(gallivm, blduivec.vec_type, "index_store");

      lp_build_if(&if_ctx, gallivm, have_elts);
      {
         /*
          * Note: you'd expect some comparison/clamp against fetch_elt_max
          * here.
          * There used to be one here but it was incorrect: overflow was
          * detected if index > fetch_elt_max - but the correct condition
          * would be index >= fetch_elt_max (since this is just size of elts
          * buffer / element size).
          * Using the correct condition however will cause failures - due to
          * vsplit/vcache code which rebases indices. So, as an example, if
          * fetch_elt_max is just 1 and fetch_count 2, vsplit cache will
          * replace all invalid indices with 0 - which in case of elt_bias
          * not being zero will get a different fetch index than the valid
          * index 0. So, just rely on vsplit code preventing out-of-bounds
          * fetches. This is also why it's safe to do elts fetch even if there
          * was no index buffer bound - the real buffer is never seen here, at
          * least not if there are index buffer overflows...
          */

         /*
          * XXX should not have to do this, as scale can be handled
          * natively by loads (hits asserts though).
          */
         tmp = lp_build_shl_imm(&blduivec, true_index_array, 2);
         fetch_elts = LLVMBuildBitCast(builder, fetch_elts,
                                       LLVMPointerType(LLVMInt8TypeInContext(context),
                                                       0), "");
         tmp = lp_build_gather(gallivm, vs_type.length,
                               32, bld.type, TRUE,
                               fetch_elts, tmp, FALSE);
         LLVMBuildStore(builder, tmp, index_store);
      }
      lp_build_else(&if_ctx);
      {
         tmp = LLVMBuildAdd(builder, true_index_array, start_vec, "");
         LLVMBuildStore(builder, tmp, index_store);
      }
      lp_build_endif(&if_ctx);

      true_index_array = LLVMBuildLoad(builder, index_store, "");

      for (j = 0; j < key->nr_vertex_elements; ++j) {
         struct pipe_vertex_element *velem = &key->vertex_element[j];
         const struct util_format_description *format_desc =
            util_format_description(velem->src_format);

         if (format_desc->format == PIPE_FORMAT_NONE) {
            for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
               inputs[j][i] = lp_build_zero(gallivm, vs_type);
            }
         }
         else if (velem->instance_divisor) {
            fetch_instanced(gallivm, format_desc, vs_type,
                            vb_stride[j], map_ptr[j],
                            buffer_size_adj[j],
                            inputs[j], instance_index[j]);
         }
         else {
            fetch_vector(gallivm, format_desc, vs_type,
                         vb_stride[j], map_ptr[j],
                         buffer_size_adj[j],
                         inputs[j], true_index_array);
         }
      }

      /* In the paths with elts vertex id has to be unaffected by the
       * index bias and because indices inside our elements array have
       * already had index bias applied we need to subtract it here to
       * get back to the original index.
       * in the linear paths vertex id has to be unaffected by the
       * original start index and because we abuse the 'start' variable
       * to either represent the actual start index or the index at which
       * the primitive was split (we split rendering into chunks of at
       * most 4095-vertices) we need to back out the original start
       * index out of our vertex id here.
       */
      system_values.basevertex = lp_build_broadcast_scalar(&blduivec,
                                                           vertex_id_offset);
      system_values.vertex_id = true_index_array;
      system_values.vertex_id_nobase = LLVMBuildSub(builder, true_index_array,
                                                      system_values.basevertex, "");

      ptr_aos = (const LLVMValueRef (*)[TGSI_NUM_CHANNELS]) inputs;
      generate_vs(variant,
                  builder,
                  vs_type,
                  outputs,
                  ptr_aos,
                  &system_values,
                  context_ptr,
                  sampler,
                  key->clamp_vertex_color);

      if (pos != -1 && cv != -1) {
         /* store original positions in clip before further manipulation */
         store_clip(gallivm, vs_type, io, outputs, pos);

         /* do cliptest */
         if (enable_cliptest) {
            LLVMValueRef temp = LLVMBuildLoad(builder, clipmask_bool_ptr, "");
            /* allocate clipmask, assign it integer type */
            clipmask = generate_clipmask(llvm,
                                         gallivm,
                                         vs_type,
                                         outputs,
                                         key,
                                         context_ptr, &have_clipdist);
            temp = LLVMBuildOr(builder, clipmask, temp, "");
            /* store temporary clipping boolean value */
            LLVMBuildStore(builder, temp, clipmask_bool_ptr);
         }
         else {
            clipmask = blduivec.zero;
         }

         /* do viewport mapping */
         if (!bypass_viewport) {
            generate_viewport(variant, builder, vs_type, outputs, context_ptr);
         }
      }
      else {
         clipmask = blduivec.zero;
      }

      /* store clipmask in vertex header,
       * original positions in clip
       * and transformed positions in data
       */
      convert_to_aos(gallivm, io, NULL, outputs, clipmask,
                     vs_info->num_outputs, vs_type,
                     enable_cliptest && key->need_edgeflags);
   }
   lp_build_loop_end_cond(&lp_loop, count, step, LLVMIntUGE);

   sampler->destroy(sampler);

   /* return clipping boolean value for function */
   ret = clipmask_booli8(gallivm, vs_type, clipmask_bool_ptr,
                         enable_cliptest && key->need_edgeflags);

   LLVMBuildRet(builder, ret);

   gallivm_verify_function(gallivm, variant_func);
}


struct draw_llvm_variant_key *
draw_llvm_make_variant_key(struct draw_llvm *llvm, char *store)
{
   unsigned i;
   struct draw_llvm_variant_key *key;
   struct draw_sampler_static_state *draw_sampler;

   key = (struct draw_llvm_variant_key *)store;

   memset(key, 0, offsetof(struct draw_llvm_variant_key, vertex_element[0]));

   key->clamp_vertex_color = llvm->draw->rasterizer->clamp_vertex_color; /**/

   /* will have to rig this up properly later */
   key->clip_xy = llvm->draw->clip_xy;
   key->clip_z = llvm->draw->clip_z;
   key->clip_user = llvm->draw->clip_user;
   key->bypass_viewport = llvm->draw->bypass_viewport;
   key->clip_halfz = llvm->draw->rasterizer->clip_halfz;
   /* XXX assumes edgeflag output not at 0 */
   key->need_edgeflags = (llvm->draw->vs.edgeflag_output ? TRUE : FALSE);
   key->ucp_enable = llvm->draw->rasterizer->clip_plane_enable;
   key->has_gs = llvm->draw->gs.geometry_shader != NULL;
   key->num_outputs = draw_total_vs_outputs(llvm->draw);

   /* All variants of this shader will have the same value for
    * nr_samplers.  Not yet trying to compact away holes in the
    * sampler array.
    */
   key->nr_samplers = llvm->draw->vs.vertex_shader->info.file_max[TGSI_FILE_SAMPLER] + 1;
   if (llvm->draw->vs.vertex_shader->info.file_max[TGSI_FILE_SAMPLER_VIEW] != -1) {
      key->nr_sampler_views =
         llvm->draw->vs.vertex_shader->info.file_max[TGSI_FILE_SAMPLER_VIEW] + 1;
   }
   else {
      key->nr_sampler_views = key->nr_samplers;
   }

   /* Presumably all variants of the shader should have the same
    * number of vertex elements - ie the number of shader inputs.
    * NOTE: we NEED to store the needed number of needed inputs
    * here, not the number of provided elements to match keysize
    * (and the offset of sampler state in the key).
    * If we have excess number of vertex elements, this is valid,
    * but the excess ones don't matter.
    * If we don't have enough vertex elements (which looks not really
    * valid but we'll handle it gracefully) fill out missing ones with
    * zero (we'll recognize these later by PIPE_FORMAT_NONE).
    */
   key->nr_vertex_elements =
      llvm->draw->vs.vertex_shader->info.file_max[TGSI_FILE_INPUT] + 1;

   if (llvm->draw->pt.nr_vertex_elements < key->nr_vertex_elements) {
      debug_printf("draw: vs with %d inputs but only have %d vertex elements\n",
                   key->nr_vertex_elements, llvm->draw->pt.nr_vertex_elements);
      memset(key->vertex_element, 0,
             sizeof(struct pipe_vertex_element) * key->nr_vertex_elements);
   }
   memcpy(key->vertex_element,
          llvm->draw->pt.vertex_element,
          sizeof(struct pipe_vertex_element) *
             MIN2(key->nr_vertex_elements, llvm->draw->pt.nr_vertex_elements));

   draw_sampler = draw_llvm_variant_key_samplers(key);
   memset(draw_sampler, 0,
          MAX2(key->nr_samplers, key->nr_sampler_views) * sizeof *draw_sampler);

   for (i = 0 ; i < key->nr_samplers; i++) {
      lp_sampler_static_sampler_state(&draw_sampler[i].sampler_state,
                                      llvm->draw->samplers[PIPE_SHADER_VERTEX][i]);
   }
   for (i = 0 ; i < key->nr_sampler_views; i++) {
      lp_sampler_static_texture_state(&draw_sampler[i].texture_state,
                                      llvm->draw->sampler_views[PIPE_SHADER_VERTEX][i]);
   }

   return key;
}


void
draw_llvm_dump_variant_key(struct draw_llvm_variant_key *key)
{
   unsigned i;
   struct draw_sampler_static_state *sampler = draw_llvm_variant_key_samplers(key);

   debug_printf("clamp_vertex_color = %u\n", key->clamp_vertex_color);
   debug_printf("clip_xy = %u\n", key->clip_xy);
   debug_printf("clip_z = %u\n", key->clip_z);
   debug_printf("clip_user = %u\n", key->clip_user);
   debug_printf("bypass_viewport = %u\n", key->bypass_viewport);
   debug_printf("clip_halfz = %u\n", key->clip_halfz);
   debug_printf("need_edgeflags = %u\n", key->need_edgeflags);
   debug_printf("has_gs = %u\n", key->has_gs);
   debug_printf("ucp_enable = %u\n", key->ucp_enable);

   for (i = 0 ; i < key->nr_vertex_elements; i++) {
      debug_printf("vertex_element[%i].src_offset = %u\n", i, key->vertex_element[i].src_offset);
      debug_printf("vertex_element[%i].instance_divisor = %u\n", i, key->vertex_element[i].instance_divisor);
      debug_printf("vertex_element[%i].vertex_buffer_index = %u\n", i, key->vertex_element[i].vertex_buffer_index);
      debug_printf("vertex_element[%i].src_format = %s\n", i, util_format_name(key->vertex_element[i].src_format));
   }

   for (i = 0 ; i < key->nr_sampler_views; i++) {
      debug_printf("sampler[%i].src_format = %s\n", i, util_format_name(sampler[i].texture_state.format));
   }
}


void
draw_llvm_set_mapped_texture(struct draw_context *draw,
                             enum pipe_shader_type shader_stage,
                             unsigned sview_idx,
                             uint32_t width, uint32_t height, uint32_t depth,
                             uint32_t first_level, uint32_t last_level,
                             const void *base_ptr,
                             uint32_t row_stride[PIPE_MAX_TEXTURE_LEVELS],
                             uint32_t img_stride[PIPE_MAX_TEXTURE_LEVELS],
                             uint32_t mip_offsets[PIPE_MAX_TEXTURE_LEVELS])
{
   unsigned j;
   struct draw_jit_texture *jit_tex;

   assert(shader_stage == PIPE_SHADER_VERTEX ||
          shader_stage == PIPE_SHADER_GEOMETRY);

   if (shader_stage == PIPE_SHADER_VERTEX) {
      assert(sview_idx < ARRAY_SIZE(draw->llvm->jit_context.textures));

      jit_tex = &draw->llvm->jit_context.textures[sview_idx];
   } else if (shader_stage == PIPE_SHADER_GEOMETRY) {
      assert(sview_idx < ARRAY_SIZE(draw->llvm->gs_jit_context.textures));

      jit_tex = &draw->llvm->gs_jit_context.textures[sview_idx];
   } else {
      assert(0);
      return;
   }

   jit_tex->width = width;
   jit_tex->height = height;
   jit_tex->depth = depth;
   jit_tex->first_level = first_level;
   jit_tex->last_level = last_level;
   jit_tex->base = base_ptr;

   for (j = first_level; j <= last_level; j++) {
      jit_tex->mip_offsets[j] = mip_offsets[j];
      jit_tex->row_stride[j] = row_stride[j];
      jit_tex->img_stride[j] = img_stride[j];
   }
}


void
draw_llvm_set_sampler_state(struct draw_context *draw, 
                            enum pipe_shader_type shader_type)
{
   unsigned i;

   if (shader_type == PIPE_SHADER_VERTEX) {
      for (i = 0; i < draw->num_samplers[PIPE_SHADER_VERTEX]; i++) {
         struct draw_jit_sampler *jit_sam = &draw->llvm->jit_context.samplers[i];

         if (draw->samplers[PIPE_SHADER_VERTEX][i]) {
            const struct pipe_sampler_state *s
               = draw->samplers[PIPE_SHADER_VERTEX][i];
            jit_sam->min_lod = s->min_lod;
            jit_sam->max_lod = s->max_lod;
            jit_sam->lod_bias = s->lod_bias;
            COPY_4V(jit_sam->border_color, s->border_color.f);
         }
      }
   } else if (shader_type == PIPE_SHADER_GEOMETRY) {
      for (i = 0; i < draw->num_samplers[PIPE_SHADER_GEOMETRY]; i++) {
         struct draw_jit_sampler *jit_sam = &draw->llvm->gs_jit_context.samplers[i];

         if (draw->samplers[PIPE_SHADER_GEOMETRY][i]) {
            const struct pipe_sampler_state *s
               = draw->samplers[PIPE_SHADER_GEOMETRY][i];
            jit_sam->min_lod = s->min_lod;
            jit_sam->max_lod = s->max_lod;
            jit_sam->lod_bias = s->lod_bias;
            COPY_4V(jit_sam->border_color, s->border_color.f);
         }
      }
   }
}


void
draw_llvm_destroy_variant(struct draw_llvm_variant *variant)
{
   struct draw_llvm *llvm = variant->llvm;

   if (gallivm_debug & (GALLIVM_DEBUG_TGSI | GALLIVM_DEBUG_IR)) {
      debug_printf("Deleting VS variant: %u vs variants,\t%u total variants\n",
                    variant->shader->variants_cached, llvm->nr_variants);
   }

   gallivm_destroy(variant->gallivm);

   remove_from_list(&variant->list_item_local);
   variant->shader->variants_cached--;
   remove_from_list(&variant->list_item_global);
   llvm->nr_variants--;
   FREE(variant);
}


/**
 * Create LLVM types for various structures.
 */
static void
create_gs_jit_types(struct draw_gs_llvm_variant *var)
{
   struct gallivm_state *gallivm = var->gallivm;
   LLVMTypeRef texture_type, sampler_type, context_type;

   texture_type = create_jit_texture_type(gallivm, "texture");
   sampler_type = create_jit_sampler_type(gallivm, "sampler");

   context_type = create_gs_jit_context_type(gallivm,
                                             var->shader->base.vector_length,
                                             texture_type, sampler_type,
                                             "draw_gs_jit_context");
   var->context_ptr_type = LLVMPointerType(context_type, 0);

   var->input_array_type = create_gs_jit_input_type(gallivm);
}

static LLVMTypeRef
get_gs_context_ptr_type(struct draw_gs_llvm_variant *variant)
{
   if (!variant->context_ptr_type)
      create_gs_jit_types(variant);
   return variant->context_ptr_type;
}

static LLVMValueRef
generate_mask_value(struct draw_gs_llvm_variant *variant,
                    struct lp_type gs_type)
{
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMBuilderRef builder = gallivm->builder;
   struct lp_type mask_type = lp_int_type(gs_type);
   LLVMValueRef num_prims;
   LLVMValueRef mask_val = lp_build_const_vec(gallivm, mask_type, 0);
   unsigned i;

   num_prims = lp_build_broadcast(gallivm, lp_build_vec_type(gallivm, mask_type),
                                  variant->num_prims);
   for (i = 0; i < gs_type.length; i++) {
      LLVMValueRef idx = lp_build_const_int32(gallivm, i);
      mask_val = LLVMBuildInsertElement(builder, mask_val, idx, idx, "");
   }
   mask_val = lp_build_compare(gallivm, mask_type,
                               PIPE_FUNC_GREATER, num_prims, mask_val);

   return mask_val;
}

static void
draw_gs_llvm_generate(struct draw_llvm *llvm,
                      struct draw_gs_llvm_variant *variant)
{
   struct gallivm_state *gallivm = variant->gallivm;
   LLVMContextRef context = gallivm->context;
   LLVMTypeRef int32_type = LLVMInt32TypeInContext(context);
   LLVMTypeRef arg_types[7];
   LLVMTypeRef func_type;
   LLVMValueRef variant_func;
   LLVMValueRef context_ptr;
   LLVMValueRef prim_id_ptr;
   LLVMBasicBlockRef block;
   LLVMBuilderRef builder;
   LLVMValueRef io_ptr, input_array, num_prims, mask_val;
   struct lp_build_sampler_soa *sampler = 0;
   struct lp_build_context bld;
   struct lp_bld_tgsi_system_values system_values;
   char func_name[64];
   struct lp_type gs_type;
   unsigned i;
   struct draw_gs_llvm_iface gs_iface;
   const struct tgsi_token *tokens = variant->shader->base.state.tokens;
   LLVMValueRef consts_ptr, num_consts_ptr;
   LLVMValueRef outputs[PIPE_MAX_SHADER_OUTPUTS][TGSI_NUM_CHANNELS];
   struct lp_build_mask_context mask;
   const struct tgsi_shader_info *gs_info = &variant->shader->base.info;
   unsigned vector_length = variant->shader->base.vector_length;

   memset(&system_values, 0, sizeof(system_values));

   util_snprintf(func_name, sizeof(func_name), "draw_llvm_gs_variant%u",
                 variant->shader->variants_cached);

   assert(variant->vertex_header_ptr_type);

   arg_types[0] = get_gs_context_ptr_type(variant);    /* context */
   arg_types[1] = variant->input_array_type;           /* input */
   arg_types[2] = variant->vertex_header_ptr_type;     /* vertex_header */
   arg_types[3] = int32_type;                          /* num_prims */
   arg_types[4] = int32_type;                          /* instance_id */
   arg_types[5] = LLVMPointerType(
      LLVMVectorType(int32_type, vector_length), 0);   /* prim_id_ptr */
   arg_types[6] = int32_type;

   func_type = LLVMFunctionType(int32_type, arg_types, ARRAY_SIZE(arg_types), 0);

   variant_func = LLVMAddFunction(gallivm->module, func_name, func_type);

   variant->function = variant_func;

   LLVMSetFunctionCallConv(variant_func, LLVMCCallConv);

   for (i = 0; i < ARRAY_SIZE(arg_types); ++i)
      if (LLVMGetTypeKind(arg_types[i]) == LLVMPointerTypeKind)
         lp_add_function_attr(variant_func, i + 1, LP_FUNC_ATTR_NOALIAS);

   context_ptr               = LLVMGetParam(variant_func, 0);
   input_array               = LLVMGetParam(variant_func, 1);
   io_ptr                    = LLVMGetParam(variant_func, 2);
   num_prims                 = LLVMGetParam(variant_func, 3);
   system_values.instance_id = LLVMGetParam(variant_func, 4);
   prim_id_ptr               = LLVMGetParam(variant_func, 5);
   system_values.invocation_id = LLVMGetParam(variant_func, 6);

   lp_build_name(context_ptr, "context");
   lp_build_name(input_array, "input");
   lp_build_name(io_ptr, "io");
   lp_build_name(num_prims, "num_prims");
   lp_build_name(system_values.instance_id, "instance_id");
   lp_build_name(prim_id_ptr, "prim_id_ptr");
   lp_build_name(system_values.invocation_id, "invocation_id");

   variant->context_ptr = context_ptr;
   variant->io_ptr = io_ptr;
   variant->num_prims = num_prims;

   gs_iface.base.fetch_input = draw_gs_llvm_fetch_input;
   gs_iface.base.emit_vertex = draw_gs_llvm_emit_vertex;
   gs_iface.base.end_primitive = draw_gs_llvm_end_primitive;
   gs_iface.base.gs_epilogue = draw_gs_llvm_epilogue;
   gs_iface.input = input_array;
   gs_iface.variant = variant;

   /*
    * Function body
    */

   block = LLVMAppendBasicBlockInContext(gallivm->context, variant_func, "entry");
   builder = gallivm->builder;
   LLVMPositionBuilderAtEnd(builder, block);

   lp_build_context_init(&bld, gallivm, lp_type_int(32));

   memset(&gs_type, 0, sizeof gs_type);
   gs_type.floating = TRUE; /* floating point values */
   gs_type.sign = TRUE;     /* values are signed */
   gs_type.norm = FALSE;    /* values are not limited to [0,1] or [-1,1] */
   gs_type.width = 32;      /* 32-bit float */
   gs_type.length = vector_length;

   consts_ptr = draw_gs_jit_context_constants(variant->gallivm, context_ptr);
   num_consts_ptr =
      draw_gs_jit_context_num_constants(variant->gallivm, context_ptr);

   /* code generated texture sampling */
   sampler = draw_llvm_sampler_soa_create(variant->key.samplers);

   mask_val = generate_mask_value(variant, gs_type);
   lp_build_mask_begin(&mask, gallivm, gs_type, mask_val);

   if (gs_info->uses_primid) {
      system_values.prim_id = LLVMBuildLoad(builder, prim_id_ptr, "prim_id");
   }

   if (gallivm_debug & (GALLIVM_DEBUG_TGSI | GALLIVM_DEBUG_IR)) {
      tgsi_dump(tokens, 0);
      draw_gs_llvm_dump_variant_key(&variant->key);
   }

   lp_build_tgsi_soa(variant->gallivm,
                     tokens,
                     gs_type,
                     &mask,
                     consts_ptr,
                     num_consts_ptr,
                     &system_values,
                     NULL,
                     outputs,
                     context_ptr,
                     NULL,
                     sampler,
                     &llvm->draw->gs.geometry_shader->info,
                     (const struct lp_build_tgsi_gs_iface *)&gs_iface);

   sampler->destroy(sampler);

   lp_build_mask_end(&mask);

   LLVMBuildRet(builder, lp_build_zero(gallivm, lp_type_uint(32)));

   gallivm_verify_function(gallivm, variant_func);
}


struct draw_gs_llvm_variant *
draw_gs_llvm_create_variant(struct draw_llvm *llvm,
                            unsigned num_outputs,
                            const struct draw_gs_llvm_variant_key *key)
{
   struct draw_gs_llvm_variant *variant;
   struct llvm_geometry_shader *shader =
      llvm_geometry_shader(llvm->draw->gs.geometry_shader);
   LLVMTypeRef vertex_header;
   char module_name[64];

   variant = MALLOC(sizeof *variant +
                    shader->variant_key_size -
                    sizeof variant->key);
   if (!variant)
      return NULL;

   variant->llvm = llvm;
   variant->shader = shader;

   util_snprintf(module_name, sizeof(module_name), "draw_llvm_gs_variant%u",
                 variant->shader->variants_cached);

   variant->gallivm = gallivm_create(module_name, llvm->context);

   create_gs_jit_types(variant);

   memcpy(&variant->key, key, shader->variant_key_size);

   vertex_header = create_jit_vertex_header(variant->gallivm, num_outputs);

   variant->vertex_header_ptr_type = LLVMPointerType(vertex_header, 0);

   draw_gs_llvm_generate(llvm, variant);

   gallivm_compile_module(variant->gallivm);

   variant->jit_func = (draw_gs_jit_func)
         gallivm_jit_function(variant->gallivm, variant->function);

   gallivm_free_ir(variant->gallivm);

   variant->list_item_global.base = variant;
   variant->list_item_local.base = variant;
   /*variant->no = */shader->variants_created++;
   variant->list_item_global.base = variant;

   return variant;
}

void
draw_gs_llvm_destroy_variant(struct draw_gs_llvm_variant *variant)
{
   struct draw_llvm *llvm = variant->llvm;

   if (gallivm_debug & (GALLIVM_DEBUG_TGSI | GALLIVM_DEBUG_IR)) {
      debug_printf("Deleting GS variant: %u gs variants,\t%u total variants\n",
                    variant->shader->variants_cached, llvm->nr_gs_variants);
   }

   gallivm_destroy(variant->gallivm);

   remove_from_list(&variant->list_item_local);
   variant->shader->variants_cached--;
   remove_from_list(&variant->list_item_global);
   llvm->nr_gs_variants--;
   FREE(variant);
}

struct draw_gs_llvm_variant_key *
draw_gs_llvm_make_variant_key(struct draw_llvm *llvm, char *store)
{
   unsigned i;
   struct draw_gs_llvm_variant_key *key;
   struct draw_sampler_static_state *draw_sampler;

   key = (struct draw_gs_llvm_variant_key *)store;

   memset(key, 0, offsetof(struct draw_gs_llvm_variant_key, samplers[0]));

   key->num_outputs = draw_total_gs_outputs(llvm->draw);

   /* All variants of this shader will have the same value for
    * nr_samplers.  Not yet trying to compact away holes in the
    * sampler array.
    */
   key->nr_samplers = llvm->draw->gs.geometry_shader->info.file_max[TGSI_FILE_SAMPLER] + 1;
   if (llvm->draw->gs.geometry_shader->info.file_max[TGSI_FILE_SAMPLER_VIEW] != -1) {
      key->nr_sampler_views =
         llvm->draw->gs.geometry_shader->info.file_max[TGSI_FILE_SAMPLER_VIEW] + 1;
   }
   else {
      key->nr_sampler_views = key->nr_samplers;
   }

   draw_sampler = key->samplers;

   memset(draw_sampler, 0, MAX2(key->nr_samplers, key->nr_sampler_views) * sizeof *draw_sampler);

   for (i = 0 ; i < key->nr_samplers; i++) {
      lp_sampler_static_sampler_state(&draw_sampler[i].sampler_state,
                                      llvm->draw->samplers[PIPE_SHADER_GEOMETRY][i]);
   }
   for (i = 0 ; i < key->nr_sampler_views; i++) {
      lp_sampler_static_texture_state(&draw_sampler[i].texture_state,
                                      llvm->draw->sampler_views[PIPE_SHADER_GEOMETRY][i]);
   }

   return key;
}

void
draw_gs_llvm_dump_variant_key(struct draw_gs_llvm_variant_key *key)
{
   unsigned i;
   struct draw_sampler_static_state *sampler = key->samplers;

   for (i = 0 ; i < key->nr_sampler_views; i++) {
      debug_printf("sampler[%i].src_format = %s\n", i,
                   util_format_name(sampler[i].texture_state.format));
   }
}
