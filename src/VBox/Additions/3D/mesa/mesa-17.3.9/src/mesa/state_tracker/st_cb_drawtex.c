/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 *
 **************************************************************************/


/**
 * Implementation of glDrawTex() for GL_OES_draw_tex
 */



#include "main/imports.h"
#include "main/image.h"
#include "main/macros.h"
#include "main/teximage.h"
#include "main/framebuffer.h"
#include "program/program.h"
#include "program/prog_print.h"

#include "st_context.h"
#include "st_atom.h"
#include "st_cb_bitmap.h"
#include "st_cb_drawtex.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "pipe/p_shader_tokens.h"
#include "util/u_draw_quad.h"
#include "util/u_simple_shaders.h"
#include "util/u_upload_mgr.h"

#include "cso_cache/cso_context.h"


struct cached_shader
{
   void *handle;

   uint num_attribs;
   uint semantic_names[2 + MAX_TEXTURE_UNITS];
   uint semantic_indexes[2 + MAX_TEXTURE_UNITS];
};

#define MAX_SHADERS (2 * MAX_TEXTURE_UNITS)

/**
 * Simple linear list cache.
 * Most of the time there'll only be one cached shader.
 * XXX This should be per-st_context state.
 */
static struct cached_shader CachedShaders[MAX_SHADERS];
static GLuint NumCachedShaders = 0;


static void *
lookup_shader(struct pipe_context *pipe,
              uint num_attribs,
              const uint *semantic_names,
              const uint *semantic_indexes)
{
   GLuint i, j;

   /* look for existing shader with same attributes */
   for (i = 0; i < NumCachedShaders; i++) {
      if (CachedShaders[i].num_attribs == num_attribs) {
         GLboolean match = GL_TRUE;
         for (j = 0; j < num_attribs; j++) {
            if (semantic_names[j] != CachedShaders[i].semantic_names[j] ||
                semantic_indexes[j] != CachedShaders[i].semantic_indexes[j]) {
               match = GL_FALSE;
               break;
            }
         }
         if (match)
            return CachedShaders[i].handle;
      }
   }

   /* not found - create new one now */
   if (NumCachedShaders >= MAX_SHADERS) {
      return NULL;
   }

   CachedShaders[i].num_attribs = num_attribs;
   for (j = 0; j < num_attribs; j++) {
      CachedShaders[i].semantic_names[j] = semantic_names[j];
      CachedShaders[i].semantic_indexes[j] = semantic_indexes[j];
   }

   CachedShaders[i].handle =
      util_make_vertex_passthrough_shader(pipe,
                                          num_attribs,
                                          semantic_names,
                                          semantic_indexes, FALSE);
   NumCachedShaders++;

   return CachedShaders[i].handle;
}


static void
st_DrawTex(struct gl_context *ctx, GLfloat x, GLfloat y, GLfloat z,
           GLfloat width, GLfloat height)
{
   struct st_context *st = ctx->st;
   struct pipe_context *pipe = st->pipe;
   struct cso_context *cso = st->cso_context;
   struct pipe_resource *vbuffer = NULL;
   GLuint i, numTexCoords, numAttribs;
   GLboolean emitColor;
   uint semantic_names[2 + MAX_TEXTURE_UNITS];
   uint semantic_indexes[2 + MAX_TEXTURE_UNITS];
   struct pipe_vertex_element velements[2 + MAX_TEXTURE_UNITS];
   unsigned offset;

   st_flush_bitmap_cache(st);
   st_invalidate_readpix_cache(st);

   st_validate_state(st, ST_PIPELINE_RENDER);

   /* determine if we need vertex color */
   if (ctx->FragmentProgram._Current->info.inputs_read & VARYING_BIT_COL0)
      emitColor = GL_TRUE;
   else
      emitColor = GL_FALSE;

   /* determine how many enabled sets of texcoords */
   numTexCoords = 0;
   for (i = 0; i < ctx->Const.MaxTextureUnits; i++) {
      if (ctx->Texture.Unit[i]._Current &&
          ctx->Texture.Unit[i]._Current->Target == GL_TEXTURE_2D) {
         numTexCoords++;
      }
   }

   /* total number of attributes per vertex */
   numAttribs = 1 + emitColor + numTexCoords;

   /* load vertex buffer */
   {
#define SET_ATTRIB(VERT, ATTR, X, Y, Z, W)                              \
      do {                                                              \
         GLuint k = (((VERT) * numAttribs + (ATTR)) * 4);               \
         assert(k < 4 * 4 * numAttribs);                                \
         vbuf[k + 0] = X;                                               \
         vbuf[k + 1] = Y;                                               \
         vbuf[k + 2] = Z;                                               \
         vbuf[k + 3] = W;                                               \
      } while (0)

      const GLfloat x0 = x, y0 = y, x1 = x + width, y1 = y + height;
      GLfloat *vbuf = NULL;
      GLuint tex_attr;

      u_upload_alloc(pipe->stream_uploader, 0,
                     numAttribs * 4 * 4 * sizeof(GLfloat), 4,
                     &offset, &vbuffer, (void **) &vbuf);
      if (!vbuffer) {
         return;
      }

      z = CLAMP(z, 0.0f, 1.0f);

      /* positions (in clip coords) */
      {
         const struct gl_framebuffer *fb = ctx->DrawBuffer;
         const GLfloat fb_width = (GLfloat)_mesa_geometric_width(fb);
         const GLfloat fb_height = (GLfloat)_mesa_geometric_height(fb);

         const GLfloat clip_x0 = (GLfloat)(x0 / fb_width * 2.0 - 1.0);
         const GLfloat clip_y0 = (GLfloat)(y0 / fb_height * 2.0 - 1.0);
         const GLfloat clip_x1 = (GLfloat)(x1 / fb_width * 2.0 - 1.0);
         const GLfloat clip_y1 = (GLfloat)(y1 / fb_height * 2.0 - 1.0);

         SET_ATTRIB(0, 0, clip_x0, clip_y0, z, 1.0f);   /* lower left */
         SET_ATTRIB(1, 0, clip_x1, clip_y0, z, 1.0f);   /* lower right */
         SET_ATTRIB(2, 0, clip_x1, clip_y1, z, 1.0f);   /* upper right */
         SET_ATTRIB(3, 0, clip_x0, clip_y1, z, 1.0f);   /* upper left */

         semantic_names[0] = TGSI_SEMANTIC_POSITION;
         semantic_indexes[0] = 0;
      }

      /* colors */
      if (emitColor) {
         const GLfloat *c = ctx->Current.Attrib[VERT_ATTRIB_COLOR0];
         SET_ATTRIB(0, 1, c[0], c[1], c[2], c[3]);
         SET_ATTRIB(1, 1, c[0], c[1], c[2], c[3]);
         SET_ATTRIB(2, 1, c[0], c[1], c[2], c[3]);
         SET_ATTRIB(3, 1, c[0], c[1], c[2], c[3]);
         semantic_names[1] = TGSI_SEMANTIC_COLOR;
         semantic_indexes[1] = 0;
         tex_attr = 2;
      }
      else {
         tex_attr = 1;
      }

      /* texcoords */
      for (i = 0; i < ctx->Const.MaxTextureUnits; i++) {
         if (ctx->Texture.Unit[i]._Current &&
             ctx->Texture.Unit[i]._Current->Target == GL_TEXTURE_2D) {
            struct gl_texture_object *obj = ctx->Texture.Unit[i]._Current;
            const struct gl_texture_image *img = _mesa_base_tex_image(obj);
            const GLfloat wt = (GLfloat) img->Width;
            const GLfloat ht = (GLfloat) img->Height;
            const GLfloat s0 = obj->CropRect[0] / wt;
            const GLfloat t0 = obj->CropRect[1] / ht;
            const GLfloat s1 = (obj->CropRect[0] + obj->CropRect[2]) / wt;
            const GLfloat t1 = (obj->CropRect[1] + obj->CropRect[3]) / ht;

            /*printf("crop texcoords: %g, %g .. %g, %g\n", s0, t0, s1, t1);*/
            SET_ATTRIB(0, tex_attr, s0, t0, 0.0f, 1.0f);  /* lower left */
            SET_ATTRIB(1, tex_attr, s1, t0, 0.0f, 1.0f);  /* lower right */
            SET_ATTRIB(2, tex_attr, s1, t1, 0.0f, 1.0f);  /* upper right */
            SET_ATTRIB(3, tex_attr, s0, t1, 0.0f, 1.0f);  /* upper left */

            semantic_names[tex_attr] = st->needs_texcoord_semantic ?
               TGSI_SEMANTIC_TEXCOORD : TGSI_SEMANTIC_GENERIC;
            /* XXX: should this use semantic index i instead of 0 ? */
            semantic_indexes[tex_attr] = 0;

            tex_attr++;
         }
      }

      u_upload_unmap(pipe->stream_uploader);

#undef SET_ATTRIB
   }

   cso_save_state(cso, (CSO_BIT_VIEWPORT |
                        CSO_BIT_STREAM_OUTPUTS |
                        CSO_BIT_VERTEX_SHADER |
                        CSO_BIT_TESSCTRL_SHADER |
                        CSO_BIT_TESSEVAL_SHADER |
                        CSO_BIT_GEOMETRY_SHADER |
                        CSO_BIT_VERTEX_ELEMENTS |
                        CSO_BIT_AUX_VERTEX_BUFFER_SLOT));

   {
      void *vs = lookup_shader(pipe, numAttribs,
                               semantic_names, semantic_indexes);
      cso_set_vertex_shader_handle(cso, vs);
   }
   cso_set_tessctrl_shader_handle(cso, NULL);
   cso_set_tesseval_shader_handle(cso, NULL);
   cso_set_geometry_shader_handle(cso, NULL);

   for (i = 0; i < numAttribs; i++) {
      velements[i].src_offset = i * 4 * sizeof(float);
      velements[i].instance_divisor = 0;
      velements[i].vertex_buffer_index = 0;
      velements[i].src_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
   }
   cso_set_vertex_elements(cso, numAttribs, velements);
   cso_set_stream_outputs(cso, 0, NULL, NULL);

   /* viewport state: viewport matching window dims */
   {
      const struct gl_framebuffer *fb = ctx->DrawBuffer;
      const GLboolean invert = (st_fb_orientation(fb) == Y_0_TOP);
      const GLfloat width = (GLfloat)_mesa_geometric_width(fb);
      const GLfloat height = (GLfloat)_mesa_geometric_height(fb);
      struct pipe_viewport_state vp;
      vp.scale[0] =  0.5f * width;
      vp.scale[1] = height * (invert ? -0.5f : 0.5f);
      vp.scale[2] = 1.0f;
      vp.translate[0] = 0.5f * width;
      vp.translate[1] = 0.5f * height;
      vp.translate[2] = 0.0f;
      cso_set_viewport(cso, &vp);
   }

   util_draw_vertex_buffer(pipe, cso, vbuffer,
			   cso_get_aux_vertex_buffer_slot(cso),
                           offset,  /* offset */
                           PIPE_PRIM_TRIANGLE_FAN,
                           4,  /* verts */
                           numAttribs); /* attribs/vert */

   pipe_resource_reference(&vbuffer, NULL);

   /* restore state */
   cso_restore_state(cso);
}


void
st_init_drawtex_functions(struct dd_function_table *functions)
{
   functions->DrawTex = st_DrawTex;
}


/**
 * Free any cached shaders
 */
void
st_destroy_drawtex(struct st_context *st)
{
   GLuint i;
   for (i = 0; i < NumCachedShaders; i++) {
      cso_delete_vertex_shader(st->cso_context, CachedShaders[i].handle);
   }
   NumCachedShaders = 0;
}
