/* $Id$ */
/** @file
 * VBoxGuestLib - Host-Guest Communication Manager, ring-0 client drivers.
 *
 * These public functions can be only used by other drivers. They all
 * do an IOCTL to VBoxGuest via IDC.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
/* Entire file is ifdef'ed with !VBGL_VBOXGUEST */
#ifndef VBGL_VBOXGUEST

#include "VBGLInternal.h"

#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#define VBGL_HGCM_ASSERT_MSG AssertReleaseMsg

/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Fast heap for HGCM handles data.
 * @{
 */
static RTSEMFASTMUTEX            g_hMtxHGCMHandleData;
static struct VBGLHGCMHANDLEDATA g_aHGCMHandleData[64];
/** @} */


/**
 * Initializes the HGCM VBGL bits.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglR0HGCMInit(void)
{
    AssertReturn(g_hMtxHGCMHandleData == NIL_RTSEMFASTMUTEX, VINF_ALREADY_INITIALIZED);
    return RTSemFastMutexCreate(&g_hMtxHGCMHandleData);
}

/**
 * Initializes the HGCM VBGL bits.
 *
 * @return VBox status code.
 */
DECLVBGL(int) VbglR0HGCMTerminate(void)
{
    RTSemFastMutexDestroy(g_hMtxHGCMHandleData);
    g_hMtxHGCMHandleData = NIL_RTSEMFASTMUTEX;

    return VINF_SUCCESS;
}

DECLINLINE(int) vbglR0HandleHeapEnter(void)
{
    int rc = RTSemFastMutexRequest(g_hMtxHGCMHandleData);

    VBGL_HGCM_ASSERT_MSG(RT_SUCCESS(rc), ("Failed to request handle heap mutex, rc = %Rrc\n", rc));

    return rc;
}

DECLINLINE(void) vbglR0HandleHeapLeave(void)
{
    RTSemFastMutexRelease(g_hMtxHGCMHandleData);
}

struct VBGLHGCMHANDLEDATA *vbglR0HGCMHandleAlloc(void)
{
    struct VBGLHGCMHANDLEDATA *p = NULL;
    int rc = vbglR0HandleHeapEnter();
    if (RT_SUCCESS(rc))
    {
        uint32_t i;

        /* Simple linear search in array. This will be called not so often, only connect/disconnect. */
        /** @todo bitmap for faster search and other obvious optimizations. */
        for (i = 0; i < RT_ELEMENTS(g_aHGCMHandleData); i++)
        {
            if (!g_aHGCMHandleData[i].fAllocated)
            {
                p = &g_aHGCMHandleData[i];
                p->fAllocated = 1;
                break;
            }
        }

        vbglR0HandleHeapLeave();

        VBGL_HGCM_ASSERT_MSG(p != NULL, ("Not enough HGCM handles.\n"));
    }
    return p;
}

void vbglR0HGCMHandleFree(struct VBGLHGCMHANDLEDATA *pHandle)
{
    if (pHandle)
    {
        int rc = vbglR0HandleHeapEnter();
        if (RT_SUCCESS(rc))
        {
            VBGL_HGCM_ASSERT_MSG(pHandle->fAllocated, ("Freeing not allocated handle.\n"));

            RT_ZERO(*pHandle);
            vbglR0HandleHeapLeave();
        }
    }
}

DECLVBGL(int) VbglR0HGCMConnect(VBGLHGCMHANDLE *pHandle, const char *pszServiceName, HGCMCLIENTID *pidClient)
{
    int rc;
    if (pHandle && pszServiceName && pidClient)
    {
        struct VBGLHGCMHANDLEDATA *pHandleData = vbglR0HGCMHandleAlloc();
        if (pHandleData)
        {
            rc = vbglDriverOpen(&pHandleData->driver);
            if (RT_SUCCESS(rc))
            {
                VBoxGuestHGCMConnectInfo Info;
                RT_ZERO(Info);
                Info.result       = VINF_SUCCESS;
                Info.u32ClientID  = 0;
                Info.Loc.type     = VMMDevHGCMLoc_LocalHost_Existing;
                rc = RTStrCopy(Info.Loc.u.host.achName, sizeof(Info.Loc.u.host.achName), pszServiceName);
                if (RT_SUCCESS(rc))
                {
                    rc = vbglDriverIOCtl(&pHandleData->driver, VBOXGUEST_IOCTL_HGCM_CONNECT, &Info, sizeof(Info));
                    if (RT_SUCCESS(rc))
                        rc = Info.result;
                    if (RT_SUCCESS(rc))
                    {
                        *pidClient = Info.u32ClientID;
                        *pHandle   = pHandleData;
                        return rc;
                    }
                }

                vbglDriverClose(&pHandleData->driver);
            }

            vbglR0HGCMHandleFree(pHandleData);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
}

DECLVBGL(int) VbglR0HGCMDisconnect(VBGLHGCMHANDLE handle, HGCMCLIENTID idClient)
{
    int rc;
    VBoxGuestHGCMDisconnectInfo Info;

    RT_ZERO(Info);
    Info.result      = VINF_SUCCESS;
    Info.u32ClientID = idClient;
    rc = vbglDriverIOCtl(&handle->driver, VBOXGUEST_IOCTL_HGCM_DISCONNECT, &Info, sizeof(Info));

    vbglDriverClose(&handle->driver);

    vbglR0HGCMHandleFree(handle);

    return rc;
}

DECLVBGL(int) VbglR0HGCMCall(VBGLHGCMHANDLE handle, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    VBGL_HGCM_ASSERT_MSG(cbData >= sizeof(VBoxGuestHGCMCallInfo) + pData->cParms * sizeof(HGCMFunctionParameter),
                         ("cbData = %d, cParms = %d (calculated size %d)\n", cbData, pData->cParms,
                          sizeof(VBoxGuestHGCMCallInfo) + pData->cParms * sizeof(VBoxGuestHGCMCallInfo)));

    return vbglDriverIOCtl(&handle->driver, VBOXGUEST_IOCTL_HGCM_CALL(cbData), pData, cbData);
}

DECLVBGL(int) VbglR0HGCMCallUserData(VBGLHGCMHANDLE handle, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    VBGL_HGCM_ASSERT_MSG(cbData >= sizeof(VBoxGuestHGCMCallInfo) + pData->cParms * sizeof(HGCMFunctionParameter),
                         ("cbData = %d, cParms = %d (calculated size %d)\n", cbData, pData->cParms,
                          sizeof(VBoxGuestHGCMCallInfo) + pData->cParms * sizeof(VBoxGuestHGCMCallInfo)));

    return vbglDriverIOCtl(&handle->driver, VBOXGUEST_IOCTL_HGCM_CALL_USERDATA(cbData), pData, cbData);
}


DECLVBGL(int) VbglR0HGCMCallTimed(VBGLHGCMHANDLE handle, VBoxGuestHGCMCallInfoTimed *pData, uint32_t cbData)
{
    uint32_t cbExpected = sizeof(VBoxGuestHGCMCallInfoTimed)
                        + pData->info.cParms * sizeof(HGCMFunctionParameter);
    VBGL_HGCM_ASSERT_MSG(cbData >= cbExpected,
                         ("cbData = %d, cParms = %d (calculated size %d)\n", cbData, pData->info.cParms, cbExpected));
    NOREF(cbExpected);

    return vbglDriverIOCtl(&handle->driver, VBOXGUEST_IOCTL_HGCM_CALL_TIMED(cbData), pData, cbData);
}

#endif /* !VBGL_VBOXGUEST */

