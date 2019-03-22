/* $Id$ */
/** @file
 * HGCM Service Testcase.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/hgcmsvc.h>
#include <iprt/initterm.h>
#include <iprt/test.h>

/** Test the getString member function.  Indirectly tests the getPointer
 * and getBuffer APIs.
 * @param  hTest  an running IPRT test
 * @param  type  the type that the parameter should be set to before
 *                calling getString
 * @param  pcch   the value that the parameter should be set to before
 *                calling getString, and also the address (!) which we
 *                expect getString to return.  Stricter than needed of
 *                course, but I was feeling lazy.
 * @param  cb     the size that the parameter should be set to before
 *                calling getString, and also the size which we expect
 *                getString to return.
 * @param  rcExp  the expected return value of the call to getString.
 */
void doTestGetString(VBOXHGCMSVCPARM *pParm, RTTEST hTest, uint32_t type,
                     const char *pcch, uint32_t cb, int rcExp)
{
    /* An RTTest API like this, which would print out an additional line
     * of context if a test failed, would be nice.  This is because the
     * line number alone doesn't help much here, given that this is a
     * subroutine called many times. */
    /*
    RTTestContextF(hTest,
                   ("doTestGetString, type=%u, pcch=%p, acp=%u, rcExp=%Rrc",
                    type, pcch, acp, rcExp));
     */
    HGCMSvcSetPv(pParm, (void *)pcch, cb);
    pParm->type = type;  /* in case we don't want VBOX_HGCM_SVC_PARM_PTR */
    const char *pcch2 = NULL;
    uint32_t cb2 = 0;
    int rc = HGCMSvcGetCStr(pParm, &pcch2, &cb2);
    RTTEST_CHECK_RC(hTest, rc, rcExp);
    if (RT_SUCCESS(rcExp))
    {
        RTTEST_CHECK_MSG_RETV(hTest, (pcch2 == pcch),
                              (hTest, "expected %p, got %p", pcch, pcch2));
        RTTEST_CHECK_MSG_RETV(hTest, (cb2 == cb),
                              (hTest, "expected %u, got %u", cb, cb2));
    }
}

/** Run some unit tests on the getString method and indirectly test
 * getPointer and getBuffer as well. */
void testGetString(VBOXHGCMSVCPARM *pParm, RTTEST hTest)
{
    RTTestSub(hTest, "HGCM string parameter handling");
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_32BIT, "test", 3,
                    VERR_INVALID_PARAMETER);
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_PTR, "test", 5,
                    VINF_SUCCESS);
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_PTR, "test", 3,
                    VERR_BUFFER_OVERFLOW);
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_PTR, "test\xf0", 6,
                    VERR_INVALID_UTF8_ENCODING);
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_PTR, "test", 0,
                    VERR_INVALID_PARAMETER);
    doTestGetString(pParm, hTest, VBOX_HGCM_SVC_PARM_PTR, (const char *)0x1, 5,
                    VERR_INVALID_PARAMETER);
    RTTestSubDone(hTest);
}

int main()
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstHGCMSvc", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Run the test.
     */
    VBOXHGCMSVCPARM parm;
    testGetString(&parm, hTest);

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

