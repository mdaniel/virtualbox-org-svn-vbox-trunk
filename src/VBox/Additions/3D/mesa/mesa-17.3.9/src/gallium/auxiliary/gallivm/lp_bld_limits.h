/**************************************************************************
 *
 * Copyright 2010-2012 VMware, Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 **************************************************************************/


#ifndef LP_BLD_LIMITS_H_
#define LP_BLD_LIMITS_H_


#include <limits.h>

#include "pipe/p_state.h"
#include "pipe/p_defines.h"


/*
 * TGSI translation limits.
 *
 * Some are slightly above SM 3.0 requirements to give some wiggle room to
 * the state trackers.
 */

#define LP_MAX_TGSI_TEMPS 4096

#define LP_MAX_TGSI_ADDRS 16

#define LP_MAX_TGSI_IMMEDIATES 4096

#define LP_MAX_TGSI_CONSTS 4096

#define LP_MAX_TGSI_CONST_BUFFERS 16

#define LP_MAX_TGSI_CONST_BUFFER_SIZE (LP_MAX_TGSI_CONSTS * sizeof(float[4]))

/*
 * For quick access we cache registers in statically
 * allocated arrays. Here we define the maximum size
 * for those arrays.
 */
#define LP_MAX_INLINED_TEMPS 256

#define LP_MAX_INLINED_IMMEDIATES 256

/**
 * Maximum control flow nesting
 *
 * SM4.0 requires 64 (per subroutine actually, subroutine nesting itself is 32)
 * SM3.0 requires 24 (most likely per subroutine too)
 * add 2 more (some translation could add one more)
 */
#define LP_MAX_TGSI_NESTING 66

/**
 * Maximum iterations before loop termination
 * Shared between every loop in a TGSI shader
 */
#define LP_MAX_TGSI_LOOP_ITERATIONS 65535


/**
 * Some of these limits are actually infinite (i.e., only limited by available
 * memory), however advertising INT_MAX would cause some test problems to
 * actually try to allocate the maximum and run out of memory and crash.  So
 * stick with something reasonable here.
 */
static inline int
gallivm_get_shader_param(enum pipe_shader_cap param)
{
   switch(param) {
   case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
      return 1 * 1024 * 1024;
   case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      return LP_MAX_TGSI_NESTING;
   case PIPE_SHADER_CAP_MAX_INPUTS:
      return 32;
   case PIPE_SHADER_CAP_MAX_OUTPUTS:
      return 32;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE:
      return LP_MAX_TGSI_CONST_BUFFER_SIZE;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
      return PIPE_MAX_CONSTANT_BUFFERS;
   case PIPE_SHADER_CAP_MAX_TEMPS:
      return LP_MAX_TGSI_TEMPS;
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
   case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
      return 1;
   case PIPE_SHADER_CAP_TGSI_DROUND_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_DFRACEXP_DLDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_LDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_FMA_SUPPORTED:
   case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
   case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
   case PIPE_SHADER_CAP_LOWER_IF_THRESHOLD:
   case PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS:
      return 0;
   case PIPE_SHADER_CAP_MAX_UNROLL_ITERATIONS_HINT:
      return 32;
   }
   /* if we get here, we missed a shader cap above (and should have seen
    * a compiler warning.)
    */
   return 0;
}


#endif /* LP_BLD_LIMITS_H_ */
