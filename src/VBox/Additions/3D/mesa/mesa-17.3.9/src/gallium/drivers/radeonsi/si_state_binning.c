/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* This file handles register programming of primitive binning. */

#include "si_pipe.h"
#include "sid.h"
#include "gfx9d.h"
#include "radeon/r600_cs.h"

struct uvec2 {
	unsigned x, y;
};

struct si_bin_size_map {
	unsigned start;
	unsigned bin_size_x;
	unsigned bin_size_y;
};

typedef struct si_bin_size_map si_bin_size_subtable[3][9];

/* Find the bin size where sum is >= table[i].start and < table[i + 1].start. */
static struct uvec2 si_find_bin_size(struct si_screen *sscreen,
				     const si_bin_size_subtable table[],
				     unsigned sum)
{
	unsigned log_num_rb_per_se =
		util_logbase2_ceil(sscreen->b.info.num_render_backends /
				   sscreen->b.info.max_se);
	unsigned log_num_se = util_logbase2_ceil(sscreen->b.info.max_se);
	unsigned i;

	/* Get the chip-specific subtable. */
	const struct si_bin_size_map *subtable =
		&table[log_num_rb_per_se][log_num_se][0];

	for (i = 0; subtable[i].start != UINT_MAX; i++) {
		if (sum >= subtable[i].start && sum < subtable[i + 1].start)
			break;
	}

	struct uvec2 size = {subtable[i].bin_size_x, subtable[i].bin_size_y};
	return size;
}

static struct uvec2 si_get_color_bin_size(struct si_context *sctx,
					  unsigned cb_target_enabled_4bit)
{
	unsigned nr_samples = sctx->framebuffer.nr_samples;
	unsigned sum = 0;

	/* Compute the sum of all Bpp. */
	for (unsigned i = 0; i < sctx->framebuffer.state.nr_cbufs; i++) {
		if (!(cb_target_enabled_4bit & (0xf << (i * 4))))
			continue;

		struct r600_texture *rtex =
			(struct r600_texture*)sctx->framebuffer.state.cbufs[i]->texture;
		sum += rtex->surface.bpe;
	}

	/* Multiply the sum by some function of the number of samples. */
	if (nr_samples >= 2) {
		if (sctx->ps_iter_samples >= 2)
			sum *= nr_samples;
		else
			sum *= 2;
	}

	static const si_bin_size_subtable table[] = {
		{
			/* One RB / SE */
			{
				/* One shader engine */
				{        0,  128,  128 },
				{        1,   64,  128 },
				{        2,   32,  128 },
				{        3,   16,  128 },
				{       17,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Two shader engines */
				{        0,  128,  128 },
				{        2,   64,  128 },
				{        3,   32,  128 },
				{        5,   16,  128 },
				{       17,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Four shader engines */
				{        0,  128,  128 },
				{        3,   64,  128 },
				{        5,   16,  128 },
				{       17,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
		},
		{
			/* Two RB / SE */
			{
				/* One shader engine */
				{        0,  128,  128 },
				{        2,   64,  128 },
				{        3,   32,  128 },
				{        5,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Two shader engines */
				{        0,  128,  128 },
				{        3,   64,  128 },
				{        5,   32,  128 },
				{        9,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Four shader engines */
				{        0,  256,  256 },
				{        2,  128,  256 },
				{        3,  128,  128 },
				{        5,   64,  128 },
				{        9,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
		},
		{
			/* Four RB / SE */
			{
				/* One shader engine */
				{        0,  128,  256 },
				{        2,  128,  128 },
				{        3,   64,  128 },
				{        5,   32,  128 },
				{        9,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Two shader engines */
				{        0,  256,  256 },
				{        2,  128,  256 },
				{        3,  128,  128 },
				{        5,   64,  128 },
				{        9,   32,  128 },
				{       17,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				/* Four shader engines */
				{        0,  256,  512 },
				{        2,  256,  256 },
				{        3,  128,  256 },
				{        5,  128,  128 },
				{        9,   64,  128 },
				{       17,   16,  128 },
				{       33,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
		},
	};

	return si_find_bin_size(sctx->screen, table, sum);
}

static struct uvec2 si_get_depth_bin_size(struct si_context *sctx)
{
	struct si_state_dsa *dsa = sctx->queued.named.dsa;

	if (!sctx->framebuffer.state.zsbuf ||
	    (!dsa->depth_enabled && !dsa->stencil_enabled)) {
		/* Return the max size. */
		struct uvec2 size = {512, 512};
		return size;
	}

	struct r600_texture *rtex =
		(struct r600_texture*)sctx->framebuffer.state.zsbuf->texture;
	unsigned depth_coeff = dsa->depth_enabled ? 5 : 0;
	unsigned stencil_coeff = rtex->surface.has_stencil &&
				 dsa->stencil_enabled ? 1 : 0;
	unsigned sum = 4 * (depth_coeff + stencil_coeff) *
		       sctx->framebuffer.nr_samples;

	static const si_bin_size_subtable table[] = {
		{
			// One RB / SE
			{
				// One shader engine
				{        0,  128,  256 },
				{        2,  128,  128 },
				{        4,   64,  128 },
				{        7,   32,  128 },
				{       13,   16,  128 },
				{       49,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Two shader engines
				{        0,  256,  256 },
				{        2,  128,  256 },
				{        4,  128,  128 },
				{        7,   64,  128 },
				{       13,   32,  128 },
				{       25,   16,  128 },
				{       49,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Four shader engines
				{        0,  256,  512 },
				{        2,  256,  256 },
				{        4,  128,  256 },
				{        7,  128,  128 },
				{       13,   64,  128 },
				{       25,   16,  128 },
				{       49,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
		},
		{
			// Two RB / SE
			{
				// One shader engine
				{        0,  256,  256 },
				{        2,  128,  256 },
				{        4,  128,  128 },
				{        7,   64,  128 },
				{       13,   32,  128 },
				{       25,   16,  128 },
				{       97,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Two shader engines
				{        0,  256,  512 },
				{        2,  256,  256 },
				{        4,  128,  256 },
				{        7,  128,  128 },
				{       13,   64,  128 },
				{       25,   32,  128 },
				{       49,   16,  128 },
				{       97,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Four shader engines
				{        0,  512,  512 },
				{        2,  256,  512 },
				{        4,  256,  256 },
				{        7,  128,  256 },
				{       13,  128,  128 },
				{       25,   64,  128 },
				{       49,   16,  128 },
				{       97,    0,    0 },
				{ UINT_MAX,    0,    0 },
			},
		},
		{
			// Four RB / SE
			{
				// One shader engine
				{        0,  256,  512 },
				{        2,  256,  256 },
				{        4,  128,  256 },
				{        7,  128,  128 },
				{       13,   64,  128 },
				{       25,   32,  128 },
				{       49,   16,  128 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Two shader engines
				{        0,  512,  512 },
				{        2,  256,  512 },
				{        4,  256,  256 },
				{        7,  128,  256 },
				{       13,  128,  128 },
				{       25,   64,  128 },
				{       49,   32,  128 },
				{       97,   16,  128 },
				{ UINT_MAX,    0,    0 },
			},
			{
				// Four shader engines
				{        0,  512,  512 },
				{        4,  256,  512 },
				{        7,  256,  256 },
				{       13,  128,  256 },
				{       25,  128,  128 },
				{       49,   64,  128 },
				{       97,   16,  128 },
				{ UINT_MAX,    0,    0 },
			},
		},
	};

	return si_find_bin_size(sctx->screen, table, sum);
}

static void si_emit_dpbb_disable(struct si_context *sctx)
{
	struct radeon_winsys_cs *cs = sctx->b.gfx.cs;

	radeon_set_context_reg(cs, R_028C44_PA_SC_BINNER_CNTL_0,
			       S_028C44_BINNING_MODE(V_028C44_DISABLE_BINNING_USE_LEGACY_SC) |
			       S_028C44_DISABLE_START_OF_PRIM(1));
	radeon_set_context_reg(cs, R_028060_DB_DFSM_CONTROL,
			       S_028060_PUNCHOUT_MODE(V_028060_FORCE_OFF));
}

void si_emit_dpbb_state(struct si_context *sctx, struct r600_atom *state)
{
	struct si_screen *sscreen = sctx->screen;
	struct si_state_blend *blend = sctx->queued.named.blend;
	struct si_state_dsa *dsa = sctx->queued.named.dsa;
	unsigned db_shader_control = sctx->ps_db_shader_control;

	assert(sctx->b.chip_class >= GFX9);

	if (!sscreen->dpbb_allowed || !blend || !dsa) {
		si_emit_dpbb_disable(sctx);
		return;
	}

	bool ps_can_kill = G_02880C_KILL_ENABLE(db_shader_control) ||
			   G_02880C_MASK_EXPORT_ENABLE(db_shader_control) ||
			   G_02880C_COVERAGE_TO_MASK_ENABLE(db_shader_control) ||
			   blend->alpha_to_coverage;

	/* This is ported from Vulkan, but it doesn't make much sense to me.
	 * Maybe it's for RE-Z? But Vulkan doesn't use RE-Z. TODO: Clarify this.
	 */
	bool ps_can_reject_z_trivially =
		!G_02880C_Z_EXPORT_ENABLE(db_shader_control) ||
		G_02880C_CONSERVATIVE_Z_EXPORT(db_shader_control);

	/* Disable binning if PS can kill trivially with DB writes.
	 * Ported from Vulkan. (heuristic?)
	 */
	if (ps_can_kill &&
	    ps_can_reject_z_trivially &&
	    sctx->framebuffer.state.zsbuf &&
	    dsa->db_can_write) {
		si_emit_dpbb_disable(sctx);
		return;
	}

	/* Compute the bin size. */
	/* TODO: We could also look at enabled pixel shader outputs. */
	unsigned cb_target_enabled_4bit = sctx->framebuffer.colorbuf_enabled_4bit &
					  blend->cb_target_enabled_4bit;
	struct uvec2 color_bin_size =
		si_get_color_bin_size(sctx, cb_target_enabled_4bit);
	struct uvec2 depth_bin_size = si_get_depth_bin_size(sctx);

	unsigned color_area = color_bin_size.x * color_bin_size.y;
	unsigned depth_area = depth_bin_size.x * depth_bin_size.y;

	struct uvec2 bin_size = color_area < depth_area ? color_bin_size
							: depth_bin_size;

	if (!bin_size.x || !bin_size.y) {
		si_emit_dpbb_disable(sctx);
		return;
	}

	/* Enable DFSM if it's preferred. */
	unsigned punchout_mode = V_028060_FORCE_OFF;
	bool disable_start_of_prim = true;

	if (sscreen->dfsm_allowed &&
	    cb_target_enabled_4bit &&
	    !G_02880C_KILL_ENABLE(db_shader_control) &&
	    /* These two also imply that DFSM is disabled when PS writes to memory. */
	    !G_02880C_EXEC_ON_HIER_FAIL(db_shader_control) &&
	    !G_02880C_EXEC_ON_NOOP(db_shader_control) &&
	    G_02880C_Z_ORDER(db_shader_control) == V_02880C_EARLY_Z_THEN_LATE_Z) {
		punchout_mode = V_028060_AUTO;
		disable_start_of_prim = (cb_target_enabled_4bit &
					 blend->blend_enable_4bit) != 0;
	}

	/* Tunable parameters. Also test with DFSM enabled/disabled. */
	unsigned context_states_per_bin; /* allowed range: [0, 5] */
	unsigned persistent_states_per_bin; /* allowed range: [0, 31] */
	unsigned fpovs_per_batch; /* allowed range: [0, 255], 0 = unlimited */

	switch (sctx->b.family) {
	case CHIP_VEGA10:
	case CHIP_RAVEN:
		/* Tuned for Raven. Vega might need different values. */
		context_states_per_bin = 5;
		persistent_states_per_bin = 31;
		fpovs_per_batch = 63;
		break;
	default:
		assert(0);
	}

	/* Emit registers. */
	struct uvec2 bin_size_extend = {};
	if (bin_size.x >= 32)
		bin_size_extend.x = util_logbase2(bin_size.x) - 5;
	if (bin_size.y >= 32)
		bin_size_extend.y = util_logbase2(bin_size.y) - 5;

	struct radeon_winsys_cs *cs = sctx->b.gfx.cs;
	radeon_set_context_reg(cs, R_028C44_PA_SC_BINNER_CNTL_0,
			       S_028C44_BINNING_MODE(V_028C44_BINNING_ALLOWED) |
			       S_028C44_BIN_SIZE_X(bin_size.x == 16) |
			       S_028C44_BIN_SIZE_Y(bin_size.y == 16) |
			       S_028C44_BIN_SIZE_X_EXTEND(bin_size_extend.x) |
			       S_028C44_BIN_SIZE_Y_EXTEND(bin_size_extend.y) |
			       S_028C44_CONTEXT_STATES_PER_BIN(context_states_per_bin) |
			       S_028C44_PERSISTENT_STATES_PER_BIN(persistent_states_per_bin) |
			       S_028C44_DISABLE_START_OF_PRIM(disable_start_of_prim) |
			       S_028C44_FPOVS_PER_BATCH(fpovs_per_batch) |
			       S_028C44_OPTIMAL_BIN_SELECTION(1));
	radeon_set_context_reg(cs, R_028060_DB_DFSM_CONTROL,
			       S_028060_PUNCHOUT_MODE(punchout_mode));
}
