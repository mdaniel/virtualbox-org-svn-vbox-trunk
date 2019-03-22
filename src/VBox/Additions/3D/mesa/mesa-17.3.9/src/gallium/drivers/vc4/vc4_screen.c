/*
 * Copyright © 2014 Broadcom
 * Copyright (C) 2012 Rob Clark <robclark@freedesktop.org>
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

#include "os/os_misc.h"
#include "pipe/p_defines.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"

#include "util/u_cpu_detect.h"
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "util/u_format.h"
#include "util/u_hash_table.h"
#include "util/ralloc.h"

#include <xf86drm.h>
#include "drm_fourcc.h"
#include "vc4_drm.h"
#include "vc4_screen.h"
#include "vc4_context.h"
#include "vc4_resource.h"

static const struct debug_named_value debug_options[] = {
        { "cl",       VC4_DEBUG_CL,
          "Dump command list during creation" },
        { "surf",       VC4_DEBUG_SURFACE,
          "Dump surface layouts" },
        { "qpu",      VC4_DEBUG_QPU,
          "Dump generated QPU instructions" },
        { "qir",      VC4_DEBUG_QIR,
          "Dump QPU IR during program compile" },
        { "nir",      VC4_DEBUG_NIR,
          "Dump NIR during program compile" },
        { "tgsi",     VC4_DEBUG_TGSI,
          "Dump TGSI during program compile" },
        { "shaderdb", VC4_DEBUG_SHADERDB,
          "Dump program compile information for shader-db analysis" },
        { "perf",     VC4_DEBUG_PERF,
          "Print during performance-related events" },
        { "norast",   VC4_DEBUG_NORAST,
          "Skip actual hardware execution of commands" },
        { "always_flush", VC4_DEBUG_ALWAYS_FLUSH,
          "Flush after each draw call" },
        { "always_sync", VC4_DEBUG_ALWAYS_SYNC,
          "Wait for finish after each flush" },
#if USE_VC4_SIMULATOR
        { "dump", VC4_DEBUG_DUMP,
          "Write a GPU command stream trace file" },
#endif
        { NULL }
};

DEBUG_GET_ONCE_FLAGS_OPTION(vc4_debug, "VC4_DEBUG", debug_options, 0)
uint32_t vc4_debug;

static const char *
vc4_screen_get_name(struct pipe_screen *pscreen)
{
        struct vc4_screen *screen = vc4_screen(pscreen);

        if (!screen->name) {
                screen->name = ralloc_asprintf(screen,
                                               "VC4 V3D %d.%d",
                                               screen->v3d_ver / 10,
                                               screen->v3d_ver % 10);
        }

        return screen->name;
}

static const char *
vc4_screen_get_vendor(struct pipe_screen *pscreen)
{
        return "Broadcom";
}

static void
vc4_screen_destroy(struct pipe_screen *pscreen)
{
        struct vc4_screen *screen = vc4_screen(pscreen);

        util_hash_table_destroy(screen->bo_handles);
        vc4_bufmgr_destroy(pscreen);
        slab_destroy_parent(&screen->transfer_pool);
        free(screen->ro);

#if USE_VC4_SIMULATOR
        vc4_simulator_destroy(screen);
#endif

        close(screen->fd);
        ralloc_free(pscreen);
}

static bool
vc4_has_feature(struct vc4_screen *screen, uint32_t feature)
{
        struct drm_vc4_get_param p = {
                .param = feature,
        };
        int ret = vc4_ioctl(screen->fd, DRM_IOCTL_VC4_GET_PARAM, &p);

        if (ret != 0)
                return false;

        return p.value;
}

static int
vc4_screen_get_param(struct pipe_screen *pscreen, enum pipe_cap param)
{
        struct vc4_screen *screen = vc4_screen(pscreen);

        switch (param) {
                /* Supported features (boolean caps). */
        case PIPE_CAP_VERTEX_COLOR_CLAMPED:
        case PIPE_CAP_VERTEX_COLOR_UNCLAMPED:
        case PIPE_CAP_FRAGMENT_COLOR_CLAMPED:
        case PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT:
        case PIPE_CAP_NPOT_TEXTURES:
        case PIPE_CAP_SHAREABLE_SHADERS:
        case PIPE_CAP_USER_CONSTANT_BUFFERS:
        case PIPE_CAP_TEXTURE_SHADOW_MAP:
        case PIPE_CAP_BLEND_EQUATION_SEPARATE:
        case PIPE_CAP_TWO_SIDED_STENCIL:
        case PIPE_CAP_TEXTURE_MULTISAMPLE:
        case PIPE_CAP_TEXTURE_SWIZZLE:
        case PIPE_CAP_GLSL_OPTIMIZE_CONSERVATIVELY:
        case PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION:
        case PIPE_CAP_TEXTURE_BARRIER:
                return 1;

        case PIPE_CAP_TILE_RASTER_ORDER:
                return vc4_has_feature(screen,
                                       DRM_VC4_PARAM_SUPPORTS_FIXED_RCL_ORDER);

                /* lying for GL 2.0 */
        case PIPE_CAP_OCCLUSION_QUERY:
        case PIPE_CAP_POINT_SPRITE:
                return 1;

        case PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT:
                return 256;

        case PIPE_CAP_GLSL_FEATURE_LEVEL:
                return 120;

        case PIPE_CAP_MAX_VIEWPORTS:
                return 1;

        case PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT:
        case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
                return 1;

        case PIPE_CAP_MIXED_FRAMEBUFFER_SIZES:
        case PIPE_CAP_MIXED_COLOR_DEPTH_BITS:
                return 1;

                /* Unsupported features. */
        case PIPE_CAP_ANISOTROPIC_FILTER:
        case PIPE_CAP_TEXTURE_BUFFER_OBJECTS:
        case PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY:
        case PIPE_CAP_CUBE_MAP_ARRAY:
        case PIPE_CAP_TEXTURE_MIRROR_CLAMP:
        case PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR:
        case PIPE_CAP_MIXED_COLORBUFFER_FORMATS:
        case PIPE_CAP_SEAMLESS_CUBE_MAP:
        case PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION:
        case PIPE_CAP_TGSI_INSTANCEID:
        case PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY:
        case PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY:
        case PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY:
        case PIPE_CAP_COMPUTE:
        case PIPE_CAP_START_INSTANCE:
        case PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS:
        case PIPE_CAP_SHADER_STENCIL_EXPORT:
        case PIPE_CAP_TGSI_TEXCOORD:
        case PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER:
        case PIPE_CAP_CONDITIONAL_RENDER:
        case PIPE_CAP_PRIMITIVE_RESTART:
        case PIPE_CAP_SM3:
        case PIPE_CAP_INDEP_BLEND_ENABLE:
        case PIPE_CAP_INDEP_BLEND_FUNC:
        case PIPE_CAP_DEPTH_CLIP_DISABLE:
        case PIPE_CAP_SEAMLESS_CUBE_MAP_PER_TEXTURE:
        case PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT:
        case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER:
        case PIPE_CAP_TGSI_CAN_COMPACT_CONSTANTS:
        case PIPE_CAP_USER_VERTEX_BUFFERS:
        case PIPE_CAP_QUERY_PIPELINE_STATISTICS:
        case PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK:
        case PIPE_CAP_TGSI_VS_LAYER_VIEWPORT:
        case PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS:
        case PIPE_CAP_TEXTURE_GATHER_SM5:
        case PIPE_CAP_FAKE_SW_MSAA:
        case PIPE_CAP_TEXTURE_QUERY_LOD:
        case PIPE_CAP_SAMPLE_SHADING:
        case PIPE_CAP_TEXTURE_GATHER_OFFSETS:
        case PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET:
        case PIPE_CAP_TGSI_VS_WINDOW_SPACE_POSITION:
        case PIPE_CAP_MAX_TEXEL_OFFSET:
        case PIPE_CAP_MAX_VERTEX_STREAMS:
        case PIPE_CAP_DRAW_INDIRECT:
        case PIPE_CAP_MULTI_DRAW_INDIRECT:
        case PIPE_CAP_MULTI_DRAW_INDIRECT_PARAMS:
        case PIPE_CAP_TGSI_FS_FINE_DERIVATIVE:
        case PIPE_CAP_CONDITIONAL_RENDER_INVERTED:
        case PIPE_CAP_SAMPLER_VIEW_TARGET:
        case PIPE_CAP_CLIP_HALFZ:
        case PIPE_CAP_VERTEXID_NOBASE:
        case PIPE_CAP_POLYGON_OFFSET_CLAMP:
        case PIPE_CAP_MULTISAMPLE_Z_RESOLVE:
        case PIPE_CAP_RESOURCE_FROM_USER_MEMORY:
        case PIPE_CAP_DEVICE_RESET_STATUS_QUERY:
        case PIPE_CAP_MAX_SHADER_PATCH_VARYINGS:
        case PIPE_CAP_TEXTURE_FLOAT_LINEAR:
        case PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR:
        case PIPE_CAP_DEPTH_BOUNDS_TEST:
        case PIPE_CAP_TGSI_TXQS:
        case PIPE_CAP_FORCE_PERSAMPLE_INTERP:
        case PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS:
        case PIPE_CAP_CLEAR_TEXTURE:
        case PIPE_CAP_DRAW_PARAMETERS:
        case PIPE_CAP_TGSI_PACK_HALF_FLOAT:
        case PIPE_CAP_TGSI_FS_POSITION_IS_SYSVAL:
        case PIPE_CAP_TGSI_FS_FACE_IS_INTEGER_SYSVAL:
        case PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT:
        case PIPE_CAP_INVALIDATE_BUFFER:
        case PIPE_CAP_GENERATE_MIPMAP:
        case PIPE_CAP_STRING_MARKER:
        case PIPE_CAP_SURFACE_REINTERPRET_BLOCKS:
        case PIPE_CAP_QUERY_BUFFER_OBJECT:
        case PIPE_CAP_QUERY_MEMORY_INFO:
        case PIPE_CAP_PCI_GROUP:
        case PIPE_CAP_PCI_BUS:
        case PIPE_CAP_PCI_DEVICE:
        case PIPE_CAP_PCI_FUNCTION:
        case PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT:
        case PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR:
        case PIPE_CAP_CULL_DISTANCE:
        case PIPE_CAP_PRIMITIVE_RESTART_FOR_PATCHES:
        case PIPE_CAP_TGSI_VOTE:
        case PIPE_CAP_MAX_WINDOW_RECTANGLES:
        case PIPE_CAP_POLYGON_OFFSET_UNITS_UNSCALED:
        case PIPE_CAP_VIEWPORT_SUBPIXEL_BITS:
        case PIPE_CAP_TGSI_ARRAY_COMPONENTS:
        case PIPE_CAP_TGSI_CAN_READ_OUTPUTS:
        case PIPE_CAP_NATIVE_FENCE_FD:
        case PIPE_CAP_TGSI_FS_FBFETCH:
        case PIPE_CAP_TGSI_MUL_ZERO_WINS:
        case PIPE_CAP_DOUBLES:
        case PIPE_CAP_INT64:
        case PIPE_CAP_INT64_DIVMOD:
        case PIPE_CAP_TGSI_TEX_TXF_LZ:
        case PIPE_CAP_TGSI_CLOCK:
        case PIPE_CAP_POLYGON_MODE_FILL_RECTANGLE:
        case PIPE_CAP_SPARSE_BUFFER_PAGE_SIZE:
        case PIPE_CAP_TGSI_BALLOT:
        case PIPE_CAP_TGSI_TES_LAYER_VIEWPORT:
	case PIPE_CAP_CAN_BIND_CONST_BUFFER_AS_VERTEX:
        case PIPE_CAP_POST_DEPTH_COVERAGE:
        case PIPE_CAP_BINDLESS_TEXTURE:
        case PIPE_CAP_NIR_SAMPLERS_AS_DEREF:
        case PIPE_CAP_QUERY_SO_OVERFLOW:
	case PIPE_CAP_MEMOBJ:
        case PIPE_CAP_LOAD_CONSTBUF:
	case PIPE_CAP_TGSI_ANY_REG_AS_ADDRESS:
                return 0;

                /* Stream output. */
        case PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS:
        case PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME:
        case PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS:
        case PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS:
        case PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS:
                return 0;

                /* Geometry shader output, unsupported. */
        case PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES:
        case PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
                return 0;

                /* Texturing. */
        case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
        case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
                return VC4_MAX_MIP_LEVELS;
        case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
                /* Note: Not supported in hardware, just faking it. */
                return 5;
        case PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS:
                return 0;

                /* Render targets. */
        case PIPE_CAP_MAX_RENDER_TARGETS:
                return 1;

                /* Queries. */
        case PIPE_CAP_QUERY_TIME_ELAPSED:
        case PIPE_CAP_QUERY_TIMESTAMP:
                return 0;

        case PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET:
        case PIPE_CAP_MIN_TEXEL_OFFSET:
                return 0;

        case PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE:
                return 2048;

        case PIPE_CAP_ENDIANNESS:
                return PIPE_ENDIAN_LITTLE;

        case PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT:
                return 64;

        case PIPE_CAP_VENDOR_ID:
                return 0x14E4;
        case PIPE_CAP_DEVICE_ID:
                return 0xFFFFFFFF;
        case PIPE_CAP_ACCELERATED:
                return 1;
        case PIPE_CAP_VIDEO_MEMORY: {
                uint64_t system_memory;

                if (!os_get_total_physical_memory(&system_memory))
                        return 0;

                return (int)(system_memory >> 20);
        }
        case PIPE_CAP_UMA:
                return 1;

        default:
                fprintf(stderr, "unknown param %d\n", param);
                return 0;
        }
}

static float
vc4_screen_get_paramf(struct pipe_screen *pscreen, enum pipe_capf param)
{
        switch (param) {
        case PIPE_CAPF_MAX_LINE_WIDTH:
        case PIPE_CAPF_MAX_LINE_WIDTH_AA:
                return 32;

        case PIPE_CAPF_MAX_POINT_WIDTH:
        case PIPE_CAPF_MAX_POINT_WIDTH_AA:
                return 512.0f;

        case PIPE_CAPF_MAX_TEXTURE_ANISOTROPY:
                return 0.0f;
        case PIPE_CAPF_MAX_TEXTURE_LOD_BIAS:
                return 0.0f;
        case PIPE_CAPF_GUARD_BAND_LEFT:
        case PIPE_CAPF_GUARD_BAND_TOP:
        case PIPE_CAPF_GUARD_BAND_RIGHT:
        case PIPE_CAPF_GUARD_BAND_BOTTOM:
                return 0.0f;
        default:
                fprintf(stderr, "unknown paramf %d\n", param);
                return 0;
        }
}

static int
vc4_screen_get_shader_param(struct pipe_screen *pscreen,
                            enum pipe_shader_type shader,
                            enum pipe_shader_cap param)
{
        if (shader != PIPE_SHADER_VERTEX &&
            shader != PIPE_SHADER_FRAGMENT) {
                return 0;
        }

        /* this is probably not totally correct.. but it's a start: */
        switch (param) {
        case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
        case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
        case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
        case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
                return 16384;

        case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
                return vc4_screen(pscreen)->has_control_flow;

        case PIPE_SHADER_CAP_MAX_INPUTS:
                return 8;
        case PIPE_SHADER_CAP_MAX_OUTPUTS:
                return shader == PIPE_SHADER_FRAGMENT ? 1 : 8;
        case PIPE_SHADER_CAP_MAX_TEMPS:
                return 256; /* GL_MAX_PROGRAM_TEMPORARIES_ARB */
        case PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE:
                return 16 * 1024 * sizeof(float);
        case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
                return 1;
        case PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED:
                return 0;
        case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
                return 0;
        case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
                return 1;
        case PIPE_SHADER_CAP_SUBROUTINES:
                return 0;
        case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
                return 0;
        case PIPE_SHADER_CAP_INTEGERS:
                return 1;
        case PIPE_SHADER_CAP_INT64_ATOMICS:
        case PIPE_SHADER_CAP_FP16:
        case PIPE_SHADER_CAP_TGSI_DROUND_SUPPORTED:
        case PIPE_SHADER_CAP_TGSI_DFRACEXP_DLDEXP_SUPPORTED:
        case PIPE_SHADER_CAP_TGSI_LDEXP_SUPPORTED:
        case PIPE_SHADER_CAP_TGSI_FMA_SUPPORTED:
        case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
                return 0;
        case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
        case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS:
                return VC4_MAX_TEXTURE_SAMPLERS;
        case PIPE_SHADER_CAP_PREFERRED_IR:
                return PIPE_SHADER_IR_NIR;
        case PIPE_SHADER_CAP_SUPPORTED_IRS:
                return 0;
	case PIPE_SHADER_CAP_MAX_UNROLL_ITERATIONS_HINT:
		return 32;
        case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
        case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
	case PIPE_SHADER_CAP_LOWER_IF_THRESHOLD:
        case PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS:
                return 0;
        default:
                fprintf(stderr, "unknown shader param %d\n", param);
                return 0;
        }
        return 0;
}

static boolean
vc4_screen_is_format_supported(struct pipe_screen *pscreen,
                               enum pipe_format format,
                               enum pipe_texture_target target,
                               unsigned sample_count,
                               unsigned usage)
{
        struct vc4_screen *screen = vc4_screen(pscreen);
        unsigned retval = 0;

        if (sample_count > 1 && sample_count != VC4_MAX_SAMPLES)
                return FALSE;

        if ((target >= PIPE_MAX_TEXTURE_TYPES) ||
            !util_format_is_supported(format, usage)) {
                return FALSE;
        }

        if (usage & PIPE_BIND_VERTEX_BUFFER) {
                switch (format) {
                case PIPE_FORMAT_R32G32B32A32_FLOAT:
                case PIPE_FORMAT_R32G32B32_FLOAT:
                case PIPE_FORMAT_R32G32_FLOAT:
                case PIPE_FORMAT_R32_FLOAT:
                case PIPE_FORMAT_R32G32B32A32_SNORM:
                case PIPE_FORMAT_R32G32B32_SNORM:
                case PIPE_FORMAT_R32G32_SNORM:
                case PIPE_FORMAT_R32_SNORM:
                case PIPE_FORMAT_R32G32B32A32_SSCALED:
                case PIPE_FORMAT_R32G32B32_SSCALED:
                case PIPE_FORMAT_R32G32_SSCALED:
                case PIPE_FORMAT_R32_SSCALED:
                case PIPE_FORMAT_R16G16B16A16_UNORM:
                case PIPE_FORMAT_R16G16B16_UNORM:
                case PIPE_FORMAT_R16G16_UNORM:
                case PIPE_FORMAT_R16_UNORM:
                case PIPE_FORMAT_R16G16B16A16_SNORM:
                case PIPE_FORMAT_R16G16B16_SNORM:
                case PIPE_FORMAT_R16G16_SNORM:
                case PIPE_FORMAT_R16_SNORM:
                case PIPE_FORMAT_R16G16B16A16_USCALED:
                case PIPE_FORMAT_R16G16B16_USCALED:
                case PIPE_FORMAT_R16G16_USCALED:
                case PIPE_FORMAT_R16_USCALED:
                case PIPE_FORMAT_R16G16B16A16_SSCALED:
                case PIPE_FORMAT_R16G16B16_SSCALED:
                case PIPE_FORMAT_R16G16_SSCALED:
                case PIPE_FORMAT_R16_SSCALED:
                case PIPE_FORMAT_R8G8B8A8_UNORM:
                case PIPE_FORMAT_R8G8B8_UNORM:
                case PIPE_FORMAT_R8G8_UNORM:
                case PIPE_FORMAT_R8_UNORM:
                case PIPE_FORMAT_R8G8B8A8_SNORM:
                case PIPE_FORMAT_R8G8B8_SNORM:
                case PIPE_FORMAT_R8G8_SNORM:
                case PIPE_FORMAT_R8_SNORM:
                case PIPE_FORMAT_R8G8B8A8_USCALED:
                case PIPE_FORMAT_R8G8B8_USCALED:
                case PIPE_FORMAT_R8G8_USCALED:
                case PIPE_FORMAT_R8_USCALED:
                case PIPE_FORMAT_R8G8B8A8_SSCALED:
                case PIPE_FORMAT_R8G8B8_SSCALED:
                case PIPE_FORMAT_R8G8_SSCALED:
                case PIPE_FORMAT_R8_SSCALED:
                        retval |= PIPE_BIND_VERTEX_BUFFER;
                        break;
                default:
                        break;
                }
        }

        if ((usage & PIPE_BIND_RENDER_TARGET) &&
            vc4_rt_format_supported(format)) {
                retval |= PIPE_BIND_RENDER_TARGET;
        }

        if ((usage & PIPE_BIND_SAMPLER_VIEW) &&
            vc4_tex_format_supported(format) &&
            (format != PIPE_FORMAT_ETC1_RGB8 || screen->has_etc1)) {
                retval |= PIPE_BIND_SAMPLER_VIEW;
        }

        if ((usage & PIPE_BIND_DEPTH_STENCIL) &&
            (format == PIPE_FORMAT_S8_UINT_Z24_UNORM ||
             format == PIPE_FORMAT_X8Z24_UNORM)) {
                retval |= PIPE_BIND_DEPTH_STENCIL;
        }

        if ((usage & PIPE_BIND_INDEX_BUFFER) &&
            (format == PIPE_FORMAT_I8_UINT ||
             format == PIPE_FORMAT_I16_UINT)) {
                retval |= PIPE_BIND_INDEX_BUFFER;
        }

#if 0
        if (retval != usage) {
                fprintf(stderr,
                        "not supported: format=%s, target=%d, sample_count=%d, "
                        "usage=0x%x, retval=0x%x\n", util_format_name(format),
                        target, sample_count, usage, retval);
        }
#endif

        return retval == usage;
}

static void
vc4_screen_query_dmabuf_modifiers(struct pipe_screen *pscreen,
                                  enum pipe_format format, int max,
                                  uint64_t *modifiers,
                                  unsigned int *external_only,
                                  int *count)
{
        int m, i;
        uint64_t available_modifiers[] = {
                DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED,
                DRM_FORMAT_MOD_LINEAR,
        };
        struct vc4_screen *screen = vc4_screen(pscreen);
        int num_modifiers = screen->has_tiling_ioctl ? 2 : 1;

        if (!modifiers) {
                *count = num_modifiers;
                return;
        }

        *count = MIN2(max, num_modifiers);
        m = screen->has_tiling_ioctl ? 0 : 1;
        /* We support both modifiers (tiled and linear) for all sampler
         * formats, but if we don't have the DRM_VC4_GET_TILING ioctl
         * we shouldn't advertise the tiled formats.
         */
        for (i = 0; i < *count; i++) {
                modifiers[i] = available_modifiers[m++];
                if (external_only)
                        external_only[i] = false;
       }
}

#define PTR_TO_UINT(x) ((unsigned)((intptr_t)(x)))

static unsigned handle_hash(void *key)
{
    return PTR_TO_UINT(key);
}

static int handle_compare(void *key1, void *key2)
{
    return PTR_TO_UINT(key1) != PTR_TO_UINT(key2);
}

static bool
vc4_get_chip_info(struct vc4_screen *screen)
{
        struct drm_vc4_get_param ident0 = {
                .param = DRM_VC4_PARAM_V3D_IDENT0,
        };
        struct drm_vc4_get_param ident1 = {
                .param = DRM_VC4_PARAM_V3D_IDENT1,
        };
        int ret;

        ret = vc4_ioctl(screen->fd, DRM_IOCTL_VC4_GET_PARAM, &ident0);
        if (ret != 0) {
                if (errno == EINVAL) {
                        /* Backwards compatibility with 2835 kernels which
                         * only do V3D 2.1.
                         */
                        screen->v3d_ver = 21;
                        return true;
                } else {
                        fprintf(stderr, "Couldn't get V3D IDENT0: %s\n",
                                strerror(errno));
                        return false;
                }
        }
        ret = vc4_ioctl(screen->fd, DRM_IOCTL_VC4_GET_PARAM, &ident1);
        if (ret != 0) {
                fprintf(stderr, "Couldn't get V3D IDENT1: %s\n",
                        strerror(errno));
                return false;
        }

        uint32_t major = (ident0.value >> 24) & 0xff;
        uint32_t minor = (ident1.value >> 0) & 0xf;
        screen->v3d_ver = major * 10 + minor;

        if (screen->v3d_ver != 21 && screen->v3d_ver != 26) {
                fprintf(stderr,
                        "V3D %d.%d not supported by this version of Mesa.\n",
                        screen->v3d_ver / 10,
                        screen->v3d_ver % 10);
                return false;
        }

        return true;
}

struct pipe_screen *
vc4_screen_create(int fd, struct renderonly *ro)
{
        struct vc4_screen *screen = rzalloc(NULL, struct vc4_screen);
        struct pipe_screen *pscreen;

        pscreen = &screen->base;

        pscreen->destroy = vc4_screen_destroy;
        pscreen->get_param = vc4_screen_get_param;
        pscreen->get_paramf = vc4_screen_get_paramf;
        pscreen->get_shader_param = vc4_screen_get_shader_param;
        pscreen->context_create = vc4_context_create;
        pscreen->is_format_supported = vc4_screen_is_format_supported;

        screen->fd = fd;
        if (ro) {
                screen->ro = renderonly_dup(ro);
                if (!screen->ro) {
                        fprintf(stderr, "Failed to dup renderonly object\n");
                        ralloc_free(screen);
                        return NULL;
                }
        }

        list_inithead(&screen->bo_cache.time_list);
        (void) mtx_init(&screen->bo_handles_mutex, mtx_plain);
        screen->bo_handles = util_hash_table_create(handle_hash, handle_compare);

        screen->has_control_flow =
                vc4_has_feature(screen, DRM_VC4_PARAM_SUPPORTS_BRANCHES);
        screen->has_etc1 =
                vc4_has_feature(screen, DRM_VC4_PARAM_SUPPORTS_ETC1);
        screen->has_threaded_fs =
                vc4_has_feature(screen, DRM_VC4_PARAM_SUPPORTS_THREADED_FS);

        if (!vc4_get_chip_info(screen))
                goto fail;

        util_cpu_detect();

        slab_create_parent(&screen->transfer_pool, sizeof(struct vc4_transfer), 16);

        vc4_fence_init(screen);

        vc4_debug = debug_get_option_vc4_debug();
        if (vc4_debug & VC4_DEBUG_SHADERDB)
                vc4_debug |= VC4_DEBUG_NORAST;

#if USE_VC4_SIMULATOR
        vc4_simulator_init(screen);
#endif

        vc4_resource_screen_init(pscreen);

        pscreen->get_name = vc4_screen_get_name;
        pscreen->get_vendor = vc4_screen_get_vendor;
        pscreen->get_device_vendor = vc4_screen_get_vendor;
        pscreen->get_compiler_options = vc4_screen_get_compiler_options;
        pscreen->query_dmabuf_modifiers = vc4_screen_query_dmabuf_modifiers;

        return pscreen;

fail:
        close(fd);
        ralloc_free(pscreen);
        return NULL;
}
