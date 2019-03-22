/* $Id$ */
/** @file
 * Shared Folders Service - guest/host path convertion and verification.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_FOLDERS
#include <iprt/path.h>
#include <iprt/string.h>


#if defined(RT_OS_WINDOWS)
static void vbsfPathResolveRelative(char *pszPathBegin)
{
    char *pszCur = pszPathBegin;
    char * const pszTop = pszCur;

    /*
     * Get rid of double dot path components by evaluating them.
     */
    for (;;)
    {
        char const chFirst = pszCur[0];
        if (   chFirst == '.'
            && pszCur[1] == '.'
            && (!pszCur[2] || pszCur[2] == RTPATH_SLASH))
        {
            /* rewind to the previous component if any */
            char *pszPrev = pszCur;
            if ((uintptr_t)pszPrev > (uintptr_t)pszTop)
            {
                pszPrev--;
                while (   (uintptr_t)pszPrev > (uintptr_t)pszTop
                       && pszPrev[-1] != RTPATH_SLASH)
                    pszPrev--;
            }
            if (!pszCur[2])
            {
                if (pszPrev != pszTop)
                    pszPrev[-1] = '\0';
                else
                    *pszPrev = '\0';
                break;
            }
            Assert(pszPrev[-1] == RTPATH_SLASH);
            memmove(pszPrev, pszCur + 3, strlen(pszCur + 3) + 1);
            pszCur = pszPrev - 1;
        }
        else if (   chFirst == '.'
                 && (!pszCur[1] || pszCur[1] == RTPATH_SLASH))
        {
            /* remove unnecessary '.' */
            if (!pszCur[1])
            {
                if (pszCur != pszTop)
                    pszCur[-1] = '\0';
                else
                    *pszCur = '\0';
                break;
            }
            memmove(pszCur, pszCur + 2, strlen(pszCur + 2) + 1);
            continue;
        }
        else
        {
            /* advance to end of component. */
            while (*pszCur && *pszCur != RTPATH_SLASH)
                pszCur++;
        }

        if (!*pszCur)
            break;

        /* skip the slash */
        ++pszCur;
    }
}
#endif /* RT_OS_WINDOWS */

int vbsfPathAbs(const char *pszRoot, const char *pszPath, char *pszAbsPath, size_t cbAbsPath)
{
#if defined(RT_OS_WINDOWS)
    const char *pszPathStart = pszRoot? pszRoot: pszPath;

    /* Windows extended-length paths. */
    if (   RTPATH_IS_SLASH(pszPathStart[0])
        && RTPATH_IS_SLASH(pszPathStart[1])
        && pszPathStart[2] == '?'
        && RTPATH_IS_SLASH(pszPathStart[3])
       )
    {
        /* Maximum total path length of 32,767 characters. */
        if (cbAbsPath > _32K)
            cbAbsPath = _32K;

        /* Copy the root to pszAbsPath buffer. */
        size_t cchRoot = pszRoot? strlen(pszRoot): 0;
        if (cchRoot >= cbAbsPath)
            return VERR_FILENAME_TOO_LONG;

        if (pszRoot)
        {
            /* Caller must ensure that the path is relative, without the leading path separator. */
            if (RTPATH_IS_SLASH(pszPath[0]))
                return VERR_INVALID_PARAMETER;

            if (cchRoot)
               memcpy(pszAbsPath, pszRoot, cchRoot);

            if (cchRoot == 0 || !RTPATH_IS_SLASH(pszAbsPath[cchRoot - 1]))
            {
                /* Append path separator after the root. */
                ++cchRoot;
                if (cchRoot >= cbAbsPath)
                    return VERR_FILENAME_TOO_LONG;

                pszAbsPath[cchRoot - 1] = RTPATH_SLASH;
            }
        }

        /* Append the path to the pszAbsPath buffer. */
        const size_t cchPath = strlen(pszPath);
        if (cchRoot + cchPath >= cbAbsPath)
            return VERR_FILENAME_TOO_LONG;

        memcpy(&pszAbsPath[cchRoot], pszPath, cchPath + 1); /* Including trailing 0. */

        /* Find out where the actual path begins, i.e. skip the root spec. */
        char *pszPathBegin = &pszAbsPath[4]; /* Skip the extended-length path prefix "\\?\" */
        if (   pszPathBegin[0]
            && RTPATH_IS_VOLSEP(pszPathBegin[1])
            && pszPathBegin[2] == RTPATH_SLASH)
        {
            /* "\\?\C:\" */
            pszPathBegin += 3;
        }
        else if (   pszPathBegin[0] == 'U'
                 && pszPathBegin[1] == 'N'
                 && pszPathBegin[2] == 'C'
                 && pszPathBegin[3] == RTPATH_SLASH)
        {
            /* "\\?\UNC\server\share" */
            pszPathBegin += 4;

            /* Skip "server\share" too. */
            while (*pszPathBegin != RTPATH_SLASH && *pszPathBegin)
                ++pszPathBegin;
            if (*pszPathBegin == RTPATH_SLASH)
            {
                ++pszPathBegin;
                while (*pszPathBegin != RTPATH_SLASH && *pszPathBegin)
                    ++pszPathBegin;
                if (*pszPathBegin == RTPATH_SLASH)
                    ++pszPathBegin;
            }
        }
        else
            return VERR_INVALID_NAME;

        /* Process pszAbsPath in place. */
        vbsfPathResolveRelative(pszPathBegin);

        return VINF_SUCCESS;
    }
#endif /* RT_OS_WINDOWS */

    /* Fallback for the common paths. */
    return RTPathAbsEx(pszRoot, pszPath, pszAbsPath, cbAbsPath);
}
