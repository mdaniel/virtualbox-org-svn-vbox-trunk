/* $Id$ */
/** @file
 * NEM - Native execution manager, native ring-3 Windows backend.
 *
 * Log group 2: Exit logging.
 * Log group 3: Log context on exit.
 * Log group 5: Ring-3 memory management
 * Log group 6: Ring-0 memory management
 * Log group 12: API intercepts.
 */

/*
 * Copyright (C) 2018-2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_NEM
#define VMCPU_INCL_CPUM_GST_CTX
#include <iprt/nt/nt-and-windows.h>
#include <iprt/nt/hyperv.h>
#include <WinHvPlatform.h>

#ifndef _WIN32_WINNT_WIN10
# error "Missing _WIN32_WINNT_WIN10"
#endif
#ifndef _WIN32_WINNT_WIN10_RS1 /* Missing define, causing trouble for us. */
# define _WIN32_WINNT_WIN10_RS1 (_WIN32_WINNT_WIN10 + 1)
#endif
#include <sysinfoapi.h>
#include <debugapi.h>
#include <errhandlingapi.h>
#include <fileapi.h>
#include <winerror.h> /* no api header for this. */

#include <VBox/dis.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/pdmapic.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/dbgftrace.h>
#include "NEMInternal.h"
#include <VBox/vmm/vmcc.h>

#include <iprt/formats/arm-psci.h>

#include <iprt/ldr.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/utf16.h>

#ifndef NTDDI_WIN10_VB /* Present in W10 2004 SDK, quite possibly earlier. */
HRESULT WINAPI WHvQueryGpaRangeDirtyBitmap(WHV_PARTITION_HANDLE, WHV_GUEST_PHYSICAL_ADDRESS, UINT64, UINT64 *, UINT32);
# define WHvMapGpaRangeFlagTrackDirtyPages      ((WHV_MAP_GPA_RANGE_FLAGS)0x00000008)
#endif

/** Our saved state version for Hyper-V specific things. */
#define NEM_HV_SAVED_STATE_VERSION 1


/*
 * The following definitions appeared in build 27744 allow configuring the base address of the GICv3 controller,
 * (there is no official SDK for this yet).
 */
/** @todo Better way of defining these which doesn't require casting later on when calling APIs. */
#define WHV_PARTITION_PROPERTY_CODE_ARM64_IC_PARAMETERS UINT32_C(0x00001012)
/** No GIC present. */
#define WHV_ARM64_IC_EMULATION_MODE_NONE  0
/** Hyper-V emulates a GICv3. */
#define WHV_ARM64_IC_EMULATION_MODE_GICV3 1

/**
 * Configures the interrupt controller emulated by Hyper-V.
 */
typedef struct MY_WHV_ARM64_IC_PARAMETERS
{
    uint32_t u32EmulationMode;
    uint32_t u32Rsvd;
    union
    {
        struct
        {
            RTGCPHYS        GCPhysGicdBase;
            RTGCPHYS        GCPhysGitsTranslaterBase;
            uint32_t        u32Rsvd;
            uint32_t        cLpiIntIdBits;
            uint32_t        u32PpiCntvOverflw;
            uint32_t        u32PpiPmu;
            uint32_t        au32Rsvd[6];
        } GicV3;
    } u;
} MY_WHV_ARM64_IC_PARAMETERS;
AssertCompileSize(MY_WHV_ARM64_IC_PARAMETERS, 64);


/**
 * The hypercall exit context.
 */
typedef struct MY_WHV_HYPERCALL_CONTEXT
{
    WHV_INTERCEPT_MESSAGE_HEADER Header;
    uint16_t                     Immediate;
    uint16_t                     u16Rsvd;
    uint32_t                     u32Rsvd;
    uint64_t                     X[18];
} MY_WHV_HYPERCALL_CONTEXT;
typedef MY_WHV_HYPERCALL_CONTEXT *PMY_WHV_HYPERCALL_CONTEXT;
AssertCompileSize(MY_WHV_HYPERCALL_CONTEXT, 24 + 19 * sizeof(uint64_t));


/**
 * The ARM64 reset context.
 */
typedef struct MY_WHV_ARM64_RESET_CONTEXT
{
    WHV_INTERCEPT_MESSAGE_HEADER Header;
    uint32_t                     ResetType;
    uint32_t                     u32Rsvd;
} MY_WHV_ARM64_RESET_CONTEXT;
typedef MY_WHV_ARM64_RESET_CONTEXT *PMY_WHV_ARM64_RESET_CONTEXT;
AssertCompileSize(MY_WHV_ARM64_RESET_CONTEXT, 24 + 2 * sizeof(uint32_t));


#define WHV_ARM64_RESET_CONTEXT_TYPE_POWER_OFF   0
#define WHV_ARM64_RESET_CONTEXT_TYPE_RESET       1


/**
 * The exit reason context for arm64, the size is different
 * from the default SDK we build against.
 */
typedef struct MY_WHV_RUN_VP_EXIT_CONTEXT
{
    WHV_RUN_VP_EXIT_REASON  ExitReason;
    uint32_t                u32Rsvd;
    uint64_t                u64Rsvd;
    union
    {
        WHV_MEMORY_ACCESS_CONTEXT           MemoryAccess;
        WHV_RUN_VP_CANCELED_CONTEXT         CancelReason;
        MY_WHV_HYPERCALL_CONTEXT            Hypercall;
        WHV_UNRECOVERABLE_EXCEPTION_CONTEXT UnrecoverableException;
        MY_WHV_ARM64_RESET_CONTEXT          Arm64Reset;
        uint64_t au64Rsvd2[32];
    };
} MY_WHV_RUN_VP_EXIT_CONTEXT;
typedef MY_WHV_RUN_VP_EXIT_CONTEXT *PMY_WHV_RUN_VP_EXIT_CONTEXT;
AssertCompileSize(MY_WHV_RUN_VP_EXIT_CONTEXT, 272);

#define My_WHvArm64RegisterGicrBaseGpa ((WHV_REGISTER_NAME)UINT32_C(0x00063000))
#define My_WHvArm64RegisterActlrEl1    ((WHV_REGISTER_NAME)UINT32_C(0x00040003))


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name APIs imported from WinHvPlatform.dll
 * @{ */
static decltype(WHvGetCapability) *                 g_pfnWHvGetCapability;
static decltype(WHvCreatePartition) *               g_pfnWHvCreatePartition;
static decltype(WHvSetupPartition) *                g_pfnWHvSetupPartition;
static decltype(WHvDeletePartition) *               g_pfnWHvDeletePartition;
static decltype(WHvGetPartitionProperty) *          g_pfnWHvGetPartitionProperty;
static decltype(WHvSetPartitionProperty) *          g_pfnWHvSetPartitionProperty;
static decltype(WHvMapGpaRange) *                   g_pfnWHvMapGpaRange;
static decltype(WHvUnmapGpaRange) *                 g_pfnWHvUnmapGpaRange;
static decltype(WHvTranslateGva) *                  g_pfnWHvTranslateGva;
static decltype(WHvQueryGpaRangeDirtyBitmap) *      g_pfnWHvQueryGpaRangeDirtyBitmap;
static decltype(WHvCreateVirtualProcessor) *        g_pfnWHvCreateVirtualProcessor;
static decltype(WHvDeleteVirtualProcessor) *        g_pfnWHvDeleteVirtualProcessor;
static decltype(WHvRunVirtualProcessor) *           g_pfnWHvRunVirtualProcessor;
static decltype(WHvCancelRunVirtualProcessor) *     g_pfnWHvCancelRunVirtualProcessor;
static decltype(WHvGetVirtualProcessorRegisters) *  g_pfnWHvGetVirtualProcessorRegisters;
static decltype(WHvSetVirtualProcessorRegisters) *  g_pfnWHvSetVirtualProcessorRegisters;
static decltype(WHvSuspendPartitionTime) *          g_pfnWHvSuspendPartitionTime;
static decltype(WHvResumePartitionTime) *           g_pfnWHvResumePartitionTime;
decltype(WHvGetVirtualProcessorState) *             g_pfnWHvGetVirtualProcessorState;
decltype(WHvSetVirtualProcessorState) *             g_pfnWHvSetVirtualProcessorState;
decltype(WHvRequestInterrupt) *                     g_pfnWHvRequestInterrupt;
/** @} */

/** The Windows build number. */
static uint32_t g_uBuildNo = 17134;



/**
 * Import instructions.
 */
static const struct
{
    uint8_t     idxDll;     /**< 0 for WinHvPlatform.dll, 1 for vid.dll. */
    bool        fOptional;  /**< Set if import is optional. */
    PFNRT      *ppfn;       /**< The function pointer variable. */
    const char *pszName;    /**< The function name. */
} g_aImports[] =
{
#define NEM_WIN_IMPORT(a_idxDll, a_fOptional, a_Name) { (a_idxDll), (a_fOptional), (PFNRT *)&RT_CONCAT(g_pfn,a_Name), #a_Name }
    NEM_WIN_IMPORT(0, false, WHvGetCapability),
    NEM_WIN_IMPORT(0, false, WHvCreatePartition),
    NEM_WIN_IMPORT(0, false, WHvSetupPartition),
    NEM_WIN_IMPORT(0, false, WHvDeletePartition),
    NEM_WIN_IMPORT(0, false, WHvGetPartitionProperty),
    NEM_WIN_IMPORT(0, false, WHvSetPartitionProperty),
    NEM_WIN_IMPORT(0, false, WHvMapGpaRange),
    NEM_WIN_IMPORT(0, false, WHvUnmapGpaRange),
    NEM_WIN_IMPORT(0, false, WHvTranslateGva),
    NEM_WIN_IMPORT(0, true,  WHvQueryGpaRangeDirtyBitmap),
    NEM_WIN_IMPORT(0, false, WHvCreateVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvDeleteVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvCancelRunVirtualProcessor),
    NEM_WIN_IMPORT(0, false, WHvGetVirtualProcessorRegisters),
    NEM_WIN_IMPORT(0, false, WHvSetVirtualProcessorRegisters),
    NEM_WIN_IMPORT(0, false, WHvSuspendPartitionTime),
    NEM_WIN_IMPORT(0, false, WHvResumePartitionTime),
    NEM_WIN_IMPORT(0, false, WHvGetVirtualProcessorState),
    NEM_WIN_IMPORT(0, false, WHvSetVirtualProcessorState),
    NEM_WIN_IMPORT(0, false, WHvRequestInterrupt),
#undef NEM_WIN_IMPORT
};


/*
 * Let the preprocessor alias the APIs to import variables for better autocompletion.
 */
#ifndef IN_SLICKEDIT
# define WHvGetCapability                           g_pfnWHvGetCapability
# define WHvCreatePartition                         g_pfnWHvCreatePartition
# define WHvSetupPartition                          g_pfnWHvSetupPartition
# define WHvDeletePartition                         g_pfnWHvDeletePartition
# define WHvGetPartitionProperty                    g_pfnWHvGetPartitionProperty
# define WHvSetPartitionProperty                    g_pfnWHvSetPartitionProperty
# define WHvMapGpaRange                             g_pfnWHvMapGpaRange
# define WHvUnmapGpaRange                           g_pfnWHvUnmapGpaRange
# define WHvTranslateGva                            g_pfnWHvTranslateGva
# define WHvQueryGpaRangeDirtyBitmap                g_pfnWHvQueryGpaRangeDirtyBitmap
# define WHvCreateVirtualProcessor                  g_pfnWHvCreateVirtualProcessor
# define WHvDeleteVirtualProcessor                  g_pfnWHvDeleteVirtualProcessor
# define WHvRunVirtualProcessor                     g_pfnWHvRunVirtualProcessor
# define WHvGetRunExitContextSize                   g_pfnWHvGetRunExitContextSize
# define WHvCancelRunVirtualProcessor               g_pfnWHvCancelRunVirtualProcessor
# define WHvGetVirtualProcessorRegisters            g_pfnWHvGetVirtualProcessorRegisters
# define WHvSetVirtualProcessorRegisters            g_pfnWHvSetVirtualProcessorRegisters
# define WHvSuspendPartitionTime                    g_pfnWHvSuspendPartitionTime
# define WHvResumePartitionTime                     g_pfnWHvResumePartitionTime
# define WHvGetVirtualProcessorState                g_pfnWHvGetVirtualProcessorState
# define WHvSetVirtualProcessorState                g_pfnWHvSetVirtualProcessorState
# define WHvRequestInterrupt                        g_pfnWHvRequestInterrupt
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
DECLINLINE(int) nemR3NativeGCPhys2R3PtrReadOnly(PVM pVM, RTGCPHYS GCPhys, const void **ppv);
DECLINLINE(int) nemR3NativeGCPhys2R3PtrWriteable(PVM pVM, RTGCPHYS GCPhys, void **ppv);


/**
 * Worker for nemR3NativeInit that probes and load the native API.
 *
 * @returns VBox status code.
 * @param   fForced             Whether the HMForced flag is set and we should
 *                              fail if we cannot initialize.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitProbeAndLoad(bool fForced, PRTERRINFO pErrInfo)
{
    /*
     * Check that the DLL files we need are present, but without loading them.
     * We'd like to avoid loading them unnecessarily.
     */
    WCHAR wszPath[MAX_PATH + 64];
    UINT  cwcPath = GetSystemDirectoryW(wszPath, MAX_PATH);
    if (cwcPath >= MAX_PATH || cwcPath < 2)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "GetSystemDirectoryW failed (%#x / %u)", cwcPath, GetLastError());

    if (wszPath[cwcPath - 1] != '\\' || wszPath[cwcPath - 1] != '/')
        wszPath[cwcPath++] = '\\';
    RTUtf16CopyAscii(&wszPath[cwcPath], RT_ELEMENTS(wszPath) - cwcPath, "WinHvPlatform.dll");
    if (GetFileAttributesW(wszPath) == INVALID_FILE_ATTRIBUTES)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "The native API dll was not found (%ls)", wszPath);

    /*
     * Check that we're in a VM and that the hypervisor identifies itself as Hyper-V.
     */
    /** @todo */

    /** @todo would be great if we could recognize a root partition from the
     *        CPUID info, but I currently don't dare do that. */

    /*
     * Now try load the DLLs and resolve the APIs.
     */
    static const char * const s_apszDllNames[1] = { "WinHvPlatform.dll" };
    RTLDRMOD                  ahMods[1]         = { NIL_RTLDRMOD };
    int                       rc = VINF_SUCCESS;
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszDllNames); i++)
    {
        int rc2 = RTLdrLoadSystem(s_apszDllNames[i], true /*fNoUnload*/, &ahMods[i]);
        if (RT_FAILURE(rc2))
        {
            if (!RTErrInfoIsSet(pErrInfo))
                RTErrInfoSetF(pErrInfo, rc2, "Failed to load API DLL: %s: %Rrc", s_apszDllNames[i], rc2);
            else
                RTErrInfoAddF(pErrInfo, rc2, "; %s: %Rrc", s_apszDllNames[i], rc2);
            ahMods[i] = NIL_RTLDRMOD;
            rc = VERR_NEM_INIT_FAILED;
        }
    }
    if (RT_SUCCESS(rc))
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aImports); i++)
        {
            int rc2 = RTLdrGetSymbol(ahMods[g_aImports[i].idxDll], g_aImports[i].pszName, (void **)g_aImports[i].ppfn);
            if (RT_SUCCESS(rc2))
            {
                if (g_aImports[i].fOptional)
                    LogRel(("NEM:  info: Found optional import %s!%s.\n",
                            s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName));
            }
            else
            {
                *g_aImports[i].ppfn = NULL;

                LogRel(("NEM:  %s: Failed to import %s!%s: %Rrc",
                        g_aImports[i].fOptional ? "info" : fForced ? "fatal" : "error",
                        s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName, rc2));
                if (!g_aImports[i].fOptional)
                {
                    if (RTErrInfoIsSet(pErrInfo))
                        RTErrInfoAddF(pErrInfo, rc2, ", %s!%s",
                                      s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName);
                    else
                        rc = RTErrInfoSetF(pErrInfo, rc2, "Failed to import: %s!%s",
                                           s_apszDllNames[g_aImports[i].idxDll], g_aImports[i].pszName);
                    Assert(RT_FAILURE(rc));
                }
            }
        }
        if (RT_SUCCESS(rc))
        {
            Assert(!RTErrInfoIsSet(pErrInfo));
        }
    }

    for (unsigned i = 0; i < RT_ELEMENTS(ahMods); i++)
        RTLdrClose(ahMods[i]);
    return rc;
}


/**
 * Wrapper for different WHvGetCapability signatures.
 */
DECLINLINE(HRESULT) WHvGetCapabilityWrapper(WHV_CAPABILITY_CODE enmCap, WHV_CAPABILITY *pOutput, uint32_t cbOutput)
{
    return g_pfnWHvGetCapability(enmCap, pOutput, cbOutput, NULL);
}


/**
 * Worker for nemR3NativeInit that gets the hypervisor capabilities.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitCheckCapabilities(PVM pVM, PRTERRINFO pErrInfo)
{
#define NEM_LOG_REL_CAP_EX(a_szField, a_szFmt, a_Value)     LogRel(("NEM: %-38s= " a_szFmt "\n", a_szField, a_Value))
#define NEM_LOG_REL_CAP_SUB_EX(a_szField, a_szFmt, a_Value) LogRel(("NEM:   %36s: " a_szFmt "\n", a_szField, a_Value))
#define NEM_LOG_REL_CAP_SUB(a_szField, a_Value)             NEM_LOG_REL_CAP_SUB_EX(a_szField, "%d", a_Value)

    /*
     * Is the hypervisor present with the desired capability?
     *
     * In build 17083 this translates into:
     *      - CPUID[0x00000001].HVP is set
     *      - CPUID[0x40000000] == "Microsoft Hv"
     *      - CPUID[0x40000001].eax == "Hv#1"
     *      - CPUID[0x40000003].ebx[12] is set.
     *      - VidGetExoPartitionProperty(INVALID_HANDLE_VALUE, 0x60000, &Ignored) returns
     *        a non-zero value.
     */
    /**
     * @todo Someone at Microsoft please explain weird API design:
     *   1. Pointless CapabilityCode duplication int the output;
     *   2. No output size.
     */
    WHV_CAPABILITY Caps;
    RT_ZERO(Caps);
    SetLastError(0);
    HRESULT hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeHypervisorPresent, &Caps, sizeof(Caps));
    DWORD   rcWin = GetLastError();
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeHypervisorPresent failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    if (!Caps.HypervisorPresent)
    {
        if (!RTPathExists(RTPATH_NT_PASSTHRU_PREFIX "Device\\VidExo"))
            return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE,
                                 "WHvCapabilityCodeHypervisorPresent is FALSE! Make sure you have enabled the 'Windows Hypervisor Platform' feature.");
        return RTErrInfoSetF(pErrInfo, VERR_NEM_NOT_AVAILABLE, "WHvCapabilityCodeHypervisorPresent is FALSE! (%u)", rcWin);
    }
    LogRel(("NEM: WHvCapabilityCodeHypervisorPresent is TRUE, so this might work...\n"));


    /*
     * Check what extended VM exits are supported.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeExtendedVmExits, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeExtendedVmExits failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeExtendedVmExits", "%'#018RX64", Caps.ExtendedVmExits.AsUINT64);
    pVM->nem.s.fHypercallExit      = RT_BOOL(Caps.ExtendedVmExits.HypercallExit);
    pVM->nem.s.fGpaAccessFaultExit = RT_BOOL(Caps.ExtendedVmExits.GpaAccessFaultExit);
    NEM_LOG_REL_CAP_SUB("fHypercallExit",      pVM->nem.s.fHypercallExit);
    NEM_LOG_REL_CAP_SUB("fGpaAccessFaultExit", pVM->nem.s.fGpaAccessFaultExit);
    if (Caps.ExtendedVmExits.AsUINT64 & ~(uint64_t)7)
        LogRel(("NEM: Warning! Unknown VM exit definitions: %#RX64\n", Caps.ExtendedVmExits.AsUINT64));
    /** @todo RECHECK: WHV_EXTENDED_VM_EXITS typedef. */

    /*
     * Check features in case they end up defining any.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeFeatures, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeFeatures failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    if (Caps.Features.AsUINT64 & ~(uint64_t)0)
        LogRel(("NEM: Warning! Unknown feature definitions: %#RX64\n", Caps.Features.AsUINT64));
    /** @todo RECHECK: WHV_CAPABILITY_FEATURES typedef. */

    /*
     * Check that the CPU vendor is supported.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorVendor, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorVendor failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    switch (Caps.ProcessorVendor)
    {
        /** @todo RECHECK: WHV_PROCESSOR_VENDOR typedef. */
        case WHvProcessorVendorArm:
            NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorVendor", "%d - ARM", Caps.ProcessorVendor);
            pVM->nem.s.enmCpuVendor = CPUMCPUVENDOR_UNKNOWN;
            break;
        default:
            NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorVendor", "%d", Caps.ProcessorVendor);
            return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Unknown processor vendor: %d", Caps.ProcessorVendor);
    }

    /*
     * CPU features, guessing these are virtual CPU features?
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorFeatures, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorFeatures failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorFeatures", "%'#018RX64", Caps.ProcessorFeatures.AsUINT64);
#define NEM_LOG_REL_CPU_FEATURE(a_Field)    NEM_LOG_REL_CAP_SUB(#a_Field, Caps.ProcessorFeatures.a_Field)
    NEM_LOG_REL_CPU_FEATURE(Asid16);
    NEM_LOG_REL_CPU_FEATURE(TGran16);
    NEM_LOG_REL_CPU_FEATURE(TGran64);
    NEM_LOG_REL_CPU_FEATURE(Haf);
    NEM_LOG_REL_CPU_FEATURE(Hdbs);
    NEM_LOG_REL_CPU_FEATURE(Pan);
    NEM_LOG_REL_CPU_FEATURE(AtS1E1);
    NEM_LOG_REL_CPU_FEATURE(Uao);
    NEM_LOG_REL_CPU_FEATURE(El0Aarch32);
    NEM_LOG_REL_CPU_FEATURE(Fp);
    NEM_LOG_REL_CPU_FEATURE(FpHp);
    NEM_LOG_REL_CPU_FEATURE(AdvSimd);
    NEM_LOG_REL_CPU_FEATURE(AdvSimdHp);
    NEM_LOG_REL_CPU_FEATURE(GicV3V4);
    NEM_LOG_REL_CPU_FEATURE(GicV41);
    NEM_LOG_REL_CPU_FEATURE(Ras);
    NEM_LOG_REL_CPU_FEATURE(PmuV3);
    NEM_LOG_REL_CPU_FEATURE(PmuV3ArmV81);
    NEM_LOG_REL_CPU_FEATURE(PmuV3ArmV84);
    NEM_LOG_REL_CPU_FEATURE(PmuV3ArmV85);
    NEM_LOG_REL_CPU_FEATURE(Aes);
    NEM_LOG_REL_CPU_FEATURE(PolyMul);
    NEM_LOG_REL_CPU_FEATURE(Sha1);
    NEM_LOG_REL_CPU_FEATURE(Sha256);
    NEM_LOG_REL_CPU_FEATURE(Sha512);
    NEM_LOG_REL_CPU_FEATURE(Crc32);
    NEM_LOG_REL_CPU_FEATURE(Atomic);
    NEM_LOG_REL_CPU_FEATURE(Rdm);
    NEM_LOG_REL_CPU_FEATURE(Sha3);
    NEM_LOG_REL_CPU_FEATURE(Sm3);
    NEM_LOG_REL_CPU_FEATURE(Sm4);
    NEM_LOG_REL_CPU_FEATURE(Dp);
    NEM_LOG_REL_CPU_FEATURE(Fhm);
    NEM_LOG_REL_CPU_FEATURE(DcCvap);
    NEM_LOG_REL_CPU_FEATURE(DcCvadp);
    NEM_LOG_REL_CPU_FEATURE(ApaBase);
    NEM_LOG_REL_CPU_FEATURE(ApaEp);
    NEM_LOG_REL_CPU_FEATURE(ApaEp2);
    NEM_LOG_REL_CPU_FEATURE(ApaEp2Fp);
    NEM_LOG_REL_CPU_FEATURE(ApaEp2Fpc);
    NEM_LOG_REL_CPU_FEATURE(Jscvt);
    NEM_LOG_REL_CPU_FEATURE(Fcma);
    NEM_LOG_REL_CPU_FEATURE(RcpcV83);
    NEM_LOG_REL_CPU_FEATURE(RcpcV84);
    NEM_LOG_REL_CPU_FEATURE(Gpa);
    NEM_LOG_REL_CPU_FEATURE(L1ipPipt);
    NEM_LOG_REL_CPU_FEATURE(DzPermitted);

#undef NEM_LOG_REL_CPU_FEATURE
    if (Caps.ProcessorFeatures.AsUINT64 & (~(RT_BIT_64(47) - 1)))
        LogRel(("NEM: Warning! Unknown CPU features: %#RX64\n", Caps.ProcessorFeatures.AsUINT64));
    pVM->nem.s.uCpuFeatures.u64 = Caps.ProcessorFeatures.AsUINT64;
    /** @todo RECHECK: WHV_PROCESSOR_FEATURES typedef. */

    /*
     * The cache line flush size.
     */
    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodeProcessorClFlushSize, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodeProcessorClFlushSize failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodeProcessorClFlushSize", "2^%u", Caps.ProcessorClFlushSize);
    if (Caps.ProcessorClFlushSize < 8 && Caps.ProcessorClFlushSize > 9)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Unsupported cache line flush size: %u", Caps.ProcessorClFlushSize);
    pVM->nem.s.cCacheLineFlushShift = Caps.ProcessorClFlushSize;

    RT_ZERO(Caps);
    hrc = WHvGetCapabilityWrapper(WHvCapabilityCodePhysicalAddressWidth, &Caps, sizeof(Caps));
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED,
                             "WHvGetCapability/WHvCapabilityCodePhysicalAddressWidth failed: %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    NEM_LOG_REL_CAP_EX("WHvCapabilityCodePhysicalAddressWidth", "2^%u", Caps.PhysicalAddressWidth);
    if (Caps.PhysicalAddressWidth < 32 && Caps.PhysicalAddressWidth > 52)
        return RTErrInfoSetF(pErrInfo, VERR_NEM_INIT_FAILED, "Unsupported physical address width: %u", Caps.ProcessorClFlushSize);
    pVM->nem.s.cPhysicalAddressWidth = Caps.PhysicalAddressWidth;


    /*
     * See if they've added more properties that we're not aware of.
     */
    /** @todo RECHECK: WHV_CAPABILITY_CODE typedef. */
    if (!IsDebuggerPresent()) /* Too noisy when in debugger, so skip. */
    {
        static const struct
        {
            uint32_t iMin, iMax; } s_aUnknowns[] =
        {
            { 0x0004, 0x000f },
            { 0x1003, 0x100f },
            { 0x2000, 0x200f },
            { 0x3000, 0x300f },
            { 0x4000, 0x400f },
        };
        for (uint32_t j = 0; j < RT_ELEMENTS(s_aUnknowns); j++)
            for (uint32_t i = s_aUnknowns[j].iMin; i <= s_aUnknowns[j].iMax; i++)
            {
                RT_ZERO(Caps);
                hrc = WHvGetCapabilityWrapper((WHV_CAPABILITY_CODE)i, &Caps, sizeof(Caps));
                if (SUCCEEDED(hrc))
                    LogRel(("NEM: Warning! Unknown capability %#x returning: %.*Rhxs\n", i, sizeof(Caps), &Caps));
            }
    }

    /*
     * For proper operation, we require CPUID exits.
     */
    /** @todo Any? */

#undef NEM_LOG_REL_CAP_EX
#undef NEM_LOG_REL_CAP_SUB_EX
#undef NEM_LOG_REL_CAP_SUB
    return VINF_SUCCESS;
}


/**
 * Initializes the GIC controller emulation provided by Hyper-V.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 *
 * @note Needs to be done early when setting up the partition so this has to live here and not in GICNem-win.cpp
 */
static int nemR3WinGicCreate(PVM pVM)
{
    PCFGMNODE pGicCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "Devices/gic-nem/0/Config");
    AssertPtrReturn(pGicCfg, VERR_NEM_IPE_5);

    /*
     * Query the MMIO ranges.
     */
    RTGCPHYS GCPhysMmioBaseDist = 0;
    int rc = CFGMR3QueryU64(pGicCfg, "DistributorMmioBase", &GCPhysMmioBaseDist);
    if (RT_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          "Configuration error: Failed to get the \"DistributorMmioBase\" value\n");

    RTGCPHYS GCPhysMmioBaseReDist = 0;
    rc = CFGMR3QueryU64(pGicCfg, "RedistributorMmioBase", &GCPhysMmioBaseReDist);
    if (RT_FAILURE(rc))
        return VMSetError(pVM, rc, RT_SRC_POS,
                          "Configuration error: Failed to get the \"RedistributorMmioBase\" value\n");

    RTGCPHYS GCPhysMmioBaseIts = 0;
    rc = CFGMR3QueryU64(pGicCfg, "ItsMmioBase", &GCPhysMmioBaseIts);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return VMSetError(pVM, rc, RT_SRC_POS,
                          "Configuration error: Failed to get the \"ItsMmioBase\" value\n");
    rc = VINF_SUCCESS;

    /*
     * One can only set the GIC distributor base. The re-distributor regions for the individual
     * vCPUs are configured when the vCPUs are created, so we need to save the base of the MMIO region.
     */
    pVM->nem.s.GCPhysMmioBaseReDist = GCPhysMmioBaseReDist;

    WHV_PARTITION_HANDLE hPartition = pVM->nem.s.hPartition;

    MY_WHV_ARM64_IC_PARAMETERS Property; RT_ZERO(Property);
    Property.u32EmulationMode                 = WHV_ARM64_IC_EMULATION_MODE_GICV3;
    Property.u.GicV3.GCPhysGicdBase           = GCPhysMmioBaseDist;
    Property.u.GicV3.GCPhysGitsTranslaterBase = GCPhysMmioBaseIts;
    Property.u.GicV3.cLpiIntIdBits            = 1; /** @todo LPIs are currently not supported with our device emulations. */
    Property.u.GicV3.u32PpiCntvOverflw        = pVM->nem.s.u32GicPpiVTimer + 16; /* Calculate the absolute timer INTID. */
    Property.u.GicV3.u32PpiPmu                = 23; /** @todo Configure dynamically (from SBSA, needs a PMU/NEM emulation just like with the GIC probably). */
    HRESULT hrc = WHvSetPartitionProperty(hPartition, (WHV_PARTITION_PROPERTY_CODE)WHV_PARTITION_PROPERTY_CODE_ARM64_IC_PARAMETERS, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeArm64IcParameters: %Rhrc (Last=%#x/%u)",
                          hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    return rc;
}


/**
 * Creates and sets up a Hyper-V (exo) partition.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pErrInfo            Where to always return error info.
 */
static int nemR3WinInitCreatePartition(PVM pVM, PRTERRINFO pErrInfo)
{
    AssertReturn(!pVM->nem.s.hPartition,       RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));
    AssertReturn(!pVM->nem.s.hPartitionDevice, RTErrInfoSet(pErrInfo, VERR_WRONG_ORDER, "Wrong initalization order"));

    /*
     * Create the partition.
     */
    WHV_PARTITION_HANDLE hPartition;
    HRESULT hrc = WHvCreatePartition(&hPartition);
    if (FAILED(hrc))
        return RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED, "WHvCreatePartition failed with %Rhrc (Last=%#x/%u)",
                             hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    int rc;

    /*
     * Set partition properties, most importantly the CPU count.
     */
    /**
     * @todo Someone at Microsoft please explain another weird API:
     *  - Why this API doesn't take the WHV_PARTITION_PROPERTY_CODE value as an
     *    argument rather than as part of the struct.  That is so weird if you've
     *    used any other NT or windows API,  including WHvGetCapability().
     *  - Why use PVOID when WHV_PARTITION_PROPERTY is what's expected.  We
     *    technically only need 9 bytes for setting/getting
     *    WHVPartitionPropertyCodeProcessorClFlushSize, but the API insists on 16. */
    WHV_PARTITION_PROPERTY Property;
    RT_ZERO(Property);
    Property.ProcessorCount = pVM->cCpus;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorCount, &Property, sizeof(Property));
    if (SUCCEEDED(hrc))
    {
        RT_ZERO(Property);
        Property.ExtendedVmExits.HypercallExit  = pVM->nem.s.fHypercallExit;
        hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeExtendedVmExits, &Property, sizeof(Property));
        if (SUCCEEDED(hrc))
        {
            /*
             * We'll continue setup in nemR3NativeInitAfterCPUM.
             */
            pVM->nem.s.fCreatedEmts     = false;
            pVM->nem.s.hPartition       = hPartition;
            LogRel(("NEM: Created partition %p.\n", hPartition));
            return VINF_SUCCESS;
        }

        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED,
                           "Failed setting WHvPartitionPropertyCodeExtendedVmExits to %'#RX64: %Rhrc",
                           Property.ExtendedVmExits.AsUINT64, hrc);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_NEM_VM_CREATE_FAILED,
                           "Failed setting WHvPartitionPropertyCodeProcessorCount to %u: %Rhrc (Last=%#x/%u)",
                           pVM->cCpus, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());
    WHvDeletePartition(hPartition);

    Assert(!pVM->nem.s.hPartitionDevice);
    Assert(!pVM->nem.s.hPartition);
    return rc;
}


static int nemR3NativeInitSetupVm(PVM pVM)
{
    WHV_PARTITION_HANDLE hPartition = pVM->nem.s.hPartition;
    AssertReturn(hPartition != NULL, VERR_WRONG_ORDER);
    AssertReturn(!pVM->nem.s.hPartitionDevice, VERR_WRONG_ORDER);
    AssertReturn(!pVM->nem.s.fCreatedEmts, VERR_WRONG_ORDER);

    /*
     * Continue setting up the partition now that we've got most of the CPUID feature stuff.
     */
    WHV_PARTITION_PROPERTY Property;
    HRESULT                hrc;

    /* Not sure if we really need to set the cache line flush size. */
    RT_ZERO(Property);
    Property.ProcessorClFlushSize = pVM->nem.s.cCacheLineFlushShift;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorClFlushSize, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorClFlushSize to %u: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.cCacheLineFlushShift, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    /*
     * Sync CPU features with CPUM.
     */
    /** @todo sync CPU features with CPUM. */

    /* Set the partition property. */
    RT_ZERO(Property);
    Property.ProcessorFeatures.AsUINT64 = pVM->nem.s.uCpuFeatures.u64;
    hrc = WHvSetPartitionProperty(hPartition, WHvPartitionPropertyCodeProcessorFeatures, &Property, sizeof(Property));
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Failed to set WHvPartitionPropertyCodeProcessorFeatures to %'#RX64: %Rhrc (Last=%#x/%u)",
                          pVM->nem.s.uCpuFeatures.u64, hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    /* Configure the GIC. */
    int rc = nemR3WinGicCreate(pVM);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Set up the partition.
     *
     * Seems like this is where the partition is actually instantiated and we get
     * a handle to it.
     */
    hrc = WHvSetupPartition(hPartition);
    if (FAILED(hrc))
        return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                          "Call to WHvSetupPartition failed: %Rhrc (Last=%#x/%u)",
                          hrc, RTNtLastStatusValue(), RTNtLastErrorValue());

    /*
     * Setup the EMTs.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        hrc = WHvCreateVirtualProcessor(hPartition, idCpu, 0 /*fFlags*/);
        if (FAILED(hrc))
        {
            NTSTATUS const rcNtLast  = RTNtLastStatusValue();
            DWORD const    dwErrLast = RTNtLastErrorValue();
            while (idCpu-- > 0)
            {
                HRESULT hrc2 = WHvDeleteVirtualProcessor(hPartition, idCpu);
                AssertLogRelMsg(SUCCEEDED(hrc2), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                                  hPartition, idCpu, hrc2, RTNtLastStatusValue(),
                                                  RTNtLastErrorValue()));
            }
            return VMSetError(pVM, VERR_NEM_VM_CREATE_FAILED, RT_SRC_POS,
                              "Call to WHvCreateVirtualProcessor failed: %Rhrc (Last=%#x/%u)", hrc, rcNtLast, dwErrLast);
        }

        if (idCpu == 0)
        {
            /*
             * Need to query the ID registers and populate CPUM,
             * these are partition wide registers and need to be queried/set with WHV_ANY_VP.
             */
            CPUMARMV8IDREGS IdRegs; RT_ZERO(IdRegs);

            WHV_REGISTER_NAME  aenmNames[10];
            WHV_REGISTER_VALUE aValues[10];
            RT_ZERO(aValues);

            aenmNames[0] = WHvArm64RegisterIdAa64Dfr0El1;
            aenmNames[1] = WHvArm64RegisterIdAa64Dfr1El1;
            aenmNames[2] = WHvArm64RegisterIdAa64Isar0El1;
            aenmNames[3] = WHvArm64RegisterIdAa64Isar1El1;
            aenmNames[4] = WHvArm64RegisterIdAa64Isar2El1;
            aenmNames[5] = WHvArm64RegisterIdAa64Mmfr0El1;
            aenmNames[6] = WHvArm64RegisterIdAa64Mmfr1El1;
            aenmNames[7] = WHvArm64RegisterIdAa64Mmfr2El1;
            aenmNames[8] = WHvArm64RegisterIdAa64Pfr0El1;
            aenmNames[9] = WHvArm64RegisterIdAa64Pfr1El1;

            hrc = WHvGetVirtualProcessorRegisters(hPartition, WHV_ANY_VP /*idCpu*/, aenmNames, RT_ELEMENTS(aenmNames), aValues);
            AssertLogRelMsgReturn(SUCCEEDED(hrc),
                                  ("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                                   hPartition, WHV_ANY_VP, RT_ELEMENTS(aenmNames), hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                                  , VERR_NEM_GET_REGISTERS_FAILED);

            IdRegs.u64RegIdAa64Pfr0El1  = aValues[8].Reg64;
            IdRegs.u64RegIdAa64Pfr1El1  = aValues[9].Reg64;
            IdRegs.u64RegIdAa64Dfr0El1  = aValues[0].Reg64;
            IdRegs.u64RegIdAa64Dfr1El1  = aValues[1].Reg64;
            IdRegs.u64RegIdAa64Isar0El1 = aValues[2].Reg64;
            IdRegs.u64RegIdAa64Isar1El1 = aValues[3].Reg64;
            IdRegs.u64RegIdAa64Isar2El1 = aValues[4].Reg64;
            IdRegs.u64RegIdAa64Mmfr0El1 = aValues[5].Reg64;
            IdRegs.u64RegIdAa64Mmfr1El1 = aValues[6].Reg64;
            IdRegs.u64RegIdAa64Mmfr2El1 = aValues[7].Reg64;

            rc = CPUMR3PopulateFeaturesByIdRegisters(pVM, &IdRegs);
            if (RT_FAILURE(rc))
                return rc;

            /* Apply any overrides to the partition. */
            PCCPUMARMV8IDREGS pIdRegsGst = NULL;
            rc = CPUMR3QueryGuestIdRegs(pVM, &pIdRegsGst);
            AssertRCReturn(rc, rc);

            aValues[0].Reg64 = pIdRegsGst->u64RegIdAa64Dfr0El1;
            aValues[1].Reg64 = pIdRegsGst->u64RegIdAa64Dfr1El1;
            aValues[2].Reg64 = pIdRegsGst->u64RegIdAa64Isar0El1;
            aValues[3].Reg64 = pIdRegsGst->u64RegIdAa64Isar1El1;
            aValues[4].Reg64 = pIdRegsGst->u64RegIdAa64Isar2El1;
            aValues[5].Reg64 = pIdRegsGst->u64RegIdAa64Mmfr0El1;
            aValues[6].Reg64 = pIdRegsGst->u64RegIdAa64Mmfr1El1;
            aValues[7].Reg64 = pIdRegsGst->u64RegIdAa64Mmfr2El1;
            aValues[8].Reg64 = pIdRegsGst->u64RegIdAa64Pfr0El1;
            aValues[9].Reg64 = pIdRegsGst->u64RegIdAa64Pfr1El1;

            hrc = WHvSetVirtualProcessorRegisters(hPartition, WHV_ANY_VP /*idCpu*/, aenmNames, RT_ELEMENTS(aenmNames), aValues);
            AssertLogRelMsgReturn(SUCCEEDED(hrc),
                                  ("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                                   hPartition, WHV_ANY_VP, RT_ELEMENTS(aenmNames), hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                                  , VERR_NEM_SET_REGISTERS_FAILED);

            /* Save the amount of break-/watchpoints supported for syncing the guest register state later. */
            pVM->nem.s.cBreakpoints = RT_BF_GET(pIdRegsGst->u64RegIdAa64Dfr0El1, ARMV8_ID_AA64DFR0_EL1_BRPS) + 1;
            pVM->nem.s.cWatchpoints = RT_BF_GET(pIdRegsGst->u64RegIdAa64Dfr0El1, ARMV8_ID_AA64DFR0_EL1_WRPS) + 1;
        }

        /* Configure the GIC re-distributor region for the GIC. */
        WHV_REGISTER_NAME  enmName = My_WHvArm64RegisterGicrBaseGpa;
        WHV_REGISTER_VALUE Value;
        Value.Reg64 = pVM->nem.s.GCPhysMmioBaseReDist + idCpu * _128K;

        hrc = WHvSetVirtualProcessorRegisters(hPartition, idCpu, &enmName, 1, &Value);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorRegisters(%p, %u, WHvArm64RegisterGicrBaseGpa,) -> %Rhrc (Last=%#x/%u)\n",
                               hPartition, idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_SET_REGISTERS_FAILED);
    }

    pVM->nem.s.fCreatedEmts = true;

    LogRel(("NEM: Successfully set up partition\n"));
    return VINF_SUCCESS;
}


/**
 * Try initialize the native API.
 *
 * This may only do part of the job, more can be done in
 * nemR3NativeInitAfterCPUM() and nemR3NativeInitCompleted().
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   fFallback       Whether we're in fallback mode or use-NEM mode. In
 *                          the latter we'll fail if we cannot initialize.
 * @param   fForced         Whether the HMForced flag is set and we should
 *                          fail if we cannot initialize.
 */
int nemR3NativeInit(PVM pVM, bool fFallback, bool fForced)
{
    g_uBuildNo = RTSystemGetNtBuildNo();

    /*
     * Error state.
     * The error message will be non-empty on failure and 'rc' will be set too.
     */
    RTERRINFOSTATIC ErrInfo;
    PRTERRINFO pErrInfo = RTErrInfoInitStatic(&ErrInfo);
    int rc = nemR3WinInitProbeAndLoad(fForced, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * Check the capabilties of the hypervisor, starting with whether it's present.
         */
        rc = nemR3WinInitCheckCapabilities(pVM, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create and initialize a partition.
             */
            rc = nemR3WinInitCreatePartition(pVM, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                rc = nemR3NativeInitSetupVm(pVM);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Set ourselves as the execution engine and make config adjustments.
                     */
                    VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_NATIVE_API);
                    Log(("NEM: Marked active!\n"));
                    PGMR3EnableNemMode(pVM);

                    /*
                     * Register release statistics
                     */
                    STAMR3Register(pVM, (void *)&pVM->nem.s.cMappedPages, STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesCurrentlyMapped", STAMUNIT_PAGES, "Number guest pages currently mapped by the VM");
                    STAMR3Register(pVM, (void *)&pVM->nem.s.StatMapPage, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesMapCalls", STAMUNIT_PAGES, "Calls to WHvMapGpaRange/HvCallMapGpaPages");
                    STAMR3Register(pVM, (void *)&pVM->nem.s.StatMapPageFailed, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesMapFails", STAMUNIT_PAGES, "Calls to WHvMapGpaRange/HvCallMapGpaPages that failed");
                    STAMR3Register(pVM, (void *)&pVM->nem.s.StatUnmapPage, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesUnmapCalls", STAMUNIT_PAGES, "Calls to WHvUnmapGpaRange/HvCallUnmapGpaPages");
                    STAMR3Register(pVM, (void *)&pVM->nem.s.StatUnmapPageFailed, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesUnmapFails", STAMUNIT_PAGES, "Calls to WHvUnmapGpaRange/HvCallUnmapGpaPages that failed");
                    STAMR3Register(pVM, &pVM->nem.s.StatProfMapGpaRange, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesMapGpaRange", STAMUNIT_TICKS_PER_CALL, "Profiling calls to WHvMapGpaRange for bigger stuff");
                    STAMR3Register(pVM, &pVM->nem.s.StatProfUnmapGpaRange, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesUnmapGpaRange", STAMUNIT_TICKS_PER_CALL, "Profiling calls to WHvUnmapGpaRange for bigger stuff");
                    STAMR3Register(pVM, &pVM->nem.s.StatProfMapGpaRangePage, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesMapGpaRangePage", STAMUNIT_TICKS_PER_CALL, "Profiling calls to WHvMapGpaRange for single pages");
                    STAMR3Register(pVM, &pVM->nem.s.StatProfUnmapGpaRangePage, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS,
                                   "/NEM/PagesUnmapGpaRangePage", STAMUNIT_TICKS_PER_CALL, "Profiling calls to WHvUnmapGpaRange for single pages");

                    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
                    {
                        PNEMCPU pNemCpu = &pVM->apCpusR3[idCpu]->nem.s;
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitPortIo,          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of port I/O exits",               "/NEM/CPU%u/ExitPortIo", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitMemUnmapped,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unmapped memory exits",        "/NEM/CPU%u/ExitMemUnmapped", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitMemIntercept,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of intercepted memory exits",     "/NEM/CPU%u/ExitMemIntercept", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitHalt,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of HLT exits",                    "/NEM/CPU%u/ExitHalt", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitInterruptWindow, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of interrupt window exits",       "/NEM/CPU%u/ExitInterruptWindow", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitCpuId,           STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of CPUID exits",                  "/NEM/CPU%u/ExitCpuId", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitMsr,             STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of MSR access exits",             "/NEM/CPU%u/ExitMsr", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitException,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of exception exits",              "/NEM/CPU%u/ExitException", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionBp,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #BP exits",                    "/NEM/CPU%u/ExitExceptionBp", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionDb,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #DB exits",                    "/NEM/CPU%u/ExitExceptionDb", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionGp,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #GP exits",                    "/NEM/CPU%u/ExitExceptionGp", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionGpMesa, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #GP exits from mesa driver",   "/NEM/CPU%u/ExitExceptionGpMesa", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUd,     STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of #UD exits",                    "/NEM/CPU%u/ExitExceptionUd", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitExceptionUdHandled, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of handled #UD exits",         "/NEM/CPU%u/ExitExceptionUdHandled", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatExitUnrecoverable,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of unrecoverable exits",          "/NEM/CPU%u/ExitUnrecoverable", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatGetMsgTimeout,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of get message timeouts/alerts",  "/NEM/CPU%u/GetMsgTimeout", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuSuccess,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of successful CPU stops",         "/NEM/CPU%u/StopCpuSuccess", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPending,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stops",            "/NEM/CPU%u/StopCpuPending", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingAlerts,STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pending CPU stop alerts",      "/NEM/CPU%u/StopCpuPendingAlerts", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatStopCpuPendingOdd,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of odd pending CPU stops (see code)", "/NEM/CPU%u/StopCpuPendingOdd", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatCancelChangedState,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel changed state",         "/NEM/CPU%u/CancelChangedState", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatCancelAlertedThread, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel alerted EMT",           "/NEM/CPU%u/CancelAlertedEMT", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPre,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of pre execution FF breaks",      "/NEM/CPU%u/BreakOnFFPre", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnFFPost,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of post execution FF breaks",     "/NEM/CPU%u/BreakOnFFPost", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnCancel,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of cancel execution breaks",      "/NEM/CPU%u/BreakOnCancel", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatBreakOnStatus,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of status code breaks",           "/NEM/CPU%u/BreakOnStatus", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnDemand,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of on-demand state imports",      "/NEM/CPU%u/ImportOnDemand", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturn,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of state imports on loop return", "/NEM/CPU%u/ImportOnReturn", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatImportOnReturnSkipped, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of skipped state imports on loop return", "/NEM/CPU%u/ImportOnReturnSkipped", idCpu);
                        STAMR3RegisterF(pVM, &pNemCpu->StatQueryCpuTick,        STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Number of TSC queries",                  "/NEM/CPU%u/QueryCpuTick", idCpu);
                    }

#if defined(VBOX_WITH_R0_MODULES) && !defined(VBOX_WITH_MINIMAL_R0)
                    if (!SUPR3IsDriverless())
                    {
                        PUVM pUVM = pVM->pUVM;
                        STAMR3RegisterRefresh(pUVM, &pVM->nem.s.R0Stats.cPagesAvailable, STAMTYPE_U64, STAMVISIBILITY_ALWAYS,
                                              STAMUNIT_PAGES, STAM_REFRESH_GRP_NEM, "Free pages available to the hypervisor",
                                              "/NEM/R0Stats/cPagesAvailable");
                        STAMR3RegisterRefresh(pUVM, &pVM->nem.s.R0Stats.cPagesInUse,     STAMTYPE_U64, STAMVISIBILITY_ALWAYS,
                                              STAMUNIT_PAGES, STAM_REFRESH_GRP_NEM, "Pages in use by hypervisor",
                                              "/NEM/R0Stats/cPagesInUse");
                    }
#endif /* VBOX_WITH_R0_MODULES && !VBOX_WITH_MINIMAL_R0 */
                }
            }
        }
    }

    /*
     * We only fail if in forced mode, otherwise just log the complaint and return.
     */
    Assert(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API || RTErrInfoIsSet(pErrInfo));
    if (   (fForced || !fFallback)
        && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API)
        return VMSetError(pVM, RT_SUCCESS_NP(rc) ? VERR_NEM_NOT_AVAILABLE : rc, RT_SRC_POS, "%s", pErrInfo->pszMsg);

    if (RTErrInfoIsSet(pErrInfo))
        LogRel(("NEM: Not available: %s\n", pErrInfo->pszMsg));
    return VINF_SUCCESS;
}


/**
 * This is called after CPUMR3Init is done.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle..
 */
int nemR3NativeInitAfterCPUM(PVM pVM)
{
    /*
     * Validate sanity.
     */
    AssertReturn(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API, VERR_WRONG_ORDER);

    /** @todo */

    /*
     * Any hyper-v statistics we can get at now? HvCallMapStatsPage isn't accessible any more.
     */
    /** @todo stats   */

    /*
     * Adjust features.
     *
     * Note! We've already disabled X2APIC and MONITOR/MWAIT via CFGM during
     *       the first init call.
     */

    return VINF_SUCCESS;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) nemR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save the Hyper-V activity state for all CPUs.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPUCC pVCpu = pVM->apCpusR3[i];

        static const WHV_REGISTER_NAME s_Name = WHvRegisterInternalActivityState;
        WHV_REGISTER_VALUE Reg;

        HRESULT hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, &s_Name, 1, &Reg);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorRegisters(%p, 0,{WHvRegisterInternalActivityState}, 1,) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_IPE_9);

        SSMR3PutU64(pSSM, Reg.Reg64);
    }

    return SSMR3PutU32(pSSM, UINT32_MAX); /* terminator */
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) nemR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Validate version.
     */
    if (uVersion != 1)
    {
        AssertMsgFailed(("nemR3Load: Invalid version uVersion=%u!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Restore the Hyper-V activity states for all vCPUs.
     */
    VMCPU_SET_STATE(pVM->apCpusR3[0], VMCPUSTATE_STARTED);
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPUCC pVCpu = pVM->apCpusR3[i];

        static const WHV_REGISTER_NAME s_Name = WHvRegisterInternalActivityState;
        WHV_REGISTER_VALUE Reg;
        int rc = SSMR3GetU64(pSSM, &Reg.Reg64);
        if (RT_FAILURE(rc))
            return rc;

        HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, &s_Name, 1, &Reg);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorRegisters(%p, 0,{WHvRegisterInternalActivityState}, 1,) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_IPE_9);
    }

    /* terminator */
    uint32_t u32;
    int rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != UINT32_MAX)
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    return VINF_SUCCESS;
}


int nemR3NativeInitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    //BOOL fRet = SetThreadPriority(GetCurrentThread(), 0);
    //AssertLogRel(fRet);

    if (enmWhat == VMINITCOMPLETED_RING3)
    {
        /*
         * Register the saved state data unit.
         */
        int rc = SSMR3RegisterInternal(pVM, "nem-win", 1, NEM_HV_SAVED_STATE_VERSION,
                                       sizeof(uint64_t),
                                       NULL, NULL, NULL,
                                       NULL, nemR3Save, NULL,
                                       NULL, nemR3Load, NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    NOREF(pVM); NOREF(enmWhat);
    return VINF_SUCCESS;
}


int nemR3NativeTerm(PVM pVM)
{
    /*
     * Delete the partition.
     */
    WHV_PARTITION_HANDLE hPartition = pVM->nem.s.hPartition;
    pVM->nem.s.hPartition       = NULL;
    pVM->nem.s.hPartitionDevice = NULL;
    if (hPartition != NULL)
    {
        VMCPUID idCpu = pVM->nem.s.fCreatedEmts ? pVM->cCpus : 0;
        LogRel(("NEM: Destroying partition %p with its %u VCpus...\n", hPartition, idCpu));
        while (idCpu-- > 0)
        {
            HRESULT hrc = WHvDeleteVirtualProcessor(hPartition, idCpu);
            AssertLogRelMsg(SUCCEEDED(hrc), ("WHvDeleteVirtualProcessor(%p, %u) -> %Rhrc (Last=%#x/%u)\n",
                                             hPartition, idCpu, hrc, RTNtLastStatusValue(),
                                             RTNtLastErrorValue()));
        }
        WHvDeletePartition(hPartition);
    }
    pVM->nem.s.fCreatedEmts = false;
    return VINF_SUCCESS;
}


/**
 * VM reset notification.
 *
 * @param   pVM         The cross context VM structure.
 */
void nemR3NativeReset(PVM pVM)
{
    RT_NOREF(pVM);
}


/**
 * Reset CPU due to INIT IPI or hot (un)plugging.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the CPU being
 *                      reset.
 * @param   fInitIpi    Whether this is the INIT IPI or hot (un)plugging case.
 */
void nemR3NativeResetCpu(PVMCPU pVCpu, bool fInitIpi)
{
    RT_NOREF(pVCpu, fInitIpi);
}


NEM_TMPL_STATIC int nemHCWinCopyStateToHyperV(PVMCC pVM, PVMCPUCC pVCpu)
{
    WHV_REGISTER_NAME  aenmNames[128];
    WHV_REGISTER_VALUE aValues[128];

    uint64_t const fWhat = ~pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL;
    if (!fWhat)
        return VINF_SUCCESS;
    uintptr_t iReg = 0;

#define ADD_REG64(a_enmName, a_uValue) do { \
            aenmNames[iReg]      = (a_enmName); \
            aValues[iReg].Reg128.High64 = 0; \
            aValues[iReg].Reg64  = (a_uValue).x; \
            iReg++; \
        } while (0)
#define ADD_REG64_RAW(a_enmName, a_uValue) do { \
            aenmNames[iReg]      = (a_enmName); \
            aValues[iReg].Reg128.High64 = 0; \
            aValues[iReg].Reg64  = (a_uValue); \
            iReg++; \
        } while (0)
#define ADD_SYSREG64(a_enmName, a_uValue) do { \
            aenmNames[iReg]      = (a_enmName); \
            aValues[iReg].Reg128.High64 = 0; \
            aValues[iReg].Reg64  = (a_uValue).u64; \
            iReg++; \
        } while (0)
#define ADD_REG128(a_enmName, a_uValue) do { \
            aenmNames[iReg] = (a_enmName); \
            aValues[iReg].Reg128.Low64  = (a_uValue).au64[0]; \
            aValues[iReg].Reg128.High64 = (a_uValue).au64[1]; \
            iReg++; \
        } while (0)

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_X0)
            ADD_REG64(WHvArm64RegisterX0, pVCpu->cpum.GstCtx.aGRegs[0]);
        if (fWhat & CPUMCTX_EXTRN_X1)
            ADD_REG64(WHvArm64RegisterX1, pVCpu->cpum.GstCtx.aGRegs[1]);
        if (fWhat & CPUMCTX_EXTRN_X2)
            ADD_REG64(WHvArm64RegisterX2, pVCpu->cpum.GstCtx.aGRegs[2]);
        if (fWhat & CPUMCTX_EXTRN_X3)
            ADD_REG64(WHvArm64RegisterX3, pVCpu->cpum.GstCtx.aGRegs[3]);
        if (fWhat & CPUMCTX_EXTRN_X4_X28)
        {
            ADD_REG64(WHvArm64RegisterX4,  pVCpu->cpum.GstCtx.aGRegs[4]);
            ADD_REG64(WHvArm64RegisterX5,  pVCpu->cpum.GstCtx.aGRegs[5]);
            ADD_REG64(WHvArm64RegisterX6,  pVCpu->cpum.GstCtx.aGRegs[6]);
            ADD_REG64(WHvArm64RegisterX7,  pVCpu->cpum.GstCtx.aGRegs[7]);
            ADD_REG64(WHvArm64RegisterX8,  pVCpu->cpum.GstCtx.aGRegs[8]);
            ADD_REG64(WHvArm64RegisterX9,  pVCpu->cpum.GstCtx.aGRegs[9]);
            ADD_REG64(WHvArm64RegisterX10, pVCpu->cpum.GstCtx.aGRegs[10]);
            ADD_REG64(WHvArm64RegisterX11, pVCpu->cpum.GstCtx.aGRegs[11]);
            ADD_REG64(WHvArm64RegisterX12, pVCpu->cpum.GstCtx.aGRegs[12]);
            ADD_REG64(WHvArm64RegisterX13, pVCpu->cpum.GstCtx.aGRegs[13]);
            ADD_REG64(WHvArm64RegisterX14, pVCpu->cpum.GstCtx.aGRegs[14]);
            ADD_REG64(WHvArm64RegisterX15, pVCpu->cpum.GstCtx.aGRegs[15]);
            ADD_REG64(WHvArm64RegisterX16, pVCpu->cpum.GstCtx.aGRegs[16]);
            ADD_REG64(WHvArm64RegisterX17, pVCpu->cpum.GstCtx.aGRegs[17]);
            ADD_REG64(WHvArm64RegisterX18, pVCpu->cpum.GstCtx.aGRegs[18]);
            ADD_REG64(WHvArm64RegisterX19, pVCpu->cpum.GstCtx.aGRegs[19]);
            ADD_REG64(WHvArm64RegisterX20, pVCpu->cpum.GstCtx.aGRegs[20]);
            ADD_REG64(WHvArm64RegisterX21, pVCpu->cpum.GstCtx.aGRegs[21]);
            ADD_REG64(WHvArm64RegisterX22, pVCpu->cpum.GstCtx.aGRegs[22]);
            ADD_REG64(WHvArm64RegisterX23, pVCpu->cpum.GstCtx.aGRegs[23]);
            ADD_REG64(WHvArm64RegisterX24, pVCpu->cpum.GstCtx.aGRegs[24]);
            ADD_REG64(WHvArm64RegisterX25, pVCpu->cpum.GstCtx.aGRegs[25]);
            ADD_REG64(WHvArm64RegisterX26, pVCpu->cpum.GstCtx.aGRegs[26]);
            ADD_REG64(WHvArm64RegisterX27, pVCpu->cpum.GstCtx.aGRegs[27]);
            ADD_REG64(WHvArm64RegisterX28, pVCpu->cpum.GstCtx.aGRegs[28]);
        }
        if (fWhat & CPUMCTX_EXTRN_LR)
            ADD_REG64(WHvArm64RegisterLr, pVCpu->cpum.GstCtx.aGRegs[30]);
        if (fWhat & CPUMCTX_EXTRN_FP)
            ADD_REG64(WHvArm64RegisterFp, pVCpu->cpum.GstCtx.aGRegs[29]);
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_PC)
        ADD_SYSREG64(WHvArm64RegisterPc, pVCpu->cpum.GstCtx.Pc);
    if (fWhat & CPUMCTX_EXTRN_PSTATE)
        ADD_REG64_RAW(WHvArm64RegisterPstate, pVCpu->cpum.GstCtx.fPState);
    if (fWhat & CPUMCTX_EXTRN_SPSR)
        ADD_SYSREG64(WHvArm64RegisterSpsrEl1, pVCpu->cpum.GstCtx.Spsr);
    if (fWhat & CPUMCTX_EXTRN_ELR)
        ADD_SYSREG64(WHvArm64RegisterElrEl1, pVCpu->cpum.GstCtx.Elr);
    if (fWhat & CPUMCTX_EXTRN_SP)
    {
        ADD_SYSREG64(WHvArm64RegisterSpEl0, pVCpu->cpum.GstCtx.aSpReg[0]);
        ADD_SYSREG64(WHvArm64RegisterSpEl1, pVCpu->cpum.GstCtx.aSpReg[1]);
    }
    if (fWhat & CPUMCTX_EXTRN_SCTLR_TCR_TTBR)
    {
        ADD_SYSREG64(WHvArm64RegisterSctlrEl1, pVCpu->cpum.GstCtx.Sctlr);
        ADD_SYSREG64(WHvArm64RegisterTcrEl1,   pVCpu->cpum.GstCtx.Tcr);
        ADD_SYSREG64(WHvArm64RegisterTtbr0El1, pVCpu->cpum.GstCtx.Ttbr0);
        ADD_SYSREG64(WHvArm64RegisterTtbr1El1, pVCpu->cpum.GstCtx.Ttbr1);
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_V0_V31)
    {
        ADD_REG128(WHvArm64RegisterQ0,  pVCpu->cpum.GstCtx.aVRegs[0]);
        ADD_REG128(WHvArm64RegisterQ1,  pVCpu->cpum.GstCtx.aVRegs[1]);
        ADD_REG128(WHvArm64RegisterQ2,  pVCpu->cpum.GstCtx.aVRegs[2]);
        ADD_REG128(WHvArm64RegisterQ3,  pVCpu->cpum.GstCtx.aVRegs[3]);
        ADD_REG128(WHvArm64RegisterQ4,  pVCpu->cpum.GstCtx.aVRegs[4]);
        ADD_REG128(WHvArm64RegisterQ5,  pVCpu->cpum.GstCtx.aVRegs[5]);
        ADD_REG128(WHvArm64RegisterQ6,  pVCpu->cpum.GstCtx.aVRegs[6]);
        ADD_REG128(WHvArm64RegisterQ7,  pVCpu->cpum.GstCtx.aVRegs[7]);
        ADD_REG128(WHvArm64RegisterQ8,  pVCpu->cpum.GstCtx.aVRegs[8]);
        ADD_REG128(WHvArm64RegisterQ9,  pVCpu->cpum.GstCtx.aVRegs[9]);
        ADD_REG128(WHvArm64RegisterQ10, pVCpu->cpum.GstCtx.aVRegs[10]);
        ADD_REG128(WHvArm64RegisterQ11, pVCpu->cpum.GstCtx.aVRegs[11]);
        ADD_REG128(WHvArm64RegisterQ12, pVCpu->cpum.GstCtx.aVRegs[12]);
        ADD_REG128(WHvArm64RegisterQ13, pVCpu->cpum.GstCtx.aVRegs[13]);
        ADD_REG128(WHvArm64RegisterQ14, pVCpu->cpum.GstCtx.aVRegs[14]);
        ADD_REG128(WHvArm64RegisterQ15, pVCpu->cpum.GstCtx.aVRegs[15]);
        ADD_REG128(WHvArm64RegisterQ16, pVCpu->cpum.GstCtx.aVRegs[16]);
        ADD_REG128(WHvArm64RegisterQ17, pVCpu->cpum.GstCtx.aVRegs[17]);
        ADD_REG128(WHvArm64RegisterQ18, pVCpu->cpum.GstCtx.aVRegs[18]);
        ADD_REG128(WHvArm64RegisterQ19, pVCpu->cpum.GstCtx.aVRegs[19]);
        ADD_REG128(WHvArm64RegisterQ20, pVCpu->cpum.GstCtx.aVRegs[20]);
        ADD_REG128(WHvArm64RegisterQ21, pVCpu->cpum.GstCtx.aVRegs[21]);
        ADD_REG128(WHvArm64RegisterQ22, pVCpu->cpum.GstCtx.aVRegs[22]);
        ADD_REG128(WHvArm64RegisterQ23, pVCpu->cpum.GstCtx.aVRegs[23]);
        ADD_REG128(WHvArm64RegisterQ24, pVCpu->cpum.GstCtx.aVRegs[24]);
        ADD_REG128(WHvArm64RegisterQ25, pVCpu->cpum.GstCtx.aVRegs[25]);
        ADD_REG128(WHvArm64RegisterQ26, pVCpu->cpum.GstCtx.aVRegs[26]);
        ADD_REG128(WHvArm64RegisterQ27, pVCpu->cpum.GstCtx.aVRegs[27]);
        ADD_REG128(WHvArm64RegisterQ28, pVCpu->cpum.GstCtx.aVRegs[28]);
        ADD_REG128(WHvArm64RegisterQ29, pVCpu->cpum.GstCtx.aVRegs[29]);
        ADD_REG128(WHvArm64RegisterQ30, pVCpu->cpum.GstCtx.aVRegs[30]);
        ADD_REG128(WHvArm64RegisterQ31, pVCpu->cpum.GstCtx.aVRegs[31]);
    }

    if (fWhat & CPUMCTX_EXTRN_FPCR)
        ADD_REG64_RAW(WHvArm64RegisterFpcr, pVCpu->cpum.GstCtx.fpcr);
    if (fWhat & CPUMCTX_EXTRN_FPSR)
        ADD_REG64_RAW(WHvArm64RegisterFpsr, pVCpu->cpum.GstCtx.fpsr);

    /* System registers. */
    if (fWhat & CPUMCTX_EXTRN_SYSREG_MISC)
    {
        ADD_SYSREG64(WHvArm64RegisterVbarEl1,       pVCpu->cpum.GstCtx.VBar);
        ADD_SYSREG64(WHvArm64RegisterEsrEl1,        pVCpu->cpum.GstCtx.Esr);
        ADD_SYSREG64(WHvArm64RegisterFarEl1,        pVCpu->cpum.GstCtx.Far);
        ADD_SYSREG64(WHvArm64RegisterCntkctlEl1,    pVCpu->cpum.GstCtx.CntKCtl);
        ADD_SYSREG64(WHvArm64RegisterContextidrEl1, pVCpu->cpum.GstCtx.ContextIdr);
        ADD_SYSREG64(WHvArm64RegisterCpacrEl1,      pVCpu->cpum.GstCtx.Cpacr);
        ADD_SYSREG64(WHvArm64RegisterCsselrEl1,     pVCpu->cpum.GstCtx.Csselr);
        ADD_SYSREG64(WHvArm64RegisterMairEl1,       pVCpu->cpum.GstCtx.Mair);
        ADD_SYSREG64(WHvArm64RegisterParEl1,        pVCpu->cpum.GstCtx.Par);
        ADD_SYSREG64(WHvArm64RegisterTpidrroEl0,    pVCpu->cpum.GstCtx.TpIdrRoEl0);
        ADD_SYSREG64(WHvArm64RegisterTpidrEl0,      pVCpu->cpum.GstCtx.aTpIdr[0]);
        ADD_SYSREG64(WHvArm64RegisterTpidrEl1,      pVCpu->cpum.GstCtx.aTpIdr[1]);
        ADD_SYSREG64(My_WHvArm64RegisterActlrEl1,   pVCpu->cpum.GstCtx.Actlr);
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_DEBUG)
    {
        for (uint32_t i = 0; i < pVM->nem.s.cBreakpoints; i++)
        {
            ADD_SYSREG64((WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbcr0El1 + i), pVCpu->cpum.GstCtx.aBp[i].Ctrl);
            ADD_SYSREG64((WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbvr0El1 + i), pVCpu->cpum.GstCtx.aBp[i].Value);
        }

        for (uint32_t i = 0; i < pVM->nem.s.cWatchpoints; i++)
        {
            ADD_SYSREG64((WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwcr0El1 + i), pVCpu->cpum.GstCtx.aWp[i].Ctrl);
            ADD_SYSREG64((WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwvr0El1 + i), pVCpu->cpum.GstCtx.aWp[i].Value);
        }

        ADD_SYSREG64(WHvArm64RegisterMdscrEl1, pVCpu->cpum.GstCtx.Mdscr);
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_PAUTH_KEYS)
    {
        ADD_SYSREG64(WHvArm64RegisterApdAKeyHiEl1, pVCpu->cpum.GstCtx.Apda.High);
        ADD_SYSREG64(WHvArm64RegisterApdAKeyLoEl1, pVCpu->cpum.GstCtx.Apda.Low);
        ADD_SYSREG64(WHvArm64RegisterApdBKeyHiEl1, pVCpu->cpum.GstCtx.Apdb.High);
        ADD_SYSREG64(WHvArm64RegisterApdBKeyLoEl1, pVCpu->cpum.GstCtx.Apdb.Low);
        ADD_SYSREG64(WHvArm64RegisterApgAKeyHiEl1, pVCpu->cpum.GstCtx.Apga.High);
        ADD_SYSREG64(WHvArm64RegisterApgAKeyLoEl1, pVCpu->cpum.GstCtx.Apga.Low);
        ADD_SYSREG64(WHvArm64RegisterApiAKeyHiEl1, pVCpu->cpum.GstCtx.Apia.High);
        ADD_SYSREG64(WHvArm64RegisterApiAKeyLoEl1, pVCpu->cpum.GstCtx.Apia.Low);
        ADD_SYSREG64(WHvArm64RegisterApiBKeyHiEl1, pVCpu->cpum.GstCtx.Apib.High);
        ADD_SYSREG64(WHvArm64RegisterApiBKeyLoEl1, pVCpu->cpum.GstCtx.Apib.Low);
    }

#undef ADD_REG64
#undef ADD_REG64_RAW
#undef ADD_REG128

    /*
     * Set the registers.
     */
    Assert(iReg < RT_ELEMENTS(aValues));
    Assert(iReg < RT_ELEMENTS(aenmNames));
    HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, iReg, aValues);
    if (SUCCEEDED(hrc))
    {
        pVCpu->cpum.GstCtx.fExtrn |= CPUMCTX_EXTRN_ALL | CPUMCTX_EXTRN_KEEPER_NEM;
        return VINF_SUCCESS;
    }
    AssertLogRelMsgFailed(("WHvSetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, iReg,
                           hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_INTERNAL_ERROR;
}


NEM_TMPL_STATIC int nemHCWinCopyStateFromHyperV(PVMCC pVM, PVMCPUCC pVCpu, uint64_t fWhat)
{
    WHV_REGISTER_NAME  aenmNames[256];

    fWhat &= pVCpu->cpum.GstCtx.fExtrn;
    if (!fWhat)
        return VINF_SUCCESS;

    uintptr_t iReg = 0;

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_X0)
            aenmNames[iReg++] = WHvArm64RegisterX0;
        if (fWhat & CPUMCTX_EXTRN_X1)
            aenmNames[iReg++] = WHvArm64RegisterX1;
        if (fWhat & CPUMCTX_EXTRN_X2)
            aenmNames[iReg++] = WHvArm64RegisterX2;
        if (fWhat & CPUMCTX_EXTRN_X3)
            aenmNames[iReg++] = WHvArm64RegisterX3;
        if (fWhat & CPUMCTX_EXTRN_X4_X28)
        {
            aenmNames[iReg++] = WHvArm64RegisterX4;
            aenmNames[iReg++] = WHvArm64RegisterX5;
            aenmNames[iReg++] = WHvArm64RegisterX6;
            aenmNames[iReg++] = WHvArm64RegisterX7;
            aenmNames[iReg++] = WHvArm64RegisterX8;
            aenmNames[iReg++] = WHvArm64RegisterX9;
            aenmNames[iReg++] = WHvArm64RegisterX10;
            aenmNames[iReg++] = WHvArm64RegisterX11;
            aenmNames[iReg++] = WHvArm64RegisterX12;
            aenmNames[iReg++] = WHvArm64RegisterX13;
            aenmNames[iReg++] = WHvArm64RegisterX14;
            aenmNames[iReg++] = WHvArm64RegisterX15;
            aenmNames[iReg++] = WHvArm64RegisterX16;
            aenmNames[iReg++] = WHvArm64RegisterX17;
            aenmNames[iReg++] = WHvArm64RegisterX18;
            aenmNames[iReg++] = WHvArm64RegisterX19;
            aenmNames[iReg++] = WHvArm64RegisterX20;
            aenmNames[iReg++] = WHvArm64RegisterX21;
            aenmNames[iReg++] = WHvArm64RegisterX22;
            aenmNames[iReg++] = WHvArm64RegisterX23;
            aenmNames[iReg++] = WHvArm64RegisterX24;
            aenmNames[iReg++] = WHvArm64RegisterX25;
            aenmNames[iReg++] = WHvArm64RegisterX26;
            aenmNames[iReg++] = WHvArm64RegisterX27;
            aenmNames[iReg++] = WHvArm64RegisterX28;
        }
        if (fWhat & CPUMCTX_EXTRN_LR)
            aenmNames[iReg++] = WHvArm64RegisterLr;
        if (fWhat & CPUMCTX_EXTRN_FP)
            aenmNames[iReg++] = WHvArm64RegisterFp;
    }

    /* PC & Flags */
    if (fWhat & CPUMCTX_EXTRN_PC)
        aenmNames[iReg++] = WHvArm64RegisterPc;
    if (fWhat & CPUMCTX_EXTRN_PSTATE)
        aenmNames[iReg++] = WHvArm64RegisterPstate;
    if (fWhat & CPUMCTX_EXTRN_SPSR)
        aenmNames[iReg++] = WHvArm64RegisterSpsrEl1;
    if (fWhat & CPUMCTX_EXTRN_ELR)
        aenmNames[iReg++] = WHvArm64RegisterElrEl1;
    if (fWhat & CPUMCTX_EXTRN_SP)
    {
        aenmNames[iReg++] = WHvArm64RegisterSpEl0;
        aenmNames[iReg++] = WHvArm64RegisterSpEl1;
    }
    if (fWhat & CPUMCTX_EXTRN_SCTLR_TCR_TTBR)
    {
        aenmNames[iReg++] = WHvArm64RegisterSctlrEl1;
        aenmNames[iReg++] = WHvArm64RegisterTcrEl1;
        aenmNames[iReg++] = WHvArm64RegisterTtbr0El1;
        aenmNames[iReg++] = WHvArm64RegisterTtbr1El1;
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_V0_V31)
    {
        aenmNames[iReg++] = WHvArm64RegisterQ0;
        aenmNames[iReg++] = WHvArm64RegisterQ1;
        aenmNames[iReg++] = WHvArm64RegisterQ2;
        aenmNames[iReg++] = WHvArm64RegisterQ3;
        aenmNames[iReg++] = WHvArm64RegisterQ4;
        aenmNames[iReg++] = WHvArm64RegisterQ5;
        aenmNames[iReg++] = WHvArm64RegisterQ6;
        aenmNames[iReg++] = WHvArm64RegisterQ7;
        aenmNames[iReg++] = WHvArm64RegisterQ8;
        aenmNames[iReg++] = WHvArm64RegisterQ9;
        aenmNames[iReg++] = WHvArm64RegisterQ10;
        aenmNames[iReg++] = WHvArm64RegisterQ11;
        aenmNames[iReg++] = WHvArm64RegisterQ12;
        aenmNames[iReg++] = WHvArm64RegisterQ13;
        aenmNames[iReg++] = WHvArm64RegisterQ14;
        aenmNames[iReg++] = WHvArm64RegisterQ15;

        aenmNames[iReg++] = WHvArm64RegisterQ16;
        aenmNames[iReg++] = WHvArm64RegisterQ17;
        aenmNames[iReg++] = WHvArm64RegisterQ18;
        aenmNames[iReg++] = WHvArm64RegisterQ19;
        aenmNames[iReg++] = WHvArm64RegisterQ20;
        aenmNames[iReg++] = WHvArm64RegisterQ21;
        aenmNames[iReg++] = WHvArm64RegisterQ22;
        aenmNames[iReg++] = WHvArm64RegisterQ23;
        aenmNames[iReg++] = WHvArm64RegisterQ24;
        aenmNames[iReg++] = WHvArm64RegisterQ25;
        aenmNames[iReg++] = WHvArm64RegisterQ26;
        aenmNames[iReg++] = WHvArm64RegisterQ27;
        aenmNames[iReg++] = WHvArm64RegisterQ28;
        aenmNames[iReg++] = WHvArm64RegisterQ29;
        aenmNames[iReg++] = WHvArm64RegisterQ30;
        aenmNames[iReg++] = WHvArm64RegisterQ31;
    }
    if (fWhat & CPUMCTX_EXTRN_FPCR)
        aenmNames[iReg++] = WHvArm64RegisterFpcr;
    if (fWhat & CPUMCTX_EXTRN_FPSR)
        aenmNames[iReg++] = WHvArm64RegisterFpsr;

    /* System registers. */
    if (fWhat & CPUMCTX_EXTRN_SYSREG_MISC)
    {
        aenmNames[iReg++] = WHvArm64RegisterVbarEl1;
        aenmNames[iReg++] = WHvArm64RegisterEsrEl1;
        aenmNames[iReg++] = WHvArm64RegisterFarEl1;
        aenmNames[iReg++] = WHvArm64RegisterCntkctlEl1;
        aenmNames[iReg++] = WHvArm64RegisterContextidrEl1;
        aenmNames[iReg++] = WHvArm64RegisterCpacrEl1;
        aenmNames[iReg++] = WHvArm64RegisterCsselrEl1;
        aenmNames[iReg++] = WHvArm64RegisterMairEl1;
        aenmNames[iReg++] = WHvArm64RegisterParEl1;
        aenmNames[iReg++] = WHvArm64RegisterTpidrroEl0;
        aenmNames[iReg++] = WHvArm64RegisterTpidrEl0;
        aenmNames[iReg++] = WHvArm64RegisterTpidrEl1;
        aenmNames[iReg++] = My_WHvArm64RegisterActlrEl1;
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_DEBUG)
    {
        /* Hyper-V doesn't allow syncing debug break-/watchpoint registers which aren't there. */
        for (uint32_t i = 0; i < pVM->nem.s.cBreakpoints; i++)
        {
            aenmNames[iReg++] = (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbcr0El1 + i);
            aenmNames[iReg++] = (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbvr0El1 + i);
        }

        for (uint32_t i = 0; i < pVM->nem.s.cWatchpoints; i++)
        {
            aenmNames[iReg++] = (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwcr0El1 + i);
            aenmNames[iReg++] = (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwvr0El1 + i);
        }

        aenmNames[iReg++] = WHvArm64RegisterMdscrEl1;
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_PAUTH_KEYS)
    {
        aenmNames[iReg++] = WHvArm64RegisterApdAKeyHiEl1;
        aenmNames[iReg++] = WHvArm64RegisterApdAKeyLoEl1;
        aenmNames[iReg++] = WHvArm64RegisterApdBKeyHiEl1;
        aenmNames[iReg++] = WHvArm64RegisterApdBKeyLoEl1;
        aenmNames[iReg++] = WHvArm64RegisterApgAKeyHiEl1;
        aenmNames[iReg++] = WHvArm64RegisterApgAKeyLoEl1;
        aenmNames[iReg++] = WHvArm64RegisterApiAKeyHiEl1;
        aenmNames[iReg++] = WHvArm64RegisterApiAKeyLoEl1;
        aenmNames[iReg++] = WHvArm64RegisterApiBKeyHiEl1;
        aenmNames[iReg++] = WHvArm64RegisterApiBKeyLoEl1;
    }

    size_t const cRegs = iReg;
    Assert(cRegs < RT_ELEMENTS(aenmNames));

    /*
     * Get the registers.
     */
    WHV_REGISTER_VALUE aValues[256];
    RT_ZERO(aValues);
    Assert(RT_ELEMENTS(aValues) >= cRegs);
    Assert(RT_ELEMENTS(aenmNames) >= cRegs);
    HRESULT hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, aenmNames, (uint32_t)cRegs, aValues);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvGetVirtualProcessorRegisters(%p, %u,,%u,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, cRegs, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);

    iReg = 0;
#define GET_REG64(a_DstVar, a_enmName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            (a_DstVar).x = aValues[iReg].Reg64; \
            iReg++; \
        } while (0)
#define GET_REG64_RAW(a_DstVar, a_enmName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            (a_DstVar) = aValues[iReg].Reg64; \
            iReg++; \
        } while (0)
#define GET_SYSREG64(a_DstVar, a_enmName) do { \
            Assert(aenmNames[iReg] == (a_enmName)); \
            (a_DstVar).u64 = aValues[iReg].Reg64; \
            iReg++; \
        } while (0)
#define GET_REG128(a_DstVar, a_enmName) do { \
            Assert(aenmNames[iReg] == a_enmName); \
            (a_DstVar).au64[0] = aValues[iReg].Reg128.Low64; \
            (a_DstVar).au64[1] = aValues[iReg].Reg128.High64; \
            iReg++; \
        } while (0)

    /* GPRs */
    if (fWhat & CPUMCTX_EXTRN_GPRS_MASK)
    {
        if (fWhat & CPUMCTX_EXTRN_X0)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[0], WHvArm64RegisterX0);
        if (fWhat & CPUMCTX_EXTRN_X1)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[1], WHvArm64RegisterX1);
        if (fWhat & CPUMCTX_EXTRN_X2)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[2], WHvArm64RegisterX2);
        if (fWhat & CPUMCTX_EXTRN_X3)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[3], WHvArm64RegisterX3);
        if (fWhat & CPUMCTX_EXTRN_X4_X28)
        {
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[4],  WHvArm64RegisterX4);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[5],  WHvArm64RegisterX5);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[6],  WHvArm64RegisterX6);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[7],  WHvArm64RegisterX7);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[8],  WHvArm64RegisterX8);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[9],  WHvArm64RegisterX9);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[10], WHvArm64RegisterX10);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[11], WHvArm64RegisterX11);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[12], WHvArm64RegisterX12);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[13], WHvArm64RegisterX13);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[14], WHvArm64RegisterX14);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[15], WHvArm64RegisterX15);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[16], WHvArm64RegisterX16);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[17], WHvArm64RegisterX17);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[18], WHvArm64RegisterX18);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[19], WHvArm64RegisterX19);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[20], WHvArm64RegisterX20);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[21], WHvArm64RegisterX21);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[22], WHvArm64RegisterX22);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[23], WHvArm64RegisterX23);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[24], WHvArm64RegisterX24);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[25], WHvArm64RegisterX25);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[26], WHvArm64RegisterX26);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[27], WHvArm64RegisterX27);
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[28], WHvArm64RegisterX28);
        }
        if (fWhat & CPUMCTX_EXTRN_LR)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[30], WHvArm64RegisterLr);
        if (fWhat & CPUMCTX_EXTRN_FP)
            GET_REG64(pVCpu->cpum.GstCtx.aGRegs[29], WHvArm64RegisterFp);
    }

    /* RIP & Flags */
    if (fWhat & CPUMCTX_EXTRN_PC)
        GET_REG64_RAW(pVCpu->cpum.GstCtx.Pc.u64, WHvArm64RegisterPc);
    if (fWhat & CPUMCTX_EXTRN_PSTATE)
        GET_REG64_RAW(pVCpu->cpum.GstCtx.fPState, WHvArm64RegisterPstate);
    if (fWhat & CPUMCTX_EXTRN_SPSR)
        GET_SYSREG64(pVCpu->cpum.GstCtx.Spsr, WHvArm64RegisterSpsrEl1);
    if (fWhat & CPUMCTX_EXTRN_ELR)
        GET_SYSREG64(pVCpu->cpum.GstCtx.Elr, WHvArm64RegisterElrEl1);
    if (fWhat & CPUMCTX_EXTRN_SP)
    {
        GET_SYSREG64(pVCpu->cpum.GstCtx.aSpReg[0], WHvArm64RegisterSpEl0);
        GET_SYSREG64(pVCpu->cpum.GstCtx.aSpReg[1], WHvArm64RegisterSpEl1);
    }
    if (fWhat & CPUMCTX_EXTRN_SCTLR_TCR_TTBR)
    {
        GET_SYSREG64(pVCpu->cpum.GstCtx.Sctlr, WHvArm64RegisterSctlrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Tcr, WHvArm64RegisterTcrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Ttbr0, WHvArm64RegisterTtbr0El1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Ttbr1, WHvArm64RegisterTtbr1El1);
    }

    /* Vector state. */
    if (fWhat & CPUMCTX_EXTRN_V0_V31)
    {
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[0],  WHvArm64RegisterQ0);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[1],  WHvArm64RegisterQ1);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[2],  WHvArm64RegisterQ2);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[3],  WHvArm64RegisterQ3);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[4],  WHvArm64RegisterQ4);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[5],  WHvArm64RegisterQ5);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[6],  WHvArm64RegisterQ6);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[7],  WHvArm64RegisterQ7);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[8],  WHvArm64RegisterQ8);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[9],  WHvArm64RegisterQ9);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[10], WHvArm64RegisterQ10);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[11], WHvArm64RegisterQ11);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[12], WHvArm64RegisterQ12);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[13], WHvArm64RegisterQ13);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[14], WHvArm64RegisterQ14);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[15], WHvArm64RegisterQ15);

        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[16], WHvArm64RegisterQ16);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[17], WHvArm64RegisterQ17);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[18], WHvArm64RegisterQ18);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[19], WHvArm64RegisterQ19);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[20], WHvArm64RegisterQ20);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[21], WHvArm64RegisterQ21);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[22], WHvArm64RegisterQ22);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[23], WHvArm64RegisterQ23);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[24], WHvArm64RegisterQ24);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[25], WHvArm64RegisterQ25);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[26], WHvArm64RegisterQ26);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[27], WHvArm64RegisterQ27);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[28], WHvArm64RegisterQ28);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[29], WHvArm64RegisterQ29);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[30], WHvArm64RegisterQ30);
        GET_REG128(pVCpu->cpum.GstCtx.aVRegs[31], WHvArm64RegisterQ31);
    }
    if (fWhat & CPUMCTX_EXTRN_FPCR)
        GET_REG64_RAW(pVCpu->cpum.GstCtx.fpcr, WHvArm64RegisterFpcr);
    if (fWhat & CPUMCTX_EXTRN_FPSR)
        GET_REG64_RAW(pVCpu->cpum.GstCtx.fpsr, WHvArm64RegisterFpsr);

    /* System registers. */
    if (fWhat & CPUMCTX_EXTRN_SYSREG_MISC)
    {
        GET_SYSREG64(pVCpu->cpum.GstCtx.VBar,       WHvArm64RegisterVbarEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Esr,        WHvArm64RegisterEsrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Far,        WHvArm64RegisterFarEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.CntKCtl,    WHvArm64RegisterCntkctlEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.ContextIdr, WHvArm64RegisterContextidrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Cpacr,      WHvArm64RegisterCpacrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Csselr,     WHvArm64RegisterCsselrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Mair,       WHvArm64RegisterMairEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Par,        WHvArm64RegisterParEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.TpIdrRoEl0, WHvArm64RegisterTpidrroEl0);
        GET_SYSREG64(pVCpu->cpum.GstCtx.aTpIdr[0],  WHvArm64RegisterTpidrEl0);
        GET_SYSREG64(pVCpu->cpum.GstCtx.aTpIdr[1],  WHvArm64RegisterTpidrEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Actlr,      My_WHvArm64RegisterActlrEl1);
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_DEBUG)
    {
        for (uint32_t i = 0; i < pVM->nem.s.cBreakpoints; i++)
        {
            GET_SYSREG64(pVCpu->cpum.GstCtx.aBp[i].Ctrl, (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbcr0El1 + i));
            GET_SYSREG64(pVCpu->cpum.GstCtx.aBp[i].Value, (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgbvr0El1 + i));
        }

        for (uint32_t i = 0; i < pVM->nem.s.cWatchpoints; i++)
        {
            GET_SYSREG64(pVCpu->cpum.GstCtx.aWp[i].Ctrl, (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwcr0El1 + i));
            GET_SYSREG64(pVCpu->cpum.GstCtx.aWp[i].Value, (WHV_REGISTER_NAME)((uint32_t)WHvArm64RegisterDbgwvr0El1 + i));
        }

        GET_SYSREG64(pVCpu->cpum.GstCtx.Mdscr, WHvArm64RegisterMdscrEl1);
    }

    if (fWhat & CPUMCTX_EXTRN_SYSREG_PAUTH_KEYS)
    {
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apda.High, WHvArm64RegisterApdAKeyHiEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apda.Low,  WHvArm64RegisterApdAKeyLoEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apdb.High, WHvArm64RegisterApdBKeyHiEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apdb.Low,  WHvArm64RegisterApdBKeyLoEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apga.High, WHvArm64RegisterApgAKeyHiEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apga.Low,  WHvArm64RegisterApgAKeyLoEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apia.High, WHvArm64RegisterApiAKeyHiEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apia.Low,  WHvArm64RegisterApiAKeyLoEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apib.High, WHvArm64RegisterApiBKeyHiEl1);
        GET_SYSREG64(pVCpu->cpum.GstCtx.Apib.Low,  WHvArm64RegisterApiBKeyLoEl1);
    }

    /* Almost done, just update extrn flags. */
    pVCpu->cpum.GstCtx.fExtrn &= ~fWhat;
    if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL))
        pVCpu->cpum.GstCtx.fExtrn = 0;

    return VINF_SUCCESS;
}


/**
 * Interface for importing state on demand (used by IEM).
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   fWhat       What to import, CPUMCTX_EXTRN_XXX.
 */
VMM_INT_DECL(int) NEMImportStateOnDemand(PVMCPUCC pVCpu, uint64_t fWhat)
{
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnDemand);
    return nemHCWinCopyStateFromHyperV(pVCpu->pVMR3, pVCpu, fWhat);
}


/**
 * Query the CPU tick counter and optionally the TSC_AUX MSR value.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context CPU structure.
 * @param   pcTicks     Where to return the CPU tick count.
 * @param   puAux       Where to return the TSC_AUX register value.
 */
VMM_INT_DECL(int) NEMHCQueryCpuTick(PVMCPUCC pVCpu, uint64_t *pcTicks, uint32_t *puAux)
{
    STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatQueryCpuTick);

    PVMCC pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT_RETURN(pVCpu, VERR_VM_THREAD_NOT_EMT);
    AssertReturn(VM_IS_NEM_ENABLED(pVM), VERR_NEM_IPE_9);

    /* Ensure time for the partition is suspended - it will be resumed as soon as a vCPU starts executing. */
    HRESULT hrc = WHvSuspendPartitionTime(pVM->nem.s.hPartition);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvSuspendPartitionTime(%p) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);

    /* Call the offical API. */
    WHV_REGISTER_NAME  enmName = WHvArm64RegisterCntvctEl0;
    WHV_REGISTER_VALUE Value   = { { {0, 0} } };
    hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, &enmName, 1, &Value);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvGetVirtualProcessorRegisters(%p, %u,{CNTVCT_EL0},1,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);
    *pcTicks = Value.Reg64;
    LogFlow(("NEMHCQueryCpuTick: %#RX64 (host: %#RX64)\n", *pcTicks, ASMReadTSC()));
    if (puAux)
        *puAux =0;

    return VINF_SUCCESS;
}


/**
 * Resumes CPU clock (TSC) on all virtual CPUs.
 *
 * This is called by TM when the VM is started, restored, resumed or similar.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context CPU structure of the calling EMT.
 * @param   uPausedTscValue The TSC value at the time of pausing.
 */
VMM_INT_DECL(int) NEMHCResumeCpuTickOnAll(PVMCC pVM, PVMCPUCC pVCpu, uint64_t uPausedTscValue)
{
    VMCPU_ASSERT_EMT_RETURN(pVCpu, VERR_VM_THREAD_NOT_EMT);
    AssertReturn(VM_IS_NEM_ENABLED(pVM), VERR_NEM_IPE_9);

    /*
     * Call the offical API to do the job.
     */
    LogFlow(("NEMHCResumeCpuTickOnAll: %#RX64 (host: %#RX64)\n", uPausedTscValue, ASMReadTSC()));

    /*
     * Now set the CNTVCT_EL0 register for each vCPU, Hyper-V will program the timer offset in
     * CNTVOFF_EL2 accordingly. ARM guarantees that CNTVCT_EL0 is synchronised across all CPUs,
     * as long as CNTVOFF_EL2 is the same everywhere. Lets just hope scheduling will not affect it
     * if the partition time is suspended.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        WHV_REGISTER_NAME enmName = WHvArm64RegisterCntvctEl0;
        WHV_REGISTER_VALUE Value;
        Value.Reg64 = uPausedTscValue;
        HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, idCpu, &enmName, 1, &Value);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorRegisters(%p, 0,{CNTVCT_EL0},1,%#RX64) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, idCpu, uPausedTscValue, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_SET_TSC);

        /* Make sure the CNTV_CTL_EL0 and CNTV_CVAL_EL0 registers are up to date after resuming (saved state load). */
        PVMCPUCC pVCpuDst = pVM->apCpusR3[idCpu];
        pVCpuDst->nem.s.fSyncCntvRegs = true;
    }

    HRESULT hrc = WHvResumePartitionTime(pVM->nem.s.hPartition);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvResumePartitionTime(%p) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_SET_TSC);

    return VINF_SUCCESS;
}


#ifdef LOG_ENABLED
/**
 * Logs the current CPU state.
 */
static void nemR3WinLogState(PVMCC pVM, PVMCPUCC pVCpu)
{
    if (LogIs3Enabled())
    {
        char szRegs[4096];
        DBGFR3RegPrintf(pVM->pUVM, pVCpu->idCpu, &szRegs[0], sizeof(szRegs),
                        "x0=%016VR{x0} x1=%016VR{x1} x2=%016VR{x2} x3=%016VR{x3}\n"
                        "x4=%016VR{x4} x5=%016VR{x5} x6=%016VR{x6} x7=%016VR{x7}\n"
                        "x8=%016VR{x8} x9=%016VR{x9} x10=%016VR{x10} x11=%016VR{x11}\n"
                        "x12=%016VR{x12} x13=%016VR{x13} x14=%016VR{x14} x15=%016VR{x15}\n"
                        "x16=%016VR{x16} x17=%016VR{x17} x18=%016VR{x18} x19=%016VR{x19}\n"
                        "x20=%016VR{x20} x21=%016VR{x21} x22=%016VR{x22} x23=%016VR{x23}\n"
                        "x24=%016VR{x24} x25=%016VR{x25} x26=%016VR{x26} x27=%016VR{x27}\n"
                        "x28=%016VR{x28} x29=%016VR{x29} x30=%016VR{x30}\n"
                        "pc=%016VR{pc} pstate=%016VR{pstate}\n"
                        "sp_el0=%016VR{sp_el0} sp_el1=%016VR{sp_el1} elr_el1=%016VR{elr_el1}\n"
                        "sctlr_el1=%016VR{sctlr_el1} tcr_el1=%016VR{tcr_el1}\n"
                        "ttbr0_el1=%016VR{ttbr0_el1} ttbr1_el1=%016VR{ttbr1_el1}\n"
                        "vbar_el1=%016VR{vbar_el1}\n"
                        );
        char szInstr[256]; RT_ZERO(szInstr);
        DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, 0, 0,
                           DBGF_DISAS_FLAGS_CURRENT_GUEST | DBGF_DISAS_FLAGS_DEFAULT_MODE,
                           szInstr, sizeof(szInstr), NULL);
        Log3(("%s%s\n", szRegs, szInstr));
    }
}
#endif /* LOG_ENABLED */


/**
 * Copies register state from the (common) exit context.
 *
 * ASSUMES no state copied yet.
 *
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pMsgHdr         The common message header.
 */
DECLINLINE(void) nemR3WinCopyStateFromArmHeader(PVMCPUCC pVCpu, WHV_INTERCEPT_MESSAGE_HEADER const *pMsgHdr)
{
#ifdef LOG_ENABLED /* When state logging is enabled the state is synced completely upon VM exit. */
    if (!LogIs3Enabled())
#endif
        Assert(   (pVCpu->cpum.GstCtx.fExtrn & (CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_PSTATE))
               ==                              (CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_PSTATE));

    pVCpu->cpum.GstCtx.Pc.u64   = pMsgHdr->Pc;
    pVCpu->cpum.GstCtx.fPState  = pMsgHdr->Cpsr;

    pVCpu->cpum.GstCtx.fExtrn &= ~(CPUMCTX_EXTRN_PC | CPUMCTX_EXTRN_PSTATE);
}


/**
 * Returns the byte size from the given access SAS value.
 *
 * @returns Number of bytes to transfer.
 * @param   uSas            The SAS value to convert.
 */
DECLINLINE(size_t) nemR3WinGetByteCountFromSas(uint8_t uSas)
{
    switch (uSas)
    {
        case ARMV8_EC_ISS_DATA_ABRT_SAS_BYTE:     return sizeof(uint8_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_HALFWORD: return sizeof(uint16_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_WORD:     return sizeof(uint32_t);
        case ARMV8_EC_ISS_DATA_ABRT_SAS_DWORD:    return sizeof(uint64_t);
        default:
            AssertReleaseFailed();
    }

    return 0;
}


/**
 * Sets the given general purpose register to the given value.
 *
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uReg            The register index.
 * @param   f64BitReg       Flag whether to operate on a 64-bit or 32-bit register.
 * @param   fSignExtend     Flag whether to sign extend the value.
 * @param   u64Val          The value.
 */
DECLINLINE(void) nemR3WinSetGReg(PVMCPU pVCpu, uint8_t uReg, bool f64BitReg, bool fSignExtend, uint64_t u64Val)
{
    AssertReturnVoid(uReg < 31);

    if (f64BitReg)
        pVCpu->cpum.GstCtx.aGRegs[uReg].x = fSignExtend ? (int64_t)u64Val : u64Val;
    else
        pVCpu->cpum.GstCtx.aGRegs[uReg].x = (uint64_t)(fSignExtend ? (int32_t)u64Val : (uint32_t)u64Val);

    /* Mark the register as not extern anymore. */
    switch (uReg)
    {
        case 0:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X0;
            break;
        case 1:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X1;
            break;
        case 2:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X2;
            break;
        case 3:
            pVCpu->cpum.GstCtx.fExtrn &= ~CPUMCTX_EXTRN_X3;
            break;
        default:
            AssertRelease(!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_X4_X28));
            /** @todo We need to import all missing registers in order to clear this flag (or just set it in HV from here). */
    }
}


/**
 * Gets the given general purpose register and returns the value.
 *
 * @returns Value from the given register.
 * @param   pVCpu           The cross context virtual CPU structure of the
 *                          calling EMT.
 * @param   uReg            The register index.
 */
DECLINLINE(uint64_t) nemR3WinGetGReg(PVMCPU pVCpu, uint8_t uReg)
{
    AssertReturn(uReg <= ARMV8_A64_REG_XZR, 0);

    if (uReg == ARMV8_A64_REG_XZR)
        return 0;

    /** @todo Import the register if extern. */
    AssertRelease(!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_GPRS_MASK));

    return pVCpu->cpum.GstCtx.aGRegs[uReg].x;
}


/**
 * Deals with memory access exits (WHvRunVpExitReasonMemoryAccess).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @sa      nemHCWinHandleMessageMemory
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitMemory(PVMCC pVM, PVMCPUCC pVCpu, MY_WHV_RUN_VP_EXIT_CONTEXT const *pExit)
{
    uint64_t const uHostTsc = ASMReadTSC();
    Assert(pExit->MemoryAccess.Header.InterceptAccessType != 3);

    /*
     * Emulate the memory access, either access handler or special memory.
     */
    WHV_INTERCEPT_MESSAGE_HEADER const *pHdr = &pExit->MemoryAccess.Header;
    PCEMEXITREC pExitRec = EMHistoryAddExit(pVCpu,
                                              pExit->MemoryAccess.Header.InterceptAccessType == WHvMemoryAccessWrite
                                            ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_WRITE)
                                            : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_READ),
                                            pHdr->Pc, uHostTsc);
    nemR3WinCopyStateFromArmHeader(pVCpu, &pExit->MemoryAccess.Header);
    RT_NOREF_PV(pExitRec);
    int rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, IEM_CPUMCTX_EXTRN_MUST_MASK);
    AssertRCReturn(rc, rc);

#ifdef LOG_ENABLED
    uint8_t const cbInstr = pExit->MemoryAccess.InstructionByteCount;
    RTGCPTR const GCPtrVa = pExit->MemoryAccess.Gva;
#endif
    RTGCPHYS const GCPhys = pExit->MemoryAccess.Gpa;
    uint64_t const uIss   = pExit->MemoryAccess.Syndrome;
    bool fIsv        = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_ISV);
    bool fL2Fault    = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_S1PTW);
    bool fWrite      = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_WNR);
    bool f64BitReg   = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_SF);
    bool fSignExtend = RT_BOOL(uIss & ARMV8_EC_ISS_DATA_ABRT_SSE);
    uint8_t uReg     = ARMV8_EC_ISS_DATA_ABRT_SRT_GET(uIss);
    uint8_t uAcc     = ARMV8_EC_ISS_DATA_ABRT_SAS_GET(uIss);
    size_t cbAcc     = nemR3WinGetByteCountFromSas(uAcc);
    LogFlowFunc(("fIsv=%RTbool fL2Fault=%RTbool fWrite=%RTbool f64BitReg=%RTbool fSignExtend=%RTbool uReg=%u uAcc=%u GCPtrDataAbrt=%RGv GCPhys=%RGp cbInstr=%u\n",
                 fIsv, fL2Fault, fWrite, f64BitReg, fSignExtend, uReg, uAcc, GCPtrVa, GCPhys, cbInstr));

    RT_NOREF(fL2Fault);

    VBOXSTRICTRC rcStrict;
    if (fIsv)
    {
        EMHistoryAddExit(pVCpu,
                         fWrite
                         ? EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_WRITE)
                         : EMEXIT_MAKE_FT(EMEXIT_F_KIND_EM, EMEXITTYPE_MMIO_READ),
                         pVCpu->cpum.GstCtx.Pc.u64, ASMReadTSC());

        uint64_t u64Val = 0;
        if (fWrite)
        {
            u64Val = nemR3WinGetGReg(pVCpu, uReg);
            rcStrict = PGMPhysWrite(pVM, GCPhys, &u64Val, cbAcc, PGMACCESSORIGIN_HM);
            Log4(("MmioExit/%u: %08RX64: WRITE %RGp LB %u, %.*Rhxs -> rcStrict=%Rrc\n",
                  pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhys, cbAcc, cbAcc,
                  &u64Val, VBOXSTRICTRC_VAL(rcStrict) ));
        }
        else
        {
            rcStrict = PGMPhysRead(pVM, GCPhys, &u64Val, cbAcc, PGMACCESSORIGIN_HM);
            Log4(("MmioExit/%u: %08RX64: READ %RGp LB %u -> %.*Rhxs rcStrict=%Rrc\n",
                  pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhys, cbAcc, cbAcc,
                  &u64Val, VBOXSTRICTRC_VAL(rcStrict) ));
            if (rcStrict == VINF_SUCCESS)
                nemR3WinSetGReg(pVCpu, uReg, f64BitReg, fSignExtend, u64Val);
        }
    }
    else
    {
        /** @todo Our UEFI firmware accesses the flash region with the following instruction
         *        when the NVRAM actually contains data:
         *             ldrb w9, [x6, #-0x0001]!
         *        This is too complicated for the hardware so the ISV bit is not set. Until there
         *        is a proper IEM implementation we just handle this here for now to avoid annoying
         *        users too much.
         */
        /* The following ASSUMES that the vCPU state is completely synced. */

        /* Read instruction. */
        RTGCPTR GCPtrPage = pVCpu->cpum.GstCtx.Pc.u64 & ~(RTGCPTR)GUEST_PAGE_OFFSET_MASK;
        const void *pvPageR3 = NULL;
        PGMPAGEMAPLOCK  PageMapLock;

        rcStrict = PGMPhysGCPtr2CCPtrReadOnly(pVCpu, GCPtrPage, &pvPageR3, &PageMapLock);
        if (rcStrict == VINF_SUCCESS)
        {
            uint32_t u32Instr = *(uint32_t *)((uint8_t *)pvPageR3 + (pVCpu->cpum.GstCtx.Pc.u64 - GCPtrPage));
            PGMPhysReleasePageMappingLock(pVCpu->pVMR3, &PageMapLock);

            DISSTATE Dis;
            rcStrict = DISInstrWithPrefetchedBytes((uintptr_t)pVCpu->cpum.GstCtx.Pc.u64, DISCPUMODE_ARMV8_A64,  0 /*fFilter - none */,
                                                   &u32Instr, sizeof(u32Instr), NULL, NULL, &Dis, NULL);
            if (rcStrict == VINF_SUCCESS)
            {
                if (   Dis.pCurInstr->uOpcode == OP_ARMV8_A64_LDRB
                    && Dis.aParams[0].armv8.enmType == kDisArmv8OpParmReg
                    && Dis.aParams[0].armv8.Op.Reg.enmRegType == kDisOpParamArmV8RegType_Gpr_32Bit
                    && Dis.aParams[1].armv8.enmType == kDisArmv8OpParmAddrInGpr
                    && Dis.aParams[1].armv8.Op.Reg.enmRegType == kDisOpParamArmV8RegType_Gpr_64Bit
                    && (Dis.aParams[1].fUse & DISUSE_PRE_INDEXED))
                {
                    /* The fault address is already the final address. */
                    uint8_t bVal = 0;
                    rcStrict = PGMPhysRead(pVM, GCPhys, &bVal, 1, PGMACCESSORIGIN_HM);
                    Log4(("MmioExit/%u: %08RX64: READ %#RGp LB %u -> %.*Rhxs rcStrict=%Rrc\n",
                          pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhys, sizeof(bVal), sizeof(bVal),
                          &bVal, VBOXSTRICTRC_VAL(rcStrict) ));
                    if (rcStrict == VINF_SUCCESS)
                    {
                        nemR3WinSetGReg(pVCpu, Dis.aParams[0].armv8.Op.Reg.idReg, false /*f64BitReg*/, false /*fSignExtend*/, bVal);
                        /* Update the indexed register. */
                        pVCpu->cpum.GstCtx.aGRegs[Dis.aParams[1].armv8.Op.Reg.idReg].x += Dis.aParams[1].armv8.u.offBase;
                    }
                }
                /*
                 * Seeing the following with the Windows 11/ARM TPM driver:
                 *     %fffff800e5342888 48 25 45 29             ldp w8, w9, [x10, #+0x0028]
                 */
                else if (   Dis.pCurInstr->uOpcode == OP_ARMV8_A64_LDP
                         && Dis.aParams[0].armv8.enmType == kDisArmv8OpParmReg
                         && Dis.aParams[0].armv8.Op.Reg.enmRegType == kDisOpParamArmV8RegType_Gpr_32Bit
                         && Dis.aParams[1].armv8.enmType == kDisArmv8OpParmReg
                         && Dis.aParams[1].armv8.Op.Reg.enmRegType == kDisOpParamArmV8RegType_Gpr_32Bit
                         && Dis.aParams[2].armv8.enmType == kDisArmv8OpParmAddrInGpr
                         && Dis.aParams[2].armv8.Op.Reg.enmRegType == kDisOpParamArmV8RegType_Gpr_64Bit)
                {
                    /** @todo This is tricky to handle if the first register read returns something else than VINF_SUCCESS... */
                    /* The fault address is already the final address. */
                    uint32_t u32Val1 = 0;
                    uint32_t u32Val2 = 0;
                    rcStrict = PGMPhysRead(pVM, GCPhys, &u32Val1, sizeof(u32Val1), PGMACCESSORIGIN_HM);
                    if (rcStrict == VINF_SUCCESS)
                        rcStrict = PGMPhysRead(pVM, GCPhys + sizeof(uint32_t), &u32Val2, sizeof(u32Val2), PGMACCESSORIGIN_HM);
                    Log4(("MmioExit/%u: %08RX64: READ %#RGp LB %u -> %.*Rhxs %.*Rhxs rcStrict=%Rrc\n",
                          pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64, GCPhys, 2 * sizeof(uint32_t), sizeof(u32Val1),
                          &u32Val1, sizeof(u32Val2), &u32Val2, VBOXSTRICTRC_VAL(rcStrict) ));
                    if (rcStrict == VINF_SUCCESS)
                    {
                        nemR3WinSetGReg(pVCpu, Dis.aParams[0].armv8.Op.Reg.idReg, false /*f64BitReg*/, false /*fSignExtend*/, u32Val1);
                        nemR3WinSetGReg(pVCpu, Dis.aParams[1].armv8.Op.Reg.idReg, false /*f64BitReg*/, false /*fSignExtend*/, u32Val2);
                    }
                }
                else
                    AssertFailedReturn(VERR_NOT_SUPPORTED);
            }
        }
    }

    if (rcStrict == VINF_SUCCESS)
        pVCpu->cpum.GstCtx.Pc.u64 += sizeof(uint32_t); /** @todo Why is InstructionByteCount always 0? */

    return rcStrict;
}


/**
 * Deals with memory access exits (WHvRunVpExitReasonMemoryAccess).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @sa      nemHCWinHandleMessageMemory
 */
NEM_TMPL_STATIC VBOXSTRICTRC
nemR3WinHandleExitHypercall(PVMCC pVM, PVMCPUCC pVCpu, MY_WHV_RUN_VP_EXIT_CONTEXT const *pExit)
{
    VBOXSTRICTRC rcStrict = VINF_SUCCESS;

    /** @todo Raise exception to EL1 if PSCI not configured. */
    /** @todo Need a generic mechanism here to pass this to, GIM maybe?. */
    uint32_t uFunId = pExit->Hypercall.Immediate;
    bool fHvc64 = RT_BOOL(uFunId & ARM_SMCCC_FUNC_ID_64BIT); RT_NOREF(fHvc64);
    uint32_t uEntity = ARM_SMCCC_FUNC_ID_ENTITY_GET(uFunId);
    uint32_t uFunNum = ARM_SMCCC_FUNC_ID_NUM_GET(uFunId);
    if (uEntity == ARM_SMCCC_FUNC_ID_ENTITY_STD_SEC_SERVICE)
    {
        switch (uFunNum)
        {
            case ARM_PSCI_FUNC_ID_PSCI_VERSION:
                nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0, false /*f64BitReg*/, false /*fSignExtend*/, ARM_PSCI_FUNC_ID_PSCI_VERSION_SET(1, 2));
                break;
            case ARM_PSCI_FUNC_ID_SYSTEM_OFF:
                rcStrict = VMR3PowerOff(pVM->pUVM);
                break;
            case ARM_PSCI_FUNC_ID_SYSTEM_RESET:
            case ARM_PSCI_FUNC_ID_SYSTEM_RESET2:
            {
                bool fHaltOnReset;
                int rc = CFGMR3QueryBool(CFGMR3GetChild(CFGMR3GetRoot(pVM), "PDM"), "HaltOnReset", &fHaltOnReset);
                if (RT_SUCCESS(rc) && fHaltOnReset)
                {
                    Log(("nemHCLnxHandleExitHypercall: Halt On Reset!\n"));
                    rcStrict = VINF_EM_HALT;
                }
                else
                {
                    /** @todo pVM->pdm.s.fResetFlags = fFlags; */
                    VM_FF_SET(pVM, VM_FF_RESET);
                    rcStrict = VINF_EM_RESET;
                }
                break;
            }
            case ARM_PSCI_FUNC_ID_CPU_ON:
            {
                uint64_t u64TgtCpu      = pExit->Hypercall.X[1];
                RTGCPHYS GCPhysExecAddr = pExit->Hypercall.X[2];
                uint64_t u64CtxId       = pExit->Hypercall.X[3];
                VMMR3CpuOn(pVM, u64TgtCpu & 0xff, GCPhysExecAddr, u64CtxId);
                nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0, true /*f64BitReg*/, false /*fSignExtend*/, ARM_PSCI_STS_SUCCESS);
                break;
            }
            case ARM_PSCI_FUNC_ID_PSCI_FEATURES:
            {
                uint32_t u32FunNum = (uint32_t)pExit->Hypercall.X[1];
                switch (u32FunNum)
                {
                    case ARM_PSCI_FUNC_ID_PSCI_VERSION:
                    case ARM_PSCI_FUNC_ID_SYSTEM_OFF:
                    case ARM_PSCI_FUNC_ID_SYSTEM_RESET:
                    case ARM_PSCI_FUNC_ID_SYSTEM_RESET2:
                    case ARM_PSCI_FUNC_ID_CPU_ON:
                        nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0,
                                        false /*f64BitReg*/, false /*fSignExtend*/,
                                        (uint64_t)ARM_PSCI_STS_SUCCESS);
                        break;
                    default:
                        nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0,
                                        false /*f64BitReg*/, false /*fSignExtend*/,
                                        (uint64_t)ARM_PSCI_STS_NOT_SUPPORTED);
                }
                break;
            }
            default:
                nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0, false /*f64BitReg*/, false /*fSignExtend*/, (uint64_t)ARM_PSCI_STS_NOT_SUPPORTED);
        }
    }
    else
        nemR3WinSetGReg(pVCpu, ARMV8_A64_REG_X0, false /*f64BitReg*/, false /*fSignExtend*/, (uint64_t)ARM_PSCI_STS_NOT_SUPPORTED);

    /** @todo What to do if immediate is != 0? */

    if (rcStrict == VINF_SUCCESS)
        pVCpu->cpum.GstCtx.Pc.u64 += sizeof(uint32_t);

    return rcStrict;
}


/**
 * Deals with MSR access exits (WHvRunVpExitReasonUnrecoverableException).
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @sa      nemHCWinHandleMessageUnrecoverableException
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemR3WinHandleExitUnrecoverableException(PVMCC pVM, PVMCPUCC pVCpu, MY_WHV_RUN_VP_EXIT_CONTEXT const *pExit)
{
#if 0
    /*
     * Just copy the state we've got and handle it in the loop for now.
     */
    nemR3WinCopyStateFromX64Header(pVCpu, &pExit->VpContext);
    Log(("TripleExit/%u: %04x:%08RX64/%s: RFL=%#RX64 -> VINF_EM_TRIPLE_FAULT\n", pVCpu->idCpu, pExit->VpContext.Cs.Selector,
         pExit->VpContext.Rip, nemR3WinExecStateToLogStr(&pExit->VpContext), pExit->VpContext.Rflags));
    RT_NOREF_PV(pVM);
    return VINF_EM_TRIPLE_FAULT;
#else
    /*
     * Let IEM decide whether this is really it.
     */
    EMHistoryAddExit(pVCpu, EMEXIT_MAKE_FT(EMEXIT_F_KIND_NEM, NEMEXITTYPE_UNRECOVERABLE_EXCEPTION),
                     pExit->UnrecoverableException.Header.Pc, ASMReadTSC());
    nemR3WinCopyStateFromArmHeader(pVCpu, &pExit->UnrecoverableException.Header);
    AssertReleaseFailed();
    RT_NOREF_PV(pVM);
    return VINF_SUCCESS;
#endif
}


/**
 * Handles VM exits.
 *
 * @returns Strict VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pVCpu           The cross context per CPU structure.
 * @param   pExit           The VM exit information to handle.
 * @sa      nemHCWinHandleMessage
 */
NEM_TMPL_STATIC VBOXSTRICTRC nemR3WinHandleExit(PVMCC pVM, PVMCPUCC pVCpu, MY_WHV_RUN_VP_EXIT_CONTEXT const *pExit)
{
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
    {
        int rc = nemHCWinCopyStateFromHyperV(pVM, pVCpu, CPUMCTX_EXTRN_ALL);
        AssertRCReturn(rc, rc);

        nemR3WinLogState(pVM, pVCpu);
    }
#endif

    switch (pExit->ExitReason)
    {
        case WHvRunVpExitReasonUnmappedGpa:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitMemUnmapped);
            return nemR3WinHandleExitMemory(pVM, pVCpu, pExit);

        case WHvRunVpExitReasonCanceled:
            Log4(("CanceledExit/%u\n", pVCpu->idCpu));
            return VINF_SUCCESS;

        case WHvRunVpExitReasonHypercall:
            return nemR3WinHandleExitHypercall(pVM, pVCpu, pExit);

        case 0x8001000c: /* WHvRunVpExitReasonArm64Reset */
        {
            if (pExit->Arm64Reset.ResetType == WHV_ARM64_RESET_CONTEXT_TYPE_POWER_OFF)
                return VMR3PowerOff(pVM->pUVM);
            else if (pExit->Arm64Reset.ResetType == WHV_ARM64_RESET_CONTEXT_TYPE_RESET)
            {
                VM_FF_SET(pVM, VM_FF_RESET);
                return VINF_EM_RESET;
            }
            else
                AssertLogRelFailedReturn(VERR_NEM_IPE_3);
        }

        case WHvRunVpExitReasonUnrecoverableException:
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatExitUnrecoverable);
            return nemR3WinHandleExitUnrecoverableException(pVM, pVCpu, pExit);

        case WHvRunVpExitReasonUnsupportedFeature:
        case WHvRunVpExitReasonInvalidVpRegisterValue:
            LogRel(("Unimplemented exit:\n%.*Rhxd\n", (int)sizeof(*pExit), pExit));
            AssertLogRelMsgFailedReturn(("Unexpected exit on CPU #%u: %#x\n%.32Rhxd\n",
                                         pVCpu->idCpu, pExit->ExitReason, pExit), VERR_NEM_IPE_3);

        /* Undesired exits: */
        case WHvRunVpExitReasonNone:
        default:
            LogRel(("Unknown exit:\n%.*Rhxd\n", (int)sizeof(*pExit), pExit));
            AssertLogRelMsgFailedReturn(("Unknown exit on CPU #%u: %#x!\n", pVCpu->idCpu, pExit->ExitReason), VERR_NEM_IPE_3);
    }
}


VBOXSTRICTRC nemR3NativeRunGC(PVM pVM, PVMCPU pVCpu)
{
    LogFlow(("NEM/%u: %08RX64 pstate=%#08RX64 <=\n", pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc, pVCpu->cpum.GstCtx.fPState));
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
        nemR3WinLogState(pVM, pVCpu);
#endif

    /*
     * Try switch to NEM runloop state.
     */
    if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED))
    { /* likely */ }
    else
    {
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);
        LogFlow(("NEM/%u: returning immediately because canceled\n", pVCpu->idCpu));
        return VINF_SUCCESS;
    }

    if (pVCpu->nem.s.fSyncCntvRegs)
    {
        static const WHV_REGISTER_NAME s_aNames[2] = { WHvArm64RegisterCntvCtlEl0, WHvArm64RegisterCntvCvalEl0 };
        WHV_REGISTER_VALUE aRegs[RT_ELEMENTS(s_aNames)];
        aRegs[0].Reg64 = pVCpu->cpum.GstCtx.CntvCtlEl0;
        aRegs[1].Reg64 = pVCpu->cpum.GstCtx.CntvCValEl0;

        HRESULT hrc = WHvSetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, s_aNames, RT_ELEMENTS(s_aNames), aRegs);
        AssertLogRelMsgReturn(SUCCEEDED(hrc),
                              ("WHvSetVirtualProcessorRegisters(%p, 0,{CNTV_CTL_EL0, CNTV_CVAL_EL0}, 2,) -> %Rhrc (Last=%#x/%u)\n",
                               pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                              , VERR_NEM_IPE_9);
        pVCpu->nem.s.fSyncCntvRegs = false;
    }


    /*
     * The run loop.
     *
     * Current approach to state updating to use the sledgehammer and sync
     * everything every time.  This will be optimized later.
     */
    const bool      fSingleStepping     = DBGFIsStepping(pVCpu);
    VBOXSTRICTRC    rcStrict            = VINF_SUCCESS;
    for (unsigned iLoop = 0;; iLoop++)
    {
        /*
         * Poll timers and run for a bit.
         *
         * With the VID approach (ring-0 or ring-3) we can specify a timeout here,
         * so we take the time of the next timer event and uses that as a deadline.
         * The rounding heuristics are "tuned" so that rhel5 (1K timer) will boot fine.
         */
        /** @todo See if we cannot optimize this TMTimerPollGIP by only redoing
         *        the whole polling job when timers have changed... */
        uint64_t       offDeltaIgnored;
        uint64_t const nsNextTimerEvt = TMTimerPollGIP(pVM, pVCpu, &offDeltaIgnored); NOREF(nsNextTimerEvt);
        if (   !VM_FF_IS_ANY_SET(pVM, VM_FF_EMT_RENDEZVOUS | VM_FF_TM_VIRTUAL_SYNC)
            && !VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_HM_TO_R3_MASK))
        {
            if (VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM_WAIT, VMCPUSTATE_STARTED_EXEC_NEM))
            {
                /* Ensure that Hyper-V has the whole state. */
                int rc2 = nemHCWinCopyStateToHyperV(pVM, pVCpu);
                AssertRCReturn(rc2, rc2);

#ifdef LOG_ENABLED
                if (LogIsFlowEnabled())
                {
                    static const WHV_REGISTER_NAME s_aNames[2] = { WHvArm64RegisterPc, WHvArm64RegisterPstate };
                    WHV_REGISTER_VALUE aRegs[RT_ELEMENTS(s_aNames)] = { { { {0, 0} } } };
                    WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, s_aNames, RT_ELEMENTS(s_aNames), aRegs);
                    LogFlow(("NEM/%u: Entry @ %08RX64 pstate=%#RX64\n", pVCpu->idCpu, aRegs[0].Reg64, aRegs[1].Reg64));
                }
#endif

                MY_WHV_RUN_VP_EXIT_CONTEXT ExitReason = {0};
                TMNotifyStartOfExecution(pVM, pVCpu);

                HRESULT hrc = WHvRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, &ExitReason, sizeof(ExitReason));

                VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED_EXEC_NEM, VMCPUSTATE_STARTED_EXEC_NEM_WAIT);
                TMNotifyEndOfExecution(pVM, pVCpu, ASMReadTSC());
#ifdef LOG_ENABLED
                if (LogIsFlowEnabled())
                {
                    static const WHV_REGISTER_NAME s_aNames[2] = { WHvArm64RegisterPc, WHvArm64RegisterPstate };
                    WHV_REGISTER_VALUE aRegs[RT_ELEMENTS(s_aNames)] = { { { {0, 0} } } };
                    WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, s_aNames, RT_ELEMENTS(s_aNames), aRegs);
                    LogFlow(("NEM/%u: Exit @ %08RX64 pstate=%#RX64 Reason=%#x\n",
                             pVCpu->idCpu, aRegs[0].Reg64, aRegs[1].Reg64, ExitReason.ExitReason));
                }
#endif
                if (SUCCEEDED(hrc))
                {
                    /* Always sync the CNTV_CTL_EL0/CNTV_CVAL_EL0 registers, just like we do on macOS. */
                    static const WHV_REGISTER_NAME s_aNames[2] = { WHvArm64RegisterCntvCtlEl0, WHvArm64RegisterCntvCvalEl0 };
                    WHV_REGISTER_VALUE aRegs[RT_ELEMENTS(s_aNames)] = { { { {0, 0} } } };
                    hrc = WHvGetVirtualProcessorRegisters(pVM->nem.s.hPartition, pVCpu->idCpu, s_aNames, RT_ELEMENTS(s_aNames), aRegs);
                    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                                          ("WHvGetVirtualProcessorRegisters(%p, 0,{CNTV_CTL_EL0, CNTV_CVAL_EL0}, 2,) -> %Rhrc (Last=%#x/%u)\n",
                                           pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                                          , VERR_NEM_IPE_9);

                    pVCpu->cpum.GstCtx.CntvCtlEl0  = aRegs[0].Reg64;
                    pVCpu->cpum.GstCtx.CntvCValEl0 = aRegs[1].Reg64;

                    /*
                     * Deal with the message.
                     */
                    rcStrict = nemR3WinHandleExit(pVM, pVCpu, &ExitReason);
                    if (rcStrict == VINF_SUCCESS)
                    { /* hopefully likely */ }
                    else
                    {
                        LogFlow(("NEM/%u: breaking: nemR3WinHandleExit -> %Rrc\n", pVCpu->idCpu, VBOXSTRICTRC_VAL(rcStrict) ));
                        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnStatus);
                        break;
                    }
                }
                else
                    AssertLogRelMsgFailedReturn(("WHvRunVirtualProcessor failed for CPU #%u: %#x (%u)\n",
                                                 pVCpu->idCpu, hrc, GetLastError()),
                                                VERR_NEM_IPE_0);

                /*
                 * If no relevant FFs are pending, loop.
                 */
                if (   !VM_FF_IS_ANY_SET(   pVM,   !fSingleStepping ? VM_FF_HP_R0_PRE_HM_MASK    : VM_FF_HP_R0_PRE_HM_STEP_MASK)
                    && !VMCPU_FF_IS_ANY_SET(pVCpu, !fSingleStepping ? VMCPU_FF_HP_R0_PRE_HM_MASK : VMCPU_FF_HP_R0_PRE_HM_STEP_MASK) )
                    continue;

                /** @todo Try handle pending flags, not just return to EM loops.  Take care
                 *        not to set important RCs here unless we've handled a message. */
                LogFlow(("NEM/%u: breaking: pending FF (%#x / %#RX64)\n",
                         pVCpu->idCpu, pVM->fGlobalForcedActions, (uint64_t)pVCpu->fLocalForcedActions));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPost);
            }
            else
            {
                LogFlow(("NEM/%u: breaking: canceled %d (pre exec)\n", pVCpu->idCpu, VMCPU_GET_STATE(pVCpu) ));
                STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnCancel);
            }
        }
        else
        {
            LogFlow(("NEM/%u: breaking: pending FF (pre exec)\n", pVCpu->idCpu));
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatBreakOnFFPre);
        }
        break;
    } /* the run loop */


    /*
     * If the CPU is running, make sure to stop it before we try sync back the
     * state and return to EM.  We don't sync back the whole state if we can help it.
     */
    if (!VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM))
        VMCPU_CMPXCHG_STATE(pVCpu, VMCPUSTATE_STARTED, VMCPUSTATE_STARTED_EXEC_NEM_CANCELED);

    if (pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL)
    {
        /* Try anticipate what we might need. */
        uint64_t fImport = IEM_CPUMCTX_EXTRN_MUST_MASK;
        if (   (rcStrict >= VINF_EM_FIRST && rcStrict <= VINF_EM_LAST)
            || RT_FAILURE(rcStrict))
            fImport = CPUMCTX_EXTRN_ALL;
        else if (VMCPU_FF_IS_ANY_SET(pVCpu, VMCPU_FF_INTERRUPT_IRQ | VMCPU_FF_INTERRUPT_FIQ))
            fImport |= IEM_CPUMCTX_EXTRN_XCPT_MASK;

        if (pVCpu->cpum.GstCtx.fExtrn & fImport)
        {
            int rc2 = nemHCWinCopyStateFromHyperV(pVM, pVCpu, fImport);
            if (RT_SUCCESS(rc2))
                pVCpu->cpum.GstCtx.fExtrn &= ~fImport;
            else if (RT_SUCCESS(rcStrict))
                rcStrict = rc2;
            if (!(pVCpu->cpum.GstCtx.fExtrn & CPUMCTX_EXTRN_ALL))
                pVCpu->cpum.GstCtx.fExtrn = 0;
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturn);
        }
        else
            STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
    }
    else
    {
        STAM_REL_COUNTER_INC(&pVCpu->nem.s.StatImportOnReturnSkipped);
        pVCpu->cpum.GstCtx.fExtrn = 0;
    }

#if 0
    UINT32 cbWritten;
    WHV_ARM64_LOCAL_INTERRUPT_CONTROLLER_STATE IntrState;
    HRESULT hrc = WHvGetVirtualProcessorState(pVM->nem.s.hPartition, pVCpu->idCpu, WHvVirtualProcessorStateTypeInterruptControllerState2,
                                              &IntrState, sizeof(IntrState), &cbWritten);
    AssertLogRelMsgReturn(SUCCEEDED(hrc),
                          ("WHvGetVirtualProcessorState(%p, %u,WHvVirtualProcessorStateTypeInterruptControllerState2,) -> %Rhrc (Last=%#x/%u)\n",
                           pVM->nem.s.hPartition, pVCpu->idCpu, hrc, RTNtLastStatusValue(), RTNtLastErrorValue())
                          , VERR_NEM_GET_REGISTERS_FAILED);
    LogFlowFunc(("IntrState: cbWritten=%u\n"));
    for (uint32_t i = 0; i < RT_ELEMENTS(IntrState.BankedInterruptState); i++)
    {
        WHV_ARM64_INTERRUPT_STATE *pState = &IntrState.BankedInterruptState[i];
        LogFlowFunc(("IntrState: Intr %u:\n"
                     "    Enabled=%RTbool\n"
                     "    EdgeTriggered=%RTbool\n"
                     "    Asserted=%RTbool\n"
                     "    SetPending=%RTbool\n"
                     "    Active=%RTbool\n"
                     "    Direct=%RTbool\n"
                     "    GicrIpriorityrConfigured=%u\n"
                     "    GicrIpriorityrActive=%u\n",
                     i, pState->Enabled, pState->EdgeTriggered, pState->Asserted, pState->SetPending, pState->Active, pState->Direct,
                     pState->GicrIpriorityrConfigured, pState->GicrIpriorityrActive));
    }
#endif

    LogFlow(("NEM/%u: %08RX64 pstate=%#08RX64 => %Rrc\n", pVCpu->idCpu, pVCpu->cpum.GstCtx.Pc.u64,
             pVCpu->cpum.GstCtx.fPState, VBOXSTRICTRC_VAL(rcStrict) ));
    return rcStrict;
}


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
    RT_NOREF(pVM, pVCpu);
    return true;
}


VMMR3_INT_DECL(int) NEMR3Halt(PVM pVM, PVMCPU pVCpu)
{
    Assert(EMGetState(pVCpu) == EMSTATE_WAIT_SIPI);

    /*
     * Force the vCPU to get out of the SIPI state and into the normal runloop
     * as Hyper-V doesn't cause VM exits for PSCI calls so we wouldn't notice when
     * when the guest brings APs online.
     * Instead we force the EMT to run the vCPU through Hyper-V which manages the state.
     */
    RT_NOREF(pVM);
    EMSetState(pVCpu, EMSTATE_HALTED);
    return VINF_EM_RESCHEDULE;
}


bool nemR3NativeSetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
}


void nemR3NativeNotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    Log8(("nemR3NativeNotifyFF: canceling %u\n", pVCpu->idCpu));
    if (pVM->nem.s.fCreatedEmts)
    {
        HRESULT hrc = WHvCancelRunVirtualProcessor(pVM->nem.s.hPartition, pVCpu->idCpu, 0);
        AssertMsg(SUCCEEDED(hrc), ("WHvCancelRunVirtualProcessor -> hrc=%Rhrc\n", hrc));
        RT_NOREF_PV(hrc);
    }
    RT_NOREF_PV(fFlags);
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChanged(PVM pVM, bool fUseDebugLoop)
{
    RT_NOREF(pVM, fUseDebugLoop);
    return false;
}


DECLHIDDEN(bool) nemR3NativeNotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu, bool fUseDebugLoop)
{
    RT_NOREF(pVM, pVCpu, fUseDebugLoop);
    return false;
}


DECLINLINE(int) nemR3NativeGCPhys2R3PtrReadOnly(PVM pVM, RTGCPHYS GCPhys, const void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}


DECLINLINE(int) nemR3NativeGCPhys2R3PtrWriteable(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, ppv, &Lock);
    if (RT_SUCCESS(rc))
        PGMPhysReleasePageMappingLock(pVM, &Lock);
    return rc;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvR3,
                                               uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysRamRegister: %RGp LB %RGp, pvR3=%p pu2State=%p (%d) puNemRange=%p (%d)\n",
          GCPhys, cb, pvR3, pu2State, pu2State, puNemRange, *puNemRange));

    *pu2State = UINT8_MAX;
    RT_NOREF(puNemRange);

    if (pvR3)
    {
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfMapGpaRange, a);
        HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvR3, GCPhys, cb,
                                     WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfMapGpaRange, a);
        if (SUCCEEDED(hrc))
            *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
        else
        {
            LogRel(("NEMR3NotifyPhysRamRegister: GCPhys=%RGp LB %RGp pvR3=%p hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, cb, pvR3, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(bool) NEMR3IsMmio2DirtyPageTrackingSupported(PVM pVM)
{
    RT_NOREF(pVM);
    return g_pfnWHvQueryGpaRangeDirtyBitmap != NULL;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                  void *pvRam, void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("NEMR3NotifyPhysMmioExMapEarly: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, *pu2State, puNemRange, puNemRange ? *puNemRange : UINT32_MAX));
    RT_NOREF(puNemRange);

    /*
     * Unmap the RAM we're replacing.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfUnmapGpaRange, a);
        HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, cb);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfUnmapGpaRange, a);
        if (SUCCEEDED(hrc))
        { /* likely */ }
        else if (pvMmio2)
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> hrc=%Rhrc (%#x) Last=%#x/%u (ignored)\n",
                    GCPhys, cb, fFlags, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, cb, fFlags, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
            return VERR_NEM_UNMAP_PAGES_FAILED;
        }
    }

    /*
     * Map MMIO2 if any.
     */
    if (pvMmio2)
    {
        Assert(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2);
        WHV_MAP_GPA_RANGE_FLAGS fWHvFlags = WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute;
        if ((fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_TRACK_DIRTY_PAGES) && g_pfnWHvQueryGpaRangeDirtyBitmap)
            fWHvFlags |= WHvMapGpaRangeFlagTrackDirtyPages;
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfMapGpaRange, a);
        HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvMmio2, GCPhys, cb, fWHvFlags);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfMapGpaRange, a);
        if (SUCCEEDED(hrc))
            *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExMapEarly: GCPhys=%RGp LB %RGp fFlags=%#x pvMmio2=%p fWHvFlags=%#x: Map -> hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, cb, fFlags, pvMmio2, fWHvFlags, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
            return VERR_NEM_MAP_PAGES_FAILED;
        }
    }
    else
    {
        Assert(!(fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2));
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;
    }
    RT_NOREF(pvRam);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExMapLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags,
                                                 void *pvRam, void *pvMmio2, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, fFlags, pvRam, pvMmio2, puNemRange);
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int) NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvRam,
                                               void *pvMmio2, uint8_t *pu2State, uint32_t *puNemRange)
{
    int rc = VINF_SUCCESS;
    Log5(("NEMR3NotifyPhysMmioExUnmap: %RGp LB %RGp fFlags=%#x pvRam=%p pvMmio2=%p pu2State=%p uNemRange=%#x (%#x)\n",
          GCPhys, cb, fFlags, pvRam, pvMmio2, pu2State, puNemRange, *puNemRange));

    /*
     * Unmap the MMIO2 pages.
     */
    /** @todo If we implement aliasing (MMIO2 page aliased into MMIO range),
     *        we may have more stuff to unmap even in case of pure MMIO... */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_MMIO2)
    {
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfUnmapGpaRange, a);
        HRESULT hrc = WHvUnmapGpaRange(pVM->nem.s.hPartition, GCPhys, cb);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfUnmapGpaRange, a);
        if (FAILED(hrc))
        {
            LogRel2(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp fFlags=%#x: Unmap -> hrc=%Rhrc (%#x) Last=%#x/%u (ignored)\n",
                     GCPhys, cb, fFlags, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            rc = VERR_NEM_UNMAP_PAGES_FAILED;
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatUnmapPageFailed);
        }
    }

    /*
     * Restore the RAM we replaced.
     */
    if (fFlags & NEM_NOTIFY_PHYS_MMIO_EX_F_REPLACE)
    {
        AssertPtr(pvRam);
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfMapGpaRange, a);
        HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvRam, GCPhys, cb,
                                     WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfMapGpaRange, a);
        if (SUCCEEDED(hrc))
        { /* likely */ }
        else
        {
            LogRel(("NEMR3NotifyPhysMmioExUnmap: GCPhys=%RGp LB %RGp pvMmio2=%p hrc=%Rhrc (%#x) Last=%#x/%u\n",
                    GCPhys, cb, pvMmio2, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
            rc = VERR_NEM_MAP_PAGES_FAILED;
            STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
        }
        if (pu2State)
            *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
    }
    /* Mark the pages as unmapped if relevant. */
    else if (pu2State)
        *pu2State = NEM_WIN_PAGE_STATE_UNMAPPED;

    RT_NOREF(pvMmio2, puNemRange);
    return rc;
}


VMMR3_INT_DECL(int) NEMR3PhysMmio2QueryAndResetDirtyBitmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t uNemRange,
                                                           void *pvBitmap, size_t cbBitmap)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
    AssertReturn(g_pfnWHvQueryGpaRangeDirtyBitmap, VERR_INTERNAL_ERROR_2);
    Assert(cbBitmap == (uint32_t)cbBitmap);
    RT_NOREF(uNemRange);

    /* This is being profiled by PGM, see /PGM/Mmio2QueryAndResetDirtyBitmap. */
    HRESULT hrc = WHvQueryGpaRangeDirtyBitmap(pVM->nem.s.hPartition, GCPhys, cb, (UINT64 *)pvBitmap, (uint32_t)cbBitmap);
    if (SUCCEEDED(hrc))
        return VINF_SUCCESS;

    AssertLogRelMsgFailed(("GCPhys=%RGp LB %RGp pvBitmap=%p LB %#zx hrc=%Rhrc (%#x) Last=%#x/%u\n",
                           GCPhys, cb, pvBitmap, cbBitmap, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
    return VERR_NEM_QUERY_DIRTY_BITMAP_FAILED;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages, uint32_t fFlags,
                                                     uint8_t *pu2State, uint32_t *puNemRange)
{
    RT_NOREF(pVM, GCPhys, cb, pvPages, fFlags, puNemRange);

    Log5(("NEMR3NotifyPhysRomRegisterEarly: %RGp LB %RGp pvPages=%p fFlags=%#x\n", GCPhys, cb, pvPages, fFlags));
    *pu2State   = UINT8_MAX;
    *puNemRange = 0;
    return VINF_SUCCESS;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, void *pvPages,
                                                    uint32_t fFlags, uint8_t *pu2State, uint32_t *puNemRange)
{
    Log5(("nemR3NativeNotifyPhysRomRegisterLate: %RGp LB %RGp pvPages=%p fFlags=%#x pu2State=%p (%d) puNemRange=%p (%#x)\n",
          GCPhys, cb, pvPages, fFlags, pu2State, *pu2State, puNemRange, *puNemRange));
    *pu2State = UINT8_MAX;

    /*
     * (Re-)map readonly.
     */
    AssertPtrReturn(pvPages, VERR_INVALID_POINTER);
    STAM_REL_PROFILE_START(&pVM->nem.s.StatProfMapGpaRange, a);
    HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvPages, GCPhys, cb, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute);
    STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfMapGpaRange, a);
    if (SUCCEEDED(hrc))
        *pu2State = NEM_WIN_PAGE_STATE_READABLE;
    else
    {
        LogRel(("nemR3NativeNotifyPhysRomRegisterEarly: GCPhys=%RGp LB %RGp pvPages=%p fFlags=%#x hrc=%Rhrc (%#x) Last=%#x/%u\n",
                GCPhys, cb, pvPages, fFlags, hrc, hrc, RTNtLastStatusValue(), RTNtLastErrorValue()));
        STAM_REL_COUNTER_INC(&pVM->nem.s.StatMapPageFailed);
        return VERR_NEM_MAP_PAGES_FAILED;
    }
    RT_NOREF(fFlags, puNemRange);
    return VINF_SUCCESS;
}

VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
    Log(("nemR3NativeNotifySetA20: fEnabled=%RTbool\n", fEnabled));
    Assert(VM_IS_NEM_ENABLED(pVCpu->CTX_SUFF(pVM)));
    RT_NOREF(pVCpu, fEnabled);
}


void nemHCNativeNotifyHandlerPhysicalRegister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalRegister: %RGp LB %RGp enmKind=%d\n", GCPhys, cb, enmKind));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhys); NOREF(cb);
}


VMM_INT_DECL(void) NEMHCNotifyHandlerPhysicalDeregister(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhys, RTGCPHYS cb,
                                                        RTR3PTR pvMemR3, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyHandlerPhysicalDeregister: %RGp LB %RGp enmKind=%d pvMemR3=%p pu2State=%p (%d)\n",
          GCPhys, cb, enmKind, pvMemR3, pu2State, *pu2State));

    *pu2State = UINT8_MAX;
    if (pvMemR3)
    {
        STAM_REL_PROFILE_START(&pVM->nem.s.StatProfMapGpaRange, a);
        HRESULT hrc = WHvMapGpaRange(pVM->nem.s.hPartition, pvMemR3, GCPhys, cb,
                                     WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute | WHvMapGpaRangeFlagWrite);
        STAM_REL_PROFILE_STOP(&pVM->nem.s.StatProfMapGpaRange, a);
        if (SUCCEEDED(hrc))
            *pu2State = NEM_WIN_PAGE_STATE_WRITABLE;
        else
            AssertLogRelMsgFailed(("NEMHCNotifyHandlerPhysicalDeregister: WHvMapGpaRange(,%p,%RGp,%RGp,) -> %Rhrc\n",
                                   pvMemR3, GCPhys, cb, hrc));
    }
    RT_NOREF(enmKind);
}


void nemHCNativeNotifyHandlerPhysicalModify(PVMCC pVM, PGMPHYSHANDLERKIND enmKind, RTGCPHYS GCPhysOld,
                                            RTGCPHYS GCPhysNew, RTGCPHYS cb, bool fRestoreAsRAM)
{
    Log5(("nemHCNativeNotifyHandlerPhysicalModify: %RGp LB %RGp -> %RGp enmKind=%d fRestoreAsRAM=%d\n",
          GCPhysOld, cb, GCPhysNew, enmKind, fRestoreAsRAM));
    NOREF(pVM); NOREF(enmKind); NOREF(GCPhysOld); NOREF(GCPhysNew); NOREF(cb); NOREF(fRestoreAsRAM);
}


int nemHCNativeNotifyPhysPageAllocated(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, uint32_t fPageProt,
                                       PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("nemHCNativeNotifyPhysPageAllocated: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(pVM, GCPhys, HCPhys, fPageProt, enmType, pu2State);

    AssertFailed();
    return VINF_SUCCESS;
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageProtChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhys, RTR3PTR pvR3, uint32_t fPageProt,
                                                  PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageProtChanged: %RGp HCPhys=%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhys, fPageProt, enmType, *pu2State));
    RT_NOREF(pVM, GCPhys, HCPhys, pvR3, fPageProt, enmType, pu2State);
}


VMM_INT_DECL(void) NEMHCNotifyPhysPageChanged(PVMCC pVM, RTGCPHYS GCPhys, RTHCPHYS HCPhysPrev, RTHCPHYS HCPhysNew,
                                              RTR3PTR pvNewR3, uint32_t fPageProt, PGMPAGETYPE enmType, uint8_t *pu2State)
{
    Log5(("NEMHCNotifyPhysPageChanged: %RGp HCPhys=%RHp->%RHp fPageProt=%#x enmType=%d *pu2State=%d\n",
          GCPhys, HCPhysPrev, HCPhysNew, fPageProt, enmType, *pu2State));
    RT_NOREF(pVM, GCPhys, HCPhysPrev, HCPhysNew, pvNewR3, fPageProt, enmType, pu2State);

    AssertFailed();
}


/**
 * Returns features supported by the NEM backend.
 *
 * @returns Flags of features supported by the native NEM backend.
 * @param   pVM             The cross context VM structure.
 */
VMM_INT_DECL(uint32_t) NEMHCGetFeatures(PVMCC pVM)
{
    RT_NOREF(pVM);
    /** @todo Is NEM_FEAT_F_FULL_GST_EXEC always true? */
    return NEM_FEAT_F_NESTED_PAGING | NEM_FEAT_F_FULL_GST_EXEC;
}


/** @page pg_nem_win_aarmv8 NEM/win - Native Execution Manager, Windows.
 *
 * Open questions:
 *     - InstructionByteCount and InstructionBytes for unmapped GPA exit are zero...
 */

