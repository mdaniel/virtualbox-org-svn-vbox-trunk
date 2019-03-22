/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2011 Morgan Armand <morgan.devel@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <windows.h>

#define WGL_WGLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/wglext.h>

#include "stw_icd.h"
#include "stw_context.h"
#include "stw_device.h"
#include "stw_ext_context.h"

#include "util/u_debug.h"


wglCreateContext_t wglCreateContext_func = 0;
wglDeleteContext_t wglDeleteContext_func = 0;


/**
 * The implementation of this function is tricky.  The OPENGL32.DLL library
 * remaps the context IDs returned by our stw_create_context_attribs()
 * function to different values returned to the caller of wglCreateContext().
 * That is, DHGLRC (driver) handles are not equivalent to HGLRC (public)
 * handles.
 *
 * So we need to generate a new HGLRC ID here.  We do that by calling
 * the regular wglCreateContext() function.  Then, we replace the newly-
 * created stw_context with a new stw_context that reflects the arguments
 * to this function.
 */
HGLRC WINAPI
wglCreateContextAttribsARB(HDC hDC, HGLRC hShareContext, const int *attribList)
{
   HGLRC context;

   int majorVersion = 1, minorVersion = 0, layerPlane = 0;
   int contextFlags = 0x0;
   int profileMask = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
   int i;
   BOOL done = FALSE;
   const int contextFlagsAll = (WGL_CONTEXT_DEBUG_BIT_ARB |
                                WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB);

   /* parse attrib_list */
   if (attribList) {
      for (i = 0; !done && attribList[i]; i++) {
         switch (attribList[i]) {
         case WGL_CONTEXT_MAJOR_VERSION_ARB:
            majorVersion = attribList[++i];
            break;
         case WGL_CONTEXT_MINOR_VERSION_ARB:
            minorVersion = attribList[++i];
            break;
         case WGL_CONTEXT_LAYER_PLANE_ARB:
            layerPlane = attribList[++i];
            break;
         case WGL_CONTEXT_FLAGS_ARB:
            contextFlags = attribList[++i];
            break;
         case WGL_CONTEXT_PROFILE_MASK_ARB:
            profileMask = attribList[++i];
            break;
         case 0:
            /* end of list */
            done = TRUE;
            break;
         default:
            /* bad attribute */
            SetLastError(ERROR_INVALID_PARAMETER);
            return 0;
         }
      }
   }

   /* check contextFlags */
   if (contextFlags & ~contextFlagsAll) {
      SetLastError(ERROR_INVALID_PARAMETER);
      return NULL;
   }

   /* check profileMask */
   if (profileMask != WGL_CONTEXT_CORE_PROFILE_BIT_ARB &&
       profileMask != WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB &&
       profileMask != WGL_CONTEXT_ES_PROFILE_BIT_EXT) {
      SetLastError(ERROR_INVALID_PROFILE_ARB);
      return NULL;
   }

   /* check version (generate ERROR_INVALID_VERSION_ARB if bad) */
   if (majorVersion <= 0 ||
       minorVersion < 0 ||
       (profileMask != WGL_CONTEXT_ES_PROFILE_BIT_EXT &&
        ((majorVersion == 1 && minorVersion > 5) ||
         (majorVersion == 2 && minorVersion > 1) ||
         (majorVersion == 3 && minorVersion > 3) ||
         (majorVersion == 4 && minorVersion > 5) ||
         majorVersion > 4)) ||
       (profileMask == WGL_CONTEXT_ES_PROFILE_BIT_EXT &&
        ((majorVersion == 1 && minorVersion > 1) ||
         (majorVersion == 2 && minorVersion > 0) ||
         (majorVersion == 3 && minorVersion > 1) ||
         majorVersion > 3))) {
      SetLastError(ERROR_INVALID_VERSION_ARB);
      return NULL;
   }

   if ((contextFlags & WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB) &&
       majorVersion < 3) {
      SetLastError(ERROR_INVALID_VERSION_ARB);
      return 0;
   }

   /* Get pointer to OPENGL32.DLL's wglCreate/DeleteContext() functions */
   if (!wglCreateContext_func || !wglDeleteContext_func) {
      /* Get the OPENGL32.DLL library */
      HMODULE opengl_lib = GetModuleHandleA("opengl32.dll");
      if (!opengl_lib) {
         _debug_printf("wgl: GetModuleHandleA(\"opengl32.dll\") failed\n");
         return 0;
      }

      /* Get pointer to wglCreateContext() function */
      wglCreateContext_func = (wglCreateContext_t)
         GetProcAddress(opengl_lib, "wglCreateContext");
      if (!wglCreateContext_func) {
         _debug_printf("wgl: failed to get wglCreateContext()\n");
         return 0;
      }

      /* Get pointer to wglDeleteContext() function */
      wglDeleteContext_func = (wglDeleteContext_t)
         GetProcAddress(opengl_lib, "wglDeleteContext");
      if (!wglDeleteContext_func) {
         _debug_printf("wgl: failed to get wglDeleteContext()\n");
         return 0;
      }
   }

   /* Call wglCreateContext to get a valid context ID */
   context = wglCreateContext_func(hDC);

   if (context) {
      /* Now replace the context we just created with a new one that reflects
       * the attributes passed to this function.
       */
      DHGLRC dhglrc, c, share_dhglrc = 0;

      /* Convert public HGLRC to driver DHGLRC */
      if (stw_dev && stw_dev->callbacks.wglCbGetDhglrc) {
         dhglrc = stw_dev->callbacks.wglCbGetDhglrc(context);
         if (hShareContext)
            share_dhglrc = stw_dev->callbacks.wglCbGetDhglrc(hShareContext);
      }
      else {
         /* not using ICD */
         dhglrc = (DHGLRC) context;
         share_dhglrc = (DHGLRC) hShareContext;
      }

      c = stw_create_context_attribs(hDC, layerPlane, share_dhglrc,
                                     majorVersion, minorVersion,
                                     contextFlags, profileMask,
                                     dhglrc);
      if (!c) {
         wglDeleteContext_func(context);
         context = 0;
      }
   }

   return context;
}


/** Defined by WGL_ARB_make_current_read */
BOOL APIENTRY
wglMakeContextCurrentARB(HDC hDrawDC, HDC hReadDC, HGLRC hglrc)
{
   DHGLRC dhglrc = 0;

   if (stw_dev && stw_dev->callbacks.wglCbGetDhglrc) {
      /* Convert HGLRC to DHGLRC */
      dhglrc = stw_dev->callbacks.wglCbGetDhglrc(hglrc);
   }

   return stw_make_current(hDrawDC, hReadDC, dhglrc);
}
