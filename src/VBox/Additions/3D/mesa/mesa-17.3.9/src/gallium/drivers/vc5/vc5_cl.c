/*
 * Copyright © 2014-2017 Broadcom
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

#include "util/u_math.h"
#include "util/ralloc.h"
#include "vc5_context.h"
#include "broadcom/cle/v3d_packet_v33_pack.h"

void
vc5_init_cl(struct vc5_job *job, struct vc5_cl *cl)
{
        cl->base = NULL;
        cl->next = cl->base;
        cl->size = 0;
        cl->job = job;
}

uint32_t
vc5_cl_ensure_space(struct vc5_cl *cl, uint32_t space, uint32_t alignment)
{
        uint32_t offset = align(cl_offset(cl), alignment);

        if (offset + space <= cl->size) {
                cl->next = cl->base + offset;
                return offset;
        }

        vc5_bo_unreference(&cl->bo);
        cl->bo = vc5_bo_alloc(cl->job->vc5->screen, align(space, 4096), "CL");
        cl->base = vc5_bo_map(cl->bo);
        cl->size = cl->bo->size;
        cl->next = cl->base;

        return 0;
}

void
vc5_cl_ensure_space_with_branch(struct vc5_cl *cl, uint32_t space)
{
        if (cl_offset(cl) + space + cl_packet_length(BRANCH) <= cl->size)
                return;

        struct vc5_bo *new_bo = vc5_bo_alloc(cl->job->vc5->screen, 4096, "CL");
        assert(space <= new_bo->size);

        /* Chain to the new BO from the old one. */
        if (cl->bo) {
                cl_emit(cl, BRANCH, branch) {
                        branch.address = cl_address(new_bo, 0);
                }
                vc5_bo_unreference(&cl->bo);
        } else {
                /* Root the first RCL/BCL BO in the job. */
                vc5_job_add_bo(cl->job, cl->bo);
        }

        cl->bo = new_bo;
        cl->base = vc5_bo_map(cl->bo);
        cl->size = cl->bo->size;
        cl->next = cl->base;
}

void
vc5_destroy_cl(struct vc5_cl *cl)
{
        vc5_bo_unreference(&cl->bo);
}
