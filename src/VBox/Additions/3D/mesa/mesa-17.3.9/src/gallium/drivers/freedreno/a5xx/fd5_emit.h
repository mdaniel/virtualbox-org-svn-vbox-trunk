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

#ifndef FD5_EMIT_H
#define FD5_EMIT_H

#include "pipe/p_context.h"

#include "freedreno_context.h"
#include "fd5_context.h"
#include "fd5_format.h"
#include "fd5_program.h"
#include "ir3_shader.h"

struct fd_ringbuffer;

/* grouped together emit-state for prog/vertex/state emit: */
struct fd5_emit {
	struct pipe_debug_callback *debug;
	const struct fd_vertex_state *vtx;
	const struct fd_program_stateobj *prog;
	const struct pipe_draw_info *info;
	struct ir3_shader_key key;
	enum fd_dirty_3d_state dirty;

	uint32_t sprite_coord_enable;  /* bitmask */
	bool sprite_coord_mode;
	bool rasterflat;
	bool no_decode_srgb;

	/* in binning pass, we don't have real frag shader, so we
	 * don't know if real draw disqualifies lrz write.  So just
	 * figure that out up-front and stash it in the emit.
	 */
	bool no_lrz_write;

	/* cached to avoid repeated lookups of same variants: */
	const struct ir3_shader_variant *vp, *fp;
	/* TODO: other shader stages.. */

	unsigned streamout_mask;
};

static inline enum a5xx_color_fmt fd5_emit_format(struct pipe_surface *surf)
{
	if (!surf)
		return 0;
	return fd5_pipe2color(surf->format);
}

static inline const struct ir3_shader_variant *
fd5_emit_get_vp(struct fd5_emit *emit)
{
	if (!emit->vp) {
		struct fd5_shader_stateobj *so = emit->prog->vp;
		emit->vp = ir3_shader_variant(so->shader, emit->key, emit->debug);
	}
	return emit->vp;
}

static inline const struct ir3_shader_variant *
fd5_emit_get_fp(struct fd5_emit *emit)
{
	if (!emit->fp) {
		if (emit->key.binning_pass) {
			/* use dummy stateobj to simplify binning vs non-binning: */
			static const struct ir3_shader_variant binning_fp = {};
			emit->fp = &binning_fp;
		} else {
			struct fd5_shader_stateobj *so = emit->prog->fp;
			emit->fp = ir3_shader_variant(so->shader, emit->key, emit->debug);
		}
	}
	return emit->fp;
}

static inline void
fd5_cache_flush(struct fd_batch *batch, struct fd_ringbuffer *ring)
{
	fd_reset_wfi(batch);
	OUT_PKT4(ring, REG_A5XX_UCHE_CACHE_INVALIDATE_MIN_LO, 5);
	OUT_RING(ring, 0x00000000);   /* UCHE_CACHE_INVALIDATE_MIN_LO */
	OUT_RING(ring, 0x00000000);   /* UCHE_CACHE_INVALIDATE_MIN_HI */
	OUT_RING(ring, 0x00000000);   /* UCHE_CACHE_INVALIDATE_MAX_LO */
	OUT_RING(ring, 0x00000000);   /* UCHE_CACHE_INVALIDATE_MAX_HI */
	OUT_RING(ring, 0x00000012);   /* UCHE_CACHE_INVALIDATE */
	fd_wfi(batch, ring);
}

static inline void
fd5_set_render_mode(struct fd_context *ctx, struct fd_ringbuffer *ring,
		enum render_mode_cmd mode)
{
	/* TODO add preemption support, gmem bypass, etc */
	emit_marker5(ring, 7);
	OUT_PKT7(ring, CP_SET_RENDER_MODE, 5);
	OUT_RING(ring, CP_SET_RENDER_MODE_0_MODE(mode));
	OUT_RING(ring, 0x00000000);   /* ADDR_LO */
	OUT_RING(ring, 0x00000000);   /* ADDR_HI */
	OUT_RING(ring, COND(mode == GMEM, CP_SET_RENDER_MODE_3_GMEM_ENABLE) |
			COND(mode == BINNING, CP_SET_RENDER_MODE_3_VSC_ENABLE));
	OUT_RING(ring, 0x00000000);
	emit_marker5(ring, 7);
}

static inline void
fd5_emit_blit(struct fd_context *ctx, struct fd_ringbuffer *ring)
{
	struct fd5_context *fd5_ctx = fd5_context(ctx);

	emit_marker5(ring, 7);

	OUT_PKT7(ring, CP_EVENT_WRITE, 4);
	OUT_RING(ring, CP_EVENT_WRITE_0_EVENT(BLIT));
	OUT_RELOCW(ring, fd5_ctx->blit_mem, 0, 0, 0);  /* ADDR_LO/HI */
	OUT_RING(ring, 0x00000000);

	emit_marker5(ring, 7);
}

static inline void
fd5_emit_render_cntl(struct fd_context *ctx, bool blit, bool binning)
{
	struct fd_ringbuffer *ring = binning ? ctx->batch->binning : ctx->batch->draw;

	/* TODO eventually this partially depends on the pfb state, ie.
	 * which of the cbuf(s)/zsbuf has an UBWC flag buffer.. that part
	 * we could probably cache and just regenerate if framebuffer
	 * state is dirty (or something like that)..
	 *
	 * Other bits seem to depend on query state, like if samples-passed
	 * query is active.
	 */
	bool samples_passed = (fd5_context(ctx)->samples_passed_queries > 0);
	OUT_PKT4(ring, REG_A5XX_RB_RENDER_CNTL, 1);
	OUT_RING(ring, 0x00000000 |   /* RB_RENDER_CNTL */
			COND(binning, A5XX_RB_RENDER_CNTL_BINNING_PASS) |
			COND(binning, A5XX_RB_RENDER_CNTL_DISABLE_COLOR_PIPE) |
			COND(samples_passed, A5XX_RB_RENDER_CNTL_SAMPLES_PASSED) |
			COND(!blit, 0x8));

	OUT_PKT4(ring, REG_A5XX_GRAS_SC_CNTL, 1);
	OUT_RING(ring, 0x00000008 |   /* GRAS_SC_CNTL */
			COND(binning, A5XX_GRAS_SC_CNTL_BINNING_PASS) |
			COND(samples_passed, A5XX_GRAS_SC_CNTL_SAMPLES_PASSED));
}

static inline void
fd5_emit_lrz_flush(struct fd_ringbuffer *ring)
{
	/* TODO I think the extra writes to GRAS_LRZ_CNTL are probably
	 * a workaround and not needed on all a5xx.
	 */
	OUT_PKT4(ring, REG_A5XX_GRAS_LRZ_CNTL, 1);
	OUT_RING(ring, A5XX_GRAS_LRZ_CNTL_ENABLE);

	OUT_PKT7(ring, CP_EVENT_WRITE, 1);
	OUT_RING(ring, LRZ_FLUSH);

	OUT_PKT4(ring, REG_A5XX_GRAS_LRZ_CNTL, 1);
	OUT_RING(ring, 0x0);
}

void fd5_emit_vertex_bufs(struct fd_ringbuffer *ring, struct fd5_emit *emit);

void fd5_emit_state(struct fd_context *ctx, struct fd_ringbuffer *ring,
		struct fd5_emit *emit);

void fd5_emit_cs_state(struct fd_context *ctx, struct fd_ringbuffer *ring,
		struct ir3_shader_variant *cp);

void fd5_emit_restore(struct fd_batch *batch, struct fd_ringbuffer *ring);

void fd5_emit_init(struct pipe_context *pctx);

#endif /* FD5_EMIT_H */
