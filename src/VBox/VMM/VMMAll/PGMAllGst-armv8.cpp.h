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


DECLINLINE(int) pgmGstGetPageArmv8Hack(PVMCPUCC pVCpu, RTGCPTR GCPtr, PPGMPTWALK pWalk)
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

