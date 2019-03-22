/**************************************************************************
 *
 * Copyright 2012 Christian König.
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

/* implementation of a median filter for noise reduction */

#ifndef vl_median_filter_h
#define vl_median_filter_h

#include "pipe/p_state.h"

enum vl_median_filter_shape
{
   VL_MEDIAN_FILTER_BOX,
   VL_MEDIAN_FILTER_CROSS,
   VL_MEDIAN_FILTER_X,
   VL_MEDIAN_FILTER_HORIZONTAL,
   VL_MEDIAN_FILTER_VERTICAL
};

struct vl_median_filter
{
   struct pipe_context *pipe;
   struct pipe_vertex_buffer quad;

   void *rs_state;
   void *blend;
   void *sampler;
   void *ves;
   void *vs, *fs;
};

bool
vl_median_filter_init(struct vl_median_filter *filter, struct pipe_context *pipe,
                      unsigned width, unsigned height, unsigned size,
                      enum vl_median_filter_shape shape);

void
vl_median_filter_cleanup(struct vl_median_filter *filter);


void
vl_median_filter_render(struct vl_median_filter *filter,
                        struct pipe_sampler_view *src,
                        struct pipe_surface *dst);

#endif /* vl_median_filter_h */
