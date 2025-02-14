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

  struct DxvkVideoDecodeProfileInfo {
    const char *                          profileName;

    /* Vulkan profile specific information. profileInfo.pNext */
    union {
      VkVideoDecodeH264ProfileInfoKHR     h264ProfileInfo;
    };
    /* Vulkan profile info. */
    VkVideoProfileInfoKHR                 profileInfo;

    union {
      VkVideoDecodeH264CapabilitiesKHR    decodeH264Capabilities;
    };
    VkVideoDecodeCapabilitiesKHR          decodeCapabilities;
    VkVideoCapabilitiesKHR                videoCapabilities;

    VkVideoFormatPropertiesKHR            dpbFormatProperties;

    bool                                  videoQueueHasTransfer;
  };


  struct DxvkVideoDecodeInputParameters {
    StdVideoH264SequenceParameterSet sps;
    int32_t                          spsOffsetForRefFrame;

    StdVideoH264PictureParameterSet  pps;
    StdVideoH264ScalingLists         ppsScalingLists;

    StdVideoDecodeH264PictureInfo    stdH264PictureInfo;
    StdVideoDecodeH264ReferenceInfo  stdH264ReferenceInfo;

    VkDeviceSize                     bitstreamLength;

    std::vector<uint32_t>            sliceOffsets;

    uint8_t                          nal_unit_type;

    std::vector<uint8_t>             bitstream;
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
      const VkVideoSessionParametersCreateInfoKHR& sessionParametersCreateInfo);

    VkVideoSessionParametersKHR handle() const {
      return m_videoSessionParameters;
    }

  private:

    Rc<DxvkDevice>              m_device;
    VkVideoSessionParametersKHR m_videoSessionParameters = VK_NULL_HANDLE;

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
            VkFormat outputFormat);
    ~DxvkVideoDecoder();

    void BeginFrame(
      DxvkContext* ctx,
      const Rc<DxvkImageView>& imageView);

    void EndFrame(
      DxvkContext* ctx);

    void Decode(
      DxvkContext* ctx,
      DxvkVideoDecodeInputParameters parms);

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
    struct DxvkBitstreamBuffer {
      Rc<DxvkBuffer>                    buffer;
      uint32_t                          offFree = 0;
    };
    struct DxvkBitstreamBuffer          m_bitstreamBuffer;

    /* Decoded Picture Buffer.
     * Contains reconstructed pictures including the currently decoded frame.
     */
    struct DxvkDPBSlot {
      Rc<DxvkImage>                     image = nullptr;
      Rc<DxvkImageView>                 imageView = nullptr;
      uint32_t                          baseArrayLayer = 0;
      bool                              isReferencePicture = 0;
      StdVideoDecodeH264ReferenceInfo   stdRefInfo = {};
    };

    struct DxvkDPB {
      std::vector<DxvkDPBSlot>          slots;
      int                               idxCurrentDPBSlot = 0;
    };
    struct DxvkDPB                      m_DPB;

    /* Optional decode destination image if m_caps.distinctOutputImage is true. */
    Rc<DxvkImage>                       m_imageDecodeDst = nullptr;
    Rc<DxvkImageView>                   m_imageViewDecodeDst = nullptr;

    /* Store incoming SPS and PPS data and reuse its ids. */
    struct DxvkParameterSetCache
    {
      uint32_t                          spsCount = 0;
      uint32_t                          ppsCount = 0;
      uint32_t                          updateSequenceCount = 0;
      std::array<StdVideoH264SequenceParameterSet, 32>  sps;
      std::array<StdVideoH264PictureParameterSet, 256>  pps;
    };
    DxvkParameterSetCache               m_parameterSetCache;

    Rc<DxvkFence>                       m_queueOwnershipTransferFence;
    std::atomic<uint64_t>               m_queueOwnershipTransferValue = 0;

    /*
     * Internal methods.
     */
    VkResult UpdateSessionParameters(
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
