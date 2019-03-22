/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - VMSVGA hardware driver.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBoxWddmUmHlp.h>

#include "svga_public.h"
#include "svga_screen.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "state_tracker/drm_driver.h"

#include "wddm_screen.h"

struct svga_winsys_screen *
svga_wddm_winsys_screen_create(const WDDMGalliumDriverEnv *pEnv);

struct pipe_screen * WINAPI
GaDrvScreenCreate(const WDDMGalliumDriverEnv *pEnv)
{
    struct svga_winsys_screen *sws = svga_wddm_winsys_screen_create(pEnv);
    if (sws)
        return svga_screen_create(sws);
    return NULL;
}

void WINAPI
GaDrvScreenDestroy(struct pipe_screen *s)
{
    if (s)
        s->destroy(s);
}

uint32_t WINAPI
GaDrvGetSurfaceId(struct pipe_screen *pScreen, struct pipe_resource *pResource)
{
    uint32_t u32Sid = 0;

    if (   pScreen
        && pResource)
    {
        /* Get the sid (surface id). */
        struct winsys_handle whandle;
        memset(&whandle, 0, sizeof(whandle));
        whandle.type = DRM_API_HANDLE_TYPE_SHARED;

        if (pScreen->resource_get_handle(pScreen, NULL, pResource, &whandle, PIPE_HANDLE_USAGE_READ))
        {
            u32Sid = (uint32_t)whandle.handle;
        }
    }

    return u32Sid;
}

const WDDMGalliumDriverEnv *WINAPI
GaDrvGetWDDMEnv(struct pipe_screen *pScreen)
{
    HANDLE hAdapter = NULL;

    if (pScreen)
    {
        struct svga_screen *pSvgaScreen = svga_screen(pScreen);
        struct vmw_winsys_screen_wddm *vws_wddm = (struct vmw_winsys_screen_wddm *)pSvgaScreen->sws;

        return vws_wddm->pEnv;
    }

    return hAdapter;
}

uint32_t WINAPI
GaDrvGetContextId(struct pipe_context *pPipeContext)
{
    uint32 u32Cid = ~0;

    if (pPipeContext)
    {
        struct svga_winsys_context *pSWC = svga_winsys_context(pPipeContext);
        u32Cid = pSWC->cid;
    }

    return u32Cid;
}

void WINAPI
GaDrvContextFlush(struct pipe_context *pPipeContext)
{
    if (pPipeContext)
        pPipeContext->flush(pPipeContext, NULL, PIPE_FLUSH_END_OF_FRAME);
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
