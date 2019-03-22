/*
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>

#include <xf86drm.h>
#include "util/macros.h"

#include "egl_dri2.h"
#include "egl_dri2_fallbacks.h"
#include "platform_x11_dri3.h"

#include "loader.h"
#include "loader_dri3_helper.h"

static struct dri3_egl_surface *
loader_drawable_to_egl_surface(struct loader_dri3_drawable *draw) {
   size_t offset = offsetof(struct dri3_egl_surface, loader_drawable);
   return (struct dri3_egl_surface *)(((void*) draw) - offset);
}

static void
egl_dri3_set_drawable_size(struct loader_dri3_drawable *draw,
                           int width, int height)
{
   struct dri3_egl_surface *dri3_surf = loader_drawable_to_egl_surface(draw);

   dri3_surf->surf.base.Width = width;
   dri3_surf->surf.base.Height = height;
}

static bool
egl_dri3_in_current_context(struct loader_dri3_drawable *draw)
{
   struct dri3_egl_surface *dri3_surf = loader_drawable_to_egl_surface(draw);
   _EGLContext *ctx = _eglGetCurrentContext();

   return ctx->Resource.Display == dri3_surf->surf.base.Resource.Display;
}

static __DRIcontext *
egl_dri3_get_dri_context(struct loader_dri3_drawable *draw)
{
   _EGLContext *ctx = _eglGetCurrentContext();
   struct dri2_egl_context *dri2_ctx;
   if (!ctx)
      return NULL;
   dri2_ctx = dri2_egl_context(ctx);
   return dri2_ctx->dri_context;
}

static __DRIscreen *
egl_dri3_get_dri_screen(void)
{
   _EGLContext *ctx = _eglGetCurrentContext();
   struct dri2_egl_context *dri2_ctx;
   if (!ctx)
      return NULL;
   dri2_ctx = dri2_egl_context(ctx);
   return dri2_egl_display(dri2_ctx->base.Resource.Display)->dri_screen;
}

static void
egl_dri3_flush_drawable(struct loader_dri3_drawable *draw, unsigned flags)
{
   struct dri3_egl_surface *dri3_surf = loader_drawable_to_egl_surface(draw);
   _EGLDisplay *disp = dri3_surf->surf.base.Resource.Display;

   dri2_flush_drawable_for_swapbuffers(disp, &dri3_surf->surf.base);
}

static const struct loader_dri3_vtable egl_dri3_vtable = {
   .set_drawable_size = egl_dri3_set_drawable_size,
   .in_current_context = egl_dri3_in_current_context,
   .get_dri_context = egl_dri3_get_dri_context,
   .get_dri_screen = egl_dri3_get_dri_screen,
   .flush_drawable = egl_dri3_flush_drawable,
   .show_fps = NULL,
};

static EGLBoolean
dri3_destroy_surface(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);

   (void) drv;

   loader_dri3_drawable_fini(&dri3_surf->loader_drawable);

   dri2_fini_surface(surf);
   free(surf);

   return EGL_TRUE;
}

static EGLBoolean
dri3_set_swap_interval(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf,
                       EGLint interval)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);

   dri3_surf->surf.base.SwapInterval = interval;
   loader_dri3_set_swap_interval(&dri3_surf->loader_drawable, interval);

   return EGL_TRUE;
}

static _EGLSurface *
dri3_create_surface(_EGLDriver *drv, _EGLDisplay *disp, EGLint type,
                    _EGLConfig *conf, void *native_surface,
                    const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri3_egl_surface *dri3_surf;
   const __DRIconfig *dri_config;
   xcb_drawable_t drawable;

   (void) drv;

   dri3_surf = calloc(1, sizeof *dri3_surf);
   if (!dri3_surf) {
      _eglError(EGL_BAD_ALLOC, "dri3_create_surface");
      return NULL;
   }

   if (!dri2_init_surface(&dri3_surf->surf.base, disp, type, conf, attrib_list, false))
      goto cleanup_surf;

   if (type == EGL_PBUFFER_BIT) {
      drawable = xcb_generate_id(dri2_dpy->conn);
      xcb_create_pixmap(dri2_dpy->conn, conf->BufferSize,
                        drawable, dri2_dpy->screen->root,
                        dri3_surf->surf.base.Width, dri3_surf->surf.base.Height);
   } else {
      STATIC_ASSERT(sizeof(uintptr_t) == sizeof(native_surface));
      drawable = (uintptr_t) native_surface;
   }

   dri_config = dri2_get_dri_config(dri2_conf, type,
                                    dri3_surf->surf.base.GLColorspace);

   if (loader_dri3_drawable_init(dri2_dpy->conn, drawable,
                                 dri2_dpy->dri_screen,
                                 dri2_dpy->is_different_gpu, dri_config,
                                 &dri2_dpy->loader_dri3_ext,
                                 &egl_dri3_vtable,
                                 &dri3_surf->loader_drawable)) {
      _eglError(EGL_BAD_ALLOC, "dri3_surface_create");
      goto cleanup_pixmap;
   }

   return &dri3_surf->surf.base;

 cleanup_pixmap:
   if (type == EGL_PBUFFER_BIT)
      xcb_free_pixmap(dri2_dpy->conn, drawable);
 cleanup_surf:
   free(dri3_surf);

   return NULL;
}

static int
dri3_authenticate(_EGLDisplay *disp, uint32_t id)
{
#ifdef HAVE_WAYLAND_PLATFORM
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);

   if (dri2_dpy->device_name) {
      _eglLog(_EGL_WARNING,
              "Wayland client render node authentication is unnecessary");
      return 0;
   }

   _eglLog(_EGL_WARNING,
           "Wayland client primary node authentication isn't supported");
#endif

   return -1;
}

/**
 * Called via eglCreateWindowSurface(), drv->API.CreateWindowSurface().
 */
static _EGLSurface *
dri3_create_window_surface(_EGLDriver *drv, _EGLDisplay *disp,
                           _EGLConfig *conf, void *native_window,
                           const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   _EGLSurface *surf;

   surf = dri3_create_surface(drv, disp, EGL_WINDOW_BIT, conf,
                              native_window, attrib_list);
   if (surf != NULL)
      dri3_set_swap_interval(drv, disp, surf, dri2_dpy->default_swap_interval);

   return surf;
}

static _EGLSurface *
dri3_create_pixmap_surface(_EGLDriver *drv, _EGLDisplay *disp,
                           _EGLConfig *conf, void *native_pixmap,
                           const EGLint *attrib_list)
{
   return dri3_create_surface(drv, disp, EGL_PIXMAP_BIT, conf,
                              native_pixmap, attrib_list);
}

static _EGLSurface *
dri3_create_pbuffer_surface(_EGLDriver *drv, _EGLDisplay *disp,
                                _EGLConfig *conf, const EGLint *attrib_list)
{
   return dri3_create_surface(drv, disp, EGL_PBUFFER_BIT, conf,
                              NULL, attrib_list);
}

static EGLBoolean
dri3_get_sync_values(_EGLDisplay *display, _EGLSurface *surface,
                     EGLuint64KHR *ust, EGLuint64KHR *msc,
                     EGLuint64KHR *sbc)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surface);

   return loader_dri3_wait_for_msc(&dri3_surf->loader_drawable, 0, 0, 0,
                                   (int64_t *) ust, (int64_t *) msc,
                                   (int64_t *) sbc) ? EGL_TRUE : EGL_FALSE;
}

static _EGLImage *
dri3_create_image_khr_pixmap(_EGLDisplay *disp, _EGLContext *ctx,
                             EGLClientBuffer buffer, const EGLint *attr_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_image *dri2_img;
   xcb_drawable_t drawable;
   xcb_dri3_buffer_from_pixmap_cookie_t bp_cookie;
   xcb_dri3_buffer_from_pixmap_reply_t  *bp_reply;
   unsigned int format;

   drawable = (xcb_drawable_t) (uintptr_t) buffer;
   bp_cookie = xcb_dri3_buffer_from_pixmap(dri2_dpy->conn, drawable);
   bp_reply = xcb_dri3_buffer_from_pixmap_reply(dri2_dpy->conn,
                                                bp_cookie, NULL);
   if (!bp_reply) {
      _eglError(EGL_BAD_ALLOC, "xcb_dri3_buffer_from_pixmap");
      return NULL;
   }

   switch (bp_reply->depth) {
   case 16:
      format = __DRI_IMAGE_FORMAT_RGB565;
      break;
   case 24:
      format = __DRI_IMAGE_FORMAT_XRGB8888;
      break;
   case 32:
      format = __DRI_IMAGE_FORMAT_ARGB8888;
      break;
   default:
      _eglError(EGL_BAD_PARAMETER,
                "dri3_create_image_khr: unsupported pixmap depth");
      free(bp_reply);
      return EGL_NO_IMAGE_KHR;
   }

   dri2_img = malloc(sizeof *dri2_img);
   if (!dri2_img) {
      _eglError(EGL_BAD_ALLOC, "dri3_create_image_khr");
      return EGL_NO_IMAGE_KHR;
   }

   _eglInitImage(&dri2_img->base, disp);

   dri2_img->dri_image = loader_dri3_create_image(dri2_dpy->conn,
                                                  bp_reply,
                                                  format,
                                                  dri2_dpy->dri_screen,
                                                  dri2_dpy->image,
                                                  dri2_img);

   free(bp_reply);

   return &dri2_img->base;
}

static _EGLImage *
dri3_create_image_khr(_EGLDriver *drv, _EGLDisplay *disp,
                      _EGLContext *ctx, EGLenum target,
                      EGLClientBuffer buffer, const EGLint *attr_list)
{
   switch (target) {
   case EGL_NATIVE_PIXMAP_KHR:
      return dri3_create_image_khr_pixmap(disp, ctx, buffer, attr_list);
   default:
      return dri2_create_image_khr(drv, disp, ctx, target, buffer, attr_list);
   }
}

/**
 * Called by the driver when it needs to update the real front buffer with the
 * contents of its fake front buffer.
 */
static void
dri3_flush_front_buffer(__DRIdrawable *driDrawable, void *loaderPrivate)
{
   /* There does not seem to be any kind of consensus on whether we should
    * support front-buffer rendering or not:
    * http://lists.freedesktop.org/archives/mesa-dev/2013-June/040129.html
    */
   _eglLog(_EGL_WARNING, "FIXME: egl/x11 doesn't support front buffer rendering.");
   (void) driDrawable;
   (void) loaderPrivate;
}

const __DRIimageLoaderExtension dri3_image_loader_extension = {
   .base = { __DRI_IMAGE_LOADER, 1 },

   .getBuffers          = loader_dri3_get_buffers,
   .flushFrontBuffer    = dri3_flush_front_buffer,
};

static EGLBoolean
dri3_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *draw)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(draw);

   /* No-op for a pixmap or pbuffer surface */
   if (draw->Type == EGL_PIXMAP_BIT || draw->Type == EGL_PBUFFER_BIT)
      return EGL_FALSE;

   return loader_dri3_swap_buffers_msc(&dri3_surf->loader_drawable,
                                       0, 0, 0, 0,
                                       draw->SwapBehavior == EGL_BUFFER_PRESERVED) != -1;
}

static EGLBoolean
dri3_copy_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf,
                  void *native_pixmap_target)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);
   xcb_pixmap_t target;

   STATIC_ASSERT(sizeof(uintptr_t) == sizeof(native_pixmap_target));
   target = (uintptr_t) native_pixmap_target;

   loader_dri3_copy_drawable(&dri3_surf->loader_drawable, target,
                             dri3_surf->loader_drawable.drawable);

   return EGL_TRUE;
}

static int
dri3_query_buffer_age(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surf)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);

   return loader_dri3_query_buffer_age(&dri3_surf->loader_drawable);
}

static EGLBoolean
dri3_query_surface(_EGLDriver *drv, _EGLDisplay *dpy,
                   _EGLSurface *surf, EGLint attribute,
                   EGLint *value)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);

   switch (attribute) {
   case EGL_WIDTH:
   case EGL_HEIGHT:
      loader_dri3_update_drawable_geometry(&dri3_surf->loader_drawable);
      break;
   default:
      break;
   }

   return _eglQuerySurface(drv, dpy, surf, attribute, value);
}

static __DRIdrawable *
dri3_get_dri_drawable(_EGLSurface *surf)
{
   struct dri3_egl_surface *dri3_surf = dri3_egl_surface(surf);

   return dri3_surf->loader_drawable.dri_drawable;
}

static void
dri3_close_screen_notify(_EGLDisplay *dpy)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dpy);

   loader_dri3_close_screen(dri2_dpy->dri_screen);
}

struct dri2_egl_display_vtbl dri3_x11_display_vtbl = {
   .authenticate = dri3_authenticate,
   .create_window_surface = dri3_create_window_surface,
   .create_pixmap_surface = dri3_create_pixmap_surface,
   .create_pbuffer_surface = dri3_create_pbuffer_surface,
   .destroy_surface = dri3_destroy_surface,
   .create_image = dri3_create_image_khr,
   .swap_interval = dri3_set_swap_interval,
   .swap_buffers = dri3_swap_buffers,
   .swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .set_damage_region = dri2_fallback_set_damage_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri3_copy_buffers,
   .query_buffer_age = dri3_query_buffer_age,
   .query_surface = dri3_query_surface,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri3_get_sync_values,
   .get_dri_drawable = dri3_get_dri_drawable,
   .close_screen_notify = dri3_close_screen_notify,
};

EGLBoolean
dri3_x11_connect(struct dri2_egl_display *dri2_dpy)
{
   xcb_dri3_query_version_reply_t *dri3_query;
   xcb_dri3_query_version_cookie_t dri3_query_cookie;
   xcb_present_query_version_reply_t *present_query;
   xcb_present_query_version_cookie_t present_query_cookie;
   xcb_generic_error_t *error;
   const xcb_query_extension_reply_t *extension;

   xcb_prefetch_extension_data (dri2_dpy->conn, &xcb_dri3_id);
   xcb_prefetch_extension_data (dri2_dpy->conn, &xcb_present_id);

   extension = xcb_get_extension_data(dri2_dpy->conn, &xcb_dri3_id);
   if (!(extension && extension->present))
      return EGL_FALSE;

   extension = xcb_get_extension_data(dri2_dpy->conn, &xcb_present_id);
   if (!(extension && extension->present))
      return EGL_FALSE;

   dri3_query_cookie = xcb_dri3_query_version(dri2_dpy->conn,
                                              XCB_DRI3_MAJOR_VERSION,
                                              XCB_DRI3_MINOR_VERSION);

   present_query_cookie = xcb_present_query_version(dri2_dpy->conn,
                                                    XCB_PRESENT_MAJOR_VERSION,
                                                    XCB_PRESENT_MINOR_VERSION);

   dri3_query =
      xcb_dri3_query_version_reply(dri2_dpy->conn, dri3_query_cookie, &error);
   if (dri3_query == NULL || error != NULL) {
      _eglLog(_EGL_WARNING, "DRI3: failed to query the version");
      free(dri3_query);
      free(error);
      return EGL_FALSE;
   }
   free(dri3_query);

   present_query =
      xcb_present_query_version_reply(dri2_dpy->conn,
                                      present_query_cookie, &error);
   if (present_query == NULL || error != NULL) {
      _eglLog(_EGL_WARNING, "DRI3: failed to query Present version");
      free(present_query);
      free(error);
      return EGL_FALSE;
   }
   free(present_query);

   dri2_dpy->fd = loader_dri3_open(dri2_dpy->conn, dri2_dpy->screen->root, 0);
   if (dri2_dpy->fd < 0) {
      int conn_error = xcb_connection_has_error(dri2_dpy->conn);
      _eglLog(_EGL_WARNING, "DRI3: Screen seems not DRI3 capable");

      if (conn_error)
         _eglLog(_EGL_WARNING, "DRI3: Failed to initialize");

      return EGL_FALSE;
   }

   dri2_dpy->fd = loader_get_user_preferred_fd(dri2_dpy->fd, &dri2_dpy->is_different_gpu);

   dri2_dpy->driver_name = loader_get_driver_for_fd(dri2_dpy->fd);
   if (!dri2_dpy->driver_name) {
      _eglLog(_EGL_WARNING, "DRI3: No driver found");
      close(dri2_dpy->fd);
      return EGL_FALSE;
   }

#ifdef HAVE_WAYLAND_PLATFORM
   /* Only try to get a render device name since dri3 doesn't provide a
    * mechanism for authenticating client opened device node fds. If this
    * fails then don't advertise the extension. */
   dri2_dpy->device_name = drmGetRenderDeviceNameFromFd(dri2_dpy->fd);
#endif

   return EGL_TRUE;
}
