/*
 * Copyright 2008 George Sapountzis
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
 */

#if defined(GLX_DIRECT_RENDERING) && !defined(GLX_USE_APPLEGL)

#include <X11/Xlib.h>
#include "glxclient.h"
#include <dlfcn.h>
#include "dri_common.h"
#include "drisw_priv.h"

static Bool
XCreateDrawable(struct drisw_drawable * pdp,
                Display * dpy, XID drawable, int visualid)
{
   XGCValues gcvalues;
   long visMask;
   XVisualInfo visTemp;
   int num_visuals;

   /* create GC's */
   pdp->gc = XCreateGC(dpy, drawable, 0, NULL);
   pdp->swapgc = XCreateGC(dpy, drawable, 0, NULL);

   gcvalues.function = GXcopy;
   gcvalues.graphics_exposures = False;
   XChangeGC(dpy, pdp->gc, GCFunction, &gcvalues);
   XChangeGC(dpy, pdp->swapgc, GCFunction, &gcvalues);
   XChangeGC(dpy, pdp->swapgc, GCGraphicsExposures, &gcvalues);

   /* visual */
   visTemp.visualid = visualid;
   visMask = VisualIDMask;
   pdp->visinfo = XGetVisualInfo(dpy, visMask, &visTemp, &num_visuals);

   if (!pdp->visinfo || num_visuals == 0)
      return False;

   /* create XImage */
   pdp->ximage = XCreateImage(dpy,
                              pdp->visinfo->visual,
                              pdp->visinfo->depth,
                              ZPixmap, 0,             /* format, offset */
                              NULL,                   /* data */
                              0, 0,                   /* width, height */
                              32,                     /* bitmap_pad */
                              0);                     /* bytes_per_line */

  /**
   * swrast does not handle 24-bit depth with 24 bpp, so let X do the
   * the conversion for us.
   */
  if (pdp->ximage->bits_per_pixel == 24)
     pdp->ximage->bits_per_pixel = 32;

   return True;
}

static void
XDestroyDrawable(struct drisw_drawable * pdp, Display * dpy, XID drawable)
{
   XDestroyImage(pdp->ximage);
   free(pdp->visinfo);

   XFreeGC(dpy, pdp->gc);
   XFreeGC(dpy, pdp->swapgc);
}

/**
 * swrast loader functions
 */

static void
swrastGetDrawableInfo(__DRIdrawable * draw,
                      int *x, int *y, int *w, int *h,
                      void *loaderPrivate)
{
   struct drisw_drawable *pdp = loaderPrivate;
   __GLXDRIdrawable *pdraw = &(pdp->base);
   Display *dpy = pdraw->psc->dpy;
   Drawable drawable;

   Window root;
   unsigned uw, uh, bw, depth;

   drawable = pdraw->xDrawable;

   XGetGeometry(dpy, drawable, &root, x, y, &uw, &uh, &bw, &depth);
   *w = uw;
   *h = uh;
}

/**
 * Align renderbuffer pitch.
 *
 * This should be chosen by the driver and the loader (libGL, xserver/glx)
 * should use the driver provided pitch.
 *
 * It seems that the xorg loader (that is the xserver loading swrast_dri for
 * indirect rendering, not client-side libGL) requires that the pitch is
 * exactly the image width padded to 32 bits. XXX
 *
 * The above restriction can probably be overcome by using ScratchPixmap and
 * CopyArea in the xserver, similar to ShmPutImage, and setting the width of
 * the scratch pixmap to 'pitch / cpp'.
 */
static inline int
bytes_per_line(unsigned pitch_bits, unsigned mul)
{
   unsigned mask = mul - 1;

   return ((pitch_bits + mask) & ~mask) / 8;
}

static void
swrastPutImage2(__DRIdrawable * draw, int op,
                int x, int y, int w, int h, int stride,
                char *data, void *loaderPrivate)
{
   struct drisw_drawable *pdp = loaderPrivate;
   __GLXDRIdrawable *pdraw = &(pdp->base);
   Display *dpy = pdraw->psc->dpy;
   Drawable drawable;
   XImage *ximage;
   GC gc;

   switch (op) {
   case __DRI_SWRAST_IMAGE_OP_DRAW:
      gc = pdp->gc;
      break;
   case __DRI_SWRAST_IMAGE_OP_SWAP:
      gc = pdp->swapgc;
      break;
   default:
      return;
   }

   drawable = pdraw->xDrawable;

   ximage = pdp->ximage;
   ximage->data = data;
   ximage->width = w;
   ximage->height = h;
   ximage->bytes_per_line = stride ? stride : bytes_per_line(w * ximage->bits_per_pixel, 32);

   XPutImage(dpy, drawable, gc, ximage, 0, 0, x, y, w, h);

   ximage->data = NULL;
}

static void
swrastPutImage(__DRIdrawable * draw, int op,
               int x, int y, int w, int h,
               char *data, void *loaderPrivate)
{
   swrastPutImage2(draw, op, x, y, w, h, 0, data, loaderPrivate);
}

static void
swrastGetImage2(__DRIdrawable * read,
                int x, int y, int w, int h, int stride,
                char *data, void *loaderPrivate)
{
   struct drisw_drawable *prp = loaderPrivate;
   __GLXDRIdrawable *pread = &(prp->base);
   Display *dpy = pread->psc->dpy;
   Drawable readable;
   XImage *ximage;

   readable = pread->xDrawable;

   ximage = prp->ximage;
   ximage->data = data;
   ximage->width = w;
   ximage->height = h;
   ximage->bytes_per_line = stride ? stride : bytes_per_line(w * ximage->bits_per_pixel, 32);

   XGetSubImage(dpy, readable, x, y, w, h, ~0L, ZPixmap, ximage, 0, 0);

   ximage->data = NULL;
}

static void
swrastGetImage(__DRIdrawable * read,
               int x, int y, int w, int h,
               char *data, void *loaderPrivate)
{
   swrastGetImage2(read, x, y, w, h, 0, data, loaderPrivate);
}

static const __DRIswrastLoaderExtension swrastLoaderExtension = {
   .base = {__DRI_SWRAST_LOADER, 3 },

   .getDrawableInfo     = swrastGetDrawableInfo,
   .putImage            = swrastPutImage,
   .getImage            = swrastGetImage,
   .putImage2           = swrastPutImage2,
   .getImage2           = swrastGetImage2,
};

static const __DRIextension *loader_extensions[] = {
   &swrastLoaderExtension.base,
   NULL
};

/**
 * GLXDRI functions
 */

static void
drisw_destroy_context(struct glx_context *context)
{
   struct drisw_context *pcp = (struct drisw_context *) context;
   struct drisw_screen *psc = (struct drisw_screen *) context->psc;

   driReleaseDrawables(&pcp->base);

   free((char *) context->extensions);

   (*psc->core->destroyContext) (pcp->driContext);

   free(pcp);
}

static int
drisw_bind_context(struct glx_context *context, struct glx_context *old,
		   GLXDrawable draw, GLXDrawable read)
{
   struct drisw_context *pcp = (struct drisw_context *) context;
   struct drisw_screen *psc = (struct drisw_screen *) pcp->base.psc;
   struct drisw_drawable *pdraw, *pread;

   pdraw = (struct drisw_drawable *) driFetchDrawable(context, draw);
   pread = (struct drisw_drawable *) driFetchDrawable(context, read);

   driReleaseDrawables(&pcp->base);

   if ((*psc->core->bindContext) (pcp->driContext,
                                  pdraw ? pdraw->driDrawable : NULL,
                                  pread ? pread->driDrawable : NULL))
      return Success;

   return GLXBadContext;
}

static void
drisw_unbind_context(struct glx_context *context, struct glx_context *new)
{
   struct drisw_context *pcp = (struct drisw_context *) context;
   struct drisw_screen *psc = (struct drisw_screen *) pcp->base.psc;

   (*psc->core->unbindContext) (pcp->driContext);
}

static void
drisw_bind_tex_image(Display * dpy,
		    GLXDrawable drawable,
		    int buffer, const int *attrib_list)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct drisw_context *pcp = (struct drisw_context *) gc;
   __GLXDRIdrawable *base = GetGLXDRIDrawable(dpy, drawable);
   struct drisw_drawable *pdraw = (struct drisw_drawable *) base;
   struct drisw_screen *psc;

   __glXInitialize(dpy);

   if (pdraw != NULL) {
      psc = (struct drisw_screen *) base->psc;

      if (!psc->texBuffer)
         return;

      if (psc->texBuffer->base.version >= 2 &&
        psc->texBuffer->setTexBuffer2 != NULL) {
	      (*psc->texBuffer->setTexBuffer2) (pcp->driContext,
					   pdraw->base.textureTarget,
					   pdraw->base.textureFormat,
					   pdraw->driDrawable);
      }
      else {
	      (*psc->texBuffer->setTexBuffer) (pcp->driContext,
					  pdraw->base.textureTarget,
					  pdraw->driDrawable);
      }
   }
}

static void
drisw_release_tex_image(Display * dpy, GLXDrawable drawable, int buffer)
{
   struct glx_context *gc = __glXGetCurrentContext();
   struct drisw_context *pcp = (struct drisw_context *) gc;
   __GLXDRIdrawable *base = GetGLXDRIDrawable(dpy, drawable);
   struct glx_display *dpyPriv = __glXInitialize(dpy);
   struct drisw_drawable *pdraw = (struct drisw_drawable *) base;
   struct drisw_screen *psc;

   if (dpyPriv != NULL && pdraw != NULL) {
      psc = (struct drisw_screen *) base->psc;

      if (!psc->texBuffer)
         return;

      if (psc->texBuffer->base.version >= 3 &&
          psc->texBuffer->releaseTexBuffer != NULL) {
         (*psc->texBuffer->releaseTexBuffer) (pcp->driContext,
                                           pdraw->base.textureTarget,
                                           pdraw->driDrawable);
      }
   }
}

static const struct glx_context_vtable drisw_context_vtable = {
   .destroy             = drisw_destroy_context,
   .bind                = drisw_bind_context,
   .unbind              = drisw_unbind_context,
   .wait_gl             = NULL,
   .wait_x              = NULL,
   .use_x_font          = DRI_glXUseXFont,
   .bind_tex_image      = drisw_bind_tex_image,
   .release_tex_image   = drisw_release_tex_image,
   .get_proc_address    = NULL,
};

static struct glx_context *
drisw_create_context(struct glx_screen *base,
		     struct glx_config *config_base,
		     struct glx_context *shareList, int renderType)
{
   struct drisw_context *pcp, *pcp_shared;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;
   struct drisw_screen *psc = (struct drisw_screen *) base;
   __DRIcontext *shared = NULL;

   if (!psc->base.driScreen)
      return NULL;

   /* Check the renderType value */
   if (!validate_renderType_against_config(config_base, renderType))
       return NULL;

   if (shareList) {
      /* If the shareList context is not a DRISW context, we cannot possibly
       * create a DRISW context that shares it.
       */
      if (shareList->vtable->destroy != drisw_destroy_context) {
	 return NULL;
      }

      pcp_shared = (struct drisw_context *) shareList;
      shared = pcp_shared->driContext;
   }

   pcp = calloc(1, sizeof *pcp);
   if (pcp == NULL)
      return NULL;

   if (!glx_context_init(&pcp->base, &psc->base, &config->base)) {
      free(pcp);
      return NULL;
   }

   pcp->base.renderType = renderType;

   pcp->driContext =
      (*psc->core->createNewContext) (psc->driScreen,
				      config->driConfig, shared, pcp);
   if (pcp->driContext == NULL) {
      free(pcp);
      return NULL;
   }

   pcp->base.vtable = &drisw_context_vtable;

   return &pcp->base;
}

static struct glx_context *
drisw_create_context_attribs(struct glx_screen *base,
			     struct glx_config *config_base,
			     struct glx_context *shareList,
			     unsigned num_attribs,
			     const uint32_t *attribs,
			     unsigned *error)
{
   struct drisw_context *pcp, *pcp_shared;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;
   struct drisw_screen *psc = (struct drisw_screen *) base;
   __DRIcontext *shared = NULL;

   uint32_t minor_ver;
   uint32_t major_ver;
   uint32_t renderType;
   uint32_t flags;
   unsigned api;
   int reset;
   uint32_t ctx_attribs[2 * 4];
   unsigned num_ctx_attribs = 0;

   if (!psc->base.driScreen)
      return NULL;

   if (psc->swrast->base.version < 3)
      return NULL;

   /* Remap the GLX tokens to DRI2 tokens.
    */
   if (!dri2_convert_glx_attribs(num_attribs, attribs,
                                 &major_ver, &minor_ver, &renderType, &flags,
                                 &api, &reset, error))
      return NULL;

   /* Check the renderType value */
   if (!validate_renderType_against_config(config_base, renderType)) {
       return NULL;
   }

   if (reset != __DRI_CTX_RESET_NO_NOTIFICATION)
      return NULL;

   if (shareList) {
      pcp_shared = (struct drisw_context *) shareList;
      shared = pcp_shared->driContext;
   }

   pcp = calloc(1, sizeof *pcp);
   if (pcp == NULL)
      return NULL;

   if (!glx_context_init(&pcp->base, &psc->base, &config->base)) {
      free(pcp);
      return NULL;
   }

   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MAJOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = major_ver;
   ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_MINOR_VERSION;
   ctx_attribs[num_ctx_attribs++] = minor_ver;

   if (flags != 0) {
      ctx_attribs[num_ctx_attribs++] = __DRI_CTX_ATTRIB_FLAGS;

      /* The current __DRI_CTX_FLAG_* values are identical to the
       * GLX_CONTEXT_*_BIT values.
       */
      ctx_attribs[num_ctx_attribs++] = flags;
   }

   pcp->base.renderType = renderType;

   pcp->driContext =
      (*psc->swrast->createContextAttribs) (psc->driScreen,
					    api,
					    config->driConfig,
					    shared,
					    num_ctx_attribs / 2,
					    ctx_attribs,
					    error,
					    pcp);
   if (pcp->driContext == NULL) {
      free(pcp);
      return NULL;
   }

   pcp->base.vtable = &drisw_context_vtable;

   return &pcp->base;
}

static void
driswDestroyDrawable(__GLXDRIdrawable * pdraw)
{
   struct drisw_drawable *pdp = (struct drisw_drawable *) pdraw;
   struct drisw_screen *psc = (struct drisw_screen *) pdp->base.psc;

   (*psc->core->destroyDrawable) (pdp->driDrawable);

   XDestroyDrawable(pdp, pdraw->psc->dpy, pdraw->drawable);
   free(pdp);
}

static __GLXDRIdrawable *
driswCreateDrawable(struct glx_screen *base, XID xDrawable,
		    GLXDrawable drawable, struct glx_config *modes)
{
   struct drisw_drawable *pdp;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) modes;
   struct drisw_screen *psc = (struct drisw_screen *) base;
   Bool ret;
   const __DRIswrastExtension *swrast = psc->swrast;

   pdp = calloc(1, sizeof(*pdp));
   if (!pdp)
      return NULL;

   pdp->base.xDrawable = xDrawable;
   pdp->base.drawable = drawable;
   pdp->base.psc = &psc->base;

   ret = XCreateDrawable(pdp, psc->base.dpy, xDrawable, modes->visualID);
   if (!ret) {
      free(pdp);
      return NULL;
   }

   /* Create a new drawable */
   pdp->driDrawable =
      (*swrast->createNewDrawable) (psc->driScreen, config->driConfig, pdp);

   if (!pdp->driDrawable) {
      XDestroyDrawable(pdp, psc->base.dpy, xDrawable);
      free(pdp);
      return NULL;
   }

   pdp->base.destroyDrawable = driswDestroyDrawable;

   return &pdp->base;
}

static int64_t
driswSwapBuffers(__GLXDRIdrawable * pdraw,
                 int64_t target_msc, int64_t divisor, int64_t remainder,
                 Bool flush)
{
   struct drisw_drawable *pdp = (struct drisw_drawable *) pdraw;
   struct drisw_screen *psc = (struct drisw_screen *) pdp->base.psc;

   (void) target_msc;
   (void) divisor;
   (void) remainder;

   if (flush) {
      glFlush();
   }

   (*psc->core->swapBuffers) (pdp->driDrawable);

   return 0;
}

static void
driswCopySubBuffer(__GLXDRIdrawable * pdraw,
                   int x, int y, int width, int height, Bool flush)
{
   struct drisw_drawable *pdp = (struct drisw_drawable *) pdraw;
   struct drisw_screen *psc = (struct drisw_screen *) pdp->base.psc;

   if (flush) {
      glFlush();
   }

   (*psc->copySubBuffer->copySubBuffer) (pdp->driDrawable,
					    x, y, width, height);
}

static void
driswDestroyScreen(struct glx_screen *base)
{
   struct drisw_screen *psc = (struct drisw_screen *) base;

   /* Free the direct rendering per screen data */
   (*psc->core->destroyScreen) (psc->driScreen);
   driDestroyConfigs(psc->driver_configs);
   psc->driScreen = NULL;
   if (psc->driver)
      dlclose(psc->driver);
   free(psc);
}

#define SWRAST_DRIVER_NAME "swrast"

static void *
driOpenSwrast(void)
{
   void *driver = NULL;

   if (driver == NULL)
      driver = driOpenDriver(SWRAST_DRIVER_NAME);

   return driver;
}

static const struct glx_screen_vtable drisw_screen_vtable = {
   .create_context         = drisw_create_context,
   .create_context_attribs = drisw_create_context_attribs,
   .query_renderer_integer = drisw_query_renderer_integer,
   .query_renderer_string  = drisw_query_renderer_string,
};

static void
driswBindExtensions(struct drisw_screen *psc, const __DRIextension **extensions)
{
   int i;

   __glXEnableDirectExtension(&psc->base, "GLX_SGI_make_current_read");

   if (psc->swrast->base.version >= 3) {
      __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context");
      __glXEnableDirectExtension(&psc->base, "GLX_ARB_create_context_profile");

      /* DRISW version >= 2 implies support for OpenGL ES.
       */
      __glXEnableDirectExtension(&psc->base,
				 "GLX_EXT_create_context_es_profile");
      __glXEnableDirectExtension(&psc->base,
				 "GLX_EXT_create_context_es2_profile");
   }

   if (psc->copySubBuffer)
      __glXEnableDirectExtension(&psc->base, "GLX_MESA_copy_sub_buffer");      

   /* FIXME: Figure out what other extensions can be ported here from dri2. */
   for (i = 0; extensions[i]; i++) {
      if ((strcmp(extensions[i]->name, __DRI_TEX_BUFFER) == 0)) {
	 psc->texBuffer = (__DRItexBufferExtension *) extensions[i];
	 __glXEnableDirectExtension(&psc->base, "GLX_EXT_texture_from_pixmap");
      }
      /* DRISW version 3 is also required because GLX_MESA_query_renderer
       * requires GLX_ARB_create_context_profile.
       */
      if (psc->swrast->base.version >= 3
          && strcmp(extensions[i]->name, __DRI2_RENDERER_QUERY) == 0) {
         psc->rendererQuery = (__DRI2rendererQueryExtension *) extensions[i];
         __glXEnableDirectExtension(&psc->base, "GLX_MESA_query_renderer");
      }
   }
}

static struct glx_screen *
driswCreateScreen(int screen, struct glx_display *priv)
{
   __GLXDRIscreen *psp;
   const __DRIconfig **driver_configs;
   const __DRIextension **extensions;
   struct drisw_screen *psc;
   struct glx_config *configs = NULL, *visuals = NULL;
   int i;

   psc = calloc(1, sizeof *psc);
   if (psc == NULL)
      return NULL;

   if (!glx_screen_init(&psc->base, screen, priv)) {
      free(psc);
      return NULL;
   }

   psc->driver = driOpenSwrast();
   if (psc->driver == NULL)
      goto handle_error;

   extensions = driGetDriverExtensions(psc->driver, SWRAST_DRIVER_NAME);
   if (extensions == NULL)
      goto handle_error;

   for (i = 0; extensions[i]; i++) {
      if (strcmp(extensions[i]->name, __DRI_CORE) == 0)
	 psc->core = (__DRIcoreExtension *) extensions[i];
      if (strcmp(extensions[i]->name, __DRI_SWRAST) == 0)
	 psc->swrast = (__DRIswrastExtension *) extensions[i];
      if (strcmp(extensions[i]->name, __DRI_COPY_SUB_BUFFER) == 0)
	 psc->copySubBuffer = (__DRIcopySubBufferExtension *) extensions[i];
   }

   if (psc->core == NULL || psc->swrast == NULL) {
      ErrorMessageF("core dri extension not found\n");
      goto handle_error;
   }

   if (psc->swrast->base.version >= 4) {
      psc->driScreen =
         psc->swrast->createNewScreen2(screen, loader_extensions,
                                       extensions,
                                       &driver_configs, psc);
   } else {
      psc->driScreen =
         psc->swrast->createNewScreen(screen, loader_extensions,
                                      &driver_configs, psc);
   }
   if (psc->driScreen == NULL) {
      ErrorMessageF("failed to create dri screen\n");
      goto handle_error;
   }

   extensions = psc->core->getExtensions(psc->driScreen);
   driswBindExtensions(psc, extensions);

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

   psc->base.vtable = &drisw_screen_vtable;
   psp = &psc->vtable;
   psc->base.driScreen = psp;
   psp->destroyScreen = driswDestroyScreen;
   psp->createDrawable = driswCreateDrawable;
   psp->swapBuffers = driswSwapBuffers;

   if (psc->copySubBuffer)
      psp->copySubBuffer = driswCopySubBuffer;

   return &psc->base;

 handle_error:
   if (configs)
       glx_config_destroy_list(configs);
   if (visuals)
       glx_config_destroy_list(visuals);
   if (psc->driScreen)
       psc->core->destroyScreen(psc->driScreen);
   psc->driScreen = NULL;

   if (psc->driver)
      dlclose(psc->driver);
   glx_screen_cleanup(&psc->base);
   free(psc);

   CriticalErrorMessageF("failed to load driver: %s\n", SWRAST_DRIVER_NAME);

   return NULL;
}

/* Called from __glXFreeDisplayPrivate.
 */
static void
driswDestroyDisplay(__GLXDRIdisplay * dpy)
{
   free(dpy);
}

/*
 * Allocate, initialize and return a __DRIdisplayPrivate object.
 * This is called from __glXInitialize() when we are given a new
 * display pointer.
 */
_X_HIDDEN __GLXDRIdisplay *
driswCreateDisplay(Display * dpy)
{
   struct drisw_display *pdpyp;

   pdpyp = malloc(sizeof *pdpyp);
   if (pdpyp == NULL)
      return NULL;

   pdpyp->base.destroyDisplay = driswDestroyDisplay;
   pdpyp->base.createScreen = driswCreateScreen;

   return &pdpyp->base;
}

#endif /* GLX_DIRECT_RENDERING */
