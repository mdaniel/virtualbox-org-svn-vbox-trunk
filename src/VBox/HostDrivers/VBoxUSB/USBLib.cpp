/* $Id$ */
/** @file
 * VirtualBox USB Library, Common Bits.
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
#include <VBox/usblib.h>


/**
 * Calculate the hash of the serial string.
 *
 * 64bit FNV1a, chosen because it is designed to hash in to a power of two
 * space, and is much quicker and simpler than, say, a half MD4.
 *
 * @returns the hash.
 * @param   pszSerial       The serial string.
 */
USBLIB_DECL(uint64_t) USBLibHashSerial(const char *pszSerial)
{
    if (!pszSerial)
        pszSerial = "";

    const uint8_t *pu8 = (const uint8_t *)pszSerial;
    uint64_t u64 = UINT64_C(14695981039346656037);
    for (;;)
    {
        uint8_t u8 = *pu8;
        if (!u8)
            break;
        u64 = (u64 * UINT64_C(1099511628211)) ^ u8;
        pu8++;
    }

    return u64;
}

