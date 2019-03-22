/**********************************************************
 * Copyright 2009-2015 VMware, Inc.  All rights reserved.
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
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "pipe/p_defines.h"
#include "vmw_surface.h"
#include "vmw_screen.h"
#include "vmw_buffer.h"
#include "vmw_context.h"
#include "pipebuffer/pb_bufmgr.h"


void *
vmw_svga_winsys_surface_map(struct svga_winsys_context *swc,
                            struct svga_winsys_surface *srf,
                            unsigned flags, boolean *retry)
{
   struct vmw_svga_winsys_surface *vsrf = vmw_svga_winsys_surface(srf);
   void *data = NULL;
   struct pb_buffer *pb_buf;
   uint32_t pb_flags;
   struct vmw_winsys_screen *vws = vsrf->screen;
   
   *retry = FALSE;
   assert((flags & (PIPE_TRANSFER_READ | PIPE_TRANSFER_WRITE)) != 0);
   mtx_lock(&vsrf->mutex);

   if (vsrf->mapcount) {
      /*
       * Only allow multiple readers to map.
       */
      if ((flags & PIPE_TRANSFER_WRITE) ||
          (vsrf->map_mode & PIPE_TRANSFER_WRITE))
         goto out_unlock;
      
      data = vsrf->data;
      goto out_mapped;
   }

   vsrf->rebind = FALSE;

   /*
    * If we intend to read, there's no point discarding the
    * data if busy.
    */
   if (flags & PIPE_TRANSFER_READ || vsrf->shared)
      flags &= ~PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE;

   /*
    * Discard is a hint to a synchronized map.
    */
   if (flags & PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE)
      flags &= ~PIPE_TRANSFER_UNSYNCHRONIZED;

   /*
    * The surface is allowed to be referenced on the command stream iff
    * we're mapping unsynchronized or discard. This is an early check.
    * We need to recheck after a failing discard map.
    */
   if (!(flags & (PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE |
                  PIPE_TRANSFER_UNSYNCHRONIZED)) &&
       p_atomic_read(&vsrf->validated)) {
      *retry = TRUE;
      goto out_unlock;
   }

   pb_flags = flags & (PIPE_TRANSFER_READ_WRITE | PIPE_TRANSFER_UNSYNCHRONIZED);

   if (flags & PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE) {
      struct pb_manager *provider;
      struct pb_desc desc;

      /*
       * First, if possible, try to map existing storage with DONTBLOCK.
       */
      if (!p_atomic_read(&vsrf->validated)) {
         data = vmw_svga_winsys_buffer_map(&vws->base, vsrf->buf,
                                           PIPE_TRANSFER_DONTBLOCK | pb_flags);
         if (data)
            goto out_mapped;
      } 

      /*
       * Attempt to get a new buffer.
       */
      provider = vws->pools.mob_fenced;
      memset(&desc, 0, sizeof(desc));
      desc.alignment = 4096;
      pb_buf = provider->create_buffer(provider, vsrf->size, &desc);
      if (pb_buf != NULL) {
         struct svga_winsys_buffer *vbuf =
            vmw_svga_winsys_buffer_wrap(pb_buf);

         data = vmw_svga_winsys_buffer_map(&vws->base, vbuf, pb_flags);
         if (data) {
            vsrf->rebind = TRUE;
            /*
             * We've discarded data on this surface and thus
             * it's data is no longer consider referenced.
             */
            vmw_swc_surface_clear_reference(swc, vsrf);
            if (vsrf->buf)
               vmw_svga_winsys_buffer_destroy(&vws->base, vsrf->buf);
            vsrf->buf = vbuf;
            goto out_mapped;
         } else
            vmw_svga_winsys_buffer_destroy(&vws->base, vbuf);
      }
      /*
       * We couldn't get and map a new buffer for some reason.
       * Fall through to an ordinary map.
       * But tell pipe driver to flush now if already on validate list,
       * Otherwise we'll overwrite previous contents.
       */
      if (!(flags & PIPE_TRANSFER_UNSYNCHRONIZED) && 
          p_atomic_read(&vsrf->validated)) {
         *retry = TRUE;
         goto out_unlock;
      }
   }

   pb_flags |= (flags & PIPE_TRANSFER_DONTBLOCK);
   data = vmw_svga_winsys_buffer_map(&vws->base, vsrf->buf, pb_flags);
   if (data == NULL)
      goto out_unlock;

out_mapped:
   ++vsrf->mapcount;
   vsrf->data = data;
   vsrf->map_mode = flags & (PIPE_TRANSFER_READ | PIPE_TRANSFER_WRITE);
out_unlock:
   mtx_unlock(&vsrf->mutex);
   return data;
}


void
vmw_svga_winsys_surface_unmap(struct svga_winsys_context *swc,
                              struct svga_winsys_surface *srf,
                              boolean *rebind)
{
   struct vmw_svga_winsys_surface *vsrf = vmw_svga_winsys_surface(srf);
   mtx_lock(&vsrf->mutex);
   if (--vsrf->mapcount == 0) {
      *rebind = vsrf->rebind;
      vsrf->rebind = FALSE;
      vmw_svga_winsys_buffer_unmap(&vsrf->screen->base, vsrf->buf);
   } else {
      *rebind = FALSE;
   }
   mtx_unlock(&vsrf->mutex);
}

enum pipe_error
vmw_svga_winsys_surface_invalidate(struct svga_winsys_context *swc,
                                   struct svga_winsys_surface *surf)
{
   /* this is a noop since surface invalidation is not needed for DMA path.
    * DMA is enabled when guest-backed surface is not enabled or
    * guest-backed dma is enabled.  Since guest-backed dma is enabled
    * when guest-backed surface is enabled, that implies DMA is always enabled;
    * hence, surface invalidation is not needed.
    */
   return PIPE_OK;
}

void
vmw_svga_winsys_surface_reference(struct vmw_svga_winsys_surface **pdst,
                                  struct vmw_svga_winsys_surface *src)
{
   struct pipe_reference *src_ref;
   struct pipe_reference *dst_ref;
   struct vmw_svga_winsys_surface *dst;

   if(pdst == NULL || *pdst == src)
      return;

   dst = *pdst;

   src_ref = src ? &src->refcnt : NULL;
   dst_ref = dst ? &dst->refcnt : NULL;

   if (pipe_reference(dst_ref, src_ref)) {
      if (dst->buf)
         vmw_svga_winsys_buffer_destroy(&dst->screen->base, dst->buf);
      vmw_ioctl_surface_destroy(dst->screen, dst->sid);
#ifdef DEBUG
      /* to detect dangling pointers */
      assert(p_atomic_read(&dst->validated) == 0);
      dst->sid = SVGA3D_INVALID_ID;
#endif
      mtx_destroy(&dst->mutex);
      FREE(dst);
   }

   *pdst = src;
}
