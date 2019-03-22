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

/** @file vc5_tiling.c
 *
 * Handles information about the VC5 tiling formats, and loading and storing
 * from them.
 */

#include <stdint.h>
#include "vc5_screen.h"
#include "vc5_context.h"
#include "vc5_tiling.h"

struct mb_layout {
        /** Height, in pixels, of a macroblock (2x2 utiles, a UIF block). */
        uint32_t height;
        /** Width, in pixels, of a macroblock (2x2 utiles, a UIF block). */
        uint32_t width;
        uint32_t tile_row_stride;
};

enum {
        MB_LAYOUT_8BPP,
        MB_LAYOUT_16BPP,
        MB_LAYOUT_32BPP,
        MB_LAYOUT_64BPP,
        MB_LAYOUT_128BPP,
};

static const struct mb_layout mb_layouts[] = {
        [MB_LAYOUT_8BPP] = { .height = 16, .width = 16, .tile_row_stride = 8 },
        [MB_LAYOUT_16BPP] = { .height = 8, .width = 16, .tile_row_stride = 8 },
        [MB_LAYOUT_32BPP] = { .height = 8, .width = 8, .tile_row_stride = 4 },
        [MB_LAYOUT_64BPP] = { .height = 4, .width = 8, .tile_row_stride = 4 },
        [MB_LAYOUT_128BPP] = { .height = 4, .width = 4, .tile_row_stride = 2 },
};

static const struct mb_layout *
get_mb_layout(int cpp)
{
        const struct mb_layout *layout = &mb_layouts[ffs(cpp) - 1];

        /* Sanity check the table.  XXX: We should de-duplicate.  */
        assert(layout->width == vc5_utile_width(cpp) * 2);
        assert(layout->tile_row_stride == vc5_utile_width(cpp));

        return layout;
}

/** Return the width in pixels of a 64-byte microtile. */
uint32_t
vc5_utile_width(int cpp)
{
        switch (cpp) {
        case 1:
        case 2:
                return 8;
        case 4:
        case 8:
                return 4;
        case 16:
                return 2;
        default:
                unreachable("unknown cpp");
        }
}

/** Return the height in pixels of a 64-byte microtile. */
uint32_t
vc5_utile_height(int cpp)
{
        switch (cpp) {
        case 1:
                return 8;
        case 2:
        case 4:
                return 4;
        case 8:
        case 16:
                return 2;
        default:
                unreachable("unknown cpp");
        }
}

/**
 * Returns the byte address for a given pixel within a utile.
 *
 * Utiles are 64b blocks of pixels in raster order, with 32bpp being a 4x4
 * arrangement.
 */
static inline uint32_t
vc5_get_utile_pixel_offset(uint32_t cpp, uint32_t x, uint32_t y)
{
        uint32_t utile_w = vc5_utile_width(cpp);
        uint32_t utile_h = vc5_utile_height(cpp);

        assert(x < utile_w && y < utile_h);

        return x * cpp + y * utile_w * cpp;
}

/**
 * Returns the byte offset for a given pixel in a LINEARTILE layout.
 *
 * LINEARTILE is a single line of utiles in either the X or Y direction.
 */
static inline uint32_t
vc5_get_lt_pixel_offset(uint32_t cpp, uint32_t image_h, uint32_t x, uint32_t y)
{
        uint32_t utile_w = vc5_utile_width(cpp);
        uint32_t utile_h = vc5_utile_height(cpp);
        uint32_t utile_index_x = x / utile_w;
        uint32_t utile_index_y = y / utile_h;

        assert(utile_index_x == 0 || utile_index_y == 0);

        return (64 * (utile_index_x + utile_index_y) +
                vc5_get_utile_pixel_offset(cpp,
                                           x & (utile_w - 1),
                                           y & (utile_h - 1)));
}

/**
 * Returns the byte offset for a given pixel in a UBLINEAR layout.
 *
 * UBLINEAR is the layout where pixels are arranged in UIF blocks (2x2
 * utiles), and the UIF blocks are in 1 or 2 columns in raster order.
 */
static inline uint32_t
vc5_get_ublinear_pixel_offset(uint32_t cpp, uint32_t x, uint32_t y,
                              int ublinear_number)
{
        uint32_t utile_w = vc5_utile_width(cpp);
        uint32_t utile_h = vc5_utile_height(cpp);
        uint32_t ub_w = utile_w * 2;
        uint32_t ub_h = utile_h * 2;
        uint32_t ub_x = x / ub_w;
        uint32_t ub_y = y / ub_h;

        return (256 * (ub_y * ublinear_number +
                       ub_x) +
                ((x & utile_w) ? 64 : 0) +
                ((y & utile_h) ? 128 : 0) +
                + vc5_get_utile_pixel_offset(cpp,
                                             x & (utile_w - 1),
                                             y & (utile_h - 1)));
}

static inline uint32_t
vc5_get_ublinear_2_column_pixel_offset(uint32_t cpp, uint32_t image_h,
                                       uint32_t x, uint32_t y)
{
        return vc5_get_ublinear_pixel_offset(cpp, x, y, 2);
}

static inline uint32_t
vc5_get_ublinear_1_column_pixel_offset(uint32_t cpp, uint32_t image_h,
                                       uint32_t x, uint32_t y)
{
        return vc5_get_ublinear_pixel_offset(cpp, x, y, 1);
}

/**
 * Returns the byte offset for a given pixel in a UIF layout.
 *
 * UIF is the general VC5 tiling layout shared across 3D, media, and scanout.
 * It stores pixels in UIF blocks (2x2 utiles), and UIF blocks are stored in
 * 4x4 groups, and those 4x4 groups are then stored in raster order.
 */
static inline uint32_t
vc5_get_uif_pixel_offset(uint32_t cpp, uint32_t image_h, uint32_t x, uint32_t y)
{
        const struct mb_layout *layout = get_mb_layout(cpp);
        uint32_t mb_width = layout->width;
        uint32_t mb_height = layout->height;
        uint32_t log2_mb_width = ffs(mb_width) - 1;
        uint32_t log2_mb_height = ffs(mb_height) - 1;

        /* Macroblock X, y */
        uint32_t mb_x = x >> log2_mb_width;
        uint32_t mb_y = y >> log2_mb_height;
        /* X, y within the macroblock */
        uint32_t mb_pixel_x = x - (mb_x << log2_mb_width);
        uint32_t mb_pixel_y = y - (mb_y << log2_mb_height);

        uint32_t mb_h = align(image_h, 1 << log2_mb_height) >> log2_mb_height;
        uint32_t mb_id = ((mb_x / 4) * ((mb_h - 1) * 4)) + mb_x + mb_y * 4;

        uint32_t mb_base_addr = mb_id * 256;

        bool top = mb_pixel_y < mb_height / 2;
        bool left = mb_pixel_x < mb_width / 2;

        /* Docs have this in pixels, we do bytes here. */
        uint32_t mb_tile_offset = (!top * 128 + !left * 64);

        uint32_t mb_tile_y = mb_pixel_y & ~(mb_height / 2);
        uint32_t mb_tile_x = mb_pixel_x & ~(mb_width / 2);
        uint32_t mb_tile_pixel_id = (mb_tile_y *
                                     layout->tile_row_stride +
                                     mb_tile_x);

        uint32_t mb_tile_addr = mb_tile_pixel_id * cpp;

        uint32_t mb_pixel_address = (mb_base_addr +
                                     mb_tile_offset +
                                     mb_tile_addr);

        return mb_pixel_address;
}

static inline void
vc5_move_pixels_general_percpp(void *gpu, uint32_t gpu_stride,
                               void *cpu, uint32_t cpu_stride,
                               int cpp, uint32_t image_h,
                               const struct pipe_box *box,
                               uint32_t (*get_pixel_offset)(uint32_t cpp,
                                                            uint32_t image_h,
                                                            uint32_t x, uint32_t y),
                               bool is_load)
{
        for (uint32_t y = 0; y < box->height; y++) {
                void *cpu_row = cpu + y * cpu_stride;

                for (int x = 0; x < box->width; x++) {
                        uint32_t pixel_offset = get_pixel_offset(cpp, image_h,
                                                                 box->x + x,
                                                                 box->y + y);

                        if (false) {
                                fprintf(stderr, "%3d,%3d -> %d\n",
                                        box->x + x, box->y + y,
                                        pixel_offset);
                        }

                        if (is_load) {
                                memcpy(cpu_row + x * cpp,
                                       gpu + pixel_offset,
                                       cpp);
                        } else {
                                memcpy(gpu + pixel_offset,
                                       cpu_row + x * cpp,
                                       cpp);
                        }
                }
        }
}

static inline void
vc5_move_pixels_general(void *gpu, uint32_t gpu_stride,
                               void *cpu, uint32_t cpu_stride,
                               int cpp, uint32_t image_h,
                               const struct pipe_box *box,
                               uint32_t (*get_pixel_offset)(uint32_t cpp,
                                                            uint32_t image_h,
                                                            uint32_t x, uint32_t y),
                               bool is_load)
{
        switch (cpp) {
        case 1:
                vc5_move_pixels_general_percpp(gpu, gpu_stride,
                                               cpu, cpu_stride,
                                               1, image_h, box,
                                               get_pixel_offset,
                                               is_load);
                break;
        case 2:
                vc5_move_pixels_general_percpp(gpu, gpu_stride,
                                               cpu, cpu_stride,
                                               2, image_h, box,
                                               get_pixel_offset,
                                               is_load);
                break;
        case 4:
                vc5_move_pixels_general_percpp(gpu, gpu_stride,
                                               cpu, cpu_stride,
                                               4, image_h, box,
                                               get_pixel_offset,
                                               is_load);
                break;
        case 8:
                vc5_move_pixels_general_percpp(gpu, gpu_stride,
                                               cpu, cpu_stride,
                                               8, image_h, box,
                                               get_pixel_offset,
                                               is_load);
                break;
        case 16:
                vc5_move_pixels_general_percpp(gpu, gpu_stride,
                                               cpu, cpu_stride,
                                               16, image_h, box,
                                               get_pixel_offset,
                                               is_load);
                break;
        }
}

static inline void
vc5_move_tiled_image(void *gpu, uint32_t gpu_stride,
                     void *cpu, uint32_t cpu_stride,
                     enum vc5_tiling_mode tiling_format,
                     int cpp,
                     uint32_t image_h,
                     const struct pipe_box *box,
                     bool is_load)
{
        switch (tiling_format) {
        case VC5_TILING_UIF_NO_XOR:
                vc5_move_pixels_general(gpu, gpu_stride,
                                        cpu, cpu_stride,
                                        cpp, image_h, box,
                                        vc5_get_uif_pixel_offset,
                                        is_load);
                break;
        case VC5_TILING_UBLINEAR_2_COLUMN:
                vc5_move_pixels_general(gpu, gpu_stride,
                                        cpu, cpu_stride,
                                        cpp, image_h, box,
                                        vc5_get_ublinear_2_column_pixel_offset,
                                        is_load);
                break;
        case VC5_TILING_UBLINEAR_1_COLUMN:
                vc5_move_pixels_general(gpu, gpu_stride,
                                        cpu, cpu_stride,
                                        cpp, image_h, box,
                                        vc5_get_ublinear_1_column_pixel_offset,
                                        is_load);
                break;
        case VC5_TILING_LINEARTILE:
                vc5_move_pixels_general(gpu, gpu_stride,
                                        cpu, cpu_stride,
                                        cpp, image_h, box,
                                        vc5_get_lt_pixel_offset,
                                        is_load);
                break;
        default:
                unreachable("Unsupported tiling format");
                break;
        }
}

/**
 * Loads pixel data from the start (microtile-aligned) box in \p src to the
 * start of \p dst according to the given tiling format.
 */
void
vc5_load_tiled_image(void *dst, uint32_t dst_stride,
                     void *src, uint32_t src_stride,
                     enum vc5_tiling_mode tiling_format, int cpp,
                     uint32_t image_h,
                     const struct pipe_box *box)
{
        vc5_move_tiled_image(src, src_stride,
                             dst, dst_stride,
                             tiling_format,
                             cpp,
                             image_h,
                             box,
                             true);
}

/**
 * Stores pixel data from the start of \p src into a (microtile-aligned) box in
 * \p dst according to the given tiling format.
 */
void
vc5_store_tiled_image(void *dst, uint32_t dst_stride,
                      void *src, uint32_t src_stride,
                      enum vc5_tiling_mode tiling_format, int cpp,
                      uint32_t image_h,
                      const struct pipe_box *box)
{
        vc5_move_tiled_image(dst, dst_stride,
                             src, src_stride,
                             tiling_format,
                             cpp,
                             image_h,
                             box,
                             false);
}
