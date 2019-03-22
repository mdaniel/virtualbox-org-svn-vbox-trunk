/*
 * Copyright (C) 2016 Rob Clark <robclark@freedesktop.org>
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include "pipe/p_state.h"
#include "util/u_string.h"
#include "util/u_memory.h"
#include "util/u_helpers.h"
#include "util/u_format.h"
#include "util/u_viewport.h"

#include "freedreno_resource.h"
#include "freedreno_query_hw.h"

#include "fd5_emit.h"
#include "fd5_blend.h"
#include "fd5_context.h"
#include "fd5_program.h"
#include "fd5_rasterizer.h"
#include "fd5_texture.h"
#include "fd5_format.h"
#include "fd5_zsa.h"

/* regid:          base const register
 * prsc or dwords: buffer containing constant values
 * sizedwords:     size of const value buffer
 */
static void
fd5_emit_const(struct fd_ringbuffer *ring, enum shader_t type,
		uint32_t regid, uint32_t offset, uint32_t sizedwords,
		const uint32_t *dwords, struct pipe_resource *prsc)
{
	uint32_t i, sz;
	enum a4xx_state_src src;

	debug_assert((regid % 4) == 0);
	debug_assert((sizedwords % 4) == 0);

	if (prsc) {
		sz = 0;
		src = SS4_INDIRECT;
	} else {
		sz = sizedwords;
		src = SS4_DIRECT;
	}

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + sz);
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(regid/4) |
			CP_LOAD_STATE4_0_STATE_SRC(src) |
			CP_LOAD_STATE4_0_STATE_BLOCK(fd4_stage2shadersb(type)) |
			CP_LOAD_STATE4_0_NUM_UNIT(sizedwords/4));
	if (prsc) {
		struct fd_bo *bo = fd_resource(prsc)->bo;
		OUT_RELOC(ring, bo, offset,
				CP_LOAD_STATE4_1_STATE_TYPE(ST4_CONSTANTS), 0);
	} else {
		OUT_RING(ring, CP_LOAD_STATE4_1_EXT_SRC_ADDR(0) |
				CP_LOAD_STATE4_1_STATE_TYPE(ST4_CONSTANTS));
		OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
		dwords = (uint32_t *)&((uint8_t *)dwords)[offset];
	}
	for (i = 0; i < sz; i++) {
		OUT_RING(ring, dwords[i]);
	}
}

static void
fd5_emit_const_bo(struct fd_ringbuffer *ring, enum shader_t type, boolean write,
		uint32_t regid, uint32_t num, struct pipe_resource **prscs, uint32_t *offsets)
{
	uint32_t anum = align(num, 2);
	uint32_t i;

	debug_assert((regid % 4) == 0);

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (2 * anum));
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(regid/4) |
			CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
			CP_LOAD_STATE4_0_STATE_BLOCK(fd4_stage2shadersb(type)) |
			CP_LOAD_STATE4_0_NUM_UNIT(anum/2));
	OUT_RING(ring, CP_LOAD_STATE4_1_EXT_SRC_ADDR(0) |
			CP_LOAD_STATE4_1_STATE_TYPE(ST4_CONSTANTS));
	OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));

	for (i = 0; i < num; i++) {
		if (prscs[i]) {
			if (write) {
				OUT_RELOCW(ring, fd_resource(prscs[i])->bo, offsets[i], 0, 0);
			} else {
				OUT_RELOC(ring, fd_resource(prscs[i])->bo, offsets[i], 0, 0);
			}
		} else {
			OUT_RING(ring, 0xbad00000 | (i << 16));
			OUT_RING(ring, 0xbad00000 | (i << 16));
		}
	}

	for (; i < anum; i++) {
		OUT_RING(ring, 0xffffffff);
		OUT_RING(ring, 0xffffffff);
	}
}

/* Border color layout is diff from a4xx/a5xx.. if it turns out to be
 * the same as a6xx then move this somewhere common ;-)
 *
 * Entry layout looks like (total size, 0x60 bytes):
 */

struct PACKED bcolor_entry {
	uint32_t fp32[4];
	uint16_t ui16[4];
	int16_t  si16[4];
	uint16_t fp16[4];
	uint16_t rgb565;
	uint16_t rgb5a1;
	uint16_t rgba4;
	uint8_t __pad0[2];
	uint8_t  ui8[4];
	int8_t   si8[4];
	uint32_t rgb10a2;
	uint32_t z24; /* also s8? */
	uint8_t  __pad1[32];
};

#define FD5_BORDER_COLOR_SIZE        0x60
#define FD5_BORDER_COLOR_UPLOAD_SIZE (2 * PIPE_MAX_SAMPLERS * FD5_BORDER_COLOR_SIZE)

static void
setup_border_colors(struct fd_texture_stateobj *tex, struct bcolor_entry *entries)
{
	unsigned i, j;
	STATIC_ASSERT(sizeof(struct bcolor_entry) == FD5_BORDER_COLOR_SIZE);

	for (i = 0; i < tex->num_samplers; i++) {
		struct bcolor_entry *e = &entries[i];
		struct pipe_sampler_state *sampler = tex->samplers[i];
		union pipe_color_union *bc;

		if (!sampler)
			continue;

		bc = &sampler->border_color;

		/*
		 * XXX HACK ALERT XXX
		 *
		 * The border colors need to be swizzled in a particular
		 * format-dependent order. Even though samplers don't know about
		 * formats, we can assume that with a GL state tracker, there's a
		 * 1:1 correspondence between sampler and texture. Take advantage
		 * of that knowledge.
		 */
		if ((i >= tex->num_textures) || !tex->textures[i])
			continue;

		const struct util_format_description *desc =
				util_format_description(tex->textures[i]->format);

		e->rgb565 = 0;
		e->rgb5a1 = 0;
		e->rgba4 = 0;
		e->rgb10a2 = 0;
		e->z24 = 0;

		for (j = 0; j < 4; j++) {
			int c = desc->swizzle[j];

			if (c >= 4)
				continue;

			if (desc->channel[c].pure_integer) {
				uint16_t clamped;
				switch (desc->channel[c].size) {
				case 2:
					assert(desc->channel[c].type == UTIL_FORMAT_TYPE_UNSIGNED);
					clamped = CLAMP(bc->ui[j], 0, 0x3);
					break;
				case 8:
					if (desc->channel[c].type == UTIL_FORMAT_TYPE_SIGNED)
						clamped = CLAMP(bc->i[j], -128, 127);
					else
						clamped = CLAMP(bc->ui[j], 0, 255);
					break;
				case 10:
					assert(desc->channel[c].type == UTIL_FORMAT_TYPE_UNSIGNED);
					clamped = CLAMP(bc->ui[j], 0, 0x3ff);
					break;
				case 16:
					if (desc->channel[c].type == UTIL_FORMAT_TYPE_SIGNED)
						clamped = CLAMP(bc->i[j], -32768, 32767);
					else
						clamped = CLAMP(bc->ui[j], 0, 65535);
					break;
				default:
					assert(!"Unexpected bit size");
				case 32:
					clamped = 0;
					break;
				}
				e->fp32[c] = bc->ui[j];
				e->fp16[c] = clamped;
			} else {
				float f = bc->f[j];
				float f_u = CLAMP(f, 0, 1);
				float f_s = CLAMP(f, -1, 1);

				e->fp32[c] = fui(f);
				e->fp16[c] = util_float_to_half(f);
				e->ui16[c] = f_u * 0xffff;
				e->si16[c] = f_s * 0x7fff;
				e->ui8[c]  = f_u * 0xff;
				e->si8[c]  = f_s * 0x7f;
				if (c == 1)
					e->rgb565 |= (int)(f_u * 0x3f) << 5;
				else if (c < 3)
					e->rgb565 |= (int)(f_u * 0x1f) << (c ? 11 : 0);
				if (c == 3)
					e->rgb5a1 |= (f_u > 0.5) ? 0x8000 : 0;
				else
					e->rgb5a1 |= (int)(f_u * 0x1f) << (c * 5);
				if (c == 3)
					e->rgb10a2 |= (int)(f_u * 0x3) << 30;
				else
					e->rgb10a2 |= (int)(f_u * 0x3ff) << (c * 10);
				e->rgba4 |= (int)(f_u * 0xf) << (c * 4);
				if (c == 0)
					e->z24 = f_u * 0xffffff;
			}
		}

#ifdef DEBUG
		memset(&e->__pad0, 0, sizeof(e->__pad0));
		memset(&e->__pad1, 0, sizeof(e->__pad1));
#endif
	}
}

static void
emit_border_color(struct fd_context *ctx, struct fd_ringbuffer *ring)
{
	struct fd5_context *fd5_ctx = fd5_context(ctx);
	struct bcolor_entry *entries;
	unsigned off;
	void *ptr;

	STATIC_ASSERT(sizeof(struct bcolor_entry) == FD5_BORDER_COLOR_SIZE);

	u_upload_alloc(fd5_ctx->border_color_uploader,
			0, FD5_BORDER_COLOR_UPLOAD_SIZE,
			FD5_BORDER_COLOR_UPLOAD_SIZE, &off,
			&fd5_ctx->border_color_buf,
			&ptr);

	entries = ptr;

	setup_border_colors(&ctx->tex[PIPE_SHADER_VERTEX], &entries[0]);
	setup_border_colors(&ctx->tex[PIPE_SHADER_FRAGMENT],
			&entries[ctx->tex[PIPE_SHADER_VERTEX].num_samplers]);

	OUT_PKT4(ring, REG_A5XX_TPL1_TP_BORDER_COLOR_BASE_ADDR_LO, 2);
	OUT_RELOC(ring, fd_resource(fd5_ctx->border_color_buf)->bo, off, 0, 0);

	u_upload_unmap(fd5_ctx->border_color_uploader);
}

static bool
emit_textures(struct fd_context *ctx, struct fd_ringbuffer *ring,
		enum a4xx_state_block sb, struct fd_texture_stateobj *tex)
{
	bool needs_border = false;
	unsigned bcolor_offset = (sb == SB4_FS_TEX) ? ctx->tex[PIPE_SHADER_VERTEX].num_samplers : 0;
	unsigned i;

	if (tex->num_samplers > 0) {
		/* output sampler state: */
		OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (4 * tex->num_samplers));
		OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
				CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
				CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
				CP_LOAD_STATE4_0_NUM_UNIT(tex->num_samplers));
		OUT_RING(ring, CP_LOAD_STATE4_1_STATE_TYPE(ST4_SHADER) |
				CP_LOAD_STATE4_1_EXT_SRC_ADDR(0));
		OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
		for (i = 0; i < tex->num_samplers; i++) {
			static const struct fd5_sampler_stateobj dummy_sampler = {};
			const struct fd5_sampler_stateobj *sampler = tex->samplers[i] ?
					fd5_sampler_stateobj(tex->samplers[i]) :
					&dummy_sampler;
			OUT_RING(ring, sampler->texsamp0);
			OUT_RING(ring, sampler->texsamp1);
			OUT_RING(ring, sampler->texsamp2 |
					A5XX_TEX_SAMP_2_BCOLOR_OFFSET(bcolor_offset));
			OUT_RING(ring, sampler->texsamp3);

			needs_border |= sampler->needs_border;
		}
	}

	if (tex->num_textures > 0) {
		unsigned num_textures = tex->num_textures;

		/* emit texture state: */
		OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (12 * num_textures));
		OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
				CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
				CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
				CP_LOAD_STATE4_0_NUM_UNIT(num_textures));
		OUT_RING(ring, CP_LOAD_STATE4_1_STATE_TYPE(ST4_CONSTANTS) |
				CP_LOAD_STATE4_1_EXT_SRC_ADDR(0));
		OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
		for (i = 0; i < tex->num_textures; i++) {
			static const struct fd5_pipe_sampler_view dummy_view = {};
			const struct fd5_pipe_sampler_view *view = tex->textures[i] ?
					fd5_pipe_sampler_view(tex->textures[i]) :
					&dummy_view;

			OUT_RING(ring, view->texconst0);
			OUT_RING(ring, view->texconst1);
			OUT_RING(ring, view->texconst2);
			OUT_RING(ring, view->texconst3);
			if (view->base.texture) {
				struct fd_resource *rsc = fd_resource(view->base.texture);
				OUT_RELOC(ring, rsc->bo, view->offset,
						(uint64_t)view->texconst5 << 32, 0);
			} else {
				OUT_RING(ring, 0x00000000);
				OUT_RING(ring, view->texconst5);
			}
			OUT_RING(ring, view->texconst6);
			OUT_RING(ring, view->texconst7);
			OUT_RING(ring, view->texconst8);
			OUT_RING(ring, view->texconst9);
			OUT_RING(ring, view->texconst10);
			OUT_RING(ring, view->texconst11);
		}
	}

	return needs_border;
}

static void
emit_ssbos(struct fd_context *ctx, struct fd_ringbuffer *ring,
		enum a4xx_state_block sb, struct fd_shaderbuf_stateobj *so)
{
	unsigned count = util_last_bit(so->enabled_mask);

	if (count == 0)
		return;

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (4 * count));
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
			CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
			CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
			CP_LOAD_STATE4_0_NUM_UNIT(count));
	OUT_RING(ring, CP_LOAD_STATE4_1_STATE_TYPE(0) |
			CP_LOAD_STATE4_1_EXT_SRC_ADDR(0));
	OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
	for (unsigned i = 0; i < count; i++) {
		struct pipe_shader_buffer *buf = &so->sb[i];
		if (buf->buffer) {
			struct fd_resource *rsc = fd_resource(buf->buffer);
			OUT_RELOCW(ring, rsc->bo, 0, 0, 0);
		} else {
			OUT_RING(ring, 0x00000000);
			OUT_RING(ring, 0x00000000);
		}
		OUT_RING(ring, 0x00000000);
		OUT_RING(ring, 0x00000000);
	}

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (2 * count));
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
			CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
			CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
			CP_LOAD_STATE4_0_NUM_UNIT(count));
	OUT_RING(ring, CP_LOAD_STATE4_1_STATE_TYPE(1) |
			CP_LOAD_STATE4_1_EXT_SRC_ADDR(0));
	OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
	for (unsigned i = 0; i < count; i++) {
		struct pipe_shader_buffer *buf = &so->sb[i];

		// TODO maybe offset encoded somewhere here??
		OUT_RING(ring, (buf->buffer_size << 16));
		OUT_RING(ring, 0x00000000);
	}

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + (2 * count));
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
			CP_LOAD_STATE4_0_STATE_SRC(SS4_DIRECT) |
			CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
			CP_LOAD_STATE4_0_NUM_UNIT(count));
	OUT_RING(ring, CP_LOAD_STATE4_1_STATE_TYPE(2) |
			CP_LOAD_STATE4_1_EXT_SRC_ADDR(0));
	OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
	for (unsigned i = 0; i < count; i++) {
		struct pipe_shader_buffer *buf = &so->sb[i];
		if (buf->buffer) {
			struct fd_resource *rsc = fd_resource(buf->buffer);
			OUT_RELOCW(ring, rsc->bo, 0, 0, 0);
		} else {
			OUT_RING(ring, 0x00000000);
			OUT_RING(ring, 0x00000000);
		}
	}
}

void
fd5_emit_vertex_bufs(struct fd_ringbuffer *ring, struct fd5_emit *emit)
{
	int32_t i, j;
	const struct fd_vertex_state *vtx = emit->vtx;
	const struct ir3_shader_variant *vp = fd5_emit_get_vp(emit);

	for (i = 0, j = 0; i <= vp->inputs_count; i++) {
		if (vp->inputs[i].sysval)
			continue;
		if (vp->inputs[i].compmask) {
			struct pipe_vertex_element *elem = &vtx->vtx->pipe[i];
			const struct pipe_vertex_buffer *vb =
					&vtx->vertexbuf.vb[elem->vertex_buffer_index];
			struct fd_resource *rsc = fd_resource(vb->buffer.resource);
			enum pipe_format pfmt = elem->src_format;
			enum a5xx_vtx_fmt fmt = fd5_pipe2vtx(pfmt);
			bool isint = util_format_is_pure_integer(pfmt);
			uint32_t off = vb->buffer_offset + elem->src_offset;
			uint32_t size = fd_bo_size(rsc->bo) - off;
			debug_assert(fmt != ~0);

			OUT_PKT4(ring, REG_A5XX_VFD_FETCH(j), 4);
			OUT_RELOC(ring, rsc->bo, off, 0, 0);
			OUT_RING(ring, size);           /* VFD_FETCH[j].SIZE */
			OUT_RING(ring, vb->stride);     /* VFD_FETCH[j].STRIDE */

			OUT_PKT4(ring, REG_A5XX_VFD_DECODE(j), 2);
			OUT_RING(ring, A5XX_VFD_DECODE_INSTR_IDX(j) |
					A5XX_VFD_DECODE_INSTR_FORMAT(fmt) |
					COND(elem->instance_divisor, A5XX_VFD_DECODE_INSTR_INSTANCED) |
					A5XX_VFD_DECODE_INSTR_SWAP(fd5_pipe2swap(pfmt)) |
					A5XX_VFD_DECODE_INSTR_UNK30 |
					COND(!isint, A5XX_VFD_DECODE_INSTR_FLOAT));
			OUT_RING(ring, MAX2(1, elem->instance_divisor)); /* VFD_DECODE[j].STEP_RATE */

			OUT_PKT4(ring, REG_A5XX_VFD_DEST_CNTL(j), 1);
			OUT_RING(ring, A5XX_VFD_DEST_CNTL_INSTR_WRITEMASK(vp->inputs[i].compmask) |
					A5XX_VFD_DEST_CNTL_INSTR_REGID(vp->inputs[i].regid));

			j++;
		}
	}

	OUT_PKT4(ring, REG_A5XX_VFD_CONTROL_0, 1);
	OUT_RING(ring, A5XX_VFD_CONTROL_0_VTXCNT(j));
}

void
fd5_emit_state(struct fd_context *ctx, struct fd_ringbuffer *ring,
		struct fd5_emit *emit)
{
	struct pipe_framebuffer_state *pfb = &ctx->batch->framebuffer;
	const struct ir3_shader_variant *vp = fd5_emit_get_vp(emit);
	const struct ir3_shader_variant *fp = fd5_emit_get_fp(emit);
	const enum fd_dirty_3d_state dirty = emit->dirty;
	bool needs_border = false;

	emit_marker5(ring, 5);

	if ((dirty & FD_DIRTY_FRAMEBUFFER) && !emit->key.binning_pass) {
		unsigned char mrt_comp[A5XX_MAX_RENDER_TARGETS] = {0};

		for (unsigned i = 0; i < A5XX_MAX_RENDER_TARGETS; i++) {
			mrt_comp[i] = ((i < pfb->nr_cbufs) && pfb->cbufs[i]) ? 0xf : 0;
		}

		OUT_PKT4(ring, REG_A5XX_RB_RENDER_COMPONENTS, 1);
		OUT_RING(ring, A5XX_RB_RENDER_COMPONENTS_RT0(mrt_comp[0]) |
				A5XX_RB_RENDER_COMPONENTS_RT1(mrt_comp[1]) |
				A5XX_RB_RENDER_COMPONENTS_RT2(mrt_comp[2]) |
				A5XX_RB_RENDER_COMPONENTS_RT3(mrt_comp[3]) |
				A5XX_RB_RENDER_COMPONENTS_RT4(mrt_comp[4]) |
				A5XX_RB_RENDER_COMPONENTS_RT5(mrt_comp[5]) |
				A5XX_RB_RENDER_COMPONENTS_RT6(mrt_comp[6]) |
				A5XX_RB_RENDER_COMPONENTS_RT7(mrt_comp[7]));
	}

	if (dirty & (FD_DIRTY_ZSA | FD_DIRTY_FRAMEBUFFER)) {
		struct fd5_zsa_stateobj *zsa = fd5_zsa_stateobj(ctx->zsa);
		uint32_t rb_alpha_control = zsa->rb_alpha_control;

		if (util_format_is_pure_integer(pipe_surface_format(pfb->cbufs[0])))
			rb_alpha_control &= ~A5XX_RB_ALPHA_CONTROL_ALPHA_TEST;

		OUT_PKT4(ring, REG_A5XX_RB_ALPHA_CONTROL, 1);
		OUT_RING(ring, rb_alpha_control);

		OUT_PKT4(ring, REG_A5XX_RB_STENCIL_CONTROL, 1);
		OUT_RING(ring, zsa->rb_stencil_control);
	}

	if (dirty & (FD_DIRTY_ZSA | FD_DIRTY_BLEND | FD_DIRTY_PROG)) {
		struct fd5_blend_stateobj *blend = fd5_blend_stateobj(ctx->blend);
		struct fd5_zsa_stateobj *zsa = fd5_zsa_stateobj(ctx->zsa);

		if (pfb->zsbuf) {
			struct fd_resource *rsc = fd_resource(pfb->zsbuf->texture);
			uint32_t gras_lrz_cntl = zsa->gras_lrz_cntl;

			if (emit->no_lrz_write || !rsc->lrz || !rsc->lrz_valid)
				gras_lrz_cntl = 0;
			else if (emit->key.binning_pass && blend->lrz_write && zsa->lrz_write)
				gras_lrz_cntl |= A5XX_GRAS_LRZ_CNTL_LRZ_WRITE;

			OUT_PKT4(ring, REG_A5XX_GRAS_LRZ_CNTL, 1);
			OUT_RING(ring, gras_lrz_cntl);
		}
	}

	if (dirty & (FD_DIRTY_ZSA | FD_DIRTY_STENCIL_REF)) {
		struct fd5_zsa_stateobj *zsa = fd5_zsa_stateobj(ctx->zsa);
		struct pipe_stencil_ref *sr = &ctx->stencil_ref;

		OUT_PKT4(ring, REG_A5XX_RB_STENCILREFMASK, 2);
		OUT_RING(ring, zsa->rb_stencilrefmask |
				A5XX_RB_STENCILREFMASK_STENCILREF(sr->ref_value[0]));
		OUT_RING(ring, zsa->rb_stencilrefmask_bf |
				A5XX_RB_STENCILREFMASK_BF_STENCILREF(sr->ref_value[1]));
	}

	if (dirty & (FD_DIRTY_ZSA | FD_DIRTY_RASTERIZER | FD_DIRTY_PROG)) {
		struct fd5_zsa_stateobj *zsa = fd5_zsa_stateobj(ctx->zsa);
		bool fragz = fp->has_kill | fp->writes_pos;

		OUT_PKT4(ring, REG_A5XX_RB_DEPTH_CNTL, 1);
		OUT_RING(ring, zsa->rb_depth_cntl);

		OUT_PKT4(ring, REG_A5XX_RB_DEPTH_PLANE_CNTL, 1);
		OUT_RING(ring, COND(fragz, A5XX_RB_DEPTH_PLANE_CNTL_FRAG_WRITES_Z) |
				COND(fragz && fp->frag_coord, A5XX_RB_DEPTH_PLANE_CNTL_UNK1));

		OUT_PKT4(ring, REG_A5XX_GRAS_SU_DEPTH_PLANE_CNTL, 1);
		OUT_RING(ring, COND(fragz, A5XX_GRAS_SU_DEPTH_PLANE_CNTL_FRAG_WRITES_Z) |
				COND(fragz && fp->frag_coord, A5XX_GRAS_SU_DEPTH_PLANE_CNTL_UNK1));
	}

	if (dirty & FD_DIRTY_SCISSOR) {
		struct pipe_scissor_state *scissor = fd_context_get_scissor(ctx);

		OUT_PKT4(ring, REG_A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0, 2);
		OUT_RING(ring, A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0_X(scissor->minx) |
				A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0_Y(scissor->miny));
		OUT_RING(ring, A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0_X(scissor->maxx - 1) |
				A5XX_GRAS_SC_SCREEN_SCISSOR_TL_0_Y(scissor->maxy - 1));

		OUT_PKT4(ring, REG_A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0, 2);
		OUT_RING(ring, A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0_X(scissor->minx) |
				A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0_Y(scissor->miny));
		OUT_RING(ring, A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0_X(scissor->maxx - 1) |
				A5XX_GRAS_SC_VIEWPORT_SCISSOR_TL_0_Y(scissor->maxy - 1));

		ctx->batch->max_scissor.minx = MIN2(ctx->batch->max_scissor.minx, scissor->minx);
		ctx->batch->max_scissor.miny = MIN2(ctx->batch->max_scissor.miny, scissor->miny);
		ctx->batch->max_scissor.maxx = MAX2(ctx->batch->max_scissor.maxx, scissor->maxx);
		ctx->batch->max_scissor.maxy = MAX2(ctx->batch->max_scissor.maxy, scissor->maxy);
	}

	if (dirty & FD_DIRTY_VIEWPORT) {
		fd_wfi(ctx->batch, ring);
		OUT_PKT4(ring, REG_A5XX_GRAS_CL_VPORT_XOFFSET_0, 6);
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_XOFFSET_0(ctx->viewport.translate[0]));
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_XSCALE_0(ctx->viewport.scale[0]));
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_YOFFSET_0(ctx->viewport.translate[1]));
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_YSCALE_0(ctx->viewport.scale[1]));
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_ZOFFSET_0(ctx->viewport.translate[2]));
		OUT_RING(ring, A5XX_GRAS_CL_VPORT_ZSCALE_0(ctx->viewport.scale[2]));
	}

	if (dirty & FD_DIRTY_PROG)
		fd5_program_emit(ctx, ring, emit);

	if (dirty & FD_DIRTY_RASTERIZER) {
		struct fd5_rasterizer_stateobj *rasterizer =
				fd5_rasterizer_stateobj(ctx->rasterizer);

		OUT_PKT4(ring, REG_A5XX_GRAS_SU_CNTL, 1);
		OUT_RING(ring, rasterizer->gras_su_cntl);

		OUT_PKT4(ring, REG_A5XX_GRAS_SU_POINT_MINMAX, 2);
		OUT_RING(ring, rasterizer->gras_su_point_minmax);
		OUT_RING(ring, rasterizer->gras_su_point_size);

		OUT_PKT4(ring, REG_A5XX_GRAS_SU_POLY_OFFSET_SCALE, 3);
		OUT_RING(ring, rasterizer->gras_su_poly_offset_scale);
		OUT_RING(ring, rasterizer->gras_su_poly_offset_offset);
		OUT_RING(ring, rasterizer->gras_su_poly_offset_clamp);

		OUT_PKT4(ring, REG_A5XX_PC_RASTER_CNTL, 1);
		OUT_RING(ring, rasterizer->pc_raster_cntl);

		OUT_PKT4(ring, REG_A5XX_GRAS_CL_CNTL, 1);
		OUT_RING(ring, rasterizer->gras_cl_clip_cntl);
	}

	/* note: must come after program emit.. because there is some overlap
	 * in registers, ex. PC_PRIMITIVE_CNTL and we rely on some cached
	 * values from fd5_program_emit() to avoid having to re-emit the prog
	 * every time rast state changes.
	 *
	 * Since the primitive restart state is not part of a tracked object, we
	 * re-emit this register every time.
	 */
	if (emit->info && ctx->rasterizer) {
		struct fd5_rasterizer_stateobj *rasterizer =
				fd5_rasterizer_stateobj(ctx->rasterizer);
		unsigned max_loc = fd5_context(ctx)->max_loc;

		OUT_PKT4(ring, REG_A5XX_PC_PRIMITIVE_CNTL, 1);
		OUT_RING(ring, rasterizer->pc_primitive_cntl |
				 A5XX_PC_PRIMITIVE_CNTL_STRIDE_IN_VPC(max_loc) |
				 COND(emit->info->primitive_restart && emit->info->index_size,
					  A5XX_PC_PRIMITIVE_CNTL_PRIMITIVE_RESTART));
	}

	if (dirty & (FD_DIRTY_FRAMEBUFFER | FD_DIRTY_RASTERIZER | FD_DIRTY_PROG)) {
		uint32_t posz_regid = ir3_find_output_regid(fp, FRAG_RESULT_DEPTH);
		unsigned nr = pfb->nr_cbufs;

		if (emit->key.binning_pass)
			nr = 0;
		else if (ctx->rasterizer->rasterizer_discard)
			nr = 0;

		OUT_PKT4(ring, REG_A5XX_RB_FS_OUTPUT_CNTL, 1);
		OUT_RING(ring, A5XX_RB_FS_OUTPUT_CNTL_MRT(nr) |
				COND(fp->writes_pos, A5XX_RB_FS_OUTPUT_CNTL_FRAG_WRITES_Z));

		OUT_PKT4(ring, REG_A5XX_SP_FS_OUTPUT_CNTL, 1);
		OUT_RING(ring, A5XX_SP_FS_OUTPUT_CNTL_MRT(nr) |
				A5XX_SP_FS_OUTPUT_CNTL_DEPTH_REGID(posz_regid) |
				A5XX_SP_FS_OUTPUT_CNTL_SAMPLEMASK_REGID(regid(63, 0)));
	}

	if (emit->prog == &ctx->prog) { /* evil hack to deal sanely with clear path */
		ir3_emit_vs_consts(vp, ring, ctx, emit->info);
		if (!emit->key.binning_pass)
			ir3_emit_fs_consts(fp, ring, ctx);

		struct pipe_stream_output_info *info = &vp->shader->stream_output;
		if (info->num_outputs) {
			struct fd_streamout_stateobj *so = &ctx->streamout;

			for (unsigned i = 0; i < so->num_targets; i++) {
				struct pipe_stream_output_target *target = so->targets[i];

				if (!target)
					continue;

				unsigned offset = (so->offsets[i] * info->stride[i] * 4) +
						target->buffer_offset;

				OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_BASE_LO(i), 3);
				/* VPC_SO[i].BUFFER_BASE_LO: */
				OUT_RELOCW(ring, fd_resource(target->buffer)->bo, 0, 0, 0);
				OUT_RING(ring, target->buffer_size + offset);

				OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_OFFSET(i), 3);
				OUT_RING(ring, offset);
				/* VPC_SO[i].FLUSH_BASE_LO/HI: */
				// TODO just give hw a dummy addr for now.. we should
				// be using this an then CP_MEM_TO_REG to set the
				// VPC_SO[i].BUFFER_OFFSET for the next draw..
				OUT_RELOCW(ring, fd5_context(ctx)->blit_mem, 0x100, 0, 0);

				emit->streamout_mask |= (1 << i);
			}
		}
	}

	if ((dirty & FD_DIRTY_BLEND)) {
		struct fd5_blend_stateobj *blend = fd5_blend_stateobj(ctx->blend);
		uint32_t i;

		for (i = 0; i < A5XX_MAX_RENDER_TARGETS; i++) {
			enum pipe_format format = pipe_surface_format(pfb->cbufs[i]);
			bool is_int = util_format_is_pure_integer(format);
			bool has_alpha = util_format_has_alpha(format);
			uint32_t control = blend->rb_mrt[i].control;
			uint32_t blend_control = blend->rb_mrt[i].blend_control_alpha;

			if (is_int) {
				control &= A5XX_RB_MRT_CONTROL_COMPONENT_ENABLE__MASK;
				control |= A5XX_RB_MRT_CONTROL_ROP_CODE(ROP_COPY);
			}

			if (has_alpha) {
				blend_control |= blend->rb_mrt[i].blend_control_rgb;
			} else {
				blend_control |= blend->rb_mrt[i].blend_control_no_alpha_rgb;
				control &= ~A5XX_RB_MRT_CONTROL_BLEND2;
			}

			OUT_PKT4(ring, REG_A5XX_RB_MRT_CONTROL(i), 1);
			OUT_RING(ring, control);

			OUT_PKT4(ring, REG_A5XX_RB_MRT_BLEND_CONTROL(i), 1);
			OUT_RING(ring, blend_control);
		}

		OUT_PKT4(ring, REG_A5XX_RB_BLEND_CNTL, 1);
		OUT_RING(ring, blend->rb_blend_cntl |
				A5XX_RB_BLEND_CNTL_SAMPLE_MASK(0xffff));

		OUT_PKT4(ring, REG_A5XX_SP_BLEND_CNTL, 1);
		OUT_RING(ring, blend->sp_blend_cntl);
	}

	if (dirty & FD_DIRTY_BLEND_COLOR) {
		struct pipe_blend_color *bcolor = &ctx->blend_color;

		OUT_PKT4(ring, REG_A5XX_RB_BLEND_RED, 8);
		OUT_RING(ring, A5XX_RB_BLEND_RED_FLOAT(bcolor->color[0]) |
				A5XX_RB_BLEND_RED_UINT(bcolor->color[0] * 0xff) |
				A5XX_RB_BLEND_RED_SINT(bcolor->color[0] * 0x7f));
		OUT_RING(ring, A5XX_RB_BLEND_RED_F32(bcolor->color[0]));
		OUT_RING(ring, A5XX_RB_BLEND_GREEN_FLOAT(bcolor->color[1]) |
				A5XX_RB_BLEND_GREEN_UINT(bcolor->color[1] * 0xff) |
				A5XX_RB_BLEND_GREEN_SINT(bcolor->color[1] * 0x7f));
		OUT_RING(ring, A5XX_RB_BLEND_RED_F32(bcolor->color[1]));
		OUT_RING(ring, A5XX_RB_BLEND_BLUE_FLOAT(bcolor->color[2]) |
				A5XX_RB_BLEND_BLUE_UINT(bcolor->color[2] * 0xff) |
				A5XX_RB_BLEND_BLUE_SINT(bcolor->color[2] * 0x7f));
		OUT_RING(ring, A5XX_RB_BLEND_BLUE_F32(bcolor->color[2]));
		OUT_RING(ring, A5XX_RB_BLEND_ALPHA_FLOAT(bcolor->color[3]) |
				A5XX_RB_BLEND_ALPHA_UINT(bcolor->color[3] * 0xff) |
				A5XX_RB_BLEND_ALPHA_SINT(bcolor->color[3] * 0x7f));
		OUT_RING(ring, A5XX_RB_BLEND_ALPHA_F32(bcolor->color[3]));
	}

	if (ctx->dirty_shader[PIPE_SHADER_VERTEX] & FD_DIRTY_SHADER_TEX) {
		needs_border |= emit_textures(ctx, ring, SB4_VS_TEX,
				&ctx->tex[PIPE_SHADER_VERTEX]);
		OUT_PKT4(ring, REG_A5XX_TPL1_VS_TEX_COUNT, 1);
		OUT_RING(ring, ctx->tex[PIPE_SHADER_VERTEX].num_textures);
	}

	if (ctx->dirty_shader[PIPE_SHADER_FRAGMENT] & FD_DIRTY_SHADER_TEX) {
		needs_border |= emit_textures(ctx, ring, SB4_FS_TEX,
				&ctx->tex[PIPE_SHADER_FRAGMENT]);
		OUT_PKT4(ring, REG_A5XX_TPL1_FS_TEX_COUNT, 1);
		OUT_RING(ring, ctx->tex[PIPE_SHADER_FRAGMENT].num_textures);
	}

	OUT_PKT4(ring, REG_A5XX_TPL1_CS_TEX_COUNT, 1);
	OUT_RING(ring, 0);

	if (needs_border)
		emit_border_color(ctx, ring);

	if (ctx->dirty_shader[PIPE_SHADER_FRAGMENT] & FD_DIRTY_SHADER_SSBO)
		emit_ssbos(ctx, ring, SB4_SSBO, &ctx->shaderbuf[PIPE_SHADER_FRAGMENT]);
}

void
fd5_emit_cs_state(struct fd_context *ctx, struct fd_ringbuffer *ring,
		struct ir3_shader_variant *cp)
{
	enum fd_dirty_shader_state dirty = ctx->dirty_shader[PIPE_SHADER_COMPUTE];

	if (dirty & FD_DIRTY_SHADER_TEX) {
		bool needs_border = false;
		needs_border |= emit_textures(ctx, ring, SB4_CS_TEX,
				&ctx->tex[PIPE_SHADER_COMPUTE]);

		if (needs_border)
			emit_border_color(ctx, ring);

		OUT_PKT4(ring, REG_A5XX_TPL1_VS_TEX_COUNT, 1);
		OUT_RING(ring, 0);

		OUT_PKT4(ring, REG_A5XX_TPL1_HS_TEX_COUNT, 1);
		OUT_RING(ring, 0);

		OUT_PKT4(ring, REG_A5XX_TPL1_DS_TEX_COUNT, 1);
		OUT_RING(ring, 0);

		OUT_PKT4(ring, REG_A5XX_TPL1_GS_TEX_COUNT, 1);
		OUT_RING(ring, 0);

		OUT_PKT4(ring, REG_A5XX_TPL1_FS_TEX_COUNT, 1);
		OUT_RING(ring, 0);

		OUT_PKT4(ring, REG_A5XX_TPL1_CS_TEX_COUNT, 1);
		OUT_RING(ring, ctx->tex[PIPE_SHADER_COMPUTE].num_textures);
	}

	if (dirty & FD_DIRTY_SHADER_SSBO)
		emit_ssbos(ctx, ring, SB4_CS_SSBO, &ctx->shaderbuf[PIPE_SHADER_COMPUTE]);
}

/* emit setup at begin of new cmdstream buffer (don't rely on previous
 * state, there could have been a context switch between ioctls):
 */
void
fd5_emit_restore(struct fd_batch *batch, struct fd_ringbuffer *ring)
{
	struct fd_context *ctx = batch->ctx;

	fd5_set_render_mode(ctx, ring, BYPASS);
	fd5_cache_flush(batch, ring);

	OUT_PKT4(ring, REG_A5XX_HLSQ_UPDATE_CNTL, 1);
	OUT_RING(ring, 0xfffff);

/*
t7              opcode: CP_PERFCOUNTER_ACTION (50) (4 dwords)
0000000500024048:               70d08003 00000000 001c5000 00000005
t7              opcode: CP_PERFCOUNTER_ACTION (50) (4 dwords)
0000000500024058:               70d08003 00000010 001c7000 00000005

t7              opcode: CP_WAIT_FOR_IDLE (26) (1 dwords)
0000000500024068:               70268000
*/

	OUT_PKT4(ring, REG_A5XX_PC_RESTART_INDEX, 1);
	OUT_RING(ring, 0xffffffff);

	OUT_PKT4(ring, REG_A5XX_PC_RASTER_CNTL, 1);
	OUT_RING(ring, 0x00000012);

	OUT_PKT4(ring, REG_A5XX_GRAS_SU_POINT_MINMAX, 2);
	OUT_RING(ring, A5XX_GRAS_SU_POINT_MINMAX_MIN(1.0) |
			A5XX_GRAS_SU_POINT_MINMAX_MAX(4092.0));
	OUT_RING(ring, A5XX_GRAS_SU_POINT_SIZE(0.5));

	OUT_PKT4(ring, REG_A5XX_GRAS_SU_CONSERVATIVE_RAS_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* GRAS_SU_CONSERVATIVE_RAS_CNTL */

	OUT_PKT4(ring, REG_A5XX_GRAS_SC_SCREEN_SCISSOR_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* GRAS_SC_SCREEN_SCISSOR_CNTL */

	OUT_PKT4(ring, REG_A5XX_SP_VS_CONFIG_MAX_CONST, 1);
	OUT_RING(ring, 0);            /* SP_VS_CONFIG_MAX_CONST */

	OUT_PKT4(ring, REG_A5XX_SP_FS_CONFIG_MAX_CONST, 1);
	OUT_RING(ring, 0);            /* SP_FS_CONFIG_MAX_CONST */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E292, 2);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E292 */
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E293 */

	OUT_PKT4(ring, REG_A5XX_RB_MODE_CNTL, 1);
	OUT_RING(ring, 0x00000044);   /* RB_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_RB_DBG_ECO_CNTL, 1);
	OUT_RING(ring, 0x00100000);   /* RB_DBG_ECO_CNTL */

	OUT_PKT4(ring, REG_A5XX_VFD_MODE_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* VFD_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_PC_MODE_CNTL, 1);
	OUT_RING(ring, 0x0000001f);   /* PC_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_SP_MODE_CNTL, 1);
	OUT_RING(ring, 0x0000001e);   /* SP_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_SP_DBG_ECO_CNTL, 1);
	OUT_RING(ring, 0x40000800);   /* SP_DBG_ECO_CNTL */

	OUT_PKT4(ring, REG_A5XX_TPL1_MODE_CNTL, 1);
	OUT_RING(ring, 0x00000544);   /* TPL1_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_HLSQ_TIMEOUT_THRESHOLD_0, 2);
	OUT_RING(ring, 0x00000080);   /* HLSQ_TIMEOUT_THRESHOLD_0 */
	OUT_RING(ring, 0x00000000);   /* HLSQ_TIMEOUT_THRESHOLD_1 */

	OUT_PKT4(ring, REG_A5XX_VPC_DBG_ECO_CNTL, 1);
	OUT_RING(ring, 0x00000400);   /* VPC_DBG_ECO_CNTL */

	OUT_PKT4(ring, REG_A5XX_HLSQ_MODE_CNTL, 1);
	OUT_RING(ring, 0x00000001);   /* HLSQ_MODE_CNTL */

	OUT_PKT4(ring, REG_A5XX_VPC_MODE_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* VPC_MODE_CNTL */

	/* we don't use this yet.. probably best to disable.. */
	OUT_PKT7(ring, CP_SET_DRAW_STATE, 3);
	OUT_RING(ring, CP_SET_DRAW_STATE__0_COUNT(0) |
			CP_SET_DRAW_STATE__0_DISABLE_ALL_GROUPS |
			CP_SET_DRAW_STATE__0_GROUP_ID(0));
	OUT_RING(ring, CP_SET_DRAW_STATE__1_ADDR_LO(0));
	OUT_RING(ring, CP_SET_DRAW_STATE__2_ADDR_HI(0));

	OUT_PKT4(ring, REG_A5XX_GRAS_SU_CONSERVATIVE_RAS_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* GRAS_SU_CONSERVATIVE_RAS_CNTL */

	OUT_PKT4(ring, REG_A5XX_GRAS_SC_BIN_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* GRAS_SC_BIN_CNTL */

	OUT_PKT4(ring, REG_A5XX_GRAS_SC_BIN_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* GRAS_SC_BIN_CNTL */

	OUT_PKT4(ring, REG_A5XX_VPC_FS_PRIMITIVEID_CNTL, 1);
	OUT_RING(ring, 0x000000ff);   /* VPC_FS_PRIMITIVEID_CNTL */

	OUT_PKT4(ring, REG_A5XX_VPC_SO_OVERRIDE, 1);
	OUT_RING(ring, A5XX_VPC_SO_OVERRIDE_SO_DISABLE);

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_BASE_LO(0), 3);
	OUT_RING(ring, 0x00000000);   /* VPC_SO_BUFFER_BASE_LO_0 */
	OUT_RING(ring, 0x00000000);   /* VPC_SO_BUFFER_BASE_HI_0 */
	OUT_RING(ring, 0x00000000);   /* VPC_SO_BUFFER_SIZE_0 */

	OUT_PKT4(ring, REG_A5XX_VPC_SO_FLUSH_BASE_LO(0), 2);
	OUT_RING(ring, 0x00000000);   /* VPC_SO_FLUSH_BASE_LO_0 */
	OUT_RING(ring, 0x00000000);   /* VPC_SO_FLUSH_BASE_HI_0 */

	OUT_PKT4(ring, REG_A5XX_PC_GS_PARAM, 1);
	OUT_RING(ring, 0x00000000);   /* PC_GS_PARAM */

	OUT_PKT4(ring, REG_A5XX_PC_HS_PARAM, 1);
	OUT_RING(ring, 0x00000000);   /* PC_HS_PARAM */

	OUT_PKT4(ring, REG_A5XX_TPL1_TP_FS_ROTATION_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* TPL1_TP_FS_ROTATION_CNTL */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E001, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E001 */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E004, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E004 */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E093, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E093 */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E29A, 1);
	OUT_RING(ring, 0x00ffff00);   /* UNKNOWN_E29A */

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUF_CNTL, 1);
	OUT_RING(ring, 0x00000000);   /* VPC_SO_BUF_CNTL */

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_OFFSET(0), 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E2AB */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E389, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E389 */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E38D, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E38D */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E5AB, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E5AB */

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E5C2, 1);
	OUT_RING(ring, 0x00000000);   /* UNKNOWN_E5C2 */

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_BASE_LO(1), 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_OFFSET(1), 6);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_OFFSET(2), 6);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_VPC_SO_BUFFER_OFFSET(3), 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E5DB, 1);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E600, 1);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E640, 1);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_TPL1_VS_TEX_COUNT, 4);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_TPL1_FS_TEX_COUNT, 2);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7C0, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7C5, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7CA, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7CF, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7D4, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_UNKNOWN_E7D9, 3);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_RB_CLEAR_CNTL, 1);
	OUT_RING(ring, 0x00000000);
}

static void
fd5_emit_ib(struct fd_ringbuffer *ring, struct fd_ringbuffer *target)
{
	__OUT_IB5(ring, target);
}

void
fd5_emit_init(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->emit_const = fd5_emit_const;
	ctx->emit_const_bo = fd5_emit_const_bo;
	ctx->emit_ib = fd5_emit_ib;
}
