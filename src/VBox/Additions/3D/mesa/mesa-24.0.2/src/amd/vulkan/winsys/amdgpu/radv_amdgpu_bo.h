/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based on amdgpu winsys.
 * Copyright © 2011 Marek Olšák <maraeo@gmail.com>
 * Copyright © 2015 Advanced Micro Devices, Inc.
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

#ifndef RADV_AMDGPU_BO_H
#define RADV_AMDGPU_BO_H

#include "radv_amdgpu_winsys.h"

struct radv_amdgpu_map_range {
   uint64_t offset;
   uint64_t size;
   struct radv_amdgpu_winsys_bo *bo;
   uint64_t bo_offset;
};

struct radv_amdgpu_winsys_bo {
   struct radeon_winsys_bo base;
   amdgpu_va_handle va_handle;
   uint64_t size;
   bool is_virtual;
   uint8_t priority;

   union {
      /* physical bo */
      struct {
         amdgpu_bo_handle bo;
         uint32_t bo_handle;
      };
      /* virtual bo */
      struct {
         struct u_rwlock lock;

         struct radv_amdgpu_map_range *ranges;
         uint32_t range_count;
         uint32_t range_capacity;

         struct radv_amdgpu_winsys_bo **bos;
         uint32_t bo_count;
         uint32_t bo_capacity;
      };
   };
};

static inline struct radv_amdgpu_winsys_bo *
radv_amdgpu_winsys_bo(struct radeon_winsys_bo *bo)
{
   return (struct radv_amdgpu_winsys_bo *)bo;
}

void radv_amdgpu_bo_init_functions(struct radv_amdgpu_winsys *ws);

#endif /* RADV_AMDGPU_BO_H */
