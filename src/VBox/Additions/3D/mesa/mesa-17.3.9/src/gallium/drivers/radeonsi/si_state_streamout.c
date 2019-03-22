/*
 * Copyright 2013 Advanced Micro Devices, Inc.
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
 * Authors: Marek Olšák <maraeo@gmail.com>
 *
 */

#include "si_pipe.h"
#include "si_state.h"
#include "sid.h"
#include "radeon/r600_cs.h"

#include "util/u_memory.h"

static void si_set_streamout_enable(struct si_context *sctx, bool enable);

static inline void si_so_target_reference(struct si_streamout_target **dst,
					  struct pipe_stream_output_target *src)
{
	pipe_so_target_reference((struct pipe_stream_output_target**)dst, src);
}

static struct pipe_stream_output_target *
si_create_so_target(struct pipe_context *ctx,
		    struct pipe_resource *buffer,
		    unsigned buffer_offset,
		    unsigned buffer_size)
{
	struct si_context *sctx = (struct si_context *)ctx;
	struct si_streamout_target *t;
	struct r600_resource *rbuffer = (struct r600_resource*)buffer;

	t = CALLOC_STRUCT(si_streamout_target);
	if (!t) {
		return NULL;
	}

	u_suballocator_alloc(sctx->b.allocator_zeroed_memory, 4, 4,
			     &t->buf_filled_size_offset,
			     (struct pipe_resource**)&t->buf_filled_size);
	if (!t->buf_filled_size) {
		FREE(t);
		return NULL;
	}

	t->b.reference.count = 1;
	t->b.context = ctx;
	pipe_resource_reference(&t->b.buffer, buffer);
	t->b.buffer_offset = buffer_offset;
	t->b.buffer_size = buffer_size;

	util_range_add(&rbuffer->valid_buffer_range, buffer_offset,
		       buffer_offset + buffer_size);
	return &t->b;
}

static void si_so_target_destroy(struct pipe_context *ctx,
				 struct pipe_stream_output_target *target)
{
	struct si_streamout_target *t = (struct si_streamout_target*)target;
	pipe_resource_reference(&t->b.buffer, NULL);
	r600_resource_reference(&t->buf_filled_size, NULL);
	FREE(t);
}

void si_streamout_buffers_dirty(struct si_context *sctx)
{
	if (!sctx->streamout.enabled_mask)
		return;

	si_mark_atom_dirty(sctx, &sctx->streamout.begin_atom);
	si_set_streamout_enable(sctx, true);
}

static void si_set_streamout_targets(struct pipe_context *ctx,
				     unsigned num_targets,
				     struct pipe_stream_output_target **targets,
				     const unsigned *offsets)
{
	struct si_context *sctx = (struct si_context *)ctx;
	struct si_buffer_resources *buffers = &sctx->rw_buffers;
	struct si_descriptors *descs = &sctx->descriptors[SI_DESCS_RW_BUFFERS];
	unsigned old_num_targets = sctx->streamout.num_targets;
	unsigned i, bufidx;

	/* We are going to unbind the buffers. Mark which caches need to be flushed. */
	if (sctx->streamout.num_targets && sctx->streamout.begin_emitted) {
		/* Since streamout uses vector writes which go through TC L2
		 * and most other clients can use TC L2 as well, we don't need
		 * to flush it.
		 *
		 * The only cases which requires flushing it is VGT DMA index
		 * fetching (on <= CIK) and indirect draw data, which are rare
		 * cases. Thus, flag the TC L2 dirtiness in the resource and
		 * handle it at draw call time.
		 */
		for (i = 0; i < sctx->streamout.num_targets; i++)
			if (sctx->streamout.targets[i])
				r600_resource(sctx->streamout.targets[i]->b.buffer)->TC_L2_dirty = true;

		/* Invalidate the scalar cache in case a streamout buffer is
		 * going to be used as a constant buffer.
		 *
		 * Invalidate TC L1, because streamout bypasses it (done by
		 * setting GLC=1 in the store instruction), but it can contain
		 * outdated data of streamout buffers.
		 *
		 * VS_PARTIAL_FLUSH is required if the buffers are going to be
		 * used as an input immediately.
		 */
		sctx->b.flags |= SI_CONTEXT_INV_SMEM_L1 |
				 SI_CONTEXT_INV_VMEM_L1 |
				 SI_CONTEXT_VS_PARTIAL_FLUSH;
	}

	/* All readers of the streamout targets need to be finished before we can
	 * start writing to the targets.
	 */
	if (num_targets)
		sctx->b.flags |= SI_CONTEXT_PS_PARTIAL_FLUSH |
		                 SI_CONTEXT_CS_PARTIAL_FLUSH;

	/* Streamout buffers must be bound in 2 places:
	 * 1) in VGT by setting the VGT_STRMOUT registers
	 * 2) as shader resources
	 */

	/* Stop streamout. */
	if (sctx->streamout.num_targets && sctx->streamout.begin_emitted)
		si_emit_streamout_end(sctx);

	/* Set the new targets. */
	unsigned enabled_mask = 0, append_bitmask = 0;
	for (i = 0; i < num_targets; i++) {
		si_so_target_reference(&sctx->streamout.targets[i], targets[i]);
		if (!targets[i])
			continue;

		r600_context_add_resource_size(ctx, targets[i]->buffer);
		enabled_mask |= 1 << i;

		if (offsets[i] == ((unsigned)-1))
			append_bitmask |= 1 << i;
	}

	for (; i < sctx->streamout.num_targets; i++)
		si_so_target_reference(&sctx->streamout.targets[i], NULL);

	sctx->streamout.enabled_mask = enabled_mask;
	sctx->streamout.num_targets = num_targets;
	sctx->streamout.append_bitmask = append_bitmask;

	/* Update dirty state bits. */
	if (num_targets) {
		si_streamout_buffers_dirty(sctx);
	} else {
		si_set_atom_dirty(sctx, &sctx->streamout.begin_atom, false);
		si_set_streamout_enable(sctx, false);
	}

	/* Set the shader resources.*/
	for (i = 0; i < num_targets; i++) {
		bufidx = SI_VS_STREAMOUT_BUF0 + i;

		if (targets[i]) {
			struct pipe_resource *buffer = targets[i]->buffer;
			uint64_t va = r600_resource(buffer)->gpu_address;

			/* Set the descriptor.
			 *
			 * On VI, the format must be non-INVALID, otherwise
			 * the buffer will be considered not bound and store
			 * instructions will be no-ops.
			 */
			uint32_t *desc = descs->list + bufidx*4;
			desc[0] = va;
			desc[1] = S_008F04_BASE_ADDRESS_HI(va >> 32);
			desc[2] = 0xffffffff;
			desc[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) |
				  S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
				  S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) |
				  S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
				  S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);

			/* Set the resource. */
			pipe_resource_reference(&buffers->buffers[bufidx],
						buffer);
			radeon_add_to_buffer_list_check_mem(&sctx->b, &sctx->b.gfx,
							    (struct r600_resource*)buffer,
							    buffers->shader_usage,
							    RADEON_PRIO_SHADER_RW_BUFFER,
							    true);
			r600_resource(buffer)->bind_history |= PIPE_BIND_STREAM_OUTPUT;

			buffers->enabled_mask |= 1u << bufidx;
		} else {
			/* Clear the descriptor and unset the resource. */
			memset(descs->list + bufidx*4, 0,
			       sizeof(uint32_t) * 4);
			pipe_resource_reference(&buffers->buffers[bufidx],
						NULL);
			buffers->enabled_mask &= ~(1u << bufidx);
		}
	}
	for (; i < old_num_targets; i++) {
		bufidx = SI_VS_STREAMOUT_BUF0 + i;
		/* Clear the descriptor and unset the resource. */
		memset(descs->list + bufidx*4, 0, sizeof(uint32_t) * 4);
		pipe_resource_reference(&buffers->buffers[bufidx], NULL);
		buffers->enabled_mask &= ~(1u << bufidx);
	}

	sctx->descriptors_dirty |= 1u << SI_DESCS_RW_BUFFERS;
}

static void si_flush_vgt_streamout(struct si_context *sctx)
{
	struct radeon_winsys_cs *cs = sctx->b.gfx.cs;
	unsigned reg_strmout_cntl;

	/* The register is at different places on different ASICs. */
	if (sctx->b.chip_class >= CIK) {
		reg_strmout_cntl = R_0300FC_CP_STRMOUT_CNTL;
		radeon_set_uconfig_reg(cs, reg_strmout_cntl, 0);
	} else {
		reg_strmout_cntl = R_0084FC_CP_STRMOUT_CNTL;
		radeon_set_config_reg(cs, reg_strmout_cntl, 0);
	}

	radeon_emit(cs, PKT3(PKT3_EVENT_WRITE, 0, 0));
	radeon_emit(cs, EVENT_TYPE(EVENT_TYPE_SO_VGTSTREAMOUT_FLUSH) | EVENT_INDEX(0));

	radeon_emit(cs, PKT3(PKT3_WAIT_REG_MEM, 5, 0));
	radeon_emit(cs, WAIT_REG_MEM_EQUAL); /* wait until the register is equal to the reference value */
	radeon_emit(cs, reg_strmout_cntl >> 2);  /* register */
	radeon_emit(cs, 0);
	radeon_emit(cs, S_0084FC_OFFSET_UPDATE_DONE(1)); /* reference value */
	radeon_emit(cs, S_0084FC_OFFSET_UPDATE_DONE(1)); /* mask */
	radeon_emit(cs, 4); /* poll interval */
}

static void si_emit_streamout_begin(struct r600_common_context *rctx, struct r600_atom *atom)
{
	struct si_context *sctx = (struct si_context*)rctx;
	struct radeon_winsys_cs *cs = sctx->b.gfx.cs;
	struct si_streamout_target **t = sctx->streamout.targets;
	uint16_t *stride_in_dw = sctx->streamout.stride_in_dw;
	unsigned i;

	si_flush_vgt_streamout(sctx);

	for (i = 0; i < sctx->streamout.num_targets; i++) {
		if (!t[i])
			continue;

		t[i]->stride_in_dw = stride_in_dw[i];

		/* SI binds streamout buffers as shader resources.
		 * VGT only counts primitives and tells the shader
		 * through SGPRs what to do. */
		radeon_set_context_reg_seq(cs, R_028AD0_VGT_STRMOUT_BUFFER_SIZE_0 + 16*i, 2);
		radeon_emit(cs, (t[i]->b.buffer_offset +
				 t[i]->b.buffer_size) >> 2);	/* BUFFER_SIZE (in DW) */
		radeon_emit(cs, stride_in_dw[i]);		/* VTX_STRIDE (in DW) */

		if (sctx->streamout.append_bitmask & (1 << i) && t[i]->buf_filled_size_valid) {
			uint64_t va = t[i]->buf_filled_size->gpu_address +
				      t[i]->buf_filled_size_offset;

			/* Append. */
			radeon_emit(cs, PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0));
			radeon_emit(cs, STRMOUT_SELECT_BUFFER(i) |
				    STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_FROM_MEM)); /* control */
			radeon_emit(cs, 0); /* unused */
			radeon_emit(cs, 0); /* unused */
			radeon_emit(cs, va); /* src address lo */
			radeon_emit(cs, va >> 32); /* src address hi */

			radeon_add_to_buffer_list(&sctx->b,  &sctx->b.gfx,
						  t[i]->buf_filled_size,
						  RADEON_USAGE_READ,
						  RADEON_PRIO_SO_FILLED_SIZE);
		} else {
			/* Start from the beginning. */
			radeon_emit(cs, PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0));
			radeon_emit(cs, STRMOUT_SELECT_BUFFER(i) |
				    STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_FROM_PACKET)); /* control */
			radeon_emit(cs, 0); /* unused */
			radeon_emit(cs, 0); /* unused */
			radeon_emit(cs, t[i]->b.buffer_offset >> 2); /* buffer offset in DW */
			radeon_emit(cs, 0); /* unused */
		}
	}

	sctx->streamout.begin_emitted = true;
}

void si_emit_streamout_end(struct si_context *sctx)
{
	struct radeon_winsys_cs *cs = sctx->b.gfx.cs;
	struct si_streamout_target **t = sctx->streamout.targets;
	unsigned i;
	uint64_t va;

	si_flush_vgt_streamout(sctx);

	for (i = 0; i < sctx->streamout.num_targets; i++) {
		if (!t[i])
			continue;

		va = t[i]->buf_filled_size->gpu_address + t[i]->buf_filled_size_offset;
		radeon_emit(cs, PKT3(PKT3_STRMOUT_BUFFER_UPDATE, 4, 0));
		radeon_emit(cs, STRMOUT_SELECT_BUFFER(i) |
			    STRMOUT_OFFSET_SOURCE(STRMOUT_OFFSET_NONE) |
			    STRMOUT_STORE_BUFFER_FILLED_SIZE); /* control */
		radeon_emit(cs, va);     /* dst address lo */
		radeon_emit(cs, va >> 32); /* dst address hi */
		radeon_emit(cs, 0); /* unused */
		radeon_emit(cs, 0); /* unused */

		radeon_add_to_buffer_list(&sctx->b,  &sctx->b.gfx,
					  t[i]->buf_filled_size,
					  RADEON_USAGE_WRITE,
					  RADEON_PRIO_SO_FILLED_SIZE);

		/* Zero the buffer size. The counters (primitives generated,
		 * primitives emitted) may be enabled even if there is not
		 * buffer bound. This ensures that the primitives-emitted query
		 * won't increment. */
		radeon_set_context_reg(cs, R_028AD0_VGT_STRMOUT_BUFFER_SIZE_0 + 16*i, 0);

		t[i]->buf_filled_size_valid = true;
	}

	sctx->streamout.begin_emitted = false;
	sctx->b.flags |= R600_CONTEXT_STREAMOUT_FLUSH;
}

/* STREAMOUT CONFIG DERIVED STATE
 *
 * Streamout must be enabled for the PRIMITIVES_GENERATED query to work.
 * The buffer mask is an independent state, so no writes occur if there
 * are no buffers bound.
 */

static void si_emit_streamout_enable(struct r600_common_context *rctx,
				     struct r600_atom *atom)
{
	struct si_context *sctx = (struct si_context*)rctx;

	radeon_set_context_reg_seq(sctx->b.gfx.cs, R_028B94_VGT_STRMOUT_CONFIG, 2);
	radeon_emit(sctx->b.gfx.cs,
		    S_028B94_STREAMOUT_0_EN(si_get_strmout_en(sctx)) |
		    S_028B94_RAST_STREAM(0) |
		    S_028B94_STREAMOUT_1_EN(si_get_strmout_en(sctx)) |
		    S_028B94_STREAMOUT_2_EN(si_get_strmout_en(sctx)) |
		    S_028B94_STREAMOUT_3_EN(si_get_strmout_en(sctx)));
	radeon_emit(sctx->b.gfx.cs,
		    sctx->streamout.hw_enabled_mask &
		    sctx->streamout.enabled_stream_buffers_mask);
}

static void si_set_streamout_enable(struct si_context *sctx, bool enable)
{
	bool old_strmout_en = si_get_strmout_en(sctx);
	unsigned old_hw_enabled_mask = sctx->streamout.hw_enabled_mask;

	sctx->streamout.streamout_enabled = enable;

	sctx->streamout.hw_enabled_mask = sctx->streamout.enabled_mask |
					  (sctx->streamout.enabled_mask << 4) |
					  (sctx->streamout.enabled_mask << 8) |
					  (sctx->streamout.enabled_mask << 12);

	if ((old_strmout_en != si_get_strmout_en(sctx)) ||
            (old_hw_enabled_mask != sctx->streamout.hw_enabled_mask))
		si_mark_atom_dirty(sctx, &sctx->streamout.enable_atom);
}

void si_update_prims_generated_query_state(struct si_context *sctx,
					   unsigned type, int diff)
{
	if (type == PIPE_QUERY_PRIMITIVES_GENERATED) {
		bool old_strmout_en = si_get_strmout_en(sctx);

		sctx->streamout.num_prims_gen_queries += diff;
		assert(sctx->streamout.num_prims_gen_queries >= 0);

		sctx->streamout.prims_gen_query_enabled =
			sctx->streamout.num_prims_gen_queries != 0;

		if (old_strmout_en != si_get_strmout_en(sctx))
			si_mark_atom_dirty(sctx, &sctx->streamout.enable_atom);
	}
}

void si_init_streamout_functions(struct si_context *sctx)
{
	sctx->b.b.create_stream_output_target = si_create_so_target;
	sctx->b.b.stream_output_target_destroy = si_so_target_destroy;
	sctx->b.b.set_stream_output_targets = si_set_streamout_targets;
	sctx->streamout.begin_atom.emit = si_emit_streamout_begin;
	sctx->streamout.enable_atom.emit = si_emit_streamout_enable;
}
