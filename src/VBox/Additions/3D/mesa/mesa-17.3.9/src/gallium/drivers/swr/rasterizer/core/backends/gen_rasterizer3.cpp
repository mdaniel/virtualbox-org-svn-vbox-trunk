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
// @file gen_rasterizer3.cpp
// 
// @brief auto-generated file
// 
// DO NOT EDIT
//
// Generation Command Line:
//  ./rasterizer/codegen/gen_backends.py
//    --outdir
//    rasterizer/core/backends
//    --rast
//    --dim
//    5
//    2
//    2
//    3
//    5
//    2
//    --numfiles
//    4
//    --cpp
//    --hpp
//
//============================================================================

#include "core/rasterizer.h"
#include "core/rasterizer_impl.h"

void InitRasterizerFuncs3()
{
    gRasterizerFuncs[3][1][1][0][0][0] = RasterizeTriangle<RasterizerTraits<3,1,1,0,0,0>>;
    gRasterizerFuncs[3][1][1][0][0][1] = RasterizeTriangle<RasterizerTraits<3,1,1,0,0,1>>;
    gRasterizerFuncs[3][1][1][0][1][0] = RasterizeTriangle<RasterizerTraits<3,1,1,0,1,0>>;
    gRasterizerFuncs[3][1][1][0][1][1] = RasterizeTriangle<RasterizerTraits<3,1,1,0,1,1>>;
    gRasterizerFuncs[3][1][1][0][2][0] = RasterizeTriangle<RasterizerTraits<3,1,1,0,2,0>>;
    gRasterizerFuncs[3][1][1][0][2][1] = RasterizeTriangle<RasterizerTraits<3,1,1,0,2,1>>;
    gRasterizerFuncs[3][1][1][0][3][0] = RasterizeTriangle<RasterizerTraits<3,1,1,0,3,0>>;
    gRasterizerFuncs[3][1][1][0][3][1] = RasterizeTriangle<RasterizerTraits<3,1,1,0,3,1>>;
    gRasterizerFuncs[3][1][1][0][4][0] = RasterizeTriangle<RasterizerTraits<3,1,1,0,4,0>>;
    gRasterizerFuncs[3][1][1][0][4][1] = RasterizeTriangle<RasterizerTraits<3,1,1,0,4,1>>;
    gRasterizerFuncs[3][1][1][1][0][0] = RasterizeTriangle<RasterizerTraits<3,1,1,1,0,0>>;
    gRasterizerFuncs[3][1][1][1][0][1] = RasterizeTriangle<RasterizerTraits<3,1,1,1,0,1>>;
    gRasterizerFuncs[3][1][1][1][1][0] = RasterizeTriangle<RasterizerTraits<3,1,1,1,1,0>>;
    gRasterizerFuncs[3][1][1][1][1][1] = RasterizeTriangle<RasterizerTraits<3,1,1,1,1,1>>;
    gRasterizerFuncs[3][1][1][1][2][0] = RasterizeTriangle<RasterizerTraits<3,1,1,1,2,0>>;
    gRasterizerFuncs[3][1][1][1][2][1] = RasterizeTriangle<RasterizerTraits<3,1,1,1,2,1>>;
    gRasterizerFuncs[3][1][1][1][3][0] = RasterizeTriangle<RasterizerTraits<3,1,1,1,3,0>>;
    gRasterizerFuncs[3][1][1][1][3][1] = RasterizeTriangle<RasterizerTraits<3,1,1,1,3,1>>;
    gRasterizerFuncs[3][1][1][1][4][0] = RasterizeTriangle<RasterizerTraits<3,1,1,1,4,0>>;
    gRasterizerFuncs[3][1][1][1][4][1] = RasterizeTriangle<RasterizerTraits<3,1,1,1,4,1>>;
    gRasterizerFuncs[3][1][1][2][0][0] = RasterizeTriangle<RasterizerTraits<3,1,1,2,0,0>>;
    gRasterizerFuncs[3][1][1][2][0][1] = RasterizeTriangle<RasterizerTraits<3,1,1,2,0,1>>;
    gRasterizerFuncs[3][1][1][2][1][0] = RasterizeTriangle<RasterizerTraits<3,1,1,2,1,0>>;
    gRasterizerFuncs[3][1][1][2][1][1] = RasterizeTriangle<RasterizerTraits<3,1,1,2,1,1>>;
    gRasterizerFuncs[3][1][1][2][2][0] = RasterizeTriangle<RasterizerTraits<3,1,1,2,2,0>>;
    gRasterizerFuncs[3][1][1][2][2][1] = RasterizeTriangle<RasterizerTraits<3,1,1,2,2,1>>;
    gRasterizerFuncs[3][1][1][2][3][0] = RasterizeTriangle<RasterizerTraits<3,1,1,2,3,0>>;
    gRasterizerFuncs[3][1][1][2][3][1] = RasterizeTriangle<RasterizerTraits<3,1,1,2,3,1>>;
    gRasterizerFuncs[3][1][1][2][4][0] = RasterizeTriangle<RasterizerTraits<3,1,1,2,4,0>>;
    gRasterizerFuncs[3][1][1][2][4][1] = RasterizeTriangle<RasterizerTraits<3,1,1,2,4,1>>;
    gRasterizerFuncs[4][0][0][0][0][0] = RasterizeTriangle<RasterizerTraits<4,0,0,0,0,0>>;
    gRasterizerFuncs[4][0][0][0][0][1] = RasterizeTriangle<RasterizerTraits<4,0,0,0,0,1>>;
    gRasterizerFuncs[4][0][0][0][1][0] = RasterizeTriangle<RasterizerTraits<4,0,0,0,1,0>>;
    gRasterizerFuncs[4][0][0][0][1][1] = RasterizeTriangle<RasterizerTraits<4,0,0,0,1,1>>;
    gRasterizerFuncs[4][0][0][0][2][0] = RasterizeTriangle<RasterizerTraits<4,0,0,0,2,0>>;
    gRasterizerFuncs[4][0][0][0][2][1] = RasterizeTriangle<RasterizerTraits<4,0,0,0,2,1>>;
    gRasterizerFuncs[4][0][0][0][3][0] = RasterizeTriangle<RasterizerTraits<4,0,0,0,3,0>>;
    gRasterizerFuncs[4][0][0][0][3][1] = RasterizeTriangle<RasterizerTraits<4,0,0,0,3,1>>;
    gRasterizerFuncs[4][0][0][0][4][0] = RasterizeTriangle<RasterizerTraits<4,0,0,0,4,0>>;
    gRasterizerFuncs[4][0][0][0][4][1] = RasterizeTriangle<RasterizerTraits<4,0,0,0,4,1>>;
    gRasterizerFuncs[4][0][0][1][0][0] = RasterizeTriangle<RasterizerTraits<4,0,0,1,0,0>>;
    gRasterizerFuncs[4][0][0][1][0][1] = RasterizeTriangle<RasterizerTraits<4,0,0,1,0,1>>;
    gRasterizerFuncs[4][0][0][1][1][0] = RasterizeTriangle<RasterizerTraits<4,0,0,1,1,0>>;
    gRasterizerFuncs[4][0][0][1][1][1] = RasterizeTriangle<RasterizerTraits<4,0,0,1,1,1>>;
    gRasterizerFuncs[4][0][0][1][2][0] = RasterizeTriangle<RasterizerTraits<4,0,0,1,2,0>>;
    gRasterizerFuncs[4][0][0][1][2][1] = RasterizeTriangle<RasterizerTraits<4,0,0,1,2,1>>;
    gRasterizerFuncs[4][0][0][1][3][0] = RasterizeTriangle<RasterizerTraits<4,0,0,1,3,0>>;
    gRasterizerFuncs[4][0][0][1][3][1] = RasterizeTriangle<RasterizerTraits<4,0,0,1,3,1>>;
    gRasterizerFuncs[4][0][0][1][4][0] = RasterizeTriangle<RasterizerTraits<4,0,0,1,4,0>>;
    gRasterizerFuncs[4][0][0][1][4][1] = RasterizeTriangle<RasterizerTraits<4,0,0,1,4,1>>;
    gRasterizerFuncs[4][0][0][2][0][0] = RasterizeTriangle<RasterizerTraits<4,0,0,2,0,0>>;
    gRasterizerFuncs[4][0][0][2][0][1] = RasterizeTriangle<RasterizerTraits<4,0,0,2,0,1>>;
    gRasterizerFuncs[4][0][0][2][1][0] = RasterizeTriangle<RasterizerTraits<4,0,0,2,1,0>>;
    gRasterizerFuncs[4][0][0][2][1][1] = RasterizeTriangle<RasterizerTraits<4,0,0,2,1,1>>;
    gRasterizerFuncs[4][0][0][2][2][0] = RasterizeTriangle<RasterizerTraits<4,0,0,2,2,0>>;
    gRasterizerFuncs[4][0][0][2][2][1] = RasterizeTriangle<RasterizerTraits<4,0,0,2,2,1>>;
    gRasterizerFuncs[4][0][0][2][3][0] = RasterizeTriangle<RasterizerTraits<4,0,0,2,3,0>>;
    gRasterizerFuncs[4][0][0][2][3][1] = RasterizeTriangle<RasterizerTraits<4,0,0,2,3,1>>;
    gRasterizerFuncs[4][0][0][2][4][0] = RasterizeTriangle<RasterizerTraits<4,0,0,2,4,0>>;
    gRasterizerFuncs[4][0][0][2][4][1] = RasterizeTriangle<RasterizerTraits<4,0,0,2,4,1>>;
    gRasterizerFuncs[4][0][1][0][0][0] = RasterizeTriangle<RasterizerTraits<4,0,1,0,0,0>>;
    gRasterizerFuncs[4][0][1][0][0][1] = RasterizeTriangle<RasterizerTraits<4,0,1,0,0,1>>;
    gRasterizerFuncs[4][0][1][0][1][0] = RasterizeTriangle<RasterizerTraits<4,0,1,0,1,0>>;
    gRasterizerFuncs[4][0][1][0][1][1] = RasterizeTriangle<RasterizerTraits<4,0,1,0,1,1>>;
    gRasterizerFuncs[4][0][1][0][2][0] = RasterizeTriangle<RasterizerTraits<4,0,1,0,2,0>>;
    gRasterizerFuncs[4][0][1][0][2][1] = RasterizeTriangle<RasterizerTraits<4,0,1,0,2,1>>;
    gRasterizerFuncs[4][0][1][0][3][0] = RasterizeTriangle<RasterizerTraits<4,0,1,0,3,0>>;
    gRasterizerFuncs[4][0][1][0][3][1] = RasterizeTriangle<RasterizerTraits<4,0,1,0,3,1>>;
    gRasterizerFuncs[4][0][1][0][4][0] = RasterizeTriangle<RasterizerTraits<4,0,1,0,4,0>>;
    gRasterizerFuncs[4][0][1][0][4][1] = RasterizeTriangle<RasterizerTraits<4,0,1,0,4,1>>;
    gRasterizerFuncs[4][0][1][1][0][0] = RasterizeTriangle<RasterizerTraits<4,0,1,1,0,0>>;
    gRasterizerFuncs[4][0][1][1][0][1] = RasterizeTriangle<RasterizerTraits<4,0,1,1,0,1>>;
    gRasterizerFuncs[4][0][1][1][1][0] = RasterizeTriangle<RasterizerTraits<4,0,1,1,1,0>>;
    gRasterizerFuncs[4][0][1][1][1][1] = RasterizeTriangle<RasterizerTraits<4,0,1,1,1,1>>;
    gRasterizerFuncs[4][0][1][1][2][0] = RasterizeTriangle<RasterizerTraits<4,0,1,1,2,0>>;
    gRasterizerFuncs[4][0][1][1][2][1] = RasterizeTriangle<RasterizerTraits<4,0,1,1,2,1>>;
    gRasterizerFuncs[4][0][1][1][3][0] = RasterizeTriangle<RasterizerTraits<4,0,1,1,3,0>>;
    gRasterizerFuncs[4][0][1][1][3][1] = RasterizeTriangle<RasterizerTraits<4,0,1,1,3,1>>;
    gRasterizerFuncs[4][0][1][1][4][0] = RasterizeTriangle<RasterizerTraits<4,0,1,1,4,0>>;
    gRasterizerFuncs[4][0][1][1][4][1] = RasterizeTriangle<RasterizerTraits<4,0,1,1,4,1>>;
    gRasterizerFuncs[4][0][1][2][0][0] = RasterizeTriangle<RasterizerTraits<4,0,1,2,0,0>>;
    gRasterizerFuncs[4][0][1][2][0][1] = RasterizeTriangle<RasterizerTraits<4,0,1,2,0,1>>;
    gRasterizerFuncs[4][0][1][2][1][0] = RasterizeTriangle<RasterizerTraits<4,0,1,2,1,0>>;
    gRasterizerFuncs[4][0][1][2][1][1] = RasterizeTriangle<RasterizerTraits<4,0,1,2,1,1>>;
    gRasterizerFuncs[4][0][1][2][2][0] = RasterizeTriangle<RasterizerTraits<4,0,1,2,2,0>>;
    gRasterizerFuncs[4][0][1][2][2][1] = RasterizeTriangle<RasterizerTraits<4,0,1,2,2,1>>;
    gRasterizerFuncs[4][0][1][2][3][0] = RasterizeTriangle<RasterizerTraits<4,0,1,2,3,0>>;
    gRasterizerFuncs[4][0][1][2][3][1] = RasterizeTriangle<RasterizerTraits<4,0,1,2,3,1>>;
    gRasterizerFuncs[4][0][1][2][4][0] = RasterizeTriangle<RasterizerTraits<4,0,1,2,4,0>>;
    gRasterizerFuncs[4][0][1][2][4][1] = RasterizeTriangle<RasterizerTraits<4,0,1,2,4,1>>;
    gRasterizerFuncs[4][1][0][0][0][0] = RasterizeTriangle<RasterizerTraits<4,1,0,0,0,0>>;
    gRasterizerFuncs[4][1][0][0][0][1] = RasterizeTriangle<RasterizerTraits<4,1,0,0,0,1>>;
    gRasterizerFuncs[4][1][0][0][1][0] = RasterizeTriangle<RasterizerTraits<4,1,0,0,1,0>>;
    gRasterizerFuncs[4][1][0][0][1][1] = RasterizeTriangle<RasterizerTraits<4,1,0,0,1,1>>;
    gRasterizerFuncs[4][1][0][0][2][0] = RasterizeTriangle<RasterizerTraits<4,1,0,0,2,0>>;
    gRasterizerFuncs[4][1][0][0][2][1] = RasterizeTriangle<RasterizerTraits<4,1,0,0,2,1>>;
    gRasterizerFuncs[4][1][0][0][3][0] = RasterizeTriangle<RasterizerTraits<4,1,0,0,3,0>>;
    gRasterizerFuncs[4][1][0][0][3][1] = RasterizeTriangle<RasterizerTraits<4,1,0,0,3,1>>;
    gRasterizerFuncs[4][1][0][0][4][0] = RasterizeTriangle<RasterizerTraits<4,1,0,0,4,0>>;
    gRasterizerFuncs[4][1][0][0][4][1] = RasterizeTriangle<RasterizerTraits<4,1,0,0,4,1>>;
    gRasterizerFuncs[4][1][0][1][0][0] = RasterizeTriangle<RasterizerTraits<4,1,0,1,0,0>>;
    gRasterizerFuncs[4][1][0][1][0][1] = RasterizeTriangle<RasterizerTraits<4,1,0,1,0,1>>;
    gRasterizerFuncs[4][1][0][1][1][0] = RasterizeTriangle<RasterizerTraits<4,1,0,1,1,0>>;
    gRasterizerFuncs[4][1][0][1][1][1] = RasterizeTriangle<RasterizerTraits<4,1,0,1,1,1>>;
    gRasterizerFuncs[4][1][0][1][2][0] = RasterizeTriangle<RasterizerTraits<4,1,0,1,2,0>>;
    gRasterizerFuncs[4][1][0][1][2][1] = RasterizeTriangle<RasterizerTraits<4,1,0,1,2,1>>;
    gRasterizerFuncs[4][1][0][1][3][0] = RasterizeTriangle<RasterizerTraits<4,1,0,1,3,0>>;
    gRasterizerFuncs[4][1][0][1][3][1] = RasterizeTriangle<RasterizerTraits<4,1,0,1,3,1>>;
    gRasterizerFuncs[4][1][0][1][4][0] = RasterizeTriangle<RasterizerTraits<4,1,0,1,4,0>>;
    gRasterizerFuncs[4][1][0][1][4][1] = RasterizeTriangle<RasterizerTraits<4,1,0,1,4,1>>;
    gRasterizerFuncs[4][1][0][2][0][0] = RasterizeTriangle<RasterizerTraits<4,1,0,2,0,0>>;
    gRasterizerFuncs[4][1][0][2][0][1] = RasterizeTriangle<RasterizerTraits<4,1,0,2,0,1>>;
    gRasterizerFuncs[4][1][0][2][1][0] = RasterizeTriangle<RasterizerTraits<4,1,0,2,1,0>>;
    gRasterizerFuncs[4][1][0][2][1][1] = RasterizeTriangle<RasterizerTraits<4,1,0,2,1,1>>;
    gRasterizerFuncs[4][1][0][2][2][0] = RasterizeTriangle<RasterizerTraits<4,1,0,2,2,0>>;
    gRasterizerFuncs[4][1][0][2][2][1] = RasterizeTriangle<RasterizerTraits<4,1,0,2,2,1>>;
    gRasterizerFuncs[4][1][0][2][3][0] = RasterizeTriangle<RasterizerTraits<4,1,0,2,3,0>>;
    gRasterizerFuncs[4][1][0][2][3][1] = RasterizeTriangle<RasterizerTraits<4,1,0,2,3,1>>;
    gRasterizerFuncs[4][1][0][2][4][0] = RasterizeTriangle<RasterizerTraits<4,1,0,2,4,0>>;
    gRasterizerFuncs[4][1][0][2][4][1] = RasterizeTriangle<RasterizerTraits<4,1,0,2,4,1>>;
    gRasterizerFuncs[4][1][1][0][0][0] = RasterizeTriangle<RasterizerTraits<4,1,1,0,0,0>>;
    gRasterizerFuncs[4][1][1][0][0][1] = RasterizeTriangle<RasterizerTraits<4,1,1,0,0,1>>;
    gRasterizerFuncs[4][1][1][0][1][0] = RasterizeTriangle<RasterizerTraits<4,1,1,0,1,0>>;
    gRasterizerFuncs[4][1][1][0][1][1] = RasterizeTriangle<RasterizerTraits<4,1,1,0,1,1>>;
    gRasterizerFuncs[4][1][1][0][2][0] = RasterizeTriangle<RasterizerTraits<4,1,1,0,2,0>>;
    gRasterizerFuncs[4][1][1][0][2][1] = RasterizeTriangle<RasterizerTraits<4,1,1,0,2,1>>;
    gRasterizerFuncs[4][1][1][0][3][0] = RasterizeTriangle<RasterizerTraits<4,1,1,0,3,0>>;
    gRasterizerFuncs[4][1][1][0][3][1] = RasterizeTriangle<RasterizerTraits<4,1,1,0,3,1>>;
    gRasterizerFuncs[4][1][1][0][4][0] = RasterizeTriangle<RasterizerTraits<4,1,1,0,4,0>>;
    gRasterizerFuncs[4][1][1][0][4][1] = RasterizeTriangle<RasterizerTraits<4,1,1,0,4,1>>;
    gRasterizerFuncs[4][1][1][1][0][0] = RasterizeTriangle<RasterizerTraits<4,1,1,1,0,0>>;
    gRasterizerFuncs[4][1][1][1][0][1] = RasterizeTriangle<RasterizerTraits<4,1,1,1,0,1>>;
    gRasterizerFuncs[4][1][1][1][1][0] = RasterizeTriangle<RasterizerTraits<4,1,1,1,1,0>>;
    gRasterizerFuncs[4][1][1][1][1][1] = RasterizeTriangle<RasterizerTraits<4,1,1,1,1,1>>;
    gRasterizerFuncs[4][1][1][1][2][0] = RasterizeTriangle<RasterizerTraits<4,1,1,1,2,0>>;
    gRasterizerFuncs[4][1][1][1][2][1] = RasterizeTriangle<RasterizerTraits<4,1,1,1,2,1>>;
    gRasterizerFuncs[4][1][1][1][3][0] = RasterizeTriangle<RasterizerTraits<4,1,1,1,3,0>>;
    gRasterizerFuncs[4][1][1][1][3][1] = RasterizeTriangle<RasterizerTraits<4,1,1,1,3,1>>;
    gRasterizerFuncs[4][1][1][1][4][0] = RasterizeTriangle<RasterizerTraits<4,1,1,1,4,0>>;
    gRasterizerFuncs[4][1][1][1][4][1] = RasterizeTriangle<RasterizerTraits<4,1,1,1,4,1>>;
    gRasterizerFuncs[4][1][1][2][0][0] = RasterizeTriangle<RasterizerTraits<4,1,1,2,0,0>>;
    gRasterizerFuncs[4][1][1][2][0][1] = RasterizeTriangle<RasterizerTraits<4,1,1,2,0,1>>;
    gRasterizerFuncs[4][1][1][2][1][0] = RasterizeTriangle<RasterizerTraits<4,1,1,2,1,0>>;
    gRasterizerFuncs[4][1][1][2][1][1] = RasterizeTriangle<RasterizerTraits<4,1,1,2,1,1>>;
    gRasterizerFuncs[4][1][1][2][2][0] = RasterizeTriangle<RasterizerTraits<4,1,1,2,2,0>>;
    gRasterizerFuncs[4][1][1][2][2][1] = RasterizeTriangle<RasterizerTraits<4,1,1,2,2,1>>;
    gRasterizerFuncs[4][1][1][2][3][0] = RasterizeTriangle<RasterizerTraits<4,1,1,2,3,0>>;
    gRasterizerFuncs[4][1][1][2][3][1] = RasterizeTriangle<RasterizerTraits<4,1,1,2,3,1>>;
    gRasterizerFuncs[4][1][1][2][4][0] = RasterizeTriangle<RasterizerTraits<4,1,1,2,4,0>>;
    gRasterizerFuncs[4][1][1][2][4][1] = RasterizeTriangle<RasterizerTraits<4,1,1,2,4,1>>;
}
