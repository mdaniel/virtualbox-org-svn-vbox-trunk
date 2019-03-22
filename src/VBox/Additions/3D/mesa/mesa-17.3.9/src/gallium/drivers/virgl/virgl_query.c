/*
 * Copyright 2014, 2015 Red Hat.
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
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "virgl_context.h"
#include "virgl_encode.h"
#include "virgl_protocol.h"
#include "virgl_resource.h"

struct virgl_query {
   uint32_t handle;
   struct virgl_resource *buf;

   unsigned index;
   unsigned type;
   unsigned result_size;
   unsigned result_gotten_sent;
};
#define VIRGL_QUERY_OCCLUSION_COUNTER     0
#define VIRGL_QUERY_OCCLUSION_PREDICATE   1
#define VIRGL_QUERY_TIMESTAMP             2
#define VIRGL_QUERY_TIMESTAMP_DISJOINT    3
#define VIRGL_QUERY_TIME_ELAPSED          4
#define VIRGL_QUERY_PRIMITIVES_GENERATED  5
#define VIRGL_QUERY_PRIMITIVES_EMITTED    6
#define VIRGL_QUERY_SO_STATISTICS         7
#define VIRGL_QUERY_SO_OVERFLOW_PREDICATE 8
#define VIRGL_QUERY_GPU_FINISHED          9
#define VIRGL_QUERY_PIPELINE_STATISTICS  10

static const int pquery_map[] =
{
   VIRGL_QUERY_OCCLUSION_COUNTER,
   VIRGL_QUERY_OCCLUSION_PREDICATE,
   -1,
   VIRGL_QUERY_TIMESTAMP,
   VIRGL_QUERY_TIMESTAMP_DISJOINT,
   VIRGL_QUERY_TIME_ELAPSED,
   VIRGL_QUERY_PRIMITIVES_GENERATED,
   VIRGL_QUERY_PRIMITIVES_EMITTED,
   VIRGL_QUERY_SO_STATISTICS,
   VIRGL_QUERY_SO_OVERFLOW_PREDICATE,
   -1,
   VIRGL_QUERY_GPU_FINISHED,
   VIRGL_QUERY_PIPELINE_STATISTICS,
};

static int pipe_to_virgl_query(enum pipe_query_type ptype)
{
   return pquery_map[ptype];
}

static inline struct virgl_query *virgl_query(struct pipe_query *q)
{
   return (struct virgl_query *)q;
}

static void virgl_render_condition(struct pipe_context *ctx,
                                  struct pipe_query *q,
                                  boolean condition,
                                  enum pipe_render_cond_flag mode)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query = virgl_query(q);
   uint32_t handle = 0;
   if (q)
      handle = query->handle;
   virgl_encoder_render_condition(vctx, handle, condition, mode);
}

static struct pipe_query *virgl_create_query(struct pipe_context *ctx,
                                            unsigned query_type, unsigned index)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query;
   uint32_t handle;

   query = CALLOC_STRUCT(virgl_query);
   if (!query)
      return NULL;

   query->buf = (struct virgl_resource *)pipe_buffer_create(ctx->screen, PIPE_BIND_CUSTOM,
                                                           PIPE_USAGE_STAGING, sizeof(struct virgl_host_query_state));
   if (!query->buf) {
      FREE(query);
      return NULL;
   }

   handle = virgl_object_assign_handle();
   query->type = pipe_to_virgl_query(query_type);
   query->index = index;
   query->handle = handle;
   query->buf->clean = FALSE;
   virgl_encoder_create_query(vctx, handle, query->type, index, query->buf, 0);

   return (struct pipe_query *)query;
}

static void virgl_destroy_query(struct pipe_context *ctx,
                        struct pipe_query *q)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query = virgl_query(q);

   virgl_encode_delete_object(vctx, query->handle, VIRGL_OBJECT_QUERY);

   pipe_resource_reference((struct pipe_resource **)&query->buf, NULL);
   FREE(query);
}

static boolean virgl_begin_query(struct pipe_context *ctx,
                             struct pipe_query *q)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query = virgl_query(q);

   query->buf->clean = FALSE;
   virgl_encoder_begin_query(vctx, query->handle);
   return true;
}

static bool virgl_end_query(struct pipe_context *ctx,
                           struct pipe_query *q)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query = virgl_query(q);
   struct pipe_box box;

   uint32_t qs = VIRGL_QUERY_STATE_WAIT_HOST;
   u_box_1d(0, 4, &box);
   virgl_transfer_inline_write(ctx, &query->buf->u.b, 0, PIPE_TRANSFER_WRITE,
                              &box, &qs, 0, 0);


   virgl_encoder_end_query(vctx, query->handle);
   return true;
}

static boolean virgl_get_query_result(struct pipe_context *ctx,
                                     struct pipe_query *q,
                                     boolean wait,
                                     union pipe_query_result *result)
{
   struct virgl_context *vctx = virgl_context(ctx);
   struct virgl_query *query = virgl_query(q);
   struct pipe_transfer *transfer;
   struct virgl_host_query_state *host_state;

   /* ask host for query result */
   if (!query->result_gotten_sent) {
      query->result_gotten_sent = 1;
      virgl_encoder_get_query_result(vctx, query->handle, 0);
      ctx->flush(ctx, NULL, 0);
   }

   /* do we  have to flush? */
   /* now we can do the transfer to get the result back? */
 remap:
   host_state = pipe_buffer_map(ctx, &query->buf->u.b,
                               PIPE_TRANSFER_READ, &transfer);

   if (host_state->query_state != VIRGL_QUERY_STATE_DONE) {
      pipe_buffer_unmap(ctx, transfer);
      if (wait)
         goto remap;
      else
         return FALSE;
   }

   if (query->type == PIPE_QUERY_TIMESTAMP || query->type == PIPE_QUERY_TIME_ELAPSED)
      result->u64 = host_state->result;
   else
      result->u64 = (uint32_t)host_state->result;

   pipe_buffer_unmap(ctx, transfer);
   query->result_gotten_sent = 0;
   return TRUE;
}

static void
virgl_set_active_query_state(struct pipe_context *pipe, boolean enable)
{
}

void virgl_init_query_functions(struct virgl_context *vctx)
{
   vctx->base.render_condition = virgl_render_condition;
   vctx->base.create_query = virgl_create_query;
   vctx->base.destroy_query = virgl_destroy_query;
   vctx->base.begin_query = virgl_begin_query;
   vctx->base.end_query = virgl_end_query;
   vctx->base.get_query_result = virgl_get_query_result;
   vctx->base.set_active_query_state = virgl_set_active_query_state;
}
