/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Inline Exec/Decoder routines.
 */

/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_include_IEMInlineExec_h
#define VMM_INCLUDED_SRC_include_IEMInlineExec_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/err.h>


/* Documentation and forward declarations for inline functions required for every target: */

RT_NO_WARN_UNUSED_INLINE_PROTOTYPE_BEGIN

/**
 * Calculates the the IEM_F_XXX flags.
 *
 * @returns IEM_F_XXX combination match the current CPU state.
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECL_FORCE_INLINE(uint32_t) iemCalcExecFlags(PVMCPUCC pVCpu) RT_NOEXCEPT;

#if defined(VBOX_STRICT) || defined(DOXYGEN_RUNNING)
/**
 * Invalidates the decoder state and asserts various stuff - strict builds only.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECLINLINE(void)            iemInitExecTargetStrict(PVMCPUCC pVCpu) RT_NOEXCEPT;
#endif

RT_NO_WARN_UNUSED_INLINE_PROTOTYPE_END


//#ifdef VBOX_VMM_TARGET_X86
//# include "VMMAll/target-x86/IEMInlineExec-x86.h"
//#elif defined(VBOX_VMM_TARGET_ARMV8)
//# include "VMMAll/target-armv8/IEMInlineExec-armv8.h"
//#endif


# if defined(VBOX_INCLUDED_vmm_dbgf_h) || defined(DOXYGEN_RUNNING) /* dbgf.ro.cEnabledHwBreakpoints */

/**
 * Initializes the execution state.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 * @param   fExecOpts           Optional execution flags:
 *                                  - IEM_F_BYPASS_HANDLERS
 *                                  - IEM_F_X86_DISREGARD_LOCK
 *
 * @remarks Callers of this must call iemUninitExec() to undo potentially fatal
 *          side-effects in strict builds.
 */
DECLINLINE(void) iemInitExec(PVMCPUCC pVCpu, uint32_t fExecOpts) RT_NOEXCEPT
{
    IEM_CTX_ASSERT(pVCpu, IEM_CPUMCTX_EXTRN_EXEC_DECODED_NO_MEM_MASK);
    Assert(!VMCPU_FF_IS_SET(pVCpu, VMCPU_FF_IEM));

    pVCpu->iem.s.rcPassUp           = VINF_SUCCESS;
    pVCpu->iem.s.fExec              = iemCalcExecFlags(pVCpu) | fExecOpts;
    pVCpu->iem.s.cActiveMappings    = 0;
    pVCpu->iem.s.iNextMapping       = 0;

#  ifdef VBOX_STRICT
    iemInitExecTargetStrict(pVCpu);
#  endif
}


#  if defined(VBOX_WITH_NESTED_HWVIRT_SVM) || defined(VBOX_WITH_NESTED_HWVIRT_VMX)
/**
 * Performs a minimal reinitialization of the execution state.
 *
 * This is intended to be used by VM-exits, SMM, LOADALL and other similar
 * 'world-switch' types operations on the CPU. Currently only nested
 * hardware-virtualization uses it.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the calling EMT.
 * @param   cbInstr             The instruction length (for flushing).
 */
DECLINLINE(void) iemReInitExec(PVMCPUCC pVCpu, uint8_t cbInstr) RT_NOEXCEPT
{
    pVCpu->iem.s.fExec = iemCalcExecFlags(pVCpu) | (pVCpu->iem.s.fExec & IEM_F_USER_OPTS);
#   ifdef VBOX_VMM_TARGET_X86
    iemOpcodeFlushHeavy(pVCpu, cbInstr);
#   elif !defined(IEM_WITH_CODE_TLB)
    pVCpu->iem.s.cbOpcode = cbInstr;
#   else
    pVCpu->iem.s.cbInstrBufTotal = 0;
    RT_NOREF(cbInstr);
#   endif
}
#  endif

# endif /* VBOX_INCLUDED_vmm_dbgf_h || DOXYGEN_RUNNING */

/**
 * Counterpart to #iemInitExec that undoes evil strict-build stuff.
 *
 * @param   pVCpu               The cross context virtual CPU structure of the
 *                              calling thread.
 */
DECLINLINE(void) iemUninitExec(PVMCPUCC pVCpu) RT_NOEXCEPT
{
    /* Note! do not touch fInPatchCode here! (see iemUninitExecAndFiddleStatusAndMaybeReenter) */
# ifdef VBOX_STRICT
#  ifdef IEM_WITH_CODE_TLB
    NOREF(pVCpu);
#  else
    pVCpu->iem.s.cbOpcode = 0;
#  endif
# else
    NOREF(pVCpu);
# endif
}


/**
 * Calls iemUninitExec, iemExecStatusCodeFiddling and iemRCRawMaybeReenter.
 *
 * Only calling iemRCRawMaybeReenter in raw-mode, obviously.
 *
 * @returns Fiddled strict vbox status code, ready to return to non-IEM caller.
 * @param   pVCpu       The cross context virtual CPU structure of the calling thread.
 * @param   rcStrict    The status code to fiddle.
 */
DECLINLINE(VBOXSTRICTRC) iemUninitExecAndFiddleStatusAndMaybeReenter(PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict) RT_NOEXCEPT
{
    iemUninitExec(pVCpu);
    return iemExecStatusCodeFiddling(pVCpu, rcStrict);
}

#endif /* !VMM_INCLUDED_SRC_include_IEMInlineExec_h */
