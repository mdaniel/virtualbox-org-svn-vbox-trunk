/* $Id$ */
/** @file
 * GIM - Guest Interface Manager, KVM implementation.
 */

/*
 * Copyright (C) 2015-2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GIM
#include <VBox/vmm/gim.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/ssm.h>
#include "GIMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/disopcode.h>
#include <VBox/version.h>

#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/**
 * GIM KVM saved-state version.
 */
#define GIM_KVM_SAVED_STATE_VERSION         UINT32_C(1)

/**
 * VBox internal struct. to passback to EMT rendezvous callback while enabling
 * the KVM wall-clock.
 */
typedef struct KVMWALLCLOCKINFO
{
    /** Guest physical address of the wall-clock struct.  */
    RTGCPHYS GCPhysWallClock;
} KVMWALLCLOCKINFO;
/** Pointer to the wall-clock info. struct. */
typedef KVMWALLCLOCKINFO *PKVMWALLCLOCKINFO;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef VBOX_WITH_STATISTICS
# define GIMKVM_MSRRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumMsrRdFn_Gim, kCpumMsrWrFn_Gim, 0, 0, 0, 0, 0, a_szName, { 0 }, { 0 }, { 0 }, { 0 } }
#else
# define GIMKVM_MSRRANGE(a_uFirst, a_uLast, a_szName) \
    { (a_uFirst), (a_uLast), kCpumMsrRdFn_Gim, kCpumMsrWrFn_Gim, 0, 0, 0, 0, 0, a_szName }
#endif

/**
 * Array of MSR ranges supported by KVM.
 */
static CPUMMSRRANGE const g_aMsrRanges_Kvm[] =
{
    GIMKVM_MSRRANGE(MSR_GIM_KVM_RANGE0_START, MSR_GIM_KVM_RANGE0_END, "KVM range 0"),
    GIMKVM_MSRRANGE(MSR_GIM_KVM_RANGE1_START, MSR_GIM_KVM_RANGE1_END, "KVM range 1")
};
#undef GIMKVM_MSRRANGE


/**
 * Initializes the KVM GIM provider.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3KvmInit(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    AssertReturn(pVM->gim.s.enmProviderId == GIMPROVIDERID_KVM, VERR_INTERNAL_ERROR_5);

    int rc;
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;

    /*
     * Determine interface capabilities based on the version.
     */
    if (!pVM->gim.s.u32Version)
    {
        /* Basic features. */
        pKvm->uBaseFeat = 0
                        | GIM_KVM_BASE_FEAT_CLOCK_OLD
                        //| GIM_KVM_BASE_FEAT_NOP_IO_DELAY
                        //|  GIM_KVM_BASE_FEAT_MMU_OP
                        | GIM_KVM_BASE_FEAT_CLOCK
                        //| GIM_KVM_BASE_FEAT_ASYNC_PF
                        //| GIM_KVM_BASE_FEAT_STEAL_TIME
                        //| GIM_KVM_BASE_FEAT_PV_EOI
                        | GIM_KVM_BASE_FEAT_PV_UNHALT
                        ;
        /* Rest of the features are determined in gimR3KvmInitCompleted(). */
    }

    /*
     * Expose HVP (Hypervisor Present) bit to the guest.
     */
    CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_HVP);

    /*
     * Modify the standard hypervisor leaves for KVM.
     */
    CPUMCPUIDLEAF HyperLeaf;
    RT_ZERO(HyperLeaf);
    HyperLeaf.uLeaf        = UINT32_C(0x40000000);
    HyperLeaf.uEax         = UINT32_C(0x40000001); /* Minimum value for KVM is 0x40000001. */
    HyperLeaf.uEbx         = 0x4B4D564B;           /* 'KVMK' */
    HyperLeaf.uEcx         = 0x564B4D56;           /* 'VMKV' */
    HyperLeaf.uEdx         = 0x0000004D;           /* 'M000' */
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Add KVM specific leaves.
     */
    HyperLeaf.uLeaf        = UINT32_C(0x40000001);
    HyperLeaf.uEax         = pKvm->uBaseFeat;
    HyperLeaf.uEbx         = 0;                    /* Reserved */
    HyperLeaf.uEcx         = 0;                    /* Reserved */
    HyperLeaf.uEdx         = 0;                    /* Reserved */
    rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Insert all MSR ranges of KVM.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aMsrRanges_Kvm); i++)
    {
        rc = CPUMR3MsrRangesInsert(pVM, &g_aMsrRanges_Kvm[i]);
        AssertLogRelRCReturn(rc, rc);
    }

    /*
     * Setup hypercall and #UD handling.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
        VMMHypercallsEnable(&pVM->aCpus[i]);

    if (ASMIsAmdCpu())
    {
        pKvm->fTrapXcptUD   = true;
        pKvm->uOpCodeNative = OP_VMMCALL;
    }
    else
    {
        Assert(ASMIsIntelCpu() || ASMIsViaCentaurCpu());
        pKvm->fTrapXcptUD   = false;
        pKvm->uOpCodeNative = OP_VMCALL;
    }

    /* We always need to trap VMCALL/VMMCALL hypercall using #UDs for raw-mode VMs. */
    if (VM_IS_RAW_MODE_ENABLED(pVM))
        pKvm->fTrapXcptUD = true;

    return VINF_SUCCESS;
}


/**
 * Initializes remaining bits of the KVM provider.
 *
 * This is called after initializing HM and almost all other VMM components.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3KvmInitCompleted(PVM pVM)
{
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    pKvm->cTscTicksPerSecond = TMCpuTicksPerSecond(pVM);

    if (TMR3CpuTickIsFixedRateMonotonic(pVM, true /* fWithParavirtEnabled */))
    {
        /** @todo We might want to consider just enabling this bit *always*. As far
         *        as I can see in the Linux guest, the "TSC_STABLE" bit is only
         *        translated as a "monotonic" bit which even in Async systems we
         *        -should- be reporting a strictly monotonic TSC to the guest.  */
        pKvm->uBaseFeat |= GIM_KVM_BASE_FEAT_TSC_STABLE;

        CPUMCPUIDLEAF HyperLeaf;
        RT_ZERO(HyperLeaf);
        HyperLeaf.uLeaf        = UINT32_C(0x40000001);
        HyperLeaf.uEax         = pKvm->uBaseFeat;
        HyperLeaf.uEbx         = 0;
        HyperLeaf.uEcx         = 0;
        HyperLeaf.uEdx         = 0;
        int rc = CPUMR3CpuIdInsert(pVM, &HyperLeaf);
        AssertLogRelRCReturn(rc, rc);
    }
    return VINF_SUCCESS;
}


/**
 * Terminates the KVM GIM provider.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3KvmTerm(PVM pVM)
{
    gimR3KvmReset(pVM);
    return VINF_SUCCESS;
}


/**
 * This resets KVM provider MSRs and unmaps whatever KVM regions that
 * the guest may have mapped.
 *
 * This is called when the VM is being reset.
 *
 * @param   pVM     The cross context VM structure.
 * @thread EMT(0)
 */
VMMR3_INT_DECL(void) gimR3KvmReset(PVM pVM)
{
    VM_ASSERT_EMT0(pVM);
    LogRel(("GIM: KVM: Resetting MSRs\n"));

    /*
     * Reset MSRs.
     */
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    pKvm->u64WallClockMsr = 0;
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PGIMKVMCPU pKvmCpu = &pVM->aCpus[iCpu].gim.s.u.KvmCpu;
        pKvmCpu->u64SystemTimeMsr = 0;
        pKvmCpu->u32SystemTimeVersion = 0;
        pKvmCpu->fSystemTimeFlags = 0;
        pKvmCpu->GCPhysSystemTime = 0;
        pKvmCpu->uTsc = 0;
        pKvmCpu->uVirtNanoTS = 0;
    }
}


/**
 * KVM state-save operation.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 * @param   pSSM    The saved state handle.
 */
VMMR3_INT_DECL(int) gimR3KvmSave(PVM pVM, PSSMHANDLE pSSM)
{
    PCGIMKVM pKvm = &pVM->gim.s.u.Kvm;

    /*
     * Save the KVM SSM version.
     */
    SSMR3PutU32(pSSM, GIM_KVM_SAVED_STATE_VERSION);

    /*
     * Save per-VCPU data.
     */
    for (uint32_t i = 0; i < pVM->cCpus; i++)
    {
        PCGIMKVMCPU pKvmCpu = &pVM->aCpus[i].gim.s.u.KvmCpu;
        SSMR3PutU64(pSSM, pKvmCpu->u64SystemTimeMsr);
        SSMR3PutU64(pSSM, pKvmCpu->uTsc);
        SSMR3PutU64(pSSM, pKvmCpu->uVirtNanoTS);
        SSMR3PutGCPhys(pSSM, pKvmCpu->GCPhysSystemTime);
        SSMR3PutU32(pSSM, pKvmCpu->u32SystemTimeVersion);
        SSMR3PutU8(pSSM, pKvmCpu->fSystemTimeFlags);
    }

    /*
     * Save per-VM data.
     */
    SSMR3PutU64(pSSM, pKvm->u64WallClockMsr);
    return SSMR3PutU32(pSSM, pKvm->uBaseFeat);
}


/**
 * KVM state-load operation, final pass.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            The saved state handle.
 */
VMMR3_INT_DECL(int) gimR3KvmLoad(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Load the KVM SSM version first.
     */
    uint32_t uKvmSavedStatVersion;
    int rc = SSMR3GetU32(pSSM, &uKvmSavedStatVersion);
    AssertRCReturn(rc, rc);
    if (uKvmSavedStatVersion != GIM_KVM_SAVED_STATE_VERSION)
        return SSMR3SetLoadError(pSSM, VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION, RT_SRC_POS,
                                 N_("Unsupported KVM saved-state version %u (expected %u)."),
                                 uKvmSavedStatVersion, GIM_KVM_SAVED_STATE_VERSION);

    /*
     * Update the TSC frequency from TM.
     */
    PGIMKVM pKvm = &pVM->gim.s.u.Kvm;
    pKvm->cTscTicksPerSecond = TMCpuTicksPerSecond(pVM);

    /*
     * Load per-VCPU data.
     */
    for (uint32_t i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU     pVCpu   = &pVM->aCpus[i];
        PGIMKVMCPU pKvmCpu = &pVCpu->gim.s.u.KvmCpu;

        SSMR3GetU64(pSSM, &pKvmCpu->u64SystemTimeMsr);
        SSMR3GetU64(pSSM, &pKvmCpu->uTsc);
        SSMR3GetU64(pSSM, &pKvmCpu->uVirtNanoTS);
        SSMR3GetGCPhys(pSSM, &pKvmCpu->GCPhysSystemTime);
        SSMR3GetU32(pSSM, &pKvmCpu->u32SystemTimeVersion);
        rc = SSMR3GetU8(pSSM, &pKvmCpu->fSystemTimeFlags);
        AssertRCReturn(rc, rc);

        /* Enable the system-time struct. if necessary. */
        /** @todo update guest struct only if cTscTicksPerSecond doesn't match host
         *        anymore. */
        if (MSR_GIM_KVM_SYSTEM_TIME_IS_ENABLED(pKvmCpu->u64SystemTimeMsr))
        {
            Assert(!TMVirtualIsTicking(pVM));       /* paranoia. */
            Assert(!TMCpuTickIsTicking(pVCpu));
            gimR3KvmEnableSystemTime(pVM, pVCpu);
        }
    }

    /*
     * Load per-VM data.
     */
    SSMR3GetU64(pSSM, &pKvm->u64WallClockMsr);
    rc = SSMR3GetU32(pSSM, &pKvm->uBaseFeat);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Enables the KVM VCPU system-time structure.
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 * @param   pVCpu              The cross context virtual CPU structure.
 *
 * @remarks Don't do any release assertions here, these can be triggered by
 *          guest R0 code.
 */
VMMR3_INT_DECL(int) gimR3KvmEnableSystemTime(PVM pVM, PVMCPU pVCpu)
{
    PGIMKVM    pKvm    = &pVM->gim.s.u.Kvm;
    PGIMKVMCPU pKvmCpu = &pVCpu->gim.s.u.KvmCpu;

    /*
     * Validate the mapping address first.
     */
    if (!PGMPhysIsGCPhysNormal(pVM, pKvmCpu->GCPhysSystemTime))
    {
        LogRel(("GIM: KVM: VCPU%3d: Invalid physical addr requested for mapping system-time struct. GCPhysSystemTime=%#RGp\n",
               pVCpu->idCpu, pKvmCpu->GCPhysSystemTime));
        return VERR_GIM_OPERATION_FAILED;
    }

    /*
     * Construct the system-time struct.
     */
    GIMKVMSYSTEMTIME SystemTime;
    RT_ZERO(SystemTime);
    SystemTime.u32Version  = pKvmCpu->u32SystemTimeVersion;
    SystemTime.u64NanoTS   = pKvmCpu->uVirtNanoTS;
    SystemTime.u64Tsc      = pKvmCpu->uTsc;
    SystemTime.fFlags      = pKvmCpu->fSystemTimeFlags | GIM_KVM_SYSTEM_TIME_FLAGS_TSC_STABLE;

    /*
     * How the guest calculates the system time (nanoseconds):
     *
     * tsc = rdtsc - SysTime.u64Tsc
     * if (SysTime.i8TscShift >= 0)
     *     tsc <<= i8TscShift;
     * else
     *     tsc >>= -i8TscShift;
     * time = ((tsc * SysTime.u32TscScale) >> 32) + SysTime.u64NanoTS
     */
    uint64_t u64TscFreq   = pKvm->cTscTicksPerSecond;
    SystemTime.i8TscShift = 0;
    while (u64TscFreq > 2 * RT_NS_1SEC_64)
    {
        u64TscFreq >>= 1;
        SystemTime.i8TscShift--;
    }
    uint32_t uTscFreqLo = (uint32_t)u64TscFreq;
    while (uTscFreqLo <= RT_NS_1SEC)
    {
        uTscFreqLo <<= 1;
        SystemTime.i8TscShift++;
    }
    SystemTime.u32TscScale = ASMDivU64ByU32RetU32(RT_NS_1SEC_64 << 32, uTscFreqLo);

    /*
     * Update guest memory with the system-time struct.
     */
    Assert(!(SystemTime.u32Version & UINT32_C(1)));
    int rc = PGMPhysSimpleWriteGCPhys(pVM, pKvmCpu->GCPhysSystemTime, &SystemTime, sizeof(GIMKVMSYSTEMTIME));
    if (RT_SUCCESS(rc))
    {
        LogRel(("GIM: KVM: VCPU%3d: Enabled system-time struct. at %#RGp - u32TscScale=%#RX32 i8TscShift=%d uVersion=%#RU32 "
                "fFlags=%#x uTsc=%#RX64 uVirtNanoTS=%#RX64\n", pVCpu->idCpu, pKvmCpu->GCPhysSystemTime, SystemTime.u32TscScale,
                SystemTime.i8TscShift, SystemTime.u32Version, SystemTime.fFlags, pKvmCpu->uTsc, pKvmCpu->uVirtNanoTS));
        TMR3CpuTickParavirtEnable(pVM);
    }
    else
        LogRel(("GIM: KVM: VCPU%3d: Failed to write system-time struct. at %#RGp. rc=%Rrc\n",
                pVCpu->idCpu, pKvmCpu->GCPhysSystemTime, rc));

    return rc;
}


/**
 * Disables the KVM system-time struct.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(int) gimR3KvmDisableSystemTime(PVM pVM)
{
    TMR3CpuTickParavirtDisable(pVM);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{PFNVMMEMTRENDEZVOUS,
 *      Worker for gimR3KvmEnableWallClock}
 */
static DECLCALLBACK(VBOXSTRICTRC) gimR3KvmEnableWallClockCallback(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    PKVMWALLCLOCKINFO pWallClockInfo  = (PKVMWALLCLOCKINFO)pvUser; AssertPtr(pWallClockInfo);
    RTGCPHYS          GCPhysWallClock = pWallClockInfo->GCPhysWallClock;
    RT_NOREF1(pVCpu);

    /*
     * Read the wall-clock version (sequence) from the guest.
     */
    uint32_t uVersion;
    Assert(PGMPhysIsGCPhysNormal(pVM, GCPhysWallClock));
    int rc = PGMPhysSimpleReadGCPhys(pVM, &uVersion, GCPhysWallClock, sizeof(uVersion));
    if (RT_FAILURE(rc))
    {
        LogRel(("GIM: KVM: Failed to read wall-clock struct. version at %#RGp. rc=%Rrc\n", GCPhysWallClock, rc));
        return rc;
    }

    /*
     * Ensure the version is incrementally even.
     */
    /* faster: uVersion = (uVersion | 1) + 1; */
    if (!(uVersion & 1))
        ++uVersion;
    ++uVersion;

    /*
     * Update wall-clock guest struct. with UTC information.
     */
    RTTIMESPEC TimeSpec;
    int32_t    iSec;
    int32_t    iNano;
    TMR3UtcNow(pVM, &TimeSpec);
    RTTimeSpecGetSecondsAndNano(&TimeSpec, &iSec, &iNano);

    GIMKVMWALLCLOCK WallClock;
    RT_ZERO(WallClock);
    AssertCompile(sizeof(uVersion) == sizeof(WallClock.u32Version));
    WallClock.u32Version = uVersion;
    WallClock.u32Sec     = iSec;
    WallClock.u32Nano    = iNano;

    /*
     * Write out the wall-clock struct. to guest memory.
     */
    Assert(!(WallClock.u32Version & 1));
    rc = PGMPhysSimpleWriteGCPhys(pVM, GCPhysWallClock, &WallClock, sizeof(GIMKVMWALLCLOCK));
    if (RT_SUCCESS(rc))
        LogRel(("GIM: KVM: Enabled wall-clock struct. at %#RGp - u32Sec=%u u32Nano=%u uVersion=%#RU32\n", GCPhysWallClock,
                WallClock.u32Sec, WallClock.u32Nano, WallClock.u32Version));
    else
        LogRel(("GIM: KVM: Failed to write wall-clock struct. at %#RGp. rc=%Rrc\n", GCPhysWallClock, rc));
    return rc;
}


/**
 * Enables the KVM wall-clock structure.
 *
 * Since the wall-clock can be read by any VCPU but it is a global struct. in
 * guest-memory, we do an EMT rendezvous here to be on the safe side. The
 * alternative is to use an MMIO2 region and use the WallClock.u32Version field
 * for transactional update. However, this MSR is rarely written to (typically
 * once during bootup) it's currently not a performance issue especially since
 * we're already in ring-3. If we really wanted better performance in this code
 * path, we should be doing it in ring-0 with transactional update while make
 * sure there is only 1 writer as well.
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 * @param   GCPhysWallClock    Where the guest wall-clock structure is located.
 *
 * @remarks Don't do any release assertions here, these can be triggered by
 *          guest R0 code.
 */
VMMR3_INT_DECL(int) gimR3KvmEnableWallClock(PVM pVM, RTGCPHYS GCPhysWallClock)
{
    KVMWALLCLOCKINFO WallClockInfo;
    WallClockInfo.GCPhysWallClock = GCPhysWallClock;
    return VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONCE, gimR3KvmEnableWallClockCallback, &WallClockInfo);
}

