/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - X86 target, Inline Exec/Decoder routines.
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


#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineExec_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineExec_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


#ifdef IEM_OPCODE_GET_FIRST_U8
DECL_FORCE_INLINE(VBOXSTRICTRC) iemExecDecodeAndInterpretTargetInstruction(PVMCPUCC pVCpu)
{
    uint8_t b; IEM_OPCODE_GET_FIRST_U8(&b);
    return FNIEMOP_CALL(g_apfnIemInterpretOnlyOneByteMap[b]);
}
#endif


DECL_FORCE_INLINE(uint64_t) iemRegGetPC(PVMCPUCC pVCpu)
{
    return pVCpu->cpum.GstCtx.rip;
}


DECL_FORCE_INLINE(bool) iemExecLoopTargetCheckMaskedCpuFFs(PVMCPUCC pVCpu, uint64_t fCpuForceFlags)
{
    /* No FFs (irrelevant ones have already been masked out): */
    if (!fCpuForceFlags)
        return true;

    /* We can continue loop if only APIC or/and PIC FFs are pending and
       interrupts are masked (IF=0): */
    return !(fCpuForceFlags & ~(VMCPU_FF_INTERRUPT_APIC | VMCPU_FF_INTERRUPT_PIC))
        && !pVCpu->cpum.GstCtx.rflags.Bits.u1IF;
}

#ifdef VBOX_STRICT

DECLINLINE(void) iemInitDecoderStrictTarget(PVMCPUCC pVCpu)
{
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.cs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.ss));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.es));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.ds));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.fs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.gs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.ldtr));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.tr));
    RT_NOREF(pVCpu);
}


DECLINLINE(void) iemInitExecTailStrictTarget(PVMCPUCC pVCpu)
{
    /*
     * Assert hidden register sanity (also done in iemInitDecoder and iemReInitDecoder).
     */
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.cs));
    Assert(CPUMSELREG_ARE_HIDDEN_PARTS_VALID(pVCpu, &pVCpu->cpum.GstCtx.ss));
    RT_NOREF(pVCpu);
}


DECLINLINE(void) iemInitExecTargetStrict(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    iemInitDecoderStrictTarget(pVCpu);

    pVCpu->iem.s.enmDefAddrMode     = (IEMMODE)0xfe;
    pVCpu->iem.s.enmEffAddrMode     = (IEMMODE)0xfe;
    pVCpu->iem.s.enmDefOpSize       = (IEMMODE)0xfe;
    pVCpu->iem.s.enmEffOpSize       = (IEMMODE)0xfe;
    pVCpu->iem.s.fPrefixes          = 0xfeedbeef;
    pVCpu->iem.s.uRexReg            = 127;
    pVCpu->iem.s.uRexB              = 127;
    pVCpu->iem.s.offModRm           = 127;
    pVCpu->iem.s.uRexIndex          = 127;
    pVCpu->iem.s.iEffSeg            = 127;
    pVCpu->iem.s.idxPrefix          = 127;
    pVCpu->iem.s.uVex3rdReg         = 127;
    pVCpu->iem.s.uVexLength         = 127;
    pVCpu->iem.s.fEvexStuff         = 127;
    pVCpu->iem.s.uFpuOpcode         = UINT16_MAX;
#  ifdef IEM_WITH_CODE_TLB
    pVCpu->iem.s.offInstrNextByte   = UINT16_MAX;
    pVCpu->iem.s.pbInstrBuf         = NULL;
    pVCpu->iem.s.cbInstrBuf         = UINT16_MAX;
    pVCpu->iem.s.cbInstrBufTotal    = UINT16_MAX;
    pVCpu->iem.s.offCurInstrStart   = INT16_MAX;
    pVCpu->iem.s.uInstrBufPc        = UINT64_C(0xc0ffc0ffcff0c0ff);
#   ifdef IEM_WITH_CODE_TLB_AND_OPCODE_BUF
    pVCpu->iem.s.offOpcode          = 127;
#   endif
#  else
    pVCpu->iem.s.offOpcode          = 127;
    pVCpu->iem.s.cbOpcode           = 127;
#  endif
}

#endif /* VBOX_STRICT */


#ifdef DBGFTRACE_ENABLED
DECLINLINE(void) iemInitDecoderTraceTargetPc(PVMCPUCC pVCpu, uint32_t fExec)
{
    switch (fExec & IEM_F_MODE_X86_CPUMODE_MASK)
    {
        case IEMMODE_64BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I64/%u %08llx", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.rip);
            break;
        case IEMMODE_32BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I32/%u %04x:%08x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
        case IEMMODE_16BIT:
            RTTraceBufAddMsgF(pVCpu->CTX_SUFF(pVM)->CTX_SUFF(hTraceBuf), "I16/%u %04x:%04x", IEM_GET_CPL(pVCpu), pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.eip);
            break;
        case 3:
            AssertFailedBreak();
    }
}
#endif /* DBGFTRACE_ENABLED */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMInlineExec_x86_h */

