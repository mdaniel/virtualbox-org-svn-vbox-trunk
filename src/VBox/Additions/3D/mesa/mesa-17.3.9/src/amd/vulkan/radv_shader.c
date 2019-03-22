/*
 * Copyright © 2016 Red Hat.
 * Copyright © 2016 Bas Nieuwenhuizen
 *
 * based in part on anv driver which is:
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

#include "util/mesa-sha1.h"
#include "util/u_atomic.h"
#include "radv_debug.h"
#include "radv_private.h"
#include "radv_shader.h"
#include "nir/nir.h"
#include "nir/nir_builder.h"
#include "spirv/nir_spirv.h"

#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>

#include "sid.h"
#include "gfx9d.h"
#include "ac_binary.h"
#include "ac_llvm_util.h"
#include "ac_nir_to_llvm.h"
#include "vk_format.h"
#include "util/debug.h"
#include "ac_exp_param.h"

static const struct nir_shader_compiler_options nir_options = {
	.vertex_id_zero_based = true,
	.lower_scmp = true,
	.lower_flrp32 = true,
	.lower_fsat = true,
	.lower_fdiv = true,
	.lower_sub = true,
	.lower_pack_snorm_2x16 = true,
	.lower_pack_snorm_4x8 = true,
	.lower_pack_unorm_2x16 = true,
	.lower_pack_unorm_4x8 = true,
	.lower_unpack_snorm_2x16 = true,
	.lower_unpack_snorm_4x8 = true,
	.lower_unpack_unorm_2x16 = true,
	.lower_unpack_unorm_4x8 = true,
	.lower_extract_byte = true,
	.lower_extract_word = true,
	.lower_ffma = true,
	.max_unroll_iterations = 32
};

VkResult radv_CreateShaderModule(
	VkDevice                                    _device,
	const VkShaderModuleCreateInfo*             pCreateInfo,
	const VkAllocationCallbacks*                pAllocator,
	VkShaderModule*                             pShaderModule)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	struct radv_shader_module *module;

	assert(pCreateInfo->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
	assert(pCreateInfo->flags == 0);

	module = vk_alloc2(&device->alloc, pAllocator,
			     sizeof(*module) + pCreateInfo->codeSize, 8,
			     VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
	if (module == NULL)
		return vk_error(VK_ERROR_OUT_OF_HOST_MEMORY);

	module->nir = NULL;
	module->size = pCreateInfo->codeSize;
	memcpy(module->data, pCreateInfo->pCode, module->size);

	_mesa_sha1_compute(module->data, module->size, module->sha1);

	*pShaderModule = radv_shader_module_to_handle(module);

	return VK_SUCCESS;
}

void radv_DestroyShaderModule(
	VkDevice                                    _device,
	VkShaderModule                              _module,
	const VkAllocationCallbacks*                pAllocator)
{
	RADV_FROM_HANDLE(radv_device, device, _device);
	RADV_FROM_HANDLE(radv_shader_module, module, _module);

	if (!module)
		return;

	vk_free2(&device->alloc, pAllocator, module);
}

bool
radv_lower_indirect_derefs(struct nir_shader *nir,
                           struct radv_physical_device *device)
{
	/* While it would be nice not to have this flag, we are constrained
	 * by the reality that LLVM 5.0 doesn't have working VGPR indexing
	 * on GFX9.
	 */
	bool llvm_has_working_vgpr_indexing =
		device->rad_info.chip_class <= VI;

	/* TODO: Indirect indexing of GS inputs is unimplemented.
	 *
	 * TCS and TES load inputs directly from LDS or offchip memory, so
	 * indirect indexing is trivial.
	 */
	nir_variable_mode indirect_mask = 0;
	if (nir->info.stage == MESA_SHADER_GEOMETRY ||
	    (nir->info.stage != MESA_SHADER_TESS_CTRL &&
	     nir->info.stage != MESA_SHADER_TESS_EVAL &&
	     !llvm_has_working_vgpr_indexing)) {
		indirect_mask |= nir_var_shader_in;
	}
	if (!llvm_has_working_vgpr_indexing &&
	    nir->info.stage != MESA_SHADER_TESS_CTRL)
		indirect_mask |= nir_var_shader_out;

	/* TODO: We shouldn't need to do this, however LLVM isn't currently
	 * smart enough to handle indirects without causing excess spilling
	 * causing the gpu to hang.
	 *
	 * See the following thread for more details of the problem:
	 * https://lists.freedesktop.org/archives/mesa-dev/2017-July/162106.html
	 */
	indirect_mask |= nir_var_local;

	return nir_lower_indirect_derefs(nir, indirect_mask);
}

void
radv_optimize_nir(struct nir_shader *shader)
{
        bool progress;

        do {
                progress = false;

                NIR_PASS_V(shader, nir_lower_vars_to_ssa);
		NIR_PASS_V(shader, nir_lower_64bit_pack);
                NIR_PASS_V(shader, nir_lower_alu_to_scalar);
                NIR_PASS_V(shader, nir_lower_phis_to_scalar);

                NIR_PASS(progress, shader, nir_copy_prop);
                NIR_PASS(progress, shader, nir_opt_remove_phis);
                NIR_PASS(progress, shader, nir_opt_dce);
                if (nir_opt_trivial_continues(shader)) {
                        progress = true;
                        NIR_PASS(progress, shader, nir_copy_prop);
			NIR_PASS(progress, shader, nir_opt_remove_phis);
                        NIR_PASS(progress, shader, nir_opt_dce);
                }
                NIR_PASS(progress, shader, nir_opt_if);
                NIR_PASS(progress, shader, nir_opt_dead_cf);
                NIR_PASS(progress, shader, nir_opt_cse);
                NIR_PASS(progress, shader, nir_opt_peephole_select, 8);
                NIR_PASS(progress, shader, nir_opt_algebraic);
                NIR_PASS(progress, shader, nir_opt_constant_folding);
                NIR_PASS(progress, shader, nir_opt_undef);
                NIR_PASS(progress, shader, nir_opt_conditional_discard);
                if (shader->options->max_unroll_iterations) {
                        NIR_PASS(progress, shader, nir_opt_loop_unroll, 0);
                }
        } while (progress);
}

nir_shader *
radv_shader_compile_to_nir(struct radv_device *device,
			   struct radv_shader_module *module,
			   const char *entrypoint_name,
			   gl_shader_stage stage,
			   const VkSpecializationInfo *spec_info)
{
	if (strcmp(entrypoint_name, "main") != 0) {
		radv_finishme("Multiple shaders per module not really supported");
	}

	nir_shader *nir;
	nir_function *entry_point;
	if (module->nir) {
		/* Some things such as our meta clear/blit code will give us a NIR
		 * shader directly.  In that case, we just ignore the SPIR-V entirely
		 * and just use the NIR shader */
		nir = module->nir;
		nir->options = &nir_options;
		nir_validate_shader(nir);

		assert(exec_list_length(&nir->functions) == 1);
		struct exec_node *node = exec_list_get_head(&nir->functions);
		entry_point = exec_node_data(nir_function, node, node);
	} else {
		uint32_t *spirv = (uint32_t *) module->data;
		assert(module->size % 4 == 0);

		if (device->instance->debug_flags & RADV_DEBUG_DUMP_SPIRV)
			radv_print_spirv(spirv, module->size, stderr);

		uint32_t num_spec_entries = 0;
		struct nir_spirv_specialization *spec_entries = NULL;
		if (spec_info && spec_info->mapEntryCount > 0) {
			num_spec_entries = spec_info->mapEntryCount;
			spec_entries = malloc(num_spec_entries * sizeof(*spec_entries));
			for (uint32_t i = 0; i < num_spec_entries; i++) {
				VkSpecializationMapEntry entry = spec_info->pMapEntries[i];
				const void *data = spec_info->pData + entry.offset;
				assert(data + entry.size <= spec_info->pData + spec_info->dataSize);

				spec_entries[i].id = spec_info->pMapEntries[i].constantID;
				if (spec_info->dataSize == 8)
					spec_entries[i].data64 = *(const uint64_t *)data;
				else
					spec_entries[i].data32 = *(const uint32_t *)data;
			}
		}
		const struct nir_spirv_supported_extensions supported_ext = {
			.draw_parameters = true,
			.float64 = true,
			.image_read_without_format = true,
			.image_write_without_format = true,
			.tessellation = true,
			.int64 = true,
			.multiview = true,
			.variable_pointers = true,
		};
		entry_point = spirv_to_nir(spirv, module->size / 4,
					   spec_entries, num_spec_entries,
					   stage, entrypoint_name, &supported_ext, &nir_options);
		nir = entry_point->shader;
		assert(nir->info.stage == stage);
		nir_validate_shader(nir);

		free(spec_entries);

		/* We have to lower away local constant initializers right before we
		 * inline functions.  That way they get properly initialized at the top
		 * of the function and not at the top of its caller.
		 */
		NIR_PASS_V(nir, nir_lower_constant_initializers, nir_var_local);
		NIR_PASS_V(nir, nir_lower_returns);
		NIR_PASS_V(nir, nir_inline_functions);

		/* Pick off the single entrypoint that we want */
		foreach_list_typed_safe(nir_function, func, node, &nir->functions) {
			if (func != entry_point)
				exec_node_remove(&func->node);
		}
		assert(exec_list_length(&nir->functions) == 1);
		entry_point->name = ralloc_strdup(entry_point, "main");

		NIR_PASS_V(nir, nir_remove_dead_variables,
		           nir_var_shader_in | nir_var_shader_out | nir_var_system_value);

		/* Now that we've deleted all but the main function, we can go ahead and
		 * lower the rest of the constant initializers.
		 */
		NIR_PASS_V(nir, nir_lower_constant_initializers, ~0);
		NIR_PASS_V(nir, nir_lower_system_values);
		NIR_PASS_V(nir, nir_lower_clip_cull_distance_arrays);
	}

	/* Vulkan uses the separate-shader linking model */
	nir->info.separate_shader = true;

	nir_shader_gather_info(nir, entry_point->impl);

	static const nir_lower_tex_options tex_options = {
	  .lower_txp = ~0,
	};

	nir_lower_tex(nir, &tex_options);

	nir_lower_vars_to_ssa(nir);
	nir_lower_var_copies(nir);
	nir_lower_global_vars_to_local(nir);
	nir_remove_dead_variables(nir, nir_var_local);
	radv_lower_indirect_derefs(nir, device->physical_device);
	radv_optimize_nir(nir);

	return nir;
}

void *
radv_alloc_shader_memory(struct radv_device *device,
			 struct radv_shader_variant *shader)
{
	mtx_lock(&device->shader_slab_mutex);
	list_for_each_entry(struct radv_shader_slab, slab, &device->shader_slabs, slabs) {
		uint64_t offset = 0;
		list_for_each_entry(struct radv_shader_variant, s, &slab->shaders, slab_list) {
			if (s->bo_offset - offset >= shader->code_size) {
				shader->bo = slab->bo;
				shader->bo_offset = offset;
				list_addtail(&shader->slab_list, &s->slab_list);
				mtx_unlock(&device->shader_slab_mutex);
				return slab->ptr + offset;
			}
			offset = align_u64(s->bo_offset + s->code_size, 256);
		}
		if (slab->size - offset >= shader->code_size) {
			shader->bo = slab->bo;
			shader->bo_offset = offset;
			list_addtail(&shader->slab_list, &slab->shaders);
			mtx_unlock(&device->shader_slab_mutex);
			return slab->ptr + offset;
		}
	}

	mtx_unlock(&device->shader_slab_mutex);
	struct radv_shader_slab *slab = calloc(1, sizeof(struct radv_shader_slab));

	slab->size = 256 * 1024;
	slab->bo = device->ws->buffer_create(device->ws, slab->size, 256,
	                                     RADEON_DOMAIN_VRAM, 0);
	slab->ptr = (char*)device->ws->buffer_map(slab->bo);
	list_inithead(&slab->shaders);

	mtx_lock(&device->shader_slab_mutex);
	list_add(&slab->slabs, &device->shader_slabs);

	shader->bo = slab->bo;
	shader->bo_offset = 0;
	list_add(&shader->slab_list, &slab->shaders);
	mtx_unlock(&device->shader_slab_mutex);
	return slab->ptr;
}

void
radv_destroy_shader_slabs(struct radv_device *device)
{
	list_for_each_entry_safe(struct radv_shader_slab, slab, &device->shader_slabs, slabs) {
		device->ws->buffer_destroy(slab->bo);
		free(slab);
	}
	mtx_destroy(&device->shader_slab_mutex);
}

static void
radv_fill_shader_variant(struct radv_device *device,
			 struct radv_shader_variant *variant,
			 struct ac_shader_binary *binary,
			 gl_shader_stage stage)
{
	bool scratch_enabled = variant->config.scratch_bytes_per_wave > 0;
	unsigned vgpr_comp_cnt = 0;

	if (scratch_enabled && !device->llvm_supports_spill)
		radv_finishme("shader scratch support only available with LLVM 4.0");

	variant->code_size = binary->code_size;
	variant->rsrc2 = S_00B12C_USER_SGPR(variant->info.num_user_sgprs) |
			S_00B12C_SCRATCH_EN(scratch_enabled);

	variant->rsrc1 =  S_00B848_VGPRS((variant->config.num_vgprs - 1) / 4) |
		S_00B848_SGPRS((variant->config.num_sgprs - 1) / 8) |
		S_00B848_DX10_CLAMP(1) |
		S_00B848_FLOAT_MODE(variant->config.float_mode);

	switch (stage) {
	case MESA_SHADER_TESS_EVAL:
		vgpr_comp_cnt = 3;
		variant->rsrc2 |= S_00B12C_OC_LDS_EN(1);
		break;
	case MESA_SHADER_TESS_CTRL:
		if (device->physical_device->rad_info.chip_class >= GFX9)
			vgpr_comp_cnt = variant->info.vs.vgpr_comp_cnt;
		else
			variant->rsrc2 |= S_00B12C_OC_LDS_EN(1);
		break;
	case MESA_SHADER_VERTEX:
	case MESA_SHADER_GEOMETRY:
		vgpr_comp_cnt = variant->info.vs.vgpr_comp_cnt;
		break;
	case MESA_SHADER_FRAGMENT:
		break;
	case MESA_SHADER_COMPUTE:
		variant->rsrc2 |=
			S_00B84C_TGID_X_EN(1) | S_00B84C_TGID_Y_EN(1) |
			S_00B84C_TGID_Z_EN(1) | S_00B84C_TIDIG_COMP_CNT(2) |
			S_00B84C_TG_SIZE_EN(1) |
			S_00B84C_LDS_SIZE(variant->config.lds_size);
		break;
	default:
		unreachable("unsupported shader type");
		break;
	}

	if (device->physical_device->rad_info.chip_class >= GFX9 &&
	    stage == MESA_SHADER_GEOMETRY) {
		/* TODO: Figure out how many we actually need. */
		variant->rsrc1 |= S_00B228_GS_VGPR_COMP_CNT(3);
		variant->rsrc2 |= S_00B22C_ES_VGPR_COMP_CNT(3) |
		                  S_00B22C_OC_LDS_EN(1);
	} else if (device->physical_device->rad_info.chip_class >= GFX9 &&
	    stage == MESA_SHADER_TESS_CTRL)
		variant->rsrc1 |= S_00B428_LS_VGPR_COMP_CNT(vgpr_comp_cnt);
	else
		variant->rsrc1 |= S_00B128_VGPR_COMP_CNT(vgpr_comp_cnt);

	void *ptr = radv_alloc_shader_memory(device, variant);
	memcpy(ptr, binary->code, binary->code_size);
}

static struct radv_shader_variant *
shader_variant_create(struct radv_device *device,
		      struct radv_shader_module *module,
		      struct nir_shader * const *shaders,
		      int shader_count,
		      gl_shader_stage stage,
		      struct ac_nir_compiler_options *options,
		      bool gs_copy_shader,
		      void **code_out,
		      unsigned *code_size_out)
{
	enum radeon_family chip_family = device->physical_device->rad_info.family;
	bool dump_shaders = device->instance->debug_flags & RADV_DEBUG_DUMP_SHADERS;
	enum ac_target_machine_options tm_options = 0;
	struct radv_shader_variant *variant;
	struct ac_shader_binary binary;
	LLVMTargetMachineRef tm;

	variant = calloc(1, sizeof(struct radv_shader_variant));
	if (!variant)
		return NULL;

	options->family = chip_family;
	options->chip_class = device->physical_device->rad_info.chip_class;

	if (options->supports_spill)
		tm_options |= AC_TM_SUPPORTS_SPILL;
	if (device->instance->perftest_flags & RADV_PERFTEST_SISCHED)
		tm_options |= AC_TM_SISCHED;
	tm = ac_create_target_machine(chip_family, tm_options);

	if (gs_copy_shader) {
		assert(shader_count == 1);
		ac_create_gs_copy_shader(tm, *shaders, &binary, &variant->config,
					 &variant->info, options, dump_shaders);
	} else {
		ac_compile_nir_shader(tm, &binary, &variant->config,
				      &variant->info, shaders, shader_count, options,
				      dump_shaders);
	}

	LLVMDisposeTargetMachine(tm);

	radv_fill_shader_variant(device, variant, &binary, stage);

	if (code_out) {
		*code_out = binary.code;
		*code_size_out = binary.code_size;
	} else
		free(binary.code);
	free(binary.config);
	free(binary.rodata);
	free(binary.global_symbol_offsets);
	free(binary.relocs);
	variant->ref_count = 1;

	if (device->trace_bo) {
		variant->disasm_string = binary.disasm_string;
		if (!gs_copy_shader && !module->nir) {
			variant->nir = *shaders;
			variant->spirv = (uint32_t *)module->data;
			variant->spirv_size = module->size;
		}
	} else {
		free(binary.disasm_string);
	}

	return variant;
}

struct radv_shader_variant *
radv_shader_variant_create(struct radv_device *device,
			   struct radv_shader_module *module,
			   struct nir_shader *const *shaders,
			   int shader_count,
			   struct radv_pipeline_layout *layout,
			   const struct ac_shader_variant_key *key,
			   void **code_out,
			   unsigned *code_size_out)
{
	struct ac_nir_compiler_options options = {0};

	options.layout = layout;
	if (key)
		options.key = *key;

	options.unsafe_math = !!(device->instance->debug_flags & RADV_DEBUG_UNSAFE_MATH);
	options.supports_spill = device->llvm_supports_spill;

	return shader_variant_create(device, module, shaders, shader_count, shaders[shader_count - 1]->info.stage,
				     &options, false, code_out, code_size_out);
}

struct radv_shader_variant *
radv_create_gs_copy_shader(struct radv_device *device,
			   struct nir_shader *shader,
			   void **code_out,
			   unsigned *code_size_out,
			   bool multiview)
{
	struct ac_nir_compiler_options options = {0};

	options.key.has_multiview_view_index = multiview;

	return shader_variant_create(device, NULL, &shader, 1, MESA_SHADER_VERTEX,
				     &options, true, code_out, code_size_out);
}

void
radv_shader_variant_destroy(struct radv_device *device,
			    struct radv_shader_variant *variant)
{
	if (!p_atomic_dec_zero(&variant->ref_count))
		return;

	mtx_lock(&device->shader_slab_mutex);
	list_del(&variant->slab_list);
	mtx_unlock(&device->shader_slab_mutex);

	ralloc_free(variant->nir);
	free(variant->disasm_string);
	free(variant);
}

uint32_t
radv_shader_stage_to_user_data_0(gl_shader_stage stage, enum chip_class chip_class,
				 bool has_gs, bool has_tess)
{
	switch (stage) {
	case MESA_SHADER_FRAGMENT:
		return R_00B030_SPI_SHADER_USER_DATA_PS_0;
	case MESA_SHADER_VERTEX:
		if (chip_class >= GFX9) {
			return has_tess ? R_00B430_SPI_SHADER_USER_DATA_LS_0 :
			       has_gs ? R_00B330_SPI_SHADER_USER_DATA_ES_0 :
			       R_00B130_SPI_SHADER_USER_DATA_VS_0;
		}
		if (has_tess)
			return R_00B530_SPI_SHADER_USER_DATA_LS_0;
		else
			return has_gs ? R_00B330_SPI_SHADER_USER_DATA_ES_0 : R_00B130_SPI_SHADER_USER_DATA_VS_0;
	case MESA_SHADER_GEOMETRY:
		return chip_class >= GFX9 ? R_00B330_SPI_SHADER_USER_DATA_ES_0 :
		                            R_00B230_SPI_SHADER_USER_DATA_GS_0;
	case MESA_SHADER_COMPUTE:
		return R_00B900_COMPUTE_USER_DATA_0;
	case MESA_SHADER_TESS_CTRL:
		return chip_class >= GFX9 ? R_00B430_SPI_SHADER_USER_DATA_LS_0 :
		                            R_00B430_SPI_SHADER_USER_DATA_HS_0;
	case MESA_SHADER_TESS_EVAL:
		if (chip_class >= GFX9) {
			return has_gs ? R_00B330_SPI_SHADER_USER_DATA_ES_0 :
			       R_00B130_SPI_SHADER_USER_DATA_VS_0;
		}
		if (has_gs)
			return R_00B330_SPI_SHADER_USER_DATA_ES_0;
		else
			return R_00B130_SPI_SHADER_USER_DATA_VS_0;
	default:
		unreachable("unknown shader");
	}
}

const char *
radv_get_shader_name(struct radv_shader_variant *var, gl_shader_stage stage)
{
	switch (stage) {
	case MESA_SHADER_VERTEX: return var->info.vs.as_ls ? "Vertex Shader as LS" : var->info.vs.as_es ? "Vertex Shader as ES" : "Vertex Shader as VS";
	case MESA_SHADER_GEOMETRY: return "Geometry Shader";
	case MESA_SHADER_FRAGMENT: return "Pixel Shader";
	case MESA_SHADER_COMPUTE: return "Compute Shader";
	case MESA_SHADER_TESS_CTRL: return "Tessellation Control Shader";
	case MESA_SHADER_TESS_EVAL: return var->info.tes.as_es ? "Tessellation Evaluation Shader as ES" : "Tessellation Evaluation Shader as VS";
	default:
		return "Unknown shader";
	};
}

void
radv_shader_dump_stats(struct radv_device *device,
		       struct radv_shader_variant *variant,
		       gl_shader_stage stage,
		       FILE *file)
{
	unsigned lds_increment = device->physical_device->rad_info.chip_class >= CIK ? 512 : 256;
	struct ac_shader_config *conf;
	unsigned max_simd_waves;
	unsigned lds_per_wave = 0;

	switch (device->physical_device->rad_info.family) {
	/* These always have 8 waves: */
	case CHIP_POLARIS10:
	case CHIP_POLARIS11:
	case CHIP_POLARIS12:
		max_simd_waves = 8;
		break;
	default:
		max_simd_waves = 10;
	}

	conf = &variant->config;

	if (stage == MESA_SHADER_FRAGMENT) {
		lds_per_wave = conf->lds_size * lds_increment +
			       align(variant->info.fs.num_interp * 48,
				     lds_increment);
	}

	if (conf->num_sgprs) {
		if (device->physical_device->rad_info.chip_class >= VI)
			max_simd_waves = MIN2(max_simd_waves, 800 / conf->num_sgprs);
		else
			max_simd_waves = MIN2(max_simd_waves, 512 / conf->num_sgprs);
	}

	if (conf->num_vgprs)
		max_simd_waves = MIN2(max_simd_waves, 256 / conf->num_vgprs);

	/* LDS is 64KB per CU (4 SIMDs), divided into 16KB blocks per SIMD
	 * that PS can use.
	 */
	if (lds_per_wave)
		max_simd_waves = MIN2(max_simd_waves, 16384 / lds_per_wave);

	fprintf(file, "\n%s:\n", radv_get_shader_name(variant, stage));

	if (stage == MESA_SHADER_FRAGMENT) {
		fprintf(file, "*** SHADER CONFIG ***\n"
			"SPI_PS_INPUT_ADDR = 0x%04x\n"
			"SPI_PS_INPUT_ENA  = 0x%04x\n",
			conf->spi_ps_input_addr, conf->spi_ps_input_ena);
	}

	fprintf(file, "*** SHADER STATS ***\n"
		"SGPRS: %d\n"
		"VGPRS: %d\n"
		"Spilled SGPRs: %d\n"
		"Spilled VGPRs: %d\n"
		"Code Size: %d bytes\n"
		"LDS: %d blocks\n"
		"Scratch: %d bytes per wave\n"
		"Max Waves: %d\n"
		"********************\n\n\n",
		conf->num_sgprs, conf->num_vgprs,
		conf->spilled_sgprs, conf->spilled_vgprs, variant->code_size,
		conf->lds_size, conf->scratch_bytes_per_wave,
		max_simd_waves);
}
