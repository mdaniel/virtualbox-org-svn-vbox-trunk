/**************************************************************************
 *
 * Copyright 2013 Advanced Micro Devices, Inc.
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

/*
 * Authors:
 *      Christian König <christian.koenig@amd.com>
 *
 */

#ifndef OMX_VID_DEC_H
#define OMX_VID_DEC_H

#include <string.h>

#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>

#include <bellagio/st_static_component_loader.h>
#include <bellagio/omx_base_filter.h>
#include <bellagio/omx_base_video_port.h>

#include "pipe/p_video_state.h"
#include "os/os_thread.h"
#include "util/list.h"

#include "vl/vl_compositor.h"

#define OMX_VID_DEC_BASE_NAME "OMX.mesa.video_decoder"

#define OMX_VID_DEC_MPEG2_NAME "OMX.mesa.video_decoder.mpeg2"
#define OMX_VID_DEC_MPEG2_ROLE "video_decoder.mpeg2"

#define OMX_VID_DEC_AVC_NAME "OMX.mesa.video_decoder.avc"
#define OMX_VID_DEC_AVC_ROLE "video_decoder.avc"

#define OMX_VID_DEC_HEVC_NAME "OMX.mesa.video_decoder.hevc"
#define OMX_VID_DEC_HEVC_ROLE "video_decoder.hevc"

#define OMX_VID_DEC_TIMESTAMP_INVALID ((OMX_TICKS) -1)

struct vl_vlc;

DERIVEDCLASS(vid_dec_PrivateType, omx_base_filter_PrivateType)
#define vid_dec_PrivateType_FIELDS omx_base_filter_PrivateType_FIELDS \
   enum pipe_video_profile profile; \
   struct vl_screen *screen; \
   struct pipe_context *pipe; \
   struct pipe_video_codec *codec; \
   void (*Decode)(vid_dec_PrivateType *priv, struct vl_vlc *vlc, unsigned min_bits_left); \
   void (*EndFrame)(vid_dec_PrivateType *priv); \
   struct pipe_video_buffer *(*Flush)(vid_dec_PrivateType *priv, OMX_TICKS *timestamp); \
   struct pipe_video_buffer *target, *shadow; \
   union { \
      struct { \
         uint8_t intra_matrix[64]; \
         uint8_t non_intra_matrix[64]; \
      } mpeg12; \
      struct { \
         unsigned nal_ref_idc; \
         bool IdrPicFlag; \
         unsigned idr_pic_id; \
         unsigned pic_order_cnt_lsb; \
         unsigned pic_order_cnt_msb; \
         unsigned delta_pic_order_cnt_bottom; \
         unsigned delta_pic_order_cnt[2]; \
         unsigned prevFrameNumOffset; \
         struct pipe_h264_sps sps[32]; \
         struct pipe_h264_pps pps[256]; \
         struct list_head dpb_list; \
         unsigned dpb_num; \
      } h264; \
      struct { \
         unsigned temporal_id; \
         unsigned level_idc; \
         unsigned pic_width_in_luma_samples; \
         unsigned pic_height_in_luma_samples; \
         bool IdrPicFlag; \
         int slice_prev_poc; \
         void *ref_pic_set_list; \
         void *rps; \
         struct pipe_h265_sps sps[16]; \
         struct pipe_h265_pps pps[64]; \
         struct list_head dpb_list; \
         unsigned dpb_num; \
      } h265; \
   } codec_data; \
   union { \
      struct pipe_picture_desc base; \
      struct pipe_mpeg12_picture_desc mpeg12; \
      struct pipe_h264_picture_desc h264; \
      struct pipe_h265_picture_desc h265; \
   } picture; \
   unsigned num_in_buffers; \
   OMX_BUFFERHEADERTYPE *in_buffers[2]; \
   const void *inputs[2]; \
   unsigned sizes[2]; \
   OMX_TICKS timestamps[2]; \
   OMX_TICKS timestamp; \
   bool first_buf_in_frame; \
   bool frame_finished; \
   bool frame_started; \
   unsigned bytes_left; \
   const void *slice; \
   bool disable_tunnel; \
   struct vl_compositor compositor; \
   struct vl_compositor_state cstate;
ENDCLASS(vid_dec_PrivateType)

OMX_ERRORTYPE vid_dec_LoaderComponent(stLoaderComponentType *comp);

/* used by MPEG12 and H264 implementation */
void vid_dec_NeedTarget(vid_dec_PrivateType *priv);

/* vid_dec_mpeg12.c */
void vid_dec_mpeg12_Init(vid_dec_PrivateType *priv);

/* vid_dec_h264.c */
void vid_dec_h264_Init(vid_dec_PrivateType *priv);

/* vid_dec_h265.c */
void vid_dec_h265_Init(vid_dec_PrivateType *priv);

#endif
