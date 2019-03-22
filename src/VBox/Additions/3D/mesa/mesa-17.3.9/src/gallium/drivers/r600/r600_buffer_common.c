/*
 * Copyright 2013 Advanced Micro Devices, Inc.
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
 *
 * Authors:
 *      Marek Olšák
 */

#include "r600_cs.h"
#include "util/u_memory.h"
#include "util/u_upload_mgr.h"
#include <inttypes.h>
#include <stdio.h>

bool r600_rings_is_buffer_referenced(struct r600_common_context *ctx,
				     struct pb_buffer *buf,
				     enum radeon_bo_usage usage)
{
	if (ctx->ws->cs_is_buffer_referenced(ctx->gfx.cs, buf, usage)) {
		return true;
	}
	if (radeon_emitted(ctx->dma.cs, 0) &&
	    ctx->ws->cs_is_buffer_referenced(ctx->dma.cs, buf, usage)) {
		return true;
	}
	return false;
}

void *r600_buffer_map_sync_with_rings(struct r600_common_context *ctx,
                                      struct r600_resource *resource,
                                      unsigned usage)
{
	enum radeon_bo_usage rusage = RADEON_USAGE_READWRITE;
	bool busy = false;

	assert(!(resource->flags & RADEON_FLAG_SPARSE));

	if (usage & PIPE_TRANSFER_UNSYNCHRONIZED) {
		return ctx->ws->buffer_map(resource->buf, NULL, usage);
	}

	if (!(usage & PIPE_TRANSFER_WRITE)) {
		/* have to wait for the last write */
		rusage = RADEON_USAGE_WRITE;
	}

	if (radeon_emitted(ctx->gfx.cs, ctx->initial_gfx_cs_size) &&
	    ctx->ws->cs_is_buffer_referenced(ctx->gfx.cs,
					     resource->buf, rusage)) {
		if (usage & PIPE_TRANSFER_DONTBLOCK) {
			ctx->gfx.flush(ctx, RADEON_FLUSH_ASYNC, NULL);
			return NULL;
		} else {
			ctx->gfx.flush(ctx, 0, NULL);
			busy = true;
		}
	}
	if (radeon_emitted(ctx->dma.cs, 0) &&
	    ctx->ws->cs_is_buffer_referenced(ctx->dma.cs,
					     resource->buf, rusage)) {
		if (usage & PIPE_TRANSFER_DONTBLOCK) {
			ctx->dma.flush(ctx, RADEON_FLUSH_ASYNC, NULL);
			return NULL;
		} else {
			ctx->dma.flush(ctx, 0, NULL);
			busy = true;
		}
	}

	if (busy || !ctx->ws->buffer_wait(resource->buf, 0, rusage)) {
		if (usage & PIPE_TRANSFER_DONTBLOCK) {
			return NULL;
		} else {
			/* We will be wait for the GPU. Wait for any offloaded
			 * CS flush to complete to avoid busy-waiting in the winsys. */
			ctx->ws->cs_sync_flush(ctx->gfx.cs);
			if (ctx->dma.cs)
				ctx->ws->cs_sync_flush(ctx->dma.cs);
		}
	}

	/* Setting the CS to NULL will prevent doing checks we have done already. */
	return ctx->ws->buffer_map(resource->buf, NULL, usage);
}

void r600_init_resource_fields(struct r600_common_screen *rscreen,
			       struct r600_resource *res,
			       uint64_t size, unsigned alignment)
{
	struct r600_texture *rtex = (struct r600_texture*)res;

	res->bo_size = size;
	res->bo_alignment = alignment;
	res->flags = 0;
	res->texture_handle_allocated = false;
	res->image_handle_allocated = false;

	switch (res->b.b.usage) {
	case PIPE_USAGE_STREAM:
		res->flags = RADEON_FLAG_GTT_WC;
		/* fall through */
	case PIPE_USAGE_STAGING:
		/* Transfers are likely to occur more often with these
		 * resources. */
		res->domains = RADEON_DOMAIN_GTT;
		break;
	case PIPE_USAGE_DYNAMIC:
		/* Older kernels didn't always flush the HDP cache before
		 * CS execution
		 */
		if (rscreen->info.drm_major == 2 &&
		    rscreen->info.drm_minor < 40) {
			res->domains = RADEON_DOMAIN_GTT;
			res->flags |= RADEON_FLAG_GTT_WC;
			break;
		}
		/* fall through */
	case PIPE_USAGE_DEFAULT:
	case PIPE_USAGE_IMMUTABLE:
	default:
		/* Not listing GTT here improves performance in some
		 * apps. */
		res->domains = RADEON_DOMAIN_VRAM;
		res->flags |= RADEON_FLAG_GTT_WC;
		break;
	}

	if (res->b.b.target == PIPE_BUFFER &&
	    res->b.b.flags & (PIPE_RESOURCE_FLAG_MAP_PERSISTENT |
			      PIPE_RESOURCE_FLAG_MAP_COHERENT)) {
		/* Use GTT for all persistent mappings with older
		 * kernels, because they didn't always flush the HDP
		 * cache before CS execution.
		 *
		 * Write-combined CPU mappings are fine, the kernel
		 * ensures all CPU writes finish before the GPU
		 * executes a command stream.
		 */
		if (rscreen->info.drm_major == 2 &&
		    rscreen->info.drm_minor < 40)
			res->domains = RADEON_DOMAIN_GTT;
	}

	/* Tiled textures are unmappable. Always put them in VRAM. */
	if ((res->b.b.target != PIPE_BUFFER && !rtex->surface.is_linear) ||
	    res->flags & R600_RESOURCE_FLAG_UNMAPPABLE) {
		res->domains = RADEON_DOMAIN_VRAM;
		res->flags |= RADEON_FLAG_NO_CPU_ACCESS |
			 RADEON_FLAG_GTT_WC;
	}

	/* Only displayable single-sample textures can be shared between
	 * processes. */
	if (res->b.b.target == PIPE_BUFFER ||
	    res->b.b.nr_samples >= 2 ||
	    (rtex->surface.micro_tile_mode != RADEON_MICRO_MODE_DISPLAY &&
	     /* Raven doesn't use display micro mode for 32bpp, so check this: */
	     !(res->b.b.bind & PIPE_BIND_SCANOUT)))
		res->flags |= RADEON_FLAG_NO_INTERPROCESS_SHARING;

	/* If VRAM is just stolen system memory, allow both VRAM and
	 * GTT, whichever has free space. If a buffer is evicted from
	 * VRAM to GTT, it will stay there.
	 *
	 * DRM 3.6.0 has good BO move throttling, so we can allow VRAM-only
	 * placements even with a low amount of stolen VRAM.
	 */
	if (!rscreen->info.has_dedicated_vram &&
	    (rscreen->info.drm_major < 3 || rscreen->info.drm_minor < 6) &&
	    res->domains == RADEON_DOMAIN_VRAM) {
		res->domains = RADEON_DOMAIN_VRAM_GTT;
		res->flags &= ~RADEON_FLAG_NO_CPU_ACCESS; /* disallowed with VRAM_GTT */
	}

	if (rscreen->debug_flags & DBG_NO_WC)
		res->flags &= ~RADEON_FLAG_GTT_WC;

	if (res->b.b.bind & PIPE_BIND_SHARED)
		res->flags |= RADEON_FLAG_NO_SUBALLOC;

	/* Set expected VRAM and GART usage for the buffer. */
	res->vram_usage = 0;
	res->gart_usage = 0;

	if (res->domains & RADEON_DOMAIN_VRAM)
		res->vram_usage = size;
	else if (res->domains & RADEON_DOMAIN_GTT)
		res->gart_usage = size;
}

bool r600_alloc_resource(struct r600_common_screen *rscreen,
			 struct r600_resource *res)
{
	struct pb_buffer *old_buf, *new_buf;

	/* Allocate a new resource. */
	new_buf = rscreen->ws->buffer_create(rscreen->ws, res->bo_size,
					     res->bo_alignment,
					     res->domains, res->flags);
	if (!new_buf) {
		return false;
	}

	/* Replace the pointer such that if res->buf wasn't NULL, it won't be
	 * NULL. This should prevent crashes with multiple contexts using
	 * the same buffer where one of the contexts invalidates it while
	 * the others are using it. */
	old_buf = res->buf;
	res->buf = new_buf; /* should be atomic */

	if (rscreen->info.has_virtual_memory)
		res->gpu_address = rscreen->ws->buffer_get_virtual_address(res->buf);
	else
		res->gpu_address = 0;

	pb_reference(&old_buf, NULL);

	util_range_set_empty(&res->valid_buffer_range);

	/* Print debug information. */
	if (rscreen->debug_flags & DBG_VM && res->b.b.target == PIPE_BUFFER) {
		fprintf(stderr, "VM start=0x%"PRIX64"  end=0x%"PRIX64" | Buffer %"PRIu64" bytes\n",
			res->gpu_address, res->gpu_address + res->buf->size,
			res->buf->size);
	}
	return true;
}

static void r600_buffer_destroy(struct pipe_screen *screen,
				struct pipe_resource *buf)
{
	struct r600_resource *rbuffer = r600_resource(buf);

	threaded_resource_deinit(buf);
	util_range_destroy(&rbuffer->valid_buffer_range);
	pb_reference(&rbuffer->buf, NULL);
	FREE(rbuffer);
}

static bool
r600_invalidate_buffer(struct r600_common_context *rctx,
		       struct r600_resource *rbuffer)
{
	/* Shared buffers can't be reallocated. */
	if (rbuffer->b.is_shared)
		return false;

	/* Sparse buffers can't be reallocated. */
	if (rbuffer->flags & RADEON_FLAG_SPARSE)
		return false;

	/* In AMD_pinned_memory, the user pointer association only gets
	 * broken when the buffer is explicitly re-allocated.
	 */
	if (rbuffer->b.is_user_ptr)
		return false;

	/* Check if mapping this buffer would cause waiting for the GPU. */
	if (r600_rings_is_buffer_referenced(rctx, rbuffer->buf, RADEON_USAGE_READWRITE) ||
	    !rctx->ws->buffer_wait(rbuffer->buf, 0, RADEON_USAGE_READWRITE)) {
		rctx->invalidate_buffer(&rctx->b, &rbuffer->b.b);
	} else {
		util_range_set_empty(&rbuffer->valid_buffer_range);
	}

	return true;
}

/* Replace the storage of dst with src. */
void r600_replace_buffer_storage(struct pipe_context *ctx,
				 struct pipe_resource *dst,
				 struct pipe_resource *src)
{
	struct r600_common_context *rctx = (struct r600_common_context *)ctx;
	struct r600_resource *rdst = r600_resource(dst);
	struct r600_resource *rsrc = r600_resource(src);
	uint64_t old_gpu_address = rdst->gpu_address;

	pb_reference(&rdst->buf, rsrc->buf);
	rdst->gpu_address = rsrc->gpu_address;
	rdst->b.b.bind = rsrc->b.b.bind;
	rdst->flags = rsrc->flags;

	assert(rdst->vram_usage == rsrc->vram_usage);
	assert(rdst->gart_usage == rsrc->gart_usage);
	assert(rdst->bo_size == rsrc->bo_size);
	assert(rdst->bo_alignment == rsrc->bo_alignment);
	assert(rdst->domains == rsrc->domains);

	rctx->rebind_buffer(ctx, dst, old_gpu_address);
}

void r600_invalidate_resource(struct pipe_context *ctx,
			      struct pipe_resource *resource)
{
	struct r600_common_context *rctx = (struct r600_common_context*)ctx;
	struct r600_resource *rbuffer = r600_resource(resource);

	/* We currently only do anyting here for buffers */
	if (resource->target == PIPE_BUFFER)
		(void)r600_invalidate_buffer(rctx, rbuffer);
}

static void *r600_buffer_get_transfer(struct pipe_context *ctx,
				      struct pipe_resource *resource,
                                      unsigned usage,
                                      const struct pipe_box *box,
				      struct pipe_transfer **ptransfer,
				      void *data, struct r600_resource *staging,
				      unsigned offset)
{
	struct r600_common_context *rctx = (struct r600_common_context*)ctx;
	struct r600_transfer *transfer;

	if (usage & TC_TRANSFER_MAP_THREADED_UNSYNC)
		transfer = slab_alloc(&rctx->pool_transfers_unsync);
	else
		transfer = slab_alloc(&rctx->pool_transfers);

	transfer->b.b.resource = NULL;
	pipe_resource_reference(&transfer->b.b.resource, resource);
	transfer->b.b.level = 0;
	transfer->b.b.usage = usage;
	transfer->b.b.box = *box;
	transfer->b.b.stride = 0;
	transfer->b.b.layer_stride = 0;
	transfer->b.staging = NULL;
	transfer->offset = offset;
	transfer->staging = staging;
	*ptransfer = &transfer->b.b;
	return data;
}

static bool r600_can_dma_copy_buffer(struct r600_common_context *rctx,
				     unsigned dstx, unsigned srcx, unsigned size)
{
	bool dword_aligned = !(dstx % 4) && !(srcx % 4) && !(size % 4);

	return rctx->screen->has_cp_dma ||
	       (dword_aligned && (rctx->dma.cs ||
				  rctx->screen->has_streamout));

}

static void *r600_buffer_transfer_map(struct pipe_context *ctx,
                                      struct pipe_resource *resource,
                                      unsigned level,
                                      unsigned usage,
                                      const struct pipe_box *box,
                                      struct pipe_transfer **ptransfer)
{
	struct r600_common_context *rctx = (struct r600_common_context*)ctx;
	struct r600_common_screen *rscreen = (struct r600_common_screen*)ctx->screen;
	struct r600_resource *rbuffer = r600_resource(resource);
	uint8_t *data;

	assert(box->x + box->width <= resource->width0);

	/* From GL_AMD_pinned_memory issues:
	 *
	 *     4) Is glMapBuffer on a shared buffer guaranteed to return the
	 *        same system address which was specified at creation time?
	 *
	 *        RESOLVED: NO. The GL implementation might return a different
	 *        virtual mapping of that memory, although the same physical
	 *        page will be used.
	 *
	 * So don't ever use staging buffers.
	 */
	if (rbuffer->b.is_user_ptr)
		usage |= PIPE_TRANSFER_PERSISTENT;

	/* See if the buffer range being mapped has never been initialized,
	 * in which case it can be mapped unsynchronized. */
	if (!(usage & (PIPE_TRANSFER_UNSYNCHRONIZED |
		       TC_TRANSFER_MAP_NO_INFER_UNSYNCHRONIZED)) &&
	    usage & PIPE_TRANSFER_WRITE &&
	    !rbuffer->b.is_shared &&
	    !util_ranges_intersect(&rbuffer->valid_buffer_range, box->x, box->x + box->width)) {
		usage |= PIPE_TRANSFER_UNSYNCHRONIZED;
	}

	/* If discarding the entire range, discard the whole resource instead. */
	if (usage & PIPE_TRANSFER_DISCARD_RANGE &&
	    box->x == 0 && box->width == resource->width0) {
		usage |= PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE;
	}

	if (usage & PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE &&
	    !(usage & (PIPE_TRANSFER_UNSYNCHRONIZED |
		       TC_TRANSFER_MAP_NO_INVALIDATE))) {
		assert(usage & PIPE_TRANSFER_WRITE);

		if (r600_invalidate_buffer(rctx, rbuffer)) {
			/* At this point, the buffer is always idle. */
			usage |= PIPE_TRANSFER_UNSYNCHRONIZED;
		} else {
			/* Fall back to a temporary buffer. */
			usage |= PIPE_TRANSFER_DISCARD_RANGE;
		}
	}

	if ((usage & PIPE_TRANSFER_DISCARD_RANGE) &&
	    !(rscreen->debug_flags & DBG_NO_DISCARD_RANGE) &&
	    ((!(usage & (PIPE_TRANSFER_UNSYNCHRONIZED |
			 PIPE_TRANSFER_PERSISTENT)) &&
	      r600_can_dma_copy_buffer(rctx, box->x, 0, box->width)) ||
	     (rbuffer->flags & RADEON_FLAG_SPARSE))) {
		assert(usage & PIPE_TRANSFER_WRITE);

		/* Check if mapping this buffer would cause waiting for the GPU.
		 */
		if (rbuffer->flags & RADEON_FLAG_SPARSE ||
		    r600_rings_is_buffer_referenced(rctx, rbuffer->buf, RADEON_USAGE_READWRITE) ||
		    !rctx->ws->buffer_wait(rbuffer->buf, 0, RADEON_USAGE_READWRITE)) {
			/* Do a wait-free write-only transfer using a temporary buffer. */
			unsigned offset;
			struct r600_resource *staging = NULL;

			u_upload_alloc(ctx->stream_uploader, 0,
                                       box->width + (box->x % R600_MAP_BUFFER_ALIGNMENT),
				       rctx->screen->info.tcc_cache_line_size,
				       &offset, (struct pipe_resource**)&staging,
                                       (void**)&data);

			if (staging) {
				data += box->x % R600_MAP_BUFFER_ALIGNMENT;
				return r600_buffer_get_transfer(ctx, resource, usage, box,
								ptransfer, data, staging, offset);
			} else if (rbuffer->flags & RADEON_FLAG_SPARSE) {
				return NULL;
			}
		} else {
			/* At this point, the buffer is always idle (we checked it above). */
			usage |= PIPE_TRANSFER_UNSYNCHRONIZED;
		}
	}
	/* Use a staging buffer in cached GTT for reads. */
	else if (((usage & PIPE_TRANSFER_READ) &&
		  !(usage & PIPE_TRANSFER_PERSISTENT) &&
		  (rbuffer->domains & RADEON_DOMAIN_VRAM ||
		   rbuffer->flags & RADEON_FLAG_GTT_WC) &&
		  r600_can_dma_copy_buffer(rctx, 0, box->x, box->width)) ||
		 (rbuffer->flags & RADEON_FLAG_SPARSE)) {
		struct r600_resource *staging;

		assert(!(usage & TC_TRANSFER_MAP_THREADED_UNSYNC));
		staging = (struct r600_resource*) pipe_buffer_create(
				ctx->screen, 0, PIPE_USAGE_STAGING,
				box->width + (box->x % R600_MAP_BUFFER_ALIGNMENT));
		if (staging) {
			/* Copy the VRAM buffer to the staging buffer. */
			rctx->dma_copy(ctx, &staging->b.b, 0,
				       box->x % R600_MAP_BUFFER_ALIGNMENT,
				       0, 0, resource, 0, box);

			data = r600_buffer_map_sync_with_rings(rctx, staging,
							       usage & ~PIPE_TRANSFER_UNSYNCHRONIZED);
			if (!data) {
				r600_resource_reference(&staging, NULL);
				return NULL;
			}
			data += box->x % R600_MAP_BUFFER_ALIGNMENT;

			return r600_buffer_get_transfer(ctx, resource, usage, box,
							ptransfer, data, staging, 0);
		} else if (rbuffer->flags & RADEON_FLAG_SPARSE) {
			return NULL;
		}
	}

	data = r600_buffer_map_sync_with_rings(rctx, rbuffer, usage);
	if (!data) {
		return NULL;
	}
	data += box->x;

	return r600_buffer_get_transfer(ctx, resource, usage, box,
					ptransfer, data, NULL, 0);
}

static void r600_buffer_do_flush_region(struct pipe_context *ctx,
					struct pipe_transfer *transfer,
				        const struct pipe_box *box)
{
	struct r600_transfer *rtransfer = (struct r600_transfer*)transfer;
	struct r600_resource *rbuffer = r600_resource(transfer->resource);

	if (rtransfer->staging) {
		struct pipe_resource *dst, *src;
		unsigned soffset;
		struct pipe_box dma_box;

		dst = transfer->resource;
		src = &rtransfer->staging->b.b;
		soffset = rtransfer->offset + box->x % R600_MAP_BUFFER_ALIGNMENT;

		u_box_1d(soffset, box->width, &dma_box);

		/* Copy the staging buffer into the original one. */
		ctx->resource_copy_region(ctx, dst, 0, box->x, 0, 0, src, 0, &dma_box);
	}

	util_range_add(&rbuffer->valid_buffer_range, box->x,
		       box->x + box->width);
}

static void r600_buffer_flush_region(struct pipe_context *ctx,
				     struct pipe_transfer *transfer,
				     const struct pipe_box *rel_box)
{
	unsigned required_usage = PIPE_TRANSFER_WRITE |
				  PIPE_TRANSFER_FLUSH_EXPLICIT;

	if ((transfer->usage & required_usage) == required_usage) {
		struct pipe_box box;

		u_box_1d(transfer->box.x + rel_box->x, rel_box->width, &box);
		r600_buffer_do_flush_region(ctx, transfer, &box);
	}
}

static void r600_buffer_transfer_unmap(struct pipe_context *ctx,
				       struct pipe_transfer *transfer)
{
	struct r600_common_context *rctx = (struct r600_common_context*)ctx;
	struct r600_transfer *rtransfer = (struct r600_transfer*)transfer;

	if (transfer->usage & PIPE_TRANSFER_WRITE &&
	    !(transfer->usage & PIPE_TRANSFER_FLUSH_EXPLICIT))
		r600_buffer_do_flush_region(ctx, transfer, &transfer->box);

	r600_resource_reference(&rtransfer->staging, NULL);
	assert(rtransfer->b.staging == NULL); /* for threaded context only */
	pipe_resource_reference(&transfer->resource, NULL);

	/* Don't use pool_transfers_unsync. We are always in the driver
	 * thread. */
	slab_free(&rctx->pool_transfers, transfer);
}

void r600_buffer_subdata(struct pipe_context *ctx,
			 struct pipe_resource *buffer,
			 unsigned usage, unsigned offset,
			 unsigned size, const void *data)
{
	struct pipe_transfer *transfer = NULL;
	struct pipe_box box;
	uint8_t *map = NULL;

	u_box_1d(offset, size, &box);
	map = r600_buffer_transfer_map(ctx, buffer, 0,
				       PIPE_TRANSFER_WRITE |
				       PIPE_TRANSFER_DISCARD_RANGE |
				       usage,
				       &box, &transfer);
	if (!map)
		return;

	memcpy(map, data, size);
	r600_buffer_transfer_unmap(ctx, transfer);
}

static const struct u_resource_vtbl r600_buffer_vtbl =
{
	NULL,				/* get_handle */
	r600_buffer_destroy,		/* resource_destroy */
	r600_buffer_transfer_map,	/* transfer_map */
	r600_buffer_flush_region,	/* transfer_flush_region */
	r600_buffer_transfer_unmap,	/* transfer_unmap */
};

static struct r600_resource *
r600_alloc_buffer_struct(struct pipe_screen *screen,
			 const struct pipe_resource *templ)
{
	struct r600_resource *rbuffer;

	rbuffer = MALLOC_STRUCT(r600_resource);

	rbuffer->b.b = *templ;
	rbuffer->b.b.next = NULL;
	pipe_reference_init(&rbuffer->b.b.reference, 1);
	rbuffer->b.b.screen = screen;

	rbuffer->b.vtbl = &r600_buffer_vtbl;
	threaded_resource_init(&rbuffer->b.b);

	rbuffer->buf = NULL;
	rbuffer->bind_history = 0;
	util_range_init(&rbuffer->valid_buffer_range);
	return rbuffer;
}

struct pipe_resource *r600_buffer_create(struct pipe_screen *screen,
					 const struct pipe_resource *templ,
					 unsigned alignment)
{
	struct r600_common_screen *rscreen = (struct r600_common_screen*)screen;
	struct r600_resource *rbuffer = r600_alloc_buffer_struct(screen, templ);

	r600_init_resource_fields(rscreen, rbuffer, templ->width0, alignment);

	if (templ->flags & PIPE_RESOURCE_FLAG_SPARSE)
		rbuffer->flags |= RADEON_FLAG_SPARSE;

	if (!r600_alloc_resource(rscreen, rbuffer)) {
		FREE(rbuffer);
		return NULL;
	}
	return &rbuffer->b.b;
}

struct pipe_resource *r600_aligned_buffer_create(struct pipe_screen *screen,
						 unsigned flags,
						 unsigned usage,
						 unsigned size,
						 unsigned alignment)
{
	struct pipe_resource buffer;

	memset(&buffer, 0, sizeof buffer);
	buffer.target = PIPE_BUFFER;
	buffer.format = PIPE_FORMAT_R8_UNORM;
	buffer.bind = 0;
	buffer.usage = usage;
	buffer.flags = flags;
	buffer.width0 = size;
	buffer.height0 = 1;
	buffer.depth0 = 1;
	buffer.array_size = 1;
	return r600_buffer_create(screen, &buffer, alignment);
}

struct pipe_resource *
r600_buffer_from_user_memory(struct pipe_screen *screen,
			     const struct pipe_resource *templ,
			     void *user_memory)
{
	struct r600_common_screen *rscreen = (struct r600_common_screen*)screen;
	struct radeon_winsys *ws = rscreen->ws;
	struct r600_resource *rbuffer = r600_alloc_buffer_struct(screen, templ);

	rbuffer->domains = RADEON_DOMAIN_GTT;
	rbuffer->flags = 0;
	rbuffer->b.is_user_ptr = true;
	util_range_add(&rbuffer->valid_buffer_range, 0, templ->width0);
	util_range_add(&rbuffer->b.valid_buffer_range, 0, templ->width0);

	/* Convert a user pointer to a buffer. */
	rbuffer->buf = ws->buffer_from_ptr(ws, user_memory, templ->width0);
	if (!rbuffer->buf) {
		FREE(rbuffer);
		return NULL;
	}

	if (rscreen->info.has_virtual_memory)
		rbuffer->gpu_address =
			ws->buffer_get_virtual_address(rbuffer->buf);
	else
		rbuffer->gpu_address = 0;

	rbuffer->vram_usage = 0;
	rbuffer->gart_usage = templ->width0;

	return &rbuffer->b.b;
}
