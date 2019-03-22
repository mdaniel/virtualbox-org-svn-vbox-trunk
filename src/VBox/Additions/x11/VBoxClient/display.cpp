/* $Id$ */
/** @file
 * X11 guest client - display management.
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
 */

#include "VBoxClient.h"

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

/** @todo this should probably be replaced by something IPRT */
/* For system() and WEXITSTATUS() */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <time.h>

/* TESTING: Dynamic resizing and mouse integration toggling should work
 * correctly with a range of X servers (pre-1.3, 1.3 and later under Linux, 1.3
 * and later under Solaris) with Guest Additions installed.  Switching to a
 * virtual terminal while a user session is in place should disable dynamic
 * resizing and cursor integration, switching back should re-enable them. */

/** Display magic number, start of a UUID. */
#define DISPLAYSTATE_MAGIC UINT32_C(0xf0029993)

/** State information needed for the service.  The main VBoxClient code provides
 *  the daemon logic needed by all services. */
struct DISPLAYSTATE
{
    /** The service interface. */
    struct VBCLSERVICE *pInterface;
    /** Magic number for sanity checks. */
    uint32_t magic;
    /** Are we initialised yet? */
    bool mfInit;
    /** The connection to the server. */
    Display *pDisplay;
    /** The RandR extension base event number. */
    int cRREventBase;
    /** Can we use version 1.2 or later of the RandR protocol here? */
    bool fHaveRandR12;
    /** The command argument to use for the xrandr binary.  Currently only
     * used to support the non-standard location on some Solaris systems -
     * would it make sense to use absolute paths on all systems? */
    const char *pcszXrandr;
    /** Was there a recent mode hint with no following root window resize, and
     *  if so, have we waited for a reasonable time? */
    time_t timeLastModeHint;
};

static unsigned char *getRootProperty(struct DISPLAYSTATE *pState, const char *pszName,
                                      long cItems, Atom type)
{
    Atom actualType = None;
    int iFormat = 0;
    unsigned long cReturned = 0;
    unsigned long cAfter = 0;
    unsigned char *pData = 0;

    if (XGetWindowProperty(pState->pDisplay, DefaultRootWindow(pState->pDisplay),
                           XInternAtom(pState->pDisplay, pszName, 0), 0, cItems,
                           False /* delete */, type, &actualType, &iFormat,
                           &cReturned, &cAfter, &pData))
        return NULL;
    return pData;
}

static void doResize(struct DISPLAYSTATE *pState)
{
    /** @note The xrandr command can fail if something else accesses RandR at
     *  the same time.  We just ignore failure for now as we do not know what
     *  someone else is doing. */
    if (!pState->fHaveRandR12)
    {
        char szCommand[256];
        unsigned char *pData;

        pData = getRootProperty(pState, "VBOXVIDEO_PREFERRED_MODE", 1, XA_INTEGER);
        if (pData != NULL)
        {
            RTStrPrintf(szCommand, sizeof(szCommand), "%s -s %ux%u",
                        pState->pcszXrandr, ((unsigned long *)pData)[0] >> 16, ((unsigned long *)pData)[0] & 0xFFFF);
            int rcShutUpGcc = system(szCommand); RT_NOREF_PV(rcShutUpGcc);
            XFree(pData);
        }
    }
    else
    {
        const char szCommandBase[] =
            "%s --output VGA-0 --auto --output VGA-1 --auto --right-of VGA-0 "
               "--output VGA-2 --auto --right-of VGA-1 --output VGA-3 --auto --right-of VGA-2 "
               "--output VGA-4 --auto --right-of VGA-3 --output VGA-5 --auto --right-of VGA-4 "
               "--output VGA-6 --auto --right-of VGA-5 --output VGA-7 --auto --right-of VGA-6 "
               "--output VGA-8 --auto --right-of VGA-7 --output VGA-9 --auto --right-of VGA-8 "
               "--output VGA-10 --auto --right-of VGA-9 --output VGA-11 --auto --right-of VGA-10 "
               "--output VGA-12 --auto --right-of VGA-11 --output VGA-13 --auto --right-of VGA-12 "
               "--output VGA-14 --auto --right-of VGA-13 --output VGA-15 --auto --right-of VGA-14 "
               "--output VGA-16 --auto --right-of VGA-15 --output VGA-17 --auto --right-of VGA-16 "
               "--output VGA-18 --auto --right-of VGA-17 --output VGA-19 --auto --right-of VGA-18 "
               "--output VGA-20 --auto --right-of VGA-19 --output VGA-21 --auto --right-of VGA-20 "
               "--output VGA-22 --auto --right-of VGA-21 --output VGA-23 --auto --right-of VGA-22 "
               "--output VGA-24 --auto --right-of VGA-23 --output VGA-25 --auto --right-of VGA-24 "
               "--output VGA-26 --auto --right-of VGA-25 --output VGA-27 --auto --right-of VGA-26 "
               "--output VGA-28 --auto --right-of VGA-27 --output VGA-29 --auto --right-of VGA-28 "
               "--output VGA-30 --auto --right-of VGA-29 --output VGA-31 --auto --right-of VGA-30";
        char szCommand[sizeof(szCommandBase) + 256];
        RTStrPrintf(szCommand, sizeof(szCommand), szCommandBase, pState->pcszXrandr);
        int rcShutUpGcc = system(szCommand); RT_NOREF_PV(rcShutUpGcc);
    }
}

/** Main loop: handle display hot-plug events, property updates (which can
 *  signal VT switches hot-plug in old X servers). */
static void runDisplay(struct DISPLAYSTATE *pState)
{
    Display *pDisplay = pState->pDisplay;
    long cValue = 1;

    /* One way or another we want the preferred mode at server start-up. */
    doResize(pState);
    XSelectInput(pDisplay, DefaultRootWindow(pDisplay), PropertyChangeMask | StructureNotifyMask);
    if (pState->fHaveRandR12)
        XRRSelectInput(pDisplay, DefaultRootWindow(pDisplay), RRScreenChangeNotifyMask);
    /* Semantics: when VBOXCLIENT_STARTED is set, pre-1.3 X.Org Server driver
     * assumes that a client capable of handling mode hints will be present for the
     * rest of the X session.  If we crash things will not work as they should.
     * I thought that preferable to implementing complex crash-handling logic.
     */
    XChangeProperty(pState->pDisplay, DefaultRootWindow(pState->pDisplay), XInternAtom(pState->pDisplay, "VBOXCLIENT_STARTED", 0),
                    XA_INTEGER, 32, PropModeReplace, (unsigned char *)&cValue, 1);
    /* Interrupting this cleanly will be more work than making it robust
     * against spontaneous termination, especially as it will never get
     * properly tested, so I will go for the second. */
    while (true)
    {
        XEvent event;
        struct pollfd PollFd;
        int pollTimeOut = -1;
        int cFds;

        /* Do not handle overflow. */
        if (pState->timeLastModeHint > 0 && pState->timeLastModeHint < INT_MAX - 2)
            pollTimeOut = 2 - (time(0) - pState->timeLastModeHint);
        PollFd.fd = ConnectionNumber(pDisplay);
        PollFd.events = POLLIN;  /* Hang-up is always reported. */
        XFlush(pDisplay);
        cFds = poll(&PollFd, 1, pollTimeOut >= 0 ? pollTimeOut * 1000 : -1);
        while (XPending(pDisplay))
        {
            XNextEvent(pDisplay, &event);
            /* This property is deleted when the server regains the virtual
             * terminal.  Force the main thread to call xrandr again, as old X
             * servers could not handle it while switched out. */
            if (   !pState->fHaveRandR12
                && event.type == PropertyNotify
                && event.xproperty.state == PropertyDelete
                && event.xproperty.window == DefaultRootWindow(pDisplay)
                && event.xproperty.atom == XInternAtom(pDisplay, "VBOXVIDEO_NO_VT", False))
                doResize(pState);
            if (   !pState->fHaveRandR12
                && event.type == PropertyNotify
                && event.xproperty.state == PropertyNewValue
                && event.xproperty.window == DefaultRootWindow(pDisplay)
                && event.xproperty.atom == XInternAtom(pDisplay, "VBOXVIDEO_PREFERRED_MODE", False))
                doResize(pState);
            if (   pState->fHaveRandR12
                && event.type == pState->cRREventBase + RRScreenChangeNotify)
                pState->timeLastModeHint = time(0);
            if (   event.type == ConfigureNotify
                && event.xproperty.window == DefaultRootWindow(pDisplay))
                pState->timeLastModeHint = 0;
        }
        if (cFds == 0 && pState->timeLastModeHint > 0)
            doResize(pState);
    }
}

static int initDisplay(struct DISPLAYSTATE *pState)
{
    char szCommand[256];
    int status;

    pState->pDisplay = XOpenDisplay(NULL);
    if (!pState->pDisplay)
        return VERR_NOT_FOUND;
    if (!XRRQueryExtension(pState->pDisplay, &pState->cRREventBase, &status))
        return VERR_NOT_FOUND;
    pState->fHaveRandR12 = false;
    pState->pcszXrandr = "xrandr";
    if (RTFileExists("/usr/X11/bin/xrandr"))
        pState->pcszXrandr = "/usr/X11/bin/xrandr";
    status = system(pState->pcszXrandr);
    if (WEXITSTATUS(status) != 0)  /* Utility or extension not available. */
        VBClFatalError(("Failed to execute the xrandr utility.\n"));
    RTStrPrintf(szCommand, sizeof(szCommand), "%s --q12", pState->pcszXrandr);
    status = system(szCommand);
    if (WEXITSTATUS(status) == 0)
        pState->fHaveRandR12 = true;
    return VINF_SUCCESS;
}

static const char *getPidFilePath()
{
    return ".vboxclient-display.pid";
}

static struct DISPLAYSTATE *getStateFromInterface(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = (struct DISPLAYSTATE *)ppInterface;
    if (pSelf->magic != DISPLAYSTATE_MAGIC)
        VBClFatalError(("Bad display service object!\n"));
    return pSelf;
}

static int init(struct VBCLSERVICE **ppInterface)
{
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);
    int rc;

    if (pSelf->mfInit)
        return VERR_WRONG_ORDER;
    rc = initDisplay(pSelf);
    if (RT_FAILURE(rc))
        return rc;
    if (RT_SUCCESS(rc))
        pSelf->mfInit = true;
    return rc;
}

static int run(struct VBCLSERVICE **ppInterface, bool fDaemonised)
{
    RT_NOREF1(fDaemonised);
    struct DISPLAYSTATE *pSelf = getStateFromInterface(ppInterface);

    if (!pSelf->mfInit)
        return VERR_WRONG_ORDER;
    runDisplay(pSelf);
    return VERR_INTERNAL_ERROR;  /* "Should never reach here." */
}

struct VBCLSERVICE vbclDisplayInterface =
{
    getPidFilePath,
    init,
    run,
    VBClServiceDefaultCleanup
};

struct VBCLSERVICE **VBClGetDisplayService()
{
    struct DISPLAYSTATE *pService = (struct DISPLAYSTATE *)RTMemAlloc(sizeof(*pService));

    if (!pService)
        VBClFatalError(("Out of memory\n"));
    pService->pInterface = &vbclDisplayInterface;
    pService->magic = DISPLAYSTATE_MAGIC;
    pService->mfInit = false;
    return &pService->pInterface;
}
