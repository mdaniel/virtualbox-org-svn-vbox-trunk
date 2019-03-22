/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
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

#include "VBoxMPGaWddm.h"
#include "../VBoxMPVidPn.h"
#include "VBoxMPGaExt.h"

#include "Svga.h"
#include "SvgaFifo.h"
#include "SvgaCmd.h"
#include "SvgaHw.h"

#include <iprt/time.h>

void GaFenceObjectsDestroy(VBOXWDDM_EXT_GA *pGaDevExt,
                           PVBOXWDDM_DEVICE pDevice)
{
    RTLISTANCHOR list;
    RTListInit(&list);

    gaFenceObjectsLock(pGaDevExt);

    GAFENCEOBJECT *pIter, *pNext;
    RTListForEachSafe(&pGaDevExt->fenceObjects.list, pIter, pNext, GAFENCEOBJECT, node)
    {
        if (   pDevice == NULL
            || pIter->pDevice == pDevice)
        {
            /* Remove from list, add to the local list. */
            RTListNodeRemove(&pIter->node);
            GaIdFree(pGaDevExt->fenceObjects.au32HandleBits, sizeof(pGaDevExt->fenceObjects.au32HandleBits),
                     VBOXWDDM_GA_MAX_FENCE_OBJECTS, pIter->u32FenceHandle);
            RTListAppend(&list, &pIter->node);
        }
    }

    gaFenceObjectsUnlock(pGaDevExt);

    /* Deallocate list. */
    RTListForEachSafe(&list, pIter, pNext, GAFENCEOBJECT, node)
    {
        GALOG(("Deallocate u32FenceHandle = %d for %p\n", pIter->u32FenceHandle, pDevice));
        RTListNodeRemove(&pIter->node);
        GaMemFree(pIter);
    }
}

static void gaFenceDelete(VBOXWDDM_EXT_GA *pGaDevExt, GAFENCEOBJECT *pFO)
{
    GALOG(("u32FenceHandle = %d, pFO %p\n", pFO->u32FenceHandle, pFO));

    gaFenceObjectsLock(pGaDevExt);

    RTListNodeRemove(&pFO->node);
    GaIdFree(pGaDevExt->fenceObjects.au32HandleBits, sizeof(pGaDevExt->fenceObjects.au32HandleBits),
             VBOXWDDM_GA_MAX_FENCE_OBJECTS, pFO->u32FenceHandle);

    gaFenceObjectsUnlock(pGaDevExt);

    /// @todo Pool of fence objects to avoid memory allocations.
    GaMemFree(pFO);
}

static void gaFenceDeleteLocked(VBOXWDDM_EXT_GA *pGaDevExt, GAFENCEOBJECT *pFO)
{
    GALOG(("u32FenceHandle = %d, pFO %p\n", pFO->u32FenceHandle, pFO));

    RTListNodeRemove(&pFO->node);
    GaIdFree(pGaDevExt->fenceObjects.au32HandleBits, sizeof(pGaDevExt->fenceObjects.au32HandleBits),
             VBOXWDDM_GA_MAX_FENCE_OBJECTS, pFO->u32FenceHandle);

    /// @todo Pool of fence objects to avoid memory allocations.
    GaMemFree(pFO);
}

DECLINLINE(void) gaFenceRef(GAFENCEOBJECT *pFO)
{
    ASMAtomicIncU32(&pFO->cRefs);
}

DECLINLINE(void) gaFenceUnref(VBOXWDDM_EXT_GA *pGaDevExt, GAFENCEOBJECT *pFO)
{
    uint32_t c = ASMAtomicDecU32(&pFO->cRefs);
    Assert(c < UINT32_MAX / 2);
    if (c == 0)
    {
        gaFenceDelete(pGaDevExt, pFO);
    }
}

void GaFenceUnrefLocked(VBOXWDDM_EXT_GA *pGaDevExt, GAFENCEOBJECT *pFO)
{
    uint32_t c = ASMAtomicDecU32(&pFO->cRefs);
    Assert(c < UINT32_MAX / 2);
    if (c == 0)
    {
        gaFenceDeleteLocked(pGaDevExt, pFO);
    }
}

GAFENCEOBJECT *GaFenceLookup(VBOXWDDM_EXT_GA *pGaDevExt,
                             uint32_t u32FenceHandle)
{
    /* Must be called under the fence object lock. */
    GAFENCEOBJECT *pIter;
    RTListForEach(&pGaDevExt->fenceObjects.list, pIter, GAFENCEOBJECT, node)
    {
        if (pIter->u32FenceHandle == u32FenceHandle)
        {
            gaFenceRef(pIter);
            return pIter;
        }
    }
    return NULL;
}

/*
 * Fence objects.
 */

NTSTATUS GaFenceCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                       PVBOXWDDM_DEVICE pDevice,
                       uint32_t *pu32FenceHandle)
{
    GAFENCEOBJECT *pFO = (GAFENCEOBJECT *)GaMemAllocZero(sizeof(GAFENCEOBJECT));
    if (!pFO)
        return STATUS_INSUFFICIENT_RESOURCES;

    /* pFO->cRefs             = 0; */
    pFO->u32FenceState      = GAFENCE_STATE_IDLE;
    /* pFO->fu32FenceFlags    = 0; */
    /* pFO->u32SubmissionFenceId    = 0; */
    pFO->u32SeqNo           = ASMAtomicIncU32(&pGaDevExt->fenceObjects.u32SeqNoSource);
    pFO->pDevice            = pDevice;
    /* RT_ZERO(pFO->event); */

    gaFenceObjectsLock(pGaDevExt);

    NTSTATUS Status = GaIdAlloc(pGaDevExt->fenceObjects.au32HandleBits, sizeof(pGaDevExt->fenceObjects.au32HandleBits),
                                VBOXWDDM_GA_MAX_FENCE_OBJECTS, &pFO->u32FenceHandle);
    if (NT_SUCCESS(Status))
    {
        RTListAppend(&pGaDevExt->fenceObjects.list, &pFO->node);
        gaFenceRef(pFO);

        gaFenceObjectsUnlock(pGaDevExt);

        *pu32FenceHandle = pFO->u32FenceHandle;

        GALOG(("u32FenceHandle = %d\n", pFO->u32FenceHandle));
        return STATUS_SUCCESS;
    }

    /* Failure */
    gaFenceObjectsUnlock(pGaDevExt);
    GaMemFree(pFO);
    return Status;
}

NTSTATUS GaFenceQuery(PVBOXWDDM_EXT_GA pGaDevExt,
                      uint32_t u32FenceHandle,
                      uint32_t *pu32SubmittedSeqNo,
                      uint32_t *pu32ProcessedSeqNo,
                      uint32_t *pu32FenceStatus)
{
    gaFenceObjectsLock(pGaDevExt);

    GAFENCEOBJECT *pFO = GaFenceLookup(pGaDevExt, u32FenceHandle);

    gaFenceObjectsUnlock(pGaDevExt);

    GALOG(("u32FenceHandle = %d, pFO %p\n", u32FenceHandle, pFO));
    if (pFO)
    {
        *pu32SubmittedSeqNo = pFO->u32SeqNo;
        switch (pFO->u32FenceState)
        {
            default:
                AssertFailed();
                RT_FALL_THRU();
            case GAFENCE_STATE_IDLE:      *pu32FenceStatus = GA_FENCE_STATUS_IDLE;      break;
            case GAFENCE_STATE_SUBMITTED: *pu32FenceStatus = GA_FENCE_STATUS_SUBMITTED; break;
            case GAFENCE_STATE_SIGNALED:  *pu32FenceStatus = GA_FENCE_STATUS_SIGNALED;  break;
        }

        gaFenceUnref(pGaDevExt, pFO);
    }
    else
    {
        *pu32SubmittedSeqNo = 0;
        *pu32FenceStatus = GA_FENCE_STATUS_NULL;
    }
    *pu32ProcessedSeqNo = ASMAtomicReadU32(&pGaDevExt->u32LastCompletedSeqNo);

    return STATUS_SUCCESS;
}

NTSTATUS GaFenceWait(PVBOXWDDM_EXT_GA pGaDevExt,
                     uint32_t u32FenceHandle,
                     uint32_t u32TimeoutUS)
{
    gaFenceObjectsLock(pGaDevExt);

    GAFENCEOBJECT *pFO = GaFenceLookup(pGaDevExt, u32FenceHandle);
    AssertReturnStmt(pFO, gaFenceObjectsUnlock(pGaDevExt), STATUS_INVALID_PARAMETER);

    if (pFO->u32FenceState == GAFENCE_STATE_SIGNALED)
    {
        /* Already signaled. */
        gaFenceObjectsUnlock(pGaDevExt);
        gaFenceUnref(pGaDevExt, pFO);
        return STATUS_SUCCESS;
    }

    /* Wait */
    if (!RT_BOOL(pFO->fu32FenceFlags & GAFENCE_F_WAITED))
    {
        KeInitializeEvent(&pFO->event, NotificationEvent, FALSE);
        pFO->fu32FenceFlags |= GAFENCE_F_WAITED;
    }

    gaFenceObjectsUnlock(pGaDevExt);

    GALOG(("u32FenceHandle = %d, pFO %p\n", u32FenceHandle, pFO));

    LARGE_INTEGER Timeout;
    Timeout.QuadPart = u32TimeoutUS;
    Timeout.QuadPart *= -10LL; /* Microseconds to relative 100-nanoseconds units. */
    NTSTATUS Status = KeWaitForSingleObject(&pFO->event, UserRequest, KernelMode, TRUE, &Timeout);

    gaFenceUnref(pGaDevExt, pFO);

    return Status;
}

NTSTATUS GaFenceUnref(PVBOXWDDM_EXT_GA pGaDevExt,
                      uint32_t u32FenceHandle)
{
    gaFenceObjectsLock(pGaDevExt);

    GAFENCEOBJECT *pFO = GaFenceLookup(pGaDevExt, u32FenceHandle);
    AssertReturnStmt(pFO, gaFenceObjectsUnlock(pGaDevExt), STATUS_INVALID_PARAMETER);

    if (RT_BOOL(pFO->fu32FenceFlags & GAFENCE_F_WAITED))
    {
        KeSetEvent(&pFO->event, 0, FALSE);
        pFO->fu32FenceFlags &= ~GAFENCE_F_WAITED;
    }

    gaFenceObjectsUnlock(pGaDevExt);

    GALOG(("u32FenceHandle = %d, pFO %p\n", u32FenceHandle, pFO));

    /* Undo GaFenceLookup ref. */
    gaFenceUnref(pGaDevExt, pFO);

    /* Undo the GaFenceCreate ref. */
    gaFenceUnref(pGaDevExt, pFO);

    return STATUS_SUCCESS;
}
