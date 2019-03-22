/* $Id$ */
/** @file
 * Implementation of IUSBController.
 */

/*
 * Copyright (C) 2005-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "USBControllerImpl.h"

#include "Global.h"
#include "MachineImpl.h"
#include "VirtualBoxImpl.h"
#include "HostImpl.h"

#include <iprt/string.h>
#include <iprt/cpp/utils.h>

#include <VBox/err.h>
#include <VBox/settings.h>
#include <VBox/com/array.h>

#include <algorithm>

#include "AutoStateDep.h"
#include "AutoCaller.h"
#include "Logging.h"

// defines
/////////////////////////////////////////////////////////////////////////////

struct USBController::Data
{
    Data(Machine *pMachine)
        : pParent(pMachine)
    { }

    ~Data()
    {};

    Machine * const                       pParent;

    // peer machine's USB controller
    const ComObjPtr<USBController>        pPeer;

    Backupable<settings::USBController>   bd;
};



// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(USBController)

HRESULT USBController::FinalConstruct()
{
    return BaseFinalConstruct();
}

void USBController::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the USB controller object.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aName         The name of the USB controller.
 * @param enmType       The USB controller type.
 */
HRESULT USBController::init(Machine *aParent, const Utf8Str &aName, USBControllerType_T enmType)
{
    LogFlowThisFunc(("aParent=%p aName=\"%s\"\n", aParent, aName.c_str()));

    ComAssertRet(aParent && !aName.isEmpty(), E_INVALIDARG);
    if (   (enmType <= USBControllerType_Null)
        || (enmType >  USBControllerType_XHCI))
        return setError(E_INVALIDARG,
                        tr("Invalid USB controller type"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    m->bd.allocate();
    m->bd->strName = aName;
    m->bd->enmType = enmType;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 * Initializes the USB controller object given another USB controller object
 * (a kind of copy constructor). This object shares data with
 * the object passed as an argument.
 *
 * @returns COM result indicator.
 * @param aParent       Pointer to our parent object.
 * @param aPeer         The object to share.
 * @param fReshare
 *      When false, the original object will remain a data owner.
 *      Otherwise, data ownership will be transferred from the original
 *      object to this one.
 *
 *  @note This object must be destroyed before the original object
 *  it shares data with is destroyed.
 *
 *  @note Locks @a aThat object for writing if @a aReshare is @c true, or for
 *  reading if @a aReshare is false.
 */
HRESULT USBController::init(Machine *aParent, USBController *aPeer,
                            bool fReshare /* = false */)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p, fReshare=%RTbool\n",
                      aParent, aPeer, fReshare));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* sanity */
    AutoCaller peerCaller(aPeer);
    AssertComRCReturnRC(peerCaller.rc());

    if (fReshare)
    {
        AutoWriteLock peerLock(aPeer COMMA_LOCKVAL_SRC_POS);

        unconst(aPeer->m->pPeer) = this;
        m->bd.attach(aPeer->m->bd);
    }
    else
    {
        unconst(m->pPeer) = aPeer;

        AutoReadLock peerLock(aPeer COMMA_LOCKVAL_SRC_POS);
        m->bd.share(aPeer->m->bd);
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 *  Initializes the USB controller object given another guest object
 *  (a kind of copy constructor). This object makes a private copy of data
 *  of the original object passed as an argument.
 */
HRESULT USBController::initCopy(Machine *aParent, USBController *aPeer)
{
    LogFlowThisFunc(("aParent=%p, aPeer=%p\n", aParent, aPeer));

    ComAssertRet(aParent && aPeer, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m = new Data(aParent);

    /* mPeer is left null */

    AutoWriteLock thatlock(aPeer COMMA_LOCKVAL_SRC_POS);
    m->bd.attachCopy(aPeer->m->bd);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}


/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void USBController::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    m->bd.free();

    unconst(m->pPeer) = NULL;
    unconst(m->pParent) = NULL;

    delete m;
    m = NULL;
}


// Wrapped IUSBController properties
/////////////////////////////////////////////////////////////////////////////
HRESULT USBController::getName(com::Utf8Str &aName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aName = m->bd->strName;

    return S_OK;
}

HRESULT USBController::setName(const com::Utf8Str &aName)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoMultiWriteLock2 alock(m->pParent, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->strName != aName)
    {
        ComObjPtr<USBController> ctrl;
        HRESULT rc = m->pParent->i_getUSBControllerByName(aName, ctrl, false /* aSetError */);
        if (SUCCEEDED(rc))
            return setError(VBOX_E_OBJECT_IN_USE,
                            tr("USB controller named '%s' already exists"),
                            aName.c_str());

        m->bd.backup();
        m->bd->strName = aName;

        m->pParent->i_setModified(Machine::IsModified_USB);
        alock.release();

        m->pParent->i_onUSBControllerChange();
    }

    return S_OK;
}

HRESULT USBController::getType(USBControllerType_T *aType)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aType = m->bd->enmType;

    return S_OK;
}

HRESULT USBController::setType(USBControllerType_T aType)
{
    /* the machine needs to be mutable */
    AutoMutableStateDependency adep(m->pParent);
    if (FAILED(adep.rc())) return adep.rc();

    AutoMultiWriteLock2 alock(m->pParent, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd->enmType != aType)
    {
        m->bd.backup();
        m->bd->enmType = aType;

        m->pParent->i_setModified(Machine::IsModified_USB);
        alock.release();

        m->pParent->i_onUSBControllerChange();
    }

    return S_OK;
}

HRESULT USBController::getUSBStandard(USHORT *aUSBStandard)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    switch (m->bd->enmType)
    {
        case USBControllerType_OHCI:
            *aUSBStandard = 0x0101;
            break;
        case USBControllerType_EHCI:
            *aUSBStandard = 0x0200;
            break;
        case USBControllerType_XHCI:
            *aUSBStandard = 0x0200;
            break;
        default:
            AssertMsgFailedReturn(("Invalid controller type %d\n", m->bd->enmType),
                                  E_FAIL);
    }

    return S_OK;
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/** @note Locks objects for writing! */
void USBController::i_rollback()
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* we need the machine state */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    m->bd.rollback();
}

/**
 *  @note Locks this object for writing, together with the peer object (also
 *  for writing) if there is one.
 */
void USBController::i_commit()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* lock both for writing since we modify both (mPeer is "master" so locked
     * first) */
    AutoMultiWriteLock2 alock(m->pPeer, this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isBackedUp())
    {
        m->bd.commit();
        if (m->pPeer)
        {
            /* attach new data to the peer and reshare it */
            AutoWriteLock peerlock(m->pPeer COMMA_LOCKVAL_SRC_POS);
            m->pPeer->m->bd.attach(m->bd);
        }
    }
}

/**
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void USBController::i_copyFrom(USBController *aThat)
{
    AssertReturnVoid(aThat != NULL);

    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller thatCaller(aThat);
    AssertComRCReturnVoid(thatCaller.rc());

    /* even more sanity */
    AutoAnyStateDependency adep(m->pParent);
    AssertComRCReturnVoid(adep.rc());
    /* Machine::copyFrom() may not be called when the VM is running */
    AssertReturnVoid(!Global::IsOnline(adep.machineState()));

    /* peer is not modified, lock it for reading (aThat is "master" so locked
     * first) */
    AutoReadLock rl(aThat COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    /* this will back up current data */
    m->bd.assignCopy(aThat->m->bd);
}

/**
 *  Cancels sharing (if any) by making an independent copy of data.
 *  This operation also resets this object's peer to NULL.
 *
 *  @note Locks this object for writing, together with the peer object
 *  represented by @a aThat (locked for reading).
 */
void USBController::i_unshare()
{
    /* sanity */
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    /* sanity too */
    AutoCaller peerCaller(m->pPeer);
    AssertComRCReturnVoid(peerCaller.rc());

    /* peer is not modified, lock it for reading (m->pPeer is "master" so locked
     * first) */
    AutoReadLock rl(m->pPeer COMMA_LOCKVAL_SRC_POS);
    AutoWriteLock wl(this COMMA_LOCKVAL_SRC_POS);

    if (m->bd.isShared())
    {
        if (!m->bd.isBackedUp())
            m->bd.backup();

        m->bd.commit();
    }

    unconst(m->pPeer) = NULL;
}

const Utf8Str &USBController::i_getName() const
{
    return m->bd->strName;
}

const USBControllerType_T &USBController::i_getControllerType() const
{
    return m->bd->enmType;
}

ComObjPtr<USBController> USBController::i_getPeer()
{
    return m->pPeer;
}

/////////////////////////////////////////////////////////////////////////////
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
