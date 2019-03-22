/****************************************************************************
* Copyright (C) 2016 Intel Corporation.   All Rights Reserved.
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
* @file gen_ar_eventhandler.hpp
*
* @brief Event handler interface.  auto-generated file
* 
* DO NOT EDIT
*
* Generation Command Line:
*  ./rasterizer/codegen/gen_archrast.py
*    --proto
*    ./rasterizer/archrast/events.proto
*    --output
*    rasterizer/archrast/gen_ar_eventhandler.hpp
*    --gen_eventhandler_h
*
******************************************************************************/
#pragma once

#include "gen_ar_event.hpp"

namespace ArchRast
{
    //////////////////////////////////////////////////////////////////////////
    /// EventHandler - interface for handling events.
    //////////////////////////////////////////////////////////////////////////
    class EventHandler
    {
    public:
        EventHandler() {}
        virtual ~EventHandler() {}

        virtual void FlushDraw(uint32_t drawId) {}

        virtual void Handle(const Start& event) {}
        virtual void Handle(const End& event) {}
        virtual void Handle(const ThreadStartApiEvent& event) {}
        virtual void Handle(const ThreadStartWorkerEvent& event) {}
        virtual void Handle(const DrawInstancedEvent& event) {}
        virtual void Handle(const DrawIndexedInstancedEvent& event) {}
        virtual void Handle(const DispatchEvent& event) {}
        virtual void Handle(const FrameEndEvent& event) {}
        virtual void Handle(const DrawInstancedSplitEvent& event) {}
        virtual void Handle(const DrawIndexedInstancedSplitEvent& event) {}
        virtual void Handle(const SwrSyncEvent& event) {}
        virtual void Handle(const SwrInvalidateTilesEvent& event) {}
        virtual void Handle(const SwrDiscardRectEvent& event) {}
        virtual void Handle(const SwrStoreTilesEvent& event) {}
        virtual void Handle(const FrontendStatsEvent& event) {}
        virtual void Handle(const BackendStatsEvent& event) {}
        virtual void Handle(const EarlyDepthStencilInfoSingleSample& event) {}
        virtual void Handle(const EarlyDepthStencilInfoSampleRate& event) {}
        virtual void Handle(const EarlyDepthStencilInfoNullPS& event) {}
        virtual void Handle(const LateDepthStencilInfoSingleSample& event) {}
        virtual void Handle(const LateDepthStencilInfoSampleRate& event) {}
        virtual void Handle(const LateDepthStencilInfoNullPS& event) {}
        virtual void Handle(const EarlyDepthInfoPixelRate& event) {}
        virtual void Handle(const LateDepthInfoPixelRate& event) {}
        virtual void Handle(const BackendDrawEndEvent& event) {}
        virtual void Handle(const FrontendDrawEndEvent& event) {}
        virtual void Handle(const EarlyZSingleSample& event) {}
        virtual void Handle(const LateZSingleSample& event) {}
        virtual void Handle(const EarlyStencilSingleSample& event) {}
        virtual void Handle(const LateStencilSingleSample& event) {}
        virtual void Handle(const EarlyZSampleRate& event) {}
        virtual void Handle(const LateZSampleRate& event) {}
        virtual void Handle(const EarlyStencilSampleRate& event) {}
        virtual void Handle(const LateStencilSampleRate& event) {}
        virtual void Handle(const EarlyZNullPS& event) {}
        virtual void Handle(const EarlyStencilNullPS& event) {}
        virtual void Handle(const EarlyZPixelRate& event) {}
        virtual void Handle(const LateZPixelRate& event) {}
        virtual void Handle(const EarlyOmZ& event) {}
        virtual void Handle(const EarlyOmStencil& event) {}
        virtual void Handle(const LateOmZ& event) {}
        virtual void Handle(const LateOmStencil& event) {}
        virtual void Handle(const GSPrimInfo& event) {}
        virtual void Handle(const GSInputPrims& event) {}
        virtual void Handle(const GSPrimsGen& event) {}
        virtual void Handle(const GSVertsInput& event) {}
        virtual void Handle(const ClipVertexCount& event) {}
        virtual void Handle(const FlushVertClip& event) {}
        virtual void Handle(const VertsClipped& event) {}
        virtual void Handle(const TessPrimCount& event) {}
        virtual void Handle(const TessPrimFlush& event) {}
        virtual void Handle(const TessPrims& event) {}
    };
}
