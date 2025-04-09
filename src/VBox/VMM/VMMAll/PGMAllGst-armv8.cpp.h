/* $Id$ */
/** @file
 * PGM - Page Manager, ARMv8 Guest Paging Template - All context code.
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


/*
 *
 * Mode criteria:
 *      - MMU enabled/disabled.
 *      - TCR_EL1.TG0 (granule size for TTBR0_EL1).
 *      - TCR_EL1.TG1 (granule size for TTBR1_EL1).
 *      - TCR_EL1.T0SZ (address space size for TTBR0_EL1).
 *      - TCR_EL1.T1SZ (address space size for TTBR1_EL1).
 *      - TCR_EL1.IPS (intermediate physical address size).
 *      - TCR_EL1.TBI0 (ignore top address byte for TTBR0_EL1).
 *      - TCR_EL1.TBI1 (ignore top address byte for TTBR1_EL1).
 *      - TCR_EL1.HPD0 (hierarchical permisson disables for TTBR0_EL1).
 *      - TCR_EL1.HPD1 (hierarchical permisson disables for TTBR1_EL1).
 *      - More ?
 *
 * Other relevant modifiers:
 *      - TCR_EL1.HA - hardware access bit.
 *      - TCR_EL1.HD - hardware dirty bit.
 *      - ++
 *
 * Each privilege EL (1,2,3) has their own TCR_ELx and TTBR[01]_ELx registers,
 * so they should all have their own separate modes.  To make it simpler,
 * why not do a separate mode for TTBR0_ELx and one for TTBR1_ELx.  Top-level
 * functions determins which of the roots to use and call template (C++)
 * functions that takes it from there.  Using the preprocessor function template
 * approach is _not_ desirable here.
 *
 */


/*
 * Common helpers.
 * Common helpers.
 * Common helpers.
 */

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


DECLINLINE(int) pgmGstWalkFastReturnNotPresent(PVMCPUCC pVCpu, PPGMPTWALKFAST pWalk, uint8_t uLevel)
{
    RT_NOREF(pVCpu);
    pWalk->fFailed = PGM_WALKFAIL_NOT_PRESENT           | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_PAGE_TABLE_NOT_PRESENT;
}


DECLINLINE(int) pgmGstWalkFastReturnBadPhysAddr(PVMCPUCC pVCpu, PPGMPTWALKFAST pWalk, uint8_t uLevel, int rc)
{
    AssertMsg(rc == VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS, ("%Rrc\n", rc)); RT_NOREF(pVCpu, rc);
    pWalk->fFailed = PGM_WALKFAIL_BAD_PHYSICAL_ADDRESS  | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


DECLINLINE(int) pgmGstWalkFastReturnRsvdError(PVMCPUCC pVCpu, PPGMPTWALKFAST pWalk, uint8_t uLevel)
{
    RT_NOREF(pVCpu);
    pWalk->fFailed = PGM_WALKFAIL_RESERVED_BITS         | ((uint32_t)uLevel << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_RESERVED_PAGE_TABLE_BITS;
}


/*
 * Special no paging variant.
 * Special no paging variant.
 * Special no paging variant.
 */

static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneGetPage)(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk)
{
    RT_NOREF(pVCpu);

    RT_ZERO(*pWalk);
    pWalk->fSucceeded = true;
    pWalk->GCPtr      = GCPtr;
    pWalk->GCPhys     = GCPtr & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK;
    pWalk->fEffective =   PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_PX_MASK | PGM_PTATTRS_PGCS_MASK
                        | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK | PGM_PTATTRS_UX_MASK | PGM_PTATTRS_UGCS_MASK;
    return VINF_SUCCESS;
}


static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneQueryPageFast)(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags, PPGMPTWALKFAST pWalk)
{
    RT_NOREF(pVCpu, fFlags);

    pWalk->GCPtr        = GCPtr;
    pWalk->GCPhys       = GCPtr;
    pWalk->GCPhysNested = 0;
    pWalk->fInfo        = PGM_WALKINFO_SUCCEEDED;
    pWalk->fFailed      = PGM_WALKFAIL_SUCCESS;
    pWalk->fEffective =   PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_PX_MASK | PGM_PTATTRS_PGCS_MASK
                        | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK | PGM_PTATTRS_UX_MASK | PGM_PTATTRS_UGCS_MASK;
    return VINF_SUCCESS;
}


static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneModifyPage)(PVMCPUCC pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    /* Ignore. */
    RT_NOREF(pVCpu, GCPtr, cb, fFlags, fMask);
    return VINF_SUCCESS;
}


static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneWalk)(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk, PPGMPTWALKGST pGstWalk)
{
    RT_NOREF(pVCpu, GCPtr, pWalk);
    pGstWalk->enmType = PGMPTWALKGSTTYPE_INVALID;
    return VERR_PGM_NOT_USED_IN_MODE;
}


static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneEnter)(PVMCPUCC pVCpu)
{
    /* Nothing to do. */
    RT_NOREF(pVCpu);
    return VINF_SUCCESS;
}


static PGM_CTX_DECL(int) PGM_CTX(pgm,GstNoneExit)(PVMCPUCC pVCpu)
{
    /* Nothing to do. */
    RT_NOREF(pVCpu);
    return VINF_SUCCESS;
}


/*
 * Template variants for actual paging modes.
 * Template variants for actual paging modes.
 * Template variants for actual paging modes.
 */
template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
DECL_FORCE_INLINE(int) pgmGstWalkWorker(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk, PPGMPTWALKGST pGstWalk)
{
    RT_NOREF(pGstWalk); /** @todo */

    /*
     * Initial lookup level 3 is not valid and only instantiated because we need two
     * bits for the lookup level when creating the index and have to fill the slots.
     */
    if RT_CONSTEXPR_IF(a_InitialLookupLvl == 3)
    {
        AssertReleaseFailed();
        return VERR_PGM_MODE_IPE;
    }
    else
    {
        uint8_t const bEl = CPUMGetGuestEL(pVCpu);

        uint64_t fLookupMask;
        if RT_CONSTEXPR_IF(a_fTtbr0 == true)
            fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr0[bEl];
        else
            fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr1[bEl];

        RTGCPHYS GCPhysPt = CPUMGetEffectiveTtbr(pVCpu, GCPtr);
        uint64_t *pu64Pt = NULL;
        uint64_t uPt;
        int rc;
        if RT_CONSTEXPR_IF(a_InitialLookupLvl == 0)
        {
            rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 0, rc);

            uPt = pu64Pt[(GCPtr >> 39) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 0);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
            else return pgmGstWalkReturnRsvdError(pVCpu, pWalk, 0); /** @todo Only supported if TCR_EL1.DS is set. */

            /* All nine bits from now on. */
            fLookupMask = RT_BIT_64(9) - 1;
            GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
        }

        if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 1)
        {
            rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 1, rc);

            uPt = pu64Pt[(GCPtr >> 30) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 1);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
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

        if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 2)
        {
            rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 2, rc);

            uPt = pu64Pt[(GCPtr >> 21) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 2);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
            else
            {
                /* Block descriptor (2M page). */
                pWalk->GCPtr      = GCPtr;
                pWalk->fSucceeded = true;
                pWalk->GCPhys     = (RTGCPHYS)(uPt & UINT64_C(0xffffffe00000)) | (GCPtr & (RTGCPTR)(_2M - 1));
                pWalk->fBigPage   = true;
                return VINF_SUCCESS;
            }

            /* All nine bits from now on. */
            fLookupMask = RT_BIT_64(9) - 1;
            GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
        }

        AssertCompile(a_InitialLookupLvl <= 3);

        /* Next level. */
        rc = PGM_GCPHYS_2_PTR_BY_VMCPU(pVCpu, GCPhysPt, &pu64Pt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkReturnBadPhysAddr(pVCpu, pWalk, 3, rc);

        uPt = pu64Pt[(GCPtr & UINT64_C(0x1ff000)) >> 12];
        if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 3);

        if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
        else return pgmGstWalkReturnRsvdError(pVCpu, pWalk, 3); /** No block descriptors. */

        pWalk->GCPtr      = GCPtr;
        pWalk->fSucceeded = true;
        pWalk->GCPhys     = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000)) | (GCPtr & (RTGCPTR)(_4K - 1));
        return VINF_SUCCESS;
    }
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstGetPage)(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk)
{
    return pgmGstWalkWorker<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>(pVCpu, GCPtr, pWalk, NULL /*pGstWalk*/);
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstQueryPageFast)(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags, PPGMPTWALKFAST pWalk)
{
    RT_NOREF(fFlags); /** @todo */

    /*
     * Initial lookup level 3 is not valid and only instantiated because we need two
     * bits for the lookup level when creating the index and have to fill the slots.
     */
    if RT_CONSTEXPR_IF(a_InitialLookupLvl == 3)
    {
        AssertReleaseFailed();
        return VERR_PGM_MODE_IPE;
    }
    else
    {
        uint8_t const bEl = CPUMGetGuestEL(pVCpu);

        uint64_t fLookupMask;
        if RT_CONSTEXPR_IF(a_fTtbr0 == true)
            fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr0[bEl];
        else
            fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr1[bEl];

        RTGCPHYS GCPhysPt = CPUMGetEffectiveTtbr(pVCpu, GCPtr);
        uint64_t *pu64Pt = NULL;
        uint64_t uPt;
        int rc;
        if RT_CONSTEXPR_IF(a_InitialLookupLvl == 0)
        {
            rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, 0, rc);

            uPt = pu64Pt[(GCPtr >> 39) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 0);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
            else return pgmGstWalkFastReturnRsvdError(pVCpu, pWalk, 0); /** @todo Only supported if TCR_EL1.DS is set. */

            /* All nine bits from now on. */
            fLookupMask = RT_BIT_64(9) - 1;
            GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
        }

        if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 1)
        {
            rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, 1, rc);

            uPt = pu64Pt[(GCPtr >> 30) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 1);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
            else
            {
                /* Block descriptor (1G page). */
                pWalk->GCPtr       = GCPtr;
                pWalk->fInfo       = PGM_WALKINFO_SUCCEEDED | PGM_WALKINFO_GIGANTIC_PAGE;
                pWalk->GCPhys      = (RTGCPHYS)(uPt & UINT64_C(0xffffc0000000)) | (GCPtr & (RTGCPTR)(_1G - 1));
                return VINF_SUCCESS;
            }

            /* All nine bits from now on. */
            fLookupMask = RT_BIT_64(9) - 1;
            GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
        }

        if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 2)
        {
            rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&pu64Pt);
            if (RT_SUCCESS(rc)) { /* probable */ }
            else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, 2, rc);

            uPt = pu64Pt[(GCPtr >> 21) & fLookupMask];
            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 2);

            if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
            else
            {
                /* Block descriptor (2M page). */
                pWalk->GCPtr      = GCPtr;
                pWalk->fInfo      = PGM_WALKINFO_SUCCEEDED | PGM_WALKINFO_BIG_PAGE;
                pWalk->GCPhys     = (RTGCPHYS)(uPt & UINT64_C(0xffffffe00000)) | (GCPtr & (RTGCPTR)(_2M - 1));
                return VINF_SUCCESS;
            }

            /* All nine bits from now on. */
            fLookupMask = RT_BIT_64(9) - 1;
            GCPhysPt = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000));
        }

        AssertCompile(a_InitialLookupLvl <= 3);

        /* Next level. */
        rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&pu64Pt);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, 3, rc);

        uPt = pu64Pt[(GCPtr & UINT64_C(0x1ff000)) >> 12];
        if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_VALID) { /* probable */ }
        else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 3);

        if (uPt & ARMV8_VMSA64_TBL_ENTRY_F_TBL_OR_PG) { /* probable */ }
        else return pgmGstWalkFastReturnRsvdError(pVCpu, pWalk, 3); /** No block descriptors. */

        pWalk->GCPtr  = GCPtr;
        pWalk->fInfo  = PGM_WALKINFO_SUCCEEDED;
        pWalk->GCPhys = (RTGCPHYS)(uPt & UINT64_C(0xfffffffff000)) | (GCPtr & (RTGCPTR)(_4K - 1));
        return VINF_SUCCESS;
    }
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstModifyPage)(PVMCPUCC pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    /** @todo Ignore for now. */
    RT_NOREF(pVCpu, GCPtr, cb, fFlags, fMask);
    return VINF_SUCCESS;
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstWalk)(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk, PPGMPTWALKGST pGstWalk)
{
    pGstWalk->enmType = PGMPTWALKGSTTYPE_INVALID;
    return pgmGstWalkWorker<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>(pVCpu, GCPtr, pWalk, pGstWalk);
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstEnter)(PVMCPUCC pVCpu)
{
    /* Nothing to do for now. */
    RT_NOREF(pVCpu);
    return VINF_SUCCESS;
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstExit)(PVMCPUCC pVCpu)
{
    /* Nothing to do for now. */
    RT_NOREF(pVCpu);
    return VINF_SUCCESS;
}


/**
 * Guest mode data array.
 */
PGMMODEDATAGST const g_aPgmGuestModeData[PGM_GUEST_MODE_DATA_ARRAY_SIZE] =
{
    { UINT32_MAX, NULL, NULL, NULL, NULL, NULL }, /* 0 */
    {
        PGM_TYPE_NONE,
        PGM_CTX(pgm,GstNoneGetPage),
        PGM_CTX(pgm,GstNoneQueryPageFast),
        PGM_CTX(pgm,GstNoneModifyPage),
        PGM_CTX(pgm,GstNoneWalk),
        PGM_CTX(pgm,GstNoneEnter),
        PGM_CTX(pgm,GstNoneExit),
    },

#define PGM_MODE_TYPE_CREATE(a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd) \
        (2 + (  (a_fEpd ? RT_BIT_32(6) : 0)     \
              | (a_fTbi ? RT_BIT_32(5) : 0)     \
              | (a_GranuleSz << 3)              \
              | (a_InitialLookupLvl << 1)       \
              | (a_fTtbr0 ? RT_BIT_32(0) : 0) ))

#define PGM_MODE_CREATE_EX(a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd) \
    { \
        PGM_MODE_TYPE_CREATE(a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd), \
        PGM_CTX(pgm,GstGetPage)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>, \
        PGM_CTX(pgm,GstQueryPageFast)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>, \
        PGM_CTX(pgm,GstModifyPage)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>, \
        PGM_CTX(pgm,GstWalk)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>, \
        PGM_CTX(pgm,GstEnter)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd>, \
        PGM_CTX(pgm,GstExit)<a_fTtbr0, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd> \
    }

#define PGM_MODE_CREATE_TTBR(a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd) \
    PGM_MODE_CREATE_EX(false, a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd), \
    PGM_MODE_CREATE_EX(true,  a_InitialLookupLvl, a_GranuleSz, a_fTbi, a_fEpd)

#define PGM_MODE_CREATE_LOOKUP_LVL(a_GranuleSz, a_fTbi, a_fEpd) \
    PGM_MODE_CREATE_TTBR(0,  a_GranuleSz, a_fTbi, a_fEpd ), \
    PGM_MODE_CREATE_TTBR(1,  a_GranuleSz, a_fTbi, a_fEpd ), \
    PGM_MODE_CREATE_TTBR(2,  a_GranuleSz, a_fTbi, a_fEpd ), \
    PGM_MODE_CREATE_TTBR(3,  a_GranuleSz, a_fTbi, a_fEpd ) /* Invalid */

#define PGM_MODE_CREATE_GRANULE_SZ(a_fTbi, a_fEpd) \
    PGM_MODE_CREATE_LOOKUP_LVL(ARMV8_TCR_EL1_AARCH64_TG1_INVALID, a_fTbi, a_fEpd), \
    PGM_MODE_CREATE_LOOKUP_LVL(ARMV8_TCR_EL1_AARCH64_TG1_16KB,    a_fTbi, a_fEpd), \
    PGM_MODE_CREATE_LOOKUP_LVL(ARMV8_TCR_EL1_AARCH64_TG1_4KB,     a_fTbi, a_fEpd), \
    PGM_MODE_CREATE_LOOKUP_LVL(ARMV8_TCR_EL1_AARCH64_TG1_64KB,    a_fTbi, a_fEpd)

#define PGM_MODE_CREATE_TBI(a_fEpd) \
    PGM_MODE_CREATE_GRANULE_SZ(false, a_fEpd), \
    PGM_MODE_CREATE_GRANULE_SZ(true,  a_fEpd)

    /* Recursive expansion for the win, this will blow up to 128 entries covering all possible modes. */
    PGM_MODE_CREATE_TBI(false),
    PGM_MODE_CREATE_TBI(true)

#undef PGM_MODE_CREATE_TBI
#undef PGM_MODE_CREATE_GRANULE_SZ
#undef PGM_MODE_CREATE_LOOKUP_LVL
#undef PGM_MODE_CREATE_TTBR
#undef PGM_MODE_CREATE_EX
};


template<uint8_t a_offTsz, uint8_t a_offTg, uint8_t a_offTbi, uint8_t a_offEpd, bool a_fTtbr0>
DECLINLINE(uintptr_t) pgmR3DeduceTypeFromTcr(uint64_t u64RegSctlr, uint64_t u64RegTcr, uint64_t *pfInitialLookupMask)
{
    uintptr_t idxNewGst = 0;

    /*
     * MMU enabled at all?
     * Technically this is incorrect as we use ARMV8_SCTLR_EL1_M regardless of the EL but the bit is the same
     * for all exception levels.
     */
    if (u64RegSctlr & ARMV8_SCTLR_EL1_M)
    {
        uint64_t const u64Tsz = (u64RegTcr >> a_offTsz) & 0x1f;
        uint64_t const u64Tg  = (u64RegTcr >> a_offTg)  & 0x3;
        bool     const fTbi   = RT_BOOL(u64RegTcr & RT_BIT_64(a_offTbi));
        bool     const fEpd   = RT_BOOL(u64RegTcr & RT_BIT_64(a_offEpd));

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
        uint64_t uLookupLvl;
        if (/*u64Tsz >= 16 &&*/ u64Tsz <= 24)
        {
            uLookupLvl = 0;
            if (u64Tsz >= 16)
                *pfInitialLookupMask = RT_BIT_64(24 - u64Tsz + 1) - 1;
            else
                *pfInitialLookupMask = RT_BIT_64(24 - 16 + 1) - 1;
        }
        else if (u64Tsz >= 25 && u64Tsz <= 33)
        {
            uLookupLvl = 1;
            *pfInitialLookupMask = RT_BIT_64(33 - u64Tsz + 1) - 1;
        }
        else /*if (u64Tsz >= 34 && u64Tsz <= 39)*/
        {
            uLookupLvl = 2;
            if (u64Tsz <= 39)
                *pfInitialLookupMask = RT_BIT_64(39 - u64Tsz + 1) - 1;
            else
                *pfInitialLookupMask = RT_BIT_64(39 - 39 + 1) - 1;
        }

        /* Build the index into the PGM mode callback table for the given config. */
        idxNewGst = PGM_MODE_TYPE_CREATE(a_fTtbr0, uLookupLvl, u64Tg, fTbi, fEpd);
    }
    else
        idxNewGst = PGM_TYPE_NONE;

    return idxNewGst;
}
