/* $Id$ */
/** @file
 * IPRT - UUID, Windows implementation.
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
#define LOG_GROUP RTLOGGROUP_UUID
#include <iprt/win/windows.h>

#include <iprt/uuid.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>


RTDECL(int)  RTUuidClear(PRTUUID pUuid)
{
    /* check params */
    AssertPtrReturn(pUuid, VERR_INVALID_POINTER);

    return RTErrConvertFromWin32(UuidCreateNil((UUID *)pUuid));
}


RTDECL(bool)  RTUuidIsNull(PCRTUUID pUuid)
{
    /* check params */
    AssertPtrReturn(pUuid, true);

    RPC_STATUS status;
    return !!UuidIsNil((UUID *)pUuid, &status);
}


RTDECL(int)  RTUuidCompare(PCRTUUID pUuid1, PCRTUUID pUuid2)
{
    /*
     * Special cases.
     */
    if (pUuid1 == pUuid2)
        return 0;
    if (!pUuid1)
        return RTUuidIsNull(pUuid2) ? 0 : -1;
    if (!pUuid2)
        return RTUuidIsNull(pUuid1) ? 0 : 1;
    AssertPtrReturn(pUuid1, -1);
    AssertPtrReturn(pUuid2, 1);

    /*
     * Hand the rest to the Windows API.
     */
    RPC_STATUS status;
    return UuidCompare((UUID *)pUuid1, (UUID *)pUuid2, &status);
}


RTDECL(int)  RTUuidCompareStr(PCRTUUID pUuid1, const char *pszString2)
{
    /* check params */
    AssertPtrReturn(pUuid1, -1);
    AssertPtrReturn(pszString2, 1);

    /*
     * Try convert the string to a UUID and then compare the two.
     */
    RTUUID Uuid2;
    int rc = RTUuidFromStr(&Uuid2, pszString2);
    AssertRCReturn(rc, 1);

    return RTUuidCompare(pUuid1, &Uuid2);
}


RTDECL(int)  RTUuidCompare2Strs(const char *pszString1, const char *pszString2)
{
    RTUUID Uuid1;
    RTUUID Uuid2;
    int rc;

    /* check params */
    AssertPtrReturn(pszString1, -1);
    AssertPtrReturn(pszString2, 1);

    /*
     * Try convert the strings to UUIDs and then compare them.
     */
    rc = RTUuidFromStr(&Uuid1, pszString1);
    AssertRCReturn(rc, -1);

    rc = RTUuidFromStr(&Uuid2, pszString2);
    AssertRCReturn(rc, 1);

    return RTUuidCompare(&Uuid1, &Uuid2);
}


RTDECL(int)  RTUuidToStr(PCRTUUID pUuid, char *pszString, size_t cchString)
{
    /* check params */
    AssertPtrReturn(pUuid, VERR_INVALID_POINTER);
    AssertPtrReturn(pszString, VERR_INVALID_POINTER);
    AssertReturn(cchString >= RTUUID_STR_LENGTH, VERR_INVALID_PARAMETER);

    /*
     * Try convert it.
     *
     * The API allocates a new string buffer for us, so we can do our own
     * buffer overflow handling.
     */
    RPC_STATUS Status;
    unsigned char *pszTmpStr = NULL;
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    Status = UuidToStringA((UUID *)pUuid, &pszTmpStr);
#else
    Status = UuidToString((UUID *)pUuid, &pszTmpStr);
#endif
    if (Status != RPC_S_OK)
        return RTErrConvertFromWin32(Status);

    /* copy it. */
    int rc = VINF_SUCCESS;
    size_t cchTmpStr = strlen((char *)pszTmpStr);
    if (cchTmpStr < cchString)
        memcpy(pszString, pszTmpStr, cchTmpStr + 1);
    else
    {
        AssertFailed();
        rc = ERROR_BUFFER_OVERFLOW;
    }

    /* free buffer */
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    RpcStringFreeA(&pszTmpStr);
#else
    RpcStringFree(&pszTmpStr);
#endif

    /* all done */
    return rc;
}


RTDECL(int)  RTUuidFromStr(PRTUUID pUuid, const char *pszString)
{
    /* check params */
    AssertPtrReturn(pUuid, VERR_INVALID_POINTER);
    AssertPtrReturn(pszString, VERR_INVALID_POINTER);

    RPC_STATUS rc;
#ifdef RPC_UNICODE_SUPPORTED
    /* always use ASCII version! */
    rc = UuidFromStringA((unsigned char *)pszString, (UUID *)pUuid);
#else
    rc = UuidFromString((unsigned char *)pszString, (UUID *)pUuid);
#endif

    return RTErrConvertFromWin32(rc);
}

