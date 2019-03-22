/* $Id$ */
/** @file
 * IPRT - RTFileCopyEx, generic implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/file.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>


RTDECL(int) RTFileCopyEx(const char *pszSrc, const char *pszDst, uint32_t fFlags, PFNRTPROGRESS pfnProgress, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("pszSrc=%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszSrc, ("pszSrc=%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pszDst), ("pszDst=%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszDst, ("pszDst=%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!pfnProgress || VALID_PTR(pfnProgress), ("pfnProgress=%p\n", pfnProgress), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fFlags & ~RTFILECOPY_FLAGS_MASK), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Open the files.
     */
    RTFILE FileSrc;
    int rc = RTFileOpen(&FileSrc, pszSrc,
                        RTFILE_O_READ | RTFILE_O_OPEN
                        | (fFlags & RTFILECOPY_FLAGS_NO_SRC_DENY_WRITE ? RTFILE_O_DENY_NONE : RTFILE_O_DENY_WRITE));
    if (RT_SUCCESS(rc))
    {
        RTFILE FileDst;
        rc = RTFileOpen(&FileDst, pszDst,
                        RTFILE_O_WRITE | RTFILE_O_CREATE
                        | (fFlags & RTFILECOPY_FLAGS_NO_DST_DENY_WRITE ? RTFILE_O_DENY_NONE : RTFILE_O_DENY_WRITE));
        if (RT_SUCCESS(rc))
        {
            /*
             * Call the ByHandles version and let it do the job.
             */
            rc = RTFileCopyByHandlesEx(FileSrc, FileDst, pfnProgress, pvUser);

            /*
             * Close the files regardless of the result.
             * Don't bother cleaning up or anything like that.
             */
            int rc2 = RTFileClose(FileDst);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }

        int rc2 = RTFileClose(FileSrc);
        AssertRC(rc2);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}

