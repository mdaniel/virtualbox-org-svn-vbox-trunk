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
#include "util/u_inlines.h"
#include "util/u_format.h"
#include "util/bitset.h"

#include "freedreno_program.h"

#include "fd5_program.h"
#include "fd5_emit.h"
#include "fd5_texture.h"
#include "fd5_format.h"

static void
delete_shader_stateobj(struct fd5_shader_stateobj *so)
{
	ir3_shader_destroy(so->shader);
	free(so);
}

static struct fd5_shader_stateobj *
create_shader_stateobj(struct pipe_context *pctx, const struct pipe_shader_state *cso,
		enum shader_t type)
{
	struct fd_context *ctx = fd_context(pctx);
	struct ir3_compiler *compiler = ctx->screen->compiler;
	struct fd5_shader_stateobj *so = CALLOC_STRUCT(fd5_shader_stateobj);
	so->shader = ir3_shader_create(compiler, cso, type, &ctx->debug);
	return so;
}

static void *
fd5_fp_state_create(struct pipe_context *pctx,
		const struct pipe_shader_state *cso)
{
	return create_shader_stateobj(pctx, cso, SHADER_FRAGMENT);
}

static void
fd5_fp_state_delete(struct pipe_context *pctx, void *hwcso)
{
	struct fd5_shader_stateobj *so = hwcso;
	delete_shader_stateobj(so);
}

static void *
fd5_vp_state_create(struct pipe_context *pctx,
		const struct pipe_shader_state *cso)
{
	return create_shader_stateobj(pctx, cso, SHADER_VERTEX);
}

static void
fd5_vp_state_delete(struct pipe_context *pctx, void *hwcso)
{
	struct fd5_shader_stateobj *so = hwcso;
	delete_shader_stateobj(so);
}

void
fd5_emit_shader(struct fd_ringbuffer *ring, const struct ir3_shader_variant *so)
{
	const struct ir3_info *si = &so->info;
	enum a4xx_state_block sb = fd4_stage2shadersb(so->type);
	enum a4xx_state_src src;
	uint32_t i, sz, *bin;

	if (fd_mesa_debug & FD_DBG_DIRECT) {
		sz = si->sizedwords;
		src = SS4_DIRECT;
		bin = fd_bo_map(so->bo);
	} else {
		sz = 0;
		src = SS4_INDIRECT;
		bin = NULL;
	}

	OUT_PKT7(ring, CP_LOAD_STATE4, 3 + sz);
	OUT_RING(ring, CP_LOAD_STATE4_0_DST_OFF(0) |
			CP_LOAD_STATE4_0_STATE_SRC(src) |
			CP_LOAD_STATE4_0_STATE_BLOCK(sb) |
			CP_LOAD_STATE4_0_NUM_UNIT(so->instrlen));
	if (bin) {
		OUT_RING(ring, CP_LOAD_STATE4_1_EXT_SRC_ADDR(0) |
				CP_LOAD_STATE4_1_STATE_TYPE(ST4_SHADER));
		OUT_RING(ring, CP_LOAD_STATE4_2_EXT_SRC_ADDR_HI(0));
	} else {
		OUT_RELOC(ring, so->bo, 0,
				CP_LOAD_STATE4_1_STATE_TYPE(ST4_SHADER), 0);
	}

	/* for how clever coverity is, it is sometimes rather dull, and
	 * doesn't realize that the only case where bin==NULL, sz==0:
	 */
	assume(bin || (sz == 0));

	for (i = 0; i < sz; i++) {
		OUT_RING(ring, bin[i]);
	}
}

/* Add any missing varyings needed for stream-out.  Otherwise varyings not
 * used by fragment shader will be stripped out.
 */
static void
link_stream_out(struct ir3_shader_linkage *l, const struct ir3_shader_variant *v)
{
	const struct pipe_stream_output_info *strmout = &v->shader->stream_output;

	/*
	 * First, any stream-out varyings not already in linkage map (ie. also
	 * consumed by frag shader) need to be added:
	 */
	for (unsigned i = 0; i < strmout->num_outputs; i++) {
		const struct pipe_stream_output *out = &strmout->output[i];
		unsigned k = out->register_index;
		unsigned compmask =
			(1 << (out->num_components + out->start_component)) - 1;
		unsigned idx, nextloc = 0;

		/* psize/pos need to be the last entries in linkage map, and will
		 * get added link_stream_out, so skip over them:
		 */
		if ((v->outputs[k].slot == VARYING_SLOT_PSIZ) ||
				(v->outputs[k].slot == VARYING_SLOT_POS))
			continue;

		for (idx = 0; idx < l->cnt; idx++) {
			if (l->var[idx].regid == v->outputs[k].regid)
				break;
			nextloc = MAX2(nextloc, l->var[idx].loc + 4);
		}

		/* add if not already in linkage map: */
		if (idx == l->cnt)
			ir3_link_add(l, v->outputs[k].regid, compmask, nextloc);

		/* expand component-mask if needed, ie streaming out all components
		 * but frag shader doesn't consume all components:
		 */
		if (compmask & ~l->var[idx].compmask) {
			l->var[idx].compmask |= compmask;
			l->max_loc = MAX2(l->max_loc,
				l->var[idx].loc + util_last_bit(l->var[idx].compmask));
		}
	}
}

/* TODO maybe some of this we could pre-compute once rather than having
 * so much draw-time logic?
 */
static void
emit_stream_out(struct fd_ringbuffer *ring, const struct ir3_shader_variant *v,
		struct ir3_shader_linkage *l)
{
	const struct pipe_stream_output_info *strmout = &v->shader->stream_output;
	unsigned ncomp[PIPE_MAX_SO_BUFFERS] = {0};
	unsigned prog[align(l->max_loc, 2) / 2];

	memset(prog, 0, sizeof(prog));

	for (unsigned i = 0; i < strmout->num_outputs; i++) {
		const struct pipe_stream_output *out = &strmout->output[i];
		unsigned k = out->register_index;
		unsigned idx;

		ncomp[out->output_buffer] += out->num_components;

		/* linkage map sorted by order frag shader wants things, so
		 * a bit less ideal here..
		 */
		for (idx = 0; idx < l->cnt; idx++)
			if (l->var[idx].regid == v->outputs[k].regid)
				break;

		debug_assert(idx < l->cnt);

		for (unsigned j = 0; j < out->num_components; j++) {
			unsigned c   = j + out->start_component;
			unsigned loc = l->var[idx].loc + c;
			unsigned off = j + out->dst_offset;  /* in dwords */

			if (loc & 1) {
				prog[loc/2] |= A5XX_VPC_SO_PROG_B_EN |
						A5XX_VPC_SO_PROG_B_BUF(out->output_buffer) |
						A5XX_VPC_SO_PROG_B_OFF(off * 4);
			} else {
				prog[loc/2] |= A5XX_VPC_SO_PROG_A_EN |
						A5XX_VPC_SO_PROG_A_BUF(out->output_buffer) |
						A5XX_VPC_SO_PROG_A_OFF(off * 4);
			}
		}
	}

	OUT_PKT7(ring, CP_CONTEXT_REG_BUNCH, 12 + (2 * ARRAY_SIZE(prog)));
	OUT_RING(ring, REG_A5XX_VPC_SO_BUF_CNTL);
	OUT_RING(ring, A5XX_VPC_SO_BUF_CNTL_ENABLE |
			COND(ncomp[0] > 0, A5XX_VPC_SO_BUF_CNTL_BUF0) |
			COND(ncomp[1] > 0, A5XX_VPC_SO_BUF_CNTL_BUF1) |
			COND(ncomp[2] > 0, A5XX_VPC_SO_BUF_CNTL_BUF2) |
			COND(ncomp[3] > 0, A5XX_VPC_SO_BUF_CNTL_BUF3));
	OUT_RING(ring, REG_A5XX_VPC_SO_NCOMP(0));
	OUT_RING(ring, ncomp[0]);
	OUT_RING(ring, REG_A5XX_VPC_SO_NCOMP(1));
	OUT_RING(ring, ncomp[1]);
	OUT_RING(ring, REG_A5XX_VPC_SO_NCOMP(2));
	OUT_RING(ring, ncomp[2]);
	OUT_RING(ring, REG_A5XX_VPC_SO_NCOMP(3));
	OUT_RING(ring, ncomp[3]);
	OUT_RING(ring, REG_A5XX_VPC_SO_CNTL);
	OUT_RING(ring, A5XX_VPC_SO_CNTL_ENABLE);
	for (unsigned i = 0; i < ARRAY_SIZE(prog); i++) {
		OUT_RING(ring, REG_A5XX_VPC_SO_PROG);
		OUT_RING(ring, prog[i]);
	}
}

struct stage {
	const struct ir3_shader_variant *v;
	const struct ir3_info *i;
	/* const sizes are in units of 4 * vec4 */
	uint8_t constoff;
	uint8_t constlen;
	/* instr sizes are in units of 16 instructions */
	uint8_t instroff;
	uint8_t instrlen;
};

enum {
	VS = 0,
	FS = 1,
	HS = 2,
	DS = 3,
	GS = 4,
	MAX_STAGES
};

static void
setup_stages(struct fd5_emit *emit, struct stage *s)
{
	unsigned i;

	s[VS].v = fd5_emit_get_vp(emit);
	s[FS].v = fd5_emit_get_fp(emit);

	s[HS].v = s[DS].v = s[GS].v = NULL;  /* for now */

	for (i = 0; i < MAX_STAGES; i++) {
		if (s[i].v) {
			s[i].i = &s[i].v->info;
			/* constlen is in units of 4 * vec4: */
			s[i].constlen = align(s[i].v->constlen, 4) / 4;
			/* instrlen is already in units of 16 instr.. although
			 * probably we should ditch that and not make the compiler
			 * care about instruction group size of a3xx vs a5xx
			 */
			s[i].instrlen = s[i].v->instrlen;
		} else {
			s[i].i = NULL;
			s[i].constlen = 0;
			s[i].instrlen = 0;
		}
	}

	/* NOTE: at least for gles2, blob partitions VS at bottom of const
	 * space and FS taking entire remaining space.  We probably don't
	 * need to do that the same way, but for now mimic what the blob
	 * does to make it easier to diff against register values from blob
	 *
	 * NOTE: if VS.instrlen + FS.instrlen > 64, then one or both shaders
	 * is run from external memory.
	 */
	if ((s[VS].instrlen + s[FS].instrlen) > 64) {
		/* prioritize FS for internal memory: */
		if (s[FS].instrlen < 64) {
			/* if FS can fit, kick VS out to external memory: */
			s[VS].instrlen = 0;
		} else if (s[VS].instrlen < 64) {
			/* otherwise if VS can fit, kick out FS: */
			s[FS].instrlen = 0;
		} else {
			/* neither can fit, run both from external memory: */
			s[VS].instrlen = 0;
			s[FS].instrlen = 0;
		}
	}

	unsigned constoff = 0;
	for (i = 0; i < MAX_STAGES; i++) {
		s[i].constoff = constoff;
		constoff += s[i].constlen;
	}

	s[VS].instroff = 0;
	s[FS].instroff = 64 - s[FS].instrlen;
	s[HS].instroff = s[DS].instroff = s[GS].instroff = s[FS].instroff;
}

void
fd5_program_emit(struct fd_context *ctx, struct fd_ringbuffer *ring,
				 struct fd5_emit *emit)
{
	struct stage s[MAX_STAGES];
	uint32_t pos_regid, psize_regid, color_regid[8];
	uint32_t face_regid, coord_regid, zwcoord_regid;
	uint32_t vcoord_regid, vertex_regid, instance_regid;
	enum a3xx_threadsize fssz;
	uint8_t psize_loc = ~0;
	int i, j;

	setup_stages(emit, s);

	fssz = (s[FS].i->max_reg >= 24) ? TWO_QUADS : FOUR_QUADS;

	pos_regid = ir3_find_output_regid(s[VS].v, VARYING_SLOT_POS);
	psize_regid = ir3_find_output_regid(s[VS].v, VARYING_SLOT_PSIZ);
	vertex_regid = ir3_find_sysval_regid(s[VS].v, SYSTEM_VALUE_VERTEX_ID);
	instance_regid = ir3_find_sysval_regid(s[VS].v, SYSTEM_VALUE_INSTANCE_ID);

	if (s[FS].v->color0_mrt) {
		color_regid[0] = color_regid[1] = color_regid[2] = color_regid[3] =
		color_regid[4] = color_regid[5] = color_regid[6] = color_regid[7] =
			ir3_find_output_regid(s[FS].v, FRAG_RESULT_COLOR);
	} else {
		color_regid[0] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA0);
		color_regid[1] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA1);
		color_regid[2] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA2);
		color_regid[3] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA3);
		color_regid[4] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA4);
		color_regid[5] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA5);
		color_regid[6] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA6);
		color_regid[7] = ir3_find_output_regid(s[FS].v, FRAG_RESULT_DATA7);
	}

	/* TODO get these dynamically: */
	face_regid = s[FS].v->frag_face ? regid(0,0) : regid(63,0);
	coord_regid = s[FS].v->frag_coord ? regid(0,0) : regid(63,0);
	zwcoord_regid = s[FS].v->frag_coord ? regid(0,2) : regid(63,0);
	vcoord_regid = (s[FS].v->total_in > 0) ? s[FS].v->pos_regid : regid(63,0);

	/* we could probably divide this up into things that need to be
	 * emitted if frag-prog is dirty vs if vert-prog is dirty..
	 */

	OUT_PKT4(ring, REG_A5XX_HLSQ_VS_CONFIG, 5);
	OUT_RING(ring, A5XX_HLSQ_VS_CONFIG_CONSTOBJECTOFFSET(s[VS].constoff) |
			A5XX_HLSQ_VS_CONFIG_SHADEROBJOFFSET(s[VS].instroff) |
			COND(s[VS].v, A5XX_HLSQ_VS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_HLSQ_FS_CONFIG_CONSTOBJECTOFFSET(s[FS].constoff) |
			A5XX_HLSQ_FS_CONFIG_SHADEROBJOFFSET(s[FS].instroff) |
			COND(s[FS].v, A5XX_HLSQ_FS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_HLSQ_HS_CONFIG_CONSTOBJECTOFFSET(s[HS].constoff) |
			A5XX_HLSQ_HS_CONFIG_SHADEROBJOFFSET(s[HS].instroff) |
			COND(s[HS].v, A5XX_HLSQ_HS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_HLSQ_DS_CONFIG_CONSTOBJECTOFFSET(s[DS].constoff) |
			A5XX_HLSQ_DS_CONFIG_SHADEROBJOFFSET(s[DS].instroff) |
			COND(s[DS].v, A5XX_HLSQ_DS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_HLSQ_GS_CONFIG_CONSTOBJECTOFFSET(s[GS].constoff) |
			A5XX_HLSQ_GS_CONFIG_SHADEROBJOFFSET(s[GS].instroff) |
			COND(s[GS].v, A5XX_HLSQ_GS_CONFIG_ENABLED));

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CONFIG, 1);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_HLSQ_VS_CNTL, 5);
	OUT_RING(ring, A5XX_HLSQ_VS_CNTL_INSTRLEN(s[VS].instrlen) |
			COND(s[VS].v && s[VS].v->has_ssbo, A5XX_HLSQ_VS_CNTL_SSBO_ENABLE));
	OUT_RING(ring, A5XX_HLSQ_FS_CNTL_INSTRLEN(s[FS].instrlen) |
			COND(s[FS].v && s[FS].v->has_ssbo, A5XX_HLSQ_FS_CNTL_SSBO_ENABLE));
	OUT_RING(ring, A5XX_HLSQ_HS_CNTL_INSTRLEN(s[HS].instrlen) |
			COND(s[HS].v && s[HS].v->has_ssbo, A5XX_HLSQ_HS_CNTL_SSBO_ENABLE));
	OUT_RING(ring, A5XX_HLSQ_DS_CNTL_INSTRLEN(s[DS].instrlen) |
			COND(s[DS].v && s[DS].v->has_ssbo, A5XX_HLSQ_DS_CNTL_SSBO_ENABLE));
	OUT_RING(ring, A5XX_HLSQ_GS_CNTL_INSTRLEN(s[GS].instrlen) |
			COND(s[GS].v && s[GS].v->has_ssbo, A5XX_HLSQ_GS_CNTL_SSBO_ENABLE));

	OUT_PKT4(ring, REG_A5XX_SP_VS_CONFIG, 5);
	OUT_RING(ring, A5XX_SP_VS_CONFIG_CONSTOBJECTOFFSET(s[VS].constoff) |
			A5XX_SP_VS_CONFIG_SHADEROBJOFFSET(s[VS].instroff) |
			COND(s[VS].v, A5XX_SP_VS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_SP_FS_CONFIG_CONSTOBJECTOFFSET(s[FS].constoff) |
			A5XX_SP_FS_CONFIG_SHADEROBJOFFSET(s[FS].instroff) |
			COND(s[FS].v, A5XX_SP_FS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_SP_HS_CONFIG_CONSTOBJECTOFFSET(s[HS].constoff) |
			A5XX_SP_HS_CONFIG_SHADEROBJOFFSET(s[HS].instroff) |
			COND(s[HS].v, A5XX_SP_HS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_SP_DS_CONFIG_CONSTOBJECTOFFSET(s[DS].constoff) |
			A5XX_SP_DS_CONFIG_SHADEROBJOFFSET(s[DS].instroff) |
			COND(s[DS].v, A5XX_SP_DS_CONFIG_ENABLED));
	OUT_RING(ring, A5XX_SP_GS_CONFIG_CONSTOBJECTOFFSET(s[GS].constoff) |
			A5XX_SP_GS_CONFIG_SHADEROBJOFFSET(s[GS].instroff) |
			COND(s[GS].v, A5XX_SP_GS_CONFIG_ENABLED));

	OUT_PKT4(ring, REG_A5XX_SP_CS_CONFIG, 1);
	OUT_RING(ring, 0x00000000);

	OUT_PKT4(ring, REG_A5XX_HLSQ_VS_CONSTLEN, 2);
	OUT_RING(ring, s[VS].constlen);    /* HLSQ_VS_CONSTLEN */
	OUT_RING(ring, s[VS].instrlen);    /* HLSQ_VS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_HLSQ_FS_CONSTLEN, 2);
	OUT_RING(ring, s[FS].constlen);    /* HLSQ_FS_CONSTLEN */
	OUT_RING(ring, s[FS].instrlen);    /* HLSQ_FS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_HLSQ_HS_CONSTLEN, 2);
	OUT_RING(ring, s[HS].constlen);    /* HLSQ_HS_CONSTLEN */
	OUT_RING(ring, s[HS].instrlen);    /* HLSQ_HS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_HLSQ_DS_CONSTLEN, 2);
	OUT_RING(ring, s[DS].constlen);    /* HLSQ_DS_CONSTLEN */
	OUT_RING(ring, s[DS].instrlen);    /* HLSQ_DS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_HLSQ_GS_CONSTLEN, 2);
	OUT_RING(ring, s[GS].constlen);    /* HLSQ_GS_CONSTLEN */
	OUT_RING(ring, s[GS].instrlen);    /* HLSQ_GS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_HLSQ_CS_CONSTLEN, 2);
	OUT_RING(ring, 0x00000000);        /* HLSQ_CS_CONSTLEN */
	OUT_RING(ring, 0x00000000);        /* HLSQ_CS_INSTRLEN */

	OUT_PKT4(ring, REG_A5XX_SP_VS_CTRL_REG0, 1);
	OUT_RING(ring, A5XX_SP_VS_CTRL_REG0_HALFREGFOOTPRINT(s[VS].i->max_half_reg + 1) |
			A5XX_SP_VS_CTRL_REG0_FULLREGFOOTPRINT(s[VS].i->max_reg + 1) |
			0x6 | /* XXX seems to be always set? */
			A5XX_SP_VS_CTRL_REG0_BRANCHSTACK(0x3) |  // XXX need to figure this out somehow..
			COND(s[VS].v->has_samp, A5XX_SP_VS_CTRL_REG0_PIXLODENABLE));

	struct ir3_shader_linkage l = {0};
	ir3_link_shaders(&l, s[VS].v, s[FS].v);

	if ((s[VS].v->shader->stream_output.num_outputs > 0) &&
			!emit->key.binning_pass)
		link_stream_out(&l, s[VS].v);

	BITSET_DECLARE(varbs, 128) = {0};
	uint32_t *varmask = (uint32_t *)varbs;

	for (i = 0; i < l.cnt; i++)
		for (j = 0; j < util_last_bit(l.var[i].compmask); j++)
			BITSET_SET(varbs, l.var[i].loc + j);

	OUT_PKT4(ring, REG_A5XX_VPC_VAR_DISABLE(0), 4);
	OUT_RING(ring, ~varmask[0]);  /* VPC_VAR[0].DISABLE */
	OUT_RING(ring, ~varmask[1]);  /* VPC_VAR[1].DISABLE */
	OUT_RING(ring, ~varmask[2]);  /* VPC_VAR[2].DISABLE */
	OUT_RING(ring, ~varmask[3]);  /* VPC_VAR[3].DISABLE */

	/* a5xx appends pos/psize to end of the linkage map: */
	if (pos_regid != regid(63,0))
		ir3_link_add(&l, pos_regid, 0xf, l.max_loc);

	if (psize_regid != regid(63,0)) {
		psize_loc = l.max_loc;
		ir3_link_add(&l, psize_regid, 0x1, l.max_loc);
	}

	if ((s[VS].v->shader->stream_output.num_outputs > 0) &&
			!emit->key.binning_pass) {
		emit_stream_out(ring, s[VS].v, &l);

		OUT_PKT4(ring, REG_A5XX_VPC_SO_OVERRIDE, 1);
		OUT_RING(ring, 0x00000000);
	} else {
		OUT_PKT4(ring, REG_A5XX_VPC_SO_OVERRIDE, 1);
		OUT_RING(ring, A5XX_VPC_SO_OVERRIDE_SO_DISABLE);
	}

	for (i = 0, j = 0; (i < 16) && (j < l.cnt); i++) {
		uint32_t reg = 0;

		OUT_PKT4(ring, REG_A5XX_SP_VS_OUT_REG(i), 1);

		reg |= A5XX_SP_VS_OUT_REG_A_REGID(l.var[j].regid);
		reg |= A5XX_SP_VS_OUT_REG_A_COMPMASK(l.var[j].compmask);
		j++;

		reg |= A5XX_SP_VS_OUT_REG_B_REGID(l.var[j].regid);
		reg |= A5XX_SP_VS_OUT_REG_B_COMPMASK(l.var[j].compmask);
		j++;

		OUT_RING(ring, reg);
	}

	for (i = 0, j = 0; (i < 8) && (j < l.cnt); i++) {
		uint32_t reg = 0;

		OUT_PKT4(ring, REG_A5XX_SP_VS_VPC_DST_REG(i), 1);

		reg |= A5XX_SP_VS_VPC_DST_REG_OUTLOC0(l.var[j++].loc);
		reg |= A5XX_SP_VS_VPC_DST_REG_OUTLOC1(l.var[j++].loc);
		reg |= A5XX_SP_VS_VPC_DST_REG_OUTLOC2(l.var[j++].loc);
		reg |= A5XX_SP_VS_VPC_DST_REG_OUTLOC3(l.var[j++].loc);

		OUT_RING(ring, reg);
	}

	OUT_PKT4(ring, REG_A5XX_SP_VS_OBJ_START_LO, 2);
	OUT_RELOC(ring, s[VS].v->bo, 0, 0, 0);  /* SP_VS_OBJ_START_LO/HI */

	if (s[VS].instrlen)
		fd5_emit_shader(ring, s[VS].v);

	// TODO depending on other bits in this reg (if any) set somewhere else?
	OUT_PKT4(ring, REG_A5XX_PC_PRIM_VTX_CNTL, 1);
	OUT_RING(ring, COND(s[VS].v->writes_psize, A5XX_PC_PRIM_VTX_CNTL_PSIZE));

	OUT_PKT4(ring, REG_A5XX_SP_PRIMITIVE_CNTL, 1);
	OUT_RING(ring, A5XX_SP_PRIMITIVE_CNTL_VSOUT(l.cnt));

	OUT_PKT4(ring, REG_A5XX_VPC_CNTL_0, 1);
	OUT_RING(ring, A5XX_VPC_CNTL_0_STRIDE_IN_VPC(l.max_loc) |
			COND(s[FS].v->total_in > 0, A5XX_VPC_CNTL_0_VARYING) |
			COND(s[FS].v->frag_coord, A5XX_VPC_CNTL_0_VARYING) |
			0x10000);    // XXX

	fd5_context(ctx)->max_loc = l.max_loc;

	if (emit->key.binning_pass) {
		OUT_PKT4(ring, REG_A5XX_SP_FS_OBJ_START_LO, 2);
		OUT_RING(ring, 0x00000000);    /* SP_FS_OBJ_START_LO */
		OUT_RING(ring, 0x00000000);    /* SP_FS_OBJ_START_HI */
	} else {
		OUT_PKT4(ring, REG_A5XX_SP_FS_OBJ_START_LO, 2);
		OUT_RELOC(ring, s[FS].v->bo, 0, 0, 0);  /* SP_FS_OBJ_START_LO/HI */
	}

	OUT_PKT4(ring, REG_A5XX_HLSQ_CONTROL_0_REG, 5);
	OUT_RING(ring, A5XX_HLSQ_CONTROL_0_REG_FSTHREADSIZE(fssz) |
			A5XX_HLSQ_CONTROL_0_REG_CSTHREADSIZE(TWO_QUADS) |
			0x00000880);               /* XXX HLSQ_CONTROL_0 */
	OUT_RING(ring, A5XX_HLSQ_CONTROL_1_REG_PRIMALLOCTHRESHOLD(63));
	OUT_RING(ring, A5XX_HLSQ_CONTROL_2_REG_FACEREGID(face_regid) |
			0xfcfcfc00);               /* XXX */
	OUT_RING(ring, A5XX_HLSQ_CONTROL_3_REG_FRAGCOORDXYREGID(vcoord_regid) |
			0xfcfcfc00);               /* XXX */
	OUT_RING(ring, A5XX_HLSQ_CONTROL_4_REG_XYCOORDREGID(coord_regid) |
			A5XX_HLSQ_CONTROL_4_REG_ZWCOORDREGID(zwcoord_regid) |
			0x0000fcfc);               /* XXX */

	OUT_PKT4(ring, REG_A5XX_SP_FS_CTRL_REG0, 1);
	OUT_RING(ring, COND(s[FS].v->total_in > 0, A5XX_SP_FS_CTRL_REG0_VARYING) |
			COND(s[FS].v->frag_coord, A5XX_SP_FS_CTRL_REG0_VARYING) |
			0x40006 | /* XXX set pretty much everywhere */
			A5XX_SP_FS_CTRL_REG0_THREADSIZE(fssz) |
			A5XX_SP_FS_CTRL_REG0_HALFREGFOOTPRINT(s[FS].i->max_half_reg + 1) |
			A5XX_SP_FS_CTRL_REG0_FULLREGFOOTPRINT(s[FS].i->max_reg + 1) |
			A5XX_SP_FS_CTRL_REG0_BRANCHSTACK(0x3) |  // XXX need to figure this out somehow..
			COND(s[FS].v->has_samp, A5XX_SP_FS_CTRL_REG0_PIXLODENABLE));

	OUT_PKT4(ring, REG_A5XX_HLSQ_UPDATE_CNTL, 1);
	OUT_RING(ring, 0x020fffff);        /* XXX */

	OUT_PKT4(ring, REG_A5XX_VPC_GS_SIV_CNTL, 1);
	OUT_RING(ring, 0x0000ffff);        /* XXX */

	OUT_PKT4(ring, REG_A5XX_SP_SP_CNTL, 1);
	OUT_RING(ring, 0x00000010);        /* XXX */

	OUT_PKT4(ring, REG_A5XX_GRAS_CNTL, 1);
	OUT_RING(ring, COND(s[FS].v->total_in > 0, A5XX_GRAS_CNTL_VARYING) |
			COND(s[FS].v->frag_coord, A5XX_GRAS_CNTL_XCOORD |
					A5XX_GRAS_CNTL_YCOORD |
					A5XX_GRAS_CNTL_ZCOORD |
					A5XX_GRAS_CNTL_WCOORD |
					A5XX_GRAS_CNTL_UNK3) |
			COND(s[FS].v->frag_face, A5XX_GRAS_CNTL_UNK3));

	OUT_PKT4(ring, REG_A5XX_RB_RENDER_CONTROL0, 2);
	OUT_RING(ring, COND(s[FS].v->total_in > 0, A5XX_RB_RENDER_CONTROL0_VARYING) |
			COND(s[FS].v->frag_coord, A5XX_RB_RENDER_CONTROL0_XCOORD |
					A5XX_RB_RENDER_CONTROL0_YCOORD |
					A5XX_RB_RENDER_CONTROL0_ZCOORD |
					A5XX_RB_RENDER_CONTROL0_WCOORD |
					A5XX_RB_RENDER_CONTROL0_UNK3) |
			COND(s[FS].v->frag_face, A5XX_RB_RENDER_CONTROL0_UNK3));
	OUT_RING(ring, COND(s[FS].v->frag_face, A5XX_RB_RENDER_CONTROL1_FACENESS));

	OUT_PKT4(ring, REG_A5XX_SP_FS_OUTPUT_REG(0), 8);
	for (i = 0; i < 8; i++) {
		OUT_RING(ring, A5XX_SP_FS_OUTPUT_REG_REGID(color_regid[i]) |
				COND(emit->key.half_precision,
					A5XX_SP_FS_OUTPUT_REG_HALF_PRECISION));
	}


	OUT_PKT4(ring, REG_A5XX_VPC_PACK, 1);
	OUT_RING(ring, A5XX_VPC_PACK_NUMNONPOSVAR(s[FS].v->total_in) |
			A5XX_VPC_PACK_PSIZELOC(psize_loc));

	if (!emit->key.binning_pass) {
		uint32_t vinterp[8], vpsrepl[8];

		memset(vinterp, 0, sizeof(vinterp));
		memset(vpsrepl, 0, sizeof(vpsrepl));

		/* looks like we need to do int varyings in the frag
		 * shader on a5xx (no flatshad reg?  or a420.0 bug?):
		 *
		 *    (sy)(ss)nop
		 *    (sy)ldlv.u32 r0.x,l[r0.x], 1
		 *    ldlv.u32 r0.y,l[r0.x+1], 1
		 *    (ss)bary.f (ei)r63.x, 0, r0.x
		 *    (ss)(rpt1)cov.s32f16 hr0.x, (r)r0.x
		 *    (rpt5)nop
		 *    sam (f16)(xyzw)hr0.x, hr0.x, s#0, t#0
		 *
		 * Possibly on later a5xx variants we'll be able to use
		 * something like the code below instead of workaround
		 * in the shader:
		 */
		/* figure out VARYING_INTERP / VARYING_PS_REPL register values: */
		for (j = -1; (j = ir3_next_varying(s[FS].v, j)) < (int)s[FS].v->inputs_count; ) {
			/* NOTE: varyings are packed, so if compmask is 0xb
			 * then first, third, and fourth component occupy
			 * three consecutive varying slots:
			 */
			unsigned compmask = s[FS].v->inputs[j].compmask;

			uint32_t inloc = s[FS].v->inputs[j].inloc;

			if ((s[FS].v->inputs[j].interpolate == INTERP_MODE_FLAT) ||
					(s[FS].v->inputs[j].rasterflat && emit->rasterflat)) {
				uint32_t loc = inloc;

				for (i = 0; i < 4; i++) {
					if (compmask & (1 << i)) {
						vinterp[loc / 16] |= 1 << ((loc % 16) * 2);
						//flatshade[loc / 32] |= 1 << (loc % 32);
						loc++;
					}
				}
			}

			gl_varying_slot slot = s[FS].v->inputs[j].slot;

			/* since we don't enable PIPE_CAP_TGSI_TEXCOORD: */
			if (slot >= VARYING_SLOT_VAR0) {
				unsigned texmask = 1 << (slot - VARYING_SLOT_VAR0);
				/* Replace the .xy coordinates with S/T from the point sprite. Set
				 * interpolation bits for .zw such that they become .01
				 */
				if (emit->sprite_coord_enable & texmask) {
					/* mask is two 2-bit fields, where:
					 *   '01' -> S
					 *   '10' -> T
					 *   '11' -> 1 - T  (flip mode)
					 */
					unsigned mask = emit->sprite_coord_mode ? 0b1101 : 0b1001;
					uint32_t loc = inloc;
					if (compmask & 0x1) {
						vpsrepl[loc / 16] |= ((mask >> 0) & 0x3) << ((loc % 16) * 2);
						loc++;
					}
					if (compmask & 0x2) {
						vpsrepl[loc / 16] |= ((mask >> 2) & 0x3) << ((loc % 16) * 2);
						loc++;
					}
					if (compmask & 0x4) {
						/* .z <- 0.0f */
						vinterp[loc / 16] |= 0b10 << ((loc % 16) * 2);
						loc++;
					}
					if (compmask & 0x8) {
						/* .w <- 1.0f */
						vinterp[loc / 16] |= 0b11 << ((loc % 16) * 2);
						loc++;
					}
				}
			}
		}

		OUT_PKT4(ring, REG_A5XX_VPC_VARYING_INTERP_MODE(0), 8);
		for (i = 0; i < 8; i++)
			OUT_RING(ring, vinterp[i]);     /* VPC_VARYING_INTERP[i].MODE */

		OUT_PKT4(ring, REG_A5XX_VPC_VARYING_PS_REPL_MODE(0), 8);
		for (i = 0; i < 8; i++)
			OUT_RING(ring, vpsrepl[i]);   /* VPC_VARYING_PS_REPL[i] */
	}

	if (!emit->key.binning_pass)
		if (s[FS].instrlen)
			fd5_emit_shader(ring, s[FS].v);

	OUT_PKT4(ring, REG_A5XX_VFD_CONTROL_1, 5);
	OUT_RING(ring, A5XX_VFD_CONTROL_1_REGID4VTX(vertex_regid) |
			A5XX_VFD_CONTROL_1_REGID4INST(instance_regid) |
			0xfc0000);
	OUT_RING(ring, 0x0000fcfc);   /* VFD_CONTROL_2 */
	OUT_RING(ring, 0x0000fcfc);   /* VFD_CONTROL_3 */
	OUT_RING(ring, 0x000000fc);   /* VFD_CONTROL_4 */
	OUT_RING(ring, 0x00000000);   /* VFD_CONTROL_5 */
}

void
fd5_prog_init(struct pipe_context *pctx)
{
	pctx->create_fs_state = fd5_fp_state_create;
	pctx->delete_fs_state = fd5_fp_state_delete;

	pctx->create_vs_state = fd5_vp_state_create;
	pctx->delete_vs_state = fd5_vp_state_delete;

	fd_prog_init(pctx);
}
