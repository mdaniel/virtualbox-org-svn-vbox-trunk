/* $Id$ */
/** @file
 * IPRT - RTUtf16CmpAscii.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include <iprt/utf16.h>
#include "internal/iprt.h"

#include <iprt/assert.h>


RTDECL(int) RTUtf16NCmpAscii(PCRTUTF16 pwsz1, const char *psz2, size_t cwcMax)
{
    while (cwcMax-- > 0)
    {
        RTUTF16         wc1  = *pwsz1++;
        unsigned char   uch2 = *psz2++; Assert(uch2 < 0x80);
        if (wc1 != uch2)
            return wc1 < uch2 ? -1 : 1;
        if (!uch2)
            break;
    }
    return 0;
}
RT_EXPORT_SYMBOL(RTUtf16NCmpAscii);

