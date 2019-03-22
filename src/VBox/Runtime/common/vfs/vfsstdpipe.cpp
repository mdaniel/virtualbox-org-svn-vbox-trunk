/* $Id$ */
/** @file
 * IPRT - Virtual File System, Standard Pipe I/O stream Implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private data of a standard pipe.
 */
typedef struct RTVFSSTDPIPE
{
    /** The pipe handle. */
    RTPIPE          hPipe;
    /** Whether to leave the handle open when the VFS handle is closed. */
    bool            fLeaveOpen;
    /** Set if primarily read, clear if write. */
    bool            fReadPipe;
    /** Fake stream position. */
    uint64_t        offFakePos;
} RTVFSSTDPIPE;
/** Pointer to the private data of a standard pipe. */
typedef RTVFSSTDPIPE *PRTVFSSTDPIPE;


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfsStdPipe_Close(void *pvThis)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;

    int rc;
    if (!pThis->fLeaveOpen)
        rc = RTPipeClose(pThis->hPipe);
    else
        rc = VINF_SUCCESS;
    pThis->hPipe = NIL_RTPIPE;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfsStdPipe_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    return RTPipeQueryInfo(pThis->hPipe, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtVfsStdPipe_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    int           rc;
    AssertReturn(off < 0 || pThis->offFakePos == (uint64_t)off, VERR_SEEK_ON_DEVICE);

    NOREF(fBlocking);
    if (pSgBuf->cSegs == 1)
    {
        if (fBlocking)
            rc = RTPipeReadBlocking(pThis->hPipe, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
        else
            rc = RTPipeRead(        pThis->hPipe, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
        if (RT_SUCCESS(rc))
            pThis->offFakePos += pcbRead ? *pcbRead : pSgBuf->paSegs[0].cbSeg;
    }
    else
    {
        size_t  cbSeg      = 0;
        size_t  cbRead     = 0;
        size_t  cbReadSeg  = 0;
        size_t *pcbReadSeg = pcbRead ? &cbReadSeg : NULL;
        rc = VINF_SUCCESS;

        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            void *pvSeg = pSgBuf->paSegs[iSeg].pvSeg;
            cbSeg       = pSgBuf->paSegs[iSeg].cbSeg;

            cbReadSeg = cbSeg;
            if (fBlocking)
                rc = RTPipeReadBlocking(pThis->hPipe, pvSeg, cbSeg, pcbReadSeg);
            else
                rc = RTPipeRead(        pThis->hPipe, pvSeg, cbSeg, pcbReadSeg);
            if (RT_FAILURE(rc))
                break;
            pThis->offFakePos += pcbRead ? cbReadSeg : cbSeg;
            cbRead += cbReadSeg;
            if (rc != VINF_SUCCESS)
                break;
            AssertBreakStmt(!pcbRead || cbReadSeg == cbSeg, rc = VINF_TRY_AGAIN);
        }

        if (pcbRead)
            *pcbRead = cbRead;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtVfsStdPipe_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    int           rc;
    AssertReturn(off < 0 || pThis->offFakePos == (uint64_t)off, VERR_SEEK_ON_DEVICE);

    if (pSgBuf->cSegs == 1)
    {
        if (fBlocking)
            rc = RTPipeWriteBlocking(pThis->hPipe, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
        else
            rc = RTPipeWrite(        pThis->hPipe, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
        if (RT_SUCCESS(rc))
            pThis->offFakePos += pcbWritten ? *pcbWritten : pSgBuf->paSegs[0].cbSeg;
    }
    else
    {
        size_t  cbWritten     = 0;
        size_t  cbWrittenSeg;
        size_t *pcbWrittenSeg = pcbWritten ? &cbWrittenSeg : NULL;
        rc = VINF_SUCCESS;

        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            void   *pvSeg  = pSgBuf->paSegs[iSeg].pvSeg;
            size_t  cbSeg  = pSgBuf->paSegs[iSeg].cbSeg;

            cbWrittenSeg = 0;
            if (fBlocking)
                rc = RTPipeWriteBlocking(pThis->hPipe, pvSeg, cbSeg, pcbWrittenSeg);
            else
                rc = RTPipeWrite(        pThis->hPipe, pvSeg, cbSeg, pcbWrittenSeg);
            if (RT_FAILURE(rc))
                break;
            pThis->offFakePos += pcbWritten ? cbWrittenSeg : cbSeg;
            if (pcbWritten)
            {
                cbWritten += cbWrittenSeg;
                if (rc != VINF_SUCCESS)
                    break;
                AssertStmt(cbWrittenSeg == cbSeg, rc = VINF_TRY_AGAIN);
            }
            else
                AssertBreak(rc == VINF_SUCCESS);
        }

        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtVfsStdPipe_Flush(void *pvThis)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    return RTPipeFlush(pThis->hPipe);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtVfsStdPipe_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                              uint32_t *pfRetEvents)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    uint32_t const fPossibleEvt = pThis->fReadPipe ? RTPOLL_EVT_READ : RTPOLL_EVT_WRITE;

    int rc = RTPipeSelectOne(pThis->hPipe, cMillies);
    if (RT_SUCCESS(rc))
    {
        if (fEvents & fPossibleEvt)
            *pfRetEvents = fPossibleEvt;
        else
            rc = RTVfsUtilDummyPollOne(fEvents, cMillies, fIntr, pfRetEvents);
    }
    else if (   rc != VERR_TIMEOUT
             && rc != VERR_INTERRUPTED
             && rc != VERR_TRY_AGAIN /* paranoia */)
    {
        *pfRetEvents = RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtVfsStdPipe_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTVFSSTDPIPE pThis = (PRTVFSSTDPIPE)pvThis;
    *poffActual = pThis->offFakePos;
    return VINF_SUCCESS;
}


/**
 * Standard pipe operations.
 */
DECL_HIDDEN_CONST(const RTVFSIOSTREAMOPS) g_rtVfsStdPipeOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "StdFile",
        rtVfsStdPipe_Close,
        rtVfsStdPipe_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    0,
    rtVfsStdPipe_Read,
    rtVfsStdPipe_Write,
    rtVfsStdPipe_Flush,
    rtVfsStdPipe_PollOne,
    rtVfsStdPipe_Tell,
    NULL /*rtVfsStdPipe_Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,
};


/**
 * Internal worker for RTVfsIosFromRTPipe and later some create API.
 *
 * @returns IRPT status code.
 * @param   hPipe               The IPRT file handle.
 * @param   fOpen               The RTFILE_O_XXX flags.
 * @param   fLeaveOpen          Whether to leave it open or close it.
 * @param   phVfsFile           Where to return the handle.
 */
static int rtVfsFileFromRTPipe(RTPIPE hPipe, uint64_t fOpen, bool fLeaveOpen, PRTVFSIOSTREAM phVfsIos)
{
    PRTVFSSTDPIPE   pThis;
    RTVFSIOSTREAM   hVfsIos;
    int rc = RTVfsNewIoStream(&g_rtVfsStdPipeOps, sizeof(RTVFSSTDPIPE), fOpen, NIL_RTVFS, NIL_RTVFSLOCK,
                              &hVfsIos, (void **)&pThis);
    if (RT_FAILURE(rc))
        return rc;

    pThis->hPipe        = hPipe;
    pThis->fLeaveOpen   = fLeaveOpen;
    *phVfsIos = hVfsIos;
    return VINF_SUCCESS;
}


RTDECL(int) RTVfsIoStrmFromRTPipe(RTPIPE hPipe, bool fLeaveOpen, PRTVFSIOSTREAM phVfsIos)
{
    /*
     * Check the handle validity and read/write mode, then create a stream for it.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTPipeQueryInfo(hPipe, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
        rc = rtVfsFileFromRTPipe(hPipe,
                                 ObjInfo.Attr.fMode & RTFS_DOS_READONLY ? RTFILE_O_READ : RTFILE_O_WRITE,
                                 fLeaveOpen, phVfsIos);
    return rc;
}

/** @todo Create pipe API? */

