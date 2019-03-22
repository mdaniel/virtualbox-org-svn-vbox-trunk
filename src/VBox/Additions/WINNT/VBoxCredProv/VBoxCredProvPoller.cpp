/* $Id$ */
/** @file
 * VBoxCredPoller - Thread for querying / retrieving user credentials.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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
#include <iprt/win/windows.h>

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/string.h>

#include "VBoxCredProvProvider.h"

#include "VBoxCredProvCredential.h"
#include "VBoxCredProvPoller.h"
#include "VBoxCredProvUtils.h"


VBoxCredProvPoller::VBoxCredProvPoller(void) :
    m_hThreadPoller(NIL_RTTHREAD),
    m_pProv(NULL)
{
}


VBoxCredProvPoller::~VBoxCredProvPoller(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Destroying ...\n");

    Shutdown();
}


int
VBoxCredProvPoller::Initialize(VBoxCredProvProvider *pProvider)
{
    AssertPtrReturn(pProvider, VERR_INVALID_POINTER);

    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Initializing\n");

    /* Don't create more than one of them. */
    if (m_hThreadPoller != NIL_RTTHREAD)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Thread already running, returning\n");
        return VINF_SUCCESS;
    }

    if (m_pProv != NULL)
        m_pProv->Release();

    m_pProv = pProvider;
    /*
     * We must not add a reference via AddRef() here, otherwise
     * the credential provider does not get destructed properly.
     * In order to get this thread terminated normally the credential
     * provider has to call Shutdown().
     */

    /* Create the poller thread. */
    int rc = RTThreadCreate(&m_hThreadPoller, VBoxCredProvPoller::threadPoller, this, 0, RTTHREADTYPE_INFREQUENT_POLLER,
                            RTTHREADFLAGS_WAITABLE, "credpoll");
    if (RT_FAILURE(rc))
        VBoxCredProvVerbose(0, "VBoxCredProvPoller::Initialize: Failed to create thread, rc=%Rrc\n", rc);

    return rc;
}


int
VBoxCredProvPoller::Shutdown(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Shutdown\n");

    if (m_hThreadPoller == NIL_RTTHREAD)
        return VINF_SUCCESS;

    /* Post termination event semaphore. */
    int rc = RTThreadUserSignal(m_hThreadPoller);
    if (RT_SUCCESS(rc))
    {
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Waiting for thread to terminate\n");
        /* Wait until the thread has terminated. */
        rc = RTThreadWait(m_hThreadPoller, RT_INDEFINITE_WAIT, NULL);
        if (RT_FAILURE(rc))
            VBoxCredProvVerbose(0, "VBoxCredProvPoller: Wait returned error rc=%Rrc\n", rc);
    }
    else
        VBoxCredProvVerbose(0, "VBoxCredProvPoller: Error waiting for thread shutdown, rc=%Rrc\n", rc);

    m_pProv = NULL;
    m_hThreadPoller = NIL_RTTHREAD;

    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Shutdown returned with rc=%Rrc\n", rc);
    return rc;
}


/*static*/ DECLCALLBACK(int)
VBoxCredProvPoller::threadPoller(RTTHREAD hThreadSelf, void *pvUser)
{
    VBoxCredProvVerbose(0, "VBoxCredProvPoller: Starting, pvUser=0x%p\n", pvUser);

    VBoxCredProvPoller *pThis = (VBoxCredProvPoller*)pvUser;
    AssertPtr(pThis);

    for (;;)
    {
        int rc;
        rc = VbglR3CredentialsQueryAvailability();
        if (RT_FAILURE(rc))
        {
            if (rc != VERR_NOT_FOUND)
                VBoxCredProvVerbose(0, "VBoxCredProvPoller: Could not retrieve credentials! rc=%Rc\n", rc);
        }
        else
        {
            VBoxCredProvVerbose(0, "VBoxCredProvPoller: Credentials available, notifying provider\n");

            if (pThis->m_pProv)
                pThis->m_pProv->OnCredentialsProvided();
        }

        /* Wait a bit. */
        if (RTThreadUserWait(hThreadSelf, 500) == VINF_SUCCESS)
        {
            VBoxCredProvVerbose(0, "VBoxCredProvPoller: Terminating\n");
            break;
        }
    }

    return VINF_SUCCESS;
}

