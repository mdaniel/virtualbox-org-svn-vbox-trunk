/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * Copyright 2013 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "nine_shader.h"

#include "device9.h"
#include "nine_debug.h"
#include "nine_state.h"
#include "vertexdeclaration9.h"

#include "util/macros.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "pipe/p_shader_tokens.h"
#include "tgsi/tgsi_ureg.h"
#include "tgsi/tgsi_dump.h"

#define DBG_CHANNEL DBG_SHADER

#ifndef VBOX_WITH_MESA3D_MSC
#define DUMP(args...) _nine_debug_printf(DBG_CHANNEL, NULL, args)
#else
#define DUMP(...) _nine_debug_printf(DBG_CHANNEL, NULL, __VA_ARGS__)
#endif


struct shader_translator;

typedef HRESULT (*translate_instruction_func)(struct shader_translator *);

static inline const char *d3dsio_to_string(unsigned opcode);


#define NINED3D_SM1_VS 0xfffe
#define NINED3D_SM1_PS 0xffff

#define NINE_MAX_COND_DEPTH 64
#define NINE_MAX_LOOP_DEPTH 64

#define NINED3DSP_END 0x0000ffff

#define NINED3DSPTYPE_FLOAT4  0
#define NINED3DSPTYPE_INT4    1
#define NINED3DSPTYPE_BOOL    2

#define NINED3DSPR_IMMEDIATE (D3DSPR_PREDICATE + 1)

#define NINED3DSP_WRITEMASK_MASK  D3DSP_WRITEMASK_ALL
#define NINED3DSP_WRITEMASK_SHIFT 16

#define NINED3DSHADER_INST_PREDICATED (1 << 28)

#define NINED3DSHADER_REL_OP_GT 1
#define NINED3DSHADER_REL_OP_EQ 2
#define NINED3DSHADER_REL_OP_GE 3
#define NINED3DSHADER_REL_OP_LT 4
#define NINED3DSHADER_REL_OP_NE 5
#define NINED3DSHADER_REL_OP_LE 6

#define NINED3DSIO_OPCODE_FLAGS_SHIFT 16
#define NINED3DSIO_OPCODE_FLAGS_MASK  (0xff << NINED3DSIO_OPCODE_FLAGS_SHIFT)

#define NINED3DSI_TEXLD_PROJECT 0x1
#define NINED3DSI_TEXLD_BIAS    0x2

#define NINED3DSP_WRITEMASK_0   0x1
#define NINED3DSP_WRITEMASK_1   0x2
#define NINED3DSP_WRITEMASK_2   0x4
#define NINED3DSP_WRITEMASK_3   0x8
#define NINED3DSP_WRITEMASK_ALL 0xf

#define NINED3DSP_NOSWIZZLE ((0 << 0) | (1 << 2) | (2 << 4) | (3 << 6))

#define NINE_SWIZZLE4(x,y,z,w) \
   TGSI_SWIZZLE_##x, TGSI_SWIZZLE_##y, TGSI_SWIZZLE_##z, TGSI_SWIZZLE_##w

#define NINE_CONSTANT_SRC(index) \
   ureg_src_dimension(ureg_src_register(TGSI_FILE_CONSTANT, index), 0)

#define NINE_APPLY_SWIZZLE(src, s) \
   ureg_swizzle(src, NINE_SWIZZLE4(s, s, s, s))

#define NINE_CONSTANT_SRC_SWIZZLE(index, s) \
   NINE_APPLY_SWIZZLE(NINE_CONSTANT_SRC(index), s)

#define NINED3DSPDM_SATURATE (D3DSPDM_SATURATE >> D3DSP_DSTMOD_SHIFT)
#define NINED3DSPDM_PARTIALP (D3DSPDM_PARTIALPRECISION >> D3DSP_DSTMOD_SHIFT)
#define NINED3DSPDM_CENTROID (D3DSPDM_MSAMPCENTROID >> D3DSP_DSTMOD_SHIFT)

/*
 * NEG     all, not ps: m3x2, m3x3, m3x4, m4x3, m4x4
 * BIAS    <= PS 1.4 (x-0.5)
 * BIASNEG <= PS 1.4 (-(x-0.5))
 * SIGN    <= PS 1.4 (2(x-0.5))
 * SIGNNEG <= PS 1.4 (-2(x-0.5))
 * COMP    <= PS 1.4 (1-x)
 * X2       = PS 1.4 (2x)
 * X2NEG    = PS 1.4 (-2x)
 * DZ      <= PS 1.4, tex{ld,crd} (.xy/.z), z=0 => .11
 * DW      <= PS 1.4, tex{ld,crd} (.xy/.w), w=0 => .11
 * ABS     >= SM 3.0 (abs(x))
 * ABSNEG  >= SM 3.0 (-abs(x))
 * NOT     >= SM 2.0 pedication only
 */
#define NINED3DSPSM_NONE    (D3DSPSM_NONE    >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_NEG     (D3DSPSM_NEG     >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_BIAS    (D3DSPSM_BIAS    >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_BIASNEG (D3DSPSM_BIASNEG >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_SIGN    (D3DSPSM_SIGN    >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_SIGNNEG (D3DSPSM_SIGNNEG >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_COMP    (D3DSPSM_COMP    >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_X2      (D3DSPSM_X2      >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_X2NEG   (D3DSPSM_X2NEG   >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_DZ      (D3DSPSM_DZ      >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_DW      (D3DSPSM_DW      >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_ABS     (D3DSPSM_ABS     >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_ABSNEG  (D3DSPSM_ABSNEG  >> D3DSP_SRCMOD_SHIFT)
#define NINED3DSPSM_NOT     (D3DSPSM_NOT     >> D3DSP_SRCMOD_SHIFT)

static const char *sm1_mod_str[] =
{
    [NINED3DSPSM_NONE] = "",
    [NINED3DSPSM_NEG] = "-",
    [NINED3DSPSM_BIAS] = "bias",
    [NINED3DSPSM_BIASNEG] = "biasneg",
    [NINED3DSPSM_SIGN] = "sign",
    [NINED3DSPSM_SIGNNEG] = "signneg",
    [NINED3DSPSM_COMP] = "comp",
    [NINED3DSPSM_X2] = "x2",
    [NINED3DSPSM_X2NEG] = "x2neg",
    [NINED3DSPSM_DZ] = "dz",
    [NINED3DSPSM_DW] = "dw",
    [NINED3DSPSM_ABS] = "abs",
    [NINED3DSPSM_ABSNEG] = "-abs",
    [NINED3DSPSM_NOT] = "not"
};

static void
sm1_dump_writemask(BYTE mask)
{
    if (mask & 1) DUMP("x"); else DUMP("_");
    if (mask & 2) DUMP("y"); else DUMP("_");
    if (mask & 4) DUMP("z"); else DUMP("_");
    if (mask & 8) DUMP("w"); else DUMP("_");
}

static void
sm1_dump_swizzle(BYTE s)
{
    char c[4] = { 'x', 'y', 'z', 'w' };
    DUMP("%c%c%c%c",
         c[(s >> 0) & 3], c[(s >> 2) & 3], c[(s >> 4) & 3], c[(s >> 6) & 3]);
}

static const char sm1_file_char[] =
{
    [D3DSPR_TEMP] = 'r',
    [D3DSPR_INPUT] = 'v',
    [D3DSPR_CONST] = 'c',
    [D3DSPR_ADDR] = 'A',
    [D3DSPR_RASTOUT] = 'R',
    [D3DSPR_ATTROUT] = 'D',
    [D3DSPR_OUTPUT] = 'o',
    [D3DSPR_CONSTINT] = 'I',
    [D3DSPR_COLOROUT] = 'C',
    [D3DSPR_DEPTHOUT] = 'D',
    [D3DSPR_SAMPLER] = 's',
    [D3DSPR_CONST2] = 'c',
    [D3DSPR_CONST3] = 'c',
    [D3DSPR_CONST4] = 'c',
    [D3DSPR_CONSTBOOL] = 'B',
    [D3DSPR_LOOP] = 'L',
    [D3DSPR_TEMPFLOAT16] = 'h',
    [D3DSPR_MISCTYPE] = 'M',
    [D3DSPR_LABEL] = 'X',
    [D3DSPR_PREDICATE] = 'p'
};

static void
sm1_dump_reg(BYTE file, INT index)
{
    switch (file) {
    case D3DSPR_LOOP:
        DUMP("aL");
        break;
    case D3DSPR_COLOROUT:
        DUMP("oC%i", index);
        break;
    case D3DSPR_DEPTHOUT:
        DUMP("oDepth");
        break;
    case D3DSPR_RASTOUT:
        DUMP("oRast%i", index);
        break;
    case D3DSPR_CONSTINT:
        DUMP("iconst[%i]", index);
        break;
    case D3DSPR_CONSTBOOL:
        DUMP("bconst[%i]", index);
        break;
    default:
        DUMP("%c%i", sm1_file_char[file], index);
        break;
    }
}

struct sm1_src_param
{
    INT idx;
    struct sm1_src_param *rel;
    BYTE file;
    BYTE swizzle;
    BYTE mod;
    BYTE type;
    union {
        DWORD d[4];
        float f[4];
        int i[4];
        BOOL b;
    } imm;
};
static void
sm1_parse_immediate(struct shader_translator *, struct sm1_src_param *);

struct sm1_dst_param
{
    INT idx;
    struct sm1_src_param *rel;
    BYTE file;
    BYTE mask;
    BYTE mod;
    int8_t shift; /* sint4 */
    BYTE type;
};

static inline void
assert_replicate_swizzle(const struct ureg_src *reg)
{
    assert(reg->SwizzleY == reg->SwizzleX &&
           reg->SwizzleZ == reg->SwizzleX &&
           reg->SwizzleW == reg->SwizzleX);
}

static void
sm1_dump_immediate(const struct sm1_src_param *param)
{
    switch (param->type) {
    case NINED3DSPTYPE_FLOAT4:
        DUMP("{ %f %f %f %f }",
             param->imm.f[0], param->imm.f[1],
             param->imm.f[2], param->imm.f[3]);
        break;
    case NINED3DSPTYPE_INT4:
        DUMP("{ %i %i %i %i }",
             param->imm.i[0], param->imm.i[1],
             param->imm.i[2], param->imm.i[3]);
        break;
    case NINED3DSPTYPE_BOOL:
        DUMP("%s", param->imm.b ? "TRUE" : "FALSE");
        break;
    default:
        assert(0);
        break;
    }
}

static void
sm1_dump_src_param(const struct sm1_src_param *param)
{
    if (param->file == NINED3DSPR_IMMEDIATE) {
        assert(!param->mod &&
               !param->rel &&
               param->swizzle == NINED3DSP_NOSWIZZLE);
        sm1_dump_immediate(param);
        return;
    }

    if (param->mod)
        DUMP("%s(", sm1_mod_str[param->mod]);
    if (param->rel) {
        DUMP("%c[", sm1_file_char[param->file]);
        sm1_dump_src_param(param->rel);
        DUMP("+%i]", param->idx);
    } else {
        sm1_dump_reg(param->file, param->idx);
    }
    if (param->mod)
       DUMP(")");
    if (param->swizzle != NINED3DSP_NOSWIZZLE) {
       DUMP(".");
       sm1_dump_swizzle(param->swizzle);
    }
}

static void
sm1_dump_dst_param(const struct sm1_dst_param *param)
{
   if (param->mod & NINED3DSPDM_SATURATE)
      DUMP("sat ");
   if (param->mod & NINED3DSPDM_PARTIALP)
      DUMP("pp ");
   if (param->mod & NINED3DSPDM_CENTROID)
      DUMP("centroid ");
   if (param->shift < 0)
      DUMP("/%u ", 1 << -param->shift);
   if (param->shift > 0)
      DUMP("*%u ", 1 << param->shift);

   if (param->rel) {
      DUMP("%c[", sm1_file_char[param->file]);
      sm1_dump_src_param(param->rel);
      DUMP("+%i]", param->idx);
   } else {
      sm1_dump_reg(param->file, param->idx);
   }
   if (param->mask != NINED3DSP_WRITEMASK_ALL) {
      DUMP(".");
      sm1_dump_writemask(param->mask);
   }
}

struct sm1_semantic
{
   struct sm1_dst_param reg;
   BYTE sampler_type;
   D3DDECLUSAGE usage;
   BYTE usage_idx;
};

struct sm1_op_info
{
    /* NOTE: 0 is a valid TGSI opcode, but if handler is set, this parameter
     * should be ignored completely */
    unsigned sio;
    unsigned opcode; /* TGSI_OPCODE_x */

    /* versions are still set even handler is set */
    struct {
        unsigned min;
        unsigned max;
    } vert_version, frag_version;

    /* number of regs parsed outside of special handler */
    unsigned ndst;
    unsigned nsrc;

    /* some instructions don't map perfectly, so use a special handler */
    translate_instruction_func handler;
};

struct sm1_instruction
{
    D3DSHADER_INSTRUCTION_OPCODE_TYPE opcode;
    BYTE flags;
    BOOL coissue;
    BOOL predicated;
    BYTE ndst;
    BYTE nsrc;
    struct sm1_src_param src[4];
    struct sm1_src_param src_rel[4];
    struct sm1_src_param pred;
    struct sm1_src_param dst_rel[1];
    struct sm1_dst_param dst[1];

    struct sm1_op_info *info;
};

static void
sm1_dump_instruction(struct sm1_instruction *insn, unsigned indent)
{
    unsigned i;

    /* no info stored for these: */
    if (insn->opcode == D3DSIO_DCL)
        return;
    for (i = 0; i < indent; ++i)
        DUMP("  ");

    if (insn->predicated) {
        DUMP("@");
        sm1_dump_src_param(&insn->pred);
        DUMP(" ");
    }
    DUMP("%s", d3dsio_to_string(insn->opcode));
    if (insn->flags) {
        switch (insn->opcode) {
        case D3DSIO_TEX:
            DUMP(insn->flags == NINED3DSI_TEXLD_PROJECT ? "p" : "b");
            break;
        default:
            DUMP("_%x", insn->flags);
            break;
        }
    }
    if (insn->coissue)
        DUMP("_co");
    DUMP(" ");

    for (i = 0; i < insn->ndst && i < ARRAY_SIZE(insn->dst); ++i) {
        sm1_dump_dst_param(&insn->dst[i]);
        DUMP(" ");
    }

    for (i = 0; i < insn->nsrc && i < ARRAY_SIZE(insn->src); ++i) {
        sm1_dump_src_param(&insn->src[i]);
        DUMP(" ");
    }
    if (insn->opcode == D3DSIO_DEF ||
        insn->opcode == D3DSIO_DEFI ||
        insn->opcode == D3DSIO_DEFB)
        sm1_dump_immediate(&insn->src[0]);

    DUMP("\n");
}

struct sm1_local_const
{
    INT idx;
    struct ureg_src reg;
    float f[4]; /* for indirect addressing of float constants */
};

struct shader_translator
{
    const DWORD *byte_code;
    const DWORD *parse;
    const DWORD *parse_next;

    struct ureg_program *ureg;

    /* shader version */
    struct {
        BYTE major;
        BYTE minor;
    } version;
    unsigned processor; /* PIPE_SHADER_VERTEX/FRAMGENT */
    unsigned num_constf_allowed;
    unsigned num_consti_allowed;
    unsigned num_constb_allowed;

    boolean native_integers;
    boolean inline_subroutines;
    boolean want_texcoord;
    boolean shift_wpos;
    boolean wpos_is_sysval;
    boolean face_is_sysval_integer;
    unsigned texcoord_sn;

    struct sm1_instruction insn; /* current instruction */

    struct {
        struct ureg_dst *r;
        struct ureg_dst oPos;
        struct ureg_dst oPos_out; /* the real output when doing streamout */
        struct ureg_dst oFog;
        struct ureg_dst oPts;
        struct ureg_dst oCol[4];
        struct ureg_dst o[PIPE_MAX_SHADER_OUTPUTS];
        struct ureg_dst oDepth;
        struct ureg_src v[PIPE_MAX_SHADER_INPUTS];
        struct ureg_src v_consecutive; /* copy in temp array of ps inputs for rel addressing */
        struct ureg_src vPos;
        struct ureg_src vFace;
        struct ureg_src s;
        struct ureg_dst p;
        struct ureg_dst address;
        struct ureg_dst a0;
        struct ureg_dst tS[8]; /* texture stage registers */
        struct ureg_dst tdst; /* scratch dst if we need extra modifiers */
        struct ureg_dst t[5]; /* scratch TEMPs */
        struct ureg_src vC[2]; /* PS color in */
        struct ureg_src vT[8]; /* PS texcoord in */
        struct ureg_dst rL[NINE_MAX_LOOP_DEPTH]; /* loop ctr */
    } regs;
    unsigned num_temp; /* ARRAY_SIZE(regs.r) */
    unsigned num_scratch;
    unsigned loop_depth;
    unsigned loop_depth_max;
    unsigned cond_depth;
    unsigned loop_labels[NINE_MAX_LOOP_DEPTH];
    unsigned cond_labels[NINE_MAX_COND_DEPTH];
    boolean loop_or_rep[NINE_MAX_LOOP_DEPTH]; /* true: loop, false: rep */

    unsigned *inst_labels; /* LABEL op */
    unsigned num_inst_labels;

    unsigned sampler_targets[NINE_MAX_SAMPLERS]; /* TGSI_TEXTURE_x */

    struct sm1_local_const *lconstf;
    unsigned num_lconstf;
    struct sm1_local_const *lconsti;
    unsigned num_lconsti;
    struct sm1_local_const *lconstb;
    unsigned num_lconstb;

    boolean indirect_const_access;
    boolean failure;

    struct nine_vs_output_info output_info[16];
    int num_outputs;

    struct nine_shader_info *info;

    int16_t op_info_map[D3DSIO_BREAKP + 1];
};

#define IS_VS (tx->processor == PIPE_SHADER_VERTEX)
#define IS_PS (tx->processor == PIPE_SHADER_FRAGMENT)

#define FAILURE_VOID(cond) if ((cond)) {tx->failure=1;return;}

static void
sm1_read_semantic(struct shader_translator *, struct sm1_semantic *);

static void
sm1_instruction_check(const struct sm1_instruction *insn)
{
    if (insn->opcode == D3DSIO_CRS)
    {
        if (insn->dst[0].mask & NINED3DSP_WRITEMASK_3)
        {
            DBG("CRS.mask.w\n");
        }
    }
}

static void
nine_record_outputs(struct shader_translator *tx, BYTE Usage, BYTE UsageIndex,
                    int mask, int output_index)
{
    tx->output_info[tx->num_outputs].output_semantic = Usage;
    tx->output_info[tx->num_outputs].output_semantic_index = UsageIndex;
    tx->output_info[tx->num_outputs].mask = mask;
    tx->output_info[tx->num_outputs].output_index = output_index;
    tx->num_outputs++;
}

static boolean
tx_lconstf(struct shader_translator *tx, struct ureg_src *src, INT index)
{
   INT i;

   if (index < 0 || index >= tx->num_constf_allowed) {
       tx->failure = TRUE;
       return FALSE;
   }
   for (i = 0; i < tx->num_lconstf; ++i) {
      if (tx->lconstf[i].idx == index) {
         *src = tx->lconstf[i].reg;
         return TRUE;
      }
   }
   return FALSE;
}
static boolean
tx_lconsti(struct shader_translator *tx, struct ureg_src *src, INT index)
{
   int i;

   if (index < 0 || index >= tx->num_consti_allowed) {
       tx->failure = TRUE;
       return FALSE;
   }
   for (i = 0; i < tx->num_lconsti; ++i) {
      if (tx->lconsti[i].idx == index) {
         *src = tx->lconsti[i].reg;
         return TRUE;
      }
   }
   return FALSE;
}
static boolean
tx_lconstb(struct shader_translator *tx, struct ureg_src *src, INT index)
{
   int i;

   if (index < 0 || index >= tx->num_constb_allowed) {
       tx->failure = TRUE;
       return FALSE;
   }
   for (i = 0; i < tx->num_lconstb; ++i) {
      if (tx->lconstb[i].idx == index) {
         *src = tx->lconstb[i].reg;
         return TRUE;
      }
   }
   return FALSE;
}

static void
tx_set_lconstf(struct shader_translator *tx, INT index, float f[4])
{
    unsigned n;

    FAILURE_VOID(index < 0 || index >= tx->num_constf_allowed)

    for (n = 0; n < tx->num_lconstf; ++n)
        if (tx->lconstf[n].idx == index)
            break;
    if (n == tx->num_lconstf) {
       if ((n % 8) == 0) {
          tx->lconstf = REALLOC(tx->lconstf,
                                (n + 0) * sizeof(tx->lconstf[0]),
                                (n + 8) * sizeof(tx->lconstf[0]));
          assert(tx->lconstf);
       }
       tx->num_lconstf++;
    }
    tx->lconstf[n].idx = index;
    tx->lconstf[n].reg = ureg_imm4f(tx->ureg, f[0], f[1], f[2], f[3]);

    memcpy(tx->lconstf[n].f, f, sizeof(tx->lconstf[n].f));
}
static void
tx_set_lconsti(struct shader_translator *tx, INT index, int i[4])
{
    unsigned n;

    FAILURE_VOID(index < 0 || index >= tx->num_consti_allowed)

    for (n = 0; n < tx->num_lconsti; ++n)
        if (tx->lconsti[n].idx == index)
            break;
    if (n == tx->num_lconsti) {
       if ((n % 8) == 0) {
          tx->lconsti = REALLOC(tx->lconsti,
                                (n + 0) * sizeof(tx->lconsti[0]),
                                (n + 8) * sizeof(tx->lconsti[0]));
          assert(tx->lconsti);
       }
       tx->num_lconsti++;
    }

    tx->lconsti[n].idx = index;
    tx->lconsti[n].reg = tx->native_integers ?
       ureg_imm4i(tx->ureg, i[0], i[1], i[2], i[3]) :
       ureg_imm4f(tx->ureg, i[0], i[1], i[2], i[3]);
}
static void
tx_set_lconstb(struct shader_translator *tx, INT index, BOOL b)
{
    unsigned n;

    FAILURE_VOID(index < 0 || index >= tx->num_constb_allowed)

    for (n = 0; n < tx->num_lconstb; ++n)
        if (tx->lconstb[n].idx == index)
            break;
    if (n == tx->num_lconstb) {
       if ((n % 8) == 0) {
          tx->lconstb = REALLOC(tx->lconstb,
                                (n + 0) * sizeof(tx->lconstb[0]),
                                (n + 8) * sizeof(tx->lconstb[0]));
          assert(tx->lconstb);
       }
       tx->num_lconstb++;
    }

    tx->lconstb[n].idx = index;
    tx->lconstb[n].reg = tx->native_integers ?
       ureg_imm1u(tx->ureg, b ? 0xffffffff : 0) :
       ureg_imm1f(tx->ureg, b ? 1.0f : 0.0f);
}

static inline struct ureg_dst
tx_scratch(struct shader_translator *tx)
{
    if (tx->num_scratch >= ARRAY_SIZE(tx->regs.t)) {
        tx->failure = TRUE;
        return tx->regs.t[0];
    }
    if (ureg_dst_is_undef(tx->regs.t[tx->num_scratch]))
        tx->regs.t[tx->num_scratch] = ureg_DECL_local_temporary(tx->ureg);
    return tx->regs.t[tx->num_scratch++];
}

static inline struct ureg_dst
tx_scratch_scalar(struct shader_translator *tx)
{
    return ureg_writemask(tx_scratch(tx), TGSI_WRITEMASK_X);
}

static inline struct ureg_src
tx_src_scalar(struct ureg_dst dst)
{
    struct ureg_src src = ureg_src(dst);
    int c = ffs(dst.WriteMask) - 1;
    if (dst.WriteMask == (1 << c))
        src = ureg_scalar(src, c);
    return src;
}

static inline void
tx_temp_alloc(struct shader_translator *tx, INT idx)
{
    assert(idx >= 0);
    if (idx >= tx->num_temp) {
       unsigned k = tx->num_temp;
       unsigned n = idx + 1;
       tx->regs.r = REALLOC(tx->regs.r,
                            k * sizeof(tx->regs.r[0]),
                            n * sizeof(tx->regs.r[0]));
       for (; k < n; ++k)
          tx->regs.r[k] = ureg_dst_undef();
       tx->num_temp = n;
    }
    if (ureg_dst_is_undef(tx->regs.r[idx]))
        tx->regs.r[idx] = ureg_DECL_temporary(tx->ureg);
}

static inline void
tx_addr_alloc(struct shader_translator *tx, INT idx)
{
    assert(idx == 0);
    if (ureg_dst_is_undef(tx->regs.address))
        tx->regs.address = ureg_DECL_address(tx->ureg);
    if (ureg_dst_is_undef(tx->regs.a0))
        tx->regs.a0 = ureg_DECL_temporary(tx->ureg);
}

/* NOTE: It's not very clear on which ps1.1-ps1.3 instructions
 * the projection should be applied on the texture. It doesn't
 * apply on texkill.
 * The doc is very imprecise here (it says the projection is done
 * before rasterization, thus in vs, which seems wrong since ps instructions
 * are affected differently)
 * For now we only apply to the ps TEX instruction and TEXBEM.
 * Perhaps some other instructions would need it */
static inline void
apply_ps1x_projection(struct shader_translator *tx, struct ureg_dst dst,
                      struct ureg_src src, INT idx)
{
    struct ureg_dst tmp;
    unsigned dim = 1 + ((tx->info->projected >> (2 * idx)) & 3);

    /* no projection */
    if (dim == 1) {
        ureg_MOV(tx->ureg, dst, src);
    } else {
        tmp = tx_scratch_scalar(tx);
        ureg_RCP(tx->ureg, tmp, ureg_scalar(src, dim-1));
        ureg_MUL(tx->ureg, dst, tx_src_scalar(tmp), src);
    }
}

static inline void
TEX_with_ps1x_projection(struct shader_translator *tx, struct ureg_dst dst,
                         unsigned target, struct ureg_src src0,
                         struct ureg_src src1, INT idx)
{
    unsigned dim = 1 + ((tx->info->projected >> (2 * idx)) & 3);
    struct ureg_dst tmp;

    /* dim == 1: no projection
     * Looks like must be disabled when it makes no
     * sense according the texture dimensions
     */
    if (dim == 1 || dim <= target) {
        ureg_TEX(tx->ureg, dst, target, src0, src1);
    } else if (dim == 4) {
        ureg_TXP(tx->ureg, dst, target, src0, src1);
    } else {
        tmp = tx_scratch(tx);
        apply_ps1x_projection(tx, tmp, src0, idx);
        ureg_TEX(tx->ureg, dst, target, ureg_src(tmp), src1);
    }
}

static inline void
tx_texcoord_alloc(struct shader_translator *tx, INT idx)
{
    assert(IS_PS);
    assert(idx >= 0 && idx < ARRAY_SIZE(tx->regs.vT));
    if (ureg_src_is_undef(tx->regs.vT[idx]))
       tx->regs.vT[idx] = ureg_DECL_fs_input(tx->ureg, tx->texcoord_sn, idx,
                                             TGSI_INTERPOLATE_PERSPECTIVE);
}

static inline unsigned *
tx_bgnloop(struct shader_translator *tx)
{
    tx->loop_depth++;
    if (tx->loop_depth_max < tx->loop_depth)
        tx->loop_depth_max = tx->loop_depth;
    assert(tx->loop_depth < NINE_MAX_LOOP_DEPTH);
    return &tx->loop_labels[tx->loop_depth - 1];
}

static inline unsigned *
tx_endloop(struct shader_translator *tx)
{
    assert(tx->loop_depth);
    tx->loop_depth--;
    ureg_fixup_label(tx->ureg, tx->loop_labels[tx->loop_depth],
                     ureg_get_instruction_number(tx->ureg));
    return &tx->loop_labels[tx->loop_depth];
}

static struct ureg_dst
tx_get_loopctr(struct shader_translator *tx, boolean loop_or_rep)
{
    const unsigned l = tx->loop_depth - 1;

    if (!tx->loop_depth)
    {
        DBG("loop counter requested outside of loop\n");
        return ureg_dst_undef();
    }

    if (ureg_dst_is_undef(tx->regs.rL[l])) {
        /* loop or rep ctr creation */
        tx->regs.rL[l] = ureg_DECL_local_temporary(tx->ureg);
        tx->loop_or_rep[l] = loop_or_rep;
    }
    /* loop - rep - endloop - endrep not allowed */
    assert(tx->loop_or_rep[l] == loop_or_rep);

    return tx->regs.rL[l];
}

static struct ureg_src
tx_get_loopal(struct shader_translator *tx)
{
    int loop_level = tx->loop_depth - 1;

    while (loop_level >= 0) {
        /* handle loop - rep - endrep - endloop case */
        if (tx->loop_or_rep[loop_level])
            /* the value is in the loop counter y component (nine implementation) */
            return ureg_scalar(ureg_src(tx->regs.rL[loop_level]), TGSI_SWIZZLE_Y);
        loop_level--;
    }

    DBG("aL counter requested outside of loop\n");
    return ureg_src_undef();
}

static inline unsigned *
tx_cond(struct shader_translator *tx)
{
   assert(tx->cond_depth <= NINE_MAX_COND_DEPTH);
   tx->cond_depth++;
   return &tx->cond_labels[tx->cond_depth - 1];
}

static inline unsigned *
tx_elsecond(struct shader_translator *tx)
{
   assert(tx->cond_depth);
   return &tx->cond_labels[tx->cond_depth - 1];
}

static inline void
tx_endcond(struct shader_translator *tx)
{
   assert(tx->cond_depth);
   tx->cond_depth--;
   ureg_fixup_label(tx->ureg, tx->cond_labels[tx->cond_depth],
                    ureg_get_instruction_number(tx->ureg));
}

static inline struct ureg_dst
nine_ureg_dst_register(unsigned file, int index)
{
    return ureg_dst(ureg_src_register(file, index));
}

static inline struct ureg_src
nine_get_position_input(struct shader_translator *tx)
{
    struct ureg_program *ureg = tx->ureg;

    if (tx->wpos_is_sysval)
        return ureg_DECL_system_value(ureg, TGSI_SEMANTIC_POSITION, 0);
    else
        return ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_POSITION,
                                  0, TGSI_INTERPOLATE_LINEAR);
}

static struct ureg_src
tx_src_param(struct shader_translator *tx, const struct sm1_src_param *param)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_src src;
    struct ureg_dst tmp;

    switch (param->file)
    {
    case D3DSPR_TEMP:
        assert(!param->rel);
        tx_temp_alloc(tx, param->idx);
        src = ureg_src(tx->regs.r[param->idx]);
        break;
 /* case D3DSPR_TEXTURE: == D3DSPR_ADDR */
    case D3DSPR_ADDR:
        assert(!param->rel);
        if (IS_VS) {
            assert(param->idx == 0);
            /* the address register (vs only) must be
             * assigned before use */
            assert(!ureg_dst_is_undef(tx->regs.a0));
            /* Round to lowest for vs1.1 (contrary to the doc), else
             * round to nearest */
            if (tx->version.major < 2 && tx->version.minor < 2)
                ureg_ARL(ureg, tx->regs.address, ureg_src(tx->regs.a0));
            else
                ureg_ARR(ureg, tx->regs.address, ureg_src(tx->regs.a0));
            src = ureg_src(tx->regs.address);
        } else {
            if (tx->version.major < 2 && tx->version.minor < 4) {
                /* no subroutines, so should be defined */
                src = ureg_src(tx->regs.tS[param->idx]);
            } else {
                tx_texcoord_alloc(tx, param->idx);
                src = tx->regs.vT[param->idx];
            }
        }
        break;
    case D3DSPR_INPUT:
        if (IS_VS) {
            src = ureg_src_register(TGSI_FILE_INPUT, param->idx);
        } else {
            if (tx->version.major < 3) {
                assert(!param->rel);
                src = ureg_DECL_fs_input_cyl_centroid(
                    ureg, TGSI_SEMANTIC_COLOR, param->idx,
                    TGSI_INTERPOLATE_COLOR, 0,
                    tx->info->force_color_in_centroid ?
                      TGSI_INTERPOLATE_LOC_CENTROID : 0,
                    0, 1);
            } else {
                if(param->rel) {
                    /* Copy all inputs (non consecutive)
                     * to temp array (consecutive).
                     * This is not good for performance.
                     * A better way would be to have inputs
                     * consecutive (would need implement alternative
                     * way to match vs outputs and ps inputs).
                     * However even with the better way, the temp array
                     * copy would need to be used if some inputs
                     * are not GENERIC or if they have different
                     * interpolation flag. */
                    if (ureg_src_is_undef(tx->regs.v_consecutive)) {
                        int i;
                        tx->regs.v_consecutive = ureg_src(ureg_DECL_array_temporary(ureg, 10, 0));
                        for (i = 0; i < 10; i++) {
                            if (!ureg_src_is_undef(tx->regs.v[i]))
                                ureg_MOV(ureg, ureg_dst_array_offset(ureg_dst(tx->regs.v_consecutive), i), tx->regs.v[i]);
                            else
                                ureg_MOV(ureg, ureg_dst_array_offset(ureg_dst(tx->regs.v_consecutive), i), ureg_imm4f(ureg, 0.0f, 0.0f, 0.0f, 1.0f));
                        }
                    }
                    src = ureg_src_array_offset(tx->regs.v_consecutive, param->idx);
                } else {
                    assert(param->idx < ARRAY_SIZE(tx->regs.v));
                    src = tx->regs.v[param->idx];
                }
            }
        }
        break;
    case D3DSPR_PREDICATE:
        assert(!"D3DSPR_PREDICATE");
        break;
    case D3DSPR_SAMPLER:
        assert(param->mod == NINED3DSPSM_NONE);
        assert(param->swizzle == NINED3DSP_NOSWIZZLE);
        assert(!param->rel);
        src = ureg_src_register(TGSI_FILE_SAMPLER, param->idx);
        break;
    case D3DSPR_CONST:
        assert(!param->rel || IS_VS);
        if (param->rel)
            tx->indirect_const_access = TRUE;
        if (param->rel || !tx_lconstf(tx, &src, param->idx)) {
            if (!param->rel)
                nine_info_mark_const_f_used(tx->info, param->idx);
            /* vswp constant handling: we use two buffers
             * to fit all the float constants. The special handling
             * doesn't need to be elsewhere, because all the instructions
             * accessing the constants directly are VS1, and swvp
             * is VS >= 2 */
            if (IS_VS && tx->info->swvp_on) {
                if (!param->rel) {
                    if (param->idx < 4096) {
                        src = ureg_src_register(TGSI_FILE_CONSTANT, param->idx);
                        src = ureg_src_dimension(src, 0);
                    } else {
                        src = ureg_src_register(TGSI_FILE_CONSTANT, param->idx - 4096);
                        src = ureg_src_dimension(src, 1);
                    }
                } else {
                    src = ureg_src_register(TGSI_FILE_CONSTANT, param->idx); /* TODO: swvp rel > 4096 */
                    src = ureg_src_dimension(src, 0);
                }
            } else
                src = NINE_CONSTANT_SRC(param->idx);
        }
        if (!IS_VS && tx->version.major < 2) {
            /* ps 1.X clamps constants */
            tmp = tx_scratch(tx);
            ureg_MIN(ureg, tmp, src, ureg_imm1f(ureg, 1.0f));
            ureg_MAX(ureg, tmp, ureg_src(tmp), ureg_imm1f(ureg, -1.0f));
            src = ureg_src(tmp);
        }
        break;
    case D3DSPR_CONST2:
    case D3DSPR_CONST3:
    case D3DSPR_CONST4:
        DBG("CONST2/3/4 should have been collapsed into D3DSPR_CONST !\n");
        assert(!"CONST2/3/4");
        src = ureg_imm1f(ureg, 0.0f);
        break;
    case D3DSPR_CONSTINT:
        /* relative adressing only possible for float constants in vs */
        assert(!param->rel);
        if (!tx_lconsti(tx, &src, param->idx)) {
            nine_info_mark_const_i_used(tx->info, param->idx);
            if (IS_VS && tx->info->swvp_on) {
                src = ureg_src_register(TGSI_FILE_CONSTANT, param->idx);
                src = ureg_src_dimension(src, 2);
            } else
                src = NINE_CONSTANT_SRC(tx->info->const_i_base + param->idx);
        }
        break;
    case D3DSPR_CONSTBOOL:
        assert(!param->rel);
        if (!tx_lconstb(tx, &src, param->idx)) {
           char r = param->idx / 4;
           char s = param->idx & 3;
           nine_info_mark_const_b_used(tx->info, param->idx);
           if (IS_VS && tx->info->swvp_on) {
               src = ureg_src_register(TGSI_FILE_CONSTANT, r);
               src = ureg_src_dimension(src, 3);
           } else
               src = NINE_CONSTANT_SRC(tx->info->const_b_base + r);
           src = ureg_swizzle(src, s, s, s, s);
        }
        break;
    case D3DSPR_LOOP:
        if (ureg_dst_is_undef(tx->regs.address))
            tx->regs.address = ureg_DECL_address(ureg);
        if (!tx->native_integers)
            ureg_ARR(ureg, tx->regs.address, tx_get_loopal(tx));
        else
            ureg_UARL(ureg, tx->regs.address, tx_get_loopal(tx));
        src = ureg_src(tx->regs.address);
        break;
    case D3DSPR_MISCTYPE:
        switch (param->idx) {
        case D3DSMO_POSITION:
           if (ureg_src_is_undef(tx->regs.vPos))
              tx->regs.vPos = nine_get_position_input(tx);
           if (tx->shift_wpos) {
               /* TODO: do this only once */
               struct ureg_dst wpos = tx_scratch(tx);
               ureg_ADD(ureg, wpos, tx->regs.vPos,
                        ureg_imm4f(ureg, -0.5f, -0.5f, 0.0f, 0.0f));
               src = ureg_src(wpos);
           } else {
               src = tx->regs.vPos;
           }
           break;
        case D3DSMO_FACE:
           if (ureg_src_is_undef(tx->regs.vFace)) {
               if (tx->face_is_sysval_integer) {
                   tmp = ureg_DECL_temporary(ureg);
                   tx->regs.vFace =
                       ureg_DECL_system_value(ureg, TGSI_SEMANTIC_FACE, 0);

                   /* convert bool to float */
                   ureg_UCMP(ureg, tmp, ureg_scalar(tx->regs.vFace, TGSI_SWIZZLE_X),
                             ureg_imm1f(ureg, 1), ureg_imm1f(ureg, -1));
                   tx->regs.vFace = ureg_src(tmp);
               } else {
                   tx->regs.vFace = ureg_DECL_fs_input(ureg,
                                                       TGSI_SEMANTIC_FACE, 0,
                                                       TGSI_INTERPOLATE_CONSTANT);
               }
               tx->regs.vFace = ureg_scalar(tx->regs.vFace, TGSI_SWIZZLE_X);
           }
           src = tx->regs.vFace;
           break;
        default:
            assert(!"invalid src D3DSMO");
            break;
        }
        assert(!param->rel);
        break;
    case D3DSPR_TEMPFLOAT16:
        break;
    default:
        assert(!"invalid src D3DSPR");
    }
    if (param->rel)
        src = ureg_src_indirect(src, tx_src_param(tx, param->rel));

    switch (param->mod) {
    case NINED3DSPSM_DW:
        tmp = tx_scratch(tx);
        /* NOTE: app is not allowed to read w with this modifier */
        ureg_RCP(ureg, ureg_writemask(tmp, NINED3DSP_WRITEMASK_3), ureg_scalar(src, TGSI_SWIZZLE_W));
        ureg_MUL(ureg, tmp, src, ureg_swizzle(ureg_src(tmp), NINE_SWIZZLE4(W,W,W,W)));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_DZ:
        tmp = tx_scratch(tx);
        /* NOTE: app is not allowed to read z with this modifier */
        ureg_RCP(ureg, ureg_writemask(tmp, NINED3DSP_WRITEMASK_2), ureg_scalar(src, TGSI_SWIZZLE_Z));
        ureg_MUL(ureg, tmp, src, ureg_swizzle(ureg_src(tmp), NINE_SWIZZLE4(Z,Z,Z,Z)));
        src = ureg_src(tmp);
        break;
    default:
        break;
    }

    if (param->swizzle != NINED3DSP_NOSWIZZLE)
        src = ureg_swizzle(src,
                           (param->swizzle >> 0) & 0x3,
                           (param->swizzle >> 2) & 0x3,
                           (param->swizzle >> 4) & 0x3,
                           (param->swizzle >> 6) & 0x3);

    switch (param->mod) {
    case NINED3DSPSM_ABS:
        src = ureg_abs(src);
        break;
    case NINED3DSPSM_ABSNEG:
        src = ureg_negate(ureg_abs(src));
        break;
    case NINED3DSPSM_NEG:
        src = ureg_negate(src);
        break;
    case NINED3DSPSM_BIAS:
        tmp = tx_scratch(tx);
        ureg_ADD(ureg, tmp, src, ureg_imm1f(ureg, -0.5f));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_BIASNEG:
        tmp = tx_scratch(tx);
        ureg_ADD(ureg, tmp, ureg_imm1f(ureg, 0.5f), ureg_negate(src));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_NOT:
        if (tx->native_integers) {
            tmp = tx_scratch(tx);
            ureg_NOT(ureg, tmp, src);
            src = ureg_src(tmp);
            break;
        }
        /* fall through */
    case NINED3DSPSM_COMP:
        tmp = tx_scratch(tx);
        ureg_ADD(ureg, tmp, ureg_imm1f(ureg, 1.0f), ureg_negate(src));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_DZ:
    case NINED3DSPSM_DW:
        /* Already handled*/
        break;
    case NINED3DSPSM_SIGN:
        tmp = tx_scratch(tx);
        ureg_MAD(ureg, tmp, src, ureg_imm1f(ureg, 2.0f), ureg_imm1f(ureg, -1.0f));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_SIGNNEG:
        tmp = tx_scratch(tx);
        ureg_MAD(ureg, tmp, src, ureg_imm1f(ureg, -2.0f), ureg_imm1f(ureg, 1.0f));
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_X2:
        tmp = tx_scratch(tx);
        ureg_ADD(ureg, tmp, src, src);
        src = ureg_src(tmp);
        break;
    case NINED3DSPSM_X2NEG:
        tmp = tx_scratch(tx);
        ureg_ADD(ureg, tmp, src, src);
        src = ureg_negate(ureg_src(tmp));
        break;
    default:
        assert(param->mod == NINED3DSPSM_NONE);
        break;
    }

    return src;
}

static struct ureg_dst
_tx_dst_param(struct shader_translator *tx, const struct sm1_dst_param *param)
{
    struct ureg_dst dst;

    switch (param->file)
    {
    case D3DSPR_TEMP:
        assert(!param->rel);
        tx_temp_alloc(tx, param->idx);
        dst = tx->regs.r[param->idx];
        break;
 /* case D3DSPR_TEXTURE: == D3DSPR_ADDR */
    case D3DSPR_ADDR:
        assert(!param->rel);
        if (tx->version.major < 2 && !IS_VS) {
            if (ureg_dst_is_undef(tx->regs.tS[param->idx]))
                tx->regs.tS[param->idx] = ureg_DECL_temporary(tx->ureg);
            dst = tx->regs.tS[param->idx];
        } else
        if (!IS_VS && tx->insn.opcode == D3DSIO_TEXKILL) { /* maybe others, too */
            tx_texcoord_alloc(tx, param->idx);
            dst = ureg_dst(tx->regs.vT[param->idx]);
        } else {
            tx_addr_alloc(tx, param->idx);
            dst = tx->regs.a0;
        }
        break;
    case D3DSPR_RASTOUT:
        assert(!param->rel);
        switch (param->idx) {
        case 0:
            if (ureg_dst_is_undef(tx->regs.oPos))
                tx->regs.oPos =
                    ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_POSITION, 0);
            dst = tx->regs.oPos;
            break;
        case 1:
            if (ureg_dst_is_undef(tx->regs.oFog))
                tx->regs.oFog =
                    ureg_saturate(ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_FOG, 0));
            dst = tx->regs.oFog;
            break;
        case 2:
            if (ureg_dst_is_undef(tx->regs.oPts))
                tx->regs.oPts = ureg_DECL_temporary(tx->ureg);
            dst = tx->regs.oPts;
            break;
        default:
            assert(0);
            break;
        }
        break;
 /* case D3DSPR_TEXCRDOUT: == D3DSPR_OUTPUT */
    case D3DSPR_OUTPUT:
        if (tx->version.major < 3) {
            assert(!param->rel);
            dst = ureg_DECL_output(tx->ureg, tx->texcoord_sn, param->idx);
        } else {
            assert(!param->rel); /* TODO */
            assert(param->idx < ARRAY_SIZE(tx->regs.o));
            dst = tx->regs.o[param->idx];
        }
        break;
    case D3DSPR_ATTROUT: /* VS */
    case D3DSPR_COLOROUT: /* PS */
        assert(param->idx >= 0 && param->idx < 4);
        assert(!param->rel);
        tx->info->rt_mask |= 1 << param->idx;
        if (ureg_dst_is_undef(tx->regs.oCol[param->idx])) {
            /* ps < 3: oCol[0] will have fog blending afterward */
            if (!IS_VS && tx->version.major < 3 && param->idx == 0) {
                tx->regs.oCol[0] = ureg_DECL_temporary(tx->ureg);
            } else {
                tx->regs.oCol[param->idx] =
                    ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_COLOR, param->idx);
            }
        }
        dst = tx->regs.oCol[param->idx];
        if (IS_VS && tx->version.major < 3)
            dst = ureg_saturate(dst);
        break;
    case D3DSPR_DEPTHOUT:
        assert(!param->rel);
        if (ureg_dst_is_undef(tx->regs.oDepth))
           tx->regs.oDepth =
              ureg_DECL_output_masked(tx->ureg, TGSI_SEMANTIC_POSITION, 0,
                                      TGSI_WRITEMASK_Z, 0, 1);
        dst = tx->regs.oDepth; /* XXX: must write .z component */
        break;
    case D3DSPR_PREDICATE:
        assert(!"D3DSPR_PREDICATE");
        break;
    case D3DSPR_TEMPFLOAT16:
        DBG("unhandled D3DSPR: %u\n", param->file);
        break;
    default:
        assert(!"invalid dst D3DSPR");
        break;
    }
    if (param->rel)
        dst = ureg_dst_indirect(dst, tx_src_param(tx, param->rel));

    if (param->mask != NINED3DSP_WRITEMASK_ALL)
        dst = ureg_writemask(dst, param->mask);
    if (param->mod & NINED3DSPDM_SATURATE)
        dst = ureg_saturate(dst);

    return dst;
}

static struct ureg_dst
tx_dst_param(struct shader_translator *tx, const struct sm1_dst_param *param)
{
    if (param->shift) {
        tx->regs.tdst = ureg_writemask(tx_scratch(tx), param->mask);
        return tx->regs.tdst;
    }
    return _tx_dst_param(tx, param);
}

static void
tx_apply_dst0_modifiers(struct shader_translator *tx)
{
    struct ureg_dst rdst;
    float f;

    if (!tx->insn.ndst || !tx->insn.dst[0].shift || tx->insn.opcode == D3DSIO_TEXKILL)
        return;
    rdst = _tx_dst_param(tx, &tx->insn.dst[0]);

    assert(rdst.File != TGSI_FILE_ADDRESS); /* this probably isn't possible */

    if (tx->insn.dst[0].shift < 0)
        f = 1.0f / (1 << -tx->insn.dst[0].shift);
    else
        f = 1 << tx->insn.dst[0].shift;

    ureg_MUL(tx->ureg, rdst, ureg_src(tx->regs.tdst), ureg_imm1f(tx->ureg, f));
}

static struct ureg_src
tx_dst_param_as_src(struct shader_translator *tx, const struct sm1_dst_param *param)
{
    struct ureg_src src;

    assert(!param->shift);
    assert(!(param->mod & NINED3DSPDM_SATURATE));

    switch (param->file) {
    case D3DSPR_INPUT:
        if (IS_VS) {
            src = ureg_src_register(TGSI_FILE_INPUT, param->idx);
        } else {
            assert(!param->rel);
            assert(param->idx < ARRAY_SIZE(tx->regs.v));
            src = tx->regs.v[param->idx];
        }
        break;
    default:
        src = ureg_src(tx_dst_param(tx, param));
        break;
    }
    if (param->rel)
        src = ureg_src_indirect(src, tx_src_param(tx, param->rel));

    if (!param->mask)
        WARN("mask is 0, using identity swizzle\n");

    if (param->mask && param->mask != NINED3DSP_WRITEMASK_ALL) {
        char s[4];
        int n;
        int c;
        for (n = 0, c = 0; c < 4; ++c)
            if (param->mask & (1 << c))
                s[n++] = c;
        assert(n);
        for (c = n; c < 4; ++c)
            s[c] = s[n - 1];
        src = ureg_swizzle(src, s[0], s[1], s[2], s[3]);
    }
    return src;
}

static HRESULT
NineTranslateInstruction_Mkxn(struct shader_translator *tx, const unsigned k, const unsigned n)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst;
    struct ureg_src src[2];
    struct sm1_src_param *src_mat = &tx->insn.src[1];
    unsigned i;

    dst = tx_dst_param(tx, &tx->insn.dst[0]);
    src[0] = tx_src_param(tx, &tx->insn.src[0]);

    for (i = 0; i < n; i++)
    {
        const unsigned m = (1 << i);

        src[1] = tx_src_param(tx, src_mat);
        src_mat->idx++;

        if (!(dst.WriteMask & m))
            continue;

        /* XXX: src == dst case ? */

        switch (k) {
        case 3:
            ureg_DP3(ureg, ureg_writemask(dst, m), src[0], src[1]);
            break;
        case 4:
            ureg_DP4(ureg, ureg_writemask(dst, m), src[0], src[1]);
            break;
        default:
            DBG("invalid operation: M%ux%u\n", m, n);
            break;
        }
    }

    return D3D_OK;
}

#define VNOTSUPPORTED   0, 0
#define V(maj, min)     (((maj) << 8) | (min))

static inline const char *
d3dsio_to_string( unsigned opcode )
{
    static const char *names[] = {
        "NOP",
        "MOV",
        "ADD",
        "SUB",
        "MAD",
        "MUL",
        "RCP",
        "RSQ",
        "DP3",
        "DP4",
        "MIN",
        "MAX",
        "SLT",
        "SGE",
        "EXP",
        "LOG",
        "LIT",
        "DST",
        "LRP",
        "FRC",
        "M4x4",
        "M4x3",
        "M3x4",
        "M3x3",
        "M3x2",
        "CALL",
        "CALLNZ",
        "LOOP",
        "RET",
        "ENDLOOP",
        "LABEL",
        "DCL",
        "POW",
        "CRS",
        "SGN",
        "ABS",
        "NRM",
        "SINCOS",
        "REP",
        "ENDREP",
        "IF",
        "IFC",
        "ELSE",
        "ENDIF",
        "BREAK",
        "BREAKC",
        "MOVA",
        "DEFB",
        "DEFI",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "TEXCOORD",
        "TEXKILL",
        "TEX",
        "TEXBEM",
        "TEXBEML",
        "TEXREG2AR",
        "TEXREG2GB",
        "TEXM3x2PAD",
        "TEXM3x2TEX",
        "TEXM3x3PAD",
        "TEXM3x3TEX",
        NULL,
        "TEXM3x3SPEC",
        "TEXM3x3VSPEC",
        "EXPP",
        "LOGP",
        "CND",
        "DEF",
        "TEXREG2RGB",
        "TEXDP3TEX",
        "TEXM3x2DEPTH",
        "TEXDP3",
        "TEXM3x3",
        "TEXDEPTH",
        "CMP",
        "BEM",
        "DP2ADD",
        "DSX",
        "DSY",
        "TEXLDD",
        "SETP",
        "TEXLDL",
        "BREAKP"
    };

    if (opcode < ARRAY_SIZE(names)) return names[opcode];

    switch (opcode) {
    case D3DSIO_PHASE: return "PHASE";
    case D3DSIO_COMMENT: return "COMMENT";
    case D3DSIO_END: return "END";
    default:
        return NULL;
    }
}

#define NULL_INSTRUCTION            { 0, { 0, 0 }, { 0, 0 }, 0, 0, NULL }
#define IS_VALID_INSTRUCTION(inst)  ((inst).vert_version.min | \
                                     (inst).vert_version.max | \
                                     (inst).frag_version.min | \
                                     (inst).frag_version.max)

#define SPECIAL(name) \
    NineTranslateInstruction_##name

#define DECL_SPECIAL(name) \
    static HRESULT \
    NineTranslateInstruction_##name( struct shader_translator *tx )

static HRESULT
NineTranslateInstruction_Generic(struct shader_translator *);

DECL_SPECIAL(NOP)
{
    /* Nothing to do. NOP was used to avoid hangs
     * with very old d3d drivers. */
    return D3D_OK;
}

DECL_SPECIAL(SUB)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src0 = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_src src1 = tx_src_param(tx, &tx->insn.src[1]);

    ureg_ADD(ureg, dst, src0, ureg_negate(src1));
    return D3D_OK;
}

DECL_SPECIAL(ABS)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);

    ureg_MOV(ureg, dst, ureg_abs(src));
    return D3D_OK;
}

DECL_SPECIAL(XPD)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src0 = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_src src1 = tx_src_param(tx, &tx->insn.src[1]);

    ureg_MUL(ureg, ureg_writemask(dst, TGSI_WRITEMASK_XYZ),
             ureg_swizzle(src0, TGSI_SWIZZLE_Y, TGSI_SWIZZLE_Z,
                          TGSI_SWIZZLE_X, 0),
             ureg_swizzle(src1, TGSI_SWIZZLE_Z, TGSI_SWIZZLE_X,
                          TGSI_SWIZZLE_Y, 0));
    ureg_MAD(ureg, ureg_writemask(dst, TGSI_WRITEMASK_XYZ),
             ureg_swizzle(src0, TGSI_SWIZZLE_Z, TGSI_SWIZZLE_X,
                          TGSI_SWIZZLE_Y, 0),
             ureg_negate(ureg_swizzle(src1, TGSI_SWIZZLE_Y,
                                      TGSI_SWIZZLE_Z, TGSI_SWIZZLE_X, 0)),
             ureg_src(dst));
    ureg_MOV(ureg, ureg_writemask(dst, TGSI_WRITEMASK_W),
             ureg_imm1f(ureg, 1));
    return D3D_OK;
}

DECL_SPECIAL(M4x4)
{
    return NineTranslateInstruction_Mkxn(tx, 4, 4);
}

DECL_SPECIAL(M4x3)
{
    return NineTranslateInstruction_Mkxn(tx, 4, 3);
}

DECL_SPECIAL(M3x4)
{
    return NineTranslateInstruction_Mkxn(tx, 3, 4);
}

DECL_SPECIAL(M3x3)
{
    return NineTranslateInstruction_Mkxn(tx, 3, 3);
}

DECL_SPECIAL(M3x2)
{
    return NineTranslateInstruction_Mkxn(tx, 3, 2);
}

DECL_SPECIAL(CMP)
{
    ureg_CMP(tx->ureg, tx_dst_param(tx, &tx->insn.dst[0]),
             tx_src_param(tx, &tx->insn.src[0]),
             tx_src_param(tx, &tx->insn.src[2]),
             tx_src_param(tx, &tx->insn.src[1]));
    return D3D_OK;
}

DECL_SPECIAL(CND)
{
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_dst cgt;
    struct ureg_src cnd;

    /* the coissue flag was a tip for compilers to advise to
     * execute two operations at the same time, in cases
     * the two executions had same dst with different channels.
     * It has no effect on current hw. However it seems CND
     * is affected. The handling of this very specific case
     * handled below mimick wine behaviour */
    if (tx->insn.coissue && tx->version.major == 1 && tx->version.minor < 4 && tx->insn.dst[0].mask != NINED3DSP_WRITEMASK_3) {
        ureg_MOV(tx->ureg,
                 dst, tx_src_param(tx, &tx->insn.src[1]));
        return D3D_OK;
    }

    cnd = tx_src_param(tx, &tx->insn.src[0]);
    cgt = tx_scratch(tx);

    if (tx->version.major == 1 && tx->version.minor < 4)
        cnd = ureg_scalar(cnd, TGSI_SWIZZLE_W);

    ureg_SGT(tx->ureg, cgt, cnd, ureg_imm1f(tx->ureg, 0.5f));

    ureg_CMP(tx->ureg, dst, ureg_negate(ureg_src(cgt)),
             tx_src_param(tx, &tx->insn.src[1]),
             tx_src_param(tx, &tx->insn.src[2]));
    return D3D_OK;
}

DECL_SPECIAL(CALL)
{
    assert(tx->insn.src[0].idx < tx->num_inst_labels);
    ureg_CAL(tx->ureg, &tx->inst_labels[tx->insn.src[0].idx]);
    return D3D_OK;
}

DECL_SPECIAL(CALLNZ)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[1]);

    if (!tx->native_integers)
        ureg_IF(ureg, src, tx_cond(tx));
    else
        ureg_UIF(ureg, src, tx_cond(tx));
    ureg_CAL(ureg, &tx->inst_labels[tx->insn.src[0].idx]);
    tx_endcond(tx);
    ureg_ENDIF(ureg);
    return D3D_OK;
}

DECL_SPECIAL(LOOP)
{
    struct ureg_program *ureg = tx->ureg;
    unsigned *label;
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[1]);
    struct ureg_dst ctr;
    struct ureg_dst tmp;
    struct ureg_src ctrx;

    label = tx_bgnloop(tx);
    ctr = tx_get_loopctr(tx, TRUE);
    ctrx = ureg_scalar(ureg_src(ctr), TGSI_SWIZZLE_X);

    /* src: num_iterations - start_value of al - step for al - 0 */
    ureg_MOV(ureg, ctr, src);
    ureg_BGNLOOP(tx->ureg, label);
    tmp = tx_scratch_scalar(tx);
    /* Initially ctr.x contains the number of iterations.
     * ctr.y will contain the updated value of al.
     * We decrease ctr.x at the end of every iteration,
     * and stop when it reaches 0. */

    if (!tx->native_integers) {
        /* case src and ctr contain floats */
        /* to avoid precision issue, we stop when ctr <= 0.5 */
        ureg_SGE(ureg, tmp, ureg_imm1f(ureg, 0.5f), ctrx);
        ureg_IF(ureg, tx_src_scalar(tmp), tx_cond(tx));
    } else {
        /* case src and ctr contain integers */
        ureg_ISGE(ureg, tmp, ureg_imm1i(ureg, 0), ctrx);
        ureg_UIF(ureg, tx_src_scalar(tmp), tx_cond(tx));
    }
    ureg_BRK(ureg);
    tx_endcond(tx);
    ureg_ENDIF(ureg);
    return D3D_OK;
}

DECL_SPECIAL(RET)
{
    ureg_RET(tx->ureg);
    return D3D_OK;
}

DECL_SPECIAL(ENDLOOP)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst ctr = tx_get_loopctr(tx, TRUE);
    struct ureg_dst dst_ctrx, dst_al;
    struct ureg_src src_ctr, al_counter;

    dst_ctrx = ureg_writemask(ctr, NINED3DSP_WRITEMASK_0);
    dst_al = ureg_writemask(ctr, NINED3DSP_WRITEMASK_1);
    src_ctr = ureg_src(ctr);
    al_counter = ureg_scalar(src_ctr, TGSI_SWIZZLE_Z);

    /* ctr.x -= 1
     * ctr.y (aL) += step */
    if (!tx->native_integers) {
        ureg_ADD(ureg, dst_ctrx, src_ctr, ureg_imm1f(ureg, -1.0f));
        ureg_ADD(ureg, dst_al, src_ctr, al_counter);
    } else {
        ureg_UADD(ureg, dst_ctrx, src_ctr, ureg_imm1i(ureg, -1));
        ureg_UADD(ureg, dst_al, src_ctr, al_counter);
    }
    ureg_ENDLOOP(tx->ureg, tx_endloop(tx));
    return D3D_OK;
}

DECL_SPECIAL(LABEL)
{
    unsigned k = tx->num_inst_labels;
    unsigned n = tx->insn.src[0].idx;
    assert(n < 2048);
    if (n >= k)
       tx->inst_labels = REALLOC(tx->inst_labels,
                                 k * sizeof(tx->inst_labels[0]),
                                 n * sizeof(tx->inst_labels[0]));

    tx->inst_labels[n] = ureg_get_instruction_number(tx->ureg);
    return D3D_OK;
}

DECL_SPECIAL(SINCOS)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);

    assert(!(dst.WriteMask & 0xc));

    /* z undefined, w untouched */
    ureg_COS(ureg, ureg_writemask(dst, TGSI_WRITEMASK_X),
             ureg_scalar(src, TGSI_SWIZZLE_X));
    ureg_SIN(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Y),
             ureg_scalar(src, TGSI_SWIZZLE_X));
    return D3D_OK;
}

DECL_SPECIAL(SGN)
{
    ureg_SSG(tx->ureg,
             tx_dst_param(tx, &tx->insn.dst[0]),
             tx_src_param(tx, &tx->insn.src[0]));
    return D3D_OK;
}

DECL_SPECIAL(REP)
{
    struct ureg_program *ureg = tx->ureg;
    unsigned *label;
    struct ureg_src rep = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_dst ctr;
    struct ureg_dst tmp;
    struct ureg_src ctrx;

    label = tx_bgnloop(tx);
    ctr = ureg_writemask(tx_get_loopctr(tx, FALSE), NINED3DSP_WRITEMASK_0);
    ctrx = ureg_scalar(ureg_src(ctr), TGSI_SWIZZLE_X);

    /* NOTE: rep must be constant, so we don't have to save the count */
    assert(rep.File == TGSI_FILE_CONSTANT || rep.File == TGSI_FILE_IMMEDIATE);

    /* rep: num_iterations - 0 - 0 - 0 */
    ureg_MOV(ureg, ctr, rep);
    ureg_BGNLOOP(ureg, label);
    tmp = tx_scratch_scalar(tx);
    /* Initially ctr.x contains the number of iterations.
     * We decrease ctr.x at the end of every iteration,
     * and stop when it reaches 0. */

    if (!tx->native_integers) {
        /* case src and ctr contain floats */
        /* to avoid precision issue, we stop when ctr <= 0.5 */
        ureg_SGE(ureg, tmp, ureg_imm1f(ureg, 0.5f), ctrx);
        ureg_IF(ureg, tx_src_scalar(tmp), tx_cond(tx));
    } else {
        /* case src and ctr contain integers */
        ureg_ISGE(ureg, tmp, ureg_imm1i(ureg, 0), ctrx);
        ureg_UIF(ureg, tx_src_scalar(tmp), tx_cond(tx));
    }
    ureg_BRK(ureg);
    tx_endcond(tx);
    ureg_ENDIF(ureg);

    return D3D_OK;
}

DECL_SPECIAL(ENDREP)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst ctr = tx_get_loopctr(tx, FALSE);
    struct ureg_dst dst_ctrx = ureg_writemask(ctr, NINED3DSP_WRITEMASK_0);
    struct ureg_src src_ctr = ureg_src(ctr);

    /* ctr.x -= 1 */
    if (!tx->native_integers)
        ureg_ADD(ureg, dst_ctrx, src_ctr, ureg_imm1f(ureg, -1.0f));
    else
        ureg_UADD(ureg, dst_ctrx, src_ctr, ureg_imm1i(ureg, -1));

    ureg_ENDLOOP(tx->ureg, tx_endloop(tx));
    return D3D_OK;
}

DECL_SPECIAL(ENDIF)
{
    tx_endcond(tx);
    ureg_ENDIF(tx->ureg);
    return D3D_OK;
}

DECL_SPECIAL(IF)
{
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);

    if (tx->native_integers && tx->insn.src[0].file == D3DSPR_CONSTBOOL)
        ureg_UIF(tx->ureg, src, tx_cond(tx));
    else
        ureg_IF(tx->ureg, src, tx_cond(tx));

    return D3D_OK;
}

static inline unsigned
sm1_insn_flags_to_tgsi_setop(BYTE flags)
{
    switch (flags) {
    case NINED3DSHADER_REL_OP_GT: return TGSI_OPCODE_SGT;
    case NINED3DSHADER_REL_OP_EQ: return TGSI_OPCODE_SEQ;
    case NINED3DSHADER_REL_OP_GE: return TGSI_OPCODE_SGE;
    case NINED3DSHADER_REL_OP_LT: return TGSI_OPCODE_SLT;
    case NINED3DSHADER_REL_OP_NE: return TGSI_OPCODE_SNE;
    case NINED3DSHADER_REL_OP_LE: return TGSI_OPCODE_SLE;
    default:
        assert(!"invalid comparison flags");
        return TGSI_OPCODE_SGT;
    }
}

DECL_SPECIAL(IFC)
{
    const unsigned cmp_op = sm1_insn_flags_to_tgsi_setop(tx->insn.flags);
    struct ureg_src src[2];
    struct ureg_dst tmp = ureg_writemask(tx_scratch(tx), TGSI_WRITEMASK_X);
    src[0] = tx_src_param(tx, &tx->insn.src[0]);
    src[1] = tx_src_param(tx, &tx->insn.src[1]);
    ureg_insn(tx->ureg, cmp_op, &tmp, 1, src, 2, 0);
    ureg_IF(tx->ureg, ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), tx_cond(tx));
    return D3D_OK;
}

DECL_SPECIAL(ELSE)
{
    ureg_ELSE(tx->ureg, tx_elsecond(tx));
    return D3D_OK;
}

DECL_SPECIAL(BREAKC)
{
    const unsigned cmp_op = sm1_insn_flags_to_tgsi_setop(tx->insn.flags);
    struct ureg_src src[2];
    struct ureg_dst tmp = ureg_writemask(tx_scratch(tx), TGSI_WRITEMASK_X);
    src[0] = tx_src_param(tx, &tx->insn.src[0]);
    src[1] = tx_src_param(tx, &tx->insn.src[1]);
    ureg_insn(tx->ureg, cmp_op, &tmp, 1, src, 2, 0);
    ureg_IF(tx->ureg, ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), tx_cond(tx));
    ureg_BRK(tx->ureg);
    tx_endcond(tx);
    ureg_ENDIF(tx->ureg);
    return D3D_OK;
}

static const char *sm1_declusage_names[] =
{
    [D3DDECLUSAGE_POSITION] = "POSITION",
    [D3DDECLUSAGE_BLENDWEIGHT] = "BLENDWEIGHT",
    [D3DDECLUSAGE_BLENDINDICES] = "BLENDINDICES",
    [D3DDECLUSAGE_NORMAL] = "NORMAL",
    [D3DDECLUSAGE_PSIZE] = "PSIZE",
    [D3DDECLUSAGE_TEXCOORD] = "TEXCOORD",
    [D3DDECLUSAGE_TANGENT] = "TANGENT",
    [D3DDECLUSAGE_BINORMAL] = "BINORMAL",
    [D3DDECLUSAGE_TESSFACTOR] = "TESSFACTOR",
    [D3DDECLUSAGE_POSITIONT] = "POSITIONT",
    [D3DDECLUSAGE_COLOR] = "COLOR",
    [D3DDECLUSAGE_FOG] = "FOG",
    [D3DDECLUSAGE_DEPTH] = "DEPTH",
    [D3DDECLUSAGE_SAMPLE] = "SAMPLE"
};

static inline unsigned
sm1_to_nine_declusage(struct sm1_semantic *dcl)
{
    return nine_d3d9_to_nine_declusage(dcl->usage, dcl->usage_idx);
}

static void
sm1_declusage_to_tgsi(struct tgsi_declaration_semantic *sem,
                      boolean tc,
                      struct sm1_semantic *dcl)
{
    BYTE index = dcl->usage_idx;

    /* For everything that is not matching to a TGSI_SEMANTIC_****,
     * we match to a TGSI_SEMANTIC_GENERIC with index.
     *
     * The index can be anything UINT16 and usage_idx is BYTE,
     * so we can fit everything. It doesn't matter if indices
     * are close together or low.
     *
     *
     * POSITION >= 1: 10 * index + 6
     * COLOR >= 2: 10 * (index-1) + 7
     * TEXCOORD[0..15]: index
     * BLENDWEIGHT: 10 * index + 18
     * BLENDINDICES: 10 * index + 19
     * NORMAL: 10 * index + 20
     * TANGENT: 10 * index + 21
     * BINORMAL: 10 * index + 22
     * TESSFACTOR: 10 * index + 23
     */

    switch (dcl->usage) {
    case D3DDECLUSAGE_POSITION:
    case D3DDECLUSAGE_POSITIONT:
    case D3DDECLUSAGE_DEPTH:
        if (index == 0) {
            sem->Name = TGSI_SEMANTIC_POSITION;
            sem->Index = 0;
        } else {
            sem->Name = TGSI_SEMANTIC_GENERIC;
            sem->Index = 10 * index + 6;
        }
        break;
    case D3DDECLUSAGE_COLOR:
        if (index < 2) {
            sem->Name = TGSI_SEMANTIC_COLOR;
            sem->Index = index;
        } else {
            sem->Name = TGSI_SEMANTIC_GENERIC;
            sem->Index = 10 * (index-1) + 7;
        }
        break;
    case D3DDECLUSAGE_FOG:
        assert(index == 0);
        sem->Name = TGSI_SEMANTIC_FOG;
        sem->Index = 0;
        break;
    case D3DDECLUSAGE_PSIZE:
        assert(index == 0);
        sem->Name = TGSI_SEMANTIC_PSIZE;
        sem->Index = 0;
        break;
    case D3DDECLUSAGE_TEXCOORD:
        assert(index < 16);
        if (index < 8 && tc)
            sem->Name = TGSI_SEMANTIC_TEXCOORD;
        else
            sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = index;
        break;
    case D3DDECLUSAGE_BLENDWEIGHT:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 18;
        break;
    case D3DDECLUSAGE_BLENDINDICES:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 19;
        break;
    case D3DDECLUSAGE_NORMAL:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 20;
        break;
    case D3DDECLUSAGE_TANGENT:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 21;
        break;
    case D3DDECLUSAGE_BINORMAL:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 22;
        break;
    case D3DDECLUSAGE_TESSFACTOR:
        sem->Name = TGSI_SEMANTIC_GENERIC;
        sem->Index = 10 * index + 23;
        break;
    case D3DDECLUSAGE_SAMPLE:
        sem->Name = TGSI_SEMANTIC_COUNT;
        sem->Index = 0;
        break;
    default:
        unreachable("Invalid DECLUSAGE.");
        break;
    }
}

#define NINED3DSTT_1D     (D3DSTT_1D >> D3DSP_TEXTURETYPE_SHIFT)
#define NINED3DSTT_2D     (D3DSTT_2D >> D3DSP_TEXTURETYPE_SHIFT)
#define NINED3DSTT_VOLUME (D3DSTT_VOLUME >> D3DSP_TEXTURETYPE_SHIFT)
#define NINED3DSTT_CUBE   (D3DSTT_CUBE >> D3DSP_TEXTURETYPE_SHIFT)
static inline unsigned
d3dstt_to_tgsi_tex(BYTE sampler_type)
{
    switch (sampler_type) {
    case NINED3DSTT_1D:     return TGSI_TEXTURE_1D;
    case NINED3DSTT_2D:     return TGSI_TEXTURE_2D;
    case NINED3DSTT_VOLUME: return TGSI_TEXTURE_3D;
    case NINED3DSTT_CUBE:   return TGSI_TEXTURE_CUBE;
    default:
        assert(0);
        return TGSI_TEXTURE_UNKNOWN;
    }
}
static inline unsigned
d3dstt_to_tgsi_tex_shadow(BYTE sampler_type)
{
    switch (sampler_type) {
    case NINED3DSTT_1D: return TGSI_TEXTURE_SHADOW1D;
    case NINED3DSTT_2D: return TGSI_TEXTURE_SHADOW2D;
    case NINED3DSTT_VOLUME:
    case NINED3DSTT_CUBE:
    default:
        assert(0);
        return TGSI_TEXTURE_UNKNOWN;
    }
}
static inline unsigned
ps1x_sampler_type(const struct nine_shader_info *info, unsigned stage)
{
    switch ((info->sampler_ps1xtypes >> (stage * 2)) & 0x3) {
    case 1: return TGSI_TEXTURE_1D;
    case 0: return TGSI_TEXTURE_2D;
    case 3: return TGSI_TEXTURE_3D;
    default:
        return TGSI_TEXTURE_CUBE;
    }
}

static const char *
sm1_sampler_type_name(BYTE sampler_type)
{
    switch (sampler_type) {
    case NINED3DSTT_1D:     return "1D";
    case NINED3DSTT_2D:     return "2D";
    case NINED3DSTT_VOLUME: return "VOLUME";
    case NINED3DSTT_CUBE:   return "CUBE";
    default:
        return "(D3DSTT_?)";
    }
}

static inline unsigned
nine_tgsi_to_interp_mode(struct tgsi_declaration_semantic *sem)
{
    switch (sem->Name) {
    case TGSI_SEMANTIC_POSITION:
    case TGSI_SEMANTIC_NORMAL:
        return TGSI_INTERPOLATE_LINEAR;
    case TGSI_SEMANTIC_BCOLOR:
    case TGSI_SEMANTIC_COLOR:
        return TGSI_INTERPOLATE_COLOR;
    case TGSI_SEMANTIC_FOG:
    case TGSI_SEMANTIC_GENERIC:
    case TGSI_SEMANTIC_TEXCOORD:
    case TGSI_SEMANTIC_CLIPDIST:
    case TGSI_SEMANTIC_CLIPVERTEX:
        return TGSI_INTERPOLATE_PERSPECTIVE;
    case TGSI_SEMANTIC_EDGEFLAG:
    case TGSI_SEMANTIC_FACE:
    case TGSI_SEMANTIC_INSTANCEID:
    case TGSI_SEMANTIC_PCOORD:
    case TGSI_SEMANTIC_PRIMID:
    case TGSI_SEMANTIC_PSIZE:
    case TGSI_SEMANTIC_VERTEXID:
        return TGSI_INTERPOLATE_CONSTANT;
    default:
        assert(0);
        return TGSI_INTERPOLATE_CONSTANT;
    }
}

DECL_SPECIAL(DCL)
{
    struct ureg_program *ureg = tx->ureg;
    boolean is_input;
    boolean is_sampler;
    struct tgsi_declaration_semantic tgsi;
    struct sm1_semantic sem;
    sm1_read_semantic(tx, &sem);

    is_input = sem.reg.file == D3DSPR_INPUT;
    is_sampler =
        sem.usage == D3DDECLUSAGE_SAMPLE || sem.reg.file == D3DSPR_SAMPLER;

    DUMP("DCL ");
    sm1_dump_dst_param(&sem.reg);
    if (is_sampler)
        DUMP(" %s\n", sm1_sampler_type_name(sem.sampler_type));
    else
    if (tx->version.major >= 3)
        DUMP(" %s%i\n", sm1_declusage_names[sem.usage], sem.usage_idx);
    else
    if (sem.usage | sem.usage_idx)
        DUMP(" %u[%u]\n", sem.usage, sem.usage_idx);
    else
        DUMP("\n");

    if (is_sampler) {
        const unsigned m = 1 << sem.reg.idx;
        ureg_DECL_sampler(ureg, sem.reg.idx);
        tx->info->sampler_mask |= m;
        tx->sampler_targets[sem.reg.idx] = (tx->info->sampler_mask_shadow & m) ?
            d3dstt_to_tgsi_tex_shadow(sem.sampler_type) :
            d3dstt_to_tgsi_tex(sem.sampler_type);
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
        ureg_DECL_sampler_view(ureg, sem.reg.idx, tx->sampler_targets[sem.reg.idx],
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
        return D3D_OK;
    }

    sm1_declusage_to_tgsi(&tgsi, tx->want_texcoord, &sem);
    if (IS_VS) {
        if (is_input) {
            /* linkage outside of shader with vertex declaration */
            ureg_DECL_vs_input(ureg, sem.reg.idx);
            assert(sem.reg.idx < ARRAY_SIZE(tx->info->input_map));
            tx->info->input_map[sem.reg.idx] = sm1_to_nine_declusage(&sem);
            tx->info->num_inputs = MAX2(tx->info->num_inputs, sem.reg.idx + 1);
            /* NOTE: preserving order in case of indirect access */
        } else
        if (tx->version.major >= 3) {
            /* SM2 output semantic determined by file */
            assert(sem.reg.mask != 0);
            if (sem.usage == D3DDECLUSAGE_POSITIONT)
                tx->info->position_t = TRUE;
            assert(sem.reg.idx < ARRAY_SIZE(tx->regs.o));
            assert(ureg_dst_is_undef(tx->regs.o[sem.reg.idx]) && "Nine doesn't support yet packing");
            tx->regs.o[sem.reg.idx] = ureg_DECL_output_masked(
                ureg, tgsi.Name, tgsi.Index, sem.reg.mask, 0, 1);
            nine_record_outputs(tx, sem.usage, sem.usage_idx, sem.reg.mask, sem.reg.idx);
            if (tx->info->process_vertices && sem.usage == D3DDECLUSAGE_POSITION && sem.usage_idx == 0) {
                tx->regs.oPos_out = tx->regs.o[sem.reg.idx];
                tx->regs.o[sem.reg.idx] = ureg_DECL_temporary(ureg);
                tx->regs.oPos = tx->regs.o[sem.reg.idx];
            }

            if (tgsi.Name == TGSI_SEMANTIC_PSIZE) {
                tx->regs.o[sem.reg.idx] = ureg_DECL_temporary(ureg);
                tx->regs.oPts = tx->regs.o[sem.reg.idx];
            }
        }
    } else {
        if (is_input && tx->version.major >= 3) {
            unsigned interp_location = 0;
            /* SM3 only, SM2 input semantic determined by file */
            assert(sem.reg.idx < ARRAY_SIZE(tx->regs.v));
            assert(ureg_src_is_undef(tx->regs.v[sem.reg.idx]) && "Nine doesn't support yet packing");
            /* PositionT and tessfactor forbidden */
            if (sem.usage == D3DDECLUSAGE_POSITIONT || sem.usage == D3DDECLUSAGE_TESSFACTOR)
                return D3DERR_INVALIDCALL;

            if (tgsi.Name == TGSI_SEMANTIC_POSITION) {
                /* Position0 is forbidden (likely because vPos already does that) */
                if (sem.usage == D3DDECLUSAGE_POSITION)
                    return D3DERR_INVALIDCALL;
                /* Following code is for depth */
                tx->regs.v[sem.reg.idx] = nine_get_position_input(tx);
                return D3D_OK;
            }

            if (sem.reg.mod & NINED3DSPDM_CENTROID ||
                (tgsi.Name == TGSI_SEMANTIC_COLOR && tx->info->force_color_in_centroid))
                interp_location = TGSI_INTERPOLATE_LOC_CENTROID;

            tx->regs.v[sem.reg.idx] = ureg_DECL_fs_input_cyl_centroid(
                ureg, tgsi.Name, tgsi.Index,
                nine_tgsi_to_interp_mode(&tgsi),
                0, /* cylwrap */
                interp_location, 0, 1);
        } else
        if (!is_input && 0) { /* declare in COLOROUT/DEPTHOUT case */
            /* FragColor or FragDepth */
            assert(sem.reg.mask != 0);
            ureg_DECL_output_masked(ureg, tgsi.Name, tgsi.Index, sem.reg.mask,
                                    0, 1);
        }
    }
    return D3D_OK;
}

DECL_SPECIAL(DEF)
{
    tx_set_lconstf(tx, tx->insn.dst[0].idx, tx->insn.src[0].imm.f);
    return D3D_OK;
}

DECL_SPECIAL(DEFB)
{
    tx_set_lconstb(tx, tx->insn.dst[0].idx, tx->insn.src[0].imm.b);
    return D3D_OK;
}

DECL_SPECIAL(DEFI)
{
    tx_set_lconsti(tx, tx->insn.dst[0].idx, tx->insn.src[0].imm.i);
    return D3D_OK;
}

DECL_SPECIAL(POW)
{
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[2] = {
        tx_src_param(tx, &tx->insn.src[0]),
        tx_src_param(tx, &tx->insn.src[1])
    };
    ureg_POW(tx->ureg, dst, ureg_abs(src[0]), src[1]);
    return D3D_OK;
}

DECL_SPECIAL(RSQ)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_dst tmp = tx_scratch(tx);
    ureg_RSQ(ureg, tmp, ureg_abs(src));
    ureg_MIN(ureg, dst, ureg_imm1f(ureg, FLT_MAX), ureg_src(tmp));
    return D3D_OK;
}

DECL_SPECIAL(LOG)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst tmp = tx_scratch_scalar(tx);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    ureg_LG2(ureg, tmp, ureg_abs(src));
    ureg_MAX(ureg, dst, ureg_imm1f(ureg, -FLT_MAX), tx_src_scalar(tmp));
    return D3D_OK;
}

DECL_SPECIAL(LIT)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst tmp = tx_scratch(tx);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    ureg_LIT(ureg, tmp, src);
    /* d3d9 LIT is the same than gallium LIT. One difference is that d3d9
     * states that dst.z is 0 when src.y <= 0. Gallium definition can assign
     * it 0^0 if src.w=0, which value is driver dependent. */
    ureg_CMP(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Z),
             ureg_negate(ureg_scalar(src, TGSI_SWIZZLE_Y)),
             ureg_src(tmp), ureg_imm1f(ureg, 0.0f));
    ureg_MOV(ureg, ureg_writemask(dst, TGSI_WRITEMASK_XYW), ureg_src(tmp));
    return D3D_OK;
}

DECL_SPECIAL(NRM)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst tmp = tx_scratch_scalar(tx);
    struct ureg_src nrm = tx_src_scalar(tmp);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    ureg_DP3(ureg, tmp, src, src);
    ureg_RSQ(ureg, tmp, nrm);
    ureg_MIN(ureg, tmp, ureg_imm1f(ureg, FLT_MAX), nrm);
    ureg_MUL(ureg, dst, src, nrm);
    return D3D_OK;
}

DECL_SPECIAL(DP2ADD)
{
    struct ureg_dst tmp = tx_scratch_scalar(tx);
    struct ureg_src dp2 = tx_src_scalar(tmp);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[3];
    int i;
    for (i = 0; i < 3; ++i)
        src[i] = tx_src_param(tx, &tx->insn.src[i]);
    assert_replicate_swizzle(&src[2]);

    ureg_DP2(tx->ureg, tmp, src[0], src[1]);
    ureg_ADD(tx->ureg, dst, src[2], dp2);

    return D3D_OK;
}

DECL_SPECIAL(TEXCOORD)
{
    struct ureg_program *ureg = tx->ureg;
    const unsigned s = tx->insn.dst[0].idx;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);

    tx_texcoord_alloc(tx, s);
    ureg_MOV(ureg, ureg_writemask(ureg_saturate(dst), TGSI_WRITEMASK_XYZ), tx->regs.vT[s]);
    ureg_MOV(ureg, ureg_writemask(dst, TGSI_WRITEMASK_W), ureg_imm1f(tx->ureg, 1.0f));

    return D3D_OK;
}

DECL_SPECIAL(TEXCOORD_ps14)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);

    assert(tx->insn.src[0].file == D3DSPR_TEXTURE);

    ureg_MOV(ureg, dst, src);

    return D3D_OK;
}

DECL_SPECIAL(TEXKILL)
{
    struct ureg_src reg;

    if (tx->version.major > 1 || tx->version.minor > 3) {
        reg = tx_dst_param_as_src(tx, &tx->insn.dst[0]);
    } else {
        tx_texcoord_alloc(tx, tx->insn.dst[0].idx);
        reg = tx->regs.vT[tx->insn.dst[0].idx];
    }
    if (tx->version.major < 2)
        reg = ureg_swizzle(reg, NINE_SWIZZLE4(X,Y,Z,Z));
    ureg_KILL_IF(tx->ureg, reg);

    return D3D_OK;
}

DECL_SPECIAL(TEXBEM)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_dst tmp, tmp2, texcoord;
    struct ureg_src sample, m00, m01, m10, m11;
    struct ureg_src bumpenvlscale, bumpenvloffset;
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;

    assert(tx->version.major == 1);

    sample = ureg_DECL_sampler(ureg, m);
    tx->info->sampler_mask |= 1 << m;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m, ps1x_sampler_type(tx->info, m),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif

    tx_texcoord_alloc(tx, m);

    tmp = tx_scratch(tx);
    tmp2 = tx_scratch(tx);
    texcoord = tx_scratch(tx);
    /*
     * Bump-env-matrix:
     * 00 is X
     * 01 is Y
     * 10 is Z
     * 11 is W
     */
    nine_info_mark_const_f_used(tx->info, 8 + 8 + m/2);
    m00 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, X);
    m01 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, Y);
    m10 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, Z);
    m11 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, W);

    /* These two attributes are packed as X=scale0 Y=offset0 Z=scale1 W=offset1 etc */
    if (m % 2 == 0) {
        bumpenvlscale = NINE_CONSTANT_SRC_SWIZZLE(8 + 8 + m / 2, X);
        bumpenvloffset = NINE_CONSTANT_SRC_SWIZZLE(8 + 8 + m / 2, Y);
    } else {
        bumpenvlscale = NINE_CONSTANT_SRC_SWIZZLE(8 + 8 + m / 2, Z);
        bumpenvloffset = NINE_CONSTANT_SRC_SWIZZLE(8 + 8 + m / 2, W);
    }

    apply_ps1x_projection(tx, texcoord, tx->regs.vT[m], m);

    /* u' = TextureCoordinates(stage m)u + D3DTSS_BUMPENVMAT00(stage m)*t(n)R  */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), m00,
             NINE_APPLY_SWIZZLE(ureg_src(tx->regs.tS[n]), X), ureg_src(texcoord));
    /* u' = u' + D3DTSS_BUMPENVMAT10(stage m)*t(n)G */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), m10,
             NINE_APPLY_SWIZZLE(ureg_src(tx->regs.tS[n]), Y),
             NINE_APPLY_SWIZZLE(ureg_src(tmp), X));

    /* v' = TextureCoordinates(stage m)v + D3DTSS_BUMPENVMAT01(stage m)*t(n)R */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), m01,
             NINE_APPLY_SWIZZLE(ureg_src(tx->regs.tS[n]), X), ureg_src(texcoord));
    /* v' = v' + D3DTSS_BUMPENVMAT11(stage m)*t(n)G*/
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), m11,
             NINE_APPLY_SWIZZLE(ureg_src(tx->regs.tS[n]), Y),
             NINE_APPLY_SWIZZLE(ureg_src(tmp), Y));

    /* Now the texture coordinates are in tmp.xy */

    if (tx->insn.opcode == D3DSIO_TEXBEM) {
        ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m), ureg_src(tmp), sample);
    } else if (tx->insn.opcode == D3DSIO_TEXBEML) {
        /* t(m)RGBA = t(m)RGBA * [(t(n)B * D3DTSS_BUMPENVLSCALE(stage m)) + D3DTSS_BUMPENVLOFFSET(stage m)] */
        ureg_TEX(ureg, tmp, ps1x_sampler_type(tx->info, m), ureg_src(tmp), sample);
        ureg_MAD(ureg, tmp2, NINE_APPLY_SWIZZLE(ureg_src(tx->regs.tS[n]), Z),
                 bumpenvlscale, bumpenvloffset);
        ureg_MUL(ureg, dst, ureg_src(tmp), ureg_src(tmp2));
    }

    tx->info->bumpenvmat_needed = 1;

    return D3D_OK;
}

DECL_SPECIAL(TEXREG2AR)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src sample;
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    sample = ureg_DECL_sampler(ureg, m);
    tx->info->sampler_mask |= 1 << m;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m, ps1x_sampler_type(tx->info, m),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m), ureg_swizzle(ureg_src(tx->regs.tS[n]), NINE_SWIZZLE4(W,X,X,X)), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXREG2GB)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src sample;
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    sample = ureg_DECL_sampler(ureg, m);
    tx->info->sampler_mask |= 1 << m;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m, ps1x_sampler_type(tx->info, m),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m), ureg_swizzle(ureg_src(tx->regs.tS[n]), NINE_SWIZZLE4(Y,Z,Z,Z)), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXM3x2PAD)
{
    return D3D_OK; /* this is just padding */
}

DECL_SPECIAL(TEXM3x2TEX)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src sample;
    const int m = tx->insn.dst[0].idx - 1;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);
    tx_texcoord_alloc(tx, m+1);

    /* performs the matrix multiplication */
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_X), tx->regs.vT[m], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Y), tx->regs.vT[m+1], ureg_src(tx->regs.tS[n]));

    sample = ureg_DECL_sampler(ureg, m + 1);
    tx->info->sampler_mask |= 1 << (m + 1);
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m + 1, ps1x_sampler_type(tx->info, m + 1),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m + 1), ureg_src(dst), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXM3x3PAD)
{
    return D3D_OK; /* this is just padding */
}

DECL_SPECIAL(TEXM3x3SPEC)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src E = tx_src_param(tx, &tx->insn.src[1]);
    struct ureg_src sample;
    struct ureg_dst tmp;
    const int m = tx->insn.dst[0].idx - 2;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);
    tx_texcoord_alloc(tx, m+1);
    tx_texcoord_alloc(tx, m+2);

    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_X), tx->regs.vT[m], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Y), tx->regs.vT[m+1], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Z), tx->regs.vT[m+2], ureg_src(tx->regs.tS[n]));

    sample = ureg_DECL_sampler(ureg, m + 2);
    tx->info->sampler_mask |= 1 << (m + 2);
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m + 2, ps1x_sampler_type(tx->info, m + 2),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    tmp = ureg_writemask(tx_scratch(tx), TGSI_WRITEMASK_XYZ);

    /* At this step, dst = N = (u', w', z').
     * We want dst to be the texture sampled at (u'', w'', z''), with
     * (u'', w'', z'') = 2 * (N.E / N.N) * N - E */
    ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_src(dst), ureg_src(dst));
    ureg_RCP(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X));
    /* at this step tmp.x = 1/N.N */
    ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), ureg_src(dst), E);
    /* at this step tmp.y = N.E */
    ureg_MUL(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y));
    /* at this step tmp.x = N.E/N.N */
    ureg_MUL(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_imm1f(ureg, 2.0f));
    ureg_MUL(ureg, tmp, ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_src(dst));
    /* at this step tmp.xyz = 2 * (N.E / N.N) * N */
    ureg_ADD(ureg, tmp, ureg_src(tmp), ureg_negate(E));
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m + 2), ureg_src(tmp), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXREG2RGB)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src sample;
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    sample = ureg_DECL_sampler(ureg, m);
    tx->info->sampler_mask |= 1 << m;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m, ps1x_sampler_type(tx->info, m),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m), ureg_src(tx->regs.tS[n]), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXDP3TEX)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_dst tmp;
    struct ureg_src sample;
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);

    tmp = tx_scratch(tx);
    ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), tx->regs.vT[m], ureg_src(tx->regs.tS[n]));
    ureg_MOV(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_YZ), ureg_imm1f(ureg, 0.0f));

    sample = ureg_DECL_sampler(ureg, m);
    tx->info->sampler_mask |= 1 << m;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, m, ps1x_sampler_type(tx->info, m),
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
    ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m), ureg_src(tmp), sample);

    return D3D_OK;
}

DECL_SPECIAL(TEXM3x2DEPTH)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst tmp;
    const int m = tx->insn.dst[0].idx - 1;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);
    tx_texcoord_alloc(tx, m+1);

    tmp = tx_scratch(tx);

    /* performs the matrix multiplication */
    ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), tx->regs.vT[m], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), tx->regs.vT[m+1], ureg_src(tx->regs.tS[n]));

    ureg_RCP(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Z), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y));
    /* tmp.x = 'z', tmp.y = 'w', tmp.z = 1/'w'. */
    ureg_MUL(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Z));
    /* res = 'w' == 0 ? 1.0 : z/w */
    ureg_CMP(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_negate(ureg_abs(ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y))),
             ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_imm1f(ureg, 1.0f));
    /* replace the depth for depth testing with the result */
    tx->regs.oDepth = ureg_DECL_output_masked(ureg, TGSI_SEMANTIC_POSITION, 0,
                                              TGSI_WRITEMASK_Z, 0, 1);
    ureg_MOV(ureg, tx->regs.oDepth, ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X));
    /* note that we write nothing to the destination, since it's disallowed to use it afterward */
    return D3D_OK;
}

DECL_SPECIAL(TEXDP3)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    const int m = tx->insn.dst[0].idx;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);

    ureg_DP3(ureg, dst, tx->regs.vT[m], ureg_src(tx->regs.tS[n]));

    return D3D_OK;
}

DECL_SPECIAL(TEXM3x3)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src sample;
    struct ureg_dst E, tmp;
    const int m = tx->insn.dst[0].idx - 2;
    const int n = tx->insn.src[0].idx;
    assert(m >= 0 && m > n);

    tx_texcoord_alloc(tx, m);
    tx_texcoord_alloc(tx, m+1);
    tx_texcoord_alloc(tx, m+2);

    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_X), tx->regs.vT[m], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Y), tx->regs.vT[m+1], ureg_src(tx->regs.tS[n]));
    ureg_DP3(ureg, ureg_writemask(dst, TGSI_WRITEMASK_Z), tx->regs.vT[m+2], ureg_src(tx->regs.tS[n]));

    switch (tx->insn.opcode) {
    case D3DSIO_TEXM3x3:
        ureg_MOV(ureg, ureg_writemask(dst, TGSI_WRITEMASK_W), ureg_imm1f(ureg, 1.0f));
        break;
    case D3DSIO_TEXM3x3TEX:
        sample = ureg_DECL_sampler(ureg, m + 2);
        tx->info->sampler_mask |= 1 << (m + 2);
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
        ureg_DECL_sampler_view(ureg, m + 2, ps1x_sampler_type(tx->info, m + 2),
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
        ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m + 2), ureg_src(dst), sample);
        break;
    case D3DSIO_TEXM3x3VSPEC:
        sample = ureg_DECL_sampler(ureg, m + 2);
        tx->info->sampler_mask |= 1 << (m + 2);
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
        ureg_DECL_sampler_view(ureg, m + 2, ps1x_sampler_type(tx->info, m + 2),
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                               TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif
        E = tx_scratch(tx);
        tmp = ureg_writemask(tx_scratch(tx), TGSI_WRITEMASK_XYZ);
        ureg_MOV(ureg, ureg_writemask(E, TGSI_WRITEMASK_X), ureg_scalar(tx->regs.vT[m], TGSI_SWIZZLE_W));
        ureg_MOV(ureg, ureg_writemask(E, TGSI_WRITEMASK_Y), ureg_scalar(tx->regs.vT[m+1], TGSI_SWIZZLE_W));
        ureg_MOV(ureg, ureg_writemask(E, TGSI_WRITEMASK_Z), ureg_scalar(tx->regs.vT[m+2], TGSI_SWIZZLE_W));
        /* At this step, dst = N = (u', w', z').
         * We want dst to be the texture sampled at (u'', w'', z''), with
         * (u'', w'', z'') = 2 * (N.E / N.N) * N - E */
        ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_src(dst), ureg_src(dst));
        ureg_RCP(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X));
        /* at this step tmp.x = 1/N.N */
        ureg_DP3(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), ureg_src(dst), ureg_src(E));
        /* at this step tmp.y = N.E */
        ureg_MUL(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y));
        /* at this step tmp.x = N.E/N.N */
        ureg_MUL(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_imm1f(ureg, 2.0f));
        ureg_MUL(ureg, tmp, ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_src(dst));
        /* at this step tmp.xyz = 2 * (N.E / N.N) * N */
        ureg_ADD(ureg, tmp, ureg_src(tmp), ureg_negate(ureg_src(E)));
        ureg_TEX(ureg, dst, ps1x_sampler_type(tx->info, m + 2), ureg_src(tmp), sample);
        break;
    default:
        return D3DERR_INVALIDCALL;
    }
    return D3D_OK;
}

DECL_SPECIAL(TEXDEPTH)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst r5;
    struct ureg_src r5r, r5g;

    assert(tx->insn.dst[0].idx == 5); /* instruction must get r5 here */

    /* we must replace the depth by r5.g == 0 ? 1.0f : r5.r/r5.g.
     * r5 won't be used afterward, thus we can use r5.ba */
    r5 = tx->regs.r[5];
    r5r = ureg_scalar(ureg_src(r5), TGSI_SWIZZLE_X);
    r5g = ureg_scalar(ureg_src(r5), TGSI_SWIZZLE_Y);

    ureg_RCP(ureg, ureg_writemask(r5, TGSI_WRITEMASK_Z), r5g);
    ureg_MUL(ureg, ureg_writemask(r5, TGSI_WRITEMASK_X), r5r, ureg_scalar(ureg_src(r5), TGSI_SWIZZLE_Z));
    /* r5.r = r/g */
    ureg_CMP(ureg, ureg_writemask(r5, TGSI_WRITEMASK_X), ureg_negate(ureg_abs(r5g)),
             r5r, ureg_imm1f(ureg, 1.0f));
    /* replace the depth for depth testing with the result */
    tx->regs.oDepth = ureg_DECL_output_masked(ureg, TGSI_SEMANTIC_POSITION, 0,
                                              TGSI_WRITEMASK_Z, 0, 1);
    ureg_MOV(ureg, tx->regs.oDepth, r5r);

    return D3D_OK;
}

DECL_SPECIAL(BEM)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src0 = tx_src_param(tx, &tx->insn.src[0]);
    struct ureg_src src1 = tx_src_param(tx, &tx->insn.src[1]);
    struct ureg_src m00, m01, m10, m11;
    const int m = tx->insn.dst[0].idx;
    struct ureg_dst tmp;
    /*
     * Bump-env-matrix:
     * 00 is X
     * 01 is Y
     * 10 is Z
     * 11 is W
     */
    nine_info_mark_const_f_used(tx->info, 8 + m);
    m00 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, X);
    m01 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, Y);
    m10 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, Z);
    m11 = NINE_CONSTANT_SRC_SWIZZLE(8 + m, W);
    /* dest.r = src0.r + D3DTSS_BUMPENVMAT00(stage n) * src1.r  */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), m00,
             NINE_APPLY_SWIZZLE(src1, X), NINE_APPLY_SWIZZLE(src0, X));
    /* dest.r = dest.r + D3DTSS_BUMPENVMAT10(stage n) * src1.g; */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_X), m10,
             NINE_APPLY_SWIZZLE(src1, Y), NINE_APPLY_SWIZZLE(ureg_src(tmp), X));

    /* dest.g = src0.g + D3DTSS_BUMPENVMAT01(stage n) * src1.r */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), m01,
             NINE_APPLY_SWIZZLE(src1, X), src0);
    /* dest.g = dest.g + D3DTSS_BUMPENVMAT11(stage n) * src1.g */
    ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_Y), m11,
             NINE_APPLY_SWIZZLE(src1, Y), NINE_APPLY_SWIZZLE(ureg_src(tmp), Y));
    ureg_MOV(ureg, ureg_writemask(dst, TGSI_WRITEMASK_XY), ureg_src(tmp));

    tx->info->bumpenvmat_needed = 1;

    return D3D_OK;
}

DECL_SPECIAL(TEXLD)
{
    struct ureg_program *ureg = tx->ureg;
    unsigned target;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[2] = {
        tx_src_param(tx, &tx->insn.src[0]),
        tx_src_param(tx, &tx->insn.src[1])
    };
    assert(tx->insn.src[1].idx >= 0 &&
           tx->insn.src[1].idx < ARRAY_SIZE(tx->sampler_targets));
    target = tx->sampler_targets[tx->insn.src[1].idx];

    switch (tx->insn.flags) {
    case 0:
        ureg_TEX(ureg, dst, target, src[0], src[1]);
        break;
    case NINED3DSI_TEXLD_PROJECT:
        ureg_TXP(ureg, dst, target, src[0], src[1]);
        break;
    case NINED3DSI_TEXLD_BIAS:
        ureg_TXB(ureg, dst, target, src[0], src[1]);
        break;
    default:
        assert(0);
        return D3DERR_INVALIDCALL;
    }
    return D3D_OK;
}

DECL_SPECIAL(TEXLD_14)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src = tx_src_param(tx, &tx->insn.src[0]);
    const unsigned s = tx->insn.dst[0].idx;
    const unsigned t = ps1x_sampler_type(tx->info, s);

    tx->info->sampler_mask |= 1 << s;
    ureg_TEX(ureg, dst, t, src, ureg_DECL_sampler(ureg, s));
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, s, t,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif

    return D3D_OK;
}

DECL_SPECIAL(TEX)
{
    struct ureg_program *ureg = tx->ureg;
    const unsigned s = tx->insn.dst[0].idx;
    const unsigned t = ps1x_sampler_type(tx->info, s);
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[2];

    tx_texcoord_alloc(tx, s);

    src[0] = tx->regs.vT[s];
    src[1] = ureg_DECL_sampler(ureg, s);
    tx->info->sampler_mask |= 1 << s;
#ifdef VBOX_WITH_MESA3D_NINE_SVGA
    ureg_DECL_sampler_view(ureg, s, t,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT,
                           TGSI_RETURN_TYPE_FLOAT, TGSI_RETURN_TYPE_FLOAT);
#endif

    TEX_with_ps1x_projection(tx, dst, t, src[0], src[1], s);

    return D3D_OK;
}

DECL_SPECIAL(TEXLDD)
{
    unsigned target;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[4] = {
        tx_src_param(tx, &tx->insn.src[0]),
        tx_src_param(tx, &tx->insn.src[1]),
        tx_src_param(tx, &tx->insn.src[2]),
        tx_src_param(tx, &tx->insn.src[3])
    };
    assert(tx->insn.src[1].idx >= 0 &&
           tx->insn.src[1].idx < ARRAY_SIZE(tx->sampler_targets));
    target = tx->sampler_targets[tx->insn.src[1].idx];

    ureg_TXD(tx->ureg, dst, target, src[0], src[2], src[3], src[1]);
    return D3D_OK;
}

DECL_SPECIAL(TEXLDL)
{
    unsigned target;
    struct ureg_dst dst = tx_dst_param(tx, &tx->insn.dst[0]);
    struct ureg_src src[2] = {
       tx_src_param(tx, &tx->insn.src[0]),
       tx_src_param(tx, &tx->insn.src[1])
    };
    assert(tx->insn.src[1].idx >= 0 &&
           tx->insn.src[1].idx < ARRAY_SIZE(tx->sampler_targets));
    target = tx->sampler_targets[tx->insn.src[1].idx];

    ureg_TXL(tx->ureg, dst, target, src[0], src[1]);
    return D3D_OK;
}

DECL_SPECIAL(SETP)
{
    STUB(D3DERR_INVALIDCALL);
}

DECL_SPECIAL(BREAKP)
{
    STUB(D3DERR_INVALIDCALL);
}

DECL_SPECIAL(PHASE)
{
    return D3D_OK; /* we don't care about phase */
}

DECL_SPECIAL(COMMENT)
{
    return D3D_OK; /* nothing to do */
}


#define _OPI(o,t,vv1,vv2,pv1,pv2,d,s,h) \
    { D3DSIO_##o, TGSI_OPCODE_##t, { vv1, vv2 }, { pv1, pv2, }, d, s, h }

struct sm1_op_info inst_table[] =
{
    _OPI(NOP, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 0, 0, SPECIAL(NOP)), /* 0 */
    _OPI(MOV, MOV, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, NULL),
    _OPI(ADD, ADD, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 2 */
    _OPI(SUB, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(SUB)), /* 3 */
    _OPI(MAD, MAD, V(0,0), V(3,0), V(0,0), V(3,0), 1, 3, NULL), /* 4 */
    _OPI(MUL, MUL, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 5 */
    _OPI(RCP, RCP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, NULL), /* 6 */
    _OPI(RSQ, RSQ, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, SPECIAL(RSQ)), /* 7 */
    _OPI(DP3, DP3, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 8 */
    _OPI(DP4, DP4, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 9 */
    _OPI(MIN, MIN, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 10 */
    _OPI(MAX, MAX, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 11 */
    _OPI(SLT, SLT, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 12 */
    _OPI(SGE, SGE, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 13 */
    _OPI(EXP, EX2, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, NULL), /* 14 */
    _OPI(LOG, LG2, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, SPECIAL(LOG)), /* 15 */
    _OPI(LIT, LIT, V(0,0), V(3,0), V(0,0), V(0,0), 1, 1, SPECIAL(LIT)), /* 16 */
    _OPI(DST, DST, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, NULL), /* 17 */
    _OPI(LRP, LRP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 3, NULL), /* 18 */
    _OPI(FRC, FRC, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, NULL), /* 19 */

    _OPI(M4x4, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(M4x4)),
    _OPI(M4x3, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(M4x3)),
    _OPI(M3x4, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(M3x4)),
    _OPI(M3x3, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(M3x3)),
    _OPI(M3x2, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(M3x2)),

    _OPI(CALL,    CAL,     V(2,0), V(3,0), V(2,1), V(3,0), 0, 1, SPECIAL(CALL)),
    _OPI(CALLNZ,  CAL,     V(2,0), V(3,0), V(2,1), V(3,0), 0, 2, SPECIAL(CALLNZ)),
    _OPI(LOOP,    BGNLOOP, V(2,0), V(3,0), V(3,0), V(3,0), 0, 2, SPECIAL(LOOP)),
    _OPI(RET,     RET,     V(2,0), V(3,0), V(2,1), V(3,0), 0, 0, SPECIAL(RET)),
    _OPI(ENDLOOP, ENDLOOP, V(2,0), V(3,0), V(3,0), V(3,0), 0, 0, SPECIAL(ENDLOOP)),
    _OPI(LABEL,   NOP,     V(2,0), V(3,0), V(2,1), V(3,0), 0, 1, SPECIAL(LABEL)),

    _OPI(DCL, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 0, 0, SPECIAL(DCL)),

    _OPI(POW, POW, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(POW)),
    _OPI(CRS, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 2, SPECIAL(XPD)), /* XXX: .w */
    _OPI(SGN, SSG, V(2,0), V(3,0), V(0,0), V(0,0), 1, 3, SPECIAL(SGN)), /* ignore src1,2 */
    _OPI(ABS, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, SPECIAL(ABS)),
    _OPI(NRM, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 1, SPECIAL(NRM)), /* NRM doesn't fit */

    _OPI(SINCOS, NOP, V(2,0), V(2,1), V(2,0), V(2,1), 1, 3, SPECIAL(SINCOS)),
    _OPI(SINCOS, NOP, V(3,0), V(3,0), V(3,0), V(3,0), 1, 1, SPECIAL(SINCOS)),

    /* More flow control */
    _OPI(REP,    NOP,    V(2,0), V(3,0), V(2,1), V(3,0), 0, 1, SPECIAL(REP)),
    _OPI(ENDREP, NOP,    V(2,0), V(3,0), V(2,1), V(3,0), 0, 0, SPECIAL(ENDREP)),
    _OPI(IF,     IF,     V(2,0), V(3,0), V(2,1), V(3,0), 0, 1, SPECIAL(IF)),
    _OPI(IFC,    IF,     V(2,1), V(3,0), V(2,1), V(3,0), 0, 2, SPECIAL(IFC)),
    _OPI(ELSE,   ELSE,   V(2,0), V(3,0), V(2,1), V(3,0), 0, 0, SPECIAL(ELSE)),
    _OPI(ENDIF,  ENDIF,  V(2,0), V(3,0), V(2,1), V(3,0), 0, 0, SPECIAL(ENDIF)),
    _OPI(BREAK,  BRK,    V(2,1), V(3,0), V(2,1), V(3,0), 0, 0, NULL),
    _OPI(BREAKC, NOP,    V(2,1), V(3,0), V(2,1), V(3,0), 0, 2, SPECIAL(BREAKC)),
    /* we don't write to the address register, but a normal register (copied
     * when needed to the address register), thus we don't use ARR */
    _OPI(MOVA, MOV, V(2,0), V(3,0), V(0,0), V(0,0), 1, 1, NULL),

    _OPI(DEFB, NOP, V(0,0), V(3,0) , V(0,0), V(3,0) , 1, 0, SPECIAL(DEFB)),
    _OPI(DEFI, NOP, V(0,0), V(3,0) , V(0,0), V(3,0) , 1, 0, SPECIAL(DEFI)),

    _OPI(TEXCOORD,     NOP, V(0,0), V(0,0), V(0,0), V(1,3), 1, 0, SPECIAL(TEXCOORD)),
    _OPI(TEXCOORD,     MOV, V(0,0), V(0,0), V(1,4), V(1,4), 1, 1, SPECIAL(TEXCOORD_ps14)),
    _OPI(TEXKILL,      KILL_IF, V(0,0), V(0,0), V(0,0), V(3,0), 1, 0, SPECIAL(TEXKILL)),
    _OPI(TEX,          TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 0, SPECIAL(TEX)),
    _OPI(TEX,          TEX, V(0,0), V(0,0), V(1,4), V(1,4), 1, 1, SPECIAL(TEXLD_14)),
    _OPI(TEX,          TEX, V(0,0), V(0,0), V(2,0), V(3,0), 1, 2, SPECIAL(TEXLD)),
    _OPI(TEXBEM,       TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXBEM)),
    _OPI(TEXBEML,      TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXBEM)),
    _OPI(TEXREG2AR,    TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXREG2AR)),
    _OPI(TEXREG2GB,    TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXREG2GB)),
    _OPI(TEXM3x2PAD,   TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXM3x2PAD)),
    _OPI(TEXM3x2TEX,   TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXM3x2TEX)),
    _OPI(TEXM3x3PAD,   TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXM3x3PAD)),
    _OPI(TEXM3x3TEX,   TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXM3x3)),
    _OPI(TEXM3x3SPEC,  TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 2, SPECIAL(TEXM3x3SPEC)),
    _OPI(TEXM3x3VSPEC, TEX, V(0,0), V(0,0), V(0,0), V(1,3), 1, 1, SPECIAL(TEXM3x3)),

    _OPI(EXPP, EXP, V(0,0), V(1,1), V(0,0), V(0,0), 1, 1, NULL),
    _OPI(EXPP, EX2, V(2,0), V(3,0), V(0,0), V(0,0), 1, 1, NULL),
    _OPI(LOGP, LG2, V(0,0), V(3,0), V(0,0), V(0,0), 1, 1, SPECIAL(LOG)),
    _OPI(CND,  NOP, V(0,0), V(0,0), V(0,0), V(1,4), 1, 3, SPECIAL(CND)),

    _OPI(DEF, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 1, 0, SPECIAL(DEF)),

    /* More tex stuff */
    _OPI(TEXREG2RGB,   TEX, V(0,0), V(0,0), V(1,2), V(1,3), 1, 1, SPECIAL(TEXREG2RGB)),
    _OPI(TEXDP3TEX,    TEX, V(0,0), V(0,0), V(1,2), V(1,3), 1, 1, SPECIAL(TEXDP3TEX)),
    _OPI(TEXM3x2DEPTH, TEX, V(0,0), V(0,0), V(1,3), V(1,3), 1, 1, SPECIAL(TEXM3x2DEPTH)),
    _OPI(TEXDP3,       TEX, V(0,0), V(0,0), V(1,2), V(1,3), 1, 1, SPECIAL(TEXDP3)),
    _OPI(TEXM3x3,      TEX, V(0,0), V(0,0), V(1,2), V(1,3), 1, 1, SPECIAL(TEXM3x3)),
    _OPI(TEXDEPTH,     TEX, V(0,0), V(0,0), V(1,4), V(1,4), 1, 0, SPECIAL(TEXDEPTH)),

    /* Misc */
    _OPI(CMP,    CMP,  V(0,0), V(0,0), V(1,2), V(3,0), 1, 3, SPECIAL(CMP)), /* reversed */
    _OPI(BEM,    NOP,  V(0,0), V(0,0), V(1,4), V(1,4), 1, 2, SPECIAL(BEM)),
    _OPI(DP2ADD, NOP,  V(0,0), V(0,0), V(2,0), V(3,0), 1, 3, SPECIAL(DP2ADD)),
    _OPI(DSX,    DDX,  V(0,0), V(0,0), V(2,1), V(3,0), 1, 1, NULL),
    _OPI(DSY,    DDY,  V(0,0), V(0,0), V(2,1), V(3,0), 1, 1, NULL),
    _OPI(TEXLDD, TXD,  V(0,0), V(0,0), V(2,1), V(3,0), 1, 4, SPECIAL(TEXLDD)),
    _OPI(SETP,   NOP,  V(0,0), V(3,0), V(2,1), V(3,0), 1, 2, SPECIAL(SETP)),
    _OPI(TEXLDL, TXL,  V(3,0), V(3,0), V(3,0), V(3,0), 1, 2, SPECIAL(TEXLDL)),
    _OPI(BREAKP, BRK,  V(0,0), V(3,0), V(2,1), V(3,0), 0, 1, SPECIAL(BREAKP))
};

struct sm1_op_info inst_phase =
    _OPI(PHASE, NOP, V(0,0), V(0,0), V(1,4), V(1,4), 0, 0, SPECIAL(PHASE));

struct sm1_op_info inst_comment =
    _OPI(COMMENT, NOP, V(0,0), V(3,0), V(0,0), V(3,0), 0, 0, SPECIAL(COMMENT));

static void
create_op_info_map(struct shader_translator *tx)
{
    const unsigned version = (tx->version.major << 8) | tx->version.minor;
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(tx->op_info_map); ++i)
        tx->op_info_map[i] = -1;

    if (tx->processor == PIPE_SHADER_VERTEX) {
        for (i = 0; i < ARRAY_SIZE(inst_table); ++i) {
            assert(inst_table[i].sio < ARRAY_SIZE(tx->op_info_map));
            if (inst_table[i].vert_version.min <= version &&
                inst_table[i].vert_version.max >= version)
                tx->op_info_map[inst_table[i].sio] = i;
        }
    } else {
        for (i = 0; i < ARRAY_SIZE(inst_table); ++i) {
            assert(inst_table[i].sio < ARRAY_SIZE(tx->op_info_map));
            if (inst_table[i].frag_version.min <= version &&
                inst_table[i].frag_version.max >= version)
                tx->op_info_map[inst_table[i].sio] = i;
        }
    }
}

static inline HRESULT
NineTranslateInstruction_Generic(struct shader_translator *tx)
{
    struct ureg_dst dst[1];
    struct ureg_src src[4];
    unsigned i;

    for (i = 0; i < tx->insn.ndst && i < ARRAY_SIZE(dst); ++i)
        dst[i] = tx_dst_param(tx, &tx->insn.dst[i]);
    for (i = 0; i < tx->insn.nsrc && i < ARRAY_SIZE(src); ++i)
        src[i] = tx_src_param(tx, &tx->insn.src[i]);

    ureg_insn(tx->ureg, tx->insn.info->opcode,
              dst, tx->insn.ndst,
              src, tx->insn.nsrc, 0);
    return D3D_OK;
}

static inline DWORD
TOKEN_PEEK(struct shader_translator *tx)
{
    return *(tx->parse);
}

static inline DWORD
TOKEN_NEXT(struct shader_translator *tx)
{
    return *(tx->parse)++;
}

static inline void
TOKEN_JUMP(struct shader_translator *tx)
{
    if (tx->parse_next && tx->parse != tx->parse_next) {
        WARN("parse(%p) != parse_next(%p) !\n", tx->parse, tx->parse_next);
        tx->parse = tx->parse_next;
    }
}

static inline boolean
sm1_parse_eof(struct shader_translator *tx)
{
    return TOKEN_PEEK(tx) == NINED3DSP_END;
}

static void
sm1_read_version(struct shader_translator *tx)
{
    const DWORD tok = TOKEN_NEXT(tx);

    tx->version.major = D3DSHADER_VERSION_MAJOR(tok);
    tx->version.minor = D3DSHADER_VERSION_MINOR(tok);

    switch (tok >> 16) {
    case NINED3D_SM1_VS: tx->processor = PIPE_SHADER_VERTEX; break;
    case NINED3D_SM1_PS: tx->processor = PIPE_SHADER_FRAGMENT; break;
    default:
       DBG("Invalid shader type: %x\n", tok);
       tx->processor = ~0;
       break;
    }
}

/* This is just to check if we parsed the instruction properly. */
static void
sm1_parse_get_skip(struct shader_translator *tx)
{
    const DWORD tok = TOKEN_PEEK(tx);

    if (tx->version.major >= 2) {
        tx->parse_next = tx->parse + 1 /* this */ +
            ((tok & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT);
    } else {
        tx->parse_next = NULL; /* TODO: determine from param count */
    }
}

static void
sm1_print_comment(const char *comment, UINT size)
{
    if (!size)
        return;
    /* TODO */
}

static void
sm1_parse_comments(struct shader_translator *tx, BOOL print)
{
    DWORD tok = TOKEN_PEEK(tx);

    while ((tok & D3DSI_OPCODE_MASK) == D3DSIO_COMMENT)
    {
        const char *comment = "";
        UINT size = (tok & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT;
        tx->parse += size + 1;

        if (print)
            sm1_print_comment(comment, size);

        tok = TOKEN_PEEK(tx);
    }
}

static void
sm1_parse_get_param(struct shader_translator *tx, DWORD *reg, DWORD *rel)
{
    *reg = TOKEN_NEXT(tx);

    if (*reg & D3DSHADER_ADDRMODE_RELATIVE)
    {
        if (tx->version.major < 2)
            *rel = (1 << 31) |
                ((D3DSPR_ADDR << D3DSP_REGTYPE_SHIFT2) & D3DSP_REGTYPE_MASK2) |
                ((D3DSPR_ADDR << D3DSP_REGTYPE_SHIFT)  & D3DSP_REGTYPE_MASK) |
                D3DSP_NOSWIZZLE;
        else
            *rel = TOKEN_NEXT(tx);
    }
}

static void
sm1_parse_dst_param(struct sm1_dst_param *dst, DWORD tok)
{
    int8_t shift;
    dst->file =
        (tok & D3DSP_REGTYPE_MASK)  >> D3DSP_REGTYPE_SHIFT |
        (tok & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2;
    dst->type = TGSI_RETURN_TYPE_FLOAT;
    dst->idx = tok & D3DSP_REGNUM_MASK;
    dst->rel = NULL;
    dst->mask = (tok & NINED3DSP_WRITEMASK_MASK) >> NINED3DSP_WRITEMASK_SHIFT;
    dst->mod = (tok & D3DSP_DSTMOD_MASK) >> D3DSP_DSTMOD_SHIFT;
    shift = (tok & D3DSP_DSTSHIFT_MASK) >> D3DSP_DSTSHIFT_SHIFT;
    dst->shift = (shift & 0x7) - (shift & 0x8);
}

static void
sm1_parse_src_param(struct sm1_src_param *src, DWORD tok)
{
    src->file =
        ((tok & D3DSP_REGTYPE_MASK)  >> D3DSP_REGTYPE_SHIFT) |
        ((tok & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2);
    src->type = TGSI_RETURN_TYPE_FLOAT;
    src->idx = tok & D3DSP_REGNUM_MASK;
    src->rel = NULL;
    src->swizzle = (tok & D3DSP_SWIZZLE_MASK) >> D3DSP_SWIZZLE_SHIFT;
    src->mod = (tok & D3DSP_SRCMOD_MASK) >> D3DSP_SRCMOD_SHIFT;

    switch (src->file) {
    case D3DSPR_CONST2: src->file = D3DSPR_CONST; src->idx += 2048; break;
    case D3DSPR_CONST3: src->file = D3DSPR_CONST; src->idx += 4096; break;
    case D3DSPR_CONST4: src->file = D3DSPR_CONST; src->idx += 6144; break;
    default:
        break;
    }
}

static void
sm1_parse_immediate(struct shader_translator *tx,
                    struct sm1_src_param *imm)
{
    imm->file = NINED3DSPR_IMMEDIATE;
    imm->idx = INT_MIN;
    imm->rel = NULL;
    imm->swizzle = NINED3DSP_NOSWIZZLE;
    imm->mod = 0;
    switch (tx->insn.opcode) {
    case D3DSIO_DEF:
        imm->type = NINED3DSPTYPE_FLOAT4;
        memcpy(&imm->imm.d[0], tx->parse, 4 * sizeof(DWORD));
        tx->parse += 4;
        break;
    case D3DSIO_DEFI:
        imm->type = NINED3DSPTYPE_INT4;
        memcpy(&imm->imm.d[0], tx->parse, 4 * sizeof(DWORD));
        tx->parse += 4;
        break;
    case D3DSIO_DEFB:
        imm->type = NINED3DSPTYPE_BOOL;
        memcpy(&imm->imm.d[0], tx->parse, 1 * sizeof(DWORD));
        tx->parse += 1;
        break;
    default:
       assert(0);
       break;
    }
}

static void
sm1_read_dst_param(struct shader_translator *tx,
                   struct sm1_dst_param *dst,
                   struct sm1_src_param *rel)
{
    DWORD tok_dst, tok_rel = 0;

    sm1_parse_get_param(tx, &tok_dst, &tok_rel);
    sm1_parse_dst_param(dst, tok_dst);
    if (tok_dst & D3DSHADER_ADDRMODE_RELATIVE) {
        sm1_parse_src_param(rel, tok_rel);
        dst->rel = rel;
    }
}

static void
sm1_read_src_param(struct shader_translator *tx,
                   struct sm1_src_param *src,
                   struct sm1_src_param *rel)
{
    DWORD tok_src, tok_rel = 0;

    sm1_parse_get_param(tx, &tok_src, &tok_rel);
    sm1_parse_src_param(src, tok_src);
    if (tok_src & D3DSHADER_ADDRMODE_RELATIVE) {
        assert(rel);
        sm1_parse_src_param(rel, tok_rel);
        src->rel = rel;
    }
}

static void
sm1_read_semantic(struct shader_translator *tx,
                  struct sm1_semantic *sem)
{
    const DWORD tok_usg = TOKEN_NEXT(tx);
    const DWORD tok_dst = TOKEN_NEXT(tx);

    sem->sampler_type = (tok_usg & D3DSP_TEXTURETYPE_MASK) >> D3DSP_TEXTURETYPE_SHIFT;
    sem->usage = (tok_usg & D3DSP_DCL_USAGE_MASK) >> D3DSP_DCL_USAGE_SHIFT;
    sem->usage_idx = (tok_usg & D3DSP_DCL_USAGEINDEX_MASK) >> D3DSP_DCL_USAGEINDEX_SHIFT;

    sm1_parse_dst_param(&sem->reg, tok_dst);
}

static void
sm1_parse_instruction(struct shader_translator *tx)
{
    struct sm1_instruction *insn = &tx->insn;
    HRESULT hr;
    DWORD tok;
    struct sm1_op_info *info = NULL;
    unsigned i;

    sm1_parse_comments(tx, TRUE);
    sm1_parse_get_skip(tx);

    tok = TOKEN_NEXT(tx);

    insn->opcode = tok & D3DSI_OPCODE_MASK;
    insn->flags = (tok & NINED3DSIO_OPCODE_FLAGS_MASK) >> NINED3DSIO_OPCODE_FLAGS_SHIFT;
    insn->coissue = !!(tok & D3DSI_COISSUE);
    insn->predicated = !!(tok & NINED3DSHADER_INST_PREDICATED);

    if (insn->opcode < ARRAY_SIZE(tx->op_info_map)) {
        int k = tx->op_info_map[insn->opcode];
        if (k >= 0) {
            assert(k < ARRAY_SIZE(inst_table));
            info = &inst_table[k];
        }
    } else {
       if (insn->opcode == D3DSIO_PHASE)   info = &inst_phase;
       if (insn->opcode == D3DSIO_COMMENT) info = &inst_comment;
    }
    if (!info) {
       DBG("illegal or unhandled opcode: %08x\n", insn->opcode);
       TOKEN_JUMP(tx);
       return;
    }
    insn->info = info;
    insn->ndst = info->ndst;
    insn->nsrc = info->nsrc;

    assert(!insn->predicated && "TODO: predicated instructions");

    /* check version */
    {
        unsigned min = IS_VS ? info->vert_version.min : info->frag_version.min;
        unsigned max = IS_VS ? info->vert_version.max : info->frag_version.max;
        unsigned ver = (tx->version.major << 8) | tx->version.minor;
        if (ver < min || ver > max) {
            DBG("opcode not supported in this shader version: %x <= %x <= %x\n",
                min, ver, max);
            return;
        }
    }

    for (i = 0; i < insn->ndst; ++i)
        sm1_read_dst_param(tx, &insn->dst[i], &insn->dst_rel[i]);
    if (insn->predicated)
        sm1_read_src_param(tx, &insn->pred, NULL);
    for (i = 0; i < insn->nsrc; ++i)
        sm1_read_src_param(tx, &insn->src[i], &insn->src_rel[i]);

    /* parse here so we can dump them before processing */
    if (insn->opcode == D3DSIO_DEF ||
        insn->opcode == D3DSIO_DEFI ||
        insn->opcode == D3DSIO_DEFB)
        sm1_parse_immediate(tx, &tx->insn.src[0]);

    sm1_dump_instruction(insn, tx->cond_depth + tx->loop_depth);
    sm1_instruction_check(insn);

    if (info->handler)
        hr = info->handler(tx);
    else
        hr = NineTranslateInstruction_Generic(tx);
    tx_apply_dst0_modifiers(tx);

    if (hr != D3D_OK)
        tx->failure = TRUE;
    tx->num_scratch = 0; /* reset */

    TOKEN_JUMP(tx);
}

static void
tx_ctor(struct shader_translator *tx, struct nine_shader_info *info)
{
    unsigned i;

    tx->info = info;

    tx->byte_code = info->byte_code;
    tx->parse = info->byte_code;

    for (i = 0; i < ARRAY_SIZE(info->input_map); ++i)
        info->input_map[i] = NINE_DECLUSAGE_NONE;
    info->num_inputs = 0;

    info->position_t = FALSE;
    info->point_size = FALSE;

    tx->info->const_float_slots = 0;
    tx->info->const_int_slots = 0;
    tx->info->const_bool_slots = 0;

    info->sampler_mask = 0x0;
    info->rt_mask = 0x0;

    info->lconstf.data = NULL;
    info->lconstf.ranges = NULL;

    info->bumpenvmat_needed = 0;

    for (i = 0; i < ARRAY_SIZE(tx->regs.rL); ++i) {
        tx->regs.rL[i] = ureg_dst_undef();
    }
    tx->regs.address = ureg_dst_undef();
    tx->regs.a0 = ureg_dst_undef();
    tx->regs.p = ureg_dst_undef();
    tx->regs.oDepth = ureg_dst_undef();
    tx->regs.vPos = ureg_src_undef();
    tx->regs.vFace = ureg_src_undef();
    for (i = 0; i < ARRAY_SIZE(tx->regs.o); ++i)
        tx->regs.o[i] = ureg_dst_undef();
    for (i = 0; i < ARRAY_SIZE(tx->regs.oCol); ++i)
        tx->regs.oCol[i] = ureg_dst_undef();
    for (i = 0; i < ARRAY_SIZE(tx->regs.vC); ++i)
        tx->regs.vC[i] = ureg_src_undef();
    for (i = 0; i < ARRAY_SIZE(tx->regs.vT); ++i)
        tx->regs.vT[i] = ureg_src_undef();

    sm1_read_version(tx);

    info->version = (tx->version.major << 4) | tx->version.minor;

    tx->num_outputs = 0;

    create_op_info_map(tx);
}

static void
tx_dtor(struct shader_translator *tx)
{
    if (tx->num_inst_labels)
        FREE(tx->inst_labels);
    FREE(tx->lconstf);
    FREE(tx->regs.r);
    FREE(tx);
}

/* CONST[0].xyz = width/2, -height/2, zmax-zmin
 * CONST[1].xyz = x+width/2, y+height/2, zmin */
static void
shader_add_vs_viewport_transform(struct shader_translator *tx)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_src c0 = NINE_CONSTANT_SRC(0);
    struct ureg_src c1 = NINE_CONSTANT_SRC(1);
    /* struct ureg_dst pos_tmp = ureg_DECL_temporary(ureg);*/

    c0 = ureg_src_dimension(c0, 4);
    c1 = ureg_src_dimension(c1, 4);
    /* TODO: find out when we need to apply the viewport transformation or not.
     * Likely will be XYZ vs XYZRHW in vdecl_out
     * ureg_MUL(ureg, ureg_writemask(pos_tmp, TGSI_WRITEMASK_XYZ), ureg_src(tx->regs.oPos), c0);
     * ureg_ADD(ureg, ureg_writemask(tx->regs.oPos_out, TGSI_WRITEMASK_XYZ), ureg_src(pos_tmp), c1);
     */
    ureg_MOV(ureg, ureg_writemask(tx->regs.oPos_out, TGSI_WRITEMASK_XYZ), ureg_src(tx->regs.oPos));
}

static void
shader_add_ps_fog_stage(struct shader_translator *tx, struct ureg_src src_col)
{
    struct ureg_program *ureg = tx->ureg;
    struct ureg_dst oCol0 = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
    struct ureg_src fog_end, fog_coeff, fog_density;
    struct ureg_src fog_vs, depth, fog_color;
    struct ureg_dst fog_factor;

    if (!tx->info->fog_enable) {
        ureg_MOV(ureg, oCol0, src_col);
        return;
    }

    if (tx->info->fog_mode != D3DFOG_NONE) {
        depth = nine_get_position_input(tx);
        depth = ureg_scalar(depth, TGSI_SWIZZLE_Z);
    }

    nine_info_mark_const_f_used(tx->info, 33);
    fog_color = NINE_CONSTANT_SRC(32);
    fog_factor = tx_scratch_scalar(tx);

    if (tx->info->fog_mode == D3DFOG_LINEAR) {
        fog_end = NINE_CONSTANT_SRC_SWIZZLE(33, X);
        fog_coeff = NINE_CONSTANT_SRC_SWIZZLE(33, Y);
        ureg_ADD(ureg, fog_factor, fog_end, ureg_negate(depth));
        ureg_MUL(ureg, ureg_saturate(fog_factor), tx_src_scalar(fog_factor), fog_coeff);
    } else if (tx->info->fog_mode == D3DFOG_EXP) {
        fog_density = NINE_CONSTANT_SRC_SWIZZLE(33, X);
        ureg_MUL(ureg, fog_factor, depth, fog_density);
        ureg_MUL(ureg, fog_factor, tx_src_scalar(fog_factor), ureg_imm1f(ureg, -1.442695f));
        ureg_EX2(ureg, fog_factor, tx_src_scalar(fog_factor));
    } else if (tx->info->fog_mode == D3DFOG_EXP2) {
        fog_density = NINE_CONSTANT_SRC_SWIZZLE(33, X);
        ureg_MUL(ureg, fog_factor, depth, fog_density);
        ureg_MUL(ureg, fog_factor, tx_src_scalar(fog_factor), tx_src_scalar(fog_factor));
        ureg_MUL(ureg, fog_factor, tx_src_scalar(fog_factor), ureg_imm1f(ureg, -1.442695f));
        ureg_EX2(ureg, fog_factor, tx_src_scalar(fog_factor));
    } else {
        fog_vs = ureg_scalar(ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_FOG, 0,
                                            TGSI_INTERPOLATE_PERSPECTIVE),
                                            TGSI_SWIZZLE_X);
        ureg_MOV(ureg, fog_factor, fog_vs);
    }

    ureg_LRP(ureg, ureg_writemask(oCol0, TGSI_WRITEMASK_XYZ),
             tx_src_scalar(fog_factor), src_col, fog_color);
    ureg_MOV(ureg, ureg_writemask(oCol0, TGSI_WRITEMASK_W), src_col);
}

#define GET_CAP(n) screen->get_param( \
      screen, PIPE_CAP_##n)
#define GET_SHADER_CAP(n) screen->get_shader_param( \
      screen, info->type, PIPE_SHADER_CAP_##n)

HRESULT
nine_translate_shader(struct NineDevice9 *device, struct nine_shader_info *info, struct pipe_context *pipe)
{
    struct shader_translator *tx;
    HRESULT hr = D3D_OK;
    const unsigned processor = info->type;
    struct pipe_screen *screen = info->process_vertices ? device->screen_sw : device->screen;

    user_assert(processor != ~0, D3DERR_INVALIDCALL);

    tx = CALLOC_STRUCT(shader_translator);
    if (!tx)
        return E_OUTOFMEMORY;
    tx_ctor(tx, info);

    if (((tx->version.major << 16) | tx->version.minor) > 0x00030000) {
        hr = D3DERR_INVALIDCALL;
        DBG("Unsupported shader version: %u.%u !\n",
            tx->version.major, tx->version.minor);
        goto out;
    }
    if (tx->processor != processor) {
        hr = D3DERR_INVALIDCALL;
        DBG("Shader type mismatch: %u / %u !\n", tx->processor, processor);
        goto out;
    }
    DUMP("%s%u.%u\n", processor == PIPE_SHADER_VERTEX ? "VS" : "PS",
         tx->version.major, tx->version.minor);

    tx->ureg = ureg_create(processor);
    if (!tx->ureg) {
        hr = E_OUTOFMEMORY;
        goto out;
    }

    tx->native_integers = GET_SHADER_CAP(INTEGERS);
    tx->inline_subroutines = !GET_SHADER_CAP(SUBROUTINES);
    tx->want_texcoord = GET_CAP(TGSI_TEXCOORD);
    tx->shift_wpos = !GET_CAP(TGSI_FS_COORD_PIXEL_CENTER_INTEGER);
    tx->texcoord_sn = tx->want_texcoord ?
        TGSI_SEMANTIC_TEXCOORD : TGSI_SEMANTIC_GENERIC;
    tx->wpos_is_sysval = GET_CAP(TGSI_FS_POSITION_IS_SYSVAL);
    tx->face_is_sysval_integer = GET_CAP(TGSI_FS_FACE_IS_INTEGER_SYSVAL);

    if (IS_VS) {
        tx->num_constf_allowed = NINE_MAX_CONST_F;
    } else if (tx->version.major < 2) {/* IS_PS v1 */
        tx->num_constf_allowed = 8;
    } else if (tx->version.major == 2) {/* IS_PS v2 */
        tx->num_constf_allowed = 32;
    } else {/* IS_PS v3 */
        tx->num_constf_allowed = NINE_MAX_CONST_F_PS3;
    }

    if (tx->version.major < 2) {
        tx->num_consti_allowed = 0;
        tx->num_constb_allowed = 0;
    } else {
        tx->num_consti_allowed = NINE_MAX_CONST_I;
        tx->num_constb_allowed = NINE_MAX_CONST_B;
    }

    if (IS_VS && tx->version.major >= 2 && info->swvp_on) {
        tx->num_constf_allowed = 8192;
        tx->num_consti_allowed = 2048;
        tx->num_constb_allowed = 2048;
    }

    /* VS must always write position. Declare it here to make it the 1st output.
     * (Some drivers like nv50 are buggy and rely on that.)
     */
    if (IS_VS) {
        tx->regs.oPos = ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_POSITION, 0);
    } else {
        ureg_property(tx->ureg, TGSI_PROPERTY_FS_COORD_ORIGIN, TGSI_FS_COORD_ORIGIN_UPPER_LEFT);
        if (!tx->shift_wpos)
            ureg_property(tx->ureg, TGSI_PROPERTY_FS_COORD_PIXEL_CENTER, TGSI_FS_COORD_PIXEL_CENTER_INTEGER);
    }

    if (GET_CAP(TGSI_MUL_ZERO_WINS))
       ureg_property(tx->ureg, TGSI_PROPERTY_MUL_ZERO_WINS, 1);

    while (!sm1_parse_eof(tx) && !tx->failure)
        sm1_parse_instruction(tx);
    tx->parse++; /* for byte_size */

    if (tx->failure) {
        /* For VS shaders, we print the warning later,
         * we first try with swvp. */
        if (IS_PS)
            ERR("Encountered buggy shader\n");
        ureg_destroy(tx->ureg);
        hr = D3DERR_INVALIDCALL;
        goto out;
    }

    if (IS_PS && tx->version.major < 3) {
        if (tx->version.major < 2) {
            assert(tx->num_temp); /* there must be color output */
            info->rt_mask |= 0x1;
            shader_add_ps_fog_stage(tx, ureg_src(tx->regs.r[0]));
        } else {
            shader_add_ps_fog_stage(tx, ureg_src(tx->regs.oCol[0]));
        }
    }

    if (IS_VS && tx->version.major < 3 && ureg_dst_is_undef(tx->regs.oFog) && info->fog_enable) {
        tx->regs.oFog = ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_FOG, 0);
        ureg_MOV(tx->ureg, ureg_writemask(tx->regs.oFog, TGSI_WRITEMASK_X), ureg_imm1f(tx->ureg, 0.0f));
    }

    if (info->position_t)
        ureg_property(tx->ureg, TGSI_PROPERTY_VS_WINDOW_SPACE_POSITION, TRUE);

    if (IS_VS && !ureg_dst_is_undef(tx->regs.oPts)) {
        struct ureg_dst oPts = ureg_DECL_output(tx->ureg, TGSI_SEMANTIC_PSIZE, 0);
        ureg_MAX(tx->ureg, tx->regs.oPts, ureg_src(tx->regs.oPts), ureg_imm1f(tx->ureg, info->point_size_min));
        ureg_MIN(tx->ureg, oPts, ureg_src(tx->regs.oPts), ureg_imm1f(tx->ureg, info->point_size_max));
        info->point_size = TRUE;
    }

    if (info->process_vertices)
        shader_add_vs_viewport_transform(tx);

    ureg_END(tx->ureg);

    /* record local constants */
    if (tx->num_lconstf && tx->indirect_const_access) {
        struct nine_range *ranges;
        float *data;
        int *indices;
        unsigned i, k, n;

        hr = E_OUTOFMEMORY;

        data = MALLOC(tx->num_lconstf * 4 * sizeof(float));
        if (!data)
            goto out;
        info->lconstf.data = data;

        indices = MALLOC(tx->num_lconstf * sizeof(indices[0]));
        if (!indices)
            goto out;

        /* lazy sort, num_lconstf should be small */
        for (n = 0; n < tx->num_lconstf; ++n) {
            for (k = 0, i = 0; i < tx->num_lconstf; ++i) {
                if (tx->lconstf[i].idx < tx->lconstf[k].idx)
                    k = i;
            }
            indices[n] = tx->lconstf[k].idx;
            memcpy(&data[n * 4], &tx->lconstf[k].f[0], 4 * sizeof(float));
            tx->lconstf[k].idx = INT_MAX;
        }

        /* count ranges */
        for (n = 1, i = 1; i < tx->num_lconstf; ++i)
            if (indices[i] != indices[i - 1] + 1)
                ++n;
        ranges = MALLOC(n * sizeof(ranges[0]));
        if (!ranges) {
            FREE(indices);
            goto out;
        }
        info->lconstf.ranges = ranges;

        k = 0;
        ranges[k].bgn = indices[0];
        for (i = 1; i < tx->num_lconstf; ++i) {
            if (indices[i] != indices[i - 1] + 1) {
                ranges[k].next = &ranges[k + 1];
                ranges[k].end = indices[i - 1] + 1;
                ++k;
                ranges[k].bgn = indices[i];
            }
        }
        ranges[k].end = indices[i - 1] + 1;
        ranges[k].next = NULL;
        assert(n == (k + 1));

        FREE(indices);
        hr = D3D_OK;
    }

    /* r500 */
    if (info->const_float_slots > device->max_vs_const_f &&
        (info->const_int_slots || info->const_bool_slots) &&
        (!IS_VS || !info->swvp_on))
        ERR("Overlapping constant slots. The shader is likely to be buggy\n");


    if (tx->indirect_const_access) /* vs only */
        info->const_float_slots = device->max_vs_const_f;

    if (!IS_VS || !info->swvp_on) {
        unsigned s, slot_max;
        unsigned max_const_f = IS_VS ? device->max_vs_const_f : device->max_ps_const_f;

        slot_max = info->const_bool_slots > 0 ?
                       max_const_f + NINE_MAX_CONST_I
                       + DIV_ROUND_UP(info->const_bool_slots, 4) :
                           info->const_int_slots > 0 ?
                               max_const_f + info->const_int_slots :
                                   info->const_float_slots;

        info->const_used_size = sizeof(float[4]) * slot_max; /* slots start from 1 */

        for (s = 0; s < slot_max; s++)
            ureg_DECL_constant(tx->ureg, s);
    } else {
         ureg_DECL_constant2D(tx->ureg, 0, 4095, 0);
         ureg_DECL_constant2D(tx->ureg, 0, 4095, 1);
         ureg_DECL_constant2D(tx->ureg, 0, 2047, 2);
         ureg_DECL_constant2D(tx->ureg, 0, 511, 3);
    }

    if (info->process_vertices)
        ureg_DECL_constant2D(tx->ureg, 0, 2, 4); /* Viewport data */

    if (debug_get_bool_option("NINE_TGSI_DUMP", FALSE)) {
        const struct tgsi_token *toks = ureg_get_tokens(tx->ureg, NULL);
        tgsi_dump(toks, 0);
        ureg_free_tokens(toks);
    }

    if (info->process_vertices) {
        NineVertexDeclaration9_FillStreamOutputInfo(info->vdecl_out,
                                                    tx->output_info,
                                                    tx->num_outputs,
                                                    &(info->so));
        info->cso = ureg_create_shader_with_so_and_destroy(tx->ureg, pipe, &(info->so));
    } else
        info->cso = ureg_create_shader_and_destroy(tx->ureg, pipe);
    if (!info->cso) {
        hr = D3DERR_DRIVERINTERNALERROR;
        FREE(info->lconstf.data);
        FREE(info->lconstf.ranges);
        goto out;
    }

    info->byte_size = (tx->parse - tx->byte_code) * sizeof(DWORD);
out:
    tx_dtor(tx);
    return hr;
}
