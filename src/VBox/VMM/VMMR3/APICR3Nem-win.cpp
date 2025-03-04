/* $Id$ */
/** @file
 * GIC - Generic Interrupt Controller Architecture (GIC) - Hyper-V interface.
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
#include <iprt/mem.h>
#include <WinHvPlatform.h>

#include <VBox/log.h>
#include "APICInternal.h"
#include "NEMInternal.h" /* Need access to the partition handle. */
#include <VBox/vmm/pdmapic.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/vm.h>

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * APICHv PDM instance data (per-VM).
 */
typedef struct APICHVDEV
{
    /** Pointer to the PDM device instance. */
    PPDMDEVINSR3         pDevIns;
    /** The partition handle grabbed from NEM. */
    WHV_PARTITION_HANDLE hPartition;
    /** Cached TPR value. */
    uint8_t              bTpr;
} APICHVDEV;
/** Pointer to a APIC Hyper-V device. */
typedef APICHVDEV *PAPICHVDEV;
/** Pointer to a const APIC Hyper-V device. */
typedef APICHVDEV const *PCAPICHVDEV;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern decltype(WHvGetVirtualProcessorState)                     *g_pfnWHvGetVirtualProcessorState;
extern decltype(WHvSetVirtualProcessorState)                     *g_pfnWHvSetVirtualProcessorState;
extern decltype(WHvGetVirtualProcessorInterruptControllerState2) *g_pfnWHvGetVirtualProcessorInterruptControllerState2;
extern decltype(WHvSetVirtualProcessorInterruptControllerState2) *g_pfnWHvSetVirtualProcessorInterruptControllerState2;
extern decltype(WHvRequestInterrupt)                             *g_pfnWHvRequestInterrupt;

/*
 * Let the preprocessor alias the APIs to import variables for better autocompletion.
 */
#ifndef IN_SLICKEDIT
# define WHvGetVirtualProcessorState                     g_pfnWHvGetVirtualProcessorState
# define WHvSetVirtualProcessorState                     g_pfnWHvSetVirtualProcessorState
# define WHvGetVirtualProcessorInterruptControllerState2 g_pfnWHvGetVirtualProcessorInterruptControllerState2
# define WHvSetVirtualProcessorInterruptControllerState2 g_pfnWHvSetVirtualProcessorInterruptControllerState2
# define WHvRequestInterrupt                             g_pfnWHvRequestInterrupt
#endif


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnIsEnabled}
 */
static DECLCALLBACK(bool) apicR3HvIsEnabled(PCVMCPUCC pVCpu)
{
    /*
     * We should never end up here as this is called only from the VMX and SVM
     * code in R0 which we don't run if this is active.
     */
    RT_NOREF(pVCpu);
    AssertFailedReturn(false);
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnInitIpi}
 */
static DECLCALLBACK(void) apicR3HvInitIpi(PVMCPUCC pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);

    /*
     * See Intel spec. 10.4.7.3 "Local APIC State After an INIT Reset (Wait-for-SIPI State)"
     * and AMD spec 16.3.2 "APIC Registers".
     *
     * The reason we don't simply zero out the entire APIC page and only set the non-zero members
     * is because there are some registers that are not touched by the INIT IPI (e.g. version)
     * operation and this function is only a subset of the reset operation.
     */
    RT_ZERO(pXApicPage->irr);
    RT_ZERO(pXApicPage->irr);
    RT_ZERO(pXApicPage->isr);
    RT_ZERO(pXApicPage->tmr);
    RT_ZERO(pXApicPage->icr_hi);
    RT_ZERO(pXApicPage->icr_lo);
    RT_ZERO(pXApicPage->ldr);
    RT_ZERO(pXApicPage->tpr);
    RT_ZERO(pXApicPage->ppr);
    RT_ZERO(pXApicPage->timer_icr);
    RT_ZERO(pXApicPage->timer_ccr);
    RT_ZERO(pXApicPage->timer_dcr);

    pXApicPage->dfr.u.u4Model        = XAPICDESTFORMAT_FLAT;
    pXApicPage->dfr.u.u28ReservedMb1 = UINT32_C(0xfffffff);

    /** @todo CMCI. */

    RT_ZERO(pXApicPage->lvt_timer);
    pXApicPage->lvt_timer.u.u1Mask = 1;

#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    RT_ZERO(pXApicPage->lvt_thermal);
    pXApicPage->lvt_thermal.u.u1Mask = 1;
#endif

    RT_ZERO(pXApicPage->lvt_perf);
    pXApicPage->lvt_perf.u.u1Mask = 1;

    RT_ZERO(pXApicPage->lvt_lint0);
    pXApicPage->lvt_lint0.u.u1Mask = 1;

    RT_ZERO(pXApicPage->lvt_lint1);
    pXApicPage->lvt_lint1.u.u1Mask = 1;

    RT_ZERO(pXApicPage->lvt_error);
    pXApicPage->lvt_error.u.u1Mask = 1;

    RT_ZERO(pXApicPage->svr);
    pXApicPage->svr.u.u8SpuriousVector = 0xff;

    /* The self-IPI register is reset to 0. See Intel spec. 10.12.5.1 "x2APIC States" */
    PX2APICPAGE pX2ApicPage = VMCPU_TO_X2APICPAGE(pVCpu);
    RT_ZERO(pX2ApicPage->self_ipi);

    /* Clear the pending-interrupt bitmaps. */
    PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
#if 0
    RT_BZERO(&pApicCpu->ApicPibLevel, sizeof(APICPIB));
    RT_BZERO(pApicCpu->CTX_SUFF(pvApicPib), sizeof(APICPIB));
#endif

    /* Clear the interrupt line states for LINT0 and LINT1 pins. */
    pApicCpu->fActiveLint0 = false;
    pApicCpu->fActiveLint1 = false;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnSetBaseMsr}
 */
static DECLCALLBACK(int) apicR3HvSetBaseMsr(PVMCPUCC pVCpu, uint64_t u64BaseMsr)
{
    RT_NOREF(pVCpu, u64BaseMsr);
    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetBaseMsrNoCheck}
 */
static DECLCALLBACK(uint64_t) apicR3HvGetBaseMsrNoCheck(PCVMCPUCC pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
    PCAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);
    return pApicCpu->uApicBaseMsr;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetBaseMsr}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvGetBaseMsr(PVMCPUCC pVCpu, uint64_t *pu64Value)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    PCAPIC pApic = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    if (pApic->enmMaxMode != PDMAPICMODE_NONE)
    {
        *pu64Value = apicR3HvGetBaseMsrNoCheck(pVCpu);
        return VINF_SUCCESS;
    }

    if (pVCpu->apic.s.cLogMaxGetApicBaseAddr++ < 5)
        LogRel(("APIC%u: Reading APIC base MSR (%#x) when there is no APIC -> #GP(0)\n", pVCpu->idCpu, MSR_IA32_APICBASE));
    return VERR_CPUM_RAISE_GP_0;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnReadRaw32}
 */
static DECLCALLBACK(uint32_t) apicR3HvReadRaw32(PCVMCPUCC pVCpu, uint16_t offReg)
{
    RT_NOREF(pVCpu, offReg);
    AssertFailed();
    return 0;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnReadMsr}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvReadMsr(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(u32Reg >= MSR_IA32_X2APIC_ID && u32Reg <= MSR_IA32_X2APIC_SELF_IPI);
    Assert(pu64Value);

    RT_NOREF(pVCpu, u32Reg, pu64Value);
    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnWriteMsr}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvWriteMsr(PVMCPUCC pVCpu, uint32_t u32Reg, uint64_t u64Value)
{
    /*
     * Validate.
     */
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(u32Reg >= MSR_IA32_X2APIC_ID && u32Reg <= MSR_IA32_X2APIC_SELF_IPI);

    RT_NOREF(pVCpu, u32Reg, u64Value);
    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnSetTpr}
 */
static DECLCALLBACK(int) apicR3HvSetTpr(PVMCPUCC pVCpu, uint8_t u8Tpr, bool fForceX2ApicBehaviour)
{
    RT_NOREF(fForceX2ApicBehaviour);
    pVCpu->nem.s.bTpr = u8Tpr;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetTpr}
 */
static DECLCALLBACK(int) apicR3HvGetTpr(PCVMCPUCC pVCpu, uint8_t *pu8Tpr, bool *pfPending, uint8_t *pu8PendingIntr)
{
    VMCPU_ASSERT_EMT(pVCpu);

    RT_NOREF(pfPending, pu8PendingIntr);
    *pu8Tpr = pVCpu->nem.s.bTpr;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetIcrNoCheck}
 */
static DECLCALLBACK(uint64_t) apicR3HvGetIcrNoCheck(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
    AssertFailed();
    return 0;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnSetIcr}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvSetIcr(PVMCPUCC pVCpu, uint64_t u64Icr, int rcRZ)
{
    VMCPU_ASSERT_EMT(pVCpu);

    RT_NOREF(pVCpu, u64Icr, rcRZ);
    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetTimerFreq}
 */
static DECLCALLBACK(int) apicR3HvGetTimerFreq(PVMCC pVM, uint64_t *pu64Value)
{
    /*
     * Validate.
     */
    Assert(pVM);
    AssertPtrReturn(pu64Value, VERR_INVALID_PARAMETER);

    RT_NOREF(pVM, pu64Value);
    AssertFailed();
    return VERR_PDM_NO_APIC_INSTANCE;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnSetLocalInterrupt}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvSetLocalInterrupt(PVMCPUCC pVCpu, uint8_t u8Pin, uint8_t u8Level, int rcRZ)
{
    AssertReturn(u8Pin <= 1, VERR_INVALID_PARAMETER);
    AssertReturn(u8Level <= 1, VERR_INVALID_PARAMETER);

    RT_NOREF(rcRZ);
    /* The rest is handled in the NEM backend. */
    if (u8Level)
        VMCPU_FF_SET(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    else
        VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_INTERRUPT_PIC);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnGetInterrupt}
 */
static DECLCALLBACK(int) apicR3HvGetInterrupt(PVMCPUCC pVCpu, uint8_t *pu8Vector, uint32_t *puSrcTag)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pu8Vector);

    RT_NOREF(pVCpu, pu8Vector, puSrcTag);
    AssertFailed();
    return VERR_APIC_INTR_NOT_PENDING;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnPostInterrupt}
 */
static DECLCALLBACK(bool) apicR3HvPostInterrupt(PVMCPUCC pVCpu, uint8_t uVector, XAPICTRIGGERMODE enmTriggerMode, bool fAutoEoi,
                                                uint32_t uSrcTag)
{
    Assert(pVCpu);
    Assert(uVector > XAPIC_ILLEGAL_VECTOR_END);
    RT_NOREF(fAutoEoi);

    RT_NOREF(pVCpu, uVector, enmTriggerMode, fAutoEoi, uSrcTag);
    AssertFailed();
    return false;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnUpdatePendingInterrupts}
 */
static DECLCALLBACK(void) apicR3HvUpdatePendingInterrupts(PVMCPUCC pVCpu)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);
    RT_NOREF(pVCpu);
    AssertFailed();
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnBusDeliver}
 */
static DECLCALLBACK(int) apicR3HvBusDeliver(PVMCC pVM, uint8_t uDest, uint8_t uDestMode, uint8_t uDeliveryMode, uint8_t uVector,
                                            uint8_t uPolarity, uint8_t uTriggerMode, uint32_t uSrcTag)
{
    RT_NOREF(uPolarity, uSrcTag);

    Assert(pVM->nem.s.fLocalApicEmulation);

    WHV_INTERRUPT_CONTROL Control; RT_ZERO(Control);
    Control.Type            = uDeliveryMode; /* Matching up. */
    Control.DestinationMode = uDestMode;
    Control.TriggerMode     = uTriggerMode;
    Control.Destination     = uDest;
    Control.Vector          = uVector;

    HRESULT hrc = WHvRequestInterrupt(pVM->nem.s.hPartition, &Control, sizeof(Control));
    if (FAILED(hrc))
    {
        LogRelMax(10, ("APIC/WHv: Delivering interrupt failed: %Rhrc (Last=%#x/%u)",
                        hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
        return VERR_APIC_INTR_DISCARDED;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnSetEoi}
 */
static DECLCALLBACK(VBOXSTRICTRC) apicR3HvSetEoi(PVMCPUCC pVCpu, uint32_t uEoi, bool fForceX2ApicBehaviour)
{
    VMCPU_ASSERT_EMT(pVCpu);

    RT_NOREF(pVCpu, uEoi, fForceX2ApicBehaviour);
    AssertFailed();
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMAPICBACKEND,pfnHvSetCompatMode}
 */
static DECLCALLBACK(int) apicR3NemHvSetCompatMode(PVM pVM, bool fHyperVCompatMode)
{
    RT_NOREF(pVM, fHyperVCompatMode);
    //AssertFailed();
    return VINF_SUCCESS;
}


/**
 * Resets the APIC base MSR.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
static void apicResetBaseMsr(PVMCPUCC pVCpu)
{
    /*
     * Initialize the APIC base MSR. The APIC enable-bit is set upon power-up or reset[1].
     *
     * A Reset (in xAPIC and x2APIC mode) brings up the local APIC in xAPIC mode.
     * An INIT IPI does -not- cause a transition between xAPIC and x2APIC mode[2].
     *
     * [1] See AMD spec. 14.1.3 "Processor Initialization State"
     * [2] See Intel spec. 10.12.5.1 "x2APIC States".
     */
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    /* Construct. */
    PAPICCPU pApicCpu     = VMCPU_TO_APICCPU(pVCpu);
    PAPIC    pApic        = VM_TO_APIC(pVCpu->CTX_SUFF(pVM));
    uint64_t uApicBaseMsr = MSR_IA32_APICBASE_ADDR;
    if (pVCpu->idCpu == 0)
        uApicBaseMsr |= MSR_IA32_APICBASE_BSP;

    /* If the VM was configured with no APIC, don't enable xAPIC mode, obviously. */
    if (pApic->enmMaxMode != PDMAPICMODE_NONE)
    {
        uApicBaseMsr |= MSR_IA32_APICBASE_EN;

        /*
         * While coming out of a reset the APIC is enabled and in xAPIC mode. If software had previously
         * disabled the APIC (which results in the CPUID bit being cleared as well) we re-enable it here.
         * See Intel spec. 10.12.5.1 "x2APIC States".
         */
        if (CPUMSetGuestCpuIdPerCpuApicFeature(pVCpu, true /*fVisible*/) == false)
            LogRel(("APIC%u: Resetting mode to xAPIC\n", pVCpu->idCpu));
    }

    /* Commit. */
    ASMAtomicWriteU64(&pApicCpu->uApicBaseMsr, uApicBaseMsr);
}


/**
 * Initializes per-VCPU APIC to the state following a power-up or hardware
 * reset.
 *
 * @param   pVCpu               The cross context virtual CPU structure.
 * @param   fResetApicBaseMsr   Whether to reset the APIC base MSR.
 */
static void apicR3HvResetCpu(PVMCPUCC pVCpu, bool fResetApicBaseMsr)
{
    VMCPU_ASSERT_EMT_OR_NOT_RUNNING(pVCpu);

    LogFlow(("APIC%u: apicR3ResetCpu: fResetApicBaseMsr=%RTbool\n", pVCpu->idCpu, fResetApicBaseMsr));

#ifdef VBOX_STRICT
    /* Verify that the initial APIC ID reported via CPUID matches our VMCPU ID assumption. */
    uint32_t uEax, uEbx, uEcx, uEdx;
    uEax = uEbx = uEcx = uEdx = UINT32_MAX;
    CPUMGetGuestCpuId(pVCpu, 1, 0, -1 /*f64BitMode*/, &uEax, &uEbx, &uEcx, &uEdx);
    Assert(((uEbx >> 24) & 0xff) == pVCpu->idCpu);
#endif

    /*
     * The state following a power-up or reset is a superset of the INIT state.
     * See Intel spec. 10.4.7.3 "Local APIC State After an INIT Reset ('Wait-for-SIPI' State)"
     */
    apicR3HvInitIpi(pVCpu);

    /*
     * The APIC version register is read-only, so just initialize it here.
     * It is not clear from the specs, where exactly it is initialized.
     * The version determines the number of LVT entries and size of the APIC ID (8 bits for P4).
     */
    PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
#if XAPIC_HARDWARE_VERSION == XAPIC_HARDWARE_VERSION_P4
    pXApicPage->version.u.u8MaxLvtEntry = XAPIC_MAX_LVT_ENTRIES_P4 - 1;
    pXApicPage->version.u.u8Version     = XAPIC_HARDWARE_VERSION_P4;
    AssertCompile(sizeof(pXApicPage->id.u8ApicId) >= XAPIC_APIC_ID_BIT_COUNT_P4 / 8);
#else
# error "Implement Pentium and P6 family APIC architectures"
#endif

    /** @todo It isn't clear in the spec. where exactly the default base address
     *        is (re)initialized, atm we do it here in Reset. */
    if (fResetApicBaseMsr)
        apicResetBaseMsr(pVCpu);

    /*
     * Initialize the APIC ID register to xAPIC format.
     */
    RT_BZERO(&pXApicPage->id, sizeof(pXApicPage->id));
    pXApicPage->id.u8ApicId = pVCpu->idCpu;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
DECLCALLBACK(void) apicR3HvReset(PPDMDEVINS pDevIns)
{
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    VM_ASSERT_EMT0(pVM);
    VM_ASSERT_IS_NOT_RUNNING(pVM);

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpuDest = pVM->apCpusR3[idCpu];
        PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpuDest);

        apicR3HvResetCpu(pVCpuDest, true /*fResetApicBaseMsr*/);

        HRESULT hrc;
        if (WHvSetVirtualProcessorState)
            hrc = WHvSetVirtualProcessorState(pVM->nem.s.hPartition, idCpu, WHvVirtualProcessorStateTypeInterruptControllerState2,
                                              pXApicPage, sizeof(*pXApicPage));
        else
            hrc = WHvSetVirtualProcessorInterruptControllerState2(pVM->nem.s.hPartition, idCpu, pXApicPage, sizeof(*pXApicPage));
        AssertRelease(SUCCEEDED(hrc));
        AssertRelease(SUCCEEDED(hrc));
    }

    LogFlow(("GIC: gicR3HvReset\n"));
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
DECLCALLBACK(int) apicR3HvDestruct(PPDMDEVINS pDevIns)
{
    LogFlowFunc(("pDevIns=%p\n", pDevIns));
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
DECLCALLBACK(int) apicR3HvConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PAPICHVDEV      pThis    = PDMDEVINS_2_DATA(pDevIns, PAPICHVDEV);
    PCPDMDEVHLPR3   pHlp     = pDevIns->pHlpR3;
    PVM             pVM      = PDMDevHlpGetVM(pDevIns);
    Assert(iInstance == 0); NOREF(iInstance);

    RT_NOREF(pCfg, pHlp);

    /*
     * Init the data.
     */
    //pGic->pDevInsR3   = pDevIns;
    pThis->pDevIns    = pDevIns;
    pThis->hPartition = pVM->nem.s.hPartition;

    /*
     * Validate GIC settings.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "Mode|IOAPIC|NumCPUs|MacOSWorkaround", "");

    /*
     * Disable automatic PDM locking for this device.
     */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Register the APIC with PDM.
     */
    rc = PDMDevHlpIcRegister(pDevIns);
    AssertLogRelRCReturn(rc, rc);

    rc = PDMApicRegisterBackend(pVM, PDMAPICBACKENDTYPE_HYPERV, &g_ApicNemBackend);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Allocate the map the virtual-APIC pages (for syncing the state).
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU   pVCpu    = pVM->apCpusR3[idCpu];
        PAPICCPU pApicCpu = VMCPU_TO_APICCPU(pVCpu);

        Assert(pVCpu->idCpu == idCpu);
        Assert(pApicCpu->pvApicPageR3 == NIL_RTR3PTR);
        AssertCompile(sizeof(XAPICPAGE) <= HOST_PAGE_SIZE);
        pApicCpu->cbApicPage = sizeof(XAPICPAGE);
        rc = SUPR3PageAlloc(1 /* cHostPages */, 0 /* fFlags */, &pApicCpu->pvApicPageR3);
        if (RT_SUCCESS(rc))
        {
            AssertLogRelReturn(pApicCpu->pvApicPageR3 != NIL_RTR3PTR, VERR_INTERNAL_ERROR);

            /* Initialize the virtual-APIC state. */
            RT_BZERO(pApicCpu->pvApicPageR3, pApicCpu->cbApicPage);
            apicR3HvResetCpu(pVCpu, true /* fResetApicBaseMsr */);

            PXAPICPAGE pXApicPage = VMCPU_TO_XAPICPAGE(pVCpu);
            HRESULT hrc;
            if (WHvSetVirtualProcessorState)
                hrc = WHvSetVirtualProcessorState(pVM->nem.s.hPartition, idCpu, WHvVirtualProcessorStateTypeInterruptControllerState2,
                                                  pXApicPage, sizeof(*pXApicPage));
            else
                hrc = WHvSetVirtualProcessorInterruptControllerState2(pVM->nem.s.hPartition, idCpu, pXApicPage, sizeof(*pXApicPage));
            AssertRelease(SUCCEEDED(hrc));
        }
        else
        {
            LogRel(("APIC%u: Failed to allocate %u bytes for the virtual-APIC page, rc=%Rrc\n", idCpu, pApicCpu->cbApicPage, rc));
            return rc;
        }
    }

    /*
     * Register saved state callbacks.
     */
    //rc = PDMDevHlpSSMRegister(pDevIns, GIC_NEM_SAVED_STATE_VERSION, 0 /*cbGuess*/, gicR3HvSaveExec, gicR3HvLoadExec);
    //AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * APIC device registration structure.
 */
const PDMDEVREG g_DeviceAPICNem =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "apic-nem",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PIC,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(APICHVDEV),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         0,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Advanced Programmable Interrupt Controller - Hyper-V variant",
#if defined(IN_RING3)
    /* .szRCMod = */                "VMMRC.rc",
    /* .szR0Mod = */                "VMMR0.r0",
    /* .pfnConstruct = */           apicR3HvConstruct,
    /* .pfnDestruct = */            apicR3HvDestruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               apicR3HvReset,
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
 * The Hyper-V APIC backend.
 */
const PDMAPICBACKEND g_ApicNemBackend =
{
    /* .pfnIsEnabled = */               apicR3HvIsEnabled,
    /* .pfnInitIpi = */                 apicR3HvInitIpi,
    /* .pfnGetBaseMsrNoCheck = */       apicR3HvGetBaseMsrNoCheck,
    /* .pfnGetBaseMsr = */              apicR3HvGetBaseMsr,
    /* .pfnSetBaseMsr = */              apicR3HvSetBaseMsr,
    /* .pfnReadRaw32 = */               apicR3HvReadRaw32,
    /* .pfnReadMsr = */                 apicR3HvReadMsr,
    /* .pfnWriteMsr = */                apicR3HvWriteMsr,
    /* .pfnGetTpr = */                  apicR3HvGetTpr,
    /* .pfnSetTpr = */                  apicR3HvSetTpr,
    /* .pfnGetIcrNoCheck = */           apicR3HvGetIcrNoCheck,
    /* .pfnSetIcr = */                  apicR3HvSetIcr,
    /* .pfnGetTimerFreq = */            apicR3HvGetTimerFreq,
    /* .pfnSetLocalInterrupt = */       apicR3HvSetLocalInterrupt,
    /* .pfnGetInterrupt = */            apicR3HvGetInterrupt,
    /* .pfnPostInterrupt = */           apicR3HvPostInterrupt,
    /* .pfnUpdatePendingInterrupts = */ apicR3HvUpdatePendingInterrupts,
    /* .pfnBusDeliver = */              apicR3HvBusDeliver,
    /* .pfnSetEoi = */                  apicR3HvSetEoi,
    /* .pfnHvSetCompatMode = */         apicR3NemHvSetCompatMode,
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

