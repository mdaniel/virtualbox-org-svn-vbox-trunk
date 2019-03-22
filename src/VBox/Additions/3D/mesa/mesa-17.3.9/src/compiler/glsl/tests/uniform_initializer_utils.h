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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef GLSL_UNIFORM_INITIALIZER_UTILS_H
#define GLSL_UNIFORM_INITIALIZER_UTILS_H

#include "program/prog_parameter.h"
#include "ir.h"
#include "ir_uniform.h"

extern void
fill_storage_array_with_sentinels(gl_constant_value *storage,
				  unsigned data_size,
				  unsigned red_zone_size);

extern void
generate_data(void *mem_ctx, enum glsl_base_type base_type,
	      unsigned columns, unsigned rows,
	      ir_constant *&val);

extern void
generate_array_data(void *mem_ctx, enum glsl_base_type base_type,
		    unsigned columns, unsigned rows, unsigned array_size,
		    ir_constant *&val);

extern void
verify_data(gl_constant_value *storage, unsigned storage_array_size,
            ir_constant *val, unsigned red_zone_size,
            unsigned int boolean_true);

#endif /* GLSL_UNIFORM_INITIALIZER_UTILS_H */
