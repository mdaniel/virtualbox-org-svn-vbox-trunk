/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalToolsWidget class implementation.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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
#include <QGridLayout>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIGlobalToolsWidget.h"
#include "UIMachineToolsWidget.h"
#include "UIManagementToolsWidget.h"
#include "UIToolPane.h"
#include "UITools.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItem.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIGlobalToolsWidget::UIGlobalToolsWidget(QWidget *pParent, UIActionPool *pActionPool)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pMenu(0)
    , m_pPane(0)
{
    prepare();
}

void UIGlobalToolsWidget::addToolBar(QIToolBar *pToolBar)
{
    AssertPtrReturnVoid(m_pLayout);
    m_pLayout->addWidget(pToolBar, 0, 1);
}

UIToolPane *UIGlobalToolsWidget::toolPane() const
{
    return m_pPane;
}

UIMachineToolsWidget *UIGlobalToolsWidget::machineToolsWidget() const
{
    return toolPane()->machineToolsWidget();
}

UIManagementToolsWidget *UIGlobalToolsWidget::managementToolsWidget() const
{
    return toolPane()->managementToolsWidget();
}

UIToolType UIGlobalToolsWidget::menuToolType(UIToolClass enmClass) const
{
    AssertPtrReturn(toolMenu(), UIToolType_Invalid);
    return toolMenu()->toolsType(enmClass);
}

void UIGlobalToolsWidget::setMenuToolType(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);

    AssertPtrReturnVoid(toolMenu());
    toolMenu()->setToolsType(enmType);
}

UIToolType UIGlobalToolsWidget::toolType() const
{
    AssertPtrReturn(toolPane(), UIToolType_Invalid);
    return toolPane()->currentTool();
}

bool UIGlobalToolsWidget::isToolOpened(UIToolType enmType) const
{
    /* Sanity check: */
    AssertReturn(enmType != UIToolType_Invalid, false);
    /* Make sure new tool type is of Global class: */
    AssertReturn(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global), false);

    AssertPtrReturn(toolPane(), false);
    return toolPane()->isToolOpened(enmType);
}

void UIGlobalToolsWidget::switchToolTo(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Global class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global));

    /* Open corresponding tool: */
    AssertPtrReturnVoid(toolPane());
    toolPane()->openTool(enmType);

    /* Notify corresponding tool-pane it's active: */
    toolPane()->setActive(enmType != UIToolType_Machines && enmType != UIToolType_Managers);
    toolPaneMachine()->setActive(enmType == UIToolType_Machines);
    toolPaneManagement()->setActive(enmType == UIToolType_Managers);

    /* Special handling for Activities Global tool,
     * start unconditionally updating all cloud VMs: */
    if (enmType == UIToolType_Activities)
    {
        chooser()->setKeepCloudNodesUpdated(true);
        toolPane()->setCloudMachineItems(chooser()->cloudMachineItems());
    }
    /* Otherwise, stop unconditionally updating all cloud VMs,
     * (tho they will still be updated if selected) */
    else
        chooser()->setKeepCloudNodesUpdated(false);

    /* Let the parent know: */
    emit sigToolTypeChange();
}

void UIGlobalToolsWidget::closeTool(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Global class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global));

    AssertPtrReturnVoid(toolPane());
    toolPane()->closeTool(enmType);
}

QString UIGlobalToolsWidget::currentHelpKeyword() const
{
    if (toolType() == UIToolType_Machines)
    {
        AssertPtrReturn(machineToolsWidget(), QString());
        return machineToolsWidget()->currentHelpKeyword();
    }
    else if (toolType() == UIToolType_Managers)
    {
        AssertPtrReturn(managementToolsWidget(), QString());
        return managementToolsWidget()->currentHelpKeyword();
    }

    AssertPtrReturn(toolPane(), QString());
    return toolPane()->currentHelpKeyword();
}

void UIGlobalToolsWidget::sltHandleCommitData()
{
    cleanupConnections();
}

void UIGlobalToolsWidget::sltHandleMachineRegistrationChanged(const QUuid &, const bool fRegistered)
{
    /* On any VM registered switch from Home to Machines: */
    AssertPtrReturnVoid(toolMenu());
    if (fRegistered && toolMenu()->toolsType(UIToolClass_Global) == UIToolType_Home)
        setMenuToolType(UIToolType_Machines);
}

void UIGlobalToolsWidget::sltHandleSettingsExpertModeChange()
{
    /* Update tools restrictions: */
    emit sigToolMenuUpdate();
}

void UIGlobalToolsWidget::sltHandleChooserPaneSelectionChange()
{
    /* Update tools restrictions: */
    emit sigToolMenuUpdate();
}

void UIGlobalToolsWidget::sltHandleCloudProfileStateChange(const QString &, const QString &)
{
    /* If Global Activities tool is currently chosen: */
    AssertPtrReturnVoid(toolPane());
    if (toolType() == UIToolType_Activities)
    {
        /* Propagate a set of cloud machine items to Global tool-pane: */
        toolPane()->setCloudMachineItems(chooser()->cloudMachineItems());
    }
}

void UIGlobalToolsWidget::sltHandleGlobalToolMenuUpdate()
{
    /* Prepare tool restrictions: */
    QSet<UIToolType> restrictedTypes;

    /* Restrict some types for Basic mode: */
    const bool fExpertMode = gEDataManager->isSettingsInExpertMode();
    if (!fExpertMode)
        restrictedTypes << UIToolType_Media
                        << UIToolType_Network;

    /* Make sure Machines tool is hidden for empty Chooser-pane: */
    if (chooser()->isNavigationListEmpty())
        restrictedTypes << UIToolType_Machines;

    /* Make sure no restricted tool is selected: */
    if (restrictedTypes.contains(toolMenu()->toolsType(UIToolClass_Global)))
        setMenuToolType(UIToolType_Home);

    /* Hide restricted tools in the menu: */
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    toolMenu()->setRestrictedToolTypes(UIToolClass_Global, restrictions);

    /* Close all restricted tools (besides the Machines and Management): */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        if (   enmRestrictedType != UIToolType_Machines
            && enmRestrictedType != UIToolType_Managers)
            toolPane()->closeTool(enmRestrictedType);
}

void UIGlobalToolsWidget::sltHandleMachineToolMenuUpdate(UIVirtualMachineItem *pItem)
{
    /* Prepare tool restrictions: */
    QSet<UIToolType> restrictedTypes;

    /* Restrict some types for Basic mode: */
    const bool fExpertMode = gEDataManager->isSettingsInExpertMode();
    if (!fExpertMode)
        restrictedTypes << UIToolType_FileManager;

    /* Make sure local VM tools are hidden for cloud VMs: */
    if (pItem && pItem->itemType() != UIVirtualMachineItemType_Local)
        restrictedTypes << UIToolType_Snapshots
                        << UIToolType_Logs
                        << UIToolType_FileManager;

    /* Make sure no restricted tool is selected: */
    if (restrictedTypes.contains(toolMenu()->toolsType(UIToolClass_Machine)))
        setMenuToolType(UIToolType_Details);

    /* Hide restricted tools in the menu: */
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    toolMenu()->setRestrictedToolTypes(UIToolClass_Machine, restrictions);

    /// @todo finish that
    // /* Disable even unrestricted tools for inacccessible VMs: */
    // const bool fCurrentItemIsOk = isItemAccessible(pItem);
    // toolMenu()->setItemsEnabled(fCurrentItemIsOk);

    /* Close all restricted tools: */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        toolPaneMachine()->closeTool(enmRestrictedType);
}

void UIGlobalToolsWidget::sltHandleManagementToolMenuUpdate()
{
    /* Prepare tool restrictions: */
    QSet<UIToolType> restrictedTypes;

    /* Make sure no restricted tool is selected: */
    if (restrictedTypes.contains(toolMenu()->toolsType(UIToolClass_Management)))
        setMenuToolType(UIToolType_Extensions);

    /* Hide restricted tools in the menu: */
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    toolMenu()->setRestrictedToolTypes(UIToolClass_Management, restrictions);

    /* Close all restricted tools: */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        toolPaneManagement()->closeTool(enmRestrictedType);
}

void UIGlobalToolsWidget::sltHandleToolsMenuIndexChange(UIToolType enmType)
{
    /* Determine tool class of passed tool type: */
    const UIToolClass enmClass = UIToolStuff::castTypeToClass(enmType);

    /* For Global tool class: */
    if (enmClass == UIToolClass_Global)
    {
        /* Mark Machine & Management tools [un]suitable depending on Global tool selected: */
        toolMenu()->setUnsuitableToolClass(UIToolClass_Machine, enmType != UIToolType_Machines);
        toolMenu()->setUnsuitableToolClass(UIToolClass_Management, enmType != UIToolType_Managers);

        /* Switch tool-pane accordingly: */
        switchToolTo(enmType);
    }
    /* For Machine tool class => switch tool-pane accordingly: */
    else if (enmClass == UIToolClass_Machine)
        machineToolsWidget()->switchToolTo(enmType);
    /* For Management tool class => switch tool-pane accordingly: */
    else if (enmClass == UIToolClass_Management)
        managementToolsWidget()->switchToolTo(enmType);
}

void UIGlobalToolsWidget::sltSwitchToActivitiesTool()
{
    setMenuToolType(UIToolType_Activities);
}

void UIGlobalToolsWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();
}

void UIGlobalToolsWidget::prepareWidgets()
{
    /* Create layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        /* Configure layout: */
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setSpacing(0);

        /* Create tool-menu: */
        m_pMenu = new UITools(this, UIToolClass_Global, actionPool(), Qt::Widget);
        if (toolMenu())
        {
            /* Add into layout: */
            m_pLayout->addWidget(toolMenu(), 0, 0, 2, 1);
        }

        /* Create tool-pane: */
        m_pPane = new UIToolPane(this, UIToolClass_Global, actionPool());
        if (toolPane())
        {
            /// @todo make sure it's used properly
            toolPane()->setActive(true);

            /* Add into layout: */
            m_pLayout->addWidget(toolPane(), 1, 1);
        }
    }
}

void UIGlobalToolsWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIGlobalToolsWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
            this, &UIGlobalToolsWidget::sltHandleMachineRegistrationChanged);
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIGlobalToolsWidget::sltHandleSettingsExpertModeChange);

    /* Chooser-pane connections: */
    connect(chooser(), &UIChooser::sigSelectionChanged,
            this, &UIGlobalToolsWidget::sltHandleChooserPaneSelectionChange);
    connect(chooser(), &UIChooser::sigCloudProfileStateChange,
            this, &UIGlobalToolsWidget::sltHandleCloudProfileStateChange);

    /* Tools-menu connections: */
    connect(toolMenu(), &UITools::sigSelectionChanged,
            this, &UIGlobalToolsWidget::sltHandleToolsMenuIndexChange);

    /* Tools-pane connections: */
    connect(this, &UIGlobalToolsWidget::sigToolMenuUpdate,
            this, &UIGlobalToolsWidget::sltHandleGlobalToolMenuUpdate);
    connect(this, &UIGlobalToolsWidget::sigToolMenuUpdate,
            this, &UIGlobalToolsWidget::sltHandleManagementToolMenuUpdate);
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigToolMenuUpdate,
            this, &UIGlobalToolsWidget::sltHandleMachineToolMenuUpdate);
    connect(toolPaneMachine(), &UIToolPane::sigSwitchToActivityOverviewPane,
            this, &UIGlobalToolsWidget::sltSwitchToActivitiesTool);
}

void UIGlobalToolsWidget::loadSettings()
{
    /* Acquire & select tools currently chosen in the menu: */
    sltHandleToolsMenuIndexChange(toolMenu()->toolsType(UIToolClass_Global));
    sltHandleToolsMenuIndexChange(toolMenu()->toolsType(UIToolClass_Machine));
    sltHandleToolsMenuIndexChange(toolMenu()->toolsType(UIToolClass_Management));

    /* Update Global tools restrictions: */
    sltHandleGlobalToolMenuUpdate();
    /* Update Machine tools restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = machineToolsWidget()->currentItem();
    if (pItem)
        sltHandleMachineToolMenuUpdate(pItem);
    /* Update Management tools restrictions: */
    sltHandleManagementToolMenuUpdate();
}

void UIGlobalToolsWidget::cleanupConnections()
{
    /* Global COM event handlers: */
    disconnect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineRegistered,
               this, &UIGlobalToolsWidget::sltHandleMachineRegistrationChanged);
    disconnect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
               this, &UIGlobalToolsWidget::sltHandleSettingsExpertModeChange);

    /* Chooser-pane connections: */
    disconnect(chooser(), &UIChooser::sigSelectionChanged,
               this, &UIGlobalToolsWidget::sltHandleChooserPaneSelectionChange);
    disconnect(chooser(), &UIChooser::sigCloudProfileStateChange,
               this, &UIGlobalToolsWidget::sltHandleCloudProfileStateChange);

    /* Tools-menu connections: */
    disconnect(toolMenu(), &UITools::sigSelectionChanged,
               this, &UIGlobalToolsWidget::sltHandleToolsMenuIndexChange);

    /* Tools-pane connections: */
    disconnect(this, &UIGlobalToolsWidget::sigToolMenuUpdate,
               this, &UIGlobalToolsWidget::sltHandleGlobalToolMenuUpdate);
    disconnect(this, &UIGlobalToolsWidget::sigToolMenuUpdate,
               this, &UIGlobalToolsWidget::sltHandleManagementToolMenuUpdate);
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigToolMenuUpdate,
               this, &UIGlobalToolsWidget::sltHandleMachineToolMenuUpdate);
    disconnect(toolPaneMachine(), &UIToolPane::sigSwitchToActivityOverviewPane,
               this, &UIGlobalToolsWidget::sltSwitchToActivitiesTool);
}

UITools *UIGlobalToolsWidget::toolMenu() const
{
    return m_pMenu;
}

UIChooser *UIGlobalToolsWidget::chooser() const
{
    return machineToolsWidget()->chooser();
}

UIToolPane *UIGlobalToolsWidget::toolPaneMachine() const
{
    return machineToolsWidget()->toolPane();
}

UIToolPane *UIGlobalToolsWidget::toolPaneManagement() const
{
    return managementToolsWidget()->toolPane();
}
