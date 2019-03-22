/* $Id$ */
/** @file
 * VBox Qt GUI - UIMainEventListener class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMainEventListener_h___
#define ___UIMainEventListener_h___

/* Qt includes: */
#include <QList>
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CGuestProcess.h"
#include "CGuestSession.h"
#include "CMediumAttachment.h"
#include "CNetworkAdapter.h"
#include "CUSBDevice.h"
#include "CVirtualBoxErrorInfo.h"

/* Other VBox includes: */
#include <VBox/com/listeners.h>

/* Forward declarations: */
class QPoint;
class QSize;
class QString;
class UIMainEventListeningThread;
class CEventListener;
class CEventSource;
class CGuestProcessStateChangedEvent;
class CGuestSessionStateChangedEvent;

/* Note: On a first look this may seems a little bit complicated.
 * There are two reasons to use a separate class here which handles the events
 * and forward them to the public class as signals. The first one is that on
 * some platforms (e.g. Win32) this events not arrive in the main GUI thread.
 * So there we have to make sure they are first delivered to the main GUI
 * thread and later executed there. The second reason is, that the initiator
 * method may hold a lock on a object which has to be manipulated in the event
 * consumer. Doing this without being asynchronous would lead to a dead lock. To
 * avoid both problems we send signals as a queued connection to the event
 * consumer. Qt will create a event for us, place it in the main GUI event
 * queue and deliver it later on. */

/** Main event listener. */
class SHARED_LIBRARY_STUFF UIMainEventListener : public QObject
{
    Q_OBJECT;

signals:

    /** @name VirtualBoxClient related signals
      * @{ */
        /** Notifies about the VBoxSVC become @a fAvailable. */
        void sigVBoxSVCAvailabilityChange(bool fAvailable);
    /** @} */

    /** @name VirtualBox related signals
      * @{ */
        /** Notifies about @a state change event for the machine with @a uId. */
        void sigMachineStateChange(const QUuid &uId, const KMachineState state);
        /** Notifies about data change event for the machine with @a uId. */
        void sigMachineDataChange(const QUuid &uId);
        /** Notifies about machine with @a uId was @a fRegistered. */
        void sigMachineRegistered(const QUuid &uId, const bool fRegistered);
        /** Notifies about @a state change event for the session of the machine with @a uId. */
        void sigSessionStateChange(const QUuid &uId, const KSessionState state);
        /** Notifies about snapshot with @a uSnapshotId was taken for the machine with @a uId. */
        void sigSnapshotTake(const QUuid &uId, const QUuid &uSnapshotId);
        /** Notifies about snapshot with @a uSnapshotId was deleted for the machine with @a uId. */
        void sigSnapshotDelete(const QUuid &uId, const QUuid &uSnapshotId);
        /** Notifies about snapshot with @a uSnapshotId was changed for the machine with @a uId. */
        void sigSnapshotChange(const QUuid &uId, const QUuid &uSnapshotId);
        /** Notifies about snapshot with @a uSnapshotId was restored for the machine with @a uId. */
        void sigSnapshotRestore(const QUuid &uId, const QUuid &uSnapshotId);
    /** @} */

    /** @name VirtualBox Extra-data related signals
      * @{ */
        /** Notifies about extra-data of the machine with @a uId can be changed for the key @a strKey to value @a strValue. */
        void sigExtraDataCanChange(const QUuid &uId, const QString &strKey, const QString &strValue, bool &fVeto, QString &strVetoReason); /* use Qt::DirectConnection */
        /** Notifies about extra-data of the machine with @a uId changed for the key @a strKey to value @a strValue. */
        void sigExtraDataChange(const QUuid &uId, const QString &strKey, const QString &strValue);
    /** @} */

    /** @name Console related signals
      * @{ */
        /** Notifies about mouse pointer become @a fVisible and his shape changed to @a fAlpha, @a hotCorner, @a size and @a shape. */
        void sigMousePointerShapeChange(bool fVisible, bool fAlpha, QPoint hotCorner, QSize size, QVector<uint8_t> shape);
        /** Notifies about mouse capability change to @a fSupportsAbsolute, @a fSupportsRelative, @a fSupportsMultiTouch and @a fNeedsHostCursor. */
        void sigMouseCapabilityChange(bool fSupportsAbsolute, bool fSupportsRelative, bool fSupportsMultiTouch, bool fNeedsHostCursor);
        /** Notifies about guest request to change the cursor position to @a uX * @a uY.
          * @param  fContainsData  Brings whether the @a uX and @a uY values are valid and could be used by the GUI now. */
        void sigCursorPositionChange(bool fContainsData, unsigned long uX, unsigned long uY);
        /** Notifies about keyboard LEDs change for @a fNumLock, @a fCapsLock and @a fScrollLock. */
        void sigKeyboardLedsChangeEvent(bool fNumLock, bool fCapsLock, bool fScrollLock);
        /** Notifies about machine @a state change. */
        void sigStateChange(KMachineState state);
        /** Notifies about guest additions state change. */
        void sigAdditionsChange();
        /** Notifies about network @a adapter state change. */
        void sigNetworkAdapterChange(CNetworkAdapter comAdapter);
        /** Notifies about storage device change for @a attachment, which was @a fRemoved and it was @a fSilent for guest. */
        void sigStorageDeviceChange(CMediumAttachment comAttachment, bool fRemoved, bool fSilent);
        /** Notifies about storage medium @a attachment state change. */
        void sigMediumChange(CMediumAttachment comAttachment);
        /** Notifies about VRDE device state change. */
        void sigVRDEChange();
        /** Notifies about recording state change. */
        void sigRecordingChange();
        /** Notifies about USB controller state change. */
        void sigUSBControllerChange();
        /** Notifies about USB @a device state change to @a fAttached, holding additional @a error information. */
        void sigUSBDeviceStateChange(CUSBDevice comDevice, bool fAttached, CVirtualBoxErrorInfo comError);
        /** Notifies about shared folder state change. */
        void sigSharedFolderChange();
        /** Notifies about CPU execution-cap change. */
        void sigCPUExecutionCapChange();
        /** Notifies about guest-screen configuration change of @a type for @a uScreenId with @a screenGeo. */
        void sigGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo);
        /** Notifies about Runtime error with @a strErrorId which is @a fFatal and have @a strMessage. */
        void sigRuntimeError(bool fFatal, QString strErrorId, QString strMessage);
        /** Notifies about VM window can be shown, allowing to prevent it by @a fVeto with @a strReason. */
        void sigCanShowWindow(bool &fVeto, QString &strReason); /* use Qt::DirectConnection */
        /** Notifies about VM window with specified @a winId should be shown. */
        void sigShowWindow(qint64 &winId); /* use Qt::DirectConnection */
        /** Notifies about audio adapter state change. */
        void sigAudioAdapterChange();
    /** @} */

    /** @name Progress related signals
      * @{ */
        /** Notifies about @a iPercent change for progress with @a uProgressId. */
        void sigProgressPercentageChange(const QUuid &uProgressId, const int iPercent);
        /** Notifies about task complete for progress with @a uProgressId. */
        void sigProgressTaskComplete(const QUuid &uProgressId);
    /** @} */

    /** @name Guest Session related signals
      * @{ */
        /** Notifies about guest session (un)registered event @a is the (un)registed guest session. */
        void sigGuestSessionRegistered(CGuestSession comGuestSession);
        void sigGuestSessionUnregistered(CGuestSession comGuestSession);

        /** Notifies about guest process (un)registered event @a is the (un)registed guest process. */
        void sigGuestProcessRegistered(CGuestProcess comGuestProcess);
        void sigGuestProcessUnregistered(CGuestProcess comGuestProcess);
        void sigGuestSessionStatedChanged(const CGuestSessionStateChangedEvent &comEvent);
        void sigGuestProcessStateChanged(const CGuestProcessStateChangedEvent &comEvent);
    /** @} */

public:

    /** Constructs main event listener. */
    UIMainEventListener();

    /** Initialization routine. */
    HRESULT init(QObject *pParent) { Q_UNUSED(pParent); return S_OK; }
    /** Deinitialization routine. */
    void uninit() {}

    /** Registers event @a source for passive event @a listener. */
    void registerSource(const CEventSource &comSource, const CEventListener &comListener);
    /** Unregisters event sources. */
    void unregisterSources();

    /** Main event handler routine. */
    STDMETHOD(HandleEvent)(VBoxEventType_T enmType, IEvent *pEvent);

    /** Holds the list of threads handling passive event listening. */
    QList<UIMainEventListeningThread*> m_threads;
};

/** Wraps the IListener interface around our implementation class. */
typedef ListenerImpl<UIMainEventListener, QObject*> UIMainEventListenerImpl;

#endif /* !___UIMainEventListener_h___ */

