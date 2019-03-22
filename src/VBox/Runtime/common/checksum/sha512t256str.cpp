/* $Id$ */
/** @file
 * IPRT - SHA-512/256 string functions.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#include <iprt/sha.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


RTDECL(int) RTSha512t256ToString(uint8_t const pabDigest[RTSHA512T256_HASH_SIZE], char *pszDigest, size_t cchDigest)
{
    return RTStrPrintHexBytes(pszDigest, cchDigest, &pabDigest[0], RTSHA512T256_HASH_SIZE, 0 /*fFlags*/);
}


RTDECL(int) RTSha512t256FromString(char const *pszDigest, uint8_t pabDigest[RTSHA512T256_HASH_SIZE])
{
    return RTStrConvertHexBytes(RTStrStripL(pszDigest), &pabDigest[0], RTSHA512T256_HASH_SIZE, 0 /*fFlags*/);
}

