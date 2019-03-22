/* $Id$ */
/** @file
 * IPRT Testcase - RTFs Queries..
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
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/err.h>


int main(int argc, char **argv)
{
    RTR3InitExe(argc, &argv, 0);

    /*
     * Process all arguments (including the executable).
     */
    int cErrors = 0;
    for (int i = 0; i < argc; i++)
    {
        RTPrintf("tstRTFsQueries: '%s'...\n", argv[i]);

        uint32_t u32Serial;
        int rc = RTFsQuerySerial(argv[i], &u32Serial);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: u32Serial=%#010RX32\n", u32Serial);
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySerial failed, rc=%Rrc\n", rc);
            cErrors++;
        }

        RTFOFF cbTotal = 42;
        RTFOFF cbFree  = 42;
        uint32_t cbBlock = 42;
        uint32_t cbSector = 42;
        rc = RTFsQuerySizes(argv[i], &cbTotal, &cbFree, &cbBlock, &cbSector);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: cbTotal=%RTfoff cbFree=%RTfoff cbBlock=%d cbSector=%d\n",
                     cbTotal, cbFree, cbBlock, cbSector);
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySerial failed, rc=%Rrc\n", rc);
            cErrors++;
        }

        rc = RTFsQuerySizes(argv[i], NULL, NULL, NULL, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstRTFsQueries: RTFsQuerySizes(nop) failed, rc=%Rrc\n", rc);
            cErrors++;
        }

        RTFSTYPE enmType;
        rc = RTFsQueryType(argv[i], &enmType);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: file system type is '%s'\n", RTFsTypeName(enmType));
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQueryType failed, rc=%Rrc\n", rc);
            cErrors++;
        }

        RTFSPROPERTIES Props;
        rc = RTFsQueryProperties(argv[i], &Props);
        if (RT_SUCCESS(rc))
            RTPrintf("tstRTFsQueries: cbMaxComponent=%u %s %s %s %s %s %s\n",
                     Props.cbMaxComponent,
                     Props.fCaseSensitive ? "case" : "not-case",
                     Props.fCompressed ? "compressed" : "not-compressed",
                     Props.fFileCompression ? "file-compression" : "no-file-compression",
                     Props.fReadOnly ? "readonly" : "readwrite",
                     Props.fRemote ? "remote" : "not-remote",
                     Props.fSupportsUnicode ? "supports-unicode" : "doesn't-support-unicode");
        else
        {
            RTPrintf("tstRTFsQueries: RTFsQueryProperties failed, rc=%Rrc\n", rc);
            cErrors++;
        }
    }

    if (!cErrors)
        RTPrintf("tstRTFsQueries: SUCCESS\n");
    else
        RTPrintf("tstRTFsQueries: FAIlURE - %u errors\n", cErrors);
    return !!cErrors;
}
