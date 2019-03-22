/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManager class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QMenuBar>
# include <QResizeEvent>
# include <QStandardPaths>
# include <QStatusBar>
# include <QToolButton>
# include <QTimer>

/* GUI includes: */
# include "QIFileDialog.h"
# include "QISplitter.h"
# include "UIActionPoolSelector.h"
# include "UIDesktopServices.h"
# include "UIErrorString.h"
# include "UIExtraDataManager.h"
# include "UIChooser.h"
# include "UIHostNetworkManager.h"
# include "UIMedium.h"
# include "UIMediumManager.h"
# include "UIMessageCenter.h"
# include "UIModalWindowManager.h"
# include "UIVirtualBoxManager.h"
# include "UISettingsDialogSpecific.h"
# include "UISlidingWidget.h"
# include "UITabBar.h"
# include "UIToolBar.h"
# include "UIVMLogViewerDialog.h"
# include "UIVirtualMachineItem.h"
# include "UIToolPaneMachine.h"
# include "UIToolbarTools.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UIUpdateManager.h"
# endif
# include "UIVirtualBoxEventHandler.h"
# include "UIWizardCloneVM.h"
# include "UIWizardExportApp.h"
# include "UIWizardImportApp.h"
# ifdef VBOX_WS_MAC
#  include "UIImageTools.h"
#  include "UIWindowMenuManager.h"
#  include "VBoxUtils.h"
# endif
# ifdef VBOX_WS_X11
#  include "UIDesktopWidgetWatchdog.h"
# endif
# ifndef VBOX_WS_MAC
#  include "UIMenuBar.h"
# endif

/* COM includes: */
# include "CSystemProperties.h"

/* Other VBox stuff: */
# include <iprt/buildconfig.h>
# include <VBox/version.h>
# ifdef VBOX_WS_X11
#  include <iprt/env.h>
# endif /* VBOX_WS_X11 */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/* static */
UIVirtualBoxManager *UIVirtualBoxManager::s_pInstance = 0;

/* static */
void UIVirtualBoxManager::create()
{
    /* Make sure VirtualBox Manager isn't created: */
    AssertReturnVoid(s_pInstance == 0);

    /* Create VirtualBox Manager: */
    new UIVirtualBoxManager;
    /* Prepare VirtualBox Manager: */
    s_pInstance->prepare();
    /* Show VirtualBox Manager: */
    s_pInstance->show();
    /* Register in the modal window manager: */
    windowManager().setMainWindowShown(s_pInstance);
}

/* static */
void UIVirtualBoxManager::destroy()
{
    /* Make sure VirtualBox Manager is created: */
    AssertPtrReturnVoid(s_pInstance);

    /* Unregister in the modal window manager: */
    windowManager().setMainWindowShown(0);
    /* Cleanup VirtualBox Manager: */
    s_pInstance->cleanup();
    /* Destroy machine UI: */
    delete s_pInstance;
}

UIVirtualBoxManager::UIVirtualBoxManager()
    : m_fPolished(false)
    , m_fFirstMediumEnumerationHandled(false)
    , m_pActionPool(0)
    , m_pSlidingWidget(0)
    , m_pSplitter(0)
    , m_pToolBar(0)
    , m_pTabBarMachine(0)
    , m_pTabBarGlobal(0)
    , m_pActionTabBarMachine(0)
    , m_pActionTabBarGlobal(0)
    , m_pToolbarTools(0)
    , m_pPaneChooser(0)
    , m_pPaneToolsMachine(0)
    , m_pPaneToolsGlobal(0)
    , m_pGroupMenuAction(0)
    , m_pMachineMenuAction(0)
    , m_pManagerVirtualMedia(0)
    , m_pManagerHostNetwork(0)
{
    s_pInstance = this;
}

UIVirtualBoxManager::~UIVirtualBoxManager()
{
    s_pInstance = 0;
}

bool UIVirtualBoxManager::shouldBeMaximized() const
{
    return gEDataManager->selectorWindowShouldBeMaximized();
}

#ifdef VBOX_WS_MAC
bool UIVirtualBoxManager::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore for non-active window except for FileOpen event which should be always processed: */
    if (!isActiveWindow() && pEvent->type() != QEvent::FileOpen)
        return QIWithRetranslateUI<QIMainWindow>::eventFilter(pObject, pEvent);

    /* Ignore for other objects: */
    if (qobject_cast<QWidget*>(pObject) &&
        qobject_cast<QWidget*>(pObject)->window() != this)
        return QIWithRetranslateUI<QIMainWindow>::eventFilter(pObject, pEvent);

    /* Which event do we have? */
    switch (pEvent->type())
    {
        case QEvent::FileOpen:
        {
            sltHandleOpenUrlCall(QList<QUrl>() << static_cast<QFileOpenEvent*>(pEvent)->url());
            pEvent->accept();
            return true;
            break;
        }
        default:
            break;
    }
    /* Call to base-class: */
    return QIWithRetranslateUI<QIMainWindow>::eventFilter(pObject, pEvent);
}
#endif /* VBOX_WS_MAC */

void UIVirtualBoxManager::retranslateUi()
{
    /* Set window title: */
    QString strTitle(VBOX_PRODUCT);
    strTitle += " " + tr("Manager", "Note: main window title which is prepended by the product name.");
#ifdef VBOX_BLEEDING_EDGE
    strTitle += QString(" EXPERIMENTAL build ")
             +  QString(RTBldCfgVersion())
             +  QString(" r")
             +  QString(RTBldCfgRevisionStr())
             +  QString(" - " VBOX_BLEEDING_EDGE);
#endif /* VBOX_BLEEDING_EDGE */
    setWindowTitle(strTitle);

    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange(false /* update details? */, false /* update snapshots? */, false /* update the logviewer? */);

#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text.
    m_pToolBar->updateLayout();
#endif
}

bool UIVirtualBoxManager::event(QEvent *pEvent)
{
    /* Which event do we have? */
    switch (pEvent->type())
    {
        /* Handle every ScreenChangeInternal event to notify listeners: */
        case QEvent::ScreenChangeInternal:
        {
            emit sigWindowRemapped();
            break;
        }
        /* Handle every Resize and Move we keep track of the geometry. */
        case QEvent::Resize:
        {
#ifdef VBOX_WS_X11
            /* Prevent handling if fake screen detected: */
            if (gpDesktop->isFakeScreenDetected())
                break;
#endif /* VBOX_WS_X11 */

            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
                QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                m_geometry.setSize(pResizeEvent->size());
            }
            break;
        }
        case QEvent::Move:
        {
#ifdef VBOX_WS_X11
            /* Prevent handling if fake screen detected: */
            if (gpDesktop->isFakeScreenDetected())
                break;
#endif /* VBOX_WS_X11 */

            if (isVisible() && (windowState() & (Qt::WindowMaximized | Qt::WindowMinimized | Qt::WindowFullScreen)) == 0)
            {
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
                QMoveEvent *pMoveEvent = static_cast<QMoveEvent*>(pEvent);
                m_geometry.moveTo(pMoveEvent->pos());
#else /* !VBOX_WS_MAC && !VBOX_WS_WIN */
                m_geometry.moveTo(geometry().x(), geometry().y());
#endif /* !VBOX_WS_MAC && !VBOX_WS_WIN */
            }
            break;
        }
#ifdef VBOX_WS_MAC
        case QEvent::ContextMenu:
        {
            /* This is the unified context menu event. Lets show the context menu. */
            QContextMenuEvent *pContextMenuEvent = static_cast<QContextMenuEvent*>(pEvent);
            sltHandleContextMenuRequest(pContextMenuEvent->globalPos());
            /* Accept it to interrupt the chain. */
            pContextMenuEvent->accept();
            return false;
            break;
        }
#endif /* VBOX_WS_MAC */
        default:
            break;
    }
    /* Call to base-class: */
    return QIWithRetranslateUI<QIMainWindow>::event(pEvent);
}

void UIVirtualBoxManager::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QIMainWindow>::showEvent(pEvent);

    /* Is polishing required? */
    if (!m_fPolished)
    {
        /* Pass the show-event to polish-event: */
        polishEvent(pEvent);
        /* Mark as polished: */
        m_fPolished = true;
    }
}

void UIVirtualBoxManager::polishEvent(QShowEvent *)
{
    /* Make sure user warned about inaccessible medium(s)
     * even if enumeration had finished before selector window shown: */
    QTimer::singleShot(0, this, SLOT(sltHandleMediumEnumerationFinish()));

    /* Call for async polishing: */
    QMetaObject::invokeMethod(this, "sltHandlePolishEvent", Qt::QueuedConnection);
}

void UIVirtualBoxManager::closeEvent(QCloseEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI<QIMainWindow>::closeEvent(pEvent);

    /* Quit application: */
    QApplication::quit();
}

void UIVirtualBoxManager::sltHandlePolishEvent()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Make sure there is accessible VM item chosen: */
    if (pItem && pItem->accessible())
    {
        // WORKAROUND:
        // By some reason some of X11 DEs unable to update() tab-bars on startup.
        // Let's just _create_ them later, asynchronously after the showEvent().
        /* Restore previously opened Machine tools at startup: */
        QMap<ToolTypeMachine, QAction*> mapActionsMachine;
        mapActionsMachine[ToolTypeMachine_Details] = actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Details);
        mapActionsMachine[ToolTypeMachine_Snapshots] = actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots);
        mapActionsMachine[ToolTypeMachine_LogViewer] = actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer);
        for (int i = m_orderMachine.size() - 1; i >= 0; --i)
            if (m_orderMachine.at(i) != ToolTypeMachine_Invalid)
                mapActionsMachine.value(m_orderMachine.at(i))->trigger();
        /* Make sure further action triggering cause tool type switch as well: */
        actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->setProperty("watch_child_activation", true);
    }
}

#ifdef VBOX_WS_X11
void UIVirtualBoxManager::sltHandleHostScreenAvailableAreaChange()
{
    /* Prevent handling if fake screen detected: */
    if (gpDesktop->isFakeScreenDetected())
        return;

    /* Restore the geometry cached by the window: */
    resize(m_geometry.size());
    move(m_geometry.topLeft());
}
#endif /* VBOX_WS_X11 */

void UIVirtualBoxManager::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Populate toolbar/statusbar acctions: */
    QList<QAction*> actions;

    /* Create 'Show Toolbar' action: */
    QAction *pShowToolBar = new QAction(tr("Show Toolbar"), 0);
    AssertPtrReturnVoid(pShowToolBar);
    {
        /* Configure action: */
        pShowToolBar->setCheckable(true);
        pShowToolBar->setChecked(m_pToolBar->isVisible());

        /* Add into action list: */
        actions << pShowToolBar;
    }

    /* Create 'Show Toolbar Text' action: */
    QAction *pShowToolBarText = new QAction(tr("Show Toolbar Text"), 0);
    AssertPtrReturnVoid(pShowToolBarText);
    {
        /* Configure action: */
        pShowToolBarText->setCheckable(true);
        pShowToolBarText->setChecked(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);

        /* Add into action list: */
        actions << pShowToolBarText;
    }

    /* Prepare the menu position: */
    QPoint globalPosition = position;
    QWidget *pSender = static_cast<QWidget*>(sender());
    if (pSender)
        globalPosition = pSender->mapToGlobal(position);
    QAction *pResult = QMenu::exec(actions, globalPosition);
    if (pResult == pShowToolBar)
    {
        if (pResult->isChecked())
            m_pToolBar->show();
        else
            m_pToolBar->hide();
    }
    else if (pResult == pShowToolBarText)
    {
        m_pToolBar->setToolButtonStyle(  pResult->isChecked()
                                       ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
        m_pToolbarTools->setToolButtonStyle(  pResult->isChecked()
                                            ? Qt::ToolButtonTextUnderIcon
                                            : Qt::ToolButtonIconOnly);
    }
}

void UIVirtualBoxManager::sltHandleChooserPaneIndexChange(bool fUpdateDetails /* = true */,
                                                          bool fUpdateSnapshots /* = true */,
                                                          bool fUpdateLogViewer /* = true */)
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Update action visibility: */
    updateActionsVisibility();
    /* Update action appearance: */
    updateActionsAppearance();

    /* Update Tools-pane: */
    m_pPaneToolsMachine->setCurrentItem(pItem);

    /* Update Machine tab-bar visibility */
    m_pTabBarMachine->setEnabled(pItem && pItem->accessible());

    /* If current item exists & accessible: */
    if (pItem && pItem->accessible())
    {
        /* If Desktop pane is chosen currently: */
        if (m_pPaneToolsMachine->currentTool() == ToolTypeMachine_Desktop)
        {
            /* Make sure Details or Snapshot pane is chosen if opened: */
            if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
                actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Details)->trigger();
            else
            if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Snapshots))
                actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots)->trigger();
            else
            if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_LogViewer))
                actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer)->trigger();
        }

        /* Update Details-pane (if requested): */
        if (   fUpdateDetails
            && m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
            m_pPaneToolsMachine->setItems(currentItems());
        /* Update the Snapshots-pane or/and Logviewer-pane (if requested): */
        if (fUpdateSnapshots || fUpdateLogViewer)
            m_pPaneToolsMachine->setMachine(pItem->machine());
    }
    else
    {
        /* Make sure Desktop-pane raised: */
        m_pPaneToolsMachine->openTool(ToolTypeMachine_Desktop);

        /* Note that the machine becomes inaccessible (or if the last VM gets
         * deleted), we have to update all fields, ignoring input arguments. */
        if (pItem)
        {
            /* The VM is inaccessible: */
            m_pPaneToolsMachine->setDetailsError(UIErrorString::formatErrorInfo(pItem->accessError()));
        }

        /* Update Details-pane (in any case): */
        if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
            m_pPaneToolsMachine->setItems(currentItems());
        /* Update Snapshots-pane and Logviewer-pane (in any case): */
        m_pPaneToolsMachine->setMachine(CMachine());
    }
}

void UIVirtualBoxManager::sltHandleMediumEnumerationFinish()
{
    /* To avoid annoying the user, we check for inaccessible mediums just once, after
     * the first media emumeration [started from main() at startup] is complete. */
    if (m_fFirstMediumEnumerationHandled)
        return;
    m_fFirstMediumEnumerationHandled = true;

    /* Make sure MM window/tool is not opened,
     * otherwise user sees everything himself: */
    if (   m_pManagerVirtualMedia
        || m_pPaneToolsGlobal->isToolOpened(ToolTypeGlobal_VirtualMedia))
        return;

    /* Look for at least one inaccessible medium: */
    bool fIsThereAnyInaccessibleMedium = false;
    foreach (const QString &strMediumID, vboxGlobal().mediumIDs())
    {
        if (vboxGlobal().medium(strMediumID).state() == KMediumState_Inaccessible)
        {
            fIsThereAnyInaccessibleMedium = true;
            break;
        }
    }

    /* Warn the user about inaccessible medium, propose to open MM window/tool: */
    if (fIsThereAnyInaccessibleMedium && !msgCenter().warnAboutInaccessibleMedia())
    {
        /* Open the MM window: */
        sltOpenVirtualMediumManagerWindow();
    }
}

void UIVirtualBoxManager::sltHandleOpenUrlCall(QList<QUrl> list /* = QList<QUrl>() */)
{
    /* If passed list is empty: */
    if (list.isEmpty())
    {
        /* We take the one which stored in VBoxGlobal: */
        list = vboxGlobal().argUrlList();
        vboxGlobal().argUrlList().clear();
        /// @todo: Rework this getter to do .clear() as well.
    }

    /* Check if we are can handle the dropped urls: */
    for (int i = 0; i < list.size(); ++i)
    {
#ifdef VBOX_WS_MAC
        const QString strFile = ::darwinResolveAlias(list.at(i).toLocalFile());
#else
        const QString strFile = list.at(i).toLocalFile();
#endif
        /* If there is such file exists: */
        if (!strFile.isEmpty() && QFile::exists(strFile))
        {
            /* And has allowed VBox config file extension: */
            if (VBoxGlobal::hasAllowedExtension(strFile, VBoxFileExts))
            {
                /* Handle VBox config file: */
                CVirtualBox comVBox = vboxGlobal().virtualBox();
                CMachine comMachine = comVBox.FindMachine(strFile);
                if (comVBox.isOk() && comMachine.isNotNull())
                    vboxGlobal().launchMachine(comMachine);
                else
                    sltOpenAddMachineDialog(strFile);
            }
            /* And has allowed VBox OVF file extension: */
            else if (VBoxGlobal::hasAllowedExtension(strFile, OVFFileExts))
            {
                /* Allow only one file at the time: */
                sltOpenImportApplianceWizard(strFile);
                break;
            }
            /* And has allowed VBox extension pack file extension: */
            else if (VBoxGlobal::hasAllowedExtension(strFile, VBoxExtPackFileExts))
            {
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
                /* Prevent update manager from proposing us to update EP: */
                gUpdateManager->setEPInstallationRequested(true);
#endif
                /* Propose the user to install EP described by the arguments @a list. */
                vboxGlobal().doExtPackInstallation(strFile, QString(), this, NULL);
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
                /* Allow update manager to propose us to update EP: */
                gUpdateManager->setEPInstallationRequested(false);
#endif
            }
        }
    }
}

void UIVirtualBoxManager::sltHandleGroupSavingProgressChange()
{
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltHandleStateChange(const QString &)
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Make sure current item present: */
    if (!pItem)
        return;

    /* Update actions: */
    updateActionsAppearance();
}

void UIVirtualBoxManager::sltOpenVirtualMediumManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pPaneToolsGlobal->isToolOpened(ToolTypeGlobal_VirtualMedia))
    {
        sltHandleToolOpenedGlobal(ToolTypeGlobal_VirtualMedia);
        return;
    }

    /* Create instance if not yet created: */
    if (!m_pManagerVirtualMedia)
    {
        UIMediumManagerFactory().prepare(m_pManagerVirtualMedia, this);
        connect(m_pManagerVirtualMedia, &QIManagerDialog::sigClose,
                this, &UIVirtualBoxManager::sltCloseVirtualMediumManagerWindow);
    }

    /* Show instance: */
    m_pManagerVirtualMedia->show();
    m_pManagerVirtualMedia->setWindowState(m_pManagerVirtualMedia->windowState() & ~Qt::WindowMinimized);
    m_pManagerVirtualMedia->activateWindow();
}

void UIVirtualBoxManager::sltCloseVirtualMediumManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerVirtualMedia)
        UIMediumManagerFactory().cleanup(m_pManagerVirtualMedia);
}

void UIVirtualBoxManager::sltOpenHostNetworkManagerWindow()
{
    /* First check if instance of widget opened the embedded way: */
    if (m_pPaneToolsGlobal->isToolOpened(ToolTypeGlobal_HostNetwork))
    {
        sltHandleToolOpenedGlobal(ToolTypeGlobal_HostNetwork);
        return;
    }

    /* Create instance if not yet created: */
    if (!m_pManagerHostNetwork)
    {
        UIHostNetworkManagerFactory().prepare(m_pManagerHostNetwork, this);
        connect(m_pManagerHostNetwork, &QIManagerDialog::sigClose,
                this, &UIVirtualBoxManager::sltCloseHostNetworkManagerWindow);
    }

    /* Show instance: */
    m_pManagerHostNetwork->show();
    m_pManagerHostNetwork->setWindowState(m_pManagerHostNetwork->windowState() & ~Qt::WindowMinimized);
    m_pManagerHostNetwork->activateWindow();
}

void UIVirtualBoxManager::sltCloseHostNetworkManagerWindow()
{
    /* Destroy instance if still exists: */
    if (m_pManagerHostNetwork)
        UIHostNetworkManagerFactory().cleanup(m_pManagerHostNetwork);
}

void UIVirtualBoxManager::sltOpenImportApplianceWizard(const QString &strFileName /* = QString() */)
{
    /* Show Import Appliance wizard: */
#ifdef VBOX_WS_MAC
    const QString strTmpFile = ::darwinResolveAlias(strFileName);
#else
    const QString strTmpFile = strFileName;
#endif

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setEnabled(false);

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizardImportApp pWizard = new UIWizardImportApp(pWizardParent, strTmpFile);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    if (strFileName.isEmpty() || pWizard->isValid())
        pWizard->exec();
    delete pWizard;

    /// @todo Is it possible at all if event-loop unwind?
    /* Unlock the action allowing further calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance)->setEnabled(true);
}

void UIVirtualBoxManager::sltOpenExportApplianceWizard()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Populate the list of VM names: */
    QStringList names;
    for (int i = 0; i < items.size(); ++i)
        names << items.at(i)->name();

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setEnabled(false);

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);
    UISafePointerWizard pWizard = new UIWizardExportApp(pWizardParent, names);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;

    /// @todo Is it possible at all if event-loop unwind?
    /* Unlock the action allowing further calls: */
    actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance)->setEnabled(true);
}

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
void UIVirtualBoxManager::sltOpenExtraDataManagerWindow()
{
    gEDataManager->openWindow(this);
}
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

void UIVirtualBoxManager::sltOpenPreferencesDialog()
{
    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndex_M_Application_S_Preferences)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(true);

    /* Don't show the inaccessible warning
     * if the user tries to open global settings: */
    m_fFirstMediumEnumerationHandled = true;

    /* Create and execute global settings window: */
    QPointer<UISettingsDialogGlobal> pDlg = new UISettingsDialogGlobal(this);
    pDlg->execute();
    delete pDlg;

    /// @todo Is it possible at all if event-loop unwind?
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndex_M_Application_S_Preferences)->setData(false);
}

void UIVirtualBoxManager::sltPerformExit()
{
    close();
}

void UIVirtualBoxManager::sltOpenAddMachineDialog(const QString &strFileName /* = QString() */)
{
    /* Initialize variables: */
#ifdef VBOX_WS_MAC
    QString strTmpFile = ::darwinResolveAlias(strFileName);
#else /* VBOX_WS_MAC */
    QString strTmpFile = strFileName;
#endif /* !VBOX_WS_MAC */
    CVirtualBox vbox = vboxGlobal().virtualBox();
    if (strTmpFile.isEmpty())
    {
        QString strBaseFolder = vbox.GetSystemProperties().GetDefaultMachineFolder();
        QString strTitle = tr("Select a virtual machine file");
        QStringList extensions;
        for (int i = 0; i < VBoxFileExts.size(); ++i)
            extensions << QString("*.%1").arg(VBoxFileExts[i]);
        QString strFilter = tr("Virtual machine files (%1)").arg(extensions.join(" "));
        /* Create open file dialog: */
        QStringList fileNames = QIFileDialog::getOpenFileNames(strBaseFolder, strFilter, this, strTitle, 0, true, true);
        if (!fileNames.isEmpty())
            strTmpFile = fileNames.at(0);
    }
    /* Nothing was chosen? */
    if (strTmpFile.isEmpty())
        return;

    /* Make sure this machine can be opened: */
    CMachine newMachine = vbox.OpenMachine(strTmpFile);
    if (!vbox.isOk())
    {
        msgCenter().cannotOpenMachine(vbox, strTmpFile);
        return;
    }

    /* Make sure this machine was NOT registered already: */
    CMachine oldMachine = vbox.FindMachine(newMachine.GetId());
    if (!oldMachine.isNull())
    {
        msgCenter().cannotReregisterExistingMachine(strTmpFile, oldMachine.GetName());
        return;
    }

    /* Register that machine: */
    vbox.RegisterMachine(newMachine);
}

void UIVirtualBoxManager::sltOpenMachineSettingsDialog(const QString &strCategoryRef /* = QString() */,
                                                       const QString &strControlRef /* = QString() */,
                                                       const QString &strID /* = QString() */)
{
    /* This slot should not be called when there is not selection: */
    AssertMsgReturnVoid(currentItem(), ("Current item should be selected!\n"));

    /* Check that we do NOT handling that already: */
    if (actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->data().toBool())
        return;
    /* Remember that we handling that already: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setData(true);

    /* Process href from VM details / description: */
    if (!strCategoryRef.isEmpty() && strCategoryRef[0] != '#')
    {
        vboxGlobal().openURL(strCategoryRef);
        return;
    }

    /* Get category and control: */
    QString strCategory = strCategoryRef;
    QString strControl = strControlRef;
    /* Check if control is coded into the URL by %%: */
    if (strControl.isEmpty())
    {
        QStringList parts = strCategory.split("%%");
        if (parts.size() == 2)
        {
            strCategory = parts.at(0);
            strControl = parts.at(1);
        }
    }

    /* Don't show the inaccessible warning
     * if the user tries to open VM settings: */
    m_fFirstMediumEnumerationHandled = true;

    /* Create and execute corresponding VM settings window: */
    QPointer<UISettingsDialogMachine> pDlg = new UISettingsDialogMachine(this,
                                                                         QUuid(strID).isNull() ? currentItem()->id() : strID,
                                                                         strCategory, strControl);
    pDlg->execute();
    delete pDlg;

    /// @todo Is it possible at all if event-loop unwind?
    /* Remember that we do NOT handling that already: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setData(false);
}

void UIVirtualBoxManager::sltOpenCloneMachineWizard()
{
    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Lock the action preventing cascade calls: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(false);

    /* Use the "safe way" to open stack of Mac OS X Sheets: */
    QWidget *pWizardParent = windowManager().realParentWindow(this);

    const QStringList &machineGroupNames = pItem->groups();
    QString strGroup = !machineGroupNames.isEmpty() ? machineGroupNames.at(0) : QString();
    UISafePointerWizard pWizard = new UIWizardCloneVM(pWizardParent, pItem->machine(), strGroup);
    windowManager().registerNewParent(pWizard, pWizardParent);
    pWizard->prepare();
    pWizard->exec();
    delete pWizard;

    /// @todo Is it possible at all if event-loop unwind?
    /* Unlock the action allowing further calls: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(true);
}

void UIVirtualBoxManager::sltPerformMoveMachine()
{
    UIVirtualMachineItem *pItem = currentItem();
    AssertMsgReturnVoid(pItem, ("Current item should be selected!\n"));

    /* Open a session thru which we will modify the machine: */
    CSession comSession = vboxGlobal().openSession(pItem->id(), KLockType_Write);
    if (comSession.isNull())
        return;

    /* Get session machine: */
    CMachine comMachine = comSession.GetMachine();
    AssertMsgReturnVoid(comSession.isOk() && comMachine.isNotNull(), ("Unable to acquire machine!\n"));

    /* Open a file dialog for the user to select a destination folder. Start with the default machine folder: */
    CVirtualBox comVBox = vboxGlobal().virtualBox();
    QString strBaseFolder = comVBox.GetSystemProperties().GetDefaultMachineFolder();
    QString strTitle = tr("Select a destination folder to move the selected virtual machine");
    QString strDestinationFolder = QIFileDialog::getExistingDirectory(strBaseFolder, this, strTitle);
    if (!strDestinationFolder.isEmpty())
    {
        /* Prepare machine move progress: */
        CProgress comProgress = comMachine.MoveTo(strDestinationFolder, "basic");
        if (comMachine.isOk() && comProgress.isNotNull())
        {
            /* Show machine move progress: */
            msgCenter().showModalProgressDialog(comProgress, comMachine.GetName(), ":/progress_clone_90px.png");
            if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
                msgCenter().cannotMoveMachine(comProgress, comMachine.GetName());
        }
        else
            msgCenter().cannotMoveMachine(comMachine);
    }
    comSession.UnlockMachine();
}

void UIVirtualBoxManager::sltPerformStartOrShowMachine()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, VBoxGlobal::LaunchMode_Invalid);
}

void UIVirtualBoxManager::sltPerformStartMachineNormal()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, VBoxGlobal::LaunchMode_Default);
}

void UIVirtualBoxManager::sltPerformStartMachineHeadless()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, VBoxGlobal::LaunchMode_Headless);
}

void UIVirtualBoxManager::sltPerformStartMachineDetachable()
{
    /* Start selected VMs in corresponding mode: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));
    performStartOrShowVirtualMachines(items, VBoxGlobal::LaunchMode_Separate);
}

void UIVirtualBoxManager::sltPerformDiscardMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be discarded: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToDiscard;
    foreach (UIVirtualMachineItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Group_S_Discard, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToDiscard << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm discarding saved VM state: */
    if (!msgCenter().confirmDiscardSavedState(machineNames.join(", ")))
        return;

    /* For every confirmed item: */
    foreach (UIVirtualMachineItem *pItem, itemsToDiscard)
    {
        /* Open a session to modify VM: */
        CSession session = vboxGlobal().openSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session machine: */
        CMachine machine = session.GetMachine();
        machine.DiscardSavedState(true);
        if (!machine.isOk())
            msgCenter().cannotDiscardSavedState(machine);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformPauseOrResumeMachine(bool fPause)
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For every selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Get item state: */
        KMachineState state = pItem->machineState();

        /* Check if current item could be paused/resumed: */
        if (!isActionEnabled(UIActionIndexST_M_Group_T_Pause, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Check if current item already paused: */
        if (fPause &&
            (state == KMachineState_Paused ||
             state == KMachineState_TeleportingPausedVM))
            continue;

        /* Check if current item already resumed: */
        if (!fPause &&
            (state == KMachineState_Running ||
             state == KMachineState_Teleporting ||
             state == KMachineState_LiveSnapshotting))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Pause/resume VM: */
        if (fPause)
            console.Pause();
        else
            console.Resume();
        bool ok = console.isOk();
        if (!ok)
        {
            if (fPause)
                msgCenter().cannotPauseMachine(console);
            else
                msgCenter().cannotResumeMachine(console);
        }

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformResetMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be reseted: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToReset;
    foreach (UIVirtualMachineItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Group_S_Reset, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToReset << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm reseting VM: */
    if (!msgCenter().confirmResetMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToReset)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Reset VM: */
        console.Reset();

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformDetachMachineUI()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be detached: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /// @todo Detach separate UI process..
        AssertFailed();
    }
}

void UIVirtualBoxManager::sltPerformSaveMachineState()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if current item could be saved: */
        if (!isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Get session machine: */
        CMachine machine = session.GetMachine();
        /* Pause VM first if necessary: */
        if (pItem->machineState() != KMachineState_Paused)
            console.Pause();
        if (console.isOk())
        {
            /* Prepare machine state saving progress: */
            CProgress progress = machine.SaveState();
            if (machine.isOk())
            {
                /* Show machine state saving progress: */
                msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_state_save_90px.png");
                if (!progress.isOk() || progress.GetResultCode() != 0)
                    msgCenter().cannotSaveMachineState(progress, machine.GetName());
            }
            else
                msgCenter().cannotSaveMachineState(machine);
        }
        else
            msgCenter().cannotPauseMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformShutdownMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be shutdowned: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToShutdown;
    foreach (UIVirtualMachineItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToShutdown << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm ACPI shutdown current VM: */
    if (!msgCenter().confirmACPIShutdownMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToShutdown)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* ACPI Shutdown: */
        console.PowerButton();
        if (!console.isOk())
            msgCenter().cannotACPIShutdownMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltPerformPowerOffMachine()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* Prepare the list of the machines to be powered off: */
    QStringList machineNames;
    QList<UIVirtualMachineItem*> itemsToPowerOff;
    foreach (UIVirtualMachineItem *pItem, items)
        if (isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, QList<UIVirtualMachineItem*>() << pItem))
        {
            machineNames << pItem->name();
            itemsToPowerOff << pItem;
        }
    AssertMsg(!machineNames.isEmpty(), ("This action should not be allowed!"));

    /* Confirm Power Off current VM: */
    if (!msgCenter().confirmPowerOffMachine(machineNames.join(", ")))
        return;

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, itemsToPowerOff)
    {
        /* Open a session to modify VM state: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            return;

        /* Get session console: */
        CConsole console = session.GetConsole();
        /* Prepare machine power down: */
        CProgress progress = console.PowerDown();
        if (console.isOk())
        {
            /* Show machine power down progress: */
            CMachine machine = session.GetMachine();
            msgCenter().showModalProgressDialog(progress, machine.GetName(), ":/progress_poweroff_90px.png");
            if (!progress.isOk() || progress.GetResultCode() != 0)
                msgCenter().cannotPowerDownMachine(progress, machine.GetName());
        }
        else
            msgCenter().cannotPowerDownMachine(console);

        /* Unlock machine finally: */
        session.UnlockMachine();
    }
}

void UIVirtualBoxManager::sltOpenMachineLogDialog()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* First check if a logviewer is already opened in embedded mode: */
    if (m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_LogViewer))
    {
        sltHandleToolOpenedMachine(ToolTypeMachine_LogViewer);
        return;
    }

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if log could be show for the current item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        QIManagerDialog *pLogViewerDialog = 0;
        /* Create and Show VM Log Viewer: */
        if (!m_logViewers[pItem->machine().GetHardwareUUID()])
        {
            UIVMLogViewerDialogFactory dialogFactory(pItem->machine());
            dialogFactory.prepare(pLogViewerDialog, this);
            if (pLogViewerDialog)
            {
                m_logViewers[pItem->machine().GetHardwareUUID()] = pLogViewerDialog;
                connect(pLogViewerDialog, &QIManagerDialog::sigClose,
                        this, &UIVirtualBoxManager::sltCloseLogViewerWindow);
            }
        }
        else
        {
            pLogViewerDialog = m_logViewers[pItem->machine().GetHardwareUUID()];
        }
        if (pLogViewerDialog)
        {
            /* Show instance: */
            pLogViewerDialog->show();
            pLogViewerDialog->setWindowState(pLogViewerDialog->windowState() & ~Qt::WindowMinimized);
            pLogViewerDialog->activateWindow();
        }
    }
}

void UIVirtualBoxManager::sltCloseLogViewerWindow()
{
    QMap<QString, QIManagerDialog*>::iterator sendersIterator = m_logViewers.begin();

    /* Search for the sender of the signal within the m_logViewers map: */
    while (sendersIterator != m_logViewers.end() && sendersIterator.value() != sender())
        ++sendersIterator;
    /* Do nothing if we cannot find it with the map: */
    if (sendersIterator == m_logViewers.end())
        return;

    QIManagerDialog* pDialog = qobject_cast<QIManagerDialog*>(sendersIterator.value());
    if (!pDialog)
        return;

    /* First remove this log viewer dialog from the map. This should be
       done before closing the dialog which will incur a second call to
       this function and result in double delete!!!: */
    m_logViewers.erase(sendersIterator);
    UIVMLogViewerDialogFactory(CMachine()).cleanup(pDialog);
}

void UIVirtualBoxManager::sltShowMachineInFileManager()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if that item could be shown in file-browser: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Show VM in filebrowser: */
        UIDesktopServices::openInFileManager(pItem->machine().GetSettingsFilePath());
    }
}

void UIVirtualBoxManager::sltPerformCreateMachineShortcut()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    /* For each selected item: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Check if shortcuts could be created for this item: */
        if (!isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, QList<UIVirtualMachineItem*>() << pItem))
            continue;

        /* Create shortcut for this VM: */
        const CMachine &machine = pItem->machine();
        UIDesktopServices::createMachineShortcut(machine.GetSettingsFilePath(),
                                                 QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                 machine.GetName(), machine.GetId());
    }
}

void UIVirtualBoxManager::sltGroupCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
}

void UIVirtualBoxManager::sltMachineCloseMenuAboutToShow()
{
    /* Get selected items: */
    QList<UIVirtualMachineItem*> items = currentItems();
    AssertMsgReturnVoid(!items.isEmpty(), ("At least one item should be selected!\n"));

    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
}

void UIVirtualBoxManager::sltHandleToolsTypeSwitch()
{
    /* If Machine tool button is checked => go backward: */
    if (actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->isChecked())
        m_pSlidingWidget->moveBackward();

    else

    /* If Global tool button is checked => go forward: */
    if (actionPool()->action(UIActionIndexST_M_Tools_T_Global)->isChecked())
        m_pSlidingWidget->moveForward();

    /* Update action visibility: */
    updateActionsVisibility();

    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange(false /* update details? */, false /* update snapshots? */, false /* update the logviewer? */);
}

void UIVirtualBoxManager::sltHandleShowTabBarMachine()
{
    m_pActionTabBarGlobal->setVisible(false);
    m_pActionTabBarMachine->setVisible(true);
}

void UIVirtualBoxManager::sltHandleShowTabBarGlobal()
{
    m_pActionTabBarMachine->setVisible(false);
    m_pActionTabBarGlobal->setVisible(true);
}

void UIVirtualBoxManager::sltHandleToolOpenedMachine(ToolTypeMachine enmType)
{
    /* First, make sure corresponding tool set opened: */
    if (   !actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->isChecked()
        && actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->property("watch_child_activation").toBool())
        actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->setChecked(true);

    /* Open corresponding tool: */
    m_pPaneToolsMachine->openTool(enmType);
    /* If that was 'Details' => pass there current items: */
    if (   enmType == ToolTypeMachine_Details
        && m_pPaneToolsMachine->isToolOpened(ToolTypeMachine_Details))
        m_pPaneToolsMachine->setItems(currentItems());
    /* If that was 'Snapshot' or 'LogViewer' => pass there current or null machine: */
    if (enmType == ToolTypeMachine_Snapshots || enmType == ToolTypeMachine_LogViewer)
    {
        UIVirtualMachineItem *pItem = currentItem();
        m_pPaneToolsMachine->setMachine(pItem ? pItem->machine() : CMachine());
    }
}

void UIVirtualBoxManager::sltHandleToolOpenedGlobal(ToolTypeGlobal enmType)
{
    /* First, make sure corresponding tool set opened: */
    if (   !actionPool()->action(UIActionIndexST_M_Tools_T_Global)->isChecked()
        && actionPool()->action(UIActionIndexST_M_Tools_T_Global)->property("watch_child_activation").toBool())
        actionPool()->action(UIActionIndexST_M_Tools_T_Global)->setChecked(true);

    /* Open corresponding tool: */
    m_pPaneToolsGlobal->openTool(enmType);
}

void UIVirtualBoxManager::sltHandleToolClosedMachine(ToolTypeMachine enmType)
{
    /* Close corresponding tool: */
    m_pPaneToolsMachine->closeTool(enmType);
}

void UIVirtualBoxManager::sltHandleToolClosedGlobal(ToolTypeGlobal enmType)
{
    /* Close corresponding tool: */
    m_pPaneToolsGlobal->closeTool(enmType);
}

UIVirtualMachineItem *UIVirtualBoxManager::currentItem() const
{
    return m_pPaneChooser->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualBoxManager::currentItems() const
{
    return m_pPaneChooser->currentItems();
}

void UIVirtualBoxManager::prepare()
{
#ifdef VBOX_WS_X11
    /* Assign same name to both WM_CLASS name & class for now: */
    VBoxGlobal::setWMClass(this, "VirtualBox Manager", "VirtualBox Manager");
#endif

#ifdef VBOX_WS_MAC
    /* We have to make sure that we are getting the front most process: */
    ::darwinSetFrontMostProcess();
#endif /* VBOX_WS_MAC */

    /* Cache medium data early if necessary: */
    if (vboxGlobal().agressiveCaching())
        vboxGlobal().startMediumEnumeration();

    /* Prepare: */
    prepareIcon();
    prepareMenuBar();
    prepareStatusBar();
    prepareToolbar();
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    retranslateUi();

#ifdef VBOX_WS_MAC
    /* Enable unified toolbar: */
    m_pToolBar->enableMacToolbar();

    /* Beta label? */
    if (vboxGlobal().isBeta())
    {
        QPixmap betaLabel = ::betaLabel(QSize(100, 16));
        ::darwinLabelWindow(this, &betaLabel, true);
    }

    /* General event filter: */
    qApp->installEventFilter(this);
#endif /* VBOX_WS_MAC */

    /* Make sure current Chooser-pane index fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIVirtualBoxManager::prepareIcon()
{
    /* Prepare application icon.
     * On Win host it's built-in to the executable.
     * On Mac OS X the icon referenced in info.plist is used.
     * On X11 we will provide as much icons as we can. */
#if !(defined (VBOX_WS_WIN) || defined (VBOX_WS_MAC))
    QIcon icon(":/VirtualBox.svg");
    icon.addFile(":/VirtualBox_48px.png");
    icon.addFile(":/VirtualBox_64px.png");
    setWindowIcon(icon);
#endif /* !VBOX_WS_WIN && !VBOX_WS_MAC */
}

void UIVirtualBoxManager::prepareMenuBar()
{
#ifndef VBOX_WS_MAC
    /* Create menu-bar: */
    setMenuBar(new UIMenuBar);
#endif

    /* Create action-pool: */
    m_pActionPool = UIActionPool::create(UIActionPoolType_Selector);

    /* Prepare File-menu: */
    prepareMenuFile(actionPool()->action(UIActionIndexST_M_File)->menu());
    menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_File)->menu());

    /* Prepare 'Group' / 'Start or Show' menu: */
    prepareMenuGroupStartOrShow(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->menu());

    /* Prepare 'Machine' / 'Start or Show' menu: */
    prepareMenuMachineStartOrShow(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->menu());

    /* Prepare 'Group' / 'Close' menu: */
    prepareMenuGroupClose(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu());

    /* Prepare 'Machine' / 'Close' menu: */
    prepareMenuMachineClose(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu());

    /* Prepare Group-menu: */
    prepareMenuGroup(actionPool()->action(UIActionIndexST_M_Group)->menu());
    m_pGroupMenuAction = menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_Group)->menu());

    /* Prepare Machine-menu: */
    prepareMenuMachine(actionPool()->action(UIActionIndexST_M_Machine)->menu());
    m_pMachineMenuAction = menuBar()->addMenu(actionPool()->action(UIActionIndexST_M_Machine)->menu());

#ifdef VBOX_WS_MAC
    /* Prepare 'Window' menu: */
    UIWindowMenuManager::create();
    menuBar()->addMenu(gpWindowMenuManager->createMenu(this));
    gpWindowMenuManager->addWindow(this);
#endif /* VBOX_WS_MAC */

    /* Prepare Help-menu: */
    menuBar()->addMenu(actionPool()->action(UIActionIndex_Menu_Help)->menu());

    /* Setup menubar policy: */
    menuBar()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void UIVirtualBoxManager::prepareMenuFile(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

    /* The Application / 'File' menu contents is very different depending on host type. */

#ifdef VBOX_WS_MAC
    /* 'About' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_About));
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* 'Check for Updates' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_CheckForUpdates));
    /* 'Network Access Manager' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_NetworkAccessManager));
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    /* 'Reset Warnings' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_ResetWarnings));
    /* 'Preferences' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
    /* 'Close' action goes to Application menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_Close));

    /* 'Import Appliance' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
    /* 'Export Appliance' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /* 'Show Extra-data Manager' action goes to 'File' menu for Debug build: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    /* 'Show Virtual Medium Manager' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
    /* 'Show Host Network Manager' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager));

#else /* !VBOX_WS_MAC */

# ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
    addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
    addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
#  ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
#  endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
    addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
#  ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    addAction(actionPool()->action(UIActionIndex_M_Application_S_NetworkAccessManager));
    addAction(actionPool()->action(UIActionIndex_M_Application_S_CheckForUpdates));
#  endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    addAction(actionPool()->action(UIActionIndex_M_Application_S_ResetWarnings));
    addAction(actionPool()->action(UIActionIndexST_M_File_S_Close));
# endif /* VBOX_WS_X11 */

    /* 'Preferences' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
    /* Separator after 'Preferences' action of the 'File' menu: */
    pMenu->addSeparator();
    /* 'Import Appliance' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance));
    /* 'Export Appliance' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance));
    /* Separator after 'Export Appliance' action of the 'File' menu: */
    pMenu->addSeparator();
# ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /* 'Extra-data Manager' action goes to 'File' menu for Debug build: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager));
# endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    /* 'Show Virtual Medium Manager' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager));
    /* 'Show Host Network Manager' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager));
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /* 'Network Access Manager' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_NetworkAccessManager));
    /* 'Check for Updates' action goes to 'File' menu: */
    if (gEDataManager->applicationUpdateEnabled())
        pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_CheckForUpdates));
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
    /* 'Reset Warnings' action goes 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndex_M_Application_S_ResetWarnings));
    /* Separator after 'Reset Warnings' action of the 'File' menu: */
    pMenu->addSeparator();
    /* 'Close' action goes to 'File' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_File_S_Close));
#endif /* !VBOX_WS_MAC */
}

void UIVirtualBoxManager::prepareMenuGroup(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_New));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Add));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Rename));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Remove));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow));
    addAction(actionPool()->action(UIActionIndexST_M_Group_T_Pause));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Reset));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Discard));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Refresh));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut));
    addAction(actionPool()->action(UIActionIndexST_M_Group_S_Sort));
#endif /* VBOX_WS_X11 */

    /* Populate Machine-menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_New));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Add));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Rename));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Remove));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_T_Pause));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Reset));
    pMenu->addMenu(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Discard));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_S_Sort));

    /* Remember action list: */
    m_groupActions << actionPool()->action(UIActionIndexST_M_Group_S_New)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Add)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Rename)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Remove)
                   << actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)
                   << actionPool()->action(UIActionIndexST_M_Group_T_Pause)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Reset)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Discard)
                   << actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Refresh)
                   << actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)
                   << actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)
                   << actionPool()->action(UIActionIndexST_M_Group_S_Sort);
}

void UIVirtualBoxManager::prepareMenuMachine(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Add));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Clone));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Move));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Remove));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_T_Pause));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Reset));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent));
#endif /* VBOX_WS_X11 */

    /* Populate Machine-menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Add));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Clone));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Move));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Remove));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_T_Pause));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Reset));
    pMenu->addMenu(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu());
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Refresh));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut));
    pMenu->addSeparator();
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_SortParent));

    /* Remember action list: */
    m_machineActions << actionPool()->action(UIActionIndexST_M_Machine_S_New)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Add)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Settings)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Clone)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Move)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Remove)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)
                     << actionPool()->action(UIActionIndexST_M_Machine_T_Pause)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Reset)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Discard)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)
                     << actionPool()->action(UIActionIndexST_M_Machine_S_SortParent);
}

void UIVirtualBoxManager::prepareMenuGroupStartOrShow(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable));
#endif /* VBOX_WS_X11 */

    /* Populate 'Group' / 'Start or Show' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable));

    /* Remember action list: */
    m_groupActions << actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal)
                   << actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless)
                   << actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable);
}

void UIVirtualBoxManager::prepareMenuMachineStartOrShow(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable));
#endif /* VBOX_WS_X11 */

    /* Populate 'Machine' / 'Start or Show' menu: */
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable));

    /* Remember action list: */
    m_machineActions << actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable);
}

void UIVirtualBoxManager::prepareMenuGroupClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    // addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown));
    addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff));
#endif /* VBOX_WS_X11 */

    /* Populate 'Group' / 'Close' menu: */
    // pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff));

    /* Remember action list: */
    m_groupActions // << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach)
                   << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)
                   << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)
                   << actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff);
}

void UIVirtualBoxManager::prepareMenuMachineClose(QMenu *pMenu)
{
    /* Do not touch if filled already: */
    if (!pMenu->isEmpty())
        return;

#ifdef VBOX_WS_X11
    // WORKAROUND:
    // There is an issue under Ubuntu which uses special kind of QPA
    // plugin (appmenu-qt5) which redirects actions added to Qt menu-bar
    // directly to Ubuntu Application menu-bar. In that case action
    // shortcuts are not being handled by the Qt and that way ignored.
    // As a workaround we can add those actions into QMainWindow as well.
    // addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown));
    addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff));
#endif /* VBOX_WS_X11 */

    /* Populate 'Machine' / 'Close' menu: */
    // pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown));
    pMenu->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff));

    /* Remember action list: */
    m_machineActions // << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)
                     << actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff);
}

void UIVirtualBoxManager::prepareStatusBar()
{
    /* We are not using status-bar anymore: */
    statusBar()->setHidden(true);
}

void UIVirtualBoxManager::prepareToolbar()
{
    /* Create Main toolbar: */
    m_pToolBar = new UIToolBar(this);
    AssertPtrReturnVoid(m_pToolBar);
    {
        /* Configure toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        /* Add main actions block: */
        m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_New));
        m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Settings));
        m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_S_Discard));
        m_pToolBar->addAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow));
#ifdef VBOX_WS_MAC
        // WORKAROUND:
        // Actually Qt should do that itself but by some unknown reason it sometimes
        // forget to update toolbar after changing its actions on cocoa platform.
        connect(actionPool()->action(UIActionIndexST_M_Machine_S_New), &UIAction::changed,
                m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
        connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), &UIAction::changed,
                m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
        connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), &UIAction::changed,
                m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
        connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow), &UIAction::changed,
                m_pToolBar, static_cast<void(UIToolBar::*)(void)>(&UIToolBar::update));
#endif /* VBOX_WS_MAC */

        /* Create Machine tab-bar: */
        m_pTabBarMachine = new UITabBar;
        AssertPtrReturnVoid(m_pTabBarMachine);
        {
            /* Configure tab-bar: */
            const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
            const int iR = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin);
            m_pTabBarMachine->setContentsMargins(iL, 0, iR, 0);

            /* Add into toolbar: */
            m_pActionTabBarMachine = m_pToolBar->addWidget(m_pTabBarMachine);
        }

        /* Create Global tab-bar: */
        m_pTabBarGlobal = new UITabBar;
        AssertPtrReturnVoid(m_pTabBarGlobal);
        {
            /* Configure tab-bar: */
            const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
            const int iR = qApp->style()->pixelMetric(QStyle::PM_LayoutRightMargin);
            m_pTabBarGlobal->setContentsMargins(iL, 0, iR, 0);

            /* Add into toolbar: */
            m_pActionTabBarGlobal = m_pToolBar->addWidget(m_pTabBarGlobal);
        }

        /* Create Tools toolbar: */
        m_pToolbarTools = new UIToolbarTools(actionPool());
        if (m_pToolbarTools)
        {
            /* Configure toolbar: */
            m_pToolbarTools->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
            connect(m_pToolbarTools, &UIToolbarTools::sigShowTabBarMachine,
                    this, &UIVirtualBoxManager::sltHandleShowTabBarMachine);
            connect(m_pToolbarTools, &UIToolbarTools::sigShowTabBarGlobal,
                    this, &UIVirtualBoxManager::sltHandleShowTabBarGlobal);
            m_pToolbarTools->setTabBars(m_pTabBarMachine, m_pTabBarGlobal);

            /* Create exclusive action-group: */
            QActionGroup *pActionGroupTools = new QActionGroup(m_pToolbarTools);
            AssertPtrReturnVoid(pActionGroupTools);
            {
                /* Configure action-group: */
                pActionGroupTools->setExclusive(true);

                /* Add 'Tools' actions into action-group: */
                pActionGroupTools->addAction(actionPool()->action(UIActionIndexST_M_Tools_T_Machine));
                pActionGroupTools->addAction(actionPool()->action(UIActionIndexST_M_Tools_T_Global));
            }

            /* Add into toolbar: */
            m_pToolBar->addWidget(m_pToolbarTools);
        }

#ifdef VBOX_WS_MAC
        // WORKAROUND:
        // There is a bug in Qt Cocoa which result in showing a "more arrow" when
        // the necessary size of the toolbar is increased. Also for some languages
        // the with doesn't match if the text increase. So manually adjust the size
        // after changing the text.
        m_pToolBar->updateLayout();
#endif
    }
}

void UIVirtualBoxManager::prepareWidgets()
{
    /* Create central-widget: */
    QWidget *pWidget = new QWidget;
    AssertPtrReturnVoid(pWidget);
    {
        /* Configure central-widget: */
        setCentralWidget(pWidget);

        /* Create central-layout: */
        QVBoxLayout *pLayout = new QVBoxLayout(pWidget);
        AssertPtrReturnVoid(pLayout);
        {
            /* Configure layout: */
            pLayout->setSpacing(0);
            pLayout->setContentsMargins(0, 0, 0, 0);

#ifdef VBOX_WS_MAC
            /* Native toolbar on MAC: */
            addToolBar(m_pToolBar);
#else
            /* Add into layout: */
            pLayout->addWidget(m_pToolBar);
#endif

            /* Create sliding-widget: */
            m_pSlidingWidget = new UISlidingWidget;
            AssertPtrReturnVoid(m_pSlidingWidget);
            {
                /* Create splitter: */
                m_pSplitter = new QISplitter;
                AssertPtrReturnVoid(m_pSplitter);
                {
                    /* Configure splitter: */
#ifdef VBOX_WS_X11
                    m_pSplitter->setHandleType(QISplitter::Native);
#endif

                    /* Create Chooser-pane: */
                    m_pPaneChooser = new UIChooser(this);
                    AssertPtrReturnVoid(m_pPaneChooser);
                    {
                        /* Add into splitter: */
                        m_pSplitter->addWidget(m_pPaneChooser);
                    }

                    /* Create Machine Tools-pane: */
                    m_pPaneToolsMachine = new UIToolPaneMachine(actionPool());
                    AssertPtrReturnVoid(m_pPaneToolsMachine);
                    {
                        /* Add into splitter: */
                        m_pSplitter->addWidget(m_pPaneToolsMachine);
                    }

                    /* Adjust splitter colors according to main widgets it splits: */
                    m_pSplitter->configureColors(m_pPaneChooser->palette().color(QPalette::Active, QPalette::Window),
                                                 m_pPaneToolsMachine->palette().color(QPalette::Active, QPalette::Window));
                    /* Set the initial distribution. The right site is bigger. */
                    m_pSplitter->setStretchFactor(0, 2);
                    m_pSplitter->setStretchFactor(1, 3);
                }

                /* Create Global Tools-pane: */
                m_pPaneToolsGlobal = new UIToolPaneGlobal(actionPool());
                AssertPtrReturnVoid(m_pPaneToolsGlobal);

                /* Add left/right widgets into sliding widget: */
                m_pSlidingWidget->setWidgets(m_pSplitter, m_pPaneToolsGlobal);

                /* Add into layout: */
                pLayout->addWidget(m_pSlidingWidget);
            }
        }
    }

    /* Bring the VM list to the focus: */
    m_pPaneChooser->setFocus();
}

void UIVirtualBoxManager::prepareConnections()
{
#ifdef VBOX_WS_X11
    /* Desktop event handlers: */
    connect(gpDesktop, SIGNAL(sigHostScreenWorkAreaResized(int)), this, SLOT(sltHandleHostScreenAvailableAreaChange()));
#endif /* VBOX_WS_X11 */

    /* Medium enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationFinished()), this, SLOT(sltHandleMediumEnumerationFinish()));

    /* Menu-bar connections: */
    connect(menuBar(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltHandleContextMenuRequest(const QPoint&)));

    /* 'File' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowVirtualMediumManager), SIGNAL(triggered()), this, SLOT(sltOpenVirtualMediumManagerWindow()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowHostNetworkManager), SIGNAL(triggered()), this, SLOT(sltOpenHostNetworkManagerWindow()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_ImportAppliance), SIGNAL(triggered()), this, SLOT(sltOpenImportApplianceWizard()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_ExportAppliance), SIGNAL(triggered()), this, SLOT(sltOpenExportApplianceWizard()));
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    connect(actionPool()->action(UIActionIndexST_M_File_S_ShowExtraDataManager), SIGNAL(triggered()), this, SLOT(sltOpenExtraDataManagerWindow()));
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    connect(actionPool()->action(UIActionIndex_M_Application_S_Preferences), SIGNAL(triggered()), this, SLOT(sltOpenPreferencesDialog()));
    connect(actionPool()->action(UIActionIndexST_M_File_S_Close), SIGNAL(triggered()), this, SLOT(sltPerformExit()));

    /* 'Group' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Add), SIGNAL(triggered()), this, SLOT(sltOpenAddMachineDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow), SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Group_T_Pause), SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseOrResumeMachine(bool)));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Reset), SIGNAL(triggered()), this, SLOT(sltPerformResetMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_Discard), SIGNAL(triggered()), this, SLOT(sltPerformDiscardMachineState()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog), SIGNAL(triggered()), this, SLOT(sltOpenMachineLogDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager), SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut), SIGNAL(triggered()), this, SLOT(sltPerformCreateMachineShortcut()));

    /* 'Machine' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Add), SIGNAL(triggered()), this, SLOT(sltOpenAddMachineDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Settings), SIGNAL(triggered()), this, SLOT(sltOpenMachineSettingsDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Clone), SIGNAL(triggered()), this, SLOT(sltOpenCloneMachineWizard()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Move), SIGNAL(triggered()), this, SLOT(sltPerformMoveMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow), SIGNAL(triggered()), this, SLOT(sltPerformStartOrShowMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_T_Pause), SIGNAL(toggled(bool)), this, SLOT(sltPerformPauseOrResumeMachine(bool)));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Reset), SIGNAL(triggered()), this, SLOT(sltPerformResetMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_Discard), SIGNAL(triggered()), this, SLOT(sltPerformDiscardMachineState()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog), SIGNAL(triggered()), this, SLOT(sltOpenMachineLogDialog()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager), SIGNAL(triggered()), this, SLOT(sltShowMachineInFileManager()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut), SIGNAL(triggered()), this, SLOT(sltPerformCreateMachineShortcut()));

    /* 'Group/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineNormal()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineHeadless()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineDetachable()));

    /* 'Machine/Start or Show' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineNormal()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineHeadless()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable), SIGNAL(triggered()), this, SLOT(sltPerformStartMachineDetachable()));

    /* 'Group/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close)->menu(), SIGNAL(aboutToShow()), this, SLOT(sltGroupCloseMenuAboutToShow()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach), SIGNAL(triggered()), this, SLOT(sltPerformDetachMachineUI()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState), SIGNAL(triggered()), this, SLOT(sltPerformSaveMachineState()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown), SIGNAL(triggered()), this, SLOT(sltPerformShutdownMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff), SIGNAL(triggered()), this, SLOT(sltPerformPowerOffMachine()));

    /* 'Machine/Close' menu connections: */
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close)->menu(), SIGNAL(aboutToShow()), this, SLOT(sltMachineCloseMenuAboutToShow()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach), SIGNAL(triggered()), this, SLOT(sltPerformDetachMachineUI()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState), SIGNAL(triggered()), this, SLOT(sltPerformSaveMachineState()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown), SIGNAL(triggered()), this, SLOT(sltPerformShutdownMachine()));
    connect(actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff), SIGNAL(triggered()), this, SLOT(sltPerformPowerOffMachine()));

    /* 'Tools' actions connections: */
    connect(actionPool()->action(UIActionIndexST_M_Tools_T_Machine), &UIAction::toggled,
            this, &UIVirtualBoxManager::sltHandleToolsTypeSwitch);
    connect(actionPool()->action(UIActionIndexST_M_Tools_T_Global), &UIAction::toggled,
            this, &UIVirtualBoxManager::sltHandleToolsTypeSwitch);

    /* Graphics VM chooser connections: */
    connect(m_pPaneChooser, SIGNAL(sigSelectionChanged()), this, SLOT(sltHandleChooserPaneIndexChange()));
    connect(m_pPaneChooser, SIGNAL(sigSlidingStarted()), m_pPaneToolsMachine, SIGNAL(sigSlidingStarted()));
    connect(m_pPaneChooser, SIGNAL(sigToggleStarted()), m_pPaneToolsMachine, SIGNAL(sigToggleStarted()));
    connect(m_pPaneChooser, SIGNAL(sigToggleFinished()), m_pPaneToolsMachine, SIGNAL(sigToggleFinished()));
    connect(m_pPaneChooser, SIGNAL(sigGroupSavingStateChanged()), this, SLOT(sltHandleGroupSavingProgressChange()));

    /* Tool-bar connections: */
#ifndef VBOX_WS_MAC
    connect(m_pToolBar, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(sltHandleContextMenuRequest(const QPoint&)));
#else /* VBOX_WS_MAC */
    /* We want to receive right click notifications on the title bar, so register our own handler: */
    ::darwinRegisterForUnifiedToolbarContextMenuEvents(this);
#endif /* VBOX_WS_MAC */
    connect(m_pToolbarTools, &UIToolbarTools::sigToolOpenedMachine, this, &UIVirtualBoxManager::sltHandleToolOpenedMachine);
    connect(m_pToolbarTools, &UIToolbarTools::sigToolOpenedGlobal,  this, &UIVirtualBoxManager::sltHandleToolOpenedGlobal);
    connect(m_pToolbarTools, &UIToolbarTools::sigToolClosedMachine, this, &UIVirtualBoxManager::sltHandleToolClosedMachine);
    connect(m_pToolbarTools, &UIToolbarTools::sigToolClosedGlobal,  this, &UIVirtualBoxManager::sltHandleToolClosedGlobal);

    /* VM desktop connections: */
    connect(m_pPaneToolsMachine, SIGNAL(sigLinkClicked(const QString&, const QString&, const QString&)),
            this, SLOT(sltOpenMachineSettingsDialog(const QString&, const QString&, const QString&)));

    /* Global event handlers: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QString, KMachineState)), this, SLOT(sltHandleStateChange(QString)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QString, KSessionState)), this, SLOT(sltHandleStateChange(QString)));
}

void UIVirtualBoxManager::loadSettings()
{
    /* Restore window geometry: */
    {
        /* Load geometry: */
        m_geometry = gEDataManager->selectorWindowGeometry(this);

        /* Restore geometry: */
        LogRel2(("GUI: UIVirtualBoxManager: Restoring geometry to: Origin=%dx%d, Size=%dx%d\n",
                 m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));
        restoreGeometry();
    }

    /* Restore splitter handle position: */
    {
        /* Read splitter hints: */
        QList<int> sizes = gEDataManager->selectorWindowSplitterHints();
        /* If both hints are zero, we have the 'default' case: */
        if (sizes[0] == 0 && sizes[1] == 0)
        {
            /* Propose some 'default' based on current dialog width: */
            sizes[0] = (int)(width() * .9 * (1.0 / 3));
            sizes[1] = (int)(width() * .9 * (2.0 / 3));
        }
        /* Pass hints to the splitter: */
        m_pSplitter->setSizes(sizes);
    }

    /* Restore toolbar and statusbar functionality: */
    {
#ifdef VBOX_WS_MAC
        // WORKAROUND:
        // There is an issue in Qt5 main-window toolbar implementation:
        // if you are hiding it before it's shown for the first time,
        // there is an ugly empty container appears instead, so we
        // have to hide toolbar asynchronously to avoid that.
        if (!gEDataManager->selectorWindowToolBarVisible())
            QMetaObject::invokeMethod(m_pToolBar, "hide", Qt::QueuedConnection);
#else
        m_pToolBar->setHidden(!gEDataManager->selectorWindowToolBarVisible());
#endif
        m_pToolBar->setToolButtonStyle(gEDataManager->selectorWindowToolBarTextVisible()
                                       ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
        m_pToolbarTools->setToolButtonStyle(gEDataManager->selectorWindowToolBarTextVisible()
                                            ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly);
    }

    /* Restore toolbar Machine/Global tools orders:  */
    {
        m_orderMachine = gEDataManager->selectorWindowToolsOrderMachine();
        m_orderGlobal = gEDataManager->selectorWindowToolsOrderGlobal();

        /* We can restore previously opened Global tools right here: */
        QMap<ToolTypeGlobal, QAction*> mapActionsGlobal;
        mapActionsGlobal[ToolTypeGlobal_VirtualMedia] = actionPool()->action(UIActionIndexST_M_Tools_M_Global_S_VirtualMediaManager);
        mapActionsGlobal[ToolTypeGlobal_HostNetwork] = actionPool()->action(UIActionIndexST_M_Tools_M_Global_S_HostNetworkManager);
        for (int i = m_orderGlobal.size() - 1; i >= 0; --i)
            if (m_orderGlobal.at(i) != ToolTypeGlobal_Invalid)
                mapActionsGlobal.value(m_orderGlobal.at(i))->trigger();
        /* Make sure further action triggering cause tool type switch as well: */
        actionPool()->action(UIActionIndexST_M_Tools_T_Global)->setProperty("watch_child_activation", true);

        /* But we can't restore previously opened Machine tools here,
         * see the reason in corresponding async sltHandlePolishEvent slot. */
    }
}

void UIVirtualBoxManager::saveSettings()
{
    /* Save toolbar Machine/Global tools orders: */
    {
        gEDataManager->setSelectorWindowToolsOrderMachine(m_pToolbarTools->tabOrderMachine());
        gEDataManager->setSelectorWindowToolsOrderGlobal(m_pToolbarTools->tabOrderGlobal());
    }

    /* Save toolbar and statusbar visibility: */
    {
        gEDataManager->setSelectorWindowToolBarVisible(!m_pToolBar->isHidden());
        gEDataManager->setSelectorWindowToolBarTextVisible(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
    }

    /* Save splitter handle position: */
    {
        gEDataManager->setSelectorWindowSplitterHints(m_pSplitter->sizes());
    }

    /* Save window geometry: */
    {
#ifdef VBOX_WS_MAC
        gEDataManager->setSelectorWindowGeometry(m_geometry, ::darwinIsWindowMaximized(this));
#else /* VBOX_WS_MAC */
        gEDataManager->setSelectorWindowGeometry(m_geometry, isMaximized());
#endif /* !VBOX_WS_MAC */
        LogRel2(("GUI: UIVirtualBoxManager: Geometry saved as: Origin=%dx%d, Size=%dx%d\n",
                 m_geometry.x(), m_geometry.y(), m_geometry.width(), m_geometry.height()));
    }
}

void UIVirtualBoxManager::cleanupConnections()
{
#ifdef VBOX_WS_MAC
    /* Tool-bar connections: */
    ::darwinUnregisterForUnifiedToolbarContextMenuEvents(this);
#endif /* VBOX_WS_MAC */
}

void UIVirtualBoxManager::cleanupMenuBar()
{
#ifdef VBOX_WS_MAC
    /* Cleanup 'Window' menu: */
    UIWindowMenuManager::destroy();
#endif /* VBOX_WS_MAC */

    /* Destroy action-pool: */
    UIActionPool::destroy(m_pActionPool);
}

void UIVirtualBoxManager::cleanup()
{
    /* Close the sub-dialogs first: */
    sltCloseVirtualMediumManagerWindow();
    sltCloseHostNetworkManagerWindow();

    /* Save settings: */
    saveSettings();

    /* Cleanup: */
    cleanupConnections();
    cleanupMenuBar();
}

void UIVirtualBoxManager::performStartOrShowVirtualMachines(const QList<UIVirtualMachineItem*> &items, VBoxGlobal::LaunchMode enmLaunchMode)
{
    /* Do nothing while group saving is in progress: */
    if (m_pPaneChooser->isGroupSavingInProgress())
        return;

    /* Compose the list of startable items: */
    QStringList startableMachineNames;
    QList<UIVirtualMachineItem*> startableItems;
    foreach (UIVirtualMachineItem *pItem, items)
        if (isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem))
        {
            startableItems << pItem;
            startableMachineNames << pItem->name();
        }

    /* Initially we have start auto-confirmed: */
    bool fStartConfirmed = true;
    /* But if we have more than one item to start =>
     * We should still ask user for a confirmation: */
    if (startableItems.size() > 1)
        fStartConfirmed = msgCenter().confirmStartMultipleMachines(startableMachineNames.join(", "));

    /* For every item => check if it could be launched: */
    foreach (UIVirtualMachineItem *pItem, items)
        if (   isAtLeastOneItemCanBeShown(QList<UIVirtualMachineItem*>() << pItem)
            || (   isAtLeastOneItemCanBeStarted(QList<UIVirtualMachineItem*>() << pItem)
                && fStartConfirmed))
        {
            /* Fetch item launch mode: */
            VBoxGlobal::LaunchMode enmItemLaunchMode = enmLaunchMode;
            if (enmItemLaunchMode == VBoxGlobal::LaunchMode_Invalid)
                enmItemLaunchMode = UIVirtualMachineItem::isItemRunningHeadless(pItem)         ? VBoxGlobal::LaunchMode_Separate :
                                    qApp->keyboardModifiers() == Qt::ShiftModifier ? VBoxGlobal::LaunchMode_Headless :
                                                                                     VBoxGlobal::LaunchMode_Default;

            /* Launch current VM: */
            CMachine machine = pItem->machine();
            vboxGlobal().launchMachine(machine, enmItemLaunchMode);
        }
}

void UIVirtualBoxManager::updateActionsVisibility()
{
    /* Determine whether Machine or Group menu should be shown at all: */
    const bool fMachineOrGroupMenuShown = actionPool()->action(UIActionIndexST_M_Tools_T_Machine)->isChecked();
    const bool fMachineMenuShown = !m_pPaneChooser->isSingleGroupSelected();
    m_pMachineMenuAction->setVisible(fMachineOrGroupMenuShown && fMachineMenuShown);
    m_pGroupMenuAction->setVisible(fMachineOrGroupMenuShown && !fMachineMenuShown);

    /* Hide action shortcuts: */
    if (!fMachineMenuShown)
        foreach (UIAction *pAction, m_machineActions)
            pAction->hideShortcut();
    if (fMachineMenuShown)
        foreach (UIAction *pAction, m_groupActions)
            pAction->hideShortcut();

    /* Update actions visibility: */
    foreach (UIAction *pAction, m_machineActions)
        pAction->setVisible(fMachineOrGroupMenuShown);

    /* Show what should be shown: */
    if (fMachineMenuShown)
        foreach (UIAction *pAction, m_machineActions)
            pAction->showShortcut();
    if (!fMachineMenuShown)
        foreach (UIAction *pAction, m_groupActions)
            pAction->showShortcut();
}

void UIVirtualBoxManager::updateActionsAppearance()
{
    /* Get current items: */
    QList<UIVirtualMachineItem*> items = currentItems();

    /* Enable/disable group actions: */
    actionPool()->action(UIActionIndexST_M_Group_S_Rename)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Rename, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Group_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Group_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Group_S_Sort)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_S_Sort, items));

    /* Enable/disable machine actions: */
    actionPool()->action(UIActionIndexST_M_Machine_S_Settings)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Settings, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Clone)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Clone, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Move)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Move, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Remove)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Remove, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_AddGroup)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_AddGroup, items));
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_T_Pause, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Reset)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Reset, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Discard)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Discard, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowLogDialog)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowLogDialog, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_Refresh)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_Refresh, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_ShowInFileManager)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_ShowInFileManager, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_CreateShortcut)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_CreateShortcut, items));
    actionPool()->action(UIActionIndexST_M_Machine_S_SortParent)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_S_SortParent, items));

    /* Enable/disable group-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable machine-start-or-show actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable, items));

    /* Enable/disable group-close actions: */
    actionPool()->action(UIActionIndexST_M_Group_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Group_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Group_M_Close_S_PowerOff, items));

    /* Enable/disable machine-close actions: */
    actionPool()->action(UIActionIndexST_M_Machine_M_Close)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Detach)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Detach, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_SaveState)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_SaveState, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_Shutdown)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_Shutdown, items));
    actionPool()->action(UIActionIndexST_M_Machine_M_Close_S_PowerOff)->setEnabled(isActionEnabled(UIActionIndexST_M_Machine_M_Close_S_PowerOff, items));

    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();

    /* Start/Show action is deremined by 1st item: */
    if (pItem && pItem->accessible())
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(UIVirtualMachineItem::isItemPoweredOff(pItem) ? 0 : 1);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(UIVirtualMachineItem::isItemPoweredOff(pItem) ? 0 : 1);
        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
        if (pButton)
            pButton->setPopupMode(UIVirtualMachineItem::isItemPoweredOff(pItem) ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }
    else
    {
        actionPool()->action(UIActionIndexST_M_Group_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)->toActionPolymorphicMenu()->setState(0);
        QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexST_M_Machine_M_StartOrShow)));
        if (pButton)
            pButton->setPopupMode(UIVirtualMachineItem::isItemPoweredOff(pItem) ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
    }

    /* Pause/Resume action is deremined by 1st started item: */
    UIVirtualMachineItem *pFirstStartedAction = 0;
    foreach (UIVirtualMachineItem *pSelectedItem, items)
        if (UIVirtualMachineItem::isItemStarted(pSelectedItem))
        {
            pFirstStartedAction = pSelectedItem;
            break;
        }
    /* Update the group Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->setChecked(pFirstStartedAction && UIVirtualMachineItem::isItemPaused(pFirstStartedAction));
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Group_T_Pause)->blockSignals(false);
    /* Update the machine Pause/Resume action appearance: */
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(true);
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->setChecked(pFirstStartedAction && UIVirtualMachineItem::isItemPaused(pFirstStartedAction));
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->retranslateUi();
    actionPool()->action(UIActionIndexST_M_Machine_T_Pause)->blockSignals(false);

    /* Enable/disable tools actions: */
    actionPool()->action(UIActionIndexST_M_Tools_M_Machine)->setEnabled(isActionEnabled(UIActionIndexST_M_Tools_M_Machine, items));
    actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Details)->setEnabled(isActionEnabled(UIActionIndexST_M_Tools_M_Machine_S_Details, items));
    actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_Snapshots)->setEnabled(isActionEnabled(UIActionIndexST_M_Tools_M_Machine_S_Snapshots, items));
    actionPool()->action(UIActionIndexST_M_Tools_M_Machine_S_LogViewer)->setEnabled(isActionEnabled(UIActionIndexST_M_Tools_M_Machine_S_LogViewer, items));
}

bool UIVirtualBoxManager::isActionEnabled(int iActionIndex, const QList<UIVirtualMachineItem*> &items)
{
    /* No actions enabled for empty item list: */
    if (items.isEmpty())
        return false;

    /* Get first item: */
    UIVirtualMachineItem *pItem = items.first();

    /* For known action types: */
    switch (iActionIndex)
    {
        case UIActionIndexST_M_Group_S_Rename:
        case UIActionIndexST_M_Group_S_Remove:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_S_Sort:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   m_pPaneChooser->isSingleGroupSelected();
        }
        case UIActionIndexST_M_Machine_S_Settings:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   pItem->configurationAccessLevel() != ConfigurationAccessLevel_Null;
        }
        case UIActionIndexST_M_Machine_S_Clone:
        case UIActionIndexST_M_Machine_S_Move:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   items.size() == 1 &&
                   UIVirtualMachineItem::isItemEditable(pItem);
        }
        case UIActionIndexST_M_Machine_S_Remove:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemRemovable(items);
        }
        case UIActionIndexST_M_Machine_S_AddGroup:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   !m_pPaneChooser->isAllItemsOfOneGroupSelected() &&
                   isItemsPoweredOff(items);
        }
        case UIActionIndexST_M_Group_M_StartOrShow:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartNormal:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Group_M_StartOrShow_S_StartDetachable:
        case UIActionIndexST_M_Machine_M_StartOrShow:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartNormal:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartHeadless:
        case UIActionIndexST_M_Machine_M_StartOrShow_S_StartDetachable:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemCanBeStartedOrShown(items);
        }
        case UIActionIndexST_M_Group_S_Discard:
        case UIActionIndexST_M_Machine_S_Discard:
        {
            return !m_pPaneChooser->isGroupSavingInProgress() &&
                   isAtLeastOneItemDiscardable(items);
        }
        case UIActionIndexST_M_Group_S_ShowLogDialog:
        case UIActionIndexST_M_Machine_S_ShowLogDialog:
        {
            return isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Group_T_Pause:
        case UIActionIndexST_M_Machine_T_Pause:
        {
            return isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_S_Reset:
        case UIActionIndexST_M_Machine_S_Reset:
        {
            return isAtLeastOneItemRunning(items);
        }
        case UIActionIndexST_M_Group_S_Refresh:
        case UIActionIndexST_M_Machine_S_Refresh:
        {
            return isAtLeastOneItemInaccessible(items);
        }
        case UIActionIndexST_M_Group_S_ShowInFileManager:
        case UIActionIndexST_M_Machine_S_ShowInFileManager:
        {
            return isAtLeastOneItemAccessible(items);
        }
        case UIActionIndexST_M_Machine_S_SortParent:
        {
            return !m_pPaneChooser->isGroupSavingInProgress();
        }
        case UIActionIndexST_M_Group_S_CreateShortcut:
        case UIActionIndexST_M_Machine_S_CreateShortcut:
        {
            return isAtLeastOneItemSupportsShortcuts(items);
        }
        case UIActionIndexST_M_Group_M_Close:
        case UIActionIndexST_M_Machine_M_Close:
        {
            return isAtLeastOneItemStarted(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Detach:
        case UIActionIndexST_M_Machine_M_Close_S_Detach:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_SaveState:
        case UIActionIndexST_M_Machine_M_Close_S_SaveState:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Group_M_Close_S_Shutdown:
        case UIActionIndexST_M_Machine_M_Close_S_Shutdown:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items) &&
                   isAtLeastOneItemAbleToShutdown(items);
        }
        case UIActionIndexST_M_Group_M_Close_S_PowerOff:
        case UIActionIndexST_M_Machine_M_Close_S_PowerOff:
        {
            return isActionEnabled(UIActionIndexST_M_Machine_M_Close, items);
        }
        case UIActionIndexST_M_Tools_M_Machine:
        case UIActionIndexST_M_Tools_M_Machine_S_Details:
        case UIActionIndexST_M_Tools_M_Machine_S_Snapshots:
        case UIActionIndexST_M_Tools_M_Machine_S_LogViewer:
        {
            return pItem->accessible();
        }
        default:
            break;
    }

    /* Unknown actions are disabled: */
    return false;
}

/* static */
bool UIVirtualBoxManager::isItemsPoweredOff(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!UIVirtualMachineItem::isItemPoweredOff(pItem))
            return false;
    return true;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemAbleToShutdown(const QList<UIVirtualMachineItem*> &items)
{
    /* Enumerate all the passed items: */
    foreach (UIVirtualMachineItem *pItem, items)
    {
        /* Skip non-running machines: */
        if (!UIVirtualMachineItem::isItemRunning(pItem))
            continue;
        /* Skip session failures: */
        CSession session = vboxGlobal().openExistingSession(pItem->id());
        if (session.isNull())
            continue;
        /* Skip console failures: */
        CConsole console = session.GetConsole();
        if (console.isNull())
        {
            /* Do not forget to release machine: */
            session.UnlockMachine();
            continue;
        }
        /* Is the guest entered ACPI mode? */
        bool fGuestEnteredACPIMode = console.GetGuestEnteredACPIMode();
        /* Do not forget to release machine: */
        session.UnlockMachine();
        /* True if the guest entered ACPI mode: */
        if (fGuestEnteredACPIMode)
            return true;
    }
    /* False by default: */
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemSupportsShortcuts(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->accessible()
#ifdef VBOX_WS_MAC
            /* On Mac OS X this are real alias files, which don't work with the old legacy xml files. */
            && pItem->settingsFile().endsWith(".vbox", Qt::CaseInsensitive)
#endif /* VBOX_WS_MAC */
            )
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemAccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemInaccessible(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->accessible())
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemRemovable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (!pItem->accessible() || UIVirtualMachineItem::isItemEditable(pItem))
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (UIVirtualMachineItem::isItemPoweredOff(pItem) && UIVirtualMachineItem::isItemEditable(pItem))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if (UIVirtualMachineItem::isItemStarted(pItem) && (pItem->canSwitchTo() || UIVirtualMachineItem::isItemRunningHeadless(pItem)))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemCanBeStartedOrShown(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
    {
        if ((UIVirtualMachineItem::isItemPoweredOff(pItem) && UIVirtualMachineItem::isItemEditable(pItem)) ||
            (UIVirtualMachineItem::isItemStarted(pItem) && (pItem->canSwitchTo() || UIVirtualMachineItem::isItemRunningHeadless(pItem))))
            return true;
    }
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemDiscardable(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (UIVirtualMachineItem::isItemSaved(pItem) && UIVirtualMachineItem::isItemEditable(pItem))
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemStarted(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (UIVirtualMachineItem::isItemStarted(pItem))
            return true;
    return false;
}

/* static */
bool UIVirtualBoxManager::isAtLeastOneItemRunning(const QList<UIVirtualMachineItem*> &items)
{
    foreach (UIVirtualMachineItem *pItem, items)
        if (UIVirtualMachineItem::isItemRunning(pItem))
            return true;
    return false;
}
