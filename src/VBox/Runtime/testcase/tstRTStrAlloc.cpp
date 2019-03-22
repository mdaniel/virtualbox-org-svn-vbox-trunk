/* $Id$ */
/** @file
 * IPRT Testcase - String allocation APIs and related manipulators.
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
#include <iprt/string.h>

#include <iprt/asm.h>
#include <iprt/errcore.h>
#include <iprt/test.h>


/**
 * Basic API checks.
 * We'll return if any of these fails.
 */
static void tst1(void)
{
    RTTestISub("Basics");
    char *psz;
    int rc = VINF_SUCCESS;

    /* RTStrAlloc */
    RTTESTI_CHECK(psz = RTStrAlloc(0));
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrAlloc(1));
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrAlloc(128));
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    /* RTStrAllocEx */
    psz = (char*)"asdfasdf";
    RTTESTI_CHECK_RC(RTStrAllocEx(&psz, 0), VINF_SUCCESS);
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    RTTESTI_CHECK_RC(RTStrAllocEx(&psz, 1), VINF_SUCCESS);
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    RTTESTI_CHECK_RC(RTStrAllocEx(&psz, 128), VINF_SUCCESS);
    RTTESTI_CHECK(psz && !*psz);
    RTStrFree(psz);

    /* RTStrRealloc */
    psz = NULL;
    RTTESTI_CHECK_RC(RTStrRealloc(&psz, 10), VINF_SUCCESS);
    RTTESTI_CHECK(psz && !psz[0]);
    RTTESTI_CHECK(psz && !psz[9]);
    RTStrFree(psz);

    psz = NULL;
    RTTESTI_CHECK_RC(RTStrRealloc(&psz, 0), VINF_SUCCESS);
    RTTESTI_CHECK(!psz);

    psz = NULL;
    RTTESTI_CHECK_RC(RTStrRealloc(&psz, 128), VINF_SUCCESS);
    RTTESTI_CHECK(psz && !psz[0]);
    RTTESTI_CHECK(psz && !psz[127]);
    if (psz)
    {
        memset(psz, 'a', 127);
        RTTESTI_CHECK_RC(rc = RTStrRealloc(&psz, 160), VINF_SUCCESS);
        if (RT_SUCCESS(rc) && psz)
        {
            RTTESTI_CHECK(!psz[127]);
            RTTESTI_CHECK(!psz[159]);
            RTTESTI_CHECK(ASMMemIsAllU8(psz, 127, 'a'));
            memset(psz, 'b', 159);

            RTTESTI_CHECK_RC(rc = RTStrRealloc(&psz, 79), VINF_SUCCESS);
            if (RT_SUCCESS(rc))
            {
                RTTESTI_CHECK(!psz[78]);
                RTTESTI_CHECK(ASMMemIsAllU8(psz, 78, 'b'));

                RTTESTI_CHECK_RC(rc = RTStrRealloc(&psz, 0), VINF_SUCCESS);
                RTTESTI_CHECK(!psz);
            }
        }
    }
    RTStrFree(psz);

    /* RTStrDup */
    RTTESTI_CHECK(psz = RTStrDup(""));
    RTTESTI_CHECK(psz && *psz == '\0');
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDup("abcdefghijklmnop"));
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnop"));
    RTStrFree(psz);

    /* RTStrDupEx */
    psz = NULL;
    RTTESTI_CHECK_RC(RTStrDupEx(&psz, ""), VINF_SUCCESS);
    RTTESTI_CHECK(RT_FAILURE(rc) || *psz == '\0');
    if (RT_SUCCESS(rc))
        RTStrFree(psz);

    psz = (char*)"asdfasdfasdfasdf";
    RTTESTI_CHECK_RC(rc = RTStrDupEx(&psz, "abcdefghijklmnop"), VINF_SUCCESS);
    RTTESTI_CHECK(RT_FAILURE(rc) || !RTStrCmp(psz, "abcdefghijklmnop"));
    if (RT_SUCCESS(rc))
        RTStrFree(psz);

    /* RTStrDupN */
    RTTESTI_CHECK(psz = RTStrDupN("abcdefg", 3));
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDupN("abc", 100000));
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDupN("abc", 0));
    RTTESTI_CHECK(psz && *psz == '\0');
    RTStrFree(psz);

    /* RTStrAAppend */
    RTTESTI_CHECK(psz = RTStrDup("abc"));
    RTTESTI_CHECK_RC(RTStrAAppend(&psz, "def"), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdef"));
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDup("abc"));
    RTTESTI_CHECK_RC(RTStrAAppend(&psz, ""), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTTESTI_CHECK_RC(RTStrAAppend(&psz, NULL), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTStrFree(psz);

    psz = NULL;
    RTTESTI_CHECK_RC(RTStrAAppend(&psz, "xyz"), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "xyz"));
    RTStrFree(psz);

    /* RTStrAAppendN */
    RTTESTI_CHECK(psz = RTStrDup("abc"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "def", 1), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcd"));
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDup("abc"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "", 0), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "", RTSTR_MAX), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, NULL, 0), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTStrFree(psz);

    psz = NULL;
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "abc", 2), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "ab"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "cdefghijklm", 1), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abc"));
    RTTESTI_CHECK_RC(RTStrAAppendN(&psz, "defghijklm", RTSTR_MAX), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklm"));
    RTStrFree(psz);

    /* RTStrAAppendExN / RTStrAAppendExNV */
    psz = NULL;
    RTTESTI_CHECK_RC(RTStrAAppendExN(&psz, 5, "a", (size_t)1, "bc", (size_t)1, "cdefg", RTSTR_MAX, "hijkl", (size_t)2, "jklmnopqrstuvwxyz", RTSTR_MAX), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RC(RTStrAAppendExN(&psz, 0), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RC(RTStrAAppendExN(&psz, 2, NULL, (size_t)0, "", (size_t)0), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RC(RTStrAAppendExN(&psz, 1, "-", (size_t)1), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnopqrstuvwxyz-"));
    RTStrFree(psz);

    /* RTStrATruncate */
    psz = NULL;
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 0), VINF_SUCCESS);
    RTTESTI_CHECK(!psz);

    RTTESTI_CHECK(psz = RTStrDup(""));
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 0), VINF_SUCCESS);
    RTStrFree(psz);

    RTTESTI_CHECK(psz = RTStrDup("1234567890"));
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 5), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "12345"));
    RTStrFree(psz);

    psz = NULL;
    for (uint32_t i = 0; i < 128; i++)
        RTTESTI_CHECK_RC_RETV(RTStrAAppend(&psz, "abcdefghijklmnopqrstuvwxyz"), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, sizeof("abcdefghijklmnopqrstuvwxyz") - 1), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 6), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "abcdef"));
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 1), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, "a"));
    RTTESTI_CHECK_RC(RTStrATruncate(&psz, 0), VINF_SUCCESS);
    RTTESTI_CHECK(!RTStrCmp(psz, ""));
    RTStrFree(psz);

}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTStrAlloc", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tst1();

    return RTTestSummaryAndDestroy(hTest);
}

