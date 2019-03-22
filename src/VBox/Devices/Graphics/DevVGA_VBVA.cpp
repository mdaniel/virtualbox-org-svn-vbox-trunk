/* $Id$ */
/** @file
 * VirtualBox Video Acceleration (VBVA).
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VGA
#include <VBox/vmm/pdmifs.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/VMMDev.h>
#include <VBox/AssertGuest.h>
#include <VBoxVideo.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/param.h>
#ifdef VBOX_WITH_VIDEOHWACCEL
#include <iprt/semaphore.h>
#endif

#include "DevVGA.h"

/* A very detailed logging. */
#if 0 // def DEBUG_sunlover
#define LOGVBVABUFFER(a) LogFlow(a)
#else
#define LOGVBVABUFFER(a) do {} while (0)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct VBVAPARTIALRECORD
{
    uint8_t *pu8;
    uint32_t cb;
} VBVAPARTIALRECORD;

typedef struct VBVADATA
{
    struct
    {
        VBVABUFFER RT_UNTRUSTED_VOLATILE_GUEST *pVBVA;   /**< Pointer to the guest memory with the VBVABUFFER. */
        uint8_t RT_UNTRUSTED_VOLATILE_GUEST    *pu8Data; /**< For convenience, pointer to the guest ring buffer (VBVABUFFER::au8Data). */
    } guest;
    uint32_t u32VBVAOffset;           /**< VBVABUFFER offset in the guest VRAM. */
    VBVAPARTIALRECORD partialRecord;  /**< Partial record temporary storage. */
    uint32_t off32Data;               /**< The offset where the data starts in the VBVABUFFER.
                                       * The host code uses it instead of VBVABUFFER::off32Data. */
    uint32_t indexRecordFirst;        /**< Index of the first filled record in VBVABUFFER::aRecords. */
    uint32_t cbPartialWriteThreshold; /**< Copy of VBVABUFFER::cbPartialWriteThreshold used by host code. */
    uint32_t cbData;                  /**< Copy of VBVABUFFER::cbData used by host code. */
} VBVADATA;

typedef struct VBVAVIEW
{
    VBVAINFOVIEW    view;
    VBVAINFOSCREEN  screen;
    VBVADATA        vbva;
} VBVAVIEW;

typedef struct VBVAMOUSESHAPEINFO
{
    bool fSet;
    bool fVisible;
    bool fAlpha;
    uint32_t u32HotX;
    uint32_t u32HotY;
    uint32_t u32Width;
    uint32_t u32Height;
    uint32_t cbShape;
    uint32_t cbAllocated;
    uint8_t *pu8Shape;
} VBVAMOUSESHAPEINFO;

/** @todo saved state: save and restore VBVACONTEXT */
typedef struct VBVACONTEXT
{
    uint32_t cViews;
    VBVAVIEW aViews[VBOX_VIDEO_MAX_SCREENS];
    VBVAMOUSESHAPEINFO mouseShapeInfo;
    bool fPaused;
    uint32_t xCursor;
    uint32_t yCursor;
    VBVAMODEHINT aModeHints[VBOX_VIDEO_MAX_SCREENS];
} VBVACONTEXT;


static void vbvaDataCleanup(VBVADATA *pVBVAData)
{
    if (pVBVAData->guest.pVBVA)
    {
        pVBVAData->guest.pVBVA->hostFlags.u32HostEvents      = 0;
        pVBVAData->guest.pVBVA->hostFlags.u32SupportedOrders = 0;
    }

    RTMemFree(pVBVAData->partialRecord.pu8);

    RT_ZERO(*pVBVAData);
    pVBVAData->u32VBVAOffset = HGSMIOFFSET_VOID;
}

/** Copies @a cb bytes from the VBVA ring buffer to the @a pbDst.
 * Used for partial records or for records which cross the ring boundary.
 */
static bool vbvaFetchBytes(VBVADATA *pVBVAData, uint8_t *pbDst, uint32_t cb)
{
    if (cb >= pVBVAData->cbData)
    {
        AssertMsgFailed(("cb = 0x%08X, ring buffer size 0x%08X", cb, pVBVAData->cbData));
        return false;
    }

    const uint8_t RT_UNTRUSTED_VOLATILE_GUEST *pbSrc = &pVBVAData->guest.pu8Data[pVBVAData->off32Data];
    const uint32_t u32BytesTillBoundary = pVBVAData->cbData - pVBVAData->off32Data;
    const int32_t  i32Diff              = cb - u32BytesTillBoundary;

    if (i32Diff <= 0)
    {
        /* Chunk will not cross buffer boundary. */
        RT_BCOPY_VOLATILE(pbDst, pbSrc, cb);
    }
    else
    {
        /* Chunk crosses buffer boundary. */
        RT_BCOPY_VOLATILE(pbDst, pbSrc, u32BytesTillBoundary);
        RT_BCOPY_VOLATILE(pbDst + u32BytesTillBoundary, &pVBVAData->guest.pu8Data[0], i32Diff);
    }

    /* Advance data offset and sync with guest. */
    pVBVAData->off32Data = (pVBVAData->off32Data + cb) % pVBVAData->cbData;
    pVBVAData->guest.pVBVA->off32Data = pVBVAData->off32Data;
    return true;
}


static bool vbvaPartialRead(uint32_t cbRecord, VBVADATA *pVBVAData)
{
    VBVAPARTIALRECORD *pPartialRecord = &pVBVAData->partialRecord;
    uint8_t *pu8New;

    LOGVBVABUFFER(("vbvaPartialRead: p = %p, cb = %d, cbRecord 0x%08X\n",
                   pPartialRecord->pu8, pPartialRecord->cb, cbRecord));

    Assert(cbRecord > pPartialRecord->cb); /* Caller ensures this. */

    const uint32_t cbChunk = cbRecord - pPartialRecord->cb;
    if (cbChunk >= pVBVAData->cbData)
    {
        return false;
    }

    if (pPartialRecord->pu8)
    {
        Assert(pPartialRecord->cb);
        pu8New = (uint8_t *)RTMemRealloc(pPartialRecord->pu8, cbRecord);
    }
    else
    {
        Assert(!pPartialRecord->cb);
        pu8New = (uint8_t *)RTMemAlloc(cbRecord);
    }

    if (!pu8New)
    {
        /* Memory allocation failed, fail the function. */
        Log(("vbvaPartialRead: failed to (re)alocate memory for partial record!!! cbRecord 0x%08X\n",
             cbRecord));

        return false;
    }

    /* Fetch data from the ring buffer. */
    if (!vbvaFetchBytes(pVBVAData, pu8New + pPartialRecord->cb, cbChunk))
    {
        return false;
    }

    pPartialRecord->pu8 = pu8New;
    pPartialRecord->cb = cbRecord;

    return true;
}

/**
 * For contiguous chunks just return the address in the buffer. For crossing
 * boundary - allocate a buffer from heap.
 */
static bool vbvaFetchCmd(VBVADATA *pVBVAData, VBVACMDHDR RT_UNTRUSTED_VOLATILE_GUEST **ppHdr, uint32_t *pcbCmd)
{
    VBVAPARTIALRECORD *pPartialRecord = &pVBVAData->partialRecord;
    uint32_t indexRecordFirst = pVBVAData->indexRecordFirst;
    const uint32_t indexRecordFree = ASMAtomicReadU32(&pVBVAData->guest.pVBVA->indexRecordFree);

    LOGVBVABUFFER(("first = %d, free = %d\n",
                   indexRecordFirst, indexRecordFree));

    if (indexRecordFree >= RT_ELEMENTS(pVBVAData->guest.pVBVA->aRecords))
    {
        return false;
    }

    if (indexRecordFirst == indexRecordFree)
    {
        /* No records to process. Return without assigning output variables. */
        return true;
    }

    uint32_t cbRecordCurrent = ASMAtomicReadU32(&pVBVAData->guest.pVBVA->aRecords[indexRecordFirst].cbRecord);

    LOGVBVABUFFER(("cbRecord = 0x%08X, pPartialRecord->cb = 0x%08X\n", cbRecordCurrent, pPartialRecord->cb));

    uint32_t cbRecord = cbRecordCurrent & ~VBVA_F_RECORD_PARTIAL;

    if (cbRecord > VBVA_MAX_RECORD_SIZE)
    {
        return false;
    }

    if (pPartialRecord->cb)
    {
        /* There is a partial read in process. Continue with it. */
        Assert (pPartialRecord->pu8);

        LOGVBVABUFFER(("continue partial record cb = %d cbRecord 0x%08X, first = %d, free = %d\n",
                      pPartialRecord->cb, cbRecordCurrent, indexRecordFirst, indexRecordFree));

        if (cbRecord > pPartialRecord->cb)
        {
            /* New data has been added to the record. */
            if (!vbvaPartialRead(cbRecord, pVBVAData))
            {
                return false;
            }
        }

        if (!(cbRecordCurrent & VBVA_F_RECORD_PARTIAL))
        {
            /* The record is completed by guest. Return it to the caller. */
            *ppHdr = (VBVACMDHDR *)pPartialRecord->pu8;
            *pcbCmd = pPartialRecord->cb;

            pPartialRecord->pu8 = NULL;
            pPartialRecord->cb = 0;

            /* Advance the record index and sync with guest. */
            pVBVAData->indexRecordFirst = (indexRecordFirst + 1) % RT_ELEMENTS(pVBVAData->guest.pVBVA->aRecords);
            pVBVAData->guest.pVBVA->indexRecordFirst = pVBVAData->indexRecordFirst;

            LOGVBVABUFFER(("partial done ok, data = %d, free = %d\n",
                          pVBVAData->off32Data, pVBVAData->guest.pVBVA->off32Free));
        }

        return true;
    }

    /* A new record need to be processed. */
    if (cbRecordCurrent & VBVA_F_RECORD_PARTIAL)
    {
        /* Current record is being written by guest. '=' is important here,
         * because the guest will do a FLUSH at this condition.
         * This partial record is too large for the ring buffer and must
         * be accumulated in an allocated buffer.
         */
        if (cbRecord >= pVBVAData->cbData - pVBVAData->cbPartialWriteThreshold)
        {
            /* Partial read must be started. */
            if (!vbvaPartialRead(cbRecord, pVBVAData))
            {
                return false;
            }

            LOGVBVABUFFER(("started partial record cb = 0x%08X cbRecord 0x%08X, first = %d, free = %d\n",
                          pPartialRecord->cb, cbRecordCurrent, indexRecordFirst, indexRecordFree));
        }

        return true;
    }

    /* Current record is complete. If it is not empty, process it. */
    if (cbRecord >= pVBVAData->cbData)
    {
        return false;
    }

    if (cbRecord)
    {
        /* The size of largest contiguous chunk in the ring buffer. */
        uint32_t u32BytesTillBoundary = pVBVAData->cbData - pVBVAData->off32Data;

        /* The pointer to data in the ring buffer. */
        uint8_t RT_UNTRUSTED_VOLATILE_GUEST *pbSrc = &pVBVAData->guest.pu8Data[pVBVAData->off32Data];

        /* Fetch or point the data. */
        if (u32BytesTillBoundary >= cbRecord)
        {
            /* The command does not cross buffer boundary. Return address in the buffer. */
            *ppHdr = (VBVACMDHDR RT_UNTRUSTED_VOLATILE_GUEST *)pbSrc;

            /* The data offset will be updated in vbvaReleaseCmd. */
        }
        else
        {
            /* The command crosses buffer boundary. Rare case, so not optimized. */
            uint8_t *pbDst = (uint8_t *)RTMemAlloc(cbRecord);
            if (!pbDst)
            {
                LogFlowFunc (("could not allocate %d bytes from heap!!!\n", cbRecord));
                return false;
            }

            vbvaFetchBytes(pVBVAData, pbDst, cbRecord);

            *ppHdr = (VBVACMDHDR *)pbDst;

            LOGVBVABUFFER(("Allocated from heap %p\n", pbDst));
        }
    }

    *pcbCmd = cbRecord;

    /* Advance the record index and sync with guest. */
    pVBVAData->indexRecordFirst = (indexRecordFirst + 1) % RT_ELEMENTS(pVBVAData->guest.pVBVA->aRecords);
    pVBVAData->guest.pVBVA->indexRecordFirst = pVBVAData->indexRecordFirst;

    LOGVBVABUFFER(("done ok, data = %d, free = %d\n",
                  pVBVAData->off32Data, pVBVAData->guest.pVBVA->off32Free));

    return true;
}

static void vbvaReleaseCmd(VBVADATA *pVBVAData, VBVACMDHDR RT_UNTRUSTED_VOLATILE_GUEST *pHdr, uint32_t cbCmd)
{
    VBVAPARTIALRECORD                          *pPartialRecord = &pVBVAData->partialRecord;
    const uint8_t RT_UNTRUSTED_VOLATILE_GUEST  *pbRingBuffer   = pVBVAData->guest.pu8Data;

    if (   (uintptr_t)pHdr >= (uintptr_t)pbRingBuffer
        && (uintptr_t)pHdr < (uintptr_t)&pbRingBuffer[pVBVAData->cbData])
    {
        /* The pointer is inside ring buffer. Must be continuous chunk. */
        Assert(pVBVAData->cbData - (uint32_t)((uint8_t *)pHdr - pbRingBuffer) >= cbCmd);

        /* Advance data offset and sync with guest. */
        pVBVAData->off32Data = (pVBVAData->off32Data + cbCmd) % pVBVAData->cbData;
        pVBVAData->guest.pVBVA->off32Data = pVBVAData->off32Data;

        Assert(!pPartialRecord->pu8 && pPartialRecord->cb == 0);
    }
    else
    {
        /* The pointer is outside. It is then an allocated copy. */
        LOGVBVABUFFER(("Free heap %p\n", pHdr));

        if ((uint8_t *)pHdr == pPartialRecord->pu8)
        {
            pPartialRecord->pu8 = NULL;
            pPartialRecord->cb = 0;
        }
        else
        {
            Assert(!pPartialRecord->pu8 && pPartialRecord->cb == 0);
        }

        RTMemFree((void *)pHdr);
    }
}

static int vbvaFlushProcess(unsigned uScreenId, PVGASTATE pVGAState, VBVADATA *pVBVAData)
{
    LOGVBVABUFFER(("uScreenId %d, indexRecordFirst = %d, indexRecordFree = %d, off32Data = %d, off32Free = %d\n",
                  uScreenId, pVBVAData->indexRecordFirst, pVBVAData->guest.pVBVA->indexRecordFree,
                  pVBVAData->off32Data, pVBVAData->guest.pVBVA->off32Free));
    struct {
        /* The rectangle that includes all dirty rectangles. */
        int32_t xLeft;
        int32_t xRight;
        int32_t yTop;
        int32_t yBottom;
    } dirtyRect;
    RT_ZERO(dirtyRect);

    bool fUpdate = false; /* Whether there were any updates. */
    bool fDirtyEmpty = true;

    for (;;)
    {
        /* Fetch the command data. */
        VBVACMDHDR RT_UNTRUSTED_VOLATILE_GUEST *pHdr  = NULL;
        uint32_t                                cbCmd = UINT32_MAX;
        if (!vbvaFetchCmd(pVBVAData, &pHdr, &cbCmd))
        {
            LogFunc(("unable to fetch command. off32Data = %d, off32Free = %d!!!\n",
                     pVBVAData->off32Data, pVBVAData->guest.pVBVA->off32Free));
            return VERR_NOT_SUPPORTED;
        }

        if (cbCmd == uint32_t(~0))
        {
            /* No more commands yet in the queue. */
            break;
        }

        if (cbCmd < sizeof(VBVACMDHDR))
        {
            LogFunc(("short command. off32Data = %d, off32Free = %d, cbCmd %d!!!\n",
                  pVBVAData->off32Data, pVBVAData->guest.pVBVA->off32Free, cbCmd));

            return VERR_NOT_SUPPORTED;
        }

        if (cbCmd != 0)
        {
            if (!fUpdate)
            {
                pVGAState->pDrv->pfnVBVAUpdateBegin(pVGAState->pDrv, uScreenId);
                fUpdate = true;
            }

            /* Updates the rectangle and sends the command to the VRDP server. */
            pVGAState->pDrv->pfnVBVAUpdateProcess(pVGAState->pDrv, uScreenId, pHdr, cbCmd);

            int32_t xRight  = pHdr->x + pHdr->w;
            int32_t yBottom = pHdr->y + pHdr->h;

            /* These are global coords, relative to the primary screen. */

            LOGVBVABUFFER(("cbCmd = %d, x=%d, y=%d, w=%d, h=%d\n",
                           cbCmd, pHdr->x, pHdr->y, pHdr->w, pHdr->h));
            LogRel3(("%s: update command cbCmd = %d, x=%d, y=%d, w=%d, h=%d\n",
                     __FUNCTION__, cbCmd, pHdr->x, pHdr->y, pHdr->w, pHdr->h));

            /* Collect all rects into one. */
            if (fDirtyEmpty)
            {
                /* This is the first rectangle to be added. */
                dirtyRect.xLeft   = pHdr->x;
                dirtyRect.yTop    = pHdr->y;
                dirtyRect.xRight  = xRight;
                dirtyRect.yBottom = yBottom;
                fDirtyEmpty       = false;
            }
            else
            {
                /* Adjust region coordinates. */
                if (dirtyRect.xLeft > pHdr->x)
                {
                    dirtyRect.xLeft = pHdr->x;
                }

                if (dirtyRect.yTop > pHdr->y)
                {
                    dirtyRect.yTop = pHdr->y;
                }

                if (dirtyRect.xRight < xRight)
                {
                    dirtyRect.xRight = xRight;
                }

                if (dirtyRect.yBottom < yBottom)
                {
                    dirtyRect.yBottom = yBottom;
                }
            }
        }

        vbvaReleaseCmd(pVBVAData, pHdr, cbCmd);
    }

    if (fUpdate)
    {
        if (dirtyRect.xRight - dirtyRect.xLeft)
        {
            LogRel3(("%s: sending update screen=%d, x=%d, y=%d, w=%d, h=%d\n",
                     __FUNCTION__, uScreenId, dirtyRect.xLeft,
                     dirtyRect.yTop, dirtyRect.xRight - dirtyRect.xLeft,
                     dirtyRect.yBottom - dirtyRect.yTop));
            pVGAState->pDrv->pfnVBVAUpdateEnd(pVGAState->pDrv, uScreenId, dirtyRect.xLeft, dirtyRect.yTop,
                                              dirtyRect.xRight - dirtyRect.xLeft, dirtyRect.yBottom - dirtyRect.yTop);
        }
        else
        {
            pVGAState->pDrv->pfnVBVAUpdateEnd(pVGAState->pDrv, uScreenId, 0, 0, 0, 0);
        }
    }

    return VINF_SUCCESS;
}

static int vbvaFlush(PVGASTATE pVGAState, VBVACONTEXT *pCtx)
{
    int rc = VINF_SUCCESS;

    unsigned uScreenId;
    for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
    {
        VBVADATA *pVBVAData = &pCtx->aViews[uScreenId].vbva;
        if (pVBVAData->guest.pVBVA)
        {
            rc = vbvaFlushProcess(uScreenId, pVGAState, pVBVAData);
            if (RT_FAILURE(rc))
                break;
        }
    }

    if (RT_FAILURE(rc))
    {
        /* Turn off VBVA processing. */
        LogRel(("VBVA: Disabling (%Rrc)\n", rc));
        pVGAState->fGuestCaps = 0;
        pVGAState->pDrv->pfnVBVAGuestCapabilityUpdate(pVGAState->pDrv, pVGAState->fGuestCaps);
        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            VBVADATA *pVBVAData = &pCtx->aViews[uScreenId].vbva;
            if (pVBVAData->guest.pVBVA)
            {
                vbvaDataCleanup(pVBVAData);
                pVGAState->pDrv->pfnVBVADisable(pVGAState->pDrv, uScreenId);
            }
        }
    }

    return rc;
}

static int vbvaResize(PVGASTATE pVGAState, VBVAVIEW *pView, const VBVAINFOSCREEN *pNewScreen, bool fResetInputMapping)
{
    /* Callers ensure that pNewScreen contains valid data. */

    /* Apply these changes. */
    pView->screen = *pNewScreen;

    uint8_t *pu8VRAM = pVGAState->vram_ptrR3 + pView->view.u32ViewOffset;
    return pVGAState->pDrv->pfnVBVAResize (pVGAState->pDrv, &pView->view, &pView->screen, pu8VRAM, fResetInputMapping);
}

static int vbvaEnable(unsigned uScreenId, PVGASTATE pVGAState, VBVACONTEXT *pCtx,
                      VBVABUFFER RT_UNTRUSTED_VOLATILE_GUEST *pVBVA, uint32_t u32Offset, bool fRestored)
{
    /*
     * Copy into non-volatile memory and validate its content.
     */
    VBVABUFFER VbgaSafe;
    RT_COPY_VOLATILE(VbgaSafe, *pVBVA);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    uint32_t const cbVBVABuffer = RT_UOFFSETOF(VBVABUFFER, au8Data) + VbgaSafe.cbData;
    ASSERT_GUEST_RETURN(   VbgaSafe.cbData <= UINT32_MAX - RT_UOFFSETOF(VBVABUFFER, au8Data)
                        && cbVBVABuffer <= pVGAState->vram_size
                        && u32Offset <= pVGAState->vram_size - cbVBVABuffer,
                        VERR_INVALID_PARAMETER);
    if (!fRestored)
    {
        ASSERT_GUEST_RETURN(VbgaSafe.off32Data        == 0, VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(VbgaSafe.off32Free        == 0, VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(VbgaSafe.indexRecordFirst == 0, VERR_INVALID_PARAMETER);
        ASSERT_GUEST_RETURN(VbgaSafe.indexRecordFree  == 0, VERR_INVALID_PARAMETER);
    }
    ASSERT_GUEST_RETURN(   VbgaSafe.cbPartialWriteThreshold < VbgaSafe.cbData
                        && VbgaSafe.cbPartialWriteThreshold != 0,
                        VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Okay, try do the job.
     */
    int rc;
    if (pVGAState->pDrv->pfnVBVAEnable)
    {
        pVBVA->hostFlags.u32HostEvents      = 0;
        pVBVA->hostFlags.u32SupportedOrders = 0;
        rc = pVGAState->pDrv->pfnVBVAEnable(pVGAState->pDrv, uScreenId, &pVBVA->hostFlags, false);
        if (RT_SUCCESS(rc))
        {
            /* pVBVA->hostFlags has been set up by pfnVBVAEnable. */
            LogFlowFunc(("u32HostEvents=0x%08x  u32SupportedOrders=0x%08x\n",
                         pVBVA->hostFlags.u32HostEvents, pVBVA->hostFlags.u32SupportedOrders));

            VBVADATA *pVBVAData = &pCtx->aViews[uScreenId].vbva;
            pVBVAData->guest.pVBVA             = pVBVA;
            pVBVAData->guest.pu8Data           = &pVBVA->au8Data[0];
            pVBVAData->u32VBVAOffset           = u32Offset;
            pVBVAData->off32Data               = VbgaSafe.off32Data;
            pVBVAData->indexRecordFirst        = VbgaSafe.indexRecordFirst;
            pVBVAData->cbPartialWriteThreshold = VbgaSafe.cbPartialWriteThreshold;
            pVBVAData->cbData                  = VbgaSafe.cbData;

            if (!fRestored)
            {
                /** @todo Actually this function must not touch the partialRecord structure at all,
                 * because initially it is a zero and when VBVA is disabled this should be set to zero.
                 * But I'm not sure that no code depends on zeroing partialRecord here.
                 * So for now (a quick fix for 4.1) just do not do this if the VM was restored,
                 * when partialRecord might be loaded already from the saved state.
                 */
                pVBVAData->partialRecord.pu8 = NULL;
                pVBVAData->partialRecord.cb = 0;
            }

            /* VBVA is working so disable the pause. */
            pCtx->fPaused = false;
        }
    }
    else
        rc = VERR_NOT_SUPPORTED;
    return rc;
}

static int vbvaDisable (unsigned uScreenId, PVGASTATE pVGAState, VBVACONTEXT *pCtx)
{
    /* Process any pending orders and empty the VBVA ring buffer. */
    vbvaFlush (pVGAState, pCtx);

    VBVADATA *pVBVAData = &pCtx->aViews[uScreenId].vbva;
    vbvaDataCleanup(pVBVAData);

    if (uScreenId == 0)
    {
        pVGAState->fGuestCaps = 0;
        pVGAState->pDrv->pfnVBVAGuestCapabilityUpdate(pVGAState->pDrv, pVGAState->fGuestCaps);
    }
    pVGAState->pDrv->pfnVBVADisable(pVGAState->pDrv, uScreenId);
    return VINF_SUCCESS;
}

bool VBVAIsEnabled(PVGASTATE pVGAState)
{
    PHGSMIINSTANCE pHGSMI = pVGAState->pHGSMI;
    if (pHGSMI)
    {
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pHGSMI);
        if (pCtx)
        {
            if (pCtx->cViews)
            {
                VBVAVIEW * pView = &pCtx->aViews[0];
                if (pView->vbva.guest.pVBVA)
                    return true;
            }
        }
    }
    return false;
}

#ifdef DEBUG_sunlover
void dumpMouseShapeInfo(const VBVAMOUSESHAPEINFO *pMouseShapeInfo)
{
    LogFlow(("fSet = %d, fVisible %d, fAlpha %d, @%d,%d %dx%d (%p, %d/%d)\n",
             pMouseShapeInfo->fSet,
             pMouseShapeInfo->fVisible,
             pMouseShapeInfo->fAlpha,
             pMouseShapeInfo->u32HotX,
             pMouseShapeInfo->u32HotY,
             pMouseShapeInfo->u32Width,
             pMouseShapeInfo->u32Height,
             pMouseShapeInfo->pu8Shape,
             pMouseShapeInfo->cbShape,
             pMouseShapeInfo->cbAllocated
             ));
}
#endif

static int vbvaUpdateMousePointerShape(PVGASTATE pVGAState, VBVAMOUSESHAPEINFO *pMouseShapeInfo, bool fShape)
{
    LogFlowFunc(("pVGAState %p, pMouseShapeInfo %p, fShape %d\n",
                  pVGAState, pMouseShapeInfo, fShape));
#ifdef DEBUG_sunlover
    dumpMouseShapeInfo(pMouseShapeInfo);
#endif

    if (pVGAState->pDrv->pfnVBVAMousePointerShape == NULL)
    {
        return VERR_NOT_SUPPORTED;
    }

    int rc;
    if (fShape && pMouseShapeInfo->pu8Shape != NULL)
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        pMouseShapeInfo->fVisible,
                                                        pMouseShapeInfo->fAlpha,
                                                        pMouseShapeInfo->u32HotX,
                                                        pMouseShapeInfo->u32HotY,
                                                        pMouseShapeInfo->u32Width,
                                                        pMouseShapeInfo->u32Height,
                                                        pMouseShapeInfo->pu8Shape);
    }
    else
    {
        rc = pVGAState->pDrv->pfnVBVAMousePointerShape (pVGAState->pDrv,
                                                        pMouseShapeInfo->fVisible,
                                                        false,
                                                        0, 0,
                                                        0, 0,
                                                        NULL);
    }

    return rc;
}

static int vbvaMousePointerShape(PVGASTATE pVGAState, VBVACONTEXT *pCtx,
                                 const VBVAMOUSEPOINTERSHAPE RT_UNTRUSTED_VOLATILE_GUEST *pShape, HGSMISIZE cbShape)
{
    /*
     * Make non-volatile copy of the shape header and validate it.
     */
    VBVAMOUSEPOINTERSHAPE SafeShape;
    RT_COPY_VOLATILE(SafeShape, *pShape);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    LogFlowFunc(("VBVA_MOUSE_POINTER_SHAPE: i32Result 0x%x, fu32Flags 0x%x, hot spot %d,%d, size %dx%d\n",
                 SafeShape.i32Result, SafeShape.fu32Flags, SafeShape.u32HotX, SafeShape.u32HotY, SafeShape.u32Width, SafeShape.u32Height));

    const bool fVisible = RT_BOOL(SafeShape.fu32Flags & VBOX_MOUSE_POINTER_VISIBLE);
    const bool fAlpha =   RT_BOOL(SafeShape.fu32Flags & VBOX_MOUSE_POINTER_ALPHA);
    const bool fShape =   RT_BOOL(SafeShape.fu32Flags & VBOX_MOUSE_POINTER_SHAPE);

    HGSMISIZE cbPointerData = 0;
    if (fShape)
    {
        static const uint32_t s_cxMax = 2048; //used to be: 8192;
        static const uint32_t s_cyMax = 2048; //used to be: 8192;
        ASSERT_GUEST_MSG_RETURN(   SafeShape.u32Width  <= s_cxMax
                                || SafeShape.u32Height <= s_cyMax,
                                ("Too large: %ux%u, max %ux%x\n", SafeShape.u32Width, SafeShape.u32Height, s_cxMax, s_cyMax),
                                VERR_INVALID_PARAMETER);

         cbPointerData = ((((SafeShape.u32Width + 7) / 8) * SafeShape.u32Height + 3) & ~3)
                       + SafeShape.u32Width * 4 * SafeShape.u32Height;

         ASSERT_GUEST_MSG_RETURN(cbPointerData <= cbShape - RT_UOFFSETOF(VBVAMOUSEPOINTERSHAPE, au8Data),
                                 ("Insufficent pointer data: Expected %#x, got %#x\n",
                                  cbPointerData, cbShape - RT_UOFFSETOF(VBVAMOUSEPOINTERSHAPE, au8Data) ),
                                 VERR_INVALID_PARAMETER);
    }
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Do the job.
     */
    /* Save mouse info it will be used to restore mouse pointer after restoring saved state. */
    pCtx->mouseShapeInfo.fSet = true;
    pCtx->mouseShapeInfo.fVisible = fVisible;
    if (fShape)
    {
        /* Data related to shape. */
        pCtx->mouseShapeInfo.u32HotX = SafeShape.u32HotX;
        pCtx->mouseShapeInfo.u32HotY = SafeShape.u32HotY;
        pCtx->mouseShapeInfo.u32Width = SafeShape.u32Width;
        pCtx->mouseShapeInfo.u32Height = SafeShape.u32Height;
        pCtx->mouseShapeInfo.fAlpha = fAlpha;

        /* Reallocate memory buffer if necessary. */
        if (cbPointerData > pCtx->mouseShapeInfo.cbAllocated)
        {
            RTMemFree(pCtx->mouseShapeInfo.pu8Shape);
            pCtx->mouseShapeInfo.pu8Shape = NULL;
            pCtx->mouseShapeInfo.cbShape = 0;

            uint8_t *pu8Shape = (uint8_t *)RTMemAlloc(cbPointerData);
            if (pu8Shape)
            {
                pCtx->mouseShapeInfo.pu8Shape = pu8Shape;
                pCtx->mouseShapeInfo.cbAllocated = cbPointerData;
            }
        }

        /* Copy shape bitmaps. */
        if (pCtx->mouseShapeInfo.pu8Shape)
        {
            RT_BCOPY_VOLATILE(pCtx->mouseShapeInfo.pu8Shape, &pShape->au8Data[0], cbPointerData);
            pCtx->mouseShapeInfo.cbShape = cbPointerData;
        }
    }

    return vbvaUpdateMousePointerShape(pVGAState, &pCtx->mouseShapeInfo, fShape);
}

static uint32_t vbvaViewFromBufferPtr(PHGSMIINSTANCE pIns, const VBVACONTEXT *pCtx,
                                      const void RT_UNTRUSTED_VOLATILE_GUEST *pvBuffer)
{
    /* Check which view contains the buffer. */
    HGSMIOFFSET offBuffer = HGSMIPointerToOffsetHost(pIns, pvBuffer);
    if (offBuffer != HGSMIOFFSET_VOID)
    {
        unsigned uScreenId;
        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            const VBVAINFOVIEW *pView = &pCtx->aViews[uScreenId].view;
            if ((uint32_t)(offBuffer - pView->u32ViewOffset) < pView->u32ViewSize)
                return pView->u32ViewIndex;
        }
    }
    return UINT32_MAX;
}

#ifdef DEBUG_sunlover
static void dumpctx(const VBVACONTEXT *pCtx)
{
    Log(("VBVACONTEXT dump: cViews %d\n", pCtx->cViews));

    uint32_t iView;
    for (iView = 0; iView < pCtx->cViews; iView++)
    {
        const VBVAVIEW *pView = &pCtx->aViews[iView];

        Log(("                  view %d o 0x%x s 0x%x m 0x%x\n",
              pView->view.u32ViewIndex,
              pView->view.u32ViewOffset,
              pView->view.u32ViewSize,
              pView->view.u32MaxScreenSize));

        Log(("                  screen %d @%d,%d s 0x%x l 0x%x %dx%d bpp %d f 0x%x\n",
              pView->screen.u32ViewIndex,
              pView->screen.i32OriginX,
              pView->screen.i32OriginY,
              pView->screen.u32StartOffset,
              pView->screen.u32LineSize,
              pView->screen.u32Width,
              pView->screen.u32Height,
              pView->screen.u16BitsPerPixel,
              pView->screen.u16Flags));

        Log(("                  VBVA o 0x%x p %p\n",
              pView->vbva.u32VBVAOffset,
              pView->vbva.guest.pVBVA));

        Log(("                  PR cb 0x%x p %p\n",
              pView->vbva.partialRecord.cb,
              pView->vbva.partialRecord.pu8));
    }

    dumpMouseShapeInfo(&pCtx->mouseShapeInfo);
}
#endif /* DEBUG_sunlover */

#define VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC   0x12345678
#define VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC 0x9abcdef0

#ifdef VBOX_WITH_VIDEOHWACCEL
static void vbvaVHWAHHCommandReinit(VBOXVHWACMD* pHdr, VBOXVHWACMD_TYPE enmCmd, int32_t iDisplay)
{
    memset(pHdr, 0, VBOXVHWACMD_HEADSIZE());
    pHdr->cRefs = 1;
    pHdr->iDisplay = iDisplay;
    pHdr->rc = VERR_NOT_IMPLEMENTED;
    pHdr->enmCmd = enmCmd;
    pHdr->Flags = VBOXVHWACMD_FLAG_HH_CMD;
}

static VBOXVHWACMD *vbvaVHWAHHCommandCreate(VBOXVHWACMD_TYPE enmCmd, int32_t iDisplay, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD *pHdr = (VBOXVHWACMD *)RTMemAllocZ(cbCmd + VBOXVHWACMD_HEADSIZE());
    Assert(pHdr);
    if (pHdr)
        vbvaVHWAHHCommandReinit(pHdr, enmCmd, iDisplay);

    return pHdr;
}

DECLINLINE(void) vbvaVHWAHHCommandRelease(VBOXVHWACMD *pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    if (!cRefs)
        RTMemFree(pCmd);
}

DECLINLINE(void) vbvaVHWAHHCommandRetain(VBOXVHWACMD *pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

static void vbvaVHWACommandComplete(PVGASTATE pVGAState, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand, bool fAsyncCommand)
{
    if (fAsyncCommand)
    {
        Assert(pCommand->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH);
        vbvaVHWACommandCompleteAsync(&pVGAState->IVBVACallbacks, pCommand);
    }
    else
    {
        Log(("VGA Command <<< Sync rc %d %#p, %d\n", pCommand->rc, pCommand, pCommand->enmCmd));
        pCommand->Flags &= ~VBOXVHWACMD_FLAG_HG_ASYNCH;
    }

}

static void vbvaVHWACommandCompleteAllPending(PVGASTATE pVGAState, int rc)
{
    if (!ASMAtomicUoReadU32(&pVGAState->pendingVhwaCommands.cPending))
        return;

    PDMCritSectEnter(&pVGAState->CritSect, VERR_SEM_BUSY);

    VBOX_VHWA_PENDINGCMD *pIter, *pNext;
    RTListForEachSafe(&pVGAState->pendingVhwaCommands.PendingList, pIter, pNext, VBOX_VHWA_PENDINGCMD, Node)
    {
        pIter->pCommand->rc = rc;
        vbvaVHWACommandComplete(pVGAState, pIter->pCommand, true);

        /* the command is submitted/processed, remove from the pend list */
        RTListNodeRemove(&pIter->Node);
        ASMAtomicDecU32(&pVGAState->pendingVhwaCommands.cPending);
        RTMemFree(pIter);
    }

    PDMCritSectLeave(&pVGAState->CritSect);
}

static void vbvaVHWACommandClearAllPending(PVGASTATE pVGAState)
{
    if (!ASMAtomicUoReadU32(&pVGAState->pendingVhwaCommands.cPending))
        return;

    PDMCritSectEnter(&pVGAState->CritSect, VERR_SEM_BUSY);

    VBOX_VHWA_PENDINGCMD *pIter, *pNext;
    RTListForEachSafe(&pVGAState->pendingVhwaCommands.PendingList, pIter, pNext, VBOX_VHWA_PENDINGCMD, Node)
    {
        RTListNodeRemove(&pIter->Node);
        ASMAtomicDecU32(&pVGAState->pendingVhwaCommands.cPending);
        RTMemFree(pIter);
    }

    PDMCritSectLeave(&pVGAState->CritSect);
}

static void vbvaVHWACommandPend(PVGASTATE pVGAState, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand)
{
    int rc = VERR_BUFFER_OVERFLOW;

    if (ASMAtomicUoReadU32(&pVGAState->pendingVhwaCommands.cPending) < VBOX_VHWA_MAX_PENDING_COMMANDS)
    {
        VBOX_VHWA_PENDINGCMD *pPend = (VBOX_VHWA_PENDINGCMD *)RTMemAlloc(sizeof(*pPend));
        if (pPend)
        {
            pCommand->Flags |= VBOXVHWACMD_FLAG_HG_ASYNCH;
            pPend->pCommand = pCommand;
            PDMCritSectEnter(&pVGAState->CritSect, VERR_SEM_BUSY);
            if (ASMAtomicUoReadU32(&pVGAState->pendingVhwaCommands.cPending) < VBOX_VHWA_MAX_PENDING_COMMANDS)
            {
                RTListAppend(&pVGAState->pendingVhwaCommands.PendingList, &pPend->Node);
                ASMAtomicIncU32(&pVGAState->pendingVhwaCommands.cPending);
                PDMCritSectLeave(&pVGAState->CritSect);
                return;
            }
            PDMCritSectLeave(&pVGAState->CritSect);
            LogRel(("VBVA: Pending command count has reached its threshold.. completing them all.."));
            RTMemFree(pPend);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        LogRel(("VBVA: Pending command count has reached its threshold, completing them all.."));

    vbvaVHWACommandCompleteAllPending(pVGAState, rc);

    pCommand->rc = rc;

    vbvaVHWACommandComplete(pVGAState, pCommand, false);
}

static bool vbvaVHWACommandCanPend(VBOXVHWACMD_TYPE enmCmd)
{
    switch (enmCmd)
    {
        case VBOXVHWACMD_TYPE_HH_CONSTRUCT:
        case VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEBEGIN:
        case VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEEND:
        case VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEPERFORM:
        case VBOXVHWACMD_TYPE_HH_SAVESTATE_LOADPERFORM:
            return false;
        default:
            return true;
    }
}

static int vbvaVHWACommandSavePending(PVGASTATE pVGAState, PSSMHANDLE pSSM)
{
    int rc = SSMR3PutU32(pSSM, pVGAState->pendingVhwaCommands.cPending);
    AssertRCReturn(rc, rc);

    VBOX_VHWA_PENDINGCMD *pIter;
    RTListForEach(&pVGAState->pendingVhwaCommands.PendingList, pIter, VBOX_VHWA_PENDINGCMD, Node)
    {
        AssertContinue((uintptr_t)pIter->pCommand - (uintptr_t)pVGAState->vram_ptrR3 < pVGAState->vram_size);
        rc = SSMR3PutU32(pSSM, (uint32_t)(((uint8_t *)pIter->pCommand) - ((uint8_t *)pVGAState->vram_ptrR3)));
        AssertRCReturn(rc, rc);
    }
    return rc;
}

static int vbvaVHWACommandLoadPending(PVGASTATE pVGAState, PSSMHANDLE pSSM, uint32_t u32Version)
{
    if (u32Version < VGA_SAVEDSTATE_VERSION_WITH_PENDVHWA)
        return VINF_SUCCESS;

    uint32_t u32;
    int rc = SSMR3GetU32(pSSM, &u32);
    AssertRCReturn(rc, rc);
    for (uint32_t i = 0; i < u32; ++i)
    {
        uint32_t off32;
        rc = SSMR3GetU32(pSSM, &off32);
        AssertRCReturn(rc, rc);
        VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand
            = (VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *)((uint8_t volatile *)pVGAState->vram_ptrR3 + off32);
        vbvaVHWACommandPend(pVGAState, pCommand);
    }
    return rc;
}


/** Worker for vbvaVHWACommandSubmit. */
static bool vbvaVHWACommandSubmitInner(PVGASTATE pVGAState, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand, bool *pfPending)
{
    *pfPending = false;

    /*
     * Read the command type and validate it and our driver state.
     */
    VBOXVHWACMD_TYPE enmCmd = pCommand->enmCmd;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    bool fGuestCmd = (uintptr_t)pCommand - (uintptr_t)pVGAState->vram_ptrR3 < pVGAState->vram_size;
    ASSERT_GUEST_LOGREL_MSG_STMT_RETURN(   !fGuestCmd
                                        || (   enmCmd != VBOXVHWACMD_TYPE_HH_CONSTRUCT
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_RESET
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_DISABLE
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_ENABLE
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEBEGIN
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEEND
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEPERFORM
                                            && enmCmd != VBOXVHWACMD_TYPE_HH_SAVESTATE_LOADPERFORM),
                                        ("enmCmd=%d\n", enmCmd),
                                        pCommand->rc = VERR_INVALID_PARAMETER,
                                        true);
    ASSERT_GUEST_STMT_RETURN(pVGAState->pDrv->pfnVHWACommandProcess, pCommand->rc = VERR_INVALID_STATE, true);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Call the driver to process the command.
     */
    Log(("VGA Command >>> %#p, %d\n", pCommand, enmCmd));
    int rc = pVGAState->pDrv->pfnVHWACommandProcess(pVGAState->pDrv, enmCmd, fGuestCmd, pCommand);
    if (rc == VINF_CALLBACK_RETURN)
    {
        Log(("VGA Command --- Going Async %#p, %d\n", pCommand, enmCmd));
        *pfPending = true;
        return true; /* Command will be completed asynchronously by the driver and need not be put in the pending list. */
    }

    if (rc == VERR_INVALID_STATE)
    {
        Log(("VGA Command --- Trying Pend %#p, %d\n", pCommand, enmCmd));
        if (vbvaVHWACommandCanPend(enmCmd))
        {
            Log(("VGA Command --- Can Pend %#p, %d\n", pCommand, enmCmd));
            *pfPending = true;
            return false; /* put on pending list so it can be retried?? */
        }

        Log(("VGA Command --- Can NOT Pend %#p, %d\n", pCommand, enmCmd));
    }
    else
        Log(("VGA Command --- Going Complete Sync rc %d %#p, %d\n", rc, pCommand, enmCmd));

    /* the command was completed, take a special care about it (see caller) */
    pCommand->rc = rc;
    return true;
}


static bool vbvaVHWACommandSubmit(PVGASTATE pVGAState, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCommand, bool fAsyncCommand)
{
    bool fPending = false;
    bool fRet = vbvaVHWACommandSubmitInner(pVGAState, pCommand, &fPending);
    if (!fPending)
        vbvaVHWACommandComplete(pVGAState, pCommand, fAsyncCommand);
    return fRet;
}


/**
 * @returns false if commands are pending, otherwise true.
 */
static bool vbvaVHWACheckPendingCommands(PVGASTATE pVGAState)
{
    if (!ASMAtomicUoReadU32(&pVGAState->pendingVhwaCommands.cPending))
        return true;

    PDMCritSectEnter(&pVGAState->CritSect, VERR_SEM_BUSY);

    VBOX_VHWA_PENDINGCMD *pIter, *pNext;
    RTListForEachSafe(&pVGAState->pendingVhwaCommands.PendingList, pIter, pNext, VBOX_VHWA_PENDINGCMD, Node)
    {
        if (!vbvaVHWACommandSubmit(pVGAState, pIter->pCommand, true))
        {
            PDMCritSectLeave(&pVGAState->CritSect);
            return false; /* the command should be still pending */
        }

        /* the command is submitted/processed, remove from the pend list */
        RTListNodeRemove(&pIter->Node);
        ASMAtomicDecU32(&pVGAState->pendingVhwaCommands.cPending);
        RTMemFree(pIter);
    }

    PDMCritSectLeave(&pVGAState->CritSect);

    return true;
}

void vbvaTimerCb(PVGASTATE pVGAState)
{
    vbvaVHWACheckPendingCommands(pVGAState);
}

static void vbvaVHWAHandleCommand(PVGASTATE pVGAState, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    if (vbvaVHWACheckPendingCommands(pVGAState))
    {
        if (vbvaVHWACommandSubmit(pVGAState, pCmd, false))
            return;
    }

    vbvaVHWACommandPend(pVGAState, pCmd);
}

static DECLCALLBACK(void) vbvaVHWAHHCommandSetEventCallback(void * pContext)
{
    RTSemEventSignal((RTSEMEVENT)pContext);
}

static int vbvaVHWAHHCommandPost(PVGASTATE pVGAState, VBOXVHWACMD *pCmd)
{
    RTSEMEVENT hComplEvent;
    int rc = RTSemEventCreate(&hComplEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /* ensure the cmd is not deleted until we process it */
        vbvaVHWAHHCommandRetain(pCmd);

        VBOXVHWA_HH_CALLBACK_SET(pCmd, vbvaVHWAHHCommandSetEventCallback, (void *)hComplEvent);
        vbvaVHWAHandleCommand(pVGAState, pCmd);

        if ((ASMAtomicReadU32((volatile uint32_t *)&pCmd->Flags) & VBOXVHWACMD_FLAG_HG_ASYNCH) != 0)
            rc = RTSemEventWaitNoResume(hComplEvent, RT_INDEFINITE_WAIT); /** @todo Why the NoResume and event leaking here? */
        /* else: the command is completed */

        AssertRC(rc);
        if (RT_SUCCESS(rc))
            RTSemEventDestroy(hComplEvent);

        vbvaVHWAHHCommandRelease(pCmd);
    }
    return rc;
}

int vbvaVHWAConstruct(PVGASTATE pVGAState)
{
    pVGAState->pendingVhwaCommands.cPending = 0;
    RTListInit(&pVGAState->pendingVhwaCommands.PendingList);

    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(VBOXVHWACMD_TYPE_HH_CONSTRUCT, 0, sizeof(VBOXVHWACMD_HH_CONSTRUCT));
    Assert(pCmd);
    if(pCmd)
    {
        uint32_t iDisplay = 0;
        int rc = VINF_SUCCESS;
        VBOXVHWACMD_HH_CONSTRUCT *pBody = VBOXVHWACMD_BODY_HOST_HEAP(pCmd, VBOXVHWACMD_HH_CONSTRUCT);

        do
        {
            memset(pBody, 0, sizeof(VBOXVHWACMD_HH_CONSTRUCT));

            PPDMDEVINS pDevIns = pVGAState->pDevInsR3;
            PVM pVM = PDMDevHlpGetVM(pDevIns);

            pBody->pVM = pVM;
            pBody->pvVRAM = pVGAState->vram_ptrR3;
            pBody->cbVRAM = pVGAState->vram_size;

            rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
            ASMCompilerBarrier();

            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                rc = pCmd->rc;
                AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
                if(rc == VERR_NOT_IMPLEMENTED)
                {
                    /** @todo set some flag in pVGAState indicating VHWA is not supported */
                    /* VERR_NOT_IMPLEMENTED is not a failure, we just do not support it */
                    rc = VINF_SUCCESS;
                }

                if (!RT_SUCCESS(rc))
                    break;
            }
            else
                break;

            ++iDisplay;
            if (iDisplay >= pVGAState->cMonitors)
                break;
            vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_CONSTRUCT, (int32_t)iDisplay);
        } while (true);

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vbvaVHWAReset(PVGASTATE pVGAState)
{
    vbvaVHWACommandClearAllPending(pVGAState);

    /* ensure we have all pending cmds processed and h->g cmds disabled */
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(VBOXVHWACMD_TYPE_HH_RESET, 0, 0);
    Assert(pCmd);
    if (pCmd)
    {
        int rc = VINF_SUCCESS;
        uint32_t iDisplay = 0;

        do
        {
            rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
            AssertRC(rc);
            if(RT_SUCCESS(rc))
            {
                rc = pCmd->rc;
                AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
                if (rc == VERR_NOT_IMPLEMENTED)
                    rc = VINF_SUCCESS;
            }

            if (!RT_SUCCESS(rc))
                break;

            ++iDisplay;
            if (iDisplay >= pVGAState->cMonitors)
                break;
            vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_RESET, (int32_t)iDisplay);

        } while (true);

        vbvaVHWAHHCommandRelease(pCmd);

        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

typedef DECLCALLBACK(bool) FNVBOXVHWAHHCMDPRECB(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext);
typedef FNVBOXVHWAHHCMDPRECB *PFNVBOXVHWAHHCMDPRECB;

typedef DECLCALLBACK(bool) FNVBOXVHWAHHCMDPOSTCB(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, int rc, void *pvContext);
typedef FNVBOXVHWAHHCMDPOSTCB *PFNVBOXVHWAHHCMDPOSTCB;

int vbvaVHWAHHPost(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, PFNVBOXVHWAHHCMDPRECB pfnPre, PFNVBOXVHWAHHCMDPOSTCB pfnPost, void *pvContext)
{
    const VBOXVHWACMD_TYPE enmType = pCmd->enmCmd;
    int rc = VINF_SUCCESS;
    uint32_t iDisplay = 0;

    do
    {
        if (!pfnPre || pfnPre(pVGAState, pCmd, iDisplay, pvContext))
        {
            rc = vbvaVHWAHHCommandPost(pVGAState, pCmd);
            AssertRC(rc);
            if (pfnPost)
            {
                if (!pfnPost(pVGAState, pCmd, iDisplay, rc, pvContext))
                {
                    rc = VINF_SUCCESS;
                    break;
                }
                rc = VINF_SUCCESS;
            }
            else if(RT_SUCCESS(rc))
            {
                rc = pCmd->rc;
                AssertMsg(RT_SUCCESS(rc) || rc == VERR_NOT_IMPLEMENTED, ("%Rrc\n", rc));
                if(rc == VERR_NOT_IMPLEMENTED)
                {
                    rc = VINF_SUCCESS;
                }
            }

            if (!RT_SUCCESS(rc))
                break;
        }

        ++iDisplay;
        if (iDisplay >= pVGAState->cMonitors)
            break;
        vbvaVHWAHHCommandReinit(pCmd, enmType, (int32_t)iDisplay);
    } while (true);

    return rc;
}

/** @todo call this also on reset? */
int vbvaVHWAEnable (PVGASTATE pVGAState, bool bEnable)
{
    const VBOXVHWACMD_TYPE enmType = bEnable ? VBOXVHWACMD_TYPE_HH_ENABLE : VBOXVHWACMD_TYPE_HH_DISABLE;
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(enmType, 0, 0);
    Assert(pCmd);
    if(pCmd)
    {
        int rc = vbvaVHWAHHPost (pVGAState, pCmd, NULL, NULL, NULL);
        vbvaVHWAHHCommandRelease(pCmd);
        return rc;
    }
    return VERR_OUT_OF_RESOURCES;
}

int vboxVBVASaveStatePrep(PPDMDEVINS pDevIns)
{
    /* ensure we have no pending commands */
    return vbvaVHWAEnable(PDMINS_2_DATA(pDevIns, PVGASTATE), false);
}

int vboxVBVASaveStateDone(PPDMDEVINS pDevIns)
{
    /* ensure we have no pending commands */
    return vbvaVHWAEnable(PDMINS_2_DATA(pDevIns, PVGASTATE), true);
}

/**
 * @interface_method_impl{PDMIDISPLAYVBVACALLBACKS,pfnVHWACommandCompleteAsync}
 */
DECLCALLBACK(int) vbvaVHWACommandCompleteAsync(PPDMIDISPLAYVBVACALLBACKS pInterface, VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *pCmd)
{
    PVGASTATE pVGAState = PPDMIDISPLAYVBVACALLBACKS_2_PVGASTATE(pInterface);
    int rc;
    Log(("VGA Command <<< Async rc %d %#p, %d\n", pCmd->rc, pCmd, pCmd->enmCmd));

    if ((uintptr_t)pCmd - (uintptr_t)pVGAState->vram_ptrR3 < pVGAState->vram_size)
    {
        PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
        Assert(!(pCmd->Flags & VBOXVHWACMD_FLAG_HH_CMD));
        Assert(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH);
#ifdef VBOX_WITH_WDDM
        if (pVGAState->fGuestCaps & VBVACAPS_COMPLETEGCMD_BY_IOREAD)
        {
            rc = HGSMICompleteGuestCommand(pIns, pCmd, !!(pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ));
            AssertRC(rc);
        }
        else
#endif
        {
            VBVAHOSTCMD RT_UNTRUSTED_VOLATILE_GUEST *pHostCmd = NULL; /* Shut up MSC. */
            if (pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_EVENT)
            {
                rc = HGSMIHostCommandAlloc(pIns,
                                           (void RT_UNTRUSTED_VOLATILE_GUEST **)&pHostCmd,
                                           VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDEVENT)),
                                           HGSMI_CH_VBVA,
                                           VBVAHG_EVENT);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    memset((void *)pHostCmd, 0 , VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDEVENT)));
                    pHostCmd->iDstID = pCmd->iDisplay;
                    pHostCmd->customOpCode = 0;
                    VBVAHOSTCMDEVENT RT_UNTRUSTED_VOLATILE_GUEST *pBody = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDEVENT);
                    pBody->pEvent = pCmd->GuestVBVAReserved1;
                }
            }
            else
            {
                HGSMIOFFSET offCmd = HGSMIPointerToOffsetHost(pIns, pCmd);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    rc = HGSMIHostCommandAlloc(pIns,
                                               (void RT_UNTRUSTED_VOLATILE_GUEST **)&pHostCmd,
                                               VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDVHWACMDCOMPLETE)),
                                               HGSMI_CH_VBVA,
                                               VBVAHG_DISPLAY_CUSTOM);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        memset((void *)pHostCmd, 0 , VBVAHOSTCMD_SIZE(sizeof(VBVAHOSTCMDVHWACMDCOMPLETE)));
                        pHostCmd->iDstID = pCmd->iDisplay;
                        pHostCmd->customOpCode = VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE;
                        VBVAHOSTCMDVHWACMDCOMPLETE RT_UNTRUSTED_VOLATILE_GUEST *pBody
                            = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDVHWACMDCOMPLETE);
                        pBody->offCmd = offCmd;
                    }
                }
                else
                    rc = VERR_INVALID_PARAMETER;
            }

            if (RT_SUCCESS(rc))
            {
                rc = HGSMIHostCommandSubmitAndFreeAsynch(pIns, pHostCmd, RT_BOOL(pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ));
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                    return rc;

                HGSMIHostCommandFree (pIns, pHostCmd);
            }
        }
    }
    else
    {
        Assert(pCmd->Flags & VBOXVHWACMD_FLAG_HH_CMD);
        PFNVBOXVHWA_HH_CALLBACK pfn = VBOXVHWA_HH_CALLBACK_GET(pCmd);
        if (pfn)
            pfn(VBOXVHWA_HH_CALLBACK_GET_ARG(pCmd));
        rc = VINF_SUCCESS;
    }
    return rc;
}

typedef struct VBOXVBVASAVEDSTATECBDATA
{
    PSSMHANDLE pSSM;
    int rc;
    bool ab2DOn[VBOX_VIDEO_MAX_SCREENS];
} VBOXVBVASAVEDSTATECBDATA, *PVBOXVBVASAVEDSTATECBDATA;

static DECLCALLBACK(bool) vboxVBVASaveStateBeginPostCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay,
                                                       int rc, void *pvContext)
{
    RT_NOREF(pVGAState, pCmd);
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    Assert(RT_SUCCESS(pCmd->rc) || pCmd->rc == VERR_NOT_IMPLEMENTED);
    if (RT_SUCCESS(pCmd->rc))
    {
        pData->ab2DOn[iDisplay] = true;
    }
    else if (pCmd->rc != VERR_NOT_IMPLEMENTED)
    {
        pData->rc = pCmd->rc;
        return false;
    }

    return true;
}

static DECLCALLBACK(bool)
vboxVBVASaveStatePerformPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    RT_NOREF(pVGAState, pCmd);
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    int rc;

    if (pData->ab2DOn[iDisplay])
    {
        rc = SSMR3PutU32 (pData->pSSM, VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC); AssertRC(rc);
        if (RT_FAILURE(rc))
        {
            pData->rc = rc;
            return false;
        }
        return true;
    }

    rc = SSMR3PutU32 (pData->pSSM, VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC); AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    return false;
}

static DECLCALLBACK(bool) vboxVBVASaveStateEndPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    RT_NOREF(pVGAState, pCmd);
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (pData->ab2DOn[iDisplay])
    {
        return true;
    }

    return false;
}

static DECLCALLBACK(bool)
vboxVBVALoadStatePerformPostCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, int rc, void *pvContext)
{
    RT_NOREF(pVGAState, pCmd);
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    Assert(RT_SUCCESS(pCmd->rc) || pCmd->rc == VERR_NOT_IMPLEMENTED);
    if (pCmd->rc == VERR_NOT_IMPLEMENTED)
    {
        pData->rc = SSMR3SkipToEndOfUnit(pData->pSSM);
        AssertRC(pData->rc);
        return false;
    }
    if (RT_FAILURE(pCmd->rc))
    {
        pData->rc = pCmd->rc;
        return false;
    }

    return true;
}

static DECLCALLBACK(bool)
vboxVBVALoadStatePerformPreCb(PVGASTATE pVGAState, VBOXVHWACMD *pCmd, uint32_t iDisplay, void *pvContext)
{
    RT_NOREF(pVGAState, pCmd);
    PVBOXVBVASAVEDSTATECBDATA pData = (PVBOXVBVASAVEDSTATECBDATA)pvContext;
    if (RT_FAILURE(pData->rc))
        return false;

    Assert(iDisplay < RT_ELEMENTS(pData->ab2DOn));
    if (iDisplay >= RT_ELEMENTS(pData->ab2DOn))
    {
        pData->rc = VERR_INVALID_PARAMETER;
        return false;
    }

    int rc;
    uint32_t u32;
    rc = SSMR3GetU32(pData->pSSM, &u32); AssertRC(rc);
    if (RT_FAILURE(rc))
    {
        pData->rc = rc;
        return false;
    }

    switch (u32)
    {
        case VBOXVBVASAVEDSTATE_VHWAAVAILABLE_MAGIC:
            pData->ab2DOn[iDisplay] = true;
            return true;
        case VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC:
            pData->ab2DOn[iDisplay] = false;
            return false;
        default:
            pData->rc = VERR_INVALID_STATE;
            return false;
    }
}
#endif /* #ifdef VBOX_WITH_VIDEOHWACCEL */

int vboxVBVASaveDevStateExec (PVGASTATE pVGAState, PSSMHANDLE pSSM)
{
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    int rc = HGSMIHostSaveStateExec (pIns, pSSM);
    if (RT_SUCCESS(rc))
    {
        VGA_SAVED_STATE_PUT_MARKER(pSSM, 2);

        /* Save VBVACONTEXT. */
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

        if (!pCtx)
        {
            AssertFailed();

            /* Still write a valid value to the SSM. */
            rc = SSMR3PutU32 (pSSM, 0);
            AssertRCReturn(rc, rc);
        }
        else
        {
#ifdef DEBUG_sunlover
            dumpctx(pCtx);
#endif

            rc = SSMR3PutU32 (pSSM, pCtx->cViews);
            AssertRCReturn(rc, rc);

            uint32_t iView;
            for (iView = 0; iView < pCtx->cViews; iView++)
            {
                VBVAVIEW *pView = &pCtx->aViews[iView];

                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32ViewSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->view.u32MaxScreenSize);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->screen.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutS32 (pSSM, pView->screen.i32OriginX);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutS32 (pSSM, pView->screen.i32OriginY);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32StartOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32LineSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU32 (pSSM, pView->screen.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU16 (pSSM, pView->screen.u16BitsPerPixel);
                AssertRCReturn(rc, rc);
                rc = SSMR3PutU16 (pSSM, pView->screen.u16Flags);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->vbva.guest.pVBVA? pView->vbva.u32VBVAOffset: HGSMIOFFSET_VOID);
                AssertRCReturn(rc, rc);

                rc = SSMR3PutU32 (pSSM, pView->vbva.partialRecord.cb);
                AssertRCReturn(rc, rc);

                if (pView->vbva.partialRecord.cb > 0)
                {
                    rc = SSMR3PutMem (pSSM, pView->vbva.partialRecord.pu8, pView->vbva.partialRecord.cb);
                    AssertRCReturn(rc, rc);
                }
            }

            /* Save mouse pointer shape information. */
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fSet);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fVisible);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutBool (pSSM, pCtx->mouseShapeInfo.fAlpha);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32HotX);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32HotY);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32Width);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.u32Height);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pCtx->mouseShapeInfo.cbShape);
            AssertRCReturn(rc, rc);
            if (pCtx->mouseShapeInfo.cbShape)
            {
                rc = SSMR3PutMem (pSSM, pCtx->mouseShapeInfo.pu8Shape, pCtx->mouseShapeInfo.cbShape);
                AssertRCReturn(rc, rc);
            }

#ifdef VBOX_WITH_WDDM
            /* Size of some additional data. For future extensions. */
            rc = SSMR3PutU32 (pSSM, 4);
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, pVGAState->fGuestCaps);
            AssertRCReturn(rc, rc);
#else
            /* Size of some additional data. For future extensions. */
            rc = SSMR3PutU32 (pSSM, 0);
            AssertRCReturn(rc, rc);
#endif
            rc = SSMR3PutU32 (pSSM, RT_ELEMENTS(pCtx->aModeHints));
            AssertRCReturn(rc, rc);
            rc = SSMR3PutU32 (pSSM, sizeof(VBVAMODEHINT));
            AssertRCReturn(rc, rc);
            for (unsigned i = 0; i < RT_ELEMENTS(pCtx->aModeHints); ++i)
            {
                rc = SSMR3PutMem (pSSM, &pCtx->aModeHints[i],
                                  sizeof(VBVAMODEHINT));
                AssertRCReturn(rc, rc);
            }
        }
    }

    return rc;
}

int vboxVBVASaveStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    int rc;
#ifdef VBOX_WITH_VIDEOHWACCEL
    VBOXVBVASAVEDSTATECBDATA VhwaData = {0};
    VhwaData.pSSM = pSSM;
    uint32_t cbCmd = sizeof (VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM); /* maximum cmd size */
    VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEBEGIN, 0, cbCmd);
    Assert(pCmd);
    if(pCmd)
    {
        vbvaVHWAHHPost (pVGAState, pCmd, NULL, vboxVBVASaveStateBeginPostCb, &VhwaData);
        rc = VhwaData.rc;
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
#endif
            rc = vboxVBVASaveDevStateExec (pVGAState, pSSM);
            AssertRC(rc);
#ifdef VBOX_WITH_VIDEOHWACCEL
            if (RT_SUCCESS(rc))
            {
                vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEPERFORM, 0);
                VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM *pSave = VBOXVHWACMD_BODY_HOST_HEAP(pCmd, VBOXVHWACMD_HH_SAVESTATE_SAVEPERFORM);
                pSave->pSSM = pSSM;
                vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVASaveStatePerformPreCb, NULL, &VhwaData);
                rc = VhwaData.rc;
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    rc = vbvaVHWACommandSavePending(pVGAState, pSSM);
                    AssertRCReturn(rc, rc);

                    vbvaVHWAHHCommandReinit(pCmd, VBOXVHWACMD_TYPE_HH_SAVESTATE_SAVEEND, 0);
                    vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVASaveStateEndPreCb, NULL, &VhwaData);
                    rc = VhwaData.rc;
                    AssertRC(rc);
                }
            }
        }

        vbvaVHWAHHCommandRelease(pCmd);
    }
    else
        rc = VERR_OUT_OF_RESOURCES;
#else
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < pVGAState->cMonitors; ++i)
        {
            rc = SSMR3PutU32 (pSSM, VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC);
            AssertRCReturn(rc, rc);
        }
    }

    /* no pending commands */
    SSMR3PutU32(pSSM, 0);
#endif
    return rc;
}

int vboxVBVALoadStateExec (PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion)
{
    if (uVersion < VGA_SAVEDSTATE_VERSION_HGSMI)
    {
        /* Nothing was saved. */
        return VINF_SUCCESS;
    }

    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    int rc = HGSMIHostLoadStateExec (pIns, pSSM, uVersion);
    if (RT_SUCCESS(rc))
    {
        VGA_SAVED_STATE_GET_MARKER_RETURN_ON_MISMATCH(pSSM, uVersion, 2);

        /* Load VBVACONTEXT. */
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

        if (!pCtx)
        {
            /* This should not happen. */
            AssertFailed();
            rc = VERR_INVALID_PARAMETER;
        }
        else
        {
            uint32_t cViews = 0;
            rc = SSMR3GetU32 (pSSM, &cViews);
            AssertRCReturn(rc, rc);

            uint32_t iView;
            for (iView = 0; iView < cViews; iView++)
            {
                VBVAVIEW *pView = &pCtx->aViews[iView];

                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32ViewSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->view.u32MaxScreenSize);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->screen.u32ViewIndex);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetS32 (pSSM, &pView->screen.i32OriginX);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetS32 (pSSM, &pView->screen.i32OriginY);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32StartOffset);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32LineSize);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pView->screen.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU16 (pSSM, &pView->screen.u16BitsPerPixel);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU16 (pSSM, &pView->screen.u16Flags);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->vbva.u32VBVAOffset);
                AssertRCReturn(rc, rc);

                rc = SSMR3GetU32 (pSSM, &pView->vbva.partialRecord.cb);
                AssertRCReturn(rc, rc);

                if (pView->vbva.partialRecord.cb == 0)
                {
                    pView->vbva.partialRecord.pu8 = NULL;
                }
                else
                {
                    Assert(pView->vbva.partialRecord.pu8 == NULL); /* Should be it. */

                    uint8_t *pu8 = (uint8_t *)RTMemAlloc(pView->vbva.partialRecord.cb);

                    if (!pu8)
                    {
                        return VERR_NO_MEMORY;
                    }

                    pView->vbva.partialRecord.pu8 = pu8;

                    rc = SSMR3GetMem (pSSM, pView->vbva.partialRecord.pu8, pView->vbva.partialRecord.cb);
                    AssertRCReturn(rc, rc);
                }

                if (pView->vbva.u32VBVAOffset == HGSMIOFFSET_VOID)
                {
                    pView->vbva.guest.pVBVA = NULL;
                }
                else
                {
                    pView->vbva.guest.pVBVA = (VBVABUFFER *)HGSMIOffsetToPointerHost(pIns, pView->vbva.u32VBVAOffset);
                }
            }

            if (uVersion > VGA_SAVEDSTATE_VERSION_WITH_CONFIG)
            {
                /* Read mouse pointer shape information. */
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fSet);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fVisible);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetBool (pSSM, &pCtx->mouseShapeInfo.fAlpha);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32HotX);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32HotY);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32Width);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.u32Height);
                AssertRCReturn(rc, rc);
                rc = SSMR3GetU32 (pSSM, &pCtx->mouseShapeInfo.cbShape);
                AssertRCReturn(rc, rc);
                if (pCtx->mouseShapeInfo.cbShape)
                {
                    pCtx->mouseShapeInfo.pu8Shape = (uint8_t *)RTMemAlloc(pCtx->mouseShapeInfo.cbShape);
                    if (pCtx->mouseShapeInfo.pu8Shape == NULL)
                    {
                        return VERR_NO_MEMORY;
                    }
                    pCtx->mouseShapeInfo.cbAllocated = pCtx->mouseShapeInfo.cbShape;
                    rc = SSMR3GetMem (pSSM, pCtx->mouseShapeInfo.pu8Shape, pCtx->mouseShapeInfo.cbShape);
                    AssertRCReturn(rc, rc);
                }
                else
                {
                    pCtx->mouseShapeInfo.pu8Shape = NULL;
                }

                /* Size of some additional data. For future extensions. */
                uint32_t cbExtra = 0;
                rc = SSMR3GetU32 (pSSM, &cbExtra);
                AssertRCReturn(rc, rc);
#ifdef VBOX_WITH_WDDM
                if (cbExtra >= 4)
                {
                    rc = SSMR3GetU32 (pSSM, &pVGAState->fGuestCaps);
                    AssertRCReturn(rc, rc);
                    pVGAState->pDrv->pfnVBVAGuestCapabilityUpdate(pVGAState->pDrv, pVGAState->fGuestCaps);
                    cbExtra -= 4;
                }
#endif
                if (cbExtra > 0)
                {
                    rc = SSMR3Skip(pSSM, cbExtra);
                    AssertRCReturn(rc, rc);
                }

                if (uVersion >= VGA_SAVEDSTATE_VERSION_MODE_HINTS)
                {
                    uint32_t cModeHints, cbModeHints;
                    rc = SSMR3GetU32 (pSSM, &cModeHints);
                    AssertRCReturn(rc, rc);
                    rc = SSMR3GetU32 (pSSM, &cbModeHints);
                    AssertRCReturn(rc, rc);
                    memset(&pCtx->aModeHints, ~0, sizeof(pCtx->aModeHints));
                    unsigned iHint;
                    for (iHint = 0; iHint < cModeHints; ++iHint)
                    {
                        if (   cbModeHints <= sizeof(VBVAMODEHINT)
                            && iHint < RT_ELEMENTS(pCtx->aModeHints))
                            rc = SSMR3GetMem(pSSM, &pCtx->aModeHints[iHint],
                                             cbModeHints);
                        else
                            rc = SSMR3Skip(pSSM, cbModeHints);
                        AssertRCReturn(rc, rc);
                    }
                }
            }

            pCtx->cViews = iView;
            LogFlowFunc(("%d views loaded\n", pCtx->cViews));

            if (uVersion > VGA_SAVEDSTATE_VERSION_WDDM)
            {
                bool fLoadCommands;

                if (uVersion < VGA_SAVEDSTATE_VERSION_FIXED_PENDVHWA)
                {
                    const char *pcszOsArch = SSMR3HandleHostOSAndArch(pSSM);
                    Assert(pcszOsArch);
                    fLoadCommands = !pcszOsArch || RTStrNCmp(pcszOsArch, RT_STR_TUPLE("solaris"));
                }
                else
                    fLoadCommands = true;

#ifdef VBOX_WITH_VIDEOHWACCEL
                uint32_t cbCmd = sizeof (VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM); /* maximum cmd size */
                VBOXVHWACMD *pCmd = vbvaVHWAHHCommandCreate(VBOXVHWACMD_TYPE_HH_SAVESTATE_LOADPERFORM, 0, cbCmd);
                Assert(pCmd);
                if(pCmd)
                {
                    VBOXVBVASAVEDSTATECBDATA VhwaData = {0};
                    VhwaData.pSSM = pSSM;
                    VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM *pLoad = VBOXVHWACMD_BODY_HOST_HEAP(pCmd, VBOXVHWACMD_HH_SAVESTATE_LOADPERFORM);
                    pLoad->pSSM = pSSM;
                    vbvaVHWAHHPost (pVGAState, pCmd, vboxVBVALoadStatePerformPreCb, vboxVBVALoadStatePerformPostCb, &VhwaData);
                    rc = VhwaData.rc;
                    vbvaVHWAHHCommandRelease(pCmd);
                    AssertRCReturn(rc, rc);

                    if (fLoadCommands)
                    {
                        rc = vbvaVHWACommandLoadPending(pVGAState, pSSM, uVersion);
                        AssertRCReturn(rc, rc);
                    }
                }
                else
                {
                    rc = VERR_OUT_OF_RESOURCES;
                }
#else
                uint32_t u32;

                for (uint32_t i = 0; i < pVGAState->cMonitors; ++i)
                {
                    rc = SSMR3GetU32(pSSM, &u32);
                    AssertRCReturn(rc, rc);

                    if (u32 != VBOXVBVASAVEDSTATE_VHWAUNAVAILABLE_MAGIC)
                    {
                        LogRel(("VBVA: 2D data while 2D is not supported\n"));
                        return VERR_NOT_SUPPORTED;
                    }
                }

                if (fLoadCommands)
                {
                    rc = SSMR3GetU32(pSSM, &u32);
                    AssertRCReturn(rc, rc);

                    if (u32)
                    {
                        LogRel(("VBVA: 2D pending command while 2D is not supported\n"));
                        return VERR_NOT_SUPPORTED;
                    }
                }
#endif
            }

#ifdef DEBUG_sunlover
            dumpctx(pCtx);
#endif
        }
    }

    return rc;
}

int vboxVBVALoadStateDone(PPDMDEVINS pDevIns)
{
    PVGASTATE pVGAState = PDMINS_2_DATA(pDevIns, PVGASTATE);
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
    if (pCtx)
    {
        uint32_t iView;
        for (iView = 0; iView < pCtx->cViews; iView++)
        {
            VBVAVIEW *pView = &pCtx->aViews[iView];
            if (pView->vbva.guest.pVBVA)
            {
#ifdef VBOX_WITH_CRHGSMI
                Assert(!vboxCmdVBVAIsEnabled(pVGAState));
#endif
                int rc = vbvaEnable(iView, pVGAState, pCtx, pView->vbva.guest.pVBVA, pView->vbva.u32VBVAOffset, true /* fRestored */);
                if (RT_SUCCESS(rc))
                    vbvaResize(pVGAState, pView, &pView->screen, false);
                else
                    LogRel(("VBVA: can not restore: %Rrc\n", rc));
            }
        }

        if (pCtx->mouseShapeInfo.fSet)
            vbvaUpdateMousePointerShape(pVGAState, &pCtx->mouseShapeInfo, true);
    }

    return VINF_SUCCESS;
}

void VBVARaiseIrq (PVGASTATE pVGAState, uint32_t fFlags)
{
    PPDMDEVINS pDevIns = pVGAState->pDevInsR3;

    PDMCritSectEnter(&pVGAState->CritSectIRQ, VERR_SEM_BUSY);

    const uint32_t fu32CurrentGuestFlags = HGSMIGetHostGuestFlags(pVGAState->pHGSMI);
    if ((fu32CurrentGuestFlags & HGSMIHOSTFLAGS_IRQ) == 0)
    {
        /* No IRQ set yet. */
        Assert(pVGAState->fu32PendingGuestFlags == 0);

        HGSMISetHostGuestFlags(pVGAState->pHGSMI, HGSMIHOSTFLAGS_IRQ | fFlags);

        /* If VM is not running, the IRQ will be set in VBVAOnResume. */
        const VMSTATE enmVMState = PDMDevHlpVMState(pDevIns);
        if (   enmVMState == VMSTATE_RUNNING
            || enmVMState == VMSTATE_RUNNING_LS)
            PDMDevHlpPCISetIrqNoWait(pDevIns, 0, PDM_IRQ_LEVEL_HIGH);
    }
    else
    {
        /* IRQ already set, remember the new flags. */
        pVGAState->fu32PendingGuestFlags |= HGSMIHOSTFLAGS_IRQ | fFlags;
    }

    PDMCritSectLeave(&pVGAState->CritSectIRQ);
}

void VBVAOnResume(PVGASTATE pThis)
{
    PPDMDEVINS pDevIns = pThis->pDevInsR3;

    PDMCritSectEnter(&pThis->CritSectIRQ, VERR_SEM_BUSY);

    if (HGSMIGetHostGuestFlags(pThis->pHGSMI) & HGSMIHOSTFLAGS_IRQ)
        PDMDevHlpPCISetIrqNoWait(pDevIns, 0, PDM_IRQ_LEVEL_HIGH);

    PDMCritSectLeave(&pThis->CritSectIRQ);
}

static int vbvaHandleQueryConf32(PVGASTATE pVGAState, VBVACONF32 RT_UNTRUSTED_VOLATILE_GUEST *pConf32)
{
    uint32_t const idxQuery = pConf32->u32Index;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    LogFlowFunc(("VBVA_QUERY_CONF32: u32Index %d, u32Value 0x%x\n", idxQuery, pConf32->u32Value));

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
    uint32_t     uValue;
    if (idxQuery == VBOX_VBVA_CONF32_MONITOR_COUNT)
        uValue = pCtx->cViews;
    else if (idxQuery == VBOX_VBVA_CONF32_HOST_HEAP_SIZE)
        uValue = _64K; /** @todo a value calculated from the vram size */
    else if (   idxQuery == VBOX_VBVA_CONF32_MODE_HINT_REPORTING
             || idxQuery == VBOX_VBVA_CONF32_GUEST_CURSOR_REPORTING)
        uValue = VINF_SUCCESS;
    else if (idxQuery == VBOX_VBVA_CONF32_CURSOR_CAPABILITIES)
        uValue = pVGAState->fHostCursorCapabilities;
    else if (idxQuery == VBOX_VBVA_CONF32_SCREEN_FLAGS)
        uValue = VBVA_SCREEN_F_ACTIVE
               | VBVA_SCREEN_F_DISABLED
               | VBVA_SCREEN_F_BLANK
               | VBVA_SCREEN_F_BLANK2;
    else if (idxQuery == VBOX_VBVA_CONF32_MAX_RECORD_SIZE)
        uValue = VBVA_MAX_RECORD_SIZE;
    else
        ASSERT_GUEST_MSG_FAILED_RETURN(("Invalid index %#x\n", idxQuery), VERR_INVALID_PARAMETER);

    pConf32->u32Value = uValue;
    return VINF_SUCCESS;
}

static int vbvaHandleSetConf32(VBVACONF32 RT_UNTRUSTED_VOLATILE_GUEST *pConf32)
{
    uint32_t const idxQuery = pConf32->u32Index;
    uint32_t const uValue   = pConf32->u32Value;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    LogFlowFunc(("VBVA_SET_CONF32: u32Index %d, u32Value 0x%x\n", idxQuery, uValue));

    if (idxQuery == VBOX_VBVA_CONF32_MONITOR_COUNT)
    { /* do nothing. this is a const. */ }
    else if (idxQuery == VBOX_VBVA_CONF32_HOST_HEAP_SIZE)
    { /* do nothing. this is a const. */ }
    else
        ASSERT_GUEST_MSG_FAILED_RETURN(("Invalid index %#x (value=%u)\n", idxQuery, uValue), VERR_INVALID_PARAMETER);

    RT_NOREF_PV(uValue);
    return VINF_SUCCESS;
}

static int vbvaHandleInfoHeap(PVGASTATE pVGAState, const VBVAINFOHEAP RT_UNTRUSTED_VOLATILE_GUEST *pInfoHeap)
{
    uint32_t const offHeap = pInfoHeap->u32HeapOffset;
    uint32_t const cbHeap  = pInfoHeap->u32HeapSize;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    LogFlowFunc(("VBVA_INFO_HEAP: offset 0x%x, size 0x%x\n", offHeap, cbHeap));

    return HGSMIHostHeapSetup(pVGAState->pHGSMI, offHeap, cbHeap);
}

int VBVAInfoView(PVGASTATE pVGAState, const VBVAINFOVIEW RT_UNTRUSTED_VOLATILE_GUEST *pView)
{
    VBVAINFOVIEW view;
    RT_COPY_VOLATILE(view, *pView);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    LogFlowFunc(("VBVA_INFO_VIEW: u32ViewIndex %d, u32ViewOffset 0x%x, u32ViewSize 0x%x, u32MaxScreenSize 0x%x\n",
                 view.u32ViewIndex, view.u32ViewOffset, view.u32ViewSize, view.u32MaxScreenSize));

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
    ASSERT_GUEST_LOGREL_MSG_RETURN(   view.u32ViewIndex     < pCtx->cViews
                                   && view.u32ViewOffset    <= pVGAState->vram_size
                                   && view.u32ViewSize      <= pVGAState->vram_size
                                   && view.u32ViewOffset    <= pVGAState->vram_size - view.u32ViewSize
                                   && view.u32MaxScreenSize <= view.u32ViewSize,
                                   ("index %d(%d), offset 0x%x, size 0x%x, max 0x%x, vram size 0x%x\n",
                                    view.u32ViewIndex, pCtx->cViews, view.u32ViewOffset, view.u32ViewSize,
                                    view.u32MaxScreenSize, pVGAState->vram_size),
                                   VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    pCtx->aViews[view.u32ViewIndex].view = view;
    return VINF_SUCCESS;
}

int VBVAInfoScreen(PVGASTATE pVGAState, const VBVAINFOSCREEN RT_UNTRUSTED_VOLATILE_GUEST *pScreen)
{
    /*
     * Copy input into non-volatile buffer.
     */
    VBVAINFOSCREEN screen;
    RT_COPY_VOLATILE(screen, *pScreen);
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
    LogRel(("VBVA: InfoScreen: [%d] @%d,%d %dx%d, line 0x%x, BPP %d, flags 0x%x\n",
            screen.u32ViewIndex, screen.i32OriginX, screen.i32OriginY,
            screen.u32Width, screen.u32Height,
            screen.u32LineSize, screen.u16BitsPerPixel, screen.u16Flags));

    /*
     * Validate input.
     */
    /* Allow screen.u16BitsPerPixel == 0 because legacy guest code used it for screen blanking. */
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
    ASSERT_GUEST_LOGREL_MSG_RETURN(screen.u32ViewIndex <  pCtx->cViews,
                                   ("Screen index %#x is out of bound (cViews=%#x)\n", screen.u32ViewIndex, pCtx->cViews),
                                    VERR_INVALID_PARAMETER);
    ASSERT_GUEST_LOGREL_MSG_RETURN(   screen.u16BitsPerPixel <= 32
                                   && screen.u32Width        <= UINT16_MAX
                                   && screen.u32Height       <= UINT16_MAX
                                   && screen.u32LineSize     <= UINT16_MAX * UINT32_C(4),
                                   ("One or more values out of range: u16BitsPerPixel=%#x u32Width=%#x u32Height=%#x u32LineSize=%#x\n",
                                    screen.u16BitsPerPixel, screen.u32Width, screen.u32Height, screen.u32LineSize),
                                   VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    const VBVAINFOVIEW *pView = &pCtx->aViews[screen.u32ViewIndex].view;
    const uint32_t      cbPerPixel = (screen.u16BitsPerPixel + 7) / 8;
    ASSERT_GUEST_LOGREL_MSG_RETURN(screen.u32Width <= screen.u32LineSize / (cbPerPixel ? cbPerPixel : 1),
                                   ("u32Width=%#x u32LineSize=%3x cbPerPixel=%#x\n",
                                    screen.u32Width, screen.u32LineSize, cbPerPixel),
                                   VERR_INVALID_PARAMETER);

    const uint64_t u64ScreenSize = (uint64_t)screen.u32LineSize * screen.u32Height;

    ASSERT_GUEST_LOGREL_MSG_RETURN(   screen.u32StartOffset <= pView->u32ViewSize
                                   && u64ScreenSize         <= pView->u32MaxScreenSize
                                   && screen.u32StartOffset <= pView->u32ViewSize - (uint32_t)u64ScreenSize,
                                   ("u32StartOffset=%#x u32ViewSize=%#x u64ScreenSize=%#RX64 u32MaxScreenSize=%#x\n",
                                    screen.u32StartOffset, pView->u32ViewSize, u64ScreenSize, pView->u32MaxScreenSize),
                                   VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Do the job.
     */
    vbvaResize(pVGAState, &pCtx->aViews[screen.u32ViewIndex], &screen, true);
    return VINF_SUCCESS;
}

int VBVAGetInfoViewAndScreen(PVGASTATE pVGAState, uint32_t u32ViewIndex, VBVAINFOVIEW *pView, VBVAINFOSCREEN *pScreen)
{
    if (u32ViewIndex >= pVGAState->cMonitors)
        return VERR_INVALID_PARAMETER;

    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pIns);

    if (pView)
        *pView = pCtx->aViews[u32ViewIndex].view;

    if (pScreen)
        *pScreen = pCtx->aViews[u32ViewIndex].screen;

    return VINF_SUCCESS;
}

static int vbvaHandleEnable(PVGASTATE pVGAState, uint32_t fEnableFlags, uint32_t offEnable, uint32_t idScreen)
{
    LogFlowFunc(("VBVA_ENABLE[%u]: fEnableFlags=0x%x offEnable=%#x\n", idScreen, fEnableFlags, offEnable));
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBVACONTEXT   *pCtx = (VBVACONTEXT *)HGSMIContext(pIns);

    /*
     * Validate input.
     */
    ASSERT_GUEST_LOGREL_MSG_RETURN(idScreen < pCtx->cViews, ("idScreen=%#x cViews=%#x\n", idScreen, pCtx->cViews), VERR_INVALID_PARAMETER);
    ASSERT_GUEST_LOGREL_MSG_RETURN(   (fEnableFlags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_ENABLE
                                    || (fEnableFlags & (VBVA_F_ENABLE | VBVA_F_DISABLE)) == VBVA_F_DISABLE,
                                   ("fEnableFlags=%#x\n", fEnableFlags),
                                   VERR_INVALID_PARAMETER);
    if (fEnableFlags & VBVA_F_ENABLE)
    {
        ASSERT_GUEST_LOGREL_MSG_RETURN(offEnable < pVGAState->vram_size,
                                       ("offEnable=%#x vram_size=%#x\n", offEnable, pVGAState->vram_size),
                                       VERR_INVALID_PARAMETER);
        if (fEnableFlags & VBVA_F_ABSOFFSET)
            /* Offset from VRAM start. */
            ASSERT_GUEST_LOGREL_MSG_RETURN(   pVGAState->vram_size >= RT_UOFFSETOF(VBVABUFFER, au8Data)
                                           && offEnable <= pVGAState->vram_size - RT_UOFFSETOF(VBVABUFFER, au8Data),
                                           ("offEnable=%#x vram_size=%#x\n", offEnable, pVGAState->vram_size),
                                           VERR_INVALID_PARAMETER);
        else
        {
            /* Offset from the view start.  We'd be using idScreen here to fence required. */
            RT_UNTRUSTED_VALIDATED_FENCE();
            const VBVAINFOVIEW *pView = &pCtx->aViews[idScreen].view;
            ASSERT_GUEST_LOGREL_MSG_RETURN(   pVGAState->vram_size - offEnable >= pView->u32ViewOffset
                                           && pView->u32ViewSize >= RT_UOFFSETOF(VBVABUFFER, au8Data)
                                           && offEnable <= pView->u32ViewSize - RT_UOFFSETOF(VBVABUFFER, au8Data),
                                           ("offEnable=%#x vram_size=%#x view: %#x LB %#x\n",
                                            offEnable, pVGAState->vram_size, pView->u32ViewOffset, pView->u32ViewSize),
                                           VERR_INVALID_PARAMETER);
            offEnable += pView->u32ViewOffset;
        }
        ASSERT_GUEST_LOGREL_MSG_RETURN(HGSMIIsOffsetValid(pIns, offEnable),
                                       ("offEnable=%#x area %#x LB %#x\n",
                                        offEnable, HGSMIGetAreaOffset(pIns), HGSMIGetAreaSize(pIns)),
                                       VERR_INVALID_PARAMETER);
    }
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Execute.
     */
    int rc = VINF_SUCCESS;
    if (fEnableFlags & VBVA_F_ENABLE)
    {
        VBVABUFFER RT_UNTRUSTED_VOLATILE_GUEST *pVBVA
            = (VBVABUFFER RT_UNTRUSTED_VOLATILE_GUEST *)HGSMIOffsetToPointerHost(pIns, offEnable);
        ASSERT_GUEST_LOGREL_RETURN(pVBVA, VERR_INVALID_PARAMETER); /* already check above, but let's be careful. */

        /* Process any pending orders and empty the VBVA ring buffer. */
        vbvaFlush(pVGAState, pCtx);

        rc = vbvaEnable(idScreen, pVGAState, pCtx, pVBVA, offEnable, false /* fRestored */);
        if (RT_FAILURE(rc))
            LogRelMax(8, ("VBVA: can not enable: %Rrc\n", rc));
    }
    else
        rc = vbvaDisable(idScreen, pVGAState, pCtx);
    return rc;
}

static int vbvaHandleQueryModeHints(PVGASTATE pVGAState, VBVAQUERYMODEHINTS volatile *pQueryModeHints, HGSMISIZE cbBuffer)
{
    PHGSMIINSTANCE pIns = pVGAState->pHGSMI;
    VBVACONTEXT   *pCtx = (VBVACONTEXT *)HGSMIContext(pIns);

    /*
     * Copy and validate the request.
     */
    uint16_t const cHintsQueried         = pQueryModeHints->cHintsQueried;
    uint16_t const cbHintStructureGuest  = pQueryModeHints->cbHintStructureGuest;
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

    LogRelFlowFunc(("VBVA: HandleQueryModeHints: cHintsQueried=%RU16, cbHintStructureGuest=%RU16\n",
                    cHintsQueried, cbHintStructureGuest));
    ASSERT_GUEST_RETURN(cbBuffer >= sizeof(VBVAQUERYMODEHINTS) + (uint32_t)cHintsQueried * cbHintStructureGuest,
                        VERR_INVALID_PARAMETER);
    RT_UNTRUSTED_VALIDATED_FENCE();

    /*
     * Produce the requested data.
     */
    uint8_t *pbHint = (uint8_t *)(pQueryModeHints + 1);
    memset(pbHint, ~0, cbBuffer - sizeof(VBVAQUERYMODEHINTS));

    for (unsigned iHint = 0; iHint < cHintsQueried && iHint < VBOX_VIDEO_MAX_SCREENS; ++iHint)
    {
        memcpy(pbHint, &pCtx->aModeHints[iHint], RT_MIN(cbHintStructureGuest, sizeof(VBVAMODEHINT)));
        pbHint += cbHintStructureGuest;
        Assert((uintptr_t)(pbHint - (uint8_t *)pQueryModeHints) <= cbBuffer);
    }

    return VINF_SUCCESS;
}

/*
 *
 * New VBVA uses a new interface id: #define VBE_DISPI_ID_VBOX_VIDEO         0xBE01
 *
 * VBVA uses two 32 bits IO ports to write VRAM offsets of shared memory blocks for commands.
 *                                 Read                        Write
 * Host port 0x3b0                 to process                  completed
 * Guest port 0x3d0                control value?              to process
 *
 */

static DECLCALLBACK(void) vbvaNotifyGuest (void *pvCallback)
{
#if defined(VBOX_WITH_HGSMI) && (defined(VBOX_WITH_VIDEOHWACCEL) || defined(VBOX_WITH_VDMA) || defined(VBOX_WITH_WDDM))
    PVGASTATE pVGAState = (PVGASTATE)pvCallback;
    VBVARaiseIrq (pVGAState, 0);
#else
    NOREF(pvCallback);
    /* Do nothing. Later the VMMDev/VGA IRQ can be used for the notification. */
#endif
}

/**
 * The guest submitted a command buffer (hit VGA_PORT_HGSMI_GUEST).
 *
 * Verify the buffer size and invoke corresponding handler.
 *
 * @return VBox status code.
 * @param pvHandler      The VBVA channel context.
 * @param u16ChannelInfo Command code.
 * @param pvBuffer       HGSMI buffer with command data.  Considered volatile!
 * @param cbBuffer       Size of command data.
 *
 * @thread EMT
 */
static DECLCALLBACK(int) vbvaChannelHandler(void *pvHandler, uint16_t u16ChannelInfo,
                                            void RT_UNTRUSTED_VOLATILE_GUEST *pvBuffer, HGSMISIZE cbBuffer)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pvHandler %p, u16ChannelInfo %d, pvBuffer %p, cbBuffer %u\n", pvHandler, u16ChannelInfo, pvBuffer, cbBuffer));

    PVGASTATE       pVGAState = (PVGASTATE)pvHandler;
    PHGSMIINSTANCE  pIns      = pVGAState->pHGSMI;
    VBVACONTEXT    *pCtx      = (VBVACONTEXT *)HGSMIContext(pIns);

    switch (u16ChannelInfo)
    {
#ifdef VBOX_WITH_CRHGSMI
        case VBVA_CMDVBVA_SUBMIT:
            rc = vboxCmdVBVACmdSubmit(pVGAState);
            break;

        case VBVA_CMDVBVA_FLUSH:
            rc = vboxCmdVBVACmdFlush(pVGAState);
            break;

        case VBVA_CMDVBVA_CTL:
            if (cbBuffer >= VBoxSHGSMIBufferHeaderSize() + sizeof(VBOXCMDVBVA_CTL))
            {
                VBOXCMDVBVA_CTL RT_UNTRUSTED_VOLATILE_GUEST *pCtl
                    = (VBOXCMDVBVA_CTL RT_UNTRUSTED_VOLATILE_GUEST *)VBoxSHGSMIBufferData((VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
                rc = vboxCmdVBVACmdCtl(pVGAState, pCtl, cbBuffer - VBoxSHGSMIBufferHeaderSize());
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;
#endif /* VBOX_WITH_CRHGSMI */

#ifdef VBOX_WITH_VDMA
        case VBVA_VDMA_CMD:
            if (cbBuffer >= VBoxSHGSMIBufferHeaderSize() + sizeof(VBOXVDMACBUF_DR))
            {
                VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_GUEST *pCmd
                    = (VBOXVDMACBUF_DR RT_UNTRUSTED_VOLATILE_GUEST *)VBoxSHGSMIBufferData((VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
                vboxVDMACommand(pVGAState->pVdma, pCmd, cbBuffer - VBoxSHGSMIBufferHeaderSize());
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_VDMA_CTL:
            if (cbBuffer >= VBoxSHGSMIBufferHeaderSize() + sizeof(VBOXVDMA_CTL))
            {
                VBOXVDMA_CTL RT_UNTRUSTED_VOLATILE_GUEST *pCmd
                    = (VBOXVDMA_CTL RT_UNTRUSTED_VOLATILE_GUEST *)VBoxSHGSMIBufferData((VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
                vboxVDMAControl(pVGAState->pVdma, pCmd, cbBuffer - VBoxSHGSMIBufferHeaderSize());
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;
#endif /* VBOX_WITH_VDMA */

        case VBVA_QUERY_CONF32:
            if (cbBuffer >= sizeof(VBVACONF32))
                rc = vbvaHandleQueryConf32(pVGAState, (VBVACONF32 RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_SET_CONF32:
            if (cbBuffer >= sizeof(VBVACONF32))
                rc = vbvaHandleSetConf32((VBVACONF32 RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_INFO_VIEW:
            /* Expect at least one VBVAINFOVIEW structure. */
            rc = VERR_INVALID_PARAMETER;
            if (cbBuffer >= sizeof(VBVAINFOVIEW))
            {
#ifdef VBOX_WITH_CRHGSMI
                AssertMsgBreak(!vboxCmdVBVAIsEnabled(pVGAState), ("VBVA_INFO_VIEW is not acceptible for CmdVbva\n"));
#endif
                /* Guest submits an array of VBVAINFOVIEW structures. */
                const VBVAINFOVIEW RT_UNTRUSTED_VOLATILE_GUEST *pView = (VBVAINFOVIEW RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                for (;
                     cbBuffer >= sizeof(VBVAINFOVIEW);
                     ++pView, cbBuffer -= sizeof(VBVAINFOVIEW))
                {
                    rc = VBVAInfoView(pVGAState, pView);
                    if (RT_FAILURE(rc))
                        break;
                }
            }
            break;

        case VBVA_INFO_HEAP:
            if (cbBuffer >= sizeof(VBVAINFOHEAP))
                rc = vbvaHandleInfoHeap(pVGAState, (VBVAINFOHEAP RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_FLUSH:
            if (cbBuffer >= sizeof(VBVAFLUSH))
                rc = vbvaFlush(pVGAState, pCtx);
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_INFO_SCREEN:
            rc = VERR_INVALID_PARAMETER;
#ifdef VBOX_WITH_CRHGSMI
            AssertMsgBreak(!vboxCmdVBVAIsEnabled(pVGAState), ("VBVA_INFO_SCREEN is not acceptible for CmdVbva\n"));
#endif
            if (cbBuffer >= sizeof(VBVAINFOSCREEN))
                rc = VBVAInfoScreen(pVGAState, (VBVAINFOSCREEN RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
            break;

        case VBVA_ENABLE:
            rc = VERR_INVALID_PARAMETER;
#ifdef VBOX_WITH_CRHGSMI
            AssertMsgBreak(!vboxCmdVBVAIsEnabled(pVGAState), ("VBVA_ENABLE is not acceptible for CmdVbva\n"));
#endif /* VBOX_WITH_CRHGSMI */
            if (cbBuffer >= sizeof(VBVAENABLE))
            {
                VBVAENABLE RT_UNTRUSTED_VOLATILE_GUEST *pVbvaEnable = (VBVAENABLE RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                uint32_t const fEnableFlags = pVbvaEnable->u32Flags;
                uint32_t const offEnable    = pVbvaEnable->u32Offset;
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

                uint32_t idScreen;
                if (fEnableFlags & VBVA_F_EXTENDED)
                {
                    ASSERT_GUEST_STMT_BREAK(cbBuffer >= sizeof(VBVAENABLE_EX), rc = VERR_INVALID_PARAMETER);
                    idScreen = ((VBVAENABLE_EX RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer)->u32ScreenId;
                    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();
                }
                else
                    idScreen = vbvaViewFromBufferPtr(pIns, pCtx, pvBuffer);

                rc = vbvaHandleEnable(pVGAState, fEnableFlags, offEnable, idScreen);
                pVbvaEnable->i32Result = rc;
            }
            break;

        case VBVA_MOUSE_POINTER_SHAPE:
            if (cbBuffer >= sizeof(VBVAMOUSEPOINTERSHAPE))
            {
                VBVAMOUSEPOINTERSHAPE RT_UNTRUSTED_VOLATILE_GUEST *pShape
                    = (VBVAMOUSEPOINTERSHAPE RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                rc = vbvaMousePointerShape(pVGAState, pCtx, pShape, cbBuffer);
                pShape->i32Result = rc;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;


#ifdef VBOX_WITH_VIDEOHWACCEL
        case VBVA_VHWA_CMD:
            if (cbBuffer >= VBOXVHWACMD_HEADSIZE())
            {
                vbvaVHWAHandleCommand(pVGAState, (VBOXVHWACMD RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer);
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;
#endif

#ifdef VBOX_WITH_WDDM
        case VBVA_INFO_CAPS:
            if (cbBuffer >= sizeof(VBVACAPS))
            {
                VBVACAPS RT_UNTRUSTED_VOLATILE_GUEST *pCaps = (VBVACAPS RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                pVGAState->fGuestCaps = pCaps->fCaps;
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

                pVGAState->pDrv->pfnVBVAGuestCapabilityUpdate(pVGAState->pDrv, pVGAState->fGuestCaps);
                pCaps->rc = rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;
#endif

        case VBVA_SCANLINE_CFG:
            if (cbBuffer >= sizeof(VBVASCANLINECFG))
            {
                VBVASCANLINECFG RT_UNTRUSTED_VOLATILE_GUEST *pCfg = (VBVASCANLINECFG RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                pVGAState->fScanLineCfg = pCfg->fFlags;
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

                pCfg->rc = rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_QUERY_MODE_HINTS:
            if (cbBuffer >= sizeof(VBVAQUERYMODEHINTS))
            {
                VBVAQUERYMODEHINTS RT_UNTRUSTED_VOLATILE_GUEST *pQueryModeHints
                    = (VBVAQUERYMODEHINTS RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                rc = vbvaHandleQueryModeHints(pVGAState, pQueryModeHints, cbBuffer);
                pQueryModeHints->rc = rc;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_REPORT_INPUT_MAPPING:
            if (cbBuffer >= sizeof(VBVAREPORTINPUTMAPPING))
            {
                VBVAREPORTINPUTMAPPING inputMapping;
                {
                    VBVAREPORTINPUTMAPPING RT_UNTRUSTED_VOLATILE_GUEST *pInputMapping
                        = (VBVAREPORTINPUTMAPPING RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                    inputMapping.x  = pInputMapping->x;
                    inputMapping.y  = pInputMapping->y;
                    inputMapping.cx = pInputMapping->cx;
                    inputMapping.cy = pInputMapping->cy;
                }
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

                LogRelFlowFunc(("VBVA: ChannelHandler: VBVA_REPORT_INPUT_MAPPING: x=%RI32, y=%RI32, cx=%RU32, cy=%RU32\n",
                                inputMapping.x, inputMapping.y, inputMapping.cx, inputMapping.cy));
                pVGAState->pDrv->pfnVBVAInputMappingUpdate(pVGAState->pDrv,
                                                           inputMapping.x, inputMapping.y,
                                                           inputMapping.cx, inputMapping.cy);
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        case VBVA_CURSOR_POSITION:
            if (cbBuffer >= sizeof(VBVACURSORPOSITION))
            {
                VBVACURSORPOSITION RT_UNTRUSTED_VOLATILE_GUEST *pReport = (VBVACURSORPOSITION RT_UNTRUSTED_VOLATILE_GUEST *)pvBuffer;
                VBVACURSORPOSITION Report;
                Report.fReportPosition = pReport->fReportPosition;
                Report.x               = pReport->x;
                Report.y               = pReport->y;
                RT_UNTRUSTED_NONVOLATILE_COPY_FENCE();

                LogRelFlowFunc(("VBVA: ChannelHandler: VBVA_CURSOR_POSITION: fReportPosition=%RTbool, x=%RU32, y=%RU32\n",
                                RT_BOOL(Report.fReportPosition), Report.x, Report.y));

                pVGAState->pDrv->pfnVBVAReportCursorPosition(pVGAState->pDrv, RT_BOOL(Report.fReportPosition), Report.x, Report.y);
                pReport->x = pCtx->xCursor;
                pReport->y = pCtx->yCursor;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_INVALID_PARAMETER;
            break;

        default:
            Log(("Unsupported VBVA guest command %d (%#x)!!!\n", u16ChannelInfo, u16ChannelInfo));
            break;
    }

    return rc;
}

/* When VBVA is paused, then VGA device is allowed to work but
 * no HGSMI etc state is changed.
 */
void VBVAPause(PVGASTATE pVGAState, bool fPause)
{
    if (!pVGAState || !pVGAState->pHGSMI)
    {
        return;
    }

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);

    if (pCtx)
    {
        pCtx->fPaused = fPause;
    }
}

bool VBVAIsPaused(PVGASTATE pVGAState)
{
    if (pVGAState && pVGAState->pHGSMI)
    {
        const VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
        if (pCtx && pCtx->cViews)
        {
            /* If VBVA is enabled at all. */
            const VBVAVIEW *pView = &pCtx->aViews[0];
            if (pView->vbva.guest.pVBVA)
                return pCtx->fPaused;
        }
    }
    /* VBVA is disabled. */
    return true;
}

void VBVAOnVBEChanged(PVGASTATE pVGAState)
{
    /* The guest does not depend on host handling the VBE registers. */
    if (pVGAState->fGuestCaps & VBVACAPS_USE_VBVA_ONLY)
    {
        return;
    }

    VBVAPause(pVGAState, (pVGAState->vbe_regs[VBE_DISPI_INDEX_ENABLE] & VBE_DISPI_ENABLED) == 0);
}

void VBVAReset (PVGASTATE pVGAState)
{
    if (!pVGAState || !pVGAState->pHGSMI)
    {
        return;
    }

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

#ifdef VBOX_WITH_VIDEOHWACCEL
    vbvaVHWAReset (pVGAState);
#endif

    HGSMIReset(pVGAState->pHGSMI);
    /* Make sure the IRQ is reset. */
    PDMDevHlpPCISetIrq(pVGAState->pDevInsR3, 0, PDM_IRQ_LEVEL_LOW);
    pVGAState->fu32PendingGuestFlags = 0;

    if (pCtx)
    {
        vbvaFlush (pVGAState, pCtx);

        unsigned uScreenId;

        for (uScreenId = 0; uScreenId < pCtx->cViews; uScreenId++)
        {
            vbvaDisable (uScreenId, pVGAState, pCtx);
        }

        pCtx->mouseShapeInfo.fSet = false;
        RTMemFree(pCtx->mouseShapeInfo.pu8Shape);
        pCtx->mouseShapeInfo.pu8Shape = NULL;
        pCtx->mouseShapeInfo.cbAllocated = 0;
        pCtx->mouseShapeInfo.cbShape = 0;
    }

}

int VBVAUpdateDisplay (PVGASTATE pVGAState)
{
    int rc = VERR_NOT_SUPPORTED; /* Assuming that the VGA device will have to do updates. */

    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext (pVGAState->pHGSMI);

    if (pCtx)
    {
        if (!pCtx->fPaused)
        {
            rc = vbvaFlush (pVGAState, pCtx);

            if (RT_SUCCESS (rc))
            {
                if (!pCtx->aViews[0].vbva.guest.pVBVA)
                {
                    /* VBVA is not enabled for the first view, so VGA device must do updates. */
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        }
    }

    return rc;
}

static int vbvaSendModeHintWorker(PVGASTATE pThis, uint32_t cx, uint32_t cy,
                                  uint32_t cBPP, uint32_t iDisplay, uint32_t dx,
                                  uint32_t dy, uint32_t fEnabled,
                                  uint32_t fNotifyGuest)
{
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pThis->pHGSMI);
    /** @note See Display::setVideoModeHint: "It is up to the guest to decide
     *  whether the hint is valid. Therefore don't do any VRAM sanity checks
     *  here! */
    if (iDisplay >= RT_MIN(pThis->cMonitors, RT_ELEMENTS(pCtx->aModeHints)))
        return VERR_OUT_OF_RANGE;
    pCtx->aModeHints[iDisplay].magic    = VBVAMODEHINT_MAGIC;
    pCtx->aModeHints[iDisplay].cx       = cx;
    pCtx->aModeHints[iDisplay].cy       = cy;
    pCtx->aModeHints[iDisplay].cBPP     = cBPP;
    pCtx->aModeHints[iDisplay].dx       = dx;
    pCtx->aModeHints[iDisplay].dy       = dy;
    pCtx->aModeHints[iDisplay].fEnabled = fEnabled;
    if (fNotifyGuest && pThis->fGuestCaps & VBVACAPS_IRQ && pThis->fGuestCaps & VBVACAPS_VIDEO_MODE_HINTS)
        VBVARaiseIrq(pThis, HGSMIHOSTFLAGS_HOTPLUG);
    return VINF_SUCCESS;
}

/** Converts a display port interface pointer to a vga state pointer. */
#define IDISPLAYPORT_2_VGASTATE(pInterface) ( (PVGASTATE)((uintptr_t)pInterface - RT_OFFSETOF(VGASTATE, IPort)) )

DECLCALLBACK(int) vbvaPortSendModeHint(PPDMIDISPLAYPORT pInterface, uint32_t cx,
                                       uint32_t cy, uint32_t cBPP,
                                       uint32_t iDisplay, uint32_t dx,
                                       uint32_t dy, uint32_t fEnabled,
                                       uint32_t fNotifyGuest)
{
    PVGASTATE pThis;
    int rc;

    pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);
    rc = vbvaSendModeHintWorker(pThis, cx, cy, cBPP, iDisplay, dx, dy, fEnabled,
                                fNotifyGuest);
    PDMCritSectLeave(&pThis->CritSect);
    return rc;
}

DECLCALLBACK(void) vbvaPortReportHostCursorCapabilities(PPDMIDISPLAYPORT pInterface, uint32_t fCapabilitiesAdded,
                                                        uint32_t fCapabilitiesRemoved)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);
    pThis->fHostCursorCapabilities |= fCapabilitiesAdded;
    pThis->fHostCursorCapabilities &= ~fCapabilitiesRemoved;
    if (pThis->fGuestCaps & VBVACAPS_IRQ && pThis->fGuestCaps & VBVACAPS_DISABLE_CURSOR_INTEGRATION)
        VBVARaiseIrq(pThis, HGSMIHOSTFLAGS_CURSOR_CAPABILITIES);
    PDMCritSectLeave(&pThis->CritSect);
}

DECLCALLBACK(void) vbvaPortReportHostCursorPosition(PPDMIDISPLAYPORT pInterface, uint32_t x, uint32_t y)
{
    PVGASTATE pThis = IDISPLAYPORT_2_VGASTATE(pInterface);
    VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pThis->pHGSMI);
    int rc = PDMCritSectEnter(&pThis->CritSect, VERR_SEM_BUSY);
    AssertRC(rc);
    pCtx->xCursor = x;
    pCtx->yCursor = y;
    PDMCritSectLeave(&pThis->CritSect);
}

int VBVAInit(PVGASTATE pVGAState)
{
    PPDMDEVINS pDevIns = pVGAState->pDevInsR3;

    PVM pVM = PDMDevHlpGetVM(pDevIns);

    int rc = HGSMICreate(&pVGAState->pHGSMI,
                         pVM,
                         "VBVA",
                         0,
                         pVGAState->vram_ptrR3,
                         pVGAState->vram_size,
                         vbvaNotifyGuest,
                         pVGAState,
                         sizeof(VBVACONTEXT));
     if (RT_SUCCESS(rc))
     {
         rc = HGSMIHostChannelRegister(pVGAState->pHGSMI,
                                       HGSMI_CH_VBVA,
                                       vbvaChannelHandler,
                                       pVGAState);
         if (RT_SUCCESS(rc))
         {
             VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pVGAState->pHGSMI);
             pCtx->cViews = pVGAState->cMonitors;
             pCtx->fPaused = true;
             memset(pCtx->aModeHints, ~0, sizeof(pCtx->aModeHints));
             pVGAState->fHostCursorCapabilities = 0;
         }
     }

     return rc;

}

void VBVADestroy(PVGASTATE pVGAState)
{
    PHGSMIINSTANCE pHgsmi = pVGAState->pHGSMI;
    if (pHgsmi)
    {
        VBVACONTEXT *pCtx = (VBVACONTEXT *)HGSMIContext(pHgsmi);
        pCtx->mouseShapeInfo.fSet = false;
        RTMemFree(pCtx->mouseShapeInfo.pu8Shape);
        pCtx->mouseShapeInfo.pu8Shape = NULL;
        pCtx->mouseShapeInfo.cbAllocated = 0;
        pCtx->mouseShapeInfo.cbShape = 0;

        HGSMIDestroy(pHgsmi);
        pVGAState->pHGSMI = NULL;
    }
}

