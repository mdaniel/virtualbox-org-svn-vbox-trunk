/*
 * Copyright © 2018 Intel Corporation
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
#include "nir_range_analysis.h"
#include <float.h>
#include <math.h>
#include "util/hash_table.h"
#include "util/u_dynarray.h"
#include "util/u_math.h"
#include "c99_alloca.h"
#include "nir.h"

/**
 * Analyzes a sequence of operations to determine some aspects of the range of
 * the result.
 */

struct analysis_query {
   uint32_t pushed_queries;
   uint32_t result_index;
};

struct analysis_state {
   nir_shader *shader;
   const nir_unsigned_upper_bound_config *config;
   struct hash_table *range_ht;

   struct util_dynarray query_stack;
   struct util_dynarray result_stack;

   size_t query_size;
   uintptr_t (*get_key)(struct analysis_query *q);
   void (*process_query)(struct analysis_state *state, struct analysis_query *q,
                         uint32_t *result, const uint32_t *src);
};

static void *
push_analysis_query(struct analysis_state *state, size_t size)
{
   struct analysis_query *q = util_dynarray_grow_bytes(&state->query_stack, 1, size);
   q->pushed_queries = 0;
   q->result_index = util_dynarray_num_elements(&state->result_stack, uint32_t);

   util_dynarray_append(&state->result_stack, uint32_t, 0);

   return q;
}

/* Helper for performing range analysis without recursion. */
static uint32_t
perform_analysis(struct analysis_state *state)
{
   while (state->query_stack.size) {
      struct analysis_query *cur =
         (struct analysis_query *)((char *)util_dynarray_end(&state->query_stack) - state->query_size);
      uint32_t *result = util_dynarray_element(&state->result_stack, uint32_t, cur->result_index);

      uintptr_t key = state->get_key(cur);
      struct hash_entry *he = NULL;
      /* There might be a cycle-resolving entry for loop header phis. Ignore this when finishing
       * them by testing pushed_queries.
       */
      if (cur->pushed_queries == 0 && key &&
          (he = _mesa_hash_table_search(state->range_ht, (void *)key))) {
         *result = (uintptr_t)he->data;
         state->query_stack.size -= state->query_size;
         continue;
      }

      uint32_t *src = (uint32_t *)util_dynarray_end(&state->result_stack) - cur->pushed_queries;
      state->result_stack.size -= sizeof(uint32_t) * cur->pushed_queries;

      uint32_t prev_num_queries = state->query_stack.size;
      state->process_query(state, cur, result, src);

      uint32_t num_queries = state->query_stack.size;
      if (num_queries > prev_num_queries) {
         cur = (struct analysis_query *)util_dynarray_element(&state->query_stack, char,
                                                              prev_num_queries - state->query_size);
         cur->pushed_queries = (num_queries - prev_num_queries) / state->query_size;
         continue;
      }

      if (key)
         _mesa_hash_table_insert(state->range_ht, (void *)key, (void *)(uintptr_t)*result);

      state->query_stack.size -= state->query_size;
   }

   assert(state->result_stack.size == sizeof(uint32_t));

   uint32_t res = util_dynarray_top(&state->result_stack, uint32_t);
   util_dynarray_fini(&state->query_stack);
   util_dynarray_fini(&state->result_stack);

   return res;
}

static bool
is_not_negative(enum ssa_ranges r)
{
   return r == gt_zero || r == ge_zero || r == eq_zero;
}

static bool
is_not_zero(enum ssa_ranges r)
{
   return r == gt_zero || r == lt_zero || r == ne_zero;
}

static uint32_t
pack_data(const struct ssa_result_range r)
{
   return r.range | r.is_integral << 8 | r.is_finite << 9 | r.is_a_number << 10;
}

static struct ssa_result_range
unpack_data(uint32_t v)
{
   return (struct ssa_result_range){
      .range = v & 0xff,
      .is_integral = (v & 0x00100) != 0,
      .is_finite = (v & 0x00200) != 0,
      .is_a_number = (v & 0x00400) != 0
   };
}

static nir_alu_type
nir_alu_src_type(const nir_alu_instr *instr, unsigned src)
{
   return nir_alu_type_get_base_type(nir_op_infos[instr->op].input_types[src]) |
          nir_src_bit_size(instr->src[src].src);
}

static struct ssa_result_range
analyze_constant(const struct nir_alu_instr *instr, unsigned src,
                 nir_alu_type use_type)
{
   uint8_t swizzle[NIR_MAX_VEC_COMPONENTS] = { 0, 1, 2, 3,
                                               4, 5, 6, 7,
                                               8, 9, 10, 11,
                                               12, 13, 14, 15 };

   /* If the source is an explicitly sized source, then we need to reset
    * both the number of components and the swizzle.
    */
   const unsigned num_components = nir_ssa_alu_instr_src_components(instr, src);

   for (unsigned i = 0; i < num_components; ++i)
      swizzle[i] = instr->src[src].swizzle[i];

   const nir_load_const_instr *const load =
      nir_instr_as_load_const(instr->src[src].src.ssa->parent_instr);

   struct ssa_result_range r = { unknown, false, false, false };

   switch (nir_alu_type_get_base_type(use_type)) {
   case nir_type_float: {
      double min_value = DBL_MAX;
      double max_value = -DBL_MAX;
      bool any_zero = false;
      bool all_zero = true;

      r.is_integral = true;
      r.is_a_number = true;
      r.is_finite = true;

      for (unsigned i = 0; i < num_components; ++i) {
         const double v = nir_const_value_as_float(load->value[swizzle[i]],
                                                   load->def.bit_size);

         if (floor(v) != v)
            r.is_integral = false;

         if (isnan(v))
            r.is_a_number = false;

         if (!isfinite(v))
            r.is_finite = false;

         any_zero = any_zero || (v == 0.0);
         all_zero = all_zero && (v == 0.0);
         min_value = MIN2(min_value, v);
         max_value = MAX2(max_value, v);
      }

      assert(any_zero >= all_zero);
      assert(isnan(max_value) || max_value >= min_value);

      if (all_zero)
         r.range = eq_zero;
      else if (min_value > 0.0)
         r.range = gt_zero;
      else if (min_value == 0.0)
         r.range = ge_zero;
      else if (max_value < 0.0)
         r.range = lt_zero;
      else if (max_value == 0.0)
         r.range = le_zero;
      else if (!any_zero)
         r.range = ne_zero;
      else
         r.range = unknown;

      return r;
   }

   case nir_type_int:
   case nir_type_bool: {
      int64_t min_value = INT_MAX;
      int64_t max_value = INT_MIN;
      bool any_zero = false;
      bool all_zero = true;

      for (unsigned i = 0; i < num_components; ++i) {
         const int64_t v = nir_const_value_as_int(load->value[swizzle[i]],
                                                  load->def.bit_size);

         any_zero = any_zero || (v == 0);
         all_zero = all_zero && (v == 0);
         min_value = MIN2(min_value, v);
         max_value = MAX2(max_value, v);
      }

      assert(any_zero >= all_zero);
      assert(max_value >= min_value);

      if (all_zero)
         r.range = eq_zero;
      else if (min_value > 0)
         r.range = gt_zero;
      else if (min_value == 0)
         r.range = ge_zero;
      else if (max_value < 0)
         r.range = lt_zero;
      else if (max_value == 0)
         r.range = le_zero;
      else if (!any_zero)
         r.range = ne_zero;
      else
         r.range = unknown;

      return r;
   }

   case nir_type_uint: {
      bool any_zero = false;
      bool all_zero = true;

      for (unsigned i = 0; i < num_components; ++i) {
         const uint64_t v = nir_const_value_as_uint(load->value[swizzle[i]],
                                                    load->def.bit_size);

         any_zero = any_zero || (v == 0);
         all_zero = all_zero && (v == 0);
      }

      assert(any_zero >= all_zero);

      if (all_zero)
         r.range = eq_zero;
      else if (any_zero)
         r.range = ge_zero;
      else
         r.range = gt_zero;

      return r;
   }

   default:
      unreachable("Invalid alu source type");
   }
}

/**
 * Short-hand name for use in the tables in process_fp_query.  If this name
 * becomes a problem on some compiler, we can change it to _.
 */
#define _______ unknown

#if defined(__clang__)
/* clang wants _Pragma("unroll X") */
#define pragma_unroll_5 _Pragma("unroll 5")
#define pragma_unroll_7 _Pragma("unroll 7")
/* gcc wants _Pragma("GCC unroll X") */
#elif defined(__GNUC__)
#if __GNUC__ >= 8
#define pragma_unroll_5 _Pragma("GCC unroll 5")
#define pragma_unroll_7 _Pragma("GCC unroll 7")
#else
#pragma GCC optimize("unroll-loops")
#define pragma_unroll_5
#define pragma_unroll_7
#endif
#else
/* MSVC doesn't have C99's _Pragma() */
#define pragma_unroll_5
#define pragma_unroll_7
#endif

#ifndef NDEBUG
#define ASSERT_TABLE_IS_COMMUTATIVE(t)                                      \
   do {                                                                     \
      static bool first = true;                                             \
      if (first) {                                                          \
         first = false;                                                     \
         pragma_unroll_7 for (unsigned r = 0; r < ARRAY_SIZE(t); r++)       \
         {                                                                  \
            pragma_unroll_7 for (unsigned c = 0; c < ARRAY_SIZE(t[0]); c++) \
               assert(t[r][c] == t[c][r]);                                  \
         }                                                                  \
      }                                                                     \
   } while (false)

#define ASSERT_TABLE_IS_DIAGONAL(t)                                   \
   do {                                                               \
      static bool first = true;                                       \
      if (first) {                                                    \
         first = false;                                               \
         pragma_unroll_7 for (unsigned r = 0; r < ARRAY_SIZE(t); r++) \
            assert(t[r][r] == r);                                     \
      }                                                               \
   } while (false)

#else
#define ASSERT_TABLE_IS_COMMUTATIVE(t)
#define ASSERT_TABLE_IS_DIAGONAL(t)
#endif /* !defined(NDEBUG) */

static enum ssa_ranges
union_ranges(enum ssa_ranges a, enum ssa_ranges b)
{
   static const enum ssa_ranges union_table[last_range + 1][last_range + 1] = {
      /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
      /* unknown */ { _______, _______, _______, _______, _______, _______, _______ },
      /* lt_zero */ { _______, lt_zero, le_zero, ne_zero, _______, ne_zero, le_zero },
      /* le_zero */ { _______, le_zero, le_zero, _______, _______, _______, le_zero },
      /* gt_zero */ { _______, ne_zero, _______, gt_zero, ge_zero, ne_zero, ge_zero },
      /* ge_zero */ { _______, _______, _______, ge_zero, ge_zero, _______, ge_zero },
      /* ne_zero */ { _______, ne_zero, _______, ne_zero, _______, ne_zero, _______ },
      /* eq_zero */ { _______, le_zero, le_zero, ge_zero, ge_zero, _______, eq_zero },
   };

   ASSERT_TABLE_IS_COMMUTATIVE(union_table);
   ASSERT_TABLE_IS_DIAGONAL(union_table);

   return union_table[a][b];
}

#ifndef NDEBUG
/* Verify that the 'unknown' entry in each row (or column) of the table is the
 * union of all the other values in the row (or column).
 */
#define ASSERT_UNION_OF_OTHERS_MATCHES_UNKNOWN_2_SOURCE(t)                      \
   do {                                                                         \
      static bool first = true;                                                 \
      if (first) {                                                              \
         first = false;                                                         \
         pragma_unroll_7 for (unsigned i = 0; i < last_range; i++)              \
         {                                                                      \
            enum ssa_ranges col_range = t[i][unknown + 1];                      \
            enum ssa_ranges row_range = t[unknown + 1][i];                      \
                                                                                \
            pragma_unroll_5 for (unsigned j = unknown + 2; j < last_range; j++) \
            {                                                                   \
               col_range = union_ranges(col_range, t[i][j]);                    \
               row_range = union_ranges(row_range, t[j][i]);                    \
            }                                                                   \
                                                                                \
            assert(col_range == t[i][unknown]);                                 \
            assert(row_range == t[unknown][i]);                                 \
         }                                                                      \
      }                                                                         \
   } while (false)

/* For most operations, the union of ranges for a strict inequality and
 * equality should be the range of the non-strict inequality (e.g.,
 * union_ranges(range(op(lt_zero), range(op(eq_zero))) == range(op(le_zero)).
 *
 * Does not apply to selection-like opcodes (bcsel, fmin, fmax, etc.).
 */
#define ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_1_SOURCE(t) \
   do {                                                                  \
      assert(union_ranges(t[lt_zero], t[eq_zero]) == t[le_zero]);        \
      assert(union_ranges(t[gt_zero], t[eq_zero]) == t[ge_zero]);        \
   } while (false)

#define ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_2_SOURCE(t)         \
   do {                                                                          \
      static bool first = true;                                                  \
      if (first) {                                                               \
         first = false;                                                          \
         pragma_unroll_7 for (unsigned i = 0; i < last_range; i++)               \
         {                                                                       \
            assert(union_ranges(t[i][lt_zero], t[i][eq_zero]) == t[i][le_zero]); \
            assert(union_ranges(t[i][gt_zero], t[i][eq_zero]) == t[i][ge_zero]); \
            assert(union_ranges(t[lt_zero][i], t[eq_zero][i]) == t[le_zero][i]); \
            assert(union_ranges(t[gt_zero][i], t[eq_zero][i]) == t[ge_zero][i]); \
         }                                                                       \
      }                                                                          \
   } while (false)

/* Several other unordered tuples span the range of "everything."  Each should
 * have the same value as unknown: (lt_zero, ge_zero), (le_zero, gt_zero), and
 * (eq_zero, ne_zero).  union_ranges is already commutative, so only one
 * ordering needs to be checked.
 *
 * Does not apply to selection-like opcodes (bcsel, fmin, fmax, etc.).
 *
 * In cases where this can be used, it is unnecessary to also use
 * ASSERT_UNION_OF_OTHERS_MATCHES_UNKNOWN_*_SOURCE.  For any range X,
 * union_ranges(X, X) == X.  The disjoint ranges cover all of the non-unknown
 * possibilities, so the union of all the unions of disjoint ranges is
 * equivalent to the union of "others."
 */
#define ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_1_SOURCE(t)      \
   do {                                                           \
      assert(union_ranges(t[lt_zero], t[ge_zero]) == t[unknown]); \
      assert(union_ranges(t[le_zero], t[gt_zero]) == t[unknown]); \
      assert(union_ranges(t[eq_zero], t[ne_zero]) == t[unknown]); \
   } while (false)

#define ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_2_SOURCE(t)       \
   do {                                                            \
      static bool first = true;                                    \
      if (first) {                                                 \
         first = false;                                            \
         pragma_unroll_7 for (unsigned i = 0; i < last_range; i++) \
         {                                                         \
            assert(union_ranges(t[i][lt_zero], t[i][ge_zero]) ==   \
                   t[i][unknown]);                                 \
            assert(union_ranges(t[i][le_zero], t[i][gt_zero]) ==   \
                   t[i][unknown]);                                 \
            assert(union_ranges(t[i][eq_zero], t[i][ne_zero]) ==   \
                   t[i][unknown]);                                 \
                                                                   \
            assert(union_ranges(t[lt_zero][i], t[ge_zero][i]) ==   \
                   t[unknown][i]);                                 \
            assert(union_ranges(t[le_zero][i], t[gt_zero][i]) ==   \
                   t[unknown][i]);                                 \
            assert(union_ranges(t[eq_zero][i], t[ne_zero][i]) ==   \
                   t[unknown][i]);                                 \
         }                                                         \
      }                                                            \
   } while (false)

#else
#define ASSERT_UNION_OF_OTHERS_MATCHES_UNKNOWN_2_SOURCE(t)
#define ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_1_SOURCE(t)
#define ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_2_SOURCE(t)
#define ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_1_SOURCE(t)
#define ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_2_SOURCE(t)
#endif /* !defined(NDEBUG) */

struct fp_query {
   struct analysis_query head;
   const nir_alu_instr *instr;
   unsigned src;
   nir_alu_type use_type;
};

static void
push_fp_query(struct analysis_state *state, const nir_alu_instr *alu, unsigned src, nir_alu_type type)
{
   struct fp_query *pushed_q = push_analysis_query(state, sizeof(struct fp_query));
   pushed_q->instr = alu;
   pushed_q->src = src;
   pushed_q->use_type = type == nir_type_invalid ? nir_alu_src_type(alu, src) : type;
}

static uintptr_t
get_fp_key(struct analysis_query *q)
{
   struct fp_query *fp_q = (struct fp_query *)q;
   const nir_src *src = &fp_q->instr->src[fp_q->src].src;

   if (src->ssa->parent_instr->type != nir_instr_type_alu)
      return 0;

   uintptr_t type_encoding;
   uintptr_t ptr = (uintptr_t)nir_instr_as_alu(src->ssa->parent_instr);

   /* The low 2 bits have to be zero or this whole scheme falls apart. */
   assert((ptr & 0x3) == 0);

   /* NIR is typeless in the sense that sequences of bits have whatever
    * meaning is attached to them by the instruction that consumes them.
    * However, the number of bits must match between producer and consumer.
    * As a result, the number of bits does not need to be encoded here.
    */
   switch (nir_alu_type_get_base_type(fp_q->use_type)) {
   case nir_type_int:
      type_encoding = 0;
      break;
   case nir_type_uint:
      type_encoding = 1;
      break;
   case nir_type_bool:
      type_encoding = 2;
      break;
   case nir_type_float:
      type_encoding = 3;
      break;
   default:
      unreachable("Invalid base type.");
   }

   return ptr | type_encoding;
}

/**
 * Analyze an expression to determine the range of its result
 *
 * The end result of this analysis is a token that communicates something
 * about the range of values.  There's an implicit grammar that produces
 * tokens from sequences of literal values, other tokens, and operations.
 * This function implements this grammar as a recursive-descent parser.  Some
 * (but not all) of the grammar is listed in-line in the function.
 */
static void
process_fp_query(struct analysis_state *state, struct analysis_query *aq, uint32_t *result,
                 const uint32_t *src_res)
{
   /* Ensure that the _Pragma("GCC unroll 7") above are correct. */
   STATIC_ASSERT(last_range + 1 == 7);

   struct fp_query q = *(struct fp_query *)aq;
   const nir_alu_instr *instr = q.instr;
   unsigned src = q.src;
   nir_alu_type use_type = q.use_type;

   if (nir_src_is_const(instr->src[src].src)) {
      *result = pack_data(analyze_constant(instr, src, use_type));
      return;
   }

   if (instr->src[src].src.ssa->parent_instr->type != nir_instr_type_alu) {
      *result = pack_data((struct ssa_result_range){ unknown, false, false, false });
      return;
   }

   const struct nir_alu_instr *const alu =
      nir_instr_as_alu(instr->src[src].src.ssa->parent_instr);

   /* Bail if the type of the instruction generating the value does not match
    * the type the value will be interpreted as.  int/uint/bool can be
    * reinterpreted trivially.  The most important cases are between float and
    * non-float.
    */
   if (alu->op != nir_op_mov && alu->op != nir_op_bcsel) {
      const nir_alu_type use_base_type =
         nir_alu_type_get_base_type(use_type);
      const nir_alu_type src_base_type =
         nir_alu_type_get_base_type(nir_op_infos[alu->op].output_type);

      if (use_base_type != src_base_type &&
          (use_base_type == nir_type_float ||
           src_base_type == nir_type_float)) {
         *result = pack_data((struct ssa_result_range){ unknown, false, false, false });
         return;
      }
   }

   if (!aq->pushed_queries) {
      switch (alu->op) {
      case nir_op_bcsel:
         push_fp_query(state, alu, 1, use_type);
         push_fp_query(state, alu, 2, use_type);
         return;
      case nir_op_mov:
         push_fp_query(state, alu, 0, use_type);
         return;
      case nir_op_i2f32:
      case nir_op_u2f32:
      case nir_op_fabs:
      case nir_op_fexp2:
      case nir_op_frcp:
      case nir_op_fneg:
      case nir_op_fsat:
      case nir_op_fsign:
      case nir_op_ffloor:
      case nir_op_fceil:
      case nir_op_ftrunc:
      case nir_op_fdot2:
      case nir_op_fdot3:
      case nir_op_fdot4:
      case nir_op_fdot8:
      case nir_op_fdot16:
      case nir_op_fdot2_replicated:
      case nir_op_fdot3_replicated:
      case nir_op_fdot4_replicated:
      case nir_op_fdot8_replicated:
      case nir_op_fdot16_replicated:
         push_fp_query(state, alu, 0, nir_type_invalid);
         return;
      case nir_op_fadd:
      case nir_op_fmax:
      case nir_op_fmin:
      case nir_op_fmul:
      case nir_op_fmulz:
      case nir_op_fpow:
         push_fp_query(state, alu, 0, nir_type_invalid);
         push_fp_query(state, alu, 1, nir_type_invalid);
         return;
      case nir_op_ffma:
      case nir_op_flrp:
         push_fp_query(state, alu, 0, nir_type_invalid);
         push_fp_query(state, alu, 1, nir_type_invalid);
         push_fp_query(state, alu, 2, nir_type_invalid);
         return;
      default:
         break;
      }
   }

   struct ssa_result_range r = { unknown, false, false, false };

   /* ge_zero: ge_zero + ge_zero
    *
    * gt_zero: gt_zero + eq_zero
    *        | gt_zero + ge_zero
    *        | eq_zero + gt_zero   # Addition is commutative
    *        | ge_zero + gt_zero   # Addition is commutative
    *        | gt_zero + gt_zero
    *        ;
    *
    * le_zero: le_zero + le_zero
    *
    * lt_zero: lt_zero + eq_zero
    *        | lt_zero + le_zero
    *        | eq_zero + lt_zero   # Addition is commutative
    *        | le_zero + lt_zero   # Addition is commutative
    *        | lt_zero + lt_zero
    *        ;
    *
    * ne_zero: eq_zero + ne_zero
    *        | ne_zero + eq_zero   # Addition is commutative
    *        ;
    *
    * eq_zero: eq_zero + eq_zero
    *        ;
    *
    * All other cases are 'unknown'.  The seeming odd entry is (ne_zero,
    * ne_zero), but that could be (-5, +5) which is not ne_zero.
    */
   static const enum ssa_ranges fadd_table[last_range + 1][last_range + 1] = {
      /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
      /* unknown */ { _______, _______, _______, _______, _______, _______, _______ },
      /* lt_zero */ { _______, lt_zero, lt_zero, _______, _______, _______, lt_zero },
      /* le_zero */ { _______, lt_zero, le_zero, _______, _______, _______, le_zero },
      /* gt_zero */ { _______, _______, _______, gt_zero, gt_zero, _______, gt_zero },
      /* ge_zero */ { _______, _______, _______, gt_zero, ge_zero, _______, ge_zero },
      /* ne_zero */ { _______, _______, _______, _______, _______, _______, ne_zero },
      /* eq_zero */ { _______, lt_zero, le_zero, gt_zero, ge_zero, ne_zero, eq_zero },
   };

   ASSERT_TABLE_IS_COMMUTATIVE(fadd_table);
   ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_2_SOURCE(fadd_table);
   ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_2_SOURCE(fadd_table);

   /* Due to flush-to-zero semanatics of floating-point numbers with very
    * small mangnitudes, we can never really be sure a result will be
    * non-zero.
    *
    * ge_zero: ge_zero * ge_zero
    *        | ge_zero * gt_zero
    *        | ge_zero * eq_zero
    *        | le_zero * lt_zero
    *        | lt_zero * le_zero  # Multiplication is commutative
    *        | le_zero * le_zero
    *        | gt_zero * ge_zero  # Multiplication is commutative
    *        | eq_zero * ge_zero  # Multiplication is commutative
    *        | a * a              # Left source == right source
    *        | gt_zero * gt_zero
    *        | lt_zero * lt_zero
    *        ;
    *
    * le_zero: ge_zero * le_zero
    *        | ge_zero * lt_zero
    *        | lt_zero * ge_zero  # Multiplication is commutative
    *        | le_zero * ge_zero  # Multiplication is commutative
    *        | le_zero * gt_zero
    *        | lt_zero * gt_zero
    *        | gt_zero * lt_zero  # Multiplication is commutative
    *        ;
    *
    * eq_zero: eq_zero * <any>
    *          <any> * eq_zero    # Multiplication is commutative
    *
    * All other cases are 'unknown'.
    */
   static const enum ssa_ranges fmul_table[last_range + 1][last_range + 1] = {
      /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
      /* unknown */ { _______, _______, _______, _______, _______, _______, eq_zero },
      /* lt_zero */ { _______, ge_zero, ge_zero, le_zero, le_zero, _______, eq_zero },
      /* le_zero */ { _______, ge_zero, ge_zero, le_zero, le_zero, _______, eq_zero },
      /* gt_zero */ { _______, le_zero, le_zero, ge_zero, ge_zero, _______, eq_zero },
      /* ge_zero */ { _______, le_zero, le_zero, ge_zero, ge_zero, _______, eq_zero },
      /* ne_zero */ { _______, _______, _______, _______, _______, _______, eq_zero },
      /* eq_zero */ { eq_zero, eq_zero, eq_zero, eq_zero, eq_zero, eq_zero, eq_zero }
   };

   ASSERT_TABLE_IS_COMMUTATIVE(fmul_table);
   ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_2_SOURCE(fmul_table);
   ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_2_SOURCE(fmul_table);

   static const enum ssa_ranges fneg_table[last_range + 1] = {
      /* unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
      _______, gt_zero, ge_zero, lt_zero, le_zero, ne_zero, eq_zero
   };

   ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_1_SOURCE(fneg_table);
   ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_1_SOURCE(fneg_table);

   switch (alu->op) {
   case nir_op_b2f32:
   case nir_op_b2i32:
      /* b2f32 will generate either 0.0 or 1.0.  This case is trivial.
       *
       * b2i32 will generate either 0x00000000 or 0x00000001.  When those bit
       * patterns are interpreted as floating point, they are 0.0 and
       * 1.401298464324817e-45.  The latter is subnormal, but it is finite and
       * a number.
       */
      r = (struct ssa_result_range){ ge_zero, alu->op == nir_op_b2f32, true, true };
      break;

   case nir_op_bcsel: {
      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      r.is_integral = left.is_integral && right.is_integral;

      /* This could be better, but it would require a lot of work.  For
       * example, the result of the following is a number:
       *
       *    bcsel(a > 0.0, a, 38.6)
       *
       * If the result of 'a > 0.0' is true, then the use of 'a' in the true
       * part of the bcsel must be a number.
       *
       * Other cases are even more challenging.
       *
       *    bcsel(a > 0.5, a - 0.5, 0.0)
       */
      r.is_a_number = left.is_a_number && right.is_a_number;
      r.is_finite = left.is_finite && right.is_finite;

      r.range = union_ranges(left.range, right.range);
      break;
   }

   case nir_op_i2f32:
   case nir_op_u2f32:
      r = unpack_data(src_res[0]);

      r.is_integral = true;
      r.is_a_number = true;
      r.is_finite = true;

      if (r.range == unknown && alu->op == nir_op_u2f32)
         r.range = ge_zero;

      break;

   case nir_op_fabs:
      r = unpack_data(src_res[0]);

      switch (r.range) {
      case unknown:
      case le_zero:
      case ge_zero:
         r.range = ge_zero;
         break;

      case lt_zero:
      case gt_zero:
      case ne_zero:
         r.range = gt_zero;
         break;

      case eq_zero:
         break;
      }

      break;

   case nir_op_fadd: {
      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      r.is_integral = left.is_integral && right.is_integral;
      r.range = fadd_table[left.range][right.range];

      /* X + Y is NaN if either operand is NaN or if one operand is +Inf and
       * the other is -Inf.  If neither operand is NaN and at least one of the
       * operands is finite, then the result cannot be NaN.
       */
      r.is_a_number = left.is_a_number && right.is_a_number &&
                      (left.is_finite || right.is_finite);
      break;
   }

   case nir_op_fexp2: {
      /* If the parameter might be less than zero, the mathematically result
       * will be on (0, 1).  For sufficiently large magnitude negative
       * parameters, the result will flush to zero.
       */
      static const enum ssa_ranges table[last_range + 1] = {
         /* unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
         ge_zero, ge_zero, ge_zero, gt_zero, gt_zero, ge_zero, gt_zero
      };

      r = unpack_data(src_res[0]);

      ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_1_SOURCE(table);
      ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_1_SOURCE(table);

      r.is_integral = r.is_integral && is_not_negative(r.range);
      r.range = table[r.range];

      /* Various cases can result in NaN, so assume the worst. */
      r.is_finite = false;
      r.is_a_number = false;
      break;
   }

   case nir_op_fmax: {
      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      r.is_integral = left.is_integral && right.is_integral;

      /* This is conservative.  It may be possible to determine that the
       * result must be finite in more cases, but it would take some effort to
       * work out all the corners.  For example, fmax({lt_zero, finite},
       * {lt_zero}) should result in {lt_zero, finite}.
       */
      r.is_finite = left.is_finite && right.is_finite;

      /* If one source is NaN, fmax always picks the other source. */
      r.is_a_number = left.is_a_number || right.is_a_number;

      /* gt_zero: fmax(gt_zero, *)
       *        | fmax(*, gt_zero)        # Treat fmax as commutative
       *        ;
       *
       * ge_zero: fmax(ge_zero, ne_zero)
       *        | fmax(ge_zero, lt_zero)
       *        | fmax(ge_zero, le_zero)
       *        | fmax(ge_zero, eq_zero)
       *        | fmax(ne_zero, ge_zero)  # Treat fmax as commutative
       *        | fmax(lt_zero, ge_zero)  # Treat fmax as commutative
       *        | fmax(le_zero, ge_zero)  # Treat fmax as commutative
       *        | fmax(eq_zero, ge_zero)  # Treat fmax as commutative
       *        | fmax(ge_zero, ge_zero)
       *        ;
       *
       * le_zero: fmax(le_zero, lt_zero)
       *        | fmax(lt_zero, le_zero)  # Treat fmax as commutative
       *        | fmax(le_zero, le_zero)
       *        ;
       *
       * lt_zero: fmax(lt_zero, lt_zero)
       *        ;
       *
       * ne_zero: fmax(ne_zero, lt_zero)
       *        | fmax(lt_zero, ne_zero)  # Treat fmax as commutative
       *        | fmax(ne_zero, ne_zero)
       *        ;
       *
       * eq_zero: fmax(eq_zero, le_zero)
       *        | fmax(eq_zero, lt_zero)
       *        | fmax(le_zero, eq_zero)  # Treat fmax as commutative
       *        | fmax(lt_zero, eq_zero)  # Treat fmax as commutative
       *        | fmax(eq_zero, eq_zero)
       *        ;
       *
       * All other cases are 'unknown'.
       */
      static const enum ssa_ranges table[last_range + 1][last_range + 1] = {
         /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
         /* unknown */ { _______, _______, _______, gt_zero, ge_zero, _______, _______ },
         /* lt_zero */ { _______, lt_zero, le_zero, gt_zero, ge_zero, ne_zero, eq_zero },
         /* le_zero */ { _______, le_zero, le_zero, gt_zero, ge_zero, _______, eq_zero },
         /* gt_zero */ { gt_zero, gt_zero, gt_zero, gt_zero, gt_zero, gt_zero, gt_zero },
         /* ge_zero */ { ge_zero, ge_zero, ge_zero, gt_zero, ge_zero, ge_zero, ge_zero },
         /* ne_zero */ { _______, ne_zero, _______, gt_zero, ge_zero, ne_zero, _______ },
         /* eq_zero */ { _______, eq_zero, eq_zero, gt_zero, ge_zero, _______, eq_zero }
      };

      /* Treat fmax as commutative. */
      ASSERT_TABLE_IS_COMMUTATIVE(table);
      ASSERT_TABLE_IS_DIAGONAL(table);
      ASSERT_UNION_OF_OTHERS_MATCHES_UNKNOWN_2_SOURCE(table);

      r.range = table[left.range][right.range];

      /* Recall that when either value is NaN, fmax will pick the other value.
       * This means the result range of the fmax will either be the "ideal"
       * result range (calculated above) or the range of the non-NaN value.
       */
      if (!left.is_a_number)
         r.range = union_ranges(r.range, right.range);

      if (!right.is_a_number)
         r.range = union_ranges(r.range, left.range);

      break;
   }

   case nir_op_fmin: {
      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      r.is_integral = left.is_integral && right.is_integral;

      /* This is conservative.  It may be possible to determine that the
       * result must be finite in more cases, but it would take some effort to
       * work out all the corners.  For example, fmin({gt_zero, finite},
       * {gt_zero}) should result in {gt_zero, finite}.
       */
      r.is_finite = left.is_finite && right.is_finite;

      /* If one source is NaN, fmin always picks the other source. */
      r.is_a_number = left.is_a_number || right.is_a_number;

      /* lt_zero: fmin(lt_zero, *)
       *        | fmin(*, lt_zero)        # Treat fmin as commutative
       *        ;
       *
       * le_zero: fmin(le_zero, ne_zero)
       *        | fmin(le_zero, gt_zero)
       *        | fmin(le_zero, ge_zero)
       *        | fmin(le_zero, eq_zero)
       *        | fmin(ne_zero, le_zero)  # Treat fmin as commutative
       *        | fmin(gt_zero, le_zero)  # Treat fmin as commutative
       *        | fmin(ge_zero, le_zero)  # Treat fmin as commutative
       *        | fmin(eq_zero, le_zero)  # Treat fmin as commutative
       *        | fmin(le_zero, le_zero)
       *        ;
       *
       * ge_zero: fmin(ge_zero, gt_zero)
       *        | fmin(gt_zero, ge_zero)  # Treat fmin as commutative
       *        | fmin(ge_zero, ge_zero)
       *        ;
       *
       * gt_zero: fmin(gt_zero, gt_zero)
       *        ;
       *
       * ne_zero: fmin(ne_zero, gt_zero)
       *        | fmin(gt_zero, ne_zero)  # Treat fmin as commutative
       *        | fmin(ne_zero, ne_zero)
       *        ;
       *
       * eq_zero: fmin(eq_zero, ge_zero)
       *        | fmin(eq_zero, gt_zero)
       *        | fmin(ge_zero, eq_zero)  # Treat fmin as commutative
       *        | fmin(gt_zero, eq_zero)  # Treat fmin as commutative
       *        | fmin(eq_zero, eq_zero)
       *        ;
       *
       * All other cases are 'unknown'.
       */
      static const enum ssa_ranges table[last_range + 1][last_range + 1] = {
         /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
         /* unknown */ { _______, lt_zero, le_zero, _______, _______, _______, _______ },
         /* lt_zero */ { lt_zero, lt_zero, lt_zero, lt_zero, lt_zero, lt_zero, lt_zero },
         /* le_zero */ { le_zero, lt_zero, le_zero, le_zero, le_zero, le_zero, le_zero },
         /* gt_zero */ { _______, lt_zero, le_zero, gt_zero, ge_zero, ne_zero, eq_zero },
         /* ge_zero */ { _______, lt_zero, le_zero, ge_zero, ge_zero, _______, eq_zero },
         /* ne_zero */ { _______, lt_zero, le_zero, ne_zero, _______, ne_zero, _______ },
         /* eq_zero */ { _______, lt_zero, le_zero, eq_zero, eq_zero, _______, eq_zero }
      };

      /* Treat fmin as commutative. */
      ASSERT_TABLE_IS_COMMUTATIVE(table);
      ASSERT_TABLE_IS_DIAGONAL(table);
      ASSERT_UNION_OF_OTHERS_MATCHES_UNKNOWN_2_SOURCE(table);

      r.range = table[left.range][right.range];

      /* Recall that when either value is NaN, fmin will pick the other value.
       * This means the result range of the fmin will either be the "ideal"
       * result range (calculated above) or the range of the non-NaN value.
       */
      if (!left.is_a_number)
         r.range = union_ranges(r.range, right.range);

      if (!right.is_a_number)
         r.range = union_ranges(r.range, left.range);

      break;
   }

   case nir_op_fmul:
   case nir_op_fmulz: {
      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      r.is_integral = left.is_integral && right.is_integral;

      /* x * x => ge_zero */
      if (left.range != eq_zero && nir_alu_srcs_equal(alu, alu, 0, 1)) {
         /* Even if x > 0, the result of x*x can be zero when x is, for
          * example, a subnormal number.
          */
         r.range = ge_zero;
      } else if (left.range != eq_zero && nir_alu_srcs_negative_equal(alu, alu, 0, 1)) {
         /* -x * x => le_zero. */
         r.range = le_zero;
      } else
         r.range = fmul_table[left.range][right.range];

      if (alu->op == nir_op_fmul) {
         /* Mulitpliation produces NaN for X * NaN and for 0 * ±Inf.  If both
          * operands are numbers and either both are finite or one is finite and
          * the other cannot be zero, then the result must be a number.
          */
         r.is_a_number = (left.is_a_number && right.is_a_number) &&
                         ((left.is_finite && right.is_finite) ||
                          (!is_not_zero(left.range) && right.is_finite) ||
                          (left.is_finite && !is_not_zero(right.range)));
      } else {
         /* nir_op_fmulz: unlike nir_op_fmul, 0 * ±Inf is a number. */
         r.is_a_number = left.is_a_number && right.is_a_number;
      }

      break;
   }

   case nir_op_frcp:
      r = (struct ssa_result_range){
         unpack_data(src_res[0]).range,
         false,
         false, /* Various cases can result in NaN, so assume the worst. */
         false  /*    "      "    "     "    "  "    "    "    "    "    */
      };
      break;

   case nir_op_mov:
      r = unpack_data(src_res[0]);
      break;

   case nir_op_fneg:
      r = unpack_data(src_res[0]);
      r.range = fneg_table[r.range];
      break;

   case nir_op_fsat: {
      const struct ssa_result_range left = unpack_data(src_res[0]);

      /* fsat(NaN) = 0. */
      r.is_a_number = true;
      r.is_finite = true;

      switch (left.range) {
      case le_zero:
      case lt_zero:
      case eq_zero:
         r.range = eq_zero;
         r.is_integral = true;
         break;

      case gt_zero:
         /* fsat is equivalent to fmin(fmax(X, 0.0), 1.0), so if X is not a
          * number, the result will be 0.
          */
         r.range = left.is_a_number ? gt_zero : ge_zero;
         r.is_integral = left.is_integral;
         break;

      case ge_zero:
      case ne_zero:
      case unknown:
         /* Since the result must be in [0, 1], the value must be >= 0. */
         r.range = ge_zero;
         r.is_integral = left.is_integral;
         break;
      }
      break;
   }

   case nir_op_fsign:
      r = (struct ssa_result_range){
         unpack_data(src_res[0]).range,
         true,
         true, /* fsign is -1, 0, or 1, even for NaN, so it must be a number. */
         true  /* fsign is -1, 0, or 1, even for NaN, so it must be finite. */
      };
      break;

   case nir_op_fsqrt:
   case nir_op_frsq:
      r = (struct ssa_result_range){ ge_zero, false, false, false };
      break;

   case nir_op_ffloor: {
      const struct ssa_result_range left = unpack_data(src_res[0]);

      r.is_integral = true;

      /* In IEEE 754, floor(NaN) is NaN, and floor(±Inf) is ±Inf. See
       * https://pubs.opengroup.org/onlinepubs/9699919799.2016edition/functions/floor.html
       */
      r.is_a_number = left.is_a_number;
      r.is_finite = left.is_finite;

      if (left.is_integral || left.range == le_zero || left.range == lt_zero)
         r.range = left.range;
      else if (left.range == ge_zero || left.range == gt_zero)
         r.range = ge_zero;
      else if (left.range == ne_zero)
         r.range = unknown;

      break;
   }

   case nir_op_fceil: {
      const struct ssa_result_range left = unpack_data(src_res[0]);

      r.is_integral = true;

      /* In IEEE 754, ceil(NaN) is NaN, and ceil(±Inf) is ±Inf. See
       * https://pubs.opengroup.org/onlinepubs/9699919799.2016edition/functions/ceil.html
       */
      r.is_a_number = left.is_a_number;
      r.is_finite = left.is_finite;

      if (left.is_integral || left.range == ge_zero || left.range == gt_zero)
         r.range = left.range;
      else if (left.range == le_zero || left.range == lt_zero)
         r.range = le_zero;
      else if (left.range == ne_zero)
         r.range = unknown;

      break;
   }

   case nir_op_ftrunc: {
      const struct ssa_result_range left = unpack_data(src_res[0]);

      r.is_integral = true;

      /* In IEEE 754, trunc(NaN) is NaN, and trunc(±Inf) is ±Inf.  See
       * https://pubs.opengroup.org/onlinepubs/9699919799.2016edition/functions/trunc.html
       */
      r.is_a_number = left.is_a_number;
      r.is_finite = left.is_finite;

      if (left.is_integral)
         r.range = left.range;
      else if (left.range == ge_zero || left.range == gt_zero)
         r.range = ge_zero;
      else if (left.range == le_zero || left.range == lt_zero)
         r.range = le_zero;
      else if (left.range == ne_zero)
         r.range = unknown;

      break;
   }

   case nir_op_flt:
   case nir_op_fge:
   case nir_op_feq:
   case nir_op_fneu:
   case nir_op_ilt:
   case nir_op_ige:
   case nir_op_ieq:
   case nir_op_ine:
   case nir_op_ult:
   case nir_op_uge:
      /* Boolean results are 0 or -1. */
      r = (struct ssa_result_range){ le_zero, false, true, false };
      break;

   case nir_op_fdot2:
   case nir_op_fdot3:
   case nir_op_fdot4:
   case nir_op_fdot8:
   case nir_op_fdot16:
   case nir_op_fdot2_replicated:
   case nir_op_fdot3_replicated:
   case nir_op_fdot4_replicated:
   case nir_op_fdot8_replicated:
   case nir_op_fdot16_replicated: {
      const struct ssa_result_range left = unpack_data(src_res[0]);

      /* If the two sources are the same SSA value, then the result is either
       * NaN or some number >= 0.  If one source is the negation of the other,
       * the result is either NaN or some number <= 0.
       *
       * In either of these two cases, if one source is a number, then the
       * other must also be a number.  Since it should not be possible to get
       * Inf-Inf in the dot-product, the result must also be a number.
       */
      if (nir_alu_srcs_equal(alu, alu, 0, 1)) {
         r = (struct ssa_result_range){ ge_zero, false, left.is_a_number, false };
      } else if (nir_alu_srcs_negative_equal(alu, alu, 0, 1)) {
         r = (struct ssa_result_range){ le_zero, false, left.is_a_number, false };
      } else {
         r = (struct ssa_result_range){ unknown, false, false, false };
      }
      break;
   }

   case nir_op_fpow: {
      /* Due to flush-to-zero semanatics of floating-point numbers with very
       * small mangnitudes, we can never really be sure a result will be
       * non-zero.
       *
       * NIR uses pow() and powf() to constant evaluate nir_op_fpow.  The man
       * page for that function says:
       *
       *    If y is 0, the result is 1.0 (even if x is a NaN).
       *
       * gt_zero: pow(*, eq_zero)
       *        | pow(eq_zero, lt_zero)   # 0^-y = +inf
       *        | pow(eq_zero, le_zero)   # 0^-y = +inf or 0^0 = 1.0
       *        ;
       *
       * eq_zero: pow(eq_zero, gt_zero)
       *        ;
       *
       * ge_zero: pow(gt_zero, gt_zero)
       *        | pow(gt_zero, ge_zero)
       *        | pow(gt_zero, lt_zero)
       *        | pow(gt_zero, le_zero)
       *        | pow(gt_zero, ne_zero)
       *        | pow(gt_zero, unknown)
       *        | pow(ge_zero, gt_zero)
       *        | pow(ge_zero, ge_zero)
       *        | pow(ge_zero, lt_zero)
       *        | pow(ge_zero, le_zero)
       *        | pow(ge_zero, ne_zero)
       *        | pow(ge_zero, unknown)
       *        | pow(eq_zero, ge_zero)  # 0^0 = 1.0 or 0^+y = 0.0
       *        | pow(eq_zero, ne_zero)  # 0^-y = +inf or 0^+y = 0.0
       *        | pow(eq_zero, unknown)  # union of all other y cases
       *        ;
       *
       * All other cases are unknown.
       *
       * We could do better if the right operand is a constant, integral
       * value.
       */
      static const enum ssa_ranges table[last_range + 1][last_range + 1] = {
         /* left\right   unknown  lt_zero  le_zero  gt_zero  ge_zero  ne_zero  eq_zero */
         /* unknown */ { _______, _______, _______, _______, _______, _______, gt_zero },
         /* lt_zero */ { _______, _______, _______, _______, _______, _______, gt_zero },
         /* le_zero */ { _______, _______, _______, _______, _______, _______, gt_zero },
         /* gt_zero */ { ge_zero, ge_zero, ge_zero, ge_zero, ge_zero, ge_zero, gt_zero },
         /* ge_zero */ { ge_zero, ge_zero, ge_zero, ge_zero, ge_zero, ge_zero, gt_zero },
         /* ne_zero */ { _______, _______, _______, _______, _______, _______, gt_zero },
         /* eq_zero */ { ge_zero, gt_zero, gt_zero, eq_zero, ge_zero, ge_zero, gt_zero },
      };

      const struct ssa_result_range left = unpack_data(src_res[0]);
      const struct ssa_result_range right = unpack_data(src_res[1]);

      ASSERT_UNION_OF_DISJOINT_MATCHES_UNKNOWN_2_SOURCE(table);
      ASSERT_UNION_OF_EQ_AND_STRICT_INEQ_MATCHES_NONSTRICT_2_SOURCE(table);

      r.is_integral = left.is_integral && right.is_integral &&
                      is_not_negative(right.range);
      r.range = table[left.range][right.range];

      /* Various cases can result in NaN, so assume the worst. */
      r.is_a_number = false;

      break;
   }

   case nir_op_ffma: {
      const struct ssa_result_range first = unpack_data(src_res[0]);
      const struct ssa_result_range second = unpack_data(src_res[1]);
      const struct ssa_result_range third = unpack_data(src_res[2]);

      r.is_integral = first.is_integral && second.is_integral &&
                      third.is_integral;

      /* Various cases can result in NaN, so assume the worst. */
      r.is_a_number = false;

      enum ssa_ranges fmul_range;

      if (first.range != eq_zero && nir_alu_srcs_equal(alu, alu, 0, 1)) {
         /* See handling of nir_op_fmul for explanation of why ge_zero is the
          * range.
          */
         fmul_range = ge_zero;
      } else if (first.range != eq_zero && nir_alu_srcs_negative_equal(alu, alu, 0, 1)) {
         /* -x * x => le_zero */
         fmul_range = le_zero;
      } else
         fmul_range = fmul_table[first.range][second.range];

      r.range = fadd_table[fmul_range][third.range];
      break;
   }

   case nir_op_flrp: {
      const struct ssa_result_range first = unpack_data(src_res[0]);
      const struct ssa_result_range second = unpack_data(src_res[1]);
      const struct ssa_result_range third = unpack_data(src_res[2]);

      r.is_integral = first.is_integral && second.is_integral &&
                      third.is_integral;

      /* Various cases can result in NaN, so assume the worst. */
      r.is_a_number = false;

      /* Decompose the flrp to first + third * (second + -first) */
      const enum ssa_ranges inner_fadd_range =
         fadd_table[second.range][fneg_table[first.range]];

      const enum ssa_ranges fmul_range =
         fmul_table[third.range][inner_fadd_range];

      r.range = fadd_table[first.range][fmul_range];
      break;
   }

   default:
      r = (struct ssa_result_range){ unknown, false, false, false };
      break;
   }

   if (r.range == eq_zero)
      r.is_integral = true;

   /* Just like isfinite(), the is_finite flag implies the value is a number. */
   assert((int)r.is_finite <= (int)r.is_a_number);

   *result = pack_data(r);
}

#undef _______

struct ssa_result_range
nir_analyze_range(struct hash_table *range_ht,
                  const nir_alu_instr *alu, unsigned src)
{
   struct fp_query query_alloc[64];
   uint32_t result_alloc[64];

   struct analysis_state state;
   state.range_ht = range_ht;
   util_dynarray_init_from_stack(&state.query_stack, query_alloc, sizeof(query_alloc));
   util_dynarray_init_from_stack(&state.result_stack, result_alloc, sizeof(result_alloc));
   state.query_size = sizeof(struct fp_query);
   state.get_key = &get_fp_key;
   state.process_query = &process_fp_query;

   push_fp_query(&state, alu, src, nir_type_invalid);

   return unpack_data(perform_analysis(&state));
}

static uint32_t
bitmask(uint32_t size)
{
   return size >= 32 ? 0xffffffffu : ((uint32_t)1 << size) - 1u;
}

static uint64_t
mul_clamp(uint32_t a, uint32_t b)
{
   if (a != 0 && (a * b) / a != b)
      return (uint64_t)UINT32_MAX + 1;
   else
      return a * b;
}

/* recursively gather at most "buf_size" phi/bcsel sources */
static unsigned
search_phi_bcsel(nir_scalar scalar, nir_scalar *buf, unsigned buf_size, struct set *visited)
{
   if (_mesa_set_search(visited, scalar.def))
      return 0;
   _mesa_set_add(visited, scalar.def);

   if (scalar.def->parent_instr->type == nir_instr_type_phi) {
      nir_phi_instr *phi = nir_instr_as_phi(scalar.def->parent_instr);
      unsigned num_sources_left = exec_list_length(&phi->srcs);
      if (buf_size >= num_sources_left) {
         unsigned total_added = 0;
         nir_foreach_phi_src(src, phi) {
            num_sources_left--;
            unsigned added = search_phi_bcsel(nir_get_scalar(src->src.ssa, scalar.comp),
                                              buf + total_added, buf_size - num_sources_left, visited);
            assert(added <= buf_size);
            buf_size -= added;
            total_added += added;
         }
         return total_added;
      }
   }

   if (nir_scalar_is_alu(scalar)) {
      nir_op op = nir_scalar_alu_op(scalar);

      if ((op == nir_op_bcsel || op == nir_op_b32csel) && buf_size >= 2) {
         nir_scalar src1 = nir_scalar_chase_alu_src(scalar, 1);
         nir_scalar src2 = nir_scalar_chase_alu_src(scalar, 2);

         unsigned added = search_phi_bcsel(src1, buf, buf_size - 1, visited);
         buf_size -= added;
         added += search_phi_bcsel(src2, buf + added, buf_size, visited);
         return added;
      }
   }

   buf[0] = scalar;
   return 1;
}

static nir_variable *
lookup_input(nir_shader *shader, unsigned driver_location)
{
   return nir_find_variable_with_driver_location(shader, nir_var_shader_in,
                                                 driver_location);
}

/* The config here should be generic enough to be correct on any HW. */
static const nir_unsigned_upper_bound_config default_ub_config = {
   .min_subgroup_size = 1u,
   .max_subgroup_size = UINT16_MAX,
   .max_workgroup_invocations = UINT16_MAX,

   /* max_workgroup_count represents the maximum compute shader / kernel
    * dispatchable work size. On most hardware, this is essentially
    * unbounded. On some hardware max_workgroup_count[1] and
    * max_workgroup_count[2] may be smaller.
    */
   .max_workgroup_count = { UINT32_MAX, UINT32_MAX, UINT32_MAX },

   /* max_workgroup_size is the local invocation maximum. This is generally
    * small the OpenGL 4.2 minimum maximum is 1024.
    */
   .max_workgroup_size = { UINT16_MAX, UINT16_MAX, UINT16_MAX },

   .vertex_attrib_max = {
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
      UINT32_MAX,
   },
};

struct uub_query {
   struct analysis_query head;
   nir_scalar scalar;
};

static void
push_uub_query(struct analysis_state *state, nir_scalar scalar)
{
   struct uub_query *pushed_q = push_analysis_query(state, sizeof(struct uub_query));
   pushed_q->scalar = scalar;
}

static uintptr_t
get_uub_key(struct analysis_query *q)
{
   nir_scalar scalar = ((struct uub_query *)q)->scalar;
   /* keys can't be 0, so we have to add 1 to the index */
   unsigned shift_amount = ffs(NIR_MAX_VEC_COMPONENTS) - 1;
   return nir_scalar_is_const(scalar)
             ? 0
             : ((uintptr_t)(scalar.def->index + 1) << shift_amount) | scalar.comp;
}

static void
get_intrinsic_uub(struct analysis_state *state, struct uub_query q, uint32_t *result,
                  const uint32_t *src)
{
   nir_shader *shader = state->shader;
   const nir_unsigned_upper_bound_config *config = state->config;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(q.scalar.def->parent_instr);
   switch (intrin->intrinsic) {
   case nir_intrinsic_load_local_invocation_index:
      /* The local invocation index is used under the hood by RADV for
       * some non-compute-like shaders (eg. LS and NGG). These technically
       * run in workgroups on the HW, even though this fact is not exposed
       * by the API.
       * They can safely use the same code path here as variable sized
       * compute-like shader stages.
       */
      if (!gl_shader_stage_uses_workgroup(shader->info.stage) ||
          shader->info.workgroup_size_variable) {
         *result = config->max_workgroup_invocations - 1;
      } else {
         *result = (shader->info.workgroup_size[0] *
                    shader->info.workgroup_size[1] *
                    shader->info.workgroup_size[2]) -
                   1u;
      }
      break;
   case nir_intrinsic_load_local_invocation_id:
      if (shader->info.workgroup_size_variable)
         *result = config->max_workgroup_size[q.scalar.comp] - 1u;
      else
         *result = shader->info.workgroup_size[q.scalar.comp] - 1u;
      break;
   case nir_intrinsic_load_workgroup_id:
      *result = config->max_workgroup_count[q.scalar.comp] - 1u;
      break;
   case nir_intrinsic_load_num_workgroups:
      *result = config->max_workgroup_count[q.scalar.comp];
      break;
   case nir_intrinsic_load_global_invocation_id:
      if (shader->info.workgroup_size_variable) {
         *result = mul_clamp(config->max_workgroup_size[q.scalar.comp],
                             config->max_workgroup_count[q.scalar.comp]) -
                   1u;
      } else {
         *result = (shader->info.workgroup_size[q.scalar.comp] *
                    config->max_workgroup_count[q.scalar.comp]) -
                   1u;
      }
      break;
   case nir_intrinsic_load_invocation_id:
      if (shader->info.stage == MESA_SHADER_TESS_CTRL)
         *result = shader->info.tess.tcs_vertices_out
                      ? (shader->info.tess.tcs_vertices_out - 1)
                      : 511; /* Generous maximum output patch size of 512 */
      break;
   case nir_intrinsic_load_subgroup_invocation:
   case nir_intrinsic_first_invocation:
      *result = config->max_subgroup_size - 1;
      break;
   case nir_intrinsic_mbcnt_amd: {
      if (!q.head.pushed_queries) {
         push_uub_query(state, nir_get_scalar(intrin->src[1].ssa, 0));
         return;
      } else {
         uint32_t src0 = config->max_subgroup_size - 1;
         uint32_t src1 = src[0];
         if (src0 + src1 >= src0) /* check overflow */
            *result = src0 + src1;
      }
      break;
   }
   case nir_intrinsic_load_subgroup_size:
      *result = config->max_subgroup_size;
      break;
   case nir_intrinsic_load_subgroup_id:
   case nir_intrinsic_load_num_subgroups: {
      uint32_t workgroup_size = config->max_workgroup_invocations;
      if (gl_shader_stage_uses_workgroup(shader->info.stage) &&
          !shader->info.workgroup_size_variable) {
         workgroup_size = shader->info.workgroup_size[0] *
                          shader->info.workgroup_size[1] *
                          shader->info.workgroup_size[2];
      }
      *result = DIV_ROUND_UP(workgroup_size, config->min_subgroup_size);
      if (intrin->intrinsic == nir_intrinsic_load_subgroup_id)
         (*result)--;
      break;
   }
   case nir_intrinsic_load_input: {
      if (shader->info.stage == MESA_SHADER_VERTEX && nir_src_is_const(intrin->src[0])) {
         nir_variable *var = lookup_input(shader, nir_intrinsic_base(intrin));
         if (var) {
            int loc = var->data.location - VERT_ATTRIB_GENERIC0;
            if (loc >= 0)
               *result = config->vertex_attrib_max[loc];
         }
      }
      break;
   }
   case nir_intrinsic_reduce:
   case nir_intrinsic_inclusive_scan:
   case nir_intrinsic_exclusive_scan: {
      nir_op op = nir_intrinsic_reduction_op(intrin);
      if (op == nir_op_umin || op == nir_op_umax || op == nir_op_imin || op == nir_op_imax) {
         if (!q.head.pushed_queries) {
            push_uub_query(state, nir_get_scalar(intrin->src[0].ssa, q.scalar.comp));
            return;
         } else {
            *result = src[0];
         }
      }
      break;
   }
   case nir_intrinsic_read_first_invocation:
   case nir_intrinsic_read_invocation:
   case nir_intrinsic_shuffle:
   case nir_intrinsic_shuffle_xor:
   case nir_intrinsic_shuffle_up:
   case nir_intrinsic_shuffle_down:
   case nir_intrinsic_quad_broadcast:
   case nir_intrinsic_quad_swap_horizontal:
   case nir_intrinsic_quad_swap_vertical:
   case nir_intrinsic_quad_swap_diagonal:
   case nir_intrinsic_quad_swizzle_amd:
   case nir_intrinsic_masked_swizzle_amd:
      if (!q.head.pushed_queries) {
         push_uub_query(state, nir_get_scalar(intrin->src[0].ssa, q.scalar.comp));
         return;
      } else {
         *result = src[0];
      }
      break;
   case nir_intrinsic_write_invocation_amd:
      if (!q.head.pushed_queries) {
         push_uub_query(state, nir_get_scalar(intrin->src[0].ssa, q.scalar.comp));
         push_uub_query(state, nir_get_scalar(intrin->src[1].ssa, q.scalar.comp));
         return;
      } else {
         *result = MAX2(src[0], src[1]);
      }
      break;
   case nir_intrinsic_load_tess_rel_patch_id_amd:
   case nir_intrinsic_load_tcs_num_patches_amd:
      /* Very generous maximum: TCS/TES executed by largest possible workgroup */
      *result = config->max_workgroup_invocations / MAX2(shader->info.tess.tcs_vertices_out, 1u);
      break;
   case nir_intrinsic_load_typed_buffer_amd: {
      const enum pipe_format format = nir_intrinsic_format(intrin);
      if (format == PIPE_FORMAT_NONE)
         break;

      const struct util_format_description *desc = util_format_description(format);
      if (desc->channel[q.scalar.comp].type != UTIL_FORMAT_TYPE_UNSIGNED)
         break;

      if (desc->channel[q.scalar.comp].normalized) {
         *result = fui(1.0);
         break;
      }

      const uint32_t chan_max = u_uintN_max(desc->channel[q.scalar.comp].size);
      *result = desc->channel[q.scalar.comp].pure_integer ? chan_max : fui(chan_max);
      break;
   }
   case nir_intrinsic_load_scalar_arg_amd:
   case nir_intrinsic_load_vector_arg_amd: {
      uint32_t upper_bound = nir_intrinsic_arg_upper_bound_u32_amd(intrin);
      if (upper_bound)
         *result = upper_bound;
      break;
   }
   default:
      break;
   }
}

static void
get_alu_uub(struct analysis_state *state, struct uub_query q, uint32_t *result, const uint32_t *src)
{
   nir_op op = nir_scalar_alu_op(q.scalar);

   /* Early exit for unsupported ALU opcodes. */
   switch (op) {
   case nir_op_umin:
   case nir_op_imin:
   case nir_op_imax:
   case nir_op_umax:
   case nir_op_iand:
   case nir_op_ior:
   case nir_op_ixor:
   case nir_op_ishl:
   case nir_op_imul:
   case nir_op_ushr:
   case nir_op_ishr:
   case nir_op_iadd:
   case nir_op_umod:
   case nir_op_udiv:
   case nir_op_bcsel:
   case nir_op_b32csel:
   case nir_op_ubfe:
   case nir_op_bfm:
   case nir_op_fmul:
   case nir_op_fmulz:
   case nir_op_extract_u8:
   case nir_op_extract_i8:
   case nir_op_extract_u16:
   case nir_op_extract_i16:
   case nir_op_b2i8:
   case nir_op_b2i16:
   case nir_op_b2i32:
      break;
   case nir_op_u2u1:
   case nir_op_u2u8:
   case nir_op_u2u16:
   case nir_op_u2u32:
   case nir_op_f2u32:
      if (nir_scalar_chase_alu_src(q.scalar, 0).def->bit_size > 32) {
         /* If src is >32 bits, return max */
         return;
      }
      break;
   default:
      return;
   }

   if (!q.head.pushed_queries) {
      for (unsigned i = 0; i < nir_op_infos[op].num_inputs; i++)
         push_uub_query(state, nir_scalar_chase_alu_src(q.scalar, i));
      return;
   }

   uint32_t max = bitmask(q.scalar.def->bit_size);
   switch (op) {
   case nir_op_umin:
      *result = src[0] < src[1] ? src[0] : src[1];
      break;
   case nir_op_imin:
   case nir_op_imax:
   case nir_op_umax:
      *result = src[0] > src[1] ? src[0] : src[1];
      break;
   case nir_op_iand:
      *result = bitmask(util_last_bit64(src[0])) & bitmask(util_last_bit64(src[1]));
      break;
   case nir_op_ior:
   case nir_op_ixor:
      *result = bitmask(util_last_bit64(src[0])) | bitmask(util_last_bit64(src[1]));
      break;
   case nir_op_ishl: {
      uint32_t src1 = MIN2(src[1], q.scalar.def->bit_size - 1u);
      if (util_last_bit64(src[0]) + src1 <= q.scalar.def->bit_size) /* check overflow */
         *result = src[0] << src1;
      break;
   }
   case nir_op_imul:
      if (src[0] == 0 || (src[0] * src[1]) / src[0] == src[1]) /* check overflow */
         *result = src[0] * src[1];
      break;
   case nir_op_ushr: {
      nir_scalar src1_scalar = nir_scalar_chase_alu_src(q.scalar, 1);
      uint32_t mask = q.scalar.def->bit_size - 1u;
      if (nir_scalar_is_const(src1_scalar))
         *result = src[0] >> (nir_scalar_as_uint(src1_scalar) & mask);
      else
         *result = src[0];
      break;
   }
   case nir_op_ishr: {
      nir_scalar src1_scalar = nir_scalar_chase_alu_src(q.scalar, 1);
      uint32_t mask = q.scalar.def->bit_size - 1u;
      if (src[0] <= 2147483647 && nir_scalar_is_const(src1_scalar))
         *result = src[0] >> (nir_scalar_as_uint(src1_scalar) & mask);
      else
         *result = src[0];
      break;
   }
   case nir_op_iadd:
      if (src[0] + src[1] >= src[0]) /* check overflow */
         *result = src[0] + src[1];
      break;
   case nir_op_umod:
      *result = src[1] ? src[1] - 1 : 0;
      break;
   case nir_op_udiv: {
      nir_scalar src1_scalar = nir_scalar_chase_alu_src(q.scalar, 1);
      if (nir_scalar_is_const(src1_scalar))
         *result = nir_scalar_as_uint(src1_scalar)
                      ? src[0] / nir_scalar_as_uint(src1_scalar)
                      : 0;
      else
         *result = src[0];
      break;
   }
   case nir_op_bcsel:
   case nir_op_b32csel:
      *result = src[1] > src[2] ? src[1] : src[2];
      break;
   case nir_op_ubfe:
      *result = bitmask(MIN2(src[2], q.scalar.def->bit_size));
      break;
   case nir_op_bfm: {
      nir_scalar src1_scalar = nir_scalar_chase_alu_src(q.scalar, 1);
      if (nir_scalar_is_const(src1_scalar)) {
         uint32_t src0 = MIN2(src[0], 31);
         uint32_t src1 = nir_scalar_as_uint(src1_scalar) & 0x1fu;
         *result = bitmask(src0) << src1;
      } else {
         uint32_t src0 = MIN2(src[0], 31);
         uint32_t src1 = MIN2(src[1], 31);
         *result = bitmask(MIN2(src0 + src1, 32));
      }
      break;
   }
   /* limited floating-point support for f2u32(fmul(load_input(), <constant>)) */
   case nir_op_f2u32:
      /* infinity/NaN starts at 0x7f800000u, negative numbers at 0x80000000 */
      if (src[0] < 0x7f800000u) {
         float val;
         memcpy(&val, &src[0], 4);
         *result = (uint32_t)val;
      }
      break;
   case nir_op_fmul:
   case nir_op_fmulz:
      /* infinity/NaN starts at 0x7f800000u, negative numbers at 0x80000000 */
      if (src[0] < 0x7f800000u && src[1] < 0x7f800000u) {
         float src0_f, src1_f;
         memcpy(&src0_f, &src[0], 4);
         memcpy(&src1_f, &src[1], 4);
         /* not a proper rounding-up multiplication, but should be good enough */
         float max_f = ceilf(src0_f) * ceilf(src1_f);
         memcpy(result, &max_f, 4);
      }
      break;
   case nir_op_u2u1:
   case nir_op_u2u8:
   case nir_op_u2u16:
   case nir_op_u2u32:
      *result = MIN2(src[0], max);
      break;
   case nir_op_b2i8:
   case nir_op_b2i16:
   case nir_op_b2i32:
      *result = 1;
      break;
   case nir_op_msad_4x8:
      *result = MIN2((uint64_t)src[2] + 4 * 255, UINT32_MAX);
      break;
   case nir_op_extract_u8:
      *result = MIN2(src[0], UINT8_MAX);
      break;
   case nir_op_extract_i8:
      *result = (src[0] >= 0x80) ? max : MIN2(src[0], INT8_MAX);
      break;
   case nir_op_extract_u16:
      *result = MIN2(src[0], UINT16_MAX);
      break;
   case nir_op_extract_i16:
      *result = (src[0] >= 0x8000) ? max : MIN2(src[0], INT16_MAX);
      break;
   default:
      break;
   }
}

static void
get_phi_uub(struct analysis_state *state, struct uub_query q, uint32_t *result, const uint32_t *src)
{
   nir_phi_instr *phi = nir_instr_as_phi(q.scalar.def->parent_instr);

   if (exec_list_is_empty(&phi->srcs))
      return;

   if (q.head.pushed_queries) {
      *result = src[0];
      for (unsigned i = 1; i < q.head.pushed_queries; i++)
         *result = MAX2(*result, src[i]);
      return;
   }

   nir_cf_node *prev = nir_cf_node_prev(&phi->instr.block->cf_node);
   if (!prev || prev->type == nir_cf_node_block) {
      /* Resolve cycles by inserting max into range_ht. */
      uint32_t max = bitmask(q.scalar.def->bit_size);
      _mesa_hash_table_insert(state->range_ht, (void *)get_uub_key(&q.head), (void *)(uintptr_t)max);

      struct set *visited = _mesa_pointer_set_create(NULL);
      nir_scalar *defs = alloca(sizeof(nir_scalar) * 64);
      unsigned def_count = search_phi_bcsel(q.scalar, defs, 64, visited);
      _mesa_set_destroy(visited, NULL);

      for (unsigned i = 0; i < def_count; i++)
         push_uub_query(state, defs[i]);
   } else {
      nir_foreach_phi_src(src, phi)
         push_uub_query(state, nir_get_scalar(src->src.ssa, q.scalar.comp));
   }
}

static void
process_uub_query(struct analysis_state *state, struct analysis_query *aq, uint32_t *result,
                  const uint32_t *src)
{
   struct uub_query q = *(struct uub_query *)aq;

   *result = bitmask(q.scalar.def->bit_size);
   if (nir_scalar_is_const(q.scalar))
      *result = nir_scalar_as_uint(q.scalar);
   else if (nir_scalar_is_intrinsic(q.scalar))
      get_intrinsic_uub(state, q, result, src);
   else if (nir_scalar_is_alu(q.scalar))
      get_alu_uub(state, q, result, src);
   else if (q.scalar.def->parent_instr->type == nir_instr_type_phi)
      get_phi_uub(state, q, result, src);
}

uint32_t
nir_unsigned_upper_bound(nir_shader *shader, struct hash_table *range_ht,
                         nir_scalar scalar,
                         const nir_unsigned_upper_bound_config *config)
{
   if (!config)
      config = &default_ub_config;

   struct uub_query query_alloc[16];
   uint32_t result_alloc[16];

   struct analysis_state state;
   state.shader = shader;
   state.config = config;
   state.range_ht = range_ht;
   util_dynarray_init_from_stack(&state.query_stack, query_alloc, sizeof(query_alloc));
   util_dynarray_init_from_stack(&state.result_stack, result_alloc, sizeof(result_alloc));
   state.query_size = sizeof(struct uub_query);
   state.get_key = &get_uub_key;
   state.process_query = &process_uub_query;

   push_uub_query(&state, scalar);

   return perform_analysis(&state);
}

bool
nir_addition_might_overflow(nir_shader *shader, struct hash_table *range_ht,
                            nir_scalar ssa, unsigned const_val,
                            const nir_unsigned_upper_bound_config *config)
{
   if (nir_scalar_is_alu(ssa)) {
      nir_op alu_op = nir_scalar_alu_op(ssa);

      /* iadd(imul(a, #b), #c) */
      if (alu_op == nir_op_imul || alu_op == nir_op_ishl) {
         nir_scalar mul_src0 = nir_scalar_chase_alu_src(ssa, 0);
         nir_scalar mul_src1 = nir_scalar_chase_alu_src(ssa, 1);
         uint32_t stride = 1;
         if (nir_scalar_is_const(mul_src0))
            stride = nir_scalar_as_uint(mul_src0);
         else if (nir_scalar_is_const(mul_src1))
            stride = nir_scalar_as_uint(mul_src1);

         if (alu_op == nir_op_ishl)
            stride = 1u << (stride % 32u);

         if (!stride || const_val <= UINT32_MAX - (UINT32_MAX / stride * stride))
            return false;
      }

      /* iadd(iand(a, #b), #c) */
      if (alu_op == nir_op_iand) {
         nir_scalar and_src0 = nir_scalar_chase_alu_src(ssa, 0);
         nir_scalar and_src1 = nir_scalar_chase_alu_src(ssa, 1);
         uint32_t mask = 0xffffffff;
         if (nir_scalar_is_const(and_src0))
            mask = nir_scalar_as_uint(and_src0);
         else if (nir_scalar_is_const(and_src1))
            mask = nir_scalar_as_uint(and_src1);
         if (mask == 0 || const_val < (1u << (ffs(mask) - 1)))
            return false;
      }
   }

   uint32_t ub = nir_unsigned_upper_bound(shader, range_ht, ssa, config);
   return const_val + ub < const_val;
}

static uint64_t
ssa_def_bits_used(const nir_def *def, int recur)
{
   uint64_t bits_used = 0;
   uint64_t all_bits = BITFIELD64_MASK(def->bit_size);

   /* Querying the bits used from a vector is too hard of a question to
    * answer.  Return the conservative answer that all bits are used.  To
    * handle this, the function would need to be extended to be a query of a
    * single component of the vector.  That would also necessary to fully
    * handle the 'num_components > 1' inside the loop below.
    *
    * FINISHME: This restriction will eventually need to be restricted to be
    * useful for hardware that uses u16vec2 as the native 16-bit integer type.
    */
   if (def->num_components > 1)
      return all_bits;

   /* Limit recursion */
   if (recur-- <= 0)
      return all_bits;

   nir_foreach_use(src, def) {
      switch (nir_src_parent_instr(src)->type) {
      case nir_instr_type_alu: {
         nir_alu_instr *use_alu = nir_instr_as_alu(nir_src_parent_instr(src));
         unsigned src_idx = container_of(src, nir_alu_src, src) - use_alu->src;

         /* If a user of the value produces a vector result, return the
          * conservative answer that all bits are used.  It is possible to
          * answer this query by looping over the components used.  For example,
          *
          * vec4 32 ssa_5 = load_const(0x0000f000, 0x00000f00, 0x000000f0, 0x0000000f)
          * ...
          * vec4 32 ssa_8 = iand ssa_7.xxxx, ssa_5
          *
          * could conceivably return 0x0000ffff when queyring the bits used of
          * ssa_7.  This is unlikely to be worth the effort because the
          * question can eventually answered after the shader has been
          * scalarized.
          */
         if (use_alu->def.num_components > 1)
            return all_bits;

         switch (use_alu->op) {
         case nir_op_u2u8:
         case nir_op_i2i8:
            bits_used |= 0xff;
            break;

         case nir_op_u2u16:
         case nir_op_i2i16:
            bits_used |= all_bits & 0xffff;
            break;

         case nir_op_u2u32:
         case nir_op_i2i32:
            bits_used |= all_bits & 0xffffffff;
            break;

         case nir_op_extract_u8:
         case nir_op_extract_i8:
            if (src_idx == 0 && nir_src_is_const(use_alu->src[1].src)) {
               unsigned chunk = nir_src_comp_as_uint(use_alu->src[1].src,
                                                     use_alu->src[1].swizzle[0]);
               bits_used |= 0xffull << (chunk * 8);
               break;
            } else {
               return all_bits;
            }

         case nir_op_extract_u16:
         case nir_op_extract_i16:
            if (src_idx == 0 && nir_src_is_const(use_alu->src[1].src)) {
               unsigned chunk = nir_src_comp_as_uint(use_alu->src[1].src,
                                                     use_alu->src[1].swizzle[0]);
               bits_used |= 0xffffull << (chunk * 16);
               break;
            } else {
               return all_bits;
            }

         case nir_op_ishl:
         case nir_op_ishr:
         case nir_op_ushr:
            if (src_idx == 1) {
               bits_used |= (nir_src_bit_size(use_alu->src[0].src) - 1);
               break;
            } else {
               return all_bits;
            }

         case nir_op_iand:
            assert(src_idx < 2);
            if (nir_src_is_const(use_alu->src[1 - src_idx].src)) {
               uint64_t u64 = nir_src_comp_as_uint(use_alu->src[1 - src_idx].src,
                                                   use_alu->src[1 - src_idx].swizzle[0]);
               bits_used |= u64;
               break;
            } else {
               return all_bits;
            }

         case nir_op_ior:
            assert(src_idx < 2);
            if (nir_src_is_const(use_alu->src[1 - src_idx].src)) {
               uint64_t u64 = nir_src_comp_as_uint(use_alu->src[1 - src_idx].src,
                                                   use_alu->src[1 - src_idx].swizzle[0]);
               bits_used |= all_bits & ~u64;
               break;
            } else {
               return all_bits;
            }

         default:
            /* We don't know what this op does */
            return all_bits;
         }
         break;
      }

      case nir_instr_type_intrinsic: {
         nir_intrinsic_instr *use_intrin =
            nir_instr_as_intrinsic(nir_src_parent_instr(src));
         unsigned src_idx = src - use_intrin->src;

         switch (use_intrin->intrinsic) {
         case nir_intrinsic_read_invocation:
         case nir_intrinsic_shuffle:
         case nir_intrinsic_shuffle_up:
         case nir_intrinsic_shuffle_down:
         case nir_intrinsic_shuffle_xor:
         case nir_intrinsic_quad_broadcast:
         case nir_intrinsic_quad_swap_horizontal:
         case nir_intrinsic_quad_swap_vertical:
         case nir_intrinsic_quad_swap_diagonal:
            if (src_idx == 0) {
               bits_used |= ssa_def_bits_used(&use_intrin->def, recur);
            } else {
               if (use_intrin->intrinsic == nir_intrinsic_quad_broadcast) {
                  bits_used |= 3;
               } else {
                  /* Subgroups larger than 128 are not a thing */
                  bits_used |= 127;
               }
            }
            break;

         case nir_intrinsic_reduce:
         case nir_intrinsic_inclusive_scan:
         case nir_intrinsic_exclusive_scan:
            assert(src_idx == 0);
            switch (nir_intrinsic_reduction_op(use_intrin)) {
            case nir_op_iadd:
            case nir_op_imul:
            case nir_op_ior:
            case nir_op_iand:
            case nir_op_ixor:
               bits_used |= ssa_def_bits_used(&use_intrin->def, recur);
               break;

            default:
               return all_bits;
            }
            break;

         default:
            /* We don't know what this op does */
            return all_bits;
         }
         break;
      }

      case nir_instr_type_phi: {
         nir_phi_instr *use_phi = nir_instr_as_phi(nir_src_parent_instr(src));
         bits_used |= ssa_def_bits_used(&use_phi->def, recur);
         break;
      }

      default:
         return all_bits;
      }

      /* If we've somehow shown that all our bits are used, we're done */
      assert((bits_used & ~all_bits) == 0);
      if (bits_used == all_bits)
         return all_bits;
   }

   return bits_used;
}

uint64_t
nir_def_bits_used(const nir_def *def)
{
   return ssa_def_bits_used(def, 2);
}
