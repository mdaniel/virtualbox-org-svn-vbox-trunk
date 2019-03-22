/*
 * Copyright (C) 2017 Rob Clark <robclark@freedesktop.org>
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

#include "fd5_compute.h"
#include "fd5_context.h"
#include "fd5_emit.h"

struct fd5_compute_stateobj {
	struct ir3_shader *shader;
};


static void *
fd5_create_compute_state(struct pipe_context *pctx,
		const struct pipe_compute_state *cso)
{
	struct fd_context *ctx = fd_context(pctx);
	struct ir3_compiler *compiler = ctx->screen->compiler;
	struct fd5_compute_stateobj *so = CALLOC_STRUCT(fd5_compute_stateobj);
	so->shader = ir3_shader_create_compute(compiler, cso, &ctx->debug);
	return so;
}

static void
fd5_delete_compute_state(struct pipe_context *pctx, void *hwcso)
{
	struct fd5_compute_stateobj *so = hwcso;
	ir3_shader_destroy(so->shader);
	free(so);
}

/* maybe move to fd5_program? */
static void
cs_program_emit(struct fd_ringbuffer *ring, struct ir3_shader_variant *v)
{
	const struct ir3_info *i = &v->info;
	enum a3xx_threadsize thrsz;

	/* note: blob uses local_size_x/y/z threshold to choose threadsize: */
	thrsz = FOUR_QUADS;

	OUT_PKT4(ring, REG_A5XX_SP_SP_CNTL, 1);
	OUT_RING(ring, 0x00000000);        /* SP_SP_CNTL */

	OUT_PKT4(ring, REG_A5XX_HLSQ_CONTROL_0_REG, 1);
	OUT_RING(ring, A5XX_HLSQ_CONTROL_0_REG_FSTHREADSIZE(TWO_QUADS) |
		A5XX_HLSQ_CONTROL_0_REG_CSTHREADSIZE(thrsz) |
		0x00000880 /* XXX */);

	OUT_PKT4(ring, REG_A5XX_SP_CS_CTRL_REG0, 1);
	OUT_RING(ring, A5XX_SP_CS_CTRL_REG0_THREADSIZE(thrsz) |
		A5XX_SP_CS_CTRL_REG0_HALFREGFOOTPRINT(i->max_half_reg + 1) |
		A5XX_SP_CS_CTRL_REG0_FULLREGFOOTPRINT(i->max_reg + 1) |
		A5XX_SP_CS_CTRL_REG0_BRANCHSTACK(0x3) |  // XXX need to figure this out somehow..
		0x6 /* XXX */);

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CONFIG, 1);
	OUT_RING(ring, A5XX_HLSQ_CS_CONFIG_CONSTOBJECTOFFSET(0) |
		A5XX_HLSQ_CS_CONFIG_SHADEROBJOFFSET(0) |
		A5XX_HLSQ_CS_CONFIG_ENABLED);

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CNTL, 1);
	OUT_RING(ring, A5XX_HLSQ_CS_CNTL_INSTRLEN(v->instrlen) |
		COND(v->has_ssbo, A5XX_HLSQ_CS_CNTL_SSBO_ENABLE));

	OUT_PKT4(ring, REG_A5XX_SP_CS_CONFIG, 1);
	OUT_RING(ring, A5XX_SP_CS_CONFIG_CONSTOBJECTOFFSET(0) |
		A5XX_SP_CS_CONFIG_SHADEROBJOFFSET(0) |
		A5XX_SP_CS_CONFIG_ENABLED);

	unsigned constlen = align(v->constlen, 4) / 4;
	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CONSTLEN, 2);
	OUT_RING(ring, constlen);          /* HLSQ_CS_CONSTLEN */
	OUT_RING(ring, v->instrlen);       /* HLSQ_CS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_SP_CS_OBJ_START_LO, 2);
	OUT_RELOC(ring, v->bo, 0, 0, 0);   /* SP_CS_OBJ_START_LO/HI */

	OUT_PKT4(ring, REG_A5XX_HLSQ_UPDATE_CNTL, 1);
	OUT_RING(ring, 0x1f00000);

	uint32_t local_invocation_id, work_group_id;
	local_invocation_id = ir3_find_sysval_regid(v, SYSTEM_VALUE_LOCAL_INVOCATION_ID);
	work_group_id = ir3_find_sysval_regid(v, SYSTEM_VALUE_WORK_GROUP_ID);

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CNTL_0, 2);
	OUT_RING(ring, A5XX_HLSQ_CS_CNTL_0_WGIDCONSTID(work_group_id) |
			A5XX_HLSQ_CS_CNTL_0_UNK0(regid(63, 0)) |
			A5XX_HLSQ_CS_CNTL_0_UNK1(regid(63, 0)) |
			A5XX_HLSQ_CS_CNTL_0_LOCALIDREGID(local_invocation_id));
	OUT_RING(ring, 0x1);               /* HLSQ_CS_CNTL_1 */

	fd5_emit_shader(ring, v);
}

static void
fd5_launch_grid(struct fd_context *ctx, const struct pipe_grid_info *info)
{
	struct fd5_compute_stateobj *so = ctx->compute;
	struct ir3_shader_key key = {0};
	struct ir3_shader_variant *v;
	struct fd_ringbuffer *ring = ctx->batch->draw;

	if (info->indirect)
		return;  // TODO

	v = ir3_shader_variant(so->shader, key, &ctx->debug);

	if (ctx->dirty_shader[PIPE_SHADER_COMPUTE] & FD_DIRTY_SHADER_PROG)
		cs_program_emit(ring, v);

	fd5_emit_cs_state(ctx, ring, v);
	ir3_emit_cs_consts(v, ring, ctx, info);

	const unsigned *local_size = info->block; // v->shader->nir->info->cs.local_size;
	const unsigned *num_groups = info->grid;
	/* for some reason, mesa/st doesn't set info->work_dim, so just assume 3: */
	const unsigned work_dim = info->work_dim ? info->work_dim : 3;
	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_NDRANGE_0, 7);
	OUT_RING(ring, A5XX_HLSQ_CS_NDRANGE_0_KERNELDIM(work_dim) |
		A5XX_HLSQ_CS_NDRANGE_0_LOCALSIZEX(local_size[0] - 1) |
		A5XX_HLSQ_CS_NDRANGE_0_LOCALSIZEY(local_size[1] - 1) |
		A5XX_HLSQ_CS_NDRANGE_0_LOCALSIZEZ(local_size[2] - 1));
	OUT_RING(ring, A5XX_HLSQ_CS_NDRANGE_1_SIZE_X(local_size[0] * num_groups[0]));
	OUT_RING(ring, 0);            /* HLSQ_CS_NDRANGE_2 */
	OUT_RING(ring, A5XX_HLSQ_CS_NDRANGE_3_SIZE_Y(local_size[1] * num_groups[1]));
	OUT_RING(ring, 0);            /* HLSQ_CS_NDRANGE_4 */
	OUT_RING(ring, A5XX_HLSQ_CS_NDRANGE_5_SIZE_Z(local_size[2] * num_groups[2]));
	OUT_RING(ring, 0);            /* HLSQ_CS_NDRANGE_6 */

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_KERNEL_GROUP_X, 3);
	OUT_RING(ring, 1);            /* HLSQ_CS_KERNEL_GROUP_X */
	OUT_RING(ring, 1);            /* HLSQ_CS_KERNEL_GROUP_Y */
	OUT_RING(ring, 1);            /* HLSQ_CS_KERNEL_GROUP_Z */

	OUT_PKT7(ring, CP_EXEC_CS, 4);
	OUT_RING(ring, 0x00000000);
	OUT_RING(ring, CP_EXEC_CS_1_NGROUPS_X(info->grid[0]));
	OUT_RING(ring, CP_EXEC_CS_2_NGROUPS_Y(info->grid[1]));
	OUT_RING(ring, CP_EXEC_CS_3_NGROUPS_Z(info->grid[2]));
}

void
fd5_compute_init(struct pipe_context *pctx)
{
	struct fd_context *ctx = fd_context(pctx);
	ctx->launch_grid = fd5_launch_grid;
	pctx->create_compute_state = fd5_create_compute_state;
	pctx->delete_compute_state = fd5_delete_compute_state;
}
