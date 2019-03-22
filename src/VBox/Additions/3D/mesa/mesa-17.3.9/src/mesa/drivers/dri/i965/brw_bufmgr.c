/*
 * Copyright © 2007 Red Hat Inc.
 * Copyright © 2007-2017 Intel Corporation
 * Copyright © 2006 VMware, Inc.
 * All Rights Reserved.
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

/*
 * Authors: Thomas Hellström <thellstrom@vmware.com>
 *          Keith Whitwell <keithw@vmware.com>
 *          Eric Anholt <eric@anholt.net>
 *          Dave Airlie <airlied@linux.ie>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xf86drm.h>
#include <util/u_atomic.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

#include "errno.h"
#ifndef ETIME
#define ETIME ETIMEDOUT
#endif
#include "common/gen_clflush.h"
#include "common/gen_debug.h"
#include "common/gen_device_info.h"
#include "libdrm_macros.h"
#include "main/macros.h"
#include "util/macros.h"
#include "util/hash_table.h"
#include "util/list.h"
#include "brw_bufmgr.h"
#include "brw_context.h"
#include "string.h"

#include "i915_drm.h"

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#include <memcheck.h>
#define VG(x) x
#else
#define VG(x)
#endif

/* VALGRIND_FREELIKE_BLOCK unfortunately does not actually undo the earlier
 * VALGRIND_MALLOCLIKE_BLOCK but instead leaves vg convinced the memory is
 * leaked. All because it does not call VG(cli_free) from its
 * VG_USERREQ__FREELIKE_BLOCK handler. Instead of treating the memory like
 * and allocation, we mark it available for use upon mmapping and remove
 * it upon unmapping.
 */
#define VG_DEFINED(ptr, size) VG(VALGRIND_MAKE_MEM_DEFINED(ptr, size))
#define VG_NOACCESS(ptr, size) VG(VALGRIND_MAKE_MEM_NOACCESS(ptr, size))

#define memclear(s) memset(&s, 0, sizeof(s))

#define FILE_DEBUG_FLAG DEBUG_BUFMGR

static inline int
atomic_add_unless(int *v, int add, int unless)
{
   int c, old;
   c = p_atomic_read(v);
   while (c != unless && (old = p_atomic_cmpxchg(v, c, c + add)) != c)
      c = old;
   return c == unless;
}

struct bo_cache_bucket {
   struct list_head head;
   uint64_t size;
};

struct brw_bufmgr {
   int fd;

   mtx_t lock;

   /** Array of lists of cached gem objects of power-of-two sizes */
   struct bo_cache_bucket cache_bucket[14 * 4];
   int num_buckets;
   time_t time;

   struct hash_table *name_table;
   struct hash_table *handle_table;

   bool has_llc:1;
   bool has_mmap_wc:1;
   bool bo_reuse:1;
};

static int bo_set_tiling_internal(struct brw_bo *bo, uint32_t tiling_mode,
                                  uint32_t stride);

static void bo_free(struct brw_bo *bo);

static uint32_t
key_hash_uint(const void *key)
{
   return _mesa_hash_data(key, 4);
}

static bool
key_uint_equal(const void *a, const void *b)
{
   return *((unsigned *) a) == *((unsigned *) b);
}

static struct brw_bo *
hash_find_bo(struct hash_table *ht, unsigned int key)
{
   struct hash_entry *entry = _mesa_hash_table_search(ht, &key);
   return entry ? (struct brw_bo *) entry->data : NULL;
}

static uint64_t
bo_tile_size(struct brw_bufmgr *bufmgr, uint64_t size, uint32_t tiling)
{
   if (tiling == I915_TILING_NONE)
      return size;

   /* 965+ just need multiples of page size for tiling */
   return ALIGN(size, 4096);
}

/*
 * Round a given pitch up to the minimum required for X tiling on a
 * given chip.  We use 512 as the minimum to allow for a later tiling
 * change.
 */
static uint32_t
bo_tile_pitch(struct brw_bufmgr *bufmgr, uint32_t pitch, uint32_t tiling)
{
   unsigned long tile_width;

   /* If untiled, then just align it so that we can do rendering
    * to it with the 3D engine.
    */
   if (tiling == I915_TILING_NONE)
      return ALIGN(pitch, 64);

   if (tiling == I915_TILING_X)
      tile_width = 512;
   else
      tile_width = 128;

   /* 965 is flexible */
   return ALIGN(pitch, tile_width);
}

static struct bo_cache_bucket *
bucket_for_size(struct brw_bufmgr *bufmgr, uint64_t size)
{
   int i;

   for (i = 0; i < bufmgr->num_buckets; i++) {
      struct bo_cache_bucket *bucket = &bufmgr->cache_bucket[i];
      if (bucket->size >= size) {
         return bucket;
      }
   }

   return NULL;
}

int
brw_bo_busy(struct brw_bo *bo)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_busy busy;
   int ret;

   memclear(busy);
   busy.handle = bo->gem_handle;

   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_BUSY, &busy);
   if (ret == 0) {
      bo->idle = !busy.busy;
      return busy.busy;
   }
   return false;
}

int
brw_bo_madvise(struct brw_bo *bo, int state)
{
   struct drm_i915_gem_madvise madv;

   memclear(madv);
   madv.handle = bo->gem_handle;
   madv.madv = state;
   madv.retained = 1;
   drmIoctl(bo->bufmgr->fd, DRM_IOCTL_I915_GEM_MADVISE, &madv);

   return madv.retained;
}

/* drop the oldest entries that have been purged by the kernel */
static void
brw_bo_cache_purge_bucket(struct brw_bufmgr *bufmgr,
                          struct bo_cache_bucket *bucket)
{
   list_for_each_entry_safe(struct brw_bo, bo, &bucket->head, head) {
      if (brw_bo_madvise(bo, I915_MADV_DONTNEED))
         break;

      list_del(&bo->head);
      bo_free(bo);
   }
}

static struct brw_bo *
bo_alloc_internal(struct brw_bufmgr *bufmgr,
                  const char *name,
                  uint64_t size,
                  unsigned flags,
                  uint32_t tiling_mode,
                  uint32_t stride, uint64_t alignment)
{
   struct brw_bo *bo;
   unsigned int page_size = getpagesize();
   int ret;
   struct bo_cache_bucket *bucket;
   bool alloc_from_cache;
   uint64_t bo_size;
   bool busy = false;
   bool zeroed = false;

   if (flags & BO_ALLOC_BUSY)
      busy = true;

   if (flags & BO_ALLOC_ZEROED)
      zeroed = true;

   /* BUSY does doesn't really jive with ZEROED as we have to wait for it to
    * be idle before we can memset.  Just disallow that combination.
    */
   assert(!(busy && zeroed));

   /* Round the allocated size up to a power of two number of pages. */
   bucket = bucket_for_size(bufmgr, size);

   /* If we don't have caching at this size, don't actually round the
    * allocation up.
    */
   if (bucket == NULL) {
      bo_size = size;
      if (bo_size < page_size)
         bo_size = page_size;
   } else {
      bo_size = bucket->size;
   }

   mtx_lock(&bufmgr->lock);
   /* Get a buffer out of the cache if available */
retry:
   alloc_from_cache = false;
   if (bucket != NULL && !list_empty(&bucket->head)) {
      if (busy && !zeroed) {
         /* Allocate new render-target BOs from the tail (MRU)
          * of the list, as it will likely be hot in the GPU
          * cache and in the aperture for us.  If the caller
          * asked us to zero the buffer, we don't want this
          * because we are going to mmap it.
          */
         bo = LIST_ENTRY(struct brw_bo, bucket->head.prev, head);
         list_del(&bo->head);
         alloc_from_cache = true;
         bo->align = alignment;
      } else {
         assert(alignment == 0);
         /* For non-render-target BOs (where we're probably
          * going to map it first thing in order to fill it
          * with data), check if the last BO in the cache is
          * unbusy, and only reuse in that case. Otherwise,
          * allocating a new buffer is probably faster than
          * waiting for the GPU to finish.
          */
         bo = LIST_ENTRY(struct brw_bo, bucket->head.next, head);
         if (!brw_bo_busy(bo)) {
            alloc_from_cache = true;
            list_del(&bo->head);
         }
      }

      if (alloc_from_cache) {
         if (!brw_bo_madvise(bo, I915_MADV_WILLNEED)) {
            bo_free(bo);
            brw_bo_cache_purge_bucket(bufmgr, bucket);
            goto retry;
         }

         if (bo_set_tiling_internal(bo, tiling_mode, stride)) {
            bo_free(bo);
            goto retry;
         }

         if (zeroed) {
            void *map = brw_bo_map(NULL, bo, MAP_WRITE | MAP_RAW);
            if (!map) {
               bo_free(bo);
               goto retry;
            }
            memset(map, 0, bo_size);
         }
      }
   }

   if (!alloc_from_cache) {
      struct drm_i915_gem_create create;

      bo = calloc(1, sizeof(*bo));
      if (!bo)
         goto err;

      bo->size = bo_size;
      bo->idle = true;

      memclear(create);
      create.size = bo_size;

      /* All new BOs we get from the kernel are zeroed, so we don't need to
       * worry about that here.
       */
      ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_CREATE, &create);
      if (ret != 0) {
         free(bo);
         goto err;
      }

      bo->gem_handle = create.handle;

      bo->bufmgr = bufmgr;
      bo->align = alignment;

      bo->tiling_mode = I915_TILING_NONE;
      bo->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
      bo->stride = 0;

      if (bo_set_tiling_internal(bo, tiling_mode, stride))
         goto err_free;

      /* Calling set_domain() will allocate pages for the BO outside of the
       * struct mutex lock in the kernel, which is more efficient than waiting
       * to create them during the first execbuf that uses the BO.
       */
      struct drm_i915_gem_set_domain sd = {
         .handle = bo->gem_handle,
         .read_domains = I915_GEM_DOMAIN_CPU,
         .write_domain = 0,
      };

      if (drmIoctl(bo->bufmgr->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &sd) != 0)
         goto err_free;
   }

   bo->name = name;
   p_atomic_set(&bo->refcount, 1);
   bo->reusable = true;
   bo->cache_coherent = bufmgr->has_llc;
   bo->index = -1;

   mtx_unlock(&bufmgr->lock);

   DBG("bo_create: buf %d (%s) %llub\n", bo->gem_handle, bo->name,
       (unsigned long long) size);

   return bo;

err_free:
   bo_free(bo);
err:
   mtx_unlock(&bufmgr->lock);
   return NULL;
}

struct brw_bo *
brw_bo_alloc(struct brw_bufmgr *bufmgr,
             const char *name, uint64_t size, uint64_t alignment)
{
   return bo_alloc_internal(bufmgr, name, size, 0, I915_TILING_NONE, 0, 0);
}

struct brw_bo *
brw_bo_alloc_tiled(struct brw_bufmgr *bufmgr, const char *name,
                   uint64_t size, uint32_t tiling_mode, uint32_t pitch,
                   unsigned flags)
{
   return bo_alloc_internal(bufmgr, name, size, flags, tiling_mode, pitch, 0);
}

struct brw_bo *
brw_bo_alloc_tiled_2d(struct brw_bufmgr *bufmgr, const char *name,
                      int x, int y, int cpp, uint32_t tiling,
                      uint32_t *pitch, unsigned flags)
{
   uint64_t size;
   uint32_t stride;
   unsigned long aligned_y, height_alignment;

   /* If we're tiled, our allocations are in 8 or 32-row blocks,
    * so failure to align our height means that we won't allocate
    * enough pages.
    *
    * If we're untiled, we still have to align to 2 rows high
    * because the data port accesses 2x2 blocks even if the
    * bottom row isn't to be rendered, so failure to align means
    * we could walk off the end of the GTT and fault.  This is
    * documented on 965, and may be the case on older chipsets
    * too so we try to be careful.
    */
   aligned_y = y;
   height_alignment = 2;

   if (tiling == I915_TILING_X)
      height_alignment = 8;
   else if (tiling == I915_TILING_Y)
      height_alignment = 32;
   aligned_y = ALIGN(y, height_alignment);

   stride = x * cpp;
   stride = bo_tile_pitch(bufmgr, stride, tiling);
   size = stride * aligned_y;
   size = bo_tile_size(bufmgr, size, tiling);
   *pitch = stride;

   if (tiling == I915_TILING_NONE)
      stride = 0;

   return bo_alloc_internal(bufmgr, name, size, flags, tiling, stride, 0);
}

/**
 * Returns a brw_bo wrapping the given buffer object handle.
 *
 * This can be used when one application needs to pass a buffer object
 * to another.
 */
struct brw_bo *
brw_bo_gem_create_from_name(struct brw_bufmgr *bufmgr,
                            const char *name, unsigned int handle)
{
   struct brw_bo *bo;
   int ret;
   struct drm_gem_open open_arg;
   struct drm_i915_gem_get_tiling get_tiling;

   /* At the moment most applications only have a few named bo.
    * For instance, in a DRI client only the render buffers passed
    * between X and the client are named. And since X returns the
    * alternating names for the front/back buffer a linear search
    * provides a sufficiently fast match.
    */
   mtx_lock(&bufmgr->lock);
   bo = hash_find_bo(bufmgr->name_table, handle);
   if (bo) {
      brw_bo_reference(bo);
      goto out;
   }

   memclear(open_arg);
   open_arg.name = handle;
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_GEM_OPEN, &open_arg);
   if (ret != 0) {
      DBG("Couldn't reference %s handle 0x%08x: %s\n",
          name, handle, strerror(errno));
      bo = NULL;
      goto out;
   }
   /* Now see if someone has used a prime handle to get this
    * object from the kernel before by looking through the list
    * again for a matching gem_handle
    */
   bo = hash_find_bo(bufmgr->handle_table, open_arg.handle);
   if (bo) {
      brw_bo_reference(bo);
      goto out;
   }

   bo = calloc(1, sizeof(*bo));
   if (!bo)
      goto out;

   p_atomic_set(&bo->refcount, 1);

   bo->size = open_arg.size;
   bo->gtt_offset = 0;
   bo->bufmgr = bufmgr;
   bo->gem_handle = open_arg.handle;
   bo->name = name;
   bo->global_name = handle;
   bo->reusable = false;
   bo->external = true;

   _mesa_hash_table_insert(bufmgr->handle_table, &bo->gem_handle, bo);
   _mesa_hash_table_insert(bufmgr->name_table, &bo->global_name, bo);

   memclear(get_tiling);
   get_tiling.handle = bo->gem_handle;
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_GET_TILING, &get_tiling);
   if (ret != 0)
      goto err_unref;

   bo->tiling_mode = get_tiling.tiling_mode;
   bo->swizzle_mode = get_tiling.swizzle_mode;
   /* XXX stride is unknown */
   DBG("bo_create_from_handle: %d (%s)\n", handle, bo->name);

out:
   mtx_unlock(&bufmgr->lock);
   return bo;

err_unref:
   bo_free(bo);
   mtx_unlock(&bufmgr->lock);
   return NULL;
}

static void
bo_free(struct brw_bo *bo)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct drm_gem_close close;
   int ret;

   if (bo->map_cpu) {
      VG_NOACCESS(bo->map_cpu, bo->size);
      drm_munmap(bo->map_cpu, bo->size);
   }
   if (bo->map_wc) {
      VG_NOACCESS(bo->map_wc, bo->size);
      drm_munmap(bo->map_wc, bo->size);
   }
   if (bo->map_gtt) {
      VG_NOACCESS(bo->map_gtt, bo->size);
      drm_munmap(bo->map_gtt, bo->size);
   }

   if (bo->external) {
      struct hash_entry *entry;

      if (bo->global_name) {
         entry = _mesa_hash_table_search(bufmgr->name_table, &bo->global_name);
         _mesa_hash_table_remove(bufmgr->name_table, entry);
      }

      entry = _mesa_hash_table_search(bufmgr->handle_table, &bo->gem_handle);
      _mesa_hash_table_remove(bufmgr->handle_table, entry);
   }

   /* Close this object */
   memclear(close);
   close.handle = bo->gem_handle;
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_GEM_CLOSE, &close);
   if (ret != 0) {
      DBG("DRM_IOCTL_GEM_CLOSE %d failed (%s): %s\n",
          bo->gem_handle, bo->name, strerror(errno));
   }
   free(bo);
}

/** Frees all cached buffers significantly older than @time. */
static void
cleanup_bo_cache(struct brw_bufmgr *bufmgr, time_t time)
{
   int i;

   if (bufmgr->time == time)
      return;

   for (i = 0; i < bufmgr->num_buckets; i++) {
      struct bo_cache_bucket *bucket = &bufmgr->cache_bucket[i];

      list_for_each_entry_safe(struct brw_bo, bo, &bucket->head, head) {
         if (time - bo->free_time <= 1)
            break;

         list_del(&bo->head);

         bo_free(bo);
      }
   }

   bufmgr->time = time;
}

static void
bo_unreference_final(struct brw_bo *bo, time_t time)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct bo_cache_bucket *bucket;

   DBG("bo_unreference final: %d (%s)\n", bo->gem_handle, bo->name);

   bucket = bucket_for_size(bufmgr, bo->size);
   /* Put the buffer into our internal cache for reuse if we can. */
   if (bufmgr->bo_reuse && bo->reusable && bucket != NULL &&
       brw_bo_madvise(bo, I915_MADV_DONTNEED)) {
      bo->free_time = time;

      bo->name = NULL;
      bo->kflags = 0;

      list_addtail(&bo->head, &bucket->head);
   } else {
      bo_free(bo);
   }
}

void
brw_bo_unreference(struct brw_bo *bo)
{
   if (bo == NULL)
      return;

   assert(p_atomic_read(&bo->refcount) > 0);

   if (atomic_add_unless(&bo->refcount, -1, 1)) {
      struct brw_bufmgr *bufmgr = bo->bufmgr;
      struct timespec time;

      clock_gettime(CLOCK_MONOTONIC, &time);

      mtx_lock(&bufmgr->lock);

      if (p_atomic_dec_zero(&bo->refcount)) {
         bo_unreference_final(bo, time.tv_sec);
         cleanup_bo_cache(bufmgr, time.tv_sec);
      }

      mtx_unlock(&bufmgr->lock);
   }
}

static void
bo_wait_with_stall_warning(struct brw_context *brw,
                           struct brw_bo *bo,
                           const char *action)
{
   bool busy = brw && brw->perf_debug && !bo->idle;
   double elapsed = unlikely(busy) ? -get_time() : 0.0;

   brw_bo_wait_rendering(bo);

   if (unlikely(busy)) {
      elapsed += get_time();
      if (elapsed > 1e-5) /* 0.01ms */
         perf_debug("%s a busy \"%s\" BO stalled and took %.03f ms.\n",
                    action, bo->name, elapsed * 1000);
   }
}

static void
print_flags(unsigned flags)
{
   if (flags & MAP_READ)
      DBG("READ ");
   if (flags & MAP_WRITE)
      DBG("WRITE ");
   if (flags & MAP_ASYNC)
      DBG("ASYNC ");
   if (flags & MAP_PERSISTENT)
      DBG("PERSISTENT ");
   if (flags & MAP_COHERENT)
      DBG("COHERENT ");
   if (flags & MAP_RAW)
      DBG("RAW ");
   DBG("\n");
}

static void *
brw_bo_map_cpu(struct brw_context *brw, struct brw_bo *bo, unsigned flags)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   /* We disallow CPU maps for writing to non-coherent buffers, as the
    * CPU map can become invalidated when a batch is flushed out, which
    * can happen at unpredictable times.  You should use WC maps instead.
    */
   assert(bo->cache_coherent || !(flags & MAP_WRITE));

   if (!bo->map_cpu) {
      struct drm_i915_gem_mmap mmap_arg;
      void *map;

      DBG("brw_bo_map_cpu: %d (%s)\n", bo->gem_handle, bo->name);

      memclear(mmap_arg);
      mmap_arg.handle = bo->gem_handle;
      mmap_arg.size = bo->size;
      int ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg);
      if (ret != 0) {
         ret = -errno;
         DBG("%s:%d: Error mapping buffer %d (%s): %s .\n",
             __FILE__, __LINE__, bo->gem_handle, bo->name, strerror(errno));
         return NULL;
      }
      map = (void *) (uintptr_t) mmap_arg.addr_ptr;
      VG_DEFINED(map, bo->size);

      if (p_atomic_cmpxchg(&bo->map_cpu, NULL, map)) {
         VG_NOACCESS(map, bo->size);
         drm_munmap(map, bo->size);
      }
   }
   assert(bo->map_cpu);

   DBG("brw_bo_map_cpu: %d (%s) -> %p, ", bo->gem_handle, bo->name,
       bo->map_cpu);
   print_flags(flags);

   if (!(flags & MAP_ASYNC)) {
      bo_wait_with_stall_warning(brw, bo, "CPU mapping");
   }

   if (!bo->cache_coherent && !bo->bufmgr->has_llc) {
      /* If we're reusing an existing CPU mapping, the CPU caches may
       * contain stale data from the last time we read from that mapping.
       * (With the BO cache, it might even be data from a previous buffer!)
       * Even if it's a brand new mapping, the kernel may have zeroed the
       * buffer via CPU writes.
       *
       * We need to invalidate those cachelines so that we see the latest
       * contents, and so long as we only read from the CPU mmap we do not
       * need to write those cachelines back afterwards.
       *
       * On LLC, the emprical evidence suggests that writes from the GPU
       * that bypass the LLC (i.e. for scanout) do *invalidate* the CPU
       * cachelines. (Other reads, such as the display engine, bypass the
       * LLC entirely requiring us to keep dirty pixels for the scanout
       * out of any cache.)
       */
      gen_invalidate_range(bo->map_cpu, bo->size);
   }

   return bo->map_cpu;
}

static void *
brw_bo_map_wc(struct brw_context *brw, struct brw_bo *bo, unsigned flags)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   if (!bufmgr->has_mmap_wc)
      return NULL;

   if (!bo->map_wc) {
      struct drm_i915_gem_mmap mmap_arg;
      void *map;

      DBG("brw_bo_map_wc: %d (%s)\n", bo->gem_handle, bo->name);

      memclear(mmap_arg);
      mmap_arg.handle = bo->gem_handle;
      mmap_arg.size = bo->size;
      mmap_arg.flags = I915_MMAP_WC;
      int ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg);
      if (ret != 0) {
         ret = -errno;
         DBG("%s:%d: Error mapping buffer %d (%s): %s .\n",
             __FILE__, __LINE__, bo->gem_handle, bo->name, strerror(errno));
         return NULL;
      }

      map = (void *) (uintptr_t) mmap_arg.addr_ptr;
      VG_DEFINED(map, bo->size);

      if (p_atomic_cmpxchg(&bo->map_wc, NULL, map)) {
         VG_NOACCESS(map, bo->size);
         drm_munmap(map, bo->size);
      }
   }
   assert(bo->map_wc);

   DBG("brw_bo_map_wc: %d (%s) -> %p\n", bo->gem_handle, bo->name, bo->map_wc);
   print_flags(flags);

   if (!(flags & MAP_ASYNC)) {
      bo_wait_with_stall_warning(brw, bo, "WC mapping");
   }

   return bo->map_wc;
}

/**
 * Perform an uncached mapping via the GTT.
 *
 * Write access through the GTT is not quite fully coherent. On low power
 * systems especially, like modern Atoms, we can observe reads from RAM before
 * the write via GTT has landed. A write memory barrier that flushes the Write
 * Combining Buffer (i.e. sfence/mfence) is not sufficient to order the later
 * read after the write as the GTT write suffers a small delay through the GTT
 * indirection. The kernel uses an uncached mmio read to ensure the GTT write
 * is ordered with reads (either by the GPU, WB or WC) and unconditionally
 * flushes prior to execbuf submission. However, if we are not informing the
 * kernel about our GTT writes, it will not flush before earlier access, such
 * as when using the cmdparser. Similarly, we need to be careful if we should
 * ever issue a CPU read immediately following a GTT write.
 *
 * Telling the kernel about write access also has one more important
 * side-effect. Upon receiving notification about the write, it cancels any
 * scanout buffering for FBC/PSR and friends. Later FBC/PSR is then flushed by
 * either SW_FINISH or DIRTYFB. The presumption is that we never write to the
 * actual scanout via a mmaping, only to a backbuffer and so all the FBC/PSR
 * tracking is handled on the buffer exchange instead.
 */
static void *
brw_bo_map_gtt(struct brw_context *brw, struct brw_bo *bo, unsigned flags)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   /* Get a mapping of the buffer if we haven't before. */
   if (bo->map_gtt == NULL) {
      struct drm_i915_gem_mmap_gtt mmap_arg;
      void *map;

      DBG("bo_map_gtt: mmap %d (%s)\n", bo->gem_handle, bo->name);

      memclear(mmap_arg);
      mmap_arg.handle = bo->gem_handle;

      /* Get the fake offset back... */
      int ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg);
      if (ret != 0) {
         DBG("%s:%d: Error preparing buffer map %d (%s): %s .\n",
             __FILE__, __LINE__, bo->gem_handle, bo->name, strerror(errno));
         return NULL;
      }

      /* and mmap it. */
      map = drm_mmap(0, bo->size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, bufmgr->fd, mmap_arg.offset);
      if (map == MAP_FAILED) {
         DBG("%s:%d: Error mapping buffer %d (%s): %s .\n",
             __FILE__, __LINE__, bo->gem_handle, bo->name, strerror(errno));
         return NULL;
      }

      /* We don't need to use VALGRIND_MALLOCLIKE_BLOCK because Valgrind will
       * already intercept this mmap call. However, for consistency between
       * all the mmap paths, we mark the pointer as defined now and mark it
       * as inaccessible afterwards.
       */
      VG_DEFINED(map, bo->size);

      if (p_atomic_cmpxchg(&bo->map_gtt, NULL, map)) {
         VG_NOACCESS(map, bo->size);
         drm_munmap(map, bo->size);
      }
   }
   assert(bo->map_gtt);

   DBG("bo_map_gtt: %d (%s) -> %p, ", bo->gem_handle, bo->name, bo->map_gtt);
   print_flags(flags);

   if (!(flags & MAP_ASYNC)) {
      bo_wait_with_stall_warning(brw, bo, "GTT mapping");
   }

   return bo->map_gtt;
}

static bool
can_map_cpu(struct brw_bo *bo, unsigned flags)
{
   if (bo->cache_coherent)
      return true;

   /* Even if the buffer itself is not cache-coherent (such as a scanout), on
    * an LLC platform reads always are coherent (as they are performed via the
    * central system agent). It is just the writes that we need to take special
    * care to ensure that land in main memory and not stick in the CPU cache.
    */
   if (!(flags & MAP_WRITE) && bo->bufmgr->has_llc)
      return true;

   /* If PERSISTENT or COHERENT are set, the mmapping needs to remain valid
    * across batch flushes where the kernel will change cache domains of the
    * bo, invalidating continued access to the CPU mmap on non-LLC device.
    *
    * Similarly, ASYNC typically means that the buffer will be accessed via
    * both the CPU and the GPU simultaneously.  Batches may be executed that
    * use the BO even while it is mapped.  While OpenGL technically disallows
    * most drawing while non-persistent mappings are active, we may still use
    * the GPU for blits or other operations, causing batches to happen at
    * inconvenient times.
    */
   if (flags & (MAP_PERSISTENT | MAP_COHERENT | MAP_ASYNC))
      return false;

   return !(flags & MAP_WRITE);
}

void *
brw_bo_map(struct brw_context *brw, struct brw_bo *bo, unsigned flags)
{
   if (bo->tiling_mode != I915_TILING_NONE && !(flags & MAP_RAW))
      return brw_bo_map_gtt(brw, bo, flags);

   void *map;

   if (can_map_cpu(bo, flags))
      map = brw_bo_map_cpu(brw, bo, flags);
   else
      map = brw_bo_map_wc(brw, bo, flags);

   /* Allow the attempt to fail by falling back to the GTT where necessary.
    *
    * Not every buffer can be mmaped directly using the CPU (or WC), for
    * example buffers that wrap stolen memory or are imported from other
    * devices. For those, we have little choice but to use a GTT mmapping.
    * However, if we use a slow GTT mmapping for reads where we expected fast
    * access, that order of magnitude difference in throughput will be clearly
    * expressed by angry users.
    *
    * We skip MAP_RAW because we want to avoid map_gtt's fence detiling.
    */
   if (!map && !(flags & MAP_RAW)) {
      if (brw) {
         perf_debug("Fallback GTT mapping for %s with access flags %x\n",
                    bo->name, flags);
      }
      map = brw_bo_map_gtt(brw, bo, flags);
   }

   return map;
}

int
brw_bo_subdata(struct brw_bo *bo, uint64_t offset,
               uint64_t size, const void *data)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_pwrite pwrite;
   int ret;

   memclear(pwrite);
   pwrite.handle = bo->gem_handle;
   pwrite.offset = offset;
   pwrite.size = size;
   pwrite.data_ptr = (uint64_t) (uintptr_t) data;
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
   if (ret != 0) {
      ret = -errno;
      DBG("%s:%d: Error writing data to buffer %d: "
          "(%"PRIu64" %"PRIu64") %s .\n",
          __FILE__, __LINE__, bo->gem_handle, offset, size, strerror(errno));
   }

   return ret;
}

/** Waits for all GPU rendering with the object to have completed. */
void
brw_bo_wait_rendering(struct brw_bo *bo)
{
   /* We require a kernel recent enough for WAIT_IOCTL support.
    * See intel_init_bufmgr()
    */
   brw_bo_wait(bo, -1);
}

/**
 * Waits on a BO for the given amount of time.
 *
 * @bo: buffer object to wait for
 * @timeout_ns: amount of time to wait in nanoseconds.
 *   If value is less than 0, an infinite wait will occur.
 *
 * Returns 0 if the wait was successful ie. the last batch referencing the
 * object has completed within the allotted time. Otherwise some negative return
 * value describes the error. Of particular interest is -ETIME when the wait has
 * failed to yield the desired result.
 *
 * Similar to brw_bo_wait_rendering except a timeout parameter allows
 * the operation to give up after a certain amount of time. Another subtle
 * difference is the internal locking semantics are different (this variant does
 * not hold the lock for the duration of the wait). This makes the wait subject
 * to a larger userspace race window.
 *
 * The implementation shall wait until the object is no longer actively
 * referenced within a batch buffer at the time of the call. The wait will
 * not guarantee that the buffer is re-issued via another thread, or an flinked
 * handle. Userspace must make sure this race does not occur if such precision
 * is important.
 *
 * Note that some kernels have broken the inifite wait for negative values
 * promise, upgrade to latest stable kernels if this is the case.
 */
int
brw_bo_wait(struct brw_bo *bo, int64_t timeout_ns)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_wait wait;
   int ret;

   /* If we know it's idle, don't bother with the kernel round trip */
   if (bo->idle && !bo->external)
      return 0;

   memclear(wait);
   wait.bo_handle = bo->gem_handle;
   wait.timeout_ns = timeout_ns;
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_WAIT, &wait);
   if (ret == -1)
      return -errno;

   bo->idle = true;

   return ret;
}

void
brw_bufmgr_destroy(struct brw_bufmgr *bufmgr)
{
   mtx_destroy(&bufmgr->lock);

   /* Free any cached buffer objects we were going to reuse */
   for (int i = 0; i < bufmgr->num_buckets; i++) {
      struct bo_cache_bucket *bucket = &bufmgr->cache_bucket[i];

      list_for_each_entry_safe(struct brw_bo, bo, &bucket->head, head) {
         list_del(&bo->head);

         bo_free(bo);
      }
   }

   _mesa_hash_table_destroy(bufmgr->name_table, NULL);
   _mesa_hash_table_destroy(bufmgr->handle_table, NULL);

   free(bufmgr);
}

static int
bo_set_tiling_internal(struct brw_bo *bo, uint32_t tiling_mode,
                       uint32_t stride)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;
   struct drm_i915_gem_set_tiling set_tiling;
   int ret;

   if (bo->global_name == 0 &&
       tiling_mode == bo->tiling_mode && stride == bo->stride)
      return 0;

   memset(&set_tiling, 0, sizeof(set_tiling));
   do {
      /* set_tiling is slightly broken and overwrites the
       * input on the error path, so we have to open code
       * rmIoctl.
       */
      set_tiling.handle = bo->gem_handle;
      set_tiling.tiling_mode = tiling_mode;
      set_tiling.stride = stride;

      ret = ioctl(bufmgr->fd, DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling);
   } while (ret == -1 && (errno == EINTR || errno == EAGAIN));
   if (ret == -1)
      return -errno;

   bo->tiling_mode = set_tiling.tiling_mode;
   bo->swizzle_mode = set_tiling.swizzle_mode;
   bo->stride = set_tiling.stride;
   return 0;
}

int
brw_bo_get_tiling(struct brw_bo *bo, uint32_t *tiling_mode,
                  uint32_t *swizzle_mode)
{
   *tiling_mode = bo->tiling_mode;
   *swizzle_mode = bo->swizzle_mode;
   return 0;
}

static struct brw_bo *
brw_bo_gem_create_from_prime_internal(struct brw_bufmgr *bufmgr, int prime_fd,
                                      int tiling_mode, uint32_t stride)
{
   int ret;
   uint32_t handle;
   struct brw_bo *bo;

   mtx_lock(&bufmgr->lock);
   ret = drmPrimeFDToHandle(bufmgr->fd, prime_fd, &handle);
   if (ret) {
      DBG("create_from_prime: failed to obtain handle from fd: %s\n",
          strerror(errno));
      mtx_unlock(&bufmgr->lock);
      return NULL;
   }

   /*
    * See if the kernel has already returned this buffer to us. Just as
    * for named buffers, we must not create two bo's pointing at the same
    * kernel object
    */
   bo = hash_find_bo(bufmgr->handle_table, handle);
   if (bo) {
      brw_bo_reference(bo);
      goto out;
   }

   bo = calloc(1, sizeof(*bo));
   if (!bo)
      goto out;

   p_atomic_set(&bo->refcount, 1);

   /* Determine size of bo.  The fd-to-handle ioctl really should
    * return the size, but it doesn't.  If we have kernel 3.12 or
    * later, we can lseek on the prime fd to get the size.  Older
    * kernels will just fail, in which case we fall back to the
    * provided (estimated or guess size). */
   ret = lseek(prime_fd, 0, SEEK_END);
   if (ret != -1)
      bo->size = ret;

   bo->bufmgr = bufmgr;

   bo->gem_handle = handle;
   _mesa_hash_table_insert(bufmgr->handle_table, &bo->gem_handle, bo);

   bo->name = "prime";
   bo->reusable = false;
   bo->external = true;

   if (tiling_mode < 0) {
      struct drm_i915_gem_get_tiling get_tiling = { .handle = bo->gem_handle };
      if (drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_GET_TILING, &get_tiling))
         goto err;

      bo->tiling_mode = get_tiling.tiling_mode;
      bo->swizzle_mode = get_tiling.swizzle_mode;
      /* XXX stride is unknown */
   } else {
      bo_set_tiling_internal(bo, tiling_mode, stride);
   }

out:
   mtx_unlock(&bufmgr->lock);
   return bo;

err:
   bo_free(bo);
   mtx_unlock(&bufmgr->lock);
   return NULL;
}

struct brw_bo *
brw_bo_gem_create_from_prime(struct brw_bufmgr *bufmgr, int prime_fd)
{
   return brw_bo_gem_create_from_prime_internal(bufmgr, prime_fd, -1, 0);
}

struct brw_bo *
brw_bo_gem_create_from_prime_tiled(struct brw_bufmgr *bufmgr, int prime_fd,
                                   uint32_t tiling_mode, uint32_t stride)
{
   assert(tiling_mode == I915_TILING_NONE ||
          tiling_mode == I915_TILING_X ||
          tiling_mode == I915_TILING_Y);

   return brw_bo_gem_create_from_prime_internal(bufmgr, prime_fd,
                                                tiling_mode, stride);
}

static void
brw_bo_make_external(struct brw_bo *bo)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   if (!bo->external) {
      mtx_lock(&bufmgr->lock);
      if (!bo->external) {
         _mesa_hash_table_insert(bufmgr->handle_table, &bo->gem_handle, bo);
         bo->external = true;
      }
      mtx_unlock(&bufmgr->lock);
   }
}

int
brw_bo_gem_export_to_prime(struct brw_bo *bo, int *prime_fd)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   brw_bo_make_external(bo);

   if (drmPrimeHandleToFD(bufmgr->fd, bo->gem_handle,
                          DRM_CLOEXEC, prime_fd) != 0)
      return -errno;

   bo->reusable = false;

   return 0;
}

uint32_t
brw_bo_export_gem_handle(struct brw_bo *bo)
{
   brw_bo_make_external(bo);

   return bo->gem_handle;
}

int
brw_bo_flink(struct brw_bo *bo, uint32_t *name)
{
   struct brw_bufmgr *bufmgr = bo->bufmgr;

   if (!bo->global_name) {
      struct drm_gem_flink flink;

      memclear(flink);
      flink.handle = bo->gem_handle;
      if (drmIoctl(bufmgr->fd, DRM_IOCTL_GEM_FLINK, &flink))
         return -errno;

      brw_bo_make_external(bo);
      mtx_lock(&bufmgr->lock);
      if (!bo->global_name) {
         bo->global_name = flink.name;
         _mesa_hash_table_insert(bufmgr->name_table, &bo->global_name, bo);
      }
      mtx_unlock(&bufmgr->lock);

      bo->reusable = false;
   }

   *name = bo->global_name;
   return 0;
}

/**
 * Enables unlimited caching of buffer objects for reuse.
 *
 * This is potentially very memory expensive, as the cache at each bucket
 * size is only bounded by how many buffers of that size we've managed to have
 * in flight at once.
 */
void
brw_bufmgr_enable_reuse(struct brw_bufmgr *bufmgr)
{
   bufmgr->bo_reuse = true;
}

static void
add_bucket(struct brw_bufmgr *bufmgr, int size)
{
   unsigned int i = bufmgr->num_buckets;

   assert(i < ARRAY_SIZE(bufmgr->cache_bucket));

   list_inithead(&bufmgr->cache_bucket[i].head);
   bufmgr->cache_bucket[i].size = size;
   bufmgr->num_buckets++;
}

static void
init_cache_buckets(struct brw_bufmgr *bufmgr)
{
   uint64_t size, cache_max_size = 64 * 1024 * 1024;

   /* OK, so power of two buckets was too wasteful of memory.
    * Give 3 other sizes between each power of two, to hopefully
    * cover things accurately enough.  (The alternative is
    * probably to just go for exact matching of sizes, and assume
    * that for things like composited window resize the tiled
    * width/height alignment and rounding of sizes to pages will
    * get us useful cache hit rates anyway)
    */
   add_bucket(bufmgr, 4096);
   add_bucket(bufmgr, 4096 * 2);
   add_bucket(bufmgr, 4096 * 3);

   /* Initialize the linked lists for BO reuse cache. */
   for (size = 4 * 4096; size <= cache_max_size; size *= 2) {
      add_bucket(bufmgr, size);

      add_bucket(bufmgr, size + size * 1 / 4);
      add_bucket(bufmgr, size + size * 2 / 4);
      add_bucket(bufmgr, size + size * 3 / 4);
   }
}

uint32_t
brw_create_hw_context(struct brw_bufmgr *bufmgr)
{
   struct drm_i915_gem_context_create create;
   int ret;

   memclear(create);
   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, &create);
   if (ret != 0) {
      DBG("DRM_IOCTL_I915_GEM_CONTEXT_CREATE failed: %s\n", strerror(errno));
      return 0;
   }

   return create.ctx_id;
}

int
brw_hw_context_set_priority(struct brw_bufmgr *bufmgr,
                            uint32_t ctx_id,
                            int priority)
{
   struct drm_i915_gem_context_param p = {
      .ctx_id = ctx_id,
      .param = I915_CONTEXT_PARAM_PRIORITY,
      .value = priority,
   };
   int err;

   err = 0;
   if (drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM, &p))
      err = -errno;

   return err;
}

void
brw_destroy_hw_context(struct brw_bufmgr *bufmgr, uint32_t ctx_id)
{
   struct drm_i915_gem_context_destroy d = {.ctx_id = ctx_id };

   if (ctx_id != 0 &&
       drmIoctl(bufmgr->fd, DRM_IOCTL_I915_GEM_CONTEXT_DESTROY, &d) != 0) {
      fprintf(stderr, "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY failed: %s\n",
              strerror(errno));
   }
}

int
brw_reg_read(struct brw_bufmgr *bufmgr, uint32_t offset, uint64_t *result)
{
   struct drm_i915_reg_read reg_read;
   int ret;

   memclear(reg_read);
   reg_read.offset = offset;

   ret = drmIoctl(bufmgr->fd, DRM_IOCTL_I915_REG_READ, &reg_read);

   *result = reg_read.val;
   return ret;
}

static int
gem_param(int fd, int name)
{
   drm_i915_getparam_t gp;
   int v = -1; /* No param uses (yet) the sign bit, reserve it for errors */

   memset(&gp, 0, sizeof(gp));
   gp.param = name;
   gp.value = &v;
   if (drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp))
      return -1;

   return v;
}

/**
 * Initializes the GEM buffer manager, which uses the kernel to allocate, map,
 * and manage map buffer objections.
 *
 * \param fd File descriptor of the opened DRM device.
 */
struct brw_bufmgr *
brw_bufmgr_init(struct gen_device_info *devinfo, int fd)
{
   struct brw_bufmgr *bufmgr;

   bufmgr = calloc(1, sizeof(*bufmgr));
   if (bufmgr == NULL)
      return NULL;

   /* Handles to buffer objects belong to the device fd and are not
    * reference counted by the kernel.  If the same fd is used by
    * multiple parties (threads sharing the same screen bufmgr, or
    * even worse the same device fd passed to multiple libraries)
    * ownership of those handles is shared by those independent parties.
    *
    * Don't do this! Ensure that each library/bufmgr has its own device
    * fd so that its namespace does not clash with another.
    */
   bufmgr->fd = fd;

   if (mtx_init(&bufmgr->lock, mtx_plain) != 0) {
      free(bufmgr);
      return NULL;
   }

   bufmgr->has_llc = devinfo->has_llc;
   bufmgr->has_mmap_wc = gem_param(fd, I915_PARAM_MMAP_VERSION) > 0;

   init_cache_buckets(bufmgr);

   bufmgr->name_table =
      _mesa_hash_table_create(NULL, key_hash_uint, key_uint_equal);
   bufmgr->handle_table =
      _mesa_hash_table_create(NULL, key_hash_uint, key_uint_equal);

   return bufmgr;
}
