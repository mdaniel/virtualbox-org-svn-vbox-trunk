/* $Id$ */
/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMPWddm.h"
#include <VBoxVideoVBE.h>
#include <iprt/param.h>
#include <stdio.h>

/* simple handle -> value table API */
NTSTATUS vboxWddmHTableCreate(PVBOXWDDM_HTABLE pTbl, uint32_t cSize)
{
    memset(pTbl, 0, sizeof (*pTbl));
    pTbl->paData = (PVOID*)vboxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cSize);
    if (pTbl->paData)
    {
        pTbl->cSize = cSize;
        return STATUS_SUCCESS;
    }
    return STATUS_NO_MEMORY;
}

VOID vboxWddmHTableDestroy(PVBOXWDDM_HTABLE pTbl)
{
    if (!pTbl->paData)
        return;

    vboxWddmMemFree(pTbl->paData);
}

DECLINLINE(VBOXWDDM_HANDLE) vboxWddmHTableIndex2Handle(uint32_t iIndex)
{
    return iIndex+1;
}

DECLINLINE(uint32_t) vboxWddmHTableHandle2Index(VBOXWDDM_HANDLE hHandle)
{
    return hHandle-1;
}

NTSTATUS vboxWddmHTableRealloc(PVBOXWDDM_HTABLE pTbl, uint32_t cNewSize)
{
    Assert(cNewSize > pTbl->cSize);
    if (cNewSize > pTbl->cSize)
    {
        PVOID *pvNewData = (PVOID*)vboxWddmMemAllocZero(sizeof (pTbl->paData[0]) * cNewSize);
        if (!pvNewData)
        {
            WARN(("vboxWddmMemAllocZero failed for size (%d)", sizeof (pTbl->paData[0]) * cNewSize));
            return STATUS_NO_MEMORY;
        }
        memcpy(pvNewData, pTbl->paData, sizeof (pTbl->paData[0]) * pTbl->cSize);
        vboxWddmMemFree(pTbl->paData);
        pTbl->iNext2Search = pTbl->cSize;
        pTbl->cSize = cNewSize;
        pTbl->paData = pvNewData;
        return STATUS_SUCCESS;
    }
    if (cNewSize >= pTbl->cData)
    {
        AssertFailed();
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_INVALID_PARAMETER;

}
VBOXWDDM_HANDLE vboxWddmHTablePut(PVBOXWDDM_HTABLE pTbl, PVOID pvData)
{
    if (pTbl->cSize == pTbl->cData)
    {
        NTSTATUS Status = vboxWddmHTableRealloc(pTbl, pTbl->cSize + RT_MAX(10, pTbl->cSize/4));
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
            return VBOXWDDM_HANDLE_INVALID;
    }
    for (UINT i = pTbl->iNext2Search; ; i = (i + 1) % pTbl->cSize)
    {
        Assert(i < pTbl->cSize);
        if (!pTbl->paData[i])
        {
            pTbl->paData[i] = pvData;
            ++pTbl->cData;
            Assert(pTbl->cData <= pTbl->cSize);
            ++pTbl->iNext2Search;
            pTbl->iNext2Search %= pTbl->cSize;
            return vboxWddmHTableIndex2Handle(i);
        }
    }
    /* not reached */
}

PVOID vboxWddmHTableRemove(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vboxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
    {
        PVOID pvData = pTbl->paData[iIndex];
        pTbl->paData[iIndex] = NULL;
        --pTbl->cData;
        Assert(pTbl->cData <= pTbl->cSize);
        pTbl->iNext2Search = iIndex;
        return pvData;
    }
    return NULL;
}

PVOID vboxWddmHTableGet(PVBOXWDDM_HTABLE pTbl, VBOXWDDM_HANDLE hHandle)
{
    uint32_t iIndex = vboxWddmHTableHandle2Index(hHandle);
    Assert(iIndex < pTbl->cSize);
    if (iIndex < pTbl->cSize)
        return pTbl->paData[iIndex];
    return NULL;
}

VOID vboxWddmHTableIterInit(PVBOXWDDM_HTABLE pTbl, PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    pIter->pTbl = pTbl;
    pIter->iCur = ~0UL;
    pIter->cLeft = pTbl->cData;
}

BOOL vboxWddmHTableIterHasNext(PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    return pIter->cLeft;
}


PVOID vboxWddmHTableIterNext(PVBOXWDDM_HTABLE_ITERATOR pIter, VBOXWDDM_HANDLE *phHandle)
{
    if (vboxWddmHTableIterHasNext(pIter))
    {
        for (uint32_t i = pIter->iCur+1; i < pIter->pTbl->cSize ; ++i)
        {
            if (pIter->pTbl->paData[i])
            {
                pIter->iCur = i;
                --pIter->cLeft;
                VBOXWDDM_HANDLE hHandle = vboxWddmHTableIndex2Handle(i);
                Assert(hHandle);
                if (phHandle)
                    *phHandle = hHandle;
                return pIter->pTbl->paData[i];
            }
        }
    }

    Assert(!vboxWddmHTableIterHasNext(pIter));
    if (phHandle)
        *phHandle = VBOXWDDM_HANDLE_INVALID;
    return NULL;
}


PVOID vboxWddmHTableIterRemoveCur(PVBOXWDDM_HTABLE_ITERATOR pIter)
{
    VBOXWDDM_HANDLE hHandle = vboxWddmHTableIndex2Handle(pIter->iCur);
    Assert(hHandle);
    if (hHandle)
    {
        PVOID pRet = vboxWddmHTableRemove(pIter->pTbl, hHandle);
        Assert(pRet);
        return pRet;
    }
    return NULL;
}
#ifdef VBOX_WITH_CROGL
PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainCreate(UINT w, UINT h)
{
    PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmMemAllocZero(sizeof (VBOXWDDM_SWAPCHAIN));
    Assert(pSwapchain);
    if (pSwapchain)
    {
        InitializeListHead(&pSwapchain->AllocList);
        pSwapchain->enmState = VBOXWDDM_OBJSTATE_TYPE_INITIALIZED;
        pSwapchain->cRefs = 1;
        /* init to some invalid value so that the pos get submitted */
        pSwapchain->Pos.x = pSwapchain->Pos.y = VBOXWDDM_INVALID_COORD;
        pSwapchain->width = w;
        pSwapchain->height = h;
        VBoxVrListInit(&pSwapchain->VisibleRegions);
    }
    return pSwapchain;
}

DECLINLINE(BOOLEAN) vboxWddmSwapchainRetainLocked(PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    if (pSwapchain->enmState == VBOXWDDM_OBJSTATE_TYPE_INITIALIZED)
    {
        ASMAtomicIncU32(&pSwapchain->cRefs);
        return TRUE;
    }
    return FALSE;
}

BOOLEAN vboxWddmSwapchainRetain(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    KIRQL OldIrql;
    BOOLEAN bRc;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    bRc = vboxWddmSwapchainRetainLocked(pSwapchain);
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return bRc;
}

VOID vboxWddmSwapchainRelease(PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    const uint32_t cRefs = ASMAtomicDecU32(&pSwapchain->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
    {
        VBoxVrListClear(&pSwapchain->VisibleRegions);
        vboxWddmMemFree(pSwapchain);
    }
}

PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainRetainByAllocData(PVBOXMP_DEVEXT pDevExt, const struct VBOXWDDM_ALLOC_DATA *pAllocData)
{
    KIRQL OldIrql;
    PVBOXWDDM_SWAPCHAIN pSwapchain;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    pSwapchain = pAllocData->pSwapchain;
    if (pSwapchain && !vboxWddmSwapchainRetainLocked(pSwapchain))
        pSwapchain = NULL;
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return pSwapchain;
}

PVBOXWDDM_SWAPCHAIN vboxWddmSwapchainRetainByAlloc(PVBOXMP_DEVEXT pDevExt, const VBOXWDDM_ALLOCATION *pAlloc)
{
    return vboxWddmSwapchainRetainByAllocData(pDevExt, &pAlloc->AllocData);
}

VOID vboxWddmSwapchainAllocRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    Assert(pAlloc->AllocData.pSwapchain == pSwapchain);
    pAlloc->AllocData.pSwapchain = NULL;
    RemoveEntryList(&pAlloc->SwapchainEntry);
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    vboxWddmSwapchainRelease(pSwapchain);
}

BOOLEAN vboxWddmSwapchainAllocAdd(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, PVBOXWDDM_ALLOCATION pAlloc)
{
    KIRQL OldIrql;
    BOOLEAN bRc;
    Assert(!pAlloc->AllocData.pSwapchain);
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    bRc = vboxWddmSwapchainRetainLocked(pSwapchain);
    if (bRc)
    {
        if (pAlloc->AllocData.pSwapchain)
        {
            RemoveEntryList(&pAlloc->SwapchainEntry);
        }
        InsertTailList(&pSwapchain->AllocList, &pAlloc->SwapchainEntry);
        pAlloc->AllocData.pSwapchain = pSwapchain;
    }
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);
    return bRc;
}

#define VBOXSCENTRY_2_ALLOC(_pE) ((PVBOXWDDM_ALLOCATION)((uint8_t*)(_pE) - RT_OFFSETOF(VBOXWDDM_ALLOCATION, SwapchainEntry)))

static VOID vboxWddmSwapchainAllocRemoveAllInternal(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain, BOOLEAN bOnDestroy)
{
    KIRQL OldIrql;
    UINT cRemoved = 0;
    KeAcquireSpinLock(&pDevExt->SynchLock, &OldIrql);
    PLIST_ENTRY pEntry = pSwapchain->AllocList.Flink;
    do
    {
        if (pEntry != &pSwapchain->AllocList)
        {
            PVBOXWDDM_ALLOCATION pAlloc = VBOXSCENTRY_2_ALLOC(pEntry);
            pEntry = pEntry->Flink;
            Assert(pAlloc->AllocData.pSwapchain == pSwapchain);
            pAlloc->AllocData.pSwapchain = NULL;
            RemoveEntryList(&pAlloc->SwapchainEntry);
            ++cRemoved;
        }
        else
            break;
    } while (1);

    if (bOnDestroy)
        pSwapchain->enmState = VBOXWDDM_OBJSTATE_TYPE_TERMINATED;
    KeReleaseSpinLock(&pDevExt->SynchLock, OldIrql);

    for (UINT i = 0; i < cRemoved; ++i)
        vboxWddmSwapchainRelease(pSwapchain);
}

VOID vboxWddmSwapchainAllocRemoveAll(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, FALSE);
}

VOID vboxWddmSwapchainDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    vboxWddmSwapchainAllocRemoveAllInternal(pDevExt, pSwapchain, TRUE);

    vboxWddmSwapchainRelease(pSwapchain);
}

static BOOLEAN vboxWddmSwapchainCtxAddLocked(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    if (vboxWddmSwapchainRetain(pDevExt, pSwapchain))
    {
        Assert(!pSwapchain->hSwapchainKm);
        Assert(!pSwapchain->pContext);
        pSwapchain->pContext = pContext;
        pSwapchain->hSwapchainKm = vboxWddmHTablePut(&pContext->Swapchains, pSwapchain);
        InsertHeadList(&pDevExt->SwapchainList3D, &pSwapchain->DevExtListEntry);
        return TRUE;
    }
    return FALSE;
}

static VOID vboxWddmSwapchainCtxRemoveLocked(PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    Assert(pSwapchain->hSwapchainKm);
    Assert(pSwapchain->pContext);
    void *pvTst = vboxWddmHTableRemove(&pContext->Swapchains, pSwapchain->hSwapchainKm);
    Assert((PVBOXWDDM_SWAPCHAIN)pvTst == pSwapchain); NOREF(pvTst);
    RemoveEntryList(&pSwapchain->DevExtListEntry);
    pSwapchain->hSwapchainKm = NULL;
    VBoxVrListClear(&pSwapchain->VisibleRegions);
    vboxWddmSwapchainRelease(pSwapchain);
}

/* adds the given swapchain to the context's swapchain list
 * @return true on success */
BOOLEAN vboxWddmSwapchainCtxAdd(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    BOOLEAN bRc;
    VBOXWDDM_CTXLOCK_DATA
    VBOXWDDM_CTXLOCK_LOCK(pDevExt);
    bRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
    VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
    return bRc;
}

/* removes the given swapchain from the context's swapchain list
 * */
VOID vboxWddmSwapchainCtxRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext, PVBOXWDDM_SWAPCHAIN pSwapchain)
{
    VBOXWDDM_CTXLOCK_DATA
    VBOXWDDM_CTXLOCK_LOCK(pDevExt);
    vboxWddmSwapchainCtxRemoveLocked(pContext, pSwapchain);
    VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
}

/* destroys all swapchains for the given context
 * */
VOID vboxWddmSwapchainCtxDestroyAll(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    VBOXWDDM_HTABLE_ITERATOR Iter;
    VBOXWDDM_CTXLOCK_DATA
    do
    {
        VBOXWDDM_CTXLOCK_LOCK(pDevExt);
        vboxWddmHTableIterInit(&pContext->Swapchains, &Iter);
        PVBOXWDDM_SWAPCHAIN pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableIterNext(&Iter, NULL);
        if (!pSwapchain)
            break;

        /* yes, we can call remove locked even when using iterator */
        vboxWddmSwapchainCtxRemoveLocked(pContext, pSwapchain);

        VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
        /* we must not do vboxWddmSwapchainDestroy inside a context mutex */
        vboxWddmSwapchainDestroy(pDevExt, pSwapchain);
        /* start from the very beginning, we will quit the loop when no swapchains left */
    } while (1);

    /* no swapchains left, we exiteed the while loop via the "break", and we still owning the mutex */
    VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
}

/* process the swapchain info passed from user-mode display driver & synchronizes the driver state with it */
NTSTATUS vboxWddmSwapchainCtxEscape(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext,
                                    PVBOXDISPIFESCAPE_SWAPCHAININFO pSwapchainInfo, UINT cbSize)
{
    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[0]))
    {
        WARN(("invalid cbSize1 %d", cbSize));
        return STATUS_INVALID_PARAMETER;
    }

    if (cbSize < RT_OFFSETOF(VBOXDISPIFESCAPE_SWAPCHAININFO, SwapchainInfo.ahAllocs[pSwapchainInfo->SwapchainInfo.cAllocs]))
    {
        WARN(("invalid cbSize2 %d", cbSize));
        return STATUS_INVALID_PARAMETER;
    }

    if (!pSwapchainInfo->SwapchainInfo.winHostID)
    {
        WARN(("Zero winHostID specified!"));
        return STATUS_INVALID_PARAMETER;
    }

    if (!pContext)
    {
        WARN(("vboxWddmSwapchainCtxEscape: no context specified"));
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_SWAPCHAIN pSwapchain = NULL;
    PVBOXWDDM_ALLOCATION *apAlloc = NULL;
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    NTSTATUS Status = STATUS_SUCCESS;
    VBOXWDDM_CTXLOCK_DATA

    do {
        if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            /* ensure we do not overflow the 32bit buffer size value */
            if (VBOXWDDM_ARRAY_MAXELEMENTSU32(VBOXWDDM_ALLOCATION) < pSwapchainInfo->SwapchainInfo.cAllocs)
            {
                WARN(("number of allocations passed in too big (%d), max is (%d)",
                      pSwapchainInfo->SwapchainInfo.cAllocs, VBOXWDDM_ARRAY_MAXELEMENTSU32(VBOXWDDM_ALLOCATION)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            apAlloc = (PVBOXWDDM_ALLOCATION *)vboxWddmMemAlloc(  sizeof(PVBOXWDDM_ALLOCATION)
                                                               * pSwapchainInfo->SwapchainInfo.cAllocs);
            Assert(apAlloc);
            if (!apAlloc)
            {
                Status = STATUS_NO_MEMORY;
                break;
            }
            for (UINT i = 0; i < pSwapchainInfo->SwapchainInfo.cAllocs; ++i)
            {
                DXGKARGCB_GETHANDLEDATA GhData;
                GhData.hObject = pSwapchainInfo->SwapchainInfo.ahAllocs[i];
                GhData.Type = DXGK_HANDLE_ALLOCATION;
                GhData.Flags.Value = 0;
                PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pDevExt->u.primary.DxgkInterface.DxgkCbGetHandleData(&GhData);
                Assert(pAlloc);
                if (!pAlloc)
                {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }
                apAlloc[i] = pAlloc;
            }

            if (!NT_SUCCESS(Status))
                break;
        }

        if (pSwapchainInfo->SwapchainInfo.hSwapchainKm)
        {
            VBOXWDDM_CTXLOCK_LOCK(pDevExt);
            pSwapchain = (PVBOXWDDM_SWAPCHAIN)vboxWddmHTableGet(&pContext->Swapchains,
                                                                (VBOXWDDM_HANDLE)pSwapchainInfo->SwapchainInfo.hSwapchainKm);
            Assert(pSwapchain);
            if (!pSwapchain)
            {
                VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            Assert(pSwapchain->hSwapchainKm == pSwapchainInfo->SwapchainInfo.hSwapchainKm);
            Assert(pSwapchain->pContext == pContext);
            if (pSwapchain->pContext != pContext)
            {
                VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
        else if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            pSwapchain = vboxWddmSwapchainCreate(apAlloc[0]->AllocData.SurfDesc.width, apAlloc[0]->AllocData.SurfDesc.height);
            if (!pSwapchain)
            {
                Status = STATUS_NO_MEMORY;
                break;
            }

            VBOXWDDM_CTXLOCK_LOCK(pDevExt);
            BOOLEAN fRc = vboxWddmSwapchainCtxAddLocked(pDevExt, pContext, pSwapchain);
            Assert(fRc); NOREF(fRc);
        }
        else
        {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        /* do not zero up the view rect since it may still be valid */
//        memset(&pSwapchain->ViewRect, 0, sizeof (pSwapchain->ViewRect));
        /** @todo do we really need to zero this up here ? */
        VBoxVrListClear(&pSwapchain->VisibleRegions);

        vboxWddmSwapchainAllocRemoveAll(pDevExt, pSwapchain);

        if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            for (UINT i = 0; i < pSwapchainInfo->SwapchainInfo.cAllocs; ++i)
            {
                vboxWddmSwapchainAllocAdd(pDevExt, pSwapchain, apAlloc[i]);
            }
            pSwapchain->hSwapchainUm = pSwapchainInfo->SwapchainInfo.hSwapchainUm;
            if (pSwapchain->winHostID != pSwapchainInfo->SwapchainInfo.winHostID)
            {
                pSwapchain->fExposed = FALSE;
                pSwapchain->winHostID = pSwapchainInfo->SwapchainInfo.winHostID;
            }
        }
        else
        {
            vboxWddmSwapchainCtxRemoveLocked(pContext, pSwapchain);
        }

        VBOXWDDM_CTXLOCK_UNLOCK(pDevExt);

        if (pSwapchainInfo->SwapchainInfo.cAllocs)
        {
            Assert(pSwapchain->pContext);
            Assert(pSwapchain->hSwapchainKm);
            pSwapchainInfo->SwapchainInfo.hSwapchainKm = pSwapchain->hSwapchainKm;
        }
        else
        {
            vboxWddmSwapchainDestroy(pDevExt, pSwapchain);
            pSwapchainInfo->SwapchainInfo.hSwapchainKm = 0;
        }

        AssertNtStatusSuccess(Status);
    } while (0);

    /* cleanup */
    if (apAlloc)
        vboxWddmMemFree(apAlloc);

    return Status;
}

NTSTATUS vboxWddmSwapchainCtxInit(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    RT_NOREF(pDevExt);
    NTSTATUS Status = vboxWddmHTableCreate(&pContext->Swapchains, 4);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxWddmHTableCreate failes, Status (x%x)", Status));
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID vboxWddmSwapchainCtxTerm(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_CONTEXT pContext)
{
    vboxWddmSwapchainCtxDestroyAll(pDevExt, pContext);
    vboxWddmHTableDestroy(&pContext->Swapchains);
}
#endif
NTSTATUS vboxWddmRegQueryDrvKeyName(PVBOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    WCHAR fallBackBuf[2];
    PWCHAR pSuffix;
    bool bFallback = false;

    if (cbBuf > sizeof(VBOXWDDM_REG_DRVKEY_PREFIX))
    {
        memcpy(pBuf, VBOXWDDM_REG_DRVKEY_PREFIX, sizeof (VBOXWDDM_REG_DRVKEY_PREFIX));
        pSuffix = pBuf + (sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2)/2;
        cbBuf -= sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;
    }
    else
    {
        pSuffix = fallBackBuf;
        cbBuf = sizeof (fallBackBuf);
        bFallback = true;
    }

    NTSTATUS Status = IoGetDeviceProperty (pDevExt->pPDO,
                                  DevicePropertyDriverKeyName,
                                  cbBuf,
                                  pSuffix,
                                  &cbBuf);
    if (Status == STATUS_SUCCESS && bFallback)
        Status = STATUS_BUFFER_TOO_SMALL;
    if (Status == STATUS_BUFFER_TOO_SMALL)
        *pcbResult = cbBuf + sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;

    return Status;
}

NTSTATUS vboxWddmRegQueryDisplaySettingsKeyName(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PWCHAR pSuffix;
    const WCHAR* pKeyPrefix;
    UINT cbKeyPrefix;
    UNICODE_STRING* pVGuid = vboxWddmVGuidGet(pDevExt);
    Assert(pVGuid);
    if (!pVGuid)
        return STATUS_UNSUCCESSFUL;

    vboxWinVersion_t ver = VBoxQueryWinVersion();
    if (ver == WINVERSION_VISTA)
    {
        pKeyPrefix = VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA;
        cbKeyPrefix = sizeof (VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_VISTA);
    }
    else
    {
        Assert(ver > WINVERSION_VISTA);
        pKeyPrefix = VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7;
        cbKeyPrefix = sizeof (VBOXWDDM_REG_DISPLAYSETTINGSKEY_PREFIX_WIN7);
    }

    ULONG cbResult = cbKeyPrefix + pVGuid->Length + 2 + 8; // L"\\" + "XXXX"
    if (cbBuf >= cbResult)
    {
        wcscpy(pBuf, pKeyPrefix);
        pSuffix = pBuf + (cbKeyPrefix-2)/2;
        memcpy(pSuffix, pVGuid->Buffer, pVGuid->Length);
        pSuffix += pVGuid->Length/2;
        pSuffix[0] = L'\\';
        pSuffix += 1;
        swprintf(pSuffix, L"%04d", VidPnSourceId);
    }
    else
    {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    *pcbResult = cbResult;

    return Status;
}

NTSTATUS vboxWddmRegQueryVideoGuidString(PVBOXMP_DEVEXT pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    BOOLEAN fNewMethodSucceeded = FALSE;
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DEVICE, GENERIC_READ, &hKey);
    if (NT_SUCCESS(Status))
    {
        struct
        {
            KEY_VALUE_PARTIAL_INFORMATION Info;
            UCHAR Buf[1024]; /* should be enough */
        } KeyData;
        ULONG cbResult;
        UNICODE_STRING RtlStr;
        RtlInitUnicodeString(&RtlStr, L"VideoID");
        Status = ZwQueryValueKey(hKey,
                    &RtlStr,
                    KeyValuePartialInformation,
                    &KeyData.Info,
                    sizeof(KeyData),
                    &cbResult);
        if (NT_SUCCESS(Status))
        {
            if (KeyData.Info.Type == REG_SZ)
            {
                fNewMethodSucceeded = TRUE;
                *pcbResult = KeyData.Info.DataLength + 2;
                if (cbBuf >= KeyData.Info.DataLength)
                {
                    memcpy(pBuf, KeyData.Info.Data, KeyData.Info.DataLength + 2);
                    Status = STATUS_SUCCESS;
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;
            }
        }
        else
        {
            WARN(("ZwQueryValueKey failed, Status 0x%x", Status));
        }

        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }
    else
    {
        WARN(("IoOpenDeviceRegistryKey failed Status 0x%x", Status));
    }

    if (fNewMethodSucceeded)
        return Status;
    else
        WARN(("failed to acquire the VideoID, falling back to the old impl"));

    Status = vboxWddmRegOpenKey(&hKey, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY, GENERIC_READ);
    //AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        struct
        {
            KEY_BASIC_INFORMATION Name;
            WCHAR Buf[256];
        } Buf;
        WCHAR KeyBuf[sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY)/2 + 256 + 64];
        wcscpy(KeyBuf, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY);
        ULONG ResultLength;
        BOOL bFound = FALSE;
        for (ULONG i = 0; !bFound; ++i)
        {
            RtlZeroMemory(&Buf, sizeof (Buf));
            Status = ZwEnumerateKey(hKey, i, KeyBasicInformation, &Buf, sizeof (Buf), &ResultLength);
            AssertNtStatusSuccess(Status);
            /* we should not encounter STATUS_NO_MORE_ENTRIES here since this would mean we did not find our entry */
            if (Status != STATUS_SUCCESS)
                break;

            HANDLE hSubKey;
            PWCHAR pSubBuf = KeyBuf + (sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY) - 2)/2;
            memcpy(pSubBuf, Buf.Name.Name, Buf.Name.NameLength);
            pSubBuf += Buf.Name.NameLength/2;
            memcpy(pSubBuf, VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY, sizeof (VBOXWDDM_REG_DISPLAYSETTINGSVIDEOKEY_SUBKEY));
            Status = vboxWddmRegOpenKey(&hSubKey, KeyBuf, GENERIC_READ);
            //AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                struct
                {
                    KEY_VALUE_PARTIAL_INFORMATION Info;
                    UCHAR Buf[sizeof (VBOX_WDDM_DRIVERNAME)]; /* should be enough */
                } KeyData;
                ULONG cbResult;
                UNICODE_STRING RtlStr;
                RtlInitUnicodeString(&RtlStr, L"Service");
                Status = ZwQueryValueKey(hSubKey,
                            &RtlStr,
                            KeyValuePartialInformation,
                            &KeyData.Info,
                            sizeof(KeyData),
                            &cbResult);
                Assert(Status == STATUS_SUCCESS || STATUS_BUFFER_TOO_SMALL || STATUS_BUFFER_OVERFLOW);
                if (Status == STATUS_SUCCESS)
                {
                    if (KeyData.Info.Type == REG_SZ)
                    {
                        if (KeyData.Info.DataLength == sizeof (VBOX_WDDM_DRIVERNAME))
                        {
                            if (!wcscmp(VBOX_WDDM_DRIVERNAME, (PWCHAR)KeyData.Info.Data))
                            {
                                bFound = TRUE;
                                *pcbResult = Buf.Name.NameLength + 2;
                                if (cbBuf >= Buf.Name.NameLength + 2)
                                {
                                    memcpy(pBuf, Buf.Name.Name, Buf.Name.NameLength + 2);
                                }
                                else
                                {
                                    Status = STATUS_BUFFER_TOO_SMALL;
                                }
                            }
                        }
                    }
                }

                NTSTATUS rcNt2 = ZwClose(hSubKey);
                AssertNtStatusSuccess(rcNt2);
            }
            else
                break;
        }
        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }

    return Status;
}

NTSTATUS vboxWddmRegOpenKeyEx(OUT PHANDLE phKey, IN HANDLE hRootKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    OBJECT_ATTRIBUTES ObjAttr;
    UNICODE_STRING RtlStr;

    RtlInitUnicodeString(&RtlStr, pName);
    InitializeObjectAttributes(&ObjAttr, &RtlStr, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, hRootKey, NULL);

    return ZwOpenKey(phKey, fAccess, &ObjAttr);
}

NTSTATUS vboxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    return vboxWddmRegOpenKeyEx(phKey, NULL, pName, fAccess);
}

NTSTATUS vboxWddmRegOpenDisplaySettingsKey(IN PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
                                           OUT PHANDLE phKey)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vboxWddmRegQueryDisplaySettingsKeyName(pDevExt, VidPnSourceId, cbBuf, Buf, &cbBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmRegOpenKey(phKey, Buf, GENERIC_READ);
        AssertNtStatusSuccess(Status);
        if(Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
    }

    /* fall-back to make the subsequent VBoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *phKey = NULL;

    return Status;
}

NTSTATUS vboxWddmRegDisplaySettingsQueryRelX(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELX, &dwVal);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vboxWddmRegDisplaySettingsQueryRelY(HANDLE hKey, int * pResult)
{
    DWORD dwVal;
    NTSTATUS Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_REG_DISPLAYSETTINGS_ATTACH_RELY, &dwVal);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        *pResult = (int)dwVal;
    }

    return Status;
}

NTSTATUS vboxWddmDisplaySettingsQueryPos(IN PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, POINT * pPos)
{
    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    HANDLE hKey;
    NTSTATUS Status = vboxWddmRegOpenDisplaySettingsKey(pDevExt, VidPnSourceId, &hKey);
    //AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        int x, y;
        Status = vboxWddmRegDisplaySettingsQueryRelX(hKey, &x);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxWddmRegDisplaySettingsQueryRelY(hKey, &y);
            AssertNtStatusSuccess(Status);
            if (Status == STATUS_SUCCESS)
            {
                pPos->x = x;
                pPos->y = y;
            }
        }
        NTSTATUS rcNt2 = ZwClose(hKey);
        AssertNtStatusSuccess(rcNt2);
    }

    return Status;
}

void vboxWddmDisplaySettingsCheckPos(IN PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    POINT Pos = {0};
    NTSTATUS Status = vboxWddmDisplaySettingsQueryPos(pDevExt, VidPnSourceId, &Pos);
    if (!NT_SUCCESS(Status))
    {
        Log(("vboxWddmDisplaySettingsQueryPos failed %#x", Status));
        return;
    }

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[VidPnSourceId];

    if (!memcmp(&pSource->VScreenPos, &Pos, sizeof (Pos)))
        return;

    pSource->VScreenPos = Pos;
    pSource->u8SyncState &= ~VBOXWDDM_HGSYNC_F_SYNCED_DIMENSIONS;

    vboxWddmGhDisplayCheckSetInfoFromSource(pDevExt, pSource);
}

NTSTATUS vboxWddmRegDrvFlagsSet(PVBOXMP_DEVEXT pDevExt, DWORD fVal)
{
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DRIVER, GENERIC_WRITE, &hKey);
    if (!NT_SUCCESS(Status))
    {
        WARN(("IoOpenDeviceRegistryKey failed, Status = 0x%x", Status));
        return Status;
    }

    Status = vboxWddmRegSetValueDword(hKey, VBOXWDDM_REG_DRV_FLAGS_NAME, fVal);
    if (!NT_SUCCESS(Status))
        WARN(("vboxWddmRegSetValueDword failed, Status = 0x%x", Status));

    NTSTATUS rcNt2 = ZwClose(hKey);
    AssertNtStatusSuccess(rcNt2);

    return Status;
}

DWORD vboxWddmRegDrvFlagsGet(PVBOXMP_DEVEXT pDevExt, DWORD fDefault)
{
    HANDLE hKey = NULL;
    NTSTATUS Status = IoOpenDeviceRegistryKey(pDevExt->pPDO, PLUGPLAY_REGKEY_DRIVER, GENERIC_READ, &hKey);
    if (!NT_SUCCESS(Status))
    {
        WARN(("IoOpenDeviceRegistryKey failed, Status = 0x%x", Status));
        return fDefault;
    }

    DWORD dwVal = 0;
    Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_REG_DRV_FLAGS_NAME, &dwVal);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxWddmRegQueryValueDword failed, Status = 0x%x", Status));
        dwVal = fDefault;
    }

    NTSTATUS rcNt2 = ZwClose(hKey);
    AssertNtStatusSuccess(rcNt2);

    return dwVal;
}

NTSTATUS vboxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword)
{
    struct
    {
        KEY_VALUE_PARTIAL_INFORMATION Info;
        UCHAR Buf[32]; /* should be enough */
    } Buf;
    ULONG cbBuf;
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    NTSTATUS Status = ZwQueryValueKey(hKey,
                &RtlStr,
                KeyValuePartialInformation,
                &Buf.Info,
                sizeof(Buf),
                &cbBuf);
    if (Status == STATUS_SUCCESS)
    {
        if (Buf.Info.Type == REG_DWORD)
        {
            Assert(Buf.Info.DataLength == 4);
            *pDword = *((PULONG)Buf.Info.Data);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS vboxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, IN DWORD val)
{
    UNICODE_STRING RtlStr;
    RtlInitUnicodeString(&RtlStr, pName);
    return ZwSetValueKey(hKey, &RtlStr,
            NULL, /* IN ULONG  TitleIndex  OPTIONAL, reserved */
            REG_DWORD,
            &val,
            sizeof(val));
}

UNICODE_STRING* vboxWddmVGuidGet(PVBOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
        return &pDevExt->VideoGuid;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);
    WCHAR VideoGuidBuf[512];
    ULONG cbVideoGuidBuf = sizeof (VideoGuidBuf);
    NTSTATUS Status = vboxWddmRegQueryVideoGuidString(pDevExt ,cbVideoGuidBuf, VideoGuidBuf, &cbVideoGuidBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        PWCHAR pBuf = (PWCHAR)vboxWddmMemAllocZero(cbVideoGuidBuf);
        Assert(pBuf);
        if (pBuf)
        {
            memcpy(pBuf, VideoGuidBuf, cbVideoGuidBuf);
            RtlInitUnicodeString(&pDevExt->VideoGuid, pBuf);
            return &pDevExt->VideoGuid;
        }
    }

    return NULL;
}

VOID vboxWddmVGuidFree(PVBOXMP_DEVEXT pDevExt)
{
    if (pDevExt->VideoGuid.Buffer)
    {
        vboxWddmMemFree(pDevExt->VideoGuid.Buffer);
        pDevExt->VideoGuid.Buffer = NULL;
    }
}

/* mm */

NTSTATUS vboxMmInit(PVBOXWDDM_MM pMm, UINT cPages)
{
    UINT cbBuffer = VBOXWDDM_ROUNDBOUND(cPages, 8) >> 3;
    cbBuffer = VBOXWDDM_ROUNDBOUND(cbBuffer, 4);
    PULONG pBuf = (PULONG)vboxWddmMemAllocZero(cbBuffer);
    if (!pBuf)
    {
        Assert(0);
        return STATUS_NO_MEMORY;
    }
    RtlInitializeBitMap(&pMm->BitMap, pBuf, cPages);
    pMm->cPages = cPages;
    pMm->cAllocs = 0;
    pMm->pBuffer = pBuf;
    return STATUS_SUCCESS;
}

ULONG vboxMmAlloc(PVBOXWDDM_MM pMm, UINT cPages)
{
    ULONG iPage = RtlFindClearBitsAndSet(&pMm->BitMap, cPages, 0);
    if (iPage == 0xFFFFFFFF)
    {
        Assert(0);
        return VBOXWDDM_MM_VOID;
    }

    ++pMm->cAllocs;
    return iPage;
}

VOID vboxMmFree(PVBOXWDDM_MM pMm, UINT iPage, UINT cPages)
{
    Assert(RtlAreBitsSet(&pMm->BitMap, iPage, cPages));
    RtlClearBits(&pMm->BitMap, iPage, cPages);
    --pMm->cAllocs;
    Assert(pMm->cAllocs < UINT32_MAX);
}

NTSTATUS vboxMmTerm(PVBOXWDDM_MM pMm)
{
    Assert(!pMm->cAllocs);
    vboxWddmMemFree(pMm->pBuffer);
    pMm->pBuffer = NULL;
    return STATUS_SUCCESS;
}



typedef struct VBOXVIDEOCM_ALLOC
{
    VBOXWDDM_HANDLE hGlobalHandle;
    uint32_t offData;
    uint32_t cbData;
} VBOXVIDEOCM_ALLOC, *PVBOXVIDEOCM_ALLOC;

typedef struct VBOXVIDEOCM_ALLOC_REF
{
    PVBOXVIDEOCM_ALLOC_CONTEXT pContext;
    VBOXWDDM_HANDLE hSessionHandle;
    PVBOXVIDEOCM_ALLOC pAlloc;
    PKEVENT pSynchEvent;
    VBOXUHGSMI_BUFFER_TYPE_FLAGS fUhgsmiType;
    volatile uint32_t cRefs;
    PVOID pvUm;
    MDL Mdl;
} VBOXVIDEOCM_ALLOC_REF, *PVBOXVIDEOCM_ALLOC_REF;


NTSTATUS vboxVideoCmAllocAlloc(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    ExAcquireFastMutex(&pMgr->Mutex);
    UINT iPage = vboxMmAlloc(&pMgr->Mm, cPages);
    if (iPage != VBOXWDDM_MM_VOID)
    {
        uint32_t offData = pMgr->offData + (iPage << PAGE_SHIFT);
        Assert(offData + cbSize <= pMgr->offData + pMgr->cbData);
        pAlloc->offData = offData;
        pAlloc->hGlobalHandle = vboxWddmHTablePut(&pMgr->AllocTable, pAlloc);
        ExReleaseFastMutex(&pMgr->Mutex);
        if (VBOXWDDM_HANDLE_INVALID != pAlloc->hGlobalHandle)
            return STATUS_SUCCESS;

        Assert(0);
        Status = STATUS_NO_MEMORY;
        vboxMmFree(&pMgr->Mm, iPage, cPages);
    }
    else
    {
        Assert(0);
        ExReleaseFastMutex(&pMgr->Mutex);
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }
    return Status;
}

VOID vboxVideoCmAllocDealloc(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    UINT cbSize = pAlloc->cbData;
    UINT cPages = BYTES_TO_PAGES(cbSize);
    UINT iPage = BYTES_TO_PAGES(pAlloc->offData - pMgr->offData);
    ExAcquireFastMutex(&pMgr->Mutex);
    vboxWddmHTableRemove(&pMgr->AllocTable, pAlloc->hGlobalHandle);
    vboxMmFree(&pMgr->Mm, iPage, cPages);
    ExReleaseFastMutex(&pMgr->Mutex);
}


NTSTATUS vboxVideoAMgrAllocCreate(PVBOXVIDEOCM_ALLOC_MGR pMgr, UINT cbSize, PVBOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDEOCM_ALLOC pAlloc = (PVBOXVIDEOCM_ALLOC)vboxWddmMemAllocZero(sizeof (*pAlloc));
    if (pAlloc)
    {
        pAlloc->cbData = cbSize;
        Status = vboxVideoCmAllocAlloc(pMgr, pAlloc);
        if (Status == STATUS_SUCCESS)
        {
            *ppAlloc = pAlloc;
            return STATUS_SUCCESS;
        }

        Assert(0);
        vboxWddmMemFree(pAlloc);
    }
    else
    {
        Assert(0);
        Status = STATUS_NO_MEMORY;
    }

    return Status;
}

VOID vboxVideoAMgrAllocDestroy(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC pAlloc)
{
    vboxVideoCmAllocDealloc(pMgr, pAlloc);
    vboxWddmMemFree(pAlloc);
}

NTSTATUS vboxVideoAMgrCtxAllocMap(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, PVBOXVIDEOCM_ALLOC pAlloc, PVBOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = STATUS_SUCCESS;
    PKEVENT pSynchEvent = NULL;

    if (pUmAlloc->hSynch)
    {
        Status = ObReferenceObjectByHandle((HANDLE)pUmAlloc->hSynch, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                (PVOID*)&pSynchEvent,
                NULL);
        AssertNtStatusSuccess(Status);
        Assert(pSynchEvent);
    }

    if (Status == STATUS_SUCCESS)
    {
        PVOID BaseVa = pMgr->pvData + pAlloc->offData - pMgr->offData;
        SIZE_T cbLength = pAlloc->cbData;

        PVBOXVIDEOCM_ALLOC_REF pAllocRef;
        pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmMemAllocZero(  sizeof(*pAllocRef)
                                                                 +   sizeof(PFN_NUMBER)
                                                                   * ADDRESS_AND_SIZE_TO_SPAN_PAGES(BaseVa, cbLength));
        if (pAllocRef)
        {
            pAllocRef->cRefs = 1;
            MmInitializeMdl(&pAllocRef->Mdl, BaseVa, cbLength);
            __try
            {
                MmProbeAndLockPages(&pAllocRef->Mdl, KernelMode, IoWriteAccess);
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                Assert(0);
                Status = STATUS_UNSUCCESSFUL;
            }

            if (Status == STATUS_SUCCESS)
            {
                PVOID pvUm = MmMapLockedPagesSpecifyCache(&pAllocRef->Mdl, UserMode, MmNonCached,
                          NULL, /* PVOID BaseAddress */
                          FALSE, /* ULONG BugCheckOnFailure */
                          NormalPagePriority);
                if (pvUm)
                {
                    pAllocRef->pvUm = pvUm;
                    pAllocRef->pContext = pContext;
                    pAllocRef->pAlloc = pAlloc;
                    pAllocRef->fUhgsmiType = pUmAlloc->fUhgsmiType;
                    pAllocRef->pSynchEvent = pSynchEvent;
                    ExAcquireFastMutex(&pContext->Mutex);
                    pAllocRef->hSessionHandle = vboxWddmHTablePut(&pContext->AllocTable, pAllocRef);
                    ExReleaseFastMutex(&pContext->Mutex);
                    if (VBOXWDDM_HANDLE_INVALID != pAllocRef->hSessionHandle)
                    {
                        pUmAlloc->hAlloc = pAllocRef->hSessionHandle;
                        pUmAlloc->cbData = pAlloc->cbData;
                        pUmAlloc->pvData = (uintptr_t)pvUm;
                        return STATUS_SUCCESS;
                    }

                    MmUnmapLockedPages(pvUm, &pAllocRef->Mdl);
                }
                else
                {
                    Assert(0);
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                }

                MmUnlockPages(&pAllocRef->Mdl);
            }

            vboxWddmMemFree(pAllocRef);
        }
        else
        {
            Assert(0);
            Status = STATUS_NO_MEMORY;
        }

        if (pSynchEvent)
            ObDereferenceObject(pSynchEvent);
    }
    else
    {
        Assert(0);
    }


    return Status;
}

NTSTATUS vboxVideoAMgrCtxAllocUnmap(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, VBOXDISP_KMHANDLE hSesionHandle,
                                    PVBOXVIDEOCM_ALLOC *ppAlloc)
{
    NTSTATUS Status = STATUS_SUCCESS;
    ExAcquireFastMutex(&pContext->Mutex);
    PVBOXVIDEOCM_ALLOC_REF pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableRemove(&pContext->AllocTable, hSesionHandle);
    ExReleaseFastMutex(&pContext->Mutex);
    if (pAllocRef)
    {
        /* wait for the dereference, i.e. for all commands involving this allocation to complete */
        vboxWddmCounterU32Wait(&pAllocRef->cRefs, 1);

        MmUnmapLockedPages(pAllocRef->pvUm, &pAllocRef->Mdl);

        MmUnlockPages(&pAllocRef->Mdl);
        *ppAlloc = pAllocRef->pAlloc;
        if (pAllocRef->pSynchEvent)
            ObDereferenceObject(pAllocRef->pSynchEvent);
        vboxWddmMemFree(pAllocRef);
    }
    else
    {
        Assert(0);
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

static PVBOXVIDEOCM_ALLOC_REF vboxVideoAMgrCtxAllocRefAcquire(PVBOXVIDEOCM_ALLOC_CONTEXT pContext,
                                                              VBOXDISP_KMHANDLE hSesionHandle)
{
    ExAcquireFastMutex(&pContext->Mutex);
    PVBOXVIDEOCM_ALLOC_REF pAllocRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableGet(&pContext->AllocTable, hSesionHandle);
    if (pAllocRef)
        ASMAtomicIncU32(&pAllocRef->cRefs);
    ExReleaseFastMutex(&pContext->Mutex);
    return pAllocRef;
}

static VOID vboxVideoAMgrCtxAllocRefRelease(PVBOXVIDEOCM_ALLOC_REF pRef)
{
    uint32_t cRefs = ASMAtomicDecU32(&pRef->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    Assert(cRefs >= 1); /* we do not do cleanup-on-zero here, instead we wait for the cRefs to reach 1 in
                           vboxVideoAMgrCtxAllocUnmap before unmapping */
    NOREF(cRefs);
}



NTSTATUS vboxVideoAMgrCtxAllocCreate(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, PVBOXVIDEOCM_UM_ALLOC pUmAlloc)
{
    PVBOXVIDEOCM_ALLOC pAlloc;
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vboxVideoAMgrAllocCreate(pMgr, pUmAlloc->cbData, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVideoAMgrCtxAllocMap(pContext, pAlloc, pUmAlloc);
        if (Status == STATUS_SUCCESS)
            return STATUS_SUCCESS;
        else
        {
            Assert(0);
        }
        vboxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

NTSTATUS vboxVideoAMgrCtxAllocDestroy(PVBOXVIDEOCM_ALLOC_CONTEXT pContext, VBOXDISP_KMHANDLE hSesionHandle)
{
    PVBOXVIDEOCM_ALLOC pAlloc;
    PVBOXVIDEOCM_ALLOC_MGR pMgr = pContext->pMgr;
    NTSTATUS Status = vboxVideoAMgrCtxAllocUnmap(pContext, hSesionHandle, &pAlloc);
    if (Status == STATUS_SUCCESS)
    {
        vboxVideoAMgrAllocDestroy(pMgr, pAlloc);
    }
    else
    {
        Assert(0);
    }
    return Status;
}

#ifdef VBOX_WITH_CRHGSMI
static DECLCALLBACK(VOID) vboxVideoAMgrAllocSubmitCompletion(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    RT_NOREF(pCmd);
    /* we should be called from our DPC routine */
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pvContext;
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
    UINT cBufs = pBody->cBuffers;
    for (UINT i = 0; i < cBufs; ++i)
    {
        VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
        PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)pBufCmd->u64GuestData;
        if (!pBufCmd->u32GuestData)
        {
            /* signal completion */
            if (pRef->pSynchEvent)
                KeSetEvent(pRef->pSynchEvent, 3, FALSE);
        }

        vboxVideoAMgrCtxAllocRefRelease(pRef);
    }

    vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
}

/* submits a set of chromium uhgsmi buffers to host for processing */
NTSTATUS vboxVideoAMgrCtxAllocSubmit(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_CONTEXT pContext, UINT cBuffers,
                                     VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *paBuffers)
{
    /* ensure we do not overflow the 32bit buffer size value */
    if (VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXVDMACMD_CHROMIUM_CMD, aBuffers) < cBuffers)
    {
        WARN(("number of buffers passed too big (%d), max is (%d)",
              cBuffers, VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXVDMACMD_CHROMIUM_CMD, aBuffers)));
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CMD, aBuffers[cBuffers]));

    VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_HOST *pDr = vboxVdmaCBufDrCreate(&pDevExt->u.primary.Vdma, cbCmd);
    if (pDr)
    {
        // vboxVdmaCBufDrCreate zero initializes the pDr
        pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
        pDr->cbBuf = cbCmd;
        pDr->rc = VERR_NOT_IMPLEMENTED;

        PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
        pHdr->enmType = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
        pHdr->u32CmdSpecific = 0;
        VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
        pBody->cBuffers = cBuffers;
        for (UINT i = 0; i < cBuffers; ++i)
        {
            VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
            VBOXWDDM_UHGSMI_BUFFER_UI_INFO_ESCAPE *pBufInfo = &paBuffers[i];
            PVBOXVIDEOCM_ALLOC_REF pRef = vboxVideoAMgrCtxAllocRefAcquire(pContext, pBufInfo->hAlloc);
            if (pRef)
            {
#ifdef DEBUG_misha
                Assert(pRef->cRefs == 2);
#endif
                pBufCmd->offBuffer = pRef->pAlloc->offData + pBufInfo->Info.offData;
                pBufCmd->cbBuffer = pBufInfo->Info.cbData;
                pBufCmd->u32GuestData = 0;
                pBufCmd->u64GuestData = (uintptr_t)pRef;
            }
            else
            {
                WARN(("vboxVideoAMgrCtxAllocRefAcquire failed for hAlloc(0x%x)\n", pBufInfo->hAlloc));
                /* release all previously acquired aloc references */
                for (UINT j = 0; j < i; ++j)
                {
                    VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmdJ = &pBody->aBuffers[j];
                    PVBOXVIDEOCM_ALLOC_REF pRefJ = (PVBOXVIDEOCM_ALLOC_REF)pBufCmdJ;
                    vboxVideoAMgrCtxAllocRefRelease(pRefJ);
                }
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
        }

        if (Status == STATUS_SUCCESS)
        {
            PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
            vboxVdmaDdiCmdInit(pDdiCmd, 0, 0, vboxVideoAMgrAllocSubmitCompletion, (void *)pDr);
            /* mark command as submitted & invisible for the dx runtime since dx did not originate it */
            vboxVdmaDdiCmdSubmittedNotDx(pDdiCmd);
            int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
            if (RT_SUCCESS(rc))
            {
                return STATUS_SUCCESS;
            }

            WARN(("vboxVdmaCBufDrSubmit failed with rc (%d)\n", rc));

            /* failure branch */
            /* release all previously acquired aloc references */
            for (UINT i = 0; i < cBuffers; ++i)
            {
                VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
                PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)pBufCmd;
                vboxVideoAMgrCtxAllocRefRelease(pRef);
            }
        }

        vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
    }
    else
    {
        Assert(0);
        /** @todo try flushing.. */
        LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}
#endif

NTSTATUS vboxVideoAMgrCreate(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_MGR pMgr, uint32_t offData, uint32_t cbData)
{
    Assert(!(offData & (PAGE_SIZE -1)));
    Assert(!(cbData & (PAGE_SIZE -1)));
    offData = VBOXWDDM_ROUNDBOUND(offData, PAGE_SIZE);
    cbData &= (~(PAGE_SIZE -1));
    Assert(cbData);
    if (!cbData)
        return STATUS_INVALID_PARAMETER;

    ExInitializeFastMutex(&pMgr->Mutex);
    NTSTATUS Status = vboxWddmHTableCreate(&pMgr->AllocTable, 64);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxMmInit(&pMgr->Mm, BYTES_TO_PAGES(cbData));
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            PHYSICAL_ADDRESS PhysicalAddress = {0};
            PhysicalAddress.QuadPart = VBoxCommonFromDeviceExt(pDevExt)->phVRAM.QuadPart + offData;
            pMgr->pvData = (uint8_t*)MmMapIoSpace(PhysicalAddress, cbData, MmNonCached);
            Assert(pMgr->pvData);
            if (pMgr->pvData)
            {
                pMgr->offData = offData;
                pMgr->cbData = cbData;
                return STATUS_SUCCESS;
            }
            else
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            vboxMmTerm(&pMgr->Mm);
        }
        vboxWddmHTableDestroy(&pMgr->AllocTable);
    }

    return Status;
}

NTSTATUS vboxVideoAMgrDestroy(PVBOXMP_DEVEXT pDevExt, PVBOXVIDEOCM_ALLOC_MGR pMgr)
{
    RT_NOREF(pDevExt);
    MmUnmapIoSpace(pMgr->pvData, pMgr->cbData);
    vboxMmTerm(&pMgr->Mm);
    vboxWddmHTableDestroy(&pMgr->AllocTable);
    return STATUS_SUCCESS;
}

NTSTATUS vboxVideoAMgrCtxCreate(PVBOXVIDEOCM_ALLOC_MGR pMgr, PVBOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    if (pMgr->pvData)
    {
        ExInitializeFastMutex(&pCtx->Mutex);
        Status = vboxWddmHTableCreate(&pCtx->AllocTable, 32);
        AssertNtStatusSuccess(Status);
        if (Status == STATUS_SUCCESS)
        {
            pCtx->pMgr = pMgr;
            return STATUS_SUCCESS;
        }
    }
    return Status;
}

NTSTATUS vboxVideoAMgrCtxDestroy(PVBOXVIDEOCM_ALLOC_CONTEXT pCtx)
{
    if (!pCtx->pMgr)
        return STATUS_SUCCESS;

    VBOXWDDM_HTABLE_ITERATOR Iter;
    NTSTATUS Status = STATUS_SUCCESS;

    vboxWddmHTableIterInit(&pCtx->AllocTable, &Iter);
    do
    {
        PVBOXVIDEOCM_ALLOC_REF pRef = (PVBOXVIDEOCM_ALLOC_REF)vboxWddmHTableIterNext(&Iter, NULL);
        if (!pRef)
            break;

        Assert(0);

        Status = vboxVideoAMgrCtxAllocDestroy(pCtx, pRef->hSessionHandle);
        AssertNtStatusSuccess(Status);
        if (Status != STATUS_SUCCESS)
            break;
        //        vboxWddmHTableIterRemoveCur(&Iter);
    } while (1);

    if (Status == STATUS_SUCCESS)
    {
        vboxWddmHTableDestroy(&pCtx->AllocTable);
    }

    return Status;
}


VOID vboxWddmSleep(uint32_t u32Val)
{
    RT_NOREF(u32Val);
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;

    KeDelayExecutionThread(KernelMode, FALSE, &Interval);
}

VOID vboxWddmCounterU32Wait(uint32_t volatile * pu32, uint32_t u32Val)
{
    LARGE_INTEGER Interval;
    Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;
    uint32_t u32CurVal;

    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

    while ((u32CurVal = ASMAtomicReadU32(pu32)) != u32Val)
    {
        Assert(u32CurVal >= u32Val);
        Assert(u32CurVal < UINT32_MAX/2);

        KeDelayExecutionThread(KernelMode, FALSE, &Interval);
    }
}

/* dump user-mode driver debug info */
static char    g_aVBoxUmdD3DCAPS9[304];
static VBOXDISPIFESCAPE_DBGDUMPBUF_FLAGS g_VBoxUmdD3DCAPS9Flags;
static BOOLEAN g_bVBoxUmdD3DCAPS9IsInited = FALSE;

static void vboxUmdDumpDword(DWORD *pvData, DWORD cData)
{
    char aBuf[16*4];
    DWORD dw1, dw2, dw3, dw4;
    for (UINT i = 0; i < (cData & (~3)); i+=4)
    {
        dw1 = *pvData++;
        dw2 = *pvData++;
        dw3 = *pvData++;
        dw4 = *pvData++;
        sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x, 0x%08x,\n", dw1, dw2, dw3, dw4);
        LOGREL(("%s", aBuf));
    }

    cData = cData % 4;
    switch (cData)
    {
        case 3:
            dw1 = *pvData++;
            dw2 = *pvData++;
            dw3 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x, 0x%08x\n", dw1, dw2, dw3);
            LOGREL(("%s", aBuf));
            break;
        case 2:
            dw1 = *pvData++;
            dw2 = *pvData++;
            sprintf(aBuf, "0x%08x, 0x%08x\n", dw1, dw2);
            LOGREL(("%s", aBuf));
            break;
        case 1:
            dw1 = *pvData++;
            sprintf(aBuf, "0x%8x\n", dw1);
            LOGREL(("%s", aBuf));
            break;
        default:
            break;
    }
}

static void vboxUmdDumpD3DCAPS9(void *pvData, PVBOXDISPIFESCAPE_DBGDUMPBUF_FLAGS pFlags)
{
    AssertCompile(!(sizeof (g_aVBoxUmdD3DCAPS9) % sizeof (DWORD)));
    LOGREL(("*****Start Dumping D3DCAPS9:*******"));
    LOGREL(("WoW64 flag(%d)", (UINT)pFlags->WoW64));
    vboxUmdDumpDword((DWORD*)pvData, sizeof (g_aVBoxUmdD3DCAPS9) / sizeof (DWORD));
    LOGREL(("*****End Dumping D3DCAPS9**********"));
}

NTSTATUS vboxUmdDumpBuf(PVBOXDISPIFESCAPE_DBGDUMPBUF pBuf, uint32_t cbBuffer)
{
    if (cbBuffer < RT_OFFSETOF(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]))
    {
        WARN(("Buffer too small"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cbString = cbBuffer - RT_OFFSETOF(VBOXDISPIFESCAPE_DBGDUMPBUF, aBuf[0]);
    switch (pBuf->enmType)
    {
        case VBOXDISPIFESCAPE_DBGDUMPBUF_TYPE_D3DCAPS9:
        {
            if (cbString != sizeof (g_aVBoxUmdD3DCAPS9))
            {
                WARN(("wrong caps size, expected %d, but was %d", sizeof (g_aVBoxUmdD3DCAPS9), cbString));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (g_bVBoxUmdD3DCAPS9IsInited)
            {
                if (!memcmp(g_aVBoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVBoxUmdD3DCAPS9)))
                    break;

                WARN(("caps do not match!"));
                vboxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
                break;
            }

            memcpy(g_aVBoxUmdD3DCAPS9, pBuf->aBuf, sizeof (g_aVBoxUmdD3DCAPS9));
            g_VBoxUmdD3DCAPS9Flags = pBuf->Flags;
            g_bVBoxUmdD3DCAPS9IsInited = TRUE;
            vboxUmdDumpD3DCAPS9(pBuf->aBuf, &pBuf->Flags);
        }
        default: break; /* Shuts up MSC. */
    }

    return Status;
}

#if 0
VOID vboxShRcTreeInit(PVBOXMP_DEVEXT pDevExt)
{
    ExInitializeFastMutex(&pDevExt->ShRcTreeMutex);
    pDevExt->ShRcTree = NULL;
}

VOID vboxShRcTreeTerm(PVBOXMP_DEVEXT pDevExt)
{
    Assert(!pDevExt->ShRcTree);
    pDevExt->ShRcTree = NULL;
}

BOOLEAN vboxShRcTreePut(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    pAlloc->ShRcTreeEntry.Key = (AVLPVKEY)hSharedRc;
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    bool bRc = RTAvlPVInsert(&pDevExt->ShRcTree, &pAlloc->ShRcTreeEntry);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    Assert(bRc);
    return (BOOLEAN)bRc;
}

#define PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(_p) \
    ((PVBOXWDDM_ALLOCATION)(((uint8_t*)(_p)) - RT_OFFSETOF(VBOXWDDM_ALLOCATION, ShRcTreeEntry)))

PVBOXWDDM_ALLOCATION vboxShRcTreeGet(PVBOXMP_DEVEXT pDevExt, HANDLE hSharedRc)
{
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVGet(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVBOXWDDM_ALLOCATION pAlloc = PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    return pAlloc;
}

BOOLEAN vboxShRcTreeRemove(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAlloc)
{
    HANDLE hSharedRc = pAlloc->hSharedHandle;
    if (!hSharedRc)
    {
        WARN(("invalid call with zero shared handle!"));
        return FALSE;
    }
    ExAcquireFastMutex(&pDevExt->ShRcTreeMutex);
    PAVLPVNODECORE pNode = RTAvlPVRemove(&pDevExt->ShRcTree, (AVLPVKEY)hSharedRc);
    ExReleaseFastMutex(&pDevExt->ShRcTreeMutex);
    if (!pNode)
        return NULL;
    PVBOXWDDM_ALLOCATION pRetAlloc = PVBOXWDDM_ALLOCATION_FROM_SHRCTREENODE(pNode);
    Assert(pRetAlloc == pAlloc);
    return !!pRetAlloc;
}
#endif

NTSTATUS vboxWddmDrvCfgInit(PUNICODE_STRING pRegStr)
{
    HANDLE hKey;
    OBJECT_ATTRIBUTES ObjAttr;

    InitializeObjectAttributes(&ObjAttr, pRegStr, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    NTSTATUS Status = ZwOpenKey(&hKey, GENERIC_READ, &ObjAttr);
    if (!NT_SUCCESS(Status))
    {
        WARN(("ZwOpenKey for settings key failed, Status 0x%x", Status));
        return Status;
    }

    DWORD dwValue = 0;
    Status = vboxWddmRegQueryValueDword(hKey, VBOXWDDM_CFG_STR_LOG_UM, &dwValue);
    if (NT_SUCCESS(Status))
        g_VBoxLogUm = dwValue;

    ZwClose(hKey);

    return Status;
}

NTSTATUS vboxWddmThreadCreate(PKTHREAD * ppThread, PKSTART_ROUTINE pStartRoutine, PVOID pStartContext)
{
    NTSTATUS fStatus;
    HANDLE hThread;
    OBJECT_ATTRIBUTES fObjectAttributes;

    Assert(KeGetCurrentIrql() == PASSIVE_LEVEL);

    InitializeObjectAttributes(&fObjectAttributes, NULL, OBJ_KERNEL_HANDLE,
                        NULL, NULL);

    fStatus = PsCreateSystemThread(&hThread, THREAD_ALL_ACCESS,
                        &fObjectAttributes, NULL, NULL,
                        (PKSTART_ROUTINE) pStartRoutine, pStartContext);
    if (!NT_SUCCESS(fStatus))
      return fStatus;

    ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS, NULL,
                        KernelMode, (PVOID*) ppThread, NULL);
    ZwClose(hThread);
    return STATUS_SUCCESS;
}

#ifdef VBOX_VDMA_WITH_WATCHDOG
static int vboxWddmWdProgram(PVBOXMP_DEVEXT pDevExt, uint32_t cMillis)
{
    int rc = VINF_SUCCESS;
    VBOXVDMA_CTL RT_UNTRUSTED_VOLATILE_GUEST *pCmd
        = (VBOXVDMA_CTL RT_UNTRUSTED_VOLATILE_GUEST *)VBoxSHGSMICommandAlloc(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx,
                                                                             sizeof(VBOXVDMA_CTL), HGSMI_CH_VBVA, VBVA_VDMA_CTL);
    if (pCmd)
    {
        pCmd->enmCtl = VBOXVDMA_CTL_TYPE_WATCHDOG;
        pCmd->u32Offset = cMillis;
        pCmd->i32Result = VERR_NOT_SUPPORTED;

        const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
        Assert(pHdr);
        if (pHdr)
        {
            do
            {
                HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    VBVO_PORT_WRITE_U32(VBoxCommonFromDeviceExt(pDevExt)->guestCtx.port, offCmd);
                    /* Make the compiler aware that the host has changed memory. */
                    ASMCompilerBarrier();
                    rc = VBoxSHGSMICommandDoneSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        rc = pCmd->i32Result;
                        AssertRC(rc);
                    }
                    break;
                }
                else
                    rc = VERR_INVALID_PARAMETER;
                /* fail to submit, cancel it */
                VBoxSHGSMICommandCancelSynch(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pHdr);
            } while (0);
        }

        VBoxSHGSMICommandFree(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pCmd);
    }
    else
    {
        LOGREL(("HGSMIHeapAlloc failed"));
        rc = VERR_OUT_OF_RESOURCES;
    }
    return rc;
}

static uint32_t g_VBoxWdTimeout = 4000;
/* if null g_VBoxWdTimeout / 2 is used */
static uint32_t g_VBoxWdTimerPeriod = 0;

static VOID vboxWddmWdThread(PVOID pvUser)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvUser;
    BOOLEAN bExit = FALSE;
    int rc;
    while (1)
    {
        if (!bExit)
        {
            rc = vboxWddmWdProgram(pDevExt, g_VBoxWdTimeout /* ms */);
            AssertRC(rc);
        }
        else
        {
            rc = vboxWddmWdProgram(pDevExt, 0 /* to disable WatchDog */);
            AssertRC(rc);
            break;
        }
        LARGE_INTEGER Timeout;
        uint32_t timerTimeOut = g_VBoxWdTimerPeriod ? g_VBoxWdTimerPeriod : g_VBoxWdTimeout / 2;
        Timeout.QuadPart = 10000ULL * timerTimeOut /* ms */;
        NTSTATUS Status = KeWaitForSingleObject(&pDevExt->WdEvent, Executive, KernelMode, FALSE, &Timeout);
        if (Status != STATUS_TIMEOUT)
            bExit = TRUE;
    }
}

NTSTATUS vboxWddmWdInit(PVBOXMP_DEVEXT pDevExt)
{
    KeInitializeEvent(&pDevExt->WdEvent, NotificationEvent, FALSE);

    NTSTATUS Status = vboxWddmThreadCreate(&pDevExt->pWdThread, vboxWddmWdThread, pDevExt);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxWddmThreadCreate failed, Status 0x%x", Status));
        pDevExt->pWdThread = NULL;
    }
    return Status;
}

NTSTATUS vboxWddmWdTerm(PVBOXMP_DEVEXT pDevExt)
{
    if (!pDevExt->pWdThread)
        return STATUS_SUCCESS;

    KeSetEvent(&pDevExt->WdEvent, 0, FALSE);

    KeWaitForSingleObject(pDevExt->pWdThread, Executive, KernelMode, FALSE, NULL);
    ObDereferenceObject(pDevExt->pWdThread);
    pDevExt->pWdThread = NULL;
    return STATUS_SUCCESS;
}
#endif

static int vboxWddmSlConfigure(PVBOXMP_DEVEXT pDevExt, uint32_t fFlags)
{
    PHGSMIGUESTCOMMANDCONTEXT pCtx = &VBoxCommonFromDeviceExt(pDevExt)->guestCtx;
    VBVASCANLINECFG *pCfg;
    int rc = VINF_SUCCESS;

    /* Allocate the IO buffer. */
    pCfg = (VBVASCANLINECFG *)VBoxHGSMIBufferAlloc(pCtx,
                                       sizeof (VBVASCANLINECFG), HGSMI_CH_VBVA,
                                       VBVA_SCANLINE_CFG);

    if (pCfg)
    {
        /* Prepare data to be sent to the host. */
        pCfg->rc    = VERR_NOT_IMPLEMENTED;
        pCfg->fFlags = fFlags;
        rc = VBoxHGSMIBufferSubmit(pCtx, pCfg);
        if (RT_SUCCESS(rc))
        {
            AssertRC(pCfg->rc);
            rc = pCfg->rc;
        }
        /* Free the IO buffer. */
        VBoxHGSMIBufferFree(pCtx, pCfg);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

NTSTATUS VBoxWddmSlEnableVSyncNotification(PVBOXMP_DEVEXT pDevExt, BOOLEAN fEnable)
{
    if (!pDevExt->bVSyncTimerEnabled == !fEnable)
        return STATUS_SUCCESS;

    if (!fEnable)
    {
        KeCancelTimer(&pDevExt->VSyncTimer);
    }
    else
    {
        KeQuerySystemTime((PLARGE_INTEGER)&pDevExt->VSyncTime);

        LARGE_INTEGER DueTime;
        DueTime.QuadPart = -166666LL; /* 60 Hz */
        KeSetTimerEx(&pDevExt->VSyncTimer, DueTime, 16, &pDevExt->VSyncDpc);
    }

    pDevExt->bVSyncTimerEnabled = !!fEnable;

    return STATUS_SUCCESS;
}

NTSTATUS VBoxWddmSlGetScanLine(PVBOXMP_DEVEXT pDevExt, DXGKARG_GETSCANLINE *pGetScanLine)
{
    Assert((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pGetScanLine->VidPnTargetId);
    VBOXWDDM_TARGET *pTarget = &pDevExt->aTargets[pGetScanLine->VidPnTargetId];
    Assert(pTarget->Size.cx);
    Assert(pTarget->Size.cy);
    if (pTarget->Size.cy)
    {
        uint32_t curScanLine = 0;
        BOOL bVBlank = FALSE;
        LARGE_INTEGER DevVSyncTime;
        DevVSyncTime.QuadPart =  ASMAtomicReadU64((volatile uint64_t*)&pDevExt->VSyncTime.QuadPart);
        LARGE_INTEGER VSyncTime;
        KeQuerySystemTime(&VSyncTime);

        if (VSyncTime.QuadPart < DevVSyncTime.QuadPart)
        {
            WARN(("vsync time is less than the one stored in device"));
            bVBlank = TRUE;
        }
        else
        {
            VSyncTime.QuadPart = VSyncTime.QuadPart - DevVSyncTime.QuadPart;
            /*
             * Check whether we are in VBlank state or actively drawing a scan line
             * 10% of the 60Hz are dedicated to VBlank.
             *
             * Time intervals are in 100ns steps.
             */
            LARGE_INTEGER VSyncPeriod;
            VSyncPeriod.QuadPart = VSyncTime.QuadPart % 166666LL; /* ASSUMES 60Hz*/
            if (VSyncPeriod.QuadPart >= 150000LL)
                bVBlank = TRUE;
            else
                curScanLine = (uint32_t)((pTarget->Size.cy * VSyncPeriod.QuadPart) / 150000LL);
        }

        pGetScanLine->ScanLine = curScanLine;
        pGetScanLine->InVerticalBlank = bVBlank;
    }
    else
    {
        pGetScanLine->InVerticalBlank = TRUE;
        pGetScanLine->ScanLine = 0;
    }
    return STATUS_SUCCESS;
}

static BOOLEAN vboxWddmSlVSyncIrqCb(PVOID pvContext)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)pvContext;
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    BOOLEAN bNeedDpc = FALSE;
    for (UINT i = 0; i < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        PVBOXWDDM_TARGET pTarget = &pDevExt->aTargets[i];
        if (pTarget->fConnected)
        {
            memset(&notify, 0, sizeof(DXGKARGCB_NOTIFY_INTERRUPT_DATA));
            notify.InterruptType = g_VBoxDisplayOnly?
                                       DXGK_INTERRUPT_DISPLAYONLY_VSYNC:
                                       DXGK_INTERRUPT_CRTC_VSYNC;
            notify.CrtcVsync.VidPnTargetId = i;
            pDevExt->u.primary.DxgkInterface.DxgkCbNotifyInterrupt(pDevExt->u.primary.DxgkInterface.DeviceHandle, &notify);
            bNeedDpc = TRUE;
        }
    }

    if (bNeedDpc)
    {
        pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
    }

    return FALSE;
}

static VOID vboxWddmSlVSyncDpc(
  __in      struct _KDPC *Dpc,
  __in_opt  PVOID DeferredContext,
  __in_opt  PVOID SystemArgument1,
  __in_opt  PVOID SystemArgument2
)
{
    RT_NOREF(Dpc, SystemArgument1, SystemArgument2);
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)DeferredContext;
    Assert(!pDevExt->fVSyncInVBlank);
    ASMAtomicWriteU32(&pDevExt->fVSyncInVBlank, 1);

    BOOLEAN bDummy;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxWddmSlVSyncIrqCb,
            pDevExt,
            0, /* IN ULONG MessageNumber */
            &bDummy);
    if (!NT_SUCCESS(Status))
        WARN(("DxgkCbSynchronizeExecution failed Status %#x", Status));

    LARGE_INTEGER VSyncTime;
    KeQuerySystemTime(&VSyncTime);
    ASMAtomicWriteU64((volatile uint64_t*)&pDevExt->VSyncTime.QuadPart, VSyncTime.QuadPart);

    ASMAtomicWriteU32(&pDevExt->fVSyncInVBlank, 0);
}

NTSTATUS VBoxWddmSlInit(PVBOXMP_DEVEXT pDevExt)
{
    pDevExt->bVSyncTimerEnabled = FALSE;
    pDevExt->fVSyncInVBlank = 0;
    KeQuerySystemTime((PLARGE_INTEGER)&pDevExt->VSyncTime);
    KeInitializeTimer(&pDevExt->VSyncTimer);
    KeInitializeDpc(&pDevExt->VSyncDpc, vboxWddmSlVSyncDpc, pDevExt);
    return STATUS_SUCCESS;
}

NTSTATUS VBoxWddmSlTerm(PVBOXMP_DEVEXT pDevExt)
{
    KeCancelTimer(&pDevExt->VSyncTimer);
    return STATUS_SUCCESS;
}

void vboxWddmDiInitDefault(DXGK_DISPLAY_INFORMATION *pInfo, PHYSICAL_ADDRESS PhAddr, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    pInfo->Width = 1024;
    pInfo->Height = 768;
    pInfo->Pitch = pInfo->Width * 4;
    pInfo->ColorFormat = D3DDDIFMT_A8R8G8B8;
    pInfo->PhysicAddress = PhAddr;
    pInfo->TargetId = VidPnSourceId;
    pInfo->AcpiId = 0;
}

void vboxWddmDiToAllocData(PVBOXMP_DEVEXT pDevExt, const DXGK_DISPLAY_INFORMATION *pInfo, PVBOXWDDM_ALLOC_DATA pAllocData)
{
    pAllocData->SurfDesc.width = pInfo->Width;
    pAllocData->SurfDesc.height = pInfo->Height;
    pAllocData->SurfDesc.format = pInfo->ColorFormat;
    pAllocData->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pInfo->ColorFormat);
    pAllocData->SurfDesc.pitch = pInfo->Pitch;
    pAllocData->SurfDesc.depth = 1;
    pAllocData->SurfDesc.slicePitch = pInfo->Pitch;
    pAllocData->SurfDesc.cbSize = pInfo->Pitch * pInfo->Height;
    pAllocData->SurfDesc.VidPnSourceId = pInfo->TargetId;
    pAllocData->SurfDesc.RefreshRate.Numerator = 60000;
    pAllocData->SurfDesc.RefreshRate.Denominator = 1000;

    /* the address here is not a VRAM offset! so convert it to offset */
    vboxWddmAddrSetVram(&pAllocData->Addr, 1,
            vboxWddmVramAddrToOffset(pDevExt, pInfo->PhysicAddress));
}

void vboxWddmDmSetupDefaultVramLocation(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID ModifiedVidPnSourceId,
                                        VBOXWDDM_SOURCE *paSources)
{
    PVBOXWDDM_SOURCE pSource = &paSources[ModifiedVidPnSourceId];
    AssertRelease(g_VBoxDisplayOnly);
    ULONG offVram = vboxWddmVramCpuVisibleSegmentSize(pDevExt);
    offVram /= VBoxCommonFromDeviceExt(pDevExt)->cDisplays;
    offVram &= ~PAGE_OFFSET_MASK;
    offVram *= ModifiedVidPnSourceId;

    if (vboxWddmAddrSetVram(&pSource->AllocData.Addr, 1, offVram))
        pSource->u8SyncState &= ~VBOXWDDM_HGSYNC_F_SYNCED_LOCATION;
}

