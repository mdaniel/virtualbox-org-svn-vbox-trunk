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

#ifndef FD5_BLEND_H_
#define FD5_BLEND_H_

#include "pipe/p_state.h"
#include "pipe/p_context.h"

#include "freedreno_util.h"

struct fd5_blend_stateobj {
	struct pipe_blend_state base;

	struct {
		uint32_t control;
		uint32_t buf_info;
		/* Blend control bits for color if there is an alpha channel */
		uint32_t blend_control_rgb;
		/* Blend control bits for color if there is no alpha channel */
		uint32_t blend_control_no_alpha_rgb;
		/* Blend control bits for alpha channel */
		uint32_t blend_control_alpha;
	} rb_mrt[A5XX_MAX_RENDER_TARGETS];
	uint32_t rb_blend_cntl;
	uint32_t sp_blend_cntl;
	bool lrz_write;
};

static inline struct fd5_blend_stateobj *
fd5_blend_stateobj(struct pipe_blend_state *blend)
{
	return (struct fd5_blend_stateobj *)blend;
}

void * fd5_blend_state_create(struct pipe_context *pctx,
		const struct pipe_blend_state *cso);

#endif /* FD5_BLEND_H_ */
