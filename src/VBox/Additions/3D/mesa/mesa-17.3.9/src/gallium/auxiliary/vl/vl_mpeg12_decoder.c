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

#include <math.h>
#include <assert.h>

#include "util/u_memory.h"
#include "util/u_sampler.h"
#include "util/u_surface.h"
#include "util/u_video.h"

#include "vl_mpeg12_decoder.h"
#include "vl_defines.h"

#define SCALE_FACTOR_SNORM (32768.0f / 256.0f)
#define SCALE_FACTOR_SSCALED (1.0f / 256.0f)

struct format_config {
   enum pipe_format zscan_source_format;
   enum pipe_format idct_source_format;
   enum pipe_format mc_source_format;

   float idct_scale;
   float mc_scale;
};

static const struct format_config bitstream_format_config[] = {
//   { PIPE_FORMAT_R16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, PIPE_FORMAT_R16G16B16A16_FLOAT, 1.0f, SCALE_FACTOR_SSCALED },
//   { PIPE_FORMAT_R16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, 1.0f, SCALE_FACTOR_SSCALED },
   { PIPE_FORMAT_R16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, PIPE_FORMAT_R16G16B16A16_FLOAT, 1.0f, SCALE_FACTOR_SNORM },
   { PIPE_FORMAT_R16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, 1.0f, SCALE_FACTOR_SNORM }
};

static const unsigned num_bitstream_format_configs =
   sizeof(bitstream_format_config) / sizeof(struct format_config);

static const struct format_config idct_format_config[] = {
//   { PIPE_FORMAT_R16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, PIPE_FORMAT_R16G16B16A16_FLOAT, 1.0f, SCALE_FACTOR_SSCALED },
//   { PIPE_FORMAT_R16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, PIPE_FORMAT_R16G16B16A16_SSCALED, 1.0f, SCALE_FACTOR_SSCALED },
   { PIPE_FORMAT_R16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, PIPE_FORMAT_R16G16B16A16_FLOAT, 1.0f, SCALE_FACTOR_SNORM },
   { PIPE_FORMAT_R16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, PIPE_FORMAT_R16G16B16A16_SNORM, 1.0f, SCALE_FACTOR_SNORM }
};

static const unsigned num_idct_format_configs =
   sizeof(idct_format_config) / sizeof(struct format_config);

static const struct format_config mc_format_config[] = {
   //{ PIPE_FORMAT_R16_SSCALED, PIPE_FORMAT_NONE, PIPE_FORMAT_R16_SSCALED, 0.0f, SCALE_FACTOR_SSCALED },
   { PIPE_FORMAT_R16_SNORM, PIPE_FORMAT_NONE, PIPE_FORMAT_R16_SNORM, 0.0f, SCALE_FACTOR_SNORM }
};

static const unsigned num_mc_format_configs =
   sizeof(mc_format_config) / sizeof(struct format_config);

static const unsigned const_empty_block_mask_420[3][2][2] = {
   { { 0x20, 0x10 },  { 0x08, 0x04 } },
   { { 0x02, 0x02 },  { 0x02, 0x02 } },
   { { 0x01, 0x01 },  { 0x01, 0x01 } }
};

struct video_buffer_private
{
   struct list_head list;
   struct pipe_video_buffer *video_buffer;

   struct pipe_sampler_view *sampler_view_planes[VL_NUM_COMPONENTS];
   struct pipe_surface      *surfaces[VL_MAX_SURFACES];

   struct vl_mpeg12_buffer *buffer;
};

static void
vl_mpeg12_destroy_buffer(struct vl_mpeg12_buffer *buf);

static void
destroy_video_buffer_private(void *private)
{
   struct video_buffer_private *priv = private;
   unsigned i;

   list_del(&priv->list);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      pipe_sampler_view_reference(&priv->sampler_view_planes[i], NULL);

   for (i = 0; i < VL_MAX_SURFACES; ++i)
      pipe_surface_reference(&priv->surfaces[i], NULL);

   if (priv->buffer)
      vl_mpeg12_destroy_buffer(priv->buffer);

   FREE(priv);
}

static struct video_buffer_private *
get_video_buffer_private(struct vl_mpeg12_decoder *dec, struct pipe_video_buffer *buf)
{
   struct pipe_context *pipe = dec->context;
   struct video_buffer_private *priv;
   struct pipe_sampler_view **sv;
   struct pipe_surface **surf;
   unsigned i;

   priv = vl_video_buffer_get_associated_data(buf, &dec->base);
   if (priv)
      return priv;

   priv = CALLOC_STRUCT(video_buffer_private);

   list_add(&priv->list, &dec->buffer_privates);
   priv->video_buffer = buf;

   sv = buf->get_sampler_view_planes(buf);
   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      if (sv[i])
         priv->sampler_view_planes[i] = pipe->create_sampler_view(pipe, sv[i]->texture, sv[i]);

   surf = buf->get_surfaces(buf);
   for (i = 0; i < VL_MAX_SURFACES; ++i)
      if (surf[i])
         priv->surfaces[i] = pipe->create_surface(pipe, surf[i]->texture, surf[i]);

   vl_video_buffer_set_associated_data(buf, &dec->base, priv, destroy_video_buffer_private);

   return priv;
}

static void
free_video_buffer_privates(struct vl_mpeg12_decoder *dec)
{
   struct video_buffer_private *priv, *next;

   LIST_FOR_EACH_ENTRY_SAFE(priv, next, &dec->buffer_privates, list) {
      struct pipe_video_buffer *buf = priv->video_buffer;

      vl_video_buffer_set_associated_data(buf, &dec->base, NULL, NULL);
   }
}

static bool
init_zscan_buffer(struct vl_mpeg12_decoder *dec, struct vl_mpeg12_buffer *buffer)
{
   struct pipe_resource *res, res_tmpl;
   struct pipe_sampler_view sv_tmpl;
   struct pipe_surface **destination;

   unsigned i;

   assert(dec && buffer);

   memset(&res_tmpl, 0, sizeof(res_tmpl));
   res_tmpl.target = PIPE_TEXTURE_2D;
   res_tmpl.format = dec->zscan_source_format;
   res_tmpl.width0 = dec->blocks_per_line * VL_BLOCK_WIDTH * VL_BLOCK_HEIGHT;
   res_tmpl.height0 = align(dec->num_blocks, dec->blocks_per_line) / dec->blocks_per_line;
   res_tmpl.depth0 = 1;
   res_tmpl.array_size = 1;
   res_tmpl.usage = PIPE_USAGE_STREAM;
   res_tmpl.bind = PIPE_BIND_SAMPLER_VIEW;

   res = dec->context->screen->resource_create(dec->context->screen, &res_tmpl);
   if (!res)
      goto error_source;


   memset(&sv_tmpl, 0, sizeof(sv_tmpl));
   u_sampler_view_default_template(&sv_tmpl, res, res->format);
   sv_tmpl.swizzle_r = sv_tmpl.swizzle_g = sv_tmpl.swizzle_b = sv_tmpl.swizzle_a = PIPE_SWIZZLE_X;
   buffer->zscan_source = dec->context->create_sampler_view(dec->context, res, &sv_tmpl);
   pipe_resource_reference(&res, NULL);
   if (!buffer->zscan_source)
      goto error_sampler;

   if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT)
      destination = dec->idct_source->get_surfaces(dec->idct_source);
   else
      destination = dec->mc_source->get_surfaces(dec->mc_source);

   if (!destination)
      goto error_surface;

   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      if (!vl_zscan_init_buffer(i == 0 ? &dec->zscan_y : &dec->zscan_c,
                                &buffer->zscan[i], buffer->zscan_source, destination[i]))
         goto error_plane;

   return true;

error_plane:
   for (; i > 0; --i)
      vl_zscan_cleanup_buffer(&buffer->zscan[i - 1]);

error_surface:
error_sampler:
   pipe_sampler_view_reference(&buffer->zscan_source, NULL);

error_source:
   return false;
}

static void
cleanup_zscan_buffer(struct vl_mpeg12_buffer *buffer)
{
   unsigned i;

   assert(buffer);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      vl_zscan_cleanup_buffer(&buffer->zscan[i]);

   pipe_sampler_view_reference(&buffer->zscan_source, NULL);
}

static bool
init_idct_buffer(struct vl_mpeg12_decoder *dec, struct vl_mpeg12_buffer *buffer)
{
   struct pipe_sampler_view **idct_source_sv, **mc_source_sv;

   unsigned i;

   assert(dec && buffer);

   idct_source_sv = dec->idct_source->get_sampler_view_planes(dec->idct_source);
   if (!idct_source_sv)
      goto error_source_sv;

   mc_source_sv = dec->mc_source->get_sampler_view_planes(dec->mc_source);
   if (!mc_source_sv)
      goto error_mc_source_sv;

   for (i = 0; i < 3; ++i)
      if (!vl_idct_init_buffer(i == 0 ? &dec->idct_y : &dec->idct_c,
                               &buffer->idct[i], idct_source_sv[i],
                               mc_source_sv[i]))
         goto error_plane;

   return true;

error_plane:
   for (; i > 0; --i)
      vl_idct_cleanup_buffer(&buffer->idct[i - 1]);

error_mc_source_sv:
error_source_sv:
   return false;
}

static void
cleanup_idct_buffer(struct vl_mpeg12_buffer *buf)
{
   unsigned i;
   
   assert(buf);

   for (i = 0; i < 3; ++i)
      vl_idct_cleanup_buffer(&buf->idct[i]);
}

static bool
init_mc_buffer(struct vl_mpeg12_decoder *dec, struct vl_mpeg12_buffer *buf)
{
   assert(dec && buf);

   if(!vl_mc_init_buffer(&dec->mc_y, &buf->mc[0]))
      goto error_mc_y;

   if(!vl_mc_init_buffer(&dec->mc_c, &buf->mc[1]))
      goto error_mc_cb;

   if(!vl_mc_init_buffer(&dec->mc_c, &buf->mc[2]))
      goto error_mc_cr;

   return true;

error_mc_cr:
   vl_mc_cleanup_buffer(&buf->mc[1]);

error_mc_cb:
   vl_mc_cleanup_buffer(&buf->mc[0]);

error_mc_y:
   return false;
}

static void
cleanup_mc_buffer(struct vl_mpeg12_buffer *buf)
{
   unsigned i;

   assert(buf);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      vl_mc_cleanup_buffer(&buf->mc[i]);
}

static inline void
MacroBlockTypeToPipeWeights(const struct pipe_mpeg12_macroblock *mb, unsigned weights[2])
{
   assert(mb);

   switch (mb->macroblock_type & (PIPE_MPEG12_MB_TYPE_MOTION_FORWARD | PIPE_MPEG12_MB_TYPE_MOTION_BACKWARD)) {
   case PIPE_MPEG12_MB_TYPE_MOTION_FORWARD:
      weights[0] = PIPE_VIDEO_MV_WEIGHT_MAX;
      weights[1] = PIPE_VIDEO_MV_WEIGHT_MIN;
      break;

   case (PIPE_MPEG12_MB_TYPE_MOTION_FORWARD | PIPE_MPEG12_MB_TYPE_MOTION_BACKWARD):
      weights[0] = PIPE_VIDEO_MV_WEIGHT_HALF;
      weights[1] = PIPE_VIDEO_MV_WEIGHT_HALF;
      break;

   case PIPE_MPEG12_MB_TYPE_MOTION_BACKWARD:
      weights[0] = PIPE_VIDEO_MV_WEIGHT_MIN;
      weights[1] = PIPE_VIDEO_MV_WEIGHT_MAX;
      break;

   default:
      if (mb->macroblock_type & PIPE_MPEG12_MB_TYPE_INTRA) {
         weights[0] = PIPE_VIDEO_MV_WEIGHT_MIN;
         weights[1] = PIPE_VIDEO_MV_WEIGHT_MIN;
      } else {
         /* no motion vector, but also not intra mb ->
            just copy the old frame content */
         weights[0] = PIPE_VIDEO_MV_WEIGHT_MAX;
         weights[1] = PIPE_VIDEO_MV_WEIGHT_MIN;
      }
      break;
   }
}

static inline struct vl_motionvector
MotionVectorToPipe(const struct pipe_mpeg12_macroblock *mb, unsigned vector,
                   unsigned field_select_mask, unsigned weight)
{
   struct vl_motionvector mv;

   assert(mb);

   if (mb->macroblock_type & (PIPE_MPEG12_MB_TYPE_MOTION_FORWARD | PIPE_MPEG12_MB_TYPE_MOTION_BACKWARD)) {
      switch (mb->macroblock_modes.bits.frame_motion_type) {
      case PIPE_MPEG12_MO_TYPE_FRAME:
         mv.top.x = mb->PMV[0][vector][0];
         mv.top.y = mb->PMV[0][vector][1];
         mv.top.field_select = PIPE_VIDEO_FRAME;
         mv.top.weight = weight;

         mv.bottom.x = mb->PMV[0][vector][0];
         mv.bottom.y = mb->PMV[0][vector][1];
         mv.bottom.weight = weight;
         mv.bottom.field_select = PIPE_VIDEO_FRAME;
         break;

      case PIPE_MPEG12_MO_TYPE_FIELD:
         mv.top.x = mb->PMV[0][vector][0];
         mv.top.y = mb->PMV[0][vector][1];
         mv.top.field_select = (mb->motion_vertical_field_select & field_select_mask) ?
            PIPE_VIDEO_BOTTOM_FIELD : PIPE_VIDEO_TOP_FIELD;
         mv.top.weight = weight;

         mv.bottom.x = mb->PMV[1][vector][0];
         mv.bottom.y = mb->PMV[1][vector][1];
         mv.bottom.field_select = (mb->motion_vertical_field_select & (field_select_mask << 2)) ?
            PIPE_VIDEO_BOTTOM_FIELD : PIPE_VIDEO_TOP_FIELD;
         mv.bottom.weight = weight;
         break;

      default: // TODO: Support DUALPRIME and 16x8
         break;
      }
   } else {
      mv.top.x = mv.top.y = 0;
      mv.top.field_select = PIPE_VIDEO_FRAME;
      mv.top.weight = weight;

      mv.bottom.x = mv.bottom.y = 0;
      mv.bottom.field_select = PIPE_VIDEO_FRAME;
      mv.bottom.weight = weight;
   }
   return mv;
}

static inline void
UploadYcbcrBlocks(struct vl_mpeg12_decoder *dec,
                  struct vl_mpeg12_buffer *buf,
                  const struct pipe_mpeg12_macroblock *mb)
{
   unsigned intra;
   unsigned tb, x, y, num_blocks = 0;

   assert(dec && buf);
   assert(mb);

   if (!mb->coded_block_pattern)
      return;

   intra = mb->macroblock_type & PIPE_MPEG12_MB_TYPE_INTRA ? 1 : 0;

   for (y = 0; y < 2; ++y) {
      for (x = 0; x < 2; ++x) {
         if (mb->coded_block_pattern & const_empty_block_mask_420[0][y][x]) {

            struct vl_ycbcr_block *stream = buf->ycbcr_stream[0];
            stream->x = mb->x * 2 + x;
            stream->y = mb->y * 2 + y;
            stream->intra = intra;
            stream->coding = mb->macroblock_modes.bits.dct_type;
            stream->block_num = buf->block_num++;

            buf->num_ycbcr_blocks[0]++;
            buf->ycbcr_stream[0]++;

            num_blocks++;
         }
      }
   }

   /* TODO: Implement 422, 444 */
   //assert(ctx->base.chroma_format == PIPE_VIDEO_CHROMA_FORMAT_420);

   for (tb = 1; tb < 3; ++tb) {
      if (mb->coded_block_pattern & const_empty_block_mask_420[tb][0][0]) {

         struct vl_ycbcr_block *stream = buf->ycbcr_stream[tb];
         stream->x = mb->x;
         stream->y = mb->y;
         stream->intra = intra;
         stream->coding = 0;
         stream->block_num = buf->block_num++;

         buf->num_ycbcr_blocks[tb]++;
         buf->ycbcr_stream[tb]++;

         num_blocks++;
      }
   }

   memcpy(buf->texels, mb->blocks, 64 * sizeof(short) * num_blocks);
   buf->texels += 64 * num_blocks;
}

static void
vl_mpeg12_destroy_buffer(struct vl_mpeg12_buffer *buf)
{

   assert(buf);

   cleanup_zscan_buffer(buf);
   cleanup_idct_buffer(buf);
   cleanup_mc_buffer(buf);
   vl_vb_cleanup(&buf->vertex_stream);

   FREE(buf);
}

static void
vl_mpeg12_destroy(struct pipe_video_codec *decoder)
{
   struct vl_mpeg12_decoder *dec = (struct vl_mpeg12_decoder*)decoder;
   unsigned i;

   assert(decoder);

   free_video_buffer_privates(dec);

   /* Asserted in softpipe_delete_fs_state() for some reason */
   dec->context->bind_vs_state(dec->context, NULL);
   dec->context->bind_fs_state(dec->context, NULL);

   dec->context->delete_depth_stencil_alpha_state(dec->context, dec->dsa);
   dec->context->delete_sampler_state(dec->context, dec->sampler_ycbcr);

   vl_mc_cleanup(&dec->mc_y);
   vl_mc_cleanup(&dec->mc_c);
   dec->mc_source->destroy(dec->mc_source);

   if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      vl_idct_cleanup(&dec->idct_y);
      vl_idct_cleanup(&dec->idct_c);
      dec->idct_source->destroy(dec->idct_source);
   }

   vl_zscan_cleanup(&dec->zscan_y);
   vl_zscan_cleanup(&dec->zscan_c);

   dec->context->delete_vertex_elements_state(dec->context, dec->ves_ycbcr);
   dec->context->delete_vertex_elements_state(dec->context, dec->ves_mv);

   pipe_resource_reference(&dec->quads.buffer.resource, NULL);
   pipe_resource_reference(&dec->pos.buffer.resource, NULL);

   pipe_sampler_view_reference(&dec->zscan_linear, NULL);
   pipe_sampler_view_reference(&dec->zscan_normal, NULL);
   pipe_sampler_view_reference(&dec->zscan_alternate, NULL);

   for (i = 0; i < 4; ++i)
      if (dec->dec_buffers[i])
         vl_mpeg12_destroy_buffer(dec->dec_buffers[i]);

   dec->context->destroy(dec->context);

   FREE(dec);
}

static struct vl_mpeg12_buffer *
vl_mpeg12_get_decode_buffer(struct vl_mpeg12_decoder *dec, struct pipe_video_buffer *target)
{
   struct video_buffer_private *priv;
   struct vl_mpeg12_buffer *buffer;

   assert(dec);

   priv = get_video_buffer_private(dec, target);
   if (priv->buffer)
      return priv->buffer;

   buffer = dec->dec_buffers[dec->current_buffer];
   if (buffer)
      return buffer;

   buffer = CALLOC_STRUCT(vl_mpeg12_buffer);
   if (!buffer)
      return NULL;

   if (!vl_vb_init(&buffer->vertex_stream, dec->context,
                   dec->base.width / VL_MACROBLOCK_WIDTH,
                   dec->base.height / VL_MACROBLOCK_HEIGHT))
      goto error_vertex_buffer;

   if (!init_mc_buffer(dec, buffer))
      goto error_mc;

   if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT)
      if (!init_idct_buffer(dec, buffer))
         goto error_idct;

   if (!init_zscan_buffer(dec, buffer))
      goto error_zscan;

   if (dec->base.entrypoint == PIPE_VIDEO_ENTRYPOINT_BITSTREAM)
      vl_mpg12_bs_init(&buffer->bs, &dec->base);

   if (dec->base.expect_chunked_decode)
      priv->buffer = buffer;
   else
      dec->dec_buffers[dec->current_buffer] = buffer;

   return buffer;

error_zscan:
   cleanup_idct_buffer(buffer);

error_idct:
   cleanup_mc_buffer(buffer);

error_mc:
   vl_vb_cleanup(&buffer->vertex_stream);

error_vertex_buffer:
   FREE(buffer);
   return NULL;
}

static void
vl_mpeg12_begin_frame(struct pipe_video_codec *decoder,
                      struct pipe_video_buffer *target,
                      struct pipe_picture_desc *picture)
{
   struct vl_mpeg12_decoder *dec = (struct vl_mpeg12_decoder *)decoder;
   struct pipe_mpeg12_picture_desc *desc = (struct pipe_mpeg12_picture_desc *)picture;
   struct vl_mpeg12_buffer *buf;

   struct pipe_resource *tex;
   struct pipe_box rect = { 0, 0, 0, 1, 1, 1 };

   uint8_t intra_matrix[64];
   uint8_t non_intra_matrix[64];

   unsigned i;

   assert(dec && target && picture);

   buf = vl_mpeg12_get_decode_buffer(dec, target);
   assert(buf);

   if (dec->base.entrypoint == PIPE_VIDEO_ENTRYPOINT_BITSTREAM) {
      memcpy(intra_matrix, desc->intra_matrix, sizeof(intra_matrix));
      memcpy(non_intra_matrix, desc->non_intra_matrix, sizeof(non_intra_matrix));
      intra_matrix[0] = 1 << (7 - desc->intra_dc_precision);
   } else {
      memset(intra_matrix, 0x10, sizeof(intra_matrix));
      memset(non_intra_matrix, 0x10, sizeof(non_intra_matrix));
   }

   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      struct vl_zscan *zscan = i == 0 ? &dec->zscan_y : &dec->zscan_c;
      vl_zscan_upload_quant(zscan, &buf->zscan[i], intra_matrix, true);
      vl_zscan_upload_quant(zscan, &buf->zscan[i], non_intra_matrix, false);
   }

   vl_vb_map(&buf->vertex_stream, dec->context);

   tex = buf->zscan_source->texture;
   rect.width = tex->width0;
   rect.height = tex->height0;

   buf->texels =
      dec->context->transfer_map(dec->context, tex, 0,
                                 PIPE_TRANSFER_WRITE |
                                 PIPE_TRANSFER_DISCARD_RANGE,
                                 &rect, &buf->tex_transfer);

   buf->block_num = 0;

   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      buf->ycbcr_stream[i] = vl_vb_get_ycbcr_stream(&buf->vertex_stream, i);
      buf->num_ycbcr_blocks[i] = 0;
   }

   for (i = 0; i < VL_MAX_REF_FRAMES; ++i)
      buf->mv_stream[i] = vl_vb_get_mv_stream(&buf->vertex_stream, i);

   if (dec->base.entrypoint >= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      for (i = 0; i < VL_NUM_COMPONENTS; ++i)
         vl_zscan_set_layout(&buf->zscan[i], dec->zscan_linear);
   }
}

static void
vl_mpeg12_decode_macroblock(struct pipe_video_codec *decoder,
                            struct pipe_video_buffer *target,
                            struct pipe_picture_desc *picture,
                            const struct pipe_macroblock *macroblocks,
                            unsigned num_macroblocks)
{
   struct vl_mpeg12_decoder *dec = (struct vl_mpeg12_decoder *)decoder;
   const struct pipe_mpeg12_macroblock *mb = (const struct pipe_mpeg12_macroblock *)macroblocks;
   struct pipe_mpeg12_picture_desc *desc = (struct pipe_mpeg12_picture_desc *)picture;
   struct vl_mpeg12_buffer *buf;

   unsigned i, j, mv_weights[2];

   assert(dec && target && picture);
   assert(macroblocks && macroblocks->codec == PIPE_VIDEO_FORMAT_MPEG12);

   buf = vl_mpeg12_get_decode_buffer(dec, target);
   assert(buf);

   for (; num_macroblocks > 0; --num_macroblocks) {
      unsigned mb_addr = mb->y * dec->width_in_macroblocks + mb->x;

      if (mb->macroblock_type & (PIPE_MPEG12_MB_TYPE_PATTERN | PIPE_MPEG12_MB_TYPE_INTRA))
         UploadYcbcrBlocks(dec, buf, mb);

      MacroBlockTypeToPipeWeights(mb, mv_weights);

      for (i = 0; i < 2; ++i) {
          if (!desc->ref[i]) continue;

         buf->mv_stream[i][mb_addr] = MotionVectorToPipe
         (
            mb, i,
            i ? PIPE_MPEG12_FS_FIRST_BACKWARD : PIPE_MPEG12_FS_FIRST_FORWARD,
            mv_weights[i]
         );
      }

      /* see section 7.6.6 of the spec */
      if (mb->num_skipped_macroblocks > 0) {
         struct vl_motionvector skipped_mv[2];

         if (desc->ref[0] && !desc->ref[1]) {
            skipped_mv[0].top.x = skipped_mv[0].top.y = 0;
            skipped_mv[0].top.weight = PIPE_VIDEO_MV_WEIGHT_MAX;
         } else {
           skipped_mv[0] = buf->mv_stream[0][mb_addr];
           skipped_mv[1] = buf->mv_stream[1][mb_addr];
         }
         skipped_mv[0].top.field_select = PIPE_VIDEO_FRAME;
         skipped_mv[1].top.field_select = PIPE_VIDEO_FRAME;

         skipped_mv[0].bottom = skipped_mv[0].top;
         skipped_mv[1].bottom = skipped_mv[1].top;

         ++mb_addr;
         for (i = 0; i < mb->num_skipped_macroblocks; ++i, ++mb_addr) {
            for (j = 0; j < 2; ++j) {
               if (!desc->ref[j]) continue;
               buf->mv_stream[j][mb_addr] = skipped_mv[j];

            }
         }
      }

      ++mb;
   }
}

static void
vl_mpeg12_decode_bitstream(struct pipe_video_codec *decoder,
                           struct pipe_video_buffer *target,
                           struct pipe_picture_desc *picture,
                           unsigned num_buffers,
                           const void * const *buffers,
                           const unsigned *sizes)
{
   struct vl_mpeg12_decoder *dec = (struct vl_mpeg12_decoder *)decoder;
   struct pipe_mpeg12_picture_desc *desc = (struct pipe_mpeg12_picture_desc *)picture;
   struct vl_mpeg12_buffer *buf;
   
   unsigned i;

   assert(dec && target && picture);

   buf = vl_mpeg12_get_decode_buffer(dec, target);
   assert(buf);

   for (i = 0; i < VL_NUM_COMPONENTS; ++i)
      vl_zscan_set_layout(&buf->zscan[i], desc->alternate_scan ?
                          dec->zscan_alternate : dec->zscan_normal);

   vl_mpg12_bs_decode(&buf->bs, target, desc, num_buffers, buffers, sizes);
}

static void
vl_mpeg12_end_frame(struct pipe_video_codec *decoder,
                    struct pipe_video_buffer *target,
                    struct pipe_picture_desc *picture)
{
   struct vl_mpeg12_decoder *dec = (struct vl_mpeg12_decoder *)decoder;
   struct pipe_mpeg12_picture_desc *desc = (struct pipe_mpeg12_picture_desc *)picture;
   struct pipe_sampler_view **ref_frames[2];
   struct pipe_sampler_view **mc_source_sv;
   struct pipe_surface **target_surfaces;
   struct pipe_vertex_buffer vb[3];
   struct vl_mpeg12_buffer *buf;

   const unsigned *plane_order;
   unsigned i, j, component;
   unsigned nr_components;

   assert(dec && target && picture);
   assert(!target->interlaced);

   buf = vl_mpeg12_get_decode_buffer(dec, target);

   vl_vb_unmap(&buf->vertex_stream, dec->context);

   dec->context->transfer_unmap(dec->context, buf->tex_transfer);

   vb[0] = dec->quads;
   vb[1] = dec->pos;

   target_surfaces = get_video_buffer_private(dec, target)->surfaces;

   for (i = 0; i < VL_MAX_REF_FRAMES; ++i) {
      if (desc->ref[i])
         ref_frames[i] = get_video_buffer_private(dec, desc->ref[i])->sampler_view_planes;
      else
         ref_frames[i] = NULL;
   }

   dec->context->bind_vertex_elements_state(dec->context, dec->ves_mv);
   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      if (!target_surfaces[i]) continue;

      vl_mc_set_surface(&buf->mc[i], target_surfaces[i]);

      for (j = 0; j < VL_MAX_REF_FRAMES; ++j) {
         if (!ref_frames[j] || !ref_frames[j][i]) continue;

         vb[2] = vl_vb_get_mv(&buf->vertex_stream, j);
         dec->context->set_vertex_buffers(dec->context, 0, 3, vb);

         vl_mc_render_ref(i ? &dec->mc_c : &dec->mc_y, &buf->mc[i], ref_frames[j][i]);
      }
   }

   dec->context->bind_vertex_elements_state(dec->context, dec->ves_ycbcr);
   for (i = 0; i < VL_NUM_COMPONENTS; ++i) {
      if (!buf->num_ycbcr_blocks[i]) continue;

      vb[1] = vl_vb_get_ycbcr(&buf->vertex_stream, i);
      dec->context->set_vertex_buffers(dec->context, 0, 2, vb);

      vl_zscan_render(i ? &dec->zscan_c : & dec->zscan_y, &buf->zscan[i] , buf->num_ycbcr_blocks[i]);

      if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT)
         vl_idct_flush(i ? &dec->idct_c : &dec->idct_y, &buf->idct[i], buf->num_ycbcr_blocks[i]);
   }

   plane_order = vl_video_buffer_plane_order(target->buffer_format);
   mc_source_sv = dec->mc_source->get_sampler_view_planes(dec->mc_source);
   for (i = 0, component = 0; component < VL_NUM_COMPONENTS; ++i) {
      if (!target_surfaces[i]) continue;

      nr_components = util_format_get_nr_components(target_surfaces[i]->texture->format);
      for (j = 0; j < nr_components; ++j, ++component) {
         unsigned plane = plane_order[component];
         if (!buf->num_ycbcr_blocks[plane]) continue;

         vb[1] = vl_vb_get_ycbcr(&buf->vertex_stream, plane);
         dec->context->set_vertex_buffers(dec->context, 0, 2, vb);

         if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT)
            vl_idct_prepare_stage2(i ? &dec->idct_c : &dec->idct_y, &buf->idct[plane]);
         else {
            dec->context->set_sampler_views(dec->context,
                                            PIPE_SHADER_FRAGMENT, 0, 1,
                                            &mc_source_sv[plane]);
            dec->context->bind_sampler_states(dec->context,
                                              PIPE_SHADER_FRAGMENT,
                                              0, 1, &dec->sampler_ycbcr);
         }
         vl_mc_render_ycbcr(i ? &dec->mc_c : &dec->mc_y, &buf->mc[i], j, buf->num_ycbcr_blocks[plane]);
      }
   }
   dec->context->flush(dec->context, NULL, 0);
   ++dec->current_buffer;
   dec->current_buffer %= 4;
}

static void
vl_mpeg12_flush(struct pipe_video_codec *decoder)
{
   assert(decoder);

   //Noop, for shaders it is much faster to flush everything in end_frame
}

static bool
init_pipe_state(struct vl_mpeg12_decoder *dec)
{
   struct pipe_depth_stencil_alpha_state dsa;
   struct pipe_sampler_state sampler;
   unsigned i;

   assert(dec);

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
   dec->dsa = dec->context->create_depth_stencil_alpha_state(dec->context, &dsa);
   dec->context->bind_depth_stencil_alpha_state(dec->context, dec->dsa);

   memset(&sampler, 0, sizeof(sampler));
   sampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   sampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   sampler.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_BORDER;
   sampler.min_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
   sampler.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
   sampler.compare_mode = PIPE_TEX_COMPARE_NONE;
   sampler.compare_func = PIPE_FUNC_ALWAYS;
   sampler.normalized_coords = 1;
   dec->sampler_ycbcr = dec->context->create_sampler_state(dec->context, &sampler);
   if (!dec->sampler_ycbcr)
      return false;

   return true;
}

static const struct format_config*
find_format_config(struct vl_mpeg12_decoder *dec, const struct format_config configs[], unsigned num_configs)
{
   struct pipe_screen *screen;
   unsigned i;

   assert(dec);

   screen = dec->context->screen;

   for (i = 0; i < num_configs; ++i) {
      if (!screen->is_format_supported(screen, configs[i].zscan_source_format, PIPE_TEXTURE_2D,
                                       1, PIPE_BIND_SAMPLER_VIEW))
         continue;

      if (configs[i].idct_source_format != PIPE_FORMAT_NONE) {
         if (!screen->is_format_supported(screen, configs[i].idct_source_format, PIPE_TEXTURE_2D,
                                          1, PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))
            continue;

         if (!screen->is_format_supported(screen, configs[i].mc_source_format, PIPE_TEXTURE_3D,
                                          1, PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))
            continue;
      } else {
         if (!screen->is_format_supported(screen, configs[i].mc_source_format, PIPE_TEXTURE_2D,
                                          1, PIPE_BIND_SAMPLER_VIEW | PIPE_BIND_RENDER_TARGET))
            continue;
      }
      return &configs[i];
   }

   return NULL;
}

static bool
init_zscan(struct vl_mpeg12_decoder *dec, const struct format_config* format_config)
{
   unsigned num_channels;

   assert(dec);

   dec->zscan_source_format = format_config->zscan_source_format;
   dec->zscan_linear = vl_zscan_layout(dec->context, vl_zscan_linear, dec->blocks_per_line);
   dec->zscan_normal = vl_zscan_layout(dec->context, vl_zscan_normal, dec->blocks_per_line);
   dec->zscan_alternate = vl_zscan_layout(dec->context, vl_zscan_alternate, dec->blocks_per_line);

   num_channels = dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT ? 4 : 1;

   if (!vl_zscan_init(&dec->zscan_y, dec->context, dec->base.width, dec->base.height,
                      dec->blocks_per_line, dec->num_blocks, num_channels))
      return false;

   if (!vl_zscan_init(&dec->zscan_c, dec->context, dec->chroma_width, dec->chroma_height,
                      dec->blocks_per_line, dec->num_blocks, num_channels))
      return false;

   return true;
}

static bool
init_idct(struct vl_mpeg12_decoder *dec, const struct format_config* format_config)
{
   unsigned nr_of_idct_render_targets, max_inst;
   enum pipe_format formats[3];
   struct pipe_video_buffer templat;

   struct pipe_sampler_view *matrix = NULL;

   nr_of_idct_render_targets = dec->context->screen->get_param
   (
      dec->context->screen, PIPE_CAP_MAX_RENDER_TARGETS
   );
   
   max_inst = dec->context->screen->get_shader_param
   (
      dec->context->screen, PIPE_SHADER_FRAGMENT, PIPE_SHADER_CAP_MAX_INSTRUCTIONS
   );

   // Just assume we need 32 inst per render target, not 100% true, but should work in most cases
   if (nr_of_idct_render_targets >= 4 && max_inst >= 32*4)
      // more than 4 render targets usually doesn't makes any seens
      nr_of_idct_render_targets = 4;
   else
      nr_of_idct_render_targets = 1;

   formats[0] = formats[1] = formats[2] = format_config->idct_source_format;
   memset(&templat, 0, sizeof(templat));
   templat.width = dec->base.width / 4;
   templat.height = dec->base.height;
   templat.chroma_format = dec->base.chroma_format;
   dec->idct_source = vl_video_buffer_create_ex
   (
      dec->context, &templat,
      formats, 1, 1, PIPE_USAGE_DEFAULT
   );

   if (!dec->idct_source)
      goto error_idct_source;

   formats[0] = formats[1] = formats[2] = format_config->mc_source_format;
   memset(&templat, 0, sizeof(templat));
   templat.width = dec->base.width / nr_of_idct_render_targets;
   templat.height = dec->base.height / 4;
   templat.chroma_format = dec->base.chroma_format;
   dec->mc_source = vl_video_buffer_create_ex
   (
      dec->context, &templat,
      formats, nr_of_idct_render_targets, 1, PIPE_USAGE_DEFAULT
   );

   if (!dec->mc_source)
      goto error_mc_source;

   if (!(matrix = vl_idct_upload_matrix(dec->context, format_config->idct_scale)))
      goto error_matrix;

   if (!vl_idct_init(&dec->idct_y, dec->context, dec->base.width, dec->base.height,
                     nr_of_idct_render_targets, matrix, matrix))
      goto error_y;

   if(!vl_idct_init(&dec->idct_c, dec->context, dec->chroma_width, dec->chroma_height,
                    nr_of_idct_render_targets, matrix, matrix))
      goto error_c;

   pipe_sampler_view_reference(&matrix, NULL);

   return true;

error_c:
   vl_idct_cleanup(&dec->idct_y);

error_y:
   pipe_sampler_view_reference(&matrix, NULL);

error_matrix:
   dec->mc_source->destroy(dec->mc_source);

error_mc_source:
   dec->idct_source->destroy(dec->idct_source);

error_idct_source:
   return false;
}

static bool
init_mc_source_widthout_idct(struct vl_mpeg12_decoder *dec, const struct format_config* format_config)
{
   enum pipe_format formats[3];
   struct pipe_video_buffer templat;

   formats[0] = formats[1] = formats[2] = format_config->mc_source_format;
   memset(&templat, 0, sizeof(templat));
   templat.width = dec->base.width;
   templat.height = dec->base.height;
   templat.chroma_format = dec->base.chroma_format;
   dec->mc_source = vl_video_buffer_create_ex
   (
      dec->context, &templat,
      formats, 1, 1, PIPE_USAGE_DEFAULT
   );
      
   return dec->mc_source != NULL;
}

static void
mc_vert_shader_callback(void *priv, struct vl_mc *mc,
                        struct ureg_program *shader,
                        unsigned first_output,
                        struct ureg_dst tex)
{
   struct vl_mpeg12_decoder *dec = priv;
   struct ureg_dst o_vtex;

   assert(priv && mc);
   assert(shader);

   if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      struct vl_idct *idct = mc == &dec->mc_y ? &dec->idct_y : &dec->idct_c;
      vl_idct_stage2_vert_shader(idct, shader, first_output, tex);
   } else {
      o_vtex = ureg_DECL_output(shader, TGSI_SEMANTIC_GENERIC, first_output);
      ureg_MOV(shader, ureg_writemask(o_vtex, TGSI_WRITEMASK_XY), ureg_src(tex));
   }
}

static void
mc_frag_shader_callback(void *priv, struct vl_mc *mc,
                        struct ureg_program *shader,
                        unsigned first_input,
                        struct ureg_dst dst)
{
   struct vl_mpeg12_decoder *dec = priv;
   struct ureg_src src, sampler;

   assert(priv && mc);
   assert(shader);

   if (dec->base.entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      struct vl_idct *idct = mc == &dec->mc_y ? &dec->idct_y : &dec->idct_c;
      vl_idct_stage2_frag_shader(idct, shader, first_input, dst);
   } else {
      src = ureg_DECL_fs_input(shader, TGSI_SEMANTIC_GENERIC, first_input, TGSI_INTERPOLATE_LINEAR);
      sampler = ureg_DECL_sampler(shader, 0);
      ureg_TEX(shader, dst, TGSI_TEXTURE_2D, src, sampler);
   }
}

struct pipe_video_codec *
vl_create_mpeg12_decoder(struct pipe_context *context,
                         const struct pipe_video_codec *templat)
{
   const unsigned block_size_pixels = VL_BLOCK_WIDTH * VL_BLOCK_HEIGHT;
   const struct format_config *format_config;
   struct vl_mpeg12_decoder *dec;

   assert(u_reduce_video_profile(templat->profile) == PIPE_VIDEO_FORMAT_MPEG12);

   dec = CALLOC_STRUCT(vl_mpeg12_decoder);

   if (!dec)
      return NULL;

   dec->base = *templat;
   dec->base.context = context;
   dec->context = context->screen->context_create(context->screen, NULL, 0);

   dec->base.destroy = vl_mpeg12_destroy;
   dec->base.begin_frame = vl_mpeg12_begin_frame;
   dec->base.decode_macroblock = vl_mpeg12_decode_macroblock;
   dec->base.decode_bitstream = vl_mpeg12_decode_bitstream;
   dec->base.end_frame = vl_mpeg12_end_frame;
   dec->base.flush = vl_mpeg12_flush;

   dec->blocks_per_line = MAX2(util_next_power_of_two(dec->base.width) / block_size_pixels, 4);
   dec->num_blocks = (dec->base.width * dec->base.height) / block_size_pixels;
   dec->width_in_macroblocks = align(dec->base.width, VL_MACROBLOCK_WIDTH) / VL_MACROBLOCK_WIDTH;

   /* TODO: Implement 422, 444 */
   assert(dec->base.chroma_format == PIPE_VIDEO_CHROMA_FORMAT_420);

   if (dec->base.chroma_format == PIPE_VIDEO_CHROMA_FORMAT_420) {
      dec->chroma_width = dec->base.width / 2;
      dec->chroma_height = dec->base.height / 2;
      dec->num_blocks = dec->num_blocks * 2;
   } else if (dec->base.chroma_format == PIPE_VIDEO_CHROMA_FORMAT_422) {
      dec->chroma_width = dec->base.width / 2;
      dec->chroma_height = dec->base.height;
      dec->num_blocks = dec->num_blocks * 2 + dec->num_blocks;
   } else {
      dec->chroma_width = dec->base.width;
      dec->chroma_height = dec->base.height;
      dec->num_blocks = dec->num_blocks * 3;
   }

   dec->quads = vl_vb_upload_quads(dec->context);
   dec->pos = vl_vb_upload_pos(
      dec->context,
      dec->base.width / VL_MACROBLOCK_WIDTH,
      dec->base.height / VL_MACROBLOCK_HEIGHT
   );

   dec->ves_ycbcr = vl_vb_get_ves_ycbcr(dec->context);
   dec->ves_mv = vl_vb_get_ves_mv(dec->context);

   switch (templat->entrypoint) {
   case PIPE_VIDEO_ENTRYPOINT_BITSTREAM:
      format_config = find_format_config(dec, bitstream_format_config, num_bitstream_format_configs);
      break;

   case PIPE_VIDEO_ENTRYPOINT_IDCT:
      format_config = find_format_config(dec, idct_format_config, num_idct_format_configs);
      break;

   case PIPE_VIDEO_ENTRYPOINT_MC:
      format_config = find_format_config(dec, mc_format_config, num_mc_format_configs);
      break;

   default:
      assert(0);
      FREE(dec);
      return NULL;
   }

   if (!format_config) {
      FREE(dec);
      return NULL;
   }

   if (!init_zscan(dec, format_config))
      goto error_zscan;

   if (templat->entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      if (!init_idct(dec, format_config))
         goto error_sources;
   } else {
      if (!init_mc_source_widthout_idct(dec, format_config))
         goto error_sources;
   }

   if (!vl_mc_init(&dec->mc_y, dec->context, dec->base.width, dec->base.height,
                   VL_MACROBLOCK_HEIGHT, format_config->mc_scale,
                   mc_vert_shader_callback, mc_frag_shader_callback, dec))
      goto error_mc_y;

   // TODO
   if (!vl_mc_init(&dec->mc_c, dec->context, dec->base.width, dec->base.height,
                   VL_BLOCK_HEIGHT, format_config->mc_scale,
                   mc_vert_shader_callback, mc_frag_shader_callback, dec))
      goto error_mc_c;

   if (!init_pipe_state(dec))
      goto error_pipe_state;

   list_inithead(&dec->buffer_privates);

   return &dec->base;

error_pipe_state:
   vl_mc_cleanup(&dec->mc_c);

error_mc_c:
   vl_mc_cleanup(&dec->mc_y);

error_mc_y:
   if (templat->entrypoint <= PIPE_VIDEO_ENTRYPOINT_IDCT) {
      vl_idct_cleanup(&dec->idct_y);
      vl_idct_cleanup(&dec->idct_c);
      dec->idct_source->destroy(dec->idct_source);
   }
   dec->mc_source->destroy(dec->mc_source);

error_sources:
   vl_zscan_cleanup(&dec->zscan_y);
   vl_zscan_cleanup(&dec->zscan_c);

error_zscan:
   FREE(dec);
   return NULL;
}
