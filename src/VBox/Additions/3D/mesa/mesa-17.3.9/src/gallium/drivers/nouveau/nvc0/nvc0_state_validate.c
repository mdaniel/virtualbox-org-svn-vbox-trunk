
#include "util/u_framebuffer.h"
#include "util/u_math.h"
#include "util/u_viewport.h"

#include "nvc0/nvc0_context.h"

#if 0
static void
nvc0_validate_zcull(struct nvc0_context *nvc0)
{
    struct nouveau_pushbuf *push = nvc0->base.pushbuf;
    struct pipe_framebuffer_state *fb = &nvc0->framebuffer;
    struct nv50_surface *sf = nv50_surface(fb->zsbuf);
    struct nv50_miptree *mt = nv50_miptree(sf->base.texture);
    struct nouveau_bo *bo = mt->base.bo;
    uint32_t size;
    uint32_t offset = align(mt->total_size, 1 << 17);
    unsigned width, height;

    assert(mt->base.base.depth0 == 1 && mt->base.base.array_size < 2);

    size = mt->total_size * 2;

    height = align(fb->height, 32);
    width = fb->width % 224;
    if (width)
       width = fb->width + (224 - width);
    else
       width = fb->width;

    BEGIN_NVC0(push, NVC0_3D(ZCULL_REGION), 1);
    PUSH_DATA (push, 0);
    BEGIN_NVC0(push, NVC0_3D(ZCULL_ADDRESS_HIGH), 2);
    PUSH_DATAh(push, bo->offset + offset);
    PUSH_DATA (push, bo->offset + offset);
    offset += 1 << 17;
    BEGIN_NVC0(push, NVC0_3D(ZCULL_LIMIT_HIGH), 2);
    PUSH_DATAh(push, bo->offset + offset);
    PUSH_DATA (push, bo->offset + offset);
    BEGIN_NVC0(push, SUBC_3D(0x07e0), 2);
    PUSH_DATA (push, size);
    PUSH_DATA (push, size >> 16);
    BEGIN_NVC0(push, SUBC_3D(0x15c8), 1); /* bits 0x3 */
    PUSH_DATA (push, 2);
    BEGIN_NVC0(push, NVC0_3D(ZCULL_WIDTH), 4);
    PUSH_DATA (push, width);
    PUSH_DATA (push, height);
    PUSH_DATA (push, 1);
    PUSH_DATA (push, 0);
    BEGIN_NVC0(push, NVC0_3D(ZCULL_WINDOW_OFFSET_X), 2);
    PUSH_DATA (push, 0);
    PUSH_DATA (push, 0);
    BEGIN_NVC0(push, NVC0_3D(ZCULL_INVALIDATE), 1);
    PUSH_DATA (push, 0);
}
#endif

static inline void
nvc0_fb_set_null_rt(struct nouveau_pushbuf *push, unsigned i, unsigned layers)
{
   BEGIN_NVC0(push, NVC0_3D(RT_ADDRESS_HIGH(i)), 9);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 0);
   PUSH_DATA (push, 64);     // width
   PUSH_DATA (push, 0);      // height
   PUSH_DATA (push, 0);      // format
   PUSH_DATA (push, 0);      // tile mode
   PUSH_DATA (push, layers); // layers
   PUSH_DATA (push, 0);      // layer stride
   PUSH_DATA (push, 0);      // base layer
}

static void
nvc0_validate_fb(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct pipe_framebuffer_state *fb = &nvc0->framebuffer;
   struct nvc0_screen *screen = nvc0->screen;
   unsigned i, ms;
   unsigned ms_mode = NVC0_3D_MULTISAMPLE_MODE_MS1;
   unsigned nr_cbufs = fb->nr_cbufs;
   bool serialize = false;

   nouveau_bufctx_reset(nvc0->bufctx_3d, NVC0_BIND_3D_FB);

   BEGIN_NVC0(push, NVC0_3D(SCREEN_SCISSOR_HORIZ), 2);
   PUSH_DATA (push, fb->width << 16);
   PUSH_DATA (push, fb->height << 16);

   for (i = 0; i < fb->nr_cbufs; ++i) {
      struct nv50_surface *sf;
      struct nv04_resource *res;
      struct nouveau_bo *bo;

      if (!fb->cbufs[i]) {
         nvc0_fb_set_null_rt(push, i, 0);
         continue;
      }

      sf = nv50_surface(fb->cbufs[i]);
      res = nv04_resource(sf->base.texture);
      bo = res->bo;

      BEGIN_NVC0(push, NVC0_3D(RT_ADDRESS_HIGH(i)), 9);
      PUSH_DATAh(push, res->address + sf->offset);
      PUSH_DATA (push, res->address + sf->offset);
      if (likely(nouveau_bo_memtype(bo))) {
         struct nv50_miptree *mt = nv50_miptree(sf->base.texture);

         assert(sf->base.texture->target != PIPE_BUFFER);

         PUSH_DATA(push, sf->width);
         PUSH_DATA(push, sf->height);
         PUSH_DATA(push, nvc0_format_table[sf->base.format].rt);
         PUSH_DATA(push, (mt->layout_3d << 16) |
                          mt->level[sf->base.u.tex.level].tile_mode);
         PUSH_DATA(push, sf->base.u.tex.first_layer + sf->depth);
         PUSH_DATA(push, mt->layer_stride >> 2);
         PUSH_DATA(push, sf->base.u.tex.first_layer);

         ms_mode = mt->ms_mode;
      } else {
         if (res->base.target == PIPE_BUFFER) {
            PUSH_DATA(push, 262144);
            PUSH_DATA(push, 1);
         } else {
            PUSH_DATA(push, nv50_miptree(sf->base.texture)->level[0].pitch);
            PUSH_DATA(push, sf->height);
         }
         PUSH_DATA(push, nvc0_format_table[sf->base.format].rt);
         PUSH_DATA(push, 1 << 12);
         PUSH_DATA(push, 1);
         PUSH_DATA(push, 0);
         PUSH_DATA(push, 0);

         nvc0_resource_fence(res, NOUVEAU_BO_WR);

         assert(!fb->zsbuf);
      }

      if (res->status & NOUVEAU_BUFFER_STATUS_GPU_READING)
         serialize = true;
      res->status |=  NOUVEAU_BUFFER_STATUS_GPU_WRITING;
      res->status &= ~NOUVEAU_BUFFER_STATUS_GPU_READING;

      /* only register for writing, otherwise we'd always serialize here */
      BCTX_REFN(nvc0->bufctx_3d, 3D_FB, res, WR);
   }

   if (fb->zsbuf) {
      struct nv50_miptree *mt = nv50_miptree(fb->zsbuf->texture);
      struct nv50_surface *sf = nv50_surface(fb->zsbuf);
      int unk = mt->base.base.target == PIPE_TEXTURE_2D;

      BEGIN_NVC0(push, NVC0_3D(ZETA_ADDRESS_HIGH), 5);
      PUSH_DATAh(push, mt->base.address + sf->offset);
      PUSH_DATA (push, mt->base.address + sf->offset);
      PUSH_DATA (push, nvc0_format_table[fb->zsbuf->format].rt);
      PUSH_DATA (push, mt->level[sf->base.u.tex.level].tile_mode);
      PUSH_DATA (push, mt->layer_stride >> 2);
      BEGIN_NVC0(push, NVC0_3D(ZETA_ENABLE), 1);
      PUSH_DATA (push, 1);
      BEGIN_NVC0(push, NVC0_3D(ZETA_HORIZ), 3);
      PUSH_DATA (push, sf->width);
      PUSH_DATA (push, sf->height);
      PUSH_DATA (push, (unk << 16) |
                (sf->base.u.tex.first_layer + sf->depth));
      BEGIN_NVC0(push, NVC0_3D(ZETA_BASE_LAYER), 1);
      PUSH_DATA (push, sf->base.u.tex.first_layer);

      ms_mode = mt->ms_mode;

      if (mt->base.status & NOUVEAU_BUFFER_STATUS_GPU_READING)
         serialize = true;
      mt->base.status |=  NOUVEAU_BUFFER_STATUS_GPU_WRITING;
      mt->base.status &= ~NOUVEAU_BUFFER_STATUS_GPU_READING;

      BCTX_REFN(nvc0->bufctx_3d, 3D_FB, &mt->base, WR);
   } else {
       BEGIN_NVC0(push, NVC0_3D(ZETA_ENABLE), 1);
      PUSH_DATA (push, 0);
   }

   if (nr_cbufs == 0 && !fb->zsbuf) {
      assert(util_is_power_of_two(fb->samples));
      assert(fb->samples <= 8);

      nvc0_fb_set_null_rt(push, 0, fb->layers);

      if (fb->samples > 1)
         ms_mode = ffs(fb->samples) - 1;
      nr_cbufs = 1;
   }

   BEGIN_NVC0(push, NVC0_3D(RT_CONTROL), 1);
   PUSH_DATA (push, (076543210 << 4) | nr_cbufs);
   IMMED_NVC0(push, NVC0_3D(MULTISAMPLE_MODE), ms_mode);

   ms = 1 << ms_mode;
   BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
   PUSH_DATA (push, NVC0_CB_AUX_SIZE);
   PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(4));
   PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(4));
   BEGIN_1IC0(push, NVC0_3D(CB_POS), 1 + 2 * ms);
   PUSH_DATA (push, NVC0_CB_AUX_SAMPLE_INFO);
   for (i = 0; i < ms; i++) {
      float xy[2];
      nvc0->base.pipe.get_sample_position(&nvc0->base.pipe, ms, i, xy);
      PUSH_DATAf(push, xy[0]);
      PUSH_DATAf(push, xy[1]);
   }

   if (screen->base.class_3d >= GM200_3D_CLASS) {
      const uint8_t (*ptr)[2] = nvc0_get_sample_locations(ms);
      uint32_t val[4] = {};

      for (i = 0; i < 16; i++) {
         val[i / 4] |= ptr[i % ms][0] << (((i % 4) * 8) + 0);
         val[i / 4] |= ptr[i % ms][1] << (((i % 4) * 8) + 4);
      }

      BEGIN_NVC0(push, SUBC_3D(0x11e0), 4);
      PUSH_DATAp(push, val, 4);
   }

   if (serialize)
      IMMED_NVC0(push, NVC0_3D(SERIALIZE), 0);

   NOUVEAU_DRV_STAT(&nvc0->screen->base, gpu_serialize_count, serialize);
}

static void
nvc0_validate_blend_colour(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   BEGIN_NVC0(push, NVC0_3D(BLEND_COLOR(0)), 4);
   PUSH_DATAf(push, nvc0->blend_colour.color[0]);
   PUSH_DATAf(push, nvc0->blend_colour.color[1]);
   PUSH_DATAf(push, nvc0->blend_colour.color[2]);
   PUSH_DATAf(push, nvc0->blend_colour.color[3]);
}

static void
nvc0_validate_stencil_ref(struct nvc0_context *nvc0)
{
    struct nouveau_pushbuf *push = nvc0->base.pushbuf;
    const ubyte *ref = &nvc0->stencil_ref.ref_value[0];

    IMMED_NVC0(push, NVC0_3D(STENCIL_FRONT_FUNC_REF), ref[0]);
    IMMED_NVC0(push, NVC0_3D(STENCIL_BACK_FUNC_REF), ref[1]);
}

static void
nvc0_validate_stipple(struct nvc0_context *nvc0)
{
    struct nouveau_pushbuf *push = nvc0->base.pushbuf;
    unsigned i;

    BEGIN_NVC0(push, NVC0_3D(POLYGON_STIPPLE_PATTERN(0)), 32);
    for (i = 0; i < 32; ++i)
        PUSH_DATA(push, util_bswap32(nvc0->stipple.stipple[i]));
}

static void
nvc0_validate_scissor(struct nvc0_context *nvc0)
{
   int i;
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   if (!(nvc0->dirty_3d & NVC0_NEW_3D_SCISSOR) &&
      nvc0->rast->pipe.scissor == nvc0->state.scissor)
      return;

   if (nvc0->state.scissor != nvc0->rast->pipe.scissor)
      nvc0->scissors_dirty = (1 << NVC0_MAX_VIEWPORTS) - 1;

   nvc0->state.scissor = nvc0->rast->pipe.scissor;

   for (i = 0; i < NVC0_MAX_VIEWPORTS; i++) {
      struct pipe_scissor_state *s = &nvc0->scissors[i];
      if (!(nvc0->scissors_dirty & (1 << i)))
         continue;

      BEGIN_NVC0(push, NVC0_3D(SCISSOR_HORIZ(i)), 2);
      if (nvc0->rast->pipe.scissor) {
         PUSH_DATA(push, (s->maxx << 16) | s->minx);
         PUSH_DATA(push, (s->maxy << 16) | s->miny);
      } else {
         PUSH_DATA(push, (0xffff << 16) | 0);
         PUSH_DATA(push, (0xffff << 16) | 0);
      }
   }
   nvc0->scissors_dirty = 0;
}

static void
nvc0_validate_viewport(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   int x, y, w, h, i;
   float zmin, zmax;

   for (i = 0; i < NVC0_MAX_VIEWPORTS; i++) {
      struct pipe_viewport_state *vp = &nvc0->viewports[i];

      if (!(nvc0->viewports_dirty & (1 << i)))
         continue;

      BEGIN_NVC0(push, NVC0_3D(VIEWPORT_TRANSLATE_X(i)), 3);
      PUSH_DATAf(push, vp->translate[0]);
      PUSH_DATAf(push, vp->translate[1]);
      PUSH_DATAf(push, vp->translate[2]);

      BEGIN_NVC0(push, NVC0_3D(VIEWPORT_SCALE_X(i)), 3);
      PUSH_DATAf(push, vp->scale[0]);
      PUSH_DATAf(push, vp->scale[1]);
      PUSH_DATAf(push, vp->scale[2]);

      /* now set the viewport rectangle to viewport dimensions for clipping */

      x = util_iround(MAX2(0.0f, vp->translate[0] - fabsf(vp->scale[0])));
      y = util_iround(MAX2(0.0f, vp->translate[1] - fabsf(vp->scale[1])));
      w = util_iround(vp->translate[0] + fabsf(vp->scale[0])) - x;
      h = util_iround(vp->translate[1] + fabsf(vp->scale[1])) - y;

      BEGIN_NVC0(push, NVC0_3D(VIEWPORT_HORIZ(i)), 2);
      PUSH_DATA (push, (w << 16) | x);
      PUSH_DATA (push, (h << 16) | y);

      /* If the halfz setting ever changes, the viewports will also get
       * updated. The rast will get updated before the validate function has a
       * chance to hit, so we can just use it directly without an atom
       * dependency.
       */
      util_viewport_zmin_zmax(vp, nvc0->rast->pipe.clip_halfz, &zmin, &zmax);

      BEGIN_NVC0(push, NVC0_3D(DEPTH_RANGE_NEAR(i)), 2);
      PUSH_DATAf(push, zmin);
      PUSH_DATAf(push, zmax);
   }
   nvc0->viewports_dirty = 0;
}

static void
nvc0_validate_window_rects(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   bool enable = nvc0->window_rect.rects > 0 || nvc0->window_rect.inclusive;
   int i;

   IMMED_NVC0(push, NVC0_3D(CLIP_RECTS_EN), enable);
   if (!enable)
      return;

   IMMED_NVC0(push, NVC0_3D(CLIP_RECTS_MODE), !nvc0->window_rect.inclusive);
   BEGIN_NVC0(push, NVC0_3D(CLIP_RECT_HORIZ(0)), NVC0_MAX_WINDOW_RECTANGLES * 2);
   for (i = 0; i < nvc0->window_rect.rects; i++) {
      struct pipe_scissor_state *s = &nvc0->window_rect.rect[i];
      PUSH_DATA(push, (s->maxx << 16) | s->minx);
      PUSH_DATA(push, (s->maxy << 16) | s->miny);
   }
   for (; i < NVC0_MAX_WINDOW_RECTANGLES; i++) {
      PUSH_DATA(push, 0);
      PUSH_DATA(push, 0);
   }
}

static inline void
nvc0_upload_uclip_planes(struct nvc0_context *nvc0, unsigned s)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nvc0_screen *screen = nvc0->screen;

   BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
   PUSH_DATA (push, NVC0_CB_AUX_SIZE);
   PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(s));
   PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(s));
   BEGIN_1IC0(push, NVC0_3D(CB_POS), PIPE_MAX_CLIP_PLANES * 4 + 1);
   PUSH_DATA (push, NVC0_CB_AUX_UCP_INFO);
   PUSH_DATAp(push, &nvc0->clip.ucp[0][0], PIPE_MAX_CLIP_PLANES * 4);
}

static inline void
nvc0_check_program_ucps(struct nvc0_context *nvc0,
                        struct nvc0_program *vp, uint8_t mask)
{
   const unsigned n = util_logbase2(mask) + 1;

   if (vp->vp.num_ucps >= n)
      return;
   nvc0_program_destroy(nvc0, vp);

   vp->vp.num_ucps = n;
   if (likely(vp == nvc0->vertprog))
      nvc0_vertprog_validate(nvc0);
   else
   if (likely(vp == nvc0->gmtyprog))
      nvc0_gmtyprog_validate(nvc0);
   else
      nvc0_tevlprog_validate(nvc0);
}

static void
nvc0_validate_clip(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nvc0_program *vp;
   unsigned stage;
   uint8_t clip_enable = nvc0->rast->pipe.clip_plane_enable;

   if (nvc0->gmtyprog) {
      stage = 3;
      vp = nvc0->gmtyprog;
   } else
   if (nvc0->tevlprog) {
      stage = 2;
      vp = nvc0->tevlprog;
   } else {
      stage = 0;
      vp = nvc0->vertprog;
   }

   if (clip_enable && vp->vp.num_ucps < PIPE_MAX_CLIP_PLANES)
      nvc0_check_program_ucps(nvc0, vp, clip_enable);

   if (nvc0->dirty_3d & (NVC0_NEW_3D_CLIP | (NVC0_NEW_3D_VERTPROG << stage)))
      if (vp->vp.num_ucps > 0 && vp->vp.num_ucps <= PIPE_MAX_CLIP_PLANES)
         nvc0_upload_uclip_planes(nvc0, stage);

   clip_enable &= vp->vp.clip_enable;
   clip_enable |= vp->vp.cull_enable;

   if (nvc0->state.clip_enable != clip_enable) {
      nvc0->state.clip_enable = clip_enable;
      IMMED_NVC0(push, NVC0_3D(CLIP_DISTANCE_ENABLE), clip_enable);
   }
   if (nvc0->state.clip_mode != vp->vp.clip_mode) {
      nvc0->state.clip_mode = vp->vp.clip_mode;
      BEGIN_NVC0(push, NVC0_3D(CLIP_DISTANCE_MODE), 1);
      PUSH_DATA (push, vp->vp.clip_mode);
   }
}

static void
nvc0_validate_blend(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   PUSH_SPACE(push, nvc0->blend->size);
   PUSH_DATAp(push, nvc0->blend->state, nvc0->blend->size);
}

static void
nvc0_validate_zsa(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   PUSH_SPACE(push, nvc0->zsa->size);
   PUSH_DATAp(push, nvc0->zsa->state, nvc0->zsa->size);
}

static void
nvc0_validate_rasterizer(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   PUSH_SPACE(push, nvc0->rast->size);
   PUSH_DATAp(push, nvc0->rast->state, nvc0->rast->size);
}

static void
nvc0_constbufs_validate(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   unsigned s;

   for (s = 0; s < 5; ++s) {
      while (nvc0->constbuf_dirty[s]) {
         int i = ffs(nvc0->constbuf_dirty[s]) - 1;
         nvc0->constbuf_dirty[s] &= ~(1 << i);

         if (nvc0->constbuf[s][i].user) {
            struct nouveau_bo *bo = nvc0->screen->uniform_bo;
            const unsigned base = NVC0_CB_USR_INFO(s);
            const unsigned size = nvc0->constbuf[s][0].size;
            assert(i == 0); /* we really only want OpenGL uniforms here */
            assert(nvc0->constbuf[s][0].u.data);

            if (nvc0->state.uniform_buffer_bound[s] < size) {
               nvc0->state.uniform_buffer_bound[s] = align(size, 0x100);

               BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
               PUSH_DATA (push, nvc0->state.uniform_buffer_bound[s]);
               PUSH_DATAh(push, bo->offset + base);
               PUSH_DATA (push, bo->offset + base);
               BEGIN_NVC0(push, NVC0_3D(CB_BIND(s)), 1);
               PUSH_DATA (push, (0 << 4) | 1);
            }
            nvc0_cb_bo_push(&nvc0->base, bo, NV_VRAM_DOMAIN(&nvc0->screen->base),
                         base, nvc0->state.uniform_buffer_bound[s],
                         0, (size + 3) / 4,
                         nvc0->constbuf[s][0].u.data);
         } else {
            struct nv04_resource *res =
               nv04_resource(nvc0->constbuf[s][i].u.buf);
            if (res) {
               BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
               PUSH_DATA (push, nvc0->constbuf[s][i].size);
               PUSH_DATAh(push, res->address + nvc0->constbuf[s][i].offset);
               PUSH_DATA (push, res->address + nvc0->constbuf[s][i].offset);
               BEGIN_NVC0(push, NVC0_3D(CB_BIND(s)), 1);
               PUSH_DATA (push, (i << 4) | 1);

               BCTX_REFN(nvc0->bufctx_3d, 3D_CB(s, i), res, RD);

               nvc0->cb_dirty = 1; /* Force cache flush for UBO. */
               res->cb_bindings[s] |= 1 << i;
            } else {
               BEGIN_NVC0(push, NVC0_3D(CB_BIND(s)), 1);
               PUSH_DATA (push, (i << 4) | 0);
            }
            if (i == 0)
               nvc0->state.uniform_buffer_bound[s] = 0;
         }
      }
   }

   if (nvc0->screen->base.class_3d < NVE4_3D_CLASS) {
      /* Invalidate all COMPUTE constbufs because they are aliased with 3D. */
      nvc0->dirty_cp |= NVC0_NEW_CP_CONSTBUF;
      nvc0->constbuf_dirty[5] |= nvc0->constbuf_valid[5];
      nvc0->state.uniform_buffer_bound[5] = 0;
   }
}

static void
nvc0_validate_buffers(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nvc0_screen *screen = nvc0->screen;
   int i, s;

   for (s = 0; s < 5; s++) {
      BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
      PUSH_DATA (push, NVC0_CB_AUX_SIZE);
      PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(s));
      PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(s));
      BEGIN_1IC0(push, NVC0_3D(CB_POS), 1 + 4 * NVC0_MAX_BUFFERS);
      PUSH_DATA (push, NVC0_CB_AUX_BUF_INFO(0));
      for (i = 0; i < NVC0_MAX_BUFFERS; i++) {
         if (nvc0->buffers[s][i].buffer) {
            struct nv04_resource *res =
               nv04_resource(nvc0->buffers[s][i].buffer);
            PUSH_DATA (push, res->address + nvc0->buffers[s][i].buffer_offset);
            PUSH_DATAh(push, res->address + nvc0->buffers[s][i].buffer_offset);
            PUSH_DATA (push, nvc0->buffers[s][i].buffer_size);
            PUSH_DATA (push, 0);
            BCTX_REFN(nvc0->bufctx_3d, 3D_BUF, res, RDWR);
            util_range_add(&res->valid_buffer_range,
                           nvc0->buffers[s][i].buffer_offset,
                           nvc0->buffers[s][i].buffer_offset +
                           nvc0->buffers[s][i].buffer_size);
         } else {
            PUSH_DATA (push, 0);
            PUSH_DATA (push, 0);
            PUSH_DATA (push, 0);
            PUSH_DATA (push, 0);
         }
      }
   }

}

static void
nvc0_validate_sample_mask(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   unsigned mask[4] =
   {
      nvc0->sample_mask & 0xffff,
      nvc0->sample_mask & 0xffff,
      nvc0->sample_mask & 0xffff,
      nvc0->sample_mask & 0xffff
   };

   BEGIN_NVC0(push, NVC0_3D(MSAA_MASK(0)), 4);
   PUSH_DATA (push, mask[0]);
   PUSH_DATA (push, mask[1]);
   PUSH_DATA (push, mask[2]);
   PUSH_DATA (push, mask[3]);
}

static void
nvc0_validate_min_samples(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   int samples;

   samples = util_next_power_of_two(nvc0->min_samples);
   if (samples > 1) {
      // If we're using the incoming sample mask and doing sample shading, we
      // have to do sample shading "to the max", otherwise there's no way to
      // tell which sets of samples are covered by the current invocation.
      // Similarly for reading the framebuffer.
      if (nvc0->fragprog && (
                nvc0->fragprog->fp.sample_mask_in ||
                nvc0->fragprog->fp.reads_framebuffer))
         samples = util_framebuffer_get_num_samples(&nvc0->framebuffer);
      samples |= NVC0_3D_SAMPLE_SHADING_ENABLE;
   }

   IMMED_NVC0(push, NVC0_3D(SAMPLE_SHADING), samples);
}

static void
nvc0_validate_driverconst(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nvc0_screen *screen = nvc0->screen;
   int i;

   for (i = 0; i < 5; ++i) {
      BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
      PUSH_DATA (push, NVC0_CB_AUX_SIZE);
      PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(i));
      PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(i));
      BEGIN_NVC0(push, NVC0_3D(CB_BIND(i)), 1);
      PUSH_DATA (push, (15 << 4) | 1);
   }

   nvc0->dirty_cp |= NVC0_NEW_CP_DRIVERCONST;
}

static void
nvc0_validate_fp_zsa_rast(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   bool rasterizer_discard;

   if (nvc0->rast && nvc0->rast->pipe.rasterizer_discard) {
      rasterizer_discard = true;
   } else {
      bool zs = nvc0->zsa &&
         (nvc0->zsa->pipe.depth.enabled || nvc0->zsa->pipe.stencil[0].enabled);
      rasterizer_discard = !zs &&
         (!nvc0->fragprog || !nvc0->fragprog->hdr[18]);
   }

   if (rasterizer_discard != nvc0->state.rasterizer_discard) {
      nvc0->state.rasterizer_discard = rasterizer_discard;
      IMMED_NVC0(push, NVC0_3D(RASTERIZE_ENABLE), !rasterizer_discard);
   }
}

/* alpha test is disabled if there are no color RTs, so make sure we have at
 * least one if alpha test is enabled. Note that this must run after
 * nvc0_validate_fb, otherwise that will override the RT count setting.
 */
static void
nvc0_validate_zsa_fb(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   if (nvc0->zsa && nvc0->zsa->pipe.alpha.enabled &&
       nvc0->framebuffer.zsbuf &&
       nvc0->framebuffer.nr_cbufs == 0) {
      nvc0_fb_set_null_rt(push, 0, 0);
      BEGIN_NVC0(push, NVC0_3D(RT_CONTROL), 1);
      PUSH_DATA (push, (076543210 << 4) | 1);
   }
}

static void
nvc0_validate_rast_fb(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct pipe_framebuffer_state *fb = &nvc0->framebuffer;
   struct pipe_rasterizer_state *rast = &nvc0->rast->pipe;

   if (!rast)
      return;

   if (rast->offset_units_unscaled) {
      BEGIN_NVC0(push, NVC0_3D(POLYGON_OFFSET_UNITS), 1);
      if (fb->zsbuf && fb->zsbuf->format == PIPE_FORMAT_Z16_UNORM)
         PUSH_DATAf(push, rast->offset_units * (1 << 16));
      else
         PUSH_DATAf(push, rast->offset_units * (1 << 24));
   }
}


static void
nvc0_validate_tess_state(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;

   BEGIN_NVC0(push, NVC0_3D(TESS_LEVEL_OUTER(0)), 6);
   PUSH_DATAp(push, nvc0->default_tess_outer, 4);
   PUSH_DATAp(push, nvc0->default_tess_inner, 2);
}

/* If we have a frag shader bound which tries to read from the framebuffer, we
 * have to make sure that the fb is bound as a texture in the expected
 * location. For Fermi, that's in the special driver slot 16, while for Kepler
 * it's a regular binding stored in the driver constbuf.
 */
static void
nvc0_validate_fbread(struct nvc0_context *nvc0)
{
   struct nouveau_pushbuf *push = nvc0->base.pushbuf;
   struct nvc0_screen *screen = nvc0->screen;
   struct pipe_context *pipe = &nvc0->base.pipe;
   struct pipe_sampler_view *old_view = nvc0->fbtexture;
   struct pipe_sampler_view *new_view = NULL;

   if (nvc0->fragprog &&
       nvc0->fragprog->fp.reads_framebuffer &&
       nvc0->framebuffer.nr_cbufs &&
       nvc0->framebuffer.cbufs[0]) {
      struct pipe_sampler_view tmpl;
      struct pipe_surface *sf = nvc0->framebuffer.cbufs[0];

      tmpl.target = PIPE_TEXTURE_2D_ARRAY;
      tmpl.format = sf->format;
      tmpl.u.tex.first_level = tmpl.u.tex.last_level = sf->u.tex.level;
      tmpl.u.tex.first_layer = sf->u.tex.first_layer;
      tmpl.u.tex.last_layer = sf->u.tex.last_layer;
      tmpl.swizzle_r = PIPE_SWIZZLE_X;
      tmpl.swizzle_g = PIPE_SWIZZLE_Y;
      tmpl.swizzle_b = PIPE_SWIZZLE_Z;
      tmpl.swizzle_a = PIPE_SWIZZLE_W;

      /* Bail if it's the same parameters */
      if (old_view && old_view->texture == sf->texture &&
          old_view->format == sf->format &&
          old_view->u.tex.first_level == sf->u.tex.level &&
          old_view->u.tex.first_layer == sf->u.tex.first_layer &&
          old_view->u.tex.last_layer == sf->u.tex.last_layer)
         return;

      new_view = pipe->create_sampler_view(pipe, sf->texture, &tmpl);
   } else if (old_view == NULL) {
      return;
   }

   if (old_view)
      pipe_sampler_view_reference(&nvc0->fbtexture, NULL);
   nvc0->fbtexture = new_view;

   if (screen->default_tsc->id < 0) {
      struct nv50_tsc_entry *tsc = nv50_tsc_entry(screen->default_tsc);
      tsc->id = nvc0_screen_tsc_alloc(screen, tsc);
      nvc0->base.push_data(&nvc0->base, screen->txc, 65536 + tsc->id * 32,
                           NV_VRAM_DOMAIN(&screen->base), 32, tsc->tsc);
      screen->tsc.lock[tsc->id / 32] |= 1 << (tsc->id % 32);

      IMMED_NVC0(push, NVC0_3D(TSC_FLUSH), 0);
      if (screen->base.class_3d < NVE4_3D_CLASS) {
         BEGIN_NVC0(push, NVC0_3D(BIND_TSC2(0)), 1);
         PUSH_DATA (push, (tsc->id << 12) | 1);
      }
   }

   if (new_view) {
      struct nv50_tic_entry *tic = nv50_tic_entry(new_view);
      assert(tic->id < 0);
      tic->id = nvc0_screen_tic_alloc(screen, tic);
      nvc0->base.push_data(&nvc0->base, screen->txc, tic->id * 32,
                           NV_VRAM_DOMAIN(&screen->base), 32, tic->tic);
      screen->tic.lock[tic->id / 32] |= 1 << (tic->id % 32);

      if (screen->base.class_3d >= NVE4_3D_CLASS) {
         BEGIN_NVC0(push, NVC0_3D(CB_SIZE), 3);
         PUSH_DATA (push, NVC0_CB_AUX_SIZE);
         PUSH_DATAh(push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(4));
         PUSH_DATA (push, screen->uniform_bo->offset + NVC0_CB_AUX_INFO(4));
         BEGIN_1IC0(push, NVC0_3D(CB_POS), 1 + 1);
         PUSH_DATA (push, NVC0_CB_AUX_FB_TEX_INFO);
         PUSH_DATA (push, (screen->default_tsc->id << 20) | tic->id);
      } else {
         BEGIN_NVC0(push, NVC0_3D(BIND_TIC2(0)), 1);
         PUSH_DATA (push, (tic->id << 9) | 1);
      }

      IMMED_NVC0(push, NVC0_3D(TIC_FLUSH), 0);
   }
}

static void
nvc0_switch_pipe_context(struct nvc0_context *ctx_to)
{
   struct nvc0_context *ctx_from = ctx_to->screen->cur_ctx;
   unsigned s;

   if (ctx_from)
      ctx_to->state = ctx_from->state;
   else
      ctx_to->state = ctx_to->screen->save_state;

   ctx_to->dirty_3d = ~0;
   ctx_to->dirty_cp = ~0;
   ctx_to->viewports_dirty = ~0;
   ctx_to->scissors_dirty = ~0;

   for (s = 0; s < 6; ++s) {
      ctx_to->samplers_dirty[s] = ~0;
      ctx_to->textures_dirty[s] = ~0;
      ctx_to->constbuf_dirty[s] = (1 << NVC0_MAX_PIPE_CONSTBUFS) - 1;
      ctx_to->buffers_dirty[s]  = ~0;
      ctx_to->images_dirty[s]   = ~0;
   }

   /* Reset tfb as the shader that owns it may have been deleted. */
   ctx_to->state.tfb = NULL;

   if (!ctx_to->vertex)
      ctx_to->dirty_3d &= ~(NVC0_NEW_3D_VERTEX | NVC0_NEW_3D_ARRAYS);

   if (!ctx_to->vertprog)
      ctx_to->dirty_3d &= ~NVC0_NEW_3D_VERTPROG;
   if (!ctx_to->fragprog)
      ctx_to->dirty_3d &= ~NVC0_NEW_3D_FRAGPROG;

   if (!ctx_to->blend)
      ctx_to->dirty_3d &= ~NVC0_NEW_3D_BLEND;
   if (!ctx_to->rast)
      ctx_to->dirty_3d &= ~(NVC0_NEW_3D_RASTERIZER | NVC0_NEW_3D_SCISSOR);
   if (!ctx_to->zsa)
      ctx_to->dirty_3d &= ~NVC0_NEW_3D_ZSA;

   ctx_to->screen->cur_ctx = ctx_to;
}

static struct nvc0_state_validate
validate_list_3d[] = {
    { nvc0_validate_fb,            NVC0_NEW_3D_FRAMEBUFFER },
    { nvc0_validate_blend,         NVC0_NEW_3D_BLEND },
    { nvc0_validate_zsa,           NVC0_NEW_3D_ZSA },
    { nvc0_validate_sample_mask,   NVC0_NEW_3D_SAMPLE_MASK },
    { nvc0_validate_rasterizer,    NVC0_NEW_3D_RASTERIZER },
    { nvc0_validate_blend_colour,  NVC0_NEW_3D_BLEND_COLOUR },
    { nvc0_validate_stencil_ref,   NVC0_NEW_3D_STENCIL_REF },
    { nvc0_validate_stipple,       NVC0_NEW_3D_STIPPLE },
    { nvc0_validate_scissor,       NVC0_NEW_3D_SCISSOR | NVC0_NEW_3D_RASTERIZER },
    { nvc0_validate_viewport,      NVC0_NEW_3D_VIEWPORT },
    { nvc0_validate_window_rects,  NVC0_NEW_3D_WINDOW_RECTS },
    { nvc0_vertprog_validate,      NVC0_NEW_3D_VERTPROG },
    { nvc0_tctlprog_validate,      NVC0_NEW_3D_TCTLPROG },
    { nvc0_tevlprog_validate,      NVC0_NEW_3D_TEVLPROG },
    { nvc0_validate_tess_state,    NVC0_NEW_3D_TESSFACTOR },
    { nvc0_gmtyprog_validate,      NVC0_NEW_3D_GMTYPROG },
    { nvc0_validate_min_samples,   NVC0_NEW_3D_MIN_SAMPLES |
                                   NVC0_NEW_3D_FRAGPROG |
                                   NVC0_NEW_3D_FRAMEBUFFER },
    { nvc0_fragprog_validate,      NVC0_NEW_3D_FRAGPROG | NVC0_NEW_3D_RASTERIZER },
    { nvc0_validate_fp_zsa_rast,   NVC0_NEW_3D_FRAGPROG | NVC0_NEW_3D_ZSA |
                                   NVC0_NEW_3D_RASTERIZER },
    { nvc0_validate_zsa_fb,        NVC0_NEW_3D_ZSA | NVC0_NEW_3D_FRAMEBUFFER },
    { nvc0_validate_rast_fb,       NVC0_NEW_3D_RASTERIZER | NVC0_NEW_3D_FRAMEBUFFER },
    { nvc0_validate_clip,          NVC0_NEW_3D_CLIP | NVC0_NEW_3D_RASTERIZER |
                                   NVC0_NEW_3D_VERTPROG |
                                   NVC0_NEW_3D_TEVLPROG |
                                   NVC0_NEW_3D_GMTYPROG },
    { nvc0_constbufs_validate,     NVC0_NEW_3D_CONSTBUF },
    { nvc0_validate_textures,      NVC0_NEW_3D_TEXTURES },
    { nvc0_validate_samplers,      NVC0_NEW_3D_SAMPLERS },
    { nve4_set_tex_handles,        NVC0_NEW_3D_TEXTURES | NVC0_NEW_3D_SAMPLERS },
    { nvc0_validate_fbread,        NVC0_NEW_3D_FRAGPROG |
                                   NVC0_NEW_3D_FRAMEBUFFER },
    { nvc0_vertex_arrays_validate, NVC0_NEW_3D_VERTEX | NVC0_NEW_3D_ARRAYS },
    { nvc0_validate_surfaces,      NVC0_NEW_3D_SURFACES },
    { nvc0_validate_buffers,       NVC0_NEW_3D_BUFFERS },
    { nvc0_tfb_validate,           NVC0_NEW_3D_TFB_TARGETS | NVC0_NEW_3D_GMTYPROG },
    { nvc0_layer_validate,         NVC0_NEW_3D_VERTPROG |
                                   NVC0_NEW_3D_TEVLPROG |
                                   NVC0_NEW_3D_GMTYPROG },
    { nvc0_validate_driverconst,   NVC0_NEW_3D_DRIVERCONST },
};

bool
nvc0_state_validate(struct nvc0_context *nvc0, uint32_t mask,
                    struct nvc0_state_validate *validate_list, int size,
                    uint32_t *dirty, struct nouveau_bufctx *bufctx)
{
   uint32_t state_mask;
   int ret;
   unsigned i;

   if (nvc0->screen->cur_ctx != nvc0)
      nvc0_switch_pipe_context(nvc0);

   state_mask = *dirty & mask;

   if (state_mask) {
      for (i = 0; i < size; ++i) {
         struct nvc0_state_validate *validate = &validate_list[i];

         if (state_mask & validate->states)
            validate->func(nvc0);
      }
      *dirty &= ~state_mask;

      nvc0_bufctx_fence(nvc0, bufctx, false);
   }

   nouveau_pushbuf_bufctx(nvc0->base.pushbuf, bufctx);
   ret = nouveau_pushbuf_validate(nvc0->base.pushbuf);

   return !ret;
}

bool
nvc0_state_validate_3d(struct nvc0_context *nvc0, uint32_t mask)
{
   bool ret;

   ret = nvc0_state_validate(nvc0, mask, validate_list_3d,
                             ARRAY_SIZE(validate_list_3d), &nvc0->dirty_3d,
                             nvc0->bufctx_3d);

   if (unlikely(nvc0->state.flushed)) {
      nvc0->state.flushed = false;
      nvc0_bufctx_fence(nvc0, nvc0->bufctx_3d, true);
   }
   return ret;
}
