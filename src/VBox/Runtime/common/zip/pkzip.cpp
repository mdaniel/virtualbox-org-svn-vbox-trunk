/* $Id$ */
/** @file
 * IPRT - PKZIP archive I/O.
 */

/*
 * Copyright (C) 2014-2017 Oracle Corporation
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
#include <iprt/zip.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Memory stream private data.
 */
typedef struct MEMIOSTREAM
{
    /** Size of the memory buffer. */
    size_t      cbBuf;
    /** Pointer to the memory buffer. */
    uint8_t     *pu8Buf;
    /** Current offset. */
    size_t      off;
} MEMIOSTREAM;
typedef MEMIOSTREAM *PMEMIOSTREAM;


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) memFssIos_Close(void *pvThis)
{
    NOREF(pvThis);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) memFssIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PMEMIOSTREAM pThis = (PMEMIOSTREAM)pvThis;
    switch (enmAddAttr)
    {
        case RTFSOBJATTRADD_NOTHING:
        case RTFSOBJATTRADD_UNIX:
            RT_ZERO(*pObjInfo);
            pObjInfo->cbObject = pThis->cbBuf;
            break;
        default:
            return VERR_NOT_SUPPORTED;
    }
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) memFssIos_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PMEMIOSTREAM pThis = (PMEMIOSTREAM)pvThis;
    Assert(pSgBuf->cSegs == 1);
    RT_NOREF_PV(fBlocking);

    if (off < 0)
        off = pThis->off;
    if (off >= (RTFOFF)pThis->cbBuf)
        return pcbRead ? VINF_EOF : VERR_EOF;

    size_t cbLeft = pThis->cbBuf - off;
    size_t cbToRead = pSgBuf->paSegs[0].cbSeg;
    if (cbToRead > cbLeft)
    {
        if (!pcbRead)
            return VERR_EOF;
        cbToRead = (size_t)cbLeft;
    }

    memcpy(pSgBuf->paSegs[0].pvSeg, pThis->pu8Buf + off, cbToRead);
    pThis->off = off + cbToRead;
    if (pcbRead)
        *pcbRead = cbToRead;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) memFssIos_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    RT_NOREF_PV(pvThis); RT_NOREF_PV(off); RT_NOREF_PV(pSgBuf); RT_NOREF_PV(fBlocking); RT_NOREF_PV(pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) memFssIos_Flush(void *pvThis)
{
    RT_NOREF_PV(pvThis);
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) memFssIos_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr, uint32_t *pfRetEvents)
{
    RT_NOREF_PV(pvThis); RT_NOREF_PV(fEvents); RT_NOREF_PV(cMillies); RT_NOREF_PV(fIntr); RT_NOREF_PV(pfRetEvents);
    return VERR_NOT_IMPLEMENTED;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) memFssIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PMEMIOSTREAM pThis = (PMEMIOSTREAM)pvThis;
    *poffActual = pThis->off;
    return VINF_SUCCESS;
}

/**
 * Memory I/O object stream operations.
 */
static const RTVFSIOSTREAMOPS g_memFssIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "MemFsStream::IoStream",
        memFssIos_Close,
        memFssIos_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    RTVFSIOSTREAMOPS_FEAT_NO_SG,
    memFssIos_Read,
    memFssIos_Write,
    memFssIos_Flush,
    memFssIos_PollOne,
    memFssIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION
};

RTDECL(int) RTZipPkzipMemDecompress(void **ppvDst, size_t *pcbDst, const void *pvSrc, size_t cbSrc, const char *pszObject)
{
    PMEMIOSTREAM pIosData;
    RTVFSIOSTREAM hVfsIos;
    int rc = RTVfsNewIoStream(&g_memFssIosOps,
                              sizeof(*pIosData),
                              RTFILE_O_READ | RTFILE_O_DENY_WRITE | RTFILE_O_OPEN,
                              NIL_RTVFS,
                              NIL_RTVFSLOCK,
                              &hVfsIos,
                              (void **)&pIosData);
    if (RT_SUCCESS(rc))
    {
        pIosData->pu8Buf = (uint8_t*)pvSrc;
        pIosData->cbBuf  = cbSrc;
        pIosData->off    = 0;
        RTVFSFSSTREAM hVfsFss;
        rc = RTZipPkzipFsStreamFromIoStream(hVfsIos, 0 /*fFlags*/, &hVfsFss);
        RTVfsIoStrmRelease(hVfsIos);
        if (RT_SUCCESS(rc))
        {
            /*
             * Loop through all objects. Actually this wouldn't be required
             * for .zip files but we opened it as I/O stream.
             */
            for (bool fFound = false; !fFound;)
            {
                char        *pszName;
                RTVFSOBJ    hVfsObj;
                rc = RTVfsFsStrmNext(hVfsFss, &pszName, NULL /*penmType*/, &hVfsObj);
                if (RT_FAILURE(rc))
                    break;
                fFound = !strcmp(pszName, pszObject);
                if (fFound)
                {
                    RTFSOBJINFO UnixInfo;
                    rc = RTVfsObjQueryInfo(hVfsObj, &UnixInfo, RTFSOBJATTRADD_UNIX);
                    if (RT_SUCCESS(rc))
                    {
                        size_t cb = UnixInfo.cbObject;
                        void *pv = RTMemAlloc(cb);
                        if (pv)
                        {
                            RTVFSIOSTREAM hVfsIosObj = RTVfsObjToIoStream(hVfsObj);
                            if (hVfsIos)
                            {
                                rc = RTVfsIoStrmRead(hVfsIosObj, pv, cb, true /*fBlocking*/, NULL);
                                if (RT_SUCCESS(rc))
                                {
                                    *ppvDst = pv;
                                    *pcbDst = cb;
                                }
                                else
                                    RTMemFree(pv);
                            }
                        }
                    }
                }
                RTVfsObjRelease(hVfsObj);
                RTStrFree(pszName);
            }
            RTVfsFsStrmRelease(hVfsFss);
        }
    }
    return rc;
}
