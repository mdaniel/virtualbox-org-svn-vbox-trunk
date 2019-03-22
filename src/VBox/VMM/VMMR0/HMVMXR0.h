/* $Id$ */
/** @file
 * HM VMX (VT-x) - Internal header file.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_VMMR0_HMVMXR0_h
#define VMM_INCLUDED_SRC_VMMR0_HMVMXR0_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

RT_C_DECLS_BEGIN

/** @defgroup grp_vmx_int   Internal
 * @ingroup grp_vmx
 * @internal
 * @{
 */

#ifdef IN_RING0

VMMR0DECL(int)          VMXR0Enter(PVMCPU pVCpu);
VMMR0DECL(void)         VMXR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, PVMCPU pVCpu, bool fGlobalInit);
VMMR0DECL(int)          VMXR0EnableCpu(PHMPHYSCPU pHostCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys,
                                       bool fEnabledBySystem, PCSUPHWVIRTMSRS pHwvirtMsrs);
VMMR0DECL(int)          VMXR0DisableCpu(void *pvPageCpu, RTHCPHYS pPageCpuPhys);
VMMR0DECL(int)          VMXR0GlobalInit(void);
VMMR0DECL(void)         VMXR0GlobalTerm(void);
VMMR0DECL(int)          VMXR0InitVM(PVM pVM);
VMMR0DECL(int)          VMXR0TermVM(PVM pVM);
VMMR0DECL(int)          VMXR0SetupVM(PVM pVM);
VMMR0DECL(int)          VMXR0ExportHostState(PVMCPU pVCpu);
VMMR0DECL(int)          VMXR0InvalidatePage(PVMCPU pVCpu, RTGCPTR GCVirt);
VMMR0DECL(int)          VMXR0ImportStateOnDemand(PVMCPU pVCpu, uint64_t fWhat);
VMMR0DECL(VBOXSTRICTRC) VMXR0RunGuestCode(PVMCPU pVCpu);
DECLASM(int)            VMXR0StartVM32(RTHCUINT fResume, PCPUMCTX pCtx, PVMXVMCSBATCHCACHE pVmcsCache, PVM pVM, PVMCPU pVCpu);
DECLASM(int)            VMXR0StartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMXVMCSBATCHCACHE pVmcsCache, PVM pVM, PVMCPU pVCpu);

# if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS)
DECLASM(int)            VMXR0SwitcherStartVM64(RTHCUINT fResume, PCPUMCTX pCtx, PVMXVMCSBATCHCACHE pVmcsCache, PVM pVM, PVMCPU pVCpu);
VMMR0DECL(int)          VMXR0Execute64BitsHandler(PVMCPU pVCpu, HM64ON32OP enmOp, uint32_t cbParam, uint32_t *paParam);
# endif

/* Cached VMCS accesses -- defined only for 32-bit hosts (with 64-bit guest support). */
# ifdef VMX_USE_CACHED_VMCS_ACCESSES
VMMR0DECL(int) VMXWriteCachedVmcsEx(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val);

DECLINLINE(int) VMXReadCachedVmcsEx(PVMCPU pVCpu, uint32_t idxCache, RTGCUINTREG *pVal)
{
    Assert(idxCache <= VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX);
    *pVal = pVCpu->hm.s.vmx.VmcsBatchCache.Read.aFieldVal[idxCache];
    return VINF_SUCCESS;
}
# endif

# if HC_ARCH_BITS == 32
#  define VMXReadVmcsHstN                                 VMXReadVmcs32
#  define VMXReadVmcsGstN(idxField, pVal)                 VMXReadCachedVmcsEx(pVCpu, idxField##_CACHE_IDX, pVal)
#  define VMXReadVmcsGstNByIdxVal(idxField, pVal)         VMXReadCachedVmcsEx(pVCpu, idxField, pVal)
# else /* HC_ARCH_BITS == 64 */
#  define VMXReadVmcsHstN                                 VMXReadVmcs64
#  define VMXReadVmcsGstN                                 VMXReadVmcs64
#  define VMXReadVmcsGstNByIdxVal                         VMXReadVmcs64
# endif

#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_VMMR0_HMVMXR0_h */

