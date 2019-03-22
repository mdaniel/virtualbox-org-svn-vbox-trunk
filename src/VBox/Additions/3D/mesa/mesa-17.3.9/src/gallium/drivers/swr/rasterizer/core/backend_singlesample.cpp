/****************************************************************************
* Copyright (C) 2014-2015 Intel Corporation.   All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* @file backend.cpp
*
* @brief Backend handles rasterization, pixel shading and output merger
*        operations.
*
******************************************************************************/

#include <smmintrin.h>

#include "backend.h"
#include "backend_impl.h"
#include "tilemgr.h"
#include "memory/tilingtraits.h"
#include "core/multisample.h"

#include <algorithm>

template<typename T>
void BackendSingleSample(DRAW_CONTEXT *pDC, uint32_t workerId, uint32_t x, uint32_t y, SWR_TRIANGLE_DESC &work, RenderOutputBuffers &renderBuffers)
{
    SWR_CONTEXT *pContext = pDC->pContext;

    AR_BEGIN(BESingleSampleBackend, pDC->drawId);
    AR_BEGIN(BESetup, pDC->drawId);

    const API_STATE &state = GetApiState(pDC);

    BarycentricCoeffs coeffs;
    SetupBarycentricCoeffs(&coeffs, work);

    SWR_PS_CONTEXT psContext;
    const SWR_MULTISAMPLE_POS& samplePos = state.rastState.samplePositions;
    SetupPixelShaderContext<T>(&psContext, samplePos, work);

    uint8_t *pDepthBuffer, *pStencilBuffer;
    SetupRenderBuffers(psContext.pColorBuffer, &pDepthBuffer, &pStencilBuffer, state.colorHottileEnable, renderBuffers);

    AR_END(BESetup, 1);

    psContext.vY.UL     = _simd_add_ps(vULOffsetsY,     _simd_set1_ps(static_cast<float>(y)));
    psContext.vY.center = _simd_add_ps(vCenterOffsetsY, _simd_set1_ps(static_cast<float>(y)));

    const simdscalar dy = _simd_set1_ps(static_cast<float>(SIMD_TILE_Y_DIM));

    for (uint32_t yy = y; yy < y + KNOB_TILE_Y_DIM; yy += SIMD_TILE_Y_DIM)
    {
        psContext.vX.UL     = _simd_add_ps(vULOffsetsX,     _simd_set1_ps(static_cast<float>(x)));
        psContext.vX.center = _simd_add_ps(vCenterOffsetsX, _simd_set1_ps(static_cast<float>(x)));

        const simdscalar dx = _simd_set1_ps(static_cast<float>(SIMD_TILE_X_DIM));

        for (uint32_t xx = x; xx < x + KNOB_TILE_X_DIM; xx += SIMD_TILE_X_DIM)
        {
#if USE_8x2_TILE_BACKEND
            const bool useAlternateOffset = ((xx & SIMD_TILE_X_DIM) != 0);
#endif
            simdmask coverageMask = work.coverageMask[0] & MASK;

            if (coverageMask)
            {
                if (state.depthHottileEnable && state.depthBoundsState.depthBoundsTestEnable)
                {
                    static_assert(KNOB_DEPTH_HOT_TILE_FORMAT == R32_FLOAT, "Unsupported depth hot tile format");

                    const simdscalar z = _simd_load_ps(reinterpret_cast<const float *>(pDepthBuffer));

                    const float minz = state.depthBoundsState.depthBoundsTestMinValue;
                    const float maxz = state.depthBoundsState.depthBoundsTestMaxValue;

                    coverageMask &= CalcDepthBoundsAcceptMask(z, minz, maxz);
                }

                if (T::InputCoverage != SWR_INPUT_COVERAGE_NONE)
                {
                    const uint64_t* pCoverageMask = (T::InputCoverage == SWR_INPUT_COVERAGE_INNER_CONSERVATIVE) ? &work.innerCoverageMask : &work.coverageMask[0];

                    generateInputCoverage<T, T::InputCoverage>(pCoverageMask, psContext.inputMask, state.blendState.sampleMask);
                }

                AR_BEGIN(BEBarycentric, pDC->drawId);

                CalcPixelBarycentrics(coeffs, psContext);

                CalcCentroid<T, true>(&psContext, samplePos, coeffs, work.coverageMask, state.blendState.sampleMask);

                // interpolate and quantize z
                psContext.vZ = vplaneps(coeffs.vZa, coeffs.vZb, coeffs.vZc, psContext.vI.center, psContext.vJ.center);
                psContext.vZ = state.pfnQuantizeDepth(psContext.vZ);

                AR_END(BEBarycentric, 1);

                // interpolate user clip distance if available
                if (state.backendState.clipDistanceMask)
                {
                    coverageMask &= ~ComputeUserClipMask(state.backendState.clipDistanceMask, work.pUserClipBuffer, psContext.vI.center, psContext.vJ.center);
                }

                simdscalar vCoverageMask = _simd_vmask_ps(coverageMask);
                simdscalar depthPassMask = vCoverageMask;
                simdscalar stencilPassMask = vCoverageMask;

                // Early-Z?
                if (T::bCanEarlyZ)
                {
                    AR_BEGIN(BEEarlyDepthTest, pDC->drawId);
                    depthPassMask = DepthStencilTest(&state, work.triFlags.frontFacing, work.triFlags.viewportIndex,
                                                     psContext.vZ, pDepthBuffer, vCoverageMask, pStencilBuffer, &stencilPassMask);
                    AR_EVENT(EarlyDepthStencilInfoSingleSample(_simd_movemask_ps(depthPassMask), _simd_movemask_ps(stencilPassMask), _simd_movemask_ps(vCoverageMask)));
                    AR_END(BEEarlyDepthTest, 0);

                    // early-exit if no pixels passed depth or earlyZ is forced on
                    if (state.psState.forceEarlyZ || !_simd_movemask_ps(depthPassMask))
                    {
                        DepthStencilWrite(&state.vp[work.triFlags.viewportIndex], &state.depthStencilState, work.triFlags.frontFacing, psContext.vZ,
                            pDepthBuffer, depthPassMask, vCoverageMask, pStencilBuffer, stencilPassMask);

                        if (!_simd_movemask_ps(depthPassMask))
                        {
                            goto Endtile;
                        }
                    }
                }

                psContext.sampleIndex = 0;
                psContext.activeMask = _simd_castps_si(vCoverageMask);

                // execute pixel shader
                AR_BEGIN(BEPixelShader, pDC->drawId);
                UPDATE_STAT_BE(PsInvocations, _mm_popcnt_u32(_simd_movemask_ps(vCoverageMask)));
                state.psState.pfnPixelShader(GetPrivateState(pDC), &psContext);
                AR_END(BEPixelShader, 0);

                vCoverageMask = _simd_castsi_ps(psContext.activeMask);

                // late-Z
                if (!T::bCanEarlyZ)
                {
                    AR_BEGIN(BELateDepthTest, pDC->drawId);
                    depthPassMask = DepthStencilTest(&state, work.triFlags.frontFacing, work.triFlags.viewportIndex,
                                                        psContext.vZ, pDepthBuffer, vCoverageMask, pStencilBuffer, &stencilPassMask);
                    AR_EVENT(LateDepthStencilInfoSingleSample(_simd_movemask_ps(depthPassMask), _simd_movemask_ps(stencilPassMask), _simd_movemask_ps(vCoverageMask)));
                    AR_END(BELateDepthTest, 0);

                    if (!_simd_movemask_ps(depthPassMask))
                    {
                        // need to call depth/stencil write for stencil write
                        DepthStencilWrite(&state.vp[work.triFlags.viewportIndex], &state.depthStencilState, work.triFlags.frontFacing, psContext.vZ,
                            pDepthBuffer, depthPassMask, vCoverageMask, pStencilBuffer, stencilPassMask);
                        goto Endtile;
                    }
                } else {
                    // for early z, consolidate discards from shader
                    // into depthPassMask
                    depthPassMask = _simd_and_ps(depthPassMask, vCoverageMask);
                }

                uint32_t statMask = _simd_movemask_ps(depthPassMask);
                uint32_t statCount = _mm_popcnt_u32(statMask);
                UPDATE_STAT_BE(DepthPassCount, statCount);

                // output merger
                AR_BEGIN(BEOutputMerger, pDC->drawId);
#if USE_8x2_TILE_BACKEND
                OutputMerger8x2(psContext, psContext.pColorBuffer, 0, &state.blendState, state.pfnBlendFunc, vCoverageMask, depthPassMask, state.psState.renderTargetMask, useAlternateOffset);
#else
                OutputMerger4x2(psContext, psContext.pColorBuffer, 0, &state.blendState, state.pfnBlendFunc, vCoverageMask, depthPassMask, state.psState.renderTargetMask);
#endif

                // do final depth write after all pixel kills
                if (!state.psState.forceEarlyZ)
                {
                    DepthStencilWrite(&state.vp[work.triFlags.viewportIndex], &state.depthStencilState, work.triFlags.frontFacing, psContext.vZ,
                        pDepthBuffer, depthPassMask, vCoverageMask, pStencilBuffer, stencilPassMask);
                }
                AR_END(BEOutputMerger, 0);
            }

Endtile:
            AR_BEGIN(BEEndTile, pDC->drawId);

            work.coverageMask[0] >>= (SIMD_TILE_Y_DIM * SIMD_TILE_X_DIM);
            if(T::InputCoverage == SWR_INPUT_COVERAGE_INNER_CONSERVATIVE)
            {
                work.innerCoverageMask >>= (SIMD_TILE_Y_DIM * SIMD_TILE_X_DIM);
            }

#if USE_8x2_TILE_BACKEND
            if (useAlternateOffset)
            {
                DWORD rt;
                uint32_t rtMask = state.colorHottileEnable;
                while(_BitScanForward(&rt, rtMask))
                {
                    rtMask &= ~(1 << rt);
                    psContext.pColorBuffer[rt] += (2 * KNOB_SIMD_WIDTH * FormatTraits<KNOB_COLOR_HOT_TILE_FORMAT>::bpp) / 8;
                }
            }
#else
            DWORD rt;
            uint32_t rtMask = state.colorHottileEnable;
            while (_BitScanForward(&rt, rtMask))
            {
                rtMask &= ~(1 << rt);
                psContext.pColorBuffer[rt] += (KNOB_SIMD_WIDTH * FormatTraits<KNOB_COLOR_HOT_TILE_FORMAT>::bpp) / 8;
            }
#endif
            pDepthBuffer += (KNOB_SIMD_WIDTH * FormatTraits<KNOB_DEPTH_HOT_TILE_FORMAT>::bpp) / 8;
            pStencilBuffer += (KNOB_SIMD_WIDTH * FormatTraits<KNOB_STENCIL_HOT_TILE_FORMAT>::bpp) / 8;

            AR_END(BEEndTile, 0);

            psContext.vX.UL     = _simd_add_ps(psContext.vX.UL,     dx);
            psContext.vX.center = _simd_add_ps(psContext.vX.center, dx);
        }

        psContext.vY.UL     = _simd_add_ps(psContext.vY.UL,     dy);
        psContext.vY.center = _simd_add_ps(psContext.vY.center, dy);
    }

    AR_END(BESingleSampleBackend, 0);
}

// Recursive template used to auto-nest conditionals.  Converts dynamic enum function
// arguments to static template arguments.
template <uint32_t... ArgsT>
struct BEChooserSingleSample
{
    // Last Arg Terminator
    static PFN_BACKEND_FUNC GetFunc(SWR_BACKEND_FUNCS tArg)
    {
        switch(tArg)
        {
        case SWR_BACKEND_SINGLE_SAMPLE: return BackendSingleSample<SwrBackendTraits<ArgsT...>>; break;
        case SWR_BACKEND_MSAA_PIXEL_RATE:
        case SWR_BACKEND_MSAA_SAMPLE_RATE:
        default:
            SWR_ASSERT(0 && "Invalid backend func\n");
            return nullptr;
            break;
        }
    }

    // Recursively parse args
    template <typename... TArgsT>
    static PFN_BACKEND_FUNC GetFunc(SWR_INPUT_COVERAGE tArg, TArgsT... remainingArgs)
    {
        switch(tArg)
        {
        case SWR_INPUT_COVERAGE_NONE: return BEChooserSingleSample<ArgsT..., SWR_INPUT_COVERAGE_NONE>::GetFunc(remainingArgs...); break;
        case SWR_INPUT_COVERAGE_NORMAL: return BEChooserSingleSample<ArgsT..., SWR_INPUT_COVERAGE_NORMAL>::GetFunc(remainingArgs...); break;
        case SWR_INPUT_COVERAGE_INNER_CONSERVATIVE: return BEChooserSingleSample<ArgsT..., SWR_INPUT_COVERAGE_INNER_CONSERVATIVE>::GetFunc(remainingArgs...); break;
        default:
        SWR_ASSERT(0 && "Invalid sample pattern\n");
        return BEChooserSingleSample<ArgsT..., SWR_INPUT_COVERAGE_NONE>::GetFunc(remainingArgs...);
        break;
        }
    }

    // Recursively parse args
    template <typename... TArgsT>
    static PFN_BACKEND_FUNC GetFunc(SWR_MULTISAMPLE_COUNT tArg, TArgsT... remainingArgs)
    {
        switch(tArg)
        {
        case SWR_MULTISAMPLE_1X: return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_1X>::GetFunc(remainingArgs...); break;
        case SWR_MULTISAMPLE_2X: return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_2X>::GetFunc(remainingArgs...); break;
        case SWR_MULTISAMPLE_4X: return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_4X>::GetFunc(remainingArgs...); break;
        case SWR_MULTISAMPLE_8X: return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_8X>::GetFunc(remainingArgs...); break;
        case SWR_MULTISAMPLE_16X: return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_16X>::GetFunc(remainingArgs...); break;
        default:
        SWR_ASSERT(0 && "Invalid sample count\n");
        return BEChooserSingleSample<ArgsT..., SWR_MULTISAMPLE_1X>::GetFunc(remainingArgs...);
        break;
        }
    }

    // Recursively parse args
    template <typename... TArgsT>
    static PFN_BACKEND_FUNC GetFunc(bool tArg, TArgsT... remainingArgs)
    {
        if(tArg == true)
        {
            return BEChooserSingleSample<ArgsT..., 1>::GetFunc(remainingArgs...);
        }

        return BEChooserSingleSample<ArgsT..., 0>::GetFunc(remainingArgs...);
    }
};

void InitBackendSingleFuncTable(PFN_BACKEND_FUNC (&table)[SWR_INPUT_COVERAGE_COUNT][2][2])
{
    for(uint32_t inputCoverage = 0; inputCoverage < SWR_INPUT_COVERAGE_COUNT; inputCoverage++)
    {
        for(uint32_t isCentroid = 0; isCentroid < 2; isCentroid++)
        {
            for(uint32_t canEarlyZ = 0; canEarlyZ < 2; canEarlyZ++)
            {
                table[inputCoverage][isCentroid][canEarlyZ] =
                    BEChooserSingleSample<>::GetFunc(SWR_MULTISAMPLE_1X, false, (SWR_INPUT_COVERAGE)inputCoverage,
                                         (isCentroid > 0), false, (canEarlyZ > 0), SWR_BACKEND_SINGLE_SAMPLE);
            }
        }
    }
}
