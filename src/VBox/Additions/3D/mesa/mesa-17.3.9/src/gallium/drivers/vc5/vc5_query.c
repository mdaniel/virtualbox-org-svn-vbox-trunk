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

/**
 * Stub support for occlusion queries.
 *
 * Since we expose support for GL 2.0, we have to expose occlusion queries,
 * but the spec allows you to expose 0 query counter bits, so we just return 0
 * as the result of all our queries.
 */
#include "vc5_context.h"

struct vc5_query
{
        uint8_t pad;
};

static struct pipe_query *
vc5_create_query(struct pipe_context *ctx, unsigned query_type, unsigned index)
{
        struct vc5_query *query = calloc(1, sizeof(*query));

        /* Note that struct pipe_query isn't actually defined anywhere. */
        return (struct pipe_query *)query;
}

static void
vc5_destroy_query(struct pipe_context *ctx, struct pipe_query *query)
{
        free(query);
}

static boolean
vc5_begin_query(struct pipe_context *ctx, struct pipe_query *query)
{
        return true;
}

static bool
vc5_end_query(struct pipe_context *ctx, struct pipe_query *query)
{
        return true;
}

static boolean
vc5_get_query_result(struct pipe_context *ctx, struct pipe_query *query,
                     boolean wait, union pipe_query_result *vresult)
{
        uint64_t *result = &vresult->u64;

        *result = 0;

        return true;
}

static void
vc5_set_active_query_state(struct pipe_context *pipe, boolean enable)
{
}

void
vc5_query_init(struct pipe_context *pctx)
{
        pctx->create_query = vc5_create_query;
        pctx->destroy_query = vc5_destroy_query;
        pctx->begin_query = vc5_begin_query;
        pctx->end_query = vc5_end_query;
        pctx->get_query_result = vc5_get_query_result;
        pctx->set_active_query_state = vc5_set_active_query_state;
}

