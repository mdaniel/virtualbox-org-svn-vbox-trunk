/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlTreeItem class declaration.
 */

/*
 * Copyright (C) 2016-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlTreeItem_h___
#define ___UIGuestControlTreeItem_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QITreeWidget.h"
#include "UIMainEventListener.h"

/* COM includes: */
#include "CEventListener.h"
#include "CGuestSession.h"

/* Forward declarations: */
class CEventSource;
class CGuestProcessStateChangedEvent;
class CGuestSessionStateChangedEvent;

/** QITreeWidgetItem extension serving as a base class
    to UIGuestSessionTreeItem and UIGuestProcessTreeItem classes */
class UIGuestControlTreeItem : public QITreeWidgetItem
{

    Q_OBJECT;

public:

    UIGuestControlTreeItem(QITreeWidget *pTreeWidget, const QStringList &strings = QStringList());
    UIGuestControlTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, const QStringList &strings = QStringList());
    virtual ~UIGuestControlTreeItem();

private slots:

protected:

    void prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes);
    void cleanupListener(CEventSource comEventSource);
    void prepare();

    ComObjPtr<UIMainEventListenerImpl> m_pQtListener;

private:

    virtual void prepareListener() = 0;
    virtual void prepareConnections() = 0;
    virtual void cleanupListener() = 0;
    virtual void setColumnText() = 0;

    /** Holds the COM event listener instance. */
    CEventListener m_comEventListener;

};

/** UIGuestControlTreeItem extension. Represents a instance of CGuestSession
    and acts as an event listener for this com object. */
class UIGuestSessionTreeItem : public UIGuestControlTreeItem
{
    Q_OBJECT;

signals:

    void sigGuessSessionUpdated();
    void sigGuestSessionErrorText(QString strError);

public:

    UIGuestSessionTreeItem(QITreeWidget *pTreeWidget, CGuestSession& guestSession, const QStringList &strings = QStringList());
    UIGuestSessionTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestSession& guestSession, const QStringList &strings = QStringList());
    virtual ~UIGuestSessionTreeItem();
    const CGuestSession& guestSession() const;
    void errorString(QString strError);

protected:

    void prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes);
    void cleanupListener(CEventSource comEventSource);

private slots:

    void sltGuestSessionUpdated(const CGuestSessionStateChangedEvent& cEvent);
    void sltGuestProcessRegistered(CGuestProcess guestProcess);
    void sltGuestProcessUnregistered(CGuestProcess guestProcess);


private:

    virtual void prepareListener() /* override */;
    virtual void prepareConnections() /* override */;
    virtual void cleanupListener()  /* override */;
    virtual void setColumnText()  /* override */;
    void addGuestProcess(CGuestProcess guestProcess);
    void initProcessSubTree();
    CGuestSession m_comGuestSession;

};

/** UIGuestControlTreeItem extension. Represents a instance of CGuestProcess
    and acts as an event listener for this com object. */
class UIGuestProcessTreeItem : public UIGuestControlTreeItem
{
    Q_OBJECT;

signals:

    void sigGuestProcessErrorText(QString strError);

public:

    UIGuestProcessTreeItem(QITreeWidget *pTreeWidget, CGuestProcess& guestProcess, const QStringList &strings = QStringList());
    UIGuestProcessTreeItem(UIGuestControlTreeItem *pTreeWidgetItem, CGuestProcess& guestProcess, const QStringList &strings = QStringList());
    const CGuestProcess& guestProcess() const;
    virtual ~UIGuestProcessTreeItem();


protected:

    void prepareListener(CEventSource comEventSource, QVector<KVBoxEventType>& eventTypes);
    void cleanupListener(CEventSource comEventSource);

private slots:

    void sltGuestProcessUpdated(const CGuestProcessStateChangedEvent &cEvent);

private:

    virtual void prepareListener() /* override */;
    virtual void prepareConnections() /* override */;
    virtual void cleanupListener()  /* override */;
    virtual void setColumnText()  /* override */;

    CGuestProcess m_comGuestProcess;
};

#endif /* !___UIGuestControlTreeItem_h___ */

