/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PVR_DRM_JOB_COMMON_H
#define PVR_DRM_JOB_COMMON_H

#include "drm-uapi/pvr_drm.h"
#include "pvr_winsys.h"
#include "util/macros.h"

static enum drm_pvr_ctx_priority
pvr_drm_from_winsys_priority(enum pvr_winsys_ctx_priority priority)
{
   switch (priority) {
   case PVR_WINSYS_CTX_PRIORITY_HIGH:
      return DRM_PVR_CTX_PRIORITY_HIGH;
   case PVR_WINSYS_CTX_PRIORITY_MEDIUM:
      return DRM_PVR_CTX_PRIORITY_NORMAL;
   case PVR_WINSYS_CTX_PRIORITY_LOW:
      return DRM_PVR_CTX_PRIORITY_LOW;
   default:
      unreachable("Invalid winsys context priority.");
   }
}

#endif /* PVR_DRM_JOB_COMMON_H */
