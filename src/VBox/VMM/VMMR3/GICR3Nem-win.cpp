/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GICv3) - Hyper-V interface.
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
#define LOG_GROUP LOG_GROUP_DEV_APIC
#include <iprt/nt/nt-and-windows.h>
#include <iprt/nt/hyperv.h>
#include <WinHvPlatform.h>

#include <VBox/log.h>
#include "GICInternal.h"
#include "NEMInternal.h" /* Need access to the partition handle. */
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


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * GICHv PDM instance data (per-VM).
 */
typedef struct GICHVDEV
{
    /** Pointer to the PDM device instance. */
    PPDMDEVINSR3         pDevIns;
    /** The partition handle grabbed from NEM. */
    WHV_PARTITION_HANDLE hPartition;
} GICHVDEV;
/** Pointer to a GIC KVM device. */
typedef GICHVDEV *PGICHVDEV;
/** Pointer to a const GIC KVM device. */
typedef GICHVDEV const *PCGICHVDEV;


/*
 * The following definitions appeared in build 27744 allow interacting with the GICv3 controller,
 * (there is no official SDK for this yet).
 */
/** @todo Better way of defining these which doesn't require casting later on when calling APIs. */
#define MY_WHV_ARM64_IINTERRUPT_TYPE_FIXED UINT32_C(0)

typedef union MY_WHV_INTERRUPT_CONTROL2
{
    UINT64 AsUINT64;
    struct
    {
        uint32_t InterruptType;
        UINT32 Reserved1:2;
        UINT32 Asserted:1;
        UINT32 Retarget:1;
        UINT32 Reserved2:28;
    };
} MY_WHV_INTERRUPT_CONTROL2;


typedef struct MY_WHV_INTERRUPT_CONTROL
{
    UINT64                  TargetPartition;
    MY_WHV_INTERRUPT_CONTROL2  InterruptControl;
    UINT64                  DestinationAddress;
    UINT32                  RequestedVector;
    UINT8                   TargetVtl;
    UINT8                   ReservedZ0;
    UINT16                  ReservedZ1;
} MY_WHV_INTERRUPT_CONTROL;
AssertCompileSize(MY_WHV_INTERRUPT_CONTROL, 32);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern decltype(WHvRequestInterrupt) *  g_pfnWHvRequestInterrupt;

/*
 * Let the preprocessor alias the APIs to import variables for better autocompletion.
 */
#ifndef IN_SLICKEDIT
# define WHvRequestInterrupt                        g_pfnWHvRequestInterrupt
#endif


/**
 * Common worker for GICR3KvmSpiSet() and GICR3KvmPpiSet().
 *
 * @returns VBox status code.
 * @param   pDevIns     The PDM KVM GIC device instance.
 * @param   idCpu       The CPU ID for which the interrupt is updated (only valid for PPIs).
 * @param   fPpi        Flag whether this is a PPI or SPI.
 * @param   uIntId      The interrupt ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
DECLINLINE(int) gicR3HvSetIrq(PPDMDEVINS pDevIns, VMCPUID idCpu, bool fPpi, uint32_t uIntId, bool fAsserted)
{
    LogFlowFunc(("pDevIns=%p idCpu=%u fPpi=%RTbool uIntId=%u fAsserted=%RTbool\n",
                 pDevIns, idCpu, fPpi, uIntId, fAsserted));

    PGICHVDEV pThis = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);

    MY_WHV_INTERRUPT_CONTROL IntrCtrl;
    IntrCtrl.TargetPartition                         = 0;
    IntrCtrl.InterruptControl.InterruptType          = MY_WHV_ARM64_IINTERRUPT_TYPE_FIXED;
    IntrCtrl.InterruptControl.Reserved1              = 0;
    IntrCtrl.InterruptControl.Asserted               = fAsserted ? 1 : 0;
    IntrCtrl.InterruptControl.Retarget               = 0;
    IntrCtrl.InterruptControl.Reserved2              = 0;
    IntrCtrl.DestinationAddress                      = fPpi ? RT_BIT(idCpu) : 0; /* SGI1R_EL1 */
    IntrCtrl.RequestedVector                         = fPpi ? uIntId : uIntId;
    IntrCtrl.TargetVtl                               = 0;
    IntrCtrl.ReservedZ0                              = 0;
    IntrCtrl.ReservedZ1                              = 0;
    HRESULT hrc = WHvRequestInterrupt(pThis->hPartition, (const WHV_INTERRUPT_CONTROL *)&IntrCtrl, sizeof(IntrCtrl));
    if (SUCCEEDED(hrc))
        return VINF_SUCCESS;

    AssertFailed();
    LogFlowFunc(("WHvRequestInterrupt() failed with %Rhrc (Last=%#x/%u)\n", hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_NEM_IPE_9; /** @todo */
}


/**
 * Sets the given SPI inside the in-kernel KVM GIC.
 *
 * @returns VBox status code.
 * @param   pVM         The VM instance.
 * @param   uIntId      The SPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
VMMR3_INT_DECL(int) GICR3NemSpiSet(PVMCC pVM, uint32_t uIntId, bool fAsserted)
{
    PGIC pGic = VM_TO_GIC(pVM);
    PPDMDEVINS pDevIns = pGic->CTX_SUFF(pDevIns);

    /* idCpu is ignored for SPI interrupts. */
    return gicR3HvSetIrq(pDevIns, 0 /*idCpu*/, false /*fPpi*/,
                         uIntId + GIC_INTID_RANGE_SPI_START, fAsserted);
}


/**
 * Sets the given PPI inside the in-kernel KVM GIC.
 *
 * @returns VBox status code.
 * @param   pVCpu       The vCPU for whih the PPI state is updated.
 * @param   uIntId      The PPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
VMMR3_INT_DECL(int) GICR3NemPpiSet(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    PPDMDEVINS pDevIns = VMCPU_TO_DEVINS(pVCpu);

    return gicR3HvSetIrq(pDevIns, pVCpu->idCpu, true /*fPpi*/,
                         uIntId + GIC_INTID_RANGE_PPI_START, fAsserted);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) gicR3HvReset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    RT_NOREF(pVM);

    LogFlow(("GIC: gicR3HvReset\n"));
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) gicR3HvDestruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) gicR3HvConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PGICHVDEV       pThis    = PDMDEVINS_2_DATA(pDevIns, PGICHVDEV);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    PGIC            pGic     = VM_TO_GIC(pVM);
    Assert(iInstance == 0); NOREF(iInstance);

    /*
     * Init the data.
     */
    pGic->pDevInsR3   = pDevIns;
    pGic->fNemGic     = true;
    pThis->pDevIns    = pDevIns;
    pThis->hPartition = pVM->nem.s.hPartition;

    /*
     * Validate GIC settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "DistributorMmioBase|RedistributorMmioBase|ItsMmioBase", "");

    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register the GIC with PDM.
     */
    rc = PDMDevHlpApicRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Query the MMIO ranges.
     */
    RTGCPHYS GCPhysMmioBaseDist = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "DistributorMmioBase", &GCPhysMmioBaseDist);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"DistributorMmioBase\" value"));

    RTGCPHYS GCPhysMmioBaseReDist = 0;
    rc = pHlp->pfnCFGMQueryU64(pCfg, "RedistributorMmioBase", &GCPhysMmioBaseReDist);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the \"RedistributorMmioBase\" value"));

    gicR3HvReset(pDevIns);
    return VINF_SUCCESS;
}


/**
 * GIC device registration structure.
 */
const PDMDEVREG g_DeviceGICNem =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "gic-nem",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(GICHVDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic Interrupt Controller",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           gicR3HvConstruct,
    /* .pfnDestruct = */            gicR3HvDestruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               gicR3HvReset,
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
#else
# error "Not in IN_RING3!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

