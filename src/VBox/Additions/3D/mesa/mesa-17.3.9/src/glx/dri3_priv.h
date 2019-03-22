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

/* This file was derived from dri2_priv.h which carries the following
 * copyright:
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

#include <xcb/xcb.h>
#include <xcb/dri3.h>
#include <xcb/present.h>
#include <xcb/sync.h>

#include "loader_dri3_helper.h"

/* From xmlpool/options.h, user exposed so should be stable */
#define DRI_CONF_VBLANK_NEVER 0
#define DRI_CONF_VBLANK_DEF_INTERVAL_0 1
#define DRI_CONF_VBLANK_DEF_INTERVAL_1 2
#define DRI_CONF_VBLANK_ALWAYS_SYNC 3

struct dri3_display
{
   __GLXDRIdisplay base;

   const __DRIextension **loader_extensions;

   /* DRI3 bits */
   int dri3Major;
   int dri3Minor;

   /* Present bits */
   int hasPresent;
   int presentMajor;
   int presentMinor;
};

struct dri3_screen {
   struct glx_screen base;

   __DRIscreen *driScreen;
   __GLXDRIscreen vtable;

   const __DRIimageExtension *image;
   const __DRIimageDriverExtension *image_driver;
   const __DRIcoreExtension *core;
   const __DRI2flushExtension *f;
   const __DRI2configQueryExtension *config;
   const __DRItexBufferExtension *texBuffer;
   const __DRI2rendererQueryExtension *rendererQuery;
   const __DRI2interopExtension *interop;
   const __DRIconfig **driver_configs;

   void *driver;
   int fd;
   bool is_different_gpu;

   int show_fps_interval;

   struct loader_dri3_extensions loader_dri3_ext;
};

struct dri3_context
{
   struct glx_context base;
   __DRIcontext *driContext;
};

struct dri3_drawable {
   __GLXDRIdrawable base;
   struct loader_dri3_drawable loader_drawable;
   int swap_interval;

   /* LIBGL_SHOW_FPS support */
   uint64_t previous_ust;
   unsigned frames;
};


_X_HIDDEN int
dri3_query_renderer_integer(struct glx_screen *base, int attribute,
                            unsigned int *value);

_X_HIDDEN int
dri3_query_renderer_string(struct glx_screen *base, int attribute,
                           const char **value);

_X_HIDDEN int
dri3_interop_query_device_info(struct glx_context *ctx,
                               struct mesa_glinterop_device_info *out);

_X_HIDDEN int
dri3_interop_export_object(struct glx_context *ctx,
                           struct mesa_glinterop_export_in *in,
                           struct mesa_glinterop_export_out *out);
