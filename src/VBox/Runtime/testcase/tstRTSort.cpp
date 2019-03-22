/* $Id$ */
/** @file
 * IPRT Testcase - Sorting.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include <iprt/sort.h>

#include <iprt/err.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/time.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct TSTRTSORTAPV
{
    uint32_t    aValues[8192];
    void       *apv[8192];
    size_t      cElements;
} TSTRTSORTAPV;


static DECLCALLBACK(int) testApvCompare(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    TSTRTSORTAPV   *pData        = (TSTRTSORTAPV *)pvUser;
    uint32_t const *pu32Element1 = (uint32_t const *)pvElement1;
    uint32_t const *pu32Element2 = (uint32_t const *)pvElement2;
    RTTESTI_CHECK(VALID_PTR(pData) && pData->cElements <= RT_ELEMENTS(pData->aValues));
    RTTESTI_CHECK((uintptr_t)(pu32Element1 - &pData->aValues[0]) < pData->cElements);
    RTTESTI_CHECK((uintptr_t)(pu32Element2 - &pData->aValues[0]) < pData->cElements);

    if (*pu32Element1 < *pu32Element2)
        return -1;
    if (*pu32Element1 > *pu32Element2)
        return 1;
    return 0;
}

static void testApvSorter(FNRTSORTAPV pfnSorter, const char *pszName)
{
    RTTestISub(pszName);

    RTRAND hRand;
    RTTESTI_CHECK_RC_OK_RETV(RTRandAdvCreateParkMiller(&hRand));

    TSTRTSORTAPV Data;
    for (size_t cElements = 0; cElements < RT_ELEMENTS(Data.apv); cElements++)
    {
        RT_ZERO(Data);
        Data.cElements = cElements;

        /* popuplate the array */
        for (size_t i = 0; i < cElements; i++)
        {
            Data.aValues[i] = RTRandAdvU32(hRand);
            Data.apv[i]     = &Data.aValues[i];
        }

        /* sort it */
        pfnSorter(&Data.apv[0], cElements, testApvCompare, &Data);

        /* verify it */
        if (!RTSortApvIsSorted(&Data.apv[0], cElements, testApvCompare, &Data))
            RTTestIFailed("failed sorting %u elements", cElements);
    }
}


static DECLCALLBACK(int) testCompare(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    return memcmp(pvElement1, pvElement2, (size_t)pvUser);
}

static void testSorter(RTTEST hTest, FNRTSORT pfnSorter, const char *pszName)
{
    RTTestISub(pszName);

    /* Use pseudo random config and data. */
    RTRAND hRand;
    RTTESTI_CHECK_RC_OK_RETV(RTRandAdvCreateParkMiller(&hRand));
    RTTIMESPEC Now;
    uint64_t uSeed = RTTimeSpecGetSeconds(RTTimeNow(&Now));
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Seed %#RX64\n", uSeed);
    RTRandAdvSeed(hRand, uSeed);

    for (uint32_t cArrays = 0; cArrays < 512; cArrays++)
    {
        /* Create a random array with random data bytes. */
        uint32_t const cElements = RTRandAdvU32Ex(hRand, 2, 8192);
        uint32_t const cbElement = RTRandAdvU32Ex(hRand, 1, 32);
        uint8_t       *pbArray;
        RTTESTI_CHECK_RC_OK_RETV(RTTestGuardedAlloc(hTest, cElements * cbElement, 1 /*cbAlign*/,
                                                    RT_BOOL(RTRandAdvU32Ex(hRand, 0, 1)) /*fHead*/, (void **)&pbArray));
        RTTESTI_CHECK_RETV(pbArray);
        RTRandAdvBytes(hRand, pbArray, cElements * cbElement);

        /* sort it */
        pfnSorter(pbArray, cElements, cbElement, testCompare, (void *)(uintptr_t)cbElement);

        /* verify it */
        if (!RTSortIsSorted(pbArray, cElements, cbElement, testCompare, (void *)(uintptr_t)cbElement))
            RTTestIFailed("failed sorting %u elements of %u size", cElements, cbElement);

        RTTestGuardedFree(hTest, pbArray);
    }
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTTemp", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Test the different algorithms.
     */
    testSorter(hTest, RTSortShell, "RTSortShell - shell sort, variable sized element array");
    testApvSorter(RTSortApvShell, "RTSortApvShell - shell sort, pointer array");

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

