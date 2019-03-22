/*
 * Copyright © 2015 Intel Corporation
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
 */

#include "anv_private.h"
#include "vk_enum_to_str.h"
#include "vk_format_info.h"
#include "vk_util.h"

/*
 * gcc-4 and earlier don't allow compound literals where a constant
 * is required in -std=c99/gnu99 mode, so we can't use ISL_SWIZZLE()
 * here. -std=c89/gnu89 would allow it, but we depend on c99 features
 * so using -std=c89/gnu89 is not an option. Starting from gcc-5
 * compound literals can also be considered constant in -std=c99/gnu99
 * mode.
 */
#define _ISL_SWIZZLE(r, g, b, a) { \
      ISL_CHANNEL_SELECT_##r, \
      ISL_CHANNEL_SELECT_##g, \
      ISL_CHANNEL_SELECT_##b, \
      ISL_CHANNEL_SELECT_##a, \
}

#define RGBA _ISL_SWIZZLE(RED, GREEN, BLUE, ALPHA)
#define BGRA _ISL_SWIZZLE(BLUE, GREEN, RED, ALPHA)
#define RGB1 _ISL_SWIZZLE(RED, GREEN, BLUE, ONE)

#define swiz_fmt1(__vk_fmt, __hw_fmt, __swizzle) \
   [VK_ENUM_OFFSET(__vk_fmt)] = { \
      .planes = { \
         { .isl_format = __hw_fmt, .swizzle = __swizzle, \
           .denominator_scales = { 1, 1, }, \
         }, \
      }, \
      .n_planes = 1, \
   }

#define fmt1(__vk_fmt, __hw_fmt) \
   swiz_fmt1(__vk_fmt, __hw_fmt, RGBA)

#define fmt2(__vk_fmt, __fmt1, __fmt2) \
   [VK_ENUM_OFFSET(__vk_fmt)] = { \
      .planes = { \
         { .isl_format = __fmt1, \
           .swizzle = RGBA,       \
           .denominator_scales = { 1, 1, }, \
         }, \
         { .isl_format = __fmt2, \
           .swizzle = RGBA,       \
           .denominator_scales = { 1, 1, }, \
         }, \
      }, \
      .n_planes = 2, \
   }

#define fmt_unsupported(__vk_fmt) \
   [VK_ENUM_OFFSET(__vk_fmt)] = { \
      .planes = { \
         { .isl_format = ISL_FORMAT_UNSUPPORTED, }, \
      }, \
   }

#define y_plane(__hw_fmt, __swizzle, __ycbcr_swizzle, dhs, dvs) \
   { .isl_format = __hw_fmt, \
     .swizzle = __swizzle, \
     .ycbcr_swizzle = __ycbcr_swizzle, \
     .denominator_scales = { dhs, dvs, }, \
     .has_chroma = false, \
   }

#define chroma_plane(__hw_fmt, __swizzle, __ycbcr_swizzle, dhs, dvs) \
   { .isl_format = __hw_fmt, \
     .swizzle = __swizzle, \
     .ycbcr_swizzle = __ycbcr_swizzle, \
     .denominator_scales = { dhs, dvs, }, \
     .has_chroma = true, \
   }

#define ycbcr_fmt(__vk_fmt, __n_planes, ...) \
   [VK_ENUM_OFFSET(__vk_fmt)] = { \
      .planes = { \
         __VA_ARGS__, \
      }, \
      .n_planes = __n_planes, \
      .can_ycbcr = true, \
   }

/* HINT: For array formats, the ISL name should match the VK name.  For
 * packed formats, they should have the channels in reverse order from each
 * other.  The reason for this is that, for packed formats, the ISL (and
 * bspec) names are in LSB -> MSB order while VK formats are MSB -> LSB.
 */
static const struct anv_format main_formats[] = {
   fmt_unsupported(VK_FORMAT_UNDEFINED),
   fmt_unsupported(VK_FORMAT_R4G4_UNORM_PACK8),
   fmt1(VK_FORMAT_R4G4B4A4_UNORM_PACK16,             ISL_FORMAT_A4B4G4R4_UNORM),
   swiz_fmt1(VK_FORMAT_B4G4R4A4_UNORM_PACK16,        ISL_FORMAT_A4B4G4R4_UNORM,  BGRA),
   fmt1(VK_FORMAT_R5G6B5_UNORM_PACK16,               ISL_FORMAT_B5G6R5_UNORM),
   swiz_fmt1(VK_FORMAT_B5G6R5_UNORM_PACK16,          ISL_FORMAT_B5G6R5_UNORM, BGRA),
   fmt1(VK_FORMAT_R5G5B5A1_UNORM_PACK16,             ISL_FORMAT_A1B5G5R5_UNORM),
   fmt_unsupported(VK_FORMAT_B5G5R5A1_UNORM_PACK16),
   fmt1(VK_FORMAT_A1R5G5B5_UNORM_PACK16,             ISL_FORMAT_B5G5R5A1_UNORM),
   fmt1(VK_FORMAT_R8_UNORM,                          ISL_FORMAT_R8_UNORM),
   fmt1(VK_FORMAT_R8_SNORM,                          ISL_FORMAT_R8_SNORM),
   fmt1(VK_FORMAT_R8_USCALED,                        ISL_FORMAT_R8_USCALED),
   fmt1(VK_FORMAT_R8_SSCALED,                        ISL_FORMAT_R8_SSCALED),
   fmt1(VK_FORMAT_R8_UINT,                           ISL_FORMAT_R8_UINT),
   fmt1(VK_FORMAT_R8_SINT,                           ISL_FORMAT_R8_SINT),
   swiz_fmt1(VK_FORMAT_R8_SRGB,                      ISL_FORMAT_L8_UNORM_SRGB,
                                                     _ISL_SWIZZLE(RED, ZERO, ZERO, ONE)),
   fmt1(VK_FORMAT_R8G8_UNORM,                        ISL_FORMAT_R8G8_UNORM),
   fmt1(VK_FORMAT_R8G8_SNORM,                        ISL_FORMAT_R8G8_SNORM),
   fmt1(VK_FORMAT_R8G8_USCALED,                      ISL_FORMAT_R8G8_USCALED),
   fmt1(VK_FORMAT_R8G8_SSCALED,                      ISL_FORMAT_R8G8_SSCALED),
   fmt1(VK_FORMAT_R8G8_UINT,                         ISL_FORMAT_R8G8_UINT),
   fmt1(VK_FORMAT_R8G8_SINT,                         ISL_FORMAT_R8G8_SINT),
   fmt_unsupported(VK_FORMAT_R8G8_SRGB),             /* L8A8_UNORM_SRGB */
   fmt1(VK_FORMAT_R8G8B8_UNORM,                      ISL_FORMAT_R8G8B8_UNORM),
   fmt1(VK_FORMAT_R8G8B8_SNORM,                      ISL_FORMAT_R8G8B8_SNORM),
   fmt1(VK_FORMAT_R8G8B8_USCALED,                    ISL_FORMAT_R8G8B8_USCALED),
   fmt1(VK_FORMAT_R8G8B8_SSCALED,                    ISL_FORMAT_R8G8B8_SSCALED),
   fmt1(VK_FORMAT_R8G8B8_UINT,                       ISL_FORMAT_R8G8B8_UINT),
   fmt1(VK_FORMAT_R8G8B8_SINT,                       ISL_FORMAT_R8G8B8_SINT),
   fmt1(VK_FORMAT_R8G8B8_SRGB,                       ISL_FORMAT_R8G8B8_UNORM_SRGB),
   fmt1(VK_FORMAT_R8G8B8A8_UNORM,                    ISL_FORMAT_R8G8B8A8_UNORM),
   fmt1(VK_FORMAT_R8G8B8A8_SNORM,                    ISL_FORMAT_R8G8B8A8_SNORM),
   fmt1(VK_FORMAT_R8G8B8A8_USCALED,                  ISL_FORMAT_R8G8B8A8_USCALED),
   fmt1(VK_FORMAT_R8G8B8A8_SSCALED,                  ISL_FORMAT_R8G8B8A8_SSCALED),
   fmt1(VK_FORMAT_R8G8B8A8_UINT,                     ISL_FORMAT_R8G8B8A8_UINT),
   fmt1(VK_FORMAT_R8G8B8A8_SINT,                     ISL_FORMAT_R8G8B8A8_SINT),
   fmt1(VK_FORMAT_R8G8B8A8_SRGB,                     ISL_FORMAT_R8G8B8A8_UNORM_SRGB),
   fmt1(VK_FORMAT_A8B8G8R8_UNORM_PACK32,             ISL_FORMAT_R8G8B8A8_UNORM),
   fmt1(VK_FORMAT_A8B8G8R8_SNORM_PACK32,             ISL_FORMAT_R8G8B8A8_SNORM),
   fmt1(VK_FORMAT_A8B8G8R8_USCALED_PACK32,           ISL_FORMAT_R8G8B8A8_USCALED),
   fmt1(VK_FORMAT_A8B8G8R8_SSCALED_PACK32,           ISL_FORMAT_R8G8B8A8_SSCALED),
   fmt1(VK_FORMAT_A8B8G8R8_UINT_PACK32,              ISL_FORMAT_R8G8B8A8_UINT),
   fmt1(VK_FORMAT_A8B8G8R8_SINT_PACK32,              ISL_FORMAT_R8G8B8A8_SINT),
   fmt1(VK_FORMAT_A8B8G8R8_SRGB_PACK32,              ISL_FORMAT_R8G8B8A8_UNORM_SRGB),
   fmt1(VK_FORMAT_A2R10G10B10_UNORM_PACK32,          ISL_FORMAT_B10G10R10A2_UNORM),
   fmt1(VK_FORMAT_A2R10G10B10_SNORM_PACK32,          ISL_FORMAT_B10G10R10A2_SNORM),
   fmt1(VK_FORMAT_A2R10G10B10_USCALED_PACK32,        ISL_FORMAT_B10G10R10A2_USCALED),
   fmt1(VK_FORMAT_A2R10G10B10_SSCALED_PACK32,        ISL_FORMAT_B10G10R10A2_SSCALED),
   fmt1(VK_FORMAT_A2R10G10B10_UINT_PACK32,           ISL_FORMAT_B10G10R10A2_UINT),
   fmt1(VK_FORMAT_A2R10G10B10_SINT_PACK32,           ISL_FORMAT_B10G10R10A2_SINT),
   fmt1(VK_FORMAT_A2B10G10R10_UNORM_PACK32,          ISL_FORMAT_R10G10B10A2_UNORM),
   fmt1(VK_FORMAT_A2B10G10R10_SNORM_PACK32,          ISL_FORMAT_R10G10B10A2_SNORM),
   fmt1(VK_FORMAT_A2B10G10R10_USCALED_PACK32,        ISL_FORMAT_R10G10B10A2_USCALED),
   fmt1(VK_FORMAT_A2B10G10R10_SSCALED_PACK32,        ISL_FORMAT_R10G10B10A2_SSCALED),
   fmt1(VK_FORMAT_A2B10G10R10_UINT_PACK32,           ISL_FORMAT_R10G10B10A2_UINT),
   fmt1(VK_FORMAT_A2B10G10R10_SINT_PACK32,           ISL_FORMAT_R10G10B10A2_SINT),
   fmt1(VK_FORMAT_R16_UNORM,                         ISL_FORMAT_R16_UNORM),
   fmt1(VK_FORMAT_R16_SNORM,                         ISL_FORMAT_R16_SNORM),
   fmt1(VK_FORMAT_R16_USCALED,                       ISL_FORMAT_R16_USCALED),
   fmt1(VK_FORMAT_R16_SSCALED,                       ISL_FORMAT_R16_SSCALED),
   fmt1(VK_FORMAT_R16_UINT,                          ISL_FORMAT_R16_UINT),
   fmt1(VK_FORMAT_R16_SINT,                          ISL_FORMAT_R16_SINT),
   fmt1(VK_FORMAT_R16_SFLOAT,                        ISL_FORMAT_R16_FLOAT),
   fmt1(VK_FORMAT_R16G16_UNORM,                      ISL_FORMAT_R16G16_UNORM),
   fmt1(VK_FORMAT_R16G16_SNORM,                      ISL_FORMAT_R16G16_SNORM),
   fmt1(VK_FORMAT_R16G16_USCALED,                    ISL_FORMAT_R16G16_USCALED),
   fmt1(VK_FORMAT_R16G16_SSCALED,                    ISL_FORMAT_R16G16_SSCALED),
   fmt1(VK_FORMAT_R16G16_UINT,                       ISL_FORMAT_R16G16_UINT),
   fmt1(VK_FORMAT_R16G16_SINT,                       ISL_FORMAT_R16G16_SINT),
   fmt1(VK_FORMAT_R16G16_SFLOAT,                     ISL_FORMAT_R16G16_FLOAT),
   fmt1(VK_FORMAT_R16G16B16_UNORM,                   ISL_FORMAT_R16G16B16_UNORM),
   fmt1(VK_FORMAT_R16G16B16_SNORM,                   ISL_FORMAT_R16G16B16_SNORM),
   fmt1(VK_FORMAT_R16G16B16_USCALED,                 ISL_FORMAT_R16G16B16_USCALED),
   fmt1(VK_FORMAT_R16G16B16_SSCALED,                 ISL_FORMAT_R16G16B16_SSCALED),
   fmt1(VK_FORMAT_R16G16B16_UINT,                    ISL_FORMAT_R16G16B16_UINT),
   fmt1(VK_FORMAT_R16G16B16_SINT,                    ISL_FORMAT_R16G16B16_SINT),
   fmt1(VK_FORMAT_R16G16B16_SFLOAT,                  ISL_FORMAT_R16G16B16_FLOAT),
   fmt1(VK_FORMAT_R16G16B16A16_UNORM,                ISL_FORMAT_R16G16B16A16_UNORM),
   fmt1(VK_FORMAT_R16G16B16A16_SNORM,                ISL_FORMAT_R16G16B16A16_SNORM),
   fmt1(VK_FORMAT_R16G16B16A16_USCALED,              ISL_FORMAT_R16G16B16A16_USCALED),
   fmt1(VK_FORMAT_R16G16B16A16_SSCALED,              ISL_FORMAT_R16G16B16A16_SSCALED),
   fmt1(VK_FORMAT_R16G16B16A16_UINT,                 ISL_FORMAT_R16G16B16A16_UINT),
   fmt1(VK_FORMAT_R16G16B16A16_SINT,                 ISL_FORMAT_R16G16B16A16_SINT),
   fmt1(VK_FORMAT_R16G16B16A16_SFLOAT,               ISL_FORMAT_R16G16B16A16_FLOAT),
   fmt1(VK_FORMAT_R32_UINT,                          ISL_FORMAT_R32_UINT),
   fmt1(VK_FORMAT_R32_SINT,                          ISL_FORMAT_R32_SINT),
   fmt1(VK_FORMAT_R32_SFLOAT,                        ISL_FORMAT_R32_FLOAT),
   fmt1(VK_FORMAT_R32G32_UINT,                       ISL_FORMAT_R32G32_UINT),
   fmt1(VK_FORMAT_R32G32_SINT,                       ISL_FORMAT_R32G32_SINT),
   fmt1(VK_FORMAT_R32G32_SFLOAT,                     ISL_FORMAT_R32G32_FLOAT),
   fmt1(VK_FORMAT_R32G32B32_UINT,                    ISL_FORMAT_R32G32B32_UINT),
   fmt1(VK_FORMAT_R32G32B32_SINT,                    ISL_FORMAT_R32G32B32_SINT),
   fmt1(VK_FORMAT_R32G32B32_SFLOAT,                  ISL_FORMAT_R32G32B32_FLOAT),
   fmt1(VK_FORMAT_R32G32B32A32_UINT,                 ISL_FORMAT_R32G32B32A32_UINT),
   fmt1(VK_FORMAT_R32G32B32A32_SINT,                 ISL_FORMAT_R32G32B32A32_SINT),
   fmt1(VK_FORMAT_R32G32B32A32_SFLOAT,               ISL_FORMAT_R32G32B32A32_FLOAT),
   fmt1(VK_FORMAT_R64_UINT,                          ISL_FORMAT_R64_PASSTHRU),
   fmt1(VK_FORMAT_R64_SINT,                          ISL_FORMAT_R64_PASSTHRU),
   fmt1(VK_FORMAT_R64_SFLOAT,                        ISL_FORMAT_R64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64_UINT,                       ISL_FORMAT_R64G64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64_SINT,                       ISL_FORMAT_R64G64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64_SFLOAT,                     ISL_FORMAT_R64G64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64_UINT,                    ISL_FORMAT_R64G64B64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64_SINT,                    ISL_FORMAT_R64G64B64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64_SFLOAT,                  ISL_FORMAT_R64G64B64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64A64_UINT,                 ISL_FORMAT_R64G64B64A64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64A64_SINT,                 ISL_FORMAT_R64G64B64A64_PASSTHRU),
   fmt1(VK_FORMAT_R64G64B64A64_SFLOAT,               ISL_FORMAT_R64G64B64A64_PASSTHRU),
   fmt1(VK_FORMAT_B10G11R11_UFLOAT_PACK32,           ISL_FORMAT_R11G11B10_FLOAT),
   fmt1(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,            ISL_FORMAT_R9G9B9E5_SHAREDEXP),

   fmt1(VK_FORMAT_D16_UNORM,                         ISL_FORMAT_R16_UNORM),
   fmt1(VK_FORMAT_X8_D24_UNORM_PACK32,               ISL_FORMAT_R24_UNORM_X8_TYPELESS),
   fmt1(VK_FORMAT_D32_SFLOAT,                        ISL_FORMAT_R32_FLOAT),
   fmt1(VK_FORMAT_S8_UINT,                           ISL_FORMAT_R8_UINT),
   fmt_unsupported(VK_FORMAT_D16_UNORM_S8_UINT),
   fmt2(VK_FORMAT_D24_UNORM_S8_UINT,                 ISL_FORMAT_R24_UNORM_X8_TYPELESS, ISL_FORMAT_R8_UINT),
   fmt2(VK_FORMAT_D32_SFLOAT_S8_UINT,                ISL_FORMAT_R32_FLOAT, ISL_FORMAT_R8_UINT),

   swiz_fmt1(VK_FORMAT_BC1_RGB_UNORM_BLOCK,          ISL_FORMAT_BC1_UNORM, RGB1),
   swiz_fmt1(VK_FORMAT_BC1_RGB_SRGB_BLOCK,           ISL_FORMAT_BC1_UNORM_SRGB, RGB1),
   fmt1(VK_FORMAT_BC1_RGBA_UNORM_BLOCK,              ISL_FORMAT_BC1_UNORM),
   fmt1(VK_FORMAT_BC1_RGBA_SRGB_BLOCK,               ISL_FORMAT_BC1_UNORM_SRGB),
   fmt1(VK_FORMAT_BC2_UNORM_BLOCK,                   ISL_FORMAT_BC2_UNORM),
   fmt1(VK_FORMAT_BC2_SRGB_BLOCK,                    ISL_FORMAT_BC2_UNORM_SRGB),
   fmt1(VK_FORMAT_BC3_UNORM_BLOCK,                   ISL_FORMAT_BC3_UNORM),
   fmt1(VK_FORMAT_BC3_SRGB_BLOCK,                    ISL_FORMAT_BC3_UNORM_SRGB),
   fmt1(VK_FORMAT_BC4_UNORM_BLOCK,                   ISL_FORMAT_BC4_UNORM),
   fmt1(VK_FORMAT_BC4_SNORM_BLOCK,                   ISL_FORMAT_BC4_SNORM),
   fmt1(VK_FORMAT_BC5_UNORM_BLOCK,                   ISL_FORMAT_BC5_UNORM),
   fmt1(VK_FORMAT_BC5_SNORM_BLOCK,                   ISL_FORMAT_BC5_SNORM),
   fmt1(VK_FORMAT_BC6H_UFLOAT_BLOCK,                 ISL_FORMAT_BC6H_UF16),
   fmt1(VK_FORMAT_BC6H_SFLOAT_BLOCK,                 ISL_FORMAT_BC6H_SF16),
   fmt1(VK_FORMAT_BC7_UNORM_BLOCK,                   ISL_FORMAT_BC7_UNORM),
   fmt1(VK_FORMAT_BC7_SRGB_BLOCK,                    ISL_FORMAT_BC7_UNORM_SRGB),
   fmt1(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,           ISL_FORMAT_ETC2_RGB8),
   fmt1(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,            ISL_FORMAT_ETC2_SRGB8),
   fmt1(VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,         ISL_FORMAT_ETC2_RGB8_PTA),
   fmt1(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,          ISL_FORMAT_ETC2_SRGB8_PTA),
   fmt1(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,         ISL_FORMAT_ETC2_EAC_RGBA8),
   fmt1(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,          ISL_FORMAT_ETC2_EAC_SRGB8_A8),
   fmt1(VK_FORMAT_EAC_R11_UNORM_BLOCK,               ISL_FORMAT_EAC_R11),
   fmt1(VK_FORMAT_EAC_R11_SNORM_BLOCK,               ISL_FORMAT_EAC_SIGNED_R11),
   fmt1(VK_FORMAT_EAC_R11G11_UNORM_BLOCK,            ISL_FORMAT_EAC_RG11),
   fmt1(VK_FORMAT_EAC_R11G11_SNORM_BLOCK,            ISL_FORMAT_EAC_SIGNED_RG11),
   fmt1(VK_FORMAT_ASTC_4x4_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_4X4_U8SRGB),
   fmt1(VK_FORMAT_ASTC_5x4_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_5X4_U8SRGB),
   fmt1(VK_FORMAT_ASTC_5x5_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_5X5_U8SRGB),
   fmt1(VK_FORMAT_ASTC_6x5_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_6X5_U8SRGB),
   fmt1(VK_FORMAT_ASTC_6x6_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_6X6_U8SRGB),
   fmt1(VK_FORMAT_ASTC_8x5_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_8X5_U8SRGB),
   fmt1(VK_FORMAT_ASTC_8x6_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_8X6_U8SRGB),
   fmt1(VK_FORMAT_ASTC_8x8_SRGB_BLOCK,               ISL_FORMAT_ASTC_LDR_2D_8X8_U8SRGB),
   fmt1(VK_FORMAT_ASTC_10x5_SRGB_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_10X5_U8SRGB),
   fmt1(VK_FORMAT_ASTC_10x6_SRGB_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_10X6_U8SRGB),
   fmt1(VK_FORMAT_ASTC_10x8_SRGB_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_10X8_U8SRGB),
   fmt1(VK_FORMAT_ASTC_10x10_SRGB_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_10X10_U8SRGB),
   fmt1(VK_FORMAT_ASTC_12x10_SRGB_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_12X10_U8SRGB),
   fmt1(VK_FORMAT_ASTC_12x12_SRGB_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_12X12_U8SRGB),
   fmt1(VK_FORMAT_ASTC_4x4_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_4X4_FLT16),
   fmt1(VK_FORMAT_ASTC_5x4_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_5X4_FLT16),
   fmt1(VK_FORMAT_ASTC_5x5_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_5X5_FLT16),
   fmt1(VK_FORMAT_ASTC_6x5_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_6X5_FLT16),
   fmt1(VK_FORMAT_ASTC_6x6_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_6X6_FLT16),
   fmt1(VK_FORMAT_ASTC_8x5_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_8X5_FLT16),
   fmt1(VK_FORMAT_ASTC_8x6_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_8X6_FLT16),
   fmt1(VK_FORMAT_ASTC_8x8_UNORM_BLOCK,              ISL_FORMAT_ASTC_LDR_2D_8X8_FLT16),
   fmt1(VK_FORMAT_ASTC_10x5_UNORM_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_10X5_FLT16),
   fmt1(VK_FORMAT_ASTC_10x6_UNORM_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_10X6_FLT16),
   fmt1(VK_FORMAT_ASTC_10x8_UNORM_BLOCK,             ISL_FORMAT_ASTC_LDR_2D_10X8_FLT16),
   fmt1(VK_FORMAT_ASTC_10x10_UNORM_BLOCK,            ISL_FORMAT_ASTC_LDR_2D_10X10_FLT16),
   fmt1(VK_FORMAT_ASTC_12x10_UNORM_BLOCK,            ISL_FORMAT_ASTC_LDR_2D_12X10_FLT16),
   fmt1(VK_FORMAT_ASTC_12x12_UNORM_BLOCK,            ISL_FORMAT_ASTC_LDR_2D_12X12_FLT16),
   fmt_unsupported(VK_FORMAT_B8G8R8_UNORM),
   fmt_unsupported(VK_FORMAT_B8G8R8_SNORM),
   fmt_unsupported(VK_FORMAT_B8G8R8_USCALED),
   fmt_unsupported(VK_FORMAT_B8G8R8_SSCALED),
   fmt_unsupported(VK_FORMAT_B8G8R8_UINT),
   fmt_unsupported(VK_FORMAT_B8G8R8_SINT),
   fmt_unsupported(VK_FORMAT_B8G8R8_SRGB),
   fmt1(VK_FORMAT_B8G8R8A8_UNORM,                    ISL_FORMAT_B8G8R8A8_UNORM),
   fmt_unsupported(VK_FORMAT_B8G8R8A8_SNORM),
   fmt_unsupported(VK_FORMAT_B8G8R8A8_USCALED),
   fmt_unsupported(VK_FORMAT_B8G8R8A8_SSCALED),
   fmt_unsupported(VK_FORMAT_B8G8R8A8_UINT),
   fmt_unsupported(VK_FORMAT_B8G8R8A8_SINT),
   fmt1(VK_FORMAT_B8G8R8A8_SRGB,                     ISL_FORMAT_B8G8R8A8_UNORM_SRGB),
};

static const struct anv_format ycbcr_formats[] = {
   ycbcr_fmt(VK_FORMAT_G8B8G8R8_422_UNORM_KHR, 1,
             y_plane(ISL_FORMAT_YCRCB_SWAPUV, RGBA, _ISL_SWIZZLE(BLUE, GREEN, RED, ZERO), 1, 1)),
   ycbcr_fmt(VK_FORMAT_B8G8R8G8_422_UNORM_KHR, 1,
             y_plane(ISL_FORMAT_YCRCB_SWAPUVY, RGBA, _ISL_SWIZZLE(BLUE, GREEN, RED, ZERO), 1, 1)),
   ycbcr_fmt(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 2, 2),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 2, 2)),
   ycbcr_fmt(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR, 2,
             y_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8G8_UNORM, RGBA, _ISL_SWIZZLE(BLUE, RED, ZERO, ZERO), 2, 2)),
   ycbcr_fmt(VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 2, 1),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 2, 1)),
   ycbcr_fmt(VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR, 2,
             y_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8G8_UNORM, RGBA, _ISL_SWIZZLE(BLUE, RED, ZERO, ZERO), 2, 1)),
   ycbcr_fmt(VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R8_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 1, 1)),

   fmt_unsupported(VK_FORMAT_R10X6_UNORM_PACK16_KHR),
   fmt_unsupported(VK_FORMAT_R10X6G10X6_UNORM_2PACK16_KHR),
   fmt_unsupported(VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_R12X4_UNORM_PACK16_KHR),
   fmt_unsupported(VK_FORMAT_R12X4G12X4_UNORM_2PACK16_KHR),
   fmt_unsupported(VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR),
   fmt_unsupported(VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR),
   /* TODO: it is possible to enable the following 2 formats, but that
    * requires further refactoring of how we handle multiplanar formats.
    */
   fmt_unsupported(VK_FORMAT_G16B16G16R16_422_UNORM_KHR),
   fmt_unsupported(VK_FORMAT_B16G16R16G16_422_UNORM_KHR),

   ycbcr_fmt(VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 2, 2),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 2, 2)),
   ycbcr_fmt(VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR, 2,
             y_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16G16_UNORM, RGBA, _ISL_SWIZZLE(BLUE, RED, ZERO, ZERO), 2, 2)),
   ycbcr_fmt(VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 2, 1),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 2, 1)),
   ycbcr_fmt(VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR, 2,
             y_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16G16_UNORM, RGBA, _ISL_SWIZZLE(BLUE, RED, ZERO, ZERO), 2, 1)),
   ycbcr_fmt(VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR, 3,
             y_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(GREEN, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(BLUE, ZERO, ZERO, ZERO), 1, 1),
             chroma_plane(ISL_FORMAT_R16_UNORM, RGBA, _ISL_SWIZZLE(RED, ZERO, ZERO, ZERO), 1, 1)),
};

#undef _fmt
#undef swiz_fmt1
#undef fmt1
#undef fmt

static const struct {
   const struct anv_format *formats;
   uint32_t n_formats;
} anv_formats[] = {
   [0]                                       = { .formats = main_formats,
                                                 .n_formats = ARRAY_SIZE(main_formats), },
   [_VK_KHR_sampler_ycbcr_conversion_number] = { .formats = ycbcr_formats,
                                                 .n_formats = ARRAY_SIZE(ycbcr_formats), },
};

const struct anv_format *
anv_get_format(VkFormat vk_format)
{
   uint32_t enum_offset = VK_ENUM_OFFSET(vk_format);
   uint32_t ext_number = VK_ENUM_EXTENSION(vk_format);

   if (ext_number >= ARRAY_SIZE(anv_formats) ||
       enum_offset >= anv_formats[ext_number].n_formats)
      return NULL;

   const struct anv_format *format =
      &anv_formats[ext_number].formats[enum_offset];
   if (format->planes[0].isl_format == ISL_FORMAT_UNSUPPORTED)
      return NULL;

   return format;
}

/**
 * Exactly one bit must be set in \a aspect.
 */
struct anv_format_plane
anv_get_format_plane(const struct gen_device_info *devinfo, VkFormat vk_format,
                     VkImageAspectFlags aspect, VkImageTiling tiling)
{
   const struct anv_format *format = anv_get_format(vk_format);
   struct anv_format_plane plane_format = {
      .isl_format = ISL_FORMAT_UNSUPPORTED,
   };

   if (format == NULL)
      return plane_format;

   uint32_t plane = anv_image_aspect_to_plane(vk_format_aspects(vk_format), aspect);
   plane_format = format->planes[plane];
   if (plane_format.isl_format == ISL_FORMAT_UNSUPPORTED)
      return plane_format;

   if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      assert(vk_format_aspects(vk_format) &
             (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
      return plane_format;
   }

   assert((aspect & ~VK_IMAGE_ASPECT_ANY_COLOR_BIT) == 0);

   const struct isl_format_layout *isl_layout =
      isl_format_get_layout(plane_format.isl_format);

   if (tiling == VK_IMAGE_TILING_OPTIMAL &&
       !util_is_power_of_two(isl_layout->bpb)) {
      /* Tiled formats *must* be power-of-two because we need up upload
       * them with the render pipeline.  For 3-channel formats, we fix
       * this by switching them over to RGBX or RGBA formats under the
       * hood.
       */
      enum isl_format rgbx = isl_format_rgb_to_rgbx(plane_format.isl_format);
      if (rgbx != ISL_FORMAT_UNSUPPORTED &&
          isl_format_supports_rendering(devinfo, rgbx)) {
         plane_format.isl_format = rgbx;
      } else {
         plane_format.isl_format =
            isl_format_rgb_to_rgba(plane_format.isl_format);
         plane_format.swizzle = ISL_SWIZZLE(RED, GREEN, BLUE, ONE);
      }
   }

   /* The B4G4R4A4 format isn't available prior to Broadwell so we have to fall
    * back to a format with a more complex swizzle.
    */
   if (vk_format == VK_FORMAT_B4G4R4A4_UNORM_PACK16 && devinfo->gen < 8) {
      plane_format.isl_format = ISL_FORMAT_B4G4R4A4_UNORM;
      plane_format.swizzle = ISL_SWIZZLE(GREEN, RED, ALPHA, BLUE);
   }

   return plane_format;
}

// Format capabilities

static VkFormatFeatureFlags
get_image_format_properties(const struct gen_device_info *devinfo,
                            enum isl_format base, struct anv_format_plane format)
{
   if (format.isl_format == ISL_FORMAT_UNSUPPORTED)
      return 0;

   VkFormatFeatureFlags flags = 0;
   if (isl_format_supports_sampling(devinfo, format.isl_format)) {
      flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
               VK_FORMAT_FEATURE_BLIT_SRC_BIT;

      if (isl_format_supports_filtering(devinfo, format.isl_format))
         flags |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
   }

   /* We can render to swizzled formats.  However, if the alpha channel is
    * moved, then blending won't work correctly.  The PRM tells us
    * straight-up not to render to such a surface.
    */
   if (isl_format_supports_rendering(devinfo, format.isl_format) &&
       format.swizzle.a == ISL_CHANNEL_SELECT_ALPHA) {
      flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
               VK_FORMAT_FEATURE_BLIT_DST_BIT;

      if (isl_format_supports_alpha_blending(devinfo, format.isl_format))
         flags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
   }

   /* Load/store is determined based on base format.  This prevents RGB
    * formats from showing up as load/store capable.
    */
   if (isl_is_storage_image_format(base))
      flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

   if (base == ISL_FORMAT_R32_SINT || base == ISL_FORMAT_R32_UINT)
      flags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;

   if (flags) {
      flags |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR |
               VK_FORMAT_FEATURE_TRANSFER_DST_BIT_KHR;
   }

   return flags;
}

static VkFormatFeatureFlags
get_buffer_format_properties(const struct gen_device_info *devinfo,
                             enum isl_format format)
{
   if (format == ISL_FORMAT_UNSUPPORTED)
      return 0;

   VkFormatFeatureFlags flags = 0;
   if (isl_format_supports_sampling(devinfo, format) &&
       !isl_format_is_compressed(format))
      flags |= VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT;

   if (isl_format_supports_vertex_fetch(devinfo, format))
      flags |= VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;

   if (isl_is_storage_image_format(format))
      flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT;

   if (format == ISL_FORMAT_R32_SINT || format == ISL_FORMAT_R32_UINT)
      flags |= VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;

   return flags;
}

static void
anv_physical_device_get_format_properties(struct anv_physical_device *physical_device,
                                          VkFormat vk_format,
                                          VkFormatProperties *out_properties)
{
   int gen = physical_device->info.gen * 10;
   if (physical_device->info.is_haswell)
      gen += 5;

   const struct anv_format *format = anv_get_format(vk_format);
   VkFormatFeatureFlags linear = 0, tiled = 0, buffer = 0;
   if (format == NULL) {
      /* Nothing to do here */
   } else if (vk_format_is_depth_or_stencil(vk_format)) {
      tiled |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
      if (vk_format_aspects(vk_format) == VK_IMAGE_ASPECT_DEPTH_BIT ||
          physical_device->info.gen >= 8)
         tiled |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

      tiled |= VK_FORMAT_FEATURE_BLIT_SRC_BIT |
               VK_FORMAT_FEATURE_BLIT_DST_BIT |
               VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR |
               VK_FORMAT_FEATURE_TRANSFER_DST_BIT_KHR;
   } else {
      struct anv_format_plane linear_fmt, tiled_fmt;
      linear_fmt = anv_get_format_plane(&physical_device->info, vk_format,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_TILING_LINEAR);
      tiled_fmt = anv_get_format_plane(&physical_device->info, vk_format,
                                       VK_IMAGE_ASPECT_COLOR_BIT,
                                       VK_IMAGE_TILING_OPTIMAL);

      linear = get_image_format_properties(&physical_device->info,
                                           linear_fmt.isl_format, linear_fmt);
      tiled = get_image_format_properties(&physical_device->info,
                                          linear_fmt.isl_format, tiled_fmt);
      buffer = get_buffer_format_properties(&physical_device->info,
                                            linear_fmt.isl_format);

      /* XXX: We handle 3-channel formats by switching them out for RGBX or
       * RGBA formats behind-the-scenes.  This works fine for textures
       * because the upload process will fill in the extra channel.
       * We could also support it for render targets, but it will take
       * substantially more work and we have enough RGBX formats to handle
       * what most clients will want.
       */
      if (linear_fmt.isl_format != ISL_FORMAT_UNSUPPORTED &&
          !util_is_power_of_two(isl_format_layouts[linear_fmt.isl_format].bpb) &&
          isl_format_rgb_to_rgbx(linear_fmt.isl_format) == ISL_FORMAT_UNSUPPORTED) {
         tiled &= ~VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT &
                  ~VK_FORMAT_FEATURE_BLIT_DST_BIT;
      }

      /* ASTC textures must be in Y-tiled memory */
      if (isl_format_get_layout(linear_fmt.isl_format)->txc == ISL_TXC_ASTC)
         linear = 0;
   }

   if (format && format->can_ycbcr) {
      VkFormatFeatureFlags ycbcr_features = 0;

      /* The sampler doesn't have support for mid point when it handles YUV on
       * its own.
       */
      if (isl_format_is_yuv(format->planes[0].isl_format)) {
         /* TODO: We've disabled linear implicit reconstruction with the
          * sampler. The failures show a slightly out of range values on the
          * bottom left of the sampled image.
          */
         ycbcr_features |= VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT_KHR;
      } else {
         ycbcr_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT_KHR |
            VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT_KHR |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_SEPARATE_RECONSTRUCTION_FILTER_BIT_KHR;
      }

      /* We can support cosited chroma locations when handle planes with our
       * own shader snippets.
       */
      for (unsigned p = 0; p < format->n_planes; p++) {
         if (format->planes[p].denominator_scales[0] > 1 ||
             format->planes[p].denominator_scales[1] > 1) {
            ycbcr_features |= VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT_KHR;
            break;
         }
      }

      if (format->n_planes > 1)
         ycbcr_features |= VK_FORMAT_FEATURE_DISJOINT_BIT_KHR;

      linear |= ycbcr_features;
      tiled |= ycbcr_features;

      const VkFormatFeatureFlags disallowed_ycbcr_image_features =
         VK_FORMAT_FEATURE_BLIT_SRC_BIT |
         VK_FORMAT_FEATURE_BLIT_DST_BIT |
         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
         VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
         VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

      linear &= ~disallowed_ycbcr_image_features;
      tiled &= ~disallowed_ycbcr_image_features;
      buffer = 0;
   }

   out_properties->linearTilingFeatures = linear;
   out_properties->optimalTilingFeatures = tiled;
   out_properties->bufferFeatures = buffer;

   return;
}


void anv_GetPhysicalDeviceFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties*                         pFormatProperties)
{
   ANV_FROM_HANDLE(anv_physical_device, physical_device, physicalDevice);

   anv_physical_device_get_format_properties(
               physical_device,
               format,
               pFormatProperties);
}

void anv_GetPhysicalDeviceFormatProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkFormatProperties2KHR*                     pFormatProperties)
{
   anv_GetPhysicalDeviceFormatProperties(physicalDevice, format,
                                         &pFormatProperties->formatProperties);

   vk_foreach_struct(ext, pFormatProperties->pNext) {
      switch (ext->sType) {
      default:
         anv_debug_ignored_stype(ext->sType);
         break;
      }
   }
}

static VkResult
anv_get_image_format_properties(
   struct anv_physical_device *physical_device,
   const VkPhysicalDeviceImageFormatInfo2KHR *info,
   VkImageFormatProperties *pImageFormatProperties,
   VkSamplerYcbcrConversionImageFormatPropertiesKHR *pYcbcrImageFormatProperties)
{
   VkFormatProperties format_props;
   VkFormatFeatureFlags format_feature_flags;
   VkExtent3D maxExtent;
   uint32_t maxMipLevels;
   uint32_t maxArraySize;
   VkSampleCountFlags sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   const struct anv_format *format = anv_get_format(info->format);

   if (format == NULL)
      goto unsupported;

   anv_physical_device_get_format_properties(physical_device, info->format,
                                             &format_props);

   /* Extract the VkFormatFeatureFlags that are relevant for the queried
    * tiling.
    */
   if (info->tiling == VK_IMAGE_TILING_LINEAR) {
      format_feature_flags = format_props.linearTilingFeatures;
   } else if (info->tiling == VK_IMAGE_TILING_OPTIMAL) {
      format_feature_flags = format_props.optimalTilingFeatures;
   } else {
      unreachable("bad VkImageTiling");
   }

   switch (info->type) {
   default:
      unreachable("bad VkImageType");
   case VK_IMAGE_TYPE_1D:
      maxExtent.width = 16384;
      maxExtent.height = 1;
      maxExtent.depth = 1;
      maxMipLevels = 15; /* log2(maxWidth) + 1 */
      maxArraySize = 2048;
      sampleCounts = VK_SAMPLE_COUNT_1_BIT;
      break;
   case VK_IMAGE_TYPE_2D:
      /* FINISHME: Does this really differ for cube maps? The documentation
       * for RENDER_SURFACE_STATE suggests so.
       */
      maxExtent.width = 16384;
      maxExtent.height = 16384;
      maxExtent.depth = 1;
      maxMipLevels = 15; /* log2(maxWidth) + 1 */
      maxArraySize = 2048;
      break;
   case VK_IMAGE_TYPE_3D:
      maxExtent.width = 2048;
      maxExtent.height = 2048;
      maxExtent.depth = 2048;
      maxMipLevels = 12; /* log2(maxWidth) + 1 */
      maxArraySize = 1;
      break;
   }

   /* Our hardware doesn't support 1D compressed textures.
    *    From the SKL PRM, RENDER_SURFACE_STATE::SurfaceFormat:
    *    * This field cannot be a compressed (BC*, DXT*, FXT*, ETC*, EAC*) format
    *       if the Surface Type is SURFTYPE_1D.
    *    * This field cannot be ASTC format if the Surface Type is SURFTYPE_1D.
    */
   if (info->type == VK_IMAGE_TYPE_1D &&
       isl_format_is_compressed(format->planes[0].isl_format)) {
       goto unsupported;
   }

   if (info->tiling == VK_IMAGE_TILING_OPTIMAL &&
       info->type == VK_IMAGE_TYPE_2D &&
       (format_feature_flags & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) &&
       !(info->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) &&
       !(info->usage & VK_IMAGE_USAGE_STORAGE_BIT)) {
      sampleCounts = isl_device_get_sample_counts(&physical_device->isl_dev);
   }

   if (info->usage & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
      /* Accept transfers on anything we can sample from or renderer to. */
      if (!(format_feature_flags & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_STORAGE_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (!(format_feature_flags & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
         goto unsupported;
      }
   }

   if (info->usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
      /* Nothing to check. */
   }

   if (info->usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
      /* Ignore this flag because it was removed from the
       * provisional_I_20150910 header.
       */
   }

   *pImageFormatProperties = (VkImageFormatProperties) {
      .maxExtent = maxExtent,
      .maxMipLevels = maxMipLevels,
      .maxArrayLayers = maxArraySize,
      .sampleCounts = sampleCounts,

      /* FINISHME: Accurately calculate
       * VkImageFormatProperties::maxResourceSize.
       */
      .maxResourceSize = UINT32_MAX,
   };

   if (pYcbcrImageFormatProperties) {
      pYcbcrImageFormatProperties->combinedImageSamplerDescriptorCount =
         format->n_planes;
   }

   return VK_SUCCESS;

unsupported:
   *pImageFormatProperties = (VkImageFormatProperties) {
      .maxExtent = { 0, 0, 0 },
      .maxMipLevels = 0,
      .maxArrayLayers = 0,
      .sampleCounts = 0,
      .maxResourceSize = 0,
   };

   return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

VkResult anv_GetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    VkImageTiling                               tiling,
    VkImageUsageFlags                           usage,
    VkImageCreateFlags                          createFlags,
    VkImageFormatProperties*                    pImageFormatProperties)
{
   ANV_FROM_HANDLE(anv_physical_device, physical_device, physicalDevice);

   const VkPhysicalDeviceImageFormatInfo2KHR info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2_KHR,
      .pNext = NULL,
      .format = format,
      .type = type,
      .tiling = tiling,
      .usage = usage,
      .flags = createFlags,
   };

   return anv_get_image_format_properties(physical_device, &info,
                                          pImageFormatProperties, NULL);
}

static const VkExternalMemoryPropertiesKHR prime_fd_props = {
   /* If we can handle external, then we can both import and export it. */
   .externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT_KHR |
                             VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT_KHR,
   /* For the moment, let's not support mixing and matching */
   .exportFromImportedHandleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
   .compatibleHandleTypes =
      VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
};

VkResult anv_GetPhysicalDeviceImageFormatProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2KHR*  base_info,
    VkImageFormatProperties2KHR*                base_props)
{
   ANV_FROM_HANDLE(anv_physical_device, physical_device, physicalDevice);
   const VkPhysicalDeviceExternalImageFormatInfoKHR *external_info = NULL;
   VkExternalImageFormatPropertiesKHR *external_props = NULL;
   VkSamplerYcbcrConversionImageFormatPropertiesKHR *ycbcr_props = NULL;
   VkResult result;

   /* Extract input structs */
   vk_foreach_struct_const(s, base_info->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO_KHR:
         external_info = (const void *) s;
         break;
      default:
         anv_debug_ignored_stype(s->sType);
         break;
      }
   }

   /* Extract output structs */
   vk_foreach_struct(s, base_props->pNext) {
      switch (s->sType) {
      case VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES_KHR:
         external_props = (void *) s;
         break;
      case VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES_KHR:
         ycbcr_props = (void *) s;
         break;
      default:
         anv_debug_ignored_stype(s->sType);
         break;
      }
   }

   result = anv_get_image_format_properties(physical_device, base_info,
               &base_props->imageFormatProperties, ycbcr_props);
   if (result != VK_SUCCESS)
      goto fail;

   /* From the Vulkan 1.0.42 spec:
    *
    *    If handleType is 0, vkGetPhysicalDeviceImageFormatProperties2KHR will
    *    behave as if VkPhysicalDeviceExternalImageFormatInfoKHR was not
    *    present and VkExternalImageFormatPropertiesKHR will be ignored.
    */
   if (external_info && external_info->handleType != 0) {
      switch (external_info->handleType) {
      case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR:
         if (external_props)
            external_props->externalMemoryProperties = prime_fd_props;
         break;
      default:
         /* From the Vulkan 1.0.42 spec:
          *
          *    If handleType is not compatible with the [parameters] specified
          *    in VkPhysicalDeviceImageFormatInfo2KHR, then
          *    vkGetPhysicalDeviceImageFormatProperties2KHR returns
          *    VK_ERROR_FORMAT_NOT_SUPPORTED.
          */
         result = vk_errorf(physical_device->instance, physical_device,
                            VK_ERROR_FORMAT_NOT_SUPPORTED,
                            "unsupported VkExternalMemoryTypeFlagBitsKHR 0x%x",
                            external_info->handleType);
         goto fail;
      }
   }

   return VK_SUCCESS;

 fail:
   if (result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
      /* From the Vulkan 1.0.42 spec:
       *
       *    If the combination of parameters to
       *    vkGetPhysicalDeviceImageFormatProperties2KHR is not supported by
       *    the implementation for use in vkCreateImage, then all members of
       *    imageFormatProperties will be filled with zero.
       */
      base_props->imageFormatProperties = (VkImageFormatProperties) {};
   }

   return result;
}

void anv_GetPhysicalDeviceSparseImageFormatProperties(
    VkPhysicalDevice                            physicalDevice,
    VkFormat                                    format,
    VkImageType                                 type,
    uint32_t                                    samples,
    VkImageUsageFlags                           usage,
    VkImageTiling                               tiling,
    uint32_t*                                   pNumProperties,
    VkSparseImageFormatProperties*              pProperties)
{
   /* Sparse images are not yet supported. */
   *pNumProperties = 0;
}

void anv_GetPhysicalDeviceSparseImageFormatProperties2KHR(
    VkPhysicalDevice                            physicalDevice,
    const VkPhysicalDeviceSparseImageFormatInfo2KHR* pFormatInfo,
    uint32_t*                                   pPropertyCount,
    VkSparseImageFormatProperties2KHR*          pProperties)
{
   /* Sparse images are not yet supported. */
   *pPropertyCount = 0;
}

void anv_GetPhysicalDeviceExternalBufferPropertiesKHR(
    VkPhysicalDevice                             physicalDevice,
    const VkPhysicalDeviceExternalBufferInfoKHR* pExternalBufferInfo,
    VkExternalBufferPropertiesKHR*               pExternalBufferProperties)
{
   /* The Vulkan 1.0.42 spec says "handleType must be a valid
    * VkExternalMemoryHandleTypeFlagBitsKHR value" in
    * VkPhysicalDeviceExternalBufferInfoKHR. This differs from
    * VkPhysicalDeviceExternalImageFormatInfoKHR, which surprisingly permits
    * handleType == 0.
    */
   assert(pExternalBufferInfo->handleType != 0);

   /* All of the current flags are for sparse which we don't support yet.
    * Even when we do support it, doing sparse on external memory sounds
    * sketchy.  Also, just disallowing flags is the safe option.
    */
   if (pExternalBufferInfo->flags)
      goto unsupported;

   switch (pExternalBufferInfo->handleType) {
   case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR:
      pExternalBufferProperties->externalMemoryProperties = prime_fd_props;
      return;
   default:
      goto unsupported;
   }

 unsupported:
   pExternalBufferProperties->externalMemoryProperties =
      (VkExternalMemoryPropertiesKHR) {0};
}

VkResult anv_CreateSamplerYcbcrConversionKHR(
    VkDevice                                    _device,
    const VkSamplerYcbcrConversionCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSamplerYcbcrConversionKHR*                pYcbcrConversion)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   struct anv_ycbcr_conversion *conversion;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO_KHR);

   conversion = vk_alloc2(&device->alloc, pAllocator, sizeof(*conversion), 8,
                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!conversion)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   memset(conversion, 0, sizeof(*conversion));

   conversion->format = anv_get_format(pCreateInfo->format);
   conversion->ycbcr_model = pCreateInfo->ycbcrModel;
   conversion->ycbcr_range = pCreateInfo->ycbcrRange;
   conversion->mapping[0] = pCreateInfo->components.r;
   conversion->mapping[1] = pCreateInfo->components.g;
   conversion->mapping[2] = pCreateInfo->components.b;
   conversion->mapping[3] = pCreateInfo->components.a;
   conversion->chroma_offsets[0] = pCreateInfo->xChromaOffset;
   conversion->chroma_offsets[1] = pCreateInfo->yChromaOffset;
   conversion->chroma_filter = pCreateInfo->chromaFilter;

   bool has_chroma_subsampled = false;
   for (uint32_t p = 0; p < conversion->format->n_planes; p++) {
      if (conversion->format->planes[p].has_chroma &&
          (conversion->format->planes[p].denominator_scales[0] > 1 ||
           conversion->format->planes[p].denominator_scales[1] > 1))
         has_chroma_subsampled = true;
   }
   conversion->chroma_reconstruction = has_chroma_subsampled &&
      (conversion->chroma_offsets[0] == VK_CHROMA_LOCATION_COSITED_EVEN_KHR ||
       conversion->chroma_offsets[1] == VK_CHROMA_LOCATION_COSITED_EVEN_KHR);

   *pYcbcrConversion = anv_ycbcr_conversion_to_handle(conversion);

   return VK_SUCCESS;
}

void anv_DestroySamplerYcbcrConversionKHR(
    VkDevice                                    _device,
    VkSamplerYcbcrConversionKHR                 YcbcrConversion,
    const VkAllocationCallbacks*                pAllocator)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_ycbcr_conversion, conversion, YcbcrConversion);

   if (!conversion)
      return;

   vk_free2(&device->alloc, pAllocator, conversion);
}
