/*
 * Copyright © 2012 Intel Corporation
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

#ifndef BLORP_H
#define BLORP_H

#include <stdint.h>
#include <stdbool.h>

#include "isl/isl.h"

struct brw_stage_prog_data;

#ifdef __cplusplus
extern "C" {
#endif

struct blorp_batch;
struct blorp_params;

struct blorp_context {
   void *driver_ctx;

   const struct isl_device *isl_dev;

   const struct brw_compiler *compiler;

   bool (*lookup_shader)(struct blorp_context *blorp,
                         const void *key, uint32_t key_size,
                         uint32_t *kernel_out, void *prog_data_out);
   bool (*upload_shader)(struct blorp_context *blorp,
                         const void *key, uint32_t key_size,
                         const void *kernel, uint32_t kernel_size,
                         const struct brw_stage_prog_data *prog_data,
                         uint32_t prog_data_size,
                         uint32_t *kernel_out, void *prog_data_out);
   void (*exec)(struct blorp_batch *batch, const struct blorp_params *params);
};

void blorp_init(struct blorp_context *blorp, void *driver_ctx,
                struct isl_device *isl_dev);
void blorp_finish(struct blorp_context *blorp);

enum blorp_batch_flags {
   /**
    * This flag indicates that blorp should *not* re-emit the depth and
    * stencil buffer packets.  Instead, the driver guarantees that all depth
    * and stencil images passed in will match what is currently set in the
    * hardware.
    */
   BLORP_BATCH_NO_EMIT_DEPTH_STENCIL = (1 << 0),

   /* This flag indicates that the blorp call should be predicated. */
   BLORP_BATCH_PREDICATE_ENABLE      = (1 << 1),
};

struct blorp_batch {
   struct blorp_context *blorp;
   void *driver_batch;
   enum blorp_batch_flags flags;
};

void blorp_batch_init(struct blorp_context *blorp, struct blorp_batch *batch,
                      void *driver_batch, enum blorp_batch_flags flags);
void blorp_batch_finish(struct blorp_batch *batch);

struct blorp_address {
   void *buffer;
   unsigned reloc_flags;
   uint32_t offset;
   uint32_t mocs;
};

struct blorp_surf
{
   const struct isl_surf *surf;
   struct blorp_address addr;

   const struct isl_surf *aux_surf;
   struct blorp_address aux_addr;
   enum isl_aux_usage aux_usage;

   union isl_color_value clear_color;
};

void
blorp_blit(struct blorp_batch *batch,
           const struct blorp_surf *src_surf,
           unsigned src_level, unsigned src_layer,
           enum isl_format src_format, struct isl_swizzle src_swizzle,
           const struct blorp_surf *dst_surf,
           unsigned dst_level, unsigned dst_layer,
           enum isl_format dst_format, struct isl_swizzle dst_swizzle,
           float src_x0, float src_y0,
           float src_x1, float src_y1,
           float dst_x0, float dst_y0,
           float dst_x1, float dst_y1,
           uint32_t filter, bool mirror_x, bool mirror_y);

void
blorp_copy(struct blorp_batch *batch,
           const struct blorp_surf *src_surf,
           unsigned src_level, unsigned src_layer,
           const struct blorp_surf *dst_surf,
           unsigned dst_level, unsigned dst_layer,
           uint32_t src_x, uint32_t src_y,
           uint32_t dst_x, uint32_t dst_y,
           uint32_t src_width, uint32_t src_height);

void
blorp_buffer_copy(struct blorp_batch *batch,
                  struct blorp_address src,
                  struct blorp_address dst,
                  uint64_t size);

void
blorp_fast_clear(struct blorp_batch *batch,
                 const struct blorp_surf *surf, enum isl_format format,
                 uint32_t level, uint32_t start_layer, uint32_t num_layers,
                 uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);

void
blorp_clear(struct blorp_batch *batch,
            const struct blorp_surf *surf,
            enum isl_format format, struct isl_swizzle swizzle,
            uint32_t level, uint32_t start_layer, uint32_t num_layers,
            uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
            union isl_color_value clear_color,
            const bool color_write_disable[4]);

void
blorp_clear_depth_stencil(struct blorp_batch *batch,
                          const struct blorp_surf *depth,
                          const struct blorp_surf *stencil,
                          uint32_t level, uint32_t start_layer,
                          uint32_t num_layers,
                          uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                          bool clear_depth, float depth_value,
                          uint8_t stencil_mask, uint8_t stencil_value);
bool
blorp_can_hiz_clear_depth(uint8_t gen, enum isl_format format,
                          uint32_t num_samples,
                          uint32_t x0, uint32_t y0,
                          uint32_t x1, uint32_t y1);

void
blorp_gen8_hiz_clear_attachments(struct blorp_batch *batch,
                                 uint32_t num_samples,
                                 uint32_t x0, uint32_t y0,
                                 uint32_t x1, uint32_t y1,
                                 bool clear_depth, bool clear_stencil,
                                 uint8_t stencil_value);
void
blorp_clear_attachments(struct blorp_batch *batch,
                        uint32_t binding_table_offset,
                        enum isl_format depth_format,
                        uint32_t num_samples,
                        uint32_t start_layer, uint32_t num_layers,
                        uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                        bool clear_color, union isl_color_value color_value,
                        bool clear_depth, float depth_value,
                        uint8_t stencil_mask, uint8_t stencil_value);

enum blorp_fast_clear_op {
   BLORP_FAST_CLEAR_OP_NONE = 0,
   BLORP_FAST_CLEAR_OP_CLEAR,
   BLORP_FAST_CLEAR_OP_RESOLVE_PARTIAL,
   BLORP_FAST_CLEAR_OP_RESOLVE_FULL,
};

void
blorp_ccs_resolve(struct blorp_batch *batch,
                  struct blorp_surf *surf, uint32_t level, uint32_t layer,
                  enum isl_format format,
                  enum blorp_fast_clear_op resolve_op);

/* Resolves subresources of the image subresource range specified in the
 * binding table.
 */
void
blorp_ccs_resolve_attachment(struct blorp_batch *batch,
                             const uint32_t binding_table_offset,
                             struct blorp_surf * const surf,
                             const uint32_t level, const uint32_t num_layers,
                             const enum isl_format format,
                             const enum blorp_fast_clear_op resolve_op);

void
blorp_mcs_partial_resolve(struct blorp_batch *batch,
                          struct blorp_surf *surf,
                          enum isl_format format,
                          uint32_t start_layer, uint32_t num_layers);

/**
 * For an overview of the HiZ operations, see the following sections of the
 * Sandy Bridge PRM, Volume 1, Part2:
 *   - 7.5.3.1 Depth Buffer Clear
 *   - 7.5.3.2 Depth Buffer Resolve
 *   - 7.5.3.3 Hierarchical Depth Buffer Resolve
 *
 * Of these, two get entered in the resolve map as needing to be done to the
 * buffer: depth resolve and hiz resolve.
 */
enum blorp_hiz_op {
   BLORP_HIZ_OP_NONE,
   BLORP_HIZ_OP_DEPTH_CLEAR,
   BLORP_HIZ_OP_DEPTH_RESOLVE,
   BLORP_HIZ_OP_HIZ_RESOLVE,
};

void
blorp_hiz_op(struct blorp_batch *batch, struct blorp_surf *surf,
             uint32_t level, uint32_t start_layer, uint32_t num_layers,
             enum blorp_hiz_op op);

#ifdef __cplusplus
} /* end extern "C" */
#endif /* __cplusplus */

#endif /* BLORP_H */
