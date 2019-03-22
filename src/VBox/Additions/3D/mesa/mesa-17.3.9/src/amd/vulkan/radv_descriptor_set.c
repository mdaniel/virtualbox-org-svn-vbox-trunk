/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
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
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "util/mesa-sha1.h"
#include "radv_private.h"
#include "sid.h"

VkResult radv_CreateDescriptorSetLayout(
	VkDevice                                    _device,
	const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkDescriptorSetLayout*                      pSetLayout)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	struct radv_descriptor_set_layout *set_layout;

	assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);

	uint32_t max_binding = 0;
	uint32_t immutable_sampler_count = 0;
	for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
		max_binding = MAX2(max_binding, pCreateInfo->pBindings[j].binding);
		if (pCreateInfo->pBindings[j].pImmutableSamplers)
			immutable_sampler_count += pCreateInfo->pBindings[j].descriptorCount;
	}

	uint32_t samplers_offset = sizeof(struct radv_descriptor_set_layout) +
		(max_binding + 1) * sizeof(set_layout->binding[0]);
	size_t size = samplers_offset + immutable_sampler_count * 4 * sizeof(uint32_t);

	set_layout = vk_alloc2(&device->alloc, pAllocator, size, 8,
				 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (!set_layout)
		return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

	set_layout->flags = pCreateInfo->flags;

	/* We just allocate all the samplers at the end of the struct */
	uint32_t *samplers = (uint32_t*)&set_layout->binding[max_binding + 1];

	set_layout->binding_count = max_binding + 1;
	set_layout->shader_stages = 0;
	set_layout->dynamic_shader_stages = 0;
	set_layout->has_immutable_samplers = false;
	set_layout->size = 0;

	memset(set_layout->binding, 0, size - sizeof(struct radv_descriptor_set_layout));

	uint32_t buffer_count = 0;
	uint32_t dynamic_offset_count = 0;

	for (uint32_t j = 0; j < pCreateInfo->bindingCount; j++) {
		const VkDescriptorSetLayoutBinding *binding = &pCreateInfo->pBindings[j];
		uint32_t b = binding->binding;
		uint32_t alignment;
		unsigned binding_buffer_count = 0;

		switch (binding->descriptorType) {
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			assert(!(pCreateInfo->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
			set_layout->binding[b].dynamic_offset_count = 1;
			set_layout->dynamic_shader_stages |= binding->stageFlags;
			set_layout->binding[b].size = 0;
			binding_buffer_count = 1;
			alignment = 1;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			set_layout->binding[b].size = 16;
			binding_buffer_count = 1;
			alignment = 16;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			/* main descriptor + fmask descriptor */
			set_layout->binding[b].size = 64;
			binding_buffer_count = 1;
			alignment = 32;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			/* main descriptor + fmask descriptor + sampler */
			set_layout->binding[b].size = 96;
			binding_buffer_count = 1;
			alignment = 32;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			set_layout->binding[b].size = 16;
			alignment = 16;
			break;
		default:
			unreachable("unknown descriptor type\n");
			break;
		}

		set_layout->size = align(set_layout->size, alignment);
		assert(binding->descriptorCount > 0);
		set_layout->binding[b].type = binding->descriptorType;
		set_layout->binding[b].array_size = binding->descriptorCount;
		set_layout->binding[b].offset = set_layout->size;
		set_layout->binding[b].buffer_offset = buffer_count;
		set_layout->binding[b].dynamic_offset_offset = dynamic_offset_count;

		if (binding->pImmutableSamplers) {
			set_layout->binding[b].immutable_samplers_offset = samplers_offset;
			set_layout->binding[b].immutable_samplers_equal = true;
			set_layout->has_immutable_samplers = true;


			for (uint32_t i = 0; i < binding->descriptorCount; i++)
				memcpy(samplers + 4 * i, &radv_sampler_from_handle(binding->pImmutableSamplers[i])->state, 16);
			for (uint32_t i = 1; i < binding->descriptorCount; i++)
				if (memcmp(samplers + 4 * i, samplers, 16) != 0)
					set_layout->binding[b].immutable_samplers_equal = false;

			/* Don't reserve space for the samplers if they're not accessed. */
			if (set_layout->binding[b].immutable_samplers_equal) {
				if (binding->descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
					set_layout->binding[b].size -= 32;
				else if (binding->descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
					set_layout->binding[b].size -= 16;
			}
			samplers += 4 * binding->descriptorCount;
			samplers_offset += 4 * sizeof(uint32_t) * binding->descriptorCount;
		}

		set_layout->size += binding->descriptorCount * set_layout->binding[b].size;
		buffer_count += binding->descriptorCount * binding_buffer_count;
		dynamic_offset_count += binding->descriptorCount *
			set_layout->binding[b].dynamic_offset_count;
		set_layout->shader_stages |= binding->stageFlags;
	}

	set_layout->buffer_count = buffer_count;
	set_layout->dynamic_offset_count = dynamic_offset_count;

	*pSetLayout = radv_descriptor_set_layout_to_handle(set_layout);

	return VK_SUCCESS;
}

void radv_DestroyDescriptorSetLayout(
	VkDevice                                    _device,
	VkDescriptorSetLayout                       _set_layout,
	const VkAllocationCallbacks*                pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_set_layout, set_layout, _set_layout);

	if (!set_layout)
		return;

	vk_free2(&device->alloc, pAllocator, set_layout);
}

/*
 * Pipeline layouts.  These have nothing to do with the pipeline.  They are
 * just muttiple descriptor set layouts pasted together
 */

VkResult radv_CreatePipelineLayout(
	VkDevice                                    _device,
	const VkPipelineLayoutCreateInfo*           pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkPipelineLayout*                           pPipelineLayout)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	struct radv_pipeline_layout *layout;
	struct mesa_sha1 ctx;

	assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);

	layout = vk_alloc2(&device->alloc, pAllocator, sizeof(*layout), 8,
			     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (layout == NULL)
		return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

	layout->num_sets = pCreateInfo->setLayoutCount;

	unsigned dynamic_offset_count = 0;


	_mesa_sha1_init(&ctx);
	for (uint32_t set = 0; set < pCreateInfo->setLayoutCount; set++) {
		RADV_FROM_HANDLE(radv_descriptor_set_layout, set_layout,
				 pCreateInfo->pSetLayouts[set]);
		layout->set[set].layout = set_layout;

		layout->set[set].dynamic_offset_start = dynamic_offset_count;
		for (uint32_t b = 0; b < set_layout->binding_count; b++) {
			dynamic_offset_count += set_layout->binding[b].array_size * set_layout->binding[b].dynamic_offset_count;
			if (set_layout->binding[b].immutable_samplers_offset)
				_mesa_sha1_update(&ctx, radv_immutable_samplers(set_layout, set_layout->binding + b),
				                  set_layout->binding[b].array_size * 4 * sizeof(uint32_t));
		}
		_mesa_sha1_update(&ctx, set_layout->binding,
				  sizeof(set_layout->binding[0]) * set_layout->binding_count);
	}

	layout->dynamic_offset_count = dynamic_offset_count;
	layout->push_constant_size = 0;
	for (unsigned i = 0; i < pCreateInfo->pushConstantRangeCount; ++i) {
		const VkPushConstantRange *range = pCreateInfo->pPushConstantRanges + i;
		layout->push_constant_size = MAX2(layout->push_constant_size,
						  range->offset + range->size);
	}

	layout->push_constant_size = align(layout->push_constant_size, 16);
	_mesa_sha1_update(&ctx, &layout->push_constant_size,
			  sizeof(layout->push_constant_size));
	_mesa_sha1_final(&ctx, layout->sha1);
	*pPipelineLayout = radv_pipeline_layout_to_handle(layout);

	return VK_SUCCESS;
}

void radv_DestroyPipelineLayout(
	VkDevice                                    _device,
	VkPipelineLayout                            _pipelineLayout,
	const VkAllocationCallbacks*                pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_pipeline_layout, pipeline_layout, _pipelineLayout);

	if (!pipeline_layout)
		return;
	vk_free2(&device->alloc, pAllocator, pipeline_layout);
}

#define EMPTY 1

static VkResult
radv_descriptor_set_create(struct radv_device *device,
			   struct radv_descriptor_pool *pool,
			   const struct radv_descriptor_set_layout *layout,
			   struct radv_descriptor_set **out_set)
{
	struct radv_descriptor_set *set;
	unsigned range_offset = sizeof(struct radv_descriptor_set) +
		sizeof(struct radeon_winsys_bo *) * layout->buffer_count;
	unsigned mem_size = range_offset +
		sizeof(struct radv_descriptor_range) * layout->dynamic_offset_count;

	if (pool->host_memory_base) {
		if (pool->host_memory_end - pool->host_memory_ptr < mem_size)
			return vk_error(VK_ERROR_OUT_OF_POOL_MEMORY_KHR);

		set = (struct radv_descriptor_set*)pool->host_memory_ptr;
		pool->host_memory_ptr += mem_size;
	} else {
		set = vk_alloc2(&device->alloc, NULL, mem_size, 8,
		                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

		if (!set)
			return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);
	}

	memset(set, 0, mem_size);

	if (layout->dynamic_offset_count) {
		set->dynamic_descriptors = (struct radv_descriptor_range*)((uint8_t*)set + range_offset);
	}

	set->layout = layout;
	if (layout->size) {
		uint32_t layout_size = align_u32(layout->size, 32);
		set->size = layout->size;

		/* try to allocate linearly first, so that we don't spend
		 * time looking for gaps if the app only allocates &
		 * resets via the pool. */
		if (pool->current_offset + layout_size <= pool->size) {
			set->bo = pool->bo;
			set->mapped_ptr = (uint32_t*)(pool->mapped_ptr + pool->current_offset);
			set->va = radv_buffer_get_va(set->bo) + pool->current_offset;
			pool->current_offset += layout_size;
			list_addtail(&set->vram_list, &pool->vram_list);
		} else if (!pool->host_memory_base) {
			uint64_t offset = 0;
			struct list_head *prev = &pool->vram_list;
			struct radv_descriptor_set *cur;

			assert(!pool->host_memory_base);
			LIST_FOR_EACH_ENTRY(cur, &pool->vram_list, vram_list) {
				uint64_t start = (uint8_t*)cur->mapped_ptr - pool->mapped_ptr;
				if (start - offset >= layout_size)
					break;

				offset = start + cur->size;
				prev = &cur->vram_list;
			}

			if (pool->size - offset < layout_size) {
				vk_free2(&device->alloc, NULL, set);
				return vk_error(VK_ERROR_OUT_OF_POOL_MEMORY_KHR);
			}
			set->bo = pool->bo;
			set->mapped_ptr = (uint32_t*)(pool->mapped_ptr + offset);
			set->va = radv_buffer_get_va(set->bo) + offset;
			list_add(&set->vram_list, prev);
		} else
			return vk_error(VK_ERROR_OUT_OF_POOL_MEMORY_KHR);
	}

	if (layout->has_immutable_samplers) {
		for (unsigned i = 0; i < layout->binding_count; ++i) {
			if (!layout->binding[i].immutable_samplers_offset ||
			layout->binding[i].immutable_samplers_equal)
				continue;

			unsigned offset = layout->binding[i].offset / 4;
			if (layout->binding[i].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				offset += 16;

			const uint32_t *samplers = (const uint32_t*)((const char*)layout + layout->binding[i].immutable_samplers_offset);
			for (unsigned j = 0; j < layout->binding[i].array_size; ++j) {
				memcpy(set->mapped_ptr + offset, samplers + 4 * j, 16);
				offset += layout->binding[i].size / 4;
			}

		}
	}
	*out_set = set;
	return VK_SUCCESS;
}

static void
radv_descriptor_set_destroy(struct radv_device *device,
			    struct radv_descriptor_pool *pool,
			    struct radv_descriptor_set *set,
			    bool free_bo)
{
	assert(!pool->host_memory_base);

	if (free_bo && set->size)
		list_del(&set->vram_list);
	vk_free2(&device->alloc, NULL, set);
}

VkResult radv_CreateDescriptorPool(
	VkDevice                                    _device,
	const VkDescriptorPoolCreateInfo*           pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkDescriptorPool*                           pDescriptorPool)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	struct radv_descriptor_pool *pool;
	int size = sizeof(struct radv_descriptor_pool);
	uint64_t bo_size = 0, bo_count = 0, range_count = 0;


	for (unsigned i = 0; i < pCreateInfo->poolSizeCount; ++i) {
		if (pCreateInfo->pPoolSizes[i].type != VK_DESCRIPTOR_TYPE_SAMPLER)
			bo_count += pCreateInfo->pPoolSizes[i].descriptorCount;

		switch(pCreateInfo->pPoolSizes[i].type) {
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			range_count += pCreateInfo->pPoolSizes[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			/* 32 as we may need to align for images */
			bo_size += 32 * pCreateInfo->pPoolSizes[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			bo_size += 64 * pCreateInfo->pPoolSizes[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			bo_size += 96 * pCreateInfo->pPoolSizes[i].descriptorCount;
			break;
		default:
			unreachable("unknown descriptor type\n");
			break;
		}
	}

	if (!(pCreateInfo->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)) {
		uint64_t host_size = pCreateInfo->maxSets * sizeof(struct radv_descriptor_set);
		host_size += sizeof(struct radeon_winsys_bo*) * bo_count;
		host_size += sizeof(struct radv_descriptor_range) * range_count;
		size += host_size;
	}

	pool = vk_alloc2(&device->alloc, pAllocator, size, 8,
	                 VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (!pool)
		return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

	memset(pool, 0, sizeof(*pool));

	if (!(pCreateInfo->flags & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)) {
		pool->host_memory_base = (uint8_t*)pool + sizeof(struct radv_descriptor_pool);
		pool->host_memory_ptr = pool->host_memory_base;
		pool->host_memory_end = (uint8_t*)pool + size;
	}

	if (bo_size) {
		pool->bo = device->ws->buffer_create(device->ws, bo_size,
							32, RADEON_DOMAIN_VRAM, 0);
		pool->mapped_ptr = (uint8_t*)device->ws->buffer_map(pool->bo);
	}
	pool->size = bo_size;

	list_inithead(&pool->vram_list);
	*pDescriptorPool = radv_descriptor_pool_to_handle(pool);
	return VK_SUCCESS;
}

void radv_DestroyDescriptorPool(
	VkDevice                                    _device,
	VkDescriptorPool                            _pool,
	const VkAllocationCallbacks*                pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_pool, pool, _pool);

	if (!pool)
		return;

	if (!pool->host_memory_base) {
		list_for_each_entry_safe(struct radv_descriptor_set, set,
		                         &pool->vram_list, vram_list) {
			radv_descriptor_set_destroy(device, pool, set, false);
		}
	}

	if (pool->bo)
		device->ws->buffer_destroy(pool->bo);
	vk_free2(&device->alloc, pAllocator, pool);
}

VkResult radv_ResetDescriptorPool(
	VkDevice                                    _device,
	VkDescriptorPool                            descriptorPool,
	VkDescriptorPoolResetFlags                  flags)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_pool, pool, descriptorPool);

	if (!pool->host_memory_base) {
		list_for_each_entry_safe(struct radv_descriptor_set, set,
		                         &pool->vram_list, vram_list) {
			radv_descriptor_set_destroy(device, pool, set, false);
		}
	}

	list_inithead(&pool->vram_list);

	pool->current_offset = 0;
	pool->host_memory_ptr = pool->host_memory_base;

	return VK_SUCCESS;
}

VkResult radv_AllocateDescriptorSets(
	VkDevice                                    _device,
	const VkDescriptorSetAllocateInfo*          pAllocateInfo,
	VkDescriptorSet*                            pDescriptorSets)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_pool, pool, pAllocateInfo->descriptorPool);

	VkResult result = VK_SUCCESS;
	uint32_t i;
	struct radv_descriptor_set *set;

	/* allocate a set of buffers for each shader to contain descriptors */
	for (i = 0; i < pAllocateInfo->descriptorSetCount; i++) {
		RADV_FROM_HANDLE(radv_descriptor_set_layout, layout,
				 pAllocateInfo->pSetLayouts[i]);

		assert(!(layout->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));

		result = radv_descriptor_set_create(device, pool, layout, &set);
		if (result != VK_SUCCESS)
			break;

		pDescriptorSets[i] = radv_descriptor_set_to_handle(set);
	}

	if (result != VK_SUCCESS)
		radv_FreeDescriptorSets(_device, pAllocateInfo->descriptorPool,
					i, pDescriptorSets);
	return result;
}

VkResult radv_FreeDescriptorSets(
	VkDevice                                    _device,
	VkDescriptorPool                            descriptorPool,
	uint32_t                                    count,
	const VkDescriptorSet*                      pDescriptorSets)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_pool, pool, descriptorPool);

	for (uint32_t i = 0; i < count; i++) {
		RADV_FROM_HANDLE(radv_descriptor_set, set, pDescriptorSets[i]);

		if (set && !pool->host_memory_base)
			radv_descriptor_set_destroy(device, pool, set, true);
	}
	return VK_SUCCESS;
}

static void write_texel_buffer_descriptor(struct radv_device *device,
					  struct radv_cmd_buffer *cmd_buffer,
					  unsigned *dst,
					  struct radeon_winsys_bo **buffer_list,
					  const VkBufferView _buffer_view)
{
	RADV_FROM_HANDLE(radv_buffer_view, buffer_view, _buffer_view);

	memcpy(dst, buffer_view->state, 4 * 4);

	if (cmd_buffer)
		device->ws->cs_add_buffer(cmd_buffer->cs, buffer_view->bo, 7);
	else
		*buffer_list = buffer_view->bo;
}

static void write_buffer_descriptor(struct radv_device *device,
                                    struct radv_cmd_buffer *cmd_buffer,
                                    unsigned *dst,
                                    struct radeon_winsys_bo **buffer_list,
                                    const VkDescriptorBufferInfo *buffer_info)
{
	RADV_FROM_HANDLE(radv_buffer, buffer, buffer_info->buffer);
	uint64_t va = radv_buffer_get_va(buffer->bo);
	uint32_t range = buffer_info->range;

	if (buffer_info->range == VK_WHOLE_SIZE)
		range = buffer->size - buffer_info->offset;

	va += buffer_info->offset + buffer->offset;
	dst[0] = va;
	dst[1] = S_008F04_BASE_ADDRESS_HI(va >> 32);
	dst[2] = range;
	dst[3] = S_008F0C_DST_SEL_X(V_008F0C_SQ_SEL_X) |
		S_008F0C_DST_SEL_Y(V_008F0C_SQ_SEL_Y) |
		S_008F0C_DST_SEL_Z(V_008F0C_SQ_SEL_Z) |
		S_008F0C_DST_SEL_W(V_008F0C_SQ_SEL_W) |
		S_008F0C_NUM_FORMAT(V_008F0C_BUF_NUM_FORMAT_FLOAT) |
		S_008F0C_DATA_FORMAT(V_008F0C_BUF_DATA_FORMAT_32);

	if (cmd_buffer)
		device->ws->cs_add_buffer(cmd_buffer->cs, buffer->bo, 7);
	else
		*buffer_list = buffer->bo;
}

static void write_dynamic_buffer_descriptor(struct radv_device *device,
                                            struct radv_descriptor_range *range,
                                            struct radeon_winsys_bo **buffer_list,
                                            const VkDescriptorBufferInfo *buffer_info)
{
	RADV_FROM_HANDLE(radv_buffer, buffer, buffer_info->buffer);
	uint64_t va = radv_buffer_get_va(buffer->bo);
	unsigned size = buffer_info->range;

	if (buffer_info->range == VK_WHOLE_SIZE)
		size = buffer->size - buffer_info->offset;

	va += buffer_info->offset + buffer->offset;
	range->va = va;
	range->size = size;

	*buffer_list = buffer->bo;
}

static void
write_image_descriptor(struct radv_device *device,
		       struct radv_cmd_buffer *cmd_buffer,
		       unsigned *dst,
		       struct radeon_winsys_bo **buffer_list,
		       VkDescriptorType descriptor_type,
		       const VkDescriptorImageInfo *image_info)
{
	RADV_FROM_HANDLE(radv_image_view, iview, image_info->imageView);

	if (descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
		memcpy(dst, iview->storage_descriptor, 8 * 4);
		memcpy(dst + 8, iview->storage_fmask_descriptor, 8 * 4);
	} else {
		memcpy(dst, iview->descriptor, 8 * 4);
		memcpy(dst + 8, iview->fmask_descriptor, 8 * 4);
	}

	if (cmd_buffer)
		device->ws->cs_add_buffer(cmd_buffer->cs, iview->bo, 7);
	else
		*buffer_list = iview->bo;
}

static void
write_combined_image_sampler_descriptor(struct radv_device *device,
					struct radv_cmd_buffer *cmd_buffer,
					unsigned *dst,
					struct radeon_winsys_bo **buffer_list,
					VkDescriptorType descriptor_type,
					const VkDescriptorImageInfo *image_info,
					bool has_sampler)
{
	RADV_FROM_HANDLE(radv_sampler, sampler, image_info->sampler);

	write_image_descriptor(device, cmd_buffer, dst, buffer_list, descriptor_type, image_info);
	/* copy over sampler state */
	if (has_sampler)
		memcpy(dst + 16, sampler->state, 16);
}

static void
write_sampler_descriptor(struct radv_device *device,
					unsigned *dst,
					const VkDescriptorImageInfo *image_info)
{
	RADV_FROM_HANDLE(radv_sampler, sampler, image_info->sampler);

	memcpy(dst, sampler->state, 16);
}

void radv_update_descriptor_sets(
	struct radv_device*                         device,
	struct radv_cmd_buffer*                     cmd_buffer,
	VkDescriptorSet                             dstSetOverride,
	uint32_t                                    descriptorWriteCount,
	const VkWriteDescriptorSet*                 pDescriptorWrites,
	uint32_t                                    descriptorCopyCount,
	const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	uint32_t i, j;
	for (i = 0; i < descriptorWriteCount; i++) {
		const VkWriteDescriptorSet *writeset = &pDescriptorWrites[i];
		RADV_FROM_HANDLE(radv_descriptor_set, set,
		                 dstSetOverride ? dstSetOverride : writeset->dstSet);
		const struct radv_descriptor_set_binding_layout *binding_layout =
			set->layout->binding + writeset->dstBinding;
		uint32_t *ptr = set->mapped_ptr;
		struct radeon_winsys_bo **buffer_list =  set->descriptors;
		/* Immutable samplers are not copied into push descriptors when they are
		 * allocated, so if we are writing push descriptors we have to copy the
		 * immutable samplers into them now.
		 */
		const bool copy_immutable_samplers = cmd_buffer &&
			binding_layout->immutable_samplers_offset && !binding_layout->immutable_samplers_equal;
		const uint32_t *samplers = radv_immutable_samplers(set->layout, binding_layout);

		ptr += binding_layout->offset / 4;
		ptr += binding_layout->size * writeset->dstArrayElement / 4;
		buffer_list += binding_layout->buffer_offset;
		buffer_list += writeset->dstArrayElement;
		for (j = 0; j < writeset->descriptorCount; ++j) {
			switch(writeset->descriptorType) {
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
				unsigned idx = writeset->dstArrayElement + j;
				idx += binding_layout->dynamic_offset_offset;
				assert(!(set->layout->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
				write_dynamic_buffer_descriptor(device, set->dynamic_descriptors + idx,
								buffer_list, writeset->pBufferInfo + j);
				break;
			}
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				write_buffer_descriptor(device, cmd_buffer, ptr, buffer_list,
							writeset->pBufferInfo + j);
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				write_texel_buffer_descriptor(device, cmd_buffer, ptr, buffer_list,
							      writeset->pTexelBufferView[j]);
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				write_image_descriptor(device, cmd_buffer, ptr, buffer_list,
						       writeset->descriptorType,
						       writeset->pImageInfo + j);
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				write_combined_image_sampler_descriptor(device, cmd_buffer, ptr, buffer_list,
									writeset->descriptorType,
									writeset->pImageInfo + j,
									!binding_layout->immutable_samplers_offset);
				if (copy_immutable_samplers) {
					const unsigned idx = writeset->dstArrayElement + j;
					memcpy(ptr + 16, samplers + 4 * idx, 16);
				}
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				if (!binding_layout->immutable_samplers_offset) {
					write_sampler_descriptor(device, ptr,
					                         writeset->pImageInfo + j);
				} else if (copy_immutable_samplers) {
					unsigned idx = writeset->dstArrayElement + j;
					memcpy(ptr, samplers + 4 * idx, 16);
				}
				break;
			default:
				unreachable("unimplemented descriptor type");
				break;
			}
			ptr += binding_layout->size / 4;
			++buffer_list;
		}

	}

	for (i = 0; i < descriptorCopyCount; i++) {
		const VkCopyDescriptorSet *copyset = &pDescriptorCopies[i];
		RADV_FROM_HANDLE(radv_descriptor_set, src_set,
		                 copyset->srcSet);
		RADV_FROM_HANDLE(radv_descriptor_set, dst_set,
		                 copyset->dstSet);
		const struct radv_descriptor_set_binding_layout *src_binding_layout =
			src_set->layout->binding + copyset->srcBinding;
		const struct radv_descriptor_set_binding_layout *dst_binding_layout =
			dst_set->layout->binding + copyset->dstBinding;
		uint32_t *src_ptr = src_set->mapped_ptr;
		uint32_t *dst_ptr = dst_set->mapped_ptr;
		struct radeon_winsys_bo **src_buffer_list = src_set->descriptors;
		struct radeon_winsys_bo **dst_buffer_list = dst_set->descriptors;

		src_ptr += src_binding_layout->offset / 4;
		dst_ptr += dst_binding_layout->offset / 4;

		src_ptr += src_binding_layout->size * copyset->srcArrayElement / 4;
		dst_ptr += dst_binding_layout->size * copyset->dstArrayElement / 4;

		src_buffer_list += src_binding_layout->buffer_offset;
		src_buffer_list += copyset->srcArrayElement;

		dst_buffer_list += dst_binding_layout->buffer_offset;
		dst_buffer_list += copyset->dstArrayElement;

		for (j = 0; j < copyset->descriptorCount; ++j) {
			switch (src_binding_layout->type) {
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
				unsigned src_idx = copyset->srcArrayElement + j;
				unsigned dst_idx = copyset->dstArrayElement + j;
				struct radv_descriptor_range *src_range, *dst_range;
				src_idx += src_binding_layout->dynamic_offset_offset;
				dst_idx += dst_binding_layout->dynamic_offset_offset;

				src_range = src_set->dynamic_descriptors + src_idx;
				dst_range = dst_set->dynamic_descriptors + dst_idx;
				*dst_range = *src_range;
				break;
			}
			default:
				memcpy(dst_ptr, src_ptr, src_binding_layout->size);
			}
			src_ptr += src_binding_layout->size / 4;
			dst_ptr += dst_binding_layout->size / 4;
			dst_buffer_list[j] = src_buffer_list[j];
			++src_buffer_list;
			++dst_buffer_list;
		}
	}
}

void radv_UpdateDescriptorSets(
	VkDevice                                    _device,
	uint32_t                                    descriptorWriteCount,
	const VkWriteDescriptorSet*                 pDescriptorWrites,
	uint32_t                                    descriptorCopyCount,
	const VkCopyDescriptorSet*                  pDescriptorCopies)
{
	RADV_FROM_HANDLE(radv_device, device, _device);

	radv_update_descriptor_sets(device, NULL, VK_NULL_HANDLE, descriptorWriteCount, pDescriptorWrites,
			            descriptorCopyCount, pDescriptorCopies);
}

VkResult radv_CreateDescriptorUpdateTemplateKHR(VkDevice _device,
                                                const VkDescriptorUpdateTemplateCreateInfoKHR *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator,
                                                VkDescriptorUpdateTemplateKHR *pDescriptorUpdateTemplate)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_set_layout, set_layout, pCreateInfo->descriptorSetLayout);
	const uint32_t entry_count = pCreateInfo->descriptorUpdateEntryCount;
	const size_t size = sizeof(struct radv_descriptor_update_template) +
		sizeof(struct radv_descriptor_update_template_entry) * entry_count;
	struct radv_descriptor_update_template *templ;
	uint32_t i;

	templ = vk_alloc2(&device->alloc, pAllocator, size, 8, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (!templ)
		return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

	templ->entry_count = entry_count;

	for (i = 0; i < entry_count; i++) {
		const VkDescriptorUpdateTemplateEntryKHR *entry = &pCreateInfo->pDescriptorUpdateEntries[i];
		const struct radv_descriptor_set_binding_layout *binding_layout =
			set_layout->binding + entry->dstBinding;
		const uint32_t buffer_offset = binding_layout->buffer_offset + entry->dstArrayElement;
		const uint32_t *immutable_samplers = NULL;
		uint32_t dst_offset;
		uint32_t dst_stride;

		/* dst_offset is an offset into dynamic_descriptors when the descriptor
		   is dynamic, and an offset into mapped_ptr otherwise */
		switch (entry->descriptorType) {
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			assert(pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR);
			dst_offset = binding_layout->dynamic_offset_offset + entry->dstArrayElement;
			dst_stride = 0; /* Not used */
			break;
		default:
			switch (entry->descriptorType) {
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				/* Immutable samplers are copied into push descriptors when they are pushed */
				if (pCreateInfo->templateType == VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR &&
				    binding_layout->immutable_samplers_offset && !binding_layout->immutable_samplers_equal) {
					immutable_samplers = radv_immutable_samplers(set_layout, binding_layout) + entry->dstArrayElement * 4;
				}
				break;
			default:
				break;
			}
			dst_offset = binding_layout->offset / 4 + binding_layout->size * entry->dstArrayElement / 4;
			dst_stride = binding_layout->size / 4;
			break;
		}

		templ->entry[i] = (struct radv_descriptor_update_template_entry) {
			.descriptor_type = entry->descriptorType,
			.descriptor_count = entry->descriptorCount,
			.src_offset = entry->offset,
			.src_stride = entry->stride,
			.dst_offset = dst_offset,
			.dst_stride = dst_stride,
			.buffer_offset = buffer_offset,
			.has_sampler = !binding_layout->immutable_samplers_offset,
			.immutable_samplers = immutable_samplers
		};
	}

	*pDescriptorUpdateTemplate = radv_descriptor_update_template_to_handle(templ);
	return VK_SUCCESS;
}

void radv_DestroyDescriptorUpdateTemplateKHR(VkDevice _device,
                                             VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate,
                                             const VkAllocationCallbacks *pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_update_template, templ, descriptorUpdateTemplate);

	if (!templ)
		return;

	vk_free2(&device->alloc, pAllocator, templ);
}

void radv_update_descriptor_set_with_template(struct radv_device *device,
                                              struct radv_cmd_buffer *cmd_buffer,
                                              struct radv_descriptor_set *set,
                                              VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate,
                                              const void *pData)
{
	RADV_FROM_HANDLE(radv_descriptor_update_template, templ, descriptorUpdateTemplate);
	uint32_t i;

	for (i = 0; i < templ->entry_count; ++i) {
		struct radeon_winsys_bo **buffer_list = set->descriptors + templ->entry[i].buffer_offset;
		uint32_t *pDst = set->mapped_ptr + templ->entry[i].dst_offset;
		const uint8_t *pSrc = ((const uint8_t *) pData) + templ->entry[i].src_offset;
		uint32_t j;

		for (j = 0; j < templ->entry[i].descriptor_count; ++j) {
			switch (templ->entry[i].descriptor_type) {
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
				const unsigned idx = templ->entry[i].dst_offset + j;
				assert(!(set->layout->flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR));
				write_dynamic_buffer_descriptor(device, set->dynamic_descriptors + idx,
								buffer_list, (struct VkDescriptorBufferInfo *) pSrc);
				break;
			}
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				write_buffer_descriptor(device, cmd_buffer, pDst, buffer_list,
				                        (struct VkDescriptorBufferInfo *) pSrc);
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
				write_texel_buffer_descriptor(device, cmd_buffer, pDst, buffer_list,
						              *(VkBufferView *) pSrc);
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				write_image_descriptor(device, cmd_buffer, pDst, buffer_list,
						       templ->entry[i].descriptor_type,
					               (struct VkDescriptorImageInfo *) pSrc);
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				write_combined_image_sampler_descriptor(device, cmd_buffer, pDst, buffer_list,
									templ->entry[i].descriptor_type,
									(struct VkDescriptorImageInfo *) pSrc,
									templ->entry[i].has_sampler);
				if (templ->entry[i].immutable_samplers)
					memcpy(pDst + 16, templ->entry[i].immutable_samplers + 4 * j, 16);
				break;
			case VK_DESCRIPTOR_TYPE_SAMPLER:
				if (templ->entry[i].has_sampler)
					write_sampler_descriptor(device, pDst,
					                         (struct VkDescriptorImageInfo *) pSrc);
				else if (templ->entry[i].immutable_samplers)
					memcpy(pDst, templ->entry[i].immutable_samplers + 4 * j, 16);
				break;
			default:
				unreachable("unimplemented descriptor type");
				break;
			}
		        pSrc += templ->entry[i].src_stride;
			pDst += templ->entry[i].dst_stride;
			++buffer_list;
		}
	}
}

void radv_UpdateDescriptorSetWithTemplateKHR(VkDevice _device,
                                             VkDescriptorSet descriptorSet,
                                             VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate,
                                             const void *pData)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_descriptor_set, set, descriptorSet);

	radv_update_descriptor_set_with_template(device, NULL, set, descriptorUpdateTemplate, pData);
}
