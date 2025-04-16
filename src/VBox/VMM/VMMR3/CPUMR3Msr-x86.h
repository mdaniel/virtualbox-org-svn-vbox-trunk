/* $Id$ */
/** @file
 * CPUM - x86 MSR macros.
 */

/*
 * Copyright (C) 2013-2024 Oracle and/or its affiliates.
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


#ifndef VMM_INCLUDED_SRC_VMMR3_CPUMR3Msr_x86_h
#define VMM_INCLUDED_SRC_VMMR3_CPUMR3Msr_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @name Short macros for the MSR range entries.
 *
 * These are rather cryptic, but this is to reduce the attack on the right
 * margin.
 *
 * @{ */
/** Alias one MSR onto another (a_uTarget). */
#define MAL(a_uMsr, a_szName, a_uTarget) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_MsrAlias, kCpumMsrWrFn_MsrAlias, 0, a_uTarget, 0, 0, a_szName)
/** Functions handles everything. */
#define MFN(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, 0, 0, 0, a_szName)
/** Functions handles everything, with GP mask. */
#define MFG(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, 0, 0, a_fWrGpMask, a_szName)
/** Function handlers, read-only. */
#define MFO(a_uMsr, a_szName, a_enmRdFnSuff) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_ReadOnly, 0, 0, 0, UINT64_MAX, a_szName)
/** Function handlers, ignore all writes. */
#define MFI(a_uMsr, a_szName, a_enmRdFnSuff) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_IgnoreWrite, 0, 0, UINT64_MAX, 0, a_szName)
/** Function handlers, with value. */
#define MFV(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_uValue) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, a_uValue, 0, 0, a_szName)
/** Function handlers, with write ignore mask. */
#define MFW(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_fWrIgnMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, 0, a_fWrIgnMask, 0, a_szName)
/** Function handlers, extended version. */
#define MFX(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_uValue, a_fWrIgnMask, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, a_uValue, a_fWrIgnMask, a_fWrGpMask, a_szName)
/** Function handlers, with CPUMCPU storage variable. */
#define MFS(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_CpumCpuMember) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, \
         RT_OFFSETOF(CPUMCPU, a_CpumCpuMember), 0, 0, 0, a_szName)
/** Function handlers, with CPUMCPU storage variable, ignore mask and GP mask. */
#define MFZ(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_CpumCpuMember, a_fWrIgnMask, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, \
         RT_OFFSETOF(CPUMCPU, a_CpumCpuMember), 0, a_fWrIgnMask, a_fWrGpMask, a_szName)
/** Read-only fixed value. */
#define MVO(a_uMsr, a_szName, a_uValue) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_ReadOnly, 0, a_uValue, 0, UINT64_MAX, a_szName)
/** Read-only fixed value, ignores all writes. */
#define MVI(a_uMsr, a_szName, a_uValue) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_IgnoreWrite, 0, a_uValue, UINT64_MAX, 0, a_szName)
/** Read fixed value, ignore writes outside GP mask. */
#define MVG(a_uMsr, a_szName, a_uValue, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_IgnoreWrite, 0, a_uValue, 0, a_fWrGpMask, a_szName)
/** Read fixed value, extended version with both GP and ignore masks. */
#define MVX(a_uMsr, a_szName, a_uValue, a_fWrIgnMask, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_IgnoreWrite, 0, a_uValue, a_fWrIgnMask, a_fWrGpMask, a_szName)
/** The short form, no CPUM backing. */
#define MSN(a_uMsr, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask) \
    RINT(a_uMsr, a_uMsr, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, \
         a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName)

/** Range: Functions handles everything. */
#define RFN(a_uFirst, a_uLast, a_szName, a_enmRdFnSuff, a_enmWrFnSuff) \
    RINT(a_uFirst, a_uLast, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, 0, 0, 0, a_szName)
/** Range: Read fixed value, read-only. */
#define RVO(a_uFirst, a_uLast, a_szName, a_uValue) \
    RINT(a_uFirst, a_uLast, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_ReadOnly, 0, a_uValue, 0, UINT64_MAX, a_szName)
/** Range: Read fixed value, ignore writes. */
#define RVI(a_uFirst, a_uLast, a_szName, a_uValue) \
    RINT(a_uFirst, a_uLast, kCpumMsrRdFn_FixedValue, kCpumMsrWrFn_IgnoreWrite, 0, a_uValue, UINT64_MAX, 0, a_szName)
/** Range: The short form, no CPUM backing. */
#define RSN(a_uFirst, a_uLast, a_szName, a_enmRdFnSuff, a_enmWrFnSuff, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask) \
    RINT(a_uFirst, a_uLast, kCpumMsrRdFn_##a_enmRdFnSuff, kCpumMsrWrFn_##a_enmWrFnSuff, 0, \
         a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName)

/** Internal form used by the macros. */
#ifdef VBOX_WITH_STATISTICS
# define RINT(a_uFirst, a_uLast, a_enmRdFn, a_enmWrFn, a_offCpumCpu, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName) \
    { a_uFirst, a_uLast, a_enmRdFn, a_enmWrFn, a_offCpumCpu, 0, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName, \
      { 0 }, { 0 }, { 0 }, { 0 } }
#else
# define RINT(a_uFirst, a_uLast, a_enmRdFn, a_enmWrFn, a_offCpumCpu, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName) \
    { a_uFirst, a_uLast, a_enmRdFn, a_enmWrFn, a_offCpumCpu, 0, a_uInitOrReadValue, a_fWrIgnMask, a_fWrGpMask, a_szName }
#endif
/** @} */

#endif /* !VMM_INCLUDED_SRC_VMMR3_CPUMR3Msr_x86_h */
