/**************************************************************************
 * 
 * Copyright 2003 VMware, Inc.
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

/** @file intel_tris.c
 *
 * This file contains functions for managing the vertex buffer and emitting
 * primitives into it.
 */

#include "main/glheader.h"
#include "main/context.h"
#include "main/macros.h"
#include "main/enums.h"
#include "main/texobj.h"
#include "main/state.h"
#include "main/dd.h"
#include "main/fbobject.h"
#include "main/state.h"

#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "tnl/t_vertex.h"

#include "intel_screen.h"
#include "intel_context.h"
#include "intel_tris.h"
#include "intel_batchbuffer.h"
#include "intel_buffers.h"
#include "intel_reg.h"
#include "i830_context.h"
#include "i830_reg.h"
#include "i915_context.h"

static void intelRenderPrimitive(struct gl_context * ctx, GLenum prim);
static void intelRasterPrimitive(struct gl_context * ctx, GLenum rprim,
                                 GLuint hwprim);

static void
intel_flush_inline_primitive(struct intel_context *intel)
{
   GLuint used = intel->batch.used - intel->prim.start_ptr;

   assert(intel->prim.primitive != ~0);

/*    printf("/\n"); */

   if (used < 2)
      goto do_discard;

   intel->batch.map[intel->prim.start_ptr] =
      _3DPRIMITIVE | intel->prim.primitive | (used - 2);

   goto finished;

 do_discard:
   intel->batch.used = intel->prim.start_ptr;

 finished:
   intel->prim.primitive = ~0;
   intel->prim.start_ptr = 0;
   intel->prim.flush = 0;
}

static void intel_start_inline(struct intel_context *intel, uint32_t prim)
{
   BATCH_LOCALS;

   intel->vtbl.emit_state(intel);

   intel->no_batch_wrap = true;

   /* Emit a slot which will be filled with the inline primitive
    * command later.
    */
   BEGIN_BATCH(1);

   intel->prim.start_ptr = intel->batch.used;
   intel->prim.primitive = prim;
   intel->prim.flush = intel_flush_inline_primitive;

   OUT_BATCH(0);
   ADVANCE_BATCH();

   intel->no_batch_wrap = false;
/*    printf(">"); */
}

static void intel_wrap_inline(struct intel_context *intel)
{
   GLuint prim = intel->prim.primitive;

   intel_flush_inline_primitive(intel);
   intel_batchbuffer_flush(intel);
   intel_start_inline(intel, prim);  /* ??? */
}

static GLuint *intel_extend_inline(struct intel_context *intel, GLuint dwords)
{
   GLuint *ptr;

   assert(intel->prim.flush == intel_flush_inline_primitive);

   if (intel_batchbuffer_space(intel) < dwords * sizeof(GLuint))
      intel_wrap_inline(intel);

/*    printf("."); */

   intel->vtbl.assert_not_dirty(intel);

   ptr = intel->batch.map + intel->batch.used;
   intel->batch.used += dwords;

   return ptr;
}

/** Sets the primitive type for a primitive sequence, flushing as needed. */
void intel_set_prim(struct intel_context *intel, uint32_t prim)
{
   /* if we have no VBOs */

   if (intel->intelScreen->no_vbo) {
      intel_start_inline(intel, prim);
      return;
   }
   if (prim != intel->prim.primitive) {
      INTEL_FIREVERTICES(intel);
      intel->prim.primitive = prim;
   }
}

/** Returns mapped VB space for the given number of vertices */
uint32_t *intel_get_prim_space(struct intel_context *intel, unsigned int count)
{
   uint32_t *addr;

   if (intel->intelScreen->no_vbo) {
      return intel_extend_inline(intel, count * intel->vertex_size);
   }

   /* Check for space in the existing VB */
   if (intel->prim.vb_bo == NULL ||
       (intel->prim.current_offset +
	count * intel->vertex_size * 4) > INTEL_VB_SIZE ||
       (intel->prim.count + count) >= (1 << 16)) {
      /* Flush existing prim if any */
      INTEL_FIREVERTICES(intel);

      intel_finish_vb(intel);

      /* Start a new VB */
      if (intel->prim.vb == NULL)
	 intel->prim.vb = malloc(INTEL_VB_SIZE);
      intel->prim.vb_bo = drm_intel_bo_alloc(intel->bufmgr, "vb",
					     INTEL_VB_SIZE, 4);
      intel->prim.start_offset = 0;
      intel->prim.current_offset = 0;
   }

   intel->prim.flush = intel_flush_prim;

   addr = (uint32_t *)(intel->prim.vb + intel->prim.current_offset);
   intel->prim.current_offset += intel->vertex_size * 4 * count;
   intel->prim.count += count;

   return addr;
}

/** Dispatches the accumulated primitive to the batchbuffer. */
void intel_flush_prim(struct intel_context *intel)
{
   drm_intel_bo *aper_array[2];
   drm_intel_bo *vb_bo;
   unsigned int offset, count;
   BATCH_LOCALS;

   /* Must be called after an intel_start_prim. */
   assert(intel->prim.primitive != ~0);

   if (intel->prim.count == 0)
      return;

   /* Clear the current prims out of the context state so that a batch flush
    * flush triggered by emit_state doesn't loop back to flush_prim again.
    */
   vb_bo = intel->prim.vb_bo;
   drm_intel_bo_reference(vb_bo);
   count = intel->prim.count;
   intel->prim.count = 0;
   offset = intel->prim.start_offset;
   intel->prim.start_offset = intel->prim.current_offset;
   if (intel->gen < 3)
      intel->prim.current_offset = intel->prim.start_offset = ALIGN(intel->prim.start_offset, 128);
   intel->prim.flush = NULL;

   intel->vtbl.emit_state(intel);

   aper_array[0] = intel->batch.bo;
   aper_array[1] = vb_bo;
   if (dri_bufmgr_check_aperture_space(aper_array, 2)) {
      intel_batchbuffer_flush(intel);
      intel->vtbl.emit_state(intel);
   }

   /* Ensure that we don't start a new batch for the following emit, which
    * depends on the state just emitted. emit_state should be making sure we
    * have the space for this.
    */
   intel->no_batch_wrap = true;

   if (intel->always_flush_cache) {
      intel_batchbuffer_emit_mi_flush(intel);
   }

#if 0
   printf("emitting %d..%d=%d vertices size %d\n", offset,
	  intel->prim.current_offset, count,
	  intel->vertex_size * 4);
#endif

   if (intel->gen >= 3) {
      struct i915_context *i915 = i915_context(&intel->ctx);
      unsigned int cmd = 0, len = 0;

      if (vb_bo != i915->current_vb_bo) {
	 cmd |= I1_LOAD_S(0);
	 len++;
      }

      if (intel->vertex_size != i915->current_vertex_size) {
	 cmd |= I1_LOAD_S(1);
	 len++;
      }
      if (len)
	 len++;

      BEGIN_BATCH(2+len);
      if (cmd)
	 OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | cmd | (len - 2));
      if (vb_bo != i915->current_vb_bo) {
	 OUT_RELOC(vb_bo, I915_GEM_DOMAIN_VERTEX, 0, 0);
	 i915->current_vb_bo = vb_bo;
      }
      if (intel->vertex_size != i915->current_vertex_size) {
	 OUT_BATCH((intel->vertex_size << S1_VERTEX_WIDTH_SHIFT) |
		   (intel->vertex_size << S1_VERTEX_PITCH_SHIFT));
	 i915->current_vertex_size = intel->vertex_size;
      }
      OUT_BATCH(_3DPRIMITIVE |
		PRIM_INDIRECT |
		PRIM_INDIRECT_SEQUENTIAL |
		intel->prim.primitive |
		count);
      OUT_BATCH(offset / (intel->vertex_size * 4));
      ADVANCE_BATCH();
   } else {
      struct i830_context *i830 = i830_context(&intel->ctx);

      BEGIN_BATCH(5);
      OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		I1_LOAD_S(0) | I1_LOAD_S(2) | 1);
      /* S0 */
      assert((offset & ~S0_VB_OFFSET_MASK_830) == 0);
      OUT_RELOC(vb_bo, I915_GEM_DOMAIN_VERTEX, 0,
		offset | (intel->vertex_size << S0_VB_PITCH_SHIFT_830) |
		S0_VB_ENABLE_830);
      /* S2
       * This is somewhat unfortunate -- VB width is tied up with
       * vertex format data that we've already uploaded through
       * _3DSTATE_VFT[01]_CMD.  We may want to replace emits of VFT state with
       * STATE_IMMEDIATE_1 like this to avoid duplication.
       */
      OUT_BATCH((i830->state.Ctx[I830_CTXREG_VF] & VFT0_TEX_COUNT_MASK) >>
		VFT0_TEX_COUNT_SHIFT << S2_TEX_COUNT_SHIFT_830 |
		(i830->state.Ctx[I830_CTXREG_VF2] << 16) |
		intel->vertex_size << S2_VERTEX_0_WIDTH_SHIFT_830);

      OUT_BATCH(_3DPRIMITIVE |
		PRIM_INDIRECT |
		PRIM_INDIRECT_SEQUENTIAL |
		intel->prim.primitive |
		count);
      OUT_BATCH(0); /* Beginning vertex index */
      ADVANCE_BATCH();
   }

   if (intel->always_flush_cache) {
      intel_batchbuffer_emit_mi_flush(intel);
   }

   intel->no_batch_wrap = false;

   drm_intel_bo_unreference(vb_bo);
}

/**
 * Uploads the locally-accumulated VB into the buffer object.
 *
 * This avoids us thrashing the cachelines in and out as the buffer gets
 * filled, dispatched, then reused as the hardware completes rendering from it,
 * and also lets us clflush less if we dispatch with a partially-filled VB.
 *
 * This is called normally from get_space when we're finishing a BO, but also
 * at batch flush time so that we don't try accessing the contents of a
 * just-dispatched buffer.
 */
void intel_finish_vb(struct intel_context *intel)
{
   if (intel->prim.vb_bo == NULL)
      return;

   drm_intel_bo_subdata(intel->prim.vb_bo, 0, intel->prim.start_offset,
			intel->prim.vb);
   drm_intel_bo_unreference(intel->prim.vb_bo);
   intel->prim.vb_bo = NULL;
}

/***********************************************************************
 *                    Emit primitives as inline vertices               *
 ***********************************************************************/

#ifdef __i386__
#define COPY_DWORDS( j, vb, vertsize, v )			\
do {								\
   int __tmp;							\
   __asm__ __volatile__( "rep ; movsl"				\
			 : "=%c" (j), "=D" (vb), "=S" (__tmp)	\
			 : "0" (vertsize),			\
			 "D" ((long)vb),			\
			 "S" ((long)v) );			\
} while (0)
#else
#define COPY_DWORDS( j, vb, vertsize, v )	\
do {						\
   for ( j = 0 ; j < vertsize ; j++ ) {		\
      vb[j] = ((GLuint *)v)[j];			\
   }						\
   vb += vertsize;				\
} while (0)
#endif

static void
intel_draw_quad(struct intel_context *intel,
                intelVertexPtr v0,
                intelVertexPtr v1, intelVertexPtr v2, intelVertexPtr v3)
{
   GLuint vertsize = intel->vertex_size;
   GLuint *vb = intel_get_prim_space(intel, 6);
   int j;

   COPY_DWORDS(j, vb, vertsize, v0);
   COPY_DWORDS(j, vb, vertsize, v1);

   /* If smooth shading, draw like a trifan which gives better
    * rasterization.  Otherwise draw as two triangles with provoking
    * vertex in third position as required for flat shading.
    */
   if (intel->ctx.Light.ShadeModel == GL_FLAT) {
      COPY_DWORDS(j, vb, vertsize, v3);
      COPY_DWORDS(j, vb, vertsize, v1);
   }
   else {
      COPY_DWORDS(j, vb, vertsize, v2);
      COPY_DWORDS(j, vb, vertsize, v0);
   }

   COPY_DWORDS(j, vb, vertsize, v2);
   COPY_DWORDS(j, vb, vertsize, v3);
}

static void
intel_draw_triangle(struct intel_context *intel,
                    intelVertexPtr v0, intelVertexPtr v1, intelVertexPtr v2)
{
   GLuint vertsize = intel->vertex_size;
   GLuint *vb = intel_get_prim_space(intel, 3);
   int j;

   COPY_DWORDS(j, vb, vertsize, v0);
   COPY_DWORDS(j, vb, vertsize, v1);
   COPY_DWORDS(j, vb, vertsize, v2);
}


static void
intel_draw_line(struct intel_context *intel,
                intelVertexPtr v0, intelVertexPtr v1)
{
   GLuint vertsize = intel->vertex_size;
   GLuint *vb = intel_get_prim_space(intel, 2);
   int j;

   COPY_DWORDS(j, vb, vertsize, v0);
   COPY_DWORDS(j, vb, vertsize, v1);
}


static void
intel_draw_point(struct intel_context *intel, intelVertexPtr v0)
{
   GLuint vertsize = intel->vertex_size;
   GLuint *vb = intel_get_prim_space(intel, 1);
   int j;

   COPY_DWORDS(j, vb, vertsize, v0);
}



/***********************************************************************
 *                Fixup for ARB_point_parameters                       *
 ***********************************************************************/

/* Currently not working - VERT_ATTRIB_POINTSIZE isn't correctly
 * represented in the fragment program info.inputs_read field.
 */
static void
intel_atten_point(struct intel_context *intel, intelVertexPtr v0)
{
   struct gl_context *ctx = &intel->ctx;
   GLfloat psz[4], col[4], restore_psz, restore_alpha;

   _tnl_get_attr(ctx, v0, _TNL_ATTRIB_POINTSIZE, psz);
   _tnl_get_attr(ctx, v0, _TNL_ATTRIB_COLOR0, col);

   restore_psz = psz[0];
   restore_alpha = col[3];

   if (psz[0] >= ctx->Point.Threshold) {
      psz[0] = MIN2(psz[0], ctx->Point.MaxSize);
   }
   else {
      GLfloat dsize = psz[0] / ctx->Point.Threshold;
      psz[0] = MAX2(ctx->Point.Threshold, ctx->Point.MinSize);
      col[3] *= dsize * dsize;
   }

   if (psz[0] < 1.0)
      psz[0] = 1.0;

   if (restore_psz != psz[0] || restore_alpha != col[3]) {
      _tnl_set_attr(ctx, v0, _TNL_ATTRIB_POINTSIZE, psz);
      _tnl_set_attr(ctx, v0, _TNL_ATTRIB_COLOR0, col);

      intel_draw_point(intel, v0);

      psz[0] = restore_psz;
      col[3] = restore_alpha;

      _tnl_set_attr(ctx, v0, _TNL_ATTRIB_POINTSIZE, psz);
      _tnl_set_attr(ctx, v0, _TNL_ATTRIB_COLOR0, col);
   }
   else
      intel_draw_point(intel, v0);
}





/***********************************************************************
 *                Fixup for I915 WPOS texture coordinate                *
 ***********************************************************************/

static void
intel_emit_fragcoord(struct intel_context *intel, intelVertexPtr v)
{
   struct gl_context *ctx = &intel->ctx;
   struct gl_framebuffer *fb = ctx->DrawBuffer;
   GLuint offset = intel->wpos_offset;
   float *vertex_position = (float *)v;
   float *fragcoord = (float *)((char *)v + offset);

   fragcoord[0] = vertex_position[0];

   if (_mesa_is_user_fbo(fb))
      fragcoord[1] = vertex_position[1];
   else
      fragcoord[1] = fb->Height - vertex_position[1];

   fragcoord[2] = vertex_position[2];
   fragcoord[3] = vertex_position[3];
}

static void
intel_wpos_triangle(struct intel_context *intel,
                    intelVertexPtr v0, intelVertexPtr v1, intelVertexPtr v2)
{
   intel_emit_fragcoord(intel, v0);
   intel_emit_fragcoord(intel, v1);
   intel_emit_fragcoord(intel, v2);

   intel_draw_triangle(intel, v0, v1, v2);
}


static void
intel_wpos_line(struct intel_context *intel,
                intelVertexPtr v0, intelVertexPtr v1)
{
   intel_emit_fragcoord(intel, v0);
   intel_emit_fragcoord(intel, v1);
   intel_draw_line(intel, v0, v1);
}


static void
intel_wpos_point(struct intel_context *intel, intelVertexPtr v0)
{
   intel_emit_fragcoord(intel, v0);
   intel_draw_point(intel, v0);
}






/***********************************************************************
 *          Macros for t_dd_tritmp.h to draw basic primitives          *
 ***********************************************************************/

#define TRI( a, b, c )				\
do { 						\
   if (DO_FALLBACK)				\
      intel->draw_tri( intel, a, b, c );	\
   else						\
      intel_draw_triangle( intel, a, b, c );	\
} while (0)

#define QUAD( a, b, c, d )			\
do { 						\
   if (DO_FALLBACK) {				\
      intel->draw_tri( intel, a, b, d );	\
      intel->draw_tri( intel, b, c, d );	\
   } else					\
      intel_draw_quad( intel, a, b, c, d );	\
} while (0)

#define LINE( v0, v1 )				\
do { 						\
   if (DO_FALLBACK)				\
      intel->draw_line( intel, v0, v1 );	\
   else						\
      intel_draw_line( intel, v0, v1 );		\
} while (0)

#define POINT( v0 )				\
do { 						\
   if (DO_FALLBACK)				\
      intel->draw_point( intel, v0 );		\
   else						\
      intel_draw_point( intel, v0 );		\
} while (0)


/***********************************************************************
 *              Build render functions from dd templates               *
 ***********************************************************************/

#define INTEL_OFFSET_BIT 	0x01
#define INTEL_TWOSIDE_BIT	0x02
#define INTEL_UNFILLED_BIT	0x04
#define INTEL_FALLBACK_BIT	0x08
#define INTEL_MAX_TRIFUNC	0x10


static struct
{
   tnl_points_func points;
   tnl_line_func line;
   tnl_triangle_func triangle;
   tnl_quad_func quad;
} rast_tab[INTEL_MAX_TRIFUNC];


#define DO_FALLBACK ((IND & INTEL_FALLBACK_BIT) != 0)
#define DO_OFFSET   ((IND & INTEL_OFFSET_BIT) != 0)
#define DO_UNFILLED ((IND & INTEL_UNFILLED_BIT) != 0)
#define DO_TWOSIDE  ((IND & INTEL_TWOSIDE_BIT) != 0)
#define DO_FLAT      0
#define DO_TRI       1
#define DO_QUAD      1
#define DO_LINE      1
#define DO_POINTS    1
#define DO_FULL_QUAD 1

#define HAVE_SPEC         1
#define HAVE_BACK_COLORS  0
#define HAVE_HW_FLATSHADE 1
#define VERTEX            intelVertex
#define TAB               rast_tab

/* Only used to pull back colors into vertices (ie, we know color is
 * floating point).
 */
#define INTEL_COLOR( dst, src )				\
do {							\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[0], (src)[2]);	\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[1], (src)[1]);	\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[2], (src)[0]);	\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[3], (src)[3]);	\
} while (0)

#define INTEL_SPEC( dst, src )				\
do {							\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[0], (src)[2]);	\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[1], (src)[1]);	\
   UNCLAMPED_FLOAT_TO_UBYTE((dst)[2], (src)[0]);	\
} while (0)


#define DEPTH_SCALE (ctx->DrawBuffer->Visual.depthBits == 16 ? 1.0 : 2.0)
#define UNFILLED_TRI unfilled_tri
#define UNFILLED_QUAD unfilled_quad
#define VERT_X(_v) _v->v.x
#define VERT_Y(_v) _v->v.y
#define VERT_Z(_v) _v->v.z
#define AREA_IS_CCW( a ) (a > 0)
#define GET_VERTEX(e) (intel->verts + (e * intel->vertex_size * sizeof(GLuint)))

#define VERT_SET_RGBA( v, c )    if (coloroffset) INTEL_COLOR( v->ub4[coloroffset], c )
#define VERT_COPY_RGBA( v0, v1 ) if (coloroffset) v0->ui[coloroffset] = v1->ui[coloroffset]
#define VERT_SAVE_RGBA( idx )    if (coloroffset) color[idx] = v[idx]->ui[coloroffset]
#define VERT_RESTORE_RGBA( idx ) if (coloroffset) v[idx]->ui[coloroffset] = color[idx]

#define VERT_SET_SPEC( v, c )    if (specoffset) INTEL_SPEC( v->ub4[specoffset], c )
#define VERT_COPY_SPEC( v0, v1 ) if (specoffset) COPY_3V(v0->ub4[specoffset], v1->ub4[specoffset])
#define VERT_SAVE_SPEC( idx )    if (specoffset) spec[idx] = v[idx]->ui[specoffset]
#define VERT_RESTORE_SPEC( idx ) if (specoffset) v[idx]->ui[specoffset] = spec[idx]

#define LOCAL_VARS(n)							\
   struct intel_context *intel = intel_context(ctx);			\
   GLuint color[n] = { 0, }, spec[n] = { 0, };				\
   GLuint coloroffset = intel->coloroffset;				\
   GLuint specoffset = intel->specoffset;				\
   (void) color; (void) spec; (void) coloroffset; (void) specoffset;


/***********************************************************************
 *                Helpers for rendering unfilled primitives            *
 ***********************************************************************/

static const GLuint hw_prim[GL_POLYGON + 1] = {
   [GL_POINTS] = PRIM3D_POINTLIST,
   [GL_LINES] = PRIM3D_LINELIST,
   [GL_LINE_LOOP] = PRIM3D_LINELIST,
   [GL_LINE_STRIP] = PRIM3D_LINELIST,
   [GL_TRIANGLES] = PRIM3D_TRILIST,
   [GL_TRIANGLE_STRIP] = PRIM3D_TRILIST,
   [GL_TRIANGLE_FAN] = PRIM3D_TRILIST,
   [GL_QUADS] = PRIM3D_TRILIST,
   [GL_QUAD_STRIP] = PRIM3D_TRILIST,
   [GL_POLYGON] = PRIM3D_TRILIST,
};

#define RASTERIZE(x) intelRasterPrimitive( ctx, x, hw_prim[x] )
#define RENDER_PRIMITIVE intel->render_primitive
#define TAG(x) x
#define IND INTEL_FALLBACK_BIT
#include "tnl_dd/t_dd_unfilled.h"
#undef IND

/***********************************************************************
 *                      Generate GL render functions                   *
 ***********************************************************************/

#define IND (0)
#define TAG(x) x
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_OFFSET_BIT)
#define TAG(x) x##_offset
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT)
#define TAG(x) x##_twoside
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_OFFSET_BIT)
#define TAG(x) x##_twoside_offset
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_UNFILLED_BIT)
#define TAG(x) x##_unfilled
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_OFFSET_BIT|INTEL_UNFILLED_BIT)
#define TAG(x) x##_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_UNFILLED_BIT)
#define TAG(x) x##_twoside_unfilled
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_OFFSET_BIT|INTEL_UNFILLED_BIT)
#define TAG(x) x##_twoside_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_FALLBACK_BIT)
#define TAG(x) x##_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_OFFSET_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_offset_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_twoside_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_OFFSET_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_twoside_offset_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_UNFILLED_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_unfilled_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_OFFSET_BIT|INTEL_UNFILLED_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_offset_unfilled_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_UNFILLED_BIT|INTEL_FALLBACK_BIT)
#define TAG(x) x##_twoside_unfilled_fallback
#include "tnl_dd/t_dd_tritmp.h"

#define IND (INTEL_TWOSIDE_BIT|INTEL_OFFSET_BIT|INTEL_UNFILLED_BIT| \
	     INTEL_FALLBACK_BIT)
#define TAG(x) x##_twoside_offset_unfilled_fallback
#include "tnl_dd/t_dd_tritmp.h"


static void
init_rast_tab(void)
{
   init();
   init_offset();
   init_twoside();
   init_twoside_offset();
   init_unfilled();
   init_offset_unfilled();
   init_twoside_unfilled();
   init_twoside_offset_unfilled();
   init_fallback();
   init_offset_fallback();
   init_twoside_fallback();
   init_twoside_offset_fallback();
   init_unfilled_fallback();
   init_offset_unfilled_fallback();
   init_twoside_unfilled_fallback();
   init_twoside_offset_unfilled_fallback();
}


/***********************************************************************
 *                    Rasterization fallback helpers                   *
 ***********************************************************************/


/* This code is hit only when a mix of accelerated and unaccelerated
 * primitives are being drawn, and only for the unaccelerated
 * primitives.
 */
static void
intel_fallback_tri(struct intel_context *intel,
                   intelVertex * v0, intelVertex * v1, intelVertex * v2)
{
   struct gl_context *ctx = &intel->ctx;
   SWvertex v[3];

   if (0)
      fprintf(stderr, "\n%s\n", __func__);

   INTEL_FIREVERTICES(intel);

   _swsetup_Translate(ctx, v0, &v[0]);
   _swsetup_Translate(ctx, v1, &v[1]);
   _swsetup_Translate(ctx, v2, &v[2]);
   _swrast_render_start(ctx);
   _swrast_Triangle(ctx, &v[0], &v[1], &v[2]);
   _swrast_render_finish(ctx);
}


static void
intel_fallback_line(struct intel_context *intel,
                    intelVertex * v0, intelVertex * v1)
{
   struct gl_context *ctx = &intel->ctx;
   SWvertex v[2];

   if (0)
      fprintf(stderr, "\n%s\n", __func__);

   INTEL_FIREVERTICES(intel);

   _swsetup_Translate(ctx, v0, &v[0]);
   _swsetup_Translate(ctx, v1, &v[1]);
   _swrast_render_start(ctx);
   _swrast_Line(ctx, &v[0], &v[1]);
   _swrast_render_finish(ctx);
}

static void
intel_fallback_point(struct intel_context *intel,
		     intelVertex * v0)
{
   struct gl_context *ctx = &intel->ctx;
   SWvertex v[1];

   if (0)
      fprintf(stderr, "\n%s\n", __func__);

   INTEL_FIREVERTICES(intel);

   _swsetup_Translate(ctx, v0, &v[0]);
   _swrast_render_start(ctx);
   _swrast_Point(ctx, &v[0]);
   _swrast_render_finish(ctx);
}


/**********************************************************************/
/*               Render unclipped begin/end objects                   */
/**********************************************************************/

#define IND 0
#define V(x) (intelVertex *)(vertptr + ((x)*vertsize*sizeof(GLuint)))
#define RENDER_POINTS( start, count )	\
   for ( ; start < count ; start++) POINT( V(ELT(start)) );
#define RENDER_LINE( v0, v1 )         LINE( V(v0), V(v1) )
#define RENDER_TRI(  v0, v1, v2 )     TRI(  V(v0), V(v1), V(v2) )
#define RENDER_QUAD( v0, v1, v2, v3 ) QUAD( V(v0), V(v1), V(v2), V(v3) )
#define INIT(x) intelRenderPrimitive( ctx, x )
#undef LOCAL_VARS
#define LOCAL_VARS						\
    struct intel_context *intel = intel_context(ctx);			\
    GLubyte *vertptr = (GLubyte *)intel->verts;			\
    const GLuint vertsize = intel->vertex_size;       	\
    const GLuint * const elt = TNL_CONTEXT(ctx)->vb.Elts;	\
    (void) elt;
#define RESET_STIPPLE
#define RESET_OCCLUSION
#define PRESERVE_VB_DEFS
#define ELT(x) x
#define TAG(x) intel_##x##_verts
#include "tnl/t_vb_rendertmp.h"
#undef ELT
#undef TAG
#define TAG(x) intel_##x##_elts
#define ELT(x) elt[x]
#include "tnl/t_vb_rendertmp.h"

/**********************************************************************/
/*                   Render clipped primitives                        */
/**********************************************************************/



static void
intelRenderClippedPoly(struct gl_context * ctx, const GLuint * elts, GLuint n)
{
   struct intel_context *intel = intel_context(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   GLuint prim = intel->render_primitive;

   /* Render the new vertices as an unclipped polygon.
    */
   _tnl_RenderClippedPolygon(ctx, elts, n);

   /* Restore the render primitive
    */
   if (prim != GL_POLYGON)
      tnl->Driver.Render.PrimitiveNotify(ctx, prim);
}

static void
intelFastRenderClippedPoly(struct gl_context * ctx, const GLuint * elts, GLuint n)
{
   struct intel_context *intel = intel_context(ctx);
   const GLuint vertsize = intel->vertex_size;
   GLuint *vb = intel_get_prim_space(intel, (n - 2) * 3);
   GLubyte *vertptr = (GLubyte *) intel->verts;
   const GLuint *start = (const GLuint *) V(elts[0]);
   int i, j;

   if (ctx->Light.ProvokingVertex == GL_LAST_VERTEX_CONVENTION) {
      for (i = 2; i < n; i++) {
         COPY_DWORDS(j, vb, vertsize, V(elts[i - 1]));
         COPY_DWORDS(j, vb, vertsize, V(elts[i]));
         COPY_DWORDS(j, vb, vertsize, start);
      }
   } else {
      for (i = 2; i < n; i++) {
         COPY_DWORDS(j, vb, vertsize, start);
         COPY_DWORDS(j, vb, vertsize, V(elts[i - 1]));
         COPY_DWORDS(j, vb, vertsize, V(elts[i]));
      }
   }
}

/**********************************************************************/
/*                    Choose render functions                         */
/**********************************************************************/


#define DD_TRI_LIGHT_TWOSIDE (1 << 1)
#define DD_TRI_UNFILLED (1 << 2)
#define DD_TRI_STIPPLE  (1 << 4)
#define DD_TRI_OFFSET   (1 << 5)
#define DD_LINE_STIPPLE (1 << 7)
#define DD_POINT_ATTEN  (1 << 9)

#define ANY_FALLBACK_FLAGS (DD_LINE_STIPPLE | DD_TRI_STIPPLE | DD_POINT_ATTEN)
#define ANY_RASTER_FLAGS (DD_TRI_LIGHT_TWOSIDE | DD_TRI_OFFSET | DD_TRI_UNFILLED)

void
intelChooseRenderState(struct gl_context * ctx)
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct intel_context *intel = intel_context(ctx);
   GLuint flags =
      ((ctx->Light.Enabled &&
        ctx->Light.Model.TwoSide) ? DD_TRI_LIGHT_TWOSIDE : 0) |
      ((ctx->Polygon.FrontMode != GL_FILL ||
        ctx->Polygon.BackMode != GL_FILL) ? DD_TRI_UNFILLED : 0) |
      (ctx->Polygon.StippleFlag ? DD_TRI_STIPPLE : 0) |
      ((ctx->Polygon.OffsetPoint ||
        ctx->Polygon.OffsetLine ||
        ctx->Polygon.OffsetFill) ? DD_TRI_OFFSET : 0) |
      (ctx->Line.StippleFlag ? DD_LINE_STIPPLE : 0) |
      (ctx->Point._Attenuated ? DD_POINT_ATTEN : 0);
   const struct gl_program *fprog = ctx->FragmentProgram._Current;
   bool have_wpos = (fprog && (fprog->info.inputs_read & VARYING_BIT_POS));
   GLuint index = 0;

   if (INTEL_DEBUG & DEBUG_STATE)
      fprintf(stderr, "\n%s\n", __func__);

   if ((flags & (ANY_FALLBACK_FLAGS | ANY_RASTER_FLAGS)) || have_wpos) {

      if (flags & ANY_RASTER_FLAGS) {
         if (flags & DD_TRI_LIGHT_TWOSIDE)
            index |= INTEL_TWOSIDE_BIT;
         if (flags & DD_TRI_OFFSET)
            index |= INTEL_OFFSET_BIT;
         if (flags & DD_TRI_UNFILLED)
            index |= INTEL_UNFILLED_BIT;
      }

      if (have_wpos) {
         intel->draw_point = intel_wpos_point;
         intel->draw_line = intel_wpos_line;
         intel->draw_tri = intel_wpos_triangle;

         /* Make sure these get called:
          */
         index |= INTEL_FALLBACK_BIT;
      }
      else {
         intel->draw_point = intel_draw_point;
         intel->draw_line = intel_draw_line;
         intel->draw_tri = intel_draw_triangle;
      }

      /* Hook in fallbacks for specific primitives.
       */
      if (flags & ANY_FALLBACK_FLAGS) {
         if (flags & DD_LINE_STIPPLE)
            intel->draw_line = intel_fallback_line;

         if ((flags & DD_TRI_STIPPLE) && !intel->hw_stipple)
            intel->draw_tri = intel_fallback_tri;

         if (flags & DD_POINT_ATTEN) {
	    if (0)
	       intel->draw_point = intel_atten_point;
	    else
	       intel->draw_point = intel_fallback_point;
	 }

         index |= INTEL_FALLBACK_BIT;
      }
   }

   if (intel->RenderIndex != index) {
      intel->RenderIndex = index;

      tnl->Driver.Render.Points = rast_tab[index].points;
      tnl->Driver.Render.Line = rast_tab[index].line;
      tnl->Driver.Render.Triangle = rast_tab[index].triangle;
      tnl->Driver.Render.Quad = rast_tab[index].quad;

      if (index == 0) {
         tnl->Driver.Render.PrimTabVerts = intel_render_tab_verts;
         tnl->Driver.Render.PrimTabElts = intel_render_tab_elts;
         tnl->Driver.Render.ClippedLine = line; /* from tritmp.h */
         tnl->Driver.Render.ClippedPolygon = intelFastRenderClippedPoly;
      }
      else {
         tnl->Driver.Render.PrimTabVerts = _tnl_render_tab_verts;
         tnl->Driver.Render.PrimTabElts = _tnl_render_tab_elts;
         tnl->Driver.Render.ClippedLine = _tnl_RenderClippedLine;
         tnl->Driver.Render.ClippedPolygon = intelRenderClippedPoly;
      }
   }
}

static const GLenum reduced_prim[GL_POLYGON + 1] = {
   [GL_POINTS] = GL_POINTS,
   [GL_LINES] = GL_LINES,
   [GL_LINE_LOOP] = GL_LINES,
   [GL_LINE_STRIP] = GL_LINES,
   [GL_TRIANGLES] = GL_TRIANGLES,
   [GL_TRIANGLE_STRIP] = GL_TRIANGLES,
   [GL_TRIANGLE_FAN] = GL_TRIANGLES,
   [GL_QUADS] = GL_TRIANGLES,
   [GL_QUAD_STRIP] = GL_TRIANGLES,
   [GL_POLYGON] = GL_TRIANGLES
};


/**********************************************************************/
/*                 High level hooks for t_vb_render.c                 */
/**********************************************************************/




static void
intelRunPipeline(struct gl_context * ctx)
{
   struct intel_context *intel = intel_context(ctx);

   _mesa_lock_context_textures(ctx);
   
   if (ctx->NewState)
      _mesa_update_state_locked(ctx);

   /* We need to get this done before we start the pipeline, or a
    * change in the INTEL_FALLBACK() of its intel_draw_buffers() call
    * while the pipeline is running will result in mismatched swrast
    * map/unmaps, and later assertion failures.
    */
   intel_prepare_render(intel);

   if (intel->NewGLState) {
      if (intel->NewGLState & _NEW_TEXTURE) {
         intel->vtbl.update_texture_state(intel);
      }

      if (!intel->Fallback) {
         if (intel->NewGLState & _INTEL_NEW_RENDERSTATE)
            intelChooseRenderState(ctx);
      }

      intel->NewGLState = 0;
   }

   intel->tnl_pipeline_running = true;
   _tnl_run_pipeline(ctx);
   intel->tnl_pipeline_running = false;

   _mesa_unlock_context_textures(ctx);
}

static void
intelRenderStart(struct gl_context * ctx)
{
   struct intel_context *intel = intel_context(ctx);

   intel_check_front_buffer_rendering(intel);
   intel->vtbl.render_start(intel_context(ctx));
   intel->vtbl.emit_state(intel);
}

static void
intelRenderFinish(struct gl_context * ctx)
{
   struct intel_context *intel = intel_context(ctx);

   if (intel->RenderIndex & INTEL_FALLBACK_BIT)
      _swrast_flush(ctx);

   INTEL_FIREVERTICES(intel);
}




 /* System to flush dma and emit state changes based on the rasterized
  * primitive.
  */
static void
intelRasterPrimitive(struct gl_context * ctx, GLenum rprim, GLuint hwprim)
{
   struct intel_context *intel = intel_context(ctx);

   if (0)
      fprintf(stderr, "%s %s %x\n", __func__,
              _mesa_enum_to_string(rprim), hwprim);

   intel->vtbl.reduced_primitive_state(intel, rprim);

   /* Start a new primitive.  Arrange to have it flushed later on.
    */
   if (hwprim != intel->prim.primitive) {
      INTEL_FIREVERTICES(intel);

      intel_set_prim(intel, hwprim);
   }
}


 /* 
  */
static void
intelRenderPrimitive(struct gl_context * ctx, GLenum prim)
{
   struct intel_context *intel = intel_context(ctx);
   GLboolean unfilled = (ctx->Polygon.FrontMode != GL_FILL ||
                         ctx->Polygon.BackMode != GL_FILL);

   if (0)
      fprintf(stderr, "%s %s\n", __func__, _mesa_enum_to_string(prim));

   /* Let some clipping routines know which primitive they're dealing
    * with.
    */
   intel->render_primitive = prim;

   /* Shortcircuit this when called for unfilled triangles.  The rasterized
    * primitive will always be reset by lower level functions in that case,
    * potentially pingponging the state:
    */
   if (reduced_prim[prim] == GL_TRIANGLES && unfilled)
      return;

   /* Set some primitive-dependent state and Start? a new primitive.
    */
   intelRasterPrimitive(ctx, reduced_prim[prim], hw_prim[prim]);
}


 /**********************************************************************/
 /*           Transition to/from hardware rasterization.               */
 /**********************************************************************/

static char *fallbackStrings[] = {
   [0] = "Draw buffer",
   [1] = "Read buffer",
   [2] = "Depth buffer",
   [3] = "Stencil buffer",
   [4] = "User disable",
   [5] = "Render mode",

   [12] = "Texture",
   [13] = "Color mask",
   [14] = "Stencil",
   [15] = "Stipple",
   [16] = "Program",
   [17] = "Logic op",
   [18] = "Smooth polygon",
   [19] = "Smooth point",
   [20] = "point sprite coord origin",
   [21] = "depth/color drawing offset",
   [22] = "coord replace(SPRITE POINT ENABLE)",
};


static char *
getFallbackString(GLuint bit)
{
   int i = 0;
   while (bit > 1) {
      i++;
      bit >>= 1;
   }
   return fallbackStrings[i];
}



/**
 * Enable/disable a fallback flag.
 * \param bit  one of INTEL_FALLBACK_x flags.
 */
void
intelFallback(struct intel_context *intel, GLbitfield bit, bool mode)
{
   struct gl_context *ctx = &intel->ctx;
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   const GLbitfield oldfallback = intel->Fallback;

   if (mode) {
      intel->Fallback |= bit;
      if (oldfallback == 0) {
	 assert(!intel->tnl_pipeline_running);

         intel_flush(ctx);
         if (INTEL_DEBUG & DEBUG_PERF)
            fprintf(stderr, "ENTER FALLBACK %x: %s\n",
                    bit, getFallbackString(bit));
         _swsetup_Wakeup(ctx);
         intel->RenderIndex = ~0;
      }
   }
   else {
      intel->Fallback &= ~bit;
      if (oldfallback == bit) {
	 assert(!intel->tnl_pipeline_running);

         _swrast_flush(ctx);
         if (INTEL_DEBUG & DEBUG_PERF)
            fprintf(stderr, "LEAVE FALLBACK %s\n", getFallbackString(bit));
         tnl->Driver.Render.Start = intelRenderStart;
         tnl->Driver.Render.PrimitiveNotify = intelRenderPrimitive;
         tnl->Driver.Render.Finish = intelRenderFinish;
         tnl->Driver.Render.BuildVertices = _tnl_build_vertices;
         tnl->Driver.Render.CopyPV = _tnl_copy_pv;
         tnl->Driver.Render.Interp = _tnl_interp;

         _tnl_invalidate_vertex_state(ctx, ~0);
         _tnl_invalidate_vertices(ctx, ~0);
         _tnl_install_attrs(ctx,
                            intel->vertex_attrs,
                            intel->vertex_attr_count,
                            intel->ViewportMatrix.m, 0);

         intel->NewGLState |= _INTEL_NEW_RENDERSTATE;
      }
   }
}

/**********************************************************************/
/*                            Initialization.                         */
/**********************************************************************/


void
intelInitTriFuncs(struct gl_context * ctx)
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   static int firsttime = 1;

   if (firsttime) {
      init_rast_tab();
      firsttime = 0;
   }

   tnl->Driver.RunPipeline = intelRunPipeline;
   tnl->Driver.Render.Start = intelRenderStart;
   tnl->Driver.Render.Finish = intelRenderFinish;
   tnl->Driver.Render.PrimitiveNotify = intelRenderPrimitive;
   tnl->Driver.Render.ResetLineStipple = _swrast_ResetLineStipple;
   tnl->Driver.Render.BuildVertices = _tnl_build_vertices;
   tnl->Driver.Render.CopyPV = _tnl_copy_pv;
   tnl->Driver.Render.Interp = _tnl_interp;
}
