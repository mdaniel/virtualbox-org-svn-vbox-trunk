/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - Opcode Helpers.
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

#ifndef VMM_INCLUDED_SRC_include_IEMOpHlp_h
#define VMM_INCLUDED_SRC_include_IEMOpHlp_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/** @name  Common opcode decoders.
 * @{
 */

/**
 * Complains about a stub.
 *
 * Providing two versions of this macro, one for daily use and one for use when
 * working on IEM.
 */
#define IEMOP_BITCH_ABOUT_STUB() Log(("Stub: %s (line %d)\n", __FUNCTION__, __LINE__));

/** Stubs an opcode. */
#define FNIEMOP_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        RT_NOREF_PV(pVCpu); \
        IEMOP_BITCH_ABOUT_STUB(); \
        return VERR_IEM_INSTR_NOT_IMPLEMENTED; \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode. */
#define FNIEMOP_STUB_1(a_Name, a_Type0, a_Name0) \
    FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    { \
        RT_NOREF_PV(pVCpu); \
        RT_NOREF_PV(a_Name0); \
        IEMOP_BITCH_ABOUT_STUB(); \
        return VERR_IEM_INSTR_NOT_IMPLEMENTED; \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode which currently should raise \#UD. */
#define FNIEMOP_UD_STUB(a_Name) \
    FNIEMOP_DEF(a_Name) \
    { \
        Log(("Unsupported instruction %Rfn\n", __FUNCTION__)); \
        IEMOP_RAISE_INVALID_OPCODE_RET(); \
    } \
    typedef int ignore_semicolon

/** Stubs an opcode which currently should raise \#UD. */
#define FNIEMOP_UD_STUB_1(a_Name, a_Type0, a_Name0) \
    FNIEMOP_DEF_1(a_Name, a_Type0, a_Name0) \
    { \
        RT_NOREF_PV(pVCpu); \
        RT_NOREF_PV(a_Name0); \
        Log(("Unsupported instruction %Rfn\n", __FUNCTION__)); \
        IEMOP_RAISE_INVALID_OPCODE_RET(); \
    } \
    typedef int ignore_semicolon

/** @} */


/** @name   Opcode Debug Helpers.
 * @{
 */
#ifdef VBOX_WITH_STATISTICS
# ifdef IN_RING3
#  define IEMOP_INC_STATS(a_Stats) do { pVCpu->iem.s.StatsR3.a_Stats += 1; } while (0)
# else
#  define IEMOP_INC_STATS(a_Stats) do { pVCpu->iem.s.StatsRZ.a_Stats += 1; } while (0)
# endif
#else
# define IEMOP_INC_STATS(a_Stats) do { } while (0)
#endif

/** @} */


/** @name   Opcode Helpers.
 * @{
 */

#ifdef IN_RING3
# define IEMOP_HLP_MIN_CPU(a_uMinCpu, a_fOnlyIf) \
    do { \
        if (IEM_GET_TARGET_CPU(pVCpu) >= (a_uMinCpu) || !(a_fOnlyIf)) { } \
        else \
        { \
            (void)DBGFSTOP(pVCpu->CTX_SUFF(pVM)); \
            IEMOP_RAISE_INVALID_OPCODE_RET(); \
        } \
    } while (0)
#else
# define IEMOP_HLP_MIN_CPU(a_uMinCpu, a_fOnlyIf) \
    do { \
        if (IEM_GET_TARGET_CPU(pVCpu) >= (a_uMinCpu) || !(a_fOnlyIf)) { } \
        else IEMOP_RAISE_INVALID_OPCODE_RET(); \
    } while (0)
#endif


/**
 * Check for a CPUMFEATURES member to be true, raise \#UD if clear.
 */
#define IEMOP_HLP_RAISE_UD_IF_MISSING_GUEST_FEATURE(pVCpu, a_fFeature) \
    do \
    { \
        if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->a_fFeature) \
        { /* likely */ } \
        else \
            IEMOP_RAISE_INVALID_OPCODE_RET(); \
    } while (0)

/** @}  */

/*
 * Include the target specific header.
 */
#ifdef VBOX_VMM_TARGET_X86
# include "VMMAll/target-x86/IEMOpHlp-x86.h"
#endif

#endif /* !VMM_INCLUDED_SRC_include_IEMOpHlp_h */
