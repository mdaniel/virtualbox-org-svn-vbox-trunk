/* $Id$ */
/** @file
 * PMU - Performance Monitoring Unit. - All Contexts.
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
#include "PMUInternal.h"
#include <VBox/vmm/pmu.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vmcpuset.h>
#ifdef IN_RING0
# include <VBox/vmm/gvmm.h>
#endif

#include <iprt/asm-arm.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/**
 * Reads a PMU system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being read.
 * @param   pu64Value       Where to store the read value.
 */
VMM_INT_DECL(VBOXSTRICTRC) PMUReadSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pu64Value);

    *pu64Value = 0;

#if 0
    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);
#endif

    RT_NOREF(pVCpu);
    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_PMCCNTR_EL0:
            *pu64Value = ASMReadTSC() * 100;
            break;
        case ARMV8_AARCH64_SYSREG_PMCR_EL0:
        case ARMV8_AARCH64_SYSREG_PMCNTENCLR_EL0:
        case ARMV8_AARCH64_SYSREG_PMUSERENR_EL0:
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    //PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);

    LogFlowFunc(("pVCpu=%p u32Reg=%#x pu64Value=%RX64\n", pVCpu, u32Reg, *pu64Value));
    return VINF_SUCCESS;
}


/**
 * Writes an PMU system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being written (IPRT system  register identifier).
 * @param   u64Value        The value to write.
 */
VMM_INT_DECL(VBOXSTRICTRC) PMUWriteSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    LogFlowFunc(("pVCpu=%p u32Reg=%#x u64Value=%RX64\n", pVCpu, u32Reg, u64Value));

#if 0
    int const  rcLock  = PDMDevHlpCritSectEnter(pDevIns, pDevIns->pCritSectRoR3, VERR_IGNORED);
    PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(pDevIns, pDevIns->pCritSectRoR3, rcLock);
#endif

    RT_NOREF(pVCpu, u64Value);
    switch (u32Reg)
    {
        case ARMV8_AARCH64_SYSREG_PMCNTENCLR_EL0:
        case ARMV8_AARCH64_SYSREG_PMOVSCLR_EL0:
        case ARMV8_AARCH64_SYSREG_PMINTENCLR_EL1:
        case ARMV8_AARCH64_SYSREG_PMCR_EL0:
        case ARMV8_AARCH64_SYSREG_PMCCFILTR_EL0:
        case ARMV8_AARCH64_SYSREG_PMCNTENSET_EL0:
        case ARMV8_AARCH64_SYSREG_PMUSERENR_EL0:
        case ARMV8_AARCH64_SYSREG_PMCCNTR_EL0:
            break;
        default:
            AssertReleaseFailed();
            break;
    }

    //PDMDevHlpCritSectLeave(pDevIns, pDevIns->pCritSectRoR3);
    return VINF_SUCCESS;
}


#ifndef IN_RING3

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) pmuRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    AssertReleaseFailed();
    return VINF_SUCCESS;
}
#endif /* !IN_RING3 */

/**
 * PMU device registration structure.
 */
const PDMDEVREG g_DevicePMU =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "pmu",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_ARCH,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(PMUDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Performance Monitoring Unit",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           pmuR3Construct,
    /* .pfnDestruct = */            pmuR3Destruct,
    /* .pfnRelocate = */            pmuR3Relocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               pmuR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           pmuRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           pmuRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};
