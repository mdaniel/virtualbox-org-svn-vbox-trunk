/* $Id$ */
/** @file
 * IPRT - RTPathGetCurrentOnDrive, generic implementation.
 */

/*
 * Copyright (C) 2014-2017 Oracle Corporation
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
#include <iprt/path.h>
#include "internal/iprt.h"

#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/path.h"


RTDECL(int) RTPathGetCurrentOnDrive(char chDrive, char *pszPath, size_t cbPath)
{
#ifdef HAVE_DRIVE
    /*
     * Check if it's the same drive as the current directory.
     */
    int rc = RTPathGetCurrent(pszPath, cbPath);
    if (RT_SUCCESS(rc))
    {
        if (   (   chDrive == *pszPath
                || RT_C_TO_LOWER(chDrive) == RT_C_TO_LOWER(*pszPath))
            && RTPATH_IS_VOLSEP(pszPath[1]))
            return rc;

        /*
         * Different drive, indicate root.
         */
        if (cbPath >= 4)
        {
            pszPath[0] = RT_C_TO_UPPER(chDrive);
            pszPath[1] = ':';
            pszPath[2] = RTPATH_SLASH;
            pszPath[3] = '\0';
            return VINF_SUCCESS;
        }
    }
    return rc;

#else
    /*
     * No driver letters, just return root slash on whatever we're asked.
     */
    NOREF(chDrive);
    if (cbPath >= 2)
    {
        pszPath[0] = RTPATH_SLASH;
        pszPath[1] = '\0';
        return VINF_SUCCESS;
    }
    return VERR_BUFFER_OVERFLOW;
#endif
}
RT_EXPORT_SYMBOL(RTPathGetCurrentOnDrive);

