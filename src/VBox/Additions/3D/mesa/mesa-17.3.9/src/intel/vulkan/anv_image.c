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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "anv_private.h"
#include "util/debug.h"
#include "vk_util.h"

#include "vk_format_info.h"

/**
 * Exactly one bit must be set in \a aspect.
 */
static isl_surf_usage_flags_t
choose_isl_surf_usage(VkImageCreateFlags vk_create_flags,
                      VkImageUsageFlags vk_usage,
                      isl_surf_usage_flags_t isl_extra_usage,
                      VkImageAspectFlags aspect)
{
   isl_surf_usage_flags_t isl_usage = isl_extra_usage;

   if (vk_usage & VK_IMAGE_USAGE_SAMPLED_BIT)
      isl_usage |= ISL_SURF_USAGE_TEXTURE_BIT;

   if (vk_usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
      isl_usage |= ISL_SURF_USAGE_TEXTURE_BIT;

   if (vk_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      isl_usage |= ISL_SURF_USAGE_RENDER_TARGET_BIT;

   if (vk_create_flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
      isl_usage |= ISL_SURF_USAGE_CUBE_BIT;

   /* Even if we're only using it for transfer operations, clears to depth and
    * stencil images happen as depth and stencil so they need the right ISL
    * usage bits or else things will fall apart.
    */
   switch (aspect) {
   case VK_IMAGE_ASPECT_DEPTH_BIT:
      isl_usage |= ISL_SURF_USAGE_DEPTH_BIT;
      break;
   case VK_IMAGE_ASPECT_STENCIL_BIT:
      isl_usage |= ISL_SURF_USAGE_STENCIL_BIT;
      break;
   case VK_IMAGE_ASPECT_COLOR_BIT:
   case VK_IMAGE_ASPECT_PLANE_0_BIT_KHR:
   case VK_IMAGE_ASPECT_PLANE_1_BIT_KHR:
   case VK_IMAGE_ASPECT_PLANE_2_BIT_KHR:
      break;
   default:
      unreachable("bad VkImageAspect");
   }

   if (vk_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
      /* blorp implements transfers by sampling from the source image. */
      isl_usage |= ISL_SURF_USAGE_TEXTURE_BIT;
   }

   if (vk_usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT &&
       aspect == VK_IMAGE_ASPECT_COLOR_BIT) {
      /* blorp implements transfers by rendering into the destination image.
       * Only request this with color images, as we deal with depth/stencil
       * formats differently. */
      isl_usage |= ISL_SURF_USAGE_RENDER_TARGET_BIT;
   }

   return isl_usage;
}

/**
 * Exactly one bit must be set in \a aspect.
 */
static struct anv_surface *
get_surface(struct anv_image *image, VkImageAspectFlags aspect)
{
   uint32_t plane = anv_image_aspect_to_plane(image->aspects, aspect);
   return &image->planes[plane].surface;
}

static void
add_surface(struct anv_image *image, struct anv_surface *surf, uint32_t plane)
{
   assert(surf->isl.size > 0); /* isl surface must be initialized */

   if (image->disjoint) {
      surf->offset = align_u32(image->planes[plane].size, surf->isl.alignment);
      /* Plane offset is always 0 when it's disjoint. */
   } else {
      surf->offset = align_u32(image->size, surf->isl.alignment);
      /* Determine plane's offset only once when the first surface is added. */
      if (image->planes[plane].size == 0)
         image->planes[plane].offset = image->size;
   }

   image->size = surf->offset + surf->isl.size;
   image->planes[plane].size = (surf->offset + surf->isl.size) - image->planes[plane].offset;

   image->alignment = MAX2(image->alignment, surf->isl.alignment);
   image->planes[plane].alignment = MAX2(image->planes[plane].alignment,
                                         surf->isl.alignment);
}


static bool
all_formats_ccs_e_compatible(const struct gen_device_info *devinfo,
                             const struct VkImageCreateInfo *vk_info)
{
   enum isl_format format =
      anv_get_isl_format(devinfo, vk_info->format,
                         VK_IMAGE_ASPECT_COLOR_BIT, vk_info->tiling);

   if (!isl_format_supports_ccs_e(devinfo, format))
      return false;

   if (!(vk_info->flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT))
      return true;

   const VkImageFormatListCreateInfoKHR *fmt_list =
      vk_find_struct_const(vk_info->pNext, IMAGE_FORMAT_LIST_CREATE_INFO_KHR);

   if (!fmt_list || fmt_list->viewFormatCount == 0)
      return false;

   for (uint32_t i = 0; i < fmt_list->viewFormatCount; i++) {
      enum isl_format view_format =
         anv_get_isl_format(devinfo, fmt_list->pViewFormats[i],
                            VK_IMAGE_ASPECT_COLOR_BIT, vk_info->tiling);

      if (!isl_formats_are_ccs_e_compatible(devinfo, format, view_format))
         return false;
   }

   return true;
}

/**
 * For color images that have an auxiliary surface, request allocation for an
 * additional buffer that mainly stores fast-clear values. Use of this buffer
 * allows us to access the image's subresources while being aware of their
 * fast-clear values in non-trivial cases (e.g., outside of a render pass in
 * which a fast clear has occurred).
 *
 * For the purpose of discoverability, the algorithm used to manage this buffer
 * is described here. A clear value in this buffer is updated when a fast clear
 * is performed on a subresource. One of two synchronization operations is
 * performed in order for a following memory access to use the fast-clear
 * value:
 *    a. Copy the value from the buffer to the surface state object used for
 *       reading. This is done implicitly when the value is the clear value
 *       predetermined to be the default in other surface state objects. This
 *       is currently only done explicitly for the operation below.
 *    b. Do (a) and use the surface state object to resolve the subresource.
 *       This is only done during layout transitions for decent performance.
 *
 * With the above scheme, we can fast-clear whenever the hardware allows except
 * for two cases in which synchronization becomes impossible or undesirable:
 *    * The subresource is in the GENERAL layout and is cleared to a value
 *      other than the special default value.
 *
 *      Performing a synchronization operation in order to read from the
 *      subresource is undesirable in this case. Firstly, b) is not an option
 *      because a layout transition isn't required between a write and read of
 *      an image in the GENERAL layout. Secondly, it's undesirable to do a)
 *      explicitly because it would require large infrastructural changes. The
 *      Vulkan API supports us in deciding not to optimize this layout by
 *      stating that using this layout may cause suboptimal performance. NOTE:
 *      the auxiliary buffer must always be enabled to support a) implicitly.
 *
 *
 *    * For the given miplevel, only some of the layers are cleared at once.
 *
 *      If the user clears each layer to a different value, then tries to
 *      render to multiple layers at once, we have no ability to perform a
 *      synchronization operation in between. a) is not helpful because the
 *      object can only hold one clear value. b) is not an option because a
 *      layout transition isn't required in this case.
 */
static void
add_fast_clear_state_buffer(struct anv_image *image,
                            VkImageAspectFlagBits aspect,
                            uint32_t plane,
                            const struct anv_device *device)
{
   assert(image && device);
   assert(image->planes[plane].aux_surface.isl.size > 0 &&
          image->aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT);

   /* The offset to the buffer of clear values must be dword-aligned for GPU
    * memcpy operations. It is located immediately after the auxiliary surface.
    */

   /* Tiled images are guaranteed to be 4K aligned, so the image alignment
    * should also be dword-aligned.
    */
   assert(image->alignment % 4 == 0);

   /* Auxiliary buffers should be a multiple of 4K, so the start of the clear
    * values buffer should already be dword-aligned.
    */
   assert((image->planes[plane].offset + image->planes[plane].size) % 4 == 0);

   /* This buffer should be at the very end of the plane. */
   if (image->disjoint) {
      assert(image->planes[plane].size ==
             (image->planes[plane].offset + image->planes[plane].size));
   } else {
      assert(image->size ==
             (image->planes[plane].offset + image->planes[plane].size));
   }

   const unsigned entry_size = anv_fast_clear_state_entry_size(device);
   /* There's no padding between entries, so ensure that they're always a
    * multiple of 32 bits in order to enable GPU memcpy operations.
    */
   assert(entry_size % 4 == 0);

   const unsigned plane_state_size =
      entry_size * anv_image_aux_levels(image, aspect);

   image->planes[plane].fast_clear_state_offset =
      image->planes[plane].offset + image->planes[plane].size;

   image->planes[plane].size += plane_state_size;
   image->size += plane_state_size;
}

/**
 * Initialize the anv_image::*_surface selected by \a aspect. Then update the
 * image's memory requirements (that is, the image's size and alignment).
 *
 * Exactly one bit must be set in \a aspect.
 */
static VkResult
make_surface(const struct anv_device *dev,
             struct anv_image *image,
             const struct anv_image_create_info *anv_info,
             VkImageAspectFlags aspect)
{
   const VkImageCreateInfo *vk_info = anv_info->vk_info;
   bool ok UNUSED;

   static const enum isl_surf_dim vk_to_isl_surf_dim[] = {
      [VK_IMAGE_TYPE_1D] = ISL_SURF_DIM_1D,
      [VK_IMAGE_TYPE_2D] = ISL_SURF_DIM_2D,
      [VK_IMAGE_TYPE_3D] = ISL_SURF_DIM_3D,
   };

   /* Translate the Vulkan tiling to an equivalent ISL tiling, then filter the
    * result with an optionally provided ISL tiling argument.
    */
   isl_tiling_flags_t tiling_flags =
      (vk_info->tiling == VK_IMAGE_TILING_LINEAR) ?
      ISL_TILING_LINEAR_BIT : ISL_TILING_ANY_MASK;

   if (anv_info->isl_tiling_flags)
      tiling_flags &= anv_info->isl_tiling_flags;

   assert(tiling_flags);

   image->extent = anv_sanitize_image_extent(vk_info->imageType,
                                             vk_info->extent);

   const unsigned plane = anv_image_aspect_to_plane(image->aspects, aspect);
   const  struct anv_format_plane plane_format =
      anv_get_format_plane(&dev->info, image->vk_format, aspect, image->tiling);
   struct anv_surface *anv_surf = &image->planes[plane].surface;

   const isl_surf_usage_flags_t usage =
      choose_isl_surf_usage(vk_info->flags, image->usage,
                            anv_info->isl_extra_usage_flags, aspect);

   /* If an image is created as BLOCK_TEXEL_VIEW_COMPATIBLE, then we need to
    * fall back to linear on Broadwell and earlier because we aren't
    * guaranteed that we can handle offsets correctly.  On Sky Lake, the
    * horizontal and vertical alignments are sufficiently high that we can
    * just use RENDER_SURFACE_STATE::X/Y Offset.
    */
   bool needs_shadow = false;
   if (dev->info.gen <= 8 &&
       (vk_info->flags & VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT_KHR) &&
       vk_info->tiling == VK_IMAGE_TILING_OPTIMAL) {
      assert(isl_format_is_compressed(plane_format.isl_format));
      tiling_flags = ISL_TILING_LINEAR_BIT;
      needs_shadow = true;
   }

   ok = isl_surf_init(&dev->isl_dev, &anv_surf->isl,
      .dim = vk_to_isl_surf_dim[vk_info->imageType],
      .format = plane_format.isl_format,
      .width = image->extent.width / plane_format.denominator_scales[0],
      .height = image->extent.height / plane_format.denominator_scales[1],
      .depth = image->extent.depth,
      .levels = vk_info->mipLevels,
      .array_len = vk_info->arrayLayers,
      .samples = vk_info->samples,
      .min_alignment = 0,
      .row_pitch = anv_info->stride,
      .usage = usage,
      .tiling_flags = tiling_flags);

   /* isl_surf_init() will fail only if provided invalid input. Invalid input
    * is illegal in Vulkan.
    */
   assert(ok);

   image->planes[plane].aux_usage = ISL_AUX_USAGE_NONE;

   add_surface(image, anv_surf, plane);

   /* If an image is created as BLOCK_TEXEL_VIEW_COMPATIBLE, then we need to
    * create an identical tiled shadow surface for use while texturing so we
    * don't get garbage performance.
    */
   if (needs_shadow) {
      assert(aspect == VK_IMAGE_ASPECT_COLOR_BIT);
      assert(tiling_flags == ISL_TILING_LINEAR_BIT);

      ok = isl_surf_init(&dev->isl_dev, &image->planes[plane].shadow_surface.isl,
         .dim = vk_to_isl_surf_dim[vk_info->imageType],
         .format = plane_format.isl_format,
         .width = image->extent.width,
         .height = image->extent.height,
         .depth = image->extent.depth,
         .levels = vk_info->mipLevels,
         .array_len = vk_info->arrayLayers,
         .samples = vk_info->samples,
         .min_alignment = 0,
         .row_pitch = anv_info->stride,
         .usage = usage,
         .tiling_flags = ISL_TILING_ANY_MASK);

      /* isl_surf_init() will fail only if provided invalid input. Invalid input
       * is illegal in Vulkan.
       */
      assert(ok);

      add_surface(image, &image->planes[plane].shadow_surface, plane);
   }

   /* Add a HiZ surface to a depth buffer that will be used for rendering.
    */
   if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
      /* We don't advertise that depth buffers could be used as storage
       * images.
       */
       assert(!(image->usage & VK_IMAGE_USAGE_STORAGE_BIT));

      /* Allow the user to control HiZ enabling. Disable by default on gen7
       * because resolves are not currently implemented pre-BDW.
       */
      if (!(image->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
         /* It will never be used as an attachment, HiZ is pointless. */
      } else if (dev->info.gen == 7) {
         anv_perf_warn(dev->instance, image, "Implement gen7 HiZ");
      } else if (vk_info->mipLevels > 1) {
         anv_perf_warn(dev->instance, image, "Enable multi-LOD HiZ");
      } else if (vk_info->arrayLayers > 1) {
         anv_perf_warn(dev->instance, image,
                       "Implement multi-arrayLayer HiZ clears and resolves");
      } else if (dev->info.gen == 8 && vk_info->samples > 1) {
         anv_perf_warn(dev->instance, image, "Enable gen8 multisampled HiZ");
      } else if (!unlikely(INTEL_DEBUG & DEBUG_NO_HIZ)) {
         assert(image->planes[plane].aux_surface.isl.size == 0);
         ok = isl_surf_get_hiz_surf(&dev->isl_dev,
                                    &image->planes[plane].surface.isl,
                                    &image->planes[plane].aux_surface.isl);
         assert(ok);
         add_surface(image, &image->planes[plane].aux_surface, plane);
         image->planes[plane].aux_usage = ISL_AUX_USAGE_HIZ;
      }
   } else if ((aspect & VK_IMAGE_ASPECT_ANY_COLOR_BIT) && vk_info->samples == 1) {
      /* TODO: Disallow compression with :
       *
       *     1) non multiplanar images (We appear to hit a sampler bug with
       *        CCS & R16G16 format. Putting the clear state a page/4096bytes
       *        further fixes the issue).
       *
       *     2) alias images, because they might be aliases of images
       *        described in 1)
       *
       *     3) compression disabled by debug
       */
      const bool allow_compression =
         image->n_planes == 1 &&
         (vk_info->flags & VK_IMAGE_CREATE_ALIAS_BIT_KHR) == 0 &&
         likely((INTEL_DEBUG & DEBUG_NO_RBC) == 0);

      if (allow_compression) {
         assert(image->planes[plane].aux_surface.isl.size == 0);
         ok = isl_surf_get_ccs_surf(&dev->isl_dev,
                                    &image->planes[plane].surface.isl,
                                    &image->planes[plane].aux_surface.isl, 0);
         if (ok) {

            /* Disable CCS when it is not useful (i.e., when you can't render
             * to the image with CCS enabled).
             */
            if (!isl_format_supports_rendering(&dev->info,
                                               plane_format.isl_format)) {
               /* While it may be technically possible to enable CCS for this
                * image, we currently don't have things hooked up to get it
                * working.
                */
               anv_perf_warn(dev->instance, image,
                             "This image format doesn't support rendering. "
                             "Not allocating an CCS buffer.");
               image->planes[plane].aux_surface.isl.size = 0;
               return VK_SUCCESS;
            }

            add_surface(image, &image->planes[plane].aux_surface, plane);
            add_fast_clear_state_buffer(image, aspect, plane, dev);

            /* For images created without MUTABLE_FORMAT_BIT set, we know that
             * they will always be used with the original format.  In
             * particular, they will always be used with a format that
             * supports color compression.  If it's never used as a storage
             * image, then it will only be used through the sampler or the as
             * a render target.  This means that it's safe to just leave
             * compression on at all times for these formats.
             */
            if (!(vk_info->usage & VK_IMAGE_USAGE_STORAGE_BIT) &&
                all_formats_ccs_e_compatible(&dev->info, vk_info)) {
               image->planes[plane].aux_usage = ISL_AUX_USAGE_CCS_E;
            }
         }
      }
   } else if ((aspect & VK_IMAGE_ASPECT_ANY_COLOR_BIT) && vk_info->samples > 1) {
      assert(!(vk_info->usage & VK_IMAGE_USAGE_STORAGE_BIT));
      assert(image->planes[plane].aux_surface.isl.size == 0);
      ok = isl_surf_get_mcs_surf(&dev->isl_dev,
                                 &image->planes[plane].surface.isl,
                                 &image->planes[plane].aux_surface.isl);
      if (ok) {
         add_surface(image, &image->planes[plane].aux_surface, plane);
         add_fast_clear_state_buffer(image, aspect, plane, dev);
         image->planes[plane].aux_usage = ISL_AUX_USAGE_MCS;
      }
   }

   assert((image->planes[plane].offset + image->planes[plane].size) == image->size);

   /* Upper bound of the last surface should be smaller than the plane's
    * size.
    */
   assert((MAX2(image->planes[plane].surface.offset,
                image->planes[plane].aux_surface.offset) +
           (image->planes[plane].aux_surface.isl.size > 0 ?
            image->planes[plane].aux_surface.isl.size :
            image->planes[plane].surface.isl.size)) <=
          (image->planes[plane].offset + image->planes[plane].size));

   if (image->planes[plane].aux_surface.isl.size) {
      /* assert(image->planes[plane].fast_clear_state_offset == */
      /*        (image->planes[plane].aux_surface.offset + image->planes[plane].aux_surface.isl.size)); */
      assert(image->planes[plane].fast_clear_state_offset <
             (image->planes[plane].offset + image->planes[plane].size));
   }

   return VK_SUCCESS;
}

VkResult
anv_image_create(VkDevice _device,
                 const struct anv_image_create_info *create_info,
                 const VkAllocationCallbacks* alloc,
                 VkImage *pImage)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   const VkImageCreateInfo *pCreateInfo = create_info->vk_info;
   struct anv_image *image = NULL;
   VkResult r;

   assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);

   anv_assert(pCreateInfo->mipLevels > 0);
   anv_assert(pCreateInfo->arrayLayers > 0);
   anv_assert(pCreateInfo->samples > 0);
   anv_assert(pCreateInfo->extent.width > 0);
   anv_assert(pCreateInfo->extent.height > 0);
   anv_assert(pCreateInfo->extent.depth > 0);

   image = vk_zalloc2(&device->alloc, alloc, sizeof(*image), 8,
                       VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!image)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   image->type = pCreateInfo->imageType;
   image->extent = pCreateInfo->extent;
   image->vk_format = pCreateInfo->format;
   image->format = anv_get_format(pCreateInfo->format);
   image->aspects = vk_format_aspects(image->vk_format);
   image->levels = pCreateInfo->mipLevels;
   image->array_size = pCreateInfo->arrayLayers;
   image->samples = pCreateInfo->samples;
   image->usage = pCreateInfo->usage;
   image->tiling = pCreateInfo->tiling;
   image->disjoint = pCreateInfo->flags & VK_IMAGE_CREATE_DISJOINT_BIT_KHR;

   const struct anv_format *format = anv_get_format(image->vk_format);
   assert(format != NULL);

   image->n_planes = format->n_planes;

   uint32_t b;
   for_each_bit(b, image->aspects) {
      r = make_surface(device, image, create_info, (1 << b));
      if (r != VK_SUCCESS)
         goto fail;
   }

   *pImage = anv_image_to_handle(image);

   return VK_SUCCESS;

fail:
   if (image)
      vk_free2(&device->alloc, alloc, image);

   return r;
}

VkResult
anv_CreateImage(VkDevice device,
                const VkImageCreateInfo *pCreateInfo,
                const VkAllocationCallbacks *pAllocator,
                VkImage *pImage)
{
#ifdef ANDROID
   const VkNativeBufferANDROID *gralloc_info =
      vk_find_struct_const(pCreateInfo->pNext, NATIVE_BUFFER_ANDROID);

   if (gralloc_info)
      return anv_image_from_gralloc(device, pCreateInfo, gralloc_info,
                                    pAllocator, pImage);
#endif

   return anv_image_create(device,
      &(struct anv_image_create_info) {
         .vk_info = pCreateInfo,
      },
      pAllocator,
      pImage);
}

void
anv_DestroyImage(VkDevice _device, VkImage _image,
                 const VkAllocationCallbacks *pAllocator)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_image, image, _image);

   if (!image)
      return;

   for (uint32_t p = 0; p < image->n_planes; ++p) {
      if (image->planes[p].bo_is_owned) {
         assert(image->planes[p].bo != NULL);
         anv_bo_cache_release(device, &device->bo_cache, image->planes[p].bo);
      }
   }

   vk_free2(&device->alloc, pAllocator, image);
}

static void anv_image_bind_memory_plane(struct anv_device *device,
                                        struct anv_image *image,
                                        uint32_t plane,
                                        struct anv_device_memory *memory,
                                        uint32_t memory_offset)
{
   assert(!image->planes[plane].bo_is_owned);

   if (!memory) {
      image->planes[plane].bo = NULL;
      image->planes[plane].bo_offset = 0;
      return;
   }

   image->planes[plane].bo = memory->bo;
   image->planes[plane].bo_offset = memory_offset;
}

VkResult anv_BindImageMemory(
    VkDevice                                    _device,
    VkImage                                     _image,
    VkDeviceMemory                              _memory,
    VkDeviceSize                                memoryOffset)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_device_memory, mem, _memory);
   ANV_FROM_HANDLE(anv_image, image, _image);

   uint32_t aspect_bit;
   anv_foreach_image_aspect_bit(aspect_bit, image, image->aspects) {
      uint32_t plane =
         anv_image_aspect_to_plane(image->aspects, 1UL << aspect_bit);
      anv_image_bind_memory_plane(device, image, plane, mem, memoryOffset);
   }

   return VK_SUCCESS;
}

VkResult anv_BindImageMemory2KHR(
    VkDevice                                    _device,
    uint32_t                                    bindInfoCount,
    const VkBindImageMemoryInfoKHR*             pBindInfos)
{
   ANV_FROM_HANDLE(anv_device, device, _device);

   for (uint32_t i = 0; i < bindInfoCount; i++) {
      const VkBindImageMemoryInfoKHR *bind_info = &pBindInfos[i];
      ANV_FROM_HANDLE(anv_device_memory, mem, bind_info->memory);
      ANV_FROM_HANDLE(anv_image, image, bind_info->image);
      VkImageAspectFlags aspects = image->aspects;

      vk_foreach_struct_const(s, bind_info->pNext) {
         switch (s->sType) {
         case VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO_KHR: {
            const VkBindImagePlaneMemoryInfoKHR *plane_info =
               (const VkBindImagePlaneMemoryInfoKHR *) s;

            aspects = plane_info->planeAspect;
            break;
         }
         default:
            anv_debug_ignored_stype(s->sType);
            break;
         }
      }

      uint32_t aspect_bit;
      anv_foreach_image_aspect_bit(aspect_bit, image, aspects) {
         uint32_t plane =
            anv_image_aspect_to_plane(image->aspects, 1UL << aspect_bit);
         anv_image_bind_memory_plane(device, image, plane,
                                     mem, bind_info->memoryOffset);
      }
   }

   return VK_SUCCESS;
}

static void
anv_surface_get_subresource_layout(struct anv_image *image,
                                   struct anv_surface *surface,
                                   const VkImageSubresource *subresource,
                                   VkSubresourceLayout *layout)
{
   /* If we are on a non-zero mip level or array slice, we need to
    * calculate a real offset.
    */
   anv_assert(subresource->mipLevel == 0);
   anv_assert(subresource->arrayLayer == 0);

   layout->offset = surface->offset;
   layout->rowPitch = surface->isl.row_pitch;
   layout->depthPitch = isl_surf_get_array_pitch(&surface->isl);
   layout->arrayPitch = isl_surf_get_array_pitch(&surface->isl);
   layout->size = surface->isl.size;
}

void anv_GetImageSubresourceLayout(
    VkDevice                                    device,
    VkImage                                     _image,
    const VkImageSubresource*                   pSubresource,
    VkSubresourceLayout*                        pLayout)
{
   ANV_FROM_HANDLE(anv_image, image, _image);

   assert(__builtin_popcount(pSubresource->aspectMask) == 1);

   anv_surface_get_subresource_layout(image,
                                      get_surface(image,
                                                  pSubresource->aspectMask),
                                      pSubresource, pLayout);
}

/**
 * This function determines the optimal buffer to use for a given
 * VkImageLayout and other pieces of information needed to make that
 * determination. This does not determine the optimal buffer to use
 * during a resolve operation.
 *
 * @param devinfo The device information of the Intel GPU.
 * @param image The image that may contain a collection of buffers.
 * @param plane The plane of the image to be accessed.
 * @param layout The current layout of the image aspect(s).
 *
 * @return The primary buffer that should be used for the given layout.
 */
enum isl_aux_usage
anv_layout_to_aux_usage(const struct gen_device_info * const devinfo,
                        const struct anv_image * const image,
                        const VkImageAspectFlagBits aspect,
                        const VkImageLayout layout)
{
   /* Validate the inputs. */

   /* The devinfo is needed as the optimal buffer varies across generations. */
   assert(devinfo != NULL);

   /* The layout of a NULL image is not properly defined. */
   assert(image != NULL);

   /* The aspect must be exactly one of the image aspects. */
   assert(_mesa_bitcount(aspect) == 1 && (aspect & image->aspects));

   /* Determine the optimal buffer. */

   uint32_t plane = anv_image_aspect_to_plane(image->aspects, aspect);

   /* If there is no auxiliary surface allocated, we must use the one and only
    * main buffer.
    */
   if (image->planes[plane].aux_surface.isl.size == 0)
      return ISL_AUX_USAGE_NONE;

   /* All images that use an auxiliary surface are required to be tiled. */
   assert(image->tiling == VK_IMAGE_TILING_OPTIMAL);

   /* Stencil has no aux */
   assert(aspect != VK_IMAGE_ASPECT_STENCIL_BIT);

   /* The following switch currently only handles depth stencil aspects.
    * TODO: Handle the color aspect.
    */
   if (image->aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT)
      return image->planes[plane].aux_usage;

   switch (layout) {

   /* Invalid Layouts */
   case VK_IMAGE_LAYOUT_RANGE_SIZE:
   case VK_IMAGE_LAYOUT_MAX_ENUM:
      unreachable("Invalid image layout.");

   /* Undefined layouts
    *
    * The pre-initialized layout is equivalent to the undefined layout for
    * optimally-tiled images.  We can only do color compression (CCS or HiZ)
    * on tiled images.
    */
   case VK_IMAGE_LAYOUT_UNDEFINED:
   case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return ISL_AUX_USAGE_NONE;


   /* Transfer Layouts
    *
    * This buffer could be a depth buffer used in a transfer operation. BLORP
    * currently doesn't use HiZ for transfer operations so we must use the main
    * buffer for this layout. TODO: Enable HiZ in BLORP.
    */
   case VK_IMAGE_LAYOUT_GENERAL:
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return ISL_AUX_USAGE_NONE;


   /* Sampling Layouts */
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      assert((image->aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT) == 0);
      /* Fall-through */
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR:
      assert(aspect == VK_IMAGE_ASPECT_DEPTH_BIT);
      if (anv_can_sample_with_hiz(devinfo, image))
         return ISL_AUX_USAGE_HIZ;
      else
         return ISL_AUX_USAGE_NONE;

   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      assert(image->aspects == VK_IMAGE_ASPECT_COLOR_BIT);

      /* On SKL+, the render buffer can be decompressed by the presentation
       * engine. Support for this feature has not yet landed in the wider
       * ecosystem. TODO: Update this code when support lands.
       *
       * From the BDW PRM, Vol 7, Render Target Resolve:
       *
       *    If the MCS is enabled on a non-multisampled render target, the
       *    render target must be resolved before being used for other
       *    purposes (display, texture, CPU lock) The clear value from
       *    SURFACE_STATE is written into pixels in the render target
       *    indicated as clear in the MCS.
       *
       * Pre-SKL, the render buffer must be resolved before being used for
       * presentation. We can infer that the auxiliary buffer is not used.
       */
      return ISL_AUX_USAGE_NONE;


   /* Rendering Layouts */
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      assert(image->aspects == VK_IMAGE_ASPECT_COLOR_BIT);
      unreachable("Color images are not yet supported.");

   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
   case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR:
      assert(aspect == VK_IMAGE_ASPECT_DEPTH_BIT);
      return ISL_AUX_USAGE_HIZ;

   case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
      unreachable("VK_KHR_shared_presentable_image is unsupported");
   }

   /* If the layout isn't recognized in the exhaustive switch above, the
    * VkImageLayout value is not defined in vulkan.h.
    */
   unreachable("layout is not a VkImageLayout enumeration member.");
}


static struct anv_state
alloc_surface_state(struct anv_device *device)
{
   return anv_state_pool_alloc(&device->surface_state_pool, 64, 64);
}

static enum isl_channel_select
remap_swizzle(VkComponentSwizzle swizzle, VkComponentSwizzle component,
              struct isl_swizzle format_swizzle)
{
   if (swizzle == VK_COMPONENT_SWIZZLE_IDENTITY)
      swizzle = component;

   switch (swizzle) {
   case VK_COMPONENT_SWIZZLE_ZERO:  return ISL_CHANNEL_SELECT_ZERO;
   case VK_COMPONENT_SWIZZLE_ONE:   return ISL_CHANNEL_SELECT_ONE;
   case VK_COMPONENT_SWIZZLE_R:     return format_swizzle.r;
   case VK_COMPONENT_SWIZZLE_G:     return format_swizzle.g;
   case VK_COMPONENT_SWIZZLE_B:     return format_swizzle.b;
   case VK_COMPONENT_SWIZZLE_A:     return format_swizzle.a;
   default:
      unreachable("Invalid swizzle");
   }
}

void
anv_image_fill_surface_state(struct anv_device *device,
                             const struct anv_image *image,
                             VkImageAspectFlagBits aspect,
                             const struct isl_view *view_in,
                             isl_surf_usage_flags_t view_usage,
                             enum isl_aux_usage aux_usage,
                             const union isl_color_value *clear_color,
                             enum anv_image_view_state_flags flags,
                             struct anv_surface_state *state_inout,
                             struct brw_image_param *image_param_out)
{
   uint32_t plane = anv_image_aspect_to_plane(image->aspects, aspect);

   const struct anv_surface *surface = &image->planes[plane].surface,
      *aux_surface = &image->planes[plane].aux_surface;

   struct isl_view view = *view_in;
   view.usage |= view_usage;

   /* For texturing with VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL from a
    * compressed surface with a shadow surface, we use the shadow instead of
    * the primary surface.  The shadow surface will be tiled, unlike the main
    * surface, so it should get significantly better performance.
    */
   if (image->planes[plane].shadow_surface.isl.size > 0 &&
       isl_format_is_compressed(view.format) &&
       (flags & ANV_IMAGE_VIEW_STATE_TEXTURE_OPTIMAL)) {
      assert(isl_format_is_compressed(surface->isl.format));
      assert(surface->isl.tiling == ISL_TILING_LINEAR);
      assert(image->planes[plane].shadow_surface.isl.tiling != ISL_TILING_LINEAR);
      surface = &image->planes[plane].shadow_surface;
   }

   if (view_usage == ISL_SURF_USAGE_RENDER_TARGET_BIT)
      view.swizzle = anv_swizzle_for_render(view.swizzle);

   /* If this is a HiZ buffer we can sample from with a programmable clear
    * value (SKL+), define the clear value to the optimal constant.
    */
   union isl_color_value default_clear_color = { .u32 = { 0, } };
   if (device->info.gen >= 9 && aux_usage == ISL_AUX_USAGE_HIZ)
      default_clear_color.f32[0] = ANV_HZ_FC_VAL;
   if (!clear_color)
      clear_color = &default_clear_color;

   const uint64_t address = image->planes[plane].bo_offset + surface->offset;
   const uint64_t aux_address = aux_usage == ISL_AUX_USAGE_NONE ?
      0 : (image->planes[plane].bo_offset + aux_surface->offset);

   if (view_usage == ISL_SURF_USAGE_STORAGE_BIT &&
       !(flags & ANV_IMAGE_VIEW_STATE_STORAGE_WRITE_ONLY) &&
       !isl_has_matching_typed_storage_image_format(&device->info,
                                                    view.format)) {
      /* In this case, we are a writeable storage buffer which needs to be
       * lowered to linear. All tiling and offset calculations will be done in
       * the shader.
       */
      assert(aux_usage == ISL_AUX_USAGE_NONE);
      isl_buffer_fill_state(&device->isl_dev, state_inout->state.map,
                            .address = address,
                            .size = surface->isl.size,
                            .format = ISL_FORMAT_RAW,
                            .stride = 1,
                            .mocs = device->default_mocs);
      state_inout->address = address,
      state_inout->aux_address = 0;
   } else {
      if (view_usage == ISL_SURF_USAGE_STORAGE_BIT &&
          !(flags & ANV_IMAGE_VIEW_STATE_STORAGE_WRITE_ONLY)) {
         /* Typed surface reads support a very limited subset of the shader
          * image formats.  Translate it into the closest format the hardware
          * supports.
          */
         assert(aux_usage == ISL_AUX_USAGE_NONE);
         view.format = isl_lower_storage_image_format(&device->info,
                                                      view.format);
      }

      const struct isl_surf *isl_surf = &surface->isl;

      struct isl_surf tmp_surf;
      uint32_t offset_B = 0, tile_x_sa = 0, tile_y_sa = 0;
      if (isl_format_is_compressed(surface->isl.format) &&
          !isl_format_is_compressed(view.format)) {
         /* We're creating an uncompressed view of a compressed surface.  This
          * is allowed but only for a single level/layer.
          */
         assert(surface->isl.samples == 1);
         assert(view.levels == 1);
         assert(view.array_len == 1);

         isl_surf_get_image_surf(&device->isl_dev, isl_surf,
                                 view.base_level,
                                 surface->isl.dim == ISL_SURF_DIM_3D ?
                                    0 : view.base_array_layer,
                                 surface->isl.dim == ISL_SURF_DIM_3D ?
                                    view.base_array_layer : 0,
                                 &tmp_surf,
                                 &offset_B, &tile_x_sa, &tile_y_sa);

         /* The newly created image represents the one subimage we're
          * referencing with this view so it only has one array slice and
          * miplevel.
          */
         view.base_array_layer = 0;
         view.base_level = 0;

         /* We're making an uncompressed view here.  The image dimensions need
          * to be scaled down by the block size.
          */
         const struct isl_format_layout *fmtl =
            isl_format_get_layout(surface->isl.format);
         tmp_surf.format = view.format;
         tmp_surf.logical_level0_px.width =
            DIV_ROUND_UP(tmp_surf.logical_level0_px.width, fmtl->bw);
         tmp_surf.logical_level0_px.height =
            DIV_ROUND_UP(tmp_surf.logical_level0_px.height, fmtl->bh);
         tmp_surf.phys_level0_sa.width /= fmtl->bw;
         tmp_surf.phys_level0_sa.height /= fmtl->bh;
         tile_x_sa /= fmtl->bw;
         tile_y_sa /= fmtl->bh;

         isl_surf = &tmp_surf;

         if (device->info.gen <= 8) {
            assert(surface->isl.tiling == ISL_TILING_LINEAR);
            assert(tile_x_sa == 0);
            assert(tile_y_sa == 0);
         }
      }

      isl_surf_fill_state(&device->isl_dev, state_inout->state.map,
                          .surf = isl_surf,
                          .view = &view,
                          .address = address + offset_B,
                          .clear_color = *clear_color,
                          .aux_surf = &aux_surface->isl,
                          .aux_usage = aux_usage,
                          .aux_address = aux_address,
                          .mocs = device->default_mocs,
                          .x_offset_sa = tile_x_sa,
                          .y_offset_sa = tile_y_sa);
      state_inout->address = address + offset_B;
      if (device->info.gen >= 8) {
         state_inout->aux_address = aux_address;
      } else {
         /* On gen7 and prior, the bottom 12 bits of the MCS base address are
          * used to store other information.  This should be ok, however,
          * because surface buffer addresses are always 4K page alinged.
          */
         uint32_t *aux_addr_dw = state_inout->state.map +
                                 device->isl_dev.ss.aux_addr_offset;
         assert((aux_address & 0xfff) == 0);
         assert(aux_address == (*aux_addr_dw & 0xfffff000));
         state_inout->aux_address = *aux_addr_dw;
      }
   }

   anv_state_flush(device, state_inout->state);

   if (image_param_out) {
      assert(view_usage == ISL_SURF_USAGE_STORAGE_BIT);
      isl_surf_fill_image_param(&device->isl_dev, image_param_out,
                                &surface->isl, &view);
   }
}

static VkImageAspectFlags
remap_aspect_flags(VkImageAspectFlags view_aspects)
{
   if (view_aspects & VK_IMAGE_ASPECT_ANY_COLOR_BIT) {
      if (_mesa_bitcount(view_aspects) == 1)
         return VK_IMAGE_ASPECT_COLOR_BIT;

      VkImageAspectFlags color_aspects = 0;
      for (uint32_t i = 0; i < _mesa_bitcount(view_aspects); i++)
         color_aspects |= VK_IMAGE_ASPECT_PLANE_0_BIT_KHR << i;
      return color_aspects;
   }
   /* No special remapping needed for depth & stencil aspects. */
   return view_aspects;
}

VkResult
anv_CreateImageView(VkDevice _device,
                    const VkImageViewCreateInfo *pCreateInfo,
                    const VkAllocationCallbacks *pAllocator,
                    VkImageView *pView)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_image, image, pCreateInfo->image);
   struct anv_image_view *iview;

   iview = vk_zalloc2(&device->alloc, pAllocator, sizeof(*iview), 8,
                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (iview == NULL)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   const VkImageSubresourceRange *range = &pCreateInfo->subresourceRange;

   assert(range->layerCount > 0);
   assert(range->baseMipLevel < image->levels);

   const VkImageViewUsageCreateInfoKHR *usage_info =
      vk_find_struct_const(pCreateInfo, IMAGE_VIEW_USAGE_CREATE_INFO_KHR);
   VkImageUsageFlags view_usage = usage_info ? usage_info->usage : image->usage;
   /* View usage should be a subset of image usage */
   assert((view_usage & ~image->usage) == 0);
   assert(view_usage & (VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_STORAGE_BIT |
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));

   switch (image->type) {
   default:
      unreachable("bad VkImageType");
   case VK_IMAGE_TYPE_1D:
   case VK_IMAGE_TYPE_2D:
      assert(range->baseArrayLayer + anv_get_layerCount(image, range) - 1 <= image->array_size);
      break;
   case VK_IMAGE_TYPE_3D:
      assert(range->baseArrayLayer + anv_get_layerCount(image, range) - 1
             <= anv_minify(image->extent.depth, range->baseMipLevel));
      break;
   }

   /* First expand aspects to the image's ones (for example
    * VK_IMAGE_ASPECT_COLOR_BIT will be converted to
    * VK_IMAGE_ASPECT_PLANE_0_BIT_KHR | VK_IMAGE_ASPECT_PLANE_1_BIT_KHR |
    * VK_IMAGE_ASPECT_PLANE_2_BIT_KHR for an image of format
    * VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM_KHR.
    */
   VkImageAspectFlags expanded_aspects =
      anv_image_expand_aspects(image, range->aspectMask);

   iview->image = image;

   /* Remap the expanded aspects for the image view. For example if only
    * VK_IMAGE_ASPECT_PLANE_1_BIT_KHR was given in range->aspectMask, we will
    * convert it to VK_IMAGE_ASPECT_COLOR_BIT since from the point of view of
    * the image view, it only has a single plane.
    */
   iview->aspect_mask = remap_aspect_flags(expanded_aspects);
   iview->n_planes = anv_image_aspect_get_planes(iview->aspect_mask);
   iview->vk_format = pCreateInfo->format;

   iview->extent = (VkExtent3D) {
      .width  = anv_minify(image->extent.width , range->baseMipLevel),
      .height = anv_minify(image->extent.height, range->baseMipLevel),
      .depth  = anv_minify(image->extent.depth , range->baseMipLevel),
   };

   /* Now go through the underlying image selected planes (computed in
    * expanded_aspects) and map them to planes in the image view.
    */
   uint32_t iaspect_bit, vplane = 0;
   anv_foreach_image_aspect_bit(iaspect_bit, image, expanded_aspects) {
      uint32_t iplane =
         anv_image_aspect_to_plane(expanded_aspects, 1UL << iaspect_bit);
      VkImageAspectFlags vplane_aspect =
         anv_plane_to_aspect(iview->aspect_mask, vplane);
      struct anv_format_plane format =
         anv_get_format_plane(&device->info, pCreateInfo->format,
                              vplane_aspect, image->tiling);

      iview->planes[vplane].image_plane = iplane;

      iview->planes[vplane].isl = (struct isl_view) {
         .format = format.isl_format,
         .base_level = range->baseMipLevel,
         .levels = anv_get_levelCount(image, range),
         .base_array_layer = range->baseArrayLayer,
         .array_len = anv_get_layerCount(image, range),
         .swizzle = {
            .r = remap_swizzle(pCreateInfo->components.r,
                               VK_COMPONENT_SWIZZLE_R, format.swizzle),
            .g = remap_swizzle(pCreateInfo->components.g,
                               VK_COMPONENT_SWIZZLE_G, format.swizzle),
            .b = remap_swizzle(pCreateInfo->components.b,
                               VK_COMPONENT_SWIZZLE_B, format.swizzle),
            .a = remap_swizzle(pCreateInfo->components.a,
                               VK_COMPONENT_SWIZZLE_A, format.swizzle),
         },
      };

      if (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_3D) {
         iview->planes[vplane].isl.base_array_layer = 0;
         iview->planes[vplane].isl.array_len = iview->extent.depth;
      }

      if (pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_CUBE ||
          pCreateInfo->viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
         iview->planes[vplane].isl.usage = ISL_SURF_USAGE_CUBE_BIT;
      } else {
         iview->planes[vplane].isl.usage = 0;
      }

      if (view_usage & VK_IMAGE_USAGE_SAMPLED_BIT ||
          (view_usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT &&
           !(iview->aspect_mask & VK_IMAGE_ASPECT_COLOR_BIT))) {
         iview->planes[vplane].optimal_sampler_surface_state.state = alloc_surface_state(device);
         iview->planes[vplane].general_sampler_surface_state.state = alloc_surface_state(device);

         enum isl_aux_usage general_aux_usage =
            anv_layout_to_aux_usage(&device->info, image, 1UL << iaspect_bit,
                                    VK_IMAGE_LAYOUT_GENERAL);
         enum isl_aux_usage optimal_aux_usage =
            anv_layout_to_aux_usage(&device->info, image, 1UL << iaspect_bit,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

         anv_image_fill_surface_state(device, image, 1ULL << iaspect_bit,
                                      &iview->planes[vplane].isl,
                                      ISL_SURF_USAGE_TEXTURE_BIT,
                                      optimal_aux_usage, NULL,
                                      ANV_IMAGE_VIEW_STATE_TEXTURE_OPTIMAL,
                                      &iview->planes[vplane].optimal_sampler_surface_state,
                                      NULL);

         anv_image_fill_surface_state(device, image, 1ULL << iaspect_bit,
                                      &iview->planes[vplane].isl,
                                      ISL_SURF_USAGE_TEXTURE_BIT,
                                      general_aux_usage, NULL,
                                      0,
                                      &iview->planes[vplane].general_sampler_surface_state,
                                      NULL);
      }

      /* NOTE: This one needs to go last since it may stomp isl_view.format */
      if (view_usage & VK_IMAGE_USAGE_STORAGE_BIT) {
         iview->planes[vplane].storage_surface_state.state = alloc_surface_state(device);
         iview->planes[vplane].writeonly_storage_surface_state.state = alloc_surface_state(device);

         anv_image_fill_surface_state(device, image, 1ULL << iaspect_bit,
                                      &iview->planes[vplane].isl,
                                      ISL_SURF_USAGE_STORAGE_BIT,
                                      ISL_AUX_USAGE_NONE, NULL,
                                      0,
                                      &iview->planes[vplane].storage_surface_state,
                                      &iview->planes[vplane].storage_image_param);

         anv_image_fill_surface_state(device, image, 1ULL << iaspect_bit,
                                      &iview->planes[vplane].isl,
                                      ISL_SURF_USAGE_STORAGE_BIT,
                                      ISL_AUX_USAGE_NONE, NULL,
                                      ANV_IMAGE_VIEW_STATE_STORAGE_WRITE_ONLY,
                                      &iview->planes[vplane].writeonly_storage_surface_state,
                                      NULL);
      }

      vplane++;
   }

   *pView = anv_image_view_to_handle(iview);

   return VK_SUCCESS;
}

void
anv_DestroyImageView(VkDevice _device, VkImageView _iview,
                     const VkAllocationCallbacks *pAllocator)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_image_view, iview, _iview);

   if (!iview)
      return;

   for (uint32_t plane = 0; plane < iview->n_planes; plane++) {
      if (iview->planes[plane].optimal_sampler_surface_state.state.alloc_size > 0) {
         anv_state_pool_free(&device->surface_state_pool,
                             iview->planes[plane].optimal_sampler_surface_state.state);
      }

      if (iview->planes[plane].general_sampler_surface_state.state.alloc_size > 0) {
         anv_state_pool_free(&device->surface_state_pool,
                             iview->planes[plane].general_sampler_surface_state.state);
      }

      if (iview->planes[plane].storage_surface_state.state.alloc_size > 0) {
         anv_state_pool_free(&device->surface_state_pool,
                             iview->planes[plane].storage_surface_state.state);
      }

      if (iview->planes[plane].writeonly_storage_surface_state.state.alloc_size > 0) {
         anv_state_pool_free(&device->surface_state_pool,
                             iview->planes[plane].writeonly_storage_surface_state.state);
      }
   }

   vk_free2(&device->alloc, pAllocator, iview);
}


VkResult
anv_CreateBufferView(VkDevice _device,
                     const VkBufferViewCreateInfo *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkBufferView *pView)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_buffer, buffer, pCreateInfo->buffer);
   struct anv_buffer_view *view;

   view = vk_alloc2(&device->alloc, pAllocator, sizeof(*view), 8,
                     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!view)
      return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

   /* TODO: Handle the format swizzle? */

   view->format = anv_get_isl_format(&device->info, pCreateInfo->format,
                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                     VK_IMAGE_TILING_LINEAR);
   const uint32_t format_bs = isl_format_get_layout(view->format)->bpb / 8;
   view->bo = buffer->bo;
   view->offset = buffer->offset + pCreateInfo->offset;
   view->range = anv_buffer_get_range(buffer, pCreateInfo->offset,
                                              pCreateInfo->range);
   view->range = align_down_npot_u32(view->range, format_bs);

   if (buffer->usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) {
      view->surface_state = alloc_surface_state(device);

      anv_fill_buffer_surface_state(device, view->surface_state,
                                    view->format,
                                    view->offset, view->range, format_bs);
   } else {
      view->surface_state = (struct anv_state){ 0 };
   }

   if (buffer->usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) {
      view->storage_surface_state = alloc_surface_state(device);
      view->writeonly_storage_surface_state = alloc_surface_state(device);

      enum isl_format storage_format =
         isl_has_matching_typed_storage_image_format(&device->info,
                                                     view->format) ?
         isl_lower_storage_image_format(&device->info, view->format) :
         ISL_FORMAT_RAW;

      anv_fill_buffer_surface_state(device, view->storage_surface_state,
                                    storage_format,
                                    view->offset, view->range,
                                    (storage_format == ISL_FORMAT_RAW ? 1 :
                                     isl_format_get_layout(storage_format)->bpb / 8));

      /* Write-only accesses should use the original format. */
      anv_fill_buffer_surface_state(device, view->writeonly_storage_surface_state,
                                    view->format,
                                    view->offset, view->range,
                                    isl_format_get_layout(view->format)->bpb / 8);

      isl_buffer_fill_image_param(&device->isl_dev,
                                  &view->storage_image_param,
                                  view->format, view->range);
   } else {
      view->storage_surface_state = (struct anv_state){ 0 };
      view->writeonly_storage_surface_state = (struct anv_state){ 0 };
   }

   *pView = anv_buffer_view_to_handle(view);

   return VK_SUCCESS;
}

void
anv_DestroyBufferView(VkDevice _device, VkBufferView bufferView,
                      const VkAllocationCallbacks *pAllocator)
{
   ANV_FROM_HANDLE(anv_device, device, _device);
   ANV_FROM_HANDLE(anv_buffer_view, view, bufferView);

   if (!view)
      return;

   if (view->surface_state.alloc_size > 0)
      anv_state_pool_free(&device->surface_state_pool,
                          view->surface_state);

   if (view->storage_surface_state.alloc_size > 0)
      anv_state_pool_free(&device->surface_state_pool,
                          view->storage_surface_state);

   if (view->writeonly_storage_surface_state.alloc_size > 0)
      anv_state_pool_free(&device->surface_state_pool,
                          view->writeonly_storage_surface_state);

   vk_free2(&device->alloc, pAllocator, view);
}

const struct anv_surface *
anv_image_get_surface_for_aspect_mask(const struct anv_image *image,
                                      VkImageAspectFlags aspect_mask)
{
   VkImageAspectFlags sanitized_mask;

   switch (aspect_mask) {
   case VK_IMAGE_ASPECT_COLOR_BIT:
      assert(image->aspects == VK_IMAGE_ASPECT_COLOR_BIT);
      sanitized_mask = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
   case VK_IMAGE_ASPECT_DEPTH_BIT:
      assert(image->aspects & VK_IMAGE_ASPECT_DEPTH_BIT);
      sanitized_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
      break;
   case VK_IMAGE_ASPECT_STENCIL_BIT:
      assert(image->aspects & VK_IMAGE_ASPECT_STENCIL_BIT);
      sanitized_mask = VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
   case VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT:
      /* FINISHME: The Vulkan spec (git a511ba2) requires support for
       * combined depth stencil formats. Specifically, it states:
       *
       *    At least one of ename:VK_FORMAT_D24_UNORM_S8_UINT or
       *    ename:VK_FORMAT_D32_SFLOAT_S8_UINT must be supported.
       *
       * Image views with both depth and stencil aspects are only valid for
       * render target attachments, in which case
       * cmd_buffer_emit_depth_stencil() will pick out both the depth and
       * stencil surfaces from the underlying surface.
       */
      if (image->aspects & VK_IMAGE_ASPECT_DEPTH_BIT) {
         sanitized_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
      } else {
         assert(image->aspects == VK_IMAGE_ASPECT_STENCIL_BIT);
         sanitized_mask = VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      break;
   case VK_IMAGE_ASPECT_PLANE_0_BIT_KHR:
      assert((image->aspects & ~VK_IMAGE_ASPECT_ANY_COLOR_BIT) == 0);
      sanitized_mask = VK_IMAGE_ASPECT_PLANE_0_BIT_KHR;
      break;
   case VK_IMAGE_ASPECT_PLANE_1_BIT_KHR:
      assert((image->aspects & ~VK_IMAGE_ASPECT_ANY_COLOR_BIT) == 0);
      sanitized_mask = VK_IMAGE_ASPECT_PLANE_1_BIT_KHR;
      break;
   case VK_IMAGE_ASPECT_PLANE_2_BIT_KHR:
      assert((image->aspects & ~VK_IMAGE_ASPECT_ANY_COLOR_BIT) == 0);
      sanitized_mask = VK_IMAGE_ASPECT_PLANE_2_BIT_KHR;
      break;
   default:
       unreachable("image does not have aspect");
       return NULL;
   }

   uint32_t plane = anv_image_aspect_to_plane(image->aspects, sanitized_mask);
   return &image->planes[plane].surface;
}
