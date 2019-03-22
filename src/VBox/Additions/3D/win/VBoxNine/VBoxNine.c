/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Direct3D9 state tracker.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/win/windows.h>
#include <iprt/win/d3d9.h>
//#include <d3dumddi.h>

#include <VBoxWddmUmHlp.h>

//#include <windef.h>
//#include <winbase.h>
//#include <winsvc.h>
//#include <winnetwk.h>
//#include <npapi.h>
//#include <devioctl.h>
//#include <stdio.h>

//#include <iprt/alloc.h>
//#include <iprt/initterm.h>
//#include <iprt/string.h>
//#include <iprt/log.h>
//#include <VBox/version.h>
//#include <VBox/VMMDev.h>
//#include <VBox/Log.h>

#include "adapter9.h"
#include "surface9.h"
#include "pipe/p_screen.h"

#include <iprt/types.h>
// #include <iprt/asm.h>

// #include "VBoxNine.h"

struct d3dadapter9_context_wddm
{
    struct d3dadapter9_context base;
    void *reserved;
};

static void
wddm_destroy(struct d3dadapter9_context *ctx)
{
    /* struct d3dadapter9_context_wddm *ctx_wddm = (struct d3dadapter9_context_wddm *)ctx; */

/// @todo screen (hal) is deleted by the upper level. Do not delete here.
//    if (ctx->ref)
//        ctx->ref->destroy(ctx->ref);
//    /* because ref is a wrapper around hal, freeing ref frees hal too. */
//    else if (ctx->hal)
//        ctx->hal->destroy(ctx->hal);

    FREE(ctx);
}

static HRESULT
d3dadapter9_context_wddm_create(struct d3dadapter9_context_wddm **ppCtx, struct pipe_screen *s)
{
    struct d3dadapter9_context_wddm *ctx = CALLOC_STRUCT(d3dadapter9_context_wddm);

    if (!ctx) { return E_OUTOFMEMORY; }

    ctx->base.destroy = wddm_destroy;

    ctx->base.linear_framebuffer = 1;

    ctx->base.hal = s;

    ctx->base.throttling = FALSE;
    ctx->base.throttling_value = 0;

    ctx->base.vblank_mode = 1;

    ctx->base.thread_submit = 0;

    /** @todo Need software device here. Currently assigned to hw device to prevent NineDevice9_ctor crash. */
    ctx->base.ref = ctx->base.hal;

    /* read out PCI info */
    /// @todo read_descriptor(&ctx->base, fd);

    *ppCtx = ctx;
    return D3D_OK;
}

HRESULT WINAPI
GaNineD3DAdapter9Create(struct pipe_screen *s, ID3DAdapter9 **ppOut)
{
    HRESULT hr;
    struct d3dadapter9_context_wddm *pCtx = NULL;
    hr = d3dadapter9_context_wddm_create(&pCtx, s);
    if (SUCCEEDED(hr))
    {
        hr = NineAdapter9_new(&pCtx->base, (struct NineAdapter9 **)ppOut);
        if (FAILED(hr))
        {
            /// @todo NineAdapter9_new calls this as ctx->base.destroy,
            //       and will not call if memory allocation fails.
            // wddm_destroy(&pCtx->base);
        }
    }
    return hr;
}

struct pipe_resource * WINAPI
GaNinePipeResourceFromSurface(IUnknown *pSurface)
{
    /// @todo QueryInterface?
    struct NineResource9 *pResource = (struct NineResource9 *)pSurface;
    return pResource->resource;
}

extern struct pipe_context *
NineDevice9_GetPipe( struct NineDevice9 *This );

struct pipe_context * WINAPI
GaNinePipeContextFromDevice(IDirect3DDevice9 *pDevice)
{
    /// @todo Verify that this is a NineDevice?
    struct pipe_context *pPipeContext = NineDevice9_GetPipe((struct NineDevice9 *)pDevice);
    return pPipeContext;
}

BOOL WINAPI DllMain(HINSTANCE hDLLInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    BOOL fReturn = TRUE;

    RT_NOREF2(hDLLInst, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            //RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
            D3DKMTLoad();
            break;

        case DLL_PROCESS_DETACH:
            /// @todo RTR3Term();
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return fReturn;
}
