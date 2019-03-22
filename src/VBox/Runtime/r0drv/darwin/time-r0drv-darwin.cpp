/* $Id$ */
/** @file
 * IPRT - Time, Ring-0 Driver, Darwin.
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
#define LOG_GROUP RTLOGGROUP_TIME
#include "the-darwin-kernel.h"
#include "internal/iprt.h"
#include <iprt/time.h>

#include <iprt/asm.h>


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
    static int8_t s_fSimple = -1;

    /* first call: check if life is simple or not. */
    if (s_fSimple < 0)
    {
        struct mach_timebase_info Info;
        clock_timebase_info(&Info);
        ASMAtomicXchgS8((int8_t * volatile)&s_fSimple, Info.denom == 1 && Info.numer == 1);
    }

    /* special case: absolute time is in nanoseconds */
    if (s_fSimple)
        return mach_absolute_time();

    /* general case: let mach do the mult/div for us. */
    uint64_t u64;
    absolutetime_to_nanoseconds(mach_absolute_time(), &u64);
    return u64;
}


RTDECL(uint64_t) RTTimeNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


RTDECL(uint64_t) RTTimeMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / RT_NS_1MS;
}


RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / RT_NS_1MS;
}


RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    uint32_t        uSecs;
    uint32_t        uNanosecs;
#else
    clock_sec_t     uSecs;
    clock_nsec_t    uNanosecs;
#endif
    clock_get_calendar_nanotime(&uSecs, &uNanosecs);
    return RTTimeSpecSetNano(pTime, (uint64_t)uSecs * RT_NS_1SEC + uNanosecs);
}

