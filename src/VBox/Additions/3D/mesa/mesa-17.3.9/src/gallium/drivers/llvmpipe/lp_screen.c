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


#include "util/u_memory.h"
#include "util/u_math.h"
#include "util/u_cpu_detect.h"
#include "util/u_format.h"
#include "util/u_string.h"
#include "util/u_format_s3tc.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "draw/draw_context.h"
#include "gallivm/lp_bld_type.h"

#include "os/os_misc.h"
#include "os/os_time.h"
#include "lp_texture.h"
#include "lp_fence.h"
#include "lp_jit.h"
#include "lp_screen.h"
#include "lp_context.h"
#include "lp_debug.h"
#include "lp_public.h"
#include "lp_limits.h"
#include "lp_rast.h"

#include "state_tracker/sw_winsys.h"

#ifdef DEBUG
int LP_DEBUG = 0;

static const struct debug_named_value lp_debug_flags[] = {
   { "pipe",   DEBUG_PIPE, NULL },
   { "tgsi",   DEBUG_TGSI, NULL },
   { "tex",    DEBUG_TEX, NULL },
   { "setup",  DEBUG_SETUP, NULL },
   { "rast",   DEBUG_RAST, NULL },
   { "query",  DEBUG_QUERY, NULL },
   { "screen", DEBUG_SCREEN, NULL },
   { "counters", DEBUG_COUNTERS, NULL },
   { "scene", DEBUG_SCENE, NULL },
   { "fence", DEBUG_FENCE, NULL },
   { "mem", DEBUG_MEM, NULL },
   { "fs", DEBUG_FS, NULL },
   DEBUG_NAMED_VALUE_END
};
#endif

int LP_PERF = 0;
static const struct debug_named_value lp_perf_flags[] = {
   { "texmem",         PERF_TEX_MEM, NULL },
   { "no_mipmap",      PERF_NO_MIPMAPS, NULL },
   { "no_linear",      PERF_NO_LINEAR, NULL },
   { "no_mip_linear",  PERF_NO_MIP_LINEAR, NULL },
   { "no_tex",         PERF_NO_TEX, NULL },
   { "no_blend",       PERF_NO_BLEND, NULL },
   { "no_depth",       PERF_NO_DEPTH, NULL },
   { "no_alphatest",   PERF_NO_ALPHATEST, NULL },
   DEBUG_NAMED_VALUE_END
};


static const char *
llvmpipe_get_vendor(struct pipe_screen *screen)
{
   return "VMware, Inc.";
}


static const char *
llvmpipe_get_name(struct pipe_screen *screen)
{
   static char buf[100];
   util_snprintf(buf, sizeof(buf), "llvmpipe (LLVM %u.%u, %u bits)",
		 HAVE_LLVM >> 8, HAVE_LLVM & 0xff,
		 lp_native_vector_width );
   return buf;
}


static int
llvmpipe_get_param(struct pipe_screen *screen, enum pipe_cap param)
{
   switch (param) {
   case PIPE_CAP_NPOT_TEXTURES:
   case PIPE_CAP_MIXED_FRAMEBUFFER_SIZES:
   case PIPE_CAP_MIXED_COLOR_DEPTH_BITS:
      return 1;
   case PIPE_CAP_TWO_SIDED_STENCIL:
      return 1;
   case PIPE_CAP_SM3:
      return 1;
   case PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS:
      return 1;
   case PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS:
      return PIPE_MAX_SO_BUFFERS;
   case PIPE_CAP_ANISOTROPIC_FILTER:
      return 0;
   case PIPE_CAP_POINT_SPRITE:
      return 1;
   case PIPE_CAP_MAX_RENDER_TARGETS:
      return PIPE_MAX_COLOR_BUFS;
   case PIPE_CAP_OCCLUSION_QUERY:
      return 1;
   case PIPE_CAP_QUERY_TIME_ELAPSED:
      return 0;
   case PIPE_CAP_QUERY_TIMESTAMP:
      return 1;
   case PIPE_CAP_QUERY_PIPELINE_STATISTICS:
      return 1;
   case PIPE_CAP_TEXTURE_MIRROR_CLAMP:
      return 1;
   case PIPE_CAP_TEXTURE_SHADOW_MAP:
      return 1;
   case PIPE_CAP_TEXTURE_SWIZZLE:
      return 1;
   case PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK:
      return 0;
   case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
      return LP_MAX_TEXTURE_2D_LEVELS;
   case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
      return LP_MAX_TEXTURE_3D_LEVELS;
   case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
      return LP_MAX_TEXTURE_CUBE_LEVELS;
   case PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS:
      return LP_MAX_TEXTURE_ARRAY_LAYERS;
   case PIPE_CAP_BLEND_EQUATION_SEPARATE:
      return 1;
   case PIPE_CAP_INDEP_BLEND_ENABLE:
      return 1;
   case PIPE_CAP_INDEP_BLEND_FUNC:
      return 1;
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
      return 1;
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT:
      return 0;
   case PIPE_CAP_PRIMITIVE_RESTART:
      return 1;
   case PIPE_CAP_DEPTH_CLIP_DISABLE:
      return 1;
   case PIPE_CAP_SHADER_STENCIL_EXPORT:
      return 1;
   case PIPE_CAP_TGSI_INSTANCEID:
   case PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR:
   case PIPE_CAP_START_INSTANCE:
      return 1;
   case PIPE_CAP_FRAGMENT_COLOR_CLAMPED:
      return 0;
   case PIPE_CAP_MIXED_COLORBUFFER_FORMATS:
      return 1;
   case PIPE_CAP_SEAMLESS_CUBE_MAP:
   case PIPE_CAP_SEAMLESS_CUBE_MAP_PER_TEXTURE:
      return 1;
   /* this is a lie could support arbitrary large offsets */
   case PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET:
   case PIPE_CAP_MIN_TEXEL_OFFSET:
      return -32;
   case PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET:
   case PIPE_CAP_MAX_TEXEL_OFFSET:
      return 31;
   case PIPE_CAP_CONDITIONAL_RENDER:
      return 1;
   case PIPE_CAP_TEXTURE_BARRIER:
      return 0;
   case PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS:
   case PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS:
      return 16*4;
   case PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES:
   case PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
      return 1024;
   case PIPE_CAP_MAX_VERTEX_STREAMS:
      return 1;
   case PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE:
      return 2048;
   case PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME:
   case PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS:
      return 1;
   case PIPE_CAP_TGSI_CAN_COMPACT_CONSTANTS:
      return 0;
   case PIPE_CAP_VERTEX_COLOR_UNCLAMPED:
   case PIPE_CAP_VERTEX_COLOR_CLAMPED:
      return 1;
   case PIPE_CAP_GLSL_FEATURE_LEVEL:
      return 330;
   case PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION:
      return 0;
   case PIPE_CAP_COMPUTE:
      return 0;
   case PIPE_CAP_USER_VERTEX_BUFFERS:
      return 1;
   case PIPE_CAP_USER_CONSTANT_BUFFERS:
      return 0;
   case PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_TGSI_TEXCOORD:
      return 0;
   case PIPE_CAP_DRAW_INDIRECT:
      return 1;

   case PIPE_CAP_CUBE_MAP_ARRAY:
      return 1;
   case PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT:
      return 16;
   case PIPE_CAP_TEXTURE_MULTISAMPLE:
      return 0;
   case PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT:
      return 64;
   case PIPE_CAP_TEXTURE_BUFFER_OBJECTS:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_BUFFER_SIZE:
      return 65536;
   case PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
      return 1;
   case PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER:
      return 0;
   case PIPE_CAP_MAX_VIEWPORTS:
      return PIPE_MAX_VIEWPORTS;
   case PIPE_CAP_ENDIANNESS:
      return PIPE_ENDIAN_NATIVE;
   case PIPE_CAP_TGSI_VS_LAYER_VIEWPORT:
      return 1;
   case PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS:
      return 4;
   case PIPE_CAP_TEXTURE_GATHER_SM5:
   case PIPE_CAP_SAMPLE_SHADING:
   case PIPE_CAP_TEXTURE_GATHER_OFFSETS:
      return 0;
   case PIPE_CAP_TGSI_VS_WINDOW_SPACE_POSITION:
      return 1;
   case PIPE_CAP_TGSI_FS_FINE_DERIVATIVE:
   case PIPE_CAP_TGSI_TEX_TXF_LZ:
      return 0;
   case PIPE_CAP_SAMPLER_VIEW_TARGET:
      return 1;
   case PIPE_CAP_FAKE_SW_MSAA:
      return 1;
   case PIPE_CAP_TEXTURE_QUERY_LOD:
   case PIPE_CAP_CONDITIONAL_RENDER_INVERTED:
   case PIPE_CAP_TGSI_ARRAY_COMPONENTS:
   case PIPE_CAP_DOUBLES:
   case PIPE_CAP_INT64:
   case PIPE_CAP_INT64_DIVMOD:
   case PIPE_CAP_QUERY_SO_OVERFLOW:
      return 1;

   case PIPE_CAP_VENDOR_ID:
      return 0xFFFFFFFF;
   case PIPE_CAP_DEVICE_ID:
      return 0xFFFFFFFF;
   case PIPE_CAP_ACCELERATED:
      return 0;
   case PIPE_CAP_VIDEO_MEMORY: {
      /* XXX: Do we want to return the full amount fo system memory ? */
      uint64_t system_memory;

      if (!os_get_total_physical_memory(&system_memory))
         return 0;

      if (sizeof(void *) == 4)
         /* Cap to 2 GB on 32 bits system. We do this because llvmpipe does
          * eat application memory, which is quite limited on 32 bits. App
          * shouldn't expect too much available memory. */
         system_memory = MIN2(system_memory, 2048 << 20);

      return (int)(system_memory >> 20);
   }
   case PIPE_CAP_UMA:
      return 0;
   case PIPE_CAP_CLIP_HALFZ:
      return 1;
   case PIPE_CAP_VERTEXID_NOBASE:
      return 0;
   case PIPE_CAP_POLYGON_OFFSET_CLAMP:
   case PIPE_CAP_TEXTURE_FLOAT_LINEAR:
   case PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR:
      return 1;
   case PIPE_CAP_CULL_DISTANCE:
      return 1;
   case PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS:
      return 1;
   case PIPE_CAP_CLEAR_TEXTURE:
      return 1;
   case PIPE_CAP_MULTISAMPLE_Z_RESOLVE:
   case PIPE_CAP_RESOURCE_FROM_USER_MEMORY:
   case PIPE_CAP_DEVICE_RESET_STATUS_QUERY:
   case PIPE_CAP_MAX_SHADER_PATCH_VARYINGS:
   case PIPE_CAP_DEPTH_BOUNDS_TEST:
   case PIPE_CAP_TGSI_TXQS:
   case PIPE_CAP_FORCE_PERSAMPLE_INTERP:
   case PIPE_CAP_SHAREABLE_SHADERS:
   case PIPE_CAP_DRAW_PARAMETERS:
   case PIPE_CAP_TGSI_PACK_HALF_FLOAT:
   case PIPE_CAP_MULTI_DRAW_INDIRECT:
   case PIPE_CAP_MULTI_DRAW_INDIRECT_PARAMS:
   case PIPE_CAP_TGSI_FS_POSITION_IS_SYSVAL:
   case PIPE_CAP_TGSI_FS_FACE_IS_INTEGER_SYSVAL:
   case PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT:
   case PIPE_CAP_INVALIDATE_BUFFER:
   case PIPE_CAP_GENERATE_MIPMAP:
   case PIPE_CAP_STRING_MARKER:
   case PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY:
   case PIPE_CAP_SURFACE_REINTERPRET_BLOCKS:
   case PIPE_CAP_QUERY_BUFFER_OBJECT:
   case PIPE_CAP_QUERY_MEMORY_INFO:
   case PIPE_CAP_PCI_GROUP:
   case PIPE_CAP_PCI_BUS:
   case PIPE_CAP_PCI_DEVICE:
   case PIPE_CAP_PCI_FUNCTION:
   case PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT:
   case PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR:
   case PIPE_CAP_PRIMITIVE_RESTART_FOR_PATCHES:
   case PIPE_CAP_TGSI_VOTE:
   case PIPE_CAP_MAX_WINDOW_RECTANGLES:
   case PIPE_CAP_POLYGON_OFFSET_UNITS_UNSCALED:
   case PIPE_CAP_VIEWPORT_SUBPIXEL_BITS:
   case PIPE_CAP_TGSI_CAN_READ_OUTPUTS:
   case PIPE_CAP_NATIVE_FENCE_FD:
   case PIPE_CAP_GLSL_OPTIMIZE_CONSERVATIVELY:
   case PIPE_CAP_TGSI_FS_FBFETCH:
   case PIPE_CAP_TGSI_MUL_ZERO_WINS:
   case PIPE_CAP_TGSI_CLOCK:
   case PIPE_CAP_POLYGON_MODE_FILL_RECTANGLE:
   case PIPE_CAP_SPARSE_BUFFER_PAGE_SIZE:
   case PIPE_CAP_TGSI_BALLOT:
   case PIPE_CAP_TGSI_TES_LAYER_VIEWPORT:
   case PIPE_CAP_CAN_BIND_CONST_BUFFER_AS_VERTEX:
   case PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION:
   case PIPE_CAP_POST_DEPTH_COVERAGE:
   case PIPE_CAP_BINDLESS_TEXTURE:
   case PIPE_CAP_NIR_SAMPLERS_AS_DEREF:
   case PIPE_CAP_MEMOBJ:
   case PIPE_CAP_LOAD_CONSTBUF:
   case PIPE_CAP_TGSI_ANY_REG_AS_ADDRESS:
   case PIPE_CAP_TILE_RASTER_ORDER:
      return 0;
   }
   /* should only get here on unhandled cases */
   debug_printf("Unexpected PIPE_CAP %d query\n", param);
   return 0;
}

static int
llvmpipe_get_shader_param(struct pipe_screen *screen,
                          enum pipe_shader_type shader,
                          enum pipe_shader_cap param)
{
   switch(shader)
   {
   case PIPE_SHADER_FRAGMENT:
      switch (param) {
      default:
         return gallivm_get_shader_param(param);
      }
   case PIPE_SHADER_VERTEX:
   case PIPE_SHADER_GEOMETRY:
      switch (param) {
      case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
         /* At this time, the draw module and llvmpipe driver only
          * support vertex shader texture lookups when LLVM is enabled in
          * the draw module.
          */
         if (debug_get_bool_option("DRAW_USE_LLVM", TRUE))
            return PIPE_MAX_SAMPLERS;
         else
            return 0;
      case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
         if (debug_get_bool_option("DRAW_USE_LLVM", TRUE))
            return PIPE_MAX_SHADER_SAMPLER_VIEWS;
         else
            return 0;
      default:
         return draw_get_shader_param(shader, param);
      }
   default:
      return 0;
   }
}

static float
llvmpipe_get_paramf(struct pipe_screen *screen, enum pipe_capf param)
{
   switch (param) {
   case PIPE_CAPF_MAX_LINE_WIDTH:
      /* fall-through */
   case PIPE_CAPF_MAX_LINE_WIDTH_AA:
      return 255.0; /* arbitrary */
   case PIPE_CAPF_MAX_POINT_WIDTH:
      /* fall-through */
   case PIPE_CAPF_MAX_POINT_WIDTH_AA:
      return 255.0; /* arbitrary */
   case PIPE_CAPF_MAX_TEXTURE_ANISOTROPY:
      return 16.0; /* not actually signficant at this time */
   case PIPE_CAPF_MAX_TEXTURE_LOD_BIAS:
      return 16.0; /* arbitrary */
   case PIPE_CAPF_GUARD_BAND_LEFT:
   case PIPE_CAPF_GUARD_BAND_TOP:
   case PIPE_CAPF_GUARD_BAND_RIGHT:
   case PIPE_CAPF_GUARD_BAND_BOTTOM:
      return 0.0;
   }
   /* should only get here on unhandled cases */
   debug_printf("Unexpected PIPE_CAP %d query\n", param);
   return 0.0;
}


/**
 * Query format support for creating a texture, drawing surface, etc.
 * \param format  the format to test
 * \param type  one of PIPE_TEXTURE, PIPE_SURFACE
 */
static boolean
llvmpipe_is_format_supported( struct pipe_screen *_screen,
                              enum pipe_format format,
                              enum pipe_texture_target target,
                              unsigned sample_count,
                              unsigned bind)
{
   struct llvmpipe_screen *screen = llvmpipe_screen(_screen);
   struct sw_winsys *winsys = screen->winsys;
   const struct util_format_description *format_desc;

   format_desc = util_format_description(format);
   if (!format_desc)
      return FALSE;

   assert(target == PIPE_BUFFER ||
          target == PIPE_TEXTURE_1D ||
          target == PIPE_TEXTURE_1D_ARRAY ||
          target == PIPE_TEXTURE_2D ||
          target == PIPE_TEXTURE_2D_ARRAY ||
          target == PIPE_TEXTURE_RECT ||
          target == PIPE_TEXTURE_3D ||
          target == PIPE_TEXTURE_CUBE ||
          target == PIPE_TEXTURE_CUBE_ARRAY);

   if (sample_count > 1)
      return FALSE;

   if (bind & PIPE_BIND_RENDER_TARGET) {
      if (format_desc->colorspace == UTIL_FORMAT_COLORSPACE_SRGB) {
         /* this is a lie actually other formats COULD exist where we would fail */
         if (format_desc->nr_channels < 3)
            return FALSE;
      }
      else if (format_desc->colorspace != UTIL_FORMAT_COLORSPACE_RGB)
         return FALSE;

      if (format_desc->layout != UTIL_FORMAT_LAYOUT_PLAIN &&
          format != PIPE_FORMAT_R11G11B10_FLOAT)
         return FALSE;

      assert(format_desc->block.width == 1);
      assert(format_desc->block.height == 1);

      if (format_desc->is_mixed)
         return FALSE;

      if (!format_desc->is_array && !format_desc->is_bitmask &&
          format != PIPE_FORMAT_R11G11B10_FLOAT)
         return FALSE;
   }

   if ((bind & (PIPE_BIND_RENDER_TARGET | PIPE_BIND_SAMPLER_VIEW)) &&
       ((bind & PIPE_BIND_DISPLAY_TARGET) == 0)) {
      /* Disable all 3-channel formats, where channel size != 32 bits.
       * In some cases we run into crashes (in generate_unswizzled_blend()),
       * for 3-channel RGB16 variants, there was an apparent LLVM bug.
       * In any case, disabling the shallower 3-channel formats avoids a
       * number of issues with GL_ARB_copy_image support.
       */
      if (format_desc->is_array &&
          format_desc->nr_channels == 3 &&
          format_desc->block.bits != 96) {
         return FALSE;
      }
   }

   if (bind & PIPE_BIND_DISPLAY_TARGET) {
      if(!winsys->is_displaytarget_format_supported(winsys, bind, format))
         return FALSE;
   }

   if (bind & PIPE_BIND_DEPTH_STENCIL) {
      if (format_desc->layout != UTIL_FORMAT_LAYOUT_PLAIN)
         return FALSE;

      if (format_desc->colorspace != UTIL_FORMAT_COLORSPACE_ZS)
         return FALSE;

      /* TODO: Support stencil-only formats */
      if (format_desc->swizzle[0] == PIPE_SWIZZLE_NONE) {
         return FALSE;
      }
   }

   if (format_desc->layout == UTIL_FORMAT_LAYOUT_BPTC ||
       format_desc->layout == UTIL_FORMAT_LAYOUT_ASTC) {
      /* Software decoding is not hooked up. */
      return FALSE;
   }

   if (format_desc->layout == UTIL_FORMAT_LAYOUT_ETC &&
       format != PIPE_FORMAT_ETC1_RGB8)
      return FALSE;

   /*
    * Everything can be supported by u_format
    * (those without fetch_rgba_float might be not but shouldn't hit that)
    */

   return TRUE;
}




static void
llvmpipe_flush_frontbuffer(struct pipe_screen *_screen,
                           struct pipe_resource *resource,
                           unsigned level, unsigned layer,
                           void *context_private,
                           struct pipe_box *sub_box)
{
   struct llvmpipe_screen *screen = llvmpipe_screen(_screen);
   struct sw_winsys *winsys = screen->winsys;
   struct llvmpipe_resource *texture = llvmpipe_resource(resource);

   assert(texture->dt);
   if (texture->dt)
      winsys->displaytarget_display(winsys, texture->dt, context_private, sub_box);
}

static void
llvmpipe_destroy_screen( struct pipe_screen *_screen )
{
   struct llvmpipe_screen *screen = llvmpipe_screen(_screen);
   struct sw_winsys *winsys = screen->winsys;

   if (screen->rast)
      lp_rast_destroy(screen->rast);

   lp_jit_screen_cleanup(screen);

   if(winsys->destroy)
      winsys->destroy(winsys);

   mtx_destroy(&screen->rast_mutex);

   FREE(screen);
}




/**
 * Fence reference counting.
 */
static void
llvmpipe_fence_reference(struct pipe_screen *screen,
                         struct pipe_fence_handle **ptr,
                         struct pipe_fence_handle *fence)
{
   struct lp_fence **old = (struct lp_fence **) ptr;
   struct lp_fence *f = (struct lp_fence *) fence;

   lp_fence_reference(old, f);
}


/**
 * Wait for the fence to finish.
 */
static boolean
llvmpipe_fence_finish(struct pipe_screen *screen,
                      struct pipe_context *ctx,
                      struct pipe_fence_handle *fence_handle,
                      uint64_t timeout)
{
   struct lp_fence *f = (struct lp_fence *) fence_handle;

   if (!timeout)
      return lp_fence_signalled(f);

   lp_fence_wait(f);
   return TRUE;
}

static uint64_t
llvmpipe_get_timestamp(struct pipe_screen *_screen)
{
   return os_time_get_nano();
}

/**
 * Create a new pipe_screen object
 * Note: we're not presently subclassing pipe_screen (no llvmpipe_screen).
 */
struct pipe_screen *
llvmpipe_create_screen(struct sw_winsys *winsys)
{
   struct llvmpipe_screen *screen;

   util_cpu_detect();

#ifdef DEBUG
   LP_DEBUG = debug_get_flags_option("LP_DEBUG", lp_debug_flags, 0 );
#endif

   LP_PERF = debug_get_flags_option("LP_PERF", lp_perf_flags, 0 );

   screen = CALLOC_STRUCT(llvmpipe_screen);
   if (!screen)
      return NULL;

   if (!lp_jit_screen_init(screen)) {
      FREE(screen);
      return NULL;
   }

   screen->winsys = winsys;

   screen->base.destroy = llvmpipe_destroy_screen;

   screen->base.get_name = llvmpipe_get_name;
   screen->base.get_vendor = llvmpipe_get_vendor;
   screen->base.get_device_vendor = llvmpipe_get_vendor; // TODO should be the CPU vendor
   screen->base.get_param = llvmpipe_get_param;
   screen->base.get_shader_param = llvmpipe_get_shader_param;
   screen->base.get_paramf = llvmpipe_get_paramf;
   screen->base.is_format_supported = llvmpipe_is_format_supported;

   screen->base.context_create = llvmpipe_create_context;
   screen->base.flush_frontbuffer = llvmpipe_flush_frontbuffer;
   screen->base.fence_reference = llvmpipe_fence_reference;
   screen->base.fence_finish = llvmpipe_fence_finish;

   screen->base.get_timestamp = llvmpipe_get_timestamp;

   llvmpipe_init_screen_resource_funcs(&screen->base);

   screen->num_threads = util_cpu_caps.nr_cpus > 1 ? util_cpu_caps.nr_cpus : 0;
#ifdef PIPE_SUBSYSTEM_EMBEDDED
   screen->num_threads = 0;
#endif
   screen->num_threads = debug_get_num_option("LP_NUM_THREADS", screen->num_threads);
   screen->num_threads = MIN2(screen->num_threads, LP_MAX_THREADS);

   screen->rast = lp_rast_create(screen->num_threads);
   if (!screen->rast) {
      lp_jit_screen_cleanup(screen);
      FREE(screen);
      return NULL;
   }
   (void) mtx_init(&screen->rast_mutex, mtx_plain);

   return &screen->base;
}
