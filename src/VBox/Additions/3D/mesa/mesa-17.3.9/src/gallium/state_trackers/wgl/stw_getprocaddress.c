/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <windows.h>

#define WGL_WGLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/wglext.h>

#include "glapi/glapi.h"
#include "stw_device.h"
#include "stw_icd.h"
#include "stw_nopfuncs.h"

#include "util/u_debug.h"

struct stw_extension_entry
{
   const char *name;
   PROC proc;
};

#define STW_EXTENSION_ENTRY(P) { #P, (PROC) P }

static const struct stw_extension_entry stw_extension_entries[] = {

   /* WGL_ARB_extensions_string */
   STW_EXTENSION_ENTRY( wglGetExtensionsStringARB ),

   /* WGL_ARB_pbuffer */
   STW_EXTENSION_ENTRY( wglCreatePbufferARB ),
   STW_EXTENSION_ENTRY( wglGetPbufferDCARB ),
   STW_EXTENSION_ENTRY( wglReleasePbufferDCARB ),
   STW_EXTENSION_ENTRY( wglDestroyPbufferARB ),
   STW_EXTENSION_ENTRY( wglQueryPbufferARB ),

   /* WGL_ARB_pixel_format */
   STW_EXTENSION_ENTRY( wglChoosePixelFormatARB ),
   STW_EXTENSION_ENTRY( wglGetPixelFormatAttribfvARB ),
   STW_EXTENSION_ENTRY( wglGetPixelFormatAttribivARB ),

   /* WGL_EXT_extensions_string */
   STW_EXTENSION_ENTRY( wglGetExtensionsStringEXT ),

   /* WGL_EXT_swap_control */
   STW_EXTENSION_ENTRY( wglGetSwapIntervalEXT ),
   STW_EXTENSION_ENTRY( wglSwapIntervalEXT ),

   /* WGL_ARB_create_context */
   STW_EXTENSION_ENTRY( wglCreateContextAttribsARB ),

   /* WGL_ARB_render_texture */
   STW_EXTENSION_ENTRY( wglBindTexImageARB ),
   STW_EXTENSION_ENTRY( wglReleaseTexImageARB ),
   STW_EXTENSION_ENTRY( wglSetPbufferAttribARB ),

   /*  WGL_ARB_make_current_read */
   STW_EXTENSION_ENTRY( wglMakeContextCurrentARB ),
   STW_EXTENSION_ENTRY( wglGetCurrentReadDCARB ),
   { NULL, NULL }
};

PROC APIENTRY
DrvGetProcAddress(
   LPCSTR lpszProc )
{
   const struct stw_extension_entry *entry;
   PROC p;

   if (!stw_dev)
      return NULL;

   if (lpszProc[0] == 'w' && lpszProc[1] == 'g' && lpszProc[2] == 'l')
      for (entry = stw_extension_entries; entry->name; entry++)
         if (strcmp( lpszProc, entry->name ) == 0)
            return entry->proc;

   if (lpszProc[0] == 'g' && lpszProc[1] == 'l') {
      p = (PROC) _glapi_get_proc_address(lpszProc);
      if (p)
         return p;
   }

   /* If we get here, we'd normally just return NULL, but since some apps
    * (like Viewperf12) crash when they try to use the null pointer, try
    * returning a pointer to a no-op function instead.
    */
   p = stw_get_nop_function(lpszProc);
   if (p) {
      debug_printf("wglGetProcAddress(\"%s\") returning no-op function\n",
                   lpszProc);
      return p;
   }

   debug_printf("wglGetProcAddress(\"%s\") returning NULL\n", lpszProc);
   return NULL;
}
