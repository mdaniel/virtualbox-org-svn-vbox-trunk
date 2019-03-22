/* $Id$ */
/** @file
 * BS3Kit - Shutdown VM from PE16 - proof of concept (BS3Kit).
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
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
#include "bs3kit.h"
#include <iprt/assert.h>
#include <iprt/asm-amd64-x86.h>

AssertCompileSize(uint16_t, 2);
AssertCompileSize(uint32_t, 4);
AssertCompileSize(uint64_t, 8);

extern uint16_t ASMGetMsw();
#pragma aux ASMGetMsw = \
    ".286" \
    "smsw ax" \
    value [ax] \
    modify exact;

extern void ASMSetMsw(uint16_t uMsw);
#pragma aux ASMSetMsw = \
    ".286p" \
    "lmsw ax" \
    parm [ax] \
    modify exact;

/* Just a sample. */
BS3_DECL(void) Main_pe16(void)
{
    uint16_t uMsw = ASMGetMsw();
    Bs3Printf("msw=%#x cr0=%RX32 g_uBs3CpuDetected=%#x\n", uMsw, ASMGetCR0(), g_uBs3CpuDetected);
    Bs3Printf("cr2=%RX32 cr3=%RX32\n", ASMGetCR2(), ASMGetCR3());
    ASMSetMsw(X86_CR0_PE);
    Bs3Printf("lmsw(PE) => msw=%#x cr0=%RX32\n", ASMGetMsw(), ASMGetCR0());
    ASMSetMsw(UINT16_MAX);
    Bs3Printf("lmsw(0xffff) => msw=%#x cr0=%RX32\n", ASMGetMsw(), ASMGetCR0());
    ASMSetCR0(X86_CR0_PE);
    Bs3Printf("ASMSetCR0(X86_CR0_PE) => msw=%#x cr0=%RX32\n", ASMGetMsw(), ASMGetCR0());
    ASMSetCR0(UINT32_C(0x7fffffff));
    Bs3Printf("ASMSetCR0(0x7fffffff) => msw=%#x cr0=%RX32\n", ASMGetMsw(), ASMGetCR0());

    Bs3TestInit("bs3-shutdown");
    Bs3TestPrintf("detected cpu: %#x\n", g_uBs3CpuDetected);
#if 1
    ASMHalt();
#else
    Bs3Shutdown();
#endif
    return;
}

