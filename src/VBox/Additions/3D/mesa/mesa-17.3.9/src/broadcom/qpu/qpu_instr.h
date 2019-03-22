/*
 * Copyright © 2016 Broadcom
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
 * @file qpu_instr.h
 *
 * Definitions of the unpacked form of QPU instructions.  Assembly and
 * disassembly will use this for talking about instructions, with qpu_encode.c
 * and qpu_decode.c handling the pack and unpack of the actual 64-bit QPU
 * instruction.
 */

#ifndef QPU_INSTR_H
#define QPU_INSTR_H

#include <stdbool.h>
#include <stdint.h>
#include "util/macros.h"

struct v3d_device_info;

struct v3d_qpu_sig {
        bool thrsw:1;
        bool ldunif:1;
        bool ldtmu:1;
        bool ldvary:1;
        bool ldvpm:1;
        bool ldtlb:1;
        bool ldtlbu:1;
        bool small_imm:1;
        bool ucb:1;
        bool rotate:1;
        bool wrtmuc:1;
};

enum v3d_qpu_cond {
        V3D_QPU_COND_NONE,
        V3D_QPU_COND_IFA,
        V3D_QPU_COND_IFB,
        V3D_QPU_COND_IFNA,
        V3D_QPU_COND_IFNB,
};

enum v3d_qpu_pf {
        V3D_QPU_PF_NONE,
        V3D_QPU_PF_PUSHZ,
        V3D_QPU_PF_PUSHN,
        V3D_QPU_PF_PUSHC,
};

enum v3d_qpu_uf {
        V3D_QPU_UF_NONE,
        V3D_QPU_UF_ANDZ,
        V3D_QPU_UF_ANDNZ,
        V3D_QPU_UF_NORNZ,
        V3D_QPU_UF_NORZ,
        V3D_QPU_UF_ANDN,
        V3D_QPU_UF_ANDNN,
        V3D_QPU_UF_NORNN,
        V3D_QPU_UF_NORN,
        V3D_QPU_UF_ANDC,
        V3D_QPU_UF_ANDNC,
        V3D_QPU_UF_NORNC,
        V3D_QPU_UF_NORC,
};

enum v3d_qpu_waddr {
        V3D_QPU_WADDR_R0 = 0,
        V3D_QPU_WADDR_R1 = 1,
        V3D_QPU_WADDR_R2 = 2,
        V3D_QPU_WADDR_R3 = 3,
        V3D_QPU_WADDR_R4 = 4,
        V3D_QPU_WADDR_R5 = 5,
        /* 6 is reserved, but note 3.2.2.8: "Result Writes" */
        V3D_QPU_WADDR_NOP = 6,
        V3D_QPU_WADDR_TLB = 7,
        V3D_QPU_WADDR_TLBU = 8,
        V3D_QPU_WADDR_TMU = 9,
        V3D_QPU_WADDR_TMUL = 10,
        V3D_QPU_WADDR_TMUD = 11,
        V3D_QPU_WADDR_TMUA = 12,
        V3D_QPU_WADDR_TMUAU = 13,
        V3D_QPU_WADDR_VPM = 14,
        V3D_QPU_WADDR_VPMU = 15,
        V3D_QPU_WADDR_SYNC = 16,
        V3D_QPU_WADDR_SYNCU = 17,
        /* reserved */
        V3D_QPU_WADDR_RECIP = 19,
        V3D_QPU_WADDR_RSQRT = 20,
        V3D_QPU_WADDR_EXP = 21,
        V3D_QPU_WADDR_LOG = 22,
        V3D_QPU_WADDR_SIN = 23,
        V3D_QPU_WADDR_RSQRT2 = 24,
};

struct v3d_qpu_flags {
        enum v3d_qpu_cond ac, mc;
        enum v3d_qpu_pf apf, mpf;
        enum v3d_qpu_uf auf, muf;
};

enum v3d_qpu_add_op {
        V3D_QPU_A_FADD,
        V3D_QPU_A_FADDNF,
        V3D_QPU_A_VFPACK,
        V3D_QPU_A_ADD,
        V3D_QPU_A_SUB,
        V3D_QPU_A_FSUB,
        V3D_QPU_A_MIN,
        V3D_QPU_A_MAX,
        V3D_QPU_A_UMIN,
        V3D_QPU_A_UMAX,
        V3D_QPU_A_SHL,
        V3D_QPU_A_SHR,
        V3D_QPU_A_ASR,
        V3D_QPU_A_ROR,
        V3D_QPU_A_FMIN,
        V3D_QPU_A_FMAX,
        V3D_QPU_A_VFMIN,
        V3D_QPU_A_AND,
        V3D_QPU_A_OR,
        V3D_QPU_A_XOR,
        V3D_QPU_A_VADD,
        V3D_QPU_A_VSUB,
        V3D_QPU_A_NOT,
        V3D_QPU_A_NEG,
        V3D_QPU_A_FLAPUSH,
        V3D_QPU_A_FLBPUSH,
        V3D_QPU_A_FLBPOP,
        V3D_QPU_A_SETMSF,
        V3D_QPU_A_SETREVF,
        V3D_QPU_A_NOP,
        V3D_QPU_A_TIDX,
        V3D_QPU_A_EIDX,
        V3D_QPU_A_LR,
        V3D_QPU_A_VFLA,
        V3D_QPU_A_VFLNA,
        V3D_QPU_A_VFLB,
        V3D_QPU_A_VFLNB,
        V3D_QPU_A_FXCD,
        V3D_QPU_A_XCD,
        V3D_QPU_A_FYCD,
        V3D_QPU_A_YCD,
        V3D_QPU_A_MSF,
        V3D_QPU_A_REVF,
        V3D_QPU_A_VDWWT,
        V3D_QPU_A_IID,
        V3D_QPU_A_SAMPID,
        V3D_QPU_A_PATCHID,
        V3D_QPU_A_TMUWT,
        V3D_QPU_A_VPMSETUP,
        V3D_QPU_A_VPMWT,
        V3D_QPU_A_LDVPMV,
        V3D_QPU_A_LDVPMD,
        V3D_QPU_A_LDVPMP,
        V3D_QPU_A_LDVPMG,
        V3D_QPU_A_FCMP,
        V3D_QPU_A_VFMAX,
        V3D_QPU_A_FROUND,
        V3D_QPU_A_FTOIN,
        V3D_QPU_A_FTRUNC,
        V3D_QPU_A_FTOIZ,
        V3D_QPU_A_FFLOOR,
        V3D_QPU_A_FTOUZ,
        V3D_QPU_A_FCEIL,
        V3D_QPU_A_FTOC,
        V3D_QPU_A_FDX,
        V3D_QPU_A_FDY,
        V3D_QPU_A_STVPMV,
        V3D_QPU_A_STVPMD,
        V3D_QPU_A_STVPMP,
        V3D_QPU_A_ITOF,
        V3D_QPU_A_CLZ,
        V3D_QPU_A_UTOF,
};

enum v3d_qpu_mul_op {
        V3D_QPU_M_ADD,
        V3D_QPU_M_SUB,
        V3D_QPU_M_UMUL24,
        V3D_QPU_M_VFMUL,
        V3D_QPU_M_SMUL24,
        V3D_QPU_M_MULTOP,
        V3D_QPU_M_FMOV,
        V3D_QPU_M_MOV,
        V3D_QPU_M_NOP,
        V3D_QPU_M_FMUL,
};

enum v3d_qpu_output_pack {
        V3D_QPU_PACK_NONE,
        /**
         * Convert to 16-bit float, put in low 16 bits of destination leaving
         * high unmodified.
         */
        V3D_QPU_PACK_L,
        /**
         * Convert to 16-bit float, put in high 16 bits of destination leaving
         * low unmodified.
         */
        V3D_QPU_PACK_H,
};

enum v3d_qpu_input_unpack {
        /**
         * No-op input unpacking.  Note that this enum's value doesn't match
         * the packed QPU instruction value of the field (we use 0 so that the
         * default on new instruction creation is no-op).
         */
        V3D_QPU_UNPACK_NONE,
        /** Absolute value.  Only available for some operations. */
        V3D_QPU_UNPACK_ABS,
        /** Convert low 16 bits from 16-bit float to 32-bit float. */
        V3D_QPU_UNPACK_L,
        /** Convert high 16 bits from 16-bit float to 32-bit float. */
        V3D_QPU_UNPACK_H,

        /** Convert to 16f and replicate it to the high bits. */
        V3D_QPU_UNPACK_REPLICATE_32F_16,

        /** Replicate low 16 bits to high */
        V3D_QPU_UNPACK_REPLICATE_L_16,

        /** Replicate high 16 bits to low */
        V3D_QPU_UNPACK_REPLICATE_H_16,

        /** Swap high and low 16 bits */
        V3D_QPU_UNPACK_SWAP_16,
};

enum v3d_qpu_mux {
        V3D_QPU_MUX_R0,
        V3D_QPU_MUX_R1,
        V3D_QPU_MUX_R2,
        V3D_QPU_MUX_R3,
        V3D_QPU_MUX_R4,
        V3D_QPU_MUX_R5,
        V3D_QPU_MUX_A,
        V3D_QPU_MUX_B,
};

struct v3d_qpu_alu_instr {
        struct {
                enum v3d_qpu_add_op op;
                enum v3d_qpu_mux a, b;
                uint8_t waddr;
                bool magic_write;
                enum v3d_qpu_output_pack output_pack;
                enum v3d_qpu_input_unpack a_unpack;
                enum v3d_qpu_input_unpack b_unpack;
        } add;

        struct {
                enum v3d_qpu_mul_op op;
                enum v3d_qpu_mux a, b;
                uint8_t waddr;
                bool magic_write;
                enum v3d_qpu_output_pack output_pack;
                enum v3d_qpu_input_unpack a_unpack;
                enum v3d_qpu_input_unpack b_unpack;
        } mul;
};

enum v3d_qpu_branch_cond {
        V3D_QPU_BRANCH_COND_ALWAYS,
        V3D_QPU_BRANCH_COND_A0,
        V3D_QPU_BRANCH_COND_NA0,
        V3D_QPU_BRANCH_COND_ALLA,
        V3D_QPU_BRANCH_COND_ANYNA,
        V3D_QPU_BRANCH_COND_ANYA,
        V3D_QPU_BRANCH_COND_ALLNA,
};

enum v3d_qpu_msfign {
        /** Ignore multisample flags when determining branch condition. */
        V3D_QPU_MSFIGN_NONE,
        /**
         * If no multisample flags are set in the lane (a pixel in the FS, a
         * vertex in the VS), ignore the lane's condition when computing the
         * branch condition.
         */
        V3D_QPU_MSFIGN_P,
        /**
         * If no multisample flags are set in a 2x2 quad in the FS, ignore the
         * quad's a/b conditions.
         */
        V3D_QPU_MSFIGN_Q,
};

enum v3d_qpu_branch_dest {
        V3D_QPU_BRANCH_DEST_ABS,
        V3D_QPU_BRANCH_DEST_REL,
        V3D_QPU_BRANCH_DEST_LINK_REG,
        V3D_QPU_BRANCH_DEST_REGFILE,
};

struct v3d_qpu_branch_instr {
        enum v3d_qpu_branch_cond cond;
        enum v3d_qpu_msfign msfign;

        /** Selects how to compute the new IP if the branch is taken. */
        enum v3d_qpu_branch_dest bdi;

        /**
         * Selects how to compute the new uniforms pointer if the branch is
         * taken.  (ABS/REL implicitly load a uniform and use that)
         */
        enum v3d_qpu_branch_dest bdu;

        /**
         * If set, then udest determines how the uniform stream will branch,
         * otherwise the uniform stream is left as is.
         */
        bool ub;

        uint8_t raddr_a;

        uint32_t offset;
};

enum v3d_qpu_instr_type {
        V3D_QPU_INSTR_TYPE_ALU,
        V3D_QPU_INSTR_TYPE_BRANCH,
};

struct v3d_qpu_instr {
        enum v3d_qpu_instr_type type;

        struct v3d_qpu_sig sig;
        uint8_t raddr_a;
        uint8_t raddr_b;
        struct v3d_qpu_flags flags;

        union {
                struct v3d_qpu_alu_instr alu;
                struct v3d_qpu_branch_instr branch;
        };
};

const char *v3d_qpu_magic_waddr_name(enum v3d_qpu_waddr waddr);
const char *v3d_qpu_add_op_name(enum v3d_qpu_add_op op);
const char *v3d_qpu_mul_op_name(enum v3d_qpu_mul_op op);
const char *v3d_qpu_cond_name(enum v3d_qpu_cond cond);
const char *v3d_qpu_pf_name(enum v3d_qpu_pf pf);
const char *v3d_qpu_uf_name(enum v3d_qpu_uf uf);
const char *v3d_qpu_pack_name(enum v3d_qpu_output_pack pack);
const char *v3d_qpu_unpack_name(enum v3d_qpu_input_unpack unpack);
const char *v3d_qpu_branch_cond_name(enum v3d_qpu_branch_cond cond);
const char *v3d_qpu_msfign_name(enum v3d_qpu_msfign msfign);

bool v3d_qpu_add_op_has_dst(enum v3d_qpu_add_op op);
bool v3d_qpu_mul_op_has_dst(enum v3d_qpu_mul_op op);
int v3d_qpu_add_op_num_src(enum v3d_qpu_add_op op);
int v3d_qpu_mul_op_num_src(enum v3d_qpu_mul_op op);

bool v3d_qpu_sig_pack(const struct v3d_device_info *devinfo,
                      const struct v3d_qpu_sig *sig,
                      uint32_t *packed_sig);
bool v3d_qpu_sig_unpack(const struct v3d_device_info *devinfo,
                        uint32_t packed_sig,
                        struct v3d_qpu_sig *sig);

bool
v3d_qpu_flags_pack(const struct v3d_device_info *devinfo,
                   const struct v3d_qpu_flags *cond,
                   uint32_t *packed_cond);
bool
v3d_qpu_flags_unpack(const struct v3d_device_info *devinfo,
                     uint32_t packed_cond,
                     struct v3d_qpu_flags *cond);

bool
v3d_qpu_instr_pack(const struct v3d_device_info *devinfo,
                   const struct v3d_qpu_instr *instr,
                   uint64_t *packed_instr);
bool
v3d_qpu_instr_unpack(const struct v3d_device_info *devinfo,
                     uint64_t packed_instr,
                     struct v3d_qpu_instr *instr);

bool v3d_qpu_magic_waddr_is_sfu(enum v3d_qpu_waddr waddr) ATTRIBUTE_CONST;
bool v3d_qpu_magic_waddr_is_tmu(enum v3d_qpu_waddr waddr) ATTRIBUTE_CONST;
bool v3d_qpu_magic_waddr_is_tlb(enum v3d_qpu_waddr waddr) ATTRIBUTE_CONST;
bool v3d_qpu_magic_waddr_is_vpm(enum v3d_qpu_waddr waddr) ATTRIBUTE_CONST;
bool v3d_qpu_magic_waddr_is_tsy(enum v3d_qpu_waddr waddr) ATTRIBUTE_CONST;
bool v3d_qpu_writes_r3(const struct v3d_qpu_instr *instr) ATTRIBUTE_CONST;
bool v3d_qpu_writes_r4(const struct v3d_qpu_instr *instr) ATTRIBUTE_CONST;
bool v3d_qpu_writes_r5(const struct v3d_qpu_instr *instr) ATTRIBUTE_CONST;
bool v3d_qpu_uses_mux(const struct v3d_qpu_instr *inst, enum v3d_qpu_mux mux);

#endif
