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

#include "radv_meta.h"
#include "nir/nir_builder.h"

struct blit_region {
	VkOffset3D src_offset;
	VkExtent3D src_extent;
	VkOffset3D dest_offset;
	VkExtent3D dest_extent;
};

static nir_shader *
build_nir_vertex_shader(void)
{
	const struct glsl_type *vec4 = glsl_vec4_type();
	nir_builder b;

	nir_builder_init_simple_shader(&b, NULL, MESA_SHADER_VERTEX, NULL);
	b.shader->info.name = ralloc_strdup(b.shader, "meta_blit_vs");

	nir_variable *pos_out = nir_variable_create(b.shader, nir_var_shader_out,
						    vec4, "gl_Position");
	pos_out->data.location = VARYING_SLOT_POS;

	nir_variable *tex_pos_out = nir_variable_create(b.shader, nir_var_shader_out,
							vec4, "v_tex_pos");
	tex_pos_out->data.location = VARYING_SLOT_VAR0;
	tex_pos_out->data.interpolation = INTERP_MODE_SMOOTH;

	nir_ssa_def *outvec = radv_meta_gen_rect_vertices(&b);

	nir_store_var(&b, pos_out, outvec, 0xf);

	nir_intrinsic_instr *src_box = nir_intrinsic_instr_create(b.shader, nir_intrinsic_load_push_constant);
	src_box->src[0] = nir_src_for_ssa(nir_imm_int(&b, 0));
	nir_intrinsic_set_base(src_box, 0);
	nir_intrinsic_set_range(src_box, 16);
	src_box->num_components = 4;
	nir_ssa_dest_init(&src_box->instr, &src_box->dest, 4, 32, "src_box");
	nir_builder_instr_insert(&b, &src_box->instr);

	nir_intrinsic_instr *src0_z = nir_intrinsic_instr_create(b.shader, nir_intrinsic_load_push_constant);
	src0_z->src[0] = nir_src_for_ssa(nir_imm_int(&b, 0));
	nir_intrinsic_set_base(src0_z, 16);
	nir_intrinsic_set_range(src0_z, 4);
	src0_z->num_components = 1;
	nir_ssa_dest_init(&src0_z->instr, &src0_z->dest, 1, 32, "src0_z");
	nir_builder_instr_insert(&b, &src0_z->instr);

	nir_intrinsic_instr *vertex_id = nir_intrinsic_instr_create(b.shader, nir_intrinsic_load_vertex_id_zero_base);
	nir_ssa_dest_init(&vertex_id->instr, &vertex_id->dest, 1, 32, "vertexid");
	nir_builder_instr_insert(&b, &vertex_id->instr);

	/* vertex 0 - src0_x, src0_y, src0_z */
	/* vertex 1 - src0_x, src1_y, src0_z*/
	/* vertex 2 - src1_x, src0_y, src0_z */
	/* so channel 0 is vertex_id != 2 ? src_x : src_x + w
	   channel 1 is vertex id != 1 ? src_y : src_y + w */

	nir_ssa_def *c0cmp = nir_ine(&b, &vertex_id->dest.ssa,
				     nir_imm_int(&b, 2));
	nir_ssa_def *c1cmp = nir_ine(&b, &vertex_id->dest.ssa,
				     nir_imm_int(&b, 1));

	nir_ssa_def *comp[4];
	comp[0] = nir_bcsel(&b, c0cmp,
			    nir_channel(&b, &src_box->dest.ssa, 0),
			    nir_channel(&b, &src_box->dest.ssa, 2));

	comp[1] = nir_bcsel(&b, c1cmp,
			    nir_channel(&b, &src_box->dest.ssa, 1),
			    nir_channel(&b, &src_box->dest.ssa, 3));
	comp[2] = &src0_z->dest.ssa;
	comp[3] = nir_imm_float(&b, 1.0);
	nir_ssa_def *out_tex_vec = nir_vec(&b, comp, 4);
	nir_store_var(&b, tex_pos_out, out_tex_vec, 0xf);
	return b.shader;
}

static nir_shader *
build_nir_copy_fragment_shader(enum glsl_sampler_dim tex_dim)
{
	char shader_name[64];
	const struct glsl_type *vec4 = glsl_vec4_type();
	nir_builder b;

	nir_builder_init_simple_shader(&b, NULL, MESA_SHADER_FRAGMENT, NULL);

	sprintf(shader_name, "meta_blit_fs.%d", tex_dim);
	b.shader->info.name = ralloc_strdup(b.shader, shader_name);

	nir_variable *tex_pos_in = nir_variable_create(b.shader, nir_var_shader_in,
						       vec4, "v_tex_pos");
	tex_pos_in->data.location = VARYING_SLOT_VAR0;

	/* Swizzle the array index which comes in as Z coordinate into the right
	 * position.
	 */
	unsigned swz[] = { 0, (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 1), 2 };
	nir_ssa_def *const tex_pos =
		nir_swizzle(&b, nir_load_var(&b, tex_pos_in), swz,
			    (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 3), false);

	const struct glsl_type *sampler_type =
		glsl_sampler_type(tex_dim, false, tex_dim != GLSL_SAMPLER_DIM_3D,
				  glsl_get_base_type(vec4));
	nir_variable *sampler = nir_variable_create(b.shader, nir_var_uniform,
						    sampler_type, "s_tex");
	sampler->data.descriptor_set = 0;
	sampler->data.binding = 0;

	nir_tex_instr *tex = nir_tex_instr_create(b.shader, 1);
	tex->sampler_dim = tex_dim;
	tex->op = nir_texop_tex;
	tex->src[0].src_type = nir_tex_src_coord;
	tex->src[0].src = nir_src_for_ssa(tex_pos);
	tex->dest_type = nir_type_float; /* TODO */
	tex->is_array = glsl_sampler_type_is_array(sampler_type);
	tex->coord_components = tex_pos->num_components;
	tex->texture = nir_deref_var_create(tex, sampler);
	tex->sampler = nir_deref_var_create(tex, sampler);

	nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, "tex");
	nir_builder_instr_insert(&b, &tex->instr);

	nir_variable *color_out = nir_variable_create(b.shader, nir_var_shader_out,
						      vec4, "f_color");
	color_out->data.location = FRAG_RESULT_DATA0;
	nir_store_var(&b, color_out, &tex->dest.ssa, 0xf);

	return b.shader;
}

static nir_shader *
build_nir_copy_fragment_shader_depth(enum glsl_sampler_dim tex_dim)
{
	char shader_name[64];
	const struct glsl_type *vec4 = glsl_vec4_type();
	nir_builder b;

	nir_builder_init_simple_shader(&b, NULL, MESA_SHADER_FRAGMENT, NULL);

	sprintf(shader_name, "meta_blit_depth_fs.%d", tex_dim);
	b.shader->info.name = ralloc_strdup(b.shader, shader_name);

	nir_variable *tex_pos_in = nir_variable_create(b.shader, nir_var_shader_in,
						       vec4, "v_tex_pos");
	tex_pos_in->data.location = VARYING_SLOT_VAR0;

	/* Swizzle the array index which comes in as Z coordinate into the right
	 * position.
	 */
	unsigned swz[] = { 0, (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 1), 2 };
	nir_ssa_def *const tex_pos =
		nir_swizzle(&b, nir_load_var(&b, tex_pos_in), swz,
			    (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 3), false);

	const struct glsl_type *sampler_type =
		glsl_sampler_type(tex_dim, false, tex_dim != GLSL_SAMPLER_DIM_3D,
				  glsl_get_base_type(vec4));
	nir_variable *sampler = nir_variable_create(b.shader, nir_var_uniform,
						    sampler_type, "s_tex");
	sampler->data.descriptor_set = 0;
	sampler->data.binding = 0;

	nir_tex_instr *tex = nir_tex_instr_create(b.shader, 1);
	tex->sampler_dim = tex_dim;
	tex->op = nir_texop_tex;
	tex->src[0].src_type = nir_tex_src_coord;
	tex->src[0].src = nir_src_for_ssa(tex_pos);
	tex->dest_type = nir_type_float; /* TODO */
	tex->is_array = glsl_sampler_type_is_array(sampler_type);
	tex->coord_components = tex_pos->num_components;
	tex->texture = nir_deref_var_create(tex, sampler);
	tex->sampler = nir_deref_var_create(tex, sampler);

	nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, "tex");
	nir_builder_instr_insert(&b, &tex->instr);

	nir_variable *color_out = nir_variable_create(b.shader, nir_var_shader_out,
						      vec4, "f_color");
	color_out->data.location = FRAG_RESULT_DEPTH;
	nir_store_var(&b, color_out, &tex->dest.ssa, 0x1);

	return b.shader;
}

static nir_shader *
build_nir_copy_fragment_shader_stencil(enum glsl_sampler_dim tex_dim)
{
	char shader_name[64];
	const struct glsl_type *vec4 = glsl_vec4_type();
	nir_builder b;

	nir_builder_init_simple_shader(&b, NULL, MESA_SHADER_FRAGMENT, NULL);

	sprintf(shader_name, "meta_blit_stencil_fs.%d", tex_dim);
	b.shader->info.name = ralloc_strdup(b.shader, shader_name);

	nir_variable *tex_pos_in = nir_variable_create(b.shader, nir_var_shader_in,
						       vec4, "v_tex_pos");
	tex_pos_in->data.location = VARYING_SLOT_VAR0;

	/* Swizzle the array index which comes in as Z coordinate into the right
	 * position.
	 */
	unsigned swz[] = { 0, (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 1), 2 };
	nir_ssa_def *const tex_pos =
		nir_swizzle(&b, nir_load_var(&b, tex_pos_in), swz,
			    (tex_dim == GLSL_SAMPLER_DIM_1D ? 2 : 3), false);

	const struct glsl_type *sampler_type =
		glsl_sampler_type(tex_dim, false, tex_dim != GLSL_SAMPLER_DIM_3D,
				  glsl_get_base_type(vec4));
	nir_variable *sampler = nir_variable_create(b.shader, nir_var_uniform,
						    sampler_type, "s_tex");
	sampler->data.descriptor_set = 0;
	sampler->data.binding = 0;

	nir_tex_instr *tex = nir_tex_instr_create(b.shader, 1);
	tex->sampler_dim = tex_dim;
	tex->op = nir_texop_tex;
	tex->src[0].src_type = nir_tex_src_coord;
	tex->src[0].src = nir_src_for_ssa(tex_pos);
	tex->dest_type = nir_type_float; /* TODO */
	tex->is_array = glsl_sampler_type_is_array(sampler_type);
	tex->coord_components = tex_pos->num_components;
	tex->texture = nir_deref_var_create(tex, sampler);
	tex->sampler = nir_deref_var_create(tex, sampler);

	nir_ssa_dest_init(&tex->instr, &tex->dest, 4, 32, "tex");
	nir_builder_instr_insert(&b, &tex->instr);

	nir_variable *color_out = nir_variable_create(b.shader, nir_var_shader_out,
						      vec4, "f_color");
	color_out->data.location = FRAG_RESULT_STENCIL;
	nir_store_var(&b, color_out, &tex->dest.ssa, 0x1);

	return b.shader;
}

static void
meta_emit_blit(struct radv_cmd_buffer *cmd_buffer,
               struct radv_image *src_image,
               struct radv_image_view *src_iview,
	       VkImageLayout src_image_layout,
               VkOffset3D src_offset_0,
               VkOffset3D src_offset_1,
               struct radv_image *dest_image,
               struct radv_image_view *dest_iview,
	       VkImageLayout dest_image_layout,
               VkOffset2D dest_offset_0,
               VkOffset2D dest_offset_1,
               VkRect2D dest_box,
               VkFilter blit_filter)
{
	struct radv_device *device = cmd_buffer->device;
	uint32_t src_width = radv_minify(src_iview->image->info.width, src_iview->base_mip);
	uint32_t src_height = radv_minify(src_iview->image->info.height, src_iview->base_mip);
	uint32_t src_depth = radv_minify(src_iview->image->info.depth, src_iview->base_mip);
	uint32_t dst_width = radv_minify(dest_iview->image->info.width, dest_iview->base_mip);
	uint32_t dst_height = radv_minify(dest_iview->image->info.height, dest_iview->base_mip);

	assert(src_image->info.samples == dest_image->info.samples);

	float vertex_push_constants[5] = {
		(float)src_offset_0.x / (float)src_width,
		(float)src_offset_0.y / (float)src_height,
		(float)src_offset_1.x / (float)src_width,
		(float)src_offset_1.y / (float)src_height,
		(float)src_offset_0.z / (float)src_depth,
	};

	radv_CmdPushConstants(radv_cmd_buffer_to_handle(cmd_buffer),
			      device->meta_state.blit.pipeline_layout,
			      VK_SHADER_STAGE_VERTEX_BIT, 0, 20,
			      vertex_push_constants);

	VkSampler sampler;
	radv_CreateSampler(radv_device_to_handle(device),
				 &(VkSamplerCreateInfo) {
					 .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
						 .magFilter = blit_filter,
						 .minFilter = blit_filter,
						 .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
						 .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
						 .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
						 }, &cmd_buffer->pool->alloc, &sampler);

	VkFramebuffer fb;
	radv_CreateFramebuffer(radv_device_to_handle(device),
			       &(VkFramebufferCreateInfo) {
				       .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
					       .attachmentCount = 1,
					       .pAttachments = (VkImageView[]) {
					       radv_image_view_to_handle(dest_iview),
				       },
				       .width = dst_width,
				       .height = dst_height,
				       .layers = 1,
				}, &cmd_buffer->pool->alloc, &fb);
	VkPipeline pipeline;
	switch (src_iview->aspect_mask) {
	case VK_IMAGE_ASPECT_COLOR_BIT: {
		unsigned fs_key = radv_format_meta_fs_key(dest_image->vk_format);

		radv_CmdBeginRenderPass(radv_cmd_buffer_to_handle(cmd_buffer),
					      &(VkRenderPassBeginInfo) {
						      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
							      .renderPass = device->meta_state.blit.render_pass[fs_key],
							      .framebuffer = fb,
							      .renderArea = {
							      .offset = { dest_box.offset.x, dest_box.offset.y },
							      .extent = { dest_box.extent.width, dest_box.extent.height },
						      },
							      .clearValueCount = 0,
								       .pClearValues = NULL,
						       }, VK_SUBPASS_CONTENTS_INLINE);
		switch (src_image->type) {
		case VK_IMAGE_TYPE_1D:
			pipeline = device->meta_state.blit.pipeline_1d_src[fs_key];
			break;
		case VK_IMAGE_TYPE_2D:
			pipeline = device->meta_state.blit.pipeline_2d_src[fs_key];
			break;
		case VK_IMAGE_TYPE_3D:
			pipeline = device->meta_state.blit.pipeline_3d_src[fs_key];
			break;
		default:
			unreachable(!"bad VkImageType");
		}
		break;
	}
	case VK_IMAGE_ASPECT_DEPTH_BIT: {
		enum radv_blit_ds_layout ds_layout = radv_meta_blit_ds_to_type(dest_image_layout);
		radv_CmdBeginRenderPass(radv_cmd_buffer_to_handle(cmd_buffer),
					      &(VkRenderPassBeginInfo) {
						      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
							      .renderPass = device->meta_state.blit.depth_only_rp[ds_layout],
							      .framebuffer = fb,
							      .renderArea = {
							      .offset = { dest_box.offset.x, dest_box.offset.y },
							      .extent = { dest_box.extent.width, dest_box.extent.height },
						      },
							      .clearValueCount = 0,
								       .pClearValues = NULL,
						       }, VK_SUBPASS_CONTENTS_INLINE);
		switch (src_image->type) {
		case VK_IMAGE_TYPE_1D:
			pipeline = device->meta_state.blit.depth_only_1d_pipeline;
			break;
		case VK_IMAGE_TYPE_2D:
			pipeline = device->meta_state.blit.depth_only_2d_pipeline;
			break;
		case VK_IMAGE_TYPE_3D:
			pipeline = device->meta_state.blit.depth_only_3d_pipeline;
			break;
		default:
			unreachable(!"bad VkImageType");
		}
		break;
	}
	case VK_IMAGE_ASPECT_STENCIL_BIT: {
		enum radv_blit_ds_layout ds_layout = radv_meta_blit_ds_to_type(dest_image_layout);
		radv_CmdBeginRenderPass(radv_cmd_buffer_to_handle(cmd_buffer),
					      &(VkRenderPassBeginInfo) {
						      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
							      .renderPass = device->meta_state.blit.stencil_only_rp[ds_layout],
							      .framebuffer = fb,
							      .renderArea = {
							      .offset = { dest_box.offset.x, dest_box.offset.y },
							      .extent = { dest_box.extent.width, dest_box.extent.height },
						              },
							      .clearValueCount = 0,
								       .pClearValues = NULL,
						       }, VK_SUBPASS_CONTENTS_INLINE);
		switch (src_image->type) {
		case VK_IMAGE_TYPE_1D:
			pipeline = device->meta_state.blit.stencil_only_1d_pipeline;
			break;
		case VK_IMAGE_TYPE_2D:
			pipeline = device->meta_state.blit.stencil_only_2d_pipeline;
			break;
		case VK_IMAGE_TYPE_3D:
			pipeline = device->meta_state.blit.stencil_only_3d_pipeline;
			break;
		default:
			unreachable(!"bad VkImageType");
		}
		break;
	}
	default:
		unreachable(!"bad VkImageType");
	}

	radv_CmdBindPipeline(radv_cmd_buffer_to_handle(cmd_buffer),
			     VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	radv_meta_push_descriptor_set(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			              device->meta_state.blit.pipeline_layout,
				      0, /* set */
				      1, /* descriptorWriteCount */
				      (VkWriteDescriptorSet[]) {
				              {
				                      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				                      .dstBinding = 0,
				                      .dstArrayElement = 0,
				                      .descriptorCount = 1,
				                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				                      .pImageInfo = (VkDescriptorImageInfo[]) {
				                              {
				                                      .sampler = sampler,
				                                      .imageView = radv_image_view_to_handle(src_iview),
				                                      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				                              },
				                      }
				              }
				      });

	radv_CmdSetViewport(radv_cmd_buffer_to_handle(cmd_buffer), 0, 1, &(VkViewport) {
		.x = dest_offset_0.x,
		.y = dest_offset_0.y,
		.width = dest_offset_1.x - dest_offset_0.x,
		.height = dest_offset_1.y - dest_offset_0.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	});

	radv_CmdSetScissor(radv_cmd_buffer_to_handle(cmd_buffer), 0, 1, &(VkRect2D) {
		.offset = (VkOffset2D) { MIN2(dest_offset_0.x, dest_offset_1.x), MIN2(dest_offset_0.y, dest_offset_1.y) },
		.extent = (VkExtent2D) {
			abs(dest_offset_1.x - dest_offset_0.x),
			abs(dest_offset_1.y - dest_offset_0.y)
		},
	});

	radv_CmdDraw(radv_cmd_buffer_to_handle(cmd_buffer), 3, 1, 0, 0);

	radv_CmdEndRenderPass(radv_cmd_buffer_to_handle(cmd_buffer));

	/* At the point where we emit the draw call, all data from the
	 * descriptor sets, etc. has been used.  We are free to delete it.
	 */
	/* TODO: above comment is not valid for at least descriptor sets/pools,
	 * as we may not free them till after execution finishes. Check others. */

	radv_DestroySampler(radv_device_to_handle(device), sampler,
			    &cmd_buffer->pool->alloc);
	radv_DestroyFramebuffer(radv_device_to_handle(device), fb,
				&cmd_buffer->pool->alloc);
}

static bool
flip_coords(unsigned *src0, unsigned *src1, unsigned *dst0, unsigned *dst1)
{
	bool flip = false;
	if (*src0 > *src1) {
		unsigned tmp = *src0;
		*src0 = *src1;
		*src1 = tmp;
		flip = !flip;
	}

	if (*dst0 > *dst1) {
		unsigned tmp = *dst0;
		*dst0 = *dst1;
		*dst1 = tmp;
		flip = !flip;
	}
	return flip;
}

void radv_CmdBlitImage(
	VkCommandBuffer                             commandBuffer,
	VkImage                                     srcImage,
	VkImageLayout                               srcImageLayout,
	VkImage                                     destImage,
	VkImageLayout                               destImageLayout,
	uint32_t                                    regionCount,
	const VkImageBlit*                          pRegions,
	VkFilter                                    filter)

{
	RADV_FROM_HANDLE(radv_cmd_buffer, cmd_buffer, commandBuffer);
	RADV_FROM_HANDLE(radv_image, src_image, srcImage);
	RADV_FROM_HANDLE(radv_image, dest_image, destImage);
	struct radv_meta_saved_state saved_state;

	/* From the Vulkan 1.0 spec:
	 *
	 *    vkCmdBlitImage must not be used for multisampled source or
	 *    destination images. Use vkCmdResolveImage for this purpose.
	 */
	assert(src_image->info.samples == 1);
	assert(dest_image->info.samples == 1);

	radv_meta_save(&saved_state, cmd_buffer,
		       RADV_META_SAVE_GRAPHICS_PIPELINE |
		       RADV_META_SAVE_CONSTANTS |
		       RADV_META_SAVE_DESCRIPTORS);

	for (unsigned r = 0; r < regionCount; r++) {
		const VkImageSubresourceLayers *src_res = &pRegions[r].srcSubresource;
		const VkImageSubresourceLayers *dst_res = &pRegions[r].dstSubresource;

		unsigned dst_start, dst_end;
		if (dest_image->type == VK_IMAGE_TYPE_3D) {
			assert(dst_res->baseArrayLayer == 0);
			dst_start = pRegions[r].dstOffsets[0].z;
			dst_end = pRegions[r].dstOffsets[1].z;
		} else {
			dst_start = dst_res->baseArrayLayer;
			dst_end = dst_start + dst_res->layerCount;
		}

		unsigned src_start, src_end;
		if (src_image->type == VK_IMAGE_TYPE_3D) {
			assert(src_res->baseArrayLayer == 0);
			src_start = pRegions[r].srcOffsets[0].z;
			src_end = pRegions[r].srcOffsets[1].z;
		} else {
			src_start = src_res->baseArrayLayer;
			src_end = src_start + src_res->layerCount;
		}

		bool flip_z = flip_coords(&src_start, &src_end, &dst_start, &dst_end);
		float src_z_step = (float)(src_end + 1 - src_start) /
			(float)(dst_end + 1 - dst_start);

		if (flip_z) {
			src_start = src_end;
			src_z_step *= -1;
		}

		unsigned src_x0 = pRegions[r].srcOffsets[0].x;
		unsigned src_x1 = pRegions[r].srcOffsets[1].x;
		unsigned dst_x0 = pRegions[r].dstOffsets[0].x;
		unsigned dst_x1 = pRegions[r].dstOffsets[1].x;

		unsigned src_y0 = pRegions[r].srcOffsets[0].y;
		unsigned src_y1 = pRegions[r].srcOffsets[1].y;
		unsigned dst_y0 = pRegions[r].dstOffsets[0].y;
		unsigned dst_y1 = pRegions[r].dstOffsets[1].y;

		VkRect2D dest_box;
		dest_box.offset.x = MIN2(dst_x0, dst_x1);
		dest_box.offset.y = MIN2(dst_y0, dst_y1);
		dest_box.extent.width = abs(dst_x1 - dst_x0);
		dest_box.extent.height = abs(dst_y1 - dst_y0);

		const unsigned num_layers = dst_end - dst_start;
		for (unsigned i = 0; i < num_layers; i++) {
			struct radv_image_view dest_iview, src_iview;

			const VkOffset2D dest_offset_0 = {
				.x = dst_x0,
				.y = dst_y0,
			};
			const VkOffset2D dest_offset_1 = {
				.x = dst_x1,
				.y = dst_y1,
			};
			VkOffset3D src_offset_0 = {
				.x = src_x0,
				.y = src_y0,
				.z = src_start + i * src_z_step,
			};
			VkOffset3D src_offset_1 = {
				.x = src_x1,
				.y = src_y1,
				.z = src_start + i * src_z_step,
			};
			const uint32_t dest_array_slice = dst_start + i;

			/* 3D images have just 1 layer */
			const uint32_t src_array_slice = src_image->type == VK_IMAGE_TYPE_3D ? 0 : src_start + i;

			radv_image_view_init(&dest_iview, cmd_buffer->device,
					     &(VkImageViewCreateInfo) {
						     .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
							     .image = destImage,
							     .viewType = radv_meta_get_view_type(dest_image),
							     .format = dest_image->vk_format,
							     .subresourceRange = {
							     .aspectMask = dst_res->aspectMask,
							     .baseMipLevel = dst_res->mipLevel,
							     .levelCount = 1,
							     .baseArrayLayer = dest_array_slice,
							     .layerCount = 1
						     },
					     });
			radv_image_view_init(&src_iview, cmd_buffer->device,
					     &(VkImageViewCreateInfo) {
						.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
							.image = srcImage,
							.viewType = radv_meta_get_view_type(src_image),
							.format = src_image->vk_format,
							.subresourceRange = {
							.aspectMask = src_res->aspectMask,
							.baseMipLevel = src_res->mipLevel,
							.levelCount = 1,
							.baseArrayLayer = src_array_slice,
							.layerCount = 1
						},
					});
			meta_emit_blit(cmd_buffer,
				       src_image, &src_iview, srcImageLayout,
				       src_offset_0, src_offset_1,
				       dest_image, &dest_iview, destImageLayout,
				       dest_offset_0, dest_offset_1,
				       dest_box,
				       filter);
		}
	}

	radv_meta_restore(&saved_state, cmd_buffer);
}

void
radv_device_finish_meta_blit_state(struct radv_device *device)
{
	struct radv_meta_state *state = &device->meta_state;

	for (unsigned i = 0; i < NUM_META_FS_KEYS; ++i) {
		radv_DestroyRenderPass(radv_device_to_handle(device),
				       state->blit.render_pass[i],
				       &state->alloc);
		radv_DestroyPipeline(radv_device_to_handle(device),
				     state->blit.pipeline_1d_src[i],
				     &state->alloc);
		radv_DestroyPipeline(radv_device_to_handle(device),
				     state->blit.pipeline_2d_src[i],
				     &state->alloc);
		radv_DestroyPipeline(radv_device_to_handle(device),
				     state->blit.pipeline_3d_src[i],
				     &state->alloc);
	}

	for (enum radv_blit_ds_layout i = RADV_BLIT_DS_LAYOUT_TILE_ENABLE; i < RADV_BLIT_DS_LAYOUT_COUNT; i++) {
		radv_DestroyRenderPass(radv_device_to_handle(device),
				       state->blit.depth_only_rp[i], &state->alloc);
		radv_DestroyRenderPass(radv_device_to_handle(device),
				       state->blit.stencil_only_rp[i], &state->alloc);
	}

	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.depth_only_1d_pipeline, &state->alloc);
	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.depth_only_2d_pipeline, &state->alloc);
	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.depth_only_3d_pipeline, &state->alloc);

	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.stencil_only_1d_pipeline,
			     &state->alloc);
	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.stencil_only_2d_pipeline,
			     &state->alloc);
	radv_DestroyPipeline(radv_device_to_handle(device),
			     state->blit.stencil_only_3d_pipeline,
			     &state->alloc);


	radv_DestroyPipelineLayout(radv_device_to_handle(device),
				   state->blit.pipeline_layout, &state->alloc);
	radv_DestroyDescriptorSetLayout(radv_device_to_handle(device),
					state->blit.ds_layout, &state->alloc);
}

static VkFormat pipeline_formats[] = {
   VK_FORMAT_R8G8B8A8_UNORM,
   VK_FORMAT_R8G8B8A8_UINT,
   VK_FORMAT_R8G8B8A8_SINT,
   VK_FORMAT_A2R10G10B10_UINT_PACK32,
   VK_FORMAT_A2R10G10B10_SINT_PACK32,
   VK_FORMAT_R16G16B16A16_UNORM,
   VK_FORMAT_R16G16B16A16_SNORM,
   VK_FORMAT_R16G16B16A16_UINT,
   VK_FORMAT_R16G16B16A16_SINT,
   VK_FORMAT_R32_SFLOAT,
   VK_FORMAT_R32G32_SFLOAT,
   VK_FORMAT_R32G32B32A32_SFLOAT
};

static VkResult
radv_device_init_meta_blit_color(struct radv_device *device,
				 struct radv_shader_module *vs)
{
	struct radv_shader_module fs_1d = {0}, fs_2d = {0}, fs_3d = {0};
	VkResult result;

	fs_1d.nir = build_nir_copy_fragment_shader(GLSL_SAMPLER_DIM_1D);
	fs_2d.nir = build_nir_copy_fragment_shader(GLSL_SAMPLER_DIM_2D);
	fs_3d.nir = build_nir_copy_fragment_shader(GLSL_SAMPLER_DIM_3D);

	for (unsigned i = 0; i < ARRAY_SIZE(pipeline_formats); ++i) {
		unsigned key = radv_format_meta_fs_key(pipeline_formats[i]);
		result = radv_CreateRenderPass(radv_device_to_handle(device),
					&(VkRenderPassCreateInfo) {
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
							.attachmentCount = 1,
							.pAttachments = &(VkAttachmentDescription) {
							.format = pipeline_formats[i],
							.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
							.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
							.initialLayout = VK_IMAGE_LAYOUT_GENERAL,
							.finalLayout = VK_IMAGE_LAYOUT_GENERAL,
						},
							.subpassCount = 1,
									.pSubpasses = &(VkSubpassDescription) {
							.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
							.inputAttachmentCount = 0,
							.colorAttachmentCount = 1,
							.pColorAttachments = &(VkAttachmentReference) {
								.attachment = 0,
								.layout = VK_IMAGE_LAYOUT_GENERAL,
							},
							.pResolveAttachments = NULL,
							.pDepthStencilAttachment = &(VkAttachmentReference) {
								.attachment = VK_ATTACHMENT_UNUSED,
								.layout = VK_IMAGE_LAYOUT_GENERAL,
							},
							.preserveAttachmentCount = 1,
							.pPreserveAttachments = (uint32_t[]) { 0 },
						},
						.dependencyCount = 0,
					}, &device->meta_state.alloc, &device->meta_state.blit.render_pass[key]);
		if (result != VK_SUCCESS)
			goto fail;

		VkPipelineVertexInputStateCreateInfo vi_create_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 0,
			.vertexAttributeDescriptionCount = 0,
		};

		VkPipelineShaderStageCreateInfo pipeline_shader_stages[] = {
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = radv_shader_module_to_handle(vs),
				.pName = "main",
				.pSpecializationInfo = NULL
			}, {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = VK_NULL_HANDLE, /* TEMPLATE VALUE! FILL ME IN! */
				.pName = "main",
				.pSpecializationInfo = NULL
			},
		};

		const VkGraphicsPipelineCreateInfo vk_pipeline_info = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = ARRAY_SIZE(pipeline_shader_stages),
			.pStages = pipeline_shader_stages,
			.pVertexInputState = &vi_create_info,
			.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
				.primitiveRestartEnable = false,
			},
			.pViewportState = &(VkPipelineViewportStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount = 1,
				.scissorCount = 1,
			},
			.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.rasterizerDiscardEnable = false,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
			},
			.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = 1,
				.sampleShadingEnable = false,
				.pSampleMask = (VkSampleMask[]) { UINT32_MAX },
			},
			.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.attachmentCount = 1,
				.pAttachments = (VkPipelineColorBlendAttachmentState []) {
					{ .colorWriteMask =
					VK_COLOR_COMPONENT_A_BIT |
					VK_COLOR_COMPONENT_R_BIT |
					VK_COLOR_COMPONENT_G_BIT |
					VK_COLOR_COMPONENT_B_BIT },
				}
			},
			.pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				.dynamicStateCount = 4,
				.pDynamicStates = (VkDynamicState[]) {
					VK_DYNAMIC_STATE_VIEWPORT,
					VK_DYNAMIC_STATE_SCISSOR,
					VK_DYNAMIC_STATE_LINE_WIDTH,
					VK_DYNAMIC_STATE_BLEND_CONSTANTS,
				},
			},
			.flags = 0,
			.layout = device->meta_state.blit.pipeline_layout,
			.renderPass = device->meta_state.blit.render_pass[key],
			.subpass = 0,
		};

		const struct radv_graphics_pipeline_create_info radv_pipeline_info = {
			.use_rectlist = true
		};

		pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_1d);
		result = radv_graphics_pipeline_create(radv_device_to_handle(device),
						radv_pipeline_cache_to_handle(&device->meta_state.cache),
						&vk_pipeline_info, &radv_pipeline_info,
						&device->meta_state.alloc, &device->meta_state.blit.pipeline_1d_src[key]);
		if (result != VK_SUCCESS)
			goto fail;

		pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_2d);
		result = radv_graphics_pipeline_create(radv_device_to_handle(device),
						radv_pipeline_cache_to_handle(&device->meta_state.cache),
						&vk_pipeline_info, &radv_pipeline_info,
						&device->meta_state.alloc, &device->meta_state.blit.pipeline_2d_src[key]);
		if (result != VK_SUCCESS)
			goto fail;

		pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_3d);
		result = radv_graphics_pipeline_create(radv_device_to_handle(device),
						radv_pipeline_cache_to_handle(&device->meta_state.cache),
						&vk_pipeline_info, &radv_pipeline_info,
						&device->meta_state.alloc, &device->meta_state.blit.pipeline_3d_src[key]);
		if (result != VK_SUCCESS)
			goto fail;

	}

	result = VK_SUCCESS;
fail:
	ralloc_free(fs_1d.nir);
	ralloc_free(fs_2d.nir);
	ralloc_free(fs_3d.nir);
	return result;
}

static VkResult
radv_device_init_meta_blit_depth(struct radv_device *device,
				 struct radv_shader_module *vs)
{
	struct radv_shader_module fs_1d = {0}, fs_2d = {0}, fs_3d = {0};
	VkResult result;

	fs_1d.nir = build_nir_copy_fragment_shader_depth(GLSL_SAMPLER_DIM_1D);
	fs_2d.nir = build_nir_copy_fragment_shader_depth(GLSL_SAMPLER_DIM_2D);
	fs_3d.nir = build_nir_copy_fragment_shader_depth(GLSL_SAMPLER_DIM_3D);

	for (enum radv_blit_ds_layout ds_layout = RADV_BLIT_DS_LAYOUT_TILE_ENABLE; ds_layout < RADV_BLIT_DS_LAYOUT_COUNT; ds_layout++) {
		VkImageLayout layout = radv_meta_blit_ds_to_layout(ds_layout);
		result = radv_CreateRenderPass(radv_device_to_handle(device),
					       &(VkRenderPassCreateInfo) {
						       .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
						       .attachmentCount = 1,
						       .pAttachments = &(VkAttachmentDescription) {
							       .format = VK_FORMAT_D32_SFLOAT,
							       .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
							       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
							       .initialLayout = layout,
							       .finalLayout = layout,
						       },
						       .subpassCount = 1,
						       .pSubpasses = &(VkSubpassDescription) {
							       .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
							       .inputAttachmentCount = 0,
							       .colorAttachmentCount = 0,
							       .pColorAttachments = NULL,
							       .pResolveAttachments = NULL,
							       .pDepthStencilAttachment = &(VkAttachmentReference) {
								       .attachment = 0,
								       .layout = layout,
								},
							       .preserveAttachmentCount = 1,
							       .pPreserveAttachments = (uint32_t[]) { 0 },
							},
						        .dependencyCount = 0,
						}, &device->meta_state.alloc, &device->meta_state.blit.depth_only_rp[ds_layout]);
		if (result != VK_SUCCESS)
			goto fail;
	}

	VkPipelineVertexInputStateCreateInfo vi_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0,
	};

	VkPipelineShaderStageCreateInfo pipeline_shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = radv_shader_module_to_handle(vs),
			.pName = "main",
			.pSpecializationInfo = NULL
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = VK_NULL_HANDLE, /* TEMPLATE VALUE! FILL ME IN! */
			.pName = "main",
			.pSpecializationInfo = NULL
		},
	};

	const VkGraphicsPipelineCreateInfo vk_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = ARRAY_SIZE(pipeline_shader_stages),
		.pStages = pipeline_shader_stages,
		.pVertexInputState = &vi_create_info,
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			.primitiveRestartEnable = false,
		},
		.pViewportState = &(VkPipelineViewportStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.rasterizerDiscardEnable = false,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = 1,
			.sampleShadingEnable = false,
			.pSampleMask = (VkSampleMask[]) { UINT32_MAX },
		},
		.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 0,
			.pAttachments = NULL,
		},
		.pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthCompareOp = VK_COMPARE_OP_ALWAYS,
		},
		.pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 9,
			.pDynamicStates = (VkDynamicState[]) {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_LINE_WIDTH,
				VK_DYNAMIC_STATE_DEPTH_BIAS,
				VK_DYNAMIC_STATE_BLEND_CONSTANTS,
				VK_DYNAMIC_STATE_DEPTH_BOUNDS,
				VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
				VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
				VK_DYNAMIC_STATE_STENCIL_REFERENCE,
			},
		},
		.flags = 0,
		.layout = device->meta_state.blit.pipeline_layout,
		.renderPass = device->meta_state.blit.depth_only_rp[0],
		.subpass = 0,
	};

	const struct radv_graphics_pipeline_create_info radv_pipeline_info = {
		.use_rectlist = true
	};

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_1d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.depth_only_1d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_2d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.depth_only_2d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_3d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.depth_only_3d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;

fail:
	ralloc_free(fs_1d.nir);
	ralloc_free(fs_2d.nir);
	ralloc_free(fs_3d.nir);
	return result;
}

static VkResult
radv_device_init_meta_blit_stencil(struct radv_device *device,
				   struct radv_shader_module *vs)
{
	struct radv_shader_module fs_1d = {0}, fs_2d = {0}, fs_3d = {0};
	VkResult result;

	fs_1d.nir = build_nir_copy_fragment_shader_stencil(GLSL_SAMPLER_DIM_1D);
	fs_2d.nir = build_nir_copy_fragment_shader_stencil(GLSL_SAMPLER_DIM_2D);
	fs_3d.nir = build_nir_copy_fragment_shader_stencil(GLSL_SAMPLER_DIM_3D);

	for (enum radv_blit_ds_layout ds_layout = RADV_BLIT_DS_LAYOUT_TILE_ENABLE; ds_layout < RADV_BLIT_DS_LAYOUT_COUNT; ds_layout++) {
		VkImageLayout layout = radv_meta_blit_ds_to_layout(ds_layout);
		result = radv_CreateRenderPass(radv_device_to_handle(device),
					       &(VkRenderPassCreateInfo) {
						       .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
						       .attachmentCount = 1,
						       .pAttachments = &(VkAttachmentDescription) {
							       .format = VK_FORMAT_S8_UINT,
							       .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
							       .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
							       .initialLayout = layout,
							       .finalLayout = layout,
						       },
						       .subpassCount = 1,
						       .pSubpasses = &(VkSubpassDescription) {
							       .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
							       .inputAttachmentCount = 0,
							       .colorAttachmentCount = 0,
							       .pColorAttachments = NULL,
							       .pResolveAttachments = NULL,
							       .pDepthStencilAttachment = &(VkAttachmentReference) {
								       .attachment = 0,
								       .layout = layout,
							       },
							       .preserveAttachmentCount = 1,
							       .pPreserveAttachments = (uint32_t[]) { 0 },
						       },
						       .dependencyCount = 0,
					 }, &device->meta_state.alloc, &device->meta_state.blit.stencil_only_rp[ds_layout]);
	}
	if (result != VK_SUCCESS)
		goto fail;

	VkPipelineVertexInputStateCreateInfo vi_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0,
	};

	VkPipelineShaderStageCreateInfo pipeline_shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = radv_shader_module_to_handle(vs),
			.pName = "main",
			.pSpecializationInfo = NULL
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = VK_NULL_HANDLE, /* TEMPLATE VALUE! FILL ME IN! */
			.pName = "main",
			.pSpecializationInfo = NULL
		},
	};

	const VkGraphicsPipelineCreateInfo vk_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = ARRAY_SIZE(pipeline_shader_stages),
		.pStages = pipeline_shader_stages,
		.pVertexInputState = &vi_create_info,
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			.primitiveRestartEnable = false,
		},
		.pViewportState = &(VkPipelineViewportStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.rasterizerDiscardEnable = false,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = 1,
			.sampleShadingEnable = false,
			.pSampleMask = (VkSampleMask[]) { UINT32_MAX },
		},
		.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 0,
			.pAttachments = NULL,
		},
		.pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = false,
			.depthWriteEnable = false,
			.stencilTestEnable = true,
			.front = {
				.failOp = VK_STENCIL_OP_REPLACE,
				.passOp = VK_STENCIL_OP_REPLACE,
				.depthFailOp = VK_STENCIL_OP_REPLACE,
				.compareOp = VK_COMPARE_OP_ALWAYS,
				.compareMask = 0xff,
				.writeMask = 0xff,
				.reference = 0
			},
			.back = {
				.failOp = VK_STENCIL_OP_REPLACE,
				.passOp = VK_STENCIL_OP_REPLACE,
				.depthFailOp = VK_STENCIL_OP_REPLACE,
				.compareOp = VK_COMPARE_OP_ALWAYS,
				.compareMask = 0xff,
				.writeMask = 0xff,
				.reference = 0
			},
			.depthCompareOp = VK_COMPARE_OP_ALWAYS,
		},
		.pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 6,
			.pDynamicStates = (VkDynamicState[]) {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR,
				VK_DYNAMIC_STATE_LINE_WIDTH,
				VK_DYNAMIC_STATE_DEPTH_BIAS,
				VK_DYNAMIC_STATE_BLEND_CONSTANTS,
				VK_DYNAMIC_STATE_DEPTH_BOUNDS,
			},
		},
		.flags = 0,
		.layout = device->meta_state.blit.pipeline_layout,
		.renderPass = device->meta_state.blit.stencil_only_rp[0],
		.subpass = 0,
	};

	const struct radv_graphics_pipeline_create_info radv_pipeline_info = {
		.use_rectlist = true
	};

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_1d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.stencil_only_1d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_2d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.stencil_only_2d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;

	pipeline_shader_stages[1].module = radv_shader_module_to_handle(&fs_3d);
	result = radv_graphics_pipeline_create(radv_device_to_handle(device),
					       radv_pipeline_cache_to_handle(&device->meta_state.cache),
					       &vk_pipeline_info, &radv_pipeline_info,
					       &device->meta_state.alloc, &device->meta_state.blit.stencil_only_3d_pipeline);
	if (result != VK_SUCCESS)
		goto fail;


fail:
	ralloc_free(fs_1d.nir);
	ralloc_free(fs_2d.nir);
	ralloc_free(fs_3d.nir);
	return result;
}

VkResult
radv_device_init_meta_blit_state(struct radv_device *device)
{
	VkResult result;
	struct radv_shader_module vs = {0};

	VkDescriptorSetLayoutCreateInfo ds_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = 1,
		.pBindings = (VkDescriptorSetLayoutBinding[]) {
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = NULL
			},
		}
	};
	result = radv_CreateDescriptorSetLayout(radv_device_to_handle(device),
						&ds_layout_info,
						&device->meta_state.alloc,
						&device->meta_state.blit.ds_layout);
	if (result != VK_SUCCESS)
		goto fail;

	const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_VERTEX_BIT, 0, 20};

	result = radv_CreatePipelineLayout(radv_device_to_handle(device),
					   &(VkPipelineLayoutCreateInfo) {
						   .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
							   .setLayoutCount = 1,
							   .pSetLayouts = &device->meta_state.blit.ds_layout,
							   .pushConstantRangeCount = 1,
							   .pPushConstantRanges = &push_constant_range,
							   },
					   &device->meta_state.alloc, &device->meta_state.blit.pipeline_layout);
	if (result != VK_SUCCESS)
		goto fail;

	vs.nir = build_nir_vertex_shader();

	result = radv_device_init_meta_blit_color(device, &vs);
	if (result != VK_SUCCESS)
		goto fail;

	result = radv_device_init_meta_blit_depth(device, &vs);
	if (result != VK_SUCCESS)
		goto fail;

	result = radv_device_init_meta_blit_stencil(device, &vs);

fail:
	ralloc_free(vs.nir);
	if (result != VK_SUCCESS)
		radv_device_finish_meta_blit_state(device);
	return result;
}
