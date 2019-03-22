/* $Id$ */
/** @file
 * IPRT Testcase - RTMemSafer* functions.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#include <iprt/memsafer.h>

#include <iprt/asm.h>
#include <iprt/param.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/test.h>
#if defined(VBOX) && (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64))
# include <VBox/sup.h>
#endif



static void doMemSaferScramble(RTTEST hTest, void *pvBuf, size_t cbAlloc)
{
    RT_NOREF_PV(hTest);

    /*
     * Fill it with random bytes and make a reference copy of these.
     */
    RTRandBytes(pvBuf, cbAlloc);

    void *pvRef = RTMemDup(pvBuf, cbAlloc);
    RTTESTI_CHECK_RETV(pvRef);

    /*
     * Scramble the allocation and check that it no longer matches the refernece bytes.
     */
    int rc = RTMemSaferScramble(pvBuf, cbAlloc);
    if (RT_SUCCESS(rc))
    {
        if (!memcmp(pvRef, pvBuf, cbAlloc))
            RTTestIFailed("Memory blocks must differ (%z bytes, 0x%p vs. 0x%p)!\n",
                          cbAlloc, pvRef, pvBuf);
        else
        {
            /*
             * Check that unscrambling returns the original content.
             */
            rc = RTMemSaferUnscramble(pvBuf, cbAlloc);
            if (RT_SUCCESS(rc))
            {
                if (memcmp(pvRef, pvBuf, cbAlloc))
                    RTTestIFailed("Memory blocks must not differ (%z bytes, 0x%p vs. 0x%p)!\n",
                                  cbAlloc, pvRef, pvBuf);
            }
            else
                RTTestIFailed("Unscrambling %z bytes failed with %Rrc!\n", cbAlloc, rc);
        }
    }
    else
        RTTestIFailed("Scrambling %z bytes failed with %Rrc!\n", cbAlloc, rc);

    RTMemFree(pvRef);
}


static void doMemSaferAllocation(RTTEST hTest)
{
    size_t cbAlloc = RTRandS32Ex(1, _1M) * sizeof(uint8_t);

    void *pvBuf = NULL;
    int rc = RTMemSaferAllocZEx(&pvBuf, cbAlloc, 0);
    if (RT_SUCCESS(rc))
    {
        /* Fill it with random bytes. */
        RTRandBytes(pvBuf, cbAlloc);

        /* Scrambling test */
        doMemSaferScramble(hTest, pvBuf, cbAlloc);

        RTMemSaferFree(pvBuf, cbAlloc);
    }
    else
        RTTestIFailed("Allocating %z bytes of secure memory failed with %Rrc\n", cbAlloc, rc);
}


static void doMemRealloc(RTTEST hTest)
{
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "%u reallocation, grow by 1 bytes\n", PAGE_SIZE * 2);
    size_t cbAlloc = RTRandS32Ex(1, _16K);
    void  *pvBuf   = NULL;
    RTTESTI_CHECK_RC_OK_RETV(RTMemSaferAllocZEx(&pvBuf, cbAlloc, 0));
    for (uint32_t i = 0; i <= PAGE_SIZE * 2; i++)
    {
        cbAlloc += 1;
        RTTESTI_CHECK_RC_OK_RETV(RTMemSaferReallocZEx(cbAlloc - 1, pvBuf, cbAlloc, &pvBuf, 0));
        memset(pvBuf, i & 0x7f, cbAlloc);
    }
    RTMemSaferFree(pvBuf, cbAlloc);


    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "100 random reallocations\n");
    uint8_t chFiller = 0x42;
    cbAlloc = 0;
    pvBuf   = NULL;
    for (uint32_t i = 1; i <= 100; i++)
    {
        uint32_t cbNew = RTRandS32Ex(1, _16K + (i / 4) * _16K);
        RTTESTI_CHECK_RC_OK_RETV(RTMemSaferReallocZEx(cbAlloc, pvBuf, cbNew, &pvBuf, 0));

        RTTESTI_CHECK(ASMMemIsAllU8(pvBuf, RT_MIN(cbAlloc, cbNew), chFiller));

        chFiller += 0x31;
        memset(pvBuf, chFiller, cbNew);
        cbAlloc = cbNew;
    }
    RTTESTI_CHECK_RC_OK_RETV(RTMemSaferReallocZEx(cbAlloc, pvBuf, 0, &pvBuf, 0));
    RTTESTI_CHECK(pvBuf == NULL);
}


int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTMemSafer", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);
#if defined(VBOX) && (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64))
    SUPR3Init(NULL);
#endif

    /*
     * Not using sub-tests here, just printing progress.
     */
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "20 random allocations\n");
    for (uint32_t i = 0; i < 20; i++)
        doMemSaferAllocation(hTest);

    doMemRealloc(hTest);

    return RTTestSummaryAndDestroy(hTest);
}

