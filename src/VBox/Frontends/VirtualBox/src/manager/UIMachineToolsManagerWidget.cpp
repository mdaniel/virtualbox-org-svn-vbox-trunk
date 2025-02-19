/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineToolsManagerWidget class implementation.
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
#include <QTimer>

/* GUI includes: */
#include "QISplitter.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSession.h"
#include "UIMachineToolsManagerWidget.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"
#include "UITools.h"
#include "UITranslationEventListener.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItemLocal.h"


UIMachineToolsManagerWidget::UIMachineToolsManagerWidget(UIToolPaneGlobal *pParent, UIActionPool *pActionPool)
    : QWidget(pParent)
    , m_pParent(pParent)
    , m_pActionPool(pActionPool)
    , m_pSplitter(0)
    , m_pPaneChooser(0)
    , m_pPaneTools(0)
    , m_pMenuTools(0)
    , m_enmSelectionType(SelectionType_Invalid)
    , m_fSelectedMachineItemAccessible(false)
    , m_pSplitterSettingsSaveTimer(0)
{
    prepare();
}

UIChooser *UIMachineToolsManagerWidget::chooser() const
{
    return m_pPaneChooser;
}

UIVirtualMachineItem *UIMachineToolsManagerWidget::currentItem() const
{
    return chooser()->currentItem();
}

QList<UIVirtualMachineItem*> UIMachineToolsManagerWidget::currentItems() const
{
    return chooser()->currentItems();
}

bool UIMachineToolsManagerWidget::isItemAccessible(UIVirtualMachineItem *pItem /* = 0 */) const
{
    if (!pItem)
        pItem = currentItem();
    return pItem && pItem->accessible();
}

bool UIMachineToolsManagerWidget::isGroupItemSelected() const
{
    return chooser()->isGroupItemSelected();
}

bool UIMachineToolsManagerWidget::isMachineItemSelected() const
{
    return chooser()->isMachineItemSelected();
}

bool UIMachineToolsManagerWidget::isLocalMachineItemSelected() const
{
    return chooser()->isLocalMachineItemSelected();
}

bool UIMachineToolsManagerWidget::isCloudMachineItemSelected() const
{
    return chooser()->isCloudMachineItemSelected();
}

bool UIMachineToolsManagerWidget::isSingleLocalGroupSelected() const
{
    return chooser()->isSingleLocalGroupSelected();
}

bool UIMachineToolsManagerWidget::isSingleCloudProviderGroupSelected() const
{
    return chooser()->isSingleCloudProviderGroupSelected();
}

bool UIMachineToolsManagerWidget::isSingleCloudProfileGroupSelected() const
{
    return chooser()->isSingleCloudProfileGroupSelected();
}

UIMachineToolsManagerWidget::SelectionType UIMachineToolsManagerWidget::selectionType() const
{
    return   isSingleLocalGroupSelected()
           ? SelectionType_SingleLocalGroupItem
           : isSingleCloudProviderGroupSelected() || isSingleCloudProfileGroupSelected()
           ? SelectionType_SingleCloudGroupItem
           : isLocalMachineItemSelected()
           ? SelectionType_FirstIsLocalMachineItem
           : isCloudMachineItemSelected()
           ? SelectionType_FirstIsCloudMachineItem
           : SelectionType_Invalid;
}

UIToolPaneMachine *UIMachineToolsManagerWidget::toolPane() const
{
    return m_pPaneTools;
}

UIToolType UIMachineToolsManagerWidget::menuToolType() const
{
    AssertPtrReturn(toolMenu(), UIToolType_Invalid);
    return toolMenu()->toolsType(UIToolClass_Machine);
}

void UIMachineToolsManagerWidget::setMenuToolType(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Machine class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine));

    AssertPtrReturnVoid(toolMenu());
    toolMenu()->setToolsType(enmType);
}

UIToolType UIMachineToolsManagerWidget::toolType() const
{
    AssertPtrReturn(toolPane(), UIToolType_Invalid);
    return toolPane()->currentTool();
}

bool UIMachineToolsManagerWidget::isToolOpened(UIToolType enmType) const
{
    /* Sanity check: */
    AssertReturn(enmType != UIToolType_Invalid, false);
    /* Make sure new tool type is of Machine class: */
    AssertReturn(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine), false);

    AssertPtrReturn(toolPane(), false);
    return toolPane()->isToolOpened(enmType);
}

void UIMachineToolsManagerWidget::switchToolTo(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Machine class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine));

    /* Open corresponding tool: */
    AssertPtrReturnVoid(toolPane());
    toolPane()->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();
}

void UIMachineToolsManagerWidget::closeTool(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Machine class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine));

    AssertPtrReturnVoid(toolPane());
    toolPane()->closeTool(enmType);
}

QString UIMachineToolsManagerWidget::currentHelpKeyword() const
{
    AssertPtrReturn(toolPane(), QString());
    return toolPane()->currentHelpKeyword();
}

void UIMachineToolsManagerWidget::sltRetranslateUI()
{
    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIMachineToolsManagerWidget::sltHandleCommitData()
{
    cleanupConnections();
}

void UIMachineToolsManagerWidget::sltHandleMachineStateChange(const QUuid &uId)
{
    // WORKAROUND:
    // In certain intermediate states VM info can be NULL which
    // causing annoying assertions, such updates can be ignored?
    CVirtualBox comVBox = gpGlobalSession->virtualBox();
    CMachine comMachine = comVBox.FindMachine(uId.toString());
    if (comVBox.isOk() && comMachine.isNotNull())
    {
        switch (comMachine.GetState())
        {
            case KMachineState_DeletingSnapshot:
                return;
            default:
                break;
        }
    }

    /* Recache current machine item information: */
    recacheCurrentMachineItemInformation();
}

void UIMachineToolsManagerWidget::sltHandleSettingsExpertModeChange()
{
    /* Update tool restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        emit sigToolMenuUpdate(pItem);
}

void UIMachineToolsManagerWidget::sltHandleSplitterMove()
{
    /* Create timer if isn't exist already: */
    if (!m_pSplitterSettingsSaveTimer)
    {
        m_pSplitterSettingsSaveTimer = new QTimer(this);
        if (m_pSplitterSettingsSaveTimer)
        {
            m_pSplitterSettingsSaveTimer->setInterval(300);
            m_pSplitterSettingsSaveTimer->setSingleShot(true);
            connect(m_pSplitterSettingsSaveTimer, &QTimer::timeout,
                    this, &UIMachineToolsManagerWidget::sltHandleSplitterSettingsSave);
        }
    }
    /* [Re]start timer finally: */
    m_pSplitterSettingsSaveTimer->start();
}

void UIMachineToolsManagerWidget::sltHandleSplitterSettingsSave()
{
    const QList<int> splitterSizes = m_pSplitter->sizes();
    gEDataManager->setSelectorWindowSplitterHints(splitterSizes);
}

void UIMachineToolsManagerWidget::sltHandleChooserPaneIndexChange()
{
    /* Let the parent know: */
    emit sigChooserPaneIndexChange();

    /* Update tool restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        emit sigToolMenuUpdate(pItem);

    /* Recache current machine item information: */
    recacheCurrentMachineItemInformation();

    /* Calculate new selection type and item accessibility status: */
    const SelectionType enmSelectedItemType = selectionType();
    const bool fCurrentItemIsOk = isItemAccessible();

    /* Update toolbar if selection type or item accessibility status got changed: */
    if (   m_enmSelectionType != enmSelectedItemType
        || m_fSelectedMachineItemAccessible != fCurrentItemIsOk)
        emit sigChooserPaneSelectionChange();

    /* Remember selection type and item accessibility status: */
    m_enmSelectionType = enmSelectedItemType;
    m_fSelectedMachineItemAccessible = fCurrentItemIsOk;
}

void UIMachineToolsManagerWidget::sltHandleChooserPaneSelectionInvalidated()
{
    recacheCurrentMachineItemInformation(true /* fDontRaiseErrorPane */);
}

void UIMachineToolsManagerWidget::sltHandleCloudMachineStateChange(const QUuid &uId)
{
    /* Acquire current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = isItemAccessible(pItem);

    /* If current item is Ok: */
    if (fCurrentItemIsOk)
    {
        /* If Error-pane is chosen currently => switch to tool currently chosen in tools-menu: */
        if (toolPane()->currentTool() == UIToolType_Error)
            switchToolTo(toolMenu()->toolsType(UIToolClass_Machine));

        /* If we still have same item selected: */
        if (pItem && pItem->id() == uId)
        {
            /* Propagate current items to update the Details-pane: */
            toolPane()->setItems(currentItems());
        }
    }
    else
    {
        /* Make sure Error pane raised: */
        if (toolPane()->currentTool() != UIToolType_Error)
            toolPane()->openTool(UIToolType_Error);

        /* If we still have same item selected: */
        if (pItem && pItem->id() == uId)
        {
            /* Propagate current items to update the Details-pane (in any case): */
            toolPane()->setItems(currentItems());
            /* Propagate last access error to update the Error-pane (if machine selected but inaccessible): */
            toolPane()->setErrorDetails(pItem->accessError());
        }
    }

    /* Pass the signal further: */
    emit sigCloudMachineStateChange(uId);
}

void UIMachineToolsManagerWidget::sltHandleToolMenuUpdate(UIVirtualMachineItem *pItem)
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

    /* Disable even unrestricted tools for inacccessible VMs: */
    const bool fCurrentItemIsOk = isItemAccessible(pItem);
    toolMenu()->setItemsEnabled(fCurrentItemIsOk);

    /* Close all restricted tools: */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        toolPane()->closeTool(enmRestrictedType);
}

void UIMachineToolsManagerWidget::sltHandleToolMenuRequested(const QPoint &position, UIVirtualMachineItem *pItem)
{
    /* Update tools restrictions for item specified: */
    AssertPtrReturnVoid(pItem);
    if (pItem)
        emit sigToolMenuUpdate(pItem);

    /* Compose popup-menu geometry first of all: */
    QRect ourGeo = QRect(position, toolMenu()->minimumSizeHint());
    /* Adjust location only to properly fit into available geometry space: */
    const QRect availableGeo = gpDesktop->availableGeometry(position);
    ourGeo = gpDesktop->normalizeGeometry(ourGeo, availableGeo, false /* resize? */);

    /* Move, resize and show: */
    toolMenu()->move(ourGeo.topLeft());
    toolMenu()->show();
    // WORKAROUND:
    // Don't want even to think why, but for Qt::Popup resize to
    // smaller size often being ignored until it is actually shown.
    toolMenu()->resize(ourGeo.size());
}

void UIMachineToolsManagerWidget::sltHandleToolsMenuIndexChange(UIToolType enmType)
{
    switchToolTo(enmType);
}

void UIMachineToolsManagerWidget::sltSwitchToVMActivityTool(const QUuid &uMachineId)
{
    AssertPtrReturnVoid(chooser());
    AssertPtrReturnVoid(toolMenu());
    chooser()->setCurrentMachine(uMachineId);
    toolMenu()->setToolsType(UIToolType_VMActivity);
}

void UIMachineToolsManagerWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    sltRetranslateUI();
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIMachineToolsManagerWidget::sltRetranslateUI);

    /* Make sure current Chooser-pane index fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIMachineToolsManagerWidget::prepareWidgets()
{
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    if (pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);

        /* Create splitter: */
        m_pSplitter = new QISplitter;
        if (m_pSplitter)
        {
            /* Create chooser-pane: */
            m_pPaneChooser = new UIChooser(this, actionPool());
            if (chooser())
            {
                /* Add into splitter: */
                m_pSplitter->addWidget(chooser());
            }

            /* Create tool-pane: */
            m_pPaneTools = new UIToolPaneMachine(actionPool());
            if (toolPane())
            {
                /// @todo make sure it's used properly
                toolPane()->setActive(true);

                /* Add into splitter: */
                m_pSplitter->addWidget(toolPane());
            }

            /* Set the initial distribution. The right site is bigger. */
            m_pSplitter->setStretchFactor(0, 2);
            m_pSplitter->setStretchFactor(1, 3);

            /* Add into layout: */
            pLayout->addWidget(m_pSplitter);
        }

        /* Create tools-menu: */
        m_pMenuTools = new UITools(this, UIToolClass_Machine, actionPool());
    }

    /* Bring the VM list to the focus: */
    chooser()->setFocus();
}

void UIMachineToolsManagerWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIMachineToolsManagerWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIMachineToolsManagerWidget::sltHandleMachineStateChange);
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIMachineToolsManagerWidget::sltHandleSettingsExpertModeChange);

    /* Parent connections: */
    connect(m_pParent, &UIToolPaneGlobal::sigSwitchToMachineActivityPane,
            this, &UIMachineToolsManagerWidget::sltSwitchToVMActivityTool);

    /* Splitter connections: */
    connect(m_pSplitter, &QISplitter::splitterMoved,
            this, &UIMachineToolsManagerWidget::sltHandleSplitterMove);

    /* Chooser-pane connections: */
    connect(chooser(), &UIChooser::sigSelectionChanged,
            this, &UIMachineToolsManagerWidget::sltHandleChooserPaneIndexChange);
    connect(chooser(), &UIChooser::sigSelectionInvalidated,
            this, &UIMachineToolsManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    connect(chooser(), &UIChooser::sigToolMenuRequested,
            this, &UIMachineToolsManagerWidget::sltHandleToolMenuRequested);
    connect(chooser(), &UIChooser::sigCloudMachineStateChange,
            this, &UIMachineToolsManagerWidget::sltHandleCloudMachineStateChange);
    connect(chooser(), &UIChooser::sigToggleStarted,
            toolPane(), &UIToolPaneMachine::sigToggleStarted);
    connect(chooser(), &UIChooser::sigToggleFinished,
            toolPane(), &UIToolPaneMachine::sigToggleFinished);

    /* Tools-menu connections: */
    connect(this, &UIMachineToolsManagerWidget::sigToolMenuUpdate,
            this, &UIMachineToolsManagerWidget::sltHandleToolMenuUpdate);
    connect(toolMenu(), &UITools::sigSelectionChanged,
            this, &UIMachineToolsManagerWidget::sltHandleToolsMenuIndexChange);
}

void UIMachineToolsManagerWidget::loadSettings()
{
    /* Restore splitter handle position: */
    {
        QList<int> sizes = gEDataManager->selectorWindowSplitterHints();
        /* If both hints are zero, we have the 'default' case: */
        if (sizes.at(0) == 0 && sizes.at(1) == 0)
        {
            sizes[0] = (int)(width() * .9 * (1.0 / 3));
            sizes[1] = (int)(width() * .9 * (2.0 / 3));
        }
        m_pSplitter->setSizes(sizes);
    }

    /* Open tool last chosen in tools-menu: */
    switchToolTo(toolMenu()->toolsType(UIToolClass_Machine));
}

void UIMachineToolsManagerWidget::cleanupConnections()
{
    /* Global COM event handlers: */
    disconnect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
               this, &UIMachineToolsManagerWidget::sltHandleMachineStateChange);
    disconnect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
               this, &UIMachineToolsManagerWidget::sltHandleSettingsExpertModeChange);

    /* Parent connections: */
    disconnect(m_pParent, &UIToolPaneGlobal::sigSwitchToMachineActivityPane,
               this, &UIMachineToolsManagerWidget::sltSwitchToVMActivityTool);

    /* Splitter connections: */
    disconnect(m_pSplitter, &QISplitter::splitterMoved,
               this, &UIMachineToolsManagerWidget::sltHandleSplitterMove);

    /* Chooser-pane connections: */
    disconnect(chooser(), &UIChooser::sigSelectionChanged,
               this, &UIMachineToolsManagerWidget::sltHandleChooserPaneIndexChange);
    disconnect(chooser(), &UIChooser::sigSelectionInvalidated,
               this, &UIMachineToolsManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    disconnect(chooser(), &UIChooser::sigToolMenuRequested,
               this, &UIMachineToolsManagerWidget::sltHandleToolMenuRequested);
    disconnect(chooser(), &UIChooser::sigCloudMachineStateChange,
               this, &UIMachineToolsManagerWidget::sltHandleCloudMachineStateChange);
    disconnect(chooser(), &UIChooser::sigToggleStarted,
               toolPane(), &UIToolPaneMachine::sigToggleStarted);
    disconnect(chooser(), &UIChooser::sigToggleFinished,
               toolPane(), &UIToolPaneMachine::sigToggleFinished);

    /* Tools-menu connections: */
    disconnect(this, &UIMachineToolsManagerWidget::sigToolMenuUpdate,
               this, &UIMachineToolsManagerWidget::sltHandleToolMenuUpdate);
    disconnect(toolMenu(), &UITools::sigSelectionChanged,
               this, &UIMachineToolsManagerWidget::sltHandleToolsMenuIndexChange);
}

UITools *UIMachineToolsManagerWidget::toolMenu() const
{
    return m_pMenuTools;
}

void UIMachineToolsManagerWidget::recacheCurrentMachineItemInformation(bool fDontRaiseErrorPane /* = false */)
{
    /* Sanity check, this method is for machine or group of machine items: */
    if (!isMachineItemSelected() && !isGroupItemSelected())
        return;

    /* Get current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = isItemAccessible(pItem);

    /* If current item is Ok: */
    if (fCurrentItemIsOk)
    {
        /* If Error-pane is chosen currently => switch to tool currently chosen in tools-menu: */
        if (toolPane()->currentTool() == UIToolType_Error)
            switchToolTo(toolMenu()->toolsType(UIToolClass_Machine));

        /* Propagate current items to the Tools pane: */
        toolPane()->setItems(currentItems());
    }
    /* Otherwise if we were not asked separately to calm down: */
    else if (!fDontRaiseErrorPane)
    {
        /* Make sure Error pane raised: */
        if (toolPane()->currentTool() != UIToolType_Error)
            toolPane()->openTool(UIToolType_Error);

        /* Propagate last access error to the Error-pane: */
        if (pItem)
            toolPane()->setErrorDetails(pItem->accessError());
    }
}
