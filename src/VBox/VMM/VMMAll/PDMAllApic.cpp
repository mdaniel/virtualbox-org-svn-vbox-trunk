/* $Id$ */
/** @file
 * PDM - APIC (Advanced Programmable Interrupt Controller) Interface.
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
#define LOG_GROUP LOG_GROUP_PDM_APIC
#include "PDMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/gvm.h>


/**
 * Gets the PDM APIC backend name.
 *
 * @returns The backend name.
 * @param   enmBackendType      The PDM APIC backend type.
 */
VMM_INT_DECL(const char *) PDMApicGetBackendName(PDMAPICBACKENDTYPE enmBackendType)
{
    switch (enmBackendType)
    {
        case PDMAPICBACKENDTYPE_NONE:   return "None";
        case PDMAPICBACKENDTYPE_VBOX:   return "VirtualBox";
        case PDMAPICBACKENDTYPE_KVM:    return "KVM";
        case PDMAPICBACKENDTYPE_HYPERV: return "Hyper-V";
        case PDMAPICBACKENDTYPE_HVF:    return "Hypervisor.Framework";
        default:
            break;
    }
    return "Invalid";
}


/**
 * Updates pending interrupts from the pending-interrupt bitmaps to the IRR.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 *
 * @note    NEM/win is ASSUMING the an up to date TPR is not required here.
 */
VMM_INT_DECL(void) PDMApicUpdatePendingInterrupts(PVMCPUCC pVCpu)
{
    AssertReturnVoid(PDMCPU_TO_APICBACKEND(pVCpu)->pfnUpdatePendingInterrupts);
    PDMCPU_TO_APICBACKEND(pVCpu)->pfnUpdatePendingInterrupts(pVCpu);
}


/**
 * Gets the APIC TPR (Task Priority Register).
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pu8Tpr          Where to store the TPR.
 * @param   pfPending       Where to store whether there is a pending interrupt
 *                          (optional, can be NULL).
 * @param   pu8PendingIntr  Where to store the highest-priority pending
 *                          interrupt (optional, can be NULL).
 */
VMM_INT_DECL(int) PDMApicGetTpr(PCVMCPUCC pVCpu, uint8_t *pu8Tpr, bool *pfPending, uint8_t *pu8PendingIntr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetTpr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetTpr(pVCpu, pu8Tpr, pfPending, pu8PendingIntr);
}


/**
 * Sets the TPR (Task Priority Register).
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_CPUM_RAISE_GP_0
 * @retval  VERR_PDM_NO_APIC_INSTANCE
 * @retval  VERR_INVALID_POINTER
 *
 * @param   pVCpu                   The cross context virtual CPU structure.
 * @param   u8Tpr                   The TPR value to set.
 * @param   fForceX2ApicBehaviour   Pretend the APIC is in x2APIC mode during
 *                                  this write.
 */
VMM_INT_DECL(int) PDMApicSetTpr(PVMCPUCC pVCpu, uint8_t u8Tpr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetTpr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetTpr(pVCpu, u8Tpr, false /* fForceX2ApicBehaviour */);
}


/**
 * Returns whether the APIC hardware enabled or not.
 *
 * @returns @c true if enabled, @c false otherwise.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
VMM_INT_DECL(bool) PDMApicIsEnabled(PCVMCPUCC pVCpu)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnIsEnabled, false);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnIsEnabled(pVCpu);
}


/**
 * Reads an APIC MSR.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The MSR being read.
 * @param   pu64Value       Where to store the read value.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicReadMsr(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnReadMsr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnReadMsr(pVCpu, u32Reg, pu64Value);
}


/**
 * Writes an APIC MSR.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u32Reg          The MSR being written.
 * @param   u64Value        The value to write.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicWriteMsr(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnWriteMsr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnWriteMsr(pVCpu, u32Reg, u64Value);
}


/**
 * Gets the APIC timer frequency.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pu64Value       Where to store the timer frequency.
 */
VMM_INT_DECL(int) PDMApicGetTimerFreq(PVMCC pVM, uint64_t *pu64Value)
{
    AssertReturn(PDM_TO_APICBACKEND(pVM)->pfnGetTimerFreq, VERR_INVALID_POINTER);
    return PDM_TO_APICBACKEND(pVM)->pfnGetTimerFreq(pVM, pu64Value);
}


/**
 * Assert/de-assert the local APIC's LINT0/LINT1 interrupt pins.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u8Pin       The interrupt pin (0 for LINT0 or 1 for LINT1).
 * @param   u8Level     The level (0 for low or 1 for high).
 * @param   rcRZ        The return code if the operation cannot be performed in
 *                      the current context.
 *
 * @note    All callers totally ignores the status code!
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicSetLocalInterrupt(PVMCPUCC pVCpu, uint8_t u8Pin, uint8_t u8Level, int rcRZ)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetLocalInterrupt, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetLocalInterrupt(pVCpu, u8Pin, u8Level, rcRZ);
}


/**
 * Gets the APIC base MSR (no checks are performed wrt APIC hardware or its
 * state).
 *
 * @returns The base MSR value.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(uint64_t) PDMApicGetBaseMsrNoCheck(PCVMCPUCC pVCpu)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetBaseMsrNoCheck, 0);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetBaseMsrNoCheck(pVCpu);
}


/**
 * Gets the APIC base MSR.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pu64Value   Where to store the MSR value.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicGetBaseMsr(PVMCPUCC pVCpu, uint64_t *pu64Value)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetBaseMsr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetBaseMsr(pVCpu, pu64Value);
}


/**
 * Sets the APIC base MSR.
 *
 * @returns VBox status code - no informational ones, esp. not
 *          VINF_CPUM_R3_MSR_WRITE.  Only the following two:
 * @retval  VINF_SUCCESS
 * @retval  VERR_CPUM_RAISE_GP_0
 * @retval  VERR_INVALID_POINTER
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   u64BaseMsr  The value to set.
 */
VMM_INT_DECL(int) PDMApicSetBaseMsr(PVMCPUCC pVCpu, uint64_t u64BaseMsr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetBaseMsr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetBaseMsr(pVCpu, u64BaseMsr);
}


/**
 * Gets the next highest-priority interrupt from the APIC, marking it as an
 * "in-service" interrupt.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pu8Vector   Where to store the vector.
 * @param   puSrcTag    Where to store the interrupt source tag (debugging).
 */
VMM_INT_DECL(int) PDMApicGetInterrupt(PVMCPUCC pVCpu, uint8_t *pu8Vector, uint32_t *pu32TagSrc)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetInterrupt, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetInterrupt(pVCpu, pu8Vector, pu32TagSrc);
}


/**
 * Delivers an interrupt message via the system bus.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   uDest           The destination mask.
 * @param   uDestMode       The destination mode.
 * @param   uDeliveryMode   The delivery mode.
 * @param   uVector         The interrupt vector.
 * @param   uPolarity       The interrupt line polarity.
 * @param   uTriggerMode    The trigger mode.
 * @param   uSrcTag         The interrupt source tag (debugging).
 */
VMM_INT_DECL(int) PDMApicBusDeliver(PVMCC pVM, uint8_t uDest, uint8_t uDestMode, uint8_t uDeliveryMode, uint8_t uVector,
                                    uint8_t uPolarity, uint8_t uTriggerMode, uint32_t uTagSrc)
{
    AssertReturn(PDM_TO_APICBACKEND(pVM)->pfnBusDeliver, VERR_INVALID_POINTER);
    return PDM_TO_APICBACKEND(pVM)->pfnBusDeliver(pVM, uDest, uDestMode, uDeliveryMode, uVector, uPolarity, uTriggerMode,
                                                  uTagSrc);
}


#ifdef IN_RING0
/**
 * Gets the APIC page pointers for the specified VCPU.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pHCPhys         Where to store the host-context physical address.
 * @param   pR0Ptr          Where to store the ring-0 address.
 * @param   pR3Ptr          Where to store the ring-3 address (optional).
 */
VMM_INT_DECL(int) PDMR0ApicGetApicPageForCpu(PCVMCPUCC pVCpu, PRTHCPHYS pHCPhys, PRTR0PTR pR0Ptr, PRTR3PTR pR3Ptr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetApicPageForCpu, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetApicPageForCpu(pVCpu, pHCPhys, pR0Ptr, pR3Ptr);
}
#endif /* IN_RING0 */


#ifdef IN_RING3
/**
 * Sets whether Hyper-V compatibility mode (MSR interface) is enabled or not.
 *
 * This mode is a hybrid of xAPIC and x2APIC modes, some caveats:
 * 1. MSRs are used even ones that are missing (illegal) in x2APIC like DFR.
 * 2. A single ICR is used by the guest to send IPIs rather than 2 ICR writes.
 * 3. It is unclear what the behaviour will be when invalid bits are set,
 *    currently we follow x2APIC behaviour of causing a \#GP.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   fHyperVCompatMode   Whether the compatibility mode is enabled.
 */
VMMR3_INT_DECL(int) PDMR3ApicHvSetCompatMode(PVM pVM, bool fHyperVCompatMode)
{
    AssertReturn(PDM_TO_APICBACKEND(pVM)->pfnHvSetCompatMode, VERR_INVALID_POINTER);
    return PDM_TO_APICBACKEND(pVM)->pfnHvSetCompatMode(pVM, fHyperVCompatMode);
}
#endif /* IN_RING3 */


/**
 * Posts an interrupt to a target APIC.
 * Paravirtualized Hyper-V interface.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   uVector             The vector of the interrupt to be posted.
 * @param   fAutoEoi            Whether this interrupt has automatic EOI
 *                              treatment.
 * @param   enmTriggerMode      The trigger mode of the interrupt.
 *
 * @thread  Any.
 */
VMM_INT_DECL(void) PDMApicHvSendInterrupt(PVMCPUCC pVCpu, uint8_t uVector, bool fAutoEoi, XAPICTRIGGERMODE enmTriggerMode)
{
    AssertReturnVoid(PDMCPU_TO_APICBACKEND(pVCpu)->pfnPostInterrupt);
    PDMCPU_TO_APICBACKEND(pVCpu)->pfnPostInterrupt(pVCpu, uVector, enmTriggerMode, fAutoEoi, 0 /* u32SrcTag */);
}


/**
 * Sets the Task Priority Register (TPR).
 * Paravirtualized Hyper-V interface.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uTpr        The TPR value to set.
 *
 * @remarks Validates like in x2APIC mode.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicHvSetTpr(PVMCPUCC pVCpu, uint8_t uTpr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetTpr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetTpr(pVCpu, uTpr, true /* fForceX2ApicBehaviour */ );
}


/**
 * Gets the Task Priority Register (TPR).
 * Paravirtualized Hyper-V interface.
 *
 * @returns The TPR value.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
VMM_INT_DECL(uint8_t) PDMApicHvGetTpr(PVMCPUCC pVCpu)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnReadRaw32, 0);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnReadRaw32(pVCpu, XAPIC_OFF_TPR);
}


/**
 * Sets the Interrupt Command Register (ICR).
 * Paravirtualized Hyper-V interface.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   uIcr        The ICR value to set.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicHvSetIcr(PVMCPUCC pVCpu, uint64_t uIcr)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetIcr, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetIcr(pVCpu, uIcr, VINF_CPUM_R3_MSR_WRITE);
}


/**
 * Gets the Interrupt Command Register (ICR).
 * Paravirtualized Hyper-V interface.
 *
 * @returns The ICR value.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
VMM_INT_DECL(uint64_t) PDMApicHvGetIcr(PVMCPUCC pVCpu)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetIcrNoCheck, 0);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnGetIcrNoCheck(pVCpu);
}


/**
 * Sets the End-Of-Interrupt (EOI) register.
 * Paravirtualized Hyper-V interface.
 *
 * @returns Strict VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uEoi            The EOI value.
 */
VMM_INT_DECL(VBOXSTRICTRC) PDMApicHvSetEoi(PVMCPUCC pVCpu, uint32_t uEoi)
{
    AssertReturn(PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetEoi, VERR_INVALID_POINTER);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnSetEoi(pVCpu, uEoi, true /* fForceX2ApicBehaviour */);
}


#ifdef IN_RING3
/**
 * Initializes per-VCPU APIC to the state following an INIT reset
 * ("Wait-for-SIPI" state).
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(void) PDMR3ApicInitIpi(PVMCPU pVCpu)
{
    AssertReturnVoid(PDMCPU_TO_APICBACKEND(pVCpu)->pfnInitIpi);
    return PDMCPU_TO_APICBACKEND(pVCpu)->pfnInitIpi(pVCpu);
}
#endif /* IN_RING3 */


/**
 * Registers a PDM APIC backend.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   enmBackendType  The PDM APIC backend type.
 * @param   pBackend        The PDM APIC backend.
 */
VMM_INT_DECL(int) PDMApicRegisterBackend(PVMCC pVM, PDMAPICBACKENDTYPE enmBackendType, PCPDMAPICBACKEND pBackend)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pVM, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pBackend, VERR_INVALID_PARAMETER);
    AssertReturn(   enmBackendType > PDMAPICBACKENDTYPE_NONE
                 && enmBackendType < PDMAPICBACKENDTYPE_END, VERR_INVALID_PARAMETER);

    AssertPtrReturn(pBackend->pfnIsEnabled,                 VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnInitIpi,                   VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetBaseMsrNoCheck,         VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetBaseMsr,                VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetBaseMsr,                VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnReadRaw32,                 VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnReadMsr,                   VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnWriteMsr,                  VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetTpr,                    VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetTpr,                    VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetIcrNoCheck,             VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetIcr,                    VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetTimerFreq,              VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetLocalInterrupt,         VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnGetInterrupt,              VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnPostInterrupt,             VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnUpdatePendingInterrupts,   VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnBusDeliver,                VERR_INVALID_POINTER);
    AssertPtrReturn(pBackend->pfnSetEoi,                    VERR_INVALID_POINTER);
#if defined(IN_RING3)
    AssertPtrReturn(pBackend->pfnHvSetCompatMode,           VERR_INVALID_POINTER);
#elif defined(IN_RING0)
    AssertPtrReturn(pBackend->pfnGetApicPageForCpu,         VERR_INVALID_POINTER);
#endif

    /*
     * Register the backend.
     */
    pVM->pdm.s.Ic.enmKind = enmBackendType;
#ifdef IN_RING3
    pVM->pdm.s.Ic.ApicBackend = *pBackend;
#else
    pVM->pdmr0.s.Ic.ApicBackend = *pBackend;
#endif

#ifdef IN_RING3
    LogRel(("PDM: %s APIC backend registered\n", PDMApicGetBackendName(enmBackendType)));
#endif
    return VINF_SUCCESS;
}

