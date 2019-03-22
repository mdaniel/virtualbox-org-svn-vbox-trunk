/*
 * Copyright 2010 Jerome Glisse <glisse@freedesktop.org>
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
#ifndef R600_SHADER_H
#define R600_SHADER_H

#include "r600_asm.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Valid shader configurations:
 *
 * API shaders       VS | TCS | TES | GS |pass| PS
 * are compiled as:     |     |     |    |thru|
 *                      |     |     |    |    |
 * Only VS & PS:     VS | --  | --  | -- | -- | PS
 * With GS:          ES | --  | --  | GS | VS | PS
 * With Tessel.:     LS | HS  | VS  | -- | -- | PS
 * With both:        LS | HS  | ES  | GS | VS | PS
 */

struct r600_shader_io {
	unsigned		name;
	unsigned		gpr;
	unsigned		done;
	int			sid;
	int			spi_sid;
	unsigned		interpolate;
	unsigned		ij_index;
	unsigned		interpolate_location; //  TGSI_INTERPOLATE_LOC_CENTER, CENTROID, SAMPLE
	unsigned		lds_pos; /* for evergreen */
	unsigned		back_color_input;
	unsigned		write_mask;
	int			ring_offset;
};

struct r600_shader {
	unsigned		processor_type;
	struct r600_bytecode		bc;
	unsigned		ninput;
	unsigned		noutput;
	unsigned		nlds;
	unsigned		nsys_inputs;
	struct r600_shader_io	input[64];
	struct r600_shader_io	output[64];
	boolean			uses_kill;
	boolean			fs_write_all;
	boolean			two_side;
	/* Number of color outputs in the TGSI shader,
	 * sometimes it could be higher than nr_cbufs (bug?).
	 * Also with writes_all property on eg+ it will be set to max CB number */
	unsigned		nr_ps_max_color_exports;
	/* Real number of ps color exports compiled in the bytecode */
	unsigned		nr_ps_color_exports;
	/* bit n is set if the shader writes gl_ClipDistance[n] */
	unsigned		clip_dist_write;
	boolean			vs_position_window_space;
	/* flag is set if the shader writes VS_OUT_MISC_VEC (e.g. for PSIZE) */
	boolean			vs_out_misc_write;
	boolean			vs_out_point_size;
	boolean			vs_out_layer;
	boolean			vs_out_viewport;
	boolean			vs_out_edgeflag;
	boolean			has_txq_cube_array_z_comp;
	boolean			uses_tex_buffers;
	boolean                 gs_prim_id_input;

	uint8_t			ps_conservative_z;

	/* Size in bytes of a data item in the ring(s) (single vertex data).
	   Stages with only one ring items 123 will be set to 0. */
	unsigned		ring_item_sizes[4];

	unsigned		indirect_files;
	unsigned		max_arrays;
	unsigned		num_arrays;
	unsigned		vs_as_es;
	unsigned		vs_as_ls;
	unsigned		vs_as_gs_a;
	unsigned                tes_as_es;
	unsigned                tcs_prim_mode;
	unsigned                ps_prim_id_input;
	struct r600_shader_array * arrays;

	boolean			uses_doubles;
};

union r600_shader_key {
	struct {
		unsigned	nr_cbufs:4;
		unsigned	color_two_side:1;
		unsigned	alpha_to_one:1;
	} ps;
	struct {
		unsigned	prim_id_out:8;
		unsigned	as_es:1; /* export shader */
		unsigned	as_ls:1; /* local shader */
		unsigned	as_gs_a:1;
	} vs;
	struct {
		unsigned	as_es:1;
	} tes;
	struct {
		unsigned	prim_mode:3;
	} tcs;
};

struct r600_shader_array {
	unsigned gpr_start;
	unsigned gpr_count;
	unsigned comp_mask;
};

struct r600_pipe_shader {
	struct r600_pipe_shader_selector *selector;
	struct r600_pipe_shader	*next_variant;
	/* for GS - corresponding copy shader (installed as VS) */
	struct r600_pipe_shader *gs_copy_shader;
	struct r600_shader	shader;
	struct r600_command_buffer command_buffer; /* register writes */
	struct r600_resource	*bo;
	unsigned		sprite_coord_enable;
	unsigned		flatshade;
	unsigned		pa_cl_vs_out_cntl;
	unsigned		nr_ps_color_outputs;
	union r600_shader_key	key;
	unsigned		db_shader_control;
	unsigned		ps_depth_export;
	unsigned		enabled_stream_buffers_mask;
};

/* return the table index 0-5 for TGSI_INTERPOLATE_LINEAR/PERSPECTIVE and
 TGSI_INTERPOLATE_LOC_CENTER/SAMPLE/COUNT. Other input values return -1. */
int eg_get_interpolator_index(unsigned interpolate, unsigned location);

int r600_get_lds_unique_index(unsigned semantic_name, unsigned index);

#ifdef __cplusplus
}  // extern "C"
#endif


#endif
