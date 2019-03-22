/**************************************************************************
 *
 * Copyright 2009 Younes Manton.
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

#include <assert.h>

#include "pipe/p_compiler.h"
#include "pipe/p_context.h"

#include "util/u_memory.h"
#include "util/u_draw.h"
#include "util/u_surface.h"
#include "util/u_upload_mgr.h"
#include "util/u_sampler.h"

#include "tgsi/tgsi_ureg.h"

#include "vl_csc.h"
#include "vl_types.h"
#include "vl_compositor.h"

#define MIN_DIRTY (0)
#define MAX_DIRTY (1 << 15)

enum VS_OUTPUT
{
   VS_O_VPOS = 0,
   VS_O_COLOR = 0,
   VS_O_VTEX = 0,
   VS_O_VTOP,
   VS_O_VBOTTOM,
};

static void *
create_vert_shader(struct vl_compositor *c)
{
   struct ureg_program *shader;
   struct ureg_src vpos, vtex, color;
   struct ureg_dst tmp;
   struct ureg_dst o_vpos, o_vtex, o_color;
   struct ureg_dst o_vtop, o_vbottom;

   shader = ureg_create(PIPE_SHADER_VERTEX);
   if (!shader)
      return false;

   vpos = ureg_DECL_vs_input(shader, 0);
   vtex = ureg_DECL_vs_input(shader, 1);
   color = ureg_DECL_vs_input(shader, 2);
   tmp = ureg_DECL_temporary(shader);
   o_vpos = ureg_DECL_output(shader, TGSI_SEMANTIC_POSITION, VS_O_VPOS);
   o_color = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, VS_O_COLOR);
   o_vtex = ureg_DECL_output(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTEX);
   o_vtop = ureg_DECL_output(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTOP);
   o_vbottom = ureg_DECL_output(shader, TGSI_SEMANTIC_GENERIC, VS_O_VBOTTOM);

   /*
    * o_vpos = vpos
    * o_vtex = vtex
    * o_color = color
    */
   ureg_MOV(shader, o_vpos, vpos);
   ureg_MOV(shader, o_vtex, vtex);
   ureg_MOV(shader, o_color, color);

   /*
    * tmp.x = vtex.w / 2
    * tmp.y = vtex.w / 4
    *
    * o_vtop.x = vtex.x
    * o_vtop.y = vtex.y * tmp.x + 0.25f
    * o_vtop.z = vtex.y * tmp.y + 0.25f
    * o_vtop.w = 1 / tmp.x
    *
    * o_vbottom.x = vtex.x
    * o_vbottom.y = vtex.y * tmp.x - 0.25f
    * o_vbottom.z = vtex.y * tmp.y - 0.25f
    * o_vbottom.w = 1 / tmp.y
    */
   ureg_MUL(shader, ureg_writemask(tmp, TGSI_WRITEMASK_X),
            ureg_scalar(vtex, TGSI_SWIZZLE_W), ureg_imm1f(shader, 0.5f));
   ureg_MUL(shader, ureg_writemask(tmp, TGSI_WRITEMASK_Y),
            ureg_scalar(vtex, TGSI_SWIZZLE_W), ureg_imm1f(shader, 0.25f));

   ureg_MOV(shader, ureg_writemask(o_vtop, TGSI_WRITEMASK_X), vtex);
   ureg_MAD(shader, ureg_writemask(o_vtop, TGSI_WRITEMASK_Y), ureg_scalar(vtex, TGSI_SWIZZLE_Y),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_imm1f(shader, 0.25f));
   ureg_MAD(shader, ureg_writemask(o_vtop, TGSI_WRITEMASK_Z), ureg_scalar(vtex, TGSI_SWIZZLE_Y),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y), ureg_imm1f(shader, 0.25f));
   ureg_RCP(shader, ureg_writemask(o_vtop, TGSI_WRITEMASK_W),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X));

   ureg_MOV(shader, ureg_writemask(o_vbottom, TGSI_WRITEMASK_X), vtex);
   ureg_MAD(shader, ureg_writemask(o_vbottom, TGSI_WRITEMASK_Y), ureg_scalar(vtex, TGSI_SWIZZLE_Y),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_X), ureg_imm1f(shader, -0.25f));
   ureg_MAD(shader, ureg_writemask(o_vbottom, TGSI_WRITEMASK_Z), ureg_scalar(vtex, TGSI_SWIZZLE_Y),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y), ureg_imm1f(shader, -0.25f));
   ureg_RCP(shader, ureg_writemask(o_vbottom, TGSI_WRITEMASK_W),
            ureg_scalar(ureg_src(tmp), TGSI_SWIZZLE_Y));

   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void
create_frag_shader_weave(struct ureg_program *shader, struct ureg_dst fragment)
{
   struct ureg_src i_tc[2];
   struct ureg_src sampler[3];
   struct ureg_dst t_tc[2];
   struct ureg_dst t_texel[2];
   unsigned i, j;

   i_tc[0] = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTOP, TGSI_INTERPOLATE_LINEAR);
   i_tc[1] = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VBOTTOM, TGSI_INTERPOLATE_LINEAR);

   for (i = 0; i < 3; ++i) {
      sampler[i] = ureg_DECL_sampler(shader, i);
      ureg_DECL_sampler_view(shader, i, TGSI_TEXTURE_2D_ARRAY,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT);
   }
   
   for (i = 0; i < 2; ++i) {
      t_tc[i] = ureg_DECL_temporary(shader);
      t_texel[i] = ureg_DECL_temporary(shader);
   }

   /* calculate the texture offsets
    * t_tc.x = i_tc.x
    * t_tc.y = (round(i_tc.y - 0.5) + 0.5) / height * 2
    */
   for (i = 0; i < 2; ++i) {
      ureg_MOV(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_X), i_tc[i]);
      ureg_ADD(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_YZ),
               i_tc[i], ureg_imm1f(shader, -0.5f));
      ureg_ROUND(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_YZ), ureg_src(t_tc[i]));
      ureg_MOV(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_W),
               ureg_imm1f(shader, i ? 1.0f : 0.0f));
      ureg_ADD(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_YZ),
               ureg_src(t_tc[i]), ureg_imm1f(shader, 0.5f));
      ureg_MUL(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_Y),
               ureg_src(t_tc[i]), ureg_scalar(i_tc[0], TGSI_SWIZZLE_W));
      ureg_MUL(shader, ureg_writemask(t_tc[i], TGSI_WRITEMASK_Z),
               ureg_src(t_tc[i]), ureg_scalar(i_tc[1], TGSI_SWIZZLE_W));
   }

   /* fetch the texels
    * texel[0..1].x = tex(t_tc[0..1][0])
    * texel[0..1].y = tex(t_tc[0..1][1])
    * texel[0..1].z = tex(t_tc[0..1][2])
    */
   for (i = 0; i < 2; ++i)
      for (j = 0; j < 3; ++j) {
         struct ureg_src src = ureg_swizzle(ureg_src(t_tc[i]),
            TGSI_SWIZZLE_X, j ? TGSI_SWIZZLE_Z : TGSI_SWIZZLE_Y, TGSI_SWIZZLE_W, TGSI_SWIZZLE_W);

         ureg_TEX(shader, ureg_writemask(t_texel[i], TGSI_WRITEMASK_X << j),
                  TGSI_TEXTURE_2D_ARRAY, src, sampler[j]);
      }

   /* calculate linear interpolation factor
    * factor = |round(i_tc.y) - i_tc.y| * 2
    */
   ureg_ROUND(shader, ureg_writemask(t_tc[0], TGSI_WRITEMASK_YZ), i_tc[0]);
   ureg_ADD(shader, ureg_writemask(t_tc[0], TGSI_WRITEMASK_YZ),
            ureg_src(t_tc[0]), ureg_negate(i_tc[0]));
   ureg_MUL(shader, ureg_writemask(t_tc[0], TGSI_WRITEMASK_YZ),
            ureg_abs(ureg_src(t_tc[0])), ureg_imm1f(shader, 2.0f));
   ureg_LRP(shader, fragment, ureg_swizzle(ureg_src(t_tc[0]),
            TGSI_SWIZZLE_Y, TGSI_SWIZZLE_Z, TGSI_SWIZZLE_Z, TGSI_SWIZZLE_Z),
            ureg_src(t_texel[0]), ureg_src(t_texel[1]));

   for (i = 0; i < 2; ++i) {
      ureg_release_temporary(shader, t_texel[i]);
      ureg_release_temporary(shader, t_tc[i]);
   }
}

static void
create_frag_shader_csc(struct ureg_program *shader, struct ureg_dst texel,
		       struct ureg_dst fragment)
{
   struct ureg_src csc[3];
   struct ureg_src lumakey;
   struct ureg_dst temp[2];
   unsigned i;

   for (i = 0; i < 3; ++i)
      csc[i] = ureg_DECL_constant(shader, i);

   lumakey = ureg_DECL_constant(shader, 3);

   for (i = 0; i < 2; ++i)
      temp[i] = ureg_DECL_temporary(shader);

   ureg_MOV(shader, ureg_writemask(texel, TGSI_WRITEMASK_W),
	    ureg_imm1f(shader, 1.0f));

   for (i = 0; i < 3; ++i)
      ureg_DP4(shader, ureg_writemask(fragment, TGSI_WRITEMASK_X << i), csc[i],
	       ureg_src(texel));

   ureg_MOV(shader, ureg_writemask(temp[0], TGSI_WRITEMASK_W),
            ureg_scalar(ureg_src(texel), TGSI_SWIZZLE_Z));
   ureg_SLE(shader, ureg_writemask(temp[1],TGSI_WRITEMASK_W),
            ureg_src(temp[0]), ureg_scalar(lumakey, TGSI_SWIZZLE_X));
   ureg_SGT(shader, ureg_writemask(temp[0],TGSI_WRITEMASK_W),
            ureg_src(temp[0]), ureg_scalar(lumakey, TGSI_SWIZZLE_Y));
   ureg_MAX(shader, ureg_writemask(fragment, TGSI_WRITEMASK_W),
            ureg_src(temp[0]), ureg_src(temp[1]));

   for (i = 0; i < 2; ++i)
       ureg_release_temporary(shader, temp[i]);
}

static void
create_frag_shader_yuv(struct ureg_program *shader, struct ureg_dst texel)
{
   struct ureg_src tc;
   struct ureg_src sampler[3];
   unsigned i;

   tc = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTEX, TGSI_INTERPOLATE_LINEAR);
   for (i = 0; i < 3; ++i) {
      sampler[i] = ureg_DECL_sampler(shader, i);
      ureg_DECL_sampler_view(shader, i, TGSI_TEXTURE_2D_ARRAY,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT,
                             TGSI_RETURN_TYPE_FLOAT);
   }

   /*
    * texel.xyz = tex(tc, sampler[i])
    */
   for (i = 0; i < 3; ++i)
      ureg_TEX(shader, ureg_writemask(texel, TGSI_WRITEMASK_X << i), TGSI_TEXTURE_2D_ARRAY, tc, sampler[i]);
}

static void *
create_frag_shader_video_buffer(struct vl_compositor *c)
{
   struct ureg_program *shader;
   struct ureg_dst texel;
   struct ureg_dst fragment;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   create_frag_shader_yuv(shader, texel);
   create_frag_shader_csc(shader, texel, fragment);

   ureg_release_temporary(shader, texel);
   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void *
create_frag_shader_weave_rgb(struct vl_compositor *c)
{
   struct ureg_program *shader;
   struct ureg_dst texel, fragment;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   create_frag_shader_weave(shader, texel);
   create_frag_shader_csc(shader, texel, fragment);

   ureg_release_temporary(shader, texel);

   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void *
create_frag_shader_deint_yuv(struct vl_compositor *c, bool y, bool w)
{
   struct ureg_program *shader;
   struct ureg_dst texel, fragment;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   if (w)
      create_frag_shader_weave(shader, texel);
   else
      create_frag_shader_yuv(shader, texel);

   if (y)
      ureg_MOV(shader, ureg_writemask(fragment, TGSI_WRITEMASK_X), ureg_src(texel));
   else
      ureg_MOV(shader, ureg_writemask(fragment, TGSI_WRITEMASK_XY),
                       ureg_swizzle(ureg_src(texel), TGSI_SWIZZLE_Y,
                               TGSI_SWIZZLE_Z, TGSI_SWIZZLE_W, TGSI_SWIZZLE_W));

   ureg_release_temporary(shader, texel);

   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void *
create_frag_shader_palette(struct vl_compositor *c, bool include_cc)
{
   struct ureg_program *shader;
   struct ureg_src csc[3];
   struct ureg_src tc;
   struct ureg_src sampler;
   struct ureg_src palette;
   struct ureg_dst texel;
   struct ureg_dst fragment;
   unsigned i;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   for (i = 0; include_cc && i < 3; ++i)
      csc[i] = ureg_DECL_constant(shader, i);

   tc = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTEX, TGSI_INTERPOLATE_LINEAR);
   sampler = ureg_DECL_sampler(shader, 0);
   ureg_DECL_sampler_view(shader, 0, TGSI_TEXTURE_2D,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT);
   palette = ureg_DECL_sampler(shader, 1);
   ureg_DECL_sampler_view(shader, 1, TGSI_TEXTURE_1D,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT);
   
   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   /*
    * texel = tex(tc, sampler)
    * fragment.xyz = tex(texel, palette) * csc
    * fragment.a = texel.a
    */
   ureg_TEX(shader, texel, TGSI_TEXTURE_2D, tc, sampler);
   ureg_MOV(shader, ureg_writemask(fragment, TGSI_WRITEMASK_W), ureg_src(texel));

   if (include_cc) {
      ureg_TEX(shader, texel, TGSI_TEXTURE_1D, ureg_src(texel), palette);
      for (i = 0; i < 3; ++i)
         ureg_DP4(shader, ureg_writemask(fragment, TGSI_WRITEMASK_X << i), csc[i], ureg_src(texel));
   } else {
      ureg_TEX(shader, ureg_writemask(fragment, TGSI_WRITEMASK_XYZ),
               TGSI_TEXTURE_1D, ureg_src(texel), palette);
   }

   ureg_release_temporary(shader, texel);
   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void *
create_frag_shader_rgba(struct vl_compositor *c)
{
   struct ureg_program *shader;
   struct ureg_src tc, color, sampler;
   struct ureg_dst texel, fragment;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   tc = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTEX, TGSI_INTERPOLATE_LINEAR);
   color = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_COLOR, VS_O_COLOR, TGSI_INTERPOLATE_LINEAR);
   sampler = ureg_DECL_sampler(shader, 0);
   ureg_DECL_sampler_view(shader, 0, TGSI_TEXTURE_2D,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT);
   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   /*
    * fragment = tex(tc, sampler)
    */
   ureg_TEX(shader, texel, TGSI_TEXTURE_2D, tc, sampler);
   ureg_MUL(shader, fragment, ureg_src(texel), color);
   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static void *
create_frag_shader_rgb_yuv(struct vl_compositor *c, bool y)
{
   struct ureg_program *shader;
   struct ureg_src tc, sampler;
   struct ureg_dst texel, fragment;

   struct ureg_src csc[3];
   unsigned i;

   shader = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!shader)
      return false;

   for (i = 0; i < 3; ++i)
      csc[i] = ureg_DECL_constant(shader, i);

   sampler = ureg_DECL_sampler(shader, 0);
   tc = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, VS_O_VTEX, TGSI_INTERPOLATE_LINEAR);
   texel = ureg_DECL_temporary(shader);
   fragment = ureg_DECL_output(shader, TGSI_SEMANTIC_COLOR, 0);

   ureg_TEX(shader, texel, TGSI_TEXTURE_2D, tc, sampler);

   if (y) {
      ureg_DP4(shader, ureg_writemask(fragment, TGSI_WRITEMASK_X), csc[0], ureg_src(texel));
   } else {
      for (i = 0; i < 2; ++i)
         ureg_DP4(shader, ureg_writemask(fragment, TGSI_WRITEMASK_X << i), csc[i + 1], ureg_src(texel));
   }

   ureg_release_temporary(shader, texel);
   ureg_END(shader);

   return ureg_create_shader_and_destroy(shader, c->pipe);
}

static bool
init_shaders(struct vl_compositor *c)
{
   assert(c);

   c->vs = create_vert_shader(c);
   if (!c->vs) {
      debug_printf("Unable to create vertex shader.\n");
      return false;
   }

   c->fs_video_buffer = create_frag_shader_video_buffer(c);
   if (!c->fs_video_buffer) {
      debug_printf("Unable to create YCbCr-to-RGB fragment shader.\n");
      return false;
   }

   c->fs_weave_rgb = create_frag_shader_weave_rgb(c);
   if (!c->fs_weave_rgb) {
      debug_printf("Unable to create YCbCr-to-RGB weave fragment shader.\n");
      return false;
   }

   c->fs_yuv.weave.y = create_frag_shader_deint_yuv(c, true, true);
   c->fs_yuv.weave.uv = create_frag_shader_deint_yuv(c, false, true);
   c->fs_yuv.bob.y = create_frag_shader_deint_yuv(c, true, false);
   c->fs_yuv.bob.uv = create_frag_shader_deint_yuv(c, false, false);
   if (!c->fs_yuv.weave.y || !c->fs_yuv.weave.uv ||
       !c->fs_yuv.bob.y || !c->fs_yuv.bob.uv) {
      debug_printf("Unable to create YCbCr i-to-YCbCr p deint fragment shader.\n");
      return false;
   }

   c->fs_palette.yuv = create_frag_shader_palette(c, true);
   if (!c->fs_palette.yuv) {
      debug_printf("Unable to create YUV-Palette-to-RGB fragment shader.\n");
      return false;
   }

   c->fs_palette.rgb = create_frag_shader_palette(c, false);
   if (!c->fs_palette.rgb) {
      debug_printf("Unable to create RGB-Palette-to-RGB fragment shader.\n");
      return false;
   }

   c->fs_rgba = create_frag_shader_rgba(c);
   if (!c->fs_rgba) {
      debug_printf("Unable to create RGB-to-RGB fragment shader.\n");
      return false;
   }

   c->fs_rgb_yuv.y = create_frag_shader_rgb_yuv(c, true);
   c->fs_rgb_yuv.uv = create_frag_shader_rgb_yuv(c, false);
   if (!c->fs_rgb_yuv.y || !c->fs_rgb_yuv.uv) {
      debug_printf("Unable to create RGB-to-YUV fragment shader.\n");
      return false;
   }

   return true;
}

static void cleanup_shaders(struct vl_compositor *c)
{
   assert(c);

   c->pipe->delete_vs_state(c->pipe, c->vs);
   c->pipe->delete_fs_state(c->pipe, c->fs_video_buffer);
   c->pipe->delete_fs_state(c->pipe, c->fs_weave_rgb);
   c->pipe->delete_fs_state(c->pipe, c->fs_yuv.weave.y);
   c->pipe->delete_fs_state(c->pipe, c->fs_yuv.weave.uv);
   c->pipe->delete_fs_state(c->pipe, c->fs_yuv.bob.y);
   c->pipe->delete_fs_state(c->pipe, c->fs_yuv.bob.uv);
   c->pipe->delete_fs_state(c->pipe, c->fs_palette.yuv);
   c->pipe->delete_fs_state(c->pipe, c->fs_palette.rgb);
   c->pipe->delete_fs_state(c->pipe, c->fs_rgba);
   c->pipe->delete_fs_state(c->pipe, c->fs_rgb_yuv.y);
   c->pipe->delete_fs_state(c->pipe, c->fs_rgb_yuv.uv);
}

static bool
init_pipe_state(struct vl_compositor *c)
{
   struct pipe_rasterizer_state rast;
   struct pipe_sampler_state sampler;
   struct pipe_blend_state blend;
   struct pipe_depth_stencil_alpha_state dsa;
   unsigned i;

   assert(c);

   c->fb_state.nr_cbufs = 1;
   c->fb_state.zsbuf = NULL;

   memset(&sampler, 0, sizeof(sampler));
   sampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   sampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   sampler.wrap_r = PIPE_TEX_WRAP_REPEAT;
   sampler.min_img_filter = PIPE_TEX_FILTER_LINEAR;
   sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
   sampler.mag_img_filter = PIPE_TEX_FILTER_LINEAR;
   sampler.compare_mode = PIPE_TEX_COMPARE_NONE;
   sampler.compare_func = PIPE_FUNC_ALWAYS;
   sampler.normalized_coords = 1;

   c->sampler_linear = c->pipe->create_sampler_state(c->pipe, &sampler);

   sampler.min_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
   c->sampler_nearest = c->pipe->create_sampler_state(c->pipe, &sampler);

   memset(&blend, 0, sizeof blend);
   blend.independent_blend_enable = 0;
   blend.rt[0].blend_enable = 0;
   blend.logicop_enable = 0;
   blend.logicop_func = PIPE_LOGICOP_CLEAR;
   blend.rt[0].colormask = PIPE_MASK_RGBA;
   blend.dither = 0;
   c->blend_clear = c->pipe->create_blend_state(c->pipe, &blend);

   blend.rt[0].blend_enable = 1;
   blend.rt[0].rgb_func = PIPE_BLEND_ADD;
   blend.rt[0].rgb_src_factor = PIPE_BLENDFACTOR_SRC_ALPHA;
   blend.rt[0].rgb_dst_factor = PIPE_BLENDFACTOR_INV_SRC_ALPHA;
   blend.rt[0].alpha_func = PIPE_BLEND_ADD;
   blend.rt[0].alpha_src_factor = PIPE_BLENDFACTOR_ONE;
   blend.rt[0].alpha_dst_factor = PIPE_BLENDFACTOR_ONE;
   c->blend_add = c->pipe->create_blend_state(c->pipe, &blend);

   memset(&rast, 0, sizeof rast);
   rast.flatshade = 0;
   rast.front_ccw = 1;
   rast.cull_face = PIPE_FACE_NONE;
   rast.fill_back = PIPE_POLYGON_MODE_FILL;
   rast.fill_front = PIPE_POLYGON_MODE_FILL;
   rast.scissor = 1;
   rast.line_width = 1;
   rast.point_size_per_vertex = 1;
   rast.offset_units = 1;
   rast.offset_scale = 1;
   rast.half_pixel_center = 1;
   rast.bottom_edge_rule = 1;
   rast.depth_clip = 1;

   c->rast = c->pipe->create_rasterizer_state(c->pipe, &rast);

   memset(&dsa, 0, sizeof dsa);
   dsa.depth.enabled = 0;
   dsa.depth.writemask = 0;
   dsa.depth.func = PIPE_FUNC_ALWAYS;
   for (i = 0; i < 2; ++i) {
      dsa.stencil[i].enabled = 0;
      dsa.stencil[i].func = PIPE_FUNC_ALWAYS;
      dsa.stencil[i].fail_op = PIPE_STENCIL_OP_KEEP;
      dsa.stencil[i].zpass_op = PIPE_STENCIL_OP_KEEP;
      dsa.stencil[i].zfail_op = PIPE_STENCIL_OP_KEEP;
      dsa.stencil[i].valuemask = 0;
      dsa.stencil[i].writemask = 0;
   }
   dsa.alpha.enabled = 0;
   dsa.alpha.func = PIPE_FUNC_ALWAYS;
   dsa.alpha.ref_value = 0;
   c->dsa = c->pipe->create_depth_stencil_alpha_state(c->pipe, &dsa);
   c->pipe->bind_depth_stencil_alpha_state(c->pipe, c->dsa);

   return true;
}

static void cleanup_pipe_state(struct vl_compositor *c)
{
   assert(c);

   /* Asserted in softpipe_delete_fs_state() for some reason */
   c->pipe->bind_vs_state(c->pipe, NULL);
   c->pipe->bind_fs_state(c->pipe, NULL);

   c->pipe->delete_depth_stencil_alpha_state(c->pipe, c->dsa);
   c->pipe->delete_sampler_state(c->pipe, c->sampler_linear);
   c->pipe->delete_sampler_state(c->pipe, c->sampler_nearest);
   c->pipe->delete_blend_state(c->pipe, c->blend_clear);
   c->pipe->delete_blend_state(c->pipe, c->blend_add);
   c->pipe->delete_rasterizer_state(c->pipe, c->rast);
}

static bool
init_buffers(struct vl_compositor *c)
{
   struct pipe_vertex_element vertex_elems[3];

   assert(c);

   /*
    * Create our vertex buffer and vertex buffer elements
    */
   c->vertex_buf.stride = sizeof(struct vertex2f) + sizeof(struct vertex4f) * 2;
   c->vertex_buf.buffer_offset = 0;
   c->vertex_buf.buffer.resource = NULL;
   c->vertex_buf.is_user_buffer = false;

   vertex_elems[0].src_offset = 0;
   vertex_elems[0].instance_divisor = 0;
   vertex_elems[0].vertex_buffer_index = 0;
   vertex_elems[0].src_format = PIPE_FORMAT_R32G32_FLOAT;
   vertex_elems[1].src_offset = sizeof(struct vertex2f);
   vertex_elems[1].instance_divisor = 0;
   vertex_elems[1].vertex_buffer_index = 0;
   vertex_elems[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   vertex_elems[2].src_offset = sizeof(struct vertex2f) + sizeof(struct vertex4f);
   vertex_elems[2].instance_divisor = 0;
   vertex_elems[2].vertex_buffer_index = 0;
   vertex_elems[2].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   c->vertex_elems_state = c->pipe->create_vertex_elements_state(c->pipe, 3, vertex_elems);

   return true;
}

static void
cleanup_buffers(struct vl_compositor *c)
{
   assert(c);

   c->pipe->delete_vertex_elements_state(c->pipe, c->vertex_elems_state);
   pipe_resource_reference(&c->vertex_buf.buffer.resource, NULL);
}

static inline struct u_rect
default_rect(struct vl_compositor_layer *layer)
{
   struct pipe_resource *res = layer->sampler_views[0]->texture;
   struct u_rect rect = { 0, res->width0, 0, res->height0 * res->array_size };
   return rect;
}

static inline struct vertex2f
calc_topleft(struct vertex2f size, struct u_rect rect)
{
   struct vertex2f res = { rect.x0 / size.x, rect.y0 / size.y };
   return res;
}

static inline struct vertex2f
calc_bottomright(struct vertex2f size, struct u_rect rect)
{
   struct vertex2f res = { rect.x1 / size.x, rect.y1 / size.y };
   return res;
}

static inline void
calc_src_and_dst(struct vl_compositor_layer *layer, unsigned width, unsigned height,
                 struct u_rect src, struct u_rect dst)
{
   struct vertex2f size =  { width, height };

   layer->src.tl = calc_topleft(size, src);
   layer->src.br = calc_bottomright(size, src);
   layer->dst.tl = calc_topleft(size, dst);
   layer->dst.br = calc_bottomright(size, dst);
   layer->zw.x = 0.0f;
   layer->zw.y = size.y;
}

static void
gen_rect_verts(struct vertex2f *vb, struct vl_compositor_layer *layer)
{
   struct vertex2f tl, tr, br, bl;

   assert(vb && layer);

   switch (layer->rotate) {
   default:
   case VL_COMPOSITOR_ROTATE_0:
      tl = layer->dst.tl;
      tr.x = layer->dst.br.x;
      tr.y = layer->dst.tl.y;
      br = layer->dst.br;
      bl.x = layer->dst.tl.x;
      bl.y = layer->dst.br.y;
      break;
   case VL_COMPOSITOR_ROTATE_90:
      tl.x = layer->dst.br.x;
      tl.y = layer->dst.tl.y;
      tr = layer->dst.br;
      br.x = layer->dst.tl.x;
      br.y = layer->dst.br.y;
      bl = layer->dst.tl;
      break;
   case VL_COMPOSITOR_ROTATE_180:
      tl = layer->dst.br;
      tr.x = layer->dst.tl.x;
      tr.y = layer->dst.br.y;
      br = layer->dst.tl;
      bl.x = layer->dst.br.x;
      bl.y = layer->dst.tl.y;
      break;
   case VL_COMPOSITOR_ROTATE_270:
      tl.x = layer->dst.tl.x;
      tl.y = layer->dst.br.y;
      tr = layer->dst.tl;
      br.x = layer->dst.br.x;
      br.y = layer->dst.tl.y;
      bl = layer->dst.br;
      break;
   }

   vb[ 0].x = tl.x;
   vb[ 0].y = tl.y;
   vb[ 1].x = layer->src.tl.x;
   vb[ 1].y = layer->src.tl.y;
   vb[ 2] = layer->zw;
   vb[ 3].x = layer->colors[0].x;
   vb[ 3].y = layer->colors[0].y;
   vb[ 4].x = layer->colors[0].z;
   vb[ 4].y = layer->colors[0].w;

   vb[ 5].x = tr.x;
   vb[ 5].y = tr.y;
   vb[ 6].x = layer->src.br.x;
   vb[ 6].y = layer->src.tl.y;
   vb[ 7] = layer->zw;
   vb[ 8].x = layer->colors[1].x;
   vb[ 8].y = layer->colors[1].y;
   vb[ 9].x = layer->colors[1].z;
   vb[ 9].y = layer->colors[1].w;

   vb[10].x = br.x;
   vb[10].y = br.y;
   vb[11].x = layer->src.br.x;
   vb[11].y = layer->src.br.y;
   vb[12] = layer->zw;
   vb[13].x = layer->colors[2].x;
   vb[13].y = layer->colors[2].y;
   vb[14].x = layer->colors[2].z;
   vb[14].y = layer->colors[2].w;

   vb[15].x = bl.x;
   vb[15].y = bl.y;
   vb[16].x = layer->src.tl.x;
   vb[16].y = layer->src.br.y;
   vb[17] = layer->zw;
   vb[18].x = layer->colors[3].x;
   vb[18].y = layer->colors[3].y;
   vb[19].x = layer->colors[3].z;
   vb[19].y = layer->colors[3].w;
}

static inline struct u_rect
calc_drawn_area(struct vl_compositor_state *s, struct vl_compositor_layer *layer)
{
   struct vertex2f tl, br;
   struct u_rect result;

   assert(s && layer);

   // rotate
   switch (layer->rotate) {
   default:
   case VL_COMPOSITOR_ROTATE_0:
      tl = layer->dst.tl;
      br = layer->dst.br;
      break;
   case VL_COMPOSITOR_ROTATE_90:
      tl.x = layer->dst.br.x;
      tl.y = layer->dst.tl.y;
      br.x = layer->dst.tl.x;
      br.y = layer->dst.br.y;
      break;
   case VL_COMPOSITOR_ROTATE_180:
      tl = layer->dst.br;
      br = layer->dst.tl;
      break;
   case VL_COMPOSITOR_ROTATE_270:
      tl.x = layer->dst.tl.x;
      tl.y = layer->dst.br.y;
      br.x = layer->dst.br.x;
      br.y = layer->dst.tl.y;
      break;
   }

   // scale
   result.x0 = tl.x * layer->viewport.scale[0] + layer->viewport.translate[0];
   result.y0 = tl.y * layer->viewport.scale[1] + layer->viewport.translate[1];
   result.x1 = br.x * layer->viewport.scale[0] + layer->viewport.translate[0];
   result.y1 = br.y * layer->viewport.scale[1] + layer->viewport.translate[1];

   // and clip
   result.x0 = MAX2(result.x0, s->scissor.minx);
   result.y0 = MAX2(result.y0, s->scissor.miny);
   result.x1 = MIN2(result.x1, s->scissor.maxx);
   result.y1 = MIN2(result.y1, s->scissor.maxy);
   return result;
}

static void
gen_vertex_data(struct vl_compositor *c, struct vl_compositor_state *s, struct u_rect *dirty)
{
   struct vertex2f *vb;
   unsigned i;

   assert(c);

   /* Allocate new memory for vertices. */
   u_upload_alloc(c->pipe->stream_uploader, 0,
                  c->vertex_buf.stride * VL_COMPOSITOR_MAX_LAYERS * 4, /* size */
                  4, /* alignment */
                  &c->vertex_buf.buffer_offset, &c->vertex_buf.buffer.resource,
                  (void**)&vb);

   for (i = 0; i < VL_COMPOSITOR_MAX_LAYERS; i++) {
      if (s->used_layers & (1 << i)) {
         struct vl_compositor_layer *layer = &s->layers[i];
         gen_rect_verts(vb, layer);
         vb += 20;

         if (!layer->viewport_valid) {
            layer->viewport.scale[0] = c->fb_state.width;
            layer->viewport.scale[1] = c->fb_state.height;
            layer->viewport.translate[0] = 0;
            layer->viewport.translate[1] = 0;
         }

         if (dirty && layer->clearing) {
            struct u_rect drawn = calc_drawn_area(s, layer);
            if (
             dirty->x0 >= drawn.x0 &&
             dirty->y0 >= drawn.y0 &&
             dirty->x1 <= drawn.x1 &&
             dirty->y1 <= drawn.y1) {

               // We clear the dirty area anyway, no need for clear_render_target
               dirty->x0 = dirty->y0 = MAX_DIRTY;
               dirty->x1 = dirty->y1 = MIN_DIRTY;
            }
         }
      }
   }

   u_upload_unmap(c->pipe->stream_uploader);
}

static void
draw_layers(struct vl_compositor *c, struct vl_compositor_state *s, struct u_rect *dirty)
{
   unsigned vb_index, i;

   assert(c);

   for (i = 0, vb_index = 0; i < VL_COMPOSITOR_MAX_LAYERS; ++i) {
      if (s->used_layers & (1 << i)) {
         struct vl_compositor_layer *layer = &s->layers[i];
         struct pipe_sampler_view **samplers = &layer->sampler_views[0];
         unsigned num_sampler_views = !samplers[1] ? 1 : !samplers[2] ? 2 : 3;
         void *blend = layer->blend ? layer->blend : i ? c->blend_add : c->blend_clear;

         c->pipe->bind_blend_state(c->pipe, blend);
         c->pipe->set_viewport_states(c->pipe, 0, 1, &layer->viewport);
         c->pipe->bind_fs_state(c->pipe, layer->fs);
         c->pipe->bind_sampler_states(c->pipe, PIPE_SHADER_FRAGMENT, 0,
                                      num_sampler_views, layer->samplers);
         c->pipe->set_sampler_views(c->pipe, PIPE_SHADER_FRAGMENT, 0,
                                    num_sampler_views, samplers);

         util_draw_arrays(c->pipe, PIPE_PRIM_QUADS, vb_index * 4, 4);
         vb_index++;

         if (dirty) {
            // Remember the currently drawn area as dirty for the next draw command
            struct u_rect drawn = calc_drawn_area(s, layer);
            dirty->x0 = MIN2(drawn.x0, dirty->x0);
            dirty->y0 = MIN2(drawn.y0, dirty->y0);
            dirty->x1 = MAX2(drawn.x1, dirty->x1);
            dirty->y1 = MAX2(drawn.y1, dirty->y1);
         }
      }
   }
}

static void
set_yuv_layer(struct vl_compositor_state *s, struct vl_compositor *c,
              unsigned layer, struct pipe_video_buffer *buffer,
              struct u_rect *src_rect, struct u_rect *dst_rect,
              bool y, enum vl_compositor_deinterlace deinterlace)
{
   struct pipe_sampler_view **sampler_views;
   float half_a_line;
   unsigned i;

   assert(s && c && buffer);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->used_layers |= 1 << layer;
   sampler_views = buffer->get_sampler_view_components(buffer);
   for (i = 0; i < 3; ++i) {
      s->layers[layer].samplers[i] = c->sampler_linear;
      pipe_sampler_view_reference(&s->layers[layer].sampler_views[i], sampler_views[i]);
   }

   calc_src_and_dst(&s->layers[layer], buffer->width, buffer->height,
                    src_rect ? *src_rect : default_rect(&s->layers[layer]),
                    dst_rect ? *dst_rect : default_rect(&s->layers[layer]));

   half_a_line = 0.5f / s->layers[layer].zw.y;

   switch(deinterlace) {
   case VL_COMPOSITOR_BOB_TOP:
      s->layers[layer].zw.x = 0.0f;
      s->layers[layer].src.tl.y += half_a_line;
      s->layers[layer].src.br.y += half_a_line;
      s->layers[layer].fs = (y) ? c->fs_yuv.bob.y : c->fs_yuv.bob.uv;
      break;

   case VL_COMPOSITOR_BOB_BOTTOM:
      s->layers[layer].zw.x = 1.0f;
      s->layers[layer].src.tl.y -= half_a_line;
      s->layers[layer].src.br.y -= half_a_line;
      s->layers[layer].fs = (y) ? c->fs_yuv.bob.y : c->fs_yuv.bob.uv;
      break;

   default:
      s->layers[layer].fs = (y) ? c->fs_yuv.weave.y : c->fs_yuv.weave.uv;
      break;
   }
}

static void
set_rgb_to_yuv_layer(struct vl_compositor_state *s, struct vl_compositor *c,
                     unsigned layer, struct pipe_sampler_view *v,
                     struct u_rect *src_rect, struct u_rect *dst_rect, bool y)
{
   vl_csc_matrix csc_matrix;

   assert(s && c && v);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->used_layers |= 1 << layer;

   s->layers[layer].fs = y? c->fs_rgb_yuv.y : c->fs_rgb_yuv.uv;

   vl_csc_get_matrix(VL_CSC_COLOR_STANDARD_BT_709_REV, NULL, false, &csc_matrix);
   vl_compositor_set_csc_matrix(s, (const vl_csc_matrix *)&csc_matrix, 1.0f, 0.0f);

   s->layers[layer].samplers[0] = c->sampler_linear;
   s->layers[layer].samplers[1] = NULL;
   s->layers[layer].samplers[2] = NULL;

   pipe_sampler_view_reference(&s->layers[layer].sampler_views[0], v);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[1], NULL);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[2], NULL);

   calc_src_and_dst(&s->layers[layer], v->texture->width0, v->texture->height0,
                    src_rect ? *src_rect : default_rect(&s->layers[layer]),
                    dst_rect ? *dst_rect : default_rect(&s->layers[layer]));
}

void
vl_compositor_reset_dirty_area(struct u_rect *dirty)
{
   assert(dirty);

   dirty->x0 = dirty->y0 = MIN_DIRTY;
   dirty->x1 = dirty->y1 = MAX_DIRTY;
}

void
vl_compositor_set_clear_color(struct vl_compositor_state *s, union pipe_color_union *color)
{
   assert(s);
   assert(color);

   s->clear_color = *color;
}

void
vl_compositor_get_clear_color(struct vl_compositor_state *s, union pipe_color_union *color)
{
   assert(s);
   assert(color);

   *color = s->clear_color;
}

void
vl_compositor_clear_layers(struct vl_compositor_state *s)
{
   unsigned i, j;

   assert(s);

   s->used_layers = 0;
   for ( i = 0; i < VL_COMPOSITOR_MAX_LAYERS; ++i) {
      struct vertex4f v_one = { 1.0f, 1.0f, 1.0f, 1.0f };
      s->layers[i].clearing = i ? false : true;
      s->layers[i].blend = NULL;
      s->layers[i].fs = NULL;
      s->layers[i].viewport.scale[2] = 1;
      s->layers[i].viewport.translate[2] = 0;
      s->layers[i].rotate = VL_COMPOSITOR_ROTATE_0;

      for ( j = 0; j < 3; j++)
         pipe_sampler_view_reference(&s->layers[i].sampler_views[j], NULL);
      for ( j = 0; j < 4; ++j)
         s->layers[i].colors[j] = v_one;
   }
}

void
vl_compositor_cleanup(struct vl_compositor *c)
{
   assert(c);

   cleanup_buffers(c);
   cleanup_shaders(c);
   cleanup_pipe_state(c);
}

bool
vl_compositor_set_csc_matrix(struct vl_compositor_state *s,
                             vl_csc_matrix const *matrix,
                             float luma_min, float luma_max)
{
   struct pipe_transfer *buf_transfer;

   assert(s);

   float *ptr = pipe_buffer_map(s->pipe, s->csc_matrix,
                               PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE,
                               &buf_transfer);

   if (!ptr)
      return false;

   memcpy(ptr, matrix, sizeof(vl_csc_matrix));

   ptr += sizeof(vl_csc_matrix)/sizeof(float);
   ptr[0] = luma_min;
   ptr[1] = luma_max;

   pipe_buffer_unmap(s->pipe, buf_transfer);

   return true;
}

void
vl_compositor_set_dst_clip(struct vl_compositor_state *s, struct u_rect *dst_clip)
{
   assert(s);

   s->scissor_valid = dst_clip != NULL;
   if (dst_clip) {
      s->scissor.minx = dst_clip->x0;
      s->scissor.miny = dst_clip->y0;
      s->scissor.maxx = dst_clip->x1;
      s->scissor.maxy = dst_clip->y1;
   }
}

void
vl_compositor_set_layer_blend(struct vl_compositor_state *s,
                              unsigned layer, void *blend,
                              bool is_clearing)
{
   assert(s && blend);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->layers[layer].clearing = is_clearing;
   s->layers[layer].blend = blend;
}

void
vl_compositor_set_layer_dst_area(struct vl_compositor_state *s,
                                 unsigned layer, struct u_rect *dst_area)
{
   assert(s);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->layers[layer].viewport_valid = dst_area != NULL;
   if (dst_area) {
      s->layers[layer].viewport.scale[0] = dst_area->x1 - dst_area->x0;
      s->layers[layer].viewport.scale[1] = dst_area->y1 - dst_area->y0;
      s->layers[layer].viewport.translate[0] = dst_area->x0;
      s->layers[layer].viewport.translate[1] = dst_area->y0;
   }
}

void
vl_compositor_set_buffer_layer(struct vl_compositor_state *s,
                               struct vl_compositor *c,
                               unsigned layer,
                               struct pipe_video_buffer *buffer,
                               struct u_rect *src_rect,
                               struct u_rect *dst_rect,
                               enum vl_compositor_deinterlace deinterlace)
{
   struct pipe_sampler_view **sampler_views;
   unsigned i;

   assert(s && c && buffer);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->used_layers |= 1 << layer;
   sampler_views = buffer->get_sampler_view_components(buffer);
   for (i = 0; i < 3; ++i) {
      s->layers[layer].samplers[i] = c->sampler_linear;
      pipe_sampler_view_reference(&s->layers[layer].sampler_views[i], sampler_views[i]);
   }

   calc_src_and_dst(&s->layers[layer], buffer->width, buffer->height,
                    src_rect ? *src_rect : default_rect(&s->layers[layer]),
                    dst_rect ? *dst_rect : default_rect(&s->layers[layer]));

   if (buffer->interlaced) {
      float half_a_line = 0.5f / s->layers[layer].zw.y;
      switch(deinterlace) {
      case VL_COMPOSITOR_WEAVE:
         s->layers[layer].fs = c->fs_weave_rgb;
         break;

      case VL_COMPOSITOR_BOB_TOP:
         s->layers[layer].zw.x = 0.0f;
         s->layers[layer].src.tl.y += half_a_line;
         s->layers[layer].src.br.y += half_a_line;
         s->layers[layer].fs = c->fs_video_buffer;
         break;

      case VL_COMPOSITOR_BOB_BOTTOM:
         s->layers[layer].zw.x = 1.0f;
         s->layers[layer].src.tl.y -= half_a_line;
         s->layers[layer].src.br.y -= half_a_line;
         s->layers[layer].fs = c->fs_video_buffer;
         break;
      }

   } else
      s->layers[layer].fs = c->fs_video_buffer;
}

void
vl_compositor_set_palette_layer(struct vl_compositor_state *s,
                                struct vl_compositor *c,
                                unsigned layer,
                                struct pipe_sampler_view *indexes,
                                struct pipe_sampler_view *palette,
                                struct u_rect *src_rect,
                                struct u_rect *dst_rect,
                                bool include_color_conversion)
{
   assert(s && c && indexes && palette);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->used_layers |= 1 << layer;

   s->layers[layer].fs = include_color_conversion ?
      c->fs_palette.yuv : c->fs_palette.rgb;

   s->layers[layer].samplers[0] = c->sampler_linear;
   s->layers[layer].samplers[1] = c->sampler_nearest;
   s->layers[layer].samplers[2] = NULL;
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[0], indexes);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[1], palette);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[2], NULL);
   calc_src_and_dst(&s->layers[layer], indexes->texture->width0, indexes->texture->height0,
                    src_rect ? *src_rect : default_rect(&s->layers[layer]),
                    dst_rect ? *dst_rect : default_rect(&s->layers[layer]));
}

void
vl_compositor_set_rgba_layer(struct vl_compositor_state *s,
                             struct vl_compositor *c,
                             unsigned layer,
                             struct pipe_sampler_view *rgba,
                             struct u_rect *src_rect,
                             struct u_rect *dst_rect,
                             struct vertex4f *colors)
{
   unsigned i;

   assert(s && c && rgba);

   assert(layer < VL_COMPOSITOR_MAX_LAYERS);

   s->used_layers |= 1 << layer;
   s->layers[layer].fs = c->fs_rgba;
   s->layers[layer].samplers[0] = c->sampler_linear;
   s->layers[layer].samplers[1] = NULL;
   s->layers[layer].samplers[2] = NULL;
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[0], rgba);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[1], NULL);
   pipe_sampler_view_reference(&s->layers[layer].sampler_views[2], NULL);
   calc_src_and_dst(&s->layers[layer], rgba->texture->width0, rgba->texture->height0,
                    src_rect ? *src_rect : default_rect(&s->layers[layer]),
                    dst_rect ? *dst_rect : default_rect(&s->layers[layer]));

   if (colors)
      for (i = 0; i < 4; ++i)
         s->layers[layer].colors[i] = colors[i];
}

void
vl_compositor_set_layer_rotation(struct vl_compositor_state *s,
                                 unsigned layer,
                                 enum vl_compositor_rotation rotate)
{
   assert(s);
   assert(layer < VL_COMPOSITOR_MAX_LAYERS);
   s->layers[layer].rotate = rotate;
}

void
vl_compositor_yuv_deint_full(struct vl_compositor_state *s,
                             struct vl_compositor *c,
                             struct pipe_video_buffer *src,
                             struct pipe_video_buffer *dst,
                             struct u_rect *src_rect,
                             struct u_rect *dst_rect,
                             enum vl_compositor_deinterlace deinterlace)
{
   struct pipe_surface **dst_surfaces;

   dst_surfaces = dst->get_surfaces(dst);
   vl_compositor_clear_layers(s);

   set_yuv_layer(s, c, 0, src, src_rect, NULL, true, deinterlace);
   vl_compositor_set_layer_dst_area(s, 0, dst_rect);
   vl_compositor_render(s, c, dst_surfaces[0], NULL, false);

   if (dst_rect) {
      dst_rect->x1 /= 2;
      dst_rect->y1 /= 2;
   }

   set_yuv_layer(s, c, 0, src, src_rect, NULL, false, deinterlace);
   vl_compositor_set_layer_dst_area(s, 0, dst_rect);
   vl_compositor_render(s, c, dst_surfaces[1], NULL, false);

   s->pipe->flush(s->pipe, NULL, 0);
}

void
vl_compositor_convert_rgb_to_yuv(struct vl_compositor_state *s,
                                 struct vl_compositor *c,
                                 unsigned layer,
                                 struct pipe_resource *src_res,
                                 struct pipe_video_buffer *dst,
                                 struct u_rect *src_rect,
                                 struct u_rect *dst_rect)
{
   struct pipe_sampler_view *sv, sv_templ;
   struct pipe_surface **dst_surfaces;

   dst_surfaces = dst->get_surfaces(dst);

   memset(&sv_templ, 0, sizeof(sv_templ));
   u_sampler_view_default_template(&sv_templ, src_res, src_res->format);
   sv = s->pipe->create_sampler_view(s->pipe, src_res, &sv_templ);

   vl_compositor_clear_layers(s);

   set_rgb_to_yuv_layer(s, c, 0, sv, src_rect, NULL, true);
   vl_compositor_set_layer_dst_area(s, 0, dst_rect);
   vl_compositor_render(s, c, dst_surfaces[0], NULL, false);

   if (dst_rect) {
      dst_rect->x1 /= 2;
      dst_rect->y1 /= 2;
   }

   set_rgb_to_yuv_layer(s, c, 0, sv, src_rect, NULL, false);
   vl_compositor_set_layer_dst_area(s, 0, dst_rect);
   vl_compositor_render(s, c, dst_surfaces[1], NULL, false);
   pipe_sampler_view_reference(&sv, NULL);

   s->pipe->flush(s->pipe, NULL, 0);
}

void
vl_compositor_render(struct vl_compositor_state *s,
                     struct vl_compositor       *c,
                     struct pipe_surface        *dst_surface,
                     struct u_rect              *dirty_area,
                     bool                        clear_dirty)
{
   assert(c);
   assert(dst_surface);

   c->fb_state.width = dst_surface->width;
   c->fb_state.height = dst_surface->height;
   c->fb_state.cbufs[0] = dst_surface;
   
   if (!s->scissor_valid) {
      s->scissor.minx = 0;
      s->scissor.miny = 0;
      s->scissor.maxx = dst_surface->width;
      s->scissor.maxy = dst_surface->height;
   }
   c->pipe->set_scissor_states(c->pipe, 0, 1, &s->scissor);

   gen_vertex_data(c, s, dirty_area);

   if (clear_dirty && dirty_area &&
       (dirty_area->x0 < dirty_area->x1 || dirty_area->y0 < dirty_area->y1)) {

      c->pipe->clear_render_target(c->pipe, dst_surface, &s->clear_color,
                                   0, 0, dst_surface->width, dst_surface->height, false);
      dirty_area->x0 = dirty_area->y0 = MAX_DIRTY;
      dirty_area->x1 = dirty_area->y1 = MIN_DIRTY;
   }

   c->pipe->set_framebuffer_state(c->pipe, &c->fb_state);
   c->pipe->bind_vs_state(c->pipe, c->vs);
   c->pipe->set_vertex_buffers(c->pipe, 0, 1, &c->vertex_buf);
   c->pipe->bind_vertex_elements_state(c->pipe, c->vertex_elems_state);
   pipe_set_constant_buffer(c->pipe, PIPE_SHADER_FRAGMENT, 0, s->csc_matrix);
   c->pipe->bind_rasterizer_state(c->pipe, c->rast);

   draw_layers(c, s, dirty_area);
}

bool
vl_compositor_init(struct vl_compositor *c, struct pipe_context *pipe)
{
   assert(c);

   memset(c, 0, sizeof(*c));

   c->pipe = pipe;

   if (!init_pipe_state(c)) {
      return false;
   }

   if (!init_shaders(c)) {
      cleanup_pipe_state(c);
      return false;
   }

   if (!init_buffers(c)) {
      cleanup_shaders(c);
      cleanup_pipe_state(c);
      return false;
   }

   return true;
}

bool
vl_compositor_init_state(struct vl_compositor_state *s, struct pipe_context *pipe)
{
   vl_csc_matrix csc_matrix;

   assert(s);

   memset(s, 0, sizeof(*s));

   s->pipe = pipe;

   s->clear_color.f[0] = s->clear_color.f[1] = 0.0f;
   s->clear_color.f[2] = s->clear_color.f[3] = 0.0f;

   /*
    * Create our fragment shader's constant buffer
    * Const buffer contains the color conversion matrix and bias vectors
    */
   /* XXX: Create with IMMUTABLE/STATIC... although it does change every once in a long while... */
   s->csc_matrix = pipe_buffer_create
   (
      pipe->screen,
      PIPE_BIND_CONSTANT_BUFFER,
      PIPE_USAGE_DEFAULT,
      sizeof(csc_matrix) + 2*sizeof(float)
   );

   if (!s->csc_matrix)
      return false;

   vl_compositor_clear_layers(s);

   vl_csc_get_matrix(VL_CSC_COLOR_STANDARD_IDENTITY, NULL, true, &csc_matrix);
   if (!vl_compositor_set_csc_matrix(s, (const vl_csc_matrix *)&csc_matrix, 1.0f, 0.0f))
      return false;

   return true;
}

void
vl_compositor_cleanup_state(struct vl_compositor_state *s)
{
   assert(s);

   vl_compositor_clear_layers(s);
   pipe_resource_reference(&s->csc_matrix, NULL);
}
