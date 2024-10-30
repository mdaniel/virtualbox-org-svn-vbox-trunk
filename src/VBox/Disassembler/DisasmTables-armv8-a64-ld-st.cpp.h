/* $Id$ */
/** @file
 * VBox disassembler - Tables for ARMv8 A64 - Lods & Stores.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/* STRB/LDRB/LDRSB/STR/LDR/STRH/LDRH/LDRSH/LDRSW/PRFM
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegUImmGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,          30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmMemOff,     10, 12, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegUImmGpr)
    DIS_ARMV8_OP(0x39000000, "strb",            OP_ARMV8_A64_STRB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x39400000, "ldrb",            OP_ARMV8_A64_LDRB,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x39800000, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x39c00000, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x79000000, "strh",            OP_ARMV8_A64_STRH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x79400000, "ldrh",            OP_ARMV8_A64_LDRH,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x79800000, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x79c00000, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb9000000, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb9400000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb9800000, "ldrsw",           OP_ARMV8_A64_LDRSW,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf9000000, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf9400000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE, /** @todo PRFM */
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegUImmGpr, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/* SIMD STR/LDR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegUImmSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,   30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,  0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmMemOff,     10, 12, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegUImmSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128, 0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,  0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmMemOff,     10, 12, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegUImmSimd)
    DIS_ARMV8_OP(           0x3d000000, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x3d400000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x3d800000, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS, LdStRegUImmSimd128), /** @todo size == 0. */
    DIS_ARMV8_OP_ALT_DECODE(0x3dc00000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdStRegUImmSimd128), /** @todo size == 0. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegUImmSimd, 0x3fc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(22) | RT_BIT_32(23), 22);


/*
 * C4.1.94 - Loads and Stores - Load/Store register variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegUImm)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUImmGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUImmSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegUImm, RT_BIT_32(26), 26);


/*
 * STRB/LDRB/LDRSB/STR/LDR/STRH/LDRH/LDRSH/LDRSW/PRFM
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegOffGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,          30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprOff,        16,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseOption,        13,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseS,             12,  1, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegOffGpr)
    DIS_ARMV8_OP(0x38200800, "strb",            OP_ARMV8_A64_STRB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x38600800, "ldrb",            OP_ARMV8_A64_LDRB,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x38a00800, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x38e00800, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78200800, "strh",            OP_ARMV8_A64_STRH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78600800, "ldrh",            OP_ARMV8_A64_LDRH,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x78a00800, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x78e00800, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8200800, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8600800, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb8a00800, "ldrsw",           OP_ARMV8_A64_LDRSW,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT ),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf8200800, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8600800, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE, /** @todo PRFM */
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegOffGpr, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/* SIMD LDR/STR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegOffSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,   30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,  0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprOff,        16,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseOption,        13,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseS,             12,  1, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegOffSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128, 0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,  0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprOff,        16,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseOption,        13,  3, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseS,             12,  1, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegOffSimd)
    DIS_ARMV8_OP(           0x3c200800, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x3c600800, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x3ca00800, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS, LdStRegOffSimd128), /** @todo size == 0. */
    DIS_ARMV8_OP_ALT_DECODE(0x3ce00800, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdStRegOffSimd128), /** @todo size == 0. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegOffSimd, 0x3fe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(22) | RT_BIT_32(23), 22);


/*
 * C4.1.94 - Loads and Stores - Load/Store register (register offset) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegOff)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOffGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOffSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegOff, RT_BIT_32(26), 26);


/* LDRAA/LDRAB */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPac)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseLdrPacImm,      0,  0, 1 /*idxParam*/), /* Hardcoded */
    DIS_ARMV8_INSN_DECODE(kDisParmParseLdrPacW,       11,  1, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPac)
    DIS_ARMV8_OP(0xf8200400, "ldraa",           OP_ARMV8_A64_LDRAA,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8a00400, "ldrab",           OP_ARMV8_A64_LDRAB,     DISOPTYPE_HARMLESS),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPac, 0xffa00400 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(23), 23);


/* Atomic memory operations - Byte size variants */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AtomicMemoryByte)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryByteThe)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryByteLrcpc)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AtomicMemoryByte)
    DIS_ARMV8_OP(           0x38200000, "ldaddb",          OP_ARMV8_A64_LDADDB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38201000, "ldclrb",          OP_ARMV8_A64_LDCLRB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38202000, "ldeorb",          OP_ARMV8_A64_LDEORB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38203000, "ldsetb",          OP_ARMV8_A64_LDSETB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38204000, "ldsmaxb",         OP_ARMV8_A64_LDSMAXB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38205000, "ldsminb",         OP_ARMV8_A64_LDSMINB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38206000, "ldumaxb",         OP_ARMV8_A64_LDUMAXB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38207000, "lduminb",         OP_ARMV8_A64_LDUMINB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38208000, "swpb",            OP_ARMV8_A64_SWPB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x38209000, "rcwclr",          OP_ARMV8_A64_RCWCLR,    DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x3820a000, "rcwswp",          OP_ARMV8_A64_RCWSWP,    DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x3820b000, "rcwset",          OP_ARMV8_A64_RCWSET,    DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x38600000, "ldaddlb",         OP_ARMV8_A64_LDADDLB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38601000, "ldclrlb",         OP_ARMV8_A64_LDCLRLB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38602000, "ldeorlb",         OP_ARMV8_A64_LDEORLB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38603000, "ldsetlb",         OP_ARMV8_A64_LDSETLB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38604000, "ldsmaxlb",        OP_ARMV8_A64_LDSMAXLB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38605000, "ldsminlb",        OP_ARMV8_A64_LDSMINLB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38606000, "ldumaxlb",        OP_ARMV8_A64_LDUMAXLB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38607000, "lduminlb",        OP_ARMV8_A64_LDUMINLB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38608000, "swplb",           OP_ARMV8_A64_SWPLB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x38609000, "rcwclrl",         OP_ARMV8_A64_RCWCLRL,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x3860a000, "rcwswpl",         OP_ARMV8_A64_RCWSWPL,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x3860b000, "rcwsetl",         OP_ARMV8_A64_RCWSETL,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x38a00000, "ldaddab",         OP_ARMV8_A64_LDADDAB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a01000, "ldclrab",         OP_ARMV8_A64_LDCLRAB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a02000, "ldeorab",         OP_ARMV8_A64_LDEORAB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a03000, "ldsetab",         OP_ARMV8_A64_LDSETAB,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a04000, "ldsmaxab",        OP_ARMV8_A64_LDSMAXAB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a05000, "ldsminab",        OP_ARMV8_A64_LDSMINAB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a06000, "ldumaxab",        OP_ARMV8_A64_LDUMAXAB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a07000, "lduminab",        OP_ARMV8_A64_LDUMINAB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38a08000, "swpab",           OP_ARMV8_A64_SWPAB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x38a09000, "rcwclra",         OP_ARMV8_A64_RCWCLRA,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x38a0a000, "rcwswpa",         OP_ARMV8_A64_RCWSWPA,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x38a0b000, "rcwseta",         OP_ARMV8_A64_RCWSETA,   DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x38a0c000, "ldaprb",          OP_ARMV8_A64_LDAPRB,    DISOPTYPE_HARMLESS, AtomicMemoryByteLrcpc), /* FEAT_LRCPC */ /** @todo Rs == 11111 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x38e00000, "ldaddalb",        OP_ARMV8_A64_LDADDALB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e01000, "ldclralb",        OP_ARMV8_A64_LDCLRALB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e02000, "ldeoralb",        OP_ARMV8_A64_LDEORALB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e03000, "ldsetalb",        OP_ARMV8_A64_LDSETALB,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e04000, "ldsmaxalb",       OP_ARMV8_A64_LDSMAXALB, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e05000, "ldsminalb",       OP_ARMV8_A64_LDSMINALB, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e06000, "ldumaxalb",       OP_ARMV8_A64_LDUMAXALB, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e07000, "lduminalb",       OP_ARMV8_A64_LDUMINALB, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x38e08000, "swpalb",          OP_ARMV8_A64_SWPALB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x38e09000, "rcwclral",        OP_ARMV8_A64_RCWCLRAL,  DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x38e0a000, "rcwswpal",        OP_ARMV8_A64_RCWSWPAL,  DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x38e0b000, "rcwsetal",        OP_ARMV8_A64_RCWSETAL,  DISOPTYPE_HARMLESS, AtomicMemoryByteThe), /* FEAT_THE */
    /* Rest of the encodings is invalid. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(AtomicMemoryByte, 0xffe0fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* opc */    RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14)
                            /* o3  */  | RT_BIT_32(15)
                            /* R   */  | RT_BIT_32(22)
                            /* A   */  | RT_BIT_32(23), 12);


/* Atomic memory operations - Halfword size variants */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AtomicMemoryHalfword)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryHalfwordThe)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryHalfwordLrcpc)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AtomicMemoryHalfword)
    DIS_ARMV8_OP(           0x78200000, "ldaddh",          OP_ARMV8_A64_LDADDH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78201000, "ldclrh",          OP_ARMV8_A64_LDCLRH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78202000, "ldeorh",          OP_ARMV8_A64_LDEORH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78203000, "ldseth",          OP_ARMV8_A64_LDSETH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78204000, "ldsmaxh",         OP_ARMV8_A64_LDSMAXH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78205000, "ldsminh",         OP_ARMV8_A64_LDSMINH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78206000, "ldumaxh",         OP_ARMV8_A64_LDUMAXH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78207000, "lduminh",         OP_ARMV8_A64_LDUMINH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78208000, "swph",            OP_ARMV8_A64_SWPH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x78209000, "rcwsclr",         OP_ARMV8_A64_RCWSCLR,   DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x7820a000, "rcwsswp",         OP_ARMV8_A64_RCWSSWP,   DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x7820b000, "rcwsset",         OP_ARMV8_A64_RCWSSET,   DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x78600000, "ldaddlh",         OP_ARMV8_A64_LDADDLH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78601000, "ldclrlh",         OP_ARMV8_A64_LDCLRLH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78602000, "ldeorlh",         OP_ARMV8_A64_LDEORLH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78603000, "ldsetlh",         OP_ARMV8_A64_LDSETLH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78604000, "ldsmaxlh",        OP_ARMV8_A64_LDSMAXLH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78605000, "ldsminlh",        OP_ARMV8_A64_LDSMINLH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78606000, "ldumaxlh",        OP_ARMV8_A64_LDUMAXLH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78607000, "lduminlh",        OP_ARMV8_A64_LDUMINLH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78608000, "swplh",           OP_ARMV8_A64_SWPLH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x78609000, "rcwsclrl",        OP_ARMV8_A64_RCWSCLRL,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x7860a000, "rcwsswpl",        OP_ARMV8_A64_RCWSSWPL,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x7860b000, "rcwssetl",        OP_ARMV8_A64_RCWSSETL,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x78a00000, "ldaddah",         OP_ARMV8_A64_LDADDAH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a01000, "ldclrah",         OP_ARMV8_A64_LDCLRAH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a02000, "ldeorah",         OP_ARMV8_A64_LDEORAH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a03000, "ldsetah",         OP_ARMV8_A64_LDSETAH,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a04000, "ldsmaxah",        OP_ARMV8_A64_LDSMAXAH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a05000, "ldsminah",        OP_ARMV8_A64_LDSMINAH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a06000, "ldumaxah",        OP_ARMV8_A64_LDUMAXAH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a07000, "lduminah",        OP_ARMV8_A64_LDUMINAH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78a08000, "swpah",           OP_ARMV8_A64_SWPAH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x78a09000, "rcwsclra",        OP_ARMV8_A64_RCWSCLRA,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x78a0a000, "rcwsswpa",        OP_ARMV8_A64_RCWSSWPA,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x78a0b000, "rcwsseta",        OP_ARMV8_A64_RCWSSETA,  DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x78a0c000, "ldaprh",          OP_ARMV8_A64_LDAPRH,    DISOPTYPE_HARMLESS, AtomicMemoryHalfwordLrcpc), /* FEAT_LRCPC */ /** @todo Rs == 11111 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0x78e00000, "ldaddalh",        OP_ARMV8_A64_LDADDALH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e01000, "ldclralh",        OP_ARMV8_A64_LDCLRALH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e02000, "ldeoralh",        OP_ARMV8_A64_LDEORALH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e03000, "ldsetalh",        OP_ARMV8_A64_LDSETALH,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e04000, "ldsmaxalh",       OP_ARMV8_A64_LDSMAXALH, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e05000, "ldsminalh",       OP_ARMV8_A64_LDSMINALH, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e06000, "ldumaxalh",       OP_ARMV8_A64_LDUMAXALH, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e07000, "lduminalh",       OP_ARMV8_A64_LDUMINALH, DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x78e08000, "swpalh",          OP_ARMV8_A64_SWPALH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x78e09000, "rcwsclral",       OP_ARMV8_A64_RCWSCLRAL, DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x78e0a000, "rcwsswpal",       OP_ARMV8_A64_RCWSSWPAL, DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    DIS_ARMV8_OP_ALT_DECODE(0x78e0b000, "rcwssetal",       OP_ARMV8_A64_RCWSSETAL, DISOPTYPE_HARMLESS, AtomicMemoryHalfwordThe), /* FEAT_THE */
    /* Rest of the encodings is invalid. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(AtomicMemoryHalfword, 0xffe0fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* opc */    RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14)
                            /* o3  */  | RT_BIT_32(15)
                            /* R   */  | RT_BIT_32(22)
                            /* A   */  | RT_BIT_32(23), 12);


/* Atomic memory operations - Word size variants */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AtomicMemoryWord)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryWordLrcpc)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AtomicMemoryWord)
    DIS_ARMV8_OP(           0xb8200000, "ldadd",           OP_ARMV8_A64_LDADD,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8201000, "ldclr",           OP_ARMV8_A64_LDCLR,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8202000, "ldeor",           OP_ARMV8_A64_LDEOR,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8203000, "ldset",           OP_ARMV8_A64_LDSET,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8204000, "ldsmax",          OP_ARMV8_A64_LDSMAX,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8205000, "ldsmin",          OP_ARMV8_A64_LDSMIN,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8206000, "ldumax",          OP_ARMV8_A64_LDUMAX,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8207000, "ldumin",          OP_ARMV8_A64_LDUMIN,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8208000, "swp",             OP_ARMV8_A64_SWP,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xb8600000, "ldaddl",          OP_ARMV8_A64_LDADDL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8601000, "ldclrl",          OP_ARMV8_A64_LDCLRL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8602000, "ldeorl",          OP_ARMV8_A64_LDEORL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8603000, "ldsetl",          OP_ARMV8_A64_LDSETL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8604000, "ldsmaxl",         OP_ARMV8_A64_LDSMAXL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8605000, "ldsminl",         OP_ARMV8_A64_LDSMINL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8606000, "ldumaxl",         OP_ARMV8_A64_LDUMAXL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8607000, "lduminl",         OP_ARMV8_A64_LDUMINL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8608000, "swpl",            OP_ARMV8_A64_SWPL,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xb8a00000, "ldadda",          OP_ARMV8_A64_LDADDA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a01000, "ldclra",          OP_ARMV8_A64_LDCLRA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a02000, "ldeora",          OP_ARMV8_A64_LDEORA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a03000, "ldseta",          OP_ARMV8_A64_LDSETA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a04000, "ldsmaxa",         OP_ARMV8_A64_LDSMAXA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a05000, "ldsmina",         OP_ARMV8_A64_LDSMINA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a06000, "ldumaxa",         OP_ARMV8_A64_LDUMAXA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a07000, "ldumina",         OP_ARMV8_A64_LDUMINA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8a08000, "swpa",            OP_ARMV8_A64_SWPA,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP_ALT_DECODE(0xb8a0c000, "ldapr",           OP_ARMV8_A64_LDAPR,     DISOPTYPE_HARMLESS, AtomicMemoryWordLrcpc), /* FEAT_LRCPC */ /** @todo Rs == 11111 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xb8e00000, "ldaddal",         OP_ARMV8_A64_LDADDAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e01000, "ldclral",         OP_ARMV8_A64_LDCLRAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e02000, "ldeoral",         OP_ARMV8_A64_LDEORAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e03000, "ldsetal",         OP_ARMV8_A64_LDSETAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e04000, "ldsmaxal",        OP_ARMV8_A64_LDSMAXAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e05000, "ldsminal",        OP_ARMV8_A64_LDSMINAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e06000, "ldumaxal",        OP_ARMV8_A64_LDUMAXAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e07000, "lduminal",        OP_ARMV8_A64_LDUMINAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xb8e08000, "swpal",           OP_ARMV8_A64_SWPAL,     DISOPTYPE_HARMLESS),
    /* Rest of the encodings is invalid. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(AtomicMemoryWord, 0xffe0fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* opc */    RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14)
                            /* o3  */  | RT_BIT_32(15)
                            /* R   */  | RT_BIT_32(22)
                            /* A   */  | RT_BIT_32(23), 12);


/* Atomic memory operations - Word size variants */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(AtomicMemoryDWord)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(AtomicMemoryDWordLrcpc)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(AtomicMemoryDWord)
    DIS_ARMV8_OP(           0xf8200000, "ldadd",           OP_ARMV8_A64_LDADD,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8201000, "ldclr",           OP_ARMV8_A64_LDCLR,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8202000, "ldeor",           OP_ARMV8_A64_LDEOR,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8203000, "ldset",           OP_ARMV8_A64_LDSET,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8204000, "ldsmax",          OP_ARMV8_A64_LDSMAX,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8205000, "ldsmin",          OP_ARMV8_A64_LDSMIN,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8206000, "ldumax",          OP_ARMV8_A64_LDUMAX,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8207000, "ldumin",          OP_ARMV8_A64_LDUMIN,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8208000, "swp",             OP_ARMV8_A64_SWP,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xf8600000, "ldaddl",          OP_ARMV8_A64_LDADDL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8601000, "ldclrl",          OP_ARMV8_A64_LDCLRL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8602000, "ldeorl",          OP_ARMV8_A64_LDEORL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8603000, "ldsetl",          OP_ARMV8_A64_LDSETL,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8604000, "ldsmaxl",         OP_ARMV8_A64_LDSMAXL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8605000, "ldsminl",         OP_ARMV8_A64_LDSMINL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8606000, "ldumaxl",         OP_ARMV8_A64_LDUMAXL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8607000, "lduminl",         OP_ARMV8_A64_LDUMINL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8608000, "swpl",            OP_ARMV8_A64_SWPL,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE, /** @todo ST64B   - FEAT_LS64 */
    INVALID_OPCODE, /** @todo ST64BV0 - FEAT_LS64_ACCDATA */
    INVALID_OPCODE, /** @todo ST64BV  - FEAT_LS64_V */
    INVALID_OPCODE,
    INVALID_OPCODE, /** @todo LD64B   - FEAT_LS64 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xf8a00000, "ldadda",          OP_ARMV8_A64_LDADDA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a01000, "ldclra",          OP_ARMV8_A64_LDCLRA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a02000, "ldeora",          OP_ARMV8_A64_LDEORA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a03000, "ldseta",          OP_ARMV8_A64_LDSETA,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a04000, "ldsmaxa",         OP_ARMV8_A64_LDSMAXA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a05000, "ldsmina",         OP_ARMV8_A64_LDSMINA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a06000, "ldumaxa",         OP_ARMV8_A64_LDUMAXA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a07000, "ldumina",         OP_ARMV8_A64_LDUMINA,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8a08000, "swpa",            OP_ARMV8_A64_SWPA,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP_ALT_DECODE(0xf8a0c000, "ldapr",           OP_ARMV8_A64_LDAPR,     DISOPTYPE_HARMLESS, AtomicMemoryDWordLrcpc), /* FEAT_LRCPC */ /** @todo Rs == 11111 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    DIS_ARMV8_OP(           0xf8e00000, "ldaddal",         OP_ARMV8_A64_LDADDAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e01000, "ldclral",         OP_ARMV8_A64_LDCLRAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e02000, "ldeoral",         OP_ARMV8_A64_LDEORAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e03000, "ldsetal",         OP_ARMV8_A64_LDSETAL,   DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e04000, "ldsmaxal",        OP_ARMV8_A64_LDSMAXAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e05000, "ldsminal",        OP_ARMV8_A64_LDSMINAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e06000, "ldumaxal",        OP_ARMV8_A64_LDUMAXAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e07000, "lduminal",        OP_ARMV8_A64_LDUMINAL,  DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0xf8e08000, "swpal",           OP_ARMV8_A64_SWPAL,     DISOPTYPE_HARMLESS),
    /* Rest of the encodings is invalid. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(AtomicMemoryDWord, 0xffe0fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* opc */    RT_BIT_32(12) | RT_BIT_32(13) | RT_BIT_32(14)
                            /* o3  */  | RT_BIT_32(15)
                            /* R   */  | RT_BIT_32(22)
                            /* A   */  | RT_BIT_32(23), 12);


/*
 * C4.1.94.29 - Loads and Stores - Atomic memory oeprations
 *
 * Differentiate further based on the size field.
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(AtomicMemory)
    DIS_ARMV8_DECODE_MAP_ENTRY(AtomicMemoryByte),
    DIS_ARMV8_DECODE_MAP_ENTRY(AtomicMemoryHalfword),
    DIS_ARMV8_DECODE_MAP_ENTRY(AtomicMemoryWord),
    DIS_ARMV8_DECODE_MAP_ENTRY(AtomicMemoryDWord),
DIS_ARMV8_DECODE_MAP_DEFINE_END(AtomicMemory, RT_BIT_32(30) | RT_BIT_32(31), 30);


/*
 * C4.1.94 - Loads and Stores - Load/Store register variants
 *
 * Differentiate further based on the op2<1:0> field.
 *
 *     Bit  11 10
 *     +-------------------------------------------
 *           0  0 Atomic memory operations
 *           0  1 Load/store register (pac)
 *           1  0 Load/store register (register offset)
 *           1  1 Load/store register (pac)
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegOp2_11_1)
    DIS_ARMV8_DECODE_MAP_ENTRY(AtomicMemory),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPac),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOff),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPac),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegOp2_11_1, RT_BIT_32(10) | RT_BIT_32(11), 10);


/*
 * STURB/LDURB/LDURSB/STURH/LDURH/LDURSH/STUR/LDUR/LDURSW/PRFUM
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegUnscaledImmGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,               30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,               0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegUnscaledImmGpr)
    DIS_ARMV8_OP(0x38000000, "sturb",           OP_ARMV8_A64_STURB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x38400000, "ldurb",           OP_ARMV8_A64_LDURB,     DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x38800000, "ldursb",          OP_ARMV8_A64_LDURSB,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x38c00000, "ldursb",          OP_ARMV8_A64_LDURSB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78000000, "sturh",           OP_ARMV8_A64_STURH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78400000, "ldurh",           OP_ARMV8_A64_LDURH,     DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x78800000, "ldursh",          OP_ARMV8_A64_LDURSH,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    DIS_ARMV8_OP(0x78c00000, "ldursh",          OP_ARMV8_A64_LDURSH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8000000, "stur",            OP_ARMV8_A64_STUR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8400000, "ldur",            OP_ARMV8_A64_LDUR,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb8800000, "ldursw",          OP_ARMV8_A64_LDURSW,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf8000000, "stur",            OP_ARMV8_A64_STUR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8400000, "ldur",            OP_ARMV8_A64_LDUR,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE, /** @todo PRFUM */
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegUnscaledImmGpr, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/* SIMD STUR/LDUR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegUnscaledImmSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,        30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegUnscaledImmSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128,      0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegUnscaledImmSimd)
    DIS_ARMV8_OP(           0x3c000000, "stur",            OP_ARMV8_A64_STUR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x3c400000, "ldur",            OP_ARMV8_A64_LDUR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x3c800000, "stur",            OP_ARMV8_A64_STUR,      DISOPTYPE_HARMLESS, LdStRegUnscaledImmSimd128), /** @todo size == 0. */
    DIS_ARMV8_OP_ALT_DECODE(0x3cc00000, "ldur",            OP_ARMV8_A64_LDUR,      DISOPTYPE_HARMLESS, LdStRegUnscaledImmSimd128), /** @todo size == 0. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegUnscaledImmSimd, 0x3fe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(22) | RT_BIT_32(23), 22);


/*
 * C4.1.94 - Loads and Stores - Load/Store register (register offset) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegUnscaledImm)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUnscaledImmGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUnscaledImmSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegUnscaledImm, RT_BIT_32(26), 26);


/*
 * STRB/LDRB/LDRSB/STRH/LDRH/LDRSH/STR/LDR/LDRSW/STR/LDR
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegImmPreIndexGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,               30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,               0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPreIndexed,       0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegImmPreIndexGpr)
    DIS_ARMV8_OP(0x38000c00, "strb",            OP_ARMV8_A64_STRB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x38400c00, "ldrb",            OP_ARMV8_A64_LDRB,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x38800c00, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x38c00c00, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0x78000c00, "strh",            OP_ARMV8_A64_STRH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78400c00, "ldrh",            OP_ARMV8_A64_LDRH,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x78800c00, "ldrsh",           OP_ARMV8_A64_LDURSH,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x78c00c00, "ldrsh",           OP_ARMV8_A64_LDURSH,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0xb8000c00, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8400c00, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb8800c00, "ldrsw",           OP_ARMV8_A64_LDURSW,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf8000c00, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8400c00, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegImmPreIndexGpr, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/* SIMD STR/LDR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegImmPreIndexSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,        30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPreIndexed,       0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegImmPreIndexSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128,      0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPreIndexed,       0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegImmPreIndexSimd)
    DIS_ARMV8_OP(           0x3c000c00, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x3c400c00, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x3c800c00, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS, LdStRegImmPreIndexSimd128), /** @todo size == 0. */
    DIS_ARMV8_OP_ALT_DECODE(0x3cc00c00, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdStRegImmPreIndexSimd128), /** @todo size == 0. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegImmPreIndexSimd, 0x3fe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(22) | RT_BIT_32(23), 22);


/*
 * C4.1.94.28 - Loads and Stores - Load/Store register (immediate pre-indexed) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegImmPreIndex)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPreIndexGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPreIndexSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegImmPreIndex, RT_BIT_32(26), 26);


/*
 * STRB/LDRB/LDRSB/STRH/LDRH/LDRSH/STR/LDR/LDRSW/STR/LDR
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegImmPostIndexGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,               30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,               0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPostIndexed,      0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegImmPostIndexGpr)
    DIS_ARMV8_OP(0x38000400, "strb",            OP_ARMV8_A64_STRB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x38400400, "ldrb",            OP_ARMV8_A64_LDRB,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x38800400, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x38c00400, "ldrsb",           OP_ARMV8_A64_LDRSB,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0x78000400, "strh",            OP_ARMV8_A64_STRH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78400400, "ldrh",            OP_ARMV8_A64_LDRH,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x78800400, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x78c00400, "ldrsh",           OP_ARMV8_A64_LDRSH,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0xb8000400, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8400400, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb8800400, "ldrsw",           OP_ARMV8_A64_LDRSW,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf8000400, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8400400, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegImmPostIndexGpr, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/* SIMD STR/LDR */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegImmPostIndexSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize,        30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPostIndexed,      0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegImmPostIndexSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128,      0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPostIndexed,      0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegImmPostIndexSimd)
    DIS_ARMV8_OP(           0x3c000400, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x3c400400, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x3c800400, "str",             OP_ARMV8_A64_STR,       DISOPTYPE_HARMLESS, LdStRegImmPostIndexSimd128), /** @todo size == 0. */
    DIS_ARMV8_OP_ALT_DECODE(0x3cc00400, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdStRegImmPostIndexSimd128), /** @todo size == 0. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegImmPostIndexSimd, 0x3fe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(22) | RT_BIT_32(23), 22);


/*
 * C4.1.94.26 - Loads and Stores - Load/Store register (immediate post-indexed) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegImmPostIndex)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPostIndexGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPostIndexSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegImmPostIndex, RT_BIT_32(26), 26);


/*
 * STTRB/LDTRB/LDTRSB/STTRH/LDTRH/LDTRSH/LDTRSH/STTR/LDTR/LDTRSW/STTR/LDTR
 *
 * Note: The size,opc bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegUnpriv)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSize,               30,  2, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,               0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOffUnscaled, 12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegUnpriv)
    DIS_ARMV8_OP(0x38000800, "sttrb",           OP_ARMV8_A64_STTRB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x38400800, "ldtrb",           OP_ARMV8_A64_LDTRB,     DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x38800800, "ldtrsb",          OP_ARMV8_A64_LDTRSB,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x38c00800, "ldtrsb",          OP_ARMV8_A64_LDTRSB,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0x78000800, "sttrh",           OP_ARMV8_A64_STTRH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0x78400800, "ldtrh",           OP_ARMV8_A64_LDTRH,     DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0x78800800, "ldtrsh",          OP_ARMV8_A64_LDTRSH,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x78c00800, "ldtrsh",          OP_ARMV8_A64_LDTRSH,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    DIS_ARMV8_OP(0xb8000800, "sttr",            OP_ARMV8_A64_STTR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xb8400800, "ldtr",            OP_ARMV8_A64_LDTR,      DISOPTYPE_HARMLESS),
 DIS_ARMV8_OP_EX(0xb8800800, "ldtrsw",          OP_ARMV8_A64_LDTRSW,    DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    DIS_ARMV8_OP(0xf8000800, "sttr",            OP_ARMV8_A64_STTR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(0xf8400800, "ldtr",            OP_ARMV8_A64_LDTR,      DISOPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegUnpriv, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(23) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * C4.1.94 - Loads and Stores - Load/Store register variants
 *
 * Differentiate further based on the op2<1:0> field.
 *
 *     Bit  11 10
 *     +-------------------------------------------
 *           0  0 Load/store register (unscaled immediate)
 *           0  1 Load/store register (immediate post-indexed)
 *           1  0 Load/store register (unprivileged)
 *           1  1 Load/store register (immediate pre-indexed)
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegOp2_11_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUnscaledImm),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPostIndex),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUnpriv),       /* No vector variants. */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegImmPreIndex),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegOp2_11_0, RT_BIT_32(10) | RT_BIT_32(11), 10);


/*
 * C4.1.94 - Loads and Stores - Load/Store register variants
 *
 * Differentiate further based on the op2<11> field.
 *
 *     Bit  21
 *     +-------------------------------------------
 *           0 Load/store register (unscaled immediate) / Load/store register (immediate post-indexed) / Load/store register (unprivileged) / Load/store register (immediate pre-indexed)
 *           1 Atomic memory operations / Load/store register (register offset) / Load/store register (pac).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegOp2_11)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOp2_11_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOp2_11_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegOp2_11, RT_BIT_32(21), 21);


/*
 * C4.1.94 - Loads and Stores - Load/Store register variants
 *
 * Differentiate further based on the op2<14> field.
 *
 *     Bit  24
 *     +-------------------------------------------
 *           0 All the other Load/store register variants and Atomic memory operations.
 *           1 Load/store register (unsigned immediate).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStReg)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOp2_11),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegUImm),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStReg, RT_BIT_32(24), 24);


/*
 * STP/LDP/STGP/LDPSW
 *
 * Note: The opc,L bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPairOff)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,         10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,    15,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPairOff)
 DIS_ARMV8_OP_EX(0x29000000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
 DIS_ARMV8_OP_EX(0x29400000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
 DIS_ARMV8_OP_EX(0xa9000000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0xa9400000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPairOff, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * STP/LDP/STGP/LDPSW - pre-indexed variant.
 *
 * Note: The opc,L bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPairPreIndex)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,         10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,    15,  7, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPreIndexed,  0,  0, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPairPreIndex)
 DIS_ARMV8_OP_EX(0x29800000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
 DIS_ARMV8_OP_EX(0x29c00000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
 DIS_ARMV8_OP_EX(0xa9800000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0xa9c00000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPairPreIndex, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * STP/LDP/STGP/LDPSW - post-indexed variant.
 *
 * Note: The opc,L bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPairPostIndex)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,     15,  7, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPostIndexed,  0,  0, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPairPostIndex)
 DIS_ARMV8_OP_EX(0x28800000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
 DIS_ARMV8_OP_EX(0x28c00000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
 DIS_ARMV8_OP_EX(0xa8800000, "stp",             OP_ARMV8_A64_STP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0xa8c00000, "ldp",             OP_ARMV8_A64_LDP,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPairPostIndex, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * STNP/LDNP - no-allocate variant.
 *
 * Note: The opc,L bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPairNoAllocGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,     15,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPairNoAllocGpr)
 DIS_ARMV8_OP_EX(0x28000000, "stnp",            OP_ARMV8_A64_STNP,      DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
 DIS_ARMV8_OP_EX(0x28400000, "ldnp",            OP_ARMV8_A64_LDNP,      DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
 DIS_ARMV8_OP_EX(0xa8000000, "stnp",            OP_ARMV8_A64_STNP,      DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0xa8400000, "ldnp",            OP_ARMV8_A64_LDNP,      DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE,
    INVALID_OPCODE,
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPairNoAllocGpr, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * SIMD STNP/LDNP - no-allocate variant.
 *
 * Note: The opc,L bitfields are concatenated to form an index.
 */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStRegPairNoAllocSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize32,       0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,      10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,         15,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegPairNoAllocSimd64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize64,       0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,      10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,         15,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStRegPairNoAllocSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128,      0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,      10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,           5,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmMemOff,         15,  7, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStRegPairNoAllocSimd)
    DIS_ARMV8_OP(           0x2c000000, "stnp",            OP_ARMV8_A64_STNP,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x2c400000, "ldnp",            OP_ARMV8_A64_LDNP,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x6c000000, "stnp",            OP_ARMV8_A64_STNP,      DISOPTYPE_HARMLESS, LdStRegPairNoAllocSimd64),
    DIS_ARMV8_OP_ALT_DECODE(0x6c400000, "ldnp",            OP_ARMV8_A64_LDNP,      DISOPTYPE_HARMLESS, LdStRegPairNoAllocSimd64),
    DIS_ARMV8_OP_ALT_DECODE(0xac000000, "stnp",            OP_ARMV8_A64_STNP,      DISOPTYPE_HARMLESS, LdStRegPairNoAllocSimd128),
    DIS_ARMV8_OP_ALT_DECODE(0xac400000, "ldnp",            OP_ARMV8_A64_LDNP,      DISOPTYPE_HARMLESS, LdStRegPairNoAllocSimd128),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStRegPairNoAllocSimd, 0xffc00000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(22) | RT_BIT_32(30) | RT_BIT_32(31), 22);


/*
 * C4.1.94.21 - Loads and Stores - Load/Store register (immediate post-indexed) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegPairNoAlloc)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairNoAllocGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairNoAllocSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegPairNoAlloc, RT_BIT_32(26), 26);


/*
 * C4.1.94 - Loads and Stores - Load/Store register pair variants
 *
 * Differentiate further based on the op2<14:13> field.
 *
 *     Bit  24 23
 *     +-------------------------------------------
 *           0  0 Load/store no-allocate pair (offset)
 *           0  1 Load/store register pair (post-indexed)
 *           1  0 Load/store register pair (offset).
 *           1  1 Load/store register pair (pre-indexed).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStRegPair)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairNoAlloc),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairPostIndex),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairOff),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairPreIndex),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegPair, RT_BIT_32(23) | RT_BIT_32(24), 23);


/* LDR/LDRSW/PRFM - literal variant. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdRegLiteralGpr)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,          5, 19, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdRegLiteralGpr)
 DIS_ARMV8_OP_EX(0x18000000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_32BIT),
 DIS_ARMV8_OP_EX(0x58000000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
 DIS_ARMV8_OP_EX(0x98000000, "ldrsw",           OP_ARMV8_A64_LDRSW,     DISOPTYPE_HARMLESS, DISARMV8INSNCLASS_F_FORCED_64BIT),
    INVALID_OPCODE, /** @todo PRFM */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdRegLiteralGpr, 0xff000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(30) | RT_BIT_32(31), 30);


/* SIMD LDR - literal variant. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdRegLiteralSimd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize32,       0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,              5, 19, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdRegLiteralSimd64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize64,       0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,              5, 19, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdRegLiteralSimd128)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegSize128,      0,  0, DIS_ARMV8_INSN_PARAM_UNSET),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSimdRegScalar,       0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseImmRel,              5, 19, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdRegLiteralSimd)
    DIS_ARMV8_OP(           0x1c000000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x5c000000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdRegLiteralSimd64),
    DIS_ARMV8_OP_ALT_DECODE(0x9c000000, "ldr",             OP_ARMV8_A64_LDR,       DISOPTYPE_HARMLESS, LdRegLiteralSimd128),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdRegLiteralSimd, 0xff000000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeNop,
                                       RT_BIT_32(30) | RT_BIT_32(31), 30);


/*
 * C4.1.94.19 - Loads and Stores - Load register (literal) variants
 *
 * Differentiate further based on the VR field.
 *
 *     Bit  26
 *     +-------------------------------------------
 *           0 GPR variants.
 *           1 SIMD/FP variants
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdRegLiteral)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdRegLiteralGpr),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdRegLiteralSimd),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdRegLiteral, RT_BIT_32(26), 26);


/* STG/STZGM/LDG/STZG/ST2G/STGM/STZ2G/LDGM. */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStMemTags)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprSp,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmTags,       12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStMemTagsLdg) /** @todo imm9 == 0 */
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmTags,       12,  9, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStMemTagsStzgm) /** @todo imm9 == 0 */
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStMemTagsPostIndex)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprSp,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmTags,       12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPostIndexed,  0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStMemTagsPreIndex)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprSp,           0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,       5,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSImmTags,       12,  9, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseSetPreIndexed,   0,  0, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStMemTags)
    DIS_ARMV8_OP_ALT_DECODE(0xd9200000, "stzgm",           OP_ARMV8_A64_STZGM,     DISOPTYPE_HARMLESS, LdStMemTagsStzgm),     /* FEAT_MTE2 */
    DIS_ARMV8_OP_ALT_DECODE(0xd9200400, "stg",             OP_ARMV8_A64_STG,       DISOPTYPE_HARMLESS, LdStMemTagsPostIndex), /* FEAT_MTE */
    DIS_ARMV8_OP(           0xd9200800, "stg",             OP_ARMV8_A64_STG,       DISOPTYPE_HARMLESS),                       /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9200c00, "stg",             OP_ARMV8_A64_STG,       DISOPTYPE_HARMLESS, LdStMemTagsPreIndex),  /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9600000, "ldg",             OP_ARMV8_A64_LDG,       DISOPTYPE_HARMLESS, LdStMemTagsLdg),       /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9600400, "stzg",            OP_ARMV8_A64_STZG,      DISOPTYPE_HARMLESS, LdStMemTagsPostIndex), /* FEAT_MTE */
    DIS_ARMV8_OP(           0xd9600800, "stzg",            OP_ARMV8_A64_STZG,      DISOPTYPE_HARMLESS),                       /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9600c00, "stzg",            OP_ARMV8_A64_STZG,      DISOPTYPE_HARMLESS, LdStMemTagsPreIndex),  /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9a00000, "stgm",            OP_ARMV8_A64_STGM,      DISOPTYPE_HARMLESS, LdStMemTagsStzgm),     /* FEAT_MTE2 */
    DIS_ARMV8_OP_ALT_DECODE(0xd9a00400, "st2g",            OP_ARMV8_A64_ST2G,      DISOPTYPE_HARMLESS, LdStMemTagsPostIndex), /* FEAT_MTE */
    DIS_ARMV8_OP(           0xd9a00800, "st2g",            OP_ARMV8_A64_ST2G,      DISOPTYPE_HARMLESS),                       /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9a00c00, "st2g",            OP_ARMV8_A64_ST2G,      DISOPTYPE_HARMLESS, LdStMemTagsPreIndex),  /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9e00000, "ldgm",            OP_ARMV8_A64_LDGM,      DISOPTYPE_HARMLESS, LdStMemTagsStzgm),     /* FEAT_MTE2 */
    DIS_ARMV8_OP_ALT_DECODE(0xd9e00400, "stz2g",           OP_ARMV8_A64_STZ2G,     DISOPTYPE_HARMLESS, LdStMemTagsPostIndex), /* FEAT_MTE */
    DIS_ARMV8_OP(           0xd9e00800, "stz2g",           OP_ARMV8_A64_STZ2G,     DISOPTYPE_HARMLESS),                       /* FEAT_MTE */
    DIS_ARMV8_OP_ALT_DECODE(0xd9e00c00, "stz2g",           OP_ARMV8_A64_STZ2G,     DISOPTYPE_HARMLESS, LdStMemTagsPreIndex),  /* FEAT_MTE */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStMemTags, 0xffe00c00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                                       RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(22) | RT_BIT_32(23), 10);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between further based on op0<3> (bit 31).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStBit28_1_Bit29_0_Bit24_1_Bit21_1)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,         /** @todo RCW compare and swap (pair) / 128-bit atomic memory operations */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStMemTags),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStBit28_1_Bit29_0_Bit24_1_Bit21_1, 31);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between further based on op2<11> (bit 21).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStBit28_1_Bit29_0_Bit24_1)
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,         /** @todo GCS load/store / LDIAPP/STILP / LDAPR/STLR / Memory Copy and Set */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStBit28_1_Bit29_0_Bit24_1_Bit21_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStBit28_1_Bit29_0_Bit24_1, 21);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between Load register (literal) and the other classes based on op2<14> (bit 24).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStBit28_1_Bit29_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdRegLiteral),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStBit28_1_Bit29_0_Bit24_1),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStBit28_1_Bit29_0, 24);


/* C4.1.94.13 - Loads and Stores - Load/Store ordered */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStOrdered)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStOrdered64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStOrdered)
    DIS_ARMV8_OP(           0x089f7c00, "stllrb",          OP_ARMV8_A64_STLLRB,    DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x089ffc00, "stlrb",           OP_ARMV8_A64_STLRB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x08df7c00, "ldlarb",          OP_ARMV8_A64_LDLARB,    DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x08dffc00, "ldarb",           OP_ARMV8_A64_LDARB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x489f7c00, "stllrh",          OP_ARMV8_A64_STLLRH,    DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x489ffc00, "stlrh",           OP_ARMV8_A64_STLRH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48df7c00, "ldlarh",          OP_ARMV8_A64_LDLARH,    DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x48dffc00, "ldarh",           OP_ARMV8_A64_LDARH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x889f7c00, "stllr",           OP_ARMV8_A64_STLLR,     DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x889ffc00, "stlr",            OP_ARMV8_A64_STLR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88df7c00, "ldlar",           OP_ARMV8_A64_LDLAR,     DISOPTYPE_HARMLESS),                /* FEAT_LOR */
    DIS_ARMV8_OP(           0x88dffc00, "ldar",            OP_ARMV8_A64_LDAR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0xc89f7c00, "stllr",           OP_ARMV8_A64_STLLR,     DISOPTYPE_HARMLESS, LdStOrdered64), /* FEAT_LOR */
    DIS_ARMV8_OP_ALT_DECODE(0xc89ffc00, "stlr",            OP_ARMV8_A64_STLR,      DISOPTYPE_HARMLESS, LdStOrdered64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8df7c00, "ldlar",           OP_ARMV8_A64_LDLAR,     DISOPTYPE_HARMLESS, LdStOrdered64), /* FEAT_LOR */
    DIS_ARMV8_OP_ALT_DECODE(0xc8dffc00, "ldar",            OP_ARMV8_A64_LDAR,      DISOPTYPE_HARMLESS, LdStOrdered64),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStOrdered, 0xfffffc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* o0 */     RT_BIT_32(15)
                            /* L  */   | RT_BIT_32(22)
                            /* size */ | RT_BIT_32(30) | RT_BIT_32(31), 15);


/* C4.1.94.14 - Loads and Stores - Compare and swap */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStCas)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStCas64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStCas)
    DIS_ARMV8_OP(           0x08a07c00, "casb",            OP_ARMV8_A64_CASB,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x08a0fc00, "caslb",           OP_ARMV8_A64_CASLB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x08e07c00, "casab",           OP_ARMV8_A64_CASAB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x08e0fc00, "casalb",          OP_ARMV8_A64_CASALB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48a07c00, "cash",            OP_ARMV8_A64_CASH,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48a0fc00, "caslh",           OP_ARMV8_A64_CASLH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48e07c00, "casah",           OP_ARMV8_A64_CASAH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48e0fc00, "casalh",          OP_ARMV8_A64_CASALH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88a07c00, "cas",             OP_ARMV8_A64_CAS,       DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88a0fc00, "casl",            OP_ARMV8_A64_CASL,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88e07c00, "casa",            OP_ARMV8_A64_CASA,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88e0fc00, "casal",           OP_ARMV8_A64_CASAL,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0xc8a07c00, "cas",             OP_ARMV8_A64_CAS,       DISOPTYPE_HARMLESS, LdStCas64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8a0fc00, "casl",            OP_ARMV8_A64_CASL,      DISOPTYPE_HARMLESS, LdStCas64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8e07c00, "casa",            OP_ARMV8_A64_CASA,      DISOPTYPE_HARMLESS, LdStCas64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8e0fc00, "casal",           OP_ARMV8_A64_CASAL,     DISOPTYPE_HARMLESS, LdStCas64),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStCas, 0xffe0fc00 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* o0 */     RT_BIT_32(15)
                            /* L  */   | RT_BIT_32(22)
                            /* size */ | RT_BIT_32(30) | RT_BIT_32(31), 15);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between Load/Store ordered and Compare and swap instruction classes based on op2<11> (bit 21).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStOrdered_Cas)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStOrdered),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStCas),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStOrdered_Cas, 21);


/* C4.1.94.11 - Loads and Stores - Load exclusive pair */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStExclusivePair)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET), /* Not exactly an SF bit but serves the same purpose. */
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,         10,  5, 2 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 3 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStExclusiveRegLd)
    DIS_ARMV8_INSN_DECODE(kDisParmParseSf,            30,  1, DIS_ARMV8_INSN_PARAM_UNSET), /* Not exactly an SF bit but serves the same purpose. */
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,          0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr,         10,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStExclusivePair)
    DIS_ARMV8_OP(           0x88200000, "stxp",            OP_ARMV8_A64_STXP,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88208000, "stlxp",           OP_ARMV8_A64_STLXP,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x88600000, "ldxp",            OP_ARMV8_A64_LDXP,      DISOPTYPE_HARMLESS, LdStExclusiveRegLd),
    DIS_ARMV8_OP_ALT_DECODE(0x88608000, "ldaxp",           OP_ARMV8_A64_LDAXP,     DISOPTYPE_HARMLESS, LdStExclusiveRegLd),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStExclusivePair, 0xbfe08000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* o0 */     RT_BIT_32(15)
                            /* L  */   | RT_BIT_32(22), 15);


/* C4.1.94.14 - Loads and Stores - Load exclusive register */
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER(LdStExclusiveReg)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStExclusiveRegLd32)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStExclusiveRegLd64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 1 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_DECODER_ALTERNATIVE(LdStExclusiveRegSt64)
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr32,       16,  5, 0 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseGprZr64,        0,  5, 1 /*idxParam*/),
    DIS_ARMV8_INSN_DECODE(kDisParmParseAddrGprSp,      5,  5, 2 /*idxParam*/),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_BEGIN(LdStExclusiveReg)
    DIS_ARMV8_OP(           0x08000000, "stxrb",           OP_ARMV8_A64_STXRB,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x08008000, "stlxrb",          OP_ARMV8_A64_STLXRB,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x08400000, "ldxrb",           OP_ARMV8_A64_LDXRB,     DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP_ALT_DECODE(0x08408000, "ldaxrb",          OP_ARMV8_A64_LDAXRB,    DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP(           0x48000000, "stxrh",           OP_ARMV8_A64_STXRH,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x48008000, "stlxrh",          OP_ARMV8_A64_STLXRH,    DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x48400000, "ldxrh",           OP_ARMV8_A64_LDXRH,     DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP_ALT_DECODE(0x48408000, "ldaxrh",          OP_ARMV8_A64_LDAXRH,    DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP(           0x88000000, "stxr",            OP_ARMV8_A64_STXR,      DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP(           0x88008000, "stlxr",           OP_ARMV8_A64_STLXR,     DISOPTYPE_HARMLESS),
    DIS_ARMV8_OP_ALT_DECODE(0x88400000, "ldxr",            OP_ARMV8_A64_LDXR,      DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP_ALT_DECODE(0x88408000, "ldaxr",           OP_ARMV8_A64_LDAXR,     DISOPTYPE_HARMLESS, LdStExclusiveRegLd32),
    DIS_ARMV8_OP_ALT_DECODE(0xc8000000, "stxr",            OP_ARMV8_A64_STXR,      DISOPTYPE_HARMLESS, LdStExclusiveRegSt64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8008000, "stlxr",           OP_ARMV8_A64_STLXR,     DISOPTYPE_HARMLESS, LdStExclusiveRegSt64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8400000, "ldxr",            OP_ARMV8_A64_LDXR,      DISOPTYPE_HARMLESS, LdStExclusiveRegLd64),
    DIS_ARMV8_OP_ALT_DECODE(0xc8408000, "ldaxr",           OP_ARMV8_A64_LDAXR,     DISOPTYPE_HARMLESS, LdStExclusiveRegLd64),
DIS_ARMV8_DECODE_INSN_CLASS_DEFINE_END(LdStExclusiveReg, 0xffe08000 /*fFixedInsn*/,
                                       kDisArmV8OpcDecodeCollate,
                            /* o0 */     RT_BIT_32(15)
                            /* L  */   | RT_BIT_32(22)
                            /* size */ | RT_BIT_32(30) | RT_BIT_32(31), 15);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between Load/Store exclusive register and pair instruction classes based on op2<11> (bit 21).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStExclusive)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStExclusiveReg),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStExclusivePair),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStExclusive, 21);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between Advanced SIMD load/stores and the rest based on op2<13> (bit 23).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStBit28_1_Bit29_0_Bit26_1)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStExclusive),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStOrdered_Cas),
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStBit28_1_Bit29_0_Bit26_1, 23);


/**
 * C4.1.94 - Loads and Stores
 *
 * Differentiate between Advanced SIMD load/stores and the rest based on op1 (bit 26).
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStBit28_0_Bit29_0)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStBit28_1_Bit29_0_Bit26_1),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY, /** @todo  Advanced SIMD load/store multiple structures (post-indexed) / Advanced SIMD load/store single structure / Advanced SIMD load/store single structure (post-indexed) */
DIS_ARMV8_DECODE_MAP_DEFINE_END_SINGLE_BIT(LdStBit28_0_Bit29_0, 26);


/*
 * C4.1.94 - Loads and Stores
 *
 * Differentiate further based on the op0<1:0> field.
 * Splitting this up because the decoding would get insane otherwise with tables doing cross referencing...
 *
 *     Bit  29 28
 *     +-------------------------------------------
 *           0  0 Compare and swap pair / Advanced SIMD loads/stores / Load/store exclusive pair / Load/store exclusive register
 *                Load/store ordered / Compare and swap
 *           0  1 RCW compare and swap / 128-bit atomic memory instructions / GCS load/store / Load/store memory tags /
 *                LDIAPP/STILP / LDAPR/STLR / Load register (literal) / Memory Copy and Set
 *           1  0 Load/store no-allocate pair / Load/store register pair /
 *           1  1 Load/store register / Atomic memory operations
 */
DIS_ARMV8_DECODE_MAP_DEFINE_BEGIN(LdStOp0Lo)
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStBit28_0_Bit29_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStBit28_1_Bit29_0),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPair),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStReg),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStOp0Lo, RT_BIT_32(28) | RT_BIT_32(29), 28);
