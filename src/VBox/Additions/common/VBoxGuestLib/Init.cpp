/* $Id$ */
/** @file
 * VBoxGuestLibR0 - Library initialization.
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
#define VBGL_DECL_DATA
#include "VBGLInternal.h"

#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The global VBGL instance data.  */
VBGLDATA g_vbgldata;


/**
 * Used by vbglQueryDriverInfo and VbglInit to try get the host feature mask and
 * version information (g_vbgldata::hostVersion).
 *
 * This was first implemented by the host in 3.1 and we quietly ignore failures
 * for that reason.
 */
static void vbglR0QueryHostVersion(void)
{
    VMMDevReqHostVersion *pReq;
    int rc = VbglGRAlloc((VMMDevRequestHeader **) &pReq, sizeof (*pReq), VMMDevReq_GetHostVersion);
    if (RT_SUCCESS(rc))
    {
        rc = VbglGRPerform(&pReq->header);
        if (RT_SUCCESS(rc))
        {
            g_vbgldata.hostVersion = *pReq;
            Log(("vbglR0QueryHostVersion: %u.%u.%ur%u %#x\n",
                 pReq->major, pReq->minor, pReq->build, pReq->revision, pReq->features));
        }

        VbglGRFree(&pReq->header);
    }
}


#ifndef VBGL_VBOXGUEST
/**
 * The guest library uses lazy initialization for VMMDev port and memory,
 * because these values are provided by the VBoxGuest driver and it might
 * be loaded later than other drivers.
 *
 * The VbglEnter checks the current library status, tries to retrieve these
 * values and fails if they are unavailable.
 */
static void vbglQueryDriverInfo(void)
{
    int rc = RTSemMutexRequest(g_vbgldata.hMtxIdcSetup, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        if (g_vbgldata.status == VbglStatusReady)
        { /* likely */ }
        else
        {
            rc = VbglR0IdcOpen(&g_vbgldata.IdcHandle,
                               VBGL_IOC_VERSION /*uReqVersion*/,
                               VBGL_IOC_VERSION & UINT32_C(0xffff0000) /*uMinVersion*/,
                               NULL /*puSessionVersion*/, NULL /*puDriverVersion*/, NULL /*puDriverRevision*/);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Try query the port info.
                 */
                VBGLIOCGETVMMDEVIOINFO PortInfo;
                RT_ZERO(PortInfo);
                VBGLREQHDR_INIT(&PortInfo.Hdr, GET_VMMDEV_IO_INFO);
                rc = VbglR0IdcCall(&g_vbgldata.IdcHandle, VBGL_IOCTL_GET_VMMDEV_IO_INFO, &PortInfo.Hdr, sizeof(PortInfo));
                if (RT_SUCCESS(rc))
                {
                    dprintf(("Port I/O = 0x%04x, MMIO = %p\n", PortInfo.u.Out.IoPort, PortInfo.u.Out.pvVmmDevMapping));

                    g_vbgldata.portVMMDev    = PortInfo.u.Out.IoPort;
                    g_vbgldata.pVMMDevMemory = (VMMDevMemory *)PortInfo.u.Out.pvVmmDevMapping;
                    g_vbgldata.status        = VbglStatusReady;

                    vbglR0QueryHostVersion();
                }
            }

            dprintf(("vbglQueryDriverInfo rc = %Rrc\n", rc));
        }

        RTSemMutexRelease(g_vbgldata.hMtxIdcSetup);
    }
}
#endif /* !VBGL_VBOXGUEST */

/**
 * Checks if VBGL has been initialized.
 *
 * The client library, this will lazily complete the initialization.
 *
 * @return VINF_SUCCESS or VERR_VBGL_NOT_INITIALIZED.
 */
int vbglR0Enter(void)
{
    if (g_vbgldata.status == VbglStatusReady)
        return VINF_SUCCESS;

#ifndef VBGL_VBOXGUEST
    if (g_vbgldata.status == VbglStatusInitializing)
    {
        vbglQueryDriverInfo();
        if (g_vbgldata.status == VbglStatusReady)
            return VINF_SUCCESS;
    }
#endif
    return VERR_VBGL_NOT_INITIALIZED;
}


static int vbglInitCommon(void)
{
    int rc;

    RT_ZERO(g_vbgldata);
    g_vbgldata.status = VbglStatusInitializing;

    rc = VbglPhysHeapInit();
    if (RT_SUCCESS(rc))
    {
        dprintf(("vbglInitCommon: returns rc = %d\n", rc));
        return rc;
    }

    LogRel(("vbglInitCommon: VbglPhysHeapInit failed: rc=%Rrc\n", rc));
    g_vbgldata.status = VbglStatusNotInitialized;
    return rc;
}


static void vbglTerminateCommon(void)
{
    VbglPhysHeapTerminate();
    g_vbgldata.status = VbglStatusNotInitialized;
}

#ifdef VBGL_VBOXGUEST

DECLVBGL(int) VbglInitPrimary(RTIOPORT portVMMDev, VMMDevMemory *pVMMDevMemory)
{
    int rc;

# ifdef RT_OS_WINDOWS /** @todo r=bird: this doesn't make sense. Is there something special going on on windows? */
    dprintf(("vbglInit: starts g_vbgldata.status %d\n", g_vbgldata.status));

    if (   g_vbgldata.status == VbglStatusInitializing
        || g_vbgldata.status == VbglStatusReady)
    {
        /* Initialization is already in process. */
        return VINF_SUCCESS;
    }
# else
    dprintf(("vbglInit: starts\n"));
# endif

    rc = vbglInitCommon();
    if (RT_SUCCESS(rc))
    {
        g_vbgldata.portVMMDev    = portVMMDev;
        g_vbgldata.pVMMDevMemory = pVMMDevMemory;
        g_vbgldata.status        = VbglStatusReady;

        vbglR0QueryHostVersion();
        return VINF_SUCCESS;
    }

    g_vbgldata.status = VbglStatusNotInitialized;
    return rc;
}

DECLVBGL(void) VbglR0TerminatePrimary(void)
{
    vbglTerminateCommon();
}


#else /* !VBGL_VBOXGUEST */

DECLVBGL(int) VbglR0InitClient(void)
{
    int rc;

    /** @todo r=bird: explain why we need to be doing this, please... */
    if (   g_vbgldata.status == VbglStatusInitializing
        || g_vbgldata.status == VbglStatusReady)
    {
        /* Initialization is already in process. */
        return VINF_SUCCESS;
    }

    rc = vbglInitCommon();
    if (RT_SUCCESS(rc))
    {
        rc = RTSemMutexCreate(&g_vbgldata.hMtxIdcSetup);
        if (RT_SUCCESS(rc))
        {
            /* Try to obtain VMMDev port via IOCTL to VBoxGuest main driver. */
            vbglQueryDriverInfo();

# ifdef VBOX_WITH_HGCM
            rc = VbglR0HGCMInit();
# endif
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;

            RTSemMutexDestroy(g_vbgldata.hMtxIdcSetup);
            g_vbgldata.hMtxIdcSetup = NIL_RTSEMMUTEX;
        }
        vbglTerminateCommon();
    }

    return rc;
}

DECLVBGL(void) VbglR0TerminateClient(void)
{
# ifdef VBOX_WITH_HGCM
    VbglR0HGCMTerminate();
# endif

    /* driver open could fail, which does not prevent VbglInit from succeeding,
     * close the driver only if it is opened */
    VbglR0IdcClose(&g_vbgldata.IdcHandle);
    RTSemMutexDestroy(g_vbgldata.hMtxIdcSetup);
    g_vbgldata.hMtxIdcSetup = NIL_RTSEMMUTEX;

    /* note: do vbglTerminateCommon as a last step since it zeroez up the g_vbgldata
     * conceptually, doing vbglTerminateCommon last is correct
     * since this is the reverse order to how init is done */
    vbglTerminateCommon();
}


int VBOXCALL vbglR0QueryIdcHandle(PVBGLIDCHANDLE *ppIdcHandle)
{
    if (g_vbgldata.status == VbglStatusReady)
    { /* likely */ }
    else
    {
        vbglQueryDriverInfo();
        if (g_vbgldata.status != VbglStatusReady)
        {
            *ppIdcHandle = NULL;
            return VERR_TRY_AGAIN;
        }
    }

    *ppIdcHandle = &g_vbgldata.IdcHandle;
    return VINF_SUCCESS;
}

#endif /* !VBGL_VBOXGUEST */
