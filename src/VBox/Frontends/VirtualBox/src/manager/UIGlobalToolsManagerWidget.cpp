/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalToolsManagerWidget class implementation.
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
#include <QHBoxLayout>

/* GUI includes: */
#include "UIChooser.h"
#include "UICommon.h"
#include "UIExtraDataManager.h"
#include "UIGlobalToolsManagerWidget.h"
#include "UIMachineManagerWidget.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"
#include "UITools.h"
#include "UIVirtualBoxManagerAdvancedWidget.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIGlobalToolsManagerWidget::UIGlobalToolsManagerWidget(UIVirtualBoxManagerAdvancedWidget *pParent,
                                                       UIActionPool *pActionPool)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pMenu(0)
    , m_pPane(0)
{
    prepare();
}

UIGlobalToolsManagerWidget::~UIGlobalToolsManagerWidget()
{
    cleanup();
}

UIToolPaneGlobal *UIGlobalToolsManagerWidget::toolPane() const
{
    return m_pPane;
}

UIMachineManagerWidget *UIGlobalToolsManagerWidget::machineManager() const
{
    return toolPane()->machineManager();
}

UIToolType UIGlobalToolsManagerWidget::menuToolType() const
{
    AssertPtrReturn(toolMenu(), UIToolType_Invalid);
    return toolMenu()->toolsType();
}

void UIGlobalToolsManagerWidget::setMenuToolType(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Global class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global));

    AssertPtrReturnVoid(toolMenu());
    toolMenu()->setToolsType(enmType);
}

UIToolType UIGlobalToolsManagerWidget::toolType() const
{
    AssertPtrReturn(toolPane(), UIToolType_Invalid);
    return toolPane()->currentTool();
}

bool UIGlobalToolsManagerWidget::isToolOpened(UIToolType enmType) const
{
    /* Sanity check: */
    AssertReturn(enmType != UIToolType_Invalid, false);
    /* Make sure new tool type is of Global class: */
    AssertReturn(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global), false);

    AssertPtrReturn(toolPane(), false);
    return toolPane()->isToolOpened(enmType);
}

void UIGlobalToolsManagerWidget::switchToolTo(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Global class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global));

    /* Open corresponding tool: */
    AssertPtrReturnVoid(toolPane());
    toolPane()->openTool(enmType);

    /* Special handling for Machines Global tool,
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

void UIGlobalToolsManagerWidget::closeTool(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Global class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Global));

    AssertPtrReturnVoid(toolPane());
    toolPane()->closeTool(enmType);
}

QString UIGlobalToolsManagerWidget::currentHelpKeyword() const
{
    if (toolType() == UIToolType_Machines)
    {
        AssertPtrReturn(machineManager(), QString());
        return machineManager()->currentHelpKeyword();
    }

    AssertPtrReturn(toolPane(), QString());
    return toolPane()->currentHelpKeyword();
}

void UIGlobalToolsManagerWidget::sltHandleCommitData()
{
    // WORKAROUND:
    // This will be fixed proper way during session management cleanup for Qt6.
    // But for now we will just cleanup connections which is Ok anyway.
    cleanupConnections();
}

void UIGlobalToolsManagerWidget::sltHandleSettingsExpertModeChange()
{
    /* Update tools restrictions: */
    updateToolsMenu();
}

void UIGlobalToolsManagerWidget::sltHandleCloudProfileStateChange(const QString &, const QString &)
{
    /* If Global Activities tool is currently chosen: */
    AssertPtrReturnVoid(toolPane());
    if (toolType() == UIToolType_Activities)
    {
        /* Propagate a set of cloud machine items to Global tool-pane: */
        toolPane()->setCloudMachineItems(chooser()->cloudMachineItems());
    }
}

void UIGlobalToolsManagerWidget::sltHandleToolsMenuIndexChange(UIToolType enmType)
{
    switchToolTo(enmType);
}

void UIGlobalToolsManagerWidget::sltSwitchToActivitiesTool()
{
    setMenuToolType(UIToolType_Activities);
}

void UIGlobalToolsManagerWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();
}

void UIGlobalToolsManagerWidget::prepareWidgets()
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    if (pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create tool-menu: */
        m_pMenu = new UITools(this, UIToolClass_Global, actionPool(), Qt::Widget);
        if (toolMenu())
        {
            /* Add into layout: */
            pLayout->addWidget(toolMenu());
        }

        /* Create tool-pane: */
        m_pPane = new UIToolPaneGlobal(actionPool());
        if (toolPane())
        {
            /// @todo make sure it's used properly
            toolPane()->setActive(true);

            /* Add into layout: */
            pLayout->addWidget(toolPane());
        }
    }
}

void UIGlobalToolsManagerWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIGlobalToolsManagerWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIGlobalToolsManagerWidget::sltHandleSettingsExpertModeChange);

    /* Chooser-pane connections: */
    connect(chooser(), &UIChooser::sigCloudProfileStateChange,
            this, &UIGlobalToolsManagerWidget::sltHandleCloudProfileStateChange);

    /* Tools-menu connections: */
    connect(toolMenu(), &UITools::sigSelectionChanged,
            this, &UIGlobalToolsManagerWidget::sltHandleToolsMenuIndexChange);

    /* Tools-pane connections: */
    connect(toolPaneMachine(), &UIToolPaneMachine::sigSwitchToActivityOverviewPane,
            this, &UIGlobalToolsManagerWidget::sltSwitchToActivitiesTool);
}

void UIGlobalToolsManagerWidget::loadSettings()
{
    /* Open tool last chosen in tools-menu: */
    switchToolTo(toolMenu()->toolsType());
}

void UIGlobalToolsManagerWidget::cleanupConnections()
{
    /* Global COM event handlers: */
    disconnect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
               this, &UIGlobalToolsManagerWidget::sltHandleSettingsExpertModeChange);

    /* Chooser-pane connections: */
    disconnect(chooser(), &UIChooser::sigCloudProfileStateChange,
               this, &UIGlobalToolsManagerWidget::sltHandleCloudProfileStateChange);

    /* Tools-menu connections: */
    disconnect(toolMenu(), &UITools::sigSelectionChanged,
               this, &UIGlobalToolsManagerWidget::sltHandleToolsMenuIndexChange);

    /* Tools-pane connections: */
    disconnect(toolPaneMachine(), &UIToolPaneMachine::sigSwitchToActivityOverviewPane,
               this, &UIGlobalToolsManagerWidget::sltSwitchToActivitiesTool);
}

void UIGlobalToolsManagerWidget::cleanup()
{
    /* Ask sub-dialogs to commit data: */
    sltHandleCommitData();
}

UITools *UIGlobalToolsManagerWidget::toolMenu() const
{
    return m_pMenu;
}

UIChooser *UIGlobalToolsManagerWidget::chooser() const
{
    return machineManager()->chooser();
}

UIToolPaneMachine *UIGlobalToolsManagerWidget::toolPaneMachine() const
{
    return machineManager()->toolPane();
}

void UIGlobalToolsManagerWidget::updateToolsMenu()
{
    /* Update global tools restrictions: */
    QSet<UIToolType> restrictedTypes;
    const bool fExpertMode = gEDataManager->isSettingsInExpertMode();
    if (!fExpertMode)
        restrictedTypes << UIToolType_Media
                        << UIToolType_Network;
    if (restrictedTypes.contains(toolMenu()->toolsType()))
        setMenuToolType(UIToolType_Welcome);
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    toolMenu()->setRestrictedToolTypes(restrictions);

    /* Take restrictions into account, closing all restricted tools: */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        toolPane()->closeTool(enmRestrictedType);
}
