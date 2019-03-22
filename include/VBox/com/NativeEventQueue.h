/** @file
 * MS COM / XPCOM Abstraction Layer - Event and EventQueue class declaration.
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

#ifndef ___VBox_com_EventQueue_h
#define ___VBox_com_EventQueue_h

#ifndef VBOX_WITH_XPCOM
# include <iprt/win/windows.h>
#else
# include <nsEventQueueUtils.h>
#endif

#include <VBox/com/defs.h>
#include <VBox/com/assert.h>


/** @defgroup grp_com_evt   Event and EventQueue Classes
 * @ingroup grp_com
 * @{
 */

namespace com
{

class MainEventQueue;

/**
 *  Base class for all events. Intended to be subclassed to introduce new
 *  events and handlers for them.
 *
 *  Subclasses usually reimplement virtual #handler() (that does nothing by
 *  default) and add new data members describing the event.
 */
class NativeEvent
{
public:

    NativeEvent() {}
    virtual ~NativeEvent() {};

protected:

    /**
     *  Event handler. Called in the context of the event queue's thread.
     *  Always reimplemented by subclasses
     *
     *  @return reserved, should be NULL.
     */
    virtual void *handler() { return NULL; }

    friend class NativeEventQueue;
};

/**
 *  Simple event queue.
 *
 *  When using XPCOM, this will map onto the default XPCOM queue for the thread.
 *  So, if a queue is created on the main thread, it automatically processes
 *  XPCOM/IPC events while waiting.
 *
 *  When using Windows, Darwin and OS/2, this will map onto the native thread
 *  queue/runloop.  So, windows messages and what not will be processed while
 *  waiting for events.
 *
 *  @note It is intentional that there is no way to retrieve arbitrary
 *  events and controlling their processing. There is no use case which
 *  warrants introducing the complexity of platform independent events.
 */
class NativeEventQueue
{
public:

    NativeEventQueue();
    virtual ~NativeEventQueue();

    BOOL postEvent(NativeEvent *event);
    int processEventQueue(RTMSINTERVAL cMsTimeout);
    int interruptEventQueueProcessing();
    int getSelectFD();
    static int init();
    static int uninit();
    static NativeEventQueue *getMainEventQueue();

#ifdef VBOX_WITH_XPCOM
    already_AddRefed<nsIEventQueue> getIEventQueue()
    {
        return mEventQ.get();
    }
#else
    static int dispatchMessageOnWindows(MSG const *pMsg, int rc);
#endif

private:
    static NativeEventQueue *sMainQueue;

#ifndef VBOX_WITH_XPCOM

    /** The thread which the queue belongs to. */
    DWORD mThreadId;
    /** Duplicated thread handle for MsgWaitForMultipleObjects. */
    HANDLE mhThread;

#else // VBOX_WITH_XPCOM

    /** Whether it was created (and thus needs destroying) or if a queue already
     *  associated with the thread was used. */
    bool mEQCreated;

    /** Whether event processing should be interrupted. */
    bool mInterrupted;

    nsCOMPtr <nsIEventQueue> mEventQ;
    nsCOMPtr <nsIEventQueueService> mEventQService;

    static void *PR_CALLBACK plEventHandler(PLEvent *self);
    static void PR_CALLBACK plEventDestructor(PLEvent *self);

#endif // VBOX_WITH_XPCOM
};

} /* namespace com */

/** @} */

#endif

