/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxManagerAdvancedWidget class implementation.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QApplication>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIActionPoolManager.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIGlobalToolsManagerWidget.h"
#include "UIMachineToolsWidget.h"
#include "UINotificationCenter.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"
#include "UIVirtualBoxManager.h"
#include "UIVirtualBoxManagerAdvancedWidget.h"
#if defined(VBOX_WS_MAC) && (defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32))
# include "UIGlobalSession.h"
# include "UIIconPool.h"
# include "UIVersion.h"
#endif /* VBOX_WS_MAC && (RT_ARCH_ARM64 || RT_ARCH_ARM32) */

/* COM includes: */
#if defined(VBOX_WS_MAC) && (defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32))
# include "CSystemProperties.h"
#endif /* VBOX_WS_MAC && (RT_ARCH_ARM64 || RT_ARCH_ARM32) */


UIVirtualBoxManagerAdvancedWidget::UIVirtualBoxManagerAdvancedWidget(UIVirtualBoxManager *pParent)
    : m_pActionPool(pParent->actionPool())
    , m_pToolBar(0)
    , m_pGlobalToolManager(0)
{
    prepare();
}

UIVirtualBoxManagerAdvancedWidget::~UIVirtualBoxManagerAdvancedWidget()
{
    cleanup();
}

void UIVirtualBoxManagerAdvancedWidget::updateToolBarMenuButtons(bool fSeparateMenuSection)
{
    QAction *pAction = actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow);
    AssertPtrReturnVoid(pAction);
    QToolButton *pButton = qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(pAction));
    if (pButton)
        pButton->setPopupMode(fSeparateMenuSection ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
}

UIVirtualMachineItem *UIVirtualBoxManagerAdvancedWidget::currentItem() const
{
    AssertPtrReturn(chooser(), 0);
    return chooser()->currentItem();
}

QList<UIVirtualMachineItem*> UIVirtualBoxManagerAdvancedWidget::currentItems() const
{
    AssertPtrReturn(chooser(), QList<UIVirtualMachineItem*>());
    return chooser()->currentItems();
}

bool UIVirtualBoxManagerAdvancedWidget::isGroupItemSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isGroupItemSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isMachineItemSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isMachineItemSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isLocalMachineItemSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isLocalMachineItemSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isCloudMachineItemSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isCloudMachineItemSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isSingleGroupSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isSingleGroupSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isSingleLocalGroupSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isSingleLocalGroupSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isSingleCloudProviderGroupSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isSingleCloudProviderGroupSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isSingleCloudProfileGroupSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isSingleCloudProfileGroupSelected();
}

bool UIVirtualBoxManagerAdvancedWidget::isAllItemsOfOneGroupSelected() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isAllItemsOfOneGroupSelected();
}

QString UIVirtualBoxManagerAdvancedWidget::fullGroupName() const
{
    AssertPtrReturn(chooser(), QString());
    return chooser()->fullGroupName();
}

bool UIVirtualBoxManagerAdvancedWidget::isGroupSavingInProgress() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isGroupSavingInProgress();
}

bool UIVirtualBoxManagerAdvancedWidget::isCloudProfileUpdateInProgress() const
{
    AssertPtrReturn(chooser(), false);
    return chooser()->isCloudProfileUpdateInProgress();
}

void UIVirtualBoxManagerAdvancedWidget::openGroupNameEditor()
{
    AssertPtrReturnVoid(chooser());
    chooser()->openGroupNameEditor();
}

void UIVirtualBoxManagerAdvancedWidget::disbandGroup()
{
    AssertPtrReturnVoid(chooser());
    chooser()->disbandGroup();
}

void UIVirtualBoxManagerAdvancedWidget::removeMachine()
{
    AssertPtrReturnVoid(chooser());
    chooser()->removeMachine();
}

void UIVirtualBoxManagerAdvancedWidget::moveMachineToGroup(const QString &strName /* = QString() */)
{
    AssertPtrReturnVoid(chooser());
    chooser()->moveMachineToGroup(strName);
}

QStringList UIVirtualBoxManagerAdvancedWidget::possibleGroupsForMachineToMove(const QUuid &uId)
{
    AssertPtrReturn(chooser(), QStringList());
    return chooser()->possibleGroupsForMachineToMove(uId);
}

QStringList UIVirtualBoxManagerAdvancedWidget::possibleGroupsForGroupToMove(const QString &strFullName)
{
    AssertPtrReturn(chooser(), QStringList());
    return chooser()->possibleGroupsForGroupToMove(strFullName);
}

void UIVirtualBoxManagerAdvancedWidget::refreshMachine()
{
    AssertPtrReturnVoid(chooser());
    chooser()->refreshMachine();
}

void UIVirtualBoxManagerAdvancedWidget::sortGroup()
{
    AssertPtrReturnVoid(chooser());
    chooser()->sortGroup();
}

void UIVirtualBoxManagerAdvancedWidget::setMachineSearchWidgetVisibility(bool fVisible)
{
    AssertPtrReturnVoid(chooser());
    chooser()->setMachineSearchWidgetVisibility(fVisible);
}

void UIVirtualBoxManagerAdvancedWidget::setToolsTypeGlobal(UIToolType enmType, bool)
{
    AssertPtrReturnVoid(globalToolManager());
    globalToolManager()->setMenuToolType(enmType);
}

UIToolType UIVirtualBoxManagerAdvancedWidget::toolsTypeGlobal() const
{
    AssertPtrReturn(globalToolManager(), UIToolType_Invalid);
    return globalToolManager()->menuToolType();
}

void UIVirtualBoxManagerAdvancedWidget::setToolsTypeMachine(UIToolType enmType)
{
    AssertPtrReturnVoid(machineToolsWidget());
    machineToolsWidget()->setMenuToolType(enmType);
}

UIToolType UIVirtualBoxManagerAdvancedWidget::toolsTypeMachine() const
{
    AssertPtrReturn(machineToolsWidget(), UIToolType_Invalid);
    return machineToolsWidget()->menuToolType();
}

UIToolType UIVirtualBoxManagerAdvancedWidget::currentGlobalTool() const
{
    AssertPtrReturn(globalToolManager(), UIToolType_Invalid);
    return globalToolManager()->toolType();
}

UIToolType UIVirtualBoxManagerAdvancedWidget::currentMachineTool() const
{
    AssertPtrReturn(machineToolsWidget(), UIToolType_Invalid);
    return machineToolsWidget()->toolType();
}

bool UIVirtualBoxManagerAdvancedWidget::isGlobalToolOpened(UIToolType enmType) const
{
    AssertPtrReturn(globalToolManager(), false);
    return globalToolManager()->isToolOpened(enmType);
}

bool UIVirtualBoxManagerAdvancedWidget::isMachineToolOpened(UIToolType enmType) const
{
    AssertPtrReturn(machineToolsWidget(), false);
    return machineToolsWidget()->isToolOpened(enmType);
}

void UIVirtualBoxManagerAdvancedWidget::closeGlobalTool(UIToolType enmType)
{
    AssertPtrReturnVoid(globalToolManager());
    globalToolManager()->closeTool(enmType);
}

void UIVirtualBoxManagerAdvancedWidget::closeMachineTool(UIToolType enmType)
{
    AssertPtrReturnVoid(machineToolsWidget());
    machineToolsWidget()->closeTool(enmType);
}

bool UIVirtualBoxManagerAdvancedWidget::isCurrentStateItemSelected() const
{
    AssertPtrReturn(machineToolPane(), false);
    return machineToolPane()->isCurrentStateItemSelected();
}

QUuid UIVirtualBoxManagerAdvancedWidget::currentSnapshotId()
{
    AssertPtrReturn(machineToolPane(), QUuid());
    return machineToolPane()->currentSnapshotId();
}

QString UIVirtualBoxManagerAdvancedWidget::currentHelpKeyword() const
{
    AssertPtrReturn(globalToolManager(), QString());
    return globalToolManager()->currentHelpKeyword();
}

void UIVirtualBoxManagerAdvancedWidget::sltHandleToolBarContextMenuRequest(const QPoint &position)
{
    /* Populate toolbar actions: */
    QList<QAction*> actions;
    /* Add 'Show Toolbar Text' action: */
    QAction *pShowToolBarText = new QAction(UIVirtualBoxManager::tr("Show Toolbar Text"), 0);
    if (pShowToolBarText)
    {
        pShowToolBarText->setCheckable(true);
        pShowToolBarText->setChecked(m_pToolBar->toolButtonStyle() == Qt::ToolButtonTextUnderIcon);
        actions << pShowToolBarText;
    }

    /* Prepare the menu position: */
    QPoint globalPosition = position;
    QWidget *pSender = qobject_cast<QWidget*>(sender());
    if (pSender)
        globalPosition = pSender->mapToGlobal(position);

    /* Execute the menu: */
    QAction *pResult = QMenu::exec(actions, globalPosition);

    /* Handle the menu execution result: */
    if (pResult == pShowToolBarText)
    {
        m_pToolBar->setUseTextLabels(pResult->isChecked());
        gEDataManager->setSelectorWindowToolBarTextVisible(pResult->isChecked());
    }
}

void UIVirtualBoxManagerAdvancedWidget::sltHandleCommitData()
{
    cleanupConnections();
}

void UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar()
{
    /* Update toolbar to show/hide corresponding actions: */
    updateToolbar();
}

void UIVirtualBoxManagerAdvancedWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();
}

void UIVirtualBoxManagerAdvancedWidget::prepareWidgets()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if(pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);

        /* Create Global Tool Manager: */
        m_pGlobalToolManager = new UIGlobalToolsManagerWidget(this, actionPool());
        if (globalToolManager())
        {
            /* Add into layout: */
            pLayout->addWidget(globalToolManager());
        }

        /* Create Main toolbar: */
        m_pToolBar = new QIToolBar(this);
        if (m_pToolBar)
        {
            /* Configure toolbar: */
            const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
            m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
            m_pToolBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            m_pToolBar->setContextMenuPolicy(Qt::CustomContextMenu);
            m_pToolBar->setUseTextLabels(true);

#if defined(VBOX_WS_MAC) && (defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32))
            /* Check whether we should show Dev Preview tag: */
            bool fShowDevPreviewTag = false;
            const CVirtualBox comVBox = gpGlobalSession->virtualBox();
            if (comVBox.isNotNull())
            {
                const CSystemProperties comSystemProps = comVBox.GetSystemProperties();
                if (comVBox.isOk() && comSystemProps.isNotNull())
                    fShowDevPreviewTag =
                        comSystemProps.GetSupportedPlatformArchitectures().contains(KPlatformArchitecture_x86);
            }
            /* Enable Dev Preview tag: */
            if (fShowDevPreviewTag)
            {
                m_pToolBar->emulateMacToolbar();
                m_pToolBar->enableBranding(UIIconPool::iconSet(":/explosion_hazard_32px.png"),
                                           "Dev Preview", // do we need to make it NLS?
                                           QColor(246, 179, 0),
                                           74 /* width of BETA label */);
            }
#endif /* VBOX_WS_MAC && (RT_ARCH_ARM64 || RT_ARCH_ARM32) */

            /* Add toolbar into layout: */
            m_pGlobalToolManager->addToolBar(m_pToolBar);
        }
    }

    /* Create notification-center: */
    UINotificationCenter::create(this);

    /* Update toolbar finally: */
    updateToolbar();

    /* Bring the VM list to the focus: */
    chooser()->setFocus();
}

void UIVirtualBoxManagerAdvancedWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIVirtualBoxManagerAdvancedWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);

    /* Tool-bar connections: */
    connect(m_pToolBar, &QIToolBar::customContextMenuRequested,
            this, &UIVirtualBoxManagerAdvancedWidget::sltHandleToolBarContextMenuRequest);

    /* Global Tool Manager connections: */
    connect(globalToolManager(), &UIGlobalToolsManagerWidget::sigToolTypeChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    connect(globalToolManager(), &UIGlobalToolsManagerWidget::sigToolTypeChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sigToolTypeChangeGlobal);
    /* Global Tool Pane connections: */
    connect(globalToolPane(), &UIToolPaneGlobal::sigCreateMedium,
            this, &UIVirtualBoxManagerAdvancedWidget::sigCreateMedium);
    connect(globalToolPane(), &UIToolPaneGlobal::sigCopyMedium,
            this, &UIVirtualBoxManagerAdvancedWidget::sigCopyMedium);

    /* Machine Tool Manager connections: */
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigToolTypeChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigToolTypeChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sigToolTypeChangeMachine);
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigChooserPaneIndexChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sigChooserPaneIndexChange);
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigChooserPaneSelectionChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigCloudMachineStateChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sigCloudMachineStateChange);
    /* Machine Tool Pane connections: */
    connect(machineToolPane(), &UIToolPaneMachine::sigLinkClicked,
            this, &UIVirtualBoxManagerAdvancedWidget::sigMachineSettingsLinkClicked);
    connect(machineToolPane(), &UIToolPaneMachine::sigCurrentSnapshotItemChange,
            this, &UIVirtualBoxManagerAdvancedWidget::sigCurrentSnapshotItemChange);
    connect(machineToolPane(), &UIToolPaneMachine::sigDetachToolPane,
            this, &UIVirtualBoxManagerAdvancedWidget::sigDetachToolPane);

    /* Chooser-pane connections: */
    connect(chooser(), &UIChooser::sigGroupSavingStateChanged,
            this, &UIVirtualBoxManagerAdvancedWidget::sigGroupSavingStateChanged);
    connect(chooser(), &UIChooser::sigCloudUpdateStateChanged,
            this, &UIVirtualBoxManagerAdvancedWidget::sigCloudUpdateStateChanged);
    connect(chooser(), &UIChooser::sigStartOrShowRequest,
            this, &UIVirtualBoxManagerAdvancedWidget::sigStartOrShowRequest);
    connect(chooser(), &UIChooser::sigMachineSearchWidgetVisibilityChanged,
            this, &UIVirtualBoxManagerAdvancedWidget::sigMachineSearchWidgetVisibilityChanged);
}

void UIVirtualBoxManagerAdvancedWidget::loadSettings()
{
    /* Make sure stuff exists: */
    AssertPtrReturnVoid(m_pToolBar);
    m_pToolBar->setUseTextLabels(gEDataManager->selectorWindowToolBarTextVisible());
}

void UIVirtualBoxManagerAdvancedWidget::updateToolbar()
{
    /* Make sure stuff exists: */
    AssertPtrReturnVoid(m_pToolBar);
    AssertPtrReturnVoid(globalToolManager());
    AssertPtrReturnVoid(machineToolsWidget());

    /* Clear toolbar initially: */
    m_pToolBar->clear();

    switch (globalToolManager()->toolType())
    {
        case UIToolType_Home:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Home_S_New));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Home_S_Add));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Application_S_Preferences));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_File_S_ImportAppliance));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_File_S_ExportAppliance));
            break;
        }
        case UIToolType_Machines:
        {
            switch (machineToolsWidget()->toolType())
            {
                case UIToolType_Details:
                {
                    if (isSingleGroupSelected())
                    {
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_New));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_Add));
                        m_pToolBar->addSeparator();
                        if (isSingleLocalGroupSelected())
                            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_S_Discard));
                        else if (   isSingleCloudProviderGroupSelected()
                                 || isSingleCloudProfileGroupSelected())
                            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_M_Stop_S_Terminate));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Group_M_StartOrShow));
                    }
                    else
                    {
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_New));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Add));
                        m_pToolBar->addSeparator();
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                        if (isLocalMachineItemSelected())
                            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                        else if (isCloudMachineItemSelected())
                            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_Stop_S_Terminate));
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    }
                    break;
                }
                case UIToolType_Snapshots:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Take));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Delete));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Restore));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_T_Properties));
                    if (gEDataManager->isSettingsInExpertMode())
                        m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Snapshot_S_Clone));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_Logs:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Save));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Find));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Filter));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Bookmark));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_T_Preferences));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Refresh));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Log_S_Reload));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_VMActivity:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Activity_S_Export));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Activity_S_ToVMActivityOverview));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_Activity_T_Preferences));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_FileManager:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_FileManager_T_Preferences));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_FileManager_T_Operations));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndex_M_FileManager_T_Log));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Settings));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Discard));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_M_StartOrShow));
                    break;
                }
                case UIToolType_Error:
                {
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_New));
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Add));
                    m_pToolBar->addSeparator();
                    m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Machine_S_Refresh));
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case UIToolType_Extensions:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Extension_S_Install));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Extension_S_Uninstall));
            break;
        }
        case UIToolType_Media:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Add));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Create));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Copy));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Move));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Remove));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Release));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Clear));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_T_Search));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_T_Details));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Medium_S_Refresh));
            break;
        }
        case UIToolType_Network:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Create));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Remove));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_T_Details));
            //m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Network_S_Refresh));
            break;
        }
        case UIToolType_Cloud:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Add));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Import));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Remove));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_T_Details));
            m_pToolBar->addSeparator();
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_TryPage));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_Cloud_S_Help));
            break;
        }
        case UIToolType_Activities:
        {
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_VMActivityOverview_M_Columns));
            m_pToolBar->addAction(actionPool()->action(UIActionIndexMN_M_VMActivityOverview_S_SwitchToMachineActivity));
            QToolButton *pButton =
                qobject_cast<QToolButton*>(m_pToolBar->widgetForAction(actionPool()->action(UIActionIndexMN_M_VMActivityOverview_M_Columns)));
            if (pButton)
            {
                pButton->setPopupMode(QToolButton::InstantPopup);
                pButton->setAutoRaise(true);
            }
            break;
        }
        default:
            break;
    }
}

void UIVirtualBoxManagerAdvancedWidget::cleanupConnections()
{
    /* Global COM event handlers: */
    disconnect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);

    /* Tool-bar connections: */
    disconnect(m_pToolBar, &QIToolBar::customContextMenuRequested,
               this, &UIVirtualBoxManagerAdvancedWidget::sltHandleToolBarContextMenuRequest);

    /* Global Tool Manager connections: */
    disconnect(globalToolManager(), &UIGlobalToolsManagerWidget::sigToolTypeChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    disconnect(globalToolManager(), &UIGlobalToolsManagerWidget::sigToolTypeChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sigToolTypeChangeGlobal);
    /* Global Tool Pane connections: */
    disconnect(globalToolPane(), &UIToolPaneGlobal::sigCreateMedium,
               this, &UIVirtualBoxManagerAdvancedWidget::sigCreateMedium);
    disconnect(globalToolPane(), &UIToolPaneGlobal::sigCopyMedium,
               this, &UIVirtualBoxManagerAdvancedWidget::sigCopyMedium);

    /* Machine Tool Manager connections: */
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigToolTypeChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigToolTypeChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sigToolTypeChangeMachine);
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigChooserPaneIndexChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sigChooserPaneIndexChange);
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigChooserPaneSelectionChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sltUpdateToolbar);
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigCloudMachineStateChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sigCloudMachineStateChange);
    /* Machine Tool Pane connections: */
    disconnect(machineToolPane(), &UIToolPaneMachine::sigLinkClicked,
               this, &UIVirtualBoxManagerAdvancedWidget::sigMachineSettingsLinkClicked);
    disconnect(machineToolPane(), &UIToolPaneMachine::sigCurrentSnapshotItemChange,
               this, &UIVirtualBoxManagerAdvancedWidget::sigCurrentSnapshotItemChange);
    disconnect(machineToolPane(), &UIToolPaneMachine::sigDetachToolPane,
               this, &UIVirtualBoxManagerAdvancedWidget::sigDetachToolPane);

    /* Chooser-pane connections: */
    disconnect(chooser(), &UIChooser::sigGroupSavingStateChanged,
               this, &UIVirtualBoxManagerAdvancedWidget::sigGroupSavingStateChanged);
    disconnect(chooser(), &UIChooser::sigCloudUpdateStateChanged,
               this, &UIVirtualBoxManagerAdvancedWidget::sigCloudUpdateStateChanged);
    disconnect(chooser(), &UIChooser::sigStartOrShowRequest,
               this, &UIVirtualBoxManagerAdvancedWidget::sigStartOrShowRequest);
    disconnect(chooser(), &UIChooser::sigMachineSearchWidgetVisibilityChanged,
               this, &UIVirtualBoxManagerAdvancedWidget::sigMachineSearchWidgetVisibilityChanged);
}

void UIVirtualBoxManagerAdvancedWidget::cleanup()
{
    /* Destroy notification-center: */
    UINotificationCenter::destroy();
}

UIGlobalToolsManagerWidget *UIVirtualBoxManagerAdvancedWidget::globalToolManager() const
{
    return m_pGlobalToolManager;
}

UIToolPaneGlobal *UIVirtualBoxManagerAdvancedWidget::globalToolPane() const
{
    return globalToolManager()->toolPane();
}

UIMachineToolsWidget *UIVirtualBoxManagerAdvancedWidget::machineToolsWidget() const
{
    return globalToolManager()->machineToolsWidget();
}

UIToolPaneMachine *UIVirtualBoxManagerAdvancedWidget::machineToolPane() const
{
    return machineToolsWidget()->toolPane();
}

UIChooser *UIVirtualBoxManagerAdvancedWidget::chooser() const
{
    return machineToolsWidget()->chooser();
}
