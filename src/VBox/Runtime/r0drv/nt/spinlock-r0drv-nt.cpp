/* $Id$ */
/** @file
 * IPRT - Spinlocks, Ring-0 Driver, NT.
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

#include <iprt/spinlock.h>

#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>

#include "internal/magics.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Apply the NoIrq hack if defined. */
#define RTSPINLOCK_NT_HACK_NOIRQ

#ifdef RTSPINLOCK_NT_HACK_NOIRQ
/** Indicates that the spinlock is taken. */
# define RTSPINLOCK_NT_HACK_NOIRQ_TAKEN  UINT32(0x00c0ffee)
/** Indicates that the spinlock is taken. */
# define RTSPINLOCK_NT_HACK_NOIRQ_FREE   UINT32(0xfe0000fe)
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Wrapper for the KSPIN_LOCK type.
 */
typedef struct RTSPINLOCKINTERNAL
{
    /** Spinlock magic value (RTSPINLOCK_MAGIC). */
    uint32_t volatile   u32Magic;
#ifdef RTSPINLOCK_NT_HACK_NOIRQ
    /** Spinlock hack. */
    uint32_t volatile   u32Hack;
#endif
    /** The saved IRQL. */
    KIRQL volatile      SavedIrql;
    /** The saved interrupt flag. */
    RTCCUINTREG volatile fIntSaved;
    /** The spinlock creation flags. */
    uint32_t            fFlags;
    /** The NT spinlock structure. */
    KSPIN_LOCK          Spinlock;
} RTSPINLOCKINTERNAL, *PRTSPINLOCKINTERNAL;


RTDECL(int)  RTSpinlockCreate(PRTSPINLOCK pSpinlock, uint32_t fFlags, const char *pszName)
{
    AssertReturn(fFlags == RTSPINLOCK_FLAGS_INTERRUPT_SAFE || fFlags == RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, VERR_INVALID_PARAMETER);
    RT_NOREF1(pszName);

    /*
     * Allocate.
     */
    Assert(sizeof(RTSPINLOCKINTERNAL) > sizeof(void *));
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    /*
     * Initialize & return.
     */
    pThis->u32Magic     = RTSPINLOCK_MAGIC;
#ifdef RTSPINLOCK_NT_HACK_NOIRQ
    pThis->u32Hack      = RTSPINLOCK_NT_HACK_NOIRQ_FREE;
#endif
    pThis->SavedIrql    = 0;
    pThis->fIntSaved    = 0;
    pThis->fFlags       = fFlags;
    KeInitializeSpinLock(&pThis->Spinlock);

    *pSpinlock = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSpinlockDestroy(RTSPINLOCK Spinlock)
{
    /*
     * Validate input.
     */
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    if (pThis->u32Magic != RTSPINLOCK_MAGIC)
    {
        AssertMsgFailed(("Invalid spinlock %p magic=%#x\n", pThis, pThis->u32Magic));
        return VERR_INVALID_PARAMETER;
    }

    ASMAtomicIncU32(&pThis->u32Magic);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(void) RTSpinlockAcquire(RTSPINLOCK Spinlock)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pThis->u32Magic));

    KIRQL SavedIrql;
    if (pThis->fFlags & RTSPINLOCK_FLAGS_INTERRUPT_SAFE)
    {
#ifndef RTSPINLOCK_NT_HACK_NOIRQ
        RTCCUINTREG fIntSaved = ASMGetFlags();
        ASMIntDisable();
        KeAcquireSpinLock(&pThis->Spinlock, &SavedIrql);
#else
        SavedIrql = KeGetCurrentIrql();
        if (SavedIrql < DISPATCH_LEVEL)
        {
            KeRaiseIrql(DISPATCH_LEVEL, &SavedIrql);
            Assert(SavedIrql < DISPATCH_LEVEL);
        }
        RTCCUINTREG fIntSaved = ASMGetFlags();
        ASMIntDisable();

        if (!ASMAtomicCmpXchgU32(&pThis->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_TAKEN, RTSPINLOCK_NT_HACK_NOIRQ_FREE))
        {
            while (!ASMAtomicCmpXchgU32(&pThis->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_TAKEN, RTSPINLOCK_NT_HACK_NOIRQ_FREE))
                ASMNopPause();
        }
#endif
        pThis->fIntSaved = fIntSaved;
    }
    else
        KeAcquireSpinLock(&pThis->Spinlock, &SavedIrql);
    pThis->SavedIrql = SavedIrql;
}


RTDECL(void) RTSpinlockRelease(RTSPINLOCK Spinlock)
{
    PRTSPINLOCKINTERNAL pThis = (PRTSPINLOCKINTERNAL)Spinlock;
    AssertMsg(pThis && pThis->u32Magic == RTSPINLOCK_MAGIC, ("magic=%#x\n", pThis->u32Magic));

    KIRQL SavedIrql = pThis->SavedIrql;
    if (pThis->fFlags & RTSPINLOCK_FLAGS_INTERRUPT_SAFE)
    {
        RTCCUINTREG fIntSaved = pThis->fIntSaved;
        pThis->fIntSaved = 0;

#ifndef RTSPINLOCK_NT_HACK_NOIRQ
        KeReleaseSpinLock(&pThis->Spinlock, SavedIrql);
        ASMSetFlags(fIntSaved);
#else
        Assert(pThis->u32Hack == RTSPINLOCK_NT_HACK_NOIRQ_TAKEN);

        ASMAtomicWriteU32(&pThis->u32Hack, RTSPINLOCK_NT_HACK_NOIRQ_FREE);
        ASMSetFlags(fIntSaved);
        if (SavedIrql < DISPATCH_LEVEL)
            KeLowerIrql(SavedIrql);
#endif
    }
    else
        KeReleaseSpinLock(&pThis->Spinlock, SavedIrql);
}

