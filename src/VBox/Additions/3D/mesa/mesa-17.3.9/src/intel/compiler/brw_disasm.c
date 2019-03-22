/*
 * Copyright © 2008 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "brw_eu_defines.h"
#include "brw_inst.h"
#include "brw_shader.h"
#include "brw_reg.h"
#include "brw_inst.h"
#include "brw_eu.h"
#include "util/half_float.h"

static bool
has_jip(const struct gen_device_info *devinfo, enum opcode opcode)
{
   if (devinfo->gen < 6)
      return false;

   return opcode == BRW_OPCODE_IF ||
          opcode == BRW_OPCODE_ELSE ||
          opcode == BRW_OPCODE_ENDIF ||
          opcode == BRW_OPCODE_WHILE ||
          opcode == BRW_OPCODE_BREAK ||
          opcode == BRW_OPCODE_CONTINUE ||
          opcode == BRW_OPCODE_HALT;
}

static bool
has_uip(const struct gen_device_info *devinfo, enum opcode opcode)
{
   if (devinfo->gen < 6)
      return false;

   return (devinfo->gen >= 7 && opcode == BRW_OPCODE_IF) ||
          (devinfo->gen >= 8 && opcode == BRW_OPCODE_ELSE) ||
          opcode == BRW_OPCODE_BREAK ||
          opcode == BRW_OPCODE_CONTINUE ||
          opcode == BRW_OPCODE_HALT;
}

static bool
has_branch_ctrl(const struct gen_device_info *devinfo, enum opcode opcode)
{
   if (devinfo->gen < 8)
      return false;

   return opcode == BRW_OPCODE_IF ||
          opcode == BRW_OPCODE_ELSE;
          /* opcode == BRW_OPCODE_GOTO; */
}

static bool
is_logic_instruction(unsigned opcode)
{
   return opcode == BRW_OPCODE_AND ||
          opcode == BRW_OPCODE_NOT ||
          opcode == BRW_OPCODE_OR ||
          opcode == BRW_OPCODE_XOR;
}

const char *const conditional_modifier[16] = {
   [BRW_CONDITIONAL_NONE] = "",
   [BRW_CONDITIONAL_Z]    = ".z",
   [BRW_CONDITIONAL_NZ]   = ".nz",
   [BRW_CONDITIONAL_G]    = ".g",
   [BRW_CONDITIONAL_GE]   = ".ge",
   [BRW_CONDITIONAL_L]    = ".l",
   [BRW_CONDITIONAL_LE]   = ".le",
   [BRW_CONDITIONAL_R]    = ".r",
   [BRW_CONDITIONAL_O]    = ".o",
   [BRW_CONDITIONAL_U]    = ".u",
};

static const char *const m_negate[2] = {
   [0] = "",
   [1] = "-",
};

static const char *const _abs[2] = {
   [0] = "",
   [1] = "(abs)",
};

static const char *const m_bitnot[2] = { "", "~" };

static const char *const vert_stride[16] = {
   [0] = "0",
   [1] = "1",
   [2] = "2",
   [3] = "4",
   [4] = "8",
   [5] = "16",
   [6] = "32",
   [15] = "VxH",
};

static const char *const width[8] = {
   [0] = "1",
   [1] = "2",
   [2] = "4",
   [3] = "8",
   [4] = "16",
};

static const char *const horiz_stride[4] = {
   [0] = "0",
   [1] = "1",
   [2] = "2",
   [3] = "4"
};

static const char *const chan_sel[4] = {
   [0] = "x",
   [1] = "y",
   [2] = "z",
   [3] = "w",
};

static const char *const debug_ctrl[2] = {
   [0] = "",
   [1] = ".breakpoint"
};

static const char *const saturate[2] = {
   [0] = "",
   [1] = ".sat"
};

static const char *const cmpt_ctrl[2] = {
   [0] = "",
   [1] = "compacted"
};

static const char *const accwr[2] = {
   [0] = "",
   [1] = "AccWrEnable"
};

static const char *const branch_ctrl[2] = {
   [0] = "",
   [1] = "BranchCtrl"
};

static const char *const wectrl[2] = {
   [0] = "",
   [1] = "WE_all"
};

static const char *const exec_size[8] = {
   [0] = "1",
   [1] = "2",
   [2] = "4",
   [3] = "8",
   [4] = "16",
   [5] = "32"
};

static const char *const pred_inv[2] = {
   [0] = "+",
   [1] = "-"
};

const char *const pred_ctrl_align16[16] = {
   [1] = "",
   [2] = ".x",
   [3] = ".y",
   [4] = ".z",
   [5] = ".w",
   [6] = ".any4h",
   [7] = ".all4h",
};

static const char *const pred_ctrl_align1[16] = {
   [BRW_PREDICATE_NORMAL]        = "",
   [BRW_PREDICATE_ALIGN1_ANYV]   = ".anyv",
   [BRW_PREDICATE_ALIGN1_ALLV]   = ".allv",
   [BRW_PREDICATE_ALIGN1_ANY2H]  = ".any2h",
   [BRW_PREDICATE_ALIGN1_ALL2H]  = ".all2h",
   [BRW_PREDICATE_ALIGN1_ANY4H]  = ".any4h",
   [BRW_PREDICATE_ALIGN1_ALL4H]  = ".all4h",
   [BRW_PREDICATE_ALIGN1_ANY8H]  = ".any8h",
   [BRW_PREDICATE_ALIGN1_ALL8H]  = ".all8h",
   [BRW_PREDICATE_ALIGN1_ANY16H] = ".any16h",
   [BRW_PREDICATE_ALIGN1_ALL16H] = ".all16h",
   [BRW_PREDICATE_ALIGN1_ANY32H] = ".any32h",
   [BRW_PREDICATE_ALIGN1_ALL32H] = ".all32h",
};

static const char *const thread_ctrl[4] = {
   [BRW_THREAD_NORMAL] = "",
   [BRW_THREAD_ATOMIC] = "atomic",
   [BRW_THREAD_SWITCH] = "switch",
};

static const char *const compr_ctrl[4] = {
   [0] = "",
   [1] = "sechalf",
   [2] = "compr",
   [3] = "compr4",
};

static const char *const dep_ctrl[4] = {
   [0] = "",
   [1] = "NoDDClr",
   [2] = "NoDDChk",
   [3] = "NoDDClr,NoDDChk",
};

static const char *const mask_ctrl[4] = {
   [0] = "",
   [1] = "nomask",
};

static const char *const access_mode[2] = {
   [0] = "align1",
   [1] = "align16",
};

static const char *const reg_file[4] = {
   [0] = "A",
   [1] = "g",
   [2] = "m",
   [3] = "imm",
};

static const char *const writemask[16] = {
   [0x0] = ".",
   [0x1] = ".x",
   [0x2] = ".y",
   [0x3] = ".xy",
   [0x4] = ".z",
   [0x5] = ".xz",
   [0x6] = ".yz",
   [0x7] = ".xyz",
   [0x8] = ".w",
   [0x9] = ".xw",
   [0xa] = ".yw",
   [0xb] = ".xyw",
   [0xc] = ".zw",
   [0xd] = ".xzw",
   [0xe] = ".yzw",
   [0xf] = "",
};

static const char *const end_of_thread[2] = {
   [0] = "",
   [1] = "EOT"
};

/* SFIDs on Gen4-5 */
static const char *const gen4_sfid[16] = {
   [BRW_SFID_NULL]            = "null",
   [BRW_SFID_MATH]            = "math",
   [BRW_SFID_SAMPLER]         = "sampler",
   [BRW_SFID_MESSAGE_GATEWAY] = "gateway",
   [BRW_SFID_DATAPORT_READ]   = "read",
   [BRW_SFID_DATAPORT_WRITE]  = "write",
   [BRW_SFID_URB]             = "urb",
   [BRW_SFID_THREAD_SPAWNER]  = "thread_spawner",
   [BRW_SFID_VME]             = "vme",
};

static const char *const gen6_sfid[16] = {
   [BRW_SFID_NULL]                     = "null",
   [BRW_SFID_MATH]                     = "math",
   [BRW_SFID_SAMPLER]                  = "sampler",
   [BRW_SFID_MESSAGE_GATEWAY]          = "gateway",
   [BRW_SFID_URB]                      = "urb",
   [BRW_SFID_THREAD_SPAWNER]           = "thread_spawner",
   [GEN6_SFID_DATAPORT_SAMPLER_CACHE]  = "sampler",
   [GEN6_SFID_DATAPORT_RENDER_CACHE]   = "render",
   [GEN6_SFID_DATAPORT_CONSTANT_CACHE] = "const",
   [GEN7_SFID_DATAPORT_DATA_CACHE]     = "data",
   [GEN7_SFID_PIXEL_INTERPOLATOR]      = "pixel interp",
   [HSW_SFID_DATAPORT_DATA_CACHE_1]    = "dp data 1",
   [HSW_SFID_CRE]                      = "cre",
};

static const char *const gen7_gateway_subfuncid[8] = {
   [BRW_MESSAGE_GATEWAY_SFID_OPEN_GATEWAY] = "open",
   [BRW_MESSAGE_GATEWAY_SFID_CLOSE_GATEWAY] = "close",
   [BRW_MESSAGE_GATEWAY_SFID_FORWARD_MSG] = "forward msg",
   [BRW_MESSAGE_GATEWAY_SFID_GET_TIMESTAMP] = "get timestamp",
   [BRW_MESSAGE_GATEWAY_SFID_BARRIER_MSG] = "barrier msg",
   [BRW_MESSAGE_GATEWAY_SFID_UPDATE_GATEWAY_STATE] = "update state",
   [BRW_MESSAGE_GATEWAY_SFID_MMIO_READ_WRITE] = "mmio read/write",
};

static const char *const gen4_dp_read_port_msg_type[4] = {
   [0b00] = "OWord Block Read",
   [0b01] = "OWord Dual Block Read",
   [0b10] = "Media Block Read",
   [0b11] = "DWord Scattered Read",
};

static const char *const g45_dp_read_port_msg_type[8] = {
   [0b000] = "OWord Block Read",
   [0b010] = "OWord Dual Block Read",
   [0b100] = "Media Block Read",
   [0b110] = "DWord Scattered Read",
   [0b001] = "Render Target UNORM Read",
   [0b011] = "AVC Loop Filter Read",
};

static const char *const dp_write_port_msg_type[8] = {
   [0b000] = "OWord block write",
   [0b001] = "OWord dual block write",
   [0b010] = "media block write",
   [0b011] = "DWord scattered write",
   [0b100] = "RT write",
   [0b101] = "streamed VB write",
   [0b110] = "RT UNORM write", /* G45+ */
   [0b111] = "flush render cache",
};

static const char *const dp_rc_msg_type_gen6[16] = {
   [BRW_DATAPORT_READ_MESSAGE_OWORD_BLOCK_READ] = "OWORD block read",
   [GEN6_DATAPORT_READ_MESSAGE_RENDER_UNORM_READ] = "RT UNORM read",
   [GEN6_DATAPORT_READ_MESSAGE_OWORD_DUAL_BLOCK_READ] = "OWORD dual block read",
   [GEN6_DATAPORT_READ_MESSAGE_MEDIA_BLOCK_READ] = "media block read",
   [GEN6_DATAPORT_READ_MESSAGE_OWORD_UNALIGN_BLOCK_READ] =
      "OWORD unaligned block read",
   [GEN6_DATAPORT_READ_MESSAGE_DWORD_SCATTERED_READ] = "DWORD scattered read",
   [GEN6_DATAPORT_WRITE_MESSAGE_DWORD_ATOMIC_WRITE] = "DWORD atomic write",
   [GEN6_DATAPORT_WRITE_MESSAGE_OWORD_BLOCK_WRITE] = "OWORD block write",
   [GEN6_DATAPORT_WRITE_MESSAGE_OWORD_DUAL_BLOCK_WRITE] =
      "OWORD dual block write",
   [GEN6_DATAPORT_WRITE_MESSAGE_MEDIA_BLOCK_WRITE] = "media block write",
   [GEN6_DATAPORT_WRITE_MESSAGE_DWORD_SCATTERED_WRITE] =
      "DWORD scattered write",
   [GEN6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE] = "RT write",
   [GEN6_DATAPORT_WRITE_MESSAGE_STREAMED_VB_WRITE] = "streamed VB write",
   [GEN6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_UNORM_WRITE] = "RT UNORM write",
};

static const char *const dp_rc_msg_type_gen7[16] = {
   [GEN7_DATAPORT_RC_MEDIA_BLOCK_READ] = "media block read",
   [GEN7_DATAPORT_RC_TYPED_SURFACE_READ] = "typed surface read",
   [GEN7_DATAPORT_RC_TYPED_ATOMIC_OP] = "typed atomic op",
   [GEN7_DATAPORT_RC_MEMORY_FENCE] = "memory fence",
   [GEN7_DATAPORT_RC_MEDIA_BLOCK_WRITE] = "media block write",
   [GEN7_DATAPORT_RC_RENDER_TARGET_WRITE] = "RT write",
   [GEN7_DATAPORT_RC_TYPED_SURFACE_WRITE] = "typed surface write"
};

static const char *const dp_rc_msg_type_gen9[16] = {
   [GEN9_DATAPORT_RC_RENDER_TARGET_WRITE] = "RT write",
   [GEN9_DATAPORT_RC_RENDER_TARGET_READ] = "RT read"
};

static const char *const *
dp_rc_msg_type(const struct gen_device_info *devinfo)
{
   return (devinfo->gen >= 9 ? dp_rc_msg_type_gen9 :
           devinfo->gen >= 7 ? dp_rc_msg_type_gen7 :
           devinfo->gen >= 6 ? dp_rc_msg_type_gen6 :
           dp_write_port_msg_type);
}

static const char *const m_rt_write_subtype[] = {
   [0b000] = "SIMD16",
   [0b001] = "SIMD16/RepData",
   [0b010] = "SIMD8/DualSrcLow",
   [0b011] = "SIMD8/DualSrcHigh",
   [0b100] = "SIMD8",
   [0b101] = "SIMD8/ImageWrite",   /* Gen6+ */
   [0b111] = "SIMD16/RepData-111", /* no idea how this is different than 1 */
};

static const char *const dp_dc0_msg_type_gen7[16] = {
   [GEN7_DATAPORT_DC_OWORD_BLOCK_READ] = "DC OWORD block read",
   [GEN7_DATAPORT_DC_UNALIGNED_OWORD_BLOCK_READ] =
      "DC unaligned OWORD block read",
   [GEN7_DATAPORT_DC_OWORD_DUAL_BLOCK_READ] = "DC OWORD dual block read",
   [GEN7_DATAPORT_DC_DWORD_SCATTERED_READ] = "DC DWORD scattered read",
   [GEN7_DATAPORT_DC_BYTE_SCATTERED_READ] = "DC byte scattered read",
   [GEN7_DATAPORT_DC_UNTYPED_SURFACE_READ] = "DC untyped surface read",
   [GEN7_DATAPORT_DC_UNTYPED_ATOMIC_OP] = "DC untyped atomic",
   [GEN7_DATAPORT_DC_MEMORY_FENCE] = "DC mfence",
   [GEN7_DATAPORT_DC_OWORD_BLOCK_WRITE] = "DC OWORD block write",
   [GEN7_DATAPORT_DC_OWORD_DUAL_BLOCK_WRITE] = "DC OWORD dual block write",
   [GEN7_DATAPORT_DC_DWORD_SCATTERED_WRITE] = "DC DWORD scatterd write",
   [GEN7_DATAPORT_DC_BYTE_SCATTERED_WRITE] = "DC byte scattered write",
   [GEN7_DATAPORT_DC_UNTYPED_SURFACE_WRITE] = "DC untyped surface write",
};

static const char *const dp_dc1_msg_type_hsw[16] = {
   [HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ] = "untyped surface read",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP] = "DC untyped atomic op",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2] =
      "DC untyped 4x2 atomic op",
   [HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_READ] = "DC media block read",
   [HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ] = "DC typed surface read",
   [HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP] = "DC typed atomic",
   [HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2] = "DC typed 4x2 atomic op",
   [HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE] = "DC untyped surface write",
   [HSW_DATAPORT_DC_PORT1_MEDIA_BLOCK_WRITE] = "DC media block write",
   [HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP] = "DC atomic counter op",
   [HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2] =
      "DC 4x2 atomic counter op",
   [HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE] = "DC typed surface write",
};

static const char *const aop[16] = {
   [BRW_AOP_AND]    = "and",
   [BRW_AOP_OR]     = "or",
   [BRW_AOP_XOR]    = "xor",
   [BRW_AOP_MOV]    = "mov",
   [BRW_AOP_INC]    = "inc",
   [BRW_AOP_DEC]    = "dec",
   [BRW_AOP_ADD]    = "add",
   [BRW_AOP_SUB]    = "sub",
   [BRW_AOP_REVSUB] = "revsub",
   [BRW_AOP_IMAX]   = "imax",
   [BRW_AOP_IMIN]   = "imin",
   [BRW_AOP_UMAX]   = "umax",
   [BRW_AOP_UMIN]   = "umin",
   [BRW_AOP_CMPWR]  = "cmpwr",
   [BRW_AOP_PREDEC] = "predec",
};

static const char * const pixel_interpolator_msg_types[4] = {
    [GEN7_PIXEL_INTERPOLATOR_LOC_SHARED_OFFSET] = "per_message_offset",
    [GEN7_PIXEL_INTERPOLATOR_LOC_SAMPLE] = "sample_position",
    [GEN7_PIXEL_INTERPOLATOR_LOC_CENTROID] = "centroid",
    [GEN7_PIXEL_INTERPOLATOR_LOC_PER_SLOT_OFFSET] = "per_slot_offset",
};

static const char *const math_function[16] = {
   [BRW_MATH_FUNCTION_INV]    = "inv",
   [BRW_MATH_FUNCTION_LOG]    = "log",
   [BRW_MATH_FUNCTION_EXP]    = "exp",
   [BRW_MATH_FUNCTION_SQRT]   = "sqrt",
   [BRW_MATH_FUNCTION_RSQ]    = "rsq",
   [BRW_MATH_FUNCTION_SIN]    = "sin",
   [BRW_MATH_FUNCTION_COS]    = "cos",
   [BRW_MATH_FUNCTION_SINCOS] = "sincos",
   [BRW_MATH_FUNCTION_FDIV]   = "fdiv",
   [BRW_MATH_FUNCTION_POW]    = "pow",
   [BRW_MATH_FUNCTION_INT_DIV_QUOTIENT_AND_REMAINDER] = "intdivmod",
   [BRW_MATH_FUNCTION_INT_DIV_QUOTIENT]  = "intdiv",
   [BRW_MATH_FUNCTION_INT_DIV_REMAINDER] = "intmod",
   [GEN8_MATH_FUNCTION_INVM]  = "invm",
   [GEN8_MATH_FUNCTION_RSQRTM] = "rsqrtm",
};

static const char *const math_saturate[2] = {
   [0] = "",
   [1] = "sat"
};

static const char *const math_signed[2] = {
   [0] = "",
   [1] = "signed"
};

static const char *const math_scalar[2] = {
   [0] = "",
   [1] = "scalar"
};

static const char *const math_precision[2] = {
   [0] = "",
   [1] = "partial_precision"
};

static const char *const gen5_urb_opcode[] = {
   [0] = "urb_write",
   [1] = "ff_sync",
};

static const char *const gen7_urb_opcode[] = {
   [BRW_URB_OPCODE_WRITE_HWORD] = "write HWord",
   [BRW_URB_OPCODE_WRITE_OWORD] = "write OWord",
   [BRW_URB_OPCODE_READ_HWORD] = "read HWord",
   [BRW_URB_OPCODE_READ_OWORD] = "read OWord",
   [GEN7_URB_OPCODE_ATOMIC_MOV] = "atomic mov",  /* Gen7+ */
   [GEN7_URB_OPCODE_ATOMIC_INC] = "atomic inc",  /* Gen7+ */
   [GEN8_URB_OPCODE_ATOMIC_ADD] = "atomic add",  /* Gen8+ */
   [GEN8_URB_OPCODE_SIMD8_WRITE] = "SIMD8 write", /* Gen8+ */
   [GEN8_URB_OPCODE_SIMD8_READ] = "SIMD8 read",  /* Gen8+ */
   /* [9-15] - reserved */
};

static const char *const urb_swizzle[4] = {
   [BRW_URB_SWIZZLE_NONE]       = "",
   [BRW_URB_SWIZZLE_INTERLEAVE] = "interleave",
   [BRW_URB_SWIZZLE_TRANSPOSE]  = "transpose",
};

static const char *const urb_allocate[2] = {
   [0] = "",
   [1] = "allocate"
};

static const char *const urb_used[2] = {
   [0] = "",
   [1] = "used"
};

static const char *const urb_complete[2] = {
   [0] = "",
   [1] = "complete"
};

static const char *const gen5_sampler_msg_type[] = {
   [GEN5_SAMPLER_MESSAGE_SAMPLE]              = "sample",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_BIAS]         = "sample_b",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_LOD]          = "sample_l",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_COMPARE]      = "sample_c",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_DERIVS]       = "sample_d",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_BIAS_COMPARE] = "sample_b_c",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_LOD_COMPARE]  = "sample_l_c",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_LD]           = "ld",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4]      = "gather4",
   [GEN5_SAMPLER_MESSAGE_LOD]                 = "lod",
   [GEN5_SAMPLER_MESSAGE_SAMPLE_RESINFO]      = "resinfo",
   [GEN6_SAMPLER_MESSAGE_SAMPLE_SAMPLEINFO]   = "sampleinfo",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_C]    = "gather4_c",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO]   = "gather4_po",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_GATHER4_PO_C] = "gather4_po_c",
   [HSW_SAMPLER_MESSAGE_SAMPLE_DERIV_COMPARE] = "sample_d_c",
   [GEN9_SAMPLER_MESSAGE_SAMPLE_LZ]           = "sample_lz",
   [GEN9_SAMPLER_MESSAGE_SAMPLE_C_LZ]         = "sample_c_lz",
   [GEN9_SAMPLER_MESSAGE_SAMPLE_LD_LZ]        = "ld_lz",
   [GEN9_SAMPLER_MESSAGE_SAMPLE_LD2DMS_W]     = "ld2dms_w",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_LD_MCS]       = "ld_mcs",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_LD2DMS]       = "ld2dms",
   [GEN7_SAMPLER_MESSAGE_SAMPLE_LD2DSS]       = "ld2dss",
};

static const char *const gen5_sampler_simd_mode[4] = {
   [BRW_SAMPLER_SIMD_MODE_SIMD4X2]   = "SIMD4x2",
   [BRW_SAMPLER_SIMD_MODE_SIMD8]     = "SIMD8",
   [BRW_SAMPLER_SIMD_MODE_SIMD16]    = "SIMD16",
   [BRW_SAMPLER_SIMD_MODE_SIMD32_64] = "SIMD32/64",
};

static const char *const sampler_target_format[4] = {
   [0] = "F",
   [2] = "UD",
   [3] = "D"
};


static int column;

static int
string(FILE *file, const char *string)
{
   fputs(string, file);
   column += strlen(string);
   return 0;
}

static int
format(FILE *f, const char *format, ...) PRINTFLIKE(2, 3);

static int
format(FILE *f, const char *format, ...)
{
   char buf[1024];
   va_list args;
   va_start(args, format);

   vsnprintf(buf, sizeof(buf) - 1, format, args);
   va_end(args);
   string(f, buf);
   return 0;
}

static int
newline(FILE *f)
{
   putc('\n', f);
   column = 0;
   return 0;
}

static int
pad(FILE *f, int c)
{
   do
      string(f, " ");
   while (column < c);
   return 0;
}

static int
control(FILE *file, const char *name, const char *const ctrl[],
        unsigned id, int *space)
{
   if (!ctrl[id]) {
      fprintf(file, "*** invalid %s value %d ", name, id);
      return 1;
   }
   if (ctrl[id][0]) {
      if (space && *space)
         string(file, " ");
      string(file, ctrl[id]);
      if (space)
         *space = 1;
   }
   return 0;
}

static int
print_opcode(FILE *file, const struct gen_device_info *devinfo,
             enum opcode id)
{
   const struct opcode_desc *desc = brw_opcode_desc(devinfo, id);
   if (!desc) {
      format(file, "*** invalid opcode value %d ", id);
      return 1;
   }
   string(file, desc->name);
   return 0;
}

static int
reg(FILE *file, unsigned _reg_file, unsigned _reg_nr)
{
   int err = 0;

   /* Clear the Compr4 instruction compression bit. */
   if (_reg_file == BRW_MESSAGE_REGISTER_FILE)
      _reg_nr &= ~BRW_MRF_COMPR4;

   if (_reg_file == BRW_ARCHITECTURE_REGISTER_FILE) {
      switch (_reg_nr & 0xf0) {
      case BRW_ARF_NULL:
         string(file, "null");
         break;
      case BRW_ARF_ADDRESS:
         format(file, "a%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_ACCUMULATOR:
         format(file, "acc%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_FLAG:
         format(file, "f%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_MASK:
         format(file, "mask%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_MASK_STACK:
         format(file, "msd%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_STATE:
         format(file, "sr%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_CONTROL:
         format(file, "cr%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_NOTIFICATION_COUNT:
         format(file, "n%d", _reg_nr & 0x0f);
         break;
      case BRW_ARF_IP:
         string(file, "ip");
         return -1;
         break;
      case BRW_ARF_TDR:
         format(file, "tdr0");
         return -1;
      case BRW_ARF_TIMESTAMP:
         format(file, "tm%d", _reg_nr & 0x0f);
         break;
      default:
         format(file, "ARF%d", _reg_nr);
         break;
      }
   } else {
      err |= control(file, "src reg file", reg_file, _reg_file, NULL);
      format(file, "%d", _reg_nr);
   }
   return err;
}

static int
dest(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   enum brw_reg_type type = brw_inst_dst_type(devinfo, inst);
   unsigned elem_size = brw_reg_type_to_size(type);
   int err = 0;

   if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
      if (brw_inst_dst_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         err |= reg(file, brw_inst_dst_reg_file(devinfo, inst),
                    brw_inst_dst_da_reg_nr(devinfo, inst));
         if (err == -1)
            return 0;
         if (brw_inst_dst_da1_subreg_nr(devinfo, inst))
            format(file, ".%"PRIu64, brw_inst_dst_da1_subreg_nr(devinfo, inst) /
                   elem_size);
         string(file, "<");
         err |= control(file, "horiz stride", horiz_stride,
                        brw_inst_dst_hstride(devinfo, inst), NULL);
         string(file, ">");
         string(file, brw_reg_type_to_letters(type));
      } else {
         string(file, "g[a0");
         if (brw_inst_dst_ia_subreg_nr(devinfo, inst))
            format(file, ".%"PRIu64, brw_inst_dst_ia_subreg_nr(devinfo, inst) /
                   elem_size);
         if (brw_inst_dst_ia1_addr_imm(devinfo, inst))
            format(file, " %d", brw_inst_dst_ia1_addr_imm(devinfo, inst));
         string(file, "]<");
         err |= control(file, "horiz stride", horiz_stride,
                        brw_inst_dst_hstride(devinfo, inst), NULL);
         string(file, ">");
         string(file, brw_reg_type_to_letters(type));
      }
   } else {
      if (brw_inst_dst_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         err |= reg(file, brw_inst_dst_reg_file(devinfo, inst),
                    brw_inst_dst_da_reg_nr(devinfo, inst));
         if (err == -1)
            return 0;
         if (brw_inst_dst_da16_subreg_nr(devinfo, inst))
            format(file, ".%u", 16 / elem_size);
         string(file, "<1>");
         err |= control(file, "writemask", writemask,
                        brw_inst_da16_writemask(devinfo, inst), NULL);
         string(file, brw_reg_type_to_letters(type));
      } else {
         err = 1;
         string(file, "Indirect align16 address mode not supported");
      }
   }

   return 0;
}

static int
dest_3src(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   bool is_align1 = brw_inst_3src_access_mode(devinfo, inst) == BRW_ALIGN_1;
   int err = 0;
   uint32_t reg_file;
   unsigned subreg_nr;
   enum brw_reg_type type;

   if (is_align1 && brw_inst_3src_a1_dst_reg_file(devinfo, inst))
      reg_file = BRW_ARCHITECTURE_REGISTER_FILE;
   else if (devinfo->gen == 6 && brw_inst_3src_a16_dst_reg_file(devinfo, inst))
      reg_file = BRW_MESSAGE_REGISTER_FILE;
   else
      reg_file = BRW_GENERAL_REGISTER_FILE;

   err |= reg(file, reg_file, brw_inst_3src_dst_reg_nr(devinfo, inst));
   if (err == -1)
      return 0;

   if (is_align1) {
      type = brw_inst_3src_a1_dst_type(devinfo, inst);
      subreg_nr = brw_inst_3src_a1_dst_subreg_nr(devinfo, inst);
   } else {
      type = brw_inst_3src_a16_dst_type(devinfo, inst);
      subreg_nr = brw_inst_3src_a16_dst_subreg_nr(devinfo, inst) * 4;
   }
   subreg_nr /= brw_reg_type_to_size(type);

   if (subreg_nr)
      format(file, ".%u", subreg_nr);
   string(file, "<1>");

   if (!is_align1) {
      err |= control(file, "writemask", writemask,
                     brw_inst_3src_a16_dst_writemask(devinfo, inst), NULL);
   }
   string(file, brw_reg_type_to_letters(type));

   return 0;
}

static int
src_align1_region(FILE *file,
                  unsigned _vert_stride, unsigned _width,
                  unsigned _horiz_stride)
{
   int err = 0;
   string(file, "<");
   err |= control(file, "vert stride", vert_stride, _vert_stride, NULL);
   string(file, ",");
   err |= control(file, "width", width, _width, NULL);
   string(file, ",");
   err |= control(file, "horiz_stride", horiz_stride, _horiz_stride, NULL);
   string(file, ">");
   return err;
}

static int
src_da1(FILE *file,
        const struct gen_device_info *devinfo,
        unsigned opcode,
        enum brw_reg_type type, unsigned _reg_file,
        unsigned _vert_stride, unsigned _width, unsigned _horiz_stride,
        unsigned reg_num, unsigned sub_reg_num, unsigned __abs,
        unsigned _negate)
{
   int err = 0;

   if (devinfo->gen >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   err |= reg(file, _reg_file, reg_num);
   if (err == -1)
      return 0;
   if (sub_reg_num) {
      unsigned elem_size = brw_reg_type_to_size(type);
      format(file, ".%d", sub_reg_num / elem_size);   /* use formal style like spec */
   }
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static int
src_ia1(FILE *file,
        const struct gen_device_info *devinfo,
        unsigned opcode,
        enum brw_reg_type type,
        unsigned _reg_file,
        int _addr_imm,
        unsigned _addr_subreg_nr,
        unsigned _negate,
        unsigned __abs,
        unsigned _horiz_stride, unsigned _width, unsigned _vert_stride)
{
   int err = 0;

   if (devinfo->gen >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   string(file, "g[a0");
   if (_addr_subreg_nr)
      format(file, ".%d", _addr_subreg_nr);
   if (_addr_imm)
      format(file, " %d", _addr_imm);
   string(file, "]");
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static int
src_swizzle(FILE *file, unsigned swiz)
{
   unsigned x = BRW_GET_SWZ(swiz, BRW_CHANNEL_X);
   unsigned y = BRW_GET_SWZ(swiz, BRW_CHANNEL_Y);
   unsigned z = BRW_GET_SWZ(swiz, BRW_CHANNEL_Z);
   unsigned w = BRW_GET_SWZ(swiz, BRW_CHANNEL_W);
   int err = 0;

   if (x == y && x == z && x == w) {
      string(file, ".");
      err |= control(file, "channel select", chan_sel, x, NULL);
   } else if (swiz != BRW_SWIZZLE_XYZW) {
      string(file, ".");
      err |= control(file, "channel select", chan_sel, x, NULL);
      err |= control(file, "channel select", chan_sel, y, NULL);
      err |= control(file, "channel select", chan_sel, z, NULL);
      err |= control(file, "channel select", chan_sel, w, NULL);
   }
   return err;
}

static int
src_da16(FILE *file,
         const struct gen_device_info *devinfo,
         unsigned opcode,
         enum brw_reg_type type,
         unsigned _reg_file,
         unsigned _vert_stride,
         unsigned _reg_nr,
         unsigned _subreg_nr,
         unsigned __abs,
         unsigned _negate,
         unsigned swz_x, unsigned swz_y, unsigned swz_z, unsigned swz_w)
{
   int err = 0;

   if (devinfo->gen >= 8 && is_logic_instruction(opcode))
      err |= control(file, "bitnot", m_bitnot, _negate, NULL);
   else
      err |= control(file, "negate", m_negate, _negate, NULL);

   err |= control(file, "abs", _abs, __abs, NULL);

   err |= reg(file, _reg_file, _reg_nr);
   if (err == -1)
      return 0;
   if (_subreg_nr) {
      unsigned elem_size = brw_reg_type_to_size(type);

      /* bit4 for subreg number byte addressing. Make this same meaning as
         in da1 case, so output looks consistent. */
      format(file, ".%d", 16 / elem_size);
   }
   string(file, "<");
   err |= control(file, "vert stride", vert_stride, _vert_stride, NULL);
   string(file, ">");
   err |= src_swizzle(file, BRW_SWIZZLE4(swz_x, swz_y, swz_z, swz_w));
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static enum brw_vertical_stride
vstride_from_align1_3src_vstride(enum gen10_align1_3src_vertical_stride vstride)
{
   switch (vstride) {
   case BRW_ALIGN1_3SRC_VERTICAL_STRIDE_0: return BRW_VERTICAL_STRIDE_0;
   case BRW_ALIGN1_3SRC_VERTICAL_STRIDE_2: return BRW_VERTICAL_STRIDE_2;
   case BRW_ALIGN1_3SRC_VERTICAL_STRIDE_4: return BRW_VERTICAL_STRIDE_4;
   case BRW_ALIGN1_3SRC_VERTICAL_STRIDE_8: return BRW_VERTICAL_STRIDE_8;
   default:
      unreachable("not reached");
   }
}

static enum brw_horizontal_stride
hstride_from_align1_3src_hstride(enum gen10_align1_3src_src_horizontal_stride hstride)
{
   switch (hstride) {
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0: return BRW_HORIZONTAL_STRIDE_0;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1: return BRW_HORIZONTAL_STRIDE_1;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2: return BRW_HORIZONTAL_STRIDE_2;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4: return BRW_HORIZONTAL_STRIDE_4;
   default:
      unreachable("not reached");
   }
}

static enum brw_vertical_stride
vstride_from_align1_3src_hstride(enum gen10_align1_3src_src_horizontal_stride hstride)
{
   switch (hstride) {
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_0: return BRW_VERTICAL_STRIDE_0;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_1: return BRW_VERTICAL_STRIDE_1;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_2: return BRW_VERTICAL_STRIDE_2;
   case BRW_ALIGN1_3SRC_SRC_HORIZONTAL_STRIDE_4: return BRW_VERTICAL_STRIDE_4;
   default:
      unreachable("not reached");
   }
}

/* From "GEN10 Regioning Rules for Align1 Ternary Operations" in the
 * "Register Region Restrictions" documentation
 */
static enum brw_width
implied_width(enum brw_vertical_stride _vert_stride,
              enum brw_horizontal_stride _horiz_stride)
{
   /* "1. Width is 1 when Vertical and Horizontal Strides are both zero." */
   if (_vert_stride == BRW_VERTICAL_STRIDE_0 &&
       _horiz_stride == BRW_HORIZONTAL_STRIDE_0) {
      return BRW_WIDTH_1;

   /* "2. Width is equal to vertical stride when Horizontal Stride is zero." */
   } else if (_horiz_stride == BRW_HORIZONTAL_STRIDE_0) {
      switch (_vert_stride) {
      case BRW_VERTICAL_STRIDE_2: return BRW_WIDTH_2;
      case BRW_VERTICAL_STRIDE_4: return BRW_WIDTH_4;
      case BRW_VERTICAL_STRIDE_8: return BRW_WIDTH_8;
      case BRW_VERTICAL_STRIDE_0:
      default:
         unreachable("not reached");
      }

   } else {
      /* FINISHME: Implement these: */

      /* "3. Width is equal to Vertical Stride/Horizontal Stride when both
       *     Strides are non-zero.
       *
       *  4. Vertical Stride must not be zero if Horizontal Stride is non-zero.
       *     This implies Vertical Stride is always greater than Horizontal
       *     Stride."
       *
       * Given these statements and the knowledge that the stride and width
       * values are encoded in logarithmic form, we can perform the division
       * by just subtracting.
       */
      return _vert_stride - _horiz_stride;
   }
}

static int
src0_3src(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum brw_reg_file _file;
   enum brw_reg_type type;
   enum brw_vertical_stride _vert_stride;
   enum brw_width _width;
   enum brw_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = brw_inst_3src_access_mode(devinfo, inst) == BRW_ALIGN_1;

   if (is_align1) {
      if (brw_inst_3src_a1_src0_reg_file(devinfo, inst) ==
          BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE) {
         _file = BRW_GENERAL_REGISTER_FILE;
         reg_nr = brw_inst_3src_src0_reg_nr(devinfo, inst);
         subreg_nr = brw_inst_3src_a1_src0_subreg_nr(devinfo, inst);
         type = brw_inst_3src_a1_src0_type(devinfo, inst);
      } else {
         _file = BRW_IMMEDIATE_VALUE;
         uint16_t imm_val = brw_inst_3src_a1_src0_imm(devinfo, inst);
         enum brw_reg_type type = brw_inst_3src_a1_src0_type(devinfo, inst);

         if (type == BRW_REGISTER_TYPE_W) {
            format(file, "%dW", imm_val);
         } else if (type == BRW_REGISTER_TYPE_UW) {
            format(file, "0x%04xUW", imm_val);
         } else if (type == BRW_REGISTER_TYPE_HF) {
            format(file, "%-gF", _mesa_half_to_float(imm_val));
         }
         return 0;
      }

      _vert_stride = vstride_from_align1_3src_vstride(
                        brw_inst_3src_a1_src0_vstride(devinfo, inst));
      _horiz_stride = hstride_from_align1_3src_hstride(
                         brw_inst_3src_a1_src0_hstride(devinfo, inst));
      _width = implied_width(_vert_stride, _horiz_stride);
   } else {
      _file = BRW_GENERAL_REGISTER_FILE;
      reg_nr = brw_inst_3src_src0_reg_nr(devinfo, inst);
      subreg_nr = brw_inst_3src_a16_src0_subreg_nr(devinfo, inst) * 4;
      type = brw_inst_3src_a16_src_type(devinfo, inst);

      if (brw_inst_3src_a16_src0_rep_ctrl(devinfo, inst)) {
         _vert_stride = BRW_VERTICAL_STRIDE_0;
         _width = BRW_WIDTH_1;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_0;
      } else {
         _vert_stride = BRW_VERTICAL_STRIDE_4;
         _width = BRW_WIDTH_4;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_1;
      }
   }
   is_scalar_region = _vert_stride == BRW_VERTICAL_STRIDE_0 &&
                      _width == BRW_WIDTH_1 &&
                      _horiz_stride == BRW_HORIZONTAL_STRIDE_0;

   subreg_nr /= brw_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  brw_inst_3src_src0_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, brw_inst_3src_src0_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, brw_inst_3src_a16_src0_swizzle(devinfo, inst));
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static int
src1_3src(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum brw_reg_file _file;
   enum brw_reg_type type;
   enum brw_vertical_stride _vert_stride;
   enum brw_width _width;
   enum brw_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = brw_inst_3src_access_mode(devinfo, inst) == BRW_ALIGN_1;

   if (is_align1) {
      if (brw_inst_3src_a1_src1_reg_file(devinfo, inst) ==
          BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE) {
         _file = BRW_GENERAL_REGISTER_FILE;
      } else {
         _file = BRW_ARCHITECTURE_REGISTER_FILE;
      }

      reg_nr = brw_inst_3src_src1_reg_nr(devinfo, inst);
      subreg_nr = brw_inst_3src_a1_src1_subreg_nr(devinfo, inst);
      type = brw_inst_3src_a1_src1_type(devinfo, inst);

      _vert_stride = vstride_from_align1_3src_vstride(
                        brw_inst_3src_a1_src1_vstride(devinfo, inst));
      _horiz_stride = hstride_from_align1_3src_hstride(
                         brw_inst_3src_a1_src1_hstride(devinfo, inst));
      _width = implied_width(_vert_stride, _horiz_stride);
   } else {
      _file = BRW_GENERAL_REGISTER_FILE;
      reg_nr = brw_inst_3src_src1_reg_nr(devinfo, inst);
      subreg_nr = brw_inst_3src_a16_src1_subreg_nr(devinfo, inst) * 4;
      type = brw_inst_3src_a16_src_type(devinfo, inst);

      if (brw_inst_3src_a16_src1_rep_ctrl(devinfo, inst)) {
         _vert_stride = BRW_VERTICAL_STRIDE_0;
         _width = BRW_WIDTH_1;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_0;
      } else {
         _vert_stride = BRW_VERTICAL_STRIDE_4;
         _width = BRW_WIDTH_4;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_1;
      }
   }
   is_scalar_region = _vert_stride == BRW_VERTICAL_STRIDE_0 &&
                      _width == BRW_WIDTH_1 &&
                      _horiz_stride == BRW_HORIZONTAL_STRIDE_0;

   subreg_nr /= brw_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  brw_inst_3src_src1_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, brw_inst_3src_src1_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, brw_inst_3src_a16_src1_swizzle(devinfo, inst));
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static int
src2_3src(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   int err = 0;
   unsigned reg_nr, subreg_nr;
   enum brw_reg_file _file;
   enum brw_reg_type type;
   enum brw_vertical_stride _vert_stride;
   enum brw_width _width;
   enum brw_horizontal_stride _horiz_stride;
   bool is_scalar_region;
   bool is_align1 = brw_inst_3src_access_mode(devinfo, inst) == BRW_ALIGN_1;

   if (is_align1) {
      if (brw_inst_3src_a1_src2_reg_file(devinfo, inst) ==
          BRW_ALIGN1_3SRC_GENERAL_REGISTER_FILE) {
         _file = BRW_GENERAL_REGISTER_FILE;
         reg_nr = brw_inst_3src_src2_reg_nr(devinfo, inst);
         subreg_nr = brw_inst_3src_a1_src2_subreg_nr(devinfo, inst);
         type = brw_inst_3src_a1_src2_type(devinfo, inst);
      } else {
         _file = BRW_IMMEDIATE_VALUE;
         uint16_t imm_val = brw_inst_3src_a1_src2_imm(devinfo, inst);
         enum brw_reg_type type = brw_inst_3src_a1_src2_type(devinfo, inst);

         if (type == BRW_REGISTER_TYPE_W) {
            format(file, "%dW", imm_val);
         } else if (type == BRW_REGISTER_TYPE_UW) {
            format(file, "0x%04xUW", imm_val);
         } else if (type == BRW_REGISTER_TYPE_HF) {
            format(file, "%-gF", _mesa_half_to_float(imm_val));
         }
         return 0;
      }

      /* FINISHME: No vertical stride on src2. Is using the hstride in place
       *           correct? Doesn't seem like it, since there's hstride=1 but
       *           no vstride=1.
       */
      _vert_stride = vstride_from_align1_3src_hstride(
                        brw_inst_3src_a1_src2_hstride(devinfo, inst));
      _horiz_stride = hstride_from_align1_3src_hstride(
                         brw_inst_3src_a1_src2_hstride(devinfo, inst));
      _width = implied_width(_vert_stride, _horiz_stride);
   } else {
      _file = BRW_GENERAL_REGISTER_FILE;
      reg_nr = brw_inst_3src_src2_reg_nr(devinfo, inst);
      subreg_nr = brw_inst_3src_a16_src2_subreg_nr(devinfo, inst) * 4;
      type = brw_inst_3src_a16_src_type(devinfo, inst);

      if (brw_inst_3src_a16_src2_rep_ctrl(devinfo, inst)) {
         _vert_stride = BRW_VERTICAL_STRIDE_0;
         _width = BRW_WIDTH_1;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_0;
      } else {
         _vert_stride = BRW_VERTICAL_STRIDE_4;
         _width = BRW_WIDTH_4;
         _horiz_stride = BRW_HORIZONTAL_STRIDE_1;
      }
   }
   is_scalar_region = _vert_stride == BRW_VERTICAL_STRIDE_0 &&
                      _width == BRW_WIDTH_1 &&
                      _horiz_stride == BRW_HORIZONTAL_STRIDE_0;

   subreg_nr /= brw_reg_type_to_size(type);

   err |= control(file, "negate", m_negate,
                  brw_inst_3src_src2_negate(devinfo, inst), NULL);
   err |= control(file, "abs", _abs, brw_inst_3src_src2_abs(devinfo, inst), NULL);

   err |= reg(file, _file, reg_nr);
   if (err == -1)
      return 0;
   if (subreg_nr || is_scalar_region)
      format(file, ".%d", subreg_nr);
   src_align1_region(file, _vert_stride, _width, _horiz_stride);
   if (!is_scalar_region && !is_align1)
      err |= src_swizzle(file, brw_inst_3src_a16_src2_swizzle(devinfo, inst));
   string(file, brw_reg_type_to_letters(type));
   return err;
}

static int
imm(FILE *file, const struct gen_device_info *devinfo, enum brw_reg_type type,
    const brw_inst *inst)
{
   switch (type) {
   case BRW_REGISTER_TYPE_UQ:
      format(file, "0x%016lxUQ", brw_inst_imm_uq(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_Q:
      format(file, "%ldQ", brw_inst_imm_uq(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_UD:
      format(file, "0x%08xUD", brw_inst_imm_ud(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_D:
      format(file, "%dD", brw_inst_imm_d(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_UW:
      format(file, "0x%04xUW", (uint16_t) brw_inst_imm_ud(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_W:
      format(file, "%dW", (int16_t) brw_inst_imm_d(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_UV:
      format(file, "0x%08xUV", brw_inst_imm_ud(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_VF:
      format(file, "[%-gF, %-gF, %-gF, %-gF]VF",
             brw_vf_to_float(brw_inst_imm_ud(devinfo, inst)),
             brw_vf_to_float(brw_inst_imm_ud(devinfo, inst) >> 8),
             brw_vf_to_float(brw_inst_imm_ud(devinfo, inst) >> 16),
             brw_vf_to_float(brw_inst_imm_ud(devinfo, inst) >> 24));
      break;
   case BRW_REGISTER_TYPE_V:
      format(file, "0x%08xV", brw_inst_imm_ud(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_F:
      format(file, "%-gF", brw_inst_imm_f(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_DF:
      format(file, "%-gDF", brw_inst_imm_df(devinfo, inst));
      break;
   case BRW_REGISTER_TYPE_HF:
      string(file, "Half Float IMM");
      break;
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
      format(file, "*** invalid immediate type %d ", type);
   }
   return 0;
}

static int
src0(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   if (brw_inst_src0_reg_file(devinfo, inst) == BRW_IMMEDIATE_VALUE) {
      return imm(file, devinfo, brw_inst_src0_type(devinfo, inst), inst);
   } else if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
      if (brw_inst_src0_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         return src_da1(file,
                        devinfo,
                        brw_inst_opcode(devinfo, inst),
                        brw_inst_src0_type(devinfo, inst),
                        brw_inst_src0_reg_file(devinfo, inst),
                        brw_inst_src0_vstride(devinfo, inst),
                        brw_inst_src0_width(devinfo, inst),
                        brw_inst_src0_hstride(devinfo, inst),
                        brw_inst_src0_da_reg_nr(devinfo, inst),
                        brw_inst_src0_da1_subreg_nr(devinfo, inst),
                        brw_inst_src0_abs(devinfo, inst),
                        brw_inst_src0_negate(devinfo, inst));
      } else {
         return src_ia1(file,
                        devinfo,
                        brw_inst_opcode(devinfo, inst),
                        brw_inst_src0_type(devinfo, inst),
                        brw_inst_src0_reg_file(devinfo, inst),
                        brw_inst_src0_ia1_addr_imm(devinfo, inst),
                        brw_inst_src0_ia_subreg_nr(devinfo, inst),
                        brw_inst_src0_negate(devinfo, inst),
                        brw_inst_src0_abs(devinfo, inst),
                        brw_inst_src0_hstride(devinfo, inst),
                        brw_inst_src0_width(devinfo, inst),
                        brw_inst_src0_vstride(devinfo, inst));
      }
   } else {
      if (brw_inst_src0_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         return src_da16(file,
                         devinfo,
                         brw_inst_opcode(devinfo, inst),
                         brw_inst_src0_type(devinfo, inst),
                         brw_inst_src0_reg_file(devinfo, inst),
                         brw_inst_src0_vstride(devinfo, inst),
                         brw_inst_src0_da_reg_nr(devinfo, inst),
                         brw_inst_src0_da16_subreg_nr(devinfo, inst),
                         brw_inst_src0_abs(devinfo, inst),
                         brw_inst_src0_negate(devinfo, inst),
                         brw_inst_src0_da16_swiz_x(devinfo, inst),
                         brw_inst_src0_da16_swiz_y(devinfo, inst),
                         brw_inst_src0_da16_swiz_z(devinfo, inst),
                         brw_inst_src0_da16_swiz_w(devinfo, inst));
      } else {
         string(file, "Indirect align16 address mode not supported");
         return 1;
      }
   }
}

static int
src1(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   if (brw_inst_src1_reg_file(devinfo, inst) == BRW_IMMEDIATE_VALUE) {
      return imm(file, devinfo, brw_inst_src1_type(devinfo, inst), inst);
   } else if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
      if (brw_inst_src1_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         return src_da1(file,
                        devinfo,
                        brw_inst_opcode(devinfo, inst),
                        brw_inst_src1_type(devinfo, inst),
                        brw_inst_src1_reg_file(devinfo, inst),
                        brw_inst_src1_vstride(devinfo, inst),
                        brw_inst_src1_width(devinfo, inst),
                        brw_inst_src1_hstride(devinfo, inst),
                        brw_inst_src1_da_reg_nr(devinfo, inst),
                        brw_inst_src1_da1_subreg_nr(devinfo, inst),
                        brw_inst_src1_abs(devinfo, inst),
                        brw_inst_src1_negate(devinfo, inst));
      } else {
         return src_ia1(file,
                        devinfo,
                        brw_inst_opcode(devinfo, inst),
                        brw_inst_src1_type(devinfo, inst),
                        brw_inst_src1_reg_file(devinfo, inst),
                        brw_inst_src1_ia1_addr_imm(devinfo, inst),
                        brw_inst_src1_ia_subreg_nr(devinfo, inst),
                        brw_inst_src1_negate(devinfo, inst),
                        brw_inst_src1_abs(devinfo, inst),
                        brw_inst_src1_hstride(devinfo, inst),
                        brw_inst_src1_width(devinfo, inst),
                        brw_inst_src1_vstride(devinfo, inst));
      }
   } else {
      if (brw_inst_src1_address_mode(devinfo, inst) == BRW_ADDRESS_DIRECT) {
         return src_da16(file,
                         devinfo,
                         brw_inst_opcode(devinfo, inst),
                         brw_inst_src1_type(devinfo, inst),
                         brw_inst_src1_reg_file(devinfo, inst),
                         brw_inst_src1_vstride(devinfo, inst),
                         brw_inst_src1_da_reg_nr(devinfo, inst),
                         brw_inst_src1_da16_subreg_nr(devinfo, inst),
                         brw_inst_src1_abs(devinfo, inst),
                         brw_inst_src1_negate(devinfo, inst),
                         brw_inst_src1_da16_swiz_x(devinfo, inst),
                         brw_inst_src1_da16_swiz_y(devinfo, inst),
                         brw_inst_src1_da16_swiz_z(devinfo, inst),
                         brw_inst_src1_da16_swiz_w(devinfo, inst));
      } else {
         string(file, "Indirect align16 address mode not supported");
         return 1;
      }
   }
}

static int
qtr_ctrl(FILE *file, const struct gen_device_info *devinfo, const brw_inst *inst)
{
   int qtr_ctl = brw_inst_qtr_control(devinfo, inst);
   int exec_size = 1 << brw_inst_exec_size(devinfo, inst);
   const unsigned nib_ctl = devinfo->gen < 7 ? 0 :
                            brw_inst_nib_control(devinfo, inst);

   if (exec_size < 8 || nib_ctl) {
      format(file, " %dN", qtr_ctl * 2 + nib_ctl + 1);
   } else if (exec_size == 8) {
      switch (qtr_ctl) {
      case 0:
         string(file, " 1Q");
         break;
      case 1:
         string(file, " 2Q");
         break;
      case 2:
         string(file, " 3Q");
         break;
      case 3:
         string(file, " 4Q");
         break;
      }
   } else if (exec_size == 16) {
      if (qtr_ctl < 2)
         string(file, " 1H");
      else
         string(file, " 2H");
   }
   return 0;
}

#ifdef DEBUG
static __attribute__((__unused__)) int
brw_disassemble_imm(const struct gen_device_info *devinfo,
                    uint32_t dw3, uint32_t dw2, uint32_t dw1, uint32_t dw0)
{
   brw_inst inst;
   inst.data[0] = (((uint64_t) dw1) << 32) | ((uint64_t) dw0);
   inst.data[1] = (((uint64_t) dw3) << 32) | ((uint64_t) dw2);
   return brw_disassemble_inst(stderr, devinfo, &inst, false);
}
#endif

int
brw_disassemble_inst(FILE *file, const struct gen_device_info *devinfo,
                     const brw_inst *inst, bool is_compacted)
{
   int err = 0;
   int space = 0;

   const enum opcode opcode = brw_inst_opcode(devinfo, inst);
   const struct opcode_desc *desc = brw_opcode_desc(devinfo, opcode);

   if (brw_inst_pred_control(devinfo, inst)) {
      string(file, "(");
      err |= control(file, "predicate inverse", pred_inv,
                     brw_inst_pred_inv(devinfo, inst), NULL);
      format(file, "f%"PRIu64, devinfo->gen >= 7 ? brw_inst_flag_reg_nr(devinfo, inst) : 0);
      if (brw_inst_flag_subreg_nr(devinfo, inst))
         format(file, ".%"PRIu64, brw_inst_flag_subreg_nr(devinfo, inst));
      if (brw_inst_access_mode(devinfo, inst) == BRW_ALIGN_1) {
         err |= control(file, "predicate control align1", pred_ctrl_align1,
                        brw_inst_pred_control(devinfo, inst), NULL);
      } else {
         err |= control(file, "predicate control align16", pred_ctrl_align16,
                        brw_inst_pred_control(devinfo, inst), NULL);
      }
      string(file, ") ");
   }

   err |= print_opcode(file, devinfo, opcode);
   err |= control(file, "saturate", saturate, brw_inst_saturate(devinfo, inst),
                  NULL);

   err |= control(file, "debug control", debug_ctrl,
                  brw_inst_debug_control(devinfo, inst), NULL);

   if (opcode == BRW_OPCODE_MATH) {
      string(file, " ");
      err |= control(file, "function", math_function,
                     brw_inst_math_function(devinfo, inst), NULL);
   } else if (opcode != BRW_OPCODE_SEND && opcode != BRW_OPCODE_SENDC) {
      err |= control(file, "conditional modifier", conditional_modifier,
                     brw_inst_cond_modifier(devinfo, inst), NULL);

      /* If we're using the conditional modifier, print which flags reg is
       * used for it.  Note that on gen6+, the embedded-condition SEL and
       * control flow doesn't update flags.
       */
      if (brw_inst_cond_modifier(devinfo, inst) &&
          (devinfo->gen < 6 || (opcode != BRW_OPCODE_SEL &&
                                opcode != BRW_OPCODE_IF &&
                                opcode != BRW_OPCODE_WHILE))) {
         format(file, ".f%"PRIu64,
                devinfo->gen >= 7 ? brw_inst_flag_reg_nr(devinfo, inst) : 0);
         if (brw_inst_flag_subreg_nr(devinfo, inst))
            format(file, ".%"PRIu64, brw_inst_flag_subreg_nr(devinfo, inst));
      }
   }

   if (opcode != BRW_OPCODE_NOP && opcode != BRW_OPCODE_NENOP) {
      string(file, "(");
      err |= control(file, "execution size", exec_size,
                     brw_inst_exec_size(devinfo, inst), NULL);
      string(file, ")");
   }

   if (opcode == BRW_OPCODE_SEND && devinfo->gen < 6)
      format(file, " %"PRIu64, brw_inst_base_mrf(devinfo, inst));

   if (has_uip(devinfo, opcode)) {
      /* Instructions that have UIP also have JIP. */
      pad(file, 16);
      format(file, "JIP: %d", brw_inst_jip(devinfo, inst));
      pad(file, 32);
      format(file, "UIP: %d", brw_inst_uip(devinfo, inst));
   } else if (has_jip(devinfo, opcode)) {
      pad(file, 16);
      if (devinfo->gen >= 7) {
         format(file, "JIP: %d", brw_inst_jip(devinfo, inst));
      } else {
         format(file, "JIP: %d", brw_inst_gen6_jump_count(devinfo, inst));
      }
   } else if (devinfo->gen < 6 && (opcode == BRW_OPCODE_BREAK ||
                                   opcode == BRW_OPCODE_CONTINUE ||
                                   opcode == BRW_OPCODE_ELSE)) {
      pad(file, 16);
      format(file, "Jump: %d", brw_inst_gen4_jump_count(devinfo, inst));
      pad(file, 32);
      format(file, "Pop: %"PRIu64, brw_inst_gen4_pop_count(devinfo, inst));
   } else if (devinfo->gen < 6 && (opcode == BRW_OPCODE_IF ||
                                   opcode == BRW_OPCODE_IFF ||
                                   opcode == BRW_OPCODE_HALT)) {
      pad(file, 16);
      format(file, "Jump: %d", brw_inst_gen4_jump_count(devinfo, inst));
   } else if (devinfo->gen < 6 && opcode == BRW_OPCODE_ENDIF) {
      pad(file, 16);
      format(file, "Pop: %"PRIu64, brw_inst_gen4_pop_count(devinfo, inst));
   } else if (opcode == BRW_OPCODE_JMPI) {
      pad(file, 16);
      err |= src1(file, devinfo, inst);
   } else if (desc && desc->nsrc == 3) {
      pad(file, 16);
      err |= dest_3src(file, devinfo, inst);

      pad(file, 32);
      err |= src0_3src(file, devinfo, inst);

      pad(file, 48);
      err |= src1_3src(file, devinfo, inst);

      pad(file, 64);
      err |= src2_3src(file, devinfo, inst);
   } else if (desc) {
      if (desc->ndst > 0) {
         pad(file, 16);
         err |= dest(file, devinfo, inst);
      }

      if (desc->nsrc > 0) {
         pad(file, 32);
         err |= src0(file, devinfo, inst);
      }

      if (desc->nsrc > 1) {
         pad(file, 48);
         err |= src1(file, devinfo, inst);
      }
   }

   if (opcode == BRW_OPCODE_SEND || opcode == BRW_OPCODE_SENDC) {
      enum brw_message_target sfid = brw_inst_sfid(devinfo, inst);

      if (brw_inst_src1_reg_file(devinfo, inst) != BRW_IMMEDIATE_VALUE) {
         /* show the indirect descriptor source */
         pad(file, 48);
         err |= src1(file, devinfo, inst);
      }

      newline(file);
      pad(file, 16);
      space = 0;

      fprintf(file, "            ");
      err |= control(file, "SFID", devinfo->gen >= 6 ? gen6_sfid : gen4_sfid,
                     sfid, &space);


      if (brw_inst_src1_reg_file(devinfo, inst) != BRW_IMMEDIATE_VALUE) {
         format(file, " indirect");
      } else {
         switch (sfid) {
         case BRW_SFID_MATH:
            err |= control(file, "math function", math_function,
                           brw_inst_math_msg_function(devinfo, inst), &space);
            err |= control(file, "math saturate", math_saturate,
                           brw_inst_math_msg_saturate(devinfo, inst), &space);
            err |= control(file, "math signed", math_signed,
                           brw_inst_math_msg_signed_int(devinfo, inst), &space);
            err |= control(file, "math scalar", math_scalar,
                           brw_inst_math_msg_data_type(devinfo, inst), &space);
            err |= control(file, "math precision", math_precision,
                           brw_inst_math_msg_precision(devinfo, inst), &space);
            break;
         case BRW_SFID_SAMPLER:
            if (devinfo->gen >= 5) {
               err |= control(file, "sampler message", gen5_sampler_msg_type,
                              brw_inst_sampler_msg_type(devinfo, inst), &space);
               err |= control(file, "sampler simd mode", gen5_sampler_simd_mode,
                              brw_inst_sampler_simd_mode(devinfo, inst), &space);
               format(file, " Surface = %"PRIu64" Sampler = %"PRIu64,
                      brw_inst_binding_table_index(devinfo, inst),
                      brw_inst_sampler(devinfo, inst));
            } else {
               format(file, " (%"PRIu64", %"PRIu64", %"PRIu64", ",
                      brw_inst_binding_table_index(devinfo, inst),
                      brw_inst_sampler(devinfo, inst),
                      brw_inst_sampler_msg_type(devinfo, inst));
               if (!devinfo->is_g4x) {
                  err |= control(file, "sampler target format",
                                 sampler_target_format,
                                 brw_inst_sampler_return_format(devinfo, inst), NULL);
               }
               string(file, ")");
            }
            break;
         case GEN6_SFID_DATAPORT_SAMPLER_CACHE:
         case GEN6_SFID_DATAPORT_CONSTANT_CACHE:
            /* aka BRW_SFID_DATAPORT_READ on Gen4-5 */
            if (devinfo->gen >= 6) {
               format(file, " (%"PRIu64", %"PRIu64", %"PRIu64", %"PRIu64")",
                      brw_inst_binding_table_index(devinfo, inst),
                      brw_inst_dp_msg_control(devinfo, inst),
                      brw_inst_dp_msg_type(devinfo, inst),
                      devinfo->gen >= 7 ? 0 : brw_inst_dp_write_commit(devinfo, inst));
            } else {
               bool is_965 = devinfo->gen == 4 && !devinfo->is_g4x;
               err |= control(file, "DP read message type",
                              is_965 ? gen4_dp_read_port_msg_type :
                                       g45_dp_read_port_msg_type,
                              brw_inst_dp_read_msg_type(devinfo, inst),
                              &space);

               format(file, " MsgCtrl = 0x%"PRIx64,
                      brw_inst_dp_read_msg_control(devinfo, inst));

               format(file, " Surface = %"PRIu64, brw_inst_binding_table_index(devinfo, inst));
            }
            break;

         case GEN6_SFID_DATAPORT_RENDER_CACHE: {
            /* aka BRW_SFID_DATAPORT_WRITE on Gen4-5 */
            unsigned msg_type = brw_inst_dp_write_msg_type(devinfo, inst);

            err |= control(file, "DP rc message type",
                           dp_rc_msg_type(devinfo), msg_type, &space);

            bool is_rt_write = msg_type ==
               (devinfo->gen >= 6 ? GEN6_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE
                                  : BRW_DATAPORT_WRITE_MESSAGE_RENDER_TARGET_WRITE);

            if (is_rt_write) {
               err |= control(file, "RT message type", m_rt_write_subtype,
                              brw_inst_rt_message_type(devinfo, inst), &space);
               if (devinfo->gen >= 6 && brw_inst_rt_slot_group(devinfo, inst))
                  string(file, " Hi");
               if (brw_inst_rt_last(devinfo, inst))
                  string(file, " LastRT");
               if (devinfo->gen < 7 && brw_inst_dp_write_commit(devinfo, inst))
                  string(file, " WriteCommit");
            } else {
               format(file, " MsgCtrl = 0x%"PRIx64,
                      brw_inst_dp_write_msg_control(devinfo, inst));
            }

            format(file, " Surface = %"PRIu64, brw_inst_binding_table_index(devinfo, inst));
            break;
         }

         case BRW_SFID_URB: {
            unsigned opcode = brw_inst_urb_opcode(devinfo, inst);

            format(file, " %"PRIu64, brw_inst_urb_global_offset(devinfo, inst));

            space = 1;

            err |= control(file, "urb opcode",
                           devinfo->gen >= 7 ? gen7_urb_opcode
                                             : gen5_urb_opcode,
                           opcode, &space);

            if (devinfo->gen >= 7 &&
                brw_inst_urb_per_slot_offset(devinfo, inst)) {
               string(file, " per-slot");
            }

            if (opcode == GEN8_URB_OPCODE_SIMD8_WRITE ||
                opcode == GEN8_URB_OPCODE_SIMD8_READ) {
               if (brw_inst_urb_channel_mask_present(devinfo, inst))
                  string(file, " masked");
            } else {
               err |= control(file, "urb swizzle", urb_swizzle,
                              brw_inst_urb_swizzle_control(devinfo, inst),
                              &space);
            }

            if (devinfo->gen < 7) {
               err |= control(file, "urb allocate", urb_allocate,
                              brw_inst_urb_allocate(devinfo, inst), &space);
               err |= control(file, "urb used", urb_used,
                              brw_inst_urb_used(devinfo, inst), &space);
            }
            if (devinfo->gen < 8) {
               err |= control(file, "urb complete", urb_complete,
                              brw_inst_urb_complete(devinfo, inst), &space);
            }
            break;
         }
         case BRW_SFID_THREAD_SPAWNER:
            break;

         case BRW_SFID_MESSAGE_GATEWAY:
            format(file, " (%s)",
                   gen7_gateway_subfuncid[brw_inst_gateway_subfuncid(devinfo, inst)]);
            break;

         case GEN7_SFID_DATAPORT_DATA_CACHE:
            if (devinfo->gen >= 7) {
               format(file, " (");

               err |= control(file, "DP DC0 message type",
                              dp_dc0_msg_type_gen7,
                              brw_inst_dp_msg_type(devinfo, inst), &space);

               format(file, ", %"PRIu64", ", brw_inst_binding_table_index(devinfo, inst));

               switch (brw_inst_dp_msg_type(devinfo, inst)) {
               case GEN7_DATAPORT_DC_UNTYPED_ATOMIC_OP:
                  control(file, "atomic op", aop,
                          brw_inst_imm_ud(devinfo, inst) >> 8 & 0xf, &space);
                  break;
               default:
                  format(file, "%"PRIu64, brw_inst_dp_msg_control(devinfo, inst));
               }
               format(file, ")");
               break;
            }
            /* FALLTHROUGH */

         case HSW_SFID_DATAPORT_DATA_CACHE_1: {
            if (devinfo->gen >= 7) {
               format(file, " (");

               unsigned msg_ctrl = brw_inst_dp_msg_control(devinfo, inst);

               err |= control(file, "DP DC1 message type",
                              dp_dc1_msg_type_hsw,
                              brw_inst_dp_msg_type(devinfo, inst), &space);

               format(file, ", Surface = %"PRIu64", ",
                      brw_inst_binding_table_index(devinfo, inst));

               switch (brw_inst_dp_msg_type(devinfo, inst)) {
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP:
               case HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP:
                  format(file, "SIMD%d,", (msg_ctrl & (1 << 4)) ? 8 : 16);
                  /* fallthrough */
               case HSW_DATAPORT_DC_PORT1_UNTYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_TYPED_ATOMIC_OP_SIMD4X2:
               case HSW_DATAPORT_DC_PORT1_ATOMIC_COUNTER_OP_SIMD4X2:
                  control(file, "atomic op", aop, msg_ctrl & 0xf, &space);
                  break;
               case HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_READ:
               case HSW_DATAPORT_DC_PORT1_UNTYPED_SURFACE_WRITE:
               case HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_READ:
               case HSW_DATAPORT_DC_PORT1_TYPED_SURFACE_WRITE: {
                  static const char *simd_modes[] = { "4x2", "16", "8" };
                  format(file, "SIMD%s, Mask = 0x%x",
                         simd_modes[msg_ctrl >> 4], msg_ctrl & 0xf);
                  break;
               }
               default:
                  format(file, "0x%x", msg_ctrl);
               }
               format(file, ")");
               break;
            }
            /* FALLTHROUGH */
         }

         case GEN7_SFID_PIXEL_INTERPOLATOR:
            if (devinfo->gen >= 7) {
               format(file, " (%s, %s, 0x%02"PRIx64")",
                      brw_inst_pi_nopersp(devinfo, inst) ? "linear" : "persp",
                      pixel_interpolator_msg_types[brw_inst_pi_message_type(devinfo, inst)],
                      brw_inst_pi_message_data(devinfo, inst));
               break;
            }
            /* FALLTHROUGH */

         default:
            format(file, "unsupported shared function ID %d", sfid);
            break;
         }

         if (space)
            string(file, " ");
         format(file, "mlen %"PRIu64, brw_inst_mlen(devinfo, inst));
         format(file, " rlen %"PRIu64, brw_inst_rlen(devinfo, inst));
      }
   }
   pad(file, 64);
   if (opcode != BRW_OPCODE_NOP && opcode != BRW_OPCODE_NENOP) {
      string(file, "{");
      space = 1;
      err |= control(file, "access mode", access_mode,
                     brw_inst_access_mode(devinfo, inst), &space);
      if (devinfo->gen >= 6) {
         err |= control(file, "write enable control", wectrl,
                        brw_inst_mask_control(devinfo, inst), &space);
      } else {
         err |= control(file, "mask control", mask_ctrl,
                        brw_inst_mask_control(devinfo, inst), &space);
      }
      err |= control(file, "dependency control", dep_ctrl,
                     ((brw_inst_no_dd_check(devinfo, inst) << 1) |
                      brw_inst_no_dd_clear(devinfo, inst)), &space);

      if (devinfo->gen >= 6)
         err |= qtr_ctrl(file, devinfo, inst);
      else {
         if (brw_inst_qtr_control(devinfo, inst) == BRW_COMPRESSION_COMPRESSED &&
             desc && desc->ndst > 0 &&
             brw_inst_dst_reg_file(devinfo, inst) == BRW_MESSAGE_REGISTER_FILE &&
             brw_inst_dst_da_reg_nr(devinfo, inst) & BRW_MRF_COMPR4) {
            format(file, " compr4");
         } else {
            err |= control(file, "compression control", compr_ctrl,
                           brw_inst_qtr_control(devinfo, inst), &space);
         }
      }

      err |= control(file, "compaction", cmpt_ctrl, is_compacted, &space);
      err |= control(file, "thread control", thread_ctrl,
                     brw_inst_thread_control(devinfo, inst), &space);
      if (has_branch_ctrl(devinfo, opcode)) {
         err |= control(file, "branch ctrl", branch_ctrl,
                        brw_inst_branch_control(devinfo, inst), &space);
      } else if (devinfo->gen >= 6) {
         err |= control(file, "acc write control", accwr,
                        brw_inst_acc_wr_control(devinfo, inst), &space);
      }
      if (opcode == BRW_OPCODE_SEND || opcode == BRW_OPCODE_SENDC)
         err |= control(file, "end of thread", end_of_thread,
                        brw_inst_eot(devinfo, inst), &space);
      if (space)
         string(file, " ");
      string(file, "}");
   }
   string(file, ";");
   newline(file);
   return err;
}
