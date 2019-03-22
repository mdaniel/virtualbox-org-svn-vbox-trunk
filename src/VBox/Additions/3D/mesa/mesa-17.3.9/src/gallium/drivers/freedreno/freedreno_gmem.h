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

#ifndef FREEDRENO_GMEM_H_
#define FREEDRENO_GMEM_H_

#include "pipe/p_context.h"

#include "freedreno_util.h"

/* per-pipe configuration for hw binning: */
struct fd_vsc_pipe {
	struct fd_bo *bo;
	uint8_t x, y, w, h;      /* VSC_PIPE[p].CONFIG */
};

/* per-tile configuration for hw binning: */
struct fd_tile {
	uint8_t p;               /* index into vsc_pipe[]s */
	uint8_t n;               /* slot within pipe */
	uint16_t bin_w, bin_h;
	uint16_t xoff, yoff;
};

struct fd_gmem_stateobj {
	struct pipe_scissor_state scissor;
	uint32_t cbuf_base[MAX_RENDER_TARGETS];
	uint32_t zsbuf_base[2];
	uint8_t cbuf_cpp[MAX_RENDER_TARGETS];
	uint8_t zsbuf_cpp[2];
	uint16_t bin_h, nbins_y;
	uint16_t bin_w, nbins_x;
	uint16_t minx, miny;
	uint16_t width, height;
	uint16_t maxpw, maxph;   /* maximum pipe width/height */
};

struct fd_batch;

void fd_gmem_render_tiles(struct fd_batch *batch);
void fd_gmem_render_noop(struct fd_batch *batch);
void fd_gmem_flush_compute(struct fd_batch *batch);

bool fd_gmem_needs_restore(struct fd_batch *batch, struct fd_tile *tile,
		uint32_t buffers);

#endif /* FREEDRENO_GMEM_H_ */
