/* $Id$ */
/** @file
 * VBoxDxVk - Video decoder.
 */

/*
 * Copyright (C) 2024-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


#include "dxvk_video_decoder.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkVideoSessionHandle::DxvkVideoSessionHandle(
    const Rc<DxvkDevice>& device)
    : m_device(device) {
  }


  DxvkVideoSessionHandle::~DxvkVideoSessionHandle() {
    m_device->vkd()->vkDestroyVideoSessionKHR(m_device->handle(), m_videoSession, nullptr);
    m_videoSession = VK_NULL_HANDLE;
  }


  void DxvkVideoSessionHandle::create(
    const VkVideoSessionCreateInfoKHR& sessionCreateInfo) {
    VkResult vr = m_device->vkd()->vkCreateVideoSessionKHR(m_device->handle(),
      &sessionCreateInfo, nullptr, &m_videoSession);
    if (vr)
      throw DxvkError(str::format("DxvkVideoSessionHandle: vkCreateVideoSessionKHR failed: ", vr));
  }


  DxvkVideoSessionParametersHandle::DxvkVideoSessionParametersHandle(
    const Rc<DxvkDevice>& device)
    : m_device(device) {
  }


  DxvkVideoSessionParametersHandle::~DxvkVideoSessionParametersHandle() {
    m_device->vkd()->vkDestroyVideoSessionParametersKHR(m_device->handle(), m_videoSessionParameters, nullptr);
    m_videoSessionParameters = VK_NULL_HANDLE;
  }


  void DxvkVideoSessionParametersHandle::create(
    const VkVideoSessionParametersCreateInfoKHR& sessionParametersCreateInfo) {
    VkResult vr = m_device->vkd()->vkCreateVideoSessionParametersKHR(m_device->handle(),
      &sessionParametersCreateInfo, nullptr, &m_videoSessionParameters);
    if (vr)
      throw DxvkError(str::format("DxvkVideoSessionParametersHandle: vkCreateVideoSessionParametersKHR failed: ", vr));
  }


  DxvkVideoDecoder::DxvkVideoDecoder(const Rc<DxvkDevice>& device, DxvkMemoryAllocator& memAlloc,
      const DxvkVideoDecodeProfileInfo& profile, uint32_t sampleWidth, uint32_t sampleHeight, VkFormat outputFormat)
  : m_device(device),
    m_memAlloc(memAlloc),
    m_profile(profile),
    m_sampleWidth(sampleWidth),
    m_sampleHeight(sampleHeight),
    m_outputFormat(outputFormat),
    m_videoSession(new DxvkVideoSessionHandle(device)),
    m_videoSessionParameters(new DxvkVideoSessionParametersHandle(device)) {
    VkResult vr;

    DxvkFenceCreateInfo fenceInfo;
    fenceInfo.initialValue = m_queueOwnershipTransferValue;
    m_queueOwnershipTransferFence = m_device->createFence(fenceInfo);

    /*
     * Update internal pointers of m_profileInfo.
     */
    m_profile.profileInfo.pNext        = &m_profile.h264ProfileInfo;
    m_profile.decodeCapabilities.pNext = &m_profile.decodeH264Capabilities;
    m_profile.videoCapabilities.pNext  = &m_profile.decodeCapabilities;

    /*
     * Assess capabilities.
     */
    /* Check that video resolution is supported. */
    if (m_sampleWidth > m_profile.videoCapabilities.maxCodedExtent.width
     || m_sampleHeight > m_profile.videoCapabilities.maxCodedExtent.height)
      throw DxvkError(str::format("DxvkVideoDecoder: requested resolution exceeds maximum: ",
        m_sampleWidth, "x", m_sampleHeight, " > ",
        m_profile.videoCapabilities.maxCodedExtent.width, "x", m_profile.videoCapabilities.maxCodedExtent.height));

    if ((m_profile.videoCapabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) == 0) {
      /// @todo Allocate one image resource as array for DPB.
      throw DxvkError(str::format("DxvkVideoDecoder: VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR"
        " is not supported"));
    }

    /* Figure out if the decoder uses a DPB slot or a separate image for the output picture.*/
    if (m_profile.decodeCapabilities.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR)
      m_caps.distinctOutputImage = true; /* DPB and Output can be different images. */
    else
      m_caps.distinctOutputImage = false; /* DPB and Output can not be different images. */

    /* Prefer DPB_AND_OUTPUT_COINCIDE, because it does not require an additional output image. */
    if (m_profile.decodeCapabilities.flags & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR)
      m_caps.distinctOutputImage = false; /* DPB and Output can coincide. */
    else
      m_caps.distinctOutputImage = true; /* DPB and Output can not coincide. */

    /*
     * Create resources.
     */
    VkVideoProfileListInfoKHR profileListInfo =
      { VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR };
    profileListInfo.profileCount = 1;
    profileListInfo.pProfiles    = &m_profile.profileInfo;

    /* Source buffer for bitstream data. */
    DxvkBufferCreateInfo info;
    info.pNext  = &profileListInfo;
    info.flags  = 0;
    info.size   = 1024 * 1024; /// @todo Specified by caller?
    info.usage  = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
    info.stages = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
    /* Hack: Access bits are not used as buffer creation parameters,
     * however they are used for checking if the memory must be GPU writable.
     * No access makes it GPU readable.
     */
    info.access = 0;
    m_bitstreamBuffer.buffer = m_device->createBuffer(info, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                                          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                          | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    /* Decoded Picture Buffer (DPB), i.e. array of decoded frames. */
    const uint32_t cMaxDPBSlots = std::max(
      m_profile.videoCapabilities.maxActiveReferencePictures, m_profile.videoCapabilities.maxDpbSlots);
    m_DPB.slots.resize(cMaxDPBSlots);

    for (auto &slot: m_DPB.slots) {
      DxvkImageCreateInfo imgInfo = {};
      /* Do not use VK_BUFFER_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR for DPB images, because
       * "images with only DPB usage remain tied to the video profiles the image was created with,
       * as the data layout of such DPB-only images may be implementation- and codec-dependent."
       * When m_caps.distinctOutputImage is true the DPB images have the "only DPB usage".
       */
      imgInfo.pNext       = &profileListInfo;
      imgInfo.type        = VK_IMAGE_TYPE_2D;
      imgInfo.format      = m_outputFormat;
      imgInfo.flags       = 0;
      imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imgInfo.extent      = { m_sampleWidth, m_sampleHeight, 1 };
      imgInfo.numLayers   = 1;
      imgInfo.mipLevels   = 1;
      if (m_caps.distinctOutputImage) {
        imgInfo.usage       = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
        imgInfo.stages      = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
      }
      else {
        imgInfo.usage       = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
                            | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgInfo.stages      = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR
                            | VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      }

      /* Hack: Access bits are not used as image creation parameters,
       * however they are used for checking if the memory must be GPU writable.
       * Provide flags that make it GPU writable.
       */
      imgInfo.access      = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                          | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      imgInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
      imgInfo.layout      = VK_IMAGE_LAYOUT_UNDEFINED;

      slot.image = m_device->createImage(imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewCreateInfo viewInfo = {};
      viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format     = m_outputFormat;
      viewInfo.usage      = imgInfo.usage;
      viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.minLevel   = 0;
      viewInfo.numLevels  = 1;
      viewInfo.minLayer   = 0;
      viewInfo.numLayers  = 1;

      slot.imageView = m_device->createImageView(slot.image, viewInfo);
    }

    if (m_caps.distinctOutputImage) {
      /* Create an additional output image. */
      DxvkImageCreateInfo imgInfo = {};
      imgInfo.pNext       = &profileListInfo;
      imgInfo.type        = VK_IMAGE_TYPE_2D;
      imgInfo.format      = m_outputFormat;
      imgInfo.flags       = 0;
      imgInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
      imgInfo.extent      = { m_sampleWidth, m_sampleHeight, 1 };
      imgInfo.numLayers   = 1;
      imgInfo.mipLevels   = 1;
      imgInfo.usage       = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                          | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
      imgInfo.stages      = VK_PIPELINE_STAGE_2_TRANSFER_BIT
                          | VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
      /* Hack: Access bits are not used as image creation parameters,
       * however they are used for checking if the memory must be GPU writable.
       * Provide flags that make it GPU writable.
       */
      imgInfo.access      = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                          | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      imgInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
      imgInfo.layout      = VK_IMAGE_LAYOUT_UNDEFINED;

      m_imageDecodeDst = m_device->createImage(imgInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      DxvkImageViewCreateInfo viewInfo = {};
      viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format     = m_outputFormat;
      viewInfo.usage      = imgInfo.usage;
      viewInfo.aspect     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.minLevel   = 0;
      viewInfo.numLevels  = 1;
      viewInfo.minLayer   = 0;
      viewInfo.numLayers  = 1;

      m_imageViewDecodeDst = m_device->createImageView(m_imageDecodeDst, viewInfo);
    }

    /*
     * Create video session object.
     */
    VkVideoSessionCreateInfoKHR sessionCreateInfo =
      { VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR };
    sessionCreateInfo.queueFamilyIndex           = m_device->queues().videoDecode.queueFamily;
    sessionCreateInfo.flags                      = 0;
    sessionCreateInfo.pVideoProfile              = &m_profile.profileInfo;
    sessionCreateInfo.pictureFormat              = m_outputFormat;
    sessionCreateInfo.maxCodedExtent.width       = m_sampleWidth;
    sessionCreateInfo.maxCodedExtent.height      = m_sampleHeight;
    sessionCreateInfo.referencePictureFormat     = m_outputFormat;
    sessionCreateInfo.maxDpbSlots                = m_DPB.slots.size();
    sessionCreateInfo.maxActiveReferencePictures = m_DPB.slots.size() - 1;
    sessionCreateInfo.pStdHeaderVersion          = &m_profile.videoCapabilities.stdHeaderVersion;

    m_videoSession->create(sessionCreateInfo);

    /* Vulkan need an explicit memory allocation for the video session. */
    uint32_t memoryRequirementsCount = 0;
    vr = m_device->vkd()->vkGetVideoSessionMemoryRequirementsKHR(m_device->handle(),
      m_videoSession->handle(), &memoryRequirementsCount, nullptr);
    if (vr)
      throw DxvkError(str::format("DxvkVideoDecoder: vkGetVideoSessionMemoryRequirementsKHR failed: ", vr));

    std::vector<VkVideoSessionMemoryRequirementsKHR> memoryRequirements(memoryRequirementsCount,
      { VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR });
    vr = m_device->vkd()->vkGetVideoSessionMemoryRequirementsKHR(m_device->handle(),
      m_videoSession->handle(), &memoryRequirementsCount, memoryRequirements.data());
    if (vr)
      throw DxvkError(str::format("DxvkVideoDecoder: vkGetVideoSessionMemoryRequirementsKHR failed: ", vr));

    if (memoryRequirementsCount > 0) { /* Intel graphics driver returns 0. */
      m_videoSessionMemory.resize(memoryRequirementsCount);

      std::vector<VkBindVideoSessionMemoryInfoKHR> bindMemoryInfos(memoryRequirementsCount);

      for (uint32_t i = 0; i < memoryRequirementsCount; ++i) {
        const VkVideoSessionMemoryRequirementsKHR &requirement = memoryRequirements[i];

        DxvkMemoryRequirements reqs = { };
        reqs.tiling                  = VK_IMAGE_TILING_LINEAR; /* Plain memory. */
        reqs.core                    = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };
        reqs.core.memoryRequirements = requirement.memoryRequirements;
        DxvkMemoryProperties props = { };
        DxvkMemoryFlags hints;

        m_videoSessionMemory[i] = m_memAlloc.alloc(reqs, props, hints);

        VkBindVideoSessionMemoryInfoKHR &bindMemoryInfo = bindMemoryInfos[i];
        bindMemoryInfo.sType           = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
        bindMemoryInfo.memory          = m_videoSessionMemory[i].memory();
        bindMemoryInfo.memoryOffset    = m_videoSessionMemory[i].offset();
        bindMemoryInfo.memorySize      = m_videoSessionMemory[i].length();
        bindMemoryInfo.memoryBindIndex = requirement.memoryBindIndex;
      }

      vr = m_device->vkd()->vkBindVideoSessionMemoryKHR(m_device->handle(),
        m_videoSession->handle(), memoryRequirementsCount, bindMemoryInfos.data());
      if (vr)
        throw DxvkError(str::format("DxvkVideoDecoder: vkBindVideoSessionMemoryKHR failed: ", vr));
    }

    /*
     * Create video session parameters object.
     */
    VkVideoDecodeH264SessionParametersCreateInfoKHR h264SessionParametersCreateInfo =
      { VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR };
    h264SessionParametersCreateInfo.maxStdSPSCount     = m_parameterSetCache.sps.size();
    h264SessionParametersCreateInfo.maxStdPPSCount     = m_parameterSetCache.pps.size();
    h264SessionParametersCreateInfo.pParametersAddInfo = nullptr; /* Added in 'Decode' as necessary. */

    VkVideoSessionParametersCreateInfoKHR sessionParametersCreateInfo =
      { VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR, &h264SessionParametersCreateInfo };
    sessionParametersCreateInfo.flags                          = 0;
    sessionParametersCreateInfo.videoSessionParametersTemplate = nullptr;
    sessionParametersCreateInfo.videoSession                   = m_videoSession->handle();

    m_videoSessionParameters->create(sessionParametersCreateInfo);
  }


  DxvkVideoDecoder::~DxvkVideoDecoder() {
  }


  void DxvkVideoDecoder::TransferImageQueueOwnership(
    DxvkContext*          ctx,
    const Rc<DxvkImage>&  image,
    uint32_t              baseArrayLayer,
    DxvkCmdBuffer         srcCmdBuffer,
    uint32_t              srcQueueFamilyIndex,
    VkPipelineStageFlags2 srcStageMask,
    VkAccessFlags2        srcAccessMask,
    VkImageLayout         oldLayout,
    DxvkCmdBuffer         dstCmdBuffer,
    uint32_t              dstQueueFamilyIndex,
    VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2        dstAccessMask,
    VkImageLayout         newLayout) {
    std::array<VkImageMemoryBarrier2, 2> barriers{{
      { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 }}};

    /* Source queue release. dstAccessMask is ignored.
     * dstStageMask is ignored, because 'dependencyFlags does not include
     * VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR'.
     */
    barriers[0].srcStageMask        = srcStageMask;
    barriers[0].srcAccessMask       = srcAccessMask;
    barriers[0].dstStageMask        = 0;
    barriers[0].dstAccessMask       = 0;
    barriers[0].oldLayout           = oldLayout;
    barriers[0].newLayout           = newLayout;
    barriers[0].srcQueueFamilyIndex = srcQueueFamilyIndex;
    barriers[0].dstQueueFamilyIndex = dstQueueFamilyIndex;
    barriers[0].image               = image->handle();
    barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel   = 0;
    barriers[0].subresourceRange.levelCount     = 1;
    barriers[0].subresourceRange.baseArrayLayer = baseArrayLayer;
    barriers[0].subresourceRange.layerCount     = 1;

    /* Destination queue acquire. srcAccessMask is ignored.
     * srcStageMask is ignored, because 'dependencyFlags does not include
     * VK_DEPENDENCY_QUEUE_FAMILY_OWNERSHIP_TRANSFER_USE_ALL_STAGES_BIT_KHR'.
     */
    barriers[1] = barriers[0];
    barriers[1].srcStageMask        = 0;
    barriers[1].srcAccessMask       = 0;
    barriers[1].dstStageMask        = dstStageMask;
    barriers[1].dstAccessMask       = dstAccessMask;

    ctx->transferImageQueueOwnership(
      srcCmdBuffer,
      &barriers[0],
      dstCmdBuffer,
      &barriers[1],
      m_queueOwnershipTransferFence,
      ++m_queueOwnershipTransferValue);

    ctx->trackResource(DxvkAccess::Write, image);
  }


  void DxvkVideoDecoder::BeginFrame(
    DxvkContext* ctx,
    const Rc<DxvkImageView>& imageView) {
    Logger::info(str::format("VDec: BeginFrame"));

    m_outputImageView = imageView;

    if (m_profile.videoQueueHasTransfer) {
      /* Acquire ownership of the image to the video queue. */
      this->TransferImageQueueOwnership(ctx, m_outputImageView->image(), m_outputImageView->info().minLayer,
        DxvkCmdBuffer::InitBuffer,
        m_device->queues().graphics.queueFamily,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED, /* 'The contents ... may be discarded.' */
        DxvkCmdBuffer::VDecBuffer,
        m_device->queues().videoDecode.queueFamily,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL); /* A target of CopyImage */
    }
  }


  void DxvkVideoDecoder::EndFrame(
    DxvkContext* ctx) {
    Logger::info(str::format("VDec: EndFrame"));

    if (m_profile.videoQueueHasTransfer) {
      /* Return ownership of the image back to the graphics queue. */
      this->TransferImageQueueOwnership(ctx, m_outputImageView->image(), m_outputImageView->info().minLayer,
        DxvkCmdBuffer::VDecBuffer,
        m_device->queues().videoDecode.queueFamily,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
        VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        DxvkCmdBuffer::InitBuffer,
        m_device->queues().graphics.queueFamily,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, /// ALL_COMMANDS?
        VK_ACCESS_2_MEMORY_READ_BIT,
        m_outputImageView->image()->info().layout); /* VK_IMAGE_LAYOUT_GENERAL. */
    }

    m_outputImageView = nullptr;
  }


#define DXVK_VD_CMP_SPS_FIELDS(_f) \
  if (sps1._f != sps2._f) { \
    Logger::debug(str::format("SPS.", #_f ,": ", sps1._f, " != ", sps2._f)); \
    return false; \
  }
  static bool IsSPSEqual(
    const StdVideoH264SequenceParameterSet &sps1,
    const StdVideoH264SequenceParameterSet &sps2) {
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set0_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set1_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set2_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set3_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set4_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.constraint_set5_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.direct_8x8_inference_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.mb_adaptive_frame_field_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.frame_mbs_only_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.delta_pic_order_always_zero_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.separate_colour_plane_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.gaps_in_frame_num_value_allowed_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.qpprime_y_zero_transform_bypass_flag)
    DXVK_VD_CMP_SPS_FIELDS(flags.frame_cropping_flag)
    DXVK_VD_CMP_SPS_FIELDS(profile_idc)
    DXVK_VD_CMP_SPS_FIELDS(level_idc)
    DXVK_VD_CMP_SPS_FIELDS(chroma_format_idc)
    DXVK_VD_CMP_SPS_FIELDS(bit_depth_luma_minus8)
    DXVK_VD_CMP_SPS_FIELDS(bit_depth_chroma_minus8)
    DXVK_VD_CMP_SPS_FIELDS(log2_max_frame_num_minus4)
    DXVK_VD_CMP_SPS_FIELDS(pic_order_cnt_type)
    DXVK_VD_CMP_SPS_FIELDS(offset_for_non_ref_pic)
    DXVK_VD_CMP_SPS_FIELDS(offset_for_top_to_bottom_field)
    DXVK_VD_CMP_SPS_FIELDS(log2_max_pic_order_cnt_lsb_minus4)
    DXVK_VD_CMP_SPS_FIELDS(num_ref_frames_in_pic_order_cnt_cycle)
    DXVK_VD_CMP_SPS_FIELDS(max_num_ref_frames)
    DXVK_VD_CMP_SPS_FIELDS(pic_width_in_mbs_minus1)
    DXVK_VD_CMP_SPS_FIELDS(pic_height_in_map_units_minus1)
    DXVK_VD_CMP_SPS_FIELDS(frame_crop_left_offset)
    DXVK_VD_CMP_SPS_FIELDS(frame_crop_right_offset)
    DXVK_VD_CMP_SPS_FIELDS(frame_crop_top_offset)
    DXVK_VD_CMP_SPS_FIELDS(frame_crop_bottom_offset)
    return true;
  }
#undef DXVK_VD_CMP_SPS_FIELDS


#define DXVK_VD_CMP_PPS_FIELDS(_f) \
  if (pps1._f != pps2._f) { \
    Logger::debug(str::format("PPS.", #_f ,": ", pps1._f, " != ", pps2._f)); \
    return false; \
  }
  static bool IsPPSEqual(
    const StdVideoH264PictureParameterSet &pps1,
    const StdVideoH264PictureParameterSet &pps2) {
    DXVK_VD_CMP_PPS_FIELDS(flags.transform_8x8_mode_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.redundant_pic_cnt_present_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.constrained_intra_pred_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.deblocking_filter_control_present_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.weighted_pred_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.bottom_field_pic_order_in_frame_present_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.entropy_coding_mode_flag)
    DXVK_VD_CMP_PPS_FIELDS(flags.pic_scaling_matrix_present_flag)
    DXVK_VD_CMP_PPS_FIELDS(num_ref_idx_l0_default_active_minus1)
    DXVK_VD_CMP_PPS_FIELDS(num_ref_idx_l1_default_active_minus1)
    DXVK_VD_CMP_PPS_FIELDS(weighted_bipred_idc)
    DXVK_VD_CMP_PPS_FIELDS(pic_init_qp_minus26)
    DXVK_VD_CMP_PPS_FIELDS(pic_init_qs_minus26)
    DXVK_VD_CMP_PPS_FIELDS(chroma_qp_index_offset)
    DXVK_VD_CMP_PPS_FIELDS(second_chroma_qp_index_offset)
    if (pps1.flags.pic_scaling_matrix_present_flag) {
      /// @todo Compare scaling lists
    }
    return true;
  }
#undef DXVK_VD_CMP_PPS_FIELDS


  VkResult DxvkVideoDecoder::UpdateSessionParameters(
    DxvkVideoDecodeInputParameters *pParms) {
    /* Update internal pointer(s). */
    pParms->sps.pOffsetForRefFrame = &pParms->spsOffsetForRefFrame;
    pParms->pps.pScalingLists = &pParms->ppsScalingLists;

    /* Information about a possible update of session parameters. */
    VkVideoDecodeH264SessionParametersAddInfoKHR h264AddInfo =
      { VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR };

    /* Find out if the SPS is already in the cache. */
    auto itSPS = std::find_if(m_parameterSetCache.sps.begin(),
      std::next(m_parameterSetCache.sps.begin(), m_parameterSetCache.spsCount),
      [&sps = pParms->sps](StdVideoH264SequenceParameterSet &v) -> bool { return IsSPSEqual(sps, v); });
    if (itSPS == std::next(m_parameterSetCache.sps.begin(), m_parameterSetCache.spsCount)) {
      /* A new SPS. */
      if (itSPS == m_parameterSetCache.sps.end()) {
        Logger::err(str::format("DxvkVideoDecoder: SPS count > ", m_parameterSetCache.sps.size()));
        return VK_ERROR_TOO_MANY_OBJECTS;
      }

      *itSPS = pParms->sps;
      ++m_parameterSetCache.spsCount;

      h264AddInfo.stdSPSCount = 1;
      h264AddInfo.pStdSPSs = &pParms->sps;
    }

    /* Find out if the PPS is already in the cache. */
    auto itPPS = std::find_if(m_parameterSetCache.pps.begin(),
      std::next(m_parameterSetCache.pps.begin(), m_parameterSetCache.ppsCount),
      [&pps = pParms->pps](StdVideoH264PictureParameterSet &v) -> bool { return IsPPSEqual(pps, v); });
    if (itPPS == std::next(m_parameterSetCache.pps.begin(), m_parameterSetCache.ppsCount)) {
      /* A new PPS. */
      if (itPPS == m_parameterSetCache.pps.end()) {
        Logger::err(str::format("DxvkVideoDecoder: PPS count > ", m_parameterSetCache.pps.size()));
        return VK_ERROR_TOO_MANY_OBJECTS;
      }

      *itPPS = pParms->pps;
      ++m_parameterSetCache.ppsCount;

      h264AddInfo.stdPPSCount = 1;
      h264AddInfo.pStdPPSs = &pParms->pps;
    }

    const uint32_t spsId = std::distance(m_parameterSetCache.sps.begin(), itSPS);
    const uint32_t ppsId = std::distance(m_parameterSetCache.pps.begin(), itPPS);

    pParms->sps.seq_parameter_set_id                = spsId;
    pParms->pps.seq_parameter_set_id                = spsId;
    pParms->pps.pic_parameter_set_id                = ppsId;
    pParms->stdH264PictureInfo.seq_parameter_set_id = spsId;
    pParms->stdH264PictureInfo.pic_parameter_set_id = ppsId;
    pParms->sps.level_idc                           = m_profile.decodeH264Capabilities.maxLevelIdc;

    if (h264AddInfo.stdSPSCount == 0 && h264AddInfo.stdPPSCount == 0)
      return VK_SUCCESS;

    /* Update videoSessionParameters with the new picture info. */
    VkVideoSessionParametersUpdateInfoKHR updateInfo =
     { VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_UPDATE_INFO_KHR, &h264AddInfo };
    updateInfo.updateSequenceCount = ++m_parameterSetCache.updateSequenceCount; /* Must start from 1. */

    return m_device->vkd()->vkUpdateVideoSessionParametersKHR(m_device->handle(),
      m_videoSessionParameters->handle(), &updateInfo);
  }


  void DxvkVideoDecoder::Decode(
    DxvkContext* ctx,
    DxvkVideoDecodeInputParameters parms)
  {
    VkResult vr;

    /* Complete the provided parameters. */
    vr = this->UpdateSessionParameters(&parms);
    if (vr != VK_SUCCESS)
      return;

    /*
     * Allocate space in the GPU buffer and copy encoded frame into it.
     */
    /* How many bytes in the ring buffer is required including alignment. */
    const uint32_t cbFrame = align(parms.bitstreamLength, m_profile.videoCapabilities.minBitstreamBufferSizeAlignment);
    /* How many bytes remains in the buffer. */
    const uint32_t cbRemaining = uint32_t(m_bitstreamBuffer.buffer->getSliceHandle().length) - m_bitstreamBuffer.offFree;

    if (cbFrame > cbRemaining) {
      m_bitstreamBuffer.offFree = 0; /* Start from begin of the ring buffer. */
      if (cbFrame > m_bitstreamBuffer.buffer->getSliceHandle().length)
        return; /* Frame data is apparently invalid. */
    }

    /* offFrame starts at 0, i.e. aligned. */
    const uint32_t offFrame = m_bitstreamBuffer.offFree;
    memcpy((uint8_t *)m_bitstreamBuffer.buffer->getSliceHandle().mapPtr + offFrame,
      parms.bitstream.data(), parms.bitstreamLength);

    /* Advance the offset of the free space past the just copied frame. */
    m_bitstreamBuffer.offFree += cbFrame;
    m_bitstreamBuffer.offFree = align(m_bitstreamBuffer.offFree, m_profile.videoCapabilities.minBitstreamBufferOffsetAlignment);
    if (m_bitstreamBuffer.offFree >= m_bitstreamBuffer.buffer->getSliceHandle().length)
      m_bitstreamBuffer.offFree = 0; /* Start from begin of the ring buffer. */

    /* A BufferMemoryBarrier is not needed because the buffer update happens before vkSubmit and:
     * "Queue submission commands automatically perform a domain operation from host to device
     *  for all writes performed before the command executes"
     */

    /*
     * Reset frames if requested.
     */
    if (parms.nal_unit_type == 5) { /* IDR immediate decoder reset. */
      m_DPB.idxCurrentDPBSlot = 0;
      for (auto &slot: m_DPB.slots) {
        slot.isReferencePicture = false;
      }
    }

    /*
     * Begin video decoding.
     */
    /* Init the target DPB slot, i.e. the slot where the reconstructed picture will be placed. */
    const int32_t dstSlotIndex = m_DPB.idxCurrentDPBSlot; /* Destination DPB slot for the decoded frame. */

    DxvkDPBSlot &dstDPBSlot = m_DPB.slots[dstSlotIndex];
    dstDPBSlot.isReferencePicture = false;
    dstDPBSlot.stdRefInfo         = parms.stdH264ReferenceInfo;

    const int DPBCapacity = m_DPB.slots.size();

    /* Reference pictures and the destination slot to be bound for video decoding. */
    std::vector<VkVideoPictureResourceInfoKHR> pictureResourceInfo(DPBCapacity);
    std::vector<VkVideoReferenceSlotInfoKHR> referenceSlotsInfo(DPBCapacity);

    for (int i = 0; i < DPBCapacity; ++i) {
      /* Deactivate DPB slots that contain frames with the same FrameNum as the currently decoded frame. */
      if (m_DPB.slots[i].isReferencePicture
       && m_DPB.slots[i].stdRefInfo.FrameNum == dstDPBSlot.stdRefInfo.FrameNum)
        m_DPB.slots[i].isReferencePicture = false;

      pictureResourceInfo[i] =
        { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
      pictureResourceInfo[i].codedOffset      = { 0, 0 };
      pictureResourceInfo[i].codedExtent      = { m_sampleWidth, m_sampleHeight };
      pictureResourceInfo[i].baseArrayLayer   = 0; /* "relative to the image subresource range" of the view */
      pictureResourceInfo[i].imageViewBinding = m_DPB.slots[i].imageView->handle();

      referenceSlotsInfo[i] =
        { VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR };
      referenceSlotsInfo[i].slotIndex         = m_DPB.slots[i].isReferencePicture ? i : -1;
      referenceSlotsInfo[i].pPictureResource  = &pictureResourceInfo[i];
    }

    VkVideoBeginCodingInfoKHR beginCodingInfo =
      { VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR };
    beginCodingInfo.flags                  = 0; /* reserved for future use */
    beginCodingInfo.videoSession           = m_videoSession->handle();
    beginCodingInfo.videoSessionParameters = m_videoSessionParameters->handle();
    beginCodingInfo.referenceSlotCount     = referenceSlotsInfo.size();
    beginCodingInfo.pReferenceSlots        = referenceSlotsInfo.data();

#ifdef DEBUG
    Logger::info(str::format("VREF: beginVideoCoding: dstSlotIndex=", dstSlotIndex, " ", m_sampleWidth, "x", m_sampleHeight));
    for (uint32_t i = 0; i < beginCodingInfo.referenceSlotCount; ++i) {
      auto &s = beginCodingInfo.pReferenceSlots[i];
      Logger::info(str::format("      DPB[", i, "]: slotIndex=", s.slotIndex, ", FrameNum=", m_DPB.slots[i].stdRefInfo.FrameNum, ", image=", s.pPictureResource->imageViewBinding, "/", m_DPB.slots[i].imageView->imageHandle()));
    }
#endif

    ctx->beginVideoCodingKHR(&beginCodingInfo);

    if (!m_fControlResetSubmitted) {
      VkVideoCodingControlInfoKHR controlInfo =
        { VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR };
      controlInfo.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR;

      ctx->controlVideoCodingKHR(&controlInfo);

      m_fControlResetSubmitted = true;
    }

    /*
     * Prepare destination DPB image.
     */
    std::array<VkImageMemoryBarrier2, 2> barriers{{
      { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 },
      { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 }}};

    /* Change the destination DPB slot image layout to VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR */
    barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
    barriers[0].srcAccessMask       = 0;
    barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
    barriers[0].dstAccessMask       = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
    barriers[0].oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED; /* 'The contents ... may be discarded.' */
    barriers[0].newLayout           = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image               = dstDPBSlot.image->handle();
    barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel   = 0;
    barriers[0].subresourceRange.levelCount     = 1;
    barriers[0].subresourceRange.baseArrayLayer = dstDPBSlot.baseArrayLayer;
    barriers[0].subresourceRange.layerCount     = 1;

    VkDependencyInfo dependencyInfo =
      { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers    = barriers.data();

    ctx->emitPipelineBarrier(DxvkCmdBuffer::VDecBuffer, &dependencyInfo);

    if (m_caps.distinctOutputImage) {
      /*
       * Prepare decode destination.
       */
      barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
      barriers[0].srcAccessMask       = 0;
      barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
      barriers[0].dstAccessMask       = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
      barriers[0].oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED; /* 'The contents ... may be discarded.' */
      barriers[0].newLayout           = VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
      barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image               = m_imageDecodeDst->handle();
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = 0;
      barriers[0].subresourceRange.layerCount     = 1;

      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers    = barriers.data();

      ctx->emitPipelineBarrier(DxvkCmdBuffer::VDecBuffer, &dependencyInfo);
    }

    /*
     * Setup "active reference pictures."
     */
    /* Count the number of reference pictures in the DPB. */
    uint32_t refSlotsCount = 0;
    for (auto &slot: m_DPB.slots) {
      if (slot.isReferencePicture)
        ++refSlotsCount;
    }

    /* VkVideoReferenceSlotInfoKHR refSlotInfo[idxRefSlot].pNext */
    std::vector<VkVideoDecodeH264DpbSlotInfoKHR> h264DpbSlotInfo(refSlotsCount);

    /* VkVideoReferenceSlotInfoKHR refSlotInfo[idxRefSlot].pPictureResource */
    std::vector<VkVideoPictureResourceInfoKHR>   refSlotPictureResourceInfo(refSlotsCount);

    /* VkVideoDecodeInfoKHR decodeInfo.pReferenceSlots */
    std::vector<VkVideoReferenceSlotInfoKHR>     refSlotInfo(refSlotsCount);

    uint32_t idxRefSlot = 0;
    for (unsigned i = 0; i < m_DPB.slots.size(); ++i) {
      if (!m_DPB.slots[i].isReferencePicture) {
        continue;
      }

      h264DpbSlotInfo[idxRefSlot] =
        { VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR };
      h264DpbSlotInfo[idxRefSlot].pStdReferenceInfo           = &m_DPB.slots[i].stdRefInfo;

      refSlotPictureResourceInfo[idxRefSlot] =
        { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
      refSlotPictureResourceInfo[idxRefSlot].codedOffset      = { 0, 0 };
      refSlotPictureResourceInfo[idxRefSlot].codedExtent      = { m_sampleWidth, m_sampleHeight };
      refSlotPictureResourceInfo[idxRefSlot].baseArrayLayer   = 0; /* "relative to the image subresource range" of the view */
      refSlotPictureResourceInfo[idxRefSlot].imageViewBinding = m_DPB.slots[i].imageView->handle();

      refSlotInfo[idxRefSlot] =
        { VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR, &h264DpbSlotInfo[idxRefSlot] };
      refSlotInfo[idxRefSlot].slotIndex                       = i;
      refSlotInfo[idxRefSlot].pPictureResource                = &refSlotPictureResourceInfo[idxRefSlot];

      ++idxRefSlot;
    }

    /*
     * Setup video decoding parameters 'decodeInfo' (VkVideoDecodeInfoKHR).
     */
    /* VkVideoDecodeInfoKHR decodeInfo.pNext */
    VkVideoDecodeH264PictureInfoKHR h264PictureInfo =
      { VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR };
    h264PictureInfo.pStdPictureInfo = &parms.stdH264PictureInfo;
    h264PictureInfo.sliceCount      = parms.sliceOffsets.size();
    h264PictureInfo.pSliceOffsets   = parms.sliceOffsets.data();

    /* VkVideoReferenceSlotInfoKHR dstSlotInfo.pNext */
    VkVideoDecodeH264DpbSlotInfoKHR dstH264DpbSlotInfo =
      { VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR };
    dstH264DpbSlotInfo.pStdReferenceInfo    = &dstDPBSlot.stdRefInfo;

    /* VkVideoReferenceSlotInfoKHR  dstSlotInfo.pPictureResource */
    VkVideoPictureResourceInfoKHR dstPictureResourceInfo =
      { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
    dstPictureResourceInfo.codedOffset      = { 0, 0 };
    dstPictureResourceInfo.codedExtent      = { m_sampleWidth, m_sampleHeight };
    dstPictureResourceInfo.baseArrayLayer   = 0; /* "relative to the image subresource range" of the view */
    dstPictureResourceInfo.imageViewBinding = dstDPBSlot.imageView->handle();

    /* VkVideoDecodeInfoKHR decodeInfo.pSetupReferenceSlot */
    VkVideoReferenceSlotInfoKHR dstSlotInfo =
      { VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR, &dstH264DpbSlotInfo };
    dstSlotInfo.slotIndex                   = dstSlotIndex;
    dstSlotInfo.pPictureResource            = &dstPictureResourceInfo;

    VkVideoDecodeInfoKHR decodeInfo =
      { VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR, &h264PictureInfo };
    decodeInfo.flags               = 0;
    decodeInfo.srcBuffer           = m_bitstreamBuffer.buffer->getSliceHandle().handle;
    decodeInfo.srcBufferOffset     = offFrame;
    decodeInfo.srcBufferRange      = cbFrame;
    decodeInfo.dstPictureResource  =
      { VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR };
    decodeInfo.dstPictureResource.codedOffset      = { 0, 0 };
    decodeInfo.dstPictureResource.codedExtent      = { m_sampleWidth, m_sampleHeight };
    /* "baseArrayLayer relative to the image subresource range the image view specified in
     * imageViewBinding was created with."
     */
    decodeInfo.dstPictureResource.baseArrayLayer   = 0;
    if (m_caps.distinctOutputImage) {
      decodeInfo.dstPictureResource.imageViewBinding = m_imageViewDecodeDst->handle();
    }
    else {
      decodeInfo.dstPictureResource.imageViewBinding = dstDPBSlot.imageView->handle();
    }
    decodeInfo.pSetupReferenceSlot = &dstSlotInfo;
    decodeInfo.referenceSlotCount  = refSlotInfo.size();
    decodeInfo.pReferenceSlots     = refSlotInfo.data();

#ifdef DEBUG
    Logger::info(str::format("VREF: decodeVideo: dstSlotIndex=", dstSlotIndex));
    for (uint32_t i = 0; i < decodeInfo.referenceSlotCount; ++i) {
      auto &s = decodeInfo.pReferenceSlots[i];
      Logger::info(str::format("       ref[", i, "]: slotIndex=", s.slotIndex, ", FrameNum=", h264DpbSlotInfo[i].pStdReferenceInfo->FrameNum, ", image=", s.pPictureResource->imageViewBinding));
    }
    Logger::info(str::format("       dst: slotIndex=", dstSlotInfo.slotIndex, ", FrameNum=", dstH264DpbSlotInfo.pStdReferenceInfo->FrameNum, " ref=", parms.stdH264PictureInfo.flags.is_reference, ", image=", dstSlotInfo.pPictureResource->imageViewBinding));
#endif

    ctx->decodeVideoKHR(&decodeInfo);

    /*
     * End video decoding.
     */
    VkVideoEndCodingInfoKHR endCodingInfo =
      { VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR};

    ctx->endVideoCodingKHR(&endCodingInfo);

    /*
     * Copy the decoded picture to the output view. Either from DPB slot or from m_imageDecodeDst.
     */
    Rc<DxvkImage> decodedPicture;
    uint32_t decodedArrayLayer;
    VkImageLayout decodedPictureLayout;
    if (m_caps.distinctOutputImage) {
      decodedPicture       = m_imageDecodeDst;
      decodedArrayLayer    = 0;
      decodedPictureLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR;
    }
    else {
      decodedPicture       = dstDPBSlot.image;
      decodedArrayLayer    = dstDPBSlot.baseArrayLayer;
      decodedPictureLayout = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR;
    }

    if (m_profile.videoQueueHasTransfer) {
      /* Wait for the decoded image to be available as a transfer source. */
      barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
      barriers[0].srcAccessMask       = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR;
      barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barriers[0].dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT;
      barriers[0].oldLayout           = decodedPictureLayout;
      barriers[0].newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image               = decodedPicture->handle();
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = decodedArrayLayer;
      barriers[0].subresourceRange.layerCount     = 1;

      /* Output image is already in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL. */

      dependencyInfo =
        { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers    = barriers.data();

      ctx->emitPipelineBarrier(DxvkCmdBuffer::VDecBuffer, &dependencyInfo);

      /* Copy decoded image -> output image. Y plane. */
      std::array<VkImageCopy2, 2> regions{{
        { VK_STRUCTURE_TYPE_IMAGE_COPY_2 },
        { VK_STRUCTURE_TYPE_IMAGE_COPY_2 }}};
      regions[0].srcSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_0_BIT;
      //regions[0].srcSubresource.mipLevel       = 0;
      regions[0].srcSubresource.baseArrayLayer   = decodedArrayLayer;
      regions[0].srcSubresource.layerCount       = 1;
      //regions[0].srcOffset                     = { 0, 0, 0 };
      regions[0].dstSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_0_BIT;
      //regions[0].dstSubresource.mipLevel       = 0;
      regions[0].dstSubresource.baseArrayLayer   = m_outputImageView->info().minLayer;
      regions[0].dstSubresource.layerCount       = 1;
      //regions[0].dstOffset                     = { 0, 0, 0 };
      regions[0].extent                          = { m_sampleWidth, m_sampleHeight, 1 };

      /* CbCr plane at half resolution. */
      regions[1].srcSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_1_BIT;
      //regions[1].srcSubresource.mipLevel       = 0;
      regions[1].srcSubresource.baseArrayLayer   = decodedArrayLayer;
      regions[1].srcSubresource.layerCount       = 1;
      //regions[1].srcOffset                     = { 0, 0, 0 };
      regions[1].dstSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_1_BIT;
      //regions[1].dstSubresource.mipLevel       = 0;
      regions[1].dstSubresource.baseArrayLayer   = m_outputImageView->info().minLayer;
      regions[1].dstSubresource.layerCount       = 1;
      //regions[1].dstOffset                     = { 0, 0, 0 };
      regions[1].extent                          = { m_sampleWidth / 2, m_sampleHeight / 2, 1 };

      VkCopyImageInfo2 copyImageInfo =
        { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
      copyImageInfo.srcImage       = decodedPicture->handle();
      copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      copyImageInfo.dstImage       = m_outputImageView->imageHandle();
      copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      copyImageInfo.regionCount    = 2;
      copyImageInfo.pRegions       = regions.data();

      ctx->emitCopyImage(DxvkCmdBuffer::VDecBuffer, &copyImageInfo);

      /* Restore layout of the decoded image. */
      barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barriers[0].srcAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT;
      barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR;
      barriers[0].dstAccessMask       = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR;
      barriers[0].oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barriers[0].newLayout           = decodedPictureLayout;
      barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image               = decodedPicture->handle();
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = decodedArrayLayer;
      barriers[0].subresourceRange.layerCount     = 1;

      /* Output image is will be release back to the graphics queue in EndFrame. */

      dependencyInfo =
        { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers    = barriers.data();

      ctx->emitPipelineBarrier(DxvkCmdBuffer::VDecBuffer, &dependencyInfo);
    }
    else {
      /*
       * Copy decoded picture to the output view.
       * Video queue can't do transfers.
       */
      /* Transfer the decoded image ownership to the graphics queue. */
      this->TransferImageQueueOwnership(ctx, decodedPicture, decodedArrayLayer,
        DxvkCmdBuffer::VDecBuffer,
        m_device->queues().videoDecode.queueFamily,
        VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
        VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
        decodedPictureLayout,
        DxvkCmdBuffer::InitBuffer,
        m_device->queues().graphics.queueFamily,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

      /* Prepare output image for transfer destination. */
      barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_NONE;
      barriers[0].srcAccessMask       = 0;
      barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barriers[0].dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      barriers[0].oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED; /* 'The contents ... may be discarded.' */
      barriers[0].newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image               = m_outputImageView->imageHandle();
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = m_outputImageView->info().minLayer;
      barriers[0].subresourceRange.layerCount     = 1;

      dependencyInfo =
        { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers    = barriers.data();

      ctx->emitPipelineBarrier(DxvkCmdBuffer::InitBuffer, &dependencyInfo);

      /* Copy decoded image -> output image. Y plane. */
      std::array<VkImageCopy2, 2> regions{{
        { VK_STRUCTURE_TYPE_IMAGE_COPY_2 },
        { VK_STRUCTURE_TYPE_IMAGE_COPY_2 }}};
      regions[0].srcSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_0_BIT;
      //regions[0].srcSubresource.mipLevel       = 0;
      regions[0].srcSubresource.baseArrayLayer   = decodedArrayLayer;
      regions[0].srcSubresource.layerCount       = 1;
      //regions[0].srcOffset                     = { 0, 0, 0 };
      regions[0].dstSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_0_BIT;
      //regions[0].dstSubresource.mipLevel       = 0;
      regions[0].dstSubresource.baseArrayLayer   = m_outputImageView->info().minLayer;
      regions[0].dstSubresource.layerCount       = 1;
      //regions[0].dstOffset                     = { 0, 0, 0 };
      regions[0].extent                          = { m_sampleWidth, m_sampleHeight, 1 };

      /* CbCr plane at half resolution. */
      regions[1].srcSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_1_BIT;
      //regions[1].srcSubresource.mipLevel       = 0;
      regions[1].srcSubresource.baseArrayLayer   = decodedArrayLayer;
      regions[1].srcSubresource.layerCount       = 1;
      //regions[1].srcOffset                     = { 0, 0, 0 };
      regions[1].dstSubresource.aspectMask       = VK_IMAGE_ASPECT_PLANE_1_BIT;
      //regions[1].dstSubresource.mipLevel       = 0;
      regions[1].dstSubresource.baseArrayLayer   = m_outputImageView->info().minLayer;
      regions[1].dstSubresource.layerCount       = 1;
      //regions[1].dstOffset                     = { 0, 0, 0 };
      regions[1].extent                          = { m_sampleWidth / 2, m_sampleHeight / 2, 1 };

      VkCopyImageInfo2 copyImageInfo =
        { VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2 };
      copyImageInfo.srcImage       = decodedPicture->handle();
      copyImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      copyImageInfo.dstImage       = m_outputImageView->imageHandle();
      copyImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      copyImageInfo.regionCount    = 2;
      copyImageInfo.pRegions       = regions.data();

      ctx->emitCopyImage(DxvkCmdBuffer::InitBuffer, &copyImageInfo);

      /* Restore layout of the output image. */
      barriers[0].srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barriers[0].srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      barriers[0].dstStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
      barriers[0].dstAccessMask       = 0;
      barriers[0].oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barriers[0].newLayout           = m_outputImageView->image()->info().layout; /* VK_IMAGE_LAYOUT_GENERAL. */
      barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image               = m_outputImageView->imageHandle();
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = m_outputImageView->info().minLayer;
      barriers[0].subresourceRange.layerCount     = 1;

      dependencyInfo =
        { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
      dependencyInfo.imageMemoryBarrierCount = 1;
      dependencyInfo.pImageMemoryBarriers    = barriers.data();

      ctx->emitPipelineBarrier(DxvkCmdBuffer::InitBuffer, &dependencyInfo);

      /* Return the decoded image ownership back to the video queue. */
      this->TransferImageQueueOwnership(ctx, decodedPicture, decodedArrayLayer,
        DxvkCmdBuffer::InitBuffer,
        m_device->queues().graphics.queueFamily,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        VK_ACCESS_2_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        DxvkCmdBuffer::VDecBuffer,
        m_device->queues().videoDecode.queueFamily,
        VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
        VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR | VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
        decodedPictureLayout);
    }

    /* Make sure that the involved objects are alive during command buffer execution. */
    ctx->trackResource(DxvkAccess::None, m_videoSession);
    ctx->trackResource(DxvkAccess::None, m_videoSessionParameters);
    ctx->trackResource(DxvkAccess::Write, m_outputImageView->image());
    if (m_profile.videoCapabilities.flags & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR) {
      for (auto &slot: m_DPB.slots)
        ctx->trackResource(slot.isReferencePicture? DxvkAccess::Read : DxvkAccess::Write, slot.image);
    }
    else
      ctx->trackResource(DxvkAccess::Write, dstDPBSlot.image); /* Same image in every slot. */
    if (m_caps.distinctOutputImage)
      ctx->trackResource(DxvkAccess::Write, m_imageDecodeDst);
    ctx->trackResource(DxvkAccess::Read, m_bitstreamBuffer.buffer);

    /* Keep reference picture. */
    if (parms.stdH264PictureInfo.flags.is_reference) {
      dstDPBSlot.isReferencePicture = true;
      m_DPB.idxCurrentDPBSlot = (m_DPB.idxCurrentDPBSlot + 1) % DPBCapacity;
    }
  }

}
