/** @file
 * DBGF - Debugger Facility.
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

#ifndef ___VBox_vmm_dbgf_h
#define ___VBox_vmm_dbgf_h

#include <VBox/types.h>
#include <VBox/log.h>                   /* LOG_ENABLED */
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/dbgfsel.h>

#include <iprt/stdarg.h>
#include <iprt/dbg.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_dbgf     The Debugger Facility API
 * @ingroup grp_vmm
 * @{
 */

#if defined(IN_RC) || defined(IN_RING0)
/** @defgroup grp_dbgf_rz  The RZ DBGF API
 * @{
 */
VMMRZ_INT_DECL(int) DBGFRZTrap01Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6, bool fAltStepping);
VMMRZ_INT_DECL(int) DBGFRZTrap03Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame);
/** @} */
#endif



#ifdef IN_RING3

/**
 * Mixed address.
 */
typedef struct DBGFADDRESS
{
    /** The flat address. */
    RTGCUINTPTR FlatPtr;
    /** The selector offset address. */
    RTGCUINTPTR off;
    /** The selector. DBGF_SEL_FLAT is a legal value. */
    RTSEL       Sel;
    /** Flags describing further details about the address. */
    uint16_t    fFlags;
} DBGFADDRESS;
/** Pointer to a mixed address. */
typedef DBGFADDRESS *PDBGFADDRESS;
/** Pointer to a const mixed address. */
typedef const DBGFADDRESS *PCDBGFADDRESS;

/** @name DBGFADDRESS Flags.
 * @{ */
/** A 16:16 far address. */
#define DBGFADDRESS_FLAGS_FAR16         0
/** A 16:32 far address. */
#define DBGFADDRESS_FLAGS_FAR32         1
/** A 16:64 far address. */
#define DBGFADDRESS_FLAGS_FAR64         2
/** A flat address. */
#define DBGFADDRESS_FLAGS_FLAT          3
/** A physical address. */
#define DBGFADDRESS_FLAGS_PHYS          4
/** A ring-0 host address (internal use only). */
#define DBGFADDRESS_FLAGS_RING0         5
/** The address type mask. */
#define DBGFADDRESS_FLAGS_TYPE_MASK     7

/** Set if the address is valid. */
#define DBGFADDRESS_FLAGS_VALID         RT_BIT(3)

/** The address is within the hypervisor memoary area (HMA).
 * If not set, the address can be assumed to be a guest address. */
#define DBGFADDRESS_FLAGS_HMA           RT_BIT(4)

/** Checks if the mixed address is flat or not. */
#define DBGFADDRESS_IS_FLAT(pAddress)    ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FLAT )
/** Checks if the mixed address is flat or not. */
#define DBGFADDRESS_IS_PHYS(pAddress)    ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_PHYS )
/** Checks if the mixed address is far 16:16 or not. */
#define DBGFADDRESS_IS_FAR16(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR16 )
/** Checks if the mixed address is far 16:32 or not. */
#define DBGFADDRESS_IS_FAR32(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR32 )
/** Checks if the mixed address is far 16:64 or not. */
#define DBGFADDRESS_IS_FAR64(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_FAR64 )
/** Checks if the mixed address host context ring-0 (special). */
#define DBGFADDRESS_IS_R0_HC(pAddress)   ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) == DBGFADDRESS_FLAGS_RING0 )
/** Checks if the mixed address a virtual guest context address (incl HMA). */
#define DBGFADDRESS_IS_VIRT_GC(pAddress) ( ((pAddress)->fFlags & DBGFADDRESS_FLAGS_TYPE_MASK) <= DBGFADDRESS_FLAGS_FLAT )
/** Checks if the mixed address is valid. */
#define DBGFADDRESS_IS_VALID(pAddress)   RT_BOOL((pAddress)->fFlags & DBGFADDRESS_FLAGS_VALID)
/** Checks if the address is flagged as within the HMA. */
#define DBGFADDRESS_IS_HMA(pAddress)     RT_BOOL((pAddress)->fFlags & DBGFADDRESS_FLAGS_HMA)
/** @} */

VMMR3DECL(int)          DBGFR3AddrFromSelOff(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddress, RTSEL Sel, RTUINTPTR off);
VMMR3DECL(int)          DBGFR3AddrFromSelInfoOff(PUVM pUVM, PDBGFADDRESS pAddress, PCDBGFSELINFO pSelInfo, RTUINTPTR off);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromFlat(PUVM pUVM, PDBGFADDRESS pAddress, RTGCUINTPTR FlatPtr);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrFromPhys(PUVM pUVM, PDBGFADDRESS pAddress, RTGCPHYS PhysAddr);
VMMR3DECL(bool)         DBGFR3AddrIsValid(PUVM pUVM, PCDBGFADDRESS pAddress);
VMMR3DECL(int)          DBGFR3AddrToPhys(PUVM pUVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, PRTGCPHYS pGCPhys);
VMMR3DECL(int)          DBGFR3AddrToHostPhys(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddress, PRTHCPHYS pHCPhys);
VMMR3DECL(int)          DBGFR3AddrToVolatileR3Ptr(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddress, bool fReadOnly, void **ppvR3Ptr);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrAdd(PDBGFADDRESS pAddress, RTGCUINTPTR uAddend);
VMMR3DECL(PDBGFADDRESS) DBGFR3AddrSub(PDBGFADDRESS pAddress, RTGCUINTPTR uSubtrahend);

#endif /* IN_RING3 */



/**
 * VMM Debug Event Type.
 */
typedef enum DBGFEVENTTYPE
{
    /** Halt completed.
     * This notifies that a halt command have been successfully completed.
     */
    DBGFEVENT_HALT_DONE = 0,
    /** Detach completed.
     * This notifies that the detach command have been successfully completed.
     */
    DBGFEVENT_DETACH_DONE,
    /** The command from the debugger is not recognized.
     * This means internal error or half implemented features.
     */
    DBGFEVENT_INVALID_COMMAND,

    /** Fatal error.
     * This notifies a fatal error in the VMM and that the debugger get's a
     * chance to first hand information about the the problem.
     */
    DBGFEVENT_FATAL_ERROR,
    /** Breakpoint Hit.
     * This notifies that a breakpoint installed by the debugger was hit. The
     * identifier of the breakpoint can be found in the DBGFEVENT::u::Bp::iBp member.
     */
    DBGFEVENT_BREAKPOINT,
    /** I/O port breakpoint.
     * @todo not yet implemented. */
    DBGFEVENT_BREAKPOINT_IO,
    /** MMIO breakpoint.
     * @todo not yet implemented. */
    DBGFEVENT_BREAKPOINT_MMIO,
    /** Breakpoint Hit in the Hypervisor.
     * This notifies that a breakpoint installed by the debugger was hit. The
     * identifier of the breakpoint can be found in the DBGFEVENT::u::Bp::iBp member.
     */
    DBGFEVENT_BREAKPOINT_HYPER,
    /** Assertion in the Hypervisor (breakpoint instruction).
     * This notifies that a breakpoint instruction was hit in the hypervisor context.
     */
    DBGFEVENT_ASSERTION_HYPER,
    /** Single Stepped.
     * This notifies that a single step operation was completed.
     */
    DBGFEVENT_STEPPED,
    /** Single Stepped.
     * This notifies that a hypervisor single step operation was completed.
     */
    DBGFEVENT_STEPPED_HYPER,
    /** The developer have used the DBGFSTOP macro or the PDMDeviceDBGFSTOP function
     * to bring up the debugger at a specific place.
     */
    DBGFEVENT_DEV_STOP,
    /** The VM is powering off.
     * When this notification is received, the debugger thread should detach ASAP.
     */
    DBGFEVENT_POWERING_OFF,

    /** Hardware Interrupt break.
     * @todo not yet implemented. */
    DBGFEVENT_INTERRUPT_HARDWARE,
    /** Software Interrupt break.
     * @todo not yet implemented. */
    DBGFEVENT_INTERRUPT_SOFTWARE,

    /** The first selectable event.
     * Whether the debugger wants or doesn't want these events can be configured
     * via DBGFR3xxx and queried via DBGFR3yyy.  */
    DBGFEVENT_FIRST_SELECTABLE,
    /** Tripple fault. */
    DBGFEVENT_TRIPLE_FAULT = DBGFEVENT_FIRST_SELECTABLE,

    /** @name Exception events
     * The exception events normally represents guest exceptions, but depending on
     * the execution mode some virtualization exceptions may occure (no nested
     * paging, raw-mode, ++).  When necessary, we will request additional VM exits.
     * @{  */
    DBGFEVENT_XCPT_FIRST,           /**< The first exception event. */
    DBGFEVENT_XCPT_DE               /**< 0x00 - \#DE - Fault - NoErr - Integer divide error (zero/overflow). */
        = DBGFEVENT_XCPT_FIRST,
    DBGFEVENT_XCPT_DB,              /**< 0x01 - \#DB - trap/fault - NoErr - debug event. */
    DBGFEVENT_XCPT_02,              /**< 0x02 - Reserved for NMI, see interrupt events. */
    DBGFEVENT_XCPT_BP,              /**< 0x03 - \#BP - Trap  - NoErr - Breakpoint, INT 3 instruction. */
    DBGFEVENT_XCPT_OF,              /**< 0x04 - \#OF - Trap  - NoErr - Overflow, INTO instruction. */
    DBGFEVENT_XCPT_BR,              /**< 0x05 - \#BR - Fault - NoErr - BOUND Range Exceeded, BOUND instruction. */
    DBGFEVENT_XCPT_UD,              /**< 0x06 - \#UD - Fault - NoErr - Undefined(/Invalid) Opcode. */
    DBGFEVENT_XCPT_NM,              /**< 0x07 - \#NM - Fault - NoErr - Device not available, FP or (F)WAIT instruction. */
    DBGFEVENT_XCPT_DF,              /**< 0x08 - \#DF - Abort - Err=0 - Double fault. */
    DBGFEVENT_XCPT_09,              /**< 0x09 - Int9 - Fault - NoErr - Coprocessor Segment Overrun (obsolete). */
    DBGFEVENT_XCPT_TS,              /**< 0x0a - \#TS - Fault - ErrCd - Invalid TSS, Taskswitch or TSS access. */
    DBGFEVENT_XCPT_NP,              /**< 0x0b - \#NP - Fault - ErrCd - Segment not present. */
    DBGFEVENT_XCPT_SS,              /**< 0x0c - \#SS - Fault - ErrCd - Stack-Segment fault. */
    DBGFEVENT_XCPT_GP,              /**< 0x0d - \#GP - Fault - ErrCd - General protection fault. */
    DBGFEVENT_XCPT_PF,              /**< 0x0e - \#PF - Fault - ErrCd - Page fault. - interrupt gate!!! */
    DBGFEVENT_XCPT_0f,              /**< 0x0f - Rsvd - Resvd - Resvd - Intel Reserved. */
    DBGFEVENT_XCPT_MF,              /**< 0x10 - \#MF - Fault - NoErr - x86 FPU Floating-Point Error (Math fault), FP or (F)WAIT instruction. */
    DBGFEVENT_XCPT_AC,              /**< 0x11 - \#AC - Fault - Err=0 - Alignment Check. */
    DBGFEVENT_XCPT_MC,              /**< 0x12 - \#MC - Abort - NoErr - Machine Check. */
    DBGFEVENT_XCPT_XF,              /**< 0x13 - \#XF - Fault - NoErr - SIMD Floating-Point Exception. */
    DBGFEVENT_XCPT_VE,              /**< 0x14 - \#VE - Fault - Noerr - Virtualization exception. */
    DBGFEVENT_XCPT_15,              /**< 0x15 - Intel Reserved. */
    DBGFEVENT_XCPT_16,              /**< 0x16 - Intel Reserved. */
    DBGFEVENT_XCPT_17,              /**< 0x17 - Intel Reserved. */
    DBGFEVENT_XCPT_18,              /**< 0x18 - Intel Reserved. */
    DBGFEVENT_XCPT_19,              /**< 0x19 - Intel Reserved. */
    DBGFEVENT_XCPT_1a,              /**< 0x1a - Intel Reserved. */
    DBGFEVENT_XCPT_1b,              /**< 0x1b - Intel Reserved. */
    DBGFEVENT_XCPT_1c,              /**< 0x1c - Intel Reserved. */
    DBGFEVENT_XCPT_1d,              /**< 0x1d - Intel Reserved. */
    DBGFEVENT_XCPT_SX,              /**< 0x1e - \#SX - Fault - ErrCd - Security Exception. */
    DBGFEVENT_XCPT_1f,              /**< 0x1f - Intel Reserved. */
    DBGFEVENT_XCPT_LAST             /**< The last exception event. */
        = DBGFEVENT_XCPT_1f,
    /** @} */

    /** @name Instruction events
     * The instruction events exerts all possible effort to intercept the
     * relevant instructions.  However, in some execution modes we won't be able
     * to catch them.  So it goes.
     * @{ */
    DBGFEVENT_INSTR_FIRST,          /**< The first VM instruction event. */
    DBGFEVENT_INSTR_HALT            /**< Instruction: HALT */
        = DBGFEVENT_INSTR_FIRST,
    DBGFEVENT_INSTR_MWAIT,          /**< Instruction: MWAIT */
    DBGFEVENT_INSTR_MONITOR,        /**< Instruction: MONITOR */
    DBGFEVENT_INSTR_CPUID,          /**< Instruction: CPUID (missing stuff in raw-mode). */
    DBGFEVENT_INSTR_INVD,           /**< Instruction: INVD */
    DBGFEVENT_INSTR_WBINVD,         /**< Instruction: WBINVD */
    DBGFEVENT_INSTR_INVLPG,         /**< Instruction: INVLPG */
    DBGFEVENT_INSTR_RDTSC,          /**< Instruction: RDTSC */
    DBGFEVENT_INSTR_RDTSCP,         /**< Instruction: RDTSCP */
    DBGFEVENT_INSTR_RDPMC,          /**< Instruction: RDPMC */
    DBGFEVENT_INSTR_RDMSR,          /**< Instruction: RDMSR */
    DBGFEVENT_INSTR_WRMSR,          /**< Instruction: WRMSR */
    DBGFEVENT_INSTR_CRX_READ,       /**< Instruction: CRx read instruction (missing smsw in raw-mode, and reads in general in VT-x). */
    DBGFEVENT_INSTR_CRX_WRITE,      /**< Instruction: CRx write */
    DBGFEVENT_INSTR_DRX_READ,       /**< Instruction: DRx read */
    DBGFEVENT_INSTR_DRX_WRITE,      /**< Instruction: DRx write */
    DBGFEVENT_INSTR_PAUSE,          /**< Instruction: PAUSE instruction (not in raw-mode). */
    DBGFEVENT_INSTR_XSETBV,         /**< Instruction: XSETBV */
    DBGFEVENT_INSTR_SIDT,           /**< Instruction: SIDT */
    DBGFEVENT_INSTR_LIDT,           /**< Instruction: LIDT */
    DBGFEVENT_INSTR_SGDT,           /**< Instruction: SGDT */
    DBGFEVENT_INSTR_LGDT,           /**< Instruction: LGDT */
    DBGFEVENT_INSTR_SLDT,           /**< Instruction: SLDT */
    DBGFEVENT_INSTR_LLDT,           /**< Instruction: LLDT */
    DBGFEVENT_INSTR_STR,            /**< Instruction: STR */
    DBGFEVENT_INSTR_LTR,            /**< Instruction: LTR */
    DBGFEVENT_INSTR_GETSEC,         /**< Instruction: GETSEC */
    DBGFEVENT_INSTR_RSM,            /**< Instruction: RSM */
    DBGFEVENT_INSTR_RDRAND,         /**< Instruction: RDRAND */
    DBGFEVENT_INSTR_RDSEED,         /**< Instruction: RDSEED */
    DBGFEVENT_INSTR_XSAVES,         /**< Instruction: XSAVES */
    DBGFEVENT_INSTR_XRSTORS,        /**< Instruction: XRSTORS */
    DBGFEVENT_INSTR_VMM_CALL,       /**< Instruction: VMCALL (intel) or VMMCALL (AMD) */
    DBGFEVENT_INSTR_LAST_COMMON     /**< Instruction: the last common event. */
        = DBGFEVENT_INSTR_VMM_CALL,
    DBGFEVENT_INSTR_VMX_FIRST,      /**< Instruction: VT-x - First. */
    DBGFEVENT_INSTR_VMX_VMCLEAR     /**< Instruction: VT-x VMCLEAR */
        = DBGFEVENT_INSTR_VMX_FIRST,
    DBGFEVENT_INSTR_VMX_VMLAUNCH,   /**< Instruction: VT-x VMLAUNCH */
    DBGFEVENT_INSTR_VMX_VMPTRLD,    /**< Instruction: VT-x VMPTRLD */
    DBGFEVENT_INSTR_VMX_VMPTRST,    /**< Instruction: VT-x VMPTRST */
    DBGFEVENT_INSTR_VMX_VMREAD,     /**< Instruction: VT-x VMREAD */
    DBGFEVENT_INSTR_VMX_VMRESUME,   /**< Instruction: VT-x VMRESUME */
    DBGFEVENT_INSTR_VMX_VMWRITE,    /**< Instruction: VT-x VMWRITE */
    DBGFEVENT_INSTR_VMX_VMXOFF,     /**< Instruction: VT-x VMXOFF */
    DBGFEVENT_INSTR_VMX_VMXON,      /**< Instruction: VT-x VMXON */
    DBGFEVENT_INSTR_VMX_VMFUNC,     /**< Instruction: VT-x VMFUNC */
    DBGFEVENT_INSTR_VMX_INVEPT,     /**< Instruction: VT-x INVEPT */
    DBGFEVENT_INSTR_VMX_INVVPID,    /**< Instruction: VT-x INVVPID */
    DBGFEVENT_INSTR_VMX_INVPCID,    /**< Instruction: VT-x INVPCID */
    DBGFEVENT_INSTR_VMX_LAST        /**< Instruction: VT-x - Last. */
        = DBGFEVENT_INSTR_VMX_INVPCID,
    DBGFEVENT_INSTR_SVM_FIRST,      /**< Instruction: AMD-V - first */
    DBGFEVENT_INSTR_SVM_VMRUN       /**< Instruction: AMD-V VMRUN */
    = DBGFEVENT_INSTR_SVM_FIRST,
    DBGFEVENT_INSTR_SVM_VMLOAD,     /**< Instruction: AMD-V VMLOAD */
    DBGFEVENT_INSTR_SVM_VMSAVE,     /**< Instruction: AMD-V VMSAVE */
    DBGFEVENT_INSTR_SVM_STGI,       /**< Instruction: AMD-V STGI */
    DBGFEVENT_INSTR_SVM_CLGI,       /**< Instruction: AMD-V CLGI */
    DBGFEVENT_INSTR_SVM_LAST        /**< Instruction: The last ADM-V VM exit event. */
        = DBGFEVENT_INSTR_SVM_CLGI,
    DBGFEVENT_INSTR_LAST            /**< Instruction: The last instruction event.   */
        = DBGFEVENT_INSTR_SVM_LAST,
    /** @} */


    /** @name VM exit events.
     * VM exits events for VT-x and AMD-V execution mode.  Many of the VM exits
     * behind these events are also directly translated into instruction events, but
     * the difference here is that the exit events will not try provoke the exits.
     * @{ */
    DBGFEVENT_EXIT_FIRST,               /**< The first VM exit event. */
    DBGFEVENT_EXIT_TASK_SWITCH          /**< Exit: Task switch. */
        = DBGFEVENT_EXIT_FIRST,
    DBGFEVENT_EXIT_HALT,                /**< Exit: HALT instruction. */
    DBGFEVENT_EXIT_MWAIT,               /**< Exit: MWAIT instruction. */
    DBGFEVENT_EXIT_MONITOR,             /**< Exit: MONITOR instruction. */
    DBGFEVENT_EXIT_CPUID,               /**< Exit: CPUID instruction (missing stuff in raw-mode). */
    DBGFEVENT_EXIT_INVD,                /**< Exit: INVD instruction. */
    DBGFEVENT_EXIT_WBINVD,              /**< Exit: WBINVD instruction. */
    DBGFEVENT_EXIT_INVLPG,              /**< Exit: INVLPG instruction. */
    DBGFEVENT_EXIT_RDTSC,               /**< Exit: RDTSC instruction. */
    DBGFEVENT_EXIT_RDTSCP,              /**< Exit: RDTSCP instruction. */
    DBGFEVENT_EXIT_RDPMC,               /**< Exit: RDPMC instruction. */
    DBGFEVENT_EXIT_RDMSR,               /**< Exit: RDMSR instruction. */
    DBGFEVENT_EXIT_WRMSR,               /**< Exit: WRMSR instruction. */
    DBGFEVENT_EXIT_CRX_READ,            /**< Exit: CRx read instruction (missing smsw in raw-mode, and reads in  general in VT-x). */
    DBGFEVENT_EXIT_CRX_WRITE,           /**< Exit: CRx write instruction. */
    DBGFEVENT_EXIT_DRX_READ,            /**< Exit: DRx read instruction. */
    DBGFEVENT_EXIT_DRX_WRITE,           /**< Exit: DRx write instruction. */
    DBGFEVENT_EXIT_PAUSE,               /**< Exit: PAUSE instruction (not in raw-mode). */
    DBGFEVENT_EXIT_XSETBV,              /**< Exit: XSETBV instruction. */
    DBGFEVENT_EXIT_SIDT,                /**< Exit: SIDT instruction. */
    DBGFEVENT_EXIT_LIDT,                /**< Exit: LIDT instruction. */
    DBGFEVENT_EXIT_SGDT,                /**< Exit: SGDT instruction. */
    DBGFEVENT_EXIT_LGDT,                /**< Exit: LGDT instruction. */
    DBGFEVENT_EXIT_SLDT,                /**< Exit: SLDT instruction. */
    DBGFEVENT_EXIT_LLDT,                /**< Exit: LLDT instruction. */
    DBGFEVENT_EXIT_STR,                 /**< Exit: STR instruction. */
    DBGFEVENT_EXIT_LTR,                 /**< Exit: LTR instruction. */
    DBGFEVENT_EXIT_GETSEC,              /**< Exit: GETSEC instruction. */
    DBGFEVENT_EXIT_RSM,                 /**< Exit: RSM instruction. */
    DBGFEVENT_EXIT_RDRAND,              /**< Exit: RDRAND instruction. */
    DBGFEVENT_EXIT_RDSEED,              /**< Exit: RDSEED instruction. */
    DBGFEVENT_EXIT_XSAVES,              /**< Exit: XSAVES instruction. */
    DBGFEVENT_EXIT_XRSTORS,             /**< Exit: XRSTORS instruction. */
    DBGFEVENT_EXIT_VMM_CALL,            /**< Exit: VMCALL (intel) or VMMCALL (AMD) instruction. */
    DBGFEVENT_EXIT_LAST_COMMON          /**< Exit: the last common event. */
        = DBGFEVENT_EXIT_VMM_CALL,
    DBGFEVENT_EXIT_VMX_FIRST,           /**< Exit: VT-x - First. */
    DBGFEVENT_EXIT_VMX_VMCLEAR          /**< Exit: VT-x VMCLEAR instruction. */
        = DBGFEVENT_EXIT_VMX_FIRST,
    DBGFEVENT_EXIT_VMX_VMLAUNCH,        /**< Exit: VT-x VMLAUNCH instruction. */
    DBGFEVENT_EXIT_VMX_VMPTRLD,         /**< Exit: VT-x VMPTRLD instruction. */
    DBGFEVENT_EXIT_VMX_VMPTRST,         /**< Exit: VT-x VMPTRST instruction. */
    DBGFEVENT_EXIT_VMX_VMREAD,          /**< Exit: VT-x VMREAD instruction. */
    DBGFEVENT_EXIT_VMX_VMRESUME,        /**< Exit: VT-x VMRESUME instruction. */
    DBGFEVENT_EXIT_VMX_VMWRITE,         /**< Exit: VT-x VMWRITE instruction. */
    DBGFEVENT_EXIT_VMX_VMXOFF,          /**< Exit: VT-x VMXOFF instruction. */
    DBGFEVENT_EXIT_VMX_VMXON,           /**< Exit: VT-x VMXON instruction. */
    DBGFEVENT_EXIT_VMX_VMFUNC,          /**< Exit: VT-x VMFUNC instruction. */
    DBGFEVENT_EXIT_VMX_INVEPT,          /**< Exit: VT-x INVEPT instruction. */
    DBGFEVENT_EXIT_VMX_INVVPID,         /**< Exit: VT-x INVVPID instruction. */
    DBGFEVENT_EXIT_VMX_INVPCID,         /**< Exit: VT-x INVPCID instruction. */
    DBGFEVENT_EXIT_VMX_EPT_VIOLATION,   /**< Exit: VT-x EPT violation. */
    DBGFEVENT_EXIT_VMX_EPT_MISCONFIG,   /**< Exit: VT-x EPT misconfiguration. */
    DBGFEVENT_EXIT_VMX_VAPIC_ACCESS,    /**< Exit: VT-x Virtual APIC page access. */
    DBGFEVENT_EXIT_VMX_VAPIC_WRITE,     /**< Exit: VT-x Virtual APIC write. */
    DBGFEVENT_EXIT_VMX_LAST             /**< Exit: VT-x - Last. */
        = DBGFEVENT_EXIT_VMX_VAPIC_WRITE,
    DBGFEVENT_EXIT_SVM_FIRST,           /**< Exit: AMD-V - first */
    DBGFEVENT_EXIT_SVM_VMRUN            /**< Exit: AMD-V VMRUN instruction. */
        = DBGFEVENT_EXIT_SVM_FIRST,
    DBGFEVENT_EXIT_SVM_VMLOAD,          /**< Exit: AMD-V VMLOAD instruction. */
    DBGFEVENT_EXIT_SVM_VMSAVE,          /**< Exit: AMD-V VMSAVE instruction. */
    DBGFEVENT_EXIT_SVM_STGI,            /**< Exit: AMD-V STGI instruction. */
    DBGFEVENT_EXIT_SVM_CLGI,            /**< Exit: AMD-V CLGI instruction. */
    DBGFEVENT_EXIT_SVM_LAST             /**< Exit: The last ADM-V VM exit event. */
        = DBGFEVENT_EXIT_SVM_CLGI,
    DBGFEVENT_EXIT_LAST                 /**< Exit: The last VM exit event. */
        = DBGFEVENT_EXIT_SVM_LAST,
    /** @} */


    /** Access to an unassigned I/O port.
     * @todo not yet implemented. */
    DBGFEVENT_IOPORT_UNASSIGNED,
    /** Access to an unused I/O port on a device.
     * @todo not yet implemented.  */
    DBGFEVENT_IOPORT_UNUSED,
    /** Unassigned memory event.
     * @todo not yet implemented.  */
    DBGFEVENT_MEMORY_UNASSIGNED,
    /** Attempt to write to unshadowed ROM.
     * @todo not yet implemented.  */
    DBGFEVENT_MEMORY_ROM_WRITE,

    /** Windows guest reported BSOD via hyperv MSRs. */
    DBGFEVENT_BSOD_MSR,
    /** Windows guest reported BSOD via EFI variables. */
    DBGFEVENT_BSOD_EFI,

    /** End of valid event values. */
    DBGFEVENT_END,
    /** The usual 32-bit hack. */
    DBGFEVENT_32BIT_HACK = 0x7fffffff
} DBGFEVENTTYPE;
AssertCompile(DBGFEVENT_XCPT_LAST - DBGFEVENT_XCPT_FIRST == 0x1f);

/**
 * The context of an event.
 */
typedef enum DBGFEVENTCTX
{
    /** The usual invalid entry. */
    DBGFEVENTCTX_INVALID = 0,
    /** Raw mode. */
    DBGFEVENTCTX_RAW,
    /** Recompiled mode. */
    DBGFEVENTCTX_REM,
    /** VMX / AVT mode. */
    DBGFEVENTCTX_HM,
    /** Hypervisor context. */
    DBGFEVENTCTX_HYPER,
    /** Other mode */
    DBGFEVENTCTX_OTHER,

    /** The usual 32-bit hack */
    DBGFEVENTCTX_32BIT_HACK = 0x7fffffff
} DBGFEVENTCTX;

/**
 * VMM Debug Event.
 */
typedef struct DBGFEVENT
{
    /** Type. */
    DBGFEVENTTYPE   enmType;
    /** Context */
    DBGFEVENTCTX    enmCtx;
    /** Type specific data. */
    union
    {
        /** Fatal error details. */
        struct
        {
            /** The GC return code. */
            int                     rc;
        } FatalError;

        /** Source location. */
        struct
        {
            /** File name. */
            R3PTRTYPE(const char *) pszFile;
            /** Function name. */
            R3PTRTYPE(const char *) pszFunction;
            /** Message. */
            R3PTRTYPE(const char *) pszMessage;
            /** Line number. */
            unsigned                uLine;
        } Src;

        /** Assertion messages. */
        struct
        {
            /** The first message. */
            R3PTRTYPE(const char *) pszMsg1;
            /** The second message. */
            R3PTRTYPE(const char *) pszMsg2;
        } Assert;

        /** Breakpoint. */
        struct DBGFEVENTBP
        {
            /** The identifier of the breakpoint which was hit. */
            RTUINT                  iBp;
        } Bp;

        /** Generic debug event. */
        struct DBGFEVENTGENERIC
        {
            /** Argument. */
            uint64_t                uArg;
        } Generic;

        /** Padding for ensuring that the structure is 8 byte aligned. */
        uint64_t        au64Padding[4];
    } u;
} DBGFEVENT;
AssertCompileSizeAlignment(DBGFEVENT, 8);
/** Pointer to VMM Debug Event. */
typedef DBGFEVENT *PDBGFEVENT;
/** Pointer to const VMM Debug Event. */
typedef const DBGFEVENT *PCDBGFEVENT;

#ifdef IN_RING3 /* The event API only works in ring-3. */

/** @def DBGFSTOP
 * Stops the debugger raising a DBGFEVENT_DEVELOPER_STOP event.
 *
 * @returns VBox status code which must be propagated up to EM if not VINF_SUCCESS.
 * @param   pVM     The cross context VM structure.
 */
# ifdef VBOX_STRICT
#  define DBGFSTOP(pVM)  DBGFR3EventSrc(pVM, DBGFEVENT_DEV_STOP, __FILE__, __LINE__, __PRETTY_FUNCTION__, NULL)
# else
#  define DBGFSTOP(pVM)  VINF_SUCCESS
# endif

VMMR3_INT_DECL(int)     DBGFR3Init(PVM pVM);
VMMR3_INT_DECL(int)     DBGFR3Term(PVM pVM);
VMMR3_INT_DECL(void)    DBGFR3PowerOff(PVM pVM);
VMMR3_INT_DECL(void)    DBGFR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3_INT_DECL(int)     DBGFR3VMMForcedAction(PVM pVM, PVMCPU pVCpu);
VMMR3_INT_DECL(VBOXSTRICTRC)    DBGFR3EventHandlePending(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(int)          DBGFR3Event(PVM pVM, DBGFEVENTTYPE enmEvent);
VMMR3DECL(int)          DBGFR3EventSrc(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine,
                                       const char *pszFunction, const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR_MAYBE_NULL(6, 7);
VMMR3DECL(int)          DBGFR3EventSrcV(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszFile, unsigned uLine,
                                        const char *pszFunction, const char *pszFormat, va_list args) RT_IPRT_FORMAT_ATTR_MAYBE_NULL(6, 0);
VMMR3_INT_DECL(int)     DBGFR3EventAssertion(PVM pVM, DBGFEVENTTYPE enmEvent, const char *pszMsg1, const char *pszMsg2);
VMMR3_INT_DECL(int)     DBGFR3EventBreakpoint(PVM pVM, DBGFEVENTTYPE enmEvent);
VMMR3_INT_DECL(int)     DBGFR3PrgStep(PVMCPU pVCpu);

VMMR3DECL(int)          DBGFR3Attach(PUVM pUVM);
VMMR3DECL(int)          DBGFR3Detach(PUVM pUVM);
VMMR3DECL(int)          DBGFR3EventWait(PUVM pUVM, RTMSINTERVAL cMillies, PCDBGFEVENT *ppEvent);
VMMR3DECL(int)          DBGFR3Halt(PUVM pUVM);
VMMR3DECL(bool)         DBGFR3IsHalted(PUVM pUVM);
VMMR3DECL(int)          DBGFR3QueryWaitable(PUVM pUVM);
VMMR3DECL(int)          DBGFR3Resume(PUVM pUVM);
VMMR3DECL(int)          DBGFR3InjectNMI(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(int)          DBGFR3Step(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(int)          DBGFR3StepEx(PUVM pUVM, VMCPUID idCpu, uint32_t fFlags, PCDBGFADDRESS pStopPcAddr,
                                     PCDBGFADDRESS pStopPopAddr, RTGCUINTPTR cbStopPop, uint32_t cMaxSteps);

/** @name DBGF_STEP_F_XXX - Flags for DBGFR3StepEx.
 *
 * @note The stop filters are not applied to the starting instruction.
 *
 * @{ */
/** Step into CALL, INT, SYSCALL and SYSENTER instructions. */
#define DBGF_STEP_F_INTO                RT_BIT_32(0)
/** Step over CALL, INT, SYSCALL and SYSENTER instruction when considering
 *  what's "next". */
#define DBGF_STEP_F_OVER                RT_BIT_32(1)

/** Stop on the next CALL, INT, SYSCALL, SYSENTER instruction. */
#define DBGF_STEP_F_STOP_ON_CALL        RT_BIT_32(8)
/** Stop on the next RET, IRET, SYSRET, SYSEXIT instruction. */
#define DBGF_STEP_F_STOP_ON_RET         RT_BIT_32(9)
/** Stop after the next RET, IRET, SYSRET, SYSEXIT instruction. */
#define DBGF_STEP_F_STOP_AFTER_RET      RT_BIT_32(10)
/** Stop on the given address.
 * The comparison will be made using effective (flat) addresses.  */
#define DBGF_STEP_F_STOP_ON_ADDRESS     RT_BIT_32(11)
/** Stop when the stack pointer pops to or past the given address.
 * The comparison will be made using effective (flat) addresses.  */
#define DBGF_STEP_F_STOP_ON_STACK_POP   RT_BIT_32(12)
/** Mask of stop filter flags. */
#define DBGF_STEP_F_STOP_FILTER_MASK    UINT32_C(0x00001f00)

/** Mask of valid flags. */
#define DBGF_STEP_F_VALID_MASK          UINT32_C(0x00001f03)
/** @} */

/**
 * Event configuration array element, see DBGFR3EventConfigEx.
 */
typedef struct DBGFEVENTCONFIG
{
    /** The event to configure */
    DBGFEVENTTYPE   enmType;
    /** The new state. */
    bool            fEnabled;
    /** Unused. */
    uint8_t         abUnused[3];
} DBGFEVENTCONFIG;
/** Pointer to an event config. */
typedef DBGFEVENTCONFIG *PDBGFEVENTCONFIG;
/** Pointer to a const event config. */
typedef const DBGFEVENTCONFIG *PCDBGFEVENTCONFIG;

VMMR3DECL(int)          DBGFR3EventConfigEx(PUVM pUVM, PCDBGFEVENTCONFIG paConfigs, size_t cConfigs);
VMMR3DECL(int)          DBGFR3EventConfig(PUVM pUVM, DBGFEVENTTYPE enmEvent, bool fEnabled);
VMMR3DECL(bool)         DBGFR3EventIsEnabled(PUVM pUVM, DBGFEVENTTYPE enmEvent);
VMMR3DECL(int)          DBGFR3EventQuery(PUVM pUVM, PDBGFEVENTCONFIG paConfigs, size_t cConfigs);

/** @name DBGFINTERRUPTSTATE_XXX - interrupt break state.
 * @{ */
#define DBGFINTERRUPTSTATE_DISABLED     0
#define DBGFINTERRUPTSTATE_ENABLED      1
#define DBGFINTERRUPTSTATE_DONT_TOUCH   2
/** @} */

/**
 * Interrupt break state configuration entry.
 */
typedef struct DBGFINTERRUPTCONFIG
{
    /** The interrupt number. */
    uint8_t     iInterrupt;
    /** The hardware interrupt state (DBGFINTERRUPTSTATE_XXX). */
    uint8_t     enmHardState;
    /** The software interrupt state (DBGFINTERRUPTSTATE_XXX). */
    uint8_t     enmSoftState;
} DBGFINTERRUPTCONFIG;
/** Pointer to an interrupt break state config entyr. */
typedef DBGFINTERRUPTCONFIG *PDBGFINTERRUPTCONFIG;
/** Pointer to a const interrupt break state config entyr. */
typedef DBGFINTERRUPTCONFIG const *PCDBGFINTERRUPTCONFIG;

VMMR3DECL(int) DBGFR3InterruptConfigEx(PUVM pUVM, PCDBGFINTERRUPTCONFIG paConfigs, size_t cConfigs);
VMMR3DECL(int) DBGFR3InterruptHardwareConfig(PUVM pUVM, uint8_t iInterrupt, bool fEnabled);
VMMR3DECL(int) DBGFR3InterruptSoftwareConfig(PUVM pUVM, uint8_t iInterrupt, bool fEnabled);
VMMR3DECL(int) DBGFR3InterruptHardwareIsEnabled(PUVM pUVM, uint8_t iInterrupt);
VMMR3DECL(int) DBGFR3InterruptSoftwareIsEnabled(PUVM pUVM, uint8_t iInterrupt);

#endif /* IN_RING3 */

/** @def DBGF_IS_EVENT_ENABLED
 * Checks if a selectable debug event is enabled or not (fast).
 *
 * @returns true/false.
 * @param   a_pVM       Pointer to the cross context VM structure.
 * @param   a_enmEvent  The selectable event to check.
 * @remarks Only for use internally in the VMM. Use DBGFR3EventIsEnabled elsewhere.
 */
#if defined(VBOX_STRICT) && defined(RT_COMPILER_SUPPORTS_LAMBDA)
# define DBGF_IS_EVENT_ENABLED(a_pVM, a_enmEvent) \
    ([](PVM a_pLambdaVM, DBGFEVENTTYPE a_enmLambdaEvent) -> bool { \
        Assert(   a_enmLambdaEvent >= DBGFEVENT_FIRST_SELECTABLE \
               || a_enmLambdaEvent == DBGFEVENT_INTERRUPT_HARDWARE \
               || a_enmLambdaEvent == DBGFEVENT_INTERRUPT_SOFTWARE); \
        Assert(a_enmLambdaEvent < DBGFEVENT_END); \
        return ASMBitTest(&a_pLambdaVM->dbgf.ro.bmSelectedEvents, a_enmLambdaEvent); \
    }(a_pVM, a_enmEvent))
#elif defined(VBOX_STRICT) && defined(__GNUC__)
# define DBGF_IS_EVENT_ENABLED(a_pVM, a_enmEvent) \
    __extension__ ({ \
        Assert(   (a_enmEvent) >= DBGFEVENT_FIRST_SELECTABLE \
               || (a_enmEvent) == DBGFEVENT_INTERRUPT_HARDWARE \
               || (a_enmEvent) == DBGFEVENT_INTERRUPT_SOFTWARE); \
        Assert((a_enmEvent) < DBGFEVENT_END); \
        ASMBitTest(&(a_pVM)->dbgf.ro.bmSelectedEvents, (a_enmEvent)); \
    })
#else
# define DBGF_IS_EVENT_ENABLED(a_pVM, a_enmEvent) \
        ASMBitTest(&(a_pVM)->dbgf.ro.bmSelectedEvents, (a_enmEvent))
#endif


/** @def DBGF_IS_HARDWARE_INT_ENABLED
 * Checks if hardware interrupt interception is enabled or not for an interrupt.
 *
 * @returns true/false.
 * @param   a_pVM           Pointer to the cross context VM structure.
 * @param   a_iInterrupt    Interrupt to check.
 * @remarks Only for use internally in the VMM.  Use
 *          DBGFR3InterruptHardwareIsEnabled elsewhere.
 */
#define DBGF_IS_HARDWARE_INT_ENABLED(a_pVM, a_iInterrupt) \
        ASMBitTest(&(a_pVM)->dbgf.ro.bmHardIntBreakpoints, (uint8_t)(a_iInterrupt))

/** @def DBGF_IS_SOFTWARE_INT_ENABLED
 * Checks if software interrupt interception is enabled or not for an interrupt.
 *
 * @returns true/false.
 * @param   a_pVM           Pointer to the cross context VM structure.
 * @param   a_iInterrupt    Interrupt to check.
 * @remarks Only for use internally in the VMM.  Use
 *          DBGFR3InterruptSoftwareIsEnabled elsewhere.
 */
#define DBGF_IS_SOFTWARE_INT_ENABLED(a_pVM, a_iInterrupt) \
        ASMBitTest(&(a_pVM)->dbgf.ro.bmSoftIntBreakpoints, (uint8_t)(a_iInterrupt))



/** Breakpoint type. */
typedef enum DBGFBPTYPE
{
    /** Free breakpoint entry. */
    DBGFBPTYPE_FREE = 0,
    /** Debug register. */
    DBGFBPTYPE_REG,
    /** INT 3 instruction. */
    DBGFBPTYPE_INT3,
    /** Recompiler. */
    DBGFBPTYPE_REM,
    /** Port I/O breakpoint. */
    DBGFBPTYPE_PORT_IO,
    /** Memory mapped I/O breakpoint. */
    DBGFBPTYPE_MMIO,
    /** ensure 32-bit size. */
    DBGFBPTYPE_32BIT_HACK = 0x7fffffff
} DBGFBPTYPE;


/** @name DBGFBPIOACCESS_XXX - I/O (port + mmio) access types.
 * @{ */
/** Byte sized read accesses. */
#define DBGFBPIOACCESS_READ_BYTE            UINT32_C(0x00000001)
/** Word sized accesses. */
#define DBGFBPIOACCESS_READ_WORD            UINT32_C(0x00000002)
/** Double word sized accesses. */
#define DBGFBPIOACCESS_READ_DWORD           UINT32_C(0x00000004)
/** Quad word sized accesses - not available for I/O ports. */
#define DBGFBPIOACCESS_READ_QWORD           UINT32_C(0x00000008)
/** Other sized accesses - not available for I/O ports. */
#define DBGFBPIOACCESS_READ_OTHER           UINT32_C(0x00000010)
/** Read mask. */
#define DBGFBPIOACCESS_READ_MASK            UINT32_C(0x0000001f)

/** Byte sized write accesses. */
#define DBGFBPIOACCESS_WRITE_BYTE           UINT32_C(0x00000100)
/** Word sized write accesses. */
#define DBGFBPIOACCESS_WRITE_WORD           UINT32_C(0x00000200)
/** Double word sized write accesses. */
#define DBGFBPIOACCESS_WRITE_DWORD          UINT32_C(0x00000400)
/** Quad word sized write accesses - not available for I/O ports. */
#define DBGFBPIOACCESS_WRITE_QWORD          UINT32_C(0x00000800)
/** Other sized write accesses - not available for I/O ports. */
#define DBGFBPIOACCESS_WRITE_OTHER          UINT32_C(0x00001000)
/** Write mask. */
#define DBGFBPIOACCESS_WRITE_MASK           UINT32_C(0x00001f00)

/** All kind of access (read, write, all sizes). */
#define DBGFBPIOACCESS_ALL                  UINT32_C(0x00001f1f)

/** The acceptable mask for I/O ports.   */
#define DBGFBPIOACCESS_VALID_MASK_PORT_IO   UINT32_C(0x00000303)
/** The acceptable mask for MMIO.   */
#define DBGFBPIOACCESS_VALID_MASK_MMIO      UINT32_C(0x00001f1f)
/** @} */

/**
 * A Breakpoint.
 */
typedef struct DBGFBP
{
    /** The number of breakpoint hits. */
    uint64_t        cHits;
    /** The hit number which starts to trigger the breakpoint. */
    uint64_t        iHitTrigger;
    /** The hit number which stops triggering the breakpoint (disables it).
     * Use ~(uint64_t)0 if it should never stop. */
    uint64_t        iHitDisable;
    /** The breakpoint id. */
    uint16_t        iBp;
    /** The breakpoint status - enabled or disabled. */
    bool            fEnabled;
    /** The breakpoint type. */
    DBGFBPTYPE      enmType;

    /** Union of type specific data. */
    union
    {
        /** The flat GC address breakpoint address for REG, INT3 and REM breakpoints. */
        RTGCUINTPTR         GCPtr;

        /** Debug register data. */
        struct DBGFBPREG
        {
            /** The flat GC address of the breakpoint. */
            RTGCUINTPTR     GCPtr;
            /** The debug register number. */
            uint8_t         iReg;
            /** The access type (one of the X86_DR7_RW_* value). */
            uint8_t         fType;
            /** The access size. */
            uint8_t         cb;
        } Reg;

        /** INT3 breakpoint data. */
        struct DBGFBPINT3
        {
            /** The flat GC address of the breakpoint. */
            RTGCUINTPTR     GCPtr;
            /** The physical address of the breakpoint. */
            RTGCPHYS        PhysAddr;
            /** The byte value we replaced by the INT 3 instruction. */
            uint8_t         bOrg;
        } Int3;

        /** Recompiler breakpoint data. */
        struct DBGFBPREM
        {
            /** The flat GC address of the breakpoint.
             * (PC register value?) */
            RTGCUINTPTR     GCPtr;
        } Rem;

        /** I/O port breakpoint data.   */
        struct DBGFBPPORTIO
        {
            /** The first port. */
            RTIOPORT        uPort;
            /** The number of ports. */
            RTIOPORT        cPorts;
            /** Valid DBGFBPIOACCESS_XXX selection, max DWORD size. */
            uint32_t        fAccess;
        } PortIo;

        /** Memory mapped I/O breakpoint data. */
        struct DBGFBPMMIO
        {
            /** The first MMIO address. */
            RTGCPHYS        PhysAddr;
            /** The size of the MMIO range in bytes. */
            uint32_t        cb;
            /** Valid DBGFBPIOACCESS_XXX selection, max DWORD size. */
            uint32_t        fAccess;
        } Mmio;

        /** Paddind to ensure that the size is identical on win32 and linux. */
        uint64_t    u64Padding[3];
    } u;
} DBGFBP;
AssertCompileMembersAtSameOffset(DBGFBP, u.GCPtr, DBGFBP, u.Reg.GCPtr);
AssertCompileMembersAtSameOffset(DBGFBP, u.GCPtr, DBGFBP, u.Int3.GCPtr);
AssertCompileMembersAtSameOffset(DBGFBP, u.GCPtr, DBGFBP, u.Rem.GCPtr);

/** Pointer to a breakpoint. */
typedef DBGFBP *PDBGFBP;
/** Pointer to a const breakpoint. */
typedef const DBGFBP *PCDBGFBP;

#ifdef IN_RING3 /* The breakpoint management API is only available in ring-3. */
VMMR3DECL(int)  DBGFR3BpSetInt3(PUVM pUVM, VMCPUID idSrcCpu, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, uint32_t *piBp);
VMMR3DECL(int)  DBGFR3BpSetReg(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable,
                               uint8_t fType, uint8_t cb, uint32_t *piBp);
VMMR3DECL(int)  DBGFR3BpSetREM(PUVM pUVM, PCDBGFADDRESS pAddress, uint64_t iHitTrigger, uint64_t iHitDisable, uint32_t *piBp);
VMMR3DECL(int)  DBGFR3BpSetPortIo(PUVM pUVM, RTIOPORT uPort, RTIOPORT cPorts, uint32_t fAccess,
                                  uint64_t iHitTrigger, uint64_t iHitDisable, uint32_t *piBp);
VMMR3DECL(int)  DBGFR3BpSetMmio(PUVM pUVM, RTGCPHYS GCPhys, uint32_t cb, uint32_t fAccess,
                                uint64_t iHitTrigger, uint64_t iHitDisable, uint32_t *piBp);
VMMR3DECL(int)  DBGFR3BpClear(PUVM pUVM, uint32_t iBp);
VMMR3DECL(int)  DBGFR3BpEnable(PUVM pUVM, uint32_t iBp);
VMMR3DECL(int)  DBGFR3BpDisable(PUVM pUVM, uint32_t iBp);

/**
 * Breakpoint enumeration callback function.
 *
 * @returns VBox status code.
 *          The enumeration stops on failure status and VINF_CALLBACK_RETURN.
 * @param   pUVM        The user mode VM handle.
 * @param   pvUser      The user argument.
 * @param   pBp         Pointer to the breakpoint information. (readonly)
 */
typedef DECLCALLBACK(int) FNDBGFBPENUM(PUVM pUVM, void *pvUser, PCDBGFBP pBp);
/** Pointer to a breakpoint enumeration callback function. */
typedef FNDBGFBPENUM *PFNDBGFBPENUM;

VMMR3DECL(int)              DBGFR3BpEnum(PUVM pUVM, PFNDBGFBPENUM pfnCallback, void *pvUser);
#endif /* IN_RING3 */

VMM_INT_DECL(RTGCUINTREG)   DBGFBpGetDR7(PVM pVM);
VMM_INT_DECL(RTGCUINTREG)   DBGFBpGetDR0(PVM pVM);
VMM_INT_DECL(RTGCUINTREG)   DBGFBpGetDR1(PVM pVM);
VMM_INT_DECL(RTGCUINTREG)   DBGFBpGetDR2(PVM pVM);
VMM_INT_DECL(RTGCUINTREG)   DBGFBpGetDR3(PVM pVM);
VMM_INT_DECL(bool)          DBGFBpIsHwArmed(PVM pVM);
VMM_INT_DECL(bool)          DBGFBpIsHwIoArmed(PVM pVM);
VMM_INT_DECL(bool)          DBGFBpIsInt3Armed(PVM pVM);
VMM_INT_DECL(bool)          DBGFIsStepping(PVMCPU pVCpu);
VMM_INT_DECL(VBOXSTRICTRC)  DBGFBpCheckIo(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTIOPORT uIoPort, uint8_t cbValue);
VMM_INT_DECL(VBOXSTRICTRC)  DBGFEventGenericWithArg(PVM pVM, PVMCPU pVCpu, DBGFEVENTTYPE enmEvent, uint64_t uEventArg,
                                                    DBGFEVENTCTX enmCtx);


#ifdef IN_RING3 /* The CPU mode API only works in ring-3. */
VMMR3DECL(CPUMMODE)         DBGFR3CpuGetMode(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(VMCPUID)          DBGFR3CpuGetCount(PUVM pUVM);
VMMR3DECL(bool)             DBGFR3CpuIsIn64BitCode(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(bool)             DBGFR3CpuIsInV86Code(PUVM pUVM, VMCPUID idCpu);
#endif



#ifdef IN_RING3 /* The info callbacks API only works in ring-3. */

/**
 * Info helper callback structure.
 */
typedef struct DBGFINFOHLP
{
    /**
     * Print formatted string.
     *
     * @param   pHlp        Pointer to this structure.
     * @param   pszFormat   The format string.
     * @param   ...         Arguments.
     */
    DECLCALLBACKMEMBER(void, pfnPrintf)(PCDBGFINFOHLP pHlp, const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(2, 3);

    /**
     * Print formatted string.
     *
     * @param   pHlp        Pointer to this structure.
     * @param   pszFormat   The format string.
     * @param   args        Argument list.
     */
    DECLCALLBACKMEMBER(void, pfnPrintfV)(PCDBGFINFOHLP pHlp, const char *pszFormat, va_list args) RT_IPRT_FORMAT_ATTR(2, 0);
} DBGFINFOHLP;


/**
 * Info handler, device version.
 *
 * @param   pDevIns     The device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERDEV(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERDEV function. */
typedef FNDBGFHANDLERDEV  *PFNDBGFHANDLERDEV;

/**
 * Info handler, USB device version.
 *
 * @param   pUsbIns     The USB device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERUSB(PPDMUSBINS pUsbIns, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERUSB function. */
typedef FNDBGFHANDLERUSB  *PFNDBGFHANDLERUSB;

/**
 * Info handler, driver version.
 *
 * @param   pDrvIns     The driver instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERDRV(PPDMDRVINS pDrvIns, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERDRV function. */
typedef FNDBGFHANDLERDRV  *PFNDBGFHANDLERDRV;

/**
 * Info handler, internal version.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLERINT(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLERINT function. */
typedef FNDBGFHANDLERINT  *PFNDBGFHANDLERINT;

/**
 * Info handler, external version.
 *
 * @param   pvUser      User argument.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
typedef DECLCALLBACK(void) FNDBGFHANDLEREXT(void *pvUser, PCDBGFINFOHLP pHlp, const char *pszArgs);
/** Pointer to a FNDBGFHANDLEREXT function. */
typedef FNDBGFHANDLEREXT  *PFNDBGFHANDLEREXT;


/** @name Flags for the info registration functions.
 * @{ */
/** The handler must run on the EMT. */
#define DBGFINFO_FLAGS_RUN_ON_EMT       RT_BIT(0)
/** Call on all EMTs when a specific isn't specified. */
#define DBGFINFO_FLAGS_ALL_EMTS         RT_BIT(1)
/** @} */

VMMR3_INT_DECL(int) DBGFR3InfoRegisterDevice(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDEV pfnHandler, PPDMDEVINS pDevIns);
VMMR3_INT_DECL(int) DBGFR3InfoRegisterDriver(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERDRV pfnHandler, PPDMDRVINS pDrvIns);
VMMR3_INT_DECL(int) DBGFR3InfoRegisterInternal(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler);
VMMR3_INT_DECL(int) DBGFR3InfoRegisterInternalEx(PVM pVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLERINT pfnHandler, uint32_t fFlags);
VMMR3DECL(int)      DBGFR3InfoRegisterExternal(PUVM pUVM, const char *pszName, const char *pszDesc, PFNDBGFHANDLEREXT pfnHandler, void *pvUser);
VMMR3_INT_DECL(int) DBGFR3InfoDeregisterDevice(PVM pVM, PPDMDEVINS pDevIns, const char *pszName);
VMMR3_INT_DECL(int) DBGFR3InfoDeregisterDriver(PVM pVM, PPDMDRVINS pDrvIns, const char *pszName);
VMMR3_INT_DECL(int) DBGFR3InfoDeregisterInternal(PVM pVM, const char *pszName);
VMMR3DECL(int)      DBGFR3InfoDeregisterExternal(PUVM pUVM, const char *pszName);
VMMR3DECL(int)      DBGFR3Info(PUVM pUVM, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp);
VMMR3DECL(int)      DBGFR3InfoEx(PUVM pUVM, VMCPUID idCpu, const char *pszName, const char *pszArgs, PCDBGFINFOHLP pHlp);
VMMR3DECL(int)      DBGFR3InfoLogRel(PUVM pUVM, const char *pszName, const char *pszArgs);
VMMR3DECL(int)      DBGFR3InfoStdErr(PUVM pUVM, const char *pszName, const char *pszArgs);
VMMR3_INT_DECL(int) DBGFR3InfoMulti(PVM pVM, const char *pszIncludePat, const char *pszExcludePat,
                                    const char *pszSepFmt, PCDBGFINFOHLP pHlp);

/** @def DBGFR3_INFO_LOG
 * Display a piece of info writing to the log if enabled.
 *
 * This is for execution on EMTs and will only show the items on the calling
 * EMT.  This is to avoid deadlocking against other CPUs if a rendezvous is
 * initiated in parallel to this call.  (Besides, nobody really wants or need
 * info for the other EMTs when using this macro.)
 *
 * @param   a_pVM       The shared VM handle.
 * @param   a_pVCpu     The cross context per CPU structure of the calling EMT.
 * @param   a_pszName   The identifier of the info to display.
 * @param   a_pszArgs   Arguments to the info handler.
 */
#ifdef LOG_ENABLED
# define DBGFR3_INFO_LOG(a_pVM, a_pVCpu, a_pszName, a_pszArgs) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3InfoEx((a_pVM)->pUVM, (a_pVCpu)->idCpu, a_pszName, a_pszArgs, NULL); \
    } while (0)
#else
# define DBGFR3_INFO_LOG(a_pVM, a_pVCpu, a_pszName, a_pszArgs) do { } while (0)
#endif

/** @def DBGFR3_INFO_LOG_SAFE
 * Display a piece of info (rendezvous safe) writing to the log if enabled.
 *
 * @param   a_pVM       The shared VM handle.
 * @param   a_pszName   The identifier of the info to display.
 * @param   a_pszArgs   Arguments to the info handler.
 *
 * @remarks Use DBGFR3_INFO_LOG where ever possible!
 */
#ifdef LOG_ENABLED
# define DBGFR3_INFO_LOG_SAFE(a_pVM, a_pszName, a_pszArgs) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3Info((a_pVM)->pUVM, a_pszName, a_pszArgs, NULL); \
    } while (0)
#else
# define DBGFR3_INFO_LOG_SAFE(a_pVM, a_pszName, a_pszArgs) do { } while (0)
#endif

/**
 * Enumeration callback for use with DBGFR3InfoEnum.
 *
 * @returns VBox status code.
 *          A status code indicating failure will end the enumeration
 *          and DBGFR3InfoEnum will return with that status code.
 * @param   pUVM        The user mode VM handle.
 * @param   pszName     Info identifier name.
 * @param   pszDesc     The description.
 */
typedef DECLCALLBACK(int) FNDBGFINFOENUM(PUVM pUVM, const char *pszName, const char *pszDesc, void *pvUser);
/** Pointer to a FNDBGFINFOENUM function. */
typedef FNDBGFINFOENUM *PFNDBGFINFOENUM;

VMMR3DECL(int)              DBGFR3InfoEnum(PUVM pUVM, PFNDBGFINFOENUM pfnCallback, void *pvUser);
VMMR3DECL(PCDBGFINFOHLP)    DBGFR3InfoLogHlp(void);
VMMR3DECL(PCDBGFINFOHLP)    DBGFR3InfoLogRelHlp(void);

#endif /* IN_RING3 */


#ifdef IN_RING3 /* The log contrl API only works in ring-3. */
VMMR3DECL(int) DBGFR3LogModifyGroups(PUVM pUVM, const char *pszGroupSettings);
VMMR3DECL(int) DBGFR3LogModifyFlags(PUVM pUVM, const char *pszFlagSettings);
VMMR3DECL(int) DBGFR3LogModifyDestinations(PUVM pUVM, const char *pszDestSettings);
#endif /* IN_RING3 */

#ifdef IN_RING3 /* The debug information management APIs only works in ring-3. */

/** Max length (including '\\0') of a symbol name. */
#define DBGF_SYMBOL_NAME_LENGTH   512

/**
 * Debug symbol.
 */
typedef struct DBGFSYMBOL
{
    /** Symbol value (address). */
    RTGCUINTPTR         Value;
    /** Symbol size. */
    uint32_t            cb;
    /** Symbol Flags. (reserved). */
    uint32_t            fFlags;
    /** Symbol name. */
    char                szName[DBGF_SYMBOL_NAME_LENGTH];
} DBGFSYMBOL;
/** Pointer to debug symbol. */
typedef DBGFSYMBOL *PDBGFSYMBOL;
/** Pointer to const debug symbol. */
typedef const DBGFSYMBOL *PCDBGFSYMBOL;

/**
 * Debug line number information.
 */
typedef struct DBGFLINE
{
    /** Address. */
    RTGCUINTPTR         Address;
    /** Line number. */
    uint32_t            uLineNo;
    /** Filename. */
    char                szFilename[260];
} DBGFLINE;
/** Pointer to debug line number. */
typedef DBGFLINE *PDBGFLINE;
/** Pointer to const debug line number. */
typedef const DBGFLINE *PCDBGFLINE;

/** @name Address spaces aliases.
 * @{ */
/** The guest global address space. */
#define DBGF_AS_GLOBAL              ((RTDBGAS)-1)
/** The guest kernel address space.
 * This is usually resolves to the same as DBGF_AS_GLOBAL. */
#define DBGF_AS_KERNEL              ((RTDBGAS)-2)
/** The physical address space. */
#define DBGF_AS_PHYS                ((RTDBGAS)-3)
/** Raw-mode context. */
#define DBGF_AS_RC                  ((RTDBGAS)-4)
/** Ring-0 context. */
#define DBGF_AS_R0                  ((RTDBGAS)-5)
/** Raw-mode context and then global guest context.
 * When used for looking up information, it works as if the call was first made
 * with DBGF_AS_RC and then on failure with DBGF_AS_GLOBAL. When called for
 * making address space changes, it works as if DBGF_AS_RC was used. */
#define DBGF_AS_RC_AND_GC_GLOBAL    ((RTDBGAS)-6)

/** The first special one. */
#define DBGF_AS_FIRST               DBGF_AS_RC_AND_GC_GLOBAL
/** The last special one. */
#define DBGF_AS_LAST                DBGF_AS_GLOBAL
#endif
/** The number of special address space handles. */
#define DBGF_AS_COUNT               (6U)
#ifdef IN_RING3
/** Converts an alias handle to an array index. */
#define DBGF_AS_ALIAS_2_INDEX(hAlias) \
    ( (uintptr_t)(hAlias) - (uintptr_t)DBGF_AS_FIRST )
/** Predicat macro that check if the specified handle is an alias. */
#define DBGF_AS_IS_ALIAS(hAlias) \
    ( DBGF_AS_ALIAS_2_INDEX(hAlias)  <  DBGF_AS_COUNT )
/** Predicat macro that check if the specified alias is a fixed one or not. */
#define DBGF_AS_IS_FIXED_ALIAS(hAlias) \
    ( DBGF_AS_ALIAS_2_INDEX(hAlias)  <  (uintptr_t)DBGF_AS_PHYS - (uintptr_t)DBGF_AS_FIRST + 1U )

/** @} */

VMMR3DECL(RTDBGCFG)     DBGFR3AsGetConfig(PUVM pUVM);

VMMR3DECL(int)          DBGFR3AsAdd(PUVM pUVM, RTDBGAS hDbgAs, RTPROCESS ProcId);
VMMR3DECL(int)          DBGFR3AsDelete(PUVM pUVM, RTDBGAS hDbgAs);
VMMR3DECL(int)          DBGFR3AsSetAlias(PUVM pUVM, RTDBGAS hAlias, RTDBGAS hAliasFor);
VMMR3DECL(RTDBGAS)      DBGFR3AsResolve(PUVM pUVM, RTDBGAS hAlias);
VMMR3DECL(RTDBGAS)      DBGFR3AsResolveAndRetain(PUVM pUVM, RTDBGAS hAlias);
VMMR3DECL(RTDBGAS)      DBGFR3AsQueryByName(PUVM pUVM, const char *pszName);
VMMR3DECL(RTDBGAS)      DBGFR3AsQueryByPid(PUVM pUVM, RTPROCESS ProcId);

VMMR3DECL(int)          DBGFR3AsLoadImage(PUVM pUVM, RTDBGAS hDbgAs, const char *pszFilename, const char *pszModName,
                                          RTLDRARCH enmArch, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, uint32_t fFlags);
VMMR3DECL(int)          DBGFR3AsLoadMap(PUVM pUVM, RTDBGAS hDbgAs, const char *pszFilename, const char *pszModName, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, RTGCUINTPTR uSubtrahend, uint32_t fFlags);
VMMR3DECL(int)          DBGFR3AsLinkModule(PUVM pUVM, RTDBGAS hDbgAs, RTDBGMOD hMod, PCDBGFADDRESS pModAddress, RTDBGSEGIDX iModSeg, uint32_t fFlags);
VMMR3DECL(int)          DBGFR3AsUnlinkModuleByName(PUVM pUVM, RTDBGAS hDbgAs, const char *pszModName);

VMMR3DECL(int)          DBGFR3AsSymbolByAddr(PUVM pUVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress, uint32_t fFlags,
                                             PRTGCINTPTR poffDisp, PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod);
VMMR3DECL(PRTDBGSYMBOL) DBGFR3AsSymbolByAddrA(PUVM pUVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress, uint32_t Flags,
                                              PRTGCINTPTR poffDisp, PRTDBGMOD phMod);
VMMR3DECL(int)          DBGFR3AsSymbolByName(PUVM pUVM, RTDBGAS hDbgAs, const char *pszSymbol, PRTDBGSYMBOL pSymbol, PRTDBGMOD phMod);

VMMR3DECL(int)          DBGFR3AsLineByAddr(PUVM pUVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress,
                                           PRTGCINTPTR poffDisp, PRTDBGLINE pLine, PRTDBGMOD phMod);
VMMR3DECL(PRTDBGLINE)   DBGFR3AsLineByAddrA(PUVM pUVM, RTDBGAS hDbgAs, PCDBGFADDRESS pAddress,
                                            PRTGCINTPTR poffDisp, PRTDBGMOD phMod);

#endif /* IN_RING3 */

#ifdef IN_RING3 /* The stack API only works in ring-3. */

/**
 * Return type.
 */
typedef enum DBGFRETRUNTYPE
{
    /** The usual invalid 0 value. */
    DBGFRETURNTYPE_INVALID = 0,
    /** Near 16-bit return. */
    DBGFRETURNTYPE_NEAR16,
    /** Near 32-bit return. */
    DBGFRETURNTYPE_NEAR32,
    /** Near 64-bit return. */
    DBGFRETURNTYPE_NEAR64,
    /** Far 16:16 return. */
    DBGFRETURNTYPE_FAR16,
    /** Far 16:32 return. */
    DBGFRETURNTYPE_FAR32,
    /** Far 16:64 return. */
    DBGFRETURNTYPE_FAR64,
    /** 16-bit iret return (e.g. real or 286 protect mode). */
    DBGFRETURNTYPE_IRET16,
    /** 32-bit iret return. */
    DBGFRETURNTYPE_IRET32,
    /** 32-bit iret return. */
    DBGFRETURNTYPE_IRET32_PRIV,
    /** 32-bit iret return to V86 mode. */
    DBGFRETURNTYPE_IRET32_V86,
    /** @todo 64-bit iret return. */
    DBGFRETURNTYPE_IRET64,
    /** The end of the valid return types. */
    DBGFRETURNTYPE_END,
    /** The usual 32-bit blowup. */
    DBGFRETURNTYPE_32BIT_HACK = 0x7fffffff
} DBGFRETURNTYPE;

/**
 * Figures the size of the return state on the stack.
 *
 * @returns number of bytes. 0 if invalid parameter.
 * @param   enmRetType  The type of return.
 */
DECLINLINE(unsigned) DBGFReturnTypeSize(DBGFRETURNTYPE enmRetType)
{
    switch (enmRetType)
    {
        case DBGFRETURNTYPE_NEAR16:         return 2;
        case DBGFRETURNTYPE_NEAR32:         return 4;
        case DBGFRETURNTYPE_NEAR64:         return 8;
        case DBGFRETURNTYPE_FAR16:          return 4;
        case DBGFRETURNTYPE_FAR32:          return 4;
        case DBGFRETURNTYPE_FAR64:          return 8;
        case DBGFRETURNTYPE_IRET16:         return 6;
        case DBGFRETURNTYPE_IRET32:         return 4*3;
        case DBGFRETURNTYPE_IRET32_PRIV:    return 4*5;
        case DBGFRETURNTYPE_IRET32_V86:     return 4*9;
        case DBGFRETURNTYPE_IRET64:
        default:
            return 0;
    }
}


/** Pointer to stack frame info. */
typedef struct DBGFSTACKFRAME *PDBGFSTACKFRAME;
/** Pointer to const stack frame info. */
typedef struct DBGFSTACKFRAME const  *PCDBGFSTACKFRAME;
/**
 * Info about a stack frame.
 */
typedef struct DBGFSTACKFRAME
{
    /** Frame number. */
    uint32_t        iFrame;
    /** Frame flags. */
    uint32_t        fFlags;
    /** The frame address.
     * The off member is [e|r]bp and the Sel member is ss. */
    DBGFADDRESS     AddrFrame;
    /** The stack address of the frame.
     * The off member is [e|r]sp and the Sel member is ss. */
    DBGFADDRESS     AddrStack;
    /** The program counter (PC) address of the frame.
     * The off member is [e|r]ip and the Sel member is cs. */
    DBGFADDRESS     AddrPC;
    /** Pointer to the symbol nearest the program counter (PC). NULL if not found. */
    PRTDBGSYMBOL    pSymPC;
    /** Pointer to the linnumber nearest the program counter (PC). NULL if not found. */
    PRTDBGLINE      pLinePC;

    /** The return frame address.
     * The off member is [e|r]bp and the Sel member is ss. */
    DBGFADDRESS     AddrReturnFrame;
    /** The return stack address.
     * The off member is [e|r]sp and the Sel member is ss. */
    DBGFADDRESS     AddrReturnStack;
    /** The way this frame returns to the next one. */
    DBGFRETURNTYPE  enmReturnType;

    /** The program counter (PC) address which the frame returns to.
     * The off member is [e|r]ip and the Sel member is cs. */
    DBGFADDRESS     AddrReturnPC;
    /** Pointer to the symbol nearest the return PC. NULL if not found. */
    PRTDBGSYMBOL    pSymReturnPC;
    /** Pointer to the linnumber nearest the return PC. NULL if not found. */
    PRTDBGLINE      pLineReturnPC;

    /** 32-bytes of stack arguments. */
    union
    {
        /** 64-bit view */
        uint64_t    au64[4];
        /** 32-bit view */
        uint32_t    au32[8];
        /** 16-bit view */
        uint16_t    au16[16];
        /** 8-bit view */
        uint8_t     au8[32];
    } Args;

    /** Pointer to the next frame.
     * Might not be used in some cases, so consider it internal. */
    PCDBGFSTACKFRAME pNextInternal;
    /** Pointer to the first frame.
     * Might not be used in some cases, so consider it internal. */
    PCDBGFSTACKFRAME pFirstInternal;
} DBGFSTACKFRAME;

/** @name DBGFSTACKFRAME Flags.
 * @{ */
/** Set if the content of the frame is filled in by DBGFR3StackWalk() and can be used
 * to construct the next frame. */
# define DBGFSTACKFRAME_FLAGS_ALL_VALID     RT_BIT(0)
/** This is the last stack frame we can read.
 * This flag is not set if the walk stop because of max dept or recursion. */
# define DBGFSTACKFRAME_FLAGS_LAST          RT_BIT(1)
/** This is the last record because we detected a loop. */
# define DBGFSTACKFRAME_FLAGS_LOOP          RT_BIT(2)
/** This is the last record because we reached the maximum depth. */
# define DBGFSTACKFRAME_FLAGS_MAX_DEPTH     RT_BIT(3)
/** 16-bit frame. */
# define DBGFSTACKFRAME_FLAGS_16BIT         RT_BIT(4)
/** 32-bit frame. */
# define DBGFSTACKFRAME_FLAGS_32BIT         RT_BIT(5)
/** 64-bit frame. */
# define DBGFSTACKFRAME_FLAGS_64BIT         RT_BIT(6)
/** Used Odd/even heuristics for far/near return. */
# define DBGFSTACKFRAME_FLAGS_USED_ODD_EVEN RT_BIT(7)
/** @} */

/** @name DBGFCODETYPE
 * @{ */
typedef enum DBGFCODETYPE
{
    /** The usual invalid 0 value. */
    DBGFCODETYPE_INVALID = 0,
    /** Stack walk for guest code. */
    DBGFCODETYPE_GUEST,
    /** Stack walk for hypervisor code. */
    DBGFCODETYPE_HYPER,
    /** Stack walk for ring 0 code. */
    DBGFCODETYPE_RING0,
    /** The usual 32-bit blowup. */
    DBGFCODETYPE_32BIT_HACK = 0x7fffffff
} DBGFCODETYPE;
/** @} */

VMMR3DECL(int)              DBGFR3StackWalkBegin(PUVM pUVM, VMCPUID idCpu, DBGFCODETYPE enmCodeType,
                                                 PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(int)              DBGFR3StackWalkBeginEx(PUVM pUVM, VMCPUID idCpu, DBGFCODETYPE enmCodeType, PCDBGFADDRESS pAddrFrame,
                                                   PCDBGFADDRESS pAddrStack,PCDBGFADDRESS pAddrPC,
                                                   DBGFRETURNTYPE enmReturnType, PCDBGFSTACKFRAME *ppFirstFrame);
VMMR3DECL(PCDBGFSTACKFRAME) DBGFR3StackWalkNext(PCDBGFSTACKFRAME pCurrent);
VMMR3DECL(void)             DBGFR3StackWalkEnd(PCDBGFSTACKFRAME pFirstFrame);

#endif /* IN_RING3 */


#ifdef IN_RING3 /* The disassembly API only works in ring-3. */

/** @name Flags to pass to DBGFR3DisasInstrEx().
 * @{ */
/** Disassemble the current guest instruction, with annotations. */
#define DBGF_DISAS_FLAGS_CURRENT_GUEST      RT_BIT(0)
/** Disassemble the current hypervisor instruction, with annotations. */
#define DBGF_DISAS_FLAGS_CURRENT_HYPER      RT_BIT(1)
/** No annotations for current context. */
#define DBGF_DISAS_FLAGS_NO_ANNOTATION      RT_BIT(2)
/** No symbol lookup. */
#define DBGF_DISAS_FLAGS_NO_SYMBOLS         RT_BIT(3)
/** No instruction bytes. */
#define DBGF_DISAS_FLAGS_NO_BYTES           RT_BIT(4)
/** No address in the output. */
#define DBGF_DISAS_FLAGS_NO_ADDRESS         RT_BIT(5)
/** Probably a hypervisor instruction. */
#define DBGF_DISAS_FLAGS_HYPER              RT_BIT(6)
/** Disassemble original unpatched bytes (PATM). */
#define DBGF_DISAS_FLAGS_UNPATCHED_BYTES    RT_BIT(7)
/** Annotate patched instructions. */
#define DBGF_DISAS_FLAGS_ANNOTATE_PATCHED   RT_BIT(8)
/** Disassemble in the default mode of the specific context. */
#define DBGF_DISAS_FLAGS_DEFAULT_MODE       UINT32_C(0x00000000)
/** Disassemble in 16-bit mode. */
#define DBGF_DISAS_FLAGS_16BIT_MODE         UINT32_C(0x10000000)
/** Disassemble in 16-bit mode with real mode address translation. */
#define DBGF_DISAS_FLAGS_16BIT_REAL_MODE    UINT32_C(0x20000000)
/** Disassemble in 32-bit mode. */
#define DBGF_DISAS_FLAGS_32BIT_MODE         UINT32_C(0x30000000)
/** Disassemble in 64-bit mode. */
#define DBGF_DISAS_FLAGS_64BIT_MODE         UINT32_C(0x40000000)
/** The disassembly mode mask. */
#define DBGF_DISAS_FLAGS_MODE_MASK          UINT32_C(0x70000000)
/** Mask containing the valid flags. */
#define DBGF_DISAS_FLAGS_VALID_MASK         UINT32_C(0x700001ff)
/** @} */

/** Special flat selector. */
#define DBGF_SEL_FLAT                       1

VMMR3DECL(int)      DBGFR3DisasInstrEx(PUVM pUVM, VMCPUID idCpu, RTSEL Sel, RTGCPTR GCPtr, uint32_t fFlags,
                                       char *pszOutput, uint32_t cbOutput, uint32_t *pcbInstr);
VMMR3_INT_DECL(int) DBGFR3DisasInstrCurrent(PVMCPU pVCpu, char *pszOutput, uint32_t cbOutput);
VMMR3DECL(int)      DBGFR3DisasInstrCurrentLogInternal(PVMCPU pVCpu, const char *pszPrefix);

/** @def DBGFR3_DISAS_INSTR_CUR_LOG
 * Disassembles the current guest context instruction and writes it to the log.
 * All registers and data will be displayed. Addresses will be attempted resolved to symbols.
 */
#ifdef LOG_ENABLED
# define DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, pszPrefix) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3DisasInstrCurrentLogInternal(pVCpu, pszPrefix); \
    } while (0)
#else
# define DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, pszPrefix) do { } while (0)
#endif

VMMR3DECL(int) DBGFR3DisasInstrLogInternal(PVMCPU pVCpu, RTSEL Sel, RTGCPTR GCPtr, const char *pszPrefix);

/** @def DBGFR3_DISAS_INSTR_LOG
 * Disassembles the specified guest context instruction and writes it to the log.
 * Addresses will be attempted resolved to symbols.
 * @thread Any EMT.
 */
# ifdef LOG_ENABLED
#  define DBGFR3_DISAS_INSTR_LOG(pVCpu, Sel, GCPtr, pszPrefix) \
    do { \
        if (LogIsEnabled()) \
            DBGFR3DisasInstrLogInternal(pVCpu, Sel, GCPtr, pszPrefix); \
    } while (0)
# else
#  define DBGFR3_DISAS_INSTR_LOG(pVCpu, Sel, GCPtr, pszPrefix) do { } while (0)
# endif
#endif


#ifdef IN_RING3
VMMR3DECL(int) DBGFR3MemScan(PUVM pUVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, RTGCUINTPTR cbRange, RTGCUINTPTR uAlign,
                             const void *pvNeedle, size_t cbNeedle, PDBGFADDRESS pHitAddress);
VMMR3DECL(int) DBGFR3MemRead(PUVM pUVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, void *pvBuf, size_t cbRead);
VMMR3DECL(int) DBGFR3MemReadString(PUVM pUVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, char *pszBuf, size_t cbBuf);
VMMR3DECL(int) DBGFR3MemWrite(PUVM pUVM, VMCPUID idCpu, PCDBGFADDRESS pAddress, void const *pvBuf, size_t cbRead);
#endif


/** @name Flags for DBGFR3PagingDumpEx, PGMR3DumpHierarchyHCEx and
 * PGMR3DumpHierarchyGCEx
 * @{ */
/** The CR3 from the current CPU state. */
#define DBGFPGDMP_FLAGS_CURRENT_CR3     RT_BIT_32(0)
/** The current CPU paging mode (PSE, PAE, LM, EPT, NX). */
#define DBGFPGDMP_FLAGS_CURRENT_MODE    RT_BIT_32(1)
/** Whether PSE is enabled (!DBGFPGDMP_FLAGS_CURRENT_STATE).
 * Same value as X86_CR4_PSE. */
#define DBGFPGDMP_FLAGS_PSE             RT_BIT_32(4) /*  */
/** Whether PAE is enabled (!DBGFPGDMP_FLAGS_CURRENT_STATE).
 * Same value as X86_CR4_PAE. */
#define DBGFPGDMP_FLAGS_PAE             RT_BIT_32(5) /*  */
/** Whether LME is enabled (!DBGFPGDMP_FLAGS_CURRENT_STATE).
 * Same value as MSR_K6_EFER_LME. */
#define DBGFPGDMP_FLAGS_LME             RT_BIT_32(8)
/** Whether nested paging is enabled (!DBGFPGDMP_FLAGS_CURRENT_STATE). */
#define DBGFPGDMP_FLAGS_NP              RT_BIT_32(9)
/** Whether extended nested page tables are enabled
 * (!DBGFPGDMP_FLAGS_CURRENT_STATE). */
#define DBGFPGDMP_FLAGS_EPT             RT_BIT_32(10)
/** Whether no-execution is enabled (!DBGFPGDMP_FLAGS_CURRENT_STATE).
 * Same value as MSR_K6_EFER_NXE. */
#define DBGFPGDMP_FLAGS_NXE             RT_BIT_32(11)
/** Whether to print the CR3. */
#define DBGFPGDMP_FLAGS_PRINT_CR3       RT_BIT_32(27)
/** Whether to print the header. */
#define DBGFPGDMP_FLAGS_HEADER          RT_BIT_32(28)
/** Whether to dump additional page information. */
#define DBGFPGDMP_FLAGS_PAGE_INFO       RT_BIT_32(29)
/** Dump the shadow tables if set.
 * Cannot be used together with DBGFPGDMP_FLAGS_GUEST. */
#define DBGFPGDMP_FLAGS_SHADOW          RT_BIT_32(30)
/** Dump the guest tables if set.
 * Cannot be used together with DBGFPGDMP_FLAGS_SHADOW. */
#define DBGFPGDMP_FLAGS_GUEST           RT_BIT_32(31)
/** Mask of valid bits. */
#define DBGFPGDMP_FLAGS_VALID_MASK      UINT32_C(0xf8000f33)
/** The mask of bits controlling the paging mode. */
#define DBGFPGDMP_FLAGS_MODE_MASK       UINT32_C(0x00000f32)
/** @}  */
VMMDECL(int) DBGFR3PagingDumpEx(PUVM pUVM, VMCPUID idCpu, uint32_t fFlags, uint64_t cr3, uint64_t u64FirstAddr,
                                uint64_t u64LastAddr, uint32_t cMaxDepth, PCDBGFINFOHLP pHlp);


/** @name DBGFR3SelQueryInfo flags.
 * @{ */
/** Get the info from the guest descriptor table. */
#define DBGFSELQI_FLAGS_DT_GUEST            UINT32_C(0)
/** Get the info from the shadow descriptor table.
 * Only works in raw-mode.  */
#define DBGFSELQI_FLAGS_DT_SHADOW           UINT32_C(1)
/** If currently executing in in 64-bit mode, blow up data selectors. */
#define DBGFSELQI_FLAGS_DT_ADJ_64BIT_MODE   UINT32_C(2)
/** @} */
VMMR3DECL(int) DBGFR3SelQueryInfo(PUVM pUVM, VMCPUID idCpu, RTSEL Sel, uint32_t fFlags, PDBGFSELINFO pSelInfo);


/**
 * Register identifiers.
 */
typedef enum DBGFREG
{
    /* General purpose registers: */
    DBGFREG_AL  = 0,
    DBGFREG_AX  = DBGFREG_AL,
    DBGFREG_EAX = DBGFREG_AL,
    DBGFREG_RAX = DBGFREG_AL,

    DBGFREG_CL,
    DBGFREG_CX  = DBGFREG_CL,
    DBGFREG_ECX = DBGFREG_CL,
    DBGFREG_RCX = DBGFREG_CL,

    DBGFREG_DL,
    DBGFREG_DX  = DBGFREG_DL,
    DBGFREG_EDX = DBGFREG_DL,
    DBGFREG_RDX = DBGFREG_DL,

    DBGFREG_BL,
    DBGFREG_BX  = DBGFREG_BL,
    DBGFREG_EBX = DBGFREG_BL,
    DBGFREG_RBX = DBGFREG_BL,

    DBGFREG_SPL,
    DBGFREG_SP  = DBGFREG_SPL,
    DBGFREG_ESP = DBGFREG_SPL,
    DBGFREG_RSP = DBGFREG_SPL,

    DBGFREG_BPL,
    DBGFREG_BP  = DBGFREG_BPL,
    DBGFREG_EBP = DBGFREG_BPL,
    DBGFREG_RBP = DBGFREG_BPL,

    DBGFREG_SIL,
    DBGFREG_SI  = DBGFREG_SIL,
    DBGFREG_ESI = DBGFREG_SIL,
    DBGFREG_RSI = DBGFREG_SIL,

    DBGFREG_DIL,
    DBGFREG_DI  = DBGFREG_DIL,
    DBGFREG_EDI = DBGFREG_DIL,
    DBGFREG_RDI = DBGFREG_DIL,

    DBGFREG_R8,
    DBGFREG_R8B = DBGFREG_R8,
    DBGFREG_R8W = DBGFREG_R8,
    DBGFREG_R8D = DBGFREG_R8,

    DBGFREG_R9,
    DBGFREG_R9B = DBGFREG_R9,
    DBGFREG_R9W = DBGFREG_R9,
    DBGFREG_R9D = DBGFREG_R9,

    DBGFREG_R10,
    DBGFREG_R10B = DBGFREG_R10,
    DBGFREG_R10W = DBGFREG_R10,
    DBGFREG_R10D = DBGFREG_R10,

    DBGFREG_R11,
    DBGFREG_R11B = DBGFREG_R11,
    DBGFREG_R11W = DBGFREG_R11,
    DBGFREG_R11D = DBGFREG_R11,

    DBGFREG_R12,
    DBGFREG_R12B = DBGFREG_R12,
    DBGFREG_R12W = DBGFREG_R12,
    DBGFREG_R12D = DBGFREG_R12,

    DBGFREG_R13,
    DBGFREG_R13B = DBGFREG_R13,
    DBGFREG_R13W = DBGFREG_R13,
    DBGFREG_R13D = DBGFREG_R13,

    DBGFREG_R14,
    DBGFREG_R14B = DBGFREG_R14,
    DBGFREG_R14W = DBGFREG_R14,
    DBGFREG_R14D = DBGFREG_R14,

    DBGFREG_R15,
    DBGFREG_R15B = DBGFREG_R15,
    DBGFREG_R15W = DBGFREG_R15,
    DBGFREG_R15D = DBGFREG_R15,

    /* Segments and other special registers: */
    DBGFREG_CS,
    DBGFREG_CS_ATTR,
    DBGFREG_CS_BASE,
    DBGFREG_CS_LIMIT,

    DBGFREG_DS,
    DBGFREG_DS_ATTR,
    DBGFREG_DS_BASE,
    DBGFREG_DS_LIMIT,

    DBGFREG_ES,
    DBGFREG_ES_ATTR,
    DBGFREG_ES_BASE,
    DBGFREG_ES_LIMIT,

    DBGFREG_FS,
    DBGFREG_FS_ATTR,
    DBGFREG_FS_BASE,
    DBGFREG_FS_LIMIT,

    DBGFREG_GS,
    DBGFREG_GS_ATTR,
    DBGFREG_GS_BASE,
    DBGFREG_GS_LIMIT,

    DBGFREG_SS,
    DBGFREG_SS_ATTR,
    DBGFREG_SS_BASE,
    DBGFREG_SS_LIMIT,

    DBGFREG_IP,
    DBGFREG_EIP = DBGFREG_IP,
    DBGFREG_RIP = DBGFREG_IP,

    DBGFREG_FLAGS,
    DBGFREG_EFLAGS = DBGFREG_FLAGS,
    DBGFREG_RFLAGS = DBGFREG_FLAGS,

    /* FPU: */
    DBGFREG_FCW,
    DBGFREG_FSW,
    DBGFREG_FTW,
    DBGFREG_FOP,
    DBGFREG_FPUIP,
    DBGFREG_FPUCS,
    DBGFREG_FPUDP,
    DBGFREG_FPUDS,
    DBGFREG_MXCSR,
    DBGFREG_MXCSR_MASK,

    DBGFREG_ST0,
    DBGFREG_ST1,
    DBGFREG_ST2,
    DBGFREG_ST3,
    DBGFREG_ST4,
    DBGFREG_ST5,
    DBGFREG_ST6,
    DBGFREG_ST7,

    DBGFREG_MM0,
    DBGFREG_MM1,
    DBGFREG_MM2,
    DBGFREG_MM3,
    DBGFREG_MM4,
    DBGFREG_MM5,
    DBGFREG_MM6,
    DBGFREG_MM7,

    /* SSE: */
    DBGFREG_XMM0,
    DBGFREG_XMM1,
    DBGFREG_XMM2,
    DBGFREG_XMM3,
    DBGFREG_XMM4,
    DBGFREG_XMM5,
    DBGFREG_XMM6,
    DBGFREG_XMM7,
    DBGFREG_XMM8,
    DBGFREG_XMM9,
    DBGFREG_XMM10,
    DBGFREG_XMM11,
    DBGFREG_XMM12,
    DBGFREG_XMM13,
    DBGFREG_XMM14,
    DBGFREG_XMM15,
    /** @todo add XMM aliases. */

    /* AVX: */
    DBGFREG_YMM0,
    DBGFREG_YMM1,
    DBGFREG_YMM2,
    DBGFREG_YMM3,
    DBGFREG_YMM4,
    DBGFREG_YMM5,
    DBGFREG_YMM6,
    DBGFREG_YMM7,
    DBGFREG_YMM8,
    DBGFREG_YMM9,
    DBGFREG_YMM10,
    DBGFREG_YMM11,
    DBGFREG_YMM12,
    DBGFREG_YMM13,
    DBGFREG_YMM14,
    DBGFREG_YMM15,

    /* System registers: */
    DBGFREG_GDTR_BASE,
    DBGFREG_GDTR_LIMIT,
    DBGFREG_IDTR_BASE,
    DBGFREG_IDTR_LIMIT,
    DBGFREG_LDTR,
    DBGFREG_LDTR_ATTR,
    DBGFREG_LDTR_BASE,
    DBGFREG_LDTR_LIMIT,
    DBGFREG_TR,
    DBGFREG_TR_ATTR,
    DBGFREG_TR_BASE,
    DBGFREG_TR_LIMIT,

    DBGFREG_CR0,
    DBGFREG_CR2,
    DBGFREG_CR3,
    DBGFREG_CR4,
    DBGFREG_CR8,

    DBGFREG_DR0,
    DBGFREG_DR1,
    DBGFREG_DR2,
    DBGFREG_DR3,
    DBGFREG_DR6,
    DBGFREG_DR7,

    /* MSRs: */
    DBGFREG_MSR_IA32_APICBASE,
    DBGFREG_MSR_IA32_CR_PAT,
    DBGFREG_MSR_IA32_PERF_STATUS,
    DBGFREG_MSR_IA32_SYSENTER_CS,
    DBGFREG_MSR_IA32_SYSENTER_EIP,
    DBGFREG_MSR_IA32_SYSENTER_ESP,
    DBGFREG_MSR_IA32_TSC,
    DBGFREG_MSR_K6_EFER,
    DBGFREG_MSR_K6_STAR,
    DBGFREG_MSR_K8_CSTAR,
    DBGFREG_MSR_K8_FS_BASE,
    DBGFREG_MSR_K8_GS_BASE,
    DBGFREG_MSR_K8_KERNEL_GS_BASE,
    DBGFREG_MSR_K8_LSTAR,
    DBGFREG_MSR_K8_SF_MASK,
    DBGFREG_MSR_K8_TSC_AUX,

    /** The number of registers to pass to DBGFR3RegQueryAll. */
    DBGFREG_ALL_COUNT,

    /* Misc aliases that doesn't need be part of the 'all' query: */
    DBGFREG_AH = DBGFREG_ALL_COUNT,
    DBGFREG_CH,
    DBGFREG_DH,
    DBGFREG_BH,
    DBGFREG_GDTR,
    DBGFREG_IDTR,

    /** The end of the registers.  */
    DBGFREG_END,
    /** The usual 32-bit type hack. */
    DBGFREG_32BIT_HACK = 0x7fffffff
} DBGFREG;
/** Pointer to a register identifier. */
typedef DBGFREG *PDBGFREG;
/** Pointer to a const register identifier. */
typedef DBGFREG const *PCDBGFREG;

/**
 * Register value type.
 */
typedef enum DBGFREGVALTYPE
{
    DBGFREGVALTYPE_INVALID = 0,
    /** Unsigned 8-bit register value. */
    DBGFREGVALTYPE_U8,
    /** Unsigned 16-bit register value. */
    DBGFREGVALTYPE_U16,
    /** Unsigned 32-bit register value. */
    DBGFREGVALTYPE_U32,
    /** Unsigned 64-bit register value. */
    DBGFREGVALTYPE_U64,
    /** Unsigned 128-bit register value. */
    DBGFREGVALTYPE_U128,
    /** Unsigned 256-bit register value. */
    DBGFREGVALTYPE_U256,
    /** Unsigned 512-bit register value. */
    DBGFREGVALTYPE_U512,
    /** Long double register value. */
    DBGFREGVALTYPE_R80,
    /** Descriptor table register value. */
    DBGFREGVALTYPE_DTR,
    /** End of the valid register value types. */
    DBGFREGVALTYPE_END,
    /** The usual 32-bit type hack. */
    DBGFREGVALTYPE_32BIT_HACK = 0x7fffffff
} DBGFREGVALTYPE;
/** Pointer to a register value type. */
typedef DBGFREGVALTYPE *PDBGFREGVALTYPE;

/**
 * A generic register value type.
 */
typedef union DBGFREGVAL
{
    uint64_t    au64[8];        /**< The 64-bit array view. First because of the initializer. */
    uint32_t    au32[16];       /**< The 32-bit array view. */
    uint16_t    au16[32];       /**< The 16-bit array view. */
    uint8_t     au8[64];        /**< The 8-bit array view. */

    uint8_t     u8;             /**< The 8-bit view. */
    uint16_t    u16;            /**< The 16-bit view. */
    uint32_t    u32;            /**< The 32-bit view. */
    uint64_t    u64;            /**< The 64-bit view. */
    RTUINT128U  u128;           /**< The 128-bit view. */
    RTUINT256U  u256;           /**< The 256-bit view. */
    RTUINT512U  u512;           /**< The 512-bit view. */
    RTFLOAT80U  r80;            /**< The 80-bit floating point view. */
    RTFLOAT80U2 r80Ex;          /**< The 80-bit floating point view v2. */
    /** GDTR or LDTR (DBGFREGVALTYPE_DTR). */
    struct
    {
        /** The table address. */
        uint64_t u64Base;
        /** The table limit (length minus 1). */
        uint32_t u32Limit;
    }           dtr;
} DBGFREGVAL;
/** Pointer to a generic register value type. */
typedef DBGFREGVAL *PDBGFREGVAL;
/** Pointer to a const generic register value type. */
typedef DBGFREGVAL const *PCDBGFREGVAL;

/** Initialize a DBGFREGVAL variable to all zeros.  */
#define DBGFREGVAL_INITIALIZE_ZERO { { 0, 0, 0, 0, 0, 0, 0, 0 } }
/** Initialize a DBGFREGVAL variable to all bits set .  */
#define DBGFREGVAL_INITIALIZE_FFFF { { UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX } }


VMMDECL(ssize_t) DBGFR3RegFormatValue(char *pszBuf, size_t cbBuf, PCDBGFREGVAL pValue, DBGFREGVALTYPE enmType, bool fSpecial);
VMMDECL(ssize_t) DBGFR3RegFormatValueEx(char *pszBuf, size_t cbBuf, PCDBGFREGVAL pValue, DBGFREGVALTYPE enmType,
                                        unsigned uBase, signed int cchWidth, signed int cchPrecision, uint32_t fFlags);

/**
 * Register sub-field descriptor.
 */
typedef struct DBGFREGSUBFIELD
{
    /** The name of the sub-field.  NULL is used to terminate the array. */
    const char     *pszName;
    /** The index of the first bit.  Ignored if pfnGet is set. */
    uint8_t         iFirstBit;
    /** The number of bits.  Mandatory. */
    uint8_t         cBits;
    /** The shift count.  Not applied when pfnGet is set, but used to
     * calculate the minimum type. */
    int8_t          cShift;
    /** Sub-field flags, DBGFREGSUBFIELD_FLAGS_XXX.  */
    uint8_t         fFlags;
    /** Getter (optional).
     * @remarks Does not take the device lock or anything like that.
     */
    DECLCALLBACKMEMBER(int, pfnGet)(void *pvUser, struct DBGFREGSUBFIELD const *pSubField, PRTUINT128U puValue);
    /** Setter (optional).
     * @remarks Does not take the device lock or anything like that.
     */
    DECLCALLBACKMEMBER(int, pfnSet)(void *pvUser, struct DBGFREGSUBFIELD const *pSubField, RTUINT128U uValue, RTUINT128U fMask);
} DBGFREGSUBFIELD;
/** Pointer to a const register sub-field descriptor. */
typedef DBGFREGSUBFIELD const *PCDBGFREGSUBFIELD;

/** @name DBGFREGSUBFIELD_FLAGS_XXX
 * @{ */
/** The sub-field is read-only. */
#define DBGFREGSUBFIELD_FLAGS_READ_ONLY     UINT8_C(0x01)
/** @} */

/** Macro for creating a read-write sub-field entry without getters. */
#define DBGFREGSUBFIELD_RW(a_szName, a_iFirstBit, a_cBits, a_cShift) \
    { a_szName, a_iFirstBit, a_cBits, a_cShift, 0 /*fFlags*/, NULL /*pfnGet*/, NULL /*pfnSet*/ }
/** Macro for creating a read-write sub-field entry with getters. */
#define DBGFREGSUBFIELD_RW_SG(a_szName, a_cBits, a_cShift, a_pfnGet, a_pfnSet) \
    { a_szName, 0 /*iFirstBit*/, a_cBits, a_cShift, 0 /*fFlags*/, a_pfnGet, a_pfnSet }
/** Macro for creating a read-only sub-field entry without getters. */
#define DBGFREGSUBFIELD_RO(a_szName, a_iFirstBit, a_cBits, a_cShift) \
    { a_szName, a_iFirstBit, a_cBits, a_cShift, DBGFREGSUBFIELD_FLAGS_READ_ONLY, NULL /*pfnGet*/, NULL /*pfnSet*/ }
/** Macro for creating a terminator sub-field entry.  */
#define DBGFREGSUBFIELD_TERMINATOR() \
    { NULL, 0, 0, 0, 0, NULL, NULL }

/**
 * Register alias descriptor.
 */
typedef struct DBGFREGALIAS
{
    /** The alias name.  NULL is used to terminate the array. */
    const char     *pszName;
    /** Set to a valid type if the alias has a different type. */
    DBGFREGVALTYPE  enmType;
} DBGFREGALIAS;
/** Pointer to a const register alias descriptor. */
typedef DBGFREGALIAS const *PCDBGFREGALIAS;

/**
 * Register descriptor.
 */
typedef struct DBGFREGDESC
{
    /** The normal register name. */
    const char             *pszName;
    /** The register identifier if this is a CPU register. */
    DBGFREG                 enmReg;
    /** The default register type. */
    DBGFREGVALTYPE          enmType;
    /** Flags, see DBGFREG_FLAGS_XXX.  */
    uint32_t                fFlags;
    /** The internal register indicator.
     * For CPU registers this is the offset into the CPUMCTX structure,
     * thuse the 'off' prefix. */
    uint32_t                offRegister;
    /** Getter.
     * @remarks Does not take the device lock or anything like that.
     */
    DECLCALLBACKMEMBER(int, pfnGet)(void *pvUser, struct DBGFREGDESC const *pDesc, PDBGFREGVAL pValue);
    /** Setter.
     * @remarks Does not take the device lock or anything like that.
     */
    DECLCALLBACKMEMBER(int, pfnSet)(void *pvUser, struct DBGFREGDESC const *pDesc, PCDBGFREGVAL pValue, PCDBGFREGVAL pfMask);
    /** Aliases (optional). */
    PCDBGFREGALIAS          paAliases;
    /** Sub fields (optional). */
    PCDBGFREGSUBFIELD       paSubFields;
} DBGFREGDESC;

/** @name Macros for constructing DBGFREGDESC arrays.
 * @{ */
#define DBGFREGDESC_RW(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            a_offRegister, a_pfnGet, a_pfnSet, NULL /*paAlises*/, NULL /*paSubFields*/ }
#define DBGFREGDESC_RO(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, a_offRegister, a_pfnGet, a_pfnSet, NULL /*paAlises*/, NULL /*paSubFields*/ }
#define DBGFREGDESC_RW_A(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, NULL /*paSubFields*/ }
#define DBGFREGDESC_RO_A(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, NULL /*paSubFields*/ }
#define DBGFREGDESC_RW_S(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paSubFields) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            a_offRegister, a_pfnGet, a_pfnSet, /*paAliases*/, a_paSubFields }
#define DBGFREGDESC_RO_S(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paSubFields) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, a_offRegister, a_pfnGet, a_pfnSet, /*paAliases*/, a_paSubFields }
#define DBGFREGDESC_RW_AS(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, 0 /*fFlags*/,            a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define DBGFREGDESC_RO_AS(a_szName, a_TypeSuff, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields) \
    { a_szName, DBGFREG_END, DBGFREGVALTYPE_##a_TypeSuff, DBGFREG_FLAGS_READ_ONLY, a_offRegister, a_pfnGet, a_pfnSet, a_paAliases, a_paSubFields }
#define DBGFREGDESC_TERMINATOR() \
    { NULL, DBGFREG_END, DBGFREGVALTYPE_INVALID, 0, 0, NULL, NULL, NULL, NULL }
/** @} */


/** @name DBGFREG_FLAGS_XXX
 * @{ */
/** The register is read-only. */
#define DBGFREG_FLAGS_READ_ONLY         RT_BIT_32(0)
/** @} */

/**
 * Entry in a batch query or set operation.
 */
typedef struct DBGFREGENTRY
{
    /** The register identifier. */
    DBGFREG         enmReg;
    /** The size of the value in bytes. */
    DBGFREGVALTYPE  enmType;
    /** The register value. The valid view is indicated by enmType. */
    DBGFREGVAL      Val;
} DBGFREGENTRY;
/** Pointer to a register entry in a batch operation. */
typedef DBGFREGENTRY *PDBGFREGENTRY;
/** Pointer to a const register entry in a batch operation. */
typedef DBGFREGENTRY const *PCDBGFREGENTRY;

/** Used with DBGFR3Reg* to indicate the hypervisor register set instead of the
 *  guest. */
#define DBGFREG_HYPER_VMCPUID       UINT32_C(0x01000000)

VMMR3DECL(int) DBGFR3RegCpuQueryU8(  PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint8_t     *pu8);
VMMR3DECL(int) DBGFR3RegCpuQueryU16( PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint16_t    *pu16);
VMMR3DECL(int) DBGFR3RegCpuQueryU32( PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint32_t    *pu32);
VMMR3DECL(int) DBGFR3RegCpuQueryU64( PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t    *pu64);
VMMR3DECL(int) DBGFR3RegCpuQueryU128(PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint128_t   *pu128);
VMMR3DECL(int) DBGFR3RegCpuQueryLrd( PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, long double *plrd);
VMMR3DECL(int) DBGFR3RegCpuQueryXdtr(PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t *pu64Base, uint16_t *pu16Limit);
#if 0
VMMR3DECL(int) DBGFR3RegCpuQueryBatch(PUVM pUVM,VMCPUID idCpu, PDBGFREGENTRY paRegs, size_t cRegs);
VMMR3DECL(int) DBGFR3RegCpuQueryAll( PUVM pUVM, VMCPUID idCpu, PDBGFREGENTRY paRegs, size_t cRegs);

VMMR3DECL(int) DBGFR3RegCpuSetU8(    PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint8_t     u8);
VMMR3DECL(int) DBGFR3RegCpuSetU16(   PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint16_t    u16);
VMMR3DECL(int) DBGFR3RegCpuSetU32(   PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint32_t    u32);
VMMR3DECL(int) DBGFR3RegCpuSetU64(   PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint64_t    u64);
VMMR3DECL(int) DBGFR3RegCpuSetU128(  PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, uint128_t   u128);
VMMR3DECL(int) DBGFR3RegCpuSetLrd(   PUVM pUVM, VMCPUID idCpu, DBGFREG enmReg, long double lrd);
VMMR3DECL(int) DBGFR3RegCpuSetBatch( PUVM pUVM, VMCPUID idCpu, PCDBGFREGENTRY paRegs, size_t cRegs);
#endif

VMMR3DECL(const char *) DBGFR3RegCpuName(PUVM pUVM, DBGFREG enmReg, DBGFREGVALTYPE enmType);

VMMR3_INT_DECL(int) DBGFR3RegRegisterCpu(PVM pVM, PVMCPU pVCpu, PCDBGFREGDESC paRegisters, bool fGuestRegs);
VMMR3_INT_DECL(int) DBGFR3RegRegisterDevice(PVM pVM, PCDBGFREGDESC paRegisters, PPDMDEVINS pDevIns,
                                            const char *pszPrefix, uint32_t iInstance);

/**
 * Entry in a named batch query or set operation.
 */
typedef struct DBGFREGENTRYNM
{
    /** The register name. */
    const char     *pszName;
    /** The size of the value in bytes. */
    DBGFREGVALTYPE  enmType;
    /** The register value. The valid view is indicated by enmType. */
    DBGFREGVAL      Val;
} DBGFREGENTRYNM;
/** Pointer to a named register entry in a batch operation. */
typedef DBGFREGENTRYNM *PDBGFREGENTRYNM;
/** Pointer to a const named register entry in a batch operation. */
typedef DBGFREGENTRYNM const *PCDBGFREGENTRYNM;

VMMR3DECL(int) DBGFR3RegNmValidate( PUVM pUVM, VMCPUID idDefCpu, const char *pszReg);

VMMR3DECL(int) DBGFR3RegNmQuery(    PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, PDBGFREGVAL pValue, PDBGFREGVALTYPE penmType);
VMMR3DECL(int) DBGFR3RegNmQueryU8(  PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint8_t     *pu8);
VMMR3DECL(int) DBGFR3RegNmQueryU16( PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint16_t    *pu16);
VMMR3DECL(int) DBGFR3RegNmQueryU32( PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint32_t    *pu32);
VMMR3DECL(int) DBGFR3RegNmQueryU64( PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint64_t    *pu64);
VMMR3DECL(int) DBGFR3RegNmQueryU128(PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, PRTUINT128U  pu128);
/*VMMR3DECL(int) DBGFR3RegNmQueryLrd( PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, long double *plrd);*/
VMMR3DECL(int) DBGFR3RegNmQueryXdtr(PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint64_t *pu64Base, uint16_t *pu16Limit);
VMMR3DECL(int) DBGFR3RegNmQueryBatch(PUVM pUVM,VMCPUID idDefCpu, PDBGFREGENTRYNM paRegs, size_t cRegs);
VMMR3DECL(int) DBGFR3RegNmQueryAllCount(PUVM pUVM, size_t *pcRegs);
VMMR3DECL(int) DBGFR3RegNmQueryAll( PUVM pUVM,                   PDBGFREGENTRYNM paRegs, size_t cRegs);

VMMR3DECL(int) DBGFR3RegNmSet(      PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, PCDBGFREGVAL pValue, DBGFREGVALTYPE enmType);
VMMR3DECL(int) DBGFR3RegNmSetU8(    PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint8_t     u8);
VMMR3DECL(int) DBGFR3RegNmSetU16(   PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint16_t    u16);
VMMR3DECL(int) DBGFR3RegNmSetU32(   PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint32_t    u32);
VMMR3DECL(int) DBGFR3RegNmSetU64(   PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, uint64_t    u64);
VMMR3DECL(int) DBGFR3RegNmSetU128(  PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, RTUINT128U  u128);
VMMR3DECL(int) DBGFR3RegNmSetLrd(   PUVM pUVM, VMCPUID idDefCpu, const char *pszReg, long double lrd);
VMMR3DECL(int) DBGFR3RegNmSetBatch( PUVM pUVM, VMCPUID idDefCpu, PCDBGFREGENTRYNM paRegs, size_t cRegs);

/** @todo add enumeration methods.  */

VMMR3DECL(int) DBGFR3RegPrintf( PUVM pUVM, VMCPUID idDefCpu, char *pszBuf, size_t cbBuf, const char *pszFormat, ...);
VMMR3DECL(int) DBGFR3RegPrintfV(PUVM pUVM, VMCPUID idDefCpu, char *pszBuf, size_t cbBuf, const char *pszFormat, va_list va);


/**
 * Guest OS digger interface identifier.
 *
 * This is for use together with PDBGFR3QueryInterface and is used to
 * obtain access to optional interfaces.
 */
typedef enum DBGFOSINTERFACE
{
    /** The usual invalid entry. */
    DBGFOSINTERFACE_INVALID = 0,
    /** Process info. */
    DBGFOSINTERFACE_PROCESS,
    /** Thread info. */
    DBGFOSINTERFACE_THREAD,
    /** Kernel message log - DBGFOSIDMESG. */
    DBGFOSINTERFACE_DMESG,
    /** The end of the valid entries. */
    DBGFOSINTERFACE_END,
    /** The usual 32-bit type blowup. */
    DBGFOSINTERFACE_32BIT_HACK = 0x7fffffff
} DBGFOSINTERFACE;
/** Pointer to a Guest OS digger interface identifier. */
typedef DBGFOSINTERFACE *PDBGFOSINTERFACE;
/** Pointer to a const Guest OS digger interface identifier. */
typedef DBGFOSINTERFACE const *PCDBGFOSINTERFACE;


/**
 * Guest OS Digger Registration Record.
 *
 * This is used with the DBGFR3OSRegister() API.
 */
typedef struct DBGFOSREG
{
    /** Magic value (DBGFOSREG_MAGIC). */
    uint32_t u32Magic;
    /** Flags. Reserved. */
    uint32_t fFlags;
    /** The size of the instance data. */
    uint32_t cbData;
    /** Operative System name. */
    char szName[24];

    /**
     * Constructs the instance.
     *
     * @returns VBox status code.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnConstruct)(PUVM pUVM, void *pvData);

    /**
     * Destroys the instance.
     *
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(void, pfnDestruct)(PUVM pUVM, void *pvData);

    /**
     * Probes the guest memory for OS finger prints.
     *
     * No setup or so is performed, it will be followed by a call to pfnInit
     * or pfnRefresh that should take care of that.
     *
     * @returns true if is an OS handled by this module, otherwise false.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(bool, pfnProbe)(PUVM pUVM, void *pvData);

    /**
     * Initializes a fresly detected guest, loading symbols and such useful stuff.
     *
     * This is called after pfnProbe.
     *
     * @returns VBox status code.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnInit)(PUVM pUVM, void *pvData);

    /**
     * Refreshes symbols and stuff following a redetection of the same OS.
     *
     * This is called after pfnProbe.
     *
     * @returns VBox status code.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnRefresh)(PUVM pUVM, void *pvData);

    /**
     * Terminates an OS when a new (or none) OS has been detected,
     * and before destruction.
     *
     * This is called after pfnProbe and if needed before pfnDestruct.
     *
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(void, pfnTerm)(PUVM pUVM, void *pvData);

    /**
     * Queries the version of the running OS.
     *
     * This is only called after pfnInit().
     *
     * @returns VBox status code.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData      Pointer to the instance data.
     * @param   pszVersion  Where to store the version string.
     * @param   cchVersion  The size of the version string buffer.
     */
    DECLCALLBACKMEMBER(int, pfnQueryVersion)(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion);

    /**
     * Queries the pointer to a interface.
     *
     * This is called after pfnProbe.
     *
     * The returned interface must be valid until pfnDestruct is called.  Two calls
     * to this method with the same @a enmIf value must return the same pointer.
     *
     * @returns Pointer to the interface if available, NULL if not available.
     * @param   pUVM    The user mode VM handle.
     * @param   pvData  Pointer to the instance data.
     * @param   enmIf   The interface identifier.
     */
    DECLCALLBACKMEMBER(void *, pfnQueryInterface)(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf);

    /** Trailing magic (DBGFOSREG_MAGIC). */
    uint32_t u32EndMagic;
} DBGFOSREG;
/** Pointer to a Guest OS digger registration record. */
typedef DBGFOSREG *PDBGFOSREG;
/** Pointer to a const Guest OS digger registration record. */
typedef DBGFOSREG const *PCDBGFOSREG;

/** Magic value for DBGFOSREG::u32Magic and DBGFOSREG::u32EndMagic. (Hitomi Kanehara) */
#define DBGFOSREG_MAGIC     0x19830808


/**
 * Interface for querying kernel log messages (DBGFOSINTERFACE_DMESG).
 */
typedef struct DBGFOSIDMESG
{
    /** Trailing magic (DBGFOSIDMESG_MAGIC). */
    uint32_t    u32Magic;

    /**
     * Query the kernel log.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_FOUND if the messages could not be located.
     * @retval  VERR_INVALID_STATE if the messages was found to have unknown/invalid
     *          format.
     * @retval  VERR_BUFFER_OVERFLOW if the buffer isn't large enough, pcbActual
     *          will be set to the required buffer size.  The buffer, however, will
     *          be filled with as much data as it can hold (properly zero terminated
     *          of course).
     *
     * @param   pThis       Pointer to the interface structure.
     * @param   pUVM        The user mode VM handle.
     * @param   fFlags      Flags reserved for future use, MBZ.
     * @param   cMessages   The number of messages to retrieve, counting from the
     *                      end of the log (i.e. like tail), use UINT32_MAX for all.
     * @param   pszBuf      The output buffer.
     * @param   cbBuf       The buffer size.
     * @param   pcbActual   Where to store the number of bytes actually returned,
     *                      including zero terminator.  On VERR_BUFFER_OVERFLOW this
     *                      holds the necessary buffer size.  Optional.
     */
    DECLCALLBACKMEMBER(int, pfnQueryKernelLog)(struct DBGFOSIDMESG *pThis, PUVM pUVM, uint32_t fFlags, uint32_t cMessages,
                                               char *pszBuf, size_t cbBuf, size_t *pcbActual);
    /** Trailing magic (DBGFOSIDMESG_MAGIC). */
    uint32_t    u32EndMagic;
} DBGFOSIDMESG;
/** Pointer to the interface for query kernel log messages (DBGFOSINTERFACE_DMESG). */
typedef DBGFOSIDMESG *PDBGFOSIDMESG;
/** Magic value for DBGFOSIDMESG::32Magic and DBGFOSIDMESG::u32EndMagic. (Kenazburo Oe) */
#define DBGFOSIDMESG_MAGIC UINT32_C(0x19350131)


VMMR3DECL(int)      DBGFR3OSRegister(PUVM pUVM, PCDBGFOSREG pReg);
VMMR3DECL(int)      DBGFR3OSDeregister(PUVM pUVM, PCDBGFOSREG pReg);
VMMR3DECL(int)      DBGFR3OSDetect(PUVM pUVM, char *pszName, size_t cchName);
VMMR3DECL(int)      DBGFR3OSQueryNameAndVersion(PUVM pUVM, char *pszName, size_t cchName, char *pszVersion, size_t cchVersion);
VMMR3DECL(void *)   DBGFR3OSQueryInterface(PUVM pUVM, DBGFOSINTERFACE enmIf);


VMMR3DECL(int)      DBGFR3CoreWrite(PUVM pUVM, const char *pszFilename, bool fReplaceFile);


#ifdef IN_RING3

/** @defgroup grp_dbgf_plug_in      The DBGF Plug-in Interface
 * @{
 */

/** The plug-in module name prefix. */
# define DBGF_PLUG_IN_PREFIX        "DbgPlugIn"

/** The name of the plug-in entry point (FNDBGFPLUGIN) */
# define DBGF_PLUG_IN_ENTRYPOINT    "DbgPlugInEntry"

/**
 * DBGF plug-in operations.
 */
typedef enum DBGFPLUGINOP
{
    /** The usual invalid first value. */
    DBGFPLUGINOP_INVALID,
    /** Initialize the plug-in for a VM, register all the stuff.
     * The plug-in will be unloaded on failure.
     * uArg: The full VirtualBox version, see VBox/version.h. */
    DBGFPLUGINOP_INIT,
    /** Terminate the plug-ing for a VM, deregister all the stuff.
     * The plug-in will be unloaded after this call regardless of the return
     * code. */
    DBGFPLUGINOP_TERM,
    /** The usual 32-bit hack. */
    DBGFPLUGINOP_32BIT_HACK = 0x7fffffff
} DBGFPLUGINOP;

/**
 * DBGF plug-in main entry point.
 *
 * @returns VBox status code.
 *
 * @param   enmOperation    The operation.
 * @param   pUVM            The user mode VM handle. This may be NULL.
 * @param   uArg            Extra argument.
 */
typedef DECLCALLBACK(int) FNDBGFPLUGIN(DBGFPLUGINOP enmOperation, PUVM pUVM, uintptr_t uArg);
/** Pointer to a FNDBGFPLUGIN. */
typedef FNDBGFPLUGIN *PFNDBGFPLUGIN;

/** @copydoc FNDBGFPLUGIN */
DECLEXPORT(int) DbgPlugInEntry(DBGFPLUGINOP enmOperation, PUVM pUVM, uintptr_t uArg);

VMMR3DECL(int)  DBGFR3PlugInLoad(PUVM pUVM, const char *pszPlugIn, char *pszActual, size_t cbActual, PRTERRINFO pErrInfo);
VMMR3DECL(int)  DBGFR3PlugInUnload(PUVM pUVM, const char *pszName);
VMMR3DECL(void) DBGFR3PlugInLoadAll(PUVM pUVM);
VMMR3DECL(void) DBGFR3PlugInUnloadAll(PUVM pUVM);

/** @} */


/** @defgroup grp_dbgf_types        The DBGF type system Interface.
 * @{
 */

/** A few forward declarations. */
/** Pointer to a type registration structure. */
typedef struct DBGFTYPEREG *PDBGFTYPEREG;
/** Pointer to a const type registration structure. */
typedef const struct DBGFTYPEREG *PCDBGFTYPEREG;
/** Pointer to a typed buffer. */
typedef struct DBGFTYPEVAL *PDBGFTYPEVAL;

/**
 * DBGF built-in types.
 */
typedef enum DBGFTYPEBUILTIN
{
    /** The usual invalid first value. */
    DBGFTYPEBUILTIN_INVALID,
    /** Unsigned 8bit integer. */
    DBGFTYPEBUILTIN_UINT8,
    /** Signed 8bit integer. */
    DBGFTYPEBUILTIN_INT8,
    /** Unsigned 16bit integer. */
    DBGFTYPEBUILTIN_UINT16,
    /** Signed 16bit integer. */
    DBGFTYPEBUILTIN_INT16,
    /** Unsigned 32bit integer. */
    DBGFTYPEBUILTIN_UINT32,
    /** Signed 32bit integer. */
    DBGFTYPEBUILTIN_INT32,
    /** Unsigned 64bit integer. */
    DBGFTYPEBUILTIN_UINT64,
    /** Signed 64bit integer. */
    DBGFTYPEBUILTIN_INT64,
    /** 32bit Guest pointer */
    DBGFTYPEBUILTIN_PTR32,
    /** 64bit Guest pointer */
    DBGFTYPEBUILTIN_PTR64,
    /** Guest pointer - size depends on the guest bitness */
    DBGFTYPEBUILTIN_PTR,
    /** Type indicating a size, like size_t this can have different sizes
     * on 32bit and 64bit systems */
    DBGFTYPEBUILTIN_SIZE,
    /** 32bit float. */
    DBGFTYPEBUILTIN_FLOAT32,
    /** 64bit float (also known as double). */
    DBGFTYPEBUILTIN_FLOAT64,
    /** Compund types like structs and unions. */
    DBGFTYPEBUILTIN_COMPOUND,
    /** The usual 32-bit hack. */
    DBGFTYPEBUILTIN_32BIT_HACK = 0x7fffffff
} DBGFTYPEBUILTIN;
/** Pointer to a built-in type. */
typedef DBGFTYPEBUILTIN *PDBGFTYPEBUILTIN;
/** Pointer to a const built-in type. */
typedef const DBGFTYPEBUILTIN *PCDBGFTYPEBUILTIN;

/**
 * DBGF type value buffer.
 */
typedef union DBGFTYPEVALBUF
{
    uint8_t          u8;
    int8_t           i8;
    uint16_t         u16;
    int16_t          i16;
    uint32_t         u32;
    int32_t          i32;
    uint64_t         u64;
    int64_t          i64;
    float            f32;
    double           f64;
    uint64_t         size; /* For the built-in size_t which can be either 32-bit or 64-bit. */
    RTGCPTR          GCPtr;
    /** For embedded structs. */
    PDBGFTYPEVAL     pVal;
} DBGFTYPEVALBUF;
/** Pointer to a value. */
typedef DBGFTYPEVALBUF *PDBGFTYPEVALBUF;

/**
 * DBGF type value entry.
 */
typedef struct DBGFTYPEVALENTRY
{
    /** DBGF built-in type. */
    DBGFTYPEBUILTIN     enmType;
    /** Size of the type. */
    size_t              cbType;
    /** Number of entries, for arrays this can be > 1. */
    uint32_t            cEntries;
    /** Value buffer, depends on whether this is an array. */
    union
    {
        /** Single value. */
        DBGFTYPEVALBUF  Val;
        /** Pointer to the array of values. */
        PDBGFTYPEVALBUF pVal;
    } Buf;
} DBGFTYPEVALENTRY;
/** Pointer to a type value entry. */
typedef DBGFTYPEVALENTRY *PDBGFTYPEVALENTRY;
/** Pointer to a const type value entry. */
typedef const DBGFTYPEVALENTRY *PCDBGFTYPEVALENTRY;

/**
 * DBGF typed value.
 */
typedef struct DBGFTYPEVAL
{
    /** Pointer to the registration structure for this type. */
    PCDBGFTYPEREG       pTypeReg;
    /** Number of value entries. */
    uint32_t            cEntries;
    /** Variable sized array of value entries. */
    DBGFTYPEVALENTRY    aEntries[1];
} DBGFTYPEVAL;

/**
 * DBGF type variant.
 */
typedef enum DBGFTYPEVARIANT
{
    /** The usual invalid first value. */
    DBGFTYPEVARIANT_INVALID,
    /** A struct. */
    DBGFTYPEVARIANT_STRUCT,
    /** Union. */
    DBGFTYPEVARIANT_UNION,
    /** Alias for an existing type. */
    DBGFTYPEVARIANT_ALIAS,
    /** The usual 32-bit hack. */
    DBGFTYPEVARIANT_32BIT_HACK = 0x7fffffff
} DBGFTYPEVARIANT;

/** @name DBGFTYPEREGMEMBER Flags.
 * @{ */
/** The member is an array with a fixed size. */
# define DBGFTYPEREGMEMBER_F_ARRAY   RT_BIT_32(0)
/** The member denotes a pointer. */
# define DBGFTYPEREGMEMBER_F_POINTER RT_BIT_32(1)
/** @} */

/**
 * DBGF type member.
 */
typedef struct DBGFTYPEREGMEMBER
{
    /** Name of the member. */
    const char         *pszName;
    /** Flags for this member, see DBGFTYPEREGMEMBER_F_XXX. */
    uint32_t            fFlags;
    /** Type identifier. */
    const char         *pszType;
    /** The number of elements in the array, only valid for arrays. */
    uint32_t            cElements;
} DBGFTYPEREGMEMBER;
/** Pointer to a member. */
typedef DBGFTYPEREGMEMBER *PDBGFTYPEREGMEMBER;
/** Pointer to a const member. */
typedef const DBGFTYPEREGMEMBER *PCDBGFTYPEREGMEMBER;

/** @name DBGFTYPEREG Flags.
 * @{ */
/** The type is a packed structure. */
# define DBGFTYPEREG_F_PACKED        RT_BIT_32(0)
/** @} */

/**
 * New type registration structure.
 */
typedef struct DBGFTYPEREG
{
    /** Name of the type. */
    const char         *pszType;
    /** The type variant. */
    DBGFTYPEVARIANT     enmVariant;
    /** Some registration flags, see DBGFTYPEREG_F_XXX. */
    uint32_t            fFlags;
    /** Number of members this type has, only valid for structs or unions. */
    uint32_t            cMembers;
    /** Pointer to the member fields, only valid for structs or unions. */
    PCDBGFTYPEREGMEMBER paMembers;
    /** Name of the aliased type for aliases. */
    const char         *pszAliasedType;
} DBGFTYPEREG;

/**
 * DBGF typed value dumper callback.
 *
 * @returns VBox status code. Any non VINF_SUCCESS status code will abort the dumping.
 *
 * @param   off             The byte offset of the entry from the start of the type.
 * @param   pszField        The name of the field for the value.
 * @param   iLvl            The current level.
 * @param   enmType         The type enum.
 * @param   cbType          Size of the type.
 * @param   pValBuf         Pointer to the value buffer.
 * @param   cValBufs        Number of value buffers (for arrays).
 * @param   pvUser          Opaque user data.
 */
typedef DECLCALLBACK(int) FNDBGFR3TYPEVALDUMP(uint32_t off, const char *pszField, uint32_t iLvl,
                                              DBGFTYPEBUILTIN enmType, size_t cbType,
                                              PDBGFTYPEVALBUF pValBuf, uint32_t cValBufs,
                                              void *pvUser);
/** Pointer to a FNDBGFR3TYPEVALDUMP. */
typedef FNDBGFR3TYPEVALDUMP *PFNDBGFR3TYPEVALDUMP;

/**
 * DBGF type information dumper callback.
 *
 * @returns VBox status code. Any non VINF_SUCCESS status code will abort the dumping.
 *
 * @param   off             The byte offset of the entry from the start of the type.
 * @param   pszField        The name of the field for the value.
 * @param   iLvl            The current level.
 * @param   pszType         The type of the field.
 * @param   fTypeFlags      Flags for this type, see DBGFTYPEREGMEMBER_F_XXX.
 * @param   cElements       Number of for the field ( > 0 for arrays).
 * @param   pvUser          Opaque user data.
 */
typedef DECLCALLBACK(int) FNDBGFR3TYPEDUMP(uint32_t off, const char *pszField, uint32_t iLvl,
                                           const char *pszType, uint32_t fTypeFlags,
                                           uint32_t cElements, void *pvUser);
/** Pointer to a FNDBGFR3TYPEDUMP. */
typedef FNDBGFR3TYPEDUMP *PFNDBGFR3TYPEDUMP;

VMMR3DECL(int) DBGFR3TypeRegister(  PUVM pUVM, uint32_t cTypes, PCDBGFTYPEREG paTypes);
VMMR3DECL(int) DBGFR3TypeDeregister(PUVM pUVM, const char *pszType);
VMMR3DECL(int) DBGFR3TypeQueryReg(  PUVM pUVM, const char *pszType, PCDBGFTYPEREG *ppTypeReg);

VMMR3DECL(int) DBGFR3TypeQuerySize( PUVM pUVM, const char *pszType, size_t *pcbType);
VMMR3DECL(int) DBGFR3TypeSetSize(   PUVM pUVM, const char *pszType, size_t cbType);
VMMR3DECL(int) DBGFR3TypeDumpEx(    PUVM pUVM, const char *pszType, uint32_t fFlags,
                                    uint32_t cLvlMax, PFNDBGFR3TYPEDUMP pfnDump, void *pvUser);
VMMR3DECL(int) DBGFR3TypeQueryValByType(PUVM pUVM, PCDBGFADDRESS pAddress, const char *pszType,
                                        PDBGFTYPEVAL *ppVal);
VMMR3DECL(void) DBGFR3TypeValFree(PDBGFTYPEVAL pVal);
VMMR3DECL(int)  DBGFR3TypeValDumpEx(PUVM pUVM, PCDBGFADDRESS pAddress, const char *pszType, uint32_t fFlags,
                                    uint32_t cLvlMax, FNDBGFR3TYPEVALDUMP pfnDump, void *pvUser);

/** @} */


/** @defgroup grp_dbgf_flow       The DBGF control flow graph Interface.
 * @{
 */

/** A DBGF control flow graph handle. */
typedef struct DBGFFLOWINT *DBGFFLOW;
/** Pointer to a DBGF control flow graph handle. */
typedef DBGFFLOW *PDBGFFLOW;
/** A DBGF control flow graph basic block handle. */
typedef struct DBGFFLOWBBINT *DBGFFLOWBB;
/** Pointer to a DBGF control flow graph basic block handle. */
typedef DBGFFLOWBB *PDBGFFLOWBB;
/** A DBGF control flow graph branch table handle. */
typedef struct DBGFFLOWBRANCHTBLINT *DBGFFLOWBRANCHTBL;
/** Pointer to a DBGF flow control graph branch table handle. */
typedef DBGFFLOWBRANCHTBL *PDBGFFLOWBRANCHTBL;
/** A DBGF control flow graph iterator. */
typedef struct DBGFFLOWITINT *DBGFFLOWIT;
/** Pointer to a control flow graph iterator. */
typedef DBGFFLOWIT *PDBGFFLOWIT;
/** A DBGF control flow graph branch table iterator. */
typedef struct DBGFFLOWBRANCHTBLITINT *DBGFFLOWBRANCHTBLIT;
/** Pointer to a control flow graph branch table iterator. */
typedef DBGFFLOWBRANCHTBLIT *PDBGFFLOWBRANCHTBLIT;

/** @name DBGFFLOWBB Flags.
 * @{ */
/** The basic block is the entry into the owning control flow graph. */
#define DBGF_FLOW_BB_F_ENTRY             RT_BIT_32(0)
/** The basic block was not populated because the limit was reached. */
#define DBGF_FLOW_BB_F_EMPTY             RT_BIT_32(1)
/** The basic block is not complete because an error happened during disassembly. */
#define DBGF_FLOW_BB_F_INCOMPLETE_ERR    RT_BIT_32(2)
/** The basic block is reached through a branch table. */
#define DBGF_FLOW_BB_F_BRANCH_TABLE      RT_BIT_32(3)
/** @} */

/** @name Flags controlling the creating of a control flow graph.
 * @{ */
/** Default options. */
#define DBGF_FLOW_CREATE_F_DEFAULT                       0
/** Tries to resolve indirect branches, useful for code using
 * jump tables generated for large switch statements by some compilers. */
#define DBGF_FLOW_CREATE_F_TRY_RESOLVE_INDIRECT_BRANCHES RT_BIT_32(0)
/** @} */

/**
 * DBGF control graph basic block end type.
 */
typedef enum DBGFFLOWBBENDTYPE
{
    /** Invalid type. */
    DBGFFLOWBBENDTYPE_INVALID = 0,
    /** Basic block is the exit block and has no successor. */
    DBGFFLOWBBENDTYPE_EXIT,
    /** Basic block is the last disassembled block because the
     * maximum amount to disassemble was reached but is not an
     * exit block - no successors.
     */
    DBGFFLOWBBENDTYPE_LAST_DISASSEMBLED,
    /** Unconditional control flow change because the successor is referenced by multiple
     * basic blocks. - 1 successor. */
    DBGFFLOWBBENDTYPE_UNCOND,
    /** Unconditional control flow change because of an direct branch - 1 successor. */
    DBGFFLOWBBENDTYPE_UNCOND_JMP,
    /** Unconditional control flow change because of an indirect branch - n successors. */
    DBGFFLOWBBENDTYPE_UNCOND_INDIRECT_JMP,
    /** Conditional control flow change - 2 successors. */
    DBGFFLOWBBENDTYPE_COND,
    /** 32bit hack. */
    DBGFFLOWBBENDTYPE_32BIT_HACK = 0x7fffffff
} DBGFFLOWBBENDTYPE;

/**
 * DBGF control flow graph iteration order.
 */
typedef enum DBGFFLOWITORDER
{
    /** Invalid order. */
    DBGFFLOWITORDER_INVALID = 0,
    /** From lowest to highest basic block start address. */
    DBGFFLOWITORDER_BY_ADDR_LOWEST_FIRST,
    /** From highest to lowest basic block start address. */
    DBGFFLOWITORDER_BY_ADDR_HIGHEST_FIRST,
    /** Depth first traversing starting from the entry block. */
    DBGFFLOWITORDER_DEPTH_FRIST,
    /** Breadth first traversing starting from the entry block. */
    DBGFFLOWITORDER_BREADTH_FIRST,
    /** Usual 32bit hack. */
    DBGFFLOWITORDER_32BIT_HACK = 0x7fffffff
} DBGFFLOWITORDER;
/** Pointer to a iteration order enum. */
typedef DBGFFLOWITORDER *PDBGFFLOWITORDER;


VMMR3DECL(int)               DBGFR3FlowCreate(PUVM pUVM, VMCPUID idCpu, PDBGFADDRESS pAddressStart, uint32_t cbDisasmMax,
                                              uint32_t fFlagsFlow, uint32_t fFlagsDisasm, PDBGFFLOW phFlow);
VMMR3DECL(uint32_t)          DBGFR3FlowRetain(DBGFFLOW hFlow);
VMMR3DECL(uint32_t)          DBGFR3FlowRelease(DBGFFLOW hFlow);
VMMR3DECL(int)               DBGFR3FlowQueryStartBb(DBGFFLOW hFlow, PDBGFFLOWBB phFlowBb);
VMMR3DECL(int)               DBGFR3FlowQueryBbByAddress(DBGFFLOW hFlow, PDBGFADDRESS pAddr, PDBGFFLOWBB phFlowBb);
VMMR3DECL(int)               DBGFR3FlowQueryBranchTblByAddress(DBGFFLOW hFlow, PDBGFADDRESS pAddr, PDBGFFLOWBRANCHTBL phFlowBranchTbl);
VMMR3DECL(uint32_t)          DBGFR3FlowGetBbCount(DBGFFLOW hFlow);
VMMR3DECL(uint32_t)          DBGFR3FlowGetBranchTblCount(DBGFFLOW hFlow);

VMMR3DECL(uint32_t)          DBGFR3FlowBbRetain(DBGFFLOWBB hFlowBb);
VMMR3DECL(uint32_t)          DBGFR3FlowBbRelease(DBGFFLOWBB hFlowBb);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBbGetStartAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrStart);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBbGetEndAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrEnd);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBbGetBranchAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrTarget);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBbGetFollowingAddress(DBGFFLOWBB hFlowBb, PDBGFADDRESS pAddrFollow);
VMMR3DECL(DBGFFLOWBBENDTYPE) DBGFR3FlowBbGetType(DBGFFLOWBB hFlowBb);
VMMR3DECL(uint32_t)          DBGFR3FlowBbGetInstrCount(DBGFFLOWBB hFlowBb);
VMMR3DECL(uint32_t)          DBGFR3FlowBbGetFlags(DBGFFLOWBB hFlowBb);
VMMR3DECL(int)               DBGFR3FlowBbQueryBranchTbl(DBGFFLOWBB hFlowBb, PDBGFFLOWBRANCHTBL phBranchTbl);
VMMR3DECL(int)               DBGFR3FlowBbQueryError(DBGFFLOWBB hFlowBb, const char **ppszErr);
VMMR3DECL(int)               DBGFR3FlowBbQueryInstr(DBGFFLOWBB hFlowBb, uint32_t idxInstr, PDBGFADDRESS pAddrInstr,
                                                    uint32_t *pcbInstr, const char **ppszInstr);
VMMR3DECL(int)               DBGFR3FlowBbQuerySuccessors(DBGFFLOWBB hFlowBb, PDBGFFLOWBB phFlowBbFollow,
                                                         PDBGFFLOWBB phFlowBbTarget);
VMMR3DECL(uint32_t)          DBGFR3FlowBbGetRefBbCount(DBGFFLOWBB hFlowBb);
VMMR3DECL(int)               DBGFR3FlowBbGetRefBb(DBGFFLOWBB hFlowBb, PDBGFFLOWBB pahFlowBbRef, uint32_t cRef);

VMMR3DECL(uint32_t)          DBGFR3FlowBranchTblRetain(DBGFFLOWBRANCHTBL hFlowBranchTbl);
VMMR3DECL(uint32_t)          DBGFR3FlowBranchTblRelease(DBGFFLOWBRANCHTBL hFlowBranchTbl);
VMMR3DECL(uint32_t)          DBGFR3FlowBranchTblGetSlots(DBGFFLOWBRANCHTBL hFlowBranchTbl);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBranchTblGetStartAddress(DBGFFLOWBRANCHTBL hFlowBranchTbl, PDBGFADDRESS pAddrStart);
VMMR3DECL(PDBGFADDRESS)      DBGFR3FlowBranchTblGetAddrAtSlot(DBGFFLOWBRANCHTBL hFlowBranchTbl, uint32_t idxSlot, PDBGFADDRESS pAddrSlot);
VMMR3DECL(int)               DBGFR3FlowBranchTblQueryAddresses(DBGFFLOWBRANCHTBL hFlowBranchTbl, PDBGFADDRESS paAddrs, uint32_t cAddrs);

VMMR3DECL(int)               DBGFR3FlowItCreate(DBGFFLOW hFlow, DBGFFLOWITORDER enmOrder, PDBGFFLOWIT phFlowIt);
VMMR3DECL(void)              DBGFR3FlowItDestroy(DBGFFLOWIT hFlowIt);
VMMR3DECL(DBGFFLOWBB)        DBGFR3FlowItNext(DBGFFLOWIT hFlowIt);
VMMR3DECL(int)               DBGFR3FlowItReset(DBGFFLOWIT hFlowIt);

VMMR3DECL(int)               DBGFR3FlowBranchTblItCreate(DBGFFLOW hFlow, DBGFFLOWITORDER enmOrder, PDBGFFLOWBRANCHTBLIT phFlowBranchTblIt);
VMMR3DECL(void)              DBGFR3FlowBranchTblItDestroy(DBGFFLOWBRANCHTBLIT hFlowBranchTblIt);
VMMR3DECL(DBGFFLOWBRANCHTBL) DBGFR3FlowBranchTblItNext(DBGFFLOWBRANCHTBLIT hFlowBranchTblIt);
VMMR3DECL(int)               DBGFR3FlowBranchTblItReset(DBGFFLOWBRANCHTBLIT hFlowBranchTblIt);

/** @} */

#endif /* IN_RING3 */

/** @} */


RT_C_DECLS_END

#endif

