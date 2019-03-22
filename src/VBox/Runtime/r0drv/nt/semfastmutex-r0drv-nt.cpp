/* $Id$ */
/** @file
 * IPRT - Fast Mutex Semaphores, Ring-0 Driver, NT.
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
#include "the-nt-kernel.h"
#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/errcore.h>

#include "internal/magics.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Wrapper for the linux semaphore structure.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** the NT fast mutex. */
    FAST_MUTEX          Mutex;
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;



RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX phFastMtx)
{
    /*
     * Allocate.
     */
    PRTSEMFASTMUTEXINTERNAL pThis;
    Assert(sizeof(*pThis) > sizeof(void *));
    pThis = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    /*
     * Initialize.
     */
    pThis->u32Magic = RTSEMFASTMUTEX_MAGIC;
    ExInitializeFastMutex(&pThis->Mutex);

    *phFastMtx = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX hFastMtx)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    if (pThis == NIL_RTSEMFASTMUTEX)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);

    ASMAtomicWriteU32(&pThis->u32Magic, RTSEMFASTMUTEX_MAGIC_DEAD);
    Assert(pThis->Mutex.Count == 1);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX hFastMtx)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);

#if 1
    /*
     * ExAcquireFastMutex will set the IRQL to APC regardless of our current
     * level.  Lowering the IRQL may screw things up, so do not allow this.
     */
# if 0 /** @todo enable this when the logger has been fixed. */
    AssertMsg(KeGetCurrentIrql() <= APC_LEVEL, ("%d\n", KeGetCurrentIrql()), VERR_INVALID_STATE);
# else  /* the gentler approach. */
    KIRQL Irql = KeGetCurrentIrql();
    if (Irql > APC_LEVEL)
        return VERR_INVALID_STATE;
# endif
#endif

    ExAcquireFastMutex(&pThis->Mutex);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX hFastMtx)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);

    ExReleaseFastMutex(&pThis->Mutex);
    return VINF_SUCCESS;
}

