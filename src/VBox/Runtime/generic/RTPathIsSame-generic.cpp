/* $Id$ */
/** @file
 * IPRT - Assertions, generic RTPathIsSame.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
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
#include <iprt/path.h>
#include "internal/iprt.h"

#include <iprt/string.h>


RTDECL(int) RTPathIsSame(const char *pszPath1, const char *pszPath2)
{
    /*
     * Simple checks based on the path values.
     */
    if (pszPath1 == pszPath2)
        return true;
    if (!pszPath1)
        return false;
    if (!pszPath2)
        return false;
    if (!strcmp(pszPath1, pszPath2))
        return true;

    /*
     * If the files exist, try use the attributes.
     */
    RTFSOBJINFO ObjInfo1;
    int rc = RTPathQueryInfoEx(pszPath1, &ObjInfo1, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
    if (RT_SUCCESS(rc))
    {
        RTFSOBJINFO ObjInfo2;
        rc = RTPathQueryInfoEx(pszPath2, &ObjInfo2, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
        if (RT_SUCCESS(rc))
        {
            if ((ObjInfo1.Attr.fMode & RTFS_TYPE_MASK) != (ObjInfo2.Attr.fMode & RTFS_TYPE_MASK))
                return false;
            if (ObjInfo1.Attr.u.Unix.INodeIdDevice       != ObjInfo2.Attr.u.Unix.INodeIdDevice)
                return false;
            if (ObjInfo1.Attr.u.Unix.INodeId             != ObjInfo2.Attr.u.Unix.INodeId)
                return false;
            if (ObjInfo1.Attr.u.Unix.GenerationId        != ObjInfo2.Attr.u.Unix.GenerationId)
                return false;
            if (   ObjInfo1.Attr.u.Unix.INodeIdDevice != 0
                && ObjInfo1.Attr.u.Unix.INodeId       != 0)
                return true;
        }
    }

    /*
     * Fallback, compare absolute/real paths. Return failure on paths that are
     * too long.
     */
    char szPath1[RTPATH_MAX];
    rc = RTPathAbs(pszPath1, szPath1, sizeof(szPath1));
    AssertRCReturn(rc, VERR_FILENAME_TOO_LONG);

    char szPath2[RTPATH_MAX];
    rc = RTPathAbs(pszPath2, szPath2, sizeof(szPath2)); AssertRC(rc);
    AssertRCReturn(rc, VERR_FILENAME_TOO_LONG);

    if (RTPathCompare(szPath1, szPath2) == 0)
        return true;

    /** @todo Relsolve any symbolic links in the paths. Too lazy for that right
     *        now. */
    return false;
}
RT_EXPORT_SYMBOL(RTPathIsSame);

