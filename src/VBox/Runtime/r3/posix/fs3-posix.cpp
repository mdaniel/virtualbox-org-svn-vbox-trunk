/* $Id$ */
/** @file
 * IPRT - File System Helpers, POSIX, Part 3.
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
#include <iprt/fs.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/string.h>
#include "internal/fs.h"

#include <sys/time.h>
#include <grp.h>
#include <pwd.h>


/**
 * Set user-owner additional attributes.
 *
 * @param   pObjInfo            The object info to fill add attrs for.
 * @param   uid                 The user id.
 */
void    rtFsObjInfoAttrSetUnixOwner(PRTFSOBJINFO pObjInfo, RTUID uid)
{
    pObjInfo->Attr.enmAdditional   = RTFSOBJATTRADD_UNIX_OWNER;
    pObjInfo->Attr.u.UnixOwner.uid = uid;
    pObjInfo->Attr.u.UnixOwner.szName[0] = '\0';

    char            achBuf[_4K];
    struct passwd   Pwd;
    struct passwd  *pPwd;
    int rc = getpwuid_r(uid, &Pwd, achBuf, sizeof(achBuf), &pPwd);
    if (!rc && pPwd)
        RTStrCopy(pObjInfo->Attr.u.UnixOwner.szName, sizeof(pObjInfo->Attr.u.UnixOwner.szName), pPwd->pw_name);
}


/**
 * Set user-group additional attributes.
 *
 * @param   pObjInfo            The object info to fill add attrs for.
 * @param   gid                 The group id.
 */
void rtFsObjInfoAttrSetUnixGroup(PRTFSOBJINFO pObjInfo, RTUID gid)
{
    pObjInfo->Attr.enmAdditional   = RTFSOBJATTRADD_UNIX_GROUP;
    pObjInfo->Attr.u.UnixGroup.gid = gid;
    pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';

    char            achBuf[_4K];
    struct group    Grp;
    struct group   *pGrp;

    int rc = getgrgid_r(gid, &Grp, achBuf, sizeof(achBuf), &pGrp);
    if (!rc && pGrp)
        RTStrCopy(pObjInfo->Attr.u.UnixGroup.szName, sizeof(pObjInfo->Attr.u.UnixGroup.szName), pGrp->gr_name);
}

