/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Haiku.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include "the-haiku-kernel.h"

#include <iprt/mp.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/cpuset.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return smp_get_current_cpu();
}


RTDECL(int) RTMpCurSetIndex(void)
{
    return smp_get_current_cpu();
}


RTDECL(int) RTMpCurSetIndexAndId(PRTCPUID pidCpu)
{
    return *pidCpu = smp_get_current_cpu();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < smp_get_num_cpus() ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < smp_get_num_cpus() ? (RTCPUID)iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return smp_get_num_cpus() - 1;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu < smp_get_num_cpus();
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
    return smp_get_num_cpus();
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    return idCpu < smp_get_num_cpus();
    /** @todo FixMe && !CPU_ABSENT(idCpu) */
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);

    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    return smp_get_num_cpus();
}


/**
 * Wrapper between the native Haiku per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnAllHaikuWrapper(void *pvArg, int current)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    pArgs->pfnWorker(current, pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;
    /* is _sync needed ? */
    call_all_cpus_sync(rtmpOnAllHaikuWrapper, &Args);
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native Haiku per-cpu callback and PFNRTWORKER
 * for the RTMpOnOthers API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnOthersHaikuWrapper(void *pvArg, int current)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    RTCPUID idCpu = current;
    if (pArgs->idCpu != idCpu)
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    /* Will panic if no rendezvousing cpus, so check up front. */
    if (RTMpGetOnlineCount() > 1)
    {
        RTMPARGS    Args;

        Args.pfnWorker = pfnWorker;
        Args.pvUser1 = pvUser1;
        Args.pvUser2 = pvUser2;
        Args.idCpu = RTMpCpuId();
        Args.cHits = 0;
        /* is _sync needed ? */
        call_all_cpus_sync(rtmpOnOthersHaikuWrapper, &Args);
    }
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native Haiku per-cpu callback and PFNRTWORKER
 * for the RTMpOnSpecific API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificHaikuWrapper(void *pvArg, int current)
{
    PRTMPARGS   pArgs = (PRTMPARGS)pvArg;
    RTCPUID     idCpu = current;
    if (pArgs->idCpu == idCpu)
    {
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
        ASMAtomicIncU32(&pArgs->cHits);
    }
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS    Args;

    /* Will panic if no rendezvousing cpus, so make sure the cpu is online. */
    if (!RTMpIsCpuOnline(idCpu))
        return VERR_CPU_NOT_FOUND;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;
    /* is _sync needed ? */
    call_all_cpus_sync(rtmpOnSpecificHaikuWrapper, &Args);
    return Args.cHits == 1
           ? VINF_SUCCESS
           : VERR_CPU_NOT_FOUND;
}


RTDECL(bool) RTMpOnAllIsConcurrentSafe(void)
{
    return true;
}

