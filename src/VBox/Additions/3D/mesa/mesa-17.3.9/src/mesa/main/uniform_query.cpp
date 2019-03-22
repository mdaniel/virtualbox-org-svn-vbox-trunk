/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2004-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009-2010  VMware, Inc.  All Rights Reserved.
 * Copyright © 2010, 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <inttypes.h>  /* for PRIx64 macro */

#include "main/core.h"
#include "main/context.h"
#include "main/shaderapi.h"
#include "main/shaderobj.h"
#include "main/uniforms.h"
#include "compiler/glsl/ir.h"
#include "compiler/glsl/ir_uniform.h"
#include "compiler/glsl/glsl_parser_extras.h"
#include "compiler/glsl/program.h"
#include "util/bitscan.h"


extern "C" void GLAPIENTRY
_mesa_GetActiveUniform(GLuint program, GLuint index,
                       GLsizei maxLength, GLsizei *length, GLint *size,
                       GLenum *type, GLcharARB *nameOut)
{
   GET_CURRENT_CONTEXT(ctx);
   struct gl_shader_program *shProg;
   struct gl_program_resource *res;

   if (maxLength < 0) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetActiveUniform(maxLength < 0)");
      return;
   }

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glGetActiveUniform");
   if (!shProg)
      return;

   res = _mesa_program_resource_find_index((struct gl_shader_program *) shProg,
                                           GL_UNIFORM, index);

   if (!res) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetActiveUniform(index)");
      return;
   }

   if (nameOut)
      _mesa_get_program_resource_name(shProg, GL_UNIFORM, index, maxLength,
                                      length, nameOut, "glGetActiveUniform");
   if (type)
      _mesa_program_resource_prop((struct gl_shader_program *) shProg,
                                  res, index, GL_TYPE, (GLint*) type,
                                  "glGetActiveUniform");
   if (size)
      _mesa_program_resource_prop((struct gl_shader_program *) shProg,
                                  res, index, GL_ARRAY_SIZE, (GLint*) size,
                                  "glGetActiveUniform");
}

static GLenum
resource_prop_from_uniform_prop(GLenum uni_prop)
{
   switch (uni_prop) {
   case GL_UNIFORM_TYPE:
      return GL_TYPE;
   case GL_UNIFORM_SIZE:
      return GL_ARRAY_SIZE;
   case GL_UNIFORM_NAME_LENGTH:
      return GL_NAME_LENGTH;
   case GL_UNIFORM_BLOCK_INDEX:
      return GL_BLOCK_INDEX;
   case GL_UNIFORM_OFFSET:
      return GL_OFFSET;
   case GL_UNIFORM_ARRAY_STRIDE:
      return GL_ARRAY_STRIDE;
   case GL_UNIFORM_MATRIX_STRIDE:
      return GL_MATRIX_STRIDE;
   case GL_UNIFORM_IS_ROW_MAJOR:
      return GL_IS_ROW_MAJOR;
   case GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX:
      return GL_ATOMIC_COUNTER_BUFFER_INDEX;
   default:
      return 0;
   }
}

extern "C" void GLAPIENTRY
_mesa_GetActiveUniformsiv(GLuint program,
			  GLsizei uniformCount,
			  const GLuint *uniformIndices,
			  GLenum pname,
			  GLint *params)
{
   GET_CURRENT_CONTEXT(ctx);
   struct gl_shader_program *shProg;
   struct gl_program_resource *res;
   GLenum res_prop;

   if (uniformCount < 0) {
      _mesa_error(ctx, GL_INVALID_VALUE,
		  "glGetActiveUniformsiv(uniformCount < 0)");
      return;
   }

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glGetActiveUniform");
   if (!shProg)
      return;

   res_prop = resource_prop_from_uniform_prop(pname);

   /* We need to first verify that each entry exists as active uniform. If
    * not, generate error and do not cause any other side effects.
    *
    * In the case of and error condition, Page 16 (section 2.3.1 Errors)
    * of the OpenGL 4.5 spec says:
    *
    *     "If the generating command modifies values through a pointer argu-
    *     ment, no change is made to these values."
    */
   for (int i = 0; i < uniformCount; i++) {
      if (!_mesa_program_resource_find_index(shProg, GL_UNIFORM,
                                              uniformIndices[i])) {
         _mesa_error(ctx, GL_INVALID_VALUE, "glGetActiveUniformsiv(index)");
         return;
      }
   }

   for (int i = 0; i < uniformCount; i++) {
      res = _mesa_program_resource_find_index(shProg, GL_UNIFORM,
                                              uniformIndices[i]);
      if (!_mesa_program_resource_prop(shProg, res, uniformIndices[i],
                                       res_prop, &params[i],
                                       "glGetActiveUniformsiv"))
         break;
   }
}

static struct gl_uniform_storage *
validate_uniform_parameters(GLint location, GLsizei count,
                            unsigned *array_index,
                            struct gl_context *ctx,
                            struct gl_shader_program *shProg,
                            const char *caller)
{
   if (shProg == NULL) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "%s(program not linked)", caller);
      return NULL;
   }

   /* From page 12 (page 26 of the PDF) of the OpenGL 2.1 spec:
    *
    *     "If a negative number is provided where an argument of type sizei or
    *     sizeiptr is specified, the error INVALID_VALUE is generated."
    */
   if (count < 0) {
      _mesa_error(ctx, GL_INVALID_VALUE, "%s(count < 0)", caller);
      return NULL;
   }

   /* Check that the given location is in bounds of uniform remap table.
    * Unlinked programs will have NumUniformRemapTable == 0, so we can take
    * the shProg->data->LinkStatus check out of the main path.
    */
   if (unlikely(location >= (GLint) shProg->NumUniformRemapTable)) {
      if (!shProg->data->LinkStatus)
         _mesa_error(ctx, GL_INVALID_OPERATION, "%s(program not linked)",
                     caller);
      else
         _mesa_error(ctx, GL_INVALID_OPERATION, "%s(location=%d)",
                     caller, location);

      return NULL;
   }

   if (location == -1) {
      if (!shProg->data->LinkStatus)
         _mesa_error(ctx, GL_INVALID_OPERATION, "%s(program not linked)",
                     caller);

      return NULL;
   }

   /* Page 82 (page 96 of the PDF) of the OpenGL 2.1 spec says:
    *
    *     "If any of the following conditions occur, an INVALID_OPERATION
    *     error is generated by the Uniform* commands, and no uniform values
    *     are changed:
    *
    *     ...
    *
    *         - if no variable with a location of location exists in the
    *           program object currently in use and location is not -1,
    *         - if count is greater than one, and the uniform declared in the
    *           shader is not an array variable,
    */
   if (location < -1 || !shProg->UniformRemapTable[location]) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "%s(location=%d)",
                  caller, location);
      return NULL;
   }

   /* If the driver storage pointer in remap table is -1, we ignore silently.
    *
    * GL_ARB_explicit_uniform_location spec says:
    *     "What happens if Uniform* is called with an explicitly defined
    *     uniform location, but that uniform is deemed inactive by the
    *     linker?
    *
    *     RESOLVED: The call is ignored for inactive uniform variables and
    *     no error is generated."
    *
    */
   if (shProg->UniformRemapTable[location] ==
       INACTIVE_UNIFORM_EXPLICIT_LOCATION)
      return NULL;

   struct gl_uniform_storage *const uni = shProg->UniformRemapTable[location];

   /* Even though no location is assigned to a built-in uniform and this
    * function should already have returned NULL, this test makes it explicit
    * that we are not allowing to update the value of a built-in.
    */
   if (uni->builtin)
      return NULL;

   if (uni->array_elements == 0) {
      if (count > 1) {
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "%s(count = %u for non-array \"%s\"@%d)",
                     caller, count, uni->name, location);
         return NULL;
      }

      assert((location - uni->remap_location) == 0);
      *array_index = 0;
   } else {
      /* The array index specified by the uniform location is just the uniform
       * location minus the base location of of the uniform.
       */
      *array_index = location - uni->remap_location;

      /* If the uniform is an array, check that array_index is in bounds.
       * array_index is unsigned so no need to check for less than zero.
       */
      if (*array_index >= uni->array_elements) {
         _mesa_error(ctx, GL_INVALID_OPERATION, "%s(location=%d)",
                     caller, location);
         return NULL;
      }
   }
   return uni;
}

/**
 * Called via glGetUniform[fiui]v() to get the current value of a uniform.
 */
extern "C" void
_mesa_get_uniform(struct gl_context *ctx, GLuint program, GLint location,
		  GLsizei bufSize, enum glsl_base_type returnType,
		  GLvoid *paramsOut)
{
   struct gl_shader_program *shProg =
      _mesa_lookup_shader_program_err(ctx, program, "glGetUniformfv");
   unsigned offset;

   struct gl_uniform_storage *const uni =
      validate_uniform_parameters(location, 1, &offset,
                                  ctx, shProg, "glGetUniform");
   if (uni == NULL) {
      /* For glGetUniform, page 264 (page 278 of the PDF) of the OpenGL 2.1
       * spec says:
       *
       *     "The error INVALID_OPERATION is generated if program has not been
       *     linked successfully, or if location is not a valid location for
       *     program."
       *
       * For glUniform, page 82 (page 96 of the PDF) of the OpenGL 2.1 spec
       * says:
       *
       *     "If the value of location is -1, the Uniform* commands will
       *     silently ignore the data passed in, and the current uniform
       *     values will not be changed."
       *
       * Allowing -1 for the location parameter of glUniform allows
       * applications to avoid error paths in the case that, for example, some
       * uniform variable is removed by the compiler / linker after
       * optimization.  In this case, the new value of the uniform is dropped
       * on the floor.  For the case of glGetUniform, there is nothing
       * sensible to do for a location of -1.
       *
       * If the location was -1, validate_unfirom_parameters will return NULL
       * without raising an error.  Raise the error here.
       */
      if (location == -1) {
         _mesa_error(ctx, GL_INVALID_OPERATION, "glGetUniform(location=%d)",
                     location);
      }

      return;
   }

   {
      unsigned elements = uni->type->components();
      const int rmul = glsl_base_type_is_64bit(returnType) ? 2 : 1;
      int dmul = (uni->type->is_64bit()) ? 2 : 1;

      if ((uni->type->is_sampler() || uni->type->is_image()) &&
          !uni->is_bindless) {
         /* Non-bindless samplers/images are represented using unsigned integer
          * 32-bit, while bindless handles are 64-bit.
          */
         dmul = 1;
      }

      /* Calculate the source base address *BEFORE* modifying elements to
       * account for the size of the user's buffer.
       */
      const union gl_constant_value *const src =
         &uni->storage[offset * elements * dmul];

      assert(returnType == GLSL_TYPE_FLOAT || returnType == GLSL_TYPE_INT ||
             returnType == GLSL_TYPE_UINT || returnType == GLSL_TYPE_DOUBLE ||
             returnType == GLSL_TYPE_UINT64 || returnType == GLSL_TYPE_INT64);

      /* doubles have a different size than the other 3 types */
      unsigned bytes = sizeof(src[0]) * elements * rmul;
      if (bufSize < 0 || bytes > (unsigned) bufSize) {
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "glGetnUniform*vARB(out of bounds: bufSize is %d,"
                     " but %u bytes are required)", bufSize, bytes);
         return;
      }

      /* If the return type and the uniform's native type are "compatible,"
       * just memcpy the data.  If the types are not compatible, perform a
       * slower convert-and-copy process.
       */
      if (returnType == uni->type->base_type ||
          ((returnType == GLSL_TYPE_INT || returnType == GLSL_TYPE_UINT) &&
           (uni->type->is_sampler() || uni->type->is_image())) ||
          (returnType == GLSL_TYPE_UINT64 && uni->is_bindless)) {
         memcpy(paramsOut, src, bytes);
      } else {
         union gl_constant_value *const dst =
            (union gl_constant_value *) paramsOut;
         /* This code could be optimized by putting the loop inside the switch
          * statements.  However, this is not expected to be
          * performance-critical code.
          */
         for (unsigned i = 0; i < elements; i++) {
            int sidx = i * dmul;
            int didx = i * rmul;

            switch (returnType) {
            case GLSL_TYPE_FLOAT:
               switch (uni->type->base_type) {
               case GLSL_TYPE_UINT:
                  dst[didx].f = (float) src[sidx].u;
                  break;
               case GLSL_TYPE_INT:
               case GLSL_TYPE_SAMPLER:
               case GLSL_TYPE_IMAGE:
                  dst[didx].f = (float) src[sidx].i;
                  break;
               case GLSL_TYPE_BOOL:
                  dst[didx].f = src[sidx].i ? 1.0f : 0.0f;
                  break;
               case GLSL_TYPE_DOUBLE: {
                  double tmp;
                  memcpy(&tmp, &src[sidx].f, sizeof(tmp));
                  dst[didx].f = tmp;
                  break;
               }
               case GLSL_TYPE_UINT64: {
                  uint64_t tmp;
                  memcpy(&tmp, &src[sidx].u, sizeof(tmp));
                  dst[didx].f = tmp;
                  break;
                }
               case GLSL_TYPE_INT64: {
                  uint64_t tmp;
                  memcpy(&tmp, &src[sidx].i, sizeof(tmp));
                  dst[didx].f = tmp;
                  break;
               }
               default:
                  assert(!"Should not get here.");
                  break;
               }
               break;

            case GLSL_TYPE_DOUBLE:
               switch (uni->type->base_type) {
               case GLSL_TYPE_UINT: {
                  double tmp = src[sidx].u;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_INT:
               case GLSL_TYPE_SAMPLER:
               case GLSL_TYPE_IMAGE: {
                  double tmp = src[sidx].i;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_BOOL: {
                  double tmp = src[sidx].i ? 1.0 : 0.0;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_FLOAT: {
                  double tmp = src[sidx].f;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_UINT64: {
                  uint64_t tmpu;
                  double tmp;
                  memcpy(&tmpu, &src[sidx].u, sizeof(tmpu));
                  tmp = tmpu;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_INT64: {
                  int64_t tmpi;
                  double tmp;
                  memcpy(&tmpi, &src[sidx].i, sizeof(tmpi));
                  tmp = tmpi;
                  memcpy(&dst[didx].f, &tmp, sizeof(tmp));
                  break;
               }
               default:
                  assert(!"Should not get here.");
                  break;
               }
               break;

            case GLSL_TYPE_INT:
               switch (uni->type->base_type) {
               case GLSL_TYPE_FLOAT:
                  /* While the GL 3.2 core spec doesn't explicitly
                   * state how conversion of float uniforms to integer
                   * values works, in section 6.2 "State Tables" on
                   * page 267 it says:
                   *
                   *     "Unless otherwise specified, when floating
                   *      point state is returned as integer values or
                   *      integer state is returned as floating-point
                   *      values it is converted in the fashion
                   *      described in section 6.1.2"
                   *
                   * That section, on page 248, says:
                   *
                   *     "If GetIntegerv or GetInteger64v are called,
                   *      a floating-point value is rounded to the
                   *      nearest integer..."
                   */
                  dst[didx].i = (int64_t) roundf(src[sidx].f);
                  break;
               case GLSL_TYPE_BOOL:
                  dst[didx].i = src[sidx].i ? 1 : 0;
                  break;
               case GLSL_TYPE_UINT:
                  dst[didx].i = MIN2(src[sidx].i, INT_MAX);
                  break;
               case GLSL_TYPE_DOUBLE: {
                  double tmp;
                  memcpy(&tmp, &src[sidx].f, sizeof(tmp));
                  dst[didx].i = (int64_t) round(tmp);
                  break;
               }
               case GLSL_TYPE_UINT64: {
                  uint64_t tmp;
                  memcpy(&tmp, &src[sidx].u, sizeof(tmp));
                  dst[didx].i = tmp;
                  break;
               }
               case GLSL_TYPE_INT64: {
                  int64_t tmp;
                  memcpy(&tmp, &src[sidx].i, sizeof(tmp));
                  dst[didx].i = tmp;
                  break;
               }
               default:
                  assert(!"Should not get here.");
                  break;
               }
               break;

            case GLSL_TYPE_UINT:
               switch (uni->type->base_type) {
               case GLSL_TYPE_FLOAT:
                  /* The spec isn't terribly clear how to handle negative
                   * values with an unsigned return type.
                   *
                   * GL 4.5 section 2.2.2 ("Data Conversions for State
                   * Query Commands") says:
                   *
                   * "If a value is so large in magnitude that it cannot be
                   *  represented by the returned data type, then the nearest
                   *  value representable using the requested type is
                   *  returned."
                   */
                  dst[didx].u = src[sidx].f < 0.0f ?
                     0u : (uint32_t) roundf(src[sidx].f);
                  break;
               case GLSL_TYPE_BOOL:
                  dst[didx].i = src[sidx].i ? 1 : 0;
                  break;
               case GLSL_TYPE_INT:
                  dst[didx].i = MAX2(src[sidx].i, 0);
                  break;
               case GLSL_TYPE_DOUBLE: {
                  double tmp;
                  memcpy(&tmp, &src[sidx].f, sizeof(tmp));
                  dst[didx].u = tmp < 0.0 ? 0u : (uint32_t) round(tmp);
                  break;
               }
               case GLSL_TYPE_UINT64: {
                  uint64_t tmp;
                  memcpy(&tmp, &src[sidx].u, sizeof(tmp));
                  dst[didx].i = MIN2(tmp, INT_MAX);
                  break;
               }
               case GLSL_TYPE_INT64: {
                  int64_t tmp;
                  memcpy(&tmp, &src[sidx].i, sizeof(tmp));
                  dst[didx].i = MAX2(tmp, 0);
                  break;
               }
               default:
                  unreachable("invalid uniform type");
               }
               break;

            case GLSL_TYPE_INT64:
               switch (uni->type->base_type) {
               case GLSL_TYPE_UINT: {
                  uint64_t tmp = src[sidx].u;
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_INT:
               case GLSL_TYPE_SAMPLER:
               case GLSL_TYPE_IMAGE: {
                  int64_t tmp = src[sidx].i;
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_BOOL: {
                  int64_t tmp = src[sidx].i ? 1.0f : 0.0f;
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_UINT64: {
                  uint64_t u64;
                  memcpy(&u64, &src[sidx].u, sizeof(u64));
                  int64_t tmp = MIN2(u64, INT_MAX);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_FLOAT: {
                  int64_t tmp = (int64_t) roundf(src[sidx].f);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_DOUBLE: {
                  double d;
                  memcpy(&d, &src[sidx].f, sizeof(d));
                  int64_t tmp = (int64_t) round(d);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               default:
                  assert(!"Should not get here.");
                  break;
               }
               break;

            case GLSL_TYPE_UINT64:
               switch (uni->type->base_type) {
               case GLSL_TYPE_UINT: {
                  uint64_t tmp = src[sidx].u;
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_INT:
               case GLSL_TYPE_SAMPLER:
               case GLSL_TYPE_IMAGE: {
                  int64_t tmp = MAX2(src[sidx].i, 0);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_BOOL: {
                  int64_t tmp = src[sidx].i ? 1.0f : 0.0f;
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_INT64: {
                  uint64_t i64;
                  memcpy(&i64, &src[sidx].i, sizeof(i64));
                  uint64_t tmp = MAX2(i64, 0);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_FLOAT: {
                  uint64_t tmp = src[sidx].f < 0.0f ?
                     0ull : (uint64_t) roundf(src[sidx].f);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               case GLSL_TYPE_DOUBLE: {
                  double d;
                  memcpy(&d, &src[sidx].f, sizeof(d));
                  uint64_t tmp = (d < 0.0) ? 0ull : (uint64_t) round(d);
                  memcpy(&dst[didx].u, &tmp, sizeof(tmp));
                  break;
               }
               default:
                  assert(!"Should not get here.");
                  break;
               }
               break;

            default:
               assert(!"Should not get here.");
               break;
            }
         }
      }
   }
}

static void
log_uniform(const void *values, enum glsl_base_type basicType,
	    unsigned rows, unsigned cols, unsigned count,
	    bool transpose,
	    const struct gl_shader_program *shProg,
	    GLint location,
	    const struct gl_uniform_storage *uni)
{

   const union gl_constant_value *v = (const union gl_constant_value *) values;
   const unsigned elems = rows * cols * count;
   const char *const extra = (cols == 1) ? "uniform" : "uniform matrix";

   printf("Mesa: set program %u %s \"%s\" (loc %d, type \"%s\", "
	  "transpose = %s) to: ",
	  shProg->Name, extra, uni->name, location, uni->type->name,
	  transpose ? "true" : "false");
   for (unsigned i = 0; i < elems; i++) {
      if (i != 0 && ((i % rows) == 0))
	 printf(", ");

      switch (basicType) {
      case GLSL_TYPE_UINT:
	 printf("%u ", v[i].u);
	 break;
      case GLSL_TYPE_INT:
	 printf("%d ", v[i].i);
	 break;
      case GLSL_TYPE_UINT64: {
         uint64_t tmp;
         memcpy(&tmp, &v[i * 2].u, sizeof(tmp));
         printf("%" PRIu64 " ", tmp);
         break;
      }
      case GLSL_TYPE_INT64: {
         int64_t tmp;
         memcpy(&tmp, &v[i * 2].u, sizeof(tmp));
         printf("%" PRId64 " ", tmp);
         break;
      }
      case GLSL_TYPE_FLOAT:
	 printf("%g ", v[i].f);
	 break;
      case GLSL_TYPE_DOUBLE: {
         double tmp;
         memcpy(&tmp, &v[i * 2].f, sizeof(tmp));
         printf("%g ", tmp);
         break;
      }
      default:
	 assert(!"Should not get here.");
	 break;
      }
   }
   printf("\n");
   fflush(stdout);
}

#if 0
static void
log_program_parameters(const struct gl_shader_program *shProg)
{
   for (unsigned i = 0; i < MESA_SHADER_STAGES; i++) {
      if (shProg->_LinkedShaders[i] == NULL)
	 continue;

      const struct gl_program *const prog = shProg->_LinkedShaders[i]->Program;

      printf("Program %d %s shader parameters:\n",
             shProg->Name, _mesa_shader_stage_to_string(i));
      for (unsigned j = 0; j < prog->Parameters->NumParameters; j++) {
	 printf("%s: %p %f %f %f %f\n",
		prog->Parameters->Parameters[j].Name,
		prog->Parameters->ParameterValues[j],
		prog->Parameters->ParameterValues[j][0].f,
		prog->Parameters->ParameterValues[j][1].f,
		prog->Parameters->ParameterValues[j][2].f,
		prog->Parameters->ParameterValues[j][3].f);
      }
   }
   fflush(stdout);
}
#endif

/**
 * Propagate some values from uniform backing storage to driver storage
 *
 * Values propagated from uniform backing storage to driver storage
 * have all format / type conversions previously requested by the
 * driver applied.  This function is most often called by the
 * implementations of \c glUniform1f, etc. and \c glUniformMatrix2f,
 * etc.
 *
 * \param uni          Uniform whose data is to be propagated to driver storage
 * \param array_index  If \c uni is an array, this is the element of
 *                     the array to be propagated.
 * \param count        Number of array elements to propagate.
 */
extern "C" void
_mesa_propagate_uniforms_to_driver_storage(struct gl_uniform_storage *uni,
					   unsigned array_index,
					   unsigned count)
{
   unsigned i;

   const unsigned components = uni->type->vector_elements;
   const unsigned vectors = uni->type->matrix_columns;
   const int dmul = uni->type->is_64bit() ? 2 : 1;

   /* Store the data in the driver's requested type in the driver's storage
    * areas.
    */
   unsigned src_vector_byte_stride = components * 4 * dmul;

   for (i = 0; i < uni->num_driver_storage; i++) {
      struct gl_uniform_driver_storage *const store = &uni->driver_storage[i];
      uint8_t *dst = (uint8_t *) store->data;
      const unsigned extra_stride =
	 store->element_stride - (vectors * store->vector_stride);
      const uint8_t *src =
	 (uint8_t *) (&uni->storage[array_index * (dmul * components * vectors)].i);

#if 0
      printf("%s: %p[%d] components=%u vectors=%u count=%u vector_stride=%u "
	     "extra_stride=%u\n",
	     __func__, dst, array_index, components,
	     vectors, count, store->vector_stride, extra_stride);
#endif

      dst += array_index * store->element_stride;

      switch (store->format) {
      case uniform_native: {
	 unsigned j;
	 unsigned v;

	 if (src_vector_byte_stride == store->vector_stride) {
	    if (extra_stride) {
	       for (j = 0; j < count; j++) {
	          memcpy(dst, src, src_vector_byte_stride * vectors);
	          src += src_vector_byte_stride * vectors;
	          dst += store->vector_stride * vectors;

	          dst += extra_stride;
	       }
	    } else {
	       /* Unigine Heaven benchmark gets here */
	       memcpy(dst, src, src_vector_byte_stride * vectors * count);
	       src += src_vector_byte_stride * vectors * count;
	       dst += store->vector_stride * vectors * count;
	    }
	 } else {
	    for (j = 0; j < count; j++) {
	       for (v = 0; v < vectors; v++) {
	          memcpy(dst, src, src_vector_byte_stride);
	          src += src_vector_byte_stride;
	          dst += store->vector_stride;
	       }

	       dst += extra_stride;
	    }
	 }
	 break;
      }

      case uniform_int_float: {
	 const int *isrc = (const int *) src;
	 unsigned j;
	 unsigned v;
	 unsigned c;

	 for (j = 0; j < count; j++) {
	    for (v = 0; v < vectors; v++) {
	       for (c = 0; c < components; c++) {
		  ((float *) dst)[c] = (float) *isrc;
		  isrc++;
	       }

	       dst += store->vector_stride;
	    }

	    dst += extra_stride;
	 }
	 break;
      }

      default:
	 assert(!"Should not get here.");
	 break;
      }
   }
}


/**
 * Return printable string for a given GLSL_TYPE_x
 */
static const char *
glsl_type_name(enum glsl_base_type type)
{
   switch (type) {
   case GLSL_TYPE_UINT:
      return "uint";
   case GLSL_TYPE_INT:
      return "int";
   case GLSL_TYPE_FLOAT:
      return "float";
   case GLSL_TYPE_DOUBLE:
      return "double";
   case GLSL_TYPE_UINT64:
      return "uint64";
   case GLSL_TYPE_INT64:
      return "int64";
   case GLSL_TYPE_BOOL:
      return "bool";
   case GLSL_TYPE_SAMPLER:
      return "sampler";
   case GLSL_TYPE_IMAGE:
      return "image";
   case GLSL_TYPE_ATOMIC_UINT:
      return "atomic_uint";
   case GLSL_TYPE_STRUCT:
      return "struct";
   case GLSL_TYPE_INTERFACE:
      return "interface";
   case GLSL_TYPE_ARRAY:
      return "array";
   case GLSL_TYPE_VOID:
      return "void";
   case GLSL_TYPE_ERROR:
      return "error";
   default:
      return "other";
   }
}


static struct gl_uniform_storage *
validate_uniform(GLint location, GLsizei count, const GLvoid *values,
                 unsigned *offset, struct gl_context *ctx,
                 struct gl_shader_program *shProg,
                 enum glsl_base_type basicType, unsigned src_components)
{
   struct gl_uniform_storage *const uni =
      validate_uniform_parameters(location, count, offset,
                                  ctx, shProg, "glUniform");
   if (uni == NULL)
      return NULL;

   if (uni->type->is_matrix()) {
      /* Can't set matrix uniforms (like mat4) with glUniform */
      _mesa_error(ctx, GL_INVALID_OPERATION,
                  "glUniform%u(uniform \"%s\"@%d is matrix)",
                  src_components, uni->name, location);
      return NULL;
   }

   /* Verify that the types are compatible. */
   const unsigned components = uni->type->vector_elements;

   if (components != src_components) {
      /* glUniformN() must match float/vecN type */
      _mesa_error(ctx, GL_INVALID_OPERATION,
                  "glUniform%u(\"%s\"@%u has %u components, not %u)",
                  src_components, uni->name, location,
                  components, src_components);
      return NULL;
   }

   bool match;
   switch (uni->type->base_type) {
   case GLSL_TYPE_BOOL:
      match = (basicType != GLSL_TYPE_DOUBLE);
      break;
   case GLSL_TYPE_SAMPLER:
      match = (basicType == GLSL_TYPE_INT);
      break;
   case GLSL_TYPE_IMAGE:
      match = (basicType == GLSL_TYPE_INT && _mesa_is_desktop_gl(ctx));
      break;
   default:
      match = (basicType == uni->type->base_type);
      break;
   }

   if (!match) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
                  "glUniform%u(\"%s\"@%d is %s, not %s)",
                  src_components, uni->name, location,
                  glsl_type_name(uni->type->base_type),
                  glsl_type_name(basicType));
      return NULL;
   }

   if (unlikely(ctx->_Shader->Flags & GLSL_UNIFORMS)) {
      log_uniform(values, basicType, components, 1, count,
                  false, shProg, location, uni);
   }

   /* Page 100 (page 116 of the PDF) of the OpenGL 3.0 spec says:
    *
    *     "Setting a sampler's value to i selects texture image unit number
    *     i. The values of i range from zero to the implementation- dependent
    *     maximum supported number of texture image units."
    *
    * In addition, table 2.3, "Summary of GL errors," on page 17 (page 33 of
    * the PDF) says:
    *
    *     "Error         Description                    Offending command
    *                                                   ignored?
    *     ...
    *     INVALID_VALUE  Numeric argument out of range  Yes"
    *
    * Based on that, when an invalid sampler is specified, we generate a
    * GL_INVALID_VALUE error and ignore the command.
    */
   if (uni->type->is_sampler()) {
      for (int i = 0; i < count; i++) {
         const unsigned texUnit = ((unsigned *) values)[i];

         /* check that the sampler (tex unit index) is legal */
         if (texUnit >= ctx->Const.MaxCombinedTextureImageUnits) {
            _mesa_error(ctx, GL_INVALID_VALUE,
                        "glUniform1i(invalid sampler/tex unit index for "
                        "uniform %d)", location);
            return NULL;
         }
      }
      /* We need to reset the validate flag on changes to samplers in case
       * two different sampler types are set to the same texture unit.
       */
      ctx->_Shader->Validated = GL_FALSE;
   }

   if (uni->type->is_image()) {
      for (int i = 0; i < count; i++) {
         const int unit = ((GLint *) values)[i];

         /* check that the image unit is legal */
         if (unit < 0 || unit >= (int)ctx->Const.MaxImageUnits) {
            _mesa_error(ctx, GL_INVALID_VALUE,
                        "glUniform1i(invalid image unit index for uniform %d)",
                        location);
            return NULL;
         }
      }
   }

   return uni;
}

void
_mesa_flush_vertices_for_uniforms(struct gl_context *ctx,
                                  const struct gl_uniform_storage *uni)
{
   /* Opaque uniforms have no storage unless they are bindless */
   if (!uni->is_bindless && uni->type->contains_opaque()) {
      FLUSH_VERTICES(ctx, 0);
      return;
   }

   uint64_t new_driver_state = 0;
   unsigned mask = uni->active_shader_mask;

   while (mask) {
      unsigned index = u_bit_scan(&mask);

      assert(index < MESA_SHADER_STAGES);
      new_driver_state |= ctx->DriverFlags.NewShaderConstants[index];
   }

   FLUSH_VERTICES(ctx, new_driver_state ? 0 : _NEW_PROGRAM_CONSTANTS);
   ctx->NewDriverState |= new_driver_state;
}

/**
 * Called via glUniform*() functions.
 */
extern "C" void
_mesa_uniform(GLint location, GLsizei count, const GLvoid *values,
              struct gl_context *ctx, struct gl_shader_program *shProg,
              enum glsl_base_type basicType, unsigned src_components)
{
   unsigned offset;
   int size_mul = glsl_base_type_is_64bit(basicType) ? 2 : 1;

   struct gl_uniform_storage *uni;
   if (_mesa_is_no_error_enabled(ctx)) {
      /* From Seciton 7.6 (UNIFORM VARIABLES) of the OpenGL 4.5 spec:
       *
       *   "If the value of location is -1, the Uniform* commands will
       *   silently ignore the data passed in, and the current uniform values
       *   will not be changed.
       */
      if (location == -1)
         return;

      uni = shProg->UniformRemapTable[location];

      /* The array index specified by the uniform location is just the
       * uniform location minus the base location of of the uniform.
       */
      assert(uni->array_elements > 0 || location == (int)uni->remap_location);
      offset = location - uni->remap_location;
   } else {
      uni = validate_uniform(location, count, values, &offset, ctx, shProg,
                             basicType, src_components);
      if (!uni)
         return;
   }

   const unsigned components = uni->type->vector_elements;

   /* Page 82 (page 96 of the PDF) of the OpenGL 2.1 spec says:
    *
    *     "When loading N elements starting at an arbitrary position k in a
    *     uniform declared as an array, elements k through k + N - 1 in the
    *     array will be replaced with the new values. Values for any array
    *     element that exceeds the highest array element index used, as
    *     reported by GetActiveUniform, will be ignored by the GL."
    *
    * Clamp 'count' to a valid value.  Note that for non-arrays a count > 1
    * will have already generated an error.
    */
   if (uni->array_elements != 0) {
      count = MIN2(count, (int) (uni->array_elements - offset));
   }

   /* We check samplers for changes and flush if needed in the sampler
    * handling code further down, so just skip them here.
    */
   if (!uni->type->is_sampler()) {
       _mesa_flush_vertices_for_uniforms(ctx, uni);
   }

   /* Store the data in the "actual type" backing storage for the uniform.
    */
   if (!uni->type->is_boolean() && !uni->is_bindless) {
      memcpy(&uni->storage[size_mul * components * offset], values,
             sizeof(uni->storage[0]) * components * count * size_mul);
   } else if (uni->is_bindless) {
      const union gl_constant_value *src =
         (const union gl_constant_value *) values;
      GLuint64 *dst = (GLuint64 *)&uni->storage[components * offset].i;
      const unsigned elems = components * count;

      for (unsigned i = 0; i < elems; i++) {
         dst[i] = src[i].i;
      }
   } else {
      const union gl_constant_value *src =
         (const union gl_constant_value *) values;
      union gl_constant_value *dst = &uni->storage[components * offset];
      const unsigned elems = components * count;

      for (unsigned i = 0; i < elems; i++) {
         if (basicType == GLSL_TYPE_FLOAT) {
            dst[i].i = src[i].f != 0.0f ? ctx->Const.UniformBooleanTrue : 0;
         } else {
            dst[i].i = src[i].i != 0    ? ctx->Const.UniformBooleanTrue : 0;
         }
      }
   }

   _mesa_propagate_uniforms_to_driver_storage(uni, offset, count);

   /* If the uniform is a sampler, do the extra magic necessary to propagate
    * the changes through.
    */
   if (uni->type->is_sampler()) {
      bool flushed = false;

      shProg->SamplersValidated = GL_TRUE;

      for (int i = 0; i < MESA_SHADER_STAGES; i++) {
         struct gl_linked_shader *const sh = shProg->_LinkedShaders[i];

         /* If the shader stage doesn't use the sampler uniform, skip this. */
         if (!uni->opaque[i].active)
            continue;

         bool changed = false;
         for (int j = 0; j < count; j++) {
            unsigned unit = uni->opaque[i].index + offset + j;
            unsigned value = ((unsigned *)values)[j];

            if (uni->is_bindless) {
               struct gl_bindless_sampler *sampler =
                  &sh->Program->sh.BindlessSamplers[unit];

               /* Mark this bindless sampler as bound to a texture unit.
                */
               if (sampler->unit != value || !sampler->bound) {
                  sampler->unit = value;
                  changed = true;
               }
               sampler->bound = true;
               sh->Program->sh.HasBoundBindlessSampler = true;
            } else {
               if (sh->Program->SamplerUnits[unit] != value) {
                  sh->Program->SamplerUnits[unit] = value;
                  changed = true;
               }
            }
         }

         if (changed) {
            if (!flushed) {
               FLUSH_VERTICES(ctx, _NEW_TEXTURE_OBJECT | _NEW_PROGRAM);
               flushed = true;
            }

            struct gl_program *const prog = sh->Program;
            _mesa_update_shader_textures_used(shProg, prog);
            if (ctx->Driver.SamplerUniformChange)
               ctx->Driver.SamplerUniformChange(ctx, prog->Target, prog);
         }
      }
   }

   /* If the uniform is an image, update the mapping from image
    * uniforms to image units present in the shader data structure.
    */
   if (uni->type->is_image()) {
      for (int i = 0; i < MESA_SHADER_STAGES; i++) {
         struct gl_linked_shader *sh = shProg->_LinkedShaders[i];

         /* If the shader stage doesn't use the image uniform, skip this. */
         if (!uni->opaque[i].active)
            continue;

         for (int j = 0; j < count; j++) {
            unsigned unit = uni->opaque[i].index + offset + j;
            unsigned value = ((unsigned *)values)[j];

            if (uni->is_bindless) {
               struct gl_bindless_image *image =
                  &sh->Program->sh.BindlessImages[unit];

               /* Mark this bindless image as bound to an image unit.
                */
               image->unit = value;
               image->bound = true;
               sh->Program->sh.HasBoundBindlessImage = true;
            } else {
               sh->Program->sh.ImageUnits[unit] = value;
            }
         }
      }

      ctx->NewDriverState |= ctx->DriverFlags.NewImageUnits;
   }
}

/**
 * Called by glUniformMatrix*() functions.
 * Note: cols=2, rows=4  ==>  array[2] of vec4
 */
extern "C" void
_mesa_uniform_matrix(GLint location, GLsizei count,
                     GLboolean transpose, const void *values,
                     struct gl_context *ctx, struct gl_shader_program *shProg,
                     GLuint cols, GLuint rows, enum glsl_base_type basicType)
{
   unsigned offset;
   struct gl_uniform_storage *const uni =
      validate_uniform_parameters(location, count, &offset,
                                  ctx, shProg, "glUniformMatrix");
   if (uni == NULL)
      return;

   if (!uni->type->is_matrix()) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
		  "glUniformMatrix(non-matrix uniform)");
      return;
   }

   assert(basicType == GLSL_TYPE_FLOAT || basicType == GLSL_TYPE_DOUBLE);
   const unsigned size_mul = basicType == GLSL_TYPE_DOUBLE ? 2 : 1;

   assert(!uni->type->is_sampler());
   const unsigned vectors = uni->type->matrix_columns;
   const unsigned components = uni->type->vector_elements;

   /* Verify that the types are compatible.  This is greatly simplified for
    * matrices because they can only have a float base type.
    */
   if (vectors != cols || components != rows) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
		  "glUniformMatrix(matrix size mismatch)");
      return;
   }

   /* GL_INVALID_VALUE is generated if `transpose' is not GL_FALSE.
    * http://www.khronos.org/opengles/sdk/docs/man/xhtml/glUniform.xml
    */
   if (transpose) {
      if (ctx->API == API_OPENGLES2 && ctx->Version < 30) {
	 _mesa_error(ctx, GL_INVALID_VALUE,
		     "glUniformMatrix(matrix transpose is not GL_FALSE)");
	 return;
      }
   }

   /* Section 2.11.7 (Uniform Variables) of the OpenGL 4.2 Core Profile spec
    * says:
    *
    *     "If any of the following conditions occur, an INVALID_OPERATION
    *     error is generated by the Uniform* commands, and no uniform values
    *     are changed:
    *
    *     ...
    *
    *     - if the uniform declared in the shader is not of type boolean and
    *       the type indicated in the name of the Uniform* command used does
    *       not match the type of the uniform"
    *
    * There are no Boolean matrix types, so we do not need to allow
    * GLSL_TYPE_BOOL here (as _mesa_uniform does).
    */
   if (uni->type->base_type != basicType) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
                  "glUniformMatrix%ux%u(\"%s\"@%d is %s, not %s)",
                  cols, rows, uni->name, location,
                  glsl_type_name(uni->type->base_type),
                  glsl_type_name(basicType));
      return;
   }

   if (unlikely(ctx->_Shader->Flags & GLSL_UNIFORMS)) {
      log_uniform(values, uni->type->base_type, components, vectors, count,
		  bool(transpose), shProg, location, uni);
   }

   /* Page 82 (page 96 of the PDF) of the OpenGL 2.1 spec says:
    *
    *     "When loading N elements starting at an arbitrary position k in a
    *     uniform declared as an array, elements k through k + N - 1 in the
    *     array will be replaced with the new values. Values for any array
    *     element that exceeds the highest array element index used, as
    *     reported by GetActiveUniform, will be ignored by the GL."
    *
    * Clamp 'count' to a valid value.  Note that for non-arrays a count > 1
    * will have already generated an error.
    */
   if (uni->array_elements != 0) {
      count = MIN2(count, (int) (uni->array_elements - offset));
   }

   _mesa_flush_vertices_for_uniforms(ctx, uni);

   /* Store the data in the "actual type" backing storage for the uniform.
    */
   const unsigned elements = components * vectors;

   if (!transpose) {
      memcpy(&uni->storage[size_mul * elements * offset], values,
	     sizeof(uni->storage[0]) * elements * count * size_mul);
   } else if (basicType == GLSL_TYPE_FLOAT) {
      /* Copy and transpose the matrix.
       */
      const float *src = (const float *)values;
      float *dst = &uni->storage[elements * offset].f;

      for (int i = 0; i < count; i++) {
	 for (unsigned r = 0; r < rows; r++) {
	    for (unsigned c = 0; c < cols; c++) {
	       dst[(c * components) + r] = src[c + (r * vectors)];
	    }
	 }

	 dst += elements;
	 src += elements;
      }
   } else {
      assert(basicType == GLSL_TYPE_DOUBLE);
      const double *src = (const double *)values;
      double *dst = (double *)&uni->storage[elements * offset].f;

      for (int i = 0; i < count; i++) {
	 for (unsigned r = 0; r < rows; r++) {
	    for (unsigned c = 0; c < cols; c++) {
	       dst[(c * components) + r] = src[c + (r * vectors)];
	    }
	 }

	 dst += elements;
	 src += elements;
      }
   }

   _mesa_propagate_uniforms_to_driver_storage(uni, offset, count);
}

static void
update_bound_bindless_sampler_flag(struct gl_program *prog)
{
   unsigned i;

   if (likely(!prog->sh.HasBoundBindlessSampler))
      return;

   for (i = 0; i < prog->sh.NumBindlessSamplers; i++) {
      struct gl_bindless_sampler *sampler = &prog->sh.BindlessSamplers[i];

      if (sampler->bound)
         return;
   }
   prog->sh.HasBoundBindlessSampler = false;
}

static void
update_bound_bindless_image_flag(struct gl_program *prog)
{
   unsigned i;

   if (likely(!prog->sh.HasBoundBindlessImage))
      return;

   for (i = 0; i < prog->sh.NumBindlessImages; i++) {
      struct gl_bindless_image *image = &prog->sh.BindlessImages[i];

      if (image->bound)
         return;
   }
   prog->sh.HasBoundBindlessImage = false;
}

/**
 * Called via glUniformHandleui64*ARB() functions.
 */
extern "C" void
_mesa_uniform_handle(GLint location, GLsizei count, const GLvoid *values,
                     struct gl_context *ctx, struct gl_shader_program *shProg)
{
   unsigned offset;
   struct gl_uniform_storage *uni;

   if (_mesa_is_no_error_enabled(ctx)) {
      /* From Section 7.6 (UNIFORM VARIABLES) of the OpenGL 4.5 spec:
       *
       *   "If the value of location is -1, the Uniform* commands will
       *   silently ignore the data passed in, and the current uniform values
       *   will not be changed.
       */
      if (location == -1)
         return;

      uni = shProg->UniformRemapTable[location];

      /* The array index specified by the uniform location is just the
       * uniform location minus the base location of of the uniform.
       */
      assert(uni->array_elements > 0 || location == (int)uni->remap_location);
      offset = location - uni->remap_location;
   } else {
      uni = validate_uniform_parameters(location, count, &offset,
                                        ctx, shProg, "glUniformHandleui64*ARB");
      if (!uni)
         return;

      if (!uni->is_bindless) {
         /* From section "Errors" of the ARB_bindless_texture spec:
          *
          * "The error INVALID_OPERATION is generated by
          *  UniformHandleui64{v}ARB if the sampler or image uniform being
          *  updated has the "bound_sampler" or "bound_image" layout qualifier."
          *
          * From section 4.4.6 of the ARB_bindless_texture spec:
          *
          * "In the absence of these qualifiers, sampler and image uniforms are
          *  considered "bound". Additionally, if GL_ARB_bindless_texture is
          *  not enabled, these uniforms are considered "bound"."
          */
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "glUniformHandleui64*ARB(non-bindless sampler/image uniform)");
         return;
      }
   }

   const unsigned components = uni->type->vector_elements;
   const int size_mul = 2;

   if (unlikely(ctx->_Shader->Flags & GLSL_UNIFORMS)) {
      log_uniform(values, GLSL_TYPE_UINT64, components, 1, count,
                  false, shProg, location, uni);
   }

   /* Page 82 (page 96 of the PDF) of the OpenGL 2.1 spec says:
    *
    *     "When loading N elements starting at an arbitrary position k in a
    *     uniform declared as an array, elements k through k + N - 1 in the
    *     array will be replaced with the new values. Values for any array
    *     element that exceeds the highest array element index used, as
    *     reported by GetActiveUniform, will be ignored by the GL."
    *
    * Clamp 'count' to a valid value.  Note that for non-arrays a count > 1
    * will have already generated an error.
    */
   if (uni->array_elements != 0) {
      count = MIN2(count, (int) (uni->array_elements - offset));
   }

   _mesa_flush_vertices_for_uniforms(ctx, uni);

   /* Store the data in the "actual type" backing storage for the uniform.
    */
   memcpy(&uni->storage[size_mul * components * offset], values,
          sizeof(uni->storage[0]) * components * count * size_mul);

   _mesa_propagate_uniforms_to_driver_storage(uni, offset, count);

   if (uni->type->is_sampler()) {
      /* Mark this bindless sampler as not bound to a texture unit because
       * it refers to a texture handle.
       */
      for (int i = 0; i < MESA_SHADER_STAGES; i++) {
         struct gl_linked_shader *const sh = shProg->_LinkedShaders[i];

         /* If the shader stage doesn't use the sampler uniform, skip this. */
         if (!uni->opaque[i].active)
            continue;

         for (int j = 0; j < count; j++) {
            unsigned unit = uni->opaque[i].index + offset + j;
            struct gl_bindless_sampler *sampler =
               &sh->Program->sh.BindlessSamplers[unit];

            sampler->bound = false;
         }

         update_bound_bindless_sampler_flag(sh->Program);
      }
   }

   if (uni->type->is_image()) {
      /* Mark this bindless image as not bound to an image unit because it
       * refers to a texture handle.
       */
      for (int i = 0; i < MESA_SHADER_STAGES; i++) {
         struct gl_linked_shader *sh = shProg->_LinkedShaders[i];

         /* If the shader stage doesn't use the sampler uniform, skip this. */
         if (!uni->opaque[i].active)
            continue;

         for (int j = 0; j < count; j++) {
            unsigned unit = uni->opaque[i].index + offset + j;
            struct gl_bindless_image *image =
               &sh->Program->sh.BindlessImages[unit];

            image->bound = false;
         }

         update_bound_bindless_image_flag(sh->Program);
      }
   }
}

extern "C" bool
_mesa_sampler_uniforms_are_valid(const struct gl_shader_program *shProg,
				 char *errMsg, size_t errMsgLength)
{
   /* Shader does not have samplers. */
   if (shProg->data->NumUniformStorage == 0)
      return true;

   if (!shProg->SamplersValidated) {
      _mesa_snprintf(errMsg, errMsgLength,
                     "active samplers with a different type "
                     "refer to the same texture image unit");
      return false;
   }
   return true;
}

extern "C" bool
_mesa_sampler_uniforms_pipeline_are_valid(struct gl_pipeline_object *pipeline)
{
   /* Section 2.11.11 (Shader Execution), subheading "Validation," of the
    * OpenGL 4.1 spec says:
    *
    *     "[INVALID_OPERATION] is generated by any command that transfers
    *     vertices to the GL if:
    *
    *         ...
    *
    *         - Any two active samplers in the current program object are of
    *           different types, but refer to the same texture image unit.
    *
    *         - The number of active samplers in the program exceeds the
    *           maximum number of texture image units allowed."
    */

   GLbitfield mask;
   GLbitfield TexturesUsed[MAX_COMBINED_TEXTURE_IMAGE_UNITS];
   unsigned active_samplers = 0;
   const struct gl_program **prog =
      (const struct gl_program **) pipeline->CurrentProgram;


   memset(TexturesUsed, 0, sizeof(TexturesUsed));

   for (unsigned idx = 0; idx < ARRAY_SIZE(pipeline->CurrentProgram); idx++) {
      if (!prog[idx])
         continue;

      mask = prog[idx]->SamplersUsed;
      while (mask) {
         const int s = u_bit_scan(&mask);
         GLuint unit = prog[idx]->SamplerUnits[s];
         GLuint tgt = prog[idx]->sh.SamplerTargets[s];

         /* FIXME: Samplers are initialized to 0 and Mesa doesn't do a
          * great job of eliminating unused uniforms currently so for now
          * don't throw an error if two sampler types both point to 0.
          */
         if (unit == 0)
            continue;

         if (TexturesUsed[unit] & ~(1 << tgt)) {
            pipeline->InfoLog =
               ralloc_asprintf(pipeline,
                     "Program %d: "
                     "Texture unit %d is accessed with 2 different types",
                     prog[idx]->Id, unit);
            return false;
         }

         TexturesUsed[unit] |= (1 << tgt);
      }

      active_samplers += prog[idx]->info.num_textures;
   }

   if (active_samplers > MAX_COMBINED_TEXTURE_IMAGE_UNITS) {
      pipeline->InfoLog =
         ralloc_asprintf(pipeline,
                         "the number of active samplers %d exceed the "
                         "maximum %d",
                         active_samplers, MAX_COMBINED_TEXTURE_IMAGE_UNITS);
      return false;
   }

   return true;
}
