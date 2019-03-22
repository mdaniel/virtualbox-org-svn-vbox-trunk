/** @file
 * HM - Intel/AMD VM Hardware Assisted Virtualization Manager (VMM)
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_hm_h
#define ___VBox_vmm_hm_h

#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/trpm.h>
#include <iprt/mp.h>


/** @defgroup grp_hm      The Hardware Assisted Virtualization Manager API
 * @ingroup grp_vmm
 * @{
 */

RT_C_DECLS_BEGIN

/**
 * Checks whether HM (VT-x/AMD-V) is being used by this VM.
 *
 * @retval  true if used.
 * @retval  false if software virtualization (raw-mode) or NEM is used.
 *
 * @param   a_pVM       The cross context VM structure.
 * @deprecated Please use VM_IS_RAW_MODE_ENABLED, VM_IS_HM_OR_NEM_ENABLED, or
 *             VM_IS_HM_ENABLED instead.
 * @internal
 */
#if defined(VBOX_STRICT) && defined(IN_RING3)
# define HMIsEnabled(a_pVM)                 HMIsEnabledNotMacro(a_pVM)
#else
# define HMIsEnabled(a_pVM)                 ((a_pVM)->fHMEnabled)
#endif

/**
 * Checks whether raw-mode context is required for HM purposes
 *
 * @retval  true if required by HM for doing switching the cpu to 64-bit mode.
 * @retval  false if not required by HM.
 *
 * @param   a_pVM       The cross context VM structure.
 * @internal
 */
#if HC_ARCH_BITS == 64
# define HMIsRawModeCtxNeeded(a_pVM)        (false)
#else
# define HMIsRawModeCtxNeeded(a_pVM)        ((a_pVM)->fHMNeedRawModeCtx)
#endif

/**
 * Checks whether we're in the special hardware virtualization context.
 * @returns true / false.
 * @param   a_pVCpu     The caller's cross context virtual CPU structure.
 * @thread  EMT
 */
#ifdef IN_RING0
# define HMIsInHwVirtCtx(a_pVCpu)           (VMCPU_GET_STATE(a_pVCpu) == VMCPUSTATE_STARTED_HM)
#else
# define HMIsInHwVirtCtx(a_pVCpu)           (false)
#endif

/**
 * Checks whether we're in the special hardware virtualization context and we
 * cannot perform long jump without guru meditating and possibly messing up the
 * host and/or guest state.
 *
 * This is after we've turned interrupts off and such.
 *
 * @returns true / false.
 * @param   a_pVCpu     The caller's cross context virtual CPU structure.
 * @thread  EMT
 */
#ifdef IN_RING0
# define HMIsInHwVirtNoLongJmpCtx(a_pVCpu)  (VMCPU_GET_STATE(a_pVCpu) == VMCPUSTATE_STARTED_EXEC)
#else
# define HMIsInHwVirtNoLongJmpCtx(a_pVCpu)  (false)
#endif

/**
 * 64-bit raw-mode (intermediate memory context) operations.
 *
 * These are special hypervisor eip values used when running 64-bit guests on
 * 32-bit hosts. Each operation corresponds to a routine.
 *
 * @note Duplicated in the assembly code!
 */
typedef enum HM64ON32OP
{
    HM64ON32OP_INVALID = 0,
    HM64ON32OP_VMXRCStartVM64,
    HM64ON32OP_SVMRCVMRun64,
    HM64ON32OP_HMRCSaveGuestFPU64,
    HM64ON32OP_HMRCSaveGuestDebug64,
    HM64ON32OP_HMRCTestSwitcher64,
    HM64ON32OP_END,
    HM64ON32OP_32BIT_HACK = 0x7fffffff
} HM64ON32OP;

/** @name All-context HM API.
 * @{ */
VMMDECL(bool)                   HMIsEnabledNotMacro(PVM pVM);
VMMDECL(bool)                   HMCanExecuteGuest(PVMCPU pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(int)               HMInvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMM_INT_DECL(bool)              HMHasPendingIrq(PVM pVM);
VMM_INT_DECL(PX86PDPE)          HMGetPaePdpes(PVMCPU pVCpu);
VMM_INT_DECL(bool)              HMSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable);
VMM_INT_DECL(bool)              HMIsSvmActive(PVM pVM);
VMM_INT_DECL(bool)              HMIsVmxActive(PVM pVM);
VMM_INT_DECL(bool)              HMIsVmxSupported(PVM pVM);
VMM_INT_DECL(const char *)      HMVmxGetDiagDesc(VMXVDIAG enmDiag);
VMM_INT_DECL(const char *)      HMVmxGetAbortDesc(VMXABORT enmAbort);
VMM_INT_DECL(void)              HMHCPagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode);
/** @} */

/** @name All-context VMX helpers.
 * These are VMX functions (based on VMX specs.) that may be used by IEM/REM and
 * not VirtualBox functions that are used for hardware-assisted VMX. Those are
 * declared below under the !IN_RC section.
 * @{ */
VMM_INT_DECL(int)               HMVmxGetHostMsrs(PVM pVM, PVMXMSRS pVmxMsrs);
VMM_INT_DECL(int)               HMVmxGetHostMsr(PVM pVM, uint32_t idMsr, uint64_t *puValue);
VMM_INT_DECL(bool)              HMVmxCanExecuteGuest(PVMCPU pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(int)               HMVmxEntryIntInfoInjectTrpmEvent(PVMCPU pVCpu, uint32_t uEntryIntInfo, uint32_t uErrCode,
                                                                 uint32_t cbInstr, RTGCUINTPTR GCPtrFaultAddress);
VMM_INT_DECL(int)               HMVmxGetMsrPermission(void *pvMsrBitmap, uint32_t idMsr, PVMXMSREXITREAD penmRead,
                                                      PVMXMSREXITWRITE penmWrite);
/** @} */

/** @name All-context SVM helpers.
 *
 * These are SVM functions (based on AMD specs.) that may be used by IEM/REM and
 * not VirtualBox functions that are used for hardware-assisted SVM. Those are
 * declared below under the !IN_RC section.
 * @{ */
VMM_INT_DECL(TRPMEVENT)         HMSvmEventToTrpmEventType(PCSVMEVENT pSvmEvent);
VMM_INT_DECL(int)               HMSvmGetMsrpmOffsetAndBit(uint32_t idMsr, uint16_t *pbOffMsrpm, uint8_t *puMsrpmBit);
VMM_INT_DECL(bool)              HMSvmIsIOInterceptActive(void *pvIoBitmap, uint16_t u16Port, SVMIOIOTYPE enmIoType, uint8_t cbReg,
                                                         uint8_t cAddrSizeBits, uint8_t iEffSeg, bool fRep, bool fStrIo,
                                                         PSVMIOIOEXITINFO pIoExitInfo);
/** @} */

#ifndef IN_RC

/** @name R0, R3 HM (VMX/SVM agnostic) handlers.
 * @{ */
VMM_INT_DECL(int)               HMFlushTLB(PVMCPU pVCpu);
VMM_INT_DECL(int)               HMFlushTLBOnAllVCpus(PVM pVM);
VMM_INT_DECL(int)               HMInvalidatePageOnAllVCpus(PVM pVM, RTGCPTR GCVirt);
VMM_INT_DECL(int)               HMInvalidatePhysPage(PVM pVM, RTGCPHYS GCPhys);
VMM_INT_DECL(bool)              HMAreNestedPagingAndFullGuestExecEnabled(PVM pVM);
VMM_INT_DECL(bool)              HMIsLongModeAllowed(PVM pVM);
VMM_INT_DECL(bool)              HMIsNestedPagingActive(PVM pVM);
VMM_INT_DECL(bool)              HMIsMsrBitmapActive(PVM pVM);
/** @} */

/** @name R0, R3 SVM handlers.
 * @{ */
VMM_INT_DECL(bool)              HMSvmIsVGifActive(PVM pVM);
VMM_INT_DECL(uint64_t)          HMSvmNstGstApplyTscOffset(PVMCPU pVCpu, uint64_t uTicks);
# ifdef VBOX_WITH_NESTED_HWVIRT_SVM
VMM_INT_DECL(void)              HMSvmNstGstVmExitNotify(PVMCPU pVCpu, PCPUMCTX pCtx);
# endif
VMM_INT_DECL(int)               HMSvmIsSubjectToErratum170(uint32_t *pu32Family, uint32_t *pu32Model, uint32_t *pu32Stepping);
VMM_INT_DECL(int)               HMHCSvmMaybeMovTprHypercall(PVMCPU pVCpu);
/** @} */

#else /* Nops in RC: */

/** @name RC HM (VMX/SVM agnostic) handlers.
 * @{ */
# define HMFlushTLB(pVCpu)                                            do { } while (0)
# define HMFlushTLBOnAllVCpus(pVM)                                    do { } while (0)
# define HMInvalidatePageOnAllVCpus(pVM, GCVirt)                      do { } while (0)
# define HMInvalidatePhysPage(pVM,  GCVirt)                           do { } while (0)
# define HMAreNestedPagingAndFullGuestExecEnabled(pVM)                false
# define HMIsLongModeAllowed(pVM)                                     false
# define HMIsNestedPagingActive(pVM)                                  false
# define HMIsMsrBitmapsActive(pVM)                                    false
/** @} */

/** @name RC SVM handlers.
 * @{ */
# define HMSvmIsVGifActive(pVM)                                       false
# define HMSvmNstGstApplyTscOffset(pVCpu, uTicks)                     (uTicks)
# define HMSvmNstGstVmExitNotify(pVCpu, pCtx)                         do { } while (0)
# define HMSvmIsSubjectToErratum170(puFamily, puModel, puStepping)    false
# define HMHCSvmMaybeMovTprHypercall(pVCpu)                           do { } while (0)
/** @} */

#endif

#ifdef IN_RING0
/** @defgroup grp_hm_r0    The HM ring-0 Context API
 * @{
 */
VMMR0_INT_DECL(int)             HMR0Init(void);
VMMR0_INT_DECL(int)             HMR0Term(void);
VMMR0_INT_DECL(int)             HMR0InitVM(PVM pVM);
VMMR0_INT_DECL(int)             HMR0TermVM(PVM pVM);
VMMR0_INT_DECL(int)             HMR0EnableAllCpus(PVM pVM);
# ifdef VBOX_WITH_RAW_MODE
VMMR0_INT_DECL(int)             HMR0EnterSwitcher(PVM pVM, VMMSWITCHER enmSwitcher, bool *pfVTxDisabled);
VMMR0_INT_DECL(void)            HMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled);
# endif

VMMR0_INT_DECL(int)             HMR0SetupVM(PVM pVM);
VMMR0_INT_DECL(int)             HMR0RunGuestCode(PVM pVM, PVMCPU pVCpu);
VMMR0_INT_DECL(int)             HMR0Enter(PVMCPU pVCpu);
VMMR0_INT_DECL(int)             HMR0LeaveCpu(PVMCPU pVCpu);
VMMR0_INT_DECL(void)            HMR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, void *pvUser);
VMMR0_INT_DECL(void)            HMR0NotifyCpumUnloadedGuestFpuState(PVMCPU VCpu);
VMMR0_INT_DECL(void)            HMR0NotifyCpumModifiedHostCr0(PVMCPU VCpu);
VMMR0_INT_DECL(bool)            HMR0SuspendPending(void);
VMMR0_INT_DECL(int)             HMR0InvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMMR0_INT_DECL(int)             HMR0ImportStateOnDemand(PVMCPU pVCpu, uint64_t fWhat);

# if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
VMMR0_INT_DECL(int)             HMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0_INT_DECL(int)             HMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0_INT_DECL(int)             HMR0TestSwitcher3264(PVM pVM);
# endif

/** @} */
#endif /* IN_RING0 */


#ifdef IN_RING3
/** @defgroup grp_hm_r3    The HM ring-3 Context API
 * @{
 */
VMMR3DECL(bool)                 HMR3IsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsNestedPagingActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVirtApicRegsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsPostedIntrsEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVpidActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsUXActive(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsSvmEnabled(PUVM pUVM);
VMMR3DECL(bool)                 HMR3IsVmxEnabled(PUVM pUVM);

VMMR3_INT_DECL(bool)            HMR3IsEventPending(PVMCPU pVCpu);
VMMR3_INT_DECL(int)             HMR3Init(PVM pVM);
VMMR3_INT_DECL(int)             HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3_INT_DECL(void)            HMR3Relocate(PVM pVM);
VMMR3_INT_DECL(int)             HMR3Term(PVM pVM);
VMMR3_INT_DECL(void)            HMR3Reset(PVM pVM);
VMMR3_INT_DECL(void)            HMR3ResetCpu(PVMCPU pVCpu);
VMMR3_INT_DECL(void)            HMR3CheckError(PVM pVM, int iStatusCode);
VMMR3_INT_DECL(void)            HMR3NotifyDebugEventChanged(PVM pVM);
VMMR3_INT_DECL(void)            HMR3NotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(bool)            HMR3IsActive(PVMCPU pVCpu);
VMMR3_INT_DECL(int)             HMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3_INT_DECL(int)             HMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem);
VMMR3_INT_DECL(int)             HMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(bool)            HMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx);
VMMR3_INT_DECL(bool)            HMR3IsVmxPreemptionTimerUsed(PVM pVM);
VMMR3DECL(const char *)         HMR3GetVmxExitName(uint32_t uExit);
VMMR3DECL(const char *)         HMR3GetSvmExitName(uint32_t uExit);
/** @} */
#endif /* IN_RING3 */

/** @} */
RT_C_DECLS_END


#endif

