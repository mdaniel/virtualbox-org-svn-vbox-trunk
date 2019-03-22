/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupStack class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIPopupStack_h___
#define ___UIPopupStack_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>
#include <QMap>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIPopupCenter.h"

/* Forward declaration: */
class QEvent;
class QObject;
class QScrollArea;
class QShowEvent;
class QSize;
class QString;
class QVBoxLayout;
class UIPopupStackViewport;

/** QWidget extension providing GUI with popup-stack prototype class. */
class SHARED_LIBRARY_STUFF UIPopupStack : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about popup-stack viewport size change. */
    void sigProposeStackViewportSize(QSize newSize);

    /** Asks to close popup-pane with @a strID and @a iResultCode. */
    void sigPopupPaneDone(QString strID, int iResultCode);

    /** Asks to close popup-stack with @a strID. */
    void sigRemove(QString strID);

public:

    /** Constructs popup-stack with passed @a strID and @a enmOrientation. */
    UIPopupStack(const QString &strID, UIPopupStackOrientation enmOrientation);

    /** Returns whether pane with passed @a strID exists. */
    bool exists(const QString &strID) const;
    /** Creates pane with passed @a strID, @a strMessage, @a strDetails and @a buttonDescriptions. */
    void createPopupPane(const QString &strID,
                         const QString &strMessage, const QString &strDetails,
                         const QMap<int, QString> &buttonDescriptions);
    /** Updates pane with passed @a strID, @a strMessage and @a strDetails. */
    void updatePopupPane(const QString &strID,
                         const QString &strMessage, const QString &strDetails);
    /** Recalls pane with passed @a strID. */
    void recallPopupPane(const QString &strID);
    /** Defines stack @a enmOrientation. */
    void setOrientation(UIPopupStackOrientation enmOrientation);

    /** Defines stack @a pParent*/
    void setParent(QWidget *pParent);
    /** Defines stack @a pParent and @a enmFlags. */
    void setParent(QWidget *pParent, Qt::WindowFlags enmFlags);

protected:

    /** Pre-handles standard Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;

private slots:

    /** Adjusts geometry. */
    void sltAdjustGeometry();

    /** Handles removal of the popup-pane with @a strID. */
    void sltPopupPaneRemoved(QString strID);
    /** Handles removal of all the popup-panes. */
    void sltPopupPanesRemoved();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares contents. */
    void prepareContent();

    /** Propagates size. */
    void propagateSize();

    /** Returns @a pParent menu-bar height. */
    static int parentMenuBarHeight(QWidget *pParent);
    /** Returns @a pParent status-bar height. */
    static int parentStatusBarHeight(QWidget *pParent);

    /** Holds the stack ID. */
    QString                 m_strID;
    /** Holds the stack orientation. */
    UIPopupStackOrientation m_enmOrientation;

    /** Holds the main-layout instance. */
    QVBoxLayout          *m_pMainLayout;
    /** Holds the scroll-area instance. */
    QScrollArea          *m_pScrollArea;
    /** Holds the scroll-viewport instance. */
    UIPopupStackViewport *m_pScrollViewport;

    /** Holds the parent menu-bar height. */
    int m_iParentMenuBarHeight;
    /** Holds the parent status-bar height. */
    int m_iParentStatusBarHeight;
};

#endif /* !___UIPopupStack_h___ */

