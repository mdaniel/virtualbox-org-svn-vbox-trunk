/**************************************************************************
 *
 * Copyright 2016 Ilia Mirkin. All Rights Reserved.
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

#include "main/imports.h"
#include "main/shaderimage.h"
#include "program/prog_parameter.h"
#include "program/prog_print.h"
#include "compiler/glsl/ir_uniform.h"

#include "pipe/p_context.h"
#include "pipe/p_defines.h"
#include "util/u_inlines.h"
#include "util/u_surface.h"
#include "cso_cache/cso_context.h"

#include "st_cb_bufferobjects.h"
#include "st_cb_texture.h"
#include "st_debug.h"
#include "st_texture.h"
#include "st_context.h"
#include "st_atom.h"
#include "st_program.h"
#include "st_format.h"

/**
 * Convert a gl_image_unit object to a pipe_image_view object.
 */
void
st_convert_image(const struct st_context *st, const struct gl_image_unit *u,
                 struct pipe_image_view *img)
{
   struct st_texture_object *stObj = st_texture_object(u->TexObj);

   img->format = st_mesa_format_to_pipe_format(st, u->_ActualFormat);

   switch (u->Access) {
   case GL_READ_ONLY:
      img->access = PIPE_IMAGE_ACCESS_READ;
      break;
   case GL_WRITE_ONLY:
      img->access = PIPE_IMAGE_ACCESS_WRITE;
      break;
   case GL_READ_WRITE:
      img->access = PIPE_IMAGE_ACCESS_READ_WRITE;
      break;
   default:
      unreachable("bad gl_image_unit::Access");
   }

   if (stObj->base.Target == GL_TEXTURE_BUFFER) {
      struct st_buffer_object *stbuf =
         st_buffer_object(stObj->base.BufferObject);
      unsigned base, size;

      if (!stbuf || !stbuf->buffer) {
         memset(img, 0, sizeof(*img));
         return;
      }
      struct pipe_resource *buf = stbuf->buffer;

      base = stObj->base.BufferOffset;
      assert(base < buf->width0);
      size = MIN2(buf->width0 - base, (unsigned)stObj->base.BufferSize);

      img->resource = stbuf->buffer;
      img->u.buf.offset = base;
      img->u.buf.size = size;
   } else {
      if (!st_finalize_texture(st->ctx, st->pipe, u->TexObj, 0) ||
          !stObj->pt) {
         memset(img, 0, sizeof(*img));
         return;
      }

      img->resource = stObj->pt;
      img->u.tex.level = u->Level + stObj->base.MinLevel;
      if (stObj->pt->target == PIPE_TEXTURE_3D) {
         if (u->Layered) {
            img->u.tex.first_layer = 0;
            img->u.tex.last_layer = u_minify(stObj->pt->depth0, img->u.tex.level) - 1;
         } else {
            img->u.tex.first_layer = u->_Layer;
            img->u.tex.last_layer = u->_Layer;
         }
      } else {
         img->u.tex.first_layer = u->_Layer + stObj->base.MinLayer;
         img->u.tex.last_layer = u->_Layer + stObj->base.MinLayer;
         if (u->Layered && img->resource->array_size > 1) {
            if (stObj->base.Immutable)
               img->u.tex.last_layer += stObj->base.NumLayers - 1;
            else
               img->u.tex.last_layer += img->resource->array_size - 1;
         }
      }
   }
}

/**
 * Get a pipe_image_view object from an image unit.
 */
void
st_convert_image_from_unit(const struct st_context *st,
                           struct pipe_image_view *img,
                           GLuint imgUnit)
{
   struct gl_image_unit *u = &st->ctx->ImageUnits[imgUnit];

   if (!_mesa_is_image_unit_valid(st->ctx, u)) {
      memset(img, 0, sizeof(*img));
      return;
   }

   st_convert_image(st, u, img);
}

static void
st_bind_images(struct st_context *st, struct gl_program *prog,
               enum pipe_shader_type shader_type)
{
   unsigned i;
   struct pipe_image_view images[MAX_IMAGE_UNIFORMS];
   struct gl_program_constants *c;

   if (!prog || !st->pipe->set_shader_images)
      return;

   c = &st->ctx->Const.Program[prog->info.stage];

   for (i = 0; i < prog->info.num_images; i++) {
      struct pipe_image_view *img = &images[i];

      st_convert_image_from_unit(st, img, prog->sh.ImageUnits[i]);
   }
   cso_set_shader_images(st->cso_context, shader_type, 0,
                         prog->info.num_images, images);
   /* clear out any stale shader images */
   if (prog->info.num_images < c->MaxImageUniforms)
      cso_set_shader_images(
            st->cso_context, shader_type, prog->info.num_images,
            c->MaxImageUniforms - prog->info.num_images, NULL);
}

void st_bind_vs_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_VERTEX];

   st_bind_images(st, prog, PIPE_SHADER_VERTEX);
}

void st_bind_fs_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_FRAGMENT];

   st_bind_images(st, prog, PIPE_SHADER_FRAGMENT);
}

void st_bind_gs_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_GEOMETRY];

   st_bind_images(st, prog, PIPE_SHADER_GEOMETRY);
}

void st_bind_tcs_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_TESS_CTRL];

   st_bind_images(st, prog, PIPE_SHADER_TESS_CTRL);
}

void st_bind_tes_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_TESS_EVAL];

   st_bind_images(st, prog, PIPE_SHADER_TESS_EVAL);
}

void st_bind_cs_images(struct st_context *st)
{
   struct gl_program *prog =
      st->ctx->_Shader->CurrentProgram[MESA_SHADER_COMPUTE];

   st_bind_images(st, prog, PIPE_SHADER_COMPUTE);
}
