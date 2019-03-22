/* $Id$ */
/** @file
 * IPRT - Path Conversions, Darwin.
 *
 * On darwin path names on the disk are decomposed using normalization
 * form D (NFD).  Since this behavior is unique for the Mac, we will precompose
 * the path name strings we get from the XNU kernel.
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
#define LOG_GROUP RTLOGGROUP_PATH
#include <iprt/assert.h>
#include <iprt/string.h>
#include "internal/iprt.h"

#include "internal/path.h"


int rtPathToNative(const char **ppszNativePath, const char *pszPath, const char *pszBasePath)
{
    /** @todo We should decompose the string here, but the file system will do
     *        that for us if we don't, so why bother. */
    *ppszNativePath = (char *)pszPath;
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return VINF_SUCCESS;
}


void rtPathFreeNative(char const *pszNativePath, const char *pszPath)
{
    Assert(pszNativePath == pszPath || !pszNativePath);
    NOREF(pszNativePath);
    NOREF(pszPath);
}


int rtPathFromNative(char const **ppszPath, const char *pszNativePath, const char *pszBasePath)
{
    /** @todo We must compose the codepoints in the string here.  We get file names
     *        in normalization form D so we'll end up with normalization form C
     *        whatever approach we take. */
    int rc = RTStrValidateEncodingEx(pszNativePath, RTSTR_MAX, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
        *ppszPath = pszNativePath;
    else
        *ppszPath = NULL;
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return rc;
}


void rtPathFreeIprt(const char *pszPath, const char *pszNativePath)
{
    Assert(pszPath == pszNativePath || !pszPath);
    NOREF(pszPath); NOREF(pszNativePath);
}


int rtPathFromNativeCopy(char *pszPath, size_t cbPath, const char *pszNativePath, const char *pszBasePath)
{
    /** @todo We must compose the codepoints in the string here.  We get file names
     *        in normalization form D so we'll end up with normalization form C
     *        whatever approach we take. */
    int rc = RTStrValidateEncodingEx(pszNativePath, RTSTR_MAX, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
        rc = RTStrCopyEx(pszPath, cbPath, pszNativePath, RTSTR_MAX);
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return rc;
}


int rtPathFromNativeDup(char **ppszPath, const char *pszNativePath, const char *pszBasePath)
{
    /** @todo We must compose the codepoints in the string here.  We get file names
     *        in normalization form D so we'll end up with normalization form C
     *        whatever approach we take. */
    int rc = RTStrValidateEncodingEx(pszNativePath, RTSTR_MAX, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
        rc = RTStrDupEx(ppszPath, pszNativePath);
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return rc;
}

