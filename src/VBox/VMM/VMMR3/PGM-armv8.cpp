/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, ARMv8 variant. (Mixing stuff here, not good?)
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


/** @page pg_pgm_armv8 PGM - The Page Manager and Monitor (ARMv8 variant)
 *
 * For now this is just a stub for bringing up the ARMv8 hypervisor. We'll see how
 * much we really need here later on and whether it makes sense to merge this with the original PGM.cpp
 * (avoiding \#ifdef hell for with this as I'm not confident enough to fiddle around with PGM too much at this point).
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#define VBOX_WITHOUT_PAGING_BIT_FIELDS /* 64-bit bitfields are just asking for trouble. See @bugref{9841} and others. */
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/cpum-armv8.h>
#include <VBox/vmm/iom.h>
#include <VBox/sup.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/hm.h>
#include "PGMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/uvm.h>
#include "PGMInline.h"

#include <VBox/dbg.h>
#include <VBox/param.h>
#include <VBox/err.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/thread.h>


#if 0 /* now in taken from PGM.cpp where it came from */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef VBOX_STRICT
static FNVMATSTATE        pgmR3ResetNoMorePhysWritesFlag;
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifndef VBOX_WITH_PGM_NEM_MODE
# error "This requires VBOX_WITH_PGM_NEM_MODE to be set at all times!"
#endif

/**
 * Interface that NEM uses to switch PGM into simplified memory managment mode.
 *
 * This call occurs before PGMR3Init.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) PGMR3EnableNemMode(PVM pVM)
{
    AssertFatal(!PDMCritSectIsInitialized(&pVM->pgm.s.CritSectX));
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    pVM->pgm.s.fNemMode = true;
#endif
}


/**
 * Checks whether the simplificed memory management mode for NEM is enabled.
 *
 * @returns true if enabled, false if not.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(bool)    PGMR3IsNemModeEnabled(PVM pVM)
{
    RT_NOREF(pVM);
    return PGM_IS_IN_NEM_MODE(pVM);
}


/**
 * Initiates the paging of VM.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3DECL(int) PGMR3Init(PVM pVM)
{
    LogFlow(("PGMR3Init:\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompile(sizeof(pVM->pgm.s) <= sizeof(pVM->pgm.padding));
    AssertCompile(sizeof(pVM->apCpusR3[0]->pgm.s) <= sizeof(pVM->apCpusR3[0]->pgm.padding));
    AssertCompileMemberAlignment(PGM, CritSectX, sizeof(uintptr_t));

    bool const fDriverless = SUPR3IsDriverless();

    /*
     * Init the structure.
     */
    /*pVM->pgm.s.fRestoreRomPagesAtReset = false;*/

    /* We always use the simplified memory mode on arm. */
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    pVM->pgm.s.fNemMode = true;
#endif

    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
    {
        pVM->pgm.s.aHandyPages[i].HCPhysGCPhys  = NIL_GMMPAGEDESC_PHYS;
        pVM->pgm.s.aHandyPages[i].fZeroed       = false;
        pVM->pgm.s.aHandyPages[i].idPage        = NIL_GMM_PAGEID;
        pVM->pgm.s.aHandyPages[i].idSharedPage  = NIL_GMM_PAGEID;
    }

    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.aLargeHandyPage); i++)
    {
        pVM->pgm.s.aLargeHandyPage[i].HCPhysGCPhys  = NIL_GMMPAGEDESC_PHYS;
        pVM->pgm.s.aLargeHandyPage[i].fZeroed       = false;
        pVM->pgm.s.aLargeHandyPage[i].idPage        = NIL_GMM_PAGEID;
        pVM->pgm.s.aLargeHandyPage[i].idSharedPage  = NIL_GMM_PAGEID;
    }

    AssertReleaseReturn(pVM->pgm.s.cPhysHandlerTypes == 0, VERR_WRONG_ORDER);
    for (size_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.aPhysHandlerTypes); i++)
    {
#if defined(VBOX_WITH_R0_MODULES) && !defined(VBOX_WITH_MINIMAL_R0)
        if (fDriverless)
#endif
            pVM->pgm.s.aPhysHandlerTypes[i].hType  = i | (RTRandU64() & ~(uint64_t)PGMPHYSHANDLERTYPE_IDX_MASK);
        pVM->pgm.s.aPhysHandlerTypes[i].enmKind    = PGMPHYSHANDLERKIND_INVALID;
        pVM->pgm.s.aPhysHandlerTypes[i].pfnHandler = pgmR3HandlerPhysicalHandlerInvalid;
    }

#if 0
    /* Init the per-CPU part. */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];
        PPGMCPU pPGM = &pVCpu->pgm.s;
    }
#endif

    /*
     * Read the configuration.
     */
    PCFGMNODE const pCfgPGM = CFGMR3GetChild(CFGMR3GetRoot(pVM), "/PGM");

    /** @todo RamPreAlloc doesn't work for NEM-mode.   */
    int rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "RamPreAlloc", &pVM->pgm.s.fRamPreAlloc,
#ifdef VBOX_WITH_PREALLOC_RAM_BY_DEFAULT
                            true
#else
                            false
#endif
                           );
    AssertLogRelRCReturn(rc, rc);

#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    rc = CFGMR3QueryU32Def(pCfgPGM, "MaxRing3Chunks", &pVM->pgm.s.ChunkR3Map.cMax, UINT32_MAX);
    AssertLogRelRCReturn(rc, rc);
    for (uint32_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk = NIL_GMM_CHUNKID;
#endif

    /*
     * Get the configured RAM size - to estimate saved state size.
     */
    uint64_t    cbRam;
    rc = CFGMR3QueryU64(CFGMR3GetRoot(pVM), "RamSize", &cbRam);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        cbRam = 0;
    else if (RT_SUCCESS(rc))
    {
        if (cbRam < GUEST_PAGE_SIZE)
            cbRam = 0;
        cbRam = RT_ALIGN_64(cbRam, GUEST_PAGE_SIZE);
    }
    else
    {
        AssertMsgFailed(("Configuration error: Failed to query integer \"RamSize\", rc=%Rrc.\n", rc));
        return rc;
    }

    /** @cfgm{/PGM/ZeroRamPagesOnReset, boolean, true}
     * Whether to clear RAM pages on (hard) reset. */
    rc = CFGMR3QueryBoolDef(pCfgPGM, "ZeroRamPagesOnReset", &pVM->pgm.s.fZeroRamPagesOnReset, true);
    AssertLogRelRCReturn(rc, rc);

    /*
     * Register callbacks, string formatters and the saved state data unit.
     */
#ifdef VBOX_STRICT
    VMR3AtStateRegister(pVM->pUVM, pgmR3ResetNoMorePhysWritesFlag, NULL);
#endif
    PGMRegisterStringFormatTypes();

    rc = pgmR3InitSavedState(pVM, cbRam);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Initialize the PGM critical section and flush the phys TLBs
     */
    rc = PDMR3CritSectInit(pVM, &pVM->pgm.s.CritSectX, RT_SRC_POS, "PGM");
    AssertRCReturn(rc, rc);

#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    pgmR3PhysChunkInvalidateTLB(pVM, false /*fInRendezvous*/); /* includes pgmPhysInvalidatePageMapTLB call */
#endif

    /*
     * For the time being we sport a full set of handy pages in addition to the base
     * memory to simplify things.
     */
    rc = MMR3ReserveHandyPages(pVM, RT_ELEMENTS(pVM->pgm.s.aHandyPages)); /** @todo this should be changed to PGM_HANDY_PAGES_MIN but this needs proper testing... */
    AssertRCReturn(rc, rc);

    /*
     * Setup the zero page (HCPHysZeroPg is set by ring-0).
     */
    RT_ZERO(pVM->pgm.s.abZeroPg); /* paranoia */
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    if (fDriverless)
        pVM->pgm.s.HCPhysZeroPg = _4G - GUEST_PAGE_SIZE * 2 /* fake to avoid PGM_PAGE_INIT_ZERO assertion */;
    AssertRelease(pVM->pgm.s.HCPhysZeroPg != NIL_RTHCPHYS);
    AssertRelease(pVM->pgm.s.HCPhysZeroPg != 0);
#endif

    /*
     * Setup the invalid MMIO page (HCPhysMmioPg is set by ring-0).
     * (The invalid bits in HCPhysInvMmioPg are set later on init complete.)
     */
    ASMMemFill32(pVM->pgm.s.abMmioPg, sizeof(pVM->pgm.s.abMmioPg), 0xfeedface);
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    if (fDriverless)
        pVM->pgm.s.HCPhysMmioPg = _4G - GUEST_PAGE_SIZE * 3 /* fake to avoid PGM_PAGE_INIT_ZERO assertion */;
    AssertRelease(pVM->pgm.s.HCPhysMmioPg != NIL_RTHCPHYS);
    AssertRelease(pVM->pgm.s.HCPhysMmioPg != 0);
    pVM->pgm.s.HCPhysInvMmioPg = pVM->pgm.s.HCPhysMmioPg;
#endif

    /*
     * Initialize physical access handlers.
     */
    /** @cfgm{/PGM/MaxPhysicalAccessHandlers, uint32_t, 32, 65536, 6144}
     * Number of physical access handlers allowed (subject to rounding).  This is
     * managed as one time allocation during initializations.  The default is
     * lower for a driverless setup. */
    /** @todo can lower it for nested paging too, at least when there is no
     *        nested guest involved. */
    uint32_t cAccessHandlers = 0;
    rc = CFGMR3QueryU32Def(pCfgPGM, "MaxPhysicalAccessHandlers", &cAccessHandlers, !fDriverless ? 6144 : 640);
    AssertLogRelRCReturn(rc, rc);
    AssertLogRelMsgStmt(cAccessHandlers >= 32, ("cAccessHandlers=%#x, min 32\n", cAccessHandlers), cAccessHandlers = 32);
    AssertLogRelMsgStmt(cAccessHandlers <= _64K, ("cAccessHandlers=%#x, max 65536\n", cAccessHandlers), cAccessHandlers = _64K);
#if defined(VBOX_WITH_R0_MODULES) && !defined(VBOX_WITH_MINIMAL_R0)
    if (!fDriverless)
    {
        rc = VMMR3CallR0(pVM, VMMR0_DO_PGM_PHYS_HANDLER_INIT, cAccessHandlers, NULL);
        AssertRCReturn(rc, rc);
        AssertPtr(pVM->pgm.s.pPhysHandlerTree);
        AssertPtr(pVM->pgm.s.PhysHandlerAllocator.m_paNodes);
        AssertPtr(pVM->pgm.s.PhysHandlerAllocator.m_pbmAlloc);
    }
    else
#endif
    {
        uint32_t       cbTreeAndBitmap = 0;
        uint32_t const cbTotalAligned  = pgmHandlerPhysicalCalcTableSizes(&cAccessHandlers, &cbTreeAndBitmap);
        uint8_t       *pb = NULL;
        rc = SUPR3PageAlloc(cbTotalAligned >> HOST_PAGE_SHIFT, 0, (void **)&pb);
        AssertLogRelRCReturn(rc, rc);

        pVM->pgm.s.PhysHandlerAllocator.initSlabAllocator(cAccessHandlers, (PPGMPHYSHANDLER)&pb[cbTreeAndBitmap],
                                                          (uint64_t *)&pb[sizeof(PGMPHYSHANDLERTREE)]);
        pVM->pgm.s.pPhysHandlerTree = (PPGMPHYSHANDLERTREE)pb;
        pVM->pgm.s.pPhysHandlerTree->initWithAllocator(&pVM->pgm.s.PhysHandlerAllocator);
    }

    /*
     * Register the physical access handler protecting ROMs.
     */
    if (RT_SUCCESS(rc))
        /** @todo why isn't pgmPhysRomWriteHandler registered for ring-0?   */
        rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_WRITE, 0 /*fFlags*/, pgmPhysRomWriteHandler,
                                              "ROM write protection", &pVM->pgm.s.hRomPhysHandlerType);

    /*
     * Register the physical access handler doing dirty MMIO2 tracing.
     */
    if (RT_SUCCESS(rc))
        rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_WRITE, PGMPHYSHANDLER_F_KEEP_PGM_LOCK,
                                              pgmPhysMmio2WriteHandler, "MMIO2 dirty page tracing",
                                              &pVM->pgm.s.hMmio2DirtyPhysHandlerType);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    /* Almost no cleanup necessary, MM frees all memory. */
    PDMR3CritSectDelete(pVM, &pVM->pgm.s.CritSectX);

    return rc;
}


/**
 * Ring-3 init finalizing (not required here).
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3DECL(int) PGMR3InitFinalize(PVM pVM)
{
    RT_NOREF(pVM);
    int rc = VINF_SUCCESS;
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    if (pVM->pgm.s.fRamPreAlloc)
        rc = pgmR3PhysRamPreAllocate(pVM);
#endif

    //pgmLogState(pVM);
    LogRel(("PGM: PGMR3InitFinalize done: %Rrc\n", rc));
    return rc;
}


/**
 * Init phase completed callback.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   enmWhat             What has been completed.
 * @thread  EMT(0)
 */
VMMR3_INT_DECL(int) PGMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    switch (enmWhat)
    {
        case VMINITCOMPLETED_HM:
            AssertLogRelReturn(!pVM->pgm.s.fPciPassthrough, VERR_PGM_PCI_PASSTHRU_MISCONFIG);
            break;

        default:
            /* shut up gcc */
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this component.
 *
 * This function will be called at init and whenever the VMM need to relocate it
 * self inside the GC.
 *
 * @param   pVM     The cross context VM structure.
 * @param   offDelta    Relocation delta relative to old location.
 */
VMMR3DECL(void) PGMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("PGMR3Relocate: offDelta=%RGv\n", offDelta));
    RT_NOREF(pVM, offDelta);
}


/**
 * Resets a virtual CPU when unplugged.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pVCpu               The cross context virtual CPU structure.
 */
VMMR3DECL(void) PGMR3ResetCpu(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM, pVCpu);
}


/**
 * The VM is being reset.
 *
 * For the PGM component this means that any PD write monitors
 * needs to be removed.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) PGMR3Reset(PVM pVM)
{
    LogFlow(("PGMR3Reset:\n"));
    VM_ASSERT_EMT(pVM);

    PGM_LOCK_VOID(pVM);

#ifdef DEBUG
    DBGFR3_INFO_LOG_SAFE(pVM, "mappings", NULL);
    DBGFR3_INFO_LOG_SAFE(pVM, "handlers", "all nostat");
#endif

    //pgmLogState(pVM);
    PGM_UNLOCK(pVM);
}


/**
 * Memory setup after VM construction or reset.
 *
 * @param   pVM         The cross context VM structure.
 * @param   fAtReset    Indicates the context, after reset if @c true or after
 *                      construction if @c false.
 */
VMMR3_INT_DECL(void) PGMR3MemSetup(PVM pVM, bool fAtReset)
{
    if (fAtReset)
    {
        PGM_LOCK_VOID(pVM);

        int rc = pgmR3PhysRamZeroAll(pVM);
        AssertReleaseRC(rc);

        rc = pgmR3PhysRomReset(pVM);
        AssertReleaseRC(rc);

        PGM_UNLOCK(pVM);
    }
}


#ifdef VBOX_STRICT
/**
 * VM state change callback for clearing fNoMorePhysWrites after
 * a snapshot has been created.
 */
static DECLCALLBACK(void) pgmR3ResetNoMorePhysWritesFlag(PUVM pUVM, PCVMMR3VTABLE pVMM, VMSTATE enmState,
                                                         VMSTATE enmOldState, void *pvUser)
{
    if (   enmState == VMSTATE_RUNNING
        || enmState == VMSTATE_RESUMING)
        pUVM->pVM->pgm.s.fNoMorePhysWrites = false;
    RT_NOREF(pVMM, enmOldState, pvUser);
}
#endif

/**
 * Private API to reset fNoMorePhysWrites.
 */
VMMR3_INT_DECL(void) PGMR3ResetNoMorePhysWritesFlag(PVM pVM)
{
    pVM->pgm.s.fNoMorePhysWrites = false;
}

/**
 * Terminates the PGM.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 */
VMMR3DECL(int) PGMR3Term(PVM pVM)
{
    /* Must free shared pages here. */
    PGM_LOCK_VOID(pVM);
    pgmR3PhysRamTerm(pVM);
    pgmR3PhysRomTerm(pVM);
    PGM_UNLOCK(pVM);

    PGMDeregisterStringFormatTypes();
    return PDMR3CritSectDelete(pVM, &pVM->pgm.s.CritSectX);
}


/**
 * Perform an integrity check on the PGM component.
 *
 * @returns VINF_SUCCESS if everything is fine.
 * @returns VBox error status after asserting on integrity breach.
 * @param   pVM     The cross context VM structure.
 */
VMMR3DECL(int) PGMR3CheckIntegrity(PVM pVM)
{
    RT_NOREF(pVM);
    return VINF_SUCCESS;
}

#endif

VMMDECL(bool) PGMHasDirtyPages(PVM pVM)
{
#ifndef VBOX_WITH_ONLY_PGM_NEM_MODE
    return pVM->pgm.s.CTX_SUFF(pPool)->cDirtyPages != 0;
#else
    RT_NOREF(pVM);
    return false;
#endif
}


VMMDECL(bool) PGMIsLockOwner(PVMCC pVM)
{
    return PDMCritSectIsOwner(pVM, &pVM->pgm.s.CritSectX);
}


VMMDECL(int) PGMSetLargePageUsage(PVMCC pVM, bool fUseLargePages)
{
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);

    pVM->pgm.s.fUseLargePages = fUseLargePages;
    return VINF_SUCCESS;
}


#if defined(VBOX_STRICT) || defined(DOXYGEN_RUNNING)
int pgmLockDebug(PVMCC pVM, bool fVoid, RT_SRC_POS_DECL)
#else
int pgmLock(PVMCC pVM, bool fVoid)
#endif
{
#if defined(VBOX_STRICT)
    int rc = PDMCritSectEnterDebug(pVM, &pVM->pgm.s.CritSectX, VINF_SUCCESS, (uintptr_t)ASMReturnAddress(), RT_SRC_POS_ARGS);
#else
    int rc = PDMCritSectEnter(pVM, &pVM->pgm.s.CritSectX, VINF_SUCCESS);
#endif
    if (RT_SUCCESS(rc))
        return rc;
    if (fVoid)
        PDM_CRITSECT_RELEASE_ASSERT_RC(pVM, &pVM->pgm.s.CritSectX, rc);
    else
        AssertRC(rc);
    return rc;
}


void pgmUnlock(PVMCC pVM)
{
    uint32_t cDeprecatedPageLocks = pVM->pgm.s.cDeprecatedPageLocks;
    pVM->pgm.s.cDeprecatedPageLocks = 0;
    int rc = PDMCritSectLeave(pVM, &pVM->pgm.s.CritSectX);
    if (rc == VINF_SEM_NESTED)
        pVM->pgm.s.cDeprecatedPageLocks = cDeprecatedPageLocks;
}


#if !defined(IN_R0) || defined(LOG_ENABLED)

/** Format handler for PGMPAGE.
 * @copydoc FNRTSTRFORMATTYPE */
static DECLCALLBACK(size_t) pgmFormatTypeHandlerPage(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                     const char *pszType, void const *pvValue,
                                                     int cchWidth, int cchPrecision, unsigned fFlags,
                                                     void *pvUser)
{
    size_t    cch;
    PCPGMPAGE pPage = (PCPGMPAGE)pvValue;
    if (RT_VALID_PTR(pPage))
    {
        char szTmp[64+80];

        cch = 0;

        /* The single char state stuff. */
        static const char s_achPageStates[4]    = { 'Z', 'A', 'W', 'S' };
        szTmp[cch++] = s_achPageStates[PGM_PAGE_GET_STATE_NA(pPage)];

# define IS_PART_INCLUDED(lvl) ( !(fFlags & RTSTR_F_PRECISION) || cchPrecision == (lvl) || cchPrecision >= (lvl)+10 )
        if (IS_PART_INCLUDED(5))
        {
            static const char s_achHandlerStates[4*2] = { '-', 't', 'w', 'a' , '_', 'T', 'W', 'A' };
            szTmp[cch++] = s_achHandlerStates[  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage)
                                              | ((uint8_t)PGM_PAGE_IS_HNDL_PHYS_NOT_IN_HM(pPage) << 2)];
        }

        /* The type. */
        if (IS_PART_INCLUDED(4))
        {
            szTmp[cch++] = ':';
            static const char s_achPageTypes[8][4]  = { "INV", "RAM", "MI2", "M2A", "SHA", "ROM", "MIO", "BAD" };
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE_NA(pPage)][0];
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE_NA(pPage)][1];
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE_NA(pPage)][2];
        }

        /* The numbers. */
        if (IS_PART_INCLUDED(3))
        {
            szTmp[cch++] = ':';
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_HCPHYS_NA(pPage), 16, 12, 0, RTSTR_F_ZEROPAD | RTSTR_F_64BIT);
        }

        if (IS_PART_INCLUDED(2))
        {
            szTmp[cch++] = ':';
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_PAGEID(pPage), 16, 7, 0, RTSTR_F_ZEROPAD | RTSTR_F_32BIT);
        }

        if (IS_PART_INCLUDED(6))
        {
            szTmp[cch++] = ':';
            static const char s_achRefs[4] = { '-', 'U', '!', 'L' };
            szTmp[cch++] = s_achRefs[PGM_PAGE_GET_TD_CREFS_NA(pPage)];
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_TD_IDX_NA(pPage), 16, 4, 0, RTSTR_F_ZEROPAD | RTSTR_F_16BIT);
        }
# undef IS_PART_INCLUDED

        cch = pfnOutput(pvArgOutput, szTmp, cch);
    }
    else
        cch = pfnOutput(pvArgOutput, RT_STR_TUPLE("<bad-pgmpage-ptr>"));
    NOREF(pszType); NOREF(cchWidth); NOREF(pvUser);
    return cch;
}


/** Format handler for PGMRAMRANGE.
 * @copydoc FNRTSTRFORMATTYPE */
static DECLCALLBACK(size_t) pgmFormatTypeHandlerRamRange(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                         const char *pszType, void const *pvValue,
                                                         int cchWidth, int cchPrecision, unsigned fFlags,
                                                         void *pvUser)
{
    size_t              cch;
    PGMRAMRANGE const  *pRam = (PGMRAMRANGE const *)pvValue;
    if (RT_VALID_PTR(pRam))
    {
        char szTmp[80];
        cch = RTStrPrintf(szTmp, sizeof(szTmp), "%RGp-%RGp", pRam->GCPhys, pRam->GCPhysLast);
        cch = pfnOutput(pvArgOutput, szTmp, cch);
    }
    else
        cch = pfnOutput(pvArgOutput, RT_STR_TUPLE("<bad-pgmramrange-ptr>"));
    NOREF(pszType); NOREF(cchWidth); NOREF(cchPrecision); NOREF(pvUser); NOREF(fFlags);
    return cch;
}

/** Format type andlers to be registered/deregistered. */
static const struct
{
    char                szType[24];
    PFNRTSTRFORMATTYPE  pfnHandler;
} g_aPgmFormatTypes[] =
{
    { "pgmpage",        pgmFormatTypeHandlerPage },
    { "pgmramrange",    pgmFormatTypeHandlerRamRange }
};

#endif /* !IN_R0 || LOG_ENABLED */


VMMDECL(int) PGMRegisterStringFormatTypes(void)
{
#if !defined(IN_R0) || defined(LOG_ENABLED)
    int         rc = VINF_SUCCESS;
    unsigned    i;
    for (i = 0; RT_SUCCESS(rc) && i < RT_ELEMENTS(g_aPgmFormatTypes); i++)
    {
        rc = RTStrFormatTypeRegister(g_aPgmFormatTypes[i].szType, g_aPgmFormatTypes[i].pfnHandler, NULL);
# ifdef IN_RING0
        if (rc == VERR_ALREADY_EXISTS)
        {
            /* in case of cleanup failure in ring-0 */
            RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);
            rc = RTStrFormatTypeRegister(g_aPgmFormatTypes[i].szType, g_aPgmFormatTypes[i].pfnHandler, NULL);
        }
# endif
    }
    if (RT_FAILURE(rc))
        while (i-- > 0)
            RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);

    return rc;
#else
    return VINF_SUCCESS;
#endif
}


VMMDECL(void) PGMDeregisterStringFormatTypes(void)
{
#if !defined(IN_R0) || defined(LOG_ENABLED)
    for (unsigned i = 0; i < RT_ELEMENTS(g_aPgmFormatTypes); i++)
        RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);
#endif
}


VMMDECL(int)  PGMGstModifyPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    STAM_PROFILE_START(&pVCpu->pgm.s.Stats.CTX_MID_Z(Stat,GstModifyPage), a);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Validate input.
     */
    Assert(cb);

    LogFlow(("PGMGstModifyPage %RGv %d bytes fFlags=%08llx fMask=%08llx\n", GCPtr, cb, fFlags, fMask));
    RT_NOREF(pVCpu, GCPtr, cb, fFlags, fMask);

    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


VMMDECL(PGMMODE) PGMGetGuestMode(PVMCPU pVCpu)
{
    VMCPU_ASSERT_EMT(pVCpu);

    bool fMmuEnabled = CPUMGetGuestMmuEnabled(pVCpu);
    if (!fMmuEnabled)
        return PGMMODE_NONE;

    CPUMMODE enmCpuMode = CPUMGetGuestMode(pVCpu);
    return enmCpuMode == CPUMMODE_ARMV8_AARCH64
         ? PGMMODE_VMSA_V8_64
         : PGMMODE_VMSA_V8_32;
}


VMMDECL(PGMMODE) PGMGetShadowMode(PVMCPU pVCpu)
{
    RT_NOREF(pVCpu);
    return PGMMODE_NONE; /* NEM doesn't need any shadow paging. */
}


DECLINLINE(int) pgmGstWalkReturnNotPresent(PVMCPUCC pVCpu, PPGMPTWALK pWalk, uint8_t uLevel)
{
    NOREF(pVCpu);
    pWalk->fNotPresent     = true;
    pWalk->uLevel          = uLevel;
    pWalk->fFailed         = PGM_WALKFAIL_NOT_PRESENT
                           | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_PAGE_TABLE_NOT_PRESENT;
}

DECLINLINE(int) pgmGstWalkReturnBadPhysAddr(PVMCPUCC pVCpu, PPGMPTWALK pWalk, uint8_t uLevel, int rc)
{
    AssertMsg(rc == VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS, ("%Rrc\n", rc)); NOREF(rc); NOREF(pVCpu);
    pWalk->fBadPhysAddr    = true;
    pWalk->uLevel          = uLevel;
    pWalk->fFailed         = PGM_WALKFAIL_BAD_PHYSICAL_ADDRESS
                           | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) pgmGstWalkReturnRsvdError(PVMCPUCC pVCpu, PPGMPTWALK pWalk, uint8_t uLevel)
{
    NOREF(pVCpu);
    pWalk->fRsvdError      = true;
    pWalk->uLevel          = uLevel;
    pWalk->fFailed         = PGM_WALKFAIL_RESERVED_BITS
                           | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


VMMDECL(int) PGMGstGetPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk)
{
    VMCPU_ASSERT_EMT(pVCpu);
    Assert(pWalk);

    pWalk->fSucceeded = false;

    RTGCPHYS GCPhysPt = CPUMGetEffectiveTtbr(pVCpu, GCPtr);
    if (GCPhysPt == RTGCPHYS_MAX) /* MMU disabled? */
    {
        pWalk->GCPtr      = GCPtr;
        pWalk->fSucceeded = true;
        pWalk->GCPhys     = GCPtr;
        return VINF_SUCCESS;
    }

    /* Do the translation. */
    /** @todo This is just a sketch to get something working for debugging, assumes 4KiB granules and 48-bit output address.
     *        Needs to be moved to PGMAllGst like on x86 and implemented for 16KiB and 64KiB granule sizes. */
    uint64_t u64TcrEl1 = CPUMGetTcrEl1(pVCpu);
    uint8_t u8TxSz =   (GCPtr & RT_BIT_64(55))
                     ? ARMV8_TCR_EL1_AARCH64_T1SZ_GET(u64TcrEl1)
                     : ARMV8_TCR_EL1_AARCH64_T0SZ_GET(u64TcrEl1);
    uint8_t uLookupLvl;
    RTGCPHYS fLookupMask;

    /*
     * From: https://github.com/codingbelief/arm-architecture-reference-manual-for-armv8-a/blob/master/en/chapter_d4/d42_2_controlling_address_translation_stages.md
     * For all translation stages
     * The maximum TxSZ value is 39. If TxSZ is programmed to a value larger than 39 then it is IMPLEMENTATION DEFINED whether:
     *     - The implementation behaves as if the field is programmed to 39 for all purposes other than reading back the value of the field.
     *     - Any use of the TxSZ value generates a Level 0 Translation fault for the stage of translation at which TxSZ is used.
     *
     * For a stage 1 translation
     * The minimum TxSZ value is 16. If TxSZ is programmed to a value smaller than 16 then it is IMPLEMENTATION DEFINED whether:
     *     - The implementation behaves as if the field were programmed to 16 for all purposes other than reading back the value of the field.
     *     - Any use of the TxSZ value generates a stage 1 Level 0 Translation fault.
     *
     * We currently choose the former for both.
     */
    if (/*u8TxSz >= 16 &&*/ u8TxSz <= 24)
    {
        uLookupLvl = 0;
        fLookupMask = RT_BIT_64(24 - u8TxSz + 1) - 1;
    }
    else if (u8TxSz >= 25 && u8TxSz <= 33)
    {
        uLookupLvl = 1;
        fLookupMask = RT_BIT_64(33 - u8TxSz + 1) - 1;
    }
    else /*if (u8TxSz >= 34 && u8TxSz <= 39)*/
    {
        uLookupLvl = 2;
        fLookupMask = RT_BIT_64(39 - u8TxSz + 1) - 1;
    }
    /*else
        return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 0, VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS);*/ /** @todo Better status (Invalid TCR config). */

    uint64_t *pu64Pt = NULL;
    uint64_t uPt;
    int rc;
    if (uLookupLvl == 0)
    {
        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 0, rc);

        uPt = pu64Pt[(GCPtr >> 39) & fLookupMask];
        if (uPt & RT_BIT_64(0)) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 0);

        if (uPt & RT_BIT_64(1)) { /* probable */ }
        else return pgmGstWalkReturnRsvdError(pVCpu, pWalk, 0); /** @todo Only supported if TCR_EL1.DS is set. */

        /* All nine bits from now on. */
        fLookupMask = RT_BIT_64(9) - 1;
        GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
    }

    if (uLookupLvl <= 1)
    {
        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 1, rc);

        uPt = pu64Pt[(GCPtr >> 30) & fLookupMask];
        if (uPt & RT_BIT_64(0)) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 1);

        if (uPt & RT_BIT_64(1)) { /* probable */ }
        else
        {
            /* Block descriptor (1G page). */
            pWalk->GCPtr       = GCPtr;
            pWalk->fSucceeded  = true;
            pWalk->GCPhys      = (RTGCPHYS)(uPt & UINT64_C(0xffffc0000000)) | (GCPtr & (RTGCPTR)(_1G - 1));
            pWalk->fGigantPage = true;
            return VINF_SUCCESS;
        }

        /* All nine bits from now on. */
        fLookupMask = RT_BIT_64(9) - 1;
        GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
    }

    if (uLookupLvl <= 2)
    {
        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 2, rc);

        uPt = pu64Pt[(GCPtr >> 21) & fLookupMask];
        if (uPt & RT_BIT_64(0)) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 2);

        if (uPt & RT_BIT_64(1)) { /* probable */ }
        else
        {
            /* Block descriptor (2M page). */
            pWalk->GCPtr      = GCPtr;
            pWalk->fSucceeded = true;
            pWalk->GCPhys     = (RTGCPHYS)(uPt & UINT64_C(0xffffffe00000)) | (GCPtr & (RTGCPTR)(_2M - 1));
            pWalk->fBigPage = true;
            return VINF_SUCCESS;
        }

        /* All nine bits from now on. */
        fLookupMask = RT_BIT_64(9) - 1;
        GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
    }

    Assert(uLookupLvl <= 3);

    /* Next level. */
    rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
    if (RT_SUCCESS(rc)) { /* probable */ }
    else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 3, rc);

    uPt = pu64Pt[(GCPtr & UINT64_C(0x1ff000)) >> 12];
    if (uPt & RT_BIT_64(0)) { /* probable */ }
    else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 3);

    if (uPt & RT_BIT_64(1)) { /* probable */ }
    else return pgmGstWalkReturnRsvdError(pVCpu, pWalk, 3); /** No block descriptors. */

    pWalk->GCPtr      = GCPtr;
    pWalk->fSucceeded = true;
    pWalk->GCPhys     = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000)) | (GCPtr & (RTGCPTR)(_4K - 1));
    return VINF_SUCCESS;
}


VMMDECL(int) PGMShwMakePageReadonly(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fOpFlags)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, GCPtr, fOpFlags);
    return VERR_NOT_IMPLEMENTED;
}


VMMDECL(int) PGMShwMakePageWritable(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fOpFlags)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, GCPtr, fOpFlags);
    return VERR_NOT_IMPLEMENTED;
}


VMMDECL(int) PGMShwMakePageNotPresent(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fOpFlags)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, GCPtr, fOpFlags);
    return VERR_NOT_IMPLEMENTED;
}


VMM_INT_DECL(int) PGMHCChangeMode(PVMCC pVM, PVMCPUCC pVCpu, PGMMODE enmGuestMode, bool fForce)
{
    //AssertReleaseFailed(); /** @todo Called by the PGM saved state code. */
    RT_NOREF(pVM, pVCpu, enmGuestMode, fForce);
    return VINF_SUCCESS;
}


VMMDECL(int) PGMShwGetPage(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys)
{
    AssertReleaseFailed();
    RT_NOREF(pVCpu, GCPtr, pfFlags, pHCPhys);
    return VERR_NOT_SUPPORTED;
}


int pgmR3ExitShadowModeBeforePoolFlush(PVMCPU pVCpu)
{
    RT_NOREF(pVCpu);
    return VINF_SUCCESS;
}


int pgmR3ReEnterShadowModeAfterPoolFlush(PVM pVM, PVMCPU pVCpu)
{
    RT_NOREF(pVM, pVCpu);
    return VINF_SUCCESS;
}


void pgmR3RefreshShadowModeAfterA20Change(PVMCPU pVCpu)
{
    RT_NOREF(pVCpu);
}


int pgmGstPtWalk(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk, PPGMPTWALKGST pGstWalk)
{
    VMCPU_ASSERT_EMT(pVCpu);
    RT_NOREF(pGstWalk);
    return PGMGstGetPage(pVCpu, GCPtr, pWalk);
}


int pgmGstPtWalkNext(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk, PPGMPTWALKGST pGstWalk)
{
    VMCPU_ASSERT_EMT(pVCpu);
    return pgmGstPtWalk(pVCpu, GCPtr, pWalk, pGstWalk); /** @todo Always do full walk for now. */
}

