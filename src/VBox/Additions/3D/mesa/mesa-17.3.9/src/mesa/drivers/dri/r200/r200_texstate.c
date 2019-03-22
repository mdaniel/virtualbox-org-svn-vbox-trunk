/*
Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.

The Weather Channel (TM) funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86 license.
This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keithw@vmware.com>
 */

#include "main/glheader.h"
#include "main/imports.h"
#include "main/context.h"
#include "main/macros.h"
#include "main/state.h"
#include "main/teximage.h"
#include "main/texobj.h"
#include "main/enums.h"

#include "radeon_common.h"
#include "radeon_mipmap_tree.h"
#include "r200_context.h"
#include "r200_state.h"
#include "r200_ioctl.h"
#include "r200_swtcl.h"
#include "r200_tex.h"
#include "r200_tcl.h"

#define VALID_FORMAT(f) ( ((f) <= MESA_FORMAT_RGBA_DXT5) \
                             && (tx_table_be[f].format != 0xffffffff) )

/* ================================================================
 * Texture combine functions
 */

/* GL_ARB_texture_env_combine support
 */

/* The color tables have combine functions for GL_SRC_COLOR,
 * GL_ONE_MINUS_SRC_COLOR, GL_SRC_ALPHA and GL_ONE_MINUS_SRC_ALPHA.
 */
static GLuint r200_register_color[][R200_MAX_TEXTURE_UNITS] =
{
   {
      R200_TXC_ARG_A_R0_COLOR,
      R200_TXC_ARG_A_R1_COLOR,
      R200_TXC_ARG_A_R2_COLOR,
      R200_TXC_ARG_A_R3_COLOR,
      R200_TXC_ARG_A_R4_COLOR,
      R200_TXC_ARG_A_R5_COLOR
   },
   {
      R200_TXC_ARG_A_R0_COLOR | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R1_COLOR | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R2_COLOR | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R3_COLOR | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R4_COLOR | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R5_COLOR | R200_TXC_COMP_ARG_A
   },
   {
      R200_TXC_ARG_A_R0_ALPHA,
      R200_TXC_ARG_A_R1_ALPHA,
      R200_TXC_ARG_A_R2_ALPHA,
      R200_TXC_ARG_A_R3_ALPHA,
      R200_TXC_ARG_A_R4_ALPHA,
      R200_TXC_ARG_A_R5_ALPHA
   },
   {
      R200_TXC_ARG_A_R0_ALPHA | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R1_ALPHA | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R2_ALPHA | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R3_ALPHA | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R4_ALPHA | R200_TXC_COMP_ARG_A,
      R200_TXC_ARG_A_R5_ALPHA | R200_TXC_COMP_ARG_A
   },
};

static GLuint r200_tfactor_color[] =
{
   R200_TXC_ARG_A_TFACTOR_COLOR,
   R200_TXC_ARG_A_TFACTOR_COLOR | R200_TXC_COMP_ARG_A,
   R200_TXC_ARG_A_TFACTOR_ALPHA,
   R200_TXC_ARG_A_TFACTOR_ALPHA | R200_TXC_COMP_ARG_A
};

static GLuint r200_tfactor1_color[] =
{
   R200_TXC_ARG_A_TFACTOR1_COLOR,
   R200_TXC_ARG_A_TFACTOR1_COLOR | R200_TXC_COMP_ARG_A,
   R200_TXC_ARG_A_TFACTOR1_ALPHA,
   R200_TXC_ARG_A_TFACTOR1_ALPHA | R200_TXC_COMP_ARG_A
};

static GLuint r200_primary_color[] =
{
   R200_TXC_ARG_A_DIFFUSE_COLOR,
   R200_TXC_ARG_A_DIFFUSE_COLOR | R200_TXC_COMP_ARG_A,
   R200_TXC_ARG_A_DIFFUSE_ALPHA,
   R200_TXC_ARG_A_DIFFUSE_ALPHA | R200_TXC_COMP_ARG_A
};

/* GL_ZERO table - indices 0-3
 * GL_ONE  table - indices 1-4
 */
static GLuint r200_zero_color[] =
{
   R200_TXC_ARG_A_ZERO,
   R200_TXC_ARG_A_ZERO | R200_TXC_COMP_ARG_A,
   R200_TXC_ARG_A_ZERO,
   R200_TXC_ARG_A_ZERO | R200_TXC_COMP_ARG_A,
   R200_TXC_ARG_A_ZERO
};

/* The alpha tables only have GL_SRC_ALPHA and GL_ONE_MINUS_SRC_ALPHA.
 */
static GLuint r200_register_alpha[][R200_MAX_TEXTURE_UNITS] =
{
   {
      R200_TXA_ARG_A_R0_ALPHA,
      R200_TXA_ARG_A_R1_ALPHA,
      R200_TXA_ARG_A_R2_ALPHA,
      R200_TXA_ARG_A_R3_ALPHA,
      R200_TXA_ARG_A_R4_ALPHA,
      R200_TXA_ARG_A_R5_ALPHA
   },
   {
      R200_TXA_ARG_A_R0_ALPHA | R200_TXA_COMP_ARG_A,
      R200_TXA_ARG_A_R1_ALPHA | R200_TXA_COMP_ARG_A,
      R200_TXA_ARG_A_R2_ALPHA | R200_TXA_COMP_ARG_A,
      R200_TXA_ARG_A_R3_ALPHA | R200_TXA_COMP_ARG_A,
      R200_TXA_ARG_A_R4_ALPHA | R200_TXA_COMP_ARG_A,
      R200_TXA_ARG_A_R5_ALPHA | R200_TXA_COMP_ARG_A
   },
};

static GLuint r200_tfactor_alpha[] =
{
   R200_TXA_ARG_A_TFACTOR_ALPHA,
   R200_TXA_ARG_A_TFACTOR_ALPHA | R200_TXA_COMP_ARG_A
};

static GLuint r200_tfactor1_alpha[] =
{
   R200_TXA_ARG_A_TFACTOR1_ALPHA,
   R200_TXA_ARG_A_TFACTOR1_ALPHA | R200_TXA_COMP_ARG_A
};

static GLuint r200_primary_alpha[] =
{
   R200_TXA_ARG_A_DIFFUSE_ALPHA,
   R200_TXA_ARG_A_DIFFUSE_ALPHA | R200_TXA_COMP_ARG_A
};

/* GL_ZERO table - indices 0-1
 * GL_ONE  table - indices 1-2
 */
static GLuint r200_zero_alpha[] =
{
   R200_TXA_ARG_A_ZERO,
   R200_TXA_ARG_A_ZERO | R200_TXA_COMP_ARG_A,
   R200_TXA_ARG_A_ZERO,
};


/* Extract the arg from slot A, shift it into the correct argument slot
 * and set the corresponding complement bit.
 */
#define R200_COLOR_ARG( n, arg )			\
do {							\
   color_combine |=					\
      ((color_arg[n] & R200_TXC_ARG_A_MASK)		\
       << R200_TXC_ARG_##arg##_SHIFT);			\
   color_combine |=					\
      ((color_arg[n] >> R200_TXC_COMP_ARG_A_SHIFT)	\
       << R200_TXC_COMP_ARG_##arg##_SHIFT);		\
} while (0)

#define R200_ALPHA_ARG( n, arg )			\
do {							\
   alpha_combine |=					\
      ((alpha_arg[n] & R200_TXA_ARG_A_MASK)		\
       << R200_TXA_ARG_##arg##_SHIFT);			\
   alpha_combine |=					\
      ((alpha_arg[n] >> R200_TXA_COMP_ARG_A_SHIFT)	\
       << R200_TXA_COMP_ARG_##arg##_SHIFT);		\
} while (0)


/* ================================================================
 * Texture unit state management
 */

static GLboolean r200UpdateTextureEnv( struct gl_context *ctx, int unit, int slot, GLuint replaceargs )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
   GLuint color_combine, alpha_combine;
   GLuint color_scale = rmesa->hw.pix[slot].cmd[PIX_PP_TXCBLEND2] &
      ~(R200_TXC_SCALE_MASK | R200_TXC_OUTPUT_REG_MASK | R200_TXC_TFACTOR_SEL_MASK |
	R200_TXC_TFACTOR1_SEL_MASK);
   GLuint alpha_scale = rmesa->hw.pix[slot].cmd[PIX_PP_TXABLEND2] &
      ~(R200_TXA_DOT_ALPHA | R200_TXA_SCALE_MASK | R200_TXA_OUTPUT_REG_MASK |
	R200_TXA_TFACTOR_SEL_MASK | R200_TXA_TFACTOR1_SEL_MASK);

   if ( R200_DEBUG & RADEON_TEXTURE ) {
      fprintf( stderr, "%s( %p, %d )\n", __func__, (void *)ctx, unit );
   }

   /* Set the texture environment state.  Isn't this nice and clean?
    * The chip will automagically set the texture alpha to 0xff when
    * the texture format does not include an alpha component.  This
    * reduces the amount of special-casing we have to do, alpha-only
    * textures being a notable exception.
    */

   color_scale |= ((rmesa->state.texture.unit[unit].outputreg + 1) << R200_TXC_OUTPUT_REG_SHIFT) |
			(unit << R200_TXC_TFACTOR_SEL_SHIFT) |
			(replaceargs << R200_TXC_TFACTOR1_SEL_SHIFT);
   alpha_scale |= ((rmesa->state.texture.unit[unit].outputreg + 1) << R200_TXA_OUTPUT_REG_SHIFT) |
			(unit << R200_TXA_TFACTOR_SEL_SHIFT) |
			(replaceargs << R200_TXA_TFACTOR1_SEL_SHIFT);

   if ( !texUnit->_Current ) {
      assert( unit == 0);
      color_combine = R200_TXC_ARG_A_ZERO | R200_TXC_ARG_B_ZERO
	  | R200_TXC_ARG_C_DIFFUSE_COLOR | R200_TXC_OP_MADD;
      alpha_combine = R200_TXA_ARG_A_ZERO | R200_TXA_ARG_B_ZERO
	  | R200_TXA_ARG_C_DIFFUSE_ALPHA | R200_TXA_OP_MADD;
   }
   else {
      GLuint color_arg[3], alpha_arg[3];
      GLuint i;
      const GLuint numColorArgs = texUnit->_CurrentCombine->_NumArgsRGB;
      const GLuint numAlphaArgs = texUnit->_CurrentCombine->_NumArgsA;
      GLuint RGBshift = texUnit->_CurrentCombine->ScaleShiftRGB;
      GLuint Ashift = texUnit->_CurrentCombine->ScaleShiftA;


      const GLint replaceoprgb =
	 ctx->Texture.Unit[replaceargs]._CurrentCombine->OperandRGB[0] - GL_SRC_COLOR;
      const GLint replaceopa =
	 ctx->Texture.Unit[replaceargs]._CurrentCombine->OperandA[0] - GL_SRC_ALPHA;

      /* Step 1:
       * Extract the color and alpha combine function arguments.
       */
      for ( i = 0 ; i < numColorArgs ; i++ ) {
	 GLint op = texUnit->_CurrentCombine->OperandRGB[i] - GL_SRC_COLOR;
	 const GLint srcRGBi = texUnit->_CurrentCombine->SourceRGB[i];
	 assert(op >= 0);
	 assert(op <= 3);
	 switch ( srcRGBi ) {
	 case GL_TEXTURE:
	    color_arg[i] = r200_register_color[op][unit];
	    break;
	 case GL_CONSTANT:
	    color_arg[i] = r200_tfactor_color[op];
	    break;
	 case GL_PRIMARY_COLOR:
	    color_arg[i] = r200_primary_color[op];
	    break;
	 case GL_PREVIOUS:
	    if (replaceargs != unit) {
	       const GLint srcRGBreplace =
		  ctx->Texture.Unit[replaceargs]._CurrentCombine->SourceRGB[0];
	       if (op >= 2) {
		  op = op ^ replaceopa;
	       }
	       else {
		  op = op ^ replaceoprgb;
	       }
	       switch (srcRGBreplace) {
	       case GL_TEXTURE:
		  color_arg[i] = r200_register_color[op][replaceargs];
		  break;
	       case GL_CONSTANT:
		  color_arg[i] = r200_tfactor1_color[op];
		  break;
	       case GL_PRIMARY_COLOR:
		  color_arg[i] = r200_primary_color[op];
		  break;
	       case GL_PREVIOUS:
		  if (slot == 0)
		     color_arg[i] = r200_primary_color[op];
		  else
		     color_arg[i] = r200_register_color[op]
			[rmesa->state.texture.unit[replaceargs - 1].outputreg];
		  break;
	       case GL_ZERO:
		  color_arg[i] = r200_zero_color[op];
		  break;
	       case GL_ONE:
		  color_arg[i] = r200_zero_color[op+1];
		  break;
	       case GL_TEXTURE0:
	       case GL_TEXTURE1:
	       case GL_TEXTURE2:
	       case GL_TEXTURE3:
	       case GL_TEXTURE4:
	       case GL_TEXTURE5:
		  color_arg[i] = r200_register_color[op][srcRGBreplace - GL_TEXTURE0];
		  break;
	       default:
	       return GL_FALSE;
	       }
	    }
	    else {
	       if (slot == 0)
		  color_arg[i] = r200_primary_color[op];
	       else
		  color_arg[i] = r200_register_color[op]
		     [rmesa->state.texture.unit[unit - 1].outputreg];
            }
	    break;
	 case GL_ZERO:
	    color_arg[i] = r200_zero_color[op];
	    break;
	 case GL_ONE:
	    color_arg[i] = r200_zero_color[op+1];
	    break;
	 case GL_TEXTURE0:
	 case GL_TEXTURE1:
	 case GL_TEXTURE2:
	 case GL_TEXTURE3:
	 case GL_TEXTURE4:
	 case GL_TEXTURE5:
	    color_arg[i] = r200_register_color[op][srcRGBi - GL_TEXTURE0];
	    break;
	 default:
	    return GL_FALSE;
	 }
      }

      for ( i = 0 ; i < numAlphaArgs ; i++ ) {
	 GLint op = texUnit->_CurrentCombine->OperandA[i] - GL_SRC_ALPHA;
	 const GLint srcAi = texUnit->_CurrentCombine->SourceA[i];
	 assert(op >= 0);
	 assert(op <= 1);
	 switch ( srcAi ) {
	 case GL_TEXTURE:
	    alpha_arg[i] = r200_register_alpha[op][unit];
	    break;
	 case GL_CONSTANT:
	    alpha_arg[i] = r200_tfactor_alpha[op];
	    break;
	 case GL_PRIMARY_COLOR:
	    alpha_arg[i] = r200_primary_alpha[op];
	    break;
	 case GL_PREVIOUS:
	    if (replaceargs != unit) {
	       const GLint srcAreplace =
		  ctx->Texture.Unit[replaceargs]._CurrentCombine->SourceA[0];
	       op = op ^ replaceopa;
	       switch (srcAreplace) {
	       case GL_TEXTURE:
		  alpha_arg[i] = r200_register_alpha[op][replaceargs];
		  break;
	       case GL_CONSTANT:
		  alpha_arg[i] = r200_tfactor1_alpha[op];
		  break;
	       case GL_PRIMARY_COLOR:
		  alpha_arg[i] = r200_primary_alpha[op];
		  break;
	       case GL_PREVIOUS:
		  if (slot == 0)
		     alpha_arg[i] = r200_primary_alpha[op];
		  else
		     alpha_arg[i] = r200_register_alpha[op]
			[rmesa->state.texture.unit[replaceargs - 1].outputreg];
		  break;
	       case GL_ZERO:
		  alpha_arg[i] = r200_zero_alpha[op];
		  break;
	       case GL_ONE:
		  alpha_arg[i] = r200_zero_alpha[op+1];
		  break;
	       case GL_TEXTURE0:
	       case GL_TEXTURE1:
	       case GL_TEXTURE2:
	       case GL_TEXTURE3:
	       case GL_TEXTURE4:
	       case GL_TEXTURE5:
		  alpha_arg[i] = r200_register_alpha[op][srcAreplace - GL_TEXTURE0];
		  break;
	       default:
	       return GL_FALSE;
	       }
	    }
	    else {
	       if (slot == 0)
		  alpha_arg[i] = r200_primary_alpha[op];
	       else
		  alpha_arg[i] = r200_register_alpha[op]
		    [rmesa->state.texture.unit[unit - 1].outputreg];
            }
	    break;
	 case GL_ZERO:
	    alpha_arg[i] = r200_zero_alpha[op];
	    break;
	 case GL_ONE:
	    alpha_arg[i] = r200_zero_alpha[op+1];
	    break;
	 case GL_TEXTURE0:
	 case GL_TEXTURE1:
	 case GL_TEXTURE2:
	 case GL_TEXTURE3:
	 case GL_TEXTURE4:
	 case GL_TEXTURE5:
	    alpha_arg[i] = r200_register_alpha[op][srcAi - GL_TEXTURE0];
	    break;
	 default:
	    return GL_FALSE;
	 }
      }

      /* Step 2:
       * Build up the color and alpha combine functions.
       */
      switch ( texUnit->_CurrentCombine->ModeRGB ) {
      case GL_REPLACE:
	 color_combine = (R200_TXC_ARG_A_ZERO |
			  R200_TXC_ARG_B_ZERO |
			  R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, C );
	 break;
      case GL_MODULATE:
	 color_combine = (R200_TXC_ARG_C_ZERO |
			  R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, B );
	 break;
      case GL_ADD:
	 color_combine = (R200_TXC_ARG_B_ZERO |
			  R200_TXC_COMP_ARG_B | 
			  R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 break;
      case GL_ADD_SIGNED:
	 color_combine = (R200_TXC_ARG_B_ZERO |
			  R200_TXC_COMP_ARG_B |
			  R200_TXC_BIAS_ARG_C |	/* new */
			  R200_TXC_OP_MADD); /* was ADDSIGNED */
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 break;
      case GL_SUBTRACT:
	 color_combine = (R200_TXC_ARG_B_ZERO |
			  R200_TXC_COMP_ARG_B | 
			  R200_TXC_NEG_ARG_C |
			  R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 break;
      case GL_INTERPOLATE:
	 color_combine = (R200_TXC_OP_LERP);
	 R200_COLOR_ARG( 0, B );
	 R200_COLOR_ARG( 1, A );
	 R200_COLOR_ARG( 2, C );
	 break;

      case GL_DOT3_RGB_EXT:
      case GL_DOT3_RGBA_EXT:
	 /* The EXT version of the DOT3 extension does not support the
	  * scale factor, but the ARB version (and the version in OpenGL
	  * 1.3) does.
	  */
	 RGBshift = 0;
	 /* FALLTHROUGH */

      case GL_DOT3_RGB:
      case GL_DOT3_RGBA:
	 /* DOT3 works differently on R200 than on R100.  On R100, just
	  * setting the DOT3 mode did everything for you.  On R200, the
	  * driver has to enable the biasing and scale in the inputs to
	  * put them in the proper [-1,1] range.  This is what the 4x and
	  * the -0.5 in the DOT3 spec do.  The post-scale is then set
	  * normally.
	  */

	 color_combine = (R200_TXC_ARG_C_ZERO |
			  R200_TXC_OP_DOT3 |
			  R200_TXC_BIAS_ARG_A |
			  R200_TXC_BIAS_ARG_B |
			  R200_TXC_SCALE_ARG_A |
			  R200_TXC_SCALE_ARG_B);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, B );
	 break;

      case GL_MODULATE_ADD_ATI:
	 color_combine = (R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 R200_COLOR_ARG( 2, B );
	 break;
      case GL_MODULATE_SIGNED_ADD_ATI:
	 color_combine = (R200_TXC_BIAS_ARG_C |	/* new */
			  R200_TXC_OP_MADD); /* was ADDSIGNED */
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 R200_COLOR_ARG( 2, B );
	 break;
      case GL_MODULATE_SUBTRACT_ATI:
	 color_combine = (R200_TXC_NEG_ARG_C |
			  R200_TXC_OP_MADD);
	 R200_COLOR_ARG( 0, A );
	 R200_COLOR_ARG( 1, C );
	 R200_COLOR_ARG( 2, B );
	 break;
      default:
	 return GL_FALSE;
      }

      switch ( texUnit->_CurrentCombine->ModeA ) {
      case GL_REPLACE:
	 alpha_combine = (R200_TXA_ARG_A_ZERO |
			  R200_TXA_ARG_B_ZERO |
			  R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, C );
	 break;
      case GL_MODULATE:
	 alpha_combine = (R200_TXA_ARG_C_ZERO |
			  R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, B );
	 break;
      case GL_ADD:
	 alpha_combine = (R200_TXA_ARG_B_ZERO |
			  R200_TXA_COMP_ARG_B |
			  R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 break;
      case GL_ADD_SIGNED:
	 alpha_combine = (R200_TXA_ARG_B_ZERO |
			  R200_TXA_COMP_ARG_B |
			  R200_TXA_BIAS_ARG_C |	/* new */
			  R200_TXA_OP_MADD); /* was ADDSIGNED */
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 break;
      case GL_SUBTRACT:
	 alpha_combine = (R200_TXA_ARG_B_ZERO |
			  R200_TXA_COMP_ARG_B |
			  R200_TXA_NEG_ARG_C |
			  R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 break;
      case GL_INTERPOLATE:
	 alpha_combine = (R200_TXA_OP_LERP);
	 R200_ALPHA_ARG( 0, B );
	 R200_ALPHA_ARG( 1, A );
	 R200_ALPHA_ARG( 2, C );
	 break;

      case GL_MODULATE_ADD_ATI:
	 alpha_combine = (R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 R200_ALPHA_ARG( 2, B );
	 break;
      case GL_MODULATE_SIGNED_ADD_ATI:
	 alpha_combine = (R200_TXA_BIAS_ARG_C |	/* new */
			  R200_TXA_OP_MADD); /* was ADDSIGNED */
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 R200_ALPHA_ARG( 2, B );
	 break;
      case GL_MODULATE_SUBTRACT_ATI:
	 alpha_combine = (R200_TXA_NEG_ARG_C |
			  R200_TXA_OP_MADD);
	 R200_ALPHA_ARG( 0, A );
	 R200_ALPHA_ARG( 1, C );
	 R200_ALPHA_ARG( 2, B );
	 break;
      default:
	 return GL_FALSE;
      }

      if ( (texUnit->_CurrentCombine->ModeRGB == GL_DOT3_RGBA_EXT)
	   || (texUnit->_CurrentCombine->ModeRGB == GL_DOT3_RGBA) ) {
	 alpha_scale |= R200_TXA_DOT_ALPHA;
	 Ashift = RGBshift;
      }

      /* Step 3:
       * Apply the scale factor.
       */
      color_scale |= (RGBshift << R200_TXC_SCALE_SHIFT);
      alpha_scale |= (Ashift   << R200_TXA_SCALE_SHIFT);

      /* All done!
       */
   }

   if ( rmesa->hw.pix[slot].cmd[PIX_PP_TXCBLEND] != color_combine ||
	rmesa->hw.pix[slot].cmd[PIX_PP_TXABLEND] != alpha_combine ||
	rmesa->hw.pix[slot].cmd[PIX_PP_TXCBLEND2] != color_scale ||
	rmesa->hw.pix[slot].cmd[PIX_PP_TXABLEND2] != alpha_scale) {
      R200_STATECHANGE( rmesa, pix[slot] );
      rmesa->hw.pix[slot].cmd[PIX_PP_TXCBLEND] = color_combine;
      rmesa->hw.pix[slot].cmd[PIX_PP_TXABLEND] = alpha_combine;
      rmesa->hw.pix[slot].cmd[PIX_PP_TXCBLEND2] = color_scale;
      rmesa->hw.pix[slot].cmd[PIX_PP_TXABLEND2] = alpha_scale;
   }

   return GL_TRUE;
}

void r200SetTexBuffer2(__DRIcontext *pDRICtx, GLint target, GLint texture_format,
		       __DRIdrawable *dPriv)
{
	struct gl_texture_object *texObj;
	struct gl_texture_image *texImage;
	struct radeon_renderbuffer *rb;
	radeon_texture_image *rImage;
	radeonContextPtr radeon;
	struct radeon_framebuffer *rfb;
	radeonTexObjPtr t;
	uint32_t pitch_val;
	mesa_format texFormat;

	radeon = pDRICtx->driverPrivate;

	rfb = dPriv->driverPrivate;
	texObj = _mesa_get_current_tex_object(&radeon->glCtx, target);
	texImage = _mesa_get_tex_image(&radeon->glCtx, texObj, target, 0);

	rImage = get_radeon_texture_image(texImage);
	t = radeon_tex_obj(texObj);
        if (t == NULL) {
    	    return;
    	}

	radeon_update_renderbuffers(pDRICtx, dPriv, GL_TRUE);
	rb = rfb->color_rb[0];
	if (rb->bo == NULL) {
		/* Failed to BO for the buffer */
		return;
	}

	_mesa_lock_texture(&radeon->glCtx, texObj);
	if (t->bo) {
		radeon_bo_unref(t->bo);
		t->bo = NULL;
	}
	if (rImage->bo) {
		radeon_bo_unref(rImage->bo);
		rImage->bo = NULL;
	}

	radeon_miptree_unreference(&t->mt);
	radeon_miptree_unreference(&rImage->mt);

	rImage->bo = rb->bo;
	radeon_bo_ref(rImage->bo);
	t->bo = rb->bo;
	radeon_bo_ref(t->bo);
	t->tile_bits = 0;
	t->image_override = GL_TRUE;
	t->override_offset = 0;
	t->pp_txpitch &= (1 << 13) -1;
	pitch_val = rb->pitch;
	switch (rb->cpp) {
	case 4:
		if (texture_format == __DRI_TEXTURE_FORMAT_RGB) {
			texFormat = MESA_FORMAT_BGR_UNORM8;
			t->pp_txformat = tx_table_le[MESA_FORMAT_BGR_UNORM8].format;
		}
		else {
			texFormat = MESA_FORMAT_B8G8R8A8_UNORM;
			t->pp_txformat = tx_table_le[MESA_FORMAT_B8G8R8A8_UNORM].format;
		}
		t->pp_txfilter |= tx_table_le[MESA_FORMAT_B8G8R8A8_UNORM].filter;
		break;
	case 3:
	default:
		texFormat = MESA_FORMAT_BGR_UNORM8;
		t->pp_txformat = tx_table_le[MESA_FORMAT_BGR_UNORM8].format;
		t->pp_txfilter |= tx_table_le[MESA_FORMAT_BGR_UNORM8].filter;
		break;
	case 2:
		texFormat = MESA_FORMAT_B5G6R5_UNORM;
		t->pp_txformat = tx_table_le[MESA_FORMAT_B5G6R5_UNORM].format;
		t->pp_txfilter |= tx_table_le[MESA_FORMAT_B5G6R5_UNORM].filter;
		break;
	}

	_mesa_init_teximage_fields(&radeon->glCtx, texImage,
				   rb->base.Base.Width, rb->base.Base.Height,
				   1, 0,
				   rb->cpp, texFormat);
	rImage->base.RowStride = rb->pitch / rb->cpp;


        t->pp_txsize = ((rb->base.Base.Width - 1) << RADEON_TEX_USIZE_SHIFT)
		   | ((rb->base.Base.Height - 1) << RADEON_TEX_VSIZE_SHIFT);

	if (target == GL_TEXTURE_RECTANGLE_NV) {
		t->pp_txformat |= R200_TXFORMAT_NON_POWER2;
		t->pp_txpitch = pitch_val;
		t->pp_txpitch -= 32;
	} else {
		t->pp_txformat &= ~(R200_TXFORMAT_WIDTH_MASK |
				    R200_TXFORMAT_HEIGHT_MASK |
				    R200_TXFORMAT_CUBIC_MAP_ENABLE |
				    R200_TXFORMAT_F5_WIDTH_MASK |
				    R200_TXFORMAT_F5_HEIGHT_MASK);
		t->pp_txformat |= ((texImage->WidthLog2 << R200_TXFORMAT_WIDTH_SHIFT) |
				   (texImage->HeightLog2 << R200_TXFORMAT_HEIGHT_SHIFT));
	}

	t->validated = GL_TRUE;
	_mesa_unlock_texture(&radeon->glCtx, texObj);
	return;
}


void r200SetTexBuffer(__DRIcontext *pDRICtx, GLint target, __DRIdrawable *dPriv)
{
        r200SetTexBuffer2(pDRICtx, target, __DRI_TEXTURE_FORMAT_RGBA, dPriv);
}


#define REF_COLOR 1
#define REF_ALPHA 2

static GLboolean r200UpdateAllTexEnv( struct gl_context *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLint i, j, currslot;
   GLint maxunitused = -1;
   GLboolean texregfree[6] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
   GLubyte stageref[7] = {0, 0, 0, 0, 0, 0, 0};
   GLint nextunit[R200_MAX_TEXTURE_UNITS] = {0, 0, 0, 0, 0, 0};
   GLint currentnext = -1;
   GLboolean ok;

   /* find highest used unit */
   for ( j = 0; j < R200_MAX_TEXTURE_UNITS; j++) {
      if (ctx->Texture.Unit[j]._Current) {
	 maxunitused = j;
      }
   }
   stageref[maxunitused + 1] = REF_COLOR | REF_ALPHA;

   for ( j = maxunitused; j >= 0; j-- ) {
      const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[j];

      rmesa->state.texture.unit[j].outputreg = -1;

      if (stageref[j + 1]) {

	 /* use the lowest available reg. That gets us automatically reg0 for the last stage.
	    need this even for disabled units, as it may get referenced due to the replace
	    optimization */
	 for ( i = 0 ; i < R200_MAX_TEXTURE_UNITS; i++ ) {
	    if (texregfree[i]) {
	       rmesa->state.texture.unit[j].outputreg = i;
	       break;
	    }
	 }
	 if (rmesa->state.texture.unit[j].outputreg == -1) {
	    /* no more free regs we can use. Need a fallback :-( */
	    return GL_FALSE;
         }

         nextunit[j] = currentnext;

         if (!texUnit->_Current) {
	 /* the not enabled stages are referenced "indirectly",
            must not cut off the lower stages */
	    stageref[j] = REF_COLOR | REF_ALPHA;
	    continue;
         }
	 currentnext = j;
 
	 const GLuint numColorArgs = texUnit->_CurrentCombine->_NumArgsRGB;
	 const GLuint numAlphaArgs = texUnit->_CurrentCombine->_NumArgsA;
	 const GLboolean isdot3rgba = (texUnit->_CurrentCombine->ModeRGB == GL_DOT3_RGBA) ||
				      (texUnit->_CurrentCombine->ModeRGB == GL_DOT3_RGBA_EXT);


	 /* check if we need the color part, special case for dot3_rgba
	    as if only the alpha part is referenced later on it still is using the color part */
	 if ((stageref[j + 1] & REF_COLOR) || isdot3rgba) {
	    for ( i = 0 ; i < numColorArgs ; i++ ) {
	       const GLuint srcRGBi = texUnit->_CurrentCombine->SourceRGB[i];
	       const GLuint op = texUnit->_CurrentCombine->OperandRGB[i];
	       switch ( srcRGBi ) {
	       case GL_PREVIOUS:
		  /* op 0/1 are referencing color, op 2/3 alpha */
		  stageref[j] |= (op >> 1) + 1;
	          break;
	       case GL_TEXTURE:
		  texregfree[j] = GL_FALSE;
		  break;
	       case GL_TEXTURE0:
	       case GL_TEXTURE1:
	       case GL_TEXTURE2:
	       case GL_TEXTURE3:
	       case GL_TEXTURE4:
	       case GL_TEXTURE5:
		  texregfree[srcRGBi - GL_TEXTURE0] = GL_FALSE;
	          break;
	       default: /* don't care about other sources here */
		  break;
	       }
	    }
	 }

	 /* alpha args are ignored for dot3_rgba */
	 if ((stageref[j + 1] & REF_ALPHA) && !isdot3rgba) {

	    for ( i = 0 ; i < numAlphaArgs ; i++ ) {
	       const GLuint srcAi = texUnit->_CurrentCombine->SourceA[i];
	       switch ( srcAi ) {
	       case GL_PREVIOUS:
		  stageref[j] |= REF_ALPHA;
		  break;
	       case GL_TEXTURE:
		  texregfree[j] = GL_FALSE;
		  break;
	       case GL_TEXTURE0:
	       case GL_TEXTURE1:
	       case GL_TEXTURE2:
	       case GL_TEXTURE3:
	       case GL_TEXTURE4:
	       case GL_TEXTURE5:
		  texregfree[srcAi - GL_TEXTURE0] = GL_FALSE;
		  break;
	       default: /* don't care about other sources here */
		  break;
	       }
	    }
	 }
      }
   }

   /* don't enable texture sampling for units if the result is not used */
   for (i = 0; i < R200_MAX_TEXTURE_UNITS; i++) {
      if (ctx->Texture.Unit[i]._Current && !texregfree[i])
	 rmesa->state.texture.unit[i].unitneeded = 1 << _mesa_tex_target_to_index(ctx, ctx->Texture.Unit[i]._Current->Target);
      else rmesa->state.texture.unit[i].unitneeded = 0;
   }

   ok = GL_TRUE;
   currslot = 0;
   rmesa->state.envneeded = 1;

   i = 0;
   while ((i <= maxunitused) && (i >= 0)) {
      /* only output instruction if the results are referenced */
      if (ctx->Texture.Unit[i]._Current && stageref[i+1]) {
         GLuint replaceunit = i;
	 /* try to optimize GL_REPLACE away (only one level deep though) */
	 if (	(ctx->Texture.Unit[i]._CurrentCombine->ModeRGB == GL_REPLACE) &&
		(ctx->Texture.Unit[i]._CurrentCombine->ModeA == GL_REPLACE) &&
		(ctx->Texture.Unit[i]._CurrentCombine->ScaleShiftRGB == 0) &&
		(ctx->Texture.Unit[i]._CurrentCombine->ScaleShiftA == 0) &&
		(nextunit[i] > 0) ) {
	    /* yippie! can optimize it away! */
	    replaceunit = i;
	    i = nextunit[i];
	 }

	 /* need env instruction slot */
	 rmesa->state.envneeded |= 1 << currslot;
	 ok = r200UpdateTextureEnv( ctx, i, currslot, replaceunit );
	 if (!ok) return GL_FALSE;
	 currslot++;
      }
      i = i + 1;
   }

   if (currslot == 0) {
      /* need one stage at least */
      rmesa->state.texture.unit[0].outputreg = 0;
      ok = r200UpdateTextureEnv( ctx, 0, 0, 0 );
   }

   R200_STATECHANGE( rmesa, ctx );
   rmesa->hw.ctx.cmd[CTX_PP_CNTL] &= ~(R200_TEX_BLEND_ENABLE_MASK | R200_MULTI_PASS_ENABLE);
   rmesa->hw.ctx.cmd[CTX_PP_CNTL] |= rmesa->state.envneeded << R200_TEX_BLEND_0_ENABLE_SHIFT;

   return ok;
}

#undef REF_COLOR
#undef REF_ALPHA


#define TEXOBJ_TXFILTER_MASK (R200_MAX_MIP_LEVEL_MASK |		\
			      R200_MIN_FILTER_MASK | 		\
			      R200_MAG_FILTER_MASK |		\
			      R200_MAX_ANISO_MASK |		\
			      R200_YUV_TO_RGB |			\
			      R200_YUV_TEMPERATURE_MASK |	\
			      R200_CLAMP_S_MASK | 		\
			      R200_CLAMP_T_MASK | 		\
			      R200_BORDER_MODE_D3D )

#define TEXOBJ_TXFORMAT_MASK (R200_TXFORMAT_WIDTH_MASK |	\
			      R200_TXFORMAT_HEIGHT_MASK |	\
			      R200_TXFORMAT_FORMAT_MASK |	\
			      R200_TXFORMAT_F5_WIDTH_MASK |	\
			      R200_TXFORMAT_F5_HEIGHT_MASK |	\
			      R200_TXFORMAT_ALPHA_IN_MAP |	\
			      R200_TXFORMAT_CUBIC_MAP_ENABLE |	\
			      R200_TXFORMAT_NON_POWER2)

#define TEXOBJ_TXFORMAT_X_MASK (R200_DEPTH_LOG2_MASK |		\
                                R200_TEXCOORD_MASK |		\
                                R200_MIN_MIP_LEVEL_MASK |	\
                                R200_CLAMP_Q_MASK | 		\
                                R200_VOLUME_FILTER_MASK)


static void disable_tex_obj_state( r200ContextPtr rmesa, 
				   int unit )
{
   
   R200_STATECHANGE( rmesa, vtx );
   rmesa->hw.vtx.cmd[VTX_TCL_OUTPUT_VTXFMT_1] &= ~(7 << (unit * 3));

   R200_STATECHANGE( rmesa, ctx );
   rmesa->hw.ctx.cmd[CTX_PP_CNTL] &= ~(R200_TEX_0_ENABLE << unit);
   if (rmesa->radeon.TclFallback & (R200_TCL_FALLBACK_TEXGEN_0<<unit)) {
      TCL_FALLBACK( &rmesa->radeon.glCtx, (R200_TCL_FALLBACK_TEXGEN_0<<unit), GL_FALSE);
   }

   /* Actually want to keep all units less than max active texture
    * enabled, right?  Fix this for >2 texunits.
    */

   {
      GLuint tmp = rmesa->TexGenEnabled;

      rmesa->TexGenEnabled &= ~(R200_TEXGEN_TEXMAT_0_ENABLE<<unit);
      rmesa->TexGenEnabled &= ~(R200_TEXMAT_0_ENABLE<<unit);
      rmesa->TexGenNeedNormals[unit] = GL_FALSE;
      rmesa->TexGenCompSel &= ~(R200_OUTPUT_TEX_0 << unit);

      if (tmp != rmesa->TexGenEnabled) {
	 rmesa->recheck_texgen[unit] = GL_TRUE;
	 rmesa->radeon.NewGLState |= _NEW_TEXTURE_MATRIX;
      }
   }
}
static void import_tex_obj_state( r200ContextPtr rmesa,
				  int unit,
				  radeonTexObjPtr texobj )
{
/* do not use RADEON_DB_STATE to avoid stale texture caches */
   GLuint *cmd = &rmesa->hw.tex[unit].cmd[TEX_CMD_0];

   R200_STATECHANGE( rmesa, tex[unit] );

   cmd[TEX_PP_TXFILTER] &= ~TEXOBJ_TXFILTER_MASK;
   cmd[TEX_PP_TXFILTER] |= texobj->pp_txfilter & TEXOBJ_TXFILTER_MASK;
   cmd[TEX_PP_TXFORMAT] &= ~TEXOBJ_TXFORMAT_MASK;
   cmd[TEX_PP_TXFORMAT] |= texobj->pp_txformat & TEXOBJ_TXFORMAT_MASK;
   cmd[TEX_PP_TXFORMAT_X] &= ~TEXOBJ_TXFORMAT_X_MASK;
   cmd[TEX_PP_TXFORMAT_X] |= texobj->pp_txformat_x & TEXOBJ_TXFORMAT_X_MASK;
   cmd[TEX_PP_TXSIZE] = texobj->pp_txsize; /* NPOT only! */
   cmd[TEX_PP_TXPITCH] = texobj->pp_txpitch; /* NPOT only! */
   cmd[TEX_PP_BORDER_COLOR] = texobj->pp_border_color;

   if (texobj->base.Target == GL_TEXTURE_CUBE_MAP) {
      GLuint *cube_cmd = &rmesa->hw.cube[unit].cmd[CUBE_CMD_0];

      R200_STATECHANGE( rmesa, cube[unit] );
      cube_cmd[CUBE_PP_CUBIC_FACES] = texobj->pp_cubic_faces;
      /* that value is submitted twice. could change cube atom
         to not include that command when new drm is used */
      cmd[TEX_PP_CUBIC_FACES] = texobj->pp_cubic_faces;
   }

}

static void set_texgen_matrix( r200ContextPtr rmesa, 
			       GLuint unit,
			       const GLfloat *s_plane,
			       const GLfloat *t_plane,
			       const GLfloat *r_plane,
			       const GLfloat *q_plane )
{
   GLfloat m[16];

   m[0]  = s_plane[0];
   m[4]  = s_plane[1];
   m[8]  = s_plane[2];
   m[12] = s_plane[3];

   m[1]  = t_plane[0];
   m[5]  = t_plane[1];
   m[9]  = t_plane[2];
   m[13] = t_plane[3];

   m[2]  = r_plane[0];
   m[6]  = r_plane[1];
   m[10] = r_plane[2];
   m[14] = r_plane[3];

   m[3]  = q_plane[0];
   m[7]  = q_plane[1];
   m[11] = q_plane[2];
   m[15] = q_plane[3];

   _math_matrix_loadf( &(rmesa->TexGenMatrix[unit]), m);
   _math_matrix_analyse( &(rmesa->TexGenMatrix[unit]) );
   rmesa->TexGenEnabled |= R200_TEXMAT_0_ENABLE<<unit;
}


static GLuint r200_need_dis_texgen(const GLbitfield texGenEnabled,
				   const GLfloat *planeS,
				   const GLfloat *planeT,
				   const GLfloat *planeR,
				   const GLfloat *planeQ)
{
   GLuint needtgenable = 0;

   if (!(texGenEnabled & S_BIT)) {
      if (((texGenEnabled & T_BIT) && planeT[0] != 0.0) ||
	 ((texGenEnabled & R_BIT) && planeR[0] != 0.0) ||
	 ((texGenEnabled & Q_BIT) && planeQ[0] != 0.0)) {
	 needtgenable |= S_BIT;
      }
   }
   if (!(texGenEnabled & T_BIT)) {
      if (((texGenEnabled & S_BIT) && planeS[1] != 0.0) ||
	 ((texGenEnabled & R_BIT) && planeR[1] != 0.0) ||
	 ((texGenEnabled & Q_BIT) && planeQ[1] != 0.0)) {
	 needtgenable |= T_BIT;
     }
   }
   if (!(texGenEnabled & R_BIT)) {
      if (((texGenEnabled & S_BIT) && planeS[2] != 0.0) ||
	 ((texGenEnabled & T_BIT) && planeT[2] != 0.0) ||
	 ((texGenEnabled & Q_BIT) && planeQ[2] != 0.0)) {
	 needtgenable |= R_BIT;
      }
   }
   if (!(texGenEnabled & Q_BIT)) {
      if (((texGenEnabled & S_BIT) && planeS[3] != 0.0) ||
	 ((texGenEnabled & T_BIT) && planeT[3] != 0.0) ||
	 ((texGenEnabled & R_BIT) && planeR[3] != 0.0)) {
	 needtgenable |= Q_BIT;
      }
   }

   return needtgenable;
}


/*
 * Returns GL_FALSE if fallback required.  
 */
static GLboolean r200_validate_texgen( struct gl_context *ctx, GLuint unit )
{  
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   const struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
   GLuint inputshift = R200_TEXGEN_0_INPUT_SHIFT + unit*4;
   GLuint tgi, tgcm;
   GLuint mode = 0;
   GLboolean mixed_fallback = GL_FALSE;
   static const GLfloat I[16] = {
      1,  0,  0,  0,
      0,  1,  0,  0,
      0,  0,  1,  0,
      0,  0,  0,  1 };
   static const GLfloat reflect[16] = {
      -1,  0,  0,  0,
       0, -1,  0,  0,
       0,  0,  -1, 0,
       0,  0,  0,  1 };

   rmesa->TexGenCompSel &= ~(R200_OUTPUT_TEX_0 << unit);
   rmesa->TexGenEnabled &= ~(R200_TEXGEN_TEXMAT_0_ENABLE<<unit);
   rmesa->TexGenEnabled &= ~(R200_TEXMAT_0_ENABLE<<unit);
   rmesa->TexGenNeedNormals[unit] = GL_FALSE;
   tgi = rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_1] & ~(R200_TEXGEN_INPUT_MASK <<
						   inputshift);
   tgcm = rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_2] & ~(R200_TEXGEN_COMP_MASK <<
						    (unit * 4));

   if (0) 
      fprintf(stderr, "%s unit %d\n", __func__, unit);

   if (texUnit->TexGenEnabled & S_BIT) {
      mode = texUnit->GenS.Mode;
   } else {
      tgcm |= R200_TEXGEN_COMP_S << (unit * 4);
   }

   if (texUnit->TexGenEnabled & T_BIT) {
      if (texUnit->GenT.Mode != mode)
	 mixed_fallback = GL_TRUE;
   } else {
      tgcm |= R200_TEXGEN_COMP_T << (unit * 4);
   }
   if (texUnit->TexGenEnabled & R_BIT) {
      if (texUnit->GenR.Mode != mode)
	 mixed_fallback = GL_TRUE;
   } else {
      tgcm |= R200_TEXGEN_COMP_R << (unit * 4);
   }

   if (texUnit->TexGenEnabled & Q_BIT) {
      if (texUnit->GenQ.Mode != mode)
	 mixed_fallback = GL_TRUE;
   } else {
      tgcm |= R200_TEXGEN_COMP_Q << (unit * 4);
   }

   if (mixed_fallback) {
      if (R200_DEBUG & RADEON_FALLBACKS)
	 fprintf(stderr, "fallback mixed texgen, 0x%x (0x%x 0x%x 0x%x 0x%x)\n",
		 texUnit->TexGenEnabled, texUnit->GenS.Mode, texUnit->GenT.Mode,
		 texUnit->GenR.Mode, texUnit->GenQ.Mode);
      return GL_FALSE;
   }

/* we CANNOT do mixed mode if the texgen mode requires a plane where the input
   is not enabled for texgen, since the planes are concatenated into texmat,
   and thus the input will come from texcoord rather than tex gen equation!
   Either fallback or just hope that those texcoords aren't really needed...
   Assuming the former will cause lots of unnecessary fallbacks, the latter will
   generate bogus results sometimes - it's pretty much impossible to really know
   when a fallback is needed, depends on texmat and what sort of texture is bound
   etc, - for now fallback if we're missing either S or T bits, there's a high
   probability we need the texcoords in that case.
   That's a lot of work for some obscure texgen mixed mode fixup - why oh why
   doesn't the chip just directly accept the plane parameters :-(. */
   switch (mode) {
   case GL_OBJECT_LINEAR: {
      GLuint needtgenable = r200_need_dis_texgen( texUnit->TexGenEnabled,
                                                  texUnit->GenS.ObjectPlane,
                                                  texUnit->GenT.ObjectPlane,
                                                  texUnit->GenR.ObjectPlane,
                                                  texUnit->GenQ.ObjectPlane );
      if (needtgenable & (S_BIT | T_BIT)) {
	 if (R200_DEBUG & RADEON_FALLBACKS)
	 fprintf(stderr, "fallback mixed texgen / obj plane, 0x%x\n",
		 texUnit->TexGenEnabled);
	 return GL_FALSE;
      }
      if (needtgenable & (R_BIT)) {
	 tgcm &= ~(R200_TEXGEN_COMP_R << (unit * 4));
      }
      if (needtgenable & (Q_BIT)) {
	 tgcm &= ~(R200_TEXGEN_COMP_Q << (unit * 4));
      }

      tgi |= R200_TEXGEN_INPUT_OBJ << inputshift;
      set_texgen_matrix( rmesa, unit, 
	 (texUnit->TexGenEnabled & S_BIT) ? texUnit->GenS.ObjectPlane : I,
	 (texUnit->TexGenEnabled & T_BIT) ? texUnit->GenT.ObjectPlane : I + 4,
	 (texUnit->TexGenEnabled & R_BIT) ? texUnit->GenR.ObjectPlane : I + 8,
	 (texUnit->TexGenEnabled & Q_BIT) ? texUnit->GenQ.ObjectPlane : I + 12);
      }
      break;

   case GL_EYE_LINEAR: {
      GLuint needtgenable = r200_need_dis_texgen( texUnit->TexGenEnabled,
                                                  texUnit->GenS.EyePlane,
                                                  texUnit->GenT.EyePlane,
                                                  texUnit->GenR.EyePlane,
                                                  texUnit->GenQ.EyePlane );
      if (needtgenable & (S_BIT | T_BIT)) {
	 if (R200_DEBUG & RADEON_FALLBACKS)
	 fprintf(stderr, "fallback mixed texgen / eye plane, 0x%x\n",
		 texUnit->TexGenEnabled);
	 return GL_FALSE;
      }
      if (needtgenable & (R_BIT)) {
	 tgcm &= ~(R200_TEXGEN_COMP_R << (unit * 4));
      }
      if (needtgenable & (Q_BIT)) {
	 tgcm &= ~(R200_TEXGEN_COMP_Q << (unit * 4));
      }
      tgi |= R200_TEXGEN_INPUT_EYE << inputshift;
      set_texgen_matrix( rmesa, unit,
	 (texUnit->TexGenEnabled & S_BIT) ? texUnit->GenS.EyePlane : I,
	 (texUnit->TexGenEnabled & T_BIT) ? texUnit->GenT.EyePlane : I + 4,
	 (texUnit->TexGenEnabled & R_BIT) ? texUnit->GenR.EyePlane : I + 8,
	 (texUnit->TexGenEnabled & Q_BIT) ? texUnit->GenQ.EyePlane : I + 12);
      }
      break;

   case GL_REFLECTION_MAP_NV:
      rmesa->TexGenNeedNormals[unit] = GL_TRUE;
      tgi |= R200_TEXGEN_INPUT_EYE_REFLECT << inputshift;
      /* pretty weird, must only negate when lighting is enabled? */
      if (ctx->Light.Enabled)
	 set_texgen_matrix( rmesa, unit, 
	    (texUnit->TexGenEnabled & S_BIT) ? reflect : I,
	    (texUnit->TexGenEnabled & T_BIT) ? reflect + 4 : I + 4,
	    (texUnit->TexGenEnabled & R_BIT) ? reflect + 8 : I + 8,
	    I + 12);
      break;

   case GL_NORMAL_MAP_NV:
      rmesa->TexGenNeedNormals[unit] = GL_TRUE;
      tgi |= R200_TEXGEN_INPUT_EYE_NORMAL<<inputshift;
      break;

   case GL_SPHERE_MAP:
      rmesa->TexGenNeedNormals[unit] = GL_TRUE;
      tgi |= R200_TEXGEN_INPUT_SPHERE<<inputshift;
      break;

   case 0:
      /* All texgen units were disabled, so just pass coords through. */
      tgi |= unit << inputshift;
      break;

   default:
      /* Unsupported mode, fallback:
       */
      if (R200_DEBUG & RADEON_FALLBACKS)
	 fprintf(stderr, "fallback unsupported texgen, %d\n",
		 texUnit->GenS.Mode);
      return GL_FALSE;
   }

   rmesa->TexGenEnabled |= R200_TEXGEN_TEXMAT_0_ENABLE << unit;
   rmesa->TexGenCompSel |= R200_OUTPUT_TEX_0 << unit;

   if (tgi != rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_1] || 
       tgcm != rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_2])
   {
      R200_STATECHANGE(rmesa, tcg);
      rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_1] = tgi;
      rmesa->hw.tcg.cmd[TCG_TEX_PROC_CTL_2] = tgcm;
   }

   return GL_TRUE;
}

void set_re_cntl_d3d( struct gl_context *ctx, int unit, GLboolean use_d3d )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   GLuint re_cntl;

   re_cntl = rmesa->hw.set.cmd[SET_RE_CNTL] & ~(R200_VTX_STQ0_D3D << (2 * unit));
   if (use_d3d)
      re_cntl |= R200_VTX_STQ0_D3D << (2 * unit);

   if ( re_cntl != rmesa->hw.set.cmd[SET_RE_CNTL] ) {
      R200_STATECHANGE( rmesa, set );
      rmesa->hw.set.cmd[SET_RE_CNTL] = re_cntl;
   }
}

/**
 * Compute the cached hardware register values for the given texture object.
 *
 * \param rmesa Context pointer
 * \param t the r300 texture object
 */
static void setup_hardware_state(r200ContextPtr rmesa, radeonTexObj *t)
{
   const struct gl_texture_image *firstImage = t->base.Image[0][t->minLod];
   GLint log2Width, log2Height, log2Depth, texelBytes;
   uint extra_size = 0;

   if ( t->bo ) {
       return;
   }

   log2Width  = firstImage->WidthLog2;
   log2Height = firstImage->HeightLog2;
   log2Depth  = firstImage->DepthLog2;
   texelBytes = _mesa_get_format_bytes(firstImage->TexFormat);

   radeon_print(RADEON_TEXTURE, RADEON_TRACE,
	"%s(%p, tex %p) log2(w %d, h %d, d %d), texelBytes %d. format %d\n",
	__func__, rmesa, t, log2Width, log2Height,
	log2Depth, texelBytes, firstImage->TexFormat);

   if (!t->image_override) {
      if (VALID_FORMAT(firstImage->TexFormat)) {
	 const struct tx_table *table = _mesa_little_endian() ? tx_table_le :
	    tx_table_be;
	 
	 t->pp_txformat &= ~(R200_TXFORMAT_FORMAT_MASK |
			     R200_TXFORMAT_ALPHA_IN_MAP);
	 t->pp_txfilter &= ~R200_YUV_TO_RGB;
	 
	 t->pp_txformat |= table[ firstImage->TexFormat ].format;
	 t->pp_txfilter |= table[ firstImage->TexFormat ].filter;


      } else {
	 _mesa_problem(NULL, "unexpected texture format in %s",
		       __func__);
	 return;
      }
   }

   t->pp_txfilter &= ~R200_MAX_MIP_LEVEL_MASK;
   t->pp_txfilter |= ((t->maxLod) << R200_MAX_MIP_LEVEL_SHIFT)
	   & R200_MAX_MIP_LEVEL_MASK;

   if ( t->pp_txfilter &
		(R200_MIN_FILTER_NEAREST_MIP_NEAREST
		 | R200_MIN_FILTER_NEAREST_MIP_LINEAR
		 | R200_MIN_FILTER_LINEAR_MIP_NEAREST
		 | R200_MIN_FILTER_LINEAR_MIP_LINEAR
		 | R200_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST
		 | R200_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR))
		 extra_size = t->minLod;

   t->pp_txformat &= ~(R200_TXFORMAT_WIDTH_MASK |
		       R200_TXFORMAT_HEIGHT_MASK |
		       R200_TXFORMAT_CUBIC_MAP_ENABLE |
		       R200_TXFORMAT_F5_WIDTH_MASK |
		       R200_TXFORMAT_F5_HEIGHT_MASK);
   t->pp_txformat |= (((log2Width + extra_size) << R200_TXFORMAT_WIDTH_SHIFT) |
		      ((log2Height + extra_size)<< R200_TXFORMAT_HEIGHT_SHIFT));
   
   t->tile_bits = 0;
   
   t->pp_txformat_x &= ~(R200_DEPTH_LOG2_MASK | R200_TEXCOORD_MASK
		   | R200_MIN_MIP_LEVEL_MASK);

   t->pp_txformat_x |= (t->minLod << R200_MIN_MIP_LEVEL_SHIFT)
	   & R200_MIN_MIP_LEVEL_MASK;

   if (t->base.Target == GL_TEXTURE_3D) {
      t->pp_txformat_x |= (log2Depth << R200_DEPTH_LOG2_SHIFT);
      t->pp_txformat_x |= R200_TEXCOORD_VOLUME;

   }
   else if (t->base.Target == GL_TEXTURE_CUBE_MAP) {
      assert(log2Width == log2Height);
      t->pp_txformat |= ((log2Width << R200_TXFORMAT_F5_WIDTH_SHIFT) |
			 (log2Height << R200_TXFORMAT_F5_HEIGHT_SHIFT) |
			 /* don't think we need this bit, if it exists at all - fglrx does not set it */
			 (R200_TXFORMAT_CUBIC_MAP_ENABLE));
      t->pp_txformat_x |= R200_TEXCOORD_CUBIC_ENV;
      t->pp_cubic_faces = ((log2Width << R200_FACE_WIDTH_1_SHIFT) |
                           (log2Height << R200_FACE_HEIGHT_1_SHIFT) |
                           (log2Width << R200_FACE_WIDTH_2_SHIFT) |
                           (log2Height << R200_FACE_HEIGHT_2_SHIFT) |
                           (log2Width << R200_FACE_WIDTH_3_SHIFT) |
                           (log2Height << R200_FACE_HEIGHT_3_SHIFT) |
                           (log2Width << R200_FACE_WIDTH_4_SHIFT) |
                           (log2Height << R200_FACE_HEIGHT_4_SHIFT));
   }
   else {
      /* If we don't in fact send enough texture coordinates, q will be 1,
       * making TEXCOORD_PROJ act like TEXCOORD_NONPROJ (Right?)
       */
      t->pp_txformat_x |= R200_TEXCOORD_PROJ;
   }
   /* FIXME: NPOT sizes, is it correct really? */
   t->pp_txsize = (((firstImage->Width - 1) << R200_PP_TX_WIDTHMASK_SHIFT)
		   | ((firstImage->Height - 1) << R200_PP_TX_HEIGHTMASK_SHIFT));

   if ( !t->image_override ) {
      if (_mesa_is_format_compressed(firstImage->TexFormat))
         t->pp_txpitch = (firstImage->Width + 63) & ~(63);
      else
         t->pp_txpitch = ((firstImage->Width * texelBytes) + 63) & ~(63);
      t->pp_txpitch -= 32;
   }

   if (t->base.Target == GL_TEXTURE_RECTANGLE_NV) {
      t->pp_txformat |= R200_TXFORMAT_NON_POWER2;
   }

}

static GLboolean r200_validate_texture(struct gl_context *ctx, struct gl_texture_object *texObj, int unit)
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   radeonTexObj *t = radeon_tex_obj(texObj);

   if (!radeon_validate_texture_miptree(ctx, _mesa_get_samplerobj(ctx, unit), texObj))
      return GL_FALSE;

   r200_validate_texgen(ctx, unit);
   /* Configure the hardware registers (more precisely, the cached version
    * of the hardware registers). */
   setup_hardware_state(rmesa, t);

   if (texObj->Target == GL_TEXTURE_RECTANGLE_NV ||
       texObj->Target == GL_TEXTURE_2D ||
       texObj->Target == GL_TEXTURE_1D)
      set_re_cntl_d3d( ctx, unit, GL_FALSE );
   else
      set_re_cntl_d3d( ctx, unit, GL_TRUE );
   R200_STATECHANGE( rmesa, ctx );
   rmesa->hw.ctx.cmd[CTX_PP_CNTL] |= R200_TEX_0_ENABLE << unit;
   
   R200_STATECHANGE( rmesa, vtx );
   rmesa->hw.vtx.cmd[VTX_TCL_OUTPUT_VTXFMT_1] &= ~(7 << (unit * 3));
   rmesa->hw.vtx.cmd[VTX_TCL_OUTPUT_VTXFMT_1] |= 4 << (unit * 3);

   rmesa->recheck_texgen[unit] = GL_TRUE;
   r200TexUpdateParameters(ctx, unit);
   import_tex_obj_state( rmesa, unit, t );

   if (rmesa->recheck_texgen[unit]) {
      GLboolean fallback = !r200_validate_texgen( ctx, unit );
      TCL_FALLBACK( ctx, (R200_TCL_FALLBACK_TEXGEN_0<<unit), fallback);
      rmesa->recheck_texgen[unit] = 0;
      rmesa->radeon.NewGLState |= _NEW_TEXTURE_MATRIX;
   }

   t->validated = GL_TRUE;

   FALLBACK( rmesa, RADEON_FALLBACK_BORDER_MODE, t->border_fallback );

   return !t->border_fallback;
}

static GLboolean r200UpdateTextureUnit(struct gl_context *ctx, int unit)
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint unitneeded = rmesa->state.texture.unit[unit].unitneeded;

   if (!unitneeded) {
      /* disable the unit */
     disable_tex_obj_state(rmesa, unit);
     return GL_TRUE;
   }

   if (!r200_validate_texture(ctx, ctx->Texture.Unit[unit]._Current, unit)) {
    _mesa_warning(ctx,
		  "failed to validate texture for unit %d.\n",
		  unit);
    rmesa->state.texture.unit[unit].texobj = NULL;
    return GL_FALSE;
  }

   rmesa->state.texture.unit[unit].texobj = radeon_tex_obj(ctx->Texture.Unit[unit]._Current);
  return GL_TRUE;
}


void r200UpdateTextureState( struct gl_context *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLboolean ok;
   GLuint dbg;

   /* NOTE: must not manipulate rmesa->state.texture.unit[].unitneeded or
      rmesa->state.envneeded before a R200_STATECHANGE (or R200_NEWPRIM) since
      we use these to determine if we want to emit the corresponding state
      atoms. */
   R200_NEWPRIM( rmesa );

   if (_mesa_ati_fragment_shader_enabled(ctx)) {
      GLuint i;
      for (i = 0; i < R200_MAX_TEXTURE_UNITS; i++) {
         if (ctx->Texture.Unit[i]._Current)
            rmesa->state.texture.unit[i].unitneeded = 1 << _mesa_tex_target_to_index(ctx, ctx->Texture.Unit[i]._Current->Target);
         else
            rmesa->state.texture.unit[i].unitneeded = 0;
      }
      ok = GL_TRUE;
   }
   else {
      ok = r200UpdateAllTexEnv( ctx );
   }
   if (ok) {
      ok = (r200UpdateTextureUnit( ctx, 0 ) &&
	 r200UpdateTextureUnit( ctx, 1 ) &&
	 r200UpdateTextureUnit( ctx, 2 ) &&
	 r200UpdateTextureUnit( ctx, 3 ) &&
	 r200UpdateTextureUnit( ctx, 4 ) &&
	 r200UpdateTextureUnit( ctx, 5 ));
   }

   if (ok && _mesa_ati_fragment_shader_enabled(ctx)) {
      r200UpdateFragmentShader(ctx);
   }

   FALLBACK( rmesa, R200_FALLBACK_TEXTURE, !ok );

   if (rmesa->radeon.TclFallback)
      r200ChooseVertexState( ctx );


   if (rmesa->radeon.radeonScreen->chip_family == CHIP_FAMILY_R200) {

      /*
       * T0 hang workaround -------------
       * not needed for r200 derivatives
        */
      if ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_ENABLE_MASK) == R200_TEX_0_ENABLE &&
	 (rmesa->hw.tex[0].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK) > R200_MIN_FILTER_LINEAR) {

	 R200_STATECHANGE(rmesa, ctx);
	 R200_STATECHANGE(rmesa, tex[1]);
	 rmesa->hw.ctx.cmd[CTX_PP_CNTL] |= R200_TEX_1_ENABLE;
	 if (!(rmesa->hw.cst.cmd[CST_PP_CNTL_X] & R200_PPX_TEX_1_ENABLE))
	   rmesa->hw.tex[1].cmd[TEX_PP_TXFORMAT] &= ~TEXOBJ_TXFORMAT_MASK;
	 rmesa->hw.tex[1].cmd[TEX_PP_TXFORMAT] |= R200_TXFORMAT_LOOKUP_DISABLE;
      }
      else if (!_mesa_ati_fragment_shader_enabled(ctx)) {
	 if ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_1_ENABLE) &&
	    (rmesa->hw.tex[1].cmd[TEX_PP_TXFORMAT] & R200_TXFORMAT_LOOKUP_DISABLE)) {
	    R200_STATECHANGE(rmesa, tex[1]);
	    rmesa->hw.tex[1].cmd[TEX_PP_TXFORMAT] &= ~R200_TXFORMAT_LOOKUP_DISABLE;
         }
      }
      /* do the same workaround for the first pass of a fragment shader.
       * completely unknown if necessary / sufficient.
       */
      if ((rmesa->hw.cst.cmd[CST_PP_CNTL_X] & R200_PPX_TEX_ENABLE_MASK) == R200_PPX_TEX_0_ENABLE &&
	 (rmesa->hw.tex[0].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK) > R200_MIN_FILTER_LINEAR) {

	 R200_STATECHANGE(rmesa, cst);
	 R200_STATECHANGE(rmesa, tex[1]);
	 rmesa->hw.cst.cmd[CST_PP_CNTL_X] |= R200_PPX_TEX_1_ENABLE;
	 if (!(rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_1_ENABLE))
	    rmesa->hw.tex[1].cmd[TEX_PP_TXFORMAT] &= ~TEXOBJ_TXFORMAT_MASK;
	 rmesa->hw.tex[1].cmd[TEX_PP_TXMULTI_CTL] |= R200_PASS1_TXFORMAT_LOOKUP_DISABLE;
      }

      /* maybe needs to be done pairwise due to 2 parallel (physical) tex units ?
         looks like that's not the case, if 8500/9100 owners don't complain remove this...
      for ( i = 0; i < ctx->Const.MaxTextureUnits; i += 2) {
         if (((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & ((R200_TEX_0_ENABLE |
            R200_TEX_1_ENABLE ) << i)) == (R200_TEX_0_ENABLE << i)) &&
            ((rmesa->hw.tex[i].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK) >
            R200_MIN_FILTER_LINEAR)) {
            R200_STATECHANGE(rmesa, ctx);
            R200_STATECHANGE(rmesa, tex[i+1]);
            rmesa->hw.ctx.cmd[CTX_PP_CNTL] |= (R200_TEX_1_ENABLE << i);
            rmesa->hw.tex[i+1].cmd[TEX_PP_TXFORMAT] &= ~TEXOBJ_TXFORMAT_MASK;
            rmesa->hw.tex[i+1].cmd[TEX_PP_TXFORMAT] |= 0x08000000;
         }
         else {
            if ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & (R200_TEX_1_ENABLE << i)) &&
               (rmesa->hw.tex[i+1].cmd[TEX_PP_TXFORMAT] & 0x08000000)) {
               R200_STATECHANGE(rmesa, tex[i+1]);
               rmesa->hw.tex[i+1].cmd[TEX_PP_TXFORMAT] &= ~0x08000000;
            }
         }
      } */

      /*
       * Texture cache LRU hang workaround -------------
       * not needed for r200 derivatives
       * hopefully this covers first pass of a shader as well
       */

      /* While the cases below attempt to only enable the workaround in the
       * specific cases necessary, they were insufficient.  See bugzilla #1519,
       * #729, #814.  Tests with quake3 showed no impact on performance.
       */
      dbg = 0x6;

      /*
      if (((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & (R200_TEX_0_ENABLE )) &&
         ((((rmesa->hw.tex[0].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)) ||
         ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_2_ENABLE) &&
         ((((rmesa->hw.tex[2].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)) ||
         ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_4_ENABLE) &&
         ((((rmesa->hw.tex[4].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)))
      {
         dbg |= 0x02;
      }

      if (((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & (R200_TEX_1_ENABLE )) &&
         ((((rmesa->hw.tex[1].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)) ||
         ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_3_ENABLE) &&
         ((((rmesa->hw.tex[3].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)) ||
         ((rmesa->hw.ctx.cmd[CTX_PP_CNTL] & R200_TEX_5_ENABLE) &&
         ((((rmesa->hw.tex[5].cmd[TEX_PP_TXFILTER] & R200_MIN_FILTER_MASK)) &
         0x04) == 0)))
      {
         dbg |= 0x04;
      }*/

      if (dbg != rmesa->hw.tam.cmd[TAM_DEBUG3]) {
         R200_STATECHANGE( rmesa, tam );
         rmesa->hw.tam.cmd[TAM_DEBUG3] = dbg;
         if (0) printf("TEXCACHE LRU HANG WORKAROUND %x\n", dbg);
      }
   }
}
