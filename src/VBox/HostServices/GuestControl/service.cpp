/* $Id$ */
/** @file
 * Guest Control Service: Controlling the guest.
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_svc_guest_control   Guest Control HGCM Service
 *
 * This service acts as a proxy for handling and buffering host command requests
 * and clients on the guest. It tries to be as transparent as possible to let
 * the guest (client) and host side do their protocol handling as desired.
 *
 * The following terms are used:
 * - Host:   A host process (e.g. VBoxManage or another tool utilizing the Main API)
 *           which wants to control something on the guest.
 * - Client: A client (e.g. VBoxService) running inside the guest OS waiting for
 *           new host commands to perform. There can be multiple clients connected
 *           to this service. A client is represented by its unique HGCM client ID.
 * - Context ID: An (almost) unique ID automatically generated on the host (Main API)
 *               to not only distinguish clients but individual requests. Because
 *               the host does not know anything about connected clients it needs
 *               an indicator which it can refer to later. This context ID gets
 *               internally bound by the service to a client which actually processes
 *               the command in order to have a relationship between client<->context ID(s).
 *
 * The host can trigger commands which get buffered by the service (with full HGCM
 * parameter info). As soon as a client connects (or is ready to do some new work)
 * it gets a buffered host command to process it. This command then will be immediately
 * removed from the command list. If there are ready clients but no new commands to be
 * processed, these clients will be set into a deferred state (that is being blocked
 * to return until a new command is available).
 *
 * If a client needs to inform the host that something happened, it can send a
 * message to a low level HGCM callback registered in Main. This callback contains
 * the actual data as well as the context ID to let the host do the next necessary
 * steps for this context. This context ID makes it possible to wait for an event
 * inside the host's Main API function (like starting a process on the guest and
 * wait for getting its PID returned by the client) as well as cancelling blocking
 * host calls in order the client terminated/crashed (HGCM detects disconnected
 * clients and reports it to this service's callback).
 *
 * Starting at VBox 4.2 the context ID itself consists of a session ID, an object
 * ID (for example a process or file ID) and a count. This is necessary to not break
 * compatibility between older hosts and to manage guest session on the host.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_GUEST_CONTROL
#include <VBox/HostServices/GuestControlSvc.h>
#include <VBox/GuestHost/GuestControl.h> /** @todo r=bird: Why two headers??? */

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/AssertGuest.h>
#include <VBox/VMMDev.h>
#include <VBox/vmm/ssm.h>
#include <iprt/assert.h>
#include <iprt/cpp/autores.h>
#include <iprt/cpp/utils.h>
#include <iprt/mem.h>
#include <iprt/list.h>
#include <iprt/req.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <map>
#include <new>      /* for std::nothrow*/


using namespace guestControl;


/**
 * Structure for maintaining a request.
 */
typedef struct ClientRequest
{
    /** The call handle */
    VBOXHGCMCALLHANDLE mHandle;
    /** Number of parameters */
    uint32_t mNumParms;
    /** The call parameters */
    VBOXHGCMSVCPARM *mParms;
    /** The default constructor. */
    ClientRequest(void)
        : mHandle(0), mNumParms(0), mParms(NULL)
    {}
} ClientRequest;

/**
 * Structure for holding a buffered host command which has
 * not been processed yet.
 *
 * @todo r=bird: It would be nice if we could decide on _one_ term for what the
 *       host passes to the guest.  We currently have:
 *          - The enum is called eHostFn, implying it's a function
 *          - The guest retrieves messages, if the eGuestFn enum is anything to
 *            go by: GUEST_MSG_GET, GUEST_MSG_CANCEL, GUEST_MSG_XXX
 *          - Here it's called a host command.
 *          - But this HostCommand structure has a mMsgType rather than command
 *            number/enum value, impliying it's a message.
 */
typedef struct HostCommand
{
    /** Entry on the ClientState::m_HostCmdList list. */
    RTLISTNODE      m_ListEntry;
    union
    {
        /** The top two twomost bits are exploited for message destination.
         * See VBOX_GUESTCTRL_DST_XXX.  */
        uint64_t    m_idContextAndDst;
        /** The context ID this command belongs to (extracted from the first parameter). */
        uint32_t    m_idContext;
    };
    /** Dynamic structure for holding the HGCM parms */
    uint32_t mMsgType;
    /** Number of HGCM parameters. */
    uint32_t mParmCount;
    /** Array of HGCM parameters. */
    PVBOXHGCMSVCPARM mpParms;
    /** Set if we detected the message skipping hack from r121400. */
    bool m_f60BetaHackInPlay;

    HostCommand()
        : m_idContextAndDst(0)
        , mMsgType(UINT32_MAX)
        , mParmCount(0)
        , mpParms(NULL)
        , m_f60BetaHackInPlay(false)
    {
        RTListInit(&m_ListEntry);
    }

    /**
     * Releases the host command, properly deleting it if no further references.
     */
    void Delete(void)
    {
        LogFlowThisFunc(("[Cmd %RU32 (%s)] destroying\n", mMsgType, GstCtrlHostFnName((eHostFn)mMsgType)));
        Assert(m_ListEntry.pNext == NULL);
        if (mpParms)
        {
            for (uint32_t i = 0; i < mParmCount; i++)
                if (mpParms[i].type == VBOX_HGCM_SVC_PARM_PTR)
                {
                    RTMemFree(mpParms[i].u.pointer.addr);
                    mpParms[i].u.pointer.addr = NULL;
                }
            RTMemFree(mpParms);
            mpParms = NULL;
        }
        mParmCount = 0;
        delete this;
    }


    /**
     * Initializes the command.
     *
     * The specified parameters are copied and any buffers referenced by it
     * duplicated as well.
     *
     * @returns VBox status code.
     * @param   idFunction  The host function (message) number, eHostFn.
     * @param   cParms      Number of parameters in the HGCM request.
     * @param   paParms     Array of parameters.
     */
    int Init(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
    {
        LogFlowThisFunc(("[Cmd %RU32 (%s)] Allocating cParms=%RU32, paParms=%p\n",
                         idFunction, GstCtrlHostFnName((eHostFn)idFunction), cParms, paParms));
        Assert(mpParms == NULL);
        Assert(mParmCount == 0);
        Assert(RTListIsEmpty(&m_ListEntry));

        /*
         * Fend of bad stuff.
         */
        AssertReturn(cParms > 0, VERR_WRONG_PARAMETER_COUNT); /* At least one parameter (context ID) must be present. */
        AssertReturn(cParms < VMMDEV_MAX_HGCM_PARMS, VERR_WRONG_PARAMETER_COUNT);
        AssertPtrReturn(paParms, VERR_INVALID_POINTER);

        /*
         * The first parameter is the context ID and the command destiation mask.
         */
        if (paParms[0].type == VBOX_HGCM_SVC_PARM_64BIT)
        {
            m_idContextAndDst = paParms[0].u.uint64;
            AssertReturn(m_idContextAndDst & VBOX_GUESTCTRL_DST_BOTH, VERR_INTERNAL_ERROR_3);
        }
        else if (paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT)
        {
            AssertMsgFailed(("idFunction=%u %s - caller must set dst!\n", idFunction, GstCtrlHostFnName((eHostFn)idFunction)));
            m_idContextAndDst = paParms[0].u.uint32 | VBOX_GUESTCTRL_DST_BOTH;
        }
        else
            AssertFailedReturn(VERR_WRONG_PARAMETER_TYPE);

        /*
         * Just make a copy of the parameters and any buffers.
         */
        mMsgType   = idFunction;
        mParmCount = cParms;
        mpParms    = (VBOXHGCMSVCPARM *)RTMemAllocZ(sizeof(VBOXHGCMSVCPARM) * mParmCount);
        AssertReturn(mpParms, VERR_NO_MEMORY);

        for (uint32_t i = 0; i < cParms; i++)
        {
            mpParms[i].type = paParms[i].type;
            switch (paParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    mpParms[i].u.uint32 = paParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    mpParms[i].u.uint64 = paParms[i].u.uint64;
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                    mpParms[i].u.pointer.size = paParms[i].u.pointer.size;
                    if (mpParms[i].u.pointer.size > 0)
                    {
                        mpParms[i].u.pointer.addr = RTMemDup(paParms[i].u.pointer.addr, mpParms[i].u.pointer.size);
                        AssertReturn(mpParms[i].u.pointer.addr, VERR_NO_MEMORY);
                    }
                    /* else: structure is zeroed by allocator. */
                    break;

                default:
                    AssertMsgFailedReturn(("idFunction=%u (%s) parameter #%u: type=%u\n",
                                           idFunction, GstCtrlHostFnName((eHostFn)idFunction), i, paParms[i].type),
                                          VERR_WRONG_PARAMETER_TYPE);
            }
        }

        /*
         * Morph the first parameter back to 32-bit.
         */
        mpParms[0].type     = VBOX_HGCM_SVC_PARM_32BIT;
        mpParms[0].u.uint32 = (uint32_t)paParms[0].u.uint64;

        return VINF_SUCCESS;
    }


    /**
     * Sets the GUEST_MSG_PEEK_WAIT GUEST_MSG_PEEK_NOWAIT return parameters.
     *
     * @param   paDstParms  The peek parameter vector.
     * @param   cDstParms   The number of peek parameters (at least two).
     * @remarks ASSUMES the parameters has been cleared by clientMsgPeek.
     */
    inline void setPeekReturn(PVBOXHGCMSVCPARM paDstParms, uint32_t cDstParms)
    {
        Assert(cDstParms >= 2);
        if (paDstParms[0].type == VBOX_HGCM_SVC_PARM_32BIT)
            paDstParms[0].u.uint32 = mMsgType;
        else
            paDstParms[0].u.uint64 = mMsgType;
        paDstParms[1].u.uint32 = mParmCount;

        uint32_t i = RT_MIN(cDstParms, mParmCount + 2);
        while (i-- > 2)
            switch (mpParms[i - 2].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT: paDstParms[i].u.uint32 = ~(uint32_t)sizeof(uint32_t); break;
                case VBOX_HGCM_SVC_PARM_64BIT: paDstParms[i].u.uint32 = ~(uint32_t)sizeof(uint64_t); break;
                case VBOX_HGCM_SVC_PARM_PTR:   paDstParms[i].u.uint32 = mpParms[i - 2].u.pointer.size; break;
            }
    }


    /** @name Support for old-style (GUEST_MSG_WAIT) operation.
     * @{
     */

    /**
     * Worker for Assign() that opies data from the buffered HGCM request to the
     * current HGCM request.
     *
     * @returns VBox status code.
     * @param   paDstParms              Array of parameters of HGCM request to fill the data into.
     * @param   cDstParms               Number of parameters the HGCM request can handle.
     */
    int CopyTo(VBOXHGCMSVCPARM paDstParms[], uint32_t cDstParms) const
    {
        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, m_idContext=%RU32 (Session %RU32)\n",
                         mMsgType, mParmCount, m_idContext, VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(m_idContext)));

        int rc = VINF_SUCCESS;
        if (cDstParms != mParmCount)
        {
            LogFlowFunc(("Parameter count does not match (got %RU32, expected %RU32)\n",
                         cDstParms, mParmCount));
            rc = VERR_INVALID_PARAMETER;
        }

        if (RT_SUCCESS(rc))
        {
            for (uint32_t i = 0; i < mParmCount; i++)
            {
                if (paDstParms[i].type != mpParms[i].type)
                {
                    LogFunc(("Parameter %RU32 type mismatch (got %RU32, expected %RU32)\n", i, paDstParms[i].type, mpParms[i].type));
                    rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    switch (mpParms[i].type)
                    {
                        case VBOX_HGCM_SVC_PARM_32BIT:
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %RU32 (uint32_t)\n",
                                         i, mpParms[i].u.uint32));
#endif
                            paDstParms[i].u.uint32 = mpParms[i].u.uint32;
                            break;

                        case VBOX_HGCM_SVC_PARM_64BIT:
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %RU64 (uint64_t)\n",
                                         i, mpParms[i].u.uint64));
#endif
                            paDstParms[i].u.uint64 = mpParms[i].u.uint64;
                            break;

                        case VBOX_HGCM_SVC_PARM_PTR:
                        {
#ifdef DEBUG_andy
                            LogFlowFunc(("\tmpParms[%RU32] = %p (ptr), size = %RU32\n",
                                         i, mpParms[i].u.pointer.addr, mpParms[i].u.pointer.size));
#endif
                            if (!mpParms[i].u.pointer.size)
                                continue; /* Only copy buffer if there actually is something to copy. */

                            if (!paDstParms[i].u.pointer.addr)
                                rc = VERR_INVALID_PARAMETER;
                            else if (paDstParms[i].u.pointer.size < mpParms[i].u.pointer.size)
                                rc = VERR_BUFFER_OVERFLOW;
                            else
                                memcpy(paDstParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.addr,
                                       mpParms[i].u.pointer.size);
                            break;
                        }

                        default:
                            LogFunc(("Parameter %RU32 of type %RU32 is not supported yet\n", i, mpParms[i].type));
                            rc = VERR_NOT_SUPPORTED;
                            break;
                    }
                }

                if (RT_FAILURE(rc))
                {
                    LogFunc(("Parameter %RU32 invalid (%Rrc), refusing\n", i, rc));
                    break;
                }
            }
        }

        LogFlowFunc(("Returned with rc=%Rrc\n", rc));
        return rc;
    }

    int Assign(const ClientRequest *pReq)
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);

        int rc;

        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, mpParms=%p\n", mMsgType, mParmCount, mpParms));

        /* Does the current host command need more parameter space which
         * the client does not provide yet? */
        if (mParmCount > pReq->mNumParms)
        {
            LogFlowThisFunc(("[Cmd %RU32] Requires %RU32 parms, only got %RU32 from client\n",
                             mMsgType, mParmCount, pReq->mNumParms));
            /*
             * So this call apparently failed because the guest wanted to peek
             * how much parameters it has to supply in order to successfully retrieve
             * this command. Let's tell him so!
             */
            rc = VERR_TOO_MUCH_DATA;
        }
        else
        {
            rc = CopyTo(pReq->mParms, pReq->mNumParms);

            /*
             * Has there been enough parameter space but the wrong parameter types
             * were submitted -- maybe the client was just asking for the next upcoming
             * host message?
             *
             * Note: To keep this compatible to older clients we return VERR_TOO_MUCH_DATA
             *       in every case.
             */
            if (RT_FAILURE(rc))
                rc = VERR_TOO_MUCH_DATA;
        }

        return rc;
    }

    int Peek(const ClientRequest *pReq)
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);

        LogFlowThisFunc(("[Cmd %RU32] mParmCount=%RU32, mpParms=%p\n", mMsgType, mParmCount, mpParms));

        if (pReq->mNumParms >= 2)
        {
            HGCMSvcSetU32(&pReq->mParms[0], mMsgType);   /* Message ID */
            HGCMSvcSetU32(&pReq->mParms[1], mParmCount); /* Required parameters for message */
        }
        else
            LogFlowThisFunc(("Warning: Client has not (yet) submitted enough parameters (%RU32, must be at least 2) to at least peak for the next message\n",
                             pReq->mNumParms));

        /*
         * Always return VERR_TOO_MUCH_DATA data here to
         * keep it compatible with older clients and to
         * have correct accounting (mHostRc + mHostCmdTries).
         */
        return VERR_TOO_MUCH_DATA;
    }

    /** @} */
} HostCommand;

/**
 * Per-client structure used for book keeping/state tracking a
 * certain host command.
 */
typedef struct ClientContext
{
    /* Pointer to list node of this command. */
    HostCommand *mpHostCmd;
    /** The standard constructor. */
    ClientContext(void) : mpHostCmd(NULL) {}
    /** Internal constrcutor. */
    ClientContext(HostCommand *pHostCmd) : mpHostCmd(pHostCmd) {}
} ClientContext;
typedef std::map< uint32_t, ClientContext > ClientContextMap;

/**
 * Structure for holding a connected guest client state.
 */
typedef struct ClientState
{
    PVBOXHGCMSVCHELPERS     m_pSvcHelpers;
    /** Host command list to process (HostCommand). */
    RTLISTANCHOR            m_HostCmdList;
    /** The HGCM client ID. */
    uint32_t                m_idClient;
    /** The session ID for this client, UINT32_MAX if not set or master. */
    uint32_t                m_idSession;
    /** Set if master. */
    bool                    m_fIsMaster;
    /** Set if restored (needed for shutting legacy mode assert on non-masters). */
    bool                    m_fRestored;

    /** Set if we've got a pending wait cancel. */
    bool                    m_fPendingCancel;
    /** Pending client call (GUEST_MSG_PEEK_WAIT or GUEST_MSG_WAIT), zero if none pending.
     *
     * This means the client waits for a new host command to reply and won't return
     * from the waiting call until a new host command is available. */
    guestControl::eGuestFn  m_enmIsPending;
    /** Pending peek/wait request details. */
    ClientRequest           m_PendingReq;


    ClientState(void)
        : m_pSvcHelpers(NULL)
        , m_idClient(0)
        , m_idSession(UINT32_MAX)
        , m_fIsMaster(false)
        , m_fRestored(false)
        , m_fPendingCancel(false)
        , m_enmIsPending((guestControl::eGuestFn)0)
        , mHostCmdRc(VINF_SUCCESS)
        , mHostCmdTries(0)
        , mPeekCount(0)
    {
        RTListInit(&m_HostCmdList);
    }

    ClientState(PVBOXHGCMSVCHELPERS pSvcHelpers, uint32_t idClient)
        : m_pSvcHelpers(pSvcHelpers)
        , m_idClient(idClient)
        , m_idSession(UINT32_MAX)
        , m_fIsMaster(false)
        , m_fRestored(false)
        , m_fPendingCancel(false)
        , m_enmIsPending((guestControl::eGuestFn)0)
        , mHostCmdRc(VINF_SUCCESS)
        , mHostCmdTries(0)
        , mPeekCount(0)
    {
        RTListInit(&m_HostCmdList);
    }

    /**
     * Used by for Service::hostProcessCommand().
     */
    void EnqueueCommand(HostCommand *pHostCmd)
    {
        AssertPtr(pHostCmd);
        RTListAppend(&m_HostCmdList, &pHostCmd->m_ListEntry);
    }

    /**
     * Used by for Service::hostProcessCommand().
     *
     * @note This wakes up both GUEST_MSG_WAIT and GUEST_MSG_PEEK_WAIT sleepers.
     */
    int Wakeup(void)
    {
        int rc = VINF_NO_CHANGE;

        if (m_enmIsPending != 0)
        {
            LogFlowFunc(("[Client %RU32] Waking up ...\n", m_idClient));

            rc = VINF_SUCCESS;

            HostCommand *pFirstCmd = RTListGetFirstCpp(&m_HostCmdList, HostCommand, m_ListEntry);
            if (pFirstCmd)
            {
                LogFlowThisFunc(("[Client %RU32] Current host command is %RU32 (CID=%#RX32, cParms=%RU32)\n",
                                 m_idClient, pFirstCmd->mMsgType, pFirstCmd->m_idContext, pFirstCmd->mParmCount));

                if (m_enmIsPending == GUEST_MSG_PEEK_WAIT)
                {
                    pFirstCmd->setPeekReturn(m_PendingReq.mParms, m_PendingReq.mNumParms);
                    rc = m_pSvcHelpers->pfnCallComplete(m_PendingReq.mHandle, VINF_SUCCESS);

                    m_PendingReq.mHandle   = NULL;
                    m_PendingReq.mParms    = NULL;
                    m_PendingReq.mNumParms = 0;
                    m_enmIsPending         = (guestControl::eGuestFn)0;
                }
                else if (m_enmIsPending == GUEST_MSG_WAIT)
                    rc = OldRun(&m_PendingReq, pFirstCmd);
                else
                    AssertMsgFailed(("m_enmIsPending=%d\n", m_enmIsPending));
            }
            else
                AssertMsgFailed(("Waking up client ID=%RU32 with no host command in queue is a bad idea\n", m_idClient));

            return rc;
        }

        return VINF_NO_CHANGE;
    }

    /**
     * Used by Service::call() to handle GUEST_MSG_CANCEL.
     *
     * @note This cancels both GUEST_MSG_WAIT and GUEST_MSG_PEEK_WAIT sleepers.
     */
    int CancelWaiting()
    {
        LogFlowFunc(("[Client %RU32] Cancelling waiting thread, isPending=%d, pendingNumParms=%RU32, m_idSession=%x\n",
                     m_idClient, m_enmIsPending, m_PendingReq.mNumParms, m_idSession));

        /*
         * The PEEK call is simple: At least two parameters, all set to zero before sleeping.
         */
        int rcComplete;
        if (m_enmIsPending == GUEST_MSG_PEEK_WAIT)
        {
            HGCMSvcSetU32(&m_PendingReq.mParms[0], HOST_CANCEL_PENDING_WAITS);
            rcComplete = VINF_TRY_AGAIN;
        }
        /*
         * The GUEST_MSG_WAIT call is complicated, though we're generally here
         * to wake up someone who is peeking and have two parameters.  If there
         * aren't two parameters, fail the call.
         */
        else if (m_enmIsPending != 0)
        {
            Assert(m_enmIsPending == GUEST_MSG_WAIT);
            if (m_PendingReq.mNumParms > 0)
                HGCMSvcSetU32(&m_PendingReq.mParms[0], HOST_CANCEL_PENDING_WAITS);
            if (m_PendingReq.mNumParms > 1)
                HGCMSvcSetU32(&m_PendingReq.mParms[1], 0);
            rcComplete = m_PendingReq.mNumParms == 2 ? VINF_SUCCESS : VERR_TRY_AGAIN;
        }
        /*
         * If nobody is waiting, flag the next wait call as cancelled.
         */
        else
        {
            m_fPendingCancel = true;
            return VINF_SUCCESS;
        }

        m_pSvcHelpers->pfnCallComplete(m_PendingReq.mHandle, rcComplete);

        m_PendingReq.mHandle   = NULL;
        m_PendingReq.mParms    = NULL;
        m_PendingReq.mNumParms = 0;
        m_enmIsPending            = (guestControl::eGuestFn)0;
        m_fPendingCancel      = false;
        return VINF_SUCCESS;
    }


    /** @name The GUEST_MSG_WAIT state and helpers.
     *
     * @note Don't try understand this, it is certificable!
     *
     * @{
     */

    /** Last (most recent) rc after handling the host command. */
    int mHostCmdRc;
    /** How many GUEST_MSG_WAIT calls the client has issued to retrieve one command.
     *
     * This is used as a heuristic to remove a message that the client appears not
     * to be able to successfully retrieve.  */
    uint32_t mHostCmdTries;
    /** Number of times we've peeked at a pending message.
     *
     * This is necessary for being compatible with older Guest Additions.  In case
     * there are commands which only have two (2) parameters and therefore would fit
     * into the GUEST_MSG_WAIT reply immediately, we now can make sure that the
     * client first gets back the GUEST_MSG_WAIT results first.
     */
    uint32_t mPeekCount;

    /**
     * Ditches the first host command and crazy GUEST_MSG_WAIT state.
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    void OldDitchFirstHostCmd()
    {
        HostCommand *pFirstCmd = RTListGetFirstCpp(&m_HostCmdList, HostCommand, m_ListEntry);
        Assert(pFirstCmd);
        RTListNodeRemove(&pFirstCmd->m_ListEntry);
        pFirstCmd->Delete();

        /* Reset state else. */
        mHostCmdRc    = VINF_SUCCESS;
        mHostCmdTries = 0;
        mPeekCount    = 0;
    }

    /**
     * Used by Wakeup() and OldRunCurrent().
     *
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldRun(ClientRequest const *pReq, HostCommand *pHostCmd)
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);
        Assert(RTListNodeIsFirst(&m_HostCmdList, &pHostCmd->m_ListEntry));

        LogFlowFunc(("[Client %RU32] pReq=%p, mHostCmdRc=%Rrc, mHostCmdTries=%RU32, mPeekCount=%RU32\n",
                      m_idClient, pReq, mHostCmdRc, mHostCmdTries, mPeekCount));

        int rc = mHostCmdRc = OldSendReply(pReq, pHostCmd);

        LogFlowThisFunc(("[Client %RU32] Processing command %RU32 ended with rc=%Rrc\n", m_idClient, pHostCmd->mMsgType, mHostCmdRc));

        bool fRemove = false;
        if (RT_FAILURE(rc))
        {
            mHostCmdTries++;

            /*
             * If the client understood the message but supplied too little buffer space
             * don't send this message again and drop it after 6 unsuccessful attempts.
             *
             * Note: Due to legacy reasons this the retry counter has to be even because on
             *       every peek there will be the actual command retrieval from the client side.
             *       To not get the actual command if the client actually only wants to peek for
             *       the next command, there needs to be two rounds per try, e.g. 3 rounds = 6 tries.
             */
            /** @todo Fix the mess stated above. GUEST_MSG_WAIT should be become GUEST_MSG_PEEK, *only*
             *        (and every time) returning the next upcoming host command (if any, blocking). Then
             *        it's up to the client what to do next, either peeking again or getting the actual
             *        host command via an own GUEST_ type message.
             */
            if (   rc == VERR_TOO_MUCH_DATA
                || rc == VERR_CANCELLED)
            {
                if (mHostCmdTries == 6)
                    fRemove = true;
            }
            /* Client did not understand the message or something else weird happened. Try again one
             * more time and drop it if it didn't get handled then. */
            else if (mHostCmdTries > 1)
                fRemove = true;
        }
        else
            fRemove = true; /* Everything went fine, remove it. */

        LogFlowThisFunc(("[Client %RU32] Tried command %RU32 for %RU32 times, (last result=%Rrc, fRemove=%RTbool)\n",
                         m_idClient, pHostCmd->mMsgType, mHostCmdTries, rc, fRemove));

        if (fRemove)
        {
            Assert(RTListNodeIsFirst(&m_HostCmdList, &pHostCmd->m_ListEntry));
            OldDitchFirstHostCmd();
        }

        LogFlowFunc(("[Client %RU32] Returned with rc=%Rrc\n", m_idClient, rc));
        return rc;
    }

    /**
     * @note Only used by GUEST_MSG_WAIT scenarios.
     */
    int OldRunCurrent(const ClientRequest *pReq)
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);

        /*
         * If the host command list is empty, the request must wait for one to be posted.
         */
        HostCommand *pFirstCmd = RTListGetFirstCpp(&m_HostCmdList, HostCommand, m_ListEntry);
        if (!pFirstCmd)
        {
            if (!m_fPendingCancel)
            {
                /* Go to sleep. */
                ASSERT_GUEST_RETURN(m_enmIsPending == 0, VERR_WRONG_ORDER);
                m_PendingReq = *pReq;
                m_enmIsPending  = GUEST_MSG_WAIT;
                LogFlowFunc(("[Client %RU32] Is now in pending mode\n", m_idClient));
                return VINF_HGCM_ASYNC_EXECUTE;
            }

            /* Wait was cancelled. */
            m_fPendingCancel = false;
            if (pReq->mNumParms > 0)
                HGCMSvcSetU32(&pReq->mParms[0], HOST_CANCEL_PENDING_WAITS);
            if (pReq->mNumParms > 1)
                HGCMSvcSetU32(&pReq->mParms[1], 0);
            return pReq->mNumParms == 2 ? VINF_SUCCESS : VERR_TRY_AGAIN;
        }

        /*
         * Return first host command.
         */
        return OldRun(pReq, pFirstCmd);
    }

    /**
     * Internal worker for OldRun().
     * @note Only used for GUEST_MSG_WAIT.
     */
    int OldSendReply(ClientRequest const *pReq,
                     HostCommand         *pHostCmd)
    {
        AssertPtrReturn(pReq, VERR_INVALID_POINTER);
        AssertPtrReturn(pHostCmd, VERR_INVALID_POINTER);

        /* In case of VERR_CANCELLED. */
        uint32_t const cSavedPeeks = mPeekCount;

        int rc;
        /* If the client is in pending mode, always send back
         * the peek result first. */
        if (m_enmIsPending)
        {
            Assert(m_enmIsPending == GUEST_MSG_WAIT);
            rc = pHostCmd->Peek(pReq);
            mPeekCount++;
        }
        else
        {
            /* If this is the very first peek, make sure to *always* give back the peeking answer
             * instead of the actual command, even if this command would fit into the current
             * connection buffer. */
            if (!mPeekCount)
            {
                rc = pHostCmd->Peek(pReq);
                mPeekCount++;
            }
            else
            {
                /* Try assigning the host command to the client and store the
                 * result code for later use. */
                rc = pHostCmd->Assign(pReq);
                if (RT_FAILURE(rc)) /* If something failed, let the client peek (again). */
                {
                    rc = pHostCmd->Peek(pReq);
                    mPeekCount++;
                }
                else
                    mPeekCount = 0;
            }
        }

        /* Reset pending status. */
        m_enmIsPending = (guestControl::eGuestFn)0;

        /* In any case the client did something, so complete
         * the pending call with the result we just got. */
        AssertPtr(m_pSvcHelpers);
        int rc2 = m_pSvcHelpers->pfnCallComplete(pReq->mHandle, rc);

        /* Rollback in case the guest cancelled the call. */
        if (rc2 == VERR_CANCELLED && RT_SUCCESS(rc))
        {
            mPeekCount = cSavedPeeks;
            rc = VERR_CANCELLED;
        }

        LogFlowThisFunc(("[Client %RU32] Command %RU32 ended with %Rrc (mPeekCount=%RU32, pReq=%p)\n",
                         m_idClient, pHostCmd->mMsgType, rc, mPeekCount, pReq));
        return rc;
    }

    /** @} */
} ClientState;
typedef std::map< uint32_t, ClientState *> ClientStateMap;

/**
 * Prepared session (GUEST_SESSION_PREPARE).
 */
typedef struct GstCtrlPreparedSession
{
    /** List entry. */
    RTLISTNODE  ListEntry;
    /** The session ID.   */
    uint32_t    idSession;
    /** The key size. */
    uint32_t    cbKey;
    /** The key bytes. */
    uint8_t     abKey[RT_FLEXIBLE_ARRAY];
} GstCtrlPreparedSession;


/**
 * Class containing the shared information service functionality.
 */
class GstCtrlService : public RTCNonCopyable
{

private:

    /** Type definition for use in callback functions. */
    typedef GstCtrlService SELF;
    /** HGCM helper functions. */
    PVBOXHGCMSVCHELPERS mpHelpers;
    /** Callback function supplied by the host for notification of updates to properties. */
    PFNHGCMSVCEXT mpfnHostCallback;
    /** User data pointer to be supplied to the host callback function. */
    void *mpvHostData;
    /** Map containing all connected clients, key is HGCM client ID. */
    ClientStateMap  m_ClientStateMap;
    /** Session ID -> client state. */
    ClientStateMap  m_SessionIdMap;
    /** The current master client, NULL if none. */
    ClientState    *m_pMasterClient;
    /** The master HGCM client ID, UINT32_MAX if none. */
    uint32_t        m_idMasterClient;
    /** Set if we're in legacy mode (pre 6.0). */
    bool            m_fLegacyMode;
    /** Number of prepared sessions. */
    uint32_t        m_cPreparedSessions;
    /** List of prepared session (GstCtrlPreparedSession). */
    RTLISTANCHOR    m_PreparedSessions;

public:
    explicit GstCtrlService(PVBOXHGCMSVCHELPERS pHelpers)
        : mpHelpers(pHelpers)
        , mpfnHostCallback(NULL)
        , mpvHostData(NULL)
        , m_pMasterClient(NULL)
        , m_idMasterClient(UINT32_MAX)
        , m_fLegacyMode(true)
        , m_cPreparedSessions(0)
    {
        RTListInit(&m_PreparedSessions);
    }

    static DECLCALLBACK(int)  svcUnload(void *pvService);
    static DECLCALLBACK(int)  svcConnect(void *pvService, uint32_t idClient, void *pvClient,
                                         uint32_t fRequestor, bool fRestoring);
    static DECLCALLBACK(int)  svcDisconnect(void *pvService, uint32_t idClient, void *pvClient);
    static DECLCALLBACK(void) svcCall(void *pvService, VBOXHGCMCALLHANDLE hCall, uint32_t idClient, void *pvClient,
                                      uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival);
    static DECLCALLBACK(int)  svcHostCall(void *pvService, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    static DECLCALLBACK(int)  svcSaveState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM);
    static DECLCALLBACK(int)  svcLoadState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM, uint32_t uVersion);
    static DECLCALLBACK(int)  svcRegisterExtension(void *pvService, PFNHGCMSVCEXT pfnExtension, void *pvExtension);

private:
    int clientMakeMeMaster(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms);
    int clientMsgPeek(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait);
    int clientMsgGet(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgCancel(ClientState *pClient, uint32_t cParms);
    int clientMsgSkip(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionPrepare(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionCancelPrepared(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionAccept(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientSessionCloseOther(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientToMain(ClientState *pClient, uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    int clientMsgOldGet(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgOldFilterSet(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int clientMsgOldSkip(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms);

    int hostCallback(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);
    int hostProcessCommand(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[]);

    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(GstCtrlService);
};


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnUnload,
 *  Simply deletes the GstCtrlService object}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcUnload(void *pvService)
{
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    delete pThis;

    return VINF_SUCCESS;
}



/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect,
 *  Initializes the state for a new client.}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcConnect(void *pvService, uint32_t idClient, void *pvClient, uint32_t fRequestor, bool fRestoring)
{
    LogFlowFunc(("[Client %RU32] Connected\n", idClient));

    RT_NOREF(fRestoring, pvClient);
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertMsg(pThis->m_ClientStateMap.find(idClient) == pThis->m_ClientStateMap.end(),
              ("Client with ID=%RU32 already connected when it should not\n", idClient));

    /*
     * Create client state.
     */
    ClientState *pClient;
    try
    {
        pClient = new (pvClient) ClientState(pThis->mpHelpers, idClient);
        pThis->m_ClientStateMap[idClient] = pClient;
    }
    catch (std::bad_alloc &)
    {
        if (pClient)
            pClient->~ClientState();
        return VERR_NO_MEMORY;
    }

    /*
     * For legacy compatibility reasons we have to pick a master client at some
     * point, so if the /dev/vboxguest requirements checks out we pick the first
     * one through the door.
     */
/** @todo make picking the master more dynamic/flexible? */
    if (   pThis->m_fLegacyMode
        && pThis->m_idMasterClient == UINT32_MAX)
    {
        if (   fRequestor == VMMDEV_REQUESTOR_LEGACY
            || !(fRequestor & VMMDEV_REQUESTOR_USER_DEVICE))
        {
            LogFunc(("Picking %u as master for now.\n", idClient));
            pThis->m_pMasterClient  = pClient;
            pThis->m_idMasterClient = idClient;
            pClient->m_fIsMaster = true;
        }
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnConnect,
 *  Handles a client which disconnected.}
 *
 * This functiond does some internal cleanup as well as sends notifications to
 * the host so that the host can do the same (if required).
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcDisconnect(void *pvService, uint32_t idClient, void *pvClient)
{
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    ClientState *pClient = reinterpret_cast<ClientState *>(pvClient);
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);
    LogFlowFunc(("[Client %RU32] Disconnected (%zu clients total)\n", idClient, pThis->m_ClientStateMap.size()));

    /*
     * Cancel all pending host commands, replying with GUEST_DISCONNECTED if final recipient.
     */
    HostCommand *pCurCmd, *pNextCmd;
    RTListForEachSafeCpp(&pClient->m_HostCmdList, pCurCmd, pNextCmd, HostCommand, m_ListEntry)
    {
        RTListNodeRemove(&pCurCmd->m_ListEntry);

        VBOXHGCMSVCPARM Parm;
        HGCMSvcSetU32(&Parm, pCurCmd->m_idContext);
        int rc2 = pThis->hostCallback(GUEST_DISCONNECTED, 1, &Parm);
        LogFlowFunc(("Cancelled host command %u (%s) with idContext=%#x -> %Rrc\n",
                     pCurCmd->mMsgType, GstCtrlHostFnName((eHostFn)pCurCmd->mMsgType), pCurCmd->m_idContext, rc2));
        RT_NOREF(rc2);

        pCurCmd->Delete();
    }

    /*
     * Delete the client state.
     */
    pThis->m_ClientStateMap.erase(idClient);
    if (pClient->m_idSession != UINT32_MAX)
        pThis->m_SessionIdMap.erase(pClient->m_idSession);
    pClient->~ClientState();

    /*
     * If it's the master disconnecting, we need to reset related globals.
     */
    if (idClient == pThis->m_idMasterClient)
    {
        pThis->m_pMasterClient  = NULL;
        pThis->m_idMasterClient = UINT32_MAX;

        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&pThis->m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            RTListNodeRemove(&pCur->ListEntry);
            RTMemFree(pCur);
        }
        pThis->m_cPreparedSessions = 0;
    }
    else
        Assert(pClient != pThis->m_pMasterClient);

    if (pThis->m_ClientStateMap.empty())
        pThis->m_fLegacyMode = true;

    return VINF_SUCCESS;
}


/**
 * A client asks for the next message to process.
 *
 * This either fills in a pending host command into the client's parameter space
 * or defers the guest call until we have something from the host.
 *
 * @returns VBox status code.
 * @param   pClient         The client state.
 * @param   hCall           The client's call handle.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::clientMsgOldGet(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    ASSERT_GUEST(pClient->m_idSession != UINT32_MAX || pClient->m_fIsMaster || pClient->m_fRestored);

    /* Use the current (inbound) connection. */
    ClientRequest thisCon;
    thisCon.mHandle   = hCall;
    thisCon.mNumParms = cParms;
    thisCon.mParms    = paParms;

    return pClient->OldRunCurrent(&thisCon);
}


/**
 * Implements GUEST_MAKE_ME_MASTER.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success (we complete the message here).
 * @retval  VERR_ACCESS_DENIED if not using main VBoxGuest device not
 * @retval  VERR_RESOURCE_BUSY if there is already a master.
 * @retval  VERR_VERSION_MISMATCH if VBoxGuest didn't supply requestor info.
 * @retval  VERR_WRONG_PARAMETER_COUNT
 *
 * @param   pClient     The client state.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 */
int GstCtrlService::clientMakeMeMaster(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_RETURN(cParms == 0, VERR_WRONG_PARAMETER_COUNT);

    uint32_t fRequestor = mpHelpers->pfnGetRequestor(hCall);
    ASSERT_GUEST_LOGREL_MSG_RETURN(fRequestor != VMMDEV_REQUESTOR_LEGACY,
                                   ("Outdated VBoxGuest w/o requestor support. Please update!\n"),
                                   VERR_VERSION_MISMATCH);
    ASSERT_GUEST_LOGREL_MSG_RETURN(!(fRequestor & VMMDEV_REQUESTOR_USER_DEVICE), ("fRequestor=%#x\n", fRequestor),
                                   VERR_ACCESS_DENIED);

    /*
     * Do the work.
     */
    ASSERT_GUEST_MSG_RETURN(m_idMasterClient == pClient->m_idClient || m_idMasterClient == UINT32_MAX,
                            ("Already have master session %RU32, refusing %RU32.\n", m_idMasterClient, pClient->m_idClient),
                            VERR_RESOURCE_BUSY);
    int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        m_pMasterClient  = pClient;
        m_idMasterClient = pClient->m_idClient;
        m_fLegacyMode    = false;
        pClient->m_fIsMaster = true;
        Log(("[Client %RU32] is master.\n", pClient->m_idClient));
    }
    else
        LogFunc(("pfnCallComplete -> %Rrc\n", rc));

    return VINF_HGCM_ASYNC_EXECUTE;
}

/**
 * Implements GUEST_MSG_PEEK_WAIT and GUEST_MSG_PEEK_NOWAIT.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if a message was pending and is being returned.
 * @retval  VERR_TRY_AGAIN if no message pending and not blocking.
 * @retval  VERR_RESOURCE_BUSY if another read already made a waiting call.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   pClient     The client state.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 * @param   fWait       Set if we should wait for a message, clear if to return
 *                      immediately.
 */
int GstCtrlService::clientMsgPeek(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[], bool fWait)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms >= 2, ("cParms=%u!\n", cParms), VERR_WRONG_PARAMETER_COUNT);

    uint64_t idRestoreCheck = 0;
    uint32_t i              = 0;
    if (paParms[i].type == VBOX_HGCM_SVC_PARM_64BIT)
    {
        idRestoreCheck = paParms[0].u.uint64;
        paParms[0].u.uint64 = 0;
        i++;
    }
    for (; i < cParms; i++)
    {
        ASSERT_GUEST_MSG_RETURN(paParms[i].type == VBOX_HGCM_SVC_PARM_32BIT, ("#%u type=%u\n", i, paParms[i].type),
                                VERR_WRONG_PARAMETER_TYPE);
        paParms[i].u.uint32 = 0;
    }

    /*
     * Check restore session ID.
     */
    if (idRestoreCheck != 0)
    {
        uint64_t idRestore = mpHelpers->pfnGetVMMDevSessionId(mpHelpers);
        if (idRestoreCheck != idRestore)
        {
            paParms[0].u.uint64 = idRestore;
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_XXXX -> VERR_VM_RESTORED (%#RX64 -> %#RX64)\n",
                         pClient->m_idClient, idRestoreCheck, idRestore));
            return VERR_VM_RESTORED;
        }
        Assert(!mpHelpers->pfnIsCallRestored(hCall));
    }

    /*
     * Return information about the first command if one is pending in the list.
     */
    HostCommand *pFirstCmd = RTListGetFirstCpp(&pClient->m_HostCmdList, HostCommand, m_ListEntry);
    if (pFirstCmd)
    {
        pFirstCmd->setPeekReturn(paParms, cParms);
        LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_XXXX -> VINF_SUCCESS (idMsg=%u (%s), cParms=%u)\n",
                     pClient->m_idClient, pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount));
        return VINF_SUCCESS;
    }

    /*
     * If we cannot wait, fail the call.
     */
    if (!fWait)
    {
        LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_NOWAIT -> VERR_TRY_AGAIN\n", pClient->m_idClient));
        return VERR_TRY_AGAIN;
    }

    /*
     * Wait for the host to queue a message for this client.
     */
    ASSERT_GUEST_MSG_RETURN(pClient->m_enmIsPending == 0, ("Already pending! (idClient=%RU32)\n", pClient->m_idClient),
                            VERR_RESOURCE_BUSY);
    pClient->m_PendingReq.mHandle   = hCall;
    pClient->m_PendingReq.mNumParms = cParms;
    pClient->m_PendingReq.mParms    = paParms;
    pClient->m_enmIsPending         = GUEST_MSG_PEEK_WAIT;
    LogFlowFunc(("[Client %RU32] Is now in pending mode...\n", pClient->m_idClient));
    return VINF_HGCM_ASYNC_EXECUTE;
}

/**
 * Implements GUEST_MSG_GET.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if message retrieved and removed from the pending queue.
 * @retval  VERR_TRY_AGAIN if no message pending.
 * @retval  VERR_BUFFER_OVERFLOW if a parmeter buffer is too small.  The buffer
 *          size was updated to reflect the required size, though this isn't yet
 *          forwarded to the guest.  (The guest is better of using peek with
 *          parameter count + 2 parameters to get the sizes.)
 * @retval  VERR_MISMATCH if the incoming message ID does not match the pending.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message was completed already.
 *
 * @param   pClient     The client state.
 * @param   hCall       The client's call handle.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int GstCtrlService::clientMsgGet(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate the request.
     *
     * The weird first parameter logic is due to GUEST_MSG_WAIT compatibility
     * (don't want to rewrite all the message structures).
     */
    uint32_t const idMsgExpected = cParms > 0 && paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT ? paParms[0].u.uint32
                                 : cParms > 0 && paParms[0].type == VBOX_HGCM_SVC_PARM_64BIT ? paParms[0].u.uint64
                                 : UINT32_MAX;

    /*
     * Return information about the first command if one is pending in the list.
     */
    HostCommand *pFirstCmd = RTListGetFirstCpp(&pClient->m_HostCmdList, HostCommand, m_ListEntry);
    if (pFirstCmd)
    {

        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mMsgType == idMsgExpected || idMsgExpected == UINT32_MAX,
                                ("idMsg=%u (%s) cParms=%u, caller expected %u (%s) and %u\n",
                                 pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount,
                                 idMsgExpected, GstCtrlHostFnName((eHostFn)idMsgExpected), cParms),
                                VERR_MISMATCH);
        ASSERT_GUEST_MSG_RETURN(pFirstCmd->mParmCount == cParms,
                                ("idMsg=%u (%s) cParms=%u, caller expected %u (%s) and %u\n",
                                 pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType), pFirstCmd->mParmCount,
                                 idMsgExpected, GstCtrlHostFnName((eHostFn)idMsgExpected), cParms),
                                VERR_WRONG_PARAMETER_COUNT);

        /* Check the parameter types. */
        for (uint32_t i = 0; i < cParms; i++)
            ASSERT_GUEST_MSG_RETURN(pFirstCmd->mpParms[i].type == paParms[i].type,
                                    ("param #%u: type %u, caller expected %u (idMsg=%u %s)\n", i, pFirstCmd->mpParms[i].type,
                                     paParms[i].type, pFirstCmd->mMsgType, GstCtrlHostFnName((eHostFn)pFirstCmd->mMsgType)),
                                    VERR_WRONG_PARAMETER_TYPE);

        /*
         * Copy out the parameters.
         *
         * No assertions on buffer overflows, and keep going till the end so we can
         * communicate all the required buffer sizes.
         */
        int rc = VINF_SUCCESS;
        for (uint32_t i = 0; i < cParms; i++)
            switch (pFirstCmd->mpParms[i].type)
            {
                case VBOX_HGCM_SVC_PARM_32BIT:
                    paParms[i].u.uint32 = pFirstCmd->mpParms[i].u.uint32;
                    break;

                case VBOX_HGCM_SVC_PARM_64BIT:
                    paParms[i].u.uint64 = pFirstCmd->mpParms[i].u.uint64;
                    break;

                case VBOX_HGCM_SVC_PARM_PTR:
                {
                    uint32_t const cbSrc = pFirstCmd->mpParms[i].u.pointer.size;
                    uint32_t const cbDst = paParms[i].u.pointer.size;
                    paParms[i].u.pointer.size = cbSrc; /** @todo Check if this is safe in other layers...
                                                        * Update: Safe, yes, but VMMDevHGCM doesn't pass it along. */
                    if (cbSrc <= cbDst)
                        memcpy(paParms[i].u.pointer.addr, pFirstCmd->mpParms[i].u.pointer.addr, cbSrc);
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                    break;
                }

                default:
                    AssertMsgFailed(("#%u: %u\n", i, pFirstCmd->mpParms[i].type));
                    rc = VERR_INTERNAL_ERROR;
                    break;
            }
        if (RT_SUCCESS(rc))
        {
            /*
             * Complete the command and remove the pending message unless the
             * guest raced us and cancelled this call in the meantime.
             */
            AssertPtr(mpHelpers);
            rc = mpHelpers->pfnCallComplete(hCall, rc);
            if (rc != VERR_CANCELLED)
            {
                RTListNodeRemove(&pFirstCmd->m_ListEntry);
                pFirstCmd->Delete();
            }
            else
                LogFunc(("pfnCallComplete -> %Rrc\n", rc));
            return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
        }
        return rc;
    }

    paParms[0].u.uint32 = 0;
    paParms[1].u.uint32 = 0;
    LogFlowFunc(("[Client %RU32] GUEST_MSG_GET -> VERR_TRY_AGAIN\n", pClient->m_idClient));
    return VERR_TRY_AGAIN;
}

/**
 * Implements GUEST_MSG_CANCEL.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS if cancelled any calls.
 * @retval  VWRN_NOT_FOUND if no callers.
 * @retval  VINF_HGCM_ASYNC_EXECUTE if message wait is pending.
 *
 * @param   pClient     The client state.
 * @param   cParms      Number of parameters.
 */
int GstCtrlService::clientMsgCancel(ClientState *pClient, uint32_t cParms)
{
    /*
     * Validate the request.
     */
    ASSERT_GUEST_MSG_RETURN(cParms == 0, ("cParms=%u!\n", cParms), VERR_WRONG_PARAMETER_COUNT);

    /*
     * Execute.
     */
    if (pClient->m_enmIsPending != 0)
    {
        pClient->CancelWaiting();
        return VINF_SUCCESS;
    }
    return VWRN_NOT_FOUND;
}


/**
 * Implements GUEST_MSG_SKIP.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_NOT_FOUND if no message pending.
 *
 * @param   pClient     The client state.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientMsgSkip(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate the call.
     */
    ASSERT_GUEST_RETURN(cParms <= 2, VERR_WRONG_PARAMETER_COUNT);

    int32_t rcSkip = VERR_NOT_SUPPORTED;
    if (cParms >= 1)
    {
        ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
        rcSkip = (int32_t)paParms[0].u.uint32;
    }

    uint32_t idMsg = UINT32_MAX;
    if (cParms >= 2)
    {
        ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
        idMsg = paParms[1].u.uint32;
    }

    /*
     * Do the job.
     */
    HostCommand *pFirstCmd = RTListGetFirstCpp(&pClient->m_HostCmdList, HostCommand, m_ListEntry);
    if (pFirstCmd)
    {
        if (   pFirstCmd->mMsgType == idMsg
            || idMsg == UINT32_MAX)
        {
            int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Remove the command from the queue.
                 */
                Assert(RTListNodeIsFirst(&pClient->m_HostCmdList, &pFirstCmd->m_ListEntry) );
                RTListNodeRemove(&pFirstCmd->m_ListEntry);

                /*
                 * Compose a reply to the host service.
                 */
                VBOXHGCMSVCPARM aReplyParams[5];
                HGCMSvcSetU32(&aReplyParams[0], pFirstCmd->m_idContext);
                switch (pFirstCmd->mMsgType)
                {
                    case HOST_EXEC_CMD:
                        HGCMSvcSetU32(&aReplyParams[1], 0);              /* pid */
                        HGCMSvcSetU32(&aReplyParams[2], PROC_STS_ERROR); /* status */
                        HGCMSvcSetU32(&aReplyParams[3], rcSkip);         /* flags / whatever */
                        HGCMSvcSetPv(&aReplyParams[4], NULL, 0);         /* data buffer */
                        hostCallback(GUEST_EXEC_STATUS, 5, aReplyParams);
                        break;

                    case HOST_SESSION_CREATE:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_SESSION_NOTIFYTYPE_ERROR);    /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                            /* result */
                        hostCallback(GUEST_SESSION_NOTIFY, 3, aReplyParams);
                        break;

                    case HOST_EXEC_SET_INPUT:
                        HGCMSvcSetU32(&aReplyParams[1], pFirstCmd->mParmCount >= 2 ? pFirstCmd->mpParms[1].u.uint32 : 0);
                        HGCMSvcSetU32(&aReplyParams[2], INPUT_STS_ERROR);   /* status */
                        HGCMSvcSetU32(&aReplyParams[3], rcSkip);            /* flags / whatever */
                        HGCMSvcSetU32(&aReplyParams[4], 0);                 /* bytes consumed */
                        hostCallback(GUEST_EXEC_INPUT_STATUS, 5, aReplyParams);
                        break;

                    case HOST_FILE_OPEN:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_OPEN); /* type*/
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                     /* rc */
                        HGCMSvcSetU32(&aReplyParams[3], VBOX_GUESTCTRL_CONTEXTID_GET_OBJECT(pFirstCmd->m_idContext)); /* handle */
                        hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_CLOSE:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_ERROR); /* type*/
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        hostCallback(GUEST_FILE_NOTIFY, 3, aReplyParams);
                        break;
                    case HOST_FILE_READ:
                    case HOST_FILE_READ_AT:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_READ);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetPv(&aReplyParams[3], NULL, 0);                      /* data buffer */
                        hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_WRITE:
                    case HOST_FILE_WRITE_AT:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_WRITE); /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU32(&aReplyParams[3], 0);                           /* bytes written */
                        hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_SEEK:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_SEEK);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU64(&aReplyParams[3], 0);                           /* actual */
                        hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;
                    case HOST_FILE_TELL:
                        HGCMSvcSetU32(&aReplyParams[1], GUEST_FILE_NOTIFYTYPE_TELL);  /* type */
                        HGCMSvcSetU32(&aReplyParams[2], rcSkip);                      /* rc */
                        HGCMSvcSetU64(&aReplyParams[3], 0);                           /* actual */
                        hostCallback(GUEST_FILE_NOTIFY, 4, aReplyParams);
                        break;

                    case HOST_EXEC_GET_OUTPUT: /** @todo This can't be right/work. */
                    case HOST_EXEC_TERMINATE:  /** @todo This can't be right/work. */
                    case HOST_EXEC_WAIT_FOR:   /** @todo This can't be right/work. */
                    case HOST_PATH_USER_DOCUMENTS:
                    case HOST_PATH_USER_HOME:
                    case HOST_PATH_RENAME:
                    case HOST_DIR_REMOVE:
                    default:
                        HGCMSvcSetU32(&aReplyParams[1], pFirstCmd->mMsgType);
                        HGCMSvcSetU32(&aReplyParams[2], (uint32_t)rcSkip);
                        HGCMSvcSetPv(&aReplyParams[3], NULL, 0);
                        hostCallback(GUEST_MSG_REPLY, 4, aReplyParams);
                        break;
                }

                /*
                 * Free the command.
                 */
                pFirstCmd->Delete();
            }
            else
                LogFunc(("pfnCallComplete -> %Rrc\n", rc));
            return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
        }
        LogFunc(("Warning: GUEST_MSG_SKIP mismatch! Found %u, caller expected %u!\n", pFirstCmd->mMsgType, idMsg));
        return VERR_MISMATCH;
    }
    return VERR_NOT_FOUND;
}


/**
 * Implements GUEST_SESSION_PREPARE.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_OUT_OF_RESOURCES if too many pending sessions hanging around.
 * @retval  VERR_OUT_OF_RANGE if the session ID outside the allowed range.
 * @retval  VERR_BUFFER_OVERFLOW if key too large.
 * @retval  VERR_BUFFER_UNDERFLOW if key too small.
 * @retval  VERR_ACCESS_DENIED if not master or in legacy mode.
 * @retval  VERR_DUPLICATE if the session ID has been prepared already.
 *
 * @param   pClient     The client state.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionPrepare(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(idSession >= 1, VERR_OUT_OF_RANGE);
    ASSERT_GUEST_RETURN(idSession <= 0xfff0, VERR_OUT_OF_RANGE);

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_PTR, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const cbKey = paParms[1].u.pointer.size;
    void const    *pvKey = paParms[1].u.pointer.addr;
    ASSERT_GUEST_RETURN(cbKey >= 64, VERR_BUFFER_UNDERFLOW);
    ASSERT_GUEST_RETURN(cbKey <= _16K, VERR_BUFFER_OVERFLOW);

    ASSERT_GUEST_RETURN(pClient->m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient == pClient->m_idClient);
    Assert(m_pMasterClient == pClient);

    /* Now that we know it's the master, we can check for session ID duplicates. */
    GstCtrlPreparedSession *pCur;
    RTListForEach(&m_PreparedSessions, pCur, GstCtrlPreparedSession, ListEntry)
    {
        ASSERT_GUEST_RETURN(pCur->idSession != idSession, VERR_DUPLICATE);
    }

    /*
     * Make a copy of the session ID and key.
     */
    ASSERT_GUEST_RETURN(m_cPreparedSessions < 128, VERR_OUT_OF_RESOURCES);

    GstCtrlPreparedSession *pPrepped = (GstCtrlPreparedSession *)RTMemAlloc(RT_UOFFSETOF_DYN(GstCtrlPreparedSession, abKey[cbKey]));
    AssertReturn(pPrepped, VERR_NO_MEMORY);
    pPrepped->idSession = idSession;
    pPrepped->cbKey     = cbKey;
    memcpy(pPrepped->abKey, pvKey, cbKey);

    RTListAppend(&m_PreparedSessions, &pPrepped->ListEntry);
    m_cPreparedSessions++;

    /*
     * Try complete the command.
     */
    int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        LogFlow(("Prepared %u with a %#x byte key (%u pending).\n", idSession, cbKey, m_cPreparedSessions));
    else
    {
        LogFunc(("pfnCallComplete -> %Rrc\n", rc));
        RTListNodeRemove(&pPrepped->ListEntry);
        RTMemFree(pPrepped);
        m_cPreparedSessions--;
    }
    return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
}


/**
 * Implements GUEST_SESSION_CANCEL_PREPARED.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VWRN_NOT_FOUND if no session with the specified ID.
 * @retval  VERR_ACCESS_DENIED if not master or in legacy mode.
 *
 * @param   pClient     The client state.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionCancelPrepared(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 1, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;

    ASSERT_GUEST_RETURN(pClient->m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient == pClient->m_idClient);
    Assert(m_pMasterClient == pClient);

    /*
     * Do the work.
     */
    int rc = VWRN_NOT_FOUND;
    if (idSession == UINT32_MAX)
    {
        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            RTListNodeRemove(&pCur->ListEntry);
            RTMemFree(pCur);
            rc = VINF_SUCCESS;
        }
        m_cPreparedSessions = 0;
    }
    else
    {
        GstCtrlPreparedSession *pCur, *pNext;
        RTListForEachSafe(&m_PreparedSessions, pCur, pNext, GstCtrlPreparedSession, ListEntry)
        {
            if (pCur->idSession == idSession)
            {
                RTListNodeRemove(&pCur->ListEntry);
                RTMemFree(pCur);
                m_cPreparedSessions -= 1;
                rc = VINF_SUCCESS;
                break;
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Implements GUEST_SESSION_ACCEPT.
 *
 * @returns VBox status code.
 * @retval  VINF_HGCM_ASYNC_EXECUTE on success as we complete the message.
 * @retval  VERR_NOT_FOUND if the specified session ID wasn't found.
 * @retval  VERR_MISMATCH if the key didn't match.
 * @retval  VERR_ACCESS_DENIED if we're in legacy mode or is master.
 * @retval  VERR_RESOURCE_BUSY if the client is already associated with a
 *          session.
 *
 * @param   pClient     The client state.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 * @param   paParms     The parameters.
 */
int GstCtrlService::clientSessionAccept(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate parameters.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idSession = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(idSession >= 1, VERR_OUT_OF_RANGE);
    ASSERT_GUEST_RETURN(idSession <= 0xfff0, VERR_OUT_OF_RANGE);

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_PTR, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const cbKey = paParms[1].u.pointer.size;
    void const    *pvKey = paParms[1].u.pointer.addr;
    ASSERT_GUEST_RETURN(cbKey >= 64, VERR_BUFFER_UNDERFLOW);
    ASSERT_GUEST_RETURN(cbKey <= _16K, VERR_BUFFER_OVERFLOW);

    ASSERT_GUEST_RETURN(!pClient->m_fIsMaster, VERR_ACCESS_DENIED);
    ASSERT_GUEST_RETURN(!m_fLegacyMode, VERR_ACCESS_DENIED);
    Assert(m_idMasterClient != pClient->m_idClient);
    Assert(m_pMasterClient != pClient);
    ASSERT_GUEST_RETURN(pClient->m_idSession == UINT32_MAX, VERR_RESOURCE_BUSY);

    /*
     * Look for the specified session and match the key to it.
     */
    GstCtrlPreparedSession *pCur;
    RTListForEach(&m_PreparedSessions, pCur, GstCtrlPreparedSession, ListEntry)
    {
        if (pCur->idSession == idSession)
        {
            if (   pCur->cbKey == cbKey
                && memcmp(pCur->abKey, pvKey, cbKey) == 0)
            {
                /*
                 * We've got a match.
                 * Try insert it into the sessio ID map and complete the request.
                 */
                try
                {
                    m_SessionIdMap[idSession] = pClient;
                }
                catch (std::bad_alloc &)
                {
                    LogFunc(("Out of memory!\n"));
                    return VERR_NO_MEMORY;
                }

                int rc = mpHelpers->pfnCallComplete(hCall, VINF_SUCCESS);
                if (RT_SUCCESS(rc))
                {
                    pClient->m_idSession = idSession;

                    RTListNodeRemove(&pCur->ListEntry);
                    RTMemFree(pCur);
                    m_cPreparedSessions -= 1;
                    Log(("[Client %RU32] accepted session id %u.\n", pClient->m_idClient, idSession));
                }
                else
                {
                    LogFunc(("pfnCallComplete -> %Rrc\n", rc));
                    m_SessionIdMap.erase(idSession);
                }
                return VINF_HGCM_ASYNC_EXECUTE; /* The caller must not complete it. */
            }
            LogFunc(("Key mismatch for %u!\n", pClient->m_idClient));
            return VERR_MISMATCH;
        }
    }

    LogFunc(("No client prepared for %u!\n", pClient->m_idClient));
    return VERR_NOT_FOUND;
}


/**
 * Client asks another client (guest) session to close.
 *
 * @returns VBox status code.
 * @param   pClient         The client state.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::clientSessionCloseOther(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate input.
     */
    ASSERT_GUEST_RETURN(cParms == 2, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const idContext = paParms[0].u.uint32;

    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t const fFlags = paParms[1].u.uint32;

    ASSERT_GUEST_RETURN(pClient->m_fIsMaster || (m_fLegacyMode && pClient->m_idSession == UINT32_MAX), VERR_ACCESS_DENIED);

    /*
     * Forward the command to the destiation.
     * Since we modify the first parameter, we must make a copy of the parameters.
     */
    VBOXHGCMSVCPARM aParms[2];
    HGCMSvcSetU64(&aParms[0], idContext | VBOX_GUESTCTRL_DST_SESSION);
    HGCMSvcSetU32(&aParms[1], fFlags);
    int rc = hostProcessCommand(HOST_SESSION_CLOSE, RT_ELEMENTS(aParms), aParms);

    LogFlowFunc(("Closing guest context ID=%RU32 (from client ID=%RU32) returned with rc=%Rrc\n", idContext, pClient->m_idClient, rc));
    return rc;
}


/**
 * For compatiblity with old additions only - filtering / set session ID.
 *
 * @return  VBox status code.
 * @param   pClient     The client state.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int GstCtrlService::clientMsgOldFilterSet(ClientState *pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Validate input and access.
     */
    ASSERT_GUEST_RETURN(cParms == 4, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t uValue = paParms[0].u.uint32;
    ASSERT_GUEST_RETURN(paParms[1].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t fMaskAdd = paParms[1].u.uint32;
    ASSERT_GUEST_RETURN(paParms[2].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE);
    uint32_t fMaskRemove = paParms[2].u.uint32;
    ASSERT_GUEST_RETURN(paParms[3].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_TYPE); /* flags, unused */

    /*
     * We have a bunch of expectations here:
     *  - Never called in non-legacy mode.
     *  - Only called once per session.
     *  - Never called by the master session.
     *  - Clients that doesn't wish for any messages passes all zeros.
     *  - All other calls has a unique session ID.
     */
    ASSERT_GUEST_LOGREL_RETURN(m_fLegacyMode, VERR_WRONG_ORDER);
    ASSERT_GUEST_LOGREL_MSG_RETURN(pClient->m_idSession == UINT32_MAX, ("m_idSession=%#x\n", pClient->m_idSession),
                                   VERR_WRONG_ORDER);
    ASSERT_GUEST_LOGREL_RETURN(!pClient->m_fIsMaster, VERR_WRONG_ORDER);

    if (uValue == 0)
    {
        ASSERT_GUEST_LOGREL(fMaskAdd == 0);
        ASSERT_GUEST_LOGREL(fMaskRemove == 0);
        /* Nothing to do, already muted (UINT32_MAX). */
    }
    else
    {
        ASSERT_GUEST_LOGREL(fMaskAdd == UINT32_C(0xf8000000));
        ASSERT_GUEST_LOGREL(fMaskRemove == 0);

        uint32_t idSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(uValue);
        ASSERT_GUEST_LOGREL_MSG_RETURN(idSession > 0, ("idSession=%u (%#x)\n", idSession, uValue), VERR_OUT_OF_RANGE);

        ClientStateMap::iterator ItConflict = m_SessionIdMap.find(idSession);
        ASSERT_GUEST_LOGREL_MSG_RETURN(ItConflict == m_SessionIdMap.end(),
                                       ("idSession=%u uValue=%#x idClient=%u; conflicting with client %u\n",
                                        idSession, uValue, pClient->m_idClient, ItConflict->second->m_idClient),
                                       VERR_DUPLICATE);

        /* Commit it. */
        try
        {
            m_SessionIdMap[idSession] = pClient;
        }
        catch (std::bad_alloc &)
        {
            LogFunc(("Out of memory\n"));
            return VERR_NO_MEMORY;
        }
        pClient->m_idSession = idSession;
    }
    return VINF_SUCCESS;
}


/**
 * For compatibility with old additions only - skip the current command w/o
 * calling main code.
 *
 * Please note that we don't care if the caller cancelled the request, because
 * old additions code didn't give damn about VERR_INTERRUPT.
 *
 * @return  VBox status code.
 * @param   pClient     The client state.
 * @param   hCall       The call handle for completing it.
 * @param   cParms      Number of parameters.
 */
int GstCtrlService::clientMsgOldSkip(ClientState *pClient, VBOXHGCMCALLHANDLE hCall, uint32_t cParms)
{
    /*
     * Validate input and access.
     */
    ASSERT_GUEST_RETURN(cParms == 1, VERR_WRONG_PARAMETER_COUNT);

    /*
     * Execute the request.
     *
     * Note! As it turns out the old and new skip should be mostly the same.  The
     *       pre-6.0 GAs (up to BETA3) has a hack which tries to issue a
     *       VERR_NOT_SUPPORTED reply to unknown host requests, however the 5.2.x
     *       and earlier GAs doesn't.  We need old skip behavior only for the 6.0
     *       beta GAs, nothing else.
     *       So, we have to track whether they issued a MSG_REPLY or not.  Wonderful.
     */
    HostCommand *pFirstCmd = RTListGetFirstCpp(&pClient->m_HostCmdList, HostCommand, m_ListEntry);
    if (pFirstCmd)
    {
        uint32_t const idMsg             = pFirstCmd->mMsgType;
        bool const     f60BetaHackInPlay = pFirstCmd->m_f60BetaHackInPlay;
        int            rc;
        if (!f60BetaHackInPlay)
            rc = clientMsgSkip(pClient, hCall, 0, NULL);
        else
        {
            RTListNodeRemove(&pFirstCmd->m_ListEntry);
            pFirstCmd->Delete();
            rc = VINF_SUCCESS;
        }

        /* Reset legacy message wait/get state: */
        if (RT_SUCCESS(rc))
        {
            pClient->mHostCmdRc    = VINF_SUCCESS;
            pClient->mHostCmdTries = 0;
            pClient->mPeekCount    = 0;
        }

        LogFlowFunc(("[Client %RU32] Legacy message skipping: Skipped %u (%s)%s!\n",
                     pClient->m_idClient, idMsg, GstCtrlHostFnName((eHostFn)idMsg), f60BetaHackInPlay ? " hack style" : ""));
        NOREF(idMsg);
        return rc;
    }
    LogFlowFunc(("[Client %RU32] Legacy message skipping: No messages pending!\n", pClient->m_idClient));
    return VINF_SUCCESS;
}


/**
 * Forwards client call to the Main API.
 *
 * This is typically notifications and replys.
 *
 * @returns VBox status code.
 * @param   pClient         The client state.
 * @param   idFunction      Function (event) that occured.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::clientToMain(ClientState *pClient, uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * Do input validation.  This class of messages all have a 32-bit context ID as
     * the first parameter, so make sure it is there and appropriate for the caller.
     */
    ASSERT_GUEST_RETURN(cParms >= 1, VERR_WRONG_PARAMETER_COUNT);
    ASSERT_GUEST_RETURN(paParms[0].type == VBOX_HGCM_SVC_PARM_32BIT, VERR_WRONG_PARAMETER_COUNT);
    uint32_t const idContext = paParms[0].u.uint32;
    uint32_t const idSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(idContext);

    ASSERT_GUEST_MSG_RETURN(   pClient->m_idSession == idSession
                            || pClient->m_fIsMaster
                            || (   m_fLegacyMode                                /* (see bugref:9313#c16) */
                                && pClient->m_idSession == UINT32_MAX
                                && (   idFunction == GUEST_EXEC_STATUS
                                    || idFunction == GUEST_SESSION_NOTIFY)),
                            ("idSession=%u (CID=%#x) m_idSession=%u idClient=%u idFunction=%u (%s)\n", idSession, idContext,
                             pClient->m_idSession, pClient->m_idClient, idFunction, GstCtrlGuestFnName((eGuestFn)idFunction)),
                            VERR_ACCESS_DENIED);

    /*
     * It seems okay, so make the call.
     */
    return hostCallback(idFunction, cParms, paParms);
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnCall}
 *
 * @note    All functions which do not involve an unreasonable delay will be
 *          handled synchronously.  If needed, we will add a request handler
 *          thread in future for those which do.
 * @thread  HGCM
 */
/*static*/ DECLCALLBACK(void)
GstCtrlService::svcCall(void *pvService, VBOXHGCMCALLHANDLE hCall, uint32_t idClient, void *pvClient,
                        uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[], uint64_t tsArrival)
{
    LogFlowFunc(("[Client %RU32] idFunction=%RU32 (%s), cParms=%RU32, paParms=0x%p\n",
                 idClient, idFunction, GstCtrlGuestFnName((eGuestFn)idFunction), cParms, paParms));
    RT_NOREF(tsArrival, idClient);

    /*
     * Convert opaque pointers to typed ones.
     */
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertReturnVoidStmt(pThis, pThis->mpHelpers->pfnCallComplete(hCall, VERR_INTERNAL_ERROR_5));
    ClientState *pClient = reinterpret_cast<ClientState *>(pvClient);
    AssertReturnVoidStmt(pClient, pThis->mpHelpers->pfnCallComplete(hCall, VERR_INVALID_CLIENT_ID));
    Assert(pClient->m_idClient == idClient);

    /*
     * Do the dispatching.
     */
    int rc;
    switch (idFunction)
    {
        case GUEST_MAKE_ME_MASTER:
            LogFlowFunc(("[Client %RU32] GUEST_MAKE_ME_MASTER\n", idClient));
            rc = pThis->clientMakeMeMaster(pClient, hCall, cParms);
            break;
        case GUEST_MSG_PEEK_NOWAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_NOWAIT\n", idClient));
            rc = pThis->clientMsgPeek(pClient, hCall, cParms, paParms, false /*fWait*/);
            break;
        case GUEST_MSG_PEEK_WAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_PEEK_WAIT\n", idClient));
            rc = pThis->clientMsgPeek(pClient, hCall, cParms, paParms, true /*fWait*/);
            break;
        case GUEST_MSG_GET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_GET\n", idClient));
            rc = pThis->clientMsgGet(pClient, hCall, cParms, paParms);
            break;
        case GUEST_MSG_CANCEL:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_CANCEL\n", idClient));
            rc = pThis->clientMsgCancel(pClient, cParms);
            break;
        case GUEST_MSG_SKIP:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_SKIP\n", idClient));
            rc = pThis->clientMsgSkip(pClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_PREPARE:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_PREPARE\n", idClient));
            rc = pThis->clientSessionPrepare(pClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_CANCEL_PREPARED:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_CANCEL_PREPARED\n", idClient));
            rc = pThis->clientSessionCancelPrepared(pClient, cParms, paParms);
            break;
        case GUEST_SESSION_ACCEPT:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_ACCEPT\n", idClient));
            rc = pThis->clientSessionAccept(pClient, hCall, cParms, paParms);
            break;
        case GUEST_SESSION_CLOSE:
            LogFlowFunc(("[Client %RU32] GUEST_SESSION_CLOSE\n", idClient));
            rc = pThis->clientSessionCloseOther(pClient, cParms, paParms);
            break;

        /*
         * Stuff the goes to various main objects:
         */
        case GUEST_MSG_REPLY:
            if (cParms >= 3 && paParms[2].u.uint32 == (uint32_t)VERR_NOT_SUPPORTED)
            {
                HostCommand *pFirstCmd = RTListGetFirstCpp(&pClient->m_HostCmdList, HostCommand, m_ListEntry);
                if (pFirstCmd && pFirstCmd->m_idContext == paParms[0].u.uint32)
                    pFirstCmd->m_f60BetaHackInPlay = true;
            }
            RT_FALL_THROUGH();
        case GUEST_MSG_PROGRESS_UPDATE:
        case GUEST_SESSION_NOTIFY:
        case GUEST_EXEC_OUTPUT:
        case GUEST_EXEC_STATUS:
        case GUEST_EXEC_INPUT_STATUS:
        case GUEST_EXEC_IO_NOTIFY:
        case GUEST_DIR_NOTIFY:
        case GUEST_FILE_NOTIFY:
            LogFlowFunc(("[Client %RU32] %s\n", idClient, GstCtrlGuestFnName((eGuestFn)idFunction)));
            rc = pThis->clientToMain(pClient, idFunction, cParms, paParms);
            Assert(rc != VINF_HGCM_ASYNC_EXECUTE);
            break;

        /*
         * The remaining commands are here for compatibility with older guest additions:
         */
        case GUEST_MSG_WAIT:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_WAIT\n", idClient));
            pThis->clientMsgOldGet(pClient, hCall, cParms, paParms);
            rc = VINF_HGCM_ASYNC_EXECUTE;
            break;

        case GUEST_MSG_SKIP_OLD:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_SKIP_OLD\n", idClient));
            rc = pThis->clientMsgOldSkip(pClient, hCall, cParms);
            break;

        case GUEST_MSG_FILTER_SET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_SET\n", idClient));
            rc = pThis->clientMsgOldFilterSet(pClient, cParms, paParms);
            break;

        case GUEST_MSG_FILTER_UNSET:
            LogFlowFunc(("[Client %RU32] GUEST_MSG_FILTER_UNSET\n", idClient));
            rc = VERR_NOT_IMPLEMENTED;
            break;

        /*
         * Anything else shall return invalid function.
         * Note! We used to return VINF_SUCCESS for these.  See bugref:9313
         *       and Guest::i_notifyCtrlDispatcher().
         */
        default:
            ASSERT_GUEST_MSG_FAILED(("idFunction=%d (%#x)\n", idFunction, idFunction));
            rc = VERR_INVALID_FUNCTION;
            break;
    }

    if (rc != VINF_HGCM_ASYNC_EXECUTE)
    {
        /* Tell the client that the call is complete (unblocks waiting). */
        LogFlowFunc(("[Client %RU32] Calling pfnCallComplete w/ rc=%Rrc\n", idClient, rc));
        AssertPtr(pThis->mpHelpers);
        pThis->mpHelpers->pfnCallComplete(hCall, rc);
    }
}


/**
 * Notifies the host (using low-level HGCM callbacks) about an event
 * which was sent from the client.
 *
 * @returns VBox status code.
 * @param   idFunction      Function (event) that occured.
 * @param   cParms          Number of parameters.
 * @param   paParms         Array of parameters.
 */
int GstCtrlService::hostCallback(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    LogFlowFunc(("idFunction=%u (%s), cParms=%ld, paParms=%p\n", idFunction, GstCtrlGuestFnName((eGuestFn)idFunction), cParms, paParms));

    int rc;
    if (mpfnHostCallback)
    {
        VBOXGUESTCTRLHOSTCALLBACK data(cParms, paParms);
        rc = mpfnHostCallback(mpvHostData, idFunction, &data, sizeof(data));
    }
    else
        rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("Returning rc=%Rrc\n", rc));
    return rc;
}


/**
 * Processes a command received from the host side and re-routes it to
 * a connect client on the guest.
 *
 * @returns VBox status code.
 * @param   idFunction  Function code to process.
 * @param   cParms      Number of parameters.
 * @param   paParms     Array of parameters.
 */
int GstCtrlService::hostProcessCommand(uint32_t idFunction, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    /*
     * If no client is connected at all we don't buffer any host commands
     * and immediately return an error to the host.  This avoids the host
     * waiting for a response from the guest side in case VBoxService on
     * the guest is not running/system is messed up somehow.
     */
    if (m_ClientStateMap.empty())
    {
        LogFlow(("GstCtrlService::hostProcessCommand: VERR_NOT_FOUND!\n"));
        return VERR_NOT_FOUND;
    }

    /*
     * Create a host command for each destination.
     * Note! There is currently only one scenario in which we send a host
     *       command to two recipients.
     */
    HostCommand *pHostCmd = new (std::nothrow) HostCommand();
    AssertReturn(pHostCmd, VERR_NO_MEMORY);
    int rc = pHostCmd->Init(idFunction, cParms, paParms);
    if (RT_SUCCESS(rc))
    {
        uint64_t const fDestinations = pHostCmd->m_idContextAndDst & VBOX_GUESTCTRL_DST_BOTH;
        HostCommand   *pHostCmd2     = NULL;
        if (fDestinations != VBOX_GUESTCTRL_DST_BOTH)
        { /* likely */ }
        else
        {
            pHostCmd2 = new (std::nothrow) HostCommand();
            if (pHostCmd2)
                rc = pHostCmd2->Init(idFunction, cParms, paParms);
            else
                rc = VERR_NO_MEMORY;
        }
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("Handling host command m_idContextAndDst=%#RX64, idFunction=%RU32, cParms=%RU32, paParms=%p, cClients=%zu\n",
                         pHostCmd->m_idContextAndDst, idFunction, cParms, paParms, m_ClientStateMap.size()));

            /*
             * Find the message destination and post it to the client.  If the
             * session ID doesn't match any particular client it goes to the master.
             */
            AssertMsg(!m_ClientStateMap.empty(), ("Client state map is empty when it should not be!\n"));

            /* Dispatch to the session. */
            if (fDestinations & VBOX_GUESTCTRL_DST_SESSION)
            {
                uint32_t const idSession = VBOX_GUESTCTRL_CONTEXTID_GET_SESSION(pHostCmd->m_idContext);
                ClientStateMap::iterator It = m_SessionIdMap.find(idSession);
                if (It != m_SessionIdMap.end())
                {
                    ClientState *pClient = It->second;
                    Assert(pClient->m_idSession == idSession);
                    RTListAppend(&pClient->m_HostCmdList, &pHostCmd->m_ListEntry);
                    pHostCmd  = pHostCmd2;
                    pHostCmd2 = NULL;

                    int rc2 = pClient->Wakeup();
                    LogFlowFunc(("Woke up client ID=%RU32 -> rc=%Rrc\n", pClient->m_idClient, rc2));
                    RT_NOREF(rc2);
                    rc = VINF_SUCCESS;
                }
                else
                {
                    LogFunc(("No client with session ID %u was found! (idFunction=%d %s)\n",
                             idSession, idFunction, GstCtrlHostFnName((eHostFn)idFunction)));
                    rc = !(fDestinations & VBOX_GUESTCTRL_DST_ROOT_SVC) ? VERR_NOT_FOUND : VWRN_NOT_FOUND;
                }
            }

            /* Does the message go to the root service? */
            if (   (fDestinations & VBOX_GUESTCTRL_DST_ROOT_SVC)
                && RT_SUCCESS(rc))
            {
                Assert(pHostCmd);
                if (m_pMasterClient)
                {
                    RTListAppend(&m_pMasterClient->m_HostCmdList, &pHostCmd->m_ListEntry);
                    pHostCmd = NULL;

                    int rc2 = m_pMasterClient->Wakeup();
                    LogFlowFunc(("Woke up client ID=%RU32 (master) -> rc=%Rrc\n", m_pMasterClient->m_idClient, rc2));
                    NOREF(rc2);
                }
                else
                    rc = VERR_NOT_FOUND;
            }
        }

        /* Drop unset commands */
        if (pHostCmd2)
            pHostCmd2->Delete();
    }
    if (pHostCmd)
        pHostCmd->Delete();

    if (RT_FAILURE(rc))
        LogFunc(("Failed %Rrc (idFunction=%u, cParms=%u)\n", rc, idFunction, cParms));
    return rc;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnHostCall,
 *  Wraps to the hostProcessCommand() member function.}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcHostCall(void *pvService, uint32_t u32Function, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    AssertLogRelReturn(VALID_PTR(pvService), VERR_INVALID_PARAMETER);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("fn=%RU32, cParms=%RU32, paParms=0x%p\n", u32Function, cParms, paParms));
    AssertReturn(u32Function != HOST_CANCEL_PENDING_WAITS, VERR_INVALID_FUNCTION);
    return pThis->hostProcessCommand(u32Function, cParms, paParms);
}




/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnSaveState}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcSaveState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM)
{
    RT_NOREF(pvClient);
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    /* Note! We don't need to save the idSession here because it's only used
             for sessions and the sessions are not persistent across a state
             save/restore.  The Main objects aren't there.  Clients shuts down.
             Only the root service survives, so remember who that is and its mode. */

    SSMR3PutU32(pSSM, 1);
    SSMR3PutBool(pSSM, pThis->m_fLegacyMode);
    return SSMR3PutBool(pSSM, idClient == pThis->m_idMasterClient);
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnLoadState}
 */
/*static*/ DECLCALLBACK(int)
GstCtrlService::svcLoadState(void *pvService, uint32_t idClient, void *pvClient, PSSMHANDLE pSSM, uint32_t uVersion)
{
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    ClientState *pClient = reinterpret_cast<ClientState *>(pvClient);
    AssertReturn(pClient, VERR_INVALID_CLIENT_ID);
    Assert(pClient->m_idClient == idClient);

    if (uVersion >= HGCM_SAVED_STATE_VERSION)
    {
        uint32_t uSubVersion;
        int rc = SSMR3GetU32(pSSM, &uSubVersion);
        AssertRCReturn(rc, rc);
        if (uSubVersion != 1)
            return SSMR3SetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                     "sub version %u, expected 1\n", uSubVersion);
        bool fLegacyMode;
        rc = SSMR3GetBool(pSSM, &fLegacyMode);
        AssertRCReturn(rc, rc);
        pThis->m_fLegacyMode = fLegacyMode;

        bool fIsMaster;
        rc = SSMR3GetBool(pSSM, &fIsMaster);
        AssertRCReturn(rc, rc);

        pClient->m_fIsMaster = fIsMaster;
        if (fIsMaster)
        {
            pThis->m_pMasterClient  = pClient;
            pThis->m_idMasterClient = idClient;
        }
    }
    else
    {
        /*
         * For old saved states we have to guess at who should be the master.
         * Given how HGCMService::CreateAndConnectClient and associates manage
         * and saves the client, the first client connecting will be restored
         * first.  The only time this might go wrong if the there are zombie
         * VBoxService session processes in the restored guest, and I don't
         * we need to care too much about that scenario.
         *
         * Given how HGCM first re-connects the clients before this function
         * gets called, there isn't anything we need to do here it turns out. :-)
         */
    }
    pClient->m_fRestored = true;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXHGCMSVCFNTABLE,pfnRegisterExtension,
 *  Installs a host callback for notifications of property changes.}
 */
/*static*/ DECLCALLBACK(int) GstCtrlService::svcRegisterExtension(void *pvService, PFNHGCMSVCEXT pfnExtension, void *pvExtension)
{
    SELF *pThis = reinterpret_cast<SELF *>(pvService);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnExtension, VERR_INVALID_POINTER);

    pThis->mpfnHostCallback = pfnExtension;
    pThis->mpvHostData      = pvExtension;
    return VINF_SUCCESS;
}


/**
 * @copydoc VBOXHGCMSVCLOAD
 */
extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad(VBOXHGCMSVCFNTABLE *pTable)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pTable=%p\n", pTable));

    if (!VALID_PTR(pTable))
    {
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        LogFlowFunc(("pTable->cbSize=%d, pTable->u32Version=0x%08X\n", pTable->cbSize, pTable->u32Version));

        if (   pTable->cbSize != sizeof (VBOXHGCMSVCFNTABLE)
            || pTable->u32Version != VBOX_HGCM_SVC_VERSION)
        {
            rc = VERR_VERSION_MISMATCH;
        }
        else
        {
            GstCtrlService *pService = NULL;
            /* No exceptions may propagate outside. */
            try
            {
                pService = new GstCtrlService(pTable->pHelpers);
            }
            catch (int rcThrown)
            {
                rc = rcThrown;
            }
            catch(std::bad_alloc &)
            {
                rc = VERR_NO_MEMORY;
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * We don't need an additional client data area on the host,
                 * because we're a class which can have members for that :-).
                 */
                pTable->cbClient = sizeof(ClientState);

                /* Register functions. */
                pTable->pfnUnload               = GstCtrlService::svcUnload;
                pTable->pfnConnect              = GstCtrlService::svcConnect;
                pTable->pfnDisconnect           = GstCtrlService::svcDisconnect;
                pTable->pfnCall                 = GstCtrlService::svcCall;
                pTable->pfnHostCall             = GstCtrlService::svcHostCall;
                pTable->pfnSaveState            = GstCtrlService::svcSaveState;
                pTable->pfnLoadState            = GstCtrlService::svcLoadState;
                pTable->pfnRegisterExtension    = GstCtrlService::svcRegisterExtension;
                pTable->pfnNotify               = NULL;

                /* Service specific initialization. */
                pTable->pvService = pService;
            }
            else
            {
                if (pService)
                {
                    delete pService;
                    pService = NULL;
                }
            }
        }
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
    return rc;
}

