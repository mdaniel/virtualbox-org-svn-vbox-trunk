/* $Id$ */
/** @file
 * Linux seamless guest additions simulator in host.
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

#include <stdlib.h> /* exit() */

#include <iprt/errcore.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <VBox/VBoxGuestLib.h>

#include "../seamless.h"

static RTSEMEVENT eventSem;

/** Exit with a fatal error. */
void vbclFatalError(char *pszMessage)
{
    RTPrintf("Fatal error: %s", pszMessage);
    exit(1);
}

int VBClStartVTMonitor()
{
    return VINF_SUCCESS;
}

int VbglR3SeamlessSendRects(uint32_t cRects, PRTRECT pRects)
{
    RTPrintf("Received rectangle update (%u rectangles):\n", cRects);
    for (unsigned i = 0; i < cRects; ++i)
    {
        RTPrintf("  xLeft: %d  yTop: %d  xRight: %d  yBottom: %d\n",
                 pRects[i].xLeft, pRects[i].yTop, pRects[i].xRight,
                 pRects[i].yBottom);
    }
    return VINF_SUCCESS;
}

int VbglR3SeamlessSetCap(bool bState)
{
    RTPrintf("%s\n", bState ? "Seamless capability set"
                            : "Seamless capability unset");
    return VINF_SUCCESS;
}

int VbglR3CtlFilterMask(uint32_t u32OrMask, uint32_t u32NotMask)
{
    RTPrintf("IRQ filter mask changed.  Or mask: 0x%x.  Not mask: 0x%x\n",
             u32OrMask, u32NotMask);
    return VINF_SUCCESS;
}

int VbglR3SeamlessWaitEvent(VMMDevSeamlessMode *pMode)
{
    static bool active = false;

    int rc = VINF_SUCCESS;
    if (!active)
    {
        active = true;
        *pMode = VMMDev_Seamless_Visible_Region;
    }
    else
        rc = RTSemEventWait(eventSem, RT_INDEFINITE_WAIT);
    return rc;
}

VBGLR3DECL(int)     VbglR3InitUser(void) { return VINF_SUCCESS; }
VBGLR3DECL(void)    VbglR3Term(void) {}

/**
 * Xlib error handler for certain errors that we can't avoid.
 */
int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    if (pError->error_code == BadWindow)
    {
        /* This can be triggered if a guest application destroys a window before we notice. */
        RTPrintf("ignoring BadAtom error and returning\n");
        return 0;
    }
    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    RTPrintf("An X Window protocol error occurred: %s\n"
             "  Request code: %d\n"
             "  Minor code: %d\n"
             "  Serial number of the failed request: %d\n\n"
             "exiting.\n",
             errorText, (int)pError->request_code, (int)pError->minor_code,
             (int)pError->serial);
    exit(1);
}

int main( int argc, char **argv)
{
    int rc = VINF_SUCCESS;

    RTR3InitExe(argc, &argv, 0);
    RTPrintf("VirtualBox guest additions X11 seamless mode testcase\n");
    if (0 == XInitThreads())
    {
        RTPrintf("Failed to initialise X11 threading, exiting.\n");
        exit(1);
    }
    /* Set an X11 error handler, so that we don't die when we get unavoidable errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    RTPrintf("\nType Ctrl-C to exit...\n");
    RTSemEventCreate(&eventSem);
    /** Our instance of the seamless class. */
    SeamlessMain seamless;
    LogRel(("Starting seamless Guest Additions...\n"));
    rc = seamless.init();
    if (rc != VINF_SUCCESS)
    {
        RTPrintf("Failed to initialise seamless Additions, rc = %Rrc\n", rc);
    }
    rc = seamless.run();
    if (rc != VINF_SUCCESS)
    {
        RTPrintf("Failed to run seamless Additions, rc = %Rrc\n", rc);
    }
    return rc;
}
