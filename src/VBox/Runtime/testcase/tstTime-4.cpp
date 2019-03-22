/* $Id$ */
/** @file
 * IPRT Testcase - Simple RTTime vs. RTTimeSystem test.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/thread.h>
#include <VBox/sup.h>



int main()
{
    unsigned cErrors = 0;

    RTR3InitExeNoArguments(RTR3INIT_FLAGS_SUPLIB);
    RTPrintf("tstTime-4: TESTING...\n");

    /*
     * Check that RTTimeSystemNanoTS doesn't go backwards and
     * isn't too far from RTTimeNanoTS().
     */
    RTTimeSystemNanoTS(); RTTimeNanoTS(); RTThreadYield(); /* warmup */
    const uint64_t SysStartTS = RTTimeSystemNanoTS();
    const uint64_t GipStartTS = RTTimeNanoTS();
    uint64_t SysPrevTS, GipPrevTS;
    do
    {
        SysPrevTS = RTTimeSystemNanoTS();
        GipPrevTS = RTTimeNanoTS();
        if (SysPrevTS < SysStartTS)
        {
            cErrors++;
            RTPrintf("tstTime-4: Bad Sys time!\n");
        }
        if (GipPrevTS < GipStartTS)
        {
            cErrors++;
            RTPrintf("tstTime-4: Bad Gip time!\n");
        }
        uint64_t Delta = GipPrevTS > SysPrevTS ? GipPrevTS - SysPrevTS :
                                                 SysPrevTS - GipPrevTS;
        if (Delta > 100000000ULL /* 100 ms */ )
        {
            cErrors++;
            RTPrintf("tstTime-4: Delta=%llu (GipPrevTS=%llu, SysPrevTS=%llu)!\n", Delta, GipPrevTS, SysPrevTS);
        }

    } while (SysPrevTS - SysStartTS < 2000000000 /* 2s */);


    if (!cErrors)
        RTPrintf("tstTime-4: SUCCESS\n");
    else
        RTPrintf("tstTime-4: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

