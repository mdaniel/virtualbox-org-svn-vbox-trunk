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

#ifndef INTELCONTEXT_INC
#define INTELCONTEXT_INC


#include <stdbool.h>
#include <string.h>
#include "main/mtypes.h"

#include <drm.h>
#include <intel_bufmgr.h>
#include <i915_drm.h>

#include "intel_screen.h"
#include "intel_tex_obj.h"

#include "tnl/t_vertex.h"

#define TAG(x) intel##x
#include "tnl_dd/t_dd_vertex.h"
#undef TAG

#define DV_PF_555  (1<<8)
#define DV_PF_565  (2<<8)
#define DV_PF_8888 (3<<8)
#define DV_PF_4444 (8<<8)
#define DV_PF_1555 (9<<8)

struct intel_region;
struct intel_context;

typedef void (*intel_tri_func) (struct intel_context *, intelVertex *,
                                intelVertex *, intelVertex *);
typedef void (*intel_line_func) (struct intel_context *, intelVertex *,
                                 intelVertex *);
typedef void (*intel_point_func) (struct intel_context *, intelVertex *);

/**
 * Bits for intel->Fallback field
 */
/*@{*/
#define INTEL_FALLBACK_DRAW_BUFFER	 0x1
#define INTEL_FALLBACK_READ_BUFFER	 0x2
#define INTEL_FALLBACK_DEPTH_BUFFER      0x4
#define INTEL_FALLBACK_STENCIL_BUFFER    0x8
#define INTEL_FALLBACK_USER		 0x10
#define INTEL_FALLBACK_RENDERMODE	 0x20
#define INTEL_FALLBACK_TEXTURE   	 0x40
#define INTEL_FALLBACK_DRIVER            0x1000  /**< first for drivers */
/*@}*/

extern void intelFallback(struct intel_context *intel, GLbitfield bit,
                          bool mode);
#define FALLBACK( intel, bit, mode ) intelFallback( intel, bit, mode )


#define INTEL_WRITE_PART  0x1
#define INTEL_WRITE_FULL  0x2
#define INTEL_READ        0x4

#ifndef likely
#ifdef __GNUC__
#define likely(expr) (__builtin_expect(expr, 1))
#define unlikely(expr) (__builtin_expect(expr, 0))
#else
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#endif
#endif

struct intel_batchbuffer {
   /** Current batchbuffer being queued up. */
   drm_intel_bo *bo;
   /** Last BO submitted to the hardware.  Used for glFinish(). */
   drm_intel_bo *last_bo;

   uint16_t emit, total;
   uint16_t used, reserved_space;
   uint32_t *map;
   uint32_t *cpu_map;
#define BATCH_SZ (8192*sizeof(uint32_t))
};

/**
 * intel_context is derived from Mesa's context class: struct gl_context.
 */
struct intel_context
{
   struct gl_context ctx;  /**< base class, must be first field */

   struct
   {
      void (*destroy) (struct intel_context * intel);
      void (*emit_state) (struct intel_context * intel);
      void (*finish_batch) (struct intel_context * intel);
      void (*new_batch) (struct intel_context * intel);
      void (*emit_invarient_state) (struct intel_context * intel);
      void (*update_texture_state) (struct intel_context * intel);

      void (*render_start) (struct intel_context * intel);
      void (*render_prevalidate) (struct intel_context * intel);
      void (*set_draw_region) (struct intel_context * intel,
                               struct intel_region * draw_regions[],
                               struct intel_region * depth_region,
			       GLuint num_regions);
      void (*update_draw_buffer)(struct intel_context *intel);

      void (*reduced_primitive_state) (struct intel_context * intel,
                                       GLenum rprim);

      bool (*check_vertex_size) (struct intel_context * intel,
				      GLuint expected);
      void (*invalidate_state) (struct intel_context *intel,
				GLuint new_state);

      void (*assert_not_dirty) (struct intel_context *intel);

      void (*debug_batch)(struct intel_context *intel);
      void (*annotate_aub)(struct intel_context *intel);
      bool (*render_target_supported)(struct intel_context *intel,
				      struct gl_renderbuffer *rb);
   } vtbl;

   GLbitfield Fallback;  /**< mask of INTEL_FALLBACK_x bits */
   GLuint NewGLState;
 
   dri_bufmgr *bufmgr;
   unsigned int maxBatchSize;

   /**
    * Generation number of the hardware: 2 is 8xx, 3 is 9xx pre-965, 4 is 965.
    */
   int gen;
   bool is_945;
   bool has_swizzling;

   struct intel_batchbuffer batch;

   drm_intel_bo *first_post_swapbuffers_batch;
   bool need_throttle;
   bool no_batch_wrap;
   bool tnl_pipeline_running; /**< Set while i915's _tnl_run_pipeline. */

   /**
    * Set if we're either a debug context or the INTEL_DEBUG=perf environment
    * variable is set, this is the flag indicating to do expensive work that
    * might lead to a perf_debug() call.
    */
   bool perf_debug;

   struct
   {
      GLuint id;
      uint32_t start_ptr; /**< for i8xx */
      uint32_t primitive;	/**< Current hardware primitive type */
      void (*flush) (struct intel_context *);
      drm_intel_bo *vb_bo;
      uint8_t *vb;
      unsigned int start_offset; /**< Byte offset of primitive sequence */
      unsigned int current_offset; /**< Byte offset of next vertex */
      unsigned int count;	/**< Number of vertices in current primitive */
   } prim;

   struct {
      drm_intel_bo *bo;
      GLuint offset;
      uint32_t buffer_len;
      uint32_t buffer_offset;
      char buffer[4096];
   } upload;

   uint32_t max_gtt_map_object_size;

   /* Offsets of fields within the current vertex:
    */
   GLuint coloroffset;
   GLuint specoffset;
   GLuint wpos_offset;

   struct tnl_attr_map vertex_attrs[VERT_ATTRIB_MAX];
   GLuint vertex_attr_count;

   GLfloat polygon_offset_scale;        /* dependent on depth_scale, bpp */

   bool hw_stipple;
   bool no_rast;
   bool always_flush_batch;
   bool always_flush_cache;
   bool disable_throttling;

   /* State for intelvb.c and inteltris.c.
    */
   GLuint RenderIndex;
   GLmatrix ViewportMatrix;
   GLenum render_primitive;
   GLenum reduced_primitive; /*< Only gen < 6 */
   GLuint vertex_size;
   GLubyte *verts;              /* points to tnl->clipspace.vertex_buf */

   /* Fallback rasterization functions 
    */
   intel_point_func draw_point;
   intel_line_func draw_line;
   intel_tri_func draw_tri;

   /**
    * Set if rendering has occurred to the drawable's front buffer.
    *
    * This is used in the DRI2 case to detect that glFlush should also copy
    * the contents of the fake front buffer to the real front buffer.
    */
   bool front_buffer_dirty;

   bool use_early_z;

   __DRIcontext *driContext;
   struct intel_screen *intelScreen;

   /**
    * Configuration cache
    */
   driOptionCache optionCache;
};

extern char *__progname;


#define SUBPIXEL_X 0.125
#define SUBPIXEL_Y 0.125

#define INTEL_FIREVERTICES(intel)		\
do {						\
   if ((intel)->prim.flush)			\
      (intel)->prim.flush(intel);		\
} while (0)

/* ================================================================
 * Debugging:
 */
extern int INTEL_DEBUG;

#define DEBUG_TEXTURE	0x1
#define DEBUG_STATE	0x2
#define DEBUG_BLIT	0x8
#define DEBUG_MIPTREE   0x10
#define DEBUG_PERF	0x20
#define DEBUG_BATCH     0x80
#define DEBUG_PIXEL     0x100
#define DEBUG_BUFMGR    0x200
#define DEBUG_REGION    0x400
#define DEBUG_FBO       0x800
#define DEBUG_SYNC	0x2000
#define DEBUG_DRI       0x10000
#define DEBUG_STATS     0x100000
#define DEBUG_WM        0x400000
#define DEBUG_AUB       0x4000000

#ifdef HAVE_ANDROID_PLATFORM
#define LOG_TAG "INTEL-MESA"
#include <cutils/log.h>
#ifndef ALOGW
#define ALOGW LOGW
#endif
#define dbg_printf(...)	ALOGW(__VA_ARGS__)
#else
#define dbg_printf(...)	printf(__VA_ARGS__)
#endif /* HAVE_ANDROID_PLATFORM */

#define DBG(...) do {						\
	if (unlikely(INTEL_DEBUG & FILE_DEBUG_FLAG))		\
		dbg_printf(__VA_ARGS__);			\
} while(0)

#define perf_debug(...) do {					\
   static GLuint msg_id = 0;                                    \
   if (unlikely(INTEL_DEBUG & DEBUG_PERF))                      \
      dbg_printf(__VA_ARGS__);                                  \
   if (intel->perf_debug)                                       \
      _mesa_gl_debug(&intel->ctx, &msg_id,                      \
                     MESA_DEBUG_SOURCE_API,                     \
                     MESA_DEBUG_TYPE_PERFORMANCE,               \
                     MESA_DEBUG_SEVERITY_MEDIUM,                \
                     __VA_ARGS__);                              \
} while(0)

#define WARN_ONCE(cond, fmt...) do {                            \
   if (unlikely(cond)) {                                        \
      static bool _warned = false;                              \
      static GLuint msg_id = 0;                                 \
      if (!_warned) {                                           \
         fprintf(stderr, "WARNING: ");                          \
         fprintf(stderr, fmt);                                  \
         _warned = true;                                        \
                                                                \
         _mesa_gl_debug(ctx, &msg_id,                           \
                        MESA_DEBUG_SOURCE_API,                  \
                        MESA_DEBUG_TYPE_OTHER,                  \
                        MESA_DEBUG_SEVERITY_HIGH, fmt);         \
      }                                                         \
   }                                                            \
} while (0)

/* ================================================================
 * intel_context.c:
 */

extern const char *const i915_vendor_string;

extern const char *i915_get_renderer_string(unsigned deviceID);

extern bool intelInitContext(struct intel_context *intel,
                             int api,
                             unsigned major_version,
                             unsigned minor_version,
                             uint32_t flags,
                             const struct gl_config * mesaVis,
                             __DRIcontext * driContextPriv,
                             void *sharedContextPrivate,
                             struct dd_function_table *functions,
                             unsigned *dri_ctx_error);

extern void intelFinish(struct gl_context * ctx);
extern void intel_flush_rendering_to_batch(struct gl_context *ctx);
extern void _intel_flush(struct gl_context * ctx, const char *file, int line);

#define intel_flush(ctx) _intel_flush(ctx, __FILE__, __LINE__)

extern void intelInitDriverFunctions(struct dd_function_table *functions);

void intel_init_syncobj_functions(struct dd_function_table *functions);


/* ================================================================
 * intel_state.c:
 */

#define COMPAREFUNC_ALWAYS		0
#define COMPAREFUNC_NEVER		0x1
#define COMPAREFUNC_LESS		0x2
#define COMPAREFUNC_EQUAL		0x3
#define COMPAREFUNC_LEQUAL		0x4
#define COMPAREFUNC_GREATER		0x5
#define COMPAREFUNC_NOTEQUAL		0x6
#define COMPAREFUNC_GEQUAL		0x7

#define STENCILOP_KEEP			0
#define STENCILOP_ZERO			0x1
#define STENCILOP_REPLACE		0x2
#define STENCILOP_INCRSAT		0x3
#define STENCILOP_DECRSAT		0x4
#define STENCILOP_INCR			0x5
#define STENCILOP_DECR			0x6
#define STENCILOP_INVERT		0x7

#define LOGICOP_CLEAR			0
#define LOGICOP_NOR			0x1
#define LOGICOP_AND_INV 		0x2
#define LOGICOP_COPY_INV		0x3
#define LOGICOP_AND_RVRSE		0x4
#define LOGICOP_INV			0x5
#define LOGICOP_XOR			0x6
#define LOGICOP_NAND			0x7
#define LOGICOP_AND			0x8
#define LOGICOP_EQUIV			0x9
#define LOGICOP_NOOP			0xa
#define LOGICOP_OR_INV			0xb
#define LOGICOP_COPY			0xc
#define LOGICOP_OR_RVRSE		0xd
#define LOGICOP_OR			0xe
#define LOGICOP_SET			0xf

#define BLENDFACT_ZERO			0x01
#define BLENDFACT_ONE			0x02
#define BLENDFACT_SRC_COLR		0x03
#define BLENDFACT_INV_SRC_COLR 		0x04
#define BLENDFACT_SRC_ALPHA		0x05
#define BLENDFACT_INV_SRC_ALPHA 	0x06
#define BLENDFACT_DST_ALPHA		0x07
#define BLENDFACT_INV_DST_ALPHA 	0x08
#define BLENDFACT_DST_COLR		0x09
#define BLENDFACT_INV_DST_COLR		0x0a
#define BLENDFACT_SRC_ALPHA_SATURATE	0x0b
#define BLENDFACT_CONST_COLOR		0x0c
#define BLENDFACT_INV_CONST_COLOR	0x0d
#define BLENDFACT_CONST_ALPHA		0x0e
#define BLENDFACT_INV_CONST_ALPHA	0x0f
#define BLENDFACT_MASK          	0x0f

enum {
   DRI_CONF_BO_REUSE_DISABLED,
   DRI_CONF_BO_REUSE_ALL
};

extern int intel_translate_shadow_compare_func(GLenum func);
extern int intel_translate_compare_func(GLenum func);
extern int intel_translate_stencil_op(GLenum op);
extern int intel_translate_blend_factor(GLenum factor);
extern int intel_translate_logic_op(GLenum opcode);

void intel_update_renderbuffers(__DRIcontext *context,
				__DRIdrawable *drawable);
void intel_prepare_render(struct intel_context *intel);

void i915_set_buf_info_for_region(uint32_t *state, struct intel_region *region,
				  uint32_t buffer_id);
void intel_init_texture_formats(struct gl_context *ctx);

/*======================================================================
 * Inline conversion functions.  
 * These are better-typed than the macros used previously:
 */
static inline struct intel_context *
intel_context(struct gl_context * ctx)
{
   return (struct intel_context *) ctx;
}

#endif
