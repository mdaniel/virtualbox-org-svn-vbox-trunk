/* $Id$ */
/** @file
 * IPRT - RTPathSetMode, Native NT.
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
#define LOG_GROUP RTLOGGROUP_FILE
#include "internal-r3-nt.h"

#include <iprt/path.h>
#include <iprt/err.h>

#include "internal/fs.h"



RTDECL(int) RTPathSetMode(const char *pszPath, RTFMODE fMode)
{
    fMode = rtFsModeNormalize(fMode, pszPath, 0);
    AssertReturn(rtFsModeIsValidPermissions(fMode), VERR_INVALID_FMODE);

    /*
     * Convert and normalize the path.
     */
    UNICODE_STRING NtName;
    HANDLE         hRootDir;
    int rc = RTNtPathFromWinUtf8(&NtName, &hRootDir, pszPath);
    if (RT_SUCCESS(rc))
    {
        HANDLE              hPath   = RTNT_INVALID_HANDLE_VALUE;
        IO_STATUS_BLOCK     Ios     = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        OBJECT_ATTRIBUTES   ObjAttr;
        InitializeObjectAttributes(&ObjAttr, &NtName, 0 /*fAttrib*/, hRootDir, NULL);

        ULONG fOpenOptions = FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT;
        //if (fFlags & RTPATH_F_ON_LINK)
        //    fOpenOptions |= FILE_OPEN_REPARSE_POINT;
        NTSTATUS rcNt = NtCreateFile(&hPath,
                                     FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
                                     &ObjAttr,
                                     &Ios,
                                     NULL /*AllocationSize*/,
                                     FILE_ATTRIBUTE_NORMAL,
                                     FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     FILE_OPEN,
                                     fOpenOptions,
                                     NULL /*EaBuffer*/,
                                     0 /*EaLength*/);
        if (NT_SUCCESS(rcNt))
        {
            rc = rtNtFileSetModeWorker(hPath, fMode);

            rcNt = NtClose(hPath);
            if (!NT_SUCCESS(rcNt) && RT_SUCCESS(rc))
                rc = RTErrConvertFromNtStatus(rcNt);
        }
        else
            rc = RTErrConvertFromNtStatus(rcNt);

        RTNtPathFree(&NtName, &hRootDir);
    }
    return rc;
}

