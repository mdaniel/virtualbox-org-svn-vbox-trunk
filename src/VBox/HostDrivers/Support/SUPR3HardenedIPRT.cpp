/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened Support Routines using IPRT.
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
#define LOG_GROUP LOG_GROUP_SUP
#include <VBox/sup.h>
#include <iprt/errcore.h>
#include <VBox/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/assert.h>
#include <iprt/path.h>
#include <iprt/param.h>

#include "SUPLibInternal.h"


/**
 * @copydoc RTPathFilename
 */
DECLHIDDEN(char *) supR3HardenedPathFilename(const char *pszPath)
{
    return RTPathFilename(pszPath);
}


/**
 * @copydoc RTPathAppPrivateNoArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateNoArch(char *pszPath, size_t cchPath)
{
    return RTPathAppPrivateNoArch(pszPath, cchPath);
}


/**
 * @copydoc RTPathAppPrivateArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateArch(char *pszPath, size_t cchPath)
{
    return RTPathAppPrivateArch(pszPath, cchPath);
}


/**
 * @copydoc RTPathSharedLibs
 */
DECLHIDDEN(int) supR3HardenedPathAppSharedLibs(char *pszPath, size_t cchPath)
{
    return RTPathSharedLibs(pszPath, cchPath);
}


/**
 * @copydoc RTPathAppDocs
 */
DECLHIDDEN(int) supR3HardenedPathAppDocs(char *pszPath, size_t cchPath)
{
    return RTPathAppDocs(pszPath, cchPath);
}


/**
 * @copydoc RTPathExecDir
 */
DECLHIDDEN(int) supR3HardenedPathAppBin(char *pszPath, size_t cchPath)
{
    return RTPathExecDir(pszPath, cchPath);
}


DECL_NO_RETURN(DECLHIDDEN(void)) supR3HardenedFatalMsgV(const char *pszWhere, SUPINITOP enmWhat, int rc,
                                                        const char *pszMsgFmt, va_list va)
{
    va_list vaCopy;
    va_copy(vaCopy, va);
    AssertFatalMsgFailed(("%s (rc=%Rrc): %N", pszWhere, rc, pszMsgFmt, &vaCopy));
    NOREF(enmWhat);
    /* not reached */
}


DECL_NO_RETURN(DECLHIDDEN(void)) supR3HardenedFatalMsg(const char *pszWhere, SUPINITOP enmWhat, int rc,
                                                       const char *pszMsgFmt, ...)
{
    va_list va;
    va_start(va, pszMsgFmt);
    supR3HardenedFatalMsgV(pszWhere, enmWhat, rc, pszMsgFmt, va);
    /* not reached */
}


DECL_NO_RETURN(DECLHIDDEN(void)) supR3HardenedFatalV(const char *pszFormat, va_list va)
{
    va_list vaCopy;
    va_copy(vaCopy, va);
    AssertFatalMsgFailed(("%N", pszFormat, &vaCopy));
    /* not reached */
}


DECL_NO_RETURN(DECLHIDDEN(void)) supR3HardenedFatal(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedFatalV(pszFormat, va);
    /* not reached */
}


DECLHIDDEN(int) supR3HardenedErrorV(int rc, bool fFatal, const char *pszFormat, va_list va)
{
    if (fFatal)
        supR3HardenedFatalV(pszFormat, va);

    va_list vaCopy;
    va_copy(vaCopy, va);
    AssertLogRelMsgFailed(("%N", pszFormat, &vaCopy)); /** @todo figure out why this ain't working, or at seems to be that way... */
    va_end(vaCopy);

    RTLogRelPrintfV(pszFormat, va);
    return rc;
}


DECLHIDDEN(int) supR3HardenedError(int rc, bool fFatal, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedErrorV(rc, fFatal, pszFormat, va);
    va_end(va);
    return rc;
}

