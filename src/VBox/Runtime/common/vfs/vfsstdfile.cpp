/* $Id$ */
/** @file
 * IPRT - Virtual File System, Standard File Implementation.
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
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private data of a standard file.
 */
typedef struct RTVFSSTDFILE
{
    /** The file handle. */
    RTFILE          hFile;
    /** Whether to leave the handle open when the VFS handle is closed. */
    bool            fLeaveOpen;
} RTVFSSTDFILE;
/** Pointer to the private data of a standard file. */
typedef RTVFSSTDFILE *PRTVFSSTDFILE;


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfsStdFile_Close(void *pvThis)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;

    int rc;
    if (!pThis->fLeaveOpen)
        rc = RTFileClose(pThis->hFile);
    else
        rc = VINF_SUCCESS;
    pThis->hFile = NIL_RTFILE;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfsStdFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileQueryInfo(pThis->hFile, pObjInfo, enmAddAttr);
}


/**
 * RTFileRead and RTFileReadAt does not return VINF_EOF or VINF_TRY_AGAIN, this
 * function tries to fix this as best as it can.
 *
 * This fixing can be subject to races if some other thread or process is
 * modifying the file size between the read and our size query here.
 *
 * @returns VINF_SUCCESS, VINF_EOF or VINF_TRY_AGAIN.
 * @param   pThis               The instance data.
 * @param   off                 The offset parameter.
 * @param   cbToRead            The number of bytes attempted read .
 * @param   cbActuallyRead      The number of bytes actually read.
 */
DECLINLINE(int) rtVfsStdFile_ReadFixRC(PRTVFSSTDFILE pThis, RTFOFF off, size_t cbToRead, size_t cbActuallyRead)
{
    /* If the read returned less bytes than requested, it means the end of the
       file has been reached. */
    if (cbToRead > cbActuallyRead)
        return VINF_EOF;

    /* The other case here is the very special zero byte read at the end of the
       file, where we're supposed to indicate EOF. */
    if (cbToRead > 0)
        return VINF_SUCCESS;

    uint64_t cbFile;
    int rc = RTFileGetSize(pThis->hFile, &cbFile);
    if (RT_FAILURE(rc))
        return rc;

    uint64_t off2;
    if (off >= 0)
        off2 = off;
    else
    {
        rc = RTFileSeek(pThis->hFile, 0, RTFILE_SEEK_CURRENT, &off2);
        if (RT_FAILURE(rc))
            return rc;
    }

    return off2 >= cbFile ? VINF_EOF : VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtVfsStdFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int           rc;

    NOREF(fBlocking);
    if (pSgBuf->cSegs == 1)
    {
        if (off < 0)
            rc = RTFileRead(  pThis->hFile,      pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
        else
            rc = RTFileReadAt(pThis->hFile, off, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
        if (rc == VINF_SUCCESS && pcbRead)
            rc = rtVfsStdFile_ReadFixRC(pThis, off, pSgBuf->paSegs[0].cbSeg, *pcbRead);
    }
    else
    {
        size_t  cbSeg      = 0;
        size_t  cbRead     = 0;
        size_t  cbReadSeg  = 0;
        rc = VINF_SUCCESS;

        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            void *pvSeg = pSgBuf->paSegs[iSeg].pvSeg;
            cbSeg       = pSgBuf->paSegs[iSeg].cbSeg;

            cbReadSeg = cbSeg;
            if (off < 0)
                rc = RTFileRead(  pThis->hFile,      pvSeg, cbSeg, pcbRead ? &cbReadSeg : NULL);
            else
                rc = RTFileReadAt(pThis->hFile, off, pvSeg, cbSeg, pcbRead ? &cbReadSeg : NULL);
            if (RT_FAILURE(rc))
                break;
            if (off >= 0)
                off += cbReadSeg;
            cbRead  += cbReadSeg;
            if ((pcbRead && cbReadSeg != cbSeg) || rc != VINF_SUCCESS)
                break;
        }

        if (pcbRead)
        {
            *pcbRead = cbRead;
            if (rc == VINF_SUCCESS)
                rc = rtVfsStdFile_ReadFixRC(pThis, off, cbSeg, cbReadSeg);
        }
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtVfsStdFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int           rc;

    NOREF(fBlocking);
    if (pSgBuf->cSegs == 1)
    {
        if (off < 0)
            rc = RTFileWrite(pThis->hFile, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
        else
            rc = RTFileWriteAt(pThis->hFile, off, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
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
            if (off < 0)
                rc = RTFileWrite(pThis->hFile, pvSeg, cbSeg, pcbWrittenSeg);
            else
            {
                rc = RTFileWriteAt(pThis->hFile, off, pvSeg, cbSeg, pcbWrittenSeg);
                off += cbSeg;
            }
            if (RT_FAILURE(rc))
                break;
            if (pcbWritten)
            {
                cbWritten += cbWrittenSeg;
                if (cbWrittenSeg != cbSeg)
                    break;
            }
        }

        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtVfsStdFile_Flush(void *pvThis)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int rc = RTFileFlush(pThis->hFile);
#ifdef RT_OS_WINDOWS
    /* Workaround for console handles. */  /** @todo push this further down? */
    if (   rc == VERR_INVALID_HANDLE
        && RTFileIsValid(pThis->hFile))
        rc = VINF_NOT_SUPPORTED; /* not flushable */
#endif
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtVfsStdFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                              uint32_t *pfRetEvents)
{
    NOREF(pvThis);
    int rc;
    if (fEvents != RTPOLL_EVT_ERROR)
    {
        *pfRetEvents = fEvents & ~RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }
    else if (fIntr)
        rc = RTThreadSleep(cMillies);
    else
    {
        uint64_t uMsStart = RTTimeMilliTS();
        do
            rc = RTThreadSleep(cMillies);
        while (   rc == VERR_INTERRUPTED
               && !fIntr
               && RTTimeMilliTS() - uMsStart < cMillies);
        if (rc == VERR_INTERRUPTED)
            rc = VERR_TIMEOUT;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtVfsStdFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    uint64_t offActual;
    int rc = RTFileSeek(pThis->hFile, 0, RTFILE_SEEK_CURRENT, &offActual);
    if (RT_SUCCESS(rc))
        *poffActual = (RTFOFF)offActual;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnSkip}
 */
static DECLCALLBACK(int) rtVfsStdFile_Skip(void *pvThis, RTFOFF cb)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    uint64_t offIgnore;
    return RTFileSeek(pThis->hFile, cb, RTFILE_SEEK_CURRENT, &offIgnore);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    if (fMask != ~RTFS_TYPE_MASK)
    {
#if 0
        RTFMODE fCurMode;
        int rc = RTFileGetMode(pThis->hFile, &fCurMode);
        if (RT_FAILURE(rc))
            return rc;
        fMode |= ~fMask & fCurMode;
#else
        RTFSOBJINFO ObjInfo;
        int rc = RTFileQueryInfo(pThis->hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
            return rc;
        fMode |= ~fMask & ObjInfo.Attr.fMode;
#endif
    }
    return RTFileSetMode(pThis->hFile, fMode);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                               PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileSetTimes(pThis->hFile, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
#if 0
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileSetOwner(pThis->hFile, uid, gid);
#else
    NOREF(pvThis); NOREF(uid); NOREF(gid);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtVfsStdFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTVFSSTDFILE pThis     = (PRTVFSSTDFILE)pvThis;
    uint64_t      offActual = 0;
    int rc = RTFileSeek(pThis->hFile, offSeek, uMethod, &offActual);
    if (RT_SUCCESS(rc))
        *poffActual = offActual;
    return rc;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtVfsStdFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileGetSize(pThis->hFile, pcbFile);
}


/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtVfsStdFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "StdFile",
            rtVfsStdFile_Close,
            rtVfsStdFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        0,
        rtVfsStdFile_Read,
        rtVfsStdFile_Write,
        rtVfsStdFile_Flush,
        rtVfsStdFile_PollOne,
        rtVfsStdFile_Tell,
        rtVfsStdFile_Skip,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtVfsStdFile_SetMode,
        rtVfsStdFile_SetTimes,
        rtVfsStdFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtVfsStdFile_Seek,
    rtVfsStdFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


/**
 * Internal worker for RTVfsFileFromRTFile and RTVfsFileOpenNormal.
 *
 * @returns IRPT status code.
 * @param   hFile               The IPRT file handle.
 * @param   fOpen               The RTFILE_O_XXX flags.
 * @param   fLeaveOpen          Whether to leave it open or close it.
 * @param   phVfsFile           Where to return the handle.
 */
static int rtVfsFileFromRTFile(RTFILE hFile, uint64_t fOpen, bool fLeaveOpen, PRTVFSFILE phVfsFile)
{
    PRTVFSSTDFILE   pThis;
    RTVFSFILE       hVfsFile;
    int rc = RTVfsNewFile(&g_rtVfsStdFileOps, sizeof(RTVFSSTDFILE), fOpen, NIL_RTVFS, NIL_RTVFSLOCK,
                          &hVfsFile, (void **)&pThis);
    if (RT_FAILURE(rc))
        return rc;

    pThis->hFile        = hFile;
    pThis->fLeaveOpen   = fLeaveOpen;
    *phVfsFile = hVfsFile;
    return VINF_SUCCESS;
}


RTDECL(int) RTVfsFileFromRTFile(RTFILE hFile, uint64_t fOpen, bool fLeaveOpen, PRTVFSFILE phVfsFile)
{
    /*
     * Check the handle validity.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTFileQueryInfo(hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Set up some fake fOpen flags if necessary and create a VFS file handle.
     */
    if (!fOpen)
        fOpen = RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN_CREATE;

    return rtVfsFileFromRTFile(hFile, fOpen, fLeaveOpen, phVfsFile);
}


RTDECL(int) RTVfsFileOpenNormal(const char *pszFilename, uint64_t fOpen, PRTVFSFILE phVfsFile)
{
    /*
     * Open the file the normal way and pass it to RTVfsFileFromRTFile.
     */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, fOpen);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create a VFS file handle.
         */
        rc = rtVfsFileFromRTFile(hFile, fOpen, false /*fLeaveOpen*/, phVfsFile);
        if (RT_FAILURE(rc))
            RTFileClose(hFile);
    }
    return rc;
}


RTDECL(int) RTVfsIoStrmFromRTFile(RTFILE hFile, uint64_t fOpen, bool fLeaveOpen, PRTVFSIOSTREAM phVfsIos)
{
    RTVFSFILE hVfsFile;
    int rc = RTVfsFileFromRTFile(hFile, fOpen, fLeaveOpen, &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        *phVfsIos = RTVfsFileToIoStream(hVfsFile);
        RTVfsFileRelease(hVfsFile);
    }
    return rc;
}


RTDECL(int) RTVfsIoStrmOpenNormal(const char *pszFilename, uint64_t fOpen, PRTVFSIOSTREAM phVfsIos)
{
    RTVFSFILE hVfsFile;
    int rc = RTVfsFileOpenNormal(pszFilename, fOpen, &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        *phVfsIos = RTVfsFileToIoStream(hVfsFile);
        RTVfsFileRelease(hVfsFile);
    }
    return rc;
}



/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainStdFile_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                    PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_INVALID)
        return VERR_VFS_CHAIN_MUST_BE_FIRST_ELEMENT;
    if (   pElement->enmType != RTVFSOBJTYPE_FILE
        && pElement->enmType != RTVFSOBJTYPE_IO_STREAM)
        return VERR_VFS_CHAIN_ONLY_FILE_OR_IOS;

    /*
     * Join common cause with the 'open' provider.
     */
    return RTVfsChainValidateOpenFileOrIoStream(pSpec, pElement, poffError, pErrInfo);
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainStdFile_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                       PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                       PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError, pErrInfo);
    AssertReturn(hPrevVfsObj == NIL_RTVFSOBJ, VERR_VFS_CHAIN_IPE);

    RTVFSFILE hVfsFile;
    int rc = RTVfsFileOpenNormal(pElement->paArgs[0].psz, pElement->uProvider, &hVfsFile);
    if (RT_SUCCESS(rc))
    {
        *phVfsObj = RTVfsObjFromFile(hVfsFile);
        RTVfsFileRelease(hVfsFile);
        if (*phVfsObj != NIL_RTVFSOBJ)
            return VINF_SUCCESS;
        rc = VERR_VFS_CHAIN_CAST_FAILED;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainStdFile_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                            PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                            PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (strcmp(pElement->paArgs[0].psz, pReuseElement->paArgs[0].psz) == 0)
        if (pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider)
            return true;
    return false;
}


/** VFS chain element 'file'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainStdFileReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "stdfile",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a real file, providing either a file or an I/O stream object. Initial element.\n"
                                "First argument is the filename path.\n"
                                "Second argument is access mode, optional: r, w, rw.\n"
                                "Third argument is open disposition, optional: create, create-replace, open, open-create, open-append, open-truncate.\n"
                                "Forth argument is file sharing, optional: nr, nw, nrw, d.",
    /* pfnValidate = */         rtVfsChainStdFile_Validate,
    /* pfnInstantiate = */      rtVfsChainStdFile_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainStdFile_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainStdFileReg, rtVfsChainStdFileReg);

