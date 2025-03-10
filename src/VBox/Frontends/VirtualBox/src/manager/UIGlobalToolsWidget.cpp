/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalToolsWidget class implementation.
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
#include <QGridLayout>

/* GUI includes: */
#include "QIToolBar.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIGlobalToolsWidget.h"
#include "UIMachineToolsWidget.h"
#include "UIToolPane.h"
#include "UIToolPaneMachine.h"
#include "UITools.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualBoxManagerAdvancedWidget.h"
#include "UIVirtualMachineItem.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIGlobalToolsWidget::UIGlobalToolsWidget(UIVirtualBoxManagerAdvancedWidget *pParent, UIActionPool *pActionPool)
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

UIToolType UIGlobalToolsWidget::menuToolType() const
{
    AssertPtrReturn(toolMenu(), UIToolType_Invalid);
    return toolMenu()->toolsType(UIToolClass_Global);
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

    /* Special handling for Machines global tool,
     * notify Machine tool-pane it's active: */
    if (enmType == UIToolType_Machines)
    {
        toolPane()->setActive(false);
        toolPaneMachine()->setActive(true);
    }
    /* Otherwise, notify Global tool-pane it's active: */
    else
    {
        toolPaneMachine()->setActive(false);
        toolPane()->setActive(true);
    }

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
    if (!chooser()->currentItem())
        restrictedTypes << UIToolType_Machines;

    /* Make sure no restricted tool is selected: */
    if (restrictedTypes.contains(toolMenu()->toolsType(UIToolClass_Global)))
        setMenuToolType(UIToolType_Home);

    /* Hide restricted tools in the menu: */
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    toolMenu()->setRestrictedToolTypes(UIToolClass_Global, restrictions);

    /* Close all restricted tools (besides the Machines): */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        if (enmRestrictedType != UIToolType_Machines)
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

    // /* Disable even unrestricted tools for inacccessible VMs: */
    // const bool fCurrentItemIsOk = isItemAccessible(pItem);
    // toolMenu()->setItemsEnabled(fCurrentItemIsOk);
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
        switchToolTo(enmType);
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
        m_pPane = new UIToolPane(actionPool());
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
    connect(machineToolsWidget(), &UIMachineToolsWidget::sigToolMenuUpdate,
            this, &UIGlobalToolsWidget::sltHandleMachineToolMenuUpdate);
    connect(toolPaneMachine(), &UIToolPaneMachine::sigSwitchToActivityOverviewPane,
            this, &UIGlobalToolsWidget::sltSwitchToActivitiesTool);
}

void UIGlobalToolsWidget::loadSettings()
{
    /* Acquire & select tools currently chosen in the menu: */
    const UIToolType enmTypeGlobal = toolMenu()->toolsType(UIToolClass_Global);
    const UIToolType enmTypeMachine = toolMenu()->toolsType(UIToolClass_Machine);
    sltHandleToolsMenuIndexChange(enmTypeGlobal);
    sltHandleToolsMenuIndexChange(enmTypeMachine);

    /* Update tools restrictions: */
    emit sigToolMenuUpdate();
}

void UIGlobalToolsWidget::cleanupConnections()
{
    /* Global COM event handlers: */
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
    disconnect(machineToolsWidget(), &UIMachineToolsWidget::sigToolMenuUpdate,
               this, &UIGlobalToolsWidget::sltHandleMachineToolMenuUpdate);
    disconnect(toolPaneMachine(), &UIToolPaneMachine::sigSwitchToActivityOverviewPane,
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

UIToolPaneMachine *UIGlobalToolsWidget::toolPaneMachine() const
{
    return machineToolsWidget()->toolPane();
}
