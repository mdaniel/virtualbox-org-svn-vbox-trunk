/* $Id$ */
/** @file
 * IPRT - Path Conversions, generic pass through.
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
#define LOG_GROUP RTLOGGROUP_PATH
#include "internal/iprt.h"
#include "internal/path.h"

#include <iprt/assert.h>
#include <iprt/string.h>


int rtPathToNative(char const **ppszNativePath, const char *pszPath, const char *pszBasePath)
{
    *ppszNativePath = pszPath;
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return VINF_SUCCESS;
}


void rtPathFreeNative(char const *pszNativePath, const char *pszPath)
{
    Assert(pszNativePath == pszPath || !pszNativePath);
    NOREF(pszNativePath); NOREF(pszPath);
}


int rtPathFromNative(const char **ppszPath, const char *pszNativePath, const char *pszBasePath)
{
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
    int rc = RTStrValidateEncodingEx(pszNativePath, RTSTR_MAX, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
        rc = RTStrCopy(pszPath, cbPath, pszNativePath);
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return rc;
}


int rtPathFromNativeDup(char **ppszPath, const char *pszNativePath, const char *pszBasePath)
{
    int rc = RTStrValidateEncodingEx(pszNativePath, RTSTR_MAX, 0 /*fFlags*/);
    if (RT_SUCCESS(rc))
        rc = RTStrDupEx(ppszPath, pszNativePath);
    NOREF(pszBasePath); /* We don't query the FS for codeset preferences. */
    return rc;
}

