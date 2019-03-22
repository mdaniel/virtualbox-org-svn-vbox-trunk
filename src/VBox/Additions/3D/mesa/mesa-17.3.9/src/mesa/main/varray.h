/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009  VMware, Inc.  All Rights Reserved.
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


#ifndef VARRAY_H
#define VARRAY_H


#include "glheader.h"
#include "bufferobj.h"

struct gl_vertex_array;
struct gl_context;

/**
 * Returns a pointer to the vertex attribute data in a client array,
 * or the offset into the vertex buffer for an array that resides in
 * a vertex buffer.
 */
static inline const GLubyte *
_mesa_vertex_attrib_address(const struct gl_array_attributes *array,
                            const struct gl_vertex_buffer_binding *binding)
{
   if (_mesa_is_bufferobj(binding->BufferObj))
      return (const GLubyte *) (binding->Offset + array->RelativeOffset);
   else
      return array->Ptr;       
}

/**
 * Sets the fields in a gl_vertex_array to values derived from a
 * gl_vertex_attrib_array and a gl_vertex_buffer_binding.
 */
static inline void
_mesa_update_client_array(struct gl_context *ctx,
                          struct gl_vertex_array *dst,
                          const struct gl_array_attributes *src,
                          const struct gl_vertex_buffer_binding *binding)
{
   dst->Size = src->Size;
   dst->Type = src->Type;
   dst->Format = src->Format;
   dst->StrideB = binding->Stride;
   dst->Ptr = _mesa_vertex_attrib_address(src, binding);
   dst->Normalized = src->Normalized;
   dst->Integer = src->Integer;
   dst->Doubles = src->Doubles;
   dst->InstanceDivisor = binding->InstanceDivisor;
   dst->_ElementSize = src->_ElementSize;
   _mesa_reference_buffer_object(ctx, &dst->BufferObj, binding->BufferObj);
}

static inline bool
_mesa_attr_zero_aliases_vertex(const struct gl_context *ctx)
{
   return ctx->_AttribZeroAliasesVertex;
}

extern void
_mesa_update_array_format(struct gl_context *ctx,
                          struct gl_vertex_array_object *vao,
                          GLuint attrib, GLint size, GLenum type,
                          GLenum format, GLboolean normalized,
                          GLboolean integer, GLboolean doubles,
                          GLuint relativeOffset);

extern void
_mesa_enable_vertex_array_attrib(struct gl_context *ctx,
                                 struct gl_vertex_array_object *vao,
                                 unsigned attrib);

extern void
_mesa_bind_vertex_buffer(struct gl_context *ctx,
                         struct gl_vertex_array_object *vao,
                         GLuint index,
                         struct gl_buffer_object *vbo,
                         GLintptr offset, GLsizei stride);

extern void GLAPIENTRY
_mesa_VertexPointer_no_error(GLint size, GLenum type, GLsizei stride,
                             const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_VertexPointer(GLint size, GLenum type, GLsizei stride,
                    const GLvoid *ptr);

extern void GLAPIENTRY
_mesa_NormalPointer_no_error(GLenum type, GLsizei stride, const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_NormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr);

extern void GLAPIENTRY
_mesa_ColorPointer_no_error(GLint size, GLenum type, GLsizei stride,
                            const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_ColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_IndexPointer_no_error(GLenum type, GLsizei stride, const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_IndexPointer(GLenum type, GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_TexCoordPointer_no_error(GLint size, GLenum type, GLsizei stride,
                               const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                      const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_EdgeFlagPointer_no_error(GLsizei stride, const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_EdgeFlagPointer(GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_VertexPointerEXT(GLint size, GLenum type, GLsizei stride,
                       GLsizei count, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_NormalPointerEXT(GLenum type, GLsizei stride, GLsizei count,
                       const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_ColorPointerEXT(GLint size, GLenum type, GLsizei stride, GLsizei count,
                      const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_IndexPointerEXT(GLenum type, GLsizei stride, GLsizei count,
                      const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_TexCoordPointerEXT(GLint size, GLenum type, GLsizei stride,
                         GLsizei count, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_EdgeFlagPointerEXT(GLsizei stride, GLsizei count, const GLboolean *ptr);

extern void GLAPIENTRY
_mesa_FogCoordPointer_no_error(GLenum type, GLsizei stride,
                               const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_FogCoordPointer(GLenum type, GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_SecondaryColorPointer_no_error(GLint size, GLenum type,
                                     GLsizei stride, const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_SecondaryColorPointer(GLint size, GLenum type,
			       GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_PointSizePointerOES_no_error(GLenum type, GLsizei stride,
                                   const GLvoid *ptr);
extern void GLAPIENTRY
_mesa_PointSizePointerOES(GLenum type, GLsizei stride, const GLvoid *ptr);


extern void GLAPIENTRY
_mesa_VertexAttribPointer_no_error(GLuint index, GLint size, GLenum type,
                                   GLboolean normalized, GLsizei stride,
                                   const GLvoid *pointer);
extern void GLAPIENTRY
_mesa_VertexAttribPointer(GLuint index, GLint size, GLenum type,
                             GLboolean normalized, GLsizei stride,
                             const GLvoid *pointer);

void GLAPIENTRY
_mesa_VertexAttribIPointer_no_error(GLuint index, GLint size, GLenum type,
                                    GLsizei stride, const GLvoid *ptr);
void GLAPIENTRY
_mesa_VertexAttribIPointer(GLuint index, GLint size, GLenum type,
                           GLsizei stride, const GLvoid *ptr);

extern void GLAPIENTRY
_mesa_VertexAttribLPointer_no_error(GLuint index, GLint size, GLenum type,
                                    GLsizei stride, const GLvoid *pointer);
extern void GLAPIENTRY
_mesa_VertexAttribLPointer(GLuint index, GLint size, GLenum type,
                           GLsizei stride, const GLvoid *pointer);

extern void GLAPIENTRY
_mesa_EnableVertexAttribArray(GLuint index);

extern void GLAPIENTRY
_mesa_EnableVertexAttribArray_no_error(GLuint index);


extern void GLAPIENTRY
_mesa_EnableVertexArrayAttrib(GLuint vaobj, GLuint index);

extern void GLAPIENTRY
_mesa_EnableVertexArrayAttrib_no_error(GLuint vaobj, GLuint index);



extern void GLAPIENTRY
_mesa_DisableVertexAttribArray(GLuint index);

extern void GLAPIENTRY
_mesa_DisableVertexAttribArray_no_error(GLuint index);


extern void GLAPIENTRY
_mesa_DisableVertexArrayAttrib(GLuint vaobj, GLuint index);

extern void GLAPIENTRY
_mesa_DisableVertexArrayAttrib_no_error(GLuint vaobj, GLuint index);


extern void GLAPIENTRY
_mesa_GetVertexAttribdv(GLuint index, GLenum pname, GLdouble *params);

extern void GLAPIENTRY
_mesa_GetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);

extern void GLAPIENTRY
_mesa_GetVertexAttribLdv(GLuint index, GLenum pname, GLdouble *params);

extern void GLAPIENTRY
_mesa_GetVertexAttribiv(GLuint index, GLenum pname, GLint *params);

extern void GLAPIENTRY
_mesa_GetVertexAttribLui64vARB(GLuint index, GLenum pname, GLuint64EXT *params);


extern void GLAPIENTRY
_mesa_GetVertexAttribIiv(GLuint index, GLenum pname, GLint *params);


extern void GLAPIENTRY
_mesa_GetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);


extern void GLAPIENTRY
_mesa_GetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid **pointer);


void GLAPIENTRY
_mesa_GetVertexArrayIndexediv(GLuint vaobj, GLuint index,
                              GLenum pname, GLint *param);


void GLAPIENTRY
_mesa_GetVertexArrayIndexed64iv(GLuint vaobj, GLuint index,
                                GLenum pname, GLint64 *param);


extern void GLAPIENTRY
_mesa_InterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer);


extern void GLAPIENTRY
_mesa_MultiDrawArrays( GLenum mode, const GLint *first,
                          const GLsizei *count, GLsizei primcount );

extern void GLAPIENTRY
_mesa_MultiDrawElementsEXT( GLenum mode, const GLsizei *count, GLenum type,
                            const GLvoid **indices, GLsizei primcount );

extern void GLAPIENTRY
_mesa_MultiDrawElementsBaseVertex( GLenum mode,
				   const GLsizei *count, GLenum type,
				   const GLvoid **indices, GLsizei primcount,
				   const GLint *basevertex);

extern void GLAPIENTRY
_mesa_MultiModeDrawArraysIBM( const GLenum * mode, const GLint * first,
			      const GLsizei * count,
			      GLsizei primcount, GLint modestride );


extern void GLAPIENTRY
_mesa_MultiModeDrawElementsIBM( const GLenum * mode, const GLsizei * count,
				GLenum type, const GLvoid * const * indices,
				GLsizei primcount, GLint modestride );

extern void GLAPIENTRY
_mesa_LockArraysEXT(GLint first, GLsizei count);

extern void GLAPIENTRY
_mesa_UnlockArraysEXT( void );


extern void GLAPIENTRY
_mesa_DrawArrays(GLenum mode, GLint first, GLsizei count);

extern void GLAPIENTRY
_mesa_DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                          GLsizei primcount);

extern void GLAPIENTRY
_mesa_DrawElements(GLenum mode, GLsizei count, GLenum type,
                   const GLvoid *indices);

extern void GLAPIENTRY
_mesa_DrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                        GLenum type, const GLvoid *indices);

extern void GLAPIENTRY
_mesa_DrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type,
			     const GLvoid *indices, GLint basevertex);

extern void GLAPIENTRY
_mesa_DrawRangeElementsBaseVertex(GLenum mode, GLuint start, GLuint end,
				  GLsizei count, GLenum type,
				  const GLvoid *indices,
				  GLint basevertex);

extern void GLAPIENTRY
_mesa_DrawTransformFeedback(GLenum mode, GLuint name);

void GLAPIENTRY
_mesa_PrimitiveRestartIndex_no_error(GLuint index);

extern void GLAPIENTRY
_mesa_PrimitiveRestartIndex(GLuint index);

extern void GLAPIENTRY
_mesa_VertexAttribDivisor_no_error(GLuint index, GLuint divisor);
extern void GLAPIENTRY
_mesa_VertexAttribDivisor(GLuint index, GLuint divisor);

static inline unsigned
_mesa_primitive_restart_index(const struct gl_context *ctx,
                              unsigned index_size)
{
   /* From the OpenGL 4.3 core specification, page 302:
    * "If both PRIMITIVE_RESTART and PRIMITIVE_RESTART_FIXED_INDEX are
    *  enabled, the index value determined by PRIMITIVE_RESTART_FIXED_INDEX
    *  is used."
    */
   if (ctx->Array.PrimitiveRestartFixedIndex) {
      /* 1 -> 0xff, 2 -> 0xffff, 4 -> 0xffffffff */
      return 0xffffffffu >> 8 * (4 - index_size);
   }

   return ctx->Array.RestartIndex;
}

extern void GLAPIENTRY
_mesa_BindVertexBuffer_no_error(GLuint bindingIndex, GLuint buffer,
                                GLintptr offset, GLsizei stride);
extern void GLAPIENTRY
_mesa_BindVertexBuffer(GLuint bindingIndex, GLuint buffer, GLintptr offset,
                       GLsizei stride);

void GLAPIENTRY
_mesa_VertexArrayVertexBuffer_no_error(GLuint vaobj, GLuint bindingIndex,
                                       GLuint buffer, GLintptr offset,
                                       GLsizei stride);
extern void GLAPIENTRY
_mesa_VertexArrayVertexBuffer(GLuint vaobj, GLuint bindingIndex, GLuint buffer,
                              GLintptr offset, GLsizei stride);

void GLAPIENTRY
_mesa_BindVertexBuffers_no_error(GLuint first, GLsizei count,
                                 const GLuint *buffers, const GLintptr *offsets,
                                 const GLsizei *strides);

extern void GLAPIENTRY
_mesa_BindVertexBuffers(GLuint first, GLsizei count, const GLuint *buffers,
                        const GLintptr *offsets, const GLsizei *strides);

void GLAPIENTRY
_mesa_VertexArrayVertexBuffers_no_error(GLuint vaobj, GLuint first,
                                        GLsizei count, const GLuint *buffers,
                                        const GLintptr *offsets,
                                        const GLsizei *strides);

extern void GLAPIENTRY
_mesa_VertexArrayVertexBuffers(GLuint vaobj, GLuint first, GLsizei count,
                               const GLuint *buffers,
                               const GLintptr *offsets, const GLsizei *strides);

extern void GLAPIENTRY
_mesa_VertexAttribFormat(GLuint attribIndex, GLint size, GLenum type,
                         GLboolean normalized, GLuint relativeOffset);

extern void GLAPIENTRY
_mesa_VertexArrayAttribFormat(GLuint vaobj, GLuint attribIndex, GLint size,
                              GLenum type, GLboolean normalized,
                              GLuint relativeOffset);

extern void GLAPIENTRY
_mesa_VertexAttribIFormat(GLuint attribIndex, GLint size, GLenum type,
                          GLuint relativeOffset);

extern void GLAPIENTRY
_mesa_VertexArrayAttribIFormat(GLuint vaobj, GLuint attribIndex,
                               GLint size, GLenum type,
                               GLuint relativeOffset);

extern void GLAPIENTRY
_mesa_VertexAttribLFormat(GLuint attribIndex, GLint size, GLenum type,
                          GLuint relativeOffset);

extern void GLAPIENTRY
_mesa_VertexArrayAttribLFormat(GLuint vaobj, GLuint attribIndex,
                               GLint size, GLenum type,
                               GLuint relativeOffset);

void GLAPIENTRY
_mesa_VertexAttribBinding_no_error(GLuint attribIndex, GLuint bindingIndex);

extern void GLAPIENTRY
_mesa_VertexAttribBinding(GLuint attribIndex, GLuint bindingIndex);

void GLAPIENTRY
_mesa_VertexArrayAttribBinding_no_error(GLuint vaobj, GLuint attribIndex,
                                        GLuint bindingIndex);

extern void GLAPIENTRY
_mesa_VertexArrayAttribBinding(GLuint vaobj, GLuint attribIndex,
                               GLuint bindingIndex);

void GLAPIENTRY
_mesa_VertexBindingDivisor_no_error(GLuint bindingIndex, GLuint divisor);

extern void GLAPIENTRY
_mesa_VertexBindingDivisor(GLuint bindingIndex, GLuint divisor);

void GLAPIENTRY
_mesa_VertexArrayBindingDivisor_no_error(GLuint vaobj, GLuint bindingIndex,
                                         GLuint divisor);

extern void GLAPIENTRY
_mesa_VertexArrayBindingDivisor(GLuint vaobj, GLuint bindingIndex, GLuint divisor);

extern void
_mesa_copy_client_array(struct gl_context *ctx,
                        struct gl_vertex_array *dst,
                        struct gl_vertex_array *src);

extern void
_mesa_copy_vertex_attrib_array(struct gl_context *ctx,
                               struct gl_array_attributes *dst,
                               const struct gl_array_attributes *src);

extern void
_mesa_copy_vertex_buffer_binding(struct gl_context *ctx,
                                 struct gl_vertex_buffer_binding *dst,
                                 const struct gl_vertex_buffer_binding *src);

extern void
_mesa_print_arrays(struct gl_context *ctx);

extern void
_mesa_init_varray( struct gl_context * ctx );

extern void 
_mesa_free_varray_data(struct gl_context *ctx);

#endif
