/**************************************************************************
 *
 * Copyright 2011-2012 Advanced Micro Devices, Inc.
 * Copyright 2009 VMware, Inc.
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
 * TGSI to LLVM IR translation.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 * @author Tom Stellard <thomas.stellard@amd.com>
 */

#ifndef LP_BLD_TGSI_H
#define LP_BLD_TGSI_H

#include "gallivm/lp_bld.h"
#include "gallivm/lp_bld_tgsi_action.h"
#include "gallivm/lp_bld_limits.h"
#include "gallivm/lp_bld_sample.h"
#include "lp_bld_type.h"
#include "pipe/p_compiler.h"
#include "pipe/p_state.h"
#include "tgsi/tgsi_exec.h"
#include "tgsi/tgsi_scan.h"
#include "tgsi/tgsi_info.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LP_CHAN_ALL ~0u

#define LP_MAX_INSTRUCTIONS 256

struct tgsi_full_declaration;
struct tgsi_full_immediate;
struct tgsi_full_instruction;
struct tgsi_full_src_register;
struct tgsi_opcode_info;
struct tgsi_token;
struct tgsi_shader_info;
struct lp_build_mask_context;
struct gallivm_state;
struct lp_derivatives;
struct lp_build_tgsi_gs_iface;


enum lp_build_tex_modifier {
   LP_BLD_TEX_MODIFIER_NONE = 0,
   LP_BLD_TEX_MODIFIER_PROJECTED,
   LP_BLD_TEX_MODIFIER_LOD_BIAS,
   LP_BLD_TEX_MODIFIER_EXPLICIT_LOD,
   LP_BLD_TEX_MODIFIER_EXPLICIT_DERIV,
   LP_BLD_TEX_MODIFIER_LOD_ZERO
};


/**
 * Describe a channel of a register.
 *
 * The value can be a:
 * - immediate value (i.e. derived from a IMM register)
 * - CONST[n].x/y/z/w
 * - IN[n].x/y/z/w
 * - undetermined (when .file == TGSI_FILE_NULL)
 *
 * This is one of the analysis results, and is used to described
 * the output color in terms of inputs.
 */
struct lp_tgsi_channel_info
{
   unsigned file:4; /* TGSI_FILE_* */
   unsigned swizzle:3; /* PIPE_SWIZZLE_x */
   union {
      uint32_t index;
      float value; /* for TGSI_FILE_IMMEDIATE */
   } u;
};


/**
 * Describe a texture sampler interpolator.
 *
 * The interpolation is described in terms of regular inputs.
 */
struct lp_tgsi_texture_info
{
   struct lp_tgsi_channel_info coord[4];
   unsigned target:8; /* TGSI_TEXTURE_* */
   unsigned sampler_unit:8;  /* Sampler unit */
   unsigned texture_unit:8;  /* Texture unit */
   unsigned modifier:8; /* LP_BLD_TEX_MODIFIER_* */
};


struct lp_tgsi_info
{
   struct tgsi_shader_info base;

   /*
    * Whether any of the texture opcodes access a register file other than
    * TGSI_FILE_INPUT.
    *
    * We could also handle TGSI_FILE_CONST/IMMEDIATE here, but there is little
    * benefit.
    */
   unsigned indirect_textures:1;

   /*
    * Whether any of the texture (sample) ocpodes use different sampler
    * and sampler view unit.
    */
   unsigned sampler_texture_units_different:1;

   /*
    * Whether any immediate values are outside the range of 0 and 1
    */
   unsigned unclamped_immediates:1;

   /*
    * Texture opcode description. Aimed at detecting and described direct
    * texture opcodes.
    */
   unsigned num_texs;
   struct lp_tgsi_texture_info tex[PIPE_MAX_SAMPLERS];

   /*
    * Output description. Aimed at detecting and describing simple blit
    * shaders.
    */
   struct lp_tgsi_channel_info output[PIPE_MAX_SHADER_OUTPUTS][4];

   /*
    * Shortcut pointers into the above (for fragment shaders).
    */
   const struct lp_tgsi_channel_info *cbuf[PIPE_MAX_COLOR_BUFS];
};

/**
 * Reference to system values.
 */
struct lp_bld_tgsi_system_values {
   LLVMValueRef instance_id;
   LLVMValueRef vertex_id;
   LLVMValueRef vertex_id_nobase;
   LLVMValueRef prim_id;
   LLVMValueRef basevertex;
   LLVMValueRef invocation_id;
};


/**
 * Sampler code generation interface.
 *
 * Although texture sampling is a requirement for TGSI translation, it is
 * a very different problem with several different approaches to it. This
 * structure establishes an interface for texture sampling code generation, so
 * that we can easily use different texture sampling strategies.
 */
struct lp_build_sampler_soa
{
   void
   (*destroy)( struct lp_build_sampler_soa *sampler );

   void
   (*emit_tex_sample)(const struct lp_build_sampler_soa *sampler,
                      struct gallivm_state *gallivm,
                      const struct lp_sampler_params *params);

   void
   (*emit_size_query)( const struct lp_build_sampler_soa *sampler,
                       struct gallivm_state *gallivm,
                       const struct lp_sampler_size_query_params *params);
};


struct lp_build_sampler_aos
{
   LLVMValueRef
   (*emit_fetch_texel)( struct lp_build_sampler_aos *sampler,
                        struct lp_build_context *bld,
                        unsigned target, /* TGSI_TEXTURE_* */
                        unsigned unit,
                        LLVMValueRef coords,
                        const struct lp_derivatives derivs,
                        enum lp_build_tex_modifier modifier);
};


void
lp_build_tgsi_info(const struct tgsi_token *tokens,
                   struct lp_tgsi_info *info);


void
lp_build_tgsi_soa(struct gallivm_state *gallivm,
                  const struct tgsi_token *tokens,
                  struct lp_type type,
                  struct lp_build_mask_context *mask,
                  LLVMValueRef consts_ptr,
                  LLVMValueRef const_sizes_ptr,
                  const struct lp_bld_tgsi_system_values *system_values,
                  const LLVMValueRef (*inputs)[4],
                  LLVMValueRef (*outputs)[4],
                  LLVMValueRef context_ptr,
                  LLVMValueRef thread_data_ptr,
                  struct lp_build_sampler_soa *sampler,
                  const struct tgsi_shader_info *info,
                  const struct lp_build_tgsi_gs_iface *gs_iface);


void
lp_build_tgsi_aos(struct gallivm_state *gallivm,
                  const struct tgsi_token *tokens,
                  struct lp_type type,
                  const unsigned char swizzles[4],
                  LLVMValueRef consts_ptr,
                  const LLVMValueRef *inputs,
                  LLVMValueRef *outputs,
                  struct lp_build_sampler_aos *sampler,
                  const struct tgsi_shader_info *info);


enum lp_exec_mask_break_type {
   LP_EXEC_MASK_BREAK_TYPE_LOOP,
   LP_EXEC_MASK_BREAK_TYPE_SWITCH
};


struct lp_exec_mask {
   struct lp_build_context *bld;

   boolean has_mask;
   boolean ret_in_main;

   LLVMTypeRef int_vec_type;

   LLVMValueRef exec_mask;

   LLVMValueRef ret_mask;
   LLVMValueRef cond_mask;
   LLVMValueRef switch_mask;         /* current switch exec mask */
   LLVMValueRef cont_mask;
   LLVMValueRef break_mask;

   struct function_ctx {
      int pc;
      LLVMValueRef ret_mask;

      LLVMValueRef cond_stack[LP_MAX_TGSI_NESTING];
      int cond_stack_size;

      /* keep track if break belongs to switch or loop */
      enum lp_exec_mask_break_type break_type_stack[LP_MAX_TGSI_NESTING];
      enum lp_exec_mask_break_type break_type;

      struct {
         LLVMValueRef switch_val;
         LLVMValueRef switch_mask;
         LLVMValueRef switch_mask_default;
         boolean switch_in_default;
         unsigned switch_pc;
      } switch_stack[LP_MAX_TGSI_NESTING];
      int switch_stack_size;
      LLVMValueRef switch_val;
      LLVMValueRef switch_mask_default; /* reverse of switch mask used for default */
      boolean switch_in_default;        /* if switch exec is currently in default */
      unsigned switch_pc;               /* when used points to default or endswitch-1 */

      LLVMValueRef loop_limiter;
      LLVMBasicBlockRef loop_block;
      LLVMValueRef break_var;
      struct {
         LLVMBasicBlockRef loop_block;
         LLVMValueRef cont_mask;
         LLVMValueRef break_mask;
         LLVMValueRef break_var;
      } loop_stack[LP_MAX_TGSI_NESTING];
      int loop_stack_size;

   } *function_stack;
   int function_stack_size;
};

struct lp_build_tgsi_inst_list
{
   struct tgsi_full_instruction *instructions;
   uint max_instructions;
   uint num_instructions;
};

unsigned lp_bld_tgsi_list_init(struct lp_build_tgsi_context * bld_base);


unsigned lp_bld_tgsi_add_instruction(
   struct lp_build_tgsi_context * bld_base,
   const struct tgsi_full_instruction *inst_to_add);


struct lp_build_tgsi_context;


typedef LLVMValueRef (*lp_build_emit_fetch_fn)(struct lp_build_tgsi_context *,
                                        const struct tgsi_full_src_register *,
                                        enum tgsi_opcode_type,
                                        unsigned);

struct lp_build_tgsi_context
{
   struct lp_build_context base;

   struct lp_build_context uint_bld;
   struct lp_build_context int_bld;

   struct lp_build_context dbl_bld;

   struct lp_build_context uint64_bld;
   struct lp_build_context int64_bld;

   /** This array stores functions that are used to transform TGSI opcodes to
     * LLVM instructions.
     */
   struct lp_build_tgsi_action op_actions[TGSI_OPCODE_LAST];

   /* TGSI_OPCODE_RSQ is defined as 1 / sqrt( abs(src0.x) ), rsq_action
    * should compute 1 / sqrt (src0.x) */
   struct lp_build_tgsi_action rsq_action;

   struct lp_build_tgsi_action sqrt_action;

   struct lp_build_tgsi_action drsq_action;

   struct lp_build_tgsi_action dsqrt_action;
   const struct tgsi_shader_info *info;

   lp_build_emit_fetch_fn emit_fetch_funcs[TGSI_FILE_COUNT];

   LLVMValueRef (*emit_swizzle)(struct lp_build_tgsi_context *,
                         LLVMValueRef, unsigned, unsigned, unsigned, unsigned);


   void (*emit_debug)(struct lp_build_tgsi_context *,
                      const struct tgsi_full_instruction *,
                      const struct tgsi_opcode_info *);

   void (*emit_store)(struct lp_build_tgsi_context *,
                      const struct tgsi_full_instruction *,
                      const struct tgsi_opcode_info *,
                      unsigned index,
                      LLVMValueRef dst[4]);

   void (*emit_declaration)(struct lp_build_tgsi_context *,
                             const struct tgsi_full_declaration *decl);

   void (*emit_immediate)(struct lp_build_tgsi_context *,
                          const struct tgsi_full_immediate *imm);


   /* Allow the user to store data in this structure rather than passing it
    * to every function. */
   void * userdata;

   boolean soa;

   int pc;

   struct tgsi_full_instruction *instructions;
   uint max_instructions;
   uint num_instructions;

   /** This function allows the user to insert some instructions at the
     * beginning of the program.  It is optional and does not need to be
     * implemented.
     */
   void (*emit_prologue)(struct lp_build_tgsi_context*);

   /** This function allows the user to insert some instructions at the end of
     * the program.  This callback is intended to be used for emitting
     * instructions to handle the export for the output registers, but it can
     * be used for any purpose.  Implementing this function is optiona, but
     * recommended.
     */
   void (*emit_epilogue)(struct lp_build_tgsi_context*);
};

struct lp_build_tgsi_gs_iface
{
   LLVMValueRef (*fetch_input)(const struct lp_build_tgsi_gs_iface *gs_iface,
                               struct lp_build_tgsi_context * bld_base,
                               boolean is_vindex_indirect,
                               LLVMValueRef vertex_index,
                               boolean is_aindex_indirect,
                               LLVMValueRef attrib_index,
                               LLVMValueRef swizzle_index);
   void (*emit_vertex)(const struct lp_build_tgsi_gs_iface *gs_iface,
                       struct lp_build_tgsi_context * bld_base,
                       LLVMValueRef (*outputs)[4],
                       LLVMValueRef emitted_vertices_vec);
   void (*end_primitive)(const struct lp_build_tgsi_gs_iface *gs_iface,
                         struct lp_build_tgsi_context * bld_base,
                         LLVMValueRef verts_per_prim_vec,
                         LLVMValueRef emitted_prims_vec);
   void (*gs_epilogue)(const struct lp_build_tgsi_gs_iface *gs_iface,
                       struct lp_build_tgsi_context * bld_base,
                       LLVMValueRef total_emitted_vertices_vec,
                       LLVMValueRef emitted_prims_vec);
};

struct lp_build_tgsi_soa_context
{
   struct lp_build_tgsi_context bld_base;

   /* Builder for scalar elements of shader's data type (float) */
   struct lp_build_context elem_bld;

   const struct lp_build_tgsi_gs_iface *gs_iface;
   LLVMValueRef emitted_prims_vec_ptr;
   LLVMValueRef total_emitted_vertices_vec_ptr;
   LLVMValueRef emitted_vertices_vec_ptr;
   LLVMValueRef max_output_vertices_vec;

   LLVMValueRef consts_ptr;
   LLVMValueRef const_sizes_ptr;
   LLVMValueRef consts[LP_MAX_TGSI_CONST_BUFFERS];
   LLVMValueRef consts_sizes[LP_MAX_TGSI_CONST_BUFFERS];
   const LLVMValueRef (*inputs)[TGSI_NUM_CHANNELS];
   LLVMValueRef (*outputs)[TGSI_NUM_CHANNELS];
   LLVMValueRef context_ptr;
   LLVMValueRef thread_data_ptr;

   const struct lp_build_sampler_soa *sampler;

   struct tgsi_declaration_sampler_view sv[PIPE_MAX_SHADER_SAMPLER_VIEWS];

   LLVMValueRef immediates[LP_MAX_INLINED_IMMEDIATES][TGSI_NUM_CHANNELS];
   LLVMValueRef temps[LP_MAX_INLINED_TEMPS][TGSI_NUM_CHANNELS];
   LLVMValueRef addr[LP_MAX_TGSI_ADDRS][TGSI_NUM_CHANNELS];

   /* We allocate/use this array of temps if (1 << TGSI_FILE_TEMPORARY) is
    * set in the indirect_files field.
    * The temps[] array above is unused then.
    */
   LLVMValueRef temps_array;

   /* We allocate/use this array of output if (1 << TGSI_FILE_OUTPUT) is
    * set in the indirect_files field.
    * The outputs[] array above is unused then.
    */
   LLVMValueRef outputs_array;

   /* We allocate/use this array of inputs if (1 << TGSI_FILE_INPUT) is
    * set in the indirect_files field.
    * The inputs[] array above is unused then.
    */
   LLVMValueRef inputs_array;

   /* We allocate/use this array of temps if (1 << TGSI_FILE_IMMEDIATE) is
    * set in the indirect_files field.
    */
   LLVMValueRef imms_array;


   struct lp_bld_tgsi_system_values system_values;

   /** bitmask indicating which register files are accessed indirectly */
   unsigned indirect_files;

   struct lp_build_mask_context *mask;
   struct lp_exec_mask exec_mask;

   uint num_immediates;
   boolean use_immediates_array;
};

void
lp_emit_declaration_soa(
   struct lp_build_tgsi_context *bld,
   const struct tgsi_full_declaration *decl);

void lp_emit_immediate_soa(
   struct lp_build_tgsi_context *bld_base,
   const struct tgsi_full_immediate *imm);

boolean
lp_emit_instruction_soa(
   struct lp_build_tgsi_soa_context *bld,
   const struct tgsi_full_instruction *inst,
   const struct tgsi_opcode_info *info);


LLVMValueRef
lp_get_temp_ptr_soa(
   struct lp_build_tgsi_soa_context *bld,
   unsigned index,
   unsigned chan);

LLVMValueRef
lp_get_output_ptr(
   struct lp_build_tgsi_soa_context *bld,
   unsigned index,
   unsigned chan);

struct lp_build_tgsi_aos_context
{
   struct lp_build_tgsi_context bld_base;

   /* Builder for integer masks and indices */
   struct lp_build_context int_bld;

   /*
    * AoS swizzle used:
    * - swizzles[0] = red index
    * - swizzles[1] = green index
    * - swizzles[2] = blue index
    * - swizzles[3] = alpha index
    */
   unsigned char swizzles[4];
   unsigned char inv_swizzles[4];

   LLVMValueRef consts_ptr;
   const LLVMValueRef *inputs;
   LLVMValueRef *outputs;

   struct lp_build_sampler_aos *sampler;

   struct tgsi_declaration_sampler_view sv[PIPE_MAX_SHADER_SAMPLER_VIEWS];

   LLVMValueRef immediates[LP_MAX_INLINED_IMMEDIATES];
   LLVMValueRef temps[LP_MAX_INLINED_TEMPS];
   LLVMValueRef addr[LP_MAX_TGSI_ADDRS];

   /* We allocate/use this array of temps if (1 << TGSI_FILE_TEMPORARY) is
    * set in the indirect_files field.
    * The temps[] array above is unused then.
    */
   LLVMValueRef temps_array;

   /** bitmask indicating which register files are accessed indirectly */
   unsigned indirect_files;

};

static inline struct lp_build_tgsi_soa_context *
lp_soa_context(struct lp_build_tgsi_context *bld_base)
{
   return (struct lp_build_tgsi_soa_context *)bld_base;
}

static inline struct lp_build_tgsi_aos_context *
lp_aos_context(struct lp_build_tgsi_context *bld_base)
{
   return (struct lp_build_tgsi_aos_context *)bld_base;
}

void
lp_emit_declaration_aos(
   struct lp_build_tgsi_aos_context *bld,
   const struct tgsi_full_declaration *decl);


boolean
lp_emit_instruction_aos(
   struct lp_build_tgsi_aos_context *bld,
   const struct tgsi_full_instruction *inst,
   const struct tgsi_opcode_info *info,
   int *pc);

void
lp_emit_store_aos(
   struct lp_build_tgsi_aos_context *bld,
   const struct tgsi_full_instruction *inst,
   unsigned index,
   LLVMValueRef value);

void lp_build_fetch_args(
   struct lp_build_tgsi_context * bld_base,
   struct lp_build_emit_data * emit_data);

LLVMValueRef
lp_build_tgsi_inst_llvm_aos(
   struct lp_build_tgsi_context * bld_base,
   const struct tgsi_full_instruction *inst);

void
lp_build_tgsi_intrinsic(
 const struct lp_build_tgsi_action * action,
 struct lp_build_tgsi_context * bld_base,
 struct lp_build_emit_data * emit_data);

LLVMValueRef
lp_build_emit_llvm(
   struct lp_build_tgsi_context *bld_base,
   unsigned tgsi_opcode,
   struct lp_build_emit_data * emit_data);

LLVMValueRef
lp_build_emit_llvm_unary(
   struct lp_build_tgsi_context *bld_base,
   unsigned tgsi_opcode,
   LLVMValueRef arg0);

LLVMValueRef
lp_build_emit_llvm_binary(
   struct lp_build_tgsi_context *bld_base,
   unsigned tgsi_opcode,
   LLVMValueRef arg0,
   LLVMValueRef arg1);

LLVMValueRef
lp_build_emit_llvm_ternary(
   struct lp_build_tgsi_context *bld_base,
   unsigned tgsi_opcode,
   LLVMValueRef arg0,
   LLVMValueRef arg1,
   LLVMValueRef arg2);

boolean
lp_build_tgsi_inst_llvm(
   struct lp_build_tgsi_context * bld_base,
   const struct tgsi_full_instruction *inst);

LLVMValueRef
lp_build_emit_fetch_src(
   struct lp_build_tgsi_context *bld_base,
   const struct tgsi_full_src_register *reg,
   enum tgsi_opcode_type stype,
   const unsigned chan_index);

LLVMValueRef
lp_build_emit_fetch(
   struct lp_build_tgsi_context *bld_base,
   const struct tgsi_full_instruction *inst,
   unsigned src_op,
   const unsigned chan_index);


LLVMValueRef
lp_build_emit_fetch_texoffset(
   struct lp_build_tgsi_context *bld_base,
   const struct tgsi_full_instruction *inst,
   unsigned tex_off_op,
   const unsigned chan_index);

boolean
lp_build_tgsi_llvm(
   struct lp_build_tgsi_context * bld_base,
   const struct tgsi_token *tokens);

#ifdef __cplusplus
}
#endif

#endif /* LP_BLD_TGSI_H */
