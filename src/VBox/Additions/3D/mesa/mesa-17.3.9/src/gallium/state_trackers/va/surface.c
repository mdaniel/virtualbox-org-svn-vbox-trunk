/**************************************************************************
 *
 * Copyright 2010 Thomas Balling Sørensen & Orasanu Lucian.
 * Copyright 2014 Advanced Micro Devices, Inc.
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
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "pipe/p_screen.h"
#include "pipe/p_video_codec.h"

#include "state_tracker/drm_driver.h"

#include "util/u_memory.h"
#include "util/u_handle_table.h"
#include "util/u_rect.h"
#include "util/u_sampler.h"
#include "util/u_surface.h"

#include "vl/vl_compositor.h"
#include "vl/vl_video_buffer.h"
#include "vl/vl_winsys.h"

#include "va_private.h"

#include <va/va_drmcommon.h>
#include <drm-uapi/drm_fourcc.h>

static const enum pipe_format vpp_surface_formats[] = {
   PIPE_FORMAT_B8G8R8A8_UNORM, PIPE_FORMAT_R8G8B8A8_UNORM,
   PIPE_FORMAT_B8G8R8X8_UNORM, PIPE_FORMAT_R8G8B8X8_UNORM
};

VAStatus
vlVaCreateSurfaces(VADriverContextP ctx, int width, int height, int format,
                   int num_surfaces, VASurfaceID *surfaces)
{
   return vlVaCreateSurfaces2(ctx, format, width, height, surfaces, num_surfaces,
                              NULL, 0);
}

VAStatus
vlVaDestroySurfaces(VADriverContextP ctx, VASurfaceID *surface_list, int num_surfaces)
{
   vlVaDriver *drv;
   int i;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   drv = VL_VA_DRIVER(ctx);
   mtx_lock(&drv->mutex);
   for (i = 0; i < num_surfaces; ++i) {
      vlVaSurface *surf = handle_table_get(drv->htab, surface_list[i]);
      if (!surf) {
         mtx_unlock(&drv->mutex);
         return VA_STATUS_ERROR_INVALID_SURFACE;
      }
      if (surf->buffer)
         surf->buffer->destroy(surf->buffer);
      util_dynarray_fini(&surf->subpics);
      FREE(surf);
      handle_table_remove(drv->htab, surface_list[i]);
   }
   mtx_unlock(&drv->mutex);

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaSyncSurface(VADriverContextP ctx, VASurfaceID render_target)
{
   vlVaDriver *drv;
   vlVaContext *context;
   vlVaSurface *surf;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   drv = VL_VA_DRIVER(ctx);
   if (!drv)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   mtx_lock(&drv->mutex);
   surf = handle_table_get(drv->htab, render_target);

   if (!surf || !surf->buffer) {
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_SURFACE;
   }

   if (!surf->feedback) {
      // No outstanding operation: nothing to do.
      mtx_unlock(&drv->mutex);
      return VA_STATUS_SUCCESS;
   }

   context = handle_table_get(drv->htab, surf->ctx);
   if (!context) {
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_CONTEXT;
   }

   if (context->decoder->entrypoint == PIPE_VIDEO_ENTRYPOINT_ENCODE) {
      int frame_diff;
      if (context->desc.h264enc.frame_num_cnt >= surf->frame_num_cnt)
         frame_diff = context->desc.h264enc.frame_num_cnt - surf->frame_num_cnt;
      else
         frame_diff = 0xFFFFFFFF - surf->frame_num_cnt + 1 + context->desc.h264enc.frame_num_cnt;
      if ((frame_diff == 0) &&
          (surf->force_flushed == false) &&
          (context->desc.h264enc.frame_num_cnt % 2 != 0)) {
         context->decoder->flush(context->decoder);
         context->first_single_submitted = true;
      }
      context->decoder->get_feedback(context->decoder, surf->feedback, &(surf->coded_buf->coded_size));
      surf->feedback = NULL;
   }
   mtx_unlock(&drv->mutex);
   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaQuerySurfaceStatus(VADriverContextP ctx, VASurfaceID render_target, VASurfaceStatus *status)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaQuerySurfaceError(VADriverContextP ctx, VASurfaceID render_target, VAStatus error_status, void **error_info)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   return VA_STATUS_ERROR_UNIMPLEMENTED;
}

static void
upload_sampler(struct pipe_context *pipe, struct pipe_sampler_view *dst,
               const struct pipe_box *dst_box, const void *src, unsigned src_stride,
               unsigned src_x, unsigned src_y)
{
   struct pipe_transfer *transfer;
   void *map;

   map = pipe->transfer_map(pipe, dst->texture, 0, PIPE_TRANSFER_WRITE,
                            dst_box, &transfer);
   if (!map)
      return;

   util_copy_rect(map, dst->texture->format, transfer->stride, 0, 0,
                  dst_box->width, dst_box->height,
                  src, src_stride, src_x, src_y);

   pipe->transfer_unmap(pipe, transfer);
}

static VAStatus
vlVaPutSubpictures(vlVaSurface *surf, vlVaDriver *drv,
                   struct pipe_surface *surf_draw, struct u_rect *dirty_area,
                   struct u_rect *src_rect, struct u_rect *dst_rect)
{
   vlVaSubpicture *sub;
   int i;

   if (!(surf->subpics.data || surf->subpics.size))
      return VA_STATUS_SUCCESS;

   for (i = 0; i < surf->subpics.size/sizeof(vlVaSubpicture *); i++) {
      struct pipe_blend_state blend;
      void *blend_state;
      vlVaBuffer *buf;
      struct pipe_box box;
      struct u_rect *s, *d, sr, dr, c;
      int sw, sh, dw, dh;

      sub = ((vlVaSubpicture **)surf->subpics.data)[i];
      if (!sub)
         continue;

      buf = handle_table_get(drv->htab, sub->image->buf);
      if (!buf)
         return VA_STATUS_ERROR_INVALID_IMAGE;

      box.x = 0;
      box.y = 0;
      box.z = 0;
      box.width = sub->dst_rect.x1 - sub->dst_rect.x0;
      box.height = sub->dst_rect.y1 - sub->dst_rect.y0;
      box.depth = 1;

      s = &sub->src_rect;
      d = &sub->dst_rect;
      sw = s->x1 - s->x0;
      sh = s->y1 - s->y0;
      dw = d->x1 - d->x0;
      dh = d->y1 - d->y0;
      c.x0 = MAX2(d->x0, s->x0);
      c.y0 = MAX2(d->y0, s->y0);
      c.x1 = MIN2(d->x0 + dw, src_rect->x1);
      c.y1 = MIN2(d->y0 + dh, src_rect->y1);
      sr.x0 = s->x0 + (c.x0 - d->x0)*(sw/(float)dw);
      sr.y0 = s->y0 + (c.y0 - d->y0)*(sh/(float)dh);
      sr.x1 = s->x0 + (c.x1 - d->x0)*(sw/(float)dw);
      sr.y1 = s->y0 + (c.y1 - d->y0)*(sh/(float)dh);

      s = src_rect;
      d = dst_rect;
      sw = s->x1 - s->x0;
      sh = s->y1 - s->y0;
      dw = d->x1 - d->x0;
      dh = d->y1 - d->y0;
      dr.x0 = d->x0 + c.x0*(dw/(float)sw);
      dr.y0 = d->y0 + c.y0*(dh/(float)sh);
      dr.x1 = d->x0 + c.x1*(dw/(float)sw);
      dr.y1 = d->y0 + c.y1*(dh/(float)sh);

      memset(&blend, 0, sizeof(blend));
      blend.independent_blend_enable = 0;
      blend.rt[0].blend_enable = 1;
      blend.rt[0].rgb_src_factor = PIPE_BLENDFACTOR_SRC_ALPHA;
      blend.rt[0].rgb_dst_factor = PIPE_BLENDFACTOR_INV_SRC_ALPHA;
      blend.rt[0].alpha_src_factor = PIPE_BLENDFACTOR_ZERO;
      blend.rt[0].alpha_dst_factor = PIPE_BLENDFACTOR_ZERO;
      blend.rt[0].rgb_func = PIPE_BLEND_ADD;
      blend.rt[0].alpha_func = PIPE_BLEND_ADD;
      blend.rt[0].colormask = PIPE_MASK_RGBA;
      blend.logicop_enable = 0;
      blend.logicop_func = PIPE_LOGICOP_CLEAR;
      blend.dither = 0;
      blend_state = drv->pipe->create_blend_state(drv->pipe, &blend);

      vl_compositor_clear_layers(&drv->cstate);
      vl_compositor_set_layer_blend(&drv->cstate, 0, blend_state, false);
      upload_sampler(drv->pipe, sub->sampler, &box, buf->data,
                     sub->image->pitches[0], 0, 0);
      vl_compositor_set_rgba_layer(&drv->cstate, &drv->compositor, 0, sub->sampler,
                                   &sr, NULL, NULL);
      vl_compositor_set_layer_dst_area(&drv->cstate, 0, &dr);
      vl_compositor_render(&drv->cstate, &drv->compositor, surf_draw, dirty_area, false);
      drv->pipe->delete_blend_state(drv->pipe, blend_state);
   }

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaPutSurface(VADriverContextP ctx, VASurfaceID surface_id, void* draw, short srcx, short srcy,
               unsigned short srcw, unsigned short srch, short destx, short desty,
               unsigned short destw, unsigned short desth, VARectangle *cliprects,
               unsigned int number_cliprects,  unsigned int flags)
{
   vlVaDriver *drv;
   vlVaSurface *surf;
   struct pipe_screen *screen;
   struct pipe_resource *tex;
   struct pipe_surface surf_templ, *surf_draw;
   struct vl_screen *vscreen;
   struct u_rect src_rect, *dirty_area;
   struct u_rect dst_rect = {destx, destx + destw, desty, desty + desth};
   enum pipe_format format;
   VAStatus status;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   drv = VL_VA_DRIVER(ctx);
   mtx_lock(&drv->mutex);
   surf = handle_table_get(drv->htab, surface_id);
   if (!surf) {
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_SURFACE;
   }

   screen = drv->pipe->screen;
   vscreen = drv->vscreen;

   tex = vscreen->texture_from_drawable(vscreen, draw);
   if (!tex) {
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_DISPLAY;
   }

   dirty_area = vscreen->get_dirty_area(vscreen);

   memset(&surf_templ, 0, sizeof(surf_templ));
   surf_templ.format = tex->format;
   surf_draw = drv->pipe->create_surface(drv->pipe, tex, &surf_templ);
   if (!surf_draw) {
      pipe_resource_reference(&tex, NULL);
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_DISPLAY;
   }

   src_rect.x0 = srcx;
   src_rect.y0 = srcy;
   src_rect.x1 = srcw + srcx;
   src_rect.y1 = srch + srcy;

   format = surf->buffer->buffer_format;

   vl_compositor_clear_layers(&drv->cstate);

   if (format == PIPE_FORMAT_B8G8R8A8_UNORM || format == PIPE_FORMAT_B8G8R8X8_UNORM ||
       format == PIPE_FORMAT_R8G8B8A8_UNORM || format == PIPE_FORMAT_R8G8B8X8_UNORM) {
      struct pipe_sampler_view **views;

      views = surf->buffer->get_sampler_view_planes(surf->buffer);
      vl_compositor_set_rgba_layer(&drv->cstate, &drv->compositor, 0, views[0], &src_rect, NULL, NULL);
   } else
      vl_compositor_set_buffer_layer(&drv->cstate, &drv->compositor, 0, surf->buffer, &src_rect, NULL, VL_COMPOSITOR_WEAVE);

   vl_compositor_set_layer_dst_area(&drv->cstate, 0, &dst_rect);
   vl_compositor_render(&drv->cstate, &drv->compositor, surf_draw, dirty_area, true);

   status = vlVaPutSubpictures(surf, drv, surf_draw, dirty_area, &src_rect, &dst_rect);
   if (status) {
      mtx_unlock(&drv->mutex);
      return status;
   }

   /* flush before calling flush_frontbuffer so that rendering is flushed
    * to back buffer so the texture can be copied in flush_frontbuffer
    */
   drv->pipe->flush(drv->pipe, NULL, 0);

   screen->flush_frontbuffer(screen, tex, 0, 0,
                             vscreen->get_private(vscreen), NULL);


   pipe_resource_reference(&tex, NULL);
   pipe_surface_reference(&surf_draw, NULL);
   mtx_unlock(&drv->mutex);

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaLockSurface(VADriverContextP ctx, VASurfaceID surface, unsigned int *fourcc,
                unsigned int *luma_stride, unsigned int *chroma_u_stride, unsigned int *chroma_v_stride,
                unsigned int *luma_offset, unsigned int *chroma_u_offset, unsigned int *chroma_v_offset,
                unsigned int *buffer_name, void **buffer)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus
vlVaUnlockSurface(VADriverContextP ctx, VASurfaceID surface)
{
   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   return VA_STATUS_ERROR_UNIMPLEMENTED;
}

VAStatus
vlVaQuerySurfaceAttributes(VADriverContextP ctx, VAConfigID config_id,
                           VASurfaceAttrib *attrib_list, unsigned int *num_attribs)
{
   vlVaDriver *drv;
   vlVaConfig *config;
   VASurfaceAttrib *attribs;
   struct pipe_screen *pscreen;
   int i, j;

   STATIC_ASSERT(ARRAY_SIZE(vpp_surface_formats) <= VL_VA_MAX_IMAGE_FORMATS);

   if (config_id == VA_INVALID_ID)
      return VA_STATUS_ERROR_INVALID_CONFIG;

   if (!attrib_list && !num_attribs)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   if (!attrib_list) {
      *num_attribs = VL_VA_MAX_IMAGE_FORMATS + VASurfaceAttribCount;
      return VA_STATUS_SUCCESS;
   }

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   drv = VL_VA_DRIVER(ctx);

   if (!drv)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   mtx_lock(&drv->mutex);
   config = handle_table_get(drv->htab, config_id);
   mtx_unlock(&drv->mutex);

   if (!config)
      return VA_STATUS_ERROR_INVALID_CONFIG;

   pscreen = VL_VA_PSCREEN(ctx);

   if (!pscreen)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   attribs = CALLOC(VL_VA_MAX_IMAGE_FORMATS + VASurfaceAttribCount,
                    sizeof(VASurfaceAttrib));

   if (!attribs)
      return VA_STATUS_ERROR_ALLOCATION_FAILED;

   i = 0;

   /* vlVaCreateConfig returns PIPE_VIDEO_PROFILE_UNKNOWN
    * only for VAEntrypointVideoProc. */
   if (config->profile == PIPE_VIDEO_PROFILE_UNKNOWN) {
      if (config->rt_format & VA_RT_FORMAT_RGB32) {
         for (j = 0; j < ARRAY_SIZE(vpp_surface_formats); ++j) {
            attribs[i].type = VASurfaceAttribPixelFormat;
            attribs[i].value.type = VAGenericValueTypeInteger;
            attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
            attribs[i].value.value.i = PipeFormatToVaFourcc(vpp_surface_formats[j]);
            i++;
         }
      }
   }
   if (config->rt_format & VA_RT_FORMAT_YUV420) {
      attribs[i].type = VASurfaceAttribPixelFormat;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
      attribs[i].value.value.i = VA_FOURCC_NV12;
      i++;
   }
   if (config->rt_format & VA_RT_FORMAT_YUV420_10BPP) {
      attribs[i].type = VASurfaceAttribPixelFormat;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
      attribs[i].value.value.i = VA_FOURCC_P010;
      i++;
      attribs[i].type = VASurfaceAttribPixelFormat;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
      attribs[i].value.value.i = VA_FOURCC_P016;
      i++;
   }

   attribs[i].type = VASurfaceAttribMemoryType;
   attribs[i].value.type = VAGenericValueTypeInteger;
   attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
   attribs[i].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA |
         VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
   i++;

   attribs[i].type = VASurfaceAttribExternalBufferDescriptor;
   attribs[i].value.type = VAGenericValueTypePointer;
   attribs[i].flags = VA_SURFACE_ATTRIB_SETTABLE;
   attribs[i].value.value.p = NULL; /* ignore */
   i++;

   if (config->entrypoint != PIPE_VIDEO_ENTRYPOINT_UNKNOWN) {
      attribs[i].type = VASurfaceAttribMaxWidth;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
      attribs[i].value.value.i =
         pscreen->get_video_param(pscreen,
                                  config->profile, config->entrypoint,
                                  PIPE_VIDEO_CAP_MAX_WIDTH);
      i++;

      attribs[i].type = VASurfaceAttribMaxHeight;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
      attribs[i].value.value.i =
         pscreen->get_video_param(pscreen,
                                  config->profile, config->entrypoint,
                                  PIPE_VIDEO_CAP_MAX_HEIGHT);
      i++;
   } else {
      attribs[i].type = VASurfaceAttribMaxWidth;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
      attribs[i].value.value.i = vl_video_buffer_max_size(pscreen);
      i++;

      attribs[i].type = VASurfaceAttribMaxHeight;
      attribs[i].value.type = VAGenericValueTypeInteger;
      attribs[i].flags = VA_SURFACE_ATTRIB_GETTABLE;
      attribs[i].value.value.i = vl_video_buffer_max_size(pscreen);
      i++;
   }

   if (i > *num_attribs) {
      *num_attribs = i;
      FREE(attribs);
      return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
   }

   *num_attribs = i;
   memcpy(attrib_list, attribs, i * sizeof(VASurfaceAttrib));
   FREE(attribs);

   return VA_STATUS_SUCCESS;
}

static VAStatus
suface_from_external_memory(VADriverContextP ctx, vlVaSurface *surface,
                            VASurfaceAttribExternalBuffers *memory_attibute,
                            unsigned index, struct pipe_video_buffer *templat)
{
   vlVaDriver *drv;
   struct pipe_screen *pscreen;
   struct pipe_resource *resource;
   struct pipe_resource res_templ;
   struct winsys_handle whandle;
   struct pipe_resource *resources[VL_NUM_COMPONENTS];

   pscreen = VL_VA_PSCREEN(ctx);
   drv = VL_VA_DRIVER(ctx);

   if (!memory_attibute || !memory_attibute->buffers ||
       index > memory_attibute->num_buffers)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   if (surface->templat.width != memory_attibute->width ||
       surface->templat.height != memory_attibute->height ||
       memory_attibute->num_planes < 1)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   switch (memory_attibute->pixel_format) {
   case VA_FOURCC_RGBA:
   case VA_FOURCC_RGBX:
   case VA_FOURCC_BGRA:
   case VA_FOURCC_BGRX:
      if (memory_attibute->num_planes != 1)
         return VA_STATUS_ERROR_INVALID_PARAMETER;
      break;
   default:
      return VA_STATUS_ERROR_INVALID_PARAMETER;
   }

   memset(&res_templ, 0, sizeof(res_templ));
   res_templ.target = PIPE_TEXTURE_2D;
   res_templ.last_level = 0;
   res_templ.depth0 = 1;
   res_templ.array_size = 1;
   res_templ.width0 = memory_attibute->width;
   res_templ.height0 = memory_attibute->height;
   res_templ.format = surface->templat.buffer_format;
   res_templ.bind = PIPE_BIND_SAMPLER_VIEW;
   res_templ.usage = PIPE_USAGE_DEFAULT;

   memset(&whandle, 0, sizeof(struct winsys_handle));
   whandle.type = DRM_API_HANDLE_TYPE_FD;
   whandle.handle = memory_attibute->buffers[index];
   whandle.stride = memory_attibute->pitches[index];

   resource = pscreen->resource_from_handle(pscreen, &res_templ, &whandle,
                                            PIPE_HANDLE_USAGE_READ_WRITE);

   if (!resource)
      return VA_STATUS_ERROR_ALLOCATION_FAILED;

   memset(resources, 0, sizeof resources);
   resources[0] = resource;

   surface->buffer = vl_video_buffer_create_ex2(drv->pipe, templat, resources);
   if (!surface->buffer)
      return VA_STATUS_ERROR_ALLOCATION_FAILED;

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaHandleSurfaceAllocate(vlVaDriver *drv, vlVaSurface *surface,
                          struct pipe_video_buffer *templat)
{
   struct pipe_surface **surfaces;
   unsigned i;

   surface->buffer = drv->pipe->create_video_buffer(drv->pipe, templat);
   if (!surface->buffer)
      return VA_STATUS_ERROR_ALLOCATION_FAILED;

   surfaces = surface->buffer->get_surfaces(surface->buffer);
   for (i = 0; i < VL_MAX_SURFACES; ++i) {
      union pipe_color_union c = {};

      if (!surfaces[i])
         continue;

      if (i > !!surface->buffer->interlaced)
         c.f[0] = c.f[1] = c.f[2] = c.f[3] = 0.5f;

      drv->pipe->clear_render_target(drv->pipe, surfaces[i], &c, 0, 0,
				     surfaces[i]->width, surfaces[i]->height,
				     false);
   }
   drv->pipe->flush(drv->pipe, NULL, 0);

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaCreateSurfaces2(VADriverContextP ctx, unsigned int format,
                    unsigned int width, unsigned int height,
                    VASurfaceID *surfaces, unsigned int num_surfaces,
                    VASurfaceAttrib *attrib_list, unsigned int num_attribs)
{
   vlVaDriver *drv;
   VASurfaceAttribExternalBuffers *memory_attibute;
   struct pipe_video_buffer templat;
   struct pipe_screen *pscreen;
   int i;
   int memory_type;
   int expected_fourcc;
   VAStatus vaStatus;
   vlVaSurface *surf;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (!(width && height))
      return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;

   drv = VL_VA_DRIVER(ctx);

   if (!drv)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   pscreen = VL_VA_PSCREEN(ctx);

   if (!pscreen)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   /* Default. */
   memory_attibute = NULL;
   memory_type = VA_SURFACE_ATTRIB_MEM_TYPE_VA;
   expected_fourcc = 0;

   for (i = 0; i < num_attribs && attrib_list; i++) {
      if ((attrib_list[i].type == VASurfaceAttribPixelFormat) &&
          (attrib_list[i].flags & VA_SURFACE_ATTRIB_SETTABLE)) {
         if (attrib_list[i].value.type != VAGenericValueTypeInteger)
            return VA_STATUS_ERROR_INVALID_PARAMETER;
         expected_fourcc = attrib_list[i].value.value.i;
      }

      if ((attrib_list[i].type == VASurfaceAttribMemoryType) &&
          (attrib_list[i].flags & VA_SURFACE_ATTRIB_SETTABLE)) {

         if (attrib_list[i].value.type != VAGenericValueTypeInteger)
            return VA_STATUS_ERROR_INVALID_PARAMETER;

         switch (attrib_list[i].value.value.i) {
         case VA_SURFACE_ATTRIB_MEM_TYPE_VA:
         case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME:
            memory_type = attrib_list[i].value.value.i;
            break;
         default:
            return VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;
         }
      }

      if ((attrib_list[i].type == VASurfaceAttribExternalBufferDescriptor) &&
          (attrib_list[i].flags == VA_SURFACE_ATTRIB_SETTABLE)) {
         if (attrib_list[i].value.type != VAGenericValueTypePointer)
            return VA_STATUS_ERROR_INVALID_PARAMETER;
         memory_attibute = (VASurfaceAttribExternalBuffers *)attrib_list[i].value.value.p;
      }
   }

   if (VA_RT_FORMAT_YUV420 != format &&
       VA_RT_FORMAT_YUV422 != format &&
       VA_RT_FORMAT_YUV444 != format &&
       VA_RT_FORMAT_YUV420_10BPP != format &&
       VA_RT_FORMAT_RGB32  != format) {
      return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
   }

   switch (memory_type) {
   case VA_SURFACE_ATTRIB_MEM_TYPE_VA:
      break;
   case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME:
      if (!memory_attibute)
         return VA_STATUS_ERROR_INVALID_PARAMETER;

      expected_fourcc = memory_attibute->pixel_format;
      break;
   default:
      assert(0);
   }

   memset(&templat, 0, sizeof(templat));

   templat.buffer_format = pscreen->get_video_param(
      pscreen,
      PIPE_VIDEO_PROFILE_UNKNOWN,
      PIPE_VIDEO_ENTRYPOINT_BITSTREAM,
      PIPE_VIDEO_CAP_PREFERED_FORMAT
   );
   templat.interlaced = pscreen->get_video_param(
      pscreen,
      PIPE_VIDEO_PROFILE_UNKNOWN,
      PIPE_VIDEO_ENTRYPOINT_BITSTREAM,
      PIPE_VIDEO_CAP_PREFERS_INTERLACED
   );

   if (expected_fourcc) {
      enum pipe_format expected_format = VaFourccToPipeFormat(expected_fourcc);

      if (expected_format != templat.buffer_format || memory_attibute)
        templat.interlaced = 0;

      templat.buffer_format = expected_format;
   }

   templat.chroma_format = ChromaToPipe(format);

   templat.width = width;
   templat.height = height;

   memset(surfaces, VA_INVALID_ID, num_surfaces * sizeof(VASurfaceID));

   mtx_lock(&drv->mutex);
   for (i = 0; i < num_surfaces; i++) {
      surf = CALLOC(1, sizeof(vlVaSurface));
      if (!surf) {
         vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
         goto no_res;
      }

      surf->templat = templat;

      switch (memory_type) {
      case VA_SURFACE_ATTRIB_MEM_TYPE_VA:
         /* The application will clear the TILING flag when the surface is
          * intended to be exported as dmabuf. Adding shared flag because not
          * null memory_attibute means VASurfaceAttribExternalBuffers is used.
          */
         if (memory_attibute &&
             !(memory_attibute->flags & VA_SURFACE_EXTBUF_DESC_ENABLE_TILING))
            templat.bind = PIPE_BIND_LINEAR | PIPE_BIND_SHARED;

	 vaStatus = vlVaHandleSurfaceAllocate(drv, surf, &templat);
         if (vaStatus != VA_STATUS_SUCCESS)
            goto free_surf;
         break;

      case VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME:
         vaStatus = suface_from_external_memory(ctx, surf, memory_attibute, i, &templat);
         if (vaStatus != VA_STATUS_SUCCESS)
            goto free_surf;
         break;

      default:
         assert(0);
      }

      util_dynarray_init(&surf->subpics, NULL);
      surfaces[i] = handle_table_add(drv->htab, surf);
      if (!surfaces[i]) {
         vaStatus = VA_STATUS_ERROR_ALLOCATION_FAILED;
         goto destroy_surf;
      }
   }
   mtx_unlock(&drv->mutex);

   return VA_STATUS_SUCCESS;

destroy_surf:
   surf->buffer->destroy(surf->buffer);

free_surf:
   FREE(surf);

no_res:
   mtx_unlock(&drv->mutex);
   if (i)
      vlVaDestroySurfaces(ctx, surfaces, i);

   return vaStatus;
}

VAStatus
vlVaQueryVideoProcFilters(VADriverContextP ctx, VAContextID context,
                          VAProcFilterType *filters, unsigned int *num_filters)
{
   unsigned int num = 0;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (!num_filters || !filters)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   filters[num++] = VAProcFilterDeinterlacing;

   *num_filters = num;

   return VA_STATUS_SUCCESS;
}

VAStatus
vlVaQueryVideoProcFilterCaps(VADriverContextP ctx, VAContextID context,
                             VAProcFilterType type, void *filter_caps,
                             unsigned int *num_filter_caps)
{
   unsigned int i;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (!filter_caps || !num_filter_caps)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   i = 0;

   switch (type) {
   case VAProcFilterNone:
      break;
   case VAProcFilterDeinterlacing: {
      VAProcFilterCapDeinterlacing *deint = filter_caps;

      if (*num_filter_caps < 3) {
         *num_filter_caps = 3;
         return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
      }

      deint[i++].type = VAProcDeinterlacingBob;
      deint[i++].type = VAProcDeinterlacingWeave;
      deint[i++].type = VAProcDeinterlacingMotionAdaptive;
      break;
   }

   case VAProcFilterNoiseReduction:
   case VAProcFilterSharpening:
   case VAProcFilterColorBalance:
   case VAProcFilterSkinToneEnhancement:
      return VA_STATUS_ERROR_UNIMPLEMENTED;
   default:
      assert(0);
   }

   *num_filter_caps = i;

   return VA_STATUS_SUCCESS;
}

static VAProcColorStandardType vpp_input_color_standards[] = {
   VAProcColorStandardBT601
};

static VAProcColorStandardType vpp_output_color_standards[] = {
   VAProcColorStandardBT601
};

VAStatus
vlVaQueryVideoProcPipelineCaps(VADriverContextP ctx, VAContextID context,
                               VABufferID *filters, unsigned int num_filters,
                               VAProcPipelineCaps *pipeline_cap)
{
   unsigned int i = 0;

   if (!ctx)
      return VA_STATUS_ERROR_INVALID_CONTEXT;

   if (!pipeline_cap)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   if (num_filters && !filters)
      return VA_STATUS_ERROR_INVALID_PARAMETER;

   pipeline_cap->pipeline_flags = 0;
   pipeline_cap->filter_flags = 0;
   pipeline_cap->num_forward_references = 0;
   pipeline_cap->num_backward_references = 0;
   pipeline_cap->num_input_color_standards = ARRAY_SIZE(vpp_input_color_standards);
   pipeline_cap->input_color_standards = vpp_input_color_standards;
   pipeline_cap->num_output_color_standards = ARRAY_SIZE(vpp_output_color_standards);
   pipeline_cap->output_color_standards = vpp_output_color_standards;

   for (i = 0; i < num_filters; i++) {
      vlVaBuffer *buf = handle_table_get(VL_VA_DRIVER(ctx)->htab, filters[i]);
      VAProcFilterParameterBufferBase *filter;

      if (!buf || buf->type != VAProcFilterParameterBufferType)
         return VA_STATUS_ERROR_INVALID_BUFFER;

      filter = buf->data;
      switch (filter->type) {
      case VAProcFilterDeinterlacing: {
         VAProcFilterParameterBufferDeinterlacing *deint = buf->data;
         if (deint->algorithm == VAProcDeinterlacingMotionAdaptive) {
            pipeline_cap->num_forward_references = 2;
            pipeline_cap->num_backward_references = 1;
         }
         break;
      }
      default:
         return VA_STATUS_ERROR_UNIMPLEMENTED;
      }
   }

   return VA_STATUS_SUCCESS;
}

#if 0
VAStatus
vlVaExportSurfaceHandle(VADriverContextP ctx,
                        VASurfaceID surface_id,
                        uint32_t mem_type,
                        uint32_t flags,
                        void *descriptor)
{
   vlVaDriver *drv;
   vlVaSurface *surf;
   struct pipe_surface **surfaces;
   struct pipe_screen *screen;
   VAStatus ret;
   unsigned int usage;
   int i, p;

   VADRMPRIMESurfaceDescriptor *desc = descriptor;

   if (mem_type != VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2)
      return VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;
   if (flags & VA_EXPORT_SURFACE_COMPOSED_LAYERS)
      return VA_STATUS_ERROR_INVALID_SURFACE;

   drv    = VL_VA_DRIVER(ctx);
   screen = VL_VA_PSCREEN(ctx);
   mtx_lock(&drv->mutex);

   surf = handle_table_get(drv->htab, surface_id);
   if (!surf || !surf->buffer) {
      mtx_unlock(&drv->mutex);
      return VA_STATUS_ERROR_INVALID_SURFACE;
   }

   if (surf->buffer->interlaced) {
      struct pipe_video_buffer *interlaced = surf->buffer;
      struct u_rect src_rect, dst_rect;

      surf->templat.interlaced = false;

      ret = vlVaHandleSurfaceAllocate(drv, surf, &surf->templat);
      if (ret != VA_STATUS_SUCCESS) {
         mtx_unlock(&drv->mutex);
         return VA_STATUS_ERROR_ALLOCATION_FAILED;
      }

      src_rect.x0 = dst_rect.x0 = 0;
      src_rect.y0 = dst_rect.y0 = 0;
      src_rect.x1 = dst_rect.x1 = surf->templat.width;
      src_rect.y1 = dst_rect.y1 = surf->templat.height;

      vl_compositor_yuv_deint_full(&drv->cstate, &drv->compositor,
                                   interlaced, surf->buffer,
                                   &src_rect, &dst_rect,
                                   VL_COMPOSITOR_WEAVE);

      interlaced->destroy(interlaced);
   }

   surfaces = surf->buffer->get_surfaces(surf->buffer);

   usage = 0;
   if (flags & VA_EXPORT_SURFACE_READ_ONLY)
      usage |= PIPE_HANDLE_USAGE_READ;
   if (flags & VA_EXPORT_SURFACE_WRITE_ONLY)
      usage |= PIPE_HANDLE_USAGE_WRITE;

   desc->fourcc = PipeFormatToVaFourcc(surf->buffer->buffer_format);
   desc->width  = surf->buffer->width;
   desc->height = surf->buffer->height;

   for (p = 0; p < VL_MAX_SURFACES; p++) {
      struct winsys_handle whandle;
      struct pipe_resource *resource;
      uint32_t drm_format;

      if (!surfaces[p])
         break;

      resource = surfaces[p]->texture;

      switch (resource->format) {
      case PIPE_FORMAT_R8_UNORM:
         drm_format = DRM_FORMAT_R8;
         break;
      case PIPE_FORMAT_R8G8_UNORM:
         drm_format = DRM_FORMAT_GR88;
         break;
      case PIPE_FORMAT_R16_UNORM:
         drm_format = DRM_FORMAT_R16;
         break;
      case PIPE_FORMAT_R16G16_UNORM:
         drm_format = DRM_FORMAT_GR1616;
         break;
      case PIPE_FORMAT_B8G8R8A8_UNORM:
         drm_format = DRM_FORMAT_ARGB8888;
         break;
      case PIPE_FORMAT_R8G8B8A8_UNORM:
         drm_format = DRM_FORMAT_ABGR8888;
         break;
      case PIPE_FORMAT_B8G8R8X8_UNORM:
         drm_format = DRM_FORMAT_XRGB8888;
         break;
      case PIPE_FORMAT_R8G8B8X8_UNORM:
         drm_format = DRM_FORMAT_XBGR8888;
         break;
      default:
         ret = VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;
         goto fail;
      }

      memset(&whandle, 0, sizeof(whandle));
      whandle.type = DRM_API_HANDLE_TYPE_FD;

      if (!screen->resource_get_handle(screen, drv->pipe, resource,
                                       &whandle, usage)) {
         ret = VA_STATUS_ERROR_INVALID_SURFACE;
         goto fail;
      }

      desc->objects[p].fd   = (int)whandle.handle;
      desc->objects[p].size = 0;
      desc->objects[p].drm_format_modifier = whandle.modifier;

      desc->layers[p].drm_format      = drm_format;
      desc->layers[p].num_planes      = 1;
      desc->layers[p].object_index[0] = p;
      desc->layers[p].offset[0]       = whandle.offset;
      desc->layers[p].pitch[0]        = whandle.stride;
   }

   desc->num_objects = p;
   desc->num_layers  = p;

   mtx_unlock(&drv->mutex);

   return VA_STATUS_SUCCESS;

fail:
   for (i = 0; i < p; i++)
      close(desc->objects[i].fd);

   mtx_unlock(&drv->mutex);

   return ret;
}
#endif
