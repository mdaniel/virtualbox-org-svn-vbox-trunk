/* $Id$ */
/** @file
 * CPUM - CPU database part.
 */

/*
 * Copyright (C) 2013-2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/mm.h>

#include <VBox/err.h>
#if !defined(RT_ARCH_ARM64)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/mem.h>
#include <iprt/string.h>

#include "CPUMR3Msr-x86.h"


/**
 * Binary search used by cpumR3MsrRangesInsert and has some special properties
 * wrt to mismatches.
 *
 * @returns Insert location.
 * @param   paMsrRanges         The MSR ranges to search.
 * @param   cMsrRanges          The number of MSR ranges.
 * @param   uMsr                What to search for.
 */
static uint32_t cpumR3MsrRangesBinSearch(PCCPUMMSRRANGE paMsrRanges, uint32_t cMsrRanges, uint32_t uMsr)
{
    if (!cMsrRanges)
        return 0;

    uint32_t iStart = 0;
    uint32_t iLast  = cMsrRanges - 1;
    for (;;)
    {
        uint32_t i = iStart + (iLast - iStart + 1) / 2;
        if (   uMsr >= paMsrRanges[i].uFirst
            && uMsr <= paMsrRanges[i].uLast)
            return i;
        if (uMsr < paMsrRanges[i].uFirst)
        {
            if (i <= iStart)
                return i;
            iLast = i - 1;
        }
        else
        {
            if (i >= iLast)
            {
                if (i < cMsrRanges)
                    i++;
                return i;
            }
            iStart = i + 1;
        }
    }
}


/**
 * Ensures that there is space for at least @a cNewRanges in the table,
 * reallocating the table if necessary.
 *
 * @returns Pointer to the MSR ranges on success, NULL on failure.  On failure
 *          @a *ppaMsrRanges is freed and set to NULL.
 * @param   pVM             The cross context VM structure.  If NULL,
 *                          use the process heap, otherwise the VM's hyper heap.
 * @param   ppaMsrRanges    The variable pointing to the ranges (input/output).
 * @param   cMsrRanges      The current number of ranges.
 * @param   cNewRanges      The number of ranges to be added.
 */
static PCPUMMSRRANGE cpumR3MsrRangesEnsureSpace(PVM pVM, PCPUMMSRRANGE *ppaMsrRanges, uint32_t cMsrRanges, uint32_t cNewRanges)
{
    if (  cMsrRanges + cNewRanges
        > RT_ELEMENTS(pVM->cpum.s.GuestInfo.aMsrRanges) + (pVM ? 0 : 128 /* Catch too many MSRs in CPU reporter! */))
    {
        LogRel(("CPUM: Too many MSR ranges! %#x, max %#x\n",
                cMsrRanges + cNewRanges, RT_ELEMENTS(pVM->cpum.s.GuestInfo.aMsrRanges)));
        return NULL;
    }
    if (pVM)
    {
        Assert(cMsrRanges == pVM->cpum.s.GuestInfo.cMsrRanges);
        Assert(*ppaMsrRanges == pVM->cpum.s.GuestInfo.aMsrRanges);
    }
    else
    {
        if (cMsrRanges + cNewRanges > RT_ALIGN_32(cMsrRanges, 16))
        {

            uint32_t const cNew = RT_ALIGN_32(cMsrRanges + cNewRanges, 16);
            void *pvNew = RTMemRealloc(*ppaMsrRanges, cNew * sizeof(**ppaMsrRanges));
            if (pvNew)
                *ppaMsrRanges = (PCPUMMSRRANGE)pvNew;
            else
            {
                RTMemFree(*ppaMsrRanges);
                *ppaMsrRanges = NULL;
                return NULL;
            }
        }
    }

    return *ppaMsrRanges;
}


/**
 * Inserts a new MSR range in into an sorted MSR range array.
 *
 * If the new MSR range overlaps existing ranges, the existing ones will be
 * adjusted/removed to fit in the new one.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_MEMORY
 *
 * @param   pVM             The cross context VM structure.  If NULL,
 *                          use the process heap, otherwise the VM's hyper heap.
 * @param   ppaMsrRanges    The variable pointing to the ranges (input/output).
 *                          Must be NULL if using the hyper heap.
 * @param   pcMsrRanges     The variable holding number of ranges. Must be NULL
 *                          if using the hyper heap.
 * @param   pNewRange       The new range.
 */
int cpumR3MsrRangesInsert(PVM pVM, PCPUMMSRRANGE *ppaMsrRanges, uint32_t *pcMsrRanges, PCCPUMMSRRANGE pNewRange)
{
    Assert(pNewRange->uLast >= pNewRange->uFirst);
    Assert(pNewRange->enmRdFn > kCpumMsrRdFn_Invalid && pNewRange->enmRdFn < kCpumMsrRdFn_End);
    Assert(pNewRange->enmWrFn > kCpumMsrWrFn_Invalid && pNewRange->enmWrFn < kCpumMsrWrFn_End);

    /*
     * Validate and use the VM's MSR ranges array if we are using the hyper heap.
     */
    if (pVM)
    {
        AssertReturn(!ppaMsrRanges, VERR_INVALID_PARAMETER);
        AssertReturn(!pcMsrRanges,  VERR_INVALID_PARAMETER);
        AssertReturn(pVM->cpum.s.GuestInfo.paMsrRangesR3 == pVM->cpum.s.GuestInfo.aMsrRanges, VERR_INTERNAL_ERROR_3);

        ppaMsrRanges = &pVM->cpum.s.GuestInfo.paMsrRangesR3;
        pcMsrRanges  = &pVM->cpum.s.GuestInfo.cMsrRanges;
    }
    else
    {
        AssertReturn(ppaMsrRanges, VERR_INVALID_POINTER);
        AssertReturn(pcMsrRanges, VERR_INVALID_POINTER);
    }

    uint32_t        cMsrRanges  = *pcMsrRanges;
    PCPUMMSRRANGE   paMsrRanges = *ppaMsrRanges;

    /*
     * Optimize the linear insertion case where we add new entries at the end.
     */
    if (   cMsrRanges > 0
        && paMsrRanges[cMsrRanges - 1].uLast < pNewRange->uFirst)
    {
        paMsrRanges = cpumR3MsrRangesEnsureSpace(pVM, ppaMsrRanges, cMsrRanges, 1);
        if (!paMsrRanges)
            return VERR_NO_MEMORY;
        paMsrRanges[cMsrRanges] = *pNewRange;
        *pcMsrRanges += 1;
    }
    else
    {
        uint32_t i = cpumR3MsrRangesBinSearch(paMsrRanges, cMsrRanges, pNewRange->uFirst);
        Assert(i == cMsrRanges || pNewRange->uFirst <= paMsrRanges[i].uLast);
        Assert(i == 0 || pNewRange->uFirst > paMsrRanges[i - 1].uLast);

        /*
         * Adding an entirely new entry?
         */
        if (   i >= cMsrRanges
            || pNewRange->uLast < paMsrRanges[i].uFirst)
        {
            paMsrRanges = cpumR3MsrRangesEnsureSpace(pVM, ppaMsrRanges, cMsrRanges, 1);
            if (!paMsrRanges)
                return VERR_NO_MEMORY;
            if (i < cMsrRanges)
                memmove(&paMsrRanges[i + 1], &paMsrRanges[i], (cMsrRanges - i) * sizeof(paMsrRanges[0]));
            paMsrRanges[i] = *pNewRange;
            *pcMsrRanges += 1;
        }
        /*
         * Replace existing entry?
         */
        else if (   pNewRange->uFirst == paMsrRanges[i].uFirst
                 && pNewRange->uLast  == paMsrRanges[i].uLast)
            paMsrRanges[i] = *pNewRange;
        /*
         * Splitting an existing entry?
         */
        else if (   pNewRange->uFirst > paMsrRanges[i].uFirst
                 && pNewRange->uLast  < paMsrRanges[i].uLast)
        {
            paMsrRanges = cpumR3MsrRangesEnsureSpace(pVM, ppaMsrRanges, cMsrRanges, 2);
            if (!paMsrRanges)
                return VERR_NO_MEMORY;
            Assert(i < cMsrRanges);
            memmove(&paMsrRanges[i + 2], &paMsrRanges[i], (cMsrRanges - i) * sizeof(paMsrRanges[0]));
            paMsrRanges[i + 1] = *pNewRange;
            paMsrRanges[i + 2] = paMsrRanges[i];
            paMsrRanges[i    ].uLast  = pNewRange->uFirst - 1;
            paMsrRanges[i + 2].uFirst = pNewRange->uLast  + 1;
            *pcMsrRanges += 2;
        }
        /*
         * Complicated scenarios that can affect more than one range.
         *
         * The current code does not optimize memmove calls when replacing
         * one or more existing ranges, because it's tedious to deal with and
         * not expected to be a frequent usage scenario.
         */
        else
        {
            /* Adjust start of first match? */
            if (   pNewRange->uFirst <= paMsrRanges[i].uFirst
                && pNewRange->uLast  <  paMsrRanges[i].uLast)
                paMsrRanges[i].uFirst = pNewRange->uLast + 1;
            else
            {
                /* Adjust end of first match? */
                if (pNewRange->uFirst > paMsrRanges[i].uFirst)
                {
                    Assert(paMsrRanges[i].uLast >= pNewRange->uFirst);
                    paMsrRanges[i].uLast = pNewRange->uFirst - 1;
                    i++;
                }
                /* Replace the whole first match (lazy bird). */
                else
                {
                    if (i + 1 < cMsrRanges)
                        memmove(&paMsrRanges[i], &paMsrRanges[i + 1], (cMsrRanges - i - 1) * sizeof(paMsrRanges[0]));
                    cMsrRanges = *pcMsrRanges -= 1;
                }

                /* Do the new range affect more ranges? */
                while (   i < cMsrRanges
                       && pNewRange->uLast >= paMsrRanges[i].uFirst)
                {
                    if (pNewRange->uLast < paMsrRanges[i].uLast)
                    {
                        /* Adjust the start of it, then we're done. */
                        paMsrRanges[i].uFirst = pNewRange->uLast + 1;
                        break;
                    }

                    /* Remove it entirely. */
                    if (i + 1 < cMsrRanges)
                        memmove(&paMsrRanges[i], &paMsrRanges[i + 1], (cMsrRanges - i - 1) * sizeof(paMsrRanges[0]));
                    cMsrRanges = *pcMsrRanges -= 1;
                }
            }

            /* Now, perform a normal insertion. */
            paMsrRanges = cpumR3MsrRangesEnsureSpace(pVM, ppaMsrRanges, cMsrRanges, 1);
            if (!paMsrRanges)
                return VERR_NO_MEMORY;
            if (i < cMsrRanges)
                memmove(&paMsrRanges[i + 1], &paMsrRanges[i], (cMsrRanges - i) * sizeof(paMsrRanges[0]));
            paMsrRanges[i] = *pNewRange;
            *pcMsrRanges += 1;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Reconciles CPUID info with MSRs (selected ones).
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   fForceFlushCmd      Make sure MSR_IA32_FLUSH_CMD is present.
 * @param   fForceSpecCtrl      Make sure MSR_IA32_SPEC_CTRL is present.
 */
DECLHIDDEN(int) cpumR3MsrReconcileWithCpuId(PVM pVM, bool fForceFlushCmd, bool fForceSpecCtrl)
{
    PCCPUMMSRRANGE apToAdd[10];
    uint32_t       cToAdd = 0;

    /*
     * The IA32_FLUSH_CMD MSR was introduced in MCUs for CVS-2018-3646 and associates.
     */
    if (   pVM->cpum.s.GuestFeatures.fFlushCmd
        || fForceFlushCmd)
    {
        static CPUMMSRRANGE const s_FlushCmd =
        {
            /*.uFirst =*/       MSR_IA32_FLUSH_CMD,
            /*.uLast =*/        MSR_IA32_FLUSH_CMD,
            /*.enmRdFn =*/      kCpumMsrRdFn_WriteOnly,
            /*.enmWrFn =*/      kCpumMsrWrFn_Ia32FlushCmd,
            /*.offCpumCpu =*/   UINT16_MAX,
            /*.fReserved =*/    0,
            /*.uValue =*/       0,
            /*.fWrIgnMask =*/   0,
            /*.fWrGpMask =*/    ~MSR_IA32_FLUSH_CMD_F_L1D,
            /*.szName = */      "IA32_FLUSH_CMD"
        };
        apToAdd[cToAdd++] = &s_FlushCmd;
    }

    /*
     * The IA32_PRED_CMD MSR was introduced in MCUs for CVS-2018-3646 and associates.
     */
    if (   pVM->cpum.s.GuestFeatures.fIbpb
        /** @todo || pVM->cpum.s.GuestFeatures.fSbpb*/)
    {
        static CPUMMSRRANGE const s_PredCmd =
        {
            /*.uFirst =*/       MSR_IA32_PRED_CMD,
            /*.uLast =*/        MSR_IA32_PRED_CMD,
            /*.enmRdFn =*/      kCpumMsrRdFn_WriteOnly,
            /*.enmWrFn =*/      kCpumMsrWrFn_Ia32PredCmd,
            /*.offCpumCpu =*/   UINT16_MAX,
            /*.fReserved =*/    0,
            /*.uValue =*/       0,
            /*.fWrIgnMask =*/   0,
            /*.fWrGpMask =*/    ~MSR_IA32_PRED_CMD_F_IBPB,
            /*.szName = */      "IA32_PRED_CMD"
        };
        apToAdd[cToAdd++] = &s_PredCmd;
    }

    /*
     * The IA32_SPEC_CTRL MSR was introduced in MCUs for CVS-2018-3646 and associates.
     */
    if (   pVM->cpum.s.GuestFeatures.fSpecCtrlMsr
        || fForceSpecCtrl)
    {
        static CPUMMSRRANGE const s_SpecCtrl =
        {
            /*.uFirst =*/       MSR_IA32_SPEC_CTRL,
            /*.uLast =*/        MSR_IA32_SPEC_CTRL,
            /*.enmRdFn =*/      kCpumMsrRdFn_Ia32SpecCtrl,
            /*.enmWrFn =*/      kCpumMsrWrFn_Ia32SpecCtrl,
            /*.offCpumCpu =*/   UINT16_MAX,
            /*.fReserved =*/    0,
            /*.uValue =*/       0,
            /*.fWrIgnMask =*/   0,
            /*.fWrGpMask =*/    0,
            /*.szName = */      "IA32_SPEC_CTRL"
        };
        apToAdd[cToAdd++] = &s_SpecCtrl;
    }

    /*
     * The MSR_IA32_ARCH_CAPABILITIES was introduced in various spectre MCUs, or at least
     * documented in relation to such.
     */
    if (pVM->cpum.s.GuestFeatures.fArchCap)
    {
        static CPUMMSRRANGE const s_ArchCaps =
        {
            /*.uFirst =*/       MSR_IA32_ARCH_CAPABILITIES,
            /*.uLast =*/        MSR_IA32_ARCH_CAPABILITIES,
            /*.enmRdFn =*/      kCpumMsrRdFn_Ia32ArchCapabilities,
            /*.enmWrFn =*/      kCpumMsrWrFn_ReadOnly,
            /*.offCpumCpu =*/   UINT16_MAX,
            /*.fReserved =*/    0,
            /*.uValue =*/       0,
            /*.fWrIgnMask =*/   0,
            /*.fWrGpMask =*/    UINT64_MAX,
            /*.szName = */      "IA32_ARCH_CAPABILITIES"
        };
        apToAdd[cToAdd++] = &s_ArchCaps;
    }

    /*
     * Do the adding.
     */
    Assert(cToAdd <= RT_ELEMENTS(apToAdd));
    for (uint32_t i = 0; i < cToAdd; i++)
    {
        PCCPUMMSRRANGE pRange = apToAdd[i];
        Assert(pRange->uFirst == pRange->uLast);
        if (!cpumLookupMsrRange(pVM, pRange->uFirst))
        {
            LogRel(("CPUM: MSR/CPUID reconciliation insert: %#010x %s\n", pRange->uFirst, pRange->szName));
            int rc = cpumR3MsrRangesInsert(NULL /* pVM */, &pVM->cpum.s.GuestInfo.paMsrRangesR3,
                                           &pVM->cpum.s.GuestInfo.cMsrRanges, pRange);
            AssertRCReturn(rc, rc);
        }
    }
    return VINF_SUCCESS;
}


/**
 * Worker for cpumR3MsrApplyFudge that applies one table.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   paRanges            Array of MSRs to fudge.
 * @param   cRanges             Number of MSRs in the array.
 */
static int cpumR3MsrApplyFudgeTable(PVM pVM, PCCPUMMSRRANGE paRanges, size_t cRanges)
{
    for (uint32_t i = 0; i < cRanges; i++)
        if (!cpumLookupMsrRange(pVM, paRanges[i].uFirst))
        {
            LogRel(("CPUM: MSR fudge: %#010x %s\n", paRanges[i].uFirst, paRanges[i].szName));
            int rc = cpumR3MsrRangesInsert(NULL /* pVM */, &pVM->cpum.s.GuestInfo.paMsrRangesR3, &pVM->cpum.s.GuestInfo.cMsrRanges,
                                           &paRanges[i]);
            if (RT_FAILURE(rc))
                return rc;
        }
    return VINF_SUCCESS;
}


/**
 * Fudges the MSRs that guest are known to access in some odd cases.
 *
 * A typical example is a VM that has been moved between different hosts where
 * for instance the cpu vendor differs.
 *
 * Another example is older CPU profiles (e.g. Atom Bonnet) for newer CPUs (e.g.
 * Atom Silvermont), where features reported thru CPUID aren't present in the
 * MSRs (e.g. AMD64_TSC_AUX).
 *
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
int cpumR3MsrApplyFudge(PVM pVM)
{
    /*
     * Basic.
     */
    static CPUMMSRRANGE const s_aFudgeMsrs[] =
    {
        MFO(0x00000000, "IA32_P5_MC_ADDR",          Ia32P5McAddr),
        MFX(0x00000001, "IA32_P5_MC_TYPE",          Ia32P5McType,   Ia32P5McType,   0, 0, UINT64_MAX),
        MVO(0x00000017, "IA32_PLATFORM_ID",         0),
        MFN(0x0000001b, "IA32_APIC_BASE",           Ia32ApicBase,   Ia32ApicBase),
        MVI(0x0000008b, "BIOS_SIGN",                0),
        MFX(0x000000fe, "IA32_MTRRCAP",             Ia32MtrrCap,    ReadOnly,       0x508, 0, 0),
        MFX(0x00000179, "IA32_MCG_CAP",             Ia32McgCap,     ReadOnly,       0x005, 0, 0),
        MFX(0x0000017a, "IA32_MCG_STATUS",          Ia32McgStatus,  Ia32McgStatus,  0, ~(uint64_t)UINT32_MAX, 0),
        MFN(0x000001a0, "IA32_MISC_ENABLE",         Ia32MiscEnable, Ia32MiscEnable),
        MFN(0x000001d9, "IA32_DEBUGCTL",            Ia32DebugCtl,   Ia32DebugCtl),
        MFO(0x000001db, "P6_LAST_BRANCH_FROM_IP",   P6LastBranchFromIp),
        MFO(0x000001dc, "P6_LAST_BRANCH_TO_IP",     P6LastBranchToIp),
        MFO(0x000001dd, "P6_LAST_INT_FROM_IP",      P6LastIntFromIp),
        MFO(0x000001de, "P6_LAST_INT_TO_IP",        P6LastIntToIp),
        MFS(0x00000277, "IA32_PAT",                 Ia32Pat, Ia32Pat, Guest.msrPAT),
        MFZ(0x000002ff, "IA32_MTRR_DEF_TYPE",       Ia32MtrrDefType, Ia32MtrrDefType, GuestMsrs.msr.MtrrDefType, 0, ~(uint64_t)0xc07),
        MFN(0x00000400, "IA32_MCi_CTL_STATUS_ADDR_MISC", Ia32McCtlStatusAddrMiscN, Ia32McCtlStatusAddrMiscN),
    };
    int rc = cpumR3MsrApplyFudgeTable(pVM, &s_aFudgeMsrs[0], RT_ELEMENTS(s_aFudgeMsrs));
    AssertLogRelRCReturn(rc, rc);

    /*
     * XP might mistake opterons and other newer CPUs for P4s.
     */
    if (pVM->cpum.s.GuestFeatures.uFamily >= 0xf)
    {
        static CPUMMSRRANGE const s_aP4FudgeMsrs[] =
        {
            MFX(0x0000002c, "P4_EBC_FREQUENCY_ID", IntelP4EbcFrequencyId, IntelP4EbcFrequencyId, 0xf12010f, UINT64_MAX, 0),
        };
        rc = cpumR3MsrApplyFudgeTable(pVM, &s_aP4FudgeMsrs[0], RT_ELEMENTS(s_aP4FudgeMsrs));
        AssertLogRelRCReturn(rc, rc);
    }

    if (pVM->cpum.s.GuestFeatures.fRdTscP)
    {
        static CPUMMSRRANGE const s_aRdTscPFudgeMsrs[] =
        {
            MFX(0xc0000103, "AMD64_TSC_AUX", Amd64TscAux, Amd64TscAux, 0, 0, ~(uint64_t)UINT32_MAX),
        };
        rc = cpumR3MsrApplyFudgeTable(pVM, &s_aRdTscPFudgeMsrs[0], RT_ELEMENTS(s_aRdTscPFudgeMsrs));
        AssertLogRelRCReturn(rc, rc);
    }

    /*
     * Windows 10 incorrectly writes to MSR_IA32_TSX_CTRL without checking
     * CPUID.ARCH_CAP(EAX=7h,ECX=0):EDX[bit 29] or the MSR feature bits in
     * MSR_IA32_ARCH_CAPABILITIES[bit 7], see @bugref{9630}.
     * Ignore writes to this MSR and return 0 on reads.
     *
     * Windows 11 24H2 incorrectly reads MSR_IA32_MCU_OPT_CTRL without
     * checking CPUID.ARCH_CAP(EAX=7h,ECX=0).EDX[bit 9] or the MSR feature
     * bits in MSR_IA32_ARCH_CAPABILITIES[bit 18], see @bugref{10794}.
     * Ignore wrties to this MSR and return 0 on reads.
     */
    if (pVM->cpum.s.GuestFeatures.fArchCap)
    {
        static CPUMMSRRANGE const s_aTsxCtrl[] =
        {
            MVI(MSR_IA32_TSX_CTRL, "IA32_TSX_CTRL", 0),
            MVI(MSR_IA32_MCU_OPT_CTRL, "IA32_MCU_OPT_CTRL", 0),
        };
        rc = cpumR3MsrApplyFudgeTable(pVM, &s_aTsxCtrl[0], RT_ELEMENTS(s_aTsxCtrl));
        AssertLogRelRCReturn(rc, rc);
    }

    return rc;
}


/**
 * Insert an MSR range into the VM.
 *
 * If the new MSR range overlaps existing ranges, the existing ones will be
 * adjusted/removed to fit in the new one.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pNewRange           Pointer to the MSR range being inserted.
 */
VMMR3DECL(int) CPUMR3MsrRangesInsert(PVM pVM, PCCPUMMSRRANGE pNewRange)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pNewRange, VERR_INVALID_PARAMETER);

    return cpumR3MsrRangesInsert(pVM, NULL /* ppaMsrRanges */, NULL /* pcMsrRanges */, pNewRange);
}


/**
 * Register statistics for the MSRs.
 *
 * This must not be called before the MSRs have been finalized and moved to the
 * hyper heap.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
int cpumR3MsrRegStats(PVM pVM)
{
    /*
     * Global statistics.
     */
    PCPUM pCpum = &pVM->cpum.s;
    STAM_REL_REG(pVM, &pCpum->cMsrReads,                STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/Reads",
                 STAMUNIT_OCCURENCES, "All RDMSRs making it to CPUM.");
    STAM_REL_REG(pVM, &pCpum->cMsrReadsRaiseGp,         STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/ReadsRaisingGP",
                 STAMUNIT_OCCURENCES, "RDMSR raising #GPs, except unknown MSRs.");
    STAM_REL_REG(pVM, &pCpum->cMsrReadsUnknown,         STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/ReadsUnknown",
                 STAMUNIT_OCCURENCES, "RDMSR on unknown MSRs (raises #GP).");
    STAM_REL_REG(pVM, &pCpum->cMsrWrites,               STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/Writes",
                 STAMUNIT_OCCURENCES, "All WRMSRs making it to CPUM.");
    STAM_REL_REG(pVM, &pCpum->cMsrWritesRaiseGp,        STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/WritesRaisingGP",
                 STAMUNIT_OCCURENCES, "WRMSR raising #GPs, except unknown MSRs.");
    STAM_REL_REG(pVM, &pCpum->cMsrWritesToIgnoredBits,  STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/WritesToIgnoredBits",
                 STAMUNIT_OCCURENCES, "Writing of ignored bits.");
    STAM_REL_REG(pVM, &pCpum->cMsrWritesUnknown,        STAMTYPE_COUNTER,   "/CPUM/MSR-Totals/WritesUnknown",
                 STAMUNIT_OCCURENCES, "WRMSR on unknown MSRs (raises #GP).");


#ifdef VBOX_WITH_STATISTICS
    /*
     * Per range.
     */
    PCPUMMSRRANGE   paRanges = pVM->cpum.s.GuestInfo.paMsrRangesR3;
    uint32_t        cRanges  = pVM->cpum.s.GuestInfo.cMsrRanges;
    for (uint32_t i = 0; i < cRanges; i++)
    {
        char    szName[160];
        ssize_t cchName;

        if (paRanges[i].uFirst == paRanges[i].uLast)
            cchName = RTStrPrintf(szName, sizeof(szName), "/CPUM/MSRs/%#010x-%s",
                                  paRanges[i].uFirst, paRanges[i].szName);
        else
            cchName = RTStrPrintf(szName, sizeof(szName), "/CPUM/MSRs/%#010x-%#010x-%s",
                                  paRanges[i].uFirst, paRanges[i].uLast, paRanges[i].szName);

        RTStrCopy(&szName[cchName], sizeof(szName) - cchName, "-reads");
        STAMR3Register(pVM, &paRanges[i].cReads, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, szName, STAMUNIT_OCCURENCES, "RDMSR");

        RTStrCopy(&szName[cchName], sizeof(szName) - cchName, "-writes");
        STAMR3Register(pVM, &paRanges[i].cWrites, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "WRMSR");

        RTStrCopy(&szName[cchName], sizeof(szName) - cchName, "-GPs");
        STAMR3Register(pVM, &paRanges[i].cGps, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "#GPs");

        RTStrCopy(&szName[cchName], sizeof(szName) - cchName, "-ign-bits-writes");
        STAMR3Register(pVM, &paRanges[i].cIgnoredBits, STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "WRMSR w/ ignored bits");
    }
#endif /* VBOX_WITH_STATISTICS */

    return VINF_SUCCESS;
}

