/*
 * Copyright © 2014 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * Authors:
 *   Marek Olšák <maraeo@gmail.com>
 */

#include "radeon_drm_winsys.h"
#include "util/u_format.h"
#include <radeon_surface.h>

static unsigned cik_get_macro_tile_index(struct radeon_surf *surf)
{
	unsigned index, tileb;

	tileb = 8 * 8 * surf->bpe;
	tileb = MIN2(surf->u.legacy.tile_split, tileb);

	for (index = 0; tileb > 64; index++)
		tileb >>= 1;

	assert(index < 16);
	return index;
}

#define   G_009910_MICRO_TILE_MODE(x)          (((x) >> 0) & 0x03)
#define   G_009910_MICRO_TILE_MODE_NEW(x)      (((x) >> 22) & 0x07)

static void set_micro_tile_mode(struct radeon_surf *surf,
                                struct radeon_info *info)
{
    uint32_t tile_mode;

    if (info->chip_class < SI) {
        surf->micro_tile_mode = 0;
        return;
    }

    tile_mode = info->si_tile_mode_array[surf->u.legacy.tiling_index[0]];

    if (info->chip_class >= CIK)
        surf->micro_tile_mode = G_009910_MICRO_TILE_MODE_NEW(tile_mode);
    else
        surf->micro_tile_mode = G_009910_MICRO_TILE_MODE(tile_mode);
}

static void surf_level_winsys_to_drm(struct radeon_surface_level *level_drm,
                                     const struct legacy_surf_level *level_ws,
                                     unsigned bpe)
{
    level_drm->offset = level_ws->offset;
    level_drm->slice_size = level_ws->slice_size;
    level_drm->nblk_x = level_ws->nblk_x;
    level_drm->nblk_y = level_ws->nblk_y;
    level_drm->pitch_bytes = level_ws->nblk_x * bpe;
    level_drm->mode = level_ws->mode;
}

static void surf_level_drm_to_winsys(struct legacy_surf_level *level_ws,
                                     const struct radeon_surface_level *level_drm,
                                     unsigned bpe)
{
    level_ws->offset = level_drm->offset;
    level_ws->slice_size = level_drm->slice_size;
    level_ws->nblk_x = level_drm->nblk_x;
    level_ws->nblk_y = level_drm->nblk_y;
    level_ws->mode = level_drm->mode;
    assert(level_drm->nblk_x * bpe == level_drm->pitch_bytes);
}

static void surf_winsys_to_drm(struct radeon_surface *surf_drm,
                               const struct pipe_resource *tex,
                               unsigned flags, unsigned bpe,
                               enum radeon_surf_mode mode,
                               const struct radeon_surf *surf_ws)
{
    int i;

    memset(surf_drm, 0, sizeof(*surf_drm));

    surf_drm->npix_x = tex->width0;
    surf_drm->npix_y = tex->height0;
    surf_drm->npix_z = tex->depth0;
    surf_drm->blk_w = util_format_get_blockwidth(tex->format);
    surf_drm->blk_h = util_format_get_blockheight(tex->format);
    surf_drm->blk_d = 1;
    surf_drm->array_size = 1;
    surf_drm->last_level = tex->last_level;
    surf_drm->bpe = bpe;
    surf_drm->nsamples = tex->nr_samples ? tex->nr_samples : 1;

    surf_drm->flags = flags;
    surf_drm->flags = RADEON_SURF_CLR(surf_drm->flags, TYPE);
    surf_drm->flags = RADEON_SURF_CLR(surf_drm->flags, MODE);
    surf_drm->flags |= RADEON_SURF_SET(mode, MODE) |
                       RADEON_SURF_HAS_SBUFFER_MIPTREE |
                       RADEON_SURF_HAS_TILE_MODE_INDEX;

    switch (tex->target) {
    case PIPE_TEXTURE_1D:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D, TYPE);
            break;
    case PIPE_TEXTURE_RECT:
    case PIPE_TEXTURE_2D:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D, TYPE);
            break;
    case PIPE_TEXTURE_3D:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_3D, TYPE);
            break;
    case PIPE_TEXTURE_1D_ARRAY:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_1D_ARRAY, TYPE);
            surf_drm->array_size = tex->array_size;
            break;
    case PIPE_TEXTURE_CUBE_ARRAY: /* cube array layout like 2d array */
            assert(tex->array_size % 6 == 0);
            /* fall through */
    case PIPE_TEXTURE_2D_ARRAY:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_2D_ARRAY, TYPE);
            surf_drm->array_size = tex->array_size;
            break;
    case PIPE_TEXTURE_CUBE:
            surf_drm->flags |= RADEON_SURF_SET(RADEON_SURF_TYPE_CUBEMAP, TYPE);
            break;
    case PIPE_BUFFER:
    default:
            assert(0);
    }

    surf_drm->bo_size = surf_ws->surf_size;
    surf_drm->bo_alignment = surf_ws->surf_alignment;

    surf_drm->bankw = surf_ws->u.legacy.bankw;
    surf_drm->bankh = surf_ws->u.legacy.bankh;
    surf_drm->mtilea = surf_ws->u.legacy.mtilea;
    surf_drm->tile_split = surf_ws->u.legacy.tile_split;

    for (i = 0; i <= surf_drm->last_level; i++) {
        surf_level_winsys_to_drm(&surf_drm->level[i], &surf_ws->u.legacy.level[i],
                                 bpe * surf_drm->nsamples);

        surf_drm->tiling_index[i] = surf_ws->u.legacy.tiling_index[i];
    }

    if (flags & RADEON_SURF_SBUFFER) {
        surf_drm->stencil_tile_split = surf_ws->u.legacy.stencil_tile_split;

        for (i = 0; i <= surf_drm->last_level; i++) {
            surf_level_winsys_to_drm(&surf_drm->stencil_level[i],
                                     &surf_ws->u.legacy.stencil_level[i],
                                     surf_drm->nsamples);
            surf_drm->stencil_tiling_index[i] = surf_ws->u.legacy.stencil_tiling_index[i];
        }
    }
}

static void surf_drm_to_winsys(struct radeon_drm_winsys *ws,
                               struct radeon_surf *surf_ws,
                               const struct radeon_surface *surf_drm)
{
    int i;

    memset(surf_ws, 0, sizeof(*surf_ws));

    surf_ws->blk_w = surf_drm->blk_w;
    surf_ws->blk_h = surf_drm->blk_h;
    surf_ws->bpe = surf_drm->bpe;
    surf_ws->is_linear = surf_drm->level[0].mode <= RADEON_SURF_MODE_LINEAR_ALIGNED;
    surf_ws->has_stencil = !!(surf_drm->flags & RADEON_SURF_SBUFFER);
    surf_ws->flags = surf_drm->flags;

    surf_ws->surf_size = surf_drm->bo_size;
    surf_ws->surf_alignment = surf_drm->bo_alignment;

    surf_ws->u.legacy.bankw = surf_drm->bankw;
    surf_ws->u.legacy.bankh = surf_drm->bankh;
    surf_ws->u.legacy.mtilea = surf_drm->mtilea;
    surf_ws->u.legacy.tile_split = surf_drm->tile_split;

    surf_ws->u.legacy.macro_tile_index = cik_get_macro_tile_index(surf_ws);

    for (i = 0; i <= surf_drm->last_level; i++) {
        surf_level_drm_to_winsys(&surf_ws->u.legacy.level[i], &surf_drm->level[i],
                                 surf_drm->bpe * surf_drm->nsamples);
        surf_ws->u.legacy.tiling_index[i] = surf_drm->tiling_index[i];
    }

    if (surf_ws->flags & RADEON_SURF_SBUFFER) {
        surf_ws->u.legacy.stencil_tile_split = surf_drm->stencil_tile_split;

        for (i = 0; i <= surf_drm->last_level; i++) {
            surf_level_drm_to_winsys(&surf_ws->u.legacy.stencil_level[i],
                                     &surf_drm->stencil_level[i],
                                     surf_drm->nsamples);
            surf_ws->u.legacy.stencil_tiling_index[i] = surf_drm->stencil_tiling_index[i];
        }
    }

    set_micro_tile_mode(surf_ws, &ws->info);
    surf_ws->is_displayable = surf_ws->is_linear ||
			      surf_ws->micro_tile_mode == RADEON_MICRO_MODE_DISPLAY ||
			      surf_ws->micro_tile_mode == RADEON_MICRO_MODE_ROTATED;
}

static int radeon_winsys_surface_init(struct radeon_winsys *rws,
                                      const struct pipe_resource *tex,
                                      unsigned flags, unsigned bpe,
                                      enum radeon_surf_mode mode,
                                      struct radeon_surf *surf_ws)
{
    struct radeon_drm_winsys *ws = (struct radeon_drm_winsys*)rws;
    struct radeon_surface surf_drm;
    int r;

    surf_winsys_to_drm(&surf_drm, tex, flags, bpe, mode, surf_ws);

    if (!(flags & (RADEON_SURF_IMPORTED | RADEON_SURF_FMASK))) {
       r = radeon_surface_best(ws->surf_man, &surf_drm);
       if (r)
          return r;
    }

    r = radeon_surface_init(ws->surf_man, &surf_drm);
    if (r)
        return r;

    surf_drm_to_winsys(ws, surf_ws, &surf_drm);
    return 0;
}

void radeon_surface_init_functions(struct radeon_drm_winsys *ws)
{
    ws->base.surface_init = radeon_winsys_surface_init;
}
