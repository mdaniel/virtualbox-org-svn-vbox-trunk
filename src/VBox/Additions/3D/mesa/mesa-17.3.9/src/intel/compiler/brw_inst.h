/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/**
 * @file brw_inst.h
 *
 * A representation of i965 EU assembly instructions, with helper methods to
 * get and set various fields.  This is the actual hardware format.
 */

#ifndef BRW_INST_H
#define BRW_INST_H

#include <assert.h>
#include <stdint.h>

#include "brw_eu_defines.h"
#include "brw_reg_type.h"
#include "common/gen_device_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/* brw_context.h has a forward declaration of brw_inst, so name the struct. */
typedef struct brw_inst {
   uint64_t data[2];
} brw_inst;

static inline uint64_t brw_inst_bits(const brw_inst *inst,
                                     unsigned high, unsigned low);
static inline void brw_inst_set_bits(brw_inst *inst,
                                     unsigned high, unsigned low,
                                     uint64_t value);

#define FC(name, high, low, assertions)                       \
static inline void                                            \
brw_inst_set_##name(const struct gen_device_info *devinfo,    \
                    brw_inst *inst, uint64_t v)               \
{                                                             \
   assert(assertions);                                        \
   (void) devinfo;                                            \
   brw_inst_set_bits(inst, high, low, v);                     \
}                                                             \
static inline uint64_t                                        \
brw_inst_##name(const struct gen_device_info *devinfo,        \
                const brw_inst *inst)                         \
{                                                             \
   assert(assertions);                                        \
   (void) devinfo;                                            \
   return brw_inst_bits(inst, high, low);                     \
}

/* A simple macro for fields which stay in the same place on all generations. */
#define F(name, high, low) FC(name, high, low, true)

#define BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6, hi7, lo7, hi8, lo8) \
   unsigned high, low;                                                       \
   if (devinfo->gen >= 8) {                                                  \
      high = hi8;  low = lo8;                                                \
   } else if (devinfo->gen >= 7) {                                           \
      high = hi7;  low = lo7;                                                \
   } else if (devinfo->gen >= 6) {                                           \
      high = hi6;  low = lo6;                                                \
   } else if (devinfo->gen >= 5) {                                           \
      high = hi5;  low = lo5;                                                \
   } else if (devinfo->is_g4x) {                                             \
      high = hi45; low = lo45;                                               \
   } else {                                                                  \
      high = hi4;  low = lo4;                                                \
   }                                                                         \
   assert(((int) high) != -1 && ((int) low) != -1);

/* A general macro for cases where the field has moved to several different
 * bit locations across generations.  GCC appears to combine cases where the
 * bits are identical, removing some of the inefficiency.
 */
#define FF(name, hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6, hi7, lo7, hi8, lo8)\
static inline void                                                            \
brw_inst_set_##name(const struct gen_device_info *devinfo,                    \
                    brw_inst *inst, uint64_t value)                           \
{                                                                             \
   BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6, hi7, lo7, hi8, lo8)       \
   brw_inst_set_bits(inst, high, low, value);                                 \
}                                                                             \
static inline uint64_t                                                        \
brw_inst_##name(const struct gen_device_info *devinfo, const brw_inst *inst)  \
{                                                                             \
   BOUNDS(hi4, lo4, hi45, lo45, hi5, lo5, hi6, lo6, hi7, lo7, hi8, lo8)       \
   return brw_inst_bits(inst, high, low);                                     \
}

/* A macro for fields which moved as of Gen8+. */
#define F8(name, gen4_high, gen4_low, gen8_high, gen8_low) \
FF(name,                                                   \
   /* 4:   */ gen4_high, gen4_low,                         \
   /* 4.5: */ gen4_high, gen4_low,                         \
   /* 5:   */ gen4_high, gen4_low,                         \
   /* 6:   */ gen4_high, gen4_low,                         \
   /* 7:   */ gen4_high, gen4_low,                         \
   /* 8:   */ gen8_high, gen8_low);

F(src1_vstride,        120, 117)
F(src1_width,          116, 114)
F(src1_da16_swiz_w,    115, 114)
F(src1_da16_swiz_z,    113, 112)
F(src1_hstride,        113, 112)
F(src1_address_mode,   111, 111)
/** Src1.SrcMod @{ */
F(src1_negate,         110, 110)
F(src1_abs,            109, 109)
/** @} */
F8(src1_ia_subreg_nr,  /* 4+ */ 108, 106, /* 8+ */ 108, 105)
F(src1_da_reg_nr,      108, 101)
F(src1_da16_subreg_nr, 100, 100)
F(src1_da1_subreg_nr,  100,  96)
F(src1_da16_swiz_y,     99,  98)
F(src1_da16_swiz_x,     97,  96)
F8(src1_reg_hw_type,   /* 4+ */  46,  44, /* 8+ */  94,  91)
F8(src1_reg_file,      /* 4+ */  43,  42, /* 8+ */  90,  89)
F(src0_vstride,         88,  85)
F(src0_width,           84,  82)
F(src0_da16_swiz_w,     83,  82)
F(src0_da16_swiz_z,     81,  80)
F(src0_hstride,         81,  80)
F(src0_address_mode,    79,  79)
/** Src0.SrcMod @{ */
F(src0_negate,          78,  78)
F(src0_abs,             77,  77)
/** @} */
F8(src0_ia_subreg_nr,  /* 4+ */  76,  74, /* 8+ */  76,  73)
F(src0_da_reg_nr,       76,  69)
F(src0_da16_subreg_nr,  68,  68)
F(src0_da1_subreg_nr,   68,  64)
F(src0_da16_swiz_y,     67,  66)
F(src0_da16_swiz_x,     65,  64)
F(dst_address_mode,     63,  63)
F(dst_hstride,          62,  61)
F8(dst_ia_subreg_nr,   /* 4+ */  60,  58, /* 8+ */  60,  57)
F(dst_da_reg_nr,        60,  53)
F(dst_da16_subreg_nr,   52,  52)
F(dst_da1_subreg_nr,    52,  48)
F(da16_writemask,       51,  48) /* Dst.ChanEn */
F8(src0_reg_hw_type,   /* 4+ */  41,  39, /* 8+ */  46,  43)
F8(src0_reg_file,      /* 4+ */  38,  37, /* 8+ */  42,  41)
F8(dst_reg_hw_type,    /* 4+ */  36,  34, /* 8+ */  40,  37)
F8(dst_reg_file,       /* 4+ */  33,  32, /* 8+ */  36,  35)
F8(mask_control,       /* 4+ */   9,   9, /* 8+ */  34,  34)
FF(flag_reg_nr,
   /* 4-6: doesn't exist */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7: */ 90, 90,
   /* 8: */ 33, 33)
F8(flag_subreg_nr,     /* 4+ */  89, 89, /* 8+ */ 32, 32)
F(saturate,             31,  31)
F(debug_control,        30,  30)
F(cmpt_control,         29,  29)
FC(branch_control,      28,  28, devinfo->gen >= 8)
FC(acc_wr_control,      28,  28, devinfo->gen >= 6)
FC(mask_control_ex,     28,  28, devinfo->is_g4x || devinfo->gen == 5)
F(cond_modifier,        27,  24)
FC(math_function,       27,  24, devinfo->gen >= 6)
F(exec_size,            23,  21)
F(pred_inv,             20,  20)
F(pred_control,         19,  16)
F(thread_control,       15,  14)
F(qtr_control,          13,  12)
FF(nib_control,
   /* 4-6: doesn't exist */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7: */ 47, 47,
   /* 8: */ 11, 11)
F8(no_dd_check,        /* 4+ */  11, 11, /* 8+ */  10,  10)
F8(no_dd_clear,        /* 4+ */  10, 10, /* 8+ */   9,   9)
F(access_mode,           8,   8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(opcode,                6,   0)

/**
 * Three-source instructions:
 *  @{
 */
F(3src_src2_reg_nr,        125, 118) /* same in align1 */
F(3src_a16_src2_subreg_nr, 117, 115) /* Extra discontiguous bit on CHV? */
F(3src_a16_src2_swizzle,   114, 107)
F(3src_a16_src2_rep_ctrl,  106, 106)
F(3src_src1_reg_nr,        104,  97) /* same in align1 */
F(3src_a16_src1_subreg_nr,  96,  94) /* Extra discontiguous bit on CHV? */
F(3src_a16_src1_swizzle,    93,  86)
F(3src_a16_src1_rep_ctrl,   85,  85)
F(3src_src0_reg_nr,         83,  76) /* same in align1 */
F(3src_a16_src0_subreg_nr,  75,  73) /* Extra discontiguous bit on CHV? */
F(3src_a16_src0_swizzle,    72,  65)
F(3src_a16_src0_rep_ctrl,   64,  64)
F(3src_dst_reg_nr,          63,  56) /* same in align1 */
F(3src_a16_dst_subreg_nr,   55,  53)
F(3src_a16_dst_writemask,   52,  49)
F8(3src_a16_nib_ctrl,       47, 47, 11, 11) /* only exists on IVB+ */
F8(3src_a16_dst_hw_type,    45, 44, 48, 46) /* only exists on IVB+ */
F8(3src_a16_src_hw_type,    43, 42, 45, 43)
F8(3src_src2_negate,        41, 41, 42, 42)
F8(3src_src2_abs,           40, 40, 41, 41)
F8(3src_src1_negate,        39, 39, 40, 40)
F8(3src_src1_abs,           38, 38, 39, 39)
F8(3src_src0_negate,        37, 37, 38, 38)
F8(3src_src0_abs,           36, 36, 37, 37)
F8(3src_a16_flag_reg_nr,    34, 34, 33, 33)
F8(3src_a16_flag_subreg_nr, 33, 33, 32, 32)
FF(3src_a16_dst_reg_file,
   /* 4-5: doesn't exist - no 3-source instructions */ -1, -1, -1, -1, -1, -1,
   /* 6: */ 32, 32,
   /* 7-8: doesn't exist - no MRFs */ -1, -1, -1, -1)
F(3src_saturate,        31, 31)
F(3src_debug_control,   30, 30)
F(3src_cmpt_control,    29, 29)
F(3src_acc_wr_control,  28, 28)
F(3src_cond_modifier,   27, 24)
F(3src_exec_size,       23, 21)
F(3src_pred_inv,        20, 20)
F(3src_pred_control,    19, 16)
F(3src_thread_control,  15, 14)
F(3src_qtr_control,     13, 12)
F8(3src_no_dd_check,    11, 11, 10, 10)
F8(3src_no_dd_clear,    10, 10,  9,  9)
F8(3src_mask_control,    9,  9, 34, 34)
F(3src_access_mode,      8,  8)
/* Bit 7 is Reserved (for future Opcode expansion) */
F(3src_opcode,           6,  0)
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_3src_a16_##reg##_type(const struct gen_device_info *devinfo,     \
                                   brw_inst *inst, enum brw_reg_type type)    \
{                                                                             \
   unsigned hw_type = brw_reg_type_to_a16_hw_3src_type(devinfo, type);        \
   brw_inst_set_3src_a16_##reg##_hw_type(devinfo, inst, hw_type);             \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_3src_a16_##reg##_type(const struct gen_device_info *devinfo,         \
                               const brw_inst *inst)                          \
{                                                                             \
   unsigned hw_type = brw_inst_3src_a16_##reg##_hw_type(devinfo, inst);       \
   return brw_a16_hw_3src_type_to_reg_type(devinfo, hw_type);                 \
}

REG_TYPE(dst)
REG_TYPE(src)
#undef REG_TYPE

/**
 * Three-source align1 instructions:
 *  @{
 */
/* Reserved 127:126 */
/* src2_reg_nr same in align16 */
FC(3src_a1_src2_subreg_nr, 117, 113, devinfo->gen >= 10)
FC(3src_a1_src2_hstride,   112, 111, devinfo->gen >= 10)
/* Reserved 110:109. src2 vstride is an implied parameter */
FC(3src_a1_src2_hw_type,   108, 106, devinfo->gen >= 10)
/* Reserved 105 */
/* src1_reg_nr same in align16 */
FC(3src_a1_src1_subreg_nr,  96,  92, devinfo->gen >= 10)
FC(3src_a1_src1_hstride,    91,  90, devinfo->gen >= 10)
FC(3src_a1_src1_vstride,    89,  88, devinfo->gen >= 10)
FC(3src_a1_src1_hw_type,    87,  85, devinfo->gen >= 10)
/* Reserved 84 */
/* src0_reg_nr same in align16 */
FC(3src_a1_src0_subreg_nr,  75,  71, devinfo->gen >= 10)
FC(3src_a1_src0_hstride,    70,  69, devinfo->gen >= 10)
FC(3src_a1_src0_vstride,    68,  67, devinfo->gen >= 10)
FC(3src_a1_src0_hw_type,    66,  64, devinfo->gen >= 10)
/* dst_reg_nr same in align16 */
FC(3src_a1_dst_subreg_nr,   55,  54, devinfo->gen >= 10)
FC(3src_a1_special_acc,     55,  52, devinfo->gen >= 10) /* aliases dst_subreg_nr */
/* Reserved 51:50 */
FC(3src_a1_dst_hstride,     49,  49, devinfo->gen >= 10)
FC(3src_a1_dst_hw_type,     48,  46, devinfo->gen >= 10)
FC(3src_a1_src2_reg_file,   45,  45, devinfo->gen >= 10)
FC(3src_a1_src1_reg_file,   44,  44, devinfo->gen >= 10)
FC(3src_a1_src0_reg_file,   43,  43, devinfo->gen >= 10)
/* Source Modifier fields same in align16 */
FC(3src_a1_dst_reg_file,    36,  36, devinfo->gen >= 10)
FC(3src_a1_exec_type,       35,  35, devinfo->gen >= 10)
/* Fields below this same in align16 */
/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_3src_a1_##reg##_type(const struct gen_device_info *devinfo,      \
                                  brw_inst *inst, enum brw_reg_type type)     \
{                                                                             \
   UNUSED enum gen10_align1_3src_exec_type exec_type =                        \
      (enum gen10_align1_3src_exec_type) brw_inst_3src_a1_exec_type(devinfo,  \
                                                                    inst);    \
   if (brw_reg_type_is_floating_point(type)) {                                \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_FLOAT);                   \
   } else {                                                                   \
      assert(exec_type == BRW_ALIGN1_3SRC_EXEC_TYPE_INT);                     \
   }                                                                          \
   unsigned hw_type = brw_reg_type_to_a1_hw_3src_type(devinfo, type);         \
   brw_inst_set_3src_a1_##reg##_hw_type(devinfo, inst, hw_type);              \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_3src_a1_##reg##_type(const struct gen_device_info *devinfo,          \
                              const brw_inst *inst)                           \
{                                                                             \
   enum gen10_align1_3src_exec_type exec_type =                               \
      (enum gen10_align1_3src_exec_type) brw_inst_3src_a1_exec_type(devinfo,  \
                                                                    inst);    \
   unsigned hw_type = brw_inst_3src_a1_##reg##_hw_type(devinfo, inst);        \
   return brw_a1_hw_3src_type_to_reg_type(devinfo, hw_type, exec_type);       \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
REG_TYPE(src2)
#undef REG_TYPE

/**
 * Three-source align1 instruction immediates:
 *  @{
 */
static inline uint16_t
brw_inst_3src_a1_src0_imm(const struct gen_device_info *devinfo,
                          const brw_inst *insn)
{
   assert(devinfo->gen >= 10);
   return brw_inst_bits(insn, 82, 67);
}

static inline uint16_t
brw_inst_3src_a1_src2_imm(const struct gen_device_info *devinfo,
                          const brw_inst *insn)
{
   assert(devinfo->gen >= 10);
   return brw_inst_bits(insn, 124, 109);
}

static inline void
brw_inst_set_3src_a1_src0_imm(const struct gen_device_info *devinfo,
                              brw_inst *insn, uint16_t value)
{
   assert(devinfo->gen >= 10);
   brw_inst_set_bits(insn, 82, 67, value);
}

static inline void
brw_inst_set_3src_a1_src2_imm(const struct gen_device_info *devinfo,
                              brw_inst *insn, uint16_t value)
{
   assert(devinfo->gen >= 10);
   brw_inst_set_bits(insn, 124, 109, value);
}
/** @} */

/**
 * Flow control instruction bits:
 *  @{
 */
static inline void
brw_inst_set_uip(const struct gen_device_info *devinfo,
                 brw_inst *inst, int32_t value)
{
   assert(devinfo->gen >= 6);

   if (devinfo->gen >= 8) {
      brw_inst_set_bits(inst, 95, 64, (uint32_t)value);
   } else {
      assert(value <= (1 << 16) - 1);
      assert(value > -(1 << 16));
      brw_inst_set_bits(inst, 127, 112, (uint16_t)value);
   }
}

static inline int32_t
brw_inst_uip(const struct gen_device_info *devinfo, const brw_inst *inst)
{
   assert(devinfo->gen >= 6);

   if (devinfo->gen >= 8) {
      return brw_inst_bits(inst, 95, 64);
   } else {
      return (int16_t)brw_inst_bits(inst, 127, 112);
   }
}

static inline void
brw_inst_set_jip(const struct gen_device_info *devinfo,
                 brw_inst *inst, int32_t value)
{
   assert(devinfo->gen >= 6);

   if (devinfo->gen >= 8) {
      brw_inst_set_bits(inst, 127, 96, (uint32_t)value);
   } else {
      assert(value <= (1 << 15) - 1);
      assert(value >= -(1 << 15));
      brw_inst_set_bits(inst, 111, 96, (uint16_t)value);
   }
}

static inline int32_t
brw_inst_jip(const struct gen_device_info *devinfo, const brw_inst *inst)
{
   assert(devinfo->gen >= 6);

   if (devinfo->gen >= 8) {
      return brw_inst_bits(inst, 127, 96);
   } else {
      return (int16_t)brw_inst_bits(inst, 111, 96);
   }
}

/** Like FC, but using int16_t to handle negative jump targets. */
#define FJ(name, high, low, assertions)                                       \
static inline void                                                            \
brw_inst_set_##name(const struct gen_device_info *devinfo, brw_inst *inst, int16_t v) \
{                                                                             \
   assert(assertions);                                                        \
   (void) devinfo;                                                            \
   brw_inst_set_bits(inst, high, low, (uint16_t) v);                          \
}                                                                             \
static inline int16_t                                                         \
brw_inst_##name(const struct gen_device_info *devinfo, const brw_inst *inst)  \
{                                                                             \
   assert(assertions);                                                        \
   (void) devinfo;                                                            \
   return brw_inst_bits(inst, high, low);                                     \
}

FJ(gen6_jump_count,  63,  48, devinfo->gen == 6)
FJ(gen4_jump_count, 111,  96, devinfo->gen < 6)
FC(gen4_pop_count,  115, 112, devinfo->gen < 6)
/** @} */

/* Message descriptor bits */
#define MD(x) ((x) + 96)

/**
 * Fields for SEND messages:
 *  @{
 */
F(eot,                 127, 127)
FF(mlen,
   /* 4:   */ 119, 116,
   /* 4.5: */ 119, 116,
   /* 5:   */ 124, 121,
   /* 6:   */ 124, 121,
   /* 7:   */ 124, 121,
   /* 8:   */ 124, 121);
FF(rlen,
   /* 4:   */ 115, 112,
   /* 4.5: */ 115, 112,
   /* 5:   */ 120, 116,
   /* 6:   */ 120, 116,
   /* 7:   */ 120, 116,
   /* 8:   */ 120, 116);
FF(header_present,
   /* 4: doesn't exist */ -1, -1, -1, -1,
   /* 5:   */ 115, 115,
   /* 6:   */ 115, 115,
   /* 7:   */ 115, 115,
   /* 8:   */ 115, 115)
F(gateway_notify, MD(16), MD(15))
FF(function_control,
   /* 4:   */ 111,  96,
   /* 4.5: */ 111,  96,
   /* 5:   */ 114,  96,
   /* 6:   */ 114,  96,
   /* 7:   */ 114,  96,
   /* 8:   */ 114,  96)
FF(gateway_subfuncid,
   /* 4:   */ MD(1), MD(0),
   /* 4.5: */ MD(1), MD(0),
   /* 5:   */ MD(1), MD(0), /* 2:0, but bit 2 is reserved MBZ */
   /* 6:   */ MD(2), MD(0),
   /* 7:   */ MD(2), MD(0),
   /* 8:   */ MD(2), MD(0))
FF(sfid,
   /* 4:   */ 123, 120, /* called msg_target */
   /* 4.5  */ 123, 120,
   /* 5:   */  95,  92,
   /* 6:   */  27,  24,
   /* 7:   */  27,  24,
   /* 8:   */  27,  24)
FC(base_mrf,   27,  24, devinfo->gen < 6);
/** @} */

/**
 * URB message function control bits:
 *  @{
 */
FF(urb_per_slot_offset,
   /* 4-6: */ -1, -1, -1, -1, -1, -1, -1, -1,
   /* 7:   */ MD(16), MD(16),
   /* 8:   */ MD(17), MD(17))
FC(urb_channel_mask_present, MD(15), MD(15), devinfo->gen >= 8)
FC(urb_complete, MD(15), MD(15), devinfo->gen < 8)
FC(urb_used, MD(14), MD(14), devinfo->gen < 7)
FC(urb_allocate, MD(13), MD(13), devinfo->gen < 7)
FF(urb_swizzle_control,
   /* 4:   */ MD(11), MD(10),
   /* 4.5: */ MD(11), MD(10),
   /* 5:   */ MD(11), MD(10),
   /* 6:   */ MD(11), MD(10),
   /* 7:   */ MD(14), MD(14),
   /* 8:   */ MD(15), MD(15))
FF(urb_global_offset,
   /* 4:   */ MD( 9), MD(4),
   /* 4.5: */ MD( 9), MD(4),
   /* 5:   */ MD( 9), MD(4),
   /* 6:   */ MD( 9), MD(4),
   /* 7:   */ MD(13), MD(3),
   /* 8:   */ MD(14), MD(4))
FF(urb_opcode,
   /* 4:   */ MD( 3), MD(0),
   /* 4.5: */ MD( 3), MD(0),
   /* 5:   */ MD( 3), MD(0),
   /* 6:   */ MD( 3), MD(0),
   /* 7:   */ MD( 2), MD(0),
   /* 8:   */ MD( 3), MD(0))
/** @} */

/**
 * Gen4-5 math messages:
 *  @{
 */
FC(math_msg_data_type,  MD(7), MD(7), devinfo->gen < 6)
FC(math_msg_saturate,   MD(6), MD(6), devinfo->gen < 6)
FC(math_msg_precision,  MD(5), MD(5), devinfo->gen < 6)
FC(math_msg_signed_int, MD(4), MD(4), devinfo->gen < 6)
FC(math_msg_function,   MD(3), MD(0), devinfo->gen < 6)
/** @} */

/**
 * Sampler message function control bits:
 *  @{
 */
FF(sampler_simd_mode,
   /* 4: doesn't exist */ -1, -1, -1, -1,
   /* 5:   */ MD(17), MD(16),
   /* 6:   */ MD(17), MD(16),
   /* 7:   */ MD(18), MD(17),
   /* 8:   */ MD(18), MD(17))
FF(sampler_msg_type,
   /* 4:   */ MD(15), MD(14),
   /* 4.5: */ MD(15), MD(12),
   /* 5:   */ MD(15), MD(12),
   /* 6:   */ MD(15), MD(12),
   /* 7:   */ MD(16), MD(12),
   /* 8:   */ MD(16), MD(12))
FC(sampler_return_format, MD(13), MD(12), devinfo->gen == 4 && !devinfo->is_g4x)
F(sampler,                MD(11), MD(8))
F(binding_table_index,    MD( 7), MD(0)) /* also used by other messages */
/** @} */

/**
 * Data port message function control bits:
 *  @{
 */
FC(dp_category,         MD(18), MD(18), devinfo->gen >= 7)

/* Gen4-5 store fields in different bits for read/write messages. */
FF(dp_read_msg_type,
   /* 4:   */ MD(13), MD(12),
   /* 4.5: */ MD(13), MD(11),
   /* 5:   */ MD(13), MD(11),
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(17), MD(14))
FF(dp_write_msg_type,
   /* 4:   */ MD(14), MD(12),
   /* 4.5: */ MD(14), MD(12),
   /* 5:   */ MD(14), MD(12),
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(17), MD(14))
FF(dp_read_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5: */ MD(10), MD( 8),
   /* 5:   */ MD(10), MD( 8),
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
FF(dp_write_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5: */ MD(11), MD( 8),
   /* 5:   */ MD(11), MD( 8),
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
FC(dp_read_target_cache, MD(15), MD(14), devinfo->gen < 6);

FF(dp_write_commit,
   /* 4:   */ MD(15),  MD(15),
   /* 4.5: */ MD(15),  MD(15),
   /* 5:   */ MD(15),  MD(15),
   /* 6:   */ MD(17),  MD(17),
   /* 7+: does not exist */ -1, -1, -1, -1)

/* Gen6+ use the same bit locations for everything. */
FF(dp_msg_type,
   /* 4-5: use dp_read_msg_type or dp_write_msg_type instead */
   -1, -1, -1, -1, -1, -1,
   /* 6:   */ MD(16), MD(13),
   /* 7:   */ MD(17), MD(14),
   /* 8:   */ MD(17), MD(14))
FF(dp_msg_control,
   /* 4:   */ MD(11), MD( 8),
   /* 4.5-5: use dp_read_msg_control or dp_write_msg_control */ -1, -1, -1, -1,
   /* 6:   */ MD(12), MD( 8),
   /* 7:   */ MD(13), MD( 8),
   /* 8:   */ MD(13), MD( 8))
/** @} */

/**
 * Scratch message bits (Gen7+):
 *  @{
 */
FC(scratch_read_write, MD(17), MD(17), devinfo->gen >= 7) /* 0 = read,  1 = write */
FC(scratch_type,       MD(16), MD(16), devinfo->gen >= 7) /* 0 = OWord, 1 = DWord */
FC(scratch_invalidate_after_read, MD(15), MD(15), devinfo->gen >= 7)
FC(scratch_block_size,  MD(13),  MD(12), devinfo->gen >= 7)
FC(scratch_addr_offset, MD(11),  MD( 0), devinfo->gen >= 7)
/** @} */

/**
 * Render Target message function control bits:
 *  @{
 */
FF(rt_last,
   /* 4:   */ MD(11), MD(11),
   /* 4.5: */ MD(11), MD(11),
   /* 5:   */ MD(11), MD(11),
   /* 6:   */ MD(12), MD(12),
   /* 7:   */ MD(12), MD(12),
   /* 8:   */ MD(12), MD(12))
FC(rt_slot_group,      MD(11),  MD(11), devinfo->gen >= 6)
F(rt_message_type,     MD(10),  MD( 8))
/** @} */

/**
 * Thread Spawn message function control bits:
 *  @{
 */
F(ts_resource_select,  MD( 4),  MD( 4))
F(ts_request_type,     MD( 1),  MD( 1))
F(ts_opcode,           MD( 0),  MD( 0))
/** @} */

/**
 * Pixel Interpolator message function control bits:
 *  @{
 */
F(pi_simd_mode,      MD(16),  MD(16))
F(pi_nopersp,        MD(14),  MD(14))
F(pi_message_type,   MD(13),  MD(12))
F(pi_slot_group,     MD(11),  MD(11))
F(pi_message_data,   MD(7),   MD(0))
/** @} */

/**
 * Immediates:
 *  @{
 */
static inline int
brw_inst_imm_d(const struct gen_device_info *devinfo, const brw_inst *insn)
{
   (void) devinfo;
   return brw_inst_bits(insn, 127, 96);
}

static inline unsigned
brw_inst_imm_ud(const struct gen_device_info *devinfo, const brw_inst *insn)
{
   (void) devinfo;
   return brw_inst_bits(insn, 127, 96);
}

static inline uint64_t
brw_inst_imm_uq(const struct gen_device_info *devinfo, const brw_inst *insn)
{
   assert(devinfo->gen >= 8);
   return brw_inst_bits(insn, 127, 64);
}

static inline float
brw_inst_imm_f(const struct gen_device_info *devinfo, const brw_inst *insn)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.u = brw_inst_bits(insn, 127, 96);
   return ft.f;
}

static inline double
brw_inst_imm_df(const struct gen_device_info *devinfo, const brw_inst *insn)
{
   union {
      double d;
      uint64_t u;
   } dt;
   (void) devinfo;
   dt.u = brw_inst_bits(insn, 127, 64);
   return dt.d;
}

static inline void
brw_inst_set_imm_d(const struct gen_device_info *devinfo,
                   brw_inst *insn, int value)
{
   (void) devinfo;
   return brw_inst_set_bits(insn, 127, 96, value);
}

static inline void
brw_inst_set_imm_ud(const struct gen_device_info *devinfo,
                    brw_inst *insn, unsigned value)
{
   (void) devinfo;
   return brw_inst_set_bits(insn, 127, 96, value);
}

static inline void
brw_inst_set_imm_f(const struct gen_device_info *devinfo,
                   brw_inst *insn, float value)
{
   union {
      float f;
      uint32_t u;
   } ft;
   (void) devinfo;
   ft.f = value;
   brw_inst_set_bits(insn, 127, 96, ft.u);
}

static inline void
brw_inst_set_imm_df(const struct gen_device_info *devinfo,
                    brw_inst *insn, double value)
{
   union {
      double d;
      uint64_t u;
   } dt;
   (void) devinfo;
   dt.d = value;
   brw_inst_set_bits(insn, 127, 64, dt.u);
}

static inline void
brw_inst_set_imm_uq(const struct gen_device_info *devinfo,
                    brw_inst *insn, uint64_t value)
{
   (void) devinfo;
   brw_inst_set_bits(insn, 127, 64, value);
}

/** @} */

#define REG_TYPE(reg)                                                         \
static inline void                                                            \
brw_inst_set_##reg##_file_type(const struct gen_device_info *devinfo,         \
                               brw_inst *inst, enum brw_reg_file file,        \
                               enum brw_reg_type type)                        \
{                                                                             \
   assert(file <= BRW_IMMEDIATE_VALUE);                                       \
   unsigned hw_type = brw_reg_type_to_hw_type(devinfo, file, type);           \
   brw_inst_set_##reg##_reg_file(devinfo, inst, file);                        \
   brw_inst_set_##reg##_reg_hw_type(devinfo, inst, hw_type);                  \
}                                                                             \
                                                                              \
static inline enum brw_reg_type                                               \
brw_inst_##reg##_type(const struct gen_device_info *devinfo,                  \
                      const brw_inst *inst)                                   \
{                                                                             \
   unsigned file = __builtin_strcmp("dst", #reg) == 0 ?                       \
                   BRW_GENERAL_REGISTER_FILE :                                \
                   brw_inst_##reg##_reg_file(devinfo, inst);                  \
   unsigned hw_type = brw_inst_##reg##_reg_hw_type(devinfo, inst);            \
   return brw_hw_type_to_reg_type(devinfo, (enum brw_reg_file)file, hw_type); \
}

REG_TYPE(dst)
REG_TYPE(src0)
REG_TYPE(src1)
#undef REG_TYPE


/* The AddrImm fields are split into two discontiguous sections on Gen8+ */
#define BRW_IA1_ADDR_IMM(reg, g4_high, g4_low, g8_nine, g8_high, g8_low) \
static inline void                                                       \
brw_inst_set_##reg##_ia1_addr_imm(const struct gen_device_info *devinfo, \
                                  brw_inst *inst,                        \
                                  unsigned value)                        \
{                                                                        \
   assert((value & ~0x3ff) == 0);                                        \
   if (devinfo->gen >= 8) {                                              \
      brw_inst_set_bits(inst, g8_high, g8_low, value & 0x1ff);           \
      brw_inst_set_bits(inst, g8_nine, g8_nine, value >> 9);             \
   } else {                                                              \
      brw_inst_set_bits(inst, g4_high, g4_low, value);                   \
   }                                                                     \
}                                                                        \
static inline unsigned                                                   \
brw_inst_##reg##_ia1_addr_imm(const struct gen_device_info *devinfo,     \
                              const brw_inst *inst)                      \
{                                                                        \
   if (devinfo->gen >= 8) {                                              \
      return brw_inst_bits(inst, g8_high, g8_low) |                      \
             (brw_inst_bits(inst, g8_nine, g8_nine) << 9);               \
   } else {                                                              \
      return brw_inst_bits(inst, g4_high, g4_low);                       \
   }                                                                     \
}

/* AddrImm[9:0] for Align1 Indirect Addressing */
/*                     -Gen 4-  ----Gen8---- */
BRW_IA1_ADDR_IMM(src1, 105, 96, 121, 104, 96)
BRW_IA1_ADDR_IMM(src0,  73, 64,  95,  72, 64)
BRW_IA1_ADDR_IMM(dst,   57, 48,  47,  56, 48)

#define BRW_IA16_ADDR_IMM(reg, g4_high, g4_low, g8_nine, g8_high, g8_low) \
static inline void                                                        \
brw_inst_set_##reg##_ia16_addr_imm(const struct gen_device_info *devinfo, \
                                   brw_inst *inst, unsigned value)        \
{                                                                         \
   assert((value & ~0x3ff) == 0);                                         \
   if (devinfo->gen >= 8) {                                               \
      brw_inst_set_bits(inst, g8_high, g8_low, value & 0x1ff);            \
      brw_inst_set_bits(inst, g8_nine, g8_nine, value >> 9);              \
   } else {                                                               \
      brw_inst_set_bits(inst, g4_high, g4_low, value >> 9);               \
   }                                                                      \
}                                                                         \
static inline unsigned                                                    \
brw_inst_##reg##_ia16_addr_imm(const struct gen_device_info *devinfo,     \
                               const brw_inst *inst)                      \
{                                                                         \
   if (devinfo->gen >= 8) {                                               \
      return brw_inst_bits(inst, g8_high, g8_low) |                       \
             (brw_inst_bits(inst, g8_nine, g8_nine) << 9);                \
   } else {                                                               \
      return brw_inst_bits(inst, g4_high, g4_low);                        \
   }                                                                      \
}

/* AddrImm[9:0] for Align16 Indirect Addressing:
 * Compared to Align1, these are missing the low 4 bits.
 *                     -Gen 4-  ----Gen8----
 */
BRW_IA16_ADDR_IMM(src1, 105, 96, 121, 104, 100)
BRW_IA16_ADDR_IMM(src0,  73, 64,  95,  72,  68)
BRW_IA16_ADDR_IMM(dst,   57, 52,  47,  56,  52)

/**
 * Fetch a set of contiguous bits from the instruction.
 *
 * Bits indices range from 0..127; fields may not cross 64-bit boundaries.
 */
static inline uint64_t
brw_inst_bits(const brw_inst *inst, unsigned high, unsigned low)
{
   /* We assume the field doesn't cross 64-bit boundaries. */
   const unsigned word = high / 64;
   assert(word == low / 64);

   high %= 64;
   low %= 64;

   const uint64_t mask = (~0ull >> (64 - (high - low + 1)));

   return (inst->data[word] >> low) & mask;
}

/**
 * Set bits in the instruction, with proper shifting and masking.
 *
 * Bits indices range from 0..127; fields may not cross 64-bit boundaries.
 */
static inline void
brw_inst_set_bits(brw_inst *inst, unsigned high, unsigned low, uint64_t value)
{
   const unsigned word = high / 64;
   assert(word == low / 64);

   high %= 64;
   low %= 64;

   const uint64_t mask = (~0ull >> (64 - (high - low + 1))) << low;

   /* Make sure the supplied value actually fits in the given bitfield. */
   assert((value & (mask >> low)) == value);

   inst->data[word] = (inst->data[word] & ~mask) | (value << low);
}

#undef BRW_IA16_ADDR_IMM
#undef BRW_IA1_ADDR_IMM
#undef MD
#undef F8
#undef FF
#undef BOUNDS
#undef F
#undef FC

typedef struct {
   uint64_t data;
} brw_compact_inst;

/**
 * Fetch a set of contiguous bits from the compacted instruction.
 *
 * Bits indices range from 0..63.
 */
static inline unsigned
brw_compact_inst_bits(const brw_compact_inst *inst, unsigned high, unsigned low)
{
   const uint64_t mask = (1ull << (high - low + 1)) - 1;

   return (inst->data >> low) & mask;
}

/**
 * Set bits in the compacted instruction.
 *
 * Bits indices range from 0..63.
 */
static inline void
brw_compact_inst_set_bits(brw_compact_inst *inst, unsigned high, unsigned low,
                          uint64_t value)
{
   const uint64_t mask = ((1ull << (high - low + 1)) - 1) << low;

   /* Make sure the supplied value actually fits in the given bitfield. */
   assert((value & (mask >> low)) == value);

   inst->data = (inst->data & ~mask) | (value << low);
}

#define FC(name, high, low, assertions)                            \
static inline void                                                 \
brw_compact_inst_set_##name(const struct gen_device_info *devinfo, \
                            brw_compact_inst *inst, unsigned v)    \
{                                                                  \
   assert(assertions);                                             \
   (void) devinfo;                                                 \
   brw_compact_inst_set_bits(inst, high, low, v);                  \
}                                                                  \
static inline unsigned                                             \
brw_compact_inst_##name(const struct gen_device_info *devinfo,     \
                        const brw_compact_inst *inst)              \
{                                                                  \
   assert(assertions);                                             \
   (void) devinfo;                                                 \
   return brw_compact_inst_bits(inst, high, low);                  \
}

/* A simple macro for fields which stay in the same place on all generations. */
#define F(name, high, low) FC(name, high, low, true)

F(src1_reg_nr,      63, 56)
F(src0_reg_nr,      55, 48)
F(dst_reg_nr,       47, 40)
F(src1_index,       39, 35)
F(src0_index,       34, 30)
F(cmpt_control,     29, 29) /* Same location as brw_inst */
FC(flag_subreg_nr,  28, 28, devinfo->gen <= 6)
F(cond_modifier,    27, 24) /* Same location as brw_inst */
FC(acc_wr_control,  23, 23, devinfo->gen >= 6)
FC(mask_control_ex, 23, 23, devinfo->is_g4x || devinfo->gen == 5)
F(subreg_index,     22, 18)
F(datatype_index,   17, 13)
F(control_index,    12,  8)
F(debug_control,     7,  7)
F(opcode,            6,  0) /* Same location as brw_inst */

/**
 * (Gen8+) Compacted three-source instructions:
 *  @{
 */
FC(3src_src2_reg_nr,    63, 57, devinfo->gen >= 8)
FC(3src_src1_reg_nr,    56, 50, devinfo->gen >= 8)
FC(3src_src0_reg_nr,    49, 43, devinfo->gen >= 8)
FC(3src_src2_subreg_nr, 42, 40, devinfo->gen >= 8)
FC(3src_src1_subreg_nr, 39, 37, devinfo->gen >= 8)
FC(3src_src0_subreg_nr, 36, 34, devinfo->gen >= 8)
FC(3src_src2_rep_ctrl,  33, 33, devinfo->gen >= 8)
FC(3src_src1_rep_ctrl,  32, 32, devinfo->gen >= 8)
FC(3src_saturate,       31, 31, devinfo->gen >= 8)
FC(3src_debug_control,  30, 30, devinfo->gen >= 8)
FC(3src_cmpt_control,   29, 29, devinfo->gen >= 8)
FC(3src_src0_rep_ctrl,  28, 28, devinfo->gen >= 8)
/* Reserved */
FC(3src_dst_reg_nr,     18, 12, devinfo->gen >= 8)
FC(3src_source_index,   11, 10, devinfo->gen >= 8)
FC(3src_control_index,   9,  8, devinfo->gen >= 8)
/* Bit 7 is Reserved (for future Opcode expansion) */
FC(3src_opcode,          6,  0, devinfo->gen >= 8)
/** @} */

#undef F

#ifdef __cplusplus
}
#endif

#endif
