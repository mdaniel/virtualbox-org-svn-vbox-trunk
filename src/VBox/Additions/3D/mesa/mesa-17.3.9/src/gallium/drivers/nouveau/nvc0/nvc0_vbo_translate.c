
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_format.h"
#include "translate/translate.h"

#include "nvc0/nvc0_context.h"
#include "nvc0/nvc0_resource.h"

#include "nvc0/nvc0_3d.xml.h"

struct push_context {
   struct nouveau_pushbuf *push;

   struct translate *translate;
   void *dest;
   const void *idxbuf;

   uint32_t vertex_size;
   uint32_t restart_index;
   uint32_t start_instance;
   uint32_t instance_id;

   bool prim_restart;
   bool need_vertex_id;

   struct {
      bool enabled;
      bool value;
      uint8_t width;
      unsigned stride;
      const uint8_t *data;
   } edgeflag;
};

static void nvc0_push_upload_vertex_ids(struct push_context *,
                                        struct nvc0_context *,
                                        const struct pipe_draw_info *);

static void
nvc0_push_context_init(struct nvc0_context *nvc0, struct push_context *ctx)
{
   ctx->push = nvc0->base.pushbuf;

   ctx->translate = nvc0->vertex->translate;
   ctx->vertex_size = nvc0->vertex->size;
   ctx->instance_id = 0;

   ctx->need_vertex_id =
      nvc0->vertprog->vp.need_vertex_id && (nvc0->vertex->num_elements < 32);

   ctx->edgeflag.value = true;
   ctx->edgeflag.enabled = nvc0->vertprog->vp.edgeflag < PIPE_MAX_ATTRIBS;

   /* silence warnings */
   ctx->edgeflag.data = NULL;
   ctx->edgeflag.stride = 0;
   ctx->edgeflag.width = 0;
}

static inline void
nvc0_vertex_configure_translate(struct nvc0_context *nvc0, int32_t index_bias)
{
   struct translate *translate = nvc0->vertex->translate;
   unsigned i;

   for (i = 0; i < nvc0->num_vtxbufs; ++i) {
      const uint8_t *map;
      const struct pipe_vertex_buffer *vb = &nvc0->vtxbuf[i];

      if (likely(vb->is_user_buffer))
         map = (const uint8_t *)vb->buffer.user;
      else
         map = nouveau_resource_map_offset(&nvc0->base,
            nv04_resource(vb->buffer.resource), vb->buffer_offset, NOUVEAU_BO_RD);

      if (index_bias && !unlikely(nvc0->vertex->instance_bufs & (1 << i)))
         map += (intptr_t)index_bias * vb->stride;

      translate->set_buffer(translate, i, map, vb->stride, ~0);
   }
}

static inline void
nvc0_push_map_idxbuf(struct push_context *ctx, struct nvc0_context *nvc0,
                     const struct pipe_draw_info *info)
{
   if (!info->has_user_indices) {
      struct nv04_resource *buf = nv04_resource(info->index.resource);
      ctx->idxbuf = nouveau_resource_map_offset(
            &nvc0->base, buf, 0, NOUVEAU_BO_RD);
   } else {
      ctx->idxbuf = info->index.user;
   }
}

static inline void
nvc0_push_map_edgeflag(struct push_context *ctx, struct nvc0_context *nvc0,
                       int32_t index_bias)
{
   unsigned attr = nvc0->vertprog->vp.edgeflag;
   struct pipe_vertex_element *ve = &nvc0->vertex->element[attr].pipe;
   struct pipe_vertex_buffer *vb = &nvc0->vtxbuf[ve->vertex_buffer_index];
   struct nv04_resource *buf = nv04_resource(vb->buffer.resource);

   ctx->edgeflag.stride = vb->stride;
   ctx->edgeflag.width = util_format_get_blocksize(ve->src_format);
   if (!vb->is_user_buffer) {
      unsigned offset = vb->buffer_offset + ve->src_offset;
      ctx->edgeflag.data = nouveau_resource_map_offset(&nvc0->base,
                           buf, offset, NOUVEAU_BO_RD);
   } else {
      ctx->edgeflag.data = (const uint8_t *)vb->buffer.user + ve->src_offset;
   }

   if (index_bias)
      ctx->edgeflag.data += (intptr_t)index_bias * vb->stride;
}

static inline unsigned
prim_restart_search_i08(const uint8_t *elts, unsigned push, uint8_t index)
{
   unsigned i;
   for (i = 0; i < push && elts[i] != index; ++i);
   return i;
}

static inline unsigned
prim_restart_search_i16(const uint16_t *elts, unsigned push, uint16_t index)
{
   unsigned i;
   for (i = 0; i < push && elts[i] != index; ++i);
   return i;
}

static inline unsigned
prim_restart_search_i32(const uint32_t *elts, unsigned push, uint32_t index)
{
   unsigned i;
   for (i = 0; i < push && elts[i] != index; ++i);
   return i;
}

static inline bool
ef_value_8(const struct push_context *ctx, uint32_t index)
{
   uint8_t *pf = (uint8_t *)&ctx->edgeflag.data[index * ctx->edgeflag.stride];
   return !!*pf;
}

static inline bool
ef_value_32(const struct push_context *ctx, uint32_t index)
{
   uint32_t *pf = (uint32_t *)&ctx->edgeflag.data[index * ctx->edgeflag.stride];
   return !!*pf;
}

static inline bool
ef_toggle(struct push_context *ctx)
{
   ctx->edgeflag.value = !ctx->edgeflag.value;
   return ctx->edgeflag.value;
}

static inline unsigned
ef_toggle_search_i08(struct push_context *ctx, const uint8_t *elts, unsigned n)
{
   unsigned i;
   bool ef = ctx->edgeflag.value;
   if (ctx->edgeflag.width == 1)
      for (i = 0; i < n && ef_value_8(ctx, elts[i]) == ef; ++i);
   else
      for (i = 0; i < n && ef_value_32(ctx, elts[i]) == ef; ++i);
   return i;
}

static inline unsigned
ef_toggle_search_i16(struct push_context *ctx, const uint16_t *elts, unsigned n)
{
   unsigned i;
   bool ef = ctx->edgeflag.value;
   if (ctx->edgeflag.width == 1)
      for (i = 0; i < n && ef_value_8(ctx, elts[i]) == ef; ++i);
   else
      for (i = 0; i < n && ef_value_32(ctx, elts[i]) == ef; ++i);
   return i;
}

static inline unsigned
ef_toggle_search_i32(struct push_context *ctx, const uint32_t *elts, unsigned n)
{
   unsigned i;
   bool ef = ctx->edgeflag.value;
   if (ctx->edgeflag.width == 1)
      for (i = 0; i < n && ef_value_8(ctx, elts[i]) == ef; ++i);
   else
      for (i = 0; i < n && ef_value_32(ctx, elts[i]) == ef; ++i);
   return i;
}

static inline unsigned
ef_toggle_search_seq(struct push_context *ctx, unsigned start, unsigned n)
{
   unsigned i;
   bool ef = ctx->edgeflag.value;
   if (ctx->edgeflag.width == 1)
      for (i = 0; i < n && ef_value_8(ctx, start++) == ef; ++i);
   else
      for (i = 0; i < n && ef_value_32(ctx, start++) == ef; ++i);
   return i;
}

static inline void *
nvc0_push_setup_vertex_array(struct nvc0_context *nvc0, const unsigned count)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nouveau_bo *bo;
   uint64_t va;
   const unsigned size = count * nvc0->vertex->size;

   void *const dest = nouveau_scratch_get(&nvc0->base, size, &va, &bo);

   BEGIN_NVC0(push, NVC0_3D(VERTEX_ARRAY_START_HIGH(0)), 2);
   PUSH_DATAh(push, va);
   PUSH_DATA (push, va);
   BEGIN_NVC0(push, NVC0_3D(VERTEX_ARRAY_LIMIT_HIGH(0)), 2);
   PUSH_DATAh(push, va + size - 1);
   PUSH_DATA (push, va + size - 1);

   BCTX_REFN_bo(nvc0->bufctx_3d, 3D_VTX_TMP, NOUVEAU_BO_GART | NOUVEAU_BO_RD,
                bo);
   nouveau_pushbuf_validate(push);

   return dest;
}

static void
disp_vertices_i08(struct push_context *ctx, unsigned start, unsigned count)
{
   struct nouveau_pushbuf *push = ctx->push;
   struct translate *translate = ctx->translate;
   const uint8_t *restrict elts = (uint8_t *)ctx->idxbuf + start;
   unsigned pos = 0;

   do {
      unsigned nR = count;

      if (unlikely(ctx->prim_restart))
         nR = prim_restart_search_i08(elts, nR, ctx->restart_index);

      translate->run_elts8(translate, elts, nR,
                           ctx->start_instance, ctx->instance_id, ctx->dest);
      count -= nR;
      ctx->dest += nR * ctx->vertex_size;

      while (nR) {
         unsigned nE = nR;

         if (unlikely(ctx->edgeflag.enabled))
            nE = ef_toggle_search_i08(ctx, elts, nR);

         PUSH_SPACE(push, 4);
         if (likely(nE >= 2)) {
            BEGIN_NVC0(push, NVC0_3D(VERTEX_BUFFER_FIRST), 2);
            PUSH_DATA (push, pos);
            PUSH_DATA (push, nE);
         } else
         if (nE) {
            if (pos <= 0xff) {
               IMMED_NVC0(push, NVC0_3D(VB_ELEMENT_U32), pos);
            } else {
               BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
               PUSH_DATA (push, pos);
            }
         }
         if (unlikely(nE != nR))
            IMMED_NVC0(push, NVC0_3D(EDGEFLAG), ef_toggle(ctx));

         pos += nE;
         elts += nE;
         nR -= nE;
      }
      if (count) {
         BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
         PUSH_DATA (push, 0xffffffff);
         ++elts;
         ctx->dest += ctx->vertex_size;
         ++pos;
         --count;
      }
   } while (count);
}

static void
disp_vertices_i16(struct push_context *ctx, unsigned start, unsigned count)
{
   struct nouveau_pushbuf *push = ctx->push;
   struct translate *translate = ctx->translate;
   const uint16_t *restrict elts = (uint16_t *)ctx->idxbuf + start;
   unsigned pos = 0;

   do {
      unsigned nR = count;

      if (unlikely(ctx->prim_restart))
         nR = prim_restart_search_i16(elts, nR, ctx->restart_index);

      translate->run_elts16(translate, elts, nR,
                            ctx->start_instance, ctx->instance_id, ctx->dest);
      count -= nR;
      ctx->dest += nR * ctx->vertex_size;

      while (nR) {
         unsigned nE = nR;

         if (unlikely(ctx->edgeflag.enabled))
            nE = ef_toggle_search_i16(ctx, elts, nR);

         PUSH_SPACE(push, 4);
         if (likely(nE >= 2)) {
            BEGIN_NVC0(push, NVC0_3D(VERTEX_BUFFER_FIRST), 2);
            PUSH_DATA (push, pos);
            PUSH_DATA (push, nE);
         } else
         if (nE) {
            if (pos <= 0xff) {
               IMMED_NVC0(push, NVC0_3D(VB_ELEMENT_U32), pos);
            } else {
               BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
               PUSH_DATA (push, pos);
            }
         }
         if (unlikely(nE != nR))
            IMMED_NVC0(push, NVC0_3D(EDGEFLAG), ef_toggle(ctx));

         pos += nE;
         elts += nE;
         nR -= nE;
      }
      if (count) {
         BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
         PUSH_DATA (push, 0xffffffff);
         ++elts;
         ctx->dest += ctx->vertex_size;
         ++pos;
         --count;
      }
   } while (count);
}

static void
disp_vertices_i32(struct push_context *ctx, unsigned start, unsigned count)
{
   struct nouveau_pushbuf *push = ctx->push;
   struct translate *translate = ctx->translate;
   const uint32_t *restrict elts = (uint32_t *)ctx->idxbuf + start;
   unsigned pos = 0;

   do {
      unsigned nR = count;

      if (unlikely(ctx->prim_restart))
         nR = prim_restart_search_i32(elts, nR, ctx->restart_index);

      translate->run_elts(translate, elts, nR,
                          ctx->start_instance, ctx->instance_id, ctx->dest);
      count -= nR;
      ctx->dest += nR * ctx->vertex_size;

      while (nR) {
         unsigned nE = nR;

         if (unlikely(ctx->edgeflag.enabled))
            nE = ef_toggle_search_i32(ctx, elts, nR);

         PUSH_SPACE(push, 4);
         if (likely(nE >= 2)) {
            BEGIN_NVC0(push, NVC0_3D(VERTEX_BUFFER_FIRST), 2);
            PUSH_DATA (push, pos);
            PUSH_DATA (push, nE);
         } else
         if (nE) {
            if (pos <= 0xff) {
               IMMED_NVC0(push, NVC0_3D(VB_ELEMENT_U32), pos);
            } else {
               BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
               PUSH_DATA (push, pos);
            }
         }
         if (unlikely(nE != nR))
            IMMED_NVC0(push, NVC0_3D(EDGEFLAG), ef_toggle(ctx));

         pos += nE;
         elts += nE;
         nR -= nE;
      }
      if (count) {
         BEGIN_NVC0(push, NVC0_3D(VB_ELEMENT_U32), 1);
         PUSH_DATA (push, 0xffffffff);
         ++elts;
         ctx->dest += ctx->vertex_size;
         ++pos;
         --count;
      }
   } while (count);
}

static void
disp_vertices_seq(struct push_context *ctx, unsigned start, unsigned count)
{
   struct nouveau_pushbuf *push = ctx->push;
   struct translate *translate = ctx->translate;
   unsigned pos = 0;

   /* XXX: This will read the data corresponding to the primitive restart index,
    *  maybe we should avoid that ?
    */
   translate->run(translate, start, count,
                  ctx->start_instance, ctx->instance_id, ctx->dest);
   do {
      unsigned nr = count;

      if (unlikely(ctx->edgeflag.enabled))
         nr = ef_toggle_search_seq(ctx, start + pos, nr);

      PUSH_SPACE(push, 4);
      if (likely(nr)) {
         BEGIN_NVC0(push, NVC0_3D(VERTEX_BUFFER_FIRST), 2);
         PUSH_DATA (push, pos);
         PUSH_DATA (push, nr);
      }
      if (unlikely(nr != count))
         IMMED_NVC0(push, NVC0_3D(EDGEFLAG), ef_toggle(ctx));

      pos += nr;
      count -= nr;
   } while (count);
}


#define NVC0_PRIM_GL_CASE(n) \
   case PIPE_PRIM_##n: return NVC0_3D_VERTEX_BEGIN_GL_PRIMITIVE_##n

static inline unsigned
nvc0_prim_gl(unsigned prim)
{
   switch (prim) {
   NVC0_PRIM_GL_CASE(POINTS);
   NVC0_PRIM_GL_CASE(LINES);
   NVC0_PRIM_GL_CASE(LINE_LOOP);
   NVC0_PRIM_GL_CASE(LINE_STRIP);
   NVC0_PRIM_GL_CASE(TRIANGLES);
   NVC0_PRIM_GL_CASE(TRIANGLE_STRIP);
   NVC0_PRIM_GL_CASE(TRIANGLE_FAN);
   NVC0_PRIM_GL_CASE(QUADS);
   NVC0_PRIM_GL_CASE(QUAD_STRIP);
   NVC0_PRIM_GL_CASE(POLYGON);
   NVC0_PRIM_GL_CASE(LINES_ADJACENCY);
   NVC0_PRIM_GL_CASE(LINE_STRIP_ADJACENCY);
   NVC0_PRIM_GL_CASE(TRIANGLES_ADJACENCY);
   NVC0_PRIM_GL_CASE(TRIANGLE_STRIP_ADJACENCY);
   NVC0_PRIM_GL_CASE(PATCHES);
   default:
      return NVC0_3D_VERTEX_BEGIN_GL_PRIMITIVE_POINTS;
   }
}

void
nvc0_push_vbo(struct nvc0_context *nvc0, const struct pipe_draw_info *info)
{
   struct push_context ctx;
   unsigned i, index_size;
   unsigned inst_count = info->instance_count;
   unsigned vert_count = info->count;
   unsigned prim;

   nvc0_push_context_init(nvc0, &ctx);

   nvc0_vertex_configure_translate(nvc0, info->index_bias);

   if (nvc0->state.index_bias) {
      /* this is already taken care of by translate */
      IMMED_NVC0(ctx.push, NVC0_3D(VB_ELEMENT_BASE), 0);
      nvc0->state.index_bias = 0;
   }

   if (unlikely(ctx.edgeflag.enabled))
      nvc0_push_map_edgeflag(&ctx, nvc0, info->index_bias);

   ctx.prim_restart = info->primitive_restart;
   ctx.restart_index = info->restart_index;

   if (info->primitive_restart) {
      /* NOTE: I hope we won't ever need that last index (~0).
       * If we do, we have to disable primitive restart here always and
       * use END,BEGIN to restart. (XXX: would that affect PrimitiveID ?)
       * We could also deactive PRIM_RESTART_WITH_DRAW_ARRAYS temporarily,
       * and add manual restart to disp_vertices_seq.
       */
      BEGIN_NVC0(ctx.push, NVC0_3D(PRIM_RESTART_ENABLE), 2);
      PUSH_DATA (ctx.push, 1);
      PUSH_DATA (ctx.push, info->index_size ? 0xffffffff : info->restart_index);
   } else
   if (nvc0->state.prim_restart) {
      IMMED_NVC0(ctx.push, NVC0_3D(PRIM_RESTART_ENABLE), 0);
   }
   nvc0->state.prim_restart = info->primitive_restart;

   if (info->index_size) {
      nvc0_push_map_idxbuf(&ctx, nvc0, info);
      index_size = info->index_size;
   } else {
      if (unlikely(info->count_from_stream_output)) {
         struct pipe_context *pipe = &nvc0->base.pipe;
         struct nvc0_so_target *targ;
         targ = nvc0_so_target(info->count_from_stream_output);
         pipe->get_query_result(pipe, targ->pq, true, (void *)&vert_count);
         vert_count /= targ->stride;
      }
      ctx.idxbuf = NULL; /* shut up warnings */
      index_size = 0;
   }

   ctx.start_instance = info->start_instance;

   prim = nvc0_prim_gl(info->mode);
   do {
      PUSH_SPACE(ctx.push, 9);

      ctx.dest = nvc0_push_setup_vertex_array(nvc0, vert_count);
      if (unlikely(!ctx.dest))
         break;

      if (unlikely(ctx.need_vertex_id))
         nvc0_push_upload_vertex_ids(&ctx, nvc0, info);

      if (nvc0->screen->eng3d->oclass < GM107_3D_CLASS)
         IMMED_NVC0(ctx.push, NVC0_3D(VERTEX_ARRAY_FLUSH), 0);
      BEGIN_NVC0(ctx.push, NVC0_3D(VERTEX_BEGIN_GL), 1);
      PUSH_DATA (ctx.push, prim);
      switch (index_size) {
      case 1:
         disp_vertices_i08(&ctx, info->start, vert_count);
         break;
      case 2:
         disp_vertices_i16(&ctx, info->start, vert_count);
         break;
      case 4:
         disp_vertices_i32(&ctx, info->start, vert_count);
         break;
      default:
         assert(index_size == 0);
         disp_vertices_seq(&ctx, info->start, vert_count);
         break;
      }
      PUSH_SPACE(ctx.push, 1);
      IMMED_NVC0(ctx.push, NVC0_3D(VERTEX_END_GL), 0);

      if (--inst_count) {
         prim |= NVC0_3D_VERTEX_BEGIN_GL_INSTANCE_NEXT;
         ++ctx.instance_id;
      }
      nouveau_bufctx_reset(nvc0->bufctx_3d, NVC0_BIND_3D_VTX_TMP);
      nouveau_scratch_done(&nvc0->base);
   } while (inst_count);


   /* reset state and unmap buffers (no-op) */

   if (unlikely(!ctx.edgeflag.value)) {
      PUSH_SPACE(ctx.push, 1);
      IMMED_NVC0(ctx.push, NVC0_3D(EDGEFLAG), 1);
   }

   if (unlikely(ctx.need_vertex_id)) {
      PUSH_SPACE(ctx.push, 4);
      IMMED_NVC0(ctx.push, NVC0_3D(VERTEX_ID_REPLACE), 0);
      BEGIN_NVC0(ctx.push, NVC0_3D(VERTEX_ATTRIB_FORMAT(1)), 1);
      PUSH_DATA (ctx.push,
                 NVC0_3D_VERTEX_ATTRIB_FORMAT_CONST |
                 NVC0_3D_VERTEX_ATTRIB_FORMAT_TYPE_FLOAT |
                 NVC0_3D_VERTEX_ATTRIB_FORMAT_SIZE_32);
      IMMED_NVC0(ctx.push, NVC0_3D(VERTEX_ARRAY_FETCH(1)), 0);
   }

   if (info->index_size && !info->has_user_indices)
      nouveau_resource_unmap(nv04_resource(info->index.resource));
   for (i = 0; i < nvc0->num_vtxbufs; ++i)
      nouveau_resource_unmap(nv04_resource(nvc0->vtxbuf[i].buffer.resource));

   NOUVEAU_DRV_STAT(&nvc0->screen->base, draw_calls_fallback_count, 1);
}

static inline void
copy_indices_u8(uint32_t *dst, const uint8_t *elts, uint32_t bias, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; ++i)
      dst[i] = elts[i] + bias;
}

static inline void
copy_indices_u16(uint32_t *dst, const uint16_t *elts, uint32_t bias, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; ++i)
      dst[i] = elts[i] + bias;
}

static inline void
copy_indices_u32(uint32_t *dst, const uint32_t *elts, uint32_t bias, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; ++i)
      dst[i] = elts[i] + bias;
}

static void
nvc0_push_upload_vertex_ids(struct push_context *ctx,
                            struct nvc0_context *nvc0,
                            const struct pipe_draw_info *info)

{
   struct nouveau_pushbuf *push = ctx->push;
   struct nouveau_bo *bo;
   uint64_t va;
   uint32_t *data;
   uint32_t format;
   unsigned index_size = info->index_size;
   unsigned i;
   unsigned a = nvc0->vertex->num_elements;

   if (!index_size || info->index_bias)
      index_size = 4;
   data = (uint32_t *)nouveau_scratch_get(&nvc0->base,
                                          info->count * index_size, &va, &bo);

   BCTX_REFN_bo(nvc0->bufctx_3d, 3D_VTX_TMP, NOUVEAU_BO_GART | NOUVEAU_BO_RD,
                bo);
   nouveau_pushbuf_validate(push);

   if (info->index_size) {
      if (!info->index_bias) {
         memcpy(data, ctx->idxbuf, info->count * index_size);
      } else {
         switch (info->index_size) {
         case 1:
            copy_indices_u8(data, ctx->idxbuf, info->index_bias, info->count);
            break;
         case 2:
            copy_indices_u16(data, ctx->idxbuf, info->index_bias, info->count);
            break;
         default:
            copy_indices_u32(data, ctx->idxbuf, info->index_bias, info->count);
            break;
         }
      }
   } else {
      for (i = 0; i < info->count; ++i)
         data[i] = i + (info->start + info->index_bias);
   }

   format = (1 << NVC0_3D_VERTEX_ATTRIB_FORMAT_BUFFER__SHIFT) |
      NVC0_3D_VERTEX_ATTRIB_FORMAT_TYPE_UINT;

   switch (index_size) {
   case 1:
      format |= NVC0_3D_VERTEX_ATTRIB_FORMAT_SIZE_8;
      break;
   case 2:
      format |= NVC0_3D_VERTEX_ATTRIB_FORMAT_SIZE_16;
      break;
   default:
      format |= NVC0_3D_VERTEX_ATTRIB_FORMAT_SIZE_32;
      break;
   }

   PUSH_SPACE(push, 12);

   if (unlikely(nvc0->state.instance_elts & 2)) {
      nvc0->state.instance_elts &= ~2;
      IMMED_NVC0(push, NVC0_3D(VERTEX_ARRAY_PER_INSTANCE(1)), 0);
   }

   BEGIN_NVC0(push, NVC0_3D(VERTEX_ATTRIB_FORMAT(a)), 1);
   PUSH_DATA (push, format);

   BEGIN_NVC0(push, NVC0_3D(VERTEX_ARRAY_FETCH(1)), 3);
   PUSH_DATA (push, NVC0_3D_VERTEX_ARRAY_FETCH_ENABLE | index_size);
   PUSH_DATAh(push, va);
   PUSH_DATA (push, va);
   BEGIN_NVC0(push, NVC0_3D(VERTEX_ARRAY_LIMIT_HIGH(1)), 2);
   PUSH_DATAh(push, va + info->count * index_size - 1);
   PUSH_DATA (push, va + info->count * index_size - 1);

#define NVC0_3D_VERTEX_ID_REPLACE_SOURCE_ATTR_X(a) \
   (((0x80 + (a) * 0x10) / 4) << NVC0_3D_VERTEX_ID_REPLACE_SOURCE__SHIFT)

   BEGIN_NVC0(push, NVC0_3D(VERTEX_ID_REPLACE), 1);
   PUSH_DATA (push, NVC0_3D_VERTEX_ID_REPLACE_SOURCE_ATTR_X(a) | 1);
}
