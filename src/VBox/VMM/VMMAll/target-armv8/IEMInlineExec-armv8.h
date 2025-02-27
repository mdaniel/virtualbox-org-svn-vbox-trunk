/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - ARMv8 target, Inline Exec/Decoder routines.
 *
 * Target specific stuff for IEMAll.cpp.
 */

/*
 * Copyright (C) 2011-2025 Oracle and/or its affiliates.
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


#ifndef VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInlineExec_armv8_h
#define VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInlineExec_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


DECL_FORCE_INLINE(VBOXSTRICTRC) iemExecDecodeAndInterpretTargetInstruction(PVMCPUCC pVCpu)
{
#if 1
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
#else
    uint32_t const u32 = iemOpcodeGetU32Jmp(pVCpu);
    return FNIEMOP_CALL_1(g_apfnIemInterpretOnly???[u32 & ???], u32);
#endif
}


DECL_FORCE_INLINE(uint64_t) iemRegGetPC(PVMCPUCC pVCpu)
{
    return pVCpu->cpum.GstCtx.Pc.u64;
}


DECL_FORCE_INLINE(bool) iemExecLoopTargetCheckMaskedCpuFFs(PVMCPUCC pVCpu, uint64_t fCpuForceFlags)
{
    /* No FFs (irrelevant ones have already been masked out): */
    if (!fCpuForceFlags)
        return true;

    /* Remove IRQ and FIQ FFs that are masked by PSTATE and check if anything is left. */
    AssertCompile(VMCPU_FF_INTERRUPT_IRQ_BIT < ARMV8_SPSR_EL2_AARCH64_I_BIT);
    AssertCompile(VMCPU_FF_INTERRUPT_FIQ_BIT < ARMV8_SPSR_EL2_AARCH64_F_BIT);
#if 1 /** @todo ARMV8_SPSR_EL2_AARCH64_F/I are bits 6 and 7 respectively, while the
       * VMCPU_FF_INTERRUPT_FIQ/IRQ are order reversely (bits 1 and 0 respectively).
       * This makes it more tedious to ignore the masked FF here! */
    fCpuForceFlags &= ~(  (  (pVCpu->cpum.GstCtx.fPState >> (ARMV8_SPSR_EL2_AARCH64_I_BIT - VMCPU_FF_INTERRUPT_IRQ_BIT))
                           & VMCPU_FF_INTERRUPT_IRQ)
                        | (  (pVCpu->cpum.GstCtx.fPState >> (ARMV8_SPSR_EL2_AARCH64_F_BIT - VMCPU_FF_INTERRUPT_FIQ_BIT))
                           & VMCPU_FF_INTERRUPT_FIQ) );
#else
    AssertCompile(VMCPU_FF_INTERRUPT_FIQ_BIT   + 1 == VMCPU_FF_INTERRUPT_IRQ_BIT);
    AssertCompile(ARMV8_SPSR_EL2_AARCH64_F_BIT + 1 == ARMV8_SPSR_EL2_AARCH64_I_BIT);
    fCpuForceFlags &= ~(  (pVCpu->cpum.GstCtx.fPState >> (ARMV8_SPSR_EL2_AARCH64_F_BIT - VMCPU_FF_INTERRUPT_IRQ_BIT))
                        & (VMCPU_FF_INTERRUPT_FIQ | VMCPU_FF_INTERRUPT_IRQ) );
#endif
    return !fCpuForceFlags;
}

#ifdef VBOX_STRICT

DECLINLINE(void) iemInitDecoderStrictTarget(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


DECLINLINE(void) iemInitExecTailStrictTarget(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


DECLINLINE(void) iemInitExecTargetStrict(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    iemInitDecoderStrictTarget(pVCpu);

#  ifdef IEM_WITH_CODE_TLB
    pVCpu->iem.s.offInstrNextByte   = UINT16_MAX;
    pVCpu->iem.s.pbInstrBuf         = NULL;
    pVCpu->iem.s.cbInstrBufTotal    = UINT16_MAX;
    pVCpu->iem.s.uInstrBufPc        = UINT64_C(0xc0ffc0ffcff0c0ff);
#  else
    pVCpu->iem.s.cbOpcode           = 127;
#  endif
}

#endif /* VBOX_STRICT*/


#ifdef DBGFTRACE_ENABLED
DECLINLINE(void) iemInitDecoderTraceTargetPc(PVMCPUCC pVCpu, uint32_t fExec)
{
    switch (fExec & (IEM_F_MODE_ARM_32BIT | IEM_F_MODE_ARM_T32))
    {
        case 0:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "A64/%u %08llx",
                              IEM_F_MODE_ARM_GET_EL(fExec), pVCpu->cpum.GstCtx.Pc.u64);
            break;
        case IEM_F_MODE_ARM_32BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "A32/%u %08llx",
                              IEM_F_MODE_ARM_GET_EL(fExec), pVCpu->cpum.GstCtx.Pc.u64); /** @todo not sure if we're using PC or R15 here... */
            break;
        case IEM_F_MODE_ARM_32BIT | IEM_F_MODE_ARM_T32:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "T32/%u %08llx",
                              IEM_F_MODE_ARM_GET_EL(fExec), pVCpu->cpum.GstCtx.Pc.u64);
            break;
        case IEM_F_MODE_ARM_T32:
            AssertFailedBreak();
    }
}
#endif /* DBGFTRACE_ENABLED */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_armv8_IEMInlineExec_armv8_h */

