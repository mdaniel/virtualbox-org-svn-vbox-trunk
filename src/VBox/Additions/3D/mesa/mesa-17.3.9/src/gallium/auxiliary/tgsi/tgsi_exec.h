/**************************************************************************
 * 
 * Copyright 2007-2008 VMware, Inc.
 * All Rights Reserved.
 * Copyright 2009-2010 VMware, Inc.  All rights Reserved.
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

#ifndef TGSI_EXEC_H
#define TGSI_EXEC_H

#include "pipe/p_compiler.h"
#include "pipe/p_state.h"
#include "pipe/p_shader_tokens.h"

#if defined __cplusplus
extern "C" {
#endif

#define TGSI_CHAN_X 0
#define TGSI_CHAN_Y 1
#define TGSI_CHAN_Z 2
#define TGSI_CHAN_W 3

#define TGSI_NUM_CHANNELS 4  /* R,G,B,A */
#define TGSI_QUAD_SIZE    4  /* 4 pixel/quad */

#define TGSI_FOR_EACH_CHANNEL( CHAN )\
   for (CHAN = 0; CHAN < TGSI_NUM_CHANNELS; CHAN++)

#define TGSI_IS_DST0_CHANNEL_ENABLED( INST, CHAN )\
   ((INST)->Dst[0].Register.WriteMask & (1 << (CHAN)))

#define TGSI_IF_IS_DST0_CHANNEL_ENABLED( INST, CHAN )\
   if (TGSI_IS_DST0_CHANNEL_ENABLED( INST, CHAN ))

#define TGSI_FOR_EACH_DST0_ENABLED_CHANNEL( INST, CHAN )\
   TGSI_FOR_EACH_CHANNEL( CHAN )\
      TGSI_IF_IS_DST0_CHANNEL_ENABLED( INST, CHAN )

#define TGSI_IS_DST1_CHANNEL_ENABLED( INST, CHAN )\
   ((INST)->Dst[1].Register.WriteMask & (1 << (CHAN)))

#define TGSI_IF_IS_DST1_CHANNEL_ENABLED( INST, CHAN )\
   if (TGSI_IS_DST1_CHANNEL_ENABLED( INST, CHAN ))

#define TGSI_FOR_EACH_DST1_ENABLED_CHANNEL( INST, CHAN )\
   TGSI_FOR_EACH_CHANNEL( CHAN )\
      TGSI_IF_IS_DST1_CHANNEL_ENABLED( INST, CHAN )

/**
  * Registers may be treated as float, signed int or unsigned int.
  */
union tgsi_exec_channel
{
   float    f[TGSI_QUAD_SIZE];
   int      i[TGSI_QUAD_SIZE];
   unsigned u[TGSI_QUAD_SIZE];
};

/**
  * A vector[RGBA] of channels[4 pixels]
  */
struct tgsi_exec_vector
{
   union tgsi_exec_channel xyzw[TGSI_NUM_CHANNELS];
};

/**
 * For fragment programs, information for computing fragment input
 * values from plane equation of the triangle/line.
 */
struct tgsi_interp_coef
{
   float a0[TGSI_NUM_CHANNELS];	/* in an xyzw layout */
   float dadx[TGSI_NUM_CHANNELS];
   float dady[TGSI_NUM_CHANNELS];
};

enum tgsi_sampler_control
{
   TGSI_SAMPLER_LOD_NONE,
   TGSI_SAMPLER_LOD_BIAS,
   TGSI_SAMPLER_LOD_EXPLICIT,
   TGSI_SAMPLER_LOD_ZERO,
   TGSI_SAMPLER_DERIVS_EXPLICIT,
   TGSI_SAMPLER_GATHER,
};

struct tgsi_image_params {
   unsigned unit;
   unsigned tgsi_tex_instr;
   enum pipe_format format;
   unsigned execmask;
};

struct tgsi_image {
   /* image interfaces */
   void (*load)(const struct tgsi_image *image,
                const struct tgsi_image_params *params,
                const int s[TGSI_QUAD_SIZE],
                const int t[TGSI_QUAD_SIZE],
                const int r[TGSI_QUAD_SIZE],
                const int sample[TGSI_QUAD_SIZE],
                float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*store)(const struct tgsi_image *image,
                 const struct tgsi_image_params *params,
                 const int s[TGSI_QUAD_SIZE],
                 const int t[TGSI_QUAD_SIZE],
                 const int r[TGSI_QUAD_SIZE],
                 const int sample[TGSI_QUAD_SIZE],
                 float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*op)(const struct tgsi_image *image,
              const struct tgsi_image_params *params,
              unsigned opcode,
              const int s[TGSI_QUAD_SIZE],
              const int t[TGSI_QUAD_SIZE],
              const int r[TGSI_QUAD_SIZE],
              const int sample[TGSI_QUAD_SIZE],
              float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE],
              float rgba2[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*get_dims)(const struct tgsi_image *image,
                    const struct tgsi_image_params *params,
                    int dims[4]);
};

struct tgsi_buffer_params {
   unsigned unit;
   unsigned execmask;
   unsigned writemask;
};

struct tgsi_buffer {
   /* buffer interfaces */
   void (*load)(const struct tgsi_buffer *buffer,
                const struct tgsi_buffer_params *params,
                const int s[TGSI_QUAD_SIZE],
                float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*store)(const struct tgsi_buffer *buffer,
                 const struct tgsi_buffer_params *params,
                 const int s[TGSI_QUAD_SIZE],
                 float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*op)(const struct tgsi_buffer *buffer,
              const struct tgsi_buffer_params *params,
              unsigned opcode,
              const int s[TGSI_QUAD_SIZE],
              float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE],
              float rgba2[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);

   void (*get_dims)(const struct tgsi_buffer *buffer,
                    const struct tgsi_buffer_params *params,
                    int *dim);
};

/**
 * Information for sampling textures, which must be implemented
 * by code outside the TGSI executor.
 */
struct tgsi_sampler
{
   /** Get samples for four fragments in a quad */
   /* this interface contains 5 sets of channels that vary
    * depending on the sampler.
    * s - the first texture coordinate for sampling.
    * t - the second texture coordinate for sampling - unused for 1D,
          layer for 1D arrays.
    * r - the third coordinate for sampling for 3D, cube, cube arrays,
    *     layer for 2D arrays. Compare value for 1D/2D shadows.
    * c0 - Compare value for shadow cube and shadow 2d arrays,
    *      layer for cube arrays.
    * derivs - explicit derivatives.
    * offset - texel offsets
    * lod - lod value, except for shadow cube arrays (compare value there).
    */
   void (*get_samples)(struct tgsi_sampler *sampler,
                       const unsigned sview_index,
                       const unsigned sampler_index,
                       const float s[TGSI_QUAD_SIZE],
                       const float t[TGSI_QUAD_SIZE],
                       const float r[TGSI_QUAD_SIZE],
                       const float c0[TGSI_QUAD_SIZE],
                       const float c1[TGSI_QUAD_SIZE],
                       float derivs[3][2][TGSI_QUAD_SIZE],
                       const int8_t offset[3],
                       enum tgsi_sampler_control control,
                       float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);
   void (*get_dims)(struct tgsi_sampler *sampler,
                    const unsigned sview_index,
                    int level, int dims[4]);
   void (*get_texel)(struct tgsi_sampler *sampler,
                     const unsigned sview_index,
                     const int i[TGSI_QUAD_SIZE],
                     const int j[TGSI_QUAD_SIZE], const int k[TGSI_QUAD_SIZE],
                     const int lod[TGSI_QUAD_SIZE], const int8_t offset[3],
                     float rgba[TGSI_NUM_CHANNELS][TGSI_QUAD_SIZE]);
   void (*query_lod)(const struct tgsi_sampler *tgsi_sampler,
                     const unsigned sview_index,
                     const unsigned sampler_index,
                     const float s[TGSI_QUAD_SIZE],
                     const float t[TGSI_QUAD_SIZE],
                     const float p[TGSI_QUAD_SIZE],
                     const float c0[TGSI_QUAD_SIZE],
                     const enum tgsi_sampler_control control,
                     float mipmap[TGSI_QUAD_SIZE],
                     float lod[TGSI_QUAD_SIZE]);
};

#define TGSI_EXEC_NUM_TEMPS       4096
#define TGSI_EXEC_NUM_IMMEDIATES  256

/*
 * Locations of various utility registers (_I = Index, _C = Channel)
 */
#define TGSI_EXEC_TEMP_00000000_I   (TGSI_EXEC_NUM_TEMPS + 0)
#define TGSI_EXEC_TEMP_00000000_C   0

#define TGSI_EXEC_TEMP_7FFFFFFF_I   (TGSI_EXEC_NUM_TEMPS + 0)
#define TGSI_EXEC_TEMP_7FFFFFFF_C   1

#define TGSI_EXEC_TEMP_80000000_I   (TGSI_EXEC_NUM_TEMPS + 0)
#define TGSI_EXEC_TEMP_80000000_C   2

#define TGSI_EXEC_TEMP_FFFFFFFF_I   (TGSI_EXEC_NUM_TEMPS + 0)
#define TGSI_EXEC_TEMP_FFFFFFFF_C   3

#define TGSI_EXEC_TEMP_ONE_I        (TGSI_EXEC_NUM_TEMPS + 1)
#define TGSI_EXEC_TEMP_ONE_C        0

#define TGSI_EXEC_TEMP_TWO_I        (TGSI_EXEC_NUM_TEMPS + 1)
#define TGSI_EXEC_TEMP_TWO_C        1

#define TGSI_EXEC_TEMP_128_I        (TGSI_EXEC_NUM_TEMPS + 1)
#define TGSI_EXEC_TEMP_128_C        2

#define TGSI_EXEC_TEMP_MINUS_128_I  (TGSI_EXEC_NUM_TEMPS + 1)
#define TGSI_EXEC_TEMP_MINUS_128_C  3

#define TGSI_EXEC_TEMP_KILMASK_I    (TGSI_EXEC_NUM_TEMPS + 2)
#define TGSI_EXEC_TEMP_KILMASK_C    0

#define TGSI_EXEC_TEMP_OUTPUT_I     (TGSI_EXEC_NUM_TEMPS + 2)
#define TGSI_EXEC_TEMP_OUTPUT_C     1

#define TGSI_EXEC_TEMP_PRIMITIVE_I  (TGSI_EXEC_NUM_TEMPS + 2)
#define TGSI_EXEC_TEMP_PRIMITIVE_C  2

#define TGSI_EXEC_TEMP_THREE_I      (TGSI_EXEC_NUM_TEMPS + 2)
#define TGSI_EXEC_TEMP_THREE_C      3

#define TGSI_EXEC_TEMP_HALF_I       (TGSI_EXEC_NUM_TEMPS + 3)
#define TGSI_EXEC_TEMP_HALF_C       0

/* 4 register buffer for various purposes */
#define TGSI_EXEC_TEMP_R0           (TGSI_EXEC_NUM_TEMPS + 4)
#define TGSI_EXEC_NUM_TEMP_R        4

#define TGSI_EXEC_TEMP_ADDR         (TGSI_EXEC_NUM_TEMPS + 8)
#define TGSI_EXEC_NUM_ADDRS         3

#define TGSI_EXEC_NUM_TEMP_EXTRAS   12



#define TGSI_EXEC_MAX_NESTING  32
#define TGSI_EXEC_MAX_COND_NESTING  TGSI_EXEC_MAX_NESTING
#define TGSI_EXEC_MAX_LOOP_NESTING  TGSI_EXEC_MAX_NESTING
#define TGSI_EXEC_MAX_SWITCH_NESTING TGSI_EXEC_MAX_NESTING
#define TGSI_EXEC_MAX_CALL_NESTING  TGSI_EXEC_MAX_NESTING

/* The maximum number of input attributes per vertex. For 2D
 * input register files, this is the stride between two 1D
 * arrays.
 */
#define TGSI_EXEC_MAX_INPUT_ATTRIBS 32

/* The maximum number of bytes per constant buffer.
 */
#define TGSI_EXEC_MAX_CONST_BUFFER_SIZE  (4096 * sizeof(float[4]))

/* The maximum number of vertices per primitive */
#define TGSI_MAX_PRIM_VERTICES 6

/* The maximum number of primitives to be generated */
#define TGSI_MAX_PRIMITIVES 64

/* The maximum total number of vertices */
#define TGSI_MAX_TOTAL_VERTICES (TGSI_MAX_PRIM_VERTICES * TGSI_MAX_PRIMITIVES * PIPE_MAX_ATTRIBS)

#define TGSI_MAX_MISC_INPUTS 8

/** function call/activation record */
struct tgsi_call_record
{
   uint CondStackTop;
   uint LoopStackTop;
   uint ContStackTop;
   int SwitchStackTop;
   int BreakStackTop;
   uint ReturnAddr;
};


/* Switch-case block state. */
struct tgsi_switch_record {
   uint mask;                          /**< execution mask */
   union tgsi_exec_channel selector;   /**< a value case statements are compared to */
   uint defaultMask;                   /**< non-execute mask for default case */
};


enum tgsi_break_type {
   TGSI_EXEC_BREAK_INSIDE_LOOP,
   TGSI_EXEC_BREAK_INSIDE_SWITCH
};


#define TGSI_EXEC_MAX_BREAK_STACK (TGSI_EXEC_MAX_LOOP_NESTING + TGSI_EXEC_MAX_SWITCH_NESTING)


/**
 * Run-time virtual machine state for executing TGSI shader.
 */
struct tgsi_exec_machine
{
   /* Total = program temporaries + internal temporaries
    */
   struct tgsi_exec_vector       Temps[TGSI_EXEC_NUM_TEMPS +
                                       TGSI_EXEC_NUM_TEMP_EXTRAS];

   float                         Imms[TGSI_EXEC_NUM_IMMEDIATES][4];

   float                         ImmArray[TGSI_EXEC_NUM_IMMEDIATES][4];

   struct tgsi_exec_vector       *Inputs;
   struct tgsi_exec_vector       *Outputs;

   /* System values */
   unsigned                      SysSemanticToIndex[TGSI_SEMANTIC_COUNT];
   struct tgsi_exec_vector       SystemValue[TGSI_MAX_MISC_INPUTS];

   struct tgsi_exec_vector       *Addrs;

   struct tgsi_sampler           *Sampler;

   struct tgsi_image             *Image;
   struct tgsi_buffer            *Buffer;
   unsigned                      ImmLimit;

   const void *Consts[PIPE_MAX_CONSTANT_BUFFERS];
   unsigned ConstsSize[PIPE_MAX_CONSTANT_BUFFERS];

   const struct tgsi_token       *Tokens;   /**< Declarations, instructions */
   enum pipe_shader_type         ShaderType; /**< PIPE_SHADER_x */

   /* GEOMETRY processor only. */
   unsigned                      *Primitives;
   unsigned                       NumOutputs;
   unsigned                       MaxGeometryShaderOutputs;
   unsigned                       MaxOutputVertices;

   /* FRAGMENT processor only. */
   const struct tgsi_interp_coef *InterpCoefs;
   struct tgsi_exec_vector       QuadPos;
   float                         Face;    /**< +1 if front facing, -1 if back facing */
   bool                          flatshade_color;

   /* Compute Only */
   void                          *LocalMem;
   unsigned                      LocalMemSize;

   /* See GLSL 4.50 specification for definition of helper invocations */
   uint NonHelperMask;  /**< non-helpers */
   /* Conditional execution masks */
   uint CondMask;  /**< For IF/ELSE/ENDIF */
   uint LoopMask;  /**< For BGNLOOP/ENDLOOP */
   uint ContMask;  /**< For loop CONT statements */
   uint FuncMask;  /**< For function calls */
   uint ExecMask;  /**< = CondMask & LoopMask */

   /* Current switch-case state. */
   struct tgsi_switch_record Switch;

   /* Current break type. */
   enum tgsi_break_type BreakType;

   /** Condition mask stack (for nested conditionals) */
   uint CondStack[TGSI_EXEC_MAX_COND_NESTING];
   int CondStackTop;

   /** Loop mask stack (for nested loops) */
   uint LoopStack[TGSI_EXEC_MAX_LOOP_NESTING];
   int LoopStackTop;

   /** Loop label stack */
   uint LoopLabelStack[TGSI_EXEC_MAX_LOOP_NESTING];
   int LoopLabelStackTop;

   /** Loop continue mask stack (see comments in tgsi_exec.c) */
   uint ContStack[TGSI_EXEC_MAX_LOOP_NESTING];
   int ContStackTop;

   /** Switch case stack */
   struct tgsi_switch_record SwitchStack[TGSI_EXEC_MAX_SWITCH_NESTING];
   int SwitchStackTop;

   enum tgsi_break_type BreakStack[TGSI_EXEC_MAX_BREAK_STACK];
   int BreakStackTop;

   /** Function execution mask stack (for executing subroutine code) */
   uint FuncStack[TGSI_EXEC_MAX_CALL_NESTING];
   int FuncStackTop;

   /** Function call stack for saving/restoring the program counter */
   struct tgsi_call_record CallStack[TGSI_EXEC_MAX_CALL_NESTING];
   int CallStackTop;

   struct tgsi_full_instruction *Instructions;
   uint NumInstructions;

   struct tgsi_full_declaration *Declarations;
   uint NumDeclarations;

   struct tgsi_declaration_sampler_view
      SamplerViews[PIPE_MAX_SHADER_SAMPLER_VIEWS];

   boolean UsedGeometryShader;

   int pc;
};

struct tgsi_exec_machine *
tgsi_exec_machine_create(enum pipe_shader_type shader_type);

void
tgsi_exec_machine_destroy(struct tgsi_exec_machine *mach);


void 
tgsi_exec_machine_bind_shader(
   struct tgsi_exec_machine *mach,
   const struct tgsi_token *tokens,
   struct tgsi_sampler *sampler,
   struct tgsi_image *image,
   struct tgsi_buffer *buffer);

uint
tgsi_exec_machine_run(
   struct tgsi_exec_machine *mach, int start_pc );


void
tgsi_exec_machine_free_data(struct tgsi_exec_machine *mach);


boolean
tgsi_check_soa_dependencies(const struct tgsi_full_instruction *inst);


extern void
tgsi_exec_set_constant_buffers(struct tgsi_exec_machine *mach,
                               unsigned num_bufs,
                               const void **bufs,
                               const unsigned *buf_sizes);


static inline int
tgsi_exec_get_shader_param(enum pipe_shader_cap param)
{
   switch(param) {
   case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
      return INT_MAX;
   case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      return TGSI_EXEC_MAX_NESTING;
   case PIPE_SHADER_CAP_MAX_INPUTS:
      return TGSI_EXEC_MAX_INPUT_ATTRIBS;
   case PIPE_SHADER_CAP_MAX_OUTPUTS:
      return 32;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE:
      return TGSI_EXEC_MAX_CONST_BUFFER_SIZE;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
      return PIPE_MAX_CONSTANT_BUFFERS;
   case PIPE_SHADER_CAP_MAX_TEMPS:
      return TGSI_EXEC_NUM_TEMPS;
   case PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
      return 1;
   case PIPE_SHADER_CAP_SUBROUTINES:
      return 1;
   case PIPE_SHADER_CAP_INTEGERS:
      return 1;
   case PIPE_SHADER_CAP_INT64_ATOMICS:
   case PIPE_SHADER_CAP_FP16:
      return 0;
   case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
      return PIPE_MAX_SAMPLERS;
   case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
      return PIPE_MAX_SHADER_SAMPLER_VIEWS;
   case PIPE_SHADER_CAP_PREFERRED_IR:
      return PIPE_SHADER_IR_TGSI;
   case PIPE_SHADER_CAP_SUPPORTED_IRS:
      return 1 << PIPE_SHADER_IR_TGSI;
   case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_TGSI_DFRACEXP_DLDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_LDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
      return 1;
   case PIPE_SHADER_CAP_TGSI_DROUND_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_FMA_SUPPORTED:
   case PIPE_SHADER_CAP_LOWER_IF_THRESHOLD:
   case PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS:
      return 0;
   case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
      return PIPE_MAX_SHADER_BUFFERS;
   case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
      return PIPE_MAX_SHADER_IMAGES;

   case PIPE_SHADER_CAP_MAX_UNROLL_ITERATIONS_HINT:
      return 32;
   }
   /* if we get here, we missed a shader cap above (and should have seen
    * a compiler warning.)
    */
   return 0;
}

#if defined __cplusplus
} /* extern "C" */
#endif

#endif /* TGSI_EXEC_H */
