/*
 * Copyright © 2014 Intel Corporation
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
 *
 * Authors:
 *    Jason Ekstrand (jason@jlekstrand.net)
 *
 */

#ifndef _NIR_SEARCH_
#define _NIR_SEARCH_

#include "nir.h"

#define NIR_SEARCH_MAX_VARIABLES 16

typedef enum {
   nir_search_value_expression,
   nir_search_value_variable,
   nir_search_value_constant,
} nir_search_value_type;

typedef struct {
   nir_search_value_type type;

   unsigned bit_size;
} nir_search_value;

typedef struct {
   nir_search_value value;

   /** The variable index;  Must be less than NIR_SEARCH_MAX_VARIABLES */
   unsigned variable;

   /** Indicates that the given variable must be a constant
    *
    * This is only allowed in search expressions and indicates that the
    * given variable is only allowed to match constant values.
    */
   bool is_constant;

   /** Indicates that the given variable must have a certain type
    *
    * This is only allowed in search expressions and indicates that the
    * given variable is only allowed to match values that come from an ALU
    * instruction with the given output type.  A type of nir_type_void
    * means it can match any type.
    *
    * Note: A variable that is both constant and has a non-void type will
    * never match anything.
    */
   nir_alu_type type;

   /** Optional condition fxn ptr
    *
    * This is only allowed in search expressions, and allows additional
    * constraints to be placed on the match.  Typically used for 'is_constant'
    * variables to require, for example, power-of-two in order for the search
    * to match.
    */
   bool (*cond)(nir_alu_instr *instr, unsigned src,
                unsigned num_components, const uint8_t *swizzle);
} nir_search_variable;

typedef struct {
   nir_search_value value;

   nir_alu_type type;

   union {
      uint64_t u;
      int64_t i;
      double d;
   } data;
} nir_search_constant;

typedef struct {
   nir_search_value value;

   /* When set on a search expression, the expression will only match an SSA
    * value that does *not* have the exact bit set.  If unset, the exact bit
    * on the SSA value is ignored.
    */
   bool inexact;

   nir_op opcode;
   const nir_search_value *srcs[4];

   /** Optional condition fxn ptr
    *
    * This allows additional constraints on expression matching, it is
    * typically used to match an expressions uses such as the number of times
    * the expression is used, and whether its used by an if.
    */
   bool (*cond)(nir_alu_instr *instr);
} nir_search_expression;

NIR_DEFINE_CAST(nir_search_value_as_variable, nir_search_value,
                nir_search_variable, value,
                type, nir_search_value_variable)
NIR_DEFINE_CAST(nir_search_value_as_constant, nir_search_value,
                nir_search_constant, value,
                type, nir_search_value_constant)
NIR_DEFINE_CAST(nir_search_value_as_expression, nir_search_value,
                nir_search_expression, value,
                type, nir_search_value_expression)

nir_alu_instr *
nir_replace_instr(nir_alu_instr *instr, const nir_search_expression *search,
                  const nir_search_value *replace, void *mem_ctx);

#endif /* _NIR_SEARCH_ */
