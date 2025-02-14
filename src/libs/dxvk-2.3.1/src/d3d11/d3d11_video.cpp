#include <algorithm>

#include "d3d11_context_imm.h"
#include "d3d11_video.h"

#include <d3d11_video_blit_frag.h>
#include <d3d11_video_blit_vert.h>

namespace dxvk {

#ifdef VBOX_WITH_DXVK_VIDEO
  D3D11VideoDecoder::D3D11VideoDecoder(
          D3D11Device*                    pDevice,
          const D3D11_VIDEO_DECODER_DESC  &VideoDesc,
          const D3D11_VIDEO_DECODER_CONFIG &Config,
          const DxvkVideoDecodeProfileInfo& profile)
  : D3D11DeviceChild<ID3D11VideoDecoder>(pDevice),
    m_desc(VideoDesc), m_config(Config),
    m_device(pDevice->GetDXVKDevice()) {
    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(
      m_desc.OutputFormat, DXGI_VK_FORMAT_MODE_COLOR);

    if (formatInfo.Format == VK_FORMAT_UNDEFINED)
      throw DxvkError(str::format("D3D11VideoDecoder: Unsupported output DXGI format: ", m_desc.OutputFormat));

    m_videoDecoder = m_device->createVideoDecoder(profile, m_desc.SampleWidth, m_desc.SampleHeight, formatInfo.Format);
  }


  D3D11VideoDecoder::~D3D11VideoDecoder() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDecoder::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11VideoDecoder)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoDecoder), riid)) {
      Logger::warn("D3D11VideoDecoder::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDecoder::GetCreationParameters(
        D3D11_VIDEO_DECODER_DESC *pVideoDesc,
        D3D11_VIDEO_DECODER_CONFIG *pConfig) {
    if (pVideoDesc != nullptr)
      *pVideoDesc = m_desc;
    if (pConfig != nullptr)
      *pConfig = m_config;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDecoder::GetDriverHandle(
        HANDLE *pDriverHandle) {
    if (pDriverHandle != nullptr)
      *pDriverHandle = m_videoDecoder.ptr();
    return S_OK;
  }


  HRESULT D3D11VideoDecoder::GetDecoderBuffer(
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
          UINT*                           BufferSize,
          void**                          ppBuffer)
  {
    if (Type >= m_decoderBuffers.size())
      return E_INVALIDARG;

    D3D11VideoDecoderBuffer& decoderBuffer = m_decoderBuffers[Type];

    if (decoderBuffer.buffer.size() == 0) {
      size_t cbBuffer;
      switch (Type)
      {
        case D3D11_VIDEO_DECODER_BUFFER_BITSTREAM:
          /* Arbitrary. Sufficiently big for one compressed frame (usually). */
          cbBuffer = 1024*1024;
          break;
        default:
          cbBuffer = 65536;
      }
      decoderBuffer.buffer.resize(cbBuffer);
    }

    if (BufferSize != nullptr)
      *BufferSize = decoderBuffer.buffer.size();
    if (ppBuffer != nullptr)
      *ppBuffer = decoderBuffer.buffer.data();
    return S_OK;
  }


  HRESULT D3D11VideoDecoder::ReleaseDecoderBuffer(
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type)
  {
    if (Type >= m_decoderBuffers.size())
      return E_INVALIDARG;

    return S_OK;
  }


  template<typename DXVA_Slice_H264_T>
  static bool GetSliceOffsetsAndNALType(
          DxvkVideoDecodeInputParameters *pParms,
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pSliceDesc,
    const void *pSlices,
    const uint8_t *pBitStream,
          uint32_t cbBitStream) {
     const DXVA_Slice_H264_T *paSlices = (DXVA_Slice_H264_T *)pSlices;
     const uint32_t cSlices = pSliceDesc->DataSize / sizeof(DXVA_Slice_H264_T);

     /* D3D11VideoDecoder::GetVideoDecodeH264InputParameters checks that 'pSliceDesc->DataSize' is less than
      * the size of D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL buffer that is assigned in
      * D3D11VideoDecoder::GetDecoderBuffer. I.e. 'cSlices' is limuted too.
      */
     pParms->sliceOffsets.resize(cSlices);

     for (uint32_t i = 0; i < cSlices; ++i) {
       const DXVA_Slice_H264_T& slice = paSlices[i];

       if (slice.SliceBytesInBuffer > cbBitStream
        || slice.BSNALunitDataLocation > cbBitStream - slice.SliceBytesInBuffer
        || slice.SliceBytesInBuffer < 4) { /* NALU header: 00, 00, 01, xx */
         Logger::warn(str::format("D3D11VideoDecoder::GetH264: Invalid slice at ",
           slice.BSNALunitDataLocation, "/", slice.SliceBytesInBuffer, ", bitstream size ", cbBitStream));
         return false;
       }

       if (slice.wBadSliceChopping) {
         /* Should not happen because we use a sufficiently big bitstream buffer (see GetDecoderBuffer). */
         Logger::warn(str::format("D3D11VideoDecoder::GetH264: Ignored slice with wBadSliceChopping ",
           slice.wBadSliceChopping));
         return false; /// @todo not supported yet
       }

       pParms->sliceOffsets[i] = slice.BSNALunitDataLocation;

       const uint8_t *pu8NALHdr = pBitStream + slice.BSNALunitDataLocation;
       const uint8_t nal_unit_type = pu8NALHdr[3] & 0x1F;

       Logger::debug(str::format("NAL[", i, "]=", (uint32_t)nal_unit_type, " at ",
         slice.BSNALunitDataLocation, "/", slice.SliceBytesInBuffer));

       if (i == 0)
         pParms->nal_unit_type = nal_unit_type;
     }

     return true;
  }


  bool D3D11VideoDecoder::GetVideoDecodeH264InputParameters(
          UINT BufferCount,
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pBufferDescs,
          DxvkVideoDecodeInputParameters *pParms) {
    /*
     * Fetch all pieces of data from available buffers.
     */
    const DXVA_PicParams_H264*             pPicParams     = nullptr;
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pPicParamsDesc = nullptr;
    const DXVA_Qmatrix_H264*               pQmatrix       = nullptr;
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pQmatrixDesc   = nullptr;
    const void*                            pSlices        = nullptr;
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pSliceDesc     = nullptr;
    const uint8_t*                         pBitStream     = nullptr;
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pBitStreamDesc = nullptr;

    for (UINT i = 0; i < BufferCount; ++i) {
      const auto& desc = pBufferDescs[i];
      if (desc.BufferType >= m_decoderBuffers.size()) {
        Logger::warn(str::format("DXVK: Video Decode: Ignored buffer type ", desc.BufferType));
        continue;
      }

      D3D11VideoDecoderBuffer const &b = m_decoderBuffers[desc.BufferType];
      Logger::debug(str::format("D3D11VideoDecoder::GetH264: Type ", desc.BufferType, ", size ", b.buffer.size()));

      if (desc.DataSize > b.buffer.size()) {
        Logger::warn(str::format("DXVK: Video Decode: Buffer ", desc.BufferType, " invalid size: ", desc.DataSize, " > ", b.buffer.size()));
        continue;
      }

      switch (desc.BufferType) {
        case D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS:
          pPicParams = (DXVA_PicParams_H264 *)b.buffer.data();
          pPicParamsDesc = &desc;
          break;

        case D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX:
          pQmatrix = (DXVA_Qmatrix_H264 *)b.buffer.data();
          pQmatrixDesc = &desc;
          break;

        case D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL:
          pSlices = b.buffer.data();
          pSliceDesc = &desc;
          break;

        case D3D11_VIDEO_DECODER_BUFFER_BITSTREAM:
          pBitStream = (uint8_t *)b.buffer.data();
          pBitStreamDesc = &desc;
          break;

        default:
          break;
      }
    }

    if (pPicParams == nullptr || pSlices == nullptr || pBitStream == nullptr) {
      Logger::warn(str::format("DXVK: Video Decode: Not enough data:"
        " PicParams ", (uint32_t)(pPicParams != nullptr),
        " Slice ", (uint32_t)(pSlices != nullptr),
        " BitStream ", (uint32_t)(pBitStream != nullptr)));
      return false;
    }

    if (pPicParamsDesc->DataSize < sizeof(DXVA_PicParams_H264)) {
      Logger::warn(str::format("DXVK: Video Decode: PicParams buffer size is too small: ", pPicParamsDesc->DataSize));
      return false;
    }

    if (pQmatrixDesc->DataSize < sizeof(DXVA_Qmatrix_H264)) {
      Logger::warn(str::format("DXVK: Video Decode: Qmatrix buffer size is too small: ", pQmatrixDesc->DataSize));
      return false;
    }

    struct DxvkVideoDecodeInputParameters &p = *pParms;

    p.sps.flags.constraint_set0_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.constraint_set1_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.constraint_set2_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.constraint_set3_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.constraint_set4_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.constraint_set5_flag                 = 0; /* not known, assume unconstrained */
    p.sps.flags.direct_8x8_inference_flag            = pPicParams->ContinuationFlag
                                                      ? (pPicParams->direct_8x8_inference_flag ? 1 : 0)
                                                      : 0;
    p.sps.flags.mb_adaptive_frame_field_flag         = pPicParams->MbaffFrameFlag ? 1 : 0; /// @todo Is it?
    p.sps.flags.frame_mbs_only_flag                  = pPicParams->frame_mbs_only_flag ? 1 : 0;
    p.sps.flags.delta_pic_order_always_zero_flag     = pPicParams->ContinuationFlag
                                                      ? (pPicParams->delta_pic_order_always_zero_flag ? 1 : 0)
                                                      : 0;
    p.sps.flags.separate_colour_plane_flag           = 0; /* 4:4:4 only. Apparently DXVA decoding profiles do not use this flags. */
    p.sps.flags.gaps_in_frame_num_value_allowed_flag = 1; /// @todo unknown
    p.sps.flags.qpprime_y_zero_transform_bypass_flag = 0; /// @todo unknown
    p.sps.flags.frame_cropping_flag                  = 0; /* not used */
    p.sps.flags.seq_scaling_matrix_present_flag      = 0; /* not used */
    p.sps.flags.vui_parameters_present_flag          = 0; /* not used */
    p.sps.profile_idc                                = STD_VIDEO_H264_PROFILE_IDC_HIGH; /* Unknown */
    p.sps.level_idc                                  = StdVideoH264LevelIdc(0); /* Unknown, set to maxLevelIdc by Dxvk decoder. */
    p.sps.chroma_format_idc                          = StdVideoH264ChromaFormatIdc(pPicParams->chroma_format_idc);
    p.sps.seq_parameter_set_id                       = 0; /* Unknown, will be inferred by the Dxvk decoder. */
    p.sps.bit_depth_luma_minus8                      = pPicParams->bit_depth_luma_minus8;
    p.sps.bit_depth_chroma_minus8                    = pPicParams->bit_depth_chroma_minus8;
    p.sps.log2_max_frame_num_minus4                  = pPicParams->ContinuationFlag
                                                        ? pPicParams->log2_max_frame_num_minus4
                                                        : 0;
    p.sps.pic_order_cnt_type                         = pPicParams->ContinuationFlag
                                                        ? StdVideoH264PocType(pPicParams->pic_order_cnt_type)
                                                        : StdVideoH264PocType(0);
    p.sps.offset_for_non_ref_pic                     = 0; /// @todo unknown
    p.sps.offset_for_top_to_bottom_field             = 0; /// @todo unknown
    p.sps.log2_max_pic_order_cnt_lsb_minus4          = pPicParams->ContinuationFlag
                                                        ? pPicParams->log2_max_pic_order_cnt_lsb_minus4
                                                        : 0;
    p.sps.num_ref_frames_in_pic_order_cnt_cycle      = 0; /* Unknown */
    p.sps.max_num_ref_frames                         = pPicParams->num_ref_frames;
    p.sps.reserved1                                  = 0;
    p.sps.pic_width_in_mbs_minus1                    = pPicParams->wFrameWidthInMbsMinus1;
    p.sps.pic_height_in_map_units_minus1             = pPicParams->wFrameHeightInMbsMinus1; /// @todo Is it?
    p.sps.frame_crop_left_offset                     = 0; /* not used */
    p.sps.frame_crop_right_offset                    = 0; /* not used */
    p.sps.frame_crop_top_offset                      = 0; /* not used */
    p.sps.frame_crop_bottom_offset                   = 0; /* not used */
    p.sps.reserved2                                  = 0;
    p.sps.pOffsetForRefFrame                         = nullptr; /* &p.spsOffsetForRefFrame, updated by dxvk decoder. */
    p.sps.pScalingLists                              = nullptr; /* not used */
    p.sps.pSequenceParameterSetVui                   = nullptr; /* not used */
    p.spsOffsetForRefFrame                           = 0; /// @todo Is it?

    p.pps.flags.transform_8x8_mode_flag                = pPicParams->transform_8x8_mode_flag;
    p.pps.flags.redundant_pic_cnt_present_flag         = pPicParams->ContinuationFlag
                                                          ? (pPicParams->redundant_pic_cnt_present_flag ? 1 : 0)
                                                          : 0;
    p.pps.flags.constrained_intra_pred_flag            = pPicParams->constrained_intra_pred_flag ? 1 : 0;
    p.pps.flags.deblocking_filter_control_present_flag = pPicParams->deblocking_filter_control_present_flag ? 1 : 0;
    p.pps.flags.weighted_pred_flag                     = pPicParams->weighted_pred_flag ? 1 : 0;
    p.pps.flags.bottom_field_pic_order_in_frame_present_flag = pPicParams->ContinuationFlag
                                                                ? (pPicParams->pic_order_present_flag ? 1 : 0)
                                                                : 0;
    p.pps.flags.entropy_coding_mode_flag               = pPicParams->ContinuationFlag
                                                          ? (pPicParams->entropy_coding_mode_flag ? 1 : 0)
                                                          : 0;
    p.pps.flags.pic_scaling_matrix_present_flag        = pQmatrix != nullptr ? 1 : 0;
    p.pps.seq_parameter_set_id                         = 0; /* Unknown, will be inferred by the Dxvk decoder. */
    p.pps.pic_parameter_set_id                         = 0; /* Unknown, will be inferred by the Dxvk decoder. */
    p.pps.num_ref_idx_l0_default_active_minus1         = pPicParams->ContinuationFlag
                                                          ? pPicParams->num_ref_idx_l0_active_minus1
                                                          : 0;
    p.pps.num_ref_idx_l1_default_active_minus1         = pPicParams->ContinuationFlag
                                                          ? pPicParams->num_ref_idx_l1_active_minus1
                                                          : 0;
    p.pps.weighted_bipred_idc                          = StdVideoH264WeightedBipredIdc(pPicParams->weighted_bipred_idc);
    p.pps.pic_init_qp_minus26                          = pPicParams->ContinuationFlag
                                                          ? pPicParams->pic_init_qp_minus26
                                                          : 0;
    p.pps.pic_init_qs_minus26                          = pPicParams->pic_init_qs_minus26;
    p.pps.chroma_qp_index_offset                       = pPicParams->chroma_qp_index_offset;
    p.pps.second_chroma_qp_index_offset                = pPicParams->second_chroma_qp_index_offset;
    p.pps.pScalingLists                                = nullptr; /* &p.ppsScalingLists, updated by dxvk decoder. */

    if (p.pps.flags.pic_scaling_matrix_present_flag) {
      p.ppsScalingLists.scaling_list_present_mask        = 0xFF; /* 6x 4x4 and 2x 8x8 = 8 bits total */
      p.ppsScalingLists.use_default_scaling_matrix_mask  = 0;
      memcpy(p.ppsScalingLists.ScalingList4x4, pQmatrix->bScalingLists4x4, sizeof(pQmatrix->bScalingLists4x4));
      memcpy(p.ppsScalingLists.ScalingList8x8, pQmatrix->bScalingLists8x8, sizeof(pQmatrix->bScalingLists8x8));
    }

    /* Fetch slice offsets. */
    bool fSuccess = m_config.ConfigBitstreamRaw == 2
      ? GetSliceOffsetsAndNALType<DXVA_Slice_H264_Short>(&p, pSliceDesc, pSlices, pBitStream, pBitStreamDesc->DataSize)
      : GetSliceOffsetsAndNALType<DXVA_Slice_H264_Long>(&p, pSliceDesc, pSlices, pBitStream, pBitStreamDesc->DataSize);
    if (!fSuccess)
      return false;

    /// @todo Avoid intermediate buffer. Directly copy to a DxvkBuffer?
    p.bitstreamLength = pBitStreamDesc->DataSize;
    p.bitstream.resize(p.bitstreamLength);
    memcpy(p.bitstream.data(), pBitStream, p.bitstream.size());

    p.stdH264PictureInfo.flags.field_pic_flag           = pPicParams->field_pic_flag;
    p.stdH264PictureInfo.flags.is_intra                 = pPicParams->IntraPicFlag;
    p.stdH264PictureInfo.flags.IdrPicFlag               = p.nal_unit_type == 5 ? 1 : 0; 
    p.stdH264PictureInfo.flags.bottom_field_flag        = pPicParams->CurrPic.AssociatedFlag; /* flag is bottom field flag */
    p.stdH264PictureInfo.flags.is_reference             = pPicParams->RefPicFlag;
    p.stdH264PictureInfo.flags.complementary_field_pair = 0; /// @todo unknown
    p.stdH264PictureInfo.seq_parameter_set_id           = 0; /* Unknown, will be inferred by the Dxvk decoder. */
    p.stdH264PictureInfo.pic_parameter_set_id           = 0; /* Unknown, will be inferred by the Dxvk decoder. */
    p.stdH264PictureInfo.reserved1                      = 0;
    p.stdH264PictureInfo.reserved2                      = 0;
    p.stdH264PictureInfo.frame_num                      = pPicParams->frame_num;
    p.stdH264PictureInfo.idr_pic_id                     = 0; /// @todo unknown
    p.stdH264PictureInfo.PicOrderCnt[0]                 = pPicParams->CurrFieldOrderCnt[0]; /// @todo Is it?
    p.stdH264PictureInfo.PicOrderCnt[1]                 = pPicParams->CurrFieldOrderCnt[1];

    p.stdH264ReferenceInfo.flags.top_field_flag         =
      (p.stdH264PictureInfo.flags.field_pic_flag && !p.stdH264PictureInfo.flags.bottom_field_flag) ? 1 : 0;
    p.stdH264ReferenceInfo.flags.bottom_field_flag      =
      (p.stdH264PictureInfo.flags.field_pic_flag && p.stdH264PictureInfo.flags.bottom_field_flag) ? 1 : 0;
    p.stdH264ReferenceInfo.flags.used_for_long_term_reference = 0; /// @todo
    p.stdH264ReferenceInfo.flags.is_non_existing        = 0;
    p.stdH264ReferenceInfo.FrameNum                     = pPicParams->frame_num;
    p.stdH264ReferenceInfo.reserved                     = 0;
    p.stdH264ReferenceInfo.PicOrderCnt[0]               = pPicParams->CurrFieldOrderCnt[0]; /// @todo Is it?
    p.stdH264ReferenceInfo.PicOrderCnt[1]               = pPicParams->CurrFieldOrderCnt[1];

    return true;
  }




  D3D11VideoDecoderOutputView::D3D11VideoDecoderOutputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoDecoderOutputView>(pDevice),
    m_resource(pResource), m_desc(Desc) {
    /* Desc.DecodeProfile and resource format has been verified by the caller (Device). */
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(
      resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    /* In principle it is possible to use this view as video decode output if the Vulkan implementation
     * supports VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR capability. In this case
     * the image must be either created with VkVideoProfileListInfoKHR it its pNext chain or with
     * VK_IMAGE_CREATE_VIDEO_PROFILE_INDEPENDENT_BIT_KHR flag (if VK_KHR_VIDEO_MAINTENANCE_1 is supported,
     * which is not always the case).
     *
     * However the video profile is not known at D3D11_BIND_DECODER texture creation time.
     * D3D11 provides this information only when creating a VideoDecoderOutputView.
     *
     * Therefore the video decoder output view image is created without the video profile and
     * the dxvk decoder will copy decoded picture to it.
     *
     * If Vulkan implementation supports VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR
     * then the decoded picture has to be copied to the output image anyway.
     *
     * Otherwise the dxvk video decoder will use an internal output image and will copy it to
     * the video decoder output view.
     *
     * In either case the D3D11 video decoder output view is only used as a transfer destination.
     */
    Rc<DxvkImage> dxvkImage = GetCommonTexture(pResource)->GetImage();

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.aspect  = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = dxvkImage->info().usage & ~VK_IMAGE_USAGE_SAMPLED_BIT;

    switch (m_desc.ViewDimension) {
      case D3D11_VDOV_DIMENSION_TEXTURE2D:
        if (m_desc.Texture2D.ArraySlice >= dxvkImage->info().numLayers)
          throw DxvkError(str::format("Invalid video decoder output view ArraySlice ", m_desc.Texture2D.ArraySlice));

        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = 0;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = m_desc.Texture2D.ArraySlice;
        viewInfo.numLayers  = 1;
        break;

      case D3D11_VDOV_DIMENSION_UNKNOWN:
      default:
        throw DxvkError("Invalid view dimension");
    }

    m_view = pDevice->GetDXVKDevice()->createImageView(
      dxvkImage, viewInfo);
  }


  D3D11VideoDecoderOutputView::~D3D11VideoDecoderOutputView() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoDecoderOutputView::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11VideoDecoderOutputView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoDecoderOutputView), riid)) {
      Logger::warn("D3D11VideoDecoderOutputView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoDecoderOutputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_resource.ref();
  }


  void STDMETHODCALLTYPE D3D11VideoDecoderOutputView::GetDesc(
          D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
#endif




  D3D11VideoProcessorEnumerator::D3D11VideoProcessorEnumerator(
          D3D11Device*            pDevice,
    const D3D11_VIDEO_PROCESSOR_CONTENT_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorEnumerator>(pDevice),
    m_desc(Desc) {

  }


  D3D11VideoProcessorEnumerator::~D3D11VideoProcessorEnumerator() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11VideoProcessorEnumerator)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorEnumerator), riid)) {
      Logger::warn("D3D11VideoProcessorEnumerator::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorContentDesc(
          D3D11_VIDEO_PROCESSOR_CONTENT_DESC* pContentDesc) {
    *pContentDesc = m_desc;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::CheckVideoProcessorFormat(
          DXGI_FORMAT             Format,
          UINT*                   pFlags) {
    Logger::err(str::format("D3D11VideoProcessorEnumerator::CheckVideoProcessorFormat: stub, format ", Format));

    if (!pFlags)
      return E_INVALIDARG;

    *pFlags = D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT | D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCaps(
          D3D11_VIDEO_PROCESSOR_CAPS* pCaps) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorCaps: semi-stub");

    if (!pCaps)
      return E_INVALIDARG;

    *pCaps = {};
    pCaps->RateConversionCapsCount = 1;
    pCaps->MaxInputStreams = 52;
    pCaps->MaxStreamStates = 52;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps(
          UINT                    TypeIndex,
          D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS* pCaps) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorRateConversionCaps: semi-stub");
    if (!pCaps || TypeIndex)
      return E_INVALIDARG;

    *pCaps = {};
    if (m_desc.InputFrameFormat == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE) {
      pCaps->ProcessorCaps = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_FRAME_RATE_CONVERSION;
    } else {
      pCaps->ProcessorCaps = D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB;
      pCaps->PastFrames = 1;
      pCaps->FutureFrames = 1;
    }
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate(
          UINT                    TypeIndex,
          UINT                    CustomRateIndex,
          D3D11_VIDEO_PROCESSOR_CUSTOM_RATE* pRate) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorCustomRate: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange(
          D3D11_VIDEO_PROCESSOR_FILTER        Filter,
          D3D11_VIDEO_PROCESSOR_FILTER_RANGE* pRange) {
    Logger::err("D3D11VideoProcessorEnumerator::GetVideoProcessorFilterRange: Stub");
    return E_NOTIMPL;
  }




  D3D11VideoProcessor::D3D11VideoProcessor(
          D3D11Device*                    pDevice,
          D3D11VideoProcessorEnumerator*  pEnumerator,
          UINT                            RateConversionIndex)
  : D3D11DeviceChild<ID3D11VideoProcessor>(pDevice),
    m_enumerator(pEnumerator), m_rateConversionIndex(RateConversionIndex) {

  }


  D3D11VideoProcessor::~D3D11VideoProcessor() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessor::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11VideoProcessor)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessor), riid)) {
      Logger::warn("D3D11VideoProcessor::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessor::GetContentDesc(
          D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc) {
    m_enumerator->GetVideoProcessorContentDesc(pDesc);
  }


  void STDMETHODCALLTYPE D3D11VideoProcessor::GetRateConversionCaps(
          D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps) {
    m_enumerator->GetVideoProcessorRateConversionCaps(m_rateConversionIndex, pCaps);
  }




  D3D11VideoProcessorInputView::D3D11VideoProcessorInputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorInputView>(pDevice),
    m_resource(pResource), m_desc(Desc) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    Rc<DxvkImage> dxvkImage = GetCommonTexture(pResource)->GetImage();

    if (!(dxvkImage->info().usage & VK_IMAGE_USAGE_SAMPLED_BIT)) {
      DxvkImageCreateInfo info = dxvkImage->info();
      info.flags  = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
      info.usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      info.access = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      info.tiling = VK_IMAGE_TILING_OPTIMAL;
      info.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      info.shared = VK_FALSE;
      dxvkImage = m_copy = pDevice->GetDXVKDevice()->createImage(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);
    DXGI_VK_FORMAT_FAMILY formatFamily = pDevice->LookupFamily(resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    VkImageAspectFlags aspectMask = lookupFormatInfo(formatInfo.Format)->aspectMask;

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = VK_IMAGE_USAGE_SAMPLED_BIT;

    switch (m_desc.ViewDimension) {
      case D3D11_VPIV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = m_desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
#ifdef VBOX_WITH_DXVK_VIDEO
        viewInfo.minLayer   = m_desc.Texture2D.ArraySlice;
#else
        viewInfo.minLayer   = 0;
#endif
        viewInfo.numLayers  = 1;
        break;

      case D3D11_VPIV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    m_subresources.aspectMask = aspectMask;
    m_subresources.baseArrayLayer = viewInfo.minLayer;
    m_subresources.layerCount = viewInfo.numLayers;
    m_subresources.mipLevel = viewInfo.minLevel;

    for (uint32_t i = 0; aspectMask && i < m_views.size(); i++) {
      viewInfo.aspect = vk::getNextAspect(aspectMask);

      if (viewInfo.aspect != VK_IMAGE_ASPECT_COLOR_BIT)
        viewInfo.format = formatFamily.Formats[i];

      m_views[i] = pDevice->GetDXVKDevice()->createImageView(dxvkImage, viewInfo);
    }

    m_isYCbCr = IsYCbCrFormat(resourceDesc.Format);
  }


  D3D11VideoProcessorInputView::~D3D11VideoProcessorInputView() {

  }


  bool D3D11VideoProcessorInputView::IsYCbCrFormat(DXGI_FORMAT Format) {
    static const std::array<DXGI_FORMAT, 3> s_formats = {{
      DXGI_FORMAT_NV12,
      DXGI_FORMAT_YUY2,
      DXGI_FORMAT_AYUV,
    }};

    return std::find(s_formats.begin(), s_formats.end(), Format) != s_formats.end();
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorInputView::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11VideoProcessorInputView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorInputView), riid)) {
      Logger::warn("D3D11VideoProcessorInputView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_resource.ref();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorInputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }



  D3D11VideoProcessorOutputView::D3D11VideoProcessorOutputView(
          D3D11Device*            pDevice,
          ID3D11Resource*         pResource,
    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC& Desc)
  : D3D11DeviceChild<ID3D11VideoProcessorOutputView>(pDevice),
    m_resource(pResource), m_desc(Desc) {
    D3D11_COMMON_RESOURCE_DESC resourceDesc = { };
    GetCommonResourceDesc(pResource, &resourceDesc);

    DXGI_VK_FORMAT_INFO formatInfo = pDevice->LookupFormat(
      resourceDesc.Format, DXGI_VK_FORMAT_MODE_COLOR);

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format  = formatInfo.Format;
    viewInfo.aspect  = lookupFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.swizzle = formatInfo.Swizzle;
    viewInfo.usage   = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    switch (m_desc.ViewDimension) {
      case D3D11_VPOV_DIMENSION_TEXTURE2D:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.minLevel   = m_desc.Texture2D.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = 0;
        viewInfo.numLayers  = 1;
        break;

      case D3D11_VPOV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.type       = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.minLevel   = m_desc.Texture2DArray.MipSlice;
        viewInfo.numLevels  = 1;
        viewInfo.minLayer   = m_desc.Texture2DArray.FirstArraySlice;
        viewInfo.numLayers  = m_desc.Texture2DArray.ArraySize;
        break;

      case D3D11_VPOV_DIMENSION_UNKNOWN:
        throw DxvkError("Invalid view dimension");
    }

    m_view = pDevice->GetDXVKDevice()->createImageView(
      GetCommonTexture(pResource)->GetImage(), viewInfo);
  }


  D3D11VideoProcessorOutputView::~D3D11VideoProcessorOutputView() {

  }


  HRESULT STDMETHODCALLTYPE D3D11VideoProcessorOutputView::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11VideoProcessorOutputView)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11VideoProcessorOutputView), riid)) {
      Logger::warn("D3D11VideoProcessorOutputView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetResource(
          ID3D11Resource**        ppResource) {
    *ppResource = m_resource.ref();
  }


  void STDMETHODCALLTYPE D3D11VideoProcessorOutputView::GetDesc(
          D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }



  D3D11VideoContext::D3D11VideoContext(
          D3D11ImmediateContext*  pContext,
    const Rc<DxvkDevice>&         Device)
  : m_ctx(pContext), m_device(Device) {

  }


  D3D11VideoContext::~D3D11VideoContext() {

  }


  ULONG STDMETHODCALLTYPE D3D11VideoContext::AddRef() {
    return m_ctx->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D11VideoContext::Release() {
    return m_ctx->Release();
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_ctx->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetPrivateData(
          REFGUID                 Name,
          UINT*                   pDataSize,
          void*                   pData) {
    return m_ctx->GetPrivateData(Name, pDataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SetPrivateData(
          REFGUID                 Name,
          UINT                    DataSize,
    const void*                   pData)  {
    return m_ctx->SetPrivateData(Name, DataSize, pData);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SetPrivateDataInterface(
          REFGUID                 Name,
    const IUnknown*               pUnknown) {
    return m_ctx->SetPrivateDataInterface(Name, pUnknown);
  }


  void STDMETHODCALLTYPE D3D11VideoContext::GetDevice(
          ID3D11Device**          ppDevice) {
    return m_ctx->GetDevice(ppDevice);
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetDecoderBuffer(
          ID3D11VideoDecoder*             pDecoder,
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
          UINT*                           BufferSize,
          void**                          ppBuffer) {
#ifdef VBOX_WITH_DXVK_VIDEO
    auto videoDecoder = static_cast<D3D11VideoDecoder*>(pDecoder);
    return videoDecoder->GetDecoderBuffer(Type, BufferSize, ppBuffer);
#else
    Logger::err("D3D11VideoContext::GetDecoderBuffer: Stub");
    return E_NOTIMPL;
#endif
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ReleaseDecoderBuffer(
          ID3D11VideoDecoder*             pDecoder,
          D3D11_VIDEO_DECODER_BUFFER_TYPE Type) {
#ifdef VBOX_WITH_DXVK_VIDEO
    auto videoDecoder = static_cast<D3D11VideoDecoder*>(pDecoder);
    return videoDecoder->ReleaseDecoderBuffer(Type);
#else
    Logger::err("D3D11VideoContext::ReleaseDecoderBuffer: Stub");
    return E_NOTIMPL;
#endif
  }

  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderBeginFrame(
          ID3D11VideoDecoder*             pDecoder,
          ID3D11VideoDecoderOutputView*   pView,
          UINT                            KeySize,
    const void*                           pKey) {
#ifdef VBOX_WITH_DXVK_VIDEO
    auto videoDecoder = static_cast<D3D11VideoDecoder*>(pDecoder);
    auto dxvkDecoder = videoDecoder->GetDecoder();
    auto dxvkView = static_cast<D3D11VideoDecoderOutputView*>(pView)->GetView();

    m_ctx->EmitCs([
      cDecoder = dxvkDecoder,
      cView = dxvkView
    ] (DxvkContext* ctx) {
      cDecoder->BeginFrame(ctx, cView);
    });
    return S_OK;
#else
    Logger::err("D3D11VideoContext::DecoderBeginFrame: Stub");
    return E_NOTIMPL;
#endif
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderEndFrame(
          ID3D11VideoDecoder*             pDecoder) {
#ifdef VBOX_WITH_DXVK_VIDEO
    auto videoDecoder = static_cast<D3D11VideoDecoder*>(pDecoder);
    auto dxvkDecoder = videoDecoder->GetDecoder();

    m_ctx->EmitCs([
      cDecoder = dxvkDecoder
    ] (DxvkContext* ctx) {
      cDecoder->EndFrame(ctx);
    });
    return S_OK;
#else
    Logger::err("D3D11VideoContext::DecoderEndFrame: Stub");
    return E_NOTIMPL;
#endif
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::SubmitDecoderBuffers(
          ID3D11VideoDecoder*             pDecoder,
          UINT                            BufferCount,
    const D3D11_VIDEO_DECODER_BUFFER_DESC* pBufferDescs) {
#ifdef VBOX_WITH_DXVK_VIDEO
    auto videoDecoder = static_cast<D3D11VideoDecoder*>(pDecoder);
    auto dxvkDecoder = videoDecoder->GetDecoder();

    DxvkVideoDecodeInputParameters parms;
    if (!videoDecoder->GetVideoDecodeH264InputParameters(BufferCount, pBufferDescs, &parms))
       return E_INVALIDARG;

    m_ctx->EmitCs([
      cDecoder = dxvkDecoder,
      cParms = parms
    ] (DxvkContext* ctx) {
      cDecoder->Decode(ctx, cParms);
    });

    return S_OK;
#else
    Logger::err("D3D11VideoContext::SubmitDecoderBuffers: Stub");
    return E_NOTIMPL;
#endif
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::DecoderExtension(
          ID3D11VideoDecoder*             pDecoder,
    const D3D11_VIDEO_DECODER_EXTENSION*  pExtension) {
    Logger::err("D3D11VideoContext::DecoderExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputTargetRectEnabled = Enable;

    if (Enable)
      state->outputTargetRect = *pRect;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetOutputTargetRect: Stub.");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            YCbCr,
    const D3D11_VIDEO_COLOR*              pColor) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputBackgroundColorIsYCbCr = YCbCr;
    state->outputBackgroundColor = *pColor;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetOutputBackgroundColor: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputColorSpace = *pColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputAlphaFillMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
          UINT                            StreamIndex) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable,
          SIZE                            Size) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputConstriction: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputStereoMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL                            Enable) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    state->outputStereoModeEnabled = Enable;

    if (Enable)
      Logger::err("D3D11VideoContext: Stereo output not supported");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetOutputExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorSetOutputExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamFrameFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_FRAME_FORMAT        Format) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->frameFormat = Format;

    if (Format != D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE)
      Logger::err(str::format("D3D11VideoContext: Unsupported frame format: ", Format));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->colorSpace = *pColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamOutputRate(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_OUTPUT_RATE Rate,
          BOOL                            Repeat,
    const DXGI_RATIONAL*                  CustomRate) {
    Logger::err(str::format("D3D11VideoContext::VideoProcessorSetStreamOutputRate: Stub, Rate ", Rate));
    if (CustomRate)
      Logger::err(str::format("CustomRate ", CustomRate->Numerator, "/", CustomRate->Denominator));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamSourceRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->srcRectEnabled = Enable;

    if (Enable)
      state->srcRect = *pRect;

    static bool errorShown = false;

    if (!std::exchange(errorShown, true))
      Logger::err("D3D11VideoContext::VideoProcessorSetStreamSourceRect: Stub.");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->dstRectEnabled = Enable;

    if (Enable)
      state->dstRect = *pRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamAlpha(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          FLOAT                           Alpha) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
    const UINT*                           pEntries) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
    const DXGI_RATIONAL*                  pSrcAspectRatio,
    const DXGI_RATIONAL*                  pDstAspectRatio) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          FLOAT                           Lower,
          FLOAT                           Upper) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamLumaKey: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamStereoFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
          BOOL                            LeftViewFrame0,
          BOOL                            BaseViewFrame0,
          D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
          int                             MonoOffset) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamStereoFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamAutoProcessingMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->autoProcessingEnabled = Enable;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamFilter(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_FILTER    Filter,
          BOOL                            Enable,
          int                             Level) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorSetStreamExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorSetStreamRotation(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL                            Enable,
          D3D11_VIDEO_PROCESSOR_ROTATION  Rotation) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    state->rotationEnabled = Enable;
    state->rotation = Rotation;

    if (Enable && Rotation != D3D11_VIDEO_PROCESSOR_ROTATION_IDENTITY)
      Logger::err(str::format("D3D11VideoContext: Unsupported rotation: ", Rotation));
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputTargetRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pEnabled)
      *pEnabled = state->outputTargetRectEnabled;

    if (pRect)
      *pRect = state->outputTargetRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputBackgroundColor(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pYCbCr,
          D3D11_VIDEO_COLOR*              pColor) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();
    
    if (pYCbCr)
      *pYCbCr = state->outputBackgroundColorIsYCbCr;

    if (pColor)
      *pColor = state->outputBackgroundColor;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pColorSpace)
      *pColorSpace = state->outputColorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputAlphaFillMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE* pAlphaFillMode,
          UINT*                           pStreamIndex) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputAlphaFillMode: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputConstriction(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled,
          SIZE*                           pSize) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputConstriction: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputStereoMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          BOOL*                           pEnabled) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetState();

    if (pEnabled)
      *pEnabled = state->outputStereoModeEnabled;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetOutputExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorGetOutputExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamFrameFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_FRAME_FORMAT*       pFormat) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pFormat)
      *pFormat = state->frameFormat;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamColorSpace(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pColorSpace)
      *pColorSpace = state->colorSpace;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamOutputRate(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pRate,
          BOOL*                           pRepeat,
          DXGI_RATIONAL*                  pCustomRate) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamOutputRate: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamSourceRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnabled)
      *pEnabled = state->srcRectEnabled;

    if (pRect)
      *pRect = state->srcRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamDestRect(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          RECT*                           pRect) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnabled)
      *pEnabled = state->dstRectEnabled;

    if (pRect)
      *pRect = state->dstRect;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamAlpha(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          FLOAT*                          pAlpha) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamAlpha: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPalette(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          UINT                            EntryCount,
          UINT*                           pEntries) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamPalette: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          DXGI_RATIONAL*                  pSrcAspectRatio,
          DXGI_RATIONAL*                  pDstAspectRatio) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamPixelAspectRatio: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamLumaKey(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          FLOAT*                          pLower,
          FLOAT*                          pUpper) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamLumaKey: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamStereoFormat(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled,
          D3D11_VIDEO_PROCESSOR_STEREO_FORMAT* pFormat,
          BOOL*                           pLeftViewFrame0,
          BOOL*                           pBaseViewFrame0,
          D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE* pFlipMode,
          int*                            pMonoOffset) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamStereoFormat: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamAutoProcessingMode(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnabled) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    *pEnabled = state->autoProcessingEnabled;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamFilter(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          D3D11_VIDEO_PROCESSOR_FILTER    Filter,
          BOOL*                           pEnabled,
          int*                            pLevel) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamFilter: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamExtension(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
    const GUID*                           pExtensionGuid,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::VideoProcessorGetStreamExtension: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorGetStreamRotation(
          ID3D11VideoProcessor*           pVideoProcessor,
          UINT                            StreamIndex,
          BOOL*                           pEnable,
          D3D11_VIDEO_PROCESSOR_ROTATION* pRotation) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto state = static_cast<D3D11VideoProcessor*>(pVideoProcessor)->GetStreamState(StreamIndex);

    if (!state)
      return;

    if (pEnable)
      *pEnable = state->rotationEnabled;

    if (pRotation)
      *pRotation = state->rotation;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::VideoProcessorBlt(
          ID3D11VideoProcessor*           pVideoProcessor,
          ID3D11VideoProcessorOutputView* pOutputView,
          UINT                            FrameIdx,
          UINT                            StreamCount,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStreams) {
    D3D10DeviceLock lock = m_ctx->LockContext();

    auto videoProcessor = static_cast<D3D11VideoProcessor*>(pVideoProcessor);
    bool hasStreamsEnabled = false;

    // Resetting and restoring all context state incurs
    // a lot of overhead, so only do it as necessary
    for (uint32_t i = 0; i < StreamCount; i++) {
      auto streamState = videoProcessor->GetStreamState(i);

      if (!pStreams[i].Enable || !streamState)
        continue;

      if (!hasStreamsEnabled) {
        m_ctx->ResetCommandListState();
        BindOutputView(pOutputView);
        hasStreamsEnabled = true;
      }

      BlitStream(streamState, &pStreams[i]);
    }

    if (hasStreamsEnabled) {
      UnbindResources();
      m_ctx->RestoreCommandListState();
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateCryptoSessionKeyExchange(
          ID3D11CryptoSession*            pSession,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::NegotiateCryptoSessionKeyExchange: Stub");
    return E_NOTIMPL;
  }


  void STDMETHODCALLTYPE D3D11VideoContext::EncryptionBlt(
          ID3D11CryptoSession*            pSession,
          ID3D11Texture2D*                pSrcSurface,
          ID3D11Texture2D*                pDstSurface,
          UINT                            IVSize,
          void*                           pIV) {
    Logger::err("D3D11VideoContext::EncryptionBlt: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::DecryptionBlt(
          ID3D11CryptoSession*            pSession,
          ID3D11Texture2D*                pSrcSurface,
          ID3D11Texture2D*                pDstSurface,
          D3D11_ENCRYPTED_BLOCK_INFO*     pBlockInfo,
          UINT                            KeySize,
    const void*                           pKey,
          UINT                            IVSize,
          void*                           pIV) {
    Logger::err("D3D11VideoContext::DecryptionBlt: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::StartSessionKeyRefresh(
          ID3D11CryptoSession*            pSession,
          UINT                            RandomNumberSize,
          void*                           pRandomNumber) {
    Logger::err("D3D11VideoContext::StartSessionKeyRefresh: Stub");
  }


  void STDMETHODCALLTYPE D3D11VideoContext::FinishSessionKeyRefresh(
          ID3D11CryptoSession*            pSession) {
    Logger::err("D3D11VideoContext::FinishSessionKeyRefresh: Stub");
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::GetEncryptionBltKey(
          ID3D11CryptoSession*            pSession,
          UINT                            KeySize,
          void*                           pKey) {
    Logger::err("D3D11VideoContext::GetEncryptionBltKey: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            DataSize,
          void*                           pData) {
    Logger::err("D3D11VideoContext::NegotiateAuthenticatedChannelKeyExchange: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::QueryAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          UINT                            OutputSize,
          void*                           pOutput) {
    Logger::err("D3D11VideoContext::QueryAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE D3D11VideoContext::ConfigureAuthenticatedChannel(
          ID3D11AuthenticatedChannel*     pChannel,
          UINT                            InputSize,
    const void*                           pInput,
          D3D11_AUTHENTICATED_CONFIGURE_OUTPUT* pOutput) {
    Logger::err("D3D11VideoContext::ConfigureAuthenticatedChannel: Stub");
    return E_NOTIMPL;
  }


  void D3D11VideoContext::ApplyColorMatrix(float pDst[3][4], const float pSrc[3][4]) {
    float result[3][4];

    for (uint32_t i = 0; i < 3; i++) {
      for (uint32_t j = 0; j < 4; j++) {
        result[i][j] = pSrc[i][0] * pDst[0][j]
                     + pSrc[i][1] * pDst[1][j]
                     + pSrc[i][2] * pDst[2][j]
                     + pSrc[i][3] * float(j == 3);
      }
    }

    memcpy(pDst, &result[0][0], sizeof(result));
  }


  void D3D11VideoContext::ApplyYCbCrMatrix(float pColorMatrix[3][4], bool UseBt709) {
    static const float pretransform[3][4] = {
      { 0.0f, 1.0f, 0.0f,  0.0f },
      { 0.0f, 0.0f, 1.0f, -0.5f },
      { 1.0f, 0.0f, 0.0f, -0.5f },
    };

    static const float bt601[3][4] = {
      { 1.0f,  0.000000f,  1.402000f, 0.0f },
      { 1.0f, -0.344136f, -0.714136f, 0.0f },
      { 1.0f,  1.772000f,  0.000000f, 0.0f },
    };

    static const float bt709[3][4] = {
      { 1.0f,  0.000000f,  1.574800f, 0.0f },
      { 1.0f, -0.187324f, -0.468124f, 0.0f },
      { 1.0f,  1.855600f,  0.000000f, 0.0f },
    };

    ApplyColorMatrix(pColorMatrix, pretransform);
    ApplyColorMatrix(pColorMatrix, UseBt709 ? bt709 : bt601);
  }


  void D3D11VideoContext::BindOutputView(
          ID3D11VideoProcessorOutputView* pOutputView) {
    auto dxvkView = static_cast<D3D11VideoProcessorOutputView*>(pOutputView)->GetView();

    m_ctx->EmitCs([this, cView = dxvkView] (DxvkContext* ctx) {
      DxvkRenderTargets rt;
      rt.color[0].view = cView;
      rt.color[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      ctx->bindRenderTargets(std::move(rt), 0u);

      DxvkInputAssemblyState iaState;
      iaState.primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      iaState.primitiveRestart = VK_FALSE;
      iaState.patchVertexCount = 0;
      ctx->setInputAssemblyState(iaState);
    });

    VkExtent3D viewExtent = dxvkView->mipLevelExtent(0);
    m_dstExtent = { viewExtent.width, viewExtent.height };
  }


  void D3D11VideoContext::BlitStream(
    const D3D11VideoProcessorStreamState* pStreamState,
    const D3D11_VIDEO_PROCESSOR_STREAM*   pStream) {
    CreateResources();

    if (pStream->PastFrames || pStream->FutureFrames)
      Logger::err("D3D11VideoContext: Ignoring non-zero PastFrames and FutureFrames");

    if (pStream->OutputIndex)
      Logger::err("D3D11VideoContext: Ignoring non-zero OutputIndex");

    if (pStream->InputFrameOrField)
      Logger::err("D3D11VideoContext: Ignoring non-zero InputFrameOrField");

    auto view = static_cast<D3D11VideoProcessorInputView*>(pStream->pInputSurface);

    if (view->NeedsCopy()) {
      m_ctx->EmitCs([
        cDstImage     = view->GetShadowCopy(),
        cSrcImage     = view->GetImage(),
        cSrcLayers    = view->GetImageSubresources()
      ] (DxvkContext* ctx) {
        VkImageSubresourceLayers cDstLayers;
        cDstLayers.aspectMask = cSrcLayers.aspectMask;
        cDstLayers.baseArrayLayer = 0;
        cDstLayers.layerCount = cSrcLayers.layerCount;
        cDstLayers.mipLevel = cSrcLayers.mipLevel;

        ctx->copyImage(
          cDstImage, cDstLayers, VkOffset3D(),
          cSrcImage, cSrcLayers, VkOffset3D(),
          cDstImage->info().extent);
      });
    }

    m_ctx->EmitCs([this,
      cStreamState  = *pStreamState,
      cViews        = view->GetViews(),
      cIsYCbCr      = view->IsYCbCr()
    ] (DxvkContext* ctx) {
      VkViewport viewport;
      viewport.x        = 0.0f;
      viewport.y        = 0.0f;
      viewport.width    = float(m_dstExtent.width);
      viewport.height   = float(m_dstExtent.height);
      viewport.minDepth = 0.0f;
      viewport.maxDepth = 1.0f;

      VkRect2D scissor;
      scissor.offset = { 0, 0 };
      scissor.extent = m_dstExtent;

      if (cStreamState.dstRectEnabled) {
        viewport.x      = float(cStreamState.dstRect.left);
        viewport.y      = float(cStreamState.dstRect.top);
        viewport.width  = float(cStreamState.dstRect.right) - viewport.x;
        viewport.height = float(cStreamState.dstRect.bottom) - viewport.y;
      }

      UboData uboData = { };
      uboData.colorMatrix[0][0] = 1.0f;
      uboData.colorMatrix[1][1] = 1.0f;
      uboData.colorMatrix[2][2] = 1.0f;
      uboData.coordMatrix[0][0] = 1.0f;
      uboData.coordMatrix[1][1] = 1.0f;
      uboData.yMin = 0.0f;
      uboData.yMax = 1.0f;
      uboData.isPlanar = cViews[1] != nullptr;

      if (cIsYCbCr)
        ApplyYCbCrMatrix(uboData.colorMatrix, cStreamState.colorSpace.YCbCr_Matrix);

      if (cStreamState.colorSpace.Nominal_Range) {
        uboData.yMin = 0.0627451f;
        uboData.yMax = 0.9215686f;
      }

      DxvkBufferSliceHandle uboSlice = m_ubo->allocSlice();
      memcpy(uboSlice.mapPtr, &uboData, sizeof(uboData));

      ctx->invalidateBuffer(m_ubo, uboSlice);
      ctx->setViewports(1, &viewport, &scissor);

      ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(Rc<DxvkShader>(m_vs));
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(Rc<DxvkShader>(m_fs));

      ctx->bindUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, DxvkBufferSlice(m_ubo));
      ctx->bindResourceSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, Rc<DxvkSampler>(m_sampler));

      for (uint32_t i = 0; i < cViews.size(); i++)
        ctx->bindResourceImageView(VK_SHADER_STAGE_FRAGMENT_BIT, 2 + i, Rc<DxvkImageView>(cViews[i]));

      ctx->draw(3, 1, 0, 0);

      ctx->bindResourceSampler(VK_SHADER_STAGE_FRAGMENT_BIT, 1, nullptr);

      for (uint32_t i = 0; i < cViews.size(); i++)
        ctx->bindResourceImageView(VK_SHADER_STAGE_FRAGMENT_BIT, 2 + i, nullptr);
    });
  }


  void D3D11VideoContext::CreateUniformBuffer() {
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size = sizeof(UboData);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    bufferInfo.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_ubo = m_device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  void D3D11VideoContext::CreateSampler() {
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 0.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_ALWAYS;
    samplerInfo.reductionMode   = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    samplerInfo.borderColor     = VkClearColorValue();
    samplerInfo.usePixelCoord   = VK_FALSE;
    samplerInfo.nonSeamless     = VK_FALSE;
    m_sampler = m_device->createSampler(samplerInfo);
  }


  void D3D11VideoContext::CreateShaders() {
    SpirvCodeBuffer vsCode(d3d11_video_blit_vert);
    SpirvCodeBuffer fsCode(d3d11_video_blit_frag);

    const std::array<DxvkBindingInfo, 4> fsBindings = {{
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_TRUE },
      { VK_DESCRIPTOR_TYPE_SAMPLER,        1, VK_IMAGE_VIEW_TYPE_MAX_ENUM, VK_SHADER_STAGE_FRAGMENT_BIT, 0 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  2, VK_IMAGE_VIEW_TYPE_2D,       VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_SHADER_READ_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  3, VK_IMAGE_VIEW_TYPE_2D,       VK_SHADER_STAGE_FRAGMENT_BIT, VK_ACCESS_SHADER_READ_BIT },
    }};

    DxvkShaderCreateInfo vsInfo;
    vsInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vsInfo.outputMask = 0x1;
    m_vs = new DxvkShader(vsInfo, std::move(vsCode));

    DxvkShaderCreateInfo fsInfo;
    fsInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fsInfo.bindingCount = fsBindings.size();
    fsInfo.bindings = fsBindings.data();
    fsInfo.inputMask = 0x1;
    fsInfo.outputMask = 0x1;
    m_fs = new DxvkShader(fsInfo, std::move(fsCode));
  }


  void D3D11VideoContext::CreateResources() {
    if (std::exchange(m_resourcesCreated, true))
      return;

    CreateSampler();
    CreateUniformBuffer();
    CreateShaders();
  }


  void D3D11VideoContext::UnbindResources() {
    m_ctx->EmitCs([] (DxvkContext* ctx) {
      ctx->bindRenderTargets(DxvkRenderTargets(), 0u);

      ctx->bindShader<VK_SHADER_STAGE_VERTEX_BIT>(nullptr);
      ctx->bindShader<VK_SHADER_STAGE_FRAGMENT_BIT>(nullptr);

      ctx->bindUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, 0, DxvkBufferSlice());
    });
  }

}
