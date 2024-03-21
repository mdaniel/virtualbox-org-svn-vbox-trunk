/*
 * Copyright © 2015 Intel Corporation
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

#ifndef _VTN_PRIVATE_H_
#define _VTN_PRIVATE_H_

#include <setjmp.h>

#include "nir/nir.h"
#include "nir/nir_builder.h"
#include "util/u_dynarray.h"
#include "nir_spirv.h"
#include "spirv.h"
#include "vtn_generator_ids.h"

extern uint32_t mesa_spirv_debug;

#ifndef NDEBUG
#define MESA_SPIRV_DEBUG(flag) unlikely(mesa_spirv_debug & (MESA_SPIRV_DEBUG_ ## flag))
#else
#define MESA_SPIRV_DEBUG(flag) false
#endif

#define MESA_SPIRV_DEBUG_STRUCTURED     (1u << 0)

struct vtn_builder;
struct vtn_decoration;

/* setjmp/longjmp is broken on MinGW: https://sourceforge.net/p/mingw-w64/bugs/406/ */
#if defined(__MINGW32__) && !defined(_UCRT)
  #define vtn_setjmp __builtin_setjmp
  #define vtn_longjmp __builtin_longjmp
#else
  #define vtn_setjmp setjmp
  #define vtn_longjmp longjmp
#endif

void vtn_log(struct vtn_builder *b, enum nir_spirv_debug_level level,
             size_t spirv_offset, const char *message);

void vtn_logf(struct vtn_builder *b, enum nir_spirv_debug_level level,
              size_t spirv_offset, const char *fmt, ...) PRINTFLIKE(4, 5);

#define vtn_info(...) vtn_logf(b, NIR_SPIRV_DEBUG_LEVEL_INFO, 0, __VA_ARGS__)

void _vtn_warn(struct vtn_builder *b, const char *file, unsigned line,
               const char *fmt, ...) PRINTFLIKE(4, 5);
#define vtn_warn(...) _vtn_warn(b, __FILE__, __LINE__, __VA_ARGS__)

void _vtn_err(struct vtn_builder *b, const char *file, unsigned line,
               const char *fmt, ...) PRINTFLIKE(4, 5);
#define vtn_err(...) _vtn_err(b, __FILE__, __LINE__, __VA_ARGS__)

/** Fail SPIR-V parsing
 *
 * This function logs an error and then bails out of the shader compile using
 * longjmp.  This being safe relies on two things:
 *
 *  1) We must guarantee that setjmp is called after allocating the builder
 *     and setting up b->debug (so that logging works) but before before any
 *     errors have a chance to occur.
 *
 *  2) While doing the SPIR-V -> NIR conversion, we need to be careful to
 *     ensure that all heap allocations happen through ralloc and are parented
 *     to the builder.  This way they will get properly cleaned up on error.
 *
 *  3) We must ensure that _vtn_fail is never called while a mutex lock or a
 *     reference to any other resource is held with the exception of ralloc
 *     objects which are parented to the builder.
 *
 * So long as these two things continue to hold, we can easily longjmp back to
 * spirv_to_nir(), clean up the builder, and return NULL.
 */
NORETURN void
_vtn_fail(struct vtn_builder *b, const char *file, unsigned line,
             const char *fmt, ...) PRINTFLIKE(4, 5);

#define vtn_fail(...) _vtn_fail(b, __FILE__, __LINE__, __VA_ARGS__)

/** Fail if the given expression evaluates to true */
#define vtn_fail_if(expr, ...) \
   do { \
      if (unlikely(expr)) \
         vtn_fail(__VA_ARGS__); \
   } while (0)

#define _vtn_fail_with(t, msg, v) \
   vtn_fail("%s: %s (%u)\n", msg, spirv_ ## t ## _to_string(v), v)

#define vtn_fail_with_decoration(msg, v) _vtn_fail_with(decoration, msg, v)
#define vtn_fail_with_opcode(msg, v)     _vtn_fail_with(op, msg, v)

/** Assert that a condition is true and, if it isn't, vtn_fail
 *
 * This macro is transitional only and should not be used in new code.  Use
 * vtn_fail_if and provide a real message instead.
 */
#define vtn_assert(expr) \
   do { \
      if (!likely(expr)) \
         vtn_fail("%s", #expr); \
   } while (0)

/* These are used to allocate data that can be dropped at the end of
 * the parsing.  Any NIR data structure should keep using the ralloc,
 * since they will outlive the parsing.
 */
#define vtn_alloc(B, TYPE)               linear_alloc(B->lin_ctx, TYPE)
#define vtn_zalloc(B, TYPE)              linear_zalloc(B->lin_ctx, TYPE)
#define vtn_alloc_array(B, TYPE, ELEMS)  linear_alloc_array(B->lin_ctx, TYPE, ELEMS)
#define vtn_zalloc_array(B, TYPE, ELEMS) linear_zalloc_array(B->lin_ctx, TYPE, ELEMS)
#define vtn_alloc_size(B, SIZE)          linear_alloc_child(B->lin_ctx, SIZE)
#define vtn_zalloc_size(B, SIZE)         linear_zalloc_child(B->lin_ctx, SIZE)

enum vtn_value_type {
   vtn_value_type_invalid = 0,
   vtn_value_type_undef,
   vtn_value_type_string,
   vtn_value_type_decoration_group,
   vtn_value_type_type,
   vtn_value_type_constant,
   vtn_value_type_pointer,
   vtn_value_type_function,
   vtn_value_type_block,
   vtn_value_type_ssa,
   vtn_value_type_extension,
   vtn_value_type_image_pointer,
};

const char *vtn_value_type_to_string(enum vtn_value_type t);

struct vtn_case {
   struct list_head link;

   struct vtn_block *block;

   /* The uint32_t values that map to this case */
   struct util_dynarray values;

   /* True if this is the default case */
   bool is_default;

   /* Initialized to false; used when sorting the list of cases */
   bool visited;
};

struct vtn_block {
   struct list_head link;

   /** A pointer to the label instruction */
   const uint32_t *label;

   /** A pointer to the merge instruction (or NULL if non exists) */
   const uint32_t *merge;

   /** A pointer to the branch instruction that ends this block */
   const uint32_t *branch;

   /** Points to the switch case started by this block (if any) */
   struct vtn_case *switch_case;

   /** Every block ends in a nop intrinsic so that we can find it again */
   nir_intrinsic_instr *end_nop;

   /** attached nir_block */
   struct nir_block *block;

   /* Inner-most construct that this block is part of. */
   struct vtn_construct *parent;

   /* Blocks that succeed this block.  Used by structured control flow. */
   struct vtn_successor *successors;
   unsigned successors_count;

   /* Position of this block in the structured post-order traversal. */
   unsigned pos;

   bool visited;
};

struct vtn_function {
   struct list_head link;

   struct vtn_type *type;

   bool referenced;
   bool emitted;

   nir_function *nir_func;
   struct vtn_block *start_block;

   struct list_head body;

   const uint32_t *end;

   SpvLinkageType linkage;
   SpvFunctionControlMask control;

   unsigned block_count;

   /* Ordering of blocks to be processed by structured control flow.  See
    * vtn_structured_cfg.c for details.
    */
   unsigned ordered_blocks_count;
   struct vtn_block **ordered_blocks;

   /* Structured control flow constructs.  See struct vtn_construct. */
   struct list_head constructs;
};

#define vtn_foreach_function(func, func_list) \
   list_for_each_entry(struct vtn_function, func, func_list, link)

#define vtn_foreach_case(cse, case_list) \
   list_for_each_entry(struct vtn_case, cse, case_list, link)

#define vtn_foreach_case_safe(cse, case_list) \
   list_for_each_entry_safe(struct vtn_case, cse, case_list, link)

typedef bool (*vtn_instruction_handler)(struct vtn_builder *, SpvOp,
                                        const uint32_t *, unsigned);

void vtn_build_cfg(struct vtn_builder *b, const uint32_t *words,
                   const uint32_t *end);
void vtn_function_emit(struct vtn_builder *b, struct vtn_function *func,
                       vtn_instruction_handler instruction_handler);
void vtn_handle_function_call(struct vtn_builder *b, SpvOp opcode,
                              const uint32_t *w, unsigned count);

bool vtn_cfg_handle_prepass_instruction(struct vtn_builder *b, SpvOp opcode,
                                        const uint32_t *w, unsigned count);
void vtn_emit_cf_func_structured(struct vtn_builder *b, struct vtn_function *func,
                                 vtn_instruction_handler handler);
bool vtn_handle_phis_first_pass(struct vtn_builder *b, SpvOp opcode,
                                const uint32_t *w, unsigned count);
void vtn_emit_ret_store(struct vtn_builder *b, const struct vtn_block *block);
void vtn_build_structured_cfg(struct vtn_builder *b, const uint32_t *words,
                              const uint32_t *end);

const uint32_t *
vtn_foreach_instruction(struct vtn_builder *b, const uint32_t *start,
                        const uint32_t *end, vtn_instruction_handler handler);

struct vtn_ssa_value {
   bool is_variable;

   union {
      nir_def *def;
      nir_variable *var;
      struct vtn_ssa_value **elems;
   };

   /* For matrices, if this is non-NULL, then this value is actually the
    * transpose of some other value.  The value that `transposed` points to
    * always dominates this value.
    */
   struct vtn_ssa_value *transposed;

   const struct glsl_type *type;
};

enum vtn_base_type {
   vtn_base_type_void,
   vtn_base_type_scalar,
   vtn_base_type_vector,
   vtn_base_type_matrix,
   vtn_base_type_array,
   vtn_base_type_struct,
   vtn_base_type_pointer,
   vtn_base_type_image,
   vtn_base_type_sampler,
   vtn_base_type_sampled_image,
   vtn_base_type_accel_struct,
   vtn_base_type_ray_query,
   vtn_base_type_function,
   vtn_base_type_event,
   vtn_base_type_cooperative_matrix,
};

struct vtn_type {
   enum vtn_base_type base_type;

   const struct glsl_type *type;

   /* The SPIR-V id of the given type. */
   uint32_t id;

   /* Specifies the length of complex types.
    *
    * For Workgroup pointers, this is the size of the referenced type.
    */
   unsigned length;

   /* for arrays, matrices and pointers, the array stride */
   unsigned stride;

   /* Access qualifiers */
   enum gl_access_qualifier access;

   union {
      /* Members for scalar, vector, and array-like types */
      struct {
         /* for arrays, the vtn_type for the elements of the array */
         struct vtn_type *array_element;

         /* for matrices, whether the matrix is stored row-major */
         bool row_major:1;

         /* Whether this type, or a parent type, has been decorated as a
          * builtin
          */
         bool is_builtin:1;

         /* Which built-in to use */
         SpvBuiltIn builtin;
      };

      /* Members for struct types */
      struct {
         /* for structures, the vtn_type for each member */
         struct vtn_type **members;

         /* for structs, the offset of each member */
         unsigned *offsets;

         /* for structs, whether it was decorated as a "non-SSBO-like" block */
         bool block:1;

         /* for structs, whether it was decorated as an "SSBO-like" block */
         bool buffer_block:1;

         /* for structs with block == true, whether this is a builtin block
          * (i.e. a block that contains only builtins).
          */
         bool builtin_block:1;

         /* for structs and unions it specifies the minimum alignment of the
          * members. 0 means packed.
          *
          * Set by CPacked and Alignment Decorations in kernels.
          */
         bool packed:1;
      };

      /* Members for pointer types */
      struct {
         /* For pointers, the vtn_type for dereferenced type */
         struct vtn_type *deref;

         /* Storage class for pointers */
         SpvStorageClass storage_class;

         /* Required alignment for pointers */
         uint32_t align;
      };

      /* Members for image types */
      struct {
         /* GLSL image type for this type.  This is not to be confused with
          * vtn_type::type which is actually going to be the GLSL type for a
          * pointer to an image, likely a uint32_t.
          */
         const struct glsl_type *glsl_image;

         /* Image format for image_load_store type images */
         unsigned image_format;

         /* Access qualifier for storage images */
         SpvAccessQualifier access_qualifier;
      };

      /* Members for sampled image types */
      struct {
         /* For sampled images, the image type */
         struct vtn_type *image;
      };

      /* Members for function types */
      struct {
         /* For functions, the vtn_type for each parameter */
         struct vtn_type **params;

         /* Return type for functions */
         struct vtn_type *return_type;
      };

      /* Members for cooperative matrix types. */
      struct {
         struct glsl_cmat_description desc;
         struct vtn_type *component_type;
      };
   };
};

bool vtn_type_contains_block(struct vtn_builder *b, struct vtn_type *type);

bool vtn_types_compatible(struct vtn_builder *b,
                          struct vtn_type *t1, struct vtn_type *t2);

struct vtn_type *vtn_type_without_array(struct vtn_type *type);

struct vtn_variable;

enum vtn_access_mode {
   vtn_access_mode_id,
   vtn_access_mode_literal,
};

struct vtn_access_link {
   enum vtn_access_mode mode;
   int64_t id;
};

struct vtn_access_chain {
   uint32_t length;

   /** Whether or not to treat the base pointer as an array.  This is only
    * true if this access chain came from an OpPtrAccessChain.
    */
   bool ptr_as_array;

   /* Access qualifiers */
   enum gl_access_qualifier access;

   bool in_bounds;

   /** Struct elements and array offsets.
    *
    * This is an array of 1 so that it can conveniently be created on the
    * stack but the real length is given by the length field.
    */
   struct vtn_access_link link[1];
};

enum vtn_variable_mode {
   vtn_variable_mode_function,
   vtn_variable_mode_private,
   vtn_variable_mode_uniform,
   vtn_variable_mode_atomic_counter,
   vtn_variable_mode_ubo,
   vtn_variable_mode_ssbo,
   vtn_variable_mode_phys_ssbo,
   vtn_variable_mode_push_constant,
   vtn_variable_mode_workgroup,
   vtn_variable_mode_cross_workgroup,
   vtn_variable_mode_task_payload,
   vtn_variable_mode_generic,
   vtn_variable_mode_constant,
   vtn_variable_mode_input,
   vtn_variable_mode_output,
   vtn_variable_mode_image,
   vtn_variable_mode_accel_struct,
   vtn_variable_mode_call_data,
   vtn_variable_mode_call_data_in,
   vtn_variable_mode_ray_payload,
   vtn_variable_mode_ray_payload_in,
   vtn_variable_mode_hit_attrib,
   vtn_variable_mode_shader_record,
   vtn_variable_mode_node_payload,
};

struct vtn_pointer {
   /** The variable mode for the referenced data */
   enum vtn_variable_mode mode;

   /** The dereferenced type of this pointer */
   struct vtn_type *type;

   /** The pointer type of this pointer
    *
    * This may be NULL for some temporary pointers constructed as part of a
    * large load, store, or copy.  It MUST be valid for all pointers which are
    * stored as SPIR-V SSA values.
    */
   struct vtn_type *ptr_type;

   /** The referenced variable, if known
    *
    * This field may be NULL if the pointer uses a (block_index, offset) pair
    * instead of an access chain or if the access chain starts at a deref.
    */
   struct vtn_variable *var;

   /** The NIR deref corresponding to this pointer */
   nir_deref_instr *deref;

   /** A (block_index, offset) pair representing a UBO or SSBO position. */
   struct nir_def *block_index;
   struct nir_def *offset;

   /* Access qualifiers */
   enum gl_access_qualifier access;
};

struct vtn_variable {
   enum vtn_variable_mode mode;

   struct vtn_type *type;

   unsigned descriptor_set;
   unsigned binding;
   bool explicit_binding;
   unsigned offset;
   unsigned input_attachment_index;

   nir_variable *var;

   /* If the variable is a struct with a location set on it then this will be
    * stored here. This will be used to calculate locations for members that
    * don’t have their own explicit location.
    */
   int base_location;

   /**
    * In some early released versions of GLSLang, it implemented all function
    * calls by making copies of all parameters into temporary variables and
    * passing those variables into the function.  It even did so for samplers
    * and images which violates the SPIR-V spec.  Unfortunately, two games
    * (Talos Principle and Doom) shipped with this old version of GLSLang and
    * also happen to pass samplers into functions.  Talos Principle received
    * an update fairly shortly after release with an updated GLSLang.  Doom,
    * on the other hand, has never received an update so we need to work
    * around this GLSLang issue in SPIR-V -> NIR.  Hopefully, we can drop this
    * hack at some point in the future.
    */
   struct vtn_pointer *copy_prop_sampler;

   /* Access qualifiers. */
   enum gl_access_qualifier access;
};

const struct glsl_type *
vtn_type_get_nir_type(struct vtn_builder *b, struct vtn_type *type,
                      enum vtn_variable_mode mode);

mesa_scope
vtn_translate_scope(struct vtn_builder *b, SpvScope scope);

struct vtn_image_pointer {
   nir_deref_instr *image;
   nir_def *coord;
   nir_def *sample;
   nir_def *lod;
};

struct vtn_value {
   enum vtn_value_type value_type;

   /* Workaround for https://gitlab.freedesktop.org/mesa/mesa/-/issues/3406
    * Only set for OpImage / OpSampledImage. Note that this is in addition
    * the existence of a NonUniform decoration on this value.*/
   uint32_t propagated_non_uniform : 1;

   /* Valid for vtn_value_type_constant to indicate the value is OpConstantNull. */
   bool is_null_constant:1;

   /* Valid when all the members of the value are undef. */
   bool is_undef_constant:1;

   const char *name;
   struct vtn_decoration *decoration;
   struct vtn_type *type;
   union {
      const char *str;
      nir_constant *constant;
      struct vtn_pointer *pointer;
      struct vtn_image_pointer *image;
      struct vtn_function *func;
      struct vtn_block *block;
      struct vtn_ssa_value *ssa;
      vtn_instruction_handler ext_handler;
   };
};

#define VTN_DEC_DECORATION -1
#define VTN_DEC_EXECUTION_MODE -2
#define VTN_DEC_STRUCT_MEMBER_NAME0 -3
#define VTN_DEC_STRUCT_MEMBER0 0

struct vtn_decoration {
   struct vtn_decoration *next;

   /* Different kinds of decorations are stored in a value,
      the scope defines what decoration it refers to:

      - VTN_DEC_DECORATION:
            decoration associated with the value
      - VTN_DEC_EXECUTION_MODE:
            an execution mode associated with an entrypoint value
      - VTN_DEC_STRUCT_MEMBER0 + m:
            decoration associated with member m of a struct value
      - VTN_DEC_STRUCT_MEMBER_NAME0 - m:
            name of m'th member of a struct value
    */
   int scope;

   uint32_t num_operands;
   const uint32_t *operands;
   struct vtn_value *group;

   union {
      SpvDecoration decoration;
      SpvExecutionMode exec_mode;
      const char *member_name;
   };
};

struct vtn_builder {
   nir_builder nb;

   linear_ctx *lin_ctx;

   /* Used by vtn_fail to jump back to the beginning of SPIR-V compilation */
   jmp_buf fail_jump;

   const uint32_t *spirv;
   size_t spirv_word_count;
   uint32_t version;

   nir_shader *shader;
   struct spirv_to_nir_options *options;
   struct vtn_block *block;

   /* Current offset, file, line, and column.  Useful for debugging.  Set
    * automatically by vtn_foreach_instruction.
    */
   size_t spirv_offset;
   const char *file;
   int line, col;

   /*
    * Map from phi instructions (pointer to the start of the instruction)
    * to the variable corresponding to it.
    */
   struct hash_table *phi_table;

   /* In Vulkan, when lowering some modes variable access, the derefs of the
    * variables are replaced with a resource index intrinsics, leaving the
    * variable hanging.  This set keeps track of them so they can be filtered
    * (and not removed) in nir_remove_dead_variables.
    */
   struct set *vars_used_indirectly;

   unsigned num_specializations;
   struct nir_spirv_specialization *specializations;

   unsigned value_id_bound;
   struct vtn_value *values;

   /* Information on the origin of the SPIR-V */
   enum vtn_generator generator_id;
   SpvSourceLanguage source_lang;

   /* True if we need to fix up CS OpControlBarrier */
   bool wa_glslang_cs_barrier;

   /* True if we need to ignore undef initializers */
   bool wa_llvm_spirv_ignore_workgroup_initializer;

   /* True if we need to ignore OpReturn after OpEmitMeshTasksEXT. */
   bool wa_ignore_return_after_emit_mesh_tasks;

   /* Workaround discard bugs in HLSL -> SPIR-V compilers */
   bool uses_demote_to_helper_invocation;
   bool convert_discard_to_demote;

   gl_shader_stage entry_point_stage;
   const char *entry_point_name;
   struct vtn_value *entry_point;
   struct vtn_value *workgroup_size_builtin;
   bool variable_pointers;
   bool image_gather_bias_lod;

   uint32_t *interface_ids;
   size_t interface_ids_count;

   struct vtn_function *func;
   struct list_head functions;

   /* Current function parameter index */
   unsigned func_param_idx;

   /* false by default, set to true by the ContractionOff execution mode */
   bool exact;

   /* when a physical memory model is choosen */
   bool physical_ptrs;

   /* memory model specified by OpMemoryModel */
   unsigned mem_model;
};

const char *
vtn_string_literal(struct vtn_builder *b, const uint32_t *words,
                   unsigned word_count, unsigned *words_used);

nir_def *
vtn_pointer_to_ssa(struct vtn_builder *b, struct vtn_pointer *ptr);
struct vtn_pointer *
vtn_pointer_from_ssa(struct vtn_builder *b, nir_def *ssa,
                     struct vtn_type *ptr_type);

struct vtn_ssa_value *
vtn_const_ssa_value(struct vtn_builder *b, nir_constant *constant,
                    const struct glsl_type *type);

static inline struct vtn_value *
vtn_untyped_value(struct vtn_builder *b, uint32_t value_id)
{
   vtn_fail_if(value_id >= b->value_id_bound,
               "SPIR-V id %u is out-of-bounds", value_id);
   return &b->values[value_id];
}

static inline uint32_t
vtn_id_for_value(struct vtn_builder *b, struct vtn_value *value)
{
   vtn_fail_if(value <= b->values, "vtn_value pointer outside the range of valid values");
   uint32_t value_id = value - b->values;
   vtn_fail_if(value_id >= b->value_id_bound, "vtn_value pointer outside the range of valid values");
   return value_id;
}

/* Consider not using this function directly and instead use
 * vtn_push_ssa/vtn_push_pointer so that appropriate applying of
 * decorations is handled by common code.
 */
static inline struct vtn_value *
vtn_push_value(struct vtn_builder *b, uint32_t value_id,
               enum vtn_value_type value_type)
{
   struct vtn_value *val = vtn_untyped_value(b, value_id);

   vtn_fail_if(value_type == vtn_value_type_ssa,
               "Do not call vtn_push_value for value_type_ssa.  Use "
               "vtn_push_ssa_value instead.");

   vtn_fail_if(val->value_type != vtn_value_type_invalid,
               "SPIR-V id %u has already been written by another instruction",
               value_id);

   val->value_type = value_type;

   return &b->values[value_id];
}

/* These separated fail functions exist so the helpers like vtn_value()
 * can be inlined with minimal code size impact.  This allows the failure
 * handling to have more detailed output without harming callers.
 */

void _vtn_fail_value_type_mismatch(struct vtn_builder *b, uint32_t value_id,
                                   enum vtn_value_type value_type);
void _vtn_fail_value_not_pointer(struct vtn_builder *b, uint32_t value_id);

static inline struct vtn_value *
vtn_value(struct vtn_builder *b, uint32_t value_id,
          enum vtn_value_type value_type)
{
   struct vtn_value *val = vtn_untyped_value(b, value_id);
   if (unlikely(val->value_type != value_type))
      _vtn_fail_value_type_mismatch(b, value_id, value_type);
   return val;
}

static inline struct vtn_value *
vtn_pointer_value(struct vtn_builder *b, uint32_t value_id)
{
   struct vtn_value *val = vtn_untyped_value(b, value_id);
   if (unlikely(val->value_type != vtn_value_type_pointer &&
                !val->is_null_constant))
      _vtn_fail_value_not_pointer(b, value_id);
   return val;
}

static inline struct vtn_pointer *
vtn_value_to_pointer(struct vtn_builder *b, struct vtn_value *value)
{
   if (value->is_null_constant) {
      vtn_assert(glsl_type_is_vector_or_scalar(value->type->type));
      nir_def *const_ssa =
         vtn_const_ssa_value(b, value->constant, value->type->type)->def;
      return vtn_pointer_from_ssa(b, const_ssa, value->type);
   }
   vtn_assert(value->value_type == vtn_value_type_pointer);
   return value->pointer;
}

static inline struct vtn_pointer *
vtn_pointer(struct vtn_builder *b, uint32_t value_id)
{
   return vtn_value_to_pointer(b, vtn_pointer_value(b, value_id));
}

bool
vtn_set_instruction_result_type(struct vtn_builder *b, SpvOp opcode,
                                const uint32_t *w, unsigned count);

static inline uint64_t
vtn_constant_uint(struct vtn_builder *b, uint32_t value_id)
{
   struct vtn_value *val = vtn_value(b, value_id, vtn_value_type_constant);

   vtn_fail_if(val->type->base_type != vtn_base_type_scalar ||
               !glsl_type_is_integer(val->type->type),
               "Expected id %u to be an integer constant", value_id);

   switch (glsl_get_bit_size(val->type->type)) {
   case 8:  return val->constant->values[0].u8;
   case 16: return val->constant->values[0].u16;
   case 32: return val->constant->values[0].u32;
   case 64: return val->constant->values[0].u64;
   default: unreachable("Invalid bit size");
   }
}

static inline int64_t
vtn_constant_int(struct vtn_builder *b, uint32_t value_id)
{
   struct vtn_value *val = vtn_value(b, value_id, vtn_value_type_constant);

   vtn_fail_if(val->type->base_type != vtn_base_type_scalar ||
               !glsl_type_is_integer(val->type->type),
               "Expected id %u to be an integer constant", value_id);

   switch (glsl_get_bit_size(val->type->type)) {
   case 8:  return val->constant->values[0].i8;
   case 16: return val->constant->values[0].i16;
   case 32: return val->constant->values[0].i32;
   case 64: return val->constant->values[0].i64;
   default: unreachable("Invalid bit size");
   }
}

static inline struct vtn_type *
vtn_get_value_type(struct vtn_builder *b, uint32_t value_id)
{
   struct vtn_value *val = vtn_untyped_value(b, value_id);
   vtn_fail_if(val->type == NULL, "Value %u does not have a type", value_id);
   return val->type;
}

static inline struct vtn_type *
vtn_get_type(struct vtn_builder *b, uint32_t value_id)
{
   return vtn_value(b, value_id, vtn_value_type_type)->type;
}

static inline struct vtn_block *
vtn_block(struct vtn_builder *b, uint32_t value_id)
{
   return vtn_value(b, value_id, vtn_value_type_block)->block;
}

struct vtn_ssa_value *vtn_ssa_value(struct vtn_builder *b, uint32_t value_id);
struct vtn_value *vtn_push_ssa_value(struct vtn_builder *b, uint32_t value_id,
                                     struct vtn_ssa_value *ssa);

nir_def *vtn_get_nir_ssa(struct vtn_builder *b, uint32_t value_id);
struct vtn_value *vtn_push_nir_ssa(struct vtn_builder *b, uint32_t value_id,
                                   nir_def *def);
nir_deref_instr *vtn_get_deref_for_id(struct vtn_builder *b, uint32_t value_id);
nir_deref_instr *vtn_get_deref_for_ssa_value(struct vtn_builder *b, struct vtn_ssa_value *ssa);
struct vtn_value *vtn_push_var_ssa(struct vtn_builder *b, uint32_t value_id,
                                   nir_variable *var);

struct vtn_value *vtn_push_pointer(struct vtn_builder *b,
                                   uint32_t value_id,
                                   struct vtn_pointer *ptr);

struct vtn_sampled_image {
   nir_deref_instr *image;
   nir_deref_instr *sampler;
};

nir_def *vtn_sampled_image_to_nir_ssa(struct vtn_builder *b,
                                          struct vtn_sampled_image si);

void
vtn_copy_value(struct vtn_builder *b, uint32_t src_value_id,
               uint32_t dst_value_id);

struct vtn_ssa_value *vtn_create_ssa_value(struct vtn_builder *b,
                                           const struct glsl_type *type);
void vtn_set_ssa_value_var(struct vtn_builder *b, struct vtn_ssa_value *ssa, nir_variable *var);

struct vtn_ssa_value *vtn_ssa_transpose(struct vtn_builder *b,
                                        struct vtn_ssa_value *src);

nir_deref_instr *vtn_nir_deref(struct vtn_builder *b, uint32_t id);

nir_deref_instr *vtn_pointer_to_deref(struct vtn_builder *b,
                                      struct vtn_pointer *ptr);
nir_def *
vtn_pointer_to_offset(struct vtn_builder *b, struct vtn_pointer *ptr,
                      nir_def **index_out);

nir_deref_instr *
vtn_get_call_payload_for_location(struct vtn_builder *b, uint32_t location_id);

struct vtn_ssa_value *
vtn_local_load(struct vtn_builder *b, nir_deref_instr *src,
               enum gl_access_qualifier access);

void vtn_local_store(struct vtn_builder *b, struct vtn_ssa_value *src,
                     nir_deref_instr *dest,
                     enum gl_access_qualifier access);

struct vtn_ssa_value *
vtn_variable_load(struct vtn_builder *b, struct vtn_pointer *src,
                  enum gl_access_qualifier access);

void vtn_variable_store(struct vtn_builder *b, struct vtn_ssa_value *src,
                        struct vtn_pointer *dest, enum gl_access_qualifier access);

void vtn_handle_variables(struct vtn_builder *b, SpvOp opcode,
                          const uint32_t *w, unsigned count);


typedef void (*vtn_decoration_foreach_cb)(struct vtn_builder *,
                                          struct vtn_value *,
                                          int member,
                                          const struct vtn_decoration *,
                                          void *);

void vtn_foreach_decoration(struct vtn_builder *b, struct vtn_value *value,
                            vtn_decoration_foreach_cb cb, void *data);

typedef void (*vtn_execution_mode_foreach_cb)(struct vtn_builder *,
                                              struct vtn_value *,
                                              const struct vtn_decoration *,
                                              void *);

void vtn_foreach_execution_mode(struct vtn_builder *b, struct vtn_value *value,
                                vtn_execution_mode_foreach_cb cb, void *data);

nir_op vtn_nir_alu_op_for_spirv_opcode(struct vtn_builder *b,
                                       SpvOp opcode, bool *swap, bool *exact,
                                       unsigned src_bit_size, unsigned dst_bit_size);

void vtn_handle_alu(struct vtn_builder *b, SpvOp opcode,
                    const uint32_t *w, unsigned count);

void vtn_handle_integer_dot(struct vtn_builder *b, SpvOp opcode,
                            const uint32_t *w, unsigned count);

void vtn_handle_bitcast(struct vtn_builder *b, const uint32_t *w,
                        unsigned count);

void vtn_handle_no_contraction(struct vtn_builder *b, struct vtn_value *val);

void vtn_handle_subgroup(struct vtn_builder *b, SpvOp opcode,
                         const uint32_t *w, unsigned count);

bool vtn_handle_glsl450_instruction(struct vtn_builder *b, SpvOp ext_opcode,
                                    const uint32_t *words, unsigned count);

bool vtn_handle_opencl_instruction(struct vtn_builder *b, SpvOp ext_opcode,
                                   const uint32_t *words, unsigned count);
bool vtn_handle_opencl_core_instruction(struct vtn_builder *b, SpvOp opcode,
                                        const uint32_t *w, unsigned count);

struct vtn_builder* vtn_create_builder(const uint32_t *words, size_t word_count,
                                       gl_shader_stage stage, const char *entry_point_name,
                                       const struct spirv_to_nir_options *options);

void vtn_handle_entry_point(struct vtn_builder *b, const uint32_t *w,
                            unsigned count);

void vtn_handle_debug_text(struct vtn_builder *b, SpvOp opcode,
                           const uint32_t *w, unsigned count);

void vtn_handle_decoration(struct vtn_builder *b, SpvOp opcode,
                           const uint32_t *w, unsigned count);

enum vtn_variable_mode vtn_storage_class_to_mode(struct vtn_builder *b,
                                                 SpvStorageClass class,
                                                 struct vtn_type *interface_type,
                                                 nir_variable_mode *nir_mode_out);

nir_address_format vtn_mode_to_address_format(struct vtn_builder *b,
                                              enum vtn_variable_mode);

nir_rounding_mode vtn_rounding_mode_to_nir(struct vtn_builder *b,
                                           SpvFPRoundingMode mode);

static inline uint32_t
vtn_align_u32(uint32_t v, uint32_t a)
{
   assert(a != 0 && a == (a & -((int32_t) a)));
   return (v + a - 1) & ~(a - 1);
}

static inline uint64_t
vtn_u64_literal(const uint32_t *w)
{
   return (uint64_t)w[1] << 32 | w[0];
}

bool vtn_handle_amd_gcn_shader_instruction(struct vtn_builder *b, SpvOp ext_opcode,
                                           const uint32_t *words, unsigned count);

bool vtn_handle_amd_shader_ballot_instruction(struct vtn_builder *b, SpvOp ext_opcode,
                                              const uint32_t *w, unsigned count);

bool vtn_handle_amd_shader_trinary_minmax_instruction(struct vtn_builder *b, SpvOp ext_opcode,
						      const uint32_t *words, unsigned count);

bool vtn_handle_amd_shader_explicit_vertex_parameter_instruction(struct vtn_builder *b,
                                                                 SpvOp ext_opcode,
                                                                 const uint32_t *words,
                                                                 unsigned count);

SpvMemorySemanticsMask vtn_mode_to_memory_semantics(enum vtn_variable_mode mode);

void vtn_emit_memory_barrier(struct vtn_builder *b, SpvScope scope,
                             SpvMemorySemanticsMask semantics);

bool vtn_value_is_relaxed_precision(struct vtn_builder *b, struct vtn_value *val);
nir_def *
vtn_mediump_downconvert(struct vtn_builder *b, enum glsl_base_type base_type, nir_def *def);
struct vtn_ssa_value *
vtn_mediump_downconvert_value(struct vtn_builder *b, struct vtn_ssa_value *src);
void vtn_mediump_upconvert_value(struct vtn_builder *b, struct vtn_ssa_value *value);

static inline int
cmp_uint32_t(const void *pa, const void *pb)
{
   uint32_t a = *((const uint32_t *)pa);
   uint32_t b = *((const uint32_t *)pb);
   if (a < b)
      return -1;
   if (a > b)
      return 1;
   return 0;
}

void
vtn_parse_switch(struct vtn_builder *b,
                 const uint32_t *branch,
                 struct list_head *case_list);

bool vtn_get_mem_operands(struct vtn_builder *b, const uint32_t *w, unsigned count,
                          unsigned *idx, SpvMemoryAccessMask *access, unsigned *alignment,
                          SpvScope *dest_scope, SpvScope *src_scope);
void vtn_emit_make_visible_barrier(struct vtn_builder *b, SpvMemoryAccessMask access,
                                   SpvScope scope, enum vtn_variable_mode mode);
void vtn_emit_make_available_barrier(struct vtn_builder *b, SpvMemoryAccessMask access,
                                     SpvScope scope, enum vtn_variable_mode mode);


void vtn_handle_cooperative_type(struct vtn_builder *b, struct vtn_value *val,
                                 SpvOp opcode, const uint32_t *w, unsigned count);
void vtn_handle_cooperative_instruction(struct vtn_builder *b, SpvOp opcode,
                                        const uint32_t *w, unsigned count);
void vtn_handle_cooperative_alu(struct vtn_builder *b, struct vtn_value *dest_val,
                                const struct glsl_type *dest_type, SpvOp opcode,
                                const uint32_t *w, unsigned count);
struct vtn_ssa_value *vtn_cooperative_matrix_extract(struct vtn_builder *b, struct vtn_ssa_value *mat,
                                                     const uint32_t *indices, unsigned num_indices);
struct vtn_ssa_value *vtn_cooperative_matrix_insert(struct vtn_builder *b, struct vtn_ssa_value *mat,
                                                    struct vtn_ssa_value *insert,
                                                    const uint32_t *indices, unsigned num_indices);
nir_deref_instr *vtn_create_cmat_temporary(struct vtn_builder *b,
                                           const struct glsl_type *t, const char *name);

gl_shader_stage vtn_stage_for_execution_model(SpvExecutionModel model);

#endif /* _VTN_PRIVATE_H_ */
