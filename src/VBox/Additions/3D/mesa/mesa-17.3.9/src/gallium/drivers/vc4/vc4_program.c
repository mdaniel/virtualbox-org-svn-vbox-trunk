/*
 * Copyright (c) 2014 Scott Mansell
 * Copyright © 2014 Broadcom
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

#include <inttypes.h>
#include "util/u_format.h"
#include "util/crc32.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/ralloc.h"
#include "util/hash_table.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_parse.h"
#include "compiler/nir/nir.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/nir_types.h"
#include "nir/tgsi_to_nir.h"
#include "vc4_context.h"
#include "vc4_qpu.h"
#include "vc4_qir.h"

static struct qreg
ntq_get_src(struct vc4_compile *c, nir_src src, int i);
static void
ntq_emit_cf_list(struct vc4_compile *c, struct exec_list *list);

static int
type_size(const struct glsl_type *type)
{
   return glsl_count_attribute_slots(type, false);
}

static void
resize_qreg_array(struct vc4_compile *c,
                  struct qreg **regs,
                  uint32_t *size,
                  uint32_t decl_size)
{
        if (*size >= decl_size)
                return;

        uint32_t old_size = *size;
        *size = MAX2(*size * 2, decl_size);
        *regs = reralloc(c, *regs, struct qreg, *size);
        if (!*regs) {
                fprintf(stderr, "Malloc failure\n");
                abort();
        }

        for (uint32_t i = old_size; i < *size; i++)
                (*regs)[i] = c->undef;
}

static void
ntq_emit_thrsw(struct vc4_compile *c)
{
        if (!c->fs_threaded)
                return;

        /* Always thread switch after each texture operation for now.
         *
         * We could do better by batching a bunch of texture fetches up and
         * then doing one thread switch and collecting all their results
         * afterward.
         */
        qir_emit_nondef(c, qir_inst(QOP_THRSW, c->undef,
                                    c->undef, c->undef));
        c->last_thrsw_at_top_level = (c->execute.file == QFILE_NULL);
}

static struct qreg
indirect_uniform_load(struct vc4_compile *c, nir_intrinsic_instr *intr)
{
        struct qreg indirect_offset = ntq_get_src(c, intr->src[0], 0);
        uint32_t offset = nir_intrinsic_base(intr);
        struct vc4_compiler_ubo_range *range = NULL;
        unsigned i;
        for (i = 0; i < c->num_uniform_ranges; i++) {
                range = &c->ubo_ranges[i];
                if (offset >= range->src_offset &&
                    offset < range->src_offset + range->size) {
                        break;
                }
        }
        /* The driver-location-based offset always has to be within a declared
         * uniform range.
         */
        assert(range);
        if (!range->used) {
                range->used = true;
                range->dst_offset = c->next_ubo_dst_offset;
                c->next_ubo_dst_offset += range->size;
                c->num_ubo_ranges++;
        }

        offset -= range->src_offset;

        /* Adjust for where we stored the TGSI register base. */
        indirect_offset = qir_ADD(c, indirect_offset,
                                  qir_uniform_ui(c, (range->dst_offset +
                                                     offset)));

        /* Clamp to [0, array size).  Note that MIN/MAX are signed. */
        indirect_offset = qir_MAX(c, indirect_offset, qir_uniform_ui(c, 0));
        indirect_offset = qir_MIN_NOIMM(c, indirect_offset,
                                        qir_uniform_ui(c, (range->dst_offset +
                                                           range->size - 4)));

        qir_ADD_dest(c, qir_reg(QFILE_TEX_S_DIRECT, 0),
                     indirect_offset,
                     qir_uniform(c, QUNIFORM_UBO_ADDR, 0));

        c->num_texture_samples++;

        ntq_emit_thrsw(c);

        return qir_TEX_RESULT(c);
}

nir_ssa_def *
vc4_nir_get_swizzled_channel(nir_builder *b, nir_ssa_def **srcs, int swiz)
{
        switch (swiz) {
        default:
        case PIPE_SWIZZLE_NONE:
                fprintf(stderr, "warning: unknown swizzle\n");
                /* FALLTHROUGH */
        case PIPE_SWIZZLE_0:
                return nir_imm_float(b, 0.0);
        case PIPE_SWIZZLE_1:
                return nir_imm_float(b, 1.0);
        case PIPE_SWIZZLE_X:
        case PIPE_SWIZZLE_Y:
        case PIPE_SWIZZLE_Z:
        case PIPE_SWIZZLE_W:
                return srcs[swiz];
        }
}

static struct qreg *
ntq_init_ssa_def(struct vc4_compile *c, nir_ssa_def *def)
{
        struct qreg *qregs = ralloc_array(c->def_ht, struct qreg,
                                          def->num_components);
        _mesa_hash_table_insert(c->def_ht, def, qregs);
        return qregs;
}

/**
 * This function is responsible for getting QIR results into the associated
 * storage for a NIR instruction.
 *
 * If it's a NIR SSA def, then we just set the associated hash table entry to
 * the new result.
 *
 * If it's a NIR reg, then we need to update the existing qreg assigned to the
 * NIR destination with the incoming value.  To do that without introducing
 * new MOVs, we require that the incoming qreg either be a uniform, or be
 * SSA-defined by the previous QIR instruction in the block and rewritable by
 * this function.  That lets us sneak ahead and insert the SF flag beforehand
 * (knowing that the previous instruction doesn't depend on flags) and rewrite
 * its destination to be the NIR reg's destination
 */
static void
ntq_store_dest(struct vc4_compile *c, nir_dest *dest, int chan,
               struct qreg result)
{
        struct qinst *last_inst = NULL;
        if (!list_empty(&c->cur_block->instructions))
                last_inst = (struct qinst *)c->cur_block->instructions.prev;

        assert(result.file == QFILE_UNIF ||
               (result.file == QFILE_TEMP &&
                last_inst && last_inst == c->defs[result.index]));

        if (dest->is_ssa) {
                assert(chan < dest->ssa.num_components);

                struct qreg *qregs;
                struct hash_entry *entry =
                        _mesa_hash_table_search(c->def_ht, &dest->ssa);

                if (entry)
                        qregs = entry->data;
                else
                        qregs = ntq_init_ssa_def(c, &dest->ssa);

                qregs[chan] = result;
        } else {
                nir_register *reg = dest->reg.reg;
                assert(dest->reg.base_offset == 0);
                assert(reg->num_array_elems == 0);
                struct hash_entry *entry =
                        _mesa_hash_table_search(c->def_ht, reg);
                struct qreg *qregs = entry->data;

                /* Insert a MOV if the source wasn't an SSA def in the
                 * previous instruction.
                 */
                if (result.file == QFILE_UNIF) {
                        result = qir_MOV(c, result);
                        last_inst = c->defs[result.index];
                }

                /* We know they're both temps, so just rewrite index. */
                c->defs[last_inst->dst.index] = NULL;
                last_inst->dst.index = qregs[chan].index;

                /* If we're in control flow, then make this update of the reg
                 * conditional on the execution mask.
                 */
                if (c->execute.file != QFILE_NULL) {
                        last_inst->dst.index = qregs[chan].index;

                        /* Set the flags to the current exec mask.  To insert
                         * the SF, we temporarily remove our SSA instruction.
                         */
                        list_del(&last_inst->link);
                        qir_SF(c, c->execute);
                        list_addtail(&last_inst->link,
                                     &c->cur_block->instructions);

                        last_inst->cond = QPU_COND_ZS;
                        last_inst->cond_is_exec_mask = true;
                }
        }
}

static struct qreg *
ntq_get_dest(struct vc4_compile *c, nir_dest *dest)
{
        if (dest->is_ssa) {
                struct qreg *qregs = ntq_init_ssa_def(c, &dest->ssa);
                for (int i = 0; i < dest->ssa.num_components; i++)
                        qregs[i] = c->undef;
                return qregs;
        } else {
                nir_register *reg = dest->reg.reg;
                assert(dest->reg.base_offset == 0);
                assert(reg->num_array_elems == 0);
                struct hash_entry *entry =
                        _mesa_hash_table_search(c->def_ht, reg);
                return entry->data;
        }
}

static struct qreg
ntq_get_src(struct vc4_compile *c, nir_src src, int i)
{
        struct hash_entry *entry;
        if (src.is_ssa) {
                entry = _mesa_hash_table_search(c->def_ht, src.ssa);
                assert(i < src.ssa->num_components);
        } else {
                nir_register *reg = src.reg.reg;
                entry = _mesa_hash_table_search(c->def_ht, reg);
                assert(reg->num_array_elems == 0);
                assert(src.reg.base_offset == 0);
                assert(i < reg->num_components);
        }

        struct qreg *qregs = entry->data;
        return qregs[i];
}

static struct qreg
ntq_get_alu_src(struct vc4_compile *c, nir_alu_instr *instr,
                unsigned src)
{
        assert(util_is_power_of_two(instr->dest.write_mask));
        unsigned chan = ffs(instr->dest.write_mask) - 1;
        struct qreg r = ntq_get_src(c, instr->src[src].src,
                                    instr->src[src].swizzle[chan]);

        assert(!instr->src[src].abs);
        assert(!instr->src[src].negate);

        return r;
};

static inline struct qreg
qir_SAT(struct vc4_compile *c, struct qreg val)
{
        return qir_FMAX(c,
                        qir_FMIN(c, val, qir_uniform_f(c, 1.0)),
                        qir_uniform_f(c, 0.0));
}

static struct qreg
ntq_rcp(struct vc4_compile *c, struct qreg x)
{
        struct qreg r = qir_RCP(c, x);

        /* Apply a Newton-Raphson step to improve the accuracy. */
        r = qir_FMUL(c, r, qir_FSUB(c,
                                    qir_uniform_f(c, 2.0),
                                    qir_FMUL(c, x, r)));

        return r;
}

static struct qreg
ntq_rsq(struct vc4_compile *c, struct qreg x)
{
        struct qreg r = qir_RSQ(c, x);

        /* Apply a Newton-Raphson step to improve the accuracy. */
        r = qir_FMUL(c, r, qir_FSUB(c,
                                    qir_uniform_f(c, 1.5),
                                    qir_FMUL(c,
                                             qir_uniform_f(c, 0.5),
                                             qir_FMUL(c, x,
                                                      qir_FMUL(c, r, r)))));

        return r;
}

static struct qreg
ntq_umul(struct vc4_compile *c, struct qreg src0, struct qreg src1)
{
        struct qreg src0_hi = qir_SHR(c, src0,
                                      qir_uniform_ui(c, 24));
        struct qreg src1_hi = qir_SHR(c, src1,
                                      qir_uniform_ui(c, 24));

        struct qreg hilo = qir_MUL24(c, src0_hi, src1);
        struct qreg lohi = qir_MUL24(c, src0, src1_hi);
        struct qreg lolo = qir_MUL24(c, src0, src1);

        return qir_ADD(c, lolo, qir_SHL(c,
                                        qir_ADD(c, hilo, lohi),
                                        qir_uniform_ui(c, 24)));
}

static struct qreg
ntq_scale_depth_texture(struct vc4_compile *c, struct qreg src)
{
        struct qreg depthf = qir_ITOF(c, qir_SHR(c, src,
                                                 qir_uniform_ui(c, 8)));
        return qir_FMUL(c, depthf, qir_uniform_f(c, 1.0f/0xffffff));
}

/**
 * Emits a lowered TXF_MS from an MSAA texture.
 *
 * The addressing math has been lowered in NIR, and now we just need to read
 * it like a UBO.
 */
static void
ntq_emit_txf(struct vc4_compile *c, nir_tex_instr *instr)
{
        uint32_t tile_width = 32;
        uint32_t tile_height = 32;
        uint32_t tile_size = (tile_height * tile_width *
                              VC4_MAX_SAMPLES * sizeof(uint32_t));

        unsigned unit = instr->texture_index;
        uint32_t w = align(c->key->tex[unit].msaa_width, tile_width);
        uint32_t w_tiles = w / tile_width;
        uint32_t h = align(c->key->tex[unit].msaa_height, tile_height);
        uint32_t h_tiles = h / tile_height;
        uint32_t size = w_tiles * h_tiles * tile_size;

        struct qreg addr;
        assert(instr->num_srcs == 1);
        assert(instr->src[0].src_type == nir_tex_src_coord);
        addr = ntq_get_src(c, instr->src[0].src, 0);

        /* Perform the clamping required by kernel validation. */
        addr = qir_MAX(c, addr, qir_uniform_ui(c, 0));
        addr = qir_MIN_NOIMM(c, addr, qir_uniform_ui(c, size - 4));

        qir_ADD_dest(c, qir_reg(QFILE_TEX_S_DIRECT, 0),
                     addr, qir_uniform(c, QUNIFORM_TEXTURE_MSAA_ADDR, unit));

        ntq_emit_thrsw(c);

        struct qreg tex = qir_TEX_RESULT(c);
        c->num_texture_samples++;

        enum pipe_format format = c->key->tex[unit].format;
        if (util_format_is_depth_or_stencil(format)) {
                struct qreg scaled = ntq_scale_depth_texture(c, tex);
                for (int i = 0; i < 4; i++)
                        ntq_store_dest(c, &instr->dest, i, qir_MOV(c, scaled));
        } else {
                for (int i = 0; i < 4; i++)
                        ntq_store_dest(c, &instr->dest, i,
                                       qir_UNPACK_8_F(c, tex, i));
        }
}

static void
ntq_emit_tex(struct vc4_compile *c, nir_tex_instr *instr)
{
        struct qreg s, t, r, lod, compare;
        bool is_txb = false, is_txl = false;
        unsigned unit = instr->texture_index;

        if (instr->op == nir_texop_txf) {
                ntq_emit_txf(c, instr);
                return;
        }

        for (unsigned i = 0; i < instr->num_srcs; i++) {
                switch (instr->src[i].src_type) {
                case nir_tex_src_coord:
                        s = ntq_get_src(c, instr->src[i].src, 0);
                        if (instr->sampler_dim == GLSL_SAMPLER_DIM_1D)
                                t = qir_uniform_f(c, 0.5);
                        else
                                t = ntq_get_src(c, instr->src[i].src, 1);
                        if (instr->sampler_dim == GLSL_SAMPLER_DIM_CUBE)
                                r = ntq_get_src(c, instr->src[i].src, 2);
                        break;
                case nir_tex_src_bias:
                        lod = ntq_get_src(c, instr->src[i].src, 0);
                        is_txb = true;
                        break;
                case nir_tex_src_lod:
                        lod = ntq_get_src(c, instr->src[i].src, 0);
                        is_txl = true;
                        break;
                case nir_tex_src_comparator:
                        compare = ntq_get_src(c, instr->src[i].src, 0);
                        break;
                default:
                        unreachable("unknown texture source");
                }
        }

        if (c->stage != QSTAGE_FRAG && !is_txl) {
                /* From the GLSL 1.20 spec:
                 *
                 *     "If it is mip-mapped and running on the vertex shader,
                 *      then the base texture is used."
                 */
                is_txl = true;
                lod = qir_uniform_ui(c, 0);
        }

        if (c->key->tex[unit].force_first_level) {
                lod = qir_uniform(c, QUNIFORM_TEXTURE_FIRST_LEVEL, unit);
                is_txl = true;
                is_txb = false;
        }

        struct qreg texture_u[] = {
                qir_uniform(c, QUNIFORM_TEXTURE_CONFIG_P0, unit),
                qir_uniform(c, QUNIFORM_TEXTURE_CONFIG_P1, unit),
                qir_uniform(c, QUNIFORM_CONSTANT, 0),
                qir_uniform(c, QUNIFORM_CONSTANT, 0),
        };
        uint32_t next_texture_u = 0;

        /* There is no native support for GL texture rectangle coordinates, so
         * we have to rescale from ([0, width], [0, height]) to ([0, 1], [0,
         * 1]).
         */
        if (instr->sampler_dim == GLSL_SAMPLER_DIM_RECT) {
                s = qir_FMUL(c, s,
                             qir_uniform(c, QUNIFORM_TEXRECT_SCALE_X, unit));
                t = qir_FMUL(c, t,
                             qir_uniform(c, QUNIFORM_TEXRECT_SCALE_Y, unit));
        }

        if (instr->sampler_dim == GLSL_SAMPLER_DIM_CUBE || is_txl) {
                texture_u[2] = qir_uniform(c, QUNIFORM_TEXTURE_CONFIG_P2,
                                           unit | (is_txl << 16));
        }

        struct qinst *tmu;
        if (instr->sampler_dim == GLSL_SAMPLER_DIM_CUBE) {
                tmu = qir_MOV_dest(c, qir_reg(QFILE_TEX_R, 0), r);
                tmu->src[qir_get_tex_uniform_src(tmu)] =
                        texture_u[next_texture_u++];
        } else if (c->key->tex[unit].wrap_s == PIPE_TEX_WRAP_CLAMP_TO_BORDER ||
                   c->key->tex[unit].wrap_s == PIPE_TEX_WRAP_CLAMP ||
                   c->key->tex[unit].wrap_t == PIPE_TEX_WRAP_CLAMP_TO_BORDER ||
                   c->key->tex[unit].wrap_t == PIPE_TEX_WRAP_CLAMP) {
                tmu = qir_MOV_dest(c, qir_reg(QFILE_TEX_R, 0),
                                   qir_uniform(c, QUNIFORM_TEXTURE_BORDER_COLOR,
                                               unit));
                tmu->src[qir_get_tex_uniform_src(tmu)] =
                        texture_u[next_texture_u++];
        }

        if (c->key->tex[unit].wrap_s == PIPE_TEX_WRAP_CLAMP) {
                s = qir_SAT(c, s);
        }

        if (c->key->tex[unit].wrap_t == PIPE_TEX_WRAP_CLAMP) {
                t = qir_SAT(c, t);
        }

        tmu = qir_MOV_dest(c, qir_reg(QFILE_TEX_T, 0), t);
        tmu->src[qir_get_tex_uniform_src(tmu)] =
                texture_u[next_texture_u++];

        if (is_txl || is_txb) {
                tmu = qir_MOV_dest(c, qir_reg(QFILE_TEX_B, 0), lod);
                tmu->src[qir_get_tex_uniform_src(tmu)] =
                        texture_u[next_texture_u++];
        }

        tmu = qir_MOV_dest(c, qir_reg(QFILE_TEX_S, 0), s);
        tmu->src[qir_get_tex_uniform_src(tmu)] = texture_u[next_texture_u++];

        c->num_texture_samples++;

        ntq_emit_thrsw(c);

        struct qreg tex = qir_TEX_RESULT(c);

        enum pipe_format format = c->key->tex[unit].format;

        struct qreg *dest = ntq_get_dest(c, &instr->dest);
        if (util_format_is_depth_or_stencil(format)) {
                struct qreg normalized = ntq_scale_depth_texture(c, tex);
                struct qreg depth_output;

                struct qreg u0 = qir_uniform_f(c, 0.0f);
                struct qreg u1 = qir_uniform_f(c, 1.0f);
                if (c->key->tex[unit].compare_mode) {
                        /* From the GL_ARB_shadow spec:
                         *
                         *     "Let Dt (D subscript t) be the depth texture
                         *      value, in the range [0, 1].  Let R be the
                         *      interpolated texture coordinate clamped to the
                         *      range [0, 1]."
                         */
                        compare = qir_SAT(c, compare);

                        switch (c->key->tex[unit].compare_func) {
                        case PIPE_FUNC_NEVER:
                                depth_output = qir_uniform_f(c, 0.0f);
                                break;
                        case PIPE_FUNC_ALWAYS:
                                depth_output = u1;
                                break;
                        case PIPE_FUNC_EQUAL:
                                qir_SF(c, qir_FSUB(c, compare, normalized));
                                depth_output = qir_SEL(c, QPU_COND_ZS, u1, u0);
                                break;
                        case PIPE_FUNC_NOTEQUAL:
                                qir_SF(c, qir_FSUB(c, compare, normalized));
                                depth_output = qir_SEL(c, QPU_COND_ZC, u1, u0);
                                break;
                        case PIPE_FUNC_GREATER:
                                qir_SF(c, qir_FSUB(c, compare, normalized));
                                depth_output = qir_SEL(c, QPU_COND_NC, u1, u0);
                                break;
                        case PIPE_FUNC_GEQUAL:
                                qir_SF(c, qir_FSUB(c, normalized, compare));
                                depth_output = qir_SEL(c, QPU_COND_NS, u1, u0);
                                break;
                        case PIPE_FUNC_LESS:
                                qir_SF(c, qir_FSUB(c, compare, normalized));
                                depth_output = qir_SEL(c, QPU_COND_NS, u1, u0);
                                break;
                        case PIPE_FUNC_LEQUAL:
                                qir_SF(c, qir_FSUB(c, normalized, compare));
                                depth_output = qir_SEL(c, QPU_COND_NC, u1, u0);
                                break;
                        }
                } else {
                        depth_output = normalized;
                }

                for (int i = 0; i < 4; i++)
                        dest[i] = depth_output;
        } else {
                for (int i = 0; i < 4; i++)
                        dest[i] = qir_UNPACK_8_F(c, tex, i);
        }
}

/**
 * Computes x - floor(x), which is tricky because our FTOI truncates (rounds
 * to zero).
 */
static struct qreg
ntq_ffract(struct vc4_compile *c, struct qreg src)
{
        struct qreg trunc = qir_ITOF(c, qir_FTOI(c, src));
        struct qreg diff = qir_FSUB(c, src, trunc);
        qir_SF(c, diff);

        qir_FADD_dest(c, diff,
                      diff, qir_uniform_f(c, 1.0))->cond = QPU_COND_NS;

        return qir_MOV(c, diff);
}

/**
 * Computes floor(x), which is tricky because our FTOI truncates (rounds to
 * zero).
 */
static struct qreg
ntq_ffloor(struct vc4_compile *c, struct qreg src)
{
        struct qreg result = qir_ITOF(c, qir_FTOI(c, src));

        /* This will be < 0 if we truncated and the truncation was of a value
         * that was < 0 in the first place.
         */
        qir_SF(c, qir_FSUB(c, src, result));

        struct qinst *sub = qir_FSUB_dest(c, result,
                                          result, qir_uniform_f(c, 1.0));
        sub->cond = QPU_COND_NS;

        return qir_MOV(c, result);
}

/**
 * Computes ceil(x), which is tricky because our FTOI truncates (rounds to
 * zero).
 */
static struct qreg
ntq_fceil(struct vc4_compile *c, struct qreg src)
{
        struct qreg result = qir_ITOF(c, qir_FTOI(c, src));

        /* This will be < 0 if we truncated and the truncation was of a value
         * that was > 0 in the first place.
         */
        qir_SF(c, qir_FSUB(c, result, src));

        qir_FADD_dest(c, result,
                      result, qir_uniform_f(c, 1.0))->cond = QPU_COND_NS;

        return qir_MOV(c, result);
}

static struct qreg
ntq_fsin(struct vc4_compile *c, struct qreg src)
{
        float coeff[] = {
                -2.0 * M_PI,
                pow(2.0 * M_PI, 3) / (3 * 2 * 1),
                -pow(2.0 * M_PI, 5) / (5 * 4 * 3 * 2 * 1),
                pow(2.0 * M_PI, 7) / (7 * 6 * 5 * 4 * 3 * 2 * 1),
                -pow(2.0 * M_PI, 9) / (9 * 8 * 7 * 6 * 5 * 4 * 3 * 2 * 1),
        };

        struct qreg scaled_x =
                qir_FMUL(c,
                         src,
                         qir_uniform_f(c, 1.0 / (M_PI * 2.0)));

        struct qreg x = qir_FADD(c,
                                 ntq_ffract(c, scaled_x),
                                 qir_uniform_f(c, -0.5));
        struct qreg x2 = qir_FMUL(c, x, x);
        struct qreg sum = qir_FMUL(c, x, qir_uniform_f(c, coeff[0]));
        for (int i = 1; i < ARRAY_SIZE(coeff); i++) {
                x = qir_FMUL(c, x, x2);
                sum = qir_FADD(c,
                               sum,
                               qir_FMUL(c,
                                        x,
                                        qir_uniform_f(c, coeff[i])));
        }
        return sum;
}

static struct qreg
ntq_fcos(struct vc4_compile *c, struct qreg src)
{
        float coeff[] = {
                -1.0f,
                pow(2.0 * M_PI, 2) / (2 * 1),
                -pow(2.0 * M_PI, 4) / (4 * 3 * 2 * 1),
                pow(2.0 * M_PI, 6) / (6 * 5 * 4 * 3 * 2 * 1),
                -pow(2.0 * M_PI, 8) / (8 * 7 * 6 * 5 * 4 * 3 * 2 * 1),
                pow(2.0 * M_PI, 10) / (10 * 9 * 8 * 7 * 6 * 5 * 4 * 3 * 2 * 1),
        };

        struct qreg scaled_x =
                qir_FMUL(c, src,
                         qir_uniform_f(c, 1.0f / (M_PI * 2.0f)));
        struct qreg x_frac = qir_FADD(c,
                                      ntq_ffract(c, scaled_x),
                                      qir_uniform_f(c, -0.5));

        struct qreg sum = qir_uniform_f(c, coeff[0]);
        struct qreg x2 = qir_FMUL(c, x_frac, x_frac);
        struct qreg x = x2; /* Current x^2, x^4, or x^6 */
        for (int i = 1; i < ARRAY_SIZE(coeff); i++) {
                if (i != 1)
                        x = qir_FMUL(c, x, x2);

                struct qreg mul = qir_FMUL(c,
                                           x,
                                           qir_uniform_f(c, coeff[i]));
                if (i == 0)
                        sum = mul;
                else
                        sum = qir_FADD(c, sum, mul);
        }
        return sum;
}

static struct qreg
ntq_fsign(struct vc4_compile *c, struct qreg src)
{
        struct qreg t = qir_get_temp(c);

        qir_SF(c, src);
        qir_MOV_dest(c, t, qir_uniform_f(c, 0.0));
        qir_MOV_dest(c, t, qir_uniform_f(c, 1.0))->cond = QPU_COND_ZC;
        qir_MOV_dest(c, t, qir_uniform_f(c, -1.0))->cond = QPU_COND_NS;
        return qir_MOV(c, t);
}

static void
emit_vertex_input(struct vc4_compile *c, int attr)
{
        enum pipe_format format = c->vs_key->attr_formats[attr];
        uint32_t attr_size = util_format_get_blocksize(format);

        c->vattr_sizes[attr] = align(attr_size, 4);
        for (int i = 0; i < align(attr_size, 4) / 4; i++) {
                c->inputs[attr * 4 + i] =
                        qir_MOV(c, qir_reg(QFILE_VPM, attr * 4 + i));
                c->num_inputs++;
        }
}

static void
emit_fragcoord_input(struct vc4_compile *c, int attr)
{
        c->inputs[attr * 4 + 0] = qir_ITOF(c, qir_reg(QFILE_FRAG_X, 0));
        c->inputs[attr * 4 + 1] = qir_ITOF(c, qir_reg(QFILE_FRAG_Y, 0));
        c->inputs[attr * 4 + 2] =
                qir_FMUL(c,
                         qir_ITOF(c, qir_FRAG_Z(c)),
                         qir_uniform_f(c, 1.0 / 0xffffff));
        c->inputs[attr * 4 + 3] = qir_RCP(c, qir_FRAG_W(c));
}

static struct qreg
emit_fragment_varying(struct vc4_compile *c, gl_varying_slot slot,
                      uint8_t swizzle)
{
        uint32_t i = c->num_input_slots++;
        struct qreg vary = {
                QFILE_VARY,
                i
        };

        if (c->num_input_slots >= c->input_slots_array_size) {
                c->input_slots_array_size =
                        MAX2(4, c->input_slots_array_size * 2);

                c->input_slots = reralloc(c, c->input_slots,
                                          struct vc4_varying_slot,
                                          c->input_slots_array_size);
        }

        c->input_slots[i].slot = slot;
        c->input_slots[i].swizzle = swizzle;

        return qir_VARY_ADD_C(c, qir_FMUL(c, vary, qir_FRAG_W(c)));
}

static void
emit_fragment_input(struct vc4_compile *c, int attr, gl_varying_slot slot)
{
        for (int i = 0; i < 4; i++) {
                c->inputs[attr * 4 + i] =
                        emit_fragment_varying(c, slot, i);
                c->num_inputs++;
        }
}

static void
add_output(struct vc4_compile *c,
           uint32_t decl_offset,
           uint8_t slot,
           uint8_t swizzle)
{
        uint32_t old_array_size = c->outputs_array_size;
        resize_qreg_array(c, &c->outputs, &c->outputs_array_size,
                          decl_offset + 1);

        if (old_array_size != c->outputs_array_size) {
                c->output_slots = reralloc(c,
                                           c->output_slots,
                                           struct vc4_varying_slot,
                                           c->outputs_array_size);
        }

        c->output_slots[decl_offset].slot = slot;
        c->output_slots[decl_offset].swizzle = swizzle;
}

static void
declare_uniform_range(struct vc4_compile *c, uint32_t start, uint32_t size)
{
        unsigned array_id = c->num_uniform_ranges++;
        if (array_id >= c->ubo_ranges_array_size) {
                c->ubo_ranges_array_size = MAX2(c->ubo_ranges_array_size * 2,
                                                array_id + 1);
                c->ubo_ranges = reralloc(c, c->ubo_ranges,
                                         struct vc4_compiler_ubo_range,
                                         c->ubo_ranges_array_size);
        }

        c->ubo_ranges[array_id].dst_offset = 0;
        c->ubo_ranges[array_id].src_offset = start;
        c->ubo_ranges[array_id].size = size;
        c->ubo_ranges[array_id].used = false;
}

static bool
ntq_src_is_only_ssa_def_user(nir_src *src)
{
        if (!src->is_ssa)
                return false;

        if (!list_empty(&src->ssa->if_uses))
                return false;

        return (src->ssa->uses.next == &src->use_link &&
                src->ssa->uses.next->next == &src->ssa->uses);
}

/**
 * In general, emits a nir_pack_unorm_4x8 as a series of MOVs with the pack
 * bit set.
 *
 * However, as an optimization, it tries to find the instructions generating
 * the sources to be packed and just emit the pack flag there, if possible.
 */
static void
ntq_emit_pack_unorm_4x8(struct vc4_compile *c, nir_alu_instr *instr)
{
        struct qreg result = qir_get_temp(c);
        struct nir_alu_instr *vec4 = NULL;

        /* If packing from a vec4 op (as expected), identify it so that we can
         * peek back at what generated its sources.
         */
        if (instr->src[0].src.is_ssa &&
            instr->src[0].src.ssa->parent_instr->type == nir_instr_type_alu &&
            nir_instr_as_alu(instr->src[0].src.ssa->parent_instr)->op ==
            nir_op_vec4) {
                vec4 = nir_instr_as_alu(instr->src[0].src.ssa->parent_instr);
        }

        /* If the pack is replicating the same channel 4 times, use the 8888
         * pack flag.  This is common for blending using the alpha
         * channel.
         */
        if (instr->src[0].swizzle[0] == instr->src[0].swizzle[1] &&
            instr->src[0].swizzle[0] == instr->src[0].swizzle[2] &&
            instr->src[0].swizzle[0] == instr->src[0].swizzle[3]) {
                struct qreg rep = ntq_get_src(c,
                                              instr->src[0].src,
                                              instr->src[0].swizzle[0]);
                ntq_store_dest(c, &instr->dest.dest, 0, qir_PACK_8888_F(c, rep));
                return;
        }

        for (int i = 0; i < 4; i++) {
                int swiz = instr->src[0].swizzle[i];
                struct qreg src;
                if (vec4) {
                        src = ntq_get_src(c, vec4->src[swiz].src,
                                          vec4->src[swiz].swizzle[0]);
                } else {
                        src = ntq_get_src(c, instr->src[0].src, swiz);
                }

                if (vec4 &&
                    ntq_src_is_only_ssa_def_user(&vec4->src[swiz].src) &&
                    src.file == QFILE_TEMP &&
                    c->defs[src.index] &&
                    qir_is_mul(c->defs[src.index]) &&
                    !c->defs[src.index]->dst.pack) {
                        struct qinst *rewrite = c->defs[src.index];
                        c->defs[src.index] = NULL;
                        rewrite->dst = result;
                        rewrite->dst.pack = QPU_PACK_MUL_8A + i;
                        continue;
                }

                qir_PACK_8_F(c, result, src, i);
        }

        ntq_store_dest(c, &instr->dest.dest, 0, qir_MOV(c, result));
}

/** Handles sign-extended bitfield extracts for 16 bits. */
static struct qreg
ntq_emit_ibfe(struct vc4_compile *c, struct qreg base, struct qreg offset,
              struct qreg bits)
{
        assert(bits.file == QFILE_UNIF &&
               c->uniform_contents[bits.index] == QUNIFORM_CONSTANT &&
               c->uniform_data[bits.index] == 16);

        assert(offset.file == QFILE_UNIF &&
               c->uniform_contents[offset.index] == QUNIFORM_CONSTANT);
        int offset_bit = c->uniform_data[offset.index];
        assert(offset_bit % 16 == 0);

        return qir_UNPACK_16_I(c, base, offset_bit / 16);
}

/** Handles unsigned bitfield extracts for 8 bits. */
static struct qreg
ntq_emit_ubfe(struct vc4_compile *c, struct qreg base, struct qreg offset,
              struct qreg bits)
{
        assert(bits.file == QFILE_UNIF &&
               c->uniform_contents[bits.index] == QUNIFORM_CONSTANT &&
               c->uniform_data[bits.index] == 8);

        assert(offset.file == QFILE_UNIF &&
               c->uniform_contents[offset.index] == QUNIFORM_CONSTANT);
        int offset_bit = c->uniform_data[offset.index];
        assert(offset_bit % 8 == 0);

        return qir_UNPACK_8_I(c, base, offset_bit / 8);
}

/**
 * If compare_instr is a valid comparison instruction, emits the
 * compare_instr's comparison and returns the sel_instr's return value based
 * on the compare_instr's result.
 */
static bool
ntq_emit_comparison(struct vc4_compile *c, struct qreg *dest,
                    nir_alu_instr *compare_instr,
                    nir_alu_instr *sel_instr)
{
        enum qpu_cond cond;

        switch (compare_instr->op) {
        case nir_op_feq:
        case nir_op_ieq:
        case nir_op_seq:
                cond = QPU_COND_ZS;
                break;
        case nir_op_fne:
        case nir_op_ine:
        case nir_op_sne:
                cond = QPU_COND_ZC;
                break;
        case nir_op_fge:
        case nir_op_ige:
        case nir_op_uge:
        case nir_op_sge:
                cond = QPU_COND_NC;
                break;
        case nir_op_flt:
        case nir_op_ilt:
        case nir_op_slt:
                cond = QPU_COND_NS;
                break;
        default:
                return false;
        }

        struct qreg src0 = ntq_get_alu_src(c, compare_instr, 0);
        struct qreg src1 = ntq_get_alu_src(c, compare_instr, 1);

        unsigned unsized_type =
                nir_alu_type_get_base_type(nir_op_infos[compare_instr->op].input_types[0]);
        if (unsized_type == nir_type_float)
                qir_SF(c, qir_FSUB(c, src0, src1));
        else
                qir_SF(c, qir_SUB(c, src0, src1));

        switch (sel_instr->op) {
        case nir_op_seq:
        case nir_op_sne:
        case nir_op_sge:
        case nir_op_slt:
                *dest = qir_SEL(c, cond,
                                qir_uniform_f(c, 1.0), qir_uniform_f(c, 0.0));
                break;

        case nir_op_bcsel:
                *dest = qir_SEL(c, cond,
                                ntq_get_alu_src(c, sel_instr, 1),
                                ntq_get_alu_src(c, sel_instr, 2));
                break;

        default:
                *dest = qir_SEL(c, cond,
                                qir_uniform_ui(c, ~0), qir_uniform_ui(c, 0));
                break;
        }

        /* Make the temporary for nir_store_dest(). */
        *dest = qir_MOV(c, *dest);

        return true;
}

/**
 * Attempts to fold a comparison generating a boolean result into the
 * condition code for selecting between two values, instead of comparing the
 * boolean result against 0 to generate the condition code.
 */
static struct qreg ntq_emit_bcsel(struct vc4_compile *c, nir_alu_instr *instr,
                                  struct qreg *src)
{
        if (!instr->src[0].src.is_ssa)
                goto out;
        if (instr->src[0].src.ssa->parent_instr->type != nir_instr_type_alu)
                goto out;
        nir_alu_instr *compare =
                nir_instr_as_alu(instr->src[0].src.ssa->parent_instr);
        if (!compare)
                goto out;

        struct qreg dest;
        if (ntq_emit_comparison(c, &dest, compare, instr))
                return dest;

out:
        qir_SF(c, src[0]);
        return qir_MOV(c, qir_SEL(c, QPU_COND_NS, src[1], src[2]));
}

static struct qreg
ntq_fddx(struct vc4_compile *c, struct qreg src)
{
        /* Make sure that we have a bare temp to use for MUL rotation, so it
         * can be allocated to an accumulator.
         */
        if (src.pack || src.file != QFILE_TEMP)
                src = qir_MOV(c, src);

        struct qreg from_left = qir_ROT_MUL(c, src, 1);
        struct qreg from_right = qir_ROT_MUL(c, src, 15);

        /* Distinguish left/right pixels of the quad. */
        qir_SF(c, qir_AND(c, qir_reg(QFILE_QPU_ELEMENT, 0),
                          qir_uniform_ui(c, 1)));

        return qir_MOV(c, qir_SEL(c, QPU_COND_ZS,
                                  qir_FSUB(c, from_right, src),
                                  qir_FSUB(c, src, from_left)));
}

static struct qreg
ntq_fddy(struct vc4_compile *c, struct qreg src)
{
        if (src.pack || src.file != QFILE_TEMP)
                src = qir_MOV(c, src);

        struct qreg from_bottom = qir_ROT_MUL(c, src, 2);
        struct qreg from_top = qir_ROT_MUL(c, src, 14);

        /* Distinguish top/bottom pixels of the quad. */
        qir_SF(c, qir_AND(c,
                          qir_reg(QFILE_QPU_ELEMENT, 0),
                          qir_uniform_ui(c, 2)));

        return qir_MOV(c, qir_SEL(c, QPU_COND_ZS,
                                  qir_FSUB(c, from_top, src),
                                  qir_FSUB(c, src, from_bottom)));
}

static void
ntq_emit_alu(struct vc4_compile *c, nir_alu_instr *instr)
{
        /* This should always be lowered to ALU operations for VC4. */
        assert(!instr->dest.saturate);

        /* Vectors are special in that they have non-scalarized writemasks,
         * and just take the first swizzle channel for each argument in order
         * into each writemask channel.
         */
        if (instr->op == nir_op_vec2 ||
            instr->op == nir_op_vec3 ||
            instr->op == nir_op_vec4) {
                struct qreg srcs[4];
                for (int i = 0; i < nir_op_infos[instr->op].num_inputs; i++)
                        srcs[i] = ntq_get_src(c, instr->src[i].src,
                                              instr->src[i].swizzle[0]);
                for (int i = 0; i < nir_op_infos[instr->op].num_inputs; i++)
                        ntq_store_dest(c, &instr->dest.dest, i,
                                       qir_MOV(c, srcs[i]));
                return;
        }

        if (instr->op == nir_op_pack_unorm_4x8) {
                ntq_emit_pack_unorm_4x8(c, instr);
                return;
        }

        if (instr->op == nir_op_unpack_unorm_4x8) {
                struct qreg src = ntq_get_src(c, instr->src[0].src,
                                              instr->src[0].swizzle[0]);
                for (int i = 0; i < 4; i++) {
                        if (instr->dest.write_mask & (1 << i))
                                ntq_store_dest(c, &instr->dest.dest, i,
                                               qir_UNPACK_8_F(c, src, i));
                }
                return;
        }

        /* General case: We can just grab the one used channel per src. */
        struct qreg src[nir_op_infos[instr->op].num_inputs];
        for (int i = 0; i < nir_op_infos[instr->op].num_inputs; i++) {
                src[i] = ntq_get_alu_src(c, instr, i);
        }

        struct qreg result;

        switch (instr->op) {
        case nir_op_fmov:
        case nir_op_imov:
                result = qir_MOV(c, src[0]);
                break;
        case nir_op_fmul:
                result = qir_FMUL(c, src[0], src[1]);
                break;
        case nir_op_fadd:
                result = qir_FADD(c, src[0], src[1]);
                break;
        case nir_op_fsub:
                result = qir_FSUB(c, src[0], src[1]);
                break;
        case nir_op_fmin:
                result = qir_FMIN(c, src[0], src[1]);
                break;
        case nir_op_fmax:
                result = qir_FMAX(c, src[0], src[1]);
                break;

        case nir_op_f2i32:
        case nir_op_f2u32:
                result = qir_FTOI(c, src[0]);
                break;
        case nir_op_i2f32:
        case nir_op_u2f32:
                result = qir_ITOF(c, src[0]);
                break;
        case nir_op_b2f:
                result = qir_AND(c, src[0], qir_uniform_f(c, 1.0));
                break;
        case nir_op_b2i:
                result = qir_AND(c, src[0], qir_uniform_ui(c, 1));
                break;
        case nir_op_i2b:
        case nir_op_f2b:
                qir_SF(c, src[0]);
                result = qir_MOV(c, qir_SEL(c, QPU_COND_ZC,
                                            qir_uniform_ui(c, ~0),
                                            qir_uniform_ui(c, 0)));
                break;

        case nir_op_iadd:
                result = qir_ADD(c, src[0], src[1]);
                break;
        case nir_op_ushr:
                result = qir_SHR(c, src[0], src[1]);
                break;
        case nir_op_isub:
                result = qir_SUB(c, src[0], src[1]);
                break;
        case nir_op_ishr:
                result = qir_ASR(c, src[0], src[1]);
                break;
        case nir_op_ishl:
                result = qir_SHL(c, src[0], src[1]);
                break;
        case nir_op_imin:
                result = qir_MIN(c, src[0], src[1]);
                break;
        case nir_op_imax:
                result = qir_MAX(c, src[0], src[1]);
                break;
        case nir_op_iand:
                result = qir_AND(c, src[0], src[1]);
                break;
        case nir_op_ior:
                result = qir_OR(c, src[0], src[1]);
                break;
        case nir_op_ixor:
                result = qir_XOR(c, src[0], src[1]);
                break;
        case nir_op_inot:
                result = qir_NOT(c, src[0]);
                break;

        case nir_op_imul:
                result = ntq_umul(c, src[0], src[1]);
                break;

        case nir_op_seq:
        case nir_op_sne:
        case nir_op_sge:
        case nir_op_slt:
        case nir_op_feq:
        case nir_op_fne:
        case nir_op_fge:
        case nir_op_flt:
        case nir_op_ieq:
        case nir_op_ine:
        case nir_op_ige:
        case nir_op_uge:
        case nir_op_ilt:
                if (!ntq_emit_comparison(c, &result, instr, instr)) {
                        fprintf(stderr, "Bad comparison instruction\n");
                }
                break;

        case nir_op_bcsel:
                result = ntq_emit_bcsel(c, instr, src);
                break;
        case nir_op_fcsel:
                qir_SF(c, src[0]);
                result = qir_MOV(c, qir_SEL(c, QPU_COND_ZC, src[1], src[2]));
                break;

        case nir_op_frcp:
                result = ntq_rcp(c, src[0]);
                break;
        case nir_op_frsq:
                result = ntq_rsq(c, src[0]);
                break;
        case nir_op_fexp2:
                result = qir_EXP2(c, src[0]);
                break;
        case nir_op_flog2:
                result = qir_LOG2(c, src[0]);
                break;

        case nir_op_ftrunc:
                result = qir_ITOF(c, qir_FTOI(c, src[0]));
                break;
        case nir_op_fceil:
                result = ntq_fceil(c, src[0]);
                break;
        case nir_op_ffract:
                result = ntq_ffract(c, src[0]);
                break;
        case nir_op_ffloor:
                result = ntq_ffloor(c, src[0]);
                break;

        case nir_op_fsin:
                result = ntq_fsin(c, src[0]);
                break;
        case nir_op_fcos:
                result = ntq_fcos(c, src[0]);
                break;

        case nir_op_fsign:
                result = ntq_fsign(c, src[0]);
                break;

        case nir_op_fabs:
                result = qir_FMAXABS(c, src[0], src[0]);
                break;
        case nir_op_iabs:
                result = qir_MAX(c, src[0],
                                qir_SUB(c, qir_uniform_ui(c, 0), src[0]));
                break;

        case nir_op_ibitfield_extract:
                result = ntq_emit_ibfe(c, src[0], src[1], src[2]);
                break;

        case nir_op_ubitfield_extract:
                result = ntq_emit_ubfe(c, src[0], src[1], src[2]);
                break;

        case nir_op_usadd_4x8:
                result = qir_V8ADDS(c, src[0], src[1]);
                break;

        case nir_op_ussub_4x8:
                result = qir_V8SUBS(c, src[0], src[1]);
                break;

        case nir_op_umin_4x8:
                result = qir_V8MIN(c, src[0], src[1]);
                break;

        case nir_op_umax_4x8:
                result = qir_V8MAX(c, src[0], src[1]);
                break;

        case nir_op_umul_unorm_4x8:
                result = qir_V8MULD(c, src[0], src[1]);
                break;

        case nir_op_fddx:
        case nir_op_fddx_coarse:
        case nir_op_fddx_fine:
                result = ntq_fddx(c, src[0]);
                break;

        case nir_op_fddy:
        case nir_op_fddy_coarse:
        case nir_op_fddy_fine:
                result = ntq_fddy(c, src[0]);
                break;

        default:
                fprintf(stderr, "unknown NIR ALU inst: ");
                nir_print_instr(&instr->instr, stderr);
                fprintf(stderr, "\n");
                abort();
        }

        /* We have a scalar result, so the instruction should only have a
         * single channel written to.
         */
        assert(util_is_power_of_two(instr->dest.write_mask));
        ntq_store_dest(c, &instr->dest.dest,
                       ffs(instr->dest.write_mask) - 1, result);
}

static void
emit_frag_end(struct vc4_compile *c)
{
        struct qreg color;
        if (c->output_color_index != -1) {
                color = c->outputs[c->output_color_index];
        } else {
                color = qir_uniform_ui(c, 0);
        }

        uint32_t discard_cond = QPU_COND_ALWAYS;
        if (c->s->info.fs.uses_discard) {
                qir_SF(c, c->discard);
                discard_cond = QPU_COND_ZS;
        }

        if (c->fs_key->stencil_enabled) {
                qir_MOV_dest(c, qir_reg(QFILE_TLB_STENCIL_SETUP, 0),
                             qir_uniform(c, QUNIFORM_STENCIL, 0));
                if (c->fs_key->stencil_twoside) {
                        qir_MOV_dest(c, qir_reg(QFILE_TLB_STENCIL_SETUP, 0),
                                     qir_uniform(c, QUNIFORM_STENCIL, 1));
                }
                if (c->fs_key->stencil_full_writemasks) {
                        qir_MOV_dest(c, qir_reg(QFILE_TLB_STENCIL_SETUP, 0),
                                     qir_uniform(c, QUNIFORM_STENCIL, 2));
                }
        }

        if (c->output_sample_mask_index != -1) {
                qir_MS_MASK(c, c->outputs[c->output_sample_mask_index]);
        }

        if (c->fs_key->depth_enabled) {
                if (c->output_position_index != -1) {
                        qir_FTOI_dest(c, qir_reg(QFILE_TLB_Z_WRITE, 0),
                                      qir_FMUL(c,
                                               c->outputs[c->output_position_index],
                                               qir_uniform_f(c, 0xffffff)))->cond = discard_cond;
                } else {
                        qir_MOV_dest(c, qir_reg(QFILE_TLB_Z_WRITE, 0),
                                     qir_FRAG_Z(c))->cond = discard_cond;
                }
        }

        if (!c->msaa_per_sample_output) {
                qir_MOV_dest(c, qir_reg(QFILE_TLB_COLOR_WRITE, 0),
                             color)->cond = discard_cond;
        } else {
                for (int i = 0; i < VC4_MAX_SAMPLES; i++) {
                        qir_MOV_dest(c, qir_reg(QFILE_TLB_COLOR_WRITE_MS, 0),
                                     c->sample_colors[i])->cond = discard_cond;
                }
        }
}

static void
emit_scaled_viewport_write(struct vc4_compile *c, struct qreg rcp_w)
{
        struct qreg packed = qir_get_temp(c);

        for (int i = 0; i < 2; i++) {
                struct qreg scale =
                        qir_uniform(c, QUNIFORM_VIEWPORT_X_SCALE + i, 0);

                struct qreg packed_chan = packed;
                packed_chan.pack = QPU_PACK_A_16A + i;

                qir_FTOI_dest(c, packed_chan,
                              qir_FMUL(c,
                                       qir_FMUL(c,
                                                c->outputs[c->output_position_index + i],
                                                scale),
                                       rcp_w));
        }

        qir_VPM_WRITE(c, packed);
}

static void
emit_zs_write(struct vc4_compile *c, struct qreg rcp_w)
{
        struct qreg zscale = qir_uniform(c, QUNIFORM_VIEWPORT_Z_SCALE, 0);
        struct qreg zoffset = qir_uniform(c, QUNIFORM_VIEWPORT_Z_OFFSET, 0);

        qir_VPM_WRITE(c, qir_FADD(c, qir_FMUL(c, qir_FMUL(c,
                                                          c->outputs[c->output_position_index + 2],
                                                          zscale),
                                              rcp_w),
                                  zoffset));
}

static void
emit_rcp_wc_write(struct vc4_compile *c, struct qreg rcp_w)
{
        qir_VPM_WRITE(c, rcp_w);
}

static void
emit_point_size_write(struct vc4_compile *c)
{
        struct qreg point_size;

        if (c->output_point_size_index != -1)
                point_size = c->outputs[c->output_point_size_index];
        else
                point_size = qir_uniform_f(c, 1.0);

        /* Workaround: HW-2726 PTB does not handle zero-size points (BCM2835,
         * BCM21553).
         */
        point_size = qir_FMAX(c, point_size, qir_uniform_f(c, .125));

        qir_VPM_WRITE(c, point_size);
}

/**
 * Emits a VPM read of the stub vertex attribute set up by vc4_draw.c.
 *
 * The simulator insists that there be at least one vertex attribute, so
 * vc4_draw.c will emit one if it wouldn't have otherwise.  The simulator also
 * insists that all vertex attributes loaded get read by the VS/CS, so we have
 * to consume it here.
 */
static void
emit_stub_vpm_read(struct vc4_compile *c)
{
        if (c->num_inputs)
                return;

        c->vattr_sizes[0] = 4;
        (void)qir_MOV(c, qir_reg(QFILE_VPM, 0));
        c->num_inputs++;
}

static void
emit_vert_end(struct vc4_compile *c,
              struct vc4_varying_slot *fs_inputs,
              uint32_t num_fs_inputs)
{
        struct qreg rcp_w = ntq_rcp(c, c->outputs[c->output_position_index + 3]);

        emit_stub_vpm_read(c);

        emit_scaled_viewport_write(c, rcp_w);
        emit_zs_write(c, rcp_w);
        emit_rcp_wc_write(c, rcp_w);
        if (c->vs_key->per_vertex_point_size)
                emit_point_size_write(c);

        for (int i = 0; i < num_fs_inputs; i++) {
                struct vc4_varying_slot *input = &fs_inputs[i];
                int j;

                for (j = 0; j < c->num_outputs; j++) {
                        struct vc4_varying_slot *output =
                                &c->output_slots[j];

                        if (input->slot == output->slot &&
                            input->swizzle == output->swizzle) {
                                qir_VPM_WRITE(c, c->outputs[j]);
                                break;
                        }
                }
                /* Emit padding if we didn't find a declared VS output for
                 * this FS input.
                 */
                if (j == c->num_outputs)
                        qir_VPM_WRITE(c, qir_uniform_f(c, 0.0));
        }
}

static void
emit_coord_end(struct vc4_compile *c)
{
        struct qreg rcp_w = ntq_rcp(c, c->outputs[c->output_position_index + 3]);

        emit_stub_vpm_read(c);

        for (int i = 0; i < 4; i++)
                qir_VPM_WRITE(c, c->outputs[c->output_position_index + i]);

        emit_scaled_viewport_write(c, rcp_w);
        emit_zs_write(c, rcp_w);
        emit_rcp_wc_write(c, rcp_w);
        if (c->vs_key->per_vertex_point_size)
                emit_point_size_write(c);
}

static void
vc4_optimize_nir(struct nir_shader *s)
{
        bool progress;

        do {
                progress = false;

                NIR_PASS_V(s, nir_lower_vars_to_ssa);
                NIR_PASS(progress, s, nir_lower_alu_to_scalar);
                NIR_PASS(progress, s, nir_lower_phis_to_scalar);
                NIR_PASS(progress, s, nir_copy_prop);
                NIR_PASS(progress, s, nir_opt_remove_phis);
                NIR_PASS(progress, s, nir_opt_dce);
                NIR_PASS(progress, s, nir_opt_dead_cf);
                NIR_PASS(progress, s, nir_opt_cse);
                NIR_PASS(progress, s, nir_opt_peephole_select, 8);
                NIR_PASS(progress, s, nir_opt_algebraic);
                NIR_PASS(progress, s, nir_opt_constant_folding);
                NIR_PASS(progress, s, nir_opt_undef);
                NIR_PASS(progress, s, nir_opt_loop_unroll,
                         nir_var_shader_in |
                         nir_var_shader_out |
                         nir_var_local);
        } while (progress);
}

static int
driver_location_compare(const void *in_a, const void *in_b)
{
        const nir_variable *const *a = in_a;
        const nir_variable *const *b = in_b;

        return (*a)->data.driver_location - (*b)->data.driver_location;
}

static void
ntq_setup_inputs(struct vc4_compile *c)
{
        unsigned num_entries = 0;
        nir_foreach_variable(var, &c->s->inputs)
                num_entries++;

        nir_variable *vars[num_entries];

        unsigned i = 0;
        nir_foreach_variable(var, &c->s->inputs)
                vars[i++] = var;

        /* Sort the variables so that we emit the input setup in
         * driver_location order.  This is required for VPM reads, whose data
         * is fetched into the VPM in driver_location (TGSI register index)
         * order.
         */
        qsort(&vars, num_entries, sizeof(*vars), driver_location_compare);

        for (unsigned i = 0; i < num_entries; i++) {
                nir_variable *var = vars[i];
                unsigned array_len = MAX2(glsl_get_length(var->type), 1);
                unsigned loc = var->data.driver_location;

                assert(array_len == 1);
                (void)array_len;
                resize_qreg_array(c, &c->inputs, &c->inputs_array_size,
                                  (loc + 1) * 4);

                if (c->stage == QSTAGE_FRAG) {
                        if (var->data.location == VARYING_SLOT_POS) {
                                emit_fragcoord_input(c, loc);
                        } else if (var->data.location == VARYING_SLOT_PNTC ||
                                   (var->data.location >= VARYING_SLOT_VAR0 &&
                                    (c->fs_key->point_sprite_mask &
                                     (1 << (var->data.location -
                                            VARYING_SLOT_VAR0))))) {
                                c->inputs[loc * 4 + 0] = c->point_x;
                                c->inputs[loc * 4 + 1] = c->point_y;
                        } else {
                                emit_fragment_input(c, loc, var->data.location);
                        }
                } else {
                        emit_vertex_input(c, loc);
                }
        }
}

static void
ntq_setup_outputs(struct vc4_compile *c)
{
        nir_foreach_variable(var, &c->s->outputs) {
                unsigned array_len = MAX2(glsl_get_length(var->type), 1);
                unsigned loc = var->data.driver_location * 4;

                assert(array_len == 1);
                (void)array_len;

                for (int i = 0; i < 4; i++)
                        add_output(c, loc + i, var->data.location, i);

                if (c->stage == QSTAGE_FRAG) {
                        switch (var->data.location) {
                        case FRAG_RESULT_COLOR:
                        case FRAG_RESULT_DATA0:
                                c->output_color_index = loc;
                                break;
                        case FRAG_RESULT_DEPTH:
                                c->output_position_index = loc;
                                break;
                        case FRAG_RESULT_SAMPLE_MASK:
                                c->output_sample_mask_index = loc;
                                break;
                        }
                } else {
                        switch (var->data.location) {
                        case VARYING_SLOT_POS:
                                c->output_position_index = loc;
                                break;
                        case VARYING_SLOT_PSIZ:
                                c->output_point_size_index = loc;
                                break;
                        }
                }
        }
}

static void
ntq_setup_uniforms(struct vc4_compile *c)
{
        nir_foreach_variable(var, &c->s->uniforms) {
                uint32_t vec4_count = type_size(var->type);
                unsigned vec4_size = 4 * sizeof(float);

                declare_uniform_range(c, var->data.driver_location * vec4_size,
                                      vec4_count * vec4_size);

        }
}

/**
 * Sets up the mapping from nir_register to struct qreg *.
 *
 * Each nir_register gets a struct qreg per 32-bit component being stored.
 */
static void
ntq_setup_registers(struct vc4_compile *c, struct exec_list *list)
{
        foreach_list_typed(nir_register, nir_reg, node, list) {
                unsigned array_len = MAX2(nir_reg->num_array_elems, 1);
                struct qreg *qregs = ralloc_array(c->def_ht, struct qreg,
                                                  array_len *
                                                  nir_reg->num_components);

                _mesa_hash_table_insert(c->def_ht, nir_reg, qregs);

                for (int i = 0; i < array_len * nir_reg->num_components; i++)
                        qregs[i] = qir_get_temp(c);
        }
}

static void
ntq_emit_load_const(struct vc4_compile *c, nir_load_const_instr *instr)
{
        struct qreg *qregs = ntq_init_ssa_def(c, &instr->def);
        for (int i = 0; i < instr->def.num_components; i++)
                qregs[i] = qir_uniform_ui(c, instr->value.u32[i]);

        _mesa_hash_table_insert(c->def_ht, &instr->def, qregs);
}

static void
ntq_emit_ssa_undef(struct vc4_compile *c, nir_ssa_undef_instr *instr)
{
        struct qreg *qregs = ntq_init_ssa_def(c, &instr->def);

        /* QIR needs there to be *some* value, so pick 0 (same as for
         * ntq_setup_registers().
         */
        for (int i = 0; i < instr->def.num_components; i++)
                qregs[i] = qir_uniform_ui(c, 0);
}

static void
ntq_emit_color_read(struct vc4_compile *c, nir_intrinsic_instr *instr)
{
        assert(nir_src_as_const_value(instr->src[0])->u32[0] == 0);

        /* Reads of the per-sample color need to be done in
         * order.
         */
        int sample_index = (nir_intrinsic_base(instr) -
                            VC4_NIR_TLB_COLOR_READ_INPUT);
        for (int i = 0; i <= sample_index; i++) {
                if (c->color_reads[i].file == QFILE_NULL) {
                        c->color_reads[i] =
                                qir_TLB_COLOR_READ(c);
                }
        }
        ntq_store_dest(c, &instr->dest, 0,
                       qir_MOV(c, c->color_reads[sample_index]));
}

static void
ntq_emit_load_input(struct vc4_compile *c, nir_intrinsic_instr *instr)
{
        assert(instr->num_components == 1);

        nir_const_value *const_offset = nir_src_as_const_value(instr->src[0]);
        assert(const_offset && "vc4 doesn't support indirect inputs");

        if (c->stage == QSTAGE_FRAG &&
            nir_intrinsic_base(instr) >= VC4_NIR_TLB_COLOR_READ_INPUT) {
                ntq_emit_color_read(c, instr);
                return;
        }

        uint32_t offset = nir_intrinsic_base(instr) + const_offset->u32[0];
        int comp = nir_intrinsic_component(instr);
        ntq_store_dest(c, &instr->dest, 0,
                       qir_MOV(c, c->inputs[offset * 4 + comp]));
}

static void
ntq_emit_intrinsic(struct vc4_compile *c, nir_intrinsic_instr *instr)
{
        nir_const_value *const_offset;
        unsigned offset;

        switch (instr->intrinsic) {
        case nir_intrinsic_load_uniform:
                assert(instr->num_components == 1);
                const_offset = nir_src_as_const_value(instr->src[0]);
                if (const_offset) {
                        offset = nir_intrinsic_base(instr) + const_offset->u32[0];
                        assert(offset % 4 == 0);
                        /* We need dwords */
                        offset = offset / 4;
                        ntq_store_dest(c, &instr->dest, 0,
                                       qir_uniform(c, QUNIFORM_UNIFORM,
                                                   offset));
                } else {
                        ntq_store_dest(c, &instr->dest, 0,
                                       indirect_uniform_load(c, instr));
                }
                break;

        case nir_intrinsic_load_user_clip_plane:
                for (int i = 0; i < instr->num_components; i++) {
                        ntq_store_dest(c, &instr->dest, i,
                                       qir_uniform(c, QUNIFORM_USER_CLIP_PLANE,
                                                   nir_intrinsic_ucp_id(instr) *
                                                   4 + i));
                }
                break;

        case nir_intrinsic_load_blend_const_color_r_float:
        case nir_intrinsic_load_blend_const_color_g_float:
        case nir_intrinsic_load_blend_const_color_b_float:
        case nir_intrinsic_load_blend_const_color_a_float:
                ntq_store_dest(c, &instr->dest, 0,
                               qir_uniform(c, QUNIFORM_BLEND_CONST_COLOR_X +
                                           (instr->intrinsic -
                                            nir_intrinsic_load_blend_const_color_r_float),
                                           0));
                break;

        case nir_intrinsic_load_blend_const_color_rgba8888_unorm:
                ntq_store_dest(c, &instr->dest, 0,
                               qir_uniform(c, QUNIFORM_BLEND_CONST_COLOR_RGBA,
                                           0));
                break;

        case nir_intrinsic_load_blend_const_color_aaaa8888_unorm:
                ntq_store_dest(c, &instr->dest, 0,
                               qir_uniform(c, QUNIFORM_BLEND_CONST_COLOR_AAAA,
                                           0));
                break;

        case nir_intrinsic_load_alpha_ref_float:
                ntq_store_dest(c, &instr->dest, 0,
                               qir_uniform(c, QUNIFORM_ALPHA_REF, 0));
                break;

        case nir_intrinsic_load_sample_mask_in:
                ntq_store_dest(c, &instr->dest, 0,
                               qir_uniform(c, QUNIFORM_SAMPLE_MASK, 0));
                break;

        case nir_intrinsic_load_front_face:
                /* The register contains 0 (front) or 1 (back), and we need to
                 * turn it into a NIR bool where true means front.
                 */
                ntq_store_dest(c, &instr->dest, 0,
                               qir_ADD(c,
                                       qir_uniform_ui(c, -1),
                                       qir_reg(QFILE_FRAG_REV_FLAG, 0)));
                break;

        case nir_intrinsic_load_input:
                ntq_emit_load_input(c, instr);
                break;

        case nir_intrinsic_store_output:
                const_offset = nir_src_as_const_value(instr->src[1]);
                assert(const_offset && "vc4 doesn't support indirect outputs");
                offset = nir_intrinsic_base(instr) + const_offset->u32[0];

                /* MSAA color outputs are the only case where we have an
                 * output that's not lowered to being a store of a single 32
                 * bit value.
                 */
                if (c->stage == QSTAGE_FRAG && instr->num_components == 4) {
                        assert(offset == c->output_color_index);
                        for (int i = 0; i < 4; i++) {
                                c->sample_colors[i] =
                                        qir_MOV(c, ntq_get_src(c, instr->src[0],
                                                               i));
                        }
                } else {
                        offset = offset * 4 + nir_intrinsic_component(instr);
                        assert(instr->num_components == 1);
                        c->outputs[offset] =
                                qir_MOV(c, ntq_get_src(c, instr->src[0], 0));
                        c->num_outputs = MAX2(c->num_outputs, offset + 1);
                }
                break;

        case nir_intrinsic_discard:
                if (c->execute.file != QFILE_NULL) {
                        qir_SF(c, c->execute);
                        qir_MOV_cond(c, QPU_COND_ZS, c->discard,
                                     qir_uniform_ui(c, ~0));
                } else {
                        qir_MOV_dest(c, c->discard, qir_uniform_ui(c, ~0));
                }
                break;

        case nir_intrinsic_discard_if: {
                /* true (~0) if we're discarding */
                struct qreg cond = ntq_get_src(c, instr->src[0], 0);

                if (c->execute.file != QFILE_NULL) {
                        /* execute == 0 means the channel is active.  Invert
                         * the condition so that we can use zero as "executing
                         * and discarding."
                         */
                        qir_SF(c, qir_AND(c, c->execute, qir_NOT(c, cond)));
                        qir_MOV_cond(c, QPU_COND_ZS, c->discard, cond);
                } else {
                        qir_OR_dest(c, c->discard, c->discard,
                                    ntq_get_src(c, instr->src[0], 0));
                }

                break;
        }

        default:
                fprintf(stderr, "Unknown intrinsic: ");
                nir_print_instr(&instr->instr, stderr);
                fprintf(stderr, "\n");
                break;
        }
}

/* Clears (activates) the execute flags for any channels whose jump target
 * matches this block.
 */
static void
ntq_activate_execute_for_block(struct vc4_compile *c)
{
        qir_SF(c, qir_SUB(c,
                          c->execute,
                          qir_uniform_ui(c, c->cur_block->index)));
        qir_MOV_cond(c, QPU_COND_ZS, c->execute, qir_uniform_ui(c, 0));
}

static void
ntq_emit_if(struct vc4_compile *c, nir_if *if_stmt)
{
        if (!c->vc4->screen->has_control_flow) {
                fprintf(stderr,
                        "IF statement support requires updated kernel.\n");
                return;
        }

        nir_block *nir_else_block = nir_if_first_else_block(if_stmt);
        bool empty_else_block =
                (nir_else_block == nir_if_last_else_block(if_stmt) &&
                 exec_list_is_empty(&nir_else_block->instr_list));

        struct qblock *then_block = qir_new_block(c);
        struct qblock *after_block = qir_new_block(c);
        struct qblock *else_block;
        if (empty_else_block)
                else_block = after_block;
        else
                else_block = qir_new_block(c);

        bool was_top_level = false;
        if (c->execute.file == QFILE_NULL) {
                c->execute = qir_MOV(c, qir_uniform_ui(c, 0));
                was_top_level = true;
        }

        /* Set ZS for executing (execute == 0) and jumping (if->condition ==
         * 0) channels, and then update execute flags for those to point to
         * the ELSE block.
         */
        qir_SF(c, qir_OR(c,
                         c->execute,
                         ntq_get_src(c, if_stmt->condition, 0)));
        qir_MOV_cond(c, QPU_COND_ZS, c->execute,
                     qir_uniform_ui(c, else_block->index));

        /* Jump to ELSE if nothing is active for THEN, otherwise fall
         * through.
         */
        qir_SF(c, c->execute);
        qir_BRANCH(c, QPU_COND_BRANCH_ALL_ZC);
        qir_link_blocks(c->cur_block, else_block);
        qir_link_blocks(c->cur_block, then_block);

        /* Process the THEN block. */
        qir_set_emit_block(c, then_block);
        ntq_emit_cf_list(c, &if_stmt->then_list);

        if (!empty_else_block) {
                /* Handle the end of the THEN block.  First, all currently
                 * active channels update their execute flags to point to
                 * ENDIF
                 */
                qir_SF(c, c->execute);
                qir_MOV_cond(c, QPU_COND_ZS, c->execute,
                             qir_uniform_ui(c, after_block->index));

                /* If everything points at ENDIF, then jump there immediately. */
                qir_SF(c, qir_SUB(c, c->execute, qir_uniform_ui(c, after_block->index)));
                qir_BRANCH(c, QPU_COND_BRANCH_ALL_ZS);
                qir_link_blocks(c->cur_block, after_block);
                qir_link_blocks(c->cur_block, else_block);

                qir_set_emit_block(c, else_block);
                ntq_activate_execute_for_block(c);
                ntq_emit_cf_list(c, &if_stmt->else_list);
        }

        qir_link_blocks(c->cur_block, after_block);

        qir_set_emit_block(c, after_block);
        if (was_top_level) {
                c->execute = c->undef;
                c->last_top_block = c->cur_block;
        } else {
                ntq_activate_execute_for_block(c);
        }
}

static void
ntq_emit_jump(struct vc4_compile *c, nir_jump_instr *jump)
{
        struct qblock *jump_block;
        switch (jump->type) {
        case nir_jump_break:
                jump_block = c->loop_break_block;
                break;
        case nir_jump_continue:
                jump_block = c->loop_cont_block;
                break;
        default:
                unreachable("Unsupported jump type\n");
        }

        qir_SF(c, c->execute);
        qir_MOV_cond(c, QPU_COND_ZS, c->execute,
                     qir_uniform_ui(c, jump_block->index));

        /* Jump to the destination block if everyone has taken the jump. */
        qir_SF(c, qir_SUB(c, c->execute, qir_uniform_ui(c, jump_block->index)));
        qir_BRANCH(c, QPU_COND_BRANCH_ALL_ZS);
        struct qblock *new_block = qir_new_block(c);
        qir_link_blocks(c->cur_block, jump_block);
        qir_link_blocks(c->cur_block, new_block);
        qir_set_emit_block(c, new_block);
}

static void
ntq_emit_instr(struct vc4_compile *c, nir_instr *instr)
{
        switch (instr->type) {
        case nir_instr_type_alu:
                ntq_emit_alu(c, nir_instr_as_alu(instr));
                break;

        case nir_instr_type_intrinsic:
                ntq_emit_intrinsic(c, nir_instr_as_intrinsic(instr));
                break;

        case nir_instr_type_load_const:
                ntq_emit_load_const(c, nir_instr_as_load_const(instr));
                break;

        case nir_instr_type_ssa_undef:
                ntq_emit_ssa_undef(c, nir_instr_as_ssa_undef(instr));
                break;

        case nir_instr_type_tex:
                ntq_emit_tex(c, nir_instr_as_tex(instr));
                break;

        case nir_instr_type_jump:
                ntq_emit_jump(c, nir_instr_as_jump(instr));
                break;

        default:
                fprintf(stderr, "Unknown NIR instr type: ");
                nir_print_instr(instr, stderr);
                fprintf(stderr, "\n");
                abort();
        }
}

static void
ntq_emit_block(struct vc4_compile *c, nir_block *block)
{
        nir_foreach_instr(instr, block) {
                ntq_emit_instr(c, instr);
        }
}

static void ntq_emit_cf_list(struct vc4_compile *c, struct exec_list *list);

static void
ntq_emit_loop(struct vc4_compile *c, nir_loop *loop)
{
        if (!c->vc4->screen->has_control_flow) {
                fprintf(stderr,
                        "loop support requires updated kernel.\n");
                ntq_emit_cf_list(c, &loop->body);
                return;
        }

        bool was_top_level = false;
        if (c->execute.file == QFILE_NULL) {
                c->execute = qir_MOV(c, qir_uniform_ui(c, 0));
                was_top_level = true;
        }

        struct qblock *save_loop_cont_block = c->loop_cont_block;
        struct qblock *save_loop_break_block = c->loop_break_block;

        c->loop_cont_block = qir_new_block(c);
        c->loop_break_block = qir_new_block(c);

        qir_link_blocks(c->cur_block, c->loop_cont_block);
        qir_set_emit_block(c, c->loop_cont_block);
        ntq_activate_execute_for_block(c);

        ntq_emit_cf_list(c, &loop->body);

        /* If anything had explicitly continued, or is here at the end of the
         * loop, then we need to loop again.  SF updates are masked by the
         * instruction's condition, so we can do the OR of the two conditions
         * within SF.
         */
        qir_SF(c, c->execute);
        struct qinst *cont_check =
                qir_SUB_dest(c,
                             c->undef,
                             c->execute,
                             qir_uniform_ui(c, c->loop_cont_block->index));
        cont_check->cond = QPU_COND_ZC;
        cont_check->sf = true;

        qir_BRANCH(c, QPU_COND_BRANCH_ANY_ZS);
        qir_link_blocks(c->cur_block, c->loop_cont_block);
        qir_link_blocks(c->cur_block, c->loop_break_block);

        qir_set_emit_block(c, c->loop_break_block);
        if (was_top_level) {
                c->execute = c->undef;
                c->last_top_block = c->cur_block;
        } else {
                ntq_activate_execute_for_block(c);
        }

        c->loop_break_block = save_loop_break_block;
        c->loop_cont_block = save_loop_cont_block;
}

static void
ntq_emit_function(struct vc4_compile *c, nir_function_impl *func)
{
        fprintf(stderr, "FUNCTIONS not handled.\n");
        abort();
}

static void
ntq_emit_cf_list(struct vc4_compile *c, struct exec_list *list)
{
        foreach_list_typed(nir_cf_node, node, node, list) {
                switch (node->type) {
                case nir_cf_node_block:
                        ntq_emit_block(c, nir_cf_node_as_block(node));
                        break;

                case nir_cf_node_if:
                        ntq_emit_if(c, nir_cf_node_as_if(node));
                        break;

                case nir_cf_node_loop:
                        ntq_emit_loop(c, nir_cf_node_as_loop(node));
                        break;

                case nir_cf_node_function:
                        ntq_emit_function(c, nir_cf_node_as_function(node));
                        break;

                default:
                        fprintf(stderr, "Unknown NIR node type\n");
                        abort();
                }
        }
}

static void
ntq_emit_impl(struct vc4_compile *c, nir_function_impl *impl)
{
        ntq_setup_registers(c, &impl->registers);
        ntq_emit_cf_list(c, &impl->body);
}

static void
nir_to_qir(struct vc4_compile *c)
{
        if (c->stage == QSTAGE_FRAG && c->s->info.fs.uses_discard)
                c->discard = qir_MOV(c, qir_uniform_ui(c, 0));

        ntq_setup_inputs(c);
        ntq_setup_outputs(c);
        ntq_setup_uniforms(c);
        ntq_setup_registers(c, &c->s->registers);

        /* Find the main function and emit the body. */
        nir_foreach_function(function, c->s) {
                assert(strcmp(function->name, "main") == 0);
                assert(function->impl);
                ntq_emit_impl(c, function->impl);
        }
}

static const nir_shader_compiler_options nir_options = {
        .lower_extract_byte = true,
        .lower_extract_word = true,
        .lower_ffma = true,
        .lower_flrp32 = true,
        .lower_fpow = true,
        .lower_fsat = true,
        .lower_fsqrt = true,
        .lower_negate = true,
        .native_integers = true,
        .max_unroll_iterations = 32,
};

const void *
vc4_screen_get_compiler_options(struct pipe_screen *pscreen,
                                enum pipe_shader_ir ir,
                                enum pipe_shader_type shader)
{
        return &nir_options;
}

static int
count_nir_instrs(nir_shader *nir)
{
        int count = 0;
        nir_foreach_function(function, nir) {
                if (!function->impl)
                        continue;
                nir_foreach_block(block, function->impl) {
                        nir_foreach_instr(instr, block)
                                count++;
                }
        }
        return count;
}

static struct vc4_compile *
vc4_shader_ntq(struct vc4_context *vc4, enum qstage stage,
               struct vc4_key *key, bool fs_threaded)
{
        struct vc4_compile *c = qir_compile_init();

        c->vc4 = vc4;
        c->stage = stage;
        c->shader_state = &key->shader_state->base;
        c->program_id = key->shader_state->program_id;
        c->variant_id =
                p_atomic_inc_return(&key->shader_state->compiled_variant_count);
        c->fs_threaded = fs_threaded;

        c->key = key;
        switch (stage) {
        case QSTAGE_FRAG:
                c->fs_key = (struct vc4_fs_key *)key;
                if (c->fs_key->is_points) {
                        c->point_x = emit_fragment_varying(c, ~0, 0);
                        c->point_y = emit_fragment_varying(c, ~0, 0);
                } else if (c->fs_key->is_lines) {
                        c->line_x = emit_fragment_varying(c, ~0, 0);
                }
                break;
        case QSTAGE_VERT:
                c->vs_key = (struct vc4_vs_key *)key;
                break;
        case QSTAGE_COORD:
                c->vs_key = (struct vc4_vs_key *)key;
                break;
        }

        c->s = nir_shader_clone(c, key->shader_state->base.ir.nir);

        if (stage == QSTAGE_FRAG) {
                if (c->fs_key->alpha_test_func != COMPARE_FUNC_ALWAYS) {
                        NIR_PASS_V(c->s, nir_lower_alpha_test,
                                   c->fs_key->alpha_test_func,
                                   c->fs_key->sample_alpha_to_one &&
                                   c->fs_key->msaa);
                }
                NIR_PASS_V(c->s, vc4_nir_lower_blend, c);
        }

        struct nir_lower_tex_options tex_options = {
                /* We would need to implement txs, but we don't want the
                 * int/float conversions
                 */
                .lower_rect = false,

                .lower_txp = ~0,

                /* Apply swizzles to all samplers. */
                .swizzle_result = ~0,
        };

        /* Lower the format swizzle and ARB_texture_swizzle-style swizzle.
         * The format swizzling applies before sRGB decode, and
         * ARB_texture_swizzle is the last thing before returning the sample.
         */
        for (int i = 0; i < ARRAY_SIZE(key->tex); i++) {
                enum pipe_format format = c->key->tex[i].format;

                if (!format)
                        continue;

                const uint8_t *format_swizzle = vc4_get_format_swizzle(format);

                for (int j = 0; j < 4; j++) {
                        uint8_t arb_swiz = c->key->tex[i].swizzle[j];

                        if (arb_swiz <= 3) {
                                tex_options.swizzles[i][j] =
                                        format_swizzle[arb_swiz];
                        } else {
                                tex_options.swizzles[i][j] = arb_swiz;
                        }
                }

                if (util_format_is_srgb(format))
                        tex_options.lower_srgb |= (1 << i);
        }

        NIR_PASS_V(c->s, nir_lower_tex, &tex_options);

        if (c->fs_key && c->fs_key->light_twoside)
                NIR_PASS_V(c->s, nir_lower_two_sided_color);

        if (c->vs_key && c->vs_key->clamp_color)
                NIR_PASS_V(c->s, nir_lower_clamp_color_outputs);

        if (c->key->ucp_enables) {
                if (stage == QSTAGE_FRAG) {
                        NIR_PASS_V(c->s, nir_lower_clip_fs, c->key->ucp_enables);
                } else {
                        NIR_PASS_V(c->s, nir_lower_clip_vs, c->key->ucp_enables);
                        NIR_PASS_V(c->s, nir_lower_io_to_scalar,
                                   nir_var_shader_out);
                }
        }

        /* FS input scalarizing must happen after nir_lower_two_sided_color,
         * which only handles a vec4 at a time.  Similarly, VS output
         * scalarizing must happen after nir_lower_clip_vs.
         */
        if (c->stage == QSTAGE_FRAG)
                NIR_PASS_V(c->s, nir_lower_io_to_scalar, nir_var_shader_in);
        else
                NIR_PASS_V(c->s, nir_lower_io_to_scalar, nir_var_shader_out);

        NIR_PASS_V(c->s, vc4_nir_lower_io, c);
        NIR_PASS_V(c->s, vc4_nir_lower_txf_ms, c);
        NIR_PASS_V(c->s, nir_lower_idiv);

        vc4_optimize_nir(c->s);

        NIR_PASS_V(c->s, nir_convert_from_ssa, true);

        if (vc4_debug & VC4_DEBUG_SHADERDB) {
                fprintf(stderr, "SHADER-DB: %s prog %d/%d: %d NIR instructions\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id,
                        count_nir_instrs(c->s));
        }

        if (vc4_debug & VC4_DEBUG_NIR) {
                fprintf(stderr, "%s prog %d/%d NIR:\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id);
                nir_print_shader(c->s, stderr);
        }

        nir_to_qir(c);

        switch (stage) {
        case QSTAGE_FRAG:
                /* FS threading requires that the thread execute
                 * QPU_SIG_LAST_THREAD_SWITCH exactly once before terminating
                 * (with no other THRSW afterwards, obviously).  If we didn't
                 * fetch a texture at a top level block, this wouldn't be
                 * true.
                 */
                if (c->fs_threaded && !c->last_thrsw_at_top_level) {
                        c->failed = true;
                        return c;
                }

                emit_frag_end(c);
                break;
        case QSTAGE_VERT:
                emit_vert_end(c,
                              c->vs_key->fs_inputs->input_slots,
                              c->vs_key->fs_inputs->num_inputs);
                break;
        case QSTAGE_COORD:
                emit_coord_end(c);
                break;
        }

        if (vc4_debug & VC4_DEBUG_QIR) {
                fprintf(stderr, "%s prog %d/%d pre-opt QIR:\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id);
                qir_dump(c);
                fprintf(stderr, "\n");
        }

        qir_optimize(c);
        qir_lower_uniforms(c);

        qir_schedule_instructions(c);
        qir_emit_uniform_stream_resets(c);

        if (vc4_debug & VC4_DEBUG_QIR) {
                fprintf(stderr, "%s prog %d/%d QIR:\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id);
                qir_dump(c);
                fprintf(stderr, "\n");
        }

        qir_reorder_uniforms(c);
        vc4_generate_code(vc4, c);

        if (vc4_debug & VC4_DEBUG_SHADERDB) {
                fprintf(stderr, "SHADER-DB: %s prog %d/%d: %d instructions\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id,
                        c->qpu_inst_count);
                fprintf(stderr, "SHADER-DB: %s prog %d/%d: %d uniforms\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id,
                        c->num_uniforms);
        }

        ralloc_free(c->s);

        return c;
}

static void *
vc4_shader_state_create(struct pipe_context *pctx,
                        const struct pipe_shader_state *cso)
{
        struct vc4_context *vc4 = vc4_context(pctx);
        struct vc4_uncompiled_shader *so = CALLOC_STRUCT(vc4_uncompiled_shader);
        if (!so)
                return NULL;

        so->program_id = vc4->next_uncompiled_program_id++;

        nir_shader *s;

        if (cso->type == PIPE_SHADER_IR_NIR) {
                /* The backend takes ownership of the NIR shader on state
                 * creation.
                 */
                s = cso->ir.nir;

                NIR_PASS_V(s, nir_lower_io, nir_var_all, type_size,
                           (nir_lower_io_options)0);
        } else {
                assert(cso->type == PIPE_SHADER_IR_TGSI);

                if (vc4_debug & VC4_DEBUG_TGSI) {
                        fprintf(stderr, "prog %d TGSI:\n",
                                so->program_id);
                        tgsi_dump(cso->tokens, 0);
                        fprintf(stderr, "\n");
                }
                s = tgsi_to_nir(cso->tokens, &nir_options);
        }

        NIR_PASS_V(s, nir_opt_global_to_local);
        NIR_PASS_V(s, nir_lower_regs_to_ssa);
        NIR_PASS_V(s, nir_normalize_cubemap_coords);

        NIR_PASS_V(s, nir_lower_load_const_to_scalar);

        vc4_optimize_nir(s);

        NIR_PASS_V(s, nir_remove_dead_variables, nir_var_local);

        /* Garbage collect dead instructions */
        nir_sweep(s);

        so->base.type = PIPE_SHADER_IR_NIR;
        so->base.ir.nir = s;

        if (vc4_debug & VC4_DEBUG_NIR) {
                fprintf(stderr, "%s prog %d NIR:\n",
                        gl_shader_stage_name(s->info.stage),
                        so->program_id);
                nir_print_shader(s, stderr);
                fprintf(stderr, "\n");
        }

        return so;
}

static void
copy_uniform_state_to_shader(struct vc4_compiled_shader *shader,
                             struct vc4_compile *c)
{
        int count = c->num_uniforms;
        struct vc4_shader_uniform_info *uinfo = &shader->uniforms;

        uinfo->count = count;
        uinfo->data = ralloc_array(shader, uint32_t, count);
        memcpy(uinfo->data, c->uniform_data,
               count * sizeof(*uinfo->data));
        uinfo->contents = ralloc_array(shader, enum quniform_contents, count);
        memcpy(uinfo->contents, c->uniform_contents,
               count * sizeof(*uinfo->contents));
        uinfo->num_texture_samples = c->num_texture_samples;

        vc4_set_shader_uniform_dirty_flags(shader);
}

static void
vc4_setup_compiled_fs_inputs(struct vc4_context *vc4, struct vc4_compile *c,
                             struct vc4_compiled_shader *shader)
{
        struct vc4_fs_inputs inputs;

        memset(&inputs, 0, sizeof(inputs));
        inputs.input_slots = ralloc_array(shader,
                                          struct vc4_varying_slot,
                                          c->num_input_slots);

        bool input_live[c->num_input_slots];

        memset(input_live, 0, sizeof(input_live));
        qir_for_each_inst_inorder(inst, c) {
                for (int i = 0; i < qir_get_nsrc(inst); i++) {
                        if (inst->src[i].file == QFILE_VARY)
                                input_live[inst->src[i].index] = true;
                }
        }

        for (int i = 0; i < c->num_input_slots; i++) {
                struct vc4_varying_slot *slot = &c->input_slots[i];

                if (!input_live[i])
                        continue;

                /* Skip non-VS-output inputs. */
                if (slot->slot == (uint8_t)~0)
                        continue;

                if (slot->slot == VARYING_SLOT_COL0 ||
                    slot->slot == VARYING_SLOT_COL1 ||
                    slot->slot == VARYING_SLOT_BFC0 ||
                    slot->slot == VARYING_SLOT_BFC1) {
                        shader->color_inputs |= (1 << inputs.num_inputs);
                }

                inputs.input_slots[inputs.num_inputs] = *slot;
                inputs.num_inputs++;
        }
        shader->num_inputs = inputs.num_inputs;

        /* Add our set of inputs to the set of all inputs seen.  This way, we
         * can have a single pointer that identifies an FS inputs set,
         * allowing VS to avoid recompiling when the FS is recompiled (or a
         * new one is bound using separate shader objects) but the inputs
         * don't change.
         */
        struct set_entry *entry = _mesa_set_search(vc4->fs_inputs_set, &inputs);
        if (entry) {
                shader->fs_inputs = entry->key;
                ralloc_free(inputs.input_slots);
        } else {
                struct vc4_fs_inputs *alloc_inputs;

                alloc_inputs = rzalloc(vc4->fs_inputs_set, struct vc4_fs_inputs);
                memcpy(alloc_inputs, &inputs, sizeof(inputs));
                ralloc_steal(alloc_inputs, inputs.input_slots);
                _mesa_set_add(vc4->fs_inputs_set, alloc_inputs);

                shader->fs_inputs = alloc_inputs;
        }
}

static struct vc4_compiled_shader *
vc4_get_compiled_shader(struct vc4_context *vc4, enum qstage stage,
                        struct vc4_key *key)
{
        struct hash_table *ht;
        uint32_t key_size;
        bool try_threading;

        if (stage == QSTAGE_FRAG) {
                ht = vc4->fs_cache;
                key_size = sizeof(struct vc4_fs_key);
                try_threading = vc4->screen->has_threaded_fs;
        } else {
                ht = vc4->vs_cache;
                key_size = sizeof(struct vc4_vs_key);
                try_threading = false;
        }

        struct vc4_compiled_shader *shader;
        struct hash_entry *entry = _mesa_hash_table_search(ht, key);
        if (entry)
                return entry->data;

        struct vc4_compile *c = vc4_shader_ntq(vc4, stage, key, try_threading);
        /* If the FS failed to compile threaded, fall back to single threaded. */
        if (try_threading && c->failed) {
                qir_compile_destroy(c);
                c = vc4_shader_ntq(vc4, stage, key, false);
        }

        shader = rzalloc(NULL, struct vc4_compiled_shader);

        shader->program_id = vc4->next_compiled_program_id++;
        if (stage == QSTAGE_FRAG) {
                vc4_setup_compiled_fs_inputs(vc4, c, shader);

                /* Note: the temporary clone in c->s has been freed. */
                nir_shader *orig_shader = key->shader_state->base.ir.nir;
                if (orig_shader->info.outputs_written & (1 << FRAG_RESULT_DEPTH))
                        shader->disable_early_z = true;
        } else {
                shader->num_inputs = c->num_inputs;

                shader->vattr_offsets[0] = 0;
                for (int i = 0; i < 8; i++) {
                        shader->vattr_offsets[i + 1] =
                                shader->vattr_offsets[i] + c->vattr_sizes[i];

                        if (c->vattr_sizes[i])
                                shader->vattrs_live |= (1 << i);
                }
        }

        shader->failed = c->failed;
        if (c->failed) {
                shader->failed = true;
        } else {
                copy_uniform_state_to_shader(shader, c);
                shader->bo = vc4_bo_alloc_shader(vc4->screen, c->qpu_insts,
                                                 c->qpu_inst_count *
                                                 sizeof(uint64_t));
        }

        shader->fs_threaded = c->fs_threaded;

        /* Copy the compiler UBO range state to the compiled shader, dropping
         * out arrays that were never referenced by an indirect load.
         *
         * (Note that QIR dead code elimination of an array access still
         * leaves that array alive, though)
         */
        if (c->num_ubo_ranges) {
                shader->num_ubo_ranges = c->num_ubo_ranges;
                shader->ubo_ranges = ralloc_array(shader, struct vc4_ubo_range,
                                                  c->num_ubo_ranges);
                uint32_t j = 0;
                for (int i = 0; i < c->num_uniform_ranges; i++) {
                        struct vc4_compiler_ubo_range *range =
                                &c->ubo_ranges[i];
                        if (!range->used)
                                continue;

                        shader->ubo_ranges[j].dst_offset = range->dst_offset;
                        shader->ubo_ranges[j].src_offset = range->src_offset;
                        shader->ubo_ranges[j].size = range->size;
                        shader->ubo_size += c->ubo_ranges[i].size;
                        j++;
                }
        }
        if (shader->ubo_size) {
                if (vc4_debug & VC4_DEBUG_SHADERDB) {
                        fprintf(stderr, "SHADER-DB: %s prog %d/%d: %d UBO uniforms\n",
                                qir_get_stage_name(c->stage),
                                c->program_id, c->variant_id,
                                shader->ubo_size / 4);
                }
        }

        if ((vc4_debug & VC4_DEBUG_SHADERDB) && stage == QSTAGE_FRAG) {
                fprintf(stderr, "SHADER-DB: %s prog %d/%d: %d FS threads\n",
                        qir_get_stage_name(c->stage),
                        c->program_id, c->variant_id,
                        1 + shader->fs_threaded);
        }

        qir_compile_destroy(c);

        struct vc4_key *dup_key;
        dup_key = rzalloc_size(shader, key_size); /* TODO: don't use rzalloc */
        memcpy(dup_key, key, key_size);
        _mesa_hash_table_insert(ht, dup_key, shader);

        return shader;
}

static void
vc4_setup_shared_key(struct vc4_context *vc4, struct vc4_key *key,
                     struct vc4_texture_stateobj *texstate)
{
        for (int i = 0; i < texstate->num_textures; i++) {
                struct pipe_sampler_view *sampler = texstate->textures[i];
                struct vc4_sampler_view *vc4_sampler = vc4_sampler_view(sampler);
                struct pipe_sampler_state *sampler_state =
                        texstate->samplers[i];

                if (!sampler)
                        continue;

                key->tex[i].format = sampler->format;
                key->tex[i].swizzle[0] = sampler->swizzle_r;
                key->tex[i].swizzle[1] = sampler->swizzle_g;
                key->tex[i].swizzle[2] = sampler->swizzle_b;
                key->tex[i].swizzle[3] = sampler->swizzle_a;

                if (sampler->texture->nr_samples > 1) {
                        key->tex[i].msaa_width = sampler->texture->width0;
                        key->tex[i].msaa_height = sampler->texture->height0;
                } else if (sampler){
                        key->tex[i].compare_mode = sampler_state->compare_mode;
                        key->tex[i].compare_func = sampler_state->compare_func;
                        key->tex[i].wrap_s = sampler_state->wrap_s;
                        key->tex[i].wrap_t = sampler_state->wrap_t;
                        key->tex[i].force_first_level =
                                vc4_sampler->force_first_level;
                }
        }

        key->ucp_enables = vc4->rasterizer->base.clip_plane_enable;
}

static void
vc4_update_compiled_fs(struct vc4_context *vc4, uint8_t prim_mode)
{
        struct vc4_job *job = vc4->job;
        struct vc4_fs_key local_key;
        struct vc4_fs_key *key = &local_key;

        if (!(vc4->dirty & (VC4_DIRTY_PRIM_MODE |
                            VC4_DIRTY_BLEND |
                            VC4_DIRTY_FRAMEBUFFER |
                            VC4_DIRTY_ZSA |
                            VC4_DIRTY_RASTERIZER |
                            VC4_DIRTY_SAMPLE_MASK |
                            VC4_DIRTY_FRAGTEX |
                            VC4_DIRTY_UNCOMPILED_FS))) {
                return;
        }

        memset(key, 0, sizeof(*key));
        vc4_setup_shared_key(vc4, &key->base, &vc4->fragtex);
        key->base.shader_state = vc4->prog.bind_fs;
        key->is_points = (prim_mode == PIPE_PRIM_POINTS);
        key->is_lines = (prim_mode >= PIPE_PRIM_LINES &&
                         prim_mode <= PIPE_PRIM_LINE_STRIP);
        key->blend = vc4->blend->rt[0];
        if (vc4->blend->logicop_enable) {
                key->logicop_func = vc4->blend->logicop_func;
        } else {
                key->logicop_func = PIPE_LOGICOP_COPY;
        }
        if (job->msaa) {
                key->msaa = vc4->rasterizer->base.multisample;
                key->sample_coverage = (vc4->sample_mask != (1 << VC4_MAX_SAMPLES) - 1);
                key->sample_alpha_to_coverage = vc4->blend->alpha_to_coverage;
                key->sample_alpha_to_one = vc4->blend->alpha_to_one;
        }

        if (vc4->framebuffer.cbufs[0])
                key->color_format = vc4->framebuffer.cbufs[0]->format;

        key->stencil_enabled = vc4->zsa->stencil_uniforms[0] != 0;
        key->stencil_twoside = vc4->zsa->stencil_uniforms[1] != 0;
        key->stencil_full_writemasks = vc4->zsa->stencil_uniforms[2] != 0;
        key->depth_enabled = (vc4->zsa->base.depth.enabled ||
                              key->stencil_enabled);
        if (vc4->zsa->base.alpha.enabled)
                key->alpha_test_func = vc4->zsa->base.alpha.func;
        else
                key->alpha_test_func = COMPARE_FUNC_ALWAYS;

        if (key->is_points) {
                key->point_sprite_mask =
                        vc4->rasterizer->base.sprite_coord_enable;
                key->point_coord_upper_left =
                        (vc4->rasterizer->base.sprite_coord_mode ==
                         PIPE_SPRITE_COORD_UPPER_LEFT);
        }

        key->light_twoside = vc4->rasterizer->base.light_twoside;

        struct vc4_compiled_shader *old_fs = vc4->prog.fs;
        vc4->prog.fs = vc4_get_compiled_shader(vc4, QSTAGE_FRAG, &key->base);
        if (vc4->prog.fs == old_fs)
                return;

        vc4->dirty |= VC4_DIRTY_COMPILED_FS;

        if (vc4->rasterizer->base.flatshade &&
            (!old_fs || vc4->prog.fs->color_inputs != old_fs->color_inputs)) {
                vc4->dirty |= VC4_DIRTY_FLAT_SHADE_FLAGS;
        }

        if (!old_fs || vc4->prog.fs->fs_inputs != old_fs->fs_inputs)
                vc4->dirty |= VC4_DIRTY_FS_INPUTS;
}

static void
vc4_update_compiled_vs(struct vc4_context *vc4, uint8_t prim_mode)
{
        struct vc4_vs_key local_key;
        struct vc4_vs_key *key = &local_key;

        if (!(vc4->dirty & (VC4_DIRTY_PRIM_MODE |
                            VC4_DIRTY_RASTERIZER |
                            VC4_DIRTY_VERTTEX |
                            VC4_DIRTY_VTXSTATE |
                            VC4_DIRTY_UNCOMPILED_VS |
                            VC4_DIRTY_FS_INPUTS))) {
                return;
        }

        memset(key, 0, sizeof(*key));
        vc4_setup_shared_key(vc4, &key->base, &vc4->verttex);
        key->base.shader_state = vc4->prog.bind_vs;
        key->fs_inputs = vc4->prog.fs->fs_inputs;
        key->clamp_color = vc4->rasterizer->base.clamp_vertex_color;

        for (int i = 0; i < ARRAY_SIZE(key->attr_formats); i++)
                key->attr_formats[i] = vc4->vtx->pipe[i].src_format;

        key->per_vertex_point_size =
                (prim_mode == PIPE_PRIM_POINTS &&
                 vc4->rasterizer->base.point_size_per_vertex);

        struct vc4_compiled_shader *vs =
                vc4_get_compiled_shader(vc4, QSTAGE_VERT, &key->base);
        if (vs != vc4->prog.vs) {
                vc4->prog.vs = vs;
                vc4->dirty |= VC4_DIRTY_COMPILED_VS;
        }

        key->is_coord = true;
        /* Coord shaders don't care what the FS inputs are. */
        key->fs_inputs = NULL;
        struct vc4_compiled_shader *cs =
                vc4_get_compiled_shader(vc4, QSTAGE_COORD, &key->base);
        if (cs != vc4->prog.cs) {
                vc4->prog.cs = cs;
                vc4->dirty |= VC4_DIRTY_COMPILED_CS;
        }
}

bool
vc4_update_compiled_shaders(struct vc4_context *vc4, uint8_t prim_mode)
{
        vc4_update_compiled_fs(vc4, prim_mode);
        vc4_update_compiled_vs(vc4, prim_mode);

        return !(vc4->prog.cs->failed ||
                 vc4->prog.vs->failed ||
                 vc4->prog.fs->failed);
}

static uint32_t
fs_cache_hash(const void *key)
{
        return _mesa_hash_data(key, sizeof(struct vc4_fs_key));
}

static uint32_t
vs_cache_hash(const void *key)
{
        return _mesa_hash_data(key, sizeof(struct vc4_vs_key));
}

static bool
fs_cache_compare(const void *key1, const void *key2)
{
        return memcmp(key1, key2, sizeof(struct vc4_fs_key)) == 0;
}

static bool
vs_cache_compare(const void *key1, const void *key2)
{
        return memcmp(key1, key2, sizeof(struct vc4_vs_key)) == 0;
}

static uint32_t
fs_inputs_hash(const void *key)
{
        const struct vc4_fs_inputs *inputs = key;

        return _mesa_hash_data(inputs->input_slots,
                               sizeof(*inputs->input_slots) *
                               inputs->num_inputs);
}

static bool
fs_inputs_compare(const void *key1, const void *key2)
{
        const struct vc4_fs_inputs *inputs1 = key1;
        const struct vc4_fs_inputs *inputs2 = key2;

        return (inputs1->num_inputs == inputs2->num_inputs &&
                memcmp(inputs1->input_slots,
                       inputs2->input_slots,
                       sizeof(*inputs1->input_slots) *
                       inputs1->num_inputs) == 0);
}

static void
delete_from_cache_if_matches(struct hash_table *ht,
                             struct vc4_compiled_shader **last_compile,
                             struct hash_entry *entry,
                             struct vc4_uncompiled_shader *so)
{
        const struct vc4_key *key = entry->key;

        if (key->shader_state == so) {
                struct vc4_compiled_shader *shader = entry->data;
                _mesa_hash_table_remove(ht, entry);
                vc4_bo_unreference(&shader->bo);

                if (shader == *last_compile)
                        *last_compile = NULL;

                ralloc_free(shader);
        }
}

static void
vc4_shader_state_delete(struct pipe_context *pctx, void *hwcso)
{
        struct vc4_context *vc4 = vc4_context(pctx);
        struct vc4_uncompiled_shader *so = hwcso;

        struct hash_entry *entry;
        hash_table_foreach(vc4->fs_cache, entry) {
                delete_from_cache_if_matches(vc4->fs_cache, &vc4->prog.fs,
                                             entry, so);
        }
        hash_table_foreach(vc4->vs_cache, entry) {
                delete_from_cache_if_matches(vc4->vs_cache, &vc4->prog.vs,
                                             entry, so);
        }

        ralloc_free(so->base.ir.nir);
        free(so);
}

static void
vc4_fp_state_bind(struct pipe_context *pctx, void *hwcso)
{
        struct vc4_context *vc4 = vc4_context(pctx);
        vc4->prog.bind_fs = hwcso;
        vc4->dirty |= VC4_DIRTY_UNCOMPILED_FS;
}

static void
vc4_vp_state_bind(struct pipe_context *pctx, void *hwcso)
{
        struct vc4_context *vc4 = vc4_context(pctx);
        vc4->prog.bind_vs = hwcso;
        vc4->dirty |= VC4_DIRTY_UNCOMPILED_VS;
}

void
vc4_program_init(struct pipe_context *pctx)
{
        struct vc4_context *vc4 = vc4_context(pctx);

        pctx->create_vs_state = vc4_shader_state_create;
        pctx->delete_vs_state = vc4_shader_state_delete;

        pctx->create_fs_state = vc4_shader_state_create;
        pctx->delete_fs_state = vc4_shader_state_delete;

        pctx->bind_fs_state = vc4_fp_state_bind;
        pctx->bind_vs_state = vc4_vp_state_bind;

        vc4->fs_cache = _mesa_hash_table_create(pctx, fs_cache_hash,
                                                fs_cache_compare);
        vc4->vs_cache = _mesa_hash_table_create(pctx, vs_cache_hash,
                                                vs_cache_compare);
        vc4->fs_inputs_set = _mesa_set_create(pctx, fs_inputs_hash,
                                              fs_inputs_compare);
}

void
vc4_program_fini(struct pipe_context *pctx)
{
        struct vc4_context *vc4 = vc4_context(pctx);

        struct hash_entry *entry;
        hash_table_foreach(vc4->fs_cache, entry) {
                struct vc4_compiled_shader *shader = entry->data;
                vc4_bo_unreference(&shader->bo);
                ralloc_free(shader);
                _mesa_hash_table_remove(vc4->fs_cache, entry);
        }

        hash_table_foreach(vc4->vs_cache, entry) {
                struct vc4_compiled_shader *shader = entry->data;
                vc4_bo_unreference(&shader->bo);
                ralloc_free(shader);
                _mesa_hash_table_remove(vc4->vs_cache, entry);
        }
}
