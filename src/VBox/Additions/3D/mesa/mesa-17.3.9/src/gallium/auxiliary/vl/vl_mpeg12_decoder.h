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

#ifndef vl_mpeg12_decoder_h
#define vl_mpeg12_decoder_h

#include "pipe/p_video_codec.h"

#include "util/list.h"

#include "vl_mpeg12_bitstream.h"
#include "vl_zscan.h"
#include "vl_idct.h"
#include "vl_mc.h"

#include "vl_vertex_buffers.h"
#include "vl_video_buffer.h"

struct pipe_screen;
struct pipe_context;

struct vl_mpeg12_decoder
{
   struct pipe_video_codec base;
   struct pipe_context *context;

   unsigned chroma_width, chroma_height;

   unsigned blocks_per_line;
   unsigned num_blocks;
   unsigned width_in_macroblocks;

   enum pipe_format zscan_source_format;

   struct pipe_vertex_buffer quads;
   struct pipe_vertex_buffer pos;

   void *ves_ycbcr;
   void *ves_mv;

   void *sampler_ycbcr;

   struct pipe_sampler_view *zscan_linear;
   struct pipe_sampler_view *zscan_normal;
   struct pipe_sampler_view *zscan_alternate;

   struct pipe_video_buffer *idct_source;
   struct pipe_video_buffer *mc_source;

   struct vl_zscan zscan_y, zscan_c;
   struct vl_idct idct_y, idct_c;
   struct vl_mc mc_y, mc_c;

   void *dsa;

   unsigned current_buffer;
   struct vl_mpeg12_buffer *dec_buffers[4];

   struct list_head buffer_privates;
};

struct vl_mpeg12_buffer
{
   struct vl_vertex_buffer vertex_stream;

   unsigned block_num;
   unsigned num_ycbcr_blocks[3];

   struct pipe_sampler_view *zscan_source;

   struct vl_mpg12_bs bs;
   struct vl_zscan_buffer zscan[VL_NUM_COMPONENTS];
   struct vl_idct_buffer idct[VL_NUM_COMPONENTS];
   struct vl_mc_buffer mc[VL_NUM_COMPONENTS];

   struct pipe_transfer *tex_transfer;
   short *texels;

   struct vl_ycbcr_block *ycbcr_stream[VL_NUM_COMPONENTS];
   struct vl_motionvector *mv_stream[VL_MAX_REF_FRAMES];
};

/**
 * creates a shader based mpeg12 decoder
 */
struct pipe_video_codec *
vl_create_mpeg12_decoder(struct pipe_context *pipe,
                         const struct pipe_video_codec *templat);

#endif /* vl_mpeg12_decoder_h */
