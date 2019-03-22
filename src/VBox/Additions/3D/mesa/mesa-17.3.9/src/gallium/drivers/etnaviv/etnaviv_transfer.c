/*
 * Copyright (c) 2012-2015 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Wladimir J. van der Laan <laanwj@gmail.com>
 */

#include "etnaviv_transfer.h"
#include "etnaviv_clear_blit.h"
#include "etnaviv_context.h"
#include "etnaviv_debug.h"
#include "etnaviv_screen.h"

#include "pipe/p_defines.h"
#include "pipe/p_format.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "util/u_format.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_surface.h"
#include "util/u_transfer.h"

#include <drm_fourcc.h>

/* Compute offset into a 1D/2D/3D buffer of a certain box.
 * This box must be aligned to the block width and height of the
 * underlying format. */
static inline size_t
etna_compute_offset(enum pipe_format format, const struct pipe_box *box,
                    size_t stride, size_t layer_stride)
{
   return box->z * layer_stride +
          box->y / util_format_get_blockheight(format) * stride +
          box->x / util_format_get_blockwidth(format) *
             util_format_get_blocksize(format);
}

static void
etna_transfer_unmap(struct pipe_context *pctx, struct pipe_transfer *ptrans)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_transfer *trans = etna_transfer(ptrans);
   struct etna_resource *rsc = etna_resource(ptrans->resource);

   /* XXX
    * When writing to a resource that is already in use, replace the resource
    * with a completely new buffer
    * and free the old one using a fenced free.
    * The most tricky case to implement will be: tiled or supertiled surface,
    * partial write, target not aligned to 4/64. */
   assert(ptrans->level <= rsc->base.last_level);

   if (rsc->texture && !etna_resource_newer(rsc, etna_resource(rsc->texture)))
      rsc = etna_resource(rsc->texture); /* switch to using the texture resource */

   /*
    * Temporary resources are always pulled into the CPU domain, must push them
    * back into GPU domain before the RS execs the blit to the base resource.
    */
   if (trans->rsc)
      etna_bo_cpu_fini(etna_resource(trans->rsc)->bo);

   if (ptrans->usage & PIPE_TRANSFER_WRITE) {
      if (trans->rsc) {
         /* We have a temporary resource due to either tile status or
          * tiling format. Write back the updated buffer contents.
          * FIXME: we need to invalidate the tile status. */
         etna_copy_resource_box(pctx, ptrans->resource, trans->rsc, ptrans->level, &ptrans->box);
      } else if (trans->staging) {
         /* map buffer object */
         struct etna_resource_level *res_level = &rsc->levels[ptrans->level];
         void *mapped = etna_bo_map(rsc->bo) + res_level->offset;

         if (rsc->layout == ETNA_LAYOUT_TILED) {
            etna_texture_tile(
               mapped + ptrans->box.z * res_level->layer_stride,
               trans->staging, ptrans->box.x, ptrans->box.y,
               res_level->stride, ptrans->box.width, ptrans->box.height,
               ptrans->stride, util_format_get_blocksize(rsc->base.format));
         } else if (rsc->layout == ETNA_LAYOUT_LINEAR) {
            util_copy_box(mapped, rsc->base.format, res_level->stride,
                          res_level->layer_stride, ptrans->box.x,
                          ptrans->box.y, ptrans->box.z, ptrans->box.width,
                          ptrans->box.height, ptrans->box.depth,
                          trans->staging, ptrans->stride,
                          ptrans->layer_stride, 0, 0, 0 /* src x,y,z */);
         } else {
            BUG("unsupported tiling %i", rsc->layout);
         }

         FREE(trans->staging);
      }

      rsc->seqno++;

      if (rsc->base.bind & PIPE_BIND_SAMPLER_VIEW) {
         ctx->dirty |= ETNA_DIRTY_TEXTURE_CACHES;
      }
   }

   /*
    * Transfers without a temporary are only pulled into the CPU domain if they
    * are not mapped unsynchronized. If they are, must push them back into GPU
    * domain after CPU access is finished.
    */
   if (!trans->rsc && !(ptrans->usage & PIPE_TRANSFER_UNSYNCHRONIZED))
      etna_bo_cpu_fini(rsc->bo);

   pipe_resource_reference(&trans->rsc, NULL);
   pipe_resource_reference(&ptrans->resource, NULL);
   slab_free(&ctx->transfer_pool, trans);
}

static void *
etna_transfer_map(struct pipe_context *pctx, struct pipe_resource *prsc,
                  unsigned level,
                  unsigned usage,
                  const struct pipe_box *box,
                  struct pipe_transfer **out_transfer)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_resource *rsc = etna_resource(prsc);
   struct etna_transfer *trans;
   struct pipe_transfer *ptrans;
   enum pipe_format format = prsc->format;

   trans = slab_alloc(&ctx->transfer_pool);
   if (!trans)
      return NULL;

   /* slab_alloc() doesn't zero */
   memset(trans, 0, sizeof(*trans));

   ptrans = &trans->base;
   pipe_resource_reference(&ptrans->resource, prsc);
   ptrans->level = level;
   ptrans->usage = usage;
   ptrans->box = *box;

   assert(level <= prsc->last_level);

   /* Upgrade DISCARD_RANGE to WHOLE_RESOURCE if the whole resource is
    * being mapped. If we add buffer reallocation to avoid CPU/GPU sync this
    * check needs to be extended to coherent mappings and shared resources.
    */
   if ((usage & PIPE_TRANSFER_DISCARD_RANGE) &&
       !(usage & PIPE_TRANSFER_UNSYNCHRONIZED) &&
       prsc->last_level == 0 &&
       prsc->width0 == box->width &&
       prsc->height0 == box->height &&
       prsc->depth0 == box->depth &&
       prsc->array_size == 1) {
      usage |= PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE;
   }

   if (rsc->texture && !etna_resource_newer(rsc, etna_resource(rsc->texture))) {
      /* We have a texture resource which is the same age or newer than the
       * render resource. Use the texture resource, which avoids bouncing
       * pixels between the two resources, and we can de-tile it in s/w. */
      rsc = etna_resource(rsc->texture);
   } else if (rsc->ts_bo ||
              (rsc->layout != ETNA_LAYOUT_LINEAR &&
               util_format_get_blocksize(format) > 1 &&
               /* HALIGN 4 resources are incompatible with the resolve engine,
                * so fall back to using software to detile this resource. */
               rsc->halign != TEXTURE_HALIGN_FOUR)) {
      /* If the surface has tile status, we need to resolve it first.
       * The strategy we implement here is to use the RS to copy the
       * depth buffer, filling in the "holes" where the tile status
       * indicates that it's clear. We also do this for tiled
       * resources, but only if the RS can blit them. */
      if (usage & PIPE_TRANSFER_MAP_DIRECTLY) {
         slab_free(&ctx->transfer_pool, trans);
         BUG("unsupported transfer flags %#x with tile status/tiled layout", usage);
         return NULL;
      }

      if (prsc->depth0 > 1) {
         slab_free(&ctx->transfer_pool, trans);
         BUG("resource has depth >1 with tile status");
         return NULL;
      }

      struct pipe_resource templ = *prsc;
      templ.nr_samples = 0;
      templ.bind = PIPE_BIND_RENDER_TARGET;

      trans->rsc = etna_resource_alloc(pctx->screen, ETNA_LAYOUT_LINEAR,
                                       DRM_FORMAT_MOD_LINEAR, &templ);
      if (!trans->rsc) {
         slab_free(&ctx->transfer_pool, trans);
         return NULL;
      }

      /* Need to align the transfer region to satisfy RS restrictions, as we
       * really want to hit the RS blit path here.
       */
      unsigned w_align, h_align;

      if (rsc->layout & ETNA_LAYOUT_BIT_SUPER) {
         w_align = h_align = 64;
      } else {
         w_align = ETNA_RS_WIDTH_MASK + 1;
         h_align = ETNA_RS_HEIGHT_MASK + 1;
      }
      h_align *= ctx->screen->specs.pixel_pipes;

      ptrans->box.width += ptrans->box.x & (w_align - 1);
      ptrans->box.x = ptrans->box.x & ~(w_align - 1);
      ptrans->box.width = align(ptrans->box.width, (ETNA_RS_WIDTH_MASK + 1));
      ptrans->box.height += ptrans->box.y & (h_align - 1);
      ptrans->box.y = ptrans->box.y & ~(h_align - 1);
      ptrans->box.height = align(ptrans->box.height,
                                 (ETNA_RS_HEIGHT_MASK + 1) *
                                  ctx->screen->specs.pixel_pipes);

      if (!(usage & PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE))
         etna_copy_resource_box(pctx, trans->rsc, prsc, level, &ptrans->box);

      /* Switch to using the temporary resource instead */
      rsc = etna_resource(trans->rsc);
   }

   struct etna_resource_level *res_level = &rsc->levels[level];

   /*
    * Always flush if we have the temporary resource and have a copy to this
    * outstanding. Otherwise infer flush requirement from resource access and
    * current GPU usage (reads must wait for GPU writes, writes must have
    * exclusive access to the buffer).
    */
   if ((trans->rsc && (etna_resource(trans->rsc)->status & ETNA_PENDING_WRITE)) ||
       (!trans->rsc &&
        (((usage & PIPE_TRANSFER_READ) && (rsc->status & ETNA_PENDING_WRITE)) ||
        ((usage & PIPE_TRANSFER_WRITE) && rsc->status))))
      pctx->flush(pctx, NULL, 0);

   /* XXX we don't handle PIPE_TRANSFER_FLUSH_EXPLICIT; this flag can be ignored
    * when mapping in-place,
    * but when not in place we need to fire off the copy operation in
    * transfer_flush_region (currently
    * a no-op) instead of unmap. Need to handle this to support
    * ARB_map_buffer_range extension at least.
    */
   /* XXX we don't take care of current operations on the resource; which can
      be, at some point in the pipeline
      which is not yet executed:

      - bound as surface
      - bound through vertex buffer
      - bound through index buffer
      - bound in sampler view
      - used in clear_render_target / clear_depth_stencil operation
      - used in blit
      - used in resource_copy_region

      How do other drivers record this information over course of the rendering
      pipeline?
      Is it necessary at all? Only in case we want to provide a fast path and
      map the resource directly
      (and for PIPE_TRANSFER_MAP_DIRECTLY) and we don't want to force a sync.
      We also need to know whether the resource is in use to determine if a sync
      is needed (or just do it
      always, but that comes at the expense of performance).

      A conservative approximation without too much overhead would be to mark
      all resources that have
      been bound at some point as busy. A drawback would be that accessing
      resources that have
      been bound but are no longer in use for a while still carry a performance
      penalty. On the other hand,
      the program could be using PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE or
      PIPE_TRANSFER_UNSYNCHRONIZED to
      avoid this in the first place...

      A) We use an in-pipe copy engine, and queue the copy operation after unmap
      so that the copy
         will be performed when all current commands have been executed.
         Using the RS is possible, not sure if always efficient. This can also
      do any kind of tiling for us.
         Only possible when PIPE_TRANSFER_DISCARD_RANGE is set.
      B) We discard the entire resource (or at least, the mipmap level) and
      allocate new memory for it.
         Only possible when mapping the entire resource or
      PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE is set.
    */

   /*
    * Pull resources into the CPU domain. Only skipped for unsynchronized
    * transfers without a temporary resource.
    */
   if (trans->rsc || !(usage & PIPE_TRANSFER_UNSYNCHRONIZED)) {
      uint32_t prep_flags = 0;

      if (usage & PIPE_TRANSFER_READ)
         prep_flags |= DRM_ETNA_PREP_READ;
      if (usage & PIPE_TRANSFER_WRITE)
         prep_flags |= DRM_ETNA_PREP_WRITE;

      if (etna_bo_cpu_prep(rsc->bo, prep_flags))
         goto fail_prep;
   }

   /* map buffer object */
   void *mapped = etna_bo_map(rsc->bo);
   if (!mapped)
      goto fail;

   *out_transfer = ptrans;

   if (rsc->layout == ETNA_LAYOUT_LINEAR) {
      ptrans->stride = res_level->stride;
      ptrans->layer_stride = res_level->layer_stride;

      return mapped + res_level->offset +
             etna_compute_offset(prsc->format, box, res_level->stride,
                                 res_level->layer_stride);
   } else {
      unsigned divSizeX = util_format_get_blockwidth(format);
      unsigned divSizeY = util_format_get_blockheight(format);

      /* No direct mappings of tiled, since we need to manually
       * tile/untile.
       */
      if (usage & PIPE_TRANSFER_MAP_DIRECTLY)
         goto fail;

      mapped += res_level->offset;
      ptrans->stride = align(box->width, divSizeX) * util_format_get_blocksize(format); /* row stride in bytes */
      ptrans->layer_stride = align(box->height, divSizeY) * ptrans->stride;
      size_t size = ptrans->layer_stride * box->depth;

      trans->staging = MALLOC(size);
      if (!trans->staging)
         goto fail;

      if (usage & PIPE_TRANSFER_READ) {
         if (rsc->layout == ETNA_LAYOUT_TILED) {
            etna_texture_untile(trans->staging,
                                mapped + ptrans->box.z * res_level->layer_stride,
                                ptrans->box.x, ptrans->box.y, res_level->stride,
                                ptrans->box.width, ptrans->box.height, ptrans->stride,
                                util_format_get_blocksize(rsc->base.format));
         } else if (rsc->layout == ETNA_LAYOUT_LINEAR) {
            util_copy_box(trans->staging, rsc->base.format, ptrans->stride,
                          ptrans->layer_stride, 0, 0, 0, /* dst x,y,z */
                          ptrans->box.width, ptrans->box.height,
                          ptrans->box.depth, mapped, res_level->stride,
                          res_level->layer_stride, ptrans->box.x,
                          ptrans->box.y, ptrans->box.z);
         } else {
            /* TODO supertiling */
            BUG("unsupported tiling %i for reading", rsc->layout);
         }
      }

      return trans->staging;
   }

fail:
   etna_bo_cpu_fini(rsc->bo);
fail_prep:
   etna_transfer_unmap(pctx, ptrans);
   return NULL;
}

static void
etna_transfer_flush_region(struct pipe_context *pctx,
                           struct pipe_transfer *transfer,
                           const struct pipe_box *box)
{
   /* NOOP for now */
}

void
etna_transfer_init(struct pipe_context *pctx)
{
   pctx->transfer_map = etna_transfer_map;
   pctx->transfer_flush_region = etna_transfer_flush_region;
   pctx->transfer_unmap = etna_transfer_unmap;
   pctx->buffer_subdata = u_default_buffer_subdata;
   pctx->texture_subdata = u_default_texture_subdata;
}
