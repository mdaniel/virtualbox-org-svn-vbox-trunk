/*
 * Copyright © 2015 Broadcom
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
 * Implements most of the fixed function fragment pipeline in shader code.
 *
 * VC4 doesn't have any hardware support for blending, alpha test, logic ops,
 * or color mask.  Instead, you read the current contents of the destination
 * from the tile buffer after having waited for the scoreboard (which is
 * handled by vc4_qpu_emit.c), then do math using your output color and that
 * destination value, and update the output color appropriately.
 *
 * Once this pass is done, the color write will either have one component (for
 * single sample) with packed argb8888, or 4 components with the per-sample
 * argb8888 result.
 */

/**
 * Lowers fixed-function blending to a load of the destination color and a
 * series of ALU operations before the store of the output.
 */
#include "util/u_format.h"
#include "vc4_qir.h"
#include "compiler/nir/nir_builder.h"
#include "vc4_context.h"

static bool
blend_depends_on_dst_color(struct vc4_compile *c)
{
        return (c->fs_key->blend.blend_enable ||
                c->fs_key->blend.colormask != 0xf ||
                c->fs_key->logicop_func != PIPE_LOGICOP_COPY);
}

/** Emits a load of the previous fragment color from the tile buffer. */
static nir_ssa_def *
vc4_nir_get_dst_color(nir_builder *b, int sample)
{
        nir_intrinsic_instr *load =
                nir_intrinsic_instr_create(b->shader,
                                           nir_intrinsic_load_input);
        load->num_components = 1;
        nir_intrinsic_set_base(load, VC4_NIR_TLB_COLOR_READ_INPUT + sample);
        load->src[0] = nir_src_for_ssa(nir_imm_int(b, 0));
        nir_ssa_dest_init(&load->instr, &load->dest, 1, 32, NULL);
        nir_builder_instr_insert(b, &load->instr);
        return &load->dest.ssa;
}

static  nir_ssa_def *
vc4_nir_srgb_decode(nir_builder *b, nir_ssa_def *srgb)
{
        nir_ssa_def *is_low = nir_flt(b, srgb, nir_imm_float(b, 0.04045));
        nir_ssa_def *low = nir_fmul(b, srgb, nir_imm_float(b, 1.0 / 12.92));
        nir_ssa_def *high = nir_fpow(b,
                                     nir_fmul(b,
                                              nir_fadd(b, srgb,
                                                       nir_imm_float(b, 0.055)),
                                              nir_imm_float(b, 1.0 / 1.055)),
                                     nir_imm_float(b, 2.4));

        return nir_bcsel(b, is_low, low, high);
}

static  nir_ssa_def *
vc4_nir_srgb_encode(nir_builder *b, nir_ssa_def *linear)
{
        nir_ssa_def *is_low = nir_flt(b, linear, nir_imm_float(b, 0.0031308));
        nir_ssa_def *low = nir_fmul(b, linear, nir_imm_float(b, 12.92));
        nir_ssa_def *high = nir_fsub(b,
                                     nir_fmul(b,
                                              nir_imm_float(b, 1.055),
                                              nir_fpow(b,
                                                       linear,
                                                       nir_imm_float(b, 0.41666))),
                                     nir_imm_float(b, 0.055));

        return nir_bcsel(b, is_low, low, high);
}

static nir_ssa_def *
vc4_blend_channel_f(nir_builder *b,
                    nir_ssa_def **src,
                    nir_ssa_def **dst,
                    unsigned factor,
                    int channel)
{
        switch(factor) {
        case PIPE_BLENDFACTOR_ONE:
                return nir_imm_float(b, 1.0);
        case PIPE_BLENDFACTOR_SRC_COLOR:
                return src[channel];
        case PIPE_BLENDFACTOR_SRC_ALPHA:
                return src[3];
        case PIPE_BLENDFACTOR_DST_ALPHA:
                return dst[3];
        case PIPE_BLENDFACTOR_DST_COLOR:
                return dst[channel];
        case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE:
                if (channel != 3) {
                        return nir_fmin(b,
                                        src[3],
                                        nir_fsub(b,
                                                 nir_imm_float(b, 1.0),
                                                 dst[3]));
                } else {
                        return nir_imm_float(b, 1.0);
                }
        case PIPE_BLENDFACTOR_CONST_COLOR:
                return nir_load_system_value(b,
                                             nir_intrinsic_load_blend_const_color_r_float +
                                             channel,
                                             0);
        case PIPE_BLENDFACTOR_CONST_ALPHA:
                return nir_load_blend_const_color_a_float(b);
        case PIPE_BLENDFACTOR_ZERO:
                return nir_imm_float(b, 0.0);
        case PIPE_BLENDFACTOR_INV_SRC_COLOR:
                return nir_fsub(b, nir_imm_float(b, 1.0), src[channel]);
        case PIPE_BLENDFACTOR_INV_SRC_ALPHA:
                return nir_fsub(b, nir_imm_float(b, 1.0), src[3]);
        case PIPE_BLENDFACTOR_INV_DST_ALPHA:
                return nir_fsub(b, nir_imm_float(b, 1.0), dst[3]);
        case PIPE_BLENDFACTOR_INV_DST_COLOR:
                return nir_fsub(b, nir_imm_float(b, 1.0), dst[channel]);
        case PIPE_BLENDFACTOR_INV_CONST_COLOR:
                return nir_fsub(b, nir_imm_float(b, 1.0),
                                nir_load_system_value(b,
                                                      nir_intrinsic_load_blend_const_color_r_float +
                                                      channel,
                                                      0));
        case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
                return nir_fsub(b, nir_imm_float(b, 1.0),
                                nir_load_blend_const_color_a_float(b));

        default:
        case PIPE_BLENDFACTOR_SRC1_COLOR:
        case PIPE_BLENDFACTOR_SRC1_ALPHA:
        case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
        case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
                /* Unsupported. */
                fprintf(stderr, "Unknown blend factor %d\n", factor);
                return nir_imm_float(b, 1.0);
        }
}

static nir_ssa_def *
vc4_nir_set_packed_chan(nir_builder *b, nir_ssa_def *src0, nir_ssa_def *src1,
                        int chan)
{
        unsigned chan_mask = 0xff << (chan * 8);
        return nir_ior(b,
                       nir_iand(b, src0, nir_imm_int(b, ~chan_mask)),
                       nir_iand(b, src1, nir_imm_int(b, chan_mask)));
}

static nir_ssa_def *
vc4_blend_channel_i(nir_builder *b,
                    nir_ssa_def *src,
                    nir_ssa_def *dst,
                    nir_ssa_def *src_a,
                    nir_ssa_def *dst_a,
                    unsigned factor,
                    int a_chan)
{
        switch (factor) {
        case PIPE_BLENDFACTOR_ONE:
                return nir_imm_int(b, ~0);
        case PIPE_BLENDFACTOR_SRC_COLOR:
                return src;
        case PIPE_BLENDFACTOR_SRC_ALPHA:
                return src_a;
        case PIPE_BLENDFACTOR_DST_ALPHA:
                return dst_a;
        case PIPE_BLENDFACTOR_DST_COLOR:
                return dst;
        case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE:
                return vc4_nir_set_packed_chan(b,
                                               nir_umin_4x8(b,
                                                            src_a,
                                                            nir_inot(b, dst_a)),
                                               nir_imm_int(b, ~0),
                                               a_chan);
        case PIPE_BLENDFACTOR_CONST_COLOR:
                return nir_load_blend_const_color_rgba8888_unorm(b);
        case PIPE_BLENDFACTOR_CONST_ALPHA:
                return nir_load_blend_const_color_aaaa8888_unorm(b);
        case PIPE_BLENDFACTOR_ZERO:
                return nir_imm_int(b, 0);
        case PIPE_BLENDFACTOR_INV_SRC_COLOR:
                return nir_inot(b, src);
        case PIPE_BLENDFACTOR_INV_SRC_ALPHA:
                return nir_inot(b, src_a);
        case PIPE_BLENDFACTOR_INV_DST_ALPHA:
                return nir_inot(b, dst_a);
        case PIPE_BLENDFACTOR_INV_DST_COLOR:
                return nir_inot(b, dst);
        case PIPE_BLENDFACTOR_INV_CONST_COLOR:
                return nir_inot(b,
                                nir_load_blend_const_color_rgba8888_unorm(b));
        case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
                return nir_inot(b,
                                nir_load_blend_const_color_aaaa8888_unorm(b));

        default:
        case PIPE_BLENDFACTOR_SRC1_COLOR:
        case PIPE_BLENDFACTOR_SRC1_ALPHA:
        case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
        case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
                /* Unsupported. */
                fprintf(stderr, "Unknown blend factor %d\n", factor);
                return nir_imm_int(b, ~0);
        }
}

static nir_ssa_def *
vc4_blend_func_f(nir_builder *b, nir_ssa_def *src, nir_ssa_def *dst,
                 unsigned func)
{
        switch (func) {
        case PIPE_BLEND_ADD:
                return nir_fadd(b, src, dst);
        case PIPE_BLEND_SUBTRACT:
                return nir_fsub(b, src, dst);
        case PIPE_BLEND_REVERSE_SUBTRACT:
                return nir_fsub(b, dst, src);
        case PIPE_BLEND_MIN:
                return nir_fmin(b, src, dst);
        case PIPE_BLEND_MAX:
                return nir_fmax(b, src, dst);

        default:
                /* Unsupported. */
                fprintf(stderr, "Unknown blend func %d\n", func);
                return src;

        }
}

static nir_ssa_def *
vc4_blend_func_i(nir_builder *b, nir_ssa_def *src, nir_ssa_def *dst,
                 unsigned func)
{
        switch (func) {
        case PIPE_BLEND_ADD:
                return nir_usadd_4x8(b, src, dst);
        case PIPE_BLEND_SUBTRACT:
                return nir_ussub_4x8(b, src, dst);
        case PIPE_BLEND_REVERSE_SUBTRACT:
                return nir_ussub_4x8(b, dst, src);
        case PIPE_BLEND_MIN:
                return nir_umin_4x8(b, src, dst);
        case PIPE_BLEND_MAX:
                return nir_umax_4x8(b, src, dst);

        default:
                /* Unsupported. */
                fprintf(stderr, "Unknown blend func %d\n", func);
                return src;

        }
}

static void
vc4_do_blending_f(struct vc4_compile *c, nir_builder *b, nir_ssa_def **result,
                  nir_ssa_def **src_color, nir_ssa_def **dst_color)
{
        struct pipe_rt_blend_state *blend = &c->fs_key->blend;

        if (!blend->blend_enable) {
                for (int i = 0; i < 4; i++)
                        result[i] = src_color[i];
                return;
        }

        /* Clamp the src color to [0, 1].  Dest is already clamped. */
        for (int i = 0; i < 4; i++)
                src_color[i] = nir_fsat(b, src_color[i]);

        nir_ssa_def *src_blend[4], *dst_blend[4];
        for (int i = 0; i < 4; i++) {
                int src_factor = ((i != 3) ? blend->rgb_src_factor :
                                  blend->alpha_src_factor);
                int dst_factor = ((i != 3) ? blend->rgb_dst_factor :
                                  blend->alpha_dst_factor);
                src_blend[i] = nir_fmul(b, src_color[i],
                                        vc4_blend_channel_f(b,
                                                            src_color, dst_color,
                                                            src_factor, i));
                dst_blend[i] = nir_fmul(b, dst_color[i],
                                        vc4_blend_channel_f(b,
                                                            src_color, dst_color,
                                                            dst_factor, i));
        }

        for (int i = 0; i < 4; i++) {
                result[i] = vc4_blend_func_f(b, src_blend[i], dst_blend[i],
                                             ((i != 3) ? blend->rgb_func :
                                              blend->alpha_func));
        }
}

static nir_ssa_def *
vc4_nir_splat(nir_builder *b, nir_ssa_def *src)
{
        nir_ssa_def *or1 = nir_ior(b, src, nir_ishl(b, src, nir_imm_int(b, 8)));
        return nir_ior(b, or1, nir_ishl(b, or1, nir_imm_int(b, 16)));
}

static nir_ssa_def *
vc4_do_blending_i(struct vc4_compile *c, nir_builder *b,
                  nir_ssa_def *src_color, nir_ssa_def *dst_color,
                  nir_ssa_def *src_float_a)
{
        struct pipe_rt_blend_state *blend = &c->fs_key->blend;

        if (!blend->blend_enable)
                return src_color;

        enum pipe_format color_format = c->fs_key->color_format;
        const uint8_t *format_swiz = vc4_get_format_swizzle(color_format);
        nir_ssa_def *imm_0xff = nir_imm_int(b, 0xff);
        nir_ssa_def *src_a = nir_pack_unorm_4x8(b, src_float_a);
        nir_ssa_def *dst_a;
        int alpha_chan;
        for (alpha_chan = 0; alpha_chan < 4; alpha_chan++) {
                if (format_swiz[alpha_chan] == 3)
                        break;
        }
        if (alpha_chan != 4) {
                nir_ssa_def *shift = nir_imm_int(b, alpha_chan * 8);
                dst_a = vc4_nir_splat(b, nir_iand(b, nir_ushr(b, dst_color,
                                                              shift), imm_0xff));
        } else {
                dst_a = nir_imm_int(b, ~0);
        }

        nir_ssa_def *src_factor = vc4_blend_channel_i(b,
                                                      src_color, dst_color,
                                                      src_a, dst_a,
                                                      blend->rgb_src_factor,
                                                      alpha_chan);
        nir_ssa_def *dst_factor = vc4_blend_channel_i(b,
                                                      src_color, dst_color,
                                                      src_a, dst_a,
                                                      blend->rgb_dst_factor,
                                                      alpha_chan);

        if (alpha_chan != 4 &&
            blend->alpha_src_factor != blend->rgb_src_factor) {
                nir_ssa_def *src_alpha_factor =
                        vc4_blend_channel_i(b,
                                            src_color, dst_color,
                                            src_a, dst_a,
                                            blend->alpha_src_factor,
                                            alpha_chan);
                src_factor = vc4_nir_set_packed_chan(b, src_factor,
                                                     src_alpha_factor,
                                                     alpha_chan);
        }
        if (alpha_chan != 4 &&
            blend->alpha_dst_factor != blend->rgb_dst_factor) {
                nir_ssa_def *dst_alpha_factor =
                        vc4_blend_channel_i(b,
                                            src_color, dst_color,
                                            src_a, dst_a,
                                            blend->alpha_dst_factor,
                                            alpha_chan);
                dst_factor = vc4_nir_set_packed_chan(b, dst_factor,
                                                     dst_alpha_factor,
                                                     alpha_chan);
        }
        nir_ssa_def *src_blend = nir_umul_unorm_4x8(b, src_color, src_factor);
        nir_ssa_def *dst_blend = nir_umul_unorm_4x8(b, dst_color, dst_factor);

        nir_ssa_def *result =
                vc4_blend_func_i(b, src_blend, dst_blend, blend->rgb_func);
        if (alpha_chan != 4 && blend->alpha_func != blend->rgb_func) {
                nir_ssa_def *result_a = vc4_blend_func_i(b,
                                                         src_blend,
                                                         dst_blend,
                                                         blend->alpha_func);
                result = vc4_nir_set_packed_chan(b, result, result_a,
                                                 alpha_chan);
        }
        return result;
}

static nir_ssa_def *
vc4_logicop(nir_builder *b, int logicop_func,
            nir_ssa_def *src, nir_ssa_def *dst)
{
        switch (logicop_func) {
        case PIPE_LOGICOP_CLEAR:
                return nir_imm_int(b, 0);
        case PIPE_LOGICOP_NOR:
                return nir_inot(b, nir_ior(b, src, dst));
        case PIPE_LOGICOP_AND_INVERTED:
                return nir_iand(b, nir_inot(b, src), dst);
        case PIPE_LOGICOP_COPY_INVERTED:
                return nir_inot(b, src);
        case PIPE_LOGICOP_AND_REVERSE:
                return nir_iand(b, src, nir_inot(b, dst));
        case PIPE_LOGICOP_INVERT:
                return nir_inot(b, dst);
        case PIPE_LOGICOP_XOR:
                return nir_ixor(b, src, dst);
        case PIPE_LOGICOP_NAND:
                return nir_inot(b, nir_iand(b, src, dst));
        case PIPE_LOGICOP_AND:
                return nir_iand(b, src, dst);
        case PIPE_LOGICOP_EQUIV:
                return nir_inot(b, nir_ixor(b, src, dst));
        case PIPE_LOGICOP_NOOP:
                return dst;
        case PIPE_LOGICOP_OR_INVERTED:
                return nir_ior(b, nir_inot(b, src), dst);
        case PIPE_LOGICOP_OR_REVERSE:
                return nir_ior(b, src, nir_inot(b, dst));
        case PIPE_LOGICOP_OR:
                return nir_ior(b, src, dst);
        case PIPE_LOGICOP_SET:
                return nir_imm_int(b, ~0);
        default:
                fprintf(stderr, "Unknown logic op %d\n", logicop_func);
                /* FALLTHROUGH */
        case PIPE_LOGICOP_COPY:
                return src;
        }
}

static nir_ssa_def *
vc4_nir_swizzle_and_pack(struct vc4_compile *c, nir_builder *b,
                         nir_ssa_def **colors)
{
        enum pipe_format color_format = c->fs_key->color_format;
        const uint8_t *format_swiz = vc4_get_format_swizzle(color_format);

        nir_ssa_def *swizzled[4];
        for (int i = 0; i < 4; i++) {
                swizzled[i] = vc4_nir_get_swizzled_channel(b, colors,
                                                           format_swiz[i]);
        }

        return nir_pack_unorm_4x8(b,
                                  nir_vec4(b,
                                           swizzled[0], swizzled[1],
                                           swizzled[2], swizzled[3]));

}

static nir_ssa_def *
vc4_nir_blend_pipeline(struct vc4_compile *c, nir_builder *b, nir_ssa_def *src,
                       int sample)
{
        enum pipe_format color_format = c->fs_key->color_format;
        const uint8_t *format_swiz = vc4_get_format_swizzle(color_format);
        bool srgb = util_format_is_srgb(color_format);

        /* Pull out the float src/dst color components. */
        nir_ssa_def *packed_dst_color = vc4_nir_get_dst_color(b, sample);
        nir_ssa_def *dst_vec4 = nir_unpack_unorm_4x8(b, packed_dst_color);
        nir_ssa_def *src_color[4], *unpacked_dst_color[4];
        for (unsigned i = 0; i < 4; i++) {
                src_color[i] = nir_channel(b, src, i);
                unpacked_dst_color[i] = nir_channel(b, dst_vec4, i);
        }

        if (c->fs_key->sample_alpha_to_one && c->fs_key->msaa)
                src_color[3] = nir_imm_float(b, 1.0);

        nir_ssa_def *packed_color;
        if (srgb) {
                /* Unswizzle the destination color. */
                nir_ssa_def *dst_color[4];
                for (unsigned i = 0; i < 4; i++) {
                        dst_color[i] = vc4_nir_get_swizzled_channel(b,
                                                                    unpacked_dst_color,
                                                                    format_swiz[i]);
                }

                /* Turn dst color to linear. */
                for (int i = 0; i < 3; i++)
                        dst_color[i] = vc4_nir_srgb_decode(b, dst_color[i]);

                nir_ssa_def *blend_color[4];
                vc4_do_blending_f(c, b, blend_color, src_color, dst_color);

                /* sRGB encode the output color */
                for (int i = 0; i < 3; i++)
                        blend_color[i] = vc4_nir_srgb_encode(b, blend_color[i]);

                packed_color = vc4_nir_swizzle_and_pack(c, b, blend_color);
        } else {
                nir_ssa_def *packed_src_color =
                        vc4_nir_swizzle_and_pack(c, b, src_color);

                packed_color =
                        vc4_do_blending_i(c, b,
                                          packed_src_color, packed_dst_color,
                                          src_color[3]);
        }

        packed_color = vc4_logicop(b, c->fs_key->logicop_func,
                                   packed_color, packed_dst_color);

        /* If the bit isn't set in the color mask, then just return the
         * original dst color, instead.
         */
        uint32_t colormask = 0xffffffff;
        for (int i = 0; i < 4; i++) {
                if (format_swiz[i] < 4 &&
                    !(c->fs_key->blend.colormask & (1 << format_swiz[i]))) {
                        colormask &= ~(0xff << (i * 8));
                }
        }

        return nir_ior(b,
                       nir_iand(b, packed_color,
                                nir_imm_int(b, colormask)),
                       nir_iand(b, packed_dst_color,
                                nir_imm_int(b, ~colormask)));
}

static int
vc4_nir_next_output_driver_location(nir_shader *s)
{
        int maxloc = -1;

        nir_foreach_variable(var, &s->outputs)
                maxloc = MAX2(maxloc, (int)var->data.driver_location);

        return maxloc + 1;
}

static void
vc4_nir_store_sample_mask(struct vc4_compile *c, nir_builder *b,
                          nir_ssa_def *val)
{
        nir_variable *sample_mask = nir_variable_create(c->s, nir_var_shader_out,
                                                        glsl_uint_type(),
                                                        "sample_mask");
        sample_mask->data.driver_location =
                vc4_nir_next_output_driver_location(c->s);
        sample_mask->data.location = FRAG_RESULT_SAMPLE_MASK;

        nir_intrinsic_instr *intr =
                nir_intrinsic_instr_create(c->s, nir_intrinsic_store_output);
        intr->num_components = 1;
        nir_intrinsic_set_base(intr, sample_mask->data.driver_location);

        intr->src[0] = nir_src_for_ssa(val);
        intr->src[1] = nir_src_for_ssa(nir_imm_int(b, 0));
        nir_builder_instr_insert(b, &intr->instr);
}

static void
vc4_nir_lower_blend_instr(struct vc4_compile *c, nir_builder *b,
                          nir_intrinsic_instr *intr)
{
        nir_ssa_def *frag_color = intr->src[0].ssa;

        if (c->fs_key->sample_alpha_to_coverage) {
                nir_ssa_def *a = nir_channel(b, frag_color, 3);

                /* XXX: We should do a nice dither based on the fragment
                 * coordinate, instead.
                 */
                nir_ssa_def *num_samples = nir_imm_float(b, VC4_MAX_SAMPLES);
                nir_ssa_def *num_bits = nir_f2i32(b, nir_fmul(b, a, num_samples));
                nir_ssa_def *bitmask = nir_isub(b,
                                                nir_ishl(b,
                                                         nir_imm_int(b, 1),
                                                         num_bits),
                                                nir_imm_int(b, 1));
                vc4_nir_store_sample_mask(c, b, bitmask);
        }

        /* The TLB color read returns each sample in turn, so if our blending
         * depends on the destination color, we're going to have to run the
         * blending function separately for each destination sample value, and
         * then output the per-sample color using TLB_COLOR_MS.
         */
        nir_ssa_def *blend_output;
        if (c->fs_key->msaa && blend_depends_on_dst_color(c)) {
                c->msaa_per_sample_output = true;

                nir_ssa_def *samples[4];
                for (int i = 0; i < VC4_MAX_SAMPLES; i++)
                        samples[i] = vc4_nir_blend_pipeline(c, b, frag_color, i);
                blend_output = nir_vec4(b,
                                        samples[0], samples[1],
                                        samples[2], samples[3]);
        } else {
                blend_output = vc4_nir_blend_pipeline(c, b, frag_color, 0);
        }

        nir_instr_rewrite_src(&intr->instr, &intr->src[0],
                              nir_src_for_ssa(blend_output));
        intr->num_components = blend_output->num_components;
}

static bool
vc4_nir_lower_blend_block(nir_block *block, struct vc4_compile *c)
{
        nir_foreach_instr_safe(instr, block) {
                if (instr->type != nir_instr_type_intrinsic)
                        continue;
                nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
                if (intr->intrinsic != nir_intrinsic_store_output)
                        continue;

                nir_variable *output_var = NULL;
                nir_foreach_variable(var, &c->s->outputs) {
                        if (var->data.driver_location ==
                            nir_intrinsic_base(intr)) {
                                output_var = var;
                                break;
                        }
                }
                assert(output_var);

                if (output_var->data.location != FRAG_RESULT_COLOR &&
                    output_var->data.location != FRAG_RESULT_DATA0) {
                        continue;
                }

                nir_function_impl *impl =
                        nir_cf_node_get_function(&block->cf_node);
                nir_builder b;
                nir_builder_init(&b, impl);
                b.cursor = nir_before_instr(&intr->instr);
                vc4_nir_lower_blend_instr(c, &b, intr);
        }
        return true;
}

void
vc4_nir_lower_blend(nir_shader *s, struct vc4_compile *c)
{
        nir_foreach_function(function, s) {
                if (function->impl) {
                        nir_foreach_block(block, function->impl) {
                                vc4_nir_lower_blend_block(block, c);
                        }

                        nir_metadata_preserve(function->impl,
                                              nir_metadata_block_index |
                                              nir_metadata_dominance);
                }
        }

        /* If we didn't do alpha-to-coverage on the output color, we still
         * need to pass glSampleMask() through.
         */
        if (c->fs_key->sample_coverage && !c->fs_key->sample_alpha_to_coverage) {
                nir_function_impl *impl = nir_shader_get_entrypoint(s);
                nir_builder b;
                nir_builder_init(&b, impl);
                b.cursor = nir_after_block(nir_impl_last_block(impl));

                vc4_nir_store_sample_mask(c, &b, nir_load_sample_mask_in(&b));
        }
}
