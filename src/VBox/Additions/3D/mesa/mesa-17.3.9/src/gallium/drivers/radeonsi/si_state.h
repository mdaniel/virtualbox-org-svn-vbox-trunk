/*
 * Copyright 2012 Advanced Micro Devices, Inc.
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
 *
 * Authors:
 *      Christian König <christian.koenig@amd.com>
 */

#ifndef SI_STATE_H
#define SI_STATE_H

#include "si_pm4.h"
#include "radeon/r600_pipe_common.h"

#include "pipebuffer/pb_slab.h"

#define SI_NUM_GRAPHICS_SHADERS (PIPE_SHADER_TESS_EVAL+1)
#define SI_NUM_SHADERS (PIPE_SHADER_COMPUTE+1)

#define SI_MAX_ATTRIBS			16
#define SI_NUM_VERTEX_BUFFERS		SI_MAX_ATTRIBS
#define SI_NUM_SAMPLERS			32 /* OpenGL textures units per shader */
#define SI_NUM_CONST_BUFFERS		16
#define SI_NUM_IMAGES			16
#define SI_NUM_SHADER_BUFFERS		16

struct si_screen;
struct si_shader;
struct si_shader_selector;

struct si_state_blend {
	struct si_pm4_state	pm4;
	uint32_t		cb_target_mask;
	/* Set 0xf or 0x0 (4 bits) per render target if the following is
	 * true. ANDed with spi_shader_col_format.
	 */
	unsigned		cb_target_enabled_4bit;
	unsigned		blend_enable_4bit;
	unsigned		need_src_alpha_4bit;
	unsigned		commutative_4bit;
	bool			alpha_to_coverage:1;
	bool			alpha_to_one:1;
	bool			dual_src_blend:1;
	bool			logicop_enable:1;
};

struct si_state_rasterizer {
	struct si_pm4_state	pm4;
	/* poly offset states for 16-bit, 24-bit, and 32-bit zbuffers */
	struct si_pm4_state	*pm4_poly_offset;
	unsigned		pa_sc_line_stipple;
	unsigned		pa_cl_clip_cntl;
	float			line_width;
	float			max_point_size;
	unsigned		sprite_coord_enable:8;
	unsigned		clip_plane_enable:8;
	unsigned		flatshade:1;
	unsigned		two_side:1;
	unsigned		multisample_enable:1;
	unsigned		force_persample_interp:1;
	unsigned		line_stipple_enable:1;
	unsigned		poly_stipple_enable:1;
	unsigned		line_smooth:1;
	unsigned		poly_smooth:1;
	unsigned		uses_poly_offset:1;
	unsigned		clamp_fragment_color:1;
	unsigned		clamp_vertex_color:1;
	unsigned		rasterizer_discard:1;
	unsigned		scissor_enable:1;
	unsigned		clip_halfz:1;
};

struct si_dsa_stencil_ref_part {
	uint8_t			valuemask[2];
	uint8_t			writemask[2];
};

struct si_dsa_order_invariance {
	/** Whether the final result in Z/S buffers is guaranteed to be
	 * invariant under changes to the order in which fragments arrive. */
	bool zs:1;

	/** Whether the set of fragments that pass the combined Z/S test is
	 * guaranteed to be invariant under changes to the order in which
	 * fragments arrive. */
	bool pass_set:1;

	/** Whether the last fragment that passes the combined Z/S test at each
	 * sample is guaranteed to be invariant under changes to the order in
	 * which fragments arrive. */
	bool pass_last:1;
};

struct si_state_dsa {
	struct si_pm4_state		pm4;
	struct si_dsa_stencil_ref_part	stencil_ref;

	/* 0 = without stencil buffer, 1 = when both Z and S buffers are present */
	struct si_dsa_order_invariance	order_invariance[2];

	ubyte				alpha_func:3;
	bool				depth_enabled:1;
	bool				depth_write_enabled:1;
	bool				stencil_enabled:1;
	bool				stencil_write_enabled:1;
	bool				db_can_write:1;

};

struct si_stencil_ref {
	struct r600_atom		atom;
	struct pipe_stencil_ref		state;
	struct si_dsa_stencil_ref_part	dsa_part;
};

struct si_vertex_elements
{
	uint32_t			instance_divisors[SI_MAX_ATTRIBS];
	uint32_t			rsrc_word3[SI_MAX_ATTRIBS];
	uint16_t			src_offset[SI_MAX_ATTRIBS];
	uint8_t				fix_fetch[SI_MAX_ATTRIBS];
	uint8_t				format_size[SI_MAX_ATTRIBS];
	uint8_t				vertex_buffer_index[SI_MAX_ATTRIBS];

	uint8_t				count;
	bool				uses_instance_divisors;

	uint16_t			first_vb_use_mask;
	/* Vertex buffer descriptor list size aligned for optimal prefetch. */
	uint16_t			desc_list_byte_size;
	uint16_t			instance_divisor_is_one; /* bitmask of inputs */
	uint16_t			instance_divisor_is_fetched;  /* bitmask of inputs */
};

union si_state {
	struct {
		struct si_state_blend		*blend;
		struct si_state_rasterizer	*rasterizer;
		struct si_state_dsa		*dsa;
		struct si_pm4_state		*poly_offset;
		struct si_pm4_state		*ls;
		struct si_pm4_state		*hs;
		struct si_pm4_state		*es;
		struct si_pm4_state		*gs;
		struct si_pm4_state		*vgt_shader_config;
		struct si_pm4_state		*vs;
		struct si_pm4_state		*ps;
	} named;
	struct si_pm4_state	*array[0];
};

#define SI_NUM_STATES (sizeof(union si_state) / sizeof(struct si_pm4_state *))

union si_state_atoms {
	struct {
		/* The order matters. */
		struct r600_atom *render_cond;
		struct r600_atom *streamout_begin;
		struct r600_atom *streamout_enable; /* must be after streamout_begin */
		struct r600_atom *framebuffer;
		struct r600_atom *msaa_sample_locs;
		struct r600_atom *db_render_state;
		struct r600_atom *dpbb_state;
		struct r600_atom *msaa_config;
		struct r600_atom *sample_mask;
		struct r600_atom *cb_render_state;
		struct r600_atom *blend_color;
		struct r600_atom *clip_regs;
		struct r600_atom *clip_state;
		struct r600_atom *shader_pointers;
		struct r600_atom *scissors;
		struct r600_atom *viewports;
		struct r600_atom *stencil_ref;
		struct r600_atom *spi_map;
		struct r600_atom *scratch_state;
	} s;
	struct r600_atom *array[0];
};

#define SI_NUM_ATOMS (sizeof(union si_state_atoms)/sizeof(struct r600_atom*))

struct si_shader_data {
	struct r600_atom	atom;
	uint32_t		sh_base[SI_NUM_SHADERS];
};

/* Private read-write buffer slots. */
enum {
	SI_ES_RING_ESGS,
	SI_GS_RING_ESGS,

	SI_RING_GSVS,

	SI_VS_STREAMOUT_BUF0,
	SI_VS_STREAMOUT_BUF1,
	SI_VS_STREAMOUT_BUF2,
	SI_VS_STREAMOUT_BUF3,

	SI_HS_CONST_DEFAULT_TESS_LEVELS,
	SI_VS_CONST_INSTANCE_DIVISORS,
	SI_VS_CONST_CLIP_PLANES,
	SI_PS_CONST_POLY_STIPPLE,
	SI_PS_CONST_SAMPLE_POSITIONS,

	SI_NUM_RW_BUFFERS,
};

/* Indices into sctx->descriptors, laid out so that gfx and compute pipelines
 * are contiguous:
 *
 *  0 - rw buffers
 *  1 - vertex const and shader buffers
 *  2 - vertex samplers and images
 *  3 - fragment const and shader buffer
 *   ...
 *  11 - compute const and shader buffers
 *  12 - compute samplers and images
 */
enum {
	SI_SHADER_DESCS_CONST_AND_SHADER_BUFFERS,
	SI_SHADER_DESCS_SAMPLERS_AND_IMAGES,
	SI_NUM_SHADER_DESCS,
};

#define SI_DESCS_RW_BUFFERS            0
#define SI_DESCS_FIRST_SHADER          1
#define SI_DESCS_FIRST_COMPUTE         (SI_DESCS_FIRST_SHADER + \
                                        PIPE_SHADER_COMPUTE * SI_NUM_SHADER_DESCS)
#define SI_NUM_DESCS                   (SI_DESCS_FIRST_SHADER + \
                                        SI_NUM_SHADERS * SI_NUM_SHADER_DESCS)

#define SI_DESCS_SHADER_MASK(name) \
	u_bit_consecutive(SI_DESCS_FIRST_SHADER + \
			  PIPE_SHADER_##name * SI_NUM_SHADER_DESCS, \
			  SI_NUM_SHADER_DESCS)

/* This represents descriptors in memory, such as buffer resources,
 * image resources, and sampler states.
 */
struct si_descriptors {
	/* The list of descriptors in malloc'd memory. */
	uint32_t *list;
	/* The list in mapped GPU memory. */
	uint32_t *gpu_list;

	/* The buffer where the descriptors have been uploaded. */
	struct r600_resource *buffer;
	uint64_t gpu_address;

	/* The maximum number of descriptors. */
	uint32_t num_elements;

	/* Slots that are used by currently-bound shaders.
	 * It determines which slots are uploaded.
	 */
	uint32_t first_active_slot;
	uint32_t num_active_slots;

	/* The SGPR index where the 64-bit pointer to the descriptor array will
	 * be stored. */
	ubyte shader_userdata_offset;
	/* The size of one descriptor. */
	ubyte element_dw_size;
	/* If there is only one slot enabled, bind it directly instead of
	 * uploading descriptors. -1 if disabled. */
	signed char slot_index_to_bind_directly;
};

struct si_buffer_resources {
	struct pipe_resource		**buffers; /* this has num_buffers elements */

	enum radeon_bo_usage		shader_usage:4; /* READ, WRITE, or READWRITE */
	enum radeon_bo_usage		shader_usage_constbuf:4;
	enum radeon_bo_priority		priority:6;
	enum radeon_bo_priority		priority_constbuf:6;

	/* The i-th bit is set if that element is enabled (non-NULL resource). */
	unsigned			enabled_mask;
};

#define si_pm4_block_idx(member) \
	(offsetof(union si_state, named.member) / sizeof(struct si_pm4_state *))

#define si_pm4_state_changed(sctx, member) \
	((sctx)->queued.named.member != (sctx)->emitted.named.member)

#define si_pm4_state_enabled_and_changed(sctx, member) \
	((sctx)->queued.named.member && si_pm4_state_changed(sctx, member))

#define si_pm4_bind_state(sctx, member, value) \
	do { \
		(sctx)->queued.named.member = (value); \
		(sctx)->dirty_states |= 1 << si_pm4_block_idx(member); \
	} while(0)

#define si_pm4_delete_state(sctx, member, value) \
	do { \
		if ((sctx)->queued.named.member == (value)) { \
			(sctx)->queued.named.member = NULL; \
		} \
		si_pm4_free_state(sctx, (struct si_pm4_state *)(value), \
				  si_pm4_block_idx(member)); \
	} while(0)

/* si_descriptors.c */
void si_set_mutable_tex_desc_fields(struct si_screen *sscreen,
				    struct r600_texture *tex,
				    const struct legacy_surf_level *base_level_info,
				    unsigned base_level, unsigned first_level,
				    unsigned block_width, bool is_stencil,
				    uint32_t *state);
void si_get_pipe_constant_buffer(struct si_context *sctx, uint shader,
				 uint slot, struct pipe_constant_buffer *cbuf);
void si_get_shader_buffers(struct si_context *sctx,
			   enum pipe_shader_type shader,
			   uint start_slot, uint count,
			   struct pipe_shader_buffer *sbuf);
void si_set_ring_buffer(struct pipe_context *ctx, uint slot,
			struct pipe_resource *buffer,
			unsigned stride, unsigned num_records,
			bool add_tid, bool swizzle,
			unsigned element_size, unsigned index_stride, uint64_t offset);
void si_init_all_descriptors(struct si_context *sctx);
bool si_upload_vertex_buffer_descriptors(struct si_context *sctx);
bool si_upload_graphics_shader_descriptors(struct si_context *sctx);
bool si_upload_compute_shader_descriptors(struct si_context *sctx);
void si_release_all_descriptors(struct si_context *sctx);
void si_all_descriptors_begin_new_cs(struct si_context *sctx);
void si_all_resident_buffers_begin_new_cs(struct si_context *sctx);
void si_upload_const_buffer(struct si_context *sctx, struct r600_resource **rbuffer,
			    const uint8_t *ptr, unsigned size, uint32_t *const_offset);
void si_update_all_texture_descriptors(struct si_context *sctx);
void si_shader_change_notify(struct si_context *sctx);
void si_update_needs_color_decompress_masks(struct si_context *sctx);
void si_emit_graphics_shader_pointers(struct si_context *sctx,
                                      struct r600_atom *atom);
void si_emit_compute_shader_pointers(struct si_context *sctx);
void si_set_rw_buffer(struct si_context *sctx,
		      uint slot, const struct pipe_constant_buffer *input);
void si_set_active_descriptors(struct si_context *sctx, unsigned desc_idx,
			       uint64_t new_active_mask);
void si_set_active_descriptors_for_shader(struct si_context *sctx,
					  struct si_shader_selector *sel);
bool si_bindless_descriptor_can_reclaim_slab(void *priv,
					     struct pb_slab_entry *entry);
struct pb_slab *si_bindless_descriptor_slab_alloc(void *priv, unsigned heap,
						  unsigned entry_size,
						  unsigned group_index);
void si_bindless_descriptor_slab_free(void *priv, struct pb_slab *pslab);

/* si_state.c */
struct si_shader_selector;

void si_init_atom(struct si_context *sctx, struct r600_atom *atom,
		  struct r600_atom **list_elem,
		  void (*emit_func)(struct si_context *ctx, struct r600_atom *state));
void si_init_state_functions(struct si_context *sctx);
void si_init_screen_state_functions(struct si_screen *sscreen);
void
si_make_buffer_descriptor(struct si_screen *screen, struct r600_resource *buf,
			  enum pipe_format format,
			  unsigned offset, unsigned size,
			  uint32_t *state);
void
si_make_texture_descriptor(struct si_screen *screen,
			   struct r600_texture *tex,
			   bool sampler,
			   enum pipe_texture_target target,
			   enum pipe_format pipe_format,
			   const unsigned char state_swizzle[4],
			   unsigned first_level, unsigned last_level,
			   unsigned first_layer, unsigned last_layer,
			   unsigned width, unsigned height, unsigned depth,
			   uint32_t *state,
			   uint32_t *fmask_state);
struct pipe_sampler_view *
si_create_sampler_view_custom(struct pipe_context *ctx,
			      struct pipe_resource *texture,
			      const struct pipe_sampler_view *state,
			      unsigned width0, unsigned height0,
			      unsigned force_level);
void si_update_fb_dirtiness_after_rendering(struct si_context *sctx);

/* si_state_binning.c */
void si_emit_dpbb_state(struct si_context *sctx, struct r600_atom *state);

/* si_state_shaders.c */
bool si_update_shaders(struct si_context *sctx);
void si_init_shader_functions(struct si_context *sctx);
bool si_init_shader_cache(struct si_screen *sscreen);
void si_destroy_shader_cache(struct si_screen *sscreen);
void si_get_active_slot_masks(const struct tgsi_shader_info *info,
			      uint32_t *const_and_shader_buffers,
			      uint64_t *samplers_and_images);
void *si_get_blit_vs(struct si_context *sctx, enum blitter_attrib_type type,
		     unsigned num_layers);

/* si_state_draw.c */
void si_init_ia_multi_vgt_param_table(struct si_context *sctx);
void si_emit_cache_flush(struct si_context *sctx);
void si_draw_vbo(struct pipe_context *ctx, const struct pipe_draw_info *dinfo);
void si_draw_rectangle(struct blitter_context *blitter,
		       void *vertex_elements_cso,
		       blitter_get_vs_func get_vs,
		       int x1, int y1, int x2, int y2,
		       float depth, unsigned num_instances,
		       enum blitter_attrib_type type,
		       const union blitter_attrib *attrib);
void si_trace_emit(struct si_context *sctx);

/* si_state_msaa.c */
void si_init_msaa_functions(struct si_context *sctx);
void si_emit_sample_locations(struct radeon_winsys_cs *cs, int nr_samples);

/* si_state_streamout.c */
void si_streamout_buffers_dirty(struct si_context *sctx);
void si_emit_streamout_end(struct si_context *sctx);
void si_update_prims_generated_query_state(struct si_context *sctx,
					   unsigned type, int diff);
void si_init_streamout_functions(struct si_context *sctx);


static inline unsigned
si_tile_mode_index(struct r600_texture *rtex, unsigned level, bool stencil)
{
	if (stencil)
		return rtex->surface.u.legacy.stencil_tiling_index[level];
	else
		return rtex->surface.u.legacy.tiling_index[level];
}

static inline unsigned si_get_constbuf_slot(unsigned slot)
{
	/* Constant buffers are in slots [16..31], ascending */
	return SI_NUM_SHADER_BUFFERS + slot;
}

static inline unsigned si_get_shaderbuf_slot(unsigned slot)
{
	/* shader buffers are in slots [15..0], descending */
	return SI_NUM_SHADER_BUFFERS - 1 - slot;
}

static inline unsigned si_get_sampler_slot(unsigned slot)
{
	/* samplers are in slots [8..39], ascending */
	return SI_NUM_IMAGES / 2 + slot;
}

static inline unsigned si_get_image_slot(unsigned slot)
{
	/* images are in slots [15..0] (sampler slots [7..0]), descending */
	return SI_NUM_IMAGES - 1 - slot;
}

#endif
