/* $Id$ */
/** @file
 * SUP Testcase - Test SUPR3HardenedVerifyPlugIn.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#include <VBox/err.h>

#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/stream.h>


int main(int argc, char **argv)
{
    /*
     * Init.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);
    rc = SUPR3HardenedVerifyInit();
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "SUPR3HardenedVerifyInit failed: %Rrc", rc);

    /*
     * Process arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dummy",            'd', RTGETOPT_REQ_NOTHING },
    };

    //bool fKeepLoaded = false;

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case VINF_GETOPT_NOT_OPTION:
            {
                RTERRINFOSTATIC ErrInfo;
                RTErrInfoInitStatic(&ErrInfo);
                rc = SUPR3HardenedVerifyPlugIn(ValueUnion.psz, &ErrInfo.Core);
                if (RT_SUCCESS(rc))
                    RTMsgInfo("SUPR3HardenedVerifyPlugIn: %Rrc for '%s'\n", rc, ValueUnion.psz);
                else
                    RTMsgError("SUPR3HardenedVerifyPlugIn: %Rrc for '%s'  ErrInfo: %s\n",
                               rc, ValueUnion.psz, ErrInfo.Core.pszMsg);
                break;
            }

            case 'h':
                RTPrintf("%s [dll1 [dll2...]]\n", argv[0]);
                return 1;

            case 'V':
                RTPrintf("$Revision$\n");
                return 0;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    return 0;
}

