/*
 * Direct3D wine OpenGL include file
 *
 * Copyright 2002-2003 The wine-d3d team
 * Copyright 2002-2004 Jason Edmeades
 *                     Raphael Junqueira
 * Copyright 2007 Roderick Colenbrander
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 *
 * Most OpenGL 1.0/1.1/1.2/1.3 constants/types come from the Mesa-project:
 * Copyright (C) 1999-2006  Brian Paul
 *
 * From the Mesa-license:
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Oracle LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Oracle elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */

#ifndef VBOX_INCLUDED_SRC_Graphics_shaderlib_wined3d_gl_h
#define VBOX_INCLUDED_SRC_Graphics_shaderlib_wined3d_gl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef USE_WIN32_OPENGL
#define WINE_GLAPI __stdcall
#else
#define WINE_GLAPI
#endif

/****************************************************
 * OpenGL 1.0/1.1/1.2/1.3
 *     types, #defines and function pointers
 ****************************************************/

/* Types */
typedef unsigned int    GLbitfield;
typedef unsigned char   GLboolean;
typedef signed char     GLbyte;
typedef unsigned char   GLubyte;
typedef short           GLshort;
typedef unsigned short  GLushort;
typedef int             GLint;
typedef unsigned int    GLuint;
typedef unsigned int    GLenum;
typedef float           GLfloat;
typedef int             GLsizei;
typedef float           GLclampf;
typedef double          GLdouble;
typedef double          GLclampd;
typedef void            GLvoid;
typedef ptrdiff_t       GLintptr;
typedef ptrdiff_t       GLsizeiptr;
typedef INT64           GLint64;
typedef UINT64          GLuint64;
typedef struct __GLsync *GLsync;

/* Booleans */
#define GL_FALSE                                0x0
#define GL_TRUE                                 0x1

/* Data types */
#define GL_BYTE                                 0x1400
#define GL_UNSIGNED_BYTE                        0x1401
#define GL_SHORT                                0x1402
#define GL_UNSIGNED_SHORT                       0x1403
#define GL_INT                                  0x1404
#define GL_UNSIGNED_INT                         0x1405
#define GL_FLOAT                                0x1406
#define GL_DOUBLE                               0x140A
#define GL_2_BYTES                              0x1407
#define GL_3_BYTES                              0x1408
#define GL_4_BYTES                              0x1409

/* Errors */
#define GL_NO_ERROR                             0x0
#define GL_INVALID_VALUE                        0x0501
#define GL_INVALID_ENUM                         0x0500
#define GL_INVALID_OPERATION                    0x0502
#define GL_STACK_OVERFLOW                       0x0503
#define GL_STACK_UNDERFLOW                      0x0504
#define GL_OUT_OF_MEMORY                        0x0505

/* Utility */
#define GL_VENDOR                               0x1F00
#define GL_RENDERER                             0x1F01
#define GL_VERSION                              0x1F02
#define GL_EXTENSIONS                           0x1F03

/* Accumulation buffer */
#define GL_ACCUM_RED_BITS                       0x0D58
#define GL_ACCUM_GREEN_BITS                     0x0D59
#define GL_ACCUM_BLUE_BITS                      0x0D5A
#define GL_ACCUM_ALPHA_BITS                     0x0D5B
#define GL_ACCUM_CLEAR_VALUE                    0x0B80
#define GL_ACCUM                                0x0100
#define GL_ADD                                  0x0104
#define GL_LOAD                                 0x0101
#define GL_MULT                                 0x0103
#define GL_RETURN                               0x0102

/* Alpha testing */
#define GL_ALPHA_TEST                           0x0BC0
#define GL_ALPHA_TEST_REF                       0x0BC2
#define GL_ALPHA_TEST_FUNC                      0x0BC1

/* Blending */
#define GL_BLEND                                0x0BE2
#define GL_BLEND_SRC                            0x0BE1
#define GL_BLEND_DST                            0x0BE0
#define GL_ZERO                                 0x0
#define GL_ONE                                  0x1
#define GL_SRC_COLOR                            0x0300
#define GL_ONE_MINUS_SRC_COLOR                  0x0301
#define GL_SRC_ALPHA                            0x0302
#define GL_ONE_MINUS_SRC_ALPHA                  0x0303
#define GL_DST_ALPHA                            0x0304
#define GL_ONE_MINUS_DST_ALPHA                  0x0305
#define GL_DST_COLOR                            0x0306
#define GL_ONE_MINUS_DST_COLOR                  0x0307
#define GL_SRC_ALPHA_SATURATE                   0x0308
#define GL_CONSTANT_COLOR                       0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR             0x8002
#define GL_CONSTANT_ALPHA                       0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA             0x8004

/* Buffers, Pixel Drawing/Reading */
#define GL_NONE                                 0x0
#define GL_FRONT_LEFT                           0x0400
#define GL_FRONT_RIGHT                          0x0401
#define GL_BACK_LEFT                            0x0402
#define GL_BACK_RIGHT                           0x0403
#define GL_FRONT                                0x0404
#define GL_BACK                                 0x0405
#define GL_LEFT                                 0x0406
#define GL_RIGHT                                0x0407
#define GL_FRONT_AND_BACK                       0x0408
#define GL_AUX0                                 0x0409
#define GL_AUX1                                 0x040A
#define GL_AUX2                                 0x040B
#define GL_AUX3                                 0x040C
#define GL_COLOR_INDEX                          0x1900
#define GL_RED                                  0x1903
#define GL_GREEN                                0x1904
#define GL_BLUE                                 0x1905
#define GL_ALPHA                                0x1906
#define GL_LUMINANCE                            0x1909
#define GL_LUMINANCE_ALPHA                      0x190A
#define GL_ALPHA_BITS                           0x0D55
#define GL_RED_BITS                             0x0D52
#define GL_GREEN_BITS                           0x0D53
#define GL_BLUE_BITS                            0x0D54
#define GL_INDEX_BITS                           0x0D51
#define GL_SUBPIXEL_BITS                        0x0D50
#define GL_AUX_BUFFERS                          0x0C00
#define GL_READ_BUFFER                          0x0C02
#define GL_DRAW_BUFFER                          0x0C01
#define GL_DOUBLEBUFFER                         0x0C32
#define GL_STEREO                               0x0C33
#define GL_BITMAP                               0x1A00
#define GL_COLOR                                0x1800
#define GL_DEPTH                                0x1801
#define GL_STENCIL                              0x1802
#define GL_DITHER                               0x0BD0
#define GL_RGB                                  0x1907
#define GL_RGBA                                 0x1908

/* Clipping */
#define GL_CLIP_PLANE0                          0x3000
#define GL_CLIP_PLANE1                          0x3001
#define GL_CLIP_PLANE2                          0x3002
#define GL_CLIP_PLANE3                          0x3003
#define GL_CLIP_PLANE4                          0x3004
#define GL_CLIP_PLANE5                          0x3005

/* Depth buffer */
#define GL_NEVER                                0x0200
#define GL_LESS                                 0x0201
#define GL_EQUAL                                0x0202
#define GL_LEQUAL                               0x0203
#define GL_GREATER                              0x0204
#define GL_NOTEQUAL                             0x0205
#define GL_GEQUAL                               0x0206
#define GL_ALWAYS                               0x0207
#define GL_DEPTH_TEST                           0x0B71
#define GL_DEPTH_BITS                           0x0D56
#define GL_DEPTH_CLEAR_VALUE                    0x0B73
#define GL_DEPTH_FUNC                           0x0B74
#define GL_DEPTH_RANGE                          0x0B70
#define GL_DEPTH_WRITEMASK                      0x0B72
#define GL_DEPTH_COMPONENT                      0x1902

/* Evaluators */
#define GL_AUTO_NORMAL                          0x0D80
#define GL_MAP1_COLOR_4                         0x0D90
#define GL_MAP1_GRID_DOMAIN                     0x0DD0
#define GL_MAP1_GRID_SEGMENTS                   0x0DD1
#define GL_MAP1_INDEX                           0x0D91
#define GL_MAP1_NORMAL                          0x0D92
#define GL_MAP1_TEXTURE_COORD_1                 0x0D93
#define GL_MAP1_TEXTURE_COORD_2                 0x0D94
#define GL_MAP1_TEXTURE_COORD_3                 0x0D95
#define GL_MAP1_TEXTURE_COORD_4                 0x0D96
#define GL_MAP1_VERTEX_3                        0x0D97
#define GL_MAP1_VERTEX_4                        0x0D98
#define GL_MAP2_COLOR_4                         0x0DB0
#define GL_MAP2_GRID_DOMAIN                     0x0DD2
#define GL_MAP2_GRID_SEGMENTS                   0x0DD3
#define GL_MAP2_INDEX                           0x0DB1
#define GL_MAP2_NORMAL                          0x0DB2
#define GL_MAP2_TEXTURE_COORD_1                 0x0DB3
#define GL_MAP2_TEXTURE_COORD_2                 0x0DB4
#define GL_MAP2_TEXTURE_COORD_3                 0x0DB5
#define GL_MAP2_TEXTURE_COORD_4                 0x0DB6
#define GL_MAP2_VERTEX_3                        0x0DB7
#define GL_MAP2_VERTEX_4                        0x0DB8
#define GL_COEFF                                0x0A00
#define GL_DOMAIN                               0x0A02
#define GL_ORDER                                0x0A01

/* Feedback */
#define GL_2D                                   0x0600
#define GL_3D                                   0x0601
#define GL_3D_COLOR                             0x0602
#define GL_3D_COLOR_TEXTURE                     0x0603
#define GL_4D_COLOR_TEXTURE                     0x0604
#define GL_POINT_TOKEN                          0x0701
#define GL_LINE_TOKEN                           0x0702
#define GL_LINE_RESET_TOKEN                     0x0707
#define GL_POLYGON_TOKEN                        0x0703
#define GL_BITMAP_TOKEN                         0x0704
#define GL_DRAW_PIXEL_TOKEN                     0x0705
#define GL_COPY_PIXEL_TOKEN                     0x0706
#define GL_PASS_THROUGH_TOKEN                   0x0700
#define GL_FEEDBACK_BUFFER_POINTER              0x0DF0
#define GL_FEEDBACK_BUFFER_SIZE                 0x0DF1
#define GL_FEEDBACK_BUFFER_TYPE                 0x0DF2

/* Fog */
#define GL_FOG                                  0x0B60
#define GL_FOG_MODE                             0x0B65
#define GL_FOG_DENSITY                          0x0B62
#define GL_FOG_COLOR                            0x0B66
#define GL_FOG_INDEX                            0x0B61
#define GL_FOG_START                            0x0B63
#define GL_FOG_END                              0x0B64
#define GL_LINEAR                               0x2601
#define GL_EXP                                  0x0800
#define GL_EXP2                                 0x0801

/* Gets */
#define GL_ATTRIB_STACK_DEPTH                   0x0BB0
#define GL_CLIENT_ATTRIB_STACK_DEPTH            0x0BB1
#define GL_COLOR_CLEAR_VALUE                    0x0C22
#define GL_COLOR_WRITEMASK                      0x0C23
#define GL_CURRENT_INDEX                        0x0B01
#define GL_CURRENT_COLOR                        0x0B00
#define GL_CURRENT_NORMAL                       0x0B02
#define GL_CURRENT_RASTER_COLOR                 0x0B04
#define GL_CURRENT_RASTER_DISTANCE              0x0B09
#define GL_CURRENT_RASTER_INDEX                 0x0B05
#define GL_CURRENT_RASTER_POSITION              0x0B07
#define GL_CURRENT_RASTER_TEXTURE_COORDS        0x0B06
#define GL_CURRENT_RASTER_POSITION_VALID        0x0B08
#define GL_CURRENT_TEXTURE_COORDS               0x0B03
#define GL_INDEX_CLEAR_VALUE                    0x0C20
#define GL_INDEX_MODE                           0x0C30
#define GL_INDEX_WRITEMASK                      0x0C21
#define GL_MODELVIEW_MATRIX                     0x0BA6
#define GL_MODELVIEW_STACK_DEPTH                0x0BA3
#define GL_NAME_STACK_DEPTH                     0x0D70
#define GL_PROJECTION_MATRIX                    0x0BA7
#define GL_PROJECTION_STACK_DEPTH               0x0BA4
#define GL_RENDER_MODE                          0x0C40
#define GL_RGBA_MODE                            0x0C31
#define GL_TEXTURE_MATRIX                       0x0BA8
#define GL_TEXTURE_STACK_DEPTH                  0x0BA5
#define GL_VIEWPORT                             0x0BA2

/* Hints */
#define GL_FOG_HINT                             0x0C54
#define GL_LINE_SMOOTH_HINT                     0x0C52
#define GL_PERSPECTIVE_CORRECTION_HINT          0x0C50
#define GL_POINT_SMOOTH_HINT                    0x0C51
#define GL_POLYGON_SMOOTH_HINT                  0x0C53
#define GL_DONT_CARE                            0x1100
#define GL_FASTEST                              0x1101
#define GL_NICEST                               0x1102

/* Implementation limits */
#define GL_MAX_LIST_NESTING                     0x0B31
#define GL_MAX_ATTRIB_STACK_DEPTH               0x0D35
#define GL_MAX_MODELVIEW_STACK_DEPTH            0x0D36
#define GL_MAX_NAME_STACK_DEPTH                 0x0D37
#define GL_MAX_PROJECTION_STACK_DEPTH           0x0D38
#define GL_MAX_TEXTURE_STACK_DEPTH              0x0D39
#define GL_MAX_EVAL_ORDER                       0x0D30
#define GL_MAX_LIGHTS                           0x0D31
#define GL_MAX_CLIP_PLANES                      0x0D32
#define GL_MAX_TEXTURE_SIZE                     0x0D33
#define GL_MAX_PIXEL_MAP_TABLE                  0x0D34
#define GL_MAX_VIEWPORT_DIMS                    0x0D3A
#define GL_MAX_CLIENT_ATTRIB_STACK_DEPTH        0x0D3B

/* Lighting */
#define GL_LIGHTING                             0x0B50
#define GL_LIGHT0                               0x4000
#define GL_LIGHT1                               0x4001
#define GL_LIGHT2                               0x4002
#define GL_LIGHT3                               0x4003
#define GL_LIGHT4                               0x4004
#define GL_LIGHT5                               0x4005
#define GL_LIGHT6                               0x4006
#define GL_LIGHT7                               0x4007
#define GL_SPOT_EXPONENT                        0x1205
#define GL_SPOT_CUTOFF                          0x1206
#define GL_CONSTANT_ATTENUATION                 0x1207
#define GL_LINEAR_ATTENUATION                   0x1208
#define GL_QUADRATIC_ATTENUATION                0x1209
#define GL_AMBIENT                              0x1200
#define GL_DIFFUSE                              0x1201
#define GL_SPECULAR                             0x1202
#define GL_SHININESS                            0x1601
#define GL_EMISSION                             0x1600
#define GL_POSITION                             0x1203
#define GL_SPOT_DIRECTION                       0x1204
#define GL_AMBIENT_AND_DIFFUSE                  0x1602
#define GL_COLOR_INDEXES                        0x1603
#define GL_LIGHT_MODEL_TWO_SIDE                 0x0B52
#define GL_LIGHT_MODEL_LOCAL_VIEWER             0x0B51
#define GL_LIGHT_MODEL_AMBIENT                  0x0B53
#define GL_FRONT_AND_BACK                       0x0408
#define GL_SHADE_MODEL                          0x0B54
#define GL_FLAT                                 0x1D00
#define GL_SMOOTH                               0x1D01
#define GL_COLOR_MATERIAL                       0x0B57
#define GL_COLOR_MATERIAL_FACE                  0x0B55
#define GL_COLOR_MATERIAL_PARAMETER             0x0B56
#define GL_NORMALIZE                            0x0BA1

/* Lines */
#define GL_LINE_SMOOTH                          0x0B20
#define GL_LINE_STIPPLE                         0x0B24
#define GL_LINE_STIPPLE_PATTERN                 0x0B25
#define GL_LINE_STIPPLE_REPEAT                  0x0B26
#define GL_LINE_WIDTH                           0x0B21
#define GL_LINE_WIDTH_GRANULARITY               0x0B23
#define GL_LINE_WIDTH_RANGE                     0x0B22

/* Logic Ops */
#define GL_LOGIC_OP                             0x0BF1
#define GL_INDEX_LOGIC_OP                       0x0BF1
#define GL_COLOR_LOGIC_OP                       0x0BF2
#define GL_LOGIC_OP_MODE                        0x0BF0
#define GL_CLEAR                                0x1500
#define GL_SET                                  0x150F
#define GL_COPY                                 0x1503
#define GL_COPY_INVERTED                        0x150C
#define GL_NOOP                                 0x1505
#define GL_INVERT                               0x150A
#define GL_AND                                  0x1501
#define GL_NAND                                 0x150E
#define GL_OR                                   0x1507
#define GL_NOR                                  0x1508
#define GL_XOR                                  0x1506
#define GL_EQUIV                                0x1509
#define GL_AND_REVERSE                          0x1502
#define GL_AND_INVERTED                         0x1504
#define GL_OR_REVERSE                           0x150B
#define GL_OR_INVERTED                          0x150D

/* Matrix Mode */
#define GL_MATRIX_MODE                          0x0BA0
#define GL_MODELVIEW                            0x1700
#define GL_PROJECTION                           0x1701
#define GL_TEXTURE                              0x1702

/* Pixel Mode / Transfer */
#define GL_MAP_COLOR                            0x0D10
#define GL_MAP_STENCIL                          0x0D11
#define GL_INDEX_SHIFT                          0x0D12
#define GL_INDEX_OFFSET                         0x0D13
#define GL_RED_SCALE                            0x0D14
#define GL_RED_BIAS                             0x0D15
#define GL_GREEN_SCALE                          0x0D18
#define GL_GREEN_BIAS                           0x0D19
#define GL_BLUE_SCALE                           0x0D1A
#define GL_BLUE_BIAS                            0x0D1B
#define GL_ALPHA_SCALE                          0x0D1C
#define GL_ALPHA_BIAS                           0x0D1D
#define GL_DEPTH_SCALE                          0x0D1E
#define GL_DEPTH_BIAS                           0x0D1F
#define GL_PIXEL_MAP_S_TO_S_SIZE                0x0CB1
#define GL_PIXEL_MAP_I_TO_I_SIZE                0x0CB0
#define GL_PIXEL_MAP_I_TO_R_SIZE                0x0CB2
#define GL_PIXEL_MAP_I_TO_G_SIZE                0x0CB3
#define GL_PIXEL_MAP_I_TO_B_SIZE                0x0CB4
#define GL_PIXEL_MAP_I_TO_A_SIZE                0x0CB5
#define GL_PIXEL_MAP_R_TO_R_SIZE                0x0CB6
#define GL_PIXEL_MAP_G_TO_G_SIZE                0x0CB7
#define GL_PIXEL_MAP_B_TO_B_SIZE                0x0CB8
#define GL_PIXEL_MAP_A_TO_A_SIZE                0x0CB9
#define GL_PIXEL_MAP_S_TO_S                     0x0C71
#define GL_PIXEL_MAP_I_TO_I                     0x0C70
#define GL_PIXEL_MAP_I_TO_R                     0x0C72
#define GL_PIXEL_MAP_I_TO_G                     0x0C73
#define GL_PIXEL_MAP_I_TO_B                     0x0C74
#define GL_PIXEL_MAP_I_TO_A                     0x0C75
#define GL_PIXEL_MAP_R_TO_R                     0x0C76
#define GL_PIXEL_MAP_G_TO_G                     0x0C77
#define GL_PIXEL_MAP_B_TO_B                     0x0C78
#define GL_PIXEL_MAP_A_TO_A                     0x0C79
#define GL_PACK_ALIGNMENT                       0x0D05
#define GL_PACK_LSB_FIRST                       0x0D01
#define GL_PACK_ROW_LENGTH                      0x0D02
#define GL_PACK_SKIP_PIXELS                     0x0D04
#define GL_PACK_SKIP_ROWS                       0x0D03
#define GL_PACK_SWAP_BYTES                      0x0D00
#define GL_UNPACK_ALIGNMENT                     0x0CF5
#define GL_UNPACK_LSB_FIRST                     0x0CF1
#define GL_UNPACK_ROW_LENGTH                    0x0CF2
#define GL_UNPACK_SKIP_PIXELS                   0x0CF4
#define GL_UNPACK_SKIP_ROWS                     0x0CF3
#define GL_UNPACK_SWAP_BYTES                    0x0CF0
#define GL_ZOOM_X                               0x0D16
#define GL_ZOOM_Y                               0x0D17

/* Points */
#define GL_POINT_SMOOTH                         0x0B10
#define GL_POINT_SIZE                           0x0B11
#define GL_POINT_SIZE_GRANULARITY               0x0B13
#define GL_POINT_SIZE_RANGE                     0x0B12

/* Polygons */
#define GL_POINT                                0x1B00
#define GL_LINE                                 0x1B01
#define GL_FILL                                 0x1B02
#define GL_CW                                   0x0900
#define GL_CCW                                  0x0901
#define GL_FRONT                                0x0404
#define GL_BACK                                 0x0405
#define GL_POLYGON_MODE                         0x0B40
#define GL_POLYGON_SMOOTH                       0x0B41
#define GL_POLYGON_STIPPLE                      0x0B42
#define GL_EDGE_FLAG                            0x0B43
#define GL_CULL_FACE                            0x0B44
#define GL_CULL_FACE_MODE                       0x0B45
#define GL_FRONT_FACE                           0x0B46
#define GL_POLYGON_OFFSET_FACTOR                0x8038
#define GL_POLYGON_OFFSET_UNITS                 0x2A00
#define GL_POLYGON_OFFSET_POINT                 0x2A01
#define GL_POLYGON_OFFSET_LINE                  0x2A02
#define GL_POLYGON_OFFSET_FILL                  0x8037

/* Primitives */
#define GL_POINTS                               0x0000
#define GL_LINES                                0x0001
#define GL_LINE_LOOP                            0x0002
#define GL_LINE_STRIP                           0x0003
#define GL_TRIANGLES                            0x0004
#define GL_TRIANGLE_STRIP                       0x0005
#define GL_TRIANGLE_FAN                         0x0006
#define GL_QUADS                                0x0007
#define GL_QUAD_STRIP                           0x0008
#define GL_POLYGON                              0x0009

/* Push/Pop bits */
#define GL_CURRENT_BIT                          0x00000001
#define GL_POINT_BIT                            0x00000002
#define GL_LINE_BIT                             0x00000004
#define GL_POLYGON_BIT                          0x00000008
#define GL_POLYGON_STIPPLE_BIT                  0x00000010
#define GL_PIXEL_MODE_BIT                       0x00000020
#define GL_LIGHTING_BIT                         0x00000040
#define GL_FOG_BIT                              0x00000080
#define GL_DEPTH_BUFFER_BIT                     0x00000100
#define GL_ACCUM_BUFFER_BIT                     0x00000200
#define GL_STENCIL_BUFFER_BIT                   0x00000400
#define GL_VIEWPORT_BIT                         0x00000800
#define GL_TRANSFORM_BIT                        0x00001000
#define GL_ENABLE_BIT                           0x00002000
#define GL_COLOR_BUFFER_BIT                     0x00004000
#define GL_HINT_BIT                             0x00008000
#define GL_EVAL_BIT                             0x00010000
#define GL_LIST_BIT                             0x00020000
#define GL_TEXTURE_BIT                          0x00040000
#define GL_SCISSOR_BIT                          0x00080000
#define GL_ALL_ATTRIB_BITS                      0x000FFFFF

/* Render Mode */
#define GL_FEEDBACK                             0x1C01
#define GL_RENDER                               0x1C00
#define GL_SELECT                               0x1C02

/* Scissor box */
#define GL_SCISSOR_TEST                         0x0C11
#define GL_SCISSOR_BOX                          0x0C10

/* Stencil */
#define GL_STENCIL_TEST                         0x0B90
#define GL_STENCIL_WRITEMASK                    0x0B98
#define GL_STENCIL_BITS                         0x0D57
#define GL_STENCIL_FUNC                         0x0B92
#define GL_STENCIL_VALUE_MASK                   0x0B93
#define GL_STENCIL_REF                          0x0B97
#define GL_STENCIL_FAIL                         0x0B94
#define GL_STENCIL_PASS_DEPTH_PASS              0x0B96
#define GL_STENCIL_PASS_DEPTH_FAIL              0x0B95
#define GL_STENCIL_CLEAR_VALUE                  0x0B91
#define GL_STENCIL_INDEX                        0x1901
#define GL_KEEP                                 0x1E00
#define GL_REPLACE                              0x1E01
#define GL_INCR                                 0x1E02
#define GL_DECR                                 0x1E03

/* Texture mapping */
#define GL_TEXTURE_ENV                          0x2300
#define GL_TEXTURE_ENV_MODE                     0x2200
#define GL_TEXTURE_1D                           0x0DE0
#define GL_TEXTURE_2D                           0x0DE1
#define GL_TEXTURE_WRAP_S                       0x2802
#define GL_TEXTURE_WRAP_T                       0x2803
#define GL_TEXTURE_MAG_FILTER                   0x2800
#define GL_TEXTURE_MIN_FILTER                   0x2801
#define GL_TEXTURE_ENV_COLOR                    0x2201
#define GL_TEXTURE_GEN_S                        0x0C60
#define GL_TEXTURE_GEN_T                        0x0C61
#define GL_TEXTURE_GEN_MODE                     0x2500
#define GL_TEXTURE_BORDER_COLOR                 0x1004
#define GL_TEXTURE_WIDTH                        0x1000
#define GL_TEXTURE_HEIGHT                       0x1001
#define GL_TEXTURE_BORDER                       0x1005
#define GL_TEXTURE_COMPONENTS                   0x1003
#define GL_TEXTURE_RED_SIZE                     0x805C
#define GL_TEXTURE_GREEN_SIZE                   0x805D
#define GL_TEXTURE_BLUE_SIZE                    0x805E
#define GL_TEXTURE_ALPHA_SIZE                   0x805F
#define GL_TEXTURE_LUMINANCE_SIZE               0x8060
#define GL_TEXTURE_INTENSITY_SIZE               0x8061
#define GL_NEAREST_MIPMAP_NEAREST               0x2700
#define GL_NEAREST_MIPMAP_LINEAR                0x2702
#define GL_LINEAR_MIPMAP_NEAREST                0x2701
#define GL_LINEAR_MIPMAP_LINEAR                 0x2703
#define GL_OBJECT_LINEAR                        0x2401
#define GL_OBJECT_PLANE                         0x2501
#define GL_EYE_LINEAR                           0x2400
#define GL_EYE_PLANE                            0x2502
#define GL_SPHERE_MAP                           0x2402
#define GL_DECAL                                0x2101
#define GL_MODULATE                             0x2100
#define GL_NEAREST                              0x2600
#define GL_REPEAT                               0x2901
#define GL_CLAMP                                0x2900
#define GL_S                                    0x2000
#define GL_T                                    0x2001
#define GL_R                                    0x2002
#define GL_Q                                    0x2003
#define GL_TEXTURE_GEN_R                        0x0C62
#define GL_TEXTURE_GEN_Q                        0x0C63

/* Vertex Arrays */
#define GL_VERTEX_ARRAY                         0x8074
#define GL_NORMAL_ARRAY                         0x8075
#define GL_COLOR_ARRAY                          0x8076
#define GL_INDEX_ARRAY                          0x8077
#define GL_TEXTURE_COORD_ARRAY                  0x8078
#define GL_EDGE_FLAG_ARRAY                      0x8079
#define GL_VERTEX_ARRAY_SIZE                    0x807A
#define GL_VERTEX_ARRAY_TYPE                    0x807B
#define GL_VERTEX_ARRAY_STRIDE                  0x807C
#define GL_NORMAL_ARRAY_TYPE                    0x807E
#define GL_NORMAL_ARRAY_STRIDE                  0x807F
#define GL_COLOR_ARRAY_SIZE                     0x8081
#define GL_COLOR_ARRAY_TYPE                     0x8082
#define GL_COLOR_ARRAY_STRIDE                   0x8083
#define GL_INDEX_ARRAY_TYPE                     0x8085
#define GL_INDEX_ARRAY_STRIDE                   0x8086
#define GL_TEXTURE_COORD_ARRAY_SIZE             0x8088
#define GL_TEXTURE_COORD_ARRAY_TYPE             0x8089
#define GL_TEXTURE_COORD_ARRAY_STRIDE           0x808A
#define GL_EDGE_FLAG_ARRAY_STRIDE               0x808C
#define GL_VERTEX_ARRAY_POINTER                 0x808E
#define GL_NORMAL_ARRAY_POINTER                 0x808F
#define GL_COLOR_ARRAY_POINTER                  0x8090
#define GL_INDEX_ARRAY_POINTER                  0x8091
#define GL_TEXTURE_COORD_ARRAY_POINTER          0x8092
#define GL_EDGE_FLAG_ARRAY_POINTER              0x8093
#define GL_V2F                                  0x2A20
#define GL_V3F                                  0x2A21
#define GL_C4UB_V2F                             0x2A22
#define GL_C4UB_V3F                             0x2A23
#define GL_C3F_V3F                              0x2A24
#define GL_N3F_V3F                              0x2A25
#define GL_C4F_N3F_V3F                          0x2A26
#define GL_T2F_V3F                              0x2A27
#define GL_T4F_V4F                              0x2A28
#define GL_T2F_C4UB_V3F                         0x2A29
#define GL_T2F_C3F_V3F                          0x2A2A
#define GL_T2F_N3F_V3F                          0x2A2B
#define GL_T2F_C4F_N3F_V3F                      0x2A2C
#define GL_T4F_C4F_N3F_V4F                      0x2A2D

/* OpenGL 1.1 */
#define GL_PROXY_TEXTURE_1D                     0x8063
#define GL_PROXY_TEXTURE_2D                     0x8064
#define GL_TEXTURE_PRIORITY                     0x8066
#define GL_TEXTURE_RESIDENT                     0x8067
#define GL_TEXTURE_BINDING_1D                   0x8068
#define GL_TEXTURE_BINDING_2D                   0x8069
#define GL_TEXTURE_INTERNAL_FORMAT              0x1003
#define GL_ALPHA4                               0x803B
#define GL_ALPHA8                               0x803C
#define GL_ALPHA12                              0x803D
#define GL_ALPHA16                              0x803E
#define GL_LUMINANCE4                           0x803F
#define GL_LUMINANCE8                           0x8040
#define GL_LUMINANCE12                          0x8041
#define GL_LUMINANCE16                          0x8042
#define GL_LUMINANCE4_ALPHA4                    0x8043
#define GL_LUMINANCE6_ALPHA2                    0x8044
#define GL_LUMINANCE8_ALPHA8                    0x8045
#define GL_LUMINANCE12_ALPHA4                   0x8046
#define GL_LUMINANCE12_ALPHA12                  0x8047
#define GL_LUMINANCE16_ALPHA16                  0x8048
#define GL_INTENSITY                            0x8049
#define GL_INTENSITY4                           0x804A
#define GL_INTENSITY8                           0x804B
#define GL_INTENSITY12                          0x804C
#define GL_INTENSITY16                          0x804D
#define GL_R3_G3_B2                             0x2A10
#define GL_RGB4                                 0x804F
#define GL_RGB5                                 0x8050
#define GL_RGB8                                 0x8051
#define GL_RGB10                                0x8052
#define GL_RGB12                                0x8053
#define GL_RGB16                                0x8054
#define GL_RGBA2                                0x8055
#define GL_RGBA4                                0x8056
#define GL_RGB5_A1                              0x8057
#define GL_RGBA8                                0x8058
#define GL_RGB10_A2                             0x8059
#define GL_RGBA12                               0x805A
#define GL_RGBA16                               0x805B
#define GL_CLIENT_PIXEL_STORE_BIT               0x00000001
#define GL_CLIENT_VERTEX_ARRAY_BIT              0x00000002
#define GL_ALL_CLIENT_ATTRIB_BITS               0xFFFFFFFF
#define GL_CLIENT_ALL_ATTRIB_BITS               0xFFFFFFFF

/* OpenGL 1.2 constants */
#define GL_PACK_SKIP_IMAGES               0x806B
#define GL_PACK_IMAGE_HEIGHT              0x806C
#define GL_UNPACK_SKIP_IMAGES             0x806D
#define GL_UNPACK_IMAGE_HEIGHT            0x806E
#define GL_TEXTURE_3D                     0x806F
#define GL_PROXY_TEXTURE_3D               0x8070
#define GL_TEXTURE_DEPTH                  0x8071
#define GL_TEXTURE_WRAP_R                 0x8072
#define GL_MAX_3D_TEXTURE_SIZE            0x8073
#define GL_BGR                            0x80E0
#define GL_BGRA                           0x80E1
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#define GL_UNSIGNED_BYTE_2_3_3_REV        0x8362
#define GL_UNSIGNED_SHORT_5_6_5           0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV       0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4         0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV     0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034
#define GL_UNSIGNED_SHORT_1_5_5_5_REV     0x8366
#define GL_UNSIGNED_INT_8_8_8_8           0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_UNSIGNED_INT_10_10_10_2        0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV    0x8368
#define GL_RESCALE_NORMAL                 0x803A
#define GL_TEXTURE_BINDING_3D             0x806A
#define GL_LIGHT_MODEL_COLOR_CONTROL      0x81F8
#define GL_SINGLE_COLOR                   0x81F9
#define GL_SEPARATE_SPECULAR_COLOR        0x81FA
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_TEXTURE_MIN_LOD                0x813A
#define GL_TEXTURE_MAX_LOD                0x813B
#define GL_TEXTURE_BASE_LEVEL             0x813C
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_MAX_ELEMENTS_VERTICES          0x80E8
#define GL_MAX_ELEMENTS_INDICES           0x80E9
#define GL_ALIASED_POINT_SIZE_RANGE       0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE       0x846E

/* OpenGL 1.3 constants */
#define GL_ACTIVE_TEXTURE                 0x84E0
#define GL_CLIENT_ACTIVE_TEXTURE          0x84E1
#define GL_MAX_TEXTURE_UNITS              0x84E2
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE8                       0x84C8
#define GL_TEXTURE9                       0x84C9
#define GL_TEXTURE10                      0x84CA
#define GL_TEXTURE11                      0x84CB
#define GL_TEXTURE12                      0x84CC
#define GL_TEXTURE13                      0x84CD
#define GL_TEXTURE14                      0x84CE
#define GL_TEXTURE15                      0x84CF
#define GL_TEXTURE16                      0x84D0
#define GL_TEXTURE17                      0x84D1
#define GL_TEXTURE18                      0x84D2
#define GL_TEXTURE19                      0x84D3
#define GL_TEXTURE20                      0x84D4
#define GL_TEXTURE21                      0x84D5
#define GL_TEXTURE22                      0x84D6
#define GL_TEXTURE23                      0x84D7
#define GL_TEXTURE24                      0x84D8
#define GL_TEXTURE25                      0x84D9
#define GL_TEXTURE26                      0x84DA
#define GL_TEXTURE27                      0x84DB
#define GL_TEXTURE28                      0x84DC
#define GL_TEXTURE29                      0x84DD
#define GL_TEXTURE30                      0x84DE
#define GL_TEXTURE31                      0x84DF
#define GL_NORMAL_MAP                     0x8511
#define GL_REFLECTION_MAP                 0x8512
#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP       0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X    0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X    0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y    0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y    0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z    0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z    0x851A
#define GL_PROXY_TEXTURE_CUBE_MAP         0x851B
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE      0x851C
#define GL_COMBINE                        0x8570
#define GL_COMBINE_RGB                    0x8571
#define GL_COMBINE_ALPHA                  0x8572
#define GL_RGB_SCALE                      0x8573
#define GL_ADD_SIGNED                     0x8574
#define GL_INTERPOLATE                    0x8575
#define GL_CONSTANT                       0x8576
#define GL_PRIMARY_COLOR                  0x8577
#define GL_PREVIOUS                       0x8578
#define GL_SOURCE0_RGB                    0x8580
#define GL_SOURCE1_RGB                    0x8581
#define GL_SOURCE2_RGB                    0x8582
#define GL_SOURCE0_ALPHA                  0x8588
#define GL_SOURCE1_ALPHA                  0x8589
#define GL_SOURCE2_ALPHA                  0x858A
#define GL_OPERAND0_RGB                   0x8590
#define GL_OPERAND1_RGB                   0x8591
#define GL_OPERAND2_RGB                   0x8592
#define GL_OPERAND0_ALPHA                 0x8598
#define GL_OPERAND1_ALPHA                 0x8599
#define GL_OPERAND2_ALPHA                 0x859A
#define GL_SUBTRACT                       0x84E7
#define GL_TRANSPOSE_MODELVIEW_MATRIX     0x84E3
#define GL_TRANSPOSE_PROJECTION_MATRIX    0x84E4
#define GL_TRANSPOSE_TEXTURE_MATRIX       0x84E5
#define GL_TRANSPOSE_COLOR_MATRIX         0x84E6
#define GL_COMPRESSED_ALPHA               0x84E9
#define GL_COMPRESSED_LUMINANCE           0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA     0x84EB
#define GL_COMPRESSED_INTENSITY           0x84EC
#define GL_COMPRESSED_RGB                 0x84ED
#define GL_COMPRESSED_RGBA                0x84EE
#define GL_TEXTURE_COMPRESSION_HINT       0x84EF
#define GL_TEXTURE_COMPRESSED_IMAGE_SIZE  0x86A0
#define GL_TEXTURE_COMPRESSED             0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS     0x86A3
#define GL_DOT3_RGB                       0x86AE
#define GL_DOT3_RGBA                      0x86AF
#define GL_CLAMP_TO_BORDER                0x812D
#define GL_MULTISAMPLE                    0x809D
#define GL_SAMPLE_ALPHA_TO_COVERAGE       0x809E
#define GL_SAMPLE_ALPHA_TO_ONE            0x809F
#define GL_SAMPLE_COVERAGE                0x80A0
#define GL_SAMPLE_BUFFERS                 0x80A8
#define GL_SAMPLES                        0x80A9
#define GL_SAMPLE_COVERAGE_VALUE          0x80AA
#define GL_SAMPLE_COVERAGE_INVERT         0x80AB
#define GL_MULTISAMPLE_BIT                0x20000000

/* GL_VERSION_2_0 */
#ifndef GL_VERSION_2_0
#define GL_VERSION_2_0 1
#define GL_BLEND_EQUATION_RGB                               GL_BLEND_EQUATION
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED                      0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE                         0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE                       0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE                         0x8625
#define GL_CURRENT_VERTEX_ATTRIB                            0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE                        0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE                          0x8643
#define GL_VERTEX_ATTRIB_ARRAY_POINTER                      0x8645
#define GL_STENCIL_BACK_FUNC                                0x8800
#define GL_STENCIL_BACK_FAIL                                0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL                     0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS                     0x8803
#define GL_MAX_DRAW_BUFFERS                                 0x8824
#define GL_DRAW_BUFFER0                                     0x8825
#define GL_DRAW_BUFFER1                                     0x8826
#define GL_DRAW_BUFFER2                                     0x8827
#define GL_DRAW_BUFFER3                                     0x8828
#define GL_DRAW_BUFFER4                                     0x8829
#define GL_DRAW_BUFFER5                                     0x882a
#define GL_DRAW_BUFFER6                                     0x882b
#define GL_DRAW_BUFFER7                                     0x882c
#define GL_DRAW_BUFFER8                                     0x882d
#define GL_DRAW_BUFFER9                                     0x882e
#define GL_DRAW_BUFFER10                                    0x882f
#define GL_DRAW_BUFFER11                                    0x8830
#define GL_DRAW_BUFFER12                                    0x8831
#define GL_DRAW_BUFFER13                                    0x8832
#define GL_DRAW_BUFFER14                                    0x8833
#define GL_DRAW_BUFFER15                                    0x8834
#define GL_BLEND_EQUATION_ALPHA                             0x883d
#define GL_POINT_SPRITE                                     0x8861
#define GL_COORD_REPLACE                                    0x8862
#define GL_MAX_VERTEX_ATTRIBS                               0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED                   0x886a
#define GL_MAX_TEXTURE_COORDS                               0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS                          0x8872
#define GL_FRAGMENT_SHADER                                  0x8b30
#define GL_VERTEX_SHADER                                    0x8b31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS                  0x8b49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS                    0x8b4a
#define GL_MAX_VARYING_FLOATS                               0x8b4b
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS                   0x8b4c
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS                 0x8b4d
#define GL_SHADER_TYPE                                      0x8b4f
#define GL_FLOAT_VEC2                                       0x8b50
#define GL_FLOAT_VEC3                                       0x8b51
#define GL_FLOAT_VEC4                                       0x8b52
#define GL_INT_VEC2                                         0x8b53
#define GL_INT_VEC3                                         0x8b54
#define GL_INT_VEC4                                         0x8b55
#define GL_BOOL                                             0x8b56
#define GL_BOOL_VEC2                                        0x8b57
#define GL_BOOL_VEC3                                        0x8b58
#define GL_BOOL_VEC4                                        0x8b59
#define GL_FLOAT_MAT2                                       0x8b5a
#define GL_FLOAT_MAT3                                       0x8b5b
#define GL_FLOAT_MAT4                                       0x8b5c
#define GL_SAMPLER_1D                                       0x8b5d
#define GL_SAMPLER_2D                                       0x8b5e
#define GL_SAMPLER_3D                                       0x8b5f
#define GL_SAMPLER_CUBE                                     0x8b60
#define GL_SAMPLER_1D_SHADOW                                0x8b61
#define GL_SAMPLER_2D_SHADOW                                0x8b62
#define GL_DELETE_STATUS                                    0x8b80
#define GL_COMPILE_STATUS                                   0x8b81
#define GL_LINK_STATUS                                      0x8b82
#define GL_VALIDATE_STATUS                                  0x8b83
#define GL_INFO_LOG_LENGTH                                  0x8b84
#define GL_ATTACHED_SHADERS                                 0x8b85
#define GL_ACTIVE_UNIFORMS                                  0x8b86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH                        0x8b87
#define GL_SHADER_SOURCE_LENGTH                             0x8b88
#define GL_ACTIVE_ATTRIBUTES                                0x8b89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH                      0x8b8a
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT                  0x8b8b
#define GL_SHADING_LANGUAGE_VERSION                         0x8b8c
#define GL_CURRENT_PROGRAM                                  0x8b8d
#define GL_POINT_SPRITE_COORD_ORIGIN                        0x8ca0
#define GL_LOWER_LEFT                                       0x8ca1
#define GL_UPPER_LEFT                                       0x8ca2
#define GL_STENCIL_BACK_REF                                 0x8ca3
#define GL_STENCIL_BACK_VALUE_MASK                          0x8ca4
#define GL_STENCIL_BACK_WRITEMASK                           0x8ca5
typedef char GLchar;
#endif
typedef void (WINE_GLAPI *PGLFNBLENDEQUATIONSEPARATEPROC)(GLenum modeRGB, GLenum modeAlpha);
typedef void (WINE_GLAPI *PGLFNDRAWBUFFERSPROC)(GLsizei n, const GLenum *bufs);
typedef void (WINE_GLAPI *PGLFNSTENCILOPSEPARATEPROC)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void (WINE_GLAPI *PGLFNSTENCILFUNCSEPARATEPROC)(GLenum frontfunc, GLenum backfunc, GLint ref, GLuint mask);
typedef void (WINE_GLAPI *PGLFNSTENCILMASKSEPARATEPROC)(GLenum face, GLuint mask);
typedef void (WINE_GLAPI *PGLFNATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (WINE_GLAPI *PGLFNBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar *name);
typedef void (WINE_GLAPI *PGLFNCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (WINE_GLAPI *PGLFNCREATEPROGRAMPROC)(void);
typedef GLuint (WINE_GLAPI *PGLFNCREATESHADERPROC)(GLenum type);
typedef void (WINE_GLAPI *PGLFNDELETEPROGRAMPROC)(GLuint program);
typedef void (WINE_GLAPI *PGLFNDELETESHADERPROC)(GLuint shader);
typedef void (WINE_GLAPI *PGLFNDETACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (WINE_GLAPI *PGLFNDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (WINE_GLAPI *PGLFNENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (WINE_GLAPI *PGLFNGETACTIVEATTRIBPROC)(GLuint program,
        GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (WINE_GLAPI *PGLFNGETACTIVEUNIFORMPROC)(GLuint program,
        GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
typedef void (WINE_GLAPI *PGLFNGETATTACHEDSHADERSPROC)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *obj);
typedef GLint (WINE_GLAPI *PGLFNGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMINFOLOGPROC)(GLuint program,
        GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (WINE_GLAPI *PGLFNGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (WINE_GLAPI *PGLFNGETSHADERSOURCEPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
typedef GLint (WINE_GLAPI *PGLFNGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (WINE_GLAPI *PGLFNGETUNIFORMFVPROC)(GLuint program, GLint location, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETUNIFORMIVPROC)(GLuint program, GLint location, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBDVPROC)(GLuint index, GLenum pname, GLdouble *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBFVPROC)(GLuint index, GLenum pname, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBIVPROC)(GLuint index, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBPOINTERVPROC)(GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (WINE_GLAPI *PGLFNISPROGRAMPROC)(GLuint program);
typedef GLboolean (WINE_GLAPI *PGLFNISSHADERPROC)(GLuint shader);
typedef void (WINE_GLAPI *PGLFNLINKPROGRAMPROC)(GLuint program);
typedef void (WINE_GLAPI *PGLFNSHADERSOURCEPROC)(GLuint shader,
        GLsizei count, const GLchar* *string, const GLint *length);
typedef void (WINE_GLAPI *PGLFNUSEPROGRAMPROC)(GLuint program);
typedef void (WINE_GLAPI *PGLFNUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (WINE_GLAPI *PGLFNUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (WINE_GLAPI *PGLFNUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (WINE_GLAPI *PGLFNUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (WINE_GLAPI *PGLFNUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (WINE_GLAPI *PGLFNUNIFORM2IPROC)(GLint location, GLint v0, GLint v1);
typedef void (WINE_GLAPI *PGLFNUNIFORM3IPROC)(GLint location, GLint v0, GLint v1, GLint v2);
typedef void (WINE_GLAPI *PGLFNUNIFORM4IPROC)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (WINE_GLAPI *PGLFNUNIFORM1FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM2FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM1IVPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM2IVPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM3IVPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *PGLFNUNIFORM4IVPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *PGLFNUNIFORMMATRIX2FVPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORMMATRIX3FVPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNUNIFORMMATRIX4FVPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *PGLFNVALIDATEPROGRAMPROC)(GLuint program);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1DPROC)(GLuint index, GLdouble x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1DVPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1FPROC)(GLuint index, GLfloat x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1FVPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1SPROC)(GLuint index, GLshort x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1SVPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2DPROC)(GLuint index, GLdouble x, GLdouble y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2DVPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2FPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2FVPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2SPROC)(GLuint index, GLshort x, GLshort y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2SVPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3DPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3DVPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3FVPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3SPROC)(GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3SVPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NBVPROC)(GLuint index, const GLbyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NIVPROC)(GLuint index, const GLint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NSVPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUBPROC)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUBVPROC)(GLuint index, const GLubyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUIVPROC)(GLuint index, const GLuint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUSVPROC)(GLuint index, const GLushort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4BVPROC)(GLuint index, const GLbyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4DPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4DVPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4FPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4FVPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4IVPROC)(GLuint index, const GLint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4SPROC)(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4SVPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4UBVPROC)(GLuint index, const GLubyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4UIVPROC)(GLuint index, const GLuint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4USVPROC)(GLuint index, const GLushort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBPOINTERPROC)(GLuint index,
        GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

#ifndef WINED3D_EXTERN
# define WINED3D_EXTERN extern
#endif

WINED3D_EXTERN void (WINE_GLAPI *glAccum)(GLenum op, GLfloat value) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glAlphaFunc)(GLenum func, GLclampf ref) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLboolean (WINE_GLAPI *glAreTexturesResident)(GLsizei n, const GLuint *textures, GLboolean *residences) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glArrayElement)(GLint i) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glBegin)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glBindTexture)(GLenum target, GLuint texture) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glBitmap)(GLsizei width, GLsizei height, GLfloat xorig,
        GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glBlendFunc)(GLenum sfactor, GLenum dfactor) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCallList)(GLuint list) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCallLists)(GLsizei n, GLenum type, const GLvoid *lists) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClear)(GLbitfield mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClearAccum)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClearDepth)(GLclampd depth) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClearIndex)(GLfloat c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClearStencil)(GLint s) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glClipPlane)(GLenum plane, const GLdouble *equation) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3b)(GLbyte red, GLbyte green, GLbyte blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3bv)(const GLbyte *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3d)(GLdouble red, GLdouble green, GLdouble blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3f)(GLfloat red, GLfloat green, GLfloat blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3i)(GLint red, GLint green, GLint blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3s)(GLshort red, GLshort green, GLshort blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3ub)(GLubyte red, GLubyte green, GLubyte blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3ubv)(const GLubyte *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3ui)(GLuint red, GLuint green, GLuint blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3uiv)(const GLuint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3us)(GLushort red, GLushort green, GLushort blue) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor3usv)(const GLushort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4b)(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4bv)(const GLbyte *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4d)(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4i)(GLint red, GLint green, GLint blue, GLint alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4s)(GLshort red, GLshort green, GLshort blue, GLshort alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4ub)(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4ubv)(const GLubyte *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4ui)(GLuint red, GLuint green, GLuint blue, GLuint alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4uiv)(const GLuint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4us)(GLushort red, GLushort green, GLushort blue, GLushort alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColor4usv)(const GLushort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColorMaterial)(GLenum face, GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glColorPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCopyPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCopyTexImage1D)(GLenum target, GLint level,
                       GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCopyTexImage2D)(GLenum target, GLint level,
                       GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCopyTexSubImage1D)(GLenum target, GLint level,
                       GLint xoffset, GLint x, GLint y, GLsizei width) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCopyTexSubImage2D)(GLenum target, GLint level,
                       GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glCullFace)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDeleteLists)(GLuint list, GLsizei range) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDeleteTextures)(GLsizei n, const GLuint *textures) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDepthFunc)(GLenum func) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDepthMask)(GLboolean flag) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDepthRange)(GLclampd nearParam, GLclampd farParam) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDisable)(GLenum cap) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDisableWINE)(GLenum cap) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDisableClientState)(GLenum array) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDrawArrays)(GLenum mode, GLint first, GLsizei count) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDrawBuffer)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glDrawPixels)(GLsizei width, GLsizei height, GLenum format,
                       GLenum type, const GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEdgeFlag)(GLboolean flag) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEdgeFlagPointer)(GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEdgeFlagv)(const GLboolean *flag) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEnable)(GLenum cap) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEnableWINE)(GLenum cap) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEnableClientState)(GLenum array) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEnd)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEndList)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord1d)(GLdouble u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord1dv)(const GLdouble *u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord1f)(GLfloat u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord1fv)(const GLfloat *u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord2d)(GLdouble u, GLdouble v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord2dv)(const GLdouble *u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord2f)(GLfloat u, GLfloat v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalCoord2fv)(const GLfloat *u) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalMesh1)(GLenum mode, GLint i1, GLint i2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalMesh2)(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalPoint1)(GLint i) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glEvalPoint2)(GLint i, GLint j) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFeedbackBuffer)(GLsizei size, GLenum type, GLfloat *buffer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFogf)(GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFogfv)(GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFogi)(GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFogiv)(GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFrontFace)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glFrustum)(GLdouble left, GLdouble right, GLdouble bottom,
                       GLdouble top, GLdouble zNear, GLdouble zFar) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLuint (WINE_GLAPI *glGenLists)(GLsizei range) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGenTextures)(GLsizei n, GLuint *textures) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetBooleanv)(GLenum pname, GLboolean *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetClipPlane)(GLenum plane, GLdouble *equation) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetDoublev)(GLenum pname, GLdouble *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLenum (WINE_GLAPI *glGetError)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetFloatv)(GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetIntegerv)(GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetLightfv)(GLenum light, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetLightiv)(GLenum light, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetMapdv)(GLenum target, GLenum query, GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetMapfv)(GLenum target, GLenum query, GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetMapiv)(GLenum target, GLenum query, GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetMaterialfv)(GLenum face, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetMaterialiv)(GLenum face, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetPixelMapfv)(GLenum map, GLfloat *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetPixelMapuiv)(GLenum map, GLuint *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetPixelMapusv)(GLenum map, GLushort *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetPointerv)(GLenum pname, GLvoid **params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetPolygonStipple)(GLubyte *mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN const GLubyte * (WINE_GLAPI *glGetString)(GLenum name) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexEnvfv)(GLenum target, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexEnviv)(GLenum target, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexGendv)(GLenum coord, GLenum pname, GLdouble *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexGenfv)(GLenum coord, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexGeniv)(GLenum coord, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexImage)(GLenum target, GLint level, GLenum format,
                       GLenum type, GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexLevelParameterfv)(GLenum target, GLint level, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glGetTexParameteriv)(GLenum target, GLenum pname, GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glHint)(GLenum target, GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexMask)(GLuint mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexPointer)(GLenum type, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexd)(GLdouble c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexdv)(const GLdouble *c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexf)(GLfloat c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexfv)(const GLfloat *c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexi)(GLint c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexiv)(const GLint *c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexs)(GLshort c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexsv)(const GLshort *c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexub)(GLubyte c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glIndexubv)(const GLubyte *c) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glInitNames)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glInterleavedArrays)(GLenum format, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLboolean (WINE_GLAPI *glIsEnabled)(GLenum cap) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLboolean (WINE_GLAPI *glIsList)(GLuint list) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLboolean (WINE_GLAPI *glIsTexture)(GLuint texture) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightModelf)(GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightModelfv)(GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightModeli)(GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightModeliv)(GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightf)(GLenum light, GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightfv)(GLenum light, GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLighti)(GLenum light, GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLightiv)(GLenum light, GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLineStipple)(GLint factor, GLushort pattern) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLineWidth)(GLfloat width) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glListBase)(GLuint base) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLoadIdentity)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLoadMatrixd)(const GLdouble *m) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLoadMatrixf)(const GLfloat *m) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLoadName)(GLuint name) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glLogicOp)(GLenum opcode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMap1d)(GLenum target, GLdouble u1, GLdouble u2,
                       GLint stride, GLint order, const GLdouble *points) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMap1f)(GLenum target, GLfloat u1, GLfloat u2, GLint stride,
                       GLint order, const GLfloat *points) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMap2d)(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
                       GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMap2f)(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
                       GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMapGrid1d)(GLint un, GLdouble u1, GLdouble u2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMapGrid1f)(GLint un, GLfloat u1, GLfloat u2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMapGrid2d)(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMapGrid2f)(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMaterialf)(GLenum face, GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMaterialfv)(GLenum face, GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMateriali)(GLenum face, GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMaterialiv)(GLenum face, GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMatrixMode)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMultMatrixd)(const GLdouble *m) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glMultMatrixf)(const GLfloat *m) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNewList)(GLuint list, GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3b)(GLbyte nx, GLbyte ny, GLbyte nz) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3bv)(const GLbyte *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3d)(GLdouble nx, GLdouble ny, GLdouble nz) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3f)(GLfloat nx, GLfloat ny, GLfloat nz) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3i)(GLint nx, GLint ny, GLint nz) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3s)(GLshort nx, GLshort ny, GLshort nz) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormal3sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glNormalPointer)(GLenum type, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glOrtho)(GLdouble left, GLdouble right, GLdouble bottom,
                       GLdouble top, GLdouble zNear, GLdouble zFar) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPassThrough)(GLfloat token) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelMapfv)(GLenum map, GLint mapsize, const GLfloat *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelMapuiv)(GLenum map, GLint mapsize, const GLuint *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelMapusv)(GLenum map, GLint mapsize, const GLushort *values) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelStoref)(GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelStorei)(GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelTransferf)(GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelTransferi)(GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPixelZoom)(GLfloat xfactor, GLfloat yfactor) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPointSize)(GLfloat size) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPolygonMode)(GLenum face, GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPolygonOffset)(GLfloat factor, GLfloat units) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPolygonStipple)(const GLubyte *mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPopAttrib)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPopClientAttrib)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPopMatrix)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPopName)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPrioritizeTextures)(GLsizei n, const GLuint *textures, const GLclampf *priorities) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPushAttrib)(GLbitfield mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPushClientAttrib)(GLbitfield mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPushMatrix)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPushName)(GLuint name) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2d)(GLdouble x, GLdouble y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2f)(GLfloat x, GLfloat y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2i)(GLint x, GLint y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2s)(GLshort x, GLshort y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos2sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3d)(GLdouble x, GLdouble y, GLdouble z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3f)(GLfloat x, GLfloat y, GLfloat z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3i)(GLint x, GLint y, GLint z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3s)(GLshort x, GLshort y, GLshort z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos3sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4d)(GLdouble x, GLdouble y, GLdouble z, GLdouble w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4i)(GLint x, GLint y, GLint z, GLint w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4s)(GLshort x, GLshort y, GLshort z, GLshort w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRasterPos4sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glReadBuffer)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height,
                       GLenum format, GLenum type, GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectd)(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectdv)(const GLdouble *v1, const GLdouble *v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectf)(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectfv)(const GLfloat *v1, const GLfloat *v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRecti)(GLint x1, GLint y1, GLint x2, GLint y2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectiv)(const GLint *v1, const GLint *v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRects)(GLshort x1, GLshort y1, GLshort x2, GLshort y2) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRectsv)(const GLshort *v1, const GLshort *v2) DECLSPEC_HIDDEN;
WINED3D_EXTERN GLint (WINE_GLAPI *glRenderMode)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRotated)(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glRotatef)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glScaled)(GLdouble x, GLdouble y, GLdouble z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glScalef)(GLfloat x, GLfloat y, GLfloat z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glScissor)(GLint x, GLint y, GLsizei width, GLsizei height) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glSelectBuffer)(GLsizei size, GLuint *buffer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glShadeModel)(GLenum mode) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glStencilFunc)(GLenum func, GLint ref, GLuint mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glStencilMask)(GLuint mask) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1d)(GLdouble s) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1f)(GLfloat s) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1i)(GLint s) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1s)(GLshort s) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord1sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2d)(GLdouble s, GLdouble t) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2f)(GLfloat s, GLfloat t) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2i)(GLint s, GLint t) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2s)(GLshort s, GLshort t) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord2sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3d)(GLdouble s, GLdouble t, GLdouble r) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3f)(GLfloat s, GLfloat t, GLfloat r) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3i)(GLint s, GLint t, GLint r) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3s)(GLshort s, GLshort t, GLshort r) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord3sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4d)(GLdouble s, GLdouble t, GLdouble r, GLdouble q) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4f)(GLfloat s, GLfloat t, GLfloat r, GLfloat q) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4i)(GLint s, GLint t, GLint r, GLint q) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4s)(GLshort s, GLshort t, GLshort r, GLshort q) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoord4sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexCoordPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexEnvf)(GLenum target, GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexEnvfv)(GLenum target, GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexEnvi)(GLenum target, GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexEnviv)(GLenum target, GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGend)(GLenum coord, GLenum pname, GLdouble param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGendv)(GLenum coord, GLenum pname, const GLdouble *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGenf)(GLenum coord, GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGenfv)(GLenum coord, GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGeni)(GLenum coord, GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexGeniv)(GLenum coord, GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexImage1D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                       GLint border, GLenum format, GLenum type, const GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                       GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexParameterf)(GLenum target, GLenum pname, GLfloat param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexParameteri)(GLenum target, GLenum pname, GLint param) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexParameteriv)(GLenum target, GLenum pname, const GLint *params) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexSubImage1D)(GLenum target, GLint level, GLint xoffset,
                       GLsizei width, GLenum format, GLenum type, const GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                       GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTranslated)(GLdouble x, GLdouble y, GLdouble z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glTranslatef)(GLfloat x, GLfloat y, GLfloat z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2d)(GLdouble x, GLdouble y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2f)(GLfloat x, GLfloat y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2i)(GLint x, GLint y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2s)(GLshort x, GLshort y) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex2sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3d)(GLdouble x, GLdouble y, GLdouble z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3f)(GLfloat x, GLfloat y, GLfloat z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3i)(GLint x, GLint y, GLint z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3s)(GLshort x, GLshort y, GLshort z) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex3sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4d)(GLdouble x, GLdouble y, GLdouble z, GLdouble w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4dv)(const GLdouble *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4f)(GLfloat x, GLfloat y, GLfloat z, GLfloat w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4fv)(const GLfloat *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4i)(GLint x, GLint y, GLint z, GLint w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4iv)(const GLint *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4s)(GLshort x, GLshort y, GLshort z, GLshort w) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertex4sv)(const GLshort *v) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glVertexPointer)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINE_GLAPI *glPointParameterfv)(GLenum pname, const GLfloat *params) DECLSPEC_HIDDEN;

/* glFinish and glFlush are always loaded from opengl32.dll, thus they always have
 * __stdcall calling convention.
 *
 * They are wgl functions and must not be called inside the gl lock, give them a
 * name that makes this clear
 */
WINED3D_EXTERN void (__stdcall *wglFinish)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (__stdcall *wglFlush)(void) DECLSPEC_HIDDEN;

/* WGL functions */
WINED3D_EXTERN BOOL (WINAPI *pwglDeleteContext)(HGLRC) DECLSPEC_HIDDEN;
WINED3D_EXTERN HGLRC (WINAPI *pwglGetCurrentContext)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN HDC (WINAPI *pwglGetCurrentDC)(void) DECLSPEC_HIDDEN;
WINED3D_EXTERN PROC (WINAPI *pwglGetProcAddress)(LPCSTR) DECLSPEC_HIDDEN;
WINED3D_EXTERN BOOL (WINAPI *pwglMakeCurrent)(HDC, HGLRC) DECLSPEC_HIDDEN;
WINED3D_EXTERN BOOL (WINAPI *pwglSwapLayerBuffers)(HDC, UINT) DECLSPEC_HIDDEN;
WINED3D_EXTERN BOOL (WINAPI *pwglShareLists)(HGLRC, HGLRC) DECLSPEC_HIDDEN;
WINED3D_EXTERN BOOL (WINAPI *pwglChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR *) DECLSPEC_HIDDEN;
WINED3D_EXTERN int (WINAPI *pwglDescribePixelFormat)(HDC, int, UINT, LPPIXELFORMATDESCRIPTOR) DECLSPEC_HIDDEN;
WINED3D_EXTERN int (WINAPI *pwglGetPixelFormat)(HDC) DECLSPEC_HIDDEN;
WINED3D_EXTERN BOOL (WINAPI *pwglSetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR *) DECLSPEC_HIDDEN;

struct VBOXUHGSMI;
WINED3D_EXTERN HGLRC (WINAPI *pVBoxCreateContext)(HDC, struct VBOXUHGSMI*) DECLSPEC_HIDDEN;
WINED3D_EXTERN void (WINAPI *pVBoxFlushToHost)(HGLRC) DECLSPEC_HIDDEN;

#if defined(VBOX_WITH_WDDM) || defined(VBOX_WINE_WITH_SINGLE_SWAPCHAIN_CONTEXT)
# define VBOX_WINE_WITH_DIRECT_VBOXOGL
#endif

#ifdef VBOX_WINE_WITH_DIRECT_VBOXOGL
/* make sure we used stuff from VBoxOGL
 * we do it this way to avoid extra modifications to Wine code */

/* We're directly using wglMakeCurrent calls skipping GDI layer, which causes GDI SwapBuffers to fail trying to
 * call glFinish, which doesn't have any context set. So we use wglSwapLayerBuffers directly as well.
 */
# define SwapBuffers(_hdc) pwglSwapLayerBuffers((_hdc), WGL_SWAP_MAIN_PLANE)

/* we avoid using GDI32!*PixelFormat API and use VBoxOGL stuff directly
 * because SetPixelFormat may eventually lead to opengl32 setting its own window proc via SetWindowLongPtr
 * thus wined3d_[un]register_window stuff will become screwed up leading to infinite recursion or other misbehave */
# define ChoosePixelFormat pwglChoosePixelFormat
# define DescribePixelFormat pwglDescribePixelFormat
# define GetPixelFormat pwglGetPixelFormat
# define SetPixelFormat pwglSetPixelFormat
#endif

#define GL_FUNCS_GEN \
    USE_GL_FUNC(glAccum) \
    USE_GL_FUNC(glAlphaFunc) \
    USE_GL_FUNC(glAreTexturesResident) \
    USE_GL_FUNC(glArrayElement) \
    USE_GL_FUNC(glBegin) \
    USE_GL_FUNC(glBindTexture) \
    USE_GL_FUNC(glBitmap) \
    USE_GL_FUNC(glBlendFunc) \
    USE_GL_FUNC(glCallList) \
    USE_GL_FUNC(glCallLists) \
    USE_GL_FUNC(glClear) \
    USE_GL_FUNC(glClearAccum) \
    USE_GL_FUNC(glClearColor) \
    USE_GL_FUNC(glClearDepth) \
    USE_GL_FUNC(glClearIndex) \
    USE_GL_FUNC(glClearStencil) \
    USE_GL_FUNC(glClipPlane) \
    USE_GL_FUNC(glColor3b) \
    USE_GL_FUNC(glColor3bv) \
    USE_GL_FUNC(glColor3d) \
    USE_GL_FUNC(glColor3dv) \
    USE_GL_FUNC(glColor3f) \
    USE_GL_FUNC(glColor3fv) \
    USE_GL_FUNC(glColor3i) \
    USE_GL_FUNC(glColor3iv) \
    USE_GL_FUNC(glColor3s) \
    USE_GL_FUNC(glColor3sv) \
    USE_GL_FUNC(glColor3ub) \
    USE_GL_FUNC(glColor3ubv) \
    USE_GL_FUNC(glColor3ui) \
    USE_GL_FUNC(glColor3uiv) \
    USE_GL_FUNC(glColor3us) \
    USE_GL_FUNC(glColor3usv) \
    USE_GL_FUNC(glColor4b) \
    USE_GL_FUNC(glColor4bv) \
    USE_GL_FUNC(glColor4d) \
    USE_GL_FUNC(glColor4dv) \
    USE_GL_FUNC(glColor4f) \
    USE_GL_FUNC(glColor4fv) \
    USE_GL_FUNC(glColor4i) \
    USE_GL_FUNC(glColor4iv) \
    USE_GL_FUNC(glColor4s) \
    USE_GL_FUNC(glColor4sv) \
    USE_GL_FUNC(glColor4ub) \
    USE_GL_FUNC(glColor4ubv) \
    USE_GL_FUNC(glColor4ui) \
    USE_GL_FUNC(glColor4uiv) \
    USE_GL_FUNC(glColor4us) \
    USE_GL_FUNC(glColor4usv) \
    USE_GL_FUNC(glColorMask) \
    USE_GL_FUNC(glColorMaterial) \
    USE_GL_FUNC(glColorPointer) \
    USE_GL_FUNC(glCopyPixels) \
    USE_GL_FUNC(glCopyTexImage1D) \
    USE_GL_FUNC(glCopyTexImage2D) \
    USE_GL_FUNC(glCopyTexSubImage1D) \
    USE_GL_FUNC(glCopyTexSubImage2D) \
    USE_GL_FUNC(glCullFace) \
    USE_GL_FUNC(glDeleteLists) \
    USE_GL_FUNC(glDeleteTextures) \
    USE_GL_FUNC(glDepthFunc) \
    USE_GL_FUNC(glDepthMask) \
    USE_GL_FUNC(glDepthRange) \
    USE_GL_FUNC(glDisable) \
    USE_GL_FUNC(glDisableClientState) \
    USE_GL_FUNC(glDrawArrays) \
    USE_GL_FUNC(glDrawBuffer) \
    USE_GL_FUNC(glDrawElements) \
    USE_GL_FUNC(glDrawPixels) \
    USE_GL_FUNC(glEdgeFlag) \
    USE_GL_FUNC(glEdgeFlagPointer) \
    USE_GL_FUNC(glEdgeFlagv) \
    USE_GL_FUNC(glEnable) \
    USE_GL_FUNC(glEnableClientState) \
    USE_GL_FUNC(glEnd) \
    USE_GL_FUNC(glEndList) \
    USE_GL_FUNC(glEvalCoord1d) \
    USE_GL_FUNC(glEvalCoord1dv) \
    USE_GL_FUNC(glEvalCoord1f) \
    USE_GL_FUNC(glEvalCoord1fv) \
    USE_GL_FUNC(glEvalCoord2d) \
    USE_GL_FUNC(glEvalCoord2dv) \
    USE_GL_FUNC(glEvalCoord2f) \
    USE_GL_FUNC(glEvalCoord2fv) \
    USE_GL_FUNC(glEvalMesh1) \
    USE_GL_FUNC(glEvalMesh2) \
    USE_GL_FUNC(glEvalPoint1) \
    USE_GL_FUNC(glEvalPoint2) \
    USE_GL_FUNC(glFeedbackBuffer) \
    USE_GL_FUNC(glFogf) \
    USE_GL_FUNC(glFogfv) \
    USE_GL_FUNC(glFogi) \
    USE_GL_FUNC(glFogiv) \
    USE_GL_FUNC(glFrontFace) \
    USE_GL_FUNC(glFrustum) \
    USE_GL_FUNC(glGenLists) \
    USE_GL_FUNC(glGenTextures) \
    USE_GL_FUNC(glGetBooleanv) \
    USE_GL_FUNC(glGetClipPlane) \
    USE_GL_FUNC(glGetDoublev) \
    USE_GL_FUNC(glGetError) \
    USE_GL_FUNC(glGetFloatv) \
    USE_GL_FUNC(glGetIntegerv) \
    USE_GL_FUNC(glGetLightfv) \
    USE_GL_FUNC(glGetLightiv) \
    USE_GL_FUNC(glGetMapdv) \
    USE_GL_FUNC(glGetMapfv) \
    USE_GL_FUNC(glGetMapiv) \
    USE_GL_FUNC(glGetMaterialfv) \
    USE_GL_FUNC(glGetMaterialiv) \
    USE_GL_FUNC(glGetPixelMapfv) \
    USE_GL_FUNC(glGetPixelMapuiv) \
    USE_GL_FUNC(glGetPixelMapusv) \
    USE_GL_FUNC(glGetPointerv) \
    USE_GL_FUNC(glGetPolygonStipple) \
    USE_GL_FUNC(glGetString) \
    USE_GL_FUNC(glGetTexEnvfv) \
    USE_GL_FUNC(glGetTexEnviv) \
    USE_GL_FUNC(glGetTexGendv) \
    USE_GL_FUNC(glGetTexGenfv) \
    USE_GL_FUNC(glGetTexGeniv) \
    USE_GL_FUNC(glGetTexImage) \
    USE_GL_FUNC(glGetTexLevelParameterfv) \
    USE_GL_FUNC(glGetTexLevelParameteriv) \
    USE_GL_FUNC(glGetTexParameterfv) \
    USE_GL_FUNC(glGetTexParameteriv) \
    USE_GL_FUNC(glHint) \
    USE_GL_FUNC(glIndexMask) \
    USE_GL_FUNC(glIndexPointer) \
    USE_GL_FUNC(glIndexd) \
    USE_GL_FUNC(glIndexdv) \
    USE_GL_FUNC(glIndexf) \
    USE_GL_FUNC(glIndexfv) \
    USE_GL_FUNC(glIndexi) \
    USE_GL_FUNC(glIndexiv) \
    USE_GL_FUNC(glIndexs) \
    USE_GL_FUNC(glIndexsv) \
    USE_GL_FUNC(glIndexub) \
    USE_GL_FUNC(glIndexubv) \
    USE_GL_FUNC(glInitNames) \
    USE_GL_FUNC(glInterleavedArrays) \
    USE_GL_FUNC(glIsEnabled) \
    USE_GL_FUNC(glIsList) \
    USE_GL_FUNC(glIsTexture) \
    USE_GL_FUNC(glLightModelf) \
    USE_GL_FUNC(glLightModelfv) \
    USE_GL_FUNC(glLightModeli) \
    USE_GL_FUNC(glLightModeliv) \
    USE_GL_FUNC(glLightf) \
    USE_GL_FUNC(glLightfv) \
    USE_GL_FUNC(glLighti) \
    USE_GL_FUNC(glLightiv) \
    USE_GL_FUNC(glLineStipple) \
    USE_GL_FUNC(glLineWidth) \
    USE_GL_FUNC(glListBase) \
    USE_GL_FUNC(glLoadIdentity) \
    USE_GL_FUNC(glLoadMatrixd) \
    USE_GL_FUNC(glLoadMatrixf) \
    USE_GL_FUNC(glLoadName) \
    USE_GL_FUNC(glLogicOp) \
    USE_GL_FUNC(glMap1d) \
    USE_GL_FUNC(glMap1f) \
    USE_GL_FUNC(glMap2d) \
    USE_GL_FUNC(glMap2f) \
    USE_GL_FUNC(glMapGrid1d) \
    USE_GL_FUNC(glMapGrid1f) \
    USE_GL_FUNC(glMapGrid2d) \
    USE_GL_FUNC(glMapGrid2f) \
    USE_GL_FUNC(glMaterialf) \
    USE_GL_FUNC(glMaterialfv) \
    USE_GL_FUNC(glMateriali) \
    USE_GL_FUNC(glMaterialiv) \
    USE_GL_FUNC(glMatrixMode) \
    USE_GL_FUNC(glMultMatrixd) \
    USE_GL_FUNC(glMultMatrixf) \
    USE_GL_FUNC(glNewList) \
    USE_GL_FUNC(glNormal3b) \
    USE_GL_FUNC(glNormal3bv) \
    USE_GL_FUNC(glNormal3d) \
    USE_GL_FUNC(glNormal3dv) \
    USE_GL_FUNC(glNormal3f) \
    USE_GL_FUNC(glNormal3fv) \
    USE_GL_FUNC(glNormal3i) \
    USE_GL_FUNC(glNormal3iv) \
    USE_GL_FUNC(glNormal3s) \
    USE_GL_FUNC(glNormal3sv) \
    USE_GL_FUNC(glNormalPointer) \
    USE_GL_FUNC(glOrtho) \
    USE_GL_FUNC(glPassThrough) \
    USE_GL_FUNC(glPixelMapfv) \
    USE_GL_FUNC(glPixelMapuiv) \
    USE_GL_FUNC(glPixelMapusv) \
    USE_GL_FUNC(glPixelStoref) \
    USE_GL_FUNC(glPixelStorei) \
    USE_GL_FUNC(glPixelTransferf) \
    USE_GL_FUNC(glPixelTransferi) \
    USE_GL_FUNC(glPixelZoom) \
    USE_GL_FUNC(glPointSize) \
    USE_GL_FUNC(glPolygonMode) \
    USE_GL_FUNC(glPolygonOffset) \
    USE_GL_FUNC(glPolygonStipple) \
    USE_GL_FUNC(glPopAttrib) \
    USE_GL_FUNC(glPopClientAttrib) \
    USE_GL_FUNC(glPopMatrix) \
    USE_GL_FUNC(glPopName) \
    USE_GL_FUNC(glPrioritizeTextures) \
    USE_GL_FUNC(glPushAttrib) \
    USE_GL_FUNC(glPushClientAttrib) \
    USE_GL_FUNC(glPushMatrix) \
    USE_GL_FUNC(glPushName) \
    USE_GL_FUNC(glRasterPos2d) \
    USE_GL_FUNC(glRasterPos2dv) \
    USE_GL_FUNC(glRasterPos2f) \
    USE_GL_FUNC(glRasterPos2fv) \
    USE_GL_FUNC(glRasterPos2i) \
    USE_GL_FUNC(glRasterPos2iv) \
    USE_GL_FUNC(glRasterPos2s) \
    USE_GL_FUNC(glRasterPos2sv) \
    USE_GL_FUNC(glRasterPos3d) \
    USE_GL_FUNC(glRasterPos3dv) \
    USE_GL_FUNC(glRasterPos3f) \
    USE_GL_FUNC(glRasterPos3fv) \
    USE_GL_FUNC(glRasterPos3i) \
    USE_GL_FUNC(glRasterPos3iv) \
    USE_GL_FUNC(glRasterPos3s) \
    USE_GL_FUNC(glRasterPos3sv) \
    USE_GL_FUNC(glRasterPos4d) \
    USE_GL_FUNC(glRasterPos4dv) \
    USE_GL_FUNC(glRasterPos4f) \
    USE_GL_FUNC(glRasterPos4fv) \
    USE_GL_FUNC(glRasterPos4i) \
    USE_GL_FUNC(glRasterPos4iv) \
    USE_GL_FUNC(glRasterPos4s) \
    USE_GL_FUNC(glRasterPos4sv) \
    USE_GL_FUNC(glReadBuffer) \
    USE_GL_FUNC(glReadPixels) \
    USE_GL_FUNC(glRectd) \
    USE_GL_FUNC(glRectdv) \
    USE_GL_FUNC(glRectf) \
    USE_GL_FUNC(glRectfv) \
    USE_GL_FUNC(glRecti) \
    USE_GL_FUNC(glRectiv) \
    USE_GL_FUNC(glRects) \
    USE_GL_FUNC(glRectsv) \
    USE_GL_FUNC(glRenderMode) \
    USE_GL_FUNC(glRotated) \
    USE_GL_FUNC(glRotatef) \
    USE_GL_FUNC(glScaled) \
    USE_GL_FUNC(glScalef) \
    USE_GL_FUNC(glScissor) \
    USE_GL_FUNC(glSelectBuffer) \
    USE_GL_FUNC(glShadeModel) \
    USE_GL_FUNC(glStencilFunc) \
    USE_GL_FUNC(glStencilMask) \
    USE_GL_FUNC(glStencilOp) \
    USE_GL_FUNC(glTexCoord1d) \
    USE_GL_FUNC(glTexCoord1dv) \
    USE_GL_FUNC(glTexCoord1f) \
    USE_GL_FUNC(glTexCoord1fv) \
    USE_GL_FUNC(glTexCoord1i) \
    USE_GL_FUNC(glTexCoord1iv) \
    USE_GL_FUNC(glTexCoord1s) \
    USE_GL_FUNC(glTexCoord1sv) \
    USE_GL_FUNC(glTexCoord2d) \
    USE_GL_FUNC(glTexCoord2dv) \
    USE_GL_FUNC(glTexCoord2f) \
    USE_GL_FUNC(glTexCoord2fv) \
    USE_GL_FUNC(glTexCoord2i) \
    USE_GL_FUNC(glTexCoord2iv) \
    USE_GL_FUNC(glTexCoord2s) \
    USE_GL_FUNC(glTexCoord2sv) \
    USE_GL_FUNC(glTexCoord3d) \
    USE_GL_FUNC(glTexCoord3dv) \
    USE_GL_FUNC(glTexCoord3f) \
    USE_GL_FUNC(glTexCoord3fv) \
    USE_GL_FUNC(glTexCoord3i) \
    USE_GL_FUNC(glTexCoord3iv) \
    USE_GL_FUNC(glTexCoord3s) \
    USE_GL_FUNC(glTexCoord3sv) \
    USE_GL_FUNC(glTexCoord4d) \
    USE_GL_FUNC(glTexCoord4dv) \
    USE_GL_FUNC(glTexCoord4f) \
    USE_GL_FUNC(glTexCoord4fv) \
    USE_GL_FUNC(glTexCoord4i) \
    USE_GL_FUNC(glTexCoord4iv) \
    USE_GL_FUNC(glTexCoord4s) \
    USE_GL_FUNC(glTexCoord4sv) \
    USE_GL_FUNC(glTexCoordPointer) \
    USE_GL_FUNC(glTexEnvf) \
    USE_GL_FUNC(glTexEnvfv) \
    USE_GL_FUNC(glTexEnvi) \
    USE_GL_FUNC(glTexEnviv) \
    USE_GL_FUNC(glTexGend) \
    USE_GL_FUNC(glTexGendv) \
    USE_GL_FUNC(glTexGenf) \
    USE_GL_FUNC(glTexGenfv) \
    USE_GL_FUNC(glTexGeni) \
    USE_GL_FUNC(glTexGeniv) \
    USE_GL_FUNC(glTexImage1D) \
    USE_GL_FUNC(glTexImage2D) \
    USE_GL_FUNC(glTexParameterf) \
    USE_GL_FUNC(glTexParameterfv) \
    USE_GL_FUNC(glTexParameteri) \
    USE_GL_FUNC(glTexParameteriv) \
    USE_GL_FUNC(glTexSubImage1D) \
    USE_GL_FUNC(glTexSubImage2D) \
    USE_GL_FUNC(glTranslated) \
    USE_GL_FUNC(glTranslatef) \
    USE_GL_FUNC(glVertex2d) \
    USE_GL_FUNC(glVertex2dv) \
    USE_GL_FUNC(glVertex2f) \
    USE_GL_FUNC(glVertex2fv) \
    USE_GL_FUNC(glVertex2i) \
    USE_GL_FUNC(glVertex2iv) \
    USE_GL_FUNC(glVertex2s) \
    USE_GL_FUNC(glVertex2sv) \
    USE_GL_FUNC(glVertex3d) \
    USE_GL_FUNC(glVertex3dv) \
    USE_GL_FUNC(glVertex3f) \
    USE_GL_FUNC(glVertex3fv) \
    USE_GL_FUNC(glVertex3i) \
    USE_GL_FUNC(glVertex3iv) \
    USE_GL_FUNC(glVertex3s) \
    USE_GL_FUNC(glVertex3sv) \
    USE_GL_FUNC(glVertex4d) \
    USE_GL_FUNC(glVertex4dv) \
    USE_GL_FUNC(glVertex4f) \
    USE_GL_FUNC(glVertex4fv) \
    USE_GL_FUNC(glVertex4i) \
    USE_GL_FUNC(glVertex4iv) \
    USE_GL_FUNC(glVertex4s) \
    USE_GL_FUNC(glVertex4sv) \
    USE_GL_FUNC(glVertexPointer) \
    USE_GL_FUNC(glViewport) \
    USE_GL_FUNC(glPointParameterfv) \

#define WGL_FUNCS_GEN \
    USE_WGL_FUNC(wglDeleteContext) \
    USE_WGL_FUNC(wglGetCurrentContext) \
    USE_WGL_FUNC(wglGetCurrentDC) \
    USE_WGL_FUNC(wglGetProcAddress) \
    USE_WGL_FUNC(wglMakeCurrent) \
    USE_WGL_FUNC(wglShareLists) \
    USE_WGL_FUNC(wglSwapLayerBuffers) \
    USE_WGL_FUNC(wglChoosePixelFormat) \
    USE_WGL_FUNC(wglDescribePixelFormat) \
    USE_WGL_FUNC(wglGetPixelFormat) \
    USE_WGL_FUNC(wglSetPixelFormat)

#define VBOX_FUNCS_GEN \
    USE_WGL_FUNC(VBoxCreateContext) \
    USE_WGL_FUNC(VBoxFlushToHost)

/* OpenGL extensions. */
typedef enum wined3d_gl_extension
{
    WINED3D_GL_EXT_NONE,

    /* APPLE */
    APPLE_CLIENT_STORAGE,
    APPLE_FENCE,
    APPLE_FLOAT_PIXELS,
    APPLE_FLUSH_BUFFER_RANGE,
    APPLE_FLUSH_RENDER,
    APPLE_YCBCR_422,
    /* ARB */
    ARB_COLOR_BUFFER_FLOAT,
    ARB_DEPTH_BUFFER_FLOAT,
    ARB_DEPTH_CLAMP,
    ARB_DEPTH_TEXTURE,
    ARB_DRAW_BUFFERS,
    ARB_FRAGMENT_PROGRAM,
    ARB_FRAGMENT_SHADER,
    ARB_FRAMEBUFFER_OBJECT,
    ARB_GEOMETRY_SHADER4,
    ARB_HALF_FLOAT_PIXEL,
    ARB_HALF_FLOAT_VERTEX,
    ARB_IMAGING,
    ARB_MAP_BUFFER_RANGE,
    ARB_MULTISAMPLE,
    ARB_MULTITEXTURE,
    ARB_OCCLUSION_QUERY,
    ARB_PIXEL_BUFFER_OBJECT,
    ARB_POINT_PARAMETERS,
    ARB_POINT_SPRITE,
    ARB_PROVOKING_VERTEX,
    ARB_SHADER_OBJECTS,
    ARB_SHADER_TEXTURE_LOD,
    ARB_SHADING_LANGUAGE_100,
    ARB_SYNC,
    ARB_TEXTURE_BORDER_CLAMP,
    ARB_TEXTURE_COMPRESSION,
    ARB_TEXTURE_CUBE_MAP,
    ARB_TEXTURE_ENV_ADD,
    ARB_TEXTURE_ENV_COMBINE,
    ARB_TEXTURE_ENV_DOT3,
    ARB_TEXTURE_FLOAT,
    ARB_TEXTURE_MIRRORED_REPEAT,
    ARB_TEXTURE_NON_POWER_OF_TWO,
    ARB_TEXTURE_RECTANGLE,
    ARB_TEXTURE_RG,
    ARB_VERTEX_ARRAY_BGRA,
    ARB_VERTEX_BLEND,
    ARB_VERTEX_BUFFER_OBJECT,
    ARB_VERTEX_PROGRAM,
    ARB_VERTEX_SHADER,
    /* ATI */
    ATI_FRAGMENT_SHADER,
    ATI_SEPARATE_STENCIL,
    ATI_TEXTURE_COMPRESSION_3DC,
    ATI_TEXTURE_ENV_COMBINE3,
    ATI_TEXTURE_MIRROR_ONCE,
    /* EXT */
    EXT_BLEND_COLOR,
    EXT_BLEND_EQUATION_SEPARATE,
    EXT_BLEND_FUNC_SEPARATE,
    EXT_BLEND_MINMAX,
    EXT_DRAW_BUFFERS2,
    EXT_FOG_COORD,
    EXT_FRAMEBUFFER_BLIT,
    EXT_FRAMEBUFFER_MULTISAMPLE,
    EXT_FRAMEBUFFER_OBJECT,
    EXT_GPU_PROGRAM_PARAMETERS,
    EXT_GPU_SHADER4,
    EXT_PACKED_DEPTH_STENCIL,
    EXT_PALETTED_TEXTURE,
    EXT_POINT_PARAMETERS,
    EXT_PROVOKING_VERTEX,
    EXT_SECONDARY_COLOR,
    EXT_STENCIL_TWO_SIDE,
    EXT_STENCIL_WRAP,
    EXT_TEXTURE3D,
    EXT_TEXTURE_COMPRESSION_RGTC,
    EXT_TEXTURE_COMPRESSION_S3TC,
    EXT_TEXTURE_ENV_ADD,
    EXT_TEXTURE_ENV_COMBINE,
    EXT_TEXTURE_ENV_DOT3,
    EXT_TEXTURE_FILTER_ANISOTROPIC,
    EXT_TEXTURE_LOD_BIAS,
    EXT_TEXTURE_SRGB,
    EXT_VERTEX_ARRAY_BGRA,
    /* NVIDIA */
    NV_DEPTH_CLAMP,
    NV_FENCE,
    NV_FOG_DISTANCE,
    NV_FRAGMENT_PROGRAM,
    NV_FRAGMENT_PROGRAM2,
    NV_FRAGMENT_PROGRAM_OPTION,
    NV_HALF_FLOAT,
    NV_LIGHT_MAX_EXPONENT,
    NV_REGISTER_COMBINERS,
    NV_REGISTER_COMBINERS2,
    NV_TEXGEN_REFLECTION,
    NV_TEXTURE_ENV_COMBINE4,
    NV_TEXTURE_SHADER,
    NV_TEXTURE_SHADER2,
    NV_VERTEX_PROGRAM,
    NV_VERTEX_PROGRAM1_1,
    NV_VERTEX_PROGRAM2,
    NV_VERTEX_PROGRAM2_OPTION,
    NV_VERTEX_PROGRAM3,
    /* SGI */
    SGIS_GENERATE_MIPMAP,
    SGI_VIDEO_SYNC,
    /* WGL extensions */
    WGL_ARB_PIXEL_FORMAT,
    WGL_WINE_PIXEL_FORMAT_PASSTHROUGH,

    VBOX_SHARED_CONTEXTS,

    /* Internally used */
    WINE_NORMALIZED_TEXRECT,

    WINED3D_GL_EXT_COUNT,
} GL_SupportedExt;

/* GL_APPLE_client_storage */
#ifndef GL_APPLE_client_storage
#define GL_APPLE_client_storage 1
#define GL_UNPACK_CLIENT_STORAGE_APPLE                      0x85b2
#endif

/* GL_APPLE_fence */
#ifndef GL_APPLE_fence
#define GL_APPLE_fence 1
#define GL_DRAW_PIXELS_APPLE                                0x8a0a
#define GL_FENCE_APPLE                                      0x8a0b
#endif
typedef void (WINE_GLAPI *PGLFNGENFENCESAPPLEPROC)(GLsizei, GLuint *);
typedef void (WINE_GLAPI *PGLFNDELETEFENCESAPPLEPROC)(GLuint, const GLuint *);
typedef void (WINE_GLAPI *PGLFNSETFENCEAPPLEPROC)(GLuint);
typedef GLboolean (WINE_GLAPI *PGLFNTESTFENCEAPPLEPROC)(GLuint);
typedef void (WINE_GLAPI *PGLFNFINISHFENCEAPPLEPROC)(GLuint);
typedef GLboolean (WINE_GLAPI *PGLFNISFENCEAPPLEPROC)(GLuint);
typedef GLboolean (WINE_GLAPI *PGLFNTESTOBJECTAPPLEPROC)(GLenum, GLuint);
typedef void (WINE_GLAPI *PGLFNFINISHOBJECTAPPLEPROC)(GLenum, GLuint);

/* GL_APPLE_float_pixels */
#ifndef GL_APPLE_float_pixels
#define GL_APPLE_float_pixels 1
#define GL_HALF_APPLE                                       0x140b
#define GL_COLOR_FLOAT_APPLE                                0x8a0f
#define GL_RGBA_FLOAT32_APPLE                               0x8814
#define GL_RGB_FLOAT32_APPLE                                0x8815
#define GL_ALPHA_FLOAT32_APPLE                              0x8816
#define GL_INTENSITY_FLOAT32_APPLE                          0x8817
#define GL_LUMINANCE_FLOAT32_APPLE                          0x8818
#define GL_LUMINANCE_ALPHA_FLOAT32_APPLE                    0x8819
#define GL_RGBA_FLOAT16_APPLE                               0x881a
#define GL_RGB_FLOAT16_APPLE                                0x881b
#define GL_ALPHA_FLOAT16_APPLE                              0x881c
#define GL_INTENSITY_FLOAT16_APPLE                          0x881d
#define GL_LUMINANCE_FLOAT16_APPLE                          0x881e
#define GL_LUMINANCE_ALPHA_FLOAT16_APPLE                    0x881f
#endif

/* GL_APPLE_flush_buffer_range */
#ifndef GL_APPLE_flush_buffer_range
#define GL_APPLE_flush_buffer_range 1
#define GL_BUFFER_SERIALIZED_MODIFY_APPLE                   0x8a12
#define GL_BUFFER_FLUSHING_UNMAP_APPLE                      0x8a13
typedef void (WINE_GLAPI *PGLFNBUFFERPARAMETERIAPPLE)(GLenum target, GLenum pname, GLint param);
typedef void (WINE_GLAPI *PGLFNFLUSHMAPPEDBUFFERRANGEAPPLE)(GLenum target, GLintptr offset, GLsizeiptr size);
#endif

/* GL_APPLE_flush_render */
typedef void (WINE_GLAPI *PGLFNFLUSHRENDERAPPLEPROC)(void);
typedef void (WINE_GLAPI *PGLFNFINISHRENDERAPPLEPROC)(void);

/* GL_APPLE_ycbcr_422 */
#ifndef GL_APPLE_ycbcr_422
#define GL_APPLE_ycbcr_422 1
#define GL_YCBCR_422_APPLE                                  0x85b9
#define UNSIGNED_SHORT_8_8_APPLE                            0x85ba
#define UNSIGNED_SHORT_8_8_REV_APPLE                        0x85bb
#endif

/* GL_ARB_color_buffer_float */
#ifndef GL_ARB_color_buffer_float
#define GL_ARB_color_buffer_float 1
#define GL_RGBA_FLOAT_MODE_ARB                              0x8820
#define GL_CLAMP_VERTEX_COLOR_ARB                           0x891a
#define GL_CLAMP_FRAGMENT_COLOR_ARB                         0x891b
#define GL_CLAMP_READ_COLOR_ARB                             0x891c
#define GL_FIXED_ONLY_ARB                                   0x891d
#endif
typedef void (WINE_GLAPI *PGLFNCLAMPCOLORARBPROC)(GLenum target, GLenum clamp);

/* GL_ARB_depth_buffer_float */
#ifndef GL_ARB_depth_buffer_float
#define GL_ARB_depth_buffer_float 1
#define GL_DEPTH_COMPONENT32F                               0x8cac
#define GL_DEPTH32F_STENCIL8                                0x8cad
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV                   0x8dad
#endif

/* GL_ARB_depth_clamp */
#ifndef GL_ARB_depth_clamp
#define GL_ARB_depth_clamp 1
#define GL_DEPTH_CLAMP                                      0x864f
#endif

/* GL_ARB_depth_texture */
#ifndef GL_ARB_depth_texture
#define GL_ARB_depth_texture 1
#define GL_DEPTH_COMPONENT16_ARB                            0x81a5
#define GL_DEPTH_COMPONENT24_ARB                            0x81a6
#define GL_DEPTH_COMPONENT32_ARB                            0x81a7
#define GL_TEXTURE_DEPTH_SIZE_ARB                           0x884a
#define GL_DEPTH_TEXTURE_MODE_ARB                           0x884b
#endif

/* GL_ARB_draw_buffers */
#ifndef GL_ARB_draw_buffers
#define GL_ARB_draw_buffers 1
#define GL_MAX_DRAW_BUFFERS_ARB                             0x8824
#define GL_DRAW_BUFFER0_ARB                                 0x8825
#define GL_DRAW_BUFFER1_ARB                                 0x8826
#define GL_DRAW_BUFFER2_ARB                                 0x8827
#define GL_DRAW_BUFFER3_ARB                                 0x8828
#define GL_DRAW_BUFFER4_ARB                                 0x8829
#define GL_DRAW_BUFFER5_ARB                                 0x882a
#define GL_DRAW_BUFFER6_ARB                                 0x882b
#define GL_DRAW_BUFFER7_ARB                                 0x882c
#define GL_DRAW_BUFFER8_ARB                                 0x882d
#define GL_DRAW_BUFFER9_ARB                                 0x882e
#define GL_DRAW_BUFFER10_ARB                                0x882f
#define GL_DRAW_BUFFER11_ARB                                0x8830
#define GL_DRAW_BUFFER12_ARB                                0x8831
#define GL_DRAW_BUFFER13_ARB                                0x8832
#define GL_DRAW_BUFFER14_ARB                                0x8833
#define GL_DRAW_BUFFER15_ARB                                0x8834
#endif
typedef void (WINE_GLAPI *PGLFNDRAWBUFFERSARBPROC)(GLsizei n, const GLenum *bufs);

/* GL_ARB_fragment_program */
#ifndef GL_ARB_fragment_program
#define GL_ARB_fragment_program 1
#define GL_FRAGMENT_PROGRAM_ARB                             0x8804
#define GL_PROGRAM_ALU_INSTRUCTIONS_ARB                     0x8805
#define GL_PROGRAM_TEX_INSTRUCTIONS_ARB                     0x8806
#define GL_PROGRAM_TEX_INDIRECTIONS_ARB                     0x8807
#define GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB              0x8808
#define GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB              0x8809
#define GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB              0x880a
#define GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB                 0x880b
#define GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB                 0x880c
#define GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB                 0x880d
#define GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB          0x880e
#define GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB          0x880f
#define GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB          0x8810
#define GL_MAX_TEXTURE_COORDS_ARB                           0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS_ARB                      0x8872
/* All ARB_fragment_program entry points are shared with ARB_vertex_program. */
#endif

/* GL_ARB_fragment_shader */
#ifndef GL_ARB_fragment_shader
#define GL_ARB_fragment_shader 1
#define GL_FRAGMENT_SHADER_ARB                              0x8b30
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB              0x8b49
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB              0x8b8b
#endif

/* GL_ARB_framebuffer_object */
#ifndef GL_ARB_framebuffer_object
#define GL_ARB_framebuffer_object 1
#define GL_FRAMEBUFFER                                      0x8d40
#define GL_READ_FRAMEBUFFER                                 0x8ca8
#define GL_DRAW_FRAMEBUFFER                                 0x8ca9
#define GL_RENDERBUFFER                                     0x8d41
#define GL_STENCIL_INDEX1                                   0x8d46
#define GL_STENCIL_INDEX4                                   0x8d47
#define GL_STENCIL_INDEX8                                   0x8d48
#define GL_STENCIL_INDEX16                                  0x8d49
#define GL_RENDERBUFFER_WIDTH                               0x8d42
#define GL_RENDERBUFFER_HEIGHT                              0x8d43
#define GL_RENDERBUFFER_INTERNAL_FORMAT                     0x8d44
#define GL_RENDERBUFFER_RED_SIZE                            0x8d50
#define GL_RENDERBUFFER_GREEN_SIZE                          0x8d51
#define GL_RENDERBUFFER_BLUE_SIZE                           0x8d52
#define GL_RENDERBUFFER_ALPHA_SIZE                          0x8d53
#define GL_RENDERBUFFER_DEPTH_SIZE                          0x8d54
#define GL_RENDERBUFFER_STENCIL_SIZE                        0x8d55
#define GL_RENDERBUFFER_SAMPLES                             0x8cab
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE               0x8cd0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME               0x8cd1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL             0x8cd2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE     0x8cd3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER             0x8cd4
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING            0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE            0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE                  0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE                0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE                 0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE                0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE                0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE              0x8217
#define GL_SRGB                                             0x8c40
#define GL_UNSIGNED_NORMALIZED                              0x8c17
#define GL_FRAMEBUFFER_DEFAULT                              0x8218
#define GL_INDEX                                            0x8222
#define GL_COLOR_ATTACHMENT0                                0x8ce0
#define GL_COLOR_ATTACHMENT1                                0x8ce1
#define GL_COLOR_ATTACHMENT2                                0x8ce2
#define GL_COLOR_ATTACHMENT3                                0x8ce3
#define GL_COLOR_ATTACHMENT4                                0x8ce4
#define GL_COLOR_ATTACHMENT5                                0x8ce5
#define GL_COLOR_ATTACHMENT6                                0x8ce6
#define GL_COLOR_ATTACHMENT7                                0x8ce7
#define GL_COLOR_ATTACHMENT8                                0x8ce8
#define GL_COLOR_ATTACHMENT9                                0x8ce9
#define GL_COLOR_ATTACHMENT10                               0x8cea
#define GL_COLOR_ATTACHMENT11                               0x8ceb
#define GL_COLOR_ATTACHMENT12                               0x8cec
#define GL_COLOR_ATTACHMENT13                               0x8ced
#define GL_COLOR_ATTACHMENT14                               0x8cee
#define GL_COLOR_ATTACHMENT15                               0x8cef
#define GL_DEPTH_ATTACHMENT                                 0x8d00
#define GL_STENCIL_ATTACHMENT                               0x8d20
#define GL_DEPTH_STENCIL_ATTACHMENT                         0x821a
#define GL_MAX_SAMPLES                                      0x8d57
#define GL_FRAMEBUFFER_COMPLETE                             0x8cd5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT                0x8cd6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT        0x8cd7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER               0x8cdb
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER               0x8cdc
#define GL_FRAMEBUFFER_UNSUPPORTED                          0x8cdd
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE               0x8d56
#define GL_FRAMEBUFFER_UNDEFINED                            0x8219
#define GL_FRAMEBUFFER_BINDING                              0x8ca6
#define GL_DRAW_FRAMEBUFFER_BINDING                         0x8ca6
#define GL_READ_FRAMEBUFFER_BINDING                         0x8caa
#define GL_RENDERBUFFER_BINDING                             0x8ca7
#define GL_MAX_COLOR_ATTACHMENTS                            0x8cdf
#define GL_MAX_RENDERBUFFER_SIZE                            0x84e8
#define GL_INVALID_FRAMEBUFFER_OPERATION                    0x0506
#define GL_DEPTH_STENCIL                                    0x84f9
#define GL_UNSIGNED_INT_24_8                                0x84fa
#define GL_DEPTH24_STENCIL8                                 0x88f0
#define GL_TEXTURE_STENCIL_SIZE                             0x88f1
#endif
typedef GLboolean (WINE_GLAPI *PGLFNGLISRENDERBUFFERPROC)(GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (WINE_GLAPI *PGLFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (WINE_GLAPI *PGLFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat,
        GLsizei width, GLsizei height);
typedef void (WINE_GLAPI *PGLFNRENDERBUFFERSTORAGEMULTISAMPLEPROC)(GLenum target, GLsizei samples,
        GLenum internalformat, GLsizei width, GLsizei height);
typedef void (WINE_GLAPI *PGLFNGLGETRENDERBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean (WINE_GLAPI *PGLFNGLISFRAMEBUFFERPROC)(GLuint framebuffer);
typedef void (WINE_GLAPI *PGLFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (WINE_GLAPI *PGLFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (WINE_GLAPI *PGLFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef GLenum (WINE_GLAPI *PGLFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE1DPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE3DPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level, GLint layer);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURELAYERPROC)(GLenum target, GLenum attachment,
        GLuint texture, GLint level, GLint layer);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment,
        GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum target, GLenum attachment,
        GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGLBLITFRAMEBUFFERPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
        GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (WINE_GLAPI *PGLFNGLGENERATEMIPMAPPROC)(GLenum target);

/* GL_ARB_geometry_shader4 */
#ifndef GL_ARB_geometry_shader4
#define GL_ARB_geometry_shader4 1
#define GL_GEOMETRY_SHADER_ARB                              0x8dd9
#define GL_GEOMETRY_VERTICES_OUT_ARB                        0x8dda
#define GL_GEOMETRY_INPUT_TYPE_ARB                          0x8ddb
#define GL_GEOMETRY_OUTPUT_TYPE_ARB                         0x8ddc
#define GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_ARB             0x8c29
#define GL_MAX_GEOMETRY_VARYING_COMPONENTS_ARB              0x8ddd
#define GL_MAX_VERTEX_VARYING_COMPONENTS_ARB                0x8dde
#define GL_MAX_VARYING_COMPONENTS_ARB                       0x8b4b
#define GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_ARB              0x8ddf
#define GL_MAX_GEOMETRY_OUTPUT_VERTICES_ARB                 0x8de0
#define GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_ARB         0x8de1
#define GL_LINES_ADJACENCY_ARB                              0x000a
#define GL_LINE_STRIP_ADJACENCY_ARB                         0x000b
#define GL_TRIANGLES_ADJACENCY_ARB                          0x000c
#define GL_TRIANGLE_STRIP_ADJACENCY_ARB                     0x000d
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_ARB         0x8da8
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_COUNT_ARB           0x8da9
#define GL_FRAMEBUFFER_ATTACHMENT_LAYERED_ARB               0x8da7
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER             0x8cd4
#define GL_PROGRAM_POINT_SIZE_ARB                           0x8642
#endif
typedef void (WINE_GLAPI *PGLFNPROGRAMPARAMETERIARBPROC)(GLuint program, GLenum pname, GLint value);
typedef void (WINE_GLAPI *PGLFNFRAMEBUFFERTEXTUREARBPROC)(GLenum target, GLenum attachment,
        GLuint texture, GLint level);
typedef void (WINE_GLAPI *PGLFNFRAMEBUFFERTEXTURELAYERARBPROC)(GLenum target, GLenum attachment,
        GLuint texture, GLint level, GLint layer);
typedef void (WINE_GLAPI *PGLFNFRAMEBUFFERTEXTUREFACEARBPROC)(GLenum target, GLenum attachment,
        GLuint texture, GLint level, GLenum face);

/* GL_ARB_half_float_pixel */
#ifndef GL_ARB_half_float_pixel
#define GL_ARB_half_float_pixel 1
#define GL_HALF_FLOAT_ARB                                   0x140b
#endif

/* GL_ARB_half_float_vertex */
#ifndef GL_ARB_half_float_vertex
#define GL_ARB_half_float_vertex 1
/* No _ARB, see extension spec */
#define GL_HALF_FLOAT                                       0x140b
#endif

/* GL_ARB_imaging */
#ifndef GL_ARB_imaging
#define GL_ARB_imaging 1
#define GL_CONSTANT_COLOR                                   0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR                         0x8002
#define GL_CONSTANT_ALPHA                                   0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA                         0x8004
#define GL_BLEND_COLOR                                      0x8005
#define GL_FUNC_ADD                                         0x8006
#define GL_MIN                                              0x8007
#define GL_MAX                                              0x8008
#define GL_BLEND_EQUATION                                   0x8009
#define GL_FUNC_SUBTRACT                                    0x800a
#define GL_FUNC_REVERSE_SUBTRACT                            0x800b
#define GL_CONVOLUTION_1D                                   0x8010
#define GL_CONVOLUTION_2D                                   0x8011
#define GL_SEPARABLE_2D                                     0x8012
#define GL_CONVOLUTION_BORDER_MODE                          0x8013
#define GL_CONVOLUTION_FILTER_SCALE                         0x8014
#define GL_CONVOLUTION_FILTER_BIAS                          0x8015
#define GL_REDUCE                                           0x8016
#define GL_CONVOLUTION_FORMAT                               0x8017
#define GL_CONVOLUTION_WIDTH                                0x8018
#define GL_CONVOLUTION_HEIGHT                               0x8019
#define GL_MAX_CONVOLUTION_WIDTH                            0x801a
#define GL_MAX_CONVOLUTION_HEIGHT                           0x801b
#define GL_POST_CONVOLUTION_RED_SCALE                       0x801d
#define GL_POST_CONVOLUTION_GREEN_SCALE                     0x801f
#define GL_POST_CONVOLUTION_BLUE_SCALE                      0x801e
#define GL_POST_CONVOLUTION_ALPHA_SCALE                     0x801f
#define GL_POST_CONVOLUTION_RED_BIAS                        0x8020
#define GL_POST_CONVOLUTION_GREEN_BIAS                      0x8021
#define GL_POST_CONVOLUTION_BLUE_BIAS                       0x8022
#define GL_POST_CONVOLUTION_ALPHA_BIAS                      0x8023
#define GL_HISTOGRAM                                        0x8024
#define GL_PROXY_HISTOGRAM                                  0x8025
#define GL_HISTOGRAM_WIDTH                                  0x8026
#define GL_HISTOGRAM_FORMAT                                 0x8027
#define GL_HISTOGRAM_RED_SIZE                               0x8028
#define GL_HISTOGRAM_GREEN_SIZE                             0x8029
#define GL_HISTOGRAM_BLUE_SIZE                              0x802a
#define GL_HISTOGRAM_ALPHA_SIZE                             0x802b
#define GL_HISTOGRAM_LUMINANCE_SIZE                         0x802c
#define GL_HISTOGRAM_SINK                                   0x802d
#define GL_MINMAX                                           0x802e
#define GL_MINMAX_FORMAT                                    0x802f
#define GL_MINMAX_SINK                                      0x8030
#define GL_TABLE_TOO_LARGE                                  0x8031
#define GL_COLOR_MATRIX                                     0x80b1
#define GL_COLOR_MATRIX_STACK_DEPTH                         0x80b2
#define GL_MAX_COLOR_MATRIX_STACK_DEPTH                     0x80b3
#define GL_POST_COLOR_MATRIX_RED_SCALE                      0x80b4
#define GL_POST_COLOR_MATRIX_GREEN_SCALE                    0x80b5
#define GL_POST_COLOR_MATRIX_BLUE_SCALE                     0x80b6
#define GL_POST_COLOR_MATRIX_ALPHA_SCALE                    0x80b7
#define GL_POST_COLOR_MATRIX_RED_BIAS                       0x80b8
#define GL_POST_COLOR_MATRIX_GREEN_BIAS                     0x80b9
#define GL_POST_COLOR_MATRIX_BLUE_BIAS                      0x80ba
#define GL_POST_COLOR_MATRIX_ALPHA_BIAS                     0x80bb
#define GL_COLOR_TABLE                                      0x80d0
#define GL_POST_CONVOLUTION_COLOR_TABLE                     0x80d1
#define GL_POST_COLOR_MATRIX_COLOR_TABLE                    0x80d2
#define GL_PROXY_COLOR_TABLE                                0x80d3
#define GL_PROXY_POST_CONVOLUTION_COLOR_TABLE               0x80d4
#define GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE              0x80d5
#define GL_COLOR_TABLE_SCALE                                0x80d6
#define GL_COLOR_TABLE_BIAS                                 0x80d7
#define GL_COLOR_TABLE_FORMAT                               0x80d8
#define GL_COLOR_TABLE_WIDTH                                0x80d9
#define GL_COLOR_TABLE_RED_SIZE                             0x80da
#define GL_COLOR_TABLE_GREEN_SIZE                           0x80db
#define GL_COLOR_TABLE_BLUE_SIZE                            0x80dc
#define GL_COLOR_TABLE_ALPHA_SIZE                           0x80dd
#define GL_COLOR_TABLE_LUMINANCE_SIZE                       0x80de
#define GL_COLOR_TABLE_INTENSITY_SIZE                       0x80df
#define GL_CONSTANT_BORDER                                  0x8151
#define GL_REPLICATE_BORDER                                 0x8153
#define GL_CONVOLUTION_BORDER_COLOR                         0x8154
#endif
typedef void (WINE_GLAPI *PGLFNBLENDCOLORPROC)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (WINE_GLAPI *PGLFNBLENDEQUATIONPROC)(GLenum mode);

/* GL_ARB_map_buffer_range */
#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range 1
#define GL_MAP_READ_BIT                                     0x0001
#define GL_MAP_WRITE_BIT                                    0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT                         0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT                        0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT                           0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT                           0x0020
#endif
typedef GLvoid *(WINE_GLAPI *PGLFNMAPBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (WINE_GLAPI *PGLFNFLUSHMAPPEDBUFFERRANGEPROC)(GLenum target, GLintptr offset, GLsizeiptr length);

/* GL_ARB_multisample */
#ifndef GL_ARB_multisample
#define GL_ARB_multisample 1
#define GL_MULTISAMPLE_ARB                                  0x809d
#define GL_SAMPLE_ALPHA_TO_COVERAGE_ARB                     0x809e
#define GL_SAMPLE_ALPHA_TO_ONE_ARB                          0x809f
#define GL_SAMPLE_COVERAGE_ARB                              0x80a0
#define GL_SAMPLE_BUFFERS_ARB                               0x80a8
#define GL_SAMPLES_ARB                                      0x80a9
#define GL_SAMPLE_COVERAGE_VALUE_ARB                        0x80aa
#define GL_SAMPLE_COVERAGE_INVERT_ARB                       0x80ab
#define GL_MULTISAMPLE_BIT_ARB                              0x20000000
#endif
typedef void (WINE_GLAPI *WINED3D_PFNGLSAMPLECOVERAGEARBPROC)(GLclampf value, GLboolean invert);

/* GL_ARB_multitexture */
#ifndef GL_ARB_multitexture
#define GL_ARB_multitexture 1
#define GL_TEXTURE0_ARB                                     0x84c0
#define GL_TEXTURE1_ARB                                     0x84c1
#define GL_TEXTURE2_ARB                                     0x84c2
#define GL_TEXTURE3_ARB                                     0x84c3
#define GL_TEXTURE4_ARB                                     0x84c4
#define GL_TEXTURE5_ARB                                     0x84c5
#define GL_TEXTURE6_ARB                                     0x84c6
#define GL_TEXTURE7_ARB                                     0x84c7
#define GL_TEXTURE8_ARB                                     0x84c8
#define GL_TEXTURE9_ARB                                     0x84c9
#define GL_TEXTURE10_ARB                                    0x84ca
#define GL_TEXTURE11_ARB                                    0x84cb
#define GL_TEXTURE12_ARB                                    0x84cc
#define GL_TEXTURE13_ARB                                    0x84cd
#define GL_TEXTURE14_ARB                                    0x84ce
#define GL_TEXTURE15_ARB                                    0x84cf
#define GL_TEXTURE16_ARB                                    0x84d0
#define GL_TEXTURE17_ARB                                    0x84d1
#define GL_TEXTURE18_ARB                                    0x84d2
#define GL_TEXTURE19_ARB                                    0x84d3
#define GL_TEXTURE20_ARB                                    0x84d4
#define GL_TEXTURE21_ARB                                    0x84d5
#define GL_TEXTURE22_ARB                                    0x84d6
#define GL_TEXTURE23_ARB                                    0x84d7
#define GL_TEXTURE24_ARB                                    0x84d8
#define GL_TEXTURE25_ARB                                    0x84d9
#define GL_TEXTURE26_ARB                                    0x84da
#define GL_TEXTURE27_ARB                                    0x84db
#define GL_TEXTURE28_ARB                                    0x84dc
#define GL_TEXTURE29_ARB                                    0x84dd
#define GL_TEXTURE30_ARB                                    0x84de
#define GL_TEXTURE31_ARB                                    0x84df
#define GL_ACTIVE_TEXTURE_ARB                               0x84e0
#define GL_CLIENT_ACTIVE_TEXTURE_ARB                        0x84e1
#define GL_MAX_TEXTURE_UNITS_ARB                            0x84e2
#endif
typedef void (WINE_GLAPI *WINED3D_PFNGLACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (WINE_GLAPI *WINED3D_PFNGLCLIENTACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD1FARBPROC)(GLenum target, GLfloat s);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD1FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD2FARBPROC)(GLenum target, GLfloat s, GLfloat t);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD2FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD3FARBPROC)(GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD3FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD4FARBPROC)(GLenum target,
        GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD4FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD2SVARBPROC)(GLenum target, const GLshort *v);
typedef void (WINE_GLAPI *WINED3D_PFNGLMULTITEXCOORD4SVARBPROC)(GLenum target, const GLshort *v);

/* GL_ARB_occlusion_query */
#ifndef GL_ARB_occlusion_query
#define GL_ARB_occlusion_query 1
#define GL_SAMPLES_PASSED_ARB                               0x8914
#define GL_QUERY_COUNTER_BITS_ARB                           0x8864
#define GL_CURRENT_QUERY_ARB                                0x8865
#define GL_QUERY_RESULT_ARB                                 0x8866
#define GL_QUERY_RESULT_AVAILABLE_ARB                       0x8867
#endif
typedef void (WINE_GLAPI *PGLFNGENQUERIESARBPROC)(GLsizei n, GLuint *queries);
typedef void (WINE_GLAPI *PGLFNDELETEQUERIESARBPROC)(GLsizei n, const GLuint *queries);
typedef GLboolean (WINE_GLAPI *PGLFNISQUERYARBPROC)(GLuint query);
typedef void (WINE_GLAPI *PGLFNBEGINQUERYARBPROC)(GLenum target, GLuint query);
typedef void (WINE_GLAPI *PGLFNENDQUERYARBPROC)(GLenum target);
typedef void (WINE_GLAPI *PGLFNGETQUERYIVARBPROC)(GLenum target, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETQUERYOBJECTIVARBPROC)(GLuint query, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETQUERYOBJECTUIVARBPROC)(GLuint query, GLenum pname, GLuint *params);

/* GL_ARB_pixel_buffer_object */
#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_ARB                            0x88eb
#define GL_PIXEL_UNPACK_BUFFER_ARB                          0x88ec
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB                    0x88ed
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB                  0x88ef
#endif

/* GL_ARB_point_parameters */
#ifndef GL_ARB_point_parameters
#define GL_ARB_point_parameters 1
#define GL_POINT_SIZE_MIN_ARB                               0x8126
#define GL_POINT_SIZE_MAX_ARB                               0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_ARB                    0x8128
#define GL_POINT_DISTANCE_ATTENUATION_ARB                   0x8129
#endif
typedef void (WINE_GLAPI *PGLFNGLPOINTPARAMETERFARBPROC)(GLenum pname, GLfloat param);
typedef void (WINE_GLAPI *PGLFNGLPOINTPARAMETERFVARBPROC)(GLenum pname, const GLfloat *params);

/* GL_ARB_point_sprite */
#ifndef GL_ARB_point_sprite
#define GL_ARB_point_sprite 1
#define GL_POINT_SPRITE_ARB                                 0x8861
#define GL_COORD_REPLACE_ARB                                0x8862
#endif

/* GL_ARB_provoking_vertex */
#ifndef GL_ARB_provoking_vertex
#define GL_ARB_provoking_vertex 1
#define GL_FIRST_VERTEX_CONVENTION                          0x8e4d
#define GL_LAST_VERTEX_CONVENTION                           0x8e4e
#define GL_PROVOKING_VERTEX                                 0x8e4f
#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION         0x8e4c
#endif
typedef void (WINE_GLAPI *PGLFNGLPROVOKINGVERTEXPROC)(GLenum mode);

/* GL_ARB_shader_objects */
#ifndef GL_ARB_shader_objects
#define GL_ARB_shader_objects 1
typedef char GLcharARB;
#ifdef RT_OS_DARWIN
typedef void *GLhandleARB;
#else
typedef unsigned int GLhandleARB;
#endif
#define GL_PROGRAM_OBJECT_ARB                               0x8b40
#define GL_OBJECT_TYPE_ARB                                  0x8b4e
#define GL_OBJECT_SUBTYPE_ARB                               0x8b4f
#define GL_OBJECT_DELETE_STATUS_ARB                         0x8b80
#define GL_OBJECT_COMPILE_STATUS_ARB                        0x8b81
#define GL_OBJECT_LINK_STATUS_ARB                           0x8b82
#define GL_OBJECT_VALIDATE_STATUS_ARB                       0x8b83
#define GL_OBJECT_INFO_LOG_LENGTH_ARB                       0x8b84
#define GL_OBJECT_ATTACHED_OBJECTS_ARB                      0x8b85
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB                       0x8b86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB             0x8b87
#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB                  0x8b88
#define GL_SHADER_OBJECT_ARB                                0x8b48
#define GL_FLOAT_VEC2_ARB                                   0x8b50
#define GL_FLOAT_VEC3_ARB                                   0x8b51
#define GL_FLOAT_VEC4_ARB                                   0x8b52
#define GL_INT_VEC2_ARB                                     0x8b53
#define GL_INT_VEC3_ARB                                     0x8b54
#define GL_INT_VEC4_ARB                                     0x8b55
#define GL_BOOL_ARB                                         0x8b56
#define GL_BOOL_VEC2_ARB                                    0x8b57
#define GL_BOOL_VEC3_ARB                                    0x8b58
#define GL_BOOL_VEC4_ARB                                    0x8b59
#define GL_FLOAT_MAT2_ARB                                   0x8b5a
#define GL_FLOAT_MAT3_ARB                                   0x8b5b
#define GL_FLOAT_MAT4_ARB                                   0x8b5c
#define GL_SAMPLER_1D_ARB                                   0x8b5d
#define GL_SAMPLER_2D_ARB                                   0x8b5e
#define GL_SAMPLER_3D_ARB                                   0x8b5f
#define GL_SAMPLER_CUBE_ARB                                 0x8b60
#define GL_SAMPLER_1D_SHADOW_ARB                            0x8b61
#define GL_SAMPLER_2D_SHADOW_ARB                            0x8b62
#define GL_SAMPLER_2D_RECT_ARB                              0x8b63
#define GL_SAMPELR_2D_RECT_SHADOW_ARB                       0x8b64
#endif

/* GL_ARB_shading_language_100 */
#ifndef GL_ARB_shading_language_100
#define GL_ARB_shading_language_100 1
#define GL_SHADING_LANGUAGE_VERSION_ARB                     0x8b8c
#endif

/* GL_ARB_sync */
#ifndef GL_ARB_sync
#define GL_ARB_sync 1
#define GL_MAX_SERVER_WAIT_TIMEOUT              0x9111
#define GL_OBJECT_TYPE                          0x9112
#define GL_SYNC_CONDITION                       0x9113
#define GL_SYNC_STATUS                          0x9114
#define GL_SYNC_FLAGS                           0x9115
#define GL_SYNC_FENCE                           0x9116
#define GL_SYNC_GPU_COMMANDS_COMPLETE           0x9117
#define GL_UNSIGNALED                           0x9118
#define GL_SIGNALED                             0x9119
#define GL_SYNC_FLUSH_COMMANDS_BIT              0x00000001
#define GL_TIMEOUT_IGNORED                      0xffffffffffffffffULL
#define GL_ALREADY_SIGNALED                     0x911a
#define GL_TIMEOUT_EXPIRED                      0x911b
#define GL_CONDITION_SATISFIED                  0x911c
#define GL_WAIT_FAILED                          0x911d
#endif
typedef GLsync (WINE_GLAPI *PGLFNFENCESYNCPROC)(GLenum condition, GLbitfield flags);
typedef GLboolean (WINE_GLAPI *PGLFNISSYNCPROC)(GLsync sync);
typedef GLvoid (WINE_GLAPI *PGLFNDELETESYNCPROC)(GLsync sync);
typedef GLenum (WINE_GLAPI *PGLFNCLIENTWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef GLvoid (WINE_GLAPI *PGLFNWAITSYNCPROC)(GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef GLvoid (WINE_GLAPI *PGLFNGETINTEGER64VPROC)(GLenum pname, GLint64 *params);
typedef GLvoid (WINE_GLAPI *PGLFNGETSYNCIVPROC)(GLsync sync, GLenum pname, GLsizei bufsize,
        GLsizei *length, GLint *values);

/* GL_ARB_texture_border_clamp */
#ifndef GL_ARB_texture_border_clamp
#define GL_ARB_texture_border_clamp 1
#define GL_CLAMP_TO_BORDER_ARB                              0x812d
#endif

/* GL_ARB_texture_cube_map */
#ifndef GL_ARB_texture_cube_map
#define GL_ARB_texture_cube_map 1
#define GL_NORMAL_MAP_ARB                                   0x8511
#define GL_REFLECTION_MAP_ARB                               0x8512
#define GL_TEXTURE_CUBE_MAP_ARB                             0x8513
#define GL_TEXTURE_BINDING_CUBE_MAP_ARB                     0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB                  0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB                  0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB                  0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB                  0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB                  0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB                  0x851a
#define GL_PROXY_TEXTURE_CUBE_MAP_ARB                       0x851b
#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB                    0x851c
#endif

/* GL_ARB_texture_env_dot3 */
#ifndef GL_ARB_texture_env_dot3
#define GL_ARB_texture_env_dot3 1
#define GL_DOT3_RGB_ARB                                     0x86ae
#define GL_DOT3_RGBA_ARB                                    0x86af
#endif

/* GL_ARB_texture_float */
#ifndef GL_ARB_texture_float
#define GL_ARB_texture_float 1
#define GL_RGBA32F_ARB                                      0x8814
#define GL_RGB32F_ARB                                       0x8815
#define GL_RGBA16F_ARB                                      0x881a
#define GL_RGB16F_ARB                                       0x881b
#endif

/* GL_ARB_texture_mirrored_repeat */
#ifndef GL_ARB_texture_mirrored_repeat
#define GL_ARB_texture_mirrored_repeat 1
#define GL_MIRRORED_REPEAT_ARB                              0x8370
#endif

/* GL_ARB_texture_rectangle */
#ifndef GL_ARB_texture_rectangle
#define GL_ARB_texture_rectangle 1
#define GL_TEXTURE_RECTANGLE_ARB                            0x84f5
#define GL_TEXTURE_BINDING_RECTANGLE_ARB                    0x84f6
#define GL_PROXY_TEXTURE_RECTANGLE_ARB                      0x84f7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB                   0x84f8
#define GL_SAMPLER_2D_RECT_ARB                              0x8b63
#define GL_SAMPLER_2D_RECT_SHADOW_ARB                       0x8b64
#endif

/* GL_ARB_texture_rg */
#ifndef GL_ARB_texture_rg
#define GL_ARB_texture_rg 1
#define GL_RG                                               0x8227
#define GL_RG_INTEGER                                       0x8228
#define GL_R8                                               0x8229
#define GL_R16                                              0x822a
#define GL_RG8                                              0x822b
#define GL_RG16                                             0x822c
#define GL_R16F                                             0x822d
#define GL_R32F                                             0x822e
#define GL_RG16F                                            0x822f
#define GL_RG32F                                            0x8230
#define GL_R8I                                              0x8231
#define GL_R8UI                                             0x8232
#define GL_R16I                                             0x8233
#define GL_R16UI                                            0x8234
#define GL_R32I                                             0x8235
#define GL_R32UI                                            0x8236
#define GL_RG8I                                             0x8237
#define GL_RG8UI                                            0x8238
#define GL_RG16I                                            0x8239
#define GL_RG16UI                                           0x823a
#define GL_RG32I                                            0x823b
#define GL_RG32UI                                           0x823c
#endif

/* GL_ARB_vertex_blend */
#ifndef GL_ARB_vertex_blend
#define GL_ARB_vertex_blend 1
#define GL_MAX_VERTEX_UNITS_ARB                             0x86a4
#define GL_ACTIVE_VERTEX_UNITS_ARB                          0x86a5
#define GL_WEIGHT_SUM_UNITY_ARB                             0x86a6
#define GL_VERTEX_BLEND_ARB                                 0x86a7
#define GL_CURRENT_WEIGHT_ARB                               0x86a8
#define GL_WEIGHT_ARRAY_TYPE_ARB                            0x86a9
#define GL_WEIGHT_ARRAY_STRIDE_ARB                          0x86aa
#define GL_WEIGHT_ARRAY_SIZE_ARB                            0x86ab
#define GL_WEIGHT_ARRAY_POINTER_ARB                         0x86ac
#define GL_WEIGHT_ARRAY_ARB                                 0x86ad
#define GL_MODELVIEW0_ARB                                   0x1700
#define GL_MODELVIEW1_ARB                                   0x850a
#define GL_MODELVIEW2_ARB                                   0x8722
#define GL_MODELVIEW3_ARB                                   0x8723
#define GL_MODELVIEW4_ARB                                   0x8724
#define GL_MODELVIEW5_ARB                                   0x8725
#define GL_MODELVIEW6_ARB                                   0x8726
#define GL_MODELVIEW7_ARB                                   0x8727
#define GL_MODELVIEW8_ARB                                   0x8728
#define GL_MODELVIEW9_ARB                                   0x8729
#define GL_MODELVIEW10_ARB                                  0x872a
#define GL_MODELVIEW11_ARB                                  0x872b
#define GL_MODELVIEW12_ARB                                  0x872c
#define GL_MODELVIEW13_ARB                                  0x872d
#define GL_MODELVIEW14_ARB                                  0x872e
#define GL_MODELVIEW15_ARB                                  0x872f
#define GL_MODELVIEW16_ARB                                  0x8730
#define GL_MODELVIEW17_ARB                                  0x8731
#define GL_MODELVIEW18_ARB                                  0x8732
#define GL_MODELVIEW19_ARB                                  0x8733
#define GL_MODELVIEW20_ARB                                  0x8734
#define GL_MODELVIEW21_ARB                                  0x8735
#define GL_MODELVIEW22_ARB                                  0x8736
#define GL_MODELVIEW23_ARB                                  0x8737
#define GL_MODELVIEW24_ARB                                  0x8738
#define GL_MODELVIEW25_ARB                                  0x8739
#define GL_MODELVIEW26_ARB                                  0x873a
#define GL_MODELVIEW27_ARB                                  0x873b
#define GL_MODELVIEW28_ARB                                  0x873c
#define GL_MODELVIEW29_ARB                                  0x873d
#define GL_MODELVIEW30_ARB                                  0x873e
#define GL_MODELVIEW31_ARB                                  0x873f
#endif
typedef void (WINE_GLAPI *PGLFNGLWEIGHTPOINTERARB)(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTBV)(GLint size, const GLbyte *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTSV)(GLint size, const GLshort *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTIV)(GLint size, const GLint *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTFV)(GLint size, const GLfloat *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTDV)(GLint size, const GLdouble *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTUBV)(GLint size, const GLubyte *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTUSV)(GLint size, const GLushort *weights);
typedef void (WINE_GLAPI *PGLFNGLWEIGHTUIV)(GLint size, const GLuint *weights);
typedef void (WINE_GLAPI *PGLFNGLVERTEXBLENDARB)(GLint count);

/* GL_ARB_vertex_buffer_object */
#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
#define GL_BUFFER_SIZE_ARB                                  0x8764
#define GL_BUFFER_USAGE_ARB                                 0x8765
#define GL_ARRAY_BUFFER_ARB                                 0x8892
#define GL_ELEMENT_ARRAY_BUFFER_ARB                         0x8893
#define GL_ARRAY_BUFFER_BINDING_ARB                         0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB                 0x8895
#define GL_VERTEX_ARRAY_BUFFER_BINDING_ARB                  0x8896
#define GL_NORMAL_ARRAY_BUFFER_BINDING_ARB                  0x8897
#define GL_COLOR_ARRAY_BUFFER_BINDING_ARB                   0x8898
#define GL_INDEX_ARRAY_BUFFER_BINDING_ARB                   0x8899
#define GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB           0x889a
#define GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB               0x889b
#define GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB         0x889c
#define GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB          0x889d
#define GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB                  0x889e
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB           0x889f
#define GL_READ_ONLY_ARB                                    0x88b8
#define GL_WRITE_ONLY_ARB                                   0x88b9
#define GL_READ_WRITE_ARB                                   0x88ba
#define GL_BUFFER_ACCESS_ARB                                0x88bb
#define GL_BUFFER_MAPPED_ARB                                0x88bc
#define GL_BUFFER_MAP_POINTER_ARB                           0x88bd
#define GL_STREAM_DRAW_ARB                                  0x88e0
#define GL_STREAM_READ_ARB                                  0x88e1
#define GL_STREAM_COPY_ARB                                  0x88e2
#define GL_STATIC_DRAW_ARB                                  0x88e4
#define GL_STATIC_READ_ARB                                  0x88e5
#define GL_STATIC_COPY_ARB                                  0x88e6
#define GL_DYNAMIC_DRAW_ARB                                 0x88e8
#define GL_DYNAMIC_READ_ARB                                 0x88e9
#define GL_DYNAMIC_COPY_ARB                                 0x88ea
#endif
typedef void (WINE_GLAPI *PGLFNBINDBUFFERARBPROC)(GLenum target, GLuint buffer);
typedef void (WINE_GLAPI *PGLFNDELETEBUFFERSARBPROC)(GLsizei n, const GLuint *buffers);
typedef void (WINE_GLAPI *PGLFNGENBUFFERSARBPROC)(GLsizei n, GLuint *buffers);
typedef GLboolean (WINE_GLAPI *PGLFNISBUFFERARBPROC)(GLuint buffer);
typedef void (WINE_GLAPI *PGLFNBUFFERDATAARBPROC)(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef void (WINE_GLAPI *PGLFNBUFFERSUBDATAARBPROC)(GLenum target,
        GLintptr offset, GLsizeiptr size, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNGETBUFFERSUBDATAARBPROC)(GLenum target, GLintptr offset, GLsizeiptr size, GLvoid *data);
typedef GLvoid* (WINE_GLAPI *PGLFNMAPBUFFERARBPROC)(GLenum  target, GLenum access);
typedef GLboolean (WINE_GLAPI *PGLFNUNMAPBUFFERARBPROC)(GLenum target);
typedef void (WINE_GLAPI *PGLFNGETBUFFERPARAMETERIVARBPROC)(GLenum target, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETBUFFERPOINTERVARBPROC)(GLenum target, GLenum pname, GLvoid* *params);

/* GL_ARB_vertex_program */
#ifndef GL_ARB_vertex_program
#define GL_ARB_vertex_program 1
#define GL_VERTEX_PROGRAM_ARB                               0x8620
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB                    0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE_ARB                      0x8643
#define GL_COLOR_SUM_ARB                                    0x8458
#define GL_PROGRAM_FORMAT_ASCII_ARB                         0x8875
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB                  0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB                     0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB                   0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB                     0x8625
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB               0x886a
#define GL_CURRENT_VERTEX_ATTRIB_ARB                        0x8626
#define GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB                  0x8645
#define GL_PROGRAM_LENGTH_ARB                               0x8627
#define GL_PROGRAM_FORMAT_ARB                               0x8876
#define GL_PROGRAM_BINDING_ARB                              0x8677
#define GL_PROGRAM_INSTRUCTIONS_ARB                         0x88a0
#define GL_MAX_PROGRAM_INSTRUCTIONS_ARB                     0x88a1
#define GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB                  0x88a2
#define GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB              0x88a3
#define GL_PROGRAM_TEMPORARIES_ARB                          0x88a4
#define GL_MAX_PROGRAM_TEMPORARIES_ARB                      0x88a5
#define GL_PROGRAM_NATIVE_TEMPORARIES_ARB                   0x88a6
#define GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB               0x88a7
#define GL_PROGRAM_PARAMETERS_ARB                           0x88a8
#define GL_MAX_PROGRAM_PARAMETERS_ARB                       0x88a9
#define GL_PROGRAM_NATIVE_PARAMETERS_ARB                    0x88aa
#define GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB                0x88ab
#define GL_PROGRAM_ATTRIBS_ARB                              0x88ac
#define GL_MAX_PROGRAM_ATTRIBS_ARB                          0x88ad
#define GL_PROGRAM_NATIVE_ATTRIBS_ARB                       0x88ae
#define GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB                   0x88af
#define GL_PROGRAM_ADDRESS_REGISTERS_ARB                    0x88b0
#define GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB                0x88b1
#define GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB             0x88b2
#define GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB         0x88b3
#define GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB                 0x88b4
#define GL_MAX_PROGRAM_ENV_PARAMETERS_ARB                   0x88b5
#define GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB                  0x88b6
#define GL_PROGRAM_STRING_ARB                               0x8628
#define GL_PROGRAM_ERROR_POSITION_ARB                       0x864b
#define GL_CURRENT_MATRIX_ARB                               0x8641
#define GL_TRANSPOSE_CURRENT_MATRIX_ARB                     0x88b7
#define GL_CURRENT_MATRIX_STACK_DEPTH_ARB                   0x8640
#define GL_MAX_VERTEX_ATTRIBS_ARB                           0x8869
#define GL_MAX_PROGRAM_MATRICES_ARB                         0x862f
#define GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB               0x862e
#define GL_PROGRAM_ERROR_STRING_ARB                         0x8874
#define GL_MATRIX0_ARB                                      0x88c0
#define GL_MATRIX1_ARB                                      0x88c1
#define GL_MATRIX2_ARB                                      0x88c2
#define GL_MATRIX3_ARB                                      0x88c3
#define GL_MATRIX4_ARB                                      0x88c4
#define GL_MATRIX5_ARB                                      0x88c5
#define GL_MATRIX6_ARB                                      0x88c6
#define GL_MATRIX7_ARB                                      0x88c7
#define GL_MATRIX8_ARB                                      0x88c8
#define GL_MATRIX9_ARB                                      0x88c9
#define GL_MATRIX10_ARB                                     0x88ca
#define GL_MATRIX11_ARB                                     0x88cb
#define GL_MATRIX12_ARB                                     0x88cc
#define GL_MATRIX13_ARB                                     0x88cd
#define GL_MATRIX14_ARB                                     0x88ce
#define GL_MATRIX15_ARB                                     0x88cf
#define GL_MATRIX16_ARB                                     0x88d0
#define GL_MATRIX17_ARB                                     0x88d1
#define GL_MATRIX18_ARB                                     0x88d2
#define GL_MATRIX19_ARB                                     0x88d3
#define GL_MATRIX20_ARB                                     0x88d4
#define GL_MATRIX21_ARB                                     0x88d5
#define GL_MATRIX22_ARB                                     0x88d6
#define GL_MATRIX23_ARB                                     0x88d7
#define GL_MATRIX24_ARB                                     0x88d8
#define GL_MATRIX25_ARB                                     0x88d9
#define GL_MATRIX26_ARB                                     0x88da
#define GL_MATRIX27_ARB                                     0x88db
#define GL_MATRIX28_ARB                                     0x88dc
#define GL_MATRIX29_ARB                                     0x88dd
#define GL_MATRIX30_ARB                                     0x88de
#define GL_MATRIX31_ARB                                     0x88df
#endif
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1DARBPROC)(GLuint index, GLdouble x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1FARBPROC)(GLuint index, GLfloat x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1SARBPROC)(GLuint index, GLshort x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1SVARBPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2DARBPROC)(GLuint index, GLdouble x, GLdouble y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2FARBPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2SARBPROC)(GLuint index, GLshort x, GLshort y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2SVARBPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3DARBPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3FARBPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3SARBPROC)(GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3SVARBPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NBVARBPROC)(GLuint index, const GLbyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NIVARBPROC)(GLuint index, const GLint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NSVARBPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUBARBPROC)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUBVARBPROC)(GLuint index, const GLubyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUIVARBPROC)(GLuint index, const GLuint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4NUSVARBPROC)(GLuint index, const GLushort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4BVARBPROC)(GLuint index, const GLbyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4DARBPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4FARBPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4IVARBPROC)(GLuint index, const GLint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4SARBPROC)(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4SVARBPROC)(GLuint index, const GLshort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4UBVARBPROC)(GLuint index, const GLubyte *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4UIVARBPROC)(GLuint index, const GLuint *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4USVARBPROC)(GLuint index, const GLushort *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBPOINTERARBPROC)(GLuint index, GLint size,
        GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (WINE_GLAPI *PGLFNENABLEVERTEXATTRIBARRAYARBPROC)(GLuint index);
typedef void (WINE_GLAPI *PGLFNDISABLEVERTEXATTRIBARRAYARBPROC)(GLuint index);
typedef void (WINE_GLAPI *PGLFNPROGRAMSTRINGARBPROC)(GLenum target, GLenum format, GLsizei len, const GLvoid *string);
typedef void (WINE_GLAPI *PGLFNBINDPROGRAMARBPROC)(GLenum target, GLuint program);
typedef void (WINE_GLAPI *PGLFNDELETEPROGRAMSARBPROC)(GLsizei n, const GLuint *programs);
typedef void (WINE_GLAPI *PGLFNGENPROGRAMSARBPROC)(GLsizei n, GLuint *programs);
typedef void (WINE_GLAPI *PGLFNPROGRAMENVPARAMETER4DARBPROC)(GLenum target,
        GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (WINE_GLAPI *PGLFNPROGRAMENVPARAMETER4DVARBPROC)(GLenum target, GLuint index, const GLdouble *params);
typedef void (WINE_GLAPI *PGLFNPROGRAMENVPARAMETER4FARBPROC)(GLenum target,
        GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (WINE_GLAPI *PGLFNPROGRAMENVPARAMETER4FVARBPROC)(GLenum target, GLuint index, const GLfloat *params);
typedef void (WINE_GLAPI *PGLFNPROGRAMLOCALPARAMETER4DARBPROC)(GLenum target,
        GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (WINE_GLAPI *PGLFNPROGRAMLOCALPARAMETER4DVARBPROC)(GLenum target, GLuint index, const GLdouble *params);
typedef void (WINE_GLAPI *PGLFNPROGRAMLOCALPARAMETER4FARBPROC)(GLenum target,
        GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (WINE_GLAPI *PGLFNPROGRAMLOCALPARAMETER4FVARBPROC)(GLenum target, GLuint index, const GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMENVPARAMETERDVARBPROC)(GLenum target, GLuint index, GLdouble *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMENVPARAMETERFVARBPROC)(GLenum target, GLuint index, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMLOCALPARAMETERDVARBPROC)(GLenum target, GLuint index, GLdouble *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMLOCALPARAMETERFVARBPROC)(GLenum target, GLuint index, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMIVARBPROC)(GLenum target, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETPROGRAMSTRINGARBPROC)(GLenum target, GLenum pname, GLvoid *string);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBDVARBPROC)(GLuint index, GLenum pname, GLdouble *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBFVARBPROC)(GLuint index, GLenum pname, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBIVARBPROC)(GLuint index, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETVERTEXATTRIBPOINTERVARBPROC)(GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (WINE_GLAPI *PGLFNISPROGRAMARBPROC)(GLuint program);

/* GL_ARB_vertex_shader */
#ifndef GL_ARB_vertex_shader
#define GL_ARB_vertex_shader 1
#define GL_VERTEX_SHADER_ARB                                0x8b31
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB                0x8b4a
#define GL_MAX_VARYING_FLOATS_ARB                           0x8b4b
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB               0x8b4c
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB             0x8b4d
#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB                     0x8b89
#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB           0x8b8a
#endif
typedef void (WINE_GLAPI *WINED3D_PFNGLGETOBJECTPARAMETERIVARBPROC)(GLhandleARB obj, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETOBJECTPARAMETERFVARBPROC)(GLhandleARB obj, GLenum pname, GLfloat *params);
typedef GLint (WINE_GLAPI *WINED3D_PFNGLGETUNIFORMLOCATIONARBPROC)(GLhandleARB programObj, const GLcharARB *name);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETACTIVEUNIFORMARBPROC)(GLhandleARB programObj, GLuint index,
        GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM1IARBPROC)(GLint location, GLint v0);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM2IARBPROC)(GLint location, GLint v0, GLint v1);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM3IARBPROC)(GLint location, GLint v0, GLint v1, GLint v2);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM4IARBPROC)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM1FARBPROC)(GLint location, GLfloat v0);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM2FARBPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM3FARBPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM4FARBPROC)(GLint location,
        GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM1IVARBPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM2IVARBPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM3IVARBPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM4IVARBPROC)(GLint location, GLsizei count, const GLint *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM1FVARBPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM2FVARBPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM3FVARBPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORM4FVARBPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORMMATRIX2FVARBPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORMMATRIX3FVARBPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLUNIFORMMATRIX4FVARBPROC)(GLint location,
        GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETUNIFORMFVARBPROC)(GLhandleARB programObj, GLint location, GLfloat *params);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETUNIFORMIVARBPROC)(GLhandleARB programObj, GLint location, GLint *params);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETINFOLOGARBPROC)(GLhandleARB obj,
        GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
typedef void (WINE_GLAPI *WINED3D_PFNGLUSEPROGRAMOBJECTARBPROC)(GLhandleARB programObj);
typedef GLhandleARB (WINE_GLAPI *WINED3D_PFNGLCREATESHADEROBJECTARBPROC)(GLenum shaderType);
typedef void (WINE_GLAPI *WINED3D_PFNGLSHADERSOURCEARBPROC)(GLhandleARB shaderObj,
        GLsizei count, const GLcharARB* *string, const GLint *length);
typedef void (WINE_GLAPI *WINED3D_PFNGLCOMPILESHADERARBPROC)(GLhandleARB shaderObj);
typedef GLhandleARB (WINE_GLAPI *WINED3D_PFNGLCREATEPROGRAMOBJECTARBPROC)(void);
typedef void (WINE_GLAPI *WINED3D_PFNGLATTACHOBJECTARBPROC)(GLhandleARB containerObj, GLhandleARB obj);
typedef void (WINE_GLAPI *WINED3D_PFNGLLINKPROGRAMARBPROC)(GLhandleARB programObj);
typedef void (WINE_GLAPI *WINED3D_PFNGLDETACHOBJECTARBPROC)(GLhandleARB containerObj, GLhandleARB attachedObj);
typedef void (WINE_GLAPI *WINED3D_PFNGLDELETEOBJECTARBPROC)(GLhandleARB obj);
typedef void (WINE_GLAPI *WINED3D_PFNGLVALIDATEPROGRAMARBPROC)(GLhandleARB programObj);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETATTACHEDOBJECTSARBPROC)(GLhandleARB containerObj,
        GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
typedef GLhandleARB (WINE_GLAPI *WINED3D_PFNGLGETHANDLEARBPROC)(GLenum pname);
typedef void (WINE_GLAPI *WINED3D_PFNGLGETSHADERSOURCEARBPROC)(GLhandleARB obj,
        GLsizei maxLength, GLsizei *length, GLcharARB *source);
typedef void (WINE_GLAPI *WINED3D_PFNGLBINDATTRIBLOCATIONARBPROC)(GLhandleARB programObj,
        GLuint index, const GLcharARB *name);
typedef GLint (WINE_GLAPI *WINED3D_PFNGLGETATTRIBLOCATIONARBPROC)(GLhandleARB programObj, const GLcharARB *name);

/* GL_ATI_fragment_shader */
#ifndef GL_ATI_fragment_shader
#define GL_ATI_fragment_shader 1
#define GL_FRAGMENT_SHADER_ATI                              0x8920
#define GL_REG_0_ATI                                        0x8921
#define GL_REG_1_ATI                                        0x8922
#define GL_REG_2_ATI                                        0x8923
#define GL_REG_3_ATI                                        0x8924
#define GL_REG_4_ATI                                        0x8925
#define GL_REG_5_ATI                                        0x8926
#define GL_CON_0_ATI                                        0x8941
#define GL_CON_1_ATI                                        0x8942
#define GL_CON_2_ATI                                        0x8943
#define GL_CON_3_ATI                                        0x8944
#define GL_CON_4_ATI                                        0x8945
#define GL_CON_5_ATI                                        0x8946
#define GL_CON_6_ATI                                        0x8947
#define GL_CON_7_ATI                                        0x8948
#define GL_MOV_ATI                                          0x8961
#define GL_ADD_ATI                                          0x8963
#define GL_MUL_ATI                                          0x8964
#define GL_SUB_ATI                                          0x8965
#define GL_DOT3_ATI                                         0x8966
#define GL_DOT4_ATI                                         0x8967
#define GL_MAD_ATI                                          0x8968
#define GL_LERP_ATI                                         0x8969
#define GL_CND_ATI                                          0x896a
#define GL_CND0_ATI                                         0x896b
#define GL_DOT2_ADD_ATI                                     0x896c
#define GL_SECONDARY_INTERPOLATOR_ATI                       0x896d
#define GL_SWIZZLE_STR_ATI                                  0x8976
#define GL_SWIZZLE_STQ_ATI                                  0x8977
#define GL_SWIZZLE_STR_DR_ATI                               0x8978
#define GL_SWIZZLE_STQ_DQ_ATI                               0x8979
#define GL_RED_BIT_ATI                                      0x00000001
#define GL_GREEN_BIT_ATI                                    0x00000002
#define GL_BLUE_BIT_ATI                                     0x00000004
#define GL_2X_BIT_ATI                                       0x00000001
#define GL_4X_BIT_ATI                                       0x00000002
#define GL_8X_BIT_ATI                                       0x00000004
#define GL_HALF_BIT_ATI                                     0x00000008
#define GL_QUARTER_BIT_ATI                                  0x00000010
#define GL_EIGHTH_BIT_ATI                                   0x00000020
#define GL_SATURATE_BIT_ATI                                 0x00000040
#define GL_COMP_BIT_ATI                                     0x00000002
#define GL_NEGATE_BIT_ATI                                   0x00000004
#define GL_BIAS_BIT_ATI                                     0x00000008
#endif
typedef GLuint (WINE_GLAPI *PGLFNGENFRAGMENTSHADERSATI)(GLuint range);
typedef void (WINE_GLAPI *PGLFNBINDFRAGMENTSHADERATI)(GLuint id);
typedef void (WINE_GLAPI *PGLFNDELETEFRAGMENTSHADERATI)(GLuint id);
typedef void (WINE_GLAPI *PGLFNBEGINFRAGMENTSHADERATI)(void);
typedef void (WINE_GLAPI *PGLFNENDFRAGMENTSHADERATI)(void);
typedef void (WINE_GLAPI *PGLFNPASSTEXCOORDATI)(GLuint dst, GLuint coord, GLenum swizzle);
typedef void (WINE_GLAPI *PGLFNSAMPLEMAPATI)(GLuint dst, GLuint interp, GLenum swizzle);
typedef void (WINE_GLAPI *PGLFNCOLORFRAGMENTOP1ATI)(GLenum op, GLuint dst, GLuint dstMask,
        GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (WINE_GLAPI *PGLFNCOLORFRAGMENTOP2ATI)(GLenum op, GLuint dst, GLuint dstMask,
        GLuint dstMod, GLuint arg1, GLuint arg1Rep,
        GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
        GLuint arg2Mod);
typedef void (WINE_GLAPI *PGLFNCOLORFRAGMENTOP3ATI)(GLenum op, GLuint dst, GLuint dstMask,
        GLuint dstMod, GLuint arg1, GLuint arg1Rep,
        GLuint arg1Mod, GLuint arg2, GLuint arg2Rep,
        GLuint arg2Mod, GLuint arg3, GLuint arg3Rep,
        GLuint arg3Mod);
typedef void (WINE_GLAPI *PGLFNALPHAFRAGMENTOP1ATI)(GLenum op, GLuint dst, GLuint dstMod,
        GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (WINE_GLAPI *PGLFNALPHAFRAGMENTOP2ATI)(GLenum op, GLuint dst, GLuint dstMod,
        GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
        GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef void (WINE_GLAPI *PGLFNALPHAFRAGMENTOP3ATI)(GLenum op, GLuint dst, GLuint dstMod,
        GLuint arg1, GLuint arg1Rep, GLuint arg1Mod,
        GLuint arg2, GLuint arg2Rep, GLuint arg2Mod,
        GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef void (WINE_GLAPI *PGLFNSETFRAGMENTSHADERCONSTANTATI)(GLuint dst, const GLfloat *value);

/* GL_ATI_separate_stencil */
#ifndef GL_ATI_separate_stencil
#define GL_ATI_separate_stencil 1
#define GL_STENCIL_BACK_FUNC_ATI                            0x8800
#define GL_STENCIL_BACK_FAIL_ATI                            0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL_ATI                 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS_ATI                 0x8803
#endif
typedef void (WINE_GLAPI *PGLFNSTENCILOPSEPARATEATIPROC)(GLenum, GLenum, GLenum, GLenum);
typedef void (WINE_GLAPI *PGLFNSTENCILFUNCSEPARATEATIPROC)(GLenum, GLenum, GLint, GLuint);

/* GL_ATI_texture_compression_3dc */
#ifndef GL_ATI_texture_compression_3dc
#define GL_ATI_texture_compression_3dc 1
#define GL_COMPRESSED_LUMINANCE_ALPHA_3DC_ATI               0x8837
#endif

/* GL_ATI_texture_env_combine3 */
#ifndef GL_ATI_texture_env_combine3
#define GL_ATI_texture_env_combine3 1
#define GL_MODULATE_ADD_ATI                                 0x8744
#define GL_MODULATE_SIGNED_ADD_ATI                          0x8745
#define GL_MODULATE_SUBTRACT_ATI                            0x8746
/* #define ONE */
/* #define ZERO */
#endif

/* GL_ATI_texture_mirror_once */
#ifndef GL_ATI_texture_mirror_once
#define GL_ATI_texture_mirror_once 1
#define GL_MIRROR_CLAMP_ATI                                 0x8742
#define GL_MIRROR_CLAMP_TO_EDGE_ATI                         0x8743
#endif

/* GL_EXT_blend_equation_separate */
typedef void (WINE_GLAPI *PGLFNBLENDEQUATIONSEPARATEEXTPROC)(GLenum modeRGB, GLenum modeAlpha);

/* GL_EXT_blend_func_separate */
#ifndef GL_EXT_blend_func_separate
#define GL_EXT_blend_func_separate 1
#define GL_BLEND_DST_RGB_EXT                                0x80c8
#define GL_BLEND_SRC_RGB_EXT                                0x80c9
#define GL_BLEND_DST_ALPHA_EXT                              0x80ca
#define GL_BLEND_SRC_ALPHA_EXT                              0x80cb
#endif
typedef void (WINE_GLAPI *PGLFNBLENDFUNCSEPARATEEXTPROC)(GLenum sfactorRGB, GLenum dfactorRGB,
        GLenum sfactorAlpha, GLenum dfactorAlpha);

/* GL_EXT_draw_buffers2 */
typedef GLvoid (WINE_GLAPI *PGLFNCOLORMASKINDEXEDEXTPROC)(GLuint buffer_idx, GLboolean r, GLboolean g,
        GLboolean b, GLboolean a);
typedef GLvoid (WINE_GLAPI *PGLFNGETBOOLEANINDEXEDVEXTPROC)(GLenum param, GLuint index, GLboolean *value);
typedef GLvoid (WINE_GLAPI *PGLFNGETINTEGERINDEXEDVEXTPROC)(GLenum param, GLuint index, GLint *value);
typedef GLvoid (WINE_GLAPI *PGLFNENABLEINDEXEDEXTPROC)(GLenum target, GLuint index);
typedef GLvoid (WINE_GLAPI *PGLFNDISABLEINDEXEDEXTPROC)(GLenum target, GLuint index);
typedef GLboolean (WINE_GLAPI *PGLFNISENABLEDINDEXEDEXTPROC)(GLenum target, GLuint index);

/* GL_EXT_fog_coord */
#ifndef GL_EXT_fog_coord
#define GL_EXT_fog_coord 1
#define GL_FOG_COORDINATE_SOURCE_EXT                        0x8450
#define GL_FOG_COORDINATE_EXT                               0x8451
#define GL_FRAGMENT_DEPTH_EXT                               0x8452
#define GL_CURRENT_FOG_COORDINATE_EXT                       0x8453
#define GL_FOG_COORDINATE_ARRAY_TYPE_EXT                    0x8454
#define GL_FOG_COORDINATE_ARRAY_STRIDE_EXT                  0x8455
#define GL_FOG_COORDINATE_ARRAY_POINTER_EXT                 0x8456
#define GL_FOG_COORDINATE_ARRAY_EXT                         0x8457
#endif
typedef void (WINE_GLAPI *PGLFNGLFOGCOORDFEXTPROC)(GLfloat coord);
typedef void (WINE_GLAPI *PGLFNGLFOGCOORDFVEXTPROC)(const GLfloat *coord);
typedef void (WINE_GLAPI *PGLFNGLFOGCOORDDEXTPROC)(GLdouble coord);
typedef void (WINE_GLAPI *PGLFNGLFOGCOORDDVEXTPROC)(const GLdouble *coord);
typedef void (WINE_GLAPI *PGLFNGLFOGCOORDPOINTEREXTPROC)(GLenum type, GLsizei stride, GLvoid *data);

/* GL_EXT_framebuffer_blit */
#ifndef GL_EXT_framebuffer_blit
#define GL_EXT_framebuffer_blit 1
#define GL_READ_FRAMEBUFFER_EXT                             0x8ca8
#define GL_DRAW_FRAMEBUFFER_EXT                             0x8ca9
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT                     0x8ca6
#define GL_READ_FRAMEBUFFER_BINDING_EXT                     0x8caa
#endif
typedef void (WINE_GLAPI *PGLFNGLBLITFRAMEBUFFEREXTPROC)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
        GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

/* GL_EXT_framebuffer_multisample */
#ifndef GL_EXT_framebuffer_multisample
#define GL_EXT_framebuffer_multisample 1
#define GL_RENDERBUFFER_SAMPLES_EXT                         0x8cab
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT           0x8d56
#define GL_MAX_SAMPLES_EXT                                  0x8d57
#endif
typedef void (WINE_GLAPI *PGLFNRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(GLenum target, GLsizei samples,
        GLenum internalformat, GLsizei width, GLsizei height);

/* GL_EXT_framebuffer_object */
#ifndef GL_EXT_framebuffer_object
#define GL_EXT_framebuffer_object 1
#define GL_FRAMEBUFFER_EXT                                  0x8d40
#define GL_RENDERBUFFER_EXT                                 0x8d41
#define GL_STENCIL_INDEX1_EXT                               0x8d46
#define GL_STENCIL_INDEX4_EXT                               0x8d47
#define GL_STENCIL_INDEX8_EXT                               0x8d48
#define GL_STENCIL_INDEX16_EXT                              0x8d49
#define GL_RENDERBUFFER_WIDTH_EXT                           0x8d42
#define GL_RENDERBUFFER_HEIGHT_EXT                          0x8d43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT                 0x8d44
#define GL_RENDERBUFFER_RED_SIZE_EXT                        0x8d50
#define GL_RENDERBUFFER_GREEN_SIZE_EXT                      0x8d51
#define GL_RENDERBUFFER_BLUE_SIZE_EXT                       0x8d52
#define GL_RENDERBUFFER_ALPHA_SIZE_EXT                      0x8d53
#define GL_RENDERBUFFER_DEPTH_SIZE_EXT                      0x8d54
#define GL_RENDERBUFFER_STENCIL_SIZE_EXT                    0x8d55
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT           0x8cd0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT           0x8cd1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT         0x8cd2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8cd3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT    0x8cd4
#define GL_COLOR_ATTACHMENT0_EXT                            0x8ce0
#define GL_COLOR_ATTACHMENT1_EXT                            0x8ce1
#define GL_COLOR_ATTACHMENT2_EXT                            0x8ce2
#define GL_COLOR_ATTACHMENT3_EXT                            0x8ce3
#define GL_COLOR_ATTACHMENT4_EXT                            0x8ce4
#define GL_COLOR_ATTACHMENT5_EXT                            0x8ce5
#define GL_COLOR_ATTACHMENT6_EXT                            0x8ce6
#define GL_COLOR_ATTACHMENT7_EXT                            0x8ce7
#define GL_COLOR_ATTACHMENT8_EXT                            0x8ce8
#define GL_COLOR_ATTACHMENT9_EXT                            0x8ce9
#define GL_COLOR_ATTACHMENT10_EXT                           0x8cea
#define GL_COLOR_ATTACHMENT11_EXT                           0x8ceb
#define GL_COLOR_ATTACHMENT12_EXT                           0x8cec
#define GL_COLOR_ATTACHMENT13_EXT                           0x8ced
#define GL_COLOR_ATTACHMENT14_EXT                           0x8cee
#define GL_COLOR_ATTACHMENT15_EXT                           0x8cef
#define GL_DEPTH_ATTACHMENT_EXT                             0x8d00
#define GL_STENCIL_ATTACHMENT_EXT                           0x8d20
#define GL_FRAMEBUFFER_COMPLETE_EXT                         0x8cd5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT            0x8cd6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT    0x8cd7
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT            0x8cd9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT               0x8cda
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT           0x8cdb
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT           0x8cdc
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT                      0x8cdd
#define GL_FRAMEBUFFER_BINDING_EXT                          0x8ca6
#define GL_RENDERBUFFER_BINDING_EXT                         0x8ca7
#define GL_MAX_COLOR_ATTACHMENTS_EXT                        0x8cdF
#define GL_MAX_RENDERBUFFER_SIZE_EXT                        0x84e8
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT                0x0506
#endif
typedef GLboolean (WINE_GLAPI *PGLFNGLISRENDERBUFFEREXTPROC)(GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLBINDRENDERBUFFEREXTPROC)(GLenum target, GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLDELETERENDERBUFFERSEXTPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (WINE_GLAPI *PGLFNGLGENRENDERBUFFERSEXTPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (WINE_GLAPI *PGLFNGLRENDERBUFFERSTORAGEEXTPROC)(GLenum target,
        GLenum internalformat, GLsizei width, GLsizei height);
typedef void (WINE_GLAPI *PGLFNGLGETRENDERBUFFERPARAMETERIVEXTPROC)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean (WINE_GLAPI *PGLFNGLISFRAMEBUFFEREXTPROC)(GLuint framebuffer);
typedef void (WINE_GLAPI *PGLFNGLBINDFRAMEBUFFEREXTPROC)(GLenum target, GLuint framebuffer);
typedef void (WINE_GLAPI *PGLFNGLDELETEFRAMEBUFFERSEXTPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (WINE_GLAPI *PGLFNGLGENFRAMEBUFFERSEXTPROC)(GLsizei n, GLuint *framebuffers);
typedef GLenum (WINE_GLAPI *PGLFNGLCHECKFRAMEBUFFERSTATUSEXTPROC)(GLenum target);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE1DEXTPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE2DEXTPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERTEXTURE3DEXTPROC)(GLenum target, GLenum attachment,
        GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (WINE_GLAPI *PGLFNGLFRAMEBUFFERRENDERBUFFEREXTPROC)(GLenum target, GLenum attachment,
        GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (WINE_GLAPI *PGLFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC)(GLenum target,
        GLenum attachment, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGLGENERATEMIPMAPEXTPROC)(GLenum target);

/* GL_EXT_gpu_program_parameters */
#ifndef GL_EXT_gpu_program_parameters
#define GL_EXT_gpu_program_parameters 1
typedef void (WINE_GLAPI *PGLFNPROGRAMENVPARAMETERS4FVEXTPROC)(GLenum target,
        GLuint index, GLsizei count, const float *params);
typedef void (WINE_GLAPI *PGLFNPROGRAMLOCALPARAMETERS4FVEXTPROC)(GLenum target,
        GLuint index, GLsizei count, const float *params);
#endif

/* GL_EXT_gpu_shader4 */
#ifndef GL_EXT_gpu_shader4
#define GL_EXT_gpu_shader4 1
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER_EXT                  0x88fd
#define GL_SAMPLER_1D_ARRAY_EXT                             0x8dc0
#define GL_SAMPLER_2D_ARRAY_EXT                             0x8dc1
#define GL_SAMPLER_BUFFER_EXT                               0x8dc2
#define GL_SAMPLER_1D_ARRAY_SHADOW_EXT                      0x8dc3
#define GL_SAMPLER_2D_ARRAY_SHADOW_EXT                      0x8dc4
#define GL_SAMPLER_CUBE_SHADOW_EXT                          0x8dc5
#define GL_UNSIGNED_INT_VEC2_EXT                            0x8dc6
#define GL_UNSIGNED_INT_VEC3_EXT                            0x8dc7
#define GL_UNSIGNED_INT_VEC4_EXT                            0x8dc8
#define GL_INT_SAMPLER_1D_EXT                               0x8dc9
#define GL_INT_SAMPLER_2D_EXT                               0x8dca
#define GL_INT_SAMPLER_3D_EXT                               0x8dcb
#define GL_INT_SAMPLER_CUBE_EXT                             0x8dcc
#define GL_INT_SAMPLER_2D_RECT_EXT                          0x8dcd
#define GL_INT_SAMPLER_1D_ARRAY_EXT                         0x8dce
#define GL_INT_SAMPLER_2D_ARRAY_EXT                         0x8dcf
#define GL_INT_SAMPLER_BUFFER_EXT                           0x8dd0
#define GL_UNSIGNED_INT_SAMPLER_1D_EXT                      0x8dd1
#define GL_UNSIGNED_INT_SAMPLER_2D_EXT                      0x8dd2
#define GL_UNSIGNED_INT_SAMPLER_3D_EXT                      0x8dd3
#define GL_UNSIGNED_INT_SAMPLER_CUBE_EXT                    0x8dd4
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT_EXT                 0x8dd5
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY_EXT                0x8dd6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY_EXT                0x8dd7
#define GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT                  0x8dd8
#define GL_MIN_PROGRAM_TEXEL_OFFSET_EXT                     0x8904
#define GL_MAX_PROGRAM_TEXEL_OFFSET_EXT                     0x8905
#endif
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI1IEXTPROC)(GLuint index, GLint x);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI2IEXTPROC)(GLuint index, GLint x, GLint y);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI3IEXTPROC)(GLuint index, GLint x, GLint y, GLint z);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4IEXTPROC)(GLuint index, GLint x, GLint y, GLint z, GLint w);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI1UIEXTPROC)(GLuint index, GLuint x);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI2UIEXTPROC)(GLuint index, GLuint x, GLuint y);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI3UIEXTPROC)(GLuint index, GLuint x, GLuint y, GLuint z);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4UIEXTPROC)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI1IVEXTPROC)(GLuint index, const GLint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI2IVEXTPROC)(GLuint index, const GLint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI3IVEXTPROC)(GLuint index, const GLint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4IVEXTPROC)(GLuint index, const GLint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI1UIVEXTPROC)(GLuint index, const GLuint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI2UIVEXTPROC)(GLuint index, const GLuint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI3UIVEXTPROC)(GLuint index, const GLuint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4UIVEXTPROC)(GLuint index, const GLuint *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4BVEXTPROC)(GLuint index, const GLbyte *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4SVEXTPROC)(GLuint index, const GLshort *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4UBVEXTPROC)(GLuint index, const GLubyte *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBI4USVEXTPROC)(GLuint index, const GLushort *v);
typedef GLvoid (WINE_GLAPI *PGLFNVERTEXATTRIBIPOINTEREXTPROC)(GLuint index, GLint size, GLenum type,
        GLsizei stride, const GLvoid *pointer);
typedef GLvoid (WINE_GLAPI *PGLFNGETVERTEXATTRIBIIVEXTPROC)(GLuint index, GLenum pname, GLint *params);
typedef GLvoid (WINE_GLAPI *PGLFNGETVERTEXATTRIBIUIVEXTPROC)(GLuint index, GLenum pname, GLuint *params);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM1UIEXTPROC)(GLint location, GLuint v0);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM2UIEXTPROC)(GLint location, GLuint v0, GLuint v1);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM3UIEXTPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM4UIEXTPROC)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM1UIVEXTPROC)(GLint location, GLsizei count, const GLuint *value);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM2UIVEXTPROC)(GLint location, GLsizei count, const GLuint *value);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM3UIVEXTPROC)(GLint location, GLsizei count, const GLuint *value);
typedef GLvoid (WINE_GLAPI *PGLFNUNIFORM4UIVEXTPROC)(GLint location, GLsizei count, const GLuint *value);
typedef GLvoid (WINE_GLAPI *PGLFNGETUNIFORMUIVEXTPROC)(GLuint program, GLint location, const GLuint *params);
typedef GLvoid (WINE_GLAPI *PGLFNBINDFRAGDATALOCATIONEXTPROC)(GLuint program, GLuint color_number, const GLchar *name);
typedef GLint (WINE_GLAPI *PGLFNGETFRAGDATALOCATIONEXTPROC)(GLuint program, const GLchar *name);

/* GL_EXT_packed_depth_stencil */
#ifndef GL_EXT_packed_depth_stencil
#define GL_EXT_packed_depth_stencil 1
#define GL_DEPTH_STENCIL_EXT                                0x84f9
#define GL_UNSIGNED_INT_24_8_EXT                            0x84fa
#define GL_DEPTH24_STENCIL8_EXT                             0x88f0
#define GL_TEXTURE_STENCIL_SIZE_EXT                         0x88f1
#endif

/* GL_EXT_paletted_texture */
#ifndef GL_EXT_paletted_texture
#define GL_EXT_paletted_texture 1
#define GL_COLOR_INDEX1_EXT                                 0x80e2
#define GL_COLOR_INDEX2_EXT                                 0x80e3
#define GL_COLOR_INDEX4_EXT                                 0x80e4
#define GL_COLOR_INDEX8_EXT                                 0x80e5
#define GL_COLOR_INDEX12_EXT                                0x80e6
#define GL_COLOR_INDEX16_EXT                                0x80e7
#define GL_TEXTURE_INDEX_SIZE_EXT                           0x80ed
#endif
typedef void (WINE_GLAPI *PGLFNGLCOLORTABLEEXTPROC)(GLenum target, GLenum internalFormat,
        GLsizei width, GLenum format, GLenum type, const GLvoid *table);

/* GL_EXT_point_parameters */
#ifndef GL_EXT_point_parameters
#define GL_EXT_point_parameters 1
#define GL_POINT_SIZE_MIN_EXT                               0x8126
#define GL_POINT_SIZE_MAX_EXT                               0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT                    0x8128
#define GL_DISTANCE_ATTENUATION_EXT                         0x8129
#endif
typedef void (WINE_GLAPI *PGLFNGLPOINTPARAMETERFEXTPROC)(GLenum pname, GLfloat param);
typedef void (WINE_GLAPI *PGLFNGLPOINTPARAMETERFVEXTPROC)(GLenum pname, const GLfloat *params);

/* GL_EXT_provoking_vertex */
#ifndef GL_EXT_provoking_vertex
#define GL_EXT_provoking_vertex 1
#define GL_FIRST_VERTEX_CONVENTION_EXT                      0x8e4d
#define GL_LAST_VERTEX_CONVENTION_EXT                       0x8e4e
#define GL_PROVOKING_VERTEX_EXT                             0x8e4f
#define GL_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION_EXT     0x8e4c
#endif
typedef void (WINE_GLAPI *PGLFNGLPROVOKINGVERTEXEXTPROC)(GLenum mode);

/* GL_EXT_secondary_color */
#ifndef GL_EXT_secondary_color
#define GL_EXT_secondary_color 1
#define GL_COLOR_SUM_EXT                                    0x8458
#define GL_CURRENT_SECONDARY_COLOR_EXT                      0x8459
#define GL_SECONDARY_COLOR_ARRAY_SIZE_EXT                   0x845a
#define GL_SECONDARY_COLOR_ARRAY_TYPE_EXT                   0x845b
#define GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT                 0x845c
#define GL_SECONDARY_COLOR_ARRAY_POINTER_EXT                0x845d
#define GL_SECONDARY_COLOR_ARRAY_EXT                        0x845e
#endif
typedef void (WINE_GLAPI *PGLFNGLSECONDARYCOLOR3FEXTPROC)(GLfloat red, GLfloat green, GLfloat blue);
typedef void (WINE_GLAPI *PGLFNGLSECONDARYCOLOR3FVEXTPROC)(const GLfloat *v);
typedef void (WINE_GLAPI *PGLFNGLSECONDARYCOLOR3UBEXTPROC)(GLubyte red, GLubyte green, GLubyte blue);
typedef void (WINE_GLAPI *PGLFNGLSECONDARYCOLOR3UBVEXTPROC)(const GLubyte *v);
typedef void (WINE_GLAPI *PGLFNGLSECONDARYCOLORPOINTEREXTPROC)(GLint size, GLenum type,
        GLsizei stride, const GLvoid *pointer);

/* GL_EXT_stencil_two_side */
#ifndef GL_EXT_stencil_two_side
#define GL_EXT_stencil_two_side 1
#define GL_STENCIL_TEST_TWO_SIDE_EXT                        0x8910
#define GL_ACTIVE_STENCIL_FACE_EXT                          0x8911
#endif
typedef void (WINE_GLAPI *PGLFNACTIVESTENCILFACEEXTPROC)(GLenum face);

/* GL_EXT_stencil_wrap */
#ifndef GL_EXT_stencil_wrap
#define GL_EXT_stencil_wrap 1
#define GL_INCR_WRAP_EXT                                    0x8507
#define GL_DECR_WRAP_EXT                                    0x8508
#endif

/* GL_EXT_texture3D */
#ifndef GL_EXT_texture3D
#define GL_EXT_texture3D 1
#define GL_PACK_SKIP_IMAGES_EXT                             0x806b
#define GL_PACK_IMAGE_HEIGHT_EXT                            0x806c
#define GL_UNPACK_SKIP_IMAGES_EXT                           0x806d
#define GL_UNPACK_IMAGE_HEIGHT_EXT                          0x806e
#define GL_TEXTURE_3D_EXT                                   0x806f
#define GL_PROXY_TEXTURE_3D_EXT                             0x8070
#define GL_TEXTURE_DEPTH_EXT                                0x8071
#define GL_TEXTURE_WRAP_R_EXT                               0x8072
#define GL_MAX_3D_TEXTURE_SIZE_EXT                          0x8073
#endif
typedef void (WINE_GLAPI *PGLFNGLTEXIMAGE3DEXTPROC)(GLenum target, GLint level, GLenum internalformat,
        GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (WINE_GLAPI *PGLFNGLTEXSUBIMAGE3DEXTPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

/* GL_EXT_texture_compression_rgtc */
#ifndef GL_EXT_texture_compression_rgtc
#define GL_EXT_texture_compression_rgtc 1
#define GL_COMPRESSED_RED_RGTC1_EXT                         0x8dbb
#define GL_COMPRESSED_SIGNED_RED_RGTC1_EXT                  0x8dbc
#define GL_COMPRESSED_RED_GREEN_RGTC2_EXT                   0x8dbd
#define GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT            0x8dbe
#endif

/* GL_EXT_texture_compression_s3tc */
#ifndef GL_EXT_texture_compression_s3tc
#define GL_EXT_texture_compression_s3tc 1
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                     0x83f0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                    0x83f1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                    0x83f2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                    0x83f3
#endif
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXIMAGE3DPROC)(GLenum target, GLint level, GLenum internalformat,
        GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat,
        GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXIMAGE1DPROC)(GLenum target, GLint level, GLenum internalformat,
        GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXSUBIMAGE3DPROC)(GLenum target, GLint level,
        GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
        GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
        GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNCOMPRESSEDTEXSUBIMAGE1DPROC)(GLenum target, GLint level, GLint xoffset,
        GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
typedef void (WINE_GLAPI *PGLFNGETCOMPRESSEDTEXIMAGEPROC)(GLenum target, GLint level, void *img);

/* GL_EXT_texture_env_combine */
#ifndef GL_EXT_texture_env_combine
#define GL_EXT_texture_env_combine 1
#define GL_COMBINE_EXT                                      0x8570
#define GL_COMBINE_RGB_EXT                                  0x8571
#define GL_COMBINE_ALPHA_EXT                                0x8572
#define GL_RGB_SCALE_EXT                                    0x8573
#define GL_ADD_SIGNED_EXT                                   0x8574
#define GL_INTERPOLATE_EXT                                  0x8575
#define GL_SUBTRACT_EXT                                     0x84e7
#define GL_CONSTANT_EXT                                     0x8576
#define GL_PRIMARY_COLOR_EXT                                0x8577
#define GL_PREVIOUS_EXT                                     0x8578
#define GL_SOURCE0_RGB_EXT                                  0x8580
#define GL_SOURCE1_RGB_EXT                                  0x8581
#define GL_SOURCE2_RGB_EXT                                  0x8582
#define GL_SOURCE3_RGB_EXT                                  0x8583
#define GL_SOURCE4_RGB_EXT                                  0x8584
#define GL_SOURCE5_RGB_EXT                                  0x8585
#define GL_SOURCE6_RGB_EXT                                  0x8586
#define GL_SOURCE7_RGB_EXT                                  0x8587
#define GL_SOURCE0_ALPHA_EXT                                0x8588
#define GL_SOURCE1_ALPHA_EXT                                0x8589
#define GL_SOURCE2_ALPHA_EXT                                0x858a
#define GL_SOURCE3_ALPHA_EXT                                0x858b
#define GL_SOURCE4_ALPHA_EXT                                0x858c
#define GL_SOURCE5_ALPHA_EXT                                0x858d
#define GL_SOURCE6_ALPHA_EXT                                0x858e
#define GL_SOURCE7_ALPHA_EXT                                0x858f
#define GL_OPERAND0_RGB_EXT                                 0x8590
#define GL_OPERAND1_RGB_EXT                                 0x8591
#define GL_OPERAND2_RGB_EXT                                 0x8592
#define GL_OPERAND3_RGB_EXT                                 0x8593
#define GL_OPERAND4_RGB_EXT                                 0x8594
#define GL_OPERAND5_RGB_EXT                                 0x8595
#define GL_OPERAND6_RGB_EXT                                 0x8596
#define GL_OPERAND7_RGB_EXT                                 0x8597
#define GL_OPERAND0_ALPHA_EXT                               0x8598
#define GL_OPERAND1_ALPHA_EXT                               0x8599
#define GL_OPERAND2_ALPHA_EXT                               0x859a
#define GL_OPERAND3_ALPHA_EXT                               0x859b
#define GL_OPERAND4_ALPHA_EXT                               0x859c
#define GL_OPERAND5_ALPHA_EXT                               0x859d
#define GL_OPERAND6_ALPHA_EXT                               0x859e
#define GL_OPERAND7_ALPHA_EXT                               0x859f
#endif

/* GL_EXT_texture_env_dot3 */
#ifndef GL_EXT_texture_env_dot3
#define GL_EXT_texture_env_dot3 1
#define GL_DOT3_RGB_EXT                                     0x8740
#define GL_DOT3_RGBA_EXT                                    0x8741
#endif

/* GL_EXT_texture_filter_anisotropic */
#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic 1
#define GL_TEXTURE_MAX_ANISOTROPY_EXT                       0x84fe
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT                   0x84ff
#endif

/* GL_EXT_texture_lod_bias */
#ifndef GL_EXT_texture_lod_bias
#define GL_EXT_texture_lod_bias 1
#define GL_MAX_TEXTURE_LOD_BIAS_EXT                         0x84fd
#define GL_TEXTURE_FILTER_CONTROL_EXT                       0x8500
#define GL_TEXTURE_LOD_BIAS_EXT                             0x8501
#endif

/* GL_EXT_texture_sRGB */
#ifndef GL_EXT_texture_sRGB
#define GL_EXT_texture_sRGB 1
#define GL_SRGB_EXT                                         0x8c40
#define GL_SRGB8_EXT                                        0x8c41
#define GL_SRGB_ALPHA_EXT                                   0x8c42
#define GL_SRGB8_ALPHA8_EXT                                 0x8c43
#define GL_SLUMINANCE_ALPHA_EXT                             0x8c44
#define GL_SLUMINANCE8_ALPHA8_EXT                           0x8c45
#define GL_SLUMINANCE_EXT                                   0x8c46
#define GL_SLUMINANCE8_EXT                                  0x8c47
#define GL_COMPRESSED_SRGB_EXT                              0x8c48
#define GL_COMPRESSED_SRGB_ALPHA_EXT                        0x8c49
#define GL_COMPRESSED_SLUMINANCE_EXT                        0x8c4a
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT                  0x8c4b
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT                    0x8c4c
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT              0x8c4d
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT              0x8c4e
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT              0x8c4f
#endif

/* GL_NV_depth_clamp */
#ifndef GL_NV_depth_clamp
#define GL_NV_depth_clamp 1
#define GL_DEPTH_CLAMP_NV                                   0x864f
#endif

/* GL_NV_fence */
#ifndef GL_NV_fence
#define GL_NV_fence 1
#define GL_ALL_COMPLETED_NV                                 0x84f2
#define GL_FENCE_STATUS_NV                                  0x84f3
#define GL_FENCE_CONDITION_NV                               0x84f4
#endif
typedef void (WINE_GLAPI *PGLFNGENFENCESNVPROC)(GLsizei, GLuint *);
typedef void (WINE_GLAPI *PGLFNDELETEFENCESNVPROC)(GLuint, const GLuint *);
typedef void (WINE_GLAPI *PGLFNSETFENCENVPROC)(GLuint, GLenum);
typedef GLboolean (WINE_GLAPI *PGLFNTESTFENCENVPROC)(GLuint);
typedef void (WINE_GLAPI *PGLFNFINISHFENCENVPROC)(GLuint);
typedef GLboolean (WINE_GLAPI *PGLFNISFENCENVPROC)(GLuint);
typedef void (WINE_GLAPI *PGLFNGETFENCEIVNVPROC)(GLuint, GLenum, GLint *);

/* GL_NV_fog_distance */
#ifndef GL_NV_fog_distance
#define GL_NV_fog_distance 1
#define GL_FOG_DISTANCE_MODE_NV                             0x855a
#define GL_EYE_RADIAL_NV                                    0x855b
#define GL_EYE_PLANE_ABSOLUTE_NV                            0x855c
/* reuse GL_EYE_PLANE */
#endif

/* GL_NV_half_float */
#ifndef GL_NV_half_float
#define GL_NV_half_float 1
typedef unsigned short GLhalfNV;
#define GL_HALF_FLOAT_NV                                    0x140b
#endif
typedef void (WINE_GLAPI *PGLFNVERTEX2HNVPROC)(GLhalfNV x, GLhalfNV y);
typedef void (WINE_GLAPI *PGLFNVERTEX2HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEX3HNVPROC)(GLhalfNV x, GLhalfNV y, GLhalfNV z);
typedef void (WINE_GLAPI *PGLFNVERTEX3HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEX4HNVPROC)(GLhalfNV x, GLhalfNV y, GLhalfNV z, GLhalfNV w);
typedef void (WINE_GLAPI *PGLFNVERTEX4HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNNORMAL3HNVPROC)(GLhalfNV nx, GLhalfNV ny, GLhalfNV nz);
typedef void (WINE_GLAPI *PGLFNNORMAL3HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNCOLOR3HNVPROC)(GLhalfNV red, GLhalfNV green, GLhalfNV blue);
typedef void (WINE_GLAPI *PGLFNCOLOR3HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNCOLOR4HNVPROC)(GLhalfNV red, GLhalfNV green, GLhalfNV blue, GLhalfNV alpha);
typedef void (WINE_GLAPI *PGLFNCOLOR4HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNTEXCOORD1HNVPROC)(GLhalfNV s);
typedef void (WINE_GLAPI *PGLFNTEXCOORD1HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNTEXCOORD2HNVPROC)(GLhalfNV s, GLhalfNV t);
typedef void (WINE_GLAPI *PGLFNTEXCOORD2HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNTEXCOORD3HNVPROC)(GLhalfNV s, GLhalfNV t, GLhalfNV r);
typedef void (WINE_GLAPI *PGLFNTEXCOORD3HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNTEXCOORD4HNVPROC)(GLhalfNV s, GLhalfNV t, GLhalfNV r, GLhalfNV q);
typedef void (WINE_GLAPI *PGLFNTEXCOORD4HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD1HNVPROC)(GLenum target, GLhalfNV s);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD1HVNVPROC)(GLenum target, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD2HNVPROC)(GLenum target, GLhalfNV s, GLhalfNV t);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD2HVNVPROC)(GLenum target, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD3HNVPROC)(GLenum target, GLhalfNV s, GLhalfNV t, GLhalfNV r);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD3HVNVPROC)(GLenum target, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD4HNVPROC)(GLenum target, GLhalfNV s, GLhalfNV t, GLhalfNV r, GLhalfNV q);
typedef void (WINE_GLAPI *PGLFNMULTITEXCOORD4HVNVPROC)(GLenum target, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNFOGCOORDHNVPROC)(GLhalfNV fog);
typedef void (WINE_GLAPI *PGLFNFOGCOORDHVNVPROC)(const GLhalfNV *fog);
typedef void (WINE_GLAPI *PGLFNSECONDARYCOLOR3HNVPROC)(GLhalfNV red, GLhalfNV green, GLhalfNV blue);
typedef void (WINE_GLAPI *PGLFNSECONDARYCOLOR3HVNVPROC)(const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXWEIGHTHNVPROC)(GLhalfNV weight);
typedef void (WINE_GLAPI *PGLFNVERTEXWEIGHTHVNVPROC)(const GLhalfNV *weight);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1HNVPROC)(GLuint index, GLhalfNV x);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB1HVNVPROC)(GLuint index, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2HNVPROC)(GLuint index, GLhalfNV x, GLhalfNV y);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB2HVNVPROC)(GLuint index, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3HNVPROC)(GLuint index, GLhalfNV x, GLhalfNV y, GLhalfNV z);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB3HVNVPROC)(GLuint index, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4HNVPROC)(GLuint index, GLhalfNV x, GLhalfNV y, GLhalfNV z, GLhalfNV w);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIB4HVNVPROC)(GLuint index, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBS1HVNVPROC)(GLuint index, GLsizei n, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBS2HVNVPROC)(GLuint index, GLsizei n, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBS3HVNVPROC)(GLuint index, GLsizei n, const GLhalfNV *v);
typedef void (WINE_GLAPI *PGLFNVERTEXATTRIBS4HVNVPROC)(GLuint index, GLsizei n, const GLhalfNV *v);

/* GL_NV_light_max_exponent */
#ifndef GL_NV_light_max_exponent
#define GL_NV_light_max_exponent 1
#define GL_MAX_SHININESS_NV                                 0x8504
#define GL_MAX_SPOT_EXPONENT_NV                             0x8505
#endif

/* GL_NV_register_combiners */
#ifndef GL_NV_register_combiners
#define GL_NV_register_combiners 1
#define GL_REGISTER_COMBINERS_NV                            0x8522
#define GL_VARIABLE_A_NV                                    0x8523
#define GL_VARIABLE_B_NV                                    0x8524
#define GL_VARIABLE_C_NV                                    0x8525
#define GL_VARIABLE_D_NV                                    0x8526
#define GL_VARIABLE_E_NV                                    0x8527
#define GL_VARIABLE_F_NV                                    0x8528
#define GL_VARIABLE_G_NV                                    0x8529
#define GL_CONSTANT_COLOR0_NV                               0x852a
#define GL_CONSTANT_COLOR1_NV                               0x852b
#define GL_PRIMARY_COLOR_NV                                 0x852c
#define GL_SECONDARY_COLOR_NV                               0x852d
#define GL_SPARE0_NV                                        0x852e
#define GL_SPARE1_NV                                        0x852f
#define GL_DISCARD_NV                                       0x8530
#define GL_E_TIMES_F_NV                                     0x8531
#define GL_SPARE0_PLUS_SECONDARY_COLOR_NV                   0x8532
#define GL_UNSIGNED_IDENTITY_NV                             0x8536
#define GL_UNSIGNED_INVERT_NV                               0x8537
#define GL_EXPAND_NORMAL_NV                                 0x8538
#define GL_EXPAND_NEGATE_NV                                 0x8539
#define GL_HALF_BIAS_NORMAL_NV                              0x853a
#define GL_HALF_BIAS_NEGATE_NV                              0x853b
#define GL_SIGNED_IDENTITY_NV                               0x853c
#define GL_SIGNED_NEGATE_NV                                 0x853d
#define GL_SCALE_BY_TWO_NV                                  0x853e
#define GL_SCALE_BY_FOUR_NV                                 0x853f
#define GL_SCALE_BY_ONE_HALF_NV                             0x8540
#define GL_BIAS_BY_NEGATIVE_ONE_HALF_NV                     0x8541
#define GL_COMBINER_INPUT_NV                                0x8542
#define GL_COMBINER_MAPPING_NV                              0x8543
#define GL_COMBINER_COMPONENT_USAGE_NV                      0x8544
#define GL_COMBINER_AB_DOT_PRODUCT_NV                       0x8545
#define GL_COMBINER_CD_DOT_PRODUCT_NV                       0x8546
#define GL_COMBINER_MUX_SUM_NV                              0x8547
#define GL_COMBINER_SCALE_NV                                0x8548
#define GL_COMBINER_BIAS_NV                                 0x8549
#define GL_COMBINER_AB_OUTPUT_NV                            0x854a
#define GL_COMBINER_CD_OUTPUT_NV                            0x854b
#define GL_COMBINER_SUM_OUTPUT_NV                           0x854c
#define GL_MAX_GENERAL_COMBINERS_NV                         0x854d
#define GL_NUM_GENERAL_COMBINERS_NV                         0x854e
#define GL_COLOR_SUM_CLAMP_NV                               0x854f
#define GL_COMBINER0_NV                                     0x8550
#define GL_COMBINER1_NV                                     0x8551
#define GL_COMBINER2_NV                                     0x8552
#define GL_COMBINER3_NV                                     0x8553
#define GL_COMBINER4_NV                                     0x8554
#define GL_COMBINER5_NV                                     0x8555
#define GL_COMBINER6_NV                                     0x8556
#define GL_COMBINER7_NV                                     0x8557
/* reuse GL_TEXTURE0_ARB */
/* reuse GL_TEXTURE1_ARB */
/* reuse GL_ZERO */
/* reuse GL_NONE */
/* reuse GL_FOG */
#endif
typedef void (WINE_GLAPI *PGLFNCOMBINERPARAMETERFVNVPROC)(GLenum pname, const GLfloat *params);
typedef void (WINE_GLAPI *PGLFNCOMBINERPARAMETERFNVPROC)(GLenum pname, GLfloat param);
typedef void (WINE_GLAPI *PGLFNCOMBINERPARAMETERIVNVPROC)(GLenum pname, const GLint *params);
typedef void (WINE_GLAPI *PGLFNCOMBINERPARAMETERINVPROC)(GLenum pname, GLint param);
typedef void (WINE_GLAPI *PGLFNCOMBINERINPUTNVPROC)(GLenum stage, GLenum portion,
        GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (WINE_GLAPI *PGLFNCOMBINEROUTPUTNVPROC)(GLenum stage, GLenum portion,
        GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias,
        GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum);
typedef void (WINE_GLAPI *PGLFNFINALCOMBINERINPUTNVPROC)(GLenum variable, GLenum input,
        GLenum mapping, GLenum componentUsage);
typedef void (WINE_GLAPI *PGLFNGETCOMBINERINPUTPARAMETERFVNVPROC)(GLenum stage, GLenum portion,
        GLenum variable, GLenum pname, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETCOMBINERINPUTPARAMETERIVNVPROC)(GLenum stage, GLenum portion,
        GLenum variable, GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETCOMBINEROUTPUTPARAMETERFVNVPROC)(GLenum stage, GLenum portion,
        GLenum pname, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETCOMBINEROUTPUTPARAMETERIVNVPROC)(GLenum stage, GLenum portion,
        GLenum pname, GLint *params);
typedef void (WINE_GLAPI *PGLFNGETFINALCOMBINERINPUTPARAMETERFVNVPROC)(GLenum variable, GLenum pname, GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETFINALCOMBINERINPUTPARAMETERIVNVPROC)(GLenum variable, GLenum pname, GLint *params);

/* GL_NV_register_combiners2 */
#ifndef GL_NV_register_combiners2
#define GL_NV_register_combiners2 1
#define GL_PER_STAGE_CONSTANTS_NV                           0x8535
#endif
typedef void (WINE_GLAPI *PGLFNCOMBINERSTAGEPARAMETERFVNVPROC)(GLenum stage, GLenum pname, const GLfloat *params);
typedef void (WINE_GLAPI *PGLFNGETCOMBINERSTAGEPARAMETERFVNVPROC)(GLenum stage, GLenum pname, GLfloat *params);

/* GL_NV_texgen_reflection */
#ifndef GL_NV_texgen_reflection
#define GL_NV_texgen_reflection 1
#define GL_NORMAL_MAP_NV                                    0x8511
#define GL_REFLECTION_MAP_NV                                0x8512
#endif

/* GL_NV_texture_env_combine4 */
#ifndef GL_NV_texture_env_combine4
#define GL_NV_texture_env_combine4 1
#define GL_COMBINE4_NV                                      0x8503
#define GL_SOURCE3_RGB_NV                                   0x8583
#define GL_SOURCE3_ALPHA_NV                                 0x858b
#define GL_OPERAND3_RGB_NV                                  0x8593
#define GL_OPERAND3_ALPHA_NV                                0x859b
#endif

/* GL_NV_texture_shader */
#ifndef GL_NV_texture_shader
#define GL_NV_texture_shader 1
#define GL_OFFSET_TEXTURE_RECTANGLE_NV                      0x864c
#define GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV                0x864d
#define GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV                 0x864e
#define GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV             0x86d9
#define GL_UNSIGNED_INT_S8_S8_8_8_NV                        0x86da
#define GL_UNSIGNED_INT_8_8_S8_S8_REV_NV                    0x86db
#define GL_DSDT_MAG_INTENSITY_NV                            0x86dc
#define GL_SHADER_CONSISTENT_NV                             0x86dd
#define GL_TEXTURE_SHADER_NV                                0x86de
#define GL_SHADER_OPERATION_NV                              0x86df
#define GL_CULL_MODES_NV                                    0x86e0
#define GL_OFFSET_TEXTURE_MATRIX_NV                         0x86e1
#define GL_OFFSET_TEXTURE_SCALE_NV                          0x86e2
#define GL_OFFSET_TEXTURE_BIAS_NV                           0x86e3
#define GL_OFFSET_TEXTURE_2D_MATRIX_NV                      GL_OFFSET_TEXTURE_MATRIX_NV
#define GL_OFFSET_TEXTURE_2D_SCALE_NV                       GL_OFFSET_TEXTURE_SCALE_NV
#define GL_OFFSET_TEXTURE_2D_BIAS_NV                        GL_OFFSET_TEXTURE_BIAS_NV
#define GL_PREVIOUS_TEXTURE_INPUT_NV                        0x86e4
#define GL_CONST_EYE_NV                                     0x86e5
#define GL_PASS_THROUGH_NV                                  0x86e6
#define GL_CULL_FRAGMENT_NV                                 0x86e7
#define GL_OFFSET_TEXTURE_2D_NV                             0x86e8
#define GL_DEPENDENT_AR_TEXTURE_2D_NV                       0x86e9
#define GL_DEPENDENT_GB_TEXTURE_2D_NV                       0x86ea
#define GL_DOT_PRODUCT_NV                                   0x86ec
#define GL_DOT_PRODUCT_DEPTH_REPLACE_NV                     0x86ed
#define GL_DOT_PRODUCT_TEXTURE_2D_NV                        0x86ee
#define GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV                  0x86f0
#define GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV                  0x86f1
#define GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV                  0x86f2
#define GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV        0x86f3
#define GL_HILO_NV                                          0x86f4
#define GL_DSDT_NV                                          0x86f5
#define GL_DSDT_MAG_NV                                      0x86f6
#define GL_DSDT_MAG_VIB_NV                                  0x86f7
#define GL_HILO16_NV                                        0x86f8
#define GL_SIGNED_HILO_NV                                   0x86f9
#define GL_SIGNED_HILO16_NV                                 0x86fa
#define GL_SIGNED_RGBA_NV                                   0x86fb
#define GL_SIGNED_RGBA8_NV                                  0x86fc
#define GL_SIGNED_RGB_NV                                    0x86fe
#define GL_SIGNED_RGB8_NV                                   0x86ff
#define GL_SIGNED_LUMINANCE_NV                              0x8701
#define GL_SIGNED_LUMINANCE8_NV                             0x8702
#define GL_SIGNED_LUMINANCE_ALPHA_NV                        0x8703
#define GL_SIGNED_LUMINANCE8_ALPHA8_NV                      0x8704
#define GL_SIGNED_ALPHA_NV                                  0x8705
#define GL_SIGNED_ALPHA8_NV                                 0x8706
#define GL_SIGNED_INTENSITY_NV                              0x8707
#define GL_SIGNED_INTENSITY8_NV                             0x8708
#define GL_DSDT8_NV                                         0x8709
#define GL_DSDT8_MAG8_NV                                    0x870a
#define GL_DSDT8_MAG8_INTENSITY8_NV                         0x870b
#define GL_SIGNED_RGB_UNSIGNED_ALPHA_NV                     0x870c
#define GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV                   0x870d
#define GL_HI_SCALE_NV                                      0x870e
#define GL_LO_SCALE_NV                                      0x870f
#define GL_DS_SCALE_NV                                      0x8710
#define GL_DT_SCALE_NV                                      0x8711
#define GL_MAGNITUDE_SCALE_NV                               0x8712
#define GL_VIBRANCE_SCALE_NV                                0x8713
#define GL_HI_BIAS_NV                                       0x8714
#define GL_LO_BIAS_NV                                       0x8715
#define GL_DS_BIAS_NV                                       0x8716
#define GL_DT_BIAS_NV                                       0x8717
#define GL_MAGNITUDE_BIAS_NV                                0x8718
#define GL_VIBRANCE_BIAS_NV                                 0x8719
#define GL_TEXTURE_BORDER_VALUES_NV                         0x871a
#define GL_TEXTURE_HI_SIZE_NV                               0x871b
#define GL_TEXTURE_LO_SIZE_NV                               0x871c
#define GL_TEXTURE_DS_SIZE_NV                               0x871d
#define GL_TEXTURE_DT_SIZE_NV                               0x871e
#define GL_TEXTURE_MAG_SIZE_NV                              0x871f
#endif

/* GL_NV_texture_shader2 */
#ifndef GL_NV_texture_shader2
#define GL_NV_texture_shader2 1
#define GL_DOT_PRODUCT_TEXTURE_3D_NV                        0x86ef
#endif

/* GL_NV_vertex_program2_option */
#ifndef GL_NV_vertex_program2_option
#define GL_NV_vertex_program2_option 1
#define GL_MAX_PROGRAM_EXEC_INSTRUCTIONS_NV                 0x88f4
#define GL_MAX_PROGRAM_CALL_DEPTH_NV                        0x88f5
#endif

/* GL_SGIS_generate_mipmap */
#ifndef GLX_SGIS_generate_mipmap
#define GLX_SGIS_generate_mipmap 1
#define GL_GENERATE_MIPMAP_SGIS                             0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS                        0x8192
#endif

/* GLX_SGI_video_sync */
typedef int (WINE_GLAPI *PGLXFNGETVIDEOSYNCSGIPROC)(unsigned int *);
typedef int (WINE_GLAPI *PGLXFNWAITVIDEOSYNCSGIPROC)(int, int, unsigned int *);

/* WGL_ARB_extensions_string */
typedef const char *(WINAPI *WINED3D_PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);

/* WGL_ARB_multisample */
#ifndef WGL_ARB_multisample
#define WGL_ARB_multisample 1
#define WGL_SAMPLE_BUFFERS_ARB                              0x2041
#define WGL_SAMPLES_ARB                                     0x2042
#endif

/* WGL_ARB_pixel_format */
#ifndef WGL_ARB_pixel_format
#define WGL_ARB_pixel_format 1
#define WGL_NUMBER_PIXEL_FORMATS_ARB                        0x2000
#define WGL_DRAW_TO_WINDOW_ARB                              0x2001
#define WGL_DRAW_TO_BITMAP_ARB                              0x2002
#define WGL_ACCELERATION_ARB                                0x2003
#define WGL_NEED_PALETTE_ARB                                0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB                         0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB                          0x2006
#define WGL_SWAP_METHOD_ARB                                 0x2007
#define WGL_NUMBER_OVERLAYS_ARB                             0x2008
#define WGL_NUMBER_UNDERLAYS_ARB                            0x2009
#define WGL_TRANSPARENT_ARB                                 0x200a
#define WGL_TRANSPARENT_RED_VALUE_ARB                       0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB                     0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB                      0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB                     0x203a
#define WGL_TRANSPARENT_INDEX_VALUE_ARB                     0x203b
#define WGL_SHARE_DEPTH_ARB                                 0x200c
#define WGL_SHARE_STENCIL_ARB                               0x200d
#define WGL_SHARE_ACCUM_ARB                                 0x200e
#define WGL_SUPPORT_GDI_ARB                                 0x200f
#define WGL_SUPPORT_OPENGL_ARB                              0x2010
#define WGL_DOUBLE_BUFFER_ARB                               0x2011
#define WGL_STEREO_ARB                                      0x2012
#define WGL_PIXEL_TYPE_ARB                                  0x2013
#define WGL_COLOR_BITS_ARB                                  0x2014
#define WGL_RED_BITS_ARB                                    0x2015
#define WGL_RED_SHIFT_ARB                                   0x2016
#define WGL_GREEN_BITS_ARB                                  0x2017
#define WGL_GREEN_SHIFT_ARB                                 0x2018
#define WGL_BLUE_BITS_ARB                                   0x2019
#define WGL_BLUE_SHIFT_ARB                                  0x201a
#define WGL_ALPHA_BITS_ARB                                  0x201b
#define WGL_ALPHA_SHIFT_ARB                                 0x201c
#define WGL_ACCUM_BITS_ARB                                  0x201d
#define WGL_ACCUM_RED_BITS_ARB                              0x201e
#define WGL_ACCUM_GREEN_BITS_ARB                            0x201f
#define WGL_ACCUM_BLUE_BITS_ARB                             0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB                            0x2021
#define WGL_DEPTH_BITS_ARB                                  0x2022
#define WGL_STENCIL_BITS_ARB                                0x2023
#define WGL_AUX_BUFFERS_ARB                                 0x2024
#define WGL_NO_ACCELERATION_ARB                             0x2025
#define WGL_GENERIC_ACCELERATION_ARB                        0x2026
#define WGL_FULL_ACCELERATION_ARB                           0x2027
#define WGL_SWAP_EXCHANGE_ARB                               0x2028
#define WGL_SWAP_COPY_ARB                                   0x2029
#define WGL_SWAP_UNDEFINED_ARB                              0x202a
#define WGL_TYPE_RGBA_ARB                                   0x202b
#define WGL_TYPE_COLORINDEX_ARB                             0x202c
#endif
typedef BOOL (WINAPI *WINED3D_PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat,
        int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues);
typedef BOOL (WINAPI *WINED3D_PFNWGLGETPIXELFORMATATTRIBFVARBPROC)(HDC hdc, int iPixelFormat,
        int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues);
typedef BOOL (WINAPI *WINED3D_PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int *piAttribIList,
        const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);

/* WGL_ARB_pixel_format_float */
#ifndef WGL_ARB_pixel_format_float
#define WGL_ARB_pixel_format_float 1
#define WGL_TYPE_RGBA_FLOAT_ARB                             0x21a0
#endif

/* WGL_WINE_pixel_format_passthrough */
typedef BOOL (WINAPI *WINED3D_PFNWGLSETPIXELFORMATWINE)(HDC hdc, int iPixelFormat,
        const PIXELFORMATDESCRIPTOR *ppfd);

/*Global resource ids sharing*/
#define GL_SHARE_CONTEXT_RESOURCES_CR 0x8B27
/*do flush for the command buffer of a thread the context was previusly current for*/
#define GL_FLUSH_ON_THREAD_SWITCH_CR  0x8B28
/*report that the shared resource is used by this context, the parameter value is a texture name*/
#define GL_RCUSAGE_TEXTURE_SET_CR     0x8B29
/*report that the shared resource is no longer used by this context, the parameter value is a texture name*/
#define GL_RCUSAGE_TEXTURE_CLEAR_CR   0x8B2A
/*configures host to create windows initially hidden*/
#define GL_HOST_WND_CREATED_HIDDEN    0x8B2B

typedef void (WINE_GLAPI *PGLFNCHROMIUMPARAMETERUCR)(GLenum param, GLint value);

# define VBOXWDDM_GL_EXT_FUNCS_GEN \
        USE_GL_FUNC(PGLFNCHROMIUMPARAMETERUCR, \
                glChromiumParameteriCR,                VBOX_SHARED_CONTEXTS,            NULL) \


#define GL_EXT_FUNCS_GEN \
    /* GL_APPLE_fence */ \
    USE_GL_FUNC(PGLFNGENFENCESAPPLEPROC, \
            glGenFencesAPPLE,                           APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNDELETEFENCESAPPLEPROC, \
            glDeleteFencesAPPLE,                        APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNSETFENCEAPPLEPROC, \
            glSetFenceAPPLE,                            APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNTESTFENCEAPPLEPROC, \
            glTestFenceAPPLE,                           APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNFINISHFENCEAPPLEPROC, \
            glFinishFenceAPPLE,                         APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNISFENCEAPPLEPROC, \
            glIsFenceAPPLE,                             APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNTESTOBJECTAPPLEPROC, \
            glTestObjectAPPLE,                          APPLE_FENCE,                    NULL) \
    USE_GL_FUNC(PGLFNFINISHOBJECTAPPLEPROC, \
            glFinishObjectAPPLE,                        APPLE_FENCE,                    NULL) \
    /* GL_APPLE_flush_buffer_range */ \
    USE_GL_FUNC(PGLFNBUFFERPARAMETERIAPPLE, \
            glBufferParameteriAPPLE,                    APPLE_FLUSH_BUFFER_RANGE,       NULL) \
    USE_GL_FUNC(PGLFNFLUSHMAPPEDBUFFERRANGEAPPLE, \
            glFlushMappedBufferRangeAPPLE,              APPLE_FLUSH_BUFFER_RANGE,       NULL) \
    /* GL_APPLE_flush_render */ \
    USE_GL_FUNC(PGLFNFLUSHRENDERAPPLEPROC, \
            glFlushRenderAPPLE,                         APPLE_FLUSH_RENDER,             NULL) \
    USE_GL_FUNC(PGLFNFINISHRENDERAPPLEPROC, \
            glFinishRenderAPPLE,                        APPLE_FLUSH_RENDER,             NULL) \
    /* GL_ARB_color_buffer_float */ \
    USE_GL_FUNC(PGLFNCLAMPCOLORARBPROC, \
            glClampColorARB,                            ARB_COLOR_BUFFER_FLOAT,         NULL) \
    /* GL_ARB_draw_buffers */ \
    USE_GL_FUNC(PGLFNDRAWBUFFERSARBPROC, \
            glDrawBuffersARB,                           ARB_DRAW_BUFFERS,               NULL) \
    /* GL_ARB_framebuffer_object */ \
    USE_GL_FUNC(PGLFNGLISRENDERBUFFERPROC, \
            glIsRenderbuffer,                           ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLBINDRENDERBUFFERPROC, \
            glBindRenderbuffer,                         ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLDELETERENDERBUFFERSPROC, \
            glDeleteRenderbuffers,                      ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENRENDERBUFFERSPROC, \
            glGenRenderbuffers,                         ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLRENDERBUFFERSTORAGEPROC, \
            glRenderbufferStorage,                      ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNRENDERBUFFERSTORAGEMULTISAMPLEPROC, \
            glRenderbufferStorageMultisample,           ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGETRENDERBUFFERPARAMETERIVPROC, \
            glGetRenderbufferParameteriv,               ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLISFRAMEBUFFERPROC, \
            glIsFramebuffer,                            ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLBINDFRAMEBUFFERPROC, \
            glBindFramebuffer,                          ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLDELETEFRAMEBUFFERSPROC, \
            glDeleteFramebuffers,                       ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENFRAMEBUFFERSPROC, \
            glGenFramebuffers,                          ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLCHECKFRAMEBUFFERSTATUSPROC, \
            glCheckFramebufferStatus,                   ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE1DPROC, \
            glFramebufferTexture1D,                     ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE2DPROC, \
            glFramebufferTexture2D,                     ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE3DPROC, \
            glFramebufferTexture3D,                     ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURELAYERPROC,     \
            glFramebufferTextureLayer,                  ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERRENDERBUFFERPROC, \
            glFramebufferRenderbuffer,                  ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC, \
            glGetFramebufferAttachmentParameteriv,      ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLBLITFRAMEBUFFERPROC, \
            glBlitFramebuffer,                          ARB_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENERATEMIPMAPPROC, \
            glGenerateMipmap,                           ARB_FRAMEBUFFER_OBJECT,         NULL) \
    /* GL_ARB_geometry_shader4 */ \
    USE_GL_FUNC(PGLFNPROGRAMPARAMETERIARBPROC, \
            glProgramParameteriARB,                     ARB_GEOMETRY_SHADER4,           NULL) \
    USE_GL_FUNC(PGLFNFRAMEBUFFERTEXTUREARBPROC, \
            glFramebufferTextureARB,                    ARB_GEOMETRY_SHADER4,           NULL) \
    USE_GL_FUNC(PGLFNFRAMEBUFFERTEXTURELAYERARBPROC, \
            glFramebufferTextureLayerARB,               ARB_GEOMETRY_SHADER4,           NULL) \
    USE_GL_FUNC(PGLFNFRAMEBUFFERTEXTUREFACEARBPROC, \
            glFramebufferTextureFaceARB,                ARB_GEOMETRY_SHADER4,           NULL) \
    /* GL_ARB_imaging, GL_EXT_blend_minmax */ \
    USE_GL_FUNC(PGLFNBLENDCOLORPROC, \
            glBlendColorEXT,                            EXT_BLEND_COLOR,                NULL) \
    USE_GL_FUNC(PGLFNBLENDEQUATIONPROC, \
            glBlendEquationEXT,                         EXT_BLEND_MINMAX,               NULL) \
    /* GL_ARB_map_buffer_range */ \
    USE_GL_FUNC(PGLFNMAPBUFFERRANGEPROC, \
            glMapBufferRange,                           ARB_MAP_BUFFER_RANGE,           NULL) \
    USE_GL_FUNC(PGLFNFLUSHMAPPEDBUFFERRANGEPROC, \
            glFlushMappedBufferRange,                   ARB_MAP_BUFFER_RANGE,           NULL) \
    /* GL_ARB_multisample */ \
    USE_GL_FUNC(WINED3D_PFNGLSAMPLECOVERAGEARBPROC, \
            glSampleCoverageARB,                        ARB_MULTISAMPLE,                NULL) \
    /* GL_ARB_multitexture */ \
    USE_GL_FUNC(WINED3D_PFNGLACTIVETEXTUREARBPROC, \
            glActiveTextureARB,                         ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCLIENTACTIVETEXTUREARBPROC, \
            glClientActiveTextureARB,                   ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD1FARBPROC, \
            glMultiTexCoord1fARB,                       ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD1FVARBPROC, \
            glMultiTexCoord1fvARB,                      ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD2FARBPROC, \
            glMultiTexCoord2fARB,                       ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD2FVARBPROC, \
            glMultiTexCoord2fvARB,                      ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD3FARBPROC, \
            glMultiTexCoord3fARB,                       ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD3FVARBPROC, \
            glMultiTexCoord3fvARB,                      ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD4FARBPROC, \
            glMultiTexCoord4fARB,                       ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD4FVARBPROC, \
            glMultiTexCoord4fvARB,                      ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD2SVARBPROC, \
            glMultiTexCoord2svARB,                      ARB_MULTITEXTURE,               NULL) \
    USE_GL_FUNC(WINED3D_PFNGLMULTITEXCOORD4SVARBPROC, \
            glMultiTexCoord4svARB,                      ARB_MULTITEXTURE,               NULL) \
    /* GL_ARB_occlusion_query */ \
    USE_GL_FUNC(PGLFNGENQUERIESARBPROC, \
            glGenQueriesARB,                            ARB_OCCLUSION_QUERY,            NULL) \
    USE_GL_FUNC(PGLFNDELETEQUERIESARBPROC, \
            glDeleteQueriesARB,                         ARB_OCCLUSION_QUERY,            NULL) \
    USE_GL_FUNC(PGLFNBEGINQUERYARBPROC, \
            glBeginQueryARB,                            ARB_OCCLUSION_QUERY,            NULL) \
    USE_GL_FUNC(PGLFNENDQUERYARBPROC, \
            glEndQueryARB,                              ARB_OCCLUSION_QUERY,            NULL) \
    USE_GL_FUNC(PGLFNGETQUERYOBJECTIVARBPROC, \
            glGetQueryObjectivARB,                      ARB_OCCLUSION_QUERY,            NULL) \
    USE_GL_FUNC(PGLFNGETQUERYOBJECTUIVARBPROC, \
            glGetQueryObjectuivARB,                     ARB_OCCLUSION_QUERY,            NULL) \
    /* GL_ARB_point_parameters */ \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFARBPROC, \
            glPointParameterfARB,                       ARB_POINT_PARAMETERS,           NULL) \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFVARBPROC, \
            glPointParameterfvARB,                      ARB_POINT_PARAMETERS,           NULL) \
    /* GL_ARB_provoking_vertex */ \
    USE_GL_FUNC(PGLFNGLPROVOKINGVERTEXPROC, \
            glProvokingVertex,                          ARB_PROVOKING_VERTEX,           NULL) \
    /* GL_ARB_shader_objects */ \
    USE_GL_FUNC(WINED3D_PFNGLGETOBJECTPARAMETERIVARBPROC, \
            glGetObjectParameterivARB,                  ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETOBJECTPARAMETERFVARBPROC, \
            glGetObjectParameterfvARB,                  ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMLOCATIONARBPROC, \
            glGetUniformLocationARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETACTIVEUNIFORMARBPROC, \
            glGetActiveUniformARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1IARBPROC, \
            glUniform1iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2IARBPROC, \
            glUniform2iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3IARBPROC, \
            glUniform3iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4IARBPROC, \
            glUniform4iARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1FARBPROC, \
            glUniform1fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2FARBPROC, \
            glUniform2fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3FARBPROC, \
            glUniform3fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4FARBPROC, \
            glUniform4fARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1FVARBPROC, \
            glUniform1fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2FVARBPROC, \
            glUniform2fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3FVARBPROC, \
            glUniform3fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4FVARBPROC, \
            glUniform4fvARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM1IVARBPROC, \
            glUniform1ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM2IVARBPROC, \
            glUniform2ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM3IVARBPROC, \
            glUniform3ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORM4IVARBPROC, \
            glUniform4ivARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX2FVARBPROC, \
            glUniformMatrix2fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX3FVARBPROC, \
            glUniformMatrix3fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUNIFORMMATRIX4FVARBPROC, \
            glUniformMatrix4fvARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMFVARBPROC, \
            glGetUniformfvARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETUNIFORMIVARBPROC, \
            glGetUniformivARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETINFOLOGARBPROC, \
            glGetInfoLogARB,                            ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLUSEPROGRAMOBJECTARBPROC, \
            glUseProgramObjectARB,                      ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCREATESHADEROBJECTARBPROC, \
            glCreateShaderObjectARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLSHADERSOURCEARBPROC, \
            glShaderSourceARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCOMPILESHADERARBPROC, \
            glCompileShaderARB,                         ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLCREATEPROGRAMOBJECTARBPROC, \
            glCreateProgramObjectARB,                   ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLATTACHOBJECTARBPROC, \
            glAttachObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLLINKPROGRAMARBPROC, \
            glLinkProgramARB,                           ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLDETACHOBJECTARBPROC, \
            glDetachObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLDELETEOBJECTARBPROC, \
            glDeleteObjectARB,                          ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLVALIDATEPROGRAMARBPROC, \
            glValidateProgramARB,                       ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETATTACHEDOBJECTSARBPROC, \
            glGetAttachedObjectsARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETHANDLEARBPROC, \
            glGetHandleARB,                             ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETSHADERSOURCEARBPROC, \
            glGetShaderSourceARB,                       ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLBINDATTRIBLOCATIONARBPROC, \
            glBindAttribLocationARB,                    ARB_SHADER_OBJECTS,             NULL) \
    USE_GL_FUNC(WINED3D_PFNGLGETATTRIBLOCATIONARBPROC, \
            glGetAttribLocationARB,                     ARB_SHADER_OBJECTS,             NULL) \
    /* GL_ARB_sync */ \
    USE_GL_FUNC(PGLFNFENCESYNCPROC, \
            glFenceSync,                                ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNISSYNCPROC, \
            glIsSync,                                   ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNDELETESYNCPROC, \
            glDeleteSync,                               ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNCLIENTWAITSYNCPROC, \
            glClientWaitSync,                           ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNWAITSYNCPROC, \
            glWaitSync,                                 ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNGETINTEGER64VPROC, \
            glGetInteger64v,                            ARB_SYNC,                       NULL) \
    USE_GL_FUNC(PGLFNGETSYNCIVPROC, \
            glGetSynciv,                                ARB_SYNC,                       NULL) \
    /* GL_ARB_texture_compression */ \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXIMAGE2DPROC, \
            glCompressedTexImage2DARB,                  ARB_TEXTURE_COMPRESSION,        NULL) \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXIMAGE3DPROC, \
            glCompressedTexImage3DARB,                  ARB_TEXTURE_COMPRESSION,        NULL) \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXSUBIMAGE2DPROC, \
            glCompressedTexSubImage2DARB,               ARB_TEXTURE_COMPRESSION,        NULL) \
    USE_GL_FUNC(PGLFNCOMPRESSEDTEXSUBIMAGE3DPROC, \
            glCompressedTexSubImage3DARB,               ARB_TEXTURE_COMPRESSION,        NULL) \
    USE_GL_FUNC(PGLFNGETCOMPRESSEDTEXIMAGEPROC, \
            glGetCompressedTexImageARB,                 ARB_TEXTURE_COMPRESSION,        NULL) \
    /* GL_ARB_vertex_blend */ \
    USE_GL_FUNC(PGLFNGLWEIGHTPOINTERARB, \
            glWeightPointerARB,                         ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTBV, \
            glWeightbvARB,                              ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTSV, \
            glWeightsvARB,                              ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTIV, \
            glWeightivARB,                              ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTFV, \
            glWeightfvARB,                              ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTDV, \
            glWeightdvARB,                              ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTUBV, \
            glWeightubvARB,                             ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTUSV, \
            glWeightusvARB,                             ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLWEIGHTUIV, \
            glWeightuivARB,                             ARB_VERTEX_BLEND,               NULL) \
    USE_GL_FUNC(PGLFNGLVERTEXBLENDARB, \
            glVertexBlendARB,                           ARB_VERTEX_BLEND,               NULL) \
    /* GL_ARB_vertex_buffer_object */ \
    USE_GL_FUNC(PGLFNBINDBUFFERARBPROC, \
            glBindBufferARB,                            ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNDELETEBUFFERSARBPROC, \
            glDeleteBuffersARB,                         ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNGENBUFFERSARBPROC, \
            glGenBuffersARB,                            ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNISBUFFERARBPROC, \
            glIsBufferARB,                              ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNBUFFERDATAARBPROC, \
            glBufferDataARB,                            ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNBUFFERSUBDATAARBPROC, \
            glBufferSubDataARB,                         ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNGETBUFFERSUBDATAARBPROC, \
            glGetBufferSubDataARB,                      ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNMAPBUFFERARBPROC, \
            glMapBufferARB,                             ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNUNMAPBUFFERARBPROC, \
            glUnmapBufferARB,                           ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNGETBUFFERPARAMETERIVARBPROC, \
            glGetBufferParameterivARB,                  ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    USE_GL_FUNC(PGLFNGETBUFFERPOINTERVARBPROC, \
            glGetBufferPointervARB,                     ARB_VERTEX_BUFFER_OBJECT,       NULL) \
    /* GL_ARB_vertex_program */ \
    USE_GL_FUNC(PGLFNGENPROGRAMSARBPROC, \
            glGenProgramsARB,                           ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNBINDPROGRAMARBPROC, \
            glBindProgramARB,                           ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNPROGRAMSTRINGARBPROC, \
            glProgramStringARB,                         ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNDELETEPROGRAMSARBPROC, \
            glDeleteProgramsARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNPROGRAMENVPARAMETER4FVARBPROC, \
            glProgramEnvParameter4fvARB,                ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNPROGRAMLOCALPARAMETER4FVARBPROC, \
            glProgramLocalParameter4fvARB,              ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBPOINTERARBPROC, \
            glVertexAttribPointerARB,                   ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNENABLEVERTEXATTRIBARRAYARBPROC, \
            glEnableVertexAttribArrayARB,               ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNDISABLEVERTEXATTRIBARRAYARBPROC, \
            glDisableVertexAttribArrayARB,              ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1DARBPROC, \
            glVertexAttrib1dARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1DVARBPROC, \
            glVertexAttrib1dvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1FARBPROC, \
            glVertexAttrib1fARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1FVARBPROC, \
            glVertexAttrib1fvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1SARBPROC, \
            glVertexAttrib1sARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1SVARBPROC, \
            glVertexAttrib1svARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2DARBPROC, \
            glVertexAttrib2dARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2DVARBPROC, \
            glVertexAttrib2dvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2FARBPROC, \
            glVertexAttrib2fARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2FVARBPROC, \
            glVertexAttrib2fvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2SARBPROC, \
            glVertexAttrib2sARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2SVARBPROC, \
            glVertexAttrib2svARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3DARBPROC, \
            glVertexAttrib3dARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3DVARBPROC, \
            glVertexAttrib3dvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3FARBPROC, \
            glVertexAttrib3fARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3FVARBPROC, \
            glVertexAttrib3fvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3SARBPROC, \
            glVertexAttrib3sARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3SVARBPROC, \
            glVertexAttrib3svARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NBVARBPROC, \
            glVertexAttrib4NbvARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NIVARBPROC, \
            glVertexAttrib4NivARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NSVARBPROC, \
            glVertexAttrib4NsvARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NUBARBPROC, \
            glVertexAttrib4NubARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NUBVARBPROC, \
            glVertexAttrib4NubvARB,                     ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NUIVARBPROC, \
            glVertexAttrib4NuivARB,                     ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4NUSVARBPROC, \
            glVertexAttrib4NusvARB,                     ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4BVARBPROC, \
            glVertexAttrib4bvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4DARBPROC, \
            glVertexAttrib4dARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4DVARBPROC, \
            glVertexAttrib4dvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4FARBPROC, \
            glVertexAttrib4fARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4FVARBPROC, \
            glVertexAttrib4fvARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4IVARBPROC, \
            glVertexAttrib4ivARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4SARBPROC, \
            glVertexAttrib4sARB,                        ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4SVARBPROC, \
            glVertexAttrib4svARB,                       ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4UBVARBPROC, \
            glVertexAttrib4ubvARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4UIVARBPROC, \
            glVertexAttrib4uivARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4USVARBPROC, \
            glVertexAttrib4usvARB,                      ARB_VERTEX_PROGRAM,             NULL) \
    USE_GL_FUNC(PGLFNGETPROGRAMIVARBPROC, \
            glGetProgramivARB,                          ARB_VERTEX_PROGRAM,             NULL) \
    /* GL_ATI_fragment_shader */ \
    USE_GL_FUNC(PGLFNGENFRAGMENTSHADERSATI, \
            glGenFragmentShadersATI,                    ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNBINDFRAGMENTSHADERATI, \
            glBindFragmentShaderATI,                    ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNDELETEFRAGMENTSHADERATI, \
            glDeleteFragmentShaderATI,                  ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNBEGINFRAGMENTSHADERATI, \
            glBeginFragmentShaderATI,                   ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNENDFRAGMENTSHADERATI, \
            glEndFragmentShaderATI,                     ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNPASSTEXCOORDATI, \
            glPassTexCoordATI,                          ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNSAMPLEMAPATI, \
            glSampleMapATI,                             ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNCOLORFRAGMENTOP1ATI, \
            glColorFragmentOp1ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNCOLORFRAGMENTOP2ATI, \
            glColorFragmentOp2ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNCOLORFRAGMENTOP3ATI, \
            glColorFragmentOp3ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNALPHAFRAGMENTOP1ATI, \
            glAlphaFragmentOp1ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNALPHAFRAGMENTOP2ATI, \
            glAlphaFragmentOp2ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNALPHAFRAGMENTOP3ATI, \
            glAlphaFragmentOp3ATI,                      ATI_FRAGMENT_SHADER,            NULL) \
    USE_GL_FUNC(PGLFNSETFRAGMENTSHADERCONSTANTATI, \
            glSetFragmentShaderConstantATI,             ATI_FRAGMENT_SHADER,            NULL) \
    /* GL_ATI_separate_stencil */ \
    USE_GL_FUNC(PGLFNSTENCILOPSEPARATEATIPROC, \
            glStencilOpSeparateATI,                     ATI_SEPARATE_STENCIL,           NULL) \
    USE_GL_FUNC(PGLFNSTENCILFUNCSEPARATEATIPROC, \
            glStencilFuncSeparateATI,                   ATI_SEPARATE_STENCIL,           NULL) \
    /* GL_EXT_blend_equation_separate */ \
    USE_GL_FUNC(PGLFNBLENDFUNCSEPARATEEXTPROC, \
            glBlendFuncSeparateEXT,                     EXT_BLEND_FUNC_SEPARATE,        NULL) \
    /* GL_EXT_blend_func_separate */ \
    USE_GL_FUNC(PGLFNBLENDEQUATIONSEPARATEEXTPROC, \
            glBlendEquationSeparateEXT,                 EXT_BLEND_EQUATION_SEPARATE,    NULL) \
    /* GL_EXT_draw_buffers2 */ \
    USE_GL_FUNC(PGLFNCOLORMASKINDEXEDEXTPROC, \
            glColorMaskIndexedEXT,                      EXT_DRAW_BUFFERS2,              NULL) \
    USE_GL_FUNC(PGLFNGETBOOLEANINDEXEDVEXTPROC, \
            glGetBooleanIndexedvEXT,                    EXT_DRAW_BUFFERS2,              NULL) \
    USE_GL_FUNC(PGLFNGETINTEGERINDEXEDVEXTPROC, \
            glGetIntegerIndexedvEXT,                    EXT_DRAW_BUFFERS2,              NULL) \
    USE_GL_FUNC(PGLFNENABLEINDEXEDEXTPROC, \
            glEnableIndexedEXT,                         EXT_DRAW_BUFFERS2,              NULL) \
    USE_GL_FUNC(PGLFNDISABLEINDEXEDEXTPROC, \
            glDisableIndexedEXT,                        EXT_DRAW_BUFFERS2,              NULL) \
    USE_GL_FUNC(PGLFNISENABLEDINDEXEDEXTPROC, \
            glIsEnabledIndexedEXT,                      EXT_DRAW_BUFFERS2,              NULL) \
    /* GL_EXT_fog_coord */ \
    USE_GL_FUNC(PGLFNGLFOGCOORDFEXTPROC, \
            glFogCoordfEXT,                             EXT_FOG_COORD,                  NULL) \
    USE_GL_FUNC(PGLFNGLFOGCOORDFVEXTPROC, \
            glFogCoordfvEXT,                            EXT_FOG_COORD,                  NULL) \
    USE_GL_FUNC(PGLFNGLFOGCOORDDEXTPROC, \
            glFogCoorddEXT,                             EXT_FOG_COORD,                  NULL) \
    USE_GL_FUNC(PGLFNGLFOGCOORDDVEXTPROC, \
            glFogCoorddvEXT,                            EXT_FOG_COORD,                  NULL) \
    USE_GL_FUNC(PGLFNGLFOGCOORDPOINTEREXTPROC, \
            glFogCoordPointerEXT,                       EXT_FOG_COORD,                  NULL) \
    /* GL_EXT_framebuffer_blit */ \
    USE_GL_FUNC(PGLFNGLBLITFRAMEBUFFEREXTPROC, \
            glBlitFramebufferEXT,                       EXT_FRAMEBUFFER_BLIT,           NULL) \
    /* GL_EXT_framebuffer_multisample */ \
    USE_GL_FUNC(PGLFNRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, \
            glRenderbufferStorageMultisampleEXT,        EXT_FRAMEBUFFER_MULTISAMPLE,    NULL) \
    /* GL_EXT_framebuffer_object */ \
    USE_GL_FUNC(PGLFNGLISRENDERBUFFEREXTPROC, \
            glIsRenderbufferEXT,                        EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLBINDRENDERBUFFEREXTPROC, \
            glBindRenderbufferEXT,                      EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLDELETERENDERBUFFERSEXTPROC, \
            glDeleteRenderbuffersEXT,                   EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENRENDERBUFFERSEXTPROC, \
            glGenRenderbuffersEXT,                      EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLRENDERBUFFERSTORAGEEXTPROC, \
            glRenderbufferStorageEXT,                   EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLISFRAMEBUFFEREXTPROC, \
            glIsFramebufferEXT,                         EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLBINDFRAMEBUFFEREXTPROC, \
            glBindFramebufferEXT,                       EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLDELETEFRAMEBUFFERSEXTPROC, \
            glDeleteFramebuffersEXT,                    EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENFRAMEBUFFERSEXTPROC, \
            glGenFramebuffersEXT,                       EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, \
            glCheckFramebufferStatusEXT,                EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE1DEXTPROC, \
            glFramebufferTexture1DEXT,                  EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE2DEXTPROC, \
            glFramebufferTexture2DEXT,                  EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERTEXTURE3DEXTPROC, \
            glFramebufferTexture3DEXT,                  EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, \
            glFramebufferRenderbufferEXT,               EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGENERATEMIPMAPEXTPROC, \
            glGenerateMipmapEXT,                        EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGETRENDERBUFFERPARAMETERIVEXTPROC, \
            glGetRenderbufferParameterivEXT,            EXT_FRAMEBUFFER_OBJECT,         NULL) \
    USE_GL_FUNC(PGLFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC, \
            glGetFramebufferAttachmentParameterivEXT,   EXT_FRAMEBUFFER_OBJECT,         NULL) \
    /* GL_EXT_gpu_program_parameters */ \
    USE_GL_FUNC(PGLFNPROGRAMENVPARAMETERS4FVEXTPROC, \
            glProgramEnvParameters4fvEXT,               EXT_GPU_PROGRAM_PARAMETERS,     NULL) \
    USE_GL_FUNC(PGLFNPROGRAMLOCALPARAMETERS4FVEXTPROC, \
            glProgramLocalParameters4fvEXT,             EXT_GPU_PROGRAM_PARAMETERS,     NULL) \
    /* GL_EXT_gpu_shader4 */\
    USE_GL_FUNC(PGLFNVERTEXATTRIBI1IEXTPROC, \
            glVertexAttribI1iEXT,                       EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI2IEXTPROC, \
            glVertexAttribI2iEXT,                       EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI3IEXTPROC, \
            glVertexAttribI3iEXT,                       EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4IEXTPROC, \
            glVertexAttribI4iEXT,                       EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI1UIEXTPROC, \
            glVertexAttribI1uiEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI2UIEXTPROC, \
            glVertexAttribI2uiEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI3UIEXTPROC, \
            glVertexAttribI3uiEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4UIEXTPROC, \
            glVertexAttribI4uiEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI1IVEXTPROC, \
            glVertexAttribI1ivEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI2IVEXTPROC, \
            glVertexAttribI2ivEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI3IVEXTPROC, \
            glVertexAttribI3ivEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4IVEXTPROC, \
            glVertexAttribI4ivEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI1UIVEXTPROC, \
            glVertexAttribI1uivEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI2UIVEXTPROC, \
            glVertexAttribI2uivEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI3UIVEXTPROC, \
            glVertexAttribI3uivEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4UIVEXTPROC, \
            glVertexAttribI4uivEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4BVEXTPROC, \
            glVertexAttribI4bvEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4SVEXTPROC, \
            glVertexAttribI4svEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4UBVEXTPROC, \
            glVertexAttribI4ubvEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBI4USVEXTPROC, \
            glVertexAttribI4usvEXT,                     EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBIPOINTEREXTPROC, \
            glVertexAttribIPointerEXT,                  EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNGETVERTEXATTRIBIIVEXTPROC, \
            glVertexAttribIivEXT,                       EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNGETVERTEXATTRIBIUIVEXTPROC, \
            glVertexAttribIuivEXT,                      EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM1UIEXTPROC, \
            glUniform1uiEXT,                            EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM2UIEXTPROC, \
            glUniform2uiEXT,                            EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM3UIEXTPROC, \
            glUniform3uiEXT,                            EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM4UIEXTPROC, \
            glUniform4uiEXT,                            EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM1UIVEXTPROC, \
            glUniform1uivEXT,                           EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM2UIVEXTPROC, \
            glUniform2uivEXT,                           EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM3UIVEXTPROC, \
            glUniform3uivEXT,                           EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNUNIFORM4UIVEXTPROC, \
            glUniform4uivEXT,                           EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNGETUNIFORMUIVEXTPROC, \
            glGetUniformuivEXT,                         EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNBINDFRAGDATALOCATIONEXTPROC, \
            glBindFragDataLocationEXT,                  EXT_GPU_SHADER4,                NULL) \
    USE_GL_FUNC(PGLFNGETFRAGDATALOCATIONEXTPROC, \
            glGetFragDataLocationEXT,                   EXT_GPU_SHADER4,                NULL) \
    /* GL_EXT_paletted_texture */ \
    USE_GL_FUNC(PGLFNGLCOLORTABLEEXTPROC, \
            glColorTableEXT,                            EXT_PALETTED_TEXTURE,           NULL) \
    /* GL_EXT_point_parameters */ \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFEXTPROC, \
            glPointParameterfEXT,                       EXT_POINT_PARAMETERS,           NULL) \
    USE_GL_FUNC(PGLFNGLPOINTPARAMETERFVEXTPROC, \
            glPointParameterfvEXT,                      EXT_POINT_PARAMETERS,           NULL) \
    /* GL_EXT_provoking_vertex */ \
    USE_GL_FUNC(PGLFNGLPROVOKINGVERTEXEXTPROC, \
            glProvokingVertexEXT,                       EXT_PROVOKING_VERTEX,           NULL) \
    /* GL_EXT_secondary_color */ \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3UBEXTPROC, \
            glSecondaryColor3ubEXT,                     EXT_SECONDARY_COLOR,            NULL) \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3UBVEXTPROC, \
            glSecondaryColor3ubvEXT,                    EXT_SECONDARY_COLOR,            NULL) \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3FEXTPROC, \
            glSecondaryColor3fEXT,                      EXT_SECONDARY_COLOR,            NULL) \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLOR3FVEXTPROC, \
            glSecondaryColor3fvEXT,                     EXT_SECONDARY_COLOR,            NULL) \
    USE_GL_FUNC(PGLFNGLSECONDARYCOLORPOINTEREXTPROC, \
            glSecondaryColorPointerEXT,                 EXT_SECONDARY_COLOR,            NULL) \
    /* GL_EXT_stencil_two_side */ \
    USE_GL_FUNC(PGLFNACTIVESTENCILFACEEXTPROC, \
            glActiveStencilFaceEXT,                     EXT_STENCIL_TWO_SIDE,           NULL) \
    /* GL_EXT_texture3D */ \
    USE_GL_FUNC(PGLFNGLTEXIMAGE3DEXTPROC, \
            glTexImage3DEXT,                            EXT_TEXTURE3D,                  glTexImage3D) \
    USE_GL_FUNC(PGLFNGLTEXSUBIMAGE3DEXTPROC, \
            glTexSubImage3DEXT,                         EXT_TEXTURE3D,                  glTexSubImage3D) \
    /* GL_NV_fence */ \
    USE_GL_FUNC(PGLFNGENFENCESNVPROC, \
            glGenFencesNV,                              NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNDELETEFENCESNVPROC, \
            glDeleteFencesNV,                           NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNSETFENCENVPROC,    \
            glSetFenceNV,                               NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNTESTFENCENVPROC, \
            glTestFenceNV,                              NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNFINISHFENCENVPROC, \
            glFinishFenceNV,                            NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNISFENCENVPROC, \
            glIsFenceNV,                                NV_FENCE,                       NULL) \
    USE_GL_FUNC(PGLFNGETFENCEIVNVPROC, \
            glGetFenceivNV,                             NV_FENCE,                       NULL) \
    /* GL_NV_half_float */ \
    USE_GL_FUNC(PGLFNVERTEX2HNVPROC, \
            glVertex2hNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEX2HVNVPROC, \
            glVertex2hvNV,                              NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEX3HNVPROC, \
            glVertex3hNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEX3HVNVPROC, \
            glVertex3hvNV,                              NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEX4HNVPROC, \
            glVertex4hNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEX4HVNVPROC, \
            glVertex4hvNV,                              NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNNORMAL3HNVPROC, \
            glNormal3hNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNNORMAL3HVNVPROC, \
            glNormal3hvNV,                              NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNCOLOR3HNVPROC, \
            glColor3hNV,                                NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNCOLOR3HVNVPROC, \
            glColor3hvNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNCOLOR4HNVPROC, \
            glColor4hNV,                                NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNCOLOR4HVNVPROC, \
            glColor4hvNV,                               NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD1HNVPROC, \
            glTexCoord1hNV,                             NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD1HVNVPROC, \
            glTexCoord1hvNV,                            NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD2HNVPROC, \
            glTexCoord2hNV,                             NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD2HVNVPROC, \
            glTexCoord2hvNV,                            NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD3HNVPROC, \
            glTexCoord3hNV,                             NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD3HVNVPROC, \
            glTexCoord3hvNV,                            NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD4HNVPROC, \
            glTexCoord4hNV,                             NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNTEXCOORD4HVNVPROC, \
            glTexCoord4hvNV,                            NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD1HNVPROC, \
            glMultiTexCoord1hNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD1HVNVPROC, \
            glMultiTexCoord1hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD2HNVPROC, \
            glMultiTexCoord2hNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD2HVNVPROC, \
            glMultiTexCoord2hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD3HNVPROC, \
            glMultiTexCoord3hNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD3HVNVPROC, \
            glMultiTexCoord3hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD4HNVPROC, \
            glMultiTexCoord4hNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNMULTITEXCOORD4HVNVPROC, \
            glMultiTexCoord4hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNFOGCOORDHNVPROC, \
            glFogCoordhNV,                              NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNFOGCOORDHVNVPROC, \
            glFogCoordhvNV,                             NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNSECONDARYCOLOR3HNVPROC, \
            glSecondaryColor3hNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNSECONDARYCOLOR3HVNVPROC, \
            glSecondaryColor3hvNV,                      NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXWEIGHTHNVPROC, \
            glVertexWeighthNV,                          NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXWEIGHTHVNVPROC, \
            glVertexWeighthvNV,                         NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1HNVPROC, \
            glVertexAttrib1hNV,                         NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB1HVNVPROC, \
            glVertexAttrib1hvNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2HNVPROC, \
            glVertexAttrib2hNV,                         NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB2HVNVPROC, \
            glVertexAttrib2hvNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3HNVPROC, \
            glVertexAttrib3hNV,                         NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB3HVNVPROC, \
            glVertexAttrib3hvNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4HNVPROC, \
            glVertexAttrib4hNV,                         NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIB4HVNVPROC, \
            glVertexAttrib4hvNV,                        NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBS1HVNVPROC, \
            glVertexAttribs1hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBS2HVNVPROC, \
            glVertexAttribs2hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBS3HVNVPROC, \
            glVertexAttribs3hvNV,                       NV_HALF_FLOAT,                  NULL) \
    USE_GL_FUNC(PGLFNVERTEXATTRIBS4HVNVPROC, \
            glVertexAttribs4hvNV,                       NV_HALF_FLOAT,                  NULL) \
    /* GL_NV_register_combiners */ \
    USE_GL_FUNC(PGLFNCOMBINERINPUTNVPROC, \
            glCombinerInputNV,                          NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNCOMBINEROUTPUTNVPROC, \
            glCombinerOutputNV,                         NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNCOMBINERPARAMETERFNVPROC, \
            glCombinerParameterfNV,                     NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNCOMBINERPARAMETERFVNVPROC, \
            glCombinerParameterfvNV,                    NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNCOMBINERPARAMETERINVPROC, \
            glCombinerParameteriNV,                     NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNCOMBINERPARAMETERIVNVPROC, \
            glCombinerParameterivNV,                    NV_REGISTER_COMBINERS,          NULL) \
    USE_GL_FUNC(PGLFNFINALCOMBINERINPUTNVPROC, \
            glFinalCombinerInputNV,                     NV_REGISTER_COMBINERS,          NULL) \
    /* GLX_SGI_video_sync */ \
    USE_GL_FUNC(PGLXFNGETVIDEOSYNCSGIPROC, \
            glXGetVideoSyncSGI,                         SGI_VIDEO_SYNC,                 NULL) \
    USE_GL_FUNC(PGLXFNWAITVIDEOSYNCSGIPROC, \
            glXWaitVideoSyncSGI,                        SGI_VIDEO_SYNC,                 NULL) \
    VBOXWDDM_GL_EXT_FUNCS_GEN

#define WGL_EXT_FUNCS_GEN \
    USE_GL_FUNC(WINED3D_PFNWGLGETEXTENSIONSSTRINGARBPROC,       wglGetExtensionsStringARB,      0, NULL) \
    USE_GL_FUNC(WINED3D_PFNWGLGETPIXELFORMATATTRIBIVARBPROC,    wglGetPixelFormatAttribivARB,   0, NULL) \
    USE_GL_FUNC(WINED3D_PFNWGLGETPIXELFORMATATTRIBFVARBPROC,    wglGetPixelFormatAttribfvARB,   0, NULL) \
    USE_GL_FUNC(WINED3D_PFNWGLCHOOSEPIXELFORMATARBPROC,         wglChoosePixelFormatARB,        0, NULL) \
    USE_GL_FUNC(WINED3D_PFNWGLSETPIXELFORMATWINE,               wglSetPixelFormatWINE,          0, NULL)

#endif /* !VBOX_INCLUDED_SRC_Graphics_shaderlib_wined3d_gl_h */
