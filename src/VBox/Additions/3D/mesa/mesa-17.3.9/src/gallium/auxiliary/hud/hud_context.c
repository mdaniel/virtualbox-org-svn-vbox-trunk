/**************************************************************************
 *
 * Copyright 2013 Marek Olšák <maraeo@gmail.com>
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
 * IN NO EVENT SHALL THE AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

/* This head-up display module can draw transparent graphs on top of what
 * the app is rendering, visualizing various data like framerate, cpu load,
 * performance counters, etc. It can be hook up into any state tracker.
 *
 * The HUD is controlled with the GALLIUM_HUD environment variable.
 * Set GALLIUM_HUD=help for more info.
 */

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

#include "hud/hud_context.h"
#include "hud/hud_private.h"

#include "cso_cache/cso_context.h"
#include "util/u_draw_quad.h"
#include "util/u_format.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_math.h"
#include "util/u_sampler.h"
#include "util/u_simple_shaders.h"
#include "util/u_string.h"
#include "util/u_upload_mgr.h"
#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_dump.h"

/* Control the visibility of all HUD contexts */
static boolean huds_visible = TRUE;


#ifdef PIPE_OS_UNIX
static void
signal_visible_handler(int sig, siginfo_t *siginfo, void *context)
{
   huds_visible = !huds_visible;
}
#endif

static void
hud_draw_colored_prims(struct hud_context *hud, unsigned prim,
                       float *buffer, unsigned num_vertices,
                       float r, float g, float b, float a,
                       int xoffset, int yoffset, float yscale)
{
   struct cso_context *cso = hud->cso;
   unsigned size = num_vertices * hud->color_prims.vbuf.stride;

   assert(size <= hud->color_prims.buffer_size);
   memcpy(hud->color_prims.vertices, buffer, size);

   hud->constants.color[0] = r;
   hud->constants.color[1] = g;
   hud->constants.color[2] = b;
   hud->constants.color[3] = a;
   hud->constants.translate[0] = (float) xoffset;
   hud->constants.translate[1] = (float) yoffset;
   hud->constants.scale[0] = 1;
   hud->constants.scale[1] = yscale;
   cso_set_constant_buffer(cso, PIPE_SHADER_VERTEX, 0, &hud->constbuf);

   cso_set_vertex_buffers(cso, cso_get_aux_vertex_buffer_slot(cso),
                          1, &hud->color_prims.vbuf);
   cso_set_fragment_shader_handle(hud->cso, hud->fs_color);
   cso_draw_arrays(cso, prim, 0, num_vertices);

   hud->color_prims.vertices += size / sizeof(float);
   hud->color_prims.vbuf.buffer_offset += size;
   hud->color_prims.buffer_size -= size;
}

static void
hud_draw_colored_quad(struct hud_context *hud, unsigned prim,
                      unsigned x1, unsigned y1, unsigned x2, unsigned y2,
                      float r, float g, float b, float a)
{
   float buffer[] = {
      (float) x1, (float) y1,
      (float) x1, (float) y2,
      (float) x2, (float) y2,
      (float) x2, (float) y1,
   };

   hud_draw_colored_prims(hud, prim, buffer, 4, r, g, b, a, 0, 0, 1);
}

static void
hud_draw_background_quad(struct hud_context *hud,
                         unsigned x1, unsigned y1, unsigned x2, unsigned y2)
{
   float *vertices = hud->bg.vertices + hud->bg.num_vertices*2;
   unsigned num = 0;

   assert(hud->bg.num_vertices + 4 <= hud->bg.max_num_vertices);

   vertices[num++] = (float) x1;
   vertices[num++] = (float) y1;

   vertices[num++] = (float) x1;
   vertices[num++] = (float) y2;

   vertices[num++] = (float) x2;
   vertices[num++] = (float) y2;

   vertices[num++] = (float) x2;
   vertices[num++] = (float) y1;

   hud->bg.num_vertices += num/2;
}

static void
hud_draw_string(struct hud_context *hud, unsigned x, unsigned y,
                const char *str, ...)
{
   char buf[256];
   char *s = buf;
   float *vertices = hud->text.vertices + hud->text.num_vertices*4;
   unsigned num = 0;

   va_list ap;
   va_start(ap, str);
   util_vsnprintf(buf, sizeof(buf), str, ap);
   va_end(ap);

   if (!*s)
      return;

   hud_draw_background_quad(hud,
                            x, y,
                            x + strlen(buf)*hud->font.glyph_width,
                            y + hud->font.glyph_height);

   while (*s) {
      unsigned x1 = x;
      unsigned y1 = y;
      unsigned x2 = x + hud->font.glyph_width;
      unsigned y2 = y + hud->font.glyph_height;
      unsigned tx1 = (*s % 16) * hud->font.glyph_width;
      unsigned ty1 = (*s / 16) * hud->font.glyph_height;
      unsigned tx2 = tx1 + hud->font.glyph_width;
      unsigned ty2 = ty1 + hud->font.glyph_height;

      if (*s == ' ') {
         x += hud->font.glyph_width;
         s++;
         continue;
      }

      assert(hud->text.num_vertices + num/4 + 4 <= hud->text.max_num_vertices);

      vertices[num++] = (float) x1;
      vertices[num++] = (float) y1;
      vertices[num++] = (float) tx1;
      vertices[num++] = (float) ty1;

      vertices[num++] = (float) x1;
      vertices[num++] = (float) y2;
      vertices[num++] = (float) tx1;
      vertices[num++] = (float) ty2;

      vertices[num++] = (float) x2;
      vertices[num++] = (float) y2;
      vertices[num++] = (float) tx2;
      vertices[num++] = (float) ty2;

      vertices[num++] = (float) x2;
      vertices[num++] = (float) y1;
      vertices[num++] = (float) tx2;
      vertices[num++] = (float) ty1;

      x += hud->font.glyph_width;
      s++;
   }

   hud->text.num_vertices += num/4;
}

static void
number_to_human_readable(double num, enum pipe_driver_query_type type,
                         char *out)
{
   static const char *byte_units[] =
      {" B", " KB", " MB", " GB", " TB", " PB", " EB"};
   static const char *metric_units[] =
      {"", " k", " M", " G", " T", " P", " E"};
   static const char *time_units[] =
      {" us", " ms", " s"};  /* based on microseconds */
   static const char *hz_units[] =
      {" Hz", " KHz", " MHz", " GHz"};
   static const char *percent_units[] = {"%"};
   static const char *dbm_units[] = {" (-dBm)"};
   static const char *temperature_units[] = {" C"};
   static const char *volt_units[] = {" mV", " V"};
   static const char *amp_units[] = {" mA", " A"};
   static const char *watt_units[] = {" mW", " W"};

   const char **units;
   unsigned max_unit;
   double divisor = (type == PIPE_DRIVER_QUERY_TYPE_BYTES) ? 1024 : 1000;
   unsigned unit = 0;
   double d = num;

   switch (type) {
   case PIPE_DRIVER_QUERY_TYPE_MICROSECONDS:
      max_unit = ARRAY_SIZE(time_units)-1;
      units = time_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_VOLTS:
      max_unit = ARRAY_SIZE(volt_units)-1;
      units = volt_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_AMPS:
      max_unit = ARRAY_SIZE(amp_units)-1;
      units = amp_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_DBM:
      max_unit = ARRAY_SIZE(dbm_units)-1;
      units = dbm_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_TEMPERATURE:
      max_unit = ARRAY_SIZE(temperature_units)-1;
      units = temperature_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_PERCENTAGE:
      max_unit = ARRAY_SIZE(percent_units)-1;
      units = percent_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_BYTES:
      max_unit = ARRAY_SIZE(byte_units)-1;
      units = byte_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_HZ:
      max_unit = ARRAY_SIZE(hz_units)-1;
      units = hz_units;
      break;
   case PIPE_DRIVER_QUERY_TYPE_WATTS:
      max_unit = ARRAY_SIZE(watt_units)-1;
      units = watt_units;
      break;
   default:
      max_unit = ARRAY_SIZE(metric_units)-1;
      units = metric_units;
   }

   while (d > divisor && unit < max_unit) {
      d /= divisor;
      unit++;
   }

   /* Round to 3 decimal places so as not to print trailing zeros. */
   if (d*1000 != (int)(d*1000))
      d = round(d * 1000) / 1000;

   /* Show at least 4 digits with at most 3 decimal places, but not zeros. */
   if (d >= 1000 || d == (int)d)
      sprintf(out, "%.0f%s", d, units[unit]);
   else if (d >= 100 || d*10 == (int)(d*10))
      sprintf(out, "%.1f%s", d, units[unit]);
   else if (d >= 10 || d*100 == (int)(d*100))
      sprintf(out, "%.2f%s", d, units[unit]);
   else
      sprintf(out, "%.3f%s", d, units[unit]);
}

static void
hud_draw_graph_line_strip(struct hud_context *hud, const struct hud_graph *gr,
                          unsigned xoffset, unsigned yoffset, float yscale)
{
   if (gr->num_vertices <= 1)
      return;

   assert(gr->index <= gr->num_vertices);

   hud_draw_colored_prims(hud, PIPE_PRIM_LINE_STRIP,
                          gr->vertices, gr->index,
                          gr->color[0], gr->color[1], gr->color[2], 1,
                          xoffset + (gr->pane->max_num_vertices - gr->index - 1) * 2 - 1,
                          yoffset, yscale);

   if (gr->num_vertices <= gr->index)
      return;

   hud_draw_colored_prims(hud, PIPE_PRIM_LINE_STRIP,
                          gr->vertices + gr->index*2,
                          gr->num_vertices - gr->index,
                          gr->color[0], gr->color[1], gr->color[2], 1,
                          xoffset - gr->index*2 - 1, yoffset, yscale);
}

static void
hud_pane_accumulate_vertices(struct hud_context *hud,
                             const struct hud_pane *pane)
{
   struct hud_graph *gr;
   float *line_verts = hud->whitelines.vertices + hud->whitelines.num_vertices*2;
   unsigned i, num = 0;
   char str[32];
   const unsigned last_line = pane->last_line;

   /* draw background */
   hud_draw_background_quad(hud,
                            pane->x1, pane->y1,
                            pane->x2, pane->y2);

   /* draw numbers on the right-hand side */
   for (i = 0; i <= last_line; i++) {
      unsigned x = pane->x2 + 2;
      unsigned y = pane->inner_y1 +
                   pane->inner_height * (last_line - i) / last_line -
                   hud->font.glyph_height / 2;

      number_to_human_readable(pane->max_value * i / last_line,
                               pane->type, str);
      hud_draw_string(hud, x, y, "%s", str);
   }

   /* draw info below the pane */
   i = 0;
   LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
      unsigned x = pane->x1 + 2;
      unsigned y = pane->y2 + 2 + i*hud->font.glyph_height;

      number_to_human_readable(gr->current_value, pane->type, str);
      hud_draw_string(hud, x, y, "  %s: %s", gr->name, str);
      i++;
   }

   /* draw border */
   assert(hud->whitelines.num_vertices + num/2 + 8 <= hud->whitelines.max_num_vertices);
   line_verts[num++] = (float) pane->x1;
   line_verts[num++] = (float) pane->y1;
   line_verts[num++] = (float) pane->x2;
   line_verts[num++] = (float) pane->y1;

   line_verts[num++] = (float) pane->x2;
   line_verts[num++] = (float) pane->y1;
   line_verts[num++] = (float) pane->x2;
   line_verts[num++] = (float) pane->y2;

   line_verts[num++] = (float) pane->x1;
   line_verts[num++] = (float) pane->y2;
   line_verts[num++] = (float) pane->x2;
   line_verts[num++] = (float) pane->y2;

   line_verts[num++] = (float) pane->x1;
   line_verts[num++] = (float) pane->y1;
   line_verts[num++] = (float) pane->x1;
   line_verts[num++] = (float) pane->y2;

   /* draw horizontal lines inside the graph */
   for (i = 0; i <= last_line; i++) {
      float y = round((pane->max_value * i / (double)last_line) *
                      pane->yscale + pane->inner_y2);

      assert(hud->whitelines.num_vertices + num/2 + 2 <= hud->whitelines.max_num_vertices);
      line_verts[num++] = pane->x1;
      line_verts[num++] = y;
      line_verts[num++] = pane->x2;
      line_verts[num++] = y;
   }

   hud->whitelines.num_vertices += num/2;
}

static void
hud_pane_draw_colored_objects(struct hud_context *hud,
                              const struct hud_pane *pane)
{
   struct hud_graph *gr;
   unsigned i;

   /* draw colored quads below the pane */
   i = 0;
   LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
      unsigned x = pane->x1 + 2;
      unsigned y = pane->y2 + 2 + i*hud->font.glyph_height;

      hud_draw_colored_quad(hud, PIPE_PRIM_QUADS, x + 1, y + 1, x + 12, y + 13,
                            gr->color[0], gr->color[1], gr->color[2], 1);
      i++;
   }

   /* draw the line strips */
   LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
      hud_draw_graph_line_strip(hud, gr, pane->inner_x1, pane->inner_y2, pane->yscale);
   }
}

static void
hud_prepare_vertices(struct hud_context *hud, struct vertex_queue *v,
                     unsigned num_vertices, unsigned stride)
{
   v->num_vertices = 0;
   v->max_num_vertices = num_vertices;
   v->vbuf.stride = stride;
   v->buffer_size = stride * num_vertices;
}

/**
 * Draw the HUD to the texture \p tex.
 * The texture is usually the back buffer being displayed.
 */
void
hud_draw(struct hud_context *hud, struct pipe_resource *tex)
{
   struct cso_context *cso = hud->cso;
   struct pipe_context *pipe = hud->pipe;
   struct pipe_framebuffer_state fb;
   struct pipe_surface surf_templ, *surf;
   struct pipe_viewport_state viewport;
   const struct pipe_sampler_state *sampler_states[] =
         { &hud->font_sampler_state };
   struct hud_pane *pane;
   struct hud_graph *gr, *next;

   if (!huds_visible)
      return;

   hud->fb_width = tex->width0;
   hud->fb_height = tex->height0;
   hud->constants.two_div_fb_width = 2.0f / hud->fb_width;
   hud->constants.two_div_fb_height = 2.0f / hud->fb_height;

   cso_save_state(cso, (CSO_BIT_FRAMEBUFFER |
                        CSO_BIT_SAMPLE_MASK |
                        CSO_BIT_MIN_SAMPLES |
                        CSO_BIT_BLEND |
                        CSO_BIT_DEPTH_STENCIL_ALPHA |
                        CSO_BIT_FRAGMENT_SHADER |
                        CSO_BIT_FRAGMENT_SAMPLER_VIEWS |
                        CSO_BIT_FRAGMENT_SAMPLERS |
                        CSO_BIT_RASTERIZER |
                        CSO_BIT_VIEWPORT |
                        CSO_BIT_STREAM_OUTPUTS |
                        CSO_BIT_GEOMETRY_SHADER |
                        CSO_BIT_TESSCTRL_SHADER |
                        CSO_BIT_TESSEVAL_SHADER |
                        CSO_BIT_VERTEX_SHADER |
                        CSO_BIT_VERTEX_ELEMENTS |
                        CSO_BIT_AUX_VERTEX_BUFFER_SLOT |
                        CSO_BIT_PAUSE_QUERIES |
                        CSO_BIT_RENDER_CONDITION));
   cso_save_constant_buffer_slot0(cso, PIPE_SHADER_VERTEX);

   /* set states */
   memset(&surf_templ, 0, sizeof(surf_templ));
   surf_templ.format = tex->format;

   /* Without this, AA lines look thinner if they are between 2 pixels
    * because the alpha is 0.5 on both pixels. (it's ugly)
    *
    * sRGB makes the width of all AA lines look the same.
    */
   if (hud->has_srgb) {
      enum pipe_format srgb_format = util_format_srgb(tex->format);

      if (srgb_format != PIPE_FORMAT_NONE)
         surf_templ.format = srgb_format;
   }
   surf = pipe->create_surface(pipe, tex, &surf_templ);

   memset(&fb, 0, sizeof(fb));
   fb.nr_cbufs = 1;
   fb.cbufs[0] = surf;
   fb.zsbuf = NULL;
   fb.width = hud->fb_width;
   fb.height = hud->fb_height;

   viewport.scale[0] = 0.5f * hud->fb_width;
   viewport.scale[1] = 0.5f * hud->fb_height;
   viewport.scale[2] = 1.0f;
   viewport.translate[0] = 0.5f * hud->fb_width;
   viewport.translate[1] = 0.5f * hud->fb_height;
   viewport.translate[2] = 0.0f;

   cso_set_framebuffer(cso, &fb);
   cso_set_sample_mask(cso, ~0);
   cso_set_min_samples(cso, 1);
   cso_set_depth_stencil_alpha(cso, &hud->dsa);
   cso_set_rasterizer(cso, &hud->rasterizer);
   cso_set_viewport(cso, &viewport);
   cso_set_stream_outputs(cso, 0, NULL, NULL);
   cso_set_tessctrl_shader_handle(cso, NULL);
   cso_set_tesseval_shader_handle(cso, NULL);
   cso_set_geometry_shader_handle(cso, NULL);
   cso_set_vertex_shader_handle(cso, hud->vs);
   cso_set_vertex_elements(cso, 2, hud->velems);
   cso_set_render_condition(cso, NULL, FALSE, 0);
   cso_set_sampler_views(cso, PIPE_SHADER_FRAGMENT, 1,
                         &hud->font_sampler_view);
   cso_set_samplers(cso, PIPE_SHADER_FRAGMENT, 1, sampler_states);
   cso_set_constant_buffer(cso, PIPE_SHADER_VERTEX, 0, &hud->constbuf);

   /* prepare vertex buffers */
   hud_prepare_vertices(hud, &hud->bg, 16 * 256, 2 * sizeof(float));
   hud_prepare_vertices(hud, &hud->whitelines, 4 * 256, 2 * sizeof(float));
   hud_prepare_vertices(hud, &hud->text, 16 * 1024, 4 * sizeof(float));
   hud_prepare_vertices(hud, &hud->color_prims, 32 * 1024, 2 * sizeof(float));

   /* Allocate everything once and divide the storage into 3 portions
    * manually, because u_upload_alloc can unmap memory from previous calls.
    */
   u_upload_alloc(hud->pipe->stream_uploader, 0,
                  hud->bg.buffer_size +
                  hud->whitelines.buffer_size +
                  hud->text.buffer_size +
                  hud->color_prims.buffer_size,
                  16, &hud->bg.vbuf.buffer_offset, &hud->bg.vbuf.buffer.resource,
                  (void**)&hud->bg.vertices);
   if (!hud->bg.vertices) {
      goto out;
   }

   pipe_resource_reference(&hud->whitelines.vbuf.buffer.resource, hud->bg.vbuf.buffer.resource);
   pipe_resource_reference(&hud->text.vbuf.buffer.resource, hud->bg.vbuf.buffer.resource);
   pipe_resource_reference(&hud->color_prims.vbuf.buffer.resource, hud->bg.vbuf.buffer.resource);

   hud->whitelines.vbuf.buffer_offset = hud->bg.vbuf.buffer_offset +
                                        hud->bg.buffer_size;
   hud->whitelines.vertices = hud->bg.vertices +
                              hud->bg.buffer_size / sizeof(float);

   hud->text.vbuf.buffer_offset = hud->whitelines.vbuf.buffer_offset +
                                  hud->whitelines.buffer_size;
   hud->text.vertices = hud->whitelines.vertices +
                        hud->whitelines.buffer_size / sizeof(float);

   hud->color_prims.vbuf.buffer_offset = hud->text.vbuf.buffer_offset +
                                         hud->text.buffer_size;
   hud->color_prims.vertices = hud->text.vertices +
                               hud->text.buffer_size / sizeof(float);

   /* prepare all graphs */
   hud_batch_query_update(hud->batch_query);

   LIST_FOR_EACH_ENTRY(pane, &hud->pane_list, head) {
      LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
         gr->query_new_value(gr);
      }

      if (pane->sort_items) {
         LIST_FOR_EACH_ENTRY_SAFE(gr, next, &pane->graph_list, head) {
            /* ignore the last one */
            if (&gr->head == pane->graph_list.prev)
               continue;

            /* This is an incremental bubble sort, because we only do one pass
             * per frame. It will eventually reach an equilibrium.
             */
            if (gr->current_value <
                LIST_ENTRY(struct hud_graph, next, head)->current_value) {
               LIST_DEL(&gr->head);
               LIST_ADD(&gr->head, &next->head);
            }
         }
      }

      hud_pane_accumulate_vertices(hud, pane);
   }

   /* unmap the uploader's vertex buffer before drawing */
   u_upload_unmap(pipe->stream_uploader);

   /* draw accumulated vertices for background quads */
   cso_set_blend(cso, &hud->alpha_blend);
   cso_set_fragment_shader_handle(hud->cso, hud->fs_color);

   if (hud->bg.num_vertices) {
      hud->constants.color[0] = 0;
      hud->constants.color[1] = 0;
      hud->constants.color[2] = 0;
      hud->constants.color[3] = 0.666f;
      hud->constants.translate[0] = 0;
      hud->constants.translate[1] = 0;
      hud->constants.scale[0] = 1;
      hud->constants.scale[1] = 1;

      cso_set_constant_buffer(cso, PIPE_SHADER_VERTEX, 0, &hud->constbuf);
      cso_set_vertex_buffers(cso, cso_get_aux_vertex_buffer_slot(cso), 1,
                             &hud->bg.vbuf);
      cso_draw_arrays(cso, PIPE_PRIM_QUADS, 0, hud->bg.num_vertices);
   }
   pipe_resource_reference(&hud->bg.vbuf.buffer.resource, NULL);

   /* draw accumulated vertices for white lines */
   cso_set_blend(cso, &hud->no_blend);

   hud->constants.color[0] = 1;
   hud->constants.color[1] = 1;
   hud->constants.color[2] = 1;
   hud->constants.color[3] = 1;
   hud->constants.translate[0] = 0;
   hud->constants.translate[1] = 0;
   hud->constants.scale[0] = 1;
   hud->constants.scale[1] = 1;
   cso_set_constant_buffer(cso, PIPE_SHADER_VERTEX, 0, &hud->constbuf);

   if (hud->whitelines.num_vertices) {
      cso_set_vertex_buffers(cso, cso_get_aux_vertex_buffer_slot(cso), 1,
                             &hud->whitelines.vbuf);
      cso_set_fragment_shader_handle(hud->cso, hud->fs_color);
      cso_draw_arrays(cso, PIPE_PRIM_LINES, 0, hud->whitelines.num_vertices);
   }
   pipe_resource_reference(&hud->whitelines.vbuf.buffer.resource, NULL);

   /* draw accumulated vertices for text */
   cso_set_blend(cso, &hud->alpha_blend);
   if (hud->text.num_vertices) {
      cso_set_vertex_buffers(cso, cso_get_aux_vertex_buffer_slot(cso), 1,
                             &hud->text.vbuf);
      cso_set_fragment_shader_handle(hud->cso, hud->fs_text);
      cso_draw_arrays(cso, PIPE_PRIM_QUADS, 0, hud->text.num_vertices);
   }
   pipe_resource_reference(&hud->text.vbuf.buffer.resource, NULL);

   /* draw the rest */
   cso_set_rasterizer(cso, &hud->rasterizer_aa_lines);
   LIST_FOR_EACH_ENTRY(pane, &hud->pane_list, head) {
      if (pane)
         hud_pane_draw_colored_objects(hud, pane);
   }

out:
   cso_restore_state(cso);
   cso_restore_constant_buffer_slot0(cso, PIPE_SHADER_VERTEX);

   pipe_surface_reference(&surf, NULL);

   /* Start queries. */
   hud_batch_query_begin(hud->batch_query);

   LIST_FOR_EACH_ENTRY(pane, &hud->pane_list, head) {
      LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
         if (gr->begin_query)
            gr->begin_query(gr);
      }
   }
}

static void
fixup_bytes(enum pipe_driver_query_type type, int position, uint64_t *exp10)
{
   if (type == PIPE_DRIVER_QUERY_TYPE_BYTES && position % 3 == 0)
      *exp10 = (*exp10 / 1000) * 1024;
}

/**
 * Set the maximum value for the Y axis of the graph.
 * This scales the graph accordingly.
 */
void
hud_pane_set_max_value(struct hud_pane *pane, uint64_t value)
{
   double leftmost_digit;
   uint64_t exp10;
   int i;

   /* The following code determines the max_value in the graph as well as
    * how many describing lines are drawn. The max_value is rounded up,
    * so that all drawn numbers are rounded for readability.
    * We want to print multiples of a simple number instead of multiples of
    * hard-to-read numbers like 1.753.
    */

   /* Find the left-most digit. Make sure exp10 * 10 and fixup_bytes doesn't
    * overflow. (11 is safe) */
   exp10 = 1;
   for (i = 0; exp10 <= UINT64_MAX / 11 && exp10 * 9 < value; i++) {
      exp10 *= 10;
      fixup_bytes(pane->type, i + 1, &exp10);
   }

   leftmost_digit = DIV_ROUND_UP(value, exp10);

   /* Round 9 to 10. */
   if (leftmost_digit == 9) {
      leftmost_digit = 1;
      exp10 *= 10;
      fixup_bytes(pane->type, i + 1, &exp10);
   }

   switch ((unsigned)leftmost_digit) {
   case 1:
      pane->last_line = 5; /* lines in +1/5 increments */
      break;
   case 2:
      pane->last_line = 8; /* lines in +1/4 increments. */
      break;
   case 3:
   case 4:
      pane->last_line = leftmost_digit * 2; /* lines in +1/2 increments */
      break;
   case 5:
   case 6:
   case 7:
   case 8:
      pane->last_line = leftmost_digit; /* lines in +1 increments */
      break;
   default:
      assert(0);
   }

   /* Truncate {3,4} to {2.5, 3.5} if possible. */
   for (i = 3; i <= 4; i++) {
      if (leftmost_digit == i && value <= (i - 0.5) * exp10) {
         leftmost_digit = i - 0.5;
         pane->last_line = leftmost_digit * 2; /* lines in +1/2 increments. */
      }
   }

   /* Truncate 2 to a multiple of 0.2 in (1, 1.6] if possible. */
   if (leftmost_digit == 2) {
      for (i = 1; i <= 3; i++) {
         if (value <= (1 + i*0.2) * exp10) {
            leftmost_digit = 1 + i*0.2;
            pane->last_line = 5 + i; /* lines in +1/5 increments. */
            break;
         }
      }
   }

   pane->max_value = leftmost_digit * exp10;
   pane->yscale = -(int)pane->inner_height / (float)pane->max_value;
}

static void
hud_pane_update_dyn_ceiling(struct hud_graph *gr, struct hud_pane *pane)
{
   unsigned i;
   float tmp = 0.0f;

   if (pane->dyn_ceil_last_ran != gr->index) {
      LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
         for (i = 0; i <  gr->num_vertices; ++i) {
            tmp = gr->vertices[i * 2 + 1] > tmp ?
                  gr->vertices[i * 2 + 1] : tmp;
         }
      }

      /* Avoid setting it lower than the initial starting height. */
      tmp = tmp > pane->initial_max_value ? tmp : pane->initial_max_value;
      hud_pane_set_max_value(pane, tmp);
   }

   /*
    * Mark this adjustment run so we could avoid repeating a full update
    * again needlessly in case the pane has more than one graph.
    */
   pane->dyn_ceil_last_ran = gr->index;
}

static struct hud_pane *
hud_pane_create(struct hud_context *hud,
                unsigned x1, unsigned y1, unsigned x2, unsigned y2,
                unsigned period, uint64_t max_value, uint64_t ceiling,
                boolean dyn_ceiling, boolean sort_items)
{
   struct hud_pane *pane = CALLOC_STRUCT(hud_pane);

   if (!pane)
      return NULL;

   pane->hud = hud;
   pane->x1 = x1;
   pane->y1 = y1;
   pane->x2 = x2;
   pane->y2 = y2;
   pane->inner_x1 = x1 + 1;
   pane->inner_x2 = x2 - 1;
   pane->inner_y1 = y1 + 1;
   pane->inner_y2 = y2 - 1;
   pane->inner_width = pane->inner_x2 - pane->inner_x1;
   pane->inner_height = pane->inner_y2 - pane->inner_y1;
   pane->period = period;
   pane->max_num_vertices = (x2 - x1 + 2) / 2;
   pane->ceiling = ceiling;
   pane->dyn_ceiling = dyn_ceiling;
   pane->dyn_ceil_last_ran = 0;
   pane->sort_items = sort_items;
   pane->initial_max_value = max_value;
   hud_pane_set_max_value(pane, max_value);
   LIST_INITHEAD(&pane->graph_list);
   return pane;
}

/* replace '-' with a space */
static void
strip_hyphens(char *s)
{
   while (*s) {
      if (*s == '-')
         *s = ' ';
      s++;
   }
}

/**
 * Add a graph to an existing pane.
 * One pane can contain multiple graphs over each other.
 */
void
hud_pane_add_graph(struct hud_pane *pane, struct hud_graph *gr)
{
   static const float colors[][3] = {
      {0, 1, 0},
      {1, 0, 0},
      {0, 1, 1},
      {1, 0, 1},
      {1, 1, 0},
      {0.5, 1, 0.5},
      {1, 0.5, 0.5},
      {0.5, 1, 1},
      {1, 0.5, 1},
      {1, 1, 0.5},
      {0, 0.5, 0},
      {0.5, 0, 0},
      {0, 0.5, 0.5},
      {0.5, 0, 0.5},
      {0.5, 0.5, 0},
   };
   unsigned color = pane->next_color % ARRAY_SIZE(colors);

   strip_hyphens(gr->name);

   gr->vertices = MALLOC(pane->max_num_vertices * sizeof(float) * 2);
   gr->color[0] = colors[color][0];
   gr->color[1] = colors[color][1];
   gr->color[2] = colors[color][2];
   gr->pane = pane;
   LIST_ADDTAIL(&gr->head, &pane->graph_list);
   pane->num_graphs++;
   pane->next_color++;
}

void
hud_graph_add_value(struct hud_graph *gr, double value)
{
   gr->current_value = value;
   value = value > gr->pane->ceiling ? gr->pane->ceiling : value;

   if (gr->fd) {
      if (fabs(value - lround(value)) > FLT_EPSILON) {
         fprintf(gr->fd, "%f\n", value);
      }
      else {
         fprintf(gr->fd, "%" PRIu64 "\n", (uint64_t) lround(value));
      }
   }

   if (gr->index == gr->pane->max_num_vertices) {
      gr->vertices[0] = 0;
      gr->vertices[1] = gr->vertices[(gr->index-1)*2+1];
      gr->index = 1;
   }
   gr->vertices[(gr->index)*2+0] = (float) (gr->index * 2);
   gr->vertices[(gr->index)*2+1] = (float) value;
   gr->index++;

   if (gr->num_vertices < gr->pane->max_num_vertices) {
      gr->num_vertices++;
   }

   if (gr->pane->dyn_ceiling == true) {
      hud_pane_update_dyn_ceiling(gr, gr->pane);
   }
   if (value > gr->pane->max_value) {
      hud_pane_set_max_value(gr->pane, value);
   }
}

static void
hud_graph_destroy(struct hud_graph *graph)
{
   FREE(graph->vertices);
   if (graph->free_query_data)
      graph->free_query_data(graph->query_data);
   if (graph->fd)
      fclose(graph->fd);
   FREE(graph);
}

static void strcat_without_spaces(char *dst, const char *src)
{
   dst += strlen(dst);
   while (*src) {
      if (*src == ' ')
         *dst++ = '_';
      else
         *dst++ = *src;
      src++;
   }
   *dst = 0;
}


#ifdef PIPE_OS_WINDOWS
#define W_OK 0
static int
access(const char *pathname, int mode)
{
   /* no-op */
   return 0;
}

#define PATH_SEP "\\"

#else

#define PATH_SEP "/"

#endif


/**
 * If the GALLIUM_HUD_DUMP_DIR env var is set, we'll write the raw
 * HUD values to files at ${GALLIUM_HUD_DUMP_DIR}/<stat> where <stat>
 * is a HUD variable such as "fps", or "cpu"
 */
static void
hud_graph_set_dump_file(struct hud_graph *gr)
{
   const char *hud_dump_dir = getenv("GALLIUM_HUD_DUMP_DIR");

   if (hud_dump_dir && access(hud_dump_dir, W_OK) == 0) {
      char *dump_file = malloc(strlen(hud_dump_dir) + sizeof(PATH_SEP)
                               + sizeof(gr->name));
      if (dump_file) {
         strcpy(dump_file, hud_dump_dir);
         strcat(dump_file, PATH_SEP);
         strcat_without_spaces(dump_file, gr->name);
         gr->fd = fopen(dump_file, "w+");
         if (gr->fd) {
            /* flush output after each line is written */
            setvbuf(gr->fd, NULL, _IOLBF, 0);
         }
         free(dump_file);
      }
   }
}

/**
 * Read a string from the environment variable.
 * The separators "+", ",", ":", and ";" terminate the string.
 * Return the number of read characters.
 */
static int
parse_string(const char *s, char *out)
{
   int i;

   for (i = 0; *s && *s != '+' && *s != ',' && *s != ':' && *s != ';' && *s != '=';
        s++, out++, i++)
      *out = *s;

   *out = 0;

   if (*s && !i) {
      fprintf(stderr, "gallium_hud: syntax error: unexpected '%c' (%i) while "
              "parsing a string\n", *s, *s);
      fflush(stderr);
   }

   return i;
}

static char *
read_pane_settings(char *str, unsigned * const x, unsigned * const y,
               unsigned * const width, unsigned * const height,
               uint64_t * const ceiling, boolean * const dyn_ceiling,
               boolean *reset_colors, boolean *sort_items)
{
   char *ret = str;
   unsigned tmp;

   while (*str == '.') {
      ++str;
      switch (*str) {
      case 'x':
         ++str;
         *x = strtoul(str, &ret, 10);
         str = ret;
         break;

      case 'y':
         ++str;
         *y = strtoul(str, &ret, 10);
         str = ret;
         break;

      case 'w':
         ++str;
         tmp = strtoul(str, &ret, 10);
         *width = tmp > 80 ? tmp : 80; /* 80 is chosen arbitrarily */
         str = ret;
         break;

      /*
       * Prevent setting height to less than 50. If the height is set to less,
       * the text of the Y axis labels on the graph will start overlapping.
       */
      case 'h':
         ++str;
         tmp = strtoul(str, &ret, 10);
         *height = tmp > 50 ? tmp : 50;
         str = ret;
         break;

      case 'c':
         ++str;
         tmp = strtoul(str, &ret, 10);
         *ceiling = tmp > 10 ? tmp : 10;
         str = ret;
         break;

      case 'd':
         ++str;
         ret = str;
         *dyn_ceiling = true;
         break;

      case 'r':
         ++str;
         ret = str;
         *reset_colors = true;
         break;

      case 's':
         ++str;
         ret = str;
         *sort_items = true;
         break;

      default:
         fprintf(stderr, "gallium_hud: syntax error: unexpected '%c'\n", *str);
         fflush(stderr);
      }

   }

   return ret;
}

static boolean
has_occlusion_query(struct pipe_screen *screen)
{
   return screen->get_param(screen, PIPE_CAP_OCCLUSION_QUERY) != 0;
}

static boolean
has_streamout(struct pipe_screen *screen)
{
   return screen->get_param(screen, PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS) != 0;
}

static boolean
has_pipeline_stats_query(struct pipe_screen *screen)
{
   return screen->get_param(screen, PIPE_CAP_QUERY_PIPELINE_STATISTICS) != 0;
}

static void
hud_parse_env_var(struct hud_context *hud, const char *env)
{
   unsigned num, i;
   char name_a[256], s[256];
   char *name;
   struct hud_pane *pane = NULL;
   unsigned x = 10, y = 10;
   unsigned width = 251, height = 100;
   unsigned period = 500 * 1000;  /* default period (1/2 second) */
   uint64_t ceiling = UINT64_MAX;
   unsigned column_width = 251;
   boolean dyn_ceiling = false;
   boolean reset_colors = false;
   boolean sort_items = false;
   const char *period_env;

   /*
    * The GALLIUM_HUD_PERIOD env var sets the graph update rate.
    * The env var is in seconds (a float).
    * Zero means update after every frame.
    */
   period_env = getenv("GALLIUM_HUD_PERIOD");
   if (period_env) {
      float p = (float) atof(period_env);
      if (p >= 0.0f) {
         period = (unsigned) (p * 1000 * 1000);
      }
   }

   while ((num = parse_string(env, name_a)) != 0) {
      env += num;

      /* check for explicit location, size and etc. settings */
      name = read_pane_settings(name_a, &x, &y, &width, &height, &ceiling,
                                &dyn_ceiling, &reset_colors, &sort_items);

     /*
      * Keep track of overall column width to avoid pane overlapping in case
      * later we create a new column while the bottom pane in the current
      * column is less wide than the rest of the panes in it.
      */
     column_width = width > column_width ? width : column_width;

      if (!pane) {
         pane = hud_pane_create(hud, x, y, x + width, y + height, period, 10,
                                ceiling, dyn_ceiling, sort_items);
         if (!pane)
            return;
      }

      if (reset_colors) {
         pane->next_color = 0;
         reset_colors = false;
      }

      /* Add a graph. */
#if HAVE_GALLIUM_EXTRA_HUD || HAVE_LIBSENSORS
      char arg_name[64];
#endif
      /* IF YOU CHANGE THIS, UPDATE print_help! */
      if (strcmp(name, "fps") == 0) {
         hud_fps_graph_install(pane);
      }
      else if (strcmp(name, "cpu") == 0) {
         hud_cpu_graph_install(pane, ALL_CPUS);
      }
      else if (sscanf(name, "cpu%u%s", &i, s) == 1) {
         hud_cpu_graph_install(pane, i);
      }
      else if (strcmp(name, "API-thread-busy") == 0) {
         hud_thread_busy_install(pane, name, false);
      }
      else if (strcmp(name, "API-thread-offloaded-slots") == 0) {
         hud_thread_counter_install(pane, name, HUD_COUNTER_OFFLOADED);
      }
      else if (strcmp(name, "API-thread-direct-slots") == 0) {
         hud_thread_counter_install(pane, name, HUD_COUNTER_DIRECT);
      }
      else if (strcmp(name, "API-thread-num-syncs") == 0) {
         hud_thread_counter_install(pane, name, HUD_COUNTER_SYNCS);
      }
      else if (strcmp(name, "main-thread-busy") == 0) {
         hud_thread_busy_install(pane, name, true);
      }
#if HAVE_GALLIUM_EXTRA_HUD
      else if (sscanf(name, "nic-rx-%s", arg_name) == 1) {
         hud_nic_graph_install(pane, arg_name, NIC_DIRECTION_RX);
      }
      else if (sscanf(name, "nic-tx-%s", arg_name) == 1) {
         hud_nic_graph_install(pane, arg_name, NIC_DIRECTION_TX);
      }
      else if (sscanf(name, "nic-rssi-%s", arg_name) == 1) {
         hud_nic_graph_install(pane, arg_name, NIC_RSSI_DBM);
         pane->type = PIPE_DRIVER_QUERY_TYPE_DBM;
      }
      else if (sscanf(name, "diskstat-rd-%s", arg_name) == 1) {
         hud_diskstat_graph_install(pane, arg_name, DISKSTAT_RD);
         pane->type = PIPE_DRIVER_QUERY_TYPE_BYTES;
      }
      else if (sscanf(name, "diskstat-wr-%s", arg_name) == 1) {
         hud_diskstat_graph_install(pane, arg_name, DISKSTAT_WR);
         pane->type = PIPE_DRIVER_QUERY_TYPE_BYTES;
      }
      else if (sscanf(name, "cpufreq-min-cpu%u", &i) == 1) {
         hud_cpufreq_graph_install(pane, i, CPUFREQ_MINIMUM);
         pane->type = PIPE_DRIVER_QUERY_TYPE_HZ;
      }
      else if (sscanf(name, "cpufreq-cur-cpu%u", &i) == 1) {
         hud_cpufreq_graph_install(pane, i, CPUFREQ_CURRENT);
         pane->type = PIPE_DRIVER_QUERY_TYPE_HZ;
      }
      else if (sscanf(name, "cpufreq-max-cpu%u", &i) == 1) {
         hud_cpufreq_graph_install(pane, i, CPUFREQ_MAXIMUM);
         pane->type = PIPE_DRIVER_QUERY_TYPE_HZ;
      }
#endif
#if HAVE_LIBSENSORS
      else if (sscanf(name, "sensors_temp_cu-%s", arg_name) == 1) {
         hud_sensors_temp_graph_install(pane, arg_name,
                                        SENSORS_TEMP_CURRENT);
         pane->type = PIPE_DRIVER_QUERY_TYPE_TEMPERATURE;
      }
      else if (sscanf(name, "sensors_temp_cr-%s", arg_name) == 1) {
         hud_sensors_temp_graph_install(pane, arg_name,
                                        SENSORS_TEMP_CRITICAL);
         pane->type = PIPE_DRIVER_QUERY_TYPE_TEMPERATURE;
      }
      else if (sscanf(name, "sensors_volt_cu-%s", arg_name) == 1) {
         hud_sensors_temp_graph_install(pane, arg_name,
                                        SENSORS_VOLTAGE_CURRENT);
         pane->type = PIPE_DRIVER_QUERY_TYPE_VOLTS;
      }
      else if (sscanf(name, "sensors_curr_cu-%s", arg_name) == 1) {
         hud_sensors_temp_graph_install(pane, arg_name,
                                        SENSORS_CURRENT_CURRENT);
         pane->type = PIPE_DRIVER_QUERY_TYPE_AMPS;
      }
      else if (sscanf(name, "sensors_pow_cu-%s", arg_name) == 1) {
         hud_sensors_temp_graph_install(pane, arg_name,
                                        SENSORS_POWER_CURRENT);
         pane->type = PIPE_DRIVER_QUERY_TYPE_WATTS;
      }
#endif
      else if (strcmp(name, "samples-passed") == 0 &&
               has_occlusion_query(hud->pipe->screen)) {
         hud_pipe_query_install(&hud->batch_query, pane, hud->pipe,
                                "samples-passed",
                                PIPE_QUERY_OCCLUSION_COUNTER, 0, 0,
                                PIPE_DRIVER_QUERY_TYPE_UINT64,
                                PIPE_DRIVER_QUERY_RESULT_TYPE_AVERAGE,
                                0);
      }
      else if (strcmp(name, "primitives-generated") == 0 &&
               has_streamout(hud->pipe->screen)) {
         hud_pipe_query_install(&hud->batch_query, pane, hud->pipe,
                                "primitives-generated",
                                PIPE_QUERY_PRIMITIVES_GENERATED, 0, 0,
                                PIPE_DRIVER_QUERY_TYPE_UINT64,
                                PIPE_DRIVER_QUERY_RESULT_TYPE_AVERAGE,
                                0);
      }
      else {
         boolean processed = FALSE;

         /* pipeline statistics queries */
         if (has_pipeline_stats_query(hud->pipe->screen)) {
            static const char *pipeline_statistics_names[] =
            {
               "ia-vertices",
               "ia-primitives",
               "vs-invocations",
               "gs-invocations",
               "gs-primitives",
               "clipper-invocations",
               "clipper-primitives-generated",
               "ps-invocations",
               "hs-invocations",
               "ds-invocations",
               "cs-invocations"
            };
            for (i = 0; i < ARRAY_SIZE(pipeline_statistics_names); ++i)
               if (strcmp(name, pipeline_statistics_names[i]) == 0)
                  break;
            if (i < ARRAY_SIZE(pipeline_statistics_names)) {
               hud_pipe_query_install(&hud->batch_query, pane, hud->pipe, name,
                                      PIPE_QUERY_PIPELINE_STATISTICS, i,
                                      0, PIPE_DRIVER_QUERY_TYPE_UINT64,
                                      PIPE_DRIVER_QUERY_RESULT_TYPE_AVERAGE,
                                      0);
               processed = TRUE;
            }
         }

         /* driver queries */
         if (!processed) {
            if (!hud_driver_query_install(&hud->batch_query, pane, hud->pipe,
                                          name)) {
               fprintf(stderr, "gallium_hud: unknown driver query '%s'\n", name);
               fflush(stderr);
            }
         }
      }

      if (*env == ':') {
         env++;

         if (!pane) {
            fprintf(stderr, "gallium_hud: syntax error: unexpected ':', "
                    "expected a name\n");
            fflush(stderr);
            break;
         }

         num = parse_string(env, s);
         env += num;

         if (num && sscanf(s, "%u", &i) == 1) {
            hud_pane_set_max_value(pane, i);
            pane->initial_max_value = i;
         }
         else {
            fprintf(stderr, "gallium_hud: syntax error: unexpected '%c' (%i) "
                            "after ':'\n", *env, *env);
            fflush(stderr);
         }
      }

      if (*env == '=') {
         env++;

         if (!pane) {
            fprintf(stderr, "gallium_hud: syntax error: unexpected '=', "
                    "expected a name\n");
            fflush(stderr);
            break;
         }

         num = parse_string(env, s);
         env += num;

         strip_hyphens(s);
         if (!LIST_IS_EMPTY(&pane->graph_list)) {
            struct hud_graph *graph;
            graph = LIST_ENTRY(struct hud_graph, pane->graph_list.prev, head);
            strncpy(graph->name, s, sizeof(graph->name)-1);
            graph->name[sizeof(graph->name)-1] = 0;
         }
      }

      if (*env == 0)
         break;

      /* parse a separator */
      switch (*env) {
      case '+':
         env++;
         break;

      case ',':
         env++;
         if (!pane)
            break;

         y += height + hud->font.glyph_height * (pane->num_graphs + 2);
         height = 100;

         if (pane && pane->num_graphs) {
            LIST_ADDTAIL(&pane->head, &hud->pane_list);
            pane = NULL;
         }
         break;

      case ';':
         env++;
         y = 10;
         x += column_width + hud->font.glyph_width * 9;
         height = 100;

         if (pane && pane->num_graphs) {
            LIST_ADDTAIL(&pane->head, &hud->pane_list);
            pane = NULL;
         }

         /* Starting a new column; reset column width. */
         column_width = 251;
         break;

      default:
         fprintf(stderr, "gallium_hud: syntax error: unexpected '%c'\n", *env);
         fflush(stderr);
      }

      /* Reset to defaults for the next pane in case these were modified. */
      width = 251;
      ceiling = UINT64_MAX;
      dyn_ceiling = false;
      sort_items = false;

   }

   if (pane) {
      if (pane->num_graphs) {
         LIST_ADDTAIL(&pane->head, &hud->pane_list);
      }
      else {
         FREE(pane);
      }
   }

   LIST_FOR_EACH_ENTRY(pane, &hud->pane_list, head) {
      struct hud_graph *gr;

      LIST_FOR_EACH_ENTRY(gr, &pane->graph_list, head) {
         hud_graph_set_dump_file(gr);
      }
   }
}

static void
print_help(struct pipe_screen *screen)
{
   int i, num_queries, num_cpus = hud_get_num_cpus();

   puts("Syntax: GALLIUM_HUD=name1[+name2][...][:value1][,nameI...][;nameJ...]");
   puts("");
   puts("  Names are identifiers of data sources which will be drawn as graphs");
   puts("  in panes. Multiple graphs can be drawn in the same pane.");
   puts("  There can be multiple panes placed in rows and columns.");
   puts("");
   puts("  '+' separates names which will share a pane.");
   puts("  ':[value]' specifies the initial maximum value of the Y axis");
   puts("             for the given pane.");
   puts("  ',' creates a new pane below the last one.");
   puts("  ';' creates a new pane at the top of the next column.");
   puts("  '=' followed by a string, changes the name of the last data source");
   puts("      to that string");
   puts("");
   puts("  Example: GALLIUM_HUD=\"cpu,fps;primitives-generated\"");
   puts("");
   puts("  Additionally, by prepending '.[identifier][value]' modifiers to");
   puts("  a name, it is possible to explicitly set the location and size");
   puts("  of a pane, along with limiting overall maximum value of the");
   puts("  Y axis and activating dynamic readjustment of the Y axis.");
   puts("  Several modifiers may be applied to the same pane simultaneously.");
   puts("");
   puts("  'x[value]' sets the location of the pane on the x axis relative");
   puts("             to the upper-left corner of the viewport, in pixels.");
   puts("  'y[value]' sets the location of the pane on the y axis relative");
   puts("             to the upper-left corner of the viewport, in pixels.");
   puts("  'w[value]' sets width of the graph pixels.");
   puts("  'h[value]' sets height of the graph in pixels.");
   puts("  'c[value]' sets the ceiling of the value of the Y axis.");
   puts("             If the graph needs to draw values higher than");
   puts("             the ceiling allows, the value is clamped.");
   puts("  'd' activates dynamic Y axis readjustment to set the value of");
   puts("      the Y axis to match the highest value still visible in the graph.");
   puts("  'r' resets the color counter (the next color will be green)");
   puts("  's' sort items below graphs in descending order");
   puts("");
   puts("  If 'c' and 'd' modifiers are used simultaneously, both are in effect:");
   puts("  the Y axis does not go above the restriction imposed by 'c' while");
   puts("  still adjusting the value of the Y axis down when appropriate.");
   puts("");
   puts("  Example: GALLIUM_HUD=\".w256.h64.x1600.y520.d.c1000fps+cpu,.datom-count\"");
   puts("");
   puts("  Available names:");
   puts("    fps");
   puts("    cpu");

   for (i = 0; i < num_cpus; i++)
      printf("    cpu%i\n", i);

   if (has_occlusion_query(screen))
      puts("    samples-passed");
   if (has_streamout(screen))
      puts("    primitives-generated");

   if (has_pipeline_stats_query(screen)) {
      puts("    ia-vertices");
      puts("    ia-primitives");
      puts("    vs-invocations");
      puts("    gs-invocations");
      puts("    gs-primitives");
      puts("    clipper-invocations");
      puts("    clipper-primitives-generated");
      puts("    ps-invocations");
      puts("    hs-invocations");
      puts("    ds-invocations");
      puts("    cs-invocations");
   }

#if HAVE_GALLIUM_EXTRA_HUD
   hud_get_num_disks(1);
   hud_get_num_nics(1);
   hud_get_num_cpufreq(1);
#endif
#if HAVE_LIBSENSORS
   hud_get_num_sensors(1);
#endif

   if (screen->get_driver_query_info){
      boolean skipping = false;
      struct pipe_driver_query_info info;
      num_queries = screen->get_driver_query_info(screen, 0, NULL);

      for (i = 0; i < num_queries; i++){
         screen->get_driver_query_info(screen, i, &info);
         if (info.flags & PIPE_DRIVER_QUERY_FLAG_DONT_LIST) {
            if (!skipping)
               puts("    ...");
            skipping = true;
         } else {
            printf("    %s\n", info.name);
            skipping = false;
         }
      }
   }

   puts("");
   fflush(stdout);
}

struct hud_context *
hud_create(struct pipe_context *pipe, struct cso_context *cso)
{
   struct pipe_screen *screen = pipe->screen;
   struct hud_context *hud;
   struct pipe_sampler_view view_templ;
   unsigned i;
   const char *env = debug_get_option("GALLIUM_HUD", NULL);
#ifdef PIPE_OS_UNIX
   unsigned signo = debug_get_num_option("GALLIUM_HUD_TOGGLE_SIGNAL", 0);
   static boolean sig_handled = FALSE;
   struct sigaction action = {};
#endif
   huds_visible = debug_get_bool_option("GALLIUM_HUD_VISIBLE", TRUE);

   if (!env || !*env)
      return NULL;

   if (strcmp(env, "help") == 0) {
      print_help(pipe->screen);
      return NULL;
   }

   hud = CALLOC_STRUCT(hud_context);
   if (!hud)
      return NULL;

   hud->pipe = pipe;
   hud->cso = cso;

   /* font */
   if (!util_font_create(pipe, UTIL_FONT_FIXED_8X13, &hud->font)) {
      FREE(hud);
      return NULL;
   }

   hud->has_srgb = screen->is_format_supported(screen,
                                               PIPE_FORMAT_B8G8R8A8_SRGB,
                                               PIPE_TEXTURE_2D, 0,
                                               PIPE_BIND_RENDER_TARGET) != 0;

   /* blend state */
   hud->no_blend.rt[0].colormask = PIPE_MASK_RGBA;

   hud->alpha_blend.rt[0].colormask = PIPE_MASK_RGBA;
   hud->alpha_blend.rt[0].blend_enable = 1;
   hud->alpha_blend.rt[0].rgb_func = PIPE_BLEND_ADD;
   hud->alpha_blend.rt[0].rgb_src_factor = PIPE_BLENDFACTOR_SRC_ALPHA;
   hud->alpha_blend.rt[0].rgb_dst_factor = PIPE_BLENDFACTOR_INV_SRC_ALPHA;
   hud->alpha_blend.rt[0].alpha_func = PIPE_BLEND_ADD;
   hud->alpha_blend.rt[0].alpha_src_factor = PIPE_BLENDFACTOR_ZERO;
   hud->alpha_blend.rt[0].alpha_dst_factor = PIPE_BLENDFACTOR_ONE;

   /* fragment shader */
   hud->fs_color =
         util_make_fragment_passthrough_shader(pipe,
                                               TGSI_SEMANTIC_COLOR,
                                               TGSI_INTERPOLATE_CONSTANT,
                                               TRUE);

   {
      /* Read a texture and do .xxxx swizzling. */
      static const char *fragment_shader_text = {
         "FRAG\n"
         "DCL IN[0], GENERIC[0], LINEAR\n"
         "DCL SAMP[0]\n"
         "DCL SVIEW[0], RECT, FLOAT\n"
         "DCL OUT[0], COLOR[0]\n"
         "DCL TEMP[0]\n"

         "TEX TEMP[0], IN[0], SAMP[0], RECT\n"
         "MOV OUT[0], TEMP[0].xxxx\n"
         "END\n"
      };

      struct tgsi_token tokens[1000];
      struct pipe_shader_state state;

      if (!tgsi_text_translate(fragment_shader_text, tokens, ARRAY_SIZE(tokens))) {
         assert(0);
         pipe_resource_reference(&hud->font.texture, NULL);
         FREE(hud);
         return NULL;
      }
      pipe_shader_state_from_tgsi(&state, tokens);
      hud->fs_text = pipe->create_fs_state(pipe, &state);
   }

   /* rasterizer */
   hud->rasterizer.half_pixel_center = 1;
   hud->rasterizer.bottom_edge_rule = 1;
   hud->rasterizer.depth_clip = 1;
   hud->rasterizer.line_width = 1;
   hud->rasterizer.line_last_pixel = 1;

   hud->rasterizer_aa_lines = hud->rasterizer;
   hud->rasterizer_aa_lines.line_smooth = 1;

   /* vertex shader */
   {
      static const char *vertex_shader_text = {
         "VERT\n"
         "DCL IN[0..1]\n"
         "DCL OUT[0], POSITION\n"
         "DCL OUT[1], COLOR[0]\n" /* color */
         "DCL OUT[2], GENERIC[0]\n" /* texcoord */
         /* [0] = color,
          * [1] = (2/fb_width, 2/fb_height, xoffset, yoffset)
          * [2] = (xscale, yscale, 0, 0) */
         "DCL CONST[0][0..2]\n"
         "DCL TEMP[0]\n"
         "IMM[0] FLT32 { -1, 0, 0, 1 }\n"

         /* v = in * (xscale, yscale) + (xoffset, yoffset) */
         "MAD TEMP[0].xy, IN[0], CONST[0][2].xyyy, CONST[0][1].zwww\n"
         /* pos = v * (2 / fb_width, 2 / fb_height) - (1, 1) */
         "MAD OUT[0].xy, TEMP[0], CONST[0][1].xyyy, IMM[0].xxxx\n"
         "MOV OUT[0].zw, IMM[0]\n"

         "MOV OUT[1], CONST[0][0]\n"
         "MOV OUT[2], IN[1]\n"
         "END\n"
      };

      struct tgsi_token tokens[1000];
      struct pipe_shader_state state;
      if (!tgsi_text_translate(vertex_shader_text, tokens, ARRAY_SIZE(tokens))) {
         assert(0);
         pipe_resource_reference(&hud->font.texture, NULL);
         FREE(hud);
         return NULL;
      }
      pipe_shader_state_from_tgsi(&state, tokens);
      hud->vs = pipe->create_vs_state(pipe, &state);
   }

   /* vertex elements */
   for (i = 0; i < 2; i++) {
      hud->velems[i].src_offset = i * 2 * sizeof(float);
      hud->velems[i].src_format = PIPE_FORMAT_R32G32_FLOAT;
      hud->velems[i].vertex_buffer_index = cso_get_aux_vertex_buffer_slot(cso);
   }

   /* sampler view */
   u_sampler_view_default_template(
         &view_templ, hud->font.texture, hud->font.texture->format);
   hud->font_sampler_view = pipe->create_sampler_view(pipe, hud->font.texture,
                                                      &view_templ);

   /* sampler state (for font drawing) */
   hud->font_sampler_state.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   hud->font_sampler_state.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   hud->font_sampler_state.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   hud->font_sampler_state.normalized_coords = 0;

   /* constants */
   hud->constbuf.buffer_size = sizeof(hud->constants);
   hud->constbuf.user_buffer = &hud->constants;

   LIST_INITHEAD(&hud->pane_list);

   /* setup sig handler once for all hud contexts */
#ifdef PIPE_OS_UNIX
   if (!sig_handled && signo != 0) {
      action.sa_sigaction = &signal_visible_handler;
      action.sa_flags = SA_SIGINFO;

      if (signo >= NSIG)
         fprintf(stderr, "gallium_hud: invalid signal %u\n", signo);
      else if (sigaction(signo, &action, NULL) < 0)
         fprintf(stderr, "gallium_hud: unable to set handler for signal %u\n", signo);
      fflush(stderr);

      sig_handled = TRUE;
   }
#endif

   hud_parse_env_var(hud, env);
   return hud;
}

void
hud_destroy(struct hud_context *hud)
{
   struct pipe_context *pipe = hud->pipe;
   struct hud_pane *pane, *pane_tmp;
   struct hud_graph *graph, *graph_tmp;

   LIST_FOR_EACH_ENTRY_SAFE(pane, pane_tmp, &hud->pane_list, head) {
      LIST_FOR_EACH_ENTRY_SAFE(graph, graph_tmp, &pane->graph_list, head) {
         LIST_DEL(&graph->head);
         hud_graph_destroy(graph);
      }
      LIST_DEL(&pane->head);
      FREE(pane);
   }

   hud_batch_query_cleanup(&hud->batch_query);
   pipe->delete_fs_state(pipe, hud->fs_color);
   pipe->delete_fs_state(pipe, hud->fs_text);
   pipe->delete_vs_state(pipe, hud->vs);
   pipe_sampler_view_reference(&hud->font_sampler_view, NULL);
   pipe_resource_reference(&hud->font.texture, NULL);
   FREE(hud);
}

void
hud_add_queue_for_monitoring(struct hud_context *hud,
                             struct util_queue_monitoring *queue_info)
{
   assert(!hud->monitored_queue);
   hud->monitored_queue = queue_info;
}
