//============================================================================
// Copyright (C) 2017 Intel Corporation.   All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// 
// @file BackendPixelRate1.cpp
// 
// @brief auto-generated file
// 
// DO NOT EDIT
//
// Generation Command Line:
//  ./rasterizer/codegen/gen_backends.py
//    --outdir
//    rasterizer/core/backends
//    --dim
//    5
//    2
//    3
//    2
//    2
//    2
//    --numfiles
//    4
//    --cpp
//    --hpp
//
//============================================================================

#include "core/backend.h"
#include "core/backend_impl.h"

void InitBackendPixelRate1()
{
    gBackendPixelRateTable[1][0][1][1][0][0] = BackendPixelRate<SwrBackendTraits<1,0,1,1,0,0>>;
    gBackendPixelRateTable[1][0][1][1][0][1] = BackendPixelRate<SwrBackendTraits<1,0,1,1,0,1>>;
    gBackendPixelRateTable[1][0][1][1][1][0] = BackendPixelRate<SwrBackendTraits<1,0,1,1,1,0>>;
    gBackendPixelRateTable[1][0][1][1][1][1] = BackendPixelRate<SwrBackendTraits<1,0,1,1,1,1>>;
    gBackendPixelRateTable[1][0][2][0][0][0] = BackendPixelRate<SwrBackendTraits<1,0,2,0,0,0>>;
    gBackendPixelRateTable[1][0][2][0][0][1] = BackendPixelRate<SwrBackendTraits<1,0,2,0,0,1>>;
    gBackendPixelRateTable[1][0][2][0][1][0] = BackendPixelRate<SwrBackendTraits<1,0,2,0,1,0>>;
    gBackendPixelRateTable[1][0][2][0][1][1] = BackendPixelRate<SwrBackendTraits<1,0,2,0,1,1>>;
    gBackendPixelRateTable[1][0][2][1][0][0] = BackendPixelRate<SwrBackendTraits<1,0,2,1,0,0>>;
    gBackendPixelRateTable[1][0][2][1][0][1] = BackendPixelRate<SwrBackendTraits<1,0,2,1,0,1>>;
    gBackendPixelRateTable[1][0][2][1][1][0] = BackendPixelRate<SwrBackendTraits<1,0,2,1,1,0>>;
    gBackendPixelRateTable[1][0][2][1][1][1] = BackendPixelRate<SwrBackendTraits<1,0,2,1,1,1>>;
    gBackendPixelRateTable[1][1][0][0][0][0] = BackendPixelRate<SwrBackendTraits<1,1,0,0,0,0>>;
    gBackendPixelRateTable[1][1][0][0][0][1] = BackendPixelRate<SwrBackendTraits<1,1,0,0,0,1>>;
    gBackendPixelRateTable[1][1][0][0][1][0] = BackendPixelRate<SwrBackendTraits<1,1,0,0,1,0>>;
    gBackendPixelRateTable[1][1][0][0][1][1] = BackendPixelRate<SwrBackendTraits<1,1,0,0,1,1>>;
    gBackendPixelRateTable[1][1][0][1][0][0] = BackendPixelRate<SwrBackendTraits<1,1,0,1,0,0>>;
    gBackendPixelRateTable[1][1][0][1][0][1] = BackendPixelRate<SwrBackendTraits<1,1,0,1,0,1>>;
    gBackendPixelRateTable[1][1][0][1][1][0] = BackendPixelRate<SwrBackendTraits<1,1,0,1,1,0>>;
    gBackendPixelRateTable[1][1][0][1][1][1] = BackendPixelRate<SwrBackendTraits<1,1,0,1,1,1>>;
    gBackendPixelRateTable[1][1][1][0][0][0] = BackendPixelRate<SwrBackendTraits<1,1,1,0,0,0>>;
    gBackendPixelRateTable[1][1][1][0][0][1] = BackendPixelRate<SwrBackendTraits<1,1,1,0,0,1>>;
    gBackendPixelRateTable[1][1][1][0][1][0] = BackendPixelRate<SwrBackendTraits<1,1,1,0,1,0>>;
    gBackendPixelRateTable[1][1][1][0][1][1] = BackendPixelRate<SwrBackendTraits<1,1,1,0,1,1>>;
    gBackendPixelRateTable[1][1][1][1][0][0] = BackendPixelRate<SwrBackendTraits<1,1,1,1,0,0>>;
    gBackendPixelRateTable[1][1][1][1][0][1] = BackendPixelRate<SwrBackendTraits<1,1,1,1,0,1>>;
    gBackendPixelRateTable[1][1][1][1][1][0] = BackendPixelRate<SwrBackendTraits<1,1,1,1,1,0>>;
    gBackendPixelRateTable[1][1][1][1][1][1] = BackendPixelRate<SwrBackendTraits<1,1,1,1,1,1>>;
    gBackendPixelRateTable[1][1][2][0][0][0] = BackendPixelRate<SwrBackendTraits<1,1,2,0,0,0>>;
    gBackendPixelRateTable[1][1][2][0][0][1] = BackendPixelRate<SwrBackendTraits<1,1,2,0,0,1>>;
    gBackendPixelRateTable[1][1][2][0][1][0] = BackendPixelRate<SwrBackendTraits<1,1,2,0,1,0>>;
    gBackendPixelRateTable[1][1][2][0][1][1] = BackendPixelRate<SwrBackendTraits<1,1,2,0,1,1>>;
    gBackendPixelRateTable[1][1][2][1][0][0] = BackendPixelRate<SwrBackendTraits<1,1,2,1,0,0>>;
    gBackendPixelRateTable[1][1][2][1][0][1] = BackendPixelRate<SwrBackendTraits<1,1,2,1,0,1>>;
    gBackendPixelRateTable[1][1][2][1][1][0] = BackendPixelRate<SwrBackendTraits<1,1,2,1,1,0>>;
    gBackendPixelRateTable[1][1][2][1][1][1] = BackendPixelRate<SwrBackendTraits<1,1,2,1,1,1>>;
    gBackendPixelRateTable[2][0][0][0][0][0] = BackendPixelRate<SwrBackendTraits<2,0,0,0,0,0>>;
    gBackendPixelRateTable[2][0][0][0][0][1] = BackendPixelRate<SwrBackendTraits<2,0,0,0,0,1>>;
    gBackendPixelRateTable[2][0][0][0][1][0] = BackendPixelRate<SwrBackendTraits<2,0,0,0,1,0>>;
    gBackendPixelRateTable[2][0][0][0][1][1] = BackendPixelRate<SwrBackendTraits<2,0,0,0,1,1>>;
    gBackendPixelRateTable[2][0][0][1][0][0] = BackendPixelRate<SwrBackendTraits<2,0,0,1,0,0>>;
    gBackendPixelRateTable[2][0][0][1][0][1] = BackendPixelRate<SwrBackendTraits<2,0,0,1,0,1>>;
    gBackendPixelRateTable[2][0][0][1][1][0] = BackendPixelRate<SwrBackendTraits<2,0,0,1,1,0>>;
    gBackendPixelRateTable[2][0][0][1][1][1] = BackendPixelRate<SwrBackendTraits<2,0,0,1,1,1>>;
    gBackendPixelRateTable[2][0][1][0][0][0] = BackendPixelRate<SwrBackendTraits<2,0,1,0,0,0>>;
    gBackendPixelRateTable[2][0][1][0][0][1] = BackendPixelRate<SwrBackendTraits<2,0,1,0,0,1>>;
    gBackendPixelRateTable[2][0][1][0][1][0] = BackendPixelRate<SwrBackendTraits<2,0,1,0,1,0>>;
    gBackendPixelRateTable[2][0][1][0][1][1] = BackendPixelRate<SwrBackendTraits<2,0,1,0,1,1>>;
    gBackendPixelRateTable[2][0][1][1][0][0] = BackendPixelRate<SwrBackendTraits<2,0,1,1,0,0>>;
    gBackendPixelRateTable[2][0][1][1][0][1] = BackendPixelRate<SwrBackendTraits<2,0,1,1,0,1>>;
    gBackendPixelRateTable[2][0][1][1][1][0] = BackendPixelRate<SwrBackendTraits<2,0,1,1,1,0>>;
    gBackendPixelRateTable[2][0][1][1][1][1] = BackendPixelRate<SwrBackendTraits<2,0,1,1,1,1>>;
    gBackendPixelRateTable[2][0][2][0][0][0] = BackendPixelRate<SwrBackendTraits<2,0,2,0,0,0>>;
    gBackendPixelRateTable[2][0][2][0][0][1] = BackendPixelRate<SwrBackendTraits<2,0,2,0,0,1>>;
    gBackendPixelRateTable[2][0][2][0][1][0] = BackendPixelRate<SwrBackendTraits<2,0,2,0,1,0>>;
    gBackendPixelRateTable[2][0][2][0][1][1] = BackendPixelRate<SwrBackendTraits<2,0,2,0,1,1>>;
    gBackendPixelRateTable[2][0][2][1][0][0] = BackendPixelRate<SwrBackendTraits<2,0,2,1,0,0>>;
    gBackendPixelRateTable[2][0][2][1][0][1] = BackendPixelRate<SwrBackendTraits<2,0,2,1,0,1>>;
    gBackendPixelRateTable[2][0][2][1][1][0] = BackendPixelRate<SwrBackendTraits<2,0,2,1,1,0>>;
    gBackendPixelRateTable[2][0][2][1][1][1] = BackendPixelRate<SwrBackendTraits<2,0,2,1,1,1>>;
}
