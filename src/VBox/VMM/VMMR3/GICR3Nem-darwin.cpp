/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GIC) - Hypervisor.framework in kernel interface.
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
#define LOG_GROUP LOG_GROUP_DEV_GIC
#include <VBox/log.h>
#include "GICInternal.h"
#include "NEMInternal.h" /* Need access to the VM file descriptor and for GIC API currently implemented in NEM. */
#include <VBox/vmm/pdmgic.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>

#include <Hypervisor/Hypervisor.h>


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * GIC Hypervisor.Framework PDM instance data (per-VM).
 */
typedef struct GICHVFDEV
{
    /** Pointer to the PDM device instance. */
    PPDMDEVINSR3        pDevIns;
} GICHVFDEV;
/** Pointer to a GIC KVM device. */
typedef GICHVFDEV *PGICHVFDEV;
/** Pointer to a const GIC KVM device. */
typedef GICHVFDEV const *PCGICHVFDEV;

typedef hv_return_t FN_HV_GIC_SET_SPI(uint32_t intid, bool level);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

extern FN_HV_GIC_SET_SPI *g_pfnHvGicSetSpi; /* Since 15.0, exported for GICR3Nem-darwin.cpp */

#ifndef IN_SLICKEDIT
# define hv_gic_set_spi                             g_pfnHvGicSetSpi
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Converts a HV return code to a VBox status code.
 *
 * @returns VBox status code.
 * @param   hrc                 The HV return code to convert.
 */
DECLINLINE(int) nemR3DarwinHvSts2Rc(hv_return_t hrc)
{
    if (hrc == HV_SUCCESS)
        return VINF_SUCCESS;

    switch (hrc)
    {
        case HV_ERROR:        return VERR_INVALID_STATE;
        case HV_BUSY:         return VERR_RESOURCE_BUSY;
        case HV_BAD_ARGUMENT: return VERR_INVALID_PARAMETER;
        case HV_NO_RESOURCES: return VERR_OUT_OF_RESOURCES;
        case HV_NO_DEVICE:    return VERR_NOT_FOUND;
        case HV_UNSUPPORTED:  return VERR_NOT_SUPPORTED;
    }

    return VERR_IPE_UNEXPECTED_STATUS;
}


/**
 * Sets the given SPI inside the in-kernel HvF GIC.
 *
 * @returns VBox status code.
 * @param   pVM         The VM instance.
 * @param   uIntId      The SPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
static DECLCALLBACK(int) gicR3HvfSetSpi(PVMCC pVM, uint32_t uIntId, bool fAsserted)
{
    RT_NOREF(pVM);
    Assert(hv_gic_set_spi);

    hv_return_t hrc = hv_gic_set_spi(uIntId + GIC_INTID_RANGE_SPI_START, fAsserted);
    return nemR3DarwinHvSts2Rc(hrc);
}


/**
 * Sets the given PPI inside the in-kernel HvF GIC.
 *
 * @returns VBox status code.
 * @param   pVCpu       The vCPU for which the PPI state is to be updated.
 * @param   uIntId      The PPI ID to update.
 * @param   fAsserted   Flag whether the interrupt is asserted (true) or not (false).
 */
static DECLCALLBACK(int) gicR3HvfSetPpi(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    RT_NOREF(pVCpu, uIntId, fAsserted);

    /* Should never be called as the PPIs are handled entirely in Hypervisor.framework/AppleHV. */
    AssertFailed();
    return VERR_NEM_IPE_9;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) gicR3HvfConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PGICHVFDEV      pThis    = PDMDEVINS_2_DATA(pDevIns, PGICHVFDEV);
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    PGIC            pGic     = VM_TO_GIC(pVM);
    Assert(iInstance == 0); NOREF(iInstance);

    RT_NOREF(pCfg);

    /*
     * Init the data.
     */
    pGic->pDevInsR3 = pDevIns;
    pThis->pDevIns  = pDevIns;

    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register the GIC with PDM.
     */
    rc = PDMDevHlpIcRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMGicRegisterBackend(pVM, PDMGICBACKENDTYPE_HVF, &g_GicHvfBackend);
    AssertLogRelRCReturn(rc, rc);

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
    /* .cbInstanceShared = */       sizeof(GICDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Generic Interrupt Controller",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           gicR3HvfConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               NULL,
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


/**
 * The Hypervisor.Framework GIC backend.
 */
const PDMGICBACKEND g_GicHvfBackend =
{
    /* .pfnReadSysReg = */  NULL,
    /* .pfnWriteSysReg = */ NULL,
    /* .pfnSetSpi = */      gicR3HvfSetSpi,
    /* .pfnSetPpi = */      gicR3HvfSetPpi,
    /* .pfnSendMsi = */     NULL,
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

