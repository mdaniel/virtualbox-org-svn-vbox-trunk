/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager, dummy IEM stub functions.
 *
 * This is for use during porting to a new host as well as when IEM isn't
 * required for some reason.
 */

/*
 * Copyright (C) 2023-2024 Oracle and/or its affiliates.
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
#define LOG_GROUP LOG_GROUP_IEM
#include <VBox/vmm/iem.h>

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

VMM_INT_DECL(VBOXSTRICTRC)
IEMExecForExits(PVMCPUCC pVCpu, uint32_t fWillExit, uint32_t cMinInstructions, uint32_t cMaxInstructions,
                uint32_t cMaxInstructionsWithoutExits, PIEMEXECFOREXITSTATS pStats)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, fWillExit, cMinInstructions, cMaxInstructions, cMaxInstructionsWithoutExits);
    RT_ZERO(*pStats);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecLots(PVMCPUCC pVCpu, uint32_t cMaxInstructions, uint32_t cPollRate, uint32_t *pcInstructions)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, cMaxInstructions, cPollRate, pcInstructions);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOne(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneWithPrefetchedByPC(PVMCPUCC pVCpu, uint64_t OpcodeBytesPC,
                                                        const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, OpcodeBytesPC, pvOpcodeBytes, cbOpcodeBytes);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneBypass(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMExecOneBypassWithPrefetchedByPC(PVMCPUCC pVCpu, uint64_t OpcodeBytesPC,
                                                              const void *pvOpcodeBytes, size_t cbOpcodeBytes)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, OpcodeBytesPC, pvOpcodeBytes, cbOpcodeBytes);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(VBOXSTRICTRC) IEMInjectTrpmEvent(PVMCPUCC pVCpu)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(void) IEMTlbInvalidateAll(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


VMM_INT_DECL(void) IEMTlbInvalidateAllGlobal(PVMCPUCC pVCpu)
{
    RT_NOREF(pVCpu);
}


VMM_INT_DECL(void) IEMTlbInvalidateAllPhysicalAllCpus(PVMCC pVM, VMCPUID idCpuCaller, IEMTLBPHYSFLUSHREASON enmReason)
{
    RT_NOREF(pVM, idCpuCaller, enmReason);
}


VMMR3_INT_DECL(VBOXSTRICTRC) IEMR3ProcessForceFlag(PVM pVM, PVMCPUCC pVCpu, VBOXSTRICTRC rcStrict)
{
    AssertReleaseFailed();
    RT_NOREF(pVM, pVCpu, rcStrict);
    return VERR_NOT_IMPLEMENTED;
}

