/*
 * Copyright © 2013 Keith Packard
 * Copyright © 2015 Boyan Ding
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/xshmfence.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>

#include <X11/Xlib-xcb.h>

#include "loader_dri3_helper.h"

/* From xmlpool/options.h, user exposed so should be stable */
#define DRI_CONF_VBLANK_NEVER 0
#define DRI_CONF_VBLANK_DEF_INTERVAL_0 1
#define DRI_CONF_VBLANK_DEF_INTERVAL_1 2
#define DRI_CONF_VBLANK_ALWAYS_SYNC 3

/**
 * A cached blit context.
 */
struct loader_dri3_blit_context {
   mtx_t mtx;
   __DRIcontext *ctx;
   __DRIscreen *cur_screen;
   const __DRIcoreExtension *core;
};

/* For simplicity we maintain the cache only for a single screen at a time */
static struct loader_dri3_blit_context blit_context = {
   _MTX_INITIALIZER_NP, NULL
};

static void
dri3_flush_present_events(struct loader_dri3_drawable *draw);

static struct loader_dri3_buffer *
dri3_find_back_alloc(struct loader_dri3_drawable *draw);

/**
 * Do we have blit functionality in the image blit extension?
 *
 * \param draw[in]  The drawable intended to blit from / to.
 * \return  true if we have blit functionality. false otherwise.
 */
static bool loader_dri3_have_image_blit(const struct loader_dri3_drawable *draw)
{
   return draw->ext->image->base.version >= 9 &&
      draw->ext->image->blitImage != NULL;
}

/**
 * Get and lock (for use with the current thread) a dri context associated
 * with the drawable's dri screen. The context is intended to be used with
 * the dri image extension's blitImage method.
 *
 * \param draw[in]  Pointer to the drawable whose dri screen we want a
 * dri context for.
 * \return A dri context or NULL if context creation failed.
 *
 * When the caller is done with the context (even if the context returned was
 * NULL), the caller must call loader_dri3_blit_context_put.
 */
static __DRIcontext *
loader_dri3_blit_context_get(struct loader_dri3_drawable *draw)
{
   mtx_lock(&blit_context.mtx);

   if (blit_context.ctx && blit_context.cur_screen != draw->dri_screen) {
      blit_context.core->destroyContext(blit_context.ctx);
      blit_context.ctx = NULL;
   }

   if (!blit_context.ctx) {
      blit_context.ctx = draw->ext->core->createNewContext(draw->dri_screen,
                                                           NULL, NULL, NULL);
      blit_context.cur_screen = draw->dri_screen;
      blit_context.core = draw->ext->core;
   }

   return blit_context.ctx;
}

/**
 * Release (for use with other threads) a dri context previously obtained using
 * loader_dri3_blit_context_get.
 */
static void
loader_dri3_blit_context_put(void)
{
   mtx_unlock(&blit_context.mtx);
}

/**
 * Blit (parts of) the contents of a DRI image to another dri image
 *
 * \param draw[in]  The drawable which owns the images.
 * \param dst[in]  The destination image.
 * \param src[in]  The source image.
 * \param dstx0[in]  Start destination coordinate.
 * \param dsty0[in]  Start destination coordinate.
 * \param width[in]  Blit width.
 * \param height[in] Blit height.
 * \param srcx0[in]  Start source coordinate.
 * \param srcy0[in]  Start source coordinate.
 * \param flush_flag[in]  Image blit flush flag.
 * \return true iff successful.
 */
static bool
loader_dri3_blit_image(struct loader_dri3_drawable *draw,
                       __DRIimage *dst, __DRIimage *src,
                       int dstx0, int dsty0, int width, int height,
                       int srcx0, int srcy0, int flush_flag)
{
   __DRIcontext *dri_context;
   bool use_blit_context = false;

   if (!loader_dri3_have_image_blit(draw))
      return false;

   dri_context = draw->vtable->get_dri_context(draw);

   if (!dri_context || !draw->vtable->in_current_context(draw)) {
      dri_context = loader_dri3_blit_context_get(draw);
      use_blit_context = true;
      flush_flag |= __BLIT_FLAG_FLUSH;
   }

   if (dri_context)
      draw->ext->image->blitImage(dri_context, dst, src, dstx0, dsty0,
                                  width, height, srcx0, srcy0,
                                  width, height, flush_flag);

   if (use_blit_context)
      loader_dri3_blit_context_put();

   return dri_context != NULL;
}

static inline void
dri3_fence_reset(xcb_connection_t *c, struct loader_dri3_buffer *buffer)
{
   xshmfence_reset(buffer->shm_fence);
}

static inline void
dri3_fence_set(struct loader_dri3_buffer *buffer)
{
   xshmfence_trigger(buffer->shm_fence);
}

static inline void
dri3_fence_trigger(xcb_connection_t *c, struct loader_dri3_buffer *buffer)
{
   xcb_sync_trigger_fence(c, buffer->sync_fence);
}

static inline void
dri3_fence_await(xcb_connection_t *c, struct loader_dri3_drawable *draw,
                 struct loader_dri3_buffer *buffer)
{
   xcb_flush(c);
   xshmfence_await(buffer->shm_fence);
   if (draw) {
      mtx_lock(&draw->mtx);
      dri3_flush_present_events(draw);
      mtx_unlock(&draw->mtx);
   }
}

static void
dri3_update_num_back(struct loader_dri3_drawable *draw)
{
   if (draw->flipping)
      draw->num_back = 3;
   else
      draw->num_back = 2;
}

void
loader_dri3_set_swap_interval(struct loader_dri3_drawable *draw, int interval)
{
   draw->swap_interval = interval;
}

/** dri3_free_render_buffer
 *
 * Free everything associated with one render buffer including pixmap, fence
 * stuff and the driver image
 */
static void
dri3_free_render_buffer(struct loader_dri3_drawable *draw,
                        struct loader_dri3_buffer *buffer)
{
   if (buffer->own_pixmap)
      xcb_free_pixmap(draw->conn, buffer->pixmap);
   xcb_sync_destroy_fence(draw->conn, buffer->sync_fence);
   xshmfence_unmap_shm(buffer->shm_fence);
   draw->ext->image->destroyImage(buffer->image);
   if (buffer->linear_buffer)
      draw->ext->image->destroyImage(buffer->linear_buffer);
   free(buffer);
}

void
loader_dri3_drawable_fini(struct loader_dri3_drawable *draw)
{
   int i;

   draw->ext->core->destroyDrawable(draw->dri_drawable);

   for (i = 0; i < LOADER_DRI3_NUM_BUFFERS; i++) {
      if (draw->buffers[i])
         dri3_free_render_buffer(draw, draw->buffers[i]);
   }

   if (draw->special_event) {
      xcb_void_cookie_t cookie =
         xcb_present_select_input_checked(draw->conn, draw->eid, draw->drawable,
                                          XCB_PRESENT_EVENT_MASK_NO_EVENT);

      xcb_discard_reply(draw->conn, cookie.sequence);
      xcb_unregister_for_special_event(draw->conn, draw->special_event);
   }

   cnd_destroy(&draw->event_cnd);
   mtx_destroy(&draw->mtx);
}

int
loader_dri3_drawable_init(xcb_connection_t *conn,
                          xcb_drawable_t drawable,
                          __DRIscreen *dri_screen,
                          bool is_different_gpu,
                          const __DRIconfig *dri_config,
                          struct loader_dri3_extensions *ext,
                          const struct loader_dri3_vtable *vtable,
                          struct loader_dri3_drawable *draw)
{
   xcb_get_geometry_cookie_t cookie;
   xcb_get_geometry_reply_t *reply;
   xcb_generic_error_t *error;
   GLint vblank_mode = DRI_CONF_VBLANK_DEF_INTERVAL_1;
   int swap_interval;

   draw->conn = conn;
   draw->ext = ext;
   draw->vtable = vtable;
   draw->drawable = drawable;
   draw->dri_screen = dri_screen;
   draw->is_different_gpu = is_different_gpu;

   draw->have_back = 0;
   draw->have_fake_front = 0;
   draw->first_init = true;

   draw->cur_blit_source = -1;
   draw->back_format = __DRI_IMAGE_FORMAT_NONE;
   mtx_init(&draw->mtx, mtx_plain);
   cnd_init(&draw->event_cnd);

   if (draw->ext->config)
      draw->ext->config->configQueryi(draw->dri_screen,
                                      "vblank_mode", &vblank_mode);

   switch (vblank_mode) {
   case DRI_CONF_VBLANK_NEVER:
   case DRI_CONF_VBLANK_DEF_INTERVAL_0:
      swap_interval = 0;
      break;
   case DRI_CONF_VBLANK_DEF_INTERVAL_1:
   case DRI_CONF_VBLANK_ALWAYS_SYNC:
   default:
      swap_interval = 1;
      break;
   }
   draw->swap_interval = swap_interval;

   dri3_update_num_back(draw);

   /* Create a new drawable */
   draw->dri_drawable =
      draw->ext->image_driver->createNewDrawable(dri_screen,
                                                 dri_config,
                                                 draw);

   if (!draw->dri_drawable)
      return 1;

   cookie = xcb_get_geometry(draw->conn, draw->drawable);
   reply = xcb_get_geometry_reply(draw->conn, cookie, &error);
   if (reply == NULL || error != NULL) {
      draw->ext->core->destroyDrawable(draw->dri_drawable);
      return 1;
   }

   draw->width = reply->width;
   draw->height = reply->height;
   draw->depth = reply->depth;
   draw->vtable->set_drawable_size(draw, draw->width, draw->height);
   free(reply);

   draw->swap_method = __DRI_ATTRIB_SWAP_UNDEFINED;
   if (draw->ext->core->base.version >= 2) {
      (void )draw->ext->core->getConfigAttrib(dri_config,
                                              __DRI_ATTRIB_SWAP_METHOD,
                                              &draw->swap_method);
   }

   /*
    * Make sure server has the same swap interval we do for the new
    * drawable.
    */
   loader_dri3_set_swap_interval(draw, swap_interval);

   return 0;
}

/*
 * Process one Present event
 */
static void
dri3_handle_present_event(struct loader_dri3_drawable *draw,
                          xcb_present_generic_event_t *ge)
{
   switch (ge->evtype) {
   case XCB_PRESENT_CONFIGURE_NOTIFY: {
      xcb_present_configure_notify_event_t *ce = (void *) ge;

      draw->width = ce->width;
      draw->height = ce->height;
      draw->vtable->set_drawable_size(draw, draw->width, draw->height);
      draw->ext->flush->invalidate(draw->dri_drawable);
      break;
   }
   case XCB_PRESENT_COMPLETE_NOTIFY: {
      xcb_present_complete_notify_event_t *ce = (void *) ge;

      /* Compute the processed SBC number from the received 32-bit serial number
       * merged with the upper 32-bits of the sent 64-bit serial number while
       * checking for wrap.
       */
      if (ce->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP) {
         draw->recv_sbc = (draw->send_sbc & 0xffffffff00000000LL) | ce->serial;
         if (draw->recv_sbc > draw->send_sbc)
            draw->recv_sbc -= 0x100000000;
         switch (ce->mode) {
         case XCB_PRESENT_COMPLETE_MODE_FLIP:
            draw->flipping = true;
            break;
         case XCB_PRESENT_COMPLETE_MODE_COPY:
            draw->flipping = false;
            break;
         }

         if (draw->vtable->show_fps)
            draw->vtable->show_fps(draw, ce->ust);

         draw->ust = ce->ust;
         draw->msc = ce->msc;
      } else {
         draw->recv_msc_serial = ce->serial;
         draw->notify_ust = ce->ust;
         draw->notify_msc = ce->msc;
      }
      break;
   }
   case XCB_PRESENT_EVENT_IDLE_NOTIFY: {
      xcb_present_idle_notify_event_t *ie = (void *) ge;
      int b;

      for (b = 0; b < sizeof(draw->buffers) / sizeof(draw->buffers[0]); b++) {
         struct loader_dri3_buffer *buf = draw->buffers[b];

         if (buf && buf->pixmap == ie->pixmap)
            buf->busy = 0;

         if (buf && draw->num_back <= b && b < LOADER_DRI3_MAX_BACK &&
             draw->cur_blit_source != b &&
             !buf->busy) {
            dri3_free_render_buffer(draw, buf);
            draw->buffers[b] = NULL;
         }
      }
      break;
   }
   }
   free(ge);
}

static bool
dri3_wait_for_event_locked(struct loader_dri3_drawable *draw)
{
   xcb_generic_event_t *ev;
   xcb_present_generic_event_t *ge;

   xcb_flush(draw->conn);

   /* Only have one thread waiting for events at a time */
   if (draw->has_event_waiter) {
      cnd_wait(&draw->event_cnd, &draw->mtx);
      /* Another thread has updated the protected info, so retest. */
      return true;
   } else {
      draw->has_event_waiter = true;
      /* Allow other threads access to the drawable while we're waiting. */
      mtx_unlock(&draw->mtx);
      ev = xcb_wait_for_special_event(draw->conn, draw->special_event);
      mtx_lock(&draw->mtx);
      draw->has_event_waiter = false;
      cnd_broadcast(&draw->event_cnd);
   }
   if (!ev)
      return false;
   ge = (void *) ev;
   dri3_handle_present_event(draw, ge);
   return true;
}

/** loader_dri3_wait_for_msc
 *
 * Get the X server to send an event when the target msc/divisor/remainder is
 * reached.
 */
bool
loader_dri3_wait_for_msc(struct loader_dri3_drawable *draw,
                         int64_t target_msc,
                         int64_t divisor, int64_t remainder,
                         int64_t *ust, int64_t *msc, int64_t *sbc)
{
   uint32_t msc_serial;

   msc_serial = ++draw->send_msc_serial;
   xcb_present_notify_msc(draw->conn,
                          draw->drawable,
                          msc_serial,
                          target_msc,
                          divisor,
                          remainder);

   mtx_lock(&draw->mtx);
   xcb_flush(draw->conn);

   /* Wait for the event */
   if (draw->special_event) {
      while ((int32_t) (msc_serial - draw->recv_msc_serial) > 0) {
         if (!dri3_wait_for_event_locked(draw)) {
            mtx_unlock(&draw->mtx);
            return false;
         }
      }
   }

   *ust = draw->notify_ust;
   *msc = draw->notify_msc;
   *sbc = draw->recv_sbc;
   mtx_unlock(&draw->mtx);

   return true;
}

/** loader_dri3_wait_for_sbc
 *
 * Wait for the completed swap buffer count to reach the specified
 * target. Presumably the application knows that this will be reached with
 * outstanding complete events, or we're going to be here awhile.
 */
int
loader_dri3_wait_for_sbc(struct loader_dri3_drawable *draw,
                         int64_t target_sbc, int64_t *ust,
                         int64_t *msc, int64_t *sbc)
{
   /* From the GLX_OML_sync_control spec:
    *
    *     "If <target_sbc> = 0, the function will block until all previous
    *      swaps requested with glXSwapBuffersMscOML for that window have
    *      completed."
    */
   mtx_lock(&draw->mtx);
   if (!target_sbc)
      target_sbc = draw->send_sbc;

   while (draw->recv_sbc < target_sbc) {
      if (!dri3_wait_for_event_locked(draw)) {
         mtx_unlock(&draw->mtx);
         return 0;
      }
   }

   *ust = draw->ust;
   *msc = draw->msc;
   *sbc = draw->recv_sbc;
   mtx_unlock(&draw->mtx);
   return 1;
}

/** loader_dri3_find_back
 *
 * Find an idle back buffer. If there isn't one, then
 * wait for a present idle notify event from the X server
 */
static int
dri3_find_back(struct loader_dri3_drawable *draw)
{
   int b;
   int num_to_consider;

   mtx_lock(&draw->mtx);
   /* Increase the likelyhood of reusing current buffer */
   dri3_flush_present_events(draw);

   /* Check whether we need to reuse the current back buffer as new back.
    * In that case, wait until it's not busy anymore.
    */
   dri3_update_num_back(draw);
   num_to_consider = draw->num_back;
   if (!loader_dri3_have_image_blit(draw) && draw->cur_blit_source != -1) {
      num_to_consider = 1;
      draw->cur_blit_source = -1;
   }

   for (;;) {
      for (b = 0; b < num_to_consider; b++) {
         int id = LOADER_DRI3_BACK_ID((b + draw->cur_back) % draw->num_back);
         struct loader_dri3_buffer *buffer = draw->buffers[id];

         if (!buffer || !buffer->busy) {
            draw->cur_back = id;
            mtx_unlock(&draw->mtx);
            return id;
         }
      }
      if (!dri3_wait_for_event_locked(draw)) {
         mtx_unlock(&draw->mtx);
         return -1;
      }
   }
}

static xcb_gcontext_t
dri3_drawable_gc(struct loader_dri3_drawable *draw)
{
   if (!draw->gc) {
      uint32_t v = 0;
      xcb_create_gc(draw->conn,
                    (draw->gc = xcb_generate_id(draw->conn)),
                    draw->drawable,
                    XCB_GC_GRAPHICS_EXPOSURES,
                    &v);
   }
   return draw->gc;
}


static struct loader_dri3_buffer *
dri3_back_buffer(struct loader_dri3_drawable *draw)
{
   return draw->buffers[LOADER_DRI3_BACK_ID(draw->cur_back)];
}

static struct loader_dri3_buffer *
dri3_fake_front_buffer(struct loader_dri3_drawable *draw)
{
   return draw->buffers[LOADER_DRI3_FRONT_ID];
}

static void
dri3_copy_area(xcb_connection_t *c,
               xcb_drawable_t    src_drawable,
               xcb_drawable_t    dst_drawable,
               xcb_gcontext_t    gc,
               int16_t           src_x,
               int16_t           src_y,
               int16_t           dst_x,
               int16_t           dst_y,
               uint16_t          width,
               uint16_t          height)
{
   xcb_void_cookie_t cookie;

   cookie = xcb_copy_area_checked(c,
                                  src_drawable,
                                  dst_drawable,
                                  gc,
                                  src_x,
                                  src_y,
                                  dst_x,
                                  dst_y,
                                  width,
                                  height);
   xcb_discard_reply(c, cookie.sequence);
}

/**
 * Asks the driver to flush any queued work necessary for serializing with the
 * X command stream, and optionally the slightly more strict requirement of
 * glFlush() equivalence (which would require flushing even if nothing had
 * been drawn to a window system framebuffer, for example).
 */
void
loader_dri3_flush(struct loader_dri3_drawable *draw,
                  unsigned flags,
                  enum __DRI2throttleReason throttle_reason)
{
   /* NEED TO CHECK WHETHER CONTEXT IS NULL */
   __DRIcontext *dri_context = draw->vtable->get_dri_context(draw);

   if (dri_context) {
      draw->ext->flush->flush_with_flags(dri_context, draw->dri_drawable,
                                         flags, throttle_reason);
   }
}

void
loader_dri3_copy_sub_buffer(struct loader_dri3_drawable *draw,
                            int x, int y,
                            int width, int height,
                            bool flush)
{
   struct loader_dri3_buffer *back;
   unsigned flags = __DRI2_FLUSH_DRAWABLE;

   /* Check we have the right attachments */
   if (!draw->have_back || draw->is_pixmap)
      return;

   if (flush)
      flags |= __DRI2_FLUSH_CONTEXT;
   loader_dri3_flush(draw, flags, __DRI2_THROTTLE_SWAPBUFFER);

   back = dri3_find_back_alloc(draw);
   if (!back)
      return;

   y = draw->height - y - height;

   if (draw->is_different_gpu) {
      /* Update the linear buffer part of the back buffer
       * for the dri3_copy_area operation
       */
      (void) loader_dri3_blit_image(draw,
                                    back->linear_buffer,
                                    back->image,
                                    0, 0, back->width, back->height,
                                    0, 0, __BLIT_FLAG_FLUSH);
   }

   loader_dri3_swapbuffer_barrier(draw);
   dri3_fence_reset(draw->conn, back);
   dri3_copy_area(draw->conn,
                  back->pixmap,
                  draw->drawable,
                  dri3_drawable_gc(draw),
                  x, y, x, y, width, height);
   dri3_fence_trigger(draw->conn, back);
   /* Refresh the fake front (if present) after we just damaged the real
    * front.
    */
   if (draw->have_fake_front &&
       !loader_dri3_blit_image(draw,
                               dri3_fake_front_buffer(draw)->image,
                               back->image,
                               x, y, width, height,
                               x, y, __BLIT_FLAG_FLUSH) &&
       !draw->is_different_gpu) {
      dri3_fence_reset(draw->conn, dri3_fake_front_buffer(draw));
      dri3_copy_area(draw->conn,
                     back->pixmap,
                     dri3_fake_front_buffer(draw)->pixmap,
                     dri3_drawable_gc(draw),
                     x, y, x, y, width, height);
      dri3_fence_trigger(draw->conn, dri3_fake_front_buffer(draw));
      dri3_fence_await(draw->conn, NULL, dri3_fake_front_buffer(draw));
   }
   dri3_fence_await(draw->conn, draw, back);
}

void
loader_dri3_copy_drawable(struct loader_dri3_drawable *draw,
                          xcb_drawable_t dest,
                          xcb_drawable_t src)
{
   loader_dri3_flush(draw, __DRI2_FLUSH_DRAWABLE, 0);

   dri3_fence_reset(draw->conn, dri3_fake_front_buffer(draw));
   dri3_copy_area(draw->conn,
                  src, dest,
                  dri3_drawable_gc(draw),
                  0, 0, 0, 0, draw->width, draw->height);
   dri3_fence_trigger(draw->conn, dri3_fake_front_buffer(draw));
   dri3_fence_await(draw->conn, draw, dri3_fake_front_buffer(draw));
}

void
loader_dri3_wait_x(struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *front;

   if (draw == NULL || !draw->have_fake_front)
      return;

   front = dri3_fake_front_buffer(draw);

   loader_dri3_copy_drawable(draw, front->pixmap, draw->drawable);

   /* In the psc->is_different_gpu case, the linear buffer has been updated,
    * but not yet the tiled buffer.
    * Copy back to the tiled buffer we use for rendering.
    * Note that we don't need flushing.
    */
   if (draw->is_different_gpu)
      (void) loader_dri3_blit_image(draw,
                                    front->image,
                                    front->linear_buffer,
                                    0, 0, front->width, front->height,
                                    0, 0, 0);
}

void
loader_dri3_wait_gl(struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *front;

   if (draw == NULL || !draw->have_fake_front)
      return;

   front = dri3_fake_front_buffer(draw);

   /* In the psc->is_different_gpu case, we update the linear_buffer
    * before updating the real front.
    */
   if (draw->is_different_gpu)
      (void) loader_dri3_blit_image(draw,
                                    front->linear_buffer,
                                    front->image,
                                    0, 0, front->width, front->height,
                                    0, 0, __BLIT_FLAG_FLUSH);
   loader_dri3_swapbuffer_barrier(draw);
   loader_dri3_copy_drawable(draw, draw->drawable, front->pixmap);
}

/** dri3_flush_present_events
 *
 * Process any present events that have been received from the X server
 */
static void
dri3_flush_present_events(struct loader_dri3_drawable *draw)
{
   /* Check to see if any configuration changes have occurred
    * since we were last invoked
    */
   if (draw->has_event_waiter)
      return;

   if (draw->special_event) {
      xcb_generic_event_t    *ev;

      while ((ev = xcb_poll_for_special_event(draw->conn,
                                              draw->special_event)) != NULL) {
         xcb_present_generic_event_t *ge = (void *) ev;
         dri3_handle_present_event(draw, ge);
      }
   }
}

/** loader_dri3_swap_buffers_msc
 *
 * Make the current back buffer visible using the present extension
 */
int64_t
loader_dri3_swap_buffers_msc(struct loader_dri3_drawable *draw,
                             int64_t target_msc, int64_t divisor,
                             int64_t remainder, unsigned flush_flags,
                             bool force_copy)
{
   struct loader_dri3_buffer *back;
   int64_t ret = 0;
   uint32_t options = XCB_PRESENT_OPTION_NONE;

   draw->vtable->flush_drawable(draw, flush_flags);

   back = dri3_find_back_alloc(draw);

   mtx_lock(&draw->mtx);
   if (draw->is_different_gpu && back) {
      /* Update the linear buffer before presenting the pixmap */
      (void) loader_dri3_blit_image(draw,
                                    back->linear_buffer,
                                    back->image,
                                    0, 0, back->width, back->height,
                                    0, 0, __BLIT_FLAG_FLUSH);
   }

   /* If we need to preload the new back buffer, remember the source.
    * The force_copy parameter is used by EGL to attempt to preserve
    * the back buffer across a call to this function.
    */
   if (draw->swap_method != __DRI_ATTRIB_SWAP_UNDEFINED || force_copy)
      draw->cur_blit_source = LOADER_DRI3_BACK_ID(draw->cur_back);

   /* Exchange the back and fake front. Even though the server knows about these
    * buffers, it has no notion of back and fake front.
    */
   if (back && draw->have_fake_front) {
      struct loader_dri3_buffer *tmp;

      tmp = dri3_fake_front_buffer(draw);
      draw->buffers[LOADER_DRI3_FRONT_ID] = back;
      draw->buffers[LOADER_DRI3_BACK_ID(draw->cur_back)] = tmp;

      if (draw->swap_method == __DRI_ATTRIB_SWAP_COPY  || force_copy)
         draw->cur_blit_source = LOADER_DRI3_FRONT_ID;
   }

   dri3_flush_present_events(draw);

   if (back && !draw->is_pixmap) {
      dri3_fence_reset(draw->conn, back);

      /* Compute when we want the frame shown by taking the last known
       * successful MSC and adding in a swap interval for each outstanding swap
       * request. target_msc=divisor=remainder=0 means "Use glXSwapBuffers()
       * semantic"
       */
      ++draw->send_sbc;
      if (target_msc == 0 && divisor == 0 && remainder == 0)
         target_msc = draw->msc + draw->swap_interval *
                      (draw->send_sbc - draw->recv_sbc);
      else if (divisor == 0 && remainder > 0) {
         /* From the GLX_OML_sync_control spec:
          *     "If <divisor> = 0, the swap will occur when MSC becomes
          *      greater than or equal to <target_msc>."
          *
          * Note that there's no mention of the remainder.  The Present
          * extension throws BadValue for remainder != 0 with divisor == 0, so
          * just drop the passed in value.
          */
         remainder = 0;
      }

      /* From the GLX_EXT_swap_control spec
       * and the EGL 1.4 spec (page 53):
       *
       *     "If <interval> is set to a value of 0, buffer swaps are not
       *      synchronized to a video frame."
       *
       * Implementation note: It is possible to enable triple buffering
       * behaviour by not using XCB_PRESENT_OPTION_ASYNC, but this should not be
       * the default.
       */
      if (draw->swap_interval == 0)
          options |= XCB_PRESENT_OPTION_ASYNC;

      /* If we need to populate the new back, but need to reuse the back
       * buffer slot due to lack of local blit capabilities, make sure
       * the server doesn't flip and we deadlock.
       */
      if (!loader_dri3_have_image_blit(draw) && draw->cur_blit_source != -1)
         options |= XCB_PRESENT_OPTION_COPY;

      back->busy = 1;
      back->last_swap = draw->send_sbc;
      xcb_present_pixmap(draw->conn,
                         draw->drawable,
                         back->pixmap,
                         (uint32_t) draw->send_sbc,
                         0,                                    /* valid */
                         0,                                    /* update */
                         0,                                    /* x_off */
                         0,                                    /* y_off */
                         None,                                 /* target_crtc */
                         None,
                         back->sync_fence,
                         options,
                         target_msc,
                         divisor,
                         remainder, 0, NULL);
      ret = (int64_t) draw->send_sbc;

      /* Schedule a server-side back-preserving blit if necessary.
       * This happens iff all conditions below are satisfied:
       * a) We have a fake front,
       * b) We need to preserve the back buffer,
       * c) We don't have local blit capabilities.
       */
      if (!loader_dri3_have_image_blit(draw) && draw->cur_blit_source != -1 &&
          draw->cur_blit_source != LOADER_DRI3_BACK_ID(draw->cur_back)) {
         struct loader_dri3_buffer *new_back = dri3_back_buffer(draw);
         struct loader_dri3_buffer *src = draw->buffers[draw->cur_blit_source];

         dri3_fence_reset(draw->conn, new_back);
         dri3_copy_area(draw->conn, src->pixmap,
                        new_back->pixmap,
                        dri3_drawable_gc(draw),
                        0, 0, 0, 0, draw->width, draw->height);
         dri3_fence_trigger(draw->conn, new_back);
         new_back->last_swap = src->last_swap;
      }

      xcb_flush(draw->conn);
      if (draw->stamp)
         ++(*draw->stamp);
   }
   mtx_unlock(&draw->mtx);

   draw->ext->flush->invalidate(draw->dri_drawable);

   return ret;
}

int
loader_dri3_query_buffer_age(struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *back = dri3_find_back_alloc(draw);
   int ret;

   mtx_lock(&draw->mtx);
   ret = (!back || back->last_swap == 0) ? 0 :
      draw->send_sbc - back->last_swap + 1;
   mtx_unlock(&draw->mtx);

   return ret;
}

/** loader_dri3_open
 *
 * Wrapper around xcb_dri3_open
 */
int
loader_dri3_open(xcb_connection_t *conn,
                 xcb_window_t root,
                 uint32_t provider)
{
   xcb_dri3_open_cookie_t       cookie;
   xcb_dri3_open_reply_t        *reply;
   int                          fd;

   cookie = xcb_dri3_open(conn,
                          root,
                          provider);

   reply = xcb_dri3_open_reply(conn, cookie, NULL);
   if (!reply)
      return -1;

   if (reply->nfd != 1) {
      free(reply);
      return -1;
   }

   fd = xcb_dri3_open_reply_fds(conn, reply)[0];
   free(reply);
   fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);

   return fd;
}

static uint32_t
dri3_cpp_for_format(uint32_t format) {
   switch (format) {
   case  __DRI_IMAGE_FORMAT_R8:
      return 1;
   case  __DRI_IMAGE_FORMAT_RGB565:
   case  __DRI_IMAGE_FORMAT_GR88:
      return 2;
   case  __DRI_IMAGE_FORMAT_XRGB8888:
   case  __DRI_IMAGE_FORMAT_ARGB8888:
   case  __DRI_IMAGE_FORMAT_ABGR8888:
   case  __DRI_IMAGE_FORMAT_XBGR8888:
   case  __DRI_IMAGE_FORMAT_XRGB2101010:
   case  __DRI_IMAGE_FORMAT_ARGB2101010:
   case  __DRI_IMAGE_FORMAT_SARGB8:
      return 4;
   case  __DRI_IMAGE_FORMAT_NONE:
   default:
      return 0;
   }
}

/* the DRIimage createImage function takes __DRI_IMAGE_FORMAT codes, while
 * the createImageFromFds call takes __DRI_IMAGE_FOURCC codes. To avoid
 * complete confusion, just deal in __DRI_IMAGE_FORMAT codes for now and
 * translate to __DRI_IMAGE_FOURCC codes in the call to createImageFromFds
 */
static int
image_format_to_fourcc(int format)
{

   /* Convert from __DRI_IMAGE_FORMAT to __DRI_IMAGE_FOURCC (sigh) */
   switch (format) {
   case __DRI_IMAGE_FORMAT_SARGB8: return __DRI_IMAGE_FOURCC_SARGB8888;
   case __DRI_IMAGE_FORMAT_RGB565: return __DRI_IMAGE_FOURCC_RGB565;
   case __DRI_IMAGE_FORMAT_XRGB8888: return __DRI_IMAGE_FOURCC_XRGB8888;
   case __DRI_IMAGE_FORMAT_ARGB8888: return __DRI_IMAGE_FOURCC_ARGB8888;
   case __DRI_IMAGE_FORMAT_ABGR8888: return __DRI_IMAGE_FOURCC_ABGR8888;
   case __DRI_IMAGE_FORMAT_XBGR8888: return __DRI_IMAGE_FOURCC_XBGR8888;
   }
   return 0;
}

/** loader_dri3_alloc_render_buffer
 *
 * Use the driver createImage function to construct a __DRIimage, then
 * get a file descriptor for that and create an X pixmap from that
 *
 * Allocate an xshmfence for synchronization
 */
static struct loader_dri3_buffer *
dri3_alloc_render_buffer(struct loader_dri3_drawable *draw, unsigned int format,
                         int width, int height, int depth)
{
   struct loader_dri3_buffer *buffer;
   __DRIimage *pixmap_buffer;
   xcb_pixmap_t pixmap;
   xcb_sync_fence_t sync_fence;
   struct xshmfence *shm_fence;
   int buffer_fd, fence_fd;
   int stride;

   /* Create an xshmfence object and
    * prepare to send that to the X server
    */

   fence_fd = xshmfence_alloc_shm();
   if (fence_fd < 0)
      return NULL;

   shm_fence = xshmfence_map_shm(fence_fd);
   if (shm_fence == NULL)
      goto no_shm_fence;

   /* Allocate the image from the driver
    */
   buffer = calloc(1, sizeof *buffer);
   if (!buffer)
      goto no_buffer;

   buffer->cpp = dri3_cpp_for_format(format);
   if (!buffer->cpp)
      goto no_image;

   if (!draw->is_different_gpu) {
      buffer->image = draw->ext->image->createImage(draw->dri_screen,
                                                    width, height,
                                                    format,
                                                    __DRI_IMAGE_USE_SHARE |
                                                    __DRI_IMAGE_USE_SCANOUT |
                                                    __DRI_IMAGE_USE_BACKBUFFER,
                                                    buffer);
      pixmap_buffer = buffer->image;

      if (!buffer->image)
         goto no_image;
   } else {
      buffer->image = draw->ext->image->createImage(draw->dri_screen,
                                                    width, height,
                                                    format,
                                                    0,
                                                    buffer);

      if (!buffer->image)
         goto no_image;

      buffer->linear_buffer =
        draw->ext->image->createImage(draw->dri_screen,
                                      width, height, format,
                                      __DRI_IMAGE_USE_SHARE |
                                      __DRI_IMAGE_USE_LINEAR |
                                      __DRI_IMAGE_USE_BACKBUFFER,
                                      buffer);
      pixmap_buffer = buffer->linear_buffer;

      if (!buffer->linear_buffer)
         goto no_linear_buffer;
   }

   /* X wants the stride, so ask the image for it
    */
   if (!draw->ext->image->queryImage(pixmap_buffer, __DRI_IMAGE_ATTRIB_STRIDE,
                                     &stride))
      goto no_buffer_attrib;

   buffer->pitch = stride;

   if (!draw->ext->image->queryImage(pixmap_buffer, __DRI_IMAGE_ATTRIB_FD,
                                     &buffer_fd))
      goto no_buffer_attrib;

   xcb_dri3_pixmap_from_buffer(draw->conn,
                               (pixmap = xcb_generate_id(draw->conn)),
                               draw->drawable,
                               buffer->size,
                               width, height, buffer->pitch,
                               depth, buffer->cpp * 8,
                               buffer_fd);

   xcb_dri3_fence_from_fd(draw->conn,
                          pixmap,
                          (sync_fence = xcb_generate_id(draw->conn)),
                          false,
                          fence_fd);

   buffer->pixmap = pixmap;
   buffer->own_pixmap = true;
   buffer->sync_fence = sync_fence;
   buffer->shm_fence = shm_fence;
   buffer->width = width;
   buffer->height = height;

   /* Mark the buffer as idle
    */
   dri3_fence_set(buffer);

   return buffer;

no_buffer_attrib:
   draw->ext->image->destroyImage(pixmap_buffer);
no_linear_buffer:
   if (draw->is_different_gpu)
      draw->ext->image->destroyImage(buffer->image);
no_image:
   free(buffer);
no_buffer:
   xshmfence_unmap_shm(shm_fence);
no_shm_fence:
   close(fence_fd);
   return NULL;
}

/** loader_dri3_update_drawable
 *
 * Called the first time we use the drawable and then
 * after we receive present configure notify events to
 * track the geometry of the drawable
 */
static int
dri3_update_drawable(__DRIdrawable *driDrawable,
                     struct loader_dri3_drawable *draw)
{
   mtx_lock(&draw->mtx);
   if (draw->first_init) {
      xcb_get_geometry_cookie_t                 geom_cookie;
      xcb_get_geometry_reply_t                  *geom_reply;
      xcb_void_cookie_t                         cookie;
      xcb_generic_error_t                       *error;
      xcb_present_query_capabilities_cookie_t   present_capabilities_cookie;
      xcb_present_query_capabilities_reply_t    *present_capabilities_reply;

      draw->first_init = false;

      /* Try to select for input on the window.
       *
       * If the drawable is a window, this will get our events
       * delivered.
       *
       * Otherwise, we'll get a BadWindow error back from this request which
       * will let us know that the drawable is a pixmap instead.
       */

      draw->eid = xcb_generate_id(draw->conn);
      cookie =
         xcb_present_select_input_checked(draw->conn, draw->eid, draw->drawable,
                                          XCB_PRESENT_EVENT_MASK_CONFIGURE_NOTIFY |
                                          XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY |
                                          XCB_PRESENT_EVENT_MASK_IDLE_NOTIFY);

      present_capabilities_cookie =
         xcb_present_query_capabilities(draw->conn, draw->drawable);

      /* Create an XCB event queue to hold present events outside of the usual
       * application event queue
       */
      draw->special_event = xcb_register_for_special_xge(draw->conn,
                                                         &xcb_present_id,
                                                         draw->eid,
                                                         draw->stamp);
      geom_cookie = xcb_get_geometry(draw->conn, draw->drawable);

      geom_reply = xcb_get_geometry_reply(draw->conn, geom_cookie, NULL);

      if (!geom_reply) {
         mtx_unlock(&draw->mtx);
         return false;
      }

      draw->width = geom_reply->width;
      draw->height = geom_reply->height;
      draw->depth = geom_reply->depth;
      draw->vtable->set_drawable_size(draw, draw->width, draw->height);

      free(geom_reply);

      draw->is_pixmap = false;

      /* Check to see if our select input call failed. If it failed with a
       * BadWindow error, then assume the drawable is a pixmap. Destroy the
       * special event queue created above and mark the drawable as a pixmap
       */

      error = xcb_request_check(draw->conn, cookie);

      present_capabilities_reply =
          xcb_present_query_capabilities_reply(draw->conn,
                                               present_capabilities_cookie,
                                               NULL);

      if (present_capabilities_reply) {
         draw->present_capabilities = present_capabilities_reply->capabilities;
         free(present_capabilities_reply);
      } else
         draw->present_capabilities = 0;

      if (error) {
         if (error->error_code != BadWindow) {
            free(error);
            mtx_unlock(&draw->mtx);
            return false;
         }
         draw->is_pixmap = true;
         xcb_unregister_for_special_event(draw->conn, draw->special_event);
         draw->special_event = NULL;
      }
   }
   dri3_flush_present_events(draw);
   mtx_unlock(&draw->mtx);
   return true;
}

__DRIimage *
loader_dri3_create_image(xcb_connection_t *c,
                         xcb_dri3_buffer_from_pixmap_reply_t *bp_reply,
                         unsigned int format,
                         __DRIscreen *dri_screen,
                         const __DRIimageExtension *image,
                         void *loaderPrivate)
{
   int                                  *fds;
   __DRIimage                           *image_planar, *ret;
   int                                  stride, offset;

   /* Get an FD for the pixmap object
    */
   fds = xcb_dri3_buffer_from_pixmap_reply_fds(c, bp_reply);

   stride = bp_reply->stride;
   offset = 0;

   /* createImageFromFds creates a wrapper __DRIimage structure which
    * can deal with multiple planes for things like Yuv images. So, once
    * we've gotten the planar wrapper, pull the single plane out of it and
    * discard the wrapper.
    */
   image_planar = image->createImageFromFds(dri_screen,
                                            bp_reply->width,
                                            bp_reply->height,
                                            image_format_to_fourcc(format),
                                            fds, 1,
                                            &stride, &offset, loaderPrivate);
   close(fds[0]);
   if (!image_planar)
      return NULL;

   ret = image->fromPlanar(image_planar, 0, loaderPrivate);

   image->destroyImage(image_planar);

   return ret;
}

/** dri3_get_pixmap_buffer
 *
 * Get the DRM object for a pixmap from the X server and
 * wrap that with a __DRIimage structure using createImageFromFds
 */
static struct loader_dri3_buffer *
dri3_get_pixmap_buffer(__DRIdrawable *driDrawable, unsigned int format,
                       enum loader_dri3_buffer_type buffer_type,
                       struct loader_dri3_drawable *draw)
{
   int                                  buf_id = loader_dri3_pixmap_buf_id(buffer_type);
   struct loader_dri3_buffer            *buffer = draw->buffers[buf_id];
   xcb_drawable_t                       pixmap;
   xcb_dri3_buffer_from_pixmap_cookie_t bp_cookie;
   xcb_dri3_buffer_from_pixmap_reply_t  *bp_reply;
   xcb_sync_fence_t                     sync_fence;
   struct xshmfence                     *shm_fence;
   int                                  fence_fd;
   __DRIscreen                          *cur_screen;

   if (buffer)
      return buffer;

   pixmap = draw->drawable;

   buffer = calloc(1, sizeof *buffer);
   if (!buffer)
      goto no_buffer;

   fence_fd = xshmfence_alloc_shm();
   if (fence_fd < 0)
      goto no_fence;
   shm_fence = xshmfence_map_shm(fence_fd);
   if (shm_fence == NULL) {
      close (fence_fd);
      goto no_fence;
   }

   xcb_dri3_fence_from_fd(draw->conn,
                          pixmap,
                          (sync_fence = xcb_generate_id(draw->conn)),
                          false,
                          fence_fd);

   bp_cookie = xcb_dri3_buffer_from_pixmap(draw->conn, pixmap);
   bp_reply = xcb_dri3_buffer_from_pixmap_reply(draw->conn, bp_cookie, NULL);
   if (!bp_reply)
      goto no_image;

   /* Get the currently-bound screen or revert to using the drawable's screen if
    * no contexts are currently bound. The latter case is at least necessary for
    * obs-studio, when using Window Capture (Xcomposite) as a Source.
    */
   cur_screen = draw->vtable->get_dri_screen();
   if (!cur_screen) {
       cur_screen = draw->dri_screen;
   }

   buffer->image = loader_dri3_create_image(draw->conn, bp_reply, format,
                                            cur_screen, draw->ext->image,
                                            buffer);
   if (!buffer->image)
      goto no_image;

   buffer->pixmap = pixmap;
   buffer->own_pixmap = false;
   buffer->width = bp_reply->width;
   buffer->height = bp_reply->height;
   buffer->shm_fence = shm_fence;
   buffer->sync_fence = sync_fence;

   draw->buffers[buf_id] = buffer;

   free(bp_reply);

   return buffer;

no_image:
   free(bp_reply);
   xcb_sync_destroy_fence(draw->conn, sync_fence);
   xshmfence_unmap_shm(shm_fence);
no_fence:
   free(buffer);
no_buffer:
   return NULL;
}

/** dri3_get_buffer
 *
 * Find a front or back buffer, allocating new ones as necessary
 */
static struct loader_dri3_buffer *
dri3_get_buffer(__DRIdrawable *driDrawable,
                unsigned int format,
                enum loader_dri3_buffer_type buffer_type,
                struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *buffer;
   int buf_id;

   if (buffer_type == loader_dri3_buffer_back) {
      draw->back_format = format;

      buf_id = dri3_find_back(draw);

      if (buf_id < 0)
         return NULL;
   } else {
      buf_id = LOADER_DRI3_FRONT_ID;
   }

   buffer = draw->buffers[buf_id];

   /* Allocate a new buffer if there isn't an old one, or if that
    * old one is the wrong size
    */
   if (!buffer || buffer->width != draw->width ||
       buffer->height != draw->height) {
      struct loader_dri3_buffer *new_buffer;

      /* Allocate the new buffers
       */
      new_buffer = dri3_alloc_render_buffer(draw,
                                                   format,
                                                   draw->width,
                                                   draw->height,
                                                   draw->depth);
      if (!new_buffer)
         return NULL;

      /* When resizing, copy the contents of the old buffer, waiting for that
       * copy to complete using our fences before proceeding
       */
      if ((buffer_type == loader_dri3_buffer_back ||
           (buffer_type == loader_dri3_buffer_front && draw->have_fake_front))
          && buffer) {

         /* Fill the new buffer with data from an old buffer */
         dri3_fence_await(draw->conn, draw, buffer);
         if (!loader_dri3_blit_image(draw,
                                     new_buffer->image,
                                     buffer->image,
                                     0, 0, draw->width, draw->height,
                                     0, 0, 0) &&
             !buffer->linear_buffer) {
            dri3_fence_reset(draw->conn, new_buffer);
            dri3_copy_area(draw->conn,
                           buffer->pixmap,
                           new_buffer->pixmap,
                           dri3_drawable_gc(draw),
                           0, 0, 0, 0,
                           draw->width, draw->height);
            dri3_fence_trigger(draw->conn, new_buffer);
         }
         dri3_free_render_buffer(draw, buffer);
      } else if (buffer_type == loader_dri3_buffer_front) {
         /* Fill the new fake front with data from a real front */
         loader_dri3_swapbuffer_barrier(draw);
         dri3_fence_reset(draw->conn, new_buffer);
         dri3_copy_area(draw->conn,
                        draw->drawable,
                        new_buffer->pixmap,
                        dri3_drawable_gc(draw),
                        0, 0, 0, 0,
                        draw->width, draw->height);
         dri3_fence_trigger(draw->conn, new_buffer);

         if (new_buffer->linear_buffer) {
            dri3_fence_await(draw->conn, draw, new_buffer);
            (void) loader_dri3_blit_image(draw,
                                          new_buffer->image,
                                          new_buffer->linear_buffer,
                                          0, 0, draw->width, draw->height,
                                          0, 0, 0);
         }
      }
      buffer = new_buffer;
      draw->buffers[buf_id] = buffer;
   }
   dri3_fence_await(draw->conn, draw, buffer);

   /*
    * Do we need to preserve the content of a previous buffer?
    *
    * Note that this blit is needed only to avoid a wait for a buffer that
    * is currently in the flip chain or being scanned out from. That's really
    * a tradeoff. If we're ok with the wait we can reduce the number of back
    * buffers to 1 for SWAP_EXCHANGE, and 1 for SWAP_COPY,
    * but in the latter case we must disallow page-flipping.
    */
   if (buffer_type == loader_dri3_buffer_back &&
       draw->cur_blit_source != -1 &&
       draw->buffers[draw->cur_blit_source] &&
       buffer != draw->buffers[draw->cur_blit_source]) {

      struct loader_dri3_buffer *source = draw->buffers[draw->cur_blit_source];

      /* Avoid flushing here. Will propably do good for tiling hardware. */
      (void) loader_dri3_blit_image(draw,
                                    buffer->image,
                                    source->image,
                                    0, 0, draw->width, draw->height,
                                    0, 0, 0);
      buffer->last_swap = source->last_swap;
      draw->cur_blit_source = -1;
   }
   /* Return the requested buffer */
   return buffer;
}

/** dri3_free_buffers
 *
 * Free the front bufffer or all of the back buffers. Used
 * when the application changes which buffers it needs
 */
static void
dri3_free_buffers(__DRIdrawable *driDrawable,
                  enum loader_dri3_buffer_type buffer_type,
                  struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *buffer;
   int first_id;
   int n_id;
   int buf_id;

   switch (buffer_type) {
   case loader_dri3_buffer_back:
      first_id = LOADER_DRI3_BACK_ID(0);
      n_id = LOADER_DRI3_MAX_BACK;
      draw->cur_blit_source = -1;
      break;
   case loader_dri3_buffer_front:
      first_id = LOADER_DRI3_FRONT_ID;
      /* Don't free a fake front holding new backbuffer content. */
      n_id = (draw->cur_blit_source == LOADER_DRI3_FRONT_ID) ? 0 : 1;
   }

   for (buf_id = first_id; buf_id < first_id + n_id; buf_id++) {
      buffer = draw->buffers[buf_id];
      if (buffer) {
         dri3_free_render_buffer(draw, buffer);
         draw->buffers[buf_id] = NULL;
      }
   }
}

/** loader_dri3_get_buffers
 *
 * The published buffer allocation API.
 * Returns all of the necessary buffers, allocating
 * as needed.
 */
int
loader_dri3_get_buffers(__DRIdrawable *driDrawable,
                        unsigned int format,
                        uint32_t *stamp,
                        void *loaderPrivate,
                        uint32_t buffer_mask,
                        struct __DRIimageList *buffers)
{
   struct loader_dri3_drawable *draw = loaderPrivate;
   struct loader_dri3_buffer   *front, *back;

   buffers->image_mask = 0;
   buffers->front = NULL;
   buffers->back = NULL;

   front = NULL;
   back = NULL;

   if (!dri3_update_drawable(driDrawable, draw))
      return false;

   /* pixmaps always have front buffers.
    * Exchange swaps also mandate fake front buffers.
    */
   if (draw->is_pixmap || draw->swap_method == __DRI_ATTRIB_SWAP_EXCHANGE)
      buffer_mask |= __DRI_IMAGE_BUFFER_FRONT;

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {
      /* All pixmaps are owned by the server gpu.
       * When we use a different gpu, we can't use the pixmap
       * as buffer since it is potentially tiled a way
       * our device can't understand. In this case, use
       * a fake front buffer. Hopefully the pixmap
       * content will get synced with the fake front
       * buffer.
       */
      if (draw->is_pixmap && !draw->is_different_gpu)
         front = dri3_get_pixmap_buffer(driDrawable,
                                               format,
                                               loader_dri3_buffer_front,
                                               draw);
      else
         front = dri3_get_buffer(driDrawable,
                                        format,
                                        loader_dri3_buffer_front,
                                        draw);

      if (!front)
         return false;
   } else {
      dri3_free_buffers(driDrawable, loader_dri3_buffer_front, draw);
      draw->have_fake_front = 0;
   }

   if (buffer_mask & __DRI_IMAGE_BUFFER_BACK) {
      back = dri3_get_buffer(driDrawable,
                                    format,
                                    loader_dri3_buffer_back,
                                    draw);
      if (!back)
         return false;
      draw->have_back = 1;
   } else {
      dri3_free_buffers(driDrawable, loader_dri3_buffer_back, draw);
      draw->have_back = 0;
   }

   if (front) {
      buffers->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      buffers->front = front->image;
      draw->have_fake_front = draw->is_different_gpu || !draw->is_pixmap;
   }

   if (back) {
      buffers->image_mask |= __DRI_IMAGE_BUFFER_BACK;
      buffers->back = back->image;
   }

   draw->stamp = stamp;

   return true;
}

/** loader_dri3_update_drawable_geometry
 *
 * Get the current drawable geometry.
 */
void
loader_dri3_update_drawable_geometry(struct loader_dri3_drawable *draw)
{
   xcb_get_geometry_cookie_t geom_cookie;
   xcb_get_geometry_reply_t *geom_reply;

   geom_cookie = xcb_get_geometry(draw->conn, draw->drawable);

   geom_reply = xcb_get_geometry_reply(draw->conn, geom_cookie, NULL);

   if (geom_reply) {
      draw->width = geom_reply->width;
      draw->height = geom_reply->height;
      draw->vtable->set_drawable_size(draw, draw->width, draw->height);
      draw->ext->flush->invalidate(draw->dri_drawable);

      free(geom_reply);
   }
}


/**
 * Make sure the server has flushed all pending swap buffers to hardware
 * for this drawable. Ideally we'd want to send an X protocol request to
 * have the server block our connection until the swaps are complete. That
 * would avoid the potential round-trip here.
 */
void
loader_dri3_swapbuffer_barrier(struct loader_dri3_drawable *draw)
{
   int64_t ust, msc, sbc;

   (void) loader_dri3_wait_for_sbc(draw, 0, &ust, &msc, &sbc);
}

/**
 * Perform any cleanup associated with a close screen operation.
 * \param dri_screen[in,out] Pointer to __DRIscreen about to be closed.
 *
 * This function destroys the screen's cached swap context if any.
 */
void
loader_dri3_close_screen(__DRIscreen *dri_screen)
{
   mtx_lock(&blit_context.mtx);
   if (blit_context.ctx && blit_context.cur_screen == dri_screen) {
      blit_context.core->destroyContext(blit_context.ctx);
      blit_context.ctx = NULL;
   }
   mtx_unlock(&blit_context.mtx);
}

/**
 * Find a backbuffer slot - potentially allocating a back buffer
 *
 * \param draw[in,out]  Pointer to the drawable for which to find back.
 * \return Pointer to a new back buffer or NULL if allocation failed or was
 * not mandated.
 *
 * Find a potentially new back buffer, and if it's not been allocated yet and
 * in addition needs initializing, then try to allocate and initialize it.
 */
#include <stdio.h>
static struct loader_dri3_buffer *
dri3_find_back_alloc(struct loader_dri3_drawable *draw)
{
   struct loader_dri3_buffer *back;
   int id;

   id = dri3_find_back(draw);
   if (id < 0)
      return NULL;

   back = draw->buffers[id];
   /* Allocate a new back if we haven't got one */
   if (!back && draw->back_format != __DRI_IMAGE_FORMAT_NONE &&
       dri3_update_drawable(draw->dri_drawable, draw))
      back = dri3_alloc_render_buffer(draw, draw->back_format,
                                      draw->width, draw->height, draw->depth);

   if (!back)
      return NULL;

   draw->buffers[id] = back;

   /* If necessary, prefill the back with data according to swap_method mode. */
   if (draw->cur_blit_source != -1 &&
       draw->buffers[draw->cur_blit_source] &&
       back != draw->buffers[draw->cur_blit_source]) {
      struct loader_dri3_buffer *source = draw->buffers[draw->cur_blit_source];

      dri3_fence_await(draw->conn, draw, source);
      dri3_fence_await(draw->conn, draw, back);
      (void) loader_dri3_blit_image(draw,
                                    back->image,
                                    source->image,
                                    0, 0, draw->width, draw->height,
                                    0, 0, 0);
      back->last_swap = source->last_swap;
      draw->cur_blit_source = -1;
   }

   return back;
}
