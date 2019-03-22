/* $Id$ */
/** @file
 * SUP Testcase - Contiguous Memory Interface (ring-3).
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
#include <VBox/sup.h>
#include <VBox/param.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char **argv)
{
    int rc;
    int rcRet = 0;

    RTR3InitExe(argc, &argv, 0);
    rc = SUPR3Init(NULL);
    RTPrintf("tstContiguous: SUPR3Init -> rc=%Rrc\n", rc);
    rcRet += rc != 0;
    if (!rc)
    {
        /*
         * Allocate a bit of contiguous memory.
         */
        RTHCPHYS HCPhys;
        void *pv = SUPR3ContAlloc(8, NULL, &HCPhys);
        rcRet += pv == NULL || HCPhys == 0;
        if (pv && HCPhys)
        {
            memset(pv, 0xff, PAGE_SIZE * 8);
            pv = SUPR3ContAlloc(5, NULL, &HCPhys);
            rcRet += pv == NULL || HCPhys == 0;
            if (pv && HCPhys)
            {
                memset(pv, 0x7f, PAGE_SIZE * 5);
                rc = SUPR3ContFree(pv, 5);
                rcRet += rc != 0;
                if (rc)
                    RTPrintf("tstContiguous: SUPR3ContFree failed! rc=%Rrc\n", rc);

                void *apv[128];
                for (unsigned i = 0; i < RT_ELEMENTS(apv); i++)
                {
                    apv[i] = SUPR3ContAlloc(1 + (i % 11), NULL, &HCPhys);
                    if (!apv[i])
                    {
                        RTPrintf("tstContiguous: i=%d: failed to allocate %d pages", i, 1 + (i % 11));
#if defined(RT_ARCH_X86) && defined(RT_OS_LINUX)
                        /* With 32-bit address spaces it's sometimes difficult
                         * to find bigger chunks of contiguous memory */
                        if (i % 11 > 7)
                            RTPrintf(" => ignoring (32-bit host)");
                        else
#endif
                            rcRet++;
                        RTPrintf("\n");
                    }
                }
                for (unsigned i = 0; i < RT_ELEMENTS(apv); i++)
                    if (apv[i])
                    {
                        rc = SUPR3ContFree(apv[i], 1 + (i % 11));
                        rcRet += rc != 0;
                        if (rc)
                            RTPrintf("tstContiguous: i=%d SUPR3ContFree failed! rc=%Rrc\n", i, rc);
                    }
            }
            else
                RTPrintf("tstContiguous: SUPR3ContAlloc (2nd) failed!\n");
        }
        else
            RTPrintf("tstContiguous: SUPR3ContAlloc failed!\n");

        rc = SUPR3Term(false /*fForced*/);
        RTPrintf("tstContiguous: SUPR3Term -> rc=%Rrc\n", rc);
        rcRet += rc != 0;
    }

    return rcRet ? 1 : 0;
}
