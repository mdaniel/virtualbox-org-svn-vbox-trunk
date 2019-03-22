/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2012 Rob Clark <robclark@freedesktop.org>
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

#ifndef FREEDRENO_SCREEN_H_
#define FREEDRENO_SCREEN_H_

#include <freedreno_drmif.h>
#include <freedreno_ringbuffer.h>

#include "pipe/p_screen.h"
#include "util/u_memory.h"
#include "util/slab.h"
#include "os/os_thread.h"

#include "freedreno_batch_cache.h"
#include "freedreno_util.h"

struct fd_bo;

struct fd_screen {
	struct pipe_screen base;

	mtx_t lock;

	/* it would be tempting to use pipe_reference here, but that
	 * really doesn't work well if it isn't the first member of
	 * the struct, so not quite so awesome to be adding refcnting
	 * further down the inheritance hierarchy:
	 */
	int refcnt;

	/* place for winsys to stash it's own stuff: */
	void *winsys_priv;

	struct slab_parent_pool transfer_pool;

	uint32_t gmemsize_bytes;
	uint32_t device_id;
	uint32_t gpu_id;         /* 220, 305, etc */
	uint32_t chip_id;        /* coreid:8 majorrev:8 minorrev:8 patch:8 */
	uint32_t max_freq;
	uint32_t max_rts;        /* max # of render targets */
	uint32_t gmem_alignw, gmem_alignh;
	uint32_t num_vsc_pipes;
	bool has_timestamp;

	void *compiler;          /* currently unused for a2xx */

	struct fd_device *dev;
	struct fd_pipe *pipe;

	int64_t cpu_gpu_time_delta;

	struct fd_batch_cache batch_cache;

	bool reorder;
};

static inline struct fd_screen *
fd_screen(struct pipe_screen *pscreen)
{
	return (struct fd_screen *)pscreen;
}

boolean fd_screen_bo_get_handle(struct pipe_screen *pscreen,
		struct fd_bo *bo,
		unsigned stride,
		struct winsys_handle *whandle);
struct fd_bo * fd_screen_bo_from_handle(struct pipe_screen *pscreen,
		struct winsys_handle *whandle);

struct pipe_screen * fd_screen_create(struct fd_device *dev);

/* is a3xx patch revision 0? */
/* TODO a306.0 probably doesn't need this.. be more clever?? */
static inline boolean
is_a3xx_p0(struct fd_screen *screen)
{
	return (screen->chip_id & 0xff0000ff) == 0x03000000;
}

static inline boolean
is_a3xx(struct fd_screen *screen)
{
	return (screen->gpu_id >= 300) && (screen->gpu_id < 400);
}

static inline boolean
is_a4xx(struct fd_screen *screen)
{
	return (screen->gpu_id >= 400) && (screen->gpu_id < 500);
}

static inline boolean
is_a5xx(struct fd_screen *screen)
{
	return (screen->gpu_id >= 500) && (screen->gpu_id < 600);
}

/* is it using the ir3 compiler (shader isa introduced with a3xx)? */
static inline boolean
is_ir3(struct fd_screen *screen)
{
	return is_a3xx(screen) || is_a4xx(screen) || is_a5xx(screen);
}

static inline bool
has_compute(struct fd_screen *screen)
{
	return is_a5xx(screen);
}

#endif /* FREEDRENO_SCREEN_H_ */
