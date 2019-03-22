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

#include "util/u_debug.h"
#include "pipe/p_shader_tokens.h"
#include "tgsi_info.h"
#include "tgsi_parse.h"
#include "tgsi_util.h"
#include "tgsi_exec.h"
#include "util/bitscan.h"

union pointer_hack
{
   void *pointer;
   uint64_t uint64;
};

void *
tgsi_align_128bit(
   void *unaligned )
{
   union pointer_hack ph;

   ph.uint64 = 0;
   ph.pointer = unaligned;
   ph.uint64 = (ph.uint64 + 15) & ~15;
   return ph.pointer;
}

unsigned
tgsi_util_get_src_register_swizzle(
   const struct tgsi_src_register *reg,
   unsigned component )
{
   switch (component) {
   case TGSI_CHAN_X:
      return reg->SwizzleX;
   case TGSI_CHAN_Y:
      return reg->SwizzleY;
   case TGSI_CHAN_Z:
      return reg->SwizzleZ;
   case TGSI_CHAN_W:
      return reg->SwizzleW;
   default:
      assert(0);
   }
   return 0;
}


unsigned
tgsi_util_get_full_src_register_swizzle(
   const struct tgsi_full_src_register  *reg,
   unsigned component )
{
   return tgsi_util_get_src_register_swizzle(
      &reg->Register,
      component );
}

void
tgsi_util_set_src_register_swizzle(
   struct tgsi_src_register *reg,
   unsigned swizzle,
   unsigned component )
{
   switch( component ) {
   case 0:
      reg->SwizzleX = swizzle;
      break;
   case 1:
      reg->SwizzleY = swizzle;
      break;
   case 2:
      reg->SwizzleZ = swizzle;
      break;
   case 3:
      reg->SwizzleW = swizzle;
      break;
   default:
      assert( 0 );
   }
}

unsigned
tgsi_util_get_full_src_register_sign_mode(
   const struct  tgsi_full_src_register *reg,
   unsigned component )
{
   unsigned sign_mode;

   if( reg->Register.Absolute ) {
      /* Consider only the post-abs negation. */

      if( reg->Register.Negate ) {
         sign_mode = TGSI_UTIL_SIGN_SET;
      }
      else {
         sign_mode = TGSI_UTIL_SIGN_CLEAR;
      }
   }
   else {
      if( reg->Register.Negate ) {
         sign_mode = TGSI_UTIL_SIGN_TOGGLE;
      }
      else {
         sign_mode = TGSI_UTIL_SIGN_KEEP;
      }
   }

   return sign_mode;
}

void
tgsi_util_set_full_src_register_sign_mode(
   struct tgsi_full_src_register *reg,
   unsigned sign_mode )
{
   switch (sign_mode)
   {
   case TGSI_UTIL_SIGN_CLEAR:
      reg->Register.Negate = 0;
      reg->Register.Absolute = 1;
      break;

   case TGSI_UTIL_SIGN_SET:
      reg->Register.Absolute = 1;
      reg->Register.Negate = 1;
      break;

   case TGSI_UTIL_SIGN_TOGGLE:
      reg->Register.Negate = 1;
      reg->Register.Absolute = 0;
      break;

   case TGSI_UTIL_SIGN_KEEP:
      reg->Register.Negate = 0;
      reg->Register.Absolute = 0;
      break;

   default:
      assert( 0 );
   }
}

/**
 * Determine which channels of the specificed src register are effectively
 * used by this instruction.
 */
unsigned
tgsi_util_get_inst_usage_mask(const struct tgsi_full_instruction *inst,
                              unsigned src_idx)
{
   const struct tgsi_full_src_register *src = &inst->Src[src_idx];
   unsigned write_mask = inst->Dst[0].Register.WriteMask;
   unsigned read_mask;
   unsigned usage_mask;
   unsigned chan;

   switch (inst->Instruction.Opcode) {
   case TGSI_OPCODE_IF:
   case TGSI_OPCODE_UIF:
   case TGSI_OPCODE_EMIT:
   case TGSI_OPCODE_ENDPRIM:
   case TGSI_OPCODE_RCP:
   case TGSI_OPCODE_RSQ:
   case TGSI_OPCODE_SQRT:
   case TGSI_OPCODE_EX2:
   case TGSI_OPCODE_LG2:
   case TGSI_OPCODE_SIN:
   case TGSI_OPCODE_COS:
   case TGSI_OPCODE_POW: /* reads src0.x and src1.x */
   case TGSI_OPCODE_UP2H:
   case TGSI_OPCODE_UP2US:
   case TGSI_OPCODE_UP4B:
   case TGSI_OPCODE_UP4UB:
   case TGSI_OPCODE_MEMBAR:
   case TGSI_OPCODE_BALLOT:
      read_mask = TGSI_WRITEMASK_X;
      break;

   case TGSI_OPCODE_DP2:
   case TGSI_OPCODE_PK2H:
   case TGSI_OPCODE_PK2US:
   case TGSI_OPCODE_DFRACEXP:
   case TGSI_OPCODE_F2D:
   case TGSI_OPCODE_I2D:
   case TGSI_OPCODE_U2D:
   case TGSI_OPCODE_F2U64:
   case TGSI_OPCODE_F2I64:
   case TGSI_OPCODE_U2I64:
   case TGSI_OPCODE_I2I64:
   case TGSI_OPCODE_TXQS: /* bindless handle possible */
   case TGSI_OPCODE_RESQ: /* bindless handle possible */
      read_mask = TGSI_WRITEMASK_XY;
      break;

   case TGSI_OPCODE_TXQ:
      if (src_idx == 0)
         read_mask = TGSI_WRITEMASK_X;
      else
         read_mask = TGSI_WRITEMASK_XY;  /* bindless handle possible */
      break;

   case TGSI_OPCODE_DP3:
      read_mask = TGSI_WRITEMASK_XYZ;
      break;

   case TGSI_OPCODE_DSEQ:
   case TGSI_OPCODE_DSNE:
   case TGSI_OPCODE_DSLT:
   case TGSI_OPCODE_DSGE:
   case TGSI_OPCODE_DP4:
   case TGSI_OPCODE_PK4B:
   case TGSI_OPCODE_PK4UB:
   case TGSI_OPCODE_D2F:
   case TGSI_OPCODE_D2I:
   case TGSI_OPCODE_D2U:
   case TGSI_OPCODE_I2F:
   case TGSI_OPCODE_U2F:
   case TGSI_OPCODE_U64SEQ:
   case TGSI_OPCODE_U64SNE:
   case TGSI_OPCODE_U64SLT:
   case TGSI_OPCODE_U64SGE:
   case TGSI_OPCODE_U642F:
   case TGSI_OPCODE_I64SLT:
   case TGSI_OPCODE_I64SGE:
   case TGSI_OPCODE_I642F:
      read_mask = TGSI_WRITEMASK_XYZW;
      break;

   case TGSI_OPCODE_LIT:
      read_mask = write_mask & TGSI_WRITEMASK_YZ ?
                     TGSI_WRITEMASK_XY | TGSI_WRITEMASK_W : 0;
      break;

   case TGSI_OPCODE_EXP:
   case TGSI_OPCODE_LOG:
      read_mask = write_mask & TGSI_WRITEMASK_XYZ ? TGSI_WRITEMASK_X : 0;
      break;

   case TGSI_OPCODE_DST:
      if (src_idx == 0)
         read_mask = TGSI_WRITEMASK_YZ;
      else
         read_mask = TGSI_WRITEMASK_YW;
      break;

   case TGSI_OPCODE_DLDEXP:
      if (src_idx == 0) {
         read_mask = write_mask;
      } else {
         read_mask =
            (write_mask & TGSI_WRITEMASK_XY ? TGSI_WRITEMASK_X : 0) |
            (write_mask & TGSI_WRITEMASK_ZW ? TGSI_WRITEMASK_Z : 0);
      }
      break;

   case TGSI_OPCODE_READ_INVOC:
      if (src_idx == 0)
         read_mask = write_mask;
      else
         read_mask = TGSI_WRITEMASK_X;
      break;

   case TGSI_OPCODE_FBFETCH:
      read_mask = 0; /* not a real register read */
      break;

   case TGSI_OPCODE_TEX:
   case TGSI_OPCODE_TEX_LZ:
   case TGSI_OPCODE_TXF_LZ:
   case TGSI_OPCODE_TXF:
   case TGSI_OPCODE_TXB:
   case TGSI_OPCODE_TXL:
   case TGSI_OPCODE_TXP:
   case TGSI_OPCODE_TXD:
   case TGSI_OPCODE_TEX2:
   case TGSI_OPCODE_TXB2:
   case TGSI_OPCODE_TXL2:
   case TGSI_OPCODE_LODQ:
   case TGSI_OPCODE_TG4: {
      unsigned dim_layer =
         tgsi_util_get_texture_coord_dim(inst->Texture.Texture);
      unsigned dim_layer_shadow, dim;

      /* Add shadow. */
      if (tgsi_is_shadow_target(inst->Texture.Texture)) {
         dim_layer_shadow = dim_layer + 1;
         if (inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D)
            dim_layer_shadow = 3;
      } else {
         dim_layer_shadow = dim_layer;
      }

      /* Remove layer. */
      if (tgsi_is_array_sampler(inst->Texture.Texture))
         dim = dim_layer - 1;
      else
         dim = dim_layer;

      read_mask = TGSI_WRITEMASK_XY; /* bindless handle in the last operand */

      switch (src_idx) {
      case 0:
         if (inst->Instruction.Opcode == TGSI_OPCODE_LODQ)
            read_mask = u_bit_consecutive(0, dim);
         else
            read_mask = u_bit_consecutive(0, dim_layer_shadow) & 0xf;

         if (inst->Texture.Texture == TGSI_TEXTURE_SHADOW1D)
            read_mask &= ~TGSI_WRITEMASK_Y;

         if (inst->Instruction.Opcode == TGSI_OPCODE_TXF ||
             inst->Instruction.Opcode == TGSI_OPCODE_TXB ||
             inst->Instruction.Opcode == TGSI_OPCODE_TXL ||
             inst->Instruction.Opcode == TGSI_OPCODE_TXP)
            read_mask |= TGSI_WRITEMASK_W;
         break;

      case 1:
         if (inst->Instruction.Opcode == TGSI_OPCODE_TXD)
            read_mask = u_bit_consecutive(0, dim);
         else if (inst->Instruction.Opcode == TGSI_OPCODE_TEX2 ||
                  inst->Instruction.Opcode == TGSI_OPCODE_TXB2 ||
                  inst->Instruction.Opcode == TGSI_OPCODE_TXL2 ||
                  inst->Instruction.Opcode == TGSI_OPCODE_TG4)
            read_mask = TGSI_WRITEMASK_X;
         break;

      case 2:
         if (inst->Instruction.Opcode == TGSI_OPCODE_TXD)
            read_mask = u_bit_consecutive(0, dim);
         break;
      }
      break;
   }

   case TGSI_OPCODE_LOAD:
      if (src_idx == 0) {
         read_mask = TGSI_WRITEMASK_XY; /* bindless handle possible */
      } else {
         unsigned dim = tgsi_util_get_texture_coord_dim(inst->Memory.Texture);
         read_mask = u_bit_consecutive(0, dim);
      }
      break;

   case TGSI_OPCODE_STORE:
      if (src_idx == 0) {
         unsigned dim = tgsi_util_get_texture_coord_dim(inst->Memory.Texture);
         read_mask = u_bit_consecutive(0, dim);
      } else {
         read_mask = TGSI_WRITEMASK_XYZW;
      }
      break;

   case TGSI_OPCODE_ATOMUADD:
   case TGSI_OPCODE_ATOMXCHG:
   case TGSI_OPCODE_ATOMCAS:
   case TGSI_OPCODE_ATOMAND:
   case TGSI_OPCODE_ATOMOR:
   case TGSI_OPCODE_ATOMXOR:
   case TGSI_OPCODE_ATOMUMIN:
   case TGSI_OPCODE_ATOMUMAX:
   case TGSI_OPCODE_ATOMIMIN:
   case TGSI_OPCODE_ATOMIMAX:
      if (src_idx == 0) {
         read_mask = TGSI_WRITEMASK_XY; /* bindless handle possible */
      } else if (src_idx == 1) {
         unsigned dim = tgsi_util_get_texture_coord_dim(inst->Memory.Texture);
         read_mask = u_bit_consecutive(0, dim);
      } else {
         read_mask = TGSI_WRITEMASK_XYZW;
      }
      break;

   case TGSI_OPCODE_INTERP_CENTROID:
   case TGSI_OPCODE_INTERP_SAMPLE:
   case TGSI_OPCODE_INTERP_OFFSET:
      if (src_idx == 0)
         read_mask = write_mask;
      else if (inst->Instruction.Opcode == TGSI_OPCODE_INTERP_OFFSET)
         read_mask = TGSI_WRITEMASK_XY; /* offset */
      else
         read_mask = TGSI_WRITEMASK_X; /* sample */
      break;

   default:
      if (tgsi_get_opcode_info(inst->Instruction.Opcode)->output_mode ==
          TGSI_OUTPUT_COMPONENTWISE)
         read_mask = write_mask;
      else
         read_mask = TGSI_WRITEMASK_XYZW; /* assume all channels are read */
      break;
   }

   usage_mask = 0;
   for (chan = 0; chan < 4; ++chan) {
      if (read_mask & (1 << chan)) {
         usage_mask |= 1 << tgsi_util_get_full_src_register_swizzle(src, chan);
      }
   }

   return usage_mask;
}

/**
 * Convert a tgsi_ind_register into a tgsi_src_register
 */
struct tgsi_src_register
tgsi_util_get_src_from_ind(const struct tgsi_ind_register *reg)
{
   struct tgsi_src_register src = { 0 };

   src.File = reg->File;
   src.Index = reg->Index;
   src.SwizzleX = reg->Swizzle;
   src.SwizzleY = reg->Swizzle;
   src.SwizzleZ = reg->Swizzle;
   src.SwizzleW = reg->Swizzle;

   return src;
}

/**
 * Return the dimension of the texture coordinates (layer included for array
 * textures), as well as the location of the shadow reference value or the
 * sample index.
 */
int
tgsi_util_get_texture_coord_dim(unsigned tgsi_tex)
{
   /*
    * Depending on the texture target, (src0.xyzw, src1.x) is interpreted
    * differently:
    *
    *   (s, X, X, X, X),               for BUFFER
    *   (s, X, X, X, X),               for 1D
    *   (s, t, X, X, X),               for 2D, RECT
    *   (s, t, r, X, X),               for 3D, CUBE
    *
    *   (s, layer, X, X, X),           for 1D_ARRAY
    *   (s, t, layer, X, X),           for 2D_ARRAY
    *   (s, t, r, layer, X),           for CUBE_ARRAY
    *
    *   (s, X, shadow, X, X),          for SHADOW1D
    *   (s, t, shadow, X, X),          for SHADOW2D, SHADOWRECT
    *   (s, t, r, shadow, X),          for SHADOWCUBE
    *
    *   (s, layer, shadow, X, X),      for SHADOW1D_ARRAY
    *   (s, t, layer, shadow, X),      for SHADOW2D_ARRAY
    *   (s, t, r, layer, shadow),      for SHADOWCUBE_ARRAY
    *
    *   (s, t, sample, X, X),          for 2D_MSAA
    *   (s, t, layer, sample, X),      for 2D_ARRAY_MSAA
    */
   switch (tgsi_tex) {
   case TGSI_TEXTURE_BUFFER:
   case TGSI_TEXTURE_1D:
   case TGSI_TEXTURE_SHADOW1D:
      return 1;
   case TGSI_TEXTURE_2D:
   case TGSI_TEXTURE_RECT:
   case TGSI_TEXTURE_1D_ARRAY:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_2D_MSAA:
      return 2;
   case TGSI_TEXTURE_3D:
   case TGSI_TEXTURE_CUBE:
   case TGSI_TEXTURE_2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      return 3;
   case TGSI_TEXTURE_CUBE_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
      return 4;
   default:
      assert(!"unknown texture target");
      return 0;
   }
}


/**
 * Given a TGSI_TEXTURE_x target, return the src register index for the
 * shadow reference coordinate.
 */
int
tgsi_util_get_shadow_ref_src_index(unsigned tgsi_tex)
{
   switch (tgsi_tex) {
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
      return 2;
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_2D_MSAA:
   case TGSI_TEXTURE_2D_ARRAY_MSAA:
      return 3;
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
      return 4;
   default:
      /* no shadow nor sample */
      return -1;
   }
}


boolean
tgsi_is_shadow_target(unsigned target)
{
   switch (target) {
   case TGSI_TEXTURE_SHADOW1D:
   case TGSI_TEXTURE_SHADOW2D:
   case TGSI_TEXTURE_SHADOWRECT:
   case TGSI_TEXTURE_SHADOW1D_ARRAY:
   case TGSI_TEXTURE_SHADOW2D_ARRAY:
   case TGSI_TEXTURE_SHADOWCUBE:
   case TGSI_TEXTURE_SHADOWCUBE_ARRAY:
      return TRUE;
   default:
      return FALSE;
   }
}
