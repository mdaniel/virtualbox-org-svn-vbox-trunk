/* $Id$ */
/** @file
 * IPRT - Threads (Part 1), Ring-0 Driver, FreeBSD.
 */

/*
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-freebsd-kernel.h"
#include "internal/iprt.h"
#include <iprt/thread.h>

#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mp.h>
#include "internal/thread.h"


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)curthread;
}


static int rtR0ThreadFbsdSleepCommon(RTMSINTERVAL cMillies)
{
    int rc;
    int cTicks;

    /*
     * 0 ms sleep -> yield.
     */
    if (!cMillies)
    {
        RTThreadYield();
        return VINF_SUCCESS;
    }

    /*
     * Translate milliseconds into ticks and go to sleep.
     */
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        if (hz == 1000)
            cTicks = cMillies;
        else if (hz == 100)
            cTicks = cMillies / 10;
        else
        {
            int64_t cTicks64 = ((uint64_t)cMillies * hz) / 1000;
            cTicks = (int)cTicks64;
            if (cTicks != cTicks64)
                cTicks = INT_MAX;
        }
    }
    else
        cTicks = 0;     /* requires giant lock! */

    rc = tsleep((void *)RTThreadSleep,
                PZERO | PCATCH,
                "iprtsl",           /* max 6 chars */
                cTicks);
    switch (rc)
    {
        case 0:
            return VINF_SUCCESS;
        case EWOULDBLOCK:
            return VERR_TIMEOUT;
        case EINTR:
        case ERESTART:
            return VERR_INTERRUPTED;
        default:
            AssertMsgFailed(("%d\n", rc));
            return VERR_NO_TRANSLATION;
    }
}


RTDECL(int) RTThreadSleep(RTMSINTERVAL cMillies)
{
    return rtR0ThreadFbsdSleepCommon(cMillies);
}


RTDECL(int) RTThreadSleepNoLog(RTMSINTERVAL cMillies)
{
    return rtR0ThreadFbsdSleepCommon(cMillies);
}


RTDECL(bool) RTThreadYield(void)
{
#if __FreeBSD_version >= 900032
    kern_yield(curthread->td_user_pri);
#else
    uio_yield();
#endif
    return false; /** @todo figure this one ... */
}


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    return curthread->td_critnest == 0
        && ASMIntAreEnabled(); /** @todo is there a native freebsd function/macro for this? */
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    return curthread->td_owepreempt == 1;
}


RTDECL(bool) RTThreadPreemptIsPendingTrusty(void)
{
    /* yes, RTThreadPreemptIsPending is reliable. */
    return true;
}


RTDECL(bool) RTThreadPreemptIsPossible(void)
{
    /* yes, kernel preemption is possible. */
    return true;
}


RTDECL(void) RTThreadPreemptDisable(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->u32Reserved == 0);
    pState->u32Reserved = 42;

    critical_enter();
    RT_ASSERT_PREEMPT_CPUID_DISABLE(pState);
}


RTDECL(void) RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    Assert(pState->u32Reserved == 42);
    pState->u32Reserved = 0;

    RT_ASSERT_PREEMPT_CPUID_RESTORE(pState);
    critical_exit();
}


RTDECL(bool) RTThreadIsInInterrupt(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD); NOREF(hThread);
    /** @todo FreeBSD: Implement RTThreadIsInInterrupt. Required for guest
     *        additions! */
    return !ASMIntAreEnabled();
}

