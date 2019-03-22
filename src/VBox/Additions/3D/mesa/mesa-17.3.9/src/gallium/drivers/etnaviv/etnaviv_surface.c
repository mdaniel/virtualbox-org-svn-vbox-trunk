/*
 * Copyright (c) 2012-2013 Etnaviv Project
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

#include "etnaviv_surface.h"
#include "etnaviv_screen.h"

#include "etnaviv_clear_blit.h"
#include "etnaviv_context.h"
#include "etnaviv_translate.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

#include "hw/common.xml.h"

static struct pipe_surface *
etna_create_surface(struct pipe_context *pctx, struct pipe_resource *prsc,
                    const struct pipe_surface *templat)
{
   struct etna_context *ctx = etna_context(pctx);
   struct etna_resource *rsc = etna_resource(prsc);
   struct etna_surface *surf = CALLOC_STRUCT(etna_surface);

   if (!surf)
      return NULL;

   assert(templat->u.tex.first_layer == templat->u.tex.last_layer);
   unsigned layer = templat->u.tex.first_layer;
   unsigned level = templat->u.tex.level;
   assert(layer < rsc->base.array_size);

   surf->base.context = pctx;

   pipe_reference_init(&surf->base.reference, 1);
   pipe_resource_reference(&surf->base.texture, &rsc->base);

   /* Allocate a TS for the resource if there isn't one yet,
    * and it is allowed by the hw (width is a multiple of 16).
    * Avoid doing this for GPUs with MC1.0, as kernel sources
    * indicate the tile status module bypasses the memory
    * offset and MMU. */

   if (VIV_FEATURE(ctx->screen, chipFeatures, FAST_CLEAR) &&
       VIV_FEATURE(ctx->screen, chipMinorFeatures0, MC20) &&
       !rsc->ts_bo &&
       (rsc->levels[level].padded_width & ETNA_RS_WIDTH_MASK) == 0 &&
       (rsc->levels[level].padded_height & ETNA_RS_HEIGHT_MASK) == 0) {
      etna_screen_resource_alloc_ts(pctx->screen, rsc);
   }

   surf->base.texture = &rsc->base;
   surf->base.format = rsc->base.format;
   surf->base.width = rsc->levels[level].width;
   surf->base.height = rsc->levels[level].height;
   surf->base.writable = templat->writable; /* what is this for anyway */
   surf->base.u = templat->u;

   surf->level = &rsc->levels[level]; /* Keep pointer to actual level to set
                                       * clear color on underlying resource
                                       * instead of surface */
   surf->surf = rsc->levels [level];  /* Make copy of level to narrow down
                                       * address to layer */

   /* XXX we don't really need a copy but it's convenient */
   surf->surf.offset += layer * surf->surf.layer_stride;

   struct etna_resource_level *lev = &rsc->levels[level];

   /* Setup template relocations for this surface */
   for (unsigned pipe = 0; pipe < ctx->specs.pixel_pipes; ++pipe) {
      surf->reloc[pipe].bo = rsc->bo;
      surf->reloc[pipe].offset = surf->surf.offset;
      surf->reloc[pipe].flags = 0;
   }

   /* In single buffer mode, both pixel pipes must point to the same address,
    * for multi-tiled surfaces on the other hand the second pipe is expected to
    * point halfway the image vertically.
    */
   if (rsc->layout & ETNA_LAYOUT_BIT_MULTI)
      surf->reloc[1].offset = surf->surf.offset + lev->stride * lev->padded_height / 2;

   if (surf->surf.ts_size) {
      unsigned int layer_offset = layer * surf->surf.ts_layer_stride;
      assert(layer_offset < surf->surf.ts_size);

      surf->surf.ts_offset += layer_offset;
      surf->surf.ts_size -= layer_offset;
      surf->surf.ts_valid = false;

      surf->ts_reloc.bo = rsc->ts_bo;
      surf->ts_reloc.offset = surf->surf.ts_offset;
      surf->ts_reloc.flags = 0;

      /* This (ab)uses the RS as a plain buffer memset().
       * Currently uses a fixed row size of 64 bytes. Some benchmarking with
       * different sizes may be in order. */
      struct etna_bo *ts_bo = etna_resource(surf->base.texture)->ts_bo;
      etna_compile_rs_state(ctx, &surf->clear_command, &(struct rs_state) {
         .source_format = RS_FORMAT_A8R8G8B8,
         .dest_format = RS_FORMAT_A8R8G8B8,
         .dest = ts_bo,
         .dest_offset = surf->surf.ts_offset,
         .dest_stride = 0x40,
         .dest_tiling = ETNA_LAYOUT_TILED,
         .dither = {0xffffffff, 0xffffffff},
         .width = 16,
         .height = etna_align_up(surf->surf.ts_size / 0x40, 4),
         .clear_value = {ctx->specs.ts_clear_value},
         .clear_mode = VIVS_RS_CLEAR_CONTROL_MODE_ENABLED1,
         .clear_bits = 0xffff
      });
   } else {
      etna_rs_gen_clear_surface(ctx, surf, surf->level->clear_value);
   }

   return &surf->base;
}

static void
etna_surface_destroy(struct pipe_context *pctx, struct pipe_surface *psurf)
{
   pipe_resource_reference(&psurf->texture, NULL);
   FREE(psurf);
}

void
etna_surface_init(struct pipe_context *pctx)
{
   pctx->create_surface = etna_create_surface;
   pctx->surface_destroy = etna_surface_destroy;
}
