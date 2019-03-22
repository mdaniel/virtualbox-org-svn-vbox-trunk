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
 *
 * Authors:
 *    Jason Ekstrand (jason@jlekstrand.net)
 *
 */

#include "vtn_private.h"
#include "nir/nir_vla.h"
#include "nir/nir_control_flow.h"
#include "nir/nir_constant_expressions.h"
#include "spirv_info.h"

struct spec_constant_value {
   bool is_double;
   union {
      uint32_t data32;
      uint64_t data64;
   };
};

void
_vtn_warn(const char *file, int line, const char *msg, ...)
{
   char *formatted;
   va_list args;

   va_start(args, msg);
   formatted = ralloc_vasprintf(NULL, msg, args);
   va_end(args);

   fprintf(stderr, "%s:%d WARNING: %s\n", file, line, formatted);

   ralloc_free(formatted);
}

static struct vtn_ssa_value *
vtn_undef_ssa_value(struct vtn_builder *b, const struct glsl_type *type)
{
   struct vtn_ssa_value *val = rzalloc(b, struct vtn_ssa_value);
   val->type = type;

   if (glsl_type_is_vector_or_scalar(type)) {
      unsigned num_components = glsl_get_vector_elements(val->type);
      unsigned bit_size = glsl_get_bit_size(val->type);
      val->def = nir_ssa_undef(&b->nb, num_components, bit_size);
   } else {
      unsigned elems = glsl_get_length(val->type);
      val->elems = ralloc_array(b, struct vtn_ssa_value *, elems);
      if (glsl_type_is_matrix(type)) {
         const struct glsl_type *elem_type =
            glsl_vector_type(glsl_get_base_type(type),
                             glsl_get_vector_elements(type));

         for (unsigned i = 0; i < elems; i++)
            val->elems[i] = vtn_undef_ssa_value(b, elem_type);
      } else if (glsl_type_is_array(type)) {
         const struct glsl_type *elem_type = glsl_get_array_element(type);
         for (unsigned i = 0; i < elems; i++)
            val->elems[i] = vtn_undef_ssa_value(b, elem_type);
      } else {
         for (unsigned i = 0; i < elems; i++) {
            const struct glsl_type *elem_type = glsl_get_struct_field(type, i);
            val->elems[i] = vtn_undef_ssa_value(b, elem_type);
         }
      }
   }

   return val;
}

static struct vtn_ssa_value *
vtn_const_ssa_value(struct vtn_builder *b, nir_constant *constant,
                    const struct glsl_type *type)
{
   struct hash_entry *entry = _mesa_hash_table_search(b->const_table, constant);

   if (entry)
      return entry->data;

   struct vtn_ssa_value *val = rzalloc(b, struct vtn_ssa_value);
   val->type = type;

   switch (glsl_get_base_type(type)) {
   case GLSL_TYPE_INT:
   case GLSL_TYPE_UINT:
   case GLSL_TYPE_INT64:
   case GLSL_TYPE_UINT64:
   case GLSL_TYPE_BOOL:
   case GLSL_TYPE_FLOAT:
   case GLSL_TYPE_DOUBLE: {
      int bit_size = glsl_get_bit_size(type);
      if (glsl_type_is_vector_or_scalar(type)) {
         unsigned num_components = glsl_get_vector_elements(val->type);
         nir_load_const_instr *load =
            nir_load_const_instr_create(b->shader, num_components, bit_size);

         load->value = constant->values[0];

         nir_instr_insert_before_cf_list(&b->impl->body, &load->instr);
         val->def = &load->def;
      } else {
         assert(glsl_type_is_matrix(type));
         unsigned rows = glsl_get_vector_elements(val->type);
         unsigned columns = glsl_get_matrix_columns(val->type);
         val->elems = ralloc_array(b, struct vtn_ssa_value *, columns);

         for (unsigned i = 0; i < columns; i++) {
            struct vtn_ssa_value *col_val = rzalloc(b, struct vtn_ssa_value);
            col_val->type = glsl_get_column_type(val->type);
            nir_load_const_instr *load =
               nir_load_const_instr_create(b->shader, rows, bit_size);

            load->value = constant->values[i];

            nir_instr_insert_before_cf_list(&b->impl->body, &load->instr);
            col_val->def = &load->def;

            val->elems[i] = col_val;
         }
      }
      break;
   }

   case GLSL_TYPE_ARRAY: {
      unsigned elems = glsl_get_length(val->type);
      val->elems = ralloc_array(b, struct vtn_ssa_value *, elems);
      const struct glsl_type *elem_type = glsl_get_array_element(val->type);
      for (unsigned i = 0; i < elems; i++)
         val->elems[i] = vtn_const_ssa_value(b, constant->elements[i],
                                             elem_type);
      break;
   }

   case GLSL_TYPE_STRUCT: {
      unsigned elems = glsl_get_length(val->type);
      val->elems = ralloc_array(b, struct vtn_ssa_value *, elems);
      for (unsigned i = 0; i < elems; i++) {
         const struct glsl_type *elem_type =
            glsl_get_struct_field(val->type, i);
         val->elems[i] = vtn_const_ssa_value(b, constant->elements[i],
                                             elem_type);
      }
      break;
   }

   default:
      unreachable("bad constant type");
   }

   return val;
}

struct vtn_ssa_value *
vtn_ssa_value(struct vtn_builder *b, uint32_t value_id)
{
   struct vtn_value *val = vtn_untyped_value(b, value_id);
   switch (val->value_type) {
   case vtn_value_type_undef:
      return vtn_undef_ssa_value(b, val->type->type);

   case vtn_value_type_constant:
      return vtn_const_ssa_value(b, val->constant, val->const_type);

   case vtn_value_type_ssa:
      return val->ssa;

   case vtn_value_type_pointer:
      assert(val->pointer->ptr_type && val->pointer->ptr_type->type);
      struct vtn_ssa_value *ssa =
         vtn_create_ssa_value(b, val->pointer->ptr_type->type);
      ssa->def = vtn_pointer_to_ssa(b, val->pointer);
      return ssa;

   default:
      unreachable("Invalid type for an SSA value");
   }
}

static char *
vtn_string_literal(struct vtn_builder *b, const uint32_t *words,
                   unsigned word_count, unsigned *words_used)
{
   char *dup = ralloc_strndup(b, (char *)words, word_count * sizeof(*words));
   if (words_used) {
      /* Ammount of space taken by the string (including the null) */
      unsigned len = strlen(dup) + 1;
      *words_used = DIV_ROUND_UP(len, sizeof(*words));
   }
   return dup;
}

const uint32_t *
vtn_foreach_instruction(struct vtn_builder *b, const uint32_t *start,
                        const uint32_t *end, vtn_instruction_handler handler)
{
   b->file = NULL;
   b->line = -1;
   b->col = -1;

   const uint32_t *w = start;
   while (w < end) {
      SpvOp opcode = w[0] & SpvOpCodeMask;
      unsigned count = w[0] >> SpvWordCountShift;
      assert(count >= 1 && w + count <= end);

      switch (opcode) {
      case SpvOpNop:
         break; /* Do nothing */

      case SpvOpLine:
         b->file = vtn_value(b, w[1], vtn_value_type_string)->str;
         b->line = w[2];
         b->col = w[3];
         break;

      case SpvOpNoLine:
         b->file = NULL;
         b->line = -1;
         b->col = -1;
         break;

      default:
         if (!handler(b, opcode, w, count))
            return w;
         break;
      }

      w += count;
   }
   assert(w == end);
   return w;
}

static void
vtn_handle_extension(struct vtn_builder *b, SpvOp opcode,
                     const uint32_t *w, unsigned count)
{
   switch (opcode) {
   case SpvOpExtInstImport: {
      struct vtn_value *val = vtn_push_value(b, w[1], vtn_value_type_extension);
      if (strcmp((const char *)&w[2], "GLSL.std.450") == 0) {
         val->ext_handler = vtn_handle_glsl450_instruction;
      } else {
         unreachable("Unsupported extension");
      }
      break;
   }

   case SpvOpExtInst: {
      struct vtn_value *val = vtn_value(b, w[3], vtn_value_type_extension);
      bool handled = val->ext_handler(b, w[4], w, count);
      (void)handled;
      assert(handled);
      break;
   }

   default:
      unreachable("Unhandled opcode");
   }
}

static void
_foreach_decoration_helper(struct vtn_builder *b,
                           struct vtn_value *base_value,
                           int parent_member,
                           struct vtn_value *value,
                           vtn_decoration_foreach_cb cb, void *data)
{
   for (struct vtn_decoration *dec = value->decoration; dec; dec = dec->next) {
      int member;
      if (dec->scope == VTN_DEC_DECORATION) {
         member = parent_member;
      } else if (dec->scope >= VTN_DEC_STRUCT_MEMBER0) {
         assert(parent_member == -1);
         member = dec->scope - VTN_DEC_STRUCT_MEMBER0;
      } else {
         /* Not a decoration */
         continue;
      }

      if (dec->group) {
         assert(dec->group->value_type == vtn_value_type_decoration_group);
         _foreach_decoration_helper(b, base_value, member, dec->group,
                                    cb, data);
      } else {
         cb(b, base_value, member, dec, data);
      }
   }
}

/** Iterates (recursively if needed) over all of the decorations on a value
 *
 * This function iterates over all of the decorations applied to a given
 * value.  If it encounters a decoration group, it recurses into the group
 * and iterates over all of those decorations as well.
 */
void
vtn_foreach_decoration(struct vtn_builder *b, struct vtn_value *value,
                       vtn_decoration_foreach_cb cb, void *data)
{
   _foreach_decoration_helper(b, value, -1, value, cb, data);
}

void
vtn_foreach_execution_mode(struct vtn_builder *b, struct vtn_value *value,
                           vtn_execution_mode_foreach_cb cb, void *data)
{
   for (struct vtn_decoration *dec = value->decoration; dec; dec = dec->next) {
      if (dec->scope != VTN_DEC_EXECUTION_MODE)
         continue;

      assert(dec->group == NULL);
      cb(b, value, dec, data);
   }
}

static void
vtn_handle_decoration(struct vtn_builder *b, SpvOp opcode,
                      const uint32_t *w, unsigned count)
{
   const uint32_t *w_end = w + count;
   const uint32_t target = w[1];
   w += 2;

   switch (opcode) {
   case SpvOpDecorationGroup:
      vtn_push_value(b, target, vtn_value_type_decoration_group);
      break;

   case SpvOpDecorate:
   case SpvOpMemberDecorate:
   case SpvOpExecutionMode: {
      struct vtn_value *val = &b->values[target];

      struct vtn_decoration *dec = rzalloc(b, struct vtn_decoration);
      switch (opcode) {
      case SpvOpDecorate:
         dec->scope = VTN_DEC_DECORATION;
         break;
      case SpvOpMemberDecorate:
         dec->scope = VTN_DEC_STRUCT_MEMBER0 + *(w++);
         break;
      case SpvOpExecutionMode:
         dec->scope = VTN_DEC_EXECUTION_MODE;
         break;
      default:
         unreachable("Invalid decoration opcode");
      }
      dec->decoration = *(w++);
      dec->literals = w;

      /* Link into the list */
      dec->next = val->decoration;
      val->decoration = dec;
      break;
   }

   case SpvOpGroupMemberDecorate:
   case SpvOpGroupDecorate: {
      struct vtn_value *group =
         vtn_value(b, target, vtn_value_type_decoration_group);

      for (; w < w_end; w++) {
         struct vtn_value *val = vtn_untyped_value(b, *w);
         struct vtn_decoration *dec = rzalloc(b, struct vtn_decoration);

         dec->group = group;
         if (opcode == SpvOpGroupDecorate) {
            dec->scope = VTN_DEC_DECORATION;
         } else {
            dec->scope = VTN_DEC_STRUCT_MEMBER0 + *(++w);
         }

         /* Link into the list */
         dec->next = val->decoration;
         val->decoration = dec;
      }
      break;
   }

   default:
      unreachable("Unhandled opcode");
   }
}

struct member_decoration_ctx {
   unsigned num_fields;
   struct glsl_struct_field *fields;
   struct vtn_type *type;
};

/* does a shallow copy of a vtn_type */

static struct vtn_type *
vtn_type_copy(struct vtn_builder *b, struct vtn_type *src)
{
   struct vtn_type *dest = ralloc(b, struct vtn_type);
   *dest = *src;

   switch (src->base_type) {
   case vtn_base_type_void:
   case vtn_base_type_scalar:
   case vtn_base_type_vector:
   case vtn_base_type_matrix:
   case vtn_base_type_array:
   case vtn_base_type_pointer:
   case vtn_base_type_image:
   case vtn_base_type_sampler:
      /* Nothing more to do */
      break;

   case vtn_base_type_struct:
      dest->members = ralloc_array(b, struct vtn_type *, src->length);
      memcpy(dest->members, src->members,
             src->length * sizeof(src->members[0]));

      dest->offsets = ralloc_array(b, unsigned, src->length);
      memcpy(dest->offsets, src->offsets,
             src->length * sizeof(src->offsets[0]));
      break;

   case vtn_base_type_function:
      dest->params = ralloc_array(b, struct vtn_type *, src->length);
      memcpy(dest->params, src->params, src->length * sizeof(src->params[0]));
      break;
   }

   return dest;
}

static struct vtn_type *
mutable_matrix_member(struct vtn_builder *b, struct vtn_type *type, int member)
{
   type->members[member] = vtn_type_copy(b, type->members[member]);
   type = type->members[member];

   /* We may have an array of matrices.... Oh, joy! */
   while (glsl_type_is_array(type->type)) {
      type->array_element = vtn_type_copy(b, type->array_element);
      type = type->array_element;
   }

   assert(glsl_type_is_matrix(type->type));

   return type;
}

static void
struct_member_decoration_cb(struct vtn_builder *b,
                            struct vtn_value *val, int member,
                            const struct vtn_decoration *dec, void *void_ctx)
{
   struct member_decoration_ctx *ctx = void_ctx;

   if (member < 0)
      return;

   assert(member < ctx->num_fields);

   switch (dec->decoration) {
   case SpvDecorationNonWritable:
   case SpvDecorationNonReadable:
   case SpvDecorationRelaxedPrecision:
   case SpvDecorationVolatile:
   case SpvDecorationCoherent:
   case SpvDecorationUniform:
      break; /* FIXME: Do nothing with this for now. */
   case SpvDecorationNoPerspective:
      ctx->fields[member].interpolation = INTERP_MODE_NOPERSPECTIVE;
      break;
   case SpvDecorationFlat:
      ctx->fields[member].interpolation = INTERP_MODE_FLAT;
      break;
   case SpvDecorationCentroid:
      ctx->fields[member].centroid = true;
      break;
   case SpvDecorationSample:
      ctx->fields[member].sample = true;
      break;
   case SpvDecorationStream:
      /* Vulkan only allows one GS stream */
      assert(dec->literals[0] == 0);
      break;
   case SpvDecorationLocation:
      ctx->fields[member].location = dec->literals[0];
      break;
   case SpvDecorationComponent:
      break; /* FIXME: What should we do with these? */
   case SpvDecorationBuiltIn:
      ctx->type->members[member] = vtn_type_copy(b, ctx->type->members[member]);
      ctx->type->members[member]->is_builtin = true;
      ctx->type->members[member]->builtin = dec->literals[0];
      ctx->type->builtin_block = true;
      break;
   case SpvDecorationOffset:
      ctx->type->offsets[member] = dec->literals[0];
      break;
   case SpvDecorationMatrixStride:
      /* Handled as a second pass */
      break;
   case SpvDecorationColMajor:
      break; /* Nothing to do here.  Column-major is the default. */
   case SpvDecorationRowMajor:
      mutable_matrix_member(b, ctx->type, member)->row_major = true;
      break;

   case SpvDecorationPatch:
      break;

   case SpvDecorationSpecId:
   case SpvDecorationBlock:
   case SpvDecorationBufferBlock:
   case SpvDecorationArrayStride:
   case SpvDecorationGLSLShared:
   case SpvDecorationGLSLPacked:
   case SpvDecorationInvariant:
   case SpvDecorationRestrict:
   case SpvDecorationAliased:
   case SpvDecorationConstant:
   case SpvDecorationIndex:
   case SpvDecorationBinding:
   case SpvDecorationDescriptorSet:
   case SpvDecorationLinkageAttributes:
   case SpvDecorationNoContraction:
   case SpvDecorationInputAttachmentIndex:
      vtn_warn("Decoration not allowed on struct members: %s",
               spirv_decoration_to_string(dec->decoration));
      break;

   case SpvDecorationXfbBuffer:
   case SpvDecorationXfbStride:
      vtn_warn("Vulkan does not have transform feedback");
      break;

   case SpvDecorationCPacked:
   case SpvDecorationSaturatedConversion:
   case SpvDecorationFuncParamAttr:
   case SpvDecorationFPRoundingMode:
   case SpvDecorationFPFastMathMode:
   case SpvDecorationAlignment:
      vtn_warn("Decoration only allowed for CL-style kernels: %s",
               spirv_decoration_to_string(dec->decoration));
      break;

   default:
      unreachable("Unhandled decoration");
   }
}

/* Matrix strides are handled as a separate pass because we need to know
 * whether the matrix is row-major or not first.
 */
static void
struct_member_matrix_stride_cb(struct vtn_builder *b,
                               struct vtn_value *val, int member,
                               const struct vtn_decoration *dec,
                               void *void_ctx)
{
   if (dec->decoration != SpvDecorationMatrixStride)
      return;
   assert(member >= 0);

   struct member_decoration_ctx *ctx = void_ctx;

   struct vtn_type *mat_type = mutable_matrix_member(b, ctx->type, member);
   if (mat_type->row_major) {
      mat_type->array_element = vtn_type_copy(b, mat_type->array_element);
      mat_type->stride = mat_type->array_element->stride;
      mat_type->array_element->stride = dec->literals[0];
   } else {
      assert(mat_type->array_element->stride > 0);
      mat_type->stride = dec->literals[0];
   }
}

static void
type_decoration_cb(struct vtn_builder *b,
                   struct vtn_value *val, int member,
                    const struct vtn_decoration *dec, void *ctx)
{
   struct vtn_type *type = val->type;

   if (member != -1)
      return;

   switch (dec->decoration) {
   case SpvDecorationArrayStride:
      assert(type->base_type == vtn_base_type_matrix ||
             type->base_type == vtn_base_type_array ||
             type->base_type == vtn_base_type_pointer);
      type->stride = dec->literals[0];
      break;
   case SpvDecorationBlock:
      assert(type->base_type == vtn_base_type_struct);
      type->block = true;
      break;
   case SpvDecorationBufferBlock:
      assert(type->base_type == vtn_base_type_struct);
      type->buffer_block = true;
      break;
   case SpvDecorationGLSLShared:
   case SpvDecorationGLSLPacked:
      /* Ignore these, since we get explicit offsets anyways */
      break;

   case SpvDecorationRowMajor:
   case SpvDecorationColMajor:
   case SpvDecorationMatrixStride:
   case SpvDecorationBuiltIn:
   case SpvDecorationNoPerspective:
   case SpvDecorationFlat:
   case SpvDecorationPatch:
   case SpvDecorationCentroid:
   case SpvDecorationSample:
   case SpvDecorationVolatile:
   case SpvDecorationCoherent:
   case SpvDecorationNonWritable:
   case SpvDecorationNonReadable:
   case SpvDecorationUniform:
   case SpvDecorationStream:
   case SpvDecorationLocation:
   case SpvDecorationComponent:
   case SpvDecorationOffset:
   case SpvDecorationXfbBuffer:
   case SpvDecorationXfbStride:
      vtn_warn("Decoration only allowed for struct members: %s",
               spirv_decoration_to_string(dec->decoration));
      break;

   case SpvDecorationRelaxedPrecision:
   case SpvDecorationSpecId:
   case SpvDecorationInvariant:
   case SpvDecorationRestrict:
   case SpvDecorationAliased:
   case SpvDecorationConstant:
   case SpvDecorationIndex:
   case SpvDecorationBinding:
   case SpvDecorationDescriptorSet:
   case SpvDecorationLinkageAttributes:
   case SpvDecorationNoContraction:
   case SpvDecorationInputAttachmentIndex:
      vtn_warn("Decoration not allowed on types: %s",
               spirv_decoration_to_string(dec->decoration));
      break;

   case SpvDecorationCPacked:
   case SpvDecorationSaturatedConversion:
   case SpvDecorationFuncParamAttr:
   case SpvDecorationFPRoundingMode:
   case SpvDecorationFPFastMathMode:
   case SpvDecorationAlignment:
      vtn_warn("Decoration only allowed for CL-style kernels: %s",
               spirv_decoration_to_string(dec->decoration));
      break;

   default:
      unreachable("Unhandled decoration");
   }
}

static unsigned
translate_image_format(SpvImageFormat format)
{
   switch (format) {
   case SpvImageFormatUnknown:      return 0;      /* GL_NONE */
   case SpvImageFormatRgba32f:      return 0x8814; /* GL_RGBA32F */
   case SpvImageFormatRgba16f:      return 0x881A; /* GL_RGBA16F */
   case SpvImageFormatR32f:         return 0x822E; /* GL_R32F */
   case SpvImageFormatRgba8:        return 0x8058; /* GL_RGBA8 */
   case SpvImageFormatRgba8Snorm:   return 0x8F97; /* GL_RGBA8_SNORM */
   case SpvImageFormatRg32f:        return 0x8230; /* GL_RG32F */
   case SpvImageFormatRg16f:        return 0x822F; /* GL_RG16F */
   case SpvImageFormatR11fG11fB10f: return 0x8C3A; /* GL_R11F_G11F_B10F */
   case SpvImageFormatR16f:         return 0x822D; /* GL_R16F */
   case SpvImageFormatRgba16:       return 0x805B; /* GL_RGBA16 */
   case SpvImageFormatRgb10A2:      return 0x8059; /* GL_RGB10_A2 */
   case SpvImageFormatRg16:         return 0x822C; /* GL_RG16 */
   case SpvImageFormatRg8:          return 0x822B; /* GL_RG8 */
   case SpvImageFormatR16:          return 0x822A; /* GL_R16 */
   case SpvImageFormatR8:           return 0x8229; /* GL_R8 */
   case SpvImageFormatRgba16Snorm:  return 0x8F9B; /* GL_RGBA16_SNORM */
   case SpvImageFormatRg16Snorm:    return 0x8F99; /* GL_RG16_SNORM */
   case SpvImageFormatRg8Snorm:     return 0x8F95; /* GL_RG8_SNORM */
   case SpvImageFormatR16Snorm:     return 0x8F98; /* GL_R16_SNORM */
   case SpvImageFormatR8Snorm:      return 0x8F94; /* GL_R8_SNORM */
   case SpvImageFormatRgba32i:      return 0x8D82; /* GL_RGBA32I */
   case SpvImageFormatRgba16i:      return 0x8D88; /* GL_RGBA16I */
   case SpvImageFormatRgba8i:       return 0x8D8E; /* GL_RGBA8I */
   case SpvImageFormatR32i:         return 0x8235; /* GL_R32I */
   case SpvImageFormatRg32i:        return 0x823B; /* GL_RG32I */
   case SpvImageFormatRg16i:        return 0x8239; /* GL_RG16I */
   case SpvImageFormatRg8i:         return 0x8237; /* GL_RG8I */
   case SpvImageFormatR16i:         return 0x8233; /* GL_R16I */
   case SpvImageFormatR8i:          return 0x8231; /* GL_R8I */
   case SpvImageFormatRgba32ui:     return 0x8D70; /* GL_RGBA32UI */
   case SpvImageFormatRgba16ui:     return 0x8D76; /* GL_RGBA16UI */
   case SpvImageFormatRgba8ui:      return 0x8D7C; /* GL_RGBA8UI */
   case SpvImageFormatR32ui:        return 0x8236; /* GL_R32UI */
   case SpvImageFormatRgb10a2ui:    return 0x906F; /* GL_RGB10_A2UI */
   case SpvImageFormatRg32ui:       return 0x823C; /* GL_RG32UI */
   case SpvImageFormatRg16ui:       return 0x823A; /* GL_RG16UI */
   case SpvImageFormatRg8ui:        return 0x8238; /* GL_RG8UI */
   case SpvImageFormatR16ui:        return 0x8234; /* GL_R16UI */
   case SpvImageFormatR8ui:         return 0x8232; /* GL_R8UI */
   default:
      unreachable("Invalid image format");
      return 0;
   }
}

static void
vtn_handle_type(struct vtn_builder *b, SpvOp opcode,
                const uint32_t *w, unsigned count)
{
   struct vtn_value *val = vtn_push_value(b, w[1], vtn_value_type_type);

   val->type = rzalloc(b, struct vtn_type);
   val->type->val = val;

   switch (opcode) {
   case SpvOpTypeVoid:
      val->type->base_type = vtn_base_type_void;
      val->type->type = glsl_void_type();
      break;
   case SpvOpTypeBool:
      val->type->base_type = vtn_base_type_scalar;
      val->type->type = glsl_bool_type();
      break;
   case SpvOpTypeInt: {
      int bit_size = w[2];
      const bool signedness = w[3];
      val->type->base_type = vtn_base_type_scalar;
      if (bit_size == 64)
         val->type->type = (signedness ? glsl_int64_t_type() : glsl_uint64_t_type());
      else
         val->type->type = (signedness ? glsl_int_type() : glsl_uint_type());
      break;
   }
   case SpvOpTypeFloat: {
      int bit_size = w[2];
      val->type->base_type = vtn_base_type_scalar;
      val->type->type = bit_size == 64 ? glsl_double_type() : glsl_float_type();
      break;
   }

   case SpvOpTypeVector: {
      struct vtn_type *base = vtn_value(b, w[2], vtn_value_type_type)->type;
      unsigned elems = w[3];

      assert(glsl_type_is_scalar(base->type));
      val->type->base_type = vtn_base_type_vector;
      val->type->type = glsl_vector_type(glsl_get_base_type(base->type), elems);
      val->type->stride = glsl_get_bit_size(base->type) / 8;
      val->type->array_element = base;
      break;
   }

   case SpvOpTypeMatrix: {
      struct vtn_type *base = vtn_value(b, w[2], vtn_value_type_type)->type;
      unsigned columns = w[3];

      assert(glsl_type_is_vector(base->type));
      val->type->base_type = vtn_base_type_matrix;
      val->type->type = glsl_matrix_type(glsl_get_base_type(base->type),
                                         glsl_get_vector_elements(base->type),
                                         columns);
      assert(!glsl_type_is_error(val->type->type));
      val->type->length = columns;
      val->type->array_element = base;
      val->type->row_major = false;
      val->type->stride = 0;
      break;
   }

   case SpvOpTypeRuntimeArray:
   case SpvOpTypeArray: {
      struct vtn_type *array_element =
         vtn_value(b, w[2], vtn_value_type_type)->type;

      if (opcode == SpvOpTypeRuntimeArray) {
         /* A length of 0 is used to denote unsized arrays */
         val->type->length = 0;
      } else {
         val->type->length =
            vtn_value(b, w[3], vtn_value_type_constant)->constant->values[0].u32[0];
      }

      val->type->base_type = vtn_base_type_array;
      val->type->type = glsl_array_type(array_element->type, val->type->length);
      val->type->array_element = array_element;
      val->type->stride = 0;
      break;
   }

   case SpvOpTypeStruct: {
      unsigned num_fields = count - 2;
      val->type->base_type = vtn_base_type_struct;
      val->type->length = num_fields;
      val->type->members = ralloc_array(b, struct vtn_type *, num_fields);
      val->type->offsets = ralloc_array(b, unsigned, num_fields);

      NIR_VLA(struct glsl_struct_field, fields, count);
      for (unsigned i = 0; i < num_fields; i++) {
         val->type->members[i] =
            vtn_value(b, w[i + 2], vtn_value_type_type)->type;
         fields[i] = (struct glsl_struct_field) {
            .type = val->type->members[i]->type,
            .name = ralloc_asprintf(b, "field%d", i),
            .location = -1,
         };
      }

      struct member_decoration_ctx ctx = {
         .num_fields = num_fields,
         .fields = fields,
         .type = val->type
      };

      vtn_foreach_decoration(b, val, struct_member_decoration_cb, &ctx);
      vtn_foreach_decoration(b, val, struct_member_matrix_stride_cb, &ctx);

      const char *name = val->name ? val->name : "struct";

      val->type->type = glsl_struct_type(fields, num_fields, name);
      break;
   }

   case SpvOpTypeFunction: {
      val->type->base_type = vtn_base_type_function;
      val->type->type = NULL;

      val->type->return_type = vtn_value(b, w[2], vtn_value_type_type)->type;

      const unsigned num_params = count - 3;
      val->type->length = num_params;
      val->type->params = ralloc_array(b, struct vtn_type *, num_params);
      for (unsigned i = 0; i < count - 3; i++) {
         val->type->params[i] =
            vtn_value(b, w[i + 3], vtn_value_type_type)->type;
      }
      break;
   }

   case SpvOpTypePointer: {
      SpvStorageClass storage_class = w[2];
      struct vtn_type *deref_type =
         vtn_value(b, w[3], vtn_value_type_type)->type;

      val->type->base_type = vtn_base_type_pointer;
      val->type->storage_class = storage_class;
      val->type->deref = deref_type;

      if (storage_class == SpvStorageClassUniform ||
          storage_class == SpvStorageClassStorageBuffer) {
         /* These can actually be stored to nir_variables and used as SSA
          * values so they need a real glsl_type.
          */
         val->type->type = glsl_vector_type(GLSL_TYPE_UINT, 2);
      }
      break;
   }

   case SpvOpTypeImage: {
      val->type->base_type = vtn_base_type_image;

      const struct glsl_type *sampled_type =
         vtn_value(b, w[2], vtn_value_type_type)->type->type;

      assert(glsl_type_is_vector_or_scalar(sampled_type));

      enum glsl_sampler_dim dim;
      switch ((SpvDim)w[3]) {
      case SpvDim1D:       dim = GLSL_SAMPLER_DIM_1D;    break;
      case SpvDim2D:       dim = GLSL_SAMPLER_DIM_2D;    break;
      case SpvDim3D:       dim = GLSL_SAMPLER_DIM_3D;    break;
      case SpvDimCube:     dim = GLSL_SAMPLER_DIM_CUBE;  break;
      case SpvDimRect:     dim = GLSL_SAMPLER_DIM_RECT;  break;
      case SpvDimBuffer:   dim = GLSL_SAMPLER_DIM_BUF;   break;
      case SpvDimSubpassData: dim = GLSL_SAMPLER_DIM_SUBPASS; break;
      default:
         unreachable("Invalid SPIR-V Sampler dimension");
      }

      bool is_shadow = w[4];
      bool is_array = w[5];
      bool multisampled = w[6];
      unsigned sampled = w[7];
      SpvImageFormat format = w[8];

      if (count > 9)
         val->type->access_qualifier = w[9];
      else
         val->type->access_qualifier = SpvAccessQualifierReadWrite;

      if (multisampled) {
         if (dim == GLSL_SAMPLER_DIM_2D)
            dim = GLSL_SAMPLER_DIM_MS;
         else if (dim == GLSL_SAMPLER_DIM_SUBPASS)
            dim = GLSL_SAMPLER_DIM_SUBPASS_MS;
         else
            unreachable("Unsupported multisampled image type");
      }

      val->type->image_format = translate_image_format(format);

      if (sampled == 1) {
         val->type->sampled = true;
         val->type->type = glsl_sampler_type(dim, is_shadow, is_array,
                                             glsl_get_base_type(sampled_type));
      } else if (sampled == 2) {
         assert(!is_shadow);
         val->type->sampled = false;
         val->type->type = glsl_image_type(dim, is_array,
                                           glsl_get_base_type(sampled_type));
      } else {
         unreachable("We need to know if the image will be sampled");
      }
      break;
   }

   case SpvOpTypeSampledImage:
      val->type = vtn_value(b, w[2], vtn_value_type_type)->type;
      break;

   case SpvOpTypeSampler:
      /* The actual sampler type here doesn't really matter.  It gets
       * thrown away the moment you combine it with an image.  What really
       * matters is that it's a sampler type as opposed to an integer type
       * so the backend knows what to do.
       */
      val->type->base_type = vtn_base_type_sampler;
      val->type->type = glsl_bare_sampler_type();
      break;

   case SpvOpTypeOpaque:
   case SpvOpTypeEvent:
   case SpvOpTypeDeviceEvent:
   case SpvOpTypeReserveId:
   case SpvOpTypeQueue:
   case SpvOpTypePipe:
   default:
      unreachable("Unhandled opcode");
   }

   vtn_foreach_decoration(b, val, type_decoration_cb, NULL);
}

static nir_constant *
vtn_null_constant(struct vtn_builder *b, const struct glsl_type *type)
{
   nir_constant *c = rzalloc(b, nir_constant);

   /* For pointers and other typeless things, we have to return something but
    * it doesn't matter what.
    */
   if (!type)
      return c;

   switch (glsl_get_base_type(type)) {
   case GLSL_TYPE_INT:
   case GLSL_TYPE_UINT:
   case GLSL_TYPE_INT64:
   case GLSL_TYPE_UINT64:
   case GLSL_TYPE_BOOL:
   case GLSL_TYPE_FLOAT:
   case GLSL_TYPE_DOUBLE:
      /* Nothing to do here.  It's already initialized to zero */
      break;

   case GLSL_TYPE_ARRAY:
      assert(glsl_get_length(type) > 0);
      c->num_elements = glsl_get_length(type);
      c->elements = ralloc_array(b, nir_constant *, c->num_elements);

      c->elements[0] = vtn_null_constant(b, glsl_get_array_element(type));
      for (unsigned i = 1; i < c->num_elements; i++)
         c->elements[i] = c->elements[0];
      break;

   case GLSL_TYPE_STRUCT:
      c->num_elements = glsl_get_length(type);
      c->elements = ralloc_array(b, nir_constant *, c->num_elements);

      for (unsigned i = 0; i < c->num_elements; i++) {
         c->elements[i] = vtn_null_constant(b, glsl_get_struct_field(type, i));
      }
      break;

   default:
      unreachable("Invalid type for null constant");
   }

   return c;
}

static void
spec_constant_decoration_cb(struct vtn_builder *b, struct vtn_value *v,
                             int member, const struct vtn_decoration *dec,
                             void *data)
{
   assert(member == -1);
   if (dec->decoration != SpvDecorationSpecId)
      return;

   struct spec_constant_value *const_value = data;

   for (unsigned i = 0; i < b->num_specializations; i++) {
      if (b->specializations[i].id == dec->literals[0]) {
         if (const_value->is_double)
            const_value->data64 = b->specializations[i].data64;
         else
            const_value->data32 = b->specializations[i].data32;
         return;
      }
   }
}

static uint32_t
get_specialization(struct vtn_builder *b, struct vtn_value *val,
                   uint32_t const_value)
{
   struct spec_constant_value data;
   data.is_double = false;
   data.data32 = const_value;
   vtn_foreach_decoration(b, val, spec_constant_decoration_cb, &data);
   return data.data32;
}

static uint64_t
get_specialization64(struct vtn_builder *b, struct vtn_value *val,
                   uint64_t const_value)
{
   struct spec_constant_value data;
   data.is_double = true;
   data.data64 = const_value;
   vtn_foreach_decoration(b, val, spec_constant_decoration_cb, &data);
   return data.data64;
}

static void
handle_workgroup_size_decoration_cb(struct vtn_builder *b,
                                    struct vtn_value *val,
                                    int member,
                                    const struct vtn_decoration *dec,
                                    void *data)
{
   assert(member == -1);
   if (dec->decoration != SpvDecorationBuiltIn ||
       dec->literals[0] != SpvBuiltInWorkgroupSize)
      return;

   assert(val->const_type == glsl_vector_type(GLSL_TYPE_UINT, 3));

   b->shader->info.cs.local_size[0] = val->constant->values[0].u32[0];
   b->shader->info.cs.local_size[1] = val->constant->values[0].u32[1];
   b->shader->info.cs.local_size[2] = val->constant->values[0].u32[2];
}

static void
vtn_handle_constant(struct vtn_builder *b, SpvOp opcode,
                    const uint32_t *w, unsigned count)
{
   struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_constant);
   val->const_type = vtn_value(b, w[1], vtn_value_type_type)->type->type;
   val->constant = rzalloc(b, nir_constant);
   switch (opcode) {
   case SpvOpConstantTrue:
      assert(val->const_type == glsl_bool_type());
      val->constant->values[0].u32[0] = NIR_TRUE;
      break;
   case SpvOpConstantFalse:
      assert(val->const_type == glsl_bool_type());
      val->constant->values[0].u32[0] = NIR_FALSE;
      break;

   case SpvOpSpecConstantTrue:
   case SpvOpSpecConstantFalse: {
      assert(val->const_type == glsl_bool_type());
      uint32_t int_val =
         get_specialization(b, val, (opcode == SpvOpSpecConstantTrue));
      val->constant->values[0].u32[0] = int_val ? NIR_TRUE : NIR_FALSE;
      break;
   }

   case SpvOpConstant: {
      assert(glsl_type_is_scalar(val->const_type));
      int bit_size = glsl_get_bit_size(val->const_type);
      if (bit_size == 64) {
         val->constant->values->u32[0] = w[3];
         val->constant->values->u32[1] = w[4];
      } else {
         assert(bit_size == 32);
         val->constant->values->u32[0] = w[3];
      }
      break;
   }
   case SpvOpSpecConstant: {
      assert(glsl_type_is_scalar(val->const_type));
      val->constant->values[0].u32[0] = get_specialization(b, val, w[3]);
      int bit_size = glsl_get_bit_size(val->const_type);
      if (bit_size == 64)
         val->constant->values[0].u64[0] =
            get_specialization64(b, val, vtn_u64_literal(&w[3]));
      else
         val->constant->values[0].u32[0] = get_specialization(b, val, w[3]);
      break;
   }
   case SpvOpSpecConstantComposite:
   case SpvOpConstantComposite: {
      unsigned elem_count = count - 3;
      nir_constant **elems = ralloc_array(b, nir_constant *, elem_count);
      for (unsigned i = 0; i < elem_count; i++)
         elems[i] = vtn_value(b, w[i + 3], vtn_value_type_constant)->constant;

      switch (glsl_get_base_type(val->const_type)) {
      case GLSL_TYPE_UINT:
      case GLSL_TYPE_INT:
      case GLSL_TYPE_UINT64:
      case GLSL_TYPE_INT64:
      case GLSL_TYPE_FLOAT:
      case GLSL_TYPE_BOOL:
      case GLSL_TYPE_DOUBLE: {
         int bit_size = glsl_get_bit_size(val->const_type);
         if (glsl_type_is_matrix(val->const_type)) {
            assert(glsl_get_matrix_columns(val->const_type) == elem_count);
            for (unsigned i = 0; i < elem_count; i++)
               val->constant->values[i] = elems[i]->values[0];
         } else {
            assert(glsl_type_is_vector(val->const_type));
            assert(glsl_get_vector_elements(val->const_type) == elem_count);
            for (unsigned i = 0; i < elem_count; i++) {
               if (bit_size == 64) {
                  val->constant->values[0].u64[i] = elems[i]->values[0].u64[0];
               } else {
                  assert(bit_size == 32);
                  val->constant->values[0].u32[i] = elems[i]->values[0].u32[0];
               }
            }
         }
         ralloc_free(elems);
         break;
      }
      case GLSL_TYPE_STRUCT:
      case GLSL_TYPE_ARRAY:
         ralloc_steal(val->constant, elems);
         val->constant->num_elements = elem_count;
         val->constant->elements = elems;
         break;

      default:
         unreachable("Unsupported type for constants");
      }
      break;
   }

   case SpvOpSpecConstantOp: {
      SpvOp opcode = get_specialization(b, val, w[3]);
      switch (opcode) {
      case SpvOpVectorShuffle: {
         struct vtn_value *v0 = &b->values[w[4]];
         struct vtn_value *v1 = &b->values[w[5]];

         assert(v0->value_type == vtn_value_type_constant ||
                v0->value_type == vtn_value_type_undef);
         assert(v1->value_type == vtn_value_type_constant ||
                v1->value_type == vtn_value_type_undef);

         unsigned len0 = v0->value_type == vtn_value_type_constant ?
                         glsl_get_vector_elements(v0->const_type) :
                         glsl_get_vector_elements(v0->type->type);
         unsigned len1 = v1->value_type == vtn_value_type_constant ?
                         glsl_get_vector_elements(v1->const_type) :
                         glsl_get_vector_elements(v1->type->type);

         assert(len0 + len1 < 16);

         unsigned bit_size = glsl_get_bit_size(val->const_type);
         unsigned bit_size0 = v0->value_type == vtn_value_type_constant ?
                              glsl_get_bit_size(v0->const_type) :
                              glsl_get_bit_size(v0->type->type);
         unsigned bit_size1 = v1->value_type == vtn_value_type_constant ?
                              glsl_get_bit_size(v1->const_type) :
                              glsl_get_bit_size(v1->type->type);

         assert(bit_size == bit_size0 && bit_size == bit_size1);
         (void)bit_size0; (void)bit_size1;

         if (bit_size == 64) {
            uint64_t u64[8];
            if (v0->value_type == vtn_value_type_constant) {
               for (unsigned i = 0; i < len0; i++)
                  u64[i] = v0->constant->values[0].u64[i];
            }
            if (v1->value_type == vtn_value_type_constant) {
               for (unsigned i = 0; i < len1; i++)
                  u64[len0 + i] = v1->constant->values[0].u64[i];
            }

            for (unsigned i = 0, j = 0; i < count - 6; i++, j++) {
               uint32_t comp = w[i + 6];
               /* If component is not used, set the value to a known constant
                * to detect if it is wrongly used.
                */
               if (comp == (uint32_t)-1)
                  val->constant->values[0].u64[j] = 0xdeadbeefdeadbeef;
               else
                  val->constant->values[0].u64[j] = u64[comp];
            }
         } else {
            uint32_t u32[8];
            if (v0->value_type == vtn_value_type_constant) {
               for (unsigned i = 0; i < len0; i++)
                  u32[i] = v0->constant->values[0].u32[i];
            }
            if (v1->value_type == vtn_value_type_constant) {
               for (unsigned i = 0; i < len1; i++)
                  u32[len0 + i] = v1->constant->values[0].u32[i];
            }

            for (unsigned i = 0, j = 0; i < count - 6; i++, j++) {
               uint32_t comp = w[i + 6];
               /* If component is not used, set the value to a known constant
                * to detect if it is wrongly used.
                */
               if (comp == (uint32_t)-1)
                  val->constant->values[0].u32[j] = 0xdeadbeef;
               else
                  val->constant->values[0].u32[j] = u32[comp];
            }
         }
         break;
      }

      case SpvOpCompositeExtract:
      case SpvOpCompositeInsert: {
         struct vtn_value *comp;
         unsigned deref_start;
         struct nir_constant **c;
         if (opcode == SpvOpCompositeExtract) {
            comp = vtn_value(b, w[4], vtn_value_type_constant);
            deref_start = 5;
            c = &comp->constant;
         } else {
            comp = vtn_value(b, w[5], vtn_value_type_constant);
            deref_start = 6;
            val->constant = nir_constant_clone(comp->constant,
                                               (nir_variable *)b);
            c = &val->constant;
         }

         int elem = -1;
         int col = 0;
         const struct glsl_type *type = comp->const_type;
         for (unsigned i = deref_start; i < count; i++) {
            switch (glsl_get_base_type(type)) {
            case GLSL_TYPE_UINT:
            case GLSL_TYPE_INT:
            case GLSL_TYPE_UINT64:
            case GLSL_TYPE_INT64:
            case GLSL_TYPE_FLOAT:
            case GLSL_TYPE_DOUBLE:
            case GLSL_TYPE_BOOL:
               /* If we hit this granularity, we're picking off an element */
               if (glsl_type_is_matrix(type)) {
                  assert(col == 0 && elem == -1);
                  col = w[i];
                  elem = 0;
                  type = glsl_get_column_type(type);
               } else {
                  assert(elem <= 0 && glsl_type_is_vector(type));
                  elem = w[i];
                  type = glsl_scalar_type(glsl_get_base_type(type));
               }
               continue;

            case GLSL_TYPE_ARRAY:
               c = &(*c)->elements[w[i]];
               type = glsl_get_array_element(type);
               continue;

            case GLSL_TYPE_STRUCT:
               c = &(*c)->elements[w[i]];
               type = glsl_get_struct_field(type, w[i]);
               continue;

            default:
               unreachable("Invalid constant type");
            }
         }

         if (opcode == SpvOpCompositeExtract) {
            if (elem == -1) {
               val->constant = *c;
            } else {
               unsigned num_components = glsl_get_vector_elements(type);
               unsigned bit_size = glsl_get_bit_size(type);
               for (unsigned i = 0; i < num_components; i++)
                  if (bit_size == 64) {
                     val->constant->values[0].u64[i] = (*c)->values[col].u64[elem + i];
                  } else {
                     assert(bit_size == 32);
                     val->constant->values[0].u32[i] = (*c)->values[col].u32[elem + i];
                  }
            }
         } else {
            struct vtn_value *insert =
               vtn_value(b, w[4], vtn_value_type_constant);
            assert(insert->const_type == type);
            if (elem == -1) {
               *c = insert->constant;
            } else {
               unsigned num_components = glsl_get_vector_elements(type);
               unsigned bit_size = glsl_get_bit_size(type);
               for (unsigned i = 0; i < num_components; i++)
                  if (bit_size == 64) {
                     (*c)->values[col].u64[elem + i] = insert->constant->values[0].u64[i];
                  } else {
                     assert(bit_size == 32);
                     (*c)->values[col].u32[elem + i] = insert->constant->values[0].u32[i];
                  }
            }
         }
         break;
      }

      default: {
         bool swap;
         nir_alu_type dst_alu_type = nir_get_nir_type_for_glsl_type(val->const_type);
         nir_alu_type src_alu_type = dst_alu_type;
         nir_op op = vtn_nir_alu_op_for_spirv_opcode(opcode, &swap, src_alu_type, dst_alu_type);

         unsigned num_components = glsl_get_vector_elements(val->const_type);
         unsigned bit_size =
            glsl_get_bit_size(val->const_type);

         nir_const_value src[4];
         assert(count <= 7);
         for (unsigned i = 0; i < count - 4; i++) {
            nir_constant *c =
               vtn_value(b, w[4 + i], vtn_value_type_constant)->constant;

            unsigned j = swap ? 1 - i : i;
            assert(bit_size == 32);
            src[j] = c->values[0];
         }

         val->constant->values[0] =
            nir_eval_const_opcode(op, num_components, bit_size, src);
         break;
      } /* default */
      }
      break;
   }

   case SpvOpConstantNull:
      val->constant = vtn_null_constant(b, val->const_type);
      break;

   case SpvOpConstantSampler:
      unreachable("OpConstantSampler requires Kernel Capability");
      break;

   default:
      unreachable("Unhandled opcode");
   }

   /* Now that we have the value, update the workgroup size if needed */
   vtn_foreach_decoration(b, val, handle_workgroup_size_decoration_cb, NULL);
}

static void
vtn_handle_function_call(struct vtn_builder *b, SpvOp opcode,
                         const uint32_t *w, unsigned count)
{
   struct vtn_type *res_type = vtn_value(b, w[1], vtn_value_type_type)->type;
   struct nir_function *callee =
      vtn_value(b, w[3], vtn_value_type_function)->func->impl->function;

   nir_call_instr *call = nir_call_instr_create(b->nb.shader, callee);
   for (unsigned i = 0; i < call->num_params; i++) {
      unsigned arg_id = w[4 + i];
      struct vtn_value *arg = vtn_untyped_value(b, arg_id);
      if (arg->value_type == vtn_value_type_pointer &&
          arg->pointer->ptr_type->type == NULL) {
         nir_deref_var *d = vtn_pointer_to_deref(b, arg->pointer);
         call->params[i] = nir_deref_var_clone(d, call);
      } else {
         struct vtn_ssa_value *arg_ssa = vtn_ssa_value(b, arg_id);

         /* Make a temporary to store the argument in */
         nir_variable *tmp =
            nir_local_variable_create(b->impl, arg_ssa->type, "arg_tmp");
         call->params[i] = nir_deref_var_create(call, tmp);

         vtn_local_store(b, arg_ssa, call->params[i]);
      }
   }

   nir_variable *out_tmp = NULL;
   assert(res_type->type == callee->return_type);
   if (!glsl_type_is_void(callee->return_type)) {
      out_tmp = nir_local_variable_create(b->impl, callee->return_type,
                                          "out_tmp");
      call->return_deref = nir_deref_var_create(call, out_tmp);
   }

   nir_builder_instr_insert(&b->nb, &call->instr);

   if (glsl_type_is_void(callee->return_type)) {
      vtn_push_value(b, w[2], vtn_value_type_undef);
   } else {
      vtn_push_ssa(b, w[2], res_type, vtn_local_load(b, call->return_deref));
   }
}

struct vtn_ssa_value *
vtn_create_ssa_value(struct vtn_builder *b, const struct glsl_type *type)
{
   struct vtn_ssa_value *val = rzalloc(b, struct vtn_ssa_value);
   val->type = type;

   if (!glsl_type_is_vector_or_scalar(type)) {
      unsigned elems = glsl_get_length(type);
      val->elems = ralloc_array(b, struct vtn_ssa_value *, elems);
      for (unsigned i = 0; i < elems; i++) {
         const struct glsl_type *child_type;

         switch (glsl_get_base_type(type)) {
         case GLSL_TYPE_INT:
         case GLSL_TYPE_UINT:
         case GLSL_TYPE_INT64:
         case GLSL_TYPE_UINT64:
         case GLSL_TYPE_BOOL:
         case GLSL_TYPE_FLOAT:
         case GLSL_TYPE_DOUBLE:
            child_type = glsl_get_column_type(type);
            break;
         case GLSL_TYPE_ARRAY:
            child_type = glsl_get_array_element(type);
            break;
         case GLSL_TYPE_STRUCT:
            child_type = glsl_get_struct_field(type, i);
            break;
         default:
            unreachable("unkown base type");
         }

         val->elems[i] = vtn_create_ssa_value(b, child_type);
      }
   }

   return val;
}

static nir_tex_src
vtn_tex_src(struct vtn_builder *b, unsigned index, nir_tex_src_type type)
{
   nir_tex_src src;
   src.src = nir_src_for_ssa(vtn_ssa_value(b, index)->def);
   src.src_type = type;
   return src;
}

static void
vtn_handle_texture(struct vtn_builder *b, SpvOp opcode,
                   const uint32_t *w, unsigned count)
{
   if (opcode == SpvOpSampledImage) {
      struct vtn_value *val =
         vtn_push_value(b, w[2], vtn_value_type_sampled_image);
      val->sampled_image = ralloc(b, struct vtn_sampled_image);
      val->sampled_image->type =
         vtn_value(b, w[1], vtn_value_type_type)->type;
      val->sampled_image->image =
         vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      val->sampled_image->sampler =
         vtn_value(b, w[4], vtn_value_type_pointer)->pointer;
      return;
   } else if (opcode == SpvOpImage) {
      struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_pointer);
      struct vtn_value *src_val = vtn_untyped_value(b, w[3]);
      if (src_val->value_type == vtn_value_type_sampled_image) {
         val->pointer = src_val->sampled_image->image;
      } else {
         assert(src_val->value_type == vtn_value_type_pointer);
         val->pointer = src_val->pointer;
      }
      return;
   }

   struct vtn_type *ret_type = vtn_value(b, w[1], vtn_value_type_type)->type;
   struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_ssa);

   struct vtn_sampled_image sampled;
   struct vtn_value *sampled_val = vtn_untyped_value(b, w[3]);
   if (sampled_val->value_type == vtn_value_type_sampled_image) {
      sampled = *sampled_val->sampled_image;
   } else {
      assert(sampled_val->value_type == vtn_value_type_pointer);
      sampled.type = sampled_val->pointer->type;
      sampled.image = NULL;
      sampled.sampler = sampled_val->pointer;
   }

   const struct glsl_type *image_type = sampled.type->type;
   const enum glsl_sampler_dim sampler_dim = glsl_get_sampler_dim(image_type);
   const bool is_array = glsl_sampler_type_is_array(image_type);

   /* Figure out the base texture operation */
   nir_texop texop;
   switch (opcode) {
   case SpvOpImageSampleImplicitLod:
   case SpvOpImageSampleDrefImplicitLod:
   case SpvOpImageSampleProjImplicitLod:
   case SpvOpImageSampleProjDrefImplicitLod:
      texop = nir_texop_tex;
      break;

   case SpvOpImageSampleExplicitLod:
   case SpvOpImageSampleDrefExplicitLod:
   case SpvOpImageSampleProjExplicitLod:
   case SpvOpImageSampleProjDrefExplicitLod:
      texop = nir_texop_txl;
      break;

   case SpvOpImageFetch:
      if (glsl_get_sampler_dim(image_type) == GLSL_SAMPLER_DIM_MS) {
         texop = nir_texop_txf_ms;
      } else {
         texop = nir_texop_txf;
      }
      break;

   case SpvOpImageGather:
   case SpvOpImageDrefGather:
      texop = nir_texop_tg4;
      break;

   case SpvOpImageQuerySizeLod:
   case SpvOpImageQuerySize:
      texop = nir_texop_txs;
      break;

   case SpvOpImageQueryLod:
      texop = nir_texop_lod;
      break;

   case SpvOpImageQueryLevels:
      texop = nir_texop_query_levels;
      break;

   case SpvOpImageQuerySamples:
      texop = nir_texop_texture_samples;
      break;

   default:
      unreachable("Unhandled opcode");
   }

   nir_tex_src srcs[8]; /* 8 should be enough */
   nir_tex_src *p = srcs;

   unsigned idx = 4;

   struct nir_ssa_def *coord;
   unsigned coord_components;
   switch (opcode) {
   case SpvOpImageSampleImplicitLod:
   case SpvOpImageSampleExplicitLod:
   case SpvOpImageSampleDrefImplicitLod:
   case SpvOpImageSampleDrefExplicitLod:
   case SpvOpImageSampleProjImplicitLod:
   case SpvOpImageSampleProjExplicitLod:
   case SpvOpImageSampleProjDrefImplicitLod:
   case SpvOpImageSampleProjDrefExplicitLod:
   case SpvOpImageFetch:
   case SpvOpImageGather:
   case SpvOpImageDrefGather:
   case SpvOpImageQueryLod: {
      /* All these types have the coordinate as their first real argument */
      switch (sampler_dim) {
      case GLSL_SAMPLER_DIM_1D:
      case GLSL_SAMPLER_DIM_BUF:
         coord_components = 1;
         break;
      case GLSL_SAMPLER_DIM_2D:
      case GLSL_SAMPLER_DIM_RECT:
      case GLSL_SAMPLER_DIM_MS:
         coord_components = 2;
         break;
      case GLSL_SAMPLER_DIM_3D:
      case GLSL_SAMPLER_DIM_CUBE:
         coord_components = 3;
         break;
      default:
         unreachable("Invalid sampler type");
      }

      if (is_array && texop != nir_texop_lod)
         coord_components++;

      coord = vtn_ssa_value(b, w[idx++])->def;
      p->src = nir_src_for_ssa(nir_channels(&b->nb, coord,
                                            (1 << coord_components) - 1));
      p->src_type = nir_tex_src_coord;
      p++;
      break;
   }

   default:
      coord = NULL;
      coord_components = 0;
      break;
   }

   switch (opcode) {
   case SpvOpImageSampleProjImplicitLod:
   case SpvOpImageSampleProjExplicitLod:
   case SpvOpImageSampleProjDrefImplicitLod:
   case SpvOpImageSampleProjDrefExplicitLod:
      /* These have the projector as the last coordinate component */
      p->src = nir_src_for_ssa(nir_channel(&b->nb, coord, coord_components));
      p->src_type = nir_tex_src_projector;
      p++;
      break;

   default:
      break;
   }

   bool is_shadow = false;
   unsigned gather_component = 0;
   switch (opcode) {
   case SpvOpImageSampleDrefImplicitLod:
   case SpvOpImageSampleDrefExplicitLod:
   case SpvOpImageSampleProjDrefImplicitLod:
   case SpvOpImageSampleProjDrefExplicitLod:
   case SpvOpImageDrefGather:
      /* These all have an explicit depth value as their next source */
      is_shadow = true;
      (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_comparator);
      break;

   case SpvOpImageGather:
      /* This has a component as its next source */
      gather_component =
         vtn_value(b, w[idx++], vtn_value_type_constant)->constant->values[0].u32[0];
      break;

   default:
      break;
   }

   /* For OpImageQuerySizeLod, we always have an LOD */
   if (opcode == SpvOpImageQuerySizeLod)
      (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_lod);

   /* Now we need to handle some number of optional arguments */
   const struct vtn_ssa_value *gather_offsets = NULL;
   if (idx < count) {
      uint32_t operands = w[idx++];

      if (operands & SpvImageOperandsBiasMask) {
         assert(texop == nir_texop_tex);
         texop = nir_texop_txb;
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_bias);
      }

      if (operands & SpvImageOperandsLodMask) {
         assert(texop == nir_texop_txl || texop == nir_texop_txf ||
                texop == nir_texop_txs);
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_lod);
      }

      if (operands & SpvImageOperandsGradMask) {
         assert(texop == nir_texop_txl);
         texop = nir_texop_txd;
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_ddx);
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_ddy);
      }

      if (operands & SpvImageOperandsOffsetMask ||
          operands & SpvImageOperandsConstOffsetMask)
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_offset);

      if (operands & SpvImageOperandsConstOffsetsMask) {
         gather_offsets = vtn_ssa_value(b, w[idx++]);
         (*p++) = (nir_tex_src){};
      }

      if (operands & SpvImageOperandsSampleMask) {
         assert(texop == nir_texop_txf_ms);
         texop = nir_texop_txf_ms;
         (*p++) = vtn_tex_src(b, w[idx++], nir_tex_src_ms_index);
      }
   }
   /* We should have now consumed exactly all of the arguments */
   assert(idx == count);

   nir_tex_instr *instr = nir_tex_instr_create(b->shader, p - srcs);
   instr->op = texop;

   memcpy(instr->src, srcs, instr->num_srcs * sizeof(*instr->src));

   instr->coord_components = coord_components;
   instr->sampler_dim = sampler_dim;
   instr->is_array = is_array;
   instr->is_shadow = is_shadow;
   instr->is_new_style_shadow =
      is_shadow && glsl_get_components(ret_type->type) == 1;
   instr->component = gather_component;

   switch (glsl_get_sampler_result_type(image_type)) {
   case GLSL_TYPE_FLOAT:   instr->dest_type = nir_type_float;     break;
   case GLSL_TYPE_INT:     instr->dest_type = nir_type_int;       break;
   case GLSL_TYPE_UINT:    instr->dest_type = nir_type_uint;  break;
   case GLSL_TYPE_BOOL:    instr->dest_type = nir_type_bool;      break;
   default:
      unreachable("Invalid base type for sampler result");
   }

   nir_deref_var *sampler = vtn_pointer_to_deref(b, sampled.sampler);
   nir_deref_var *texture;
   if (sampled.image) {
      nir_deref_var *image = vtn_pointer_to_deref(b, sampled.image);
      texture = image;
   } else {
      texture = sampler;
   }

   instr->texture = nir_deref_var_clone(texture, instr);

   switch (instr->op) {
   case nir_texop_tex:
   case nir_texop_txb:
   case nir_texop_txl:
   case nir_texop_txd:
   case nir_texop_tg4:
      /* These operations require a sampler */
      instr->sampler = nir_deref_var_clone(sampler, instr);
      break;
   case nir_texop_txf:
   case nir_texop_txf_ms:
   case nir_texop_txs:
   case nir_texop_lod:
   case nir_texop_query_levels:
   case nir_texop_texture_samples:
   case nir_texop_samples_identical:
      /* These don't */
      instr->sampler = NULL;
      break;
   case nir_texop_txf_ms_mcs:
      unreachable("unexpected nir_texop_txf_ms_mcs");
   }

   nir_ssa_dest_init(&instr->instr, &instr->dest,
                     nir_tex_instr_dest_size(instr), 32, NULL);

   assert(glsl_get_vector_elements(ret_type->type) ==
          nir_tex_instr_dest_size(instr));

   nir_ssa_def *def;
   nir_instr *instruction;
   if (gather_offsets) {
      assert(glsl_get_base_type(gather_offsets->type) == GLSL_TYPE_ARRAY);
      assert(glsl_get_length(gather_offsets->type) == 4);
      nir_tex_instr *instrs[4] = {instr, NULL, NULL, NULL};

      /* Copy the current instruction 4x */
      for (uint32_t i = 1; i < 4; i++) {
         instrs[i] = nir_tex_instr_create(b->shader, instr->num_srcs);
         instrs[i]->op = instr->op;
         instrs[i]->coord_components = instr->coord_components;
         instrs[i]->sampler_dim = instr->sampler_dim;
         instrs[i]->is_array = instr->is_array;
         instrs[i]->is_shadow = instr->is_shadow;
         instrs[i]->is_new_style_shadow = instr->is_new_style_shadow;
         instrs[i]->component = instr->component;
         instrs[i]->dest_type = instr->dest_type;
         instrs[i]->texture = nir_deref_var_clone(texture, instrs[i]);
         instrs[i]->sampler = NULL;

         memcpy(instrs[i]->src, srcs, instr->num_srcs * sizeof(*instr->src));

         nir_ssa_dest_init(&instrs[i]->instr, &instrs[i]->dest,
                           nir_tex_instr_dest_size(instr), 32, NULL);
      }

      /* Fill in the last argument with the offset from the passed in offsets
       * and insert the instruction into the stream.
       */
      for (uint32_t i = 0; i < 4; i++) {
         nir_tex_src src;
         src.src = nir_src_for_ssa(gather_offsets->elems[i]->def);
         src.src_type = nir_tex_src_offset;
         instrs[i]->src[instrs[i]->num_srcs - 1] = src;
         nir_builder_instr_insert(&b->nb, &instrs[i]->instr);
      }

      /* Combine the results of the 4 instructions by taking their .w
       * components
       */
      nir_alu_instr *vec4 = nir_alu_instr_create(b->shader, nir_op_vec4);
      nir_ssa_dest_init(&vec4->instr, &vec4->dest.dest, 4, 32, NULL);
      vec4->dest.write_mask = 0xf;
      for (uint32_t i = 0; i < 4; i++) {
         vec4->src[i].src = nir_src_for_ssa(&instrs[i]->dest.ssa);
         vec4->src[i].swizzle[0] = 3;
      }
      def = &vec4->dest.dest.ssa;
      instruction = &vec4->instr;
   } else {
      def = &instr->dest.ssa;
      instruction = &instr->instr;
   }

   val->ssa = vtn_create_ssa_value(b, ret_type->type);
   val->ssa->def = def;

   nir_builder_instr_insert(&b->nb, instruction);
}

static void
fill_common_atomic_sources(struct vtn_builder *b, SpvOp opcode,
                           const uint32_t *w, nir_src *src)
{
   switch (opcode) {
   case SpvOpAtomicIIncrement:
      src[0] = nir_src_for_ssa(nir_imm_int(&b->nb, 1));
      break;

   case SpvOpAtomicIDecrement:
      src[0] = nir_src_for_ssa(nir_imm_int(&b->nb, -1));
      break;

   case SpvOpAtomicISub:
      src[0] =
         nir_src_for_ssa(nir_ineg(&b->nb, vtn_ssa_value(b, w[6])->def));
      break;

   case SpvOpAtomicCompareExchange:
      src[0] = nir_src_for_ssa(vtn_ssa_value(b, w[8])->def);
      src[1] = nir_src_for_ssa(vtn_ssa_value(b, w[7])->def);
      break;

   case SpvOpAtomicExchange:
   case SpvOpAtomicIAdd:
   case SpvOpAtomicSMin:
   case SpvOpAtomicUMin:
   case SpvOpAtomicSMax:
   case SpvOpAtomicUMax:
   case SpvOpAtomicAnd:
   case SpvOpAtomicOr:
   case SpvOpAtomicXor:
      src[0] = nir_src_for_ssa(vtn_ssa_value(b, w[6])->def);
      break;

   default:
      unreachable("Invalid SPIR-V atomic");
   }
}

static nir_ssa_def *
get_image_coord(struct vtn_builder *b, uint32_t value)
{
   struct vtn_ssa_value *coord = vtn_ssa_value(b, value);

   /* The image_load_store intrinsics assume a 4-dim coordinate */
   unsigned dim = glsl_get_vector_elements(coord->type);
   unsigned swizzle[4];
   for (unsigned i = 0; i < 4; i++)
      swizzle[i] = MIN2(i, dim - 1);

   return nir_swizzle(&b->nb, coord->def, swizzle, 4, false);
}

static void
vtn_handle_image(struct vtn_builder *b, SpvOp opcode,
                 const uint32_t *w, unsigned count)
{
   /* Just get this one out of the way */
   if (opcode == SpvOpImageTexelPointer) {
      struct vtn_value *val =
         vtn_push_value(b, w[2], vtn_value_type_image_pointer);
      val->image = ralloc(b, struct vtn_image_pointer);

      val->image->image = vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      val->image->coord = get_image_coord(b, w[4]);
      val->image->sample = vtn_ssa_value(b, w[5])->def;
      return;
   }

   struct vtn_image_pointer image;

   switch (opcode) {
   case SpvOpAtomicExchange:
   case SpvOpAtomicCompareExchange:
   case SpvOpAtomicCompareExchangeWeak:
   case SpvOpAtomicIIncrement:
   case SpvOpAtomicIDecrement:
   case SpvOpAtomicIAdd:
   case SpvOpAtomicISub:
   case SpvOpAtomicLoad:
   case SpvOpAtomicSMin:
   case SpvOpAtomicUMin:
   case SpvOpAtomicSMax:
   case SpvOpAtomicUMax:
   case SpvOpAtomicAnd:
   case SpvOpAtomicOr:
   case SpvOpAtomicXor:
      image = *vtn_value(b, w[3], vtn_value_type_image_pointer)->image;
      break;

   case SpvOpAtomicStore:
      image = *vtn_value(b, w[1], vtn_value_type_image_pointer)->image;
      break;

   case SpvOpImageQuerySize:
      image.image = vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      image.coord = NULL;
      image.sample = NULL;
      break;

   case SpvOpImageRead:
      image.image = vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      image.coord = get_image_coord(b, w[4]);

      if (count > 5 && (w[5] & SpvImageOperandsSampleMask)) {
         assert(w[5] == SpvImageOperandsSampleMask);
         image.sample = vtn_ssa_value(b, w[6])->def;
      } else {
         image.sample = nir_ssa_undef(&b->nb, 1, 32);
      }
      break;

   case SpvOpImageWrite:
      image.image = vtn_value(b, w[1], vtn_value_type_pointer)->pointer;
      image.coord = get_image_coord(b, w[2]);

      /* texel = w[3] */

      if (count > 4 && (w[4] & SpvImageOperandsSampleMask)) {
         assert(w[4] == SpvImageOperandsSampleMask);
         image.sample = vtn_ssa_value(b, w[5])->def;
      } else {
         image.sample = nir_ssa_undef(&b->nb, 1, 32);
      }
      break;

   default:
      unreachable("Invalid image opcode");
   }

   nir_intrinsic_op op;
   switch (opcode) {
#define OP(S, N) case SpvOp##S: op = nir_intrinsic_image_##N; break;
   OP(ImageQuerySize,         size)
   OP(ImageRead,              load)
   OP(ImageWrite,             store)
   OP(AtomicLoad,             load)
   OP(AtomicStore,            store)
   OP(AtomicExchange,         atomic_exchange)
   OP(AtomicCompareExchange,  atomic_comp_swap)
   OP(AtomicIIncrement,       atomic_add)
   OP(AtomicIDecrement,       atomic_add)
   OP(AtomicIAdd,             atomic_add)
   OP(AtomicISub,             atomic_add)
   OP(AtomicSMin,             atomic_min)
   OP(AtomicUMin,             atomic_min)
   OP(AtomicSMax,             atomic_max)
   OP(AtomicUMax,             atomic_max)
   OP(AtomicAnd,              atomic_and)
   OP(AtomicOr,               atomic_or)
   OP(AtomicXor,              atomic_xor)
#undef OP
   default:
      unreachable("Invalid image opcode");
   }

   nir_intrinsic_instr *intrin = nir_intrinsic_instr_create(b->shader, op);

   nir_deref_var *image_deref = vtn_pointer_to_deref(b, image.image);
   intrin->variables[0] = nir_deref_var_clone(image_deref, intrin);

   /* ImageQuerySize doesn't take any extra parameters */
   if (opcode != SpvOpImageQuerySize) {
      /* The image coordinate is always 4 components but we may not have that
       * many.  Swizzle to compensate.
       */
      unsigned swiz[4];
      for (unsigned i = 0; i < 4; i++)
         swiz[i] = i < image.coord->num_components ? i : 0;
      intrin->src[0] = nir_src_for_ssa(nir_swizzle(&b->nb, image.coord,
                                                   swiz, 4, false));
      intrin->src[1] = nir_src_for_ssa(image.sample);
   }

   switch (opcode) {
   case SpvOpAtomicLoad:
   case SpvOpImageQuerySize:
   case SpvOpImageRead:
      break;
   case SpvOpAtomicStore:
      intrin->src[2] = nir_src_for_ssa(vtn_ssa_value(b, w[4])->def);
      break;
   case SpvOpImageWrite:
      intrin->src[2] = nir_src_for_ssa(vtn_ssa_value(b, w[3])->def);
      break;

   case SpvOpAtomicCompareExchange:
   case SpvOpAtomicIIncrement:
   case SpvOpAtomicIDecrement:
   case SpvOpAtomicExchange:
   case SpvOpAtomicIAdd:
   case SpvOpAtomicISub:
   case SpvOpAtomicSMin:
   case SpvOpAtomicUMin:
   case SpvOpAtomicSMax:
   case SpvOpAtomicUMax:
   case SpvOpAtomicAnd:
   case SpvOpAtomicOr:
   case SpvOpAtomicXor:
      fill_common_atomic_sources(b, opcode, w, &intrin->src[2]);
      break;

   default:
      unreachable("Invalid image opcode");
   }

   if (opcode != SpvOpImageWrite) {
      struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_ssa);
      struct vtn_type *type = vtn_value(b, w[1], vtn_value_type_type)->type;

      unsigned dest_components =
         nir_intrinsic_infos[intrin->intrinsic].dest_components;
      if (intrin->intrinsic == nir_intrinsic_image_size) {
         dest_components = intrin->num_components =
            glsl_get_vector_elements(type->type);
      }

      nir_ssa_dest_init(&intrin->instr, &intrin->dest,
                        dest_components, 32, NULL);

      nir_builder_instr_insert(&b->nb, &intrin->instr);

      val->ssa = vtn_create_ssa_value(b, type->type);
      val->ssa->def = &intrin->dest.ssa;
   } else {
      nir_builder_instr_insert(&b->nb, &intrin->instr);
   }
}

static nir_intrinsic_op
get_ssbo_nir_atomic_op(SpvOp opcode)
{
   switch (opcode) {
   case SpvOpAtomicLoad:      return nir_intrinsic_load_ssbo;
   case SpvOpAtomicStore:     return nir_intrinsic_store_ssbo;
#define OP(S, N) case SpvOp##S: return nir_intrinsic_ssbo_##N;
   OP(AtomicExchange,         atomic_exchange)
   OP(AtomicCompareExchange,  atomic_comp_swap)
   OP(AtomicIIncrement,       atomic_add)
   OP(AtomicIDecrement,       atomic_add)
   OP(AtomicIAdd,             atomic_add)
   OP(AtomicISub,             atomic_add)
   OP(AtomicSMin,             atomic_imin)
   OP(AtomicUMin,             atomic_umin)
   OP(AtomicSMax,             atomic_imax)
   OP(AtomicUMax,             atomic_umax)
   OP(AtomicAnd,              atomic_and)
   OP(AtomicOr,               atomic_or)
   OP(AtomicXor,              atomic_xor)
#undef OP
   default:
      unreachable("Invalid SSBO atomic");
   }
}

static nir_intrinsic_op
get_shared_nir_atomic_op(SpvOp opcode)
{
   switch (opcode) {
   case SpvOpAtomicLoad:      return nir_intrinsic_load_var;
   case SpvOpAtomicStore:     return nir_intrinsic_store_var;
#define OP(S, N) case SpvOp##S: return nir_intrinsic_var_##N;
   OP(AtomicExchange,         atomic_exchange)
   OP(AtomicCompareExchange,  atomic_comp_swap)
   OP(AtomicIIncrement,       atomic_add)
   OP(AtomicIDecrement,       atomic_add)
   OP(AtomicIAdd,             atomic_add)
   OP(AtomicISub,             atomic_add)
   OP(AtomicSMin,             atomic_imin)
   OP(AtomicUMin,             atomic_umin)
   OP(AtomicSMax,             atomic_imax)
   OP(AtomicUMax,             atomic_umax)
   OP(AtomicAnd,              atomic_and)
   OP(AtomicOr,               atomic_or)
   OP(AtomicXor,              atomic_xor)
#undef OP
   default:
      unreachable("Invalid shared atomic");
   }
}

static void
vtn_handle_ssbo_or_shared_atomic(struct vtn_builder *b, SpvOp opcode,
                                 const uint32_t *w, unsigned count)
{
   struct vtn_pointer *ptr;
   nir_intrinsic_instr *atomic;

   switch (opcode) {
   case SpvOpAtomicLoad:
   case SpvOpAtomicExchange:
   case SpvOpAtomicCompareExchange:
   case SpvOpAtomicCompareExchangeWeak:
   case SpvOpAtomicIIncrement:
   case SpvOpAtomicIDecrement:
   case SpvOpAtomicIAdd:
   case SpvOpAtomicISub:
   case SpvOpAtomicSMin:
   case SpvOpAtomicUMin:
   case SpvOpAtomicSMax:
   case SpvOpAtomicUMax:
   case SpvOpAtomicAnd:
   case SpvOpAtomicOr:
   case SpvOpAtomicXor:
      ptr = vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      break;

   case SpvOpAtomicStore:
      ptr = vtn_value(b, w[1], vtn_value_type_pointer)->pointer;
      break;

   default:
      unreachable("Invalid SPIR-V atomic");
   }

   /*
   SpvScope scope = w[4];
   SpvMemorySemanticsMask semantics = w[5];
   */

   if (ptr->mode == vtn_variable_mode_workgroup) {
      nir_deref_var *deref = vtn_pointer_to_deref(b, ptr);
      const struct glsl_type *deref_type = nir_deref_tail(&deref->deref)->type;
      nir_intrinsic_op op = get_shared_nir_atomic_op(opcode);
      atomic = nir_intrinsic_instr_create(b->nb.shader, op);
      atomic->variables[0] = nir_deref_var_clone(deref, atomic);

      switch (opcode) {
      case SpvOpAtomicLoad:
         atomic->num_components = glsl_get_vector_elements(deref_type);
         break;

      case SpvOpAtomicStore:
         atomic->num_components = glsl_get_vector_elements(deref_type);
         nir_intrinsic_set_write_mask(atomic, (1 << atomic->num_components) - 1);
         atomic->src[0] = nir_src_for_ssa(vtn_ssa_value(b, w[4])->def);
         break;

      case SpvOpAtomicExchange:
      case SpvOpAtomicCompareExchange:
      case SpvOpAtomicCompareExchangeWeak:
      case SpvOpAtomicIIncrement:
      case SpvOpAtomicIDecrement:
      case SpvOpAtomicIAdd:
      case SpvOpAtomicISub:
      case SpvOpAtomicSMin:
      case SpvOpAtomicUMin:
      case SpvOpAtomicSMax:
      case SpvOpAtomicUMax:
      case SpvOpAtomicAnd:
      case SpvOpAtomicOr:
      case SpvOpAtomicXor:
         fill_common_atomic_sources(b, opcode, w, &atomic->src[0]);
         break;

      default:
         unreachable("Invalid SPIR-V atomic");

      }
   } else {
      assert(ptr->mode == vtn_variable_mode_ssbo);
      nir_ssa_def *offset, *index;
      offset = vtn_pointer_to_offset(b, ptr, &index, NULL);

      nir_intrinsic_op op = get_ssbo_nir_atomic_op(opcode);

      atomic = nir_intrinsic_instr_create(b->nb.shader, op);

      switch (opcode) {
      case SpvOpAtomicLoad:
         atomic->num_components = glsl_get_vector_elements(ptr->type->type);
         atomic->src[0] = nir_src_for_ssa(index);
         atomic->src[1] = nir_src_for_ssa(offset);
         break;

      case SpvOpAtomicStore:
         atomic->num_components = glsl_get_vector_elements(ptr->type->type);
         nir_intrinsic_set_write_mask(atomic, (1 << atomic->num_components) - 1);
         atomic->src[0] = nir_src_for_ssa(vtn_ssa_value(b, w[4])->def);
         atomic->src[1] = nir_src_for_ssa(index);
         atomic->src[2] = nir_src_for_ssa(offset);
         break;

      case SpvOpAtomicExchange:
      case SpvOpAtomicCompareExchange:
      case SpvOpAtomicCompareExchangeWeak:
      case SpvOpAtomicIIncrement:
      case SpvOpAtomicIDecrement:
      case SpvOpAtomicIAdd:
      case SpvOpAtomicISub:
      case SpvOpAtomicSMin:
      case SpvOpAtomicUMin:
      case SpvOpAtomicSMax:
      case SpvOpAtomicUMax:
      case SpvOpAtomicAnd:
      case SpvOpAtomicOr:
      case SpvOpAtomicXor:
         atomic->src[0] = nir_src_for_ssa(index);
         atomic->src[1] = nir_src_for_ssa(offset);
         fill_common_atomic_sources(b, opcode, w, &atomic->src[2]);
         break;

      default:
         unreachable("Invalid SPIR-V atomic");
      }
   }

   if (opcode != SpvOpAtomicStore) {
      struct vtn_type *type = vtn_value(b, w[1], vtn_value_type_type)->type;

      nir_ssa_dest_init(&atomic->instr, &atomic->dest,
                        glsl_get_vector_elements(type->type),
                        glsl_get_bit_size(type->type), NULL);

      struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_ssa);
      val->ssa = rzalloc(b, struct vtn_ssa_value);
      val->ssa->def = &atomic->dest.ssa;
      val->ssa->type = type->type;
   }

   nir_builder_instr_insert(&b->nb, &atomic->instr);
}

static nir_alu_instr *
create_vec(nir_shader *shader, unsigned num_components, unsigned bit_size)
{
   nir_op op;
   switch (num_components) {
   case 1: op = nir_op_fmov; break;
   case 2: op = nir_op_vec2; break;
   case 3: op = nir_op_vec3; break;
   case 4: op = nir_op_vec4; break;
   default: unreachable("bad vector size");
   }

   nir_alu_instr *vec = nir_alu_instr_create(shader, op);
   nir_ssa_dest_init(&vec->instr, &vec->dest.dest, num_components,
                     bit_size, NULL);
   vec->dest.write_mask = (1 << num_components) - 1;

   return vec;
}

struct vtn_ssa_value *
vtn_ssa_transpose(struct vtn_builder *b, struct vtn_ssa_value *src)
{
   if (src->transposed)
      return src->transposed;

   struct vtn_ssa_value *dest =
      vtn_create_ssa_value(b, glsl_transposed_type(src->type));

   for (unsigned i = 0; i < glsl_get_matrix_columns(dest->type); i++) {
      nir_alu_instr *vec = create_vec(b->shader,
                                      glsl_get_matrix_columns(src->type),
                                      glsl_get_bit_size(src->type));
      if (glsl_type_is_vector_or_scalar(src->type)) {
          vec->src[0].src = nir_src_for_ssa(src->def);
          vec->src[0].swizzle[0] = i;
      } else {
         for (unsigned j = 0; j < glsl_get_matrix_columns(src->type); j++) {
            vec->src[j].src = nir_src_for_ssa(src->elems[j]->def);
            vec->src[j].swizzle[0] = i;
         }
      }
      nir_builder_instr_insert(&b->nb, &vec->instr);
      dest->elems[i]->def = &vec->dest.dest.ssa;
   }

   dest->transposed = src;

   return dest;
}

nir_ssa_def *
vtn_vector_extract(struct vtn_builder *b, nir_ssa_def *src, unsigned index)
{
   unsigned swiz[4] = { index };
   return nir_swizzle(&b->nb, src, swiz, 1, true);
}

nir_ssa_def *
vtn_vector_insert(struct vtn_builder *b, nir_ssa_def *src, nir_ssa_def *insert,
                  unsigned index)
{
   nir_alu_instr *vec = create_vec(b->shader, src->num_components,
                                   src->bit_size);

   for (unsigned i = 0; i < src->num_components; i++) {
      if (i == index) {
         vec->src[i].src = nir_src_for_ssa(insert);
      } else {
         vec->src[i].src = nir_src_for_ssa(src);
         vec->src[i].swizzle[0] = i;
      }
   }

   nir_builder_instr_insert(&b->nb, &vec->instr);

   return &vec->dest.dest.ssa;
}

nir_ssa_def *
vtn_vector_extract_dynamic(struct vtn_builder *b, nir_ssa_def *src,
                           nir_ssa_def *index)
{
   nir_ssa_def *dest = vtn_vector_extract(b, src, 0);
   for (unsigned i = 1; i < src->num_components; i++)
      dest = nir_bcsel(&b->nb, nir_ieq(&b->nb, index, nir_imm_int(&b->nb, i)),
                       vtn_vector_extract(b, src, i), dest);

   return dest;
}

nir_ssa_def *
vtn_vector_insert_dynamic(struct vtn_builder *b, nir_ssa_def *src,
                          nir_ssa_def *insert, nir_ssa_def *index)
{
   nir_ssa_def *dest = vtn_vector_insert(b, src, insert, 0);
   for (unsigned i = 1; i < src->num_components; i++)
      dest = nir_bcsel(&b->nb, nir_ieq(&b->nb, index, nir_imm_int(&b->nb, i)),
                       vtn_vector_insert(b, src, insert, i), dest);

   return dest;
}

static nir_ssa_def *
vtn_vector_shuffle(struct vtn_builder *b, unsigned num_components,
                   nir_ssa_def *src0, nir_ssa_def *src1,
                   const uint32_t *indices)
{
   nir_alu_instr *vec = create_vec(b->shader, num_components, src0->bit_size);

   for (unsigned i = 0; i < num_components; i++) {
      uint32_t index = indices[i];
      if (index == 0xffffffff) {
         vec->src[i].src =
            nir_src_for_ssa(nir_ssa_undef(&b->nb, 1, src0->bit_size));
      } else if (index < src0->num_components) {
         vec->src[i].src = nir_src_for_ssa(src0);
         vec->src[i].swizzle[0] = index;
      } else {
         vec->src[i].src = nir_src_for_ssa(src1);
         vec->src[i].swizzle[0] = index - src0->num_components;
      }
   }

   nir_builder_instr_insert(&b->nb, &vec->instr);

   return &vec->dest.dest.ssa;
}

/*
 * Concatentates a number of vectors/scalars together to produce a vector
 */
static nir_ssa_def *
vtn_vector_construct(struct vtn_builder *b, unsigned num_components,
                     unsigned num_srcs, nir_ssa_def **srcs)
{
   nir_alu_instr *vec = create_vec(b->shader, num_components,
                                   srcs[0]->bit_size);

   /* From the SPIR-V 1.1 spec for OpCompositeConstruct:
    *
    *    "When constructing a vector, there must be at least two Constituent
    *    operands."
    */
   assert(num_srcs >= 2);

   unsigned dest_idx = 0;
   for (unsigned i = 0; i < num_srcs; i++) {
      nir_ssa_def *src = srcs[i];
      assert(dest_idx + src->num_components <= num_components);
      for (unsigned j = 0; j < src->num_components; j++) {
         vec->src[dest_idx].src = nir_src_for_ssa(src);
         vec->src[dest_idx].swizzle[0] = j;
         dest_idx++;
      }
   }

   /* From the SPIR-V 1.1 spec for OpCompositeConstruct:
    *
    *    "When constructing a vector, the total number of components in all
    *    the operands must equal the number of components in Result Type."
    */
   assert(dest_idx == num_components);

   nir_builder_instr_insert(&b->nb, &vec->instr);

   return &vec->dest.dest.ssa;
}

static struct vtn_ssa_value *
vtn_composite_copy(void *mem_ctx, struct vtn_ssa_value *src)
{
   struct vtn_ssa_value *dest = rzalloc(mem_ctx, struct vtn_ssa_value);
   dest->type = src->type;

   if (glsl_type_is_vector_or_scalar(src->type)) {
      dest->def = src->def;
   } else {
      unsigned elems = glsl_get_length(src->type);

      dest->elems = ralloc_array(mem_ctx, struct vtn_ssa_value *, elems);
      for (unsigned i = 0; i < elems; i++)
         dest->elems[i] = vtn_composite_copy(mem_ctx, src->elems[i]);
   }

   return dest;
}

static struct vtn_ssa_value *
vtn_composite_insert(struct vtn_builder *b, struct vtn_ssa_value *src,
                     struct vtn_ssa_value *insert, const uint32_t *indices,
                     unsigned num_indices)
{
   struct vtn_ssa_value *dest = vtn_composite_copy(b, src);

   struct vtn_ssa_value *cur = dest;
   unsigned i;
   for (i = 0; i < num_indices - 1; i++) {
      cur = cur->elems[indices[i]];
   }

   if (glsl_type_is_vector_or_scalar(cur->type)) {
      /* According to the SPIR-V spec, OpCompositeInsert may work down to
       * the component granularity. In that case, the last index will be
       * the index to insert the scalar into the vector.
       */

      cur->def = vtn_vector_insert(b, cur->def, insert->def, indices[i]);
   } else {
      cur->elems[indices[i]] = insert;
   }

   return dest;
}

static struct vtn_ssa_value *
vtn_composite_extract(struct vtn_builder *b, struct vtn_ssa_value *src,
                      const uint32_t *indices, unsigned num_indices)
{
   struct vtn_ssa_value *cur = src;
   for (unsigned i = 0; i < num_indices; i++) {
      if (glsl_type_is_vector_or_scalar(cur->type)) {
         assert(i == num_indices - 1);
         /* According to the SPIR-V spec, OpCompositeExtract may work down to
          * the component granularity. The last index will be the index of the
          * vector to extract.
          */

         struct vtn_ssa_value *ret = rzalloc(b, struct vtn_ssa_value);
         ret->type = glsl_scalar_type(glsl_get_base_type(cur->type));
         ret->def = vtn_vector_extract(b, cur->def, indices[i]);
         return ret;
      } else {
         cur = cur->elems[indices[i]];
      }
   }

   return cur;
}

static void
vtn_handle_composite(struct vtn_builder *b, SpvOp opcode,
                     const uint32_t *w, unsigned count)
{
   struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_ssa);
   const struct glsl_type *type =
      vtn_value(b, w[1], vtn_value_type_type)->type->type;
   val->ssa = vtn_create_ssa_value(b, type);

   switch (opcode) {
   case SpvOpVectorExtractDynamic:
      val->ssa->def = vtn_vector_extract_dynamic(b, vtn_ssa_value(b, w[3])->def,
                                                 vtn_ssa_value(b, w[4])->def);
      break;

   case SpvOpVectorInsertDynamic:
      val->ssa->def = vtn_vector_insert_dynamic(b, vtn_ssa_value(b, w[3])->def,
                                                vtn_ssa_value(b, w[4])->def,
                                                vtn_ssa_value(b, w[5])->def);
      break;

   case SpvOpVectorShuffle:
      val->ssa->def = vtn_vector_shuffle(b, glsl_get_vector_elements(type),
                                         vtn_ssa_value(b, w[3])->def,
                                         vtn_ssa_value(b, w[4])->def,
                                         w + 5);
      break;

   case SpvOpCompositeConstruct: {
      unsigned elems = count - 3;
      if (glsl_type_is_vector_or_scalar(type)) {
         nir_ssa_def *srcs[4];
         for (unsigned i = 0; i < elems; i++)
            srcs[i] = vtn_ssa_value(b, w[3 + i])->def;
         val->ssa->def =
            vtn_vector_construct(b, glsl_get_vector_elements(type),
                                 elems, srcs);
      } else {
         val->ssa->elems = ralloc_array(b, struct vtn_ssa_value *, elems);
         for (unsigned i = 0; i < elems; i++)
            val->ssa->elems[i] = vtn_ssa_value(b, w[3 + i]);
      }
      break;
   }
   case SpvOpCompositeExtract:
      val->ssa = vtn_composite_extract(b, vtn_ssa_value(b, w[3]),
                                       w + 4, count - 4);
      break;

   case SpvOpCompositeInsert:
      val->ssa = vtn_composite_insert(b, vtn_ssa_value(b, w[4]),
                                      vtn_ssa_value(b, w[3]),
                                      w + 5, count - 5);
      break;

   case SpvOpCopyObject:
      val->ssa = vtn_composite_copy(b, vtn_ssa_value(b, w[3]));
      break;

   default:
      unreachable("unknown composite operation");
   }
}

static void
vtn_handle_barrier(struct vtn_builder *b, SpvOp opcode,
                   const uint32_t *w, unsigned count)
{
   nir_intrinsic_op intrinsic_op;
   switch (opcode) {
   case SpvOpEmitVertex:
   case SpvOpEmitStreamVertex:
      intrinsic_op = nir_intrinsic_emit_vertex;
      break;
   case SpvOpEndPrimitive:
   case SpvOpEndStreamPrimitive:
      intrinsic_op = nir_intrinsic_end_primitive;
      break;
   case SpvOpMemoryBarrier:
      intrinsic_op = nir_intrinsic_memory_barrier;
      break;
   case SpvOpControlBarrier:
      intrinsic_op = nir_intrinsic_barrier;
      break;
   default:
      unreachable("unknown barrier instruction");
   }

   nir_intrinsic_instr *intrin =
      nir_intrinsic_instr_create(b->shader, intrinsic_op);

   if (opcode == SpvOpEmitStreamVertex || opcode == SpvOpEndStreamPrimitive)
      nir_intrinsic_set_stream_id(intrin, w[1]);

   nir_builder_instr_insert(&b->nb, &intrin->instr);
}

static unsigned
gl_primitive_from_spv_execution_mode(SpvExecutionMode mode)
{
   switch (mode) {
   case SpvExecutionModeInputPoints:
   case SpvExecutionModeOutputPoints:
      return 0; /* GL_POINTS */
   case SpvExecutionModeInputLines:
      return 1; /* GL_LINES */
   case SpvExecutionModeInputLinesAdjacency:
      return 0x000A; /* GL_LINE_STRIP_ADJACENCY_ARB */
   case SpvExecutionModeTriangles:
      return 4; /* GL_TRIANGLES */
   case SpvExecutionModeInputTrianglesAdjacency:
      return 0x000C; /* GL_TRIANGLES_ADJACENCY_ARB */
   case SpvExecutionModeQuads:
      return 7; /* GL_QUADS */
   case SpvExecutionModeIsolines:
      return 0x8E7A; /* GL_ISOLINES */
   case SpvExecutionModeOutputLineStrip:
      return 3; /* GL_LINE_STRIP */
   case SpvExecutionModeOutputTriangleStrip:
      return 5; /* GL_TRIANGLE_STRIP */
   default:
      unreachable("Invalid primitive type");
      return 4;
   }
}

static unsigned
vertices_in_from_spv_execution_mode(SpvExecutionMode mode)
{
   switch (mode) {
   case SpvExecutionModeInputPoints:
      return 1;
   case SpvExecutionModeInputLines:
      return 2;
   case SpvExecutionModeInputLinesAdjacency:
      return 4;
   case SpvExecutionModeTriangles:
      return 3;
   case SpvExecutionModeInputTrianglesAdjacency:
      return 6;
   default:
      unreachable("Invalid GS input mode");
      return 0;
   }
}

static gl_shader_stage
stage_for_execution_model(SpvExecutionModel model)
{
   switch (model) {
   case SpvExecutionModelVertex:
      return MESA_SHADER_VERTEX;
   case SpvExecutionModelTessellationControl:
      return MESA_SHADER_TESS_CTRL;
   case SpvExecutionModelTessellationEvaluation:
      return MESA_SHADER_TESS_EVAL;
   case SpvExecutionModelGeometry:
      return MESA_SHADER_GEOMETRY;
   case SpvExecutionModelFragment:
      return MESA_SHADER_FRAGMENT;
   case SpvExecutionModelGLCompute:
      return MESA_SHADER_COMPUTE;
   default:
      unreachable("Unsupported execution model");
   }
}

#define spv_check_supported(name, cap) do {		\
      if (!(b->ext && b->ext->name))			\
         vtn_warn("Unsupported SPIR-V capability: %s",  \
                  spirv_capability_to_string(cap));     \
   } while(0)

static bool
vtn_handle_preamble_instruction(struct vtn_builder *b, SpvOp opcode,
                                const uint32_t *w, unsigned count)
{
   switch (opcode) {
   case SpvOpSource:
   case SpvOpSourceExtension:
   case SpvOpSourceContinued:
   case SpvOpExtension:
      /* Unhandled, but these are for debug so that's ok. */
      break;

   case SpvOpCapability: {
      SpvCapability cap = w[1];
      switch (cap) {
      case SpvCapabilityMatrix:
      case SpvCapabilityShader:
      case SpvCapabilityGeometry:
      case SpvCapabilityGeometryPointSize:
      case SpvCapabilityUniformBufferArrayDynamicIndexing:
      case SpvCapabilitySampledImageArrayDynamicIndexing:
      case SpvCapabilityStorageBufferArrayDynamicIndexing:
      case SpvCapabilityStorageImageArrayDynamicIndexing:
      case SpvCapabilityImageRect:
      case SpvCapabilitySampledRect:
      case SpvCapabilitySampled1D:
      case SpvCapabilityImage1D:
      case SpvCapabilitySampledCubeArray:
      case SpvCapabilityImageCubeArray:
      case SpvCapabilitySampledBuffer:
      case SpvCapabilityImageBuffer:
      case SpvCapabilityImageQuery:
      case SpvCapabilityDerivativeControl:
      case SpvCapabilityInterpolationFunction:
      case SpvCapabilityMultiViewport:
      case SpvCapabilitySampleRateShading:
      case SpvCapabilityClipDistance:
      case SpvCapabilityCullDistance:
      case SpvCapabilityInputAttachment:
      case SpvCapabilityImageGatherExtended:
      case SpvCapabilityStorageImageExtendedFormats:
         break;

      case SpvCapabilityGeometryStreams:
      case SpvCapabilityLinkage:
      case SpvCapabilityVector16:
      case SpvCapabilityFloat16Buffer:
      case SpvCapabilityFloat16:
      case SpvCapabilityInt64Atomics:
      case SpvCapabilityAtomicStorage:
      case SpvCapabilityInt16:
      case SpvCapabilityStorageImageMultisample:
      case SpvCapabilityInt8:
      case SpvCapabilitySparseResidency:
      case SpvCapabilityMinLod:
      case SpvCapabilityTransformFeedback:
         vtn_warn("Unsupported SPIR-V capability: %s",
                  spirv_capability_to_string(cap));
         break;

      case SpvCapabilityFloat64:
         spv_check_supported(float64, cap);
         break;
      case SpvCapabilityInt64:
         spv_check_supported(int64, cap);
         break;

      case SpvCapabilityAddresses:
      case SpvCapabilityKernel:
      case SpvCapabilityImageBasic:
      case SpvCapabilityImageReadWrite:
      case SpvCapabilityImageMipmap:
      case SpvCapabilityPipes:
      case SpvCapabilityGroups:
      case SpvCapabilityDeviceEnqueue:
      case SpvCapabilityLiteralSampler:
      case SpvCapabilityGenericPointer:
         vtn_warn("Unsupported OpenCL-style SPIR-V capability: %s",
                  spirv_capability_to_string(cap));
         break;

      case SpvCapabilityImageMSArray:
         spv_check_supported(image_ms_array, cap);
         break;

      case SpvCapabilityTessellation:
      case SpvCapabilityTessellationPointSize:
         spv_check_supported(tessellation, cap);
         break;

      case SpvCapabilityDrawParameters:
         spv_check_supported(draw_parameters, cap);
         break;

      case SpvCapabilityStorageImageReadWithoutFormat:
         spv_check_supported(image_read_without_format, cap);
         break;

      case SpvCapabilityStorageImageWriteWithoutFormat:
         spv_check_supported(image_write_without_format, cap);
         break;

      case SpvCapabilityMultiView:
         spv_check_supported(multiview, cap);
         break;

      case SpvCapabilityVariablePointersStorageBuffer:
      case SpvCapabilityVariablePointers:
         spv_check_supported(variable_pointers, cap);
         break;

      default:
         unreachable("Unhandled capability");
      }
      break;
   }

   case SpvOpExtInstImport:
      vtn_handle_extension(b, opcode, w, count);
      break;

   case SpvOpMemoryModel:
      assert(w[1] == SpvAddressingModelLogical);
      assert(w[2] == SpvMemoryModelSimple ||
             w[2] == SpvMemoryModelGLSL450);
      break;

   case SpvOpEntryPoint: {
      struct vtn_value *entry_point = &b->values[w[2]];
      /* Let this be a name label regardless */
      unsigned name_words;
      entry_point->name = vtn_string_literal(b, &w[3], count - 3, &name_words);

      if (strcmp(entry_point->name, b->entry_point_name) != 0 ||
          stage_for_execution_model(w[1]) != b->entry_point_stage)
         break;

      assert(b->entry_point == NULL);
      b->entry_point = entry_point;
      break;
   }

   case SpvOpString:
      vtn_push_value(b, w[1], vtn_value_type_string)->str =
         vtn_string_literal(b, &w[2], count - 2, NULL);
      break;

   case SpvOpName:
      b->values[w[1]].name = vtn_string_literal(b, &w[2], count - 2, NULL);
      break;

   case SpvOpMemberName:
      /* TODO */
      break;

   case SpvOpExecutionMode:
   case SpvOpDecorationGroup:
   case SpvOpDecorate:
   case SpvOpMemberDecorate:
   case SpvOpGroupDecorate:
   case SpvOpGroupMemberDecorate:
      vtn_handle_decoration(b, opcode, w, count);
      break;

   default:
      return false; /* End of preamble */
   }

   return true;
}

static void
vtn_handle_execution_mode(struct vtn_builder *b, struct vtn_value *entry_point,
                          const struct vtn_decoration *mode, void *data)
{
   assert(b->entry_point == entry_point);

   switch(mode->exec_mode) {
   case SpvExecutionModeOriginUpperLeft:
   case SpvExecutionModeOriginLowerLeft:
      b->origin_upper_left =
         (mode->exec_mode == SpvExecutionModeOriginUpperLeft);
      break;

   case SpvExecutionModeEarlyFragmentTests:
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      b->shader->info.fs.early_fragment_tests = true;
      break;

   case SpvExecutionModeInvocations:
      assert(b->shader->info.stage == MESA_SHADER_GEOMETRY);
      b->shader->info.gs.invocations = MAX2(1, mode->literals[0]);
      break;

   case SpvExecutionModeDepthReplacing:
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      b->shader->info.fs.depth_layout = FRAG_DEPTH_LAYOUT_ANY;
      break;
   case SpvExecutionModeDepthGreater:
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      b->shader->info.fs.depth_layout = FRAG_DEPTH_LAYOUT_GREATER;
      break;
   case SpvExecutionModeDepthLess:
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      b->shader->info.fs.depth_layout = FRAG_DEPTH_LAYOUT_LESS;
      break;
   case SpvExecutionModeDepthUnchanged:
      assert(b->shader->info.stage == MESA_SHADER_FRAGMENT);
      b->shader->info.fs.depth_layout = FRAG_DEPTH_LAYOUT_UNCHANGED;
      break;

   case SpvExecutionModeLocalSize:
      assert(b->shader->info.stage == MESA_SHADER_COMPUTE);
      b->shader->info.cs.local_size[0] = mode->literals[0];
      b->shader->info.cs.local_size[1] = mode->literals[1];
      b->shader->info.cs.local_size[2] = mode->literals[2];
      break;
   case SpvExecutionModeLocalSizeHint:
      break; /* Nothing to do with this */

   case SpvExecutionModeOutputVertices:
      if (b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
          b->shader->info.stage == MESA_SHADER_TESS_EVAL) {
         b->shader->info.tess.tcs_vertices_out = mode->literals[0];
      } else {
         assert(b->shader->info.stage == MESA_SHADER_GEOMETRY);
         b->shader->info.gs.vertices_out = mode->literals[0];
      }
      break;

   case SpvExecutionModeInputPoints:
   case SpvExecutionModeInputLines:
   case SpvExecutionModeInputLinesAdjacency:
   case SpvExecutionModeTriangles:
   case SpvExecutionModeInputTrianglesAdjacency:
   case SpvExecutionModeQuads:
   case SpvExecutionModeIsolines:
      if (b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
          b->shader->info.stage == MESA_SHADER_TESS_EVAL) {
         b->shader->info.tess.primitive_mode =
            gl_primitive_from_spv_execution_mode(mode->exec_mode);
      } else {
         assert(b->shader->info.stage == MESA_SHADER_GEOMETRY);
         b->shader->info.gs.vertices_in =
            vertices_in_from_spv_execution_mode(mode->exec_mode);
      }
      break;

   case SpvExecutionModeOutputPoints:
   case SpvExecutionModeOutputLineStrip:
   case SpvExecutionModeOutputTriangleStrip:
      assert(b->shader->info.stage == MESA_SHADER_GEOMETRY);
      b->shader->info.gs.output_primitive =
         gl_primitive_from_spv_execution_mode(mode->exec_mode);
      break;

   case SpvExecutionModeSpacingEqual:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.spacing = TESS_SPACING_EQUAL;
      break;
   case SpvExecutionModeSpacingFractionalEven:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.spacing = TESS_SPACING_FRACTIONAL_EVEN;
      break;
   case SpvExecutionModeSpacingFractionalOdd:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.spacing = TESS_SPACING_FRACTIONAL_ODD;
      break;
   case SpvExecutionModeVertexOrderCw:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.ccw = false;
      break;
   case SpvExecutionModeVertexOrderCcw:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.ccw = true;
      break;
   case SpvExecutionModePointMode:
      assert(b->shader->info.stage == MESA_SHADER_TESS_CTRL ||
             b->shader->info.stage == MESA_SHADER_TESS_EVAL);
      b->shader->info.tess.point_mode = true;
      break;

   case SpvExecutionModePixelCenterInteger:
      b->pixel_center_integer = true;
      break;

   case SpvExecutionModeXfb:
      unreachable("Unhandled execution mode");
      break;

   case SpvExecutionModeVecTypeHint:
   case SpvExecutionModeContractionOff:
      break; /* OpenCL */

   default:
      unreachable("Unhandled execution mode");
   }
}

static bool
vtn_handle_variable_or_type_instruction(struct vtn_builder *b, SpvOp opcode,
                                        const uint32_t *w, unsigned count)
{
   switch (opcode) {
   case SpvOpSource:
   case SpvOpSourceContinued:
   case SpvOpSourceExtension:
   case SpvOpExtension:
   case SpvOpCapability:
   case SpvOpExtInstImport:
   case SpvOpMemoryModel:
   case SpvOpEntryPoint:
   case SpvOpExecutionMode:
   case SpvOpString:
   case SpvOpName:
   case SpvOpMemberName:
   case SpvOpDecorationGroup:
   case SpvOpDecorate:
   case SpvOpMemberDecorate:
   case SpvOpGroupDecorate:
   case SpvOpGroupMemberDecorate:
      unreachable("Invalid opcode types and variables section");
      break;

   case SpvOpTypeVoid:
   case SpvOpTypeBool:
   case SpvOpTypeInt:
   case SpvOpTypeFloat:
   case SpvOpTypeVector:
   case SpvOpTypeMatrix:
   case SpvOpTypeImage:
   case SpvOpTypeSampler:
   case SpvOpTypeSampledImage:
   case SpvOpTypeArray:
   case SpvOpTypeRuntimeArray:
   case SpvOpTypeStruct:
   case SpvOpTypeOpaque:
   case SpvOpTypePointer:
   case SpvOpTypeFunction:
   case SpvOpTypeEvent:
   case SpvOpTypeDeviceEvent:
   case SpvOpTypeReserveId:
   case SpvOpTypeQueue:
   case SpvOpTypePipe:
      vtn_handle_type(b, opcode, w, count);
      break;

   case SpvOpConstantTrue:
   case SpvOpConstantFalse:
   case SpvOpConstant:
   case SpvOpConstantComposite:
   case SpvOpConstantSampler:
   case SpvOpConstantNull:
   case SpvOpSpecConstantTrue:
   case SpvOpSpecConstantFalse:
   case SpvOpSpecConstant:
   case SpvOpSpecConstantComposite:
   case SpvOpSpecConstantOp:
      vtn_handle_constant(b, opcode, w, count);
      break;

   case SpvOpUndef:
   case SpvOpVariable:
      vtn_handle_variables(b, opcode, w, count);
      break;

   default:
      return false; /* End of preamble */
   }

   return true;
}

static bool
vtn_handle_body_instruction(struct vtn_builder *b, SpvOp opcode,
                            const uint32_t *w, unsigned count)
{
   switch (opcode) {
   case SpvOpLabel:
      break;

   case SpvOpLoopMerge:
   case SpvOpSelectionMerge:
      /* This is handled by cfg pre-pass and walk_blocks */
      break;

   case SpvOpUndef: {
      struct vtn_value *val = vtn_push_value(b, w[2], vtn_value_type_undef);
      val->type = vtn_value(b, w[1], vtn_value_type_type)->type;
      break;
   }

   case SpvOpExtInst:
      vtn_handle_extension(b, opcode, w, count);
      break;

   case SpvOpVariable:
   case SpvOpLoad:
   case SpvOpStore:
   case SpvOpCopyMemory:
   case SpvOpCopyMemorySized:
   case SpvOpAccessChain:
   case SpvOpPtrAccessChain:
   case SpvOpInBoundsAccessChain:
   case SpvOpArrayLength:
      vtn_handle_variables(b, opcode, w, count);
      break;

   case SpvOpFunctionCall:
      vtn_handle_function_call(b, opcode, w, count);
      break;

   case SpvOpSampledImage:
   case SpvOpImage:
   case SpvOpImageSampleImplicitLod:
   case SpvOpImageSampleExplicitLod:
   case SpvOpImageSampleDrefImplicitLod:
   case SpvOpImageSampleDrefExplicitLod:
   case SpvOpImageSampleProjImplicitLod:
   case SpvOpImageSampleProjExplicitLod:
   case SpvOpImageSampleProjDrefImplicitLod:
   case SpvOpImageSampleProjDrefExplicitLod:
   case SpvOpImageFetch:
   case SpvOpImageGather:
   case SpvOpImageDrefGather:
   case SpvOpImageQuerySizeLod:
   case SpvOpImageQueryLod:
   case SpvOpImageQueryLevels:
   case SpvOpImageQuerySamples:
      vtn_handle_texture(b, opcode, w, count);
      break;

   case SpvOpImageRead:
   case SpvOpImageWrite:
   case SpvOpImageTexelPointer:
      vtn_handle_image(b, opcode, w, count);
      break;

   case SpvOpImageQuerySize: {
      struct vtn_pointer *image =
         vtn_value(b, w[3], vtn_value_type_pointer)->pointer;
      if (image->mode == vtn_variable_mode_image) {
         vtn_handle_image(b, opcode, w, count);
      } else {
         assert(image->mode == vtn_variable_mode_sampler);
         vtn_handle_texture(b, opcode, w, count);
      }
      break;
   }

   case SpvOpAtomicLoad:
   case SpvOpAtomicExchange:
   case SpvOpAtomicCompareExchange:
   case SpvOpAtomicCompareExchangeWeak:
   case SpvOpAtomicIIncrement:
   case SpvOpAtomicIDecrement:
   case SpvOpAtomicIAdd:
   case SpvOpAtomicISub:
   case SpvOpAtomicSMin:
   case SpvOpAtomicUMin:
   case SpvOpAtomicSMax:
   case SpvOpAtomicUMax:
   case SpvOpAtomicAnd:
   case SpvOpAtomicOr:
   case SpvOpAtomicXor: {
      struct vtn_value *pointer = vtn_untyped_value(b, w[3]);
      if (pointer->value_type == vtn_value_type_image_pointer) {
         vtn_handle_image(b, opcode, w, count);
      } else {
         assert(pointer->value_type == vtn_value_type_pointer);
         vtn_handle_ssbo_or_shared_atomic(b, opcode, w, count);
      }
      break;
   }

   case SpvOpAtomicStore: {
      struct vtn_value *pointer = vtn_untyped_value(b, w[1]);
      if (pointer->value_type == vtn_value_type_image_pointer) {
         vtn_handle_image(b, opcode, w, count);
      } else {
         assert(pointer->value_type == vtn_value_type_pointer);
         vtn_handle_ssbo_or_shared_atomic(b, opcode, w, count);
      }
      break;
   }

   case SpvOpSelect: {
      /* Handle OpSelect up-front here because it needs to be able to handle
       * pointers and not just regular vectors and scalars.
       */
      struct vtn_type *res_type = vtn_value(b, w[1], vtn_value_type_type)->type;
      struct vtn_ssa_value *ssa = vtn_create_ssa_value(b, res_type->type);
      ssa->def = nir_bcsel(&b->nb, vtn_ssa_value(b, w[3])->def,
                                   vtn_ssa_value(b, w[4])->def,
                                   vtn_ssa_value(b, w[5])->def);
      vtn_push_ssa(b, w[2], res_type, ssa);
      break;
   }

   case SpvOpSNegate:
   case SpvOpFNegate:
   case SpvOpNot:
   case SpvOpAny:
   case SpvOpAll:
   case SpvOpConvertFToU:
   case SpvOpConvertFToS:
   case SpvOpConvertSToF:
   case SpvOpConvertUToF:
   case SpvOpUConvert:
   case SpvOpSConvert:
   case SpvOpFConvert:
   case SpvOpQuantizeToF16:
   case SpvOpConvertPtrToU:
   case SpvOpConvertUToPtr:
   case SpvOpPtrCastToGeneric:
   case SpvOpGenericCastToPtr:
   case SpvOpBitcast:
   case SpvOpIsNan:
   case SpvOpIsInf:
   case SpvOpIsFinite:
   case SpvOpIsNormal:
   case SpvOpSignBitSet:
   case SpvOpLessOrGreater:
   case SpvOpOrdered:
   case SpvOpUnordered:
   case SpvOpIAdd:
   case SpvOpFAdd:
   case SpvOpISub:
   case SpvOpFSub:
   case SpvOpIMul:
   case SpvOpFMul:
   case SpvOpUDiv:
   case SpvOpSDiv:
   case SpvOpFDiv:
   case SpvOpUMod:
   case SpvOpSRem:
   case SpvOpSMod:
   case SpvOpFRem:
   case SpvOpFMod:
   case SpvOpVectorTimesScalar:
   case SpvOpDot:
   case SpvOpIAddCarry:
   case SpvOpISubBorrow:
   case SpvOpUMulExtended:
   case SpvOpSMulExtended:
   case SpvOpShiftRightLogical:
   case SpvOpShiftRightArithmetic:
   case SpvOpShiftLeftLogical:
   case SpvOpLogicalEqual:
   case SpvOpLogicalNotEqual:
   case SpvOpLogicalOr:
   case SpvOpLogicalAnd:
   case SpvOpLogicalNot:
   case SpvOpBitwiseOr:
   case SpvOpBitwiseXor:
   case SpvOpBitwiseAnd:
   case SpvOpIEqual:
   case SpvOpFOrdEqual:
   case SpvOpFUnordEqual:
   case SpvOpINotEqual:
   case SpvOpFOrdNotEqual:
   case SpvOpFUnordNotEqual:
   case SpvOpULessThan:
   case SpvOpSLessThan:
   case SpvOpFOrdLessThan:
   case SpvOpFUnordLessThan:
   case SpvOpUGreaterThan:
   case SpvOpSGreaterThan:
   case SpvOpFOrdGreaterThan:
   case SpvOpFUnordGreaterThan:
   case SpvOpULessThanEqual:
   case SpvOpSLessThanEqual:
   case SpvOpFOrdLessThanEqual:
   case SpvOpFUnordLessThanEqual:
   case SpvOpUGreaterThanEqual:
   case SpvOpSGreaterThanEqual:
   case SpvOpFOrdGreaterThanEqual:
   case SpvOpFUnordGreaterThanEqual:
   case SpvOpDPdx:
   case SpvOpDPdy:
   case SpvOpFwidth:
   case SpvOpDPdxFine:
   case SpvOpDPdyFine:
   case SpvOpFwidthFine:
   case SpvOpDPdxCoarse:
   case SpvOpDPdyCoarse:
   case SpvOpFwidthCoarse:
   case SpvOpBitFieldInsert:
   case SpvOpBitFieldSExtract:
   case SpvOpBitFieldUExtract:
   case SpvOpBitReverse:
   case SpvOpBitCount:
   case SpvOpTranspose:
   case SpvOpOuterProduct:
   case SpvOpMatrixTimesScalar:
   case SpvOpVectorTimesMatrix:
   case SpvOpMatrixTimesVector:
   case SpvOpMatrixTimesMatrix:
      vtn_handle_alu(b, opcode, w, count);
      break;

   case SpvOpVectorExtractDynamic:
   case SpvOpVectorInsertDynamic:
   case SpvOpVectorShuffle:
   case SpvOpCompositeConstruct:
   case SpvOpCompositeExtract:
   case SpvOpCompositeInsert:
   case SpvOpCopyObject:
      vtn_handle_composite(b, opcode, w, count);
      break;

   case SpvOpEmitVertex:
   case SpvOpEndPrimitive:
   case SpvOpEmitStreamVertex:
   case SpvOpEndStreamPrimitive:
   case SpvOpControlBarrier:
   case SpvOpMemoryBarrier:
      vtn_handle_barrier(b, opcode, w, count);
      break;

   default:
      unreachable("Unhandled opcode");
   }

   return true;
}

nir_function *
spirv_to_nir(const uint32_t *words, size_t word_count,
             struct nir_spirv_specialization *spec, unsigned num_spec,
             gl_shader_stage stage, const char *entry_point_name,
             const struct nir_spirv_supported_extensions *ext,
             const nir_shader_compiler_options *options)
{
   const uint32_t *word_end = words + word_count;

   /* Handle the SPIR-V header (first 4 dwords)  */
   assert(word_count > 5);

   assert(words[0] == SpvMagicNumber);
   assert(words[1] >= 0x10000);
   /* words[2] == generator magic */
   unsigned value_id_bound = words[3];
   assert(words[4] == 0);

   words+= 5;

   /* Initialize the stn_builder object */
   struct vtn_builder *b = rzalloc(NULL, struct vtn_builder);
   b->value_id_bound = value_id_bound;
   b->values = rzalloc_array(b, struct vtn_value, value_id_bound);
   exec_list_make_empty(&b->functions);
   b->entry_point_stage = stage;
   b->entry_point_name = entry_point_name;
   b->ext = ext;

   /* Handle all the preamble instructions */
   words = vtn_foreach_instruction(b, words, word_end,
                                   vtn_handle_preamble_instruction);

   if (b->entry_point == NULL) {
      assert(!"Entry point not found");
      ralloc_free(b);
      return NULL;
   }

   b->shader = nir_shader_create(NULL, stage, options, NULL);

   /* Set shader info defaults */
   b->shader->info.gs.invocations = 1;

   /* Parse execution modes */
   vtn_foreach_execution_mode(b, b->entry_point,
                              vtn_handle_execution_mode, NULL);

   b->specializations = spec;
   b->num_specializations = num_spec;

   /* Handle all variable, type, and constant instructions */
   words = vtn_foreach_instruction(b, words, word_end,
                                   vtn_handle_variable_or_type_instruction);

   vtn_build_cfg(b, words, word_end);

   foreach_list_typed(struct vtn_function, func, node, &b->functions) {
      b->impl = func->impl;
      b->const_table = _mesa_hash_table_create(b, _mesa_hash_pointer,
                                               _mesa_key_pointer_equal);

      vtn_function_emit(b, func, vtn_handle_body_instruction);
   }

   assert(b->entry_point->value_type == vtn_value_type_function);
   nir_function *entry_point = b->entry_point->func->impl->function;
   assert(entry_point);

   ralloc_free(b);

   return entry_point;
}
