/* $Id$ */
/** @file
 * Allocate all memory we can get and then quit.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/test.h>

#include <iprt/asm.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/time.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct TSTALLOC
{
    /** The page sequence number. */
    size_t      iPageSeq;
    /** The allocation sequence number. */
    size_t      iAllocSeq;
    /** The allocation size. */
    size_t      cb;
    /** Pointer to the ourselves (paranoid). */
    void       *pv;
    /** Linked list node. */
    RTLISTNODE  Node;

} TSTALLOC;
typedef TSTALLOC *PTSTALLOC;


static bool checkList(PRTLISTNODE pHead)
{
    size_t iPageSeq  = 0;
    size_t iAllocSeq = 0;
    PTSTALLOC pCur;
    RTListForEach(pHead, pCur, TSTALLOC, Node)
    {
        RTTESTI_CHECK_RET(pCur->iAllocSeq == iAllocSeq, false);
        RTTESTI_CHECK_RET(pCur->pv == pCur, false);

        size_t const *pu    = (size_t const *)pCur;
        size_t const *puEnd = pu + pCur->cb / sizeof(size_t);
        while (pu != puEnd)
        {
            RTTESTI_CHECK_RET(*pu == iPageSeq, false);
            iPageSeq++;
            pu += PAGE_SIZE / sizeof(size_t);
        }
        iAllocSeq++;
    }
    return true;
}


static void doTest(RTTEST hTest)
{
    RTTestSub(hTest, "Allocate all memory");

    RTLISTANCHOR AllocHead;
    PTSTALLOC    pCur;
    uint64_t     cNsElapsed  = 0;
    size_t       cbPrint     = 0;
    uint64_t     uPrintTS    = 0;
    size_t       cbTotal     = 0;
#if ARCH_BITS == 64
    size_t const cbOneStart  = 64 * _1M;
    size_t const cbOneMin    = 4  * _1M;
#else
    size_t const cbOneStart  = 16 * _1M;
    size_t const cbOneMin    = 4  * _1M;
#endif
    size_t       cbOne       = cbOneStart;
    size_t       cAllocs     = 0;
    uint32_t     iPageSeq    = 0;
    RTListInit(&AllocHead);

    for (;;)
    {
        /*
         * Allocate a chunk and make sure all the pages are there.
         */
        uint64_t const uStartTS = RTTimeNanoTS();
        pCur = (PTSTALLOC)RTMemPageAlloc(cbOne);
        if (pCur)
        {
            size_t *pu    = (size_t *)pCur;
            size_t *puEnd = pu + cbOne / sizeof(size_t);
            while (pu != puEnd)
            {
                *pu = iPageSeq++;
                pu += PAGE_SIZE / sizeof(size_t);
            }
            uint64_t const uEndTS  = RTTimeNanoTS();
            uint64_t const cNsThis = uEndTS  - uStartTS;

            /*
             * Update the statistics.
             */
            cNsElapsed += cNsThis;
            cbTotal    += cbOne;
            cAllocs++;

            /*
             * Link the allocation.
             */
            pCur->iAllocSeq = cAllocs - 1;
            pCur->pv        = pCur;
            pCur->cb        = cbOne;
            RTListAppend(&AllocHead, &pCur->Node);

            /*
             * Print progress info?
             */
            if (   uEndTS  - uPrintTS >= RT_NS_1SEC_64*10
#if ARCH_BITS == 64
                || cbTotal - cbPrint  >= _4G
#else
                || cbTotal - cbPrint  >= _2G
#endif
               )
            {
                cbPrint  = cbTotal;
                uPrintTS = uEndTS;

                uint32_t cMBPerSec = (uint32_t)(cbTotal / ((double)cNsElapsed / RT_NS_1SEC) / _1M);
                RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "%'zu bytes in %'llu ns - %'u MB/s\n",
                             cbTotal, cNsElapsed, cMBPerSec);
                RTTESTI_CHECK_RETV(checkList(&AllocHead));
            }
        }
        else
        {
            /*
             * Try again with a smaller request.
             */
            RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Failed to allocate %'zu bytes (after %'zu bytes)\n", cbOne, cbTotal);
            if (cbOne <= cbOneMin)
                break;
            cbOne = cbOneMin;
        }
    }

    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Verifying...\n");
    RTTESTI_CHECK_RETV(checkList(&AllocHead));
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "... detected no corruption.\n");

    /*
     * Free up some memory before displaying the results.
     */
    size_t      i = 0;
    PTSTALLOC   pPrev;
    RTListForEachReverseSafe(&AllocHead, pCur, pPrev, TSTALLOC, Node)
    {
        RTMemPageFree(pCur->pv, pCur->cb);
        if (++i > 32)
            break;
    }

    RTTestValue(hTest, "amount", cbTotal, RTTESTUNIT_BYTES);
    RTTestValue(hTest, "time",   cNsElapsed, RTTESTUNIT_NS);
    uint32_t cMBPerSec = (uint32_t)(cbTotal / ((double)cNsElapsed / RT_NS_1SEC) / _1M);
    RTTestValue(hTest, "speed",  cMBPerSec, RTTESTUNIT_MEGABYTES_PER_SEC);
    RTTestSubDone(hTest);
}


int main(int argc, char **argv)
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("memallocall", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    NOREF(argv);
    if (argc == 1)
        doTest(hTest);
    else
        RTTestFailed(hTest, "This test takes no arguments!");

    return RTTestSummaryAndDestroy(hTest);
}

