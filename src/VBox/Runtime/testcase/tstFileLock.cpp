/* $Id$ */
/** @file
 * IPRT Testcase - File Locks.
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
#include <iprt/file.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/thread.h>  /* for RTThreadSleep() */
#include <iprt/initterm.h>
#include <iprt/string.h>

#include <stdio.h>

static char achTest1[] = "Test line 1\n";
static char achTest2[] = "Test line 2\n";
static char achTest3[] = "Test line 3\n";

static bool fRun = false;

/** @todo Create a tstFileLock-2 which doesn't require interaction and counts errors. */

int main()
{
    RTR3InitExeNoArguments(0);
    RTPrintf("tstFileLock: TESTING\n");

    RTFILE File;
    int rc = RTFileOpen(&File, "tstLock.tst", RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE);
    RTPrintf("File open: rc=%Rrc\n", rc);
    if (RT_FAILURE(rc))
    {
        if (rc != VERR_FILE_NOT_FOUND && rc != VERR_OPEN_FAILED)
        {
            RTPrintf("FATAL\n");
            return 1;
        }

        rc = RTFileOpen(&File, "tstLock.tst", RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE);
        RTPrintf("File create: rc=%Rrc\n", rc);
        if (RT_FAILURE(rc))
        {
            RTPrintf("FATAL\n");
            return 2;
        }
        fRun = true;
    }

    /* grow file a little */
    rc = RTFileSetSize(File, fRun ? 2048 : 20480);
    RTPrintf("File size: rc=%Rrc\n", rc);

    int buf;
    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Rrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest1, strlen(achTest1), NULL);
    RTPrintf("Write: rc=%Rrc\n", rc);

    /* lock: read, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_READ | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Lock: read, non-blocking, rc=%Rrc\n", rc);
    bool fl = RT_SUCCESS(rc);

    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Rrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest2, strlen(achTest2), NULL);
    RTPrintf("Write: rc=%Rrc\n", rc);
    RTPrintf("Lock test will change in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    /* change lock: write, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_WRITE | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Change lock: write, non-blocking, rc=%Rrc\n", rc);
    RTPrintf("Test will unlock in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    /* remove lock. */
    if (fl)
    {
        fl = false;
        rc = RTFileUnlock(File, 0, _4G);
        RTPrintf("Unlock: rc=%Rrc\n", rc);
        RTPrintf("Write test will lock in three seconds\n");
        for (int i = 0; i < 3; i++)
        {
            RTThreadSleep(1000);
            RTPrintf(".");
        }
        RTPrintf("\n");
    }

    /* lock: write, non-blocking. */
    rc = RTFileLock(File, RTFILE_LOCK_WRITE | RTFILE_LOCK_IMMEDIATELY, 0, _4G);
    RTPrintf("Lock: write, non-blocking, rc=%Rrc\n", rc);
    fl = RT_SUCCESS(rc);

    /* grow file test */
    rc = RTFileSetSize(File, fRun ? 2048 : 20480);
    RTPrintf("File size: rc=%Rrc\n", rc);

    /* read test. */
    rc = RTFileRead(File, &buf, sizeof(buf), NULL);
    RTPrintf("Read: rc=%Rrc\n", rc);

    /* write test. */
    rc = RTFileWrite(File, achTest3, strlen(achTest3), NULL);
    RTPrintf("Write: rc=%Rrc\n", rc);
    RTPrintf("Continuing to next test in three seconds\n");
    for (int i = 0; i < 3; i++)
    {
        RTThreadSleep(1000);
        RTPrintf(".");
    }
    RTPrintf("\n");

    RTFileClose(File);
    RTFileDelete("tstLock.tst");


    RTPrintf("tstFileLock: I've no recollection of this testcase succeeding or not, sorry.\n");
    return 0;
}
