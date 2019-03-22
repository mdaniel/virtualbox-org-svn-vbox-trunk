/* $Id$ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Solaris.
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
#include <iprt/process.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include "internal/thread.h"

#define SOL_THREAD_ID_PTR           ((uint64_t *)((char *)curthread + g_offrtSolThreadId))
#define SOL_THREAD_LOCKP_PTR        ((disp_lock_t **)((char *)curthread + g_offrtSolThreadLock))

DECLHIDDEN(int) rtThreadNativeInit(void)
{
    return VINF_SUCCESS;
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative(RTThreadNativeSelf());
}


DECLHIDDEN(int) rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    int iPriority;
    disp_lock_t **ppDispLock;
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER:    iPriority = 60;             break;
        case RTTHREADTYPE_EMULATION:            iPriority = 66;             break;
        case RTTHREADTYPE_DEFAULT:              iPriority = 72;             break;
        case RTTHREADTYPE_MSG_PUMP:             iPriority = 78;             break;
        case RTTHREADTYPE_IO:                   iPriority = 84;             break;
        case RTTHREADTYPE_TIMER:                iPriority = 99;             break;
        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }

    Assert(curthread);
    thread_lock(curthread);
    thread_change_pri(curthread, iPriority, 0);

    /*
     * thread_unlock() is a macro calling disp_lock_exit() with the thread's dispatcher lock.
     * We need to dereference the offset manually here (for S10, S11 compatibility) rather than
     * using the macro.
     */
    ppDispLock = SOL_THREAD_LOCKP_PTR;
    disp_lock_exit(*ppDispLock);

    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    NOREF(pThread);
    /* There is nothing special that needs doing here, but the
       user really better know what he's cooking. */
    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtThreadNativeWaitKludge(PRTTHREADINT pThread)
{
    thread_join(pThread->tid);
}


DECLHIDDEN(void) rtThreadNativeDestroy(PRTTHREADINT pThread)
{
    NOREF(pThread);
}


/**
 * Native thread main function.
 *
 * @param   pvThreadInt     The thread structure.
 */
static void rtThreadNativeMain(void *pvThreadInt)
{
    PRTTHREADINT pThreadInt = (PRTTHREADINT)pvThreadInt;

    AssertCompile(sizeof(kt_did_t) == sizeof(pThreadInt->tid));
    uint64_t *pu64ThrId = SOL_THREAD_ID_PTR;
    pThreadInt->tid = *pu64ThrId;
    rtThreadMain(pThreadInt, RTThreadNativeSelf(), &pThreadInt->szName[0]);
    thread_exit();
}


DECLHIDDEN(int) rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread)
{
    kthread_t *pThread;
    RT_ASSERT_PREEMPTIBLE();

    pThreadInt->tid = UINT64_MAX;

    pThread = thread_create(NULL,                            /* Stack, use base */
                            0,                               /* Stack size */
                            rtThreadNativeMain,              /* Thread function */
                            pThreadInt,                      /* Function data */
                            0,                               /* Data size */
                            &p0,                             /* Process 0 handle */
                            TS_RUN,                          /* Ready to run */
                            minclsyspri                      /* Priority */
                            );
    if (RT_LIKELY(pThread))
    {
        *pNativeThread = (RTNATIVETHREAD)pThread;
        return VINF_SUCCESS;
    }

    return VERR_OUT_OF_RESOURCES;
}

