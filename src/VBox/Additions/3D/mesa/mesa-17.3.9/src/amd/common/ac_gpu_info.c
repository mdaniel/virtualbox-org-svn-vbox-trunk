/*
 * Copyright © 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

#include "ac_gpu_info.h"
#include "sid.h"
#include "gfx9d.h"

#include "util/u_math.h"

#include <stdio.h>

#include <xf86drm.h>
#include <amdgpu_drm.h>

#include <amdgpu.h>

#define CIK_TILE_MODE_COLOR_2D			14

#define CIK__GB_TILE_MODE__PIPE_CONFIG(x)        (((x) >> 6) & 0x1f)
#define     CIK__PIPE_CONFIG__ADDR_SURF_P2               0
#define     CIK__PIPE_CONFIG__ADDR_SURF_P4_8x16          4
#define     CIK__PIPE_CONFIG__ADDR_SURF_P4_16x16         5
#define     CIK__PIPE_CONFIG__ADDR_SURF_P4_16x32         6
#define     CIK__PIPE_CONFIG__ADDR_SURF_P4_32x32         7
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_16x16_8x16    8
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_16x32_8x16    9
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_8x16    10
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_16x32_16x16   11
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_16x16   12
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_16x32   13
#define     CIK__PIPE_CONFIG__ADDR_SURF_P8_32x64_32x32   14
#define     CIK__PIPE_CONFIG__ADDR_SURF_P16_32X32_8X16   16
#define     CIK__PIPE_CONFIG__ADDR_SURF_P16_32X32_16X16  17

static unsigned cik_get_num_tile_pipes(struct amdgpu_gpu_info *info)
{
   unsigned mode2d = info->gb_tile_mode[CIK_TILE_MODE_COLOR_2D];

   switch (CIK__GB_TILE_MODE__PIPE_CONFIG(mode2d)) {
   case CIK__PIPE_CONFIG__ADDR_SURF_P2:
       return 2;
   case CIK__PIPE_CONFIG__ADDR_SURF_P4_8x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P4_16x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P4_16x32:
   case CIK__PIPE_CONFIG__ADDR_SURF_P4_32x32:
       return 4;
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_16x16_8x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_16x32_8x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_8x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_16x32_16x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_16x16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_32x32_16x32:
   case CIK__PIPE_CONFIG__ADDR_SURF_P8_32x64_32x32:
       return 8;
   case CIK__PIPE_CONFIG__ADDR_SURF_P16_32X32_8X16:
   case CIK__PIPE_CONFIG__ADDR_SURF_P16_32X32_16X16:
       return 16;
   default:
       fprintf(stderr, "Invalid CIK pipe configuration, assuming P2\n");
       assert(!"this should never occur");
       return 2;
   }
}

static bool has_syncobj(int fd)
{
	uint64_t value;
	if (drmGetCap(fd, DRM_CAP_SYNCOBJ, &value))
		return false;
	return value ? true : false;
}

bool ac_query_gpu_info(int fd, amdgpu_device_handle dev,
		       struct radeon_info *info,
		       struct amdgpu_gpu_info *amdinfo)
{
	struct amdgpu_buffer_size_alignments alignment_info = {};
	struct amdgpu_heap_info vram, vram_vis, gtt;
	struct drm_amdgpu_info_hw_ip dma = {}, compute = {}, uvd = {};
	struct drm_amdgpu_info_hw_ip uvd_enc = {}, vce = {}, vcn_dec = {};
	struct drm_amdgpu_info_hw_ip vcn_enc = {}, gfx = {};
	uint32_t vce_version = 0, vce_feature = 0, uvd_version = 0, uvd_feature = 0;
	int r, i, j;
	drmDevicePtr devinfo;

	/* Get PCI info. */
	r = drmGetDevice2(fd, 0, &devinfo);
	if (r) {
		fprintf(stderr, "amdgpu: drmGetDevice2 failed.\n");
		return false;
	}
	info->pci_domain = devinfo->businfo.pci->domain;
	info->pci_bus = devinfo->businfo.pci->bus;
	info->pci_dev = devinfo->businfo.pci->dev;
	info->pci_func = devinfo->businfo.pci->func;
	drmFreeDevice(&devinfo);

	/* Query hardware and driver information. */
	r = amdgpu_query_gpu_info(dev, amdinfo);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_gpu_info failed.\n");
		return false;
	}

	r = amdgpu_query_buffer_size_alignment(dev, &alignment_info);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_buffer_size_alignment failed.\n");
		return false;
	}

	r = amdgpu_query_heap_info(dev, AMDGPU_GEM_DOMAIN_VRAM, 0, &vram);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_heap_info(vram) failed.\n");
		return false;
	}

	r = amdgpu_query_heap_info(dev, AMDGPU_GEM_DOMAIN_VRAM,
				AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
				&vram_vis);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_heap_info(vram_vis) failed.\n");
		return false;
	}

	r = amdgpu_query_heap_info(dev, AMDGPU_GEM_DOMAIN_GTT, 0, &gtt);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_heap_info(gtt) failed.\n");
		return false;
	}

	r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_DMA, 0, &dma);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(dma) failed.\n");
		return false;
	}

	r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_GFX, 0, &gfx);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(gfx) failed.\n");
		return false;
	}

	r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_COMPUTE, 0, &compute);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(compute) failed.\n");
		return false;
	}

	r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_UVD, 0, &uvd);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(uvd) failed.\n");
		return false;
	}

	if (info->drm_major == 3 && info->drm_minor >= 17) {
		r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_VCN_DEC, 0, &vcn_dec);
		if (r) {
			fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(vcn_dec) failed.\n");
			return false;
		}
	}

	r = amdgpu_query_firmware_version(dev, AMDGPU_INFO_FW_GFX_ME, 0, 0,
					&info->me_fw_version,
					&info->me_fw_feature);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_firmware_version(me) failed.\n");
		return false;
	}

	r = amdgpu_query_firmware_version(dev, AMDGPU_INFO_FW_GFX_PFP, 0, 0,
					&info->pfp_fw_version,
					&info->pfp_fw_feature);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_firmware_version(pfp) failed.\n");
		return false;
	}

	r = amdgpu_query_firmware_version(dev, AMDGPU_INFO_FW_GFX_CE, 0, 0,
					&info->ce_fw_version,
					&info->ce_fw_feature);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_firmware_version(ce) failed.\n");
		return false;
	}

	r = amdgpu_query_firmware_version(dev, AMDGPU_INFO_FW_UVD, 0, 0,
					&uvd_version, &uvd_feature);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_firmware_version(uvd) failed.\n");
		return false;
	}

	r = amdgpu_query_hw_ip_info(dev, AMDGPU_HW_IP_VCE, 0, &vce);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_hw_ip_info(vce) failed.\n");
		return false;
	}

	r = amdgpu_query_firmware_version(dev, AMDGPU_INFO_FW_VCE, 0, 0,
					&vce_version, &vce_feature);
	if (r) {
		fprintf(stderr, "amdgpu: amdgpu_query_firmware_version(vce) failed.\n");
		return false;
	}

	/* Set chip identification. */
	info->pci_id = amdinfo->asic_id; /* TODO: is this correct? */
	info->vce_harvest_config = amdinfo->vce_harvest_config;

	switch (info->pci_id) {
#define CHIPSET(pci_id, name, cfamily) case pci_id: info->family = CHIP_##cfamily; break;
#include "pci_ids/radeonsi_pci_ids.h"
#undef CHIPSET

	default:
		fprintf(stderr, "amdgpu: Invalid PCI ID.\n");
		return false;
	}

	if (info->family >= CHIP_VEGA10)
		info->chip_class = GFX9;
	else if (info->family >= CHIP_TONGA)
		info->chip_class = VI;
	else if (info->family >= CHIP_BONAIRE)
		info->chip_class = CIK;
	else if (info->family >= CHIP_TAHITI)
		info->chip_class = SI;
	else {
		fprintf(stderr, "amdgpu: Unknown family.\n");
		return false;
	}

	/* Set which chips have dedicated VRAM. */
	info->has_dedicated_vram =
		!(amdinfo->ids_flags & AMDGPU_IDS_FLAGS_FUSION);

	/* Set hardware information. */
	info->gart_size = gtt.heap_size;
	info->vram_size = vram.heap_size;
	info->vram_vis_size = vram_vis.heap_size;
	/* The kernel can split large buffers in VRAM but not in GTT, so large
	 * allocations can fail or cause buffer movement failures in the kernel.
	 */
	info->max_alloc_size = MIN2(info->vram_size * 0.9, info->gart_size * 0.7);
	/* convert the shader clock from KHz to MHz */
	info->max_shader_clock = amdinfo->max_engine_clk / 1000;
	info->max_se = amdinfo->num_shader_engines;
	info->max_sh_per_se = amdinfo->num_shader_arrays_per_engine;
	info->has_hw_decode =
		(uvd.available_rings != 0) || (vcn_dec.available_rings != 0);
	info->uvd_fw_version =
		uvd.available_rings ? uvd_version : 0;
	info->vce_fw_version =
		vce.available_rings ? vce_version : 0;
	info->has_userptr = true;
	info->has_syncobj = has_syncobj(fd);
	info->has_sync_file = info->has_syncobj && info->drm_minor >= 21;
	info->has_ctx_priority = info->drm_minor >= 22;
	info->num_render_backends = amdinfo->rb_pipes;
	info->clock_crystal_freq = amdinfo->gpu_counter_freq;
	if (!info->clock_crystal_freq) {
		fprintf(stderr, "amdgpu: clock crystal frequency is 0, timestamps will be wrong\n");
		info->clock_crystal_freq = 1;
	}
	info->tcc_cache_line_size = 64; /* TC L2 line size on GCN */
	if (info->chip_class == GFX9) {
		info->num_tile_pipes = 1 << G_0098F8_NUM_PIPES(amdinfo->gb_addr_cfg);
		info->pipe_interleave_bytes =
			256 << G_0098F8_PIPE_INTERLEAVE_SIZE_GFX9(amdinfo->gb_addr_cfg);
	} else {
		info->num_tile_pipes = cik_get_num_tile_pipes(amdinfo);
		info->pipe_interleave_bytes =
			256 << G_0098F8_PIPE_INTERLEAVE_SIZE_GFX6(amdinfo->gb_addr_cfg);
	}
	info->has_virtual_memory = true;

	assert(util_is_power_of_two(dma.available_rings + 1));
	assert(util_is_power_of_two(compute.available_rings + 1));

	info->num_sdma_rings = util_bitcount(dma.available_rings);
	info->num_compute_rings = util_bitcount(compute.available_rings);

	/* Get the number of good compute units. */
	info->num_good_compute_units = 0;
	for (i = 0; i < info->max_se; i++)
		for (j = 0; j < info->max_sh_per_se; j++)
			info->num_good_compute_units +=
				util_bitcount(amdinfo->cu_bitmap[i][j]);

	memcpy(info->si_tile_mode_array, amdinfo->gb_tile_mode,
		sizeof(amdinfo->gb_tile_mode));
	info->enabled_rb_mask = amdinfo->enabled_rb_pipes_mask;

	memcpy(info->cik_macrotile_mode_array, amdinfo->gb_macro_tile_mode,
		sizeof(amdinfo->gb_macro_tile_mode));

	info->pte_fragment_size = alignment_info.size_local;
	info->gart_page_size = alignment_info.size_remote;

	if (info->chip_class == SI)
		info->gfx_ib_pad_with_type2 = TRUE;

	unsigned ib_align = 0;
	ib_align = MAX2(ib_align, gfx.ib_start_alignment);
	ib_align = MAX2(ib_align, compute.ib_start_alignment);
	ib_align = MAX2(ib_align, dma.ib_start_alignment);
	ib_align = MAX2(ib_align, uvd.ib_start_alignment);
	ib_align = MAX2(ib_align, uvd_enc.ib_start_alignment);
	ib_align = MAX2(ib_align, vce.ib_start_alignment);
	ib_align = MAX2(ib_align, vcn_dec.ib_start_alignment);
	ib_align = MAX2(ib_align, vcn_enc.ib_start_alignment);
	info->ib_start_alignment = ib_align;

	return true;
}

void ac_compute_driver_uuid(char *uuid, size_t size)
{
	char amd_uuid[] = "AMD-MESA-DRV";

	assert(size >= sizeof(amd_uuid));

	memset(uuid, 0, size);
	strncpy(uuid, amd_uuid, size);
}

void ac_compute_device_uuid(struct radeon_info *info, char *uuid, size_t size)
{
	uint32_t *uint_uuid = (uint32_t*)uuid;

	assert(size >= sizeof(uint32_t)*4);

	/**
	 * Use the device info directly instead of using a sha1. GL/VK UUIDs
	 * are 16 byte vs 20 byte for sha1, and the truncation that would be
	 * required would get rid of part of the little entropy we have.
	 * */
	memset(uuid, 0, size);
	uint_uuid[0] = info->pci_domain;
	uint_uuid[1] = info->pci_bus;
	uint_uuid[2] = info->pci_dev;
	uint_uuid[3] = info->pci_func;
}
