/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef AC_SHADER_ABI_H
#define AC_SHADER_ABI_H

#include <llvm-c/Core.h>

enum ac_descriptor_type {
	AC_DESC_IMAGE,
	AC_DESC_FMASK,
	AC_DESC_SAMPLER,
	AC_DESC_BUFFER,
};

/* Document the shader ABI during compilation. This is what allows radeonsi and
 * radv to share a compiler backend.
 */
struct ac_shader_abi {
	LLVMValueRef base_vertex;
	LLVMValueRef start_instance;
	LLVMValueRef draw_id;
	LLVMValueRef vertex_id;
	LLVMValueRef instance_id;
	LLVMValueRef frag_pos[4];
	LLVMValueRef front_face;
	LLVMValueRef ancillary;
	LLVMValueRef sample_coverage;

	/* For VS and PS: pre-loaded shader inputs.
	 *
	 * Currently only used for NIR shaders; indexed by variables'
	 * driver_location.
	 */
	LLVMValueRef *inputs;

	void (*emit_outputs)(struct ac_shader_abi *abi,
			     unsigned max_outputs,
			     LLVMValueRef *addrs);

	LLVMValueRef (*load_ubo)(struct ac_shader_abi *abi, LLVMValueRef index);

	/**
	 * Load the descriptor for the given buffer.
	 *
	 * \param buffer the buffer as presented in NIR: this is the descriptor
	 *               in Vulkan, and the buffer index in OpenGL/Gallium
	 * \param write whether buffer contents will be written
	 */
	LLVMValueRef (*load_ssbo)(struct ac_shader_abi *abi,
				  LLVMValueRef buffer, bool write);

	/**
	 * Load a descriptor associated to a sampler.
	 *
	 * \param descriptor_set the descriptor set index (only for Vulkan)
	 * \param base_index the base index of the sampler variable
	 * \param constant_index constant part of an array index (or 0, if the
	 *                       sampler variable is not an array)
	 * \param index non-constant part of an array index (may be NULL)
	 * \param desc_type the type of descriptor to load
	 * \param image whether the descriptor is loaded for an image operation
	 */
	LLVMValueRef (*load_sampler_desc)(struct ac_shader_abi *abi,
					  unsigned descriptor_set,
					  unsigned base_index,
					  unsigned constant_index,
					  LLVMValueRef index,
					  enum ac_descriptor_type desc_type,
					  bool image, bool write);

	/* Whether to clamp the shadow reference value to [0,1]on VI. Radeonsi currently
	 * uses it due to promoting D16 to D32, but radv needs it off. */
	bool clamp_shadow_reference;

	/* Whether to workaround GFX9 ignoring the stride for the buffer size if IDXEN=0
	 * and LLVM optimizes an indexed load with constant index to IDXEN=0. */
	bool gfx9_stride_size_workaround;
};

#endif /* AC_SHADER_ABI_H */
