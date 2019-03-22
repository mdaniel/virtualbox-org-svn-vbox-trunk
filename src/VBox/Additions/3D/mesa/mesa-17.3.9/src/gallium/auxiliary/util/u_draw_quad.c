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


#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_draw_quad.h"
#include "util/u_memory.h"
#include "cso_cache/cso_context.h"


/**
 * Draw a simple vertex buffer / primitive.
 * Limited to float[4] vertex attribs, tightly packed.
 */
void 
util_draw_vertex_buffer(struct pipe_context *pipe,
                        struct cso_context *cso,
                        struct pipe_resource *vbuf,
                        uint vbuf_slot,
                        uint offset,
                        uint prim_type,
                        uint num_verts,
                        uint num_attribs)
{
   struct pipe_vertex_buffer vbuffer;

   assert(num_attribs <= PIPE_MAX_ATTRIBS);

   /* tell pipe about the vertex buffer */
   memset(&vbuffer, 0, sizeof(vbuffer));
   vbuffer.buffer.resource = vbuf;
   vbuffer.stride = num_attribs * 4 * sizeof(float);  /* vertex size */
   vbuffer.buffer_offset = offset;

   /* note: vertex elements already set by caller */

   if (cso) {
      cso_set_vertex_buffers(cso, vbuf_slot, 1, &vbuffer);
      cso_draw_arrays(cso, prim_type, 0, num_verts);
   } else {
      pipe->set_vertex_buffers(pipe, vbuf_slot, 1, &vbuffer);
      util_draw_arrays(pipe, prim_type, 0, num_verts);
   }
}


/**
 * Draw a simple vertex buffer / primitive.
 * Limited to float[4] vertex attribs, tightly packed.
 */
void
util_draw_user_vertex_buffer(struct cso_context *cso, void *buffer,
                             uint prim_type, uint num_verts, uint num_attribs)
{
   struct pipe_vertex_buffer vbuffer = {0};

   assert(num_attribs <= PIPE_MAX_ATTRIBS);

   vbuffer.is_user_buffer = true;
   vbuffer.buffer.user = buffer;
   vbuffer.stride = num_attribs * 4 * sizeof(float);  /* vertex size */

   /* note: vertex elements already set by caller */

   cso_set_vertex_buffers(cso, 0, 1, &vbuffer);
   cso_draw_arrays(cso, prim_type, 0, num_verts);
}
