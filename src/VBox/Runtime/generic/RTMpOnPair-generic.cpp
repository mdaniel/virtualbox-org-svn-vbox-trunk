/* $Id$ */
/** @file
 * IPRT - RTMpOnPair, generic implementation using RTMpOnAll.
 */

/*
 * Copyright (C) 2015-2019 Oracle Corporation
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
#include <iprt/mp.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Argument package for the generic RTMpOnPair implemenetation.
 */
typedef struct RTMPONPAIRGENERIC
{
    RTCPUID             idCpu1;
    RTCPUID             idCpu2;
    PFNRTMPWORKER       pfnWorker;
    void               *pvUser1;
    void               *pvUser2;
    /** Count of how many CPUs actually showed up. */
    uint32_t volatile   cPresent;
} RTMPONPAIRGENERIC;
/** Pointer to the an argument package for the generic RTMpOnPair
 *  implemenation. */
typedef RTMPONPAIRGENERIC *PRTMPONPAIRGENERIC;


/**
 * @callback_method_impl{FNRTMPWORKER,
 *      Used by RTMpOnPair to call the worker on the two specified CPUs.}
 */
static DECLCALLBACK(void) rtMpOnPairGenericWorker(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    RT_NOREF(pvUser2);
    PRTMPONPAIRGENERIC pArgs = (PRTMPONPAIRGENERIC)pvUser1;

    /*
     * Only the two choosen CPUs should call the worker function, count how
     * many of them that showed up.
     */
    if (   idCpu == pArgs->idCpu1
        || idCpu == pArgs->idCpu2)
    {
        ASMAtomicIncU32(&pArgs->cPresent);
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    }
}


RTDECL(int) RTMpOnPair(RTCPUID idCpu1, RTCPUID idCpu2, uint32_t fFlags, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    AssertReturn(idCpu1 != idCpu2, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTMPON_F_VALID_MASK), VERR_INVALID_FLAGS);
    if ((fFlags & RTMPON_F_CONCURRENT_EXEC) && !RTMpOnAllIsConcurrentSafe())
        return VERR_NOT_SUPPORTED;

    /*
     * Check that both CPUs are online before doing the broadcast call.
     */
    if (   RTMpIsCpuOnline(idCpu1)
        && RTMpIsCpuOnline(idCpu2))
    {
        RTMPONPAIRGENERIC Args;
        Args.idCpu1    = idCpu1;
        Args.idCpu2    = idCpu2;
        Args.pfnWorker = pfnWorker;
        Args.pvUser1   = pvUser1;
        Args.pvUser2   = pvUser2;
        Args.cPresent  = 0;
        rc = RTMpOnAll(rtMpOnPairGenericWorker, &Args, pvUser2);
        if (RT_SUCCESS(rc))
        {
            /*
             * Let's see if both of the CPUs showed up.
             */
            if (RT_LIKELY(Args.cPresent == 2))
            { /* likely */ }
            else if (Args.cPresent == 0)
                rc = VERR_CPU_OFFLINE;
            else if (Args.cPresent == 1)
                rc = VERR_NOT_ALL_CPUS_SHOWED;
            else
            {
                rc = VERR_CPU_IPE_1;
                AssertMsgFailed(("cPresent=%#x\n", Args.cPresent));
            }
        }
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
    return RTMpOnAllIsConcurrentSafe();
}

