/* $Id$ */
/** @file
 * HGCMThread - Host-Guest Communication Manager Threads
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
 */

#define LOG_GROUP LOG_GROUP_HGCM
#include "LoggingNew.h"

#include "HGCMThread.h"

#include <VBox/err.h>
#include <VBox/vmm/stam.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/string.h>

#include <new> /* for std:nothrow */


/* HGCM uses worker threads, which process messages from other threads.
 * A message consists of the message header and message specific data.
 * Message header is opaque for callers, but message data is defined
 * and used by them.
 *
 * Messages are distinguished by message identifier and worker thread
 * they are allocated for.
 *
 * Messages are allocated for a worker thread and belong to
 * the thread. A worker thread holds the queue of messages.
 *
 * The calling thread creates a message, specifying which worker thread
 * the message is created for, then, optionally, initializes message
 * specific data and, also optionally, references the message.
 *
 * Message then is posted or sent to worker thread by inserting
 * it to the worker thread message queue and referencing the message.
 * Worker thread then again may fetch next message.
 *
 * Upon processing the message the worker thread dereferences it.
 * Dereferencing also automatically deletes message from the thread
 * queue and frees memory allocated for the message, if no more
 * references left. If there are references, the message remains
 * in the queue.
 *
 */

/* Version of HGCM message header */
#define HGCMMSG_VERSION (1)

/* Thread is initializing. */
#define HGCMMSG_TF_INITIALIZING        (0x00000001)
/* Thread must be terminated. */
#define HGCMMSG_TF_TERMINATE           (0x00000002)
/* Thread has been terminated. */
#define HGCMMSG_TF_TERMINATED          (0x00000004)

/** @todo consider use of RTReq */

static DECLCALLBACK(int) hgcmWorkerThreadFunc(RTTHREAD ThreadSelf, void *pvUser);

class HGCMThread : public HGCMReferencedObject
{
    private:
    friend DECLCALLBACK(int) hgcmWorkerThreadFunc(RTTHREAD ThreadSelf, void *pvUser);

        /* Worker thread function. */
        PFNHGCMTHREAD m_pfnThread;

        /* A user supplied thread parameter. */
        void *m_pvUser;

        /* The thread runtime handle. */
        RTTHREAD m_hThread;

        /** Event the thread waits for, signalled when a message to process is posted to
         * the thread, automatically reset. */
        RTSEMEVENT m_eventThread;

        /* A caller thread waits for completion of a SENT message on this event. */
        RTSEMEVENTMULTI m_eventSend;
        int32_t volatile m_i32MessagesProcessed;

        /* Critical section for accessing the thread data, mostly for message queues. */
        RTCRITSECT m_critsect;

        /* thread state/operation flags */
        uint32_t m_fu32ThreadFlags;

        /* Message queue variables. Messages are inserted at tail of message
         * queue. They are consumed by worker thread sequentially. If a message was
         * consumed, it is removed from message queue.
         */

        /* Head of message queue. */
        HGCMMsgCore *m_pMsgInputQueueHead;
        /* Message which another message will be inserted after. */
        HGCMMsgCore *m_pMsgInputQueueTail;

        /* Head of messages being processed queue. */
        HGCMMsgCore *m_pMsgInProcessHead;
        /* Message which another message will be inserted after. */
        HGCMMsgCore *m_pMsgInProcessTail;

        /* Head of free message structures list. */
        HGCMMsgCore *m_pFreeHead;
        /* Tail of free message structures list. */
        HGCMMsgCore *m_pFreeTail;

        /** @name Statistics
         * @{ */
        STAMCOUNTER m_StatPostMsgNoPending;
        STAMCOUNTER m_StatPostMsgOnePending;
        STAMCOUNTER m_StatPostMsgTwoPending;
        STAMCOUNTER m_StatPostMsgThreePending;
        STAMCOUNTER m_StatPostMsgManyPending;
        /** @} */

        inline int Enter(void);
        inline void Leave(void);

        HGCMMsgCore *FetchFreeListHead(void);

    protected:
        virtual ~HGCMThread(void);

    public:

        HGCMThread ();

        int WaitForTermination (void);

        int Initialize(const char *pszThreadName, PFNHGCMTHREAD pfnThread, void *pvUser, const char *pszStatsSubDir, PUVM pUVM);

        int MsgAlloc(HGCMMsgCore **pMsg, uint32_t u32MsgId, PFNHGCMNEWMSGALLOC pfnNewMessage);
        int MsgGet(HGCMMsgCore **ppMsg);
        int MsgPost(HGCMMsgCore *pMsg, PHGCMMSGCALLBACK pfnCallback, bool bWait);
        int MsgComplete(HGCMMsgCore *pMsg, int32_t result);
};


/*
 * HGCMMsgCore implementation.
 */

#define HGCM_MSG_F_PROCESSED  (0x00000001)
#define HGCM_MSG_F_WAIT       (0x00000002)
#define HGCM_MSG_F_IN_PROCESS (0x00000004)

void HGCMMsgCore::InitializeCore(uint32_t u32MsgId, HGCMThread *pThread)
{
    m_u32Version  = HGCMMSG_VERSION;
    m_u32Msg      = u32MsgId;
    m_pfnCallback = NULL;
    m_pNext       = NULL;
    m_pPrev       = NULL;
    m_fu32Flags   = 0;
    m_rcSend      = VINF_SUCCESS;
    m_pThread     = pThread;
    pThread->Reference();
}

/* virtual */ HGCMMsgCore::~HGCMMsgCore()
{
    if (m_pThread)
    {
        m_pThread->Dereference();
        m_pThread = NULL;
    }
}

/*
 * HGCMThread implementation.
 */

static DECLCALLBACK(int) hgcmWorkerThreadFunc(RTTHREAD hThreadSelf, void *pvUser)
{
    HGCMThread *pThread = (HGCMThread *)pvUser;

    LogFlow(("MAIN::hgcmWorkerThreadFunc: starting HGCM thread %p\n", pThread));

    AssertRelease(pThread);

    pThread->m_hThread = hThreadSelf;
    pThread->m_fu32ThreadFlags &= ~HGCMMSG_TF_INITIALIZING;
    int rc = RTThreadUserSignal(hThreadSelf);
    AssertRC(rc);

    pThread->m_pfnThread(pThread, pThread->m_pvUser);

    pThread->m_fu32ThreadFlags |= HGCMMSG_TF_TERMINATED;

    pThread->m_hThread = NIL_RTTHREAD;

    LogFlow(("MAIN::hgcmWorkerThreadFunc: completed HGCM thread %p\n", pThread));

    return rc;
}

HGCMThread::HGCMThread()
    :
    HGCMReferencedObject(HGCMOBJ_THREAD),
    m_pfnThread(NULL),
    m_pvUser(NULL),
    m_hThread(NIL_RTTHREAD),
    m_eventThread(NIL_RTSEMEVENT),
    m_eventSend(NIL_RTSEMEVENTMULTI),
    m_i32MessagesProcessed(0),
    m_fu32ThreadFlags(0),
    m_pMsgInputQueueHead(NULL),
    m_pMsgInputQueueTail(NULL),
    m_pMsgInProcessHead(NULL),
    m_pMsgInProcessTail(NULL),
    m_pFreeHead(NULL),
    m_pFreeTail(NULL)
{
    RT_ZERO(m_critsect);
}

HGCMThread::~HGCMThread()
{
    /*
     * Free resources allocated for the thread.
     */

    Assert(m_fu32ThreadFlags & HGCMMSG_TF_TERMINATED);

    if (RTCritSectIsInitialized(&m_critsect))
        RTCritSectDelete(&m_critsect);

    if (m_eventSend != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(m_eventSend);
        m_eventSend = NIL_RTSEMEVENTMULTI;
    }

    if (m_eventThread != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(m_eventThread);
        m_eventThread = NIL_RTSEMEVENT;
    }
}

int HGCMThread::WaitForTermination(void)
{
    int rc = VINF_SUCCESS;
    LogFlowFunc(("\n"));

    if (m_hThread != NIL_RTTHREAD)
        rc = RTThreadWait(m_hThread, 5000, NULL);

    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

int HGCMThread::Initialize(const char *pszThreadName, PFNHGCMTHREAD pfnThread, void *pvUser, const char *pszStatsSubDir, PUVM pUVM)
{
    int rc = RTSemEventCreate(&m_eventThread);

    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventMultiCreate(&m_eventSend);

        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&m_critsect);

            if (RT_SUCCESS(rc))
            {
                m_pfnThread = pfnThread;
                m_pvUser    = pvUser;

                m_fu32ThreadFlags = HGCMMSG_TF_INITIALIZING;

                RTTHREAD hThread;
                rc = RTThreadCreate(&hThread, hgcmWorkerThreadFunc, this, 0, /* default stack size; some services
                                                                                may need quite a bit */
                                    RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE,
                                    pszThreadName);

                if (RT_SUCCESS(rc))
                {
                    /* Register statistics while the thread starts. */
                    if (pUVM)
                    {
                        STAMR3RegisterFU(pUVM, &m_StatPostMsgNoPending, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                         "Times a message was appended to an empty input queue.",
                                         "/HGCM/%s/PostMsg0Pending", pszStatsSubDir);
                        STAMR3RegisterFU(pUVM, &m_StatPostMsgOnePending, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                         "Times a message was appended to input queue with only one pending message.",
                                         "/HGCM/%s/PostMsg1Pending", pszStatsSubDir);
                        STAMR3RegisterFU(pUVM, &m_StatPostMsgTwoPending, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                         "Times a message was appended to input queue with only one pending message.",
                                         "/HGCM/%s/PostMsg2Pending", pszStatsSubDir);
                        STAMR3RegisterFU(pUVM, &m_StatPostMsgTwoPending, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                         "Times a message was appended to input queue with only one pending message.",
                                         "/HGCM/%s/PostMsg3Pending", pszStatsSubDir);
                        STAMR3RegisterFU(pUVM, &m_StatPostMsgManyPending, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                                         "Times a message was appended to input queue with only one pending message.",
                                         "/HGCM/%s/PostMsgManyPending", pszStatsSubDir);
                    }


                    /* Wait until the thread is ready. */
                    rc = RTThreadUserWait(hThread, 30000);
                    AssertRC(rc);
                    Assert(!(m_fu32ThreadFlags & HGCMMSG_TF_INITIALIZING) || RT_FAILURE(rc));
                }
                else
                {
                    m_hThread = NIL_RTTHREAD;
                    Log(("hgcmThreadCreate: FAILURE: Can't start worker thread.\n"));
                }
            }
            else
            {
                Log(("hgcmThreadCreate: FAILURE: Can't init a critical section for a hgcm worker thread.\n"));
                RT_ZERO(m_critsect);
            }
        }
        else
        {
            Log(("hgcmThreadCreate: FAILURE: Can't create an event semaphore for a sent messages.\n"));
            m_eventSend = NIL_RTSEMEVENTMULTI;
        }
    }
    else
    {
        Log(("hgcmThreadCreate: FAILURE: Can't create an event semaphore for a hgcm worker thread.\n"));
        m_eventThread = NIL_RTSEMEVENT;
    }

    return rc;
}

inline int HGCMThread::Enter(void)
{
    int rc = RTCritSectEnter(&m_critsect);

#ifdef LOG_ENABLED
    if (RT_FAILURE(rc))
        Log(("HGCMThread::MsgPost: FAILURE: could not obtain worker thread mutex, rc = %Rrc!!!\n", rc));
#endif

    return rc;
}

inline void HGCMThread::Leave(void)
{
    RTCritSectLeave(&m_critsect);
}


int HGCMThread::MsgAlloc(HGCMMsgCore **ppMsg, uint32_t u32MsgId, PFNHGCMNEWMSGALLOC pfnNewMessage)
{
    /** @todo  Implement this free list / cache thingy.   */
    HGCMMsgCore *pmsg = NULL;

    bool fFromFreeList = false;

    if (!pmsg)
    {
        /* We have to allocate a new memory block. */
        pmsg = pfnNewMessage(u32MsgId);
        if (pmsg != NULL)
            pmsg->Reference(); /* (it's created with zero references) */
        else
            return VERR_NO_MEMORY;
    }

    /* Initialize just allocated message core */
    pmsg->InitializeCore(u32MsgId, this);

    /* and the message specific data. */
    pmsg->Initialize();

    LogFlow(("MAIN::hgcmMsgAlloc: allocated message %p\n", pmsg));

    *ppMsg = pmsg;

    if (fFromFreeList)
    {
        /* Message was referenced in the free list, now dereference it. */
        pmsg->Dereference();
    }

    return VINF_SUCCESS;
}

int HGCMThread::MsgPost(HGCMMsgCore *pMsg, PHGCMMSGCALLBACK pfnCallback, bool fWait)
{
    LogFlow(("HGCMThread::MsgPost: thread = %p, pMsg = %p, pfnCallback = %p\n", this, pMsg, pfnCallback));

    int rc = Enter();

    if (RT_SUCCESS(rc))
    {
        pMsg->m_pfnCallback = pfnCallback;

        if (fWait)
            pMsg->m_fu32Flags |= HGCM_MSG_F_WAIT;

        /* Insert the message to the queue tail. */
        pMsg->m_pNext = NULL;
        HGCMMsgCore * const pPrev = m_pMsgInputQueueTail;
        pMsg->m_pPrev = pPrev;

        if (pPrev)
        {
            pPrev->m_pNext = pMsg;
            if (!pPrev->m_pPrev)
                STAM_REL_COUNTER_INC(&m_StatPostMsgOnePending);
            else if (!pPrev->m_pPrev)
                STAM_REL_COUNTER_INC(&m_StatPostMsgTwoPending);
            else if (!pPrev->m_pPrev->m_pPrev)
                STAM_REL_COUNTER_INC(&m_StatPostMsgThreePending);
            else
                STAM_REL_COUNTER_INC(&m_StatPostMsgManyPending);
        }
        else
        {
            m_pMsgInputQueueHead = pMsg;
            STAM_REL_COUNTER_INC(&m_StatPostMsgNoPending);
        }

        m_pMsgInputQueueTail = pMsg;

        Leave();

        LogFlow(("HGCMThread::MsgPost: going to inform the thread %p about message, fWait = %d\n", this, fWait));

        /* Inform the worker thread that there is a message. */
        RTSemEventSignal(m_eventThread);

        LogFlow(("HGCMThread::MsgPost: event signalled\n"));

        if (fWait)
        {
            /* Immediately check if the message has been processed. */
            while ((pMsg->m_fu32Flags & HGCM_MSG_F_PROCESSED) == 0)
            {
                /* Poll infrequently to make sure no completed message has been missed. */
                RTSemEventMultiWait(m_eventSend, 1000);

                LogFlow(("HGCMThread::MsgPost: wait completed flags = %08X\n", pMsg->m_fu32Flags));

                if ((pMsg->m_fu32Flags & HGCM_MSG_F_PROCESSED) == 0)
                    RTThreadYield();
            }

            /* 'Our' message has been processed, so should reset the semaphore.
             * There is still possible that another message has been processed
             * and the semaphore has been signalled again.
             * Reset only if there are no other messages completed.
             */
            int32_t c = ASMAtomicDecS32(&m_i32MessagesProcessed);
            Assert(c >= 0);
            if (c == 0)
                RTSemEventMultiReset(m_eventSend);

            rc = pMsg->m_rcSend;
        }
    }

    LogFlow(("HGCMThread::MsgPost: rc = %Rrc\n", rc));
    return rc;
}


int HGCMThread::MsgGet(HGCMMsgCore **ppMsg)
{
    int rc = VINF_SUCCESS;

    LogFlow(("HGCMThread::MsgGet: thread = %p, ppMsg = %p\n", this, ppMsg));

    for (;;)
    {
        if (m_fu32ThreadFlags & HGCMMSG_TF_TERMINATE)
        {
            rc = VERR_INTERRUPTED;
            break;
        }

        LogFlow(("MAIN::hgcmMsgGet: m_pMsgInputQueueHead = %p\n", m_pMsgInputQueueHead));

        if (m_pMsgInputQueueHead)
        {
            /* Move the message to the m_pMsgInProcessHead list */
            rc = Enter();

            if (RT_FAILURE(rc))
            {
                break;
            }

            HGCMMsgCore *pMsg = m_pMsgInputQueueHead;

            /* Remove the message from the head of Queue list. */
            Assert(m_pMsgInputQueueHead->m_pPrev == NULL);

            if (m_pMsgInputQueueHead->m_pNext)
            {
                m_pMsgInputQueueHead = m_pMsgInputQueueHead->m_pNext;
                m_pMsgInputQueueHead->m_pPrev = NULL;
            }
            else
            {
                Assert(m_pMsgInputQueueHead == m_pMsgInputQueueTail);

                m_pMsgInputQueueHead = NULL;
                m_pMsgInputQueueTail = NULL;
            }

            /* Insert the message to the tail of the m_pMsgInProcessHead list. */
            pMsg->m_pNext = NULL;
            pMsg->m_pPrev = m_pMsgInProcessTail;

            if (m_pMsgInProcessTail)
                m_pMsgInProcessTail->m_pNext = pMsg;
            else
                m_pMsgInProcessHead = pMsg;

            m_pMsgInProcessTail = pMsg;

            pMsg->m_fu32Flags |= HGCM_MSG_F_IN_PROCESS;

            Leave();

            /* Return the message to the caller. */
            *ppMsg = pMsg;

            LogFlow(("MAIN::hgcmMsgGet: got message %p\n", *ppMsg));

            break;
        }

        /* Wait for an event. */
        RTSemEventWait(m_eventThread, RT_INDEFINITE_WAIT);
    }

    LogFlow(("HGCMThread::MsgGet: *ppMsg = %p, return rc = %Rrc\n", *ppMsg, rc));
    return rc;
}

int HGCMThread::MsgComplete(HGCMMsgCore *pMsg, int32_t result)
{
    LogFlow(("HGCMThread::MsgComplete: thread = %p, pMsg = %p, result = %Rrc (%d)\n", this, pMsg, result, result));

    AssertRelease(pMsg->m_pThread == this);
    AssertReleaseMsg((pMsg->m_fu32Flags & HGCM_MSG_F_IN_PROCESS) != 0, ("%p %x\n", pMsg, pMsg->m_fu32Flags));

    int rcRet = VINF_SUCCESS;
    if (pMsg->m_pfnCallback)
    {
        /** @todo call callback with error code in MsgPost in case of errors */

        rcRet = pMsg->m_pfnCallback(result, pMsg);

        LogFlow(("HGCMThread::MsgComplete: callback executed. pMsg = %p, thread = %p, rcRet = %Rrc\n", pMsg, this, rcRet));
    }

    /* Message processing has been completed. */

    int rc = Enter();

    if (RT_SUCCESS(rc))
    {
        /* Remove the message from the InProcess queue. */

        if (pMsg->m_pNext)
            pMsg->m_pNext->m_pPrev = pMsg->m_pPrev;
        else
            m_pMsgInProcessTail = pMsg->m_pPrev;

        if (pMsg->m_pPrev)
            pMsg->m_pPrev->m_pNext = pMsg->m_pNext;
        else
            m_pMsgInProcessHead = pMsg->m_pNext;

        pMsg->m_pNext = NULL;
        pMsg->m_pPrev = NULL;

        bool fWaited = ((pMsg->m_fu32Flags & HGCM_MSG_F_WAIT) != 0);

        if (fWaited)
        {
            ASMAtomicIncS32(&m_i32MessagesProcessed);

            /* This should be done before setting the HGCM_MSG_F_PROCESSED flag. */
            pMsg->m_rcSend = result;
        }

        /* The message is now completed. */
        pMsg->m_fu32Flags &= ~HGCM_MSG_F_IN_PROCESS;
        pMsg->m_fu32Flags &= ~HGCM_MSG_F_WAIT;
        pMsg->m_fu32Flags |= HGCM_MSG_F_PROCESSED;

        pMsg->Dereference();

        Leave();

        if (fWaited)
        {
            /* Wake up all waiters. so they can decide if their message has been processed. */
            RTSemEventMultiSignal(m_eventSend);
        }
    }

    return rcRet;
}

/*
 * Thread API. Public interface.
 */

int hgcmThreadCreate(HGCMThread **ppThread, const char *pszThreadName, PFNHGCMTHREAD pfnThread, void *pvUser,
                     const char *pszStatsSubDir, PUVM pUVM)
{
    LogFlow(("MAIN::hgcmThreadCreate\n"));
    int rc;

    /* Allocate memory for a new thread object. */
    HGCMThread *pThread = new (std::nothrow) HGCMThread();

    if (pThread)
    {
        pThread->Reference(); /* (it's created with zero references) */

        /* Initialize the object. */
        rc = pThread->Initialize(pszThreadName, pfnThread, pvUser, pszStatsSubDir, pUVM);
        if (RT_SUCCESS(rc))
        {
            *ppThread = pThread;
            LogFlow(("MAIN::hgcmThreadCreate: rc = %Rrc\n", rc));
            return rc;
        }

        Log(("hgcmThreadCreate: FAILURE: Initialize failed: rc = %Rrc\n", rc));

        pThread->Dereference();
    }
    else
    {
        Log(("hgcmThreadCreate: FAILURE: Can't allocate memory for a hgcm worker thread.\n"));
        rc = VERR_NO_MEMORY;
    }
    *ppThread = NULL;

    LogFlow(("MAIN::hgcmThreadCreate: rc = %Rrc\n", rc));
    return rc;
}

int hgcmThreadWait(HGCMThread *pThread)
{
    LogFlowFunc(("%p\n", pThread));

    int rc;
    if (pThread)
    {
        rc = pThread->WaitForTermination();

        pThread->Dereference();
    }
    else
        rc = VERR_INVALID_HANDLE;

    LogFlowFunc(("rc = %Rrc\n", rc));
    return rc;
}

int hgcmMsgAlloc(HGCMThread *pThread, HGCMMsgCore **ppMsg, uint32_t u32MsgId, PFNHGCMNEWMSGALLOC pfnNewMessage)
{
    LogFlow(("hgcmMsgAlloc: pThread = %p, ppMsg = %p, sizeof (HGCMMsgCore) = %d\n", pThread, ppMsg, sizeof(HGCMMsgCore)));

    AssertReturn(pThread, VERR_INVALID_HANDLE);
    AssertReturn(ppMsg, VERR_INVALID_PARAMETER);

    int rc = pThread->MsgAlloc(ppMsg, u32MsgId, pfnNewMessage);

    LogFlow(("MAIN::hgcmMsgAlloc: *ppMsg = %p, rc = %Rrc\n", *ppMsg, rc));
    return rc;
}

DECLINLINE(int) hgcmMsgPostInternal(HGCMMsgCore *pMsg, PHGCMMSGCALLBACK pfnCallback, bool fWait)
{
    LogFlow(("MAIN::hgcmMsgPostInternal: pMsg = %p, pfnCallback = %p, fWait = %d\n", pMsg, pfnCallback, fWait));
    Assert(pMsg);

    pMsg->Reference(); /* paranoia? */

    int rc = pMsg->Thread()->MsgPost(pMsg, pfnCallback, fWait);

    pMsg->Dereference();

    LogFlow(("MAIN::hgcmMsgPostInternal: pMsg %p, rc = %Rrc\n", pMsg, rc));
    return rc;
}

int hgcmMsgPost(HGCMMsgCore *pMsg, PHGCMMSGCALLBACK pfnCallback)
{
    int rc = hgcmMsgPostInternal(pMsg, pfnCallback, false);

    if (RT_SUCCESS(rc))
        rc = VINF_HGCM_ASYNC_EXECUTE;

    return rc;
}

int hgcmMsgSend(HGCMMsgCore *pMsg)
{
    return hgcmMsgPostInternal(pMsg, NULL, true);
}

int hgcmMsgGet(HGCMThread *pThread, HGCMMsgCore **ppMsg)
{
    LogFlow(("MAIN::hgcmMsgGet: pThread = %p, ppMsg = %p\n", pThread, ppMsg));

    AssertReturn(pThread, VERR_INVALID_HANDLE);
    AssertReturn(ppMsg, VERR_INVALID_PARAMETER);

    pThread->Reference();           /* paranoia */

    int rc = pThread->MsgGet(ppMsg);

    pThread->Dereference();

    LogFlow(("MAIN::hgcmMsgGet: *ppMsg = %p, rc = %Rrc\n", *ppMsg, rc));
    return rc;
}

int hgcmMsgComplete(HGCMMsgCore *pMsg, int32_t rcMsg)
{
    LogFlow(("MAIN::hgcmMsgComplete: pMsg = %p, rcMsg = %Rrc (%d)\n", pMsg, rcMsg, rcMsg));

    int rc;
    if (pMsg)
        rc = pMsg->Thread()->MsgComplete(pMsg, rcMsg);
    else
        rc = VINF_SUCCESS;

    LogFlow(("MAIN::hgcmMsgComplete: pMsg = %p, rcMsg =%Rrc (%d), returns rc = %Rrc\n", pMsg, rcMsg, rcMsg, rc));
    return rc;
}

int hgcmThreadInit(void)
{
    LogFlow(("MAIN::hgcmThreadInit\n"));

    /** @todo error processing. */

    int rc = hgcmObjInit();

    LogFlow(("MAIN::hgcmThreadInit: rc = %Rrc\n", rc));
    return rc;
}

void hgcmThreadUninit(void)
{
    hgcmObjUninit();
}

