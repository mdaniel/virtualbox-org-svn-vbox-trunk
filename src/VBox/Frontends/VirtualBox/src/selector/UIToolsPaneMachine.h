/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsPaneMachine class declaration.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIToolsPaneMachine_h___
#define ___UIToolsPaneMachine_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QHBoxLayout;
class QStackedLayout;
class QVBoxLayout;
class UIActionPool;
class UIDesktopPane;
class UIGDetails;
class UISnapshotPane;
class UIVMItem;
class CMachine;


/** Machine tool types. */
enum ToolTypeMachine
{
    ToolTypeMachine_Invalid,
    ToolTypeMachine_Desktop,
    ToolTypeMachine_Details,
    ToolTypeMachine_Snapshots,
};

/* Make sure QVariant can eat ToolTypeMachine: */
Q_DECLARE_METATYPE(ToolTypeMachine);


/** QWidget subclass representing container for tool panes. */
class UIToolsPaneMachine : public QWidget
{
    Q_OBJECT;

signals:

    /** Redirects signal from UISelectorWindow to UIGDetails. */
    void sigSlidingStarted();
    /** Redirects signal from UISelectorWindow to UIGDetails. */
    void sigToggleStarted();
    /** Redirects signal from UISelectorWindow to UIGDetails. */
    void sigToggleFinished();
    /** Redirects signal from UIGDetails to UISelectorWindow. */
    void sigLinkClicked(const QString &strCategory, const QString &strControl, const QString &strId);

public:

    /** Constructs tools pane passing @a pParent to the base-class. */
    UIToolsPaneMachine(UIActionPool *pActionPool, QWidget *pParent = 0);
    /** Destructs tools pane. */
    virtual ~UIToolsPaneMachine() /* override */;

    /** Returns whether tool of particular @a enmType is opened. */
    bool isToolOpened(ToolTypeMachine enmType) const;
    /** Activates tool of passed @a enmType, creates new one if necessary. */
    void openTool(ToolTypeMachine enmType);
    /** Closes tool of passed @a enmType, deletes one if exists. */
    void closeTool(ToolTypeMachine enmType);

    /** Defines @a strText and switches to text details pane. */
    void setDetailsText(const QString &strText);
    /** Defines @a strError and switches to error details pane. */
    void setDetailsError(const QString &strError);

    /** Defines the machine @a items. */
    void setItems(const QList<UIVMItem*> &items);

    /** Defines the @a comMachine object. */
    void setMachine(const CMachine &comMachine);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares stacked-layout. */
    void prepareStackedLayout();
    /** Cleanups all. */
    void cleanup();

    /** Holds the action pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the stacked-layout instance. */
    QStackedLayout *m_pLayout;
    /** Holds the Desktop pane instance. */
    UIDesktopPane  *m_pPaneDesktop;
    /** Holds the Details pane instance. */
    UIGDetails     *m_pPaneDetails;
    /** Holds the Snapshots pane instance. */
    UISnapshotPane *m_pPaneSnapshots;
};

#endif /* !___UIToolsPaneMachine_h___ */

