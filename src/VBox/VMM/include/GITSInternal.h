/* $Id$ */
/** @file
 * GITS - Generic Interrupt Controller Interrupt Translation Service - Internal.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef VMM_INCLUDED_SRC_include_GITSInternal_h
#define VMM_INCLUDED_SRC_include_GITSInternal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/gic-its.h>

/** @defgroup grp_gits_int       Internal
 * @ingroup grp_gits
 * @internal
 * @{
 */

#if 0
/**
 * Interrupt Translation Table Base.
 */
typedef struct GITSITSBASE
{
    /** The physical address of the table. */
    RTGCPHYS                GCPhys;
    /** Size of every allocated page in bytes. */
    uint32_t                cbPageSize;
    /** Number of pages allocated. */
    uint8_t                 cPages;
    /** Size of each entry in bytes. */
    uint8_t                 cbEntry;
    /** Whether this is a two-level or flat table. */
    bool                    fTwoLevel;
    /** Whether software has memory allocated for the table. */
    bool                    fValid;
    /** Memory shareability attributes. */
    GITSATTRSHARE           AttrShare;
    /** Memory cacheability attributes (Inner). */
    GITSATTRMEM             AttrMemInner;
    /** Memory cacheability attributes (Outer). */
    GITSATTRMEM             AttrMemOuter;
} GITSITSBASE;
AssertCompileSizeAlignment(GITSITSBASE, 8);
AssertCompileMemberAlignment(GITSITSBASE, AttrShare, 8);
#endif


/**
 * The GIC Interrupt Translation Service device state.
 */
typedef struct GITSDEV
{
    /** @name Control registers.
     * @{ */
    /** Whether the ITS is enabled. */
    bool                    fEnabled;
    /** Whether unmapped MSI reporting interrupt is enabled. */
    bool                    fUnmappedMsiReporting;
    /** Whether ITS is quiescent and can be powered down. */
    bool                    fQuiescent;
    /** Padding. */
    bool                    fPadding0;
    /** The ITS table descriptor registers. */
    RTUINT64U               aItsTableRegs[8];
    /** The ITS command queue base registers. */
    RTUINT64U               uCmdBaseReg;
    /** The ITS command write register. */
    uint32_t                uCmdWriteReg;
    /** The ITS command read register. */
    uint32_t                uCmdReadReg;
    /** @} */

    /** @name Interrupt translation space.
     * @{ */
    /** @} */

    /** @name Configurables.
     * @{ */
    /** The ITS architecture (GITS_PIDR2.ArchRev). */
    uint8_t                 uArchRev;
    /** Padding. */
    uint8_t                 afPadding0[7];
    /** @} */
} GITSDEV;
/** Pointer to a GITS device. */
typedef GITSDEV *PGITSDEV;
/** Pointer to a const GITS device. */
typedef GITSDEV const *PCGITSDEV;
AssertCompileSizeAlignment(GITSDEV, 8);
AssertCompileMemberAlignment(GITSDEV, uArchRev, 8);

DECL_HIDDEN_CALLBACK(void)         gitsInit(PGITSDEV pGitsDev);
DECL_HIDDEN_CALLBACK(int)          gitsSendMsi(PVMCC pVM, PCIBDF uBusDevFn, PCMSIMSG pMsi, uint32_t uEventId, uint32_t uTagSrc);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, uint32_t *puValue);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteCtrl(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue);
DECL_HIDDEN_CALLBACK(VBOXSTRICTRC) gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint32_t uValue);

#ifdef IN_RING3
DECL_HIDDEN_CALLBACK(void)         gitsR3DbgInfo(PCGITSDEV pGitsDev, PCDBGFINFOHLP pHlp, const char *pszArgs);
#endif

#ifdef LOG_ENABLED
DECL_HIDDEN_CALLBACK(const char *) gitsGetCtrlRegDescription(uint16_t offReg);
DECL_HIDDEN_CALLBACK(const char *) gitsGetTranslationRegDescription(uint16_t offReg);
#endif

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_GITSInternal_h */

