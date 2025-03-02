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


#pragma once

#include "dxvk_context.h"
#include "dxvk_memory.h"
#include "dxvk_image.h"
#include "dxvk_fence.h"

namespace dxvk {

  class DxvkDevice;

#define DXVK_VIDEO_DECODER_SURFACE_INVALID 0xff

  struct DxvkVideoDecodeProfileInfo {
    const char *                          profileName;

    /* Vulkan profile specific information. profileInfo.pNext */
    union {
      VkVideoDecodeH264ProfileInfoKHR     h264ProfileInfo;
      VkVideoDecodeH265ProfileInfoKHR     h265ProfileInfo;
    };
    /* Vulkan profile info. */
    VkVideoProfileInfoKHR                 profileInfo;

    union {
      VkVideoDecodeH264CapabilitiesKHR    decodeH264Capabilities;
      VkVideoDecodeH265CapabilitiesKHR    decodeH265Capabilities;
    };
    VkVideoDecodeCapabilitiesKHR          decodeCapabilities;
    VkVideoCapabilitiesKHR                videoCapabilities;

    VkVideoFormatPropertiesKHR            dpbFormatProperties;

    bool                                  videoQueueHasTransfer;
  };


  struct DxvkRefFrameInfo {
    uint8_t  idSurface;
    uint8_t  longTermReference : 1;
    uint8_t  usedForReference : 2;
    uint8_t  nonExistingFrame : 1;
    uint16_t frame_num;
    int32_t  PicOrderCnt[2];
  };


  struct DxvkVideoDecodeInputParameters {
    union {
      struct {
        StdVideoH264SequenceParameterSet sps;
        int32_t                          spsOffsetForRefFrame;

        StdVideoH264PictureParameterSet  pps;
        StdVideoH264ScalingLists         ppsScalingLists;

        StdVideoDecodeH264PictureInfo    stdH264PictureInfo;
        StdVideoDecodeH264ReferenceInfo  stdH264ReferenceInfo;
      } h264;
      struct {
        StdVideoH265VideoParameterSet    vps;
        StdVideoH265ProfileTierLevel     vpsProfileTierLevel;
        StdVideoH265SequenceParameterSet sps;
        StdVideoH265PictureParameterSet  pps;
        StdVideoH265ScalingLists         ppsScalingLists;

        StdVideoDecodeH265PictureInfo    stdPictureInfo;
        StdVideoDecodeH265ReferenceInfo  stdReferenceInfo;

        uint8_t                          sps_max_dec_pic_buffering;
      } h265;
    };

    uint8_t                            nal_unit_type;

    std::vector<uint32_t>              sliceOffsets;

    uint8_t                            idSurface;
    uint32_t                           refFramesCount;
    std::array<DxvkRefFrameInfo, 16>   refFrames;

    VkDeviceSize                       bitstreamLength;
    std::vector<uint8_t>               bitstream;
  };


  /**
   * \brief Video session handle
   * 
   * Manages a handle of video session object.
   */
  class DxvkVideoSessionHandle : public DxvkResource {

  public:
    DxvkVideoSessionHandle(
      const Rc<DxvkDevice>& device);
    ~DxvkVideoSessionHandle();

    void create(
      const VkVideoSessionCreateInfoKHR& sessionCreateInfo);

    VkVideoSessionKHR handle() const {
      return m_videoSession;
    }

  private:

    Rc<DxvkDevice>              m_device;
    VkVideoSessionKHR           m_videoSession = VK_NULL_HANDLE;

  };


  /**
   * \brief Video session parameters handle
   * 
   * Manages a handle of video session parameters object.
   */
  class DxvkVideoSessionParametersHandle : public DxvkResource {

  public:
    DxvkVideoSessionParametersHandle(
      const Rc<DxvkDevice>& device);
    ~DxvkVideoSessionParametersHandle();

    void create(
      const Rc<DxvkVideoSessionHandle>& videoSession,
      const void *pDecoderSessionParametersCreateInfo);

    VkVideoSessionParametersKHR handle() const {
      return m_videoSessionParameters;
    }

  private:

    Rc<DxvkDevice>              m_device;
    /* Hold a reference because session must be deleted after parameters. */
    Rc<DxvkVideoSessionHandle>  m_videoSession;
    VkVideoSessionParametersKHR m_videoSessionParameters = VK_NULL_HANDLE;

  };


  /**
   * \brief Video bitstream buffer
   * 
   * Manages a buffer for source bitstream for the decoder.
   */
  class DxvkVideoBitstreamBuffer : public DxvkResource {

  public:
    DxvkVideoBitstreamBuffer(
      const Rc<DxvkDevice>& device,
      DxvkMemoryAllocator& memAlloc);
    ~DxvkVideoBitstreamBuffer();

    void create(
      const VkVideoProfileListInfoKHR &profileListInfo,
      VkDeviceSize size);

    VkBuffer buffer() const {
      return m_buffer.buffer;
    }

    void* mapPtr(VkDeviceSize offset) const  {
      return m_mapPtr ? m_mapPtr + offset : nullptr;
    }

    VkDeviceSize length() const {
      return m_length;
    }

  private:

    Rc<DxvkDevice>              m_device;
    DxvkMemoryAllocator&        m_memAlloc;
    DxvkBufferHandle            m_buffer;

    /* Shortcuts. */
    uint8_t*                    m_mapPtr = nullptr;
    VkDeviceSize                m_length = 0;
  };


  /**
   * \brief Video decoder
   *
   * Provides decoding.
   */
  class DxvkVideoDecoder : public RcObject {
    
  public:

    DxvkVideoDecoder(
      const Rc<DxvkDevice>& device,
            DxvkMemoryAllocator& memAlloc,
      const DxvkVideoDecodeProfileInfo& profile,
            uint32_t sampleWidth,
            uint32_t sampleHeight,
            VkFormat outputFormat,
            uint32_t bitstreamBufferSize);
    ~DxvkVideoDecoder();

    void BeginFrame(
      DxvkContext* ctx,
      const Rc<DxvkImageView>& imageView);

    void EndFrame(
      DxvkContext* ctx);

    void Decode(
      DxvkContext* ctx,
      DxvkVideoDecodeInputParameters parms);

    VkVideoCodecOperationFlagsKHR GetVideoCodecOperation() const {
      return m_profile.profileInfo.videoCodecOperation;
    }

  private:

    /* Creation parameters. */
    Rc<DxvkDevice>                      m_device;
    DxvkMemoryAllocator&                m_memAlloc;
    DxvkVideoDecodeProfileInfo          m_profile;
    uint32_t                            m_sampleWidth;
    uint32_t                            m_sampleHeight;
    VkFormat                            m_outputFormat;

    /* Caps derived from Vulkan caps. */
    struct DxvkVideoDecodeCapabilities {
      /* Whether vkCmdDecodeVideo output image can be not from DPB. */
      bool                              distinctOutputImage = false;
    };
    DxvkVideoDecodeCapabilities         m_caps;

    /* Vulkan video decoding objects. */
    Rc<DxvkVideoSessionHandle>          m_videoSession;
    std::vector<DxvkMemory>             m_videoSessionMemory;
    Rc<DxvkVideoSessionParametersHandle> m_videoSessionParameters;

    /* Whether Vulkan video decoder requires an initial reset. */
    bool                                m_fControlResetSubmitted = false;

    /* Provided in BeginFrame. */
    Rc<DxvkImageView>                   m_outputImageView = nullptr;

    /* Ring buffer for incoming encoded video data. */
    Rc<DxvkVideoBitstreamBuffer>        m_bitstreamBuffer;
    /* Ring buffer offset. */
    uint32_t                            m_offFree = 0;

    /* Decoded Picture Buffer.
     * Contains reconstructed pictures including the currently decoded frame.
     */
    struct DxvkDPBSlot {
      Rc<DxvkImage>                     image = nullptr;
      Rc<DxvkImageView>                 imageView = nullptr;
      uint32_t                          baseArrayLayer = 0;
      bool                              isActive = false;
      struct {
        StdVideoDecodeH264ReferenceInfo   stdRefInfo = {};
      } h264;
      struct {
        StdVideoDecodeH265ReferenceInfo   stdRefInfo = {};
      } h265;
      uint8_t                           idSurface = DXVK_VIDEO_DECODER_SURFACE_INVALID;

      void deactivate() {
        this->isActive           = false;
        this->h264.stdRefInfo    = {};
        this->h265.stdRefInfo    = {};
        this->idSurface          = DXVK_VIDEO_DECODER_SURFACE_INVALID;
      }
    };

    /* Information about an uncompressed surface. */
    struct DxvkRefFrame {
      int32_t                           dpbSlotIndex = -1;
      DxvkRefFrameInfo                  refFrameInfo = {};
    };

    struct DxvkDPB {
      std::vector<DxvkDPBSlot>          slots;
      int                               idxCurrentDPBSlot = 0;

      /* Uncompressed surface id (idSurface) -> frame information. */
      std::map<uint8_t, DxvkRefFrame>   refFrames;

      void reset() {
        for (auto &slot: this->slots)
          slot.deactivate();
        this->idxCurrentDPBSlot = 0;
        this->refFrames.clear();
      }
    };
    struct DxvkDPB                      m_DPB;

    /* Optional decode destination image if m_caps.distinctOutputImage is true. */
    Rc<DxvkImage>                       m_imageDecodeDst = nullptr;
    Rc<DxvkImageView>                   m_imageViewDecodeDst = nullptr;

    /* Store incoming SPS and PPS data and reuse its ids. */
    struct DxvkParameterSetCache
    {
      uint32_t                          vpsCount = 0;
      uint32_t                          spsCount = 0;
      uint32_t                          ppsCount = 0;
      uint32_t                          updateSequenceCount = 0;
      struct {
        std::array<StdVideoH264SequenceParameterSet, 32>  sps;
        std::array<StdVideoH264PictureParameterSet, 256>  pps;
      } h264;
      struct {
        std::array<StdVideoH265VideoParameterSet, 32>     vps;
        std::array<StdVideoH265SequenceParameterSet, 32>  sps;
        std::array<StdVideoH265PictureParameterSet, 256>  pps;
      } h265;
    };
    DxvkParameterSetCache               m_parameterSetCache;

    Rc<DxvkFence>                       m_queueOwnershipTransferFence;
    std::atomic<uint64_t>               m_queueOwnershipTransferValue = 0;

    /*
     * Internal methods.
     */
    VkResult UpdateSessionParametersH264(
      DxvkVideoDecodeInputParameters *pParms);

    VkResult UpdateSessionParametersH265(
      DxvkVideoDecodeInputParameters *pParms);

    void TransferImageQueueOwnership(
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
      VkImageLayout         newLayout);
  };
  
}
