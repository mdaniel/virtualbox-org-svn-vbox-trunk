/* $Id$ */
/** @file
 * VBoxService - Guest Additions TimeSync Service.
 */

/*
 * Copyright (C) 2007-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_vgsvc_timesync     VBoxService - The Time Sync Service
 *
 * The time sync subservice synchronizes the guest OS walltime with the host.
 *
 * The time sync service plays along with the Time Manager (TM) in the VMM
 * to keep the guest time accurate using the host machine as reference.
 * Communication is facilitated by VMMDev.  TM will try its best to make sure
 * all timer ticks gets delivered so that there isn't normally any need to
 * adjust the guest time.
 *
 * There are three normal (= acceptable) cases:
 *      -# When the service starts up. This is because ticks and such might
 *         be lost during VM and OS startup. (Need to figure out exactly why!)
 *      -# When the TM is unable to deliver all the ticks and swallows a
 *         backlog of ticks. The threshold for this is configurable with
 *         a default of 60 seconds.
 *      -# The time is adjusted on the host. This can be caused manually by
 *         the user or by some time sync daemon (NTP, LAN server, etc.).
 *
 * There are a number of very odd case where adjusting is needed. Here
 * are some of them:
 *      -# Timer device emulation inaccuracies (like rounding).
 *      -# Inaccuracies in time source VirtualBox uses.
 *      -# The Guest and/or Host OS doesn't perform proper time keeping. This
 *         come about as a result of OS and/or hardware issues.
 *
 * The TM is our source for the host time and will make adjustments for
 * current timer delivery lag. The simplistic approach taken by TM is to
 * adjust the host time by the current guest timer delivery lag, meaning that
 * if the guest is behind 1 second with PIT/RTC/++ ticks this should be reflected
 * in the guest wall time as well.
 *
 * Now, there is any amount of trouble we can cause by changing the time.
 * Most applications probably uses the wall time when they need to measure
 * things. A walltime that is being juggled about every so often, even if just
 * a little bit, could occasionally upset these measurements by for instance
 * yielding negative results.
 *
 * This bottom line here is that the time sync service isn't really supposed
 * to do anything and will try avoid having to do anything when possible.
 *
 * The implementation uses the latency it takes to query host time as the
 * absolute maximum precision to avoid messing up under timer tick catchup
 * and/or heavy host/guest load. (Rational is that a *lot* of stuff may happen
 * on our way back from ring-3 and TM/VMMDev since we're taking the route
 * thru the inner EM loop with it's force action processing.)
 *
 * But this latency has to be measured from our perspective, which means it
 * could just as easily come out as 0. (OS/2 and Windows guest only updates
 * the current time when the timer ticks for instance.) The good thing is
 * that this isn't really a problem since we won't ever do anything unless
 * the drift is noticeable.
 *
 * It now boils down to these three (configuration) factors:
 *  -# g_cMsTimeSyncMinAdjust - The minimum drift we will ever bother with.
 *  -# g_TimeSyncLatencyFactor - The factor we multiply the latency by to
 *     calculate the dynamic minimum adjust factor.
 *  -# g_cMsTimeSyncMaxLatency - When to start discarding the data as utterly
 *     useless and take a rest (someone is too busy to give us good data).
 *  -# g_TimeSyncSetThreshold - The threshold at which we will just set the time
 *     instead of trying to adjust it (milliseconds).
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#else
# include <unistd.h>
# include <errno.h>
# include <time.h>
# include <sys/time.h>
#endif

#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The timesync interval (milliseconds). */
static uint32_t         g_TimeSyncInterval = 0;
/**
 * @see pg_vgsvc_timesync
 *
 * @remark  OS/2: There is either a 1 second resolution on the DosSetDateTime
 *                API or a bug in my settimeofday implementation.  Thus, don't
 *                bother unless there is at least a 1 second drift.
 */
#ifdef RT_OS_OS2
static uint32_t         g_cMsTimeSyncMinAdjust = 1000;
#else
static uint32_t         g_cMsTimeSyncMinAdjust = 100;
#endif
/** @see pg_vgsvc_timesync */
static uint32_t         g_TimeSyncLatencyFactor = 8;
/** @see pg_vgsvc_timesync */
static uint32_t         g_cMsTimeSyncMaxLatency = 250;
/** @see pg_vgsvc_timesync */
static uint32_t         g_TimeSyncSetThreshold = 20*60*1000;
/** Whether the next adjustment should just set the time instead of trying to
 * adjust it. This is used to implement --timesync-set-start.  */
static bool volatile    g_fTimeSyncSetNext = false;
/** Whether to set the time when the VM was restored. */
static bool             g_fTimeSyncSetOnRestore = true;
/** The logging verbosity.
 *  This uses the global verbosity level by default. */
static unsigned         g_uTimeSyncVerbosity = 0;

/** Current error count. Used to knowing when to bitch and when not to. */
static uint32_t         g_cTimeSyncErrors = 0;

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_TimeSyncEvent = NIL_RTSEMEVENTMULTI;

/** The VM session ID. Changes whenever the VM is restored or reset. */
static uint64_t         g_idTimeSyncSession;

#ifdef RT_OS_WINDOWS
/** Process token. */
static HANDLE           g_hTokenProcess = NULL;
/** Old token privileges. */
static TOKEN_PRIVILEGES g_TkOldPrivileges;
/** Backup values for time adjustment. */
static DWORD            g_dwWinTimeAdjustment;
static DWORD            g_dwWinTimeIncrement;
static BOOL             g_bWinTimeAdjustmentDisabled;
#endif


/**
 * @interface_method_impl{VBOXSERVICE,pfnPreInit}
 */
static DECLCALLBACK(int) vgsvcTimeSyncPreInit(void)
{
    /* Use global verbosity as default. */
    g_uTimeSyncVerbosity = g_cVerbosity;

#ifdef VBOX_WITH_GUEST_PROPS
    /** @todo Merge this function with vgsvcTimeSyncOption() to generalize
     *        the "command line args override guest property values" behavior. */

    /*
     * Read the service options from the VM's guest properties.
     * Note that these options can be overridden by the command line options later.
     */
    uint32_t uGuestPropSvcClientID;
    int rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
        {
            VGSvcVerbose(0, "VMInfo: Guest property service is not available, skipping\n");
            rc = VINF_SUCCESS;
        }
        else
            VGSvcError("Failed to connect to the guest property service! Error: %Rrc\n", rc);
    }
    else
    {
        rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-interval",
                                 &g_TimeSyncInterval, 50, UINT32_MAX - 1);
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-min-adjust",
                                     &g_cMsTimeSyncMinAdjust, 0, 3600000);
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-latency-factor",
                                     &g_TimeSyncLatencyFactor, 1, 1024);
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-max-latency",
                                     &g_cMsTimeSyncMaxLatency, 1, 3600000);
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-threshold",
                                     &g_TimeSyncSetThreshold, 0, 7*24*60*60*1000 /* a week */);
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
        {
            char *pszValue;
            rc = VGSvcReadProp(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-start",
                               &pszValue, NULL /* ppszFlags */, NULL /* puTimestamp */);
            if (RT_SUCCESS(rc))
            {
                g_fTimeSyncSetNext = true;
                RTStrFree(pszValue);
            }
        }
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
        {
            uint32_t value;
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-on-restore",
                                     &value, 1, 1);
            if (RT_SUCCESS(rc))
                g_fTimeSyncSetOnRestore = !!value;
        }
        if (   RT_SUCCESS(rc)
            || rc == VERR_NOT_FOUND)
        {
            uint32_t value;
            rc = VGSvcReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--timesync-verbosity",
                                     &value, 0, 255);
            if (RT_SUCCESS(rc))
                g_uTimeSyncVerbosity = (unsigned)value;
        }
        VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
    }

    if (rc == VERR_NOT_FOUND) /* If a value is not found, don't be sad! */
        rc = VINF_SUCCESS;
    return rc;
#else
    /* Nothing to do here yet. */
    return VINF_SUCCESS;
#endif
}


/**
 * Displays a verbose message based on the currently
 * set timesync verbosity level.
 *
 * @param   iLevel      Minimum log level required to display this message.
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
static void vgsvcTimeSyncLog(unsigned iLevel, const char *pszFormat, ...)
{
    if (iLevel <= g_uTimeSyncVerbosity)
    {
        va_list args;
        va_start(args, pszFormat);

        VGSvcLog(pszFormat, args);

        va_end(args);
    }
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnOption}
 */
static DECLCALLBACK(int) vgsvcTimeSyncOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    int rc = -1;
    uint32_t value;
    if (ppszShort)
        /* no short options */;
    else if (!strcmp(argv[*pi], "--timesync-interval"))
        rc = VGSvcArgUInt32(argc, argv, "", pi, &g_TimeSyncInterval, 50, UINT32_MAX - 1);
    else if (!strcmp(argv[*pi], "--timesync-min-adjust"))
        rc = VGSvcArgUInt32(argc, argv, "", pi, &g_cMsTimeSyncMinAdjust, 0, 3600000);
    else if (!strcmp(argv[*pi], "--timesync-latency-factor"))
        rc = VGSvcArgUInt32(argc, argv, "", pi, &g_TimeSyncLatencyFactor, 1, 1024);
    else if (!strcmp(argv[*pi], "--timesync-max-latency"))
        rc = VGSvcArgUInt32(argc, argv, "", pi, &g_cMsTimeSyncMaxLatency, 1, 3600000);
    else if (!strcmp(argv[*pi], "--timesync-set-threshold"))
        rc = VGSvcArgUInt32(argc, argv, "", pi, &g_TimeSyncSetThreshold, 0, 7*24*60*60*1000); /* a week */
    else if (!strcmp(argv[*pi], "--timesync-set-start"))
    {
        g_fTimeSyncSetNext = true;
        rc = VINF_SUCCESS;
    }
    else if (!strcmp(argv[*pi], "--timesync-set-on-restore"))
    {
        rc = VGSvcArgUInt32(argc, argv, "", pi, &value, 1, 1);
        if (RT_SUCCESS(rc))
            g_fTimeSyncSetOnRestore = !!value;
    }

    return rc;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vgsvcTimeSyncInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_TimeSyncInterval)
        g_TimeSyncInterval = g_DefaultInterval * 1000;
    if (!g_TimeSyncInterval)
        g_TimeSyncInterval = 10 * 1000;

    VbglR3GetSessionId(&g_idTimeSyncSession);
    /* The status code is ignored as this information is not available with VBox < 3.2.10. */

    int rc = RTSemEventMultiCreate(&g_TimeSyncEvent);
    AssertRC(rc);
#ifdef RT_OS_WINDOWS
    if (RT_SUCCESS(rc))
    {
        /*
         * Adjust privileges of this process so we can make system time adjustments.
         */
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &g_hTokenProcess))
        {
            TOKEN_PRIVILEGES tkPriv;
            RT_ZERO(tkPriv);
            tkPriv.PrivilegeCount = 1;
            tkPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &tkPriv.Privileges[0].Luid))
            {
                DWORD cbRet = sizeof(g_TkOldPrivileges);
                if (AdjustTokenPrivileges(g_hTokenProcess, FALSE, &tkPriv, sizeof(TOKEN_PRIVILEGES), &g_TkOldPrivileges, &cbRet))
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD dwErr = GetLastError();
                    rc = RTErrConvertFromWin32(dwErr);
                    VGSvcError("vgsvcTimeSyncInit: Adjusting token privileges (SE_SYSTEMTIME_NAME) failed with status code %u/%Rrc!\n",
                               dwErr, rc);
                }
            }
            else
            {
                DWORD dwErr = GetLastError();
                rc = RTErrConvertFromWin32(dwErr);
                VGSvcError("vgsvcTimeSyncInit: Looking up token privileges (SE_SYSTEMTIME_NAME) failed with status code %u/%Rrc!\n",
                           dwErr, rc);
            }
            if (RT_FAILURE(rc))
            {
                CloseHandle(g_hTokenProcess);
                g_hTokenProcess = NULL;
            }
        }
        else
        {
            DWORD dwErr = GetLastError();
            rc = RTErrConvertFromWin32(dwErr);
            VGSvcError("vgsvcTimeSyncInit: Opening process token (SE_SYSTEMTIME_NAME) failed with status code %u/%Rrc!\n",
                       dwErr, rc);
            g_hTokenProcess = NULL;
        }
    }

    if (GetSystemTimeAdjustment(&g_dwWinTimeAdjustment, &g_dwWinTimeIncrement, &g_bWinTimeAdjustmentDisabled))
        vgsvcTimeSyncLog(3, "vgsvcTimeSyncInit: Initially %ld (100ns) units per %ld (100 ns) units interval, disabled=%d\n",
                         g_dwWinTimeAdjustment, g_dwWinTimeIncrement, g_bWinTimeAdjustmentDisabled ? 1 : 0);
    else
    {
        DWORD dwErr = GetLastError();
        rc = RTErrConvertFromWin32(dwErr);
        VGSvcError("vgsvcTimeSyncInit: Could not get time adjustment values! Last error: %ld!\n", dwErr);
    }
#endif /* RT_OS_WINDOWS */

    return rc;
}


/**
 * Try adjust the time using adjtime or similar.
 *
 * @returns true on success, false on failure.
 *
 * @param   pDrift          The time adjustment.
 */
static bool vgsvcTimeSyncAdjust(PCRTTIMESPEC pDrift)
{
#ifdef RT_OS_WINDOWS
/** @todo r=bird: g_hTokenProcess cannot be NULL here.
 *        vgsvcTimeSyncInit will fail and the service will not be started with
 *        it being NULL.  vgsvcTimeSyncInit OTOH will *NOT* be called until the
 *        service thread has terminated.  If anything
 *        else is the case, there is buggy code somewhere.*/
    if (g_hTokenProcess == NULL) /* Is the token already closed when shutting down? */
        return false;

    DWORD dwWinTimeAdjustment, dwWinNewTimeAdjustment, dwWinTimeIncrement;
    BOOL  fWinTimeAdjustmentDisabled;
    if (GetSystemTimeAdjustment(&dwWinTimeAdjustment, &dwWinTimeIncrement, &fWinTimeAdjustmentDisabled))
    {
        DWORD dwDiffMax = g_dwWinTimeAdjustment * 0.50;
        DWORD dwDiffNew =   dwWinTimeAdjustment * 0.10;

        if (RTTimeSpecGetMilli(pDrift) > 0)
        {
            dwWinNewTimeAdjustment = dwWinTimeAdjustment + dwDiffNew;
            if (dwWinNewTimeAdjustment > (g_dwWinTimeAdjustment + dwDiffMax))
            {
                dwWinNewTimeAdjustment = g_dwWinTimeAdjustment + dwDiffMax;
                dwDiffNew = dwDiffMax;
            }
        }
        else
        {
            dwWinNewTimeAdjustment = dwWinTimeAdjustment - dwDiffNew;
            if (dwWinNewTimeAdjustment < (g_dwWinTimeAdjustment - dwDiffMax))
            {
                dwWinNewTimeAdjustment = g_dwWinTimeAdjustment - dwDiffMax;
                dwDiffNew = dwDiffMax;
            }
        }

        vgsvcTimeSyncLog(3, "vgsvcTimeSyncAdjust: Drift=%lldms\n", RTTimeSpecGetMilli(pDrift));
        vgsvcTimeSyncLog(3, "vgsvcTimeSyncAdjust: OrgTA=%ld, CurTA=%ld, NewTA=%ld, DiffNew=%ld, DiffMax=%ld\n",
                         g_dwWinTimeAdjustment, dwWinTimeAdjustment, dwWinNewTimeAdjustment, dwDiffNew, dwDiffMax);
        if (SetSystemTimeAdjustment(dwWinNewTimeAdjustment, FALSE /* Periodic adjustments enabled. */))
        {
            g_cTimeSyncErrors = 0;
            return true;
        }

        if (g_cTimeSyncErrors++ < 10)
             VGSvcError("vgsvcTimeSyncAdjust: SetSystemTimeAdjustment failed, error=%u\n", GetLastError());
    }
    else if (g_cTimeSyncErrors++ < 10)
        VGSvcError("vgsvcTimeSyncAdjust: GetSystemTimeAdjustment failed, error=%ld\n", GetLastError());

#elif defined(RT_OS_OS2) || defined(RT_OS_HAIKU)
    /* No API for doing gradual time adjustments. */

#else /* PORTME */
    /*
     * Try use adjtime(), most unix-like systems have this.
     */
    struct timeval tv;
    RTTimeSpecGetTimeval(pDrift, &tv);
    if (adjtime(&tv, NULL) == 0)
    {
        if (g_cVerbosity >= 1)
            vgsvcTimeSyncLog(1, "vgsvcTimeSyncAdjust: adjtime by %RDtimespec\n", pDrift);
        g_cTimeSyncErrors = 0;
        return true;
    }
#endif

    /* failed */
    return false;
}


/**
 * Cancels any pending time adjustment.
 *
 * Called when we've caught up and before calls to vgsvcTimeSyncSet.
 */
static void vgsvcTimeSyncCancelAdjust(void)
{
#ifdef RT_OS_WINDOWS
/** @todo r=bird: g_hTokenProcess cannot be NULL here.  See argumentation in
 *        vgsvcTimeSyncAdjust.  */
    if (g_hTokenProcess == NULL) /* No process token (anymore)? */
        return;
    if (SetSystemTimeAdjustment(0, TRUE /* Periodic adjustments disabled. */))
        vgsvcTimeSyncLog(3, "vgsvcTimeSyncCancelAdjust: Windows Time Adjustment is now disabled.\n");
    else if (g_cTimeSyncErrors++ < 10)
        VGSvcError("vgsvcTimeSyncCancelAdjust: SetSystemTimeAdjustment(,disable) failed, error=%u\n", GetLastError());
#endif /* !RT_OS_WINDOWS */
}


/**
 * Try adjust the time using adjtime or similar.
 *
 * @returns true on success, false on failure.
 *
 * @param   pDrift              The time adjustment.
 */
static void vgsvcTimeSyncSet(PCRTTIMESPEC pDrift)
{
    /*
     * Query the current time, adjust it by adding the drift and set it.
     */
    RTTIMESPEC NewGuestTime;
    int rc = RTTimeSet(RTTimeSpecAdd(RTTimeNow(&NewGuestTime), pDrift));
    if (RT_SUCCESS(rc))
    {
        /* Succeeded - reset the error count and log the change. */
        g_cTimeSyncErrors = 0;

        if (g_cVerbosity >= 1)
        {
            char        sz[64];
            RTTIME      Time;
            vgsvcTimeSyncLog(1, "time set to %s\n", RTTimeToString(RTTimeExplode(&Time, &NewGuestTime), sz, sizeof(sz)));
#ifdef DEBUG
            RTTIMESPEC  Tmp;
            if (g_cVerbosity >= 3)
                vgsvcTimeSyncLog(3, "        now %s\n", RTTimeToString(RTTimeExplode(&Time, RTTimeNow(&Tmp)), sz, sizeof(sz)));
#endif
        }
    }
    else if (g_cTimeSyncErrors++ < 10)
        VGSvcError("vgsvcTimeSyncSet: RTTimeSet(%RDtimespec) failed: %Rrc\n", &NewGuestTime, rc);
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnWorker}
 */
DECLCALLBACK(int) vgsvcTimeSyncWorker(bool volatile *pfShutdown)
{
    RTTIME Time;
    char sz[64];
    int rc = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Initialize the last host time to prevent log message.
     */
    RTTIMESPEC HostLast;
    if (RT_FAILURE(VbglR3GetHostTime(&HostLast)))
        RTTimeSpecSetNano(&HostLast, 0);

    /*
     * The Work Loop.
     */
    for (;;)
    {
        /*
         * Try get a reliable time reading.
         */
        int cTries = 3;
        do
        {
            /*
             * Query the session id (first to keep lantency low) and the time.
             */
            uint64_t idNewSession = g_idTimeSyncSession;
            if (g_fTimeSyncSetOnRestore)
                VbglR3GetSessionId(&idNewSession);

            RTTIMESPEC GuestNow0;
            RTTimeNow(&GuestNow0);

            RTTIMESPEC HostNow;
            int rc2 = VbglR3GetHostTime(&HostNow);
            if (RT_FAILURE(rc2))
            {
                if (g_cTimeSyncErrors++ < 10)
                    VGSvcError("vgsvcTimeSyncWorker: VbglR3GetHostTime failed; rc2=%Rrc\n", rc2);
                break;
            }

            RTTIMESPEC GuestNow;
            RTTimeNow(&GuestNow);

            /*
             * Calc latency and check if it's ok.
             */
            RTTIMESPEC GuestElapsed = GuestNow;
            RTTimeSpecSub(&GuestElapsed, &GuestNow0);
            if ((uint32_t)RTTimeSpecGetMilli(&GuestElapsed) < g_cMsTimeSyncMaxLatency)
            {
                /*
                 * If we were just restored, set the adjustment threshold to zero to force a resync.
                 */
                uint32_t TimeSyncSetThreshold = g_TimeSyncSetThreshold;
                if (   g_fTimeSyncSetOnRestore
                    && idNewSession != g_idTimeSyncSession)
                {
                    vgsvcTimeSyncLog(3, "vgsvcTimeSyncWorker: The VM session ID changed, forcing resync.\n");
                    g_idTimeSyncSession  = idNewSession;
                    TimeSyncSetThreshold = 0;
                }

                /*
                 * Calculate the adjustment threshold and the current drift.
                 */
                uint32_t MinAdjust = RTTimeSpecGetMilli(&GuestElapsed) * g_TimeSyncLatencyFactor;
                if (MinAdjust < g_cMsTimeSyncMinAdjust)
                    MinAdjust = g_cMsTimeSyncMinAdjust;

                RTTIMESPEC Drift = HostNow;
                RTTimeSpecSub(&Drift, &GuestNow);
                if (RTTimeSpecGetMilli(&Drift) < 0)
                    MinAdjust += g_cMsTimeSyncMinAdjust; /* extra buffer against moving time backwards. */

                RTTIMESPEC AbsDrift = Drift;
                RTTimeSpecAbsolute(&AbsDrift);
                if (g_cVerbosity >= 3)
                {
                    vgsvcTimeSyncLog(3, "vgsvcTimeSyncWorker: Host:    %s    (MinAdjust: %RU32 ms)\n",
                                     RTTimeToString(RTTimeExplode(&Time, &HostNow), sz, sizeof(sz)), MinAdjust);
                    vgsvcTimeSyncLog(3, "vgsvcTimeSyncWorker: Guest: - %s => %RDtimespec drift\n",
                                     RTTimeToString(RTTimeExplode(&Time, &GuestNow), sz, sizeof(sz)), &Drift);
                }

                uint32_t AbsDriftMilli = RTTimeSpecGetMilli(&AbsDrift);
                if (AbsDriftMilli > MinAdjust)
                {
                    /*
                     * Ok, the drift is above the threshold.
                     *
                     * Try a gradual adjustment first, if that fails or the drift is
                     * too big, fall back on just setting the time.
                     */
                    if (   AbsDriftMilli > TimeSyncSetThreshold
                        || g_fTimeSyncSetNext
                        || !vgsvcTimeSyncAdjust(&Drift))
                    {
                        vgsvcTimeSyncCancelAdjust();
                        vgsvcTimeSyncSet(&Drift);
                    }

                    /*
                     * Log radical host time changes.
                     */
                    int64_t cNsHostDelta = RTTimeSpecGetNano(&HostNow) - RTTimeSpecGetNano(&HostLast);
                    if ((uint64_t)RT_ABS(cNsHostDelta) > RT_NS_1HOUR / 2)
                        vgsvcTimeSyncLog(0, "vgsvcTimeSyncWorker: Radical host time change: %'RI64ns (HostNow=%RDtimespec HostLast=%RDtimespec)\n",
                                         cNsHostDelta, &HostNow, &HostLast);
                }
                else
                    vgsvcTimeSyncCancelAdjust();
                HostLast = HostNow;
                break;
            }
            vgsvcTimeSyncLog(3, "vgsvcTimeSyncWorker: %RDtimespec: latency too high (%RDtimespec, max %ums) sleeping 1s\n",
                             &GuestNow, &GuestElapsed, g_cMsTimeSyncMaxLatency);
            RTThreadSleep(1000);
        } while (--cTries > 0);

        /* Clear the set-next/set-start flag. */
        g_fTimeSyncSetNext = false;

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_TimeSyncEvent, g_TimeSyncInterval);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VGSvcError("vgsvcTimeSyncWorker: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    vgsvcTimeSyncCancelAdjust();
    return rc;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vgsvcTimeSyncStop(void)
{
    if (g_TimeSyncEvent != NIL_RTSEMEVENTMULTI)
        RTSemEventMultiSignal(g_TimeSyncEvent);
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnTerm}
 */
static DECLCALLBACK(void) vgsvcTimeSyncTerm(void)
{
#ifdef RT_OS_WINDOWS
    /*
     * Restore the SE_SYSTEMTIME_NAME token privileges (if init succeeded).
     */
    if (g_hTokenProcess)
    {
        if (!AdjustTokenPrivileges(g_hTokenProcess, FALSE, &g_TkOldPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
        {
            DWORD dwErr = GetLastError();
            VGSvcError("vgsvcTimeSyncTerm: Restoring token privileges (SE_SYSTEMTIME_NAME) failed with code %u!\n", dwErr);
        }
        CloseHandle(g_hTokenProcess);
        g_hTokenProcess = NULL;
    }
#endif /* !RT_OS_WINDOWS */

    if (g_TimeSyncEvent != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(g_TimeSyncEvent);
        g_TimeSyncEvent = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * The 'timesync' service description.
 */
VBOXSERVICE g_TimeSync =
{
    /* pszName. */
    "timesync",
    /* pszDescription. */
    "Time synchronization",
    /* pszUsage. */
    "              [--timesync-interval <ms>] [--timesync-min-adjust <ms>]\n"
    "              [--timesync-latency-factor <x>] [--timesync-max-latency <ms>]\n"
    "              [--timesync-set-threshold <ms>] [--timesync-set-start]\n"
    "              [--timesync-set-on-restore 0|1]"
    ,
    /* pszOptions. */
    "    --timesync-interval     Specifies the interval at which to synchronize the\n"
    "                            time with the host. The default is 10000 ms.\n"
    "    --timesync-min-adjust   The minimum absolute drift value measured in\n"
    "                            milliseconds to make adjustments for.\n"
    "                            The default is 1000 ms on OS/2 and 100 ms elsewhere.\n"
    "    --timesync-latency-factor\n"
    "                            The factor to multiply the time query latency with\n"
    "                            to calculate the dynamic minimum adjust time.\n"
    "                            The default is 8 times.\n"
    "    --timesync-max-latency  The max host timer query latency to accept.\n"
    "                            The default is 250 ms.\n"
    "    --timesync-set-threshold\n"
    "                            The absolute drift threshold, given as milliseconds,\n"
    "                            where to start setting the time instead of trying to\n"
    "                            adjust it. The default is 20 min.\n"
    "    --timesync-set-start    Set the time when starting the time sync service.\n"
    "    --timesync-set-on-restore 0|1\n"
    "                            Immediately set the time when the VM was restored.\n"
    "                            1 = set (default), 0 = don't set.\n"
    ,
    /* methods */
    vgsvcTimeSyncPreInit,
    vgsvcTimeSyncOption,
    vgsvcTimeSyncInit,
    vgsvcTimeSyncWorker,
    vgsvcTimeSyncStop,
    vgsvcTimeSyncTerm
};

