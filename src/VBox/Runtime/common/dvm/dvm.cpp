/* $Id$ */
/** @file
 * IPRT Disk Volume Management API (DVM) - generic code.
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
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/dvm.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/list.h>
#include "internal/dvm.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * The internal volume manager structure.
 */
typedef struct RTDVMINTERNAL
{
    /** The DVM magic (RTDVM_MAGIC). */
    uint32_t          u32Magic;
    /** The disk descriptor. */
    RTDVMDISK         DvmDisk;
    /** Pointer to the backend operations table after a successful probe. */
    PCRTDVMFMTOPS     pDvmFmtOps;
    /** The format specific volume manager data. */
    RTDVMFMT          hVolMgrFmt;
    /** Flags passed on manager creation. */
    uint32_t          fFlags;
    /** Reference counter. */
    uint32_t volatile cRefs;
    /** List of recognised volumes (RTDVMVOLUMEINTERNAL). */
    RTLISTANCHOR      VolumeList;
} RTDVMINTERNAL;
/** Pointer to an internal volume manager. */
typedef RTDVMINTERNAL *PRTDVMINTERNAL;

/**
 * The internal volume structure.
 */
typedef struct RTDVMVOLUMEINTERNAL
{
    /** The DVM volume magic (RTDVMVOLUME_MAGIC). */
    uint32_t                      u32Magic;
    /** Node for the volume list. */
    RTLISTNODE                    VolumeNode;
    /** Pointer to the owning volume manager. */
    PRTDVMINTERNAL                pVolMgr;
    /** Format specific volume data. */
    RTDVMVOLUMEFMT                hVolFmt;
    /** Set block status.callback */
    PFNDVMVOLUMEQUERYBLOCKSTATUS  pfnQueryBlockStatus;
    /** Opaque user data. */
    void                         *pvUser;
    /** Reference counter. */
    uint32_t volatile             cRefs;
} RTDVMVOLUMEINTERNAL;
/** Pointer to an internal volume. */
typedef RTDVMVOLUMEINTERNAL *PRTDVMVOLUMEINTERNAL;


/*********************************************************************************************************************************
*   Global variables                                                                                                             *
*********************************************************************************************************************************/
extern RTDVMFMTOPS g_rtDvmFmtMbr;
extern RTDVMFMTOPS g_rtDvmFmtGpt;
extern RTDVMFMTOPS g_rtDvmFmtBsdLbl;

/**
 * Supported volume formats.
 */
static PCRTDVMFMTOPS g_aDvmFmts[] =
{
    &g_rtDvmFmtMbr,
    &g_rtDvmFmtGpt,
    &g_rtDvmFmtBsdLbl
};

/**
 * Descriptions of the volume types.
 *
 * This is indexed by RTDVMVOLTYPE.
 */
static const char * g_apcszDvmVolTypes[] =
{
    "Invalid",
    "Unknown",
    "NTFS",
    "FAT16",
    "FAT32",
    "Linux swap",
    "Linux native",
    "Linux LVM",
    "Linux SoftRaid",
    "FreeBSD",
    "NetBSD",
    "OpenBSD",
    "Mac OS X HFS or HFS+",
    "Solaris"
};

/**
 * Creates a new volume.
 *
 * @returns IPRT status code.
 * @param   pThis    The DVM map instance.
 * @param   hVolFmt  The format specific volume handle.
 * @param   phVol    Where to store the generic volume handle on success.
 */
static int rtDvmVolumeCreate(PRTDVMINTERNAL pThis, RTDVMVOLUMEFMT hVolFmt,
                             PRTDVMVOLUME phVol)
{
    int rc = VINF_SUCCESS;
    PRTDVMVOLUMEINTERNAL pVol = NULL;

    pVol = (PRTDVMVOLUMEINTERNAL)RTMemAllocZ(sizeof(RTDVMVOLUMEINTERNAL));
    if (pVol)
    {
        pVol->u32Magic = RTDVMVOLUME_MAGIC;
        pVol->cRefs    = 0;
        pVol->pVolMgr  = pThis;
        pVol->hVolFmt  = hVolFmt;

        *phVol = pVol;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Destroys a volume handle.
 *
 * @param   pThis               The volume to destroy.
 */
static void rtDvmVolumeDestroy(PRTDVMVOLUMEINTERNAL pThis)
{
    PRTDVMINTERNAL pVolMgr = pThis->pVolMgr;

    AssertPtr(pVolMgr);

    /* Close the volume. */
    pVolMgr->pDvmFmtOps->pfnVolumeClose(pThis->hVolFmt);

    pThis->u32Magic = RTDVMVOLUME_MAGIC_DEAD;
    pThis->pVolMgr  = NULL;
    pThis->hVolFmt  = NIL_RTDVMVOLUMEFMT;
    RTMemFree(pThis);

    /* Release the reference of the volume manager. */
    RTDvmRelease(pVolMgr);
}

RTDECL(int) RTDvmCreate(PRTDVM phVolMgr, PFNDVMREAD pfnRead,
                        PFNDVMWRITE pfnWrite, uint64_t cbDisk,
                        uint64_t cbSector, uint32_t fFlags, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL pThis;

    AssertMsgReturn(!(fFlags & ~DVM_FLAGS_MASK),
                    ("Invalid flags given %#x\n", fFlags),
                    VERR_INVALID_PARAMETER);

    pThis = (PRTDVMINTERNAL)RTMemAllocZ(sizeof(RTDVMINTERNAL));
    if (pThis)
    {
        pThis->u32Magic         = RTDVM_MAGIC;
        pThis->DvmDisk.cbDisk   = cbDisk;
        pThis->DvmDisk.cbSector = cbSector;
        pThis->DvmDisk.pvUser   = pvUser;
        pThis->DvmDisk.pfnRead  = pfnRead;
        pThis->DvmDisk.pfnWrite = pfnWrite;
        pThis->pDvmFmtOps       = NULL;
        pThis->hVolMgrFmt       = NIL_RTDVMFMT;
        pThis->fFlags           = fFlags;
        pThis->cRefs            = 1;
        RTListInit(&pThis->VolumeList);
        *phVolMgr = pThis;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

RTDECL(uint32_t) RTDvmRetain(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

/**
 * Destroys a volume manager handle.
 *
 * @param   pThis               The volume manager to destroy.
 */
static void rtDvmDestroy(PRTDVMINTERNAL pThis)
{
    if (pThis->hVolMgrFmt != NIL_RTDVMFMT)
    {
        AssertPtr(pThis->pDvmFmtOps);

        /* Let the backend do it's own cleanup first. */
        pThis->pDvmFmtOps->pfnClose(pThis->hVolMgrFmt);
        pThis->hVolMgrFmt = NIL_RTDVMFMT;
    }

    pThis->DvmDisk.cbDisk   = 0;
    pThis->DvmDisk.pvUser   = NULL;
    pThis->DvmDisk.pfnRead  = NULL;
    pThis->DvmDisk.pfnWrite = NULL;
    pThis->u32Magic         = RTDVM_MAGIC_DEAD;
    RTMemFree(pThis);
}

RTDECL(uint32_t) RTDvmRelease(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    if (pThis == NIL_RTDVM)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtDvmDestroy(pThis);
    return cRefs;
}

RTDECL(int) RTDvmMapOpen(RTDVM hVolMgr)
{
    int            rc = VINF_SUCCESS;
    uint32_t       uScoreMax = RTDVM_MATCH_SCORE_UNSUPPORTED;
    PCRTDVMFMTOPS  pDvmFmtOpsMatch = NULL;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt == NIL_RTDVMFMT, VERR_INVALID_HANDLE);

    Assert(!pThis->pDvmFmtOps);

    for (unsigned i = 0; i < RT_ELEMENTS(g_aDvmFmts); i++)
    {
        uint32_t uScore;
        PCRTDVMFMTOPS pDvmFmtOps = g_aDvmFmts[i];

        rc = pDvmFmtOps->pfnProbe(&pThis->DvmDisk, &uScore);
        if (   RT_SUCCESS(rc)
            && uScore > uScoreMax)
        {
            pDvmFmtOpsMatch = pDvmFmtOps;
            uScoreMax       = uScore;
        }
        else if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (uScoreMax > RTDVM_MATCH_SCORE_UNSUPPORTED)
        {
            AssertPtr(pDvmFmtOpsMatch);

            /* Open the format. */
            rc = pDvmFmtOpsMatch->pfnOpen(&pThis->DvmDisk, &pThis->hVolMgrFmt);
            if (RT_SUCCESS(rc))
            {
                uint32_t cVols;

                pThis->pDvmFmtOps = pDvmFmtOpsMatch;

                cVols = pThis->pDvmFmtOps->pfnGetValidVolumes(pThis->hVolMgrFmt);

                /* Construct volume list. */
                if (cVols)
                {
                    PRTDVMVOLUMEINTERNAL pVol = NULL;
                    RTDVMVOLUMEFMT hVolFmt = NIL_RTDVMVOLUMEFMT;

                    rc = pThis->pDvmFmtOps->pfnQueryFirstVolume(pThis->hVolMgrFmt, &hVolFmt);
                    if (RT_SUCCESS(rc))
                    {
                        rc = rtDvmVolumeCreate(pThis, hVolFmt, &pVol);
                        if (RT_FAILURE(rc))
                            pThis->pDvmFmtOps->pfnVolumeClose(hVolFmt);
                    }

                    if (RT_SUCCESS(rc))
                    {
                        cVols--;
                        RTListAppend(&pThis->VolumeList, &pVol->VolumeNode);

                        while (   cVols > 0
                               && RT_SUCCESS(rc))
                        {
                            rc = pThis->pDvmFmtOps->pfnQueryNextVolume(pThis->hVolMgrFmt, pVol->hVolFmt, &hVolFmt);
                            if (RT_SUCCESS(rc))
                            {
                                rc = rtDvmVolumeCreate(pThis, hVolFmt, &pVol);
                                if (RT_FAILURE(rc))
                                    pThis->pDvmFmtOps->pfnVolumeClose(hVolFmt);
                                else
                                    RTListAppend(&pThis->VolumeList, &pVol->VolumeNode);
                                cVols--;
                            }
                        }
                    }

                    if (RT_FAILURE(rc))
                    {
                        /* Remove all entries. */
                        PRTDVMVOLUMEINTERNAL pItNext, pIt;
                        RTListForEachSafe(&pThis->VolumeList, pIt, pItNext, RTDVMVOLUMEINTERNAL, VolumeNode)
                        {
                            RTListNodeRemove(&pIt->VolumeNode);
                            rtDvmVolumeDestroy(pIt);
                        }
                    }
                }
            }
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    return rc;
}

RTDECL(int) RTDvmMapInitialize(RTDVM hVolMgr, const char *pszFmt)
{
    int rc = VERR_NOT_SUPPORTED;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszFmt, VERR_INVALID_POINTER);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt == NIL_RTDVMFMT, VERR_INVALID_HANDLE);

    for (unsigned i = 0; i < RT_ELEMENTS(g_aDvmFmts); i++)
    {
        PCRTDVMFMTOPS pDvmFmtOps = g_aDvmFmts[i];

        if (!RTStrCmp(pDvmFmtOps->pcszFmt, pszFmt))
        {
            rc = pDvmFmtOps->pfnInitialize(&pThis->DvmDisk, &pThis->hVolMgrFmt);
            if (RT_SUCCESS(rc))
                pThis->pDvmFmtOps = pDvmFmtOps;

            break;
        }
    }

    return rc;
}

RTDECL(const char *) RTDvmMapGetFormat(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, NULL);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, NULL);

    return pThis->pDvmFmtOps->pcszFmt;
}

RTDECL(uint32_t) RTDvmMapGetValidVolumes(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, UINT32_MAX);

    return pThis->pDvmFmtOps->pfnGetValidVolumes(pThis->hVolMgrFmt);
}

RTDECL(uint32_t) RTDvmMapGetMaxVolumes(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, UINT32_MAX);

    return pThis->pDvmFmtOps->pfnGetMaxVolumes(pThis->hVolMgrFmt);
}

RTDECL(int) RTDvmMapQueryFirstVolume(RTDVM hVolMgr, PRTDVMVOLUME phVol)
{
    int rc = VERR_DVM_MAP_EMPTY;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVol, VERR_INVALID_POINTER);

    PRTDVMVOLUMEINTERNAL pVol = RTListGetFirst(&pThis->VolumeList, RTDVMVOLUMEINTERNAL, VolumeNode);
    if (pVol)
    {
        rc = VINF_SUCCESS;
        RTDvmVolumeRetain(pVol);
        *phVol = pVol;
    }

    return rc;
}

RTDECL(int) RTDvmMapQueryNextVolume(RTDVM hVolMgr, RTDVMVOLUME hVol, PRTDVMVOLUME phVolNext)
{
    int rc = VERR_DVM_MAP_NO_VOLUME;
    PRTDVMINTERNAL       pThis = hVolMgr;
    PRTDVMVOLUMEINTERNAL pVol = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, VERR_INVALID_HANDLE);
    AssertPtrReturn(pVol, VERR_INVALID_HANDLE);
    AssertReturn(pVol->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVolNext, VERR_INVALID_POINTER);

    PRTDVMVOLUMEINTERNAL pVolNext = RTListGetNext(&pThis->VolumeList, pVol,
                                                  RTDVMVOLUMEINTERNAL, VolumeNode);
    if (pVolNext)
    {
        rc = VINF_SUCCESS;
        RTDvmVolumeRetain(pVolNext);
        *phVolNext = pVolNext;
    }

    return rc;
}

RTDECL(int) RTDvmMapQueryBlockStatus(RTDVM hVolMgr, uint64_t off, uint64_t cb,
                                     bool *pfAllocated)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL       pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfAllocated, VERR_INVALID_POINTER);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, VERR_INVALID_HANDLE);
    AssertReturn(off + cb <= pThis->DvmDisk.cbDisk * pThis->DvmDisk.cbSector,
                 VERR_INVALID_PARAMETER);

    /* Check whether the range is inuse by the volume manager metadata first. */
    rc = pThis->pDvmFmtOps->pfnQueryRangeUse(pThis->hVolMgrFmt, off, cb, pfAllocated);
    if (RT_FAILURE(rc))
        return rc;

    if (!*pfAllocated)
    {
        bool fAllocated = false;

        while (   cb > 0
               && !fAllocated)
        {
            bool fVolFound = false;
            uint64_t cbIntersect;
            uint64_t offVol;

            /*
             * Search through all volumes. It is not possible to
             * get all start sectors and sizes of all volumes here
             * because volumes can be scattered around the disk for certain formats.
             * Linux LVM is one example, extents of logical volumes don't need to be
             * contigous on the medium.
             */
            PRTDVMVOLUMEINTERNAL pVol;
            RTListForEach(&pThis->VolumeList, pVol, RTDVMVOLUMEINTERNAL, VolumeNode)
            {
                bool fIntersect = pThis->pDvmFmtOps->pfnVolumeIsRangeIntersecting(pVol->hVolFmt, off,
                                                                                  cb, &offVol,
                                                                                  &cbIntersect);
                if (fIntersect)
                {
                    fVolFound = true;
                    if (pVol->pfnQueryBlockStatus)
                    {
                        bool fVolAllocated = true;

                        rc = pVol->pfnQueryBlockStatus(pVol->pvUser, offVol, cbIntersect,
                                                       &fVolAllocated);
                        if (RT_FAILURE(rc))
                            break;
                        else if (fVolAllocated)
                        {
                            fAllocated = true;
                            break;
                        }
                    }
                    else if (!(pThis->fFlags & DVM_FLAGS_NO_STATUS_CALLBACK_MARK_AS_UNUSED))
                        fAllocated = true;
                    /* else, flag is set, continue. */

                    cb  -= cbIntersect;
                    off += cbIntersect;
                    break;
                }
            }

            if (!fVolFound)
            {
                if (pThis->fFlags & DVM_FLAGS_UNUSED_SPACE_MARK_AS_USED)
                    fAllocated = true;

                cb  -= pThis->DvmDisk.cbSector;
                off += pThis->DvmDisk.cbSector;
            }
        }

        *pfAllocated = fAllocated;
    }

    return rc;
}

RTDECL(uint32_t) RTDvmVolumeRetain(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs >= 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 1)
        RTDvmRetain(pThis->pVolMgr);
    return cRefs;
}

RTDECL(uint32_t) RTDvmVolumeRelease(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    if (pThis == NIL_RTDVMVOLUME)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
    {
        /* Release the volume manager. */
        pThis->pfnQueryBlockStatus = NULL;
        RTDvmRelease(pThis->pVolMgr);
    }
    return cRefs;
}

RTDECL(void) RTDvmVolumeSetQueryBlockStatusCallback(RTDVMVOLUME hVol,
                                                    PFNDVMVOLUMEQUERYBLOCKSTATUS pfnQueryBlockStatus,
                                                    void *pvUser)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pThis->u32Magic == RTDVMVOLUME_MAGIC);

    pThis->pfnQueryBlockStatus = pfnQueryBlockStatus;
    pThis->pvUser = pvUser;
}

RTDECL(uint64_t) RTDvmVolumeGetSize(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, 0);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetSize(pThis->hVolFmt);
}

RTDECL(int) RTDvmVolumeQueryName(RTDVMVOLUME hVol, char **ppszVolName)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(ppszVolName, VERR_INVALID_POINTER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeQueryName(pThis->hVolFmt, ppszVolName);
}

RTDECL(RTDVMVOLTYPE) RTDvmVolumeGetType(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, RTDVMVOLTYPE_INVALID);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, RTDVMVOLTYPE_INVALID);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetType(pThis->hVolFmt);
}

RTDECL(uint64_t) RTDvmVolumeGetFlags(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, UINT64_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT64_MAX);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetFlags(pThis->hVolFmt);
}

RTDECL(int) RTDvmVolumeRead(RTDVMVOLUME hVol, uint64_t off, void *pvBuf, size_t cbRead)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbRead > 0, VERR_INVALID_PARAMETER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeRead(pThis->hVolFmt, off, pvBuf, cbRead);
}

RTDECL(int) RTDvmVolumeWrite(RTDVMVOLUME hVol, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbWrite > 0, VERR_INVALID_PARAMETER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeWrite(pThis->hVolFmt, off, pvBuf, cbWrite);
}

RTDECL(const char *) RTDvmVolumeTypeGetDescr(RTDVMVOLTYPE enmVolType)
{
    AssertReturn(enmVolType >= RTDVMVOLTYPE_INVALID && enmVolType < RTDVMVOLTYPE_END, NULL);

    return g_apcszDvmVolTypes[enmVolType];
}
