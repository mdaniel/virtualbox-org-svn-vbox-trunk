/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
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
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "nine_helpers.h"
#include "nine_shader.h"

#include "vertexdeclaration9.h"
#include "vertexshader9.h"

#include "device9.h"
#include "pipe/p_context.h"
#include "cso_cache/cso_context.h"

#define DBG_CHANNEL DBG_VERTEXSHADER

HRESULT
NineVertexShader9_ctor( struct NineVertexShader9 *This,
                        struct NineUnknownParams *pParams,
                        const DWORD *pFunction, void *cso )
{
    struct NineDevice9 *device;
    struct nine_shader_info info;
    struct pipe_context *pipe;
    HRESULT hr;
    unsigned i;

    DBG("This=%p pParams=%p pFunction=%p cso=%p\n",
        This, pParams, pFunction, cso);

    hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr))
        return hr;

    if (cso) {
        This->ff_cso = cso;
        return D3D_OK;
    }

    device = This->base.device;

    info.type = PIPE_SHADER_VERTEX;
    info.byte_code = pFunction;
    info.const_i_base = NINE_CONST_I_BASE(device->max_vs_const_f) / 16;
    info.const_b_base = NINE_CONST_B_BASE(device->max_vs_const_f) / 16;
    info.sampler_mask_shadow = 0x0;
    info.sampler_ps1xtypes = 0x0;
    info.fog_enable = 0;
    info.point_size_min = 0;
    info.point_size_max = 0;
    info.swvp_on = !!(device->params.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING);
    info.process_vertices = false;

    pipe = nine_context_get_pipe_acquire(device);
    hr = nine_translate_shader(device, &info, pipe);
    if (hr == D3DERR_INVALIDCALL &&
        (device->params.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING)) {
        /* Retry with a swvp shader. It will require swvp to be on. */
        info.swvp_on = true;
        hr = nine_translate_shader(device, &info, pipe);
    }
    nine_context_get_pipe_release(device);
    if (hr == D3DERR_INVALIDCALL)
        ERR("Encountered buggy shader\n");
    if (FAILED(hr))
        return hr;
    This->byte_code.version = info.version;
    This->swvp_only = info.swvp_on;

    This->byte_code.tokens = mem_dup(pFunction, info.byte_size);
    if (!This->byte_code.tokens)
        return E_OUTOFMEMORY;
    This->byte_code.size = info.byte_size;

    This->variant.cso = info.cso;
    This->last_cso = info.cso;
    This->last_key = (uint32_t) (info.swvp_on << 9);

    This->const_used_size = info.const_used_size;
    This->lconstf = info.lconstf;
    This->sampler_mask = info.sampler_mask;
    This->position_t = info.position_t;
    This->point_size = info.point_size;

    for (i = 0; i < info.num_inputs && i < ARRAY_SIZE(This->input_map); ++i)
        This->input_map[i].ndecl = info.input_map[i];
    This->num_inputs = i;

    return D3D_OK;
}

void
NineVertexShader9_dtor( struct NineVertexShader9 *This )
{
    DBG("This=%p\n", This);

    if (This->base.device) {
        struct pipe_context *pipe = nine_context_get_pipe_multithread(This->base.device);
        struct nine_shader_variant *var = &This->variant;
        struct nine_shader_variant_so *var_so = &This->variant_so;

        do {
            if (var->cso) {
                if (This->base.device->context.cso_shader.vs == var->cso)
                    pipe->bind_vs_state(pipe, NULL);
                pipe->delete_vs_state(pipe, var->cso);
            }
            var = var->next;
        } while (var);

        while (var_so && var_so->vdecl) {
            if (var_so->cso) {
                cso_delete_vertex_shader(This->base.device->cso_sw, var_so->cso );
            }
            var_so = var_so->next;
        }

        if (This->ff_cso) {
            if (This->ff_cso == This->base.device->context.cso_shader.vs)
                pipe->bind_vs_state(pipe, NULL);
            pipe->delete_vs_state(pipe, This->ff_cso);
        }
    }
    nine_shader_variants_free(&This->variant);
    nine_shader_variants_so_free(&This->variant_so);

    FREE((void *)This->byte_code.tokens); /* const_cast */

    FREE(This->lconstf.data);
    FREE(This->lconstf.ranges);

    NineUnknown_dtor(&This->base);
}

HRESULT NINE_WINAPI
NineVertexShader9_GetFunction( struct NineVertexShader9 *This,
                               void *pData,
                               UINT *pSizeOfData )
{
    user_assert(pSizeOfData, D3DERR_INVALIDCALL);

    if (!pData) {
        *pSizeOfData = This->byte_code.size;
        return D3D_OK;
    }
    user_assert(*pSizeOfData >= This->byte_code.size, D3DERR_INVALIDCALL);

    memcpy(pData, This->byte_code.tokens, This->byte_code.size);

    return D3D_OK;
}

void *
NineVertexShader9_GetVariant( struct NineVertexShader9 *This )
{
    /* GetVariant is called from nine_context, thus we can
     * get pipe directly */
    struct pipe_context *pipe = This->base.device->context.pipe;
    void *cso;
    uint64_t key;

    key = This->next_key;
    if (key == This->last_key)
        return This->last_cso;

    cso = nine_shader_variant_get(&This->variant, key);
    if (!cso) {
        struct NineDevice9 *device = This->base.device;
        struct nine_shader_info info;
        HRESULT hr;

        info.type = PIPE_SHADER_VERTEX;
        info.const_i_base = NINE_CONST_I_BASE(device->max_vs_const_f) / 16;
        info.const_b_base = NINE_CONST_B_BASE(device->max_vs_const_f) / 16;
        info.byte_code = This->byte_code.tokens;
        info.sampler_mask_shadow = key & 0xf;
        info.fog_enable = device->context.rs[D3DRS_FOGENABLE];
        info.point_size_min = asfloat(device->context.rs[D3DRS_POINTSIZE_MIN]);
        info.point_size_max = asfloat(device->context.rs[D3DRS_POINTSIZE_MAX]);
        info.swvp_on = device->context.swvp;
        info.process_vertices = false;

        hr = nine_translate_shader(This->base.device, &info, pipe);
        if (FAILED(hr))
            return NULL;
        nine_shader_variant_add(&This->variant, key, info.cso);
        cso = info.cso;
    }

    This->last_key = key;
    This->last_cso = cso;

    return cso;
}

void *
NineVertexShader9_GetVariantProcessVertices( struct NineVertexShader9 *This,
                                             struct NineVertexDeclaration9 *vdecl_out,
                                             struct pipe_stream_output_info *so )
{
    struct nine_shader_info info;
    HRESULT hr;
    void *cso;

    cso = nine_shader_variant_so_get(&This->variant_so, vdecl_out, so);
    if (cso)
        return cso;

    info.type = PIPE_SHADER_VERTEX;
    info.const_i_base = 0;
    info.const_b_base = 0;
    info.byte_code = This->byte_code.tokens;
    info.sampler_mask_shadow = 0;
    info.fog_enable = false;
    info.point_size_min = 0;
    info.point_size_max = 0;
    info.swvp_on = true;
    info.vdecl_out = vdecl_out;
    info.process_vertices = true;
    hr = nine_translate_shader(This->base.device, &info, This->base.device->pipe_sw);
    if (FAILED(hr))
        return NULL;
    *so = info.so;
    nine_shader_variant_so_add(&This->variant_so, vdecl_out, so, info.cso);
    return info.cso;
}

IDirect3DVertexShader9Vtbl NineVertexShader9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineUnknown_GetDevice,
    (void *)NineVertexShader9_GetFunction
};

static const GUID *NineVertexShader9_IIDs[] = {
    &IID_IDirect3DVertexShader9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineVertexShader9_new( struct NineDevice9 *pDevice,
                       struct NineVertexShader9 **ppOut,
                       const DWORD *pFunction, void *cso )
{
    NINE_DEVICE_CHILD_NEW(VertexShader9, ppOut, pDevice, pFunction, cso);
}
