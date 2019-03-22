/* $Id$ */
/** @file
 * IPRT - Thread Affinity, Linux ring-3 implementation.
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
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <features.h>
#if __GLIBC_PREREQ(2,4)

#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <iprt/thread.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/mp.h>



RTR3DECL(int) RTThreadSetAffinity(PCRTCPUSET pCpuSet)
{
    /* convert */
    cpu_set_t LnxCpuSet;
    CPU_ZERO(&LnxCpuSet);
    if (!pCpuSet)
        for (unsigned iCpu = 0; iCpu < CPU_SETSIZE; iCpu++)
            CPU_SET(iCpu, &LnxCpuSet);
    else
        for (unsigned iCpu = 0; iCpu < RT_MIN(CPU_SETSIZE, RTCPUSET_MAX_CPUS); iCpu++)
            if (RTCpuSetIsMemberByIndex(pCpuSet, iCpu))
                CPU_SET(iCpu, &LnxCpuSet);

    int rc = pthread_setaffinity_np(pthread_self(), sizeof(LnxCpuSet), &LnxCpuSet);
    if (!rc)
        return VINF_SUCCESS;
    rc = errno;
    if (rc == ENOENT)
        return VERR_CPU_NOT_FOUND;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int) RTThreadGetAffinity(PRTCPUSET pCpuSet)
{
    cpu_set_t LnxCpuSet;
    int rc = pthread_getaffinity_np(pthread_self(), sizeof(LnxCpuSet), &LnxCpuSet);
    if (rc != 0)
        return RTErrConvertFromErrno(errno);

    /* convert */
    RTCpuSetEmpty(pCpuSet);
    for (unsigned iCpu = 0; iCpu < RT_MIN(CPU_SETSIZE, RTCPUSET_MAX_CPUS); iCpu++)
        if (CPU_ISSET(iCpu, &LnxCpuSet))
            RTCpuSetAddByIndex(pCpuSet, iCpu);

    return VINF_SUCCESS;
}

#else
# include "../../generic/RTThreadGetAffinity-stub-generic.cpp"
# include "../../generic/RTThreadSetAffinity-stub-generic.cpp"
#endif

