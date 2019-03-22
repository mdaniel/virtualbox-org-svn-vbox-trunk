/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Events.
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
#include <VBox/log.h>
#include <iprt/assert.h>
#include "VBoxGuestR3LibInternal.h"


/**
 * Wait for the host to signal one or more events and return which.
 *
 * The events will only be delivered by the host if they have been enabled
 * previously using @a VbglR3CtlFilterMask.  If one or several of the events
 * have already been signalled but not yet waited for, this function will return
 * immediately and return those events.
 *
 * @returns IPRT status code.
 *
 * @param   fMask       The events we want to wait for, or-ed together.
 * @param   cMillies    How long to wait before giving up and returning
 *                      (VERR_TIMEOUT). Use RT_INDEFINITE_WAIT to wait until we
 *                      are interrupted or one of the events is signalled.
 * @param   pfEvents    Where to store the events signalled. Optional.
 */
VBGLR3DECL(int) VbglR3WaitEvent(uint32_t fMask, uint32_t cMillies, uint32_t *pfEvents)
{
    LogFlow(("VbglR3WaitEvent: fMask=%#x, cMillies=%u, pfEvents=%p\n", fMask, cMillies, pfEvents));
    AssertReturn((fMask & ~VMMDEV_EVENT_VALID_EVENT_MASK) == 0, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfEvents, VERR_INVALID_POINTER);

    VBGLIOCWAITFOREVENTS WaitEvents;
    VBGLREQHDR_INIT(&WaitEvents.Hdr, WAIT_FOR_EVENTS);
    WaitEvents.u.In.fEvents     = fMask;
    WaitEvents.u.In.cMsTimeOut  = cMillies;
    int rc = vbglR3DoIOCtl(VBGL_IOCTL_WAIT_FOR_EVENTS, &WaitEvents.Hdr, sizeof(WaitEvents));
    if (pfEvents)
    {
        if (RT_SUCCESS(rc))
            *pfEvents = WaitEvents.u.Out.fEvents;
        else
            *pfEvents = 0;
    }

    LogFlow(("VbglR3WaitEvent: rc=%Rrc fEvents=%#x\n", rc, WaitEvents.u.Out.fEvents));
    return rc;
}


/**
 * Causes any pending VbglR3WaitEvent calls (VBGL_IOCTL_WAIT_FOR_EVENTS) to
 * return with a VERR_INTERRUPTED status.
 *
 * Can be used in combination with a termination flag variable for interrupting
 * event loops.  After calling this, VBGL_IOCTL_WAIT_FOR_EVENTS should no longer
 * be called in the same session.  At the time of writing this is not enforced;
 * at the time of reading it may be.
 *
 * @returns IPRT status code.
 */
VBGLR3DECL(int) VbglR3InterruptEventWaits(void)
{
    VBGLREQHDR Req;
    VBGLREQHDR_INIT(&Req, INTERRUPT_ALL_WAIT_FOR_EVENTS);
    return vbglR3DoIOCtl(VBGL_IOCTL_INTERRUPT_ALL_WAIT_FOR_EVENTS, &Req, sizeof(Req));
}

