/* $Id$ */
/** @file
 * HM - All contexts.
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
#include <iprt/errcore.h>
#include <VBox/log.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/x86.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#define EXIT_REASON(a_Def, a_Val, a_Str)      #a_Def " - " #a_Val " - " a_Str
#define EXIT_REASON_NIL()                     NULL

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
 * Checks if the guest is in a suitable state for hardware-assisted execution.
 *
 * @returns @c true if it is suitable, @c false otherwise.
 * @param   pVCpu   The cross context virtual CPU structure.
 * @param   pCtx    Pointer to the guest CPU context.
 *
 * @remarks @a pCtx can be a partial context created and not necessarily the same as
 *          pVCpu->cpum.GstCtx.
 */
VMMDECL(bool) HMCanExecuteGuest(PVMCPU pVCpu, PCCPUMCTX pCtx)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(HMIsEnabled(pVM));

#ifdef VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM
    if (   CPUMIsGuestInSvmNestedHwVirtMode(pCtx)
        || CPUMIsGuestInVmxNonRootMode(pCtx))
    {
        LogFunc(("In nested-guest mode - returning false"));
        return false;
    }
#endif

    /* AMD-V supports real & protected mode with or without paging. */
    if (pVM->hm.s.svm.fEnabled)
    {
        pVCpu->hm.s.fActive = true;
        return true;
    }

    bool rc = HMCanExecuteVmxGuest(pVCpu, pCtx);
    LogFlowFunc(("returning %RTbool\n", rc));
    return rc;
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
VMM_INT_DECL(int) HMFlushTlb(PVMCPU pVCpu)
{
    LogFlow(("HMFlushTlb\n"));

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
VMM_INT_DECL(int) HMFlushTlbOnAllVCpus(PVM pVM)
{
    if (pVM->cCpus == 1)
        return HMFlushTlb(&pVM->aCpus[0]);

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
    return HMFlushTlbOnAllVCpus(pVM);
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
    return pVM->hm.s.vmx.fSupported && HMIsEnabled(pVM);
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
VMM_INT_DECL(void) HMHCChangedPagingMode(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode)
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

    Log4(("HMHCChangedPagingMode: Guest paging mode '%s', shadow paging mode '%s'\n", PGMGetModeName(enmGuestMode),
          PGMGetModeName(enmShadowMode)));
}
#endif /* !IN_RC */


/**
 * Gets VMX MSRs from the provided hardware-virtualization MSRs struct.
 *
 * This abstraction exists to allow insultation of the support driver from including
 * VMX structures from HM headers.
 *
 * @param   pHwvirtMsrs     The hardware-virtualization MSRs.
 * @param   pVmxMsrs        Where to store the VMX MSRs.
 */
VMM_INT_DECL(void) HMGetVmxMsrsFromHwvirtMsrs(PCSUPHWVIRTMSRS pHwvirtMsrs, PVMXMSRS pVmxMsrs)
{
    AssertReturnVoid(pHwvirtMsrs);
    AssertReturnVoid(pVmxMsrs);
    pVmxMsrs->u64FeatCtrl      = pHwvirtMsrs->u.vmx.u64FeatCtrl;
    pVmxMsrs->u64Basic         = pHwvirtMsrs->u.vmx.u64Basic;
    pVmxMsrs->PinCtls.u        = pHwvirtMsrs->u.vmx.u64PinCtls;
    pVmxMsrs->ProcCtls.u       = pHwvirtMsrs->u.vmx.u64ProcCtls;
    pVmxMsrs->ProcCtls2.u      = pHwvirtMsrs->u.vmx.u64ProcCtls2;
    pVmxMsrs->ExitCtls.u       = pHwvirtMsrs->u.vmx.u64ExitCtls;
    pVmxMsrs->EntryCtls.u      = pHwvirtMsrs->u.vmx.u64EntryCtls;
    pVmxMsrs->TruePinCtls.u    = pHwvirtMsrs->u.vmx.u64TruePinCtls;
    pVmxMsrs->TrueProcCtls.u   = pHwvirtMsrs->u.vmx.u64TrueProcCtls;
    pVmxMsrs->TrueEntryCtls.u  = pHwvirtMsrs->u.vmx.u64TrueEntryCtls;
    pVmxMsrs->TrueExitCtls.u   = pHwvirtMsrs->u.vmx.u64TrueExitCtls;
    pVmxMsrs->u64Misc          = pHwvirtMsrs->u.vmx.u64Misc;
    pVmxMsrs->u64Cr0Fixed0     = pHwvirtMsrs->u.vmx.u64Cr0Fixed0;
    pVmxMsrs->u64Cr0Fixed1     = pHwvirtMsrs->u.vmx.u64Cr0Fixed1;
    pVmxMsrs->u64Cr4Fixed0     = pHwvirtMsrs->u.vmx.u64Cr4Fixed0;
    pVmxMsrs->u64Cr4Fixed1     = pHwvirtMsrs->u.vmx.u64Cr4Fixed1;
    pVmxMsrs->u64VmcsEnum      = pHwvirtMsrs->u.vmx.u64VmcsEnum;
    pVmxMsrs->u64VmFunc        = pHwvirtMsrs->u.vmx.u64VmFunc;
    pVmxMsrs->u64EptVpidCaps   = pHwvirtMsrs->u.vmx.u64EptVpidCaps;
}


/**
 * Gets SVM MSRs from the provided hardware-virtualization MSRs struct.
 *
 * This abstraction exists to allow insultation of the support driver from including
 * SVM structures from HM headers.
 *
 * @param   pHwvirtMsrs     The hardware-virtualization MSRs.
 * @param   pSvmMsrs        Where to store the SVM MSRs.
 */
VMM_INT_DECL(void) HMGetSvmMsrsFromHwvirtMsrs(PCSUPHWVIRTMSRS pHwvirtMsrs, PSVMMSRS pSvmMsrs)
{
    AssertReturnVoid(pHwvirtMsrs);
    AssertReturnVoid(pSvmMsrs);
    pSvmMsrs->u64MsrHwcr = pHwvirtMsrs->u.svm.u64MsrHwcr;
}


/**
 * Gets the name of a VT-x exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The VT-x exit to name.
 */
VMM_INT_DECL(const char *) HMGetVmxExitName(uint32_t uExit)
{
    if (uExit <= MAX_EXITREASON_VTX)
    {
        Assert(uExit < RT_ELEMENTS(g_apszVmxExitReasons));
        return g_apszVmxExitReasons[uExit];
    }
    return NULL;
}


/**
 * Gets the name of an AMD-V exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The AMD-V exit to name.
 */
VMM_INT_DECL(const char *) HMGetSvmExitName(uint32_t uExit)
{
    if (uExit <= MAX_EXITREASON_AMDV)
    {
        Assert(uExit < RT_ELEMENTS(g_apszSvmExitReasons));
        return g_apszSvmExitReasons[uExit];
    }
    return hmSvmGetSpecialExitReasonDesc(uExit);
}

