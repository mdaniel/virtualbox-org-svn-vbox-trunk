/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 * Copyright 2009 Marek Olšák <maraeo@gmail.com>
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

/**
 * @file
 * Simple vertex/fragment shader generators.
 *  
 * @author Brian Paul
           Marek Olšák
 */


#include "pipe/p_context.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/p_state.h"
#include "util/u_simple_shaders.h"
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "util/u_string.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_strings.h"
#include "tgsi/tgsi_ureg.h"
#include "tgsi/tgsi_text.h"
#include <stdio.h> /* include last */



/**
 * Make simple vertex pass-through shader.
 * \param num_attribs  number of attributes to pass through
 * \param semantic_names  array of semantic names for each attribute
 * \param semantic_indexes  array of semantic indexes for each attribute
 */
void *
util_make_vertex_passthrough_shader(struct pipe_context *pipe,
                                    uint num_attribs,
                                    const uint *semantic_names,
                                    const uint *semantic_indexes,
                                    bool window_space)
{
   return util_make_vertex_passthrough_shader_with_so(pipe, num_attribs,
                                                      semantic_names,
                                                      semantic_indexes,
                                                      window_space, false, NULL);
}

void *
util_make_vertex_passthrough_shader_with_so(struct pipe_context *pipe,
                                    uint num_attribs,
                                    const uint *semantic_names,
                                    const uint *semantic_indexes,
                                    bool window_space, bool layered,
				    const struct pipe_stream_output_info *so)
{
   struct ureg_program *ureg;
   uint i;

   ureg = ureg_create( PIPE_SHADER_VERTEX );
   if (!ureg)
      return NULL;

   if (window_space)
      ureg_property(ureg, TGSI_PROPERTY_VS_WINDOW_SPACE_POSITION, TRUE);

   for (i = 0; i < num_attribs; i++) {
      struct ureg_src src;
      struct ureg_dst dst;

      src = ureg_DECL_vs_input( ureg, i );
      
      dst = ureg_DECL_output( ureg,
                              semantic_names[i],
                              semantic_indexes[i]);
      
      ureg_MOV( ureg, dst, src );
   }

   if (layered) {
      struct ureg_src instance_id =
         ureg_DECL_system_value(ureg, TGSI_SEMANTIC_INSTANCEID, 0);
      struct ureg_dst layer = ureg_DECL_output(ureg, TGSI_SEMANTIC_LAYER, 0);

      ureg_MOV(ureg, ureg_writemask(layer, TGSI_WRITEMASK_X),
               ureg_scalar(instance_id, TGSI_SWIZZLE_X));
   }

   ureg_END( ureg );

   return ureg_create_shader_with_so_and_destroy( ureg, pipe, so );
}


void *util_make_layered_clear_vertex_shader(struct pipe_context *pipe)
{
   const unsigned semantic_names[] = {TGSI_SEMANTIC_POSITION,
                                      TGSI_SEMANTIC_GENERIC};
   const unsigned semantic_indices[] = {0, 0};

   return util_make_vertex_passthrough_shader_with_so(pipe, 2, semantic_names,
                                                      semantic_indices, false,
                                                      true, NULL);
}

/**
 * Takes position and color, and outputs position, color, and instance id.
 */
void *util_make_layered_clear_helper_vertex_shader(struct pipe_context *pipe)
{
   static const char text[] =
         "VERT\n"
         "DCL IN[0]\n"
         "DCL IN[1]\n"
         "DCL SV[0], INSTANCEID\n"
         "DCL OUT[0], POSITION\n"
         "DCL OUT[1], GENERIC[0]\n"
         "DCL OUT[2], GENERIC[1]\n"

         "MOV OUT[0], IN[0]\n"
         "MOV OUT[1], IN[1]\n"
         "MOV OUT[2].x, SV[0].xxxx\n"
         "END\n";
   struct tgsi_token tokens[1000];
   struct pipe_shader_state state;

   if (!tgsi_text_translate(text, tokens, ARRAY_SIZE(tokens))) {
      assert(0);
      return NULL;
   }
   pipe_shader_state_from_tgsi(&state, tokens);
   return pipe->create_vs_state(pipe, &state);
}

/**
 * Takes position, color, and target layer, and emits vertices on that target
 * layer, with the specified color.
 */
void *util_make_layered_clear_geometry_shader(struct pipe_context *pipe)
{
   static const char text[] =
      "GEOM\n"
      "PROPERTY GS_INPUT_PRIMITIVE TRIANGLES\n"
      "PROPERTY GS_OUTPUT_PRIMITIVE TRIANGLE_STRIP\n"
      "PROPERTY GS_MAX_OUTPUT_VERTICES 3\n"
      "PROPERTY GS_INVOCATIONS 1\n"
      "DCL IN[][0], POSITION\n" /* position */
      "DCL IN[][1], GENERIC[0]\n" /* color */
      "DCL IN[][2], GENERIC[1]\n" /* vs invocation */
      "DCL OUT[0], POSITION\n"
      "DCL OUT[1], GENERIC[0]\n"
      "DCL OUT[2], LAYER\n"
      "IMM[0] INT32 {0, 0, 0, 0}\n"

      "MOV OUT[0], IN[0][0]\n"
      "MOV OUT[1], IN[0][1]\n"
      "MOV OUT[2].x, IN[0][2].xxxx\n"
      "EMIT IMM[0].xxxx\n"
      "MOV OUT[0], IN[1][0]\n"
      "MOV OUT[1], IN[1][1]\n"
      "MOV OUT[2].x, IN[1][2].xxxx\n"
      "EMIT IMM[0].xxxx\n"
      "MOV OUT[0], IN[2][0]\n"
      "MOV OUT[1], IN[2][1]\n"
      "MOV OUT[2].x, IN[2][2].xxxx\n"
      "EMIT IMM[0].xxxx\n"
      "END\n";
   struct tgsi_token tokens[1000];
   struct pipe_shader_state state;

   if (!tgsi_text_translate(text, tokens, ARRAY_SIZE(tokens))) {
      assert(0);
      return NULL;
   }
   pipe_shader_state_from_tgsi(&state, tokens);
   return pipe->create_gs_state(pipe, &state);
}

static void
ureg_load_tex(struct ureg_program *ureg, struct ureg_dst out,
              struct ureg_src coord, struct ureg_src sampler,
              unsigned tex_target, bool load_level_zero, bool use_txf)
{
   if (use_txf) {
      struct ureg_dst temp = ureg_DECL_temporary(ureg);

      ureg_F2I(ureg, temp, coord);

      if (load_level_zero)
         ureg_TXF_LZ(ureg, out, tex_target, ureg_src(temp), sampler);
      else
         ureg_TXF(ureg, out, tex_target, ureg_src(temp), sampler);
   } else {
      if (load_level_zero)
         ureg_TEX_LZ(ureg, out, tex_target, coord, sampler);
      else
         ureg_TEX(ureg, out, tex_target, coord, sampler);
   }
}

/**
 * Make simple fragment texture shader:
 *  IMM {0,0,0,1}                         // (if writemask != 0xf)
 *  MOV TEMP[0], IMM[0]                   // (if writemask != 0xf)
 *  TEX TEMP[0].writemask, IN[0], SAMP[0], 2D;
 *   .. optional SINT <-> UINT clamping ..
 *  MOV OUT[0], TEMP[0]
 *  END;
 *
 * \param tex_target  one of PIPE_TEXTURE_x
 * \parma interp_mode  either TGSI_INTERPOLATE_LINEAR or PERSPECTIVE
 * \param writemask  mask of TGSI_WRITEMASK_x
 */
void *
util_make_fragment_tex_shader_writemask(struct pipe_context *pipe,
                                        unsigned tex_target,
                                        unsigned interp_mode,
                                        unsigned writemask,
                                        enum tgsi_return_type stype,
                                        enum tgsi_return_type dtype,
                                        bool load_level_zero,
                                        bool use_txf)
{
   struct ureg_program *ureg;
   struct ureg_src sampler;
   struct ureg_src tex;
   struct ureg_dst temp;
   struct ureg_dst out;

   assert((stype == TGSI_RETURN_TYPE_FLOAT) == (dtype == TGSI_RETURN_TYPE_FLOAT));
   assert(interp_mode == TGSI_INTERPOLATE_LINEAR ||
          interp_mode == TGSI_INTERPOLATE_PERSPECTIVE);

   ureg = ureg_create( PIPE_SHADER_FRAGMENT );
   if (!ureg)
      return NULL;
   
   sampler = ureg_DECL_sampler( ureg, 0 );

   ureg_DECL_sampler_view(ureg, 0, tex_target, stype, stype, stype, stype);

   tex = ureg_DECL_fs_input( ureg, 
                             TGSI_SEMANTIC_GENERIC, 0, 
                             interp_mode );

   out = ureg_DECL_output( ureg, 
                           TGSI_SEMANTIC_COLOR,
                           0 );

   temp = ureg_DECL_temporary(ureg);

   if (writemask != TGSI_WRITEMASK_XYZW) {
      struct ureg_src imm = ureg_imm4f( ureg, 0, 0, 0, 1 );

      ureg_MOV( ureg, out, imm );
   }

   if (tex_target == TGSI_TEXTURE_BUFFER)
      ureg_TXF(ureg,
               ureg_writemask(temp, writemask),
               tex_target, tex, sampler);
   else
      ureg_load_tex(ureg, ureg_writemask(temp, writemask), tex, sampler,
                    tex_target, load_level_zero, use_txf);

   if (stype != dtype) {
      if (stype == TGSI_RETURN_TYPE_SINT) {
         assert(dtype == TGSI_RETURN_TYPE_UINT);

         ureg_IMAX(ureg, temp, ureg_src(temp), ureg_imm1i(ureg, 0));
      } else {
         assert(stype == TGSI_RETURN_TYPE_UINT);
         assert(dtype == TGSI_RETURN_TYPE_SINT);

         ureg_UMIN(ureg, temp, ureg_src(temp), ureg_imm1u(ureg, (1u << 31) - 1));
      }
   }

   ureg_MOV(ureg, out, ureg_src(temp));

   ureg_END( ureg );

   return ureg_create_shader_and_destroy( ureg, pipe );
}


/**
 * Make a simple fragment shader that sets the output color to a color
 * taken from a texture.
 * \param tex_target  one of PIPE_TEXTURE_x
 */
void *
util_make_fragment_tex_shader(struct pipe_context *pipe, unsigned tex_target,
                              unsigned interp_mode,
                              enum tgsi_return_type stype,
                              enum tgsi_return_type dtype,
                              bool load_level_zero,
                              bool use_txf)
{
   return util_make_fragment_tex_shader_writemask( pipe,
                                                   tex_target,
                                                   interp_mode,
                                                   TGSI_WRITEMASK_XYZW,
                                                   stype, dtype, load_level_zero,
                                                   use_txf);
}


/**
 * Make a simple fragment texture shader which reads an X component from
 * a texture and writes it as depth.
 */
void *
util_make_fragment_tex_shader_writedepth(struct pipe_context *pipe,
                                         unsigned tex_target,
                                         unsigned interp_mode,
                                         bool load_level_zero,
                                         bool use_txf)
{
   struct ureg_program *ureg;
   struct ureg_src sampler;
   struct ureg_src tex;
   struct ureg_dst out, depth;
   struct ureg_src imm;

   ureg = ureg_create( PIPE_SHADER_FRAGMENT );
   if (!ureg)
      return NULL;

   sampler = ureg_DECL_sampler( ureg, 0 );

   ureg_DECL_sampler_view(ureg, 0, tex_target,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT);

   tex = ureg_DECL_fs_input( ureg,
                             TGSI_SEMANTIC_GENERIC, 0,
                             interp_mode );

   out = ureg_DECL_output( ureg,
                           TGSI_SEMANTIC_COLOR,
                           0 );

   depth = ureg_DECL_output( ureg,
                             TGSI_SEMANTIC_POSITION,
                             0 );

   imm = ureg_imm4f( ureg, 0, 0, 0, 1 );

   ureg_MOV( ureg, out, imm );

   ureg_load_tex(ureg, ureg_writemask(depth, TGSI_WRITEMASK_Z), tex, sampler,
                 tex_target, load_level_zero, use_txf);
   ureg_END( ureg );

   return ureg_create_shader_and_destroy( ureg, pipe );
}


/**
 * Make a simple fragment texture shader which reads the texture unit 0 and 1
 * and writes it as depth and stencil, respectively.
 */
void *
util_make_fragment_tex_shader_writedepthstencil(struct pipe_context *pipe,
                                                unsigned tex_target,
                                                unsigned interp_mode,
                                                bool load_level_zero,
                                                bool use_txf)
{
   struct ureg_program *ureg;
   struct ureg_src depth_sampler, stencil_sampler;
   struct ureg_src tex;
   struct ureg_dst out, depth, stencil;
   struct ureg_src imm;

   ureg = ureg_create( PIPE_SHADER_FRAGMENT );
   if (!ureg)
      return NULL;

   depth_sampler = ureg_DECL_sampler( ureg, 0 );
   ureg_DECL_sampler_view(ureg, 0, tex_target,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT,
                          TGSI_RETURN_TYPE_FLOAT);
   stencil_sampler = ureg_DECL_sampler( ureg, 1 );
   ureg_DECL_sampler_view(ureg, 0, tex_target,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT);

   tex = ureg_DECL_fs_input( ureg,
                             TGSI_SEMANTIC_GENERIC, 0,
                             interp_mode );

   out = ureg_DECL_output( ureg,
                           TGSI_SEMANTIC_COLOR,
                           0 );

   depth = ureg_DECL_output( ureg,
                             TGSI_SEMANTIC_POSITION,
                             0 );

   stencil = ureg_DECL_output( ureg,
                             TGSI_SEMANTIC_STENCIL,
                             0 );

   imm = ureg_imm4f( ureg, 0, 0, 0, 1 );

   ureg_MOV( ureg, out, imm );

   ureg_load_tex(ureg, ureg_writemask(depth, TGSI_WRITEMASK_Z), tex,
                 depth_sampler, tex_target, load_level_zero, use_txf);
   ureg_load_tex(ureg, ureg_writemask(stencil, TGSI_WRITEMASK_Y), tex,
                 stencil_sampler, tex_target, load_level_zero, use_txf);
   ureg_END( ureg );

   return ureg_create_shader_and_destroy( ureg, pipe );
}


/**
 * Make a simple fragment texture shader which reads a texture and writes it
 * as stencil.
 */
void *
util_make_fragment_tex_shader_writestencil(struct pipe_context *pipe,
                                           unsigned tex_target,
                                           unsigned interp_mode,
                                           bool load_level_zero,
                                           bool use_txf)
{
   struct ureg_program *ureg;
   struct ureg_src stencil_sampler;
   struct ureg_src tex;
   struct ureg_dst out, stencil;
   struct ureg_src imm;

   ureg = ureg_create( PIPE_SHADER_FRAGMENT );
   if (!ureg)
      return NULL;

   stencil_sampler = ureg_DECL_sampler( ureg, 0 );

   ureg_DECL_sampler_view(ureg, 0, tex_target,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT,
                          TGSI_RETURN_TYPE_UINT);

   tex = ureg_DECL_fs_input( ureg,
                             TGSI_SEMANTIC_GENERIC, 0,
                             interp_mode );

   out = ureg_DECL_output( ureg,
                           TGSI_SEMANTIC_COLOR,
                           0 );

   stencil = ureg_DECL_output( ureg,
                             TGSI_SEMANTIC_STENCIL,
                             0 );

   imm = ureg_imm4f( ureg, 0, 0, 0, 1 );

   ureg_MOV( ureg, out, imm );

   ureg_load_tex(ureg, ureg_writemask(stencil, TGSI_WRITEMASK_Y), tex,
                 stencil_sampler, tex_target, load_level_zero, use_txf);
   ureg_END( ureg );

   return ureg_create_shader_and_destroy( ureg, pipe );
}


/**
 * Make simple fragment color pass-through shader that replicates OUT[0]
 * to all bound colorbuffers.
 */
void *
util_make_fragment_passthrough_shader(struct pipe_context *pipe,
                                      int input_semantic,
                                      int input_interpolate,
                                      boolean write_all_cbufs)
{
   static const char shader_templ[] =
         "FRAG\n"
         "%s"
         "DCL IN[0], %s[0], %s\n"
         "DCL OUT[0], COLOR[0]\n"

         "MOV OUT[0], IN[0]\n"
         "END\n";

   char text[sizeof(shader_templ)+100];
   struct tgsi_token tokens[1000];
   struct pipe_shader_state state;

   sprintf(text, shader_templ,
           write_all_cbufs ? "PROPERTY FS_COLOR0_WRITES_ALL_CBUFS 1\n" : "",
           tgsi_semantic_names[input_semantic],
           tgsi_interpolate_names[input_interpolate]);

   if (!tgsi_text_translate(text, tokens, ARRAY_SIZE(tokens))) {
      assert(0);
      return NULL;
   }
   pipe_shader_state_from_tgsi(&state, tokens);
#if 0
   tgsi_dump(state.tokens, 0);
#endif

   return pipe->create_fs_state(pipe, &state);
}


void *
util_make_empty_fragment_shader(struct pipe_context *pipe)
{
   struct ureg_program *ureg = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!ureg)
      return NULL;

   ureg_END(ureg);
   return ureg_create_shader_and_destroy(ureg, pipe);
}


/**
 * Make a fragment shader that copies the input color to N output colors.
 */
void *
util_make_fragment_cloneinput_shader(struct pipe_context *pipe, int num_cbufs,
                                     int input_semantic,
                                     int input_interpolate)
{
   struct ureg_program *ureg;
   struct ureg_src src;
   struct ureg_dst dst[PIPE_MAX_COLOR_BUFS];
   int i;

   assert(num_cbufs <= PIPE_MAX_COLOR_BUFS);

   ureg = ureg_create( PIPE_SHADER_FRAGMENT );
   if (!ureg)
      return NULL;

   src = ureg_DECL_fs_input( ureg, input_semantic, 0,
                             input_interpolate );

   for (i = 0; i < num_cbufs; i++)
      dst[i] = ureg_DECL_output( ureg, TGSI_SEMANTIC_COLOR, i );

   for (i = 0; i < num_cbufs; i++)
      ureg_MOV( ureg, dst[i], src );

   ureg_END( ureg );

   return ureg_create_shader_and_destroy( ureg, pipe );
}


static void *
util_make_fs_blit_msaa_gen(struct pipe_context *pipe,
                           enum tgsi_texture_type tgsi_tex,
                           const char *samp_type,
                           const char *output_semantic,
                           const char *output_mask,
                           const char *conversion_decl,
                           const char *conversion)
{
   static const char shader_templ[] =
         "FRAG\n"
         "DCL IN[0], GENERIC[0], LINEAR\n"
         "DCL SAMP[0]\n"
         "DCL SVIEW[0], %s, %s\n"
         "DCL OUT[0], %s\n"
         "DCL TEMP[0]\n"
         "%s"

         "F2U TEMP[0], IN[0]\n"
         "TXF TEMP[0], TEMP[0], SAMP[0], %s\n"
         "%s"
         "MOV OUT[0]%s, TEMP[0]\n"
         "END\n";

   const char *type = tgsi_texture_names[tgsi_tex];
   char text[sizeof(shader_templ)+100];
   struct tgsi_token tokens[1000];
   struct pipe_shader_state state;

   assert(tgsi_tex == TGSI_TEXTURE_2D_MSAA ||
          tgsi_tex == TGSI_TEXTURE_2D_ARRAY_MSAA);

   util_snprintf(text, sizeof(text), shader_templ, type, samp_type,
                 output_semantic, conversion_decl, type, conversion, output_mask);

   if (!tgsi_text_translate(text, tokens, ARRAY_SIZE(tokens))) {
      puts(text);
      assert(0);
      return NULL;
   }
   pipe_shader_state_from_tgsi(&state, tokens);
#if 0
   tgsi_dump(state.tokens, 0);
#endif

   return pipe->create_fs_state(pipe, &state);
}


/**
 * Make a fragment shader that sets the output color to a color
 * fetched from a multisample texture.
 * \param tex_target  one of PIPE_TEXTURE_x
 */
void *
util_make_fs_blit_msaa_color(struct pipe_context *pipe,
                             enum tgsi_texture_type tgsi_tex,
                             enum tgsi_return_type stype,
                             enum tgsi_return_type dtype)
{
   const char *samp_type;
   const char *conversion_decl = "";
   const char *conversion = "";

   if (stype == TGSI_RETURN_TYPE_UINT) {
      samp_type = "UINT";

      if (dtype == TGSI_RETURN_TYPE_SINT) {
         conversion_decl = "IMM[0] UINT32 {2147483647, 0, 0, 0}\n";
         conversion = "UMIN TEMP[0], TEMP[0], IMM[0].xxxx\n";
      }
   } else if (stype == TGSI_RETURN_TYPE_SINT) {
      samp_type = "SINT";

      if (dtype == TGSI_RETURN_TYPE_UINT) {
         conversion_decl = "IMM[0] INT32 {0, 0, 0, 0}\n";
         conversion = "IMAX TEMP[0], TEMP[0], IMM[0].xxxx\n";
      }
   } else {
      assert(dtype == TGSI_RETURN_TYPE_FLOAT);
      samp_type = "FLOAT";
   }

   return util_make_fs_blit_msaa_gen(pipe, tgsi_tex, samp_type,
                                     "COLOR[0]", "", conversion_decl,
                                     conversion);
}


/**
 * Make a fragment shader that sets the output depth to a depth value
 * fetched from a multisample texture.
 * \param tex_target  one of PIPE_TEXTURE_x
 */
void *
util_make_fs_blit_msaa_depth(struct pipe_context *pipe,
                             enum tgsi_texture_type tgsi_tex)
{
   return util_make_fs_blit_msaa_gen(pipe, tgsi_tex, "FLOAT",
                                     "POSITION", ".z", "", "");
}


/**
 * Make a fragment shader that sets the output stencil to a stencil value
 * fetched from a multisample texture.
 * \param tex_target  one of PIPE_TEXTURE_x
 */
void *
util_make_fs_blit_msaa_stencil(struct pipe_context *pipe,
                               enum tgsi_texture_type tgsi_tex)
{
   return util_make_fs_blit_msaa_gen(pipe, tgsi_tex, "UINT",
                                     "STENCIL", ".y", "", "");
}


/**
 * Make a fragment shader that sets the output depth and stencil to depth
 * and stencil values fetched from two multisample textures / samplers.
 * The sizes of both textures should match (it should be one depth-stencil
 * texture).
 * \param tex_target  one of PIPE_TEXTURE_x
 */
void *
util_make_fs_blit_msaa_depthstencil(struct pipe_context *pipe,
                                    enum tgsi_texture_type tgsi_tex)
{
   static const char shader_templ[] =
         "FRAG\n"
         "DCL IN[0], GENERIC[0], LINEAR\n"
         "DCL SAMP[0..1]\n"
         "DCL SVIEW[0..1], %s, FLOAT\n"
         "DCL OUT[0], POSITION\n"
         "DCL OUT[1], STENCIL\n"
         "DCL TEMP[0]\n"

         "F2U TEMP[0], IN[0]\n"
         "TXF OUT[0].z, TEMP[0], SAMP[0], %s\n"
         "TXF OUT[1].y, TEMP[0], SAMP[1], %s\n"
         "END\n";

   const char *type = tgsi_texture_names[tgsi_tex];
   char text[sizeof(shader_templ)+100];
   struct tgsi_token tokens[1000];
   struct pipe_shader_state state;

   assert(tgsi_tex == TGSI_TEXTURE_2D_MSAA ||
          tgsi_tex == TGSI_TEXTURE_2D_ARRAY_MSAA);

   sprintf(text, shader_templ, type, type, type);

   if (!tgsi_text_translate(text, tokens, ARRAY_SIZE(tokens))) {
      assert(0);
      return NULL;
   }
   pipe_shader_state_from_tgsi(&state, tokens);
#if 0
   tgsi_dump(state.tokens, 0);
#endif

   return pipe->create_fs_state(pipe, &state);
}


void *
util_make_fs_msaa_resolve(struct pipe_context *pipe,
                          enum tgsi_texture_type tgsi_tex, unsigned nr_samples,
                          enum tgsi_return_type stype)
{
   struct ureg_program *ureg;
   struct ureg_src sampler, coord;
   struct ureg_dst out, tmp_sum, tmp_coord, tmp;
   unsigned i;

   ureg = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!ureg)
      return NULL;

   /* Declarations. */
   sampler = ureg_DECL_sampler(ureg, 0);
   ureg_DECL_sampler_view(ureg, 0, tgsi_tex, stype, stype, stype, stype);
   coord = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_GENERIC, 0,
                              TGSI_INTERPOLATE_LINEAR);
   out = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
   tmp_sum = ureg_DECL_temporary(ureg);
   tmp_coord = ureg_DECL_temporary(ureg);
   tmp = ureg_DECL_temporary(ureg);

   /* Instructions. */
   ureg_MOV(ureg, tmp_sum, ureg_imm1f(ureg, 0));
   ureg_F2U(ureg, tmp_coord, coord);

   for (i = 0; i < nr_samples; i++) {
      /* Read one sample. */
      ureg_MOV(ureg, ureg_writemask(tmp_coord, TGSI_WRITEMASK_W),
               ureg_imm1u(ureg, i));
      ureg_TXF(ureg, tmp, tgsi_tex, ureg_src(tmp_coord), sampler);

      if (stype == TGSI_RETURN_TYPE_UINT)
         ureg_U2F(ureg, tmp, ureg_src(tmp));
      else if (stype == TGSI_RETURN_TYPE_SINT)
         ureg_I2F(ureg, tmp, ureg_src(tmp));

      /* Add it to the sum.*/
      ureg_ADD(ureg, tmp_sum, ureg_src(tmp_sum), ureg_src(tmp));
   }

   /* Calculate the average and return. */
   ureg_MUL(ureg, tmp_sum, ureg_src(tmp_sum),
            ureg_imm1f(ureg, 1.0 / nr_samples));

   if (stype == TGSI_RETURN_TYPE_UINT)
      ureg_F2U(ureg, out, ureg_src(tmp_sum));
   else if (stype == TGSI_RETURN_TYPE_SINT)
      ureg_F2I(ureg, out, ureg_src(tmp_sum));
   else
      ureg_MOV(ureg, out, ureg_src(tmp_sum));

   ureg_END(ureg);

   return ureg_create_shader_and_destroy(ureg, pipe);
}


void *
util_make_fs_msaa_resolve_bilinear(struct pipe_context *pipe,
                                   enum tgsi_texture_type tgsi_tex,
                                   unsigned nr_samples,
                                   enum tgsi_return_type stype)
{
   struct ureg_program *ureg;
   struct ureg_src sampler, coord;
   struct ureg_dst out, tmp, top, bottom;
   struct ureg_dst tmp_coord[4], tmp_sum[4];
   unsigned i, c;

   ureg = ureg_create(PIPE_SHADER_FRAGMENT);
   if (!ureg)
      return NULL;

   /* Declarations. */
   sampler = ureg_DECL_sampler(ureg, 0);
   ureg_DECL_sampler_view(ureg, 0, tgsi_tex, stype, stype, stype, stype);
   coord = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_GENERIC, 0,
                              TGSI_INTERPOLATE_LINEAR);
   out = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
   for (c = 0; c < 4; c++)
      tmp_sum[c] = ureg_DECL_temporary(ureg);
   for (c = 0; c < 4; c++)
      tmp_coord[c] = ureg_DECL_temporary(ureg);
   tmp = ureg_DECL_temporary(ureg);
   top = ureg_DECL_temporary(ureg);
   bottom = ureg_DECL_temporary(ureg);

   /* Instructions. */
   for (c = 0; c < 4; c++)
      ureg_MOV(ureg, tmp_sum[c], ureg_imm1f(ureg, 0));

   /* Get 4 texture coordinates for the bilinear filter. */
   ureg_F2U(ureg, tmp_coord[0], coord); /* top-left */
   ureg_UADD(ureg, tmp_coord[1], ureg_src(tmp_coord[0]),
             ureg_imm4u(ureg, 1, 0, 0, 0)); /* top-right */
   ureg_UADD(ureg, tmp_coord[2], ureg_src(tmp_coord[0]),
             ureg_imm4u(ureg, 0, 1, 0, 0)); /* bottom-left */
   ureg_UADD(ureg, tmp_coord[3], ureg_src(tmp_coord[0]),
             ureg_imm4u(ureg, 1, 1, 0, 0)); /* bottom-right */

   for (i = 0; i < nr_samples; i++) {
      for (c = 0; c < 4; c++) {
         /* Read one sample. */
         ureg_MOV(ureg, ureg_writemask(tmp_coord[c], TGSI_WRITEMASK_W),
                  ureg_imm1u(ureg, i));
         ureg_TXF(ureg, tmp, tgsi_tex, ureg_src(tmp_coord[c]), sampler);

         if (stype == TGSI_RETURN_TYPE_UINT)
            ureg_U2F(ureg, tmp, ureg_src(tmp));
         else if (stype == TGSI_RETURN_TYPE_SINT)
            ureg_I2F(ureg, tmp, ureg_src(tmp));

         /* Add it to the sum.*/
         ureg_ADD(ureg, tmp_sum[c], ureg_src(tmp_sum[c]), ureg_src(tmp));
      }
   }

   /* Calculate the average. */
   for (c = 0; c < 4; c++)
      ureg_MUL(ureg, tmp_sum[c], ureg_src(tmp_sum[c]),
               ureg_imm1f(ureg, 1.0 / nr_samples));

   /* Take the 4 average values and apply a standard bilinear filter. */
   ureg_FRC(ureg, tmp, coord);

   ureg_LRP(ureg, top,
            ureg_scalar(ureg_src(tmp), 0),
            ureg_src(tmp_sum[1]),
            ureg_src(tmp_sum[0]));

   ureg_LRP(ureg, bottom,
            ureg_scalar(ureg_src(tmp), 0),
            ureg_src(tmp_sum[3]),
            ureg_src(tmp_sum[2]));

   ureg_LRP(ureg, tmp,
            ureg_scalar(ureg_src(tmp), 1),
            ureg_src(bottom),
            ureg_src(top));

   /* Convert to the texture format and return. */
   if (stype == TGSI_RETURN_TYPE_UINT)
      ureg_F2U(ureg, out, ureg_src(tmp));
   else if (stype == TGSI_RETURN_TYPE_SINT)
      ureg_F2I(ureg, out, ureg_src(tmp));
   else
      ureg_MOV(ureg, out, ureg_src(tmp));

   ureg_END(ureg);

   return ureg_create_shader_and_destroy(ureg, pipe);
}

void *
util_make_geometry_passthrough_shader(struct pipe_context *pipe,
                                      uint num_attribs,
                                      const ubyte *semantic_names,
                                      const ubyte *semantic_indexes)
{
   static const unsigned zero[4] = {0, 0, 0, 0};

   struct ureg_program *ureg;
   struct ureg_dst dst[PIPE_MAX_SHADER_OUTPUTS];
   struct ureg_src src[PIPE_MAX_SHADER_INPUTS];
   struct ureg_src imm;

   unsigned i;

   ureg = ureg_create(PIPE_SHADER_GEOMETRY);
   if (!ureg)
      return NULL;

   ureg_property(ureg, TGSI_PROPERTY_GS_INPUT_PRIM, PIPE_PRIM_POINTS);
   ureg_property(ureg, TGSI_PROPERTY_GS_OUTPUT_PRIM, PIPE_PRIM_POINTS);
   ureg_property(ureg, TGSI_PROPERTY_GS_MAX_OUTPUT_VERTICES, 1);
   ureg_property(ureg, TGSI_PROPERTY_GS_INVOCATIONS, 1);
   imm = ureg_DECL_immediate_uint(ureg, zero, 4);

   /**
    * Loop over all the attribs and declare the corresponding
    * declarations in the geometry shader
    */
   for (i = 0; i < num_attribs; i++) {
      src[i] = ureg_DECL_input(ureg, semantic_names[i],
                               semantic_indexes[i], 0, 1);
      src[i] = ureg_src_dimension(src[i], 0);
      dst[i] = ureg_DECL_output(ureg, semantic_names[i], semantic_indexes[i]);
   }

   /* MOV dst[i] src[i] */
   for (i = 0; i < num_attribs; i++) {
      ureg_MOV(ureg, dst[i], src[i]);
   }

   /* EMIT IMM[0] */
   ureg_insn(ureg, TGSI_OPCODE_EMIT, NULL, 0, &imm, 1, 0);

   /* END */
   ureg_END(ureg);

   return ureg_create_shader_and_destroy(ureg, pipe);
}

