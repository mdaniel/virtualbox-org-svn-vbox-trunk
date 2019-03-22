/*
 * Copyright 2010 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <xf86drm.h>
#include <nouveau_drm.h>
#include <nvif/class.h>
#include "util/u_format.h"
#include "util/u_format_s3tc.h"
#include "pipe/p_screen.h"

#include "nouveau_vp3_video.h"

#include "nvc0/nvc0_context.h"
#include "nvc0/nvc0_screen.h"

#include "nvc0/mme/com9097.mme.h"
#include "nvc0/mme/com90c0.mme.h"

#include "nv50/g80_texture.xml.h"

static boolean
nvc0_screen_is_format_supported(struct pipe_screen *pscreen,
                                enum pipe_format format,
                                enum pipe_texture_target target,
                                unsigned sample_count,
                                unsigned bindings)
{
   const struct util_format_description *desc = util_format_description(format);

   if (sample_count > 8)
      return false;
   if (!(0x117 & (1 << sample_count))) /* 0, 1, 2, 4 or 8 */
      return false;

   /* Short-circuit the rest of the logic -- this is used by the state tracker
    * to determine valid MS levels in a no-attachments scenario.
    */
   if (format == PIPE_FORMAT_NONE && bindings & PIPE_BIND_RENDER_TARGET)
      return true;

   if (!util_format_is_supported(format, bindings))
      return false;

   if ((bindings & PIPE_BIND_SAMPLER_VIEW) && (target != PIPE_BUFFER))
      if (util_format_get_blocksizebits(format) == 3 * 32)
         return false;

   if (bindings & PIPE_BIND_LINEAR)
      if (util_format_is_depth_or_stencil(format) ||
          (target != PIPE_TEXTURE_1D &&
           target != PIPE_TEXTURE_2D &&
           target != PIPE_TEXTURE_RECT) ||
          sample_count > 1)
         return false;

   /* Restrict ETC2 and ASTC formats here. These are only supported on GK20A.
    */
   if ((desc->layout == UTIL_FORMAT_LAYOUT_ETC ||
        desc->layout == UTIL_FORMAT_LAYOUT_ASTC) &&
       /* The claim is that this should work on GM107 but it doesn't. Need to
        * test further and figure out if it's a nouveau issue or a HW one.
       nouveau_screen(pscreen)->class_3d < GM107_3D_CLASS &&
        */
       nouveau_screen(pscreen)->class_3d != NVEA_3D_CLASS)
      return false;

   /* shared is always supported */
   bindings &= ~(PIPE_BIND_LINEAR |
                 PIPE_BIND_SHARED);

   if (bindings & PIPE_BIND_SHADER_IMAGE) {
      if (sample_count > 1 &&
          nouveau_screen(pscreen)->class_3d >= GM107_3D_CLASS) {
         /* MS images are currently unsupported on Maxwell because they have to
          * be handled explicitly. */
         return false;
      }

      if (format == PIPE_FORMAT_B8G8R8A8_UNORM &&
          nouveau_screen(pscreen)->class_3d < NVE4_3D_CLASS) {
         /* This should work on Fermi, but for currently unknown reasons it
          * does not and results in breaking reads from pbos. */
         return false;
      }
   }

   return (( nvc0_format_table[format].usage |
            nvc0_vertex_format[format].usage) & bindings) == bindings;
}

static int
nvc0_screen_get_param(struct pipe_screen *pscreen, enum pipe_cap param)
{
   const uint16_t class_3d = nouveau_screen(pscreen)->class_3d;
   struct nouveau_device *dev = nouveau_screen(pscreen)->device;

   switch (param) {
   /* non-boolean caps */
   case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
   case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
      return 15;
   case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
      return 12;
   case PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS:
      return 2048;
   case PIPE_CAP_MIN_TEXEL_OFFSET:
      return -8;
   case PIPE_CAP_MAX_TEXEL_OFFSET:
      return 7;
   case PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET:
      return -32;
   case PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET:
      return 31;
   case PIPE_CAP_MAX_TEXTURE_BUFFER_SIZE:
      return 128 * 1024 * 1024;
   case PIPE_CAP_GLSL_FEATURE_LEVEL:
      return 430;
   case PIPE_CAP_MAX_RENDER_TARGETS:
      return 8;
   case PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS:
      return 1;
   case PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS:
      return 4;
   case PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS:
   case PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS:
      return 128;
   case PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES:
   case PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
      return 1024;
   case PIPE_CAP_MAX_VERTEX_STREAMS:
      return 4;
   case PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE:
      return 2048;
   case PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT:
      return 256;
   case PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
      if (class_3d < GM107_3D_CLASS)
         return 256; /* IMAGE bindings require alignment to 256 */
      return 16;
   case PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT:
      return 16;
   case PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT:
      return NOUVEAU_MIN_BUFFER_MAP_ALIGN;
   case PIPE_CAP_MAX_VIEWPORTS:
      return NVC0_MAX_VIEWPORTS;
   case PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS:
      return 4;
   case PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK:
      return PIPE_QUIRK_TEXTURE_BORDER_COLOR_SWIZZLE_NV50;
   case PIPE_CAP_ENDIANNESS:
      return PIPE_ENDIAN_LITTLE;
   case PIPE_CAP_MAX_SHADER_PATCH_VARYINGS:
      return 30;
   case PIPE_CAP_MAX_WINDOW_RECTANGLES:
      return NVC0_MAX_WINDOW_RECTANGLES;

   /* supported caps */
   case PIPE_CAP_TEXTURE_MIRROR_CLAMP:
   case PIPE_CAP_TEXTURE_SWIZZLE:
   case PIPE_CAP_TEXTURE_SHADOW_MAP:
   case PIPE_CAP_NPOT_TEXTURES:
   case PIPE_CAP_MIXED_FRAMEBUFFER_SIZES:
   case PIPE_CAP_MIXED_COLOR_DEPTH_BITS:
   case PIPE_CAP_ANISOTROPIC_FILTER:
   case PIPE_CAP_SEAMLESS_CUBE_MAP:
   case PIPE_CAP_CUBE_MAP_ARRAY:
   case PIPE_CAP_TEXTURE_BUFFER_OBJECTS:
   case PIPE_CAP_TEXTURE_MULTISAMPLE:
   case PIPE_CAP_TWO_SIDED_STENCIL:
   case PIPE_CAP_DEPTH_CLIP_DISABLE:
   case PIPE_CAP_POINT_SPRITE:
   case PIPE_CAP_TGSI_TEXCOORD:
   case PIPE_CAP_SM3:
   case PIPE_CAP_FRAGMENT_COLOR_CLAMPED:
   case PIPE_CAP_VERTEX_COLOR_UNCLAMPED:
   case PIPE_CAP_VERTEX_COLOR_CLAMPED:
   case PIPE_CAP_QUERY_TIMESTAMP:
   case PIPE_CAP_QUERY_TIME_ELAPSED:
   case PIPE_CAP_OCCLUSION_QUERY:
   case PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME:
   case PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS:
   case PIPE_CAP_QUERY_PIPELINE_STATISTICS:
   case PIPE_CAP_BLEND_EQUATION_SEPARATE:
   case PIPE_CAP_INDEP_BLEND_ENABLE:
   case PIPE_CAP_INDEP_BLEND_FUNC:
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
   case PIPE_CAP_PRIMITIVE_RESTART:
   case PIPE_CAP_TGSI_INSTANCEID:
   case PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR:
   case PIPE_CAP_MIXED_COLORBUFFER_FORMATS:
   case PIPE_CAP_CONDITIONAL_RENDER:
   case PIPE_CAP_TEXTURE_BARRIER:
   case PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION:
   case PIPE_CAP_START_INSTANCE:
   case PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT:
   case PIPE_CAP_DRAW_INDIRECT:
   case PIPE_CAP_USER_CONSTANT_BUFFERS:
   case PIPE_CAP_USER_VERTEX_BUFFERS:
   case PIPE_CAP_TEXTURE_QUERY_LOD:
   case PIPE_CAP_SAMPLE_SHADING:
   case PIPE_CAP_TEXTURE_GATHER_OFFSETS:
   case PIPE_CAP_TEXTURE_GATHER_SM5:
   case PIPE_CAP_TGSI_FS_FINE_DERIVATIVE:
   case PIPE_CAP_CONDITIONAL_RENDER_INVERTED:
   case PIPE_CAP_SAMPLER_VIEW_TARGET:
   case PIPE_CAP_CLIP_HALFZ:
   case PIPE_CAP_POLYGON_OFFSET_CLAMP:
   case PIPE_CAP_MULTISAMPLE_Z_RESOLVE:
   case PIPE_CAP_TEXTURE_FLOAT_LINEAR:
   case PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR:
   case PIPE_CAP_DEPTH_BOUNDS_TEST:
   case PIPE_CAP_TGSI_TXQS:
   case PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS:
   case PIPE_CAP_FORCE_PERSAMPLE_INTERP:
   case PIPE_CAP_SHAREABLE_SHADERS:
   case PIPE_CAP_CLEAR_TEXTURE:
   case PIPE_CAP_DRAW_PARAMETERS:
   case PIPE_CAP_TGSI_PACK_HALF_FLOAT:
   case PIPE_CAP_MULTI_DRAW_INDIRECT:
   case PIPE_CAP_MULTI_DRAW_INDIRECT_PARAMS:
   case PIPE_CAP_TGSI_FS_FACE_IS_INTEGER_SYSVAL:
   case PIPE_CAP_QUERY_BUFFER_OBJECT:
   case PIPE_CAP_INVALIDATE_BUFFER:
   case PIPE_CAP_STRING_MARKER:
   case PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT:
   case PIPE_CAP_CULL_DISTANCE:
   case PIPE_CAP_PRIMITIVE_RESTART_FOR_PATCHES:
   case PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR:
   case PIPE_CAP_TGSI_VOTE:
   case PIPE_CAP_POLYGON_OFFSET_UNITS_UNSCALED:
   case PIPE_CAP_TGSI_ARRAY_COMPONENTS:
   case PIPE_CAP_TGSI_MUL_ZERO_WINS:
   case PIPE_CAP_DOUBLES:
   case PIPE_CAP_INT64:
   case PIPE_CAP_TGSI_TEX_TXF_LZ:
   case PIPE_CAP_TGSI_CLOCK:
   case PIPE_CAP_COMPUTE:
   case PIPE_CAP_CAN_BIND_CONST_BUFFER_AS_VERTEX:
   case PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION:
      return 1;
   case PIPE_CAP_SEAMLESS_CUBE_MAP_PER_TEXTURE:
      return (class_3d >= NVE4_3D_CLASS) ? 1 : 0;
   case PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER:
      return nouveau_screen(pscreen)->vram_domain & NOUVEAU_BO_VRAM ? 1 : 0;
   case PIPE_CAP_TGSI_FS_FBFETCH:
      return class_3d >= NVE4_3D_CLASS; /* needs testing on fermi */
   case PIPE_CAP_POLYGON_MODE_FILL_RECTANGLE:
   case PIPE_CAP_TGSI_VS_LAYER_VIEWPORT:
   case PIPE_CAP_TGSI_TES_LAYER_VIEWPORT:
   case PIPE_CAP_POST_DEPTH_COVERAGE:
      return class_3d >= GM200_3D_CLASS;
   case PIPE_CAP_TGSI_BALLOT:
      return class_3d >= NVE4_3D_CLASS;

   /* unsupported caps */
   case PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT:
   case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER:
   case PIPE_CAP_SHADER_STENCIL_EXPORT:
   case PIPE_CAP_TGSI_CAN_COMPACT_CONSTANTS:
   case PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY:
   case PIPE_CAP_FAKE_SW_MSAA:
   case PIPE_CAP_TGSI_VS_WINDOW_SPACE_POSITION:
   case PIPE_CAP_VERTEXID_NOBASE:
   case PIPE_CAP_RESOURCE_FROM_USER_MEMORY:
   case PIPE_CAP_DEVICE_RESET_STATUS_QUERY:
   case PIPE_CAP_TGSI_FS_POSITION_IS_SYSVAL:
   case PIPE_CAP_GENERATE_MIPMAP:
   case PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY:
   case PIPE_CAP_SURFACE_REINTERPRET_BLOCKS:
   case PIPE_CAP_QUERY_MEMORY_INFO:
   case PIPE_CAP_PCI_GROUP:
   case PIPE_CAP_PCI_BUS:
   case PIPE_CAP_PCI_DEVICE:
   case PIPE_CAP_PCI_FUNCTION:
   case PIPE_CAP_VIEWPORT_SUBPIXEL_BITS:
   case PIPE_CAP_TGSI_CAN_READ_OUTPUTS:
   case PIPE_CAP_NATIVE_FENCE_FD:
   case PIPE_CAP_GLSL_OPTIMIZE_CONSERVATIVELY:
   case PIPE_CAP_INT64_DIVMOD:
   case PIPE_CAP_SPARSE_BUFFER_PAGE_SIZE:
   case PIPE_CAP_BINDLESS_TEXTURE:
   case PIPE_CAP_NIR_SAMPLERS_AS_DEREF:
   case PIPE_CAP_QUERY_SO_OVERFLOW:
   case PIPE_CAP_MEMOBJ:
   case PIPE_CAP_LOAD_CONSTBUF:
   case PIPE_CAP_TGSI_ANY_REG_AS_ADDRESS:
   case PIPE_CAP_TILE_RASTER_ORDER:
      return 0;

   case PIPE_CAP_VENDOR_ID:
      return 0x10de;
   case PIPE_CAP_DEVICE_ID: {
      uint64_t device_id;
      if (nouveau_getparam(dev, NOUVEAU_GETPARAM_PCI_DEVICE, &device_id)) {
         NOUVEAU_ERR("NOUVEAU_GETPARAM_PCI_DEVICE failed.\n");
         return -1;
      }
      return device_id;
   }
   case PIPE_CAP_ACCELERATED:
      return 1;
   case PIPE_CAP_VIDEO_MEMORY:
      return dev->vram_size >> 20;
   case PIPE_CAP_UMA:
      return 0;
   }

   NOUVEAU_ERR("unknown PIPE_CAP %d\n", param);
   return 0;
}

static int
nvc0_screen_get_shader_param(struct pipe_screen *pscreen,
                             enum pipe_shader_type shader,
                             enum pipe_shader_cap param)
{
   const uint16_t class_3d = nouveau_screen(pscreen)->class_3d;

   switch (shader) {
   case PIPE_SHADER_VERTEX:
   case PIPE_SHADER_GEOMETRY:
   case PIPE_SHADER_FRAGMENT:
   case PIPE_SHADER_COMPUTE:
   case PIPE_SHADER_TESS_CTRL:
   case PIPE_SHADER_TESS_EVAL:
      break;
   default:
      return 0;
   }

   switch (param) {
   case PIPE_SHADER_CAP_PREFERRED_IR:
      return PIPE_SHADER_IR_TGSI;
   case PIPE_SHADER_CAP_SUPPORTED_IRS:
      return 1 << PIPE_SHADER_IR_TGSI;
   case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
      return 16384;
   case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      return 16;
   case PIPE_SHADER_CAP_MAX_INPUTS:
      if (shader == PIPE_SHADER_VERTEX)
         return 32;
      /* NOTE: These only count our slots for GENERIC varyings.
       * The address space may be larger, but the actual hard limit seems to be
       * less than what the address space layout permits, so don't add TEXCOORD,
       * COLOR, etc. here.
       */
      if (shader == PIPE_SHADER_FRAGMENT)
         return 0x1f0 / 16;
      /* Actually this counts CLIPVERTEX, which occupies the last generic slot,
       * and excludes 0x60 per-patch inputs.
       */
      return 0x200 / 16;
   case PIPE_SHADER_CAP_MAX_OUTPUTS:
      return 32;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE:
      return 65536;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
      return NVC0_MAX_PIPE_CONSTBUFS;
   case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
      return shader != PIPE_SHADER_FRAGMENT;
   case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
      return 1;
   case PIPE_SHADER_CAP_MAX_TEMPS:
      return NVC0_CAP_MAX_PROGRAM_TEMPS;
   case PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_SUBROUTINES:
      return 1;
   case PIPE_SHADER_CAP_INTEGERS:
      return 1;
   case PIPE_SHADER_CAP_TGSI_DROUND_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_TGSI_FMA_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS:
      return 1;
   case PIPE_SHADER_CAP_TGSI_DFRACEXP_DLDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_LDEXP_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
   case PIPE_SHADER_CAP_LOWER_IF_THRESHOLD:
   case PIPE_SHADER_CAP_INT64_ATOMICS:
   case PIPE_SHADER_CAP_FP16:
      return 0;
   case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
      return NVC0_MAX_BUFFERS;
   case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
      return (class_3d >= NVE4_3D_CLASS) ? 32 : 16;
   case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
      return (class_3d >= NVE4_3D_CLASS) ? 32 : 16;
   case PIPE_SHADER_CAP_MAX_UNROLL_ITERATIONS_HINT:
      return 32;
   case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
      if (class_3d >= NVE4_3D_CLASS)
         return NVC0_MAX_IMAGES;
      if (shader == PIPE_SHADER_FRAGMENT || shader == PIPE_SHADER_COMPUTE)
         return NVC0_MAX_IMAGES;
      return 0;
   default:
      NOUVEAU_ERR("unknown PIPE_SHADER_CAP %d\n", param);
      return 0;
   }
}

static float
nvc0_screen_get_paramf(struct pipe_screen *pscreen, enum pipe_capf param)
{
   switch (param) {
   case PIPE_CAPF_MAX_LINE_WIDTH:
   case PIPE_CAPF_MAX_LINE_WIDTH_AA:
      return 10.0f;
   case PIPE_CAPF_MAX_POINT_WIDTH:
      return 63.0f;
   case PIPE_CAPF_MAX_POINT_WIDTH_AA:
      return 63.375f;
   case PIPE_CAPF_MAX_TEXTURE_ANISOTROPY:
      return 16.0f;
   case PIPE_CAPF_MAX_TEXTURE_LOD_BIAS:
      return 15.0f;
   case PIPE_CAPF_GUARD_BAND_LEFT:
   case PIPE_CAPF_GUARD_BAND_TOP:
      return 0.0f;
   case PIPE_CAPF_GUARD_BAND_RIGHT:
   case PIPE_CAPF_GUARD_BAND_BOTTOM:
      return 0.0f; /* that or infinity */
   }

   NOUVEAU_ERR("unknown PIPE_CAPF %d\n", param);
   return 0.0f;
}

static int
nvc0_screen_get_compute_param(struct pipe_screen *pscreen,
                              enum pipe_shader_ir ir_type,
                              enum pipe_compute_cap param, void *data)
{
   struct nvc0_screen *screen = nvc0_screen(pscreen);
   const uint16_t obj_class = screen->compute->oclass;

#define RET(x) do {                  \
   if (data)                         \
      memcpy(data, x, sizeof(x));    \
   return sizeof(x);                 \
} while (0)

   switch (param) {
   case PIPE_COMPUTE_CAP_GRID_DIMENSION:
      RET((uint64_t []) { 3 });
   case PIPE_COMPUTE_CAP_MAX_GRID_SIZE:
      if (obj_class >= NVE4_COMPUTE_CLASS) {
         RET(((uint64_t []) { 0x7fffffff, 65535, 65535 }));
      } else {
         RET(((uint64_t []) { 65535, 65535, 65535 }));
      }
   case PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE:
      RET(((uint64_t []) { 1024, 1024, 64 }));
   case PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK:
      RET((uint64_t []) { 1024 });
   case PIPE_COMPUTE_CAP_MAX_VARIABLE_THREADS_PER_BLOCK:
      if (obj_class >= NVE4_COMPUTE_CLASS) {
         RET((uint64_t []) { 1024 });
      } else {
         RET((uint64_t []) { 512 });
      }
   case PIPE_COMPUTE_CAP_MAX_GLOBAL_SIZE: /* g[] */
      RET((uint64_t []) { 1ULL << 40 });
   case PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE: /* s[] */
      switch (obj_class) {
      case GM200_COMPUTE_CLASS:
         RET((uint64_t []) { 96 << 10 });
         break;
      case GM107_COMPUTE_CLASS:
         RET((uint64_t []) { 64 << 10 });
         break;
      default:
         RET((uint64_t []) { 48 << 10 });
         break;
      }
   case PIPE_COMPUTE_CAP_MAX_PRIVATE_SIZE: /* l[] */
      RET((uint64_t []) { 512 << 10 });
   case PIPE_COMPUTE_CAP_MAX_INPUT_SIZE: /* c[], arbitrary limit */
      RET((uint64_t []) { 4096 });
   case PIPE_COMPUTE_CAP_SUBGROUP_SIZE:
      RET((uint32_t []) { 32 });
   case PIPE_COMPUTE_CAP_MAX_MEM_ALLOC_SIZE:
      RET((uint64_t []) { 1ULL << 40 });
   case PIPE_COMPUTE_CAP_IMAGES_SUPPORTED:
      RET((uint32_t []) { 0 });
   case PIPE_COMPUTE_CAP_MAX_COMPUTE_UNITS:
      RET((uint32_t []) { screen->mp_count_compute });
   case PIPE_COMPUTE_CAP_MAX_CLOCK_FREQUENCY:
      RET((uint32_t []) { 512 }); /* FIXME: arbitrary limit */
   case PIPE_COMPUTE_CAP_ADDRESS_BITS:
      RET((uint32_t []) { 64 });
   default:
      return 0;
   }

#undef RET
}

static void
nvc0_screen_destroy(struct pipe_screen *pscreen)
{
   struct nvc0_screen *screen = nvc0_screen(pscreen);

   if (!nouveau_drm_screen_unref(&screen->base))
      return;

   if (screen->base.fence.current) {
      struct nouveau_fence *current = NULL;

      /* nouveau_fence_wait will create a new current fence, so wait on the
       * _current_ one, and remove both.
       */
      nouveau_fence_ref(screen->base.fence.current, &current);
      nouveau_fence_wait(current, NULL);
      nouveau_fence_ref(NULL, &current);
      nouveau_fence_ref(NULL, &screen->base.fence.current);
   }
   if (screen->base.pushbuf)
      screen->base.pushbuf->user_priv = NULL;

   if (screen->blitter)
      nvc0_blitter_destroy(screen);
   if (screen->pm.prog) {
      screen->pm.prog->code = NULL; /* hardcoded, don't FREE */
      nvc0_program_destroy(NULL, screen->pm.prog);
      FREE(screen->pm.prog);
   }

   nouveau_bo_ref(NULL, &screen->text);
   nouveau_bo_ref(NULL, &screen->uniform_bo);
   nouveau_bo_ref(NULL, &screen->tls);
   nouveau_bo_ref(NULL, &screen->txc);
   nouveau_bo_ref(NULL, &screen->fence.bo);
   nouveau_bo_ref(NULL, &screen->poly_cache);

   nouveau_heap_destroy(&screen->lib_code);
   nouveau_heap_destroy(&screen->text_heap);

   FREE(screen->default_tsc);
   FREE(screen->tic.entries);

   nouveau_object_del(&screen->eng3d);
   nouveau_object_del(&screen->eng2d);
   nouveau_object_del(&screen->m2mf);
   nouveau_object_del(&screen->compute);
   nouveau_object_del(&screen->nvsw);

   nouveau_screen_fini(&screen->base);

   FREE(screen);
}

static int
nvc0_graph_set_macro(struct nvc0_screen *screen, uint32_t m, unsigned pos,
                     unsigned size, const uint32_t *data)
{
   struct nouveau_pushbuf *push = screen->base.pushbuf;

   size /= 4;

   assert((pos + size) <= 0x800);

   BEGIN_NVC0(push, SUBC_3D(NVC0_GRAPH_MACRO_ID), 2);
   PUSH_DATA (push, (m - 0x3800) / 8);
   PUSH_DATA (push, pos);
   BEGIN_1IC0(push, SUBC_3D(NVC0_GRAPH_MACRO_UPLOAD_POS), size + 1);
   PUSH_DATA (push, pos);
   PUSH_DATAp(push, data, size);

   return pos + size;
}

static void
nvc0_magic_3d_init(struct nouveau_pushbuf *push, uint16_t obj_class)
{
   BEGIN_NVC0(push, SUBC_3D(0x10cc), 1);
   PUSH_DATA (push, 0xff);
   BEGIN_NVC0(push, SUBC_3D(0x10e0), 2);
   PUSH_DATA (push, 0xff);
   PUSH_DATA (push, 0xff);
   BEGIN_NVC0(push, SUBC_3D(0x10ec), 2);
   PUSH_DATA (push, 0xff);
   PUSH_DATA (push, 0xff);
   BEGIN_NVC0(push, SUBC_3D(0x074c), 1);
   PUSH_DATA (push, 0x3f);

   BEGIN_NVC0(push, SUBC_3D(0x16a8), 1);
   PUSH_DATA (push, (3 << 16) | 3);
   BEGIN_NVC0(push, SUBC_3D(0x1794), 1);
   PUSH_DATA (push, (2 << 16) | 2);

   if (obj_class < GM107_3D_CLASS) {
      BEGIN_NVC0(push, SUBC_3D(0x12ac), 1);
      PUSH_DATA (push, 0);
   }
   BEGIN_NVC0(push, SUBC_3D(0x0218), 1);
   PUSH_DATA (push, 0x10);
   BEGIN_NVC0(push, SUBC_3D(0x10fc), 1);
   PUSH_DATA (push, 0x10);
   BEGIN_NVC0(push, SUBC_3D(0x1290), 1);
   PUSH_DATA (push, 0x10);
   BEGIN_NVC0(push, SUBC_3D(0x12d8), 2);
   PUSH_DATA (push, 0x10);
   PUSH_DATA (push, 0x10);
   BEGIN_NVC0(push, SUBC_3D(0x1140), 1);
   PUSH_DATA (push, 0x10);
   BEGIN_NVC0(push, SUBC_3D(0x1610), 1);
   PUSH_DATA (push, 0xe);

   BEGIN_NVC0(push, NVC0_3D(VERTEX_ID_GEN_MODE), 1);
   PUSH_DATA (push, NVC0_3D_VERTEX_ID_GEN_MODE_DRAW_ARRAYS_ADD_START);
   BEGIN_NVC0(push, SUBC_3D(0x030c), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, SUBC_3D(0x0300), 1);
   PUSH_DATA (push, 3);

   BEGIN_NVC0(push, SUBC_3D(0x02d0), 1);
   PUSH_DATA (push, 0x3fffff);
   BEGIN_NVC0(push, SUBC_3D(0x0fdc), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, SUBC_3D(0x19c0), 1);
   PUSH_DATA (push, 1);

   if (obj_class < GM107_3D_CLASS) {
      BEGIN_NVC0(push, SUBC_3D(0x075c), 1);
      PUSH_DATA (push, 3);

      if (obj_class >= NVE4_3D_CLASS) {
         BEGIN_NVC0(push, SUBC_3D(0x07fc), 1);
         PUSH_DATA (push, 1);
      }
   }

   /* TODO: find out what software methods 0x1528, 0x1280 and (on nve4) 0x02dc
    * are supposed to do */
}

static void
nvc0_screen_fence_emit(struct pipe_screen *pscreen, u32 *sequence)
{
   struct nvc0_screen *screen = nvc0_screen(pscreen);
   struct nouveau_pushbuf *push = screen->base.pushbuf;

   /* we need to do it after possible flush in MARK_RING */
   *sequence = ++screen->base.fence.sequence;

   assert(PUSH_AVAIL(push) + push->rsvd_kick >= 5);
   PUSH_DATA (push, NVC0_FIFO_PKHDR_SQ(NVC0_3D(QUERY_ADDRESS_HIGH), 4));
   PUSH_DATAh(push, screen->fence.bo->offset);
   PUSH_DATA (push, screen->fence.bo->offset);
   PUSH_DATA (push, *sequence);
   PUSH_DATA (push, NVC0_3D_QUERY_GET_FENCE | NVC0_3D_QUERY_GET_SHORT |
              (0xf << NVC0_3D_QUERY_GET_UNIT__SHIFT));
}

static u32
nvc0_screen_fence_update(struct pipe_screen *pscreen)
{
   struct nvc0_screen *screen = nvc0_screen(pscreen);
   return screen->fence.map[0];
}

static int
nvc0_screen_init_compute(struct nvc0_screen *screen)
{
   screen->base.base.get_compute_param = nvc0_screen_get_compute_param;

   switch (screen->base.device->chipset & ~0xf) {
   case 0xc0:
   case 0xd0:
      return nvc0_screen_compute_setup(screen, screen->base.pushbuf);
   case 0xe0:
   case 0xf0:
   case 0x100:
   case 0x110:
   case 0x120:
   case 0x130:
      return nve4_screen_compute_setup(screen, screen->base.pushbuf);
   default:
      return -1;
   }
}

static int
nvc0_screen_resize_tls_area(struct nvc0_screen *screen,
                            uint32_t lpos, uint32_t lneg, uint32_t cstack)
{
   struct nouveau_bo *bo = NULL;
   int ret;
   uint64_t size = (lpos + lneg) * 32 + cstack;

   if (size >= (1 << 20)) {
      NOUVEAU_ERR("requested TLS size too large: 0x%"PRIx64"\n", size);
      return -1;
   }

   size *= (screen->base.device->chipset >= 0xe0) ? 64 : 48; /* max warps */
   size  = align(size, 0x8000);
   size *= screen->mp_count;

   size = align(size, 1 << 17);

   ret = nouveau_bo_new(screen->base.device, NV_VRAM_DOMAIN(&screen->base), 1 << 17, size,
                        NULL, &bo);
   if (ret)
      return ret;
   nouveau_bo_ref(NULL, &screen->tls);
   screen->tls = bo;
   return 0;
}

int
nvc0_screen_resize_text_area(struct nvc0_screen *screen, uint64_t size)
{
   struct nouveau_pushbuf *push = screen->base.pushbuf;
   struct nouveau_bo *bo;
   int ret;

   ret = nouveau_bo_new(screen->base.device, NV_VRAM_DOMAIN(&screen->base),
                        1 << 17, size, NULL, &bo);
   if (ret)
      return ret;

   nouveau_bo_ref(NULL, &screen->text);
   screen->text = bo;

   nouveau_heap_destroy(&screen->lib_code);
   nouveau_heap_destroy(&screen->text_heap);

   /* XXX: getting a page fault at the end of the code buffer every few
    *  launches, don't use the last 256 bytes to work around them - prefetch ?
    */
   nouveau_heap_init(&screen->text_heap, 0, size - 0x100);

   /* update the code segment setup */
   BEGIN_NVC0(push, NVC0_3D(CODE_ADDRESS_HIGH), 2);
   PUSH_DATAh(push, screen->text->offset);
   PUSH_DATA (push, screen->text->offset);
   if (screen->compute) {
      BEGIN_NVC0(push, NVC0_CP(CODE_ADDRESS_HIGH), 2);
      PUSH_DATAh(push, screen->text->offset);
      PUSH_DATA (push, screen->text->offset);
   }

   return 0;
}

#define FAIL_SCREEN_INIT(str, err)                    \
   do {                                               \
      NOUVEAU_ERR(str, err);                          \
      goto fail;                                      \
   } while(0)

struct nouveau_screen *
nvc0_screen_create(struct nouveau_device *dev)
{
   struct nvc0_screen *screen;
   struct pipe_screen *pscreen;
   struct nouveau_object *chan;
   struct nouveau_pushbuf *push;
   uint64_t value;
   uint32_t obj_class;
   uint32_t flags;
   int ret;
   unsigned i;

   switch (dev->chipset & ~0xf) {
   case 0xc0:
   case 0xd0:
   case 0xe0:
   case 0xf0:
   case 0x100:
   case 0x110:
   case 0x120:
   case 0x130:
      break;
   default:
      return NULL;
   }

   screen = CALLOC_STRUCT(nvc0_screen);
   if (!screen)
      return NULL;
   pscreen = &screen->base.base;
   pscreen->destroy = nvc0_screen_destroy;

   ret = nouveau_screen_init(&screen->base, dev);
   if (ret)
      FAIL_SCREEN_INIT("Base screen init failed: %d\n", ret);
   chan = screen->base.channel;
   push = screen->base.pushbuf;
   push->user_priv = screen;
   push->rsvd_kick = 5;

   screen->base.vidmem_bindings |= PIPE_BIND_CONSTANT_BUFFER |
      PIPE_BIND_SHADER_BUFFER |
      PIPE_BIND_VERTEX_BUFFER | PIPE_BIND_INDEX_BUFFER |
      PIPE_BIND_COMMAND_ARGS_BUFFER | PIPE_BIND_QUERY_BUFFER;
   screen->base.sysmem_bindings |=
      PIPE_BIND_VERTEX_BUFFER | PIPE_BIND_INDEX_BUFFER;

   if (screen->base.vram_domain & NOUVEAU_BO_GART) {
      screen->base.sysmem_bindings |= screen->base.vidmem_bindings;
      screen->base.vidmem_bindings = 0;
   }

   pscreen->context_create = nvc0_create;
   pscreen->is_format_supported = nvc0_screen_is_format_supported;
   pscreen->get_param = nvc0_screen_get_param;
   pscreen->get_shader_param = nvc0_screen_get_shader_param;
   pscreen->get_paramf = nvc0_screen_get_paramf;
   pscreen->get_driver_query_info = nvc0_screen_get_driver_query_info;
   pscreen->get_driver_query_group_info = nvc0_screen_get_driver_query_group_info;

   nvc0_screen_init_resource_functions(pscreen);

   screen->base.base.get_video_param = nouveau_vp3_screen_get_video_param;
   screen->base.base.is_video_format_supported = nouveau_vp3_screen_video_supported;

   flags = NOUVEAU_BO_GART | NOUVEAU_BO_MAP;
   if (screen->base.drm->version >= 0x01000202)
      flags |= NOUVEAU_BO_COHERENT;

   ret = nouveau_bo_new(dev, flags, 0, 4096, NULL, &screen->fence.bo);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating fence BO: %d\n", ret);
   nouveau_bo_map(screen->fence.bo, 0, NULL);
   screen->fence.map = screen->fence.bo->map;
   screen->base.fence.emit = nvc0_screen_fence_emit;
   screen->base.fence.update = nvc0_screen_fence_update;


   ret = nouveau_object_new(chan, (dev->chipset < 0xe0) ? 0x1f906e : 0x906e,
                            NVIF_CLASS_SW_GF100, NULL, 0, &screen->nvsw);
   if (ret)
      FAIL_SCREEN_INIT("Error creating SW object: %d\n", ret);

   BEGIN_NVC0(push, SUBC_SW(NV01_SUBCHAN_OBJECT), 1);
   PUSH_DATA (push, screen->nvsw->handle);

   switch (dev->chipset & ~0xf) {
   case 0x130:
   case 0x120:
   case 0x110:
   case 0x100:
   case 0xf0:
      obj_class = NVF0_P2MF_CLASS;
      break;
   case 0xe0:
      obj_class = NVE4_P2MF_CLASS;
      break;
   default:
      obj_class = NVC0_M2MF_CLASS;
      break;
   }
   ret = nouveau_object_new(chan, 0xbeef323f, obj_class, NULL, 0,
                            &screen->m2mf);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating PGRAPH context for M2MF: %d\n", ret);

   BEGIN_NVC0(push, SUBC_M2MF(NV01_SUBCHAN_OBJECT), 1);
   PUSH_DATA (push, screen->m2mf->oclass);
   if (screen->m2mf->oclass == NVE4_P2MF_CLASS) {
      BEGIN_NVC0(push, SUBC_COPY(NV01_SUBCHAN_OBJECT), 1);
      PUSH_DATA (push, 0xa0b5);
   }

   ret = nouveau_object_new(chan, 0xbeef902d, NVC0_2D_CLASS, NULL, 0,
                            &screen->eng2d);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating PGRAPH context for 2D: %d\n", ret);

   BEGIN_NVC0(push, SUBC_2D(NV01_SUBCHAN_OBJECT), 1);
   PUSH_DATA (push, screen->eng2d->oclass);
   BEGIN_NVC0(push, SUBC_2D(NVC0_2D_SINGLE_GPC), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_2D(OPERATION), 1);
   PUSH_DATA (push, NV50_2D_OPERATION_SRCCOPY);
   BEGIN_NVC0(push, NVC0_2D(CLIP_ENABLE), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_2D(COLOR_KEY_ENABLE), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, SUBC_2D(0x0884), 1);
   PUSH_DATA (push, 0x3f);
   BEGIN_NVC0(push, SUBC_2D(0x0888), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_2D(COND_MODE), 1);
   PUSH_DATA (push, NV50_2D_COND_MODE_ALWAYS);

   BEGIN_NVC0(push, SUBC_2D(NVC0_GRAPH_NOTIFY_ADDRESS_HIGH), 2);
   PUSH_DATAh(push, screen->fence.bo->offset + 16);
   PUSH_DATA (push, screen->fence.bo->offset + 16);

   switch (dev->chipset & ~0xf) {
   case 0x130:
      switch (dev->chipset) {
      case 0x130:
      case 0x13b:
         obj_class = GP100_3D_CLASS;
         break;
      default:
         obj_class = GP102_3D_CLASS;
         break;
      }
      break;
   case 0x120:
      obj_class = GM200_3D_CLASS;
      break;
   case 0x110:
      obj_class = GM107_3D_CLASS;
      break;
   case 0x100:
   case 0xf0:
      obj_class = NVF0_3D_CLASS;
      break;
   case 0xe0:
      switch (dev->chipset) {
      case 0xea:
         obj_class = NVEA_3D_CLASS;
         break;
      default:
         obj_class = NVE4_3D_CLASS;
         break;
      }
      break;
   case 0xd0:
      obj_class = NVC8_3D_CLASS;
      break;
   case 0xc0:
   default:
      switch (dev->chipset) {
      case 0xc8:
         obj_class = NVC8_3D_CLASS;
         break;
      case 0xc1:
         obj_class = NVC1_3D_CLASS;
         break;
      default:
         obj_class = NVC0_3D_CLASS;
         break;
      }
      break;
   }
   ret = nouveau_object_new(chan, 0xbeef003d, obj_class, NULL, 0,
                            &screen->eng3d);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating PGRAPH context for 3D: %d\n", ret);
   screen->base.class_3d = obj_class;

   BEGIN_NVC0(push, SUBC_3D(NV01_SUBCHAN_OBJECT), 1);
   PUSH_DATA (push, screen->eng3d->oclass);

   BEGIN_NVC0(push, NVC0_3D(COND_MODE), 1);
   PUSH_DATA (push, NVC0_3D_COND_MODE_ALWAYS);

   if (debug_get_bool_option("NOUVEAU_SHADER_WATCHDOG", true)) {
      /* kill shaders after about 1 second (at 100 MHz) */
      BEGIN_NVC0(push, NVC0_3D(WATCHDOG_TIMER), 1);
      PUSH_DATA (push, 0x17);
   }

   IMMED_NVC0(push, NVC0_3D(ZETA_COMP_ENABLE),
                    screen->base.drm->version >= 0x01000101);
   BEGIN_NVC0(push, NVC0_3D(RT_COMP_ENABLE(0)), 8);
   for (i = 0; i < 8; ++i)
      PUSH_DATA(push, screen->base.drm->version >= 0x01000101);

   BEGIN_NVC0(push, NVC0_3D(RT_CONTROL), 1);
   PUSH_DATA (push, 1);

   BEGIN_NVC0(push, NVC0_3D(CSAA_ENABLE), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(MULTISAMPLE_ENABLE), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(MULTISAMPLE_MODE), 1);
   PUSH_DATA (push, NVC0_3D_MULTISAMPLE_MODE_MS1);
   BEGIN_NVC0(push, NVC0_3D(MULTISAMPLE_CTRL), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(LINE_WIDTH_SEPARATE), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_3D(PRIM_RESTART_WITH_DRAW_ARRAYS), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_3D(BLEND_SEPARATE_ALPHA), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_3D(BLEND_ENABLE_COMMON), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(SHADE_MODEL), 1);
   PUSH_DATA (push, NVC0_3D_SHADE_MODEL_SMOOTH);
   if (screen->eng3d->oclass < NVE4_3D_CLASS) {
      IMMED_NVC0(push, NVC0_3D(TEX_MISC), 0);
   } else {
      BEGIN_NVC0(push, NVE4_3D(TEX_CB_INDEX), 1);
      PUSH_DATA (push, 15);
   }
   BEGIN_NVC0(push, NVC0_3D(CALL_LIMIT_LOG), 1);
   PUSH_DATA (push, 8); /* 128 */
   BEGIN_NVC0(push, NVC0_3D(ZCULL_STATCTRS_ENABLE), 1);
   PUSH_DATA (push, 1);
   if (screen->eng3d->oclass >= NVC1_3D_CLASS) {
      BEGIN_NVC0(push, NVC0_3D(CACHE_SPLIT), 1);
      PUSH_DATA (push, NVC0_3D_CACHE_SPLIT_48K_SHARED_16K_L1);
   }

   nvc0_magic_3d_init(push, screen->eng3d->oclass);

   ret = nvc0_screen_resize_text_area(screen, 1 << 19);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating TEXT area: %d\n", ret);

   ret = nouveau_bo_new(dev, NV_VRAM_DOMAIN(&screen->base), 1 << 12, 7 << 16, NULL,
                        &screen->uniform_bo);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating uniform BO: %d\n", ret);

   PUSH_REFN (push, screen->uniform_bo, NV_VRAM_DOMAIN(&screen->base) | NOUVEAU_BO_WR);

   /* return { 0.0, 0.0, 0.0, 0.0 } for out-of-bounds vtxbuf access */
   BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
   PUSH_DATA (push, 256);
   PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_RUNOUT_INFO);
   PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_RUNOUT_INFO);
   BEGIN_1IC0(push, NVC0_3D(CB_POS), 5);
   PUSH_DATA (push, 0);
   PUSH_DATAf(push, 0.0f);
   PUSH_DATAf(push, 0.0f);
   PUSH_DATAf(push, 0.0f);
   PUSH_DATAf(push, 0.0f);
   BEGIN_NVC0(push, NVC0_3D(VERTEX_RUNOUT_ADDRESS_HIGH), 2);
   PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_RUNOUT_INFO);
   PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_RUNOUT_INFO);

   if (screen->base.drm->version >= 0x01000101) {
      ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_GRAPH_UNITS, &value);
      if (ret)
         FAIL_SCREEN_INIT("NOUVEAU_GETPARAM_GRAPH_UNITS failed: %d\n", ret);
   } else {
      if (dev->chipset >= 0xe0 && dev->chipset < 0xf0)
         value = (8 << 8) | 4;
      else
         value = (16 << 8) | 4;
   }
   screen->gpc_count = value & 0x000000ff;
   screen->mp_count = value >> 8;
   screen->mp_count_compute = screen->mp_count;

   ret = nvc0_screen_resize_tls_area(screen, 128 * 16, 0, 0x200);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating TLS area: %d\n", ret);

   BEGIN_NVC0(push, NVC0_3D(TEMP_ADDRESS_HIGH), 4);
   PUSH_DATAh(push, screen->tls->offset);
   PUSH_DATA (push, screen->tls->offset);
   PUSH_DATA (push, screen->tls->size >> 32);
   PUSH_DATA (push, screen->tls->size);
   BEGIN_NVC0(push, NVC0_3D(WARP_TEMP_ALLOC), 1);
   PUSH_DATA (push, 0);
   /* Reduce likelihood of collision with real buffers by placing the hole at
    * the top of the 4G area. This will have to be dealt with for real
    * eventually by blocking off that area from the VM.
    */
   BEGIN_NVC0(push, NVC0_3D(LOCAL_BASE), 1);
   PUSH_DATA (push, 0xff << 24);

   if (screen->eng3d->oclass < GM107_3D_CLASS) {
      ret = nouveau_bo_new(dev, NV_VRAM_DOMAIN(&screen->base), 1 << 17, 1 << 20, NULL,
                           &screen->poly_cache);
      if (ret)
         FAIL_SCREEN_INIT("Error allocating poly cache BO: %d\n", ret);

      BEGIN_NVC0(push, NVC0_3D(VERTEX_QUARANTINE_ADDRESS_HIGH), 3);
      PUSH_DATAh(push, screen->poly_cache->offset);
      PUSH_DATA (push, screen->poly_cache->offset);
      PUSH_DATA (push, 3);
   }

   ret = nouveau_bo_new(dev, NV_VRAM_DOMAIN(&screen->base), 1 << 17, 1 << 17, NULL,
                        &screen->txc);
   if (ret)
      FAIL_SCREEN_INIT("Error allocating txc BO: %d\n", ret);

   BEGIN_NVC0(push, NVC0_3D(TIC_ADDRESS_HIGH), 3);
   PUSH_DATAh(push, screen->txc->offset);
   PUSH_DATA (push, screen->txc->offset);
   PUSH_DATA (push, NVC0_TIC_MAX_ENTRIES - 1);
   if (screen->eng3d->oclass >= GM107_3D_CLASS) {
      screen->tic.maxwell = true;
      if (screen->eng3d->oclass == GM107_3D_CLASS) {
         screen->tic.maxwell =
            debug_get_bool_option("NOUVEAU_MAXWELL_TIC", true);
         IMMED_NVC0(push, SUBC_3D(0x0f10), screen->tic.maxwell);
      }
   }

   BEGIN_NVC0(push, NVC0_3D(TSC_ADDRESS_HIGH), 3);
   PUSH_DATAh(push, screen->txc->offset + 65536);
   PUSH_DATA (push, screen->txc->offset + 65536);
   PUSH_DATA (push, NVC0_TSC_MAX_ENTRIES - 1);

   BEGIN_NVC0(push, NVC0_3D(SCREEN_Y_CONTROL), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(WINDOW_OFFSET_X), 2);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(ZCULL_REGION), 1); /* deactivate ZCULL */
   PUSH_DATA (push, 0x3f);

   BEGIN_NVC0(push, NVC0_3D(CLIP_RECTS_MODE), 1);
   PUSH_DATA (push, NVC0_3D_CLIP_RECTS_MODE_INSIDE_ANY);
   BEGIN_NVC0(push, NVC0_3D(CLIP_RECT_HORIZ(0)), 8 * 2);
   for (i = 0; i < 8 * 2; ++i)
      PUSH_DATA(push, 0);
   BEGIN_NVC0(push, NVC0_3D(CLIP_RECTS_EN), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(CLIPID_ENABLE), 1);
   PUSH_DATA (push, 0);

   /* neither scissors, viewport nor stencil mask should affect clears */
   BEGIN_NVC0(push, NVC0_3D(CLEAR_FLAGS), 1);
   PUSH_DATA (push, 0);

   BEGIN_NVC0(push, NVC0_3D(VIEWPORT_TRANSFORM_EN), 1);
   PUSH_DATA (push, 1);
   for (i = 0; i < NVC0_MAX_VIEWPORTS; i++) {
      BEGIN_NVC0(push, NVC0_3D(DEPTH_RANGE_NEAR(i)), 2);
      PUSH_DATAf(push, 0.0f);
      PUSH_DATAf(push, 1.0f);
   }
   BEGIN_NVC0(push, NVC0_3D(VIEW_VOLUME_CLIP_CTRL), 1);
   PUSH_DATA (push, NVC0_3D_VIEW_VOLUME_CLIP_CTRL_UNK1_UNK1);

   /* We use scissors instead of exact view volume clipping,
    * so they're always enabled.
    */
   for (i = 0; i < NVC0_MAX_VIEWPORTS; i++) {
      BEGIN_NVC0(push, NVC0_3D(SCISSOR_ENABLE(i)), 3);
      PUSH_DATA (push, 1);
      PUSH_DATA (push, 8192 << 16);
      PUSH_DATA (push, 8192 << 16);
   }

#define MK_MACRO(m, n) i = nvc0_graph_set_macro(screen, m, i, sizeof(n), n);

   i = 0;
   MK_MACRO(NVC0_3D_MACRO_VERTEX_ARRAY_PER_INSTANCE, mme9097_per_instance_bf);
   MK_MACRO(NVC0_3D_MACRO_BLEND_ENABLES, mme9097_blend_enables);
   MK_MACRO(NVC0_3D_MACRO_VERTEX_ARRAY_SELECT, mme9097_vertex_array_select);
   MK_MACRO(NVC0_3D_MACRO_TEP_SELECT, mme9097_tep_select);
   MK_MACRO(NVC0_3D_MACRO_GP_SELECT, mme9097_gp_select);
   MK_MACRO(NVC0_3D_MACRO_POLYGON_MODE_FRONT, mme9097_poly_mode_front);
   MK_MACRO(NVC0_3D_MACRO_POLYGON_MODE_BACK, mme9097_poly_mode_back);
   MK_MACRO(NVC0_3D_MACRO_DRAW_ARRAYS_INDIRECT, mme9097_draw_arrays_indirect);
   MK_MACRO(NVC0_3D_MACRO_DRAW_ELEMENTS_INDIRECT, mme9097_draw_elts_indirect);
   MK_MACRO(NVC0_3D_MACRO_DRAW_ARRAYS_INDIRECT_COUNT, mme9097_draw_arrays_indirect_count);
   MK_MACRO(NVC0_3D_MACRO_DRAW_ELEMENTS_INDIRECT_COUNT, mme9097_draw_elts_indirect_count);
   MK_MACRO(NVC0_3D_MACRO_QUERY_BUFFER_WRITE, mme9097_query_buffer_write);
   MK_MACRO(NVC0_CP_MACRO_LAUNCH_GRID_INDIRECT, mme90c0_launch_grid_indirect);

   BEGIN_NVC0(push, NVC0_3D(RASTERIZE_ENABLE), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_3D(RT_SEPARATE_FRAG_DATA), 1);
   PUSH_DATA (push, 1);
   BEGIN_NVC0(push, NVC0_3D(MACRO_GP_SELECT), 1);
   PUSH_DATA (push, 0x40);
   BEGIN_NVC0(push, NVC0_3D(LAYER), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(MACRO_TEP_SELECT), 1);
   PUSH_DATA (push, 0x30);
   BEGIN_NVC0(push, NVC0_3D(PATCH_VERTICES), 1);
   PUSH_DATA (push, 3);
   BEGIN_NVC0(push, NVC0_3D(SP_SELECT(2)), 1);
   PUSH_DATA (push, 0x20);
   BEGIN_NVC0(push, NVC0_3D(SP_SELECT(0)), 1);
   PUSH_DATA (push, 0x00);
   screen->save_state.patch_vertices = 3;

   BEGIN_NVC0(push, NVC0_3D(POINT_COORD_REPLACE), 1);
   PUSH_DATA (push, 0);
   BEGIN_NVC0(push, NVC0_3D(POINT_RASTER_RULES), 1);
   PUSH_DATA (push, NVC0_3D_POINT_RASTER_RULES_OGL);

   IMMED_NVC0(push, NVC0_3D(EDGEFLAG), 1);

   if (nvc0_screen_init_compute(screen))
      goto fail;

   /* XXX: Compute and 3D are somehow aliased on Fermi. */
   for (i = 0; i < 5; ++i) {
      /* TIC and TSC entries for each unit (nve4+ only) */
      /* auxiliary constants (6 user clip planes, base instance id) */
      BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
      PUSH_DATA (push, NVC0_CB_AUX_SIZE);
      PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(i));
      PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(i));
      BEGIN_NVC0(push, NVC0_3D(CB_BIND(i)), 1);
      PUSH_DATA (push, (15 << 4) | 1);
      if (screen->eng3d->oclass >= NVE4_3D_CLASS) {
         unsigned j;
         BEGIN_1IC0(push, NVC0_3D(CB_POS), 9);
         PUSH_DATA (push, NVC0_CB_AUX_UNK_INFO);
         for (j = 0; j < 8; ++j)
            PUSH_DATA(push, j);
      } else {
         BEGIN_NVC0(push, NVC0_3D(TEX_LIMITS(i)), 1);
         PUSH_DATA (push, 0x54);
      }

      /* MS sample coordinate offsets: these do not work with _ALT modes ! */
      BEGIN_1IC0(push, NVC0_3D(CB_POS), 1 + 2 * 8);
      PUSH_DATA (push, NVC0_CB_AUX_MS_INFO);
      PUSH_DATA (push, 0); /* 0 */
      PUSH_DATA (push, 0);
      PUSH_DATA (push, 1); /* 1 */
      PUSH_DATA (push, 0);
      PUSH_DATA (push, 0); /* 2 */
      PUSH_DATA (push, 1);
      PUSH_DATA (push, 1); /* 3 */
      PUSH_DATA (push, 1);
      PUSH_DATA (push, 2); /* 4 */
      PUSH_DATA (push, 0);
      PUSH_DATA (push, 3); /* 5 */
      PUSH_DATA (push, 0);
      PUSH_DATA (push, 2); /* 6 */
      PUSH_DATA (push, 1);
      PUSH_DATA (push, 3); /* 7 */
      PUSH_DATA (push, 1);
   }
   BEGIN_NVC0(push, NVC0_3D(LINKED_TSC), 1);
   PUSH_DATA (push, 0);

   PUSH_KICK (push);

   screen->tic.entries = CALLOC(4096, sizeof(void *));
   screen->tsc.entries = screen->tic.entries + 2048;

   if (!nvc0_blitter_create(screen))
      goto fail;

   screen->default_tsc = CALLOC_STRUCT(nv50_tsc_entry);
   screen->default_tsc->tsc[0] = G80_TSC_0_SRGB_CONVERSION;

   nouveau_fence_new(&screen->base, &screen->base.fence.current);

   return &screen->base;

fail:
   screen->base.base.context_create = NULL;
   return &screen->base;
}

int
nvc0_screen_tic_alloc(struct nvc0_screen *screen, void *entry)
{
   int i = screen->tic.next;

   while (screen->tic.lock[i / 32] & (1 << (i % 32)))
      i = (i + 1) & (NVC0_TIC_MAX_ENTRIES - 1);

   screen->tic.next = (i + 1) & (NVC0_TIC_MAX_ENTRIES - 1);

   if (screen->tic.entries[i])
      nv50_tic_entry(screen->tic.entries[i])->id = -1;

   screen->tic.entries[i] = entry;
   return i;
}

int
nvc0_screen_tsc_alloc(struct nvc0_screen *screen, void *entry)
{
   int i = screen->tsc.next;

   while (screen->tsc.lock[i / 32] & (1 << (i % 32)))
      i = (i + 1) & (NVC0_TSC_MAX_ENTRIES - 1);

   screen->tsc.next = (i + 1) & (NVC0_TSC_MAX_ENTRIES - 1);

   if (screen->tsc.entries[i])
      nv50_tsc_entry(screen->tsc.entries[i])->id = -1;

   screen->tsc.entries[i] = entry;
   return i;
}
