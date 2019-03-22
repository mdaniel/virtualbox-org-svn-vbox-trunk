/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2014 Rob Clark <robclark@freedesktop.org>
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
#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "freedreno_query_hw.h"
#include "freedreno_context.h"
#include "freedreno_resource.h"
#include "freedreno_util.h"

struct fd_hw_sample_period {
	struct fd_hw_sample *start, *end;
	struct list_head list;
};

static struct fd_hw_sample *
get_sample(struct fd_batch *batch, struct fd_ringbuffer *ring,
		unsigned query_type)
{
	struct fd_context *ctx = batch->ctx;
	struct fd_hw_sample *samp = NULL;
	int idx = pidx(query_type);

	assume(idx >= 0);   /* query never would have been created otherwise */

	if (!batch->sample_cache[idx]) {
		struct fd_hw_sample *new_samp =
			ctx->hw_sample_providers[idx]->get_sample(batch, ring);
		fd_hw_sample_reference(ctx, &batch->sample_cache[idx], new_samp);
		util_dynarray_append(&batch->samples, struct fd_hw_sample *, new_samp);
		batch->needs_flush = true;
	}

	fd_hw_sample_reference(ctx, &samp, batch->sample_cache[idx]);

	return samp;
}

static void
clear_sample_cache(struct fd_batch *batch)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(batch->sample_cache); i++)
		fd_hw_sample_reference(batch->ctx, &batch->sample_cache[i], NULL);
}

static bool
is_active(struct fd_hw_query *hq, enum fd_render_stage stage)
{
	return !!(hq->provider->active & stage);
}


static void
resume_query(struct fd_batch *batch, struct fd_hw_query *hq,
		struct fd_ringbuffer *ring)
{
	int idx = pidx(hq->provider->query_type);
	DBG("%p", hq);
	assert(idx >= 0);   /* query never would have been created otherwise */
	assert(!hq->period);
	batch->active_providers |= (1 << idx);
	hq->period = slab_alloc_st(&batch->ctx->sample_period_pool);
	list_inithead(&hq->period->list);
	hq->period->start = get_sample(batch, ring, hq->base.type);
	/* NOTE: slab_alloc_st() does not zero out the buffer: */
	hq->period->end = NULL;
}

static void
pause_query(struct fd_batch *batch, struct fd_hw_query *hq,
		struct fd_ringbuffer *ring)
{
	int idx = pidx(hq->provider->query_type);
	DBG("%p", hq);
	assert(idx >= 0);   /* query never would have been created otherwise */
	assert(hq->period && !hq->period->end);
	assert(batch->active_providers & (1 << idx));
	hq->period->end = get_sample(batch, ring, hq->base.type);
	list_addtail(&hq->period->list, &hq->periods);
	hq->period = NULL;
}

static void
destroy_periods(struct fd_context *ctx, struct fd_hw_query *hq)
{
	struct fd_hw_sample_period *period, *s;
	LIST_FOR_EACH_ENTRY_SAFE(period, s, &hq->periods, list) {
		fd_hw_sample_reference(ctx, &period->start, NULL);
		fd_hw_sample_reference(ctx, &period->end, NULL);
		list_del(&period->list);
		slab_free_st(&ctx->sample_period_pool, period);
	}
}

static void
fd_hw_destroy_query(struct fd_context *ctx, struct fd_query *q)
{
	struct fd_hw_query *hq = fd_hw_query(q);

	DBG("%p: active=%d", q, q->active);

	destroy_periods(ctx, hq);
	list_del(&hq->list);

	free(hq);
}

static boolean
fd_hw_begin_query(struct fd_context *ctx, struct fd_query *q)
{
	struct fd_batch *batch = ctx->batch;
	struct fd_hw_query *hq = fd_hw_query(q);

	DBG("%p: active=%d", q, q->active);

	/* begin_query() should clear previous results: */
	destroy_periods(ctx, hq);

	if (batch && is_active(hq, batch->stage))
		resume_query(batch, hq, batch->draw);

	/* add to active list: */
	assert(list_empty(&hq->list));
	list_addtail(&hq->list, &ctx->hw_active_queries);

	return true;
}

static void
fd_hw_end_query(struct fd_context *ctx, struct fd_query *q)
{
	struct fd_batch *batch = ctx->batch;
	struct fd_hw_query *hq = fd_hw_query(q);

	DBG("%p: active=%d", q, q->active);

	if (batch && is_active(hq, batch->stage))
		pause_query(batch, hq, batch->draw);

	/* remove from active list: */
	list_delinit(&hq->list);
}

/* helper to get ptr to specified sample: */
static void * sampptr(struct fd_hw_sample *samp, uint32_t n, void *ptr)
{
	return ((char *)ptr) + (samp->tile_stride * n) + samp->offset;
}

static boolean
fd_hw_get_query_result(struct fd_context *ctx, struct fd_query *q,
		boolean wait, union pipe_query_result *result)
{
	struct fd_hw_query *hq = fd_hw_query(q);
	const struct fd_hw_sample_provider *p = hq->provider;
	struct fd_hw_sample_period *period;

	DBG("%p: wait=%d, active=%d", q, wait, q->active);

	if (LIST_IS_EMPTY(&hq->periods))
		return true;

	assert(LIST_IS_EMPTY(&hq->list));
	assert(!hq->period);

	/* if !wait, then check the last sample (the one most likely to
	 * not be ready yet) and bail if it is not ready:
	 */
	if (!wait) {
		int ret;

		period = LIST_ENTRY(struct fd_hw_sample_period,
				hq->periods.prev, list);

		struct fd_resource *rsc = fd_resource(period->end->prsc);

		if (pending(rsc, false)) {
			/* piglit spec@arb_occlusion_query@occlusion_query_conform
			 * test, and silly apps perhaps, get stuck in a loop trying
			 * to get  query result forever with wait==false..  we don't
			 * wait to flush unnecessarily but we also don't want to
			 * spin forever:
			 */
			if (hq->no_wait_cnt++ > 5)
				fd_batch_flush(rsc->write_batch, false);
			return false;
		}

		if (!rsc->bo)
			return false;

		ret = fd_bo_cpu_prep(rsc->bo, ctx->screen->pipe,
				DRM_FREEDRENO_PREP_READ | DRM_FREEDRENO_PREP_NOSYNC);
		if (ret)
			return false;

		fd_bo_cpu_fini(rsc->bo);
	}

	/* sum the result across all sample periods: */
	LIST_FOR_EACH_ENTRY(period, &hq->periods, list) {
		struct fd_hw_sample *start = period->start;
		struct fd_hw_sample *end = period->end;
		unsigned i;

		/* start and end samples should be from same batch: */
		assert(start->prsc == end->prsc);
		assert(start->num_tiles == end->num_tiles);

		struct fd_resource *rsc = fd_resource(start->prsc);

		if (rsc->write_batch)
			fd_batch_flush(rsc->write_batch, true);

		/* some piglit tests at least do query with no draws, I guess: */
		if (!rsc->bo)
			continue;

		fd_bo_cpu_prep(rsc->bo, ctx->screen->pipe, DRM_FREEDRENO_PREP_READ);

		void *ptr = fd_bo_map(rsc->bo);

		for (i = 0; i < start->num_tiles; i++) {
			p->accumulate_result(ctx, sampptr(period->start, i, ptr),
					sampptr(period->end, i, ptr), result);
		}

		fd_bo_cpu_fini(rsc->bo);
	}

	return true;
}

static const struct fd_query_funcs hw_query_funcs = {
		.destroy_query    = fd_hw_destroy_query,
		.begin_query      = fd_hw_begin_query,
		.end_query        = fd_hw_end_query,
		.get_query_result = fd_hw_get_query_result,
};

struct fd_query *
fd_hw_create_query(struct fd_context *ctx, unsigned query_type)
{
	struct fd_hw_query *hq;
	struct fd_query *q;
	int idx = pidx(query_type);

	if ((idx < 0) || !ctx->hw_sample_providers[idx])
		return NULL;

	hq = CALLOC_STRUCT(fd_hw_query);
	if (!hq)
		return NULL;

	DBG("%p: query_type=%u", hq, query_type);

	hq->provider = ctx->hw_sample_providers[idx];

	list_inithead(&hq->periods);
	list_inithead(&hq->list);

	q = &hq->base;
	q->funcs = &hw_query_funcs;
	q->type = query_type;

	return q;
}

struct fd_hw_sample *
fd_hw_sample_init(struct fd_batch *batch, uint32_t size)
{
	struct fd_hw_sample *samp = slab_alloc_st(&batch->ctx->sample_pool);
	pipe_reference_init(&samp->reference, 1);
	samp->size = size;
	debug_assert(util_is_power_of_two(size));
	batch->next_sample_offset = align(batch->next_sample_offset, size);
	samp->offset = batch->next_sample_offset;
	/* NOTE: slab_alloc_st() does not zero out the buffer: */
	samp->prsc = NULL;
	samp->num_tiles = 0;
	samp->tile_stride = 0;
	batch->next_sample_offset += size;

	if (!batch->query_buf) {
		struct pipe_screen *pscreen = &batch->ctx->screen->base;
		struct pipe_resource templ = {
			.target  = PIPE_BUFFER,
			.format  = PIPE_FORMAT_R8_UNORM,
			.bind    = PIPE_BIND_QUERY_BUFFER,
			.width0  = 0,    /* create initially zero size buffer */
			.height0 = 1,
			.depth0  = 1,
			.array_size = 1,
			.last_level = 0,
			.nr_samples = 1,
		};
		batch->query_buf = pscreen->resource_create(pscreen, &templ);
	}

	pipe_resource_reference(&samp->prsc, batch->query_buf);

	return samp;
}

void
__fd_hw_sample_destroy(struct fd_context *ctx, struct fd_hw_sample *samp)
{
	pipe_resource_reference(&samp->prsc, NULL);
	slab_free_st(&ctx->sample_pool, samp);
}

/* called from gmem code once total storage requirements are known (ie.
 * number of samples times number of tiles)
 */
void
fd_hw_query_prepare(struct fd_batch *batch, uint32_t num_tiles)
{
	uint32_t tile_stride = batch->next_sample_offset;

	if (tile_stride > 0)
		fd_resource_resize(batch->query_buf, tile_stride * num_tiles);

	batch->query_tile_stride = tile_stride;

	while (batch->samples.size > 0) {
		struct fd_hw_sample *samp =
			util_dynarray_pop(&batch->samples, struct fd_hw_sample *);
		samp->num_tiles = num_tiles;
		samp->tile_stride = tile_stride;
		fd_hw_sample_reference(batch->ctx, &samp, NULL);
	}

	/* reset things for next batch: */
	batch->next_sample_offset = 0;
}

void
fd_hw_query_prepare_tile(struct fd_batch *batch, uint32_t n,
		struct fd_ringbuffer *ring)
{
	uint32_t tile_stride = batch->query_tile_stride;
	uint32_t offset = tile_stride * n;

	/* bail if no queries: */
	if (tile_stride == 0)
		return;

	fd_wfi(batch, ring);
	OUT_PKT0 (ring, HW_QUERY_BASE_REG, 1);
	OUT_RELOCW(ring, fd_resource(batch->query_buf)->bo, offset, 0, 0);
}

void
fd_hw_query_set_stage(struct fd_batch *batch, enum fd_render_stage stage)
{
	if (stage != batch->stage) {
		struct fd_hw_query *hq;
		LIST_FOR_EACH_ENTRY(hq, &batch->ctx->hw_active_queries, list) {
			bool was_active = is_active(hq, batch->stage);
			bool now_active = is_active(hq, stage);

			if (now_active && !was_active)
				resume_query(batch, hq, batch->draw);
			else if (was_active && !now_active)
				pause_query(batch, hq, batch->draw);
		}
	}
	clear_sample_cache(batch);
}

/* call the provider->enable() for all the hw queries that were active
 * in the current batch.  This sets up perfctr selector regs statically
 * for the duration of the batch.
 */
void
fd_hw_query_enable(struct fd_batch *batch, struct fd_ringbuffer *ring)
{
	struct fd_context *ctx = batch->ctx;
	for (int idx = 0; idx < MAX_HW_SAMPLE_PROVIDERS; idx++) {
		if (batch->active_providers & (1 << idx)) {
			assert(ctx->hw_sample_providers[idx]);
			if (ctx->hw_sample_providers[idx]->enable)
				ctx->hw_sample_providers[idx]->enable(ctx, ring);
		}
	}
	batch->active_providers = 0;  /* clear it for next frame */
}

void
fd_hw_query_register_provider(struct pipe_context *pctx,
		const struct fd_hw_sample_provider *provider)
{
	struct fd_context *ctx = fd_context(pctx);
	int idx = pidx(provider->query_type);

	assert((0 <= idx) && (idx < MAX_HW_SAMPLE_PROVIDERS));
	assert(!ctx->hw_sample_providers[idx]);

	ctx->hw_sample_providers[idx] = provider;
}

void
fd_hw_query_init(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);

	slab_create(&ctx->sample_pool, sizeof(struct fd_hw_sample),
			16);
	slab_create(&ctx->sample_period_pool, sizeof(struct fd_hw_sample_period),
			16);
}

void
fd_hw_query_fini(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);

	slab_destroy(&ctx->sample_pool);
	slab_destroy(&ctx->sample_period_pool);
}
