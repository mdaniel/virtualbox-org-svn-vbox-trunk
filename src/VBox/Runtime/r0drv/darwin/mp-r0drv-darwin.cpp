/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2008-2024 Oracle and/or its affiliates.
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
#include "the-darwin-kernel.h"
#include "internal/iprt.h"
#include <iprt/mp.h>

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#else
# include <iprt/thread.h>
#endif
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include "r0drv/mp-r0drv.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static int32_t volatile g_cMaxCpus = -1;


static int rtMpDarwinInitMaxCpus(void)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    int32_t cCpus = -1;
    size_t  oldLen = sizeof(cCpus);
    int rc = sysctlbyname("hw.ncpu", &cCpus, &oldLen, NULL, NULL);
    if (rc)
    {
        printf("IPRT: sysctlbyname(hw.ncpu) failed with rc=%d!\n", rc);
        cCpus = 64; /* whatever */
    }

    ASMAtomicWriteS32(&g_cMaxCpus, cCpus);

    IPRT_DARWIN_RESTORE_EFL_AC();
    return cCpus;
}


DECLINLINE(int) rtMpDarwinMaxCpus(void)
{
    int cCpus = g_cMaxCpus;
    if (RT_UNLIKELY(cCpus <= 0))
        return rtMpDarwinInitMaxCpus();
    return cCpus;
}


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return cpu_number();
}


RTDECL(int) RTMpCurSetIndex(void)
{
    return cpu_number();
}


RTDECL(int) RTMpCurSetIndexAndId(PRTCPUID pidCpu)
{
    return *pidCpu = cpu_number();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < RTCPUSET_MAX_CPUS ? (RTCPUID)iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return rtMpDarwinMaxCpus() - 1;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS
        && idCpu < (RTCPUID)rtMpDarwinMaxCpus();
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpIsCpuPossible(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return rtMpDarwinMaxCpus();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    /** @todo darwin R0 MP */
    return RTMpGetSet(pSet);
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    /** @todo darwin R0 MP */
    /** @todo this is getting worse with M3 Max and 14.x, where we're not able to
     *        RTMpOnAll/Specific on a bunch of performance cores when idle.
     * Note: We can probably get the processor_t via IOCPU and IOKit.  Doubt the
     * state is of much use there, but it may, though not for the SIGPdisabled. */
    return RTMpGetCount();
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    /** @todo darwin R0 MP */
    /** @todo this is getting worse with M3 Max and 14.x, where we're not able to
     *        RTMpOnAll/Specific on a bunch of performance cores when idle. */
    return RTMpIsCpuPossible(idCpu);
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    /** @todo darwin R0 MP (rainy day) */
    RT_NOREF(idCpu);
    return 0;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    /** @todo darwin R0 MP (rainy day) */
    RT_NOREF(idCpu);
    return 0;
}


RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    /** @todo (not used on non-Windows platforms yet). */
    return false;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnAllDarwinWrapper(void *pvArg)
{
    PRTMPARGS const pArgs = (PRTMPARGS)pvArg;
    IPRT_DARWIN_SAVE_EFL_AC();
    pArgs->pfnWorker(cpu_number(), pArgs->pvUser1, pArgs->pvUser2);
    IPRT_DARWIN_RESTORE_EFL_AC();

#if defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    /* Wake up the thread calling RTMpOnAll if we're the last CPU out of here: */
    if (ASMAtomicDecU32(&pArgs->cCpusLeftSynch) == 0)
        thread_wakeup((event_t)&pArgs->cCpusLeftSynch);
#endif
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RT_ASSERT_INTS_ON();
    IPRT_DARWIN_SAVE_EFL_AC();

    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    mp_rendezvous_no_intrs(rtmpOnAllDarwinWrapper, &Args);

#elif defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    Args.cCpusLeftSynch = 0;
    if (g_pfnR0DarwinCpuBroadcastXCall && g_pfnR0DarwinCpuNumberMayBeNull)
        g_pfnR0DarwinCpuBroadcastXCall(&Args.cCpusLeftSynch, TRUE /*fCallSelf*/, rtmpOnAllDarwinWrapper, &Args);
    else
        return VERR_NOT_IMPLEMENTED;

#else
# error "port me"
#endif

    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnOthers API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnOthersDarwinWrapper(void *pvArg)
{
    PRTMPARGS const pArgs = (PRTMPARGS)pvArg;
    RTCPUID const   idCpu = cpu_number();
#if !defined(RT_ARCH_ARM64) && !defined(RT_ARCH_ARM32) /* cpu_broadcast_xcall filters for us */
    if (pArgs->idCpu != idCpu)
#endif
    {
        IPRT_DARWIN_SAVE_EFL_AC();
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
        IPRT_DARWIN_RESTORE_EFL_AC();
    }

#if defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    /* Wake up the thread calling RTMpOnOthers if we're the last CPU out of here: */
    if (ASMAtomicDecU32(&pArgs->cCpusLeftSynch) == 0)
        thread_wakeup((event_t)&pArgs->cCpusLeftSynch);
#endif
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RT_ASSERT_INTS_ON();
    IPRT_DARWIN_SAVE_EFL_AC();

    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = RTMpCpuId();
    Args.cHits = 0;

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    mp_rendezvous_no_intrs(rtmpOnOthersDarwinWrapper, &Args);

#elif defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    Args.cCpusLeftSynch = 0;
    if (g_pfnR0DarwinCpuBroadcastXCall && g_pfnR0DarwinCpuNumberMayBeNull)
        g_pfnR0DarwinCpuBroadcastXCall(&Args.cCpusLeftSynch, FALSE /*fCallSelf*/, rtmpOnOthersDarwinWrapper, &Args);
    else
        return VERR_NOT_IMPLEMENTED;

#else
# error "port me"
#endif

    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnSpecific API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificDarwinBroadcastWrapper(void *pvArg)
{
    PRTMPARGS const pArgs = (PRTMPARGS)pvArg;
    RTCPUID const   idCpu = cpu_number();
    if (pArgs->idCpu == idCpu)
    {
        IPRT_DARWIN_SAVE_EFL_AC();
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
        ASMAtomicIncU32(&pArgs->cHits);
        IPRT_DARWIN_RESTORE_EFL_AC();
    }
#if defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    /* Wake up the thread calling RTMpOnSpecific if we're the last CPU out of here: */
    if (ASMAtomicDecU32(&pArgs->cCpusLeftSynch) == 0)
        thread_wakeup((event_t)&pArgs->cCpusLeftSynch);
#endif
}


#if defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
/**
 * Wrapper between the native darwin on-cpu callback and PFNRTWORKER for the
 * RTMpOnSpecific API when implemented using cpu_xcall() on arm.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificDarwinSingleWrapper(void *pvArg)
{
    PRTMPARGS const pArgs = (PRTMPARGS)pvArg;
    RTCPUID const   idCpu = cpu_number();
    Assert(pArgs->idCpu == idCpu);

    pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    ASMAtomicIncU32(&pArgs->cHits);

    /* Wake up the thread calling RTMpOnSpecific if it has blocked: */
    if (ASMAtomicDecU32(&pArgs->cCpusLeftSynch) == 0)
        thread_wakeup((event_t)&pArgs->cCpusLeftSynch);
}
#endif /* RT_ARCH_ARM64 || RT_ARCH_ARM32 */


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RT_ASSERT_INTS_ON();
    IPRT_DARWIN_SAVE_EFL_AC();

    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    mp_rendezvous_no_intrs(rtmpOnSpecificDarwinBroadcastWrapper, &Args);

#elif defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    if (g_pfnR0DarwinCpuXCall && g_pfnR0DarwinCpuNumberMayBeNull)
    {
        ASMAtomicWriteU32(&Args.cCpusLeftSynch, 1);

        /*
         * Prepare waiting on Args.cCpusLeftSynch.
         */
        assert_wait((event_t)&Args.cCpusLeftSynch, THREAD_UNINT);

        /*
         * Disable preemption as we need to deal with cross calls to the same
         * CPU here to make this work.
         */
        RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
        RTThreadPreemptDisable(&PreemptState);

        RTCPUID const idCpuSelf = RTMpCpuId();
        uint32_t      cWaitFor  = 0;
        int           rc        = VINF_SUCCESS;
        if (idCpuSelf != idCpu)
        {
            kern_return_t const krc = g_pfnR0DarwinCpuXCall(idCpu, rtmpOnSpecificDarwinSingleWrapper, &Args);
            if (krc == KERN_SUCCESS)
                cWaitFor = 1;
            else
                rc = VERR_CPU_OFFLINE;
        }
        else
        {
            rtmpOnSpecificDarwinSingleWrapper(&Args);
            cWaitFor = 0;
        }

        RTThreadPreemptRestore(&PreemptState);

        /*
         * Synchronize with any CPUs we've dispatched to pfnWorker.
         */
        if (cWaitFor == 0 || ASMAtomicReadU32(&Args.cCpusLeftSynch) == 0)
        {
            /* clear_wait(current_thread(), THREAD_AWAKENED); - not exported, so we have to do: */
            thread_wakeup((event_t)&Args.cCpusLeftSynch);
            thread_block(THREAD_CONTINUE_NULL);
        }
        else
            thread_block(THREAD_CONTINUE_NULL);

        Assert(rc != VINF_SUCCESS || Args.cHits == 1);
        IPRT_DARWIN_RESTORE_EFL_AC();
        return rc;
    }

    /*
     * Use broadcast to implement it, just like on x86/amd64:
     *
     * Note! This is unreliable, as with 14.7.5 / M3 Max often has several
     *       performance cores completely disabled when the system is idle,
     *       this means that SIGPdisabled is set and they'll be skipped by
     *       cpu_broadcast_xcall. The result is VERR_CPU_NOT_FOUND.
     */
    if (g_pfnR0DarwinCpuBroadcastXCall && g_pfnR0DarwinCpuNumberMayBeNull)
    {
        Args.cCpusLeftSynch = 0;
        g_pfnR0DarwinCpuBroadcastXCall(&Args.cCpusLeftSynch, TRUE /*fCallSelf*/, rtmpOnSpecificDarwinBroadcastWrapper, &Args);
    }
    else
        return VERR_NOT_IMPLEMENTED;

#else
# error "port me"
#endif

    IPRT_DARWIN_RESTORE_EFL_AC();
    return Args.cHits == 1
         ? VINF_SUCCESS
         : VERR_CPU_NOT_FOUND;
}

#if defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)

/**
 * Argument package for the darwin.arm64/arm32 RTMpOnPair implemenetation.
 *
 * We require a seperate implementation as the generic RTMpOnAll approach
 * doesn't work reliably, for reasons mentioned above in the RTMpOnSpecific code
 * and that our RTMpIsCpuOnline implementation is basically missing.
 */
typedef struct RTMPONPAIRDARWIN
{
    /** The state. */
    uint32_t volatile   fState;
# define RTMPONPAIRDARWIN_STATE_READY(a_idx)    RT_BIT_32(a_idxCpu)
# define RTMPONPAIRDARWIN_STATE_BOTH_READY      UINT32_C(0x00000003)
# define RTMPONPAIRDARWIN_STATE_SETUP_CANCEL    UINT32_C(0x80000000)
    /** Number of CPUs we're waiting for to complete. */
    uint32_t volatile   cCpusLeftSynch;
# ifdef RT_STRICT
    /** For logging/debugging. */
    uint32_t volatile   cCalls;
    /** For assertions. */
    RTCPUID             aidCpus[2];
# endif
    PFNRTMPWORKER       pfnWorker;
    void               *pvUser1;
    void               *pvUser2;
} RTMPONPAIRDARWIN;
/** Pointer to the an argument package for the generic RTMpOnPair
 *  implemenation. */
typedef RTMPONPAIRDARWIN *PRTMPONPAIRDARWIN;

/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnPair API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
template<uint32_t const a_idxCpu>
static void rtmpOnPairDarwinWrapper(void *pvArg)
{
    PRTMPONPAIRDARWIN const pArgs = (PRTMPONPAIRDARWIN)pvArg;
    RTCPUID           const idCpu = cpu_number();
    Assert(idCpu == pArgs->aidCpus[a_idxCpu]);

    uint64_t const  cMaxLoops = _4G * 8;
    uint64_t        cLoops    = 0;
    uint32_t fState = ASMAtomicOrExU32(&pArgs->fState, RTMPONPAIRDARWIN_STATE_READY(a_idxCpu))
                    | RTMPONPAIRDARWIN_STATE_READY(a_idxCpu);
    while (   fState == RTMPONPAIRDARWIN_STATE_READY(a_idxCpu)
           && cLoops++ < cMaxLoops)
    {
        ASMNopPause();
        fState = ASMAtomicUoReadU32(&pArgs->fState);
    }
    Assert(cLoops < cMaxLoops);

    if (fState == RTMPONPAIRDARWIN_STATE_BOTH_READY)
    {
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
# ifdef RT_STRICT
        ASMAtomicIncU32(&pArgs->cCalls);
# endif
    }

    /* Wake up the thread calling RTMpOnSpecific if we're the last CPU out of here: */
    if (ASMAtomicDecU32(&pArgs->cCpusLeftSynch) == 0)
        thread_wakeup((event_t)&pArgs->cCpusLeftSynch);
}


RTDECL(int) RTMpOnPair(RTCPUID idCpu1, RTCPUID idCpu2, uint32_t fFlags, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RT_ASSERT_INTS_ON();
    AssertReturn(idCpu1 != idCpu2, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTMPON_F_VALID_MASK), VERR_INVALID_FLAGS);

    /*
     * Check that both CPUs are valid first.
     */
    int rc;
    if (   RTMpIsCpuOnline(idCpu1)
        && RTMpIsCpuOnline(idCpu2))
    {
        if (g_pfnR0DarwinCpuXCall && g_pfnR0DarwinCpuNumberMayBeNull)
        {
            /** @todo With an MacBook M3 Max and 14.7.5 there is what looks like stack
             * corruption after doing some RTMpOnAll/RTMpOnPair/RTMpOnSpecific stuff when
             * supdrvOSAreTscDeltasInSync() returns true. Doesn't happen with M1 or M2 and
             * the same OS version, so possibly related to the VERR_CPU_OFFLINE issues
             * mentioned in RTMpGetOnlineCount.  For now, this isn't important enough to
             * spend more time tracking down... */
            RTMPONPAIRDARWIN Args;
# ifdef RT_STRICT
            Args.aidCpus[0]     = idCpu1;
            Args.aidCpus[1]     = idCpu2;
            Args.cCalls         = 0;
# endif
            Args.pfnWorker      = pfnWorker;
            Args.pvUser1        = pvUser1;
            Args.pvUser2        = pvUser2;
            Args.fState         = 0;
            ASMAtomicWriteU32(&Args.cCpusLeftSynch, 2);

            /*
             * Prepare waiting on Args.cCpusLeftSynch.
             */
            assert_wait((event_t)&Args.cCpusLeftSynch, THREAD_UNINT);

            /*
             * Disable preemption as we need to deal with cross calls to the same
             * CPU here to make this work.
             */
            RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
            RTThreadPreemptDisable(&PreemptState);

            RTCPUID const idCpuSelf = RTMpCpuId();
            uint32_t      cWaitFor  = 0;
            kern_return_t krc;
            if (idCpuSelf != idCpu1)
            {
                krc = g_pfnR0DarwinCpuXCall(idCpu1, rtmpOnPairDarwinWrapper<0>, &Args);
                if (krc == KERN_SUCCESS)
                {
                    cWaitFor = 1;
                    if (idCpuSelf != idCpu2)
                    {
                        krc = g_pfnR0DarwinCpuXCall(idCpu2, rtmpOnPairDarwinWrapper<1>, &Args);
                        if (krc == KERN_SUCCESS)
                        {
                            cWaitFor = 2;
                            rc = VINF_SUCCESS;
                        }
                        else
                        {
                            ASMAtomicWriteU32(&Args.fState, RTMPONPAIRDARWIN_STATE_SETUP_CANCEL);
                            rc = VERR_CPU_OFFLINE;
                        }
                    }
                    else
                    {
                        rtmpOnPairDarwinWrapper<1>(&Args);
                        rc = VINF_SUCCESS;
                    }
                }
                else
                    rc = VERR_CPU_OFFLINE;
            }
            else
            {
                krc = g_pfnR0DarwinCpuXCall(idCpu2, rtmpOnPairDarwinWrapper<1>, &Args);
                if (krc == KERN_SUCCESS)
                {
                    rtmpOnPairDarwinWrapper<0>(&Args);
                    cWaitFor = 1;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_CPU_OFFLINE;
            }

            RTThreadPreemptRestore(&PreemptState);

            /*
             * Synchronize with any CPUs we've dispatched to pfnWorker.
             */
            if (cWaitFor == 0 || ASMAtomicSubU32(&Args.cCpusLeftSynch, 2 - cWaitFor) == 0)
            {
                /* clear_wait(current_thread(), THREAD_AWAKENED); - not exported, so we have to do: */
                thread_wakeup((event_t)&Args.cCpusLeftSynch);
                thread_block(THREAD_CONTINUE_NULL);
            }
            else
                thread_block(THREAD_CONTINUE_NULL);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    /*
     * A CPU must be present to be considered just offline.
     */
    else if (   RTMpIsCpuPresent(idCpu1)
             && RTMpIsCpuPresent(idCpu2))
        rc = VERR_CPU_OFFLINE;
    else
        rc = VERR_CPU_NOT_FOUND;
    return rc;
}


RTDECL(bool) RTMpOnPairIsConcurrentExecSupported(void)
{
    return true;
}

#endif /* RT_ARCH_ARM64 || RT_ARCH_ARM32 */


RTDECL(int) RTMpPokeCpu(RTCPUID idCpu)
{
    RT_ASSERT_INTS_ON();

    if (g_pfnR0DarwinCpuInterrupt == NULL)
        return VERR_NOT_SUPPORTED;
    IPRT_DARWIN_SAVE_EFL_AC(); /* paranoia */
    /// @todo use mp_cpus_kick() when available (since 10.10)?  It's probably slower (locks, mask iteration, checks), though...
    g_pfnR0DarwinCpuInterrupt(idCpu);
    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}


RTDECL(bool) RTMpOnAllIsConcurrentSafe(void)
{
    return true;
}

