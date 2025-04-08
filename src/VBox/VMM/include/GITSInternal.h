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

#include <iprt/cdefs.h>
#include <VBox/types.h>
#include <VBox/gic-its.h>
#include <VBox/vmm/pdmthread.h>
#include <VBox/vmm/stam.h>

/** @defgroup grp_gits_int       Internal
 * @ingroup grp_gits
 * @internal
 * @{
 */

/**
 * GITS error diagnostics.
 * Sorted alphabetically so it's easier to add and locate items, no other reason.
 *
 * @note Members of this enum are used as array indices, so no gaps in enum
 *       values are not allowed. Update g_apszItsDiagDesc when you modify
 *       fields in this enum.
 */
typedef enum GITSDIAG
{
    /* No error, this must be zero! */
    kGitsDiag_None = 0,

    /* Command queue errors. */
    kGitsDiag_CmdQueue_PhysAddr_Invalid,

    /* Member for determining array index limit. */
    kGitsDiag_End,

    /* Usual 32-bit hack. */
    kGitsDiag_32Bit_Hack = 0x7fffffff
} GITSDIAG;
AssertCompileSize(GITSDIAG, 4);

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
 * GITS Collection Table Entry (CTE).
 */
typedef struct GITSCTE
{
    /** Whether this entry is valid. */
    bool        fValid;
    /** Alignment. */
    bool        afPadding;
    /** Target CPU ID (size based on GICR_TYPER.Processor_Number). */
    uint16_t    idTargetCpu;
} GITSCTE;
/** Pointer to a GITS Collection Table Entry (CTE). */
typedef GITSCTE *PGITSCTE;
/** Pointer to a const GITS Collection Table Entry (CTE). */
typedef GITSCTE const *PCGITSCTE;
AssertCompileSize(GITSCTE, 4);

/**
 * The GIC Interrupt Translation Service device state.
 */
typedef struct GITSDEV
{
    /** @name Control registers.
     * @{ */
    /** The ITS control register (GITS_CTLR). */
    uint32_t                uCtrlReg;
    /** Implmentation-specific error diagnostic. */
    GITSDIAG                enmDiag;
    /** The ITS type register (GITS_TYPER). */
    RTUINT64U               uTypeReg;
    /** The ITS table descriptor registers (GITS_BASER<n>). */
    RTUINT64U               aItsTableRegs[8];
    /** The ITS command queue base registers (GITS_CBASER). */
    RTUINT64U               uCmdBaseReg;
    /** The ITS command read register (GITS_CREADR). */
    uint32_t                uCmdReadReg;
    /** The ITS command write register (GITS_CWRITER). */
    uint32_t                uCmdWriteReg;
    /** @} */

    /** @name Interrupt translation space.
     * @{ */
    /** @} */

    /** @name Command queue.
     * @{ */
    /** The command-queue thread. */
    R3PTRTYPE(PPDMTHREAD)   pCmdQueueThread;
    /** The event semaphore the command-queue thread waits on. */
    SUPSEMEVENT             hEvtCmdQueue;
    /** Errors while processing command-queue. */
    uint32_t                uCmdQueueError;
    /** Padding. */
    uint32_t                uPadding0;
    /** @} */

    /** @name Tables.
     * @{
     */
    /** The collection table. */
    GITSCTE                 aCtes[2048];
    /** @} */

    /** @name Configurables.
     * @{ */
    /** The ITS architecture (GITS_PIDR2.ArchRev). */
    uint8_t                 uArchRev;
    /** Padding. */
    uint8_t                 afPadding0[7];
    /** @} */

    /** @name Statistics.
     * @{ */
#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER             StatCmdMapc;
    STAMCOUNTER             StatCmdSync;
    STAMCOUNTER             StatCmdInvall;
#endif
    /** @} */
} GITSDEV;
/** Pointer to a GITS device. */
typedef GITSDEV *PGITSDEV;
/** Pointer to a const GITS device. */
typedef GITSDEV const *PCGITSDEV;
AssertCompileSizeAlignment(GITSDEV, 8);
AssertCompileMemberAlignment(GITSDEV, aItsTableRegs, 8);
AssertCompileMemberAlignment(GITSDEV, uCmdReadReg, 4);
AssertCompileMemberAlignment(GITSDEV, uCmdWriteReg, 4);
AssertCompileMemberAlignment(GITSDEV, hEvtCmdQueue, 8);
AssertCompileMemberAlignment(GITSDEV, aCtes, 8);
AssertCompileMemberAlignment(GITSDEV, uArchRev, 8);

DECL_HIDDEN_CALLBACK(void)         gitsInit(PGITSDEV pGitsDev);
DECL_HIDDEN_CALLBACK(int)          gitsSendMsi(PVMCC pVM, PCIBDF uBusDevFn, PCMSIMSG pMsi, uint32_t uEventId, uint32_t uTagSrc);
DECL_HIDDEN_CALLBACK(uint64_t)     gitsMmioReadCtrl(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb);
DECL_HIDDEN_CALLBACK(uint64_t)     gitsMmioReadTranslate(PCGITSDEV pGitsDev, uint16_t offReg, unsigned cb);
DECL_HIDDEN_CALLBACK(void)         gitsMmioWriteCtrl(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb);
DECL_HIDDEN_CALLBACK(void)         gitsMmioWriteTranslate(PGITSDEV pGitsDev, uint16_t offReg, uint64_t uValue, unsigned cb);

#ifdef IN_RING3
DECL_HIDDEN_CALLBACK(void)         gitsR3DbgInfo(PCGITSDEV pGitsDev, PCDBGFINFOHLP pHlp, const char *pszArgs);
DECL_HIDDEN_CALLBACK(int)          gitsR3CmdQueueProcess(PPDMDEVINS pDevIns, PGITSDEV pGitsDev, void *pvBuf, uint32_t cbBuf);
#endif

#ifdef LOG_ENABLED
DECL_HIDDEN_CALLBACK(const char *) gitsGetCtrlRegDescription(uint16_t offReg);
DECL_HIDDEN_CALLBACK(const char *) gitsGetTranslationRegDescription(uint16_t offReg);
#endif

/** @} */

#endif /* !VMM_INCLUDED_SRC_include_GITSInternal_h */

