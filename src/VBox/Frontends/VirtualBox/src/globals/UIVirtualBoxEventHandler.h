/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxEventHandler class declaration.
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

#ifndef ___UIVirtualBoxEventHandler_h___
#define ___UIVirtualBoxEventHandler_h___

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class UIVirtualBoxEventHandlerProxy;

/** Singleton QObject extension
  * providing GUI with the CVirtualBoxClient and CVirtualBox event-sources. */
class SHARED_LIBRARY_STUFF UIVirtualBoxEventHandler : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about the VBoxSVC become @a fAvailable. */
    void sigVBoxSVCAvailabilityChange(bool fAvailable);

    /** Notifies about @a state change event for the machine with @a strId. */
    void sigMachineStateChange(const QUuid &aId, const KMachineState state);
    /** Notifies about data change event for the machine with @a strId. */
    void sigMachineDataChange(const QUuid &aId);
    /** Notifies about machine with @a strId was @a fRegistered. */
    void sigMachineRegistered(const QUuid &aId, const bool fRegistered);
    /** Notifies about @a state change event for the session of the machine with @a strId. */
    void sigSessionStateChange(const QUuid &aId, const KSessionState state);
    /** Notifies about snapshot with @a strSnapshotId was taken for the machine with @a strId. */
    void sigSnapshotTake(const QUuid &sId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was deleted for the machine with @a strId. */
    void sigSnapshotDelete(const QUuid &aId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was changed for the machine with @a strId. */
    void sigSnapshotChange(const QUuid &aId, const QUuid &aSnapshotId);
    /** Notifies about snapshot with @a strSnapshotId was restored for the machine with @a strId. */
    void sigSnapshotRestore(const QUuid &aId, const QUuid &aSnapshotId);

public:

    /** Returns singleton instance. */
    static UIVirtualBoxEventHandler *instance();
    /** Destroys singleton instance. */
    static void destroy();

protected:

    /** Constructs VirtualBox event handler. */
    UIVirtualBoxEventHandler();

    /** Prepares all. */
    void prepare();
    /** Prepares connections. */
    void prepareConnections();

private:

    /** Holds the singleton instance. */
    static UIVirtualBoxEventHandler *s_pInstance;

    /** Holds the VirtualBox event proxy instance. */
    UIVirtualBoxEventHandlerProxy *m_pProxy;
};

/** Singleton VirtualBox Event Handler 'official' name. */
#define gVBoxEvents UIVirtualBoxEventHandler::instance()

#endif /* !___UIVirtualBoxEventHandler_h___ */

