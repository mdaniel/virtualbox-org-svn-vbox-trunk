/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics to
 develop this 3D driver.

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

 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */

#ifndef BRW_EU_DEFINES_H
#define BRW_EU_DEFINES_H

#include "util/macros.h"

/* The following hunk, up-to "Execution Unit" is used by both the
 * intel/compiler and i965 codebase. */

#define INTEL_MASK(high, low) (((1u<<((high)-(low)+1))-1)<<(low))
/* Using the GNU statement expression extension */
#define SET_FIELD(value, field)                                         \
   ({                                                                   \
      uint32_t fieldval = (value) << field ## _SHIFT;                   \
      assert((fieldval & ~ field ## _MASK) == 0);                       \
      fieldval & field ## _MASK;                                        \
   })

#define GET_BITS(data, high, low) ((data & INTEL_MASK((high), (low))) >> (low))
#define GET_FIELD(word, field) (((word)  & field ## _MASK) >> field ## _SHIFT)

#define _3DPRIM_POINTLIST         0x01
#define _3DPRIM_LINELIST          0x02
#define _3DPRIM_LINESTRIP         0x03
#define _3DPRIM_TRILIST           0x04
#define _3DPRIM_TRISTRIP          0x05
#define _3DPRIM_TRIFAN            0x06
#define _3DPRIM_QUADLIST          0x07
#define _3DPRIM_QUADSTRIP         0x08
#define _3DPRIM_LINELIST_ADJ      0x09 /* G45+ */
#define _3DPRIM_LINESTRIP_ADJ     0x0A /* G45+ */
#define _3DPRIM_TRILIST_ADJ       0x0B /* G45+ */
#define _3DPRIM_TRISTRIP_ADJ      0x0C /* G45+ */
#define _3DPRIM_TRISTRIP_REVERSE  0x0D
#define _3DPRIM_POLYGON           0x0E
#define _3DPRIM_RECTLIST          0x0F
#define _3DPRIM_LINELOOP          0x10
#define _3DPRIM_POINTLIST_BF      0x11
#define _3DPRIM_LINESTRIP_CONT    0x12
#define _3DPRIM_LINESTRIP_BF      0x13
#define _3DPRIM_LINESTRIP_CONT_BF 0x14
#define _3DPRIM_TRIFAN_NOSTIPPLE  0x16
#define _3DPRIM_PATCHLIST(n) ({ assert(n > 0 && n <= 32); 0x20 + (n - 1); })

/* Bitfields for the URB_WRITE message, DW2 of message header: */
#define URB_WRITE_PRIM_END		0x1
#define URB_WRITE_PRIM_START		0x2
#define URB_WRITE_PRIM_TYPE_SHIFT	2

#define BRW_SPRITE_POINT_ENABLE  16

# define GEN7_GS_CONTROL_DATA_FORMAT_GSCTL_CUT		0
# define GEN7_GS_CONTROL_DATA_FORMAT_GSCTL_SID		1

/* Execution Unit (EU) defines
 */

#define BRW_ALIGN_1   0
#define BRW_ALIGN_16  1

#define BRW_ADDRESS_DIRECT                        0
#define BRW_ADDRESS_REGISTER_INDIRECT_REGISTER    1

#define BRW_CHANNEL_X     0
#define BRW_CHANNEL_Y     1
#define BRW_CHANNEL_Z     2
#define BRW_CHANNEL_W     3

enum brw_compression {
   BRW_COMPRESSION_NONE       = 0,
   BRW_COMPRESSION_2NDHALF    = 1,
   BRW_COMPRESSION_COMPRESSED = 2,
};

#define GEN6_COMPRESSION_1Q		0
#define GEN6_COMPRESSION_2Q		1
#define GEN6_COMPRESSION_3Q		2
#define GEN6_COMPRESSION_4Q		3
#define GEN6_COMPRESSION_1H		0
#define GEN6_COMPRESSION_2H		2

enum PACKED brw_conditional_mod {
   BRW_CONDITIONAL_NONE = 0,
   BRW_CONDITIONAL_Z    = 1,
   BRW_CONDITIONAL_NZ   = 2,
   BRW_CONDITIONAL_EQ   = 1,	/* Z */
   BRW_CONDITIONAL_NEQ  = 2,	/* NZ */
   BRW_CONDITIONAL_G    = 3,
   BRW_CONDITIONAL_GE   = 4,
   BRW_CONDITIONAL_L    = 5,
   BRW_CONDITIONAL_LE   = 6,
   BRW_CONDITIONAL_R    = 7,    /* Gen <= 5 */
   BRW_CONDITIONAL_O    = 8,
   BRW_CONDITIONAL_U    = 9,
};

#define BRW_DEBUG_NONE        0
#define BRW_DEBUG_BREAKPOINT  1

#define BRW_DEPENDENCY_NORMAL         0
#define BRW_DEPENDENCY_NOTCLEARED     1
#define BRW_DEPENDENCY_NOTCHECKED     2
#define BRW_DEPENDENCY_DISABLE        3

enum PACKED brw_execution_size {
   BRW_EXECUTE_1  = 0,
   BRW_EXECUTE_2  = 1,
   BRW_EXECUTE_4  = 2,
   BRW_EXECUTE_8  = 3,
   BRW_EXECUTE_16 = 4,
   BRW_EXECUTE_32 = 5,
};

enum PACKED brw_horizontal_stride {
   BRW_HORIZONTAL_STRIDE_0 = 0,
   BRW_HORIZONTAL_STRIDE_1 = 1,
   BRW_HORIZONTAL_STRIDE_2 = 2,
   BRW_HORIZONTAL_STRIDE_4 = 3,
};

enum PACKED gen10_align1_3src_src_horizontal_stride {
   BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0 = 0,
   BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1 = 1,
   BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2 = 2,
   BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4 = 3,
};

enum PACKED gen10_align1_3src_dst_horizontal_stride {
   BRW_ALIGN1_3SRC_DST_HORIZONTAL_STRIDE_1 = 0,
   BRW_ALIGN1_3SRC_DST_HORIZONTAL_STRIDE_2 = 1,
};

#define BRW_INSTRUCTION_NORMAL    0
#define BRW_INSTRUCTION_SATURATE  1

#define BRW_MASK_ENABLE   0
#define BRW_MASK_DISABLE  1

/** @{
 *
 * Gen6 has replaced "mask enable/disable" with WECtrl, which is
 * effectively the same but much simpler to think about.  Now, there
 * are two contributors ANDed together to whether channels are
 * executed: The predication on the instruction, and the channel write
 * enable.
 */
/**
 * This is the default value.  It means that a channel's write enable is set
 * if the per-channel IP is pointing at this instruction.
 */
#define BRW_WE_NORMAL		0
/**
 * This is used like BRW_MASK_DISABLE, and causes all channels to have
 * their write enable set.  Note that predication still contributes to
 * whether the channel actually gets written.
 */
#define BRW_WE_ALL		1
/** @} */

enum opcode {
   /* These are the actual hardware opcodes. */
   BRW_OPCODE_ILLEGAL = 0,
   BRW_OPCODE_MOV =	1,
   BRW_OPCODE_SEL =	2,
   BRW_OPCODE_MOVI =	3,   /**< G45+ */
   BRW_OPCODE_NOT =	4,
   BRW_OPCODE_AND =	5,
   BRW_OPCODE_OR =	6,
   BRW_OPCODE_XOR =	7,
   BRW_OPCODE_SHR =	8,
   BRW_OPCODE_SHL =	9,
   BRW_OPCODE_DIM =	10,  /**< Gen7.5 only */ /* Reused */
   // BRW_OPCODE_SMOV =	10,  /**< Gen8+       */ /* Reused */
   /* Reserved - 11 */
   BRW_OPCODE_ASR =	12,
   /* Reserved - 13-15 */
   BRW_OPCODE_CMP =	16,
   BRW_OPCODE_CMPN =	17,
   BRW_OPCODE_CSEL =	18,  /**< Gen8+ */
   BRW_OPCODE_F32TO16 = 19,  /**< Gen7 only */
   BRW_OPCODE_F16TO32 = 20,  /**< Gen7 only */
   /* Reserved - 21-22 */
   BRW_OPCODE_BFREV =	23,  /**< Gen7+ */
   BRW_OPCODE_BFE =	24,  /**< Gen7+ */
   BRW_OPCODE_BFI1 =	25,  /**< Gen7+ */
   BRW_OPCODE_BFI2 =	26,  /**< Gen7+ */
   /* Reserved - 27-31 */
   BRW_OPCODE_JMPI =	32,
   // BRW_OPCODE_BRD =	33,  /**< Gen7+ */
   BRW_OPCODE_IF =	34,
   BRW_OPCODE_IFF =	35,  /**< Pre-Gen6    */ /* Reused */
   // BRW_OPCODE_BRC =	35,  /**< Gen7+       */ /* Reused */
   BRW_OPCODE_ELSE =	36,
   BRW_OPCODE_ENDIF =	37,
   BRW_OPCODE_DO =	38,  /**< Pre-Gen6    */ /* Reused */
   // BRW_OPCODE_CASE =	38,  /**< Gen6 only   */ /* Reused */
   BRW_OPCODE_WHILE =	39,
   BRW_OPCODE_BREAK =	40,
   BRW_OPCODE_CONTINUE = 41,
   BRW_OPCODE_HALT =	42,
   // BRW_OPCODE_CALLA =	43,  /**< Gen7.5+     */
   // BRW_OPCODE_MSAVE =	44,  /**< Pre-Gen6    */ /* Reused */
   // BRW_OPCODE_CALL =	44,  /**< Gen6+       */ /* Reused */
   // BRW_OPCODE_MREST =	45,  /**< Pre-Gen6    */ /* Reused */
   // BRW_OPCODE_RET =	45,  /**< Gen6+       */ /* Reused */
   // BRW_OPCODE_PUSH =	46,  /**< Pre-Gen6    */ /* Reused */
   // BRW_OPCODE_FORK =	46,  /**< Gen6 only   */ /* Reused */
   // BRW_OPCODE_GOTO =	46,  /**< Gen8+       */ /* Reused */
   // BRW_OPCODE_POP =	47,  /**< Pre-Gen6    */
   BRW_OPCODE_WAIT =	48,
   BRW_OPCODE_SEND =	49,
   BRW_OPCODE_SENDC =	50,
   BRW_OPCODE_SENDS =	51,  /**< Gen9+ */
   BRW_OPCODE_SENDSC =	52,  /**< Gen9+ */
   /* Reserved 53-55 */
   BRW_OPCODE_MATH =	56,  /**< Gen6+ */
   /* Reserved 57-63 */
   BRW_OPCODE_ADD =	64,
   BRW_OPCODE_MUL =	65,
   BRW_OPCODE_AVG =	66,
   BRW_OPCODE_FRC =	67,
   BRW_OPCODE_RNDU =	68,
   BRW_OPCODE_RNDD =	69,
   BRW_OPCODE_RNDE =	70,
   BRW_OPCODE_RNDZ =	71,
   BRW_OPCODE_MAC =	72,
   BRW_OPCODE_MACH =	73,
   BRW_OPCODE_LZD =	74,
   BRW_OPCODE_FBH =	75,  /**< Gen7+ */
   BRW_OPCODE_FBL =	76,  /**< Gen7+ */
   BRW_OPCODE_CBIT =	77,  /**< Gen7+ */
   BRW_OPCODE_ADDC =	78,  /**< Gen7+ */
   BRW_OPCODE_SUBB =	79,  /**< Gen7+ */
   BRW_OPCODE_SAD2 =	80,
   BRW_OPCODE_SADA2 =	81,
   /* Reserved 82-83 */
   BRW_OPCODE_DP4 =	84,
   BRW_OPCODE_DPH =	85,
   BRW_OPCODE_DP3 =	86,
   BRW_OPCODE_DP2 =	87,
   /* Reserved 88 */
   BRW_OPCODE_LINE =	89,
   BRW_OPCODE_PLN =	90,  /**< G45+ */
   BRW_OPCODE_MAD =	91,  /**< Gen6+ */
   BRW_OPCODE_LRP =	92,  /**< Gen6+ */
   // BRW_OPCODE_MADM =	93,  /**< Gen8+ */
   /* Reserved 94-124 */
   BRW_OPCODE_NENOP =	125, /**< G45 only */
   BRW_OPCODE_NOP =	126,
   /* Reserved 127 */

   /* These are compiler backend opcodes that get translated into other
    * instructions.
    */
   FS_OPCODE_FB_WRITE = 128,

   /**
    * Same as FS_OPCODE_FB_WRITE but expects its arguments separately as
    * individual sources instead of as a single payload blob. The
    * position/ordering of the arguments are defined by the enum
    * fb_write_logical_srcs.
    */
   FS_OPCODE_FB_WRITE_LOGICAL,

   FS_OPCODE_REP_FB_WRITE,

   FS_OPCODE_FB_READ,
   FS_OPCODE_FB_READ_LOGICAL,

   SHADER_OPCODE_RCP,
   SHADER_OPCODE_RSQ,
   SHADER_OPCODE_SQRT,
   SHADER_OPCODE_EXP2,
   SHADER_OPCODE_LOG2,
   SHADER_OPCODE_POW,
   SHADER_OPCODE_INT_QUOTIENT,
   SHADER_OPCODE_INT_REMAINDER,
   SHADER_OPCODE_SIN,
   SHADER_OPCODE_COS,

   /**
    * Texture sampling opcodes.
    *
    * LOGICAL opcodes are eventually translated to the matching non-LOGICAL
    * opcode but instead of taking a single payload blob they expect their
    * arguments separately as individual sources. The position/ordering of the
    * arguments are defined by the enum tex_logical_srcs.
    */
   SHADER_OPCODE_TEX,
   SHADER_OPCODE_TEX_LOGICAL,
   SHADER_OPCODE_TXD,
   SHADER_OPCODE_TXD_LOGICAL,
   SHADER_OPCODE_TXF,
   SHADER_OPCODE_TXF_LOGICAL,
   SHADER_OPCODE_TXF_LZ,
   SHADER_OPCODE_TXL,
   SHADER_OPCODE_TXL_LOGICAL,
   SHADER_OPCODE_TXL_LZ,
   SHADER_OPCODE_TXS,
   SHADER_OPCODE_TXS_LOGICAL,
   FS_OPCODE_TXB,
   FS_OPCODE_TXB_LOGICAL,
   SHADER_OPCODE_TXF_CMS,
   SHADER_OPCODE_TXF_CMS_LOGICAL,
   SHADER_OPCODE_TXF_CMS_W,
   SHADER_OPCODE_TXF_CMS_W_LOGICAL,
   SHADER_OPCODE_TXF_UMS,
   SHADER_OPCODE_TXF_UMS_LOGICAL,
   SHADER_OPCODE_TXF_MCS,
   SHADER_OPCODE_TXF_MCS_LOGICAL,
   SHADER_OPCODE_LOD,
   SHADER_OPCODE_LOD_LOGICAL,
   SHADER_OPCODE_TG4,
   SHADER_OPCODE_TG4_LOGICAL,
   SHADER_OPCODE_TG4_OFFSET,
   SHADER_OPCODE_TG4_OFFSET_LOGICAL,
   SHADER_OPCODE_SAMPLEINFO,
   SHADER_OPCODE_SAMPLEINFO_LOGICAL,

   /**
    * Combines multiple sources of size 1 into a larger virtual GRF.
    * For example, parameters for a send-from-GRF message.  Or, updating
    * channels of a size 4 VGRF used to store vec4s such as texturing results.
    *
    * This will be lowered into MOVs from each source to consecutive offsets
    * of the destination VGRF.
    *
    * src[0] may be BAD_FILE.  If so, the lowering pass skips emitting the MOV,
    * but still reserves the first channel of the destination VGRF.  This can be
    * used to reserve space for, say, a message header set up by the generators.
    */
   SHADER_OPCODE_LOAD_PAYLOAD,

   /**
    * Packs a number of sources into a single value. Unlike LOAD_PAYLOAD, this
    * acts intra-channel, obtaining the final value for each channel by
    * combining the sources values for the same channel, the first source
    * occupying the lowest bits and the last source occupying the highest
    * bits.
    */
   FS_OPCODE_PACK,

   SHADER_OPCODE_SHADER_TIME_ADD,

   /**
    * Typed and untyped surface access opcodes.
    *
    * LOGICAL opcodes are eventually translated to the matching non-LOGICAL
    * opcode but instead of taking a single payload blob they expect their
    * arguments separately as individual sources:
    *
    * Source 0: [required] Surface coordinates.
    * Source 1: [optional] Operation source.
    * Source 2: [required] Surface index.
    * Source 3: [required] Number of coordinate components (as UD immediate).
    * Source 4: [required] Opcode-specific control immediate, same as source 2
    *                      of the matching non-LOGICAL opcode.
    */
   SHADER_OPCODE_UNTYPED_ATOMIC,
   SHADER_OPCODE_UNTYPED_ATOMIC_LOGICAL,
   SHADER_OPCODE_UNTYPED_SURFACE_READ,
   SHADER_OPCODE_UNTYPED_SURFACE_READ_LOGICAL,
   SHADER_OPCODE_UNTYPED_SURFACE_WRITE,
   SHADER_OPCODE_UNTYPED_SURFACE_WRITE_LOGICAL,

   SHADER_OPCODE_TYPED_ATOMIC,
   SHADER_OPCODE_TYPED_ATOMIC_LOGICAL,
   SHADER_OPCODE_TYPED_SURFACE_READ,
   SHADER_OPCODE_TYPED_SURFACE_READ_LOGICAL,
   SHADER_OPCODE_TYPED_SURFACE_WRITE,
   SHADER_OPCODE_TYPED_SURFACE_WRITE_LOGICAL,

   SHADER_OPCODE_MEMORY_FENCE,

   SHADER_OPCODE_GEN4_SCRATCH_READ,
   SHADER_OPCODE_GEN4_SCRATCH_WRITE,
   SHADER_OPCODE_GEN7_SCRATCH_READ,

   /**
    * Gen8+ SIMD8 URB Read messages.
    */
   SHADER_OPCODE_URB_READ_SIMD8,
   SHADER_OPCODE_URB_READ_SIMD8_PER_SLOT,

   SHADER_OPCODE_URB_WRITE_SIMD8,
   SHADER_OPCODE_URB_WRITE_SIMD8_PER_SLOT,
   SHADER_OPCODE_URB_WRITE_SIMD8_MASKED,
   SHADER_OPCODE_URB_WRITE_SIMD8_MASKED_PER_SLOT,

   /**
    * Return the index of an arbitrary live channel (i.e. one of the channels
    * enabled in the current execution mask) and assign it to the first
    * component of the destination.  Expected to be used as input for the
    * BROADCAST pseudo-opcode.
    */
   SHADER_OPCODE_FIND_LIVE_CHANNEL,

   /**
    * Pick the channel from its first source register given by the index
    * specified as second source.  Useful for variable indexing of surfaces.
    *
    * Note that because the result of this instruction is by definition
    * uniform and it can always be splatted to multiple channels using a
    * scalar regioning mode, only the first channel of the destination region
    * is guaranteed to be updated, which implies that BROADCAST instructions
    * should usually be marked force_writemask_all.
    */
   SHADER_OPCODE_BROADCAST,

   VEC4_OPCODE_MOV_BYTES,
   VEC4_OPCODE_PACK_BYTES,
   VEC4_OPCODE_UNPACK_UNIFORM,
   VEC4_OPCODE_DOUBLE_TO_F32,
   VEC4_OPCODE_DOUBLE_TO_D32,
   VEC4_OPCODE_DOUBLE_TO_U32,
   VEC4_OPCODE_TO_DOUBLE,
   VEC4_OPCODE_PICK_LOW_32BIT,
   VEC4_OPCODE_PICK_HIGH_32BIT,
   VEC4_OPCODE_SET_LOW_32BIT,
   VEC4_OPCODE_SET_HIGH_32BIT,

   FS_OPCODE_DDX_COARSE,
   FS_OPCODE_DDX_FINE,
   /**
    * Compute dFdy(), dFdyCoarse(), or dFdyFine().
    */
   FS_OPCODE_DDY_COARSE,
   FS_OPCODE_DDY_FINE,
   FS_OPCODE_CINTERP,
   FS_OPCODE_LINTERP,
   FS_OPCODE_PIXEL_X,
   FS_OPCODE_PIXEL_Y,
   FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD,
   FS_OPCODE_UNIFORM_PULL_CONSTANT_LOAD_GEN7,
   FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GEN4,
   FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_GEN7,
   FS_OPCODE_VARYING_PULL_CONSTANT_LOAD_LOGICAL,
   FS_OPCODE_GET_BUFFER_SIZE,
   FS_OPCODE_MOV_DISPATCH_TO_FLAGS,
   FS_OPCODE_DISCARD_JUMP,
   FS_OPCODE_SET_SAMPLE_ID,
   FS_OPCODE_PACK_HALF_2x16_SPLIT,
   FS_OPCODE_UNPACK_HALF_2x16_SPLIT_X,
   FS_OPCODE_UNPACK_HALF_2x16_SPLIT_Y,
   FS_OPCODE_PLACEHOLDER_HALT,
   FS_OPCODE_INTERPOLATE_AT_SAMPLE,
   FS_OPCODE_INTERPOLATE_AT_SHARED_OFFSET,
   FS_OPCODE_INTERPOLATE_AT_PER_SLOT_OFFSET,

   VS_OPCODE_URB_WRITE,
   VS_OPCODE_PULL_CONSTANT_LOAD,
   VS_OPCODE_PULL_CONSTANT_LOAD_GEN7,
   VS_OPCODE_SET_SIMD4X2_HEADER_GEN9,

   VS_OPCODE_GET_BUFFER_SIZE,

   VS_OPCODE_UNPACK_FLAGS_SIMD4X2,

   /**
    * Write geometry shader output data to the URB.
    *
    * Unlike VS_OPCODE_URB_WRITE, this opcode doesn't do an implied move from
    * R0 to the first MRF.  This allows the geometry shader to override the
    * "Slot {0,1} Offset" fields in the message header.
    */
   GS_OPCODE_URB_WRITE,

   /**
    * Write geometry shader output data to the URB and request a new URB
    * handle (gen6).
    *
    * This opcode doesn't do an implied move from R0 to the first MRF.
    */
   GS_OPCODE_URB_WRITE_ALLOCATE,

   /**
    * Terminate the geometry shader thread by doing an empty URB write.
    *
    * This opcode doesn't do an implied move from R0 to the first MRF.  This
    * allows the geometry shader to override the "GS Number of Output Vertices
    * for Slot {0,1}" fields in the message header.
    */
   GS_OPCODE_THREAD_END,

   /**
    * Set the "Slot {0,1} Offset" fields of a URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src0.x indicates which portion of the URB should be written to (e.g. a
    *   vertex number)
    *
    * - src1 is an immediate multiplier which will be applied to src0
    *   (e.g. the size of a single vertex in the URB).
    *
    * Note: the hardware will apply this offset *in addition to* the offset in
    * vec4_instruction::offset.
    */
   GS_OPCODE_SET_WRITE_OFFSET,

   /**
    * Set the "GS Number of Output Vertices for Slot {0,1}" fields of a
    * URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src0.x is the vertex count.  The upper 16 bits will be ignored.
    */
   GS_OPCODE_SET_VERTEX_COUNT,

   /**
    * Set DWORD 2 of dst to the value in src.
    */
   GS_OPCODE_SET_DWORD_2,

   /**
    * Prepare the dst register for storage in the "Channel Mask" fields of a
    * URB_WRITE message header.
    *
    * DWORD 4 of dst is shifted left by 4 bits, so that later,
    * GS_OPCODE_SET_CHANNEL_MASKS can OR DWORDs 0 and 4 together to form the
    * final channel mask.
    *
    * Note: since GS_OPCODE_SET_CHANNEL_MASKS ORs DWORDs 0 and 4 together to
    * form the final channel mask, DWORDs 0 and 4 of the dst register must not
    * have any extraneous bits set prior to execution of this opcode (that is,
    * they should be in the range 0x0 to 0xf).
    */
   GS_OPCODE_PREPARE_CHANNEL_MASKS,

   /**
    * Set the "Channel Mask" fields of a URB_WRITE message header.
    *
    * - dst is the MRF containing the message header.
    *
    * - src.x is the channel mask, as prepared by
    *   GS_OPCODE_PREPARE_CHANNEL_MASKS.  DWORDs 0 and 4 are OR'ed together to
    *   form the final channel mask.
    */
   GS_OPCODE_SET_CHANNEL_MASKS,

   /**
    * Get the "Instance ID" fields from the payload.
    *
    * - dst is the GRF for gl_InvocationID.
    */
   GS_OPCODE_GET_INSTANCE_ID,

   /**
    * Send a FF_SYNC message to allocate initial URB handles (gen6).
    *
    * - dst will be used as the writeback register for the FF_SYNC operation.
    *
    * - src0 is the number of primitives written.
    *
    * - src1 is the value to hold in M0.0: number of SO vertices to write
    *   and number of SO primitives needed. Its value will be overwritten
    *   with the SVBI values if transform feedback is enabled.
    *
    * Note: This opcode uses an implicit MRF register for the ff_sync message
    * header, so the caller is expected to set inst->base_mrf and initialize
    * that MRF register to r0. This opcode will also write to this MRF register
    * to include the allocated URB handle so it can then be reused directly as
    * the header in the URB write operation we are allocating the handle for.
    */
   GS_OPCODE_FF_SYNC,

   /**
    * Move r0.1 (which holds PrimitiveID information in gen6) to a separate
    * register.
    *
    * - dst is the GRF where PrimitiveID information will be moved.
    */
   GS_OPCODE_SET_PRIMITIVE_ID,

   /**
    * Write transform feedback data to the SVB by sending a SVB WRITE message.
    * Used in gen6.
    *
    * - dst is the MRF register containing the message header.
    *
    * - src0 is the register where the vertex data is going to be copied from.
    *
    * - src1 is the destination register when write commit occurs.
    */
   GS_OPCODE_SVB_WRITE,

   /**
    * Set destination index in the SVB write message payload (M0.5). Used
    * in gen6 for transform feedback.
    *
    * - dst is the header to save the destination indices for SVB WRITE.
    * - src is the register that holds the destination indices value.
    */
   GS_OPCODE_SVB_SET_DST_INDEX,

   /**
    * Prepare Mx.0 subregister for being used in the FF_SYNC message header.
    * Used in gen6 for transform feedback.
    *
    * - dst will hold the register with the final Mx.0 value.
    *
    * - src0 has the number of vertices emitted in SO (NumSOVertsToWrite)
    *
    * - src1 has the number of needed primitives for SO (NumSOPrimsNeeded)
    *
    * - src2 is the value to hold in M0: number of SO vertices to write
    *   and number of SO primitives needed.
    */
   GS_OPCODE_FF_SYNC_SET_PRIMITIVES,

   /**
    * Terminate the compute shader.
    */
   CS_OPCODE_CS_TERMINATE,

   /**
    * GLSL barrier()
    */
   SHADER_OPCODE_BARRIER,

   /**
    * Calculate the high 32-bits of a 32x32 multiply.
    */
   SHADER_OPCODE_MULH,

   /**
    * A MOV that uses VxH indirect addressing.
    *
    * Source 0: A register to start from (HW_REG).
    * Source 1: An indirect offset (in bytes, UD GRF).
    * Source 2: The length of the region that could be accessed (in bytes,
    *           UD immediate).
    */
   SHADER_OPCODE_MOV_INDIRECT,

   VEC4_OPCODE_URB_READ,
   TCS_OPCODE_GET_INSTANCE_ID,
   TCS_OPCODE_URB_WRITE,
   TCS_OPCODE_SET_INPUT_URB_OFFSETS,
   TCS_OPCODE_SET_OUTPUT_URB_OFFSETS,
   TCS_OPCODE_GET_PRIMITIVE_ID,
   TCS_OPCODE_CREATE_BARRIER_HEADER,
   TCS_OPCODE_SRC0_010_IS_ZERO,
   TCS_OPCODE_RELEASE_INPUT,
   TCS_OPCODE_THREAD_END,

   TES_OPCODE_GET_PRIMITIVE_ID,
   TES_OPCODE_CREATE_INPUT_READ_HEADER,
   TES_OPCODE_ADD_INDIRECT_URB_OFFSET,
};

enum brw_urb_write_flags {
   BRW_URB_WRITE_NO_FLAGS = 0,

   /**
    * Causes a new URB entry to be allocated, and its address stored in the
    * destination register (gen < 7).
    */
   BRW_URB_WRITE_ALLOCATE = 0x1,

   /**
    * Causes the current URB entry to be deallocated (gen < 7).
    */
   BRW_URB_WRITE_UNUSED = 0x2,

   /**
    * Causes the thread to terminate.
    */
   BRW_URB_WRITE_EOT = 0x4,

   /**
    * Indicates that the given URB entry is complete, and may be sent further
    * down the 3D pipeline (gen < 7).
    */
   BRW_URB_WRITE_COMPLETE = 0x8,

   /**
    * Indicates that an additional offset (which may be different for the two
    * vec4 slots) is stored in the message header (gen == 7).
    */
   BRW_URB_WRITE_PER_SLOT_OFFSET = 0x10,

   /**
    * Indicates that the channel masks in the URB_WRITE message header should
    * not be overridden to 0xff (gen == 7).
    */
   BRW_URB_WRITE_USE_CHANNEL_MASKS = 0x20,

   /**
    * Indicates that the data should be sent to the URB using the
    * URB_WRITE_OWORD message rather than URB_WRITE_HWORD (gen == 7).  This
    * causes offsets to be interpreted as multiples of an OWORD instead of an
    * HWORD, and only allows one OWORD to be written.
    */
   BRW_URB_WRITE_OWORD = 0x40,

   /**
    * Convenient combination of flags: end the thread while simultaneously
    * marking the given URB entry as complete.
    */
   BRW_URB_WRITE_EOT_COMPLETE = BRW_URB_WRITE_EOT | BRW_URB_WRITE_COMPLETE,

   /**
    * Convenient combination of flags: mark the given URB entry as complete
    * and simultaneously allocate a new one.
    */
   BRW_URB_WRITE_ALLOCATE_COMPLETE =
      BRW_URB_WRITE_ALLOCATE | BRW_URB_WRITE_COMPLETE,
};

enum fb_write_logical_srcs {
   FB_WRITE_LOGICAL_SRC_COLOR0,      /* REQUIRED */
   FB_WRITE_LOGICAL_SRC_COLOR1,      /* for dual source blend messages */
   FB_WRITE_LOGICAL_SRC_SRC0_ALPHA,
   FB_WRITE_LOGICAL_SRC_SRC_DEPTH,   /* gl_FragDepth */
   FB_WRITE_LOGICAL_SRC_DST_DEPTH,   /* GEN4-5: passthrough from thread */
   FB_WRITE_LOGICAL_SRC_SRC_STENCIL, /* gl_FragStencilRefARB */
   FB_WRITE_LOGICAL_SRC_OMASK,       /* Sample Mask (gl_SampleMask) */
   FB_WRITE_LOGICAL_SRC_COMPONENTS,  /* REQUIRED */
   FB_WRITE_LOGICAL_NUM_SRCS
};

enum tex_logical_srcs {
   /** Texture coordinates */
   TEX_LOGICAL_SRC_COORDINATE,
   /** Shadow comparator */
   TEX_LOGICAL_SRC_SHADOW_C,
   /** dPdx if the operation takes explicit derivatives, otherwise LOD value */
   TEX_LOGICAL_SRC_LOD,
   /** dPdy if the operation takes explicit derivatives */
   TEX_LOGICAL_SRC_LOD2,
   /** Sample index */
   TEX_LOGICAL_SRC_SAMPLE_INDEX,
   /** MCS data */
   TEX_LOGICAL_SRC_MCS,
   /** REQUIRED: Texture surface index */
   TEX_LOGICAL_SRC_SURFACE,
   /** Texture sampler index */
   TEX_LOGICAL_SRC_SAMPLER,
   /** Texel offset for gathers */
   TEX_LOGICAL_SRC_TG4_OFFSET,
   /** REQUIRED: Number of coordinate components (as UD immediate) */
   TEX_LOGICAL_SRC_COORD_COMPONENTS,
   /** REQUIRED: Number of derivative components (as UD immediate) */
   TEX_LOGICAL_SRC_GRAD_COMPONENTS,

   TEX_LOGICAL_NUM_SRCS,
};

#ifdef __cplusplus
/**
 * Allow brw_urb_write_flags enums to be ORed together.
 */
inline brw_urb_write_flags
operator|(brw_urb_write_flags x, brw_urb_write_flags y)
{
   return static_cast<brw_urb_write_flags>(static_cast<int>(x) |
                                           static_cast<int>(y));
}
#endif

enum PACKED brw_predicate {
   BRW_PREDICATE_NONE                =  0,
   BRW_PREDICATE_NORMAL              =  1,
   BRW_PREDICATE_ALIGN1_ANYV         =  2,
   BRW_PREDICATE_ALIGN1_ALLV         =  3,
   BRW_PREDICATE_ALIGN1_ANY2H        =  4,
   BRW_PREDICATE_ALIGN1_ALL2H        =  5,
   BRW_PREDICATE_ALIGN1_ANY4H        =  6,
   BRW_PREDICATE_ALIGN1_ALL4H        =  7,
   BRW_PREDICATE_ALIGN1_ANY8H        =  8,
   BRW_PREDICATE_ALIGN1_ALL8H        =  9,
   BRW_PREDICATE_ALIGN1_ANY16H       = 10,
   BRW_PREDICATE_ALIGN1_ALL16H       = 11,
   BRW_PREDICATE_ALIGN1_ANY32H       = 12,
   BRW_PREDICATE_ALIGN1_ALL32H       = 13,
   BRW_PREDICATE_ALIGN16_REPLICATE_X =  2,
   BRW_PREDICATE_ALIGN16_REPLICATE_Y =  3,
   BRW_PREDICATE_ALIGN16_REPLICATE_Z =  4,
   BRW_PREDICATE_ALIGN16_REPLICATE_W =  5,
   BRW_PREDICATE_ALIGN16_ANY4H       =  6,
   BRW_PREDICATE_ALIGN16_ALL4H       =  7,
};

enum PACKED brw_reg_file {
   BRW_ARCHITECTURE_REGISTER_FILE = 0,
   BRW_GENERAL_REGISTER_FILE      = 1,
   BRW_MESSAGE_REGISTER_FILE      = 2,
   BRW_IMMEDIATE_VALUE            = 3,

   ARF = BRW_ARCHITECTURE_REGISTER_FILE,
   FIXED_GRF = BRW_GENERAL_REGISTER_FILE,
   MRF = BRW_MESSAGE_REGISTER_FILE,
   IMM = BRW_IMMEDIATE_VALUE,

   /* These are not hardware values */
   VGRF,
   ATTR,
   UNIFORM, /* prog_data->params[reg] */
   BAD_FILE,
};

enum PACKED gen10_align1_3src_reg_file {
   BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE = 0,
   BRW_ALIGN1_3SRC_IMMEDIATE_VALUE       = 1, /* src0, src2 */
   BRW_ALIGN1_3SRC_ACCUMULATOR           = 1, /* dest, src1 */
};

/* CNL adds Align1 support for 3-src instructions. Bit 35 of the instruction
 * word is "Execution Datatype" which controls whether the instruction operates
 * on float or integer types. The register arguments have fields that offer
 * more fine control their respective types.
 */
enum PACKED gen10_align1_3src_exec_type {
   BRW_ALIGN1_3SRC_EXEC_TYPE_INT   = 0,
   BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT = 1,
};

#define BRW_ARF_NULL                  0x00
#define BRW_ARF_ADDRESS               0x10
#define BRW_ARF_ACCUMULATOR           0x20
#define BRW_ARF_FLAG                  0x30
#define BRW_ARF_MASK                  0x40
#define BRW_ARF_MASK_STACK            0x50
#define BRW_ARF_MASK_STACK_DEPTH      0x60
#define BRW_ARF_STATE                 0x70
#define BRW_ARF_CONTROL               0x80
#define BRW_ARF_NOTIFICATION_COUNT    0x90
#define BRW_ARF_IP                    0xA0
#define BRW_ARF_TDR                   0xB0
#define BRW_ARF_TIMESTAMP             0xC0

#define BRW_MRF_COMPR4			(1 << 7)

#define BRW_AMASK   0
#define BRW_IMASK   1
#define BRW_LMASK   2
#define BRW_CMASK   3



#define BRW_THREAD_NORMAL     0
#define BRW_THREAD_ATOMIC     1
#define BRW_THREAD_SWITCH     2

enum PACKED brw_vertical_stride {
   BRW_VERTICAL_STRIDE_0               = 0,
   BRW_VERTICAL_STRIDE_1               = 1,
   BRW_VERTICAL_STRIDE_2               = 2,
   BRW_VERTICAL_STRIDE_4               = 3,
   BRW_VERTICAL_STRIDE_8               = 4,
   BRW_VERTICAL_STRIDE_16              = 5,
   BRW_VERTICAL_STRIDE_32              = 6,
   BRW_VERTICAL_STRIDE_ONE_DIMENSIONAL = 0xF,
};

enum PACKED gen10_align1_3src_vertical_stride {
   BRW_ALIGN1_3SRC_VERTICAL_STRIDE_0 = 0,
   BRW_ALIGN1_3SRC_VERTICAL_STRIDE_2 = 1,
   BRW_ALIGN1_3SRC_VERTICAL_STRIDE_4 = 2,
   BRW_ALIGN1_3SRC_VERTICAL_STRIDE_8 = 3,
};

enum PACKED brw_width {
   BRW_WIDTH_1  = 0,
   BRW_WIDTH_2  = 1,
   BRW_WIDTH_4  = 2,
   BRW_WIDTH_8  = 3,
   BRW_WIDTH_16 = 4,
};

/**
 * Message target: Shared Function ID for where to SEND a message.
 *
 * These are enumerated in the ISA reference under "send - Send Message".
 * In particular, see the following tables:
 * - G45 PRM, Volume 4, Table 14-15 "Message Descriptor Definition"
 * - Sandybridge PRM, Volume 4 Part 2, Table 8-16 "Extended Message Descriptor"
 * - Ivybridge PRM, Volume 1 Part 1, section 3.2.7 "GPE Function IDs"
 */
enum brw_message_target {
   BRW_SFID_NULL                     = 0,
   BRW_SFID_MATH                     = 1, /* Only valid on Gen4-5 */
   BRW_SFID_SAMPLER                  = 2,
   BRW_SFID_MESSAGE_GATEWAY          = 3,
   BRW_SFID_DATAPORT_READ            = 4,
   BRW_SFID_DATAPORT_WRITE           = 5,
   BRW_SFID_URB                      = 6,
   BRW_SFID_THREAD_SPAWNER           = 7,
   BRW_SFID_VME                      = 8,

   GEN6_SFID_DATAPORT_SAMPLER_CACHE  = 4,
   GEN6_SFID_DATAPORT_RENDER_CACHE   = 5,
   GEN6_SFID_DATAPORT_CONSTANT_CACHE = 9,

   GEN7_SFID_DATAPORT_DATA_CACHE     = 10,
   GEN7_SFID_PIXEL_INTERPOLATOR      = 11,
   HSW_SFID_DATAPORT_DATA_CACHE_1    = 12,
   HSW_SFID_CRE                      = 13,
};

#define GEN7_MESSAGE_TARGET_DP_DATA_CACHE     10

#define BRW_SAMPLER_RETURN_FORMAT_FLOAT32     0
#define BRW_SAMPLER_RETURN_FORMAT_UINT32      2
#define BRW_SAMPLER_RETURN_FORMAT_SINT32      3

#define BRW_SAMPLER_MESSAGE_SIMD8_SAMPLE              0
#define BRW_SAMPLER_MESSAGE_SIMD16_SAMPLE             0
#define BRW_SAMPLER_MESSAGE_SIMD16_SAMPLE_BIAS        0
#define BRW_SAMPLER_MESSAGE_SIMD8_KILLPIX             1
#define BRW_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD        1
#define BRW_SAMPLER_MESSAGE_SIMD16_SAMPLE_LOD         1
#define BRW_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_GRADIENTS  2
#define BRW_SAMPLER_MESSAGE_SIMD8_SAMPLE_GRADIENTS    2
#define BRW_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_COMPARE    0
#define BRW_SAMPLER_MESSAGE_SIMD16_SAMPLE_COMPARE     2
#define BRW_SAMPLER_MESSAGE_SIMD8_SAMPLE_BIAS_COMPARE 0
#define BRW_SAMPLER_MESSAGE_SIMD4X2_SAMPLE_LOD_COMPARE 1
#define BRW_SAMPLER_MESSAGE_SIMD8_SAMPLE_LOD_COMPARE  1
#define BRW_SAMPLER_MESSAGE_SIMD4X2_RESINFO           2
#define BRW_SAMPLER_MESSAGE_SIMD16_RESINFO            2
#define BRW_SAMPLER_MESSAGE_SIMD4X2_LD                3
#define BRW_SAMPLER_MESSAGE_SIMD8_LD                  3
#define BRW_SAMPLER_MESSAGE_SIMD16_LD                 3

#define GEN5_SAMPLER_MESSAGE_SAMPLE              0
#define GEN5_SAMPLER_MESSAGE_SAMPLE_BIAS         1
#define GEN5_SAMPLER_MESSAGE_SAMPLE_LOD          2
#define GEN5_SAMPLER_MESSAGE_SAMPLE_COMPARE      3
#define GEN5_SAMPLER_MESSAGE_SAMPLE_DERIVS       4
#define GEN5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE 5
#define GEN5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE  6
#define GEN5_SAMPLER_MESSAGE_SAMPLE_LD           7
#define GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4      8
#define GEN5_SAMPLER_MESSAGE_LOD                 9
#define GEN5_SAMPLER_MESSAGE_SAMPLE_RESINFO      10
#define GEN6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO   11
#define GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C    16
#define GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO   17
#define GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C 18
#define HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE 20
#define GEN9_SAMPLER_MESSAGE_SAMPLE_LZ           24
#define GEN9_SAMPLER_MESSAGE_SAMPLE_C_LZ         25
#define GEN9_SAMPLER_MESSAGE_SAMPLE_LD_LZ        26
#define GEN9_SAMPLER_MESSAGE_SAMPLE_LD2DMS_W     28
#define GEN7_SAMPLER_MESSAGE_SAMPLE_LD_MCS       29
#define GEN7_SAMPLER_MESSAGE_SAMPLE_LD2DMS       30
#define GEN7_SAMPLER_MESSAGE_SAMPLE_LD2DSS       31

/* for GEN5 only */
#define BRW_SAMPLER_SIMD_MODE_SIMD4X2                   0
#define BRW_SAMPLER_SIMD_MODE_SIMD8                     1
#define BRW_SAMPLER_SIMD_MODE_SIMD16                    2
#define BRW_SAMPLER_SIMD_MODE_SIMD32_64                 3

/* GEN9 changes SIMD mode 0 to mean SIMD8D, but lets us get the SIMD4x2
 * behavior by setting bit 22 of dword 2 in the message header. */
#define GEN9_SAMPLER_SIMD_MODE_SIMD8D                   0
#define GEN9_SAMPLER_SIMD_MODE_EXTENSION_SIMD4X2        (1 << 22)

#define BRW_DATAPORT_OWORD_BLOCK_1_OWORDLOW   0
#define BRW_DATAPORT_OWORD_BLOCK_1_OWORDHIGH  1
#define BRW_DATAPORT_OWORD_BLOCK_2_OWORDS     2
#define BRW_DATAPORT_OWORD_BLOCK_4_OWORDS     3
#define BRW_DATAPORT_OWORD_BLOCK_8_OWORDS     4
#define BRW_DATAPORT_OWORD_BLOCK_DWORDS(n)              \
   ((n) == 4 ? BRW_DATAPORT_OWORD_BLOCK_1_OWORDLOW :    \
    (n) == 8 ? BRW_DATAPORT_OWORD_BLOCK_2_OWORDS :      \
    (n) == 16 ? BRW_DATAPORT_OWORD_BLOCK_4_OWORDS :     \
    (n) == 32 ? BRW_DATAPORT_OWORD_BLOCK_8_OWORDS :     \
    (abort(), ~0))

#define BRW_DATAPORT_OWORD_DUAL_BLOCK_1OWORD     0
#define BRW_DATAPORT_OWORD_DUAL_BLOCK_4OWORDS    2

#define BRW_DATAPORT_DWORD_SCATTERED_BLOCK_8DWORDS   2
#define BRW_DATAPORT_DWORD_SCATTERED_BLOCK_16DWORDS  3

/* This one stays the same across generations. */
#define BRW_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ          0
/* GEN4 */
#define BRW_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     1
#define BRW_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          2
#define BRW_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      3
/* G45, GEN5 */
#define G45_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ	    1
#define G45_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     2
#define G45_DATAPORT_READ_MESSAGE_AVC_LOOP_FILTER_READ	    3
#define G45_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          4
#define G45_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      6
/* GEN6 */
#define GEN6_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ	    1
#define GEN6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ     2
#define GEN6_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ          4
#define GEN6_DATAPORT_READ_MESSAGE_OWORD_UNALIGN_BLOCK_READ  5
#define GEN6_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ      6

#define BRW_DATAPORT_READ_TARGET_DATA_CACHE      0
#define BRW_DATAPORT_READ_TARGET_RENDER_CACHE    1
#define BRW_DATAPORT_READ_TARGET_SAMPLER_CACHE   2

#define BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE                0
#define BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD16_SINGLE_SOURCE_REPLICATED     1
#define BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN01         2
#define BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_DUAL_SOURCE_SUBSPAN23         3
#define BRW_DATAPORT_RENDER_TARGET_WRITE_SIMD8_SINGLE_SOURCE_SUBSPAN01       4

#define BRW_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE                0
#define BRW_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE           1
#define BRW_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE                2
#define BRW_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE            3
#define BRW_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE              4
#define BRW_DATAPORT_WRITE_MESSAGE_STREAMED_VERTEX_BUFFER_WRITE     5
#define BRW_DATAPORT_WRITE_MESSAGE_FLUSH_RENDER_CACHE               7

/* GEN6 */
#define GEN6_DATAPORT_WRITE_MESSAGE_DWORD_ATOMIC_WRITE              7
#define GEN6_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE               8
#define GEN6_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE          9
#define GEN6_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE               10
#define GEN6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE           11
#define GEN6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE             12
#define GEN6_DATAPORT_WRITE_MESSAGE_STREAMED_VB_WRITE               13
#define GEN6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_UNORM_WRITE       14

/* GEN7 */
#define GEN7_DATAPORT_RC_MEDIA_BLOCK_READ                           4
#define GEN7_DATAPORT_RC_TYPED_SURFACE_READ                         5
#define GEN7_DATAPORT_RC_TYPED_ATOMIC_OP                            6
#define GEN7_DATAPORT_RC_MEMORY_FENCE                               7
#define GEN7_DATAPORT_RC_MEDIA_BLOCK_WRITE                          10
#define GEN7_DATAPORT_RC_RENDER_TARGET_WRITE                        12
#define GEN7_DATAPORT_RC_TYPED_SURFACE_WRITE                        13
#define GEN7_DATAPORT_DC_OWORD_BLOCK_READ                           0
#define GEN7_DATAPORT_DC_UNALIGNED_OWORD_BLOCK_READ                 1
#define GEN7_DATAPORT_DC_OWORD_DUAL_BLOCK_READ                      2
#define GEN7_DATAPORT_DC_DWORD_SCATTERED_READ                       3
#define GEN7_DATAPORT_DC_BYTE_SCATTERED_READ                        4
#define GEN7_DATAPORT_DC_UNTYPED_SURFACE_READ                       5
#define GEN7_DATAPORT_DC_UNTYPED_ATOMIC_OP                          6
#define GEN7_DATAPORT_DC_MEMORY_FENCE                               7
#define GEN7_DATAPORT_DC_OWORD_BLOCK_WRITE                          8
#define GEN7_DATAPORT_DC_OWORD_DUAL_BLOCK_WRITE                     10
#define GEN7_DATAPORT_DC_DWORD_SCATTERED_WRITE                      11
#define GEN7_DATAPORT_DC_BYTE_SCATTERED_WRITE                       12
#define GEN7_DATAPORT_DC_UNTYPED_SURFACE_WRITE                      13

#define GEN7_DATAPORT_SCRATCH_READ                            ((1 << 18) | \
                                                               (0 << 17))
#define GEN7_DATAPORT_SCRATCH_WRITE                           ((1 << 18) | \
                                                               (1 << 17))
#define GEN7_DATAPORT_SCRATCH_NUM_REGS_SHIFT                        12

#define GEN7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET     0
#define GEN7_PIXEL_INTERPOLATOR_LOC_SAMPLE            1
#define GEN7_PIXEL_INTERPOLATOR_LOC_CENTROID          2
#define GEN7_PIXEL_INTERPOLATOR_LOC_PER_SLOT_OFFSET   3

/* HSW */
#define HSW_DATAPORT_DC_PORT0_OWORD_BLOCK_READ                      0
#define HSW_DATAPORT_DC_PORT0_UNALIGNED_OWORD_BLOCK_READ            1
#define HSW_DATAPORT_DC_PORT0_OWORD_DUAL_BLOCK_READ                 2
#define HSW_DATAPORT_DC_PORT0_DWORD_SCATTERED_READ                  3
#define HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_READ                   4
#define HSW_DATAPORT_DC_PORT0_MEMORY_FENCE                          7
#define HSW_DATAPORT_DC_PORT0_OWORD_BLOCK_WRITE                     8
#define HSW_DATAPORT_DC_PORT0_OWORD_DUAL_BLOCK_WRITE                10
#define HSW_DATAPORT_DC_PORT0_DWORD_SCATTERED_WRITE                 11
#define HSW_DATAPORT_DC_PORT0_BYTE_SCATTERED_WRITE                  12

#define HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ                  1
#define HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP                     2
#define HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2             3
#define HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_READ                      4
#define HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ                    5
#define HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP                       6
#define HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2               7
#define HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE                 9
#define HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_WRITE                     10
#define HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP                     11
#define HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2             12
#define HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE                   13

/* GEN9 */
#define GEN9_DATAPORT_RC_RENDER_TARGET_WRITE                        12
#define GEN9_DATAPORT_RC_RENDER_TARGET_READ                         13

/* Dataport special binding table indices: */
#define BRW_BTI_STATELESS                255
#define GEN7_BTI_SLM                     254
/* Note that on Gen8+ BTI 255 was redefined to be IA-coherent according to the
 * hardware spec, however because the DRM sets bit 4 of HDC_CHICKEN0 on BDW,
 * CHV and at least some pre-production steppings of SKL due to
 * WaForceEnableNonCoherent, HDC memory access may have been overridden by the
 * kernel to be non-coherent (matching the behavior of the same BTI on
 * pre-Gen8 hardware) and BTI 255 may actually be an alias for BTI 253.
 */
#define GEN8_BTI_STATELESS_IA_COHERENT   255
#define GEN8_BTI_STATELESS_NON_COHERENT  253

/* dataport atomic operations. */
#define BRW_AOP_AND                   1
#define BRW_AOP_OR                    2
#define BRW_AOP_XOR                   3
#define BRW_AOP_MOV                   4
#define BRW_AOP_INC                   5
#define BRW_AOP_DEC                   6
#define BRW_AOP_ADD                   7
#define BRW_AOP_SUB                   8
#define BRW_AOP_REVSUB                9
#define BRW_AOP_IMAX                  10
#define BRW_AOP_IMIN                  11
#define BRW_AOP_UMAX                  12
#define BRW_AOP_UMIN                  13
#define BRW_AOP_CMPWR                 14
#define BRW_AOP_PREDEC                15

#define BRW_MATH_FUNCTION_INV                              1
#define BRW_MATH_FUNCTION_LOG                              2
#define BRW_MATH_FUNCTION_EXP                              3
#define BRW_MATH_FUNCTION_SQRT                             4
#define BRW_MATH_FUNCTION_RSQ                              5
#define BRW_MATH_FUNCTION_SIN                              6
#define BRW_MATH_FUNCTION_COS                              7
#define BRW_MATH_FUNCTION_SINCOS                           8 /* gen4, gen5 */
#define BRW_MATH_FUNCTION_FDIV                             9 /* gen6+ */
#define BRW_MATH_FUNCTION_POW                              10
#define BRW_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER   11
#define BRW_MATH_FUNCTION_INT_DIV_QUOTIENT                 12
#define BRW_MATH_FUNCTION_INT_DIV_REMAINDER                13
#define GEN8_MATH_FUNCTION_INVM                            14
#define GEN8_MATH_FUNCTION_RSQRTM                          15

#define BRW_MATH_INTEGER_UNSIGNED     0
#define BRW_MATH_INTEGER_SIGNED       1

#define BRW_MATH_PRECISION_FULL        0
#define BRW_MATH_PRECISION_PARTIAL     1

#define BRW_MATH_SATURATE_NONE         0
#define BRW_MATH_SATURATE_SATURATE     1

#define BRW_MATH_DATA_VECTOR  0
#define BRW_MATH_DATA_SCALAR  1

#define BRW_URB_OPCODE_WRITE_HWORD  0
#define BRW_URB_OPCODE_WRITE_OWORD  1
#define BRW_URB_OPCODE_READ_HWORD   2
#define BRW_URB_OPCODE_READ_OWORD   3
#define GEN7_URB_OPCODE_ATOMIC_MOV  4
#define GEN7_URB_OPCODE_ATOMIC_INC  5
#define GEN8_URB_OPCODE_ATOMIC_ADD  6
#define GEN8_URB_OPCODE_SIMD8_WRITE 7
#define GEN8_URB_OPCODE_SIMD8_READ  8

#define BRW_URB_SWIZZLE_NONE          0
#define BRW_URB_SWIZZLE_INTERLEAVE    1
#define BRW_URB_SWIZZLE_TRANSPOSE     2

#define BRW_SCRATCH_SPACE_SIZE_1K     0
#define BRW_SCRATCH_SPACE_SIZE_2K     1
#define BRW_SCRATCH_SPACE_SIZE_4K     2
#define BRW_SCRATCH_SPACE_SIZE_8K     3
#define BRW_SCRATCH_SPACE_SIZE_16K    4
#define BRW_SCRATCH_SPACE_SIZE_32K    5
#define BRW_SCRATCH_SPACE_SIZE_64K    6
#define BRW_SCRATCH_SPACE_SIZE_128K   7
#define BRW_SCRATCH_SPACE_SIZE_256K   8
#define BRW_SCRATCH_SPACE_SIZE_512K   9
#define BRW_SCRATCH_SPACE_SIZE_1M     10
#define BRW_SCRATCH_SPACE_SIZE_2M     11

#define BRW_MESSAGE_GATEWAY_SFID_OPEN_GATEWAY         0
#define BRW_MESSAGE_GATEWAY_SFID_CLOSE_GATEWAY        1
#define BRW_MESSAGE_GATEWAY_SFID_FORWARD_MSG          2
#define BRW_MESSAGE_GATEWAY_SFID_GET_TIMESTAMP        3
#define BRW_MESSAGE_GATEWAY_SFID_BARRIER_MSG          4
#define BRW_MESSAGE_GATEWAY_SFID_UPDATE_GATEWAY_STATE 5
#define BRW_MESSAGE_GATEWAY_SFID_MMIO_READ_WRITE      6


/* Gen7 "GS URB Entry Allocation Size" is a U9-1 field, so the maximum gs_size
 * is 2^9, or 512.  It's counted in multiples of 64 bytes.
 *
 * Identical for VS, DS, and HS.
 */
#define GEN7_MAX_GS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GEN7_MAX_DS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GEN7_MAX_HS_URB_ENTRY_SIZE_BYTES                (512*64)
#define GEN7_MAX_VS_URB_ENTRY_SIZE_BYTES                (512*64)

/* Gen6 "GS URB Entry Allocation Size" is defined as a number of 1024-bit
 * (128 bytes) URB rows and the maximum allowed value is 5 rows.
 */
#define GEN6_MAX_GS_URB_ENTRY_SIZE_BYTES                (5*128)

/* GS Thread Payload
 */

/* 3DSTATE_GS "Output Vertex Size" has an effective maximum of 62. It's
 * counted in multiples of 16 bytes.
 */
#define GEN7_MAX_GS_OUTPUT_VERTEX_SIZE_BYTES            (62*16)


/* R0 */
# define GEN7_GS_PAYLOAD_INSTANCE_ID_SHIFT		27

#endif /* BRW_EU_DEFINES_H */
