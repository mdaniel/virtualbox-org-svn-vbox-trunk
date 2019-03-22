/*
 * Copyright (C) 2009 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "nouveau_driver.h"
#include "nouveau_context.h"
#include "nouveau_gldefs.h"
#include "nouveau_texture.h"
#include "nv20_3d.xml.h"
#include "nouveau_util.h"
#include "nv20_driver.h"
#include "main/samplerobj.h"

void
nv20_emit_tex_gen(struct gl_context *ctx, int emit)
{
	const int i = emit - NOUVEAU_STATE_TEX_GEN0;
	struct nouveau_context *nctx = to_nouveau_context(ctx);
	struct nouveau_pushbuf *push = context_push(ctx);
	struct gl_texture_unit *unit = &ctx->Texture.Unit[i];
	int j;

	for (j = 0; j < 4; j++) {
		if (nctx->fallback == HWTNL && (unit->TexGenEnabled & 1 << j)) {
			struct gl_texgen *coord = get_texgen_coord(unit, j);
			float *k = get_texgen_coeff(coord);

			if (k) {
				BEGIN_NV04(push, NV20_3D(TEX_GEN_COEFF(i, j)), 4);
				PUSH_DATAp(push, k, 4);
			}

			BEGIN_NV04(push, NV20_3D(TEX_GEN_MODE(i, j)), 1);
			PUSH_DATA (push, nvgl_texgen_mode(coord->Mode));

		} else {
			BEGIN_NV04(push, NV20_3D(TEX_GEN_MODE(i, j)), 1);
			PUSH_DATA (push, 0);
		}
	}
}

void
nv20_emit_tex_mat(struct gl_context *ctx, int emit)
{
	const int i = emit - NOUVEAU_STATE_TEX_MAT0;
	struct nouveau_context *nctx = to_nouveau_context(ctx);
	struct nouveau_pushbuf *push = context_push(ctx);

	if (nctx->fallback == HWTNL &&
	    (ctx->Texture._TexMatEnabled & 1 << i)) {
		BEGIN_NV04(push, NV20_3D(TEX_MATRIX_ENABLE(i)), 1);
		PUSH_DATA (push, 1);

		BEGIN_NV04(push, NV20_3D(TEX_MATRIX(i,0)), 16);
		PUSH_DATAm(push, ctx->TextureMatrixStack[i].Top->m);

	} else {
		BEGIN_NV04(push, NV20_3D(TEX_MATRIX_ENABLE(i)), 1);
		PUSH_DATA (push, 0);
	}
}

static uint32_t
get_tex_format_pot(struct gl_texture_image *ti)
{
	switch (ti->TexFormat) {
	case MESA_FORMAT_B8G8R8A8_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A8R8G8B8;

	case MESA_FORMAT_B5G5R5A1_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A1R5G5B5;

	case MESA_FORMAT_B4G4R4A4_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A4R4G4B4;

	case MESA_FORMAT_B8G8R8X8_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_X8R8G8B8;

	case MESA_FORMAT_B5G6R5_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_R5G6B5;

	case MESA_FORMAT_A_UNORM8:
	case MESA_FORMAT_I_UNORM8:
		return NV20_3D_TEX_FORMAT_FORMAT_I8;

	case MESA_FORMAT_L_UNORM8:
		return NV20_3D_TEX_FORMAT_FORMAT_L8;

	case MESA_FORMAT_RGB_DXT1:
	case MESA_FORMAT_RGBA_DXT1:
		return NV20_3D_TEX_FORMAT_FORMAT_DXT1;

	case MESA_FORMAT_RGBA_DXT3:
		return NV20_3D_TEX_FORMAT_FORMAT_DXT3;

	case MESA_FORMAT_RGBA_DXT5:
		return NV20_3D_TEX_FORMAT_FORMAT_DXT5;

	default:
		assert(0);
	}
}

static uint32_t
get_tex_format_rect(struct gl_texture_image *ti)
{
	switch (ti->TexFormat) {
	case MESA_FORMAT_B8G8R8A8_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A8R8G8B8_RECT;

	case MESA_FORMAT_B5G5R5A1_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A1R5G5B5_RECT;

	case MESA_FORMAT_B4G4R4A4_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_A4R4G4B4_RECT;

	case MESA_FORMAT_B8G8R8X8_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_R8G8B8_RECT;

	case MESA_FORMAT_B5G6R5_UNORM:
		return NV20_3D_TEX_FORMAT_FORMAT_R5G6B5_RECT;

	case MESA_FORMAT_L_UNORM8:
		return NV20_3D_TEX_FORMAT_FORMAT_L8_RECT;

	case MESA_FORMAT_A_UNORM8:
	case MESA_FORMAT_I_UNORM8:
		return NV20_3D_TEX_FORMAT_FORMAT_I8_RECT;

	default:
		assert(0);
	}
}

void
nv20_emit_tex_obj(struct gl_context *ctx, int emit)
{
	const int i = emit - NOUVEAU_STATE_TEX_OBJ0;
	struct nouveau_pushbuf *push = context_push(ctx);
	const int bo_flags = NOUVEAU_BO_RD | NOUVEAU_BO_GART | NOUVEAU_BO_VRAM;
	struct gl_texture_object *t;
	struct nouveau_surface *s;
	struct gl_texture_image *ti;
	const struct gl_sampler_object *sa;
	uint8_t r, g, b, a;
	uint32_t tx_format, tx_filter, tx_wrap, tx_bcolor, tx_enable;

	PUSH_RESET(push, BUFCTX_TEX(i));

	if (!ctx->Texture.Unit[i]._Current) {
		BEGIN_NV04(push, NV20_3D(TEX_ENABLE(i)), 1);
		PUSH_DATA (push, 0);

		context_dirty(ctx, TEX_SHADER);
		return;
	}

	t = ctx->Texture.Unit[i]._Current;
	s = &to_nouveau_texture(t)->surfaces[t->BaseLevel];
	ti = t->Image[0][t->BaseLevel];
	sa = _mesa_get_samplerobj(ctx, i);

	if (!nouveau_texture_validate(ctx, t))
		return;

	/* Recompute the texturing registers. */
	tx_format = ti->DepthLog2 << 28
		| ti->HeightLog2 << 24
		| ti->WidthLog2 << 20
		| NV20_3D_TEX_FORMAT_DIMS_2D
		| NV20_3D_TEX_FORMAT_NO_BORDER
		| 1 << 16;

	switch (t->Target) {
	case GL_TEXTURE_1D:
		tx_wrap = NV20_3D_TEX_WRAP_R_CLAMP_TO_EDGE
			| NV20_3D_TEX_WRAP_T_CLAMP_TO_EDGE
			| nvgl_wrap_mode_nv20(sa->WrapS) << 0;
		break;

	default:
		tx_wrap = nvgl_wrap_mode_nv20(sa->WrapR) << 16
			| nvgl_wrap_mode_nv20(sa->WrapT) << 8
			| nvgl_wrap_mode_nv20(sa->WrapS) << 0;
		break;
	}

	tx_filter = nvgl_filter_mode(sa->MagFilter) << 24
		| nvgl_filter_mode(sa->MinFilter) << 16
		| 2 << 12;

	r = FLOAT_TO_UBYTE(sa->BorderColor.f[0]);
	g = FLOAT_TO_UBYTE(sa->BorderColor.f[1]);
	b = FLOAT_TO_UBYTE(sa->BorderColor.f[2]);
	a = FLOAT_TO_UBYTE(sa->BorderColor.f[3]);
	switch (ti->_BaseFormat) {
	case GL_LUMINANCE:
		a = 0xff;
		/* fallthrough */
	case GL_LUMINANCE_ALPHA:
		g = b = r;
		break;
	case GL_RGB:
		a = 0xff;
		break;
	case GL_INTENSITY:
		g = b = a = r;
		break;
	case GL_ALPHA:
		r = g = b = 0;
		break;
	}
	tx_bcolor = b << 0 | g << 8 | r << 16 | a << 24;

	tx_enable = NV20_3D_TEX_ENABLE_ENABLE
		| log2i(sa->MaxAnisotropy) << 4;

	if (t->Target == GL_TEXTURE_RECTANGLE) {
		BEGIN_NV04(push, NV20_3D(TEX_NPOT_PITCH(i)), 1);
		PUSH_DATA (push, s->pitch << 16);
		BEGIN_NV04(push, NV20_3D(TEX_NPOT_SIZE(i)), 1);
		PUSH_DATA (push, s->width << 16 | s->height);

		tx_format |= get_tex_format_rect(ti);
	} else {
		tx_format |= get_tex_format_pot(ti);
	}

	if (sa->MinFilter != GL_NEAREST &&
	    sa->MinFilter != GL_LINEAR) {
		int lod_min = sa->MinLod;
		int lod_max = MIN2(sa->MaxLod, t->_MaxLambda);
		int lod_bias = sa->LodBias
			+ ctx->Texture.Unit[i].LodBias;

		lod_max = CLAMP(lod_max, 0, 15);
		lod_min = CLAMP(lod_min, 0, 15);
		lod_bias = CLAMP(lod_bias, 0, 15);

		tx_format |= NV20_3D_TEX_FORMAT_MIPMAP;
		tx_filter |= lod_bias << 8;
		tx_enable |= lod_min << 26
			| lod_max << 14;
	}

	/* Write it to the hardware. */
	BEGIN_NV04(push, NV20_3D(TEX_FORMAT(i)), 1);
	PUSH_MTHD (push, NV20_3D(TEX_FORMAT(i)), BUFCTX_TEX(i),
			 s->bo, tx_format, bo_flags | NOUVEAU_BO_OR,
			 NV20_3D_TEX_FORMAT_DMA0,
			 NV20_3D_TEX_FORMAT_DMA1);

	BEGIN_NV04(push, NV20_3D(TEX_OFFSET(i)), 1);
	PUSH_MTHDl(push, NV20_3D(TEX_OFFSET(i)), BUFCTX_TEX(i),
			 s->bo, s->offset, bo_flags);

	BEGIN_NV04(push, NV20_3D(TEX_WRAP(i)), 1);
	PUSH_DATA (push, tx_wrap);

	BEGIN_NV04(push, NV20_3D(TEX_FILTER(i)), 1);
	PUSH_DATA (push, tx_filter);

	BEGIN_NV04(push, NV20_3D(TEX_BORDER_COLOR(i)), 1);
	PUSH_DATA (push, tx_bcolor);

	BEGIN_NV04(push, NV20_3D(TEX_ENABLE(i)), 1);
	PUSH_DATA (push, tx_enable);

	context_dirty(ctx, TEX_SHADER);
}

void
nv20_emit_tex_shader(struct gl_context *ctx, int emit)
{
	struct nouveau_pushbuf *push = context_push(ctx);
	uint32_t tx_shader_op = 0;
	int i;

	for (i = 0; i < NV20_TEXTURE_UNITS; i++) {
		if (!ctx->Texture.Unit[i]._Current)
			continue;

		tx_shader_op |= NV20_3D_TEX_SHADER_OP_TX0_TEXTURE_2D << 5 * i;
	}

	BEGIN_NV04(push, NV20_3D(TEX_SHADER_OP), 1);
	PUSH_DATA (push, tx_shader_op);
}
