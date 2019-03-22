/* $Id$ */
/** @file
 * HM - Intel/AMD VM Hardware Support Manager.
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

/** @page pg_hm     HM - Hardware Assisted Virtualization Manager
 *
 * The HM manages guest execution using the VT-x and AMD-V CPU hardware
 * extensions.
 *
 * {summary of what HM does}
 *
 * Hardware assisted virtualization manager was originally abbreviated HWACCM,
 * however that was cumbersome to write and parse for such a central component,
 * so it was shortened to HM when refactoring the code in the 4.3 development
 * cycle.
 *
 * {add sections with more details}
 *
 * @sa @ref grp_hm
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#define VMCPU_INCL_CPUM_GST_CTX
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/iem.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/nem.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/hm_svm.h>
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/env.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#define EXIT_REASON(def, val, str) #def " - " #val " - " str
#define EXIT_REASON_NIL() NULL
/** Exit reason descriptions for VT-x, used to describe statistics and exit
 *  history. */
static const char * const g_apszVmxExitReasons[MAX_EXITREASON_STAT] =
{
    EXIT_REASON(VMX_EXIT_XCPT_OR_NMI            ,   0, "Exception or non-maskable interrupt (NMI)."),
    EXIT_REASON(VMX_EXIT_EXT_INT                ,   1, "External interrupt."),
    EXIT_REASON(VMX_EXIT_TRIPLE_FAULT           ,   2, "Triple fault."),
    EXIT_REASON(VMX_EXIT_INIT_SIGNAL            ,   3, "INIT signal."),
    EXIT_REASON(VMX_EXIT_SIPI                   ,   4, "Start-up IPI (SIPI)."),
    EXIT_REASON(VMX_EXIT_IO_SMI_IRQ             ,   5, "I/O system-management interrupt (SMI)."),
    EXIT_REASON(VMX_EXIT_SMI_IRQ                ,   6, "Other SMI."),
    EXIT_REASON(VMX_EXIT_INT_WINDOW             ,   7, "Interrupt window."),
    EXIT_REASON(VMX_EXIT_NMI_WINDOW             ,   8, "NMI window."),
    EXIT_REASON(VMX_EXIT_TASK_SWITCH            ,   9, "Task switch."),
    EXIT_REASON(VMX_EXIT_CPUID                  ,  10, "CPUID instruction."),
    EXIT_REASON(VMX_EXIT_GETSEC                 ,  11, "GETSEC instrunction."),
    EXIT_REASON(VMX_EXIT_HLT                    ,  12, "HLT instruction."),
    EXIT_REASON(VMX_EXIT_INVD                   ,  13, "INVD instruction."),
    EXIT_REASON(VMX_EXIT_INVLPG                 ,  14, "INVLPG instruction."),
    EXIT_REASON(VMX_EXIT_RDPMC                  ,  15, "RDPMCinstruction."),
    EXIT_REASON(VMX_EXIT_RDTSC                  ,  16, "RDTSC instruction."),
    EXIT_REASON(VMX_EXIT_RSM                    ,  17, "RSM instruction in SMM."),
    EXIT_REASON(VMX_EXIT_VMCALL                 ,  18, "VMCALL instruction."),
    EXIT_REASON(VMX_EXIT_VMCLEAR                ,  19, "VMCLEAR instruction."),
    EXIT_REASON(VMX_EXIT_VMLAUNCH               ,  20, "VMLAUNCH instruction."),
    EXIT_REASON(VMX_EXIT_VMPTRLD                ,  21, "VMPTRLD instruction."),
    EXIT_REASON(VMX_EXIT_VMPTRST                ,  22, "VMPTRST instruction."),
    EXIT_REASON(VMX_EXIT_VMREAD                 ,  23, "VMREAD instruction."),
    EXIT_REASON(VMX_EXIT_VMRESUME               ,  24, "VMRESUME instruction."),
    EXIT_REASON(VMX_EXIT_VMWRITE                ,  25, "VMWRITE instruction."),
    EXIT_REASON(VMX_EXIT_VMXOFF                 ,  26, "VMXOFF instruction."),
    EXIT_REASON(VMX_EXIT_VMXON                  ,  27, "VMXON instruction."),
    EXIT_REASON(VMX_EXIT_MOV_CRX                ,  28, "Control-register accesses."),
    EXIT_REASON(VMX_EXIT_MOV_DRX                ,  29, "Debug-register accesses."),
    EXIT_REASON(VMX_EXIT_PORT_IO                ,  30, "I/O instruction."),
    EXIT_REASON(VMX_EXIT_RDMSR                  ,  31, "RDMSR instruction."),
    EXIT_REASON(VMX_EXIT_WRMSR                  ,  32, "WRMSR instruction."),
    EXIT_REASON(VMX_EXIT_ERR_INVALID_GUEST_STATE,  33, "VM-entry failure due to invalid guest state."),
    EXIT_REASON(VMX_EXIT_ERR_MSR_LOAD           ,  34, "VM-entry failure due to MSR loading."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_MWAIT                  ,  36, "MWAIT instruction."),
    EXIT_REASON(VMX_EXIT_MTF                    ,  37, "Monitor Trap Flag."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_MONITOR                ,  39, "MONITOR instruction."),
    EXIT_REASON(VMX_EXIT_PAUSE                  ,  40, "PAUSE instruction."),
    EXIT_REASON(VMX_EXIT_ERR_MACHINE_CHECK      ,  41, "VM-entry failure due to machine-check."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_TPR_BELOW_THRESHOLD    ,  43, "TPR below threshold (MOV to CR8)."),
    EXIT_REASON(VMX_EXIT_APIC_ACCESS            ,  44, "APIC access."),
    EXIT_REASON(VMX_EXIT_VIRTUALIZED_EOI        ,  45, "Virtualized EOI."),
    EXIT_REASON(VMX_EXIT_GDTR_IDTR_ACCESS       ,  46, "GDTR/IDTR access using LGDT/SGDT/LIDT/SIDT."),
    EXIT_REASON(VMX_EXIT_LDTR_TR_ACCESS         ,  47, "LDTR/TR access using LLDT/SLDT/LTR/STR."),
    EXIT_REASON(VMX_EXIT_EPT_VIOLATION          ,  48, "EPT violation."),
    EXIT_REASON(VMX_EXIT_EPT_MISCONFIG          ,  49, "EPT misconfiguration."),
    EXIT_REASON(VMX_EXIT_INVEPT                 ,  50, "INVEPT instruction."),
    EXIT_REASON(VMX_EXIT_RDTSCP                 ,  51, "RDTSCP instruction."),
    EXIT_REASON(VMX_EXIT_PREEMPT_TIMER          ,  52, "VMX-preemption timer expired."),
    EXIT_REASON(VMX_EXIT_INVVPID                ,  53, "INVVPID instruction."),
    EXIT_REASON(VMX_EXIT_WBINVD                 ,  54, "WBINVD instruction."),
    EXIT_REASON(VMX_EXIT_XSETBV                 ,  55, "XSETBV instruction."),
    EXIT_REASON(VMX_EXIT_APIC_WRITE             ,  56, "APIC write completed to virtual-APIC page."),
    EXIT_REASON(VMX_EXIT_RDRAND                 ,  57, "RDRAND instruction."),
    EXIT_REASON(VMX_EXIT_INVPCID                ,  58, "INVPCID instruction."),
    EXIT_REASON(VMX_EXIT_VMFUNC                 ,  59, "VMFUNC instruction."),
    EXIT_REASON(VMX_EXIT_ENCLS                  ,  60, "ENCLS instruction."),
    EXIT_REASON(VMX_EXIT_RDSEED                 ,  61, "RDSEED instruction."),
    EXIT_REASON(VMX_EXIT_PML_FULL               ,  62, "Page-modification log full."),
    EXIT_REASON(VMX_EXIT_XSAVES                 ,  63, "XSAVES instruction."),
    EXIT_REASON(VMX_EXIT_XRSTORS                ,  64, "XRSTORS instruction.")
};
/** Array index of the last valid VT-x exit reason. */
#define MAX_EXITREASON_VTX                         64

/** A partial list of \#EXIT reason descriptions for AMD-V, used to describe
 *  statistics and exit history.
 *
 *  @note AMD-V have annoyingly large gaps (e.g. \#NPF VMEXIT comes at 1024),
 *        this array doesn't contain the entire set of exit reasons, we
 *        handle them via hmSvmGetSpecialExitReasonDesc(). */
static const char * const g_apszSvmExitReasons[MAX_EXITREASON_STAT] =
{
    EXIT_REASON(SVM_EXIT_READ_CR0     ,    0, "Read CR0."),
    EXIT_REASON(SVM_EXIT_READ_CR1     ,    1, "Read CR1."),
    EXIT_REASON(SVM_EXIT_READ_CR2     ,    2, "Read CR2."),
    EXIT_REASON(SVM_EXIT_READ_CR3     ,    3, "Read CR3."),
    EXIT_REASON(SVM_EXIT_READ_CR4     ,    4, "Read CR4."),
    EXIT_REASON(SVM_EXIT_READ_CR5     ,    5, "Read CR5."),
    EXIT_REASON(SVM_EXIT_READ_CR6     ,    6, "Read CR6."),
    EXIT_REASON(SVM_EXIT_READ_CR7     ,    7, "Read CR7."),
    EXIT_REASON(SVM_EXIT_READ_CR8     ,    8, "Read CR8."),
    EXIT_REASON(SVM_EXIT_READ_CR9     ,    9, "Read CR9."),
    EXIT_REASON(SVM_EXIT_READ_CR10    ,   10, "Read CR10."),
    EXIT_REASON(SVM_EXIT_READ_CR11    ,   11, "Read CR11."),
    EXIT_REASON(SVM_EXIT_READ_CR12    ,   12, "Read CR12."),
    EXIT_REASON(SVM_EXIT_READ_CR13    ,   13, "Read CR13."),
    EXIT_REASON(SVM_EXIT_READ_CR14    ,   14, "Read CR14."),
    EXIT_REASON(SVM_EXIT_READ_CR15    ,   15, "Read CR15."),
    EXIT_REASON(SVM_EXIT_WRITE_CR0    ,   16, "Write CR0."),
    EXIT_REASON(SVM_EXIT_WRITE_CR1    ,   17, "Write CR1."),
    EXIT_REASON(SVM_EXIT_WRITE_CR2    ,   18, "Write CR2."),
    EXIT_REASON(SVM_EXIT_WRITE_CR3    ,   19, "Write CR3."),
    EXIT_REASON(SVM_EXIT_WRITE_CR4    ,   20, "Write CR4."),
    EXIT_REASON(SVM_EXIT_WRITE_CR5    ,   21, "Write CR5."),
    EXIT_REASON(SVM_EXIT_WRITE_CR6    ,   22, "Write CR6."),
    EXIT_REASON(SVM_EXIT_WRITE_CR7    ,   23, "Write CR7."),
    EXIT_REASON(SVM_EXIT_WRITE_CR8    ,   24, "Write CR8."),
    EXIT_REASON(SVM_EXIT_WRITE_CR9    ,   25, "Write CR9."),
    EXIT_REASON(SVM_EXIT_WRITE_CR10   ,   26, "Write CR10."),
    EXIT_REASON(SVM_EXIT_WRITE_CR11   ,   27, "Write CR11."),
    EXIT_REASON(SVM_EXIT_WRITE_CR12   ,   28, "Write CR12."),
    EXIT_REASON(SVM_EXIT_WRITE_CR13   ,   29, "Write CR13."),
    EXIT_REASON(SVM_EXIT_WRITE_CR14   ,   30, "Write CR14."),
    EXIT_REASON(SVM_EXIT_WRITE_CR15   ,   31, "Write CR15."),
    EXIT_REASON(SVM_EXIT_READ_DR0     ,   32, "Read DR0."),
    EXIT_REASON(SVM_EXIT_READ_DR1     ,   33, "Read DR1."),
    EXIT_REASON(SVM_EXIT_READ_DR2     ,   34, "Read DR2."),
    EXIT_REASON(SVM_EXIT_READ_DR3     ,   35, "Read DR3."),
    EXIT_REASON(SVM_EXIT_READ_DR4     ,   36, "Read DR4."),
    EXIT_REASON(SVM_EXIT_READ_DR5     ,   37, "Read DR5."),
    EXIT_REASON(SVM_EXIT_READ_DR6     ,   38, "Read DR6."),
    EXIT_REASON(SVM_EXIT_READ_DR7     ,   39, "Read DR7."),
    EXIT_REASON(SVM_EXIT_READ_DR8     ,   40, "Read DR8."),
    EXIT_REASON(SVM_EXIT_READ_DR9     ,   41, "Read DR9."),
    EXIT_REASON(SVM_EXIT_READ_DR10    ,   42, "Read DR10."),
    EXIT_REASON(SVM_EXIT_READ_DR11    ,   43, "Read DR11"),
    EXIT_REASON(SVM_EXIT_READ_DR12    ,   44, "Read DR12."),
    EXIT_REASON(SVM_EXIT_READ_DR13    ,   45, "Read DR13."),
    EXIT_REASON(SVM_EXIT_READ_DR14    ,   46, "Read DR14."),
    EXIT_REASON(SVM_EXIT_READ_DR15    ,   47, "Read DR15."),
    EXIT_REASON(SVM_EXIT_WRITE_DR0    ,   48, "Write DR0."),
    EXIT_REASON(SVM_EXIT_WRITE_DR1    ,   49, "Write DR1."),
    EXIT_REASON(SVM_EXIT_WRITE_DR2    ,   50, "Write DR2."),
    EXIT_REASON(SVM_EXIT_WRITE_DR3    ,   51, "Write DR3."),
    EXIT_REASON(SVM_EXIT_WRITE_DR4    ,   52, "Write DR4."),
    EXIT_REASON(SVM_EXIT_WRITE_DR5    ,   53, "Write DR5."),
    EXIT_REASON(SVM_EXIT_WRITE_DR6    ,   54, "Write DR6."),
    EXIT_REASON(SVM_EXIT_WRITE_DR7    ,   55, "Write DR7."),
    EXIT_REASON(SVM_EXIT_WRITE_DR8    ,   56, "Write DR8."),
    EXIT_REASON(SVM_EXIT_WRITE_DR9    ,   57, "Write DR9."),
    EXIT_REASON(SVM_EXIT_WRITE_DR10   ,   58, "Write DR10."),
    EXIT_REASON(SVM_EXIT_WRITE_DR11   ,   59, "Write DR11."),
    EXIT_REASON(SVM_EXIT_WRITE_DR12   ,   60, "Write DR12."),
    EXIT_REASON(SVM_EXIT_WRITE_DR13   ,   61, "Write DR13."),
    EXIT_REASON(SVM_EXIT_WRITE_DR14   ,   62, "Write DR14."),
    EXIT_REASON(SVM_EXIT_WRITE_DR15   ,   63, "Write DR15."),
    EXIT_REASON(SVM_EXIT_XCPT_0       ,   64, "Exception 0  (#DE)."),
    EXIT_REASON(SVM_EXIT_XCPT_1       ,   65, "Exception 1  (#DB)."),
    EXIT_REASON(SVM_EXIT_XCPT_2       ,   66, "Exception 2  (#NMI)."),
    EXIT_REASON(SVM_EXIT_XCPT_3       ,   67, "Exception 3  (#BP)."),
    EXIT_REASON(SVM_EXIT_XCPT_4       ,   68, "Exception 4  (#OF)."),
    EXIT_REASON(SVM_EXIT_XCPT_5       ,   69, "Exception 5  (#BR)."),
    EXIT_REASON(SVM_EXIT_XCPT_6       ,   70, "Exception 6  (#UD)."),
    EXIT_REASON(SVM_EXIT_XCPT_7       ,   71, "Exception 7  (#NM)."),
    EXIT_REASON(SVM_EXIT_XCPT_8       ,   72, "Exception 8  (#DF)."),
    EXIT_REASON(SVM_EXIT_XCPT_9       ,   73, "Exception 9  (#CO_SEG_OVERRUN)."),
    EXIT_REASON(SVM_EXIT_XCPT_10      ,   74, "Exception 10 (#TS)."),
    EXIT_REASON(SVM_EXIT_XCPT_11      ,   75, "Exception 11 (#NP)."),
    EXIT_REASON(SVM_EXIT_XCPT_12      ,   76, "Exception 12 (#SS)."),
    EXIT_REASON(SVM_EXIT_XCPT_13      ,   77, "Exception 13 (#GP)."),
    EXIT_REASON(SVM_EXIT_XCPT_14      ,   78, "Exception 14 (#PF)."),
    EXIT_REASON(SVM_EXIT_XCPT_15      ,   79, "Exception 15 (0x0f)."),
    EXIT_REASON(SVM_EXIT_XCPT_16      ,   80, "Exception 16 (#MF)."),
    EXIT_REASON(SVM_EXIT_XCPT_17      ,   81, "Exception 17 (#AC)."),
    EXIT_REASON(SVM_EXIT_XCPT_18      ,   82, "Exception 18 (#MC)."),
    EXIT_REASON(SVM_EXIT_XCPT_19      ,   83, "Exception 19 (#XF)."),
    EXIT_REASON(SVM_EXIT_XCPT_20      ,   84, "Exception 20 (#VE)."),
    EXIT_REASON(SVM_EXIT_XCPT_21      ,   85, "Exception 22 (0x15)."),
    EXIT_REASON(SVM_EXIT_XCPT_22      ,   86, "Exception 22 (0x16)."),
    EXIT_REASON(SVM_EXIT_XCPT_23      ,   87, "Exception 23 (0x17)."),
    EXIT_REASON(SVM_EXIT_XCPT_24      ,   88, "Exception 24 (0x18)."),
    EXIT_REASON(SVM_EXIT_XCPT_25      ,   89, "Exception 25 (0x19)."),
    EXIT_REASON(SVM_EXIT_XCPT_26      ,   90, "Exception 26 (0x1a)."),
    EXIT_REASON(SVM_EXIT_XCPT_27      ,   91, "Exception 27 (0x1b)."),
    EXIT_REASON(SVM_EXIT_XCPT_28      ,   92, "Exception 28 (0x1c)."),
    EXIT_REASON(SVM_EXIT_XCPT_29      ,   93, "Exception 29 (0x1d)."),
    EXIT_REASON(SVM_EXIT_XCPT_30      ,   94, "Exception 30 (#SX)."),
    EXIT_REASON(SVM_EXIT_XCPT_31      ,   95, "Exception 31 (0x1F)."),
    EXIT_REASON(SVM_EXIT_INTR         ,   96, "Physical maskable interrupt (host)."),
    EXIT_REASON(SVM_EXIT_NMI          ,   97, "Physical non-maskable interrupt (host)."),
    EXIT_REASON(SVM_EXIT_SMI          ,   98, "System management interrupt (host)."),
    EXIT_REASON(SVM_EXIT_INIT         ,   99, "Physical INIT signal (host)."),
    EXIT_REASON(SVM_EXIT_VINTR        ,  100, "Virtual interrupt-window exit."),
    EXIT_REASON(SVM_EXIT_CR0_SEL_WRITE,  101, "Selective CR0 Write (to bits other than CR0.TS and CR0.MP)."),
    EXIT_REASON(SVM_EXIT_IDTR_READ    ,  102, "Read IDTR."),
    EXIT_REASON(SVM_EXIT_GDTR_READ    ,  103, "Read GDTR."),
    EXIT_REASON(SVM_EXIT_LDTR_READ    ,  104, "Read LDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ      ,  105, "Read TR."),
    EXIT_REASON(SVM_EXIT_IDTR_WRITE   ,  106, "Write IDTR."),
    EXIT_REASON(SVM_EXIT_GDTR_WRITE   ,  107, "Write GDTR."),
    EXIT_REASON(SVM_EXIT_LDTR_WRITE   ,  108, "Write LDTR."),
    EXIT_REASON(SVM_EXIT_TR_WRITE     ,  109, "Write TR."),
    EXIT_REASON(SVM_EXIT_RDTSC        ,  110, "RDTSC instruction."),
    EXIT_REASON(SVM_EXIT_RDPMC        ,  111, "RDPMC instruction."),
    EXIT_REASON(SVM_EXIT_PUSHF        ,  112, "PUSHF instruction."),
    EXIT_REASON(SVM_EXIT_POPF         ,  113, "POPF instruction."),
    EXIT_REASON(SVM_EXIT_CPUID        ,  114, "CPUID instruction."),
    EXIT_REASON(SVM_EXIT_RSM          ,  115, "RSM instruction."),
    EXIT_REASON(SVM_EXIT_IRET         ,  116, "IRET instruction."),
    EXIT_REASON(SVM_EXIT_SWINT        ,  117, "Software interrupt (INTn instructions)."),
    EXIT_REASON(SVM_EXIT_INVD         ,  118, "INVD instruction."),
    EXIT_REASON(SVM_EXIT_PAUSE        ,  119, "PAUSE instruction."),
    EXIT_REASON(SVM_EXIT_HLT          ,  120, "HLT instruction."),
    EXIT_REASON(SVM_EXIT_INVLPG       ,  121, "INVLPG instruction."),
    EXIT_REASON(SVM_EXIT_INVLPGA      ,  122, "INVLPGA instruction."),
    EXIT_REASON(SVM_EXIT_IOIO         ,  123, "IN/OUT/INS/OUTS instruction."),
    EXIT_REASON(SVM_EXIT_MSR          ,  124, "RDMSR or WRMSR access to protected MSR."),
    EXIT_REASON(SVM_EXIT_TASK_SWITCH  ,  125, "Task switch."),
    EXIT_REASON(SVM_EXIT_FERR_FREEZE  ,  126, "FERR Freeze; CPU frozen in an x87/mmx instruction waiting for interrupt."),
    EXIT_REASON(SVM_EXIT_SHUTDOWN     ,  127, "Shutdown."),
    EXIT_REASON(SVM_EXIT_VMRUN        ,  128, "VMRUN instruction."),
    EXIT_REASON(SVM_EXIT_VMMCALL      ,  129, "VMCALL instruction."),
    EXIT_REASON(SVM_EXIT_VMLOAD       ,  130, "VMLOAD instruction."),
    EXIT_REASON(SVM_EXIT_VMSAVE       ,  131, "VMSAVE instruction."),
    EXIT_REASON(SVM_EXIT_STGI         ,  132, "STGI instruction."),
    EXIT_REASON(SVM_EXIT_CLGI         ,  133, "CLGI instruction."),
    EXIT_REASON(SVM_EXIT_SKINIT       ,  134, "SKINIT instruction."),
    EXIT_REASON(SVM_EXIT_RDTSCP       ,  135, "RDTSCP instruction."),
    EXIT_REASON(SVM_EXIT_ICEBP        ,  136, "ICEBP instruction."),
    EXIT_REASON(SVM_EXIT_WBINVD       ,  137, "WBINVD instruction."),
    EXIT_REASON(SVM_EXIT_MONITOR      ,  138, "MONITOR instruction."),
    EXIT_REASON(SVM_EXIT_MWAIT        ,  139, "MWAIT instruction."),
    EXIT_REASON(SVM_EXIT_MWAIT_ARMED  ,  140, "MWAIT instruction when armed."),
    EXIT_REASON(SVM_EXIT_XSETBV       ,  141, "XSETBV instruction."),
};
/** Array index of the last valid AMD-V exit reason. */
#define MAX_EXITREASON_AMDV              141

/** Special exit reasons not covered in the array above. */
#define SVM_EXIT_REASON_NPF                  EXIT_REASON(SVM_EXIT_NPF                , 1024, "Nested Page Fault.")
#define SVM_EXIT_REASON_AVIC_INCOMPLETE_IPI  EXIT_REASON(SVM_EXIT_AVIC_INCOMPLETE_IPI, 1025, "AVIC - Incomplete IPI delivery.")
#define SVM_EXIT_REASON_AVIC_NOACCEL         EXIT_REASON(SVM_EXIT_AVIC_NOACCEL       , 1026, "AVIC - Unhandled register.")

/**
 * Gets the SVM exit reason if it's one of the reasons not present in the @c
 * g_apszSvmExitReasons array.
 *
 * @returns The exit reason or NULL if unknown.
 * @param   uExit       The exit.
 */
DECLINLINE(const char *) hmSvmGetSpecialExitReasonDesc(uint16_t uExit)
{
    switch (uExit)
    {
        case SVM_EXIT_NPF:                 return SVM_EXIT_REASON_NPF;
        case SVM_EXIT_AVIC_INCOMPLETE_IPI: return SVM_EXIT_REASON_AVIC_INCOMPLETE_IPI;
        case SVM_EXIT_AVIC_NOACCEL:        return SVM_EXIT_REASON_AVIC_NOACCEL;
    }
    return EXIT_REASON_NIL();
}
#undef EXIT_REASON_NIL
#undef EXIT_REASON

/** @def HMVMX_REPORT_FEAT
 * Reports VT-x feature to the release log.
 *
 * @param   allowed1        Mask of allowed feature bits.
 * @param   disallowed0     Mask of disallowed feature bits.
 * @param   strdesc         The description string to report.
 * @param   featflag        Mask of the feature to report.
 */
#define HMVMX_REPORT_FEAT(allowed1, disallowed0, strdesc, featflag) \
    do { \
        if ((allowed1) & (featflag)) \
        { \
            if ((disallowed0) & (featflag)) \
                LogRel(("HM:   " strdesc " (must be set)\n")); \
            else \
                LogRel(("HM:   " strdesc "\n")); \
        } \
        else \
            LogRel(("HM:   " strdesc " (must be cleared)\n")); \
    } while (0)

/** @def HMVMX_REPORT_ALLOWED_FEAT
 * Reports an allowed VT-x feature to the release log.
 *
 * @param   allowed1        Mask of allowed feature bits.
 * @param   strdesc         The description string to report.
 * @param   featflag        Mask of the feature to report.
 */
#define HMVMX_REPORT_ALLOWED_FEAT(allowed1, strdesc, featflag) \
    do { \
        if ((allowed1) & (featflag)) \
            LogRel(("HM:   " strdesc "\n")); \
        else \
            LogRel(("HM:   " strdesc " not supported\n")); \
    } while (0)

/** @def HMVMX_REPORT_MSR_CAP
 * Reports MSR feature capability.
 *
 * @param   msrcaps         Mask of MSR feature bits.
 * @param   strdesc         The description string to report.
 * @param   cap             Mask of the feature to report.
 */
#define HMVMX_REPORT_MSR_CAP(msrcaps, strdesc, cap) \
    do { \
        if ((msrcaps) & (cap)) \
            LogRel(("HM:   " strdesc "\n")); \
    } while (0)

/** @def HMVMX_LOGREL_FEAT
 * Dumps a feature flag from a bitmap of features to the release log.
 *
 * @param   a_fVal         The value of all the features.
 * @param   a_fMask        The specific bitmask of the feature.
 */
#define HMVMX_LOGREL_FEAT(a_fVal, a_fMask) \
    do { \
        if ((a_fVal) & (a_fMask)) \
            LogRel(("HM:   %s\n",  #a_fMask)); \
    } while (0)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)  hmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  hmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(void) hmR3InfoSvmNstGstVmcbCache(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) hmR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) hmR3InfoEventPending(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static int                hmR3InitCPU(PVM pVM);
static int                hmR3InitFinalizeR0(PVM pVM);
static int                hmR3InitFinalizeR0Intel(PVM pVM);
static int                hmR3InitFinalizeR0Amd(PVM pVM);
static int                hmR3TermCPU(PVM pVM);



/**
 * Initializes the HM.
 *
 * This is the very first component to really do init after CFGM so that we can
 * establish the predominat execution engine for the VM prior to initializing
 * other modules.  It takes care of NEM initialization if needed (HM disabled or
 * not available in HW).
 *
 * If VT-x or AMD-V hardware isn't available, HM will try fall back on a native
 * hypervisor API via NEM, and then back on raw-mode if that isn't available
 * either.  The fallback to raw-mode will not happen if /HM/HMForced is set
 * (like for guest using SMP or 64-bit as well as for complicated guest like OS
 * X, OS/2 and others).
 *
 * Note that a lot of the set up work is done in ring-0 and thus postponed till
 * the ring-3 and ring-0 callback to HMR3InitCompleted.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 *
 * @remarks Be careful with what we call here, since most of the VMM components
 *          are uninitialized.
 */
VMMR3_INT_DECL(int) HMR3Init(PVM pVM)
{
    LogFlow(("HMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, hm.s, 32);
    AssertCompile(sizeof(pVM->hm.s) <= sizeof(pVM->hm.padding));

    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "HWACCM", 0, HM_SAVED_STATE_VERSION, sizeof(HM),
                                   NULL, NULL, NULL,
                                   NULL, hmR3Save, NULL,
                                   NULL, hmR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register info handlers.
     */
    rc = DBGFR3InfoRegisterInternalEx(pVM, "hm", "Dumps HM info.", hmR3Info, DBGFINFO_FLAGS_ALL_EMTS);
    AssertRCReturn(rc, rc);

    rc = DBGFR3InfoRegisterInternalEx(pVM, "hmeventpending", "Dumps the pending HM event.", hmR3InfoEventPending,
                                      DBGFINFO_FLAGS_ALL_EMTS);
    AssertRCReturn(rc, rc);

    rc = DBGFR3InfoRegisterInternalEx(pVM, "svmvmcbcache", "Dumps the HM SVM nested-guest VMCB cache.",
                                      hmR3InfoSvmNstGstVmcbCache, DBGFINFO_FLAGS_ALL_EMTS);
    AssertRCReturn(rc, rc);

    /*
     * Read configuration.
     */
    PCFGMNODE pCfgHm = CFGMR3GetChild(CFGMR3GetRoot(pVM), "HM/");

    /*
     * Validate the HM settings.
     */
    rc = CFGMR3ValidateConfig(pCfgHm, "/HM/",
                              "HMForced"
                              "|UseNEMInstead"
                              "|FallbackToNEM"
                              "|EnableNestedPaging"
                              "|EnableUX"
                              "|EnableLargePages"
                              "|EnableVPID"
                              "|IBPBOnVMExit"
                              "|IBPBOnVMEntry"
                              "|SpecCtrlByHost"
                              "|TPRPatchingEnabled"
                              "|64bitEnabled"
                              "|Exclusive"
                              "|MaxResumeLoops"
                              "|VmxPleGap"
                              "|VmxPleWindow"
                              "|UseVmxPreemptTimer"
                              "|SvmPauseFilter"
                              "|SvmPauseFilterThreshold"
                              "|SvmVirtVmsaveVmload"
                              "|SvmVGif",
                              "" /* pszValidNodes */, "HM" /* pszWho */, 0 /* uInstance */);
    if (RT_FAILURE(rc))
        return rc;

    /** @cfgm{/HM/HMForced, bool, false}
     * Forces hardware virtualization, no falling back on raw-mode. HM must be
     * enabled, i.e. /HMEnabled must be true. */
    bool fHMForced;
#ifdef VBOX_WITH_RAW_MODE
    rc = CFGMR3QueryBoolDef(pCfgHm, "HMForced", &fHMForced, false);
    AssertRCReturn(rc, rc);
    AssertLogRelMsgReturn(!fHMForced || pVM->fHMEnabled, ("Configuration error: HM forced but not enabled!\n"),
                          VERR_INVALID_PARAMETER);
# if defined(RT_OS_DARWIN)
    if (pVM->fHMEnabled)
        fHMForced = true;
# endif
    AssertLogRelMsgReturn(pVM->cCpus == 1 || pVM->fHMEnabled, ("Configuration error: SMP requires HM to be enabled!\n"),
                          VERR_INVALID_PARAMETER);
    if (pVM->cCpus > 1)
        fHMForced = true;
#else  /* !VBOX_WITH_RAW_MODE */
    AssertRelease(pVM->fHMEnabled);
    fHMForced = true;
#endif /* !VBOX_WITH_RAW_MODE */

    /** @cfgm{/HM/UseNEMInstead, bool, true}
     * Don't use HM, use NEM instead. */
    bool fUseNEMInstead = false;
    rc = CFGMR3QueryBoolDef(pCfgHm, "UseNEMInstead", &fUseNEMInstead, false);
    AssertRCReturn(rc, rc);
    if (fUseNEMInstead && pVM->fHMEnabled)
    {
        LogRel(("HM: Setting fHMEnabled to false because fUseNEMInstead is set.\n"));
        pVM->fHMEnabled = false;
    }

    /** @cfgm{/HM/FallbackToNEM, bool, true}
     * Enables fallback on NEM. */
    bool fFallbackToNEM = true;
    rc = CFGMR3QueryBoolDef(pCfgHm, "FallbackToNEM", &fFallbackToNEM, true);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/EnableNestedPaging, bool, false}
     * Enables nested paging (aka extended page tables). */
    rc = CFGMR3QueryBoolDef(pCfgHm, "EnableNestedPaging", &pVM->hm.s.fAllowNestedPaging, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/EnableUX, bool, true}
     * Enables the VT-x unrestricted execution feature. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "EnableUX", &pVM->hm.s.vmx.fAllowUnrestricted, true);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/EnableLargePages, bool, false}
     * Enables using large pages (2 MB) for guest memory, thus saving on (nested)
     * page table walking and maybe better TLB hit rate in some cases. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "EnableLargePages", &pVM->hm.s.fLargePages, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/EnableVPID, bool, false}
     * Enables the VT-x VPID feature. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "EnableVPID", &pVM->hm.s.vmx.fAllowVpid, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/TPRPatchingEnabled, bool, false}
     * Enables TPR patching for 32-bit windows guests with IO-APIC. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "TPRPatchingEnabled", &pVM->hm.s.fTprPatchingAllowed, false);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/64bitEnabled, bool, 32-bit:false, 64-bit:true}
     * Enables AMD64 cpu features.
     * On 32-bit hosts this isn't default and require host CPU support. 64-bit hosts
     * already have the support. */
#ifdef VBOX_ENABLE_64_BITS_GUESTS
    rc = CFGMR3QueryBoolDef(pCfgHm, "64bitEnabled", &pVM->hm.s.fAllow64BitGuests, HC_ARCH_BITS == 64);
    AssertLogRelRCReturn(rc, rc);
#else
    pVM->hm.s.fAllow64BitGuests = false;
#endif

    /** @cfgm{/HM/VmxPleGap, uint32_t, 0}
     * The pause-filter exiting gap in TSC ticks. When the number of ticks between
     * two successive PAUSE instructions exceeds VmxPleGap, the CPU considers the
     * latest PAUSE instruction to be start of a new PAUSE loop.
     */
    rc = CFGMR3QueryU32Def(pCfgHm, "VmxPleGap", &pVM->hm.s.vmx.cPleGapTicks, 0);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/VmxPleWindow, uint32_t, 0}
     * The pause-filter exiting window in TSC ticks. When the number of ticks
     * between the current PAUSE instruction and first PAUSE of a loop exceeds
     * VmxPleWindow, a VM-exit is triggered.
     *
     * Setting VmxPleGap and VmxPleGap to 0 disables pause-filter exiting.
     */
    rc = CFGMR3QueryU32Def(pCfgHm, "VmxPleWindow", &pVM->hm.s.vmx.cPleWindowTicks, 0);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/SvmPauseFilterCount, uint16_t, 0}
     * A counter that is decrement each time a PAUSE instruction is executed by the
     * guest. When the counter is 0, a \#VMEXIT is triggered.
     *
     * Setting SvmPauseFilterCount to 0 disables pause-filter exiting.
     */
    rc = CFGMR3QueryU16Def(pCfgHm, "SvmPauseFilter", &pVM->hm.s.svm.cPauseFilter, 0);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/SvmPauseFilterThreshold, uint16_t, 0}
     * The pause filter threshold in ticks. When the elapsed time (in ticks) between
     * two successive PAUSE instructions exceeds SvmPauseFilterThreshold, the
     * PauseFilter count is reset to its initial value. However, if PAUSE is
     * executed PauseFilter times within PauseFilterThreshold ticks, a VM-exit will
     * be triggered.
     *
     * Requires SvmPauseFilterCount to be non-zero for pause-filter threshold to be
     * activated.
     */
    rc = CFGMR3QueryU16Def(pCfgHm, "SvmPauseFilterThreshold", &pVM->hm.s.svm.cPauseFilterThresholdTicks, 0);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/SvmVirtVmsaveVmload, bool, true}
     * Whether to make use of virtualized VMSAVE/VMLOAD feature of the CPU if it's
     * available. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "SvmVirtVmsaveVmload", &pVM->hm.s.svm.fVirtVmsaveVmload, true);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/SvmVGif, bool, true}
     * Whether to make use of Virtual GIF (Global Interrupt Flag) feature of the CPU
     * if it's available. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "SvmVGif", &pVM->hm.s.svm.fVGif, true);
    AssertRCReturn(rc, rc);

    /** @cfgm{/HM/Exclusive, bool}
     * Determines the init method for AMD-V and VT-x. If set to true, HM will do a
     * global init for each host CPU.  If false, we do local init each time we wish
     * to execute guest code.
     *
     * On Windows, default is false due to the higher risk of conflicts with other
     * hypervisors.
     *
     * On Mac OS X, this setting is ignored since the code does not handle local
     * init when it utilizes the OS provided VT-x function, SUPR0EnableVTx().
     */
#if defined(RT_OS_DARWIN)
    pVM->hm.s.fGlobalInit = true;
#else
    rc = CFGMR3QueryBoolDef(pCfgHm, "Exclusive", &pVM->hm.s.fGlobalInit,
# if defined(RT_OS_WINDOWS)
                            false
# else
                            true
# endif
                           );
    AssertLogRelRCReturn(rc, rc);
#endif

    /** @cfgm{/HM/MaxResumeLoops, uint32_t}
     * The number of times to resume guest execution before we forcibly return to
     * ring-3.  The return value of RTThreadPreemptIsPendingTrusty in ring-0
     * determines the default value. */
    rc = CFGMR3QueryU32Def(pCfgHm, "MaxResumeLoops", &pVM->hm.s.cMaxResumeLoops, 0 /* set by R0 later */);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/HM/UseVmxPreemptTimer, bool}
     * Whether to make use of the VMX-preemption timer feature of the CPU if it's
     * available. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "UseVmxPreemptTimer", &pVM->hm.s.vmx.fUsePreemptTimer, true);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/HM/IBPBOnVMExit, bool}
     * Costly paranoia setting. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "IBPBOnVMExit", &pVM->hm.s.fIbpbOnVmExit, false);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/HM/IBPBOnVMEntry, bool}
     * Costly paranoia setting. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "IBPBOnVMEntry", &pVM->hm.s.fIbpbOnVmEntry, false);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/HM/SpecCtrlByHost, bool}
     * Another expensive paranoia setting. */
    rc = CFGMR3QueryBoolDef(pCfgHm, "SpecCtrlByHost", &pVM->hm.s.fSpecCtrlByHost, false);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Check if VT-x or AMD-v support according to the users wishes.
     */
    /** @todo SUPR3QueryVTCaps won't catch VERR_VMX_IN_VMX_ROOT_MODE or
     *        VERR_SVM_IN_USE. */
    if (pVM->fHMEnabled)
    {
        uint32_t fCaps;
        rc = SUPR3QueryVTCaps(&fCaps);
        if (RT_SUCCESS(rc))
        {
            if (fCaps & SUPVTCAPS_AMD_V)
            {
                LogRel(("HM: HMR3Init: AMD-V%s\n", fCaps & SUPVTCAPS_NESTED_PAGING ? " w/ nested paging" : ""));
                pVM->hm.s.svm.fSupported = true;
                VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_HW_VIRT);
            }
            else if (fCaps & SUPVTCAPS_VT_X)
            {
                const char *pszWhy;
                rc = SUPR3QueryVTxSupported(&pszWhy);
                if (RT_SUCCESS(rc))
                {
                    LogRel(("HM: HMR3Init: VT-x%s%s%s\n",
                            fCaps & SUPVTCAPS_NESTED_PAGING ? " w/ nested paging" : "",
                            fCaps & SUPVTCAPS_VTX_UNRESTRICTED_GUEST ? " and unrestricted guest execution" : "",
                            (fCaps & (SUPVTCAPS_NESTED_PAGING | SUPVTCAPS_VTX_UNRESTRICTED_GUEST)) ? " hw support" : ""));
                    pVM->hm.s.vmx.fSupported = true;
                    VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_HW_VIRT);
                }
                else
                {
                    /*
                     * Before failing, try fallback to NEM if we're allowed to do that.
                     */
                    pVM->fHMEnabled = false;
                    Assert(pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NOT_SET);
                    if (fFallbackToNEM)
                    {
                        LogRel(("HM: HMR3Init: Attempting fall back to NEM: The host kernel does not support VT-x - %s\n", pszWhy));
                        int rc2 = NEMR3Init(pVM, true /*fFallback*/, fHMForced);

                        ASMCompilerBarrier(); /* NEMR3Init may have changed bMainExecutionEngine. */
                        if (   RT_SUCCESS(rc2)
                            && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NOT_SET)
                            rc = VINF_SUCCESS;
                    }
                    if (RT_FAILURE(rc))
                    {
                        if (fHMForced)
                            return VMSetError(pVM, rc, RT_SRC_POS, "The host kernel does not support VT-x: %s\n", pszWhy);

                        /* Fall back to raw-mode. */
                        LogRel(("HM: HMR3Init: Falling back to raw-mode: The host kernel does not support VT-x - %s\n", pszWhy));
                        VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_RAW_MODE);
                    }
                }
            }
            else
                AssertLogRelMsgFailedReturn(("SUPR3QueryVTCaps didn't return either AMD-V or VT-x flag set (%#x)!\n", fCaps),
                                            VERR_INTERNAL_ERROR_5);

            /*
             * Do we require a little bit or raw-mode for 64-bit guest execution?
             */
            pVM->fHMNeedRawModeCtx = HC_ARCH_BITS == 32
                                  && pVM->fHMEnabled
                                  && pVM->hm.s.fAllow64BitGuests;

            /*
             * Disable nested paging and unrestricted guest execution now if they're
             * configured so that CPUM can make decisions based on our configuration.
             */
            Assert(!pVM->hm.s.fNestedPaging);
            if (pVM->hm.s.fAllowNestedPaging)
            {
                if (fCaps & SUPVTCAPS_NESTED_PAGING)
                    pVM->hm.s.fNestedPaging = true;
                else
                    pVM->hm.s.fAllowNestedPaging = false;
            }

            if (fCaps & SUPVTCAPS_VT_X)
            {
                Assert(!pVM->hm.s.vmx.fUnrestrictedGuest);
                if (pVM->hm.s.vmx.fAllowUnrestricted)
                {
                    if (   (fCaps & SUPVTCAPS_VTX_UNRESTRICTED_GUEST)
                        && pVM->hm.s.fNestedPaging)
                        pVM->hm.s.vmx.fUnrestrictedGuest = true;
                    else
                        pVM->hm.s.vmx.fAllowUnrestricted = false;
                }
            }
        }
        else
        {
            const char *pszMsg;
            switch (rc)
            {
                case VERR_UNSUPPORTED_CPU:          pszMsg = "Unknown CPU, VT-x or AMD-v features cannot be ascertained"; break;
                case VERR_VMX_NO_VMX:               pszMsg = "VT-x is not available"; break;
                case VERR_VMX_MSR_VMX_DISABLED:     pszMsg = "VT-x is disabled in the BIOS"; break;
                case VERR_VMX_MSR_ALL_VMX_DISABLED: pszMsg = "VT-x is disabled in the BIOS for all CPU modes"; break;
                case VERR_VMX_MSR_LOCKING_FAILED:   pszMsg = "Failed to enable and lock VT-x features"; break;
                case VERR_SVM_NO_SVM:               pszMsg = "AMD-V is not available"; break;
                case VERR_SVM_DISABLED:             pszMsg = "AMD-V is disabled in the BIOS (or by the host OS)"; break;
                default:
                    return VMSetError(pVM, rc, RT_SRC_POS, "SUPR3QueryVTCaps failed with %Rrc", rc);
            }

            /*
             * Before failing, try fallback to NEM if we're allowed to do that.
             */
            pVM->fHMEnabled = false;
            if (fFallbackToNEM)
            {
                LogRel(("HM: HMR3Init: Attempting fall back to NEM: %s\n", pszMsg));
                int rc2 = NEMR3Init(pVM, true /*fFallback*/, fHMForced);
                ASMCompilerBarrier(); /* NEMR3Init may have changed bMainExecutionEngine. */
                if (   RT_SUCCESS(rc2)
                    && pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NOT_SET)
                    rc = VINF_SUCCESS;
            }
            if (RT_FAILURE(rc))
            {
                if (fHMForced)
                    return VM_SET_ERROR(pVM, rc, pszMsg);

                LogRel(("HM: HMR3Init: Falling back to raw-mode: %s\n", pszMsg));
                VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_RAW_MODE);
            }
        }
    }
    else
    {
        /*
         * Disabled HM mean raw-mode, unless NEM is supposed to be used.
         */
        if (!fUseNEMInstead)
            VM_SET_MAIN_EXECUTION_ENGINE(pVM, VM_EXEC_ENGINE_RAW_MODE);
        else
        {
            rc = NEMR3Init(pVM, false /*fFallback*/, true);
            ASMCompilerBarrier(); /* NEMR3Init may have changed bMainExecutionEngine. */
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Initializes the per-VCPU HM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int hmR3InitCPU(PVM pVM)
{
    LogFlow(("HMR3InitCPU\n"));

    if (!HMIsEnabled(pVM))
        return VINF_SUCCESS;

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->hm.s.fActive = false;
    }

#ifdef VBOX_WITH_STATISTICS
    STAM_REG(pVM, &pVM->hm.s.StatTprPatchSuccess,   STAMTYPE_COUNTER, "/HM/TPR/Patch/Success",  STAMUNIT_OCCURENCES, "Number of times an instruction was successfully patched.");
    STAM_REG(pVM, &pVM->hm.s.StatTprPatchFailure,   STAMTYPE_COUNTER, "/HM/TPR/Patch/Failed",   STAMUNIT_OCCURENCES, "Number of unsuccessful patch attempts.");
    STAM_REG(pVM, &pVM->hm.s.StatTprReplaceSuccessCr8, STAMTYPE_COUNTER, "/HM/TPR/Replace/SuccessCR8",STAMUNIT_OCCURENCES, "Number of instruction replacements by MOV CR8.");
    STAM_REG(pVM, &pVM->hm.s.StatTprReplaceSuccessVmc, STAMTYPE_COUNTER, "/HM/TPR/Replace/SuccessVMC",STAMUNIT_OCCURENCES, "Number of instruction replacements by VMMCALL.");
    STAM_REG(pVM, &pVM->hm.s.StatTprReplaceFailure, STAMTYPE_COUNTER, "/HM/TPR/Replace/Failed", STAMUNIT_OCCURENCES, "Number of unsuccessful replace attempts.");
#endif

    /*
     * Statistics.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        int    rc;

#ifdef VBOX_WITH_STATISTICS
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of RTMpPokeCpu.",
                             "/PROF/CPU%d/HM/Poke", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatSpinPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of poke wait.",
                             "/PROF/CPU%d/HM/PokeWait", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatSpinPokeFailed, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of poke wait when RTMpPokeCpu fails.",
                             "/PROF/CPU%d/HM/PokeWaitFailed", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatEntry, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of entry until entering GC.",
                             "/PROF/CPU%d/HM/Entry", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatPreExit, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of pre-exit processing after returning from GC.",
                             "/PROF/CPU%d/HM/SwitchFromGC_1", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitHandling, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of exit handling (longjmps not included!)",
                             "/PROF/CPU%d/HM/SwitchFromGC_2", i);
        AssertRC(rc);

        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitIO, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "I/O.",
                             "/PROF/CPU%d/HM/SwitchFromGC_2/IO", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitMovCRx, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "MOV CRx.",
                             "/PROF/CPU%d/HM/SwitchFromGC_2/MovCRx", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitXcptNmi, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Exceptions, NMIs.",
                             "/PROF/CPU%d/HM/SwitchFromGC_2/XcptNmi", i);
        AssertRC(rc);

        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatImportGuestState, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of importing guest state from hardware after VM-exit.",
                             "/PROF/CPU%d/HM/ImportGuestState", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExportGuestState, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of exporting guest state to hardware before VM-entry.",
                             "/PROF/CPU%d/HM/ExportGuestState", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatLoadGuestFpuState, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of CPUMR0LoadGuestFPU.",
                             "/PROF/CPU%d/HM/LoadGuestFpuState", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatInGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of execution of guest-code in hardware.",
                             "/PROF/CPU%d/HM/InGC", i);
        AssertRC(rc);

# if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatWorldSwitch3264, STAMTYPE_PROFILE, STAMVISIBILITY_USED,
                             STAMUNIT_TICKS_PER_CALL, "Profiling of the 32/64 switcher.",
                             "/PROF/CPU%d/HM/Switcher3264", i);
        AssertRC(rc);
# endif

# ifdef HM_PROFILE_EXIT_DISPATCH
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitDispatch, STAMTYPE_PROFILE_ADV, STAMVISIBILITY_USED,
                             STAMUNIT_TICKS_PER_CALL, "Profiling the dispatching of exit handlers.",
                             "/PROF/CPU%d/HM/ExitDispatch", i);
        AssertRC(rc);
# endif

#endif
# define HM_REG_COUNTER(a, b, desc) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, desc, b, i); \
        AssertRC(rc);

#ifdef VBOX_WITH_STATISTICS
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitAll,                "/HM/CPU%d/Exit/All", "Exits (total).");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowNM,           "/HM/CPU%d/Exit/Trap/Shw/#NM", "Shadow #NM (device not available, no math co-processor) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestNM,            "/HM/CPU%d/Exit/Trap/Gst/#NM", "Guest #NM (device not available, no math co-processor) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowPF,           "/HM/CPU%d/Exit/Trap/Shw/#PF", "Shadow #PF (page fault) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowPFEM,         "/HM/CPU%d/Exit/Trap/Shw/#PF-EM", "#PF (page fault) exception going back to ring-3 for emulating the instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestPF,            "/HM/CPU%d/Exit/Trap/Gst/#PF", "Guest #PF (page fault) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestUD,            "/HM/CPU%d/Exit/Trap/Gst/#UD", "Guest #UD (undefined opcode) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestSS,            "/HM/CPU%d/Exit/Trap/Gst/#SS", "Guest #SS (stack-segment fault) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestNP,            "/HM/CPU%d/Exit/Trap/Gst/#NP", "Guest #NP (segment not present) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestGP,            "/HM/CPU%d/Exit/Trap/Gst/#GP", "Guest #GP (general protection) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestMF,            "/HM/CPU%d/Exit/Trap/Gst/#MF", "Guest #MF (x87 FPU error, math fault) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestDE,            "/HM/CPU%d/Exit/Trap/Gst/#DE", "Guest #DE (divide error) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestDB,            "/HM/CPU%d/Exit/Trap/Gst/#DB", "Guest #DB (debug) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestBP,            "/HM/CPU%d/Exit/Trap/Gst/#BP", "Guest #BP (breakpoint) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestXF,            "/HM/CPU%d/Exit/Trap/Gst/#XF", "Guest #XF (extended math fault, SIMD FPU) exception.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestXcpUnk,        "/HM/CPU%d/Exit/Trap/Gst/Other", "Other guest exceptions.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitHlt,                "/HM/CPU%d/Exit/Instr/Hlt", "HLT instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdmsr,              "/HM/CPU%d/Exit/Instr/Rdmsr", "RDMSR instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitWrmsr,              "/HM/CPU%d/Exit/Instr/Wrmsr", "WRMSR instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMwait,              "/HM/CPU%d/Exit/Instr/Mwait", "MWAIT instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMonitor,            "/HM/CPU%d/Exit/Instr/Monitor", "MONITOR instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitDRxWrite,           "/HM/CPU%d/Exit/Instr/DR-Write", "Debug register write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitDRxRead,            "/HM/CPU%d/Exit/Instr/DR-Read", "Debug register read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR0Read,            "/HM/CPU%d/Exit/Instr/CR-Read/CR0", "CR0 read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR2Read,            "/HM/CPU%d/Exit/Instr/CR-Read/CR2", "CR2 read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR3Read,            "/HM/CPU%d/Exit/Instr/CR-Read/CR3", "CR3 read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR4Read,            "/HM/CPU%d/Exit/Instr/CR-Read/CR4", "CR4 read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR8Read,            "/HM/CPU%d/Exit/Instr/CR-Read/CR8", "CR8 read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR0Write,           "/HM/CPU%d/Exit/Instr/CR-Write/CR0", "CR0 write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR2Write,           "/HM/CPU%d/Exit/Instr/CR-Write/CR2", "CR2 write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR3Write,           "/HM/CPU%d/Exit/Instr/CR-Write/CR3", "CR3 write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR4Write,           "/HM/CPU%d/Exit/Instr/CR-Write/CR4", "CR4 write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCR8Write,           "/HM/CPU%d/Exit/Instr/CR-Write/CR8", "CR8 write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitClts,               "/HM/CPU%d/Exit/Instr/CLTS", "CLTS instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitLmsw,               "/HM/CPU%d/Exit/Instr/LMSW", "LMSW instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCli,                "/HM/CPU%d/Exit/Instr/Cli", "CLI instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitSti,                "/HM/CPU%d/Exit/Instr/Sti", "STI instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPushf,              "/HM/CPU%d/Exit/Instr/Pushf", "PUSHF instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPopf,               "/HM/CPU%d/Exit/Instr/Popf", "POPF instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIret,               "/HM/CPU%d/Exit/Instr/Iret", "IRET instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitInt,                "/HM/CPU%d/Exit/Instr/Int", "INT instruction.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitXdtrAccess,         "/HM/CPU%d/Exit/Instr/XdtrAccess", "GDTR, IDTR, LDTR access.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOWrite,            "/HM/CPU%d/Exit/IO/Write", "I/O write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIORead,             "/HM/CPU%d/Exit/IO/Read", "I/O read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOStringWrite,      "/HM/CPU%d/Exit/IO/WriteString", "String I/O write.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOStringRead,       "/HM/CPU%d/Exit/IO/ReadString", "String I/O read.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIntWindow,          "/HM/CPU%d/Exit/IntWindow", "Interrupt-window exit. Guest is ready to receive interrupts again.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitExtInt,             "/HM/CPU%d/Exit/ExtInt", "Physical maskable interrupt (host).");
#endif
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitHostNmiInGC,        "/HM/CPU%d/Exit/HostNmiInGC", "Host NMI received while in guest context.");
#ifdef VBOX_WITH_STATISTICS
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPreemptTimer,       "/HM/CPU%d/Exit/PreemptTimer", "VMX-preemption timer expired.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitTprBelowThreshold,  "/HM/CPU%d/Exit/TprBelowThreshold", "TPR lowered below threshold by the guest.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitTaskSwitch,         "/HM/CPU%d/Exit/TaskSwitch", "Task switch.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMtf,                "/HM/CPU%d/Exit/MonitorTrapFlag", "Monitor Trap Flag.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitApicAccess,         "/HM/CPU%d/Exit/ApicAccess", "APIC access. Guest attempted to access memory at a physical address on the APIC-access page.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchTprMaskedIrq,     "/HM/CPU%d/Switch/TprMaskedIrq", "PDMGetInterrupt() signals TPR masks pending Irq.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchGuestIrq,         "/HM/CPU%d/Switch/IrqPending", "PDMGetInterrupt() cleared behind our back!?!.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchPendingHostIrq,   "/HM/CPU%d/Switch/PendingHostIrq", "Exit to ring-3 due to pending host interrupt before executing guest code.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchHmToR3FF,         "/HM/CPU%d/Switch/HmToR3FF", "Exit to ring-3 due to pending timers, EMT rendezvous, critical section etc.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchExitToR3,         "/HM/CPU%d/Switch/ExitToR3", "Exit to ring-3 (total).");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchLongJmpToR3,      "/HM/CPU%d/Switch/LongJmpToR3", "Longjump to ring-3.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchMaxResumeLoops,   "/HM/CPU%d/Switch/MaxResumeToR3", "Maximum VMRESUME inner-loop counter reached.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchHltToR3,          "/HM/CPU%d/Switch/HltToR3", "HLT causing us to go to ring-3.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchApicAccessToR3,   "/HM/CPU%d/Switch/ApicAccessToR3", "APIC access causing us to go to ring-3.");
#endif
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchPreempt,          "/HM/CPU%d/Switch/Preempting", "EMT has been preempted while in HM context.");
#ifdef VBOX_WITH_STATISTICS
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchPreemptExportHostState, "/HM/CPU%d/Switch/ExportHostState", "Preemption caused us to re-export the host state.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatInjectInterrupt,        "/HM/CPU%d/EventInject/Interrupt", "Injected an external interrupt into the guest.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatInjectXcpt,             "/HM/CPU%d/EventInject/Trap", "Injected an exception into the guest.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatInjectPendingReflect,   "/HM/CPU%d/EventInject/PendingReflect", "Reflecting an exception (or #DF) caused due to event injection.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatInjectPendingInterpret, "/HM/CPU%d/EventInject/PendingInterpret", "Falling to interpreter for handling exception caused due to event injection.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPage,              "/HM/CPU%d/Flush/Page", "Invalidating a guest page on all guest CPUs.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPageManual,        "/HM/CPU%d/Flush/Page/Virt", "Invalidating a guest page using guest-virtual address.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPhysPageManual,    "/HM/CPU%d/Flush/Page/Phys", "Invalidating a guest page using guest-physical address.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlb,               "/HM/CPU%d/Flush/TLB", "Forcing a full guest-TLB flush (ring-0).");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbManual,         "/HM/CPU%d/Flush/TLB/Manual", "Request a full guest-TLB flush.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbWorldSwitch,    "/HM/CPU%d/Flush/TLB/CpuSwitch", "Forcing a full guest-TLB flush due to host-CPU reschedule or ASID-limit hit by another guest-VCPU.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch,  "/HM/CPU%d/Flush/TLB/Skipped", "No TLB flushing required.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushEntire,            "/HM/CPU%d/Flush/TLB/Entire", "Flush the entire TLB (host + guest).");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushAsid,              "/HM/CPU%d/Flush/TLB/ASID", "Flushed guest-TLB entries for the current VPID.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushNestedPaging,      "/HM/CPU%d/Flush/TLB/NestedPaging", "Flushed guest-TLB entries for the current EPT.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbInvlpgVirt,     "/HM/CPU%d/Flush/TLB/InvlpgVirt", "Invalidated a guest-TLB entry for a guest-virtual address.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbInvlpgPhys,     "/HM/CPU%d/Flush/TLB/InvlpgPhys", "Currently not possible, flushes entire guest-TLB.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTlbShootdown,           "/HM/CPU%d/Flush/Shootdown/Page", "Inter-VCPU request to flush queued guest page.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTlbShootdownFlush,      "/HM/CPU%d/Flush/Shootdown/TLB", "Inter-VCPU request to flush entire guest-TLB.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatTscParavirt,            "/HM/CPU%d/TSC/Paravirt", "Paravirtualized TSC in effect.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTscOffset,              "/HM/CPU%d/TSC/Offset", "TSC offsetting is in effect.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTscIntercept,           "/HM/CPU%d/TSC/Intercept", "Intercept TSC accesses.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxArmed,               "/HM/CPU%d/Debug/Armed", "Loaded guest-debug state while loading guest-state.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxContextSwitch,       "/HM/CPU%d/Debug/ContextSwitch", "Loaded guest-debug state on MOV DRx.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxIoCheck,             "/HM/CPU%d/Debug/IOCheck", "Checking for I/O breakpoint.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatExportMinimal,          "/HM/CPU%d/Export/Minimal", "VM-entry exporting minimal guest-state.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExportFull,             "/HM/CPU%d/Export/Full", "VM-entry exporting the full guest-state.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatLoadGuestFpu,           "/HM/CPU%d/Export/GuestFpu", "VM-entry loading the guest-FPU state.");

        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadRmSelBase,   "/HM/CPU%d/VMXCheck/RMSelBase", "Could not use VMX due to unsuitable real-mode selector base.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadRmSelLimit,  "/HM/CPU%d/VMXCheck/RMSelLimit", "Could not use VMX due to unsuitable real-mode selector limit.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckRmOk,           "/HM/CPU%d/VMXCheck/VMX_RM", "VMX execution in real (V86) mode OK.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadSel,         "/HM/CPU%d/VMXCheck/Selector", "Could not use VMX due to unsuitable selector.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadRpl,         "/HM/CPU%d/VMXCheck/RPL", "Could not use VMX due to unsuitable RPL.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadLdt,         "/HM/CPU%d/VMXCheck/LDT", "Could not use VMX due to unsuitable LDT.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckBadTr,          "/HM/CPU%d/VMXCheck/TR", "Could not use VMX due to unsuitable TR.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatVmxCheckPmOk,           "/HM/CPU%d/VMXCheck/VMX_PM", "VMX execution in protected mode OK.");

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
        HM_REG_COUNTER(&pVCpu->hm.s.StatFpu64SwitchBack,        "/HM/CPU%d/Switch64/Fpu", "Saving guest FPU/XMM state.");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDebug64SwitchBack,      "/HM/CPU%d/Switch64/Debug", "Saving guest debug state.");
#endif

#undef HM_REG_COUNTER

        const char *const *papszDesc = ASMIsIntelCpu() || ASMIsViaCentaurCpu() ? &g_apszVmxExitReasons[0]
                                                                               : &g_apszSvmExitReasons[0];

        /*
         * Guest Exit reason stats.
         */
        pVCpu->hm.s.paStatExitReason = NULL;
        rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT * sizeof(*pVCpu->hm.s.paStatExitReason), 0 /* uAlignment */, MM_TAG_HM,
                          (void **)&pVCpu->hm.s.paStatExitReason);
        AssertRCReturn(rc, rc);
        for (int j = 0; j < MAX_EXITREASON_STAT; j++)
        {
            if (papszDesc[j])
            {
                rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.paStatExitReason[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                                     STAMUNIT_OCCURENCES, papszDesc[j], "/HM/CPU%d/Exit/Reason/%02x", i, j);
                AssertRCReturn(rc, rc);
            }
        }
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitReasonNpf, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                             "Nested page fault", "/HM/CPU%d/Exit/Reason/#NPF", i);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.paStatExitReasonR0 = MMHyperR3ToR0(pVM, pVCpu->hm.s.paStatExitReason);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hm.s.paStatExitReasonR0 != NIL_RTR0PTR || !HMIsEnabled(pVM));
# else
        Assert(pVCpu->hm.s.paStatExitReasonR0 != NIL_RTR0PTR);
# endif

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
        /*
         * Nested-guest Exit reason stats.
         */
        pVCpu->hm.s.paStatNestedExitReason = NULL;
        rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT * sizeof(*pVCpu->hm.s.paStatNestedExitReason), 0 /* uAlignment */, MM_TAG_HM,
                          (void **)&pVCpu->hm.s.paStatNestedExitReason);
        AssertRCReturn(rc, rc);
        for (int j = 0; j < MAX_EXITREASON_STAT; j++)
        {
            if (papszDesc[j])
            {
                rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.paStatNestedExitReason[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                                     STAMUNIT_OCCURENCES, papszDesc[j], "/HM/CPU%d/NestedExit/Reason/%02x", i, j);
                AssertRC(rc);
            }
        }
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatNestedExitReasonNpf, STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                             STAMUNIT_OCCURENCES, "Nested page fault", "/HM/CPU%d/NestedExit/Reason/#NPF", i);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.paStatNestedExitReasonR0 = MMHyperR3ToR0(pVM, pVCpu->hm.s.paStatNestedExitReason);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hm.s.paStatNestedExitReasonR0 != NIL_RTR0PTR || !HMIsEnabled(pVM));
# else
        Assert(pVCpu->hm.s.paStatNestedExitReasonR0 != NIL_RTR0PTR);
# endif
#endif

        /*
         * Injected events stats.
         */
        rc = MMHyperAlloc(pVM, sizeof(STAMCOUNTER) * 256, 8, MM_TAG_HM, (void **)&pVCpu->hm.s.paStatInjectedIrqs);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.paStatInjectedIrqsR0 = MMHyperR3ToR0(pVM, pVCpu->hm.s.paStatInjectedIrqs);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR || !HMIsEnabled(pVM));
# else
        Assert(pVCpu->hm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR);
# endif
        for (unsigned j = 0; j < 255; j++)
        {
            STAMR3RegisterF(pVM, &pVCpu->hm.s.paStatInjectedIrqs[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "Injected event.",
                            (j < 0x20) ? "/HM/CPU%d/EventInject/InjectTrap/%02X" : "/HM/CPU%d/EventInject/InjectIRQ/%02X", i, j);
        }

#endif /* VBOX_WITH_STATISTICS */
    }

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /*
     * Magic marker for searching in crash dumps.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
        strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
        pCache->uMagic = UINT64_C(0xdeadbeefdeadbeef);
    }
#endif

    return VINF_SUCCESS;
}


/**
 * Called when a init phase has completed.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   enmWhat             The phase that completed.
 */
VMMR3_INT_DECL(int) HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    switch (enmWhat)
    {
        case VMINITCOMPLETED_RING3:
            return hmR3InitCPU(pVM);
        case VMINITCOMPLETED_RING0:
            return hmR3InitFinalizeR0(pVM);
        default:
            return VINF_SUCCESS;
    }
}


/**
 * Turns off normal raw mode features.
 *
 * @param   pVM         The cross context VM structure.
 */
static void hmR3DisableRawMode(PVM pVM)
{
/** @todo r=bird: HM shouldn't be doing this crap. */
    /* Reinit the paging mode to force the new shadow mode. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        PGMHCChangeMode(pVM, pVCpu, PGMMODE_REAL);
    }
}


/**
 * Initialize VT-x or AMD-V.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int hmR3InitFinalizeR0(PVM pVM)
{
    int rc;

    if (!HMIsEnabled(pVM))
        return VINF_SUCCESS;

    /*
     * Hack to allow users to work around broken BIOSes that incorrectly set
     * EFER.SVME, which makes us believe somebody else is already using AMD-V.
     */
    if (   !pVM->hm.s.vmx.fSupported
        && !pVM->hm.s.svm.fSupported
        &&  pVM->hm.s.rcInit == VERR_SVM_IN_USE /* implies functional AMD-V */
        &&  RTEnvExist("VBOX_HWVIRTEX_IGNORE_SVM_IN_USE"))
    {
        LogRel(("HM: VBOX_HWVIRTEX_IGNORE_SVM_IN_USE active!\n"));
        pVM->hm.s.svm.fSupported        = true;
        pVM->hm.s.svm.fIgnoreInUseError = true;
        pVM->hm.s.rcInit = VINF_SUCCESS;
    }

    /*
     * Report ring-0 init errors.
     */
    if (   !pVM->hm.s.vmx.fSupported
        && !pVM->hm.s.svm.fSupported)
    {
        LogRel(("HM: Failed to initialize VT-x / AMD-V: %Rrc\n", pVM->hm.s.rcInit));
        LogRel(("HM: VMX MSR_IA32_FEATURE_CONTROL=%RX64\n", pVM->hm.s.vmx.Msrs.u64FeatCtrl));
        switch (pVM->hm.s.rcInit)
        {
            case VERR_VMX_IN_VMX_ROOT_MODE:
                return VM_SET_ERROR(pVM, VERR_VMX_IN_VMX_ROOT_MODE, "VT-x is being used by another hypervisor");
            case VERR_VMX_NO_VMX:
                return VM_SET_ERROR(pVM, VERR_VMX_NO_VMX, "VT-x is not available");
            case VERR_VMX_MSR_VMX_DISABLED:
                return VM_SET_ERROR(pVM, VERR_VMX_MSR_VMX_DISABLED, "VT-x is disabled in the BIOS");
            case VERR_VMX_MSR_ALL_VMX_DISABLED:
                return VM_SET_ERROR(pVM, VERR_VMX_MSR_ALL_VMX_DISABLED, "VT-x is disabled in the BIOS for all CPU modes");
            case VERR_VMX_MSR_LOCKING_FAILED:
                return VM_SET_ERROR(pVM, VERR_VMX_MSR_LOCKING_FAILED, "Failed to lock VT-x features while trying to enable VT-x");
            case VERR_VMX_MSR_VMX_ENABLE_FAILED:
                return VM_SET_ERROR(pVM, VERR_VMX_MSR_VMX_ENABLE_FAILED, "Failed to enable VT-x features");
            case VERR_VMX_MSR_SMX_VMX_ENABLE_FAILED:
                return VM_SET_ERROR(pVM, VERR_VMX_MSR_SMX_VMX_ENABLE_FAILED, "Failed to enable VT-x features in SMX mode");

            case VERR_SVM_IN_USE:
                return VM_SET_ERROR(pVM, VERR_SVM_IN_USE, "AMD-V is being used by another hypervisor");
            case VERR_SVM_NO_SVM:
                return VM_SET_ERROR(pVM, VERR_SVM_NO_SVM, "AMD-V is not available");
            case VERR_SVM_DISABLED:
                return VM_SET_ERROR(pVM, VERR_SVM_DISABLED, "AMD-V is disabled in the BIOS");
        }
        return VMSetError(pVM, pVM->hm.s.rcInit, RT_SRC_POS, "HM ring-0 init failed: %Rrc", pVM->hm.s.rcInit);
    }

    /*
     * Enable VT-x or AMD-V on all host CPUs.
     */
    rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_ENABLE, 0, NULL);
    if (RT_FAILURE(rc))
    {
        LogRel(("HM: Failed to enable, error %Rrc\n", rc));
        HMR3CheckError(pVM, rc);
        return rc;
    }

    /*
     * No TPR patching is required when the IO-APIC is not enabled for this VM.
     * (Main should have taken care of this already)
     */
    if (!PDMHasIoApic(pVM))
    {
        Assert(!pVM->hm.s.fTprPatchingAllowed); /* paranoia */
        pVM->hm.s.fTprPatchingAllowed = false;
    }

    /*
     * Sync options.
     */
    /** @todo Move this out of of CPUMCTX and into some ring-0 only HM structure.
     *        That will require a little bit of work, of course. */
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
    {
        PVMCPU   pVCpu   = &pVM->aCpus[iCpu];
        PCPUMCTX pCpuCtx = &pVCpu->cpum.GstCtx;
        pCpuCtx->fWorldSwitcher &= ~(CPUMCTX_WSF_IBPB_EXIT | CPUMCTX_WSF_IBPB_ENTRY);
        if (pVM->cpum.ro.HostFeatures.fIbpb)
        {
            if (pVM->hm.s.fIbpbOnVmExit)
                pCpuCtx->fWorldSwitcher |= CPUMCTX_WSF_IBPB_EXIT;
            if (pVM->hm.s.fIbpbOnVmEntry)
                pCpuCtx->fWorldSwitcher |= CPUMCTX_WSF_IBPB_ENTRY;
        }
        if (iCpu == 0)
            LogRel(("HM: fWorldSwitcher=%#x (fIbpbOnVmExit=%RTbool fIbpbOnVmEntry=%RTbool)\n",
                    pCpuCtx->fWorldSwitcher, pVM->hm.s.fIbpbOnVmExit, pVM->hm.s.fIbpbOnVmEntry));
    }

    /*
     * Do the vendor specific initialization
     *
     * Note! We disable release log buffering here since we're doing relatively
     *       lot of logging and doesn't want to hit the disk with each LogRel
     *       statement.
     */
    AssertLogRelReturn(!pVM->hm.s.fInitialized, VERR_HM_IPE_5);
    bool fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);
    if (pVM->hm.s.vmx.fSupported)
        rc = hmR3InitFinalizeR0Intel(pVM);
    else
        rc = hmR3InitFinalizeR0Amd(pVM);
    LogRel(("HM: VT-x/AMD-V init method: %s\n", (pVM->hm.s.fGlobalInit) ? "GLOBAL" : "LOCAL"));
    RTLogRelSetBuffering(fOldBuffered);
    pVM->hm.s.fInitialized = true;

    return rc;
}


/**
 * @callback_method_impl{FNPDMVMMDEVHEAPNOTIFY}
 */
static DECLCALLBACK(void) hmR3VmmDevHeapNotify(PVM pVM, void *pvAllocation, RTGCPHYS GCPhysAllocation)
{
    NOREF(pVM);
    NOREF(pvAllocation);
    NOREF(GCPhysAllocation);
}


/**
 * Returns a description of the VMCS (and associated regions') memory type given the
 * IA32_VMX_BASIC MSR.
 *
 * @returns The descriptive memory type.
 * @param   uMsrVmxBasic        IA32_VMX_BASIC MSR value.
 */
static const char *hmR3VmxGetMemTypeDesc(uint64_t uMsrVmxBasic)
{
    uint8_t const uMemType = RT_BF_GET(uMsrVmxBasic, VMX_BF_BASIC_VMCS_MEM_TYPE);
    switch (uMemType)
    {
        case VMX_BASIC_MEM_TYPE_WB: return "Write Back (WB)";
        case VMX_BASIC_MEM_TYPE_UC: return "Uncacheable (UC)";
    }
    return "Unknown";
}


/**
 * Returns a single-line description of all the activity-states supported by the CPU
 * given the IA32_VMX_MISC MSR.
 *
 * @returns All supported activity states.
 * @param   uMsrMisc            IA32_VMX_MISC MSR value.
 */
static const char *hmR3VmxGetActivityStateAllDesc(uint64_t uMsrMisc)
{
    static const char * const s_apszActStates[] =
    {
        "",
        " ( HLT )",
        " ( SHUTDOWN )",
        " ( HLT SHUTDOWN )",
        " ( SIPI_WAIT )",
        " ( HLT SIPI_WAIT )",
        " ( SHUTDOWN SIPI_WAIT )",
        " ( HLT SHUTDOWN SIPI_WAIT )"
    };
    uint8_t const idxActStates = RT_BF_GET(uMsrMisc, VMX_BF_MISC_ACTIVITY_STATES);
    Assert(idxActStates < RT_ELEMENTS(s_apszActStates));
    return s_apszActStates[idxActStates];
}


/**
 * Reports MSR_IA32_FEATURE_CONTROL MSR to the log.
 *
 * @param   fFeatMsr    The feature control MSR value.
 */
static void hmR3VmxReportFeatCtlMsr(uint64_t fFeatMsr)
{
    uint64_t const val = fFeatMsr;
    LogRel(("HM: MSR_IA32_FEATURE_CONTROL          = %#RX64\n", val));
    HMVMX_REPORT_MSR_CAP(val, "LOCK",             MSR_IA32_FEATURE_CONTROL_LOCK);
    HMVMX_REPORT_MSR_CAP(val, "SMX_VMXON",        MSR_IA32_FEATURE_CONTROL_SMX_VMXON);
    HMVMX_REPORT_MSR_CAP(val, "VMXON",            MSR_IA32_FEATURE_CONTROL_VMXON);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN0", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_0);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN1", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_1);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN2", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_2);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN3", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_3);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN4", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_4);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN5", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_5);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_LOCAL_FN6", MSR_IA32_FEATURE_CONTROL_SENTER_LOCAL_FN_6);
    HMVMX_REPORT_MSR_CAP(val, "SENTER_GLOBAL_EN", MSR_IA32_FEATURE_CONTROL_SENTER_GLOBAL_EN);
    HMVMX_REPORT_MSR_CAP(val, "SGX_LAUNCH_EN",    MSR_IA32_FEATURE_CONTROL_SGX_LAUNCH_EN);
    HMVMX_REPORT_MSR_CAP(val, "SGX_GLOBAL_EN",    MSR_IA32_FEATURE_CONTROL_SGX_GLOBAL_EN);
    HMVMX_REPORT_MSR_CAP(val, "LMCE",             MSR_IA32_FEATURE_CONTROL_LMCE);
    if (!(val & MSR_IA32_FEATURE_CONTROL_LOCK))
        LogRel(("HM:   MSR_IA32_FEATURE_CONTROL lock bit not set, possibly bad hardware!\n"));
}


/**
 * Reports MSR_IA32_VMX_BASIC MSR to the log.
 *
 * @param   uBasicMsr    The VMX basic MSR value.
 */
static void hmR3VmxReportBasicMsr(uint64_t uBasicMsr)
{
    LogRel(("HM: MSR_IA32_VMX_BASIC                = %#RX64\n",     uBasicMsr));
    LogRel(("HM:   VMCS id                           = %#x\n",      RT_BF_GET(uBasicMsr, VMX_BF_BASIC_VMCS_ID)));
    LogRel(("HM:   VMCS size                         = %u bytes\n", RT_BF_GET(uBasicMsr, VMX_BF_BASIC_VMCS_SIZE)));
    LogRel(("HM:   VMCS physical address limit       = %s\n",       RT_BF_GET(uBasicMsr, VMX_BF_BASIC_PHYSADDR_WIDTH) ?
                                                                    "< 4 GB" : "None"));
    LogRel(("HM:   VMCS memory type                  = %s\n",       hmR3VmxGetMemTypeDesc(uBasicMsr)));
    LogRel(("HM:   Dual-monitor treatment support    = %RTbool\n",  RT_BF_GET(uBasicMsr, VMX_BF_BASIC_DUAL_MON)));
    LogRel(("HM:   OUTS & INS instruction-info       = %RTbool\n",  RT_BF_GET(uBasicMsr, VMX_BF_BASIC_VMCS_INS_OUTS)));
    LogRel(("HM:   Supports true capability MSRs     = %RTbool\n",  RT_BF_GET(uBasicMsr, VMX_BF_BASIC_TRUE_CTLS)));
}


/**
 * Reports MSR_IA32_PINBASED_CTLS to the log.
 *
 * @param   pVmxMsr    Pointer to the VMX MSR.
 */
static void hmR3VmxReportPinBasedCtlsMsr(PCVMXCTLSMSR pVmxMsr)
{
    uint64_t const val = pVmxMsr->n.allowed1;
    uint64_t const zap = pVmxMsr->n.disallowed0;
    LogRel(("HM: MSR_IA32_VMX_PINBASED_CTLS        = %#RX64\n", pVmxMsr->u));
    HMVMX_REPORT_FEAT(val, zap, "EXT_INT_EXIT",  VMX_PIN_CTLS_EXT_INT_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "NMI_EXIT",      VMX_PIN_CTLS_NMI_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "VIRTUAL_NMI",   VMX_PIN_CTLS_VIRT_NMI);
    HMVMX_REPORT_FEAT(val, zap, "PREEMPT_TIMER", VMX_PIN_CTLS_PREEMPT_TIMER);
    HMVMX_REPORT_FEAT(val, zap, "POSTED_INT",    VMX_PIN_CTLS_POSTED_INT);
}


/**
 * Reports MSR_IA32_VMX_PROCBASED_CTLS MSR to the log.
 *
 * @param   pVmxMsr    Pointer to the VMX MSR.
 */
static void hmR3VmxReportProcBasedCtlsMsr(PCVMXCTLSMSR pVmxMsr)
{
    uint64_t const val = pVmxMsr->n.allowed1;
    uint64_t const zap = pVmxMsr->n.disallowed0;
    LogRel(("HM: MSR_IA32_VMX_PROCBASED_CTLS       = %#RX64\n", pVmxMsr->u));
    HMVMX_REPORT_FEAT(val, zap, "INT_WINDOW_EXIT",         VMX_PROC_CTLS_INT_WINDOW_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "USE_TSC_OFFSETTING",      VMX_PROC_CTLS_USE_TSC_OFFSETTING);
    HMVMX_REPORT_FEAT(val, zap, "HLT_EXIT",                VMX_PROC_CTLS_HLT_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "INVLPG_EXIT",             VMX_PROC_CTLS_INVLPG_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "MWAIT_EXIT",              VMX_PROC_CTLS_MWAIT_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "RDPMC_EXIT",              VMX_PROC_CTLS_RDPMC_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "RDTSC_EXIT",              VMX_PROC_CTLS_RDTSC_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "CR3_LOAD_EXIT",           VMX_PROC_CTLS_CR3_LOAD_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "CR3_STORE_EXIT",          VMX_PROC_CTLS_CR3_STORE_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "CR8_LOAD_EXIT",           VMX_PROC_CTLS_CR8_LOAD_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "CR8_STORE_EXIT",          VMX_PROC_CTLS_CR8_STORE_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "USE_TPR_SHADOW",          VMX_PROC_CTLS_USE_TPR_SHADOW);
    HMVMX_REPORT_FEAT(val, zap, "NMI_WINDOW_EXIT",         VMX_PROC_CTLS_NMI_WINDOW_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "MOV_DR_EXIT",             VMX_PROC_CTLS_MOV_DR_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "UNCOND_IO_EXIT",          VMX_PROC_CTLS_UNCOND_IO_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "USE_IO_BITMAPS",          VMX_PROC_CTLS_USE_IO_BITMAPS);
    HMVMX_REPORT_FEAT(val, zap, "MONITOR_TRAP_FLAG",       VMX_PROC_CTLS_MONITOR_TRAP_FLAG);
    HMVMX_REPORT_FEAT(val, zap, "USE_MSR_BITMAPS",         VMX_PROC_CTLS_USE_MSR_BITMAPS);
    HMVMX_REPORT_FEAT(val, zap, "MONITOR_EXIT",            VMX_PROC_CTLS_MONITOR_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "PAUSE_EXIT",              VMX_PROC_CTLS_PAUSE_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "USE_SECONDARY_CTLS",      VMX_PROC_CTLS_USE_SECONDARY_CTLS);
}


/**
 * Reports MSR_IA32_VMX_PROCBASED_CTLS2 MSR to the log.
 *
 * @param   pVmxMsr    Pointer to the VMX MSR.
 */
static void hmR3VmxReportProcBasedCtls2Msr(PCVMXCTLSMSR pVmxMsr)
{
    uint64_t const val = pVmxMsr->n.allowed1;
    uint64_t const zap = pVmxMsr->n.disallowed0;
    LogRel(("HM: MSR_IA32_VMX_PROCBASED_CTLS2      = %#RX64\n", pVmxMsr->u));
    HMVMX_REPORT_FEAT(val, zap, "VIRT_APIC_ACCESS",      VMX_PROC_CTLS2_VIRT_APIC_ACCESS);
    HMVMX_REPORT_FEAT(val, zap, "EPT",                   VMX_PROC_CTLS2_EPT);
    HMVMX_REPORT_FEAT(val, zap, "DESC_TABLE_EXIT",       VMX_PROC_CTLS2_DESC_TABLE_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "RDTSCP",                VMX_PROC_CTLS2_RDTSCP);
    HMVMX_REPORT_FEAT(val, zap, "VIRT_X2APIC_MODE",      VMX_PROC_CTLS2_VIRT_X2APIC_MODE);
    HMVMX_REPORT_FEAT(val, zap, "VPID",                  VMX_PROC_CTLS2_VPID);
    HMVMX_REPORT_FEAT(val, zap, "WBINVD_EXIT",           VMX_PROC_CTLS2_WBINVD_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "UNRESTRICTED_GUEST",    VMX_PROC_CTLS2_UNRESTRICTED_GUEST);
    HMVMX_REPORT_FEAT(val, zap, "APIC_REG_VIRT",         VMX_PROC_CTLS2_APIC_REG_VIRT);
    HMVMX_REPORT_FEAT(val, zap, "VIRT_INT_DELIVERY",     VMX_PROC_CTLS2_VIRT_INT_DELIVERY);
    HMVMX_REPORT_FEAT(val, zap, "PAUSE_LOOP_EXIT",       VMX_PROC_CTLS2_PAUSE_LOOP_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "RDRAND_EXIT",           VMX_PROC_CTLS2_RDRAND_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "INVPCID",               VMX_PROC_CTLS2_INVPCID);
    HMVMX_REPORT_FEAT(val, zap, "VMFUNC",                VMX_PROC_CTLS2_VMFUNC);
    HMVMX_REPORT_FEAT(val, zap, "VMCS_SHADOWING",        VMX_PROC_CTLS2_VMCS_SHADOWING);
    HMVMX_REPORT_FEAT(val, zap, "ENCLS_EXIT",            VMX_PROC_CTLS2_ENCLS_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "RDSEED_EXIT",           VMX_PROC_CTLS2_RDSEED_EXIT);
    HMVMX_REPORT_FEAT(val, zap, "PML",                   VMX_PROC_CTLS2_PML);
    HMVMX_REPORT_FEAT(val, zap, "EPT_VE",                VMX_PROC_CTLS2_EPT_VE);
    HMVMX_REPORT_FEAT(val, zap, "CONCEAL_FROM_PT",       VMX_PROC_CTLS2_CONCEAL_FROM_PT);
    HMVMX_REPORT_FEAT(val, zap, "XSAVES_XRSTORS",        VMX_PROC_CTLS2_XSAVES_XRSTORS);
    HMVMX_REPORT_FEAT(val, zap, "TSC_SCALING",           VMX_PROC_CTLS2_TSC_SCALING);
}


/**
 * Reports MSR_IA32_VMX_ENTRY_CTLS to the log.
 *
 * @param   pVmxMsr    Pointer to the VMX MSR.
 */
static void hmR3VmxReportEntryCtlsMsr(PCVMXCTLSMSR pVmxMsr)
{
    uint64_t const val = pVmxMsr->n.allowed1;
    uint64_t const zap = pVmxMsr->n.disallowed0;
    LogRel(("HM: MSR_IA32_VMX_ENTRY_CTLS           = %#RX64\n", pVmxMsr->u));
    HMVMX_REPORT_FEAT(val, zap, "LOAD_DEBUG",          VMX_ENTRY_CTLS_LOAD_DEBUG);
    HMVMX_REPORT_FEAT(val, zap, "IA32E_MODE_GUEST",    VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    HMVMX_REPORT_FEAT(val, zap, "ENTRY_TO_SMM",        VMX_ENTRY_CTLS_ENTRY_TO_SMM);
    HMVMX_REPORT_FEAT(val, zap, "DEACTIVATE_DUAL_MON", VMX_ENTRY_CTLS_DEACTIVATE_DUAL_MON);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_PERF_MSR",       VMX_ENTRY_CTLS_LOAD_PERF_MSR);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_PAT_MSR",        VMX_ENTRY_CTLS_LOAD_PAT_MSR);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_EFER_MSR",       VMX_ENTRY_CTLS_LOAD_EFER_MSR);
}


/**
 * Reports MSR_IA32_VMX_EXIT_CTLS to the log.
 *
 * @param   pVmxMsr    Pointer to the VMX MSR.
 */
static void hmR3VmxReportExitCtlsMsr(PCVMXCTLSMSR pVmxMsr)
{
    uint64_t const val = pVmxMsr->n.allowed1;
    uint64_t const zap = pVmxMsr->n.disallowed0;
    LogRel(("HM: MSR_IA32_VMX_EXIT_CTLS            = %#RX64\n", pVmxMsr->u));
    HMVMX_REPORT_FEAT(val, zap, "SAVE_DEBUG",             VMX_EXIT_CTLS_SAVE_DEBUG);
    HMVMX_REPORT_FEAT(val, zap, "HOST_ADDR_SPACE_SIZE",   VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_PERF_MSR",          VMX_EXIT_CTLS_LOAD_PERF_MSR);
    HMVMX_REPORT_FEAT(val, zap, "ACK_EXT_INT",            VMX_EXIT_CTLS_ACK_EXT_INT);
    HMVMX_REPORT_FEAT(val, zap, "SAVE_PAT_MSR",           VMX_EXIT_CTLS_SAVE_PAT_MSR);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_PAT_MSR",           VMX_EXIT_CTLS_LOAD_PAT_MSR);
    HMVMX_REPORT_FEAT(val, zap, "SAVE_EFER_MSR",          VMX_EXIT_CTLS_SAVE_EFER_MSR);
    HMVMX_REPORT_FEAT(val, zap, "LOAD_EFER_MSR",          VMX_EXIT_CTLS_LOAD_EFER_MSR);
    HMVMX_REPORT_FEAT(val, zap, "SAVE_PREEMPT_TIMER",     VMX_EXIT_CTLS_SAVE_PREEMPT_TIMER);
}


/**
 * Reports MSR_IA32_VMX_EPT_VPID_CAP MSR to the log.
 *
 * @param   fCaps    The VMX EPT/VPID capability MSR value.
 */
static void hmR3VmxReportEptVpidCapsMsr(uint64_t fCaps)
{
    LogRel(("HM: MSR_IA32_VMX_EPT_VPID_CAP         = %#RX64\n", fCaps));
    HMVMX_REPORT_MSR_CAP(fCaps, "RWX_X_ONLY",                            MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY);
    HMVMX_REPORT_MSR_CAP(fCaps, "PAGE_WALK_LENGTH_4",                    MSR_IA32_VMX_EPT_VPID_CAP_PAGE_WALK_LENGTH_4);
    HMVMX_REPORT_MSR_CAP(fCaps, "EMT_UC",                                MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC);
    HMVMX_REPORT_MSR_CAP(fCaps, "EMT_WB",                                MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB);
    HMVMX_REPORT_MSR_CAP(fCaps, "PDE_2M",                                MSR_IA32_VMX_EPT_VPID_CAP_PDE_2M);
    HMVMX_REPORT_MSR_CAP(fCaps, "PDPTE_1G",                              MSR_IA32_VMX_EPT_VPID_CAP_PDPTE_1G);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVEPT",                                MSR_IA32_VMX_EPT_VPID_CAP_INVEPT);
    HMVMX_REPORT_MSR_CAP(fCaps, "EPT_ACCESS_DIRTY",                      MSR_IA32_VMX_EPT_VPID_CAP_EPT_ACCESS_DIRTY);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVEPT_SINGLE_CONTEXT",                 MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVEPT_ALL_CONTEXTS",                   MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVVPID",                               MSR_IA32_VMX_EPT_VPID_CAP_INVVPID);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVVPID_INDIV_ADDR",                    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVVPID_SINGLE_CONTEXT",                MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVVPID_ALL_CONTEXTS",                  MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS);
    HMVMX_REPORT_MSR_CAP(fCaps, "INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS", MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS);
}


/**
 * Reports MSR_IA32_VMX_MISC MSR to the log.
 *
 * @param   pVM      Pointer to the VM.
 * @param   fMisc    The VMX misc. MSR value.
 */
static void hmR3VmxReportMiscMsr(PVM pVM, uint64_t fMisc)
{
    LogRel(("HM: MSR_IA32_VMX_MISC                 = %#RX64\n", fMisc));
    uint8_t const cPreemptTimerShift = RT_BF_GET(fMisc, VMX_BF_MISC_PREEMPT_TIMER_TSC);
    if (cPreemptTimerShift == pVM->hm.s.vmx.cPreemptTimerShift)
        LogRel(("HM:   PREEMPT_TIMER_TSC                 = %#x\n",    cPreemptTimerShift));
    else
    {
        LogRel(("HM:   PREEMPT_TIMER_TSC                 = %#x - erratum detected, using %#x instead\n", cPreemptTimerShift,
                pVM->hm.s.vmx.cPreemptTimerShift));
    }
    LogRel(("HM:   EXIT_STORE_EFER_LMA               = %RTbool\n",    RT_BF_GET(fMisc, VMX_BF_MISC_EXIT_STORE_EFER_LMA)));
    LogRel(("HM:   ACTIVITY_STATES                   = %#x%s\n",      RT_BF_GET(fMisc, VMX_BF_MISC_ACTIVITY_STATES),
                                                                      hmR3VmxGetActivityStateAllDesc(fMisc)));
    LogRel(("HM:   PT                                = %RTbool\n",    RT_BF_GET(fMisc, VMX_BF_MISC_PT)));
    LogRel(("HM:   SMM_READ_SMBASE_MSR               = %RTbool\n",    RT_BF_GET(fMisc, VMX_BF_MISC_SMM_READ_SMBASE_MSR)));
    LogRel(("HM:   CR3_TARGET                        = %#x\n",        RT_BF_GET(fMisc, VMX_BF_MISC_CR3_TARGET)));
    LogRel(("HM:   MAX_MSR                           = %#x ( %u )\n", RT_BF_GET(fMisc, VMX_BF_MISC_MAX_MSRS),
                                                                      VMX_MISC_MAX_MSRS(fMisc)));
    LogRel(("HM:   VMXOFF_BLOCK_SMI                  = %RTbool\n",    RT_BF_GET(fMisc, VMX_BF_MISC_VMXOFF_BLOCK_SMI)));
    LogRel(("HM:   VMWRITE_ALL                       = %RTbool\n",    RT_BF_GET(fMisc, VMX_BF_MISC_VMWRITE_ALL)));
    LogRel(("HM:   ENTRY_INJECT_SOFT_INT             = %#x\n",        RT_BF_GET(fMisc, VMX_BF_MISC_ENTRY_INJECT_SOFT_INT)));
    LogRel(("HM:   MSEG_ID                           = %#x\n",        RT_BF_GET(fMisc, VMX_BF_MISC_MSEG_ID)));
}


/**
 * Reports MSR_IA32_VMX_VMCS_ENUM MSR to the log.
 *
 * @param   uVmcsEnum    The VMX VMCS enum MSR value.
 */
static void hmR3VmxReportVmcsEnumMsr(uint64_t uVmcsEnum)
{
    LogRel(("HM: MSR_IA32_VMX_VMCS_ENUM            = %#RX64\n", uVmcsEnum));
    LogRel(("HM:   HIGHEST_IDX                       = %#x\n", RT_BF_GET(uVmcsEnum, VMX_BF_VMCS_ENUM_HIGHEST_IDX)));
}


/**
 * Reports MSR_IA32_VMX_VMFUNC MSR to the log.
 *
 * @param   uVmFunc    The VMX VMFUNC MSR value.
 */
static void hmR3VmxReportVmFuncMsr(uint64_t uVmFunc)
{
    LogRel(("HM: MSR_IA32_VMX_VMFUNC               = %#RX64\n", uVmFunc));
    HMVMX_REPORT_ALLOWED_FEAT(uVmFunc, "EPTP_SWITCHING", RT_BF_GET(uVmFunc, VMX_BF_VMFUNC_EPTP_SWITCHING));
}


/**
 * Reports VMX CR0, CR4 fixed MSRs.
 *
 * @param   pMsrs    Pointer to the VMX MSRs.
 */
static void hmR3VmxReportCrFixedMsrs(PVMXMSRS pMsrs)
{
    LogRel(("HM: MSR_IA32_VMX_CR0_FIXED0           = %#RX64\n", pMsrs->u64Cr0Fixed0));
    LogRel(("HM: MSR_IA32_VMX_CR0_FIXED1           = %#RX64\n", pMsrs->u64Cr0Fixed1));
    LogRel(("HM: MSR_IA32_VMX_CR4_FIXED0           = %#RX64\n", pMsrs->u64Cr4Fixed0));
    LogRel(("HM: MSR_IA32_VMX_CR4_FIXED1           = %#RX64\n", pMsrs->u64Cr4Fixed1));
}


/**
 * Finish VT-x initialization (after ring-0 init).
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 */
static int hmR3InitFinalizeR0Intel(PVM pVM)
{
    int rc;

    Log(("pVM->hm.s.vmx.fSupported = %d\n", pVM->hm.s.vmx.fSupported));
    AssertLogRelReturn(pVM->hm.s.vmx.Msrs.u64FeatCtrl != 0, VERR_HM_IPE_4);

    LogRel(("HM: Using VT-x implementation 2.0\n"));
    LogRel(("HM: Max resume loops                  = %u\n", pVM->hm.s.cMaxResumeLoops));
    LogRel(("HM: Host CR4                          = %#RX64\n", pVM->hm.s.vmx.u64HostCr4));
    LogRel(("HM: Host EFER                         = %#RX64\n", pVM->hm.s.vmx.u64HostEfer));
    LogRel(("HM: MSR_IA32_SMM_MONITOR_CTL          = %#RX64\n", pVM->hm.s.vmx.u64HostSmmMonitorCtl));

    hmR3VmxReportFeatCtlMsr(pVM->hm.s.vmx.Msrs.u64FeatCtrl);
    hmR3VmxReportBasicMsr(pVM->hm.s.vmx.Msrs.u64Basic);

    hmR3VmxReportPinBasedCtlsMsr(&pVM->hm.s.vmx.Msrs.PinCtls);
    hmR3VmxReportProcBasedCtlsMsr(&pVM->hm.s.vmx.Msrs.ProcCtls);
    if (pVM->hm.s.vmx.Msrs.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
        hmR3VmxReportProcBasedCtls2Msr(&pVM->hm.s.vmx.Msrs.ProcCtls2);

    hmR3VmxReportEntryCtlsMsr(&pVM->hm.s.vmx.Msrs.EntryCtls);
    hmR3VmxReportExitCtlsMsr(&pVM->hm.s.vmx.Msrs.ExitCtls);

    if (RT_BF_GET(pVM->hm.s.vmx.Msrs.u64Basic, VMX_BF_BASIC_TRUE_CTLS))
    {
        /* We don't extensively dump the true capability MSRs as we don't use them, see @bugref{9180#c5}. */
        LogRel(("HM: MSR_IA32_VMX_TRUE_PINBASED_CTLS   = %#RX64\n", pVM->hm.s.vmx.Msrs.TruePinCtls));
        LogRel(("HM: MSR_IA32_VMX_TRUE_PROCBASED_CTLS  = %#RX64\n", pVM->hm.s.vmx.Msrs.TrueProcCtls));
        LogRel(("HM: MSR_IA32_VMX_TRUE_ENTRY_CTLS      = %#RX64\n", pVM->hm.s.vmx.Msrs.TrueEntryCtls));
        LogRel(("HM: MSR_IA32_VMX_TRUE_EXIT_CTLS       = %#RX64\n", pVM->hm.s.vmx.Msrs.TrueExitCtls));
    }

    hmR3VmxReportMiscMsr(pVM, pVM->hm.s.vmx.Msrs.u64Misc);
    hmR3VmxReportVmcsEnumMsr(pVM->hm.s.vmx.Msrs.u64VmcsEnum);
    if (pVM->hm.s.vmx.Msrs.u64EptVpidCaps)
        hmR3VmxReportEptVpidCapsMsr(pVM->hm.s.vmx.Msrs.u64EptVpidCaps);
    if (pVM->hm.s.vmx.Msrs.u64VmFunc)
        hmR3VmxReportVmFuncMsr(pVM->hm.s.vmx.Msrs.u64VmFunc);
    hmR3VmxReportCrFixedMsrs(&pVM->hm.s.vmx.Msrs);

    LogRel(("HM: APIC-access page physaddr         = %#RHp\n",  pVM->hm.s.vmx.HCPhysApicAccess));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        LogRel(("HM: VCPU%3d: MSR bitmap physaddr      = %#RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysMsrBitmap));
        LogRel(("HM: VCPU%3d: VMCS physaddr            = %#RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysVmcs));
    }

    /*
     * EPT and unrestricted guest execution are determined in HMR3Init, verify the sanity of that.
     */
    AssertLogRelReturn(   !pVM->hm.s.fNestedPaging
                       || (pVM->hm.s.vmx.Msrs.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_EPT),
                       VERR_HM_IPE_1);
    AssertLogRelReturn(   !pVM->hm.s.vmx.fUnrestrictedGuest
                       || (   (pVM->hm.s.vmx.Msrs.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)
                           && pVM->hm.s.fNestedPaging),
                       VERR_HM_IPE_1);

    /*
     * Enable VPID if configured and supported.
     */
    if (pVM->hm.s.vmx.Msrs.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_VPID)
        pVM->hm.s.vmx.fVpid = pVM->hm.s.vmx.fAllowVpid;

#if 0
    /*
     * Enable APIC register virtualization and virtual-interrupt delivery if supported.
     */
    if (   (pVM->hm.s.vmx.Msrs.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_APIC_REG_VIRT)
        && (pVM->hm.s.vmx.Msrs.ProcCtls2.n.allowed1 & VMX_PROC_CTLS2_VIRT_INTR_DELIVERY))
        pVM->hm.s.fVirtApicRegs = true;

    /*
     * Enable posted-interrupt processing if supported.
     */
    /** @todo Add and query IPRT API for host OS support for posted-interrupt IPI
     *        here. */
    if (   (pVM->hm.s.vmx.Msrs.PinCtls.n.allowed1  & VMX_PIN_CTLS_POSTED_INT)
        && (pVM->hm.s.vmx.Msrs.ExitCtls.n.allowed1 & VMX_EXIT_CTLS_ACK_EXT_INT))
        pVM->hm.s.fPostedIntrs = true;
#endif

    /*
     * Disallow RDTSCP in the guest if there is no secondary process-based VM execution controls as otherwise
     * RDTSCP would cause a #UD. There might be no CPUs out there where this happens, as RDTSCP was introduced
     * in Nehalems and secondary VM exec. controls should be supported in all of them, but nonetheless it's Intel...
     */
    if (   !(pVM->hm.s.vmx.Msrs.ProcCtls.n.allowed1 & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
        && CPUMR3GetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP))
    {
        CPUMR3ClearGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP);
        LogRel(("HM: Disabled RDTSCP\n"));
    }

    if (!pVM->hm.s.vmx.fUnrestrictedGuest)
    {
        /* Allocate three pages for the TSS we need for real mode emulation. (2 pages for the IO bitmap) */
        rc = PDMR3VmmDevHeapAlloc(pVM, HM_VTX_TOTAL_DEVHEAP_MEM, hmR3VmmDevHeapNotify, (RTR3PTR *)&pVM->hm.s.vmx.pRealModeTSS);
        if (RT_SUCCESS(rc))
        {
            /* The IO bitmap starts right after the virtual interrupt redirection bitmap.
               Refer Intel spec. 20.3.3 "Software Interrupt Handling in Virtual-8086 mode"
               esp. Figure 20-5.*/
            ASMMemZero32(pVM->hm.s.vmx.pRealModeTSS, sizeof(*pVM->hm.s.vmx.pRealModeTSS));
            pVM->hm.s.vmx.pRealModeTSS->offIoBitmap = sizeof(*pVM->hm.s.vmx.pRealModeTSS);

            /* Bit set to 0 means software interrupts are redirected to the
               8086 program interrupt handler rather than switching to
               protected-mode handler. */
            memset(pVM->hm.s.vmx.pRealModeTSS->IntRedirBitmap, 0, sizeof(pVM->hm.s.vmx.pRealModeTSS->IntRedirBitmap));

            /* Allow all port IO, so that port IO instructions do not cause
               exceptions and would instead cause a VM-exit (based on VT-x's
               IO bitmap which we currently configure to always cause an exit). */
            memset(pVM->hm.s.vmx.pRealModeTSS + 1, 0, PAGE_SIZE * 2);
            *((unsigned char *)pVM->hm.s.vmx.pRealModeTSS + HM_VTX_TSS_SIZE - 2) = 0xff;

            /*
             * Construct a 1024 element page directory with 4 MB pages for the identity mapped
             * page table used in real and protected mode without paging with EPT.
             */
            pVM->hm.s.vmx.pNonPagingModeEPTPageTable = (PX86PD)((char *)pVM->hm.s.vmx.pRealModeTSS + PAGE_SIZE * 3);
            for (uint32_t i = 0; i < X86_PG_ENTRIES; i++)
            {
                pVM->hm.s.vmx.pNonPagingModeEPTPageTable->a[i].u  = _4M * i;
                pVM->hm.s.vmx.pNonPagingModeEPTPageTable->a[i].u |= X86_PDE4M_P | X86_PDE4M_RW | X86_PDE4M_US
                                                                  | X86_PDE4M_A | X86_PDE4M_D | X86_PDE4M_PS
                                                                  | X86_PDE4M_G;
            }

            /* We convert it here every time as PCI regions could be reconfigured. */
            if (PDMVmmDevHeapIsEnabled(pVM))
            {
                RTGCPHYS GCPhys;
                rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pRealModeTSS, &GCPhys);
                AssertRCReturn(rc, rc);
                LogRel(("HM: Real Mode TSS guest physaddr      = %#RGp\n", GCPhys));

                rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                AssertRCReturn(rc, rc);
                LogRel(("HM: Non-Paging Mode EPT CR3           = %#RGp\n", GCPhys));
            }
        }
        else
        {
            LogRel(("HM: No real mode VT-x support (PDMR3VMMDevHeapAlloc returned %Rrc)\n", rc));
            pVM->hm.s.vmx.pRealModeTSS = NULL;
            pVM->hm.s.vmx.pNonPagingModeEPTPageTable = NULL;
            return VMSetError(pVM, rc, RT_SRC_POS,
                              "HM failure: No real mode VT-x support (PDMR3VMMDevHeapAlloc returned %Rrc)", rc);
        }
    }

    LogRel((pVM->hm.s.fAllow64BitGuests
            ? "HM: Guest support: 32-bit and 64-bit\n"
            : "HM: Guest support: 32-bit only\n"));

    /*
     * Call ring-0 to set up the VM.
     */
    rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /* idCpu */, VMMR0_DO_HM_SETUP_VM, 0 /* u64Arg */, NULL /* pReqHdr */);
    if (rc != VINF_SUCCESS)
    {
        AssertMsgFailed(("%Rrc\n", rc));
        LogRel(("HM: VMX setup failed with rc=%Rrc!\n", rc));
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];
            LogRel(("HM: CPU[%u] Last instruction error  %#x\n", i, pVCpu->hm.s.vmx.LastError.u32InstrError));
            LogRel(("HM: CPU[%u] HM error                %#x (%u)\n", i, pVCpu->hm.s.u32HMError, pVCpu->hm.s.u32HMError));
        }
        HMR3CheckError(pVM, rc);
        return VMSetError(pVM, rc, RT_SRC_POS, "VT-x setup failed: %Rrc", rc);
    }

    LogRel(("HM: Supports VMCS EFER fields         = %RTbool\n", pVM->hm.s.vmx.fSupportsVmcsEfer));
    LogRel(("HM: Enabled VMX\n"));
    pVM->hm.s.vmx.fEnabled = true;

    hmR3DisableRawMode(pVM); /** @todo make this go away! */

    /*
     * Change the CPU features.
     */
    CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
    if (pVM->hm.s.fAllow64BitGuests)
    {
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);            /* 64 bits only on Intel CPUs */
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
    }
    /* Turn on NXE if PAE has been enabled *and* the host has turned on NXE
       (we reuse the host EFER in the switcher). */
    /** @todo this needs to be fixed properly!! */
    else if (CPUMR3GetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE))
    {
        if (pVM->hm.s.vmx.u64HostEfer & MSR_K6_EFER_NXE)
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
        else
            LogRel(("HM: NX not enabled on the host, unavailable to PAE guest\n"));
    }

    /*
     * Log configuration details.
     */
    if (pVM->hm.s.fNestedPaging)
    {
        LogRel(("HM: Enabled nested paging\n"));
        if (pVM->hm.s.vmx.enmTlbFlushEpt == VMXTLBFLUSHEPT_SINGLE_CONTEXT)
            LogRel(("HM:   EPT flush type                  = Single context\n"));
        else if (pVM->hm.s.vmx.enmTlbFlushEpt == VMXTLBFLUSHEPT_ALL_CONTEXTS)
            LogRel(("HM:   EPT flush type                  = All contexts\n"));
        else if (pVM->hm.s.vmx.enmTlbFlushEpt == VMXTLBFLUSHEPT_NOT_SUPPORTED)
            LogRel(("HM:   EPT flush type                  = Not supported\n"));
        else
            LogRel(("HM:   EPT flush type                  = %#x\n", pVM->hm.s.vmx.enmTlbFlushEpt));

        if (pVM->hm.s.vmx.fUnrestrictedGuest)
            LogRel(("HM: Enabled unrestricted guest execution\n"));

#if HC_ARCH_BITS == 64
        if (pVM->hm.s.fLargePages)
        {
            /* Use large (2 MB) pages for our EPT PDEs where possible. */
            PGMSetLargePageUsage(pVM, true);
            LogRel(("HM: Enabled large page support\n"));
        }
#endif
    }
    else
        Assert(!pVM->hm.s.vmx.fUnrestrictedGuest);

    if (pVM->hm.s.fVirtApicRegs)
        LogRel(("HM:   Enabled APIC-register virtualization support\n"));

    if (pVM->hm.s.fPostedIntrs)
        LogRel(("HM:   Enabled posted-interrupt processing support\n"));

    if (pVM->hm.s.vmx.fVpid)
    {
        LogRel(("HM: Enabled VPID\n"));
        if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_INDIV_ADDR)
            LogRel(("HM:   VPID flush type                 = Individual addresses\n"));
        else if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_SINGLE_CONTEXT)
            LogRel(("HM:   VPID flush type                 = Single context\n"));
        else if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_ALL_CONTEXTS)
            LogRel(("HM:   VPID flush type                 = All contexts\n"));
        else if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
            LogRel(("HM:   VPID flush type                 = Single context retain globals\n"));
        else
            LogRel(("HM:   VPID flush type                 = %#x\n", pVM->hm.s.vmx.enmTlbFlushVpid));
    }
    else if (pVM->hm.s.vmx.enmTlbFlushVpid == VMXTLBFLUSHVPID_NOT_SUPPORTED)
        LogRel(("HM: Ignoring VPID capabilities of CPU\n"));

    if (pVM->hm.s.vmx.fUsePreemptTimer)
        LogRel(("HM: Enabled VMX-preemption timer (cPreemptTimerShift=%u)\n", pVM->hm.s.vmx.cPreemptTimerShift));
    else
        LogRel(("HM: Disabled VMX-preemption timer\n"));

    return VINF_SUCCESS;
}


/**
 * Finish AMD-V initialization (after ring-0 init).
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 */
static int hmR3InitFinalizeR0Amd(PVM pVM)
{
    Log(("pVM->hm.s.svm.fSupported = %d\n", pVM->hm.s.svm.fSupported));

    LogRel(("HM: Using AMD-V implementation 2.0\n"));

    uint32_t u32Family;
    uint32_t u32Model;
    uint32_t u32Stepping;
    if (HMSvmIsSubjectToErratum170(&u32Family, &u32Model, &u32Stepping))
        LogRel(("HM: AMD Cpu with erratum 170 family %#x model %#x stepping %#x\n", u32Family, u32Model, u32Stepping));
    LogRel(("HM: Max resume loops                  = %u\n",     pVM->hm.s.cMaxResumeLoops));
    LogRel(("HM: AMD HWCR MSR                      = %#RX64\n", pVM->hm.s.svm.u64MsrHwcr));
    LogRel(("HM: AMD-V revision                    = %#x\n",    pVM->hm.s.svm.u32Rev));
    LogRel(("HM: AMD-V max ASID                    = %RU32\n",  pVM->hm.s.uMaxAsid));
    LogRel(("HM: AMD-V features                    = %#x\n",    pVM->hm.s.svm.u32Features));

    /*
     * Enumerate AMD-V features.
     */
    static const struct { uint32_t fFlag; const char *pszName; } s_aSvmFeatures[] =
    {
#define HMSVM_REPORT_FEATURE(a_StrDesc, a_Define) { a_Define, a_StrDesc }
        HMSVM_REPORT_FEATURE("NESTED_PAGING",          X86_CPUID_SVM_FEATURE_EDX_NESTED_PAGING),
        HMSVM_REPORT_FEATURE("LBR_VIRT",               X86_CPUID_SVM_FEATURE_EDX_LBR_VIRT),
        HMSVM_REPORT_FEATURE("SVM_LOCK",               X86_CPUID_SVM_FEATURE_EDX_SVM_LOCK),
        HMSVM_REPORT_FEATURE("NRIP_SAVE",              X86_CPUID_SVM_FEATURE_EDX_NRIP_SAVE),
        HMSVM_REPORT_FEATURE("TSC_RATE_MSR",           X86_CPUID_SVM_FEATURE_EDX_TSC_RATE_MSR),
        HMSVM_REPORT_FEATURE("VMCB_CLEAN",             X86_CPUID_SVM_FEATURE_EDX_VMCB_CLEAN),
        HMSVM_REPORT_FEATURE("FLUSH_BY_ASID",          X86_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID),
        HMSVM_REPORT_FEATURE("DECODE_ASSISTS",         X86_CPUID_SVM_FEATURE_EDX_DECODE_ASSISTS),
        HMSVM_REPORT_FEATURE("PAUSE_FILTER",           X86_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER),
        HMSVM_REPORT_FEATURE("PAUSE_FILTER_THRESHOLD", X86_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER_THRESHOLD),
        HMSVM_REPORT_FEATURE("AVIC",                   X86_CPUID_SVM_FEATURE_EDX_AVIC),
        HMSVM_REPORT_FEATURE("VIRT_VMSAVE_VMLOAD",     X86_CPUID_SVM_FEATURE_EDX_VIRT_VMSAVE_VMLOAD),
        HMSVM_REPORT_FEATURE("VGIF",                   X86_CPUID_SVM_FEATURE_EDX_VGIF),
#undef HMSVM_REPORT_FEATURE
    };

    uint32_t fSvmFeatures = pVM->hm.s.svm.u32Features;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aSvmFeatures); i++)
        if (fSvmFeatures & s_aSvmFeatures[i].fFlag)
        {
            LogRel(("HM:   %s\n", s_aSvmFeatures[i].pszName));
            fSvmFeatures &= ~s_aSvmFeatures[i].fFlag;
        }
    if (fSvmFeatures)
        for (unsigned iBit = 0; iBit < 32; iBit++)
            if (RT_BIT_32(iBit) & fSvmFeatures)
                LogRel(("HM:   Reserved bit %u\n", iBit));

    /*
     * Nested paging is determined in HMR3Init, verify the sanity of that.
     */
    AssertLogRelReturn(   !pVM->hm.s.fNestedPaging
                       || (pVM->hm.s.svm.u32Features & X86_CPUID_SVM_FEATURE_EDX_NESTED_PAGING),
                       VERR_HM_IPE_1);

#if 0
    /** @todo Add and query IPRT API for host OS support for posted-interrupt IPI
     *        here. */
    if (RTR0IsPostIpiSupport())
        pVM->hm.s.fPostedIntrs = true;
#endif

    /*
     * Call ring-0 to set up the VM.
     */
    int rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_SETUP_VM, 0, NULL);
    if (rc != VINF_SUCCESS)
    {
        AssertMsgFailed(("%Rrc\n", rc));
        LogRel(("HM: AMD-V setup failed with rc=%Rrc!\n", rc));
        return VMSetError(pVM, rc, RT_SRC_POS, "AMD-V setup failed: %Rrc", rc);
    }

    LogRel(("HM: Enabled SVM\n"));
    pVM->hm.s.svm.fEnabled = true;

    if (pVM->hm.s.fNestedPaging)
    {
        LogRel(("HM:   Enabled nested paging\n"));

        /*
         * Enable large pages (2 MB) if applicable.
         */
#if HC_ARCH_BITS == 64
        if (pVM->hm.s.fLargePages)
        {
            PGMSetLargePageUsage(pVM, true);
            LogRel(("HM:   Enabled large page support\n"));
        }
#endif
    }

    if (pVM->hm.s.fVirtApicRegs)
        LogRel(("HM:   Enabled APIC-register virtualization support\n"));

    if (pVM->hm.s.fPostedIntrs)
        LogRel(("HM:   Enabled posted-interrupt processing support\n"));

    hmR3DisableRawMode(pVM);

    /*
     * Change the CPU features.
     */
    CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
    CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);
    if (pVM->hm.s.fAllow64BitGuests)
    {
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
    }
    /* Turn on NXE if PAE has been enabled. */
    else if (CPUMR3GetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE))
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);

    LogRel(("HM: %s TPR patching\n", (pVM->hm.s.fTprPatchingAllowed) ? "Enabled" : "Disabled"));

    LogRel((pVM->hm.s.fAllow64BitGuests
            ? "HM: Guest support: 32-bit and 64-bit\n"
            : "HM: Guest support: 32-bit only\n"));

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) HMR3Relocate(PVM pVM)
{
    Log(("HMR3Relocate to %RGv\n", MMHyperGetArea(pVM, 0)));

    /* Fetch the current paging mode during the relocate callback during state loading. */
    if (VMR3GetState(pVM) == VMSTATE_LOADING)
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];
            pVCpu->hm.s.enmShadowMode = PGMGetShadowMode(pVCpu);
        }
    }
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS)
    if (HMIsEnabled(pVM))
    {
        switch (PGMGetHostMode(pVM))
        {
            case PGMMODE_32_BIT:
                pVM->hm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_32_TO_AMD64);
                break;

            case PGMMODE_PAE:
            case PGMMODE_PAE_NX:
                pVM->hm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_PAE_TO_AMD64);
                break;

            default:
                AssertFailed();
                break;
        }
    }
#endif
    return;
}


/**
 * Terminates the HM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is, at this point, powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) HMR3Term(PVM pVM)
{
    if (pVM->hm.s.vmx.pRealModeTSS)
    {
        PDMR3VmmDevHeapFree(pVM, pVM->hm.s.vmx.pRealModeTSS);
        pVM->hm.s.vmx.pRealModeTSS       = 0;
    }
    hmR3TermCPU(pVM);
    return 0;
}


/**
 * Terminates the per-VCPU HM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
static int hmR3TermCPU(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i]; NOREF(pVCpu);

#ifdef VBOX_WITH_STATISTICS
        if (pVCpu->hm.s.paStatExitReason)
        {
            MMHyperFree(pVM, pVCpu->hm.s.paStatExitReason);
            pVCpu->hm.s.paStatExitReason   = NULL;
            pVCpu->hm.s.paStatExitReasonR0 = NIL_RTR0PTR;
        }
        if (pVCpu->hm.s.paStatInjectedIrqs)
        {
            MMHyperFree(pVM, pVCpu->hm.s.paStatInjectedIrqs);
            pVCpu->hm.s.paStatInjectedIrqs   = NULL;
            pVCpu->hm.s.paStatInjectedIrqsR0 = NIL_RTR0PTR;
        }
#endif

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        memset(pVCpu->hm.s.vmx.VMCSCache.aMagic, 0, sizeof(pVCpu->hm.s.vmx.VMCSCache.aMagic));
        pVCpu->hm.s.vmx.VMCSCache.uMagic = 0;
        pVCpu->hm.s.vmx.VMCSCache.uPos = 0xffffffff;
#endif
    }
    return 0;
}


/**
 * Resets a virtual CPU.
 *
 * Used by HMR3Reset and CPU hot plugging.
 *
 * @param   pVCpu   The cross context virtual CPU structure to reset.
 */
VMMR3_INT_DECL(void) HMR3ResetCpu(PVMCPU pVCpu)
{
    /* Sync. entire state on VM reset R0-reentry. It's safe to reset
       the HM flags here, all other EMTs are in ring-3. See VMR3Reset(). */
    pVCpu->hm.s.fCtxChanged |= HM_CHANGED_HOST_CONTEXT | HM_CHANGED_ALL_GUEST;

    pVCpu->hm.s.fActive               = false;
    pVCpu->hm.s.Event.fPending        = false;
    pVCpu->hm.s.vmx.fWasInRealMode    = true;
    pVCpu->hm.s.vmx.u64MsrApicBase    = 0;
    pVCpu->hm.s.vmx.fSwitchedTo64on32 = false;

    /* Reset the contents of the read cache. */
    PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
    for (unsigned j = 0; j < pCache->Read.cValidEntries; j++)
        pCache->Read.aFieldVal[j] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
    pCache->uMagic = UINT64_C(0xdeadbeefdeadbeef);
#endif
}


/**
 * The VM is being reset.
 *
 * For the HM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) HMR3Reset(PVM pVM)
{
    LogFlow(("HMR3Reset:\n"));

    if (HMIsEnabled(pVM))
        hmR3DisableRawMode(pVM);

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        HMR3ResetCpu(pVCpu);
    }

    /* Clear all patch information. */
    pVM->hm.s.pGuestPatchMem     = 0;
    pVM->hm.s.pFreeGuestPatchMem = 0;
    pVM->hm.s.cbGuestPatchMem    = 0;
    pVM->hm.s.cPatches           = 0;
    pVM->hm.s.PatchTree          = 0;
    pVM->hm.s.fTPRPatchingActive = false;
    ASMMemZero32(pVM->hm.s.aPatches, sizeof(pVM->hm.s.aPatches));
}


/**
 * Callback to patch a TPR instruction (vmmcall or mov cr8).
 *
 * @returns VBox strict status code.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pvUser  Unused.
 */
static DECLCALLBACK(VBOXSTRICTRC) hmR3RemovePatches(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    VMCPUID idCpu = (VMCPUID)(uintptr_t)pvUser;

    /* Only execute the handler on the VCPU the original patch request was issued. */
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    Log(("hmR3RemovePatches\n"));
    for (unsigned i = 0; i < pVM->hm.s.cPatches; i++)
    {
        uint8_t         abInstr[15];
        PHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];
        RTGCPTR         pInstrGC = (RTGCPTR)pPatch->Core.Key;
        int             rc;

#ifdef LOG_ENABLED
        char            szOutput[256];
        rc = DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, CPUMGetGuestCS(pVCpu), pInstrGC, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                szOutput, sizeof(szOutput), NULL);
        if (RT_SUCCESS(rc))
            Log(("Patched instr: %s\n", szOutput));
#endif

        /* Check if the instruction is still the same. */
        rc = PGMPhysSimpleReadGCPtr(pVCpu, abInstr, pInstrGC, pPatch->cbNewOp);
        if (rc != VINF_SUCCESS)
        {
            Log(("Patched code removed? (rc=%Rrc0\n", rc));
            continue;   /* swapped out or otherwise removed; skip it. */
        }

        if (memcmp(abInstr, pPatch->aNewOpcode, pPatch->cbNewOp))
        {
            Log(("Patched instruction was changed! (rc=%Rrc0\n", rc));
            continue;   /* skip it. */
        }

        rc = PGMPhysSimpleWriteGCPtr(pVCpu, pInstrGC, pPatch->aOpcode, pPatch->cbOp);
        AssertRC(rc);

#ifdef LOG_ENABLED
        rc = DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, CPUMGetGuestCS(pVCpu), pInstrGC, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                szOutput, sizeof(szOutput), NULL);
        if (RT_SUCCESS(rc))
            Log(("Original instr: %s\n", szOutput));
#endif
    }
    pVM->hm.s.cPatches           = 0;
    pVM->hm.s.PatchTree          = 0;
    pVM->hm.s.pFreeGuestPatchMem = pVM->hm.s.pGuestPatchMem;
    pVM->hm.s.fTPRPatchingActive = false;
    return VINF_SUCCESS;
}


/**
 * Worker for enabling patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   idCpu       VCPU to execute hmR3RemovePatches on.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
static int hmR3EnablePatching(PVM pVM, VMCPUID idCpu, RTRCPTR pPatchMem, unsigned cbPatchMem)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hmR3RemovePatches, (void *)(uintptr_t)idCpu);
    AssertRC(rc);

    pVM->hm.s.pGuestPatchMem      = pPatchMem;
    pVM->hm.s.pFreeGuestPatchMem  = pPatchMem;
    pVM->hm.s.cbGuestPatchMem     = cbPatchMem;
    return VINF_SUCCESS;
}


/**
 * Enable patching in a VT-x/AMD-V guest
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
VMMR3_INT_DECL(int)  HMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    VM_ASSERT_EMT(pVM);
    Log(("HMR3EnablePatching %RGv size %x\n", pPatchMem, cbPatchMem));
    if (pVM->cCpus > 1)
    {
        /* We own the IOM lock here and could cause a deadlock by waiting for a VCPU that is blocking on the IOM lock. */
        int rc = VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE,
                                   (PFNRT)hmR3EnablePatching, 4, pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
        AssertRC(rc);
        return rc;
    }
    return hmR3EnablePatching(pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
}


/**
 * Disable patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
VMMR3_INT_DECL(int)  HMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    Log(("HMR3DisablePatching %RGv size %x\n", pPatchMem, cbPatchMem));
    RT_NOREF2(pPatchMem, cbPatchMem);

    Assert(pVM->hm.s.pGuestPatchMem == pPatchMem);
    Assert(pVM->hm.s.cbGuestPatchMem == cbPatchMem);

    /** @todo Potential deadlock when other VCPUs are waiting on the IOM lock (we own it)!! */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hmR3RemovePatches,
                                (void *)(uintptr_t)VMMGetCpuId(pVM));
    AssertRC(rc);

    pVM->hm.s.pGuestPatchMem      = 0;
    pVM->hm.s.pFreeGuestPatchMem  = 0;
    pVM->hm.s.cbGuestPatchMem     = 0;
    pVM->hm.s.fTPRPatchingActive  = false;
    return VINF_SUCCESS;
}


/**
 * Callback to patch a TPR instruction (vmmcall or mov cr8).
 *
 * @returns VBox strict status code.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pvUser  User specified CPU context.
 *
 */
static DECLCALLBACK(VBOXSTRICTRC) hmR3ReplaceTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    /*
     * Only execute the handler on the VCPU the original patch request was
     * issued. (The other CPU(s) might not yet have switched to protected
     * mode, nor have the correct memory context.)
     */
    VMCPUID idCpu = (VMCPUID)(uintptr_t)pvUser;
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    /*
     * We're racing other VCPUs here, so don't try patch the instruction twice
     * and make sure there is still room for our patch record.
     */
    PCPUMCTX    pCtx   = &pVCpu->cpum.GstCtx;
    PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hmR3ReplaceTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const idx = pVM->hm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        Log(("hmR3ReplaceTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hm.s.aPatches[idx];

    Log(("hmR3ReplaceTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));

    /*
     * Disassembler the instruction and get cracking.
     */
    DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, "hmR3ReplaceTprInstr");
    PDISCPUSTATE    pDis = &pVCpu->hm.s.DisState;
    uint32_t        cbOp;
    int rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    AssertRC(rc);
    if (    rc == VINF_SUCCESS
        &&  pDis->pCurInstr->uOpcode == OP_MOV
        &&  cbOp >= 3)
    {
        static uint8_t const s_abVMMCall[3] = { 0x0f, 0x01, 0xd9 };

        rc = PGMPhysSimpleReadGCPtr(pVCpu, pPatch->aOpcode, pCtx->rip, cbOp);
        AssertRC(rc);

        pPatch->cbOp = cbOp;

        if (pDis->Param1.fUse == DISUSE_DISPLACEMENT32)
        {
            /* write. */
            if (pDis->Param2.fUse == DISUSE_REG_GEN32)
            {
                pPatch->enmType     = HMTPRINSTR_WRITE_REG;
                pPatch->uSrcOperand = pDis->Param2.Base.idxGenReg;
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_WRITE_REG %u\n", pDis->Param2.Base.idxGenReg));
            }
            else
            {
                Assert(pDis->Param2.fUse == DISUSE_IMMEDIATE32);
                pPatch->enmType     = HMTPRINSTR_WRITE_IMM;
                pPatch->uSrcOperand = pDis->Param2.uValue;
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_WRITE_IMM %#llx\n", pDis->Param2.uValue));
            }
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, s_abVMMCall, sizeof(s_abVMMCall));
            AssertRC(rc);

            memcpy(pPatch->aNewOpcode, s_abVMMCall, sizeof(s_abVMMCall));
            pPatch->cbNewOp = sizeof(s_abVMMCall);
            STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceSuccessVmc);
        }
        else
        {
            /*
             * TPR Read.
             *
             * Found:
             *   mov eax, dword [fffe0080]        (5 bytes)
             * Check if next instruction is:
             *   shr eax, 4
             */
            Assert(pDis->Param1.fUse == DISUSE_REG_GEN32);

            uint8_t  const idxMmioReg = pDis->Param1.Base.idxGenReg;
            uint8_t  const cbOpMmio   = cbOp;
            uint64_t const uSavedRip  = pCtx->rip;

            pCtx->rip += cbOp;
            rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
            DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, "Following read");
            pCtx->rip = uSavedRip;

            if (    rc == VINF_SUCCESS
                &&  pDis->pCurInstr->uOpcode == OP_SHR
                &&  pDis->Param1.fUse == DISUSE_REG_GEN32
                &&  pDis->Param1.Base.idxGenReg == idxMmioReg
                &&  pDis->Param2.fUse == DISUSE_IMMEDIATE8
                &&  pDis->Param2.uValue == 4
                &&  cbOpMmio + cbOp < sizeof(pVM->hm.s.aPatches[idx].aOpcode))
            {
                uint8_t abInstr[15];

                /* Replacing the two instructions above with an AMD-V specific lock-prefixed 32-bit MOV CR8 instruction so as to
                   access CR8 in 32-bit mode and not cause a #VMEXIT. */
                rc = PGMPhysSimpleReadGCPtr(pVCpu, &pPatch->aOpcode, pCtx->rip, cbOpMmio + cbOp);
                AssertRC(rc);

                pPatch->cbOp = cbOpMmio + cbOp;

                /* 0xf0, 0x0f, 0x20, 0xc0 = mov eax, cr8 */
                abInstr[0] = 0xf0;
                abInstr[1] = 0x0f;
                abInstr[2] = 0x20;
                abInstr[3] = 0xc0 | pDis->Param1.Base.idxGenReg;
                for (unsigned i = 4; i < pPatch->cbOp; i++)
                    abInstr[i] = 0x90;  /* nop */

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, abInstr, pPatch->cbOp);
                AssertRC(rc);

                memcpy(pPatch->aNewOpcode, abInstr, pPatch->cbOp);
                pPatch->cbNewOp = pPatch->cbOp;
                STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceSuccessCr8);

                Log(("Acceptable read/shr candidate!\n"));
                pPatch->enmType = HMTPRINSTR_READ_SHR4;
            }
            else
            {
                pPatch->enmType     = HMTPRINSTR_READ;
                pPatch->uDstOperand = idxMmioReg;

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, s_abVMMCall, sizeof(s_abVMMCall));
                AssertRC(rc);

                memcpy(pPatch->aNewOpcode, s_abVMMCall, sizeof(s_abVMMCall));
                pPatch->cbNewOp = sizeof(s_abVMMCall);
                STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceSuccessVmc);
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_READ %u\n", pPatch->uDstOperand));
            }
        }

        pPatch->Core.Key = pCtx->eip;
        rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
        AssertRC(rc);

        pVM->hm.s.cPatches++;
        return VINF_SUCCESS;
    }

    /*
     * Save invalid patch, so we will not try again.
     */
    Log(("hmR3ReplaceTprInstr: Failed to patch instr!\n"));
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceFailure);
    return VINF_SUCCESS;
}


/**
 * Callback to patch a TPR instruction (jump to generated code).
 *
 * @returns VBox strict status code.
 * @param   pVM     The cross context VM structure.
 * @param   pVCpu   The cross context virtual CPU structure of the calling EMT.
 * @param   pvUser  User specified CPU context.
 *
 */
static DECLCALLBACK(VBOXSTRICTRC) hmR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    /*
     * Only execute the handler on the VCPU the original patch request was
     * issued. (The other CPU(s) might not yet have switched to protected
     * mode, nor have the correct memory context.)
     */
    VMCPUID         idCpu  = (VMCPUID)(uintptr_t)pvUser;
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    /*
     * We're racing other VCPUs here, so don't try patch the instruction twice
     * and make sure there is still room for our patch record.
     */
    PCPUMCTX    pCtx   = &pVCpu->cpum.GstCtx;
    PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hmR3PatchTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const  idx = pVM->hm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        Log(("hmR3PatchTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hm.s.aPatches[idx];

    Log(("hmR3PatchTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));
    DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, "hmR3PatchTprInstr");

    /*
     * Disassemble the instruction and get cracking.
     */
    PDISCPUSTATE    pDis   = &pVCpu->hm.s.DisState;
    uint32_t        cbOp;
    int rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    AssertRC(rc);
    if (    rc == VINF_SUCCESS
        &&  pDis->pCurInstr->uOpcode == OP_MOV
        &&  cbOp >= 5)
    {
        uint8_t         aPatch[64];
        uint32_t        off = 0;

        rc = PGMPhysSimpleReadGCPtr(pVCpu, pPatch->aOpcode, pCtx->rip, cbOp);
        AssertRC(rc);

        pPatch->cbOp    = cbOp;
        pPatch->enmType = HMTPRINSTR_JUMP_REPLACEMENT;

        if (pDis->Param1.fUse == DISUSE_DISPLACEMENT32)
        {
            /*
             * TPR write:
             *
             * push ECX                      [51]
             * push EDX                      [52]
             * push EAX                      [50]
             * xor EDX,EDX                   [31 D2]
             * mov EAX,EAX                   [89 C0]
             *  or
             * mov EAX,0000000CCh            [B8 CC 00 00 00]
             * mov ECX,0C0000082h            [B9 82 00 00 C0]
             * wrmsr                         [0F 30]
             * pop EAX                       [58]
             * pop EDX                       [5A]
             * pop ECX                       [59]
             * jmp return_address            [E9 return_address]
             */
            bool fUsesEax = (pDis->Param2.fUse == DISUSE_REG_GEN32 && pDis->Param2.Base.idxGenReg == DISGREG_EAX);

            aPatch[off++] = 0x51;    /* push ecx */
            aPatch[off++] = 0x52;    /* push edx */
            if (!fUsesEax)
                aPatch[off++] = 0x50;    /* push eax */
            aPatch[off++] = 0x31;    /* xor edx, edx */
            aPatch[off++] = 0xd2;
            if (pDis->Param2.fUse == DISUSE_REG_GEN32)
            {
                if (!fUsesEax)
                {
                    aPatch[off++] = 0x89;    /* mov eax, src_reg */
                    aPatch[off++] = MAKE_MODRM(3, pDis->Param2.Base.idxGenReg, DISGREG_EAX);
                }
            }
            else
            {
                Assert(pDis->Param2.fUse == DISUSE_IMMEDIATE32);
                aPatch[off++] = 0xb8;    /* mov eax, immediate */
                *(uint32_t *)&aPatch[off] = pDis->Param2.uValue;
                off += sizeof(uint32_t);
            }
            aPatch[off++] = 0xb9;    /* mov ecx, 0xc0000082 */
            *(uint32_t *)&aPatch[off] = MSR_K8_LSTAR;
            off += sizeof(uint32_t);

            aPatch[off++] = 0x0f;    /* wrmsr */
            aPatch[off++] = 0x30;
            if (!fUsesEax)
                aPatch[off++] = 0x58;    /* pop eax */
            aPatch[off++] = 0x5a;    /* pop edx */
            aPatch[off++] = 0x59;    /* pop ecx */
        }
        else
        {
            /*
             * TPR read:
             *
             * push ECX                      [51]
             * push EDX                      [52]
             * push EAX                      [50]
             * mov ECX,0C0000082h            [B9 82 00 00 C0]
             * rdmsr                         [0F 32]
             * mov EAX,EAX                   [89 C0]
             * pop EAX                       [58]
             * pop EDX                       [5A]
             * pop ECX                       [59]
             * jmp return_address            [E9 return_address]
             */
            Assert(pDis->Param1.fUse == DISUSE_REG_GEN32);

            if (pDis->Param1.Base.idxGenReg != DISGREG_ECX)
                aPatch[off++] = 0x51;    /* push ecx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EDX )
                aPatch[off++] = 0x52;    /* push edx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
                aPatch[off++] = 0x50;    /* push eax */

            aPatch[off++] = 0x31;    /* xor edx, edx */
            aPatch[off++] = 0xd2;

            aPatch[off++] = 0xb9;    /* mov ecx, 0xc0000082 */
            *(uint32_t *)&aPatch[off] = MSR_K8_LSTAR;
            off += sizeof(uint32_t);

            aPatch[off++] = 0x0f;    /* rdmsr */
            aPatch[off++] = 0x32;

            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
            {
                aPatch[off++] = 0x89;    /* mov dst_reg, eax */
                aPatch[off++] = MAKE_MODRM(3, DISGREG_EAX, pDis->Param1.Base.idxGenReg);
            }

            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
                aPatch[off++] = 0x58;    /* pop eax */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EDX )
                aPatch[off++] = 0x5a;    /* pop edx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_ECX)
                aPatch[off++] = 0x59;    /* pop ecx */
        }
        aPatch[off++] = 0xe9;    /* jmp return_address */
        *(RTRCUINTPTR *)&aPatch[off] = ((RTRCUINTPTR)pCtx->eip + cbOp) - ((RTRCUINTPTR)pVM->hm.s.pFreeGuestPatchMem + off + 4);
        off += sizeof(RTRCUINTPTR);

        if (pVM->hm.s.pFreeGuestPatchMem + off <= pVM->hm.s.pGuestPatchMem + pVM->hm.s.cbGuestPatchMem)
        {
            /* Write new code to the patch buffer. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pVM->hm.s.pFreeGuestPatchMem, aPatch, off);
            AssertRC(rc);

#ifdef LOG_ENABLED
            uint32_t cbCurInstr;
            for (RTGCPTR GCPtrInstr = pVM->hm.s.pFreeGuestPatchMem;
                 GCPtrInstr < pVM->hm.s.pFreeGuestPatchMem + off;
                 GCPtrInstr += RT_MAX(cbCurInstr, 1))
            {
                char     szOutput[256];
                rc = DBGFR3DisasInstrEx(pVM->pUVM, pVCpu->idCpu, pCtx->cs.Sel, GCPtrInstr, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                        szOutput, sizeof(szOutput), &cbCurInstr);
                if (RT_SUCCESS(rc))
                    Log(("Patch instr %s\n", szOutput));
                else
                    Log(("%RGv: rc=%Rrc\n", GCPtrInstr, rc));
            }
#endif

            pPatch->aNewOpcode[0] = 0xE9;
            *(RTRCUINTPTR *)&pPatch->aNewOpcode[1] = ((RTRCUINTPTR)pVM->hm.s.pFreeGuestPatchMem) - ((RTRCUINTPTR)pCtx->eip + 5);

            /* Overwrite the TPR instruction with a jump. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->eip, pPatch->aNewOpcode, 5);
            AssertRC(rc);

            DBGFR3_DISAS_INSTR_CUR_LOG(pVCpu, "Jump");

            pVM->hm.s.pFreeGuestPatchMem += off;
            pPatch->cbNewOp = 5;

            pPatch->Core.Key = pCtx->eip;
            rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
            AssertRC(rc);

            pVM->hm.s.cPatches++;
            pVM->hm.s.fTPRPatchingActive = true;
            STAM_COUNTER_INC(&pVM->hm.s.StatTprPatchSuccess);
            return VINF_SUCCESS;
        }

        Log(("Ran out of space in our patch buffer!\n"));
    }
    else
        Log(("hmR3PatchTprInstr: Failed to patch instr!\n"));


    /*
     * Save invalid patch, so we will not try again.
     */
    pPatch = &pVM->hm.s.aPatches[idx];
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hm.s.StatTprPatchFailure);
    return VINF_SUCCESS;
}


/**
 * Attempt to patch TPR mmio instructions.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(int) HMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE,
                                pVM->hm.s.pGuestPatchMem ? hmR3PatchTprInstr : hmR3ReplaceTprInstr,
                                (void *)(uintptr_t)pVCpu->idCpu);
    AssertRC(rc);
    return rc;
}


/**
 * Checks if we need to reschedule due to VMM device heap changes.
 *
 * @returns true if a reschedule is required, otherwise false.
 * @param   pVM         The cross context VM structure.
 * @param   pCtx        VM execution context.
 */
VMMR3_INT_DECL(bool) HMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx)
{
    /*
     * The VMM device heap is a requirement for emulating real-mode or protected-mode without paging
     * when the unrestricted guest execution feature is missing (VT-x only).
     */
    if (    pVM->hm.s.vmx.fEnabled
        && !pVM->hm.s.vmx.fUnrestrictedGuest
        &&  CPUMIsGuestInRealModeEx(pCtx)
        && !PDMVmmDevHeapIsEnabled(pVM))
        return true;

    return false;
}


/**
 * Noticiation callback from DBGF when interrupt breakpoints or generic debug
 * event settings changes.
 *
 * DBGF will call HMR3NotifyDebugEventChangedPerCpu on each CPU afterwards, this
 * function is just updating the VM globals.
 *
 * @param   pVM         The VM cross context VM structure.
 * @thread  EMT(0)
 */
VMMR3_INT_DECL(void) HMR3NotifyDebugEventChanged(PVM pVM)
{
    /* Interrupts. */
    bool fUseDebugLoop = pVM->dbgf.ro.cSoftIntBreakpoints > 0
                      || pVM->dbgf.ro.cHardIntBreakpoints > 0;

    /* CPU Exceptions. */
    for (DBGFEVENTTYPE enmEvent = DBGFEVENT_XCPT_FIRST;
         !fUseDebugLoop && enmEvent <= DBGFEVENT_XCPT_LAST;
         enmEvent = (DBGFEVENTTYPE)(enmEvent + 1))
        fUseDebugLoop = DBGF_IS_EVENT_ENABLED(pVM, enmEvent);

    /* Common VM exits. */
    for (DBGFEVENTTYPE enmEvent = DBGFEVENT_EXIT_FIRST;
         !fUseDebugLoop && enmEvent <= DBGFEVENT_EXIT_LAST_COMMON;
         enmEvent = (DBGFEVENTTYPE)(enmEvent + 1))
        fUseDebugLoop = DBGF_IS_EVENT_ENABLED(pVM, enmEvent);

    /* Vendor specific VM exits. */
    if (HMR3IsVmxEnabled(pVM->pUVM))
        for (DBGFEVENTTYPE enmEvent = DBGFEVENT_EXIT_VMX_FIRST;
             !fUseDebugLoop && enmEvent <= DBGFEVENT_EXIT_VMX_LAST;
             enmEvent = (DBGFEVENTTYPE)(enmEvent + 1))
            fUseDebugLoop = DBGF_IS_EVENT_ENABLED(pVM, enmEvent);
    else
        for (DBGFEVENTTYPE enmEvent = DBGFEVENT_EXIT_SVM_FIRST;
             !fUseDebugLoop && enmEvent <= DBGFEVENT_EXIT_SVM_LAST;
             enmEvent = (DBGFEVENTTYPE)(enmEvent + 1))
            fUseDebugLoop = DBGF_IS_EVENT_ENABLED(pVM, enmEvent);

    /* Done. */
    pVM->hm.s.fUseDebugLoop = fUseDebugLoop;
}


/**
 * Follow up notification callback to HMR3NotifyDebugEventChanged for each CPU.
 *
 * HM uses this to combine the decision made by HMR3NotifyDebugEventChanged with
 * per CPU settings.
 *
 * @param   pVM         The VM cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure of the calling EMT.
 */
VMMR3_INT_DECL(void) HMR3NotifyDebugEventChangedPerCpu(PVM pVM, PVMCPU pVCpu)
{
    pVCpu->hm.s.fUseDebugLoop = pVCpu->hm.s.fSingleInstruction | pVM->hm.s.fUseDebugLoop;
}


/**
 * Checks if we are currently using hardware acceleration.
 *
 * @returns true if hardware acceleration is being used, otherwise false.
 * @param   pVCpu        The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(bool) HMR3IsActive(PVMCPU pVCpu)
{
    return pVCpu->hm.s.fActive;
}


/**
 * External interface for querying whether hardware acceleration is enabled.
 *
 * @returns true if VT-x or AMD-V is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 * @sa      HMIsEnabled, HMIsEnabledNotMacro.
 */
VMMR3DECL(bool) HMR3IsEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->fHMEnabled; /* Don't use the macro as the GUI may query us very very early. */
}


/**
 * External interface for querying whether VT-x is being used.
 *
 * @returns true if VT-x is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 * @sa      HMR3IsSvmEnabled, HMIsEnabled
 */
VMMR3DECL(bool) HMR3IsVmxEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.vmx.fEnabled
        && pVM->hm.s.vmx.fSupported
        && pVM->fHMEnabled;
}


/**
 * External interface for querying whether AMD-V is being used.
 *
 * @returns true if VT-x is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 * @sa      HMR3IsVmxEnabled, HMIsEnabled
 */
VMMR3DECL(bool) HMR3IsSvmEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.svm.fEnabled
        && pVM->hm.s.svm.fSupported
        && pVM->fHMEnabled;
}


/**
 * Checks if we are currently using nested paging.
 *
 * @returns true if nested paging is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 */
VMMR3DECL(bool) HMR3IsNestedPagingActive(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.fNestedPaging;
}


/**
 * Checks if virtualized APIC registers is enabled.
 *
 * When enabled this feature allows the hardware to access most of the
 * APIC registers in the virtual-APIC page without causing VM-exits. See
 * Intel spec. 29.1.1 "Virtualized APIC Registers".
 *
 * @returns true if virtualized APIC registers is enabled, otherwise
 *          false.
 * @param   pUVM        The user mode VM handle.
 */
VMMR3DECL(bool) HMR3IsVirtApicRegsEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.fVirtApicRegs;
}


/**
 * Checks if APIC posted-interrupt processing is enabled.
 *
 * This returns whether we can deliver interrupts to the guest without
 * leaving guest-context by updating APIC state from host-context.
 *
 * @returns true if APIC posted-interrupt processing is enabled,
 *          otherwise false.
 * @param   pUVM        The user mode VM handle.
 */
VMMR3DECL(bool) HMR3IsPostedIntrsEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.fPostedIntrs;
}


/**
 * Checks if we are currently using VPID in VT-x mode.
 *
 * @returns true if VPID is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 */
VMMR3DECL(bool) HMR3IsVpidActive(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.vmx.fVpid;
}


/**
 * Checks if we are currently using VT-x unrestricted execution,
 * aka UX.
 *
 * @returns true if UX is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 */
VMMR3DECL(bool) HMR3IsUXActive(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return pVM->hm.s.vmx.fUnrestrictedGuest
        || pVM->hm.s.svm.fSupported;
}


/**
 * Checks if internal events are pending. In that case we are not allowed to dispatch interrupts.
 *
 * @returns true if an internal event is pending, otherwise false.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
VMMR3_INT_DECL(bool) HMR3IsEventPending(PVMCPU pVCpu)
{
    return HMIsEnabled(pVCpu->pVMR3)
        && pVCpu->hm.s.Event.fPending;
}


/**
 * Checks if the VMX-preemption timer is being used.
 *
 * @returns true if the VMX-preemption timer is being used, otherwise false.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(bool) HMR3IsVmxPreemptionTimerUsed(PVM pVM)
{
    return HMIsEnabled(pVM)
        && pVM->hm.s.vmx.fEnabled
        && pVM->hm.s.vmx.fUsePreemptTimer;
}


/**
 * Check fatal VT-x/AMD-V error and produce some meaningful
 * log release message.
 *
 * @param   pVM         The cross context VM structure.
 * @param   iStatusCode VBox status code.
 */
VMMR3_INT_DECL(void) HMR3CheckError(PVM pVM, int iStatusCode)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        switch (iStatusCode)
        {
            /** @todo r=ramshankar: Are all EMTs out of ring-0 at this point!? If not, we
             *  might be getting inaccurate values for non-guru'ing EMTs. */
            case VERR_VMX_INVALID_VMCS_FIELD:
                break;

            case VERR_VMX_INVALID_VMCS_PTR:
                LogRel(("HM: VERR_VMX_INVALID_VMCS_PTR:\n"));
                LogRel(("HM: CPU[%u] Current pointer      %#RGp vs %#RGp\n", i, pVCpu->hm.s.vmx.LastError.u64VMCSPhys,
                                                                                pVCpu->hm.s.vmx.HCPhysVmcs));
                LogRel(("HM: CPU[%u] Current VMCS version %#x\n", i, pVCpu->hm.s.vmx.LastError.u32VMCSRevision));
                LogRel(("HM: CPU[%u] Entered Host Cpu     %u\n",  i, pVCpu->hm.s.vmx.LastError.idEnteredCpu));
                LogRel(("HM: CPU[%u] Current Host Cpu     %u\n",  i, pVCpu->hm.s.vmx.LastError.idCurrentCpu));
                break;

            case VERR_VMX_UNABLE_TO_START_VM:
                LogRel(("HM: VERR_VMX_UNABLE_TO_START_VM:\n"));
                LogRel(("HM: CPU[%u] Instruction error    %#x\n", i, pVCpu->hm.s.vmx.LastError.u32InstrError));
                LogRel(("HM: CPU[%u] Exit reason          %#x\n", i, pVCpu->hm.s.vmx.LastError.u32ExitReason));

                if (   pVM->aCpus[i].hm.s.vmx.LastError.u32InstrError == VMXINSTRERR_VMLAUNCH_NON_CLEAR_VMCS
                    || pVM->aCpus[i].hm.s.vmx.LastError.u32InstrError == VMXINSTRERR_VMRESUME_NON_LAUNCHED_VMCS)
                {
                    LogRel(("HM: CPU[%u] Entered Host Cpu     %u\n",  i, pVCpu->hm.s.vmx.LastError.idEnteredCpu));
                    LogRel(("HM: CPU[%u] Current Host Cpu     %u\n",  i, pVCpu->hm.s.vmx.LastError.idCurrentCpu));
                }
                else if (pVM->aCpus[i].hm.s.vmx.LastError.u32InstrError == VMXINSTRERR_VMENTRY_INVALID_CTLS)
                {
                    LogRel(("HM: CPU[%u] PinCtls          %#RX32\n", i, pVCpu->hm.s.vmx.u32PinCtls));
                    {
                        uint32_t const u32Val = pVCpu->hm.s.vmx.u32PinCtls;
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PIN_CTLS_EXT_INT_EXIT );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PIN_CTLS_NMI_EXIT     );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PIN_CTLS_VIRT_NMI     );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PIN_CTLS_PREEMPT_TIMER);
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PIN_CTLS_POSTED_INT   );
                    }
                    LogRel(("HM: CPU[%u] ProcCtls         %#RX32\n", i, pVCpu->hm.s.vmx.u32ProcCtls));
                    {
                        uint32_t const u32Val = pVCpu->hm.s.vmx.u32ProcCtls;
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_INT_WINDOW_EXIT   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_USE_TSC_OFFSETTING);
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_HLT_EXIT          );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_INVLPG_EXIT       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_MWAIT_EXIT        );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_RDPMC_EXIT        );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_RDTSC_EXIT        );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_CR3_LOAD_EXIT     );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_CR3_STORE_EXIT    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_CR8_LOAD_EXIT     );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_CR8_STORE_EXIT    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_USE_TPR_SHADOW    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_NMI_WINDOW_EXIT   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_MOV_DR_EXIT       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_UNCOND_IO_EXIT    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_USE_IO_BITMAPS    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_MONITOR_TRAP_FLAG );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_USE_MSR_BITMAPS   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_MONITOR_EXIT      );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_PAUSE_EXIT        );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS_USE_SECONDARY_CTLS);
                    }
                    LogRel(("HM: CPU[%u] ProcCtls2        %#RX32\n", i, pVCpu->hm.s.vmx.u32ProcCtls2));
                    {
                        uint32_t const u32Val = pVCpu->hm.s.vmx.u32ProcCtls2;
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VIRT_APIC_ACCESS  );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_EPT               );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_DESC_TABLE_EXIT   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_RDTSCP            );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VIRT_X2APIC_MODE  );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VPID              );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_WBINVD_EXIT       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_UNRESTRICTED_GUEST);
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_APIC_REG_VIRT     );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VIRT_INT_DELIVERY );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_PAUSE_LOOP_EXIT   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_RDRAND_EXIT       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_INVPCID           );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VMFUNC            );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_VMCS_SHADOWING    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_ENCLS_EXIT        );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_RDSEED_EXIT       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_PML               );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_EPT_VE            );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_CONCEAL_FROM_PT   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_XSAVES_XRSTORS    );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_PROC_CTLS2_TSC_SCALING       );
                    }
                    LogRel(("HM: CPU[%u] EntryCtls        %#RX32\n", i, pVCpu->hm.s.vmx.u32EntryCtls));
                    {
                        uint32_t const u32Val = pVCpu->hm.s.vmx.u32EntryCtls;
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_LOAD_DEBUG         );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_IA32E_MODE_GUEST   );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_ENTRY_TO_SMM       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_DEACTIVATE_DUAL_MON);
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_LOAD_PERF_MSR      );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_LOAD_PAT_MSR       );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_ENTRY_CTLS_LOAD_EFER_MSR      );
                    }
                    LogRel(("HM: CPU[%u] ExitCtls         %#RX32\n", i, pVCpu->hm.s.vmx.u32ExitCtls));
                    {
                        uint32_t const u32Val = pVCpu->hm.s.vmx.u32ExitCtls;
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_SAVE_DEBUG            );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE  );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_LOAD_PERF_MSR         );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_ACK_EXT_INT           );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_SAVE_PAT_MSR          );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_LOAD_PAT_MSR          );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_SAVE_EFER_MSR         );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_LOAD_EFER_MSR         );
                        HMVMX_LOGREL_FEAT(u32Val, VMX_EXIT_CTLS_SAVE_PREEMPT_TIMER    );
                    }
                    LogRel(("HM: CPU[%u] HCPhysMsrBitmap  %#RHp\n",  i, pVCpu->hm.s.vmx.HCPhysMsrBitmap));
                    LogRel(("HM: CPU[%u] HCPhysGuestMsr   %#RHp\n",  i, pVCpu->hm.s.vmx.HCPhysGuestMsr));
                    LogRel(("HM: CPU[%u] HCPhysHostMsr    %#RHp\n",  i, pVCpu->hm.s.vmx.HCPhysHostMsr));
                    LogRel(("HM: CPU[%u] cMsrs            %u\n",     i, pVCpu->hm.s.vmx.cMsrs));
                }
                /** @todo Log VM-entry event injection control fields
                 *        VMX_VMCS_CTRL_ENTRY_IRQ_INFO, VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE
                 *        and VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH from the VMCS. */
                break;

            /* The guru will dump the HM error and exit history. Nothing extra to report for these errors. */
            case VERR_VMX_INVALID_VMXON_PTR:
            case VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO:
            case VERR_VMX_INVALID_GUEST_STATE:
            case VERR_VMX_UNEXPECTED_EXIT:
            case VERR_SVM_UNKNOWN_EXIT:
            case VERR_SVM_UNEXPECTED_EXIT:
            case VERR_SVM_UNEXPECTED_PATCH_TYPE:
            case VERR_SVM_UNEXPECTED_XCPT_EXIT:
            case VERR_VMX_UNEXPECTED_INTERRUPTION_EXIT_TYPE:
                break;
        }
    }

    if (iStatusCode == VERR_VMX_UNABLE_TO_START_VM)
    {
        LogRel(("HM: VERR_VMX_UNABLE_TO_START_VM: VM-entry allowed    %#RX32\n", pVM->hm.s.vmx.Msrs.EntryCtls.n.allowed1));
        LogRel(("HM: VERR_VMX_UNABLE_TO_START_VM: VM-entry disallowed %#RX32\n", pVM->hm.s.vmx.Msrs.EntryCtls.n.disallowed0));
    }
    else if (iStatusCode == VERR_VMX_INVALID_VMXON_PTR)
        LogRel(("HM: HCPhysVmxEnableError         = %#RHp\n", pVM->hm.s.vmx.HCPhysVmxEnableError));
}


/**
 * Execute state save operation.
 *
 * Save only data that cannot be re-loaded while entering HM ring-0 code. This
 * is because we always save the VM state from ring-3 and thus most HM state
 * will be re-synced dynamically at runtime and don't need to be part of the VM
 * saved state.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) hmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    int rc;

    Log(("hmR3Save:\n"));

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        Assert(!pVM->aCpus[i].hm.s.Event.fPending);
        if (pVM->cpum.ro.GuestFeatures.fSvm)
        {
            PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = &pVM->aCpus[i].hm.s.svm.NstGstVmcbCache;
            rc  = SSMR3PutBool(pSSM, pVmcbNstGstCache->fCacheValid);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16InterceptRdCRx);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16InterceptWrCRx);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16InterceptRdDRx);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16InterceptWrDRx);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16PauseFilterThreshold);
            rc |= SSMR3PutU16(pSSM,  pVmcbNstGstCache->u16PauseFilterCount);
            rc |= SSMR3PutU32(pSSM,  pVmcbNstGstCache->u32InterceptXcpt);
            rc |= SSMR3PutU64(pSSM,  pVmcbNstGstCache->u64InterceptCtrl);
            rc |= SSMR3PutU64(pSSM,  pVmcbNstGstCache->u64TSCOffset);
            rc |= SSMR3PutBool(pSSM, pVmcbNstGstCache->fVIntrMasking);
            rc |= SSMR3PutBool(pSSM, pVmcbNstGstCache->fNestedPaging);
            rc |= SSMR3PutBool(pSSM, pVmcbNstGstCache->fLbrVirt);
            AssertRCReturn(rc, rc);
        }
    }

    /* Save the guest patch data. */
    rc  = SSMR3PutGCPtr(pSSM, pVM->hm.s.pGuestPatchMem);
    rc |= SSMR3PutGCPtr(pSSM, pVM->hm.s.pFreeGuestPatchMem);
    rc |= SSMR3PutU32(pSSM, pVM->hm.s.cbGuestPatchMem);

    /* Store all the guest patch records too. */
    rc |= SSMR3PutU32(pSSM, pVM->hm.s.cPatches);
    AssertRCReturn(rc, rc);

    for (uint32_t i = 0; i < pVM->hm.s.cPatches; i++)
    {
        AssertCompileSize(HMTPRINSTR, 4);
        PCHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];
        rc  = SSMR3PutU32(pSSM, pPatch->Core.Key);
        rc |= SSMR3PutMem(pSSM, pPatch->aOpcode, sizeof(pPatch->aOpcode));
        rc |= SSMR3PutU32(pSSM, pPatch->cbOp);
        rc |= SSMR3PutMem(pSSM, pPatch->aNewOpcode, sizeof(pPatch->aNewOpcode));
        rc |= SSMR3PutU32(pSSM, pPatch->cbNewOp);
        rc |= SSMR3PutU32(pSSM, (uint32_t)pPatch->enmType);
        rc |= SSMR3PutU32(pSSM, pPatch->uSrcOperand);
        rc |= SSMR3PutU32(pSSM, pPatch->uDstOperand);
        rc |= SSMR3PutU32(pSSM, pPatch->pJumpTarget);
        rc |= SSMR3PutU32(pSSM, pPatch->cFaults);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
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
static DECLCALLBACK(int) hmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    LogFlowFunc(("uVersion=%u\n", uVersion));
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Validate version.
     */
    if (   uVersion != HM_SAVED_STATE_VERSION_SVM_NESTED_HWVIRT
        && uVersion != HM_SAVED_STATE_VERSION_TPR_PATCHING
        && uVersion != HM_SAVED_STATE_VERSION_NO_TPR_PATCHING
        && uVersion != HM_SAVED_STATE_VERSION_2_0_X)
    {
        AssertMsgFailed(("hmR3Load: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Load per-VCPU state.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        if (uVersion >= HM_SAVED_STATE_VERSION_SVM_NESTED_HWVIRT)
        {
            /* Load the SVM nested hw.virt state if the VM is configured for it. */
            if (pVM->cpum.ro.GuestFeatures.fSvm)
            {
                PSVMNESTEDVMCBCACHE pVmcbNstGstCache = &pVM->aCpus[i].hm.s.svm.NstGstVmcbCache;
                rc  = SSMR3GetBool(pSSM, &pVmcbNstGstCache->fCacheValid);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16InterceptRdCRx);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16InterceptWrCRx);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16InterceptRdDRx);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16InterceptWrDRx);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16PauseFilterThreshold);
                rc |= SSMR3GetU16(pSSM,  &pVmcbNstGstCache->u16PauseFilterCount);
                rc |= SSMR3GetU32(pSSM,  &pVmcbNstGstCache->u32InterceptXcpt);
                rc |= SSMR3GetU64(pSSM,  &pVmcbNstGstCache->u64InterceptCtrl);
                rc |= SSMR3GetU64(pSSM,  &pVmcbNstGstCache->u64TSCOffset);
                rc |= SSMR3GetBool(pSSM, &pVmcbNstGstCache->fVIntrMasking);
                rc |= SSMR3GetBool(pSSM, &pVmcbNstGstCache->fNestedPaging);
                rc |= SSMR3GetBool(pSSM, &pVmcbNstGstCache->fLbrVirt);
                AssertRCReturn(rc, rc);
            }
        }
        else
        {
            /* Pending HM event (obsolete for a long time since TPRM holds the info.) */
            rc  = SSMR3GetU32(pSSM, &pVM->aCpus[i].hm.s.Event.fPending);
            rc |= SSMR3GetU32(pSSM, &pVM->aCpus[i].hm.s.Event.u32ErrCode);
            rc |= SSMR3GetU64(pSSM, &pVM->aCpus[i].hm.s.Event.u64IntInfo);

            /* VMX fWasInRealMode related data. */
            uint32_t uDummy;
            rc |= SSMR3GetU32(pSSM, &uDummy);   AssertRCReturn(rc, rc);
            rc |= SSMR3GetU32(pSSM, &uDummy);   AssertRCReturn(rc, rc);
            rc |= SSMR3GetU32(pSSM, &uDummy);   AssertRCReturn(rc, rc);
            AssertRCReturn(rc, rc);
        }
    }

    /*
     * Load TPR patching data.
     */
    if (uVersion >= HM_SAVED_STATE_VERSION_TPR_PATCHING)
    {
        rc  = SSMR3GetGCPtr(pSSM, &pVM->hm.s.pGuestPatchMem);
        rc |= SSMR3GetGCPtr(pSSM, &pVM->hm.s.pFreeGuestPatchMem);
        rc |= SSMR3GetU32(pSSM, &pVM->hm.s.cbGuestPatchMem);

        /* Fetch all TPR patch records. */
        rc |= SSMR3GetU32(pSSM, &pVM->hm.s.cPatches);
        AssertRCReturn(rc, rc);
        for (uint32_t i = 0; i < pVM->hm.s.cPatches; i++)
        {
            PHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];
            rc  = SSMR3GetU32(pSSM, &pPatch->Core.Key);
            rc |= SSMR3GetMem(pSSM, pPatch->aOpcode, sizeof(pPatch->aOpcode));
            rc |= SSMR3GetU32(pSSM, &pPatch->cbOp);
            rc |= SSMR3GetMem(pSSM, pPatch->aNewOpcode, sizeof(pPatch->aNewOpcode));
            rc |= SSMR3GetU32(pSSM, &pPatch->cbNewOp);
            rc |= SSMR3GetU32(pSSM, (uint32_t *)&pPatch->enmType);

            if (pPatch->enmType == HMTPRINSTR_JUMP_REPLACEMENT)
                pVM->hm.s.fTPRPatchingActive = true;
            Assert(pPatch->enmType == HMTPRINSTR_JUMP_REPLACEMENT || pVM->hm.s.fTPRPatchingActive == false);

            rc |= SSMR3GetU32(pSSM, &pPatch->uSrcOperand);
            rc |= SSMR3GetU32(pSSM, &pPatch->uDstOperand);
            rc |= SSMR3GetU32(pSSM, &pPatch->cFaults);
            rc |= SSMR3GetU32(pSSM, &pPatch->pJumpTarget);
            AssertRCReturn(rc, rc);

            LogFlow(("hmR3Load: patch %d\n", i));
            LogFlow(("Key       = %x\n", pPatch->Core.Key));
            LogFlow(("cbOp      = %d\n", pPatch->cbOp));
            LogFlow(("cbNewOp   = %d\n", pPatch->cbNewOp));
            LogFlow(("type      = %d\n", pPatch->enmType));
            LogFlow(("srcop     = %d\n", pPatch->uSrcOperand));
            LogFlow(("dstop     = %d\n", pPatch->uDstOperand));
            LogFlow(("cFaults   = %d\n", pPatch->cFaults));
            LogFlow(("target    = %x\n", pPatch->pJumpTarget));

            rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
            AssertRCReturn(rc, rc);
        }
    }

    return VINF_SUCCESS;
}


/**
 * Gets the name of a VT-x exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The VT-x exit to name.
 */
VMMR3DECL(const char *) HMR3GetVmxExitName(uint32_t uExit)
{
    if (uExit < RT_ELEMENTS(g_apszVmxExitReasons))
        return g_apszVmxExitReasons[uExit];
    return NULL;
}


/**
 * Gets the name of an AMD-V exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The AMD-V exit to name.
 */
VMMR3DECL(const char *) HMR3GetSvmExitName(uint32_t uExit)
{
    if (uExit < RT_ELEMENTS(g_apszSvmExitReasons))
        return g_apszSvmExitReasons[uExit];
    return hmSvmGetSpecialExitReasonDesc(uExit);
}


/**
 * Displays HM info.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) hmR3Info(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    if (HMIsEnabled(pVM))
    {
        if (pVM->hm.s.vmx.fSupported)
            pHlp->pfnPrintf(pHlp, "CPU[%u]: VT-x info:\n", pVCpu->idCpu);
        else
            pHlp->pfnPrintf(pHlp, "CPU[%u]: AMD-V info:\n", pVCpu->idCpu);
        pHlp->pfnPrintf(pHlp, "  HM error       = %#x (%u)\n", pVCpu->hm.s.u32HMError, pVCpu->hm.s.u32HMError);
        pHlp->pfnPrintf(pHlp, "  rcLastExitToR3 = %Rrc\n", pVCpu->hm.s.rcLastExitToR3);
    }
    else
        pHlp->pfnPrintf(pHlp, "HM is not enabled for this VM!\n");
}


/**
 * Displays the HM pending event.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) hmR3InfoEventPending(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    if (HMIsEnabled(pVM))
    {
        pHlp->pfnPrintf(pHlp, "CPU[%u]: HM event (fPending=%RTbool)\n", pVCpu->idCpu, pVCpu->hm.s.Event.fPending);
        if (pVCpu->hm.s.Event.fPending)
        {
            pHlp->pfnPrintf(pHlp, "  u64IntInfo        = %#RX64\n", pVCpu->hm.s.Event.u64IntInfo);
            pHlp->pfnPrintf(pHlp, "  u32ErrCode        = %#RX64\n", pVCpu->hm.s.Event.u32ErrCode);
            pHlp->pfnPrintf(pHlp, "  cbInstr           = %u bytes\n", pVCpu->hm.s.Event.cbInstr);
            pHlp->pfnPrintf(pHlp, "  GCPtrFaultAddress = %#RGp\n", pVCpu->hm.s.Event.GCPtrFaultAddress);
        }
    }
    else
        pHlp->pfnPrintf(pHlp, "HM is not enabled for this VM!\n");
}


/**
 * Displays the SVM nested-guest VMCB cache.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) hmR3InfoSvmNstGstVmcbCache(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    NOREF(pszArgs);
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    bool const fSvmEnabled = HMR3IsSvmEnabled(pVM->pUVM);
    if (   fSvmEnabled
        && pVM->cpum.ro.GuestFeatures.fSvm)
    {
        PCSVMNESTEDVMCBCACHE pVmcbNstGstCache = &pVCpu->hm.s.svm.NstGstVmcbCache;
        pHlp->pfnPrintf(pHlp, "CPU[%u]: HM SVM nested-guest VMCB cache\n", pVCpu->idCpu);
        pHlp->pfnPrintf(pHlp, "  fCacheValid             = %#RTbool\n", pVmcbNstGstCache->fCacheValid);
        pHlp->pfnPrintf(pHlp, "  u16InterceptRdCRx       = %#RX16\n",   pVmcbNstGstCache->u16InterceptRdCRx);
        pHlp->pfnPrintf(pHlp, "  u16InterceptWrCRx       = %#RX16\n",   pVmcbNstGstCache->u16InterceptWrCRx);
        pHlp->pfnPrintf(pHlp, "  u16InterceptRdDRx       = %#RX16\n",   pVmcbNstGstCache->u16InterceptRdDRx);
        pHlp->pfnPrintf(pHlp, "  u16InterceptWrDRx       = %#RX16\n",   pVmcbNstGstCache->u16InterceptWrDRx);
        pHlp->pfnPrintf(pHlp, "  u16PauseFilterThreshold = %#RX16\n",   pVmcbNstGstCache->u16PauseFilterThreshold);
        pHlp->pfnPrintf(pHlp, "  u16PauseFilterCount     = %#RX16\n",   pVmcbNstGstCache->u16PauseFilterCount);
        pHlp->pfnPrintf(pHlp, "  u32InterceptXcpt        = %#RX32\n",   pVmcbNstGstCache->u32InterceptXcpt);
        pHlp->pfnPrintf(pHlp, "  u64InterceptCtrl        = %#RX64\n",   pVmcbNstGstCache->u64InterceptCtrl);
        pHlp->pfnPrintf(pHlp, "  u64TSCOffset            = %#RX64\n",   pVmcbNstGstCache->u64TSCOffset);
        pHlp->pfnPrintf(pHlp, "  fVIntrMasking           = %RTbool\n",  pVmcbNstGstCache->fVIntrMasking);
        pHlp->pfnPrintf(pHlp, "  fNestedPaging           = %RTbool\n",  pVmcbNstGstCache->fNestedPaging);
        pHlp->pfnPrintf(pHlp, "  fLbrVirt                = %RTbool\n",  pVmcbNstGstCache->fLbrVirt);
    }
    else
    {
        if (!fSvmEnabled)
            pHlp->pfnPrintf(pHlp, "HM SVM is not enabled for this VM!\n");
        else
            pHlp->pfnPrintf(pHlp, "SVM feature is not exposed to the guest!\n");
    }
}

