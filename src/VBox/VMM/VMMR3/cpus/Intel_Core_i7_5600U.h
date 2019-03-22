/* $Id$ */
/** @file
 * CPU database entry "Intel Core i7-5600U".
 * Generated at 2015-11-04T14:14:27Z by VBoxCpuReport v5.0.51r103906 on win.amd64.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_CPUDB_Intel_Core_i7_5600U
#define VBOX_CPUDB_Intel_Core_i7_5600U


#ifndef CPUM_DB_STANDALONE
/**
 * CPUID leaves for Intel(R) Core(TM) i7-5600U CPU @ 2.60GHz.
 */
static CPUMCPUIDLEAF const g_aCpuIdLeaves_Intel_Core_i7_5600U[] =
{
    { 0x00000000, 0x00000000, 0x00000000, 0x00000014, 0x756e6547, 0x6c65746e, 0x49656e69, 0 },
    { 0x00000001, 0x00000000, 0x00000000, 0x000306d4, 0x00100800, 0x7ffafbff, 0xbfebfbff, 0 | CPUMCPUIDLEAF_F_CONTAINS_APIC_ID | CPUMCPUIDLEAF_F_CONTAINS_APIC },
    { 0x00000002, 0x00000000, 0x00000000, 0x76036301, 0x00f0b5ff, 0x00000000, 0x00c30000, 0 },
    { 0x00000003, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000004, 0x00000000, UINT32_MAX, 0x1c004121, 0x01c0003f, 0x0000003f, 0x00000000, 0 },
    { 0x00000004, 0x00000001, UINT32_MAX, 0x1c004122, 0x01c0003f, 0x0000003f, 0x00000000, 0 },
    { 0x00000004, 0x00000002, UINT32_MAX, 0x1c004143, 0x01c0003f, 0x000001ff, 0x00000000, 0 },
    { 0x00000004, 0x00000003, UINT32_MAX, 0x1c03c163, 0x03c0003f, 0x00000fff, 0x00000006, 0 },
    { 0x00000004, 0x00000004, UINT32_MAX, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000005, 0x00000000, 0x00000000, 0x00000040, 0x00000040, 0x00000003, 0x11142120, 0 },
    { 0x00000006, 0x00000000, 0x00000000, 0x00000077, 0x00000002, 0x00000009, 0x00000000, 0 },
    { 0x00000007, 0x00000000, UINT32_MAX, 0x00000000, 0x021c2fbb, 0x00000000, 0x00000000, 0 },
    { 0x00000007, 0x00000001, UINT32_MAX, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000008, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000009, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x0000000a, 0x00000000, 0x00000000, 0x07300403, 0x00000000, 0x00000000, 0x00000603, 0 },
    { 0x0000000b, 0x00000000, UINT32_MAX, 0x00000001, 0x00000002, 0x00000100, 0x00000000, 0 | CPUMCPUIDLEAF_F_INTEL_TOPOLOGY_SUBLEAVES | CPUMCPUIDLEAF_F_CONTAINS_APIC_ID },
    { 0x0000000b, 0x00000001, UINT32_MAX, 0x00000004, 0x00000004, 0x00000201, 0x00000000, 0 | CPUMCPUIDLEAF_F_INTEL_TOPOLOGY_SUBLEAVES | CPUMCPUIDLEAF_F_CONTAINS_APIC_ID },
    { 0x0000000b, 0x00000002, UINT32_MAX, 0x00000000, 0x00000000, 0x00000002, 0x00000000, 0 | CPUMCPUIDLEAF_F_INTEL_TOPOLOGY_SUBLEAVES | CPUMCPUIDLEAF_F_CONTAINS_APIC_ID },
    { 0x0000000c, 0x00000000, UINT32_MAX, 0x00000000, 0x00000001, 0x00000001, 0x00000000, 0 },
    { 0x0000000c, 0x00000001, UINT32_MAX, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x0000000d, 0x00000000, UINT32_MAX, 0x00000007, 0x00000340, 0x00000340, 0x00000000, 0 },
    { 0x0000000d, 0x00000001, UINT32_MAX, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x0000000d, 0x00000002, UINT32_MAX, 0x00000100, 0x00000240, 0x00000000, 0x00000000, 0 },
    { 0x0000000d, 0x00000003, UINT32_MAX, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x0000000e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x0000000f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000010, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000011, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000012, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000013, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x00000014, 0x00000000, UINT32_MAX, 0x00000000, 0x00000001, 0x00000001, 0x00000000, 0 },
    { 0x00000014, 0x00000001, UINT32_MAX, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x80000000, 0x00000000, 0x00000000, 0x80000008, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x80000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000121, 0x2c100800, 0 },
    { 0x80000002, 0x00000000, 0x00000000, 0x65746e49, 0x2952286c, 0x726f4320, 0x4d542865, 0 },
    { 0x80000003, 0x00000000, 0x00000000, 0x37692029, 0x3036352d, 0x43205530, 0x40205550, 0 },
    { 0x80000004, 0x00000000, 0x00000000, 0x362e3220, 0x7a484730, 0x00000000, 0x00000000, 0 },
    { 0x80000005, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0 },
    { 0x80000006, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01006040, 0x00000000, 0 },
    { 0x80000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000100, 0 },
    { 0x80000008, 0x00000000, 0x00000000, 0x00003027, 0x00000000, 0x00000000, 0x00000000, 0 },
};
#endif /* !CPUM_DB_STANDALONE */


#ifndef CPUM_DB_STANDALONE
/**
 * MSR ranges for Intel(R) Core(TM) i7-5600U CPU @ 2.60GHz.
 */
static CPUMMSRRANGE const g_aMsrRanges_Intel_Core_i7_5600U[] =
{
    MFX(0x00000000, "IA32_P5_MC_ADDR", Ia32P5McAddr, Ia32P5McAddr, 0, UINT64_C(0xffffffffffffff00), 0), /* value=0xff */
    MFX(0x00000001, "IA32_P5_MC_TYPE", Ia32P5McType, Ia32P5McType, 0, 0, UINT64_MAX), /* value=0x0 */
    MFX(0x00000006, "IA32_MONITOR_FILTER_LINE_SIZE", Ia32MonitorFilterLineSize, Ia32MonitorFilterLineSize, 0, 0, UINT64_C(0xffffffffffff0000)), /* value=0x40 */
    MFN(0x00000010, "IA32_TIME_STAMP_COUNTER", Ia32TimestampCounter, Ia32TimestampCounter), /* value=0x1c93`50dd535c */
    MFX(0x00000017, "IA32_PLATFORM_ID", Ia32PlatformId, ReadOnly, UINT64_C(0x18000000000000), 0, 0), /* value=0x180000`00000000 */
    MFX(0x0000001b, "IA32_APIC_BASE", Ia32ApicBase, Ia32ApicBase, UINT32_C(0xfee00900), 0, UINT64_C(0xffffff80000002ff)),
    MFX(0x0000002a, "EBL_CR_POWERON", IntelEblCrPowerOn, ReadOnly, 0, 0, 0), /* value=0x0 */
    MVX(0x0000002e, "I7_UNK_0000_002e", 0, 0x400, UINT64_C(0xfffffffffffffbff)),
    MVX(0x00000033, "TEST_CTL", 0, 0, UINT64_C(0xffffffff7fffffff)),
    MVO(0x00000034, "P6_UNK_0000_0034", 0x97b),
    MFO(0x00000035, "MSR_CORE_THREAD_COUNT", IntelI7CoreThreadCount), /* value=0x20004 */
    MFO(0x0000003a, "IA32_FEATURE_CONTROL", Ia32FeatureControl), /* value=0x5 */
    MVX(0x0000003b, "P6_UNK_0000_003b", UINT64_C(0xfffffffffffffffe), 0, 0),
    MVX(0x0000003e, "I7_UNK_0000_003e", 0x1, 0, UINT64_C(0xfffffffffffffffe)),
    MFN(0x00000079, "IA32_BIOS_UPDT_TRIG", WriteOnly, IgnoreWrite),
    MFX(0x0000008b, "BBL_CR_D3|BIOS_SIGN", Ia32BiosSignId, Ia32BiosSignId, 0, 0, UINT32_MAX), /* value=0x1f`00000000 */
    MVX(0x00000095, "TODO_0000_0095", 0, 0, UINT64_C(0xfffffffffffffffe)),
    MFO(0x0000009b, "IA32_SMM_MONITOR_CTL", Ia32SmmMonitorCtl), /* value=0x0 */
    RSN(0x000000c1, 0x000000c4, "IA32_PMCn", Ia32PmcN, Ia32PmcN, 0x0, ~(uint64_t)UINT32_MAX, 0),
    MFO(0x000000ce, "IA32_PLATFORM_INFO", IntelPlatformInfo), /* value=0x5053b`f3011a00 */
    MFX(0x000000e2, "MSR_PKG_CST_CONFIG_CONTROL", IntelPkgCStConfigControl, IntelPkgCStConfigControl, 0, 0, UINT64_C(0xffffffff01ffffff)), /* value=0x1e008408 */
    MFX(0x000000e3, "C2_SMM_CST_MISC_INFO", IntelCore2SmmCStMiscInfo, IntelCore2SmmCStMiscInfo, 0, UINT32_C(0xffff7000), ~(uint64_t)UINT32_MAX), /* value=0x8b800000 */
    MFX(0x000000e4, "MSR_PMG_IO_CAPTURE_BASE", IntelPmgIoCaptureBase, IntelPmgIoCaptureBase, 0, 0, UINT64_C(0xfffffffffff80000)), /* value=0x51814 */
    MFN(0x000000e7, "IA32_MPERF", Ia32MPerf, Ia32MPerf), /* value=0x23c`764b31c5 */
    MFN(0x000000e8, "IA32_APERF", Ia32APerf, Ia32APerf), /* value=0x2af`f518152c */
    MFX(0x000000fe, "IA32_MTRRCAP", Ia32MtrrCap, ReadOnly, 0xd0a, 0, 0), /* value=0xd0a */
    MVX(0x00000102, "I7_IB_UNK_0000_0102", 0, 0, UINT64_C(0xffffffff7fff8000)),
    MVX(0x00000103, "I7_IB_UNK_0000_0103", 0, 0, UINT64_C(0xfffffffffffff000)),
    MVX(0x00000104, "I7_IB_UNK_0000_0104", 0, 0, UINT64_C(0xfffffffffffffffe)),
    MVO(0x00000110, "TODO_0000_0110", 0x3),
    MVX(0x0000011f, "TODO_0000_011f", 0, 0, UINT64_C(0xffffffffffffff00)),
    MVO(0x0000013a, "TODO_0000_013a", UINT64_C(0x30000007f)),
    MFO(0x0000013c, "I7_SB_AES_NI_CTL", IntelI7SandyAesNiCtl), /* value=0x1 */
    MVX(0x00000140, "I7_IB_UNK_0000_0140", 0, 0, UINT64_C(0xfffffffffffffffe)),
    MVX(0x00000142, "I7_IB_UNK_0000_0142", 0, 0, UINT64_C(0xfffffffffffffffe)),
    MVX(0x00000150, "P6_UNK_0000_0150", 0, UINT64_C(0x8000ffffffffffff), UINT64_C(0x7fff000000000000)),
    MFX(0x00000174, "IA32_SYSENTER_CS", Ia32SysEnterCs, Ia32SysEnterCs, 0, ~(uint64_t)UINT32_MAX, 0), /* value=0x0 */
    MFN(0x00000175, "IA32_SYSENTER_ESP", Ia32SysEnterEsp, Ia32SysEnterEsp), /* value=0x0 */
    MFN(0x00000176, "IA32_SYSENTER_EIP", Ia32SysEnterEip, Ia32SysEnterEip), /* value=0x0 */
    MVX(0x00000178, "TODO_0000_0178", 0, 0, UINT64_C(0xfffffffffffffffc)),
    MFX(0x00000179, "IA32_MCG_CAP", Ia32McgCap, ReadOnly, 0x1000c07, 0, 0), /* value=0x1000c07 */
    MFX(0x0000017a, "IA32_MCG_STATUS", Ia32McgStatus, Ia32McgStatus, 0, 0, UINT64_C(0xfffffffffffffff8)), /* value=0x0 */
    RSN(0x00000186, 0x00000187, "IA32_PERFEVTSELn", Ia32PerfEvtSelN, Ia32PerfEvtSelN, 0x0, 0, UINT64_C(0xfffffffe00080000)), /* XXX: The range ended earlier than expected! */
    MVX(0x00000188, "IA32_PERFEVTSEL2", 0, 0, UINT64_C(0xfffffffc00080000)),
    MVX(0x00000189, "IA32_PERFEVTSEL3", 0, 0, UINT64_C(0xfffffffe00080000)),
    MFX(0x00000194, "CLOCK_FLEX_MAX", IntelFlexRatio, IntelFlexRatio, 0x90000, 0xe0000, UINT64_C(0xffffffffffe00000)), /* value=0x90000 */
    MFX(0x00000198, "IA32_PERF_STATUS", Ia32PerfStatus, ReadOnly, UINT64_C(0x273c00002000), 0, 0), /* value=0x273c`00002000 */
    MFX(0x00000199, "IA32_PERF_CTL", Ia32PerfCtl, Ia32PerfCtl, 0x2000, 0, 0), /* Might bite. value=0x2000 */
    MFX(0x0000019a, "IA32_CLOCK_MODULATION", Ia32ClockModulation, Ia32ClockModulation, 0, 0, UINT64_C(0xffffffffffffffe0)), /* value=0x0 */
    MFX(0x0000019b, "IA32_THERM_INTERRUPT", Ia32ThermInterrupt, Ia32ThermInterrupt, 0x10, 0, UINT64_C(0xfffffffffe0000e8)), /* value=0x10 */
    MFX(0x0000019c, "IA32_THERM_STATUS", Ia32ThermStatus, Ia32ThermStatus, UINT32_C(0x88150800), UINT32_C(0xf87f07fd), UINT64_C(0xffffffff0780f000)), /* value=0x88150800 */
    MFX(0x0000019d, "IA32_THERM2_CTL", Ia32Therm2Ctl, ReadOnly, 0, 0, 0), /* value=0x0 */
/// @todo WARNING: IA32_MISC_ENABLE probing needs hacking on this CPU!
    MFX(0x000001a0, "IA32_MISC_ENABLE", Ia32MiscEnable, Ia32MiscEnable, 0x850089, 0x1080, UINT64_C(0xffffffbbff3aef72)), /* value=0x850089 */
    MVO(0x000001a1, "P6_UNK_0000_01a1", 0x995),
    MFX(0x000001a2, "I7_MSR_TEMPERATURE_TARGET", IntelI7TemperatureTarget, IntelI7TemperatureTarget, 0x5690000, 0xffff00, UINT64_C(0xffffffffc00000ff)), /* value=0x5690000 */
    MVX(0x000001a4, "I7_UNK_0000_01a4", 0, 0, UINT64_C(0xfffffffffffff7f0)),
    RSN(0x000001a6, 0x000001a7, "I7_MSR_OFFCORE_RSP_n", IntelI7MsrOffCoreResponseN, IntelI7MsrOffCoreResponseN, 0x0, 0, UINT64_C(0xffffffc000007000)),
    MVX(0x000001a8, "I7_UNK_0000_01a8", 0, 0, UINT64_C(0xfffffffffffffffc)),
    MFX(0x000001aa, "MSR_MISC_PWR_MGMT", IntelI7MiscPwrMgmt, IntelI7MiscPwrMgmt, 0, 0, UINT64_C(0xffffffffffbffffe)), /* value=0x400000 */
    MFX(0x000001ad, "I7_MSR_TURBO_RATIO_LIMIT", IntelI7TurboRatioLimit, IntelI7TurboRatioLimit, UINT64_C(0x1f1f1f1f1f20), UINT64_MAX, 0), /* value=0x1f1f`1f1f1f20 */
    MVX(0x000001b0, "IA32_ENERGY_PERF_BIAS", 0x6, 0, UINT64_C(0xfffffffffffffff0)),
    MVX(0x000001b1, "IA32_PACKAGE_THERM_STATUS", UINT32_C(0x880d0802), UINT32_C(0xf87f07fd), UINT64_C(0xffffffff0780f000)),
    MVX(0x000001b2, "IA32_PACKAGE_THERM_INTERRUPT", 0, 0, UINT64_C(0xfffffffffe0000e8)),
    MVO(0x000001c6, "I7_UNK_0000_01c6", 0x3),
    MFX(0x000001c8, "MSR_LBR_SELECT", IntelI7LbrSelect, IntelI7LbrSelect, 0, 0, UINT64_C(0xfffffffffffffc00)), /* value=0x0 */
    MFX(0x000001c9, "MSR_LASTBRANCH_TOS", IntelLastBranchTos, IntelLastBranchTos, 0, 0, UINT64_C(0xfffffffffffffff0)), /* value=0x0 */
    MFX(0x000001d9, "IA32_DEBUGCTL", Ia32DebugCtl, Ia32DebugCtl, 0, 0, UINT64_C(0xffffffffffff003c)), /* value=0x0 */
    MFO(0x000001db, "P6_LAST_BRANCH_FROM_IP", P6LastBranchFromIp), /* value=0x0 */
    MFO(0x000001dc, "P6_LAST_BRANCH_TO_IP", P6LastBranchToIp), /* value=0x0 */
    MFX(0x000001dd, "P6_LAST_INT_FROM_IP", P6LastIntFromIp, P6LastIntFromIp, 0, 0, UINT64_C(0x1fff800000000000)), /* value=0x0 */
    MFN(0x000001de, "P6_LAST_INT_TO_IP", P6LastIntToIp, P6LastIntToIp), /* value=0x0 */
    MFO(0x000001f0, "I7_VLW_CAPABILITY", IntelI7VirtualLegacyWireCap), /* value=0x74 */
    MFO(0x000001f2, "IA32_SMRR_PHYSBASE", Ia32SmrrPhysBase), /* value=0xdc000006 */
    MFO(0x000001f3, "IA32_SMRR_PHYSMASK", Ia32SmrrPhysMask), /* value=0xff000800 */
    MFX(0x000001fc, "I7_MSR_POWER_CTL", IntelI7PowerCtl, IntelI7PowerCtl, 0, UINT32_C(0x80000020), UINT64_C(0xffffffff3e100000)), /* value=0x4005f */
    MFX(0x00000200, "IA32_MTRR_PHYS_BASE0", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x0, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x6 */
    MFX(0x00000201, "IA32_MTRR_PHYS_MASK0", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x0, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7e`00000800 */
    MFX(0x00000202, "IA32_MTRR_PHYS_BASE1", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x1, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x2`00000006 */
    MFX(0x00000203, "IA32_MTRR_PHYS_MASK1", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x1, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`f0000800 */
    MFX(0x00000204, "IA32_MTRR_PHYS_BASE2", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x2, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x2`10000006 */
    MFX(0x00000205, "IA32_MTRR_PHYS_MASK2", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x2, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`f8000800 */
    MFX(0x00000206, "IA32_MTRR_PHYS_BASE3", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x3, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x2`18000006 */
    MFX(0x00000207, "IA32_MTRR_PHYS_MASK3", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x3, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`fc000800 */
    MFX(0x00000208, "IA32_MTRR_PHYS_BASE4", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x4, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x2`1c000006 */
    MFX(0x00000209, "IA32_MTRR_PHYS_MASK4", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x4, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`fe000800 */
    MFX(0x0000020a, "IA32_MTRR_PHYS_BASE5", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x5, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x2`1e000006 */
    MFX(0x0000020b, "IA32_MTRR_PHYS_MASK5", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x5, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`ff800800 */
    MFX(0x0000020c, "IA32_MTRR_PHYS_BASE6", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x6, 0, UINT64_C(0xffffff8000000ff8)), /* value=0xe0000000 */
    MFX(0x0000020d, "IA32_MTRR_PHYS_MASK6", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x6, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`e0000800 */
    MFX(0x0000020e, "IA32_MTRR_PHYS_BASE7", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x7, 0, UINT64_C(0xffffff8000000ff8)), /* value=0xde000000 */
    MFX(0x0000020f, "IA32_MTRR_PHYS_MASK7", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x7, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`fe000800 */
    MFX(0x00000210, "IA32_MTRR_PHYS_BASE8", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x8, 0, UINT64_C(0xffffff8000000ff8)), /* value=0xdd000000 */
    MFX(0x00000211, "IA32_MTRR_PHYS_MASK8", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x8, 0, UINT64_C(0xffffff80000007ff)), /* value=0x7f`ff000800 */
    MFX(0x00000212, "IA32_MTRR_PHYS_BASE9", Ia32MtrrPhysBaseN, Ia32MtrrPhysBaseN, 0x9, 0, UINT64_C(0xffffff8000000ff8)), /* value=0x0 */
    MFX(0x00000213, "IA32_MTRR_PHYS_MASK9", Ia32MtrrPhysMaskN, Ia32MtrrPhysMaskN, 0x9, 0, UINT64_C(0xffffff80000007ff)), /* value=0x0 */
    MFS(0x00000250, "IA32_MTRR_FIX64K_00000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix64K_00000),
    MFS(0x00000258, "IA32_MTRR_FIX16K_80000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix16K_80000),
    MFS(0x00000259, "IA32_MTRR_FIX16K_A0000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix16K_A0000),
    MFS(0x00000268, "IA32_MTRR_FIX4K_C0000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_C0000),
    MFS(0x00000269, "IA32_MTRR_FIX4K_C8000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_C8000),
    MFS(0x0000026a, "IA32_MTRR_FIX4K_D0000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_D0000),
    MFS(0x0000026b, "IA32_MTRR_FIX4K_D8000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_D8000),
    MFS(0x0000026c, "IA32_MTRR_FIX4K_E0000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_E0000),
    MFS(0x0000026d, "IA32_MTRR_FIX4K_E8000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_E8000),
    MFS(0x0000026e, "IA32_MTRR_FIX4K_F0000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_F0000),
    MFS(0x0000026f, "IA32_MTRR_FIX4K_F8000", Ia32MtrrFixed, Ia32MtrrFixed, GuestMsrs.msr.MtrrFix4K_F8000),
    MFS(0x00000277, "IA32_PAT", Ia32Pat, Ia32Pat, Guest.msrPAT),
    MFX(0x00000280, "IA32_MC0_CTL2", Ia32McNCtl2, Ia32McNCtl2, 0x0, 0x40000000, UINT64_C(0xffffffffbfff8000)), /* value=0x0 */
    RSN(0x00000281, 0x00000283, "IA32_MC1_CTLn", Ia32McNCtl2, Ia32McNCtl2, 0x1, 0, UINT64_C(0xffffffffbfff8000)),
    MFX(0x00000284, "IA32_MC4_CTL2", Ia32McNCtl2, Ia32McNCtl2, 0x4, 0x40007fff, UINT64_C(0xffffffffbfff8000)), /* value=0x0 */
    RSN(0x00000285, 0x00000286, "IA32_MC5_CTLn", Ia32McNCtl2, Ia32McNCtl2, 0x5, 0, UINT64_C(0xffffffffbfff8000)),
    MVX(0x000002e0, "I7_SB_NO_EVICT_MODE", 0, 0, UINT64_C(0xfffffffffffffffc)),
    MVX(0x000002e7, "I7_IB_UNK_0000_02e7", 0x1, 0x1, UINT64_C(0xfffffffffffffffe)),
    MFZ(0x000002ff, "IA32_MTRR_DEF_TYPE", Ia32MtrrDefType, Ia32MtrrDefType, GuestMsrs.msr.MtrrDefType, 0, UINT64_C(0xfffffffffffff3f8)),
    MVO(0x00000305, "I7_SB_UNK_0000_0305", 0),
    RSN(0x00000309, 0x0000030b, "IA32_FIXED_CTRn", Ia32FixedCtrN, Ia32FixedCtrN, 0x0, 0, UINT64_C(0xffff000000000000)),
    MFX(0x00000345, "IA32_PERF_CAPABILITIES", Ia32PerfCapabilities, ReadOnly, 0x32c4, 0, 0), /* value=0x32c4 */
    MFX(0x0000038d, "IA32_FIXED_CTR_CTRL", Ia32FixedCtrCtrl, Ia32FixedCtrCtrl, 0, 0, UINT64_C(0xfffffffffffff000)), /* value=0x0 */
    MFX(0x0000038e, "IA32_PERF_GLOBAL_STATUS", Ia32PerfGlobalStatus, ReadOnly, 0, 0, 0), /* value=0x0 */
    MFX(0x0000038f, "IA32_PERF_GLOBAL_CTRL", Ia32PerfGlobalCtrl, Ia32PerfGlobalCtrl, 0, 0, UINT64_C(0xfffffff8fffffff0)), /* value=0xf */
    MFX(0x00000390, "IA32_PERF_GLOBAL_OVF_CTRL", Ia32PerfGlobalOvfCtrl, Ia32PerfGlobalOvfCtrl, 0, UINT64_C(0xe08000070000000f), UINT64_C(0x1f7ffff8fffffff0)), /* value=0x0 */
    MFX(0x00000391, "I7_UNC_PERF_GLOBAL_CTRL", IntelI7UncPerfGlobalCtrl, IntelI7UncPerfGlobalCtrl, 0, 0, UINT64_C(0xffffffff1fffff80)), /* value=0x0 */
    MFX(0x00000392, "I7_UNC_PERF_GLOBAL_STATUS", IntelI7UncPerfGlobalStatus, IntelI7UncPerfGlobalStatus, 0, 0xf, UINT64_C(0xfffffffffffffff0)), /* value=0x0 */
    MFX(0x00000393, "I7_UNC_PERF_GLOBAL_OVF_CTRL", IntelI7UncPerfGlobalOvfCtrl, IntelI7UncPerfGlobalOvfCtrl, 0, 0x3, UINT64_C(0xfffffffffffffffc)), /* value=0x0 */
    MFX(0x00000394, "I7_UNC_PERF_FIXED_CTR_CTRL", IntelI7UncPerfFixedCtrCtrl, IntelI7UncPerfFixedCtrCtrl, 0, 0, UINT64_C(0xffffffffffafffff)), /* value=0x0 */
    MFX(0x00000395, "I7_UNC_PERF_FIXED_CTR", IntelI7UncPerfFixedCtr, IntelI7UncPerfFixedCtr, 0, 0, UINT64_C(0xffff000000000000)), /* value=0x0 */
    MFO(0x00000396, "I7_UNC_CBO_CONFIG", IntelI7UncCBoxConfig), /* value=0x3 */
    MVX(0x00000397, "I7_SB_UNK_0000_0397", 0, 0, UINT64_C(0xfffffffffffffff0)),
    MFX(0x000003b0, "I7_UNC_ARB_PERF_CTR0", IntelI7UncArbPerfCtrN, IntelI7UncArbPerfCtrN, 0, 0, UINT64_C(0xfffff00000000000)), /* value=0x0 */
    MFX(0x000003b1, "I7_UNC_ARB_PERF_CTR1", IntelI7UncArbPerfCtrN, IntelI7UncArbPerfCtrN, 0, 0, UINT64_C(0xfffff00000000000)), /* value=0x0 */
    MFX(0x000003b2, "I7_UNC_ARB_PERF_EVT_SEL0", IntelI7UncArbPerfEvtSelN, IntelI7UncArbPerfEvtSelN, 0, 0, UINT64_C(0xffffffffe0230000)), /* value=0x0 */
    MFX(0x000003b3, "I7_UNC_ARB_PERF_EVT_SEL1", IntelI7UncArbPerfEvtSelN, IntelI7UncArbPerfEvtSelN, 0, 0, UINT64_C(0xffffffffe0230000)), /* value=0x0 */
    MVO(0x000003f0, "TODO_0000_03f0", 0),
    MFX(0x000003f1, "IA32_PEBS_ENABLE", Ia32PebsEnable, Ia32PebsEnable, 0, 0, UINT64_C(0xfffffff0fffffff0)), /* value=0x0 */
    MFX(0x000003f6, "I7_MSR_PEBS_LD_LAT", IntelI7PebsLdLat, IntelI7PebsLdLat, 0, UINT64_C(0xffffffffffff0000), 0), /* value=0xffff */
    MFX(0x000003f8, "I7_MSR_PKG_C3_RESIDENCY", IntelI7PkgCnResidencyN, ReadOnly, 0x3, 0, UINT64_MAX), /* value=0x4`465710e6 */
    RSN(0x000003f9, 0x000003fa, "I7_MSR_PKG_Cn_RESIDENCY", IntelI7PkgCnResidencyN, ReadOnly, 0x6, 0, UINT64_MAX),
    MFX(0x000003fc, "I7_MSR_CORE_C3_RESIDENCY", IntelI7CoreCnResidencyN, ReadOnly, 0x3, 0, UINT64_MAX), /* value=0x2`3a8a1eca */
    RSN(0x000003fd, 0x000003fe, "I7_MSR_CORE_Cn_RESIDENCY", IntelI7CoreCnResidencyN, ReadOnly, 0x6, 0, UINT64_MAX),
    RFN(0x00000400, 0x0000041b, "IA32_MCi_CTL_STATUS_ADDR_MISC", Ia32McCtlStatusAddrMiscN, Ia32McCtlStatusAddrMiscN),
    MFX(0x00000480, "IA32_VMX_BASIC", Ia32VmxBasic, ReadOnly, UINT64_C(0xda040000000012), 0, 0), /* value=0xda0400`00000012 */
    MFX(0x00000481, "IA32_VMX_PINBASED_CTLS", Ia32VmxPinbasedCtls, ReadOnly, UINT64_C(0x7f00000016), 0, 0), /* value=0x7f`00000016 */
    MFX(0x00000482, "IA32_VMX_PROCBASED_CTLS", Ia32VmxProcbasedCtls, ReadOnly, UINT64_C(0xfff9fffe0401e172), 0, 0), /* value=0xfff9fffe`0401e172 */
    MFX(0x00000483, "IA32_VMX_EXIT_CTLS", Ia32VmxExitCtls, ReadOnly, UINT64_C(0x7fffff00036dff), 0, 0), /* value=0x7fffff`00036dff */
    MFX(0x00000484, "IA32_VMX_ENTRY_CTLS", Ia32VmxEntryCtls, ReadOnly, UINT64_C(0xffff000011ff), 0, 0), /* value=0xffff`000011ff */
    MFX(0x00000485, "IA32_VMX_MISC", Ia32VmxMisc, ReadOnly, 0x300481e5, 0, 0), /* value=0x300481e5 */
    MFX(0x00000486, "IA32_VMX_CR0_FIXED0", Ia32VmxCr0Fixed0, ReadOnly, UINT32_C(0x80000021), 0, 0), /* value=0x80000021 */
    MFX(0x00000487, "IA32_VMX_CR0_FIXED1", Ia32VmxCr0Fixed1, ReadOnly, UINT32_MAX, 0, 0), /* value=0xffffffff */
    MFX(0x00000488, "IA32_VMX_CR4_FIXED0", Ia32VmxCr4Fixed0, ReadOnly, 0x2000, 0, 0), /* value=0x2000 */
    MFX(0x00000489, "IA32_VMX_CR4_FIXED1", Ia32VmxCr4Fixed1, ReadOnly, 0x3767ff, 0, 0), /* value=0x3767ff */
    MFX(0x0000048a, "IA32_VMX_VMCS_ENUM", Ia32VmxVmcsEnum, ReadOnly, 0x2a, 0, 0), /* value=0x2a */
    MFX(0x0000048b, "IA32_VMX_PROCBASED_CTLS2", Ia32VmxProcBasedCtls2, ReadOnly, UINT64_C(0x57cff00000000), 0, 0), /* value=0x57cff`00000000 */
    MFX(0x0000048c, "IA32_VMX_EPT_VPID_CAP", Ia32VmxEptVpidCap, ReadOnly, UINT64_C(0xf0106334141), 0, 0), /* value=0xf01`06334141 */
    MFX(0x0000048d, "IA32_VMX_TRUE_PINBASED_CTLS", Ia32VmxTruePinbasedCtls, ReadOnly, UINT64_C(0x7f00000016), 0, 0), /* value=0x7f`00000016 */
    MFX(0x0000048e, "IA32_VMX_TRUE_PROCBASED_CTLS", Ia32VmxTrueProcbasedCtls, ReadOnly, UINT64_C(0xfff9fffe04006172), 0, 0), /* value=0xfff9fffe`04006172 */
    MFX(0x0000048f, "IA32_VMX_TRUE_EXIT_CTLS", Ia32VmxTrueExitCtls, ReadOnly, UINT64_C(0x7fffff00036dfb), 0, 0), /* value=0x7fffff`00036dfb */
    MFX(0x00000490, "IA32_VMX_TRUE_ENTRY_CTLS", Ia32VmxTrueEntryCtls, ReadOnly, UINT64_C(0xffff000011fb), 0, 0), /* value=0xffff`000011fb */
    MFX(0x00000491, "IA32_VMX_VMFUNC", Ia32VmxVmFunc, ReadOnly, 0x1, 0, 0), /* value=0x1 */
    RSN(0x000004c1, 0x000004c4, "IA32_A_PMCn", Ia32PmcN, Ia32PmcN, 0x0, 0, UINT64_C(0xffff000000000000)),
    MVO(0x000004e0, "TODO_0000_04e0", 0x1),
    MVO(0x000004e2, "TODO_0000_04e2", 0x5),
    MVO(0x000004e3, "TODO_0000_04e3", 0xff0),
    MVX(0x00000560, "TODO_0000_0560", 0, 0, UINT64_C(0xffffff800000007f)),
    MVX(0x00000561, "TODO_0000_0561", 0x7f, UINT64_C(0x70000007f), UINT32_C(0xffffff80)),
    MVX(0x00000570, "TODO_0000_0570", 0x2100, 0x2100, UINT64_C(0xffffffffffffd272)),
    MVX(0x00000571, "TODO_0000_0571", 0, 0x7, UINT64_C(0xffffffffffffffc8)),
    MVX(0x00000572, "TODO_0000_0572", 0, 0, UINT64_C(0xffff00000000001f)),
    MFN(0x00000600, "IA32_DS_AREA", Ia32DsArea, Ia32DsArea), /* value=0x0 */
    MFX(0x00000601, "I7_SB_MSR_VR_CURRENT_CONFIG", IntelI7SandyVrCurrentConfig, IntelI7SandyVrCurrentConfig, 0, UINT32_C(0x80001fff), UINT64_C(0x800000007fffe000)), /* value=0x40101414`80000100 */
    MFX(0x00000603, "I7_SB_MSR_VR_MISC_CONFIG", IntelI7SandyVrMiscConfig, IntelI7SandyVrMiscConfig, 0, 0, UINT64_C(0xff80000000000000)), /* value=0x360000`00333333 */
    MFO(0x00000606, "I7_SB_MSR_RAPL_POWER_UNIT", IntelI7SandyRaplPowerUnit), /* value=0xa0e03 */
    MVX(0x00000609, "I7_SB_UNK_0000_0609", 0x1a, 0xc0, UINT64_C(0xffffffffffffff00)),
    MFX(0x0000060a, "I7_SB_MSR_PKGC3_IRTL", IntelI7SandyPkgCnIrtlN, IntelI7SandyPkgCnIrtlN, 0x3, 0, UINT64_C(0xffffffffffff6000)), /* value=0x8842 */
    RSN(0x0000060b, 0x0000060c, "I7_SB_MSR_PKGC6_IRTn", IntelI7SandyPkgCnIrtlN, IntelI7SandyPkgCnIrtlN, 0x6, 0, UINT64_C(0xffffffffffff6000)),
    MFO(0x0000060d, "I7_SB_MSR_PKG_C2_RESIDENCY", IntelI7SandyPkgC2Residency), /* value=0x1b`88fad668 */
    MFX(0x00000610, "I7_SB_MSR_PKG_POWER_LIMIT", IntelI7RaplPkgPowerLimit, IntelI7RaplPkgPowerLimit, 0, UINT64_C(0x80ffffff00ffffff), UINT64_C(0x7f000000ff000000)), /* value=0x804280c8`00dd8078 */
    MFO(0x00000611, "I7_SB_MSR_PKG_ENERGY_STATUS", IntelI7RaplPkgEnergyStatus), /* value=0x7e40b254 */
    MFO(0x00000613, "I7_SB_MSR_PKG_PERF_STATUS", IntelI7RaplPkgPerfStatus), /* value=0xff3 */
    MFO(0x00000614, "I7_SB_MSR_PKG_POWER_INFO", IntelI7RaplPkgPowerInfo), /* value=0x78 */
    MVX(0x00000615, "TODO_0000_0615", 0, 0, UINT64_C(0xffffffff00010000)),
    MFX(0x00000618, "I7_SB_MSR_DRAM_POWER_LIMIT", IntelI7RaplDramPowerLimit, IntelI7RaplDramPowerLimit, 0, UINT64_C(0x80feffff00feffff), UINT64_C(0x7f010000ff010000)), /* value=0x805400de`00000000 */
    MFO(0x00000619, "I7_SB_MSR_DRAM_ENERGY_STATUS", IntelI7RaplDramEnergyStatus), /* value=0x9dbe152 */
    MFO(0x0000061b, "I7_SB_MSR_DRAM_PERF_STATUS", IntelI7RaplDramPerfStatus), /* value=0x0 */
    MVO(0x0000061d, "TODO_0000_061d", UINT64_C(0x6e231cb3da)),
    MVX(0x00000620, "TODO_0000_0620", 0x71d, 0, UINT64_C(0xffffffffffff8080)),
    MVO(0x00000621, "TODO_0000_0621", 0x1d),
    MVX(0x00000622, "TODO_0000_0622", 0x1, 0, UINT64_C(0xfffffffffffffffe)),
    MVO(0x00000623, "TODO_0000_0623", 0x1),
    MVO(0x00000630, "TODO_0000_0630", 0),
    MVO(0x00000631, "TODO_0000_0631", 0),
    MVO(0x00000632, "TODO_0000_0632", 0),
    MVX(0x00000633, "TODO_0000_0633", 0x88e4, 0, UINT64_C(0xffffffffffff6000)),
    MVX(0x00000634, "TODO_0000_0634", 0x8945, 0, UINT64_C(0xffffffffffff6000)),
    MVX(0x00000635, "TODO_0000_0635", 0x89ef, 0, UINT64_C(0xffffffffffff6000)),
    MVX(0x00000636, "TODO_0000_0636", 0x6a, 0, UINT64_C(0xffffffffffff0000)),
    MVO(0x00000637, "TODO_0000_0637", UINT64_C(0x43af89cfdf)),
    MFX(0x00000638, "I7_SB_MSR_PP0_POWER_LIMIT", IntelI7RaplPp0PowerLimit, IntelI7RaplPp0PowerLimit, 0, 0, UINT64_C(0xffffffff7f000000)), /* value=0x0 */
    MFO(0x00000639, "I7_SB_MSR_PP0_ENERGY_STATUS", IntelI7RaplPp0EnergyStatus), /* value=0x6f9c685f */
    MFX(0x0000063a, "I7_SB_MSR_PP0_POLICY", IntelI7RaplPp0Policy, IntelI7RaplPp0Policy, 0, 0, UINT64_C(0xffffffffffffffe0)), /* value=0x7 */
    MFX(0x00000640, "I7_HW_MSR_PP0_POWER_LIMIT", IntelI7RaplPp1PowerLimit, IntelI7RaplPp1PowerLimit, 0, 0, UINT64_C(0xffffffff7f000000)), /* value=0x0 */
    MFO(0x00000641, "I7_HW_MSR_PP0_ENERGY_STATUS", IntelI7RaplPp1EnergyStatus), /* value=0x4d471 */
    MFX(0x00000642, "I7_HW_MSR_PP0_POLICY", IntelI7RaplPp1Policy, IntelI7RaplPp1Policy, 0, 0, UINT64_C(0xffffffffffffffe0)), /* value=0xb */
    MFO(0x00000648, "I7_IB_MSR_CONFIG_TDP_NOMINAL", IntelI7IvyConfigTdpNominal), /* value=0x1a */
    MFO(0x00000649, "I7_IB_MSR_CONFIG_TDP_LEVEL1", IntelI7IvyConfigTdpLevel1), /* value=0x6003c */
    MFO(0x0000064a, "I7_IB_MSR_CONFIG_TDP_LEVEL2", IntelI7IvyConfigTdpLevel2), /* value=0x0 */
    MFX(0x0000064b, "I7_IB_MSR_CONFIG_TDP_CONTROL", IntelI7IvyConfigTdpControl, IntelI7IvyConfigTdpControl, 0, 0, UINT64_C(0xffffffff7ffffffc)), /* value=0x80000000 */
    MFX(0x0000064c, "I7_IB_MSR_TURBO_ACTIVATION_RATIO", IntelI7IvyTurboActivationRatio, IntelI7IvyTurboActivationRatio, 0, 0, UINT64_C(0xffffffff7fffff00)), /* value=0x80000019 */
    RFN(0x00000680, 0x0000068f, "MSR_LASTBRANCH_n_FROM_IP", IntelLastBranchFromN, IntelLastBranchFromN),
    MVX(0x00000690, "TODO_0000_0690", 0x1d200000, UINT32_C(0xe6dfffff), ~(uint64_t)UINT32_MAX),
    MVX(0x000006b0, "TODO_0000_06b0", 0x1d000000, UINT32_C(0xe2ffffff), ~(uint64_t)UINT32_MAX),
    MVX(0x000006b1, "TODO_0000_06b1", 0xd000000, UINT32_C(0xf2ffffff), ~(uint64_t)UINT32_MAX),
    RFN(0x000006c0, 0x000006cf, "MSR_LASTBRANCH_n_TO_IP", IntelLastBranchToN, IntelLastBranchToN),
    MFI(0x000006e0, "IA32_TSC_DEADLINE", Ia32TscDeadline), /* value=0x0 */
    MVX(0x00000700, "TODO_0000_0700", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000701, "TODO_0000_0701", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000702, "TODO_0000_0702", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000703, "TODO_0000_0703", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000704, "TODO_0000_0704", 0, 0, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000705, "TODO_0000_0705", 0, 0xf, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000706, "TODO_0000_0706", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000707, "TODO_0000_0707", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000708, "TODO_0000_0708", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000709, "TODO_0000_0709", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000710, "TODO_0000_0710", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000711, "TODO_0000_0711", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000712, "TODO_0000_0712", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000713, "TODO_0000_0713", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000714, "TODO_0000_0714", 0, 0, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000715, "TODO_0000_0715", 0, 0xf, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000716, "TODO_0000_0716", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000717, "TODO_0000_0717", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000718, "TODO_0000_0718", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000719, "TODO_0000_0719", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000720, "TODO_0000_0720", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000721, "TODO_0000_0721", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000722, "TODO_0000_0722", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000723, "TODO_0000_0723", 0, 0, UINT64_C(0xffffffffe0230000)),
    MVX(0x00000724, "TODO_0000_0724", 0, 0, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000725, "TODO_0000_0725", 0, 0xf, UINT64_C(0xfffffffffffffff0)),
    MVX(0x00000726, "TODO_0000_0726", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000727, "TODO_0000_0727", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000728, "TODO_0000_0728", 0, 0, UINT64_C(0xfffff00000000000)),
    MVX(0x00000729, "TODO_0000_0729", 0, 0, UINT64_C(0xfffff00000000000)),
    MFO(0x00000c80, "IA32_DEBUG_INTERFACE", Ia32DebugInterface), /* value=0x40000000 */
    MFX(0xc0000080, "AMD64_EFER", Amd64Efer, Amd64Efer, 0xd01, 0x400, UINT64_C(0xfffffffffffff2fe)),
    MFN(0xc0000081, "AMD64_STAR", Amd64SyscallTarget, Amd64SyscallTarget), /* value=0x230010`00000000 */
    MFN(0xc0000082, "AMD64_STAR64", Amd64LongSyscallTarget, Amd64LongSyscallTarget), /* value=0xfffff802`f9b59200 */
    MFN(0xc0000083, "AMD64_STARCOMPAT", Amd64CompSyscallTarget, Amd64CompSyscallTarget), /* value=0xfffff802`f9b58f40 */
    MFX(0xc0000084, "AMD64_SYSCALL_FLAG_MASK", Amd64SyscallFlagMask, Amd64SyscallFlagMask, 0, ~(uint64_t)UINT32_MAX, 0), /* value=0x4700 */
    MFN(0xc0000100, "AMD64_FS_BASE", Amd64FsBase, Amd64FsBase), /* value=0x212000 */
    MFN(0xc0000101, "AMD64_GS_BASE", Amd64GsBase, Amd64GsBase), /* value=0xffffd001`83740000 */
    MFN(0xc0000102, "AMD64_KERNEL_GS_BASE", Amd64KernelGsBase, Amd64KernelGsBase), /* value=0x210000 */
    MFX(0xc0000103, "AMD64_TSC_AUX", Amd64TscAux, Amd64TscAux, 0, 0, ~(uint64_t)UINT32_MAX), /* value=0x0 */
};
#endif /* !CPUM_DB_STANDALONE */


/**
 * Database entry for Intel(R) Core(TM) i7-5600U CPU @ 2.60GHz.
 */
static CPUMDBENTRY const g_Entry_Intel_Core_i7_5600U =
{
    /*.pszName          = */ "Intel Core i7-5600U",
    /*.pszFullName      = */ "Intel(R) Core(TM) i7-5600U CPU @ 2.60GHz",
    /*.enmVendor        = */ CPUMCPUVENDOR_INTEL,
    /*.uFamily          = */ 6,
    /*.uModel           = */ 61,
    /*.uStepping        = */ 4,
    /*.enmMicroarch     = */ kCpumMicroarch_Intel_Core7_Broadwell,
    /*.uScalableBusFreq = */ CPUM_SBUSFREQ_100MHZ,
    /*.fFlags           = */ 0,
    /*.cMaxPhysAddrWidth= */ 39,
    /*.fMxCsrMask       = */ 0xffff,
    /*.paCpuIdLeaves    = */ NULL_ALONE(g_aCpuIdLeaves_Intel_Core_i7_5600U),
    /*.cCpuIdLeaves     = */ ZERO_ALONE(RT_ELEMENTS(g_aCpuIdLeaves_Intel_Core_i7_5600U)),
    /*.enmUnknownCpuId  = */ CPUMUNKNOWNCPUID_LAST_STD_LEAF_WITH_ECX,
    /*.DefUnknownCpuId  = */ { 0x00000000, 0x00000001, 0x00000001, 0x00000000 },
    /*.fMsrMask         = */ UINT32_MAX,
    /*.cMsrRanges       = */ ZERO_ALONE(RT_ELEMENTS(g_aMsrRanges_Intel_Core_i7_5600U)),
    /*.paMsrRanges      = */ NULL_ALONE(g_aMsrRanges_Intel_Core_i7_5600U),
};

#endif /* !VBOX_DB_Intel_Core_i7_5600U */

