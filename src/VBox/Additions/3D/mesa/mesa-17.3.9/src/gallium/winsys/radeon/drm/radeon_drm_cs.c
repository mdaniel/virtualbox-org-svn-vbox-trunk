/*
 * Copyright © 2008 Jérôme Glisse
 * Copyright © 2010 Marek Olšák <maraeo@gmail.com>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *      Marek Olšák <maraeo@gmail.com>
 *
 * Based on work from libdrm_radeon by:
 *      Aapo Tahkola <aet@rasterburn.org>
 *      Nicolai Haehnle <prefect_@gmx.net>
 *      Jérôme Glisse <glisse@freedesktop.org>
 */

/*
    This file replaces libdrm's radeon_cs_gem with our own implemention.
    It's optimized specifically for Radeon DRM.
    Adding buffers and space checking are faster and simpler than their
    counterparts in libdrm (the time complexity of all the functions
    is O(1) in nearly all scenarios, thanks to hashing).

    It works like this:

    cs_add_buffer(cs, buf, read_domain, write_domain) adds a new relocation and
    also adds the size of 'buf' to the used_gart and used_vram winsys variables
    based on the domains, which are simply or'd for the accounting purposes.
    The adding is skipped if the reloc is already present in the list, but it
    accounts any newly-referenced domains.

    cs_validate is then called, which just checks:
        used_vram/gart < vram/gart_size * 0.8
    The 0.8 number allows for some memory fragmentation. If the validation
    fails, the pipe driver flushes CS and tries do the validation again,
    i.e. it validates only that one operation. If it fails again, it drops
    the operation on the floor and prints some nasty message to stderr.
    (done in the pipe driver)

    cs_write_reloc(cs, buf) just writes a reloc that has been added using
    cs_add_buffer. The read_domain and write_domain parameters have been removed,
    because we already specify them in cs_add_buffer.
*/

#include "radeon_drm_cs.h"

#include "util/u_memory.h"
#include "os/os_time.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <xf86drm.h>


#define RELOC_DWORDS (sizeof(struct drm_radeon_cs_reloc) / sizeof(uint32_t))

static struct pipe_fence_handle *
radeon_cs_create_fence(struct radeon_winsys_cs *rcs);
static void radeon_fence_reference(struct pipe_fence_handle **dst,
                                   struct pipe_fence_handle *src);

static struct radeon_winsys_ctx *radeon_drm_ctx_create(struct radeon_winsys *ws)
{
    /* No context support here. Just return the winsys pointer
     * as the "context". */
    return (struct radeon_winsys_ctx*)ws;
}

static void radeon_drm_ctx_destroy(struct radeon_winsys_ctx *ctx)
{
    /* No context support here. */
}

static bool radeon_init_cs_context(struct radeon_cs_context *csc,
                                   struct radeon_drm_winsys *ws)
{
    int i;

    csc->fd = ws->fd;

    csc->chunks[0].chunk_id = RADEON_CHUNK_ID_IB;
    csc->chunks[0].length_dw = 0;
    csc->chunks[0].chunk_data = (uint64_t)(uintptr_t)csc->buf;
    csc->chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
    csc->chunks[1].length_dw = 0;
    csc->chunks[1].chunk_data = (uint64_t)(uintptr_t)csc->relocs;
    csc->chunks[2].chunk_id = RADEON_CHUNK_ID_FLAGS;
    csc->chunks[2].length_dw = 2;
    csc->chunks[2].chunk_data = (uint64_t)(uintptr_t)&csc->flags;

    csc->chunk_array[0] = (uint64_t)(uintptr_t)&csc->chunks[0];
    csc->chunk_array[1] = (uint64_t)(uintptr_t)&csc->chunks[1];
    csc->chunk_array[2] = (uint64_t)(uintptr_t)&csc->chunks[2];

    csc->cs.chunks = (uint64_t)(uintptr_t)csc->chunk_array;

    for (i = 0; i < ARRAY_SIZE(csc->reloc_indices_hashlist); i++) {
        csc->reloc_indices_hashlist[i] = -1;
    }
    return true;
}

static void radeon_cs_context_cleanup(struct radeon_cs_context *csc)
{
    unsigned i;

    for (i = 0; i < csc->num_relocs; i++) {
        p_atomic_dec(&csc->relocs_bo[i].bo->num_cs_references);
        radeon_bo_reference(&csc->relocs_bo[i].bo, NULL);
    }
    for (i = 0; i < csc->num_slab_buffers; ++i) {
        p_atomic_dec(&csc->slab_buffers[i].bo->num_cs_references);
        radeon_bo_reference(&csc->slab_buffers[i].bo, NULL);
    }

    csc->num_relocs = 0;
    csc->num_validated_relocs = 0;
    csc->num_slab_buffers = 0;
    csc->chunks[0].length_dw = 0;
    csc->chunks[1].length_dw = 0;

    for (i = 0; i < ARRAY_SIZE(csc->reloc_indices_hashlist); i++) {
        csc->reloc_indices_hashlist[i] = -1;
    }
}

static void radeon_destroy_cs_context(struct radeon_cs_context *csc)
{
    radeon_cs_context_cleanup(csc);
    FREE(csc->slab_buffers);
    FREE(csc->relocs_bo);
    FREE(csc->relocs);
}


static struct radeon_winsys_cs *
radeon_drm_cs_create(struct radeon_winsys_ctx *ctx,
                     enum ring_type ring_type,
                     void (*flush)(void *ctx, unsigned flags,
                                   struct pipe_fence_handle **fence),
                     void *flush_ctx)
{
    struct radeon_drm_winsys *ws = (struct radeon_drm_winsys*)ctx;
    struct radeon_drm_cs *cs;

    cs = CALLOC_STRUCT(radeon_drm_cs);
    if (!cs) {
        return NULL;
    }
    util_queue_fence_init(&cs->flush_completed);

    cs->ws = ws;
    cs->flush_cs = flush;
    cs->flush_data = flush_ctx;

    if (!radeon_init_cs_context(&cs->csc1, cs->ws)) {
        FREE(cs);
        return NULL;
    }
    if (!radeon_init_cs_context(&cs->csc2, cs->ws)) {
        radeon_destroy_cs_context(&cs->csc1);
        FREE(cs);
        return NULL;
    }

    /* Set the first command buffer as current. */
    cs->csc = &cs->csc1;
    cs->cst = &cs->csc2;
    cs->base.current.buf = cs->csc->buf;
    cs->base.current.max_dw = ARRAY_SIZE(cs->csc->buf);
    cs->ring_type = ring_type;

    p_atomic_inc(&ws->num_cs);
    return &cs->base;
}

int radeon_lookup_buffer(struct radeon_cs_context *csc, struct radeon_bo *bo)
{
    unsigned hash = bo->hash & (ARRAY_SIZE(csc->reloc_indices_hashlist)-1);
    struct radeon_bo_item *buffers;
    unsigned num_buffers;
    int i = csc->reloc_indices_hashlist[hash];

    if (bo->handle) {
        buffers = csc->relocs_bo;
        num_buffers = csc->num_relocs;
    } else {
        buffers = csc->slab_buffers;
        num_buffers = csc->num_slab_buffers;
    }

    /* not found or found */
    if (i == -1 || (i < num_buffers && buffers[i].bo == bo))
        return i;

    /* Hash collision, look for the BO in the list of relocs linearly. */
    for (i = num_buffers - 1; i >= 0; i--) {
        if (buffers[i].bo == bo) {
            /* Put this reloc in the hash list.
             * This will prevent additional hash collisions if there are
             * several consecutive lookup_buffer calls for the same buffer.
             *
             * Example: Assuming buffers A,B,C collide in the hash list,
             * the following sequence of relocs:
             *         AAAAAAAAAAABBBBBBBBBBBBBBCCCCCCCC
             * will collide here: ^ and here:   ^,
             * meaning that we should get very few collisions in the end. */
            csc->reloc_indices_hashlist[hash] = i;
            return i;
        }
    }
    return -1;
}

static unsigned radeon_lookup_or_add_real_buffer(struct radeon_drm_cs *cs,
                                                 struct radeon_bo *bo)
{
    struct radeon_cs_context *csc = cs->csc;
    struct drm_radeon_cs_reloc *reloc;
    unsigned hash = bo->hash & (ARRAY_SIZE(csc->reloc_indices_hashlist)-1);
    int i = -1;

    i = radeon_lookup_buffer(csc, bo);

    if (i >= 0) {
        /* For async DMA, every add_buffer call must add a buffer to the list
         * no matter how many duplicates there are. This is due to the fact
         * the DMA CS checker doesn't use NOP packets for offset patching,
         * but always uses the i-th buffer from the list to patch the i-th
         * offset. If there are N offsets in a DMA CS, there must also be N
         * buffers in the relocation list.
         *
         * This doesn't have to be done if virtual memory is enabled,
         * because there is no offset patching with virtual memory.
         */
        if (cs->ring_type != RING_DMA || cs->ws->info.has_virtual_memory) {
            return i;
        }
    }

    /* New relocation, check if the backing array is large enough. */
    if (csc->num_relocs >= csc->max_relocs) {
        uint32_t size;
        csc->max_relocs = MAX2(csc->max_relocs + 16, (unsigned)(csc->max_relocs * 1.3));

        size = csc->max_relocs * sizeof(csc->relocs_bo[0]);
        csc->relocs_bo = realloc(csc->relocs_bo, size);

        size = csc->max_relocs * sizeof(struct drm_radeon_cs_reloc);
        csc->relocs = realloc(csc->relocs, size);

        csc->chunks[1].chunk_data = (uint64_t)(uintptr_t)csc->relocs;
    }

    /* Initialize the new relocation. */
    csc->relocs_bo[csc->num_relocs].bo = NULL;
    csc->relocs_bo[csc->num_relocs].u.real.priority_usage = 0;
    radeon_bo_reference(&csc->relocs_bo[csc->num_relocs].bo, bo);
    p_atomic_inc(&bo->num_cs_references);
    reloc = &csc->relocs[csc->num_relocs];
    reloc->handle = bo->handle;
    reloc->read_domains = 0;
    reloc->write_domain = 0;
    reloc->flags = 0;

    csc->reloc_indices_hashlist[hash] = csc->num_relocs;

    csc->chunks[1].length_dw += RELOC_DWORDS;

    return csc->num_relocs++;
}

static int radeon_lookup_or_add_slab_buffer(struct radeon_drm_cs *cs,
                                            struct radeon_bo *bo)
{
    struct radeon_cs_context *csc = cs->csc;
    unsigned hash;
    struct radeon_bo_item *item;
    int idx;
    int real_idx;

    idx = radeon_lookup_buffer(csc, bo);
    if (idx >= 0)
        return idx;

    real_idx = radeon_lookup_or_add_real_buffer(cs, bo->u.slab.real);

    /* Check if the backing array is large enough. */
    if (csc->num_slab_buffers >= csc->max_slab_buffers) {
        unsigned new_max = MAX2(csc->max_slab_buffers + 16,
                                (unsigned)(csc->max_slab_buffers * 1.3));
        struct radeon_bo_item *new_buffers =
            REALLOC(csc->slab_buffers,
                    csc->max_slab_buffers * sizeof(*new_buffers),
                    new_max * sizeof(*new_buffers));
        if (!new_buffers) {
            fprintf(stderr, "radeon_lookup_or_add_slab_buffer: allocation failure\n");
            return -1;
        }

        csc->max_slab_buffers = new_max;
        csc->slab_buffers = new_buffers;
    }

    /* Initialize the new relocation. */
    idx = csc->num_slab_buffers++;
    item = &csc->slab_buffers[idx];

    item->bo = NULL;
    item->u.slab.real_idx = real_idx;
    radeon_bo_reference(&item->bo, bo);
    p_atomic_inc(&bo->num_cs_references);

    hash = bo->hash & (ARRAY_SIZE(csc->reloc_indices_hashlist)-1);
    csc->reloc_indices_hashlist[hash] = idx;

    return idx;
}

static unsigned radeon_drm_cs_add_buffer(struct radeon_winsys_cs *rcs,
                                        struct pb_buffer *buf,
                                        enum radeon_bo_usage usage,
                                        enum radeon_bo_domain domains,
                                        enum radeon_bo_priority priority)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    struct radeon_bo *bo = (struct radeon_bo*)buf;
    enum radeon_bo_domain added_domains;
    enum radeon_bo_domain rd = usage & RADEON_USAGE_READ ? domains : 0;
    enum radeon_bo_domain wd = usage & RADEON_USAGE_WRITE ? domains : 0;
    struct drm_radeon_cs_reloc *reloc;
    int index;

    if (!bo->handle) {
        index = radeon_lookup_or_add_slab_buffer(cs, bo);
        if (index < 0)
            return 0;

        index = cs->csc->slab_buffers[index].u.slab.real_idx;
    } else {
        index = radeon_lookup_or_add_real_buffer(cs, bo);
    }

    reloc = &cs->csc->relocs[index];
    added_domains = (rd | wd) & ~(reloc->read_domains | reloc->write_domain);
    reloc->read_domains |= rd;
    reloc->write_domain |= wd;
    reloc->flags = MAX2(reloc->flags, priority);
    cs->csc->relocs_bo[index].u.real.priority_usage |= 1ull << priority;

    if (added_domains & RADEON_DOMAIN_VRAM)
        cs->base.used_vram += bo->base.size;
    else if (added_domains & RADEON_DOMAIN_GTT)
        cs->base.used_gart += bo->base.size;

    return index;
}

static int radeon_drm_cs_lookup_buffer(struct radeon_winsys_cs *rcs,
                                   struct pb_buffer *buf)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);

    return radeon_lookup_buffer(cs->csc, (struct radeon_bo*)buf);
}

static bool radeon_drm_cs_validate(struct radeon_winsys_cs *rcs)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    bool status =
        cs->base.used_gart < cs->ws->info.gart_size * 0.8 &&
        cs->base.used_vram < cs->ws->info.vram_size * 0.8;

    if (status) {
        cs->csc->num_validated_relocs = cs->csc->num_relocs;
    } else {
        /* Remove lately-added buffers. The validation failed with them
         * and the CS is about to be flushed because of that. Keep only
         * the already-validated buffers. */
        unsigned i;

        for (i = cs->csc->num_validated_relocs; i < cs->csc->num_relocs; i++) {
            p_atomic_dec(&cs->csc->relocs_bo[i].bo->num_cs_references);
            radeon_bo_reference(&cs->csc->relocs_bo[i].bo, NULL);
        }
        cs->csc->num_relocs = cs->csc->num_validated_relocs;

        /* Flush if there are any relocs. Clean up otherwise. */
        if (cs->csc->num_relocs) {
            cs->flush_cs(cs->flush_data, RADEON_FLUSH_ASYNC, NULL);
        } else {
            radeon_cs_context_cleanup(cs->csc);
            cs->base.used_vram = 0;
            cs->base.used_gart = 0;

            assert(cs->base.current.cdw == 0);
            if (cs->base.current.cdw != 0) {
                fprintf(stderr, "radeon: Unexpected error in %s.\n", __func__);
            }
        }
    }
    return status;
}

static bool radeon_drm_cs_check_space(struct radeon_winsys_cs *rcs, unsigned dw)
{
   assert(rcs->current.cdw <= rcs->current.max_dw);
   return rcs->current.max_dw - rcs->current.cdw >= dw;
}

static unsigned radeon_drm_cs_get_buffer_list(struct radeon_winsys_cs *rcs,
                                              struct radeon_bo_list_item *list)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    int i;

    if (list) {
        for (i = 0; i < cs->csc->num_relocs; i++) {
            list[i].bo_size = cs->csc->relocs_bo[i].bo->base.size;
            list[i].vm_address = cs->csc->relocs_bo[i].bo->va;
            list[i].priority_usage = cs->csc->relocs_bo[i].u.real.priority_usage;
        }
    }
    return cs->csc->num_relocs;
}

void radeon_drm_cs_emit_ioctl_oneshot(void *job, int thread_index)
{
    struct radeon_cs_context *csc = ((struct radeon_drm_cs*)job)->cst;
    unsigned i;
    int r;

    r = drmCommandWriteRead(csc->fd, DRM_RADEON_CS,
                            &csc->cs, sizeof(struct drm_radeon_cs));
    if (r) {
	if (r == -ENOMEM)
	    fprintf(stderr, "radeon: Not enough memory for command submission.\n");
	else if (debug_get_bool_option("RADEON_DUMP_CS", false)) {
            unsigned i;

            fprintf(stderr, "radeon: The kernel rejected CS, dumping...\n");
            for (i = 0; i < csc->chunks[0].length_dw; i++) {
                fprintf(stderr, "0x%08X\n", csc->buf[i]);
            }
        } else {
            fprintf(stderr, "radeon: The kernel rejected CS, "
                    "see dmesg for more information (%i).\n", r);
        }
    }

    for (i = 0; i < csc->num_relocs; i++)
        p_atomic_dec(&csc->relocs_bo[i].bo->num_active_ioctls);
    for (i = 0; i < csc->num_slab_buffers; i++)
        p_atomic_dec(&csc->slab_buffers[i].bo->num_active_ioctls);

    radeon_cs_context_cleanup(csc);
}

/*
 * Make sure previous submission of this cs are completed
 */
void radeon_drm_cs_sync_flush(struct radeon_winsys_cs *rcs)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);

    /* Wait for any pending ioctl of this CS to complete. */
    if (util_queue_is_initialized(&cs->ws->cs_queue))
        util_queue_fence_wait(&cs->flush_completed);
}

/* Add the given fence to a slab buffer fence list.
 *
 * There is a potential race condition when bo participates in submissions on
 * two or more threads simultaneously. Since we do not know which of the
 * submissions will be sent to the GPU first, we have to keep the fences
 * of all submissions.
 *
 * However, fences that belong to submissions that have already returned from
 * their respective ioctl do not have to be kept, because we know that they
 * will signal earlier.
 */
static void radeon_bo_slab_fence(struct radeon_bo *bo, struct radeon_bo *fence)
{
    unsigned dst;

    assert(fence->num_cs_references);

    /* Cleanup older fences */
    dst = 0;
    for (unsigned src = 0; src < bo->u.slab.num_fences; ++src) {
        if (bo->u.slab.fences[src]->num_cs_references) {
            bo->u.slab.fences[dst] = bo->u.slab.fences[src];
            dst++;
        } else {
            radeon_bo_reference(&bo->u.slab.fences[src], NULL);
        }
    }
    bo->u.slab.num_fences = dst;

    /* Check available space for the new fence */
    if (bo->u.slab.num_fences >= bo->u.slab.max_fences) {
        unsigned new_max_fences = bo->u.slab.max_fences + 1;
        struct radeon_bo **new_fences = REALLOC(bo->u.slab.fences,
                                                bo->u.slab.max_fences * sizeof(*new_fences),
                                                new_max_fences * sizeof(*new_fences));
        if (!new_fences) {
            fprintf(stderr, "radeon_bo_slab_fence: allocation failure, dropping fence\n");
            return;
        }

        bo->u.slab.fences = new_fences;
        bo->u.slab.max_fences = new_max_fences;
    }

    /* Add the new fence */
    bo->u.slab.fences[bo->u.slab.num_fences] = NULL;
    radeon_bo_reference(&bo->u.slab.fences[bo->u.slab.num_fences], fence);
    bo->u.slab.num_fences++;
}

DEBUG_GET_ONCE_BOOL_OPTION(noop, "RADEON_NOOP", false)

static int radeon_drm_cs_flush(struct radeon_winsys_cs *rcs,
                               unsigned flags,
                               struct pipe_fence_handle **pfence)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    struct radeon_cs_context *tmp;

    switch (cs->ring_type) {
    case RING_DMA:
        /* pad DMA ring to 8 DWs */
        if (cs->ws->info.chip_class <= SI) {
            while (rcs->current.cdw & 7)
                radeon_emit(&cs->base, 0xf0000000); /* NOP packet */
        } else {
            while (rcs->current.cdw & 7)
                radeon_emit(&cs->base, 0x00000000); /* NOP packet */
        }
        break;
    case RING_GFX:
        /* pad GFX ring to 8 DWs to meet CP fetch alignment requirements
         * r6xx, requires at least 4 dw alignment to avoid a hw bug.
         */
        if (cs->ws->info.gfx_ib_pad_with_type2) {
            while (rcs->current.cdw & 7)
                radeon_emit(&cs->base, 0x80000000); /* type2 nop packet */
        } else {
            while (rcs->current.cdw & 7)
                radeon_emit(&cs->base, 0xffff1000); /* type3 nop packet */
        }
        break;
    case RING_UVD:
        while (rcs->current.cdw & 15)
            radeon_emit(&cs->base, 0x80000000); /* type2 nop packet */
        break;
    default:
        break;
    }

    if (rcs->current.cdw > rcs->current.max_dw) {
       fprintf(stderr, "radeon: command stream overflowed\n");
    }

    if (pfence || cs->csc->num_slab_buffers) {
        struct pipe_fence_handle *fence;

        if (cs->next_fence) {
            fence = cs->next_fence;
            cs->next_fence = NULL;
        } else {
            fence = radeon_cs_create_fence(rcs);
        }

        if (fence) {
            if (pfence)
                radeon_fence_reference(pfence, fence);

            mtx_lock(&cs->ws->bo_fence_lock);
            for (unsigned i = 0; i < cs->csc->num_slab_buffers; ++i) {
                struct radeon_bo *bo = cs->csc->slab_buffers[i].bo;
                p_atomic_inc(&bo->num_active_ioctls);
                radeon_bo_slab_fence(bo, (struct radeon_bo *)fence);
            }
            mtx_unlock(&cs->ws->bo_fence_lock);

            radeon_fence_reference(&fence, NULL);
        }
    } else {
        radeon_fence_reference(&cs->next_fence, NULL);
    }

    radeon_drm_cs_sync_flush(rcs);

    /* Swap command streams. */
    tmp = cs->csc;
    cs->csc = cs->cst;
    cs->cst = tmp;

    /* If the CS is not empty or overflowed, emit it in a separate thread. */
    if (cs->base.current.cdw && cs->base.current.cdw <= cs->base.current.max_dw && !debug_get_option_noop()) {
        unsigned i, num_relocs;

        num_relocs = cs->cst->num_relocs;

        cs->cst->chunks[0].length_dw = cs->base.current.cdw;

        for (i = 0; i < num_relocs; i++) {
            /* Update the number of active asynchronous CS ioctls for the buffer. */
            p_atomic_inc(&cs->cst->relocs_bo[i].bo->num_active_ioctls);
        }

        switch (cs->ring_type) {
        case RING_DMA:
            cs->cst->flags[0] = 0;
            cs->cst->flags[1] = RADEON_CS_RING_DMA;
            cs->cst->cs.num_chunks = 3;
            if (cs->ws->info.has_virtual_memory) {
                cs->cst->flags[0] |= RADEON_CS_USE_VM;
            }
            break;

        case RING_UVD:
            cs->cst->flags[0] = 0;
            cs->cst->flags[1] = RADEON_CS_RING_UVD;
            cs->cst->cs.num_chunks = 3;
            break;

        case RING_VCE:
            cs->cst->flags[0] = 0;
            cs->cst->flags[1] = RADEON_CS_RING_VCE;
            cs->cst->cs.num_chunks = 3;
            break;

        default:
        case RING_GFX:
        case RING_COMPUTE:
            cs->cst->flags[0] = RADEON_CS_KEEP_TILING_FLAGS;
            cs->cst->flags[1] = RADEON_CS_RING_GFX;
            cs->cst->cs.num_chunks = 3;

            if (cs->ws->info.has_virtual_memory) {
                cs->cst->flags[0] |= RADEON_CS_USE_VM;
                cs->cst->cs.num_chunks = 3;
            }
            if (flags & RADEON_FLUSH_END_OF_FRAME) {
                cs->cst->flags[0] |= RADEON_CS_END_OF_FRAME;
                cs->cst->cs.num_chunks = 3;
            }
            if (cs->ring_type == RING_COMPUTE) {
                cs->cst->flags[1] = RADEON_CS_RING_COMPUTE;
                cs->cst->cs.num_chunks = 3;
            }
            break;
        }

        if (util_queue_is_initialized(&cs->ws->cs_queue)) {
            util_queue_add_job(&cs->ws->cs_queue, cs, &cs->flush_completed,
                               radeon_drm_cs_emit_ioctl_oneshot, NULL);
            if (!(flags & RADEON_FLUSH_ASYNC))
                radeon_drm_cs_sync_flush(rcs);
        } else {
            radeon_drm_cs_emit_ioctl_oneshot(cs, 0);
        }
    } else {
        radeon_cs_context_cleanup(cs->cst);
    }

    /* Prepare a new CS. */
    cs->base.current.buf = cs->csc->buf;
    cs->base.current.cdw = 0;
    cs->base.used_vram = 0;
    cs->base.used_gart = 0;

    if (cs->ring_type == RING_GFX)
        cs->ws->num_gfx_IBs++;
    else if (cs->ring_type == RING_DMA)
        cs->ws->num_sdma_IBs++;
    return 0;
}

static void radeon_drm_cs_destroy(struct radeon_winsys_cs *rcs)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);

    radeon_drm_cs_sync_flush(rcs);
    util_queue_fence_destroy(&cs->flush_completed);
    radeon_cs_context_cleanup(&cs->csc1);
    radeon_cs_context_cleanup(&cs->csc2);
    p_atomic_dec(&cs->ws->num_cs);
    radeon_destroy_cs_context(&cs->csc1);
    radeon_destroy_cs_context(&cs->csc2);
    radeon_fence_reference(&cs->next_fence, NULL);
    FREE(cs);
}

static bool radeon_bo_is_referenced(struct radeon_winsys_cs *rcs,
                                    struct pb_buffer *_buf,
                                    enum radeon_bo_usage usage)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    struct radeon_bo *bo = (struct radeon_bo*)_buf;
    int index;

    if (!bo->num_cs_references)
        return false;

    index = radeon_lookup_buffer(cs->csc, bo);
    if (index == -1)
        return false;

    if (!bo->handle)
        index = cs->csc->slab_buffers[index].u.slab.real_idx;

    if ((usage & RADEON_USAGE_WRITE) && cs->csc->relocs[index].write_domain)
        return true;
    if ((usage & RADEON_USAGE_READ) && cs->csc->relocs[index].read_domains)
        return true;

    return false;
}

/* FENCES */

static struct pipe_fence_handle *
radeon_cs_create_fence(struct radeon_winsys_cs *rcs)
{
    struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
    struct pb_buffer *fence;

    /* Create a fence, which is a dummy BO. */
    fence = cs->ws->base.buffer_create(&cs->ws->base, 1, 1,
                                       RADEON_DOMAIN_GTT, RADEON_FLAG_NO_SUBALLOC);
    if (!fence)
       return NULL;

    /* Add the fence as a dummy relocation. */
    cs->ws->base.cs_add_buffer(rcs, fence,
                              RADEON_USAGE_READWRITE, RADEON_DOMAIN_GTT,
                              RADEON_PRIO_FENCE);
    return (struct pipe_fence_handle*)fence;
}

static bool radeon_fence_wait(struct radeon_winsys *ws,
                              struct pipe_fence_handle *fence,
                              uint64_t timeout)
{
    return ws->buffer_wait((struct pb_buffer*)fence, timeout,
                           RADEON_USAGE_READWRITE);
}

static void radeon_fence_reference(struct pipe_fence_handle **dst,
                                   struct pipe_fence_handle *src)
{
    pb_reference((struct pb_buffer**)dst, (struct pb_buffer*)src);
}

static struct pipe_fence_handle *
radeon_drm_cs_get_next_fence(struct radeon_winsys_cs *rcs)
{
   struct radeon_drm_cs *cs = radeon_drm_cs(rcs);
   struct pipe_fence_handle *fence = NULL;

   if (cs->next_fence) {
      radeon_fence_reference(&fence, cs->next_fence);
      return fence;
   }

   fence = radeon_cs_create_fence(rcs);
   if (!fence)
      return NULL;

   radeon_fence_reference(&cs->next_fence, fence);
   return fence;
}

void radeon_drm_cs_init_functions(struct radeon_drm_winsys *ws)
{
    ws->base.ctx_create = radeon_drm_ctx_create;
    ws->base.ctx_destroy = radeon_drm_ctx_destroy;
    ws->base.cs_create = radeon_drm_cs_create;
    ws->base.cs_destroy = radeon_drm_cs_destroy;
    ws->base.cs_add_buffer = radeon_drm_cs_add_buffer;
    ws->base.cs_lookup_buffer = radeon_drm_cs_lookup_buffer;
    ws->base.cs_validate = radeon_drm_cs_validate;
    ws->base.cs_check_space = radeon_drm_cs_check_space;
    ws->base.cs_get_buffer_list = radeon_drm_cs_get_buffer_list;
    ws->base.cs_flush = radeon_drm_cs_flush;
    ws->base.cs_get_next_fence = radeon_drm_cs_get_next_fence;
    ws->base.cs_is_buffer_referenced = radeon_bo_is_referenced;
    ws->base.cs_sync_flush = radeon_drm_cs_sync_flush;
    ws->base.fence_wait = radeon_fence_wait;
    ws->base.fence_reference = radeon_fence_reference;
}
