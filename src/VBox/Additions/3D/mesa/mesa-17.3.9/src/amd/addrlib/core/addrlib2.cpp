/*
 * Copyright © 2017 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

/**
************************************************************************************************************************
* @file  addrlib2.cpp
* @brief Contains the implementation for the AddrLib2 base class.
************************************************************************************************************************
*/

#include "addrinterface.h"
#include "addrlib2.h"
#include "addrcommon.h"

namespace Addr
{
namespace V2
{

////////////////////////////////////////////////////////////////////////////////////////////////////
//                               Static Const Member
////////////////////////////////////////////////////////////////////////////////////////////////////

const Dim2d Lib::Block256_2d[] = {{16, 16}, {16, 8}, {8, 8}, {8, 4}, {4, 4}};

const Dim3d Lib::Block1K_3d[]  = {{16, 8, 8}, {8, 8, 8}, {8, 8, 4}, {8, 4, 4}, {4, 4, 4}};

////////////////////////////////////////////////////////////////////////////////////////////////////
//                               Constructor/Destructor
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
************************************************************************************************************************
*   Lib::Lib
*
*   @brief
*       Constructor for the Addr::V2::Lib class
*
************************************************************************************************************************
*/
Lib::Lib()
    :
    Addr::Lib()
{
}

/**
************************************************************************************************************************
*   Lib::Lib
*
*   @brief
*       Constructor for the AddrLib2 class with hClient as parameter
*
************************************************************************************************************************
*/
Lib::Lib(const Client* pClient)
    :
    Addr::Lib(pClient)
{
}

/**
************************************************************************************************************************
*   Lib::~Lib
*
*   @brief
*       Destructor for the AddrLib2 class
*
************************************************************************************************************************
*/
Lib::~Lib()
{
}

/**
************************************************************************************************************************
*   Lib::GetLib
*
*   @brief
*       Get Addr::V2::Lib pointer
*
*   @return
*      An Addr::V2::Lib class pointer
************************************************************************************************************************
*/
Lib* Lib::GetLib(
    ADDR_HANDLE hLib)   ///< [in] handle of ADDR_HANDLE
{
    Addr::Lib* pAddrLib = Addr::Lib::GetLib(hLib);
    if ((pAddrLib != NULL) &&
        (pAddrLib->GetChipFamily() <= ADDR_CHIP_FAMILY_VI))
    {
        // only valid and GFX9+ AISC can use AddrLib2 function.
        ADDR_ASSERT_ALWAYS();
        hLib = NULL;
    }
    return static_cast<Lib*>(hLib);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//                               Surface Methods
////////////////////////////////////////////////////////////////////////////////////////////////////


/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfo
*
*   @brief
*       Interface function stub of AddrComputeSurfaceInfo.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceInfo(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (GetFillSizeFieldsFlags() == TRUE)
    {
        if ((pIn->size != sizeof(ADDR2_COMPUTE_SURFACE_INFO_INPUT)) ||
            (pOut->size != sizeof(ADDR2_COMPUTE_SURFACE_INFO_OUTPUT)))
        {
            returnCode = ADDR_PARAMSIZEMISMATCH;
        }
    }

    // Adjust coming parameters.
    ADDR2_COMPUTE_SURFACE_INFO_INPUT localIn = *pIn;
    localIn.width        = Max(pIn->width, 1u);
    localIn.height       = Max(pIn->height, 1u);
    localIn.numMipLevels = Max(pIn->numMipLevels, 1u);
    localIn.numSlices    = Max(pIn->numSlices, 1u);
    localIn.numSamples   = Max(pIn->numSamples, 1u);
    localIn.numFrags     = (localIn.numFrags == 0) ? localIn.numSamples : pIn->numFrags;

    UINT_32  expandX  = 1;
    UINT_32  expandY  = 1;
    ElemMode elemMode = ADDR_UNCOMPRESSED;

    if (returnCode == ADDR_OK)
    {
        // Set format to INVALID will skip this conversion
        if (localIn.format != ADDR_FMT_INVALID)
        {
            // Get compression/expansion factors and element mode which indicates compression/expansion
            localIn.bpp = GetElemLib()->GetBitsPerPixel(localIn.format,
                                                        &elemMode,
                                                        &expandX,
                                                        &expandY);

            // Special flag for 96 bit surface. 96 (or 48 if we support) bit surface's width is
            // pre-multiplied by 3 and bpp is divided by 3. So pitch alignment for linear-
            // aligned does not meet 64-pixel in real. We keep special handling in hwl since hw
            // restrictions are different.
            // Also Mip 1+ needs an element pitch of 32 bits so we do not need this workaround
            // but we use this flag to skip RestoreSurfaceInfo below

            if ((elemMode == ADDR_EXPANDED) && (expandX > 1))
            {
                ADDR_ASSERT(IsLinear(localIn.swizzleMode));
            }

            UINT_32 basePitch = 0;
            GetElemLib()->AdjustSurfaceInfo(elemMode,
                                            expandX,
                                            expandY,
                                            &localIn.bpp,
                                            &basePitch,
                                            &localIn.width,
                                            &localIn.height);

            // Overwrite these parameters if we have a valid format
        }

        if (localIn.bpp != 0)
        {
            localIn.width  = Max(localIn.width, 1u);
            localIn.height = Max(localIn.height, 1u);
        }
        else // Rule out some invalid parameters
        {
            ADDR_ASSERT_ALWAYS();

            returnCode = ADDR_INVALIDPARAMS;
        }
    }

    if (returnCode == ADDR_OK)
    {
        returnCode = ComputeSurfaceInfoSanityCheck(&localIn);
    }

    if (returnCode == ADDR_OK)
    {
        VerifyMipLevelInfo(pIn);

        if (IsLinear(pIn->swizzleMode))
        {
            // linear mode
            returnCode = ComputeSurfaceInfoLinear(&localIn, pOut);
        }
        else
        {
            // tiled mode
            returnCode = ComputeSurfaceInfoTiled(&localIn, pOut);
        }

        if (returnCode == ADDR_OK)
        {
            pOut->bpp = localIn.bpp;
            pOut->pixelPitch = pOut->pitch;
            pOut->pixelHeight = pOut->height;
            pOut->pixelMipChainPitch = pOut->mipChainPitch;
            pOut->pixelMipChainHeight = pOut->mipChainHeight;
            pOut->pixelBits = localIn.bpp;

            if (localIn.format != ADDR_FMT_INVALID)
            {
                UINT_32 pixelBits = pOut->pixelBits;

                GetElemLib()->RestoreSurfaceInfo(elemMode,
                                                 expandX,
                                                 expandY,
                                                 &pOut->pixelBits,
                                                 &pOut->pixelPitch,
                                                 &pOut->pixelHeight);

                GetElemLib()->RestoreSurfaceInfo(elemMode,
                                                 expandX,
                                                 expandY,
                                                 &pixelBits,
                                                 &pOut->pixelMipChainPitch,
                                                 &pOut->pixelMipChainHeight);

                if ((localIn.numMipLevels > 1) && (pOut->pMipInfo != NULL))
                {
                    for (UINT_32 i = 0; i < localIn.numMipLevels; i++)
                    {
                        pOut->pMipInfo[i].pixelPitch  = pOut->pMipInfo[i].pitch;
                        pOut->pMipInfo[i].pixelHeight = pOut->pMipInfo[i].height;

                        GetElemLib()->RestoreSurfaceInfo(elemMode,
                                                         expandX,
                                                         expandY,
                                                         &pixelBits,
                                                         &pOut->pMipInfo[i].pixelPitch,
                                                         &pOut->pMipInfo[i].pixelHeight);
                    }
                }
            }

            if (localIn.flags.needEquation && (Log2(localIn.numFrags) == 0))
            {
                pOut->equationIndex = GetEquationIndex(&localIn, pOut);
            }

            if (localIn.flags.qbStereo)
            {
                if (pOut->pStereoInfo != NULL)
                {
                    ComputeQbStereoInfo(pOut);
                }
            }
        }
    }

    ADDR_ASSERT(pOut->surfSize != 0);

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfo
*
*   @brief
*       Interface function stub of AddrComputeSurfaceInfo.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceAddrFromCoord(
    const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (GetFillSizeFieldsFlags() == TRUE)
    {
        if ((pIn->size != sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT)) ||
            (pOut->size != sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT)))
        {
            returnCode = ADDR_PARAMSIZEMISMATCH;
        }
    }

    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT localIn = *pIn;
    localIn.unalignedWidth  = Max(pIn->unalignedWidth, 1u);
    localIn.unalignedHeight = Max(pIn->unalignedHeight, 1u);
    localIn.numMipLevels    = Max(pIn->numMipLevels, 1u);
    localIn.numSlices       = Max(pIn->numSlices, 1u);
    localIn.numSamples      = Max(pIn->numSamples, 1u);
    localIn.numFrags        = Max(pIn->numFrags, 1u);

    if ((localIn.bpp < 8)        ||
        (localIn.bpp > 128)      ||
        ((localIn.bpp % 8) != 0) ||
        (localIn.sample >= localIn.numSamples)  ||
        (localIn.slice >= localIn.numSlices)    ||
        (localIn.mipId >= localIn.numMipLevels) ||
        (IsTex3d(localIn.resourceType) &&
         (Valid3DMipSliceIdConstraint(localIn.numSlices, localIn.mipId, localIn.slice) == FALSE)))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }

    if (returnCode == ADDR_OK)
    {
        if (IsLinear(localIn.swizzleMode))
        {
            returnCode = ComputeSurfaceAddrFromCoordLinear(&localIn, pOut);
        }
        else
        {
            returnCode = ComputeSurfaceAddrFromCoordTiled(&localIn, pOut);
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceCoordFromAddr
*
*   @brief
*       Interface function stub of ComputeSurfaceCoordFromAddr.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceCoordFromAddr(
    const ADDR2_COMPUTE_SURFACE_COORDFROMADDR_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (GetFillSizeFieldsFlags() == TRUE)
    {
        if ((pIn->size != sizeof(ADDR2_COMPUTE_SURFACE_COORDFROMADDR_INPUT)) ||
            (pOut->size != sizeof(ADDR2_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT)))
        {
            returnCode = ADDR_PARAMSIZEMISMATCH;
        }
    }

    if ((pIn->bpp < 8)        ||
        (pIn->bpp > 128)      ||
        ((pIn->bpp % 8) != 0) ||
        (pIn->bitPosition >= 8))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }

    if (returnCode == ADDR_OK)
    {
        if (IsLinear(pIn->swizzleMode))
        {
            returnCode = ComputeSurfaceCoordFromAddrLinear(pIn, pOut);
        }
        else
        {
            returnCode = ComputeSurfaceCoordFromAddrTiled(pIn, pOut);
        }
    }

    return returnCode;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//                               CMASK/HTILE
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
************************************************************************************************************************
*   Lib::ComputeHtileInfo
*
*   @brief
*       Interface function stub of AddrComputeHtilenfo
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeHtileInfo(
    const ADDR2_COMPUTE_HTILE_INFO_INPUT*    pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_INFO_OUTPUT*         pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_HTILE_INFO_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_HTILE_INFO_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeHtileInfo(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeHtileAddrFromCoord
*
*   @brief
*       Interface function stub of AddrComputeHtileAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeHtileAddrFromCoord(
    const ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_INPUT*   pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_OUTPUT*        pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_HTILE_ADDRFROMCOORD_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeHtileAddrFromCoord(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeHtileCoordFromAddr
*
*   @brief
*       Interface function stub of AddrComputeHtileCoordFromAddr
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeHtileCoordFromAddr(
    const ADDR2_COMPUTE_HTILE_COORDFROMADDR_INPUT*   pIn,    ///< [in] input structure
    ADDR2_COMPUTE_HTILE_COORDFROMADDR_OUTPUT*        pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_HTILE_COORDFROMADDR_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_HTILE_COORDFROMADDR_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeHtileCoordFromAddr(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeCmaskInfo
*
*   @brief
*       Interface function stub of AddrComputeCmaskInfo
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeCmaskInfo(
    const ADDR2_COMPUTE_CMASK_INFO_INPUT*    pIn,    ///< [in] input structure
    ADDR2_COMPUTE_CMASK_INFO_OUTPUT*         pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_CMASK_INFO_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_CMASK_INFO_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else if (pIn->cMaskFlags.linear)
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeCmaskInfo(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeCmaskAddrFromCoord
*
*   @brief
*       Interface function stub of AddrComputeCmaskAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeCmaskAddrFromCoord(
    const ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_INPUT*   pIn,    ///< [in] input structure
    ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_OUTPUT*        pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_CMASK_ADDRFROMCOORD_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeCmaskAddrFromCoord(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeCmaskCoordFromAddr
*
*   @brief
*       Interface function stub of AddrComputeCmaskCoordFromAddr
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeCmaskCoordFromAddr(
    const ADDR2_COMPUTE_CMASK_COORDFROMADDR_INPUT*   pIn,    ///< [in] input structure
    ADDR2_COMPUTE_CMASK_COORDFROMADDR_OUTPUT*        pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_NOTIMPLEMENTED;

    ADDR_NOT_IMPLEMENTED();

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeFmaskInfo
*
*   @brief
*       Interface function stub of ComputeFmaskInfo.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeFmaskInfo(
    const ADDR2_COMPUTE_FMASK_INFO_INPUT*    pIn,    ///< [in] input structure
    ADDR2_COMPUTE_FMASK_INFO_OUTPUT*         pOut    ///< [out] output structure
    )
{
    ADDR_E_RETURNCODE returnCode;

    BOOL_32 valid = (IsZOrderSwizzle(pIn->swizzleMode) == TRUE) &&
                    ((pIn->numSamples > 0) || (pIn->numFrags > 0));

    if (GetFillSizeFieldsFlags())
    {
        if ((pIn->size != sizeof(ADDR2_COMPUTE_FMASK_INFO_INPUT)) ||
            (pOut->size != sizeof(ADDR2_COMPUTE_FMASK_INFO_OUTPUT)))
        {
            valid = FALSE;
        }
    }

    if (valid == FALSE)
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        ADDR2_COMPUTE_SURFACE_INFO_INPUT  localIn = {0};
        ADDR2_COMPUTE_SURFACE_INFO_OUTPUT localOut = {0};

        localIn.size = sizeof(ADDR2_COMPUTE_SURFACE_INFO_INPUT);
        localOut.size = sizeof(ADDR2_COMPUTE_SURFACE_INFO_OUTPUT);

        localIn.swizzleMode  = pIn->swizzleMode;
        localIn.numSlices    = Max(pIn->numSlices, 1u);
        localIn.width        = Max(pIn->unalignedWidth, 1u);
        localIn.height       = Max(pIn->unalignedHeight, 1u);
        localIn.bpp          = GetFmaskBpp(pIn->numSamples, pIn->numFrags);
        localIn.flags.fmask  = 1;
        localIn.numFrags     = 1;
        localIn.numSamples   = 1;
        localIn.resourceType = ADDR_RSRC_TEX_2D;

        if (localIn.bpp == 8)
        {
            localIn.format = ADDR_FMT_8;
        }
        else if (localIn.bpp == 16)
        {
            localIn.format = ADDR_FMT_16;
        }
        else if (localIn.bpp == 32)
        {
            localIn.format = ADDR_FMT_32;
        }
        else
        {
            localIn.format = ADDR_FMT_32_32;
        }

        returnCode = ComputeSurfaceInfo(&localIn, &localOut);

        if (returnCode == ADDR_OK)
        {
            pOut->pitch      = localOut.pitch;
            pOut->height     = localOut.height;
            pOut->baseAlign  = localOut.baseAlign;
            pOut->numSlices  = localOut.numSlices;
            pOut->fmaskBytes = static_cast<UINT_32>(localOut.surfSize);
            pOut->sliceSize  = static_cast<UINT_32>(localOut.sliceSize);
            pOut->bpp        = localIn.bpp;
            pOut->numSamples = 1;
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeFmaskAddrFromCoord
*
*   @brief
*       Interface function stub of ComputeFmaskAddrFromCoord.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeFmaskAddrFromCoord(
    const ADDR2_COMPUTE_FMASK_ADDRFROMCOORD_INPUT*   pIn,    ///< [in] input structure
    ADDR2_COMPUTE_FMASK_ADDRFROMCOORD_OUTPUT*        pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_NOTIMPLEMENTED;

    ADDR_NOT_IMPLEMENTED();

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeFmaskCoordFromAddr
*
*   @brief
*       Interface function stub of ComputeFmaskAddrFromCoord.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeFmaskCoordFromAddr(
    const ADDR2_COMPUTE_FMASK_COORDFROMADDR_INPUT*  pIn,     ///< [in] input structure
    ADDR2_COMPUTE_FMASK_COORDFROMADDR_OUTPUT*       pOut     ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_NOTIMPLEMENTED;

    ADDR_NOT_IMPLEMENTED();

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeDccInfo
*
*   @brief
*       Interface function to compute DCC key info
*
*   @return
*       return code of HwlComputeDccInfo
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeDccInfo(
    const ADDR2_COMPUTE_DCCINFO_INPUT*    pIn,    ///< [in] input structure
    ADDR2_COMPUTE_DCCINFO_OUTPUT*         pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_DCCINFO_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_DCCINFO_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeDccInfo(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeDccAddrFromCoord
*
*   @brief
*       Interface function stub of ComputeDccAddrFromCoord
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeDccAddrFromCoord(
    const ADDR2_COMPUTE_DCC_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
    ADDR2_COMPUTE_DCC_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_DCC_ADDRFROMCOORD_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_DCC_ADDRFROMCOORD_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeDccAddrFromCoord(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputePipeBankXor
*
*   @brief
*       Interface function stub of Addr2ComputePipeBankXor.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputePipeBankXor(
    const ADDR2_COMPUTE_PIPEBANKXOR_INPUT* pIn,
    ADDR2_COMPUTE_PIPEBANKXOR_OUTPUT*      pOut)
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_PIPEBANKXOR_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_PIPEBANKXOR_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else if (IsXor(pIn->swizzleMode) == FALSE)
    {
        returnCode = ADDR_NOTSUPPORTED;
    }
    else
    {
        returnCode = HwlComputePipeBankXor(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSlicePipeBankXor
*
*   @brief
*       Interface function stub of Addr2ComputeSlicePipeBankXor.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSlicePipeBankXor(
    const ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT* pIn,
    ADDR2_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT*      pOut)
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_SLICE_PIPEBANKXOR_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_SLICE_PIPEBANKXOR_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else if ((IsThin(pIn->resourceType, pIn->swizzleMode) == FALSE) ||
             (IsNonPrtXor(pIn->swizzleMode) == FALSE) ||
             (pIn->numSamples > 1))
    {
        returnCode = ADDR_NOTSUPPORTED;
    }
    else
    {
        returnCode = HwlComputeSlicePipeBankXor(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSubResourceOffsetForSwizzlePattern
*
*   @brief
*       Interface function stub of Addr2ComputeSubResourceOffsetForSwizzlePattern.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSubResourceOffsetForSwizzlePattern(
    const ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_INPUT* pIn,
    ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_OUTPUT*      pOut)
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_INPUT)) ||
         (pOut->size != sizeof(ADDR2_COMPUTE_SUBRESOURCE_OFFSET_FORSWIZZLEPATTERN_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeSubResourceOffsetForSwizzlePattern(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ExtractPipeBankXor
*
*   @brief
*       Internal function to extract bank and pipe xor bits from combined xor bits.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ExtractPipeBankXor(
    UINT_32  pipeBankXor,
    UINT_32  bankBits,
    UINT_32  pipeBits,
    UINT_32* pBankX,
    UINT_32* pPipeX)
{
    ADDR_E_RETURNCODE returnCode;

    if (pipeBankXor < (1u << (pipeBits + bankBits)))
    {
        *pPipeX = pipeBankXor % (1 << pipeBits);
        *pBankX = pipeBankXor >> pipeBits;
        returnCode = ADDR_OK;
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        returnCode = ADDR_INVALIDPARAMS;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfoSanityCheck
*
*   @brief
*       Internal function to do basic sanity check before compute surface info
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceInfoSanityCheck(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT*  pIn   ///< [in] input structure
    ) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        (pIn->size != sizeof(ADDR2_COMPUTE_SURFACE_INFO_INPUT)))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlComputeSurfaceInfoSanityCheck(pIn);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ApplyCustomizedPitchHeight
*
*   @brief
*       Helper function to override hw required row pitch/slice pitch by customrized one
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ApplyCustomizedPitchHeight(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
    UINT_32  elementBytes,                          ///< [in] element bytes per element
    UINT_32  pitchAlignInElement,                   ///< [in] pitch alignment in element
    UINT_32* pPitch,                                ///< [in/out] pitch
    UINT_32* pHeight                                ///< [in/out] height
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    if (pIn->numMipLevels <= 1)
    {
        if (pIn->pitchInElement > 0)
        {
            if ((pIn->pitchInElement % pitchAlignInElement) != 0)
            {
                returnCode = ADDR_INVALIDPARAMS;
            }
            else if (pIn->pitchInElement < (*pPitch))
            {
                returnCode = ADDR_INVALIDPARAMS;
            }
            else
            {
                *pPitch = pIn->pitchInElement;
            }
        }

        if (returnCode == ADDR_OK)
        {
            if (pIn->sliceAlign > 0)
            {
                UINT_32 customizedHeight = pIn->sliceAlign / elementBytes / (*pPitch);

                if (customizedHeight * elementBytes * (*pPitch) != pIn->sliceAlign)
                {
                    returnCode = ADDR_INVALIDPARAMS;
                }
                else if ((pIn->numSlices > 1) && ((*pHeight) != customizedHeight))
                {
                    returnCode = ADDR_INVALIDPARAMS;
                }
                else
                {
                    *pHeight = customizedHeight;
                }
            }
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfoLinear
*
*   @brief
*       Internal function to calculate alignment for linear swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceInfoLinear(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    UINT_32 pitch = 0;
    UINT_32 actualHeight = 0;
    UINT_32 elementBytes = pIn->bpp >> 3;
    const UINT_32 alignment = pIn->flags.prt ? PrtAlignment : 256;

    if (IsTex1d(pIn->resourceType))
    {
        if (pIn->height > 1)
        {
            returnCode = ADDR_INVALIDPARAMS;
        }
        else
        {
            const UINT_32 pitchAlignInElement = alignment / elementBytes;
            pitch = PowTwoAlign(pIn->width, pitchAlignInElement);
            actualHeight = pIn->numMipLevels;

            if (pIn->flags.prt == FALSE)
            {
                returnCode = ApplyCustomizedPitchHeight(pIn, elementBytes, pitchAlignInElement,
                                                        &pitch, &actualHeight);
            }

            if (returnCode == ADDR_OK)
            {
                if (pOut->pMipInfo != NULL)
                {
                    for (UINT_32 i = 0; i < pIn->numMipLevels; i++)
                    {
                        pOut->pMipInfo[i].offset = pitch * elementBytes * i;
                        pOut->pMipInfo[i].pitch = pitch;
                        pOut->pMipInfo[i].height = 1;
                        pOut->pMipInfo[i].depth = 1;
                    }
                }
            }
        }
    }
    else
    {
        returnCode = ComputeSurfaceLinearPadding(pIn, &pitch, &actualHeight, pOut->pMipInfo);
    }

    if ((pitch == 0) || (actualHeight == 0))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }

    if (returnCode == ADDR_OK)
    {
        pOut->pitch = pitch;
        pOut->height = pIn->height;
        pOut->numSlices = pIn->numSlices;
        pOut->mipChainPitch = pitch;
        pOut->mipChainHeight = actualHeight;
        pOut->mipChainSlice = pOut->numSlices;
        pOut->epitchIsHeight = (pIn->numMipLevels > 1) ? TRUE : FALSE;
        pOut->sliceSize = static_cast<UINT_64>(pOut->pitch) * actualHeight * elementBytes;
        pOut->surfSize = pOut->sliceSize * pOut->numSlices;
        pOut->baseAlign = (pIn->swizzleMode == ADDR_SW_LINEAR_GENERAL) ? (pIn->bpp / 8) : alignment;
        pOut->blockWidth = (pIn->swizzleMode == ADDR_SW_LINEAR_GENERAL) ? 1 : (256 * 8 / pIn->bpp);
        pOut->blockHeight = 1;
        pOut->blockSlices = 1;
    }

    // Post calculation validate
    ADDR_ASSERT(pOut->sliceSize > 0);

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfoTiled
*
*   @brief
*       Internal function to calculate alignment for tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceInfoTiled(
     const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_INFO_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    return HwlComputeSurfaceInfoTiled(pIn, pOut);
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceAddrFromCoordLinear
*
*   @brief
*       Internal function to calculate address from coord for linear swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceAddrFromCoordLinear(
     const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;
    BOOL_32 valid = (pIn->numSamples <= 1) && (pIn->numFrags <= 1) && (pIn->pipeBankXor == 0);

    if (valid)
    {
        if (IsTex1d(pIn->resourceType))
        {
            valid = (pIn->y == 0);
        }
    }

    if (valid)
    {
        ADDR2_COMPUTE_SURFACE_INFO_INPUT  localIn  = {0};
        ADDR2_COMPUTE_SURFACE_INFO_OUTPUT localOut = {0};
        localIn.bpp          = pIn->bpp;
        localIn.flags        = pIn->flags;
        localIn.width        = Max(pIn->unalignedWidth, 1u);
        localIn.height       = Max(pIn->unalignedHeight, 1u);
        localIn.numSlices    = Max(pIn->numSlices, 1u);
        localIn.numMipLevels = Max(pIn->numMipLevels, 1u);
        localIn.resourceType = pIn->resourceType;
        if (localIn.numMipLevels <= 1)
        {
            localIn.pitchInElement = pIn->pitchInElement;
        }
        returnCode = ComputeSurfaceInfoLinear(&localIn, &localOut);

        if (returnCode == ADDR_OK)
        {
            UINT_32 elementBytes = pIn->bpp >> 3;
            UINT_64 sliceOffsetInSurf = localOut.sliceSize * pIn->slice;
            UINT_64 mipOffsetInSlice = 0;
            UINT_64 offsetInMip = 0;

            if (IsTex1d(pIn->resourceType))
            {
                offsetInMip = static_cast<UINT_64>(pIn->x) * elementBytes;
                mipOffsetInSlice = static_cast<UINT_64>(pIn->mipId) * localOut.pitch * elementBytes;
            }
            else
            {
                UINT_64 mipStartHeight = SumGeo(localIn.height, pIn->mipId);
                mipOffsetInSlice = static_cast<UINT_64>(mipStartHeight) * localOut.pitch * elementBytes;
                offsetInMip = (pIn->y * localOut.pitch + pIn->x) * elementBytes;
            }

            pOut->addr = sliceOffsetInSurf + mipOffsetInSlice + offsetInMip;
            pOut->bitPosition = 0;
        }
        else
        {
            valid = FALSE;
        }
    }

    if (valid == FALSE)
    {
        returnCode = ADDR_INVALIDPARAMS;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceAddrFromCoordTiled
*
*   @brief
*       Internal function to calculate address from coord for tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceAddrFromCoordTiled(
     const ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    return HwlComputeSurfaceAddrFromCoordTiled(pIn, pOut);
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceCoordFromAddrLinear
*
*   @brief
*       Internal function to calculate coord from address for linear swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceCoordFromAddrLinear(
     const ADDR2_COMPUTE_SURFACE_COORDFROMADDR_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    BOOL_32 valid = (pIn->numSamples <= 1) && (pIn->numFrags <= 1);

    if (valid)
    {
        if (IsTex1d(pIn->resourceType))
        {
            valid = (pIn->unalignedHeight == 1);
        }
    }

    if (valid)
    {
        ADDR2_COMPUTE_SURFACE_INFO_INPUT  localIn  = {0};
        ADDR2_COMPUTE_SURFACE_INFO_OUTPUT localOut = {0};
        localIn.bpp          = pIn->bpp;
        localIn.flags        = pIn->flags;
        localIn.width        = Max(pIn->unalignedWidth, 1u);
        localIn.height       = Max(pIn->unalignedHeight, 1u);
        localIn.numSlices    = Max(pIn->numSlices, 1u);
        localIn.numMipLevels = Max(pIn->numMipLevels, 1u);
        localIn.resourceType = pIn->resourceType;
        if (localIn.numMipLevels <= 1)
        {
            localIn.pitchInElement = pIn->pitchInElement;
        }
        returnCode = ComputeSurfaceInfoLinear(&localIn, &localOut);

        if (returnCode == ADDR_OK)
        {
            pOut->slice = static_cast<UINT_32>(pIn->addr / localOut.sliceSize);
            pOut->sample = 0;

            UINT_32 offsetInSlice = static_cast<UINT_32>(pIn->addr % localOut.sliceSize);
            UINT_32 elementBytes = pIn->bpp >> 3;
            UINT_32 mipOffsetInSlice = 0;
            UINT_32 mipSize = 0;
            UINT_32 mipId = 0;
            for (; mipId < pIn->numMipLevels ; mipId++)
            {
                if (IsTex1d(pIn->resourceType))
                {
                    mipSize = localOut.pitch * elementBytes;
                }
                else
                {
                    UINT_32 currentMipHeight = (PowTwoAlign(localIn.height, (1 << mipId))) >> mipId;
                    mipSize = currentMipHeight * localOut.pitch * elementBytes;
                }

                if (mipSize == 0)
                {
                    valid = FALSE;
                    break;
                }
                else if ((mipSize + mipOffsetInSlice) > offsetInSlice)
                {
                    break;
                }
                else
                {
                    mipOffsetInSlice += mipSize;
                    if ((mipId == (pIn->numMipLevels - 1)) ||
                        (mipOffsetInSlice >= localOut.sliceSize))
                    {
                        valid = FALSE;
                    }
                }
            }

            if (valid)
            {
                pOut->mipId = mipId;

                UINT_32 elemOffsetInMip = (offsetInSlice - mipOffsetInSlice) / elementBytes;
                if (IsTex1d(pIn->resourceType))
                {
                    if (elemOffsetInMip < localOut.pitch)
                    {
                        pOut->x = elemOffsetInMip;
                        pOut->y = 0;
                    }
                    else
                    {
                        valid = FALSE;
                    }
                }
                else
                {
                    pOut->y = elemOffsetInMip / localOut.pitch;
                    pOut->x = elemOffsetInMip % localOut.pitch;
                }

                if ((pOut->slice >= pIn->numSlices)    ||
                    (pOut->mipId >= pIn->numMipLevels) ||
                    (pOut->x >= Max((pIn->unalignedWidth >> pOut->mipId), 1u))  ||
                    (pOut->y >= Max((pIn->unalignedHeight >> pOut->mipId), 1u)) ||
                    (IsTex3d(pIn->resourceType) &&
                     (FALSE == Valid3DMipSliceIdConstraint(pIn->numSlices,
                                                           pOut->mipId,
                                                           pOut->slice))))
                {
                    valid = FALSE;
                }
            }
        }
        else
        {
            valid = FALSE;
        }
    }

    if (valid == FALSE)
    {
        returnCode = ADDR_INVALIDPARAMS;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceCoordFromAddrTiled
*
*   @brief
*       Internal function to calculate coord from address for tiled swizzle surface
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceCoordFromAddrTiled(
     const ADDR2_COMPUTE_SURFACE_COORDFROMADDR_INPUT* pIn,    ///< [in] input structure
     ADDR2_COMPUTE_SURFACE_COORDFROMADDR_OUTPUT*      pOut    ///< [out] output structure
     ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_NOTIMPLEMENTED;

    ADDR_NOT_IMPLEMENTED();

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurfaceInfoLinear
*
*   @brief
*       Internal function to calculate padding for linear swizzle 2D/3D surface
*
*   @return
*       N/A
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeSurfaceLinearPadding(
    const ADDR2_COMPUTE_SURFACE_INFO_INPUT* pIn,    ///< [in] input srtucture
    UINT_32* pMipmap0PaddedWidth,                   ///< [out] padded width in element
    UINT_32* pSlice0PaddedHeight,                   ///< [out] padded height for HW
    ADDR2_MIP_INFO* pMipInfo                        ///< [out] per mip information
    ) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    UINT_32 elementBytes = pIn->bpp >> 3;
    UINT_32 pitchAlignInElement = 0;

    if (pIn->swizzleMode == ADDR_SW_LINEAR_GENERAL)
    {
        ADDR_ASSERT(pIn->numMipLevels <= 1);
        ADDR_ASSERT(pIn->numSlices <= 1);
        pitchAlignInElement = 1;
    }
    else
    {
        pitchAlignInElement = (256 / elementBytes);
    }

    UINT_32 mipChainWidth = PowTwoAlign(pIn->width, pitchAlignInElement);
    UINT_32 slice0PaddedHeight = pIn->height;

    returnCode = ApplyCustomizedPitchHeight(pIn, elementBytes, pitchAlignInElement,
                                            &mipChainWidth, &slice0PaddedHeight);

    if (returnCode == ADDR_OK)
    {
        UINT_32 mipChainHeight = 0;
        UINT_32 mipHeight = pIn->height;

        for (UINT_32 i = 0; i < pIn->numMipLevels; i++)
        {
            if (pMipInfo != NULL)
            {
                pMipInfo[i].offset = mipChainWidth * mipChainHeight * elementBytes;
                pMipInfo[i].pitch = mipChainWidth;
                pMipInfo[i].height = mipHeight;
                pMipInfo[i].depth = 1;
            }

            mipChainHeight += mipHeight;
            mipHeight = RoundHalf(mipHeight);
            mipHeight = Max(mipHeight, 1u);
        }

        *pMipmap0PaddedWidth = mipChainWidth;
        *pSlice0PaddedHeight = (pIn->numMipLevels > 1) ? mipChainHeight : slice0PaddedHeight;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeBlockDimensionForSurf
*
*   @brief
*       Internal function to get block width/height/depth in element from surface input params.
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeBlockDimensionForSurf(
    UINT_32*         pWidth,
    UINT_32*         pHeight,
    UINT_32*         pDepth,
    UINT_32          bpp,
    UINT_32          numSamples,
    AddrResourceType resourceType,
    AddrSwizzleMode  swizzleMode) const
{
    ADDR_E_RETURNCODE returnCode = ComputeBlockDimension(pWidth,
                                                         pHeight,
                                                         pDepth,
                                                         bpp,
                                                         resourceType,
                                                         swizzleMode);

    if ((returnCode == ADDR_OK) && (numSamples > 1) && IsThin(resourceType, swizzleMode))
    {
        const UINT_32 log2blkSize = GetBlockSizeLog2(swizzleMode);
        const UINT_32 log2sample  = Log2(numSamples);
        const UINT_32 q           = log2sample >> 1;
        const UINT_32 r           = log2sample & 1;

        if (log2blkSize & 1)
        {
            *pWidth  >>= q;
            *pHeight >>= (q + r);
        }
        else
        {
            *pWidth  >>= (q + r);
            *pHeight >>= q;
        }
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeBlockDimension
*
*   @brief
*       Internal function to get block width/height/depth in element without considering MSAA case
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeBlockDimension(
    UINT_32*          pWidth,
    UINT_32*          pHeight,
    UINT_32*          pDepth,
    UINT_32           bpp,
    AddrResourceType  resourceType,
    AddrSwizzleMode   swizzleMode) const
{
    ADDR_E_RETURNCODE returnCode = ADDR_OK;

    UINT_32 eleBytes                 = bpp >> 3;
    UINT_32 microBlockSizeTableIndex = Log2(eleBytes);
    UINT_32 log2blkSize              = GetBlockSizeLog2(swizzleMode);

    if (IsThin(resourceType, swizzleMode))
    {
        UINT_32 log2blkSizeIn256B = log2blkSize - 8;
        UINT_32 widthAmp          = log2blkSizeIn256B / 2;
        UINT_32 heightAmp         = log2blkSizeIn256B - widthAmp;

        ADDR_ASSERT(microBlockSizeTableIndex < sizeof(Block256_2d) / sizeof(Block256_2d[0]));

        *pWidth  = (Block256_2d[microBlockSizeTableIndex].w << widthAmp);
        *pHeight = (Block256_2d[microBlockSizeTableIndex].h << heightAmp);
        *pDepth  = 1;
    }
    else if (IsThick(resourceType, swizzleMode))
    {
        UINT_32 log2blkSizeIn1KB = log2blkSize - 10;
        UINT_32 averageAmp       = log2blkSizeIn1KB / 3;
        UINT_32 restAmp          = log2blkSizeIn1KB % 3;

        ADDR_ASSERT(microBlockSizeTableIndex < sizeof(Block1K_3d) / sizeof(Block1K_3d[0]));

        *pWidth  = Block1K_3d[microBlockSizeTableIndex].w << averageAmp;
        *pHeight = Block1K_3d[microBlockSizeTableIndex].h << (averageAmp + (restAmp / 2));
        *pDepth  = Block1K_3d[microBlockSizeTableIndex].d << (averageAmp + ((restAmp != 0) ? 1 : 0));
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        returnCode = ADDR_INVALIDPARAMS;
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::GetMipTailDim
*
*   @brief
*       Internal function to get out max dimension of first level in mip tail
*
*   @return
*       Max Width/Height/Depth value of the first mip fitted in mip tail
************************************************************************************************************************
*/
Dim3d Lib::GetMipTailDim(
    AddrResourceType  resourceType,
    AddrSwizzleMode   swizzleMode,
    UINT_32           blockWidth,
    UINT_32           blockHeight,
    UINT_32           blockDepth) const
{
    Dim3d   out         = {blockWidth, blockHeight, blockDepth};
    UINT_32 log2blkSize = GetBlockSizeLog2(swizzleMode);

    if (IsThick(resourceType, swizzleMode))
    {
        UINT_32 dim = log2blkSize % 3;

        if (dim == 0)
        {
            out.h >>= 1;
        }
        else if (dim == 1)
        {
            out.w >>= 1;
        }
        else
        {
            out.d >>= 1;
        }
    }
    else
    {
        if (log2blkSize & 1)
        {
            out.h >>= 1;
        }
        else
        {
            out.w >>= 1;
        }
    }

    return out;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurface2DMicroBlockOffset
*
*   @brief
*       Internal function to calculate micro block (256B) offset from coord for 2D resource
*
*   @return
*       micro block (256B) offset for 2D resource
************************************************************************************************************************
*/
UINT_32 Lib::ComputeSurface2DMicroBlockOffset(
    const _ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn) const
{
    ADDR_ASSERT(IsThin(pIn->resourceType, pIn->swizzleMode));

    UINT_32 log2ElementBytes = Log2(pIn->bpp >> 3);
    UINT_32 microBlockOffset = 0;
    if (IsStandardSwizzle(pIn->resourceType, pIn->swizzleMode))
    {
        UINT_32 xBits = pIn->x << log2ElementBytes;
        microBlockOffset = (xBits & 0xf) | ((pIn->y & 0x3) << 4);
        if (log2ElementBytes < 3)
        {
            microBlockOffset |= (pIn->y & 0x4) << 4;
            if (log2ElementBytes == 0)
            {
                microBlockOffset |= (pIn->y & 0x8) << 4;
            }
            else
            {
                microBlockOffset |= (xBits & 0x10) << 3;
            }
        }
        else
        {
            microBlockOffset |= (xBits & 0x30) << 2;
        }
    }
    else if (IsDisplaySwizzle(pIn->resourceType, pIn->swizzleMode))
    {
        if (log2ElementBytes == 4)
        {
            microBlockOffset = (GetBit(pIn->x, 0) << 4) |
                               (GetBit(pIn->y, 0) << 5) |
                               (GetBit(pIn->x, 1) << 6) |
                               (GetBit(pIn->y, 1) << 7);
        }
        else
        {
            microBlockOffset = GetBits(pIn->x, 0, 3, log2ElementBytes)     |
                               GetBits(pIn->y, 1, 2, 3 + log2ElementBytes) |
                               GetBits(pIn->x, 3, 1, 5 + log2ElementBytes) |
                               GetBits(pIn->y, 3, 1, 6 + log2ElementBytes);
            microBlockOffset = GetBits(microBlockOffset, 0, 4, 0) |
                               (GetBit(pIn->y, 0) << 4) |
                               GetBits(microBlockOffset, 4, 3, 5);
        }
    }
    else if (IsRotateSwizzle(pIn->swizzleMode))
    {
        microBlockOffset = GetBits(pIn->y, 0, 3, log2ElementBytes) |
                           GetBits(pIn->x, 1, 2, 3 + log2ElementBytes) |
                           GetBits(pIn->x, 3, 1, 5 + log2ElementBytes) |
                           GetBits(pIn->y, 3, 1, 6 + log2ElementBytes);
        microBlockOffset = GetBits(microBlockOffset, 0, 4, 0) |
                           (GetBit(pIn->x, 0) << 4) |
                           GetBits(microBlockOffset, 4, 3, 5);
        if (log2ElementBytes == 3)
        {
           microBlockOffset = GetBits(microBlockOffset, 0, 6, 0) |
                              GetBits(pIn->x, 1, 2, 6);
        }
    }

    return microBlockOffset;
}

/**
************************************************************************************************************************
*   Lib::ComputeSurface3DMicroBlockOffset
*
*   @brief
*       Internal function to calculate micro block (1KB) offset from coord for 3D resource
*
*   @return
*       micro block (1KB) offset for 3D resource
************************************************************************************************************************
*/
UINT_32 Lib::ComputeSurface3DMicroBlockOffset(
    const _ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pIn) const
{
    ADDR_ASSERT(IsThick(pIn->resourceType, pIn->swizzleMode));

    UINT_32 log2ElementBytes = Log2(pIn->bpp >> 3);
    UINT_32 microBlockOffset = 0;
    if (IsStandardSwizzle(pIn->resourceType, pIn->swizzleMode))
    {
        if (log2ElementBytes == 0)
        {
            microBlockOffset = ((pIn->slice & 4) >> 2) | ((pIn->y & 4) >> 1);
        }
        else if (log2ElementBytes == 1)
        {
            microBlockOffset = ((pIn->slice & 4) >> 2) | ((pIn->y & 4) >> 1);
        }
        else if (log2ElementBytes == 2)
        {
            microBlockOffset = ((pIn->y & 4) >> 2) | ((pIn->x & 4) >> 1);
        }
        else if (log2ElementBytes == 3)
        {
            microBlockOffset = (pIn->x & 6) >> 1;
        }
        else
        {
            microBlockOffset = pIn->x & 3;
        }

        microBlockOffset <<= 8;

        UINT_32 xBits = pIn->x << log2ElementBytes;
        microBlockOffset |= (xBits & 0xf) | ((pIn->y & 0x3) << 4) | ((pIn->slice & 0x3) << 6);
    }
    else if (IsZOrderSwizzle(pIn->swizzleMode))
    {
        UINT_32 xh, yh, zh;

        if (log2ElementBytes == 0)
        {
            microBlockOffset =
                (pIn->x & 1) | ((pIn->y & 1) << 1) | ((pIn->x & 2) << 1) | ((pIn->y & 2) << 2);
            microBlockOffset = microBlockOffset | ((pIn->slice & 3) << 4) | ((pIn->x & 4) << 4);

            xh = pIn->x >> 3;
            yh = pIn->y >> 2;
            zh = pIn->slice >> 2;
        }
        else if (log2ElementBytes == 1)
        {
            microBlockOffset =
                (pIn->x & 1) | ((pIn->y & 1) << 1) | ((pIn->x & 2) << 1) | ((pIn->y & 2) << 2);
            microBlockOffset = (microBlockOffset << 1) | ((pIn->slice & 3) << 5);

            xh = pIn->x >> 2;
            yh = pIn->y >> 2;
            zh = pIn->slice >> 2;
        }
        else if (log2ElementBytes == 2)
        {
            microBlockOffset =
                (pIn->x & 1) | ((pIn->y & 1) << 1) | ((pIn->x & 2) << 1) | ((pIn->slice & 1) << 3);
            microBlockOffset = (microBlockOffset << 2) | ((pIn->y & 2) << 5);

            xh = pIn->x >> 2;
            yh = pIn->y >> 2;
            zh = pIn->slice >> 1;
        }
        else if (log2ElementBytes == 3)
        {
            microBlockOffset =
                (pIn->x & 1) | ((pIn->y & 1) << 1) | ((pIn->slice & 1) << 2) | ((pIn->x & 2) << 2);
            microBlockOffset <<= 3;

            xh = pIn->x >> 2;
            yh = pIn->y >> 1;
            zh = pIn->slice >> 1;
        }
        else
        {
            microBlockOffset =
                (((pIn->x & 1) | ((pIn->y & 1) << 1) | ((pIn->slice & 1) << 2)) << 4);

            xh = pIn->x >> 1;
            yh = pIn->y >> 1;
            zh = pIn->slice >> 1;
        }

        microBlockOffset |= ((MortonGen3d(xh, yh, zh, 1) << 7) & 0x380);
    }

    return microBlockOffset;
}

/**
************************************************************************************************************************
*   Lib::GetPipeXorBits
*
*   @brief
*       Internal function to get bits number for pipe/se xor operation
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
UINT_32 Lib::GetPipeXorBits(
    UINT_32 macroBlockBits) const
{
    ADDR_ASSERT(macroBlockBits >= m_pipeInterleaveLog2);

    // Total available xor bits
    UINT_32 xorBits = macroBlockBits - m_pipeInterleaveLog2;

    // Pipe/Se xor bits
    UINT_32 pipeBits = Min(xorBits, m_pipesLog2 + m_seLog2);

    return pipeBits;
}

/**
************************************************************************************************************************
*   Lib::GetBankXorBits
*
*   @brief
*       Internal function to get bits number for pipe/se xor operation
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
UINT_32 Lib::GetBankXorBits(
    UINT_32 macroBlockBits) const
{
    UINT_32 pipeBits = GetPipeXorBits(macroBlockBits);

    // Bank xor bits
    UINT_32 bankBits = Min(macroBlockBits - pipeBits - m_pipeInterleaveLog2, m_banksLog2);

    return bankBits;
}

/**
************************************************************************************************************************
*   Lib::Addr2GetPreferredSurfaceSetting
*
*   @brief
*       Internal function to get suggested surface information for cliet to use
*
*   @return
*       ADDR_E_RETURNCODE
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::Addr2GetPreferredSurfaceSetting(
    const ADDR2_GET_PREFERRED_SURF_SETTING_INPUT* pIn,
    ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT*      pOut) const
{
    ADDR_E_RETURNCODE returnCode;

    if ((GetFillSizeFieldsFlags() == TRUE) &&
        ((pIn->size != sizeof(ADDR2_GET_PREFERRED_SURF_SETTING_INPUT)) ||
         (pOut->size != sizeof(ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT))))
    {
        returnCode = ADDR_INVALIDPARAMS;
    }
    else
    {
        returnCode = HwlGetPreferredSurfaceSetting(pIn, pOut);
    }

    return returnCode;
}

/**
************************************************************************************************************************
*   Lib::ComputeBlock256Equation
*
*   @brief
*       Compute equation for block 256B
*
*   @return
*       If equation computed successfully
*
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeBlock256Equation(
    AddrResourceType rsrcType,
    AddrSwizzleMode swMode,
    UINT_32 elementBytesLog2,
    ADDR_EQUATION* pEquation) const
{
    ADDR_E_RETURNCODE ret;

    if (IsBlock256b(swMode))
    {
        ret = HwlComputeBlock256Equation(rsrcType, swMode, elementBytesLog2, pEquation);
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        ret = ADDR_INVALIDPARAMS;
    }

    return ret;
}

/**
************************************************************************************************************************
*   Lib::ComputeThinEquation
*
*   @brief
*       Compute equation for 2D/3D resource which use THIN mode
*
*   @return
*       If equation computed successfully
*
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeThinEquation(
    AddrResourceType rsrcType,
    AddrSwizzleMode swMode,
    UINT_32 elementBytesLog2,
    ADDR_EQUATION* pEquation) const
{
    ADDR_E_RETURNCODE ret;

    if (IsThin(rsrcType, swMode))
    {
        ret = HwlComputeThinEquation(rsrcType, swMode, elementBytesLog2, pEquation);
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        ret = ADDR_INVALIDPARAMS;
    }

    return ret;
}

/**
************************************************************************************************************************
*   Lib::ComputeThickEquation
*
*   @brief
*       Compute equation for 3D resource which use THICK mode
*
*   @return
*       If equation computed successfully
*
************************************************************************************************************************
*/
ADDR_E_RETURNCODE Lib::ComputeThickEquation(
    AddrResourceType rsrcType,
    AddrSwizzleMode swMode,
    UINT_32 elementBytesLog2,
    ADDR_EQUATION* pEquation) const
{
    ADDR_E_RETURNCODE ret;

    if (IsThick(rsrcType, swMode))
    {
        ret = HwlComputeThickEquation(rsrcType, swMode, elementBytesLog2, pEquation);
    }
    else
    {
        ADDR_ASSERT_ALWAYS();
        ret = ADDR_INVALIDPARAMS;
    }

    return ret;
}

/**
************************************************************************************************************************
*   Lib::ComputeQbStereoInfo
*
*   @brief
*       Get quad buffer stereo information
*   @return
*       N/A
************************************************************************************************************************
*/
VOID Lib::ComputeQbStereoInfo(
    ADDR2_COMPUTE_SURFACE_INFO_OUTPUT* pOut    ///< [in,out] updated pOut+pStereoInfo
    ) const
{
    ADDR_ASSERT(pOut->bpp >= 8);
    ADDR_ASSERT((pOut->surfSize % pOut->baseAlign) == 0);

    // Save original height
    pOut->pStereoInfo->eyeHeight = pOut->height;

    // Right offset
    pOut->pStereoInfo->rightOffset = static_cast<UINT_32>(pOut->surfSize);

    // Double height
    pOut->height <<= 1;

    ADDR_ASSERT(pOut->height <= MaxSurfaceHeight);

    pOut->pixelHeight <<= 1;

    // Double size
    pOut->surfSize <<= 1;
}


} // V2
} // Addr

