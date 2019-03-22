/* $Id$ */
/** @file
 * VirtualBox COM class implementation: Guest
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
 */

#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include "LoggingNew.h"

#include "GuestImpl.h"
#ifdef VBOX_WITH_GUEST_CONTROL
# include "GuestSessionImpl.h"
# include "GuestSessionImplTasks.h"
# include "GuestCtrlImplPrivate.h"
#endif

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#include "VBoxEvents.h"
#include "VMMDev.h"

#include "AutoCaller.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
#endif
#include <iprt/cpp/utils.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <VBox/vmm/pgm.h>
#include <VBox/AssertGuest.h>

#include <memory>


/*
 * This #ifdef goes almost to the end of the file where there are a couple of
 * IGuest method implementations.
 */
#ifdef VBOX_WITH_GUEST_CONTROL


// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Static callback function for receiving updates on guest control messages
 * from the guest. Acts as a dispatcher for the actual class instance.
 *
 * @returns VBox status code.
 * @param   pvExtension         Pointer to HGCM service extension.
 * @param   idMessage           HGCM message ID the callback was called for.
 * @param   pvData              Pointer to user-supplied callback data.
 * @param   cbData              Size (in bytes) of user-supplied callback data.
 */
/* static */
DECLCALLBACK(int) Guest::i_notifyCtrlDispatcher(void    *pvExtension,
                                                uint32_t idMessage,
                                                void    *pvData,
                                                uint32_t cbData)
{
    using namespace guestControl;

    /*
     * No locking, as this is purely a notification which does not make any
     * changes to the object state.
     */
    Log2Func(("pvExtension=%p, idMessage=%RU32, pvParms=%p, cbParms=%RU32\n", pvExtension, idMessage, pvData, cbData));

    ComObjPtr<Guest> pGuest = reinterpret_cast<Guest *>(pvExtension);
    AssertReturn(pGuest.isNotNull(), VERR_WRONG_ORDER);

    /*
     * The data packet should ever be a problem, but check to be sure.
     */
    AssertMsgReturn(cbData == sizeof(VBOXGUESTCTRLHOSTCALLBACK),
                    ("Guest control host callback data has wrong size (expected %zu, got %zu) - buggy host service!\n",
                     sizeof(VBOXGUESTCTRLHOSTCALLBACK), cbData), VERR_INVALID_PARAMETER);
    PVBOXGUESTCTRLHOSTCALLBACK pSvcCb = (PVBOXGUESTCTRLHOSTCALLBACK)pvData;
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    /*
     * For guest control 2.0 using the legacy messages we need to do the following here:
     * - Get the callback header to access the context ID
     * - Get the context ID of the callback
     * - Extract the session ID out of the context ID
     * - Dispatch the whole stuff to the appropriate session (if still exists)
     *
     * At least context ID parameter must always be present.
     */
    ASSERT_GUEST_RETURN(pSvcCb->mParms > 0, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_MSG_RETURN(pSvcCb->mpaParms[0].type == VBOX_HGCM_SVC_PARM_32BIT,
                            ("type=%d\n", pSvcCb->mpaParms[0].type), VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idContext = pSvcCb->mpaParms[0].u.uint32;

    VBOXGUESTCTRLHOSTCBCTX CtxCb = { idMessage, idContext };
    int rc = pGuest->i_dispatchToSession(&CtxCb, pSvcCb);

    Log2Func(("CID=%#x, idSession=%RU32, uObject=%RU32, uCount=%RU32, rc=%Rrc\n",
              idContext, VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(idContext), VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(idContext),
              VBOX_GUESTCTRL_CONTEXTID_GET_COUNT(idContext), rc));
    return rc;
}

// private methods
/////////////////////////////////////////////////////////////////////////////

/**
 * Dispatches a host service callback to the appropriate guest control session object.
 *
 * @returns VBox status code.
 * @param   pCtxCb              Pointer to host callback context.
 * @param   pSvcCb              Pointer to callback parameters.
 */
int Guest::i_dispatchToSession(PVBOXGUESTCTRLHOSTCBCTX pCtxCb, PVBOXGUESTCTRLHOSTCALLBACK pSvcCb)
{
    LogFlowFunc(("pCtxCb=%p, pSvcCb=%p\n", pCtxCb, pSvcCb));

    AssertPtrReturn(pCtxCb, VERR_INVALID_POINTER);
    AssertPtrReturn(pSvcCb, VERR_INVALID_POINTER);

    Log2Func(("uMessage=%RU32, uContextID=%RU32, uProtocol=%RU32\n", pCtxCb->uMessage, pCtxCb->uContextID, pCtxCb->uProtocol));

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    const uint32_t uSessionID = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pCtxCb->uContextID);

    Log2Func(("uSessionID=%RU32 (%zu total)\n", uSessionID, mData.mGuestSessions.size()));

    GuestSessions::const_iterator itSession = mData.mGuestSessions.find(uSessionID);

    int rc;
    if (itSession != mData.mGuestSessions.end())
    {
        ComObjPtr<GuestSession> pSession(itSession->second);
        Assert(!pSession.isNull());

        alock.release();

#ifdef DEBUG
        /*
         * Pre-check: If we got a status message with an error and VERR_TOO_MUCH_DATA
         *            it means that that guest could not handle the entire message
         *            because of its exceeding size. This should not happen on daily
         *            use but testcases might try this. It then makes no sense to dispatch
         *            this further because we don't have a valid context ID.
         */
        bool fDispatch = true;
        rc = VERR_INVALID_FUNCTION;
        if (   pCtxCb->uMessage == GUEST_MSG_EXEC_STATUS
            && pSvcCb->mParms    >= 5)
        {
            CALLBACKDATA_PROC_STATUS dataCb;
            /* pSvcCb->mpaParms[0] always contains the context ID. */
            HGCMSvcGetU32(&pSvcCb->mpaParms[1], &dataCb.uPID);
            HGCMSvcGetU32(&pSvcCb->mpaParms[2], &dataCb.uStatus);
            HGCMSvcGetU32(&pSvcCb->mpaParms[3], &dataCb.uFlags);
            HGCMSvcGetPv(&pSvcCb->mpaParms[4], &dataCb.pvData, &dataCb.cbData);

            if (   dataCb.uStatus == PROC_STS_ERROR
                && (int32_t)dataCb.uFlags == VERR_TOO_MUCH_DATA)
            {
                LogFlowFunc(("Requested message with too much data, skipping dispatching ...\n"));
                Assert(dataCb.uPID == 0);
                fDispatch = false;
            }
        }
        if (fDispatch)
#endif
        {
            switch (pCtxCb->uMessage)
            {
                case GUEST_MSG_DISCONNECTED:
                    rc = pSession->i_dispatchToThis(pCtxCb, pSvcCb);
                    break;

                /* Process stuff. */
                case GUEST_MSG_EXEC_STATUS:
                case GUEST_MSG_EXEC_OUTPUT:
                case GUEST_MSG_EXEC_INPUT_STATUS:
                case GUEST_MSG_EXEC_IO_NOTIFY:
                    rc = pSession->i_dispatchToObject(pCtxCb, pSvcCb);
                    break;

                /* File stuff. */
                case GUEST_MSG_FILE_NOTIFY:
                    rc = pSession->i_dispatchToObject(pCtxCb, pSvcCb);
                    break;

                /* Session stuff. */
                case GUEST_MSG_SESSION_NOTIFY:
                    rc = pSession->i_dispatchToThis(pCtxCb, pSvcCb);
                    break;

                default:
                    rc = pSession->i_dispatchToObject(pCtxCb, pSvcCb);
                    break;
            }
        }
    }
    else
        rc = VERR_INVALID_SESSION_ID;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Removes a guest control session from the internal list and destroys the session.
 *
 * @returns VBox status code.
 * @param   uSessionID          ID of the guest control session to remove.
 */
int Guest::i_sessionRemove(uint32_t uSessionID)
{
    LogFlowThisFuncEnter();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_NOT_FOUND;

    LogFlowThisFunc(("Removing session (ID=%RU32) ...\n", uSessionID));

    GuestSessions::iterator itSessions = mData.mGuestSessions.find(uSessionID);
    if (itSessions == mData.mGuestSessions.end())
        return VERR_NOT_FOUND;

    /* Make sure to consume the pointer before the one of the
     * iterator gets released. */
    ComObjPtr<GuestSession> pSession = itSessions->second;

    LogFlowThisFunc(("Removing session %RU32 (now total %ld sessions)\n",
                     uSessionID, mData.mGuestSessions.size() ? mData.mGuestSessions.size() - 1 : 0));

    rc = pSession->i_onRemove();
    mData.mGuestSessions.erase(itSessions);

    alock.release(); /* Release lock before firing off event. */

    fireGuestSessionRegisteredEvent(mEventSource, pSession, false /* Unregistered */);
    pSession.setNull();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a new guest session.
 * This will invoke VBoxService running on the guest creating a new (dedicated) guest session
 * On older Guest Additions this call has no effect on the guest, and only the credentials will be
 * used for starting/impersonating guest processes.
 *
 * @returns VBox status code.
 * @param   ssInfo              Guest session startup information.
 * @param   guestCreds          Guest OS (user) credentials to use on the guest for creating the session.
 *                              The specified user must be able to logon to the guest and able to start new processes.
 * @param   pGuestSession       Where to store the created guest session on success.
 */
int Guest::i_sessionCreate(const GuestSessionStartupInfo &ssInfo,
                           const GuestCredentials &guestCreds, ComObjPtr<GuestSession> &pGuestSession)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    int rc = VERR_MAX_PROCS_REACHED;
    if (mData.mGuestSessions.size() >= VBOX_GUESTCTRL_MAX_SESSIONS)
        return rc;

    try
    {
        /* Create a new session ID and assign it. */
        uint32_t uNewSessionID = VBOX_GUESTCTRL_SESSION_ID_BASE;
        uint32_t uTries = 0;

        for (;;)
        {
            /* Is the context ID already used? */
            if (!i_sessionExists(uNewSessionID))
            {
                rc = VINF_SUCCESS;
                break;
            }
            uNewSessionID++;
            if (uNewSessionID >= VBOX_GUESTCTRL_MAX_SESSIONS)
                uNewSessionID = VBOX_GUESTCTRL_SESSION_ID_BASE;

            if (++uTries == VBOX_GUESTCTRL_MAX_SESSIONS)
                break; /* Don't try too hard. */
        }
        if (RT_FAILURE(rc)) throw rc;

        /* Create the session object. */
        HRESULT hr = pGuestSession.createObject();
        if (FAILED(hr)) throw VERR_COM_UNEXPECTED;

        /** @todo Use an overloaded copy operator. Later. */
        GuestSessionStartupInfo startupInfo;
        startupInfo.mID = uNewSessionID; /* Assign new session ID. */
        startupInfo.mName = ssInfo.mName;
        startupInfo.mOpenFlags = ssInfo.mOpenFlags;
        startupInfo.mOpenTimeoutMS = ssInfo.mOpenTimeoutMS;

        GuestCredentials guestCredentials;
        if (!guestCreds.mUser.isEmpty())
        {
            /** @todo Use an overloaded copy operator. Later. */
            guestCredentials.mUser = guestCreds.mUser;
            guestCredentials.mPassword = guestCreds.mPassword;
            guestCredentials.mDomain = guestCreds.mDomain;
        }
        else
        {
            /* Internal (annonymous) session. */
            startupInfo.mIsInternal = true;
        }

        rc = pGuestSession->init(this, startupInfo, guestCredentials);
        if (RT_FAILURE(rc)) throw rc;

        /*
         * Add session object to our session map. This is necessary
         * before calling openSession because the guest calls back
         * with the creation result of this session.
         */
        mData.mGuestSessions[uNewSessionID] = pGuestSession;

        alock.release(); /* Release lock before firing off event. */

        fireGuestSessionRegisteredEvent(mEventSource, pGuestSession,
                                        true /* Registered */);
    }
    catch (int rc2)
    {
        rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns whether a guest control session with a specific ID exists or not.
 *
 * @returns Returns \c true if the session exists, \c false if not.
 * @param   uSessionID          ID to check for.
 */
inline bool Guest::i_sessionExists(uint32_t uSessionID)
{
    GuestSessions::const_iterator itSessions = mData.mGuestSessions.find(uSessionID);
    return (itSessions == mData.mGuestSessions.end()) ? false : true;
}

#endif /* VBOX_WITH_GUEST_CONTROL */


// implementation of public methods
/////////////////////////////////////////////////////////////////////////////
HRESULT Guest::createSession(const com::Utf8Str &aUser, const com::Utf8Str &aPassword, const com::Utf8Str &aDomain,
                             const com::Utf8Str &aSessionName, ComPtr<IGuestSession> &aGuestSession)

{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* Do not allow anonymous sessions (with system rights) with public API. */
    if (RT_UNLIKELY(!aUser.length()))
        return setError(E_INVALIDARG, tr("No user name specified"));

    LogFlowFuncEnter();

    GuestSessionStartupInfo startupInfo;
    startupInfo.mName = aSessionName;

    GuestCredentials guestCreds;
    guestCreds.mUser = aUser;
    guestCreds.mPassword = aPassword;
    guestCreds.mDomain = aDomain;

    ComObjPtr<GuestSession> pSession;
    int vrc = i_sessionCreate(startupInfo, guestCreds, pSession);
    if (RT_SUCCESS(vrc))
    {
        /* Return guest session to the caller. */
        HRESULT hr2 = pSession.queryInterfaceTo(aGuestSession.asOutParam());
        if (FAILED(hr2))
            vrc = VERR_COM_OBJECT_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
        /* Start (fork) the session asynchronously
         * on the guest. */
        vrc = pSession->i_startSessionAsync();

    HRESULT hr = S_OK;

    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setErrorBoth(VBOX_E_MAXIMUM_REACHED, vrc, tr("Maximum number of concurrent guest sessions (%d) reached"),
                                  VBOX_GUESTCTRL_MAX_SESSIONS);
                break;

            /** @todo Add more errors here. */

            default:
                hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Could not create guest session: %Rrc"), vrc);
                break;
        }
    }

    LogFlowThisFunc(("Returning rc=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT Guest::findSession(const com::Utf8Str &aSessionName, std::vector<ComPtr<IGuestSession> > &aSessions)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */

    LogFlowFuncEnter();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Utf8Str strName(aSessionName);
    std::list < ComObjPtr<GuestSession> > listSessions;

    GuestSessions::const_iterator itSessions = mData.mGuestSessions.begin();
    while (itSessions != mData.mGuestSessions.end())
    {
        if (strName.contains(itSessions->second->i_getName())) /** @todo Use a (simple) pattern match (IPRT?). */
            listSessions.push_back(itSessions->second);
        ++itSessions;
    }

    LogFlowFunc(("Sessions with \"%s\" = %RU32\n",
                 aSessionName.c_str(), listSessions.size()));

    aSessions.resize(listSessions.size());
    if (!listSessions.empty())
    {
        size_t i = 0;
        for (std::list < ComObjPtr<GuestSession> >::const_iterator it = listSessions.begin(); it != listSessions.end(); ++it, ++i)
            (*it).queryInterfaceTo(aSessions[i].asOutParam());

        return S_OK;

    }

    return setErrorNoLog(VBOX_E_OBJECT_NOT_FOUND,
                         tr("Could not find sessions with name '%s'"),
                         aSessionName.c_str());
#endif /* VBOX_WITH_GUEST_CONTROL */
}

HRESULT Guest::updateGuestAdditions(const com::Utf8Str &aSource, const std::vector<com::Utf8Str> &aArguments,
                                    const std::vector<AdditionsUpdateFlag_T> &aFlags, ComPtr<IProgress> &aProgress)
{
#ifndef VBOX_WITH_GUEST_CONTROL
    ReturnComNotImplemented();
#else /* VBOX_WITH_GUEST_CONTROL */

    /* Validate flags. */
    uint32_t fFlags = AdditionsUpdateFlag_None;
    if (aFlags.size())
        for (size_t i = 0; i < aFlags.size(); ++i)
            fFlags |= aFlags[i];

    if (fFlags && !(fFlags & AdditionsUpdateFlag_WaitForUpdateStartOnly))
        return setError(E_INVALIDARG, tr("Unknown flags (%#x)"), fFlags);

    int vrc = VINF_SUCCESS;

    ProcessArguments aArgs;
    aArgs.resize(0);

    if (aArguments.size())
    {
        try
        {
            for (size_t i = 0; i < aArguments.size(); ++i)
                aArgs.push_back(aArguments[i]);
        }
        catch(std::bad_alloc &)
        {
            vrc = VERR_NO_MEMORY;
        }
    }

    HRESULT hr = S_OK;

    /*
     * Create an anonymous session. This is required to run the Guest Additions
     * update process with administrative rights.
     */
    GuestSessionStartupInfo startupInfo;
    startupInfo.mName = "Updating Guest Additions";

    GuestCredentials guestCreds;
    RT_ZERO(guestCreds);

    ComObjPtr<GuestSession> pSession;
    if (RT_SUCCESS(vrc))
        vrc = i_sessionCreate(startupInfo, guestCreds, pSession);
    if (RT_FAILURE(vrc))
    {
        switch (vrc)
        {
            case VERR_MAX_PROCS_REACHED:
                hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Maximum number of concurrent guest sessions (%d) reached"),
                                  VBOX_GUESTCTRL_MAX_SESSIONS);
                break;

            /** @todo Add more errors here. */

           default:
                hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Could not create guest session: %Rrc"), vrc);
                break;
        }
    }
    else
    {
        Assert(!pSession.isNull());
        int rcGuest;
        vrc = pSession->i_startSession(&rcGuest);
        if (RT_FAILURE(vrc))
        {
            /** @todo Handle rcGuest! */

            hr = setErrorBoth(VBOX_E_IPRT_ERROR, vrc, tr("Could not open guest session: %Rrc"), vrc);
        }
        else
        {

            ComObjPtr<Progress> pProgress;
            GuestSessionTaskUpdateAdditions *pTask = NULL;
            try
            {
                try
                {
                    pTask = new GuestSessionTaskUpdateAdditions(pSession /* GuestSession */, aSource, aArgs, fFlags);
                }
                catch(...)
                {
                    hr = setError(E_OUTOFMEMORY, tr("Failed to create SessionTaskUpdateAdditions object "));
                    throw;
                }


                hr = pTask->Init(Utf8StrFmt(tr("Updating Guest Additions")));
                if (FAILED(hr))
                {
                    delete pTask;
                    hr = setError(hr, tr("Creating progress object for SessionTaskUpdateAdditions object failed"));
                    throw hr;
                }

                hr = pTask->createThreadWithType(RTTHREADTYPE_MAIN_HEAVY_WORKER);

                if (SUCCEEDED(hr))
                {
                    /* Return progress to the caller. */
                    pProgress = pTask->GetProgressObject();
                    hr = pProgress.queryInterfaceTo(aProgress.asOutParam());
                }
                else
                    hr = setError(hr, tr("Starting thread for updating Guest Additions on the guest failed "));
            }
            catch(std::bad_alloc &)
            {
                hr = E_OUTOFMEMORY;
            }
            catch(...)
            {
                LogFlowThisFunc(("Exception was caught in the function\n"));
            }
        }
    }

    LogFlowFunc(("Returning hr=%Rhrc\n", hr));
    return hr;
#endif /* VBOX_WITH_GUEST_CONTROL */
}

