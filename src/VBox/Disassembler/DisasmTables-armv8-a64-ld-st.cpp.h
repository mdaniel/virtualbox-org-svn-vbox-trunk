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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegOff, RT_BIT_32(26), 26);


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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,         /** @todo */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,         /** @todo */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegOff),
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,         /** @todo */
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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairPostIndex),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairOff),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPairPreIndex),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStRegPair, RT_BIT_32(23) | RT_BIT_32(24), 23);


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
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
    DIS_ARMV8_DECODE_MAP_INVALID_ENTRY,             /** @todo */
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStRegPair),
    DIS_ARMV8_DECODE_MAP_ENTRY(LdStReg),
DIS_ARMV8_DECODE_MAP_DEFINE_END(LdStOp0Lo, RT_BIT_32(28) | RT_BIT_32(29), 28);
