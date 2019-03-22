/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2013 Rob Clark <robclark@freedesktop.org>
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

#ifndef FREEDRENO_QUERY_H_
#define FREEDRENO_QUERY_H_

#include "pipe/p_context.h"

struct fd_context;
struct fd_query;

struct fd_query_funcs {
	void (*destroy_query)(struct fd_context *ctx,
			struct fd_query *q);
	boolean (*begin_query)(struct fd_context *ctx, struct fd_query *q);
	void (*end_query)(struct fd_context *ctx, struct fd_query *q);
	boolean (*get_query_result)(struct fd_context *ctx,
			struct fd_query *q, boolean wait,
			union pipe_query_result *result);
};

struct fd_query {
	const struct fd_query_funcs *funcs;
	bool active;
	int type;
};

static inline struct fd_query *
fd_query(struct pipe_query *pq)
{
	return (struct fd_query *)pq;
}

#define FD_QUERY_DRAW_CALLS      (PIPE_QUERY_DRIVER_SPECIFIC + 0)
#define FD_QUERY_BATCH_TOTAL     (PIPE_QUERY_DRIVER_SPECIFIC + 1)  /* total # of batches (submits) */
#define FD_QUERY_BATCH_SYSMEM    (PIPE_QUERY_DRIVER_SPECIFIC + 2)  /* batches using system memory (GMEM bypass) */
#define FD_QUERY_BATCH_GMEM      (PIPE_QUERY_DRIVER_SPECIFIC + 3)  /* batches using GMEM */
#define FD_QUERY_BATCH_RESTORE   (PIPE_QUERY_DRIVER_SPECIFIC + 4)  /* batches requiring GMEM restore */

void fd_query_screen_init(struct pipe_screen *pscreen);
void fd_query_context_init(struct pipe_context *pctx);

static inline bool
skip_begin_query(int type)
{
	switch (type) {
	case PIPE_QUERY_TIMESTAMP:
	case PIPE_QUERY_GPU_FINISHED:
		return true;
	default:
		return false;
	}
}

/* maps query_type to sample provider idx: */
static inline
int pidx(unsigned query_type)
{
	switch (query_type) {
	case PIPE_QUERY_OCCLUSION_COUNTER:
		return 0;
	case PIPE_QUERY_OCCLUSION_PREDICATE:
		return 1;
	case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
		return 2;
	/* TODO currently queries only emitted in main pass (not in binning pass)..
	 * which is fine for occlusion query, but pretty much not anything else.
	 */
	case PIPE_QUERY_TIME_ELAPSED:
		return 3;
	case PIPE_QUERY_TIMESTAMP:
		return 4;
	default:
		return -1;
	}
}

#endif /* FREEDRENO_QUERY_H_ */
