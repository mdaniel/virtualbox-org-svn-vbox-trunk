/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_EM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/mm.h>
#if defined(VBOX_VMM_TARGET_ARMV8)
# include "IEMInternal-armv8.h"
#else
# include "IEMInternal.h"
#endif
#include <VBox/vmm/vm.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#ifdef VBOX_WITH_DEBUGGER
# include <VBox/dbg.h>
#endif

#include <iprt/assert.h>
#include <iprt/getopt.h>
#include <iprt/string.h>

#if defined(VBOX_WITH_IEM_RECOMPILER) && !defined(VBOX_VMM_TARGET_ARMV8)
# include "IEMN8veRecompiler.h"
# include "IEMThreadedFunctions.h"
# include "IEMInline.h"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static FNDBGFINFOARGVINT iemR3InfoITlb;
static FNDBGFINFOARGVINT iemR3InfoDTlb;
#if defined(VBOX_WITH_IEM_RECOMPILER) && !defined(VBOX_VMM_TARGET_ARMV8)
static FNDBGFINFOARGVINT iemR3InfoTb;
#endif
#ifdef VBOX_WITH_DEBUGGER
static void iemR3RegisterDebuggerCommands(void);
#endif


#if !defined(VBOX_VMM_TARGET_ARMV8)
static const char *iemGetTargetCpuName(uint32_t enmTargetCpu)
{
    switch (enmTargetCpu)
    {
#define CASE_RET_STR(enmValue) case enmValue: return #enmValue + (sizeof("IEMTARGETCPU_") - 1)
        CASE_RET_STR(IEMTARGETCPU_8086);
        CASE_RET_STR(IEMTARGETCPU_V20);
        CASE_RET_STR(IEMTARGETCPU_186);
        CASE_RET_STR(IEMTARGETCPU_286);
        CASE_RET_STR(IEMTARGETCPU_386);
        CASE_RET_STR(IEMTARGETCPU_486);
        CASE_RET_STR(IEMTARGETCPU_PENTIUM);
        CASE_RET_STR(IEMTARGETCPU_PPRO);
        CASE_RET_STR(IEMTARGETCPU_CURRENT);
#undef CASE_RET_STR
        default: return "Unknown";
    }
}
#endif


/**
 * Initializes the interpreted execution manager.
 *
 * This must be called after CPUM as we're quering information from CPUM about
 * the guest and host CPUs.
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 */
VMMR3DECL(int)      IEMR3Init(PVM pVM)
{
    /*
     * Read configuration.
     */
#if (!defined(VBOX_VMM_TARGET_ARMV8) && !defined(VBOX_WITHOUT_CPUID_HOST_CALL)) || defined(VBOX_WITH_IEM_RECOMPILER)
    PCFGMNODE const pIem = CFGMR3GetChild(CFGMR3GetRoot(pVM), "IEM");
    int rc;
#endif

#if !defined(VBOX_VMM_TARGET_ARMV8) && !defined(VBOX_WITHOUT_CPUID_HOST_CALL)
    /** @cfgm{/IEM/CpuIdHostCall, boolean, false}
     * Controls whether the custom VBox specific CPUID host call interface is
     * enabled or not. */
# ifdef DEBUG_bird
    rc = CFGMR3QueryBoolDef(pIem, "CpuIdHostCall", &pVM->iem.s.fCpuIdHostCall, true);
# else
    rc = CFGMR3QueryBoolDef(pIem, "CpuIdHostCall", &pVM->iem.s.fCpuIdHostCall, false);
# endif
    AssertLogRelRCReturn(rc, rc);
#endif

#ifdef VBOX_WITH_IEM_RECOMPILER
    /** @cfgm{/IEM/MaxTbCount, uint32_t, 524288}
     * Max number of TBs per EMT. */
    uint32_t cMaxTbs = 0;
    rc = CFGMR3QueryU32Def(pIem, "MaxTbCount", &cMaxTbs, _512K);
    AssertLogRelRCReturn(rc, rc);
    if (cMaxTbs < _16K || cMaxTbs > _8M)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "MaxTbCount value %u (%#x) is out of range (min %u, max %u)", cMaxTbs, cMaxTbs, _16K, _8M);

    /** @cfgm{/IEM/InitialTbCount, uint32_t, 32678}
     * Initial (minimum) number of TBs per EMT in ring-3. */
    uint32_t cInitialTbs = 0;
    rc = CFGMR3QueryU32Def(pIem, "InitialTbCount", &cInitialTbs, RT_MIN(cMaxTbs, _32K));
    AssertLogRelRCReturn(rc, rc);
    if (cInitialTbs < _16K || cInitialTbs > _8M)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialTbCount value %u (%#x) is out of range (min %u, max %u)", cInitialTbs, cInitialTbs, _16K, _8M);

    /* Check that the two values makes sense together. Expect user/api to do
       the right thing or get lost. */
    if (cInitialTbs > cMaxTbs)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialTbCount value %u (%#x) is higher than the MaxTbCount value %u (%#x)",
                          cInitialTbs, cInitialTbs, cMaxTbs, cMaxTbs);

    /** @cfgm{/IEM/MaxExecMem, uint64_t, 512 MiB}
     * Max executable memory for recompiled code per EMT. */
    uint64_t cbMaxExec = 0;
    rc = CFGMR3QueryU64Def(pIem, "MaxExecMem", &cbMaxExec, _512M);
    AssertLogRelRCReturn(rc, rc);
    if (cbMaxExec < _1M || cbMaxExec > 16*_1G64)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "MaxExecMem value %'RU64 (%#RX64) is out of range (min %'RU64, max %'RU64)",
                          cbMaxExec, cbMaxExec, (uint64_t)_1M, 16*_1G64);

    /** @cfgm{/IEM/ExecChunkSize, uint32_t, 0 (auto)}
     * The executable memory allocator chunk size. */
    uint32_t cbChunkExec = 0;
    rc = CFGMR3QueryU32Def(pIem, "ExecChunkSize", &cbChunkExec, 0);
    AssertLogRelRCReturn(rc, rc);
    if (cbChunkExec != 0 && cbChunkExec != UINT32_MAX && (cbChunkExec < _1M || cbChunkExec > _256M))
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "ExecChunkSize value %'RU32 (%#RX32) is out of range (min %'RU32, max %'RU32)",
                          cbChunkExec, cbChunkExec, _1M, _256M);

    /** @cfgm{/IEM/InitialExecMemSize, uint64_t, 1}
     * The initial executable memory allocator size (per EMT).  The value is
     * rounded up to the nearest chunk size, so 1 byte means one chunk. */
    uint64_t cbInitialExec = 0;
    rc = CFGMR3QueryU64Def(pIem, "InitialExecMemSize", &cbInitialExec, 0);
    AssertLogRelRCReturn(rc, rc);
    if (cbInitialExec > cbMaxExec)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialExecMemSize value %'RU64 (%#RX64) is out of range (max %'RU64)",
                          cbInitialExec, cbInitialExec, cbMaxExec);

#endif /* VBOX_WITH_IEM_RECOMPILER*/

    /*
     * Initialize per-CPU data and register statistics.
     */
    uint64_t const uInitialTlbRevision = UINT64_C(0) - (IEMTLB_REVISION_INCR * 200U);
    uint64_t const uInitialTlbPhysRev  = UINT64_C(0) - (IEMTLB_PHYS_REV_INCR * 100U);

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];
        AssertCompile(sizeof(pVCpu->iem.s) <= sizeof(pVCpu->iem.padding)); /* (tstVMStruct can't do it's job w/o instruction stats) */

        pVCpu->iem.s.CodeTlb.uTlbRevision = pVCpu->iem.s.DataTlb.uTlbRevision = uInitialTlbRevision;
        pVCpu->iem.s.CodeTlb.uTlbPhysRev  = pVCpu->iem.s.DataTlb.uTlbPhysRev  = uInitialTlbPhysRev;

        /*
         * Host and guest CPU information.
         */
        if (idCpu == 0)
        {
            pVCpu->iem.s.enmCpuVendor                     = CPUMGetGuestCpuVendor(pVM);
            pVCpu->iem.s.enmHostCpuVendor                 = CPUMGetHostCpuVendor(pVM);
#if !defined(VBOX_VMM_TARGET_ARMV8)
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]       =    pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL
                                                            || pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_VIA /*??*/
                                                          ? IEMTARGETCPU_EFL_BEHAVIOR_INTEL : IEMTARGETCPU_EFL_BEHAVIOR_AMD;
# if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
            if (pVCpu->iem.s.enmCpuVendor == pVCpu->iem.s.enmHostCpuVendor)
                pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = IEMTARGETCPU_EFL_BEHAVIOR_NATIVE;
            else
# endif
                pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = pVCpu->iem.s.aidxTargetCpuEflFlavour[0];
#else
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]   = IEMTARGETCPU_EFL_BEHAVIOR_NATIVE;
            pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = pVCpu->iem.s.aidxTargetCpuEflFlavour[0];
#endif

#if !defined(VBOX_VMM_TARGET_ARMV8) && (IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC)
            switch (pVM->cpum.ro.GuestFeatures.enmMicroarch)
            {
                case kCpumMicroarch_Intel_8086:     pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_8086; break;
                case kCpumMicroarch_Intel_80186:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_186; break;
                case kCpumMicroarch_Intel_80286:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_286; break;
                case kCpumMicroarch_Intel_80386:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_386; break;
                case kCpumMicroarch_Intel_80486:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_486; break;
                case kCpumMicroarch_Intel_P5:       pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_PENTIUM; break;
                case kCpumMicroarch_Intel_P6:       pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_PPRO; break;
                case kCpumMicroarch_NEC_V20:        pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_V20; break;
                case kCpumMicroarch_NEC_V30:        pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_V20; break;
                default:                            pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_CURRENT; break;
            }
            LogRel(("IEM: TargetCpu=%s, Microarch=%s aidxTargetCpuEflFlavour={%d,%d}\n",
                    iemGetTargetCpuName(pVCpu->iem.s.uTargetCpu), CPUMMicroarchName(pVM->cpum.ro.GuestFeatures.enmMicroarch),
                    pVCpu->iem.s.aidxTargetCpuEflFlavour[0], pVCpu->iem.s.aidxTargetCpuEflFlavour[1]));
#else
            LogRel(("IEM: Microarch=%s aidxTargetCpuEflFlavour={%d,%d}\n",
                    CPUMMicroarchName(pVM->cpum.ro.GuestFeatures.enmMicroarch),
                    pVCpu->iem.s.aidxTargetCpuEflFlavour[0], pVCpu->iem.s.aidxTargetCpuEflFlavour[1]));
#endif
        }
        else
        {
            pVCpu->iem.s.enmCpuVendor                     = pVM->apCpusR3[0]->iem.s.enmCpuVendor;
            pVCpu->iem.s.enmHostCpuVendor                 = pVM->apCpusR3[0]->iem.s.enmHostCpuVendor;
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]       = pVM->apCpusR3[0]->iem.s.aidxTargetCpuEflFlavour[0];
            pVCpu->iem.s.aidxTargetCpuEflFlavour[1]       = pVM->apCpusR3[0]->iem.s.aidxTargetCpuEflFlavour[1];
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
            pVCpu->iem.s.uTargetCpu                       = pVM->apCpusR3[0]->iem.s.uTargetCpu;
#endif
        }

        /*
         * Mark all buffers free.
         */
        uint32_t iMemMap = RT_ELEMENTS(pVCpu->iem.s.aMemMappings);
        while (iMemMap-- > 0)
            pVCpu->iem.s.aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    }


#ifdef VBOX_WITH_IEM_RECOMPILER
    /*
     * Initialize the TB allocator and cache (/ hash table).
     *
     * This is done by each EMT to try get more optimal thread/numa locality of
     * the allocations.
     */
    rc = VMR3ReqCallWait(pVM, VMCPUID_ALL, (PFNRT)iemTbInit, 6,
                         pVM, cInitialTbs, cMaxTbs, cbInitialExec, cbMaxExec, cbChunkExec);
    AssertLogRelRCReturn(rc, rc);
#endif

    /*
     * Register statistics.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
#if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX) /* quick fix for stupid structure duplication non-sense */
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        STAMR3RegisterF(pVM, &pVCpu->iem.s.cInstructions,               STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Instructions interpreted",                     "/IEM/CPU%u/cInstructions", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cLongJumps,                  STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Number of longjmp calls",                      "/IEM/CPU%u/cLongJumps", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPotentialExits,             STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Potential exits",                              "/IEM/CPU%u/cPotentialExits", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetAspectNotImplemented,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_ASPECT_NOT_IMPLEMENTED",              "/IEM/CPU%u/cRetAspectNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInstrNotImplemented,     STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_INSTR_NOT_IMPLEMENTED",               "/IEM/CPU%u/cRetInstrNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInfStatuses,             STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Informational statuses returned",              "/IEM/CPU%u/cRetInfStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetErrStatuses,             STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Error statuses returned",                      "/IEM/CPU%u/cRetErrStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cbWritten,                   STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Approx bytes written",                         "/IEM/CPU%u/cbWritten", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPendingCommit,              STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Times RC/R0 had to postpone instruction committing to ring-3", "/IEM/CPU%u/cPendingCommit", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cMisalignedAtomics,          STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Number of misaligned (for the host) atomic instructions", "/IEM/CPU%u/cMisalignedAtomics", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbMisses,          STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB misses",                          "/IEM/CPU%u/CodeTlb-Misses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.uTlbRevision,        STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Code TLB revision",                        "/IEM/CPU%u/CodeTlb-Revision", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.CodeTlb.uTlbPhysRev, STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Code TLB physical revision",               "/IEM/CPU%u/CodeTlb-PhysRev", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbSlowReadPath,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB slow read path",                  "/IEM/CPU%u/CodeTlb-SlowReads", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbMisses,          STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB misses",                          "/IEM/CPU%u/DataTlb-Misses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbSafeReadPath,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB safe read path",                  "/IEM/CPU%u/DataTlb-SafeReads", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbSafeWritePath,   STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB safe write path",                 "/IEM/CPU%u/DataTlb-SafeWrites", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.uTlbRevision,        STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Data TLB revision",                        "/IEM/CPU%u/DataTlb-Revision", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.DataTlb.uTlbPhysRev, STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Data TLB physical revision",               "/IEM/CPU%u/DataTlb-PhysRev", idCpu);

# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbHits,            STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB hits",                            "/IEM/CPU%u/CodeTlb-Hits", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbHits,            STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB hits",                            "/IEM/CPU%u/DataTlb-Hits-Other", idCpu);
#  ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeTlbHitsForStack, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB native stack access hits",        "/IEM/CPU%u/DataTlb-Hits-Native-Stack", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeTlbHitsForFetch, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB native data fetch hits",          "/IEM/CPU%u/DataTlb-Hits-Native-Fetch", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeTlbHitsForStore, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB native data store hits",          "/IEM/CPU%u/DataTlb-Hits-Native-Store", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeTlbHitsForMapped, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB native mapped data hits",         "/IEM/CPU%u/DataTlb-Hits-Native-Mapped", idCpu);
#  endif
        char szPat[128];
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/DataTlb-Hits-*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat,
                          "Data TLB hits total",                    "/IEM/CPU%u/DataTlb-Hits", idCpu);

        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/DataTlb-Safe*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat,
                          "Data TLB actual misses",                 "/IEM/CPU%u/DataTlb-SafeTotal", idCpu);
        char szVal[128];
        RTStrPrintf(szVal, sizeof(szVal), "/IEM/CPU%u/DataTlb-SafeTotal", idCpu);
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/DataTlb-Hits-*", idCpu);
        STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PPM, szVal, true, szPat,
                               "Data TLB actual miss rate",         "/IEM/CPU%u/DataTlb-SafeRate", idCpu);

#  ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCodeTlbMissesNewPage, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB native misses on new page",           "/IEM/CPU%u/CodeTlb-Misses-New-Page", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCodeTlbMissesNewPageWithOffset, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB native misses on new page w/ offset", "/IEM/CPU%u/CodeTlb-Misses-New-Page-With-Offset", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCodeTlbHitsForNewPage, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB native hits on new page",   "/IEM/CPU%u/CodeTlb-Hits-New-Page", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCodeTlbHitsForNewPageWithOffset, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB native hits on new page /w offset",   "/IEM/CPU%u/CodeTlb-Hits-New-Page-With-Offset", idCpu);
#  endif
# endif

#ifdef VBOX_WITH_IEM_RECOMPILER
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.cTbExecNative,       STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Executed native translation block",            "/IEM/CPU%u/re/cTbExecNative", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.cTbExecThreaded,     STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Executed threaded translation block",          "/IEM/CPU%u/re/cTbExecThreaded", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbExecBreaks,    STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Times TB execution was interrupted/broken off", "/IEM/CPU%u/re/cTbExecBreaks", idCpu);

        PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatAllocs,         STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS,
                        "Translation block allocations",                "/IEM/CPU%u/re/cTbAllocCalls", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatFrees,          STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS,
                        "Translation block frees",                      "/IEM/CPU%u/re/cTbFreeCalls", idCpu);
# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatPrune,          STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Time spent freeing up TBs when full at alloc", "/IEM/CPU%u/re/TbPruningAlloc", idCpu);
# endif
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatPruneNative,    STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Time spent freeing up native TBs when out of executable memory", "/IEM/CPU%u/re/TbPruningNative", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cAllocatedChunks,   STAMTYPE_U16,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Populated TB chunks",                          "/IEM/CPU%u/re/cTbChunks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cMaxChunks,         STAMTYPE_U8,    STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Max number of TB chunks",                      "/IEM/CPU%u/re/cTbChunksMax", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cTotalTbs,          STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Total number of TBs in the allocator",         "/IEM/CPU%u/re/cTbTotal", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cMaxTbs,            STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Max total number of TBs allowed",              "/IEM/CPU%u/re/cTbTotalMax", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cInUseTbs,          STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated TBs",            "/IEM/CPU%u/re/cTbAllocated", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cNativeTbs,         STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated native TBs",     "/IEM/CPU%u/re/cTbAllocatedNative", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cThreadedTbs,       STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated threaded TBs",   "/IEM/CPU%u/re/cTbAllocatedThreaded", idCpu);

        PIEMTBCACHE     const pTbCache     = pVCpu->iem.s.pTbCacheR3;
        STAMR3RegisterF(pVM, (void *)&pTbCache->cHash,                  STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Translation block lookup table size",          "/IEM/CPU%u/re/cTbHashTab", idCpu);

        STAMR3RegisterF(pVM, (void *)&pTbCache->cLookupHits,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block lookup hits",                "/IEM/CPU%u/re/cTbLookupHits", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbCache->cLookupMisses,          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block lookup misses",              "/IEM/CPU%u/re/cTbLookupMisses", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbCache->cCollisions,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block hash table collisions",      "/IEM/CPU%u/re/cTbCollisions", idCpu);
# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, (void *)&pTbCache->StatPrune,              STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Time spent shortening collision lists",        "/IEM/CPU%u/re/TbPruningCollisions", idCpu);
# endif

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbThreadedCalls, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS_PER_TB,
                        "Calls per threaded translation block",         "/IEM/CPU%u/re/ThrdCallsPerTb", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbThreadedInstr, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_INSTR_PER_TB,
                        "Instruction per threaded translation block",   "/IEM/CPU%u/re/ThrdInstrPerTb", idCpu);

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckIrqBreaks,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "TB breaks by CheckIrq",                        "/IEM/CPU%u/re/CheckIrqBreaks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckModeBreaks, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "TB breaks by CheckMode",                       "/IEM/CPU%u/re/CheckModeBreaks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckBranchMisses, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Branch target misses",                         "/IEM/CPU%u/re/CheckTbJmpMisses", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckNeedCsLimChecking, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Needing CS.LIM checking TB after branch or on page crossing", "/IEM/CPU%u/re/CheckTbNeedCsLimChecking", idCpu);

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCallsRecompiled, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS_PER_TB,
                        "Number of threaded calls per TB that have been properly recompiled to native code",
                        "/IEM/CPU%u/re/NativeCallsRecompiledPerTb", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeCallsThreaded, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS_PER_TB,
                        "Number of threaded calls per TB that could not be recompiler to native code",
                        "/IEM/CPU%u/re/NativeCallsThreadedPerTb", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeFullyRecompiledTbs, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of threaded calls that could not be recompiler to native code",
                        "/IEM/CPU%u/re/NativeFullyRecompiledTbs", idCpu);

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbNativeCode, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES_PER_TB,
                        "Size of native code per TB",                   "/IEM/CPU%u/re/NativeCodeSizePerTb", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatNativeRecompilation, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Profiling iemNativeRecompile()",               "/IEM/CPU%u/re/NativeRecompilation", idCpu);

# ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
#  ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeRegFindFree, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of calls to iemNativeRegAllocFindFree.",
                        "/IEM/CPU%u/re/NativeRegFindFree", idCpu);
#  endif
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeRegFindFreeVar, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of times iemNativeRegAllocFindFree needed to free a variable.",
                        "/IEM/CPU%u/re/NativeRegFindFreeVar", idCpu);
#  ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeRegFindFreeNoVar, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of times iemNativeRegAllocFindFree did not needed to free any variables.",
                        "/IEM/CPU%u/re/NativeRegFindFreeNoVar", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeRegFindFreeLivenessUnshadowed, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Times liveness info freeed up shadowed guest registers in iemNativeRegAllocFindFree.",
                        "/IEM/CPU%u/re/NativeRegFindFreeLivenessUnshadowed", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeRegFindFreeLivenessHelped,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Times liveness info helped finding the return register in iemNativeRegAllocFindFree.",
                        "/IEM/CPU%u/re/NativeRegFindFreeLivenessHelped", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflCfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.CF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsCfSkippable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflPfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.PF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsPfSkippable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflAfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.AF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsAfSkippable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflZfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.ZF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsZfSkippable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflSfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.SF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsSfSkippable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflOfSkippable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Skippable EFLAGS.OF updating",       "/IEM/CPU%u/re/NativeLivenessEFlagsOfSkippable", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflCfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.CF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsCfRequired", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflPfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.PF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsPfRequired", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflAfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.AF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsAfRequired", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflZfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.ZF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsZfRequired", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflSfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.SF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsSfRequired", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflOfRequired,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Required EFLAGS.OF updating",        "/IEM/CPU%u/re/NativeLivenessEFlagsOfRequired", idCpu);

#   ifdef IEMLIVENESS_EXTENDED_LAYOUT
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflCfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.CF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsCfDelayable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflPfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.PF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsPfDelayable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflAfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.AF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsAfDelayable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflZfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.ZF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsZfDelayable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflSfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.SF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsSfDelayable", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativeLivenessEflOfDelayable,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Maybe delayable EFLAGS.OF updating", "/IEM/CPU%u/re/NativeLivenessEFlagsOfDelayable", idCpu);
#   endif

        /* Sum up all status bits ('_' is a sorting hack). */
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlags?fSkippable*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat, "Total skippable EFLAGS status bit updating",
                          "/IEM/CPU%u/re/NativeLivenessEFlags_StatusSkippable", idCpu);

        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlags?fRequired*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat, "Total required STATUS status bit updating",
                          "/IEM/CPU%u/re/NativeLivenessEFlags_StatusRequired", idCpu);

#   ifdef IEMLIVENESS_EXTENDED_LAYOUT
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlags?fDelayable*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat, "Total potentially delayable STATUS status bit updating",
                          "/IEM/CPU%u/re/NativeLivenessEFlags_StatusDelayable", idCpu);
#   endif

        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlags?f*", idCpu);
        STAMR3RegisterSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, szPat, "Total STATUS status bit events of any kind",
                          "/IEM/CPU%u/re/NativeLivenessEFlags_StatusTotal", idCpu);

        /* Ratio of the status bit skippables. */
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlags_StatusTotal", idCpu);
        RTStrPrintf(szVal, sizeof(szVal), "/IEM/CPU%u/re/NativeLivenessEFlags_StatusSkippable", idCpu);
        STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, false, szPat,
                               "Total skippable EFLAGS status bit updating percentage",
                               "/IEM/CPU%u/re/NativeLivenessEFlags_StatusSkippablePct", idCpu);

#   ifdef IEMLIVENESS_EXTENDED_LAYOUT
        /* Ratio of the status bit skippables. */
        RTStrPrintf(szVal, sizeof(szVal), "/IEM/CPU%u/re/NativeLivenessEFlags_StatusDelayable", idCpu);
        STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, false, szPat,
                               "Total potentially delayable EFLAGS status bit updating percentage",
                               "/IEM/CPU%u/re/NativeLivenessEFlags_StatusDelayablePct", idCpu);
#   endif

        /* Ratios of individual bits. */
        size_t const offFlagChar = RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativeLivenessEFlagsCf*", idCpu) - 3;
        Assert(szPat[offFlagChar] == 'C');
        RTStrPrintf(szVal, sizeof(szVal), "/IEM/CPU%u/re/NativeLivenessEFlagsCfSkippable", idCpu);
        Assert(szVal[offFlagChar] == 'C');
        szPat[offFlagChar] = szVal[offFlagChar] = 'C'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.CF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsCfSkippablePct", idCpu);
        szPat[offFlagChar] = szVal[offFlagChar] = 'P'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.PF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsPfSkippablePct", idCpu);
        szPat[offFlagChar] = szVal[offFlagChar] = 'A'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.AF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsAfSkippablePct", idCpu);
        szPat[offFlagChar] = szVal[offFlagChar] = 'Z'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.ZF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsZfSkippablePct", idCpu);
        szPat[offFlagChar] = szVal[offFlagChar] = 'S'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.SF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsSfSkippablePct", idCpu);
        szPat[offFlagChar] = szVal[offFlagChar] = 'O'; STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, true, szPat, "Skippable EFLAGS.OF updating percentage", "/IEM/CPU%u/re/NativeLivenessEFlagsOfSkippablePct", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativePcUpdateTotal,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Total RIP updates",   "/IEM/CPU%u/re/NativePcUpdateTotal", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.StatNativePcUpdateDelayed, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT, "Delayed RIP updates", "/IEM/CPU%u/re/NativePcUpdateDelayed", idCpu);

        /* Ratio of the status bit skippables. */
        RTStrPrintf(szPat, sizeof(szPat), "/IEM/CPU%u/re/NativePcUpdateTotal", idCpu);
        RTStrPrintf(szVal, sizeof(szVal), "/IEM/CPU%u/re/NativePcUpdateDelayed", idCpu);
        STAMR3RegisterPctOfSum(pVM->pUVM, STAMVISIBILITY_ALWAYS, STAMUNIT_PCT, szVal, false, szPat,
                               "Delayed RIP updating percentage",
                               "/IEM/CPU%u/re/NativePcUpdateDelayed_StatusDelayedPct", idCpu);

#  endif /* VBOX_WITH_STATISTICS */
# endif /* VBOX_WITH_IEM_NATIVE_RECOMPILER */

#endif /* VBOX_WITH_IEM_RECOMPILER */

        for (uint32_t i = 0; i < RT_ELEMENTS(pVCpu->iem.s.aStatXcpts); i++)
            STAMR3RegisterF(pVM, &pVCpu->iem.s.aStatXcpts[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "", "/IEM/CPU%u/Exceptions/%02x", idCpu, i);
        for (uint32_t i = 0; i < RT_ELEMENTS(pVCpu->iem.s.aStatInts); i++)
            STAMR3RegisterF(pVM, &pVCpu->iem.s.aStatInts[i], STAMTYPE_U32_RESET, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "", "/IEM/CPU%u/Interrupts/%02x", idCpu, i);

# if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_STATISTICS) && !defined(DOXYGEN_RUNNING)
        /* Instruction statistics: */
#  define IEM_DO_INSTR_STAT(a_Name, a_szDesc) \
            STAMR3RegisterF(pVM, &pVCpu->iem.s.StatsRZ.a_Name, STAMTYPE_U32_RESET, STAMVISIBILITY_USED, \
                            STAMUNIT_COUNT, a_szDesc, "/IEM/CPU%u/instr-RZ/" #a_Name, idCpu); \
            STAMR3RegisterF(pVM, &pVCpu->iem.s.StatsR3.a_Name, STAMTYPE_U32_RESET, STAMVISIBILITY_USED, \
                            STAMUNIT_COUNT, a_szDesc, "/IEM/CPU%u/instr-R3/" #a_Name, idCpu);
#  include "IEMInstructionStatisticsTmpl.h"
#  undef IEM_DO_INSTR_STAT
# endif

# if defined(VBOX_WITH_STATISTICS) && defined(VBOX_WITH_IEM_RECOMPILER) && !defined(VBOX_VMM_TARGET_ARMV8)
        /* Threaded function statistics: */
        for (unsigned i = 1; i < (unsigned)kIemThreadedFunc_End; i++)
            STAMR3RegisterF(pVM, &pVCpu->iem.s.acThreadedFuncStats[i], STAMTYPE_U32_RESET, STAMVISIBILITY_USED,
                            STAMUNIT_COUNT, NULL, "/IEM/CPU%u/ThrdFuncs/%s", idCpu, g_apszIemThreadedFunctionStats[i]);
# endif

#endif /* !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX) - quick fix for stupid structure duplication non-sense */
    }

#if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX)
    /*
     * Register the per-VM VMX APIC-access page handler type.
     */
    if (pVM->cpum.ro.GuestFeatures.fVmx)
    {
        rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_ALL, PGMPHYSHANDLER_F_NOT_IN_HM,
                                              iemVmxApicAccessPageHandler,
                                              "VMX APIC-access page", &pVM->iem.s.hVmxApicAccessPage);
        AssertLogRelRCReturn(rc, rc);
    }
#endif

    DBGFR3InfoRegisterInternalArgv(pVM, "itlb", "IEM instruction TLB", iemR3InfoITlb, DBGFINFO_FLAGS_RUN_ON_EMT);
    DBGFR3InfoRegisterInternalArgv(pVM, "dtlb", "IEM instruction TLB", iemR3InfoDTlb, DBGFINFO_FLAGS_RUN_ON_EMT);
#if defined(VBOX_WITH_IEM_RECOMPILER) && !defined(VBOX_VMM_TARGET_ARMV8)
    DBGFR3InfoRegisterInternalArgv(pVM, "tb",   "IEM translation block", iemR3InfoTb, DBGFINFO_FLAGS_RUN_ON_EMT);
#endif
#ifdef VBOX_WITH_DEBUGGER
    iemR3RegisterDebuggerCommands();
#endif

    return VINF_SUCCESS;
}


VMMR3DECL(int)      IEMR3Term(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}


VMMR3DECL(void)     IEMR3Relocate(PVM pVM)
{
    RT_NOREF(pVM);
}


/**
 * Gets the name of a generic IEM exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The IEM exit to name.
 */
VMMR3DECL(const char *) IEMR3GetExitName(uint32_t uExit)
{
    static const char * const s_apszNames[] =
    {
        /* external interrupts */
        "ExtInt 00h", "ExtInt 01h", "ExtInt 02h", "ExtInt 03h", "ExtInt 04h", "ExtInt 05h", "ExtInt 06h", "ExtInt 07h",
        "ExtInt 08h", "ExtInt 09h", "ExtInt 0ah", "ExtInt 0bh", "ExtInt 0ch", "ExtInt 0dh", "ExtInt 0eh", "ExtInt 0fh",
        "ExtInt 10h", "ExtInt 11h", "ExtInt 12h", "ExtInt 13h", "ExtInt 14h", "ExtInt 15h", "ExtInt 16h", "ExtInt 17h",
        "ExtInt 18h", "ExtInt 19h", "ExtInt 1ah", "ExtInt 1bh", "ExtInt 1ch", "ExtInt 1dh", "ExtInt 1eh", "ExtInt 1fh",
        "ExtInt 20h", "ExtInt 21h", "ExtInt 22h", "ExtInt 23h", "ExtInt 24h", "ExtInt 25h", "ExtInt 26h", "ExtInt 27h",
        "ExtInt 28h", "ExtInt 29h", "ExtInt 2ah", "ExtInt 2bh", "ExtInt 2ch", "ExtInt 2dh", "ExtInt 2eh", "ExtInt 2fh",
        "ExtInt 30h", "ExtInt 31h", "ExtInt 32h", "ExtInt 33h", "ExtInt 34h", "ExtInt 35h", "ExtInt 36h", "ExtInt 37h",
        "ExtInt 38h", "ExtInt 39h", "ExtInt 3ah", "ExtInt 3bh", "ExtInt 3ch", "ExtInt 3dh", "ExtInt 3eh", "ExtInt 3fh",
        "ExtInt 40h", "ExtInt 41h", "ExtInt 42h", "ExtInt 43h", "ExtInt 44h", "ExtInt 45h", "ExtInt 46h", "ExtInt 47h",
        "ExtInt 48h", "ExtInt 49h", "ExtInt 4ah", "ExtInt 4bh", "ExtInt 4ch", "ExtInt 4dh", "ExtInt 4eh", "ExtInt 4fh",
        "ExtInt 50h", "ExtInt 51h", "ExtInt 52h", "ExtInt 53h", "ExtInt 54h", "ExtInt 55h", "ExtInt 56h", "ExtInt 57h",
        "ExtInt 58h", "ExtInt 59h", "ExtInt 5ah", "ExtInt 5bh", "ExtInt 5ch", "ExtInt 5dh", "ExtInt 5eh", "ExtInt 5fh",
        "ExtInt 60h", "ExtInt 61h", "ExtInt 62h", "ExtInt 63h", "ExtInt 64h", "ExtInt 65h", "ExtInt 66h", "ExtInt 67h",
        "ExtInt 68h", "ExtInt 69h", "ExtInt 6ah", "ExtInt 6bh", "ExtInt 6ch", "ExtInt 6dh", "ExtInt 6eh", "ExtInt 6fh",
        "ExtInt 70h", "ExtInt 71h", "ExtInt 72h", "ExtInt 73h", "ExtInt 74h", "ExtInt 75h", "ExtInt 76h", "ExtInt 77h",
        "ExtInt 78h", "ExtInt 79h", "ExtInt 7ah", "ExtInt 7bh", "ExtInt 7ch", "ExtInt 7dh", "ExtInt 7eh", "ExtInt 7fh",
        "ExtInt 80h", "ExtInt 81h", "ExtInt 82h", "ExtInt 83h", "ExtInt 84h", "ExtInt 85h", "ExtInt 86h", "ExtInt 87h",
        "ExtInt 88h", "ExtInt 89h", "ExtInt 8ah", "ExtInt 8bh", "ExtInt 8ch", "ExtInt 8dh", "ExtInt 8eh", "ExtInt 8fh",
        "ExtInt 90h", "ExtInt 91h", "ExtInt 92h", "ExtInt 93h", "ExtInt 94h", "ExtInt 95h", "ExtInt 96h", "ExtInt 97h",
        "ExtInt 98h", "ExtInt 99h", "ExtInt 9ah", "ExtInt 9bh", "ExtInt 9ch", "ExtInt 9dh", "ExtInt 9eh", "ExtInt 9fh",
        "ExtInt a0h", "ExtInt a1h", "ExtInt a2h", "ExtInt a3h", "ExtInt a4h", "ExtInt a5h", "ExtInt a6h", "ExtInt a7h",
        "ExtInt a8h", "ExtInt a9h", "ExtInt aah", "ExtInt abh", "ExtInt ach", "ExtInt adh", "ExtInt aeh", "ExtInt afh",
        "ExtInt b0h", "ExtInt b1h", "ExtInt b2h", "ExtInt b3h", "ExtInt b4h", "ExtInt b5h", "ExtInt b6h", "ExtInt b7h",
        "ExtInt b8h", "ExtInt b9h", "ExtInt bah", "ExtInt bbh", "ExtInt bch", "ExtInt bdh", "ExtInt beh", "ExtInt bfh",
        "ExtInt c0h", "ExtInt c1h", "ExtInt c2h", "ExtInt c3h", "ExtInt c4h", "ExtInt c5h", "ExtInt c6h", "ExtInt c7h",
        "ExtInt c8h", "ExtInt c9h", "ExtInt cah", "ExtInt cbh", "ExtInt cch", "ExtInt cdh", "ExtInt ceh", "ExtInt cfh",
        "ExtInt d0h", "ExtInt d1h", "ExtInt d2h", "ExtInt d3h", "ExtInt d4h", "ExtInt d5h", "ExtInt d6h", "ExtInt d7h",
        "ExtInt d8h", "ExtInt d9h", "ExtInt dah", "ExtInt dbh", "ExtInt dch", "ExtInt ddh", "ExtInt deh", "ExtInt dfh",
        "ExtInt e0h", "ExtInt e1h", "ExtInt e2h", "ExtInt e3h", "ExtInt e4h", "ExtInt e5h", "ExtInt e6h", "ExtInt e7h",
        "ExtInt e8h", "ExtInt e9h", "ExtInt eah", "ExtInt ebh", "ExtInt ech", "ExtInt edh", "ExtInt eeh", "ExtInt efh",
        "ExtInt f0h", "ExtInt f1h", "ExtInt f2h", "ExtInt f3h", "ExtInt f4h", "ExtInt f5h", "ExtInt f6h", "ExtInt f7h",
        "ExtInt f8h", "ExtInt f9h", "ExtInt fah", "ExtInt fbh", "ExtInt fch", "ExtInt fdh", "ExtInt feh", "ExtInt ffh",
        /* software interrups */
        "SoftInt 00h", "SoftInt 01h", "SoftInt 02h", "SoftInt 03h", "SoftInt 04h", "SoftInt 05h", "SoftInt 06h", "SoftInt 07h",
        "SoftInt 08h", "SoftInt 09h", "SoftInt 0ah", "SoftInt 0bh", "SoftInt 0ch", "SoftInt 0dh", "SoftInt 0eh", "SoftInt 0fh",
        "SoftInt 10h", "SoftInt 11h", "SoftInt 12h", "SoftInt 13h", "SoftInt 14h", "SoftInt 15h", "SoftInt 16h", "SoftInt 17h",
        "SoftInt 18h", "SoftInt 19h", "SoftInt 1ah", "SoftInt 1bh", "SoftInt 1ch", "SoftInt 1dh", "SoftInt 1eh", "SoftInt 1fh",
        "SoftInt 20h", "SoftInt 21h", "SoftInt 22h", "SoftInt 23h", "SoftInt 24h", "SoftInt 25h", "SoftInt 26h", "SoftInt 27h",
        "SoftInt 28h", "SoftInt 29h", "SoftInt 2ah", "SoftInt 2bh", "SoftInt 2ch", "SoftInt 2dh", "SoftInt 2eh", "SoftInt 2fh",
        "SoftInt 30h", "SoftInt 31h", "SoftInt 32h", "SoftInt 33h", "SoftInt 34h", "SoftInt 35h", "SoftInt 36h", "SoftInt 37h",
        "SoftInt 38h", "SoftInt 39h", "SoftInt 3ah", "SoftInt 3bh", "SoftInt 3ch", "SoftInt 3dh", "SoftInt 3eh", "SoftInt 3fh",
        "SoftInt 40h", "SoftInt 41h", "SoftInt 42h", "SoftInt 43h", "SoftInt 44h", "SoftInt 45h", "SoftInt 46h", "SoftInt 47h",
        "SoftInt 48h", "SoftInt 49h", "SoftInt 4ah", "SoftInt 4bh", "SoftInt 4ch", "SoftInt 4dh", "SoftInt 4eh", "SoftInt 4fh",
        "SoftInt 50h", "SoftInt 51h", "SoftInt 52h", "SoftInt 53h", "SoftInt 54h", "SoftInt 55h", "SoftInt 56h", "SoftInt 57h",
        "SoftInt 58h", "SoftInt 59h", "SoftInt 5ah", "SoftInt 5bh", "SoftInt 5ch", "SoftInt 5dh", "SoftInt 5eh", "SoftInt 5fh",
        "SoftInt 60h", "SoftInt 61h", "SoftInt 62h", "SoftInt 63h", "SoftInt 64h", "SoftInt 65h", "SoftInt 66h", "SoftInt 67h",
        "SoftInt 68h", "SoftInt 69h", "SoftInt 6ah", "SoftInt 6bh", "SoftInt 6ch", "SoftInt 6dh", "SoftInt 6eh", "SoftInt 6fh",
        "SoftInt 70h", "SoftInt 71h", "SoftInt 72h", "SoftInt 73h", "SoftInt 74h", "SoftInt 75h", "SoftInt 76h", "SoftInt 77h",
        "SoftInt 78h", "SoftInt 79h", "SoftInt 7ah", "SoftInt 7bh", "SoftInt 7ch", "SoftInt 7dh", "SoftInt 7eh", "SoftInt 7fh",
        "SoftInt 80h", "SoftInt 81h", "SoftInt 82h", "SoftInt 83h", "SoftInt 84h", "SoftInt 85h", "SoftInt 86h", "SoftInt 87h",
        "SoftInt 88h", "SoftInt 89h", "SoftInt 8ah", "SoftInt 8bh", "SoftInt 8ch", "SoftInt 8dh", "SoftInt 8eh", "SoftInt 8fh",
        "SoftInt 90h", "SoftInt 91h", "SoftInt 92h", "SoftInt 93h", "SoftInt 94h", "SoftInt 95h", "SoftInt 96h", "SoftInt 97h",
        "SoftInt 98h", "SoftInt 99h", "SoftInt 9ah", "SoftInt 9bh", "SoftInt 9ch", "SoftInt 9dh", "SoftInt 9eh", "SoftInt 9fh",
        "SoftInt a0h", "SoftInt a1h", "SoftInt a2h", "SoftInt a3h", "SoftInt a4h", "SoftInt a5h", "SoftInt a6h", "SoftInt a7h",
        "SoftInt a8h", "SoftInt a9h", "SoftInt aah", "SoftInt abh", "SoftInt ach", "SoftInt adh", "SoftInt aeh", "SoftInt afh",
        "SoftInt b0h", "SoftInt b1h", "SoftInt b2h", "SoftInt b3h", "SoftInt b4h", "SoftInt b5h", "SoftInt b6h", "SoftInt b7h",
        "SoftInt b8h", "SoftInt b9h", "SoftInt bah", "SoftInt bbh", "SoftInt bch", "SoftInt bdh", "SoftInt beh", "SoftInt bfh",
        "SoftInt c0h", "SoftInt c1h", "SoftInt c2h", "SoftInt c3h", "SoftInt c4h", "SoftInt c5h", "SoftInt c6h", "SoftInt c7h",
        "SoftInt c8h", "SoftInt c9h", "SoftInt cah", "SoftInt cbh", "SoftInt cch", "SoftInt cdh", "SoftInt ceh", "SoftInt cfh",
        "SoftInt d0h", "SoftInt d1h", "SoftInt d2h", "SoftInt d3h", "SoftInt d4h", "SoftInt d5h", "SoftInt d6h", "SoftInt d7h",
        "SoftInt d8h", "SoftInt d9h", "SoftInt dah", "SoftInt dbh", "SoftInt dch", "SoftInt ddh", "SoftInt deh", "SoftInt dfh",
        "SoftInt e0h", "SoftInt e1h", "SoftInt e2h", "SoftInt e3h", "SoftInt e4h", "SoftInt e5h", "SoftInt e6h", "SoftInt e7h",
        "SoftInt e8h", "SoftInt e9h", "SoftInt eah", "SoftInt ebh", "SoftInt ech", "SoftInt edh", "SoftInt eeh", "SoftInt efh",
        "SoftInt f0h", "SoftInt f1h", "SoftInt f2h", "SoftInt f3h", "SoftInt f4h", "SoftInt f5h", "SoftInt f6h", "SoftInt f7h",
        "SoftInt f8h", "SoftInt f9h", "SoftInt fah", "SoftInt fbh", "SoftInt fch", "SoftInt fdh", "SoftInt feh", "SoftInt ffh",
    };
    if (uExit < RT_ELEMENTS(s_apszNames))
        return s_apszNames[uExit];
    return NULL;
}


/** Worker for iemR3InfoTlbPrintSlots and iemR3InfoTlbPrintAddress. */
static void iemR3InfoTlbPrintHeader(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb, bool *pfHeader)
{
    if (*pfHeader)
        return;
    pHlp->pfnPrintf(pHlp, "%cTLB for CPU %u:\n", &pVCpu->iem.s.CodeTlb == pTlb ? 'I' : 'D', pVCpu->idCpu);
    *pfHeader = true;
}


/** Worker for iemR3InfoTlbPrintSlots and iemR3InfoTlbPrintAddress. */
static void iemR3InfoTlbPrintSlot(PCDBGFINFOHLP pHlp, IEMTLB const *pTlb, IEMTLBENTRY const *pTlbe, uint32_t uSlot)
{
    pHlp->pfnPrintf(pHlp, "%02x: %s %#018RX64 -> %RGp / %p / %#05x %s%s%s%s/%s%s%s/%s %s\n",
                    uSlot,
                    (pTlbe->uTag & IEMTLB_REVISION_MASK) == pTlb->uTlbRevision ? "valid  "
                    : (pTlbe->uTag & IEMTLB_REVISION_MASK) == 0                ? "empty  "
                                                                               : "expired",
                    (pTlbe->uTag & ~IEMTLB_REVISION_MASK) << X86_PAGE_SHIFT,
                    pTlbe->GCPhys, pTlbe->pbMappingR3,
                    (uint32_t)(pTlbe->fFlagsAndPhysRev & ~IEMTLBE_F_PHYS_REV),
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_EXEC      ? "NX" : " X",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_WRITE     ? "RO" : "RW",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_ACCESSED  ? "-"  : "A",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_DIRTY     ? "-"  : "D",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_NO_WRITE     ? "-"  : "w",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_NO_READ      ? "-"  : "r",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_UNASSIGNED   ? "U"  : "-",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_NO_MAPPINGR3    ? "S"  : "M",
                    (pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PHYS_REV) == pTlb->uTlbPhysRev ? "phys-valid"
                    : (pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PHYS_REV) == 0 ? "phys-empty" : "phys-expired");
}


/** Displays one or more TLB slots. */
static void iemR3InfoTlbPrintSlots(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb,
                                   uint32_t uSlot, uint32_t cSlots, bool *pfHeader)
{
    if (uSlot < RT_ELEMENTS(pTlb->aEntries))
    {
        if (cSlots > RT_ELEMENTS(pTlb->aEntries))
        {
            pHlp->pfnPrintf(pHlp, "error: Too many slots given: %u, adjusting it down to the max (%u)\n",
                            cSlots, RT_ELEMENTS(pTlb->aEntries));
            cSlots = RT_ELEMENTS(pTlb->aEntries);
        }

        iemR3InfoTlbPrintHeader(pVCpu, pHlp, pTlb, pfHeader);
        while (cSlots-- > 0)
        {
            IEMTLBENTRY const Tlbe = pTlb->aEntries[uSlot];
            iemR3InfoTlbPrintSlot(pHlp, pTlb, &Tlbe, uSlot);
            uSlot = (uSlot + 1) % RT_ELEMENTS(pTlb->aEntries);
        }
    }
    else
        pHlp->pfnPrintf(pHlp, "error: TLB slot is out of range: %u (%#x), max %u (%#x)\n",
                        uSlot, uSlot, RT_ELEMENTS(pTlb->aEntries) - 1, RT_ELEMENTS(pTlb->aEntries) - 1);
}


/** Displays the TLB slot for the given address. */
static void iemR3InfoTlbPrintAddress(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb,
                                     uint64_t uAddress, bool *pfHeader)
{
    iemR3InfoTlbPrintHeader(pVCpu, pHlp, pTlb, pfHeader);

    uint64_t const    uTag  = (uAddress << 16) >> (X86_PAGE_SHIFT + 16);
    uint32_t const    uSlot = (uint8_t)uTag;
    IEMTLBENTRY const Tlbe  = pTlb->aEntries[uSlot];
    pHlp->pfnPrintf(pHlp, "Address %#RX64 -> slot %#x - %s\n", uAddress, uSlot,
                    Tlbe.uTag == (uTag | pTlb->uTlbRevision)  ? "match"
                    : (Tlbe.uTag & ~IEMTLB_REVISION_MASK) == uTag ? "expired" : "mismatch");
    iemR3InfoTlbPrintSlot(pHlp, pTlb, &Tlbe, uSlot);
}


/** Common worker for iemR3InfoDTlb and iemR3InfoITlb. */
static void iemR3InfoTlbCommon(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs, bool fITlb)
{
    /*
     * This is entirely argument driven.
     */
    static RTGETOPTDEF const s_aOptions[] =
    {
        { "--cpu",     'c', RTGETOPT_REQ_UINT32                          },
        { "--vcpu",    'c', RTGETOPT_REQ_UINT32                          },
        { "all",       'A', RTGETOPT_REQ_NOTHING                         },
        { "--all",     'A', RTGETOPT_REQ_NOTHING                         },
        { "--address", 'a', RTGETOPT_REQ_UINT64      | RTGETOPT_FLAG_HEX },
        { "--range",   'r', RTGETOPT_REQ_UINT32_PAIR | RTGETOPT_FLAG_HEX },
        { "--slot",    's', RTGETOPT_REQ_UINT32      | RTGETOPT_FLAG_HEX },
    };

    char  szDefault[] = "-A";
    char *papszDefaults[2] = { szDefault, NULL };
    if (cArgs == 0)
    {
        cArgs     = 1;
        papszArgs = papszDefaults;
    }

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /*iFirst*/, 0 /*fFlags*/);
    AssertRCReturnVoid(rc);

    bool            fNeedHeader  = true;
    bool            fAddressMode = true;
    PVMCPU          pVCpu        = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = VMMGetCpuById(pVM, 0);

    RTGETOPTUNION   ValueUnion;
    while ((rc = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                if (ValueUnion.u32 >= pVM->cCpus)
                    pHlp->pfnPrintf(pHlp, "error: Invalid CPU ID: %u\n", ValueUnion.u32);
                else if (!pVCpu || pVCpu->idCpu != ValueUnion.u32)
                {
                    pVCpu = VMMGetCpuById(pVM, ValueUnion.u32);
                    fNeedHeader = true;
                }
                break;

            case 'a':
                iemR3InfoTlbPrintAddress(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                         ValueUnion.u64, &fNeedHeader);
                fAddressMode = true;
                break;

            case 'A':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       0, RT_ELEMENTS(pVCpu->iem.s.CodeTlb.aEntries), &fNeedHeader);
                break;

            case 'r':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       ValueUnion.PairU32.uFirst, ValueUnion.PairU32.uSecond, &fNeedHeader);
                fAddressMode = false;
                break;

            case 's':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       ValueUnion.u32, 1, &fNeedHeader);
                fAddressMode = false;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (fAddressMode)
                {
                    uint64_t uAddr;
                    rc = RTStrToUInt64Full(ValueUnion.psz, 16, &uAddr);
                    if (RT_SUCCESS(rc) && rc != VWRN_NUMBER_TOO_BIG)
                        iemR3InfoTlbPrintAddress(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                                 uAddr, &fNeedHeader);
                    else
                        pHlp->pfnPrintf(pHlp, "error: Invalid or malformed guest address '%s': %Rrc\n", ValueUnion.psz, rc);
                }
                else
                {
                    uint32_t uSlot;
                    rc = RTStrToUInt32Full(ValueUnion.psz, 16, &uSlot);
                    if (RT_SUCCESS(rc) && rc != VWRN_NUMBER_TOO_BIG)
                        iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                               uSlot, 1, &fNeedHeader);
                    else
                        pHlp->pfnPrintf(pHlp, "error: Invalid or malformed TLB slot number '%s': %Rrc\n", ValueUnion.psz, rc);
                }
                break;

            case 'h':
                pHlp->pfnPrintf(pHlp,
                                "Usage: info %ctlb [options]\n"
                                "\n"
                                "Options:\n"
                                "  -c<n>, --cpu=<n>, --vcpu=<n>\n"
                                "    Selects the CPU which TLBs we're looking at. Default: Caller / 0\n"
                                "  -A, --all, all\n"
                                "    Display all the TLB entries (default if no other args).\n"
                                "  -a<virt>, --address=<virt>\n"
                                "    Shows the TLB entry for the specified guest virtual address.\n"
                                "  -r<slot:count>, --range=<slot:count>\n"
                                "    Shows the TLB entries for the specified slot range.\n"
                                "  -s<slot>,--slot=<slot>\n"
                                "    Shows the given TLB slot.\n"
                                "\n"
                                "Non-options are interpreted according to the last -a, -r or -s option,\n"
                                "defaulting to addresses if not preceeded by any of those options.\n"
                                , fITlb ? 'i' : 'd');
                return;

            default:
                pHlp->pfnGetOptError(pHlp, rc, &ValueUnion, &State);
                return;
        }
    }
}


/**
 * @callback_method_impl{FNDBGFINFOARGVINT, itlb}
 */
static DECLCALLBACK(void) iemR3InfoITlb(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs)
{
    return iemR3InfoTlbCommon(pVM, pHlp, cArgs, papszArgs, true /*fITlb*/);
}


/**
 * @callback_method_impl{FNDBGFINFOARGVINT, dtlb}
 */
static DECLCALLBACK(void) iemR3InfoDTlb(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs)
{
    return iemR3InfoTlbCommon(pVM, pHlp, cArgs, papszArgs, false /*fITlb*/);
}

#if defined(VBOX_WITH_IEM_RECOMPILER) && !defined(VBOX_VMM_TARGET_ARMV8)
/**
 * @callback_method_impl{FNDBGFINFOARGVINT, dtlb}
 */
static DECLCALLBACK(void) iemR3InfoTb(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs)
{
    /*
     * Parse arguments.
     */
    static RTGETOPTDEF const s_aOptions[] =
    {
        { "--cpu",              'c', RTGETOPT_REQ_UINT32                          },
        { "--vcpu",             'c', RTGETOPT_REQ_UINT32                          },
        { "--addr",             'a', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--address",          'a', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--phys",             'p', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--physical",         'p', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--phys-addr",        'p', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--phys-address",     'p', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--physical-address", 'p', RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_HEX },
        { "--flags",            'f', RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_HEX },
    };

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /*iFirst*/, 0 /*fFlags*/);
    AssertRCReturnVoid(rc);

    PVMCPU const    pVCpuThis   = VMMGetCpu(pVM);
    PVMCPU          pVCpu       = pVCpuThis ? pVCpuThis : VMMGetCpuById(pVM, 0);
    RTGCPHYS        GCPhysPc    = NIL_RTGCPHYS;
    RTGCPHYS        GCVirt      = NIL_RTGCPTR;
    uint32_t        fFlags      = UINT32_MAX;

    RTGETOPTUNION ValueUnion;
    while ((rc = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                if (ValueUnion.u32 >= pVM->cCpus)
                    pHlp->pfnPrintf(pHlp, "error: Invalid CPU ID: %u\n", ValueUnion.u32);
                else if (!pVCpu || pVCpu->idCpu != ValueUnion.u32)
                    pVCpu = VMMGetCpuById(pVM, ValueUnion.u32);
                break;

            case 'a':
                GCVirt   = ValueUnion.u64;
                GCPhysPc = NIL_RTGCPHYS;
                break;

            case 'p':
                GCVirt   = NIL_RTGCPHYS;
                GCPhysPc = ValueUnion.u64;
                break;

            case 'f':
                fFlags = ValueUnion.u32;
                break;

            case 'h':
                pHlp->pfnPrintf(pHlp,
                                "Usage: info %ctlb [options]\n"
                                "\n"
                                "Options:\n"
                                "  -c<n>, --cpu=<n>, --vcpu=<n>\n"
                                "    Selects the CPU which TBs we're looking at. Default: Caller / 0\n"
                                "  -a<virt>, --address=<virt>\n"
                                "    Shows the TB for the specified guest virtual address.\n"
                                "  -p<phys>, --phys=<phys>, --phys-addr=<phys>\n"
                                "    Shows the TB for the specified guest physical address.\n"
                                "  -f<flags>,--flags=<flags>\n"
                                "    The TB flags value (hex) to use when looking up the TB.\n"
                                "\n"
                                "The default is to use CS:RIP and derive flags from the CPU mode.\n");
                return;

            default:
                pHlp->pfnGetOptError(pHlp, rc, &ValueUnion, &State);
                return;
        }
    }

    /* Currently, only do work on the same EMT. */
    if (pVCpu != pVCpuThis)
    {
        pHlp->pfnPrintf(pHlp, "TODO: Cross EMT calling not supported yet: targeting %u, caller on %d\n",
                        pVCpu->idCpu, pVCpuThis ? (int)pVCpuThis->idCpu : -1);
        return;
    }

    /*
     * Defaults.
     */
    if (GCPhysPc == NIL_RTGCPHYS)
    {
        if (GCVirt == NIL_RTGCPTR)
            GCVirt = CPUMGetGuestFlatPC(pVCpu);
        rc = PGMPhysGCPtr2GCPhys(pVCpu, GCVirt, &GCPhysPc);
        if (RT_FAILURE(rc))
        {
            pHlp->pfnPrintf(pHlp, "Failed to convert %%%RGv to an guest physical address: %Rrc\n", GCVirt, rc);
            return;
        }
    }
    if (fFlags == UINT32_MAX)
    {
        /* Note! This is duplicating code in IEMAllThrdRecompiler. */
        fFlags = iemCalcExecFlags(pVCpu);
        if (pVM->cCpus == 1)
            fFlags |= IEM_F_X86_DISREGARD_LOCK;
        if (CPUMIsInInterruptShadow(&pVCpu->cpum.GstCtx))
            fFlags |= IEMTB_F_INHIBIT_SHADOW;
        if (CPUMAreInterruptsInhibitedByNmiEx(&pVCpu->cpum.GstCtx))
            fFlags |= IEMTB_F_INHIBIT_NMI;
        if ((IEM_F_MODE_CPUMODE_MASK & fFlags) != IEMMODE_64BIT)
        {
            int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
            if (offFromLim < X86_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
                fFlags |= IEMTB_F_CS_LIM_CHECKS;
        }
    }

    /*
     * Do the lookup...
     *
     * Note! This is also duplicating code in IEMAllThrdRecompiler.  We don't
     *       have much choice since we don't want to increase use counters and
     *       trigger native recompilation.
     */
    fFlags &= IEMTB_F_KEY_MASK;
    IEMTBCACHE const * const pTbCache = pVCpu->iem.s.pTbCacheR3;
    uint32_t const           idxHash  = IEMTBCACHE_HASH_NO_KEY_MASK(pTbCache, fFlags, GCPhysPc);
    PCIEMTB                  pTb      = IEMTBCACHE_PTR_GET_TB(pTbCache->apHash[idxHash]);
    while (pTb)
    {
        if (pTb->GCPhysPc == GCPhysPc)
        {
            if ((pTb->fFlags & IEMTB_F_KEY_MASK) == fFlags)
            {
                /// @todo if (pTb->x86.fAttr == (uint16_t)pVCpu->cpum.GstCtx.cs.Attr.u)
                break;
            }
        }
        pTb = pTb->pNext;
    }
    if (!pTb)
        pHlp->pfnPrintf(pHlp, "PC=%RGp fFlags=%#x - no TB found on #%u\n", GCPhysPc, fFlags, pVCpu->idCpu);
    else
    {
        /*
         * Disassemble according to type.
         */
        switch (pTb->fFlags & IEMTB_F_TYPE_MASK)
        {
# ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER
            case IEMTB_F_TYPE_NATIVE:
                pHlp->pfnPrintf(pHlp, "PC=%RGp fFlags=%#x on #%u: %p - native\n", GCPhysPc, fFlags, pVCpu->idCpu, pTb);
                iemNativeDisassembleTb(pTb, pHlp);
                break;
# endif

            case IEMTB_F_TYPE_THREADED:
                pHlp->pfnPrintf(pHlp, "PC=%RGp fFlags=%#x on #%u: %p - threaded\n", GCPhysPc, fFlags, pVCpu->idCpu, pTb);
                iemThreadedDisassembleTb(pTb, pHlp);
                break;

            default:
                pHlp->pfnPrintf(pHlp, "PC=%RGp fFlags=%#x on #%u: %p - ??? %#x\n",
                                GCPhysPc, fFlags, pVCpu->idCpu, pTb, pTb->fFlags);
                break;
        }
    }
}
#endif /* VBOX_WITH_IEM_RECOMPILER && !VBOX_VMM_TARGET_ARMV8 */


#ifdef VBOX_WITH_DEBUGGER

/** @callback_method_impl{FNDBGCCMD,
 * Implements the '.alliem' command. }
 */
static DECLCALLBACK(int) iemR3DbgFlushTlbs(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PUVM pUVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    VMCPUID idCpu = DBGCCmdHlpGetCurrentCpu(pCmdHlp);
    PVMCPU pVCpu = VMMR3GetCpuByIdU(pUVM, idCpu);
    if (pVCpu)
    {
        VMR3ReqPriorityCallVoidWaitU(pUVM, idCpu, (PFNRT)IEMTlbInvalidateAll, 1, pVCpu);
        return VINF_SUCCESS;
    }
    RT_NOREF(paArgs, cArgs);
    return DBGCCmdHlpFail(pCmdHlp, pCmd, "failed to get the PVMCPU for the current CPU");
}


/**
 * Called by IEMR3Init to register debugger commands.
 */
static void iemR3RegisterDebuggerCommands(void)
{
    /*
     * Register debugger commands.
     */
    static DBGCCMD const s_aCmds[] =
    {
        {
            /* .pszCmd = */         "iemflushtlb",
            /* .cArgsMin = */       0,
            /* .cArgsMax = */       0,
            /* .paArgDescs = */     NULL,
            /* .cArgDescs = */      0,
            /* .fFlags = */         0,
            /* .pfnHandler = */     iemR3DbgFlushTlbs,
            /* .pszSyntax = */      "",
            /* .pszDescription = */ "Flushed the code and data TLBs"
        },
    };

    int rc = DBGCRegisterCommands(&s_aCmds[0], RT_ELEMENTS(s_aCmds));
    AssertLogRelRC(rc);
}

#endif /* VBOX_WITH_DEBUGGER */

