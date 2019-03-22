/* $Id$ */
/** @file
 * HM - All contexts.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pgm.h>
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/x86.h>
#include <iprt/asm-amd64-x86.h>


/**
 * Checks whether HM (VT-x/AMD-V) is being used by this VM.
 *
 * @retval  true if used.
 * @retval  false if software virtualization (raw-mode) is used.
 * @param   pVM        The cross context VM structure.
 * @sa      HMIsEnabled, HMR3IsEnabled
 * @internal
 */
VMMDECL(bool) HMIsEnabledNotMacro(PVM pVM)
{
    Assert(pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NOT_SET);
    return pVM->fHMEnabled;
}


/**
 * Queues a guest page for invalidation.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   GCVirt      Page to invalidate.
 */
static void hmQueueInvlPage(PVMCPU pVCpu, RTGCPTR GCVirt)
{
    /* Nothing to do if a TLB flush is already pending */
    if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_TLB_FLUSH))
        return;
    VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    NOREF(GCVirt);
}


/**
 * Invalidates a guest page.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   GCVirt      Page to invalidate.
 */
VMM_INT_DECL(int) HMInvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt)
{
    STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushPageManual);
#ifdef IN_RING0
    return HMR0InvalidatePage(pVCpu, GCVirt);
#else
    hmQueueInvlPage(pVCpu, GCVirt);
    return VINF_SUCCESS;
#endif
}


#ifdef IN_RING0

/**
 * Dummy RTMpOnSpecific handler since RTMpPokeCpu couldn't be used.
 *
 */
static DECLCALLBACK(void) hmFlushHandler(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    NOREF(idCpu); NOREF(pvUser1); NOREF(pvUser2);
    return;
}


/**
 * Wrapper for RTMpPokeCpu to deal with VERR_NOT_SUPPORTED.
 */
static void hmR0PokeCpu(PVMCPU pVCpu, RTCPUID idHostCpu)
{
    uint32_t cWorldSwitchExits = ASMAtomicUoReadU32(&pVCpu->hm.s.cWorldSwitchExits);

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatPoke, x);
    int rc = RTMpPokeCpu(idHostCpu);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatPoke, x);

    /* Not implemented on some platforms (Darwin, Linux kernel < 2.6.19); fall
       back to a less efficient implementation (broadcast). */
    if (rc == VERR_NOT_SUPPORTED)
    {
        STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatSpinPoke, z);
        /* synchronous. */
        RTMpOnSpecific(idHostCpu, hmFlushHandler, 0, 0);
        STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatSpinPoke, z);
    }
    else
    {
        if (rc == VINF_SUCCESS)
            STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatSpinPoke, z);
        else
            STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatSpinPokeFailed, z);

/** @todo If more than one CPU is going to be poked, we could optimize this
 *        operation by poking them first and wait afterwards.  Would require
 *        recording who to poke and their current cWorldSwitchExits values,
 *        that's something not suitable for stack... So, pVCpu->hm.s.something
 *        then. */
        /* Spin until the VCPU has switched back (poking is async). */
        while (   ASMAtomicUoReadBool(&pVCpu->hm.s.fCheckedTLBFlush)
               && cWorldSwitchExits == ASMAtomicUoReadU32(&pVCpu->hm.s.cWorldSwitchExits))
            ASMNopPause();

        if (rc == VINF_SUCCESS)
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatSpinPoke, z);
        else
            STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatSpinPokeFailed, z);
    }
}

#endif /* IN_RING0 */
#ifndef IN_RC

/**
 * Flushes the guest TLB.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(int) HMFlushTLB(PVMCPU pVCpu)
{
    LogFlow(("HMFlushTLB\n"));

    VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
    STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbManual);
    return VINF_SUCCESS;
}

/**
 * Poke an EMT so it can perform the appropriate TLB shootdowns.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              EMT poke.
 * @param   fAccountFlushStat   Whether to account the call to
 *                              StatTlbShootdownFlush or StatTlbShootdown.
 */
static void hmPokeCpuForTlbFlush(PVMCPU pVCpu, bool fAccountFlushStat)
{
    if (ASMAtomicUoReadBool(&pVCpu->hm.s.fCheckedTLBFlush))
    {
        if (fAccountFlushStat)
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTlbShootdownFlush);
        else
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTlbShootdown);
#ifdef IN_RING0
        RTCPUID idHostCpu = pVCpu->hm.s.idEnteredCpu;
        if (idHostCpu != NIL_RTCPUID)
            hmR0PokeCpu(pVCpu, idHostCpu);
#else
        VMR3NotifyCpuFFU(pVCpu->pUVCpu, VMNOTIFYFF_FLAGS_POKE);
#endif
    }
    else
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushPageManual);
}


/**
 * Invalidates a guest page on all VCPUs.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   GCVirt      Page to invalidate.
 */
VMM_INT_DECL(int) HMInvalidatePageOnAllVCpus(PVM pVM, RTGCPTR GCVirt)
{
    /*
     * The VT-x/AMD-V code will be flushing TLB each time a VCPU migrates to a different
     * host CPU, see hmR0VmxFlushTaggedTlbBoth() and hmR0SvmFlushTaggedTlb().
     *
     * This is the reason why we do not care about thread preemption here and just
     * execute HMInvalidatePage() assuming it might be the 'right' CPU.
     */
    VMCPUID idCurCpu = VMMGetCpuId(pVM);
    STAM_COUNTER_INC(&pVM->aCpus[idCurCpu].hm.s.StatFlushPage);

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];

        /* Nothing to do if a TLB flush is already pending; the VCPU should
           have already been poked if it were active. */
        if (VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_TLB_FLUSH))
            continue;

        if (pVCpu->idCpu == idCurCpu)
            HMInvalidatePage(pVCpu, GCVirt);
        else
        {
            hmQueueInvlPage(pVCpu, GCVirt);
            hmPokeCpuForTlbFlush(pVCpu, false /* fAccountFlushStat */);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Flush the TLBs of all VCPUs.
 *
 * @returns VBox status code.
 * @param   pVM       The cross context VM structure.
 */
VMM_INT_DECL(int) HMFlushTLBOnAllVCpus(PVM pVM)
{
    if (pVM->cCpus == 1)
        return HMFlushTLB(&pVM->aCpus[0]);

    VMCPUID idThisCpu = VMMGetCpuId(pVM);

    STAM_COUNTER_INC(&pVM->aCpus[idThisCpu].hm.s.StatFlushTlb);

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];

        /* Nothing to do if a TLB flush is already pending; the VCPU should
           have already been poked if it were active. */
        if (!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_TLB_FLUSH))
        {
            VMCPU_FF_SET(pVCpu, VMCPU_FF_TLB_FLUSH);
            if (idThisCpu != idCpu)
                hmPokeCpuForTlbFlush(pVCpu, true /* fAccountFlushStat */);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Invalidates a guest page by physical address.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   GCPhys      Page to invalidate.
 *
 * @remarks Assumes the current instruction references this physical page
 *          though a virtual address!
 */
VMM_INT_DECL(int) HMInvalidatePhysPage(PVM pVM, RTGCPHYS GCPhys)
{
    if (!HMIsNestedPagingActive(pVM))
        return VINF_SUCCESS;

    /*
     * AMD-V: Doesn't support invalidation with guest physical addresses.
     *
     * VT-x: Doesn't support invalidation with guest physical addresses.
     * INVVPID instruction takes only a linear address while invept only flushes by EPT
     * not individual addresses.
     *
     * We update the force flag and flush before the next VM-entry, see @bugref{6568}.
     */
    RT_NOREF(GCPhys);
    /** @todo Remove or figure out to way to update the Phys STAT counter.  */
    /* STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbInvlpgPhys); */
    return HMFlushTLBOnAllVCpus(pVM);
}


/**
 * Checks if nested paging is enabled.
 *
 * @returns true if nested paging is active, false otherwise.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Works before hmR3InitFinalizeR0.
 */
VMM_INT_DECL(bool) HMIsNestedPagingActive(PVM pVM)
{
    return HMIsEnabled(pVM) && pVM->hm.s.fNestedPaging;
}


/**
 * Checks if both nested paging and unhampered guest execution are enabled.
 *
 * The almost complete guest execution in hardware is only applicable to VT-x.
 *
 * @returns true if we have both enabled, otherwise false.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Works before hmR3InitFinalizeR0.
 */
VMM_INT_DECL(bool) HMAreNestedPagingAndFullGuestExecEnabled(PVM pVM)
{
    return HMIsEnabled(pVM)
        && pVM->hm.s.fNestedPaging
        && (   pVM->hm.s.vmx.fUnrestrictedGuest
            || pVM->hm.s.svm.fSupported);
}


/**
 * Checks if this VM is using HM and is long-mode capable.
 *
 * Use VMR3IsLongModeAllowed() instead of this, when possible.
 *
 * @returns true if long mode is allowed, false otherwise.
 * @param   pVM         The cross context VM structure.
 * @sa      VMR3IsLongModeAllowed, NEMHCIsLongModeAllowed
 */
VMM_INT_DECL(bool) HMIsLongModeAllowed(PVM pVM)
{
    return HMIsEnabled(pVM) && pVM->hm.s.fAllow64BitGuests;
}


/**
 * Checks if MSR bitmaps are active. It is assumed that when it's available
 * it will be used as well.
 *
 * @returns true if MSR bitmaps are available, false otherwise.
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(bool) HMIsMsrBitmapActive(PVM pVM)
{
    if (HMIsEnabled(pVM))
    {
        if (pVM->hm.s.svm.fSupported)
            return true;

        if (   pVM->hm.s.vmx.fSupported
            && (pVM->hm.s.vmx.Msrs.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_MSR_BITMAPS))
            return true;
    }
    return false;
}


/**
 * Checks if AMD-V is active.
 *
 * @returns true if AMD-V is active.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Works before hmR3InitFinalizeR0.
 */
VMM_INT_DECL(bool) HMIsSvmActive(PVM pVM)
{
    return pVM->hm.s.svm.fSupported && HMIsEnabled(pVM);
}


/**
 * Checks if VT-x is active.
 *
 * @returns true if VT-x is active.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Works before hmR3InitFinalizeR0.
 */
VMM_INT_DECL(bool) HMIsVmxActive(PVM pVM)
{
    return HMIsVmxSupported(pVM) && HMIsEnabled(pVM);
}


/**
 * Checks if VT-x is supported by the host CPU.
 *
 * @returns true if VT-x is supported, false otherwise.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Works before hmR3InitFinalizeR0.
 */
VMM_INT_DECL(bool) HMIsVmxSupported(PVM pVM)
{
    return pVM->hm.s.vmx.fSupported;
}

#endif /* !IN_RC */

/**
 * Checks if an interrupt event is currently pending.
 *
 * @returns Interrupt event pending state.
 * @param   pVM         The cross context VM structure.
 */
VMM_INT_DECL(bool) HMHasPendingIrq(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);
    return !!pVCpu->hm.s.Event.fPending;
}


/**
 * Return the PAE PDPE entries.
 *
 * @returns Pointer to the PAE PDPE array.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMM_INT_DECL(PX86PDPE) HMGetPaePdpes(PVMCPU pVCpu)
{
    return &pVCpu->hm.s.aPdpes[0];
}


/**
 * Checks if the current AMD CPU is subject to erratum 170 "In SVM mode,
 * incorrect code bytes may be fetched after a world-switch".
 *
 * @param   pu32Family      Where to store the CPU family (can be NULL).
 * @param   pu32Model       Where to store the CPU model (can be NULL).
 * @param   pu32Stepping    Where to store the CPU stepping (can be NULL).
 * @returns true if the erratum applies, false otherwise.
 */
VMM_INT_DECL(int) HMAmdIsSubjectToErratum170(uint32_t *pu32Family, uint32_t *pu32Model, uint32_t *pu32Stepping)
{
    /*
     * Erratum 170 which requires a forced TLB flush for each world switch:
     * See AMD spec. "Revision Guide for AMD NPT Family 0Fh Processors".
     *
     * All BH-G1/2 and DH-G1/2 models include a fix:
     * Athlon X2:   0x6b 1/2
     *              0x68 1/2
     * Athlon 64:   0x7f 1
     *              0x6f 2
     * Sempron:     0x7f 1/2
     *              0x6f 2
     *              0x6c 2
     *              0x7c 2
     * Turion 64:   0x68 2
     */
    uint32_t u32Dummy;
    uint32_t u32Version, u32Family, u32Model, u32Stepping, u32BaseFamily;
    ASMCpuId(1, &u32Version, &u32Dummy, &u32Dummy, &u32Dummy);
    u32BaseFamily = (u32Version >> 8) & 0xf;
    u32Family     = u32BaseFamily + (u32BaseFamily == 0xf ? ((u32Version >> 20) & 0x7f) : 0);
    u32Model      = ((u32Version >> 4) & 0xf);
    u32Model      = u32Model | ((u32BaseFamily == 0xf ? (u32Version >> 16) & 0x0f : 0) << 4);
    u32Stepping   = u32Version & 0xf;

    bool fErratumApplies = false;
    if (   u32Family == 0xf
        && !((u32Model == 0x68 || u32Model == 0x6b || u32Model == 0x7f) && u32Stepping >= 1)
        && !((u32Model == 0x6f || u32Model == 0x6c || u32Model == 0x7c) && u32Stepping >= 2))
    {
        fErratumApplies = true;
    }

    if (pu32Family)
        *pu32Family   = u32Family;
    if (pu32Model)
        *pu32Model    = u32Model;
    if (pu32Stepping)
        *pu32Stepping = u32Stepping;

    return fErratumApplies;
}


/**
 * Sets or clears the single instruction flag.
 *
 * When set, HM will try its best to return to ring-3 after executing a single
 * instruction.  This can be used for debugging.  See also
 * EMR3HmSingleInstruction.
 *
 * @returns The old flag state.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   fEnable The new flag state.
 */
VMM_INT_DECL(bool) HMSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    VMCPU_ASSERT_EMT(pVCpu);
    bool fOld = pVCpu->hm.s.fSingleInstruction;
    pVCpu->hm.s.fSingleInstruction = fEnable;
    pVCpu->hm.s.fUseDebugLoop = fEnable || pVM->hm.s.fUseDebugLoop;
    return fOld;
}


/**
 * Notifies HM that GIM provider wants to trap \#UD.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
VMM_INT_DECL(void) HMTrapXcptUDForGIMEnable(PVMCPU pVCpu)
{
    pVCpu->hm.s.fGIMTrapXcptUD = true;
    if (pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fSupported)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS);
    else
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS);
}


/**
 * Notifies HM that GIM provider no longer wants to trap \#UD.
 *
 * @param   pVCpu   The cross context virtual CPU structure.
 */
VMM_INT_DECL(void) HMTrapXcptUDForGIMDisable(PVMCPU pVCpu)
{
    pVCpu->hm.s.fGIMTrapXcptUD = false;
    if (pVCpu->CTX_SUFF(pVM)->hm.s.vmx.fSupported)
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS);
    else
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS);
}


/**
 * VMX nested-guest VM-exit handler.
 *
 * @param   pVCpu              The cross context virtual CPU structure.
 * @param   uBasicExitReason   The basic exit reason.
 */
VMM_INT_DECL(void) HMNstGstVmxVmExit(PVMCPU pVCpu, uint16_t uBasicExitReason)
{
    RT_NOREF2(pVCpu, uBasicExitReason);
}


/**
 * Gets a copy of the VMX host MSRs that were read by HM during ring-0
 * initialization.
 *
 * @return VBox status code.
 * @param   pVM        The cross context VM structure.
 * @param   pVmxMsrs   Where to store the VMXMSRS struct (only valid when
 *                     VINF_SUCCESS is returned).
 *
 * @remarks Caller needs to take care not to call this function too early. Call
 *          after HM initialization is fully complete.
 */
VMM_INT_DECL(int) HMVmxGetHostMsrs(PVM pVM, PVMXMSRS pVmxMsrs)
{
    AssertPtrReturn(pVM,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pVmxMsrs, VERR_INVALID_PARAMETER);
    if (pVM->hm.s.vmx.fSupported)
    {
        *pVmxMsrs = pVM->hm.s.vmx.Msrs;
        return VINF_SUCCESS;
    }
    return VERR_VMX_NOT_SUPPORTED;
}


/**
 * Gets the specified VMX host MSR that was read by HM during ring-0
 * initialization.
 *
 * @return VBox status code.
 * @param   pVM        The cross context VM structure.
 * @param   idMsr      The MSR.
 * @param   puValue    Where to store the MSR value (only updated when VINF_SUCCESS
 *                     is returned).
 *
 * @remarks Caller needs to take care not to call this function too early. Call
 *          after HM initialization is fully complete.
 */
VMM_INT_DECL(int) HMVmxGetHostMsr(PVM pVM, uint32_t idMsr, uint64_t *puValue)
{
    AssertPtrReturn(pVM,     VERR_INVALID_PARAMETER);
    AssertPtrReturn(puValue, VERR_INVALID_PARAMETER);

    if (!pVM->hm.s.vmx.fSupported)
        return VERR_VMX_NOT_SUPPORTED;

    PCVMXMSRS pVmxMsrs = &pVM->hm.s.vmx.Msrs;
    switch (idMsr)
    {
        case MSR_IA32_FEATURE_CONTROL:         *puValue =  pVmxMsrs->u64FeatCtrl;      break;
        case MSR_IA32_VMX_BASIC:               *puValue =  pVmxMsrs->u64Basic;         break;
        case MSR_IA32_VMX_PINBASED_CTLS:       *puValue =  pVmxMsrs->PinCtls.u;        break;
        case MSR_IA32_VMX_PROCBASED_CTLS:      *puValue =  pVmxMsrs->ProcCtls.u;       break;
        case MSR_IA32_VMX_PROCBASED_CTLS2:     *puValue =  pVmxMsrs->ProcCtls2.u;      break;
        case MSR_IA32_VMX_EXIT_CTLS:           *puValue =  pVmxMsrs->ExitCtls.u;       break;
        case MSR_IA32_VMX_ENTRY_CTLS:          *puValue =  pVmxMsrs->EntryCtls.u;      break;
        case MSR_IA32_VMX_TRUE_PINBASED_CTLS:  *puValue =  pVmxMsrs->TruePinCtls.u;    break;
        case MSR_IA32_VMX_TRUE_PROCBASED_CTLS: *puValue =  pVmxMsrs->TrueProcCtls.u;   break;
        case MSR_IA32_VMX_TRUE_ENTRY_CTLS:     *puValue =  pVmxMsrs->TrueEntryCtls.u;  break;
        case MSR_IA32_VMX_TRUE_EXIT_CTLS:      *puValue =  pVmxMsrs->TrueExitCtls.u;   break;
        case MSR_IA32_VMX_MISC:                *puValue =  pVmxMsrs->u64Misc;          break;
        case MSR_IA32_VMX_CR0_FIXED0:          *puValue =  pVmxMsrs->u64Cr0Fixed0;     break;
        case MSR_IA32_VMX_CR0_FIXED1:          *puValue =  pVmxMsrs->u64Cr0Fixed1;     break;
        case MSR_IA32_VMX_CR4_FIXED0:          *puValue =  pVmxMsrs->u64Cr4Fixed0;     break;
        case MSR_IA32_VMX_CR4_FIXED1:          *puValue =  pVmxMsrs->u64Cr4Fixed1;     break;
        case MSR_IA32_VMX_VMCS_ENUM:           *puValue =  pVmxMsrs->u64VmcsEnum;      break;
        case MSR_IA32_VMX_VMFUNC:              *puValue =  pVmxMsrs->u64VmFunc;        break;
        case MSR_IA32_VMX_EPT_VPID_CAP:        *puValue =  pVmxMsrs->u64EptVpidCaps;   break;
        default:
        {
            AssertMsgFailed(("Invalid MSR %#x\n", idMsr));
            return VERR_NOT_FOUND;
        }
    }
    return VINF_SUCCESS;
}


#ifndef IN_RC
/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 *
 * This is called by PGM.
 *
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   enmShadowMode   New shadow paging mode.
 * @param   enmGuestMode    New guest paging mode.
 */
VMM_INT_DECL(void) HMHCPagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode)
{
# ifdef IN_RING3
    /* Ignore page mode changes during state loading. */
    if (VMR3GetState(pVM) == VMSTATE_LOADING)
        return;
# endif

    pVCpu->hm.s.enmShadowMode = enmShadowMode;

    /*
     * If the guest left protected mode VMX execution, we'll have to be
     * extra careful if/when the guest switches back to protected mode.
     */
    if (enmGuestMode == PGMMODE_REAL)
        pVCpu->hm.s.vmx.fWasInRealMode = true;

# ifdef IN_RING0
    /*
     * We need to tickle SVM and VT-x state updates.
     *
     * Note! We could probably reduce this depending on what exactly changed.
     */
    if (VM_IS_HM_ENABLED(pVM))
    {
        CPUM_ASSERT_NOT_EXTRN(pVCpu, CPUMCTX_EXTRN_CR0 | CPUMCTX_EXTRN_CR3 | CPUMCTX_EXTRN_CR4 | CPUMCTX_EXTRN_EFER); /* No recursion! */
        uint64_t fChanged = HM_CHANGED_GUEST_CR0 | HM_CHANGED_GUEST_CR3 | HM_CHANGED_GUEST_CR4 | HM_CHANGED_GUEST_EFER_MSR;
        if (pVM->hm.s.svm.fSupported)
            fChanged |= HM_CHANGED_SVM_GUEST_XCPT_INTERCEPTS;
        else
            fChanged |= HM_CHANGED_VMX_GUEST_XCPT_INTERCEPTS | HM_CHANGED_VMX_ENTRY_CTLS | HM_CHANGED_VMX_EXIT_CTLS;
        ASMAtomicUoOrU64(&pVCpu->hm.s.fCtxChanged, fChanged);
    }
# endif

    Log4(("HMHCPagingModeChanged: Guest paging mode '%s', shadow paging mode '%s'\n", PGMGetModeName(enmGuestMode),
          PGMGetModeName(enmShadowMode)));
}
#endif /* !IN_RC */

