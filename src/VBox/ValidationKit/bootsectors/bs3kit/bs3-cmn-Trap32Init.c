/* $Id$ */
/** @file
 * BS3Kit - Bs3Trap32Init
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
#include "bs3kit-template-header.h"


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
#define g_Bs3Trap32DoubleFaultHandlerFlatAddr BS3_DATA_NM(g_Bs3Trap32DoubleFaultHandlerFlatAddr)
extern uint32_t g_Bs3Trap32DoubleFaultHandlerFlatAddr;


#undef Bs3Trap32Init
BS3_CMN_DEF(void, Bs3Trap32Init,(void))
{
     X86TSS32 BS3_FAR *pTss;
     unsigned iIdt;

    /*
     * IDT entries, except the system call gate.
     */
    for (iIdt = 0; iIdt < BS3_TRAP_SYSCALL; iIdt++)
        Bs3Trap32SetGate(iIdt, X86_SEL_TYPE_SYS_386_INT_GATE, 0 /*bDpl*/,
                         BS3_SEL_R0_CS32, g_Bs3Trap32GenericEntriesFlatAddr + iIdt * 10, 0 /*cParams*/);
    for (iIdt = BS3_TRAP_SYSCALL + 1; iIdt < 256; iIdt++)
        Bs3Trap32SetGate(iIdt, X86_SEL_TYPE_SYS_386_INT_GATE, 0 /*bDpl*/,
                         BS3_SEL_R0_CS32, g_Bs3Trap32GenericEntriesFlatAddr + iIdt * 10, 0 /*cParams*/);

    /*
     * Initialize the normal TSS so we can do ring transitions via the IDT.
     */
    pTss = &Bs3Tss32;
    Bs3MemZero(pTss, sizeof(*pTss));
    pTss->esp0      = BS3_ADDR_STACK_R0;
    pTss->ss0       = BS3_SEL_R0_SS32;
    pTss->esp1      = BS3_ADDR_STACK_R1;
    pTss->ss1       = BS3_SEL_R1_SS32 | 1;
    pTss->esp2      = BS3_ADDR_STACK_R2;
    pTss->ss2       = BS3_SEL_R2_SS32 | 2;

    /*
     * Initialize the double fault TSS.
     * cr3 is filled in by switcher code, when needed.
     */
    pTss = &Bs3Tss32DoubleFault;
    Bs3MemZero(pTss, sizeof(*pTss));
    pTss->esp0      = BS3_ADDR_STACK_R0;
    pTss->ss0       = BS3_SEL_R0_SS32;
    pTss->esp1      = BS3_ADDR_STACK_R1;
    pTss->ss1       = BS3_SEL_R1_SS32 | 1;
    pTss->esp2      = BS3_ADDR_STACK_R2;
    pTss->ss2       = BS3_SEL_R2_SS32 | 2;
    pTss->eip       = g_Bs3Trap32DoubleFaultHandlerFlatAddr;
    pTss->eflags    = X86_EFL_1;
    pTss->esp       = BS3_ADDR_STACK_R0_IST1;
    pTss->es        = BS3_SEL_R0_DS32;
    pTss->ds        = BS3_SEL_R0_DS32;
    pTss->cs        = BS3_SEL_R0_CS32;
    pTss->ss        = BS3_SEL_R0_SS32;

    Bs3Trap32SetGate(X86_XCPT_DF, X86_SEL_TYPE_SYS_TASK_GATE, 0 /*bDpl*/, BS3_SEL_TSS32_DF, 0, 0 /*cParams*/);
}

