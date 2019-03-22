/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian Paul <brian@precisioninsight.com>
 *
 */

#if defined(GLX_DIRECT_RENDERING) && !defined(GLX_USE_APPLEGL)

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include "glxclient.h"
#include "xf86dri.h"
#include "dri2.h"
#include "dri_sarea.h"
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/mman.h>
#include "xf86drm.h"
#include "dri_common.h"

struct dri_display
{
   __GLXDRIdisplay base;

   /*
    ** XFree86-DRI version information
    */
   int driMajor;
   int driMinor;
   int driPatch;
};

struct dri_screen
{
   struct glx_screen base;

   __DRIscreen *driScreen;
   __GLXDRIscreen vtable;
   const __DRIlegacyExtension *legacy;
   const __DRIcoreExtension *core;
   const __DRIswapControlExtension *swapControl;
   const __DRImediaStreamCounterExtension *msc;
   const __DRIconfig **driver_configs;
   const __DRIcopySubBufferExtension *driCopySubBuffer;

   void *driver;
   int fd;
};

struct dri_context
{
   struct glx_context base;
   __DRIcontext *driContext;
   XID hwContextID;
};

struct dri_drawable
{
   __GLXDRIdrawable base;

   __DRIdrawable *driDrawable;
};

/*
 * Given a display pointer and screen number, determine the name of
 * the DRI driver for the screen (i.e., "i965", "radeon", "nouveau", etc).
 * Return True for success, False for failure.
 */
static Bool
driGetDriverName(Display * dpy, int scrNum, char **driverName)
{
   int directCapable;
   Bool b;
   int event, error;
   int driverMajor, driverMinor, driverPatch;

   *driverName = NULL;

   if (XF86DRIQueryExtension(dpy, &event, &error)) {    /* DRI1 */
      if (!XF86DRIQueryDirectRenderingCapable(dpy, scrNum, &directCapable)) {
         ErrorMessageF("XF86DRIQueryDirectRenderingCapable failed\n");
         return False;
      }
      if (!directCapable) {
         ErrorMessageF("XF86DRIQueryDirectRenderingCapable returned false\n");
         return False;
      }

      b = XF86DRIGetClientDriverName(dpy, scrNum, &driverMajor, &driverMinor,
                                     &driverPatch, driverName);
      if (!b) {
         ErrorMessageF("Cannot determine driver name for screen %d\n",
                       scrNum);
         return False;
      }

      InfoMessageF("XF86DRIGetClientDriverName: %d.%d.%d %s (screen %d)\n",
                   driverMajor, driverMinor, driverPatch, *driverName,
                   scrNum);

      return True;
   }
   else if (DRI2QueryExtension(dpy, &event, &error)) {  /* DRI2 */
      char *dev;
      Bool ret = DRI2Connect(dpy, RootWindow(dpy, scrNum), driverName, &dev);

      if (ret)
         free(dev);

      return ret;
   }

   return False;
}

/*
 * Exported function for querying the DRI driver for a given screen.
 *
 * The returned char pointer points to a static array that will be
 * overwritten by subsequent calls.
 */
_GLX_PUBLIC const char *
glXGetScreenDriver(Display * dpy, int scrNum)
{
   static char ret[32];
   char *driverName;
   if (driGetDriverName(dpy, scrNum, &driverName)) {
      int len;
      if (!driverName)
         return NULL;
      len = strlen(driverName);
      if (len >= 31)
         return NULL;
      memcpy(ret, driverName, len + 1);
      free(driverName);
      return ret;
   }
   return NULL;
}

/* glXGetDriverConfig must return a pointer with a static lifetime. To avoid
 * keeping drivers loaded and other leaks, we keep a cache of results here that
 * is cleared by an atexit handler.
 */
struct driver_config_entry {
   struct driver_config_entry *next;
   char *driverName;
   char *config;
};

static pthread_mutex_t driver_config_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct driver_config_entry *driver_config_cache = NULL;

/* Called as an atexit function. Otherwise, this would have to be called with
 * driver_config_mutex locked.
 */
static void
clear_driver_config_cache()
{
   while (driver_config_cache) {
      struct driver_config_entry *e = driver_config_cache;
      driver_config_cache = e->next;

      free(e->driverName);
      free(e->config);
      free(e);
   }
}

static char *
get_driver_config(const char *driverName)
{
   void *handle = driOpenDriver(driverName);
   const __DRIextension **extensions;

   if (!handle)
      return NULL;

   char *config = NULL;

   extensions = driGetDriverExtensions(handle, driverName);
   if (extensions) {
      for (int i = 0; extensions[i]; i++) {
         if (strcmp(extensions[i]->name, __DRI_CONFIG_OPTIONS) != 0)
            continue;

         __DRIconfigOptionsExtension *ext =
            (__DRIconfigOptionsExtension *)extensions[i];

         if (ext->base.version >= 2)
            config = ext->getXml(driverName);
         else
            config = strdup(ext->xml);

         break;
      }
   }

   if (!config) {
      /* Fall back to the old method */
      config = dlsym(handle, "__driConfigOptions");
      if (config)
         config = strdup(config);
   }

   dlclose(handle);

   return config;
}

/*
 * Exported function for obtaining a driver's option list (UTF-8 encoded XML).
 *
 * The returned char pointer points directly into the driver. Therefore
 * it should be treated as a constant.
 *
 * If the driver was not found or does not support configuration NULL is
 * returned.
 */
_GLX_PUBLIC const char *
glXGetDriverConfig(const char *driverName)
{
   struct driver_config_entry *e;

   pthread_mutex_lock(&driver_config_mutex);

   for (e = driver_config_cache; e; e = e->next) {
      if (strcmp(e->driverName, driverName) == 0)
         goto out;
   }

   e = malloc(sizeof(*e));
   if (!e)
      goto out;

   e->config = get_driver_config(driverName);
   e->driverName = strdup(driverName);
   if (!e->config || !e->driverName) {
      free(e->config);
      free(e->driverName);
      free(e);
      e = NULL;
      goto out;
   }

   e->next = driver_config_cache;
   driver_config_cache = e;

   if (!e->next)
      atexit(clear_driver_config_cache);

out:
   pthread_mutex_unlock(&driver_config_mutex);

   return e ? e->config : NULL;
}

static GLboolean
has_damage_post(Display * dpy)
{
   static GLboolean inited = GL_FALSE;
   static GLboolean has_damage;

   if (!inited) {
      int major, minor;

      if (XDamageQueryVersion(dpy, &major, &minor) &&
          major == 1 && minor >= 1) {
         has_damage = GL_TRUE;
      }
      else {
         has_damage = GL_FALSE;
      }
      inited = GL_TRUE;
   }

   return has_damage;
}

static void
__glXReportDamage(__DRIdrawable * driDraw,
                  int x, int y,
                  drm_clip_rect_t * rects, int num_rects,
                  GLboolean front_buffer, void *loaderPrivate)
{
   XRectangle *xrects;
   XserverRegion region;
   int i;
   int x_off, y_off;
   __GLXDRIdrawable *glxDraw = loaderPrivate;
   struct glx_screen *psc = glxDraw->psc;
   Display *dpy = psc->dpy;
   Drawable drawable;

   if (!has_damage_post(dpy))
      return;

   if (front_buffer) {
      x_off = x;
      y_off = y;
      drawable = RootWindow(dpy, psc->scr);
   }
   else {
      x_off = 0;
      y_off = 0;
      drawable = glxDraw->xDrawable;
   }

   xrects = malloc(sizeof(XRectangle) * num_rects);
   if (xrects == NULL)
      return;

   for (i = 0; i < num_rects; i++) {
      xrects[i].x = rects[i].x1 + x_off;
      xrects[i].y = rects[i].y1 + y_off;
      xrects[i].width = rects[i].x2 - rects[i].x1;
      xrects[i].height = rects[i].y2 - rects[i].y1;
   }
   region = XFixesCreateRegion(dpy, xrects, num_rects);
   free(xrects);
   XDamageAdd(dpy, drawable, region);
   XFixesDestroyRegion(dpy, region);
}

static const __DRIdamageExtension damageExtension = {
   .base = {__DRI_DAMAGE, 1 },

   .reportDamage        = __glXReportDamage,
};

static GLboolean
__glXDRIGetDrawableInfo(__DRIdrawable * drawable,
                        unsigned int *index, unsigned int *stamp,
                        int *X, int *Y, int *W, int *H,
                        int *numClipRects, drm_clip_rect_t ** pClipRects,
                        int *backX, int *backY,
                        int *numBackClipRects,
                        drm_clip_rect_t ** pBackClipRects,
                        void *loaderPrivate)
{
   __GLXDRIdrawable *glxDraw = loaderPrivate;
   struct glx_screen *psc = glxDraw->psc;
   Display *dpy = psc->dpy;

   return XF86DRIGetDrawableInfo(dpy, psc->scr, glxDraw->drawable,
                                 index, stamp, X, Y, W, H,
                                 numClipRects, pClipRects,
                                 backX, backY,
                                 numBackClipRects, pBackClipRects);
}

static const __DRIgetDrawableInfoExtension getDrawableInfoExtension = {
   .base = {__DRI_GET_DRAWABLE_INFO, 1 },

   .getDrawableInfo     = __glXDRIGetDrawableInfo
};

static const __DRIextension *loader_extensions[] = {
   &systemTimeExtension.base,
   &getDrawableInfoExtension.base,
#ifdef XDAMAGE_1_1_INTERFACE
   &damageExtension.base,
#endif
   NULL
};

/**
 * Perform the required libGL-side initialization and call the client-side
 * driver's \c __driCreateNewScreen function.
 * 
 * \param dpy    Display pointer.
 * \param scrn   Screen number on the display.
 * \param psc    DRI screen information.
 * \param driDpy DRI display information.
 * \param createNewScreen  Pointer to the client-side driver's
 *               \c __driCreateNewScreen function.
 * \returns A pointer to the \c __DRIscreen structure returned by
 *          the client-side driver on success, or \c NULL on failure.
 */
static void *
CallCreateNewScreen(Display *dpy, int scrn, struct dri_screen *psc,
                    struct dri_display * driDpy)
{
   void *psp = NULL;
   drm_handle_t hSAREA;
   drmAddress pSAREA = MAP_FAILED;
   char *BusID;
   __DRIversion ddx_version;
   __DRIversion dri_version;
   __DRIversion drm_version;
   __DRIframebuffer framebuffer;
   int fd = -1;
   int status;

   drm_magic_t magic;
   drmVersionPtr version;
   int newlyopened;
   char *driverName;
   drm_handle_t hFB;
   int junk;
   const __DRIconfig **driver_configs;
   struct glx_config *visual, *configs = NULL, *visuals = NULL;

   /* DRI protocol version. */
   dri_version.major = driDpy->driMajor;
   dri_version.minor = driDpy->driMinor;
   dri_version.patch = driDpy->driPatch;

   framebuffer.base = MAP_FAILED;
   framebuffer.dev_priv = NULL;
   framebuffer.size = 0;

   if (!XF86DRIOpenConnection(dpy, scrn, &hSAREA, &BusID)) {
      ErrorMessageF("XF86DRIOpenConnection failed\n");
      goto handle_error;
   }

   fd = drmOpenOnce(NULL, BusID, &newlyopened);

   free(BusID);                /* No longer needed */

   if (fd < 0) {
      ErrorMessageF("drmOpenOnce failed (%s)\n", strerror(-fd));
      goto handle_error;
   }

   if (drmGetMagic(fd, &magic)) {
      ErrorMessageF("drmGetMagic failed\n");
      goto handle_error;
   }

   version = drmGetVersion(fd);
   if (version) {
      drm_version.major = version->version_major;
      drm_version.minor = version->version_minor;
      drm_version.patch = version->version_patchlevel;
      drmFreeVersion(version);
   }
   else {
      drm_version.major = -1;
      drm_version.minor = -1;
      drm_version.patch = -1;
   }

   if (newlyopened && !XF86DRIAuthConnection(dpy, scrn, magic)) {
      ErrorMessageF("XF86DRIAuthConnection failed\n");
      goto handle_error;
   }

   /* Get device name (like "radeon") and the ddx version numbers.
    * We'll check the version in each DRI driver's "createNewScreen"
    * function. */
   if (!XF86DRIGetClientDriverName(dpy, scrn,
                                   &ddx_version.major,
                                   &ddx_version.minor,
                                   &ddx_version.patch, &driverName)) {
      ErrorMessageF("XF86DRIGetClientDriverName failed\n");
      goto handle_error;
   }

   free(driverName);           /* No longer needed. */

   /*
    * Get device-specific info.  pDevPriv will point to a struct
    * (such as DRIRADEONRec in xfree86/driver/ati/radeon_dri.h) that
    * has information about the screen size, depth, pitch, ancilliary
    * buffers, DRM mmap handles, etc.
    */
   if (!XF86DRIGetDeviceInfo(dpy, scrn, &hFB, &junk,
                             &framebuffer.size, &framebuffer.stride,
                             &framebuffer.dev_priv_size,
                             &framebuffer.dev_priv)) {
      ErrorMessageF("XF86DRIGetDeviceInfo failed\n");
      goto handle_error;
   }

   framebuffer.width = DisplayWidth(dpy, scrn);
   framebuffer.height = DisplayHeight(dpy, scrn);

   /* Map the framebuffer region. */
   status = drmMap(fd, hFB, framebuffer.size,
                   (drmAddressPtr) & framebuffer.base);
   if (status != 0) {
      ErrorMessageF("drmMap of framebuffer failed (%s)\n", strerror(-status));
      goto handle_error;
   }

   /* Map the SAREA region.  Further mmap regions may be setup in
    * each DRI driver's "createNewScreen" function.
    */
   status = drmMap(fd, hSAREA, SAREA_MAX, &pSAREA);
   if (status != 0) {
      ErrorMessageF("drmMap of SAREA failed (%s)\n", strerror(-status));
      goto handle_error;
   }

   psp = (*psc->legacy->createNewScreen) (scrn,
                                          &ddx_version,
                                          &dri_version,
                                          &drm_version,
                                          &framebuffer,
                                          pSAREA,
                                          fd,
                                          loader_extensions,
                                          &driver_configs, psc);

   if (psp == NULL) {
      ErrorMessageF("Calling driver entry point failed\n");
      goto handle_error;
   }

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

   /* Visuals with depth != screen depth are subject to automatic compositing
    * in the X server, so DRI1 can't render to them properly. Mark them as
    * non-conformant to prevent apps from picking them up accidentally.
    */
   for (visual = psc->base.visuals; visual; visual = visual->next) {
      XVisualInfo templ;
      XVisualInfo *visuals;
      int num_visuals;
      long mask;

      templ.visualid = visual->visualID;
      mask = VisualIDMask;
      visuals = XGetVisualInfo(dpy, mask, &templ, &num_visuals);

      if (visuals) {
         if (num_visuals > 0 && visuals->depth != DefaultDepth(dpy, scrn))
            visual->visualRating = GLX_NON_CONFORMANT_CONFIG;

         free(visuals);
      }
   }

   return psp;

 handle_error:
   if (configs)
       glx_config_destroy_list(configs);
   if (visuals)
       glx_config_destroy_list(visuals);

   if (pSAREA != MAP_FAILED)
      drmUnmap(pSAREA, SAREA_MAX);

   if (framebuffer.base != MAP_FAILED)
      drmUnmap((drmAddress) framebuffer.base, framebuffer.size);

   free(framebuffer.dev_priv);

   if (fd >= 0)
      drmCloseOnce(fd);

   XF86DRICloseConnection(dpy, scrn);

   ErrorMessageF("reverting to software direct rendering\n");

   return NULL;
}

static void
dri_destroy_context(struct glx_context * context)
{
   struct dri_context *pcp = (struct dri_context *) context;
   struct dri_screen *psc = (struct dri_screen *) context->psc;

   driReleaseDrawables(&pcp->base);

   free((char *) context->extensions);

   (*psc->core->destroyContext) (pcp->driContext);

   XF86DRIDestroyContext(psc->base.dpy, psc->base.scr, pcp->hwContextID);
   free(pcp);
}

static int
dri_bind_context(struct glx_context *context, struct glx_context *old,
		 GLXDrawable draw, GLXDrawable read)
{
   struct dri_context *pcp = (struct dri_context *) context;
   struct dri_screen *psc = (struct dri_screen *) pcp->base.psc;
   struct dri_drawable *pdraw, *pread;

   pdraw = (struct dri_drawable *) driFetchDrawable(context, draw);
   pread = (struct dri_drawable *) driFetchDrawable(context, read);

   driReleaseDrawables(&pcp->base);

   if (pdraw == NULL || pread == NULL)
      return GLXBadDrawable;

   if ((*psc->core->bindContext) (pcp->driContext,
				  pdraw->driDrawable, pread->driDrawable))
      return Success;

   return GLXBadContext;
}

static void
dri_unbind_context(struct glx_context *context, struct glx_context *new)
{
   struct dri_context *pcp = (struct dri_context *) context;
   struct dri_screen *psc = (struct dri_screen *) pcp->base.psc;

   (*psc->core->unbindContext) (pcp->driContext);
}

static const struct glx_context_vtable dri_context_vtable = {
   .destroy             = dri_destroy_context,
   .bind                = dri_bind_context,
   .unbind              = dri_unbind_context,
   .wait_gl             = NULL,
   .wait_x              = NULL,
   .use_x_font          = DRI_glXUseXFont,
   .bind_tex_image      = NULL,
   .release_tex_image   = NULL,
   .get_proc_address    = NULL,
};

static struct glx_context *
dri_create_context(struct glx_screen *base,
		   struct glx_config *config_base,
		   struct glx_context *shareList, int renderType)
{
   struct dri_context *pcp, *pcp_shared;
   struct dri_screen *psc = (struct dri_screen *) base;
   drm_context_t hwContext;
   __DRIcontext *shared = NULL;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;

   if (!psc->base.driScreen)
      return NULL;

   /* Check the renderType value */
   if (!validate_renderType_against_config(config_base, renderType))
       return NULL;

   if (shareList) {
      /* If the shareList context is not a DRI context, we cannot possibly
       * create a DRI context that shares it.
       */
      if (shareList->vtable->destroy != dri_destroy_context) {
	 return NULL;
      }

      pcp_shared = (struct dri_context *) shareList;
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

   if (!XF86DRICreateContextWithConfig(psc->base.dpy, psc->base.scr,
                                       config->base.visualID,
                                       &pcp->hwContextID, &hwContext)) {
      free(pcp);
      return NULL;
   }

   pcp->driContext =
      (*psc->legacy->createNewContext) (psc->driScreen,
                                        config->driConfig,
                                        renderType, shared, hwContext, pcp);
   if (pcp->driContext == NULL) {
      XF86DRIDestroyContext(psc->base.dpy, psc->base.scr, pcp->hwContextID);
      free(pcp);
      return NULL;
   }

   pcp->base.vtable = &dri_context_vtable;

   return &pcp->base;
}

static void
driDestroyDrawable(__GLXDRIdrawable * pdraw)
{
   struct dri_screen *psc = (struct dri_screen *) pdraw->psc;
   struct dri_drawable *pdp = (struct dri_drawable *) pdraw;

   (*psc->core->destroyDrawable) (pdp->driDrawable);
   XF86DRIDestroyDrawable(psc->base.dpy, psc->base.scr, pdraw->drawable);
   free(pdraw);
}

static __GLXDRIdrawable *
driCreateDrawable(struct glx_screen *base,
                  XID xDrawable,
                  GLXDrawable drawable, struct glx_config *config_base)
{
   drm_drawable_t hwDrawable;
   void *empty_attribute_list = NULL;
   __GLXDRIconfigPrivate *config = (__GLXDRIconfigPrivate *) config_base;
   struct dri_screen *psc = (struct dri_screen *) base;
   struct dri_drawable *pdp;

   /* Old dri can't handle GLX 1.3+ drawable constructors. */
   if (xDrawable != drawable)
      return NULL;

   pdp = calloc(1, sizeof *pdp);
   if (!pdp)
      return NULL;

   pdp->base.drawable = drawable;
   pdp->base.psc = &psc->base;

   if (!XF86DRICreateDrawable(psc->base.dpy, psc->base.scr,
			      drawable, &hwDrawable)) {
      free(pdp);
      return NULL;
   }

   /* Create a new drawable */
   pdp->driDrawable =
      (*psc->legacy->createNewDrawable) (psc->driScreen,
                                         config->driConfig,
                                         hwDrawable,
                                         GLX_WINDOW_BIT,
                                         empty_attribute_list, pdp);

   if (!pdp->driDrawable) {
      XF86DRIDestroyDrawable(psc->base.dpy, psc->base.scr, drawable);
      free(pdp);
      return NULL;
   }

   pdp->base.destroyDrawable = driDestroyDrawable;

   return &pdp->base;
}

static int64_t
driSwapBuffers(__GLXDRIdrawable * pdraw, int64_t unused1, int64_t unused2,
	       int64_t unused3, Bool flush)
{
   struct dri_screen *psc = (struct dri_screen *) pdraw->psc;
   struct dri_drawable *pdp = (struct dri_drawable *) pdraw;

   if (flush) {
      glFlush();
   }

   (*psc->core->swapBuffers) (pdp->driDrawable);
   return 0;
}

static void
driCopySubBuffer(__GLXDRIdrawable * pdraw,
                 int x, int y, int width, int height, Bool flush)
{
   struct dri_drawable *pdp = (struct dri_drawable *) pdraw;
   struct dri_screen *psc = (struct dri_screen *) pdp->base.psc;

   if (flush) {
      glFlush();
   }

   (*psc->driCopySubBuffer->copySubBuffer) (pdp->driDrawable,
					    x, y, width, height);
}

static void
driDestroyScreen(struct glx_screen *base)
{
   struct dri_screen *psc = (struct dri_screen *) base;

   /* Free the direct rendering per screen data */
   if (psc->driScreen)
      (*psc->core->destroyScreen) (psc->driScreen);
   driDestroyConfigs(psc->driver_configs);
   psc->driScreen = NULL;
   if (psc->driver)
      dlclose(psc->driver);
}

static int
driSetSwapInterval(__GLXDRIdrawable *pdraw, int interval)
{
   struct dri_drawable *pdp = (struct dri_drawable *) pdraw;

   if (pdraw != NULL) {
      struct dri_screen *psc = (struct dri_screen *) pdraw->psc;

      if (psc->swapControl != NULL) {
         psc->swapControl->setSwapInterval(pdp->driDrawable, interval);
         return 0;
      }
   }
   return GLX_BAD_CONTEXT;
}

static int
driGetSwapInterval(__GLXDRIdrawable *pdraw)
{
   struct dri_drawable *pdp = (struct dri_drawable *) pdraw;

   if (pdraw != NULL) {
      struct dri_screen *psc = (struct dri_screen *) pdraw->psc;

      if (psc->swapControl != NULL)
         return psc->swapControl->getSwapInterval(pdp->driDrawable);
   }
   return 0;
}

/* Bind DRI1 specific extensions */
static void
driBindExtensions(struct dri_screen *psc, const __DRIextension **extensions)
{
   int i;

   for (i = 0; extensions[i]; i++) {
      /* No DRI2 support for swap_control at the moment, since SwapBuffers
       * is done by the X server */
      if (strcmp(extensions[i]->name, __DRI_SWAP_CONTROL) == 0) {
	 psc->swapControl = (__DRIswapControlExtension *) extensions[i];
	 __glXEnableDirectExtension(&psc->base, "GLX_SGI_swap_control");
	 __glXEnableDirectExtension(&psc->base, "GLX_MESA_swap_control");
      }

      if (strcmp(extensions[i]->name, __DRI_MEDIA_STREAM_COUNTER) == 0) {
         psc->msc = (__DRImediaStreamCounterExtension *) extensions[i];
         __glXEnableDirectExtension(&psc->base, "GLX_SGI_video_sync");
      }

      if (strcmp(extensions[i]->name, __DRI_COPY_SUB_BUFFER) == 0) {
	 psc->driCopySubBuffer = (__DRIcopySubBufferExtension *) extensions[i];
	 __glXEnableDirectExtension(&psc->base, "GLX_MESA_copy_sub_buffer");
      }

      if (strcmp(extensions[i]->name, __DRI_READ_DRAWABLE) == 0) {
	 __glXEnableDirectExtension(&psc->base, "GLX_SGI_make_current_read");
      }
      /* Ignore unknown extensions */
   }
}

static const struct glx_screen_vtable dri_screen_vtable = {
   .create_context         = dri_create_context,
   .create_context_attribs = NULL,
   .query_renderer_integer = NULL,
   .query_renderer_string  = NULL,
};

static struct glx_screen *
driCreateScreen(int screen, struct glx_display *priv)
{
   struct dri_display *pdp;
   __GLXDRIscreen *psp;
   const __DRIextension **extensions;
   struct dri_screen *psc;
   char *driverName;
   int i;

   psc = calloc(1, sizeof *psc);
   if (psc == NULL)
      return NULL;

   if (!glx_screen_init(&psc->base, screen, priv)) {
      free(psc);
      return NULL;
   }

   if (!driGetDriverName(priv->dpy, screen, &driverName)) {
      goto cleanup;
   }

   psc->driver = driOpenDriver(driverName);
   if (psc->driver == NULL)
      goto cleanup;

   extensions = dlsym(psc->driver, __DRI_DRIVER_EXTENSIONS);
   if (extensions == NULL) {
      ErrorMessageF("driver exports no extensions (%s)\n", dlerror());
      goto cleanup;
   }

   for (i = 0; extensions[i]; i++) {
      if (strcmp(extensions[i]->name, __DRI_CORE) == 0)
	 psc->core = (__DRIcoreExtension *) extensions[i];
      if (strcmp(extensions[i]->name, __DRI_LEGACY) == 0)
	 psc->legacy = (__DRIlegacyExtension *) extensions[i];
   }

   if (psc->core == NULL || psc->legacy == NULL)
      goto cleanup;

   pdp = (struct dri_display *) priv->driDisplay;
   psc->driScreen =
      CallCreateNewScreen(psc->base.dpy, screen, psc, pdp);
   if (psc->driScreen == NULL)
      goto cleanup;

   extensions = psc->core->getExtensions(psc->driScreen);
   driBindExtensions(psc, extensions);

   psc->base.vtable = &dri_screen_vtable;
   psp = &psc->vtable;
   psc->base.driScreen = psp;
   if (psc->driCopySubBuffer)
      psp->copySubBuffer = driCopySubBuffer;

   psp->destroyScreen = driDestroyScreen;
   psp->createDrawable = driCreateDrawable;
   psp->swapBuffers = driSwapBuffers;

   psp->setSwapInterval = driSetSwapInterval;
   psp->getSwapInterval = driGetSwapInterval;

   free(driverName);

   return &psc->base;

cleanup:
   CriticalErrorMessageF("failed to load driver: %s\n", driverName);

   free(driverName);

   if (psc->driver)
      dlclose(psc->driver);
   glx_screen_cleanup(&psc->base);
   free(psc);

   return NULL;
}

/* Called from __glXFreeDisplayPrivate.
 */
static void
driDestroyDisplay(__GLXDRIdisplay * dpy)
{
   free(dpy);
}

/*
 * Allocate, initialize and return a __DRIdisplayPrivate object.
 * This is called from __glXInitialize() when we are given a new
 * display pointer.
 */
_X_HIDDEN __GLXDRIdisplay *
driCreateDisplay(Display * dpy)
{
   struct dri_display *pdpyp;
   int eventBase, errorBase;
   int major, minor, patch;

   if (!XF86DRIQueryExtension(dpy, &eventBase, &errorBase)) {
      return NULL;
   }

   if (!XF86DRIQueryVersion(dpy, &major, &minor, &patch)) {
      return NULL;
   }

   pdpyp = malloc(sizeof *pdpyp);
   if (!pdpyp) {
      return NULL;
   }

   pdpyp->driMajor = major;
   pdpyp->driMinor = minor;
   pdpyp->driPatch = patch;

   pdpyp->base.destroyDisplay = driDestroyDisplay;
   pdpyp->base.createScreen = driCreateScreen;

   return &pdpyp->base;
}

#endif /* GLX_DIRECT_RENDERING */
