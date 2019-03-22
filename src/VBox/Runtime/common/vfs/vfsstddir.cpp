/* $Id$ */
/** @file
 * IPRT - Virtual File System, Standard Directory Implementation.
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
#define LOG_GROUP RTLOGGROUP_VFS
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/path.h>
#include <iprt/string.h>

#define RTDIR_AGNOSTIC
#include "internal/dir.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private data of a standard directory.
 */
typedef struct RTVFSSTDDIR
{
    /** The directory handle. */
    PRTDIR          hDir;
    /** Whether to leave the handle open when the VFS handle is closed. */
    bool            fLeaveOpen;
    /** Open flags, RTDIR_F_XXX. */
    uint32_t        fFlags;
    /** Handle to the director so we can make sure it sticks around for symbolic
     * link objects. */
    RTVFSDIR        hSelf;
} RTVFSSTDDIR;
/** Pointer to the private data of a standard directory. */
typedef RTVFSSTDDIR *PRTVFSSTDDIR;


/**
 * Private data of a standard symbolic link.
 */
typedef struct RTVFSSTDSYMLINK
{
    /** Pointer to the VFS directory where the symbolic link lives . */
    PRTVFSSTDDIR    pDir;
    /** The symbolic link name. */
    char            szSymlink[RT_FLEXIBLE_ARRAY];
} RTVFSSTDSYMLINK;
/** Pointer to the private data of a standard symbolic link. */
typedef RTVFSSTDSYMLINK *PRTVFSSTDSYMLINK;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int) rtVfsStdDir_OpenDir(void *pvThis, const char *pszSubDir, uint32_t fFlags, PRTVFSDIR phVfsDir);
static DECLCALLBACK(int) rtVfsStdDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink);
static DECLCALLBACK(int) rtVfsStdDir_QueryEntryInfo(void *pvThis, const char *pszEntry,
                                                    PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);
static int rtVfsDirFromRTDir(PRTDIR hDir, uint32_t fFlags, bool fLeaveOpen, PRTVFSDIR phVfsDir);



/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfsStdSym_Close(void *pvThis)
{
    PRTVFSSTDSYMLINK pThis = (PRTVFSSTDSYMLINK)pvThis;
    RTVfsDirRelease(pThis->pDir->hSelf);
    pThis->pDir = NULL;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfsStdSym_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDSYMLINK pThis = (PRTVFSSTDSYMLINK)pvThis;
    return rtVfsStdDir_QueryEntryInfo(pThis->pDir, pThis->szSymlink, pObjInfo, enmAddAttr);
}

/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtVfsStdSym_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis); NOREF(fMode); NOREF(fMask);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtVfsStdSym_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis); NOREF(pAccessTime); NOREF(pModificationTime); NOREF(pChangeTime); NOREF(pBirthTime);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtVfsStdSym_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis); NOREF(uid); NOREF(gid);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSSYMLINKOPS,pfnRead}
 */
static DECLCALLBACK(int) rtVfsStdSym_Read(void *pvThis, char *pszTarget, size_t cbTarget)
{
    PRTVFSSTDSYMLINK pThis = (PRTVFSSTDSYMLINK)pvThis;
    return RTDirRelSymlinkRead(pThis->pDir->hDir, pThis->szSymlink, pszTarget, cbTarget, 0 /*fRead*/);
}


/**
 * Symbolic operations for standard directory.
 */
static const RTVFSSYMLINKOPS g_rtVfsStdSymOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_SYMLINK,
        "StdSymlink",
        rtVfsStdSym_Close,
        rtVfsStdSym_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSSYMLINKOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSSYMLINKOPS, Obj) - RT_OFFSETOF(RTVFSSYMLINKOPS, ObjSet),
        rtVfsStdSym_SetMode,
        rtVfsStdSym_SetTimes,
        rtVfsStdSym_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtVfsStdSym_Read,
    RTVFSSYMLINKOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfsStdDir_Close(void *pvThis)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;

    int rc;
    if (!pThis->fLeaveOpen)
        rc = RTDirClose(pThis->hDir);
    else
        rc = VINF_SUCCESS;
    pThis->hDir = NULL;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfsStdDir_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    return RTDirQueryInfo(pThis->hDir, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtVfsStdDir_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    if (fMask != ~RTFS_TYPE_MASK)
    {
        RTFSOBJINFO ObjInfo;
        int rc = RTDirQueryInfo(pThis->hDir, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
            return rc;
        fMode |= ~fMask & ObjInfo.Attr.fMode;
    }
    //RTPathSetMode
    //return RTFileSetMode(pThis->hDir, fMode);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtVfsStdDir_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                              PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    return RTDirSetTimes(pThis->hDir, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtVfsStdDir_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    return RTDirRelPathSetOwner(pThis->hDir, ".", uid, gid, RTPATH_F_FOLLOW_LINK);
}



/**
 * @interface_method_impl{RTVFSDIROPS,pfnTraversalOpen}
 */
static DECLCALLBACK(int) rtVfsStdDir_TraversalOpen(void *pvThis, const char *pszEntry, PRTVFSDIR phVfsDir,
                                                   PRTVFSSYMLINK phVfsSymlink, PRTVFS phVfsMounted)
{
    /* No union mounting or mount points here (yet). */
    if (phVfsMounted)
        *phVfsMounted = NIL_RTVFS;

    int rc;
    if (phVfsDir || phVfsSymlink)
    {
        if (phVfsDir)
            *phVfsDir = NIL_RTVFSDIR;
        if (phVfsSymlink)
            *phVfsSymlink = NIL_RTVFSSYMLINK;

        RTFSOBJINFO ObjInfo;
        rc = rtVfsStdDir_QueryEntryInfo(pvThis, pszEntry, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_SUCCESS(rc))
        {
            switch (ObjInfo.Attr.fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                    if (phVfsDir)
                        rc = rtVfsStdDir_OpenDir(pvThis, pszEntry, 0, phVfsDir);
                    else
                        rc = VERR_NOT_SYMLINK;
                    break;

                case RTFS_TYPE_SYMLINK:
                    if (phVfsSymlink)
                        rc = rtVfsStdDir_OpenSymlink(pvThis, pszEntry, phVfsSymlink);
                    else
                        rc = VERR_NOT_A_DIRECTORY;
                    break;

                default:
                    rc = phVfsDir ? VERR_NOT_A_DIRECTORY : VERR_NOT_SYMLINK;
                    break;
            }
        }
    }
    else
        rc = VERR_PATH_NOT_FOUND;

    LogFlow(("rtVfsStdDir_TraversalOpen: %s -> %Rrc\n", pszEntry, rc));
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenFile}
 */
static DECLCALLBACK(int) rtVfsStdDir_OpenFile(void *pvThis, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    RTFILE       hFile;
    int rc = RTDirRelFileOpen(pThis->hDir, pszFilename, fOpen, &hFile);
    if (RT_SUCCESS(rc))
    {
        rc = RTVfsFileFromRTFile(hFile, fOpen, false /*fLeaveOpen*/, phVfsFile);
        if (RT_FAILURE(rc))
            RTFileClose(hFile);
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenDir}
 */
static DECLCALLBACK(int) rtVfsStdDir_OpenDir(void *pvThis, const char *pszSubDir, uint32_t fFlags, PRTVFSDIR phVfsDir)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    /** @todo subdir open flags */
    PRTDIR hSubDir;
    int rc = RTDirRelDirOpenFiltered(pThis->hDir, pszSubDir, RTDIRFILTER_NONE, fFlags, &hSubDir);
    if (RT_SUCCESS(rc))
    {
        rc = rtVfsDirFromRTDir(hSubDir, fFlags, false, phVfsDir);
        if (RT_FAILURE(rc))
            RTDirClose(hSubDir);
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtVfsStdDir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    int rc;
    if (!phVfsDir)
        rc = RTDirRelDirCreate(pThis->hDir, pszSubDir, fMode, 0 /* fFlags */, NULL);
    else
    {
        PRTDIR hSubDir;
        rc = RTDirRelDirCreate(pThis->hDir, pszSubDir, fMode, 0 /* fFlags */, &hSubDir);
        if (RT_SUCCESS(rc))
        {
            /** @todo subdir open flags...   */
            rc = rtVfsDirFromRTDir(hSubDir, 0, false, phVfsDir);
            if (RT_FAILURE(rc))
                RTDirClose(hSubDir);
        }
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtVfsStdDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RTFSOBJINFO ObjInfo;
    int rc = rtVfsStdDir_QueryEntryInfo(pvThis, pszSymlink, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_SYMLINK(ObjInfo.Attr.fMode))
        {
            PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
            uint32_t cRefs = RTVfsDirRetain(pThis->hSelf);
            if (cRefs != UINT32_MAX)
            {
                PRTVFSSTDSYMLINK pNewSymlink;
                size_t           cchSymlink = strlen(pszSymlink);
                rc = RTVfsNewSymlink(&g_rtVfsStdSymOps, RT_UOFFSETOF(RTVFSSTDSYMLINK, szSymlink[cchSymlink + 1]),
                                     NIL_RTVFS, NIL_RTVFSLOCK, phVfsSymlink, (void **)&pNewSymlink);
                if (RT_SUCCESS(rc))
                {
                    memcpy(pNewSymlink->szSymlink, pszSymlink, cchSymlink);
                    pNewSymlink->szSymlink[cchSymlink] = '\0';
                    pNewSymlink->pDir = pThis;
                    return VINF_SUCCESS;
                }

                RTVfsDirRelease(pThis->hSelf);
            }
            else
                rc = VERR_INTERNAL_ERROR_2;
        }
        else
            rc = VERR_NOT_SYMLINK;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtVfsStdDir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                   RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    int rc = RTDirRelSymlinkCreate(pThis->hDir, pszSymlink, pszTarget, enmType, 0 /*fCreate*/);
    if (RT_SUCCESS(rc))
    {
        if (!phVfsSymlink)
            return VINF_SUCCESS;
        return rtVfsStdDir_OpenSymlink(pThis, pszSymlink, phVfsSymlink);
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnQueryEntryInfo}
 */
static DECLCALLBACK(int) rtVfsStdDir_QueryEntryInfo(void *pvThis, const char *pszEntry,
                                                    PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    return RTDirRelPathQueryInfo(pThis->hDir, pszEntry, pObjInfo, enmAddAttr, RTPATH_F_ON_LINK);
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtVfsStdDir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    if (fType != 0)
    {
        RTFSOBJINFO ObjInfo;
        int rc = rtVfsStdDir_QueryEntryInfo(pThis, pszEntry, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
            return rc;
        if ((fType & RTFS_TYPE_MASK) != (ObjInfo.Attr.fMode & RTFS_TYPE_MASK))
            return VERR_WRONG_TYPE;
    }
    return RTDirRelPathUnlink(pThis->hDir, pszEntry, 0 /*fUnlink*/);
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRenameEntry}
 */
static DECLCALLBACK(int) rtVfsStdDir_RenameEntry(void *pvThis, const char *pszEntry, RTFMODE fType, const char *pszNewName)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    if (fType != 0)
    {
        RTFSOBJINFO ObjInfo;
        int rc = rtVfsStdDir_QueryEntryInfo(pThis, pszEntry, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
            return rc;
        if ((fType & RTFS_TYPE_MASK) != (ObjInfo.Attr.fMode & RTFS_TYPE_MASK))
            return VERR_WRONG_TYPE;
    }

    /** @todo  RTVFSDIROPS::pfnRenameEntry doesn't really work, this must move to
     *         file system level. */
    return RTDirRelPathRename(pThis->hDir, pszEntry, pThis->hDir, pszNewName,
                              RTPATHRENAME_FLAGS_NO_SYMLINKS | RTPATHRENAME_FLAGS_NO_REPLACE);
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtVfsStdDir_RewindDir(void *pvThis)
{
    NOREF(pvThis);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtVfsStdDir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDDIR pThis = (PRTVFSSTDDIR)pvThis;
    return RTDirReadEx(pThis->hDir, pDirEntry, pcbDirEntry, enmAddAttr, RTPATH_F_ON_LINK);
}


/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSDIROPS) g_rtVfsStdDirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_DIR,
        "StdDir",
        rtVfsStdDir_Close,
        rtVfsStdDir_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSDIROPS, Obj) - RT_OFFSETOF(RTVFSDIROPS, ObjSet),
        rtVfsStdDir_SetMode,
        rtVfsStdDir_SetTimes,
        rtVfsStdDir_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtVfsStdDir_TraversalOpen,
    rtVfsStdDir_OpenFile,
    rtVfsStdDir_OpenDir,
    rtVfsStdDir_CreateDir,
    rtVfsStdDir_OpenSymlink,
    rtVfsStdDir_CreateSymlink,
    rtVfsStdDir_QueryEntryInfo,
    rtVfsStdDir_UnlinkEntry,
    rtVfsStdDir_RenameEntry,
    rtVfsStdDir_RewindDir,
    rtVfsStdDir_ReadDir,
    RTVFSDIROPS_VERSION
};


/**
 * Internal worker for RTVfsDirFromRTDir and RTVfsDirOpenNormal.
 *
 * @returns IRPT status code.
 * @param   hDir                The IPRT directory handle.
 * @param   fOpen               Reserved for future.
 * @param   fLeaveOpen          Whether to leave it open or close it.
 * @param   phVfsDir            Where to return the handle.
 */
static int rtVfsDirFromRTDir(PRTDIR hDir, uint32_t fFlags, bool fLeaveOpen, PRTVFSDIR phVfsDir)
{
    PRTVFSSTDDIR    pThis;
    RTVFSDIR        hVfsDir;
    int rc = RTVfsNewDir(&g_rtVfsStdDirOps, sizeof(RTVFSSTDDIR), 0 /*fFlags*/, NIL_RTVFS, NIL_RTVFSLOCK,
                         &hVfsDir, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hDir         = hDir;
        pThis->fLeaveOpen   = fLeaveOpen;
        pThis->fFlags       = fFlags;
        pThis->hSelf        = hVfsDir;

        *phVfsDir = hVfsDir;
        return VINF_SUCCESS;
    }
    return rc;
}


RTDECL(int) RTVfsDirFromRTDir(PRTDIR hDir, bool fLeaveOpen, PRTVFSDIR phVfsDir)
{
    AssertReturn(RTDirIsValid(hDir), VERR_INVALID_HANDLE);
    return rtVfsDirFromRTDir(hDir, hDir->fFlags, fLeaveOpen, phVfsDir);
}


RTDECL(int) RTVfsDirOpenNormal(const char *pszPath, uint32_t fFlags, PRTVFSDIR phVfsDir)
{
    /*
     * Open the file the normal way and pass it to RTVfsFileFromRTFile.
     */
    PRTDIR hDir;
    int rc = RTDirOpenFiltered(&hDir, pszPath, RTDIRFILTER_NONE, fFlags);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create a VFS file handle.
         */
        rc = rtVfsDirFromRTDir(hDir, fFlags, false /*fLeaveOpen*/, phVfsDir);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;

        RTDirClose(hDir);
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainStdDir_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                   PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_INVALID)
        return VERR_VFS_CHAIN_MUST_BE_FIRST_ELEMENT;
    if (pElement->enmType != RTVFSOBJTYPE_DIR)
        return VERR_VFS_CHAIN_ONLY_DIR;
    if (pElement->cArgs < 1)
        return VERR_VFS_CHAIN_AT_LEAST_ONE_ARG;

    /*
     * Parse flag arguments if any, storing them in the element.
     */
    uint32_t fFlags = 0;
    for (uint32_t i = 1; i < pElement->cArgs; i++)
        if (strcmp(pElement->paArgs[i].psz, "deny-ascent") == 0)
            fFlags |= RTDIR_F_DENY_ASCENT;
        else if (strcmp(pElement->paArgs[i].psz, "allow-ascent") == 0)
            fFlags &= ~RTDIR_F_DENY_ASCENT;
        else
        {
            *poffError = pElement->paArgs[i].offSpec;
            return RTErrInfoSetF(pErrInfo, VERR_VFS_CHAIN_INVALID_ARGUMENT, "Unknown flag argument: %s", pElement->paArgs[i].psz);
        }
    pElement->uProvider = fFlags;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainStdDir_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                      PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                      PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError, pErrInfo);
    AssertReturn(hPrevVfsObj == NIL_RTVFSOBJ, VERR_VFS_CHAIN_IPE);

    RTVFSDIR hVfsDir;
    int rc = RTVfsDirOpenNormal(pElement->paArgs[0].psz, (uint32_t)pElement->uProvider, &hVfsDir);
    if (RT_SUCCESS(rc))
    {
        *phVfsObj = RTVfsObjFromDir(hVfsDir);
        RTVfsDirRelease(hVfsDir);
        if (*phVfsObj != NIL_RTVFSOBJ)
            return VINF_SUCCESS;
        rc = VERR_VFS_CHAIN_CAST_FAILED;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainStdDir_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
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
static RTVFSCHAINELEMENTREG g_rtVfsChainStdDirReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "stddir",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a real directory. Initial element.\n"
                                "Takes zero or more flag arguments: deny-ascent, allow-ascent",
    /* pfnValidate = */         rtVfsChainStdDir_Validate,
    /* pfnInstantiate = */      rtVfsChainStdDir_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainStdDir_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainStdDirReg, rtVfsChainStdDirReg);

