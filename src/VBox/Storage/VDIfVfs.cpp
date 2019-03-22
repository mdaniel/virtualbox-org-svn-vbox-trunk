/* $Id$ */
/** @file
 * Virtual Disk Image (VDI), I/O interface to IPRT VFS I/O stream glue.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/sg.h>
#include <iprt/vfslowlevel.h>
#include <iprt/poll.h>
#include <VBox/vd.h>
#include <VBox/vd-ifs-internal.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * The internal data of an VD I/O to VFS file or I/O stream wrapper.
 */
typedef struct VDIFVFSIOSFILE
{
    /** The VD I/O interface we prefer wrap.
     *  Can be NULL, in which case pVDIfsIoInt must be valid. */
    PVDINTERFACEIO      pVDIfsIo;
    /** The VD I/O interface we alternatively can wrap.
        Can be NULL, in which case pVDIfsIo must be valid. */
    PVDINTERFACEIOINT   pVDIfsIoInt;
    /** User pointer to pass to the VD I/O interface methods. */
    PVDIOSTORAGE        pStorage;
    /** The current stream position. */
    RTFOFF              offCurPos;
} VDIFVFSIOSFILE;
/** Pointer to a the internal data of a DVM volume file. */
typedef VDIFVFSIOSFILE *PVDIFVFSIOSFILE;



/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) vdIfVfsIos_Close(void *pvThis)
{
    /* We don't close anything. */
    RT_NOREF1(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) vdIfVfsIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis);
    NOREF(pObjInfo);
    NOREF(enmAddAttr);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) vdIfVfsIos_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;
    Assert(pSgBuf->cSegs == 1); NOREF(fBlocking);
    Assert(off >= -1);

    /*
     * This may end up being a little more complicated, esp. wrt VERR_EOF.
     */
    if (off == -1)
        off = pThis->offCurPos;
    int rc;
    if (pThis->pVDIfsIo)
        rc = vdIfIoFileReadSync(pThis->pVDIfsIo, pThis->pStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg, pcbRead);
    else
    {
        rc = vdIfIoIntFileReadSync(pThis->pVDIfsIoInt, (PVDIOSTORAGE)pThis->pStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg);
        if (pcbRead)
            *pcbRead = RT_SUCCESS(rc) ? pSgBuf->paSegs[0].cbSeg : 0;
    }
    if (RT_SUCCESS(rc))
    {
        size_t cbAdvance = pcbRead ? *pcbRead : pSgBuf->paSegs[0].cbSeg;
        pThis->offCurPos = off + cbAdvance;
        if (pcbRead && !cbAdvance)
            rc = VINF_EOF;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) vdIfVfsIos_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;
    Assert(pSgBuf->cSegs == 1); NOREF(fBlocking);
    Assert(off >= -1);

    /*
     * This may end up being a little more complicated, esp. wrt VERR_EOF.
     */
    if (off == -1)
        off = pThis->offCurPos;
    int rc;
    if (pThis->pVDIfsIo)
        rc = vdIfIoFileWriteSync(pThis->pVDIfsIo, pThis->pStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg, pcbWritten);
    else
    {
        rc = vdIfIoIntFileWriteSync(pThis->pVDIfsIoInt, pThis->pStorage, off, pSgBuf[0].pvSegCur, pSgBuf->paSegs[0].cbSeg);
        if (pcbWritten)
            *pcbWritten = RT_SUCCESS(rc) ? pSgBuf->paSegs[0].cbSeg : 0;
    }
    if (RT_SUCCESS(rc))
        pThis->offCurPos = off + (pcbWritten ? *pcbWritten : pSgBuf->paSegs[0].cbSeg);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) vdIfVfsIos_Flush(void *pvThis)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;
    int rc;
    if (pThis->pVDIfsIo)
        rc = vdIfIoFileFlushSync(pThis->pVDIfsIo, pThis->pStorage);
    else
        rc = vdIfIoIntFileFlushSync(pThis->pVDIfsIoInt, pThis->pStorage);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) vdIfVfsIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;
    *poffActual = pThis->offCurPos;
    return VINF_SUCCESS;
}


/**
 * VFS I/O stream operations for a VD file or stream.
 */
DECL_HIDDEN_CONST(const RTVFSIOSTREAMOPS) g_vdIfVfsIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "VDIfIos",
        vdIfVfsIos_Close,
        vdIfVfsIos_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    RTVFSIOSTREAMOPS_FEAT_NO_SG,
    vdIfVfsIos_Read,
    vdIfVfsIos_Write,
    vdIfVfsIos_Flush,
    NULL /*PollOne*/,
    vdIfVfsIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,

};

VBOXDDU_DECL(int) VDIfCreateVfsStream(PVDINTERFACEIO pVDIfsIo, void *pvStorage, uint32_t fFlags, PRTVFSIOSTREAM phVfsIos)
{
    AssertPtrReturn(pVDIfsIo, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVfsIos, VERR_INVALID_POINTER);

    /*
     * Create the volume file.
     */
    RTVFSIOSTREAM hVfsIos;
    PVDIFVFSIOSFILE pThis;
    int rc = RTVfsNewIoStream(&g_vdIfVfsIosOps, sizeof(*pThis), fFlags,
                              NIL_RTVFS, NIL_RTVFSLOCK, &hVfsIos, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->pVDIfsIo     = pVDIfsIo;
        pThis->pVDIfsIoInt  = NULL;
        pThis->pStorage     = (PVDIOSTORAGE)pvStorage;
        pThis->offCurPos    = 0;

        *phVfsIos = hVfsIos;
        return VINF_SUCCESS;
    }

    return rc;
}



/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetMode}
 */
static DECLCALLBACK(int) vdIfVfsFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis);
    NOREF(fMode);
    NOREF(fMask);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) vdIfVfsFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                              PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis);
    NOREF(pAccessTime);
    NOREF(pModificationTime);
    NOREF(pChangeTime);
    NOREF(pBirthTime);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) vdIfVfsFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis);
    NOREF(uid);
    NOREF(gid);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) vdIfVfsFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;

    uint64_t cbFile;
    int rc;
    if (pThis->pVDIfsIo)
        rc = vdIfIoFileGetSize(pThis->pVDIfsIo, pThis->pStorage, &cbFile);
    else
        rc = vdIfIoIntFileGetSize(pThis->pVDIfsIoInt, pThis->pStorage, &cbFile);
    if (RT_FAILURE(rc))
        return rc;
    if (cbFile >= (uint64_t)RTFOFF_MAX)
        cbFile = RTFOFF_MAX;

    /* Recalculate the request to RTFILE_SEEK_BEGIN. */
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            break;
        case RTFILE_SEEK_CURRENT:
            offSeek += pThis->offCurPos;
            break;
        case RTFILE_SEEK_END:
            offSeek = cbFile + offSeek;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    /* Do limit checks. */
    if (offSeek < 0)
        offSeek = 0;
    else if (offSeek > (RTFOFF)cbFile)
        offSeek = cbFile;

    /* Apply and return. */
    pThis->offCurPos = offSeek;
    if (poffActual)
        *poffActual = offSeek;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) vdIfVfsFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PVDIFVFSIOSFILE pThis = (PVDIFVFSIOSFILE)pvThis;
    int rc;
    if (pThis->pVDIfsIo)
        rc = vdIfIoFileGetSize(pThis->pVDIfsIo, pThis->pStorage, pcbFile);
    else
        rc = vdIfIoIntFileGetSize(pThis->pVDIfsIoInt, pThis->pStorage, pcbFile);
    return rc;
}



/**
 * VFS file operations for a VD file.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_vdIfVfsFileOps =
{
    { /* I/O stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "VDIfFile",
            vdIfVfsIos_Close,
            vdIfVfsIos_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        vdIfVfsIos_Read,
        vdIfVfsIos_Write,
        vdIfVfsIos_Flush,
        NULL /*PollOne*/,
        vdIfVfsIos_Tell,
        NULL /*Skip*/,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_UOFFSETOF(RTVFSFILEOPS, ObjSet) - RT_UOFFSETOF(RTVFSFILEOPS, Stream.Obj),
        vdIfVfsFile_SetMode,
        vdIfVfsFile_SetTimes,
        vdIfVfsFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    vdIfVfsFile_Seek,
    vdIfVfsFile_QuerySize,
    NULL /*SetSize*/,
    NULL /*QueryMaxSize*/,
    RTVFSFILEOPS_VERSION,
};


VBOXDDU_DECL(int) VDIfCreateVfsFile(PVDINTERFACEIO pVDIfs, struct VDINTERFACEIOINT *pVDIfsInt, void *pvStorage, uint32_t fFlags, PRTVFSFILE phVfsFile)
{
    AssertReturn((pVDIfs != NULL) != (pVDIfsInt != NULL), VERR_INVALID_PARAMETER); /* Exactly one needs to be specified. */
    AssertPtrReturn(phVfsFile, VERR_INVALID_POINTER);

    /*
     * Create the volume file.
     */
    RTVFSFILE hVfsFile;
    PVDIFVFSIOSFILE pThis;
    int rc = RTVfsNewFile(&g_vdIfVfsFileOps, sizeof(*pThis), fFlags,
                          NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFile, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->pVDIfsIo     = pVDIfs;
        pThis->pVDIfsIoInt  = pVDIfsInt;
        pThis->pStorage     = (PVDIOSTORAGE)pvStorage;
        pThis->offCurPos    = 0;

        *phVfsFile = hVfsFile;
        return VINF_SUCCESS;
    }

    return rc;
}

