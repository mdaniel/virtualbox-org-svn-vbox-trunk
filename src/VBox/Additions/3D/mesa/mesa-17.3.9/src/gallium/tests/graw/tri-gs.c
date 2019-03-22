/* Display a cleared blue window.  This demo has no dependencies on
 * any utility code, just the graw interface and gallium.
 */

#include <stdio.h>
#include "state_tracker/graw.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_defines.h"

#include "util/u_memory.h"      /* Offset() */
#include "util/u_draw_quad.h"
#include "util/u_inlines.h"

enum pipe_format formats[] = {
   PIPE_FORMAT_RGBA8888_UNORM,
   PIPE_FORMAT_BGRA8888_UNORM,
   PIPE_FORMAT_NONE
};

static const int WIDTH = 300;
static const int HEIGHT = 300;

static struct pipe_screen *screen = NULL;
static struct pipe_context *ctx = NULL;
static struct pipe_surface *surf = NULL;
static struct pipe_resource *tex = NULL;
static void *window = NULL;

struct vertex {
   float position[4];
   float color[4];
};

static struct vertex vertices[4] =
{
   { { 0.0f, -0.9f, 0.0f, 1.0f },
     { 1.0f, 0.0f, 0.0f, 1.0f }
   },
   { { -0.9f, 0.9f, 0.0f, 1.0f },
     { 0.0f, 1.0f, 0.0f, 1.0f }
   },
   { { 0.9f, 0.9f, 0.0f, 1.0f },
     { 0.0f, 0.0f, 1.0f, 1.0f }
   }
};




static void set_viewport( float x, float y,
                          float width, float height,
                          float zNear, float zFar)
{
   float z = zFar;
   float half_width = (float)width / 2.0f;
   float half_height = (float)height / 2.0f;
   float half_depth = ((float)zFar - (float)zNear) / 2.0f;
   struct pipe_viewport_state vp;

   vp.scale[0] = half_width;
   vp.scale[1] = half_height;
   vp.scale[2] = half_depth;

   vp.translate[0] = half_width + x;
   vp.translate[1] = half_height + y;
   vp.translate[2] = half_depth + z;

   ctx->set_viewport_states( ctx, 0, 1, &vp );
}

static void set_vertices( void )
{
   struct pipe_vertex_element ve[2];
   struct pipe_vertex_buffer vbuf;
   void *handle;

   memset(ve, 0, sizeof ve);

   ve[0].src_offset = Offset(struct vertex, position);
   ve[0].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   ve[1].src_offset = Offset(struct vertex, color);
   ve[1].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;

   handle = ctx->create_vertex_elements_state(ctx, 2, ve);
   ctx->bind_vertex_elements_state(ctx, handle);

   memset(&vbuf, 0, sizeof vbuf);

   vbuf.stride = sizeof( struct vertex );
   vbuf.buffer_offset = 0;
   vbuf.buffer.resource = pipe_buffer_create_with_data(ctx,
                                              PIPE_BIND_VERTEX_BUFFER,
                                              PIPE_USAGE_DEFAULT,
                                              sizeof(vertices),
                                              vertices);

   ctx->set_vertex_buffers(ctx, 0, 1, &vbuf);
}

static void set_vertex_shader( void )
{
   void *handle;
   const char *text =
      "VERT\n"
      "DCL IN[0]\n"
      "DCL IN[1]\n"
      "DCL OUT[0], POSITION\n"
      "DCL OUT[1], COLOR\n"
      "  0: MOV OUT[1], IN[1]\n"
      "  1: MOV OUT[0], IN[0]\n"
      "  2: END\n";

   handle = graw_parse_vertex_shader(ctx, text);
   ctx->bind_vs_state(ctx, handle);
}

static void set_fragment_shader( void )
{
   void *handle;
   const char *text =
      "FRAG\n"
      "DCL IN[0], COLOR, LINEAR\n"
      "DCL OUT[0], COLOR\n"
      "  0: MOV OUT[0], IN[0]\n"
      "  1: END\n";

   handle = graw_parse_fragment_shader(ctx, text);
   ctx->bind_fs_state(ctx, handle);
}


static void set_geometry_shader( void )
{
   void *handle;
   const char *text =
      "GEOM\n"
      "PROPERTY GS_INPUT_PRIMITIVE TRIANGLES\n"
      "PROPERTY GS_OUTPUT_PRIMITIVE TRIANGLE_STRIP\n"
      "DCL IN[][0], POSITION, CONSTANT\n"
      "DCL IN[][1], COLOR, CONSTANT\n"
      "DCL OUT[0], POSITION, CONSTANT\n"
      "DCL OUT[1], COLOR, CONSTANT\n"
      "0:MOV OUT[0], IN[0][0]\n"
      "1:MOV OUT[1], IN[0][1]\n"
      "2:EMIT\n"
      "3:MOV OUT[0], IN[1][0]\n"
      "4:MOV OUT[1], IN[0][1]\n" /* copy color from input vertex 0 */
      "5:EMIT\n"
      "6:MOV OUT[0], IN[2][0]\n"
      "7:MOV OUT[1], IN[2][1]\n"
      "8:EMIT\n"
      "9:ENDPRIM\n"
      "10:END\n";

   handle = graw_parse_geometry_shader(ctx, text);
   ctx->bind_gs_state(ctx, handle);
}   

static void draw( void )
{
   union pipe_color_union clear_color = { {1,0,1,1} };

   ctx->clear(ctx, PIPE_CLEAR_COLOR, &clear_color, 0, 0);
   util_draw_arrays(ctx, PIPE_PRIM_TRIANGLES, 0, 3);
   ctx->flush(ctx, NULL, 0);

   screen->flush_frontbuffer(screen, tex, 0, 0, window, NULL);
}


static void init( void )
{
   struct pipe_framebuffer_state fb;
   struct pipe_resource templat;
   struct pipe_surface surf_tmpl;
   int i;

   /* It's hard to say whether window or screen should be created
    * first.  Different environments would prefer one or the other.
    *
    * Also, no easy way of querying supported formats if the screen
    * cannot be created first.
    */
   for (i = 0; formats[i] != PIPE_FORMAT_NONE; i++) {
      screen = graw_create_window_and_screen(0, 0, 300, 300,
                                             formats[i],
                                             &window);
      if (window && screen)
         break;
   }
   if (!screen || !window) {
      fprintf(stderr, "Unable to create window\n");
      exit(1);
   }
   
   ctx = screen->context_create(screen, NULL, 0);
   if (ctx == NULL)
      exit(3);

   memset(&templat, 0, sizeof(templat));
   templat.target = PIPE_TEXTURE_2D;
   templat.format = formats[i];
   templat.width0 = WIDTH;
   templat.height0 = HEIGHT;
   templat.depth0 = 1;
   templat.array_size = 1;
   templat.last_level = 0;
   templat.nr_samples = 1;
   templat.bind = (PIPE_BIND_RENDER_TARGET |
                   PIPE_BIND_DISPLAY_TARGET);
   
   tex = screen->resource_create(screen,
                                 &templat);
   if (tex == NULL)
      exit(4);

   surf_tmpl.format = templat.format;
   surf_tmpl.u.tex.level = 0;
   surf_tmpl.u.tex.first_layer = 0;
   surf_tmpl.u.tex.last_layer = 0;
   surf = ctx->create_surface(ctx, tex, &surf_tmpl);
   if (surf == NULL)
      exit(5);

   memset(&fb, 0, sizeof fb);
   fb.nr_cbufs = 1;
   fb.width = WIDTH;
   fb.height = HEIGHT;
   fb.cbufs[0] = surf;

   ctx->set_framebuffer_state(ctx, &fb);
   
   {
      struct pipe_blend_state blend;
      void *handle;
      memset(&blend, 0, sizeof blend);
      blend.rt[0].colormask = PIPE_MASK_RGBA;
      handle = ctx->create_blend_state(ctx, &blend);
      ctx->bind_blend_state(ctx, handle);
   }

   {
      struct pipe_depth_stencil_alpha_state depthstencil;
      void *handle;
      memset(&depthstencil, 0, sizeof depthstencil);
      handle = ctx->create_depth_stencil_alpha_state(ctx, &depthstencil);
      ctx->bind_depth_stencil_alpha_state(ctx, handle);
   }

   {
      struct pipe_rasterizer_state rasterizer;
      void *handle;
      memset(&rasterizer, 0, sizeof rasterizer);
      rasterizer.cull_face = PIPE_FACE_NONE;
      rasterizer.half_pixel_center = 1;
      rasterizer.bottom_edge_rule = 1;
      rasterizer.depth_clip = 1;
      handle = ctx->create_rasterizer_state(ctx, &rasterizer);
      ctx->bind_rasterizer_state(ctx, handle);
   }

   set_viewport(0, 0, WIDTH, HEIGHT, 30, 1000);
   set_vertices();
   set_vertex_shader();
   set_fragment_shader();
   set_geometry_shader();
}


int main( int argc, char *argv[] )
{
   init();

   graw_set_display_func( draw );
   graw_main_loop();
   return 0;
}
