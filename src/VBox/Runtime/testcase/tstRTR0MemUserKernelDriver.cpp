/* $Id$ */
/** @file
 * IPRT R0 Testcase - Thread Preemption, driver program.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
#include <iprt/initterm.h>

#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/thread.h>
#ifdef VBOX
# include <VBox/sup.h>
# include "tstRTR0MemUserKernel.h"
#endif


/**
 * Entry point.
 */
extern "C" DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    RT_NOREF3(argc, argv, envp);
#ifndef VBOX
    RTPrintf("tstSup: SKIPPED\n");
    return 0;
#else
    /*
     * Init.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTR0MemUserKernel", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    uint8_t *pbPage = (uint8_t *)RTTestGuardedAllocTail(hTest, PAGE_SIZE);
    if (!pbPage)
    {
        RTTestFailed(hTest, "RTTestGuardedAllocTail failed with rc=%Rrc\n", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    PSUPDRVSESSION pSession;
    rc = SUPR3Init(&pSession);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "SUPR3Init failed with rc=%Rrc\n", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    char szPath[RTPATH_MAX];
    rc = RTPathExecDir(szPath, sizeof(szPath));
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(szPath, sizeof(szPath), "tstRTR0MemUserKernel.r0");
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "Failed constructing .r0 filename (rc=%Rrc)", rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    void *pvImageBase;
    rc = SUPR3LoadServiceModule(szPath, "tstRTR0MemUserKernel",
                                "TSTRTR0MemUserKernelSrvReqHandler",
                                &pvImageBase);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "SUPR3LoadServiceModule(%s,,,) failed with rc=%Rrc\n", szPath, rc);
        return RTTestSummaryAndDestroy(hTest);
    }

    /* test request */
    struct
    {
        SUPR0SERVICEREQHDR  Hdr;
        char                szMsg[256];
    } Req;

    /*
     * Sanity checks.
     */
    RTTestSub(hTest, "Sanity");
    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_SANITY_OK, 0, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    RTTESTI_CHECK_MSG(Req.szMsg[0] == '\0', ("%s", Req.szMsg));
    if (Req.szMsg[0] != '\0')
        return RTTestSummaryAndDestroy(hTest);

    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_SANITY_FAILURE, 0, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    RTTESTI_CHECK_MSG(!strncmp(Req.szMsg, RT_STR_TUPLE("!42failure42")), ("%s", Req.szMsg));
    if (strncmp(Req.szMsg, RT_STR_TUPLE("!42failure42")))
        return RTTestSummaryAndDestroy(hTest);

    /*
     * Basic tests, bail out on failure.
     */
    RTTestSub(hTest, "Basics");
    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_BASIC, (uintptr_t)pbPage, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    if (Req.szMsg[0] == '!')
    {
        RTTestIFailed("%s", &Req.szMsg[1]);
        return RTTestSummaryAndDestroy(hTest);
    }
    if (Req.szMsg[0])
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s", Req.szMsg);

    /*
     * Good buffer, bail out on failure.
     */
    RTTestSub(hTest, "Good buffer");
    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_GOOD, (uintptr_t)pbPage, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    if (Req.szMsg[0] == '!')
    {
        RTTestIFailed("%s", &Req.szMsg[1]);
        return RTTestSummaryAndDestroy(hTest);
    }
    if (Req.szMsg[0])
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s", Req.szMsg);

    /*
     * Bad buffer, bail out on failure.
     */
    RTTestSub(hTest, "Bad buffer");
    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_BAD, (uintptr_t)pbPage + PAGE_SIZE, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    if (Req.szMsg[0] == '!')
    {
        RTTestIFailed("%s", &Req.szMsg[1]);
        return RTTestSummaryAndDestroy(hTest);
    }
    if (Req.szMsg[0])
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s", Req.szMsg);

    /*
     * Bad buffer, bail out on failure.
     */
    RTTestSub(hTest, "Kernel buffer");
    Req.Hdr.u32Magic = SUPR0SERVICEREQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.szMsg[0] = '\0';
    RTTESTI_CHECK_RC(rc = SUPR3CallR0Service("tstRTR0MemUserKernel", sizeof("tstRTR0MemUserKernel") - 1,
                                             TSTRTR0MEMUSERKERNEL_INVALID_ADDRESS, (uintptr_t)pvImageBase, &Req.Hdr), VINF_SUCCESS);
    if (RT_FAILURE(rc))
        return RTTestSummaryAndDestroy(hTest);
    if (Req.szMsg[0] == '!')
    {
        RTTestIFailed("%s", &Req.szMsg[1]);
        return RTTestSummaryAndDestroy(hTest);
    }
    if (Req.szMsg[0])
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%s", Req.szMsg);

    /*
     * Done.
     */
    return RTTestSummaryAndDestroy(hTest);
#endif
}


#if !defined(VBOX_WITH_HARDENING) || !defined(RT_OS_WINDOWS)
/**
 * Main entry point.
 */
int main(int argc, char **argv, char **envp)
{
    return TrustedMain(argc, argv, envp);
}
#endif

