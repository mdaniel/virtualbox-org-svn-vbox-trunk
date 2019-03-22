/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestSessionTreeItem class implementation.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIGuestControlTreeItem.h"
#include "UIGuestProcessControlWidget.h"
#include "VBoxGlobal.h"

/* COM includes: */
#include "CGuest.h"
#include "CEventSource.h"
#include "CGuestProcessStateChangedEvent.h"
#include "CGuestSessionStateChangedEvent.h"


/*********************************************************************************************************************************
*   UIGuestControlTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/

UIGuestControlTreeItem::UIGuestControlTreeItem(QITreeWidget *pTreeWidget, const QStringList &strings /* = QStringList() */)
    :QITreeWidgetItem(pTreeWidget,strings)
{
}

UIGuestControlTreeItem::UIGuestControlTreeItem(UIGuestControlTreeItem *pTreeWidgetItem,
                                               const QStringList &strings/* = QStringList() */)
    :QITreeWidgetItem(pTreeWidgetItem, strings)
{

}

UIGuestControlTreeItem::~UIGuestControlTreeItem()
{
}

void UIGuestControlTreeItem::prepare()
{
    prepareListener();
    prepareConnections();
    setColumnText();
}

void UIGuestControlTreeItem::prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes)
{
    if (!comEventSource.isOk())
        return;
    /* Create event listener instance: */
    m_pQtListener.createObject();
    m_pQtListener->init(new UIMainEventListener, this);
    m_comEventListener = CEventListener(m_pQtListener);

    /* Register event listener for CProgress event source: */
    comEventSource.RegisterListener(m_comEventListener, eventTypes,
        gEDataManager->eventHandlingType() == EventHandlingType_Active ? TRUE : FALSE);

    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Register event sources in their listeners as well: */
        m_pQtListener->getWrapped()->registerSource(comEventSource, m_comEventListener);
    }
}

void UIGuestControlTreeItem::cleanupListener(CEventSource comEventSource)
{
    if (!comEventSource.isOk())
        return;
    /* If event listener registered as passive one: */
    if (gEDataManager->eventHandlingType() == EventHandlingType_Passive)
    {
        /* Unregister everything: */
        m_pQtListener->getWrapped()->unregisterSources();
    }

    /* Make sure VBoxSVC is available: */
    if (!vboxGlobal().isVBoxSVCAvailable())
        return;

    /* Unregister event listener for CProgress event source: */
    comEventSource.UnregisterListener(m_comEventListener);
}


/*********************************************************************************************************************************
*   UIGuestSessionTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/

UIGuestSessionTreeItem::UIGuestSessionTreeItem(QITreeWidget *pTreeWidget, CGuestSession& guestSession,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidget, strings)
    , m_comGuestSession(guestSession)
{
    prepare();
    initProcessSubTree();
}

UIGuestSessionTreeItem::UIGuestSessionTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestSession& guestSession,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidgetItem, strings)
    , m_comGuestSession(guestSession)
{
    prepare();
    initProcessSubTree();
}

UIGuestSessionTreeItem::~UIGuestSessionTreeItem()
{
    cleanupListener();
}

const CGuestSession& UIGuestSessionTreeItem::guestSession() const
{
    return m_comGuestSession;
}

void UIGuestSessionTreeItem::prepareConnections()
{

    qRegisterMetaType<CGuestProcess>();
    qRegisterMetaType<CGuestSessionStateChangedEvent>();
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestSessionStatedChanged,
            this, &UIGuestSessionTreeItem::sltGuestSessionUpdated);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessRegistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessRegistered);
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessUnregistered,
            this, &UIGuestSessionTreeItem::sltGuestProcessUnregistered);
}

void UIGuestSessionTreeItem::prepareListener()
{
    QVector<KVBoxEventType> eventTypes;
    eventTypes << KVBoxEventType_OnGuestSessionStateChanged
               << KVBoxEventType_OnGuestProcessRegistered;

    UIGuestControlTreeItem::prepareListener(m_comGuestSession.GetEventSource(), eventTypes);
}

void UIGuestSessionTreeItem::cleanupListener()
{
    UIGuestControlTreeItem::cleanupListener(m_comGuestSession.GetEventSource());
}

void UIGuestSessionTreeItem::initProcessSubTree()
{
    if (!m_comGuestSession.isOk())
        return;
    QVector<CGuestProcess> processes = m_comGuestSession.GetProcesses();
    printf("processes # %d\n", processes.size());
    for (int  i =0; i < processes.size(); ++i)
        addGuestProcess(processes[i]);
}

void UIGuestSessionTreeItem::sltGuestSessionUpdated(const CGuestSessionStateChangedEvent& cEvent)
{
    if (cEvent.isOk() && m_comGuestSession.isOk() && m_comGuestSession.GetStatus() == KGuestSessionStatus_Error)
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk() && cErrorInfo.GetResultCode() != S_OK)
        {
            emit sigGuestSessionErrorText(cErrorInfo.GetText());
        }
    }
    setColumnText();
    emit sigGuessSessionUpdated();
}

void UIGuestSessionTreeItem::sltGuestProcessRegistered(CGuestProcess guestProcess)
{
    const ULONG waitTimeout = 2000;
    KProcessWaitResult waitResult = guestProcess.WaitFor(KProcessWaitForFlag_Start, waitTimeout);
    if (waitResult != KProcessWaitResult_Start)
    {
        return ;
    }

    if (!guestProcess.isOk())
        return;
    addGuestProcess(guestProcess);
}

void UIGuestSessionTreeItem::addGuestProcess(CGuestProcess guestProcess)
{
    /* Dont add the tree items for already terminated or currently being terminated
       guest processes: */
    KProcessStatus processStatus = guestProcess.GetStatus();
    if (processStatus != KProcessStatus_Starting &&
        processStatus != KProcessStatus_Started &&
        processStatus != KProcessStatus_Paused)
        return;

    UIGuestProcessTreeItem *newItem = new UIGuestProcessTreeItem(this, guestProcess);
    connect(newItem, &UIGuestProcessTreeItem::sigGuestProcessErrorText,
            this, &UIGuestSessionTreeItem::sigGuestSessionErrorText);
    setExpanded(true);
}

void UIGuestSessionTreeItem::errorString(QString strError)
{
    emit sigGuestSessionErrorText(strError);
}

KGuestSessionStatus UIGuestSessionTreeItem::status() const
{
    if (!m_comGuestSession.isOk())
        return KGuestSessionStatus_Undefined;
    return m_comGuestSession.GetStatus();
}

QString UIGuestSessionTreeItem::propertyString() const
{
    QString strProperty;
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Session Name")).arg(m_comGuestSession.GetName());
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Session Id")).arg(m_comGuestSession.GetId());
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Session Status")).arg(gpConverter->toInternalString(m_comGuestSession.GetStatus()));
    return strProperty;
}

void UIGuestSessionTreeItem::sltGuestProcessUnregistered(CGuestProcess guestProcess)
{
    if (!UIGuestProcessControlWidget::m_fDeleteAfterUnregister)
        return;
    for (int i = 0; i < childCount(); ++i)
    {
        UIGuestProcessTreeItem* item = dynamic_cast<UIGuestProcessTreeItem*>(child(i));
        if (item && item->guestProcess() == guestProcess)
        {
            delete item;
            break;
        }
    }
}

void UIGuestSessionTreeItem::setColumnText()
{
    if (!m_comGuestSession.isOk())
        return;
    setText(0, QString("%1").arg(m_comGuestSession.GetId()));
    setText(1, QString("%1").arg(m_comGuestSession.GetName()));
    setText(2, QString("%1").arg(gpConverter->toInternalString(m_comGuestSession.GetStatus())));
}


/*********************************************************************************************************************************
*   UIGuestProcessTreeItem implementation.                                                                                       *
*********************************************************************************************************************************/
UIGuestProcessTreeItem::UIGuestProcessTreeItem(QITreeWidget *pTreeWidget, CGuestProcess& guestProcess,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidget, strings)
    , m_comGuestProcess(guestProcess)
{
    prepare();
}

UIGuestProcessTreeItem::UIGuestProcessTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestProcess& guestProcess,
                                               const QStringList &strings /* = QStringList() */)
    :UIGuestControlTreeItem(pTreeWidgetItem, strings)
    , m_comGuestProcess(guestProcess)
{
    prepare();
}

void UIGuestProcessTreeItem::prepareConnections()
{
    qRegisterMetaType<CGuestProcessStateChangedEvent>();
    connect(m_pQtListener->getWrapped(), &UIMainEventListener::sigGuestProcessStateChanged,
            this, &UIGuestProcessTreeItem::sltGuestProcessUpdated);
}

UIGuestProcessTreeItem::~UIGuestProcessTreeItem()
{
    cleanupListener();
}

KProcessStatus UIGuestProcessTreeItem::status() const
{
    if (!m_comGuestProcess.isOk())
        return KProcessStatus_Undefined;
    return m_comGuestProcess.GetStatus();
}

QString UIGuestProcessTreeItem::propertyString() const
{
    QString strProperty;
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Process Name")).arg(m_comGuestProcess.GetName());
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Process Id")).arg(m_comGuestProcess.GetPID());
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Process Status")).arg(gpConverter->toInternalString(m_comGuestProcess.GetStatus()));
    strProperty += QString("<b>%1: </b>%2<br/>").arg(tr("Executable Path")).arg(m_comGuestProcess.GetExecutablePath());
    return strProperty;
}

void UIGuestProcessTreeItem::prepareListener()
{
    QVector<KVBoxEventType> eventTypes;
    eventTypes  << KVBoxEventType_OnGuestProcessStateChanged
                << KVBoxEventType_OnGuestProcessInputNotify
                << KVBoxEventType_OnGuestProcessOutput;
    UIGuestControlTreeItem::prepareListener(m_comGuestProcess.GetEventSource(), eventTypes);
}

void UIGuestProcessTreeItem::cleanupListener()
{
    UIGuestControlTreeItem::cleanupListener(m_comGuestProcess.GetEventSource());
}

void UIGuestProcessTreeItem::sltGuestProcessUpdated(const CGuestProcessStateChangedEvent &cEvent)
{
    if (cEvent.isOk() && m_comGuestProcess.isOk() && m_comGuestProcess.GetStatus() == KProcessStatus_Error)
    {
        CVirtualBoxErrorInfo cErrorInfo = cEvent.GetError();
        if (cErrorInfo.isOk() && cErrorInfo.GetResultCode() != S_OK)
            emit sigGuestProcessErrorText(cErrorInfo.GetText());

    }
    setColumnText();
    KProcessStatus processStatus = m_comGuestProcess.GetStatus();
    if (processStatus != KProcessStatus_Starting &&
        processStatus !=  KProcessStatus_Started &&
        processStatus !=  KProcessStatus_Paused)
    {
        if (UIGuestProcessControlWidget::m_fDeleteAfterUnregister)
            this->deleteLater();
    }
}

 void UIGuestProcessTreeItem::setColumnText()
{
    if (!m_comGuestProcess.isOk())
        return;
    setText(0, QString("%1").arg(m_comGuestProcess.GetPID()));
    setText(1, QString("%1").arg(m_comGuestProcess.GetExecutablePath()));
    setText(2, QString("%1").arg(gpConverter->toInternalString(m_comGuestProcess.GetStatus())));
}

const CGuestProcess& UIGuestProcessTreeItem::guestProcess() const
{
    return m_comGuestProcess;
}
