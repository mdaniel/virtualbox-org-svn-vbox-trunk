/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef RADEON_WINSYS_H
#define RADEON_WINSYS_H

/* The public winsys interface header for the radeon driver. */

#include "pipebuffer/pb_buffer.h"

#include "amd/common/ac_gpu_info.h"
#include "amd/common/ac_surface.h"

#define RADEON_FLUSH_ASYNC		(1 << 0)
#define RADEON_FLUSH_END_OF_FRAME       (1 << 1)

/* Tiling flags. */
enum radeon_bo_layout {
    RADEON_LAYOUT_LINEAR = 0,
    RADEON_LAYOUT_TILED,
    RADEON_LAYOUT_SQUARETILED,

    RADEON_LAYOUT_UNKNOWN
};

enum radeon_bo_domain { /* bitfield */
    RADEON_DOMAIN_GTT  = 2,
    RADEON_DOMAIN_VRAM = 4,
    RADEON_DOMAIN_VRAM_GTT = RADEON_DOMAIN_VRAM | RADEON_DOMAIN_GTT
};

enum radeon_bo_flag { /* bitfield */
    RADEON_FLAG_GTT_WC =        (1 << 0),
    RADEON_FLAG_NO_CPU_ACCESS = (1 << 1),
    RADEON_FLAG_NO_SUBALLOC =   (1 << 2),
    RADEON_FLAG_SPARSE =        (1 << 3),
    RADEON_FLAG_NO_INTERPROCESS_SHARING = (1 << 4),
};

enum radeon_bo_usage { /* bitfield */
    RADEON_USAGE_READ = 2,
    RADEON_USAGE_WRITE = 4,
    RADEON_USAGE_READWRITE = RADEON_USAGE_READ | RADEON_USAGE_WRITE,

    /* The winsys ensures that the CS submission will be scheduled after
     * previously flushed CSs referencing this BO in a conflicting way.
     */
    RADEON_USAGE_SYNCHRONIZED = 8
};

#define RADEON_SPARSE_PAGE_SIZE (64 * 1024)

enum ring_type {
    RING_GFX = 0,
    RING_COMPUTE,
    RING_DMA,
    RING_UVD,
    RING_VCE,
    RING_UVD_ENC,
    RING_VCN_DEC,
    RING_LAST,
};

enum radeon_value_id {
    RADEON_REQUESTED_VRAM_MEMORY,
    RADEON_REQUESTED_GTT_MEMORY,
    RADEON_MAPPED_VRAM,
    RADEON_MAPPED_GTT,
    RADEON_BUFFER_WAIT_TIME_NS,
    RADEON_NUM_MAPPED_BUFFERS,
    RADEON_TIMESTAMP,
    RADEON_NUM_GFX_IBS,
    RADEON_NUM_SDMA_IBS,
    RADEON_GFX_BO_LIST_COUNTER, /* number of BOs submitted in gfx IBs */
    RADEON_GFX_IB_SIZE_COUNTER,
    RADEON_NUM_BYTES_MOVED,
    RADEON_NUM_EVICTIONS,
    RADEON_NUM_VRAM_CPU_PAGE_FAULTS,
    RADEON_VRAM_USAGE,
    RADEON_VRAM_VIS_USAGE,
    RADEON_GTT_USAGE,
    RADEON_GPU_TEMPERATURE, /* DRM 2.42.0 */
    RADEON_CURRENT_SCLK,
    RADEON_CURRENT_MCLK,
    RADEON_GPU_RESET_COUNTER, /* DRM 2.43.0 */
    RADEON_CS_THREAD_TIME,
};

/* Each group of four has the same priority. */
enum radeon_bo_priority {
    RADEON_PRIO_FENCE = 0,
    RADEON_PRIO_TRACE,
    RADEON_PRIO_SO_FILLED_SIZE,
    RADEON_PRIO_QUERY,

    RADEON_PRIO_IB1 = 4, /* main IB submitted to the kernel */
    RADEON_PRIO_IB2, /* IB executed with INDIRECT_BUFFER */
    RADEON_PRIO_DRAW_INDIRECT,
    RADEON_PRIO_INDEX_BUFFER,

    RADEON_PRIO_VCE = 8,
    RADEON_PRIO_UVD,
    RADEON_PRIO_SDMA_BUFFER,
    RADEON_PRIO_SDMA_TEXTURE,

    RADEON_PRIO_CP_DMA = 12,

    RADEON_PRIO_CONST_BUFFER = 16,
    RADEON_PRIO_DESCRIPTORS,
    RADEON_PRIO_BORDER_COLORS,

    RADEON_PRIO_SAMPLER_BUFFER = 20,
    RADEON_PRIO_VERTEX_BUFFER,

    RADEON_PRIO_SHADER_RW_BUFFER = 24,
    RADEON_PRIO_COMPUTE_GLOBAL,

    RADEON_PRIO_SAMPLER_TEXTURE = 28,
    RADEON_PRIO_SHADER_RW_IMAGE,

    RADEON_PRIO_SAMPLER_TEXTURE_MSAA = 32,

    RADEON_PRIO_COLOR_BUFFER = 36,

    RADEON_PRIO_DEPTH_BUFFER = 40,

    RADEON_PRIO_COLOR_BUFFER_MSAA = 44,

    RADEON_PRIO_DEPTH_BUFFER_MSAA = 48,

    RADEON_PRIO_CMASK = 52,
    RADEON_PRIO_DCC,
    RADEON_PRIO_HTILE,
    RADEON_PRIO_SHADER_BINARY, /* the hw can't hide instruction cache misses */

    RADEON_PRIO_SHADER_RINGS = 56,

    RADEON_PRIO_SCRATCH_BUFFER = 60,
    /* 63 is the maximum value */
};

struct winsys_handle;
struct radeon_winsys_ctx;

struct radeon_winsys_cs_chunk {
    unsigned cdw;  /* Number of used dwords. */
    unsigned max_dw; /* Maximum number of dwords. */
    uint32_t *buf; /* The base pointer of the chunk. */
};

struct radeon_winsys_cs {
    struct radeon_winsys_cs_chunk current;
    struct radeon_winsys_cs_chunk *prev;
    unsigned                      num_prev; /* Number of previous chunks. */
    unsigned                      max_prev; /* Space in array pointed to by prev. */
    unsigned                      prev_dw; /* Total number of dwords in previous chunks. */

    /* Memory usage of the buffer list. These are always 0 for preamble IBs. */
    uint64_t                      used_vram;
    uint64_t                      used_gart;
};

/* Tiling info for display code, DRI sharing, and other data. */
struct radeon_bo_metadata {
    /* Tiling flags describing the texture layout for display code
     * and DRI sharing.
     */
    union {
        struct {
            enum radeon_bo_layout   microtile;
            enum radeon_bo_layout   macrotile;
            unsigned                pipe_config;
            unsigned                bankw;
            unsigned                bankh;
            unsigned                tile_split;
            unsigned                mtilea;
            unsigned                num_banks;
            unsigned                stride;
            bool                    scanout;
        } legacy;

        struct {
            /* surface flags */
            unsigned swizzle_mode:5;
        } gfx9;
    } u;

    /* Additional metadata associated with the buffer, in bytes.
     * The maximum size is 64 * 4. This is opaque for the winsys & kernel.
     * Supported by amdgpu only.
     */
    uint32_t                size_metadata;
    uint32_t                metadata[64];
};

enum radeon_feature_id {
    RADEON_FID_R300_HYPERZ_ACCESS,     /* ZMask + HiZ */
    RADEON_FID_R300_CMASK_ACCESS,
};

struct radeon_bo_list_item {
    uint64_t bo_size;
    uint64_t vm_address;
    uint64_t priority_usage; /* mask of (1 << RADEON_PRIO_*) */
};

struct radeon_winsys {
    /**
     * The screen object this winsys was created for
     */
    struct pipe_screen *screen;

    /**
     * Decrement the winsys reference count.
     *
     * \param ws  The winsys this function is called for.
     * \return    True if the winsys and screen should be destroyed.
     */
    bool (*unref)(struct radeon_winsys *ws);

    /**
     * Destroy this winsys.
     *
     * \param ws        The winsys this function is called from.
     */
    void (*destroy)(struct radeon_winsys *ws);

    /**
     * Query an info structure from winsys.
     *
     * \param ws        The winsys this function is called from.
     * \param info      Return structure
     */
    void (*query_info)(struct radeon_winsys *ws,
                       struct radeon_info *info);

    /**************************************************************************
     * Buffer management. Buffer attributes are mostly fixed over its lifetime.
     *
     * Remember that gallium gets to choose the interface it needs, and the
     * window systems must then implement that interface (rather than the
     * other way around...).
     *************************************************************************/

    /**
     * Create a buffer object.
     *
     * \param ws        The winsys this function is called from.
     * \param size      The size to allocate.
     * \param alignment An alignment of the buffer in memory.
     * \param use_reusable_pool Whether the cache buffer manager should be used.
     * \param domain    A bitmask of the RADEON_DOMAIN_* flags.
     * \return          The created buffer object.
     */
    struct pb_buffer *(*buffer_create)(struct radeon_winsys *ws,
                                       uint64_t size,
                                       unsigned alignment,
                                       enum radeon_bo_domain domain,
                                       enum radeon_bo_flag flags);

    /**
     * Map the entire data store of a buffer object into the client's address
     * space.
     *
     * \param buf       A winsys buffer object to map.
     * \param cs        A command stream to flush if the buffer is referenced by it.
     * \param usage     A bitmask of the PIPE_TRANSFER_* flags.
     * \return          The pointer at the beginning of the buffer.
     */
    void *(*buffer_map)(struct pb_buffer *buf,
                        struct radeon_winsys_cs *cs,
                        enum pipe_transfer_usage usage);

    /**
     * Unmap a buffer object from the client's address space.
     *
     * \param buf       A winsys buffer object to unmap.
     */
    void (*buffer_unmap)(struct pb_buffer *buf);

    /**
     * Wait for the buffer and return true if the buffer is not used
     * by the device.
     *
     * The timeout of 0 will only return the status.
     * The timeout of PIPE_TIMEOUT_INFINITE will always wait until the buffer
     * is idle.
     */
    bool (*buffer_wait)(struct pb_buffer *buf, uint64_t timeout,
                        enum radeon_bo_usage usage);

    /**
     * Return buffer metadata.
     * (tiling info for display code, DRI sharing, and other data)
     *
     * \param buf       A winsys buffer object to get the flags from.
     * \param md        Metadata
     */
    void (*buffer_get_metadata)(struct pb_buffer *buf,
                                struct radeon_bo_metadata *md);

    /**
     * Set buffer metadata.
     * (tiling info for display code, DRI sharing, and other data)
     *
     * \param buf       A winsys buffer object to set the flags for.
     * \param md        Metadata
     */
    void (*buffer_set_metadata)(struct pb_buffer *buf,
                                struct radeon_bo_metadata *md);

    /**
     * Get a winsys buffer from a winsys handle. The internal structure
     * of the handle is platform-specific and only a winsys should access it.
     *
     * \param ws        The winsys this function is called from.
     * \param whandle   A winsys handle pointer as was received from a state
     *                  tracker.
     * \param stride    The returned buffer stride in bytes.
     */
    struct pb_buffer *(*buffer_from_handle)(struct radeon_winsys *ws,
                                            struct winsys_handle *whandle,
                                            unsigned *stride, unsigned *offset);

    /**
     * Get a winsys buffer from a user pointer. The resulting buffer can't
     * be exported. Both pointer and size must be page aligned.
     *
     * \param ws        The winsys this function is called from.
     * \param pointer   User pointer to turn into a buffer object.
     * \param Size      Size in bytes for the new buffer.
     */
    struct pb_buffer *(*buffer_from_ptr)(struct radeon_winsys *ws,
                                         void *pointer, uint64_t size);

    /**
     * Whether the buffer was created from a user pointer.
     *
     * \param buf       A winsys buffer object
     * \return          whether \p buf was created via buffer_from_ptr
     */
    bool (*buffer_is_user_ptr)(struct pb_buffer *buf);

    /** Whether the buffer was suballocated. */
    bool (*buffer_is_suballocated)(struct pb_buffer *buf);

    /**
     * Get a winsys handle from a winsys buffer. The internal structure
     * of the handle is platform-specific and only a winsys should access it.
     *
     * \param buf       A winsys buffer object to get the handle from.
     * \param whandle   A winsys handle pointer.
     * \param stride    A stride of the buffer in bytes, for texturing.
     * \return          true on success.
     */
    bool (*buffer_get_handle)(struct pb_buffer *buf,
                              unsigned stride, unsigned offset,
                              unsigned slice_size,
                              struct winsys_handle *whandle);

    /**
     * Change the commitment of a (64KB-page aligned) region of the given
     * sparse buffer.
     *
     * \warning There is no automatic synchronization with command submission.
     *
     * \note Only implemented by the amdgpu winsys.
     *
     * \return false on out of memory or other failure, true on success.
     */
    bool (*buffer_commit)(struct pb_buffer *buf,
                          uint64_t offset, uint64_t size,
                          bool commit);

    /**
     * Return the virtual address of a buffer.
     *
     * When virtual memory is not in use, this is the offset relative to the
     * relocation base (non-zero for sub-allocated buffers).
     *
     * \param buf       A winsys buffer object
     * \return          virtual address
     */
    uint64_t (*buffer_get_virtual_address)(struct pb_buffer *buf);

    /**
     * Return the offset of this buffer relative to the relocation base.
     * This is only non-zero for sub-allocated buffers.
     *
     * This is only supported in the radeon winsys, since amdgpu uses virtual
     * addresses in submissions even for the video engines.
     *
     * \param buf      A winsys buffer object
     * \return         the offset for relocations
     */
    unsigned (*buffer_get_reloc_offset)(struct pb_buffer *buf);

    /**
     * Query the initial placement of the buffer from the kernel driver.
     */
    enum radeon_bo_domain (*buffer_get_initial_domain)(struct pb_buffer *buf);

    /**************************************************************************
     * Command submission.
     *
     * Each pipe context should create its own command stream and submit
     * commands independently of other contexts.
     *************************************************************************/

    /**
     * Create a command submission context.
     * Various command streams can be submitted to the same context.
     */
    struct radeon_winsys_ctx *(*ctx_create)(struct radeon_winsys *ws);

    /**
     * Destroy a context.
     */
    void (*ctx_destroy)(struct radeon_winsys_ctx *ctx);

    /**
     * Query a GPU reset status.
     */
    enum pipe_reset_status (*ctx_query_reset_status)(struct radeon_winsys_ctx *ctx);

    /**
     * Create a command stream.
     *
     * \param ctx       The submission context
     * \param ring_type The ring type (GFX, DMA, UVD)
     * \param flush     Flush callback function associated with the command stream.
     * \param user      User pointer that will be passed to the flush callback.
     */
    struct radeon_winsys_cs *(*cs_create)(struct radeon_winsys_ctx *ctx,
                                          enum ring_type ring_type,
                                          void (*flush)(void *ctx, unsigned flags,
							struct pipe_fence_handle **fence),
                                          void *flush_ctx);

    /**
     * Destroy a command stream.
     *
     * \param cs        A command stream to destroy.
     */
    void (*cs_destroy)(struct radeon_winsys_cs *cs);

    /**
     * Add a buffer. Each buffer used by a CS must be added using this function.
     *
     * \param cs      Command stream
     * \param buf     Buffer
     * \param usage   Whether the buffer is used for read and/or write.
     * \param domain  Bitmask of the RADEON_DOMAIN_* flags.
     * \param priority  A higher number means a greater chance of being
     *                  placed in the requested domain. 15 is the maximum.
     * \return Buffer index.
     */
    unsigned (*cs_add_buffer)(struct radeon_winsys_cs *cs,
                             struct pb_buffer *buf,
                             enum radeon_bo_usage usage,
                             enum radeon_bo_domain domain,
                             enum radeon_bo_priority priority);

    /**
     * Return the index of an already-added buffer.
     *
     * Not supported on amdgpu. Drivers with GPUVM should not care about
     * buffer indices.
     *
     * \param cs        Command stream
     * \param buf       Buffer
     * \return          The buffer index, or -1 if the buffer has not been added.
     */
    int (*cs_lookup_buffer)(struct radeon_winsys_cs *cs,
                            struct pb_buffer *buf);

    /**
     * Return true if there is enough memory in VRAM and GTT for the buffers
     * added so far. If the validation fails, all buffers which have
     * been added since the last call of cs_validate will be removed and
     * the CS will be flushed (provided there are still any buffers).
     *
     * \param cs        A command stream to validate.
     */
    bool (*cs_validate)(struct radeon_winsys_cs *cs);

    /**
     * Check whether the given number of dwords is available in the IB.
     * Optionally chain a new chunk of the IB if necessary and supported.
     *
     * \param cs        A command stream.
     * \param dw        Number of CS dwords requested by the caller.
     */
    bool (*cs_check_space)(struct radeon_winsys_cs *cs, unsigned dw);

    /**
     * Return the buffer list.
     *
     * This is the buffer list as passed to the kernel, i.e. it only contains
     * the parent buffers of sub-allocated buffers.
     *
     * \param cs    Command stream
     * \param list  Returned buffer list. Set to NULL to query the count only.
     * \return      The buffer count.
     */
    unsigned (*cs_get_buffer_list)(struct radeon_winsys_cs *cs,
                                   struct radeon_bo_list_item *list);

    /**
     * Flush a command stream.
     *
     * \param cs          A command stream to flush.
     * \param flags,      RADEON_FLUSH_ASYNC or 0.
     * \param fence       Pointer to a fence. If non-NULL, a fence is inserted
     *                    after the CS and is returned through this parameter.
     * \return Negative POSIX error code or 0 for success.
     *         Asynchronous submissions never return an error.
     */
    int (*cs_flush)(struct radeon_winsys_cs *cs,
                    unsigned flags,
                    struct pipe_fence_handle **fence);

    /**
     * Create a fence before the CS is flushed.
     * The user must flush manually to complete the initializaton of the fence.
     * The fence must not be used before the flush.
     */
    struct pipe_fence_handle *(*cs_get_next_fence)(struct radeon_winsys_cs *cs);

    /**
     * Return true if a buffer is referenced by a command stream.
     *
     * \param cs        A command stream.
     * \param buf       A winsys buffer.
     */
    bool (*cs_is_buffer_referenced)(struct radeon_winsys_cs *cs,
                                    struct pb_buffer *buf,
                                    enum radeon_bo_usage usage);

    /**
     * Request access to a feature for a command stream.
     *
     * \param cs        A command stream.
     * \param fid       Feature ID, one of RADEON_FID_*
     * \param enable    Whether to enable or disable the feature.
     */
    bool (*cs_request_feature)(struct radeon_winsys_cs *cs,
                               enum radeon_feature_id fid,
                               bool enable);
     /**
      * Make sure all asynchronous flush of the cs have completed
      *
      * \param cs        A command stream.
      */
    void (*cs_sync_flush)(struct radeon_winsys_cs *cs);

    /**
     * Add a fence dependency to the CS, so that the CS will wait for
     * the fence before execution.
     */
    void (*cs_add_fence_dependency)(struct radeon_winsys_cs *cs,
                                    struct pipe_fence_handle *fence);

    /**
     * Wait for the fence and return true if the fence has been signalled.
     * The timeout of 0 will only return the status.
     * The timeout of PIPE_TIMEOUT_INFINITE will always wait until the fence
     * is signalled.
     */
    bool (*fence_wait)(struct radeon_winsys *ws,
                       struct pipe_fence_handle *fence,
                       uint64_t timeout);

    /**
     * Reference counting for fences.
     */
    void (*fence_reference)(struct pipe_fence_handle **dst,
                            struct pipe_fence_handle *src);

    /**
     * Create a new fence object corresponding to the given sync_file.
     */
    struct pipe_fence_handle *(*fence_import_sync_file)(struct radeon_winsys *ws,
							int fd);

    /**
     * Return a sync_file FD corresponding to the given fence object.
     */
    int (*fence_export_sync_file)(struct radeon_winsys *ws,
				  struct pipe_fence_handle *fence);

    /**
     * Return a sync file FD that is already signalled.
     */
    int (*export_signalled_sync_file)(struct radeon_winsys *ws);

    /**
     * Initialize surface
     *
     * \param ws        The winsys this function is called from.
     * \param tex       Input texture description
     * \param flags     Bitmask of RADEON_SURF_* flags
     * \param bpe       Bytes per pixel, it can be different for Z buffers.
     * \param mode      Preferred tile mode. (linear, 1D, or 2D)
     * \param surf      Output structure
     */
    int (*surface_init)(struct radeon_winsys *ws,
                        const struct pipe_resource *tex,
                        unsigned flags, unsigned bpe,
                        enum radeon_surf_mode mode,
                        struct radeon_surf *surf);

    uint64_t (*query_value)(struct radeon_winsys *ws,
                            enum radeon_value_id value);

    bool (*read_registers)(struct radeon_winsys *ws, unsigned reg_offset,
                           unsigned num_registers, uint32_t *out);

    const char* (*get_chip_name)(struct radeon_winsys *ws);
};

static inline bool radeon_emitted(struct radeon_winsys_cs *cs, unsigned num_dw)
{
    return cs && (cs->prev_dw + cs->current.cdw > num_dw);
}

static inline void radeon_emit(struct radeon_winsys_cs *cs, uint32_t value)
{
    cs->current.buf[cs->current.cdw++] = value;
}

static inline void radeon_emit_array(struct radeon_winsys_cs *cs,
				     const uint32_t *values, unsigned count)
{
    memcpy(cs->current.buf + cs->current.cdw, values, count * 4);
    cs->current.cdw += count;
}

enum radeon_heap {
    RADEON_HEAP_VRAM_NO_CPU_ACCESS,
    RADEON_HEAP_VRAM,
    RADEON_HEAP_VRAM_GTT, /* combined heaps */
    RADEON_HEAP_GTT_WC,
    RADEON_HEAP_GTT,
    RADEON_MAX_SLAB_HEAPS,
    RADEON_MAX_CACHED_HEAPS = RADEON_MAX_SLAB_HEAPS,
};

static inline enum radeon_bo_domain radeon_domain_from_heap(enum radeon_heap heap)
{
    switch (heap) {
    case RADEON_HEAP_VRAM_NO_CPU_ACCESS:
    case RADEON_HEAP_VRAM:
        return RADEON_DOMAIN_VRAM;
    case RADEON_HEAP_VRAM_GTT:
        return RADEON_DOMAIN_VRAM_GTT;
    case RADEON_HEAP_GTT_WC:
    case RADEON_HEAP_GTT:
        return RADEON_DOMAIN_GTT;
    default:
        assert(0);
        return (enum radeon_bo_domain)0;
    }
}

static inline unsigned radeon_flags_from_heap(enum radeon_heap heap)
{
    switch (heap) {
    case RADEON_HEAP_VRAM_NO_CPU_ACCESS:
        return RADEON_FLAG_GTT_WC |
               RADEON_FLAG_NO_CPU_ACCESS |
               RADEON_FLAG_NO_INTERPROCESS_SHARING;

    case RADEON_HEAP_VRAM:
    case RADEON_HEAP_VRAM_GTT:
    case RADEON_HEAP_GTT_WC:
        return RADEON_FLAG_GTT_WC |
               RADEON_FLAG_NO_INTERPROCESS_SHARING;

    case RADEON_HEAP_GTT:
    default:
        return RADEON_FLAG_NO_INTERPROCESS_SHARING;
    }
}

/* The pb cache bucket is chosen to minimize pb_cache misses.
 * It must be between 0 and 3 inclusive.
 */
static inline unsigned radeon_get_pb_cache_bucket_index(enum radeon_heap heap)
{
    switch (heap) {
    case RADEON_HEAP_VRAM_NO_CPU_ACCESS:
        return 0;
    case RADEON_HEAP_VRAM:
    case RADEON_HEAP_VRAM_GTT:
        return 1;
    case RADEON_HEAP_GTT_WC:
        return 2;
    case RADEON_HEAP_GTT:
    default:
        return 3;
    }
}

/* Return the heap index for winsys allocators, or -1 on failure. */
static inline int radeon_get_heap_index(enum radeon_bo_domain domain,
                                        enum radeon_bo_flag flags)
{
    /* VRAM implies WC (write combining) */
    assert(!(domain & RADEON_DOMAIN_VRAM) || flags & RADEON_FLAG_GTT_WC);
    /* NO_CPU_ACCESS implies VRAM only. */
    assert(!(flags & RADEON_FLAG_NO_CPU_ACCESS) || domain == RADEON_DOMAIN_VRAM);

    /* Resources with interprocess sharing don't use any winsys allocators. */
    if (!(flags & RADEON_FLAG_NO_INTERPROCESS_SHARING))
        return -1;

    /* Unsupported flags: NO_SUBALLOC, SPARSE. */
    if (flags & ~(RADEON_FLAG_GTT_WC |
                  RADEON_FLAG_NO_CPU_ACCESS |
                  RADEON_FLAG_NO_INTERPROCESS_SHARING))
        return -1;

    switch (domain) {
    case RADEON_DOMAIN_VRAM:
        if (flags & RADEON_FLAG_NO_CPU_ACCESS)
            return RADEON_HEAP_VRAM_NO_CPU_ACCESS;
        else
            return RADEON_HEAP_VRAM;
    case RADEON_DOMAIN_VRAM_GTT:
        return RADEON_HEAP_VRAM_GTT;
    case RADEON_DOMAIN_GTT:
        if (flags & RADEON_FLAG_GTT_WC)
            return RADEON_HEAP_GTT_WC;
        else
            return RADEON_HEAP_GTT;
    }
    return -1;
}

#endif
