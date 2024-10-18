/* $Id$ */
/** @file
 * PMU - Performance Monitoring Unit.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_DEV_PMU
#include <VBox/log.h>
#include "PMUInternal.h"
#include <VBox/vmm/gic.h>
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


# define PMU_SYSREGRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumSysRegRdFn_Pmu, kCpumSysRegWrFn_Pmu, 0, 0, 0, 0, 0, 0, a_szName, { 0 }, { 0 }, { 0 }, { 0 } }


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * System register ranges for the PMU.
 */
static CPUMSYSREGRANGE const g_aSysRegRanges_PMU[] =
{
    PMU_SYSREGRANGE(ARMV8_AARCH64_SYSREG_PMINTENCLR_EL1, ARMV8_AARCH64_SYSREG_PMINTENCLR_EL1, "PMINTENCLR_EL1"),
    PMU_SYSREGRANGE(ARMV8_AARCH64_SYSREG_PMCR_EL0,       ARMV8_AARCH64_SYSREG_PMOVSCLR_EL0,   "PMCR_EL0 - PMOVSCLR_EL0"),
    PMU_SYSREGRANGE(ARMV8_AARCH64_SYSREG_PMCCNTR_EL0,    ARMV8_AARCH64_SYSREG_PMCCNTR_EL0,    "PMCCNTR_EL0"),
    PMU_SYSREGRANGE(ARMV8_AARCH64_SYSREG_PMUSERENR_EL0,  ARMV8_AARCH64_SYSREG_PMUSERENR_EL0,  "PMUSERENR_EL0"),
    PMU_SYSREGRANGE(ARMV8_AARCH64_SYSREG_PMCCFILTR_EL0,  ARMV8_AARCH64_SYSREG_PMCCFILTR_EL0,  "PMCCFILTR_EL0"),
};


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) pmuR3Reset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    LogFlow(("PMU: pmuR3Reset\n"));

#if 0
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU  pVCpuDest = pVM->apCpusR3[idCpu];

        @todo
    }
#endif
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
DECLCALLBACK(void) pmuR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(pDevIns, offDelta);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) pmuR3Destruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) pmuR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    Assert(iInstance == 0); NOREF(iInstance); RT_NOREF(pCfg);

    /*
     * Init the data.
     */
    //pPmu->pDevInsR3     = pDevIns;

    /*
     * Validate PMU settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "", "");

    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Initialize the PMU state.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(g_aSysRegRanges_PMU); i++)
    {
        rc = CPUMR3SysRegRangesInsert(pVM, &g_aSysRegRanges_PMU[i]);
        AssertLogRelRCReturn(rc, rc);
    }

#if 0

    /* Finally, initialize the state. */
    rc = pmuR3InitState(pVM);
    AssertRCReturn(rc, rc);

    pmuR3Reset(pDevIns);
#endif
    return VINF_SUCCESS;
}

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

