/* $Id$ */
/** @file
 * PDM - GICv3 (Generic Interrupt Controller) Interface.
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
#define LOG_GROUP LOG_GROUP_PDM_GIC
#include "PDMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvm.h>


/**
 * Gets the PDM GIC backend name.
 *
 * @returns The backend name.
 * @param   enmBackendType      The PDM GIC backend type.
 */
VMM_INT_DECL(const char *) PDMGicGetBackendName(PDMGICBACKENDTYPE enmBackendType)
{
    switch (enmBackendType)
    {
        case PDMGICBACKENDTYPE_NONE:   return "None";
        case PDMGICBACKENDTYPE_VBOX:   return "VirtualBox";
        case PDMGICBACKENDTYPE_KVM:    return "KVM";
        case PDMGICBACKENDTYPE_HYPERV: return "Hyper-V";
        case PDMGICBACKENDTYPE_HVF:    return "Hypervisor.Framework";
        default:
            break;
    }
    return "Invalid";
}


/**
 * Reads a GIC system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being read.
 * @param   pu64Value       Where to store the read value.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMGicReadSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    AssertReturn(PDMCPU_TO_GICBACKEND(pVCpu)->pfnReadSysReg, VERR_INVALID_POINTER);
    return PDMCPU_TO_GICBACKEND(pVCpu)->pfnReadSysReg(pVCpu, u32Reg, pu64Value);
}


/**
 * Writes an GIC system register.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The system register being written (IPRT system  register identifier).
 * @param   u64Value        The value to write.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMGicWriteSysReg(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    AssertReturn(PDMCPU_TO_GICBACKEND(pVCpu)->pfnWriteSysReg, VERR_INVALID_POINTER);
    return PDMCPU_TO_GICBACKEND(pVCpu)->pfnWriteSysReg(pVCpu, u32Reg, u64Value);
}


/**
 * Sets the specified shared peripheral interrupt starting.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context virtual machine structure.
 * @param   uIntId              The SPI ID (minus GIC_INTID_RANGE_SPI_START) to assert/de-assert.
 * @param   fAsserted           Flag whether to mark the interrupt as asserted/de-asserted.
 */
VMM_INT_DECL(int) PDMGicSetSpi(PVMCC pVM, uint32_t uIntId, bool fAsserted)
{
    AssertReturn(PDM_TO_GICBACKEND(pVM)->pfnSetSpi, VERR_INVALID_POINTER);
    return PDM_TO_GICBACKEND(pVM)->pfnSetSpi(pVM, uIntId, fAsserted);
}


/**
 * Sets the specified private peripheral interrupt starting.
 *
 * @returns VBox status code.
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uIntId              The PPI ID (minus GIC_INTID_RANGE_PPI_START) to assert/de-assert.
 * @param   fAsserted           Flag whether to mark the interrupt as asserted/de-asserted.
 */
VMM_INT_DECL(int) PDMGicSetPpi(PVMCPUCC pVCpu, uint32_t uIntId, bool fAsserted)
{
    AssertReturn(PDMCPU_TO_GICBACKEND(pVCpu)->pfnSetPpi, VERR_INVALID_POINTER);
    return PDMCPU_TO_GICBACKEND(pVCpu)->pfnSetPpi(pVCpu, uIntId, fAsserted);
}


/**
 * Registers a PDM GIC backend.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmBackendType  The PDM GIC backend type.
 * @param   pBackend        The PDM GIC backend.
 */
VMM_INT_DECL(int) PDMGicRegisterBackend(PVMCC pVM, PDMGICBACKENDTYPE enmBackendType, PCPDMGICBACKEND pBackend)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pVM, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pBackend, VERR_INVALID_PARAMETER);
    AssertReturn(   enmBackendType > PDMGICBACKENDTYPE_NONE
                 && enmBackendType < PDMGICBACKENDTYPE_END, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pBackend->pfnSetSpi,      VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetPpi,      VERR_INVALID_POINTER);

    /*
     * Register the backend.
     */
    pVM->pdm.s.Ic.u.armv8.enmKind = enmBackendType;
#ifdef IN_RING3
    pVM->pdm.s.Ic.u.armv8.GicBackend = *pBackend;
#else
# error "No GIC ring-0 support in this host target"
#endif

#ifdef IN_RING3
    LogRel(("PDM: %s GIC backend registered\n", PDMGicGetBackendName(enmBackendType)));
#endif
    return VINF_SUCCESS;
}

