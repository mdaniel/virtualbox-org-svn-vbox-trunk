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

#ifndef VC5_TILING_H
#define VC5_TILING_H

uint32_t vc5_utile_width(int cpp) ATTRIBUTE_CONST;
uint32_t vc5_utile_height(int cpp) ATTRIBUTE_CONST;
bool vc5_size_is_lt(uint32_t width, uint32_t height, int cpp) ATTRIBUTE_CONST;
void vc5_load_utile(void *dst, void *src, uint32_t dst_stride, uint32_t cpp);
void vc5_store_utile(void *dst, void *src, uint32_t src_stride, uint32_t cpp);
void vc5_load_tiled_image(void *dst, uint32_t dst_stride,
                          void *src, uint32_t src_stride,
                          enum vc5_tiling_mode tiling_format, int cpp,
                          uint32_t image_h,
                          const struct pipe_box *box);
void vc5_store_tiled_image(void *dst, uint32_t dst_stride,
                           void *src, uint32_t src_stride,
                           enum vc5_tiling_mode tiling_format, int cpp,
                           uint32_t image_h,
                           const struct pipe_box *box);

#endif /* VC5_TILING_H */
