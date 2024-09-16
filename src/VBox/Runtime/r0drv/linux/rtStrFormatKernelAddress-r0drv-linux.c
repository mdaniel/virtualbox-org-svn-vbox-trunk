/* $Id$ */
/** @file
 * IPRT - IPRT String Formatter, ring-0 addresses.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_STRING
#include "the-linux-kernel.h"
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/string.h>

#include "internal/string.h"


DECLHIDDEN(size_t) rtStrFormatKernelAddress(char *pszBuf, size_t cbBuf, RTR0INTPTR uPtr, signed int cchWidth,
                                            signed int cchPrecision, unsigned int fFlags)
{
#if !defined(DEBUG) && RTLNX_VER_MIN(2,6,38)
    RT_NOREF(cchWidth, cchPrecision);
    /* use the Linux kernel function which is able to handle "%pK" */
    static const char s_szFmt[] = "0x%pK";
    const char *pszFmt = s_szFmt;
    if (!(fFlags & RTSTR_F_SPECIAL))
        pszFmt += 2;
    return scnprintf(pszBuf, cbBuf, pszFmt, uPtr);
#else
    Assert(cbBuf >= 64);
    return RTStrFormatNumber(pszBuf, uPtr, 16, cchWidth, cchPrecision, fFlags);
#endif
}
