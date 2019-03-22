/*
 * Copyright © 2016 Intel Corporation
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

#ifndef VK_FORMAT_INFO_H
#define VK_FORMAT_INFO_H

#include <stdbool.h>
#include <vulkan/vulkan.h>

static inline VkImageAspectFlags
vk_format_aspects(VkFormat format)
{
   switch (format) {
   case VK_FORMAT_UNDEFINED:
      return 0;

   case VK_FORMAT_S8_UINT:
      return VK_IMAGE_ASPECT_STENCIL_BIT;

   case VK_FORMAT_D16_UNORM_S8_UINT:
   case VK_FORMAT_D24_UNORM_S8_UINT:
   case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

   case VK_FORMAT_D16_UNORM:
   case VK_FORMAT_X8_D24_UNORM_PACK32:
   case VK_FORMAT_D32_SFLOAT:
      return VK_IMAGE_ASPECT_DEPTH_BIT;

   case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR:
   case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM_KHR:
   case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM_KHR:
   case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16_KHR:
   case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16_KHR:
   case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16_KHR:
   case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16_KHR:
   case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16_KHR:
   case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16_KHR:
   case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM_KHR:
   case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM_KHR:
   case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM_KHR:
      return (VK_IMAGE_ASPECT_PLANE_0_BIT_KHR |
              VK_IMAGE_ASPECT_PLANE_1_BIT_KHR |
              VK_IMAGE_ASPECT_PLANE_2_BIT_KHR);

   case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM_KHR:
   case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM_KHR:
   case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16_KHR:
   case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16_KHR:
   case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16_KHR:
   case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16_KHR:
   case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM_KHR:
   case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM_KHR:
      return (VK_IMAGE_ASPECT_PLANE_0_BIT_KHR |
              VK_IMAGE_ASPECT_PLANE_1_BIT_KHR);

   default:
      return VK_IMAGE_ASPECT_COLOR_BIT;
   }
}

static inline bool
vk_format_is_color(VkFormat format)
{
   return vk_format_aspects(format) == VK_IMAGE_ASPECT_COLOR_BIT;
}

static inline bool
vk_format_is_depth_or_stencil(VkFormat format)
{
   const VkImageAspectFlags aspects = vk_format_aspects(format);
   return aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

static inline bool
vk_format_has_depth(VkFormat format)
{
   const VkImageAspectFlags aspects = vk_format_aspects(format);
   return aspects & VK_IMAGE_ASPECT_DEPTH_BIT;
}

#endif /* VK_FORMAT_INFO_H */
