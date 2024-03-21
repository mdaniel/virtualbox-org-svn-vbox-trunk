/*
 * Copyright © 2019 Intel Corporation
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

#ifndef OVERLAY_PARAMS_H
#define OVERLAY_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define OVERLAY_PARAMS                               \
   OVERLAY_PARAM_BOOL(device)                        \
   OVERLAY_PARAM_BOOL(format)                        \
   OVERLAY_PARAM_BOOL(fps)                           \
   OVERLAY_PARAM_BOOL(frame)                         \
   OVERLAY_PARAM_BOOL(frame_timing)                  \
   OVERLAY_PARAM_BOOL(submit)                        \
   OVERLAY_PARAM_BOOL(draw)                          \
   OVERLAY_PARAM_BOOL(draw_indexed)                  \
   OVERLAY_PARAM_BOOL(draw_indirect)                 \
   OVERLAY_PARAM_BOOL(draw_indexed_indirect)         \
   OVERLAY_PARAM_BOOL(draw_indirect_count)           \
   OVERLAY_PARAM_BOOL(draw_indexed_indirect_count)   \
   OVERLAY_PARAM_BOOL(dispatch)                      \
   OVERLAY_PARAM_BOOL(dispatch_indirect)             \
   OVERLAY_PARAM_BOOL(pipeline_graphics)             \
   OVERLAY_PARAM_BOOL(pipeline_compute)              \
   OVERLAY_PARAM_BOOL(pipeline_raytracing)           \
   OVERLAY_PARAM_BOOL(acquire)                       \
   OVERLAY_PARAM_BOOL(acquire_timing)                \
   OVERLAY_PARAM_BOOL(present_timing)                \
   OVERLAY_PARAM_BOOL(vertices)                      \
   OVERLAY_PARAM_BOOL(primitives)                    \
   OVERLAY_PARAM_BOOL(vert_invocations)              \
   OVERLAY_PARAM_BOOL(geom_invocations)              \
   OVERLAY_PARAM_BOOL(geom_primitives)               \
   OVERLAY_PARAM_BOOL(clip_invocations)              \
   OVERLAY_PARAM_BOOL(clip_primitives)               \
   OVERLAY_PARAM_BOOL(frag_invocations)              \
   OVERLAY_PARAM_BOOL(tess_ctrl_patches)             \
   OVERLAY_PARAM_BOOL(tess_eval_invocations)         \
   OVERLAY_PARAM_BOOL(compute_invocations)           \
   OVERLAY_PARAM_BOOL(gpu_timing)                    \
   OVERLAY_PARAM_CUSTOM(fps_sampling_period)         \
   OVERLAY_PARAM_CUSTOM(output_file)                 \
   OVERLAY_PARAM_CUSTOM(position)                    \
   OVERLAY_PARAM_CUSTOM(width)                       \
   OVERLAY_PARAM_CUSTOM(height)                      \
   OVERLAY_PARAM_CUSTOM(no_display)                  \
   OVERLAY_PARAM_CUSTOM(control)                     \
   OVERLAY_PARAM_CUSTOM(help)

enum overlay_param_position {
   LAYER_POSITION_TOP_LEFT,
   LAYER_POSITION_TOP_RIGHT,
   LAYER_POSITION_BOTTOM_LEFT,
   LAYER_POSITION_BOTTOM_RIGHT,
};

enum overlay_param_enabled {
#define OVERLAY_PARAM_BOOL(name) OVERLAY_PARAM_ENABLED_##name,
#define OVERLAY_PARAM_CUSTOM(name)
   OVERLAY_PARAMS
#undef OVERLAY_PARAM_BOOL
#undef OVERLAY_PARAM_CUSTOM
   OVERLAY_PARAM_ENABLED_MAX
};

struct overlay_params {
   bool enabled[OVERLAY_PARAM_ENABLED_MAX];
   enum overlay_param_position position;
   FILE *output_file;
   int control;
   uint32_t fps_sampling_period; /* us */
   bool help;
   bool no_display;
   unsigned width;
   unsigned height;
};

const extern char *overlay_param_names[];

void parse_overlay_env(struct overlay_params *params,
                       const char *env);

#ifdef __cplusplus
}
#endif

#endif /* OVERLAY_PARAMS_H */
