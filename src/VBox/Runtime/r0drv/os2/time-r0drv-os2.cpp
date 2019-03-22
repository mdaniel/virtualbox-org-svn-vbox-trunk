/* $Id$ */
/** @file
 * IPRT - Time, Ring-0 Driver, OS/2.
 */

/*
 * Contributed by knut st. osmundsen.
 *
 * Copyright (C) 2007-2019 Oracle Corporation
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
 *
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-os2-kernel.h"

#include <iprt/time.h>


RTDECL(uint64_t) RTTimeNanoTS(void)
{
    /** @remark OS/2 Ring-0: will wrap after 48 days. */
    return g_pGIS->msecs * UINT64_C(1000000);
}


RTDECL(uint64_t) RTTimeMilliTS(void)
{
    /** @remark OS/2 Ring-0: will wrap after 48 days. */
    return g_pGIS->msecs;
}


RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    /** @remark OS/2 Ring-0: will wrap after 48 days. */
    return g_pGIS->msecs * UINT64_C(1000000);
}


RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    /** @remark OS/2 Ring-0: will wrap after 48 days. */
    return g_pGIS->msecs;
}


RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    /*
     * Get the seconds since the unix epoch (local time) and current hundredths.
     */
    GINFOSEG volatile *pGIS = (GINFOSEG volatile *)g_pGIS;
    UCHAR uchHundredths;
    ULONG ulSeconds;
    do
    {
        uchHundredths = pGIS->hundredths;
        ulSeconds = pGIS->time;
    } while (   uchHundredths  == pGIS->hundredths
             && ulSeconds      == pGIS->time);

    /*
     * Combine the two and convert to UCT (later).
     */
    uint64_t u64 = ulSeconds * UINT64_C(1000000000) + (uint32_t)uchHundredths * 10000000;
    /** @todo convert from local to UCT. */

    /** @remark OS/2 Ring-0: Currently returns local time instead of UCT. */
    return RTTimeSpecSetNano(pTime, u64);
}

