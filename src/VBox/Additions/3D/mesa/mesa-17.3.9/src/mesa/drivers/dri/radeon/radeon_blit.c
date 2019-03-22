/*
 * Copyright (C) 2010 Advanced Micro Devices, Inc.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "radeon_common.h"
#include "radeon_context.h"
#include "radeon_blit.h"
#include "radeon_tex.h"

static inline uint32_t cmdpacket0(struct radeon_screen *rscrn,
                                  int reg, int count)
{
    if (count)
	    return CP_PACKET0(reg, count - 1);
    return CP_PACKET2;
}

/* common formats supported as both textures and render targets */
unsigned r100_check_blit(mesa_format mesa_format, uint32_t dst_pitch)
{
    /* XXX others?  */
    if (_mesa_little_endian()) {
	switch (mesa_format) {
	case MESA_FORMAT_B8G8R8A8_UNORM:
	case MESA_FORMAT_B8G8R8X8_UNORM:
	case MESA_FORMAT_B5G6R5_UNORM:
	case MESA_FORMAT_B4G4R4A4_UNORM:
	case MESA_FORMAT_B5G5R5A1_UNORM:
	case MESA_FORMAT_A_UNORM8:
	case MESA_FORMAT_L_UNORM8:
	case MESA_FORMAT_I_UNORM8:
	    break;
	default:
	    return 0;
	}
    }
    else {
	switch (mesa_format) {
	case MESA_FORMAT_A8R8G8B8_UNORM:
	case MESA_FORMAT_X8R8G8B8_UNORM:
	case MESA_FORMAT_R5G6B5_UNORM:
	case MESA_FORMAT_A4R4G4B4_UNORM:
	case MESA_FORMAT_A1R5G5B5_UNORM:
	case MESA_FORMAT_A_UNORM8:
	case MESA_FORMAT_L_UNORM8:
	case MESA_FORMAT_I_UNORM8:
	    break;
	default:
	    return 0;
	}
    }

    /* Rendering to small buffer doesn't work.
     * Looks like a hw limitation.
     */
    if (dst_pitch < 32)
        return 0;

    /* ??? */
    if (_mesa_get_format_bits(mesa_format, GL_DEPTH_BITS) > 0)
	    return 0;

    return 1;
}

static inline void emit_vtx_state(struct r100_context *r100)
{
    BATCH_LOCALS(&r100->radeon);

    BEGIN_BATCH(8);
    if (r100->radeon.radeonScreen->chip_flags & RADEON_CHIPSET_TCL) {
	    OUT_BATCH_REGVAL(RADEON_SE_CNTL_STATUS, 0);
    } else {
	    OUT_BATCH_REGVAL(RADEON_SE_CNTL_STATUS, RADEON_TCL_BYPASS);

    }
    OUT_BATCH_REGVAL(RADEON_SE_COORD_FMT, (RADEON_VTX_XY_PRE_MULT_1_OVER_W0 |
					   RADEON_TEX1_W_ROUTING_USE_W0));
    OUT_BATCH_REGVAL(RADEON_SE_VTX_FMT, RADEON_SE_VTX_FMT_XY | RADEON_SE_VTX_FMT_ST0);
    OUT_BATCH_REGVAL(RADEON_SE_CNTL, (RADEON_DIFFUSE_SHADE_GOURAUD |
				      RADEON_BFACE_SOLID |
				      RADEON_FFACE_SOLID |
				      RADEON_VTX_PIX_CENTER_OGL |
				      RADEON_ROUND_MODE_ROUND |
				      RADEON_ROUND_PREC_4TH_PIX));
    END_BATCH();
}

static void inline emit_tx_setup(struct r100_context *r100,
				 mesa_format mesa_format,
				 struct radeon_bo *bo,
				 intptr_t offset,
				 unsigned width,
				 unsigned height,
				 unsigned pitch)
{
    uint32_t txformat = RADEON_TXFORMAT_NON_POWER2;
    BATCH_LOCALS(&r100->radeon);

    assert(width <= 2048);
    assert(height <= 2048);
    assert(offset % 32 == 0);

    txformat |= tx_table[mesa_format].format;

    if (bo->flags & RADEON_BO_FLAGS_MACRO_TILE)
       offset |= RADEON_TXO_MACRO_TILE;
    if (bo->flags & RADEON_BO_FLAGS_MICRO_TILE)
       offset |= RADEON_TXO_MICRO_TILE_X2;

    BEGIN_BATCH(18);
    OUT_BATCH_REGVAL(RADEON_PP_CNTL, RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE);
    OUT_BATCH_REGVAL(RADEON_PP_TXCBLEND_0, (RADEON_COLOR_ARG_A_ZERO |
					    RADEON_COLOR_ARG_B_ZERO |
					    RADEON_COLOR_ARG_C_T0_COLOR |
					    RADEON_BLEND_CTL_ADD |
					    RADEON_CLAMP_TX));
    OUT_BATCH_REGVAL(RADEON_PP_TXABLEND_0, (RADEON_ALPHA_ARG_A_ZERO |
					    RADEON_ALPHA_ARG_B_ZERO |
					    RADEON_ALPHA_ARG_C_T0_ALPHA |
					    RADEON_BLEND_CTL_ADD |
					    RADEON_CLAMP_TX));
    OUT_BATCH_REGVAL(RADEON_PP_TXFILTER_0, (RADEON_CLAMP_S_CLAMP_LAST |
					    RADEON_CLAMP_T_CLAMP_LAST |
					    RADEON_MAG_FILTER_NEAREST |
					    RADEON_MIN_FILTER_NEAREST));
    OUT_BATCH_REGVAL(RADEON_PP_TXFORMAT_0, txformat);
    OUT_BATCH_REGVAL(RADEON_PP_TEX_SIZE_0, ((width - 1) |
					    ((height - 1) << RADEON_TEX_VSIZE_SHIFT)));
    OUT_BATCH_REGVAL(RADEON_PP_TEX_PITCH_0, pitch * _mesa_get_format_bytes(mesa_format) - 32);

    OUT_BATCH_REGSEQ(RADEON_PP_TXOFFSET_0, 1);
    OUT_BATCH_RELOC(offset, bo, offset, RADEON_GEM_DOMAIN_GTT|RADEON_GEM_DOMAIN_VRAM, 0, 0);

    END_BATCH();
}

static inline void emit_cb_setup(struct r100_context *r100,
				 struct radeon_bo *bo,
				 intptr_t offset,
				 mesa_format mesa_format,
				 unsigned pitch,
				 unsigned width,
				 unsigned height)
{
    uint32_t dst_pitch = pitch;
    uint32_t dst_format = 0;
    BATCH_LOCALS(&r100->radeon);

    /* XXX others? */
    switch (mesa_format) {
    /* The first of each pair is for little, the second for big endian. */
    case MESA_FORMAT_B8G8R8A8_UNORM:
    case MESA_FORMAT_A8R8G8B8_UNORM:
    case MESA_FORMAT_B8G8R8X8_UNORM:
    case MESA_FORMAT_X8R8G8B8_UNORM:
	    dst_format = RADEON_COLOR_FORMAT_ARGB8888;
	    break;
    case MESA_FORMAT_B5G6R5_UNORM:
    case MESA_FORMAT_R5G6B5_UNORM:
	    dst_format = RADEON_COLOR_FORMAT_RGB565;
	    break;
    case MESA_FORMAT_B4G4R4A4_UNORM:
    case MESA_FORMAT_A4R4G4B4_UNORM:
	    dst_format = RADEON_COLOR_FORMAT_ARGB4444;
	    break;
    case MESA_FORMAT_B5G5R5A1_UNORM:
    case MESA_FORMAT_A1R5G5B5_UNORM:
	    dst_format = RADEON_COLOR_FORMAT_ARGB1555;
	    break;
    case MESA_FORMAT_A_UNORM8:
    case MESA_FORMAT_L_UNORM8:
    case MESA_FORMAT_I_UNORM8:
	    dst_format = RADEON_COLOR_FORMAT_RGB8;
	    break;
    default:
	    break;
    }

    if (bo->flags & RADEON_BO_FLAGS_MACRO_TILE)
        dst_pitch |= RADEON_COLOR_TILE_ENABLE;

    if (bo->flags & RADEON_BO_FLAGS_MICRO_TILE)
        dst_pitch |= RADEON_COLOR_MICROTILE_ENABLE;

    BEGIN_BATCH(18);
    OUT_BATCH_REGVAL(RADEON_RE_TOP_LEFT, 0);
    OUT_BATCH_REGVAL(RADEON_RE_WIDTH_HEIGHT, (((width - 1) << RADEON_RE_WIDTH_SHIFT) |
					      ((height - 1) << RADEON_RE_HEIGHT_SHIFT)));
    OUT_BATCH_REGVAL(RADEON_RB3D_PLANEMASK, 0xffffffff);
    OUT_BATCH_REGVAL(RADEON_RB3D_BLENDCNTL, RADEON_SRC_BLEND_GL_ONE | RADEON_DST_BLEND_GL_ZERO);
    OUT_BATCH_REGVAL(RADEON_RB3D_CNTL, dst_format);

    OUT_BATCH_REGSEQ(RADEON_RB3D_COLOROFFSET, 1);
    OUT_BATCH_RELOC(offset, bo, offset, 0, RADEON_GEM_DOMAIN_GTT|RADEON_GEM_DOMAIN_VRAM, 0);
    OUT_BATCH_REGSEQ(RADEON_RB3D_COLORPITCH, 1);
    OUT_BATCH_RELOC(dst_pitch, bo, dst_pitch, 0, RADEON_GEM_DOMAIN_GTT|RADEON_GEM_DOMAIN_VRAM, 0);

    END_BATCH();
}

static GLboolean validate_buffers(struct r100_context *r100,
                                  struct radeon_bo *src_bo,
                                  struct radeon_bo *dst_bo)
{
    int ret;

    radeon_cs_space_reset_bos(r100->radeon.cmdbuf.cs);

    ret = radeon_cs_space_check_with_bo(r100->radeon.cmdbuf.cs,
                                        src_bo, RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT, 0);
    if (ret)
        return GL_FALSE;

    ret = radeon_cs_space_check_with_bo(r100->radeon.cmdbuf.cs,
                                        dst_bo, 0, RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT);
    if (ret)
        return GL_FALSE;

    return GL_TRUE;
}

/**
 * Calculate texcoords for given image region.
 * Output values are [minx, maxx, miny, maxy]
 */
static inline void calc_tex_coords(float img_width, float img_height,
				   float x, float y,
				   float reg_width, float reg_height,
				   unsigned flip_y, float *buf)
{
    buf[0] = x / img_width;
    buf[1] = buf[0] + reg_width / img_width;
    buf[2] = y / img_height;
    buf[3] = buf[2] + reg_height / img_height;
    if (flip_y)
    {
        buf[2] = 1.0 - buf[2];
        buf[3] = 1.0 - buf[3];
    }
}

static inline void emit_draw_packet(struct r100_context *r100,
				    unsigned src_width, unsigned src_height,
				    unsigned src_x_offset, unsigned src_y_offset,
				    unsigned dst_x_offset, unsigned dst_y_offset,
				    unsigned reg_width, unsigned reg_height,
				    unsigned flip_y)
{
    float texcoords[4];
    float verts[12];
    BATCH_LOCALS(&r100->radeon);

    calc_tex_coords(src_width, src_height,
                    src_x_offset, src_y_offset,
                    reg_width, reg_height,
                    flip_y, texcoords);

    verts[0] = dst_x_offset;
    verts[1] = dst_y_offset + reg_height;
    verts[2] = texcoords[0];
    verts[3] = texcoords[3];

    verts[4] = dst_x_offset + reg_width;
    verts[5] = dst_y_offset + reg_height;
    verts[6] = texcoords[1];
    verts[7] = texcoords[3];

    verts[8] = dst_x_offset + reg_width;
    verts[9] = dst_y_offset;
    verts[10] = texcoords[1];
    verts[11] = texcoords[2];

    BEGIN_BATCH(15);
    OUT_BATCH(RADEON_CP_PACKET3_3D_DRAW_IMMD | (13 << 16));
    OUT_BATCH(RADEON_CP_VC_FRMT_XY | RADEON_CP_VC_FRMT_ST0);
    OUT_BATCH(RADEON_CP_VC_CNTL_PRIM_WALK_RING |
	      RADEON_CP_VC_CNTL_PRIM_TYPE_RECT_LIST |
	      RADEON_CP_VC_CNTL_MAOS_ENABLE |
	      RADEON_CP_VC_CNTL_VTX_FMT_RADEON_MODE |
              (3 << 16));
    OUT_BATCH_TABLE(verts, 12);
    END_BATCH();
}

/**
 * Copy a region of [@a width x @a height] pixels from source buffer
 * to destination buffer.
 * @param[in] r100 r100 context
 * @param[in] src_bo source radeon buffer object
 * @param[in] src_offset offset of the source image in the @a src_bo
 * @param[in] src_mesaformat source image format
 * @param[in] src_pitch aligned source image width
 * @param[in] src_width source image width
 * @param[in] src_height source image height
 * @param[in] src_x_offset x offset in the source image
 * @param[in] src_y_offset y offset in the source image
 * @param[in] dst_bo destination radeon buffer object
 * @param[in] dst_offset offset of the destination image in the @a dst_bo
 * @param[in] dst_mesaformat destination image format
 * @param[in] dst_pitch aligned destination image width
 * @param[in] dst_width destination image width
 * @param[in] dst_height destination image height
 * @param[in] dst_x_offset x offset in the destination image
 * @param[in] dst_y_offset y offset in the destination image
 * @param[in] width region width
 * @param[in] height region height
 * @param[in] flip_y set if y coords of the source image need to be flipped
 */
unsigned r100_blit(struct gl_context *ctx,
                   struct radeon_bo *src_bo,
                   intptr_t src_offset,
                   mesa_format src_mesaformat,
                   unsigned src_pitch,
                   unsigned src_width,
                   unsigned src_height,
                   unsigned src_x_offset,
                   unsigned src_y_offset,
                   struct radeon_bo *dst_bo,
                   intptr_t dst_offset,
                   mesa_format dst_mesaformat,
                   unsigned dst_pitch,
                   unsigned dst_width,
                   unsigned dst_height,
                   unsigned dst_x_offset,
                   unsigned dst_y_offset,
                   unsigned reg_width,
                   unsigned reg_height,
                   unsigned flip_y)
{
    struct r100_context *r100 = R100_CONTEXT(ctx);

    if (!r100_check_blit(dst_mesaformat, dst_pitch))
        return GL_FALSE;

    /* Make sure that colorbuffer has even width - hw limitation */
    if (dst_pitch % 2 > 0)
        ++dst_pitch;

    /* Need to clamp the region size to make sure
     * we don't read outside of the source buffer
     * or write outside of the destination buffer.
     */
    if (reg_width + src_x_offset > src_width)
        reg_width = src_width - src_x_offset;
    if (reg_height + src_y_offset > src_height)
        reg_height = src_height - src_y_offset;
    if (reg_width + dst_x_offset > dst_width)
        reg_width = dst_width - dst_x_offset;
    if (reg_height + dst_y_offset > dst_height)
        reg_height = dst_height - dst_y_offset;

    if (src_bo == dst_bo) {
        return GL_FALSE;
    }

    if (src_offset % 32 || dst_offset % 32) {
        return GL_FALSE;
    }

    if (0) {
        fprintf(stderr, "src: size [%d x %d], pitch %d, offset %zd "
                "offset [%d x %d], format %s, bo %p\n",
                src_width, src_height, src_pitch, src_offset,
                src_x_offset, src_y_offset,
                _mesa_get_format_name(src_mesaformat),
                src_bo);
        fprintf(stderr, "dst: pitch %d offset %zd, offset[%d x %d], format %s, bo %p\n",
                dst_pitch, dst_offset,  dst_x_offset, dst_y_offset,
                _mesa_get_format_name(dst_mesaformat), dst_bo);
        fprintf(stderr, "region: %d x %d\n", reg_width, reg_height);
    }

    /* Flush is needed to make sure that source buffer has correct data */
    radeonFlush(ctx);

    rcommonEnsureCmdBufSpace(&r100->radeon, 59, __func__);

    if (!validate_buffers(r100, src_bo, dst_bo))
        return GL_FALSE;

    /* 8 */
    emit_vtx_state(r100);
    /* 18 */
    emit_tx_setup(r100, src_mesaformat, src_bo, src_offset, src_width, src_height, src_pitch);
    /* 18 */
    emit_cb_setup(r100, dst_bo, dst_offset, dst_mesaformat, dst_pitch, dst_width, dst_height);
    /* 15 */
    emit_draw_packet(r100, src_width, src_height,
                     src_x_offset, src_y_offset,
                     dst_x_offset, dst_y_offset,
                     reg_width, reg_height,
                     flip_y);

    radeonFlush(ctx);

    /* We submitted those packets outside our state atom mechanism. Thus
     * make sure they are all resubmitted the next time. */
    r100->hw.ctx.dirty = GL_TRUE;
    r100->hw.msk.dirty = GL_TRUE;
    r100->hw.set.dirty = GL_TRUE;
    r100->hw.tex[0].dirty = GL_TRUE;
    r100->hw.txr[0].dirty = GL_TRUE;

    return GL_TRUE;
}
