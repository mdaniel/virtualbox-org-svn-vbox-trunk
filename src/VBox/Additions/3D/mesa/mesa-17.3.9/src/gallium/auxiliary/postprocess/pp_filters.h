/**************************************************************************
 *
 * Copyright 2011 Lauri Kasanen
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef PP_FILTERS_H
#define PP_FILTERS_H

/* Internal include, mainly for the filters */

#include "cso_cache/cso_context.h"
#include "pipe/p_context.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/p_state.h"
#include "tgsi/tgsi_text.h"
#include "util/u_memory.h"
#include "util/u_draw_quad.h"

#define PP_MAX_TOKENS 2048


/* Helper functions for the filters */

void pp_filter_setup_in(struct pp_program *, struct pipe_resource *);
void pp_filter_setup_out(struct pp_program *, struct pipe_resource *);
void pp_filter_end_pass(struct pp_program *);
void *pp_tgsi_to_state(struct pipe_context *, const char *, bool,
                       const char *);
void pp_filter_misc_state(struct pp_program *);
void pp_filter_draw(struct pp_program *);
void pp_filter_set_fb(struct pp_program *);
void pp_filter_set_clear_fb(struct pp_program *);


#endif
