/*
 * Copyright 2003 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "main/bufferobj.h"
#include "main/context.h"
#include "main/enums.h"
#include "main/macros.h"
#include "main/glformats.h"

#include "brw_draw.h"
#include "brw_defines.h"
#include "brw_context.h"
#include "brw_state.h"

#include "intel_batchbuffer.h"
#include "intel_buffer_objects.h"

static const GLuint double_types_float[5] = {
   0,
   ISL_FORMAT_R64_FLOAT,
   ISL_FORMAT_R64G64_FLOAT,
   ISL_FORMAT_R64G64B64_FLOAT,
   ISL_FORMAT_R64G64B64A64_FLOAT
};

static const GLuint double_types_passthru[5] = {
   0,
   ISL_FORMAT_R64_PASSTHRU,
   ISL_FORMAT_R64G64_PASSTHRU,
   ISL_FORMAT_R64G64B64_PASSTHRU,
   ISL_FORMAT_R64G64B64A64_PASSTHRU
};

static const GLuint float_types[5] = {
   0,
   ISL_FORMAT_R32_FLOAT,
   ISL_FORMAT_R32G32_FLOAT,
   ISL_FORMAT_R32G32B32_FLOAT,
   ISL_FORMAT_R32G32B32A32_FLOAT
};

static const GLuint half_float_types[5] = {
   0,
   ISL_FORMAT_R16_FLOAT,
   ISL_FORMAT_R16G16_FLOAT,
   ISL_FORMAT_R16G16B16_FLOAT,
   ISL_FORMAT_R16G16B16A16_FLOAT
};

static const GLuint fixed_point_types[5] = {
   0,
   ISL_FORMAT_R32_SFIXED,
   ISL_FORMAT_R32G32_SFIXED,
   ISL_FORMAT_R32G32B32_SFIXED,
   ISL_FORMAT_R32G32B32A32_SFIXED,
};

static const GLuint uint_types_direct[5] = {
   0,
   ISL_FORMAT_R32_UINT,
   ISL_FORMAT_R32G32_UINT,
   ISL_FORMAT_R32G32B32_UINT,
   ISL_FORMAT_R32G32B32A32_UINT
};

static const GLuint uint_types_norm[5] = {
   0,
   ISL_FORMAT_R32_UNORM,
   ISL_FORMAT_R32G32_UNORM,
   ISL_FORMAT_R32G32B32_UNORM,
   ISL_FORMAT_R32G32B32A32_UNORM
};

static const GLuint uint_types_scale[5] = {
   0,
   ISL_FORMAT_R32_USCALED,
   ISL_FORMAT_R32G32_USCALED,
   ISL_FORMAT_R32G32B32_USCALED,
   ISL_FORMAT_R32G32B32A32_USCALED
};

static const GLuint int_types_direct[5] = {
   0,
   ISL_FORMAT_R32_SINT,
   ISL_FORMAT_R32G32_SINT,
   ISL_FORMAT_R32G32B32_SINT,
   ISL_FORMAT_R32G32B32A32_SINT
};

static const GLuint int_types_norm[5] = {
   0,
   ISL_FORMAT_R32_SNORM,
   ISL_FORMAT_R32G32_SNORM,
   ISL_FORMAT_R32G32B32_SNORM,
   ISL_FORMAT_R32G32B32A32_SNORM
};

static const GLuint int_types_scale[5] = {
   0,
   ISL_FORMAT_R32_SSCALED,
   ISL_FORMAT_R32G32_SSCALED,
   ISL_FORMAT_R32G32B32_SSCALED,
   ISL_FORMAT_R32G32B32A32_SSCALED
};

static const GLuint ushort_types_direct[5] = {
   0,
   ISL_FORMAT_R16_UINT,
   ISL_FORMAT_R16G16_UINT,
   ISL_FORMAT_R16G16B16_UINT,
   ISL_FORMAT_R16G16B16A16_UINT
};

static const GLuint ushort_types_norm[5] = {
   0,
   ISL_FORMAT_R16_UNORM,
   ISL_FORMAT_R16G16_UNORM,
   ISL_FORMAT_R16G16B16_UNORM,
   ISL_FORMAT_R16G16B16A16_UNORM
};

static const GLuint ushort_types_scale[5] = {
   0,
   ISL_FORMAT_R16_USCALED,
   ISL_FORMAT_R16G16_USCALED,
   ISL_FORMAT_R16G16B16_USCALED,
   ISL_FORMAT_R16G16B16A16_USCALED
};

static const GLuint short_types_direct[5] = {
   0,
   ISL_FORMAT_R16_SINT,
   ISL_FORMAT_R16G16_SINT,
   ISL_FORMAT_R16G16B16_SINT,
   ISL_FORMAT_R16G16B16A16_SINT
};

static const GLuint short_types_norm[5] = {
   0,
   ISL_FORMAT_R16_SNORM,
   ISL_FORMAT_R16G16_SNORM,
   ISL_FORMAT_R16G16B16_SNORM,
   ISL_FORMAT_R16G16B16A16_SNORM
};

static const GLuint short_types_scale[5] = {
   0,
   ISL_FORMAT_R16_SSCALED,
   ISL_FORMAT_R16G16_SSCALED,
   ISL_FORMAT_R16G16B16_SSCALED,
   ISL_FORMAT_R16G16B16A16_SSCALED
};

static const GLuint ubyte_types_direct[5] = {
   0,
   ISL_FORMAT_R8_UINT,
   ISL_FORMAT_R8G8_UINT,
   ISL_FORMAT_R8G8B8_UINT,
   ISL_FORMAT_R8G8B8A8_UINT
};

static const GLuint ubyte_types_norm[5] = {
   0,
   ISL_FORMAT_R8_UNORM,
   ISL_FORMAT_R8G8_UNORM,
   ISL_FORMAT_R8G8B8_UNORM,
   ISL_FORMAT_R8G8B8A8_UNORM
};

static const GLuint ubyte_types_scale[5] = {
   0,
   ISL_FORMAT_R8_USCALED,
   ISL_FORMAT_R8G8_USCALED,
   ISL_FORMAT_R8G8B8_USCALED,
   ISL_FORMAT_R8G8B8A8_USCALED
};

static const GLuint byte_types_direct[5] = {
   0,
   ISL_FORMAT_R8_SINT,
   ISL_FORMAT_R8G8_SINT,
   ISL_FORMAT_R8G8B8_SINT,
   ISL_FORMAT_R8G8B8A8_SINT
};

static const GLuint byte_types_norm[5] = {
   0,
   ISL_FORMAT_R8_SNORM,
   ISL_FORMAT_R8G8_SNORM,
   ISL_FORMAT_R8G8B8_SNORM,
   ISL_FORMAT_R8G8B8A8_SNORM
};

static const GLuint byte_types_scale[5] = {
   0,
   ISL_FORMAT_R8_SSCALED,
   ISL_FORMAT_R8G8_SSCALED,
   ISL_FORMAT_R8G8B8_SSCALED,
   ISL_FORMAT_R8G8B8A8_SSCALED
};

static GLuint
double_types(struct brw_context *brw,
             int size,
             GLboolean doubles)
{
   /* From the BDW PRM, Volume 2d, page 588 (VERTEX_ELEMENT_STATE):
    * "When SourceElementFormat is set to one of the *64*_PASSTHRU formats,
    * 64-bit components are stored in the URB without any conversion."
    * Also included on BDW PRM, Volume 7, page 470, table "Source Element
    * Formats Supported in VF Unit"
    *
    * Previous PRMs don't include those references, so for gen7 we can't use
    * PASSTHRU formats directly. But in any case, we prefer to return passthru
    * even in that case, because that reflects what we want to achieve, even
    * if we would need to workaround on gen < 8.
    */
   return (doubles
           ? double_types_passthru[size]
           : double_types_float[size]);
}

/**
 * Given vertex array type/size/format/normalized info, return
 * the appopriate hardware surface type.
 * Format will be GL_RGBA or possibly GL_BGRA for GLubyte[4] color arrays.
 */
unsigned
brw_get_vertex_surface_type(struct brw_context *brw,
                            const struct gl_vertex_array *glarray)
{
   int size = glarray->Size;
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   const bool is_ivybridge_or_older =
      devinfo->gen <= 7 && !devinfo->is_baytrail && !devinfo->is_haswell;

   if (unlikely(INTEL_DEBUG & DEBUG_VERTS))
      fprintf(stderr, "type %s size %d normalized %d\n",
              _mesa_enum_to_string(glarray->Type),
              glarray->Size, glarray->Normalized);

   if (glarray->Integer) {
      assert(glarray->Format == GL_RGBA); /* sanity check */
      switch (glarray->Type) {
      case GL_INT: return int_types_direct[size];
      case GL_SHORT:
         if (is_ivybridge_or_older && size == 3)
            return short_types_direct[4];
         else
            return short_types_direct[size];
      case GL_BYTE:
         if (is_ivybridge_or_older && size == 3)
            return byte_types_direct[4];
         else
            return byte_types_direct[size];
      case GL_UNSIGNED_INT: return uint_types_direct[size];
      case GL_UNSIGNED_SHORT:
         if (is_ivybridge_or_older && size == 3)
            return ushort_types_direct[4];
         else
            return ushort_types_direct[size];
      case GL_UNSIGNED_BYTE:
         if (is_ivybridge_or_older && size == 3)
            return ubyte_types_direct[4];
         else
            return ubyte_types_direct[size];
      default: unreachable("not reached");
      }
   } else if (glarray->Type == GL_UNSIGNED_INT_10F_11F_11F_REV) {
      return ISL_FORMAT_R11G11B10_FLOAT;
   } else if (glarray->Normalized) {
      switch (glarray->Type) {
      case GL_DOUBLE: return double_types(brw, size, glarray->Doubles);
      case GL_FLOAT: return float_types[size];
      case GL_HALF_FLOAT:
      case GL_HALF_FLOAT_OES:
         if (devinfo->gen < 6 && size == 3)
            return half_float_types[4];
         else
            return half_float_types[size];
      case GL_INT: return int_types_norm[size];
      case GL_SHORT: return short_types_norm[size];
      case GL_BYTE: return byte_types_norm[size];
      case GL_UNSIGNED_INT: return uint_types_norm[size];
      case GL_UNSIGNED_SHORT: return ushort_types_norm[size];
      case GL_UNSIGNED_BYTE:
         if (glarray->Format == GL_BGRA) {
            /* See GL_EXT_vertex_array_bgra */
            assert(size == 4);
            return ISL_FORMAT_B8G8R8A8_UNORM;
         }
         else {
            return ubyte_types_norm[size];
         }
      case GL_FIXED:
         if (devinfo->gen >= 8 || devinfo->is_haswell)
            return fixed_point_types[size];

         /* This produces GL_FIXED inputs as values between INT32_MIN and
          * INT32_MAX, which will be scaled down by 1/65536 by the VS.
          */
         return int_types_scale[size];
      /* See GL_ARB_vertex_type_2_10_10_10_rev.
       * W/A: Pre-Haswell, the hardware doesn't really support the formats we'd
       * like to use here, so upload everything as UINT and fix
       * it in the shader
       */
      case GL_INT_2_10_10_10_REV:
         assert(size == 4);
         if (devinfo->gen >= 8 || devinfo->is_haswell) {
            return glarray->Format == GL_BGRA
               ? ISL_FORMAT_B10G10R10A2_SNORM
               : ISL_FORMAT_R10G10B10A2_SNORM;
         }
         return ISL_FORMAT_R10G10B10A2_UINT;
      case GL_UNSIGNED_INT_2_10_10_10_REV:
         assert(size == 4);
         if (devinfo->gen >= 8 || devinfo->is_haswell) {
            return glarray->Format == GL_BGRA
               ? ISL_FORMAT_B10G10R10A2_UNORM
               : ISL_FORMAT_R10G10B10A2_UNORM;
         }
         return ISL_FORMAT_R10G10B10A2_UINT;
      default: unreachable("not reached");
      }
   }
   else {
      /* See GL_ARB_vertex_type_2_10_10_10_rev.
       * W/A: the hardware doesn't really support the formats we'd
       * like to use here, so upload everything as UINT and fix
       * it in the shader
       */
      if (glarray->Type == GL_INT_2_10_10_10_REV) {
         assert(size == 4);
         if (devinfo->gen >= 8 || devinfo->is_haswell) {
            return glarray->Format == GL_BGRA
               ? ISL_FORMAT_B10G10R10A2_SSCALED
               : ISL_FORMAT_R10G10B10A2_SSCALED;
         }
         return ISL_FORMAT_R10G10B10A2_UINT;
      } else if (glarray->Type == GL_UNSIGNED_INT_2_10_10_10_REV) {
         assert(size == 4);
         if (devinfo->gen >= 8 || devinfo->is_haswell) {
            return glarray->Format == GL_BGRA
               ? ISL_FORMAT_B10G10R10A2_USCALED
               : ISL_FORMAT_R10G10B10A2_USCALED;
         }
         return ISL_FORMAT_R10G10B10A2_UINT;
      }
      assert(glarray->Format == GL_RGBA); /* sanity check */
      switch (glarray->Type) {
      case GL_DOUBLE: return double_types(brw, size, glarray->Doubles);
      case GL_FLOAT: return float_types[size];
      case GL_HALF_FLOAT:
      case GL_HALF_FLOAT_OES:
         if (devinfo->gen < 6 && size == 3)
            return half_float_types[4];
         else
            return half_float_types[size];
      case GL_INT: return int_types_scale[size];
      case GL_SHORT: return short_types_scale[size];
      case GL_BYTE: return byte_types_scale[size];
      case GL_UNSIGNED_INT: return uint_types_scale[size];
      case GL_UNSIGNED_SHORT: return ushort_types_scale[size];
      case GL_UNSIGNED_BYTE: return ubyte_types_scale[size];
      case GL_FIXED:
         if (devinfo->gen >= 8 || devinfo->is_haswell)
            return fixed_point_types[size];

         /* This produces GL_FIXED inputs as values between INT32_MIN and
          * INT32_MAX, which will be scaled down by 1/65536 by the VS.
          */
         return int_types_scale[size];
      default: unreachable("not reached");
      }
   }
}

static void
copy_array_to_vbo_array(struct brw_context *brw,
			struct brw_vertex_element *element,
			int min, int max,
			struct brw_vertex_buffer *buffer,
			GLuint dst_stride)
{
   const int src_stride = element->glarray->StrideB;

   /* If the source stride is zero, we just want to upload the current
    * attribute once and set the buffer's stride to 0.  There's no need
    * to replicate it out.
    */
   if (src_stride == 0) {
      intel_upload_data(brw, element->glarray->Ptr,
                        element->glarray->_ElementSize,
                        element->glarray->_ElementSize,
			&buffer->bo, &buffer->offset);

      buffer->stride = 0;
      buffer->size = element->glarray->_ElementSize;
      return;
   }

   const unsigned char *src = element->glarray->Ptr + min * src_stride;
   int count = max - min + 1;
   GLuint size = count * dst_stride;
   uint8_t *dst = intel_upload_space(brw, size, dst_stride,
                                     &buffer->bo, &buffer->offset);

   /* The GL 4.5 spec says:
    *      "If any enabled array’s buffer binding is zero when DrawArrays or
    *      one of the other drawing commands defined in section 10.4 is called,
    *      the result is undefined."
    *
    * In this case, let's the dst with undefined values
    */
   if (src != NULL) {
      if (dst_stride == src_stride) {
         memcpy(dst, src, size);
      } else {
         while (count--) {
            memcpy(dst, src, dst_stride);
            src += src_stride;
            dst += dst_stride;
         }
      }
   }
   buffer->stride = dst_stride;
   buffer->size = size;
}

void
brw_prepare_vertices(struct brw_context *brw)
{
   const struct gen_device_info *devinfo = &brw->screen->devinfo;
   struct gl_context *ctx = &brw->ctx;
   /* BRW_NEW_VS_PROG_DATA */
   const struct brw_vs_prog_data *vs_prog_data =
      brw_vs_prog_data(brw->vs.base.prog_data);
   GLbitfield64 vs_inputs = vs_prog_data->inputs_read;
   const unsigned char *ptr = NULL;
   GLuint interleaved = 0;
   unsigned int min_index = brw->vb.min_index + brw->basevertex;
   unsigned int max_index = brw->vb.max_index + brw->basevertex;
   unsigned i;
   int delta, j;

   struct brw_vertex_element *upload[VERT_ATTRIB_MAX];
   GLuint nr_uploads = 0;

   /* _NEW_POLYGON
    *
    * On gen6+, edge flags don't end up in the VUE (either in or out of the
    * VS).  Instead, they're uploaded as the last vertex element, and the data
    * is passed sideband through the fixed function units.  So, we need to
    * prepare the vertex buffer for it, but it's not present in inputs_read.
    */
   if (devinfo->gen >= 6 && (ctx->Polygon.FrontMode != GL_FILL ||
                           ctx->Polygon.BackMode != GL_FILL)) {
      vs_inputs |= VERT_BIT_EDGEFLAG;
   }

   if (0)
      fprintf(stderr, "%s %d..%d\n", __func__, min_index, max_index);

   /* Accumulate the list of enabled arrays. */
   brw->vb.nr_enabled = 0;
   while (vs_inputs) {
      GLuint first = ffsll(vs_inputs) - 1;
      assert (first < 64);
      GLuint index =
         first - DIV_ROUND_UP(_mesa_bitcount_64(vs_prog_data->double_inputs_read &
                                                BITFIELD64_MASK(first)), 2);
      struct brw_vertex_element *input = &brw->vb.inputs[index];
      input->is_dual_slot = (vs_prog_data->double_inputs_read & BITFIELD64_BIT(first)) != 0;
      vs_inputs &= ~BITFIELD64_BIT(first);
      if (input->is_dual_slot)
         vs_inputs &= ~BITFIELD64_BIT(first + 1);
      brw->vb.enabled[brw->vb.nr_enabled++] = input;
   }

   if (brw->vb.nr_enabled == 0)
      return;

   if (brw->vb.nr_buffers)
      return;

   /* The range of data in a given buffer represented as [min, max) */
   struct intel_buffer_object *enabled_buffer[VERT_ATTRIB_MAX];
   uint32_t buffer_range_start[VERT_ATTRIB_MAX];
   uint32_t buffer_range_end[VERT_ATTRIB_MAX];

   for (i = j = 0; i < brw->vb.nr_enabled; i++) {
      struct brw_vertex_element *input = brw->vb.enabled[i];
      const struct gl_vertex_array *glarray = input->glarray;

      if (_mesa_is_bufferobj(glarray->BufferObj)) {
	 struct intel_buffer_object *intel_buffer =
	    intel_buffer_object(glarray->BufferObj);

         const uint32_t offset = (uintptr_t)glarray->Ptr;

         /* Start with the worst case */
         uint32_t start = 0;
         uint32_t range = intel_buffer->Base.Size;
         if (glarray->InstanceDivisor) {
            if (brw->num_instances) {
               start = offset + glarray->StrideB * brw->baseinstance;
               range = (glarray->StrideB * ((brw->num_instances - 1) /
                                            glarray->InstanceDivisor) +
                        glarray->_ElementSize);
            }
         } else {
            if (brw->vb.index_bounds_valid) {
               start = offset + min_index * glarray->StrideB;
               range = (glarray->StrideB * (max_index - min_index) +
                        glarray->_ElementSize);
            }
         }

	 /* If we have a VB set to be uploaded for this buffer object
	  * already, reuse that VB state so that we emit fewer
	  * relocations.
	  */
	 unsigned k;
	 for (k = 0; k < i; k++) {
	    const struct gl_vertex_array *other = brw->vb.enabled[k]->glarray;
	    if (glarray->BufferObj == other->BufferObj &&
		glarray->StrideB == other->StrideB &&
		glarray->InstanceDivisor == other->InstanceDivisor &&
		(uintptr_t)(glarray->Ptr - other->Ptr) < glarray->StrideB)
	    {
	       input->buffer = brw->vb.enabled[k]->buffer;
	       input->offset = glarray->Ptr - other->Ptr;

               buffer_range_start[input->buffer] =
                  MIN2(buffer_range_start[input->buffer], start);
               buffer_range_end[input->buffer] =
                  MAX2(buffer_range_end[input->buffer], start + range);
	       break;
	    }
	 }
	 if (k == i) {
	    struct brw_vertex_buffer *buffer = &brw->vb.buffers[j];

	    /* Named buffer object: Just reference its contents directly. */
	    buffer->offset = offset;
	    buffer->stride = glarray->StrideB;
	    buffer->step_rate = glarray->InstanceDivisor;
            buffer->size = glarray->BufferObj->Size - offset;

            enabled_buffer[j] = intel_buffer;
            buffer_range_start[j] = start;
            buffer_range_end[j] = start + range;

	    input->buffer = j++;
	    input->offset = 0;
	 }
      } else {
	 /* Queue the buffer object up to be uploaded in the next pass,
	  * when we've decided if we're doing interleaved or not.
	  */
	 if (nr_uploads == 0) {
	    interleaved = glarray->StrideB;
	    ptr = glarray->Ptr;
	 }
	 else if (interleaved != glarray->StrideB ||
                  glarray->InstanceDivisor != 0 ||
                  glarray->Ptr < ptr ||
                  (uintptr_t)(glarray->Ptr - ptr) + glarray->_ElementSize > interleaved)
	 {
            /* If our stride is different from the first attribute's stride,
             * or if we are using an instance divisor or if the first
             * attribute's stride didn't cover our element, disable the
             * interleaved upload optimization.  The second case can most
             * commonly occur in cases where there is a single vertex and, for
             * example, the data is stored on the application's stack.
             *
             * NOTE: This will also disable the optimization in cases where
             * the data is in a different order than the array indices.
             * Something like:
             *
             *     float data[...];
             *     glVertexAttribPointer(0, 4, GL_FLOAT, 32, &data[4]);
             *     glVertexAttribPointer(1, 4, GL_FLOAT, 32, &data[0]);
             */
	    interleaved = 0;
	 }

	 upload[nr_uploads++] = input;
      }
   }

   /* Now that we've set up all of the buffers, we walk through and reference
    * each of them.  We do this late so that we get the right size in each
    * buffer and don't reference too little data.
    */
   for (i = 0; i < j; i++) {
      struct brw_vertex_buffer *buffer = &brw->vb.buffers[i];
      if (buffer->bo)
         continue;

      const uint32_t start = buffer_range_start[i];
      const uint32_t range = buffer_range_end[i] - buffer_range_start[i];

      buffer->bo = intel_bufferobj_buffer(brw, enabled_buffer[i], start,
                                          range, false);
      brw_bo_reference(buffer->bo);
   }

   /* If we need to upload all the arrays, then we can trim those arrays to
    * only the used elements [min_index, max_index] so long as we adjust all
    * the values used in the 3DPRIMITIVE i.e. by setting the vertex bias.
    */
   brw->vb.start_vertex_bias = 0;
   delta = min_index;
   if (nr_uploads == brw->vb.nr_enabled) {
      brw->vb.start_vertex_bias = -delta;
      delta = 0;
   }

   /* Handle any arrays to be uploaded. */
   if (nr_uploads > 1) {
      if (interleaved) {
	 struct brw_vertex_buffer *buffer = &brw->vb.buffers[j];
	 /* All uploads are interleaved, so upload the arrays together as
	  * interleaved.  First, upload the contents and set up upload[0].
	  */
	 copy_array_to_vbo_array(brw, upload[0], min_index, max_index,
				 buffer, interleaved);
	 buffer->offset -= delta * interleaved;
         buffer->size += delta * interleaved;
         buffer->step_rate = 0;

	 for (i = 0; i < nr_uploads; i++) {
	    /* Then, just point upload[i] at upload[0]'s buffer. */
	    upload[i]->offset =
	       ((const unsigned char *)upload[i]->glarray->Ptr - ptr);
	    upload[i]->buffer = j;
	 }
	 j++;

	 nr_uploads = 0;
      }
   }
   /* Upload non-interleaved arrays */
   for (i = 0; i < nr_uploads; i++) {
      struct brw_vertex_buffer *buffer = &brw->vb.buffers[j];
      if (upload[i]->glarray->InstanceDivisor == 0) {
         copy_array_to_vbo_array(brw, upload[i], min_index, max_index,
                                 buffer, upload[i]->glarray->_ElementSize);
      } else {
         /* This is an instanced attribute, since its InstanceDivisor
          * is not zero. Therefore, its data will be stepped after the
          * instanced draw has been run InstanceDivisor times.
          */
         uint32_t instanced_attr_max_index =
            (brw->num_instances - 1) / upload[i]->glarray->InstanceDivisor;
         copy_array_to_vbo_array(brw, upload[i], 0, instanced_attr_max_index,
                                 buffer, upload[i]->glarray->_ElementSize);
      }
      buffer->offset -= delta * buffer->stride;
      buffer->size += delta * buffer->stride;
      buffer->step_rate = upload[i]->glarray->InstanceDivisor;
      upload[i]->buffer = j++;
      upload[i]->offset = 0;
   }

   brw->vb.nr_buffers = j;
}

void
brw_prepare_shader_draw_parameters(struct brw_context *brw)
{
   const struct brw_vs_prog_data *vs_prog_data =
      brw_vs_prog_data(brw->vs.base.prog_data);

   /* For non-indirect draws, upload gl_BaseVertex. */
   if ((vs_prog_data->uses_basevertex || vs_prog_data->uses_baseinstance) &&
       brw->draw.draw_params_bo == NULL) {
      intel_upload_data(brw, &brw->draw.params, sizeof(brw->draw.params), 4,
			&brw->draw.draw_params_bo,
                        &brw->draw.draw_params_offset);
   }

   if (vs_prog_data->uses_drawid) {
      intel_upload_data(brw, &brw->draw.gl_drawid, sizeof(brw->draw.gl_drawid), 4,
                        &brw->draw.draw_id_bo,
                        &brw->draw.draw_id_offset);
   }
}

static void
brw_upload_indices(struct brw_context *brw)
{
   const struct _mesa_index_buffer *index_buffer = brw->ib.ib;
   GLuint ib_size;
   struct brw_bo *old_bo = brw->ib.bo;
   struct gl_buffer_object *bufferobj;
   GLuint offset;
   GLuint ib_type_size;

   if (index_buffer == NULL)
      return;

   ib_type_size = index_buffer->index_size;
   ib_size = index_buffer->count ? ib_type_size * index_buffer->count :
                                   index_buffer->obj->Size;
   bufferobj = index_buffer->obj;

   /* Turn into a proper VBO:
    */
   if (!_mesa_is_bufferobj(bufferobj)) {
      /* Get new bufferobj, offset:
       */
      intel_upload_data(brw, index_buffer->ptr, ib_size, ib_type_size,
			&brw->ib.bo, &offset);
      brw->ib.size = brw->ib.bo->size;
   } else {
      offset = (GLuint) (unsigned long) index_buffer->ptr;

      struct brw_bo *bo =
         intel_bufferobj_buffer(brw, intel_buffer_object(bufferobj),
                                offset, ib_size, false);
      if (bo != brw->ib.bo) {
         brw_bo_unreference(brw->ib.bo);
         brw->ib.bo = bo;
         brw->ib.size = bufferobj->Size;
         brw_bo_reference(bo);
      }
   }

   /* Use 3DPRIMITIVE's start_vertex_offset to avoid re-uploading
    * the index buffer state when we're just moving the start index
    * of our drawing.
    */
   brw->ib.start_vertex_offset = offset / ib_type_size;

   if (brw->ib.bo != old_bo)
      brw->ctx.NewDriverState |= BRW_NEW_INDEX_BUFFER;

   if (index_buffer->index_size != brw->ib.index_size) {
      brw->ib.index_size = index_buffer->index_size;
      brw->ctx.NewDriverState |= BRW_NEW_INDEX_BUFFER;
   }
}

const struct brw_tracked_state brw_indices = {
   .dirty = {
      .mesa = 0,
      .brw = BRW_NEW_BLORP |
             BRW_NEW_INDICES,
   },
   .emit = brw_upload_indices,
};
