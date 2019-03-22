/**********************************************************
 * Copyright 2009-2015 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

/**
 * @file
 * Common definitions for the VMware SVGA winsys.
 *
 * @author Jose Fonseca <jfonseca@vmware.com>
 */


#ifndef VMW_SCREEN_H_
#define VMW_SCREEN_H_

#ifdef VBOX_WITH_MESA3D_COMPILE
#define dev_t uint32_t
#endif

#include "pipe/p_compiler.h"
#include "pipe/p_state.h"

#include "svga_winsys.h"
#include "pipebuffer/pb_buffer_fenced.h"
#include <os/os_thread.h>

#ifndef VBOX_WITH_MESA3D_NINE_SVGA
#define VMW_GMR_POOL_SIZE (16*1024*1024)
#else
#define VMW_GMR_POOL_SIZE (32*1024*1024)
#endif
#define VMW_QUERY_POOL_SIZE (8192)
#define VMW_DEBUG_FLUSH_STACK 10

/*
 * Something big, but arbitrary. The kernel reports an error if it can't
 * handle this, and the svga driver will resort to multiple partial
 * uploads.
 */
#define VMW_MAX_BUFFER_SIZE (512*1024*1024)

struct pb_manager;
struct vmw_region;

struct vmw_cap_3d {
   boolean has_cap;
   SVGA3dDevCapResult result;
};

struct vmw_winsys_screen
{
   struct svga_winsys_screen base;

   struct {
      int drm_fd;
      uint32_t hwversion;
      uint32_t num_cap_3d;
      struct vmw_cap_3d *cap_3d;
      uint64_t max_mob_memory;
      uint64_t max_surface_memory;
      uint64_t max_texture_size;
      boolean have_drm_2_6;
      boolean have_drm_2_9;
      uint32_t drm_execbuf_version;
   } ioctl;

   struct {
      struct pb_manager *gmr;
      struct pb_manager *gmr_mm;
      struct pb_manager *gmr_fenced;
      struct pb_manager *gmr_slab;
      struct pb_manager *gmr_slab_fenced;
      struct pb_manager *query_mm;
      struct pb_manager *query_fenced;
      struct pb_manager *mob_fenced;
      struct pb_manager *mob_cache;
      struct pb_manager *mob_shader_slab;
      struct pb_manager *mob_shader_slab_fenced;
   } pools;

   struct pb_fence_ops *fence_ops;

   /*
    * Screen instances
    */
   dev_t device;
   int open_count;

   cnd_t cs_cond;
   mtx_t cs_mutex;
};


static inline struct vmw_winsys_screen *
vmw_winsys_screen(struct svga_winsys_screen *base)
{
   return (struct vmw_winsys_screen *)base;
}

/*  */
uint32_t
vmw_region_size(struct vmw_region *region);

uint32
vmw_ioctl_context_create(struct vmw_winsys_screen *vws);

uint32
vmw_ioctl_extended_context_create(struct vmw_winsys_screen *vws,
                                  boolean vgpu10);

void
vmw_ioctl_context_destroy(struct vmw_winsys_screen *vws,
                          uint32 cid);

uint32
vmw_ioctl_surface_create(struct vmw_winsys_screen *vws,
                         SVGA3dSurfaceFlags flags,
                         SVGA3dSurfaceFormat format,
                         unsigned usage,
                         SVGA3dSize size,
                         uint32 numFaces,
                         uint32 numMipLevels,
                         unsigned sampleCount);
uint32
vmw_ioctl_gb_surface_create(struct vmw_winsys_screen *vws,
			    SVGA3dSurfaceFlags flags,
			    SVGA3dSurfaceFormat format,
                            unsigned usage,
			    SVGA3dSize size,
			    uint32 numFaces,
			    uint32 numMipLevels,
                            unsigned sampleCount,
                            uint32 buffer_handle,
			    struct vmw_region **p_region);

int
vmw_ioctl_gb_surface_ref(struct vmw_winsys_screen *vws,
                         const struct winsys_handle *whandle,
                         SVGA3dSurfaceFlags *flags,
                         SVGA3dSurfaceFormat *format,
                         uint32_t *numMipLevels,
                         uint32_t *handle,
                         struct vmw_region **p_region);

void
vmw_ioctl_surface_destroy(struct vmw_winsys_screen *vws,
                          uint32 sid);

void
vmw_ioctl_command(struct vmw_winsys_screen *vws,
                  int32_t cid,
                  uint32_t throttle_us,
                  void *commands,
                  uint32_t size,
                  struct pipe_fence_handle **fence,
                  int32_t imported_fence_fd,
                  uint32_t flags);

struct vmw_region *
vmw_ioctl_region_create(struct vmw_winsys_screen *vws, uint32_t size);

void
vmw_ioctl_region_destroy(struct vmw_region *region);

struct SVGAGuestPtr
vmw_ioctl_region_ptr(struct vmw_region *region);

void *
vmw_ioctl_region_map(struct vmw_region *region);
void
vmw_ioctl_region_unmap(struct vmw_region *region);


int
vmw_ioctl_fence_finish(struct vmw_winsys_screen *vws,
                       uint32_t handle, uint32_t flags);

int
vmw_ioctl_fence_signalled(struct vmw_winsys_screen *vws,
                          uint32_t handle, uint32_t flags);

void
vmw_ioctl_fence_unref(struct vmw_winsys_screen *vws,
		      uint32_t handle);

uint32
vmw_ioctl_shader_create(struct vmw_winsys_screen *vws,
			SVGA3dShaderType type,
			uint32 code_len);
void
vmw_ioctl_shader_destroy(struct vmw_winsys_screen *vws, uint32 shid);

int
vmw_ioctl_syncforcpu(struct vmw_region *region,
                     boolean dont_block,
                     boolean readonly,
                     boolean allow_cs);
void
vmw_ioctl_releasefromcpu(struct vmw_region *region,
                         boolean readonly,
                         boolean allow_cs);
/* Initialize parts of vmw_winsys_screen at startup:
 */
boolean vmw_ioctl_init(struct vmw_winsys_screen *vws);
boolean vmw_pools_init(struct vmw_winsys_screen *vws);
boolean vmw_query_pools_init(struct vmw_winsys_screen *vws);
boolean vmw_mob_pools_init(struct vmw_winsys_screen *vws);
boolean vmw_winsys_screen_init_svga(struct vmw_winsys_screen *vws);

void vmw_ioctl_cleanup(struct vmw_winsys_screen *vws);
void vmw_pools_cleanup(struct vmw_winsys_screen *vws);

struct vmw_winsys_screen *vmw_winsys_create(int fd);
void vmw_winsys_destroy(struct vmw_winsys_screen *sws);
void vmw_winsys_screen_set_throttling(struct pipe_screen *screen,
				      uint32_t throttle_us);

struct pb_manager *
simple_fenced_bufmgr_create(struct pb_manager *provider,
			    struct pb_fence_ops *ops);
void
vmw_fences_signal(struct pb_fence_ops *fence_ops,
                  uint32_t signaled,
                  uint32_t emitted,
                  boolean has_emitted);

struct svga_winsys_gb_shader *
vmw_svga_winsys_shader_create(struct svga_winsys_screen *sws,
			      SVGA3dShaderType type,
			      const uint32 *bytecode,
			      uint32 bytecodeLen);
void
vmw_svga_winsys_shader_destroy(struct svga_winsys_screen *sws,
			       struct svga_winsys_gb_shader *shader);

#endif /* VMW_SCREEN_H_ */
