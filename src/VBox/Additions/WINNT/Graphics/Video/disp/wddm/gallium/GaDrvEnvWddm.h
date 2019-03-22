/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface to the WDDM miniport driver.
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

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDrvEnvWddm_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDrvEnvWddm_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBoxGaDriver.h>

#include <iprt/win/d3d9.h>
#include <d3dumddi.h>

#include <iprt/avl.h>

typedef struct GaWddmCallbacks
{
    HANDLE hAdapter;
    HANDLE hDevice;
    D3DDDI_DEVICECALLBACKS DeviceCallbacks;
} GaWddmCallbacks;

class GaDrvEnvWddm
{
    public:
        GaDrvEnvWddm();
        ~GaDrvEnvWddm();

        HRESULT Init(HANDLE hAdapter,
                     HANDLE hDevice,
                     const D3DDDI_DEVICECALLBACKS *pDeviceCallbacks,
                     const VBOXGAHWINFO *pHWInfo);

        const WDDMGalliumDriverEnv *Env();

        HANDLE GaDrvEnvWddmContextHandle(uint32_t u32Cid);

    private:
        GaWddmCallbacks mWddmCallbacks;

        VBOXGAHWINFO mHWInfo;

        /* Map to convert context id (cid) to WDDM context information (GAWDDMCONTEXTINFO).
         * Key is the 32 bit context id.
         */
        AVLU32TREE mContextTree;

        WDDMGalliumDriverEnv mEnv;

        static DECLCALLBACK(uint32_t) gaEnvWddmContextCreate(void *pvEnv,
                                                             boolean extended,
                                                             boolean vgpu10);
        static DECLCALLBACK(void) gaEnvWddmContextDestroy(void *pvEnv,
                                                          uint32_t u32Cid);
        static DECLCALLBACK(int) gaEnvWddmSurfaceDefine(void *pvEnv,
                                                        GASURFCREATE *pCreateParms,
                                                        GASURFSIZE *paSizes,
                                                        uint32_t cSizes,
                                                        uint32_t *pu32Sid);
        static DECLCALLBACK(void) gaEnvWddmSurfaceDestroy(void *pvEnv,
                                                          uint32_t u32Sid);
        static DECLCALLBACK(int) gaEnvWddmRender(void *pvEnv,
                                                 uint32_t u32Cid,
                                                 void *pvCommands,
                                                 uint32_t cbCommands,
                                                 GAFENCEQUERY *pFenceQuery);
        static DECLCALLBACK(void) gaEnvWddmFenceUnref(void *pvEnv,
                                                      uint32_t u32FenceHandle);
        static DECLCALLBACK(int) gaEnvWddmFenceQuery(void *pvEnv,
                                                     uint32_t u32FenceHandle,
                                                     GAFENCEQUERY *pFenceQuery);
        static DECLCALLBACK(int) gaEnvWddmFenceWait(void *pvEnv,
                                                    uint32_t u32FenceHandle,
                                                    uint32_t u32TimeoutUS);
        static DECLCALLBACK(int) gaEnvWddmRegionCreate(void *pvEnv,
                                                       uint32_t u32RegionSize,
                                                       uint32_t *pu32GmrId,
                                                       void **ppvMap);
        static DECLCALLBACK(void) gaEnvWddmRegionDestroy(void *pvEnv,
                                                         uint32_t u32GmrId,
                                                         void *pvMap);
};

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_GaDrvEnvWddm_h */
