/* $Id$ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Haiku.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/thread.h>

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
#include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include "internal/thread.h"


DECLHIDDEN(int) rtThreadNativeInit(void)
{
    /* No TLS in Ring-0. :-/ */
    return VINF_SUCCESS;
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative((RTNATIVETHREAD)find_thread(NULL));
}


DECLHIDDEN(int) rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    int32 iPriority;
    status_t status;

    /*
     * Convert the priority type to native priorities.
     * (This is quite naive but should be ok.)
     */
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER: iPriority = B_LOWEST_ACTIVE_PRIORITY;      break;
        case RTTHREADTYPE_EMULATION:         iPriority = B_LOW_PRIORITY;                break;
        case RTTHREADTYPE_DEFAULT:           iPriority = B_NORMAL_PRIORITY;             break;
        case RTTHREADTYPE_MSG_PUMP:          iPriority = B_DISPLAY_PRIORITY;            break;
        case RTTHREADTYPE_IO:                iPriority = B_URGENT_DISPLAY_PRIORITY;     break;
        case RTTHREADTYPE_TIMER:             iPriority = B_REAL_TIME_DISPLAY_PRIORITY;  break;
        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }

    status = set_thread_priority((thread_id)pThread->Core.Key, iPriority);

    return RTErrConvertFromHaikuKernReturn(status);
}


DECLHIDDEN(int) rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    return VERR_NOT_IMPLEMENTED;
}


DECLHIDDEN(void) rtThreadNativeWaitKludge(PRTTHREADINT pThread)
{
    /** @todo fix RTThreadWait/RTR0Term race on freebsd. */
    RTThreadSleep(1);
}


DECLHIDDEN(void) rtThreadNativeDestroy(PRTTHREADINT pThread)
{
    NOREF(pThread);
}


/**
 * Native kernel thread wrapper function.
 *
 * This will forward to rtThreadMain and do termination upon return.
 *
 * @param pvArg         Pointer to the argument package.
 * @param Ignored       Wait result, which we ignore.
 */
static status_t rtThreadNativeMain(void *pvArg)
{
    const thread_id Self = find_thread(NULL);
    PRTTHREADINT pThread = (PRTTHREADINT)pvArg;

    int rc = rtThreadMain(pThread, (RTNATIVETHREAD)Self, &pThread->szName[0]);

    if (rc < 0)
        return RTErrConvertFromHaikuKernReturn(rc);
    return rc;
}


DECLHIDDEN(int) rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread)
{
    thread_id NativeThread;
    RT_ASSERT_PREEMPTIBLE();

    NativeThread = spawn_kernel_thread(rtThreadNativeMain, pThreadInt->szName, B_NORMAL_PRIORITY, pThreadInt);
    if (NativeThread >= B_OK)
    {
        resume_thread(NativeThread);
        *pNativeThread = (RTNATIVETHREAD)NativeThread;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromHaikuKernReturn(NativeThread);
}

