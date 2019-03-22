/*
 * Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.
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

#ifndef DRI_HELPERS_H
#define DRI_HELPERS_H

#include "dri_context.h"
#include "dri_screen.h"

extern const __DRI2fenceExtension dri2FenceExtension;

__DRIimage *
dri2_lookup_egl_image(struct dri_screen *screen, void *handle);

__DRIimage *
dri2_create_image_from_renderbuffer(__DRIcontext *context,
				    int renderbuffer, void *loaderPrivate);

__DRIimage *
dri2_create_image_from_renderbuffer2(__DRIcontext *context,
				     int renderbuffer, void *loaderPrivate,
                                     unsigned *error);

void
dri2_destroy_image(__DRIimage *img);

__DRIimage *
dri2_create_from_texture(__DRIcontext *context, int target, unsigned texture,
                         int depth, int level, unsigned *error,
                         void *loaderPrivate);
#endif

/* vim: set sw=3 ts=8 sts=3 expandtab: */
