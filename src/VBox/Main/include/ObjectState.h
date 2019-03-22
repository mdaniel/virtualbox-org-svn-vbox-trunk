/* $Id$ */
/** @file
 *
 * VirtualBox object state handling definitions
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

#ifndef ____H_OBJECTSTATE
#define ____H_OBJECTSTATE

#include "VBox/com/defs.h"
#include "VBox/com/AutoLock.h"
#include "VBox/com/ErrorInfo.h"

// Forward declaration needed, but nothing more.
class VirtualBoxBase;

////////////////////////////////////////////////////////////////////////////////
//
// ObjectState
//
////////////////////////////////////////////////////////////////////////////////

/**
 * Thec functionality implemented by this class is the primary object state
 * (used by VirtualBoxBase and thus part of all API classes) that indicates
 * if the object is ready to serve the calls, and if not, what stage it is
 * currently at. Here is the primary state diagram:
 *
 *              +-------------------------------------------------------+
 *              |                                                       |
 *              |         (InitFailed) -----------------------+         |
 *              |              ^                              |         |
 *              v              |                              v         |
 *  [*] ---> NotReady ----> (InInit) -----> Ready -----> (InUninit) ----+
 *                     ^       |
 *                     |       v
 *                     |    Limited
 *                     |       |
 *                     +-------+
 *
 * The object is fully operational only when its state is Ready. The Limited
 * state means that only some vital part of the object is operational, and it
 * requires some sort of reinitialization to become fully operational. The
 * NotReady state means the object is basically dead: it either was not yet
 * initialized after creation at all, or was uninitialized and is waiting to be
 * destroyed when the last reference to it is released. All other states are
 * transitional.
 *
 * The NotReady->InInit->Ready, NotReady->InInit->Limited and
 * NotReady->InInit->InitFailed transition is done by the AutoInitSpan smart
 * class.
 *
 * The Limited->InInit->Ready, Limited->InInit->Limited and
 * Limited->InInit->InitFailed transition is done by the AutoReinitSpan smart
 * class.
 *
 * The Ready->InUninit->NotReady and InitFailed->InUninit->NotReady
 * transitions are done by the AutoUninitSpan smart class.
 *
 * In order to maintain the primary state integrity and declared functionality
 * the following rules apply everywhere:
 *
 * 1) Use the above Auto*Span classes to perform state transitions. See the
 *    individual class descriptions for details.
 *
 * 2) All public methods of subclasses (i.e. all methods that can be called
 *    directly, not only from within other methods of the subclass) must have a
 *    standard prolog as described in the AutoCaller and AutoLimitedCaller
 *    documentation. Alternatively, they must use #addCaller() and
 *    #releaseCaller() directly (and therefore have both the prolog and the
 *    epilog), but this is not recommended because it is easy to forget the
 *    matching release, e.g. returning before reaching the call.
 */
class ObjectState
{
public:
    enum State { NotReady, Ready, InInit, InUninit, InitFailed, Limited };

    ObjectState(VirtualBoxBase *aObj);
    ~ObjectState();

    State getState();

    HRESULT addCaller(bool aLimited = false);
    void releaseCaller();

    bool autoInitSpanConstructor(State aExpectedState);
    void autoInitSpanDestructor(State aNewState, HRESULT aFailedRC, com::ErrorInfo *aFailedEI);
    State autoUninitSpanConstructor();
    void autoUninitSpanDestructor();

private:
    ObjectState();

    void setState(State aState);

    /** Pointer to the managed object, mostly for error signalling or debugging
     * purposes, not used much. Guaranteed to be valid during the lifetime of
     * this object, no need to mess with refcount. */
    VirtualBoxBase *mObj;
    /** Primary state of this object */
    State mState;
    /** Thread that caused the last state change */
    RTTHREAD mStateChangeThread;
    /** Result code for failed object initialization */
    HRESULT mFailedRC;
    /** Error information for failed object initialization */
    com::ErrorInfo *mpFailedEI;
    /** Total number of active calls to this object */
    unsigned mCallers;
    /** Posted when the number of callers drops to zero */
    RTSEMEVENT mZeroCallersSem;
    /** Posted when the object goes from InInit/InUninit to some other state */
    RTSEMEVENTMULTI mInitUninitSem;
    /** Number of threads waiting for mInitUninitDoneSem */
    unsigned mInitUninitWaiters;

    /** Protects access to state related data members */
    util::RWLockHandle mStateLock;

private:
    DECLARE_CLS_COPY_CTOR_ASSIGN_NOOP(ObjectState); /* Shuts up MSC warning C4625. */
};

#endif // !____H_OBJECTSTATE
