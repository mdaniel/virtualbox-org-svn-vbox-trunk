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
            if (uPt & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 0);

            if (uPt & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
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
            if (uPt & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 1);

            if (uPt & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
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
            if (uPt & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
            else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 2);

            if (uPt & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
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
        if (uPt & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
        else return pgmGstWalkReturnNotPresent(pVCpu, pWalk, 3);

        if (uPt & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
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


static const PGMWALKFAIL g_aPermPrivRead[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    1     1  */ PGM_WALKFAIL_SUCCESS
};


static const PGMWALKFAIL g_aPermPrivWrite[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   0    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   1    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     0  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   1    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     0  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   0    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   1    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    1     0  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   1    1     1  */ PGM_WALKFAIL_NOT_WRITABLE
};


static const PGMWALKFAIL g_aPermPrivExec[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     0  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   0   1    0     1  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   0   1    1     0  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   0   1    1     1  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    0     0  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    0     1  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    1     0  */ PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    1     1  */ PGM_WALKFAIL_NOT_EXECUTABLE
};


static const PGMWALKFAIL g_aPermUnprivRead[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   1    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   1    1     1  */ PGM_WALKFAIL_SUCCESS
};


static const PGMWALKFAIL g_aPermUnprivWrite[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   0    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   1    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   0   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_WRITABLE,
    /*   0   1    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   0    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   0    1     1  */ PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   1    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE,
    /*   1   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   1    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_WRITABLE,
    /*   1   1    1     1  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE
};


static const PGMWALKFAIL g_aPermUnprivExec[] =
{
    /* UXN PXN AP[2] AP[1] */
    /*   0   0    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   0    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    0     1  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     0  */ PGM_WALKFAIL_SUCCESS,
    /*   0   1    1     1  */ PGM_WALKFAIL_SUCCESS,
    /*   1   0    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   0    0     1  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   0    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   0    1     1  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    0     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    0     1  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    1     0  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE,
    /*   1   1    1     1  */ PGM_WALKFAIL_NOT_ACCESSIBLE_BY_MODE | PGM_WALKFAIL_NOT_EXECUTABLE
};


DECL_FORCE_INLINE(int) pgmGstQueryPageCheckPermissions(PPGMPTWALKFAST pWalk, ARMV8VMSA64DESC Desc, uint32_t fFlags, uint8_t uLvl)
{
    Assert(!(fFlags & ~PGMQPAGE_F_VALID_MASK));

    /* Descriptor flags to page table attribute flags mapping. */
    static const PGMPTATTRS s_aEffective[] =
    {
        /* UXN PXN AP[2] AP[1] */
        /*   0   0    0     0  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK |                                             PGM_PTATTRS_PX_MASK | PGM_PTATTRS_UX_MASK,
        /*   0   0    0     1  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK | PGM_PTATTRS_PX_MASK | PGM_PTATTRS_UX_MASK,
        /*   0   0    1     0  */ PGM_PTATTRS_PR_MASK |                                                                   PGM_PTATTRS_PX_MASK | PGM_PTATTRS_UX_MASK,
        /*   0   0    1     1  */ PGM_PTATTRS_PR_MASK |                       PGM_PTATTRS_UR_MASK |                       PGM_PTATTRS_PX_MASK | PGM_PTATTRS_UX_MASK,

        /*   0   1    0     0  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK |                                                                   PGM_PTATTRS_UX_MASK,
        /*   0   1    0     1  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK |                       PGM_PTATTRS_UX_MASK,
        /*   0   1    1     0  */ PGM_PTATTRS_PR_MASK |                                                                                         PGM_PTATTRS_UX_MASK,
        /*   0   1    1     1  */ PGM_PTATTRS_PR_MASK |                       PGM_PTATTRS_UR_MASK |                                             PGM_PTATTRS_UX_MASK,

        /*   1   0    0     0  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK |                                             PGM_PTATTRS_PX_MASK,
        /*   1   0    0     1  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK | PGM_PTATTRS_PX_MASK,
        /*   1   0    1     0  */ PGM_PTATTRS_PR_MASK |                                                                   PGM_PTATTRS_PX_MASK,
        /*   1   0    1     1  */ PGM_PTATTRS_PR_MASK |                       PGM_PTATTRS_UR_MASK |                       PGM_PTATTRS_PX_MASK,

        /*   1   1    0     0  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK,
        /*   1   1    0     1  */ PGM_PTATTRS_PR_MASK | PGM_PTATTRS_PW_MASK | PGM_PTATTRS_UR_MASK | PGM_PTATTRS_UW_MASK ,
        /*   1   1    1     0  */ PGM_PTATTRS_PR_MASK,
        /*   1   1    1     1  */ PGM_PTATTRS_PR_MASK |                       PGM_PTATTRS_UR_MASK,
    };

    static const uint32_t *s_apaPerm[] =
    {
        /* U X W R */
        /* 0 0 0 0 */ &g_aPermPrivRead[0],    /* Don't check or modify anything, this translates to a privileged read */
        /* 0 0 0 1 */ &g_aPermPrivRead[0],    /* Privileged read access       */
        /* 0 0 1 0 */ &g_aPermPrivWrite[0],   /* Privileged write access      */
        /* 0 0 1 1 */ NULL,                   /* Invalid access flags         */
        /* 0 1 0 0 */ &g_aPermPrivExec[0],    /* Privileged execute access    */
        /* 0 1 0 1 */ NULL,                   /* Invalid access flags         */
        /* 0 1 1 0 */ NULL,                   /* Invalid access flags         */
        /* 0 1 1 1 */ NULL,                   /* Invalid access flags         */

        /* 1 0 0 0 */ NULL,                   /* Invalid access flags         */
        /* 1 0 0 1 */ &g_aPermUnprivRead[0],  /* Unprivileged read access     */
        /* 1 0 1 0 */ &g_aPermUnprivWrite[0], /* Unprivileged write access    */
        /* 1 0 1 1 */ NULL,                   /* Invalid access flags         */
        /* 1 1 0 0 */ &g_aPermUnprivExec[0],  /* Unprivileged execute access  */
        /* 1 1 0 1 */ NULL,                   /* Invalid access flags         */
        /* 1 1 1 0 */ NULL,                   /* Invalid access flags         */
        /* 1 1 1 1 */ NULL,                   /* Invalid access flags         */
    };
    Assert(fFlags < RT_ELEMENTS(s_apaPerm));

    const uint32_t *paPerm = s_apaPerm[fFlags];
    AssertReturn(paPerm, VERR_PGM_MODE_IPE);

    uint32_t const idxPerm =   RT_BF_GET(Desc, ARMV8_VMSA64_DESC_PG_OR_BLOCK_LATTR_AP)
                             | ((Desc & ARMV8_VMSA64_DESC_PG_OR_BLOCK_UATTR_2PRIV_PXN) >> ARMV8_VMSA64_DESC_PG_OR_BLOCK_UATTR_2PRIV_PXN_BIT) << 2
                             | ((Desc & ARMV8_VMSA64_DESC_PG_OR_BLOCK_UATTR_2PRIV_UXN) >> ARMV8_VMSA64_DESC_PG_OR_BLOCK_UATTR_2PRIV_UXN_BIT) << 3;

    pWalk->fEffective = s_aEffective[idxPerm];

    PGMWALKFAIL const fFailed = paPerm[idxPerm];
    if (fFailed == PGM_WALKFAIL_SUCCESS)
    {
        pWalk->fInfo |= PGM_WALKINFO_SUCCEEDED;
        return VINF_SUCCESS;
    }

    pWalk->fFailed = fFailed | (uLvl << PGM_WALKFAIL_LEVEL_SHIFT);
    return VERR_ACCESS_DENIED;
}


template<bool a_fTtbr0, uint8_t a_InitialLookupLvl, uint8_t a_GranuleSz, bool a_fTbi, bool a_fEpd>
static PGM_CTX_DECL(int) PGM_CTX(pgm,GstQueryPageFast)(PVMCPUCC pVCpu, RTGCPTR GCPtr, uint32_t fFlags, PPGMPTWALKFAST pWalk)
{
    /* This also applies to TG1 granule sizes, as both share the same encoding in TCR. */
    AssertCompile(ARMV8_TCR_EL1_AARCH64_TG0_INVALID == ARMV8_TCR_EL1_AARCH64_TG1_INVALID);
    AssertCompile(ARMV8_TCR_EL1_AARCH64_TG0_16KB    == ARMV8_TCR_EL1_AARCH64_TG1_16KB);
    AssertCompile(ARMV8_TCR_EL1_AARCH64_TG0_4KB     == ARMV8_TCR_EL1_AARCH64_TG1_4KB);
    AssertCompile(ARMV8_TCR_EL1_AARCH64_TG0_64KB    == ARMV8_TCR_EL1_AARCH64_TG1_64KB);

    uint64_t fLookupMaskFull;
    RTGCPTR  offPageMask;

    RTGCPTR offLvl1BlockMask;
    RTGCPTR offLvl2BlockMask;

    uint64_t fNextTableOrPageMask;
    uint8_t  cLvl0Shift;
    uint8_t  cLvl1Shift;
    uint8_t  cLvl2Shift;
    uint8_t  cLvl3Shift;

    RTGCPHYS fGCPhysLvl1BlockBase;
    RTGCPHYS fGCPhysLvl2BlockBase;

    /** @todo This needs to go into defines in armv8.h if final. */
    if RT_CONSTEXPR_IF(a_GranuleSz == ARMV8_TCR_EL1_AARCH64_TG0_4KB)
    {
        fLookupMaskFull      = RT_BIT_64(9) - 1;
        offLvl1BlockMask     = (RTGCPTR)(_1G - 1);
        offLvl2BlockMask     = (RTGCPTR)(_2M - 1);
        offPageMask          = (RTGCPTR)(_4K - 1);
        fNextTableOrPageMask = UINT64_C(0xfffffffff000);
        cLvl0Shift           = 39;
        cLvl1Shift           = 30;
        cLvl2Shift           = 21;
        cLvl3Shift           = 12;
        fGCPhysLvl1BlockBase = UINT64_C(0xffffc0000000);
        fGCPhysLvl2BlockBase = UINT64_C(0xffffffe00000);
    }
    else if RT_CONSTEXPR_IF(a_GranuleSz == ARMV8_TCR_EL1_AARCH64_TG0_16KB)
    {
        fLookupMaskFull      = RT_BIT_64(11) - 1;
        offLvl1BlockMask     = 0; /** @todo TCR_EL1.DS support. */
        offLvl2BlockMask     = (RTGCPTR)(_32M - 1);
        offPageMask          = (RTGCPTR)(_16K - 1);
        fNextTableOrPageMask = UINT64_C(0xffffffffc000);
        cLvl0Shift           = 47;
        cLvl1Shift           = 36;
        cLvl2Shift           = 25;
        cLvl3Shift           = 14;
        fGCPhysLvl1BlockBase = 0; /* Not supported. */
        fGCPhysLvl2BlockBase = UINT64_C(0xfffffe000000);
    }
    else if RT_CONSTEXPR_IF(a_GranuleSz == ARMV8_TCR_EL1_AARCH64_TG0_64KB)
    {
        Assert(a_InitialLookupLvl > 0);

        fLookupMaskFull      = RT_BIT_64(13)   - 1;
        offLvl1BlockMask     = 0; /** @todo FEAT_LPA (RTGCPTR)(4*_1T - 1) */
        offLvl2BlockMask     = (RTGCPTR)(_512M - 1);
        offPageMask          = (RTGCPTR)(_64K  - 1);
        fNextTableOrPageMask = UINT64_C(0xffffffff0000);
        cLvl0Shift           = 0; /* No Level 0 with 64KiB granules. */
        cLvl1Shift           = 42;
        cLvl2Shift           = 29;
        cLvl3Shift           = 16;
        fGCPhysLvl1BlockBase = 0; /* Not supported. */
        fGCPhysLvl2BlockBase = UINT64_C(0xffffe0000000);
    }
    else
        AssertReleaseFailedReturn(VERR_PGM_MODE_IPE);

    /* Get the initial lookup mask. */
    uint8_t const bEl = (fFlags & PGMQPAGE_F_USER_MODE) ? 0 : 1; /** @todo EL2 support */
    uint64_t fLookupMask;
    if RT_CONSTEXPR_IF(a_fTtbr0 == true)
        fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr0[bEl];
    else
        fLookupMask = pVCpu->pgm.s.afLookupMaskTtbr1[bEl];

    RTGCPHYS         GCPhysPt = CPUMGetEffectiveTtbr(pVCpu, GCPtr);
    PARMV8VMSA64DESC paDesc   = NULL;
    ARMV8VMSA64DESC  Desc;
    int rc;
    if RT_CONSTEXPR_IF(a_InitialLookupLvl == 0)
    {
        Assert(cLvl0Shift != 0);
        uint8_t const uLvl = 0;

        rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&paDesc);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, uLvl, rc);

        Desc = ASMAtomicUoReadU64(&paDesc[(GCPtr >> cLvl0Shift) & fLookupMask]);
        if (Desc & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
        else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, uLvl);

        if (Desc & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
        else return pgmGstWalkFastReturnRsvdError(pVCpu, pWalk, uLvl); /** @todo Only supported if TCR_EL1.DS is set. */

        /* Full lookup mask from now on. */
        fLookupMask = fLookupMaskFull;
        GCPhysPt = (RTGCPHYS)(Desc & fNextTableOrPageMask);
    }

    if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 1)
    {
        uint8_t const uLvl = 1;

        rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&paDesc);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, uLvl, rc);

        Desc = ASMAtomicUoReadU64(&paDesc[(GCPtr >> cLvl1Shift) & fLookupMask]);
        if (Desc & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
        else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, uLvl);

        if (Desc & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
        else
        {
            if (offLvl1BlockMask != 0)
            {
                /* Block descriptor. */
                pWalk->GCPtr      = GCPtr;
                pWalk->fInfo      = PGM_WALKINFO_GIGANTIC_PAGE;
                pWalk->GCPhys     = (RTGCPHYS)(Desc & fGCPhysLvl1BlockBase) | (GCPtr & offLvl1BlockMask);
                return pgmGstQueryPageCheckPermissions(pWalk, Desc, fFlags, uLvl);
            }
            else
                return pgmGstWalkFastReturnRsvdError(pVCpu, pWalk, uLvl);
        }

        /* Full lookup mask from now on. */
        fLookupMask = fLookupMaskFull;
        GCPhysPt = (RTGCPHYS)(Desc & fNextTableOrPageMask);
    }

    if RT_CONSTEXPR_IF(a_InitialLookupLvl <= 2)
    {
        uint8_t const uLvl = 2;

        rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&paDesc);
        if (RT_SUCCESS(rc)) { /* probable */ }
        else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, uLvl, rc);

        Desc = ASMAtomicUoReadU64(&paDesc[(GCPtr >> cLvl2Shift) & fLookupMask]);
        if (Desc & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
        else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, uLvl);

        if (Desc & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
        else
        {
            /* Block descriptor. */
            pWalk->GCPtr      = GCPtr;
            pWalk->fInfo      = PGM_WALKINFO_BIG_PAGE;
            pWalk->GCPhys     = (RTGCPHYS)(Desc & fGCPhysLvl2BlockBase) | (GCPtr & offLvl2BlockMask);
            return pgmGstQueryPageCheckPermissions(pWalk, Desc, fFlags, uLvl);
        }

        /* Full lookup mask from now on. */
        fLookupMask = fLookupMaskFull;
        GCPhysPt = (RTGCPHYS)(Desc & fNextTableOrPageMask);
    }

    AssertCompile(a_InitialLookupLvl <= 3);
    uint8_t const uLvl = 3;

    /* Next level. */
    rc = pgmPhysGCPhys2CCPtrLockless(pVCpu, GCPhysPt, (void **)&paDesc);
    if (RT_SUCCESS(rc)) { /* probable */ }
    else return pgmGstWalkFastReturnBadPhysAddr(pVCpu, pWalk, uLvl, rc);

    Desc = ASMAtomicUoReadU64(&paDesc[(GCPtr >> cLvl3Shift) & fLookupMask]);
    if (Desc & ARMV8_VMSA64_DESC_F_VALID) { /* probable */ }
    else return pgmGstWalkFastReturnNotPresent(pVCpu, pWalk, uLvl);

    if (Desc & ARMV8_VMSA64_DESC_F_TBL_OR_PG) { /* probable */ }
    else return pgmGstWalkFastReturnRsvdError(pVCpu, pWalk, uLvl); /* No block descriptors. */

    pWalk->GCPtr  = GCPtr;
    pWalk->GCPhys = (RTGCPHYS)(Desc & fNextTableOrPageMask) | (GCPtr & offPageMask);
    return pgmGstQueryPageCheckPermissions(pWalk, Desc, fFlags, uLvl);
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
        uint64_t       u64Tg  = (u64RegTcr >> a_offTg)  & 0x3;
        bool     const fTbi   = RT_BOOL(u64RegTcr & RT_BIT_64(a_offTbi));
        bool     const fEpd   = RT_BOOL(u64RegTcr & RT_BIT_64(a_offEpd));

        /*
         * From the ARM reference manual regarding granule size choices:
         *
         * If the value is programmed to either a reserved value or a size that has not been implemented, then
         * the hardware will treat the field as if it has been programmed to an IMPLEMENTATION DEFINED
         * choice of the sizes that has been implemented for all purposes other than the value read back from
         * this register.
         *
         * We always fall back on the 4KiB granule size in that case.
         */
        /** @todo Can this be made table driven? */
        uint64_t uLookupLvl;
        if (u64Tg == ARMV8_TCR_EL1_AARCH64_TG0_16KB)
        {
            if (u64Tsz <= 16)
            {
                uLookupLvl = 0;
                *pfInitialLookupMask = 0x1;
            }
            else if (u64Tsz >= 17 && u64Tsz <= 27)
            {
                uLookupLvl = 1;
                *pfInitialLookupMask = RT_BIT_64(28 - u64Tsz + 1) - 1;
            }
            else if (u64Tsz >= 28 && u64Tsz <= 38)
            {
                uLookupLvl = 2;
                *pfInitialLookupMask = RT_BIT_64(38 - u64Tsz + 1) - 1;
            }
            else /* if (u64Tsz == 39) */
            {
                uLookupLvl = 3;
                *pfInitialLookupMask = 0x1;
            }
        }
        else if (u64Tg == ARMV8_TCR_EL1_AARCH64_TG0_64KB)
        {
            if (/*u64Tsz >= 16 &&*/ u64Tsz <= 21)
            {
                uLookupLvl = 1;
                *pfInitialLookupMask = RT_BIT_64(21 - u64Tsz + 1) - 1;
            }
            else if (u64Tsz >= 22 && u64Tsz <= 34)
            {
                uLookupLvl = 2;
                *pfInitialLookupMask = RT_BIT_64(34 - u64Tsz + 1) - 1;
            }
            else /*if (u64Tsz >= 35 && u64Tsz <= 39)*/
            {
                uLookupLvl = 3;
                if (u64Tsz <= 39)
                    *pfInitialLookupMask = RT_BIT_64(39 - u64Tsz + 1) - 1;
                else
                    *pfInitialLookupMask = 0x1;
            }
        }
        else /* if (u64Tg == ARMV8_TCR_EL1_AARCH64_TG0_4KB) */
        {
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
            if (/*u64Tsz >= 16 &&*/ u64Tsz <= 24)
            {
                uLookupLvl = 0;
                if (u64Tsz >= 16)
                    *pfInitialLookupMask = RT_BIT_64(24 - u64Tsz + 1) - 1;
                else
                    *pfInitialLookupMask = RT_BIT_64(9) - 1;
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
                    *pfInitialLookupMask = 0x1;
            }

            u64Tg = ARMV8_TCR_EL1_AARCH64_TG0_4KB;
        }

        /* Build the index into the PGM mode callback table for the given config. */
        idxNewGst = PGM_MODE_TYPE_CREATE(a_fTtbr0, uLookupLvl, u64Tg, fTbi, fEpd);
    }
    else
        idxNewGst = PGM_TYPE_NONE;

    return idxNewGst;
}
