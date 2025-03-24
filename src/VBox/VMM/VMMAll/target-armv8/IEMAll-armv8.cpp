/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - ARMv8 target, miscellaneous.
 */

/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP   LOG_GROUP_IEM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/armv8.h>



/**
 * Slow iemCalcExecDbgFlags() code path.
 */
uint32_t iemCalcExecDbgFlagsSlow(PVMCPUCC pVCpu)
{
    uint32_t fExec = 0;

#if 0 /** @todo ARM hardware breakpoints/watchpoints. */
    /*
     * Helper for invalidate the data TLB for breakpoint addresses.
     *
     * This is to make sure any access to the page will always trigger a TLB
     * load for as long as the breakpoint is enabled.
     */
#ifdef IEM_WITH_DATA_TLB
# define INVALID_TLB_ENTRY_FOR_BP(a_uValue) do { \
        RTGCPTR uTagNoRev = (a_uValue); \
        uTagNoRev = IEMTLB_CALC_TAG_NO_REV(uTagNoRev); \
        /** @todo do large page accounting */ \
        uintptr_t const idxEven = IEMTLB_TAG_TO_EVEN_INDEX(uTagNoRev); \
        if (pVCpu->iem.s.DataTlb.aEntries[idxEven].uTag == (uTagNoRev | pVCpu->iem.s.DataTlb.uTlbRevision)) \
            pVCpu->iem.s.DataTlb.aEntries[idxEven].uTag = 0; \
        if (pVCpu->iem.s.DataTlb.aEntries[idxEven + 1].uTag == (uTagNoRev | pVCpu->iem.s.DataTlb.uTlbRevisionGlobal)) \
            pVCpu->iem.s.DataTlb.aEntries[idxEven + 1].uTag = 0; \
    } while (0)
#else
# define INVALID_TLB_ENTRY_FOR_BP(a_uValue) do { } while (0)
#endif

    /*
     * Process guest breakpoints.
     */
#define PROCESS_ONE_BP(a_fDr7, a_iBp, a_uValue) do { \
        if (a_fDr7 & X86_DR7_L_G(a_iBp)) \
        { \
            switch (X86_DR7_GET_RW(a_fDr7, a_iBp)) \
            { \
                case X86_DR7_RW_EO: \
                    fExec |= IEM_F_PENDING_BRK_INSTR; \
                    break; \
                case X86_DR7_RW_WO: \
                case X86_DR7_RW_RW: \
                    fExec |= IEM_F_PENDING_BRK_DATA; \
                    INVALID_TLB_ENTRY_FOR_BP(a_uValue); \
                    break; \
                case X86_DR7_RW_IO: \
                    fExec |= IEM_F_PENDING_BRK_X86_IO; \
                    break; \
            } \
        } \
    } while (0)

    uint32_t const fGstDr7 = (uint32_t)pVCpu->cpum.GstCtx.dr[7];
    if (fGstDr7 & X86_DR7_ENABLED_MASK)
    {
/** @todo extract more details here to simplify matching later. */
#ifdef IEM_WITH_DATA_TLB
        IEM_CTX_IMPORT_NORET(pVCpu, CPUMCTX_EXTRN_DR0_DR3);
#endif
        PROCESS_ONE_BP(fGstDr7, 0, pVCpu->cpum.GstCtx.dr[0]);
        PROCESS_ONE_BP(fGstDr7, 1, pVCpu->cpum.GstCtx.dr[1]);
        PROCESS_ONE_BP(fGstDr7, 2, pVCpu->cpum.GstCtx.dr[2]);
        PROCESS_ONE_BP(fGstDr7, 3, pVCpu->cpum.GstCtx.dr[3]);
    }

    /*
     * Process hypervisor breakpoints.
     */
    PVMCC const    pVM       = pVCpu->CTX_SUFF(pVM);
    uint32_t const fHyperDr7 = DBGFBpGetDR7(pVM);
    if (fHyperDr7 & X86_DR7_ENABLED_MASK)
    {
/** @todo extract more details here to simplify matching later. */
        PROCESS_ONE_BP(fHyperDr7, 0, DBGFBpGetDR0(pVM));
        PROCESS_ONE_BP(fHyperDr7, 1, DBGFBpGetDR1(pVM));
        PROCESS_ONE_BP(fHyperDr7, 2, DBGFBpGetDR2(pVM));
        PROCESS_ONE_BP(fHyperDr7, 3, DBGFBpGetDR3(pVM));
    }
#else
    RT_NOREF(pVCpu);
#endif

    return fExec;
}

