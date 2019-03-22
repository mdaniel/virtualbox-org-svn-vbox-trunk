/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

#ifndef SVGA_DRAW_H
#define SVGA_DRAW_H

#include "pipe/p_compiler.h"

#include "svga_hw_reg.h"

struct svga_hwtnl;
struct svga_winsys_context;
struct svga_screen;
struct svga_context;
struct pipe_resource;
struct u_upload_mgr;

struct svga_hwtnl *svga_hwtnl_create(struct svga_context *svga);

void svga_hwtnl_destroy(struct svga_hwtnl *hwtnl);

void svga_hwtnl_set_flatshade(struct svga_hwtnl *hwtnl,
                              boolean flatshade, boolean flatshade_first);

void svga_hwtnl_set_fillmode(struct svga_hwtnl *hwtnl, unsigned mode);

void
svga_hwtnl_vertex_decls(struct svga_hwtnl *hwtnl,
                        unsigned count,
                        const SVGA3dVertexDecl * decls,
                        const unsigned *buffer_indexes,
                        SVGA3dElementLayoutId layoutId);

void
svga_hwtnl_vertex_buffers(struct svga_hwtnl *hwtnl,
                          unsigned count, struct pipe_vertex_buffer *buffers);

enum pipe_error
svga_hwtnl_draw_arrays(struct svga_hwtnl *hwtnl,
                       enum pipe_prim_type prim, unsigned start, unsigned count,
                       unsigned start_instance, unsigned instance_count);

enum pipe_error
svga_hwtnl_draw_range_elements(struct svga_hwtnl *hwtnl,
                               struct pipe_resource *indexBuffer,
                               unsigned index_size,
                               int index_bias,
                               unsigned min_index,
                               unsigned max_index,
                               enum pipe_prim_type prim, unsigned start, unsigned count,
                               unsigned start_instance, unsigned instance_count);

boolean
svga_hwtnl_is_buffer_referred(struct svga_hwtnl *hwtnl,
                              struct pipe_resource *buffer);

enum pipe_error svga_hwtnl_flush(struct svga_hwtnl *hwtnl);

void svga_hwtnl_set_index_bias(struct svga_hwtnl *hwtnl, int index_bias);

#ifdef VBOX_WITH_MESA3D_SVGA_INSTANCING
void svga_hwtnl_set_instanced(struct svga_hwtnl *hwtnl, boolean instanced);
#endif

#endif /* SVGA_DRAW_H_ */
