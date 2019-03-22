/** $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions,
 * Create a PID file.
 */

/*
 * Copyright (C) 2015-2016 Oracle Corporation
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
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include "VBoxGuestR3LibInternal.h"

/**
 * Creates a PID File and returns the open file descriptor.
 *
 * On DOS based system, file sharing (deny write) is used for locking the PID
 * file.
 *
 * On Unix-y systems, an exclusive advisory lock is used for locking the PID
 * file since the file sharing support is usually missing there.
 *
 * This API will overwrite any existing PID Files without a lock on them, on the
 * assumption that they are stale files which an old process did not properly
 * clean up.
 *
 * @returns IPRT status code.
 * @param   pszPath  The path and filename to create the PID File under
 * @param   phFile   Where to store the file descriptor of the open (and locked
 *                   on Unix-y systems) PID File. On failure, or if another
 *                   process owns the PID File, this will be set to NIL_RTFILE.
 */
VBGLR3DECL(int) VbglR3PidFile(const char *pszPath, PRTFILE phFile)
{
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phFile, VERR_INVALID_PARAMETER);
    *phFile = NIL_RTFILE;

    RTFILE hPidFile;
    int rc = RTFileOpen(&hPidFile, pszPath,
                        RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_WRITE
                        | (0644 << RTFILE_O_CREATE_MODE_SHIFT));
    if (RT_SUCCESS(rc))
    {
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
        /** @todo using size 0 for locking means lock all on Posix.
         * We should adopt this as our convention too, or something
         * similar. */
        rc = RTFileLock(hPidFile, RTFILE_LOCK_WRITE, 0, 0);
        if (RT_FAILURE(rc))
            RTFileClose(hPidFile);
        else
#endif
        {
            char szBuf[256];
            size_t cbPid = RTStrPrintf(szBuf, sizeof(szBuf), "%d\n",
                                       RTProcSelf());
            RTFileWrite(hPidFile, szBuf, cbPid, NULL);
            *phFile = hPidFile;
        }
    }
    return rc;
}


/**
 * Close and remove an open PID File.
 *
 * @param  pszPath  The path to the PID File,
 * @param  hFile    The handle for the file. NIL_RTFILE is ignored as usual.
 */
VBGLR3DECL(void) VbglR3ClosePidFile(const char *pszPath, RTFILE hFile)
{
    AssertPtrReturnVoid(pszPath);
    if (hFile != NIL_RTFILE)
    {
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        RTFileWriteAt(hFile, 0, "-1", 2, NULL);
#else
        RTFileDelete(pszPath);
#endif
        RTFileClose(hFile);
    }
}

