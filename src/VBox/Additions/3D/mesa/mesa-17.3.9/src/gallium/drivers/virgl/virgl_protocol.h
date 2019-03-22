/*
 * Copyright 2014, 2015 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef VIRGL_PROTOCOL_H
#define VIRGL_PROTOCOL_H

#define VIRGL_QUERY_STATE_NEW 0
#define VIRGL_QUERY_STATE_DONE 1
#define VIRGL_QUERY_STATE_WAIT_HOST 2

struct virgl_host_query_state {
   uint32_t query_state;
   uint32_t result_size;
   uint64_t result;
};

enum virgl_object_type {
   VIRGL_OBJECT_NULL,
   VIRGL_OBJECT_BLEND,
   VIRGL_OBJECT_RASTERIZER,
   VIRGL_OBJECT_DSA,
   VIRGL_OBJECT_SHADER,
   VIRGL_OBJECT_VERTEX_ELEMENTS,
   VIRGL_OBJECT_SAMPLER_VIEW,
   VIRGL_OBJECT_SAMPLER_STATE,
   VIRGL_OBJECT_SURFACE,
   VIRGL_OBJECT_QUERY,
   VIRGL_OBJECT_STREAMOUT_TARGET,
   VIRGL_MAX_OBJECTS,
};

/* context cmds to be encoded in the command stream */
enum virgl_context_cmd {
   VIRGL_CCMD_NOP = 0,
   VIRGL_CCMD_CREATE_OBJECT = 1,
   VIRGL_CCMD_BIND_OBJECT,
   VIRGL_CCMD_DESTROY_OBJECT,
   VIRGL_CCMD_SET_VIEWPORT_STATE,
   VIRGL_CCMD_SET_FRAMEBUFFER_STATE,
   VIRGL_CCMD_SET_VERTEX_BUFFERS,
   VIRGL_CCMD_CLEAR,
   VIRGL_CCMD_DRAW_VBO,
   VIRGL_CCMD_RESOURCE_INLINE_WRITE,
   VIRGL_CCMD_SET_SAMPLER_VIEWS,
   VIRGL_CCMD_SET_INDEX_BUFFER,
   VIRGL_CCMD_SET_CONSTANT_BUFFER,
   VIRGL_CCMD_SET_STENCIL_REF,
   VIRGL_CCMD_SET_BLEND_COLOR,
   VIRGL_CCMD_SET_SCISSOR_STATE,
   VIRGL_CCMD_BLIT,
   VIRGL_CCMD_RESOURCE_COPY_REGION,
   VIRGL_CCMD_BIND_SAMPLER_STATES,
   VIRGL_CCMD_BEGIN_QUERY,
   VIRGL_CCMD_END_QUERY,
   VIRGL_CCMD_GET_QUERY_RESULT,
   VIRGL_CCMD_SET_POLYGON_STIPPLE,
   VIRGL_CCMD_SET_CLIP_STATE,
   VIRGL_CCMD_SET_SAMPLE_MASK,
   VIRGL_CCMD_SET_STREAMOUT_TARGETS,
   VIRGL_CCMD_SET_RENDER_CONDITION,
   VIRGL_CCMD_SET_UNIFORM_BUFFER,

   VIRGL_CCMD_SET_SUB_CTX,
   VIRGL_CCMD_CREATE_SUB_CTX,
   VIRGL_CCMD_DESTROY_SUB_CTX,
   VIRGL_CCMD_BIND_SHADER,
};

/*
 8-bit cmd headers
 8-bit object type
 16-bit length
*/

#define VIRGL_CMD0(cmd, obj, len) ((cmd) | ((obj) << 8) | ((len) << 16))

/* hw specification */
#define VIRGL_MAX_COLOR_BUFS 8
#define VIRGL_MAX_CLIP_PLANES 8

#define VIRGL_OBJ_CREATE_HEADER 0
#define VIRGL_OBJ_CREATE_HANDLE 1

#define VIRGL_OBJ_BIND_HEADER 0
#define VIRGL_OBJ_BIND_HANDLE 1

#define VIRGL_OBJ_DESTROY_HANDLE 1

/* some of these defines are a specification - not used in the code */
/* bit offsets for blend state object */
#define VIRGL_OBJ_BLEND_SIZE (VIRGL_MAX_COLOR_BUFS + 3)
#define VIRGL_OBJ_BLEND_HANDLE 1
#define VIRGL_OBJ_BLEND_S0 2
#define VIRGL_OBJ_BLEND_S0_INDEPENDENT_BLEND_ENABLE(x) ((x) & 0x1 << 0)
#define VIRGL_OBJ_BLEND_S0_LOGICOP_ENABLE(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_BLEND_S0_DITHER(x) (((x) & 0x1) << 2)
#define VIRGL_OBJ_BLEND_S0_ALPHA_TO_COVERAGE(x) (((x) & 0x1) << 3)
#define VIRGL_OBJ_BLEND_S0_ALPHA_TO_ONE(x) (((x) & 0x1) << 4)
#define VIRGL_OBJ_BLEND_S1 3
#define VIRGL_OBJ_BLEND_S1_LOGICOP_FUNC(x) (((x) & 0xf) << 0)
/* repeated once per number of cbufs */

#define VIRGL_OBJ_BLEND_S2(cbuf) (4 + (cbuf))
#define VIRGL_OBJ_BLEND_S2_RT_BLEND_ENABLE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_FUNC(x) (((x) & 0x7) << 1)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_SRC_FACTOR(x) (((x) & 0x1f) << 4)
#define VIRGL_OBJ_BLEND_S2_RT_RGB_DST_FACTOR(x) (((x) & 0x1f) << 9)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_FUNC(x) (((x) & 0x7) << 14)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_SRC_FACTOR(x) (((x) & 0x1f) << 17)
#define VIRGL_OBJ_BLEND_S2_RT_ALPHA_DST_FACTOR(x) (((x) & 0x1f) << 22)
#define VIRGL_OBJ_BLEND_S2_RT_COLORMASK(x) (((x) & 0xf) << 27)

/* bit offsets for DSA state */
#define VIRGL_OBJ_DSA_SIZE 5
#define VIRGL_OBJ_DSA_HANDLE 1
#define VIRGL_OBJ_DSA_S0 2
#define VIRGL_OBJ_DSA_S0_DEPTH_ENABLE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_DSA_S0_DEPTH_WRITEMASK(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_DSA_S0_DEPTH_FUNC(x) (((x) & 0x7) << 2)
#define VIRGL_OBJ_DSA_S0_ALPHA_ENABLED(x) (((x) & 0x1) << 8)
#define VIRGL_OBJ_DSA_S0_ALPHA_FUNC(x) (((x) & 0x7) << 9)
#define VIRGL_OBJ_DSA_S1 3
#define VIRGL_OBJ_DSA_S2 4
#define VIRGL_OBJ_DSA_S1_STENCIL_ENABLED(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_DSA_S1_STENCIL_FUNC(x) (((x) & 0x7) << 1)
#define VIRGL_OBJ_DSA_S1_STENCIL_FAIL_OP(x) (((x) & 0x7) << 4)
#define VIRGL_OBJ_DSA_S1_STENCIL_ZPASS_OP(x) (((x) & 0x7) << 7)
#define VIRGL_OBJ_DSA_S1_STENCIL_ZFAIL_OP(x) (((x) & 0x7) << 10)
#define VIRGL_OBJ_DSA_S1_STENCIL_VALUEMASK(x) (((x) & 0xff) << 13)
#define VIRGL_OBJ_DSA_S1_STENCIL_WRITEMASK(x) (((x) & 0xff) << 21)
#define VIRGL_OBJ_DSA_ALPHA_REF 5

/* offsets for rasterizer state */
#define VIRGL_OBJ_RS_SIZE 9
#define VIRGL_OBJ_RS_HANDLE 1
#define VIRGL_OBJ_RS_S0 2
#define VIRGL_OBJ_RS_S0_FLATSHADE(x) (((x) & 0x1) << 0)
#define VIRGL_OBJ_RS_S0_DEPTH_CLIP(x) (((x) & 0x1) << 1)
#define VIRGL_OBJ_RS_S0_CLIP_HALFZ(x) (((x) & 0x1) << 2)
#define VIRGL_OBJ_RS_S0_RASTERIZER_DISCARD(x) (((x) & 0x1) << 3)
#define VIRGL_OBJ_RS_S0_FLATSHADE_FIRST(x) (((x) & 0x1) << 4)
#define VIRGL_OBJ_RS_S0_LIGHT_TWOSIZE(x) (((x) & 0x1) << 5)
#define VIRGL_OBJ_RS_S0_SPRITE_COORD_MODE(x) (((x) & 0x1) << 6)
#define VIRGL_OBJ_RS_S0_POINT_QUAD_RASTERIZATION(x) (((x) & 0x1) << 7)
#define VIRGL_OBJ_RS_S0_CULL_FACE(x) (((x) & 0x3) << 8)
#define VIRGL_OBJ_RS_S0_FILL_FRONT(x) (((x) & 0x3) << 10)
#define VIRGL_OBJ_RS_S0_FILL_BACK(x) (((x) & 0x3) << 12)
#define VIRGL_OBJ_RS_S0_SCISSOR(x) (((x) & 0x1) << 14)
#define VIRGL_OBJ_RS_S0_FRONT_CCW(x) (((x) & 0x1) << 15)
#define VIRGL_OBJ_RS_S0_CLAMP_VERTEX_COLOR(x) (((x) & 0x1) << 16)
#define VIRGL_OBJ_RS_S0_CLAMP_FRAGMENT_COLOR(x) (((x) & 0x1) << 17)
#define VIRGL_OBJ_RS_S0_OFFSET_LINE(x) (((x) & 0x1) << 18)
#define VIRGL_OBJ_RS_S0_OFFSET_POINT(x) (((x) & 0x1) << 19)
#define VIRGL_OBJ_RS_S0_OFFSET_TRI(x) (((x) & 0x1) << 20)
#define VIRGL_OBJ_RS_S0_POLY_SMOOTH(x) (((x) & 0x1) << 21)
#define VIRGL_OBJ_RS_S0_POLY_STIPPLE_ENABLE(x) (((x) & 0x1) << 22)
#define VIRGL_OBJ_RS_S0_POINT_SMOOTH(x) (((x) & 0x1) << 23)
#define VIRGL_OBJ_RS_S0_POINT_SIZE_PER_VERTEX(x) (((x) & 0x1) << 24)
#define VIRGL_OBJ_RS_S0_MULTISAMPLE(x) (((x) & 0x1) << 25)
#define VIRGL_OBJ_RS_S0_LINE_SMOOTH(x) (((x) & 0x1) << 26)
#define VIRGL_OBJ_RS_S0_LINE_STIPPLE_ENABLE(x) (((x) & 0x1) << 27)
#define VIRGL_OBJ_RS_S0_LINE_LAST_PIXEL(x) (((x) & 0x1) << 28)
#define VIRGL_OBJ_RS_S0_HALF_PIXEL_CENTER(x) (((x) & 0x1) << 29)
#define VIRGL_OBJ_RS_S0_BOTTOM_EDGE_RULE(x) (((x) & 0x1) << 30)

#define VIRGL_OBJ_RS_POINT_SIZE 3
#define VIRGL_OBJ_RS_SPRITE_COORD_ENABLE 4
#define VIRGL_OBJ_RS_S3 5

#define VIRGL_OBJ_RS_S3_LINE_STIPPLE_PATTERN(x) (((x) & 0xffff) << 0)
#define VIRGL_OBJ_RS_S3_LINE_STIPPLE_FACTOR(x) (((x) & 0xff) << 16)
#define VIRGL_OBJ_RS_S3_CLIP_PLANE_ENABLE(x) (((x) & 0xff) << 24)
#define VIRGL_OBJ_RS_LINE_WIDTH 6
#define VIRGL_OBJ_RS_OFFSET_UNITS 7
#define VIRGL_OBJ_RS_OFFSET_SCALE 8
#define VIRGL_OBJ_RS_OFFSET_CLAMP 9

#define VIRGL_OBJ_CLEAR_SIZE 8
#define VIRGL_OBJ_CLEAR_BUFFERS 1
#define VIRGL_OBJ_CLEAR_COLOR_0 2 /* color is 4 * u32/f32/i32 */
#define VIRGL_OBJ_CLEAR_COLOR_1 3
#define VIRGL_OBJ_CLEAR_COLOR_2 4
#define VIRGL_OBJ_CLEAR_COLOR_3 5
#define VIRGL_OBJ_CLEAR_DEPTH_0 6 /* depth is a double precision float */
#define VIRGL_OBJ_CLEAR_DEPTH_1 7
#define VIRGL_OBJ_CLEAR_STENCIL 8

/* shader object */
#define VIRGL_OBJ_SHADER_HDR_SIZE(nso) (5 + ((nso) ? (2 * nso) + 4 : 0))
#define VIRGL_OBJ_SHADER_HANDLE 1
#define VIRGL_OBJ_SHADER_TYPE 2
#define VIRGL_OBJ_SHADER_OFFSET 3
#define VIRGL_OBJ_SHADER_OFFSET_VAL(x) (((x) & 0x7fffffff) << 0)
/* start contains full length in VAL - also implies continuations */
/* continuation contains offset in VAL */
#define VIRGL_OBJ_SHADER_OFFSET_CONT (0x1 << 31)
#define VIRGL_OBJ_SHADER_NUM_TOKENS 4
#define VIRGL_OBJ_SHADER_SO_NUM_OUTPUTS 5
#define VIRGL_OBJ_SHADER_SO_STRIDE(x) (6 + (x))
#define VIRGL_OBJ_SHADER_SO_OUTPUT0(x) (10 + (x * 2))
#define VIRGL_OBJ_SHADER_SO_OUTPUT_REGISTER_INDEX(x) (((x) & 0xff) << 0)
#define VIRGL_OBJ_SHADER_SO_OUTPUT_START_COMPONENT(x) (((x) & 0x3) << 8)
#define VIRGL_OBJ_SHADER_SO_OUTPUT_NUM_COMPONENTS(x) (((x) & 0x7) << 10)
#define VIRGL_OBJ_SHADER_SO_OUTPUT_BUFFER(x) (((x) & 0x7) << 13)
#define VIRGL_OBJ_SHADER_SO_OUTPUT_DST_OFFSET(x) (((x) & 0xffff) << 16)
#define VIRGL_OBJ_SHADER_SO_OUTPUT0_SO(x) (11 + (x * 2))
#define VIRGL_OBJ_SHADER_SO_OUTPUT_STREAM(x) (((x) & 0x03) << 0)

/* viewport state */
#define VIRGL_SET_VIEWPORT_STATE_SIZE(num_viewports) ((6 * num_viewports) + 1)
#define VIRGL_SET_VIEWPORT_START_SLOT 1
#define VIRGL_SET_VIEWPORT_STATE_SCALE_0(x) (2 + (x * 6))
#define VIRGL_SET_VIEWPORT_STATE_SCALE_1(x) (3 + (x * 6))
#define VIRGL_SET_VIEWPORT_STATE_SCALE_2(x) (4 + (x * 6))
#define VIRGL_SET_VIEWPORT_STATE_TRANSLATE_0(x) (5 + (x * 6))
#define VIRGL_SET_VIEWPORT_STATE_TRANSLATE_1(x) (6 + (x * 6))
#define VIRGL_SET_VIEWPORT_STATE_TRANSLATE_2(x) (7 + (x * 6))

/* framebuffer state */
#define VIRGL_SET_FRAMEBUFFER_STATE_SIZE(nr_cbufs) (nr_cbufs + 2)
#define VIRGL_SET_FRAMEBUFFER_STATE_NR_CBUFS 1
#define VIRGL_SET_FRAMEBUFFER_STATE_NR_ZSURF_HANDLE 2
#define VIRGL_SET_FRAMEBUFFER_STATE_CBUF_HANDLE(x) ((x) + 3)

/* vertex elements object */
#define VIRGL_OBJ_VERTEX_ELEMENTS_SIZE(num_elements) (((num_elements) * 4) + 1)
#define VIRGL_OBJ_VERTEX_ELEMENTS_HANDLE 1
#define VIRGL_OBJ_VERTEX_ELEMENTS_V0_SRC_OFFSET(x) (((x) * 4) + 2) /* repeated per VE */
#define VIRGL_OBJ_VERTEX_ELEMENTS_V0_INSTANCE_DIVISOR(x) (((x) * 4) + 3)
#define VIRGL_OBJ_VERTEX_ELEMENTS_V0_VERTEX_BUFFER_INDEX(x) (((x) * 4) + 4)
#define VIRGL_OBJ_VERTEX_ELEMENTS_V0_SRC_FORMAT(x) (((x) * 4) + 5)

/* vertex buffers */
#define VIRGL_SET_VERTEX_BUFFERS_SIZE(num_buffers) ((num_buffers) * 3)
#define VIRGL_SET_VERTEX_BUFFER_STRIDE(x) (((x) * 3) + 1)
#define VIRGL_SET_VERTEX_BUFFER_OFFSET(x) (((x) * 3) + 2)
#define VIRGL_SET_VERTEX_BUFFER_HANDLE(x) (((x) * 3) + 3)

/* index buffer */
#define VIRGL_SET_INDEX_BUFFER_SIZE(ib) (((ib) ? 2 : 0) + 1)
#define VIRGL_SET_INDEX_BUFFER_HANDLE 1
#define VIRGL_SET_INDEX_BUFFER_INDEX_SIZE 2 /* only if sending an IB handle */
#define VIRGL_SET_INDEX_BUFFER_OFFSET 3     /* only if sending an IB handle */

/* constant buffer */
#define VIRGL_SET_CONSTANT_BUFFER_SHADER_TYPE 1
#define VIRGL_SET_CONSTANT_BUFFER_INDEX 2
#define VIRGL_SET_CONSTANT_BUFFER_DATA_START 3

#define VIRGL_SET_UNIFORM_BUFFER_SIZE 5
#define VIRGL_SET_UNIFORM_BUFFER_SHADER_TYPE 1
#define VIRGL_SET_UNIFORM_BUFFER_INDEX 2
#define VIRGL_SET_UNIFORM_BUFFER_OFFSET 3
#define VIRGL_SET_UNIFORM_BUFFER_LENGTH 4
#define VIRGL_SET_UNIFORM_BUFFER_RES_HANDLE 5

/* draw VBO */
#define VIRGL_DRAW_VBO_SIZE 12
#define VIRGL_DRAW_VBO_START 1
#define VIRGL_DRAW_VBO_COUNT 2
#define VIRGL_DRAW_VBO_MODE 3
#define VIRGL_DRAW_VBO_INDEXED 4
#define VIRGL_DRAW_VBO_INSTANCE_COUNT 5
#define VIRGL_DRAW_VBO_INDEX_BIAS 6
#define VIRGL_DRAW_VBO_START_INSTANCE 7
#define VIRGL_DRAW_VBO_PRIMITIVE_RESTART 8
#define VIRGL_DRAW_VBO_RESTART_INDEX 9
#define VIRGL_DRAW_VBO_MIN_INDEX 10
#define VIRGL_DRAW_VBO_MAX_INDEX 11
#define VIRGL_DRAW_VBO_COUNT_FROM_SO 12

/* create surface */
#define VIRGL_OBJ_SURFACE_SIZE 5
#define VIRGL_OBJ_SURFACE_HANDLE 1
#define VIRGL_OBJ_SURFACE_RES_HANDLE 2
#define VIRGL_OBJ_SURFACE_FORMAT 3
#define VIRGL_OBJ_SURFACE_BUFFER_FIRST_ELEMENT 4
#define VIRGL_OBJ_SURFACE_BUFFER_LAST_ELEMENT 5
#define VIRGL_OBJ_SURFACE_TEXTURE_LEVEL 4
#define VIRGL_OBJ_SURFACE_TEXTURE_LAYERS 5

/* create streamout target */
#define VIRGL_OBJ_STREAMOUT_SIZE 4
#define VIRGL_OBJ_STREAMOUT_HANDLE 1
#define VIRGL_OBJ_STREAMOUT_RES_HANDLE 2
#define VIRGL_OBJ_STREAMOUT_BUFFER_OFFSET 3
#define VIRGL_OBJ_STREAMOUT_BUFFER_SIZE 4

/* sampler state */
#define VIRGL_OBJ_SAMPLER_STATE_SIZE 9
#define VIRGL_OBJ_SAMPLER_STATE_HANDLE 1
#define VIRGL_OBJ_SAMPLER_STATE_S0 2
#define VIRGL_OBJ_SAMPLE_STATE_S0_WRAP_S(x) (((x) & 0x7) << 0)
#define VIRGL_OBJ_SAMPLE_STATE_S0_WRAP_T(x) (((x) & 0x7) << 3)
#define VIRGL_OBJ_SAMPLE_STATE_S0_WRAP_R(x) (((x) & 0x7) << 6)
#define VIRGL_OBJ_SAMPLE_STATE_S0_MIN_IMG_FILTER(x) (((x) & 0x3) << 9)
#define VIRGL_OBJ_SAMPLE_STATE_S0_MIN_MIP_FILTER(x) (((x) & 0x3) << 11)
#define VIRGL_OBJ_SAMPLE_STATE_S0_MAG_IMG_FILTER(x) (((x) & 0x3) << 13)
#define VIRGL_OBJ_SAMPLE_STATE_S0_COMPARE_MODE(x) (((x) & 0x1) << 15)
#define VIRGL_OBJ_SAMPLE_STATE_S0_COMPARE_FUNC(x) (((x) & 0x7) << 16)

#define VIRGL_OBJ_SAMPLER_STATE_LOD_BIAS 3
#define VIRGL_OBJ_SAMPLER_STATE_MIN_LOD 4
#define VIRGL_OBJ_SAMPLER_STATE_MAX_LOD 5
#define VIRGL_OBJ_SAMPLER_STATE_BORDER_COLOR(x) ((x) + 6) /* 6 - 9 */


/* sampler view */
#define VIRGL_OBJ_SAMPLER_VIEW_SIZE 6
#define VIRGL_OBJ_SAMPLER_VIEW_HANDLE 1
#define VIRGL_OBJ_SAMPLER_VIEW_RES_HANDLE 2
#define VIRGL_OBJ_SAMPLER_VIEW_FORMAT 3
#define VIRGL_OBJ_SAMPLER_VIEW_BUFFER_FIRST_ELEMENT 4
#define VIRGL_OBJ_SAMPLER_VIEW_BUFFER_LAST_ELEMENT 5
#define VIRGL_OBJ_SAMPLER_VIEW_TEXTURE_LAYER 4
#define VIRGL_OBJ_SAMPLER_VIEW_TEXTURE_LEVEL 5
#define VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE 6
#define VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE_R(x) (((x) & 0x7) << 0)
#define VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE_G(x) (((x) & 0x7) << 3)
#define VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE_B(x) (((x) & 0x7) << 6)
#define VIRGL_OBJ_SAMPLER_VIEW_SWIZZLE_A(x) (((x) & 0x7) << 9)

/* set sampler views */
#define VIRGL_SET_SAMPLER_VIEWS_SIZE(num_views) ((num_views) + 2)
#define VIRGL_SET_SAMPLER_VIEWS_SHADER_TYPE 1
#define VIRGL_SET_SAMPLER_VIEWS_START_SLOT 2
#define VIRGL_SET_SAMPLER_VIEWS_V0_HANDLE 3

/* bind sampler states */
#define VIRGL_BIND_SAMPLER_STATES(num_states) ((num_states) + 2)
#define VIRGL_BIND_SAMPLER_STATES_SHADER_TYPE 1
#define VIRGL_BIND_SAMPLER_STATES_START_SLOT 2
#define VIRGL_BIND_SAMPLER_STATES_S0_HANDLE 3

/* set stencil reference */
#define VIRGL_SET_STENCIL_REF_SIZE 1
#define VIRGL_SET_STENCIL_REF 1
#define VIRGL_STENCIL_REF_VAL(f, s) ((f & 0xff) | (((s & 0xff) << 8)))

/* set blend color */
#define VIRGL_SET_BLEND_COLOR_SIZE 4
#define VIRGL_SET_BLEND_COLOR(x) ((x) + 1)

/* set scissor state */
#define VIRGL_SET_SCISSOR_STATE_SIZE(x) (1 + 2 * x)
#define VIRGL_SET_SCISSOR_START_SLOT 1
#define VIRGL_SET_SCISSOR_MINX_MINY(x) (2 + (x * 2))
#define VIRGL_SET_SCISSOR_MAXX_MAXY(x) (3 + (x * 2))

/* resource copy region */
#define VIRGL_CMD_RESOURCE_COPY_REGION_SIZE 13
#define VIRGL_CMD_RCR_DST_RES_HANDLE 1
#define VIRGL_CMD_RCR_DST_LEVEL 2
#define VIRGL_CMD_RCR_DST_X 3
#define VIRGL_CMD_RCR_DST_Y 4
#define VIRGL_CMD_RCR_DST_Z 5
#define VIRGL_CMD_RCR_SRC_RES_HANDLE 6
#define VIRGL_CMD_RCR_SRC_LEVEL 7
#define VIRGL_CMD_RCR_SRC_X 8
#define VIRGL_CMD_RCR_SRC_Y 9
#define VIRGL_CMD_RCR_SRC_Z 10
#define VIRGL_CMD_RCR_SRC_W 11
#define VIRGL_CMD_RCR_SRC_H 12
#define VIRGL_CMD_RCR_SRC_D 13

/* blit */
#define VIRGL_CMD_BLIT_SIZE 21
#define VIRGL_CMD_BLIT_S0 1
#define VIRGL_CMD_BLIT_S0_MASK(x) (((x) & 0xff) << 0)
#define VIRGL_CMD_BLIT_S0_FILTER(x) (((x) & 0x3) << 8)
#define VIRGL_CMD_BLIT_S0_SCISSOR_ENABLE(x) (((x) & 0x1) << 10)
#define VIRGL_CMD_BLIT_S0_RENDER_CONDITION_ENABLE(x) (((x) & 0x1) << 11)
#define VIRGL_CMD_BLIT_S0_ALPHA_BLEND(x) (((x) & 0x1) << 12)
#define VIRGL_CMD_BLIT_SCISSOR_MINX_MINY 2
#define VIRGL_CMD_BLIT_SCISSOR_MAXX_MAXY 3
#define VIRGL_CMD_BLIT_DST_RES_HANDLE 4
#define VIRGL_CMD_BLIT_DST_LEVEL 5
#define VIRGL_CMD_BLIT_DST_FORMAT 6
#define VIRGL_CMD_BLIT_DST_X 7
#define VIRGL_CMD_BLIT_DST_Y 8
#define VIRGL_CMD_BLIT_DST_Z 9
#define VIRGL_CMD_BLIT_DST_W 10
#define VIRGL_CMD_BLIT_DST_H 11
#define VIRGL_CMD_BLIT_DST_D 12
#define VIRGL_CMD_BLIT_SRC_RES_HANDLE 13
#define VIRGL_CMD_BLIT_SRC_LEVEL 14
#define VIRGL_CMD_BLIT_SRC_FORMAT 15
#define VIRGL_CMD_BLIT_SRC_X 16
#define VIRGL_CMD_BLIT_SRC_Y 17
#define VIRGL_CMD_BLIT_SRC_Z 18
#define VIRGL_CMD_BLIT_SRC_W 19
#define VIRGL_CMD_BLIT_SRC_H 20
#define VIRGL_CMD_BLIT_SRC_D 21

/* query object */
#define VIRGL_OBJ_QUERY_SIZE 4
#define VIRGL_OBJ_QUERY_HANDLE 1
#define VIRGL_OBJ_QUERY_TYPE_INDEX 2
#define VIRGL_OBJ_QUERY_TYPE(x) (x & 0xffff)
#define VIRGL_OBJ_QUERY_INDEX(x) ((x & 0xffff) << 16)
#define VIRGL_OBJ_QUERY_OFFSET 3
#define VIRGL_OBJ_QUERY_RES_HANDLE 4

#define VIRGL_QUERY_BEGIN_HANDLE 1

#define VIRGL_QUERY_END_HANDLE 1

#define VIRGL_QUERY_RESULT_HANDLE 1
#define VIRGL_QUERY_RESULT_WAIT 2

/* render condition */
#define VIRGL_RENDER_CONDITION_SIZE 3
#define VIRGL_RENDER_CONDITION_HANDLE 1
#define VIRGL_RENDER_CONDITION_CONDITION 2
#define VIRGL_RENDER_CONDITION_MODE 3

/* resource inline write */
#define VIRGL_RESOURCE_IW_RES_HANDLE 1
#define VIRGL_RESOURCE_IW_LEVEL 2
#define VIRGL_RESOURCE_IW_USAGE 3
#define VIRGL_RESOURCE_IW_STRIDE 4
#define VIRGL_RESOURCE_IW_LAYER_STRIDE 5
#define VIRGL_RESOURCE_IW_X 6
#define VIRGL_RESOURCE_IW_Y 7
#define VIRGL_RESOURCE_IW_Z 8
#define VIRGL_RESOURCE_IW_W 9
#define VIRGL_RESOURCE_IW_H 10
#define VIRGL_RESOURCE_IW_D 11
#define VIRGL_RESOURCE_IW_DATA_START 12

/* set streamout targets */
#define VIRGL_SET_STREAMOUT_TARGETS_APPEND_BITMASK 1
#define VIRGL_SET_STREAMOUT_TARGETS_H0 2

/* set sample mask */
#define VIRGL_SET_SAMPLE_MASK_SIZE 1
#define VIRGL_SET_SAMPLE_MASK_MASK 1

/* set clip state */
#define VIRGL_SET_CLIP_STATE_SIZE 32
#define VIRGL_SET_CLIP_STATE_C0 1

/* polygon stipple */
#define VIRGL_POLYGON_STIPPLE_SIZE 32
#define VIRGL_POLYGON_STIPPLE_P0 1

#define VIRGL_BIND_SHADER_SIZE 2
#define VIRGL_BIND_SHADER_HANDLE 1
#define VIRGL_BIND_SHADER_TYPE 2

#endif
