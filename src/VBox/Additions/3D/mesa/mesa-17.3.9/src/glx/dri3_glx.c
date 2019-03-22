/*
 * Copyright © 2013 Keith Packard
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

/*
 * Portions of this code were adapted from dri2_glx.c which carries the
 * following copyright:
 *
 * Copyright © 2008 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Kristian Høgsberg (krh@redhat.com)
 */

#if defined(GLX_DIRECT_RENDERING) && !defined(GLX_USE_APPLEGL)

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xlib-xcb.h>
#include <X11/xshmfence.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <GL/gl.h>
#include "glxclient.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "dri_common.h"
#include "dri3_priv.h"
#include "loader.h"
#include "dri2.h"

static struct dri3_drawable *
loader_drawable_to_dri3_drawable(struct loader_dri3_drawable *draw) {
   size_t offset = offsetof(struct dri3_drawable, loader_drawable);
   if (!draw)
      return NULL;
   return (struct dri3_drawable *)(((void*) draw) - offset);
}

static void
glx_dri3_set_drawable_size(struct loader_dri3_drawable *draw,
                           int width, int height)
{
   /* Nothing to do */
}

static bool
glx_dri3_in_current_context(struct loader_dri3_drawable *draw)
{
   struct dri3_drawable *priv = loader_drawable_to_dri3_drawable(draw);

   if (!priv)
      return false;

   struct dri3_context *pcp = (struct dri3_context *) __glXGetCurrentContext();
   struct dri3_screen *psc = (struct dri3_screen *) priv->base.psc;

   return (&pcp->base != &dummyContext) && pcp->base.psc == &psc->base;
}

static __DRIcontext *
glx_dri3_get_dri_context(struct loader_dri3_drawable *draw)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct dri3_context *dri3Ctx = (struct dri3_context *) gc;

   return (gc != &dummyContext) ? dri3Ctx->driContext : NULL;
}

static __DRIscreen *
glx_dri3_get_dri_screen(void)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct dri3_context *pcp = (struct dri3_context *) gc;
   struct dri3_screen *psc = (struct dri3_screen *) pcp->base.psc;

   return (gc != &dummyContext && psc) ? psc->driScreen : NULL;
}

static void
glx_dri3_flush_drawable(struct loader_dri3_drawable *draw, unsigned flags)
{
   loader_dri3_flush(draw, flags, __DRI2_THROTTLE_SWAPBUFFER);
}

static void
glx_dri3_show_fps(struct loader_dri3_drawable *draw, uint64_t current_ust)
{
   struct dri3_drawable *priv = loader_drawable_to_dri3_drawable(draw);
   const uint64_t interval =
      ((struct dri3_screen *) priv->base.psc)->show_fps_interval;

   if (!interval)
      return;

   priv->frames++;

   /* DRI3+Present together uses microseconds for UST. */
   if (priv->previous_ust + interval * 1000000 <= current_ust) {
      if (priv->previous_ust) {
         fprintf(stderr, "libGL: FPS = %.1f\n",
                 ((uint64_t) priv->frames * 1000000) /
                 (double)(current_ust - priv->previous_ust));
      }
      priv->frames = 0;
      priv->previous_ust = current_ust;
   }
}

static const struct loader_dri3_vtable glx_dri3_vtable = {
   .set_drawable_size = glx_dri3_set_drawable_size,
   .in_current_context = glx_dri3_in_current_context,
   .get_dri_context = glx_dri3_get_dri_context,
   .get_dri_screen = glx_dri3_get_dri_screen,
   .flush_drawable = glx_dri3_flush_drawable,
   .show_fps = glx_dri3_show_fps,
};


static const struct glx_context_vtable dri3_context_vtable;

static void
dri3_destroy_context(struct glx_context *context)
{
   struct dri3_context *pcp = (struct dri3_context *) context;
   struct dri3_screen *psc = (struct dri3_screen *) context->psc;

   driReleaseDrawables(&pcp->base);

   free((char *) context->extensions);

   (*psc->core->destroyContext) (pcp->driContext);

   free(pcp);
}

static Bool
dri3_bind_context(struct glx_context *context, struct glx_context *old,
                  GLXDrawable draw, GLXDrawable read)
{
   struct dri3_context *pcp = (struct dri3_context *) context;
   struct dri3_screen *psc = (struct dri3_screen *) pcp->base.psc;
   struct dri3_drawable *pdraw, *pread;
   __DRIdrawable *dri_draw = NULL, *dri_read = NULL;

   pdraw = (struct dri3_drawable *) driFetchDrawable(context, draw);
   pread = (struct dri3_drawable *) driFetchDrawable(context, read);

   driReleaseDrawables(&pcp->base);

   if (pdraw)
      dri_draw = pdraw->loader_drawable.dri_drawable;
   else if (draw != None)
      return GLXBadDrawable;

   if (pread)
      dri_read = pread->loader_drawable.dri_drawable;
   else if (read != None)
      return GLXBadDrawable;

   if (!(*psc->core->bindContext) (pcp->driContext, dri_draw, dri_read))
      return GLXBadContext;

   if (dri_draw)
      (*psc->f->invalidate)(dri_draw);
   if (dri_read && dri_read != dri_draw)
      (*psc->f->invalidate)(dri_read);

   return Success;
}

static void
dri3_unbind_context(struct glx_context *context, struct glx_context *new)
{
   struct dri3_context *pcp = (struct dri3_context *) context;
   struct dri3_screen *psc = (struct dri3_screen *) pcp->base.psc;

   (*psc->core->unbindContext) (pcp->driContext);
}

static struct glx_context *
dri3_create_context_attribs(struct glx_screen *base,
                            struct glx_config *config_base,
                            struct glx_context *shareList,
                            unsigned num_attribs,
                            const uint32_t *attribs,
                            unsigned *error)
{
   struct dri3_context *pcp = NULL;
   struct dri3_context *pcp_shared = NULL;
   struct dri3_screen *psc = (struct dri3_screen *) base;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;
   __DRIcontext *shared = NULL;

   uint32_t minor_ver = 1;
   uint32_t major_ver = 2;
   uint32_t flags = 0;
   unsigned api;
   int reset = __DRI_CTX_RESET_NO_NOTIFICATION;
   uint32_t ctx_attribs[2 * 5];
   unsigned num_ctx_attribs = 0;
   uint32_t render_type;

   /* Remap the GLX tokens to DRI2 tokens.
    */
   if (!dri2_convert_glx_attribs(num_attribs, attribs,
                                 &major_ver, &minor_ver,
                                 &render_type, &flags, &api,
                                 &reset, error))
      goto error_exit;

   /* Check the renderType value */
   if (!validate_renderType_against_config(config_base, render_type))
       goto error_exit;

   if (shareList) {
      pcp_shared = (struct dri3_context *) shareList;
      shared = pcp_shared->driContext;
   }

   pcp = calloc(1, sizeof *pcp);
   if (pcp == NULL) {
      *error = __DRI_CTX_ERROR_NO_MEMORY;
      goto error_exit;
   }

   if (!glx_context_init(&pcp->base, &psc->base, &config->base))
      goto error_exit;

   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MAJOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = major_ver;
   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MINOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = minor_ver;

   /* Only send a value when the non-default value is requested.  By doing
    * this we don't have to check the driver's DRI3 version before sending the
    * default value.
    */
   if (reset != __DRI_CTX_RESET_NO_NOTIFICATION) {
      ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_RESET_STRATEGY;
      ctx_attribs[num_ctx_attribs++] = reset;
   }

   if (flags != 0) {
      ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_FLAGS;

      /* The current __DRI_CTX_FLAG_* values are identical to the
       * GLX_CONTEXT_*_BIT values.
       */
      ctx_attribs[num_ctx_attribs++] = flags;
   }

   pcp->driContext =
      (*psc->image_driver->createContextAttribs) (psc->driScreen,
                                                  api,
                                                  config->driConfig,
                                                  shared,
                                                  num_ctx_attribs / 2,
                                                  ctx_attribs,
                                                  error,
                                                  pcp);

   if (pcp->driContext == NULL)
      goto error_exit;

   pcp->base.vtable = &dri3_context_vtable;

   return &pcp->base;

error_exit:
   free(pcp);

   return NULL;
}

static struct glx_context *
dri3_create_context(struct glx_screen *base,
                    struct glx_config *config_base,
                    struct glx_context *shareList, int renderType)
{
   unsigned int error;
   uint32_t attribs[2] = { GLX_RENDER_TYPE, renderType };

   return dri3_create_context_attribs(base, config_base, shareList,
                                      1, attribs, &error);
}

static void
dri3_destroy_drawable(__GLXDRIdrawable *base)
{
   struct dri3_drawable *pdraw = (struct dri3_drawable *) base;

   loader_dri3_drawable_fini(&pdraw->loader_drawable);

   free(pdraw);
}

static __GLXDRIdrawable *
dri3_create_drawable(struct glx_screen *base, XID xDrawable,
                     GLXDrawable drawable, struct glx_config *config_base)
{
   struct dri3_drawable *pdraw;
   struct dri3_screen *psc = (struct dri3_screen *) base;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;

   pdraw = calloc(1, sizeof(*pdraw));
   if (!pdraw)
      return NULL;

   pdraw->base.destroyDrawable = dri3_destroy_drawable;
   pdraw->base.xDrawable = xDrawable;
   pdraw->base.drawable = drawable;
   pdraw->base.psc = &psc->base;

   (void) __glXInitialize(psc->base.dpy);

   if (loader_dri3_drawable_init(XGetXCBConnection(base->dpy),
                                 xDrawable, psc->driScreen,
                                 psc->is_different_gpu, config->driConfig,
                                 &psc->loader_dri3_ext, &glx_dri3_vtable,
                                 &pdraw->loader_drawable)) {
      free(pdraw);
      return NULL;
   }

   return &pdraw->base;
}

/** dri3_wait_for_msc
 *
 * Get the X server to send an event when the target msc/divisor/remainder is
 * reached.
 */
static int
dri3_wait_for_msc(__GLXDRIdrawable *pdraw, int64_t target_msc, int64_t divisor,
                  int64_t remainder, int64_t *ust, int64_t *msc, int64_t *sbc)
{
   struct dri3_drawable *priv = (struct dri3_drawable *) pdraw;

   loader_dri3_wait_for_msc(&priv->loader_drawable, target_msc, divisor,
                            remainder, ust, msc, sbc);

   return 1;
}

/** dri3_drawable_get_msc
 *
 * Return the current UST/MSC/SBC triplet by asking the server
 * for an event
 */
static int
dri3_drawable_get_msc(struct glx_screen *psc, __GLXDRIdrawable *pdraw,
                      int64_t *ust, int64_t *msc, int64_t *sbc)
{
   return dri3_wait_for_msc(pdraw, 0, 0, 0, ust, msc,sbc);
}

/** dri3_wait_for_sbc
 *
 * Wait for the completed swap buffer count to reach the specified
 * target. Presumably the application knows that this will be reached with
 * outstanding complete events, or we're going to be here awhile.
 */
static int
dri3_wait_for_sbc(__GLXDRIdrawable *pdraw, int64_t target_sbc, int64_t *ust,
                  int64_t *msc, int64_t *sbc)
{
   struct dri3_drawable *priv = (struct dri3_drawable *) pdraw;

   return loader_dri3_wait_for_sbc(&priv->loader_drawable, target_sbc,
                                   ust, msc, sbc);
}

static void
dri3_copy_sub_buffer(__GLXDRIdrawable *pdraw, int x, int y,
                     int width, int height,
                     Bool flush)
{
   struct dri3_drawable *priv = (struct dri3_drawable *) pdraw;

   loader_dri3_copy_sub_buffer(&priv->loader_drawable, x, y,
                               width, height, flush);
}

static void
dri3_wait_x(struct glx_context *gc)
{
   struct dri3_drawable *priv = (struct dri3_drawable *)
      GetGLXDRIDrawable(gc->currentDpy, gc->currentDrawable);

   if (priv)
      loader_dri3_wait_x(&priv->loader_drawable);
}

static void
dri3_wait_gl(struct glx_context *gc)
{
   struct dri3_drawable *priv = (struct dri3_drawable *)
      GetGLXDRIDrawable(gc->currentDpy, gc->currentDrawable);

   if (priv)
      loader_dri3_wait_gl(&priv->loader_drawable);
}

/**
 * Called by the driver when it needs to update the real front buffer with the
 * contents of its fake front buffer.
 */
static void
dri3_flush_front_buffer(__DRIdrawable *driDrawable, void *loaderPrivate)
{
   struct loader_dri3_drawable *draw = loaderPrivate;
   struct dri3_drawable *pdraw = loader_drawable_to_dri3_drawable(draw);
   struct dri3_screen *psc;

   if (!pdraw)
      return;

   if (!pdraw->base.psc)
      return;

   psc = (struct dri3_screen *) pdraw->base.psc;

   (void) __glXInitialize(psc->base.dpy);

   loader_dri3_flush(draw, __DRI2_FLUSH_DRAWABLE, __DRI2_THROTTLE_FLUSHFRONT);

   (*psc->f->invalidate)(driDrawable);
   loader_dri3_wait_gl(draw);
}

/**
 * Make sure all pending swapbuffers have been submitted to hardware
 *
 * \param driDrawable[in]  Pointer to the dri drawable whose swaps we are
 * flushing.
 * \param loaderPrivate[in]  Pointer to the corresponding struct
 * loader_dri_drawable.
 */
static void
dri3_flush_swap_buffers(__DRIdrawable *driDrawable, void *loaderPrivate)
{
   struct loader_dri3_drawable *draw = loaderPrivate;
   struct dri3_drawable *pdraw = loader_drawable_to_dri3_drawable(draw);
   struct dri3_screen *psc;

   if (!pdraw)
      return;

   if (!pdraw->base.psc)
      return;

   psc = (struct dri3_screen *) pdraw->base.psc;

   (void) __glXInitialize(psc->base.dpy);
   loader_dri3_swapbuffer_barrier(draw);
}

static void
dri_set_background_context(void *loaderPrivate)
{
   struct dri3_context *pcp = (struct dri3_context *)loaderPrivate;
   __glXSetCurrentContext(&pcp->base);
}

static GLboolean
dri_is_thread_safe(void *loaderPrivate)
{
   /* Unlike DRI2, DRI3 doesn't call GetBuffers/GetBuffersWithFormat
    * during draw so we're safe here.
    */
   return true;
}

/* The image loader extension record for DRI3
 */
static const __DRIimageLoaderExtension imageLoaderExtension = {
   .base = { __DRI_IMAGE_LOADER, 3 },

   .getBuffers          = loader_dri3_get_buffers,
   .flushFrontBuffer    = dri3_flush_front_buffer,
   .flushSwapBuffers    = dri3_flush_swap_buffers,
};

const __DRIuseInvalidateExtension dri3UseInvalidate = {
   .base = { __DRI_USE_INVALIDATE, 1 }
};

static const __DRIbackgroundCallableExtension driBackgroundCallable = {
   .base = { __DRI_BACKGROUND_CALLABLE, 2 },

   .setBackgroundContext = dri_set_background_context,
   .isThreadSafe         = dri_is_thread_safe,
};

static const __DRIextension *loader_extensions[] = {
   &imageLoaderExtension.base,
   &dri3UseInvalidate.base,
   &driBackgroundCallable.base,
   NULL
};

/** dri3_swap_buffers
 *
 * Make the current back buffer visible using the present extension
 */
static int64_t
dri3_swap_buffers(__GLXDRIdrawable *pdraw, int64_t target_msc, int64_t divisor,
                  int64_t remainder, Bool flush)
{
   struct dri3_drawable *priv = (struct dri3_drawable *) pdraw;
   unsigned flags = __DRI2_FLUSH_DRAWABLE;

   if (flush)
      flags |= __DRI2_FLUSH_CONTEXT;

   return loader_dri3_swap_buffers_msc(&priv->loader_drawable,
                                       target_msc, divisor, remainder,
                                       flags, false);
}

static int
dri3_get_buffer_age(__GLXDRIdrawable *pdraw)
{
   struct dri3_drawable *priv = (struct dri3_drawable *)pdraw;

   return loader_dri3_query_buffer_age(&priv->loader_drawable);
}

/** dri3_destroy_screen
 */
static void
dri3_destroy_screen(struct glx_screen *base)
{
   struct dri3_screen *psc = (struct dri3_screen *) base;

   /* Free the direct rendering per screen data */
   loader_dri3_close_screen(psc->driScreen);
   (*psc->core->destroyScreen) (psc->driScreen);
   driDestroyConfigs(psc->driver_configs);
   close(psc->fd);
   free(psc);
}

/** dri3_set_swap_interval
 *
 * Record the application swap interval specification,
 */
static int
dri3_set_swap_interval(__GLXDRIdrawable *pdraw, int interval)
{
   assert(pdraw != NULL);

   struct dri3_drawable *priv =  (struct dri3_drawable *) pdraw;
   GLint vblank_mode = DRI_CONF_VBLANK_DEF_INTERVAL_1;
   struct dri3_screen *psc = (struct dri3_screen *) priv->base.psc;

   if (psc->config)
      psc->config->configQueryi(psc->driScreen,
                                "vblank_mode", &vblank_mode);

   switch (vblank_mode) {
   case DRI_CONF_VBLANK_NEVER:
      if (interval != 0)
         return GLX_BAD_VALUE;
      break;
   case DRI_CONF_VBLANK_ALWAYS_SYNC:
      if (interval <= 0)
         return GLX_BAD_VALUE;
      break;
   default:
      break;
   }

   priv->swap_interval = interval;
   loader_dri3_set_swap_interval(&priv->loader_drawable, interval);

   return 0;
}

/** dri3_get_swap_interval
 *
 * Return the stored swap interval
 */
static int
dri3_get_swap_interval(__GLXDRIdrawable *pdraw)
{
   assert(pdraw != NULL);

   struct dri3_drawable *priv =  (struct dri3_drawable *) pdraw;

  return priv->swap_interval;
}

static void
dri3_bind_tex_image(Display * dpy,
                    GLXDrawable drawable,
                    int buffer, const int *attrib_list)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct dri3_context *pcp = (struct dri3_context *) gc;
   __GLXDRIdrawable *base = GetGLXDRIDrawable(dpy, drawable);
   struct dri3_drawable *pdraw = (struct dri3_drawable *) base;
   struct dri3_screen *psc;

   if (pdraw != NULL) {
      psc = (struct dri3_screen *) base->psc;

      (*psc->f->invalidate)(pdraw->loader_drawable.dri_drawable);

      XSync(dpy, false);

      (*psc->texBuffer->setTexBuffer2) (pcp->driContext,
                                        pdraw->base.textureTarget,
                                        pdraw->base.textureFormat,
                                        pdraw->loader_drawable.dri_drawable);
   }
}

static void
dri3_release_tex_image(Display * dpy, GLXDrawable drawable, int buffer)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct dri3_context *pcp = (struct dri3_context *) gc;
   __GLXDRIdrawable *base = GetGLXDRIDrawable(dpy, drawable);
   struct dri3_drawable *pdraw = (struct dri3_drawable *) base;
   struct dri3_screen *psc;

   if (pdraw != NULL) {
      psc = (struct dri3_screen *) base->psc;

      if (psc->texBuffer->base.version >= 3 &&
          psc->texBuffer->releaseTexBuffer != NULL)
         (*psc->texBuffer->releaseTexBuffer) (pcp->driContext,
                                              pdraw->base.textureTarget,
                                              pdraw->loader_drawable.dri_drawable);
   }
}

static const struct glx_context_vtable dri3_context_vtable = {
   .destroy             = dri3_destroy_context,
   .bind                = dri3_bind_context,
   .unbind              = dri3_unbind_context,
   .wait_gl             = dri3_wait_gl,
   .wait_x              = dri3_wait_x,
   .use_x_font          = DRI_glXUseXFont,
   .bind_tex_image      = dri3_bind_tex_image,
   .release_tex_image   = dri3_release_tex_image,
   .get_proc_address    = NULL,
   .interop_query_device_info = dri3_interop_query_device_info,
   .interop_export_object = dri3_interop_export_object
};

/** dri3_bind_extensions
 *
 * Enable all of the extensions supported on DRI3
 */
static void
dri3_bind_extensions(struct dri3_screen *psc, struct glx_display * priv,
                     const char *driverName)
{
   const __DRIextension **extensions;
   unsigned mask;
   int i;

   extensions = psc->core->getExtensions(psc->driScreen);

   __glXEnableDirectExtension(&psc->base, "GLX_SGI_video_sync");
   __glXEnableDirectExtension(&psc->base, "GLX_SGI_swap_control");
   __glXEnableDirectExtension(&psc->base, "GLX_MESA_swap_control");
   __glXEnableDirectExtension(&psc->base, "GLX_SGI_make_current_read");
   __glXEnableDirectExtension(&psc->base, "GLX_INTEL_swap_event");

   mask = psc->image_driver->getAPIMask(psc->driScreen);

   __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context");
   __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context_profile");

   if ((mask & ((1 << __DRI_API_GLES) |
                (1 << __DRI_API_GLES2) |
                (1 << __DRI_API_GLES3))) != 0) {
      __glXEnableDirectExtension(&psc->base,
                                 "GLX_EXT_create_context_es_profile");
      __glXEnableDirectExtension(&psc->base,
                                 "GLX_EXT_create_context_es2_profile");
   }

   for (i = 0; extensions[i]; i++) {
      /* when on a different gpu than the server, the server pixmaps
       * can have a tiling mode we can't read. Thus we can't create
       * a texture from them.
       */
      if (!psc->is_different_gpu &&
         (strcmp(extensions[i]->name, __DRI_TEX_BUFFER) == 0)) {
         psc->texBuffer = (__DRItexBufferExtension *) extensions[i];
         __glXEnableDirectExtension(&psc->base, "GLX_EXT_texture_from_pixmap");
      }

      if ((strcmp(extensions[i]->name, __DRI2_FLUSH) == 0)) {
         psc->f = (__DRI2flushExtension *) extensions[i];
         /* internal driver extension, no GL extension exposed */
      }

      if (strcmp(extensions[i]->name, __DRI_IMAGE) == 0)
         psc->image = (__DRIimageExtension *) extensions[i];

      if ((strcmp(extensions[i]->name, __DRI2_CONFIG_QUERY) == 0))
         psc->config = (__DRI2configQueryExtension *) extensions[i];

      if (strcmp(extensions[i]->name, __DRI2_ROBUSTNESS) == 0)
         __glXEnableDirectExtension(&psc->base,
                                    "GLX_ARB_create_context_robustness");

      if (strcmp(extensions[i]->name, __DRI2_RENDERER_QUERY) == 0) {
         psc->rendererQuery = (__DRI2rendererQueryExtension *) extensions[i];
         __glXEnableDirectExtension(&psc->base, "GLX_MESA_query_renderer");
      }

      if (strcmp(extensions[i]->name, __DRI2_INTEROP) == 0)
	 psc->interop = (__DRI2interopExtension*)extensions[i];
   }
}

static const struct glx_screen_vtable dri3_screen_vtable = {
   .create_context         = dri3_create_context,
   .create_context_attribs = dri3_create_context_attribs,
   .query_renderer_integer = dri3_query_renderer_integer,
   .query_renderer_string  = dri3_query_renderer_string,
};

/** dri3_create_screen
 *
 * Initialize DRI3 on the specified screen.
 *
 * Opens the DRI device, locates the appropriate DRI driver
 * and loads that.
 *
 * Checks to see if the driver supports the necessary extensions
 *
 * Initializes the driver for the screen and sets up our structures
 */

static struct glx_screen *
dri3_create_screen(int screen, struct glx_display * priv)
{
   xcb_connection_t *c = XGetXCBConnection(priv->dpy);
   const __DRIconfig **driver_configs;
   const __DRIextension **extensions;
   const struct dri3_display *const pdp = (struct dri3_display *)
      priv->dri3Display;
   struct dri3_screen *psc;
   __GLXDRIscreen *psp;
   struct glx_config *configs = NULL, *visuals = NULL;
   char *driverName, *tmp;
   int i;
   unsigned char disable;

   psc = calloc(1, sizeof *psc);
   if (psc == NULL)
      return NULL;

   psc->fd = -1;

   if (!glx_screen_init(&psc->base, screen, priv)) {
      free(psc);
      return NULL;
   }

   psc->fd = loader_dri3_open(c, RootWindow(priv->dpy, screen), None);
   if (psc->fd < 0) {
      int conn_error = xcb_connection_has_error(c);

      glx_screen_cleanup(&psc->base);
      free(psc);
      InfoMessageF("screen %d does not appear to be DRI3 capable\n", screen);

      if (conn_error)
         ErrorMessageF("Connection closed during DRI3 initialization failure");

      return NULL;
   }

   psc->fd = loader_get_user_preferred_fd(psc->fd, &psc->is_different_gpu);

   driverName = loader_get_driver_for_fd(psc->fd);
   if (!driverName) {
      ErrorMessageF("No driver found\n");
      goto handle_error;
   }

   psc->driver = driOpenDriver(driverName);
   if (psc->driver == NULL) {
      ErrorMessageF("driver pointer missing\n");
      goto handle_error;
   }

   extensions = driGetDriverExtensions(psc->driver, driverName);
   if (extensions == NULL)
      goto handle_error;

   for (i = 0; extensions[i]; i++) {
      if (strcmp(extensions[i]->name, __DRI_CORE) == 0)
         psc->core = (__DRIcoreExtension *) extensions[i];
      if (strcmp(extensions[i]->name, __DRI_IMAGE_DRIVER) == 0)
         psc->image_driver = (__DRIimageDriverExtension *) extensions[i];
   }


   if (psc->core == NULL) {
      ErrorMessageF("core dri driver extension not found\n");
      goto handle_error;
   }

   if (psc->image_driver == NULL) {
      ErrorMessageF("image driver extension not found\n");
      goto handle_error;
   }

   psc->driScreen =
      psc->image_driver->createNewScreen2(screen, psc->fd,
                                          pdp->loader_extensions,
                                          extensions,
                                          &driver_configs, psc);

   if (psc->driScreen == NULL) {
      ErrorMessageF("failed to create dri screen\n");
      goto handle_error;
   }

   dri3_bind_extensions(psc, priv, driverName);

   if (!psc->image || psc->image->base.version < 7 || !psc->image->createImageFromFds) {
      ErrorMessageF("Version 7 or imageFromFds image extension not found\n");
      goto handle_error;
   }

   if (!psc->f || psc->f->base.version < 4) {
      ErrorMessageF("Version 4 or later of flush extension not found\n");
      goto handle_error;
   }

   if (psc->is_different_gpu && psc->image->base.version < 9) {
      ErrorMessageF("Different GPU, but image extension version 9 or later not found\n");
      goto handle_error;
   }

   if (psc->is_different_gpu && !psc->image->blitImage) {
      ErrorMessageF("Different GPU, but blitImage not implemented for this driver\n");
      goto handle_error;
   }

   if (!psc->is_different_gpu && (
       !psc->texBuffer || psc->texBuffer->base.version < 2 ||
       !psc->texBuffer->setTexBuffer2
       )) {
      ErrorMessageF("Version 2 or later of texBuffer extension not found\n");
      goto handle_error;
   }

   psc->loader_dri3_ext.core = psc->core;
   psc->loader_dri3_ext.image_driver = psc->image_driver;
   psc->loader_dri3_ext.flush = psc->f;
   psc->loader_dri3_ext.tex_buffer = psc->texBuffer;
   psc->loader_dri3_ext.image = psc->image;
   psc->loader_dri3_ext.config = psc->config;

   configs = driConvertConfigs(psc->core, psc->base.configs, driver_configs);
   visuals = driConvertConfigs(psc->core, psc->base.visuals, driver_configs);

   if (!configs || !visuals) {
       ErrorMessageF("No matching fbConfigs or visuals found\n");
       goto handle_error;
   }

   glx_config_destroy_list(psc->base.configs);
   psc->base.configs = configs;
   glx_config_destroy_list(psc->base.visuals);
   psc->base.visuals = visuals;

   psc->driver_configs = driver_configs;

   psc->base.vtable = &dri3_screen_vtable;
   psp = &psc->vtable;
   psc->base.driScreen = psp;
   psp->destroyScreen = dri3_destroy_screen;
   psp->createDrawable = dri3_create_drawable;
   psp->swapBuffers = dri3_swap_buffers;

   psp->getDrawableMSC = dri3_drawable_get_msc;
   psp->waitForMSC = dri3_wait_for_msc;
   psp->waitForSBC = dri3_wait_for_sbc;
   psp->setSwapInterval = dri3_set_swap_interval;
   psp->getSwapInterval = dri3_get_swap_interval;
   if (psc->config->configQueryb(psc->driScreen,
                                 "glx_disable_oml_sync_control",
                                 &disable) || !disable)
      __glXEnableDirectExtension(&psc->base, "GLX_OML_sync_control");

   psp->copySubBuffer = dri3_copy_sub_buffer;
   __glXEnableDirectExtension(&psc->base, "GLX_MESA_copy_sub_buffer");

   psp->getBufferAge = dri3_get_buffer_age;
   if (psc->config->configQueryb(psc->driScreen,
                                 "glx_disable_ext_buffer_age",
                                 &disable) || !disable)
      __glXEnableDirectExtension(&psc->base, "GLX_EXT_buffer_age");

   free(driverName);

   tmp = getenv("LIBGL_SHOW_FPS");
   psc->show_fps_interval = tmp ? atoi(tmp) : 0;
   if (psc->show_fps_interval < 0)
      psc->show_fps_interval = 0;

   InfoMessageF("Using DRI3 for screen %d\n", screen);

   return &psc->base;

handle_error:
   CriticalErrorMessageF("failed to load driver: %s\n", driverName);

   if (configs)
       glx_config_destroy_list(configs);
   if (visuals)
       glx_config_destroy_list(visuals);
   if (psc->driScreen)
       psc->core->destroyScreen(psc->driScreen);
   psc->driScreen = NULL;
   if (psc->fd >= 0)
      close(psc->fd);
   if (psc->driver)
      dlclose(psc->driver);

   free(driverName);
   glx_screen_cleanup(&psc->base);
   free(psc);

   return NULL;
}

/** dri_destroy_display
 *
 * Called from __glXFreeDisplayPrivate.
 */
static void
dri3_destroy_display(__GLXDRIdisplay * dpy)
{
   free(dpy);
}

/** dri3_create_display
 *
 * Allocate, initialize and return a __DRIdisplayPrivate object.
 * This is called from __glXInitialize() when we are given a new
 * display pointer. This is public to that function, but hidden from
 * outside of libGL.
 */
_X_HIDDEN __GLXDRIdisplay *
dri3_create_display(Display * dpy)
{
   struct dri3_display                  *pdp;
   xcb_connection_t                     *c = XGetXCBConnection(dpy);
   xcb_dri3_query_version_cookie_t      dri3_cookie;
   xcb_dri3_query_version_reply_t       *dri3_reply;
   xcb_present_query_version_cookie_t   present_cookie;
   xcb_present_query_version_reply_t    *present_reply;
   xcb_generic_error_t                  *error;
   const xcb_query_extension_reply_t    *extension;

   xcb_prefetch_extension_data(c, &xcb_dri3_id);
   xcb_prefetch_extension_data(c, &xcb_present_id);

   extension = xcb_get_extension_data(c, &xcb_dri3_id);
   if (!(extension && extension->present))
      return NULL;

   extension = xcb_get_extension_data(c, &xcb_present_id);
   if (!(extension && extension->present))
      return NULL;

   dri3_cookie = xcb_dri3_query_version(c,
                                        XCB_DRI3_MAJOR_VERSION,
                                        XCB_DRI3_MINOR_VERSION);


   present_cookie = xcb_present_query_version(c,
                                   XCB_PRESENT_MAJOR_VERSION,
                                   XCB_PRESENT_MINOR_VERSION);

   pdp = malloc(sizeof *pdp);
   if (pdp == NULL)
      return NULL;

   dri3_reply = xcb_dri3_query_version_reply(c, dri3_cookie, &error);
   if (!dri3_reply) {
      free(error);
      goto no_extension;
   }

   pdp->dri3Major = dri3_reply->major_version;
   pdp->dri3Minor = dri3_reply->minor_version;
   free(dri3_reply);

   present_reply = xcb_present_query_version_reply(c, present_cookie, &error);
   if (!present_reply) {
      free(error);
      goto no_extension;
   }
   pdp->presentMajor = present_reply->major_version;
   pdp->presentMinor = present_reply->minor_version;
   free(present_reply);

   pdp->base.destroyDisplay = dri3_destroy_display;
   pdp->base.createScreen = dri3_create_screen;

   loader_set_logger(dri_message);

   pdp->loader_extensions = loader_extensions;

   return &pdp->base;
no_extension:
   free(pdp);
   return NULL;
}

#endif /* GLX_DIRECT_RENDERING */
