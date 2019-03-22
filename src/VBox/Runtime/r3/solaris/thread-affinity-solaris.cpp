/* $Id$ */
/** @file
 * IPRT - Thread Affinity, Solaris ring-3 implementation.
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
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
#include <iprt/thread.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/mp.h>

#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <unistd.h>
#include <errno.h>


/* Note! The current implementation can only bind to a single CPU. */


RTR3DECL(int) RTThreadSetAffinity(PCRTCPUSET pCpuSet)
{
    int rc;
    if (pCpuSet == NULL)
        rc = processor_bind(P_LWPID, P_MYID, PBIND_NONE, NULL);
    else
    {
        RTCPUSET PresentSet;
        int cCpusInSet = RTCpuSetCount(pCpuSet);
        if (cCpusInSet == 1)
        {
            unsigned iCpu = 0;
            while (   iCpu < RTCPUSET_MAX_CPUS
                   && !RTCpuSetIsMemberByIndex(pCpuSet, iCpu))
                iCpu++;
            rc = processor_bind(P_LWPID, P_MYID, iCpu, NULL);
        }
        else if (   cCpusInSet == RTCPUSET_MAX_CPUS
                 || RTCpuSetIsEqual(pCpuSet, RTMpGetPresentSet(&PresentSet)))
            rc = processor_bind(P_LWPID, P_MYID, PBIND_NONE, NULL);
        else
            return VERR_NOT_SUPPORTED;
    }
    if (!rc)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int) RTThreadGetAffinity(PRTCPUSET pCpuSet)
{
    processorid_t iOldCpu;
    int rc = processor_bind(P_LWPID, P_MYID, PBIND_QUERY, &iOldCpu);
    if (rc)
        return RTErrConvertFromErrno(errno);
    if (iOldCpu == PBIND_NONE)
        RTMpGetPresentSet(pCpuSet);
    else
    {
        RTCpuSetEmpty(pCpuSet);
        if (RTCpuSetAdd(pCpuSet, iOldCpu) != 0)
            return VERR_INTERNAL_ERROR_5;
    }
    return VINF_SUCCESS;
}

