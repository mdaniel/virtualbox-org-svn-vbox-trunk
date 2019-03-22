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

#include "pipe/p_state.h"
#include "util/u_string.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_format.h"

#include "freedreno_program.h"

#include "fd3_program.h"
#include "fd3_emit.h"
#include "fd3_texture.h"
#include "fd3_format.h"

static void
delete_shader_stateobj(struct fd3_shader_stateobj *so)
{
	ir3_shader_destroy(so->shader);
	free(so);
}

static struct fd3_shader_stateobj *
create_shader_stateobj(struct pipe_context *pctx, const struct pipe_shader_state *cso,
		enum shader_t type)
{
	struct fd_context *ctx = fd_context(pctx);
	struct ir3_compiler *compiler = ctx->screen->compiler;
	struct fd3_shader_stateobj *so = CALLOC_STRUCT(fd3_shader_stateobj);
	so->shader = ir3_shader_create(compiler, cso, type, &ctx->debug);
	return so;
}

static void *
fd3_fp_state_create(struct pipe_context *pctx,
		const struct pipe_shader_state *cso)
{
	return create_shader_stateobj(pctx, cso, SHADER_FRAGMENT);
}

static void
fd3_fp_state_delete(struct pipe_context *pctx, void *hwcso)
{
	struct fd3_shader_stateobj *so = hwcso;
	delete_shader_stateobj(so);
}

static void *
fd3_vp_state_create(struct pipe_context *pctx,
		const struct pipe_shader_state *cso)
{
	return create_shader_stateobj(pctx, cso, SHADER_VERTEX);
}

static void
fd3_vp_state_delete(struct pipe_context *pctx, void *hwcso)
{
	struct fd3_shader_stateobj *so = hwcso;
	delete_shader_stateobj(so);
}

bool
fd3_needs_manual_clipping(const struct fd3_shader_stateobj *so,
						  const struct pipe_rasterizer_state *rast)
{
	uint64_t outputs = ir3_shader_outputs(so->shader);

	return (!rast->depth_clip ||
			util_bitcount(rast->clip_plane_enable) > 6 ||
			outputs & ((1ULL << VARYING_SLOT_CLIP_VERTEX) |
					   (1ULL << VARYING_SLOT_CLIP_DIST0) |
					   (1ULL << VARYING_SLOT_CLIP_DIST1)));
}


static void
emit_shader(struct fd_ringbuffer *ring, const struct ir3_shader_variant *so)
{
	const struct ir3_info *si = &so->info;
	enum adreno_state_block sb;
	enum adreno_state_src src;
	uint32_t i, sz, *bin;

	if (so->type == SHADER_VERTEX) {
		sb = SB_VERT_SHADER;
	} else {
		sb = SB_FRAG_SHADER;
	}

	if (fd_mesa_debug & FD_DBG_DIRECT) {
		sz = si->sizedwords;
		src = SS_DIRECT;
		bin = fd_bo_map(so->bo);
	} else {
		sz = 0;
		src = SS_INDIRECT;
		bin = NULL;
	}

	OUT_PKT3(ring, CP_LOAD_STATE, 2 + sz);
	OUT_RING(ring, CP_LOAD_STATE_0_DST_OFF(0) |
			CP_LOAD_STATE_0_STATE_SRC(src) |
			CP_LOAD_STATE_0_STATE_BLOCK(sb) |
			CP_LOAD_STATE_0_NUM_UNIT(so->instrlen));
	if (bin) {
		OUT_RING(ring, CP_LOAD_STATE_1_EXT_SRC_ADDR(0) |
				CP_LOAD_STATE_1_STATE_TYPE(ST_SHADER));
	} else {
		OUT_RELOC(ring, so->bo, 0,
				CP_LOAD_STATE_1_STATE_TYPE(ST_SHADER), 0);
	}
	for (i = 0; i < sz; i++) {
		OUT_RING(ring, bin[i]);
	}
}

void
fd3_program_emit(struct fd_ringbuffer *ring, struct fd3_emit *emit,
				 int nr, struct pipe_surface **bufs)
{
	const struct ir3_shader_variant *vp, *fp;
	const struct ir3_info *vsi, *fsi;
	enum a3xx_instrbuffermode fpbuffer, vpbuffer;
	uint32_t fpbuffersz, vpbuffersz, fsoff;
	uint32_t pos_regid, posz_regid, psize_regid, color_regid[4] = {0};
	int constmode;
	int i, j;

	debug_assert(nr <= ARRAY_SIZE(color_regid));

	vp = fd3_emit_get_vp(emit);
	fp = fd3_emit_get_fp(emit);

	vsi = &vp->info;
	fsi = &fp->info;

	fpbuffer = BUFFER;
	vpbuffer = BUFFER;
	fpbuffersz = fp->instrlen;
	vpbuffersz = vp->instrlen;

	/*
	 * Decide whether to use BUFFER or CACHE mode for VS and FS.  It
	 * appears like 256 is the hard limit, but when the combined size
	 * exceeds 128 then blob will try to keep FS in BUFFER mode and
	 * switch to CACHE for VS until VS is too large.  The blob seems
	 * to switch FS out of BUFFER mode at slightly under 128.  But
	 * a bit fuzzy on the decision tree, so use slightly conservative
	 * limits.
	 *
	 * TODO check if these thresholds for BUFFER vs CACHE mode are the
	 *      same for all a3xx or whether we need to consider the gpuid
	 */

	if ((fpbuffersz + vpbuffersz) > 128) {
		if (fpbuffersz < 112) {
			/* FP:BUFFER   VP:CACHE  */
			vpbuffer = CACHE;
			vpbuffersz = 256 - fpbuffersz;
		} else if (vpbuffersz < 112) {
			/* FP:CACHE    VP:BUFFER */
			fpbuffer = CACHE;
			fpbuffersz = 256 - vpbuffersz;
		} else {
			/* FP:CACHE    VP:CACHE  */
			vpbuffer = fpbuffer = CACHE;
			vpbuffersz = fpbuffersz = 192;
		}
	}

	if (fpbuffer == BUFFER) {
		fsoff = 128 - fpbuffersz;
	} else {
		fsoff = 256 - fpbuffersz;
	}

	/* seems like vs->constlen + fs->constlen > 256, then CONSTMODE=1 */
	constmode = ((vp->constlen + fp->constlen) > 256) ? 1 : 0;

	pos_regid = ir3_find_output_regid(vp, VARYING_SLOT_POS);
	posz_regid = ir3_find_output_regid(fp, FRAG_RESULT_DEPTH);
	psize_regid = ir3_find_output_regid(vp, VARYING_SLOT_PSIZ);
	if (fp->color0_mrt) {
		color_regid[0] = color_regid[1] = color_regid[2] = color_regid[3] =
			ir3_find_output_regid(fp, FRAG_RESULT_COLOR);
	} else {
		color_regid[0] = ir3_find_output_regid(fp, FRAG_RESULT_DATA0);
		color_regid[1] = ir3_find_output_regid(fp, FRAG_RESULT_DATA1);
		color_regid[2] = ir3_find_output_regid(fp, FRAG_RESULT_DATA2);
		color_regid[3] = ir3_find_output_regid(fp, FRAG_RESULT_DATA3);
	}

	/* adjust regids for alpha output formats. there is no alpha render
	 * format, so it's just treated like red
	 */
	for (i = 0; i < nr; i++)
		if (util_format_is_alpha(pipe_surface_format(bufs[i])))
			color_regid[i] += 3;

	/* we could probably divide this up into things that need to be
	 * emitted if frag-prog is dirty vs if vert-prog is dirty..
	 */

	OUT_PKT0(ring, REG_A3XX_HLSQ_CONTROL_0_REG, 6);
	OUT_RING(ring, A3XX_HLSQ_CONTROL_0_REG_FSTHREADSIZE(FOUR_QUADS) |
			A3XX_HLSQ_CONTROL_0_REG_CONSTMODE(constmode) |
			/* NOTE:  I guess SHADERRESTART and CONSTFULLUPDATE maybe
			 * flush some caches? I think we only need to set those
			 * bits if we have updated const or shader..
			 */
			A3XX_HLSQ_CONTROL_0_REG_SPSHADERRESTART |
			A3XX_HLSQ_CONTROL_0_REG_SPCONSTFULLUPDATE);
	OUT_RING(ring, A3XX_HLSQ_CONTROL_1_REG_VSTHREADSIZE(TWO_QUADS) |
			A3XX_HLSQ_CONTROL_1_REG_VSSUPERTHREADENABLE |
			COND(fp->frag_coord, A3XX_HLSQ_CONTROL_1_REG_FRAGCOORDXYREGID(regid(0,0)) |
					A3XX_HLSQ_CONTROL_1_REG_FRAGCOORDZWREGID(regid(0,2))));
	OUT_RING(ring, A3XX_HLSQ_CONTROL_2_REG_PRIMALLOCTHRESHOLD(31));
	OUT_RING(ring, A3XX_HLSQ_CONTROL_3_REG_REGID(fp->pos_regid));
	OUT_RING(ring, A3XX_HLSQ_VS_CONTROL_REG_CONSTLENGTH(vp->constlen) |
			A3XX_HLSQ_VS_CONTROL_REG_CONSTSTARTOFFSET(0) |
			A3XX_HLSQ_VS_CONTROL_REG_INSTRLENGTH(vpbuffersz));
	OUT_RING(ring, A3XX_HLSQ_FS_CONTROL_REG_CONSTLENGTH(fp->constlen) |
			A3XX_HLSQ_FS_CONTROL_REG_CONSTSTARTOFFSET(128) |
			A3XX_HLSQ_FS_CONTROL_REG_INSTRLENGTH(fpbuffersz));

	OUT_PKT0(ring, REG_A3XX_SP_SP_CTRL_REG, 1);
	OUT_RING(ring, A3XX_SP_SP_CTRL_REG_CONSTMODE(constmode) |
			COND(emit->key.binning_pass, A3XX_SP_SP_CTRL_REG_BINNING) |
			A3XX_SP_SP_CTRL_REG_SLEEPMODE(1) |
			A3XX_SP_SP_CTRL_REG_L0MODE(0));

	OUT_PKT0(ring, REG_A3XX_SP_VS_LENGTH_REG, 1);
	OUT_RING(ring, A3XX_SP_VS_LENGTH_REG_SHADERLENGTH(vp->instrlen));

	OUT_PKT0(ring, REG_A3XX_SP_VS_CTRL_REG0, 3);
	OUT_RING(ring, A3XX_SP_VS_CTRL_REG0_THREADMODE(MULTI) |
			A3XX_SP_VS_CTRL_REG0_INSTRBUFFERMODE(vpbuffer) |
			COND(vpbuffer == CACHE, A3XX_SP_VS_CTRL_REG0_CACHEINVALID) |
			A3XX_SP_VS_CTRL_REG0_HALFREGFOOTPRINT(vsi->max_half_reg + 1) |
			A3XX_SP_VS_CTRL_REG0_FULLREGFOOTPRINT(vsi->max_reg + 1) |
			A3XX_SP_VS_CTRL_REG0_THREADSIZE(TWO_QUADS) |
			A3XX_SP_VS_CTRL_REG0_SUPERTHREADMODE |
			A3XX_SP_VS_CTRL_REG0_LENGTH(vpbuffersz));
	OUT_RING(ring, A3XX_SP_VS_CTRL_REG1_CONSTLENGTH(vp->constlen) |
			A3XX_SP_VS_CTRL_REG1_INITIALOUTSTANDING(vp->total_in) |
			A3XX_SP_VS_CTRL_REG1_CONSTFOOTPRINT(MAX2(vp->constlen + 1, 0)));
	OUT_RING(ring, A3XX_SP_VS_PARAM_REG_POSREGID(pos_regid) |
			A3XX_SP_VS_PARAM_REG_PSIZEREGID(psize_regid) |
			A3XX_SP_VS_PARAM_REG_TOTALVSOUTVAR(fp->varying_in));

	struct ir3_shader_linkage l = {0};
	ir3_link_shaders(&l, vp, fp);

	for (i = 0, j = 0; (i < 16) && (j < l.cnt); i++) {
		uint32_t reg = 0;

		OUT_PKT0(ring, REG_A3XX_SP_VS_OUT_REG(i), 1);

		reg |= A3XX_SP_VS_OUT_REG_A_REGID(l.var[j].regid);
		reg |= A3XX_SP_VS_OUT_REG_A_COMPMASK(l.var[j].compmask);
		j++;

		reg |= A3XX_SP_VS_OUT_REG_B_REGID(l.var[j].regid);
		reg |= A3XX_SP_VS_OUT_REG_B_COMPMASK(l.var[j].compmask);
		j++;

		OUT_RING(ring, reg);
	}

	for (i = 0, j = 0; (i < 8) && (j < l.cnt); i++) {
		uint32_t reg = 0;

		OUT_PKT0(ring, REG_A3XX_SP_VS_VPC_DST_REG(i), 1);

		reg |= A3XX_SP_VS_VPC_DST_REG_OUTLOC0(l.var[j++].loc + 8);
		reg |= A3XX_SP_VS_VPC_DST_REG_OUTLOC1(l.var[j++].loc + 8);
		reg |= A3XX_SP_VS_VPC_DST_REG_OUTLOC2(l.var[j++].loc + 8);
		reg |= A3XX_SP_VS_VPC_DST_REG_OUTLOC3(l.var[j++].loc + 8);

		OUT_RING(ring, reg);
	}

	OUT_PKT0(ring, REG_A3XX_SP_VS_OBJ_OFFSET_REG, 2);
	OUT_RING(ring, A3XX_SP_VS_OBJ_OFFSET_REG_CONSTOBJECTOFFSET(0) |
			A3XX_SP_VS_OBJ_OFFSET_REG_SHADEROBJOFFSET(0));
	OUT_RELOC(ring, vp->bo, 0, 0, 0);  /* SP_VS_OBJ_START_REG */

	if (emit->key.binning_pass) {
		OUT_PKT0(ring, REG_A3XX_SP_FS_LENGTH_REG, 1);
		OUT_RING(ring, 0x00000000);

		OUT_PKT0(ring, REG_A3XX_SP_FS_CTRL_REG0, 2);
		OUT_RING(ring, A3XX_SP_FS_CTRL_REG0_THREADMODE(MULTI) |
				A3XX_SP_FS_CTRL_REG0_INSTRBUFFERMODE(BUFFER));
		OUT_RING(ring, 0x00000000);

		OUT_PKT0(ring, REG_A3XX_SP_FS_OBJ_OFFSET_REG, 1);
		OUT_RING(ring, A3XX_SP_FS_OBJ_OFFSET_REG_CONSTOBJECTOFFSET(128) |
				A3XX_SP_FS_OBJ_OFFSET_REG_SHADEROBJOFFSET(0));
	} else {
		OUT_PKT0(ring, REG_A3XX_SP_FS_LENGTH_REG, 1);
		OUT_RING(ring, A3XX_SP_FS_LENGTH_REG_SHADERLENGTH(fp->instrlen));

		OUT_PKT0(ring, REG_A3XX_SP_FS_CTRL_REG0, 2);
		OUT_RING(ring, A3XX_SP_FS_CTRL_REG0_THREADMODE(MULTI) |
				A3XX_SP_FS_CTRL_REG0_INSTRBUFFERMODE(fpbuffer) |
				COND(fpbuffer == CACHE, A3XX_SP_FS_CTRL_REG0_CACHEINVALID) |
				A3XX_SP_FS_CTRL_REG0_HALFREGFOOTPRINT(fsi->max_half_reg + 1) |
				A3XX_SP_FS_CTRL_REG0_FULLREGFOOTPRINT(fsi->max_reg + 1) |
				A3XX_SP_FS_CTRL_REG0_INOUTREGOVERLAP |
				A3XX_SP_FS_CTRL_REG0_THREADSIZE(FOUR_QUADS) |
				A3XX_SP_FS_CTRL_REG0_SUPERTHREADMODE |
				COND(fp->has_samp > 0, A3XX_SP_FS_CTRL_REG0_PIXLODENABLE) |
				A3XX_SP_FS_CTRL_REG0_LENGTH(fpbuffersz));
		OUT_RING(ring, A3XX_SP_FS_CTRL_REG1_CONSTLENGTH(fp->constlen) |
				A3XX_SP_FS_CTRL_REG1_INITIALOUTSTANDING(fp->total_in) |
				A3XX_SP_FS_CTRL_REG1_CONSTFOOTPRINT(MAX2(fp->constlen + 1, 0)) |
				A3XX_SP_FS_CTRL_REG1_HALFPRECVAROFFSET(63));

		OUT_PKT0(ring, REG_A3XX_SP_FS_OBJ_OFFSET_REG, 2);
		OUT_RING(ring, A3XX_SP_FS_OBJ_OFFSET_REG_CONSTOBJECTOFFSET(
					MAX2(128, vp->constlen)) |
				A3XX_SP_FS_OBJ_OFFSET_REG_SHADEROBJOFFSET(fsoff));
		OUT_RELOC(ring, fp->bo, 0, 0, 0);  /* SP_FS_OBJ_START_REG */
	}

	OUT_PKT0(ring, REG_A3XX_SP_FS_OUTPUT_REG, 1);
	OUT_RING(ring,
			 COND(fp->writes_pos, A3XX_SP_FS_OUTPUT_REG_DEPTH_ENABLE) |
			 A3XX_SP_FS_OUTPUT_REG_DEPTH_REGID(posz_regid) |
			 A3XX_SP_FS_OUTPUT_REG_MRT(MAX2(1, nr) - 1));

	OUT_PKT0(ring, REG_A3XX_SP_FS_MRT_REG(0), 4);
	for (i = 0; i < 4; i++) {
		uint32_t mrt_reg = A3XX_SP_FS_MRT_REG_REGID(color_regid[i]) |
			COND(fp->key.half_precision, A3XX_SP_FS_MRT_REG_HALF_PRECISION);

		if (i < nr) {
			enum pipe_format fmt = pipe_surface_format(bufs[i]);
			mrt_reg |= COND(util_format_is_pure_uint(fmt), A3XX_SP_FS_MRT_REG_UINT) |
				COND(util_format_is_pure_sint(fmt), A3XX_SP_FS_MRT_REG_SINT);
		}
		OUT_RING(ring, mrt_reg);
	}

	if (emit->key.binning_pass) {
		OUT_PKT0(ring, REG_A3XX_VPC_ATTR, 2);
		OUT_RING(ring, A3XX_VPC_ATTR_THRDASSIGN(1) |
				A3XX_VPC_ATTR_LMSIZE(1) |
				COND(vp->writes_psize, A3XX_VPC_ATTR_PSIZE));
		OUT_RING(ring, 0x00000000);
	} else {
		uint32_t vinterp[4], flatshade[2], vpsrepl[4];

		memset(vinterp, 0, sizeof(vinterp));
		memset(flatshade, 0, sizeof(flatshade));
		memset(vpsrepl, 0, sizeof(vpsrepl));

		/* figure out VARYING_INTERP / FLAT_SHAD register values: */
		for (j = -1; (j = ir3_next_varying(fp, j)) < (int)fp->inputs_count; ) {
			/* NOTE: varyings are packed, so if compmask is 0xb
			 * then first, third, and fourth component occupy
			 * three consecutive varying slots:
			 */
			unsigned compmask = fp->inputs[j].compmask;

			uint32_t inloc = fp->inputs[j].inloc;

			if ((fp->inputs[j].interpolate == INTERP_MODE_FLAT) ||
					(fp->inputs[j].rasterflat && emit->rasterflat)) {
				uint32_t loc = inloc;

				for (i = 0; i < 4; i++) {
					if (compmask & (1 << i)) {
						vinterp[loc / 16] |= FLAT << ((loc % 16) * 2);
						flatshade[loc / 32] |= 1 << (loc % 32);
						loc++;
					}
				}
			}

			gl_varying_slot slot = fp->inputs[j].slot;

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

		OUT_PKT0(ring, REG_A3XX_VPC_ATTR, 2);
		OUT_RING(ring, A3XX_VPC_ATTR_TOTALATTR(fp->total_in) |
				A3XX_VPC_ATTR_THRDASSIGN(1) |
				A3XX_VPC_ATTR_LMSIZE(1) |
				COND(vp->writes_psize, A3XX_VPC_ATTR_PSIZE));
		OUT_RING(ring, A3XX_VPC_PACK_NUMFPNONPOSVAR(fp->total_in) |
				A3XX_VPC_PACK_NUMNONPOSVSVAR(fp->total_in));

		OUT_PKT0(ring, REG_A3XX_VPC_VARYING_INTERP_MODE(0), 4);
		OUT_RING(ring, vinterp[0]);    /* VPC_VARYING_INTERP[0].MODE */
		OUT_RING(ring, vinterp[1]);    /* VPC_VARYING_INTERP[1].MODE */
		OUT_RING(ring, vinterp[2]);    /* VPC_VARYING_INTERP[2].MODE */
		OUT_RING(ring, vinterp[3]);    /* VPC_VARYING_INTERP[3].MODE */

		OUT_PKT0(ring, REG_A3XX_VPC_VARYING_PS_REPL_MODE(0), 4);
		OUT_RING(ring, vpsrepl[0]);    /* VPC_VARYING_PS_REPL[0].MODE */
		OUT_RING(ring, vpsrepl[1]);    /* VPC_VARYING_PS_REPL[1].MODE */
		OUT_RING(ring, vpsrepl[2]);    /* VPC_VARYING_PS_REPL[2].MODE */
		OUT_RING(ring, vpsrepl[3]);    /* VPC_VARYING_PS_REPL[3].MODE */

		OUT_PKT0(ring, REG_A3XX_SP_FS_FLAT_SHAD_MODE_REG_0, 2);
		OUT_RING(ring, flatshade[0]);        /* SP_FS_FLAT_SHAD_MODE_REG_0 */
		OUT_RING(ring, flatshade[1]);        /* SP_FS_FLAT_SHAD_MODE_REG_1 */
	}

	if (vpbuffer == BUFFER)
		emit_shader(ring, vp);

	OUT_PKT0(ring, REG_A3XX_VFD_PERFCOUNTER0_SELECT, 1);
	OUT_RING(ring, 0x00000000);        /* VFD_PERFCOUNTER0_SELECT */

	if (!emit->key.binning_pass) {
		if (fpbuffer == BUFFER)
			emit_shader(ring, fp);

		OUT_PKT0(ring, REG_A3XX_VFD_PERFCOUNTER0_SELECT, 1);
		OUT_RING(ring, 0x00000000);        /* VFD_PERFCOUNTER0_SELECT */
	}
}

void
fd3_prog_init(struct pipe_context *pctx)
{
	pctx->create_fs_state = fd3_fp_state_create;
	pctx->delete_fs_state = fd3_fp_state_delete;

	pctx->create_vs_state = fd3_vp_state_create;
	pctx->delete_vs_state = fd3_vp_state_delete;

	fd_prog_init(pctx);
}
