/* $Id$ */
/** @file
 * IEM - Native Recompiler, Emitters for Built-In Threaded Functions.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_IEM_RE_NATIVE
//#define IEM_WITH_OPAQUE_DECODER_STATE - need offCurInstrStart access for iemNativeHlpMemCodeNewPageTlbMiss and friends.
#define VMCPU_INCL_CPUM_GST_CTX
#define VMM_INCLUDED_SRC_include_IEMMc_h /* block IEMMc.h inclusion. */
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#if   defined(RT_ARCH_AMD64)
# include <iprt/x86.h>
#elif defined(RT_ARCH_ARM64)
# include <iprt/armv8.h>
#endif


#include "IEMInline.h"
#include "IEMThreadedFunctions.h"
#include "IEMN8veRecompiler.h"
#include "IEMN8veRecompilerEmit.h"



/*********************************************************************************************************************************
*   TB Helper Functions                                                                                                          *
*********************************************************************************************************************************/
#ifdef RT_ARCH_AMD64
DECLASM(void) iemNativeHlpAsmSafeWrapLogCpuState(void);
#endif


/**
 * Used by TB code to deal with a TLB miss for a new page.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemCodeNewPageTlbMiss,(PVMCPUCC pVCpu))
{
    STAM_COUNTER_INC(&pVCpu->iem.s.StatNativeCodeTlbMissesNewPage);
    pVCpu->iem.s.pbInstrBuf       = NULL;
    pVCpu->iem.s.offCurInstrStart = GUEST_PAGE_SIZE;
    pVCpu->iem.s.offInstrNextByte = GUEST_PAGE_SIZE;
    iemOpcodeFetchBytesJmp(pVCpu, 0, NULL);
    if (pVCpu->iem.s.pbInstrBuf)
    { /* likely */ }
    else
    {
        IEM_DO_LONGJMP(pVCpu, VINF_IEM_REEXEC_BREAK);
    }
}


/**
 * Used by TB code to deal with a TLB miss for a new page.
 */
IEM_DECL_NATIVE_HLP_DEF(RTGCPHYS, iemNativeHlpMemCodeNewPageTlbMissWithOff,(PVMCPUCC pVCpu, uint8_t offInstr))
{
    STAM_COUNTER_INC(&pVCpu->iem.s.StatNativeCodeTlbMissesNewPage);
    pVCpu->iem.s.pbInstrBuf       = NULL;
    pVCpu->iem.s.offCurInstrStart = GUEST_PAGE_SIZE - offInstr;
    pVCpu->iem.s.offInstrNextByte = GUEST_PAGE_SIZE;
    iemOpcodeFetchBytesJmp(pVCpu, 0, NULL);
    return pVCpu->iem.s.pbInstrBuf ? pVCpu->iem.s.GCPhysInstrBuf : NIL_RTGCPHYS;
}


/*********************************************************************************************************************************
*   Builtin functions                                                                                                            *
*********************************************************************************************************************************/

/**
 * Built-in function that does nothing.
 *
 * Whether this is called or not can be controlled by the entry in the
 * IEMThreadedGenerator.katBltIns table.  This can be useful to determine
 * whether why behaviour changes when enabling the LogCpuState builtins.  I.e.
 * whether it's the reduced call count in the TBs or the threaded calls flushing
 * register state.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_Nop)
{
    RT_NOREF(pReNative, pCallEntry);
    return off;
}


/**
 * Emits for for LogCpuState.
 *
 * This shouldn't have any relevant impact on the recompiler state.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_LogCpuState)
{
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 20);
    /* push rax */
    pbCodeBuf[off++] = 0x50 + X86_GREG_xAX;
    /* push imm32 */
    pbCodeBuf[off++] = 0x68;
    pbCodeBuf[off++] = RT_BYTE1(pCallEntry->auParams[0]);
    pbCodeBuf[off++] = RT_BYTE2(pCallEntry->auParams[0]);
    pbCodeBuf[off++] = RT_BYTE3(pCallEntry->auParams[0]);
    pbCodeBuf[off++] = RT_BYTE4(pCallEntry->auParams[0]);
    /* mov rax, iemNativeHlpAsmSafeWrapLogCpuState */
    pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0xb8 + X86_GREG_xAX;
    *(uint64_t *)&pbCodeBuf[off] = (uintptr_t)iemNativeHlpAsmSafeWrapLogCpuState;
    off += sizeof(uint64_t);
    /* call rax */
    pbCodeBuf[off++] = 0xff;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);
    /* pop rax */
    pbCodeBuf[off++] = 0x58 + X86_GREG_xAX;
    /* pop rax */
    pbCodeBuf[off++] = 0x58 + X86_GREG_xAX;
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
    /** @todo Implement this  */
    AssertFailed();
    RT_NOREF(pReNative, pCallEntry);
#endif
    return off;
}


/**
 * Built-in function that calls a C-implemention function taking zero arguments.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_DeferToCImpl0)
{
    PFNIEMCIMPL0 const pfnCImpl     = (PFNIEMCIMPL0)(uintptr_t)pCallEntry->auParams[0];
    uint8_t const      cbInstr      = (uint8_t)pCallEntry->auParams[1];
    uint64_t const     fGstShwFlush = pCallEntry->auParams[2];
    return iemNativeEmitCImplCall(pReNative, off, pCallEntry->idxInstr, fGstShwFlush, (uintptr_t)pfnCImpl, cbInstr, 0, 0, 0, 0);
}


/**
 * Built-in function that checks for pending interrupts that can be delivered or
 * forced action flags.
 *
 * This triggers after the completion of an instruction, so EIP is already at
 * the next instruction.  If an IRQ or important FF is pending, this will return
 * a non-zero status that stops TB execution.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckIrq)
{
    RT_NOREF(pCallEntry);

    /* It's too convenient to use iemNativeEmitTestBitInGprAndJmpToLabelIfNotSet below
       and I'm too lazy to create a 'Fixed' version of that one. */
    uint32_t const idxLabelVmCheck = iemNativeLabelCreate(pReNative, kIemNativeLabelType_CheckIrq,
                                                          UINT32_MAX, pReNative->uCheckIrqSeqNo++);

    uint32_t const idxLabelReturnBreak = iemNativeLabelCreate(pReNative, kIemNativeLabelType_ReturnBreak);

    /* Again, we need to load the extended EFLAGS before we actually need them
       in case we jump.  We couldn't use iemNativeRegAllocTmpForGuestReg if we
       loaded them inside the check, as the shadow state would not be correct
       when the code branches before the load.  Ditto PC. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    uint8_t const idxPcReg  = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ReadOnly);

    uint8_t idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);

    /*
     * Start by checking the local forced actions of the EMT we're on for IRQs
     * and other FFs that needs servicing.
     */
    /** @todo this isn't even close to the NMI and interrupt conditions in EM! */
    /* Load FFs in to idxTmpReg and AND with all relevant flags. */
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxTmpReg, RT_UOFFSETOF(VMCPUCC, fLocalForcedActions));
    off = iemNativeEmitAndGprByImm(pReNative, off, idxTmpReg,
                                   VMCPU_FF_ALL_MASK & ~(  VMCPU_FF_PGM_SYNC_CR3
                                                         | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL
                                                         | VMCPU_FF_TLB_FLUSH
                                                         | VMCPU_FF_UNHALT ),
                                   true /*fSetFlags*/);
    /* If we end up with ZERO in idxTmpReg there is nothing to do.*/
    uint32_t const offFixupJumpToVmCheck1 = off;
    off = iemNativeEmitJzToFixed(pReNative, off, off /* ASSUME jz rel8 suffices */);

    /* Some relevant FFs are set, but if's only APIC or/and PIC being set,
       these may be supressed by EFLAGS.IF or CPUMIsInInterruptShadow. */
    off = iemNativeEmitAndGprByImm(pReNative, off, idxTmpReg,
                                   ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC), true /*fSetFlags*/);
    /* Return VINF_IEM_REEXEC_BREAK if other FFs are set. */
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabelReturnBreak);

    /* So, it's only interrupt releated FFs and we need to see if IRQs are being
       suppressed by the CPU or not. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfNotSet(pReNative, off, idxEflReg, X86_EFL_IF_BIT, idxLabelVmCheck);
    off = iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfNoneSet(pReNative, off, idxEflReg, CPUMCTX_INHIBIT_SHADOW,
                                                              idxLabelReturnBreak);

    /* We've got shadow flags set, so we must check that the PC they are valid
       for matches our current PC value. */
    /** @todo AMD64 can do this more efficiently w/o loading uRipInhibitInt into
     *        a register. */
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxTmpReg, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.uRipInhibitInt));
    off = iemNativeEmitTestIfGprNotEqualGprAndJmpToLabel(pReNative, off, idxTmpReg, idxPcReg, idxLabelReturnBreak);

    /*
     * Now check the force flags of the VM.
     */
    iemNativeLabelDefine(pReNative, idxLabelVmCheck, off);
    iemNativeFixupFixedJump(pReNative, offFixupJumpToVmCheck1, off);
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxTmpReg, RT_UOFFSETOF(VMCPUCC, CTX_SUFF(pVM))); /* idxTmpReg = pVM */
    off = iemNativeEmitLoadGprByGprU32(pReNative, off, idxTmpReg, idxTmpReg, RT_UOFFSETOF(VMCC, fGlobalForcedActions));
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxTmpReg, VM_FF_ALL_MASK, true /*fSetFlags*/);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabelReturnBreak);

    /** @todo STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatCheckIrqBreaks); */

    /*
     * We're good, no IRQs or FFs pending.
     */
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
    iemNativeRegFreeTmp(pReNative, idxEflReg);
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


/**
 * Built-in function checks if IEMCPU::fExec has the expected value.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckMode)
{
    uint32_t const fExpectedExec = (uint32_t)pCallEntry->auParams[0];
    uint8_t const  idxTmpReg     = iemNativeRegAllocTmp(pReNative, &off);

    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, idxTmpReg, RT_UOFFSETOF(VMCPUCC, iem.s.fExec));
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxTmpReg, IEMTB_F_KEY_MASK);
    off = iemNativeEmitTestIfGpr32NotEqualImmAndJmpToNewLabel(pReNative, off, idxTmpReg, fExpectedExec & IEMTB_F_KEY_MASK,
                                                              kIemNativeLabelType_ReturnBreak);
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
    return off;
}


/**
 * Sets idxTbCurInstr in preparation of raising an exception or aborting the TB.
 */
/** @todo Optimize this, so we don't set the same value more than once.  Just
 *        needs some tracking. */
#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
# define BODY_SET_CUR_INSTR() \
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, pCallEntry->idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr))
#else
# define BODY_SET_CUR_INSTR() ((void)0)
#endif

/**
 * Flushes pending writes in preparation of raising an exception or aborting the TB.
 */
#define BODY_FLUSH_PENDING_WRITES() \
    off = iemNativeRegFlushPendingWrites(pReNative, off);


/**
 * Macro that emits the 16/32-bit CS.LIM check.
 */
#define BODY_CHECK_CS_LIM(a_cbInstr) \
    off = iemNativeEmitBltInCheckCsLim(pReNative, off, (a_cbInstr))

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInCheckCsLim(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    Assert(cbInstr >  0);
    Assert(cbInstr < 16);
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000001);
#endif

    /*
     * We need CS.LIM and RIP here. When cbInstr is larger than 1, we also need
     * a temporary register for calculating the last address of the instruction.
     *
     * The calculation and comparisons are 32-bit.  We ASSUME that the incoming
     * RIP isn't totally invalid, i.e. that any jump/call/ret/iret instruction
     * that last updated EIP here checked it already, and that we're therefore
     * safe in the 32-bit wrap-around scenario to only check that the last byte
     * is within CS.LIM.  In the case of instruction-by-instruction advancing
     * up to a EIP wrap-around, we know that CS.LIM is 4G-1 because the limit
     * must be using 4KB granularity and the previous instruction was fine.
     */
    uint8_t const  idxRegPc     = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc,
                                                                  kIemNativeGstRegUse_ReadOnly);
    uint8_t const  idxRegCsLim  = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_LIMIT(X86_SREG_CS),
                                                                  kIemNativeGstRegUse_ReadOnly);
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf   = iemNativeInstrBufEnsure(pReNative, off, 8);
#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
#else
# error "Port me"
#endif

    if (cbInstr != 1)
    {
        uint8_t const idxRegTmp = iemNativeRegAllocTmp(pReNative, &off);

        /*
         * 1. idxRegTmp = idxRegPc + cbInstr;
         * 2. if idxRegTmp > idxRegCsLim then raise #GP(0).
         */
#ifdef RT_ARCH_AMD64
        /* 1. lea tmp32, [Pc + cbInstr - 1] */
        if (idxRegTmp >= 8 || idxRegPc >= 8)
            pbCodeBuf[off++] = (idxRegTmp < 8 ? 0 : X86_OP_REX_R) | (idxRegPc < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x8d;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, idxRegTmp & 7, idxRegPc & 7);
        if ((idxRegPc & 7) == X86_GREG_xSP)
            pbCodeBuf[off++] = X86_SIB_MAKE(idxRegPc & 7, 4 /*no index*/, 0);
        pbCodeBuf[off++] = cbInstr - 1;

        /* 2. cmp tmp32(r), CsLim(r/m). */
        if (idxRegTmp >= 8 || idxRegCsLim >= 8)
            pbCodeBuf[off++] = (idxRegTmp < 8 ? 0 : X86_OP_REX_R) | (idxRegCsLim < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x3b;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegTmp & 7, idxRegCsLim & 7);

#elif defined(RT_ARCH_ARM64)
        /* 1. add tmp32, Pc, #cbInstr-1 */
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxRegTmp, idxRegPc, cbInstr - 1, false /*f64Bit*/);
        /* 2. cmp tmp32, CsLim */
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, ARMV8_A64_REG_XZR, idxRegTmp, idxRegCsLim,
                                                      false /*f64Bit*/, true /*fSetFlags*/);

#endif
        iemNativeRegFreeTmp(pReNative, idxRegTmp);
    }
    else
    {
        /*
         * Here we can skip step 1 and compare PC and CS.LIM directly.
         */
#ifdef RT_ARCH_AMD64
        /* 2. cmp eip(r), CsLim(r/m). */
        if (idxRegPc >= 8 || idxRegCsLim >= 8)
            pbCodeBuf[off++] = (idxRegPc < 8 ? 0 : X86_OP_REX_R) | (idxRegCsLim < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0x3b;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxRegPc & 7, idxRegCsLim & 7);

#elif defined(RT_ARCH_ARM64)
        /* 2. cmp Pc, CsLim */
        pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, ARMV8_A64_REG_XZR, idxRegPc, idxRegCsLim,
                                                      false /*f64Bit*/, true /*fSetFlags*/);

#endif
    }

    /* 3. Jump if greater. */
    off = iemNativeEmitJaToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);

    iemNativeRegFreeTmp(pReNative, idxRegCsLim);
    iemNativeRegFreeTmp(pReNative, idxRegPc);
    return off;
}


/**
 * Macro that considers whether we need CS.LIM checking after a branch or
 * crossing over to a new page.
 */
#define BODY_CONSIDER_CS_LIM_CHECKING(a_pTb, a_cbInstr) \
    RT_NOREF(a_cbInstr); \
    off = iemNativeEmitBltInConsiderLimChecking(pReNative, off)

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInConsiderLimChecking(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000002);
#endif

    /*
     * This check must match the ones in the iem in iemGetTbFlagsForCurrentPc
     * exactly:
     *
     *  int64_t const offFromLim = (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.eip;
     *  if (offFromLim >= X86_PAGE_SIZE + 16 - (int32_t)(pVCpu->cpum.GstCtx.cs.u64Base & GUEST_PAGE_OFFSET_MASK))
     *      return fRet;
     *  return fRet | IEMTB_F_CS_LIM_CHECKS;
     *
     *
     * We need EIP, CS.LIM and CS.BASE here.
     */

    /* Calculate the offFromLim first: */
    uint8_t const  idxRegPc     = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc,
                                                                  kIemNativeGstRegUse_ReadOnly);
    uint8_t const  idxRegCsLim  = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_LIMIT(X86_SREG_CS),
                                                                  kIemNativeGstRegUse_ReadOnly);
    uint8_t const  idxRegLeft   = iemNativeRegAllocTmp(pReNative, &off);

#ifdef RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegLeft, idxRegCsLim, idxRegPc);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
#else
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegLeft, idxRegCsLim);
    off = iemNativeEmitSubTwoGprs(pReNative, off, idxRegLeft, idxRegPc);
#endif

    iemNativeRegFreeTmp(pReNative, idxRegCsLim);
    iemNativeRegFreeTmp(pReNative, idxRegPc);

    /* Calculate the threshold level (right side). */
    uint8_t const  idxRegCsBase = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_BASE(X86_SREG_CS),
                                                                  kIemNativeGstRegUse_ReadOnly);
    uint8_t const  idxRegRight  = iemNativeRegAllocTmp(pReNative, &off);

#ifdef RT_ARCH_ARM64
    pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 4);
    Assert(Armv8A64ConvertImmRImmS2Mask32(11, 0) == GUEST_PAGE_OFFSET_MASK);
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(idxRegRight, idxRegCsBase, 11, 0, false /*f64Bit*/);
    pu32CodeBuf[off++] = Armv8A64MkInstrNeg(idxRegRight);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegRight, idxRegRight, (X86_PAGE_SIZE + 16) / 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegRight, idxRegRight, (X86_PAGE_SIZE + 16) / 2);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
    off = iemNativeEmitLoadGprImm32(pReNative, off, idxRegRight, GUEST_PAGE_OFFSET_MASK);
    off = iemNativeEmitAndGpr32ByGpr32(pReNative, off, idxRegRight, idxRegCsBase);
    off = iemNativeEmitNegGpr(pReNative, off, idxRegRight);
    off = iemNativeEmitAddGprImm(pReNative, off, idxRegRight, X86_PAGE_SIZE + 16);
#endif

    iemNativeRegFreeTmp(pReNative, idxRegCsBase);

    /* Compare the two and jump out if we're too close to the limit. */
    off = iemNativeEmitCmpGprWithGpr(pReNative, off, idxRegLeft, idxRegRight);
    off = iemNativeEmitJlToNewLabel(pReNative, off, kIemNativeLabelType_NeedCsLimChecking);

    iemNativeRegFreeTmp(pReNative, idxRegRight);
    iemNativeRegFreeTmp(pReNative, idxRegLeft);
    return off;
}



/**
 * Macro that implements opcode (re-)checking.
 */
#define BODY_CHECK_OPCODES(a_pTb, a_idxRange, a_offRange, a_cbInstr) \
    RT_NOREF(a_cbInstr); \
    off = iemNativeEmitBltInCheckOpcodes(pReNative, off, (a_pTb), (a_idxRange), (a_offRange))

#if 0 /* debugging aid */
bool g_fBpOnObsoletion = false;
# define BP_ON_OBSOLETION g_fBpOnObsoletion
#else
# define BP_ON_OBSOLETION 0
#endif

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInCheckOpcodes(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTB pTb, uint8_t idxRange, uint16_t offRange)
{
    Assert(idxRange < pTb->cRanges && pTb->cRanges <= RT_ELEMENTS(pTb->aRanges));
    Assert(offRange < pTb->aRanges[idxRange].cbOpcodes);
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000003);
#endif

    uint32_t const      idxLabelObsoleteTb = iemNativeLabelCreate(pReNative, kIemNativeLabelType_ObsoleteTb);

    /*
     * Where to start and how much to compare.
     *
     * Looking at the ranges produced when r160746 was running a DOS VM with TB
     * logging, the ranges can be anything from 1 byte to at least 0x197 bytes,
     * with the 6, 5, 4, 7, 8, 40, 3, 2, 9 and 10 being the top 10 in the sample.
     *
     * The top 10 for the early boot phase of a 64-bit debian 9.4 VM: 5, 9, 8,
     * 12, 10, 11, 6, 13, 15 and 16.  Max 0x359 bytes. Same revision as above.
     */
    uint16_t            offPage     = pTb->aRanges[idxRange].offPhysPage + offRange;
    uint16_t            cbLeft      = pTb->aRanges[idxRange].cbOpcodes   - offRange;
    Assert(cbLeft > 0);
    uint8_t const      *pbOpcodes   = &pTb->pabOpcodes[pTb->aRanges[idxRange].offOpcodes + offRange];
    uint32_t            offConsolidatedJump = UINT32_MAX;

#ifdef RT_ARCH_AMD64
    /* AMD64/x86 offers a bunch of options.  Smaller stuff will can be
       completely inlined, for larger we use REPE CMPS.  */
# define CHECK_OPCODES_CMP_IMMXX(a_idxReg, a_bOpcode) /* cost: 3 bytes */  do { \
            pbCodeBuf[off++] = a_bOpcode; \
            Assert(offPage < 127); \
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, 7, a_idxReg); \
            pbCodeBuf[off++] = RT_BYTE1(offPage); \
        } while (0)

# define CHECK_OPCODES_CMP_JMP() /* cost: 7 bytes first time, then 2 bytes */ do { \
            if (offConsolidatedJump != UINT32_MAX) \
            { \
                int32_t const offDisp = (int32_t)offConsolidatedJump - (int32_t)(off + 2); \
                Assert(offDisp >= -128); \
                pbCodeBuf[off++] = 0x75; /* jnz near */ \
                pbCodeBuf[off++] = (uint8_t)offDisp; \
            } \
            else \
            { \
                pbCodeBuf[off++] = 0x74; /* jz near +5 */ \
                pbCodeBuf[off++] = 0x05 + BP_ON_OBSOLETION; \
                offConsolidatedJump = off; \
                if (BP_ON_OBSOLETION) pbCodeBuf[off++] = 0xcc; \
                pbCodeBuf[off++] = 0xe9; /* jmp rel32 */ \
                iemNativeAddFixup(pReNative, off, idxLabelObsoleteTb, kIemNativeFixupType_Rel32, -4); \
                pbCodeBuf[off++] = 0x00; \
                pbCodeBuf[off++] = 0x00; \
                pbCodeBuf[off++] = 0x00; \
                pbCodeBuf[off++] = 0x00; \
            } \
        } while (0)

# define CHECK_OPCODES_CMP_IMM32(a_idxReg) /* cost: 3+4+2 = 9 */ do { \
        CHECK_OPCODES_CMP_IMMXX(a_idxReg, 0x81); \
        pbCodeBuf[off++] = *pbOpcodes++; \
        pbCodeBuf[off++] = *pbOpcodes++; \
        pbCodeBuf[off++] = *pbOpcodes++; \
        pbCodeBuf[off++] = *pbOpcodes++; \
        cbLeft  -= 4; \
        offPage += 4; \
        CHECK_OPCODES_CMP_JMP(); \
    } while (0)

# define CHECK_OPCODES_CMP_IMM16(a_idxReg) /* cost: 1+3+2+2 = 8 */ do { \
        pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP; \
        CHECK_OPCODES_CMP_IMMXX(a_idxReg, 0x81); \
        pbCodeBuf[off++] = *pbOpcodes++; \
        pbCodeBuf[off++] = *pbOpcodes++; \
        cbLeft  -= 2; \
        offPage += 2; \
        CHECK_OPCODES_CMP_JMP(); \
    } while (0)

# define CHECK_OPCODES_CMP_IMM8(a_idxReg) /* cost: 3+1+2 = 6 */ do { \
        CHECK_OPCODES_CMP_IMMXX(a_idxReg, 0x80); \
        pbCodeBuf[off++] = *pbOpcodes++; \
        cbLeft  -= 1; \
        offPage += 1; \
        CHECK_OPCODES_CMP_JMP(); \
    } while (0)

# define CHECK_OPCODES_CMPSX(a_bOpcode, a_cbToSubtract, a_bPrefix) /* cost: 2+2 = 4 */ do { \
        if (a_bPrefix) \
            pbCodeBuf[off++] = (a_bPrefix); \
        pbCodeBuf[off++] = (a_bOpcode); \
        CHECK_OPCODES_CMP_JMP(); \
        cbLeft -= (a_cbToSubtract); \
    } while (0)

# define CHECK_OPCODES_ECX_IMM(a_uValue) /* cost: 5 */ do { \
        pbCodeBuf[off++] = 0xb8 + X86_GREG_xCX; \
        pbCodeBuf[off++] = RT_BYTE1(a_uValue); \
        pbCodeBuf[off++] = RT_BYTE2(a_uValue); \
        pbCodeBuf[off++] = RT_BYTE3(a_uValue); \
        pbCodeBuf[off++] = RT_BYTE4(a_uValue); \
    } while (0)

    if (cbLeft <= 24)
    {
        uint8_t const idxRegTmp = iemNativeRegAllocTmpEx(pReNative, &off,
                                                           (  RT_BIT_32(X86_GREG_xAX)
                                                            | RT_BIT_32(X86_GREG_xCX)
                                                            | RT_BIT_32(X86_GREG_xDX)
                                                            | RT_BIT_32(X86_GREG_xBX)
                                                            | RT_BIT_32(X86_GREG_xSI)
                                                            | RT_BIT_32(X86_GREG_xDI))
                                                         & ~IEMNATIVE_REG_FIXED_MASK); /* pick reg not requiring rex prefix */
        off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.pbInstrBuf));
        if (offPage >= 128 - cbLeft)
        {
            off = iemNativeEmitAddGprImm(pReNative, off, idxRegTmp, offPage & ~(uint16_t)3);
            offPage &= 3;
        }

        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5 + 14 + 54 + 8 + 6 + BP_ON_OBSOLETION /* = 87 */);

        if (cbLeft > 8)
            switch (offPage & 3)
            {
                case 0:
                    break;
                case 1: /* cost: 6 + 8 = 14 */
                    CHECK_OPCODES_CMP_IMM8(idxRegTmp);
                    RT_FALL_THRU();
                case 2: /* cost: 8 */
                    CHECK_OPCODES_CMP_IMM16(idxRegTmp);
                    break;
                case 3: /* cost: 6 */
                    CHECK_OPCODES_CMP_IMM8(idxRegTmp);
                    break;
            }

        while (cbLeft >= 4)
            CHECK_OPCODES_CMP_IMM32(idxRegTmp);     /* max iteration: 24/4 = 6; --> cost: 6 * 9 = 54 */

        if (cbLeft >= 2)
            CHECK_OPCODES_CMP_IMM16(idxRegTmp);     /* cost: 8 */
        if (cbLeft)
            CHECK_OPCODES_CMP_IMM8(idxRegTmp);      /* cost: 6 */

        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        iemNativeRegFreeTmp(pReNative, idxRegTmp);
    }
    else
    {
        /* RDI = &pbInstrBuf[offPage] */
        uint8_t const idxRegDi = iemNativeRegAllocTmpEx(pReNative, &off, RT_BIT_32(X86_GREG_xDI));
        off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegDi, RT_UOFFSETOF(VMCPU, iem.s.pbInstrBuf));
        if (offPage != 0)
            off = iemNativeEmitAddGprImm(pReNative, off, idxRegDi, offPage);

        /* RSI = pbOpcodes */
        uint8_t const idxRegSi = iemNativeRegAllocTmpEx(pReNative, &off, RT_BIT_32(X86_GREG_xSI));
        off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegSi, (uintptr_t)pbOpcodes);

        /* RCX = counts. */
        uint8_t const idxRegCx = iemNativeRegAllocTmpEx(pReNative, &off, RT_BIT_32(X86_GREG_xCX));

        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5 + 10 + 5 + 5 + 3 + 4 + 3 + BP_ON_OBSOLETION /*= 35*/);

        /** @todo profile and optimize this further.  Maybe an idea to align by
         *        offPage if the two cannot be reconsidled. */
        /* Align by the page offset, so that at least one of the accesses are naturally aligned. */
        switch (offPage & 7)                                            /* max cost: 10 */
        {
            case 0:
                break;
            case 1: /* cost: 3+4+3 = 10 */
                CHECK_OPCODES_CMPSX(0xa6, 1, 0);
                RT_FALL_THRU();
            case 2: /* cost: 4+3 = 7 */
                CHECK_OPCODES_CMPSX(0xa7, 2, X86_OP_PRF_SIZE_OP);
                CHECK_OPCODES_CMPSX(0xa7, 4, 0);
                break;
            case 3: /* cost: 3+3 = 6 */
                CHECK_OPCODES_CMPSX(0xa6, 1, 0);
                RT_FALL_THRU();
            case 4: /* cost: 3 */
                CHECK_OPCODES_CMPSX(0xa7, 4, 0);
                break;
            case 5: /* cost: 3+4 = 7 */
                CHECK_OPCODES_CMPSX(0xa6, 1, 0);
                RT_FALL_THRU();
            case 6: /* cost: 4 */
                CHECK_OPCODES_CMPSX(0xa7, 2, X86_OP_PRF_SIZE_OP);
                break;
            case 7: /* cost: 3 */
                CHECK_OPCODES_CMPSX(0xa6, 1, 0);
                break;
        }

        /* Compare qwords: */
        uint32_t const cQWords = cbLeft >> 3;
        CHECK_OPCODES_ECX_IMM(cQWords);                                     /* cost: 5 */

        pbCodeBuf[off++] = X86_OP_PRF_REPZ;                                 /* cost: 5 */
        CHECK_OPCODES_CMPSX(0xa7, 0, X86_OP_REX_W);
        cbLeft &= 7;

        if (cbLeft & 4)
            CHECK_OPCODES_CMPSX(0xa7, 4, 0);                                /* cost: 3 */
        if (cbLeft & 2)
            CHECK_OPCODES_CMPSX(0xa7, 2, X86_OP_PRF_SIZE_OP);               /* cost: 4 */
        if (cbLeft & 1)
            CHECK_OPCODES_CMPSX(0xa6, 1, 0);                                /* cost: 3 */

        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        iemNativeRegFreeTmp(pReNative, idxRegCx);
        iemNativeRegFreeTmp(pReNative, idxRegSi);
        iemNativeRegFreeTmp(pReNative, idxRegDi);
    }

#elif defined(RT_ARCH_ARM64)
    /* We need pbInstrBuf in a register, whatever we do. */
    uint8_t const idxRegSrc1Ptr = iemNativeRegAllocTmp(pReNative, &off);
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegSrc1Ptr, RT_UOFFSETOF(VMCPU, iem.s.pbInstrBuf));

    /* We also need at least one more register for holding bytes & words we
       load via pbInstrBuf. */
    uint8_t const idxRegSrc1Val = iemNativeRegAllocTmp(pReNative, &off);

    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 64);

    /* One byte compare can be done with the opcode byte as an immediate. We'll
       do this to uint16_t align src1. */
    bool fPendingJmp = RT_BOOL(offPage & 1);
    if (fPendingJmp)
    {
        pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Byte, idxRegSrc1Val, idxRegSrc1Ptr, offPage);
        pu32CodeBuf[off++] = Armv8A64MkInstrCmpUImm12(idxRegSrc1Val, *pbOpcodes++, false /*f64Bit*/);
        offPage += 1;
        cbLeft  -= 1;
    }

    if (cbLeft > 0)
    {
        /* We need a register for holding the opcode bytes we're comparing with,
           as CCMP only has a 5-bit immediate form and thus cannot hold bytes. */
        uint8_t const idxRegSrc2Val = iemNativeRegAllocTmp(pReNative, &off);

        /* Word (uint32_t) aligning the src1 pointer is best done using a 16-bit constant load. */
        if ((offPage & 3) && cbLeft >= 2)
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Half, idxRegSrc1Val, idxRegSrc1Ptr, offPage / 2);
            pu32CodeBuf[off++] = Armv8A64MkInstrMovZ(idxRegSrc2Val, RT_MAKE_U16(pbOpcodes[0], pbOpcodes[1]));
            if (fPendingJmp)
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, false /*f64Bit*/);
            else
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val, false /*f64Bit*/);
                fPendingJmp = true;
            }
            pbOpcodes += 2;
            offPage   += 2;
            cbLeft    -= 2;
        }

        /* DWord (uint64_t) aligning the src2 pointer. We use a 32-bit constant here for simplicitly. */
        if ((offPage & 7) && cbLeft >= 4)
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Word, idxRegSrc1Val, idxRegSrc1Ptr, offPage / 4);
            off = iemNativeEmitLoadGpr32ImmEx(pu32CodeBuf, off, idxRegSrc2Val,
                                              RT_MAKE_U32_FROM_MSB_U8(pbOpcodes[3], pbOpcodes[2], pbOpcodes[1], pbOpcodes[0]));
            if (fPendingJmp)
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, false /*f64Bit*/);
            else
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val, false /*f64Bit*/);
                fPendingJmp = true;
            }
            pbOpcodes += 4;
            offPage   += 4;
            cbLeft    -= 4;
        }

        /*
         * If we've got 16 bytes or more left, switch to memcmp-style.
         */
        if (cbLeft >= 16)
        {
            /* We need a pointer to the copy of the original opcode bytes. */
            uint8_t const idxRegSrc2Ptr = iemNativeRegAllocTmp(pReNative, &off);
            off = iemNativeEmitLoadGprImmEx(pu32CodeBuf, off, idxRegSrc2Ptr, (uintptr_t)pbOpcodes);

            /* If there are more than 32 bytes to compare we create a loop, for
               which we'll need a loop register. */
            if (cbLeft >= 64)
            {
                if (fPendingJmp)
                {
                    iemNativeAddFixup(pReNative, off, idxLabelObsoleteTb, kIemNativeFixupType_RelImm19At5);
                    pu32CodeBuf[off++] = Armv8A64MkInstrBCond(kArmv8InstrCond_Ne, 0);
                    fPendingJmp = false;
                }

                uint8_t const  idxRegLoop = iemNativeRegAllocTmp(pReNative, &off);
                uint16_t const cLoops     = cbLeft / 32;
                cbLeft                    = cbLeft % 32;
                pbOpcodes                += cLoops * 32;
                pu32CodeBuf[off++] = Armv8A64MkInstrMovZ(idxRegLoop, cLoops);

                if (offPage != 0) /** @todo optimize out this instruction. */
                {
                    pu32CodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegSrc1Ptr, idxRegSrc1Ptr, offPage);
                    offPage = 0;
                }

                uint32_t const offLoopStart = off;
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val, idxRegSrc1Ptr, 0);
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc2Val, idxRegSrc2Ptr, 0);
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val);

                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val, idxRegSrc1Ptr, 1);
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc2Val, idxRegSrc2Ptr, 1);
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq);

                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val, idxRegSrc1Ptr, 2);
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc2Val, idxRegSrc2Ptr, 2);
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq);

                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val, idxRegSrc1Ptr, 3);
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc2Val, idxRegSrc2Ptr, 3);
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq);

                iemNativeAddFixup(pReNative, off, idxLabelObsoleteTb, kIemNativeFixupType_RelImm19At5);
                pu32CodeBuf[off++] = Armv8A64MkInstrBCond(kArmv8InstrCond_Ne, 0);

                /* Advance and loop. */
                pu32CodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegSrc1Ptr, idxRegSrc1Ptr, 0x20);
                pu32CodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegSrc2Ptr, idxRegSrc2Ptr, 0x20);
                pu32CodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegLoop, idxRegLoop, 1, false /*f64Bit*/, true /*fSetFlags*/);
                pu32CodeBuf[off++] = Armv8A64MkInstrBCond(kArmv8InstrCond_Ne, (int32_t)offLoopStart - (int32_t)off);

                iemNativeRegFreeTmp(pReNative, idxRegLoop);
            }

            /* Deal with any remaining dwords (uint64_t).  There can be up to
               three if we looped and four if we didn't. */
            uint32_t offSrc2 = 0;
            while (cbLeft >= 8)
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val,
                                                              idxRegSrc1Ptr, offPage / 8);
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc2Val,
                                                              idxRegSrc2Ptr, offSrc2 / 8);
                if (fPendingJmp)
                    pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                                ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq);
                else
                {
                    pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val);
                    fPendingJmp = true;
                }
                pbOpcodes += 8;
                offPage   += 8;
                offSrc2   += 8;
                cbLeft    -= 8;
            }

            iemNativeRegFreeTmp(pReNative, idxRegSrc2Ptr);
            /* max cost thus far: memcmp-loop=43 vs memcmp-no-loop=30 */
        }
        /*
         * Otherwise, we compare with constants and merge with the general mop-up.
         */
        else
        {
            while (cbLeft >= 8)
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Dword, idxRegSrc1Val, idxRegSrc1Ptr,
                                                              offPage / 8);
                off = iemNativeEmitLoadGprImmEx(pu32CodeBuf, off, idxRegSrc2Val,
                                                RT_MAKE_U64_FROM_MSB_U8(pbOpcodes[7], pbOpcodes[6], pbOpcodes[5], pbOpcodes[4],
                                                                        pbOpcodes[3], pbOpcodes[2], pbOpcodes[1], pbOpcodes[0]));
                if (fPendingJmp)
                    pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                                ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, true /*f64Bit*/);
                else
                {
                    pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val, true /*f64Bit*/);
                    fPendingJmp = true;
                }
                pbOpcodes += 8;
                offPage   += 8;
                cbLeft    -= 8;
            }
            /* max cost thus far: 21 */
        }

        /* Deal with any remaining bytes (7 or less). */
        Assert(cbLeft < 8);
        if (cbLeft >= 4)
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Word, idxRegSrc1Val, idxRegSrc1Ptr,
                                                          offPage / 4);
            off = iemNativeEmitLoadGpr32ImmEx(pu32CodeBuf, off, idxRegSrc2Val,
                                              RT_MAKE_U32_FROM_MSB_U8(pbOpcodes[3], pbOpcodes[2], pbOpcodes[1], pbOpcodes[0]));
            if (fPendingJmp)
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, false /*f64Bit*/);
            else
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val, false /*f64Bit*/);
                fPendingJmp = true;
            }
            pbOpcodes += 4;
            offPage   += 4;
            cbLeft    -= 4;

        }

        if (cbLeft >= 2)
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Half, idxRegSrc1Val, idxRegSrc1Ptr,
                                                          offPage / 2);
            pu32CodeBuf[off++] = Armv8A64MkInstrMovZ(idxRegSrc2Val, RT_MAKE_U16(pbOpcodes[0], pbOpcodes[1]));
            if (fPendingJmp)
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, false /*f64Bit*/);
            else
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpReg(idxRegSrc1Val, idxRegSrc2Val, false /*f64Bit*/);
                fPendingJmp = true;
            }
            pbOpcodes += 2;
            offPage   += 2;
            cbLeft    -= 2;
        }

        if (cbLeft > 0)
        {
            Assert(cbLeft == 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrStLdRUOff(kArmv8A64InstrLdStType_Ld_Byte, idxRegSrc1Val, idxRegSrc1Ptr, offPage);
            if (fPendingJmp)
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrMovZ(idxRegSrc2Val, pbOpcodes[0]);
                pu32CodeBuf[off++] = Armv8A64MkInstrCCmpReg(idxRegSrc1Val, idxRegSrc2Val,
                                                            ARMA64_NZCV_F_N0_Z0_C0_V0, kArmv8InstrCond_Eq, false /*f64Bit*/);
            }
            else
            {
                pu32CodeBuf[off++] = Armv8A64MkInstrCmpUImm12(idxRegSrc1Val, pbOpcodes[0], false /*f64Bit*/);
                fPendingJmp = true;
            }
            pbOpcodes += 1;
            offPage   += 1;
            cbLeft    -= 1;
        }

        iemNativeRegFreeTmp(pReNative, idxRegSrc2Val);
    }
    Assert(cbLeft == 0);

    /*
     * Finally, the branch on difference.
     */
    if (fPendingJmp)
    {
        iemNativeAddFixup(pReNative, off, idxLabelObsoleteTb, kIemNativeFixupType_RelImm19At5);
        pu32CodeBuf[off++] = Armv8A64MkInstrBCond(kArmv8InstrCond_Ne, 0);
    }
    RT_NOREF(pu32CodeBuf, cbLeft, offPage, pbOpcodes, offConsolidatedJump, idxLabelObsoleteTb);

    /* max costs: memcmp-loop=54; memcmp-no-loop=41; only-src1-ptr=32 */
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    iemNativeRegFreeTmp(pReNative, idxRegSrc1Val);
    iemNativeRegFreeTmp(pReNative, idxRegSrc1Ptr);

#else
# error "Port me"
#endif
    return off;
}


/** Duplicated in IEMAllThrdFuncsBltIn.cpp. */
DECL_FORCE_INLINE(RTGCPHYS) iemTbGetRangePhysPageAddr(PCIEMTB pTb, uint8_t idxRange)
{
    Assert(idxRange < RT_MIN(pTb->cRanges, RT_ELEMENTS(pTb->aRanges)));
    uint8_t const idxPage = pTb->aRanges[idxRange].idxPhysPage;
    Assert(idxPage <= RT_ELEMENTS(pTb->aGCPhysPages));
    if (idxPage == 0)
        return pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
    Assert(!(pTb->aGCPhysPages[idxPage - 1] & GUEST_PAGE_OFFSET_MASK));
    return pTb->aGCPhysPages[idxPage - 1];
}


/**
 * Macro that implements PC check after a conditional branch.
 */
#define BODY_CHECK_PC_AFTER_BRANCH(a_pTb, a_idxRange, a_offRange, a_cbInstr)  \
    RT_NOREF(a_cbInstr); \
    off = iemNativeEmitBltInCheckPcAfterBranch(pReNative, off, a_pTb, a_idxRange, a_offRange)

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInCheckPcAfterBranch(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTB pTb,
                                     uint8_t idxRange, uint16_t offRange)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000004);
#endif

    /*
     * The GCPhysRangePageWithOffset value in the threaded function is a fixed
     * constant for us here.
     *
     * We can pretend that iem.s.cbInstrBufTotal is X86_PAGE_SIZE here, because
     * it serves no purpose as a CS.LIM, if that's needed we've just performed
     * it, and as long as we don't implement code TLB reload code here there is
     * no point in checking that the TLB data we're using is still valid.
     *
     * What we to do is.
     *      1. Calculate the FLAT PC (RIP + CS.BASE).
     *      2. Subtract iem.s.uInstrBufPc from it and getting 'off'.
     *      3. The 'off' must be less than X86_PAGE_SIZE/cbInstrBufTotal or
     *         we're in the wrong spot and need to find a new TB.
     *      4. Add 'off' to iem.s.GCPhysInstrBuf and compare with the
     *         GCPhysRangePageWithOffset constant mentioned above.
     *
     * The adding of CS.BASE to RIP can be skipped in the first step if we're
     * in 64-bit code or flat 32-bit.
     */

    /* Allocate registers for step 1. Get the shadowed stuff before allocating
       the temp register, so we don't accidentally clobber something we'll be
       needing again immediately.  This is why we get idxRegCsBase here. */
    uint8_t const  idxRegPc     = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc,
                                                                  kIemNativeGstRegUse_ReadOnly);
    uint8_t const  idxRegCsBase = IEM_F_MODE_X86_IS_FLAT(pReNative->fExec) ? UINT8_MAX
                                : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_BASE(X86_SREG_CS),
                                                                 kIemNativeGstRegUse_ReadOnly);

    uint8_t const  idxRegTmp    = iemNativeRegAllocTmp(pReNative, &off);

#ifdef VBOX_STRICT
    /* Do assertions before idxRegTmp contains anything. */
    Assert(RT_SIZEOFMEMB(VMCPUCC, iem.s.cbInstrBufTotal) == sizeof(uint16_t));
# ifdef RT_ARCH_AMD64
    {
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8+2+1 + 11+2+1);
        /* Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_F_MODE_X86_IS_FLAT(pReNative->fExec)); */
        if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
        {
            /* cmp r/m64, imm8 */
            pbCodeBuf[off++] = X86_OP_REX_W;
            pbCodeBuf[off++] = 0x83;
            off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, 7, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.cs.u64Base));
            pbCodeBuf[off++] = 0;
            /* je rel8 */
            pbCodeBuf[off++] = 0x74;
            pbCodeBuf[off++] = 1;
            /* int3 */
            pbCodeBuf[off++] = 0xcc;

        }

        /* Assert(!(pVCpu->iem.s.GCPhysInstrBuf & X86_PAGE_OFFSET_MASK)); - done later by the non-x86 code */
        /* test r/m64, imm32 */
        pbCodeBuf[off++] = X86_OP_REX_W;
        pbCodeBuf[off++] = 0xf7;
        off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, 0, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
        pbCodeBuf[off++] = RT_BYTE1(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE2(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE3(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE4(X86_PAGE_OFFSET_MASK);
        /* jz rel8 */
        pbCodeBuf[off++] = 0x74;
        pbCodeBuf[off++] = 1;
        /* int3 */
        pbCodeBuf[off++] = 0xcc;
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
# else

    /* Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_F_MODE_X86_IS_FLAT(pReNative->fExec)); */
    if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
    {
        off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.cs.u64Base));
# ifdef RT_ARCH_ARM64
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, idxRegTmp);
        pu32CodeBuf[off++] = Armv8A64MkInstrBrk(0x2004);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
# else
#  error "Port me!"
# endif
    }
# endif

#endif /* VBOX_STRICT */

    /* 1+2. Calculate 'off' first (into idxRegTmp). */
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.uInstrBufPc));
    if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
    {
#ifdef RT_ARCH_ARM64
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegTmp, idxRegPc, idxRegTmp);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
#else
        off = iemNativeEmitNegGpr(pReNative, off, idxRegTmp);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegPc);
#endif
    }
    else
    {
#ifdef RT_ARCH_ARM64
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegTmp, idxRegCsBase, idxRegTmp);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegTmp, idxRegTmp, idxRegPc);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
#else
        off = iemNativeEmitNegGpr(pReNative, off, idxRegTmp);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegCsBase);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegPc);
#endif
        iemNativeRegFreeTmp(pReNative, idxRegCsBase);
    }
    iemNativeRegFreeTmp(pReNative, idxRegPc);

    /* 3. Check that off is less than X86_PAGE_SIZE/cbInstrBufTotal. */
    off = iemNativeEmitCmpGprWithImm(pReNative, off, idxRegTmp, X86_PAGE_SIZE - 1);
    off = iemNativeEmitJaToNewLabel(pReNative, off, kIemNativeLabelType_CheckBranchMiss);

    /* 4. Add iem.s.GCPhysInstrBuf and compare with GCPhysRangePageWithOffset. */
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    pbCodeBuf[off++] = idxRegTmp < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x03; /* add r64, r/m64 */
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)
    uint8_t const idxRegTmp2 = iemNativeRegAllocTmp(pReNative, &off);

    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp2, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegTmp, idxRegTmp, idxRegTmp2);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

# ifdef VBOX_STRICT /* Assert(!(pVCpu->iem.s.GCPhysInstrBuf & X86_PAGE_OFFSET_MASK)); */
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegTmp2, X86_PAGE_OFFSET_MASK, true /*fSetFlags*/);
    off = iemNativeEmitJzToFixed(pReNative, off, off + 2 /* correct for ARM64 */);
    off = iemNativeEmitBrk(pReNative, off, 0x2005);
# endif
    iemNativeRegFreeTmp(pReNative, idxRegTmp2);
#else
# error "Port me"
#endif

    RTGCPHYS const GCPhysRangePageWithOffset = (  iemTbGetRangePhysPageAddr(pTb, idxRange)
                                                | pTb->aRanges[idxRange].offPhysPage)
                                             + offRange;
    off = iemNativeEmitTestIfGprNotEqualImmAndJmpToNewLabel(pReNative, off, idxRegTmp, GCPhysRangePageWithOffset,
                                                            kIemNativeLabelType_CheckBranchMiss);

    iemNativeRegFreeTmp(pReNative, idxRegTmp);
    return off;
}


/**
 * Macro that implements TLB loading and updating pbInstrBuf updating for an
 * instruction crossing into a new page.
 *
 * This may long jump if we're raising a \#PF, \#GP or similar trouble.
 */
#define BODY_LOAD_TLB_FOR_NEW_PAGE(a_pTb, a_offInstr, a_idxRange, a_cbInstr) \
    RT_NOREF(a_cbInstr); \
    off = iemNativeEmitBltLoadTlbForNewPage(pReNative, off, pTb, a_idxRange, a_offInstr)

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltLoadTlbForNewPage(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTB pTb, uint8_t idxRange, uint8_t offInstr)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000005);
#endif

    /*
     * Move/spill/flush stuff out of call-volatile registers.
     * This is the easy way out. We could contain this to the tlb-miss branch
     * by saving and restoring active stuff here.
     */
    /** @todo save+restore active registers and maybe guest shadows in tlb-miss.  */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /* vacate all non-volatile regs */);

    /*
     * Define labels and allocate the register for holding the GCPhys of the new page.
     */
    uint16_t const uTlbSeqNo        = pReNative->uTlbSeqNo++;
    uint32_t const idxLabelTlbMiss  = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, UINT32_MAX, uTlbSeqNo);
    uint32_t const idxLabelTlbDone  = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbDone, UINT32_MAX, uTlbSeqNo);
    uint32_t const idxRegGCPhys     = iemNativeRegAllocTmp(pReNative, &off);

    /*
     * First we try to go via the TLB.
     */
    /** @todo  */

    /*
     * TLB miss: Call iemNativeHlpMemCodeNewPageTlbMissWithOff to do the work.
     */
    iemNativeLabelDefine(pReNative, idxLabelTlbMiss, off);

    /* IEMNATIVE_CALL_ARG1_GREG = offInstr */
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, offInstr);

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpMemCodeNewPageTlbMissWithOff);

    /* Move the result to the right register. */
    if (idxRegGCPhys != IEMNATIVE_CALL_RET_GREG)
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegGCPhys, IEMNATIVE_CALL_RET_GREG);

    iemNativeLabelDefine(pReNative, idxLabelTlbDone, off);

    /*
     * Now check the physical address of the page matches the expected one.
     */
    RTGCPHYS const GCPhysNewPage = iemTbGetRangePhysPageAddr(pTb, idxRange);
    off = iemNativeEmitTestIfGprNotEqualImmAndJmpToNewLabel(pReNative, off, idxRegGCPhys, GCPhysNewPage,
                                                            kIemNativeLabelType_ObsoleteTb);

    iemNativeRegFreeTmp(pReNative, idxRegGCPhys);
    return off;
}


/**
 * Macro that implements TLB loading and updating pbInstrBuf updating when
 * branching or when crossing a page on an instruction boundrary.
 *
 * This differs from BODY_LOAD_TLB_FOR_NEW_PAGE in that it will first check if
 * it is an inter-page branch and also check the page offset.
 *
 * This may long jump if we're raising a \#PF, \#GP or similar trouble.
 */
#define BODY_LOAD_TLB_AFTER_BRANCH(a_pTb, a_idxRange, a_cbInstr) \
    RT_NOREF(a_cbInstr); \
    off = iemNativeEmitBltLoadTlbAfterBranch(pReNative, off, pTb, a_idxRange)

#if 0
do { \
        /* Is RIP within the current code page? */ \
        Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_IS_64BIT_CODE(pVCpu)); \
        uint64_t const uPc = pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base; \
        uint64_t const off = uPc - pVCpu->iem.s.uInstrBufPc; \
        if (off < pVCpu->iem.s.cbInstrBufTotal) \
        { \
            Assert(!(pVCpu->iem.s.GCPhysInstrBuf & GUEST_PAGE_OFFSET_MASK)); \
            Assert(pVCpu->iem.s.pbInstrBuf); \
            RTGCPHYS const GCPhysRangePageWithOffset = iemTbGetRangePhysPageAddr(a_pTb, a_idxRange) \
                                                     | pTb->aRanges[(a_idxRange)].offPhysPage; \
            if (GCPhysRangePageWithOffset == pVCpu->iem.s.GCPhysInstrBuf + off) \
            { /* we're good */ } \
            else \
            { \
                Log7(("TB jmp miss: %p at %04x:%08RX64 LB %u; branching/1; GCPhysWithOffset=%RGp expected %RGp, pbInstrBuf=%p - #%u\n", \
                      (a_pTb), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, (a_cbInstr), \
                      pVCpu->iem.s.GCPhysInstrBuf + off, GCPhysRangePageWithOffset, pVCpu->iem.s.pbInstrBuf, __LINE__)); \
                RT_NOREF(a_cbInstr); \
                STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatCheckBranchMisses); \
                return VINF_IEM_REEXEC_BREAK; \
            } \
        } \
        else \
        { \
            /* Must translate new RIP. */ \
            pVCpu->iem.s.pbInstrBuf       = NULL; \
            pVCpu->iem.s.offCurInstrStart = 0; \
            pVCpu->iem.s.offInstrNextByte = 0; \
            iemOpcodeFetchBytesJmp(pVCpu, 0, NULL); \
            Assert(!(pVCpu->iem.s.GCPhysInstrBuf & GUEST_PAGE_OFFSET_MASK) || !pVCpu->iem.s.pbInstrBuf); \
            \
            RTGCPHYS const GCPhysRangePageWithOffset = iemTbGetRangePhysPageAddr(a_pTb, a_idxRange) \
                                                     | pTb->aRanges[(a_idxRange)].offPhysPage; \
            uint64_t const offNew                    = uPc - pVCpu->iem.s.uInstrBufPc; \
            if (   GCPhysRangePageWithOffset == pVCpu->iem.s.GCPhysInstrBuf + offNew \
                && pVCpu->iem.s.pbInstrBuf) \
            { /* likely */ } \
            else \
            { \
                Log7(("TB jmp miss: %p at %04x:%08RX64 LB %u; branching/2; GCPhysWithOffset=%RGp expected %RGp, pbInstrBuf=%p - #%u\n", \
                      (a_pTb), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip, (a_cbInstr), \
                      pVCpu->iem.s.GCPhysInstrBuf + offNew, GCPhysRangePageWithOffset, pVCpu->iem.s.pbInstrBuf, __LINE__)); \
                RT_NOREF(a_cbInstr); \
                STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatCheckBranchMisses); \
                return VINF_IEM_REEXEC_BREAK; \
            } \
        } \
    } while(0)
#endif

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltLoadTlbAfterBranch(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTB pTb, uint8_t idxRange)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off, 0x80000006);
#endif

    /*
     * Define labels and allocate the register for holding the GCPhys of the new page.
     */
    uint32_t const idxLabelCheckBranchMiss = iemNativeLabelCreate(pReNative, kIemNativeLabelType_CheckBranchMiss);
    uint16_t const uTlbSeqNo        = pReNative->uTlbSeqNo++;
    uint32_t const idxLabelTlbMiss  = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, UINT32_MAX, uTlbSeqNo);
    //

    RTGCPHYS const GCPhysRangePageWithOffset = iemTbGetRangePhysPageAddr(pTb, idxRange)
                                             | pTb->aRanges[idxRange].offPhysPage;

    /*
     *
     * First check if RIP is within the current code.
     *
     * This is very similar to iemNativeEmitBltInCheckPcAfterBranch, the only
     * difference is what we do when stuff doesn't match up.
     *
     * What we to do is.
     *      1. Calculate the FLAT PC (RIP + CS.BASE).
     *      2. Subtract iem.s.uInstrBufPc from it and getting 'off'.
     *      3. The 'off' must be less than X86_PAGE_SIZE/cbInstrBufTotal or
     *         we need to retranslate RIP via the TLB.
     *      4. Add 'off' to iem.s.GCPhysInstrBuf and compare with the
     *         GCPhysRangePageWithOffset constant mentioned above.
     *
     * The adding of CS.BASE to RIP can be skipped in the first step if we're
     * in 64-bit code or flat 32-bit.
     *
     */

    /* Allocate registers for step 1. Get the shadowed stuff before allocating
       the temp register, so we don't accidentally clobber something we'll be
       needing again immediately.  This is why we get idxRegCsBase here. */
    /** @todo save+restore active registers and guest shadows in tlb-miss! */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /* vacate all non-volatile regs */);
    uint8_t const  idxRegPc     = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc,
                                                                  kIemNativeGstRegUse_ReadOnly, true /*fNoVolatileRegs*/);
    uint8_t const  idxRegCsBase = IEM_F_MODE_X86_IS_FLAT(pReNative->fExec) ? UINT8_MAX
                                : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_BASE(X86_SREG_CS),
                                                                 kIemNativeGstRegUse_ReadOnly, true /*fNoVolatileRegs*/);

    uint8_t const  idxRegTmp    = iemNativeRegAllocTmp(pReNative, &off); /* volatile reg is okay for these two */
    uint8_t const  idxRegTmp2   = iemNativeRegAllocTmp(pReNative, &off);

#ifdef VBOX_STRICT
    /* Do assertions before idxRegTmp contains anything. */
    Assert(RT_SIZEOFMEMB(VMCPUCC, iem.s.cbInstrBufTotal) == sizeof(uint16_t));
# ifdef RT_ARCH_AMD64
    {
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8+2+1 + 11+2+1);
        /* Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_F_MODE_X86_IS_FLAT(pReNative->fExec)); */
        if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
        {
            /* cmp r/m64, imm8 */
            pbCodeBuf[off++] = X86_OP_REX_W;
            pbCodeBuf[off++] = 0x83;
            off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, 7, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.cs.u64Base));
            pbCodeBuf[off++] = 0;
            /* je rel8 */
            pbCodeBuf[off++] = 0x74;
            pbCodeBuf[off++] = 1;
            /* int3 */
            pbCodeBuf[off++] = 0xcc;

        }

        /* Assert(!(pVCpu->iem.s.GCPhysInstrBuf & X86_PAGE_OFFSET_MASK)); - done later by the non-x86 code */
        /* test r/m64, imm32 */
        pbCodeBuf[off++] = X86_OP_REX_W;
        pbCodeBuf[off++] = 0xf7;
        off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, 0, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
        pbCodeBuf[off++] = RT_BYTE1(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE2(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE3(X86_PAGE_OFFSET_MASK);
        pbCodeBuf[off++] = RT_BYTE4(X86_PAGE_OFFSET_MASK);
        /* jz rel8 */
        pbCodeBuf[off++] = 0x74;
        pbCodeBuf[off++] = 1;
        /* int3 */
        pbCodeBuf[off++] = 0xcc;
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
# else

    /* Assert(pVCpu->cpum.GstCtx.cs.u64Base == 0 || !IEM_F_MODE_X86_IS_FLAT(pReNative->fExec)); */
    if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
    {
        off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.cs.u64Base));
# ifdef RT_ARCH_ARM64
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, idxRegTmp);
        pu32CodeBuf[off++] = Armv8A64MkInstrBrk(0x2006);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
# else
#  error "Port me!"
# endif
    }
# endif

#endif /* VBOX_STRICT */

    /* Because we're lazy, we'll jump back here to recalc 'off' and share the
       GCPhysRangePageWithOffset check.  This is a little risky, so we use the
       2nd register to check if we've looped more than once already.*/
    off = iemNativeEmitGprZero(pReNative, off, idxRegTmp2);

    uint32_t const offLabelRedoChecks = off;

    /* 1+2. Calculate 'off' first (into idxRegTmp). */
    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.uInstrBufPc));
    if (IEM_F_MODE_X86_IS_FLAT(pReNative->fExec))
    {
#ifdef RT_ARCH_ARM64
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegTmp, idxRegPc, idxRegTmp);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
#else
        off = iemNativeEmitNegGpr(pReNative, off, idxRegTmp);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegPc);
#endif
    }
    else
    {
#ifdef RT_ARCH_ARM64
        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        pu32CodeBuf[off++] = Armv8A64MkInstrSubReg(idxRegTmp, idxRegCsBase, idxRegTmp);
        pu32CodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegTmp, idxRegTmp, idxRegPc);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
#else
        off = iemNativeEmitNegGpr(pReNative, off, idxRegTmp);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegCsBase);
        off = iemNativeEmitAddTwoGprs(pReNative, off, idxRegTmp, idxRegPc);
#endif
    }

    /* 3. Check that off is less than X86_PAGE_SIZE/cbInstrBufTotal.
          Unlike iemNativeEmitBltInCheckPcAfterBranch we'll jump to the TLB loading if this fails. */
    off = iemNativeEmitCmpGprWithImm(pReNative, off, idxRegTmp, X86_PAGE_SIZE - 1);
    uint32_t const offFixedJumpToTlbLoad = off;
    off = iemNativeEmitJaToFixed(pReNative, off, off /* (ASSUME ja rel8 suffices) */);

    /* 4a. Add iem.s.GCPhysInstrBuf to off ... */
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    pbCodeBuf[off++] = idxRegTmp < 8 ? X86_OP_REX_W : X86_OP_REX_W | X86_OP_REX_R;
    pbCodeBuf[off++] = 0x03; /* add r64, r/m64 */
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#elif defined(RT_ARCH_ARM64)

    off = iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxRegTmp2, RT_UOFFSETOF(VMCPUCC, iem.s.GCPhysInstrBuf));
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddReg(idxRegTmp, idxRegTmp, idxRegTmp2);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

# ifdef VBOX_STRICT /* Assert(!(pVCpu->iem.s.GCPhysInstrBuf & X86_PAGE_OFFSET_MASK)); */
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegTmp2, X86_PAGE_OFFSET_MASK, true /*fSetFlags*/);
    off = iemNativeEmitJzToFixed(pReNative, off, off + 2 /* correct for ARM64 */);
    off = iemNativeEmitBrk(pReNative, off, 0x2005);
# endif
#else
# error "Port me"
#endif

    /* 4b. ... and compare with GCPhysRangePageWithOffset.

       Unlike iemNativeEmitBltInCheckPcAfterBranch we'll have to be more
       careful and avoid implicit temporary register usage here.

       Unlike the threaded version of this code, we do not obsolete TBs here to
       reduce the code size and because indirect calls may legally end at the
       same offset in two different pages depending on the program state. */
    /** @todo synch the threaded BODY_LOAD_TLB_AFTER_BRANCH version with this. */
    off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegTmp2, GCPhysRangePageWithOffset);
    off = iemNativeEmitCmpGprWithGpr(pReNative, off, idxRegTmp, idxRegTmp2);
    off = iemNativeEmitJnzToLabel(pReNative, off, idxLabelCheckBranchMiss);
    uint32_t const offFixedJumpToEnd = off;
    off = iemNativeEmitJmpToFixed(pReNative, off, off + 512 /* force rel32 */);

    /*
     * First we try to go via the TLB.
     */
    iemNativeFixupFixedJump(pReNative, offFixedJumpToTlbLoad, off);

    /* Check that we haven't been here before. */
    off = iemNativeEmitTestIfGprIsNotZeroAndJmpToLabel(pReNative, off, idxRegTmp2,  false /*f64Bit*/, idxLabelCheckBranchMiss);

    /*
     * TLB miss: Call iemNativeHlpMemCodeNewPageTlbMiss to do the work.
     */
    iemNativeLabelDefine(pReNative, idxLabelTlbMiss, off);

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpMemCodeNewPageTlbMiss);

    /* Jmp back to the start and redo the checks. */
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, idxRegTmp2, 1); /* indicate that we've looped once already */
    off = iemNativeEmitJmpToFixed(pReNative, off, offLabelRedoChecks);

    /* The end. */
    iemNativeFixupFixedJump(pReNative, offFixedJumpToEnd, off);

    iemNativeRegFreeTmp(pReNative, idxRegTmp2);
    iemNativeRegFreeTmp(pReNative, idxRegTmp);
    iemNativeRegFreeTmp(pReNative, idxRegPc);
    if (idxRegCsBase != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegCsBase);
    return off;
}


#ifdef BODY_CHECK_CS_LIM
/**
 * Built-in function that checks the EIP/IP + uParam0 is within CS.LIM,
 * raising a \#GP(0) if this isn't the case.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLim)
{
    uint32_t const cbInstr = (uint32_t)pCallEntry->auParams[0];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for re-checking opcodes and CS.LIM after an instruction
 * that may have modified them.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndOpcodes)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES)
/**
 * Built-in function for re-checking opcodes after an instruction that may have
 * modified them.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodes)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for re-checking opcodes and considering the need for CS.LIM
 * checking after an instruction that may have modified them.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesConsiderCsLim)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    return off;
}
#endif


/*
 * Post-branching checkers.
 */

#if defined(BODY_CHECK_OPCODES) && defined(BODY_CHECK_PC_AFTER_BRANCH) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for checking CS.LIM, checking the PC and checking opcodes
 * after conditional branching within the same page.
 *
 * @see iemThreadedFunc_BltIn_CheckPcAndOpcodes
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndPcAndOpcodes)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, offRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_CHECK_PC_AFTER_BRANCH)
/**
 * Built-in function for checking the PC and checking opcodes after conditional
 * branching within the same page.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndPcAndOpcodes
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckPcAndOpcodes)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, offRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_CHECK_PC_AFTER_BRANCH) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for checking the PC and checking opcodes and considering
 * the need for CS.LIM checking after conditional branching within the same
 * page.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndPcAndOpcodes
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckPcAndOpcodesConsiderCsLim)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, offRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_AFTER_BRANCH) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for checking CS.LIM, loading TLB and checking opcodes when
 * transitioning to a different code page.
 *
 * The code page transition can either be natural over onto the next page (with
 * the instruction starting at page offset zero) or by means of branching.
 *
 * @see iemThreadedFunc_BltIn_CheckOpcodesLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    Assert(offRange == 0);
    BODY_LOAD_TLB_AFTER_BRANCH(pTb, idxRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_AFTER_BRANCH)
/**
 * Built-in function for loading TLB and checking opcodes when transitioning to
 * a different code page.
 *
 * The code page transition can either be natural over onto the next page (with
 * the instruction starting at page offset zero) or by means of branching.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesLoadingTlb)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    Assert(offRange == 0);
    BODY_LOAD_TLB_AFTER_BRANCH(pTb, idxRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_AFTER_BRANCH) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for loading TLB and checking opcodes and considering the
 * need for CS.LIM checking when transitioning to a different code page.
 *
 * The code page transition can either be natural over onto the next page (with
 * the instruction starting at page offset zero) or by means of branching.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesLoadingTlbConsiderCsLim)
{
    PCIEMTB const  pTb      = pReNative->pTbOrg;
    uint32_t const cbInstr  = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange = (uint32_t)pCallEntry->auParams[2];
    //LogFunc(("idxRange=%u @ %#x LB %#x: offPhysPage=%#x LB %#x\n", idxRange, offRange, cbInstr, pTb->aRanges[idxRange].offPhysPage, pTb->aRanges[idxRange].cbOpcodes));
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    Assert(offRange == 0);
    BODY_LOAD_TLB_AFTER_BRANCH(pTb, idxRange, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange, offRange, cbInstr);
    //LogFunc(("okay\n"));
    return off;
}
#endif



/*
 * Natural page crossing checkers.
 */

#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for checking CS.LIM, loading TLB and checking opcodes on
 * both pages when transitioning to a different code page.
 *
 * This is used when the previous instruction requires revalidation of opcodes
 * bytes and the current instruction stries a page boundrary with opcode bytes
 * in both the old and new page.
 *
 * @see iemThreadedFunc_BltIn_CheckOpcodesAcrossPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange1   = (uint32_t)pCallEntry->auParams[2];
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange1, offRange1, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE)
/**
 * Built-in function for loading TLB and checking opcodes on both pages when
 * transitioning to a different code page.
 *
 * This is used when the previous instruction requires revalidation of opcodes
 * bytes and the current instruction stries a page boundrary with opcode bytes
 * in both the old and new page.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesAcrossPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange1   = (uint32_t)pCallEntry->auParams[2];
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_OPCODES(pTb, idxRange1, offRange1, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for loading TLB and checking opcodes on both pages and
 * considering the need for CS.LIM checking when transitioning to a different
 * code page.
 *
 * This is used when the previous instruction requires revalidation of opcodes
 * bytes and the current instruction stries a page boundrary with opcode bytes
 * in both the old and new page.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesAcrossPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesAcrossPageLoadingTlbConsiderCsLim)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    uint32_t const offRange1   = (uint32_t)pCallEntry->auParams[2];
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange1, offRange1, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for checking CS.LIM, loading TLB and checking opcodes when
 * advancing naturally to a different code page.
 *
 * Only opcodes on the new page is checked.
 *
 * @see iemThreadedFunc_BltIn_CheckOpcodesOnNextPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    //uint32_t const offRange1   = (uint32_t)uParam2;
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE)
/**
 * Built-in function for loading TLB and checking opcodes when advancing
 * naturally to a different code page.
 *
 * Only opcodes on the new page is checked.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesOnNextPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    //uint32_t const offRange1   = (uint32_t)pCallEntry->auParams[2];
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for loading TLB and checking opcodes and considering the
 * need for CS.LIM checking when advancing naturally to a different code page.
 *
 * Only opcodes on the new page is checked.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNextPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesOnNextPageLoadingTlbConsiderCsLim)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const cbStartPage = (uint32_t)(pCallEntry->auParams[0] >> 32);
    uint32_t const idxRange1   = (uint32_t)pCallEntry->auParams[1];
    //uint32_t const offRange1   = (uint32_t)pCallEntry->auParams[2];
    uint32_t const idxRange2   = idxRange1 + 1;
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, cbStartPage, idxRange2, cbInstr);
    BODY_CHECK_OPCODES(pTb, idxRange2, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CHECK_CS_LIM)
/**
 * Built-in function for checking CS.LIM, loading TLB and checking opcodes when
 * advancing naturally to a different code page with first instr at byte 0.
 *
 * @see iemThreadedFunc_BltIn_CheckOpcodesOnNewPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange    = (uint32_t)pCallEntry->auParams[1];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, 0, idxRange, cbInstr);
    //Assert(pVCpu->iem.s.offCurInstrStart == 0);
    BODY_CHECK_OPCODES(pTb, idxRange, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE)
/**
 * Built-in function for loading TLB and checking opcodes when advancing
 * naturally to a different code page with first instr at byte 0.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesOnNewPageLoadingTlb)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange    = (uint32_t)pCallEntry->auParams[1];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, 0, idxRange, cbInstr);
    //Assert(pVCpu->iem.s.offCurInstrStart == 0);
    BODY_CHECK_OPCODES(pTb, idxRange, 0, cbInstr);
    return off;
}
#endif


#if defined(BODY_CHECK_OPCODES) && defined(BODY_LOAD_TLB_FOR_NEW_PAGE) && defined(BODY_CONSIDER_CS_LIM_CHECKING)
/**
 * Built-in function for loading TLB and checking opcodes and considering the
 * need for CS.LIM checking when advancing naturally to a different code page
 * with first instr at byte 0.
 *
 * @see iemThreadedFunc_BltIn_CheckCsLimAndOpcodesOnNewPageLoadingTlb
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckOpcodesOnNewPageLoadingTlbConsiderCsLim)
{
    PCIEMTB const  pTb         = pReNative->pTbOrg;
    uint32_t const cbInstr     = (uint32_t)pCallEntry->auParams[0];
    uint32_t const idxRange    = (uint32_t)pCallEntry->auParams[1];
    BODY_SET_CUR_INSTR();
    BODY_FLUSH_PENDING_WRITES();
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, 0, idxRange, cbInstr);
    //Assert(pVCpu->iem.s.offCurInstrStart == 0);
    BODY_CHECK_OPCODES(pTb, idxRange, 0, cbInstr);
    return off;
}
#endif

