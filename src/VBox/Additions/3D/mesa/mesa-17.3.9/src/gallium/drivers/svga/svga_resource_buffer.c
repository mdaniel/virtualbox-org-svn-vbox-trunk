/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#include "svga_cmd.h"

#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "os/os_thread.h"
#include "util/u_math.h"
#include "util/u_memory.h"
#include "util/u_resource.h"

#include "svga_context.h"
#include "svga_screen.h"
#include "svga_resource_buffer.h"
#include "svga_resource_buffer_upload.h"
#include "svga_winsys.h"
#include "svga_debug.h"


/**
 * Vertex and index buffers need hardware backing.  Constant buffers
 * do not.  No other types of buffers currently supported.
 */
static inline boolean
svga_buffer_needs_hw_storage(unsigned usage)
{
   return (usage & (PIPE_BIND_VERTEX_BUFFER | PIPE_BIND_INDEX_BUFFER |
                    PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_STREAM_OUTPUT)) != 0;
}


/**
 * Create a buffer transfer.
 *
 * Unlike texture DMAs (which are written immediately to the command buffer and
 * therefore inherently serialized with other context operations), for buffers
 * we try to coalesce multiple range mappings (i.e, multiple calls to this
 * function) into a single DMA command, for better efficiency in command
 * processing.  This means we need to exercise extra care here to ensure that
 * the end result is exactly the same as if one DMA was used for every mapped
 * range.
 */
static void *
svga_buffer_transfer_map(struct pipe_context *pipe,
                         struct pipe_resource *resource,
                         unsigned level,
                         unsigned usage,
                         const struct pipe_box *box,
                         struct pipe_transfer **ptransfer)
{
   struct svga_context *svga = svga_context(pipe);
   struct svga_screen *ss = svga_screen(pipe->screen);
   struct svga_buffer *sbuf = svga_buffer(resource);
   struct pipe_transfer *transfer;
   uint8_t *map = NULL;
   int64_t begin = svga_get_time(svga);

   SVGA_STATS_TIME_PUSH(svga_sws(svga), SVGA_STATS_TIME_BUFFERTRANSFERMAP);

   assert(box->y == 0);
   assert(box->z == 0);
   assert(box->height == 1);
   assert(box->depth == 1);

   transfer = MALLOC_STRUCT(pipe_transfer);
   if (!transfer) {
      goto done;
   }

   transfer->resource = resource;
   transfer->level = level;
   transfer->usage = usage;
   transfer->box = *box;
   transfer->stride = 0;
   transfer->layer_stride = 0;

   if (usage & PIPE_TRANSFER_WRITE) {
      /* If we write to the buffer for any reason, free any saved translated
       * vertices.
       */
      pipe_resource_reference(&sbuf->translated_indices.buffer, NULL);
   }

   if ((usage & PIPE_TRANSFER_READ) && sbuf->dirty) {
      enum pipe_error ret;

      /* Host-side buffers can only be dirtied with vgpu10 features
       * (streamout and buffer copy).
       */
      assert(svga_have_vgpu10(svga));

      if (!sbuf->user) {
         (void) svga_buffer_handle(svga, resource, sbuf->bind_flags);
      }

      if (sbuf->dma.pending > 0) {
         svga_buffer_upload_flush(svga, sbuf);
         svga_context_finish(svga);
      }

      assert(sbuf->handle);

      ret = SVGA3D_vgpu10_ReadbackSubResource(svga->swc, sbuf->handle, 0);
      if (ret != PIPE_OK) {
         svga_context_flush(svga, NULL);
         ret = SVGA3D_vgpu10_ReadbackSubResource(svga->swc, sbuf->handle, 0);
         assert(ret == PIPE_OK);
      }

      svga->hud.num_readbacks++;

      svga_context_finish(svga);

      sbuf->dirty = FALSE;
   }

   if (usage & PIPE_TRANSFER_WRITE) {
      if (usage & PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE) {
         /*
          * Flush any pending primitives, finish writing any pending DMA
          * commands, and tell the host to discard the buffer contents on
          * the next DMA operation.
          */

         svga_hwtnl_flush_buffer(svga, resource);

         if (sbuf->dma.pending) {
            svga_buffer_upload_flush(svga, sbuf);

            /*
             * Instead of flushing the context command buffer, simply discard
             * the current hwbuf, and start a new one.
             * With GB objects, the map operation takes care of this
             * if passed the PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE flag,
             * and the old backing store is busy.
             */

            if (!svga_have_gb_objects(svga))
               svga_buffer_destroy_hw_storage(ss, sbuf);
         }

         sbuf->map.num_ranges = 0;
         sbuf->dma.flags.discard = TRUE;
      }

      if (usage & PIPE_TRANSFER_UNSYNCHRONIZED) {
         if (!sbuf->map.num_ranges) {
            /*
             * No pending ranges to upload so far, so we can tell the host to
             * not synchronize on the next DMA command.
             */

            sbuf->dma.flags.unsynchronized = TRUE;
         }
      } else {
         /*
          * Synchronizing, so flush any pending primitives, finish writing any
          * pending DMA command, and ensure the next DMA will be done in order.
          */

         svga_hwtnl_flush_buffer(svga, resource);

         if (sbuf->dma.pending) {
            svga_buffer_upload_flush(svga, sbuf);

            if (svga_buffer_has_hw_storage(sbuf)) {
               /*
                * We have a pending DMA upload from a hardware buffer, therefore
                * we need to ensure that the host finishes processing that DMA
                * command before the state tracker can start overwriting the
                * hardware buffer.
                *
                * XXX: This could be avoided by tying the hardware buffer to
                * the transfer (just as done with textures), which would allow
                * overlapping DMAs commands to be queued on the same context
                * buffer. However, due to the likelihood of software vertex
                * processing, it is more convenient to hold on to the hardware
                * buffer, allowing to quickly access the contents from the CPU
                * without having to do a DMA download from the host.
                */

               if (usage & PIPE_TRANSFER_DONTBLOCK) {
                  /*
                   * Flushing the command buffer here will most likely cause
                   * the map of the hwbuf below to block, so preemptively
                   * return NULL here if DONTBLOCK is set to prevent unnecessary
                   * command buffer flushes.
                   */

                  FREE(transfer);
                  goto done;
               }

               svga_context_flush(svga, NULL);
            }
         }

         sbuf->dma.flags.unsynchronized = FALSE;
      }
   }

   if (!sbuf->swbuf && !svga_buffer_has_hw_storage(sbuf)) {
      if (svga_buffer_create_hw_storage(ss, sbuf, sbuf->bind_flags) != PIPE_OK) {
         /*
          * We can't create a hardware buffer big enough, so create a malloc
          * buffer instead.
          */
         if (0) {
            debug_printf("%s: failed to allocate %u KB of DMA, "
                         "splitting DMA transfers\n",
                         __FUNCTION__,
                         (sbuf->b.b.width0 + 1023)/1024);
         }

         sbuf->swbuf = align_malloc(sbuf->b.b.width0, 16);
         if (!sbuf->swbuf) {
            FREE(transfer);
            goto done;
         }
      }
   }

   if (sbuf->swbuf) {
      /* User/malloc buffer */
      map = sbuf->swbuf;
   }
   else if (svga_buffer_has_hw_storage(sbuf)) {
      boolean retry;

      map = svga_buffer_hw_storage_map(svga, sbuf, transfer->usage, &retry);
      if (map == NULL && retry) {
         /*
          * At this point, svga_buffer_get_transfer() has already
          * hit the DISCARD_WHOLE_RESOURCE path and flushed HWTNL
          * for this buffer.
          */
         svga_context_flush(svga, NULL);
         map = svga_buffer_hw_storage_map(svga, sbuf, transfer->usage, &retry);
      }
   }
   else {
      map = NULL;
   }

   if (map) {
      ++sbuf->map.count;
      map += transfer->box.x;
      *ptransfer = transfer;
   } else {
      FREE(transfer);
   }

   svga->hud.map_buffer_time += (svga_get_time(svga) - begin);

done:
   SVGA_STATS_TIME_POP(svga_sws(svga));
   return map;
}


static void
svga_buffer_transfer_flush_region( struct pipe_context *pipe,
                                   struct pipe_transfer *transfer,
                                   const struct pipe_box *box)
{
   struct svga_screen *ss = svga_screen(pipe->screen);
   struct svga_buffer *sbuf = svga_buffer(transfer->resource);

   unsigned offset = transfer->box.x + box->x;
   unsigned length = box->width;

   assert(transfer->usage & PIPE_TRANSFER_WRITE);
   assert(transfer->usage & PIPE_TRANSFER_FLUSH_EXPLICIT);

   mtx_lock(&ss->swc_mutex);
   svga_buffer_add_range(sbuf, offset, offset + length);
   mtx_unlock(&ss->swc_mutex);
}


static void
svga_buffer_transfer_unmap( struct pipe_context *pipe,
                            struct pipe_transfer *transfer )
{
   struct svga_screen *ss = svga_screen(pipe->screen);
   struct svga_context *svga = svga_context(pipe);
   struct svga_buffer *sbuf = svga_buffer(transfer->resource);

   SVGA_STATS_TIME_PUSH(svga_sws(svga), SVGA_STATS_TIME_BUFFERTRANSFERUNMAP);

   mtx_lock(&ss->swc_mutex);

   assert(sbuf->map.count);
   if (sbuf->map.count) {
      --sbuf->map.count;
   }

   if (svga_buffer_has_hw_storage(sbuf)) {
      /* Note: we may wind up flushing here and unmapping other buffers
       * which leads to recursively locking ss->swc_mutex.
       */
      svga_buffer_hw_storage_unmap(svga, sbuf);
   }

   if (transfer->usage & PIPE_TRANSFER_WRITE) {
      if (!(transfer->usage & PIPE_TRANSFER_FLUSH_EXPLICIT)) {
         /*
          * Mapped range not flushed explicitly, so flush the whole buffer,
          * and tell the host to discard the contents when processing the DMA
          * command.
          */

         SVGA_DBG(DEBUG_DMA, "flushing the whole buffer\n");

         sbuf->dma.flags.discard = TRUE;

#ifndef VBOX_WITH_MESA3D_NINE_SVGA
         svga_buffer_add_range(sbuf, 0, sbuf->b.b.width0);
#else
         svga_buffer_add_range(sbuf, transfer->box.x, transfer->box.x + transfer->box.width);
#endif
      }
   }

   mtx_unlock(&ss->swc_mutex);
   FREE(transfer);
   SVGA_STATS_TIME_POP(svga_sws(svga));
}


static void
svga_buffer_destroy( struct pipe_screen *screen,
		     struct pipe_resource *buf )
{
   struct svga_screen *ss = svga_screen(screen); 
   struct svga_buffer *sbuf = svga_buffer( buf );

   assert(!p_atomic_read(&buf->reference.count));

   assert(!sbuf->dma.pending);

   if (sbuf->handle)
      svga_buffer_destroy_host_surface(ss, sbuf);

   if (sbuf->uploaded.buffer)
      pipe_resource_reference(&sbuf->uploaded.buffer, NULL);

   if (sbuf->hwbuf)
      svga_buffer_destroy_hw_storage(ss, sbuf);

   if (sbuf->swbuf && !sbuf->user)
      align_free(sbuf->swbuf);

   pipe_resource_reference(&sbuf->translated_indices.buffer, NULL);

   ss->hud.total_resource_bytes -= sbuf->size;
   assert(ss->hud.num_resources > 0);
   if (ss->hud.num_resources > 0)
      ss->hud.num_resources--;

   FREE(sbuf);
}


struct u_resource_vtbl svga_buffer_vtbl =
{
   u_default_resource_get_handle,      /* get_handle */
   svga_buffer_destroy,		     /* resource_destroy */
   svga_buffer_transfer_map,	     /* transfer_map */
   svga_buffer_transfer_flush_region,  /* transfer_flush_region */
   svga_buffer_transfer_unmap,	     /* transfer_unmap */
};



struct pipe_resource *
svga_buffer_create(struct pipe_screen *screen,
		   const struct pipe_resource *template)
{
   struct svga_screen *ss = svga_screen(screen);
   struct svga_buffer *sbuf;
   unsigned bind_flags;

   SVGA_STATS_TIME_PUSH(ss->sws, SVGA_STATS_TIME_CREATEBUFFER);

   sbuf = CALLOC_STRUCT(svga_buffer);
   if (!sbuf)
      goto error1;

   sbuf->b.b = *template;
   sbuf->b.vtbl = &svga_buffer_vtbl;
   pipe_reference_init(&sbuf->b.b.reference, 1);
   sbuf->b.b.screen = screen;
   bind_flags = template->bind;

   LIST_INITHEAD(&sbuf->surfaces);

   if (bind_flags & PIPE_BIND_CONSTANT_BUFFER) {
      /* Constant buffers can only have the PIPE_BIND_CONSTANT_BUFFER
       * flag set.
       */
      if (ss->sws->have_vgpu10) {
         bind_flags = PIPE_BIND_CONSTANT_BUFFER;
      }
   }

   /* Although svga device only requires constant buffer size to be
    * in multiples of 16, in order to allow bind_flags promotion,
    * we are mandating all buffer size to be in multiples of 16.
    */
   sbuf->b.b.width0 = align(sbuf->b.b.width0, 16);

   if (svga_buffer_needs_hw_storage(bind_flags)) {

      /* If the buffer is not used for constant buffer, set
       * the vertex/index bind flags as well so that the buffer will be
       * accepted for those uses.
       * Note that the PIPE_BIND_ flags we get from the state tracker are
       * just a hint about how the buffer may be used.  And OpenGL buffer
       * object may be used for many different things.
       * Also note that we do not unconditionally set the streamout
       * bind flag since streamout buffer is an output buffer and
       * might have performance implication.
       */
      if (!(template->bind & PIPE_BIND_CONSTANT_BUFFER)) {
         /* Not a constant buffer.  The buffer may be used for vertex data
          * or indexes.
          */
         bind_flags |= (PIPE_BIND_VERTEX_BUFFER |
                        PIPE_BIND_INDEX_BUFFER);
      }

      if (svga_buffer_create_host_surface(ss, sbuf, bind_flags) != PIPE_OK)
         goto error2;
   }
   else {
      sbuf->swbuf = align_malloc(sbuf->b.b.width0, 64);
      if (!sbuf->swbuf)
         goto error2;
   }

   debug_reference(&sbuf->b.b.reference,
                   (debug_reference_descriptor)debug_describe_resource, 0);

   sbuf->bind_flags = bind_flags;
   sbuf->size = util_resource_size(&sbuf->b.b);
   ss->hud.total_resource_bytes += sbuf->size;

   ss->hud.num_resources++;
   SVGA_STATS_TIME_POP(ss->sws);

   return &sbuf->b.b;

error2:
   FREE(sbuf);
error1:
   SVGA_STATS_TIME_POP(ss->sws);
   return NULL;
}


struct pipe_resource *
svga_user_buffer_create(struct pipe_screen *screen,
                        void *ptr,
                        unsigned bytes,
			unsigned bind)
{
   struct svga_buffer *sbuf;
   struct svga_screen *ss = svga_screen(screen);

   sbuf = CALLOC_STRUCT(svga_buffer);
   if (!sbuf)
      goto no_sbuf;

   pipe_reference_init(&sbuf->b.b.reference, 1);
   sbuf->b.vtbl = &svga_buffer_vtbl;
   sbuf->b.b.screen = screen;
   sbuf->b.b.format = PIPE_FORMAT_R8_UNORM; /* ?? */
   sbuf->b.b.usage = PIPE_USAGE_IMMUTABLE;
   sbuf->b.b.bind = bind;
   sbuf->b.b.width0 = bytes;
   sbuf->b.b.height0 = 1;
   sbuf->b.b.depth0 = 1;
   sbuf->b.b.array_size = 1;

   sbuf->bind_flags = bind;
   sbuf->swbuf = ptr;
   sbuf->user = TRUE;

   debug_reference(&sbuf->b.b.reference,
                   (debug_reference_descriptor)debug_describe_resource, 0);

   ss->hud.num_resources++;

   return &sbuf->b.b;

no_sbuf:
   return NULL;
}



