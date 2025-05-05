/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineToolsWidget class implementation.
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
#include <QHBoxLayout>
#include <QTimer>

/* GUI includes: */
#include "QISplitter.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSession.h"
#include "UIMachineToolsWidget.h"
#include "UIToolPane.h"
#include "UITools.h"
#include "UITranslationEventListener.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIVirtualMachineItemLocal.h"


UIMachineToolsWidget::UIMachineToolsWidget(UIToolPane *pParent, UIActionPool *pActionPool)
    : QWidget(pParent)
    , m_pParent(pParent)
    , m_pActionPool(pActionPool)
    , m_pMenu(0)
    , m_pSplitter(0)
    , m_pPaneChooser(0)
    , m_pPaneTools(0)
    , m_enmSelectionType(SelectionType_Invalid)
    , m_fSelectedMachineItemAccessible(false)
    , m_fSelectedMachineItemStarted(false)
    , m_pSplitterSettingsSaveTimer(0)
{
    prepare();
}

UIChooser *UIMachineToolsWidget::chooser() const
{
    return m_pPaneChooser;
}

UIVirtualMachineItem *UIMachineToolsWidget::currentItem() const
{
    return chooser()->currentItem();
}

QList<UIVirtualMachineItem*> UIMachineToolsWidget::currentItems() const
{
    return chooser()->currentItems();
}

bool UIMachineToolsWidget::isItemAccessible(UIVirtualMachineItem *pItem /* = 0 */) const
{
    if (!pItem)
        pItem = currentItem();
    return pItem && pItem->accessible();
}

bool UIMachineToolsWidget::isItemStarted(UIVirtualMachineItem *pItem /* = 0 */) const
{
    if (!pItem)
        pItem = currentItem();
    return pItem && pItem->isItemStarted();
}

bool UIMachineToolsWidget::isGroupItemSelected() const
{
    return chooser()->isGroupItemSelected();
}

bool UIMachineToolsWidget::isMachineItemSelected() const
{
    return chooser()->isMachineItemSelected();
}

bool UIMachineToolsWidget::isLocalMachineItemSelected() const
{
    return chooser()->isLocalMachineItemSelected();
}

bool UIMachineToolsWidget::isCloudMachineItemSelected() const
{
    return chooser()->isCloudMachineItemSelected();
}

bool UIMachineToolsWidget::isSingleLocalGroupSelected() const
{
    return chooser()->isSingleLocalGroupSelected();
}

bool UIMachineToolsWidget::isSingleCloudProviderGroupSelected() const
{
    return chooser()->isSingleCloudProviderGroupSelected();
}

bool UIMachineToolsWidget::isSingleCloudProfileGroupSelected() const
{
    return chooser()->isSingleCloudProfileGroupSelected();
}

UIMachineToolsWidget::SelectionType UIMachineToolsWidget::selectionType() const
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

UITools *UIMachineToolsWidget::toolMenu() const
{
    return m_pMenu;
}

UIToolPane *UIMachineToolsWidget::toolPane() const
{
    return m_pPaneTools;
}

UIToolType UIMachineToolsWidget::menuToolType(UIToolClass enmClass) const
{
    AssertPtrReturn(toolMenu(), UIToolType_Invalid);
    return toolMenu()->toolsType(enmClass);
}

void UIMachineToolsWidget::setMenuToolType(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);

    AssertPtrReturnVoid(toolMenu());
    toolMenu()->setToolsType(enmType);
}

UIToolType UIMachineToolsWidget::toolType() const
{
    AssertPtrReturn(toolPane(), UIToolType_Invalid);
    return toolPane()->currentTool();
}

bool UIMachineToolsWidget::isToolOpened(UIToolType enmType) const
{
    /* Sanity check: */
    AssertReturn(enmType != UIToolType_Invalid, false);
    /* Make sure new tool type is of Machine class: */
    AssertReturn(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine), false);

    AssertPtrReturn(toolPane(), false);
    return toolPane()->isToolOpened(enmType);
}

void UIMachineToolsWidget::switchToolTo(UIToolType enmType)
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

void UIMachineToolsWidget::closeTool(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Machine class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Machine));

    AssertPtrReturnVoid(toolPane());
    toolPane()->closeTool(enmType);
}

QString UIMachineToolsWidget::currentHelpKeyword() const
{
    AssertPtrReturn(toolPane(), QString());
    return toolPane()->currentHelpKeyword();
}

void UIMachineToolsWidget::sltRetranslateUI()
{
    /* Fetch Chooser-pane selection class: */
    recalculateChooserPaneSelectionClass();
}

void UIMachineToolsWidget::sltHandleCommitData()
{
    cleanupConnections();
}

void UIMachineToolsWidget::sltHandleMachineStateChange(const QUuid &uId)
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
    /* Fetch Chooser-pane selection class: */
    recalculateChooserPaneSelectionClass();
}

void UIMachineToolsWidget::sltHandleSettingsExpertModeChange()
{
    /* Update tool restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        emit sigToolMenuUpdate(pItem);
}

void UIMachineToolsWidget::sltHandleSplitterMove()
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
                    this, &UIMachineToolsWidget::sltHandleSplitterSettingsSave);
        }
    }
    /* [Re]start timer finally: */
    m_pSplitterSettingsSaveTimer->start();
}

void UIMachineToolsWidget::sltHandleSplitterSettingsSave()
{
    const QList<int> splitterSizes = m_pSplitter->sizes();
    gEDataManager->setSelectorWindowSplitterHints(splitterSizes);
}

void UIMachineToolsWidget::sltHandleChooserPaneSelectionChange()
{
    /* Recache current machine item information: */
    recacheCurrentMachineItemInformation();

    /* Let the parent know: */
    emit sigChooserPaneSelectionChange();

    /* Update tool restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        emit sigToolMenuUpdate(pItem);

    /* Fetch Chooser-pane selection class: */
    recalculateChooserPaneSelectionClass();
}

void UIMachineToolsWidget::sltHandleChooserPaneSelectionInvalidated()
{
    /* Recache current machine item information: */
    recacheCurrentMachineItemInformation(true /* fDontRaiseErrorPane */);
}

void UIMachineToolsWidget::sltHandleCloudMachineStateChange(const QUuid &uId)
{
    /* Acquire current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = isItemAccessible(pItem);

    /* If current item is Ok: */
    if (fCurrentItemIsOk)
    {
        /* If Error-pane is chosen currently => switch to Details: */
        if (toolPane()->currentTool() == UIToolType_Error)
            switchToolTo(UIToolType_Details);

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

void UIMachineToolsWidget::sltHandleToolMenuUpdate(UIVirtualMachineItem *pItem)
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

void UIMachineToolsWidget::sltHandleToolsMenuIndexChange(UIToolType enmType)
{
    /* Determine tool class of passed tool type: */
    const UIToolClass enmClass = UIToolStuff::castTypeToClass(enmType);

    /* For Machine tool class => switch tool-pane accordingly: */
    if (enmClass == UIToolClass_Machine)
        switchToolTo(enmType);
}

void UIMachineToolsWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIMachineToolsWidget::sltRetranslateUI);
    sltRetranslateUI();
}

void UIMachineToolsWidget::prepareWidgets()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
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

            /* Create container: */
            QWidget *pWidget = new QWidget(this);
            if (pWidget)
            {
                /* Create container layout: */
                QVBoxLayout *pSubLayout = new QVBoxLayout(pWidget);
                if (pSubLayout)
                {
                    /* Configure layout: */
                    pSubLayout->setContentsMargins(0, 0, 0, 0);
                    pSubLayout->setSpacing(0);

                    /* Create tool-menu: */
                    m_pMenu = new UITools(this, UIToolClass_Machine);
                    if (toolMenu())
                    {
                        /* Add into layout: */
                        pSubLayout->addWidget(toolMenu());
                    }

                    /* Create tool-pane: */
                    m_pPaneTools = new UIToolPane(this, UIToolClass_Machine, actionPool());
                    if (toolPane())
                    {
                        /* Add into splitter: */
                        pSubLayout->addWidget(toolPane());
                    }
                }

                /* Add into splitter: */
                m_pSplitter->addWidget(pWidget);
            }

            /* Set the initial distribution. The right site is bigger. */
            m_pSplitter->setStretchFactor(0, 2);
            m_pSplitter->setStretchFactor(1, 3);

            /* Add into layout: */
            pLayout->addWidget(m_pSplitter);
        }
    }

    /* Bring the VM list to the focus: */
    chooser()->setFocus();
}

void UIMachineToolsWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIMachineToolsWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIMachineToolsWidget::sltHandleMachineStateChange);
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIMachineToolsWidget::sltHandleSettingsExpertModeChange);

    /* Splitter connections: */
    connect(m_pSplitter, &QISplitter::splitterMoved,
            this, &UIMachineToolsWidget::sltHandleSplitterMove);

    /* Chooser-pane connections: */
    connect(chooser(), &UIChooser::sigSelectionChanged,
            this, &UIMachineToolsWidget::sltHandleChooserPaneSelectionChange);
    connect(chooser(), &UIChooser::sigSelectionInvalidated,
            this, &UIMachineToolsWidget::sltHandleChooserPaneSelectionInvalidated);
    connect(chooser(), &UIChooser::sigCloudMachineStateChange,
            this, &UIMachineToolsWidget::sltHandleCloudMachineStateChange);
    connect(chooser(), &UIChooser::sigToggleStarted,
            toolPane(), &UIToolPane::sigToggleStarted);
    connect(chooser(), &UIChooser::sigToggleFinished,
            toolPane(), &UIToolPane::sigToggleFinished);

    /* Tools-menu connections: */
    connect(this, &UIMachineToolsWidget::sigToolMenuUpdate,
            this, &UIMachineToolsWidget::sltHandleToolMenuUpdate);
    connect(toolMenu(), &UITools::sigSelectionChanged,
            this, &UIMachineToolsWidget::sltHandleToolsMenuIndexChange);
}

void UIMachineToolsWidget::loadSettings()
{
    /* Acquire & select tools currently chosen in the menu: */
    sltHandleToolsMenuIndexChange(toolMenu()->toolsType(UIToolClass_Machine));

    /* Update Machine tools restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        sltHandleToolMenuUpdate(pItem);

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
}

void UIMachineToolsWidget::recalculateChooserPaneSelectionClass()
{
    /* Calculate new status: */
    const SelectionType enmSelectedItemType = selectionType();
    const bool fCurrentItemIsOk = isItemAccessible();
    const bool fItemStarted = isItemStarted();

    /* Notify listeners about selection class change: */
    if (   m_enmSelectionType != enmSelectedItemType
        || m_fSelectedMachineItemAccessible != fCurrentItemIsOk
        || m_fSelectedMachineItemStarted != fItemStarted)
        emit sigChooserPaneSelectionClassChange();

    /* Remember new status: */
    m_enmSelectionType = enmSelectedItemType;
    m_fSelectedMachineItemAccessible = fCurrentItemIsOk;
    m_fSelectedMachineItemStarted = fItemStarted;
}

void UIMachineToolsWidget::cleanupConnections()
{
    /* Global COM event handlers: */
    disconnect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
               this, &UIMachineToolsWidget::sltHandleMachineStateChange);
    disconnect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
               this, &UIMachineToolsWidget::sltHandleSettingsExpertModeChange);

    /* Splitter connections: */
    disconnect(m_pSplitter, &QISplitter::splitterMoved,
               this, &UIMachineToolsWidget::sltHandleSplitterMove);

    /* Chooser-pane connections: */
    disconnect(chooser(), &UIChooser::sigSelectionChanged,
               this, &UIMachineToolsWidget::sltHandleChooserPaneSelectionChange);
    disconnect(chooser(), &UIChooser::sigSelectionInvalidated,
               this, &UIMachineToolsWidget::sltHandleChooserPaneSelectionInvalidated);
    disconnect(chooser(), &UIChooser::sigCloudMachineStateChange,
               this, &UIMachineToolsWidget::sltHandleCloudMachineStateChange);
    disconnect(chooser(), &UIChooser::sigToggleStarted,
               toolPane(), &UIToolPane::sigToggleStarted);
    disconnect(chooser(), &UIChooser::sigToggleFinished,
               toolPane(), &UIToolPane::sigToggleFinished);

    /* Tools-menu connections: */
    disconnect(this, &UIMachineToolsWidget::sigToolMenuUpdate,
               this, &UIMachineToolsWidget::sltHandleToolMenuUpdate);
    disconnect(toolMenu(), &UITools::sigSelectionChanged,
               this, &UIMachineToolsWidget::sltHandleToolsMenuIndexChange);
}

void UIMachineToolsWidget::recacheCurrentMachineItemInformation(bool fDontRaiseErrorPane /* = false */)
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
        /* If Error-pane is chosen currently => switch to Details: */
        if (toolPane()->currentTool() == UIToolType_Error)
            switchToolTo(UIToolType_Details);

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
