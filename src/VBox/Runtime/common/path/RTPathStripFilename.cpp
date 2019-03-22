/* $Id$ */
/** @file
 * IPRT - RTPathStripFilename
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
#include <iprt/path.h>
#include <iprt/ctype.h>


/**
 * Strips the filename from a path. Truncates the given string in-place by overwriting the
 * last path separator character with a null byte in a platform-neutral way.
 *
 * @param   pszPath     Path from which filename should be extracted, will be truncated.
 *                      If the string contains no path separator, it will be changed to a "." string.
 */
RTDECL(void) RTPathStripFilename(char *pszPath)
{
    char *psz = pszPath;
    char *pszLastSep = NULL;


    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastSep = psz + 1;
                if (RTPATH_IS_SLASH(psz[1]))
                    pszPath = psz + 1;
                else
                    pszPath = psz;
                break;

            case '\\':
#endif
            case '/':
                pszLastSep = psz;
                break;

            /* the end */
            case '\0':
                if (!pszLastSep)
                {
                    /* no directory component */
                    pszPath[0] = '.';
                    pszPath[1] = '\0';
                }
                else if (pszLastSep == pszPath)
                {
                    /* only root. */
                    pszLastSep[1] = '\0';
                }
                else
                    pszLastSep[0] = '\0';
                return;
        }
    }
    /* will never get here */
}

