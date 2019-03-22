/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * Copyright 2009-2010 Chia-I Wu <olvaffe@gmail.com>
 * Copyright 2010-2011 LunarG, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#ifndef EGLDISPLAY_INCLUDED
#define EGLDISPLAY_INCLUDED

#include "c99_compat.h"
#include "c11/threads.h"

#include "egltypedefs.h"
#include "egldefines.h"
#include "eglarray.h"


#ifdef __cplusplus
extern "C" {
#endif

enum _egl_platform_type {
   _EGL_PLATFORM_X11,
   _EGL_PLATFORM_WAYLAND,
   _EGL_PLATFORM_DRM,
   _EGL_PLATFORM_ANDROID,
   _EGL_PLATFORM_HAIKU,
   _EGL_PLATFORM_SURFACELESS,

   _EGL_NUM_PLATFORMS,
   _EGL_INVALID_PLATFORM = -1
};
typedef enum _egl_platform_type _EGLPlatformType;


enum _egl_resource_type {
   _EGL_RESOURCE_CONTEXT,
   _EGL_RESOURCE_SURFACE,
   _EGL_RESOURCE_IMAGE,
   _EGL_RESOURCE_SYNC,

   _EGL_NUM_RESOURCES
};
/* this cannot and need not go into egltypedefs.h */
typedef enum _egl_resource_type _EGLResourceType;


/**
 * A resource of a display.
 */
struct _egl_resource
{
   /* which display the resource belongs to */
   _EGLDisplay *Display;
   EGLBoolean IsLinked;
   EGLint RefCount;

   EGLLabelKHR Label;

   /* used to link resources of the same type */
   _EGLResource *Next;
};


/**
 * Optional EGL extensions info.
 */
struct _egl_extensions
{
   /* Please keep these sorted alphabetically. */
   EGLBoolean ANDROID_framebuffer_target;
   EGLBoolean ANDROID_image_native_buffer;
   EGLBoolean ANDROID_native_fence_sync;
   EGLBoolean ANDROID_recordable;

   EGLBoolean CHROMIUM_sync_control;

   EGLBoolean EXT_buffer_age;
   EGLBoolean EXT_create_context_robustness;
   EGLBoolean EXT_image_dma_buf_import;
   EGLBoolean EXT_image_dma_buf_import_modifiers;
   EGLBoolean EXT_swap_buffers_with_damage;

   unsigned int IMG_context_priority;
#define  __EGL_CONTEXT_PRIORITY_LOW_BIT    0
#define  __EGL_CONTEXT_PRIORITY_MEDIUM_BIT 1
#define  __EGL_CONTEXT_PRIORITY_HIGH_BIT   2

   EGLBoolean KHR_cl_event2;
   EGLBoolean KHR_config_attribs;
   EGLBoolean KHR_create_context;
   EGLBoolean KHR_fence_sync;
   EGLBoolean KHR_get_all_proc_addresses;
   EGLBoolean KHR_gl_colorspace;
   EGLBoolean KHR_gl_renderbuffer_image;
   EGLBoolean KHR_gl_texture_2D_image;
   EGLBoolean KHR_gl_texture_3D_image;
   EGLBoolean KHR_gl_texture_cubemap_image;
   EGLBoolean KHR_image_base;
   EGLBoolean KHR_image_pixmap;
   EGLBoolean KHR_no_config_context;
   EGLBoolean KHR_partial_update;
   EGLBoolean KHR_reusable_sync;
   EGLBoolean KHR_surfaceless_context;
   EGLBoolean KHR_wait_sync;
   EGLBoolean KHR_create_context_no_error;

   EGLBoolean MESA_drm_image;
   EGLBoolean MESA_image_dma_buf_export;

   EGLBoolean NOK_swap_region;
   EGLBoolean NOK_texture_from_pixmap;

   EGLBoolean NV_post_sub_buffer;

   EGLBoolean WL_bind_wayland_display;
   EGLBoolean WL_create_wayland_buffer_from_image;
};


struct _egl_display
{
   /* used to link displays */
   _EGLDisplay *Next;

   mtx_t Mutex;

   _EGLPlatformType Platform; /**< The type of the platform display */
   void *PlatformDisplay;     /**< A pointer to the platform display */

   _EGLDriver *Driver;        /**< Matched driver of the display */
   EGLBoolean Initialized;    /**< True if the display is initialized */

   /* options that affect how the driver initializes the display */
   struct {
      EGLBoolean UseFallback; /**< Use fallback driver (sw or less features) */
      void *Platform;         /**< Platform-specific options */
   } Options;

   /* these fields are set by the driver during init */
   void *DriverData;          /**< Driver private data */
   EGLint Version;            /**< EGL version major*10+minor */
   EGLint ClientAPIs;         /**< Bitmask of APIs supported (EGL_xxx_BIT) */
   _EGLExtensions Extensions; /**< Extensions supported */

   /* these fields are derived from above */
   char VersionString[100];                        /**< EGL_VERSION */
   char ClientAPIsString[100];                     /**< EGL_CLIENT_APIS */
   char ExtensionsString[_EGL_MAX_EXTENSIONS_LEN]; /**< EGL_EXTENSIONS */

   _EGLArray *Configs;

   /* lists of resources */
   _EGLResource *ResourceLists[_EGL_NUM_RESOURCES];

   EGLLabelKHR Label;
};


extern _EGLPlatformType
_eglGetNativePlatform(void *nativeDisplay);


extern void
_eglFiniDisplay(void);


extern _EGLDisplay *
_eglFindDisplay(_EGLPlatformType plat, void *plat_dpy);


extern void
_eglReleaseDisplayResources(_EGLDriver *drv, _EGLDisplay *dpy);


extern void
_eglCleanupDisplay(_EGLDisplay *disp);


extern EGLBoolean
_eglCheckDisplayHandle(EGLDisplay dpy);


extern EGLBoolean
_eglCheckResource(void *res, _EGLResourceType type, _EGLDisplay *dpy);


/**
 * Lookup a handle to find the linked display.
 * Return NULL if the handle has no corresponding linked display.
 */
static inline _EGLDisplay *
_eglLookupDisplay(EGLDisplay display)
{
   _EGLDisplay *dpy = (_EGLDisplay *) display;
   if (!_eglCheckDisplayHandle(display))
      dpy = NULL;
   return dpy;
}


/**
 * Return the handle of a linked display, or EGL_NO_DISPLAY.
 */
static inline EGLDisplay
_eglGetDisplayHandle(_EGLDisplay *dpy)
{
   return (EGLDisplay) ((dpy) ? dpy : EGL_NO_DISPLAY);
}


extern void
_eglInitResource(_EGLResource *res, EGLint size, _EGLDisplay *dpy);


extern void
_eglGetResource(_EGLResource *res);


extern EGLBoolean
_eglPutResource(_EGLResource *res);


extern void
_eglLinkResource(_EGLResource *res, _EGLResourceType type);


extern void
_eglUnlinkResource(_EGLResource *res, _EGLResourceType type);


/**
 * Return true if the resource is linked.
 */
static inline EGLBoolean
_eglIsResourceLinked(_EGLResource *res)
{
   return res->IsLinked;
}

#ifdef HAVE_X11_PLATFORM
_EGLDisplay*
_eglGetX11Display(Display *native_display, const EGLint *attrib_list);
#endif

#ifdef HAVE_DRM_PLATFORM
struct gbm_device;

_EGLDisplay*
_eglGetGbmDisplay(struct gbm_device *native_display,
                  const EGLint *attrib_list);
#endif

#ifdef HAVE_WAYLAND_PLATFORM
struct wl_display;

_EGLDisplay*
_eglGetWaylandDisplay(struct wl_display *native_display,
                      const EGLint *attrib_list);
#endif

#ifdef HAVE_SURFACELESS_PLATFORM
_EGLDisplay*
_eglGetSurfacelessDisplay(void *native_display,
                          const EGLint *attrib_list);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EGLDISPLAY_INCLUDED */
