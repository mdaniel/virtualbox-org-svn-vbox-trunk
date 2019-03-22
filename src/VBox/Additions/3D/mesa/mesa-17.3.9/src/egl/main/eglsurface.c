/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * Copyright 2009-2010 Chia-I Wu <olvaffe@gmail.com>
 * Copyright 2010 LunarG, Inc.
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


/**
 * Surface-related functions.
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "egldisplay.h"
#include "egldriver.h"
#include "eglcontext.h"
#include "eglconfig.h"
#include "eglcurrent.h"
#include "egllog.h"
#include "eglsurface.h"


/**
 * Parse the list of surface attributes and return the proper error code.
 */
static EGLint
_eglParseSurfaceAttribList(_EGLSurface *surf, const EGLint *attrib_list)
{
   _EGLDisplay *dpy = surf->Resource.Display;
   EGLint type = surf->Type;
   EGLint texture_type = EGL_PBUFFER_BIT;
   EGLint i, err = EGL_SUCCESS;
   EGLint attr = EGL_NONE;
   EGLint val = EGL_NONE;

   if (!attrib_list)
      return EGL_SUCCESS;

   if (dpy->Extensions.NOK_texture_from_pixmap)
      texture_type |= EGL_PIXMAP_BIT;

   for (i = 0; attrib_list[i] != EGL_NONE; i++) {
      attr = attrib_list[i++];
      val = attrib_list[i];

      switch (attr) {
      /* common attributes */
      case EGL_GL_COLORSPACE_KHR:
         if (!dpy->Extensions.KHR_gl_colorspace) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         switch (val) {
         case EGL_GL_COLORSPACE_SRGB_KHR:
         case EGL_GL_COLORSPACE_LINEAR_KHR:
            break;
         default:
            err = EGL_BAD_ATTRIBUTE;
         }
         if (err != EGL_SUCCESS)
            break;
         surf->GLColorspace = val;
         break;
      case EGL_VG_COLORSPACE:
         switch (val) {
         case EGL_VG_COLORSPACE_sRGB:
         case EGL_VG_COLORSPACE_LINEAR:
            break;
         default:
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (err != EGL_SUCCESS)
            break;
         surf->VGColorspace = val;
         break;
      case EGL_VG_ALPHA_FORMAT:
         switch (val) {
         case EGL_VG_ALPHA_FORMAT_NONPRE:
         case EGL_VG_ALPHA_FORMAT_PRE:
            break;
         default:
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (err != EGL_SUCCESS)
            break;
         surf->VGAlphaFormat = val;
         break;
      /* window surface attributes */
      case EGL_RENDER_BUFFER:
         if (type != EGL_WINDOW_BIT) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (val != EGL_BACK_BUFFER && val != EGL_SINGLE_BUFFER) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         surf->RenderBuffer = val;
         break;
      case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
         if (!dpy->Extensions.NV_post_sub_buffer ||
             type != EGL_WINDOW_BIT) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (val != EGL_TRUE && val != EGL_FALSE) {
            err = EGL_BAD_PARAMETER;
            break;
         }
         surf->PostSubBufferSupportedNV = val;
         break;
      /* pbuffer surface attributes */
      case EGL_WIDTH:
         if (type != EGL_PBUFFER_BIT) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (val < 0) {
            err = EGL_BAD_PARAMETER;
            break;
         }
         surf->Width = val;
         break;
      case EGL_HEIGHT:
         if (type != EGL_PBUFFER_BIT) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (val < 0) {
            err = EGL_BAD_PARAMETER;
            break;
         }
         surf->Height = val;
         break;
      case EGL_LARGEST_PBUFFER:
         if (type != EGL_PBUFFER_BIT) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         surf->LargestPbuffer = !!val;
         break;
      /* for eglBindTexImage */
      case EGL_TEXTURE_FORMAT:
         if (!(type & texture_type)) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }

         switch (val) {
         case EGL_TEXTURE_RGB:
         case EGL_TEXTURE_RGBA:
         case EGL_NO_TEXTURE:
            break;
         default:
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (err != EGL_SUCCESS)
            break;
         surf->TextureFormat = val;
         break;
      case EGL_TEXTURE_TARGET:
         if (!(type & texture_type)) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }

         switch (val) {
         case EGL_TEXTURE_2D:
         case EGL_NO_TEXTURE:
            break;
         default:
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         if (err != EGL_SUCCESS)
            break;
         surf->TextureTarget = val;
         break;
      case EGL_MIPMAP_TEXTURE:
         if (!(type & texture_type)) {
            err = EGL_BAD_ATTRIBUTE;
            break;
         }
         surf->MipmapTexture = !!val;
         break;
      /* no pixmap surface specific attributes */
      default:
         err = EGL_BAD_ATTRIBUTE;
         break;
      }

      if (err != EGL_SUCCESS)
         break;
   }

   if (err == EGL_SUCCESS && type == EGL_PBUFFER_BIT) {
      if ((surf->TextureTarget == EGL_NO_TEXTURE && surf->TextureFormat != EGL_NO_TEXTURE) ||
          (surf->TextureFormat == EGL_NO_TEXTURE && surf->TextureTarget != EGL_NO_TEXTURE)) {
         attr = surf->TextureTarget == EGL_NO_TEXTURE ? EGL_TEXTURE_TARGET : EGL_TEXTURE_FORMAT;
         err = EGL_BAD_MATCH;
      }
   }

   if (err != EGL_SUCCESS)
      _eglLog(_EGL_WARNING, "bad surface attribute 0x%04x", attr);

   return err;
}


/**
 * Do error check on parameters and initialize the given _EGLSurface object.
 * \return EGL_TRUE if no errors, EGL_FALSE otherwise.
 */
EGLBoolean
_eglInitSurface(_EGLSurface *surf, _EGLDisplay *dpy, EGLint type,
                _EGLConfig *conf, const EGLint *attrib_list)
{
   const char *func;
   EGLint renderBuffer = EGL_BACK_BUFFER;
   EGLint swapBehavior = EGL_BUFFER_DESTROYED;
   EGLint err;

   /* Swap behavior can be preserved only if config supports this. */
   if (conf->SurfaceType & EGL_SWAP_BEHAVIOR_PRESERVED_BIT)
      swapBehavior = EGL_BUFFER_PRESERVED;

   switch (type) {
   case EGL_WINDOW_BIT:
      func = "eglCreateWindowSurface";
      swapBehavior = EGL_BUFFER_DESTROYED;
      break;
   case EGL_PIXMAP_BIT:
      func = "eglCreatePixmapSurface";
      renderBuffer = EGL_SINGLE_BUFFER;
      break;
   case EGL_PBUFFER_BIT:
      func = "eglCreatePBufferSurface";
      break;
   default:
      _eglLog(_EGL_WARNING, "Bad type in _eglInitSurface");
      return EGL_FALSE;
   }

   if ((conf->SurfaceType & type) == 0)
      /* The config can't be used to create a surface of this type */
      return _eglError(EGL_BAD_MATCH, func);

   _eglInitResource(&surf->Resource, sizeof(*surf), dpy);
   surf->Type = type;
   surf->Config = conf;
   surf->Lost = EGL_FALSE;

   surf->Width = 0;
   surf->Height = 0;
   surf->TextureFormat = EGL_NO_TEXTURE;
   surf->TextureTarget = EGL_NO_TEXTURE;
   surf->MipmapTexture = EGL_FALSE;
   surf->LargestPbuffer = EGL_FALSE;
   surf->RenderBuffer = renderBuffer;
   surf->VGAlphaFormat = EGL_VG_ALPHA_FORMAT_NONPRE;
   surf->VGColorspace = EGL_VG_COLORSPACE_sRGB;
   surf->GLColorspace = EGL_GL_COLORSPACE_LINEAR_KHR;

   surf->MipmapLevel = 0;
   surf->MultisampleResolve = EGL_MULTISAMPLE_RESOLVE_DEFAULT;
   surf->SwapBehavior = swapBehavior;

   surf->HorizontalResolution = EGL_UNKNOWN;
   surf->VerticalResolution = EGL_UNKNOWN;
   surf->AspectRatio = EGL_UNKNOWN;

   surf->PostSubBufferSupportedNV = EGL_FALSE;
   surf->SetDamageRegionCalled = EGL_FALSE;
   surf->BufferAgeRead = EGL_FALSE;

   /* the default swap interval is 1 */
   surf->SwapInterval = 1;

   err = _eglParseSurfaceAttribList(surf, attrib_list);
   if (err != EGL_SUCCESS)
      return _eglError(err, func);

   /* if EGL_LARGEST_PBUFFER in use, clamp width and height */
   if (surf->LargestPbuffer) {
      surf->Width = MIN2(surf->Width, _EGL_MAX_PBUFFER_WIDTH);
      surf->Height = MIN2(surf->Height, _EGL_MAX_PBUFFER_HEIGHT);
   }

   return EGL_TRUE;
}


EGLBoolean
_eglQuerySurface(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surface,
                 EGLint attribute, EGLint *value)
{
   switch (attribute) {
   case EGL_WIDTH:
      *value = surface->Width;
      break;
   case EGL_HEIGHT:
      *value = surface->Height;
      break;
   case EGL_CONFIG_ID:
      *value = surface->Config->ConfigID;
      break;
   case EGL_LARGEST_PBUFFER:
      if (surface->Type == EGL_PBUFFER_BIT)
         *value = surface->LargestPbuffer;
      break;
   case EGL_TEXTURE_FORMAT:
      /* texture attributes: only for pbuffers, no error otherwise */
      if (surface->Type == EGL_PBUFFER_BIT)
         *value = surface->TextureFormat;
      break;
   case EGL_TEXTURE_TARGET:
      if (surface->Type == EGL_PBUFFER_BIT)
         *value = surface->TextureTarget;
      break;
   case EGL_MIPMAP_TEXTURE:
      if (surface->Type == EGL_PBUFFER_BIT)
         *value = surface->MipmapTexture;
      break;
   case EGL_MIPMAP_LEVEL:
      if (surface->Type == EGL_PBUFFER_BIT)
         *value = surface->MipmapLevel;
      break;
   case EGL_SWAP_BEHAVIOR:
      *value = surface->SwapBehavior;
      break;
   case EGL_RENDER_BUFFER:
      *value = surface->RenderBuffer;
      break;
   case EGL_PIXEL_ASPECT_RATIO:
      *value = surface->AspectRatio;
      break;
   case EGL_HORIZONTAL_RESOLUTION:
      *value = surface->HorizontalResolution;
      break;
   case EGL_VERTICAL_RESOLUTION:
      *value = surface->VerticalResolution;
      break;
   case EGL_MULTISAMPLE_RESOLVE:
      *value = surface->MultisampleResolve;
      break;
   case EGL_VG_ALPHA_FORMAT:
      *value = surface->VGAlphaFormat;
      break;
   case EGL_VG_COLORSPACE:
      *value = surface->VGColorspace;
      break;
   case EGL_GL_COLORSPACE_KHR:
      if (!dpy->Extensions.KHR_gl_colorspace)
         return _eglError(EGL_BAD_ATTRIBUTE, "eglQuerySurface");

      *value = surface->GLColorspace;
      break;
   case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
      *value = surface->PostSubBufferSupportedNV;
      break;
   case EGL_BUFFER_AGE_EXT:
      if (!dpy->Extensions.EXT_buffer_age)
         return _eglError(EGL_BAD_ATTRIBUTE, "eglQuerySurface");

      _EGLContext *ctx = _eglGetCurrentContext();
      EGLint result = drv->API.QueryBufferAge(drv, dpy, surface);
      /* error happened */
      if (result < 0)
         return EGL_FALSE;
      if (_eglGetContextHandle(ctx) == EGL_NO_CONTEXT ||
          ctx->DrawSurface != surface)
         return _eglError(EGL_BAD_SURFACE, "eglQuerySurface");

      *value = result;
      surface->BufferAgeRead = EGL_TRUE;
      break;
   default:
      return _eglError(EGL_BAD_ATTRIBUTE, "eglQuerySurface");
   }

   return EGL_TRUE;
}


/**
 * Default fallback routine - drivers might override this.
 */
EGLBoolean
_eglSurfaceAttrib(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surface,
                  EGLint attribute, EGLint value)
{
   EGLint confval;
   EGLint err = EGL_SUCCESS;
   EGLint all_es_bits = EGL_OPENGL_ES_BIT |
                        EGL_OPENGL_ES2_BIT |
                        EGL_OPENGL_ES3_BIT_KHR;

   switch (attribute) {
   case EGL_MIPMAP_LEVEL:
      confval = surface->Config->RenderableType;
      if (!(confval & all_es_bits)) {
         err = EGL_BAD_PARAMETER;
         break;
      }
      surface->MipmapLevel = value;
      break;
   case EGL_MULTISAMPLE_RESOLVE:
      switch (value) {
      case EGL_MULTISAMPLE_RESOLVE_DEFAULT:
         break;
      case EGL_MULTISAMPLE_RESOLVE_BOX:
         confval = surface->Config->SurfaceType;
         if (!(confval & EGL_MULTISAMPLE_RESOLVE_BOX_BIT))
            err = EGL_BAD_MATCH;
         break;
      default:
         err = EGL_BAD_ATTRIBUTE;
         break;
      }
      if (err != EGL_SUCCESS)
         break;
      surface->MultisampleResolve = value;
      break;
   case EGL_SWAP_BEHAVIOR:
      switch (value) {
      case EGL_BUFFER_DESTROYED:
         break;
      case EGL_BUFFER_PRESERVED:
         confval = surface->Config->SurfaceType;
         if (!(confval & EGL_SWAP_BEHAVIOR_PRESERVED_BIT))
            err = EGL_BAD_MATCH;
         break;
      default:
         err = EGL_BAD_ATTRIBUTE;
         break;
      }
      if (err != EGL_SUCCESS)
         break;
      surface->SwapBehavior = value;
      break;
   default:
      err = EGL_BAD_ATTRIBUTE;
      break;
   }

   if (err != EGL_SUCCESS)
      return _eglError(err, "eglSurfaceAttrib");
   return EGL_TRUE;
}


EGLBoolean
_eglBindTexImage(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surface,
                 EGLint buffer)
{
   EGLint texture_type = EGL_PBUFFER_BIT;

   /* Just do basic error checking and return success/fail.
    * Drivers must implement the real stuff.
    */

   if (dpy->Extensions.NOK_texture_from_pixmap)
      texture_type |= EGL_PIXMAP_BIT;

   if (!(surface->Type & texture_type))
      return _eglError(EGL_BAD_SURFACE, "eglBindTexImage");

   if (surface->TextureFormat == EGL_NO_TEXTURE)
      return _eglError(EGL_BAD_MATCH, "eglBindTexImage");

   if (surface->TextureTarget == EGL_NO_TEXTURE)
      return _eglError(EGL_BAD_MATCH, "eglBindTexImage");

   if (buffer != EGL_BACK_BUFFER)
      return _eglError(EGL_BAD_PARAMETER, "eglBindTexImage");

   surface->BoundToTexture = EGL_TRUE;

   return EGL_TRUE;
}

EGLBoolean
_eglReleaseTexImage(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surf,
                    EGLint buffer)
{
   /* Just do basic error checking and return success/fail.
    * Drivers must implement the real stuff.
    */

   EGLint texture_type = EGL_PBUFFER_BIT;

   if (surf == EGL_NO_SURFACE)
      return _eglError(EGL_BAD_SURFACE, "eglReleaseTexImage");

   if (!surf->BoundToTexture)
   {
      /* Not an error, simply nothing to do */
      return EGL_TRUE;
   }

   if (surf->TextureFormat == EGL_NO_TEXTURE)
      return _eglError(EGL_BAD_MATCH, "eglReleaseTexImage");

   if (buffer != EGL_BACK_BUFFER)
      return _eglError(EGL_BAD_PARAMETER, "eglReleaseTexImage");

   if (dpy->Extensions.NOK_texture_from_pixmap)
      texture_type |= EGL_PIXMAP_BIT;

   if (!(surf->Type & texture_type))
      return _eglError(EGL_BAD_SURFACE, "eglReleaseTexImage");

   surf->BoundToTexture = EGL_FALSE;

   return EGL_TRUE;
}


EGLBoolean
_eglSwapInterval(_EGLDriver *drv, _EGLDisplay *dpy, _EGLSurface *surf,
                 EGLint interval)
{
   return EGL_TRUE;
}
