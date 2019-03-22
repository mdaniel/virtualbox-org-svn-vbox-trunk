/** @file
 * CPUM - CPU Monitor(/ Manager).
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

#ifndef ___VBox_vmm_cpum_h
#define ___VBox_vmm_cpum_h

#include <iprt/x86.h>
#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/vmm/hm_vmx.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_cpum      The CPU Monitor / Manager API
 * @ingroup grp_vmm
 * @{
 */

/**
 * CPUID feature to set or clear.
 */
typedef enum CPUMCPUIDFEATURE
{
    CPUMCPUIDFEATURE_INVALID = 0,
    /** The APIC feature bit. (Std+Ext)
     * Note! There is a per-cpu flag for masking this CPUID feature bit when the
     *       APICBASE.ENABLED bit is zero.  So, this feature is only set/cleared
     *       at VM construction time like all the others.  This didn't used to be
     *       that way, this is new with 5.1. */
    CPUMCPUIDFEATURE_APIC,
    /** The sysenter/sysexit feature bit. (Std) */
    CPUMCPUIDFEATURE_SEP,
    /** The SYSCALL/SYSEXIT feature bit (64 bits mode only for Intel CPUs). (Ext) */
    CPUMCPUIDFEATURE_SYSCALL,
    /** The PAE feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAE,
    /** The NX feature bit. (Ext) */
    CPUMCPUIDFEATURE_NX,
    /** The LAHF/SAHF feature bit (64 bits mode only). (Ext) */
    CPUMCPUIDFEATURE_LAHF,
    /** The LONG MODE feature bit. (Ext) */
    CPUMCPUIDFEATURE_LONG_MODE,
    /** The PAT feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAT,
    /** The x2APIC  feature bit. (Std) */
    CPUMCPUIDFEATURE_X2APIC,
    /** The RDTSCP feature bit. (Ext) */
    CPUMCPUIDFEATURE_RDTSCP,
    /** The Hypervisor Present bit. (Std) */
    CPUMCPUIDFEATURE_HVP,
    /** The MWait Extensions bits (Std) */
    CPUMCPUIDFEATURE_MWAIT_EXTS,
    /** The speculation control feature bits. (StExt) */
    CPUMCPUIDFEATURE_SPEC_CTRL,
    /** 32bit hackishness. */
    CPUMCPUIDFEATURE_32BIT_HACK = 0x7fffffff
} CPUMCPUIDFEATURE;

/**
 * CPU Vendor.
 */
typedef enum CPUMCPUVENDOR
{
    CPUMCPUVENDOR_INVALID = 0,
    CPUMCPUVENDOR_INTEL,
    CPUMCPUVENDOR_AMD,
    CPUMCPUVENDOR_VIA,
    CPUMCPUVENDOR_CYRIX,
    CPUMCPUVENDOR_UNKNOWN,
    /** 32bit hackishness. */
    CPUMCPUVENDOR_32BIT_HACK = 0x7fffffff
} CPUMCPUVENDOR;


/**
 * X86 and AMD64 CPU microarchitectures and in processor generations.
 *
 * @remarks The separation here is sometimes a little bit too finely grained,
 *          and the differences is more like processor generation than micro
 *          arch.  This can be useful, so we'll provide functions for getting at
 *          more coarse grained info.
 */
typedef enum CPUMMICROARCH
{
    kCpumMicroarch_Invalid = 0,

    kCpumMicroarch_Intel_First,

    kCpumMicroarch_Intel_8086 = kCpumMicroarch_Intel_First,
    kCpumMicroarch_Intel_80186,
    kCpumMicroarch_Intel_80286,
    kCpumMicroarch_Intel_80386,
    kCpumMicroarch_Intel_80486,
    kCpumMicroarch_Intel_P5,

    kCpumMicroarch_Intel_P6_Core_Atom_First,
    kCpumMicroarch_Intel_P6 = kCpumMicroarch_Intel_P6_Core_Atom_First,
    kCpumMicroarch_Intel_P6_II,
    kCpumMicroarch_Intel_P6_III,

    kCpumMicroarch_Intel_P6_M_Banias,
    kCpumMicroarch_Intel_P6_M_Dothan,
    kCpumMicroarch_Intel_Core_Yonah,        /**< Core, also known as Enhanced Pentium M. */

    kCpumMicroarch_Intel_Core2_First,
    kCpumMicroarch_Intel_Core2_Merom = kCpumMicroarch_Intel_Core2_First,    /**< 65nm, Merom/Conroe/Kentsfield/Tigerton */
    kCpumMicroarch_Intel_Core2_Penryn,      /**< 45nm, Penryn/Wolfdale/Yorkfield/Harpertown */
    kCpumMicroarch_Intel_Core2_End,

    kCpumMicroarch_Intel_Core7_First,
    kCpumMicroarch_Intel_Core7_Nehalem = kCpumMicroarch_Intel_Core7_First,
    kCpumMicroarch_Intel_Core7_Westmere,
    kCpumMicroarch_Intel_Core7_SandyBridge,
    kCpumMicroarch_Intel_Core7_IvyBridge,
    kCpumMicroarch_Intel_Core7_Haswell,
    kCpumMicroarch_Intel_Core7_Broadwell,
    kCpumMicroarch_Intel_Core7_Skylake,
    kCpumMicroarch_Intel_Core7_KabyLake,
    kCpumMicroarch_Intel_Core7_CoffeeLake,
    kCpumMicroarch_Intel_Core7_CannonLake,
    kCpumMicroarch_Intel_Core7_IceLake,
    kCpumMicroarch_Intel_Core7_TigerLake,
    kCpumMicroarch_Intel_Core7_End,

    kCpumMicroarch_Intel_Atom_First,
    kCpumMicroarch_Intel_Atom_Bonnell = kCpumMicroarch_Intel_Atom_First,
    kCpumMicroarch_Intel_Atom_Lincroft,     /**< Second generation bonnell (44nm). */
    kCpumMicroarch_Intel_Atom_Saltwell,     /**< 32nm shrink of Bonnell. */
    kCpumMicroarch_Intel_Atom_Silvermont,   /**< 22nm */
    kCpumMicroarch_Intel_Atom_Airmount,     /**< 14nm */
    kCpumMicroarch_Intel_Atom_Goldmont,     /**< 14nm */
    kCpumMicroarch_Intel_Atom_GoldmontPlus, /**< 14nm */
    kCpumMicroarch_Intel_Atom_Unknown,
    kCpumMicroarch_Intel_Atom_End,


    kCpumMicroarch_Intel_Phi_First,
    kCpumMicroarch_Intel_Phi_KnightsFerry = kCpumMicroarch_Intel_Phi_First,
    kCpumMicroarch_Intel_Phi_KnightsCorner,
    kCpumMicroarch_Intel_Phi_KnightsLanding,
    kCpumMicroarch_Intel_Phi_KnightsHill,
    kCpumMicroarch_Intel_Phi_KnightsMill,
    kCpumMicroarch_Intel_Phi_End,

    kCpumMicroarch_Intel_P6_Core_Atom_End,

    kCpumMicroarch_Intel_NB_First,
    kCpumMicroarch_Intel_NB_Willamette = kCpumMicroarch_Intel_NB_First, /**< 180nm */
    kCpumMicroarch_Intel_NB_Northwood,      /**< 130nm */
    kCpumMicroarch_Intel_NB_Prescott,       /**< 90nm */
    kCpumMicroarch_Intel_NB_Prescott2M,     /**< 90nm */
    kCpumMicroarch_Intel_NB_CedarMill,      /**< 65nm */
    kCpumMicroarch_Intel_NB_Gallatin,       /**< 90nm Xeon, Pentium 4 Extreme Edition ("Emergency Edition"). */
    kCpumMicroarch_Intel_NB_Unknown,
    kCpumMicroarch_Intel_NB_End,

    kCpumMicroarch_Intel_Unknown,
    kCpumMicroarch_Intel_End,

    kCpumMicroarch_AMD_First,
    kCpumMicroarch_AMD_Am286 = kCpumMicroarch_AMD_First,
    kCpumMicroarch_AMD_Am386,
    kCpumMicroarch_AMD_Am486,
    kCpumMicroarch_AMD_Am486Enh,            /**< Covers Am5x86 as well. */
    kCpumMicroarch_AMD_K5,
    kCpumMicroarch_AMD_K6,

    kCpumMicroarch_AMD_K7_First,
    kCpumMicroarch_AMD_K7_Palomino = kCpumMicroarch_AMD_K7_First,
    kCpumMicroarch_AMD_K7_Spitfire,
    kCpumMicroarch_AMD_K7_Thunderbird,
    kCpumMicroarch_AMD_K7_Morgan,
    kCpumMicroarch_AMD_K7_Thoroughbred,
    kCpumMicroarch_AMD_K7_Barton,
    kCpumMicroarch_AMD_K7_Unknown,
    kCpumMicroarch_AMD_K7_End,

    kCpumMicroarch_AMD_K8_First,
    kCpumMicroarch_AMD_K8_130nm = kCpumMicroarch_AMD_K8_First, /**< 130nm Clawhammer, Sledgehammer, Newcastle, Paris, Odessa, Dublin */
    kCpumMicroarch_AMD_K8_90nm,             /**< 90nm shrink */
    kCpumMicroarch_AMD_K8_90nm_DualCore,    /**< 90nm with two cores. */
    kCpumMicroarch_AMD_K8_90nm_AMDV,        /**< 90nm with AMD-V (usually) and two cores (usually). */
    kCpumMicroarch_AMD_K8_65nm,             /**< 65nm shrink. */
    kCpumMicroarch_AMD_K8_End,

    kCpumMicroarch_AMD_K10,
    kCpumMicroarch_AMD_K10_Lion,
    kCpumMicroarch_AMD_K10_Llano,
    kCpumMicroarch_AMD_Bobcat,
    kCpumMicroarch_AMD_Jaguar,

    kCpumMicroarch_AMD_15h_First,
    kCpumMicroarch_AMD_15h_Bulldozer = kCpumMicroarch_AMD_15h_First,
    kCpumMicroarch_AMD_15h_Piledriver,
    kCpumMicroarch_AMD_15h_Steamroller,     /**< Yet to be released, might have different family.  */
    kCpumMicroarch_AMD_15h_Excavator,       /**< Yet to be released, might have different family.  */
    kCpumMicroarch_AMD_15h_Unknown,
    kCpumMicroarch_AMD_15h_End,

    kCpumMicroarch_AMD_16h_First,
    kCpumMicroarch_AMD_16h_End,

    kCpumMicroarch_AMD_Zen_First,
    kCpumMicroarch_AMD_Zen_Ryzen = kCpumMicroarch_AMD_Zen_First,
    kCpumMicroarch_AMD_Zen_End,

    kCpumMicroarch_AMD_Unknown,
    kCpumMicroarch_AMD_End,

    kCpumMicroarch_VIA_First,
    kCpumMicroarch_Centaur_C6 = kCpumMicroarch_VIA_First,
    kCpumMicroarch_Centaur_C2,
    kCpumMicroarch_Centaur_C3,
    kCpumMicroarch_VIA_C3_M2,
    kCpumMicroarch_VIA_C3_C5A,          /**< 180nm Samuel - Cyrix III, C3, 1GigaPro. */
    kCpumMicroarch_VIA_C3_C5B,          /**< 150nm Samuel 2 - Cyrix III, C3, 1GigaPro, Eden ESP, XP 2000+. */
    kCpumMicroarch_VIA_C3_C5C,          /**< 130nm Ezra - C3, Eden ESP. */
    kCpumMicroarch_VIA_C3_C5N,          /**< 130nm Ezra-T - C3. */
    kCpumMicroarch_VIA_C3_C5XL,         /**< 130nm Nehemiah - C3, Eden ESP, Eden-N. */
    kCpumMicroarch_VIA_C3_C5P,          /**< 130nm Nehemiah+ - C3. */
    kCpumMicroarch_VIA_C7_C5J,          /**< 90nm Esther - C7, C7-D, C7-M, Eden, Eden ULV. */
    kCpumMicroarch_VIA_Isaiah,
    kCpumMicroarch_VIA_Unknown,
    kCpumMicroarch_VIA_End,

    kCpumMicroarch_Cyrix_First,
    kCpumMicroarch_Cyrix_5x86 = kCpumMicroarch_Cyrix_First,
    kCpumMicroarch_Cyrix_M1,
    kCpumMicroarch_Cyrix_MediaGX,
    kCpumMicroarch_Cyrix_MediaGXm,
    kCpumMicroarch_Cyrix_M2,
    kCpumMicroarch_Cyrix_Unknown,
    kCpumMicroarch_Cyrix_End,

    kCpumMicroarch_NEC_First,
    kCpumMicroarch_NEC_V20 = kCpumMicroarch_NEC_First,
    kCpumMicroarch_NEC_V30,
    kCpumMicroarch_NEC_End,

    kCpumMicroarch_Unknown,

    kCpumMicroarch_32BitHack = 0x7fffffff
} CPUMMICROARCH;


/** Predicate macro for catching netburst CPUs. */
#define CPUMMICROARCH_IS_INTEL_NETBURST(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_NB_First && (a_enmMicroarch) <= kCpumMicroarch_Intel_NB_End)

/** Predicate macro for catching Core7 CPUs. */
#define CPUMMICROARCH_IS_INTEL_CORE7(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_Core7_First && (a_enmMicroarch) <= kCpumMicroarch_Intel_Core7_End)

/** Predicate macro for catching Core 2 CPUs. */
#define CPUMMICROARCH_IS_INTEL_CORE2(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_Core2_First && (a_enmMicroarch) <= kCpumMicroarch_Intel_Core2_End)

/** Predicate macro for catching Atom CPUs, Silvermont and upwards. */
#define CPUMMICROARCH_IS_INTEL_SILVERMONT_PLUS(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_Atom_Silvermont && (a_enmMicroarch) <= kCpumMicroarch_Intel_Atom_End)

/** Predicate macro for catching AMD Family OFh CPUs (aka K8).    */
#define CPUMMICROARCH_IS_AMD_FAM_0FH(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_K8_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_K8_End)

/** Predicate macro for catching AMD Family 10H CPUs (aka K10).    */
#define CPUMMICROARCH_IS_AMD_FAM_10H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10)

/** Predicate macro for catching AMD Family 11H CPUs (aka Lion).    */
#define CPUMMICROARCH_IS_AMD_FAM_11H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10_Lion)

/** Predicate macro for catching AMD Family 12H CPUs (aka Llano).    */
#define CPUMMICROARCH_IS_AMD_FAM_12H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10_Llano)

/** Predicate macro for catching AMD Family 14H CPUs (aka Bobcat).    */
#define CPUMMICROARCH_IS_AMD_FAM_14H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_Bobcat)

/** Predicate macro for catching AMD Family 15H CPUs (bulldozer and it's
 * decendants). */
#define CPUMMICROARCH_IS_AMD_FAM_15H(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_15h_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_15h_End)

/** Predicate macro for catching AMD Family 16H CPUs. */
#define CPUMMICROARCH_IS_AMD_FAM_16H(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_16h_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_16h_End)



/**
 * CPUID leaf.
 *
 * @remarks This structure is used by the patch manager and is therefore
 *          more or less set in stone.
 */
typedef struct CPUMCPUIDLEAF
{
    /** The leaf number. */
    uint32_t    uLeaf;
    /** The sub-leaf number. */
    uint32_t    uSubLeaf;
    /** Sub-leaf mask.  This is 0 when sub-leaves aren't used. */
    uint32_t    fSubLeafMask;

    /** The EAX value. */
    uint32_t    uEax;
    /** The EBX value. */
    uint32_t    uEbx;
    /** The ECX value. */
    uint32_t    uEcx;
    /** The EDX value. */
    uint32_t    uEdx;

    /** Flags. */
    uint32_t    fFlags;
} CPUMCPUIDLEAF;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMCPUIDLEAF, 32);
#endif
/** Pointer to a CPUID leaf. */
typedef CPUMCPUIDLEAF *PCPUMCPUIDLEAF;
/** Pointer to a const CPUID leaf. */
typedef CPUMCPUIDLEAF const *PCCPUMCPUIDLEAF;

/** @name CPUMCPUIDLEAF::fFlags
 * @{ */
/** Indicates working intel leaf 0xb where the lower 8 ECX bits are not modified
 * and EDX containing the extended APIC ID. */
#define CPUMCPUIDLEAF_F_INTEL_TOPOLOGY_SUBLEAVES    RT_BIT_32(0)
/** The leaf contains an APIC ID that needs changing to that of the current CPU. */
#define CPUMCPUIDLEAF_F_CONTAINS_APIC_ID            RT_BIT_32(1)
/** The leaf contains an OSXSAVE which needs individual handling on each CPU. */
#define CPUMCPUIDLEAF_F_CONTAINS_OSXSAVE            RT_BIT_32(2)
/** The leaf contains an APIC feature bit which is tied to APICBASE.EN. */
#define CPUMCPUIDLEAF_F_CONTAINS_APIC               RT_BIT_32(3)
/** Mask of the valid flags. */
#define CPUMCPUIDLEAF_F_VALID_MASK                  UINT32_C(0xf)
/** @} */

/**
 * Method used to deal with unknown CPUID leaves.
 * @remarks Used in patch code.
 */
typedef enum CPUMUNKNOWNCPUID
{
    /** Invalid zero value. */
    CPUMUNKNOWNCPUID_INVALID = 0,
    /** Use given default values (DefCpuId). */
    CPUMUNKNOWNCPUID_DEFAULTS,
    /** Return the last standard leaf.
     * Intel Sandy Bridge has been observed doing this. */
    CPUMUNKNOWNCPUID_LAST_STD_LEAF,
    /** Return the last standard leaf, with ecx observed.
     * Intel Sandy Bridge has been observed doing this. */
    CPUMUNKNOWNCPUID_LAST_STD_LEAF_WITH_ECX,
    /** The register values are passed thru unmodified. */
    CPUMUNKNOWNCPUID_PASSTHRU,
    /** End of valid value. */
    CPUMUNKNOWNCPUID_END,
    /** Ensure 32-bit type. */
    CPUMUNKNOWNCPUID_32BIT_HACK = 0x7fffffff
} CPUMUNKNOWNCPUID;
/** Pointer to unknown CPUID leaf method. */
typedef CPUMUNKNOWNCPUID *PCPUMUNKNOWNCPUID;


/**
 * MSR read functions.
 */
typedef enum CPUMMSRRDFN
{
    /** Invalid zero value. */
    kCpumMsrRdFn_Invalid = 0,
    /** Return the CPUMMSRRANGE::uValue. */
    kCpumMsrRdFn_FixedValue,
    /** Alias to the MSR range starting at the MSR given by
     * CPUMMSRRANGE::uValue.  Must be used in pair with
     * kCpumMsrWrFn_MsrAlias. */
    kCpumMsrRdFn_MsrAlias,
    /** Write only register, GP all read attempts. */
    kCpumMsrRdFn_WriteOnly,

    kCpumMsrRdFn_Ia32P5McAddr,
    kCpumMsrRdFn_Ia32P5McType,
    kCpumMsrRdFn_Ia32TimestampCounter,
    kCpumMsrRdFn_Ia32PlatformId,            /**< Takes real CPU value for reference. */
    kCpumMsrRdFn_Ia32ApicBase,
    kCpumMsrRdFn_Ia32FeatureControl,
    kCpumMsrRdFn_Ia32BiosSignId,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32SmmMonitorCtl,
    kCpumMsrRdFn_Ia32PmcN,
    kCpumMsrRdFn_Ia32MonitorFilterLineSize,
    kCpumMsrRdFn_Ia32MPerf,
    kCpumMsrRdFn_Ia32APerf,
    kCpumMsrRdFn_Ia32MtrrCap,               /**< Takes real CPU value for reference.  */
    kCpumMsrRdFn_Ia32MtrrPhysBaseN,         /**< Takes register number. */
    kCpumMsrRdFn_Ia32MtrrPhysMaskN,         /**< Takes register number. */
    kCpumMsrRdFn_Ia32MtrrFixed,             /**< Takes CPUMCPU offset. */
    kCpumMsrRdFn_Ia32MtrrDefType,
    kCpumMsrRdFn_Ia32Pat,
    kCpumMsrRdFn_Ia32SysEnterCs,
    kCpumMsrRdFn_Ia32SysEnterEsp,
    kCpumMsrRdFn_Ia32SysEnterEip,
    kCpumMsrRdFn_Ia32McgCap,
    kCpumMsrRdFn_Ia32McgStatus,
    kCpumMsrRdFn_Ia32McgCtl,
    kCpumMsrRdFn_Ia32DebugCtl,
    kCpumMsrRdFn_Ia32SmrrPhysBase,
    kCpumMsrRdFn_Ia32SmrrPhysMask,
    kCpumMsrRdFn_Ia32PlatformDcaCap,
    kCpumMsrRdFn_Ia32CpuDcaCap,
    kCpumMsrRdFn_Ia32Dca0Cap,
    kCpumMsrRdFn_Ia32PerfEvtSelN,           /**< Range value indicates the register number. */
    kCpumMsrRdFn_Ia32PerfStatus,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32PerfCtl,               /**< Range value returned. */
    kCpumMsrRdFn_Ia32FixedCtrN,             /**< Takes register number of start of range. */
    kCpumMsrRdFn_Ia32PerfCapabilities,      /**< Takes reference value. */
    kCpumMsrRdFn_Ia32FixedCtrCtrl,
    kCpumMsrRdFn_Ia32PerfGlobalStatus,      /**< Takes reference value. */
    kCpumMsrRdFn_Ia32PerfGlobalCtrl,
    kCpumMsrRdFn_Ia32PerfGlobalOvfCtrl,
    kCpumMsrRdFn_Ia32PebsEnable,
    kCpumMsrRdFn_Ia32ClockModulation,       /**< Range value returned. */
    kCpumMsrRdFn_Ia32ThermInterrupt,        /**< Range value returned. */
    kCpumMsrRdFn_Ia32ThermStatus,           /**< Range value returned. */
    kCpumMsrRdFn_Ia32Therm2Ctl,             /**< Range value returned. */
    kCpumMsrRdFn_Ia32MiscEnable,            /**< Range value returned. */
    kCpumMsrRdFn_Ia32McCtlStatusAddrMiscN,  /**< Takes bank number. */
    kCpumMsrRdFn_Ia32McNCtl2,               /**< Takes register number of start of range. */
    kCpumMsrRdFn_Ia32DsArea,
    kCpumMsrRdFn_Ia32TscDeadline,
    kCpumMsrRdFn_Ia32X2ApicN,
    kCpumMsrRdFn_Ia32DebugInterface,
    kCpumMsrRdFn_Ia32VmxBasic,              /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxPinbasedCtls,       /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxProcbasedCtls,      /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxExitCtls,           /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxEntryCtls,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxMisc,               /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr0Fixed0,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr0Fixed1,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr4Fixed0,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxCr4Fixed1,          /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxVmcsEnum,           /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxProcBasedCtls2,     /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxEptVpidCap,         /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTruePinbasedCtls,   /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueProcbasedCtls,  /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueExitCtls,       /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxTrueEntryCtls,      /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32VmxVmFunc,             /**< Takes real value as reference. */
    kCpumMsrRdFn_Ia32SpecCtrl,
    kCpumMsrRdFn_Ia32ArchCapabilities,

    kCpumMsrRdFn_Amd64Efer,
    kCpumMsrRdFn_Amd64SyscallTarget,
    kCpumMsrRdFn_Amd64LongSyscallTarget,
    kCpumMsrRdFn_Amd64CompSyscallTarget,
    kCpumMsrRdFn_Amd64SyscallFlagMask,
    kCpumMsrRdFn_Amd64FsBase,
    kCpumMsrRdFn_Amd64GsBase,
    kCpumMsrRdFn_Amd64KernelGsBase,
    kCpumMsrRdFn_Amd64TscAux,

    kCpumMsrRdFn_IntelEblCrPowerOn,
    kCpumMsrRdFn_IntelI7CoreThreadCount,
    kCpumMsrRdFn_IntelP4EbcHardPowerOn,
    kCpumMsrRdFn_IntelP4EbcSoftPowerOn,
    kCpumMsrRdFn_IntelP4EbcFrequencyId,
    kCpumMsrRdFn_IntelP6FsbFrequency,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelPlatformInfo,
    kCpumMsrRdFn_IntelFlexRatio,            /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelPkgCStConfigControl,
    kCpumMsrRdFn_IntelPmgIoCaptureBase,
    kCpumMsrRdFn_IntelLastBranchFromToN,
    kCpumMsrRdFn_IntelLastBranchFromN,
    kCpumMsrRdFn_IntelLastBranchToN,
    kCpumMsrRdFn_IntelLastBranchTos,
    kCpumMsrRdFn_IntelBblCrCtl,
    kCpumMsrRdFn_IntelBblCrCtl3,
    kCpumMsrRdFn_IntelI7TemperatureTarget,  /**< Range value returned. */
    kCpumMsrRdFn_IntelI7MsrOffCoreResponseN,/**< Takes register number. */
    kCpumMsrRdFn_IntelI7MiscPwrMgmt,
    kCpumMsrRdFn_IntelP6CrN,
    kCpumMsrRdFn_IntelCpuId1FeatureMaskEcdx,
    kCpumMsrRdFn_IntelCpuId1FeatureMaskEax,
    kCpumMsrRdFn_IntelCpuId80000001FeatureMaskEcdx,
    kCpumMsrRdFn_IntelI7SandyAesNiCtl,
    kCpumMsrRdFn_IntelI7TurboRatioLimit,    /**< Returns range value. */
    kCpumMsrRdFn_IntelI7LbrSelect,
    kCpumMsrRdFn_IntelI7SandyErrorControl,
    kCpumMsrRdFn_IntelI7VirtualLegacyWireCap,/**< Returns range value. */
    kCpumMsrRdFn_IntelI7PowerCtl,
    kCpumMsrRdFn_IntelI7SandyPebsNumAlt,
    kCpumMsrRdFn_IntelI7PebsLdLat,
    kCpumMsrRdFn_IntelI7PkgCnResidencyN,     /**< Takes C-state number. */
    kCpumMsrRdFn_IntelI7CoreCnResidencyN,    /**< Takes C-state number. */
    kCpumMsrRdFn_IntelI7SandyVrCurrentConfig,/**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyVrMiscConfig,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyRaplPowerUnit,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyPkgCnIrtlN,     /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7SandyPkgC2Residency, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgEnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPerfStatus,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPkgPowerInfo,    /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPowerLimit,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramEnergyStatus,/**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPerfStatus,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplDramPowerInfo,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0PowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0EnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0Policy,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp0PerfStatus,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1PowerLimit,   /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1EnergyStatus, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7RaplPp1Policy,       /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpNominal, /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpLevel1,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpLevel2,  /**< Takes real value as reference. */
    kCpumMsrRdFn_IntelI7IvyConfigTdpControl,
    kCpumMsrRdFn_IntelI7IvyTurboActivationRatio,
    kCpumMsrRdFn_IntelI7UncPerfGlobalCtrl,
    kCpumMsrRdFn_IntelI7UncPerfGlobalStatus,
    kCpumMsrRdFn_IntelI7UncPerfGlobalOvfCtrl,
    kCpumMsrRdFn_IntelI7UncPerfFixedCtrCtrl,
    kCpumMsrRdFn_IntelI7UncPerfFixedCtr,
    kCpumMsrRdFn_IntelI7UncCBoxConfig,
    kCpumMsrRdFn_IntelI7UncArbPerfCtrN,
    kCpumMsrRdFn_IntelI7UncArbPerfEvtSelN,
    kCpumMsrRdFn_IntelI7SmiCount,
    kCpumMsrRdFn_IntelCore2EmttmCrTablesN,  /**< Range value returned. */
    kCpumMsrRdFn_IntelCore2SmmCStMiscInfo,
    kCpumMsrRdFn_IntelCore1ExtConfig,
    kCpumMsrRdFn_IntelCore1DtsCalControl,
    kCpumMsrRdFn_IntelCore2PeciControl,
    kCpumMsrRdFn_IntelAtSilvCoreC1Recidency,

    kCpumMsrRdFn_P6LastBranchFromIp,
    kCpumMsrRdFn_P6LastBranchToIp,
    kCpumMsrRdFn_P6LastIntFromIp,
    kCpumMsrRdFn_P6LastIntToIp,

    kCpumMsrRdFn_AmdFam15hTscRate,
    kCpumMsrRdFn_AmdFam15hLwpCfg,
    kCpumMsrRdFn_AmdFam15hLwpCbAddr,
    kCpumMsrRdFn_AmdFam10hMc4MiscN,
    kCpumMsrRdFn_AmdK8PerfCtlN,
    kCpumMsrRdFn_AmdK8PerfCtrN,
    kCpumMsrRdFn_AmdK8SysCfg,               /**< Range value returned. */
    kCpumMsrRdFn_AmdK8HwCr,
    kCpumMsrRdFn_AmdK8IorrBaseN,
    kCpumMsrRdFn_AmdK8IorrMaskN,
    kCpumMsrRdFn_AmdK8TopOfMemN,
    kCpumMsrRdFn_AmdK8NbCfg1,
    kCpumMsrRdFn_AmdK8McXcptRedir,
    kCpumMsrRdFn_AmdK8CpuNameN,
    kCpumMsrRdFn_AmdK8HwThermalCtrl,        /**< Range value returned. */
    kCpumMsrRdFn_AmdK8SwThermalCtrl,
    kCpumMsrRdFn_AmdK8FidVidControl,        /**< Range value returned. */
    kCpumMsrRdFn_AmdK8FidVidStatus,         /**< Range value returned. */
    kCpumMsrRdFn_AmdK8McCtlMaskN,
    kCpumMsrRdFn_AmdK8SmiOnIoTrapN,
    kCpumMsrRdFn_AmdK8SmiOnIoTrapCtlSts,
    kCpumMsrRdFn_AmdK8IntPendingMessage,
    kCpumMsrRdFn_AmdK8SmiTriggerIoCycle,
    kCpumMsrRdFn_AmdFam10hMmioCfgBaseAddr,
    kCpumMsrRdFn_AmdFam10hTrapCtlMaybe,
    kCpumMsrRdFn_AmdFam10hPStateCurLimit,   /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateControl,    /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateStatus,     /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hPStateN,          /**< Returns range value. This isn't an register index! */
    kCpumMsrRdFn_AmdFam10hCofVidControl,    /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hCofVidStatus,     /**< Returns range value. */
    kCpumMsrRdFn_AmdFam10hCStateIoBaseAddr,
    kCpumMsrRdFn_AmdFam10hCpuWatchdogTimer,
    kCpumMsrRdFn_AmdK8SmmBase,
    kCpumMsrRdFn_AmdK8SmmAddr,
    kCpumMsrRdFn_AmdK8SmmMask,
    kCpumMsrRdFn_AmdK8VmCr,
    kCpumMsrRdFn_AmdK8IgnNe,
    kCpumMsrRdFn_AmdK8SmmCtl,
    kCpumMsrRdFn_AmdK8VmHSavePa,
    kCpumMsrRdFn_AmdFam10hVmLockKey,
    kCpumMsrRdFn_AmdFam10hSmmLockKey,
    kCpumMsrRdFn_AmdFam10hLocalSmiStatus,
    kCpumMsrRdFn_AmdFam10hOsVisWrkIdLength,
    kCpumMsrRdFn_AmdFam10hOsVisWrkStatus,
    kCpumMsrRdFn_AmdFam16hL2IPerfCtlN,
    kCpumMsrRdFn_AmdFam16hL2IPerfCtrN,
    kCpumMsrRdFn_AmdFam15hNorthbridgePerfCtlN,
    kCpumMsrRdFn_AmdFam15hNorthbridgePerfCtrN,
    kCpumMsrRdFn_AmdK7MicrocodeCtl,         /**< Returns range value. */
    kCpumMsrRdFn_AmdK7ClusterIdMaybe,       /**< Returns range value. */
    kCpumMsrRdFn_AmdK8CpuIdCtlStd07hEbax,
    kCpumMsrRdFn_AmdK8CpuIdCtlStd06hEcx,
    kCpumMsrRdFn_AmdK8CpuIdCtlStd01hEdcx,
    kCpumMsrRdFn_AmdK8CpuIdCtlExt01hEdcx,
    kCpumMsrRdFn_AmdK8PatchLevel,           /**< Returns range value. */
    kCpumMsrRdFn_AmdK7DebugStatusMaybe,
    kCpumMsrRdFn_AmdK7BHTraceBaseMaybe,
    kCpumMsrRdFn_AmdK7BHTracePtrMaybe,
    kCpumMsrRdFn_AmdK7BHTraceLimitMaybe,
    kCpumMsrRdFn_AmdK7HardwareDebugToolCfgMaybe,
    kCpumMsrRdFn_AmdK7FastFlushCountMaybe,
    kCpumMsrRdFn_AmdK7NodeId,
    kCpumMsrRdFn_AmdK7DrXAddrMaskN,      /**< Takes register index. */
    kCpumMsrRdFn_AmdK7Dr0DataMatchMaybe,
    kCpumMsrRdFn_AmdK7Dr0DataMaskMaybe,
    kCpumMsrRdFn_AmdK7LoadStoreCfg,
    kCpumMsrRdFn_AmdK7InstrCacheCfg,
    kCpumMsrRdFn_AmdK7DataCacheCfg,
    kCpumMsrRdFn_AmdK7BusUnitCfg,
    kCpumMsrRdFn_AmdK7DebugCtl2Maybe,
    kCpumMsrRdFn_AmdFam15hFpuCfg,
    kCpumMsrRdFn_AmdFam15hDecoderCfg,
    kCpumMsrRdFn_AmdFam10hBusUnitCfg2,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg2,
    kCpumMsrRdFn_AmdFam15hCombUnitCfg3,
    kCpumMsrRdFn_AmdFam15hExecUnitCfg,
    kCpumMsrRdFn_AmdFam15hLoadStoreCfg2,
    kCpumMsrRdFn_AmdFam10hIbsFetchCtl,
    kCpumMsrRdFn_AmdFam10hIbsFetchLinAddr,
    kCpumMsrRdFn_AmdFam10hIbsFetchPhysAddr,
    kCpumMsrRdFn_AmdFam10hIbsOpExecCtl,
    kCpumMsrRdFn_AmdFam10hIbsOpRip,
    kCpumMsrRdFn_AmdFam10hIbsOpData,
    kCpumMsrRdFn_AmdFam10hIbsOpData2,
    kCpumMsrRdFn_AmdFam10hIbsOpData3,
    kCpumMsrRdFn_AmdFam10hIbsDcLinAddr,
    kCpumMsrRdFn_AmdFam10hIbsDcPhysAddr,
    kCpumMsrRdFn_AmdFam10hIbsCtl,
    kCpumMsrRdFn_AmdFam14hIbsBrTarget,

    kCpumMsrRdFn_Gim,

    /** End of valid MSR read function indexes. */
    kCpumMsrRdFn_End
} CPUMMSRRDFN;

/**
 * MSR write functions.
 */
typedef enum CPUMMSRWRFN
{
    /** Invalid zero value. */
    kCpumMsrWrFn_Invalid = 0,
    /** Writes are ignored, the fWrGpMask is observed though. */
    kCpumMsrWrFn_IgnoreWrite,
    /** Writes cause GP(0) to be raised, the fWrGpMask should be UINT64_MAX. */
    kCpumMsrWrFn_ReadOnly,
    /** Alias to the MSR range starting at the MSR given by
     * CPUMMSRRANGE::uValue.  Must be used in pair with
     * kCpumMsrRdFn_MsrAlias. */
    kCpumMsrWrFn_MsrAlias,

    kCpumMsrWrFn_Ia32P5McAddr,
    kCpumMsrWrFn_Ia32P5McType,
    kCpumMsrWrFn_Ia32TimestampCounter,
    kCpumMsrWrFn_Ia32ApicBase,
    kCpumMsrWrFn_Ia32FeatureControl,
    kCpumMsrWrFn_Ia32BiosSignId,
    kCpumMsrWrFn_Ia32BiosUpdateTrigger,
    kCpumMsrWrFn_Ia32SmmMonitorCtl,
    kCpumMsrWrFn_Ia32PmcN,
    kCpumMsrWrFn_Ia32MonitorFilterLineSize,
    kCpumMsrWrFn_Ia32MPerf,
    kCpumMsrWrFn_Ia32APerf,
    kCpumMsrWrFn_Ia32MtrrPhysBaseN,         /**< Takes register number. */
    kCpumMsrWrFn_Ia32MtrrPhysMaskN,         /**< Takes register number. */
    kCpumMsrWrFn_Ia32MtrrFixed,             /**< Takes CPUMCPU offset. */
    kCpumMsrWrFn_Ia32MtrrDefType,
    kCpumMsrWrFn_Ia32Pat,
    kCpumMsrWrFn_Ia32SysEnterCs,
    kCpumMsrWrFn_Ia32SysEnterEsp,
    kCpumMsrWrFn_Ia32SysEnterEip,
    kCpumMsrWrFn_Ia32McgStatus,
    kCpumMsrWrFn_Ia32McgCtl,
    kCpumMsrWrFn_Ia32DebugCtl,
    kCpumMsrWrFn_Ia32SmrrPhysBase,
    kCpumMsrWrFn_Ia32SmrrPhysMask,
    kCpumMsrWrFn_Ia32PlatformDcaCap,
    kCpumMsrWrFn_Ia32Dca0Cap,
    kCpumMsrWrFn_Ia32PerfEvtSelN,           /**< Range value indicates the register number. */
    kCpumMsrWrFn_Ia32PerfStatus,
    kCpumMsrWrFn_Ia32PerfCtl,
    kCpumMsrWrFn_Ia32FixedCtrN,             /**< Takes register number of start of range. */
    kCpumMsrWrFn_Ia32PerfCapabilities,
    kCpumMsrWrFn_Ia32FixedCtrCtrl,
    kCpumMsrWrFn_Ia32PerfGlobalStatus,
    kCpumMsrWrFn_Ia32PerfGlobalCtrl,
    kCpumMsrWrFn_Ia32PerfGlobalOvfCtrl,
    kCpumMsrWrFn_Ia32PebsEnable,
    kCpumMsrWrFn_Ia32ClockModulation,
    kCpumMsrWrFn_Ia32ThermInterrupt,
    kCpumMsrWrFn_Ia32ThermStatus,
    kCpumMsrWrFn_Ia32Therm2Ctl,
    kCpumMsrWrFn_Ia32MiscEnable,
    kCpumMsrWrFn_Ia32McCtlStatusAddrMiscN,  /**< Takes bank number. */
    kCpumMsrWrFn_Ia32McNCtl2,               /**< Takes register number of start of range. */
    kCpumMsrWrFn_Ia32DsArea,
    kCpumMsrWrFn_Ia32TscDeadline,
    kCpumMsrWrFn_Ia32X2ApicN,
    kCpumMsrWrFn_Ia32DebugInterface,
    kCpumMsrWrFn_Ia32SpecCtrl,
    kCpumMsrWrFn_Ia32PredCmd,

    kCpumMsrWrFn_Amd64Efer,
    kCpumMsrWrFn_Amd64SyscallTarget,
    kCpumMsrWrFn_Amd64LongSyscallTarget,
    kCpumMsrWrFn_Amd64CompSyscallTarget,
    kCpumMsrWrFn_Amd64SyscallFlagMask,
    kCpumMsrWrFn_Amd64FsBase,
    kCpumMsrWrFn_Amd64GsBase,
    kCpumMsrWrFn_Amd64KernelGsBase,
    kCpumMsrWrFn_Amd64TscAux,
    kCpumMsrWrFn_IntelEblCrPowerOn,
    kCpumMsrWrFn_IntelP4EbcHardPowerOn,
    kCpumMsrWrFn_IntelP4EbcSoftPowerOn,
    kCpumMsrWrFn_IntelP4EbcFrequencyId,
    kCpumMsrWrFn_IntelFlexRatio,
    kCpumMsrWrFn_IntelPkgCStConfigControl,
    kCpumMsrWrFn_IntelPmgIoCaptureBase,
    kCpumMsrWrFn_IntelLastBranchFromToN,
    kCpumMsrWrFn_IntelLastBranchFromN,
    kCpumMsrWrFn_IntelLastBranchToN,
    kCpumMsrWrFn_IntelLastBranchTos,
    kCpumMsrWrFn_IntelBblCrCtl,
    kCpumMsrWrFn_IntelBblCrCtl3,
    kCpumMsrWrFn_IntelI7TemperatureTarget,
    kCpumMsrWrFn_IntelI7MsrOffCoreResponseN, /**< Takes register number. */
    kCpumMsrWrFn_IntelI7MiscPwrMgmt,
    kCpumMsrWrFn_IntelP6CrN,
    kCpumMsrWrFn_IntelCpuId1FeatureMaskEcdx,
    kCpumMsrWrFn_IntelCpuId1FeatureMaskEax,
    kCpumMsrWrFn_IntelCpuId80000001FeatureMaskEcdx,
    kCpumMsrWrFn_IntelI7SandyAesNiCtl,
    kCpumMsrWrFn_IntelI7TurboRatioLimit,
    kCpumMsrWrFn_IntelI7LbrSelect,
    kCpumMsrWrFn_IntelI7SandyErrorControl,
    kCpumMsrWrFn_IntelI7PowerCtl,
    kCpumMsrWrFn_IntelI7SandyPebsNumAlt,
    kCpumMsrWrFn_IntelI7PebsLdLat,
    kCpumMsrWrFn_IntelI7SandyVrCurrentConfig,
    kCpumMsrWrFn_IntelI7SandyVrMiscConfig,
    kCpumMsrWrFn_IntelI7SandyRaplPowerUnit,  /**< R/O but found writable bits on a Silvermont CPU here. */
    kCpumMsrWrFn_IntelI7SandyPkgCnIrtlN,
    kCpumMsrWrFn_IntelI7SandyPkgC2Residency, /**< R/O but found writable bits on a Silvermont CPU here. */
    kCpumMsrWrFn_IntelI7RaplPkgPowerLimit,
    kCpumMsrWrFn_IntelI7RaplDramPowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp0PowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp0Policy,
    kCpumMsrWrFn_IntelI7RaplPp1PowerLimit,
    kCpumMsrWrFn_IntelI7RaplPp1Policy,
    kCpumMsrWrFn_IntelI7IvyConfigTdpControl,
    kCpumMsrWrFn_IntelI7IvyTurboActivationRatio,
    kCpumMsrWrFn_IntelI7UncPerfGlobalCtrl,
    kCpumMsrWrFn_IntelI7UncPerfGlobalStatus,
    kCpumMsrWrFn_IntelI7UncPerfGlobalOvfCtrl,
    kCpumMsrWrFn_IntelI7UncPerfFixedCtrCtrl,
    kCpumMsrWrFn_IntelI7UncPerfFixedCtr,
    kCpumMsrWrFn_IntelI7UncArbPerfCtrN,
    kCpumMsrWrFn_IntelI7UncArbPerfEvtSelN,
    kCpumMsrWrFn_IntelCore2EmttmCrTablesN,
    kCpumMsrWrFn_IntelCore2SmmCStMiscInfo,
    kCpumMsrWrFn_IntelCore1ExtConfig,
    kCpumMsrWrFn_IntelCore1DtsCalControl,
    kCpumMsrWrFn_IntelCore2PeciControl,

    kCpumMsrWrFn_P6LastIntFromIp,
    kCpumMsrWrFn_P6LastIntToIp,

    kCpumMsrWrFn_AmdFam15hTscRate,
    kCpumMsrWrFn_AmdFam15hLwpCfg,
    kCpumMsrWrFn_AmdFam15hLwpCbAddr,
    kCpumMsrWrFn_AmdFam10hMc4MiscN,
    kCpumMsrWrFn_AmdK8PerfCtlN,
    kCpumMsrWrFn_AmdK8PerfCtrN,
    kCpumMsrWrFn_AmdK8SysCfg,
    kCpumMsrWrFn_AmdK8HwCr,
    kCpumMsrWrFn_AmdK8IorrBaseN,
    kCpumMsrWrFn_AmdK8IorrMaskN,
    kCpumMsrWrFn_AmdK8TopOfMemN,
    kCpumMsrWrFn_AmdK8NbCfg1,
    kCpumMsrWrFn_AmdK8McXcptRedir,
    kCpumMsrWrFn_AmdK8CpuNameN,
    kCpumMsrWrFn_AmdK8HwThermalCtrl,
    kCpumMsrWrFn_AmdK8SwThermalCtrl,
    kCpumMsrWrFn_AmdK8FidVidControl,
    kCpumMsrWrFn_AmdK8McCtlMaskN,
    kCpumMsrWrFn_AmdK8SmiOnIoTrapN,
    kCpumMsrWrFn_AmdK8SmiOnIoTrapCtlSts,
    kCpumMsrWrFn_AmdK8IntPendingMessage,
    kCpumMsrWrFn_AmdK8SmiTriggerIoCycle,
    kCpumMsrWrFn_AmdFam10hMmioCfgBaseAddr,
    kCpumMsrWrFn_AmdFam10hTrapCtlMaybe,
    kCpumMsrWrFn_AmdFam10hPStateControl,
    kCpumMsrWrFn_AmdFam10hPStateStatus,
    kCpumMsrWrFn_AmdFam10hPStateN,
    kCpumMsrWrFn_AmdFam10hCofVidControl,
    kCpumMsrWrFn_AmdFam10hCofVidStatus,
    kCpumMsrWrFn_AmdFam10hCStateIoBaseAddr,
    kCpumMsrWrFn_AmdFam10hCpuWatchdogTimer,
    kCpumMsrWrFn_AmdK8SmmBase,
    kCpumMsrWrFn_AmdK8SmmAddr,
    kCpumMsrWrFn_AmdK8SmmMask,
    kCpumMsrWrFn_AmdK8VmCr,
    kCpumMsrWrFn_AmdK8IgnNe,
    kCpumMsrWrFn_AmdK8SmmCtl,
    kCpumMsrWrFn_AmdK8VmHSavePa,
    kCpumMsrWrFn_AmdFam10hVmLockKey,
    kCpumMsrWrFn_AmdFam10hSmmLockKey,
    kCpumMsrWrFn_AmdFam10hLocalSmiStatus,
    kCpumMsrWrFn_AmdFam10hOsVisWrkIdLength,
    kCpumMsrWrFn_AmdFam10hOsVisWrkStatus,
    kCpumMsrWrFn_AmdFam16hL2IPerfCtlN,
    kCpumMsrWrFn_AmdFam16hL2IPerfCtrN,
    kCpumMsrWrFn_AmdFam15hNorthbridgePerfCtlN,
    kCpumMsrWrFn_AmdFam15hNorthbridgePerfCtrN,
    kCpumMsrWrFn_AmdK7MicrocodeCtl,
    kCpumMsrWrFn_AmdK7ClusterIdMaybe,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd07hEbax,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd06hEcx,
    kCpumMsrWrFn_AmdK8CpuIdCtlStd01hEdcx,
    kCpumMsrWrFn_AmdK8CpuIdCtlExt01hEdcx,
    kCpumMsrWrFn_AmdK8PatchLoader,
    kCpumMsrWrFn_AmdK7DebugStatusMaybe,
    kCpumMsrWrFn_AmdK7BHTraceBaseMaybe,
    kCpumMsrWrFn_AmdK7BHTracePtrMaybe,
    kCpumMsrWrFn_AmdK7BHTraceLimitMaybe,
    kCpumMsrWrFn_AmdK7HardwareDebugToolCfgMaybe,
    kCpumMsrWrFn_AmdK7FastFlushCountMaybe,
    kCpumMsrWrFn_AmdK7NodeId,
    kCpumMsrWrFn_AmdK7DrXAddrMaskN,      /**< Takes register index. */
    kCpumMsrWrFn_AmdK7Dr0DataMatchMaybe,
    kCpumMsrWrFn_AmdK7Dr0DataMaskMaybe,
    kCpumMsrWrFn_AmdK7LoadStoreCfg,
    kCpumMsrWrFn_AmdK7InstrCacheCfg,
    kCpumMsrWrFn_AmdK7DataCacheCfg,
    kCpumMsrWrFn_AmdK7BusUnitCfg,
    kCpumMsrWrFn_AmdK7DebugCtl2Maybe,
    kCpumMsrWrFn_AmdFam15hFpuCfg,
    kCpumMsrWrFn_AmdFam15hDecoderCfg,
    kCpumMsrWrFn_AmdFam10hBusUnitCfg2,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg2,
    kCpumMsrWrFn_AmdFam15hCombUnitCfg3,
    kCpumMsrWrFn_AmdFam15hExecUnitCfg,
    kCpumMsrWrFn_AmdFam15hLoadStoreCfg2,
    kCpumMsrWrFn_AmdFam10hIbsFetchCtl,
    kCpumMsrWrFn_AmdFam10hIbsFetchLinAddr,
    kCpumMsrWrFn_AmdFam10hIbsFetchPhysAddr,
    kCpumMsrWrFn_AmdFam10hIbsOpExecCtl,
    kCpumMsrWrFn_AmdFam10hIbsOpRip,
    kCpumMsrWrFn_AmdFam10hIbsOpData,
    kCpumMsrWrFn_AmdFam10hIbsOpData2,
    kCpumMsrWrFn_AmdFam10hIbsOpData3,
    kCpumMsrWrFn_AmdFam10hIbsDcLinAddr,
    kCpumMsrWrFn_AmdFam10hIbsDcPhysAddr,
    kCpumMsrWrFn_AmdFam10hIbsCtl,
    kCpumMsrWrFn_AmdFam14hIbsBrTarget,

    kCpumMsrWrFn_Gim,

    /** End of valid MSR write function indexes. */
    kCpumMsrWrFn_End
} CPUMMSRWRFN;

/**
 * MSR range.
 */
typedef struct CPUMMSRRANGE
{
    /** The first MSR. [0] */
    uint32_t    uFirst;
    /** The last MSR. [4] */
    uint32_t    uLast;
    /** The read function (CPUMMSRRDFN). [8] */
    uint16_t    enmRdFn;
    /** The write function (CPUMMSRWRFN). [10] */
    uint16_t    enmWrFn;
    /** The offset of the 64-bit MSR value relative to the start of CPUMCPU.
     * UINT16_MAX if not used by the read and write functions.  [12] */
    uint16_t    offCpumCpu;
    /** Reserved for future hacks. [14] */
    uint16_t    fReserved;
    /** The init/read value. [16]
     * When enmRdFn is kCpumMsrRdFn_INIT_VALUE, this is the value returned on RDMSR.
     * offCpumCpu must be UINT16_MAX in that case, otherwise it must be a valid
     * offset into CPUM. */
    uint64_t    uValue;
    /** The bits to ignore when writing. [24]   */
    uint64_t    fWrIgnMask;
    /** The bits that will cause a GP(0) when writing. [32]
     * This is always checked prior to calling the write function.  Using
     * UINT64_MAX effectively marks the MSR as read-only. */
    uint64_t    fWrGpMask;
    /** The register name, if applicable. [40] */
    char        szName[56];

#ifdef VBOX_WITH_STATISTICS
    /** The number of reads. */
    STAMCOUNTER cReads;
    /** The number of writes. */
    STAMCOUNTER cWrites;
    /** The number of times ignored bits were written. */
    STAMCOUNTER cIgnoredBits;
    /** The number of GPs generated. */
    STAMCOUNTER cGps;
#endif
} CPUMMSRRANGE;
#ifndef VBOX_FOR_DTRACE_LIB
# ifdef VBOX_WITH_STATISTICS
AssertCompileSize(CPUMMSRRANGE, 128);
# else
AssertCompileSize(CPUMMSRRANGE, 96);
# endif
#endif
/** Pointer to an MSR range. */
typedef CPUMMSRRANGE *PCPUMMSRRANGE;
/** Pointer to a const MSR range. */
typedef CPUMMSRRANGE const *PCCPUMMSRRANGE;


/**
 * CPU features and quirks.
 * This is mostly exploded CPUID info.
 */
typedef struct CPUMFEATURES
{
    /** The CPU vendor (CPUMCPUVENDOR). */
    uint8_t         enmCpuVendor;
    /** The CPU family. */
    uint8_t         uFamily;
    /** The CPU model. */
    uint8_t         uModel;
    /** The CPU stepping. */
    uint8_t         uStepping;
    /** The microarchitecture. */
#ifndef VBOX_FOR_DTRACE_LIB
    CPUMMICROARCH   enmMicroarch;
#else
    uint32_t        enmMicroarch;
#endif
    /** The maximum physical address width of the CPU. */
    uint8_t         cMaxPhysAddrWidth;
    /** The maximum linear address width of the CPU. */
    uint8_t         cMaxLinearAddrWidth;
    /** Max size of the extended state (or FPU state if no XSAVE). */
    uint16_t        cbMaxExtendedState;

    /** Supports MSRs. */
    uint32_t        fMsr : 1;
    /** Supports the page size extension (4/2 MB pages). */
    uint32_t        fPse : 1;
    /** Supports 36-bit page size extension (4 MB pages can map memory above
     *  4GB). */
    uint32_t        fPse36 : 1;
    /** Supports physical address extension (PAE). */
    uint32_t        fPae : 1;
    /** Page attribute table (PAT) support (page level cache control). */
    uint32_t        fPat : 1;
    /** Supports the FXSAVE and FXRSTOR instructions. */
    uint32_t        fFxSaveRstor : 1;
    /** Supports the XSAVE and XRSTOR instructions. */
    uint32_t        fXSaveRstor : 1;
    /** The XSAVE/XRSTOR bit in CR4 has been set (only applicable for host!). */
    uint32_t        fOpSysXSaveRstor : 1;
    /** Supports MMX. */
    uint32_t        fMmx : 1;
    /** Supports AMD extensions to MMX instructions. */
    uint32_t        fAmdMmxExts : 1;
    /** Supports SSE. */
    uint32_t        fSse : 1;
    /** Supports SSE2. */
    uint32_t        fSse2 : 1;
    /** Supports SSE3. */
    uint32_t        fSse3 : 1;
    /** Supports SSSE3. */
    uint32_t        fSsse3 : 1;
    /** Supports SSE4.1. */
    uint32_t        fSse41 : 1;
    /** Supports SSE4.2. */
    uint32_t        fSse42 : 1;
    /** Supports AVX. */
    uint32_t        fAvx : 1;
    /** Supports AVX2. */
    uint32_t        fAvx2 : 1;
    /** Supports AVX512 foundation. */
    uint32_t        fAvx512Foundation : 1;
    /** Supports RDTSC. */
    uint32_t        fTsc : 1;
    /** Intel SYSENTER/SYSEXIT support */
    uint32_t        fSysEnter : 1;
    /** First generation APIC. */
    uint32_t        fApic : 1;
    /** Second generation APIC. */
    uint32_t        fX2Apic : 1;
    /** Hypervisor present. */
    uint32_t        fHypervisorPresent : 1;
    /** MWAIT & MONITOR instructions supported. */
    uint32_t        fMonitorMWait : 1;
    /** MWAIT Extensions present. */
    uint32_t        fMWaitExtensions : 1;
    /** Supports CMPXCHG16B in 64-bit mode. */
    uint32_t        fMovCmpXchg16b : 1;
    /** Supports CLFLUSH. */
    uint32_t        fClFlush : 1;
    /** Supports CLFLUSHOPT. */
    uint32_t        fClFlushOpt : 1;
    /** Supports IA32_PRED_CMD.IBPB. */
    uint32_t        fIbpb : 1;
    /** Supports IA32_SPEC_CTRL.IBRS. */
    uint32_t        fIbrs : 1;
    /** Supports IA32_SPEC_CTRL.STIBP. */
    uint32_t        fStibp : 1;
    /** Supports IA32_ARCH_CAP. */
    uint32_t        fArchCap : 1;
    /** Supports PCID. */
    uint32_t        fPcid : 1;
    /** Supports INVPCID. */
    uint32_t        fInvpcid : 1;
    /** Supports read/write FSGSBASE instructions. */
    uint32_t        fFsGsBase : 1;

    /** Supports AMD 3DNow instructions. */
    uint32_t        f3DNow : 1;
    /** Supports the 3DNow/AMD64 prefetch instructions (could be nops). */
    uint32_t        f3DNowPrefetch : 1;

    /** AMD64: Supports long mode. */
    uint32_t        fLongMode : 1;
    /** AMD64: SYSCALL/SYSRET support. */
    uint32_t        fSysCall : 1;
    /** AMD64: No-execute page table bit. */
    uint32_t        fNoExecute : 1;
    /** AMD64: Supports LAHF & SAHF instructions in 64-bit mode. */
    uint32_t        fLahfSahf : 1;
    /** AMD64: Supports RDTSCP. */
    uint32_t        fRdTscP : 1;
    /** AMD64: Supports MOV CR8 in 32-bit code (lock prefix hack). */
    uint32_t        fMovCr8In32Bit : 1;
    /** AMD64: Supports XOP (similar to VEX3/AVX). */
    uint32_t        fXop : 1;

    /** Indicates that FPU instruction and data pointers may leak.
     * This generally applies to recent AMD CPUs, where the FPU IP and DP pointer
     * is only saved and restored if an exception is pending. */
    uint32_t        fLeakyFxSR : 1;

    /** AMD64: Supports AMD SVM. */
    uint32_t        fSvm : 1;

    /** Support for Intel VMX. */
    uint32_t        fVmx : 1;

    /** Indicates that speculative execution control CPUID bits and
     *  MSRs are exposed. The details are different for Intel and
     * AMD but both have similar functionality. */
    uint32_t        fSpeculationControl : 1;

    /** Alignment padding / reserved for future use. */
    uint32_t        fPadding : 15;

    /** SVM: Supports Nested-paging. */
    uint32_t        fSvmNestedPaging : 1;
    /** SVM: Support LBR (Last Branch Record) virtualization. */
    uint32_t        fSvmLbrVirt : 1;
    /** SVM: Supports SVM lock. */
    uint32_t        fSvmSvmLock : 1;
    /** SVM: Supports Next RIP save. */
    uint32_t        fSvmNextRipSave : 1;
    /** SVM: Supports TSC rate MSR. */
    uint32_t        fSvmTscRateMsr : 1;
    /** SVM: Supports VMCB clean bits. */
    uint32_t        fSvmVmcbClean : 1;
    /** SVM: Supports Flush-by-ASID. */
    uint32_t        fSvmFlusbByAsid : 1;
    /** SVM: Supports decode assist. */
    uint32_t        fSvmDecodeAssists : 1;
    /** SVM: Supports Pause filter. */
    uint32_t        fSvmPauseFilter : 1;
    /** SVM: Supports Pause filter threshold. */
    uint32_t        fSvmPauseFilterThreshold : 1;
    /** SVM: Supports AVIC (Advanced Virtual Interrupt Controller). */
    uint32_t        fSvmAvic : 1;
    /** SVM: Supports Virtualized VMSAVE/VMLOAD. */
    uint32_t        fSvmVirtVmsaveVmload : 1;
    /** SVM: Supports VGIF (Virtual Global Interrupt Flag). */
    uint32_t        fSvmVGif : 1;
    /** SVM: Padding / reserved for future features. */
    uint32_t        fSvmPadding0 : 19;
    /** SVM: Maximum supported ASID. */
    uint32_t        uSvmMaxAsid;

    /** VMX: Maximum physical address width. */
    uint8_t         cVmxMaxPhysAddrWidth;
    /** VMX: Padding / reserved for future. */
    uint8_t         abVmxPadding[3];
    /** VMX: Padding / reserved for future.  */
    uint32_t        fVmxPadding0;

    /** @name VMX basic controls.
     * @{ */
    /** VMX: Supports INS/OUTS VM-exit instruction info. */
    uint32_t        fVmxInsOutInfo : 1;
    /** @} */

    /** @name VMX Pin-based controls.
     * @{ */
    /** VMX: Supports external interrupt VM-exit. */
    uint32_t        fVmxExtIntExit : 1;
    /** VMX: Supports NMI VM-exit. */
    uint32_t        fVmxNmiExit : 1;
    /** VMX: Supports Virtual NMIs. */
    uint32_t        fVmxVirtNmi : 1;
    /** VMX: Supports preemption timer. */
    uint32_t        fVmxPreemptTimer : 1;
    /** VMX: Supports posted interrupts. */
    uint32_t        fVmxPostedInt : 1;
    /** @} */

    /** @name VMX Processor-based controls.
     * @{ */
    /** VMX: Supports Interrupt-window exiting. */
    uint32_t        fVmxIntWindowExit : 1;
    /** VMX: Supports TSC offsetting. */
    uint32_t        fVmxTscOffsetting : 1;
    /** VMX: Supports HLT exiting. */
    uint32_t        fVmxHltExit : 1;
    /** VMX: Supports INVLPG exiting. */
    uint32_t        fVmxInvlpgExit : 1;
    /** VMX: Supports MWAIT exiting. */
    uint32_t        fVmxMwaitExit : 1;
    /** VMX: Supports RDPMC exiting. */
    uint32_t        fVmxRdpmcExit : 1;
    /** VMX: Supports RDTSC exiting. */
    uint32_t        fVmxRdtscExit : 1;
    /** VMX: Supports CR3-load exiting. */
    uint32_t        fVmxCr3LoadExit : 1;
    /** VMX: Supports CR3-store exiting. */
    uint32_t        fVmxCr3StoreExit : 1;
    /** VMX: Supports CR8-load exiting. */
    uint32_t        fVmxCr8LoadExit : 1;
    /** VMX: Supports CR8-store exiting. */
    uint32_t        fVmxCr8StoreExit : 1;
    /** VMX: Supports TPR shadow. */
    uint32_t        fVmxUseTprShadow : 1;
    /** VMX: Supports NMI-window exiting. */
    uint32_t        fVmxNmiWindowExit : 1;
    /** VMX: Supports Mov-DRx exiting. */
    uint32_t        fVmxMovDRxExit : 1;
    /** VMX: Supports Unconditional I/O exiting. */
    uint32_t        fVmxUncondIoExit : 1;
    /** VMX: Supportgs I/O bitmaps. */
    uint32_t        fVmxUseIoBitmaps : 1;
    /** VMX: Supports Monitor Trap Flag. */
    uint32_t        fVmxMonitorTrapFlag : 1;
    /** VMX: Supports MSR bitmap. */
    uint32_t        fVmxUseMsrBitmaps : 1;
    /** VMX: Supports MONITOR exiting. */
    uint32_t        fVmxMonitorExit : 1;
    /** VMX: Supports PAUSE exiting. */
    uint32_t        fVmxPauseExit : 1;
    /** VMX: Supports secondary processor-based VM-execution controls. */
    uint32_t        fVmxSecondaryExecCtls : 1;
    /** @} */

    /** @name VMX Secondary processor-based controls.
     * @{ */
    /** VMX: Supports virtualize-APIC access. */
    uint32_t        fVmxVirtApicAccess : 1;
    /** VMX: Supports EPT (Extended Page Tables). */
    uint32_t        fVmxEpt : 1;
    /** VMX: Supports descriptor-table exiting. */
    uint32_t        fVmxDescTableExit : 1;
    /** VMX: Supports RDTSCP. */
    uint32_t        fVmxRdtscp : 1;
    /** VMX: Supports virtualize-x2APIC mode. */
    uint32_t        fVmxVirtX2ApicMode : 1;
    /** VMX: Supports VPID. */
    uint32_t        fVmxVpid : 1;
    /** VMX: Supports WBIND exiting. */
    uint32_t        fVmxWbinvdExit : 1;
    /** VMX: Supports Unrestricted guest. */
    uint32_t        fVmxUnrestrictedGuest : 1;
    /** VMX: Supports APIC-register virtualization. */
    uint32_t        fVmxApicRegVirt : 1;
    /** VMX: Supports virtual-interrupt delivery. */
    uint32_t        fVmxVirtIntDelivery : 1;
    /** VMX: Supports Pause-loop exiting. */
    uint32_t        fVmxPauseLoopExit : 1;
    /** VMX: Supports RDRAND exiting. */
    uint32_t        fVmxRdrandExit : 1;
    /** VMX: Supports INVPCID. */
    uint32_t        fVmxInvpcid : 1;
    /** VMX: Supports VM functions. */
    uint32_t        fVmxVmFunc : 1;
    /** VMX: Supports VMCS shadowing. */
    uint32_t        fVmxVmcsShadowing : 1;
    /** VMX: Supports RDSEED exiting. */
    uint32_t        fVmxRdseedExit : 1;
    /** VMX: Supports PML. */
    uint32_t        fVmxPml : 1;
    /** VMX: Supports EPT-violations \#VE. */
    uint32_t        fVmxEptXcptVe : 1;
    /** VMX: Supports XSAVES/XRSTORS. */
    uint32_t        fVmxXsavesXrstors : 1;
    /** VMX: Supports TSC scaling. */
    uint32_t        fVmxUseTscScaling : 1;
    /** @} */

    /** @name VMX VM-entry controls.
     * @{ */
    /** VMX: Supports load-debug controls on VM-entry. */
    uint32_t        fVmxEntryLoadDebugCtls : 1;
    /** VMX: Supports IA32e mode guest. */
    uint32_t        fVmxIa32eModeGuest : 1;
    /** VMX: Supports load guest EFER MSR on VM-entry. */
    uint32_t        fVmxEntryLoadEferMsr : 1;
    /** VMX: Supports load guest PAT MSR on VM-entry. */
    uint32_t        fVmxEntryLoadPatMsr : 1;
    /** @} */

    /** @name VMX VM-exit controls.
     * @{ */
    /** VMX: Supports save debug controls on VM-exit. */
    uint32_t        fVmxExitSaveDebugCtls : 1;
    /** VMX: Supports host-address space size. */
    uint32_t        fVmxHostAddrSpaceSize : 1;
    /** VMX: Supports acknowledge external interrupt on VM-exit. */
    uint32_t        fVmxExitAckExtInt : 1;
    /** VMX: Supports save guest PAT MSR on VM-exit. */
    uint32_t        fVmxExitSavePatMsr : 1;
    /** VMX: Supports load hsot PAT MSR on VM-exit. */
    uint32_t        fVmxExitLoadPatMsr : 1;
    /** VMX: Supports save guest EFER MSR on VM-exit. */
    uint32_t        fVmxExitSaveEferMsr : 1;
    /** VMX: Supports load host EFER MSR on VM-exit. */
    uint32_t        fVmxExitLoadEferMsr : 1;
    /** VMX: Supports save VMX preemption timer on VM-exit. */
    uint32_t        fVmxSavePreemptTimer : 1;
    /** @} */

    /** @name VMX Miscellaneous data.
     * @{ */
    /** VMX: Supports storing EFER.LMA into IA32e-mode guest field on VM-exit. */
    uint32_t        fVmxExitSaveEferLma : 1;
    /** VMX: Whether Intel PT (Processor Trace) is supported in VMX mode or not. */
    uint32_t        fVmxIntelPt : 1;
    /** VMX: Supports VMWRITE to any valid VMCS field incl. read-only fields, otherwise
     *  VMWRITE cannot modify read-only VM-exit information fields. */
    uint32_t        fVmxVmwriteAll : 1;
    /** VMX: Supports injection of software interrupts, ICEBP on VM-entry for zero
     *  length instructions. */
    uint32_t        fVmxEntryInjectSoftInt : 1;
    /** @} */

    /** VMX: Padding / reserved for future features. */
    uint32_t        fVmxPadding1 : 1;
    uint32_t        fVmxPadding2;
} CPUMFEATURES;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMFEATURES, 48);
#endif
/** Pointer to a CPU feature structure. */
typedef CPUMFEATURES *PCPUMFEATURES;
/** Pointer to a const CPU feature structure. */
typedef CPUMFEATURES const *PCCPUMFEATURES;


#ifndef VBOX_FOR_DTRACE_LIB

/** @name Guest Register Getters.
 * @{ */
VMMDECL(void)       CPUMGetGuestGDTR(PVMCPU pVCpu, PVBOXGDTR pGDTR);
VMMDECL(RTGCPTR)    CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)      CPUMGetGuestTR(PVMCPU pVCpu, PCPUMSELREGHID pHidden);
VMMDECL(RTSEL)      CPUMGetGuestLDTR(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestLdtrEx(PVMCPU pVCpu, uint64_t *pGCPtrBase, uint32_t *pcbLimit);
VMMDECL(uint64_t)   CPUMGetGuestCR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR4(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR8(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestCRx(PVMCPU pVCpu, unsigned iReg, uint64_t *pValue);
VMMDECL(uint32_t)   CPUMGetGuestEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestECX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBP(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestCS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestDS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestES(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestFS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestGS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestSS(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestFlatPC(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestFlatSP(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR1(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR6(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR7(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t *pValue);
VMMDECL(void)       CPUMGetGuestCpuId(PVMCPU pVCpu, uint32_t iLeaf, uint32_t iSubLeaf,
                                      uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
VMMDECL(uint64_t)   CPUMGetGuestEFER(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32MtrrCap(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32FeatureControl(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxBasic(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxPinbasedCtls(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxProcbasedCtls(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxProcbasedCtls2(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxExitCtls(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxEntryCtls(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxMisc(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxCr0Fixed0(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxCr0Fixed1(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxCr4Fixed0(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxCr4Fixed1(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxVmcsEnum(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32VmxVmFunc(PVMCPU pVCpu);
VMM_INT_DECL(uint64_t)  CPUMGetGuestIa32SmmMonitorCtl(PVMCPU pVCpu);
VMMDECL(VBOXSTRICTRC)   CPUMQueryGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t *puValue);
VMMDECL(VBOXSTRICTRC)   CPUMSetGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t uValue);
VMMDECL(CPUMCPUVENDOR)  CPUMGetGuestCpuVendor(PVM pVM);
VMMDECL(CPUMCPUVENDOR)  CPUMGetHostCpuVendor(PVM pVM);
/** @} */

/** @name Guest Register Setters.
 * @{ */
VMMDECL(int)        CPUMSetGuestGDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestIDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestTR(PVMCPU pVCpu, uint16_t tr);
VMMDECL(int)        CPUMSetGuestLDTR(PVMCPU pVCpu, uint16_t ldtr);
VMMDECL(int)        CPUMSetGuestCR0(PVMCPU pVCpu, uint64_t cr0);
VMMDECL(int)        CPUMSetGuestCR2(PVMCPU pVCpu, uint64_t cr2);
VMMDECL(int)        CPUMSetGuestCR3(PVMCPU pVCpu, uint64_t cr3);
VMMDECL(int)        CPUMSetGuestCR4(PVMCPU pVCpu, uint64_t cr4);
VMMDECL(int)        CPUMSetGuestDR0(PVMCPU pVCpu, uint64_t uDr0);
VMMDECL(int)        CPUMSetGuestDR1(PVMCPU pVCpu, uint64_t uDr1);
VMMDECL(int)        CPUMSetGuestDR2(PVMCPU pVCpu, uint64_t uDr2);
VMMDECL(int)        CPUMSetGuestDR3(PVMCPU pVCpu, uint64_t uDr3);
VMMDECL(int)        CPUMSetGuestDR6(PVMCPU pVCpu, uint64_t uDr6);
VMMDECL(int)        CPUMSetGuestDR7(PVMCPU pVCpu, uint64_t uDr7);
VMMDECL(int)        CPUMSetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t Value);
VMM_INT_DECL(int)   CPUMSetGuestXcr0(PVMCPU pVCpu, uint64_t uNewValue);
VMMDECL(int)        CPUMSetGuestEFlags(PVMCPU pVCpu, uint32_t eflags);
VMMDECL(int)        CPUMSetGuestEIP(PVMCPU pVCpu, uint32_t eip);
VMMDECL(int)        CPUMSetGuestEAX(PVMCPU pVCpu, uint32_t eax);
VMMDECL(int)        CPUMSetGuestEBX(PVMCPU pVCpu, uint32_t ebx);
VMMDECL(int)        CPUMSetGuestECX(PVMCPU pVCpu, uint32_t ecx);
VMMDECL(int)        CPUMSetGuestEDX(PVMCPU pVCpu, uint32_t edx);
VMMDECL(int)        CPUMSetGuestESI(PVMCPU pVCpu, uint32_t esi);
VMMDECL(int)        CPUMSetGuestEDI(PVMCPU pVCpu, uint32_t edi);
VMMDECL(int)        CPUMSetGuestESP(PVMCPU pVCpu, uint32_t esp);
VMMDECL(int)        CPUMSetGuestEBP(PVMCPU pVCpu, uint32_t ebp);
VMMDECL(int)        CPUMSetGuestCS(PVMCPU pVCpu, uint16_t cs);
VMMDECL(int)        CPUMSetGuestDS(PVMCPU pVCpu, uint16_t ds);
VMMDECL(int)        CPUMSetGuestES(PVMCPU pVCpu, uint16_t es);
VMMDECL(int)        CPUMSetGuestFS(PVMCPU pVCpu, uint16_t fs);
VMMDECL(int)        CPUMSetGuestGS(PVMCPU pVCpu, uint16_t gs);
VMMDECL(int)        CPUMSetGuestSS(PVMCPU pVCpu, uint16_t ss);
VMMDECL(void)       CPUMSetGuestEFER(PVMCPU pVCpu, uint64_t val);
VMMR3_INT_DECL(void) CPUMR3SetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMR3_INT_DECL(void) CPUMR3ClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMR3_INT_DECL(bool) CPUMR3GetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(bool)       CPUMSetGuestCpuIdPerCpuApicFeature(PVMCPU pVCpu, bool fVisible);
VMMDECL(void)       CPUMSetGuestCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
VMM_INT_DECL(void)  CPUMGuestLazyLoadHiddenCsAndSs(PVMCPU pVCpu);
VMM_INT_DECL(void)  CPUMGuestLazyLoadHiddenSelectorReg(PVMCPU pVCpu, PCPUMSELREG pSReg);
VMM_INT_DECL(void)     CPUMSetGuestTscAux(PVMCPU pVCpu, uint64_t uValue);
VMM_INT_DECL(uint64_t) CPUMGetGuestTscAux(PVMCPU pVCpu);
VMM_INT_DECL(void)     CPUMSetGuestSpecCtrl(PVMCPU pVCpu, uint64_t uValue);
VMM_INT_DECL(uint64_t) CPUMGetGuestSpecCtrl(PVMCPU pVCpu);
/** @} */


/** @name Misc Guest Predicate Functions.
 * @{  */
VMMDECL(bool)       CPUMIsGuestIn16BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestIn32BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestIn64BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestNXEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPageSizeExtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPagingEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestR0WriteProtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealOrV86Mode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPagedProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInLongMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPAEMode(PVMCPU pVCpu);
VMM_INT_DECL(bool)  CPUMIsGuestInRawMode(PVMCPU pVCpu);
/** @} */

/** @name Nested Hardware-Virtualization Helpers.
 * @{  */
VMM_INT_DECL(bool)      CPUMCanSvmNstGstTakePhysIntr(PVMCPU pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(bool)      CPUMCanSvmNstGstTakeVirtIntr(PVMCPU pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(uint8_t)   CPUMGetSvmNstGstInterrupt(PCCPUMCTX pCtx);
VMM_INT_DECL(void)      CPUMSvmVmExitRestoreHostState(PVMCPU pVCpu, PCPUMCTX pCtx);
VMM_INT_DECL(void)      CPUMSvmVmRunSaveHostState(PCPUMCTX pCtx, uint8_t cbInstr);
VMM_INT_DECL(uint64_t)  CPUMApplyNestedGuestTscOffset(PVMCPU pVCpu, uint64_t uTicks);
VMM_INT_DECL(bool)      CPUMCanVmxNstGstTakePhysIntr(PVMCPU pVCpu, PCCPUMCTX pCtx);
VMM_INT_DECL(bool)      CPUMCanVmxNstGstTakeVirtIntr(PVMCPU pVCpu, PCCPUMCTX pCtx);
/** @} */

/** @name Externalized State Helpers.
 * @{ */
/** @def CPUM_ASSERT_NOT_EXTRN
 * Macro for asserting that @a a_fNotExtrn are present.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fNotExtrn     Mask of CPUMCTX_EXTRN_XXX bits to check.
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_ASSERT_NOT_EXTRN(a_pVCpu, a_fNotExtrn) \
    AssertMsg(!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fNotExtrn)), \
              ("%#RX64; a_fNotExtrn=%#RX64\n", (a_pVCpu)->cpum.GstCtx.fExtrn, (a_fNotExtrn)))

/** @def CPUM_IMPORT_EXTRN_RET
 * Macro for making sure the state specified by @a fExtrnImport is present,
 * calling CPUMImportGuestStateOnDemand() to get it if necessary.
 *
 * Will return if CPUMImportGuestStateOnDemand() fails.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fExtrnImport  Mask of CPUMCTX_EXTRN_XXX bits to get.
 * @thread  EMT(a_pVCpu)
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_IMPORT_EXTRN_RET(a_pVCpu, a_fExtrnImport) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* already present, consider this likely */ } \
        else \
        { \
            int rcCpumImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertRCReturn(rcCpumImport, rcCpumImport); \
        } \
    } while (0)

/** @def CPUM_IMPORT_EXTRN_RCSTRICT
 * Macro for making sure the state specified by @a fExtrnImport is present,
 * calling CPUMImportGuestStateOnDemand() to get it if necessary.
 *
 * Will update a_rcStrict if CPUMImportGuestStateOnDemand() fails.
 *
 * @param   a_pVCpu         The cross context virtual CPU structure of the calling EMT.
 * @param   a_fExtrnImport  Mask of CPUMCTX_EXTRN_XXX bits to get.
 * @param   a_rcStrict      Strict status code variable to update on failure.
 * @thread  EMT(a_pVCpu)
 *
 * @remarks Requires VMCPU_INCL_CPUM_GST_CTX to be defined.
 */
#define CPUM_IMPORT_EXTRN_RCSTRICT(a_pVCpu, a_fExtrnImport, a_rcStrict) \
    do { \
        if (!((a_pVCpu)->cpum.GstCtx.fExtrn & (a_fExtrnImport))) \
        { /* already present, consider this likely */ } \
        else \
        { \
            int rcCpumImport = CPUMImportGuestStateOnDemand(a_pVCpu, a_fExtrnImport); \
            AssertStmt(RT_SUCCESS(rcCpumImport) || RT_FAILURE_NP(a_rcStrict), a_rcStrict = rcCpumImport); \
        } \
    } while (0)

VMM_INT_DECL(int) CPUMImportGuestStateOnDemand(PVMCPU pVCpu, uint64_t fExtrnImport);
/** @} */

#ifndef IPRT_WITHOUT_NAMED_UNIONS_AND_STRUCTS

/**
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInRealModeEx(PCCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE);
}

/**
 * Tests if the guest is running in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInRealOrV86ModeEx(PCCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE)
        || pCtx->eflags.Bits.u1VM;  /* Cannot be set in long mode. Intel spec 2.3.1 "System Flags and Fields in IA-32e Mode". */
}

/**
 * Tests if the guest is running in virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInV86ModeEx(PCCPUMCTX pCtx)
{
    return (pCtx->eflags.Bits.u1VM == 1);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInPagedProtectedModeEx(PCPUMCTX pCtx)
{
    return (pCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInLongModeEx(PCCPUMCTX pCtx)
{
    return (pCtx->msrEFER & MSR_K6_EFER_LMA) == MSR_K6_EFER_LMA;
}

VMM_INT_DECL(bool) CPUMIsGuestIn64BitCodeSlow(PCPUMCTX pCtx);

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits protected mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestIn64BitCodeEx(PCPUMCTX pCtx)
{
    if (!(pCtx->msrEFER & MSR_K6_EFER_LMA))
        return false;
    if (!CPUMSELREG_ARE_HIDDEN_PARTS_VALID(NULL, &pCtx->cs))
        return CPUMIsGuestIn64BitCodeSlow(pCtx);
    return pCtx->cs.Attr.n.u1Long;
}

/**
 * Tests if the guest has paging enabled or not.
 *
 * @returns true if paging is enabled, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestPagingEnabledEx(PCCPUMCTX pCtx)
{
    return !!(pCtx->cr0 & X86_CR0_PG);
}

/**
 * Tests if the guest is running in PAE mode or not.
 *
 * @returns true if in PAE mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInPAEModeEx(PCCPUMCTX pCtx)
{
    /* Intel mentions EFER.LMA and EFER.LME in different parts of their spec. We shall use EFER.LMA rather
       than EFER.LME as it reflects if the CPU has entered paging with EFER.LME set.  */
    return (   (pCtx->cr4 & X86_CR4_PAE)
            && CPUMIsGuestPagingEnabledEx(pCtx)
            && !(pCtx->msrEFER & MSR_K6_EFER_LMA));
}

/**
 * Tests if the guest has AMD SVM enabled or not.
 *
 * @returns true if SMV is enabled, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestSvmEnabled(PCCPUMCTX pCtx)
{
    return RT_BOOL(pCtx->msrEFER & MSR_K6_EFER_SVME);
}

/**
 * Tests if the guest has Intel VT-x enabled or not.
 *
 * @returns true if VMX is enabled, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestVmxEnabled(PCCPUMCTX pCtx)
{
    return RT_BOOL(pCtx->cr4 & X86_CR4_VMXE);
}

/**
 * Checks if we are executing inside an SVM nested hardware-virtualized guest.
 *
 * @returns @c true if in SVM nested-guest mode, @c false otherwise.
 * @param   pCtx        Pointer to the context.
 */
DECLINLINE(bool) CPUMIsGuestInSvmNestedHwVirtMode(PCCPUMCTX pCtx)
{
    /*
     * With AMD-V, the VMRUN intercept is a pre-requisite to entering SVM guest-mode.
     * See AMD spec. 15.5 "VMRUN instruction" subsection "Canonicalization and Consistency Checks".
     */
#ifndef IN_RC
    if (   pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM
        || !(pCtx->hwvirt.svm.CTX_SUFF(pVmcb)->ctrl.u64InterceptCtrl & SVM_CTRL_INTERCEPT_VMRUN))
        return false;
    return true;
#else
    NOREF(pCtx);
    return false;
#endif
}

/**
 * Checks if the guest is in VMX non-root operation.
 *
 * @returns @c true if in VMX non-root operation, @c false otherwise.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInVmxNonRootMode(PCCPUMCTX pCtx)
{
#ifndef IN_RC
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_VMX)
        return false;
    Assert(!pCtx->hwvirt.vmx.fInVmxNonRootMode || pCtx->hwvirt.vmx.fInVmxRootMode);
    return pCtx->hwvirt.vmx.fInVmxNonRootMode;
#else
    NOREF(pCtx);
    return false;
#endif
}

/**
 * Checks if the guest is in VMX root operation.
 *
 * @returns @c true if in VMX root operation, @c false otherwise.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestInVmxRootMode(PCCPUMCTX pCtx)
{
#ifndef IN_RC
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_VMX)
        return false;
    return pCtx->hwvirt.vmx.fInVmxRootMode;
#else
    NOREF(pCtx);
    return false;
#endif
}

# ifndef IN_RC

/**
 * Checks if the nested-guest VMCB has the specified ctrl/instruction intercept
 * active.
 *
 * @returns @c true if in intercept is set, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   fIntercept  The SVM control/instruction intercept, see
 *                      SVM_CTRL_INTERCEPT_*.
 */
DECLINLINE(bool) CPUMIsGuestSvmCtrlInterceptSet(PVMCPU pVCpu, PCPUMCTX pCtx, uint64_t fIntercept)
{
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmCtrlInterceptSet(pVCpu, fIntercept);
    return RT_BOOL(pVmcb->ctrl.u64InterceptCtrl & fIntercept);
}

/**
 * Checks if the nested-guest VMCB has the specified CR read intercept active.
 *
 * @returns @c true if in intercept is set, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx    Pointer to the context.
 * @param   uCr     The CR register number (0 to 15).
 */
DECLINLINE(bool) CPUMIsGuestSvmReadCRxInterceptSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t uCr)
{
    Assert(uCr < 16);
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmReadCRxInterceptSet(pVCpu, uCr);
    return RT_BOOL(pVmcb->ctrl.u16InterceptRdCRx & (UINT16_C(1) << uCr));
}

/**
 * Checks if the nested-guest VMCB has the specified CR write intercept active.
 *
 * @returns @c true if in intercept is set, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx    Pointer to the context.
 * @param   uCr     The CR register number (0 to 15).
 */
DECLINLINE(bool) CPUMIsGuestSvmWriteCRxInterceptSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t uCr)
{
    Assert(uCr < 16);
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmWriteCRxInterceptSet(pVCpu, uCr);
    return RT_BOOL(pVmcb->ctrl.u16InterceptWrCRx & (UINT16_C(1) << uCr));
}

/**
 * Checks if the nested-guest VMCB has the specified DR read intercept active.
 *
 * @returns @c true if in intercept is set, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx    Pointer to the context.
 * @param   uDr     The DR register number (0 to 15).
 */
DECLINLINE(bool) CPUMIsGuestSvmReadDRxInterceptSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t uDr)
{
    Assert(uDr < 16);
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmReadDRxInterceptSet(pVCpu, uDr);
    return RT_BOOL(pVmcb->ctrl.u16InterceptRdDRx & (UINT16_C(1) << uDr));
}

/**
 * Checks if the nested-guest VMCB has the specified DR write intercept active.
 *
 * @returns @c true if in intercept is set, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx    Pointer to the context.
 * @param   uDr     The DR register number (0 to 15).
 */
DECLINLINE(bool) CPUMIsGuestSvmWriteDRxInterceptSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t uDr)
{
    Assert(uDr < 16);
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmWriteDRxInterceptSet(pVCpu, uDr);
    return RT_BOOL(pVmcb->ctrl.u16InterceptWrDRx & (UINT16_C(1) << uDr));
}

/**
 * Checks if the nested-guest VMCB has the specified exception intercept active.
 *
 * @returns @c true if in intercept is active, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   uVector     The exception / interrupt vector.
 */
DECLINLINE(bool) CPUMIsGuestSvmXcptInterceptSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t uVector)
{
    Assert(uVector < 32);
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmXcptInterceptSet(pVCpu, uVector);
    return RT_BOOL(pVmcb->ctrl.u32InterceptXcpt & (UINT32_C(1) << uVector));
}

/**
 * Checks if the nested-guest VMCB has virtual-interrupt masking enabled.
 *
 * @returns @c true if virtual-interrupts are masked, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 *
 * @remarks Should only be called when SVM feature is exposed to the guest.
 */
DECLINLINE(bool) CPUMIsGuestSvmVirtIntrMasking(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmVirtIntrMasking(pVCpu);
    return pVmcb->ctrl.IntCtrl.n.u1VIntrMasking;
}

/**
 * Checks if the nested-guest VMCB has nested-paging enabled.
 *
 * @returns @c true if nested-paging is enabled, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 *
 * @remarks Should only be called when SVM feature is exposed to the guest.
 */
DECLINLINE(bool) CPUMIsGuestSvmNestedPagingEnabled(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMIsGuestSvmNestedPagingEnabled(pVCpu);
    return pVmcb->ctrl.NestedPagingCtrl.n.u1NestedPaging;
}

/**
 * Gets the nested-guest VMCB pause-filter count.
 *
 * @returns The pause-filter count.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 *
 * @remarks Should only be called when SVM feature is exposed to the guest.
 */
DECLINLINE(uint16_t) CPUMGetGuestSvmPauseFilterCount(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    if (pCtx->hwvirt.enmHwvirt != CPUMHWVIRT_SVM)
        return false;
    PCSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb); Assert(pVmcb);
    if (HMHasGuestSvmVmcbCached(pVCpu))
        return HMGetGuestSvmPauseFilterCount(pVCpu);
    return pVmcb->ctrl.u16PauseFilterCount;
}

/**
 * Updates the NextRIP (NRIP) field in the nested-guest VMCB.
 *
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   cbInstr     The length of the current instruction in bytes.
 *
 * @remarks Should only be called when SVM feature is exposed to the guest.
 */
DECLINLINE(void) CPUMGuestSvmUpdateNRip(PVMCPU pVCpu, PCCPUMCTX pCtx, uint8_t cbInstr)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_SVM);
    PSVMVMCB pVmcb = pCtx->hwvirt.svm.CTX_SUFF(pVmcb);
    Assert(pVmcb);
    pVmcb->ctrl.u64NextRIP = pCtx->rip + cbInstr;
}

/**
 * Checks whether one of the given Pin-based VM-execution controls are set when
 * executing a nested-guest.
 *
 * @returns @c true if set, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   uPinCtls    The Pin-based VM-execution controls to check.
 *
 * @remarks This does not check if all given controls are set if more than one
 *          control is passed in @a uPinCtl.
 */
DECLINLINE(bool) CPUMIsGuestVmxPinCtlsSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint32_t uPinCtls)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_VMX);
    Assert(pCtx->hwvirt.vmx.fInVmxNonRootMode);
    Assert(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs));
    return RT_BOOL(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs)->u32PinCtls & uPinCtls);
}

/**
 * Checks whether one of the given Processor-based VM-execution controls are set
 * when executing a nested-guest.
 *
 * @returns @c true if set, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   uProcCtls   The Processor-based VM-execution controls to check.
 *
 * @remarks This does not check if all given controls are set if more than one
 *          control is passed in @a uProcCtls.
 */
DECLINLINE(bool) CPUMIsGuestVmxProcCtlsSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint32_t uProcCtls)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_VMX);
    Assert(pCtx->hwvirt.vmx.fInVmxNonRootMode);
    Assert(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs));
    return RT_BOOL(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs)->u32ProcCtls & uProcCtls);
}

/**
 * Checks whether one of the given Secondary Processor-based VM-execution controls
 * are set when executing a nested-guest.
 *
 * @returns @c true if set, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   uProcCtls2  The Secondary Processor-based VM-execution controls to
 *                      check.
 *
 * @remarks This does not check if all given controls are set if more than one
 *          control is passed in @a uProcCtls2.
 */
DECLINLINE(bool) CPUMIsGuestVmxProcCtls2Set(PVMCPU pVCpu, PCCPUMCTX pCtx, uint32_t uProcCtls2)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_VMX);
    Assert(pCtx->hwvirt.vmx.fInVmxNonRootMode);
    Assert(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs));
    return RT_BOOL(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs)->u32ProcCtls2 & uProcCtls2);
}


/**
 * Checks whether one of the given VM-exit controls are set when executing a
 * nested-guest.
 *
 * @returns @c true if set, @c false otherwise.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 * @param   uExitCtls   The VM-exit controls to check.
 *
 * @remarks This does not check if all given controls are set if more than one
 *          control is passed in @a uExitCtls.
 */
DECLINLINE(bool) CPUMIsGuestVmxExitCtlsSet(PVMCPU pVCpu, PCCPUMCTX pCtx, uint32_t uExitCtls)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_VMX);
    Assert(pCtx->hwvirt.vmx.fInVmxNonRootMode);
    Assert(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs));
    return RT_BOOL(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs)->u32ExitCtls & uExitCtls);
}


/**
 * Returns the guest-physical address of the APIC-access page when executing a
 * nested-guest.
 *
 * @returns The APIC-access page guest-physical address.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 * @param   pCtx        Pointer to the context.
 */
DECLINLINE(uint64_t) CPUMGetGuestVmxApicAccessPageAddr(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    RT_NOREF(pVCpu);
    Assert(pCtx->hwvirt.enmHwvirt == CPUMHWVIRT_VMX);
    Assert(pCtx->hwvirt.vmx.fInVmxNonRootMode);
    Assert(pCtx->hwvirt.vmx.CTX_SUFF(pVmcs));
    return pCtx->hwvirt.vmx.CTX_SUFF(pVmcs)->u64AddrApicAccess.u;
}

# endif /* !IN_RC */

#endif /* IPRT_WITHOUT_NAMED_UNIONS_AND_STRUCTS */

/** @} */


/** @name Hypervisor Register Getters.
 * @{ */
VMMDECL(RTSEL)          CPUMGetHyperCS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperDS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperES(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperFS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperGS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperSS(PVMCPU pVCpu);
#if 0 /* these are not correct. */
VMMDECL(uint32_t)       CPUMGetHyperCR0(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR2(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR4(PVMCPU pVCpu);
#endif
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperECX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)       CPUMGetHyperRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(uint32_t)       CPUMGetHyperGDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)          CPUMGetHyperLDTR(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR0(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR1(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR2(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR3(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR6(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR7(PVMCPU pVCpu);
VMMDECL(void)           CPUMGetHyperCtx(PVMCPU pVCpu, PCPUMCTX pCtx);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
/** @} */

/** @name Hypervisor Register Setters.
 * @{ */
VMMDECL(void)           CPUMSetHyperGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperLDTR(PVMCPU pVCpu, RTSEL SelLDTR);
VMMDECL(void)           CPUMSetHyperIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperCR3(PVMCPU pVCpu, uint32_t cr3);
VMMDECL(void)           CPUMSetHyperTR(PVMCPU pVCpu, RTSEL SelTR);
VMMDECL(void)           CPUMSetHyperCS(PVMCPU pVCpu, RTSEL SelCS);
VMMDECL(void)           CPUMSetHyperDS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperES(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperFS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperGS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperSS(PVMCPU pVCpu, RTSEL SelSS);
VMMDECL(void)           CPUMSetHyperESP(PVMCPU pVCpu, uint32_t u32ESP);
VMMDECL(int)            CPUMSetHyperEFlags(PVMCPU pVCpu, uint32_t Efl);
VMMDECL(void)           CPUMSetHyperEIP(PVMCPU pVCpu, uint32_t u32EIP);
VMM_INT_DECL(void)      CPUMSetHyperState(PVMCPU pVCpu, uint32_t u32EIP, uint32_t u32ESP, uint32_t u32EAX, uint32_t u32EDX);
VMMDECL(void)           CPUMSetHyperDR0(PVMCPU pVCpu, RTGCUINTREG uDr0);
VMMDECL(void)           CPUMSetHyperDR1(PVMCPU pVCpu, RTGCUINTREG uDr1);
VMMDECL(void)           CPUMSetHyperDR2(PVMCPU pVCpu, RTGCUINTREG uDr2);
VMMDECL(void)           CPUMSetHyperDR3(PVMCPU pVCpu, RTGCUINTREG uDr3);
VMMDECL(void)           CPUMSetHyperDR6(PVMCPU pVCpu, RTGCUINTREG uDr6);
VMMDECL(void)           CPUMSetHyperDR7(PVMCPU pVCpu, RTGCUINTREG uDr7);
VMMDECL(void)           CPUMSetHyperCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
VMMDECL(int)            CPUMRecalcHyperDRx(PVMCPU pVCpu, uint8_t iGstReg, bool fForceHyper);
/** @} */

VMMDECL(void)           CPUMPushHyper(PVMCPU pVCpu, uint32_t u32);
VMMDECL(int)            CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx);
VMMDECL(PCPUMCTX)       CPUMGetHyperCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetHyperCtxCore(PVMCPU pVCpu);
VMMDECL(PCPUMCTX)       CPUMQueryGuestCtxPtr(PVMCPU pVCpu);
VMM_INT_DECL(PCPUMCTXMSRS) CPUMQueryGuestCtxMsrsPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetGuestCtxCore(PVMCPU pVCpu);
VMM_INT_DECL(int)       CPUMRawEnter(PVMCPU pVCpu);
VMM_INT_DECL(int)       CPUMRawLeave(PVMCPU pVCpu, int rc);
VMMDECL(uint32_t)       CPUMRawGetEFlags(PVMCPU pVCpu);
VMMDECL(void)           CPUMRawSetEFlags(PVMCPU pVCpu, uint32_t fEfl);

/** @name Changed flags.
 * These flags are used to keep track of which important register that
 * have been changed since last they were reset. The only one allowed
 * to clear them is REM!
 * @{
 */
#define CPUM_CHANGED_FPU_REM                    RT_BIT(0)
#define CPUM_CHANGED_CR0                        RT_BIT(1)
#define CPUM_CHANGED_CR4                        RT_BIT(2)
#define CPUM_CHANGED_GLOBAL_TLB_FLUSH           RT_BIT(3)
#define CPUM_CHANGED_CR3                        RT_BIT(4)
#define CPUM_CHANGED_GDTR                       RT_BIT(5)
#define CPUM_CHANGED_IDTR                       RT_BIT(6)
#define CPUM_CHANGED_LDTR                       RT_BIT(7)
#define CPUM_CHANGED_TR                         RT_BIT(8)  /**@< Currently unused. */
#define CPUM_CHANGED_SYSENTER_MSR               RT_BIT(9)
#define CPUM_CHANGED_HIDDEN_SEL_REGS            RT_BIT(10) /**@< Currently unused. */
#define CPUM_CHANGED_CPUID                      RT_BIT(11)
#define CPUM_CHANGED_ALL                        (  CPUM_CHANGED_FPU_REM \
                                                 | CPUM_CHANGED_CR0 \
                                                 | CPUM_CHANGED_CR4 \
                                                 | CPUM_CHANGED_GLOBAL_TLB_FLUSH \
                                                 | CPUM_CHANGED_CR3 \
                                                 | CPUM_CHANGED_GDTR \
                                                 | CPUM_CHANGED_IDTR \
                                                 | CPUM_CHANGED_LDTR \
                                                 | CPUM_CHANGED_TR \
                                                 | CPUM_CHANGED_SYSENTER_MSR \
                                                 | CPUM_CHANGED_HIDDEN_SEL_REGS \
                                                 | CPUM_CHANGED_CPUID )
/** @} */

VMMDECL(void)           CPUMSetChangedFlags(PVMCPU pVCpu, uint32_t fChangedAdd);
VMMR3DECL(uint32_t)     CPUMR3RemEnter(PVMCPU pVCpu, uint32_t *puCpl);
VMMR3DECL(void)         CPUMR3RemLeave(PVMCPU pVCpu, bool fNoOutOfSyncSels);
VMMDECL(bool)           CPUMSupportsXSave(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysEnter(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysCall(PVM pVM);
VMMDECL(bool)           CPUMIsGuestFPUStateActive(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestFPUStateLoaded(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHostFPUStateSaved(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActive(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActivePending(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateGuestDebugState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHyperDebugStateActive(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHyperDebugStateActivePending(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCPL(PVMCPU pVCpu);
VMMDECL(CPUMMODE)       CPUMGetGuestMode(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCodeBits(PVMCPU pVCpu);
VMMDECL(DISCPUMODE)     CPUMGetGuestDisMode(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestMxCsrMask(PVM pVM);
VMMDECL(uint64_t)       CPUMGetGuestScalableBusFrequency(PVM pVM);
VMMDECL(uint64_t)       CPUMGetGuestEferMsrValidMask(PVM pVM);
VMMDECL(int)            CPUMIsGuestEferMsrWriteValid(PVM pVM, uint64_t uCr0, uint64_t uOldEfer, uint64_t uNewEfer,
                                                     uint64_t *puValidEfer);
VMMDECL(void)           CPUMSetGuestEferMsrNoChecks(PVMCPU pVCpu, uint64_t uOldEfer, uint64_t uValidEfer);
VMMDECL(bool)           CPUMIsPatMsrValid(uint64_t uValue);

/** Guest CPU interruptibility level, see CPUMGetGuestInterruptibility(). */
typedef enum CPUMINTERRUPTIBILITY
{
    CPUMINTERRUPTIBILITY_INVALID = 0,
    CPUMINTERRUPTIBILITY_UNRESTRAINED,
    CPUMINTERRUPTIBILITY_INT_INHIBITED,
    CPUMINTERRUPTIBILITY_INT_DISABLED,
    CPUMINTERRUPTIBILITY_NMI_INHIBIT,
    CPUMINTERRUPTIBILITY_GLOBAL_INHIBIT,
    CPUMINTERRUPTIBILITY_END,
    CPUMINTERRUPTIBILITY_32BIT_HACK = 0x7fffffff
} CPUMINTERRUPTIBILITY;

/**
 * Calculates the interruptiblity of the guest.
 *
 * @returns Interruptibility level.
 * @param   pVCpu               The cross context virtual CPU structure.
 */
VMM_INT_DECL(CPUMINTERRUPTIBILITY) CPUMGetGuestInterruptibility(PVMCPU pVCpu);


/** @name Typical scalable bus frequency values.
 * @{ */
/** Special internal value indicating that we don't know the frequency.
 *  @internal  */
#define CPUM_SBUSFREQ_UNKNOWN       UINT64_C(1)
#define CPUM_SBUSFREQ_100MHZ        UINT64_C(100000000)
#define CPUM_SBUSFREQ_133MHZ        UINT64_C(133333333)
#define CPUM_SBUSFREQ_167MHZ        UINT64_C(166666666)
#define CPUM_SBUSFREQ_200MHZ        UINT64_C(200000000)
#define CPUM_SBUSFREQ_267MHZ        UINT64_C(266666666)
#define CPUM_SBUSFREQ_333MHZ        UINT64_C(333333333)
#define CPUM_SBUSFREQ_400MHZ        UINT64_C(400000000)
/** @} */


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPUM ring-3 API
 * @{
 */

VMMR3DECL(int)          CPUMR3Init(PVM pVM);
VMMR3DECL(int)          CPUMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat);
VMMR3DECL(void)         CPUMR3LogCpuIds(PVM pVM);
VMMR3DECL(void)         CPUMR3Relocate(PVM pVM);
VMMR3DECL(int)          CPUMR3Term(PVM pVM);
VMMR3DECL(void)         CPUMR3Reset(PVM pVM);
VMMR3DECL(void)         CPUMR3ResetCpu(PVM pVM, PVMCPU pVCpu);
VMMDECL(bool)           CPUMR3IsStateRestorePending(PVM pVM);
VMMR3DECL(int)          CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd);

VMMR3DECL(int)              CPUMR3CpuIdInsert(PVM pVM, PCPUMCPUIDLEAF pNewLeaf);
VMMR3DECL(int)              CPUMR3CpuIdGetLeaf(PVM pVM, PCPUMCPUIDLEAF pLeaf, uint32_t uLeaf, uint32_t uSubLeaf);
VMMR3DECL(CPUMMICROARCH)    CPUMR3CpuIdDetermineMicroarchEx(CPUMCPUVENDOR enmVendor, uint8_t bFamily,
                                                            uint8_t bModel, uint8_t bStepping);
VMMR3DECL(const char *)     CPUMR3MicroarchName(CPUMMICROARCH enmMicroarch);
VMMR3DECL(int)              CPUMR3CpuIdCollectLeaves(PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves);
VMMR3DECL(int)              CPUMR3CpuIdDetectUnknownLeafMethod(PCPUMUNKNOWNCPUID penmUnknownMethod, PCPUMCPUID pDefUnknown);
VMMR3DECL(const char *)     CPUMR3CpuIdUnknownLeafMethodName(CPUMUNKNOWNCPUID enmUnknownMethod);
VMMR3DECL(CPUMCPUVENDOR)    CPUMR3CpuIdDetectVendorEx(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX);
VMMR3DECL(const char *)     CPUMR3CpuVendorName(CPUMCPUVENDOR enmVendor);
VMMR3DECL(uint32_t)         CPUMR3DeterminHostMxCsrMask(void);

VMMR3DECL(int)              CPUMR3MsrRangesInsert(PVM pVM, PCCPUMMSRRANGE pNewRange);

# if defined(VBOX_WITH_RAW_MODE) || defined(DOXYGEN_RUNNING)
/** @name APIs for the CPUID raw-mode patch (legacy).
 * @{ */
VMMR3_INT_DECL(RCPTRTYPE(PCCPUMCPUID))     CPUMR3GetGuestCpuIdPatmDefRCPtr(PVM pVM);
VMMR3_INT_DECL(uint32_t)                   CPUMR3GetGuestCpuIdPatmStdMax(PVM pVM);
VMMR3_INT_DECL(uint32_t)                   CPUMR3GetGuestCpuIdPatmExtMax(PVM pVM);
VMMR3_INT_DECL(uint32_t)                   CPUMR3GetGuestCpuIdPatmCentaurMax(PVM pVM);
VMMR3_INT_DECL(RCPTRTYPE(PCCPUMCPUID))     CPUMR3GetGuestCpuIdPatmStdRCPtr(PVM pVM);
VMMR3_INT_DECL(RCPTRTYPE(PCCPUMCPUID))     CPUMR3GetGuestCpuIdPatmExtRCPtr(PVM pVM);
VMMR3_INT_DECL(RCPTRTYPE(PCCPUMCPUID))     CPUMR3GetGuestCpuIdPatmCentaurRCPtr(PVM pVM);
/** @} */
# endif

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RC
/** @defgroup grp_cpum_rc    The CPUM Raw-mode Context API
 * @{
 */

/**
 * Calls a guest trap/interrupt handler directly
 *
 * Assumes a trap stack frame has already been setup on the guest's stack!
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 * @param   selCS       Code selector of handler
 * @param   pHandler    GC virtual address of handler
 * @param   eflags      Callee's EFLAGS
 * @param   selSS       Stack selector for handler
 * @param   pEsp        Stack address for handler
 */
DECLASM(void)           CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTRCPTR pHandler,
                                                   uint32_t eflags, uint32_t selSS, RTRCPTR pEsp);

/**
 * Call guest V86 code directly.
 *
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 */
DECLASM(void)           CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

VMMDECL(int)            CPUMHandleLazyFPU(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMRCGetGuestCPL(PVMCPU pVCpu, PCPUMCTXCORE pRegFrame);
#ifdef VBOX_WITH_RAW_RING1
VMMDECL(void)           CPUMRCRecheckRawState(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
#endif
VMMRCDECL(void)         CPUMRCProcessForceFlag(PVMCPU pVCpu);

/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPUM ring-0 API
 * @{
 */
VMMR0_INT_DECL(int)     CPUMR0ModuleInit(void);
VMMR0_INT_DECL(int)     CPUMR0ModuleTerm(void);
VMMR0_INT_DECL(int)     CPUMR0InitVM(PVM pVM);
DECLASM(void)           CPUMR0RegisterVCpuThread(PVMCPU pVCpu);
DECLASM(void)           CPUMR0TouchHostFpu(void);
VMMR0_INT_DECL(int)     CPUMR0Trap07Handler(PVM pVM, PVMCPU pVCpu);
VMMR0_INT_DECL(int)     CPUMR0LoadGuestFPU(PVM pVM, PVMCPU pVCpu);
VMMR0_INT_DECL(bool)    CPUMR0FpuStateMaybeSaveGuestAndRestoreHost(PVMCPU pVCpu);
VMMR0_INT_DECL(int)     CPUMR0SaveHostDebugState(PVM pVM, PVMCPU pVCpu);
VMMR0_INT_DECL(bool)    CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(PVMCPU pVCpu, bool fDr6);
VMMR0_INT_DECL(bool)    CPUMR0DebugStateMaybeSaveGuest(PVMCPU pVCpu, bool fDr6);

VMMR0_INT_DECL(void)    CPUMR0LoadGuestDebugState(PVMCPU pVCpu, bool fDr6);
VMMR0_INT_DECL(void)    CPUMR0LoadHyperDebugState(PVMCPU pVCpu, bool fDr6);
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
VMMR0_INT_DECL(void)    CPUMR0SetLApic(PVMCPU pVCpu, uint32_t iHostCpuSet);
#endif

/** @} */
#endif /* IN_RING0 */

/** @defgroup grp_cpum_rz    The CPUM raw-mode and ring-0 context API
 * @{
 */
VMMRZ_INT_DECL(void)    CPUMRZFpuStatePrepareHostCpuForUse(PVMCPU pVCpu);
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForRead(PVMCPU pVCpu);
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeForChange(PVMCPU pVCpu);
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeSseForRead(PVMCPU pVCpu);
VMMRZ_INT_DECL(void)    CPUMRZFpuStateActualizeAvxForRead(PVMCPU pVCpu);
/** @} */


#endif /* !VBOX_FOR_DTRACE_LIB */
/** @} */
RT_C_DECLS_END


#endif

