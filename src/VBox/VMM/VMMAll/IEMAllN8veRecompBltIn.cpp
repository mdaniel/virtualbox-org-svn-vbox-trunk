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
#define IEM_WITH_OPAQUE_DECODER_STATE
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



/*********************************************************************************************************************************
*   Builtin functions                                                                                                            *
*********************************************************************************************************************************/

/**
 * Built-in function that calls a C-implemention function taking zero arguments.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_DeferToCImpl0)
{
    PFNIEMCIMPL0 const pfnCImpl     = (PFNIEMCIMPL0)(uintptr_t)pCallEntry->auParams[0];
    uint8_t const      cbInstr      = (uint8_t)pCallEntry->auParams[1];
    uint64_t const     fGstShwFlush = (uint8_t)pCallEntry->auParams[2];
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
    off = iemNativeEmitJzToFixed(pReNative, off, 0);

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
    off = iemNativeEmitLoadGpr32ByGpr(pReNative, off, idxTmpReg, idxTmpReg, RT_UOFFSETOF(VMCC, fGlobalForcedActions));
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
 * Sets idxTbCurInstr in preparation of raising an exception.
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
 * Macro that emits the 16/32-bit CS.LIM check.
 */
#define BODY_CHECK_CS_LIM(a_cbInstr) \
    off = iemNativeEmitBltInCheckCsLim(pReNative, off, (a_cbInstr))

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInCheckCsLim(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    Assert(cbInstr >  0);
    Assert(cbInstr < 16);

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
    RT_NOREF(cbInstr); \
    off = iemNativeEmitBltInConsiderLimChecking(pReNative, off)

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInConsiderLimChecking(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
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
    RT_NOREF(cbInstr); \
    off = iemNativeEmitBltInCheckOpcodes(pReNative, off, (a_pTb), (a_idxRange), (a_offRange))

DECL_FORCE_INLINE(uint32_t)
iemNativeEmitBltInCheckOpcodes(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTB pTb, uint8_t idxRange, uint16_t offRange)
{
    Assert(idxRange < pTb->cRanges && pTb->cRanges <= RT_ELEMENTS(pTb->aRanges));
    Assert(offRange < pTb->aRanges[idxRange].cbOpcodes);

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
    uint8_t const      *pbOpcodes   = &pTb->pabOpcodes[pTb->aRanges[idxRange].offOpcodes];
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
                pbCodeBuf[off++] = 0x05; \
                offConsolidatedJump = off; \
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

        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5 + 14 + 54 + 8 + 6 /* = 87 */);

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

        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5 + 10 + 5 + 5 + 3 + 4 + 3 /*= 35*/);

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
            CHECK_OPCODES_CMPSX(0xa7, 0, 0);                                /* cost: 3 */
        if (cbLeft & 2)
            CHECK_OPCODES_CMPSX(0xa7, 0, X86_OP_PRF_SIZE_OP);               /* cost: 4 */
        if (cbLeft & 2)
            CHECK_OPCODES_CMPSX(0xa6, 0, 0);                                /* cost: 3 */

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


#ifdef BODY_CHECK_CS_LIM
/**
 * Built-in function that checks the EIP/IP + uParam0 is within CS.LIM,
 * raising a \#GP(0) if this isn't the case.
 */
IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_CheckCsLim)
{
    uint32_t const cbInstr = (uint32_t)pCallEntry->auParams[0];
    BODY_SET_CUR_INSTR();
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
    BODY_CHECK_CS_LIM(cbInstr);
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, cbInstr);
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
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, cbInstr);
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
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_CHECK_PC_AFTER_BRANCH(pTb, idxRange, cbInstr);
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
    BODY_CHECK_CS_LIM(cbInstr);
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
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
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
    BODY_CONSIDER_CS_LIM_CHECKING(pTb, cbInstr);
    BODY_LOAD_TLB_FOR_NEW_PAGE(pTb, 0, idxRange, cbInstr);
    //Assert(pVCpu->iem.s.offCurInstrStart == 0);
    BODY_CHECK_OPCODES(pTb, idxRange, 0, cbInstr);
    return off;
}
#endif

