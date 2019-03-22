/** $Id$ */
/** @file
 * VBoxClient - User specific services, Darwin.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
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
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/process.h>
#include <iprt/string.h>

#include "VBoxClientInternal.h"


/*********************************************************************************************************************************
*   Glogal Variables                                                                                                             *
*********************************************************************************************************************************/

static int                  g_cVerbosity = 0;
static PRTLOGGER            g_pLogger = NULL;

static VBOXCLIENTSERVICE    g_aServices[] =
{
    g_ClipboardService
};


/**
 * Create default logger in order to print output to the specified file.
 *
 * @return  IPRT status code.
 */
static int vbclInitLogger(char *pszLogFileName)
{
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    int rc = RTLogCreateEx(&g_pLogger, RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG, "all", "VBOXCLIENT_RELEASE_LOG",
                           RT_ELEMENTS(s_apszGroups), s_apszGroups, UINT32_MAX /*cMaxEntriesPerGroup*/,
                           RTLOGDEST_STDOUT,
                           NULL /*pfnPhase*/,
                           pszLogFileName ? 10 : 0 /*cHistory*/,
                           pszLogFileName ? 100 * _1M : 0 /*cbHistoryFileMax*/,
                           pszLogFileName ? RT_SEC_1DAY : 0 /*cSecsHistoryTimeSlot*/,
                           NULL /*pErrInfo*/, "%s", pszLogFileName);

    AssertRCReturn(rc, rc);

    /* Register this logger as the release logger */
    RTLogRelSetDefaultInstance(g_pLogger);

    /* Explicitly flush the log in case of VBOXCLIENT_RELEASE_LOG=buffered. */
    RTLogFlush(g_pLogger);

    return VINF_SUCCESS;
}


/**
 * Destroy logger.
 */
static void vbclTermLogger(char *szLogFileName)
{
    // Why SIGBUS here?
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));

    if (szLogFileName)
        RTStrFree(szLogFileName);
}

/**
 * Displays a verbose message.
 *
 * @param   iLevel      Minimum log level required to display this message.
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
void VBoxClientVerbose(int iLevel, const char *pszFormat, ...)
{
    if (iLevel > g_cVerbosity)
        return;

    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogRel(("%s", psz));

    RTStrFree(psz);
}

/**
 * Wait for signals in order to safely terminate process.
 */
static void vbclWait(void)
{
    sigset_t signalMask;
    int      iSignal;

    /* Register signals that we are waiting for */
    sigemptyset(&signalMask);
    sigaddset(&signalMask, SIGHUP);
    sigaddset(&signalMask, SIGINT);
    sigaddset(&signalMask, SIGQUIT);
    sigaddset(&signalMask, SIGABRT);
    sigaddset(&signalMask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &signalMask, NULL);

    /* Ignoring return status */
    sigwait(&signalMask, &iSignal);
}

/**
 * Start registered services.
 *
 * @return  IPRT status code.
 */
static int vbclStartServices(void)
{
    int rc;
    unsigned int iServiceId = 0;

    VBoxClientVerbose(1, "Starting services...\n");
    for (iServiceId = 0; iServiceId < RT_ELEMENTS(g_aServices); iServiceId++)
    {
        VBoxClientVerbose(1, "Starting service: %s\n", g_aServices[iServiceId].pszName);
        rc = (g_aServices[iServiceId].pfnStart)();
        if (RT_FAILURE(rc))
        {
            VBoxClientVerbose(1, "unable to start service: %s (%Rrc)\n", g_aServices[iServiceId].pszName, rc);
            VBoxClientVerbose(1, "Rolling back..\n");

            /* Stop running services */
            do
            {
                VBoxClientVerbose(1, "Stopping service: %s\n", g_aServices[iServiceId].pszName);
                int rcStop = (g_aServices[iServiceId].pfnStop)();
                if (RT_FAILURE(rcStop))
                    VBoxClientVerbose(1, "unable to stop service: %s (%Rrc)\n", g_aServices[iServiceId].pszName, rcStop);
            } while (--iServiceId != 0);

            break;
        }
    }

    if (RT_SUCCESS(rc))
        VBoxClientVerbose(1, "Services start completed.\n");

    return rc;
}

/**
 * Stop registered services.
 *
 * @return  IPRT status code.
 */
static void vbclStopServices(void)
{
    unsigned int iServiceId = 0;

    VBoxClientVerbose(1, "Stopping services...\n");
    for (iServiceId = 0; iServiceId < RT_ELEMENTS(g_aServices); iServiceId++)
    {
        VBoxClientVerbose(1, "Stopping service: %s\n", g_aServices[iServiceId].pszName);
        int rc = (g_aServices[iServiceId].pfnStop)();
        if (RT_FAILURE(rc))
            VBoxClientVerbose(1, "unable to stop service: %s (%Rrc)\n", g_aServices[iServiceId].pszName, rc);
    }
    VBoxClientVerbose(1, "Services stop completed\n");
}


static void usage(char *sProgName)
{
    RTPrintf("usage: %s [-fvl]\n", sProgName);
    RTPrintf("       -f\tRun in foreground (default: no)\n", sProgName);
    RTPrintf("       -v\tIncrease verbosity level (default: no verbosity)\n", sProgName);
    RTPrintf("       -l\tSpecify log file name (default: no log file)\n", sProgName);
    exit(1);
}

int main(int argc, char *argv[])
{
    int  rc;
    int  c;

    bool         fDemonize     = true;
    static char *szLogFileName = NULL;

    /* Parse command line */
    while((c = getopt(argc, argv, "fvl:")) != -1)
    {
        switch(c)
        {
            case 'f':
                fDemonize = false;
                break;
            case 'v':
                g_cVerbosity++;
                break;
            case 'l':
                szLogFileName = RTStrDup(optarg);
                break;

            default : usage(argv[0]);
        }
    }

    /* No more arguments allowed */
    if ((argc - optind) != 0)
        usage(argv[0]);

    if (fDemonize)
    {
        rc = RTProcDaemonizeUsingFork(true /* fNoChDir */, false /* fNoClose */, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("failed to run into background\n");
            return 1;
        }
    }

    rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
    {
        RTPrintf("RTR3InitExe() failed: (%Rrc)\n", rc);
        return RTMsgInitFailure(rc);
    }

    rc = VbglR3Init();
    if (RT_SUCCESS(rc))
    {
        rc = vbclInitLogger(szLogFileName);
        if (RT_SUCCESS(rc))
        {
            rc = vbclStartServices();
            if (RT_SUCCESS(rc))
            {
                vbclWait();
                vbclStopServices();
            }
            else
            {
                RTPrintf("failed to start services: (%Rrc)\n", rc);
            }

            vbclTermLogger(szLogFileName);
        }
        else
        {
            RTPrintf("failed to start logger: (%Rrc)\n", rc);
        }

        VbglR3Term();
    }
    else
    {
        RTPrintf("failed to initialize guest library: (%Rrc)\n", rc);
    }

    return 0;
}
