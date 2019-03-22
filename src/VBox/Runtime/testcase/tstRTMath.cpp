/* $Id$ */
/** @file
 * IPRT Testcase - base mathematics.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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

#include <iprt/test.h>

RT_C_DECLS_BEGIN
extern uint64_t __udivmoddi4(uint64_t u64A, uint64_t u64B, uint64_t *pu64R);
RT_C_DECLS_END

static struct {
    uint64_t u64A;
    uint64_t u64B;
    uint64_t u64Div;
    uint64_t u64Rem;
} gUDivMod[]
=
{
    {                           10,                            5,                            2,                            0 },
    {                (uint64_t)-10,                 (uint64_t)-5,                            0,                (uint64_t)-10 },
    { UINT64_C(0x7FFFFFFFFFFFFFFF),                            1, UINT64_C(0x7FFFFFFFFFFFFFFF),                            0 },
    { UINT64_C(0x7FFFFFFFFFFFFFFF), UINT64_C(0x7FFFFFFFFFFFFFFF),                            1,                            0 },
    { UINT64_C(0xFFFFFFFFFFFFFFFF),                            2, UINT64_C(0x7FFFFFFFFFFFFFFF),                            1 },
    {                            1,                            2,                            0,                            1 },
    { UINT64_C(0xFFFFFFFFFFFFFFFE), UINT64_C(0xFFFFFFFFFFFFFFFF),                            0, UINT64_C(0xFFFFFFFFFFFFFFFE) },
    { UINT64_C(0xEEEEEEEE12345678), UINT64_C(0x00000000EEEEEEEE),                  0x100000000,                   0x12345678 },
};

static void tstCorrectness(RTTEST hTest)
{
    RTTestSub(hTest, "Correctness");

    uint64_t u64Rem;
    for (unsigned i = 0; i < RT_ELEMENTS(gUDivMod); i++)
        RTTEST_CHECK(hTest, __udivmoddi4(gUDivMod[i].u64A, gUDivMod[i].u64B, &u64Rem) == gUDivMod[i].u64Div && u64Rem == gUDivMod[i].u64Rem);
}

int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTMath", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstCorrectness(hTest);

    return RTTestSummaryAndDestroy(hTest);
}
