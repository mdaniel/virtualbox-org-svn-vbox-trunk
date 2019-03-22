/* $Id$ */
/** @file
 * IPRT Testcase - Unique directory creation.
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
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
#include <iprt/dir.h>

#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static char g_szTempPath[RTPATH_MAX - 50];


static void tst1(size_t cTest, size_t cchDigits, char chSep)
{
    RTTestISubF("tst #%u (digits: %u; sep: %c)", cTest, cchDigits, chSep ? chSep : ' ');

    /* We try to create max possible + one. */
    size_t cTimes = 1;
    for (size_t i = 0; i < cchDigits; ++i)
        cTimes *= 10;

    /* Allocate the result array. */
    char **papszNames = (char **)RTMemTmpAllocZ((cTimes + 1) * sizeof(char *));
    RTTESTI_CHECK_RETV(papszNames != NULL);

    int rc = VERR_INTERNAL_ERROR;
    /* The test loop. */
    size_t i;
    for (i = 0; i < cTimes + 1; i++)
    {
        char szName[RTPATH_MAX];
        RTTESTI_CHECK_RC(rc = RTPathAppend(strcpy(szName, g_szTempPath), sizeof(szName), "RTDirCreateUniqueNumbered"), VINF_SUCCESS);
        if (RT_FAILURE(rc))
            break;

        rc = RTDirCreateUniqueNumbered(szName, sizeof(szName), 0700, cchDigits, chSep);
        if (rc != VINF_SUCCESS)
        {
            /* Random selection (system) isn't 100% predictable, so we must give a little
               leeway for the 2+ digit tests.  (Using random is essential for performance.) */
            if (cchDigits == 1 || rc != VERR_ALREADY_EXISTS || i < cTimes - 1)
                RTTestIFailed("RTDirCreateUniqueNumbered(%s) call #%u -> %Rrc\n", szName, i, rc);
            break;
        }

        RTTESTI_CHECK(papszNames[i] = RTStrDup(szName));
        if (!papszNames[i])
            break;

        RTTestIPrintf(RTTESTLVL_DEBUG, "%s\n", papszNames[i]);
    }

    /* Try to create one more, which shouldn't be possible. */
    if (RT_SUCCESS(rc) && i == cTimes + 1)
    {
        char szName[RTPATH_MAX];
        RTTESTI_CHECK_RC(rc = RTPathAppend(strcpy(szName, g_szTempPath), sizeof(szName), "RTDirCreateUniqueNumbered"), VINF_SUCCESS);
        if (RT_SUCCESS(rc))
            RTTESTI_CHECK_RC(rc = RTDirCreateUniqueNumbered(szName, sizeof(szName), 0700, cchDigits, chSep), VERR_ALREADY_EXISTS);
    }

    /* cleanup */
    while (i-- > 0)
    {
        RTTESTI_CHECK_RC(RTDirRemove(papszNames[i]), VINF_SUCCESS);
        RTStrFree(papszNames[i]);
    }
    RTMemTmpFree(papszNames);
}


int main()
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTDirCreateUniqueNumbered", &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);

    /*
     * Get the temp directory (this is essential to the testcase).
     */
    int rc;
    RTTESTI_CHECK_RC(rc = RTPathTemp(g_szTempPath, sizeof(g_szTempPath)), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);

    /*
     * Create some test directories.
     */
    tst1(1, 1, '\0');
    tst1(2, 1, '-');
    tst1(3, 2, '\0');
    tst1(4, 2, '-');

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

