/* $Id$ */
/** @file
 * xmmsaving - Test that all XMM register state is handled correctly and
 *             not corrupted the VMM.
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
#include <iprt/test.h>
#include <iprt/x86.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct MYXMMREGSET
{
    RTUINT128U  aRegs[16];
} MYXMMREGSET;


DECLASM(int) XmmSavingTestLoadSet(const MYXMMREGSET *pSet, const MYXMMREGSET *pPrevSet, PRTUINT128U pBadVal);


static void XmmSavingTest(void)
{
    RTTestISub("xmm saving and restoring");

    /* Create the test sets. */
    static MYXMMREGSET s_aSets[256];
    for (unsigned s = 0; s < RT_ELEMENTS(s_aSets); s++)
    {
        for (unsigned r = 0; r < RT_ELEMENTS(s_aSets[s].aRegs); r++)
        {
            unsigned x = (s << 4) | r;
            s_aSets[s].aRegs[r].au32[0] =  x        | UINT32_C(0x12345000);
            s_aSets[s].aRegs[r].au32[1] = (x << 8)  | UINT32_C(0x88700011);
            s_aSets[s].aRegs[r].au32[2] = (x << 16) | UINT32_C(0xe000dcba);
            s_aSets[s].aRegs[r].au32[3] = (x << 20) | UINT32_C(0x00087654);
        }
    }

    /* Do the actual testing. */
    const MYXMMREGSET *pPrev2 = NULL;
    const MYXMMREGSET *pPrev = NULL;
    for (int i = 0; i < 1000000; i++)
    {
        if ((i % 50000) == 0)
        {
            RTTestIPrintf(RTTESTLVL_ALWAYS, ".");
            pPrev = pPrev2 = NULL; /* May be trashed by the above call. */
        }
        for (unsigned s = 0; s < RT_ELEMENTS(s_aSets); s++)
        {
            RTUINT128U         BadVal;
            const MYXMMREGSET *pSet = &s_aSets[s];
            int r = XmmSavingTestLoadSet(pSet, pPrev, &BadVal);
            if (r-- != 0)
            {
                RTTestIFailed("i=%d s=%d r=%d", i, s, r);
                RTTestIFailureDetails("XMM%-2d  = %08x,%08x,%08x,%08x\n",
                                      r,
                                      BadVal.au32[0],
                                      BadVal.au32[1],
                                      BadVal.au32[2],
                                      BadVal.au32[3]);
                RTTestIFailureDetails("Expected %08x,%08x,%08x,%08x\n",
                                      pPrev->aRegs[r].au32[0],
                                      pPrev->aRegs[r].au32[1],
                                      pPrev->aRegs[r].au32[2],
                                      pPrev->aRegs[r].au32[3]);
                if (pPrev2)
                    RTTestIFailureDetails("PrevPrev %08x,%08x,%08x,%08x\n",
                                          pPrev2->aRegs[r].au32[0],
                                          pPrev2->aRegs[r].au32[1],
                                          pPrev2->aRegs[r].au32[2],
                                          pPrev2->aRegs[r].au32[3]);
                return;
            }
            pPrev2 = pPrev;
            pPrev = pSet;
        }
    }
    RTTestISubDone();
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("xmmsaving", &hTest);
    if (rc)
        return rc;
    XmmSavingTest();
    return RTTestSummaryAndDestroy(hTest);
}

