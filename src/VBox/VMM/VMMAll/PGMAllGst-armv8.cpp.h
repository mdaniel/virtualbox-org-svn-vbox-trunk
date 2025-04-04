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
    pWalk->fEffective = X86_PTE_P | X86_PTE_RW | X86_PTE_US; /** @todo */
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
    pWalk->fEffective   = X86_PTE_P | X86_PTE_RW | X86_PTE_US | X86_PTE_A | X86_PTE_D; /** @todo */
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
    uint8_t const bEl = CPUMGetGuestEL(pVCpu);


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
            if (uPt & RT_BIT_64(0)) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 0);

            if (uPt & RT_BIT_64(1)) { /* probable */ }
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

        if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 2)
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
        if (uPt & RT_BIT_64(0)) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 3);

        if (uPt & RT_BIT_64(1)) { /* probable */ }
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
    uint8_t const bEl = CPUMGetGuestEL(pVCpu);

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
            if (uPt & RT_BIT_64(0)) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 0);

            if (uPt & RT_BIT_64(1)) { /* probable */ }
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
            if (uPt & RT_BIT_64(0)) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 1);

            if (uPt & RT_BIT_64(1)) { /* probable */ }
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
            if (uPt & RT_BIT_64(0)) { /* probable */ }
            else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 2);

            if (uPt & RT_BIT_64(1)) { /* probable */ }
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
        if (uPt & RT_BIT_64(0)) { /* probable */ }
        else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, 3);

        if (uPt & RT_BIT_64(1)) { /* probable */ }
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
