/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GIC).
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_DEV_GIC
#include <VBox/log.h>
#include "GICInternal.h"
#include <VBox/vmm/pdmgic.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>

#include <iprt/armv8.h>


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** GIC saved state version. */
#define GIC_SAVED_STATE_VERSION                     6

# define GIC_SYSREGRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumSysRegRdFn_GicIcc, kCpumSysRegWrFn_GicIcc, 0, 0, 0, 0, 0, 0, a_szName, { 0 }, { 0 }, { 0 }, { 0 } }


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * System register ranges for the GIC.
 */
static CPUMSYSREGRANGE const g_aSysRegRanges_GIC[] =
{
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_PMR_EL1,   ARMV8_AARCH64_SYSREG_ICC_PMR_EL1,     "ICC_PMR_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_IAR0_EL1,  ARMV8_AARCH64_SYSREG_ICC_AP0R3_EL1,   "ICC_IAR0_EL1 - ICC_AP0R3_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_AP1R0_EL1, ARMV8_AARCH64_SYSREG_ICC_NMIAR1_EL1,  "ICC_AP1R0_EL1 - ICC_NMIAR1_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_DIR_EL1,   ARMV8_AARCH64_SYSREG_ICC_SGI0R_EL1,   "ICC_DIR_EL1 - ICC_SGI0R_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_IAR1_EL1,  ARMV8_AARCH64_SYSREG_ICC_IGRPEN1_EL1, "ICC_IAR1_EL1 - ICC_IGRPEN1_EL1"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_SRE_EL2,   ARMV8_AARCH64_SYSREG_ICC_SRE_EL2,     "ICC_SRE_EL2"),
    GIC_SYSREGRANGE(ARMV8_AARCH64_SYSREG_ICC_SRE_EL3,   ARMV8_AARCH64_SYSREG_ICC_SRE_EL3,     "ICC_SRE_EL3")
};


/**
 * Dumps basic GIC state.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);
    PCGIC      pGic    = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);
    PCGICDEV   pGicDev = PDMDEVINS_2_DATA(pDevIns, PCGICDEV);

    pHlp->pfnPrintf(pHlp, "GIC:\n");
    pHlp->pfnPrintf(pHlp, "  uArchRev         = %u\n",      pGicDev->uArchRev);
    pHlp->pfnPrintf(pHlp, "  uMaxSpi          = %u (upto IntId %u)\n", pGicDev->uMaxSpi, 32 * (pGicDev->uMaxSpi + 1));
    pHlp->pfnPrintf(pHlp, "  fExtSpi          = %RTbool\n", pGicDev->fExtSpi);
    pHlp->pfnPrintf(pHlp, "  uMaxExtSpi       = %u (upto IntId %u)\n", pGicDev->uMaxExtSpi,
                    GIC_INTID_RANGE_EXT_SPI_START - 1 + 32 * (pGicDev->uMaxExtSpi + 1));
    pHlp->pfnPrintf(pHlp, "  fExtPpi          = %RTbool\n", pGicDev->fExtPpi);
    pHlp->pfnPrintf(pHlp, "  uMaxExtPpi       = %u (upto IntId %u)\n", pGicDev->uMaxExtPpi,
                    pGicDev->uMaxExtPpi == GIC_REDIST_REG_TYPER_PPI_NUM_MAX_1087 ? 1087 : GIC_INTID_RANGE_EXT_PPI_LAST);
    pHlp->pfnPrintf(pHlp, "  fRangeSelSupport = %RTbool\n", pGicDev->fRangeSel);
    pHlp->pfnPrintf(pHlp, "  fNmi             = %RTbool\n", pGicDev->fNmi);
    pHlp->pfnPrintf(pHlp, "  fMbi             = %RTbool\n", pGicDev->fMbi);
    pHlp->pfnPrintf(pHlp, "  fAff3Levels      = %RTbool\n", pGicDev->fAff3Levels);
}


/**
 * Dumps GIC Distributor information.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3InfoDist(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    RT_NOREF(pszArgs);

    PGIC pGic = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);
    PCGICDEV   pGicDev = PDMDEVINS_2_DATA(pDevIns, PCGICDEV);

#define GIC_DBGFINFO_DIST_INTR_BITMAP(a_Name, a_bmIntr) \
    do \
    { \
        pHlp->pfnPrintf(pHlp, "  " a_Name " =\n"); \
        for (uint32_t i = 0; i < RT_ELEMENTS(a_bmIntr); i += 8) \
            pHlp->pfnPrintf(pHlp, "    [%2u..%-2u] %#010x %#010x %#010x %#010x %#010x %#010x %#010x %#010x\n", i, i + 7, \
                            (a_bmIntr)[i],   (a_bmIntr)[i+1], (a_bmIntr)[i+2], (a_bmIntr)[i+3],  \
                            (a_bmIntr)[i+4], (a_bmIntr)[i+5], (a_bmIntr)[i+6], (a_bmIntr)[i+7]); \
    } while (0)

    pHlp->pfnPrintf(pHlp, "GIC Distributor:\n");
    pHlp->pfnPrintf(pHlp, "  fIntrGroup0Enabled = %RTbool\n", pGicDev->fIntrGroup0Enabled);
    pHlp->pfnPrintf(pHlp, "  fIntrGroup1Enabled = %RTbool\n", pGicDev->fIntrGroup1Enabled);
    pHlp->pfnPrintf(pHlp, "  fAffRoutingEnabled = %RTbool\n", pGicDev->fAffRoutingEnabled);
    GIC_DBGFINFO_DIST_INTR_BITMAP("bmIntrGroup",   pGicDev->bmIntrGroup);
    GIC_DBGFINFO_DIST_INTR_BITMAP("bmIntrEnabled", pGicDev->bmIntrEnabled);
    GIC_DBGFINFO_DIST_INTR_BITMAP("bmIntrPending", pGicDev->bmIntrPending);
    GIC_DBGFINFO_DIST_INTR_BITMAP("bmIntrActive",  pGicDev->bmIntrActive);

    /* Interrupt priorities.*/
    {
        uint32_t const cPriorities = RT_ELEMENTS(pGicDev->abIntrPriority);
        AssertCompile(!(cPriorities % 16));
        pHlp->pfnPrintf(pHlp, "  Interrupt priorities:\n");
        for (uint32_t i = 0; i < cPriorities; i += 16)
            pHlp->pfnPrintf(pHlp, "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u"
                                  "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u\n",
                                  gicDistGetIntIdFromIndex(i),     gicDistGetIntIdFromIndex(i + 7),
                                  pGicDev->abIntrPriority[i],      pGicDev->abIntrPriority[i + 1],
                                  pGicDev->abIntrPriority[i + 2],  pGicDev->abIntrPriority[i + 3],
                                  pGicDev->abIntrPriority[i + 4],  pGicDev->abIntrPriority[i + 5],
                                  pGicDev->abIntrPriority[i + 6],  pGicDev->abIntrPriority[i + 7],
                                  gicDistGetIntIdFromIndex(i + 8), gicDistGetIntIdFromIndex(i + 15),
                                  pGicDev->abIntrPriority[i + 8],  pGicDev->abIntrPriority[i + 9],
                                  pGicDev->abIntrPriority[i + 10], pGicDev->abIntrPriority[i + 11],
                                  pGicDev->abIntrPriority[i + 12], pGicDev->abIntrPriority[i + 13],
                                  pGicDev->abIntrPriority[i + 14], pGicDev->abIntrPriority[i + 15]);
    }

    /* Interrupt routing.*/
    {
        /** @todo Interrupt rounting mode. */
        uint32_t const cRouting = RT_ELEMENTS(pGicDev->au32IntrRouting);
        AssertCompile(!(cRouting % 16));
        pHlp->pfnPrintf(pHlp, "  Interrupt routing:\n");
        for (uint32_t i = 0; i < cRouting; i += 16)
            pHlp->pfnPrintf(pHlp, "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u"
                                  "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u\n",
                                  gicDistGetIntIdFromIndex(i),      gicDistGetIntIdFromIndex(i + 7),
                                  pGicDev->au32IntrRouting[i],      pGicDev->au32IntrRouting[i + 1],
                                  pGicDev->au32IntrRouting[i + 2],  pGicDev->au32IntrRouting[i + 3],
                                  pGicDev->au32IntrRouting[i + 4],  pGicDev->au32IntrRouting[i + 5],
                                  pGicDev->au32IntrRouting[i + 6],  pGicDev->au32IntrRouting[i + 7],
                                  gicDistGetIntIdFromIndex(i + 8),  gicDistGetIntIdFromIndex(i + 15),
                                  pGicDev->au32IntrRouting[i + 8],  pGicDev->au32IntrRouting[i + 9],
                                  pGicDev->au32IntrRouting[i + 10], pGicDev->au32IntrRouting[i + 11],
                                  pGicDev->au32IntrRouting[i + 12], pGicDev->au32IntrRouting[i + 13],
                                  pGicDev->au32IntrRouting[i + 14], pGicDev->au32IntrRouting[i + 15]);
    }

#undef GIC_DBGFINFO_DIST_INTR_BITMAP
}


/**
 * Dumps the GIC Redistributor information.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helpers.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) gicR3InfoReDist(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = pVM->apCpusR3[0];

    PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);

    pHlp->pfnPrintf(pHlp, "VCPU[%u] Redistributor:\n", pVCpu->idCpu);
    AssertCompile(RT_ELEMENTS(pGicCpu->bmIntrGroup)   >= 3);
    AssertCompile(RT_ELEMENTS(pGicCpu->bmIntrEnabled) >= 3);
    AssertCompile(RT_ELEMENTS(pGicCpu->bmIntrPending) >= 3);
    AssertCompile(RT_ELEMENTS(pGicCpu->bmIntrActive)  >= 3);
    pHlp->pfnPrintf(pHlp, "  bmIntrGroup[0..2]   = %#010x %#010x %#010x\n", pGicCpu->bmIntrGroup[0],   pGicCpu->bmIntrGroup[1],   pGicCpu->bmIntrGroup[2]);
    pHlp->pfnPrintf(pHlp, "  bmIntrEnabled[0..2] = %#010x %#010x %#010x\n", pGicCpu->bmIntrEnabled[0], pGicCpu->bmIntrEnabled[1], pGicCpu->bmIntrEnabled[2]);
    pHlp->pfnPrintf(pHlp, "  bmIntrPending[0..2] = %#010x %#010x %#010x\n", pGicCpu->bmIntrPending[0], pGicCpu->bmIntrPending[1], pGicCpu->bmIntrPending[2]);
    pHlp->pfnPrintf(pHlp, "  bmIntrActive[0..2]  = %#010x %#010x %#010x\n", pGicCpu->bmIntrActive[0],  pGicCpu->bmIntrActive[1],  pGicCpu->bmIntrActive[2]);

    /* Interrupt priorities. */
    {
        uint32_t const cPriorities = RT_ELEMENTS(pGicCpu->abIntrPriority);
        AssertCompile(!(cPriorities % 16));
        pHlp->pfnPrintf(pHlp, "  Interrupt priorities:\n");
        for (uint32_t i = 0; i < cPriorities; i += 16)
            pHlp->pfnPrintf(pHlp, "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u"
                                  "    IntId[%4u..%-4u] = %3u %3u %3u %3u %3u %3u %3u %3u\n",
                                  gicReDistGetIntIdFromIndex(i),     gicReDistGetIntIdFromIndex(i + 7),
                                  pGicCpu->abIntrPriority[i],        pGicCpu->abIntrPriority[i + 1],
                                  pGicCpu->abIntrPriority[i + 2],    pGicCpu->abIntrPriority[i + 3],
                                  pGicCpu->abIntrPriority[i + 4],    pGicCpu->abIntrPriority[i + 5],
                                  pGicCpu->abIntrPriority[i + 6],    pGicCpu->abIntrPriority[i + 7],
                                  gicReDistGetIntIdFromIndex(i + 8), gicReDistGetIntIdFromIndex(i + 15),
                                  pGicCpu->abIntrPriority[i + 8],    pGicCpu->abIntrPriority[i + 9],
                                  pGicCpu->abIntrPriority[i + 10],   pGicCpu->abIntrPriority[i + 11],
                                  pGicCpu->abIntrPriority[i + 12],   pGicCpu->abIntrPriority[i + 13],
                                  pGicCpu->abIntrPriority[i + 14],   pGicCpu->abIntrPriority[i + 15]);
    }

    pHlp->pfnPrintf(pHlp, "\nVCPU[%u] ICC system register state:\n", pVCpu->idCpu);
    pHlp->pfnPrintf(pHlp, "  uIccCtlr            = %#RX64\n",  pGicCpu->uIccCtlr);
    pHlp->pfnPrintf(pHlp, "  fIntrGroup0Enabled  = %RTbool\n", pGicCpu->fIntrGroup0Enabled);
    pHlp->pfnPrintf(pHlp, "  fIntrGroup1Enabled  = %RTbool\n", pGicCpu->fIntrGroup1Enabled);
    pHlp->pfnPrintf(pHlp, "  bBinaryPtGroup0     = %#x\n",     pGicCpu->bBinaryPtGroup0);
    pHlp->pfnPrintf(pHlp, "  bBinaryPtGroup1     = %#x\n",     pGicCpu->bBinaryPtGroup1);
    pHlp->pfnPrintf(pHlp, "  idxRunningPriority  = %#x\n",     pGicCpu->idxRunningPriority);
    pHlp->pfnPrintf(pHlp, "  Running priority    = %#x\n",     pGicCpu->abRunningPriorities[pGicCpu->idxRunningPriority]);

    /* Running interrupt priorities. */
    {
        uint32_t const cPriorities = RT_ELEMENTS(pGicCpu->abRunningPriorities);
        AssertCompile(!(cPriorities % 16));
        pHlp->pfnPrintf(pHlp, "  Running-interrupt priorities:\n");
        for (uint32_t i = 0; i < cPriorities; i += 16)
            pHlp->pfnPrintf(pHlp, "    [%3u..%-3u] = %3u %3u %3u %3u %3u %3u %3u %3u"
                                  "    [%3u..%-3u] = %3u %3u %3u %3u %3u %3u %3u %3u\n",
                                       i,                                    i + 7,
                                       pGicCpu->abRunningPriorities[i],      pGicCpu->abRunningPriorities[i + 1],
                                       pGicCpu->abRunningPriorities[i + 2],  pGicCpu->abRunningPriorities[i + 3],
                                       pGicCpu->abRunningPriorities[i + 4],  pGicCpu->abRunningPriorities[i + 5],
                                       pGicCpu->abRunningPriorities[i + 6],  pGicCpu->abRunningPriorities[i + 7],
                                       i + 8,                                i + 15,
                                       pGicCpu->abRunningPriorities[i + 8],  pGicCpu->abRunningPriorities[i + 9],
                                       pGicCpu->abRunningPriorities[i + 10], pGicCpu->abRunningPriorities[i + 11],
                                       pGicCpu->abRunningPriorities[i + 12], pGicCpu->abRunningPriorities[i + 13],
                                       pGicCpu->abRunningPriorities[i + 14], pGicCpu->abRunningPriorities[i + 15]);
    }

    AssertCompile(RT_ELEMENTS(pGicCpu->bmActivePriorityGroup0) >= 4);
    pHlp->pfnPrintf(pHlp, "  Active-interrupt priorities Group 0:\n");
    pHlp->pfnPrintf(pHlp, "    [0..3] = %#010x %#010x %#010x %#010x\n",
                               pGicCpu->bmActivePriorityGroup0[0], pGicCpu->bmActivePriorityGroup0[1],
                               pGicCpu->bmActivePriorityGroup0[2], pGicCpu->bmActivePriorityGroup0[3]);
    AssertCompile(RT_ELEMENTS(pGicCpu->bmActivePriorityGroup1) >= 4);
    pHlp->pfnPrintf(pHlp, "  Active-interrupt priorities Group 1:\n");
    pHlp->pfnPrintf(pHlp, "    [0..3] = %#010x %#010x %#010x %#010x\n",
                               pGicCpu->bmActivePriorityGroup1[0], pGicCpu->bmActivePriorityGroup1[1],
                               pGicCpu->bmActivePriorityGroup1[2], pGicCpu->bmActivePriorityGroup1[3]);
}


/**
 * @copydoc FNSSMDEVSAVEEXEC
 */
static DECLCALLBACK(int) gicR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PCVM          pVM     = PDMDevHlpGetVM(pDevIns);
    PCPDMDEVHLPR3 pHlp    = pDevIns->pHlpR3;
    PCGICDEV      pGicDev = PDMDEVINS_2_DATA(pDevIns, PCGICDEV);
    AssertPtrReturn(pVM, VERR_INVALID_VM_HANDLE);
    LogFlowFunc(("\n"));

    /*
     * Save per-VM data.
     */
    pHlp->pfnSSMPutU32(pSSM,  pVM->cCpus);
    pHlp->pfnSSMPutU8(pSSM,   pGicDev->uArchRev);
    pHlp->pfnSSMPutU8(pSSM,   pGicDev->uMaxSpi);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fExtSpi);
    pHlp->pfnSSMPutU8(pSSM,   pGicDev->uMaxExtSpi);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fExtPpi);
    pHlp->pfnSSMPutU8(pSSM,   pGicDev->uMaxExtPpi);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fRangeSel);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fNmi);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fMbi);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fAff3Levels);

    /* Distributor state. */
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fIntrGroup0Enabled);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fIntrGroup1Enabled);
    pHlp->pfnSSMPutBool(pSSM, pGicDev->fAffRoutingEnabled);
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrGroup[0],       sizeof(pGicDev->bmIntrGroup));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrConfig[0],      sizeof(pGicDev->bmIntrConfig));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrEnabled[0],     sizeof(pGicDev->bmIntrEnabled));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrPending[0],     sizeof(pGicDev->bmIntrPending));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrActive[0],      sizeof(pGicDev->bmIntrActive));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->abIntrPriority[0],    sizeof(pGicDev->abIntrPriority));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->au32IntrRouting[0],   sizeof(pGicDev->au32IntrRouting));
    pHlp->pfnSSMPutMem(pSSM,  &pGicDev->bmIntrRoutingMode[0], sizeof(pGicDev->bmIntrRoutingMode));

    /*
     * Save per-VCPU data.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PCGICCPU pGicCpu = VMCPU_TO_GICCPU(pVM->apCpusR3[idCpu]);
        Assert(pGicCpu);

        /* Redistributor state. */
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmIntrGroup[0],    sizeof(pGicCpu->bmIntrGroup));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmIntrConfig[0],   sizeof(pGicCpu->bmIntrConfig));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmIntrEnabled[0],  sizeof(pGicCpu->bmIntrEnabled));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmIntrPending[0],  sizeof(pGicCpu->bmIntrPending));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmIntrActive[0],   sizeof(pGicCpu->bmIntrActive));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->abIntrPriority[0], sizeof(pGicCpu->abIntrPriority));

        /* ICC system register state. */
        pHlp->pfnSSMPutU64(pSSM,  pGicCpu->uIccCtlr);
        pHlp->pfnSSMPutU8(pSSM,   pGicCpu->bIntrPriorityMask);
        pHlp->pfnSSMPutU8(pSSM,   pGicCpu->idxRunningPriority);
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->abRunningPriorities[0],    sizeof(pGicCpu->abRunningPriorities));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmActivePriorityGroup0[0], sizeof(pGicCpu->bmActivePriorityGroup0));
        pHlp->pfnSSMPutMem(pSSM, &pGicCpu->bmActivePriorityGroup1[0], sizeof(pGicCpu->bmActivePriorityGroup1));
        pHlp->pfnSSMPutU8(pSSM,   pGicCpu->bBinaryPtGroup0);
        pHlp->pfnSSMPutU8(pSSM,   pGicCpu->bBinaryPtGroup1);
        pHlp->pfnSSMPutBool(pSSM, pGicCpu->fIntrGroup0Enabled);
        pHlp->pfnSSMPutBool(pSSM, pGicCpu->fIntrGroup1Enabled);
    }

    return pHlp->pfnSSMPutU32(pSSM, UINT32_MAX);
}


/**
 * @copydoc FNSSMDEVLOADEXEC
 */
static DECLCALLBACK(int) gicR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PVM           pVM  = PDMDevHlpGetVM(pDevIns);
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;

    AssertPtrReturn(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(uPass == SSM_PASS_FINAL, VERR_WRONG_ORDER);
    LogFlowFunc(("uVersion=%u uPass=%#x\n", uVersion, uPass));

    /*
     * Validate supported saved-state versions.
     */
    if (uVersion != GIC_SAVED_STATE_VERSION)
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Invalid saved-state version %u"), uVersion);

    /*
     * Load per-VM data.
     */
    uint32_t cCpus;
    pHlp->pfnSSMGetU32(pSSM,  &cCpus);
    if (cCpus != pVM->cCpus)
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch: cCpus: got=%u expected=%u"), cCpus, pVM->cCpus);

    PGICDEV pGicDev = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    pHlp->pfnSSMGetU8(pSSM,   &pGicDev->uArchRev);
    pHlp->pfnSSMGetU8(pSSM,   &pGicDev->uMaxSpi);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fExtSpi);
    pHlp->pfnSSMGetU8(pSSM,   &pGicDev->uMaxExtSpi);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fExtPpi);
    pHlp->pfnSSMGetU8(pSSM,   &pGicDev->uMaxExtPpi);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fRangeSel);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fNmi);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fMbi);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fAff3Levels);

    /* Distributor state. */
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fIntrGroup0Enabled);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fIntrGroup1Enabled);
    pHlp->pfnSSMGetBool(pSSM, &pGicDev->fAffRoutingEnabled);
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrGroup[0],       sizeof(pGicDev->bmIntrGroup));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrConfig[0],      sizeof(pGicDev->bmIntrConfig));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrEnabled[0],     sizeof(pGicDev->bmIntrEnabled));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrPending[0],     sizeof(pGicDev->bmIntrPending));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrActive[0],      sizeof(pGicDev->bmIntrActive));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->abIntrPriority[0],    sizeof(pGicDev->abIntrPriority));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->au32IntrRouting[0],   sizeof(pGicDev->au32IntrRouting));
    pHlp->pfnSSMGetMem(pSSM,  &pGicDev->bmIntrRoutingMode[0], sizeof(pGicDev->bmIntrRoutingMode));

    /*
     * Load per-VCPU data.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVM->apCpusR3[idCpu]);
        Assert(pGicCpu);

        /* Redistributor state. */
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->bmIntrGroup[0],    sizeof(pGicCpu->bmIntrGroup));
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->bmIntrConfig[0],   sizeof(pGicCpu->bmIntrConfig));
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->bmIntrEnabled[0],  sizeof(pGicCpu->bmIntrEnabled));
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->bmIntrPending[0],  sizeof(pGicCpu->bmIntrPending));
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->bmIntrActive[0],   sizeof(pGicCpu->bmIntrActive));
        pHlp->pfnSSMGetMem(pSSM, &pGicCpu->abIntrPriority[0], sizeof(pGicCpu->abIntrPriority));

        /* ICC system register state. */
        pHlp->pfnSSMGetU64(pSSM,  &pGicCpu->uIccCtlr);
        pHlp->pfnSSMGetU8(pSSM,   &pGicCpu->bIntrPriorityMask);
        pHlp->pfnSSMGetU8(pSSM,   &pGicCpu->idxRunningPriority);
        pHlp->pfnSSMGetMem(pSSM,  &pGicCpu->abRunningPriorities[0],    sizeof(pGicCpu->abRunningPriorities));
        pHlp->pfnSSMGetMem(pSSM,  &pGicCpu->bmActivePriorityGroup0[0], sizeof(pGicCpu->bmActivePriorityGroup0));
        pHlp->pfnSSMGetMem(pSSM,  &pGicCpu->bmActivePriorityGroup1[0], sizeof(pGicCpu->bmActivePriorityGroup1));
        pHlp->pfnSSMGetU8(pSSM,   &pGicCpu->bBinaryPtGroup0);
        pHlp->pfnSSMGetU8(pSSM,   &pGicCpu->bBinaryPtGroup1);
        pHlp->pfnSSMGetBool(pSSM, &pGicCpu->fIntrGroup0Enabled);
        pHlp->pfnSSMGetBool(pSSM, &pGicCpu->fIntrGroup1Enabled);
    }

    /*
     * Check that we're still good wrt restored data.
     */
    int rc = pHlp->pfnSSMHandleGetStatus(pSSM);
    AssertRCReturn(rc, rc);

    uint32_t uMarker = 0;
    rc = pHlp->pfnSSMGetU32(pSSM, &uMarker);
    AssertRCReturn(rc, rc);
    if (uMarker == UINT32_MAX)
    { /* likely */ }
    else
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch: Marker: got=%u expected=%u"), uMarker, UINT32_MAX);

    /*
     * Finally, perform sanity checks.
     */
    if (pGicDev->uArchRev <= GIC_DIST_REG_PIDR2_ARCH_REV_GICV4)
    { /* likely */ }
    else
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Invalid uArchRev, got %u expected range [1,31]"), pGicDev->uArchRev,
                                       GIC_DIST_REG_PIDR2_ARCH_REV_GICV1, GIC_DIST_REG_PIDR2_ARCH_REV_GICV4);
    if (pGicDev->uMaxSpi - 1 < 31)
    { /* likely */ }
    else
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Invalid MaxSpi, got %u expected range [1,31]"), pGicDev->uMaxSpi);
    if (pGicDev->uMaxExtSpi <= 31)
    { /* likely */ }
    else
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Invalid MaxExtSpi, got %u expected range [0,31]"), pGicDev->uMaxExtSpi);
    if (   pGicDev->uMaxExtPpi == GIC_REDIST_REG_TYPER_PPI_NUM_MAX_1087
        || pGicDev->uMaxExtPpi == GIC_REDIST_REG_TYPER_PPI_NUM_MAX_1119)
    { /* likely */ }
    else
        return pHlp->pfnSSMSetCfgError(pSSM, RT_SRC_POS, N_("Invalid MaxExtPpi, got %u expected range [1,2]"), pGicDev->uMaxExtPpi);
    return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) gicR3Reset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    LogFlow(("GIC: gicR3Reset\n"));

    gicReset(pDevIns);
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpuDest = pVM->apCpusR3[idCpu];
        gicResetCpu(pDevIns, pVCpuDest);
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
DECLCALLBACK(void) gicR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(pDevIns, offDelta);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) gicR3Destruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) gicR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PGICDEV         pGicDev  = PDMDEVINS_2_DATA(pDevIns, PGICDEV);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    PGIC            pGic     = VM_TO_GIC(pVM);
    Assert(iInstance == 0); NOREF(iInstance);

    /*
     * Init the data.
     */
    pGic->pDevInsR3 = pDevIns;

    /*
     * Validate GIC settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DistributorMmioBase|RedistributorMmioBase|ItsMmioBase"
                                           "|ArchRev"
                                           "|MaxSpi"
                                           "|ExtSpi"
                                           "|MaxExtSpi"
                                           "|ExtPpi"
                                           "|MaxExtPpi"
                                           "|RangeSel"
                                           "|Nmi"
                                           "|Mbi"
                                           "|Aff3Levels", "");

#if 0
    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);
#endif

    /** @devcfgm{gic, ArchRev, uint8_t, 3}
     * Configures the GIC architecture revision (GICD_PIDR2.ArchRev and
     * GICR_PIDR2.ArchRev).
     *
     * Currently we only support GICv3. */
    int rc = pHlp->pfnCFGMQueryU8Def(pCfg, "ArchRev", &pGicDev->uArchRev, 3);
    AssertLogRelRCReturn(rc, rc);
    if (pGicDev->uArchRev == 3)
    { /* likely */ }
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Configuration error: \"ArchRev\" value %u is not supported"), pGicDev->uArchRev);

    /** @devcfgm{gic, MaxSpi, uint8_t, 31}
     * Configures GICD_TYPER.ItLinesNumber.
     *
     * For the IntId range [32,1023], configures the maximum SPI supported. Valid values
     * are [1,31] which equates to interrupt IDs [63,1023]. A value of 0 implies SPIs
     * are not supported. We don't allow configuring this value as it's expected that
     * most guests would assume support for SPIs. */
    AssertCompile(GIC_DIST_REG_TYPER_NUM_ITLINES == 31);
    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "MaxSpi", &pGicDev->uMaxSpi, 31 /* Upto and incl. IntId 1023 */);
    AssertLogRelRCReturn(rc, rc);
    if (pGicDev->uMaxSpi - 1 < 31)
    { /* likely */ }
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Configuration error: \"MaxSpi\" must be in the range [1,%u]"),
                                   GIC_DIST_REG_TYPER_NUM_ITLINES);

    /** @devcfgm{gic, ExtSpi, bool, false}
     * Configures whether extended SPIs supported is enabled (GICD_TYPER.ESPI). */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "ExtSpi", &pGicDev->fExtSpi, false);
    AssertLogRelRCReturn(rc, rc);

    /** @devcfgm{gic, MaxExtSpi, uint8_t, 31}
     * Configures GICD_TYPER.ESPI_range.
     *
     * For the extended SPI range [4096,5119], configures the maximum extended SPI
     * supported. Valid values are [0,31] which equates to extended SPI IntIds
     * [4127,5119]. This is ignored (set to 0 in the register) when extended SPIs are
     * disabled. */
    AssertCompile(GIC_DIST_REG_TYPER_ESPI_RANGE >> GIC_DIST_REG_TYPER_ESPI_RANGE_BIT == 31);
    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "MaxExtSpi", &pGicDev->uMaxExtSpi, 31);
    AssertLogRelRCReturn(rc, rc);
    if (pGicDev->uMaxExtSpi <= 31)
    { /* likely */ }
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Configuration error: \"MaxExtSpi\" must be in the range [0,31]"));

    /** @devcfgm{gic, ExtPpi, bool, true}
     * Configures whether extended PPIs support is enabled. */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "ExtPpi", &pGicDev->fExtPpi, true);
    AssertLogRelRCReturn(rc, rc);

    /** @devcfgm{gic, MaxExtPpi, uint8_t, 2}
     * Configures GICR_TYPER.PPInum.
     *
     * For the extended PPI range [1056,5119], configures the maximum extended PPI
     * supported. Valid values are [1,2] which equates to extended PPI IntIds
     * [1087,1119]. This is unused when extended PPIs are disabled. */
    rc = pHlp->pfnCFGMQueryU8Def(pCfg, "MaxExtPpi", &pGicDev->uMaxExtPpi, 2);
    AssertLogRelRCReturn(rc, rc);
    if (   pGicDev->uMaxExtPpi == GIC_REDIST_REG_TYPER_PPI_NUM_MAX_1087
        || pGicDev->uMaxExtPpi == GIC_REDIST_REG_TYPER_PPI_NUM_MAX_1119)
    { /* likely */ }
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("Configuration error: \"MaxExtPpi\" must be in the range [0,2]"));

    /** @devcfgm{gic, RangeSelSupport, bool, true}
     * Configures whether range-selector support is enabled (GICD_TYPER.RSS and
     * ICC_CTLR_EL1.RSS). */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "RangeSel", &pGicDev->fRangeSel, true);
    AssertLogRelRCReturn(rc, rc);

    /** @devcfgm{gic, Nmi, bool, false}
     * Configures whether non-maskable interrupts (NMIs) are supported
     * (GICD_TYPER.NMI). */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "Nmi", &pGicDev->fNmi, false);
    AssertLogRelRCReturn(rc, rc);

    /** @devcfgm{gic, Mbi, bool, true}
     * Configures whether message-based interrupts (MBIs) are supported
     * (GICD_TYPER.MBIS). */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "Mbi", &pGicDev->fMbi, true);
    AssertLogRelRCReturn(rc, rc);

    /** @devcfgm{gic, Aff3Levels, bool, true}
     * Configures whether non-zero affinity 3 levels (A3V) are supported
     * (GICD_TYPER.A3V) and (ICC_CTLR.A3V). */
    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "Aff3Levels", &pGicDev->fAff3Levels, true);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register the GIC with PDM.
     */
    rc = PDMDevHlpIcRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMGicRegisterBackend(pVM, PDMGICBACKENDTYPE_VBOX, &g_GicBackend);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Insert the GIC system registers.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aSysRegRanges_GIC); i++)
    {
        rc = CPUMR3SysRegRangesInsert(pVM, &g_aSysRegRanges_GIC[i]);
        AssertLogRelRCReturn(rc, rc);
    }

    /*
     * Register the MMIO ranges.
     */
    RTGCPHYS GCPhysMmioBase = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "DistributorMmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DistributorMmioBase\" value"));

    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, GIC_DIST_REG_FRAME_SIZE, gicDistMmioWrite, gicDistMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "GIC_Dist", &pGicDev->hMmioDist);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnCFGMQueryU64(pCfg, "RedistributorMmioBase", &GCPhysMmioBase);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"RedistributorMmioBase\" value"));

    RTGCPHYS const cbRegion = (RTGCPHYS)pVM->cCpus
                            * (GIC_REDIST_REG_FRAME_SIZE + GIC_REDIST_SGI_PPI_REG_FRAME_SIZE); /* Adjacent and per vCPU. */
    rc = PDMDevHlpMmioCreateAndMap(pDevIns, GCPhysMmioBase, cbRegion, gicReDistMmioWrite, gicReDistMmioRead,
                                   IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_DWORD_ZEROED, "GIC_ReDist", &pGicDev->hMmioReDist);
    AssertRCReturn(rc, rc);

    /*
     * Register saved state callbacks.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, GIC_SAVED_STATE_VERSION, 0, gicR3SaveExec, gicR3LoadExec);
    AssertRCReturn(rc, rc);

    /*
     * Register debugger info callbacks.
     *
     * We use separate callbacks rather than arguments so they can also be
     * dumped in an automated fashion while collecting crash diagnostics and
     * not just used during live debugging via the VM debugger.
     */
    DBGFR3InfoRegisterInternalEx(pVM, "gic",       "Dumps GIC basic information.",         gicR3Info,       DBGFINFO_FLAGS_ALL_EMTS);
    DBGFR3InfoRegisterInternalEx(pVM, "gicdist",   "Dumps GIC distributor information.",   gicR3InfoDist,   DBGFINFO_FLAGS_ALL_EMTS);
    DBGFR3InfoRegisterInternalEx(pVM, "gicredist", "Dumps GIC redistributor information.", gicR3InfoReDist, DBGFINFO_FLAGS_ALL_EMTS);

    /*
     * Statistics.
     */
#ifdef VBOX_WITH_STATISTICS
# define GIC_REG_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
         PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, \
                                a_pszDesc, a_pszNameFmt, idCpu)
# define GIC_PROF_COUNTER(a_pvReg, a_pszNameFmt, a_pszDesc) \
         PDMDevHlpSTAMRegisterF(pDevIns, a_pvReg, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, \
                                a_pszDesc, a_pszNameFmt, idCpu)

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU  pVCpu   = pVM->apCpusR3[idCpu];
        PGICCPU pGicCpu = VMCPU_TO_GICCPU(pVCpu);

        GIC_REG_COUNTER(&pGicCpu->StatMmioReadR3,    "%u/MmioRead",     "Number of MMIO reads in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatMmioWriteR3,   "%u/MmioWrite",    "Number of MMIO writes in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSysRegReadR3,  "%u/SysRegRead",   "Number of system register reads in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSysRegWriteR3, "%u/SysRegWrite",  "Number of system register writes in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSetSpiR3,      "%u/SetSpi",       "Number of set SPI callbacks in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSetPpiR3,      "%u/SetPpi",       "Number of set PPI callbacks in R3.");
        GIC_REG_COUNTER(&pGicCpu->StatSetSgiR3,      "%u/SetSgi",       "Number of SGIs generated in R3.");

        GIC_PROF_COUNTER(&pGicCpu->StatProfIntrAckR3, "%u/Prof/IntrAck", "Profiling of interrupt acknowledge (IAR) in R3.");
        GIC_PROF_COUNTER(&pGicCpu->StatProfSetSpiR3,  "%u/Prof/SetSpi",  "Profiling of set SPI callback in R3.");
        GIC_PROF_COUNTER(&pGicCpu->StatProfSetPpiR3,  "%u/Prof/SetPpi",  "Profiling of set PPI callback in R3.");
        GIC_PROF_COUNTER(&pGicCpu->StatProfSetSgiR3,  "%u/Prof/SetSgi",  "Profiling of SGIs generated in R3.");
    }
# undef GIC_PROF_COUNTER
#endif

    gicR3Reset(pDevIns);
    return VINF_SUCCESS;
}

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

