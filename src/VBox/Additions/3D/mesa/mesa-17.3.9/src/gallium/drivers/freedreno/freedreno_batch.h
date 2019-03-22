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

#ifndef FREEDRENO_BATCH_H_
#define FREEDRENO_BATCH_H_

#include "util/u_inlines.h"
#include "util/u_queue.h"
#include "util/list.h"

#include "freedreno_util.h"

struct fd_context;
struct fd_resource;
enum fd_resource_status;

/* Bitmask of stages in rendering that a particular query query is
 * active.  Queries will be automatically started/stopped (generating
 * additional fd_hw_sample_period's) on entrance/exit from stages that
 * are applicable to the query.
 *
 * NOTE: set the stage to NULL at end of IB to ensure no query is still
 * active.  Things aren't going to work out the way you want if a query
 * is active across IB's (or between tile IB and draw IB)
 */
enum fd_render_stage {
	FD_STAGE_NULL     = 0x01,
	FD_STAGE_DRAW     = 0x02,
	FD_STAGE_CLEAR    = 0x04,
	/* used for driver internal draws (ie. util_blitter_blit()): */
	FD_STAGE_BLIT     = 0x08,
	FD_STAGE_ALL      = 0xff,
};

#define MAX_HW_SAMPLE_PROVIDERS 5
struct fd_hw_sample_provider;
struct fd_hw_sample;

/* A batch tracks everything about a cmdstream batch/submit, including the
 * ringbuffers used for binning, draw, and gmem cmds, list of associated
 * fd_resource-s, etc.
 */
struct fd_batch {
	struct pipe_reference reference;
	unsigned seqno;
	unsigned idx;

	int in_fence_fd;
	bool needs_out_fence_fd;

	struct fd_context *ctx;

	struct util_queue_fence flush_fence;

	/* do we need to mem2gmem before rendering.  We don't, if for example,
	 * there was a glClear() that invalidated the entire previous buffer
	 * contents.  Keep track of which buffer(s) are cleared, or needs
	 * restore.  Masks of PIPE_CLEAR_*
	 *
	 * The 'cleared' bits will be set for buffers which are *entirely*
	 * cleared, and 'partial_cleared' bits will be set if you must
	 * check cleared_scissor.
	 */
	enum {
		/* align bitmask values w/ PIPE_CLEAR_*.. since that is convenient.. */
		FD_BUFFER_COLOR   = PIPE_CLEAR_COLOR,
		FD_BUFFER_DEPTH   = PIPE_CLEAR_DEPTH,
		FD_BUFFER_STENCIL = PIPE_CLEAR_STENCIL,
		FD_BUFFER_ALL     = FD_BUFFER_COLOR | FD_BUFFER_DEPTH | FD_BUFFER_STENCIL,
	} cleared, partial_cleared, restore, resolve;

	bool needs_flush : 1;
	bool blit : 1;
	bool back_blit : 1;      /* only blit so far is resource shadowing back-blit */

	/* Keep track if WAIT_FOR_IDLE is needed for registers we need
	 * to update via RMW:
	 */
	bool needs_wfi : 1;

	/* To decide whether to render to system memory, keep track of the
	 * number of draws, and whether any of them require multisample,
	 * depth_test (or depth write), stencil_test, blending, and
	 * color_logic_Op (since those functions are disabled when by-
	 * passing GMEM.
	 */
	enum {
		FD_GMEM_CLEARS_DEPTH_STENCIL = 0x01,
		FD_GMEM_DEPTH_ENABLED        = 0x02,
		FD_GMEM_STENCIL_ENABLED      = 0x04,

		FD_GMEM_MSAA_ENABLED         = 0x08,
		FD_GMEM_BLEND_ENABLED        = 0x10,
		FD_GMEM_LOGICOP_ENABLED      = 0x20,
	} gmem_reason;
	unsigned num_draws;   /* number of draws in current batch */

	/* Track the maximal bounds of the scissor of all the draws within a
	 * batch.  Used at the tile rendering step (fd_gmem_render_tiles(),
	 * mem2gmem/gmem2mem) to avoid needlessly moving data in/out of gmem.
	 */
	struct pipe_scissor_state max_scissor;

	/* Track the cleared scissor for color/depth/stencil, so we know
	 * which, if any, tiles need to be restored (mem2gmem).  Only valid
	 * if the corresponding bit in ctx->cleared is set.
	 */
	struct {
		struct pipe_scissor_state color, depth, stencil;
	} cleared_scissor;

	/* Keep track of DRAW initiators that need to be patched up depending
	 * on whether we using binning or not:
	 */
	struct util_dynarray draw_patches;

	/* Keep track of writes to RB_RENDER_CONTROL which need to be patched
	 * once we know whether or not to use GMEM, and GMEM tile pitch.
	 *
	 * (only for a3xx.. but having gen specific subclasses of fd_batch
	 * seemed overkill for now)
	 */
	struct util_dynarray rbrc_patches;

	struct pipe_framebuffer_state framebuffer;

	/** draw pass cmdstream: */
	struct fd_ringbuffer *draw;
	/** binning pass cmdstream: */
	struct fd_ringbuffer *binning;
	/** tiling/gmem (IB0) cmdstream: */
	struct fd_ringbuffer *gmem;

	// TODO maybe more generically split out clear and clear_binning rings?
	struct fd_ringbuffer *lrz_clear;

	/**
	 * hw query related state:
	 */
	/*@{*/
	/* next sample offset.. incremented for each sample in the batch/
	 * submit, reset to zero on next submit.
	 */
	uint32_t next_sample_offset;

	/* cached samples (in case multiple queries need to reference
	 * the same sample snapshot)
	 */
	struct fd_hw_sample *sample_cache[MAX_HW_SAMPLE_PROVIDERS];

	/* which sample providers were active in the current batch: */
	uint32_t active_providers;

	/* tracking for current stage, to know when to start/stop
	 * any active queries:
	 */
	enum fd_render_stage stage;

	/* list of samples in current batch: */
	struct util_dynarray samples;

	/* current query result bo and tile stride: */
	struct pipe_resource *query_buf;
	uint32_t query_tile_stride;
	/*@}*/


	/* Set of resources used by currently-unsubmitted batch (read or
	 * write).. does not hold a reference to the resource.
	 */
	struct set *resources;

	/** key in batch-cache (if not null): */
	const void *key;
	uint32_t hash;

	/** set of dependent batches.. holds refs to dependent batches: */
	uint32_t dependents_mask;
};

struct fd_batch * fd_batch_create(struct fd_context *ctx);

void fd_batch_reset(struct fd_batch *batch);
void fd_batch_sync(struct fd_batch *batch);
void fd_batch_flush(struct fd_batch *batch, bool sync);
void fd_batch_resource_used(struct fd_batch *batch, struct fd_resource *rsc, bool write);
void fd_batch_check_size(struct fd_batch *batch);

/* not called directly: */
void __fd_batch_describe(char* buf, const struct fd_batch *batch);
void __fd_batch_destroy(struct fd_batch *batch);

/*
 * NOTE the rule is, you need to hold the screen->lock when destroying
 * a batch..  so either use fd_batch_reference() (which grabs the lock
 * for you) if you don't hold the lock, or fd_batch_reference_locked()
 * if you do hold the lock.
 *
 * WARNING the _locked() version can briefly drop the lock.  Without
 * recursive mutexes, I'm not sure there is much else we can do (since
 * __fd_batch_destroy() needs to unref resources)
 */

static inline void
fd_batch_reference(struct fd_batch **ptr, struct fd_batch *batch)
{
	struct fd_batch *old_batch = *ptr;
	if (pipe_reference_described(&(*ptr)->reference, &batch->reference,
			(debug_reference_descriptor)__fd_batch_describe))
		__fd_batch_destroy(old_batch);
	*ptr = batch;
}

/* fwd-decl prototypes to untangle header dependency :-/ */
static inline void fd_context_assert_locked(struct fd_context *ctx);
static inline void fd_context_lock(struct fd_context *ctx);
static inline void fd_context_unlock(struct fd_context *ctx);

static inline void
fd_batch_reference_locked(struct fd_batch **ptr, struct fd_batch *batch)
{
	struct fd_batch *old_batch = *ptr;

	if (old_batch)
		fd_context_assert_locked(old_batch->ctx);
	else if (batch)
		fd_context_assert_locked(batch->ctx);

	if (pipe_reference_described(&(*ptr)->reference, &batch->reference,
			(debug_reference_descriptor)__fd_batch_describe)) {
		struct fd_context *ctx = old_batch->ctx;
		fd_context_unlock(ctx);
		__fd_batch_destroy(old_batch);
		fd_context_lock(ctx);
	}
	*ptr = batch;
}

#include "freedreno_context.h"

static inline void
fd_reset_wfi(struct fd_batch *batch)
{
	batch->needs_wfi = true;
}

void fd_wfi(struct fd_batch *batch, struct fd_ringbuffer *ring);

/* emit a CP_EVENT_WRITE:
 */
static inline void
fd_event_write(struct fd_batch *batch, struct fd_ringbuffer *ring,
		enum vgt_event_type evt)
{
	OUT_PKT3(ring, CP_EVENT_WRITE, 1);
	OUT_RING(ring, evt);
	fd_reset_wfi(batch);
}

#endif /* FREEDRENO_BATCH_H_ */
