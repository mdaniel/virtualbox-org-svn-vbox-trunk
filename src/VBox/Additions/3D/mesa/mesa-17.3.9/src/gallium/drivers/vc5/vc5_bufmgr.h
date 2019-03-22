/*
 * Copyright © 2014 Broadcom
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

#ifndef VC5_BUFMGR_H
#define VC5_BUFMGR_H

#include <stdint.h>
#include "util/u_hash_table.h"
#include "util/u_inlines.h"
#include "util/list.h"
#include "vc5_screen.h"

struct vc5_context;

struct vc5_bo {
        struct pipe_reference reference;
        struct vc5_screen *screen;
        void *map;
        const char *name;
        uint32_t handle;
        uint32_t size;

        /* Address of the BO in our page tables. */
        uint32_t offset;

        /** Entry in the linked list of buffers freed, by age. */
        struct list_head time_list;
        /** Entry in the per-page-count linked list of buffers freed (by age). */
        struct list_head size_list;
        /** Approximate second when the bo was freed. */
        time_t free_time;
        /**
         * Whether only our process has a reference to the BO (meaning that
         * it's safe to reuse it in the BO cache).
         */
        bool private;
};

struct vc5_bo *vc5_bo_alloc(struct vc5_screen *screen, uint32_t size,
                            const char *name);
void vc5_bo_last_unreference(struct vc5_bo *bo);
void vc5_bo_last_unreference_locked_timed(struct vc5_bo *bo, time_t time);
struct vc5_bo *vc5_bo_open_name(struct vc5_screen *screen, uint32_t name,
                                uint32_t winsys_stride);
struct vc5_bo *vc5_bo_open_dmabuf(struct vc5_screen *screen, int fd,
                                  uint32_t winsys_stride);
bool vc5_bo_flink(struct vc5_bo *bo, uint32_t *name);
int vc5_bo_get_dmabuf(struct vc5_bo *bo);

static inline void
vc5_bo_set_reference(struct vc5_bo **old_bo, struct vc5_bo *new_bo)
{
        if (pipe_reference(&(*old_bo)->reference, &new_bo->reference))
                vc5_bo_last_unreference(*old_bo);
        *old_bo = new_bo;
}

static inline struct vc5_bo *
vc5_bo_reference(struct vc5_bo *bo)
{
        pipe_reference(NULL, &bo->reference);
        return bo;
}

static inline void
vc5_bo_unreference(struct vc5_bo **bo)
{
        struct vc5_screen *screen;
        if (!*bo)
                return;

        if ((*bo)->private) {
                /* Avoid the mutex for private BOs */
                if (pipe_reference(&(*bo)->reference, NULL))
                        vc5_bo_last_unreference(*bo);
        } else {
                screen = (*bo)->screen;
                mtx_lock(&screen->bo_handles_mutex);

                if (pipe_reference(&(*bo)->reference, NULL)) {
                        util_hash_table_remove(screen->bo_handles,
                                               (void *)(uintptr_t)(*bo)->handle);
                        vc5_bo_last_unreference(*bo);
                }

                mtx_unlock(&screen->bo_handles_mutex);
        }

        *bo = NULL;
}

static inline void
vc5_bo_unreference_locked_timed(struct vc5_bo **bo, time_t time)
{
        if (!*bo)
                return;

        if (pipe_reference(&(*bo)->reference, NULL))
                vc5_bo_last_unreference_locked_timed(*bo, time);
        *bo = NULL;
}

void *
vc5_bo_map(struct vc5_bo *bo);

void *
vc5_bo_map_unsynchronized(struct vc5_bo *bo);

bool
vc5_bo_wait(struct vc5_bo *bo, uint64_t timeout_ns, const char *reason);

bool
vc5_wait_seqno(struct vc5_screen *screen, uint64_t seqno, uint64_t timeout_ns,
               const char *reason);

void
vc5_bufmgr_destroy(struct pipe_screen *pscreen);

#endif /* VC5_BUFMGR_H */

