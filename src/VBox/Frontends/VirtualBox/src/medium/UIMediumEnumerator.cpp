/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumEnumerator class implementation.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QSet>

/* GUI includes: */
# include "UIMediumEnumerator.h"
# include "UIThreadPool.h"
# include "UIVirtualBoxEventHandler.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "COMEnums.h"
# include "CMachine.h"
# include "CSnapshot.h"
# include "CMediumAttachment.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** UITask extension used for medium enumeration purposes. */
class UITaskMediumEnumeration : public UITask
{
    Q_OBJECT;

public:

    /** Constructs @a medium enumeration task. */
    UITaskMediumEnumeration(const UIMedium &medium)
        : UITask(UITask::Type_MediumEnumeration)
    {
        /* Store medium as property: */
        setProperty("medium", QVariant::fromValue(medium));
    }

private:

    /** Contains medium enumeration task body. */
    void run()
    {
        /* Get medium: */
        UIMedium medium = property("medium").value<UIMedium>();
        /* Enumerate it: */
        medium.blockAndQueryState();
        /* Put it back: */
        setProperty("medium", QVariant::fromValue(medium));
    }
};


UIMediumEnumerator::UIMediumEnumerator()
    : m_fMediumEnumerationInProgress(false)
{
    /* Allow UIMedium to be used in inter-thread signals: */
    qRegisterMetaType<UIMedium>();

    /* Prepare Main event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineDataChange, this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotTake,      this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotDelete,    this, &UIMediumEnumerator::sltHandleSnapshotDeleted);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotChange,    this, &UIMediumEnumerator::sltHandleMachineUpdate);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigSnapshotRestore,   this, &UIMediumEnumerator::sltHandleSnapshotDeleted);
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered, this, &UIMediumEnumerator::sltHandleMachineRegistration);

    /* Listen for global thread-pool: */
    connect(vboxGlobal().threadPool(), &UIThreadPool::sigTaskComplete, this, &UIMediumEnumerator::sltHandleMediumEnumerationTaskComplete);
}

QList<QString> UIMediumEnumerator::mediumIDs() const
{
    /* Return keys of current medium-map: */
    return m_media.keys();
}

UIMedium UIMediumEnumerator::medium(const QString &strMediumID)
{
    /* Search through current medium-map for the medium with passed ID: */
    if (m_media.contains(strMediumID))
        return m_media[strMediumID];
    /* Return NULL medium otherwise: */
    return UIMedium();
}

void UIMediumEnumerator::createMedium(const UIMedium &medium)
{
    /* Get medium ID: */
    const QString strMediumID = medium.id();

    /* Do not create UIMedium(s) with incorrect ID: */
    AssertReturnVoid(!strMediumID.isNull());
    AssertReturnVoid(strMediumID != UIMedium::nullID());
    /* Make sure medium doesn't exists already: */
    AssertReturnVoid(!m_media.contains(strMediumID));

    /* Insert medium: */
    m_media[strMediumID] = medium;
    LogRel(("GUI: UIMediumEnumerator: Medium with key={%s} created\n", strMediumID.toUtf8().constData()));

    /* Notify listener: */
    emit sigMediumCreated(strMediumID);
}

void UIMediumEnumerator::deleteMedium(const QString &strMediumID)
{
    /* Do not delete UIMedium(s) with incorrect ID: */
    AssertReturnVoid(!strMediumID.isNull());
    AssertReturnVoid(strMediumID != UIMedium::nullID());
    /* Make sure medium still exists: */
    AssertReturnVoid(m_media.contains(strMediumID));

    /* Remove medium: */
    m_media.remove(strMediumID);
    LogRel(("GUI: UIMediumEnumerator: Medium with key={%s} deleted\n", strMediumID.toUtf8().constData()));

    /* Notify listener: */
    emit sigMediumDeleted(strMediumID);
}

void UIMediumEnumerator::enumerateMedia()
{
    /* Make sure we are not already in progress: */
    AssertReturnVoid(!m_fMediumEnumerationInProgress);

    /* Compose new map of all currently known media & their children.
     * While composing we are using data from already existing media. */
    UIMediumMap media;
    addNullMediumToMap(media);
    addMediaToMap(vboxGlobal().virtualBox().GetHardDisks(), media);
    addMediaToMap(vboxGlobal().host().GetDVDDrives(), media);
    addMediaToMap(vboxGlobal().virtualBox().GetDVDImages(), media);
    addMediaToMap(vboxGlobal().host().GetFloppyDrives(), media);
    addMediaToMap(vboxGlobal().virtualBox().GetFloppyImages(), media);
    if (VBoxGlobal::isCleaningUp())
        return; /* VBoxGlobal is cleaning up, abort immediately. */
    m_media = media;

    /* Notify listener: */
    LogRel(("GUI: UIMediumEnumerator: Medium-enumeration started...\n"));
    m_fMediumEnumerationInProgress = true;
    emit sigMediumEnumerationStarted();

    /* Make sure we really have more than one medium (which is Null): */
    if (m_media.size() == 1)
    {
        /* Notify listener: */
        LogRel(("GUI: UIMediumEnumerator: Medium-enumeration finished!\n"));
        m_fMediumEnumerationInProgress = false;
        emit sigMediumEnumerationFinished();
    }

    /* Start enumeration for UIMedium(s) with correct ID: */
    foreach (const QString &strMediumID, m_media.keys())
        if (!strMediumID.isNull() && strMediumID != UIMedium::nullID())
            createMediumEnumerationTask(m_media[strMediumID]);
}

void UIMediumEnumerator::refreshMedia()
{
    /* Make sure we are not already in progress: */
    AssertReturnVoid(!m_fMediumEnumerationInProgress);

    /* Refresh all known media we have: */
    foreach (const QString &strMediumID, m_media.keys())
        m_media[strMediumID].refresh();
}

void UIMediumEnumerator::sltHandleMachineUpdate(QString strMachineID)
{
    LogRel2(("GUI: UIMediumEnumerator: Machine (or snapshot) event received, ID = %s\n",
             strMachineID.toUtf8().constData()));

    /* Gather previously used UIMedium IDs: */
    QStringList previousUIMediumIDs;
    calculateCachedUsage(strMachineID, previousUIMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
             previousUIMediumIDs.isEmpty() ? "<empty>" : previousUIMediumIDs.join(", ").toUtf8().constData()));

    /* Gather currently used CMediums and their IDs: */
    CMediumMap currentCMediums;
    QStringList currentCMediumIDs;
    calculateActualUsage(strMachineID, currentCMediums, currentCMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
             currentCMediumIDs.isEmpty() ? "<empty>" : currentCMediumIDs.join(", ").toUtf8().constData()));

    /* Determine excluded media: */
    const QSet<QString> previousSet = previousUIMediumIDs.toSet();
    const QSet<QString> currentSet = currentCMediumIDs.toSet();
    const QSet<QString> excludedSet = previousSet - currentSet;
    const QStringList excludedUIMediumIDs = excludedSet.toList();
    if (!excludedUIMediumIDs.isEmpty())
        LogRel2(("GUI: UIMediumEnumerator:  Items excluded from usage: %s\n", excludedUIMediumIDs.join(", ").toUtf8().constData()));
    if (!currentCMediumIDs.isEmpty())
        LogRel2(("GUI: UIMediumEnumerator:  Items currently in usage: %s\n", currentCMediumIDs.join(", ").toUtf8().constData()));

    /* Update cache for excluded UIMediums: */
    recacheFromCachedUsage(excludedUIMediumIDs);

    /* Update cache for current CMediums: */
    recacheFromActualUsage(currentCMediums, currentCMediumIDs);

    LogRel2(("GUI: UIMediumEnumerator: Machine (or snapshot) event processed, ID = %s\n",
             strMachineID.toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleMachineRegistration(QString strMachineID, bool fRegistered)
{
    LogRel2(("GUI: UIMediumEnumerator: Machine %s event received, ID = %s\n",
             fRegistered ? "registration" : "unregistration",
             strMachineID.toUtf8().constData()));

    /* Machine was registered: */
    if (fRegistered)
    {
        /* Gather currently used CMediums and their IDs: */
        CMediumMap currentCMediums;
        QStringList currentCMediumIDs;
        calculateActualUsage(strMachineID, currentCMediums, currentCMediumIDs, false /* take into account current state only */);
        LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
                 currentCMediumIDs.isEmpty() ? "<empty>" : currentCMediumIDs.join(", ").toUtf8().constData()));
        /* Update cache with currently used CMediums: */
        recacheFromActualUsage(currentCMediums, currentCMediumIDs);
    }
    /* Machine was unregistered: */
    else
    {
        /* Gather previously used UIMedium IDs: */
        QStringList previousUIMediumIDs;
        calculateCachedUsage(strMachineID, previousUIMediumIDs, false /* take into account current state only */);
        LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
                 previousUIMediumIDs.isEmpty() ? "<empty>" : previousUIMediumIDs.join(", ").toUtf8().constData()));
        /* Update cache for previously used UIMediums: */
        recacheFromCachedUsage(previousUIMediumIDs);
    }

    LogRel2(("GUI: UIMediumEnumerator: Machine %s event processed, ID = %s\n",
             fRegistered ? "registration" : "unregistration",
             strMachineID.toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleSnapshotDeleted(QString strMachineID, QString strSnapshotID)
{
    LogRel2(("GUI: UIMediumEnumerator: Snapshot-deleted event received, Machine ID = {%s}, Snapshot ID = {%s}\n",
             strMachineID.toUtf8().constData(), strSnapshotID.toUtf8().constData()));

    /* Gather previously used UIMedium IDs: */
    QStringList previousUIMediumIDs;
    calculateCachedUsage(strMachineID, previousUIMediumIDs, false /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  Old usage: %s\n",
             previousUIMediumIDs.isEmpty() ? "<empty>" : previousUIMediumIDs.join(", ").toUtf8().constData()));

    /* Gather currently used CMediums and their IDs: */
    CMediumMap currentCMediums;
    QStringList currentCMediumIDs;
    calculateActualUsage(strMachineID, currentCMediums, currentCMediumIDs, true /* take into account current state only */);
    LogRel2(("GUI: UIMediumEnumerator:  New usage: %s\n",
             currentCMediumIDs.isEmpty() ? "<empty>" : currentCMediumIDs.join(", ").toUtf8().constData()));

    /* Update everything: */
    recacheFromCachedUsage(previousUIMediumIDs);
    recacheFromActualUsage(currentCMediums, currentCMediumIDs);

    LogRel2(("GUI: UIMediumEnumerator: Snapshot-deleted event processed, Machine ID = {%s}, Snapshot ID = {%s}\n",
             strMachineID.toUtf8().constData(), strSnapshotID.toUtf8().constData()));
}

void UIMediumEnumerator::sltHandleMediumEnumerationTaskComplete(UITask *pTask)
{
    /* Make sure that is one of our tasks: */
    if (pTask->type() != UITask::Type_MediumEnumeration)
        return;
    AssertReturnVoid(m_tasks.contains(pTask));

    /* Get enumerated UIMedium: */
    const UIMedium uimedium = pTask->property("medium").value<UIMedium>();
    const QString strUIMediumKey = uimedium.key();
    LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} enumerated\n", strUIMediumKey.toUtf8().constData()));

    /* Remove task from internal set: */
    m_tasks.remove(pTask);

    /* Make sure such UIMedium still exists: */
    if (!m_media.contains(strUIMediumKey))
    {
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} already deleted by a third party\n", strUIMediumKey.toUtf8().constData()));
        return;
    }

    /* Check if UIMedium ID was changed: */
    const QString strUIMediumID = uimedium.id();
    /* UIMedium ID was changed to nullID: */
    if (strUIMediumID == UIMedium::nullID())
    {
        /* Delete this medium: */
        m_media.remove(strUIMediumKey);
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} closed and deleted (after enumeration)\n", strUIMediumKey.toUtf8().constData()));

        /* And notify listener about delete: */
        emit sigMediumDeleted(strUIMediumKey);
    }
    /* UIMedium ID was changed to something proper: */
    else if (strUIMediumID != strUIMediumKey)
    {
        /* We have to reinject enumerated medium: */
        m_media.remove(strUIMediumKey);
        m_media[strUIMediumID] = uimedium;
        m_media[strUIMediumID].setKey(strUIMediumID);
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} has it changed to {%s}\n", strUIMediumKey.toUtf8().constData(),
                                                                                           strUIMediumID.toUtf8().constData()));

        /* And notify listener about delete/create: */
        emit sigMediumDeleted(strUIMediumKey);
        emit sigMediumCreated(strUIMediumID);
    }
    /* UIMedium ID was not changed: */
    else
    {
        /* Just update enumerated medium: */
        m_media[strUIMediumID] = uimedium;
        LogRel2(("GUI: UIMediumEnumerator: Medium with key={%s} updated\n", strUIMediumID.toUtf8().constData()));

        /* And notify listener about update: */
        emit sigMediumEnumerated(strUIMediumID);
    }

    /* If there are no more tasks we know about: */
    if (m_tasks.isEmpty())
    {
        /* Notify listener: */
        LogRel(("GUI: UIMediumEnumerator: Medium-enumeration finished!\n"));
        m_fMediumEnumerationInProgress = false;
        emit sigMediumEnumerationFinished();
    }
}

void UIMediumEnumerator::retranslateUi()
{
    /* Translating NULL uimedium by recreating it: */
    if (m_media.contains(UIMedium::nullID()))
        m_media[UIMedium::nullID()] = UIMedium();
}

void UIMediumEnumerator::createMediumEnumerationTask(const UIMedium &medium)
{
    /* Prepare medium-enumeration task: */
    UITask *pTask = new UITaskMediumEnumeration(medium);
    /* Append to internal set: */
    m_tasks << pTask;
    /* Post into global thread-pool: */
    vboxGlobal().threadPool()->enqueueTask(pTask);
}

void UIMediumEnumerator::addNullMediumToMap(UIMediumMap &media)
{
    /* Insert NULL uimedium to the passed uimedium map.
     * Get existing one from the previous map if any. */
    QString strNullMediumID = UIMedium::nullID();
    UIMedium uimedium = m_media.contains(strNullMediumID) ? m_media[strNullMediumID] : UIMedium();
    media.insert(strNullMediumID, uimedium);
}

void UIMediumEnumerator::addMediaToMap(const CMediumVector &inputMedia, UIMediumMap &outputMedia)
{
    /* Insert hard-disks to the passed uimedium map.
     * Get existing one from the previous map if any. */
    foreach (CMedium medium, inputMedia)
    {
        /* If VBoxGlobal is cleaning up, abort immediately: */
        if (VBoxGlobal::isCleaningUp())
            break;

        /* Prepare uimedium on the basis of current medium: */
        QString strMediumID = medium.GetId();
        UIMedium uimedium = m_media.contains(strMediumID) ? m_media[strMediumID] :
            UIMedium(medium, UIMediumDefs::mediumTypeToLocal(medium.GetDeviceType()));

        /* Insert uimedium into map: */
        outputMedia.insert(uimedium.id(), uimedium);

        /* Insert medium children into map too: */
        addMediaToMap(medium.GetChildren(), outputMedia);
    }
}

/**
 * Calculates last known UIMedium <i>usage</i> based on cached data.
 * @param strMachineID describes the machine we are calculating <i>usage</i> for.
 * @param previousUIMediumIDs receives UIMedium IDs used in cached data.
 * @param fTakeIntoAccountCurrentStateOnly defines whether we should take into accound current VM state only.
 */
void UIMediumEnumerator::calculateCachedUsage(const QString &strMachineID, QStringList &previousUIMediumIDs, bool fTakeIntoAccountCurrentStateOnly) const
{
    /* For each the UIMedium ID cache have: */
    foreach (const QString &strMediumID, mediumIDs())
    {
        /* Get corresponding UIMedium: */
        const UIMedium &uimedium = m_media[strMediumID];
        /* Get the list of the machines this UIMedium attached to.
         * Take into account current-state only if necessary. */
        const QList<QString> &machineIDs = fTakeIntoAccountCurrentStateOnly ?
                                           uimedium.curStateMachineIds() : uimedium.machineIds();
        /* Add this UIMedium ID to previous usage if necessary: */
        if (machineIDs.contains(strMachineID))
            previousUIMediumIDs << strMediumID;
    }
}

/**
 * Calculates new CMedium <i>usage</i> based on actual data.
 * @param strMachineID describes the machine we are calculating <i>usage</i> for.
 * @param currentCMediums receives CMedium used in actual data.
 * @param currentCMediumIDs receives CMedium IDs used in actual data.
 * @param fTakeIntoAccountCurrentStateOnly defines whether we should take into accound current VM state only.
 */
void UIMediumEnumerator::calculateActualUsage(const QString &strMachineID, CMediumMap &currentCMediums, QStringList &currentCMediumIDs, bool fTakeIntoAccountCurrentStateOnly) const
{
    /* Search for corresponding machine: */
    CMachine machine = vboxGlobal().virtualBox().FindMachine(strMachineID);
    if (machine.isNull())
    {
        /* Usually means the machine is already gone, not harmful. */
        return;
    }

    /* Calculate actual usage starting from root-snapshot if necessary: */
    if (!fTakeIntoAccountCurrentStateOnly)
        calculateActualUsage(machine.FindSnapshot(QString()), currentCMediums, currentCMediumIDs);
    /* Calculate actual usage for current machine state: */
    calculateActualUsage(machine, currentCMediums, currentCMediumIDs);
}

/**
 * Calculates new CMedium <i>usage</i> based on actual data.
 * @param snapshot is reference we are calculating <i>usage</i> for.
 * @param currentCMediums receives CMedium used in actual data.
 * @param currentCMediumIDs receives CMedium IDs used in actual data.
 */
void UIMediumEnumerator::calculateActualUsage(const CSnapshot &snapshot, CMediumMap &currentCMediums, QStringList &currentCMediumIDs) const
{
    /* Check passed snapshot: */
    if (snapshot.isNull())
        return;

    /* Calculate actual usage for passed snapshot machine: */
    calculateActualUsage(snapshot.GetMachine(), currentCMediums, currentCMediumIDs);

    /* Iterate through passed snapshot children: */
    foreach (const CSnapshot &childSnapshot, snapshot.GetChildren())
        calculateActualUsage(childSnapshot, currentCMediums, currentCMediumIDs);
}

/**
 * Calculates new CMedium <i>usage</i> based on actual data.
 * @param machine is reference we are calculating <i>usage</i> for.
 * @param currentCMediums receives CMedium used in actual data.
 * @param currentCMediumIDs receives CMedium IDs used in actual data.
 */
void UIMediumEnumerator::calculateActualUsage(const CMachine &machine, CMediumMap &currentCMediums, QStringList &currentCMediumIDs) const
{
    /* Check passed machine: */
    AssertReturnVoid(!machine.isNull());

    /* For each the attachment machine have: */
    foreach (const CMediumAttachment &attachment, machine.GetMediumAttachments())
    {
        /* Get corresponding CMedium: */
        CMedium cmedium = attachment.GetMedium();
        if (!cmedium.isNull())
        {
            /* Make sure that CMedium was not yet closed: */
            const QString strCMediumID = cmedium.GetId();
            if (cmedium.isOk() && !strCMediumID.isNull())
            {
                /* Add this CMedium to current usage: */
                currentCMediums.insert(strCMediumID, cmedium);
                currentCMediumIDs << strCMediumID;
            }
        }
    }
}

/**
 * Updates cache using known changes in cached data.
 * @param previousUIMediumIDs reflects UIMedium IDs used in cached data.
 */
void UIMediumEnumerator::recacheFromCachedUsage(const QStringList &previousUIMediumIDs)
{
    /* For each of previously used UIMedium ID: */
    foreach (const QString &strMediumID, previousUIMediumIDs)
    {
        /* Make sure this ID still in our map: */
        if (m_media.contains(strMediumID))
        {
            /* Get corresponding UIMedium: */
            UIMedium &uimedium = m_media[strMediumID];

            /* If corresponding CMedium still exists: */
            CMedium cmedium = uimedium.medium();
            if (!cmedium.GetId().isNull() && cmedium.isOk())
            {
                /* Refresh UIMedium parent first of all: */
                uimedium.updateParentID();
                /* Enumerate corresponding UIMedium: */
                createMediumEnumerationTask(uimedium);
            }
            /* If corresponding CMedium was closed already: */
            else
            {
                /* Uncache corresponding UIMedium: */
                m_media.remove(strMediumID);
                LogRel2(("GUI: UIMediumEnumerator:  Medium with key={%s} uncached\n", strMediumID.toUtf8().constData()));

                /* And notify listeners: */
                emit sigMediumDeleted(strMediumID);
            }
        }
    }
}

/**
 * Updates cache using known changes in actual data.
 * @param currentCMediums reflects CMedium used in actual data.
 * @param currentCMediumIDs reflects CMedium IDs used in actual data.
 */
void UIMediumEnumerator::recacheFromActualUsage(const CMediumMap &currentCMediums, const QStringList &currentCMediumIDs)
{
    /* For each of currently used CMedium ID: */
    foreach (const QString &strCMediumID, currentCMediumIDs)
    {
        /* If that ID is not in our map: */
        if (!m_media.contains(strCMediumID))
        {
            /* Create new UIMedium: */
            const CMedium &cmedium = currentCMediums[strCMediumID];
            UIMedium uimedium(cmedium, UIMediumDefs::mediumTypeToLocal(cmedium.GetDeviceType()));
            QString strUIMediumKey = uimedium.key();

            /* Cache created UIMedium: */
            m_media.insert(strUIMediumKey, uimedium);
            LogRel2(("GUI: UIMediumEnumerator:  Medium with key={%s} cached\n", strUIMediumKey.toUtf8().constData()));

            /* And notify listeners: */
            emit sigMediumCreated(strUIMediumKey);
        }

        /* Enumerate corresponding UIMedium: */
        createMediumEnumerationTask(m_media[strCMediumID]);
    }
}


#include "UIMediumEnumerator.moc"
