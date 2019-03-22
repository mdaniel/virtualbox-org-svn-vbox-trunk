/**************************************************************************
 *
 * Copyright 2011 Advanced Micro Devices, Inc.
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
 *	Christian König <christian.koenig@amd.com>
 *
 */

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "pipe/p_video_codec.h"

#include "util/u_memory.h"
#include "util/u_video.h"

#include "vl/vl_defines.h"
#include "vl/vl_mpeg12_decoder.h"

#include "r600_pipe_common.h"
#include "radeon_video.h"
#include "radeon_uvd.h"

#define NUM_BUFFERS 4

#define NUM_MPEG2_REFS 6
#define NUM_H264_REFS 17
#define NUM_VC1_REFS 5

#define FB_BUFFER_OFFSET 0x1000
#define FB_BUFFER_SIZE 2048
#define FB_BUFFER_SIZE_TONGA (2048 * 64)
#define IT_SCALING_TABLE_SIZE 992
#define UVD_SESSION_CONTEXT_SIZE (128 * 1024)

/* UVD decoder representation */
struct ruvd_decoder {
	struct pipe_video_codec		base;

	ruvd_set_dtb			set_dtb;

	unsigned			stream_handle;
	unsigned			stream_type;
	unsigned			frame_number;

	struct pipe_screen		*screen;
	struct radeon_winsys*		ws;
	struct radeon_winsys_cs*	cs;

	unsigned			cur_buffer;

	struct rvid_buffer		msg_fb_it_buffers[NUM_BUFFERS];
	struct ruvd_msg			*msg;
	uint32_t			*fb;
	unsigned			fb_size;
	uint8_t				*it;

	struct rvid_buffer		bs_buffers[NUM_BUFFERS];
	void*				bs_ptr;
	unsigned			bs_size;

	struct rvid_buffer		dpb;
	bool				use_legacy;
	struct rvid_buffer		ctx;
	struct rvid_buffer		sessionctx;
	struct {
		unsigned 		data0;
		unsigned		data1;
		unsigned		cmd;
		unsigned		cntl;
	} reg;

	void				*render_pic_list[16];
};

/* flush IB to the hardware */
static int flush(struct ruvd_decoder *dec, unsigned flags)
{
	return dec->ws->cs_flush(dec->cs, flags, NULL);
}

/* add a new set register command to the IB */
static void set_reg(struct ruvd_decoder *dec, unsigned reg, uint32_t val)
{
	radeon_emit(dec->cs, RUVD_PKT0(reg >> 2, 0));
	radeon_emit(dec->cs, val);
}

/* send a command to the VCPU through the GPCOM registers */
static void send_cmd(struct ruvd_decoder *dec, unsigned cmd,
		     struct pb_buffer* buf, uint32_t off,
		     enum radeon_bo_usage usage, enum radeon_bo_domain domain)
{
	int reloc_idx;

	reloc_idx = dec->ws->cs_add_buffer(dec->cs, buf, usage | RADEON_USAGE_SYNCHRONIZED,
					   domain,
					  RADEON_PRIO_UVD);
	if (!dec->use_legacy) {
		uint64_t addr;
		addr = dec->ws->buffer_get_virtual_address(buf);
		addr = addr + off;
		set_reg(dec, dec->reg.data0, addr);
		set_reg(dec, dec->reg.data1, addr >> 32);
	} else {
		off += dec->ws->buffer_get_reloc_offset(buf);
		set_reg(dec, RUVD_GPCOM_VCPU_DATA0, off);
		set_reg(dec, RUVD_GPCOM_VCPU_DATA1, reloc_idx * 4);
	}
	set_reg(dec, dec->reg.cmd, cmd << 1);
}

/* do the codec needs an IT buffer ?*/
static bool have_it(struct ruvd_decoder *dec)
{
	return dec->stream_type == RUVD_CODEC_H264_PERF ||
		dec->stream_type == RUVD_CODEC_H265;
}

/* map the next available message/feedback/itscaling buffer */
static void map_msg_fb_it_buf(struct ruvd_decoder *dec)
{
	struct rvid_buffer* buf;
	uint8_t *ptr;

	/* grab the current message/feedback buffer */
	buf = &dec->msg_fb_it_buffers[dec->cur_buffer];

	/* and map it for CPU access */
	ptr = dec->ws->buffer_map(buf->res->buf, dec->cs, PIPE_TRANSFER_WRITE);

	/* calc buffer offsets */
	dec->msg = (struct ruvd_msg *)ptr;
	memset(dec->msg, 0, sizeof(*dec->msg));

	dec->fb = (uint32_t *)(ptr + FB_BUFFER_OFFSET);
	if (have_it(dec))
		dec->it = (uint8_t *)(ptr + FB_BUFFER_OFFSET + dec->fb_size);
}

/* unmap and send a message command to the VCPU */
static void send_msg_buf(struct ruvd_decoder *dec)
{
	struct rvid_buffer* buf;

	/* ignore the request if message/feedback buffer isn't mapped */
	if (!dec->msg || !dec->fb)
		return;

	/* grab the current message buffer */
	buf = &dec->msg_fb_it_buffers[dec->cur_buffer];

	/* unmap the buffer */
	dec->ws->buffer_unmap(buf->res->buf);
	dec->msg = NULL;
	dec->fb = NULL;
	dec->it = NULL;


	if (dec->sessionctx.res)
		send_cmd(dec, RUVD_CMD_SESSION_CONTEXT_BUFFER,
			 dec->sessionctx.res->buf, 0, RADEON_USAGE_READWRITE,
			 RADEON_DOMAIN_VRAM);

	/* and send it to the hardware */
	send_cmd(dec, RUVD_CMD_MSG_BUFFER, buf->res->buf, 0,
		 RADEON_USAGE_READ, RADEON_DOMAIN_GTT);
}

/* cycle to the next set of buffers */
static void next_buffer(struct ruvd_decoder *dec)
{
	++dec->cur_buffer;
	dec->cur_buffer %= NUM_BUFFERS;
}

/* convert the profile into something UVD understands */
static uint32_t profile2stream_type(struct ruvd_decoder *dec, unsigned family)
{
	switch (u_reduce_video_profile(dec->base.profile)) {
	case PIPE_VIDEO_FORMAT_MPEG4_AVC:
		return (family >= CHIP_TONGA) ?
			RUVD_CODEC_H264_PERF : RUVD_CODEC_H264;

	case PIPE_VIDEO_FORMAT_VC1:
		return RUVD_CODEC_VC1;

	case PIPE_VIDEO_FORMAT_MPEG12:
		return RUVD_CODEC_MPEG2;

	case PIPE_VIDEO_FORMAT_MPEG4:
		return RUVD_CODEC_MPEG4;

	case PIPE_VIDEO_FORMAT_HEVC:
		return RUVD_CODEC_H265;

	case PIPE_VIDEO_FORMAT_JPEG:
		return RUVD_CODEC_MJPEG;

	default:
		assert(0);
		return 0;
	}
}

static unsigned calc_ctx_size_h264_perf(struct ruvd_decoder *dec)
{
	unsigned width_in_mb, height_in_mb, ctx_size;
	unsigned width = align(dec->base.width, VL_MACROBLOCK_WIDTH);
	unsigned height = align(dec->base.height, VL_MACROBLOCK_HEIGHT);

	unsigned max_references = dec->base.max_references + 1;

	// picture width & height in 16 pixel units
	width_in_mb = width / VL_MACROBLOCK_WIDTH;
	height_in_mb = align(height / VL_MACROBLOCK_HEIGHT, 2);

	if (!dec->use_legacy) {
		unsigned fs_in_mb = width_in_mb * height_in_mb;
		unsigned num_dpb_buffer;
		switch(dec->base.level) {
		case 30:
			num_dpb_buffer = 8100 / fs_in_mb;
			break;
		case 31:
			num_dpb_buffer = 18000 / fs_in_mb;
			break;
		case 32:
			num_dpb_buffer = 20480 / fs_in_mb;
			break;
		case 41:
			num_dpb_buffer = 32768 / fs_in_mb;
			break;
		case 42:
			num_dpb_buffer = 34816 / fs_in_mb;
			break;
		case 50:
			num_dpb_buffer = 110400 / fs_in_mb;
			break;
		case 51:
			num_dpb_buffer = 184320 / fs_in_mb;
			break;
		default:
			num_dpb_buffer = 184320 / fs_in_mb;
			break;
		}
		num_dpb_buffer++;
		max_references = MAX2(MIN2(NUM_H264_REFS, num_dpb_buffer), max_references);
		ctx_size = max_references * align(width_in_mb * height_in_mb  * 192, 256);
	} else {
		// the firmware seems to always assume a minimum of ref frames
		max_references = MAX2(NUM_H264_REFS, max_references);
		// macroblock context buffer
		ctx_size = align(width_in_mb * height_in_mb * max_references * 192, 256);
	}

	return ctx_size;
}

static unsigned calc_ctx_size_h265_main(struct ruvd_decoder *dec)
{
	unsigned width = align(dec->base.width, VL_MACROBLOCK_WIDTH);
	unsigned height = align(dec->base.height, VL_MACROBLOCK_HEIGHT);

	unsigned max_references = dec->base.max_references + 1;

	if (dec->base.width * dec->base.height >= 4096*2000)
		max_references = MAX2(max_references, 8);
	else
		max_references = MAX2(max_references, 17);

	width = align (width, 16);
	height = align (height, 16);
	return ((width + 255) / 16) * ((height + 255) / 16) * 16 * max_references + 52 * 1024;
}

static unsigned calc_ctx_size_h265_main10(struct ruvd_decoder *dec, struct pipe_h265_picture_desc *pic)
{
	unsigned block_size, log2_ctb_size, width_in_ctb, height_in_ctb, num_16x16_block_per_ctb;
	unsigned context_buffer_size_per_ctb_row, cm_buffer_size, max_mb_address, db_left_tile_pxl_size;
	unsigned db_left_tile_ctx_size = 4096 / 16 * (32 + 16 * 4);

	unsigned width = align(dec->base.width, VL_MACROBLOCK_WIDTH);
	unsigned height = align(dec->base.height, VL_MACROBLOCK_HEIGHT);
	unsigned coeff_10bit = (pic->pps->sps->bit_depth_luma_minus8 || pic->pps->sps->bit_depth_chroma_minus8) ? 2 : 1;

	unsigned max_references = dec->base.max_references + 1;

	if (dec->base.width * dec->base.height >= 4096*2000)
		max_references = MAX2(max_references, 8);
	else
		max_references = MAX2(max_references, 17);

	block_size = (1 << (pic->pps->sps->log2_min_luma_coding_block_size_minus3 + 3));
	log2_ctb_size = block_size + pic->pps->sps->log2_diff_max_min_luma_coding_block_size;

	width_in_ctb = (width + ((1 << log2_ctb_size) - 1)) >> log2_ctb_size;
	height_in_ctb = (height + ((1 << log2_ctb_size) - 1)) >> log2_ctb_size;

	num_16x16_block_per_ctb = ((1 << log2_ctb_size) >> 4) * ((1 << log2_ctb_size) >> 4);
	context_buffer_size_per_ctb_row = align(width_in_ctb * num_16x16_block_per_ctb * 16, 256);
	max_mb_address = (unsigned) ceil(height * 8 / 2048.0);

	cm_buffer_size = max_references * context_buffer_size_per_ctb_row * height_in_ctb;
	db_left_tile_pxl_size = coeff_10bit * (max_mb_address * 2 * 2048 + 1024);

	return cm_buffer_size + db_left_tile_ctx_size + db_left_tile_pxl_size;
}

static unsigned get_db_pitch_alignment(struct ruvd_decoder *dec)
{
	if (((struct r600_common_screen*)dec->screen)->family < CHIP_VEGA10)
		return 16;
	else
		return 32;
}

/* calculate size of reference picture buffer */
static unsigned calc_dpb_size(struct ruvd_decoder *dec)
{
	unsigned width_in_mb, height_in_mb, image_size, dpb_size;

	// always align them to MB size for dpb calculation
	unsigned width = align(dec->base.width, VL_MACROBLOCK_WIDTH);
	unsigned height = align(dec->base.height, VL_MACROBLOCK_HEIGHT);

	// always one more for currently decoded picture
	unsigned max_references = dec->base.max_references + 1;

	// aligned size of a single frame
	image_size = align(width, get_db_pitch_alignment(dec)) * height;
	image_size += image_size / 2;
	image_size = align(image_size, 1024);

	// picture width & height in 16 pixel units
	width_in_mb = width / VL_MACROBLOCK_WIDTH;
	height_in_mb = align(height / VL_MACROBLOCK_HEIGHT, 2);

	switch (u_reduce_video_profile(dec->base.profile)) {
	case PIPE_VIDEO_FORMAT_MPEG4_AVC: {
		if (!dec->use_legacy) {
			unsigned fs_in_mb = width_in_mb * height_in_mb;
			unsigned alignment = 64, num_dpb_buffer;

			if (dec->stream_type == RUVD_CODEC_H264_PERF)
				alignment = 256;
			switch(dec->base.level) {
			case 30:
				num_dpb_buffer = 8100 / fs_in_mb;
				break;
			case 31:
				num_dpb_buffer = 18000 / fs_in_mb;
				break;
			case 32:
				num_dpb_buffer = 20480 / fs_in_mb;
				break;
			case 41:
				num_dpb_buffer = 32768 / fs_in_mb;
				break;
			case 42:
				num_dpb_buffer = 34816 / fs_in_mb;
				break;
			case 50:
				num_dpb_buffer = 110400 / fs_in_mb;
				break;
			case 51:
				num_dpb_buffer = 184320 / fs_in_mb;
				break;
			default:
				num_dpb_buffer = 184320 / fs_in_mb;
				break;
			}
			num_dpb_buffer++;
			max_references = MAX2(MIN2(NUM_H264_REFS, num_dpb_buffer), max_references);
			dpb_size = image_size * max_references;
			if ((dec->stream_type != RUVD_CODEC_H264_PERF) ||
			    (((struct r600_common_screen*)dec->screen)->family < CHIP_POLARIS10)) {
				dpb_size += max_references * align(width_in_mb * height_in_mb  * 192, alignment);
				dpb_size += align(width_in_mb * height_in_mb * 32, alignment);
			}
		} else {
			// the firmware seems to allways assume a minimum of ref frames
			max_references = MAX2(NUM_H264_REFS, max_references);
			// reference picture buffer
			dpb_size = image_size * max_references;
			if ((dec->stream_type != RUVD_CODEC_H264_PERF) ||
			    (((struct r600_common_screen*)dec->screen)->family < CHIP_POLARIS10)) {
				// macroblock context buffer
				dpb_size += width_in_mb * height_in_mb * max_references * 192;
				// IT surface buffer
				dpb_size += width_in_mb * height_in_mb * 32;
			}
		}
		break;
	}

	case PIPE_VIDEO_FORMAT_HEVC:
		if (dec->base.width * dec->base.height >= 4096*2000)
			max_references = MAX2(max_references, 8);
		else
			max_references = MAX2(max_references, 17);

		width = align (width, 16);
		height = align (height, 16);
		if (dec->base.profile == PIPE_VIDEO_PROFILE_HEVC_MAIN_10)
			dpb_size = align((align(width, get_db_pitch_alignment(dec)) * height * 9) / 4, 256) * max_references;
		else
			dpb_size = align((align(width, get_db_pitch_alignment(dec)) * height * 3) / 2, 256) * max_references;
		break;

	case PIPE_VIDEO_FORMAT_VC1:
		// the firmware seems to allways assume a minimum of ref frames
		max_references = MAX2(NUM_VC1_REFS, max_references);

		// reference picture buffer
		dpb_size = image_size * max_references;

		// CONTEXT_BUFFER
		dpb_size += width_in_mb * height_in_mb * 128;

		// IT surface buffer
		dpb_size += width_in_mb * 64;

		// DB surface buffer
		dpb_size += width_in_mb * 128;

		// BP
		dpb_size += align(MAX2(width_in_mb, height_in_mb) * 7 * 16, 64);
		break;

	case PIPE_VIDEO_FORMAT_MPEG12:
		// reference picture buffer, must be big enough for all frames
		dpb_size = image_size * NUM_MPEG2_REFS;
		break;

	case PIPE_VIDEO_FORMAT_MPEG4:
		// reference picture buffer
		dpb_size = image_size * max_references;

		// CM
		dpb_size += width_in_mb * height_in_mb * 64;

		// IT surface buffer
		dpb_size += align(width_in_mb * height_in_mb * 32, 64);

		dpb_size = MAX2(dpb_size, 30 * 1024 * 1024);
		break;

	case PIPE_VIDEO_FORMAT_JPEG:
		dpb_size = 0;
		break;

	default:
		// something is missing here
		assert(0);

		// at least use a sane default value
		dpb_size = 32 * 1024 * 1024;
		break;
	}
	return dpb_size;
}

/* free associated data in the video buffer callback */
static void ruvd_destroy_associated_data(void *data)
{
	/* NOOP, since we only use an intptr */
}

/* get h264 specific message bits */
static struct ruvd_h264 get_h264_msg(struct ruvd_decoder *dec, struct pipe_h264_picture_desc *pic)
{
	struct ruvd_h264 result;

	memset(&result, 0, sizeof(result));
	switch (pic->base.profile) {
	case PIPE_VIDEO_PROFILE_MPEG4_AVC_BASELINE:
	case PIPE_VIDEO_PROFILE_MPEG4_AVC_CONSTRAINED_BASELINE:
		result.profile = RUVD_H264_PROFILE_BASELINE;
		break;

	case PIPE_VIDEO_PROFILE_MPEG4_AVC_MAIN:
		result.profile = RUVD_H264_PROFILE_MAIN;
		break;

	case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH:
		result.profile = RUVD_H264_PROFILE_HIGH;
		break;

	default:
		assert(0);
		break;
	}

	result.level = dec->base.level;

	result.sps_info_flags = 0;
	result.sps_info_flags |= pic->pps->sps->direct_8x8_inference_flag << 0;
	result.sps_info_flags |= pic->pps->sps->mb_adaptive_frame_field_flag << 1;
	result.sps_info_flags |= pic->pps->sps->frame_mbs_only_flag << 2;
	result.sps_info_flags |= pic->pps->sps->delta_pic_order_always_zero_flag << 3;

	result.bit_depth_luma_minus8 = pic->pps->sps->bit_depth_luma_minus8;
	result.bit_depth_chroma_minus8 = pic->pps->sps->bit_depth_chroma_minus8;
	result.log2_max_frame_num_minus4 = pic->pps->sps->log2_max_frame_num_minus4;
	result.pic_order_cnt_type = pic->pps->sps->pic_order_cnt_type;
	result.log2_max_pic_order_cnt_lsb_minus4 = pic->pps->sps->log2_max_pic_order_cnt_lsb_minus4;

	switch (dec->base.chroma_format) {
	case PIPE_VIDEO_CHROMA_FORMAT_NONE:
		/* TODO: assert? */
		break;
	case PIPE_VIDEO_CHROMA_FORMAT_400:
		result.chroma_format = 0;
		break;
	case PIPE_VIDEO_CHROMA_FORMAT_420:
		result.chroma_format = 1;
		break;
	case PIPE_VIDEO_CHROMA_FORMAT_422:
		result.chroma_format = 2;
		break;
	case PIPE_VIDEO_CHROMA_FORMAT_444:
		result.chroma_format = 3;
		break;
	}

	result.pps_info_flags = 0;
	result.pps_info_flags |= pic->pps->transform_8x8_mode_flag << 0;
	result.pps_info_flags |= pic->pps->redundant_pic_cnt_present_flag << 1;
	result.pps_info_flags |= pic->pps->constrained_intra_pred_flag << 2;
	result.pps_info_flags |= pic->pps->deblocking_filter_control_present_flag << 3;
	result.pps_info_flags |= pic->pps->weighted_bipred_idc << 4;
	result.pps_info_flags |= pic->pps->weighted_pred_flag << 6;
	result.pps_info_flags |= pic->pps->bottom_field_pic_order_in_frame_present_flag << 7;
	result.pps_info_flags |= pic->pps->entropy_coding_mode_flag << 8;

	result.num_slice_groups_minus1 = pic->pps->num_slice_groups_minus1;
	result.slice_group_map_type = pic->pps->slice_group_map_type;
	result.slice_group_change_rate_minus1 = pic->pps->slice_group_change_rate_minus1;
	result.pic_init_qp_minus26 = pic->pps->pic_init_qp_minus26;
	result.chroma_qp_index_offset = pic->pps->chroma_qp_index_offset;
	result.second_chroma_qp_index_offset = pic->pps->second_chroma_qp_index_offset;

	memcpy(result.scaling_list_4x4, pic->pps->ScalingList4x4, 6*16);
	memcpy(result.scaling_list_8x8, pic->pps->ScalingList8x8, 2*64);

	if (dec->stream_type == RUVD_CODEC_H264_PERF) {
		memcpy(dec->it, result.scaling_list_4x4, 6*16);
		memcpy((dec->it + 96), result.scaling_list_8x8, 2*64);
	}

	result.num_ref_frames = pic->num_ref_frames;

	result.num_ref_idx_l0_active_minus1 = pic->num_ref_idx_l0_active_minus1;
	result.num_ref_idx_l1_active_minus1 = pic->num_ref_idx_l1_active_minus1;

	result.frame_num = pic->frame_num;
	memcpy(result.frame_num_list, pic->frame_num_list, 4*16);
	result.curr_field_order_cnt_list[0] = pic->field_order_cnt[0];
	result.curr_field_order_cnt_list[1] = pic->field_order_cnt[1];
	memcpy(result.field_order_cnt_list, pic->field_order_cnt_list, 4*16*2);

	result.decoded_pic_idx = pic->frame_num;

	return result;
}

/* get h265 specific message bits */
static struct ruvd_h265 get_h265_msg(struct ruvd_decoder *dec, struct pipe_video_buffer *target,
				     struct pipe_h265_picture_desc *pic)
{
	struct ruvd_h265 result;
	unsigned i, j;

	memset(&result, 0, sizeof(result));

	result.sps_info_flags = 0;
	result.sps_info_flags |= pic->pps->sps->scaling_list_enabled_flag << 0;
	result.sps_info_flags |= pic->pps->sps->amp_enabled_flag << 1;
	result.sps_info_flags |= pic->pps->sps->sample_adaptive_offset_enabled_flag << 2;
	result.sps_info_flags |= pic->pps->sps->pcm_enabled_flag << 3;
	result.sps_info_flags |= pic->pps->sps->pcm_loop_filter_disabled_flag << 4;
	result.sps_info_flags |= pic->pps->sps->long_term_ref_pics_present_flag << 5;
	result.sps_info_flags |= pic->pps->sps->sps_temporal_mvp_enabled_flag << 6;
	result.sps_info_flags |= pic->pps->sps->strong_intra_smoothing_enabled_flag << 7;
	result.sps_info_flags |= pic->pps->sps->separate_colour_plane_flag << 8;
	if (((struct r600_common_screen*)dec->screen)->family == CHIP_CARRIZO)
		result.sps_info_flags |= 1 << 9;
	if (pic->UseRefPicList == true)
		result.sps_info_flags |= 1 << 10;

	result.chroma_format = pic->pps->sps->chroma_format_idc;
	result.bit_depth_luma_minus8 = pic->pps->sps->bit_depth_luma_minus8;
	result.bit_depth_chroma_minus8 = pic->pps->sps->bit_depth_chroma_minus8;
	result.log2_max_pic_order_cnt_lsb_minus4 = pic->pps->sps->log2_max_pic_order_cnt_lsb_minus4;
	result.sps_max_dec_pic_buffering_minus1 = pic->pps->sps->sps_max_dec_pic_buffering_minus1;
	result.log2_min_luma_coding_block_size_minus3 = pic->pps->sps->log2_min_luma_coding_block_size_minus3;
	result.log2_diff_max_min_luma_coding_block_size = pic->pps->sps->log2_diff_max_min_luma_coding_block_size;
	result.log2_min_transform_block_size_minus2 = pic->pps->sps->log2_min_transform_block_size_minus2;
	result.log2_diff_max_min_transform_block_size = pic->pps->sps->log2_diff_max_min_transform_block_size;
	result.max_transform_hierarchy_depth_inter = pic->pps->sps->max_transform_hierarchy_depth_inter;
	result.max_transform_hierarchy_depth_intra = pic->pps->sps->max_transform_hierarchy_depth_intra;
	result.pcm_sample_bit_depth_luma_minus1 = pic->pps->sps->pcm_sample_bit_depth_luma_minus1;
	result.pcm_sample_bit_depth_chroma_minus1 = pic->pps->sps->pcm_sample_bit_depth_chroma_minus1;
	result.log2_min_pcm_luma_coding_block_size_minus3 = pic->pps->sps->log2_min_pcm_luma_coding_block_size_minus3;
	result.log2_diff_max_min_pcm_luma_coding_block_size = pic->pps->sps->log2_diff_max_min_pcm_luma_coding_block_size;
	result.num_short_term_ref_pic_sets = pic->pps->sps->num_short_term_ref_pic_sets;

	result.pps_info_flags = 0;
	result.pps_info_flags |= pic->pps->dependent_slice_segments_enabled_flag << 0;
	result.pps_info_flags |= pic->pps->output_flag_present_flag << 1;
	result.pps_info_flags |= pic->pps->sign_data_hiding_enabled_flag << 2;
	result.pps_info_flags |= pic->pps->cabac_init_present_flag << 3;
	result.pps_info_flags |= pic->pps->constrained_intra_pred_flag << 4;
	result.pps_info_flags |= pic->pps->transform_skip_enabled_flag << 5;
	result.pps_info_flags |= pic->pps->cu_qp_delta_enabled_flag << 6;
	result.pps_info_flags |= pic->pps->pps_slice_chroma_qp_offsets_present_flag << 7;
	result.pps_info_flags |= pic->pps->weighted_pred_flag << 8;
	result.pps_info_flags |= pic->pps->weighted_bipred_flag << 9;
	result.pps_info_flags |= pic->pps->transquant_bypass_enabled_flag << 10;
	result.pps_info_flags |= pic->pps->tiles_enabled_flag << 11;
	result.pps_info_flags |= pic->pps->entropy_coding_sync_enabled_flag << 12;
	result.pps_info_flags |= pic->pps->uniform_spacing_flag << 13;
	result.pps_info_flags |= pic->pps->loop_filter_across_tiles_enabled_flag << 14;
	result.pps_info_flags |= pic->pps->pps_loop_filter_across_slices_enabled_flag << 15;
	result.pps_info_flags |= pic->pps->deblocking_filter_override_enabled_flag << 16;
	result.pps_info_flags |= pic->pps->pps_deblocking_filter_disabled_flag << 17;
	result.pps_info_flags |= pic->pps->lists_modification_present_flag << 18;
	result.pps_info_flags |= pic->pps->slice_segment_header_extension_present_flag << 19;
	//result.pps_info_flags |= pic->pps->deblocking_filter_control_present_flag; ???

	result.num_extra_slice_header_bits = pic->pps->num_extra_slice_header_bits;
	result.num_long_term_ref_pic_sps = pic->pps->sps->num_long_term_ref_pics_sps;
	result.num_ref_idx_l0_default_active_minus1 = pic->pps->num_ref_idx_l0_default_active_minus1;
	result.num_ref_idx_l1_default_active_minus1 = pic->pps->num_ref_idx_l1_default_active_minus1;
	result.pps_cb_qp_offset = pic->pps->pps_cb_qp_offset;
	result.pps_cr_qp_offset = pic->pps->pps_cr_qp_offset;
	result.pps_beta_offset_div2 = pic->pps->pps_beta_offset_div2;
	result.pps_tc_offset_div2 = pic->pps->pps_tc_offset_div2;
	result.diff_cu_qp_delta_depth = pic->pps->diff_cu_qp_delta_depth;
	result.num_tile_columns_minus1 = pic->pps->num_tile_columns_minus1;
	result.num_tile_rows_minus1 = pic->pps->num_tile_rows_minus1;
	result.log2_parallel_merge_level_minus2 = pic->pps->log2_parallel_merge_level_minus2;
	result.init_qp_minus26 = pic->pps->init_qp_minus26;

	for (i = 0; i < 19; ++i)
		result.column_width_minus1[i] = pic->pps->column_width_minus1[i];

	for (i = 0; i < 21; ++i)
		result.row_height_minus1[i] = pic->pps->row_height_minus1[i];

	result.num_delta_pocs_ref_rps_idx = pic->NumDeltaPocsOfRefRpsIdx;
	result.curr_poc = pic->CurrPicOrderCntVal;

	for (i = 0 ; i < 16 ; i++) {
		for (j = 0; (pic->ref[j] != NULL) && (j < 16) ; j++) {
			if (dec->render_pic_list[i] == pic->ref[j])
				break;
			if (j == 15)
				dec->render_pic_list[i] = NULL;
			else if (pic->ref[j+1] == NULL)
				dec->render_pic_list[i] = NULL;
		}
	}
	for (i = 0 ; i < 16 ; i++) {
		if (dec->render_pic_list[i] == NULL) {
			dec->render_pic_list[i] = target;
			result.curr_idx = i;
			break;
		}
	}

	vl_video_buffer_set_associated_data(target, &dec->base,
					    (void *)(uintptr_t)result.curr_idx,
					    &ruvd_destroy_associated_data);

	for (i = 0; i < 16; ++i) {
		struct pipe_video_buffer *ref = pic->ref[i];
		uintptr_t ref_pic = 0;

		result.poc_list[i] = pic->PicOrderCntVal[i];

		if (ref)
			ref_pic = (uintptr_t)vl_video_buffer_get_associated_data(ref, &dec->base);
		else
			ref_pic = 0x7F;
		result.ref_pic_list[i] = ref_pic;
	}

	for (i = 0; i < 8; ++i) {
		result.ref_pic_set_st_curr_before[i] = 0xFF;
		result.ref_pic_set_st_curr_after[i] = 0xFF;
		result.ref_pic_set_lt_curr[i] = 0xFF;
	}

	for (i = 0; i < pic->NumPocStCurrBefore; ++i)
		result.ref_pic_set_st_curr_before[i] = pic->RefPicSetStCurrBefore[i];

	for (i = 0; i < pic->NumPocStCurrAfter; ++i)
		result.ref_pic_set_st_curr_after[i] = pic->RefPicSetStCurrAfter[i];

	for (i = 0; i < pic->NumPocLtCurr; ++i)
		result.ref_pic_set_lt_curr[i] = pic->RefPicSetLtCurr[i];

	for (i = 0; i < 6; ++i)
		result.ucScalingListDCCoefSizeID2[i] = pic->pps->sps->ScalingListDCCoeff16x16[i];

	for (i = 0; i < 2; ++i)
		result.ucScalingListDCCoefSizeID3[i] = pic->pps->sps->ScalingListDCCoeff32x32[i];

	memcpy(dec->it, pic->pps->sps->ScalingList4x4, 6 * 16);
	memcpy(dec->it + 96, pic->pps->sps->ScalingList8x8, 6 * 64);
	memcpy(dec->it + 480, pic->pps->sps->ScalingList16x16, 6 * 64);
	memcpy(dec->it + 864, pic->pps->sps->ScalingList32x32, 2 * 64);

	for (i = 0 ; i < 2 ; i++) {
		for (j = 0 ; j < 15 ; j++)
			result.direct_reflist[i][j] = pic->RefPicList[i][j];
	}

	if (pic->base.profile == PIPE_VIDEO_PROFILE_HEVC_MAIN_10) {
		if (target->buffer_format == PIPE_FORMAT_P016) {
			result.p010_mode = 1;
			result.msb_mode = 1;
		} else {
			result.luma_10to8 = 5;
			result.chroma_10to8 = 5;
			result.sclr_luma10to8 = 4;
			result.sclr_chroma10to8 = 4;
		}
	}

	/* TODO
	result.highestTid;
	result.isNonRef;

	IDRPicFlag;
	RAPPicFlag;
	NumPocTotalCurr;
	NumShortTermPictureSliceHeaderBits;
	NumLongTermPictureSliceHeaderBits;

	IsLongTerm[16];
	*/

	return result;
}

/* get vc1 specific message bits */
static struct ruvd_vc1 get_vc1_msg(struct pipe_vc1_picture_desc *pic)
{
	struct ruvd_vc1 result;

	memset(&result, 0, sizeof(result));

	switch(pic->base.profile) {
	case PIPE_VIDEO_PROFILE_VC1_SIMPLE:
		result.profile = RUVD_VC1_PROFILE_SIMPLE;
		result.level = 1;
		break;

	case PIPE_VIDEO_PROFILE_VC1_MAIN:
		result.profile = RUVD_VC1_PROFILE_MAIN;
		result.level = 2;
		break;

	case PIPE_VIDEO_PROFILE_VC1_ADVANCED:
		result.profile = RUVD_VC1_PROFILE_ADVANCED;
		result.level = 4;
		break;

	default:
		assert(0);
	}

	/* fields common for all profiles */
	result.sps_info_flags |= pic->postprocflag << 7;
	result.sps_info_flags |= pic->pulldown << 6;
	result.sps_info_flags |= pic->interlace << 5;
	result.sps_info_flags |= pic->tfcntrflag << 4;
	result.sps_info_flags |= pic->finterpflag << 3;
	result.sps_info_flags |= pic->psf << 1;

	result.pps_info_flags |= pic->range_mapy_flag << 31;
	result.pps_info_flags |= pic->range_mapy << 28;
	result.pps_info_flags |= pic->range_mapuv_flag << 27;
	result.pps_info_flags |= pic->range_mapuv << 24;
	result.pps_info_flags |= pic->multires << 21;
	result.pps_info_flags |= pic->maxbframes << 16;
	result.pps_info_flags |= pic->overlap << 11;
	result.pps_info_flags |= pic->quantizer << 9;
	result.pps_info_flags |= pic->panscan_flag << 7;
	result.pps_info_flags |= pic->refdist_flag << 6;
	result.pps_info_flags |= pic->vstransform << 0;

	/* some fields only apply to main/advanced profile */
	if (pic->base.profile != PIPE_VIDEO_PROFILE_VC1_SIMPLE) {
		result.pps_info_flags |= pic->syncmarker << 20;
		result.pps_info_flags |= pic->rangered << 19;
		result.pps_info_flags |= pic->loopfilter << 5;
		result.pps_info_flags |= pic->fastuvmc << 4;
		result.pps_info_flags |= pic->extended_mv << 3;
		result.pps_info_flags |= pic->extended_dmv << 8;
		result.pps_info_flags |= pic->dquant << 1;
	}

	result.chroma_format = 1;

#if 0
//(((unsigned int)(pPicParams->advance.reserved1))        << SPS_INFO_VC1_RESERVED_SHIFT)
uint32_t 	slice_count
uint8_t 	picture_type
uint8_t 	frame_coding_mode
uint8_t 	deblockEnable
uint8_t 	pquant
#endif

	return result;
}

/* extract the frame number from a referenced video buffer */
static uint32_t get_ref_pic_idx(struct ruvd_decoder *dec, struct pipe_video_buffer *ref)
{
	uint32_t min = MAX2(dec->frame_number, NUM_MPEG2_REFS) - NUM_MPEG2_REFS;
	uint32_t max = MAX2(dec->frame_number, 1) - 1;
	uintptr_t frame;

	/* seems to be the most sane fallback */
	if (!ref)
		return max;

	/* get the frame number from the associated data */
	frame = (uintptr_t)vl_video_buffer_get_associated_data(ref, &dec->base);

	/* limit the frame number to a valid range */
	return MAX2(MIN2(frame, max), min);
}

/* get mpeg2 specific msg bits */
static struct ruvd_mpeg2 get_mpeg2_msg(struct ruvd_decoder *dec,
				       struct pipe_mpeg12_picture_desc *pic)
{
	const int *zscan = pic->alternate_scan ? vl_zscan_alternate : vl_zscan_normal;
	struct ruvd_mpeg2 result;
	unsigned i;

	memset(&result, 0, sizeof(result));
	result.decoded_pic_idx = dec->frame_number;
	for (i = 0; i < 2; ++i)
		result.ref_pic_idx[i] = get_ref_pic_idx(dec, pic->ref[i]);

	if(pic->intra_matrix) {
		result.load_intra_quantiser_matrix = 1;
		for (i = 0; i < 64; ++i) {
			result.intra_quantiser_matrix[i] = pic->intra_matrix[zscan[i]];
		}
	}
	if(pic->non_intra_matrix) {
		result.load_nonintra_quantiser_matrix = 1;
		for (i = 0; i < 64; ++i) {
			result.nonintra_quantiser_matrix[i] = pic->non_intra_matrix[zscan[i]];
		}
	}

	result.profile_and_level_indication = 0;
	result.chroma_format = 0x1;

	result.picture_coding_type = pic->picture_coding_type;
	result.f_code[0][0] = pic->f_code[0][0] + 1;
	result.f_code[0][1] = pic->f_code[0][1] + 1;
	result.f_code[1][0] = pic->f_code[1][0] + 1;
	result.f_code[1][1] = pic->f_code[1][1] + 1;
	result.intra_dc_precision = pic->intra_dc_precision;
	result.pic_structure = pic->picture_structure;
	result.top_field_first = pic->top_field_first;
	result.frame_pred_frame_dct = pic->frame_pred_frame_dct;
	result.concealment_motion_vectors = pic->concealment_motion_vectors;
	result.q_scale_type = pic->q_scale_type;
	result.intra_vlc_format = pic->intra_vlc_format;
	result.alternate_scan = pic->alternate_scan;

	return result;
}

/* get mpeg4 specific msg bits */
static struct ruvd_mpeg4 get_mpeg4_msg(struct ruvd_decoder *dec,
				       struct pipe_mpeg4_picture_desc *pic)
{
	struct ruvd_mpeg4 result;
	unsigned i;

	memset(&result, 0, sizeof(result));
	result.decoded_pic_idx = dec->frame_number;
	for (i = 0; i < 2; ++i)
		result.ref_pic_idx[i] = get_ref_pic_idx(dec, pic->ref[i]);

	result.variant_type = 0;
	result.profile_and_level_indication = 0xF0; // ASP Level0

	result.video_object_layer_verid = 0x5; // advanced simple
	result.video_object_layer_shape = 0x0; // rectangular

	result.video_object_layer_width = dec->base.width;
	result.video_object_layer_height = dec->base.height;

	result.vop_time_increment_resolution = pic->vop_time_increment_resolution;

	result.flags |= pic->short_video_header << 0;
	//result.flags |= obmc_disable << 1;
	result.flags |= pic->interlaced << 2;
        result.flags |= 1 << 3; // load_intra_quant_mat
	result.flags |= 1 << 4; // load_nonintra_quant_mat
	result.flags |= pic->quarter_sample << 5;
	result.flags |= 1 << 6; // complexity_estimation_disable
	result.flags |= pic->resync_marker_disable << 7;
	//result.flags |= data_partitioned << 8;
	//result.flags |= reversible_vlc << 9;
	result.flags |= 0 << 10; // newpred_enable
	result.flags |= 0 << 11; // reduced_resolution_vop_enable
	//result.flags |= scalability << 12;
	//result.flags |= is_object_layer_identifier << 13;
	//result.flags |= fixed_vop_rate << 14;
	//result.flags |= newpred_segment_type << 15;

	result.quant_type = pic->quant_type;

	for (i = 0; i < 64; ++i) {
		result.intra_quant_mat[i] = pic->intra_matrix[vl_zscan_normal[i]];
		result.nonintra_quant_mat[i] = pic->non_intra_matrix[vl_zscan_normal[i]];
	}

	/*
	int32_t 	trd [2]
	int32_t 	trb [2]
	uint8_t 	vop_coding_type
	uint8_t 	vop_fcode_forward
	uint8_t 	vop_fcode_backward
	uint8_t 	rounding_control
	uint8_t 	alternate_vertical_scan_flag
	uint8_t 	top_field_first
	*/

	return result;
}

static void get_mjpeg_slice_header(struct ruvd_decoder *dec, struct pipe_mjpeg_picture_desc *pic)
{
	int size = 0, saved_size, len_pos, i;
	uint16_t *bs;
	uint8_t *buf = dec->bs_ptr;

	/* SOI */
	buf[size++] = 0xff;
	buf[size++] = 0xd8;

	/* DQT */
	buf[size++] = 0xff;
	buf[size++] = 0xdb;

	len_pos = size++;
	size++;

	for (i = 0; i < 4; ++i) {
		if (pic->quantization_table.load_quantiser_table[i] == 0)
			continue;

		buf[size++] = i;
		memcpy((buf + size), &pic->quantization_table.quantiser_table[i], 64);
		size += 64;
	}

	bs = (uint16_t*)&buf[len_pos];
	*bs = util_bswap16(size - 4);

	saved_size = size;

	/* DHT */
	buf[size++] = 0xff;
	buf[size++] = 0xc4;

	len_pos = size++;
	size++;

	for (i = 0; i < 2; ++i) {
		if (pic->huffman_table.load_huffman_table[i] == 0)
			continue;

		buf[size++] = 0x00 | i;
		memcpy((buf + size), &pic->huffman_table.table[i].num_dc_codes, 16);
		size += 16;
		memcpy((buf + size), &pic->huffman_table.table[i].dc_values, 12);
		size += 12;
	}

	for (i = 0; i < 2; ++i) {
		if (pic->huffman_table.load_huffman_table[i] == 0)
			continue;

		buf[size++] = 0x10 | i;
		memcpy((buf + size), &pic->huffman_table.table[i].num_ac_codes, 16);
		size += 16;
		memcpy((buf + size), &pic->huffman_table.table[i].ac_values, 162);
		size += 162;
	}

	bs = (uint16_t*)&buf[len_pos];
	*bs = util_bswap16(size - saved_size - 2);

	saved_size = size;

	/* DRI */
	if (pic->slice_parameter.restart_interval) {
		buf[size++] = 0xff;
		buf[size++] = 0xdd;
		buf[size++] = 0x00;
		buf[size++] = 0x04;
		bs = (uint16_t*)&buf[size++];
		*bs = util_bswap16(pic->slice_parameter.restart_interval);
		saved_size = ++size;
	}

	/* SOF */
	buf[size++] = 0xff;
	buf[size++] = 0xc0;

	len_pos = size++;
	size++;

	buf[size++] = 0x08;

	bs = (uint16_t*)&buf[size++];
	*bs = util_bswap16(pic->picture_parameter.picture_height);
	size++;

	bs = (uint16_t*)&buf[size++];
	*bs = util_bswap16(pic->picture_parameter.picture_width);
	size++;

	buf[size++] = pic->picture_parameter.num_components;

	for (i = 0; i < pic->picture_parameter.num_components; ++i) {
		buf[size++] = pic->picture_parameter.components[i].component_id;
		buf[size++] = pic->picture_parameter.components[i].h_sampling_factor << 4 |
			pic->picture_parameter.components[i].v_sampling_factor;
		buf[size++] = pic->picture_parameter.components[i].quantiser_table_selector;
	}

	bs = (uint16_t*)&buf[len_pos];
	*bs = util_bswap16(size - saved_size - 2);

	saved_size = size;

	/* SOS */
	buf[size++] = 0xff;
	buf[size++] = 0xda;

	len_pos = size++;
	size++;

	buf[size++] = pic->slice_parameter.num_components;

	for (i = 0; i < pic->slice_parameter.num_components; ++i) {
		buf[size++] = pic->slice_parameter.components[i].component_selector;
		buf[size++] = pic->slice_parameter.components[i].dc_table_selector << 4 |
			pic->slice_parameter.components[i].ac_table_selector;
	}

	buf[size++] = 0x00;
	buf[size++] = 0x3f;
	buf[size++] = 0x00;

	bs = (uint16_t*)&buf[len_pos];
	*bs = util_bswap16(size - saved_size - 2);

	dec->bs_ptr += size;
	dec->bs_size += size;
}

/**
 * destroy this video decoder
 */
static void ruvd_destroy(struct pipe_video_codec *decoder)
{
	struct ruvd_decoder *dec = (struct ruvd_decoder*)decoder;
	unsigned i;

	assert(decoder);

	map_msg_fb_it_buf(dec);
	dec->msg->size = sizeof(*dec->msg);
	dec->msg->msg_type = RUVD_MSG_DESTROY;
	dec->msg->stream_handle = dec->stream_handle;
	send_msg_buf(dec);

	flush(dec, 0);

	dec->ws->cs_destroy(dec->cs);

	for (i = 0; i < NUM_BUFFERS; ++i) {
		si_vid_destroy_buffer(&dec->msg_fb_it_buffers[i]);
		si_vid_destroy_buffer(&dec->bs_buffers[i]);
	}

	si_vid_destroy_buffer(&dec->dpb);
	si_vid_destroy_buffer(&dec->ctx);
	si_vid_destroy_buffer(&dec->sessionctx);

	FREE(dec);
}

/**
 * start decoding of a new frame
 */
static void ruvd_begin_frame(struct pipe_video_codec *decoder,
			     struct pipe_video_buffer *target,
			     struct pipe_picture_desc *picture)
{
	struct ruvd_decoder *dec = (struct ruvd_decoder*)decoder;
	uintptr_t frame;

	assert(decoder);

	frame = ++dec->frame_number;
	vl_video_buffer_set_associated_data(target, decoder, (void *)frame,
					    &ruvd_destroy_associated_data);

	dec->bs_size = 0;
	dec->bs_ptr = dec->ws->buffer_map(
		dec->bs_buffers[dec->cur_buffer].res->buf,
		dec->cs, PIPE_TRANSFER_WRITE);
}

/**
 * decode a macroblock
 */
static void ruvd_decode_macroblock(struct pipe_video_codec *decoder,
				   struct pipe_video_buffer *target,
				   struct pipe_picture_desc *picture,
				   const struct pipe_macroblock *macroblocks,
				   unsigned num_macroblocks)
{
	/* not supported (yet) */
	assert(0);
}

/**
 * decode a bitstream
 */
static void ruvd_decode_bitstream(struct pipe_video_codec *decoder,
				  struct pipe_video_buffer *target,
				  struct pipe_picture_desc *picture,
				  unsigned num_buffers,
				  const void * const *buffers,
				  const unsigned *sizes)
{
	struct ruvd_decoder *dec = (struct ruvd_decoder*)decoder;
	enum pipe_video_format format = u_reduce_video_profile(picture->profile);
	unsigned i;

	assert(decoder);

	if (!dec->bs_ptr)
		return;

	if (format == PIPE_VIDEO_FORMAT_JPEG)
		get_mjpeg_slice_header(dec, (struct pipe_mjpeg_picture_desc*)picture);

	for (i = 0; i < num_buffers; ++i) {
		struct rvid_buffer *buf = &dec->bs_buffers[dec->cur_buffer];
		unsigned new_size = dec->bs_size + sizes[i];

		if (format == PIPE_VIDEO_FORMAT_JPEG)
			new_size += 2; /* save for EOI */

		if (new_size > buf->res->buf->size) {
			dec->ws->buffer_unmap(buf->res->buf);
			if (!si_vid_resize_buffer(dec->screen, dec->cs, buf, new_size)) {
				RVID_ERR("Can't resize bitstream buffer!");
				return;
			}

			dec->bs_ptr = dec->ws->buffer_map(buf->res->buf, dec->cs,
							  PIPE_TRANSFER_WRITE);
			if (!dec->bs_ptr)
				return;

			dec->bs_ptr += dec->bs_size;
		}

		memcpy(dec->bs_ptr, buffers[i], sizes[i]);
		dec->bs_size += sizes[i];
		dec->bs_ptr += sizes[i];
	}

	if (format == PIPE_VIDEO_FORMAT_JPEG) {
		((uint8_t *)dec->bs_ptr)[0] = 0xff;	/* EOI */
		((uint8_t *)dec->bs_ptr)[1] = 0xd9;
		dec->bs_size += 2;
		dec->bs_ptr += 2;
	}
}

/**
 * end decoding of the current frame
 */
static void ruvd_end_frame(struct pipe_video_codec *decoder,
			   struct pipe_video_buffer *target,
			   struct pipe_picture_desc *picture)
{
	struct ruvd_decoder *dec = (struct ruvd_decoder*)decoder;
	struct pb_buffer *dt;
	struct rvid_buffer *msg_fb_it_buf, *bs_buf;
	unsigned bs_size;

	assert(decoder);

	if (!dec->bs_ptr)
		return;

	msg_fb_it_buf = &dec->msg_fb_it_buffers[dec->cur_buffer];
	bs_buf = &dec->bs_buffers[dec->cur_buffer];

	bs_size = align(dec->bs_size, 128);
	memset(dec->bs_ptr, 0, bs_size - dec->bs_size);
	dec->ws->buffer_unmap(bs_buf->res->buf);

	map_msg_fb_it_buf(dec);
	dec->msg->size = sizeof(*dec->msg);
	dec->msg->msg_type = RUVD_MSG_DECODE;
	dec->msg->stream_handle = dec->stream_handle;
	dec->msg->status_report_feedback_number = dec->frame_number;

	dec->msg->body.decode.stream_type = dec->stream_type;
	dec->msg->body.decode.decode_flags = 0x1;
	dec->msg->body.decode.width_in_samples = dec->base.width;
	dec->msg->body.decode.height_in_samples = dec->base.height;

	if ((picture->profile == PIPE_VIDEO_PROFILE_VC1_SIMPLE) ||
	    (picture->profile == PIPE_VIDEO_PROFILE_VC1_MAIN)) {
		dec->msg->body.decode.width_in_samples = align(dec->msg->body.decode.width_in_samples, 16) / 16;
		dec->msg->body.decode.height_in_samples = align(dec->msg->body.decode.height_in_samples, 16) / 16;
	}

	if (dec->dpb.res)
		dec->msg->body.decode.dpb_size = dec->dpb.res->buf->size;
	dec->msg->body.decode.bsd_size = bs_size;
	dec->msg->body.decode.db_pitch = align(dec->base.width, get_db_pitch_alignment(dec));

	if (dec->stream_type == RUVD_CODEC_H264_PERF &&
	    ((struct r600_common_screen*)dec->screen)->family >= CHIP_POLARIS10)
		dec->msg->body.decode.dpb_reserved = dec->ctx.res->buf->size;

	dt = dec->set_dtb(dec->msg, (struct vl_video_buffer *)target);
	if (((struct r600_common_screen*)dec->screen)->family >= CHIP_STONEY)
		dec->msg->body.decode.dt_wa_chroma_top_offset = dec->msg->body.decode.dt_pitch / 2;

	switch (u_reduce_video_profile(picture->profile)) {
	case PIPE_VIDEO_FORMAT_MPEG4_AVC:
		dec->msg->body.decode.codec.h264 = get_h264_msg(dec, (struct pipe_h264_picture_desc*)picture);
		break;

	case PIPE_VIDEO_FORMAT_HEVC:
		dec->msg->body.decode.codec.h265 = get_h265_msg(dec, target, (struct pipe_h265_picture_desc*)picture);
		if (dec->ctx.res == NULL) {
			unsigned ctx_size;
			if (dec->base.profile == PIPE_VIDEO_PROFILE_HEVC_MAIN_10)
				ctx_size = calc_ctx_size_h265_main10(dec, (struct pipe_h265_picture_desc*)picture);
			else
				ctx_size = calc_ctx_size_h265_main(dec);
			if (!si_vid_create_buffer(dec->screen, &dec->ctx, ctx_size, PIPE_USAGE_DEFAULT)) {
				RVID_ERR("Can't allocated context buffer.\n");
			}
			si_vid_clear_buffer(decoder->context, &dec->ctx);
		}

		if (dec->ctx.res)
			dec->msg->body.decode.dpb_reserved = dec->ctx.res->buf->size;
		break;

	case PIPE_VIDEO_FORMAT_VC1:
		dec->msg->body.decode.codec.vc1 = get_vc1_msg((struct pipe_vc1_picture_desc*)picture);
		break;

	case PIPE_VIDEO_FORMAT_MPEG12:
		dec->msg->body.decode.codec.mpeg2 = get_mpeg2_msg(dec, (struct pipe_mpeg12_picture_desc*)picture);
		break;

	case PIPE_VIDEO_FORMAT_MPEG4:
		dec->msg->body.decode.codec.mpeg4 = get_mpeg4_msg(dec, (struct pipe_mpeg4_picture_desc*)picture);
		break;

	case PIPE_VIDEO_FORMAT_JPEG:
		break;

	default:
		assert(0);
		return;
	}

	dec->msg->body.decode.db_surf_tile_config = dec->msg->body.decode.dt_surf_tile_config;
	dec->msg->body.decode.extension_support = 0x1;

	/* set at least the feedback buffer size */
	dec->fb[0] = dec->fb_size;

	send_msg_buf(dec);

	if (dec->dpb.res)
		send_cmd(dec, RUVD_CMD_DPB_BUFFER, dec->dpb.res->buf, 0,
			RADEON_USAGE_READWRITE, RADEON_DOMAIN_VRAM);

	if (dec->ctx.res)
		send_cmd(dec, RUVD_CMD_CONTEXT_BUFFER, dec->ctx.res->buf, 0,
			RADEON_USAGE_READWRITE, RADEON_DOMAIN_VRAM);
	send_cmd(dec, RUVD_CMD_BITSTREAM_BUFFER, bs_buf->res->buf,
		 0, RADEON_USAGE_READ, RADEON_DOMAIN_GTT);
	send_cmd(dec, RUVD_CMD_DECODING_TARGET_BUFFER, dt, 0,
		 RADEON_USAGE_WRITE, RADEON_DOMAIN_VRAM);
	send_cmd(dec, RUVD_CMD_FEEDBACK_BUFFER, msg_fb_it_buf->res->buf,
		 FB_BUFFER_OFFSET, RADEON_USAGE_WRITE, RADEON_DOMAIN_GTT);
	if (have_it(dec))
		send_cmd(dec, RUVD_CMD_ITSCALING_TABLE_BUFFER, msg_fb_it_buf->res->buf,
			 FB_BUFFER_OFFSET + dec->fb_size, RADEON_USAGE_READ, RADEON_DOMAIN_GTT);
	set_reg(dec, dec->reg.cntl, 1);

	flush(dec, RADEON_FLUSH_ASYNC);
	next_buffer(dec);
}

/**
 * flush any outstanding command buffers to the hardware
 */
static void ruvd_flush(struct pipe_video_codec *decoder)
{
}

/**
 * create and UVD decoder
 */
struct pipe_video_codec *si_common_uvd_create_decoder(struct pipe_context *context,
						      const struct pipe_video_codec *templ,
						      ruvd_set_dtb set_dtb)
{
	struct radeon_winsys* ws = ((struct r600_common_context *)context)->ws;
	struct r600_common_context *rctx = (struct r600_common_context*)context;
	unsigned dpb_size;
	unsigned width = templ->width, height = templ->height;
	unsigned bs_buf_size;
	struct radeon_info info;
	struct ruvd_decoder *dec;
	int r, i;

	ws->query_info(ws, &info);

	switch(u_reduce_video_profile(templ->profile)) {
	case PIPE_VIDEO_FORMAT_MPEG12:
		if (templ->entrypoint > PIPE_VIDEO_ENTRYPOINT_BITSTREAM)
			return vl_create_mpeg12_decoder(context, templ);

		/* fall through */
	case PIPE_VIDEO_FORMAT_MPEG4:
		width = align(width, VL_MACROBLOCK_WIDTH);
		height = align(height, VL_MACROBLOCK_HEIGHT);
		break;
	case PIPE_VIDEO_FORMAT_MPEG4_AVC:
		width = align(width, VL_MACROBLOCK_WIDTH);
		height = align(height, VL_MACROBLOCK_HEIGHT);
		break;

	default:
		break;
	}


	dec = CALLOC_STRUCT(ruvd_decoder);

	if (!dec)
		return NULL;

	if (info.drm_major < 3)
		dec->use_legacy = true;

	dec->base = *templ;
	dec->base.context = context;
	dec->base.width = width;
	dec->base.height = height;

	dec->base.destroy = ruvd_destroy;
	dec->base.begin_frame = ruvd_begin_frame;
	dec->base.decode_macroblock = ruvd_decode_macroblock;
	dec->base.decode_bitstream = ruvd_decode_bitstream;
	dec->base.end_frame = ruvd_end_frame;
	dec->base.flush = ruvd_flush;

	dec->stream_type = profile2stream_type(dec, info.family);
	dec->set_dtb = set_dtb;
	dec->stream_handle = si_vid_alloc_stream_handle();
	dec->screen = context->screen;
	dec->ws = ws;
	dec->cs = ws->cs_create(rctx->ctx, RING_UVD, NULL, NULL);
	if (!dec->cs) {
		RVID_ERR("Can't get command submission context.\n");
		goto error;
	}

	for (i = 0; i < 16; i++)
		 dec->render_pic_list[i] = NULL;
	dec->fb_size = (info.family == CHIP_TONGA) ? FB_BUFFER_SIZE_TONGA :
			FB_BUFFER_SIZE;
	bs_buf_size = width * height * (512 / (16 * 16));
	for (i = 0; i < NUM_BUFFERS; ++i) {
		unsigned msg_fb_it_size = FB_BUFFER_OFFSET + dec->fb_size;
		STATIC_ASSERT(sizeof(struct ruvd_msg) <= FB_BUFFER_OFFSET);
		if (have_it(dec))
			msg_fb_it_size += IT_SCALING_TABLE_SIZE;
		if (!si_vid_create_buffer(dec->screen, &dec->msg_fb_it_buffers[i],
					msg_fb_it_size, PIPE_USAGE_STAGING)) {
			RVID_ERR("Can't allocated message buffers.\n");
			goto error;
		}

		if (!si_vid_create_buffer(dec->screen, &dec->bs_buffers[i],
					bs_buf_size, PIPE_USAGE_STAGING)) {
			RVID_ERR("Can't allocated bitstream buffers.\n");
			goto error;
		}

		si_vid_clear_buffer(context, &dec->msg_fb_it_buffers[i]);
		si_vid_clear_buffer(context, &dec->bs_buffers[i]);
	}

	dpb_size = calc_dpb_size(dec);
	if (dpb_size) {
		if (!si_vid_create_buffer(dec->screen, &dec->dpb, dpb_size, PIPE_USAGE_DEFAULT)) {
			RVID_ERR("Can't allocated dpb.\n");
			goto error;
		}
		si_vid_clear_buffer(context, &dec->dpb);
	}

	if (dec->stream_type == RUVD_CODEC_H264_PERF && info.family >= CHIP_POLARIS10) {
		unsigned ctx_size = calc_ctx_size_h264_perf(dec);
		if (!si_vid_create_buffer(dec->screen, &dec->ctx, ctx_size, PIPE_USAGE_DEFAULT)) {
			RVID_ERR("Can't allocated context buffer.\n");
			goto error;
		}
		si_vid_clear_buffer(context, &dec->ctx);
	}

	if (info.family >= CHIP_POLARIS10 && info.drm_minor >= 3) {
		if (!si_vid_create_buffer(dec->screen, &dec->sessionctx,
					UVD_SESSION_CONTEXT_SIZE,
					PIPE_USAGE_DEFAULT)) {
			RVID_ERR("Can't allocated session ctx.\n");
			goto error;
		}
		si_vid_clear_buffer(context, &dec->sessionctx);
	}

	if (info.family >= CHIP_VEGA10) {
		dec->reg.data0 = RUVD_GPCOM_VCPU_DATA0_SOC15;
		dec->reg.data1 = RUVD_GPCOM_VCPU_DATA1_SOC15;
		dec->reg.cmd = RUVD_GPCOM_VCPU_CMD_SOC15;
		dec->reg.cntl = RUVD_ENGINE_CNTL_SOC15;
	} else {
		dec->reg.data0 = RUVD_GPCOM_VCPU_DATA0;
		dec->reg.data1 = RUVD_GPCOM_VCPU_DATA1;
		dec->reg.cmd = RUVD_GPCOM_VCPU_CMD;
		dec->reg.cntl = RUVD_ENGINE_CNTL;
	}

	map_msg_fb_it_buf(dec);
	dec->msg->size = sizeof(*dec->msg);
	dec->msg->msg_type = RUVD_MSG_CREATE;
	dec->msg->stream_handle = dec->stream_handle;
	dec->msg->body.create.stream_type = dec->stream_type;
	dec->msg->body.create.width_in_samples = dec->base.width;
	dec->msg->body.create.height_in_samples = dec->base.height;
	dec->msg->body.create.dpb_size = dpb_size;
	send_msg_buf(dec);
	r = flush(dec, 0);
	if (r)
		goto error;

	next_buffer(dec);

	return &dec->base;

error:
	if (dec->cs) dec->ws->cs_destroy(dec->cs);

	for (i = 0; i < NUM_BUFFERS; ++i) {
		si_vid_destroy_buffer(&dec->msg_fb_it_buffers[i]);
		si_vid_destroy_buffer(&dec->bs_buffers[i]);
	}

	si_vid_destroy_buffer(&dec->dpb);
	si_vid_destroy_buffer(&dec->ctx);
	si_vid_destroy_buffer(&dec->sessionctx);

	FREE(dec);

	return NULL;
}

/* calculate top/bottom offset */
static unsigned texture_offset(struct radeon_surf *surface, unsigned layer,
				enum ruvd_surface_type type)
{
	switch (type) {
	default:
	case RUVD_SURFACE_TYPE_LEGACY:
		return surface->u.legacy.level[0].offset +
			layer * surface->u.legacy.level[0].slice_size;
		break;
	case RUVD_SURFACE_TYPE_GFX9:
		return surface->u.gfx9.surf_offset +
			layer * surface->u.gfx9.surf_slice_size;
		break;
	}
}

/* hw encode the aspect of macro tiles */
static unsigned macro_tile_aspect(unsigned macro_tile_aspect)
{
	switch (macro_tile_aspect) {
	default:
	case 1: macro_tile_aspect = 0;  break;
	case 2: macro_tile_aspect = 1;  break;
	case 4: macro_tile_aspect = 2;  break;
	case 8: macro_tile_aspect = 3;  break;
	}
	return macro_tile_aspect;
}

/* hw encode the bank width and height */
static unsigned bank_wh(unsigned bankwh)
{
	switch (bankwh) {
	default:
	case 1: bankwh = 0;     break;
	case 2: bankwh = 1;     break;
	case 4: bankwh = 2;     break;
	case 8: bankwh = 3;     break;
	}
	return bankwh;
}

/**
 * fill decoding target field from the luma and chroma surfaces
 */
void si_uvd_set_dt_surfaces(struct ruvd_msg *msg, struct radeon_surf *luma,
			    struct radeon_surf *chroma, enum ruvd_surface_type type)
{
	switch (type) {
	default:
	case RUVD_SURFACE_TYPE_LEGACY:
		msg->body.decode.dt_pitch = luma->u.legacy.level[0].nblk_x * luma->blk_w;
		switch (luma->u.legacy.level[0].mode) {
		case RADEON_SURF_MODE_LINEAR_ALIGNED:
			msg->body.decode.dt_tiling_mode = RUVD_TILE_LINEAR;
			msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_LINEAR;
			break;
		case RADEON_SURF_MODE_1D:
			msg->body.decode.dt_tiling_mode = RUVD_TILE_8X8;
			msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_1D_THIN;
			break;
		case RADEON_SURF_MODE_2D:
			msg->body.decode.dt_tiling_mode = RUVD_TILE_8X8;
			msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_2D_THIN;
			break;
		default:
			assert(0);
			break;
		}

		msg->body.decode.dt_luma_top_offset = texture_offset(luma, 0, type);
		if (chroma)
			msg->body.decode.dt_chroma_top_offset = texture_offset(chroma, 0, type);
		if (msg->body.decode.dt_field_mode) {
			msg->body.decode.dt_luma_bottom_offset = texture_offset(luma, 1, type);
			if (chroma)
				msg->body.decode.dt_chroma_bottom_offset = texture_offset(chroma, 1, type);
		} else {
			msg->body.decode.dt_luma_bottom_offset = msg->body.decode.dt_luma_top_offset;
			msg->body.decode.dt_chroma_bottom_offset = msg->body.decode.dt_chroma_top_offset;
		}

		if (chroma) {
			assert(luma->u.legacy.bankw == chroma->u.legacy.bankw);
			assert(luma->u.legacy.bankh == chroma->u.legacy.bankh);
			assert(luma->u.legacy.mtilea == chroma->u.legacy.mtilea);
		}

		msg->body.decode.dt_surf_tile_config |= RUVD_BANK_WIDTH(bank_wh(luma->u.legacy.bankw));
		msg->body.decode.dt_surf_tile_config |= RUVD_BANK_HEIGHT(bank_wh(luma->u.legacy.bankh));
		msg->body.decode.dt_surf_tile_config |= RUVD_MACRO_TILE_ASPECT_RATIO(macro_tile_aspect(luma->u.legacy.mtilea));
		break;
	case RUVD_SURFACE_TYPE_GFX9:
		msg->body.decode.dt_pitch = luma->u.gfx9.surf_pitch * luma->blk_w;
		/* SWIZZLE LINEAR MODE */
		msg->body.decode.dt_tiling_mode = RUVD_TILE_LINEAR;
		msg->body.decode.dt_array_mode = RUVD_ARRAY_MODE_LINEAR;
		msg->body.decode.dt_luma_top_offset = texture_offset(luma, 0, type);
		msg->body.decode.dt_chroma_top_offset = texture_offset(chroma, 0, type);
		if (msg->body.decode.dt_field_mode) {
			msg->body.decode.dt_luma_bottom_offset = texture_offset(luma, 1, type);
			msg->body.decode.dt_chroma_bottom_offset = texture_offset(chroma, 1, type);
		} else {
			msg->body.decode.dt_luma_bottom_offset = msg->body.decode.dt_luma_top_offset;
			msg->body.decode.dt_chroma_bottom_offset = msg->body.decode.dt_chroma_top_offset;
		}
		msg->body.decode.dt_surf_tile_config = 0;
		break;
	}
}
