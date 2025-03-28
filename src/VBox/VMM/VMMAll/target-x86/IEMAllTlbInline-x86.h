/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager - x86 target, Inline TLB routines.
 *
 * Mainly related to large pages.
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


#ifndef VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllTlbInline_x86_h
#define VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllTlbInline_x86_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if defined(IEM_WITH_CODE_TLB) || defined(IEM_WITH_DATA_TLB)

/**
 * Helper for doing large page accounting at TLB load time.
 */
template<bool const a_fGlobal>
DECL_FORCE_INLINE(void) iemTlbLoadedLargePage(PVMCPUCC pVCpu, IEMTLB *pTlb, RTGCPTR uTagNoRev, bool f2MbLargePages)
{
    if (a_fGlobal)
        pTlb->cTlbGlobalLargePageCurLoads++;
    else
        pTlb->cTlbNonGlobalLargePageCurLoads++;

# ifdef IEMTLB_WITH_LARGE_PAGE_BITMAP
    RTGCPTR const idxBit = IEMTLB_TAG_TO_EVEN_INDEX(uTagNoRev) + a_fGlobal;
    ASMBitSet(pTlb->bmLargePage, idxBit);
# endif

    AssertCompile(IEMTLB_CALC_TAG_NO_REV(pVCpu, (RTGCPTR)0x8731U << GUEST_PAGE_SHIFT) == 0x8731U);
    uint32_t const                 fMask = (f2MbLargePages ? _2M - 1U : _4M - 1U) >> GUEST_PAGE_SHIFT;
    IEMTLB::LARGEPAGERANGE * const pRange = a_fGlobal
                                          ? &pTlb->GlobalLargePageRange
                                          : &pTlb->NonGlobalLargePageRange;
    uTagNoRev &= ~(RTGCPTR)fMask;
    if (uTagNoRev < pRange->uFirstTag)
        pRange->uFirstTag = uTagNoRev;

    uTagNoRev |= fMask;
    if (uTagNoRev > pRange->uLastTag)
        pRange->uLastTag = uTagNoRev;

    RT_NOREF_PV(pVCpu);
}


/** @todo graduate this to cdefs.h or asm-mem.h.   */
# ifdef RT_ARCH_ARM64              /** @todo RT_CACHELINE_SIZE is wrong for M1 */
#  undef RT_CACHELINE_SIZE
#  define RT_CACHELINE_SIZE 128
# endif

# if defined(_MM_HINT_T0) && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86))
#  define MY_PREFETCH(a_pvAddr)     _mm_prefetch((const char *)(a_pvAddr), _MM_HINT_T0)
# elif defined(_MSC_VER) && (defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32))
#  define MY_PREFETCH(a_pvAddr)     __prefetch((a_pvAddr))
# elif defined(__GNUC__) || RT_CLANG_HAS_FEATURE(__builtin_prefetch)
#  define MY_PREFETCH(a_pvAddr)     __builtin_prefetch((a_pvAddr), 0 /*rw*/, 3 /*locality*/)
# else
#  define MY_PREFETCH(a_pvAddr)     ((void)0)
# endif
# if 0
#  undef  MY_PREFETCH
#  define MY_PREFETCH(a_pvAddr)     ((void)0)
# endif

/** @def MY_PREFETCH_64
 * 64 byte prefetch hint, could be more depending on cache line size. */
/** @def MY_PREFETCH_128
 * 128 byte prefetch hint. */
/** @def MY_PREFETCH_256
 * 256 byte prefetch hint. */
# if RT_CACHELINE_SIZE >= 128
    /* 128 byte cache lines */
#  define MY_PREFETCH_64(a_pvAddr)  MY_PREFETCH(a_pvAddr)
#  define MY_PREFETCH_128(a_pvAddr) MY_PREFETCH(a_pvAddr)
#  define MY_PREFETCH_256(a_pvAddr) do { \
        MY_PREFETCH(a_pvAddr); \
        MY_PREFETCH((uint8_t const *)a_pvAddr + 128); \
    } while (0)
# else
    /* 64 byte cache lines */
#  define MY_PREFETCH_64(a_pvAddr)  MY_PREFETCH(a_pvAddr)
#  define MY_PREFETCH_128(a_pvAddr) do { \
        MY_PREFETCH(a_pvAddr); \
        MY_PREFETCH((uint8_t const *)a_pvAddr + 64); \
    } while (0)
#  define MY_PREFETCH_256(a_pvAddr) do { \
        MY_PREFETCH(a_pvAddr); \
        MY_PREFETCH((uint8_t const *)a_pvAddr + 64); \
        MY_PREFETCH((uint8_t const *)a_pvAddr + 128); \
        MY_PREFETCH((uint8_t const *)a_pvAddr + 192); \
    } while (0)
# endif

template<bool const a_fDataTlb, bool const a_f2MbLargePage, bool const a_fGlobal, bool const a_fNonGlobal>
DECLINLINE(void) iemTlbInvalidateLargePageWorkerInner(PVMCPUCC pVCpu, IEMTLB *pTlb, RTGCPTR GCPtrTag,
                                                      RTGCPTR GCPtrInstrBufPcTag) RT_NOEXCEPT
{
    IEMTLBTRACE_LARGE_SCAN(pVCpu, a_fGlobal, a_fNonGlobal, a_fDataTlb);
    AssertCompile(IEMTLB_ENTRY_COUNT >= 16); /* prefetching + unroll assumption */

    if (a_fGlobal)
        pTlb->cTlbInvlPgLargeGlobal += 1;
    if (a_fNonGlobal)
        pTlb->cTlbInvlPgLargeNonGlobal += 1;

    /*
     * Set up the scan.
     *
     * GCPtrTagMask: A 2MB page consists of 512 4K pages, so a 256 TLB will map
     * offset zero and offset 1MB to the same slot pair.  Our GCPtrTag[Globl]
     * values are for the range 0-1MB, or slots 0-256.  So, we construct a mask
     * that fold large page offsets 1MB-2MB into the 0-1MB range.
     *
     * For our example with 2MB pages and a 256 entry TLB: 0xfffffffffffffeff
     *
     * MY_PREFETCH: Hope that prefetching 256 bytes at the time is okay for
     * relevant host architectures.
     */
    /** @todo benchmark this code from the guest side. */
    bool const      fPartialScan = IEMTLB_ENTRY_COUNT > (a_f2MbLargePage ? 512 : 1024);
#ifdef IEMTLB_WITH_LARGE_PAGE_BITMAP
    uintptr_t       idxBitmap    = fPartialScan ? IEMTLB_TAG_TO_EVEN_INDEX(GCPtrTag) / 64 : 0;
    uintptr_t const idxBitmapEnd = fPartialScan ? idxBitmap + ((a_f2MbLargePage ? 512 : 1024) * 2) / 64
                                                : IEMTLB_ENTRY_COUNT * 2 / 64;
#else
    uintptr_t       idxEven      = fPartialScan ? IEMTLB_TAG_TO_EVEN_INDEX(GCPtrTag) : 0;
    MY_PREFETCH_256(&pTlb->aEntries[idxEven + !a_fNonGlobal]);
    uintptr_t const idxEvenEnd   = fPartialScan ? idxEven + ((a_f2MbLargePage ? 512 : 1024) * 2) : IEMTLB_ENTRY_COUNT * 2;
#endif
    RTGCPTR const   GCPtrTagMask = fPartialScan ? ~(RTGCPTR)0
                                 : ~(RTGCPTR)(  (RT_BIT_32(a_f2MbLargePage ? 9 : 10) - 1U)
                                              & ~(uint32_t)(RT_BIT_32(IEMTLB_ENTRY_COUNT_AS_POWER_OF_TWO) - 1U));

    /*
     * Set cbInstrBufTotal to zero if GCPtrInstrBufPcTag is within any of the tag ranges.
     * We make ASSUMPTIONS about IEMTLB_CALC_TAG_NO_REV here.
     */
    AssertCompile(IEMTLB_CALC_TAG_NO_REV(pVCpu, (RTGCPTR)0x8731U << GUEST_PAGE_SHIFT) == 0x8731U);
    if (   !a_fDataTlb
        && GCPtrInstrBufPcTag - GCPtrTag < (a_f2MbLargePage ? 512U : 1024U))
        pVCpu->iem.s.cbInstrBufTotal = 0;

    /*
     * Combine TAG values with the TLB revisions.
     */
    RTGCPTR GCPtrTagGlob = a_fGlobal ? GCPtrTag | pTlb->uTlbRevisionGlobal : 0;
    if (a_fNonGlobal)
        GCPtrTag |= pTlb->uTlbRevision;

    /*
     * Do the scanning.
     */
#ifdef IEMTLB_WITH_LARGE_PAGE_BITMAP
    uint64_t const bmMask  = a_fGlobal && a_fNonGlobal ? UINT64_MAX
                           : a_fGlobal ? UINT64_C(0xaaaaaaaaaaaaaaaa) : UINT64_C(0x5555555555555555);
    /* Scan bitmap entries (64 bits at the time): */
    for (;;)
    {
# if 1
        uint64_t bmEntry = pTlb->bmLargePage[idxBitmap] & bmMask;
        if (bmEntry)
        {
            /* Scan the non-zero 64-bit value in groups of 8 bits: */
            uint64_t  bmToClear = 0;
            uintptr_t idxEven   = idxBitmap * 64;
            uint32_t  idxTag    = 0;
            for (;;)
            {
                if (bmEntry & 0xff)
                {
#  define ONE_PAIR(a_idxTagIter, a_idxEvenIter, a_bmNonGlobal, a_bmGlobal) \
                        if (a_fNonGlobal) \
                        { \
                            if (bmEntry & a_bmNonGlobal) \
                            { \
                                Assert(pTlb->aEntries[a_idxEvenIter].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE); \
                                if ((pTlb->aEntries[a_idxEvenIter].uTag & GCPtrTagMask) == (GCPtrTag + a_idxTagIter)) \
                                { \
                                    IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTag + a_idxTagIter, \
                                                                 pTlb->aEntries[a_idxEvenIter].GCPhys, \
                                                                 a_idxEvenIter, a_fDataTlb); \
                                    pTlb->aEntries[a_idxEvenIter].uTag = 0; \
                                    bmToClearSub8 |= a_bmNonGlobal; \
                                } \
                            } \
                            else \
                                Assert(   !(pTlb->aEntries[a_idxEvenIter].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE)\
                                       ||    (pTlb->aEntries[a_idxEvenIter].uTag & IEMTLB_REVISION_MASK) \
                                          != (GCPtrTag & IEMTLB_REVISION_MASK)); \
                        } \
                        if (a_fGlobal) \
                        { \
                            if (bmEntry & a_bmGlobal) \
                            {  \
                                Assert(pTlb->aEntries[a_idxEvenIter + 1].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE); \
                                if ((pTlb->aEntries[a_idxEvenIter + 1].uTag & GCPtrTagMask) == (GCPtrTagGlob + a_idxTagIter)) \
                                { \
                                    IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTagGlob + a_idxTagIter, \
                                                                 pTlb->aEntries[a_idxEvenIter + 1].GCPhys, \
                                                                 a_idxEvenIter + 1, a_fDataTlb); \
                                    pTlb->aEntries[a_idxEvenIter + 1].uTag = 0; \
                                    bmToClearSub8 |= a_bmGlobal; \
                                } \
                            } \
                            else \
                                Assert(   !(pTlb->aEntries[a_idxEvenIter + 1].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE)\
                                       ||    (pTlb->aEntries[a_idxEvenIter + 1].uTag & IEMTLB_REVISION_MASK) \
                                          != (GCPtrTagGlob & IEMTLB_REVISION_MASK)); \
                        }
                    uint64_t bmToClearSub8 = 0;
                    ONE_PAIR(idxTag + 0, idxEven + 0, 0x01, 0x02)
                    ONE_PAIR(idxTag + 1, idxEven + 2, 0x04, 0x08)
                    ONE_PAIR(idxTag + 2, idxEven + 4, 0x10, 0x20)
                    ONE_PAIR(idxTag + 3, idxEven + 6, 0x40, 0x80)
                    bmToClear |= bmToClearSub8 << (idxTag * 2);
#  undef ONE_PAIR
                }

                /* advance to the next 8 bits. */
                bmEntry >>= 8;
                if (!bmEntry)
                    break;
                idxEven  += 8;
                idxTag   += 4;
            }

            /* Clear the large page flags we covered. */
            pTlb->bmLargePage[idxBitmap] &= ~bmToClear;
        }
# else
        uint64_t const bmEntry = pTlb->bmLargePage[idxBitmap] & bmMask;
        if (bmEntry)
        {
            /* Scan the non-zero 64-bit value completely unrolled: */
            uintptr_t const idxEven   = idxBitmap * 64;
            uint64_t        bmToClear = 0;
#  define ONE_PAIR(a_idxTagIter, a_idxEvenIter, a_bmNonGlobal, a_bmGlobal) \
                if (a_fNonGlobal) \
                { \
                    if (bmEntry & a_bmNonGlobal) \
                    { \
                        Assert(pTlb->aEntries[a_idxEvenIter].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE); \
                        if ((pTlb->aEntries[a_idxEvenIter].uTag & GCPtrTagMask) == (GCPtrTag + a_idxTagIter)) \
                        { \
                            IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTag + a_idxTagIter, \
                                                         pTlb->aEntries[a_idxEvenIter].GCPhys, \
                                                         a_idxEvenIter, a_fDataTlb); \
                            pTlb->aEntries[a_idxEvenIter].uTag = 0; \
                            bmToClear |= a_bmNonGlobal; \
                        } \
                    } \
                    else \
                        Assert(   !(pTlb->aEntriqes[a_idxEvenIter].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE)\
                               ||    (pTlb->aEntries[a_idxEvenIter].uTag & IEMTLB_REVISION_MASK) \
                                  != (GCPtrTag & IEMTLB_REVISION_MASK)); \
                } \
                if (a_fGlobal) \
                { \
                    if (bmEntry & a_bmGlobal) \
                    {  \
                        Assert(pTlb->aEntries[a_idxEvenIter + 1].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE); \
                        if ((pTlb->aEntries[a_idxEvenIter + 1].uTag & GCPtrTagMask) == (GCPtrTagGlob + a_idxTagIter)) \
                        { \
                            IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTagGlob + a_idxTagIter, \
                                                         pTlb->aEntries[a_idxEvenIter + 1].GCPhys, \
                                                         a_idxEvenIter + 1, a_fDataTlb); \
                            pTlb->aEntries[a_idxEvenIter + 1].uTag = 0; \
                            bmToClear |= a_bmGlobal; \
                        } \
                    } \
                    else \
                        Assert(   !(pTlb->aEntries[a_idxEvenIter + 1].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE)\
                               ||    (pTlb->aEntries[a_idxEvenIter + 1].uTag & IEMTLB_REVISION_MASK) \
                                  != (GCPtrTagGlob & IEMTLB_REVISION_MASK)); \
                } ((void)0)
#  define FOUR_PAIRS(a_iByte, a_cShift) \
                ONE_PAIR(0 + a_iByte * 4, idxEven + 0 + a_iByte * 8, UINT64_C(0x01) << a_cShift, UINT64_C(0x02) << a_cShift); \
                ONE_PAIR(1 + a_iByte * 4, idxEven + 2 + a_iByte * 8, UINT64_C(0x04) << a_cShift, UINT64_C(0x08) << a_cShift); \
                ONE_PAIR(2 + a_iByte * 4, idxEven + 4 + a_iByte * 8, UINT64_C(0x10) << a_cShift, UINT64_C(0x20) << a_cShift); \
                ONE_PAIR(3 + a_iByte * 4, idxEven + 6 + a_iByte * 8, UINT64_C(0x40) << a_cShift, UINT64_C(0x80) << a_cShift)
            if (bmEntry & (uint32_t)UINT16_MAX)
            {
                FOUR_PAIRS(0,  0);
                FOUR_PAIRS(1,  8);
            }
            if (bmEntry & ((uint32_t)UINT16_MAX << 16))
            {
                FOUR_PAIRS(2, 16);
                FOUR_PAIRS(3, 24);
            }
            if (bmEntry & ((uint64_t)UINT16_MAX << 32))
            {
                FOUR_PAIRS(4, 32);
                FOUR_PAIRS(5, 40);
            }
            if (bmEntry & ((uint64_t)UINT16_MAX << 16))
            {
                FOUR_PAIRS(6, 48);
                FOUR_PAIRS(7, 56);
            }
#  undef FOUR_PAIRS

            /* Clear the large page flags we covered. */
            pTlb->bmLargePage[idxBitmap] &= ~bmToClear;
        }
# endif

        /* advance */
        idxBitmap++;
        if (idxBitmap >= idxBitmapEnd)
            break;
        if (a_fNonGlobal)
            GCPtrTag     += 32;
        if (a_fGlobal)
            GCPtrTagGlob += 32;
    }

#else  /* !IEMTLB_WITH_LARGE_PAGE_BITMAP */

    for (; idxEven < idxEvenEnd; idxEven += 8)
    {
# define ONE_ITERATION(a_idxEvenIter) \
            if (a_fNonGlobal)  \
            { \
                if ((pTlb->aEntries[a_idxEvenIter].uTag & GCPtrTagMask) == GCPtrTag) \
                { \
                    if (pTlb->aEntries[a_idxEvenIter].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE) \
                    { \
                        IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTag, pTlb->aEntries[a_idxEvenIter].GCPhys, \
                                                     a_idxEvenIter, a_fDataTlb); \
                        pTlb->aEntries[a_idxEvenIter].uTag = 0; \
                    } \
                } \
                GCPtrTag++; \
            } \
            \
            if (a_fGlobal) \
            { \
                if ((pTlb->aEntries[a_idxEvenIter + 1].uTag & GCPtrTagMask) == GCPtrTagGlob) \
                { \
                    if (pTlb->aEntries[a_idxEvenIter + 1].fFlagsAndPhysRev & IEMTLBE_F_PT_LARGE_PAGE) \
                    { \
                        IEMTLBTRACE_LARGE_EVICT_SLOT(pVCpu, GCPtrTag, pTlb->aEntries[a_idxEvenIter + 1].GCPhys, \
                                                     a_idxEvenIter + 1, a_fDataTlb); \
                        pTlb->aEntries[a_idxEvenIter + 1].uTag = 0; \
                    } \
                } \
                GCPtrTagGlob++; \
            }
        if (idxEven < idxEvenEnd - 4)
            MY_PREFETCH_256(&pTlb->aEntries[idxEven +  8 + !a_fNonGlobal]);
        ONE_ITERATION(idxEven)
        ONE_ITERATION(idxEven + 2)
        ONE_ITERATION(idxEven + 4)
        ONE_ITERATION(idxEven + 6)
# undef ONE_ITERATION
    }
#endif /* !IEMTLB_WITH_LARGE_PAGE_BITMAP */
}

template<bool const a_fDataTlb, bool const a_f2MbLargePage>
DECLINLINE(void) iemTlbInvalidateLargePageWorker(PVMCPUCC pVCpu, IEMTLB *pTlb, RTGCPTR GCPtrTag,
                                                 RTGCPTR GCPtrInstrBufPcTag) RT_NOEXCEPT
{
    AssertCompile(IEMTLB_CALC_TAG_NO_REV(pVCpu, (RTGCPTR)0x8731U << GUEST_PAGE_SHIFT) == 0x8731U);

    GCPtrTag &= ~(RTGCPTR)(RT_BIT_64((a_f2MbLargePage ? 21 : 22) - GUEST_PAGE_SHIFT) - 1U);
    if (   GCPtrTag >= pTlb->GlobalLargePageRange.uFirstTag
        && GCPtrTag <= pTlb->GlobalLargePageRange.uLastTag)
    {
        if (   GCPtrTag < pTlb->NonGlobalLargePageRange.uFirstTag
            || GCPtrTag > pTlb->NonGlobalLargePageRange.uLastTag)
            iemTlbInvalidateLargePageWorkerInner<a_fDataTlb, a_f2MbLargePage, true, false>(pVCpu, pTlb, GCPtrTag, GCPtrInstrBufPcTag);
        else
            iemTlbInvalidateLargePageWorkerInner<a_fDataTlb, a_f2MbLargePage, true, true>(pVCpu, pTlb, GCPtrTag, GCPtrInstrBufPcTag);
    }
    else if (   GCPtrTag < pTlb->NonGlobalLargePageRange.uFirstTag
             || GCPtrTag > pTlb->NonGlobalLargePageRange.uLastTag)
    {
        /* Large pages aren't as likely in the non-global TLB half. */
        IEMTLBTRACE_LARGE_SCAN(pVCpu, false, false, a_fDataTlb);
    }
    else
        iemTlbInvalidateLargePageWorkerInner<a_fDataTlb, a_f2MbLargePage, false, true>(pVCpu, pTlb, GCPtrTag, GCPtrInstrBufPcTag);
}

template<bool const a_fDataTlb>
DECLINLINE(void) iemTlbInvalidatePageWorker(PVMCPUCC pVCpu, IEMTLB *pTlb, RTGCPTR GCPtrTag, uintptr_t idxEven) RT_NOEXCEPT
{
    pTlb->cTlbInvlPg += 1;

    /*
     * Flush the entry pair.
     */
    if (pTlb->aEntries[idxEven].uTag == (GCPtrTag | pTlb->uTlbRevision))
    {
        IEMTLBTRACE_EVICT_SLOT(pVCpu, GCPtrTag, pTlb->aEntries[idxEven].GCPhys, idxEven, a_fDataTlb);
        pTlb->aEntries[idxEven].uTag = 0;
        if (!a_fDataTlb && GCPtrTag == IEMTLB_CALC_TAG_NO_REV(pVCpu, pVCpu->iem.s.uInstrBufPc))
            pVCpu->iem.s.cbInstrBufTotal = 0;
    }
    if (pTlb->aEntries[idxEven + 1].uTag == (GCPtrTag | pTlb->uTlbRevisionGlobal))
    {
        IEMTLBTRACE_EVICT_SLOT(pVCpu, GCPtrTag, pTlb->aEntries[idxEven + 1].GCPhys, idxEven + 1, a_fDataTlb);
        pTlb->aEntries[idxEven + 1].uTag = 0;
        if (!a_fDataTlb && GCPtrTag == IEMTLB_CALC_TAG_NO_REV(pVCpu, pVCpu->iem.s.uInstrBufPc))
            pVCpu->iem.s.cbInstrBufTotal = 0;
    }

    /*
     * If there are (or has been) large pages in the TLB, we must check if the
     * address being flushed may involve one of those, as then we'd have to
     * scan for entries relating to the same page and flush those as well.
     */
# if 0 /** @todo do accurate counts or currently loaded large stuff and we can use those  */
    if (pTlb->cTlbGlobalLargePageCurLoads || pTlb->cTlbNonGlobalLargePageCurLoads)
# else
    if (pTlb->GlobalLargePageRange.uLastTag || pTlb->NonGlobalLargePageRange.uLastTag)
# endif
    {
        RTGCPTR const GCPtrInstrBufPcTag = a_fDataTlb ? 0 : IEMTLB_CALC_TAG_NO_REV(pVCpu, pVCpu->iem.s.uInstrBufPc);
        if (pVCpu->cpum.GstCtx.cr4 & X86_CR4_PAE)
            iemTlbInvalidateLargePageWorker<a_fDataTlb, true>(pVCpu, pTlb, GCPtrTag, GCPtrInstrBufPcTag);
        else
            iemTlbInvalidateLargePageWorker<a_fDataTlb, false>(pVCpu, pTlb, GCPtrTag, GCPtrInstrBufPcTag);
    }
}

#endif /* defined(IEM_WITH_CODE_TLB) || defined(IEM_WITH_DATA_TLB) */

#endif /* !VMM_INCLUDED_SRC_VMMAll_target_x86_IEMAllTlbInline_x86_h */
