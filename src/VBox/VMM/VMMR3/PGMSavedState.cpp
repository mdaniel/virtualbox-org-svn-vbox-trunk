/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, The Saved State Part.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/vmm/pdmdev.h>
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"

#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/vmm/ftm.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/crc.h>
#include <iprt/mem.h>
#include <iprt/sha.h>
#include <iprt/string.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Saved state data unit version.  */
#define PGM_SAVED_STATE_VERSION                 14
/** Saved state data unit version before the PAE PDPE registers. */
#define PGM_SAVED_STATE_VERSION_PRE_PAE         13
/** Saved state data unit version after this includes ballooned page flags in
 *  the state (see @bugref{5515}). */
#define PGM_SAVED_STATE_VERSION_BALLOON_BROKEN  12
/** Saved state before the balloon change. */
#define PGM_SAVED_STATE_VERSION_PRE_BALLOON     11
/** Saved state data unit version used during 3.1 development, misses the RAM
 *  config. */
#define PGM_SAVED_STATE_VERSION_NO_RAM_CFG      10
/** Saved state data unit version for 3.0 (pre teleportation). */
#define PGM_SAVED_STATE_VERSION_3_0_0           9
/** Saved state data unit version for 2.2.2 and later. */
#define PGM_SAVED_STATE_VERSION_2_2_2           8
/** Saved state data unit version for 2.2.0. */
#define PGM_SAVED_STATE_VERSION_RR_DESC         7
/** Saved state data unit version. */
#define PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE   6


/** @name Sparse state record types
 * @{  */
/** Zero page. No data. */
#define PGM_STATE_REC_RAM_ZERO          UINT8_C(0x00)
/** Raw page. */
#define PGM_STATE_REC_RAM_RAW           UINT8_C(0x01)
/** Raw MMIO2 page. */
#define PGM_STATE_REC_MMIO2_RAW         UINT8_C(0x02)
/** Zero MMIO2 page. */
#define PGM_STATE_REC_MMIO2_ZERO        UINT8_C(0x03)
/** Virgin ROM page. Followed by protection (8-bit) and the raw bits. */
#define PGM_STATE_REC_ROM_VIRGIN        UINT8_C(0x04)
/** Raw shadowed ROM page. The protection (8-bit) precedes the raw bits. */
#define PGM_STATE_REC_ROM_SHW_RAW       UINT8_C(0x05)
/** Zero shadowed ROM page. The protection (8-bit) is the only payload. */
#define PGM_STATE_REC_ROM_SHW_ZERO      UINT8_C(0x06)
/** ROM protection (8-bit). */
#define PGM_STATE_REC_ROM_PROT          UINT8_C(0x07)
/** Ballooned page. No data. */
#define PGM_STATE_REC_RAM_BALLOONED     UINT8_C(0x08)
/** The last record type. */
#define PGM_STATE_REC_LAST              PGM_STATE_REC_RAM_BALLOONED
/** End marker. */
#define PGM_STATE_REC_END               UINT8_C(0xff)
/** Flag indicating that the data is preceded by the page address.
 *  For RAW pages this is a RTGCPHYS.  For MMIO2 and ROM pages this is a 8-bit
 *  range ID and a 32-bit page index.
 */
#define PGM_STATE_REC_FLAG_ADDR         UINT8_C(0x80)
/** @} */

/** The CRC-32 for a zero page. */
#define PGM_STATE_CRC32_ZERO_PAGE       UINT32_C(0xc71c0011)
/** The CRC-32 for a zero half page. */
#define PGM_STATE_CRC32_ZERO_HALF_PAGE  UINT32_C(0xf1e8ba9e)



/** @name Old Page types used in older saved states.
 * @{  */
/** Old saved state: The usual invalid zero entry. */
#define PGMPAGETYPE_OLD_INVALID             0
/** Old saved state: RAM page. (RWX) */
#define PGMPAGETYPE_OLD_RAM                 1
/** Old saved state: MMIO2 page. (RWX) */
#define PGMPAGETYPE_OLD_MMIO2               1
/** Old saved state: MMIO2 page aliased over an MMIO page. (RWX)
 * See PGMHandlerPhysicalPageAlias(). */
#define PGMPAGETYPE_OLD_MMIO2_ALIAS_MMIO    2
/** Old saved state: Shadowed ROM. (RWX) */
#define PGMPAGETYPE_OLD_ROM_SHADOW          3
/** Old saved state: ROM page. (R-X) */
#define PGMPAGETYPE_OLD_ROM                 4
/** Old saved state: MMIO page. (---) */
#define PGMPAGETYPE_OLD_MMIO                5
/** @}  */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** For loading old saved states. (pre-smp) */
typedef struct
{
    /** If set no conflict checks are required.  (boolean) */
    bool                            fMappingsFixed;
    /** Size of fixed mapping */
    uint32_t                        cbMappingFixed;
    /** Base address (GC) of fixed mapping */
    RTGCPTR                         GCPtrMappingFixed;
    /** A20 gate mask.
     * Our current approach to A20 emulation is to let REM do it and don't bother
     * anywhere else. The interesting guests will be operating with it enabled anyway.
     * But should the need arise, we'll subject physical addresses to this mask. */
    RTGCPHYS                        GCPhysA20Mask;
    /** A20 gate state - boolean! */
    bool                            fA20Enabled;
    /** The guest paging mode. */
    PGMMODE                         enmGuestMode;
} PGMOLD;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** PGM fields to save/load. */

static const SSMFIELD s_aPGMFields[] =
{
    SSMFIELD_ENTRY(         PGM, fMappingsFixed),
    SSMFIELD_ENTRY_GCPTR(   PGM, GCPtrMappingFixed),
    SSMFIELD_ENTRY(         PGM, cbMappingFixed),
    SSMFIELD_ENTRY(         PGM, cBalloonedPages),
    SSMFIELD_ENTRY_TERM()
};

static const SSMFIELD s_aPGMFieldsPreBalloon[] =
{
    SSMFIELD_ENTRY(         PGM, fMappingsFixed),
    SSMFIELD_ENTRY_GCPTR(   PGM, GCPtrMappingFixed),
    SSMFIELD_ENTRY(         PGM, cbMappingFixed),
    SSMFIELD_ENTRY_TERM()
};

static const SSMFIELD s_aPGMCpuFields[] =
{
    SSMFIELD_ENTRY(         PGMCPU, fA20Enabled),
    SSMFIELD_ENTRY_GCPHYS(  PGMCPU, GCPhysA20Mask),
    SSMFIELD_ENTRY(         PGMCPU, enmGuestMode),
    SSMFIELD_ENTRY(         PGMCPU, aGCPhysGstPaePDs[0]),
    SSMFIELD_ENTRY(         PGMCPU, aGCPhysGstPaePDs[1]),
    SSMFIELD_ENTRY(         PGMCPU, aGCPhysGstPaePDs[2]),
    SSMFIELD_ENTRY(         PGMCPU, aGCPhysGstPaePDs[3]),
    SSMFIELD_ENTRY_TERM()
};

static const SSMFIELD s_aPGMCpuFieldsPrePae[] =
{
    SSMFIELD_ENTRY(         PGMCPU, fA20Enabled),
    SSMFIELD_ENTRY_GCPHYS(  PGMCPU, GCPhysA20Mask),
    SSMFIELD_ENTRY(         PGMCPU, enmGuestMode),
    SSMFIELD_ENTRY_TERM()
};

static const SSMFIELD s_aPGMFields_Old[] =
{
    SSMFIELD_ENTRY(         PGMOLD, fMappingsFixed),
    SSMFIELD_ENTRY_GCPTR(   PGMOLD, GCPtrMappingFixed),
    SSMFIELD_ENTRY(         PGMOLD, cbMappingFixed),
    SSMFIELD_ENTRY(         PGMOLD, fA20Enabled),
    SSMFIELD_ENTRY_GCPHYS(  PGMOLD, GCPhysA20Mask),
    SSMFIELD_ENTRY(         PGMOLD, enmGuestMode),
    SSMFIELD_ENTRY_TERM()
};


/**
 * Find the ROM tracking structure for the given page.
 *
 * @returns Pointer to the ROM page structure. NULL if the caller didn't check
 *          that it's a ROM page.
 * @param   pVM         The cross context VM structure.
 * @param   GCPhys      The address of the ROM page.
 */
static PPGMROMPAGE pgmR3GetRomPage(PVM pVM, RTGCPHYS GCPhys) /** @todo change this to take a hint. */
{
    for (PPGMROMRANGE pRomRange = pVM->pgm.s.CTX_SUFF(pRomRanges);
         pRomRange;
         pRomRange = pRomRange->CTX_SUFF(pNext))
    {
        RTGCPHYS off = GCPhys - pRomRange->GCPhys;
        if (GCPhys - pRomRange->GCPhys < pRomRange->cb)
            return &pRomRange->aPages[off >> PAGE_SHIFT];
    }
    return NULL;
}


/**
 * Prepares the ROM pages for a live save.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
static int pgmR3PrepRomPages(PVM pVM)
{
    /*
     * Initialize the live save tracking in the ROM page descriptors.
     */
    pgmLock(pVM);
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        PPGMRAMRANGE    pRamHint = NULL;;
        uint32_t const  cPages   = pRom->cb >> PAGE_SHIFT;

        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            pRom->aPages[iPage].LiveSave.u8Prot           = (uint8_t)PGMROMPROT_INVALID;
            pRom->aPages[iPage].LiveSave.fWrittenTo       = false;
            pRom->aPages[iPage].LiveSave.fDirty           = true;
            pRom->aPages[iPage].LiveSave.fDirtiedRecently = true;
            if (!(pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED))
            {
                if (PGMROMPROT_IS_ROM(pRom->aPages[iPage].enmProt))
                    pRom->aPages[iPage].LiveSave.fWrittenTo = !PGM_PAGE_IS_ZERO(&pRom->aPages[iPage].Shadow) && !PGM_PAGE_IS_BALLOONED(&pRom->aPages[iPage].Shadow);
                else
                {
                    RTGCPHYS GCPhys = pRom->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
                    PPGMPAGE pPage;
                    int rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, &pRamHint);
                    AssertLogRelMsgRC(rc, ("%Rrc GCPhys=%RGp\n", rc, GCPhys));
                    if (RT_SUCCESS(rc))
                        pRom->aPages[iPage].LiveSave.fWrittenTo = !PGM_PAGE_IS_ZERO(pPage) && !PGM_PAGE_IS_BALLOONED(pPage);
                    else
                        pRom->aPages[iPage].LiveSave.fWrittenTo = !PGM_PAGE_IS_ZERO(&pRom->aPages[iPage].Shadow) && !PGM_PAGE_IS_BALLOONED(&pRom->aPages[iPage].Shadow);
                }
            }
        }

        pVM->pgm.s.LiveSave.Rom.cDirtyPages += cPages;
        if (pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
            pVM->pgm.s.LiveSave.Rom.cDirtyPages += cPages;
    }
    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * Assigns IDs to the ROM ranges and saves them.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                Saved state handle.
 */
static int pgmR3SaveRomRanges(PVM pVM, PSSMHANDLE pSSM)
{
    pgmLock(pVM);
    uint8_t id = 1;
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3, id++)
    {
        pRom->idSavedState = id;
        SSMR3PutU8(pSSM, id);
        SSMR3PutStrZ(pSSM, "");         /* device name */
        SSMR3PutU32(pSSM, 0);           /* device instance */
        SSMR3PutU8(pSSM, 0);            /* region */
        SSMR3PutStrZ(pSSM, pRom->pszDesc);
        SSMR3PutGCPhys(pSSM, pRom->GCPhys);
        int rc = SSMR3PutGCPhys(pSSM, pRom->cb);
        if (RT_FAILURE(rc))
            break;
    }
    pgmUnlock(pVM);
    return SSMR3PutU8(pSSM, UINT8_MAX);
}


/**
 * Loads the ROM range ID assignments.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The saved state handle.
 */
static int pgmR3LoadRomRanges(PVM pVM, PSSMHANDLE pSSM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
        pRom->idSavedState = UINT8_MAX;

    for (;;)
    {
        /*
         * Read the data.
         */
        uint8_t id;
        int rc = SSMR3GetU8(pSSM, &id);
        if (RT_FAILURE(rc))
            return rc;
        if (id == UINT8_MAX)
        {
            for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
                AssertLogRelMsg(pRom->idSavedState != UINT8_MAX,
                                ("The \"%s\" ROM was not found in the saved state. Probably due to some misconfiguration\n",
                                 pRom->pszDesc));
            return VINF_SUCCESS;        /* the end */
        }
        AssertLogRelReturn(id != 0, VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        char szDevName[RT_SIZEOFMEMB(PDMDEVREG, szName)];
        rc = SSMR3GetStrZ(pSSM, szDevName, sizeof(szDevName));
        AssertLogRelRCReturn(rc, rc);

        uint32_t    uInstance;
        SSMR3GetU32(pSSM, &uInstance);
        uint8_t     iRegion;
        SSMR3GetU8(pSSM, &iRegion);

        char szDesc[64];
        rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
        AssertLogRelRCReturn(rc, rc);

        RTGCPHYS GCPhys;
        SSMR3GetGCPhys(pSSM, &GCPhys);
        RTGCPHYS cb;
        rc = SSMR3GetGCPhys(pSSM, &cb);
        if (RT_FAILURE(rc))
            return rc;
        AssertLogRelMsgReturn(!(GCPhys & PAGE_OFFSET_MASK), ("GCPhys=%RGp %s\n", GCPhys, szDesc), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        AssertLogRelMsgReturn(!(cb & PAGE_OFFSET_MASK),     ("cb=%RGp %s\n", cb, szDesc),         VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /*
         * Locate a matching ROM range.
         */
        AssertLogRelMsgReturn(   uInstance == 0
                              && iRegion == 0
                              && szDevName[0] == '\0',
                              ("GCPhys=%RGp %s\n", GCPhys, szDesc),
                              VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        PPGMROMRANGE pRom;
        for (pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
        {
            if (    pRom->idSavedState == UINT8_MAX
                &&  !strcmp(pRom->pszDesc, szDesc))
            {
                pRom->idSavedState = id;
                break;
            }
        }
        if (!pRom)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("ROM at %RGp by the name '%s' was not found"), GCPhys, szDesc);
    } /* forever */
}


/**
 * Scan ROM pages.
 *
 * @param   pVM                 The cross context VM structure.
 */
static void pgmR3ScanRomPages(PVM pVM)
{
    /*
     * The shadow ROMs.
     */
    pgmLock(pVM);
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        if (pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
        {
            uint32_t const cPages = pRom->cb >> PAGE_SHIFT;
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                PPGMROMPAGE pRomPage = &pRom->aPages[iPage];
                if (pRomPage->LiveSave.fWrittenTo)
                {
                    pRomPage->LiveSave.fWrittenTo = false;
                    if (!pRomPage->LiveSave.fDirty)
                    {
                        pRomPage->LiveSave.fDirty = true;
                        pVM->pgm.s.LiveSave.Rom.cReadyPages--;
                        pVM->pgm.s.LiveSave.Rom.cDirtyPages++;
                    }
                    pRomPage->LiveSave.fDirtiedRecently = true;
                }
                else
                    pRomPage->LiveSave.fDirtiedRecently = false;
            }
        }
    }
    pgmUnlock(pVM);
}


/**
 * Takes care of the virgin ROM pages in the first pass.
 *
 * This is an attempt at simplifying the handling of ROM pages a little bit.
 * This ASSUMES that no new ROM ranges will be added and that they won't be
 * relinked in any way.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   fLiveSave           Whether we're in a live save or not.
 */
static int pgmR3SaveRomVirginPages(PVM pVM, PSSMHANDLE pSSM, bool fLiveSave)
{
    if (FTMIsDeltaLoadSaveActive(pVM))
        return VINF_SUCCESS;    /* nothing to do as nothing has changed here */

    pgmLock(pVM);
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        uint32_t const cPages = pRom->cb >> PAGE_SHIFT;
        for (uint32_t iPage = 0; iPage < cPages; iPage++)
        {
            RTGCPHYS   GCPhys  = pRom->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
            PGMROMPROT enmProt = pRom->aPages[iPage].enmProt;

            /* Get the virgin page descriptor. */
            PPGMPAGE pPage;
            if (PGMROMPROT_IS_ROM(enmProt))
                pPage = pgmPhysGetPage(pVM, GCPhys);
            else
                pPage = &pRom->aPages[iPage].Virgin;

            /* Get the page bits. (Cannot use pgmPhysGCPhys2CCPtrInternalReadOnly here!) */
            int rc = VINF_SUCCESS;
            char abPage[PAGE_SIZE];
            if (    !PGM_PAGE_IS_ZERO(pPage)
                &&  !PGM_PAGE_IS_BALLOONED(pPage))
            {
                void const *pvPage;
                rc = pgmPhysPageMapReadOnly(pVM, pPage, GCPhys, &pvPage);
                if (RT_SUCCESS(rc))
                    memcpy(abPage, pvPage, PAGE_SIZE);
            }
            else
                ASMMemZeroPage(abPage);
            pgmUnlock(pVM);
            AssertLogRelMsgRCReturn(rc, ("rc=%Rrc GCPhys=%RGp\n", rc, GCPhys), rc);

            /* Save it. */
            if (iPage > 0)
                SSMR3PutU8(pSSM, PGM_STATE_REC_ROM_VIRGIN);
            else
            {
                SSMR3PutU8(pSSM, PGM_STATE_REC_ROM_VIRGIN | PGM_STATE_REC_FLAG_ADDR);
                SSMR3PutU8(pSSM, pRom->idSavedState);
                SSMR3PutU32(pSSM, iPage);
            }
            SSMR3PutU8(pSSM, (uint8_t)enmProt);
            rc = SSMR3PutMem(pSSM, abPage, PAGE_SIZE);
            if (RT_FAILURE(rc))
                return rc;

            /* Update state. */
            pgmLock(pVM);
            pRom->aPages[iPage].LiveSave.u8Prot = (uint8_t)enmProt;
            if (fLiveSave)
            {
                pVM->pgm.s.LiveSave.Rom.cDirtyPages--;
                pVM->pgm.s.LiveSave.Rom.cReadyPages++;
                pVM->pgm.s.LiveSave.cSavedPages++;
            }
        }
    }
    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Saves dirty pages in the shadowed ROM ranges.
 *
 * Used by pgmR3LiveExecPart2 and pgmR3SaveExecMemory.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   fLiveSave           Whether it's a live save or not.
 * @param   fFinalPass          Whether this is the final pass or not.
 */
static int pgmR3SaveShadowedRomPages(PVM pVM, PSSMHANDLE pSSM, bool fLiveSave, bool fFinalPass)
{
    if (FTMIsDeltaLoadSaveActive(pVM))
        return VINF_SUCCESS;    /* nothing to do as we deal with those pages separately */

    /*
     * The Shadowed ROMs.
     *
     * ASSUMES that the ROM ranges are fixed.
     * ASSUMES that all the ROM ranges are mapped.
     */
    pgmLock(pVM);
    for (PPGMROMRANGE pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
    {
        if (pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)
        {
            uint32_t const cPages    = pRom->cb >> PAGE_SHIFT;
            uint32_t       iPrevPage = cPages;
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                PPGMROMPAGE pRomPage = &pRom->aPages[iPage];
                if (    !fLiveSave
                    ||  (   pRomPage->LiveSave.fDirty
                         && (   (   !pRomPage->LiveSave.fDirtiedRecently
                                 && !pRomPage->LiveSave.fWrittenTo)
                             || fFinalPass
                             )
                         )
                    )
                {
                    uint8_t     abPage[PAGE_SIZE];
                    PGMROMPROT  enmProt = pRomPage->enmProt;
                    RTGCPHYS    GCPhys  = pRom->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
                    PPGMPAGE    pPage   = PGMROMPROT_IS_ROM(enmProt) ? &pRomPage->Shadow : pgmPhysGetPage(pVM, GCPhys);
                    bool        fZero   = PGM_PAGE_IS_ZERO(pPage) || PGM_PAGE_IS_BALLOONED(pPage); Assert(!PGM_PAGE_IS_BALLOONED(pPage)); /* Shouldn't be ballooned. */
                    int         rc      = VINF_SUCCESS;
                    if (!fZero)
                    {
                        void const *pvPage;
                        rc = pgmPhysPageMapReadOnly(pVM, pPage, GCPhys, &pvPage);
                        if (RT_SUCCESS(rc))
                            memcpy(abPage, pvPage, PAGE_SIZE);
                    }
                    if (fLiveSave && RT_SUCCESS(rc))
                    {
                        pRomPage->LiveSave.u8Prot = (uint8_t)enmProt;
                        pRomPage->LiveSave.fDirty = false;
                        pVM->pgm.s.LiveSave.Rom.cReadyPages++;
                        pVM->pgm.s.LiveSave.Rom.cDirtyPages--;
                        pVM->pgm.s.LiveSave.cSavedPages++;
                    }
                    pgmUnlock(pVM);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc GCPhys=%RGp\n", rc, GCPhys), rc);

                    if (iPage - 1U == iPrevPage && iPage > 0)
                        SSMR3PutU8(pSSM, (fZero ? PGM_STATE_REC_ROM_SHW_ZERO : PGM_STATE_REC_ROM_SHW_RAW));
                    else
                    {
                        SSMR3PutU8(pSSM, (fZero ? PGM_STATE_REC_ROM_SHW_ZERO : PGM_STATE_REC_ROM_SHW_RAW) | PGM_STATE_REC_FLAG_ADDR);
                        SSMR3PutU8(pSSM, pRom->idSavedState);
                        SSMR3PutU32(pSSM, iPage);
                    }
                    rc = SSMR3PutU8(pSSM, (uint8_t)enmProt);
                    if (!fZero)
                        rc = SSMR3PutMem(pSSM, abPage, PAGE_SIZE);
                    if (RT_FAILURE(rc))
                        return rc;

                    pgmLock(pVM);
                    iPrevPage = iPage;
                }
                /*
                 * In the final pass, make sure the protection is in sync.
                 */
                else if (   fFinalPass
                         && pRomPage->LiveSave.u8Prot != pRomPage->enmProt)
                {
                    PGMROMPROT enmProt = pRomPage->enmProt;
                    pRomPage->LiveSave.u8Prot = (uint8_t)enmProt;
                    pgmUnlock(pVM);

                    if (iPage - 1U == iPrevPage && iPage > 0)
                        SSMR3PutU8(pSSM, PGM_STATE_REC_ROM_PROT);
                    else
                    {
                        SSMR3PutU8(pSSM, PGM_STATE_REC_ROM_PROT | PGM_STATE_REC_FLAG_ADDR);
                        SSMR3PutU8(pSSM, pRom->idSavedState);
                        SSMR3PutU32(pSSM, iPage);
                    }
                    int rc = SSMR3PutU8(pSSM, (uint8_t)enmProt);
                    if (RT_FAILURE(rc))
                        return rc;

                    pgmLock(pVM);
                    iPrevPage = iPage;
                }
            }
        }
    }
    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Cleans up ROM pages after a live save.
 *
 * @param   pVM                 The cross context VM structure.
 */
static void pgmR3DoneRomPages(PVM pVM)
{
    NOREF(pVM);
}


/**
 * Prepares the MMIO2 pages for a live save.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
static int pgmR3PrepMmio2Pages(PVM pVM)
{
    /*
     * Initialize the live save tracking in the MMIO2 ranges.
     * ASSUME nothing changes here.
     */
    pgmLock(pVM);
    for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
    {
        if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
        {
            uint32_t const  cPages = pRegMmio->RamRange.cb >> PAGE_SHIFT;
            pgmUnlock(pVM);

            PPGMLIVESAVEMMIO2PAGE paLSPages = (PPGMLIVESAVEMMIO2PAGE)MMR3HeapAllocZ(pVM, MM_TAG_PGM, sizeof(PGMLIVESAVEMMIO2PAGE) * cPages);
            if (!paLSPages)
                return VERR_NO_MEMORY;
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                /* Initialize it as a dirty zero page. */
                paLSPages[iPage].fDirty          = true;
                paLSPages[iPage].cUnchangedScans = 0;
                paLSPages[iPage].fZero           = true;
                paLSPages[iPage].u32CrcH1        = PGM_STATE_CRC32_ZERO_HALF_PAGE;
                paLSPages[iPage].u32CrcH2        = PGM_STATE_CRC32_ZERO_HALF_PAGE;
            }

            pgmLock(pVM);
            pRegMmio->paLSPages = paLSPages;
            pVM->pgm.s.LiveSave.Mmio2.cDirtyPages += cPages;
        }
    }
    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Assigns IDs to the MMIO2 ranges and saves them.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                Saved state handle.
 */
static int pgmR3SaveMmio2Ranges(PVM pVM, PSSMHANDLE pSSM)
{
    pgmLock(pVM);
    uint8_t id = 1;
    for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
    {
        if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
        {
            pRegMmio->idSavedState = id;
            SSMR3PutU8(pSSM, id);
            SSMR3PutStrZ(pSSM, pRegMmio->pDevInsR3->pReg->szName);
            SSMR3PutU32(pSSM, pRegMmio->pDevInsR3->iInstance);
            SSMR3PutU8(pSSM, pRegMmio->iRegion);
            SSMR3PutStrZ(pSSM, pRegMmio->RamRange.pszDesc);
            int rc = SSMR3PutGCPhys(pSSM, pRegMmio->RamRange.cb);
            if (RT_FAILURE(rc))
                break;
            id++;
        }
    }
    pgmUnlock(pVM);
    return SSMR3PutU8(pSSM, UINT8_MAX);
}


/**
 * Loads the MMIO2 range ID assignments.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The saved state handle.
 */
static int pgmR3LoadMmio2Ranges(PVM pVM, PSSMHANDLE pSSM)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
        if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
            pRegMmio->idSavedState = UINT8_MAX;

    for (;;)
    {
        /*
         * Read the data.
         */
        uint8_t id;
        int rc = SSMR3GetU8(pSSM, &id);
        if (RT_FAILURE(rc))
            return rc;
        if (id == UINT8_MAX)
        {
            for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
                AssertLogRelMsg(   pRegMmio->idSavedState != UINT8_MAX
                                || !(pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2),
                                ("%s\n", pRegMmio->RamRange.pszDesc));
            return VINF_SUCCESS;        /* the end */
        }
        AssertLogRelReturn(id != 0, VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        char szDevName[RT_SIZEOFMEMB(PDMDEVREG, szName)];
        rc = SSMR3GetStrZ(pSSM, szDevName, sizeof(szDevName));
        AssertLogRelRCReturn(rc, rc);

        uint32_t    uInstance;
        SSMR3GetU32(pSSM, &uInstance);
        uint8_t     iRegion;
        SSMR3GetU8(pSSM, &iRegion);

        char szDesc[64];
        rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
        AssertLogRelRCReturn(rc, rc);

        RTGCPHYS cb;
        rc = SSMR3GetGCPhys(pSSM, &cb);
        AssertLogRelMsgReturn(!(cb & PAGE_OFFSET_MASK), ("cb=%RGp %s\n", cb, szDesc), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /*
         * Locate a matching MMIO2 range.
         */
        PPGMREGMMIORANGE pRegMmio;
        for (pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
        {
            if (    pRegMmio->idSavedState == UINT8_MAX
                &&  pRegMmio->iRegion == iRegion
                &&  pRegMmio->pDevInsR3->iInstance == uInstance
                &&  (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
                &&  !strcmp(pRegMmio->pDevInsR3->pReg->szName, szDevName))
            {
                pRegMmio->idSavedState = id;
                break;
            }
        }
        if (!pRegMmio)
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Failed to locate a MMIO2 range called '%s' owned by %s/%u, region %d"),
                                    szDesc, szDevName, uInstance, iRegion);

        /*
         * Validate the configuration, the size of the MMIO2 region should be
         * the same.
         */
        if (cb != pRegMmio->RamRange.cb)
        {
            LogRel(("PGM: MMIO2 region \"%s\" size mismatch: saved=%RGp config=%RGp\n",
                    pRegMmio->RamRange.pszDesc, cb, pRegMmio->RamRange.cb));
            if (cb > pRegMmio->RamRange.cb) /* bad idea? */
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("MMIO2 region \"%s\" size mismatch: saved=%RGp config=%RGp"),
                                        pRegMmio->RamRange.pszDesc, cb, pRegMmio->RamRange.cb);
        }
    } /* forever */
}


/**
 * Scans one MMIO2 page.
 *
 * @returns True if changed, false if unchanged.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pbPage              The page bits.
 * @param   pLSPage             The live save tracking structure for the page.
 *
 */
DECLINLINE(bool) pgmR3ScanMmio2Page(PVM pVM, uint8_t const *pbPage, PPGMLIVESAVEMMIO2PAGE pLSPage)
{
    /*
     * Special handling of zero pages.
     */
    bool const fZero = pLSPage->fZero;
    if (fZero)
    {
        if (ASMMemIsZeroPage(pbPage))
        {
            /* Not modified. */
            if (pLSPage->fDirty)
                pLSPage->cUnchangedScans++;
            return false;
        }

        pLSPage->fZero    = false;
        pLSPage->u32CrcH1 = RTCrc32(pbPage, PAGE_SIZE / 2);
    }
    else
    {
        /*
         * CRC the first half, if it doesn't match the page is dirty and
         * we won't check the 2nd half (we'll do that next time).
         */
        uint32_t u32CrcH1 = RTCrc32(pbPage, PAGE_SIZE / 2);
        if (u32CrcH1 == pLSPage->u32CrcH1)
        {
            uint32_t u32CrcH2 = RTCrc32(pbPage + PAGE_SIZE / 2, PAGE_SIZE / 2);
            if (u32CrcH2 == pLSPage->u32CrcH2)
            {
                /* Probably not modified. */
                if (pLSPage->fDirty)
                    pLSPage->cUnchangedScans++;
                return false;
            }

            pLSPage->u32CrcH2 = u32CrcH2;
        }
        else
        {
            pLSPage->u32CrcH1 = u32CrcH1;
            if (    u32CrcH1 == PGM_STATE_CRC32_ZERO_HALF_PAGE
                &&  ASMMemIsZeroPage(pbPage))
            {
                pLSPage->u32CrcH2 = PGM_STATE_CRC32_ZERO_HALF_PAGE;
                pLSPage->fZero    = true;
            }
        }
    }

    /* dirty page path */
    pLSPage->cUnchangedScans = 0;
    if (!pLSPage->fDirty)
    {
        pLSPage->fDirty = true;
        pVM->pgm.s.LiveSave.Mmio2.cReadyPages--;
        pVM->pgm.s.LiveSave.Mmio2.cDirtyPages++;
        if (fZero)
            pVM->pgm.s.LiveSave.Mmio2.cZeroPages--;
    }
    return true;
}


/**
 * Scan for MMIO2 page modifications.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   uPass               The pass number.
 */
static void pgmR3ScanMmio2Pages(PVM pVM, uint32_t uPass)
{
    /*
     * Since this is a bit expensive we lower the scan rate after a little while.
     */
    if (    (    (uPass & 3) != 0
             &&  uPass > 10)
        ||  uPass == SSM_PASS_FINAL)
        return;

    pgmLock(pVM);                       /* paranoia */
    for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
        if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
        {
            PPGMLIVESAVEMMIO2PAGE paLSPages = pRegMmio->paLSPages;
            uint32_t              cPages    = pRegMmio->RamRange.cb >> PAGE_SHIFT;
            pgmUnlock(pVM);

            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                uint8_t const *pbPage = (uint8_t const *)pRegMmio->pvR3 + iPage * PAGE_SIZE;
                pgmR3ScanMmio2Page(pVM, pbPage, &paLSPages[iPage]);
            }

            pgmLock(pVM);
        }
    pgmUnlock(pVM);

}


/**
 * Save quiescent MMIO2 pages.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   fLiveSave           Whether it's a live save or not.
 * @param   uPass               The pass number.
 */
static int pgmR3SaveMmio2Pages(PVM pVM, PSSMHANDLE pSSM, bool fLiveSave, uint32_t uPass)
{
    /** @todo implement live saving of MMIO2 pages. (Need some way of telling the
     *        device that we wish to know about changes.) */

    int rc = VINF_SUCCESS;
    if (uPass == SSM_PASS_FINAL)
    {
        /*
         * The mop up round.
         */
        pgmLock(pVM);
        for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3;
             pRegMmio && RT_SUCCESS(rc);
             pRegMmio = pRegMmio->pNextR3)
            if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
            {
                PPGMLIVESAVEMMIO2PAGE paLSPages = pRegMmio->paLSPages;
                uint8_t const        *pbPage    = (uint8_t const *)pRegMmio->RamRange.pvR3;
                uint32_t              cPages    = pRegMmio->RamRange.cb >> PAGE_SHIFT;
                uint32_t              iPageLast = cPages;
                for (uint32_t iPage = 0; iPage < cPages; iPage++, pbPage += PAGE_SIZE)
                {
                    uint8_t u8Type;
                    if (!fLiveSave)
                        u8Type = ASMMemIsZeroPage(pbPage) ? PGM_STATE_REC_MMIO2_ZERO : PGM_STATE_REC_MMIO2_RAW;
                    else
                    {
                        /* Try figure if it's a clean page, compare the SHA-1 to be really sure. */
                        if (   !paLSPages[iPage].fDirty
                            && !pgmR3ScanMmio2Page(pVM, pbPage, &paLSPages[iPage]))
                        {
                            if (paLSPages[iPage].fZero)
                                continue;

                            uint8_t abSha1Hash[RTSHA1_HASH_SIZE];
                            RTSha1(pbPage, PAGE_SIZE, abSha1Hash);
                            if (!memcmp(abSha1Hash, paLSPages[iPage].abSha1Saved, sizeof(abSha1Hash)))
                                continue;
                        }
                        u8Type = paLSPages[iPage].fZero ? PGM_STATE_REC_MMIO2_ZERO : PGM_STATE_REC_MMIO2_RAW;
                        pVM->pgm.s.LiveSave.cSavedPages++;
                    }

                    if (iPage != 0 && iPage == iPageLast + 1)
                        rc = SSMR3PutU8(pSSM, u8Type);
                    else
                    {
                        SSMR3PutU8(pSSM, u8Type | PGM_STATE_REC_FLAG_ADDR);
                        SSMR3PutU8(pSSM, pRegMmio->idSavedState);
                        rc = SSMR3PutU32(pSSM, iPage);
                    }
                    if (u8Type == PGM_STATE_REC_MMIO2_RAW)
                        rc = SSMR3PutMem(pSSM, pbPage, PAGE_SIZE);
                    if (RT_FAILURE(rc))
                        break;
                    iPageLast = iPage;
                }
            }
        pgmUnlock(pVM);
    }
    /*
     * Reduce the rate after a little while since the current MMIO2 approach is
     * a bit expensive.
     * We position it two passes after the scan pass to avoid saving busy pages.
     */
    else if (   uPass <= 10
             || (uPass & 3) == 2)
    {
        pgmLock(pVM);
        for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3;
             pRegMmio && RT_SUCCESS(rc);
             pRegMmio = pRegMmio->pNextR3)
            if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
            {
                PPGMLIVESAVEMMIO2PAGE paLSPages = pRegMmio->paLSPages;
                uint8_t const        *pbPage    = (uint8_t const *)pRegMmio->RamRange.pvR3;
                uint32_t              cPages    = pRegMmio->RamRange.cb >> PAGE_SHIFT;
                uint32_t              iPageLast = cPages;
                pgmUnlock(pVM);

                for (uint32_t iPage = 0; iPage < cPages; iPage++, pbPage += PAGE_SIZE)
                {
                    /* Skip clean pages and pages which hasn't quiesced. */
                    if (!paLSPages[iPage].fDirty)
                        continue;
                    if (paLSPages[iPage].cUnchangedScans < 3)
                        continue;
                    if (pgmR3ScanMmio2Page(pVM, pbPage, &paLSPages[iPage]))
                        continue;

                    /* Save it. */
                    bool const fZero = paLSPages[iPage].fZero;
                    uint8_t abPage[PAGE_SIZE];
                    if (!fZero)
                    {
                        memcpy(abPage, pbPage, PAGE_SIZE);
                        RTSha1(abPage, PAGE_SIZE, paLSPages[iPage].abSha1Saved);
                    }

                    uint8_t u8Type = paLSPages[iPage].fZero ? PGM_STATE_REC_MMIO2_ZERO : PGM_STATE_REC_MMIO2_RAW;
                    if (iPage != 0 && iPage == iPageLast + 1)
                        rc = SSMR3PutU8(pSSM, u8Type);
                    else
                    {
                        SSMR3PutU8(pSSM, u8Type | PGM_STATE_REC_FLAG_ADDR);
                        SSMR3PutU8(pSSM, pRegMmio->idSavedState);
                        rc = SSMR3PutU32(pSSM, iPage);
                    }
                    if (u8Type == PGM_STATE_REC_MMIO2_RAW)
                        rc = SSMR3PutMem(pSSM, abPage, PAGE_SIZE);
                    if (RT_FAILURE(rc))
                        break;

                    /* Housekeeping. */
                    paLSPages[iPage].fDirty = false;
                    pVM->pgm.s.LiveSave.Mmio2.cDirtyPages--;
                    pVM->pgm.s.LiveSave.Mmio2.cReadyPages++;
                    if (u8Type == PGM_STATE_REC_MMIO2_ZERO)
                        pVM->pgm.s.LiveSave.Mmio2.cZeroPages++;
                    pVM->pgm.s.LiveSave.cSavedPages++;
                    iPageLast = iPage;
                }

                pgmLock(pVM);
            }
        pgmUnlock(pVM);
    }

    return rc;
}


/**
 * Cleans up MMIO2 pages after a live save.
 *
 * @param   pVM                 The cross context VM structure.
 */
static void pgmR3DoneMmio2Pages(PVM pVM)
{
    /*
     * Free the tracking structures for the MMIO2 pages.
     * We do the freeing outside the lock in case the VM is running.
     */
    pgmLock(pVM);
    for (PPGMREGMMIORANGE pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
        if (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2)
        {
            void *pvMmio2ToFree = pRegMmio->paLSPages;
            if (pvMmio2ToFree)
            {
                pRegMmio->paLSPages = NULL;
                pgmUnlock(pVM);
                MMR3HeapFree(pvMmio2ToFree);
                pgmLock(pVM);
            }
        }
    pgmUnlock(pVM);
}


/**
 * Prepares the RAM pages for a live save.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 */
static int pgmR3PrepRamPages(PVM pVM)
{

    /*
     * Try allocating tracking structures for the ram ranges.
     *
     * To avoid lock contention, we leave the lock every time we're allocating
     * a new array.  This means we'll have to ditch the allocation and start
     * all over again if the RAM range list changes in-between.
     *
     * Note! pgmR3SaveDone will always be called and it is therefore responsible
     *       for cleaning up.
     */
    PPGMRAMRANGE pCur;
    pgmLock(pVM);
    do
    {
        for (pCur = pVM->pgm.s.pRamRangesXR3; pCur; pCur = pCur->pNextR3)
        {
            if (   !pCur->paLSPages
                && !PGM_RAM_RANGE_IS_AD_HOC(pCur))
            {
                uint32_t const  idRamRangesGen = pVM->pgm.s.idRamRangesGen;
                uint32_t const  cPages = pCur->cb >> PAGE_SHIFT;
                pgmUnlock(pVM);
                PPGMLIVESAVERAMPAGE paLSPages = (PPGMLIVESAVERAMPAGE)MMR3HeapAllocZ(pVM, MM_TAG_PGM, cPages * sizeof(PGMLIVESAVERAMPAGE));
                if (!paLSPages)
                    return VERR_NO_MEMORY;
                pgmLock(pVM);
                if (pVM->pgm.s.idRamRangesGen != idRamRangesGen)
                {
                    pgmUnlock(pVM);
                    MMR3HeapFree(paLSPages);
                    pgmLock(pVM);
                    break;              /* try again */
                }
                pCur->paLSPages = paLSPages;

                /*
                 * Initialize the array.
                 */
                uint32_t iPage = cPages;
                while (iPage-- > 0)
                {
                    /** @todo yield critsect! (after moving this away from EMT0) */
                    PCPGMPAGE pPage = &pCur->aPages[iPage];
                    paLSPages[iPage].cDirtied               = 0;
                    paLSPages[iPage].fDirty                 = 1; /* everything is dirty at this time */
                    paLSPages[iPage].fWriteMonitored        = 0;
                    paLSPages[iPage].fWriteMonitoredJustNow = 0;
                    paLSPages[iPage].u2Reserved             = 0;
                    switch (PGM_PAGE_GET_TYPE(pPage))
                    {
                        case PGMPAGETYPE_RAM:
                            if (    PGM_PAGE_IS_ZERO(pPage)
                                ||  PGM_PAGE_IS_BALLOONED(pPage))
                            {
                                paLSPages[iPage].fZero   = 1;
                                paLSPages[iPage].fShared = 0;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                paLSPages[iPage].u32Crc  = PGM_STATE_CRC32_ZERO_PAGE;
#endif
                            }
                            else if (PGM_PAGE_IS_SHARED(pPage))
                            {
                                paLSPages[iPage].fZero   = 0;
                                paLSPages[iPage].fShared = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                paLSPages[iPage].u32Crc  = UINT32_MAX;
#endif
                            }
                            else
                            {
                                paLSPages[iPage].fZero   = 0;
                                paLSPages[iPage].fShared = 0;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                paLSPages[iPage].u32Crc  = UINT32_MAX;
#endif
                            }
                            paLSPages[iPage].fIgnore     = 0;
                            pVM->pgm.s.LiveSave.Ram.cDirtyPages++;
                            break;

                        case PGMPAGETYPE_ROM_SHADOW:
                        case PGMPAGETYPE_ROM:
                        {
                            paLSPages[iPage].fZero   = 0;
                            paLSPages[iPage].fShared = 0;
                            paLSPages[iPage].fDirty  = 0;
                            paLSPages[iPage].fIgnore = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                            paLSPages[iPage].u32Crc  = UINT32_MAX;
#endif
                            pVM->pgm.s.LiveSave.cIgnoredPages++;
                            break;
                        }

                        default:
                            AssertMsgFailed(("%R[pgmpage]", pPage));
                            RT_FALL_THRU();
                        case PGMPAGETYPE_MMIO2:
                        case PGMPAGETYPE_MMIO2_ALIAS_MMIO:
                            paLSPages[iPage].fZero   = 0;
                            paLSPages[iPage].fShared = 0;
                            paLSPages[iPage].fDirty  = 0;
                            paLSPages[iPage].fIgnore = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                            paLSPages[iPage].u32Crc  = UINT32_MAX;
#endif
                            pVM->pgm.s.LiveSave.cIgnoredPages++;
                            break;

                        case PGMPAGETYPE_MMIO:
                        case PGMPAGETYPE_SPECIAL_ALIAS_MMIO:
                            paLSPages[iPage].fZero   = 0;
                            paLSPages[iPage].fShared = 0;
                            paLSPages[iPage].fDirty  = 0;
                            paLSPages[iPage].fIgnore = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                            paLSPages[iPage].u32Crc  = UINT32_MAX;
#endif
                            pVM->pgm.s.LiveSave.cIgnoredPages++;
                            break;
                    }
                }
            }
        }
    } while (pCur);
    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * Saves the RAM configuration.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The saved state handle.
 */
static int pgmR3SaveRamConfig(PVM pVM, PSSMHANDLE pSSM)
{
    uint32_t cbRamHole = 0;
    int rc = CFGMR3QueryU32Def(CFGMR3GetRoot(pVM), "RamHoleSize", &cbRamHole, MM_RAM_HOLE_SIZE_DEFAULT);
    AssertRCReturn(rc, rc);

    uint64_t cbRam     = 0;
    rc = CFGMR3QueryU64Def(CFGMR3GetRoot(pVM), "RamSize", &cbRam, 0);
    AssertRCReturn(rc, rc);

    SSMR3PutU32(pSSM, cbRamHole);
    return SSMR3PutU64(pSSM, cbRam);
}


/**
 * Loads and verifies the RAM configuration.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The saved state handle.
 */
static int pgmR3LoadRamConfig(PVM pVM, PSSMHANDLE pSSM)
{
    uint32_t cbRamHoleCfg = 0;
    int rc = CFGMR3QueryU32Def(CFGMR3GetRoot(pVM), "RamHoleSize", &cbRamHoleCfg, MM_RAM_HOLE_SIZE_DEFAULT);
    AssertRCReturn(rc, rc);

    uint64_t cbRamCfg     = 0;
    rc = CFGMR3QueryU64Def(CFGMR3GetRoot(pVM), "RamSize", &cbRamCfg, 0);
    AssertRCReturn(rc, rc);

    uint32_t cbRamHoleSaved;
    SSMR3GetU32(pSSM, &cbRamHoleSaved);

    uint64_t cbRamSaved;
    rc = SSMR3GetU64(pSSM, &cbRamSaved);
    AssertRCReturn(rc, rc);

    if (   cbRamHoleCfg != cbRamHoleSaved
        || cbRamCfg     != cbRamSaved)
        return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Ram config mismatch: saved=%RX64/%RX32 config=%RX64/%RX32 (RAM/Hole)"),
                                cbRamSaved, cbRamHoleSaved, cbRamCfg, cbRamHoleCfg);
    return VINF_SUCCESS;
}

#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32

/**
 * Calculates the CRC-32 for a RAM page and updates the live save page tracking
 * info with it.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCur                The current RAM range.
 * @param   paLSPages           The current array of live save page tracking
 *                              structures.
 * @param   iPage               The page index.
 */
static void pgmR3StateCalcCrc32ForRamPage(PVM pVM, PPGMRAMRANGE pCur, PPGMLIVESAVERAMPAGE paLSPages, uint32_t iPage)
{
    RTGCPHYS        GCPhys = pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
    PGMPAGEMAPLOCK  PgMpLck;
    void const     *pvPage;
    int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, &pCur->aPages[iPage], GCPhys, &pvPage, &PgMpLck);
    if (RT_SUCCESS(rc))
    {
        paLSPages[iPage].u32Crc = RTCrc32(pvPage, PAGE_SIZE);
        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
    }
    else
        paLSPages[iPage].u32Crc = UINT32_MAX; /* Invalid */
}


/**
 * Verifies the CRC-32 for a page given it's raw bits.
 *
 * @param   pvPage              The page bits.
 * @param   pCur                The current RAM range.
 * @param   paLSPages           The current array of live save page tracking
 *                              structures.
 * @param   iPage               The page index.
 */
static void pgmR3StateVerifyCrc32ForPage(void const *pvPage, PPGMRAMRANGE pCur, PPGMLIVESAVERAMPAGE paLSPages, uint32_t iPage, const  char *pszWhere)
{
    if (paLSPages[iPage].u32Crc != UINT32_MAX)
    {
        uint32_t u32Crc = RTCrc32(pvPage, PAGE_SIZE);
        Assert(   (   !PGM_PAGE_IS_ZERO(&pCur->aPages[iPage])
                   && !PGM_PAGE_IS_BALLOONED(&pCur->aPages[iPage]))
               || u32Crc == PGM_STATE_CRC32_ZERO_PAGE);
        AssertMsg(paLSPages[iPage].u32Crc == u32Crc,
                  ("%08x != %08x for %RGp %R[pgmpage] %s\n", paLSPages[iPage].u32Crc, u32Crc,
                   pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pCur->aPages[iPage], pszWhere));
    }
}


/**
 * Verifies the CRC-32 for a RAM page.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCur                The current RAM range.
 * @param   paLSPages           The current array of live save page tracking
 *                              structures.
 * @param   iPage               The page index.
 */
static void pgmR3StateVerifyCrc32ForRamPage(PVM pVM, PPGMRAMRANGE pCur, PPGMLIVESAVERAMPAGE paLSPages, uint32_t iPage, const char *pszWhere)
{
    if (paLSPages[iPage].u32Crc != UINT32_MAX)
    {
        RTGCPHYS        GCPhys = pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
        PGMPAGEMAPLOCK  PgMpLck;
        void const     *pvPage;
        int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, &pCur->aPages[iPage], GCPhys, &pvPage, &PgMpLck);
        if (RT_SUCCESS(rc))
        {
            pgmR3StateVerifyCrc32ForPage(pvPage, pCur, paLSPages, iPage, pszWhere);
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
        }
    }
}

#endif /* PGMLIVESAVERAMPAGE_WITH_CRC32 */

/**
 * Scan for RAM page modifications and reprotect them.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   fFinalPass          Whether this is the final pass or not.
 */
static void pgmR3ScanRamPages(PVM pVM, bool fFinalPass)
{
    /*
     * The RAM.
     */
    RTGCPHYS GCPhysCur = 0;
    PPGMRAMRANGE pCur;
    pgmLock(pVM);
    do
    {
        uint32_t const  idRamRangesGen = pVM->pgm.s.idRamRangesGen;
        for (pCur = pVM->pgm.s.pRamRangesXR3; pCur; pCur = pCur->pNextR3)
        {
            if (    pCur->GCPhysLast > GCPhysCur
                && !PGM_RAM_RANGE_IS_AD_HOC(pCur))
            {
                PPGMLIVESAVERAMPAGE paLSPages = pCur->paLSPages;
                uint32_t         cPages    = pCur->cb >> PAGE_SHIFT;
                uint32_t         iPage     = GCPhysCur <= pCur->GCPhys ? 0 : (GCPhysCur - pCur->GCPhys) >> PAGE_SHIFT;
                GCPhysCur = 0;
                for (; iPage < cPages; iPage++)
                {
                    /* Do yield first. */
                    if (   !fFinalPass
#ifndef PGMLIVESAVERAMPAGE_WITH_CRC32
                        && (iPage & 0x7ff) == 0x100
#endif
                        && PDMR3CritSectYield(&pVM->pgm.s.CritSectX)
                        && pVM->pgm.s.idRamRangesGen != idRamRangesGen)
                    {
                        GCPhysCur = pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
                        break; /* restart */
                    }

                    /* Skip already ignored pages. */
                    if (paLSPages[iPage].fIgnore)
                        continue;

                    if (RT_LIKELY(PGM_PAGE_GET_TYPE(&pCur->aPages[iPage]) == PGMPAGETYPE_RAM))
                    {
                        /*
                         * A RAM page.
                         */
                        switch (PGM_PAGE_GET_STATE(&pCur->aPages[iPage]))
                        {
                            case PGM_PAGE_STATE_ALLOCATED:
                                /** @todo Optimize this: Don't always re-enable write
                                 * monitoring if the page is known to be very busy. */
                                if (PGM_PAGE_IS_WRITTEN_TO(&pCur->aPages[iPage]))
                                {
                                    AssertMsg(paLSPages[iPage].fWriteMonitored,
                                              ("%RGp %R[pgmpage]\n", pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pCur->aPages[iPage]));
                                    PGM_PAGE_CLEAR_WRITTEN_TO(pVM, &pCur->aPages[iPage]);
                                    Assert(pVM->pgm.s.cWrittenToPages > 0);
                                    pVM->pgm.s.cWrittenToPages--;
                                }
                                else
                                {
                                    AssertMsg(!paLSPages[iPage].fWriteMonitored,
                                              ("%RGp %R[pgmpage]\n", pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT), &pCur->aPages[iPage]));
                                    pVM->pgm.s.LiveSave.Ram.cMonitoredPages++;
                                }

                                if (!paLSPages[iPage].fDirty)
                                {
                                    pVM->pgm.s.LiveSave.Ram.cReadyPages--;
                                    if (paLSPages[iPage].fZero)
                                        pVM->pgm.s.LiveSave.Ram.cZeroPages--;
                                    pVM->pgm.s.LiveSave.Ram.cDirtyPages++;
                                    if (++paLSPages[iPage].cDirtied > PGMLIVSAVEPAGE_MAX_DIRTIED)
                                        paLSPages[iPage].cDirtied = PGMLIVSAVEPAGE_MAX_DIRTIED;
                                }

                                pgmPhysPageWriteMonitor(pVM, &pCur->aPages[iPage],
                                                        pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT));
                                paLSPages[iPage].fWriteMonitored        = 1;
                                paLSPages[iPage].fWriteMonitoredJustNow = 1;
                                paLSPages[iPage].fDirty                 = 1;
                                paLSPages[iPage].fZero                  = 0;
                                paLSPages[iPage].fShared                = 0;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                paLSPages[iPage].u32Crc                 = UINT32_MAX; /* invalid */
#endif
                                break;

                            case PGM_PAGE_STATE_WRITE_MONITORED:
                                Assert(paLSPages[iPage].fWriteMonitored);
                                if (PGM_PAGE_GET_WRITE_LOCKS(&pCur->aPages[iPage]) == 0)
                                {
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                    if (paLSPages[iPage].fWriteMonitoredJustNow)
                                        pgmR3StateCalcCrc32ForRamPage(pVM, pCur, paLSPages, iPage);
                                    else
                                        pgmR3StateVerifyCrc32ForRamPage(pVM, pCur, paLSPages, iPage, "scan");
#endif
                                    paLSPages[iPage].fWriteMonitoredJustNow = 0;
                                }
                                else
                                {
                                    paLSPages[iPage].fWriteMonitoredJustNow = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                    paLSPages[iPage].u32Crc                 = UINT32_MAX; /* invalid */
#endif
                                    if (!paLSPages[iPage].fDirty)
                                    {
                                        pVM->pgm.s.LiveSave.Ram.cReadyPages--;
                                        pVM->pgm.s.LiveSave.Ram.cDirtyPages++;
                                        if (++paLSPages[iPage].cDirtied > PGMLIVSAVEPAGE_MAX_DIRTIED)
                                            paLSPages[iPage].cDirtied = PGMLIVSAVEPAGE_MAX_DIRTIED;
                                    }
                                }
                                break;

                            case PGM_PAGE_STATE_ZERO:
                            case PGM_PAGE_STATE_BALLOONED:
                                if (!paLSPages[iPage].fZero)
                                {
                                    if (!paLSPages[iPage].fDirty)
                                    {
                                        paLSPages[iPage].fDirty = 1;
                                        pVM->pgm.s.LiveSave.Ram.cReadyPages--;
                                        pVM->pgm.s.LiveSave.Ram.cDirtyPages++;
                                    }
                                    paLSPages[iPage].fZero = 1;
                                    paLSPages[iPage].fShared = 0;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                    paLSPages[iPage].u32Crc = PGM_STATE_CRC32_ZERO_PAGE;
#endif
                                }
                                break;

                            case PGM_PAGE_STATE_SHARED:
                                if (!paLSPages[iPage].fShared)
                                {
                                    if (!paLSPages[iPage].fDirty)
                                    {
                                        paLSPages[iPage].fDirty = 1;
                                        pVM->pgm.s.LiveSave.Ram.cReadyPages--;
                                        if (paLSPages[iPage].fZero)
                                            pVM->pgm.s.LiveSave.Ram.cZeroPages--;
                                        pVM->pgm.s.LiveSave.Ram.cDirtyPages++;
                                    }
                                    paLSPages[iPage].fZero = 0;
                                    paLSPages[iPage].fShared = 1;
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                                    pgmR3StateCalcCrc32ForRamPage(pVM, pCur, paLSPages, iPage);
#endif
                                }
                                break;
                        }
                    }
                    else
                    {
                        /*
                         * All other types => Ignore the page.
                         */
                        Assert(!paLSPages[iPage].fIgnore); /* skipped before switch */
                        paLSPages[iPage].fIgnore = 1;
                        if (paLSPages[iPage].fWriteMonitored)
                        {
                            /** @todo this doesn't hold water when we start monitoring MMIO2 and ROM shadow
                             *        pages! */
                            if (RT_UNLIKELY(PGM_PAGE_GET_STATE(&pCur->aPages[iPage]) == PGM_PAGE_STATE_WRITE_MONITORED))
                            {
                                AssertMsgFailed(("%R[pgmpage]", &pCur->aPages[iPage])); /* shouldn't happen. */
                                PGM_PAGE_SET_STATE(pVM, &pCur->aPages[iPage], PGM_PAGE_STATE_ALLOCATED);
                                Assert(pVM->pgm.s.cMonitoredPages > 0);
                                pVM->pgm.s.cMonitoredPages--;
                            }
                            if (PGM_PAGE_IS_WRITTEN_TO(&pCur->aPages[iPage]))
                            {
                                PGM_PAGE_CLEAR_WRITTEN_TO(pVM, &pCur->aPages[iPage]);
                                Assert(pVM->pgm.s.cWrittenToPages > 0);
                                pVM->pgm.s.cWrittenToPages--;
                            }
                            pVM->pgm.s.LiveSave.Ram.cMonitoredPages--;
                        }

                        /** @todo the counting doesn't quite work out here. fix later? */
                        if (paLSPages[iPage].fDirty)
                            pVM->pgm.s.LiveSave.Ram.cDirtyPages--;
                        else
                        {
                            pVM->pgm.s.LiveSave.Ram.cReadyPages--;
                            if (paLSPages[iPage].fZero)
                                pVM->pgm.s.LiveSave.Ram.cZeroPages--;
                        }
                        pVM->pgm.s.LiveSave.cIgnoredPages++;
                    }
                } /* for each page in range */

                if (GCPhysCur != 0)
                    break; /* Yield + ramrange change */
                GCPhysCur = pCur->GCPhysLast;
            }
        } /* for each range */
    } while (pCur);
    pgmUnlock(pVM);
}


/**
 * Save quiescent RAM pages.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   fLiveSave           Whether it's a live save or not.
 * @param   uPass               The pass number.
 */
static int pgmR3SaveRamPages(PVM pVM, PSSMHANDLE pSSM, bool fLiveSave, uint32_t uPass)
{
    NOREF(fLiveSave);

    /*
     * The RAM.
     */
    RTGCPHYS GCPhysLast = NIL_RTGCPHYS;
    RTGCPHYS GCPhysCur = 0;
    PPGMRAMRANGE pCur;
    bool fFTMDeltaSaveActive = FTMIsDeltaLoadSaveActive(pVM);

    pgmLock(pVM);
    do
    {
        uint32_t const  idRamRangesGen = pVM->pgm.s.idRamRangesGen;
        for (pCur = pVM->pgm.s.pRamRangesXR3; pCur; pCur = pCur->pNextR3)
        {
            if (   pCur->GCPhysLast > GCPhysCur
                && !PGM_RAM_RANGE_IS_AD_HOC(pCur))
            {
                PPGMLIVESAVERAMPAGE paLSPages = pCur->paLSPages;
                uint32_t         cPages    = pCur->cb >> PAGE_SHIFT;
                uint32_t         iPage     = GCPhysCur <= pCur->GCPhys ? 0 : (GCPhysCur - pCur->GCPhys) >> PAGE_SHIFT;
                GCPhysCur = 0;
                for (; iPage < cPages; iPage++)
                {
                    /* Do yield first. */
                    if (   uPass != SSM_PASS_FINAL
                        && (iPage & 0x7ff) == 0x100
                        && PDMR3CritSectYield(&pVM->pgm.s.CritSectX)
                        && pVM->pgm.s.idRamRangesGen != idRamRangesGen)
                    {
                        GCPhysCur = pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
                        break; /* restart */
                    }

                    PPGMPAGE pCurPage = &pCur->aPages[iPage];

                    /*
                     * Only save pages that haven't changed since last scan and are dirty.
                     */
                    if (    uPass != SSM_PASS_FINAL
                        &&  paLSPages)
                    {
                        if (!paLSPages[iPage].fDirty)
                            continue;
                        if (paLSPages[iPage].fWriteMonitoredJustNow)
                            continue;
                        if (paLSPages[iPage].fIgnore)
                            continue;
                        if (PGM_PAGE_GET_TYPE(pCurPage) != PGMPAGETYPE_RAM) /* in case of recent remappings */
                            continue;
                        if (    PGM_PAGE_GET_STATE(pCurPage)
                            !=  (  paLSPages[iPage].fZero
                                 ? PGM_PAGE_STATE_ZERO
                                 : paLSPages[iPage].fShared
                                 ? PGM_PAGE_STATE_SHARED
                                 : PGM_PAGE_STATE_WRITE_MONITORED))
                            continue;
                        if (PGM_PAGE_GET_WRITE_LOCKS(&pCur->aPages[iPage]) > 0)
                            continue;
                    }
                    else
                    {
                        if (   paLSPages
                            && !paLSPages[iPage].fDirty
                            && !paLSPages[iPage].fIgnore)
                        {
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                            if (PGM_PAGE_GET_TYPE(pCurPage) != PGMPAGETYPE_RAM)
                                pgmR3StateVerifyCrc32ForRamPage(pVM, pCur, paLSPages, iPage, "save#1");
#endif
                            continue;
                        }
                        if (PGM_PAGE_GET_TYPE(pCurPage) != PGMPAGETYPE_RAM)
                            continue;
                    }

                    /*
                     * Do the saving outside the PGM critsect since SSM may block on I/O.
                     */
                    int         rc;
                    RTGCPHYS    GCPhys = pCur->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);
                    bool        fZero  = PGM_PAGE_IS_ZERO(pCurPage);
                    bool        fBallooned = PGM_PAGE_IS_BALLOONED(pCurPage);
                    bool        fSkipped = false;

                    if (!fZero && !fBallooned)
                    {
                        /*
                         * Copy the page and then save it outside the lock (since any
                         * SSM call may block).
                         */
                        uint8_t         abPage[PAGE_SIZE];
                        PGMPAGEMAPLOCK  PgMpLck;
                        void const     *pvPage;
                        rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pCurPage, GCPhys, &pvPage, &PgMpLck);
                        if (RT_SUCCESS(rc))
                        {
                            memcpy(abPage, pvPage, PAGE_SIZE);
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                            if (paLSPages)
                                pgmR3StateVerifyCrc32ForPage(abPage, pCur, paLSPages, iPage, "save#3");
#endif
                            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                        }
                        pgmUnlock(pVM);
                        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc GCPhys=%RGp\n", rc, GCPhys), rc);

                        /* Try save some memory when restoring. */
                        if (!ASMMemIsZeroPage(pvPage))
                        {
                            if (fFTMDeltaSaveActive)
                            {
                                if (    PGM_PAGE_IS_WRITTEN_TO(pCurPage)
                                    ||  PGM_PAGE_IS_FT_DIRTY(pCurPage))
                                {
                                    if (GCPhys == GCPhysLast + PAGE_SIZE)
                                        SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_RAW);
                                    else
                                    {
                                        SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_RAW | PGM_STATE_REC_FLAG_ADDR);
                                        SSMR3PutGCPhys(pSSM, GCPhys);
                                    }
                                    rc = SSMR3PutMem(pSSM, abPage, PAGE_SIZE);
                                    PGM_PAGE_CLEAR_WRITTEN_TO(pVM, pCurPage);
                                    PGM_PAGE_CLEAR_FT_DIRTY(pCurPage);
                                }
                                /* else nothing changed, so skip it. */
                                else
                                    fSkipped = true;
                            }
                            else
                            {
                                if (GCPhys == GCPhysLast + PAGE_SIZE)
                                    SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_RAW);
                                else
                                {
                                    SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_RAW | PGM_STATE_REC_FLAG_ADDR);
                                    SSMR3PutGCPhys(pSSM, GCPhys);
                                }
                                rc = SSMR3PutMem(pSSM, abPage, PAGE_SIZE);
                            }
                        }
                        else
                        {
                            if (GCPhys == GCPhysLast + PAGE_SIZE)
                                rc = SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_ZERO);
                            else
                            {
                                SSMR3PutU8(pSSM, PGM_STATE_REC_RAM_ZERO | PGM_STATE_REC_FLAG_ADDR);
                                rc = SSMR3PutGCPhys(pSSM, GCPhys);
                            }
                        }
                    }
                    else
                    {
                        /*
                         * Dirty zero or ballooned page.
                         */
#ifdef PGMLIVESAVERAMPAGE_WITH_CRC32
                        if (paLSPages)
                            pgmR3StateVerifyCrc32ForRamPage(pVM, pCur, paLSPages, iPage, "save#2");
#endif
                        pgmUnlock(pVM);

                        uint8_t u8RecType = fBallooned ? PGM_STATE_REC_RAM_BALLOONED : PGM_STATE_REC_RAM_ZERO;
                        if (GCPhys == GCPhysLast + PAGE_SIZE)
                            rc = SSMR3PutU8(pSSM, u8RecType);
                        else
                        {
                            SSMR3PutU8(pSSM, u8RecType | PGM_STATE_REC_FLAG_ADDR);
                            rc = SSMR3PutGCPhys(pSSM, GCPhys);
                        }
                    }
                    if (RT_FAILURE(rc))
                        return rc;

                    pgmLock(pVM);
                    if (!fSkipped)
                        GCPhysLast = GCPhys;
                    if (paLSPages)
                    {
                        paLSPages[iPage].fDirty = 0;
                        pVM->pgm.s.LiveSave.Ram.cReadyPages++;
                        if (fZero)
                            pVM->pgm.s.LiveSave.Ram.cZeroPages++;
                        pVM->pgm.s.LiveSave.Ram.cDirtyPages--;
                        pVM->pgm.s.LiveSave.cSavedPages++;
                    }
                    if (idRamRangesGen != pVM->pgm.s.idRamRangesGen)
                    {
                        GCPhysCur = GCPhys | PAGE_OFFSET_MASK;
                        break; /* restart */
                    }

                } /* for each page in range */

                if (GCPhysCur != 0)
                    break; /* Yield + ramrange change */
                GCPhysCur = pCur->GCPhysLast;
            }
        } /* for each range */
    } while (pCur);

    pgmUnlock(pVM);

    return VINF_SUCCESS;
}


/**
 * Cleans up RAM pages after a live save.
 *
 * @param   pVM                 The cross context VM structure.
 */
static void pgmR3DoneRamPages(PVM pVM)
{
    /*
     * Free the tracking arrays and disable write monitoring.
     *
     * Play nice with the PGM lock in case we're called while the VM is still
     * running.  This means we have to delay the freeing since we wish to use
     * paLSPages as an indicator of which RAM ranges which we need to scan for
     * write monitored pages.
     */
    void *pvToFree = NULL;
    PPGMRAMRANGE pCur;
    uint32_t cMonitoredPages = 0;
    pgmLock(pVM);
    do
    {
        for (pCur = pVM->pgm.s.pRamRangesXR3; pCur; pCur = pCur->pNextR3)
        {
            if (pCur->paLSPages)
            {
                if (pvToFree)
                {
                    uint32_t idRamRangesGen = pVM->pgm.s.idRamRangesGen;
                    pgmUnlock(pVM);
                    MMR3HeapFree(pvToFree);
                    pvToFree = NULL;
                    pgmLock(pVM);
                    if (idRamRangesGen != pVM->pgm.s.idRamRangesGen)
                        break;          /* start over again. */
                }

                pvToFree = pCur->paLSPages;
                pCur->paLSPages = NULL;

                uint32_t iPage = pCur->cb >> PAGE_SHIFT;
                while (iPage--)
                {
                    PPGMPAGE pPage = &pCur->aPages[iPage];
                    PGM_PAGE_CLEAR_WRITTEN_TO(pVM, pPage);
                    if (PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED)
                    {
                        PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
                        cMonitoredPages++;
                    }
                }
            }
        }
    } while (pCur);

    Assert(pVM->pgm.s.cMonitoredPages >= cMonitoredPages);
    if (pVM->pgm.s.cMonitoredPages < cMonitoredPages)
        pVM->pgm.s.cMonitoredPages = 0;
    else
        pVM->pgm.s.cMonitoredPages -= cMonitoredPages;

    pgmUnlock(pVM);

    MMR3HeapFree(pvToFree);
    pvToFree = NULL;
}


/**
 * @callback_method_impl{FNSSMINTLIVEEXEC}
 */
static DECLCALLBACK(int) pgmR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    int rc;

    /*
     * Save the MMIO2 and ROM range IDs in pass 0.
     */
    if (uPass == 0)
    {
        rc = pgmR3SaveRamConfig(pVM, pSSM);
        if (RT_FAILURE(rc))
            return rc;
        rc = pgmR3SaveRomRanges(pVM, pSSM);
        if (RT_FAILURE(rc))
            return rc;
        rc = pgmR3SaveMmio2Ranges(pVM, pSSM);
        if (RT_FAILURE(rc))
            return rc;
    }
    /*
     * Reset the page-per-second estimate to avoid inflation by the initial
     * load of zero pages.  pgmR3LiveVote ASSUMES this is done at pass 7.
     */
    else if (uPass == 7)
    {
        pVM->pgm.s.LiveSave.cSavedPages  = 0;
        pVM->pgm.s.LiveSave.uSaveStartNS = RTTimeNanoTS();
    }

    /*
     * Do the scanning.
     */
    pgmR3ScanRomPages(pVM);
    pgmR3ScanMmio2Pages(pVM, uPass);
    pgmR3ScanRamPages(pVM, false /*fFinalPass*/);
    pgmR3PoolClearAll(pVM, true /*fFlushRemTlb*/); /** @todo this could perhaps be optimized a bit. */

    /*
     * Save the pages.
     */
    if (uPass == 0)
        rc = pgmR3SaveRomVirginPages(  pVM, pSSM, true /*fLiveSave*/);
    else
        rc = VINF_SUCCESS;
    if (RT_SUCCESS(rc))
        rc = pgmR3SaveShadowedRomPages(pVM, pSSM, true /*fLiveSave*/, false /*fFinalPass*/);
    if (RT_SUCCESS(rc))
        rc = pgmR3SaveMmio2Pages(      pVM, pSSM, true /*fLiveSave*/, uPass);
    if (RT_SUCCESS(rc))
        rc = pgmR3SaveRamPages(        pVM, pSSM, true /*fLiveSave*/, uPass);
    SSMR3PutU8(pSSM, PGM_STATE_REC_END);    /* (Ignore the rc, SSM takes care of it.) */

    return rc;
}


/**
 * @callback_method_impl{FNSSMINTLIVEVOTE}
 */
static DECLCALLBACK(int)  pgmR3LiveVote(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    /*
     * Update and calculate parameters used in the decision making.
     */
    const uint32_t cHistoryEntries = RT_ELEMENTS(pVM->pgm.s.LiveSave.acDirtyPagesHistory);

    /* update history. */
    pgmLock(pVM);
    uint32_t const cWrittenToPages = pVM->pgm.s.cWrittenToPages;
    pgmUnlock(pVM);
    uint32_t const cDirtyNow = pVM->pgm.s.LiveSave.Rom.cDirtyPages
                             + pVM->pgm.s.LiveSave.Mmio2.cDirtyPages
                             + pVM->pgm.s.LiveSave.Ram.cDirtyPages
                             + cWrittenToPages;
    uint32_t i = pVM->pgm.s.LiveSave.iDirtyPagesHistory;
    pVM->pgm.s.LiveSave.acDirtyPagesHistory[i] = cDirtyNow;
    pVM->pgm.s.LiveSave.iDirtyPagesHistory = (i + 1) % cHistoryEntries;

    /* calc shortterm average (4 passes). */
    AssertCompile(RT_ELEMENTS(pVM->pgm.s.LiveSave.acDirtyPagesHistory) > 4);
    uint64_t cTotal = pVM->pgm.s.LiveSave.acDirtyPagesHistory[i];
    cTotal += pVM->pgm.s.LiveSave.acDirtyPagesHistory[(i + cHistoryEntries - 1) % cHistoryEntries];
    cTotal += pVM->pgm.s.LiveSave.acDirtyPagesHistory[(i + cHistoryEntries - 2) % cHistoryEntries];
    cTotal += pVM->pgm.s.LiveSave.acDirtyPagesHistory[(i + cHistoryEntries - 3) % cHistoryEntries];
    uint32_t const cDirtyPagesShort = cTotal / 4;
    pVM->pgm.s.LiveSave.cDirtyPagesShort = cDirtyPagesShort;

    /* calc longterm average. */
    cTotal = 0;
    if (uPass < cHistoryEntries)
        for (i = 0; i < cHistoryEntries && i <= uPass; i++)
              cTotal += pVM->pgm.s.LiveSave.acDirtyPagesHistory[i];
    else
        for (i = 0; i < cHistoryEntries; i++)
            cTotal += pVM->pgm.s.LiveSave.acDirtyPagesHistory[i];
    uint32_t const cDirtyPagesLong = cTotal / cHistoryEntries;
    pVM->pgm.s.LiveSave.cDirtyPagesLong = cDirtyPagesLong;

    /* estimate the speed */
    uint64_t cNsElapsed = RTTimeNanoTS() - pVM->pgm.s.LiveSave.uSaveStartNS;
    uint32_t cPagesPerSecond = (uint32_t)(   pVM->pgm.s.LiveSave.cSavedPages
                                          / ((long double)cNsElapsed / 1000000000.0) );
    pVM->pgm.s.LiveSave.cPagesPerSecond = cPagesPerSecond;

    /*
     * Try make a decision.
     */
    if (    cDirtyPagesShort <= cDirtyPagesLong
        &&  (   cDirtyNow    <= cDirtyPagesShort
             || cDirtyNow - cDirtyPagesShort < RT_MIN(cDirtyPagesShort / 8, 16)
            )
       )
    {
        if (uPass > 10)
        {
            uint32_t cMsLeftShort   = (uint32_t)(cDirtyPagesShort / (long double)cPagesPerSecond * 1000.0);
            uint32_t cMsLeftLong    = (uint32_t)(cDirtyPagesLong  / (long double)cPagesPerSecond * 1000.0);
            uint32_t cMsMaxDowntime = SSMR3HandleMaxDowntime(pSSM);
            if (cMsMaxDowntime < 32)
                cMsMaxDowntime = 32;
            if (   (   cMsLeftLong  <= cMsMaxDowntime
                    && cMsLeftShort <  cMsMaxDowntime)
                || cMsLeftShort < cMsMaxDowntime / 2
               )
            {
                Log(("pgmR3LiveVote: VINF_SUCCESS - pass=%d cDirtyPagesShort=%u|%ums cDirtyPagesLong=%u|%ums cMsMaxDowntime=%u\n",
                     uPass, cDirtyPagesShort, cMsLeftShort, cDirtyPagesLong, cMsLeftLong, cMsMaxDowntime));
                return VINF_SUCCESS;
            }
        }
        else
        {
            if (   (   cDirtyPagesShort <= 128
                    && cDirtyPagesLong  <= 1024)
                || cDirtyPagesLong <= 256
               )
            {
                Log(("pgmR3LiveVote: VINF_SUCCESS - pass=%d cDirtyPagesShort=%u cDirtyPagesLong=%u\n", uPass, cDirtyPagesShort, cDirtyPagesLong));
                return VINF_SUCCESS;
            }
        }
    }

    /*
     * Come up with a completion percentage.  Currently this is a simple
     * dirty page (long term) vs. total pages ratio + some pass trickery.
     */
    unsigned uPctDirty = (unsigned)(  (long double)cDirtyPagesLong
                                    / (pVM->pgm.s.cAllPages - pVM->pgm.s.LiveSave.cIgnoredPages - pVM->pgm.s.cZeroPages) );
    if (uPctDirty <= 100)
        SSMR3HandleReportLivePercent(pSSM, RT_MIN(100 - uPctDirty, uPass * 2));
    else
        AssertMsgFailed(("uPctDirty=%u cDirtyPagesLong=%#x cAllPages=%#x cIgnoredPages=%#x cZeroPages=%#x\n",
                         uPctDirty, cDirtyPagesLong, pVM->pgm.s.cAllPages, pVM->pgm.s.LiveSave.cIgnoredPages, pVM->pgm.s.cZeroPages));

    return VINF_SSM_VOTE_FOR_ANOTHER_PASS;
}


/**
 * @callback_method_impl{FNSSMINTLIVEPREP}
 *
 * This will attempt to allocate and initialize the tracking structures.  It
 * will also prepare for write monitoring of pages and initialize PGM::LiveSave.
 * pgmR3SaveDone will do the cleanups.
 */
static DECLCALLBACK(int) pgmR3LivePrep(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Indicate that we will be using the write monitoring.
     */
    pgmLock(pVM);
    /** @todo find a way of mediating this when more users are added. */
    if (pVM->pgm.s.fPhysWriteMonitoringEngaged)
    {
        pgmUnlock(pVM);
        AssertLogRelFailedReturn(VERR_PGM_WRITE_MONITOR_ENGAGED);
    }
    pVM->pgm.s.fPhysWriteMonitoringEngaged = true;
    pgmUnlock(pVM);

    /*
     * Initialize the statistics.
     */
    pVM->pgm.s.LiveSave.Rom.cReadyPages   = 0;
    pVM->pgm.s.LiveSave.Rom.cDirtyPages   = 0;
    pVM->pgm.s.LiveSave.Mmio2.cReadyPages = 0;
    pVM->pgm.s.LiveSave.Mmio2.cDirtyPages = 0;
    pVM->pgm.s.LiveSave.Ram.cReadyPages   = 0;
    pVM->pgm.s.LiveSave.Ram.cDirtyPages   = 0;
    pVM->pgm.s.LiveSave.cIgnoredPages     = 0;
    pVM->pgm.s.LiveSave.fActive           = true;
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.LiveSave.acDirtyPagesHistory); i++)
        pVM->pgm.s.LiveSave.acDirtyPagesHistory[i] = UINT32_MAX / 2;
    pVM->pgm.s.LiveSave.iDirtyPagesHistory = 0;
    pVM->pgm.s.LiveSave.cSavedPages       = 0;
    pVM->pgm.s.LiveSave.uSaveStartNS      = RTTimeNanoTS();
    pVM->pgm.s.LiveSave.cPagesPerSecond   = 8192;

    /*
     * Per page type.
     */
    int rc = pgmR3PrepRomPages(pVM);
    if (RT_SUCCESS(rc))
        rc = pgmR3PrepMmio2Pages(pVM);
    if (RT_SUCCESS(rc))
        rc = pgmR3PrepRamPages(pVM);

    NOREF(pSSM);
    return rc;
}


/**
 * @callback_method_impl{FNSSMINTSAVEEXEC}
 */
static DECLCALLBACK(int) pgmR3SaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    int     rc   = VINF_SUCCESS;
    PPGM    pPGM = &pVM->pgm.s;

    /*
     * Lock PGM and set the no-more-writes indicator.
     */
    pgmLock(pVM);
    pVM->pgm.s.fNoMorePhysWrites = true;

    /*
     * Save basic data (required / unaffected by relocation).
     */
    bool const fMappingsFixed  = pVM->pgm.s.fMappingsFixed;
    pVM->pgm.s.fMappingsFixed |= pVM->pgm.s.fMappingsFixedRestored;
    SSMR3PutStruct(pSSM, pPGM, &s_aPGMFields[0]);
    pVM->pgm.s.fMappingsFixed  = fMappingsFixed;

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
        rc = SSMR3PutStruct(pSSM, &pVM->aCpus[idCpu].pgm.s, &s_aPGMCpuFields[0]);

    /*
     * Save the (remainder of the) memory.
     */
    if (RT_SUCCESS(rc))
    {
        if (pVM->pgm.s.LiveSave.fActive)
        {
            pgmR3ScanRomPages(pVM);
            pgmR3ScanMmio2Pages(pVM, SSM_PASS_FINAL);
            pgmR3ScanRamPages(pVM, true /*fFinalPass*/);

            rc = pgmR3SaveShadowedRomPages(    pVM, pSSM, true /*fLiveSave*/, true /*fFinalPass*/);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveMmio2Pages(      pVM, pSSM, true /*fLiveSave*/, SSM_PASS_FINAL);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveRamPages(        pVM, pSSM, true /*fLiveSave*/, SSM_PASS_FINAL);
        }
        else
        {
            rc = pgmR3SaveRamConfig(pVM, pSSM);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveRomRanges(pVM, pSSM);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveMmio2Ranges(pVM, pSSM);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveRomVirginPages(  pVM, pSSM, false /*fLiveSave*/);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveShadowedRomPages(pVM, pSSM, false /*fLiveSave*/, true /*fFinalPass*/);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveMmio2Pages(      pVM, pSSM, false /*fLiveSave*/, SSM_PASS_FINAL);
            if (RT_SUCCESS(rc))
                rc = pgmR3SaveRamPages(        pVM, pSSM, false /*fLiveSave*/, SSM_PASS_FINAL);
        }
        SSMR3PutU8(pSSM, PGM_STATE_REC_END);    /* (Ignore the rc, SSM takes of it.) */
    }

    pgmUnlock(pVM);
    return rc;
}


/**
 * @callback_method_impl{FNSSMINTSAVEDONE}
 */
static DECLCALLBACK(int) pgmR3SaveDone(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Do per page type cleanups first.
     */
    if (pVM->pgm.s.LiveSave.fActive)
    {
        pgmR3DoneRomPages(pVM);
        pgmR3DoneMmio2Pages(pVM);
        pgmR3DoneRamPages(pVM);
    }

    /*
     * Clear the live save indicator and disengage write monitoring.
     */
    pgmLock(pVM);
    pVM->pgm.s.LiveSave.fActive = false;
    /** @todo this is blindly assuming that we're the only user of write
     *        monitoring. Fix this when more users are added. */
    pVM->pgm.s.fPhysWriteMonitoringEngaged = false;
    pgmUnlock(pVM);

    NOREF(pSSM);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMINTLOADPREP}
 */
static DECLCALLBACK(int) pgmR3LoadPrep(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Call the reset function to make sure all the memory is cleared.
     */
    PGMR3Reset(pVM);
    pVM->pgm.s.LiveSave.fActive = false;
    NOREF(pSSM);
    return VINF_SUCCESS;
}


/**
 * Load an ignored page.
 *
 * @returns VBox status code.
 * @param   pSSM            The saved state handle.
 */
static int pgmR3LoadPageToDevNullOld(PSSMHANDLE pSSM)
{
    uint8_t abPage[PAGE_SIZE];
    return SSMR3GetMem(pSSM, &abPage[0], sizeof(abPage));
}


/**
 * Compares a page with an old save type value.
 *
 * @returns true if equal, false if not.
 * @param   pPage           The page to compare.
 * @param   uOldType        The old type value from the saved state.
 */
DECLINLINE(bool) pgmR3CompareNewAndOldPageTypes(PPGMPAGE pPage, uint8_t uOldType)
{
    uint8_t uOldPageType;
    switch (PGM_PAGE_GET_TYPE(pPage))
    {
        case PGMPAGETYPE_INVALID:               uOldPageType = PGMPAGETYPE_OLD_INVALID; break;
        case PGMPAGETYPE_RAM:                   uOldPageType = PGMPAGETYPE_OLD_RAM; break;
        case PGMPAGETYPE_MMIO2:                 uOldPageType = PGMPAGETYPE_OLD_MMIO2; break;
        case PGMPAGETYPE_MMIO2_ALIAS_MMIO:      uOldPageType = PGMPAGETYPE_OLD_MMIO2_ALIAS_MMIO; break;
        case PGMPAGETYPE_ROM_SHADOW:            uOldPageType = PGMPAGETYPE_OLD_ROM_SHADOW; break;
        case PGMPAGETYPE_ROM:                   uOldPageType = PGMPAGETYPE_OLD_ROM; break;
        case PGMPAGETYPE_SPECIAL_ALIAS_MMIO:    RT_FALL_THRU();
        case PGMPAGETYPE_MMIO:                  uOldPageType = PGMPAGETYPE_OLD_MMIO; break;
        default:
            AssertFailed();
            uOldPageType = PGMPAGETYPE_OLD_INVALID;
            break;
    }
    return uOldPageType == uOldType;
}


/**
 * Loads a page without any bits in the saved state, i.e. making sure it's
 * really zero.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   uOldType        The page type or PGMPAGETYPE_OLD_INVALID (old saved
 *                          state).
 * @param   pPage           The guest page tracking structure.
 * @param   GCPhys          The page address.
 * @param   pRam            The ram range (logging).
 */
static int pgmR3LoadPageZeroOld(PVM pVM, uint8_t uOldType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    if (   uOldType != PGMPAGETYPE_OLD_INVALID
        && !pgmR3CompareNewAndOldPageTypes(pPage, uOldType))
        return VERR_SSM_UNEXPECTED_DATA;

    /* I think this should be sufficient. */
    if (    !PGM_PAGE_IS_ZERO(pPage)
        &&  !PGM_PAGE_IS_BALLOONED(pPage))
        return VERR_SSM_UNEXPECTED_DATA;

    NOREF(pVM);
    NOREF(GCPhys);
    NOREF(pRam);
    return VINF_SUCCESS;
}


/**
 * Loads a page from the saved state.
 *
 * @returns VBox status code.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            The SSM handle.
 * @param   uOldType        The page type or PGMPAGETYPE_OLD_INVALID (old saved
 *                          state).
 * @param   pPage           The guest page tracking structure.
 * @param   GCPhys          The page address.
 * @param   pRam            The ram range (logging).
 */
static int pgmR3LoadPageBitsOld(PVM pVM, PSSMHANDLE pSSM, uint8_t uOldType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    /*
     * Match up the type, dealing with MMIO2 aliases (dropped).
     */
    AssertLogRelMsgReturn(   uOldType == PGMPAGETYPE_INVALID
                          || pgmR3CompareNewAndOldPageTypes(pPage, uOldType)
                          /* kudge for the expanded PXE bios (r67885) - @bugref{5687}: */
                          || (   uOldType == PGMPAGETYPE_OLD_RAM
                              && GCPhys >= 0xed000
                              && GCPhys <= 0xeffff
                              && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM)
                          ,
                          ("pPage=%R[pgmpage] GCPhys=%#x %s\n", pPage, GCPhys, pRam->pszDesc),
                          VERR_SSM_UNEXPECTED_DATA);

    /*
     * Load the page.
     */
    PGMPAGEMAPLOCK PgMpLck;
    void          *pvPage;
    int rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvPage, &PgMpLck);
    if (RT_SUCCESS(rc))
    {
        rc = SSMR3GetMem(pSSM, pvPage, PAGE_SIZE);
        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
    }

    return rc;
}


/**
 * Loads a page (counter part to pgmR3SavePage).
 *
 * @returns VBox status code, fully bitched errors.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            The SSM handle.
 * @param   uOldType        The page type.
 * @param   pPage           The page.
 * @param   GCPhys          The page address.
 * @param   pRam            The RAM range (for error messages).
 */
static int pgmR3LoadPageOld(PVM pVM, PSSMHANDLE pSSM, uint8_t uOldType, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    uint8_t uState;
    int rc = SSMR3GetU8(pSSM, &uState);
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] GCPhys=%#x %s rc=%Rrc\n", pPage, GCPhys, pRam->pszDesc, rc), rc);
    if (uState == 0 /* zero */)
        rc = pgmR3LoadPageZeroOld(pVM, uOldType, pPage, GCPhys, pRam);
    else if (uState == 1)
        rc = pgmR3LoadPageBitsOld(pVM, pSSM, uOldType, pPage, GCPhys, pRam);
    else
        rc = VERR_PGM_INVALID_SAVED_PAGE_STATE;
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] uState=%d uOldType=%d GCPhys=%RGp %s rc=%Rrc\n",
                                 pPage, uState, uOldType, GCPhys, pRam->pszDesc, rc),
                            rc);
    return VINF_SUCCESS;
}


/**
 * Loads a shadowed ROM page.
 *
 * @returns VBox status code, errors are fully bitched.
 * @param   pVM             The cross context VM structure.
 * @param   pSSM            The saved state handle.
 * @param   pPage           The page.
 * @param   GCPhys          The page address.
 * @param   pRam            The RAM range (for error messages).
 */
static int pgmR3LoadShadowedRomPageOld(PVM pVM, PSSMHANDLE pSSM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPGMRAMRANGE pRam)
{
    /*
     * Load and set the protection first, then load the two pages, the first
     * one is the active the other is the passive.
     */
    PPGMROMPAGE pRomPage = pgmR3GetRomPage(pVM, GCPhys);
    AssertLogRelMsgReturn(pRomPage, ("GCPhys=%RGp %s\n", GCPhys, pRam->pszDesc), VERR_PGM_SAVED_ROM_PAGE_NOT_FOUND);

    uint8_t uProt;
    int rc = SSMR3GetU8(pSSM, &uProt);
    AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] GCPhys=%#x %s\n", pPage, GCPhys, pRam->pszDesc), rc);
    PGMROMPROT  enmProt = (PGMROMPROT)uProt;
    AssertLogRelMsgReturn(    enmProt >= PGMROMPROT_INVALID
                          &&  enmProt <  PGMROMPROT_END,
                          ("enmProt=%d pPage=%R[pgmpage] GCPhys=%#x %s\n", enmProt, pPage, GCPhys, pRam->pszDesc),
                          VERR_SSM_UNEXPECTED_DATA);

    if (pRomPage->enmProt != enmProt)
    {
        rc = PGMR3PhysRomProtect(pVM, GCPhys, PAGE_SIZE, enmProt);
        AssertLogRelRCReturn(rc, rc);
        AssertLogRelReturn(pRomPage->enmProt == enmProt, VERR_PGM_SAVED_ROM_PAGE_PROT);
    }

    PPGMPAGE pPageActive  = PGMROMPROT_IS_ROM(enmProt) ? &pRomPage->Virgin      : &pRomPage->Shadow;
    PPGMPAGE pPagePassive = PGMROMPROT_IS_ROM(enmProt) ? &pRomPage->Shadow      : &pRomPage->Virgin;
    uint8_t  u8ActiveType = PGMROMPROT_IS_ROM(enmProt) ? PGMPAGETYPE_ROM        : PGMPAGETYPE_ROM_SHADOW;
    uint8_t  u8PassiveType= PGMROMPROT_IS_ROM(enmProt) ? PGMPAGETYPE_ROM_SHADOW : PGMPAGETYPE_ROM;

    /** @todo this isn't entirely correct as long as pgmPhysGCPhys2CCPtrInternal is
     *        used down the line (will the 2nd page will be written to the first
     *        one because of a false TLB hit since the TLB is using GCPhys and
     *        doesn't check the HCPhys of the desired page). */
    rc = pgmR3LoadPageOld(pVM, pSSM, u8ActiveType, pPage, GCPhys, pRam);
    if (RT_SUCCESS(rc))
    {
        *pPageActive = *pPage;
        rc = pgmR3LoadPageOld(pVM, pSSM, u8PassiveType, pPagePassive, GCPhys, pRam);
    }
    return rc;
}

/**
 * Ram range flags and bits for older versions of the saved state.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pSSM        The SSM handle.
 * @param   uVersion    The saved state version.
 */
static int pgmR3LoadMemoryOld(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PPGM pPGM = &pVM->pgm.s;

    /*
     * Ram range flags and bits.
     */
    uint32_t i = 0;
    for (PPGMRAMRANGE pRam = pPGM->pRamRangesXR3; ; pRam = pRam->pNextR3, i++)
    {
        /* Check the sequence number / separator. */
        uint32_t u32Sep;
        int rc = SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
            return rc;
        if (u32Sep == ~0U)
            break;
        if (u32Sep != i)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
        AssertLogRelReturn(pRam, VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

        /* Get the range details. */
        RTGCPHYS GCPhys;
        SSMR3GetGCPhys(pSSM, &GCPhys);
        RTGCPHYS GCPhysLast;
        SSMR3GetGCPhys(pSSM, &GCPhysLast);
        RTGCPHYS cb;
        SSMR3GetGCPhys(pSSM, &cb);
        uint8_t     fHaveBits;
        rc = SSMR3GetU8(pSSM, &fHaveBits);
        if (RT_FAILURE(rc))
            return rc;
        if (fHaveBits & ~1)
        {
            AssertMsgFailed(("u32Sep=%#x (last)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
        size_t  cchDesc = 0;
        char    szDesc[256];
        szDesc[0] = '\0';
        if (uVersion >= PGM_SAVED_STATE_VERSION_RR_DESC)
        {
            rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
            if (RT_FAILURE(rc))
                return rc;
            /* Since we've modified the description strings in r45878, only compare
               them if the saved state is more recent. */
            if (uVersion != PGM_SAVED_STATE_VERSION_RR_DESC)
                cchDesc = strlen(szDesc);
        }

        /*
         * Match it up with the current range.
         *
         * Note there is a hack for dealing with the high BIOS mapping
         * in the old saved state format, this means we might not have
         * a 1:1 match on success.
         */
        if (    (   GCPhys     != pRam->GCPhys
                 || GCPhysLast != pRam->GCPhysLast
                 || cb         != pRam->cb
                 ||  (   cchDesc
                      && strcmp(szDesc, pRam->pszDesc)) )
                /* Hack for PDMDevHlpPhysReserve(pDevIns, 0xfff80000, 0x80000, "High ROM Region"); */
            &&  (   uVersion != PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE
                 || GCPhys     != UINT32_C(0xfff80000)
                 || GCPhysLast != UINT32_C(0xffffffff)
                 || pRam->GCPhysLast != GCPhysLast
                 || pRam->GCPhys     <  GCPhys
                 || !fHaveBits)
           )
        {
            LogRel(("Ram range: %RGp-%RGp %RGp bytes %s %s\n"
                    "State    : %RGp-%RGp %RGp bytes %s %s\n",
                    pRam->GCPhys, pRam->GCPhysLast, pRam->cb, pRam->pvR3 ? "bits" : "nobits", pRam->pszDesc,
                    GCPhys, GCPhysLast, cb, fHaveBits ? "bits" : "nobits", szDesc));
            /*
             * If we're loading a state for debugging purpose, don't make a fuss if
             * the MMIO and ROM stuff isn't 100% right, just skip the mismatches.
             */
            if (    SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT
                ||  GCPhys < 8 * _1M)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                        N_("RAM range mismatch; saved={%RGp-%RGp %RGp bytes %s %s} config={%RGp-%RGp %RGp bytes %s %s}"),
                                        GCPhys, GCPhysLast, cb, fHaveBits ? "bits" : "nobits", szDesc,
                                        pRam->GCPhys, pRam->GCPhysLast, pRam->cb, pRam->pvR3 ? "bits" : "nobits", pRam->pszDesc);

            AssertMsgFailed(("debug skipping not implemented, sorry\n"));
            continue;
        }

        uint32_t cPages = (GCPhysLast - GCPhys + 1) >> PAGE_SHIFT;
        if (uVersion >= PGM_SAVED_STATE_VERSION_RR_DESC)
        {
            /*
             * Load the pages one by one.
             */
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                PPGMPAGE        pPage      = &pRam->aPages[iPage];
                uint8_t         uOldType;
                rc = SSMR3GetU8(pSSM, &uOldType);
                AssertLogRelMsgRCReturn(rc, ("pPage=%R[pgmpage] iPage=%#x GCPhysPage=%#x %s\n", pPage, iPage, GCPhysPage, pRam->pszDesc), rc);
                if (uOldType == PGMPAGETYPE_OLD_ROM_SHADOW)
                    rc = pgmR3LoadShadowedRomPageOld(pVM, pSSM, pPage, GCPhysPage, pRam);
                else
                    rc = pgmR3LoadPageOld(pVM, pSSM, uOldType, pPage, GCPhysPage, pRam);
                AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhysPage=%#x %s\n", rc, iPage, GCPhysPage, pRam->pszDesc), rc);
            }
        }
        else
        {
            /*
             * Old format.
             */

            /* Of the page flags, pick up MMIO2 and ROM/RESERVED for the !fHaveBits case.
               The rest is generally irrelevant and wrong since the stuff have to match registrations. */
            uint32_t fFlags = 0;
            for (uint32_t iPage = 0; iPage < cPages; iPage++)
            {
                uint16_t u16Flags;
                rc = SSMR3GetU16(pSSM, &u16Flags);
                AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                fFlags |= u16Flags;
            }

            /* Load the bits */
            if (    !fHaveBits
                &&  GCPhysLast < UINT32_C(0xe0000000))
            {
                /*
                 * Dynamic chunks.
                 */
                const uint32_t cPagesInChunk = (1*1024*1024) >> PAGE_SHIFT;
                AssertLogRelMsgReturn(cPages % cPagesInChunk == 0,
                                      ("cPages=%#x cPagesInChunk=%#x GCPhys=%RGp %s\n", cPages, cPagesInChunk, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                for (uint32_t iPage = 0; iPage < cPages; /* incremented by inner loop */ )
                {
                    uint8_t fPresent;
                    rc = SSMR3GetU8(pSSM, &fPresent);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                    AssertLogRelMsgReturn(fPresent == (uint8_t)true || fPresent == (uint8_t)false,
                                          ("fPresent=%#x iPage=%#x GCPhys=%#x %s\n", fPresent, iPage, pRam->GCPhys, pRam->pszDesc),
                                          VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                    for (uint32_t iChunkPage = 0; iChunkPage < cPagesInChunk; iChunkPage++, iPage++)
                    {
                        RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                        PPGMPAGE        pPage      = &pRam->aPages[iPage];
                        if (fPresent)
                        {
                            if (   PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO
                                || PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_SPECIAL_ALIAS_MMIO)
                                rc = pgmR3LoadPageToDevNullOld(pSSM);
                            else
                                rc = pgmR3LoadPageBitsOld(pVM, pSSM, PGMPAGETYPE_INVALID, pPage, GCPhysPage, pRam);
                        }
                        else
                            rc = pgmR3LoadPageZeroOld(pVM, PGMPAGETYPE_INVALID, pPage, GCPhysPage, pRam);
                        AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhysPage=%#x %s\n", rc, iPage, GCPhysPage, pRam->pszDesc), rc);
                    }
                }
            }
            else if (pRam->pvR3)
            {
                /*
                 * MMIO2.
                 */
                AssertLogRelMsgReturn((fFlags & 0x0f) == RT_BIT(3) /*MM_RAM_FLAGS_MMIO2*/,
                                      ("fFlags=%#x GCPhys=%#x %s\n", fFlags, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                AssertLogRelMsgReturn(pRam->pvR3,
                                      ("GCPhys=%#x %s\n", pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                rc = SSMR3GetMem(pSSM, pRam->pvR3, pRam->cb);
                AssertLogRelMsgRCReturn(rc, ("GCPhys=%#x %s\n", pRam->GCPhys, pRam->pszDesc), rc);
            }
            else if (GCPhysLast < UINT32_C(0xfff80000))
            {
                /*
                 * PCI MMIO, no pages saved.
                 */
            }
            else
            {
                /*
                 * Load the 0xfff80000..0xffffffff BIOS range.
                 * It starts with X reserved pages that we have to skip over since
                 * the RAMRANGE create by the new code won't include those.
                 */
                AssertLogRelMsgReturn(   !(fFlags & RT_BIT(3) /*MM_RAM_FLAGS_MMIO2*/)
                                      && (fFlags  & RT_BIT(0) /*MM_RAM_FLAGS_RESERVED*/),
                                      ("fFlags=%#x GCPhys=%#x %s\n", fFlags, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                AssertLogRelMsgReturn(GCPhys == UINT32_C(0xfff80000),
                                      ("GCPhys=%RGp pRamRange{GCPhys=%#x %s}\n", GCPhys, pRam->GCPhys, pRam->pszDesc),
                                      VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                /* Skip wasted reserved pages before the ROM. */
                while (GCPhys < pRam->GCPhys)
                {
                    rc = pgmR3LoadPageToDevNullOld(pSSM);
                    GCPhys += PAGE_SIZE;
                }

                /* Load the bios pages. */
                cPages = pRam->cb >> PAGE_SHIFT;
                for (uint32_t iPage = 0; iPage < cPages; iPage++)
                {
                    RTGCPHYS const  GCPhysPage = ((RTGCPHYS)iPage << PAGE_SHIFT) + pRam->GCPhys;
                    PPGMPAGE        pPage      = &pRam->aPages[iPage];

                    AssertLogRelMsgReturn(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM,
                                          ("GCPhys=%RGp pPage=%R[pgmpage]\n", GCPhys, GCPhys),
                                          VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
                    rc = pgmR3LoadPageBitsOld(pVM, pSSM, PGMPAGETYPE_ROM, pPage, GCPhysPage, pRam);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc iPage=%#x GCPhys=%#x %s\n", rc, iPage, pRam->GCPhys, pRam->pszDesc), rc);
                }
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Worker for pgmR3Load and pgmR3LoadLocked.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   uVersion            The PGM saved state unit version.
 * @param   uPass               The pass number.
 *
 * @todo    This needs splitting up if more record types or code twists are
 *          added...
 */
static int pgmR3LoadMemory(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    NOREF(uPass);

    /*
     * Process page records until we hit the terminator.
     */
    RTGCPHYS            GCPhys   = NIL_RTGCPHYS;
    PPGMRAMRANGE        pRamHint = NULL;
    uint8_t             id       = UINT8_MAX;
    uint32_t            iPage    = UINT32_MAX - 10;
    PPGMROMRANGE        pRom     = NULL;
    PPGMREGMMIORANGE    pRegMmio = NULL;

    /*
     * We batch up pages that should be freed instead of calling GMM for
     * each and every one of them.  Note that we'll lose the pages in most
     * failure paths - this should probably be addressed one day.
     */
    uint32_t            cPendingPages = 0;
    PGMMFREEPAGESREQ    pReq;
    int rc = GMMR3FreePagesPrepare(pVM, &pReq, 128 /* batch size */, GMMACCOUNT_BASE);
    AssertLogRelRCReturn(rc, rc);

    for (;;)
    {
        /*
         * Get the record type and flags.
         */
        uint8_t u8;
        rc = SSMR3GetU8(pSSM, &u8);
        if (RT_FAILURE(rc))
            return rc;
        if (u8 == PGM_STATE_REC_END)
        {
            /*
             * Finish off any pages pending freeing.
             */
            if (cPendingPages)
            {
                Log(("pgmR3LoadMemory: GMMR3FreePagesPerform pVM=%p cPendingPages=%u\n", pVM, cPendingPages));
                rc = GMMR3FreePagesPerform(pVM, pReq, cPendingPages);
                AssertLogRelRCReturn(rc, rc);
            }
            GMMR3FreePagesCleanup(pReq);
            return VINF_SUCCESS;
        }
        AssertLogRelMsgReturn((u8 & ~PGM_STATE_REC_FLAG_ADDR) <= PGM_STATE_REC_LAST, ("%#x\n", u8), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
        switch (u8 & ~PGM_STATE_REC_FLAG_ADDR)
        {
            /*
             * RAM page.
             */
            case PGM_STATE_REC_RAM_ZERO:
            case PGM_STATE_REC_RAM_RAW:
            case PGM_STATE_REC_RAM_BALLOONED:
            {
                /*
                 * Get the address and resolve it into a page descriptor.
                 */
                if (!(u8 & PGM_STATE_REC_FLAG_ADDR))
                    GCPhys += PAGE_SIZE;
                else
                {
                    rc = SSMR3GetGCPhys(pSSM, &GCPhys);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                AssertLogRelMsgReturn(!(GCPhys & PAGE_OFFSET_MASK), ("%RGp\n", GCPhys), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

                PPGMPAGE pPage;
                rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pPage, &pRamHint);
                AssertLogRelMsgRCReturn(rc, ("rc=%Rrc %RGp\n", rc, GCPhys), rc);

                /*
                 * Take action according to the record type.
                 */
                switch (u8 & ~PGM_STATE_REC_FLAG_ADDR)
                {
                    case PGM_STATE_REC_RAM_ZERO:
                    {
                        if (PGM_PAGE_IS_ZERO(pPage))
                            break;

                        /* Ballooned pages must be unmarked (live snapshot and
                           teleportation scenarios). */
                        if (PGM_PAGE_IS_BALLOONED(pPage))
                        {
                            Assert(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM);
                            if (uVersion == PGM_SAVED_STATE_VERSION_BALLOON_BROKEN)
                                break;
                            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ZERO);
                            break;
                        }

                        AssertLogRelMsgReturn(PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_ALLOCATED, ("GCPhys=%RGp %R[pgmpage]\n", GCPhys, pPage), VERR_PGM_UNEXPECTED_PAGE_STATE);

                        /* If this is a ROM page, we must clear it and not try to
                         * free it.  Ditto if the VM is using RamPreAlloc (see
                         * @bugref{6318}). */
                        if (   PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM
                            || PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_ROM_SHADOW
                            || pVM->pgm.s.fRamPreAlloc)
                        {
                            PGMPAGEMAPLOCK PgMpLck;
                            void          *pvDstPage;
                            rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDstPage, &PgMpLck);
                            AssertLogRelMsgRCReturn(rc, ("GCPhys=%RGp %R[pgmpage] rc=%Rrc\n", GCPhys, pPage, rc), rc);

                            ASMMemZeroPage(pvDstPage);
                            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                        }
                        /* Free it only if it's not part of a previously
                           allocated large page (no need to clear the page). */
                        else if (   PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE
                                 && PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE_DISABLED)
                        {
                            rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPage, GCPhys);
                            AssertRCReturn(rc, rc);
                        }
                        /** @todo handle large pages (see @bugref{5545}) */
                        break;
                    }

                    case PGM_STATE_REC_RAM_BALLOONED:
                    {
                        Assert(PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM);
                        if (PGM_PAGE_IS_BALLOONED(pPage))
                            break;

                        /* We don't map ballooned pages in our shadow page tables, let's
                           just free it if allocated and mark as ballooned.  See @bugref{5515}. */
                        if (PGM_PAGE_IS_ALLOCATED(pPage))
                        {
                            /** @todo handle large pages + ballooning when it works. (see @bugref{5515},
                             *        @bugref{5545}). */
                            AssertLogRelMsgReturn(   PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE
                                                  && PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE_DISABLED,
                                                     ("GCPhys=%RGp %R[pgmpage]\n", GCPhys, pPage), VERR_PGM_LOAD_UNEXPECTED_PAGE_TYPE);

                            rc = pgmPhysFreePage(pVM, pReq, &cPendingPages, pPage, GCPhys);
                            AssertRCReturn(rc, rc);
                        }
                        Assert(PGM_PAGE_IS_ZERO(pPage));
                        PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_BALLOONED);
                        break;
                    }

                    case PGM_STATE_REC_RAM_RAW:
                    {
                        PGMPAGEMAPLOCK PgMpLck;
                        void          *pvDstPage;
                        rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDstPage, &PgMpLck);
                        AssertLogRelMsgRCReturn(rc, ("GCPhys=%RGp %R[pgmpage] rc=%Rrc\n", GCPhys, pPage, rc), rc);
                        rc = SSMR3GetMem(pSSM, pvDstPage, PAGE_SIZE);
                        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                        if (RT_FAILURE(rc))
                            return rc;
                        break;
                    }

                    default:
                        AssertMsgFailedReturn(("%#x\n", u8), VERR_PGM_SAVED_REC_TYPE);
                }
                id = UINT8_MAX;
                break;
            }

            /*
             * MMIO2 page.
             */
            case PGM_STATE_REC_MMIO2_RAW:
            case PGM_STATE_REC_MMIO2_ZERO:
            {
                /*
                 * Get the ID + page number and resolved that into a MMIO2 page.
                 */
                if (!(u8 & PGM_STATE_REC_FLAG_ADDR))
                    iPage++;
                else
                {
                    SSMR3GetU8(pSSM, &id);
                    rc = SSMR3GetU32(pSSM, &iPage);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                if (   !pRegMmio
                    || pRegMmio->idSavedState != id)
                {
                    for (pRegMmio = pVM->pgm.s.pRegMmioRangesR3; pRegMmio; pRegMmio = pRegMmio->pNextR3)
                        if (   pRegMmio->idSavedState == id
                            && (pRegMmio->fFlags & PGMREGMMIORANGE_F_MMIO2))
                            break;
                    AssertLogRelMsgReturn(pRegMmio, ("id=%#u iPage=%#x\n", id, iPage), VERR_PGM_SAVED_MMIO2_RANGE_NOT_FOUND);
                }
                AssertLogRelMsgReturn(iPage < (pRegMmio->RamRange.cb >> PAGE_SHIFT), ("iPage=%#x cb=%RGp %s\n", iPage, pRegMmio->RamRange.cb, pRegMmio->RamRange.pszDesc), VERR_PGM_SAVED_MMIO2_PAGE_NOT_FOUND);
                void *pvDstPage = (uint8_t *)pRegMmio->RamRange.pvR3 + ((size_t)iPage << PAGE_SHIFT);

                /*
                 * Load the page bits.
                 */
                if ((u8 & ~PGM_STATE_REC_FLAG_ADDR) == PGM_STATE_REC_MMIO2_ZERO)
                    ASMMemZeroPage(pvDstPage);
                else
                {
                    rc = SSMR3GetMem(pSSM, pvDstPage, PAGE_SIZE);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                GCPhys = NIL_RTGCPHYS;
                break;
            }

            /*
             * ROM pages.
             */
            case PGM_STATE_REC_ROM_VIRGIN:
            case PGM_STATE_REC_ROM_SHW_RAW:
            case PGM_STATE_REC_ROM_SHW_ZERO:
            case PGM_STATE_REC_ROM_PROT:
            {
                /*
                 * Get the ID + page number and resolved that into a ROM page descriptor.
                 */
                if (!(u8 & PGM_STATE_REC_FLAG_ADDR))
                    iPage++;
                else
                {
                    SSMR3GetU8(pSSM, &id);
                    rc = SSMR3GetU32(pSSM, &iPage);
                    if (RT_FAILURE(rc))
                        return rc;
                }
                if (    !pRom
                    ||  pRom->idSavedState != id)
                {
                    for (pRom = pVM->pgm.s.pRomRangesR3; pRom; pRom = pRom->pNextR3)
                        if (pRom->idSavedState == id)
                            break;
                    AssertLogRelMsgReturn(pRom, ("id=%#u iPage=%#x\n", id, iPage), VERR_PGM_SAVED_ROM_RANGE_NOT_FOUND);
                }
                AssertLogRelMsgReturn(iPage < (pRom->cb >> PAGE_SHIFT), ("iPage=%#x cb=%RGp %s\n", iPage, pRom->cb, pRom->pszDesc), VERR_PGM_SAVED_ROM_PAGE_NOT_FOUND);
                PPGMROMPAGE pRomPage = &pRom->aPages[iPage];
                GCPhys = pRom->GCPhys + ((RTGCPHYS)iPage << PAGE_SHIFT);

                /*
                 * Get and set the protection.
                 */
                uint8_t u8Prot;
                rc = SSMR3GetU8(pSSM, &u8Prot);
                if (RT_FAILURE(rc))
                    return rc;
                PGMROMPROT enmProt = (PGMROMPROT)u8Prot;
                AssertLogRelMsgReturn(enmProt > PGMROMPROT_INVALID && enmProt < PGMROMPROT_END, ("GCPhys=%RGp enmProt=%d\n", GCPhys, enmProt), VERR_PGM_SAVED_ROM_PAGE_PROT);

                if (enmProt != pRomPage->enmProt)
                {
                    if (RT_UNLIKELY(!(pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)))
                        return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                                N_("Protection change of unshadowed ROM page: GCPhys=%RGp enmProt=%d %s"),
                                                GCPhys, enmProt, pRom->pszDesc);
                    rc = PGMR3PhysRomProtect(pVM, GCPhys, PAGE_SIZE, enmProt);
                    AssertLogRelMsgRCReturn(rc, ("GCPhys=%RGp rc=%Rrc\n", GCPhys, rc), rc);
                    AssertLogRelReturn(pRomPage->enmProt == enmProt, VERR_PGM_SAVED_ROM_PAGE_PROT);
                }
                if ((u8 & ~PGM_STATE_REC_FLAG_ADDR) == PGM_STATE_REC_ROM_PROT)
                    break; /* done */

                /*
                 * Get the right page descriptor.
                 */
                PPGMPAGE pRealPage;
                switch (u8 & ~PGM_STATE_REC_FLAG_ADDR)
                {
                    case PGM_STATE_REC_ROM_VIRGIN:
                        if (!PGMROMPROT_IS_ROM(enmProt))
                            pRealPage = &pRomPage->Virgin;
                        else
                            pRealPage = NULL;
                        break;

                    case PGM_STATE_REC_ROM_SHW_RAW:
                    case PGM_STATE_REC_ROM_SHW_ZERO:
                        if (RT_UNLIKELY(!(pRom->fFlags & PGMPHYS_ROM_FLAGS_SHADOWED)))
                            return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                                    N_("Shadowed / non-shadowed page type mismatch: GCPhys=%RGp enmProt=%d %s"),
                                                    GCPhys, enmProt, pRom->pszDesc);
                        if (PGMROMPROT_IS_ROM(enmProt))
                            pRealPage = &pRomPage->Shadow;
                        else
                            pRealPage = NULL;
                        break;

                    default: AssertLogRelFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE); /* shut up gcc */
                }
                if (!pRealPage)
                {
                    rc = pgmPhysGetPageWithHintEx(pVM, GCPhys, &pRealPage, &pRamHint);
                    AssertLogRelMsgRCReturn(rc, ("rc=%Rrc %RGp\n", rc, GCPhys), rc);
                }

                /*
                 * Make it writable and map it (if necessary).
                 */
                void *pvDstPage = NULL;
                switch (u8 & ~PGM_STATE_REC_FLAG_ADDR)
                {
                    case PGM_STATE_REC_ROM_SHW_ZERO:
                        if (    PGM_PAGE_IS_ZERO(pRealPage)
                            ||  PGM_PAGE_IS_BALLOONED(pRealPage))
                            break;
                        /** @todo implement zero page replacing. */
                        RT_FALL_THRU();
                    case PGM_STATE_REC_ROM_VIRGIN:
                    case PGM_STATE_REC_ROM_SHW_RAW:
                    {
                        rc = pgmPhysPageMakeWritableAndMap(pVM, pRealPage, GCPhys, &pvDstPage);
                        AssertLogRelMsgRCReturn(rc, ("GCPhys=%RGp rc=%Rrc\n", GCPhys, rc), rc);
                        break;
                    }
                }

                /*
                 * Load the bits.
                 */
                switch (u8 & ~PGM_STATE_REC_FLAG_ADDR)
                {
                    case PGM_STATE_REC_ROM_SHW_ZERO:
                        if (pvDstPage)
                            ASMMemZeroPage(pvDstPage);
                        break;

                    case PGM_STATE_REC_ROM_VIRGIN:
                    case PGM_STATE_REC_ROM_SHW_RAW:
                        rc = SSMR3GetMem(pSSM, pvDstPage, PAGE_SIZE);
                        if (RT_FAILURE(rc))
                            return rc;
                        break;
                }
                GCPhys = NIL_RTGCPHYS;
                break;
            }

            /*
             * Unknown type.
             */
            default:
                AssertLogRelMsgFailedReturn(("%#x\n", u8), VERR_PGM_SAVED_REC_TYPE);
        }
    } /* forever */
}


/**
 * Worker for pgmR3Load.
 *
 * @returns VBox status code.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pSSM                The SSM handle.
 * @param   uVersion            The saved state version.
 */
static int pgmR3LoadFinalLocked(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PPGM        pPGM = &pVM->pgm.s;
    int         rc;
    uint32_t    u32Sep;

    /*
     * Load basic data (required / unaffected by relocation).
     */
    if (uVersion >= PGM_SAVED_STATE_VERSION_3_0_0)
    {
        if (uVersion > PGM_SAVED_STATE_VERSION_PRE_BALLOON)
            rc = SSMR3GetStruct(pSSM, pPGM, &s_aPGMFields[0]);
        else
            rc = SSMR3GetStruct(pSSM, pPGM, &s_aPGMFieldsPreBalloon[0]);

        AssertLogRelRCReturn(rc, rc);

        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            if (uVersion <= PGM_SAVED_STATE_VERSION_PRE_PAE)
                rc = SSMR3GetStruct(pSSM, &pVM->aCpus[i].pgm.s, &s_aPGMCpuFieldsPrePae[0]);
            else
                rc = SSMR3GetStruct(pSSM, &pVM->aCpus[i].pgm.s, &s_aPGMCpuFields[0]);
            AssertLogRelRCReturn(rc, rc);
        }
    }
    else if (uVersion >= PGM_SAVED_STATE_VERSION_RR_DESC)
    {
        AssertRelease(pVM->cCpus == 1);

        PGMOLD pgmOld;
        rc = SSMR3GetStruct(pSSM, &pgmOld, &s_aPGMFields_Old[0]);
        AssertLogRelRCReturn(rc, rc);

        pPGM->fMappingsFixed    = pgmOld.fMappingsFixed;
        pPGM->GCPtrMappingFixed = pgmOld.GCPtrMappingFixed;
        pPGM->cbMappingFixed    = pgmOld.cbMappingFixed;

        pVM->aCpus[0].pgm.s.fA20Enabled   = pgmOld.fA20Enabled;
        pVM->aCpus[0].pgm.s.GCPhysA20Mask = pgmOld.GCPhysA20Mask;
        pVM->aCpus[0].pgm.s.enmGuestMode  = pgmOld.enmGuestMode;
    }
    else
    {
        AssertRelease(pVM->cCpus == 1);

        SSMR3GetBool(pSSM,      &pPGM->fMappingsFixed);
        SSMR3GetGCPtr(pSSM,     &pPGM->GCPtrMappingFixed);
        SSMR3GetU32(pSSM,       &pPGM->cbMappingFixed);

        uint32_t cbRamSizeIgnored;
        rc = SSMR3GetU32(pSSM,  &cbRamSizeIgnored);
        if (RT_FAILURE(rc))
            return rc;
        SSMR3GetGCPhys(pSSM,    &pVM->aCpus[0].pgm.s.GCPhysA20Mask);

        uint32_t u32 = 0;
        SSMR3GetUInt(pSSM,      &u32);
        pVM->aCpus[0].pgm.s.fA20Enabled = !!u32;
        SSMR3GetUInt(pSSM,      &pVM->aCpus[0].pgm.s.fSyncFlags);
        RTUINT uGuestMode;
        SSMR3GetUInt(pSSM,      &uGuestMode);
        pVM->aCpus[0].pgm.s.enmGuestMode = (PGMMODE)uGuestMode;

        /* check separator. */
        SSMR3GetU32(pSSM, &u32Sep);
        if (RT_FAILURE(rc))
            return rc;
        if (u32Sep != (uint32_t)~0)
        {
            AssertMsgFailed(("u32Sep=%#x (first)\n", u32Sep));
            return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        }
    }

    /*
     * Fix the A20 mask.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->pgm.s.GCPhysA20Mask = ~((RTGCPHYS)!pVCpu->pgm.s.fA20Enabled << 20);
        pgmR3RefreshShadowModeAfterA20Change(pVCpu);
    }

    /*
     * The guest mappings - skipped now, see re-fixation in the caller.
     */
    if (uVersion <= PGM_SAVED_STATE_VERSION_PRE_PAE)
    {
        for (uint32_t i = 0; ; i++)
        {
            rc = SSMR3GetU32(pSSM, &u32Sep);        /* sequence number */
            if (RT_FAILURE(rc))
                return rc;
            if (u32Sep == ~0U)
                break;
            AssertMsgReturn(u32Sep == i, ("u32Sep=%#x i=%#x\n", u32Sep, i), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

            char szDesc[256];
            rc = SSMR3GetStrZ(pSSM, szDesc, sizeof(szDesc));
            if (RT_FAILURE(rc))
                return rc;
            RTGCPTR GCPtrIgnore;
            SSMR3GetGCPtr(pSSM, &GCPtrIgnore);      /* GCPtr */
            rc = SSMR3GetGCPtr(pSSM, &GCPtrIgnore); /* cPTs  */
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    /*
     * Load the RAM contents.
     */
    if (uVersion > PGM_SAVED_STATE_VERSION_3_0_0)
    {
        if (!pVM->pgm.s.LiveSave.fActive)
        {
            if (uVersion > PGM_SAVED_STATE_VERSION_NO_RAM_CFG)
            {
                rc = pgmR3LoadRamConfig(pVM, pSSM);
                if (RT_FAILURE(rc))
                    return rc;
            }
            rc = pgmR3LoadRomRanges(pVM, pSSM);
            if (RT_FAILURE(rc))
                return rc;
            rc = pgmR3LoadMmio2Ranges(pVM, pSSM);
            if (RT_FAILURE(rc))
                return rc;
        }

        rc = pgmR3LoadMemory(pVM, pSSM, uVersion, SSM_PASS_FINAL);
    }
    else
        rc = pgmR3LoadMemoryOld(pVM, pSSM, uVersion);

    /* Refresh balloon accounting. */
    if (pVM->pgm.s.cBalloonedPages)
    {
        Log(("pgmR3LoadFinalLocked: pVM=%p cBalloonedPages=%#x\n", pVM, pVM->pgm.s.cBalloonedPages));
        rc = GMMR3BalloonedPages(pVM, GMMBALLOONACTION_INFLATE, pVM->pgm.s.cBalloonedPages);
        AssertRCReturn(rc, rc);
    }
    return rc;
}


/**
 * @callback_method_impl{FNSSMINTLOADEXEC}
 */
static DECLCALLBACK(int) pgmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    /*
     * Validate version.
     */
    if (   (   uPass != SSM_PASS_FINAL
            && uVersion != PGM_SAVED_STATE_VERSION
            && uVersion != PGM_SAVED_STATE_VERSION_PRE_PAE
            && uVersion != PGM_SAVED_STATE_VERSION_BALLOON_BROKEN
            && uVersion != PGM_SAVED_STATE_VERSION_PRE_BALLOON
            && uVersion != PGM_SAVED_STATE_VERSION_NO_RAM_CFG)
        || (   uVersion != PGM_SAVED_STATE_VERSION
            && uVersion != PGM_SAVED_STATE_VERSION_PRE_PAE
            && uVersion != PGM_SAVED_STATE_VERSION_BALLOON_BROKEN
            && uVersion != PGM_SAVED_STATE_VERSION_PRE_BALLOON
            && uVersion != PGM_SAVED_STATE_VERSION_NO_RAM_CFG
            && uVersion != PGM_SAVED_STATE_VERSION_3_0_0
            && uVersion != PGM_SAVED_STATE_VERSION_2_2_2
            && uVersion != PGM_SAVED_STATE_VERSION_RR_DESC
            && uVersion != PGM_SAVED_STATE_VERSION_OLD_PHYS_CODE)
       )
    {
        AssertMsgFailed(("pgmR3Load: Invalid version uVersion=%d (current %d)!\n", uVersion, PGM_SAVED_STATE_VERSION));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Do the loading while owning the lock because a bunch of the functions
     * we're using requires this.
     */
    if (uPass != SSM_PASS_FINAL)
    {
        pgmLock(pVM);
        if (uPass != 0)
            rc = pgmR3LoadMemory(pVM, pSSM, uVersion, uPass);
        else
        {
            pVM->pgm.s.LiveSave.fActive = true;
            if (uVersion > PGM_SAVED_STATE_VERSION_NO_RAM_CFG)
                rc = pgmR3LoadRamConfig(pVM, pSSM);
            else
                rc = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
                rc = pgmR3LoadRomRanges(pVM, pSSM);
            if (RT_SUCCESS(rc))
                rc = pgmR3LoadMmio2Ranges(pVM, pSSM);
            if (RT_SUCCESS(rc))
                rc = pgmR3LoadMemory(pVM, pSSM, uVersion, uPass);
        }
        pgmUnlock(pVM);
    }
    else
    {
        pgmLock(pVM);
        rc = pgmR3LoadFinalLocked(pVM, pSSM, uVersion);
        pVM->pgm.s.LiveSave.fActive = false;
        pgmUnlock(pVM);
        if (RT_SUCCESS(rc))
        {
            /*
             * We require a full resync now.
             */
            for (VMCPUID i = 0; i < pVM->cCpus; i++)
            {
                PVMCPU pVCpu = &pVM->aCpus[i];
                VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
                VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
                pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
                /** @todo For guest PAE, we might get the wrong
                 *        aGCPhysGstPaePDs values now. We should used the
                 *        saved ones... Postponing this since it nothing new
                 *        and PAE/PDPTR needs some general readjusting, see
                 *        @bugref{5880}. */
            }

            pgmR3HandlerPhysicalUpdateAll(pVM);

            /*
             * Change the paging mode (indirectly restores PGMCPU::GCPhysCR3).
             * (Requires the CPUM state to be restored already!)
             */
            if (CPUMR3IsStateRestorePending(pVM))
                return SSMR3SetLoadError(pSSM, VERR_WRONG_ORDER, RT_SRC_POS,
                                         N_("PGM was unexpectedly restored before CPUM"));

            for (VMCPUID i = 0; i < pVM->cCpus; i++)
            {
                PVMCPU pVCpu = &pVM->aCpus[i];

                rc = PGMR3ChangeMode(pVM, pVCpu, pVCpu->pgm.s.enmGuestMode);
                AssertLogRelRCReturn(rc, rc);

                /* Update the PSE, NX flags and validity masks. */
                pVCpu->pgm.s.fGst32BitPageSizeExtension = CPUMIsGuestPageSizeExtEnabled(pVCpu);
                PGMNotifyNxeChanged(pVCpu, CPUMIsGuestNXEnabled(pVCpu));
            }

            /*
             * Try re-fixate the guest mappings.
             */
            pVM->pgm.s.fMappingsFixedRestored = false;
            if (   pVM->pgm.s.fMappingsFixed
                && pgmMapAreMappingsEnabled(pVM))
            {
#ifndef PGM_WITHOUT_MAPPINGS
                RTGCPTR     GCPtrFixed    = pVM->pgm.s.GCPtrMappingFixed;
                uint32_t    cbFixed       = pVM->pgm.s.cbMappingFixed;
                pVM->pgm.s.fMappingsFixed = false;

                uint32_t    cbRequired;
                int rc2 = PGMR3MappingsSize(pVM, &cbRequired); AssertRC(rc2);
                if (   RT_SUCCESS(rc2)
                    && cbRequired > cbFixed)
                    rc2 = VERR_OUT_OF_RANGE;
                if (RT_SUCCESS(rc2))
                    rc2 = pgmR3MappingsFixInternal(pVM, GCPtrFixed, cbFixed);
                if (RT_FAILURE(rc2))
                {
                    LogRel(("PGM: Unable to re-fixate the guest mappings at %RGv-%RGv: rc=%Rrc (cbRequired=%#x)\n",
                            GCPtrFixed, GCPtrFixed + cbFixed, rc2, cbRequired));
                    pVM->pgm.s.fMappingsFixed         = false;
                    pVM->pgm.s.fMappingsFixedRestored = true;
                    pVM->pgm.s.GCPtrMappingFixed      = GCPtrFixed;
                    pVM->pgm.s.cbMappingFixed         = cbFixed;
                }
#else
                AssertFailed();
#endif
            }
            else
            {
                /* We used to set fixed + disabled while we only use disabled now,
                   so wipe the state to avoid any confusion. */
                pVM->pgm.s.fMappingsFixed    = false;
                pVM->pgm.s.GCPtrMappingFixed = NIL_RTGCPTR;
                pVM->pgm.s.cbMappingFixed    = 0;
            }

            /*
             * If we have floating mappings, do a CR3 sync now to make sure the HMA
             * doesn't conflict with guest code / data and thereby cause trouble
             * when restoring other components like PATM.
             */
            if (pgmMapAreMappingsFloating(pVM))
            {
                PVMCPU pVCpu = &pVM->aCpus[0];
                rc = PGMSyncCR3(pVCpu, CPUMGetGuestCR0(pVCpu), CPUMGetGuestCR3(pVCpu),  CPUMGetGuestCR4(pVCpu), true);
                if (RT_FAILURE(rc))
                    return SSMR3SetLoadError(pSSM, VERR_WRONG_ORDER, RT_SRC_POS,
                                             N_("PGMSyncCR3 failed unexpectedly with rc=%Rrc"), rc);

                /* Make sure to re-sync before executing code. */
                VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL);
                VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
                pVCpu->pgm.s.fSyncFlags |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
            }
        }
    }

    return rc;
}


/**
 * @callback_method_impl{FNSSMINTLOADDONE}
 */
static DECLCALLBACK(int) pgmR3LoadDone(PVM pVM, PSSMHANDLE pSSM)
{
    pVM->pgm.s.fRestoreRomPagesOnReset = true;
    NOREF(pSSM);
    return VINF_SUCCESS;
}


/**
 * Registers the saved state callbacks with SSM.
 *
 * @returns VBox status code.
 * @param   pVM     The cross context VM structure.
 * @param   cbRam   The RAM size.
 */
int pgmR3InitSavedState(PVM pVM, uint64_t cbRam)
{
    return SSMR3RegisterInternal(pVM, "pgm", 1, PGM_SAVED_STATE_VERSION, (size_t)cbRam + sizeof(PGM),
                                 pgmR3LivePrep, pgmR3LiveExec, pgmR3LiveVote,
                                 NULL,          pgmR3SaveExec, pgmR3SaveDone,
                                 pgmR3LoadPrep, pgmR3Load,     pgmR3LoadDone);
}

