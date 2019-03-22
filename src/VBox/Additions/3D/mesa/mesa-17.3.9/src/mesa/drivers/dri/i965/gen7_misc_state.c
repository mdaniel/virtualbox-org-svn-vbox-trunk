/*
 * Copyright © 2011 Intel Corporation
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

#include "main/mtypes.h"
#include "intel_batchbuffer.h"
#include "intel_mipmap_tree.h"
#include "intel_fbo.h"
#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"

void
gen7_emit_depth_stencil_hiz(struct brw_context *brw,
                            struct intel_mipmap_tree *depth_mt,
                            uint32_t depth_offset, uint32_t depthbuffer_format,
                            uint32_t depth_surface_type,
                            struct intel_mipmap_tree *stencil_mt,
                            bool hiz, bool separate_stencil,
                            uint32_t width, uint32_t height,
                            uint32_t tile_x, uint32_t tile_y)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   struct gl_context *ctx = &brw->ctx;
   const uint8_t mocs = GEN7_MOCS_L3;
   struct gl_framebuffer *fb = ctx->DrawBuffer;
   uint32_t surftype;
   unsigned int depth = 1;
   unsigned int min_array_element;
   GLenum gl_target = GL_TEXTURE_2D;
   unsigned int lod;
   const struct intel_mipmap_tree *mt = depth_mt ? depth_mt : stencil_mt;
   const struct intel_renderbuffer *irb = NULL;
   const struct gl_renderbuffer *rb = NULL;

   /* Skip repeated NULL depth/stencil emits (think 2D rendering). */
   if (!mt && brw->no_depth_or_stencil) {
      assert(brw->hw_ctx);
      return;
   }

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

   min_array_element = irb ? irb->mt_layer : 0;

   lod = irb ? irb->mt_level - irb->mt->first_level : 0;

   if (mt) {
      width = mt->surf.logical_level0_px.width;
      height = mt->surf.logical_level0_px.height;
   }

   /* _NEW_DEPTH, _NEW_STENCIL, _NEW_BUFFERS */
   BEGIN_BATCH(7);
   /* 3DSTATE_DEPTH_BUFFER dw0 */
   OUT_BATCH(GEN7_3DSTATE_DEPTH_BUFFER << 16 | (7 - 2));

   /* 3DSTATE_DEPTH_BUFFER dw1 */
   OUT_BATCH((depth_mt ? depth_mt->surf.row_pitch - 1 : 0) |
             (depthbuffer_format << 18) |
             ((hiz ? 1 : 0) << 22) |
             ((stencil_mt != NULL && brw->stencil_write_enabled) << 27) |
             (brw_depth_writes_enabled(brw) << 28) |
             (surftype << 29));

   /* 3DSTATE_DEPTH_BUFFER dw2 */
   if (depth_mt) {
      OUT_RELOC(depth_mt->bo, RELOC_WRITE, 0);
   } else {
      OUT_BATCH(0);
   }

   /* 3DSTATE_DEPTH_BUFFER dw3 */
   OUT_BATCH(((width - 1) << 4) |
             ((height - 1) << 18) |
             lod);

   /* 3DSTATE_DEPTH_BUFFER dw4 */
   OUT_BATCH(((depth - 1) << 21) |
             (min_array_element << 10) |
             mocs);

   /* 3DSTATE_DEPTH_BUFFER dw5 */
   OUT_BATCH(0);

   /* 3DSTATE_DEPTH_BUFFER dw6 */
   OUT_BATCH((depth - 1) << 21);
   ADVANCE_BATCH();

   if (!hiz) {
      BEGIN_BATCH(3);
      OUT_BATCH(GEN7_3DSTATE_HIER_DEPTH_BUFFER << 16 | (3 - 2));
      OUT_BATCH(0);
      OUT_BATCH(0);
      ADVANCE_BATCH();
   } else {
      assert(depth_mt);

      BEGIN_BATCH(3);
      OUT_BATCH(GEN7_3DSTATE_HIER_DEPTH_BUFFER << 16 | (3 - 2));
      OUT_BATCH((mocs << 25) |
                (depth_mt->hiz_buf->pitch - 1));
      OUT_RELOC(depth_mt->hiz_buf->bo, RELOC_WRITE, 0);
      ADVANCE_BATCH();
   }

   if (stencil_mt == NULL) {
      BEGIN_BATCH(3);
      OUT_BATCH(GEN7_3DSTATE_STENCIL_BUFFER << 16 | (3 - 2));
      OUT_BATCH(0);
      OUT_BATCH(0);
      ADVANCE_BATCH();
   } else {
      stencil_mt->r8stencil_needs_update = true;
      const int enabled = devinfo->is_haswell ? HSW_STENCIL_ENABLED : 0;

      BEGIN_BATCH(3);
      OUT_BATCH(GEN7_3DSTATE_STENCIL_BUFFER << 16 | (3 - 2));
      OUT_BATCH(enabled |
                mocs << 25 |
	        (stencil_mt->surf.row_pitch - 1));
      OUT_RELOC(stencil_mt->bo, RELOC_WRITE, 0);
      ADVANCE_BATCH();
   }

   BEGIN_BATCH(3);
   OUT_BATCH(GEN7_3DSTATE_CLEAR_PARAMS << 16 | (3 - 2));
   if (depth_mt) {
      OUT_BATCH(brw_convert_depth_value(depth_mt->format,
                                        depth_mt->fast_clear_color.f32[0]));
   } else {
      OUT_BATCH(0);
   }
   OUT_BATCH(1);
   ADVANCE_BATCH();

   brw->no_depth_or_stencil = !mt;
}

/**
 * \see brw_context.state.depth_region
 */
const struct brw_tracked_state gen7_depthbuffer = {
   .dirty = {
      .mesa = _NEW_BUFFERS |
              _NEW_DEPTH |
              _NEW_STENCIL,
      .brw = BRW_NEW_BATCH |
             BRW_NEW_BLORP,
   },
   .emit = brw_emit_depthbuffer,
};
