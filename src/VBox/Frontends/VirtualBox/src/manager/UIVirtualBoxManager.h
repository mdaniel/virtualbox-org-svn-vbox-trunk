/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVirtualBoxManager_h___
#define ___UIVirtualBoxManager_h___

/* Qt includes: */
#include <QUrl>

/* GUI includes: */
#include "QIMainWindow.h"
#include "QIWithRetranslateUI.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"
#include "VBoxGlobal.h"

/* Forward declarations: */
class QMenu;
class QIManagerDialog;
class QISplitter;
class UIAction;
class UIActionPool;
class UIChooser;
class UISlidingWidget;
class UITabBar;
class UIToolBar;
class UIToolbarTools;
class UIVirtualMachineItem;

/* Type definitions: */
typedef QMap<QString, QIManagerDialog*> VMLogViewerMap;

/** Singleton QIMainWindow extension used as VirtualBox Manager instance. */
class UIVirtualBoxManager : public QIWithRetranslateUI<QIMainWindow>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about this window remapped to another screen. */
    void sigWindowRemapped();

public:

    /** Singleton constructor. */
    static void create();
    /** Singleton destructor. */
    static void destroy();
    /** Singleton instance provider. */
    static UIVirtualBoxManager *instance() { return s_pInstance; }

    /** Returns the action-pool instance. */
    UIActionPool *actionPool() const { return m_pActionPool; }

protected:

    /** Constructs VirtualBox Manager. */
    UIVirtualBoxManager();
    /** Destructs VirtualBox Manager. */
    virtual ~UIVirtualBoxManager() /* override */;

    /** Returns whether the window should be maximized when geometry being restored. */
    virtual bool shouldBeMaximized() const /* override */;

private slots:

    /** @name Common stuff.
      * @{ */
        /** Handles polishing in the async way. */
        void sltHandlePolishEvent();

#if QT_VERSION == 0
        /** Stupid moc does not warn if it cannot find headers! */
        void QT_VERSION_NOT_DEFINED
#elif defined(VBOX_WS_X11)
        /** Handles host-screen available-area change. */
        void sltHandleHostScreenAvailableAreaChange();
#endif /* VBOX_WS_X11 */

        /** Handles context-menu request for passed @a position. */
        void sltHandleContextMenuRequest(const QPoint &position);

        /** Handles signal about Chooser-pane index change.
          * @param  fUpdateDetails    Brings whether details should be updated.
          * @param  fUpdateSnapshots  Brings whether tools should be updated. */
        void sltHandleChooserPaneIndexChange(bool fUpdateDetails = true,
                                             bool fUpdateSnapshots = true,
                                             bool fUpdateLogViewer = true);

        /** Handles signal about medium-enumeration finished. */
        void sltHandleMediumEnumerationFinish();

        /** Handles call to open a @a list of URLs. */
        void sltHandleOpenUrlCall(QList<QUrl> list = QList<QUrl>());

        /** Handles signal about group saving progress change. */
        void sltHandleGroupSavingProgressChange();

#ifdef VBOX_WS_MAC
        /** Handles signal about some @a pAction hovered. */
        void sltActionHovered(UIAction *pAction);
#endif
    /** @} */

    /** @name CVirtualBox event handling stuff.
      * @{ */
        /** Handles CVirtualBox event about state change for machine with @a strID. */
        void sltHandleStateChange(const QString &strID);
    /** @} */

    /** @name File menu stuff.
      * @{ */
        /** Handles call to open Virtual Medium Manager window. */
        void sltOpenVirtualMediumManagerWindow();
        /** Handles call to close Virtual Medium Manager window. */
        void sltCloseVirtualMediumManagerWindow();

        /** Handles call to open Host Network Manager window. */
        void sltOpenHostNetworkManagerWindow();
        /** Handles call to close Host Network Manager window. */
        void sltCloseHostNetworkManagerWindow();

        /** Handles call to close a Machine LogViewer window. */
        void sltCloseLogViewerWindow();

        /** Handles call to open Import Appliance wizard.
          * @param strFileName can bring the name of file to import appliance from. */
        void sltOpenImportApplianceWizard(const QString &strFileName = QString());
        /** Handles call to open Export Appliance wizard. */
        void sltOpenExportApplianceWizard();

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
        /** Handles call to open Extra-data Manager window. */
        void sltOpenExtraDataManagerWindow();
#endif

        /** Handles call to open Preferences dialog. */
        void sltOpenPreferencesDialog();

        /** Handles call to exit application. */
        void sltPerformExit();
    /** @} */

    /** @name Machine menu stuff.
      * @{ */
        /** Handles call to open Add Machine dialog.
          * @param strFileName can bring the name of file to add machine from. */
        void sltOpenAddMachineDialog(const QString &strFileName = QString());

        /** Handles call to open Machine Settings dialog.
          * @param strCategory can bring the settings category to start from.
          * @param strControl  can bring the widget of the page to focus.
          * @param strID       can bring the ID of machine to manage. */
        void sltOpenMachineSettingsDialog(const QString &strCategory = QString(),
                                          const QString &strControl = QString(),
                                          const QString &strID = QString());

        /** Handles call to open Clone Machine wizard. */
        void sltOpenCloneMachineWizard();

        /** Handles the Move Machine action. */
        void sltPerformMoveMachine();

        /** Handles call to start or show machine. */
        void sltPerformStartOrShowMachine();
        /** Handles call to start machine in normal mode. */
        void sltPerformStartMachineNormal();
        /** Handles call to start machine in headless mode. */
        void sltPerformStartMachineHeadless();
        /** Handles call to start machine in detachable mode. */
        void sltPerformStartMachineDetachable();

        /** Handles call to discard machine state. */
        void sltPerformDiscardMachineState();

        /** Handles call to @a fPause or resume machine otherwise. */
        void sltPerformPauseOrResumeMachine(bool fPause);

        /** Handles call to reset machine. */
        void sltPerformResetMachine();

        /** Handles call to detach machine UI. */
        void sltPerformDetachMachineUI();
        /** Handles call to save machine state. */
        void sltPerformSaveMachineState();
        /** Handles call to ask machine for shutdown. */
        void sltPerformShutdownMachine();
        /** Handles call to power machine off. */
        void sltPerformPowerOffMachine();

        /** Handles call to open machine Log dialog. */
        void sltOpenMachineLogDialog();

        /** Handles call to show machine in File Manager. */
        void sltShowMachineInFileManager();

        /** Handles call to create machine shortcut. */
        void sltPerformCreateMachineShortcut();

        /** Handles call to show group Close menu. */
        void sltGroupCloseMenuAboutToShow();
        /** Handles call to show machine Close menu. */
        void sltMachineCloseMenuAboutToShow();
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Handles tools type switch. */
        void sltHandleToolsTypeSwitch();

        /** Handles request to show Machine tab-bar. */
        void sltHandleShowTabBarMachine();
        /** Handles request to show Global tab-bar. */
        void sltHandleShowTabBarGlobal();

        /** Handles rquest to open Machine tool of passed @a enmType. */
        void sltHandleToolOpenedMachine(ToolTypeMachine enmType);
        /** Handles rquest to open Global tool of passed @a enmType. */
        void sltHandleToolOpenedGlobal(ToolTypeGlobal enmType);

        /** Handles rquest to close Machine tool of passed @a enmType. */
        void sltHandleToolClosedMachine(ToolTypeMachine enmType);
        /** Handles rquest to close Global tool of passed @a enmType. */
        void sltHandleToolClosedGlobal(ToolTypeGlobal enmType);
    /** @} */

private:

    /** Returns current-item. */
    UIVirtualMachineItem *currentItem() const;
    /** Returns a list of current-items. */
    QList<UIVirtualMachineItem*> currentItems() const;

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles any Qt @a pEvent. */
        virtual bool event(QEvent *pEvent) /* override */;
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;
        /** Handles first show @a pEvent. */
        virtual void polishEvent(QShowEvent *pEvent) /* override */;
#ifdef VBOX_WS_MAC
        /** Mac OS X: Preprocesses any @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
#endif
        /** Handles close @a pEvent. */
        virtual void closeEvent(QCloseEvent *pEvent) /* override */;
    /** @} */

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares window. */
        void prepare();
        /** Prepares icon. */
        void prepareIcon();
        /** Prepares menu-bar. */
        void prepareMenuBar();
        /** Prepares @a pMenu File. */
        void prepareMenuFile(QMenu *pMenu);
        /** Prepares @a pMenu Group. */
        void prepareMenuGroup(QMenu *pMenu);
        /** Prepares @a pMenu Machine. */
        void prepareMenuMachine(QMenu *pMenu);
        /** Prepares @a pMenu Group => Start or Show. */
        void prepareMenuGroupStartOrShow(QMenu *pMenu);
        /** Prepares @a pMenu Machine => Start or Show. */
        void prepareMenuMachineStartOrShow(QMenu *pMenu);
        /** Prepares @a pMenu Group => Close. */
        void prepareMenuGroupClose(QMenu *pMenu);
        /** Prepares @a pMenu Machine => Close. */
        void prepareMenuMachineClose(QMenu *pMenu);
        /** Prepares status-bar. */
        void prepareStatusBar();
        /** Prepares toolbar. */
        void prepareToolbar();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Saves settings. */
        void saveSettings();
        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups menu-bar. */
        void cleanupMenuBar();
        /** Cleanups window. */
        void cleanup();
    /** @} */

    /** @name VM launching stuff.
      * @{ */
        /** Launches or shows virtual machines represented by passed @a items in corresponding @a enmLaunchMode (for launch). */
        void performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, VBoxGlobal::LaunchMode enmLaunchMode);
    /** @} */

    /** @name Action update stuff.
      * @{ */
        /** Performs update of actions visibility. */
        void updateActionsVisibility();
        /** Performs update of actions appearance. */
        void updateActionsAppearance();

        /** Returns whether the action with @a iActionIndex is enabled.
          * @param items used to calculate verdict about should the action be enabled. */
        bool isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items);

        /** Returns whether all passed @a items are powered off. */
        static bool isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is able to shutdown. */
        static bool isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items supports shortcut creation. */
        static bool isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is accessible. */
        static bool isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is inaccessible. */
        static bool isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is removable. */
        static bool isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started. */
        static bool isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be shown. */
        static bool isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be started or shown. */
        static bool isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items can be discarded. */
        static bool isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is started. */
        static bool isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items);
        /** Returns whether at least one of passed @a items is running. */
        static bool isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items);
    /** @} */

    /** Holds the static instance. */
    static UIVirtualBoxManager *s_pInstance;

    /** Holds whether the dialog is polished. */
    bool  m_fPolished                      : 1;
    /** Holds whether first medium enumeration handled. */
    bool  m_fFirstMediumEnumerationHandled : 1;

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the sliding-widget isntance. */
    UISlidingWidget *m_pSlidingWidget;

    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;

    /** Holds the main toolbar instance. */
    UIToolBar *m_pToolBar;

    /** Holds the Machine tab-bar instance. */
    UITabBar *m_pTabBarMachine;
    /** Holds the Global tab-bar instance. */
    UITabBar *m_pTabBarGlobal;
    /** Holds the Machine tab-bar action reference. */
    QAction *m_pActionTabBarMachine;
    /** Holds the Global tab-bar action reference. */
    QAction *m_pActionTabBarGlobal;

    /** Holds the Tools-toolbar instance. */
    UIToolbarTools *m_pToolbarTools;

    /** Holds the Machine Tools order. */
    QList<ToolTypeMachine>  m_orderMachine;
    /** Holds the Global Tools order. */
    QList<ToolTypeGlobal>   m_orderGlobal;

    /** Holds the Chooser-pane instance. */
    UIChooser         *m_pPaneChooser;
    /** Holds the Machine Tools-pane instance. */
    UIToolPaneMachine *m_pPaneToolsMachine;
    /** Holds the Global Tools-pane instance. */
    UIToolPaneGlobal  *m_pPaneToolsGlobal;

    /** Holds the list of Group menu actions. */
    QList<UIAction*>  m_groupActions;
    /** Holds the Group menu parent action. */
    QAction          *m_pGroupMenuAction;

    /** Holds the list of Machine menu actions. */
    QList<UIAction*>  m_machineActions;
    /** Holds the Machine menu parent action. */
    QAction          *m_pMachineMenuAction;

    /** Holds the Virtual Media Manager window instance. */
    QIManagerDialog *m_pManagerVirtualMedia;
    /** Holds the Host Network Manager window instance. */
    QIManagerDialog *m_pManagerHostNetwork;
    /** Holds a map of (machineUUID, UIVMLogViewerDialog). */
    VMLogViewerMap   m_logViewers;
};

#define gpManager UIVirtualBoxManager::instance()

#endif /* !___UIVirtualBoxManager_h___ */
