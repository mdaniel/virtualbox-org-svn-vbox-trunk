/*
 * Copyright (c) 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include "intel_batchbuffer.h"
#include "intel_fbo.h"
#include "intel_mipmap_tree.h"

#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"

#include "main/mtypes.h"
#include "main/fbobject.h"
#include "main/glformats.h"

void
gen6_emit_depth_stencil_hiz(struct brw_context *brw,
                            struct intel_mipmap_tree *depth_mt,
                            uint32_t depth_offset, uint32_t depthbuffer_format,
                            uint32_t depth_surface_type,
                            struct intel_mipmap_tree *stencil_mt,
                            bool hiz, bool separate_stencil,
                            uint32_t width, uint32_t height,
                            uint32_t tile_x, uint32_t tile_y)
{
   struct gl_context *ctx = &brw->ctx;
   struct gl_framebuffer *fb = ctx->DrawBuffer;
   uint32_t surftype;
   unsigned int depth = 1;
   GLenum gl_target = GL_TEXTURE_2D;
   unsigned int lod;
   const struct intel_mipmap_tree *mt = depth_mt ? depth_mt : stencil_mt;
   const struct intel_renderbuffer *irb = NULL;
   const struct gl_renderbuffer *rb = NULL;

   /* Enable the hiz bit if we're doing separate stencil, because it and the
    * separate stencil bit must have the same value. From Section 2.11.5.6.1.1
    * 3DSTATE_DEPTH_BUFFER, Bit 1.21 "Separate Stencil Enable":
    *     [DevIL]: If this field is enabled, Hierarchical Depth Buffer
    *     Enable must also be enabled.
    *
    *     [DevGT]: This field must be set to the same value (enabled or
    *     disabled) as Hierarchical Depth Buffer Enable
    */
   bool enable_hiz_ss = hiz || separate_stencil;

   brw_emit_depth_stall_flushes(brw);

   irb = intel_get_renderbuffer(fb, BUFFER_DEPTH);
   if (!irb)
      irb = intel_get_renderbuffer(fb, BUFFER_STENCIL);
   rb = (struct gl_renderbuffer*) irb;

   if (rb) {
      depth = MAX2(irb->layer_count, 1);
      if (rb->TexImage)
         gl_target = rb->TexImage->TexObject->Target;
   }

   switch (gl_target) {
   case GL_TEXTURE_CUBE_MAP_ARRAY:
   case GL_TEXTURE_CUBE_MAP:
      /* The PRM claims that we should use BRW_SURFACE_CUBE for this
       * situation, but experiments show that gl_Layer doesn't work when we do
       * this.  So we use BRW_SURFACE_2D, since for rendering purposes this is
       * equivalent.
       */
      surftype = BRW_SURFACE_2D;
      depth *= 6;
      break;
   case GL_TEXTURE_3D:
      assert(mt);
      depth = mt->surf.logical_level0_px.depth;
      /* fallthrough */
   default:
      surftype = translate_tex_target(gl_target);
      break;
   }

   const unsigned min_array_element = irb ? irb->mt_layer : 0;

   lod = irb ? irb->mt_level - irb->mt->first_level : 0;

   if (mt) {
      width = mt->surf.logical_level0_px.width;
      height = mt->surf.logical_level0_px.height;
   }

   BEGIN_BATCH(7);
   /* 3DSTATE_DEPTH_BUFFER dw0 */
   OUT_BATCH(_3DSTATE_DEPTH_BUFFER << 16 | (7 - 2));

   /* 3DSTATE_DEPTH_BUFFER dw1 */
   OUT_BATCH((depth_mt ? depth_mt->surf.row_pitch - 1 : 0) |
             (depthbuffer_format << 18) |
             ((enable_hiz_ss ? 1 : 0) << 21) | /* separate stencil enable */
             ((enable_hiz_ss ? 1 : 0) << 22) | /* hiz enable */
             (BRW_TILEWALK_YMAJOR << 26) |
             (1 << 27) |
             (surftype << 29));

   /* 3DSTATE_DEPTH_BUFFER dw2 */
   if (depth_mt) {
      OUT_RELOC(depth_mt->bo, RELOC_WRITE, 0);
   } else {
      OUT_BATCH(0);
   }

   /* 3DSTATE_DEPTH_BUFFER dw3 */
   OUT_BATCH(((width - 1) << 6) |
             ((height - 1) << 19) |
             lod << 2);

   /* 3DSTATE_DEPTH_BUFFER dw4 */
   OUT_BATCH((depth - 1) << 21 |
             min_array_element << 10 |
             (depth - 1) << 1);

   /* 3DSTATE_DEPTH_BUFFER dw5 */
   OUT_BATCH(0);
   assert(tile_x == 0 && tile_y == 0);

   /* 3DSTATE_DEPTH_BUFFER dw6 */
   OUT_BATCH(0);

   ADVANCE_BATCH();

   if (hiz || separate_stencil) {
      /*
       * In the 3DSTATE_DEPTH_BUFFER batch emitted above, the 'separate
       * stencil enable' and 'hiz enable' bits were set. Therefore we must
       * emit 3DSTATE_HIER_DEPTH_BUFFER and 3DSTATE_STENCIL_BUFFER. Even if
       * there is no stencil buffer, 3DSTATE_STENCIL_BUFFER must be emitted;
       * failure to do so causes hangs on gen5 and a stall on gen6.
       */

      /* Emit hiz buffer. */
      if (hiz) {
         assert(depth_mt);

         uint32_t offset;
         isl_surf_get_image_offset_B_tile_sa(&depth_mt->hiz_buf->surf,
                                             lod, 0, 0, &offset, NULL, NULL);

	 BEGIN_BATCH(3);
	 OUT_BATCH((_3DSTATE_HIER_DEPTH_BUFFER << 16) | (3 - 2));
	 OUT_BATCH(depth_mt->hiz_buf->surf.row_pitch - 1);
	 OUT_RELOC(depth_mt->hiz_buf->bo, RELOC_WRITE, offset);
	 ADVANCE_BATCH();
      } else {
	 BEGIN_BATCH(3);
	 OUT_BATCH((_3DSTATE_HIER_DEPTH_BUFFER << 16) | (3 - 2));
	 OUT_BATCH(0);
	 OUT_BATCH(0);
	 ADVANCE_BATCH();
      }

      /* Emit stencil buffer. */
      if (separate_stencil) {
         assert(stencil_mt->format == MESA_FORMAT_S_UINT8);
         assert(stencil_mt->surf.size > 0);

         uint32_t offset;
         isl_surf_get_image_offset_B_tile_sa(&stencil_mt->surf,
                                             lod, 0, 0, &offset, NULL, NULL);

	 BEGIN_BATCH(3);
	 OUT_BATCH((_3DSTATE_STENCIL_BUFFER << 16) | (3 - 2));
	 OUT_BATCH(stencil_mt->surf.row_pitch - 1);
	 OUT_RELOC(stencil_mt->bo, RELOC_WRITE, offset);
	 ADVANCE_BATCH();
      } else {
	 BEGIN_BATCH(3);
	 OUT_BATCH((_3DSTATE_STENCIL_BUFFER << 16) | (3 - 2));
	 OUT_BATCH(0);
	 OUT_BATCH(0);
	 ADVANCE_BATCH();
      }
   }

   /*
    * On Gen >= 6, emit clear params for safety. If using hiz, then clear
    * params must be emitted.
    *
    * From Section 2.11.5.6.4.1 3DSTATE_CLEAR_PARAMS:
    *     3DSTATE_CLEAR_PARAMS packet must follow the DEPTH_BUFFER_STATE packet
    *     when HiZ is enabled and the DEPTH_BUFFER_STATE changes.
    */
   BEGIN_BATCH(2);
   OUT_BATCH(_3DSTATE_CLEAR_PARAMS << 16 |
             GEN5_DEPTH_CLEAR_VALID |
             (2 - 2));
   if (depth_mt) {
      OUT_BATCH(brw_convert_depth_value(depth_mt->format,
                                        depth_mt->fast_clear_color.f32[0]));
   } else {
      OUT_BATCH(0);
   }
   ADVANCE_BATCH();
}
