/* $Id$ */
/** @file
 * IPRT - Threads, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/thread.h>

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/mp.h>

#define SOL_THREAD_PREEMPT       (*((char *)curthread + g_offrtSolThreadPreempt))
#define SOL_CPU_RUNRUN           (*((char *)CPU + g_offrtSolCpuPreempt))
#define SOL_CPU_KPRUNRUN         (*((char *)CPU + g_offrtSolCpuForceKernelPreempt))

RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)curthread;
}


static int rtR0ThreadSolSleepCommon(RTMSINTERVAL cMillies)
{
    clock_t cTicks;
    RT_ASSERT_PREEMPTIBLE();

    if (!cMillies)
    {
        RTThreadYield();
        return VINF_SUCCESS;
    }

    if (cMillies != RT_INDEFINITE_WAIT)
        cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
    else
        cTicks = 0;

    delay(cTicks);
    return VINF_SUCCESS;
}


RTDECL(int) RTThreadSleep(RTMSINTERVAL cMillies)
{
    return rtR0ThreadSolSleepCommon(cMillies);
}


RTDECL(int) RTThreadSleepNoLog(RTMSINTERVAL cMillies)
{
    return rtR0ThreadSolSleepCommon(cMillies);
}


RTDECL(bool) RTThreadYield(void)
{
    RT_ASSERT_PREEMPTIBLE();

    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
    RTThreadPreemptDisable(&PreemptState);

    char const bThreadPreempt = SOL_THREAD_PREEMPT;
    char const bForcePreempt  = SOL_CPU_KPRUNRUN;
    bool fWillYield = false;
    Assert(bThreadPreempt >= 1);

    /*
     * If we are the last preemption enabler for this thread and if force
     * preemption is set on the CPU, only then we are guaranteed to be preempted.
     */
    if (bThreadPreempt == 1 && bForcePreempt != 0)
        fWillYield = true;

    RTThreadPreemptRestore(&PreemptState);
    return fWillYield;
}


RTDECL(bool) RTThreadPreemptIsEnabled(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    if (RT_UNLIKELY(g_frtSolInitDone == false))
    {
        cmn_err(CE_CONT, "!RTThreadPreemptIsEnabled called before RTR0Init!\n");
        return true;
    }

    bool fThreadPreempt = false;
    if (SOL_THREAD_PREEMPT == 0)
        fThreadPreempt = true;

    if (!fThreadPreempt)
        return false;
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    if (!ASMIntAreEnabled())
        return false;
#endif
    if (getpil() >= DISP_LEVEL)
        return false;
    return true;
}


RTDECL(bool) RTThreadPreemptIsPending(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);

    char const bPreempt      = SOL_CPU_RUNRUN;
    char const bForcePreempt = SOL_CPU_KPRUNRUN;
    return (bPreempt != 0 || bForcePreempt != 0);
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

    SOL_THREAD_PREEMPT++;
    Assert(SOL_THREAD_PREEMPT >= 1);

    RT_ASSERT_PREEMPT_CPUID_DISABLE(pState);
}


RTDECL(void) RTThreadPreemptRestore(PRTTHREADPREEMPTSTATE pState)
{
    AssertPtr(pState);
    RT_ASSERT_PREEMPT_CPUID_RESTORE(pState);

    Assert(SOL_THREAD_PREEMPT >= 1);
    if (--SOL_THREAD_PREEMPT == 0 && SOL_CPU_RUNRUN != 0)
        kpreempt(KPREEMPT_SYNC);
}


RTDECL(bool) RTThreadIsInInterrupt(RTTHREAD hThread)
{
    Assert(hThread == NIL_RTTHREAD);
    return servicing_interrupt() ? true : false;
}

