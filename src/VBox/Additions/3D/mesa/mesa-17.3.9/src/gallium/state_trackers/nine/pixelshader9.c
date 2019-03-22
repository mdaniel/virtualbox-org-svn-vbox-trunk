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

#include "pixelshader9.h"

#include "device9.h"
#include "pipe/p_context.h"

#define DBG_CHANNEL DBG_PIXELSHADER

HRESULT
NinePixelShader9_ctor( struct NinePixelShader9 *This,
                       struct NineUnknownParams *pParams,
                       const DWORD *pFunction, void *cso )
{
    struct NineDevice9 *device;
    struct nine_shader_info info;
    struct pipe_context *pipe;
    HRESULT hr;

    DBG("This=%p pParams=%p pFunction=%p cso=%p\n", This, pParams, pFunction, cso);

    hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr))
        return hr;

    if (cso) {
        This->ff_cso = cso;
        return D3D_OK;
    }
    device = This->base.device;

    info.type = PIPE_SHADER_FRAGMENT;
    info.byte_code = pFunction;
    info.const_i_base = NINE_CONST_I_BASE(device->max_ps_const_f) / 16;
    info.const_b_base = NINE_CONST_B_BASE(device->max_ps_const_f) / 16;
    info.sampler_mask_shadow = 0x0;
    info.sampler_ps1xtypes = 0x0;
    info.fog_enable = 0;
    info.projected = 0;
    info.process_vertices = false;

    pipe = nine_context_get_pipe_acquire(device);
    hr = nine_translate_shader(device, &info, pipe);
    nine_context_get_pipe_release(device);
    if (FAILED(hr))
        return hr;
    This->byte_code.version = info.version;

    This->byte_code.tokens = mem_dup(pFunction, info.byte_size);
    if (!This->byte_code.tokens)
        return E_OUTOFMEMORY;
    This->byte_code.size = info.byte_size;

    This->variant.cso = info.cso;
    This->last_cso = info.cso;
    This->last_key = 0;

    This->sampler_mask = info.sampler_mask;
    This->rt_mask = info.rt_mask;
    This->const_used_size = info.const_used_size;
    This->bumpenvmat_needed = info.bumpenvmat_needed;
    /* no constant relative addressing for ps */
    assert(info.lconstf.data == NULL);
    assert(info.lconstf.ranges == NULL);

    return D3D_OK;
}

void
NinePixelShader9_dtor( struct NinePixelShader9 *This )
{
    DBG("This=%p\n", This);

    if (This->base.device) {
        struct pipe_context *pipe = nine_context_get_pipe_multithread(This->base.device);
        struct nine_shader_variant *var = &This->variant;

        do {
            if (var->cso) {
                if (This->base.device->context.cso_shader.ps == var->cso)
                    pipe->bind_fs_state(pipe, NULL);
                pipe->delete_fs_state(pipe, var->cso);
            }
            var = var->next;
        } while (var);

        if (This->ff_cso) {
            if (This->ff_cso == This->base.device->context.cso_shader.ps)
                pipe->bind_fs_state(pipe, NULL);
            pipe->delete_fs_state(pipe, This->ff_cso);
        }
    }
    nine_shader_variants_free(&This->variant);

    FREE((void *)This->byte_code.tokens); /* const_cast */

    NineUnknown_dtor(&This->base);
}

HRESULT NINE_WINAPI
NinePixelShader9_GetFunction( struct NinePixelShader9 *This,
                              void *pData,
                              UINT *pSizeOfData )
{
    DBG("This=%p pData=%p pSizeOfData=%p\n", This, pData, pSizeOfData);

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
NinePixelShader9_GetVariant( struct NinePixelShader9 *This )
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

        info.type = PIPE_SHADER_FRAGMENT;
        info.const_i_base = NINE_CONST_I_BASE(device->max_ps_const_f) / 16;
        info.const_b_base = NINE_CONST_B_BASE(device->max_ps_const_f) / 16;
        info.byte_code = This->byte_code.tokens;
        info.sampler_mask_shadow = key & 0xffff;
        info.sampler_ps1xtypes = key;
        info.fog_enable = device->context.rs[D3DRS_FOGENABLE];
        info.fog_mode = device->context.rs[D3DRS_FOGTABLEMODE];
        info.force_color_in_centroid = key >> 34 & 1;
        info.projected = (key >> 48) & 0xffff;
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

IDirect3DPixelShader9Vtbl NinePixelShader9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineUnknown_GetDevice,
    (void *)NinePixelShader9_GetFunction
};

static const GUID *NinePixelShader9_IIDs[] = {
    &IID_IDirect3DPixelShader9,
    &IID_IUnknown,
    NULL
};

HRESULT
NinePixelShader9_new( struct NineDevice9 *pDevice,
                      struct NinePixelShader9 **ppOut,
                      const DWORD *pFunction, void *cso )
{
    NINE_DEVICE_CHILD_NEW(PixelShader9, ppOut, pDevice, pFunction, cso);
}
