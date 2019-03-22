/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - OpenGL driver loader.
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
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

#include <common/wddm/VBoxMPIf.h>

static const char *g_pszGalliumDll =
#ifdef VBOX_WOW64
    "VBoxGL-x86.dll"
#else
    "VBoxGL.dll"
#endif
;

static const char *g_pszChromiumDll =
#ifdef VBOX_WOW64
    "VBoxOGL-x86.dll"
#else
    "VBoxOGL.dll"
#endif
;

extern struct VBOXWDDMDLLPROC aIcdProcs[];

HMODULE volatile g_hmodICD = NULL;

static NTSTATUS
vboxDdiQueryAdapterInfo(D3DKMT_HANDLE hAdapter,
                        VBOXWDDM_QAI *pAdapterInfo,
                        uint32_t cbAdapterInfo)
{
    NTSTATUS Status;
    D3DKMTFUNCTIONS const *d3dkmt = D3DKMTFunctions();

    if (d3dkmt->pfnD3DKMTQueryAdapterInfo)
    {
        D3DKMT_QUERYADAPTERINFO   QAI;
        memset(&QAI, 0, sizeof(QAI));
        QAI.hAdapter              = hAdapter;
        QAI.Type                  = KMTQAITYPE_UMDRIVERPRIVATE;
        QAI.pPrivateDriverData    = pAdapterInfo;
        QAI.PrivateDriverDataSize = cbAdapterInfo;

        Status = d3dkmt->pfnD3DKMTQueryAdapterInfo(&QAI);
    }
    else
    {
        Status = STATUS_NOT_SUPPORTED;
    }

    return Status;
}

void VBoxLoadICD(void)
{
    NTSTATUS Status;
    D3DKMT_HANDLE hAdapter = 0;

    D3DKMTLoad();

    Status = vboxDispKmtOpenAdapter(&hAdapter);
    if (Status == STATUS_SUCCESS)
    {
        VBOXWDDM_QAI adapterInfo;
        Status = vboxDdiQueryAdapterInfo(hAdapter, &adapterInfo, sizeof(adapterInfo));
        if (Status == STATUS_SUCCESS)
        {
            const char *pszDll = NULL;
            switch (adapterInfo.enmHwType)
            {
                case VBOXVIDEO_HWTYPE_VBOX:   pszDll = g_pszChromiumDll; break;
                default:
                case VBOXVIDEO_HWTYPE_VMSVGA: pszDll = g_pszGalliumDll; break;
            }

            if (pszDll)
            {
                g_hmodICD = VBoxWddmLoadSystemDll(pszDll);
                if (g_hmodICD)
                {
                    VBoxWddmLoadAdresses(g_hmodICD, aIcdProcs);
                }
            }
        }

        vboxDispKmtCloseAdapter(hAdapter);
    }
}

/*
 * MSDN says:
 * "You should never perform the following tasks from within DllMain:
 *   Call LoadLibrary or LoadLibraryEx (either directly or indirectly)."
 *
 * However it turned out that loading the real ICD from DLL_PROCESS_ATTACH works,
 * and loading it in a lazy way fails for unknown reason on 64 bit Windows.
 *
 * So just call VBoxLoadICD from DLL_PROCESS_ATTACH.
 */
BOOL WINAPI DllMain(HINSTANCE hDLLInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    RT_NOREF(hDLLInst);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            VBoxLoadICD();
            break;

        case DLL_PROCESS_DETACH:
            if (lpvReserved == NULL)
            {
                /* "The DLL is being unloaded because of a call to FreeLibrary." */
                if (g_hmodICD)
                {
                    FreeLibrary(g_hmodICD);
                    g_hmodICD = NULL;
                }
            }
            else
            {
                /* "The DLL is being unloaded due to process termination." */
                /* Do not bother. */
            }
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return TRUE;
}
