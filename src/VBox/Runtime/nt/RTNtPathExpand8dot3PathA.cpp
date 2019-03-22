/* $Id$ */
/** @file
 * IPRT - Native NT, RTNtPathExpand8dot3PathA.
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
#define LOG_GROUP RTLOGGROUP_FS
#ifdef IN_SUP_HARDENED_R3
# include <iprt/nt/nt-and-windows.h>
#else
# include <iprt/nt/nt.h>
#endif

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/utf16.h>



/**
 * Wrapper around RTNtPathExpand8dot3Path that allocates a buffer instead of
 * working on the input buffer.
 *
 * @returns IPRT status code, see RTNtPathExpand8dot3Path().
 * @param   pUniStrSrc  The path to fix up. MaximumLength is the max buffer
 *                      length.
 * @param   fPathOnly   Whether to only process the path and leave the filename
 *                      as passed in.
 * @param   pUniStrDst  Output string.  On success, the caller must use
 *                      RTUtf16Free to free what the Buffer member points to.
 *                      This is all zeros and NULL on failure.
 */
RTDECL(int) RTNtPathExpand8dot3PathA(PCUNICODE_STRING pShort, bool fPathOnly, PUNICODE_STRING pUniStrDst)
{
    /* Guess a reasonable size for the final version. */
    size_t const cbShort = pShort->Length;
    size_t       cbLong  = RT_MIN(_64K - 1, cbShort * 8);
    if (cbLong < RTPATH_MAX)
        cbLong = RTPATH_MAX * 2;
    AssertCompile(RTPATH_MAX * 2 < _64K);

    pUniStrDst->Buffer = (WCHAR *)RTUtf16Alloc(cbLong);
    if (pUniStrDst->Buffer != NULL)
    {
        /* Copy over the short name and fix it up. */
        pUniStrDst->MaximumLength = (uint16_t)cbLong;
        pUniStrDst->Length        = (uint16_t)cbShort;
        memcpy(pUniStrDst->Buffer, pShort->Buffer, cbShort);
        pUniStrDst->Buffer[cbShort / sizeof(WCHAR)] = '\0';
        int rc = RTNtPathExpand8dot3Path(pUniStrDst, fPathOnly);
        if (RT_SUCCESS(rc))
            return rc;

        /* We failed, bail. */
        RTUtf16Free(pUniStrDst->Buffer);
        pUniStrDst->Buffer    = NULL;
    }
    pUniStrDst->Length        = 0;
    pUniStrDst->MaximumLength = 0;
    return VERR_NO_UTF16_MEMORY;
}

