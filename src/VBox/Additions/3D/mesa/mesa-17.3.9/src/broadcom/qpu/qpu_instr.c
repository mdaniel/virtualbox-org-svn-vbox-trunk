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

#include <stdlib.h>
#include "util/macros.h"
#include "qpu_instr.h"

#ifndef QPU_MASK
#define QPU_MASK(high, low) ((((uint64_t)1<<((high)-(low)+1))-1)<<(low))
/* Using the GNU statement expression extension */
#define QPU_SET_FIELD(value, field)                                       \
        ({                                                                \
                uint64_t fieldval = (uint64_t)(value) << field ## _SHIFT; \
                assert((fieldval & ~ field ## _MASK) == 0);               \
                fieldval & field ## _MASK;                                \
         })

#define QPU_GET_FIELD(word, field) ((uint32_t)(((word)  & field ## _MASK) >> field ## _SHIFT))

#define QPU_UPDATE_FIELD(inst, value, field)                              \
        (((inst) & ~(field ## _MASK)) | QPU_SET_FIELD(value, field))
#endif /* QPU_MASK */

#define VC5_QPU_OP_MUL_SHIFT                58
#define VC5_QPU_OP_MUL_MASK                 QPU_MASK(63, 58)

#define VC5_QPU_SIG_SHIFT                   53
#define VC5_QPU_SIG_MASK                    QPU_MASK(57, 53)
# define VC5_QPU_SIG_THRSW_BIT              0x1
# define VC5_QPU_SIG_LDUNIF_BIT             0x2
# define VC5_QPU_SIG_LDTMU_BIT              0x4
# define VC5_QPU_SIG_LDVARY_BIT             0x8

#define VC5_QPU_COND_SHIFT                  46
#define VC5_QPU_COND_MASK                   QPU_MASK(52, 46)

#define VC5_QPU_COND_IFA                    0
#define VC5_QPU_COND_IFB                    1
#define VC5_QPU_COND_IFNA                   2
#define VC5_QPU_COND_IFNB                   3

#define VC5_QPU_MM                          QPU_MASK(45, 45)
#define VC5_QPU_MA                          QPU_MASK(44, 44)

#define V3D_QPU_WADDR_M_SHIFT               38
#define V3D_QPU_WADDR_M_MASK                QPU_MASK(43, 38)

#define VC5_QPU_BRANCH_ADDR_LOW_SHIFT       35
#define VC5_QPU_BRANCH_ADDR_LOW_MASK        QPU_MASK(55, 35)

#define V3D_QPU_WADDR_A_SHIFT               32
#define V3D_QPU_WADDR_A_MASK                QPU_MASK(37, 32)

#define VC5_QPU_BRANCH_COND_SHIFT           32
#define VC5_QPU_BRANCH_COND_MASK            QPU_MASK(34, 32)

#define VC5_QPU_BRANCH_ADDR_HIGH_SHIFT      24
#define VC5_QPU_BRANCH_ADDR_HIGH_MASK       QPU_MASK(31, 24)

#define VC5_QPU_OP_ADD_SHIFT                24
#define VC5_QPU_OP_ADD_MASK                 QPU_MASK(31, 24)

#define VC5_QPU_MUL_B_SHIFT                 21
#define VC5_QPU_MUL_B_MASK                  QPU_MASK(23, 21)

#define VC5_QPU_BRANCH_MSFIGN_SHIFT         21
#define VC5_QPU_BRANCH_MSFIGN_MASK          QPU_MASK(22, 21)

#define VC5_QPU_MUL_A_SHIFT                 18
#define VC5_QPU_MUL_A_MASK                  QPU_MASK(20, 18)

#define VC5_QPU_ADD_B_SHIFT                 15
#define VC5_QPU_ADD_B_MASK                  QPU_MASK(17, 15)

#define VC5_QPU_BRANCH_BDU_SHIFT            15
#define VC5_QPU_BRANCH_BDU_MASK             QPU_MASK(17, 15)

#define VC5_QPU_BRANCH_UB                   QPU_MASK(14, 14)

#define VC5_QPU_ADD_A_SHIFT                 12
#define VC5_QPU_ADD_A_MASK                  QPU_MASK(14, 12)

#define VC5_QPU_BRANCH_BDI_SHIFT            12
#define VC5_QPU_BRANCH_BDI_MASK             QPU_MASK(13, 12)

#define VC5_QPU_RADDR_A_SHIFT               6
#define VC5_QPU_RADDR_A_MASK                QPU_MASK(11, 6)

#define VC5_QPU_RADDR_B_SHIFT               0
#define VC5_QPU_RADDR_B_MASK                QPU_MASK(5, 0)

const char *
v3d_qpu_magic_waddr_name(enum v3d_qpu_waddr waddr)
{
        static const char *waddr_magic[] = {
                [V3D_QPU_WADDR_R0] = "r0",
                [V3D_QPU_WADDR_R1] = "r1",
                [V3D_QPU_WADDR_R2] = "r2",
                [V3D_QPU_WADDR_R3] = "r3",
                [V3D_QPU_WADDR_R4] = "r4",
                [V3D_QPU_WADDR_R5] = "r5",
                [V3D_QPU_WADDR_NOP] = "-",
                [V3D_QPU_WADDR_TLB] = "tlb",
                [V3D_QPU_WADDR_TLBU] = "tlbu",
                [V3D_QPU_WADDR_TMU] = "tmu",
                [V3D_QPU_WADDR_TMUL] = "tmul",
                [V3D_QPU_WADDR_TMUD] = "tmud",
                [V3D_QPU_WADDR_TMUA] = "tmua",
                [V3D_QPU_WADDR_TMUAU] = "tmuau",
                [V3D_QPU_WADDR_VPM] = "vpm",
                [V3D_QPU_WADDR_VPMU] = "vpmu",
                [V3D_QPU_WADDR_SYNC] = "sync",
                [V3D_QPU_WADDR_SYNCU] = "syncu",
                [V3D_QPU_WADDR_RECIP] = "recip",
                [V3D_QPU_WADDR_RSQRT] = "rsqrt",
                [V3D_QPU_WADDR_EXP] = "exp",
                [V3D_QPU_WADDR_LOG] = "log",
                [V3D_QPU_WADDR_SIN] = "sin",
                [V3D_QPU_WADDR_RSQRT2] = "rsqrt2",
        };

        return waddr_magic[waddr];
}

const char *
v3d_qpu_add_op_name(enum v3d_qpu_add_op op)
{
        static const char *op_names[] = {
                [V3D_QPU_A_FADD] = "fadd",
                [V3D_QPU_A_FADDNF] = "faddnf",
                [V3D_QPU_A_VFPACK] = "vfpack",
                [V3D_QPU_A_ADD] = "add",
                [V3D_QPU_A_SUB] = "sub",
                [V3D_QPU_A_FSUB] = "fsub",
                [V3D_QPU_A_MIN] = "min",
                [V3D_QPU_A_MAX] = "max",
                [V3D_QPU_A_UMIN] = "umin",
                [V3D_QPU_A_UMAX] = "umax",
                [V3D_QPU_A_SHL] = "shl",
                [V3D_QPU_A_SHR] = "shr",
                [V3D_QPU_A_ASR] = "asr",
                [V3D_QPU_A_ROR] = "ror",
                [V3D_QPU_A_FMIN] = "fmin",
                [V3D_QPU_A_FMAX] = "fmax",
                [V3D_QPU_A_VFMIN] = "vfmin",
                [V3D_QPU_A_AND] = "and",
                [V3D_QPU_A_OR] = "or",
                [V3D_QPU_A_XOR] = "xor",
                [V3D_QPU_A_VADD] = "vadd",
                [V3D_QPU_A_VSUB] = "vsub",
                [V3D_QPU_A_NOT] = "not",
                [V3D_QPU_A_NEG] = "neg",
                [V3D_QPU_A_FLAPUSH] = "flapush",
                [V3D_QPU_A_FLBPUSH] = "flbpush",
                [V3D_QPU_A_FLBPOP] = "flbpop",
                [V3D_QPU_A_SETMSF] = "setmsf",
                [V3D_QPU_A_SETREVF] = "setrevf",
                [V3D_QPU_A_NOP] = "nop",
                [V3D_QPU_A_TIDX] = "tidx",
                [V3D_QPU_A_EIDX] = "eidx",
                [V3D_QPU_A_LR] = "lr",
                [V3D_QPU_A_VFLA] = "vfla",
                [V3D_QPU_A_VFLNA] = "vflna",
                [V3D_QPU_A_VFLB] = "vflb",
                [V3D_QPU_A_VFLNB] = "vflnb",
                [V3D_QPU_A_FXCD] = "fxcd",
                [V3D_QPU_A_XCD] = "xcd",
                [V3D_QPU_A_FYCD] = "fycd",
                [V3D_QPU_A_YCD] = "ycd",
                [V3D_QPU_A_MSF] = "msf",
                [V3D_QPU_A_REVF] = "revf",
                [V3D_QPU_A_VDWWT] = "vdwwt",
                [V3D_QPU_A_IID] = "iid",
                [V3D_QPU_A_SAMPID] = "sampid",
                [V3D_QPU_A_PATCHID] = "patchid",
                [V3D_QPU_A_TMUWT] = "tmuwt",
                [V3D_QPU_A_VPMSETUP] = "vpmsetup",
                [V3D_QPU_A_VPMWT] = "vpmwt",
                [V3D_QPU_A_LDVPMV] = "ldvpmv",
                [V3D_QPU_A_LDVPMD] = "ldvpmd",
                [V3D_QPU_A_LDVPMP] = "ldvpmp",
                [V3D_QPU_A_LDVPMG] = "ldvpmg",
                [V3D_QPU_A_FCMP] = "fcmp",
                [V3D_QPU_A_VFMAX] = "vfmax",
                [V3D_QPU_A_FROUND] = "fround",
                [V3D_QPU_A_FTOIN] = "ftoin",
                [V3D_QPU_A_FTRUNC] = "ftrunc",
                [V3D_QPU_A_FTOIZ] = "ftoiz",
                [V3D_QPU_A_FFLOOR] = "ffloor",
                [V3D_QPU_A_FTOUZ] = "ftouz",
                [V3D_QPU_A_FCEIL] = "fceil",
                [V3D_QPU_A_FTOC] = "ftoc",
                [V3D_QPU_A_FDX] = "fdx",
                [V3D_QPU_A_FDY] = "fdy",
                [V3D_QPU_A_STVPMV] = "stvpmv",
                [V3D_QPU_A_STVPMD] = "stvpmd",
                [V3D_QPU_A_STVPMP] = "stvpmp",
                [V3D_QPU_A_ITOF] = "itof",
                [V3D_QPU_A_CLZ] = "clz",
                [V3D_QPU_A_UTOF] = "utof",
        };

        if (op >= ARRAY_SIZE(op_names))
                return NULL;

        return op_names[op];
}

const char *
v3d_qpu_mul_op_name(enum v3d_qpu_mul_op op)
{
        static const char *op_names[] = {
                [V3D_QPU_M_ADD] = "add",
                [V3D_QPU_M_SUB] = "sub",
                [V3D_QPU_M_UMUL24] = "umul24",
                [V3D_QPU_M_VFMUL] = "vfmul",
                [V3D_QPU_M_SMUL24] = "smul24",
                [V3D_QPU_M_MULTOP] = "multop",
                [V3D_QPU_M_FMOV] = "fmov",
                [V3D_QPU_M_MOV] = "mov",
                [V3D_QPU_M_NOP] = "nop",
                [V3D_QPU_M_FMUL] = "fmul",
        };

        if (op >= ARRAY_SIZE(op_names))
                return NULL;

        return op_names[op];
}

const char *
v3d_qpu_cond_name(enum v3d_qpu_cond cond)
{
        switch (cond) {
        case V3D_QPU_COND_NONE:
                return "";
        case V3D_QPU_COND_IFA:
                return ".ifa";
        case V3D_QPU_COND_IFB:
                return ".ifb";
        case V3D_QPU_COND_IFNA:
                return ".ifna";
        case V3D_QPU_COND_IFNB:
                return ".ifnb";
        default:
                unreachable("bad cond value");
        }
}

const char *
v3d_qpu_branch_cond_name(enum v3d_qpu_branch_cond cond)
{
        switch (cond) {
        case V3D_QPU_BRANCH_COND_ALWAYS:
                return "";
        case V3D_QPU_BRANCH_COND_A0:
                return ".a0";
        case V3D_QPU_BRANCH_COND_NA0:
                return ".na0";
        case V3D_QPU_BRANCH_COND_ALLA:
                return ".alla";
        case V3D_QPU_BRANCH_COND_ANYNA:
                return ".anyna";
        case V3D_QPU_BRANCH_COND_ANYA:
                return ".anya";
        case V3D_QPU_BRANCH_COND_ALLNA:
                return ".allna";
        default:
                unreachable("bad branch cond value");
        }
}

const char *
v3d_qpu_msfign_name(enum v3d_qpu_msfign msfign)
{
        switch (msfign) {
        case V3D_QPU_MSFIGN_NONE:
                return "";
        case V3D_QPU_MSFIGN_P:
                return "p";
        case V3D_QPU_MSFIGN_Q:
                return "q";
        default:
                unreachable("bad branch cond value");
        }
}

const char *
v3d_qpu_pf_name(enum v3d_qpu_pf pf)
{
        switch (pf) {
        case V3D_QPU_PF_NONE:
                return "";
        case V3D_QPU_PF_PUSHZ:
                return ".pushz";
        case V3D_QPU_PF_PUSHN:
                return ".pushn";
        case V3D_QPU_PF_PUSHC:
                return ".pushc";
        default:
                unreachable("bad pf value");
        }
}

const char *
v3d_qpu_uf_name(enum v3d_qpu_uf uf)
{
        switch (uf) {
        case V3D_QPU_UF_NONE:
                return "";
        case V3D_QPU_UF_ANDZ:
                return ".andz";
        case V3D_QPU_UF_ANDNZ:
                return ".andnz";
        case V3D_QPU_UF_NORZ:
                return ".norz";
        case V3D_QPU_UF_NORNZ:
                return ".nornz";
        case V3D_QPU_UF_ANDN:
                return ".andn";
        case V3D_QPU_UF_ANDNN:
                return ".andnn";
        case V3D_QPU_UF_NORN:
                return ".norn";
        case V3D_QPU_UF_NORNN:
                return ".nornn";
        case V3D_QPU_UF_ANDC:
                return ".andc";
        case V3D_QPU_UF_ANDNC:
                return ".andnc";
        case V3D_QPU_UF_NORC:
                return ".norc";
        case V3D_QPU_UF_NORNC:
                return ".nornc";
        default:
                unreachable("bad pf value");
        }
}

const char *
v3d_qpu_pack_name(enum v3d_qpu_output_pack pack)
{
        switch (pack) {
        case V3D_QPU_PACK_NONE:
                return "";
        case V3D_QPU_PACK_L:
                return ".l";
        case V3D_QPU_PACK_H:
                return ".h";
        default:
                unreachable("bad pack value");
        }
}

const char *
v3d_qpu_unpack_name(enum v3d_qpu_input_unpack unpack)
{
        switch (unpack) {
        case V3D_QPU_UNPACK_NONE:
                return "";
        case V3D_QPU_UNPACK_L:
                return ".l";
        case V3D_QPU_UNPACK_H:
                return ".h";
        case V3D_QPU_UNPACK_ABS:
                return ".abs";
        case V3D_QPU_UNPACK_REPLICATE_32F_16:
                return ".ff";
        case V3D_QPU_UNPACK_REPLICATE_L_16:
                return ".ll";
        case V3D_QPU_UNPACK_REPLICATE_H_16:
                return ".hh";
        case V3D_QPU_UNPACK_SWAP_16:
                return ".swp";
        default:
                unreachable("bad unpack value");
        }
}

#define D	1
#define A	2
#define B	4
static const uint8_t add_op_args[] = {
        [V3D_QPU_A_FADD] = D | A | B,
        [V3D_QPU_A_FADDNF] = D | A | B,
        [V3D_QPU_A_VFPACK] = D | A | B,
        [V3D_QPU_A_ADD] = D | A | B,
        [V3D_QPU_A_VFPACK] = D | A | B,
        [V3D_QPU_A_SUB] = D | A | B,
        [V3D_QPU_A_VFPACK] = D | A | B,
        [V3D_QPU_A_FSUB] = D | A | B,
        [V3D_QPU_A_MIN] = D | A | B,
        [V3D_QPU_A_MAX] = D | A | B,
        [V3D_QPU_A_UMIN] = D | A | B,
        [V3D_QPU_A_UMAX] = D | A | B,
        [V3D_QPU_A_SHL] = D | A | B,
        [V3D_QPU_A_SHR] = D | A | B,
        [V3D_QPU_A_ASR] = D | A | B,
        [V3D_QPU_A_ROR] = D | A | B,
        [V3D_QPU_A_FMIN] = D | A | B,
        [V3D_QPU_A_FMAX] = D | A | B,
        [V3D_QPU_A_VFMIN] = D | A | B,

        [V3D_QPU_A_AND] = D | A | B,
        [V3D_QPU_A_OR] = D | A | B,
        [V3D_QPU_A_XOR] = D | A | B,

        [V3D_QPU_A_VADD] = D | A | B,
        [V3D_QPU_A_VSUB] = D | A | B,
        [V3D_QPU_A_NOT] = D | A,
        [V3D_QPU_A_NEG] = D | A,
        [V3D_QPU_A_FLAPUSH] = D | A,
        [V3D_QPU_A_FLBPUSH] = D | A,
        [V3D_QPU_A_FLBPOP] = D | A,
        [V3D_QPU_A_SETMSF] = D | A,
        [V3D_QPU_A_SETREVF] = D | A,
        [V3D_QPU_A_NOP] = 0,
        [V3D_QPU_A_TIDX] = D,
        [V3D_QPU_A_EIDX] = D,
        [V3D_QPU_A_LR] = D,
        [V3D_QPU_A_VFLA] = D,
        [V3D_QPU_A_VFLNA] = D,
        [V3D_QPU_A_VFLB] = D,
        [V3D_QPU_A_VFLNB] = D,

        [V3D_QPU_A_FXCD] = D,
        [V3D_QPU_A_XCD] = D,
        [V3D_QPU_A_FYCD] = D,
        [V3D_QPU_A_YCD] = D,

        [V3D_QPU_A_MSF] = D,
        [V3D_QPU_A_REVF] = D,
        [V3D_QPU_A_VDWWT] = D,
        [V3D_QPU_A_IID] = D,
        [V3D_QPU_A_SAMPID] = D,
        [V3D_QPU_A_PATCHID] = D,
        [V3D_QPU_A_TMUWT] = D,
        [V3D_QPU_A_VPMWT] = D,

        [V3D_QPU_A_VPMSETUP] = D | A,

        [V3D_QPU_A_LDVPMV] = D | A,
        [V3D_QPU_A_LDVPMD] = D | A,
        [V3D_QPU_A_LDVPMP] = D | A,
        [V3D_QPU_A_LDVPMG] = D | A | B,

        /* FIXME: MOVABSNEG */

        [V3D_QPU_A_FCMP] = D | A | B,
        [V3D_QPU_A_VFMAX] = D | A | B,

        [V3D_QPU_A_FROUND] = D | A,
        [V3D_QPU_A_FTOIN] = D | A,
        [V3D_QPU_A_FTRUNC] = D | A,
        [V3D_QPU_A_FTOIZ] = D | A,
        [V3D_QPU_A_FFLOOR] = D | A,
        [V3D_QPU_A_FTOUZ] = D | A,
        [V3D_QPU_A_FCEIL] = D | A,
        [V3D_QPU_A_FTOC] = D | A,

        [V3D_QPU_A_FDX] = D | A,
        [V3D_QPU_A_FDY] = D | A,

        [V3D_QPU_A_STVPMV] = A | B,
        [V3D_QPU_A_STVPMD] = A | B,
        [V3D_QPU_A_STVPMP] = A | B,

        [V3D_QPU_A_ITOF] = D | A,
        [V3D_QPU_A_CLZ] = D | A,
        [V3D_QPU_A_UTOF] = D | A,
};

static const uint8_t mul_op_args[] = {
        [V3D_QPU_M_ADD] = D | A | B,
        [V3D_QPU_M_SUB] = D | A | B,
        [V3D_QPU_M_UMUL24] = D | A | B,
        [V3D_QPU_M_VFMUL] = D | A | B,
        [V3D_QPU_M_SMUL24] = D | A | B,
        [V3D_QPU_M_MULTOP] = D | A | B,
        [V3D_QPU_M_FMOV] = D | A,
        [V3D_QPU_M_NOP] = 0,
        [V3D_QPU_M_MOV] = D | A,
        [V3D_QPU_M_FMUL] = D | A | B,
};

bool
v3d_qpu_add_op_has_dst(enum v3d_qpu_add_op op)
{
        assert(op < ARRAY_SIZE(add_op_args));

        return add_op_args[op] & D;
}

bool
v3d_qpu_mul_op_has_dst(enum v3d_qpu_mul_op op)
{
        assert(op < ARRAY_SIZE(mul_op_args));

        return mul_op_args[op] & D;
}

int
v3d_qpu_add_op_num_src(enum v3d_qpu_add_op op)
{
        assert(op < ARRAY_SIZE(add_op_args));

        uint8_t args = add_op_args[op];
        if (args & B)
                return 2;
        else if (args & A)
                return 1;
        else
                return 0;
}

int
v3d_qpu_mul_op_num_src(enum v3d_qpu_mul_op op)
{
        assert(op < ARRAY_SIZE(mul_op_args));

        uint8_t args = mul_op_args[op];
        if (args & B)
                return 2;
        else if (args & A)
                return 1;
        else
                return 0;
}

bool
v3d_qpu_magic_waddr_is_sfu(enum v3d_qpu_waddr waddr)
{
        switch (waddr) {
        case V3D_QPU_WADDR_RECIP:
        case V3D_QPU_WADDR_RSQRT:
        case V3D_QPU_WADDR_EXP:
        case V3D_QPU_WADDR_LOG:
        case V3D_QPU_WADDR_SIN:
        case V3D_QPU_WADDR_RSQRT2:
                return true;
        default:
                return false;
        }
}

bool
v3d_qpu_magic_waddr_is_tmu(enum v3d_qpu_waddr waddr)
{
        switch (waddr) {
        case V3D_QPU_WADDR_TMU:
        case V3D_QPU_WADDR_TMUL:
        case V3D_QPU_WADDR_TMUD:
        case V3D_QPU_WADDR_TMUA:
        case V3D_QPU_WADDR_TMUAU:
                return true;
        default:
                return false;
        }
}

bool
v3d_qpu_magic_waddr_is_tlb(enum v3d_qpu_waddr waddr)
{
        return (waddr == V3D_QPU_WADDR_TLB ||
                waddr == V3D_QPU_WADDR_TLBU);
}

bool
v3d_qpu_magic_waddr_is_vpm(enum v3d_qpu_waddr waddr)
{
        return (waddr == V3D_QPU_WADDR_VPM ||
                waddr == V3D_QPU_WADDR_VPMU);
}

bool
v3d_qpu_magic_waddr_is_tsy(enum v3d_qpu_waddr waddr)
{
        return (waddr == V3D_QPU_WADDR_SYNC ||
                waddr == V3D_QPU_WADDR_SYNCU);
}

bool
v3d_qpu_writes_r3(const struct v3d_qpu_instr *inst)
{
        return inst->sig.ldvary || inst->sig.ldvpm;
}

bool
v3d_qpu_writes_r4(const struct v3d_qpu_instr *inst)
{
        if (inst->sig.ldtmu)
                return true;

        if (inst->type == V3D_QPU_INSTR_TYPE_ALU) {
                if (inst->alu.add.magic_write &&
                    v3d_qpu_magic_waddr_is_sfu(inst->alu.add.waddr)) {
                        return true;
                }

                if (inst->alu.mul.magic_write &&
                    v3d_qpu_magic_waddr_is_sfu(inst->alu.mul.waddr)) {
                        return true;
                }
        }

        return false;
}

bool
v3d_qpu_writes_r5(const struct v3d_qpu_instr *inst)
{
        return inst->sig.ldvary || inst->sig.ldunif;
}

bool
v3d_qpu_uses_mux(const struct v3d_qpu_instr *inst, enum v3d_qpu_mux mux)
{
        int add_nsrc = v3d_qpu_add_op_num_src(inst->alu.add.op);
        int mul_nsrc = v3d_qpu_mul_op_num_src(inst->alu.mul.op);

        return ((add_nsrc > 0 && inst->alu.add.a == mux) ||
                (add_nsrc > 1 && inst->alu.add.b == mux) ||
                (mul_nsrc > 0 && inst->alu.mul.a == mux) ||
                (mul_nsrc > 1 && inst->alu.mul.b == mux));
}
