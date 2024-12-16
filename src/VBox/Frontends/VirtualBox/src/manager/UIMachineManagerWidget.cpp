/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineManagerWidget class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "QISplitter.h"
#include "UIChooser.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIExtraDataManager.h"
#include "UIGlobalSession.h"
#include "UILoggingDefs.h"
#include "UINotificationCenter.h"
#include "UIToolPaneGlobal.h"
#include "UIToolPaneMachine.h"
#include "UITools.h"
#include "UITranslationEventListener.h"
#include "UIVirtualBoxEventHandler.h"
#include "UIMachineManagerWidget.h"
#include "UIVirtualMachineItemLocal.h"


UIMachineManagerWidget::UIMachineManagerWidget(UIToolPaneGlobal *pParent, UIActionPool *pActionPool)
    : QWidget(pParent)
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

UIMachineManagerWidget::~UIMachineManagerWidget()
{
    cleanup();
}

void UIMachineManagerWidget::setActive(bool fActive)
{
    m_pPaneTools->setActive(fActive);
}

UIChooser *UIMachineManagerWidget::chooser() const
{
    return m_pPaneChooser;
}

UIVirtualMachineItem *UIMachineManagerWidget::currentItem() const
{
    return m_pPaneChooser->currentItem();
}

QList<UIVirtualMachineItem*> UIMachineManagerWidget::currentItems() const
{
    return m_pPaneChooser->currentItems();
}

bool UIMachineManagerWidget::isItemAccessible(UIVirtualMachineItem *pItem /* = 0 */) const
{
    if (!pItem)
        pItem = currentItem();
    return pItem && pItem->accessible();
}

bool UIMachineManagerWidget::isGroupItemSelected() const
{
    return m_pPaneChooser->isGroupItemSelected();
}

bool UIMachineManagerWidget::isMachineItemSelected() const
{
    return m_pPaneChooser->isMachineItemSelected();
}

bool UIMachineManagerWidget::isLocalMachineItemSelected() const
{
    return m_pPaneChooser->isLocalMachineItemSelected();
}

bool UIMachineManagerWidget::isCloudMachineItemSelected() const
{
    return m_pPaneChooser->isCloudMachineItemSelected();
}

bool UIMachineManagerWidget::isSingleGroupSelected() const
{
    return m_pPaneChooser->isSingleGroupSelected();
}

bool UIMachineManagerWidget::isSingleLocalGroupSelected() const
{
    return m_pPaneChooser->isSingleLocalGroupSelected();
}

bool UIMachineManagerWidget::isSingleCloudProviderGroupSelected() const
{
    return m_pPaneChooser->isSingleCloudProviderGroupSelected();
}

bool UIMachineManagerWidget::isSingleCloudProfileGroupSelected() const
{
    return m_pPaneChooser->isSingleCloudProfileGroupSelected();
}

bool UIMachineManagerWidget::isAllItemsOfOneGroupSelected() const
{
    return m_pPaneChooser->isAllItemsOfOneGroupSelected();
}

UIMachineManagerWidget::SelectionType UIMachineManagerWidget::selectionType() const
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

QString UIMachineManagerWidget::fullGroupName() const
{
    return m_pPaneChooser->fullGroupName();
}

bool UIMachineManagerWidget::isGroupSavingInProgress() const
{
    return m_pPaneChooser->isGroupSavingInProgress();
}

bool UIMachineManagerWidget::isCloudProfileUpdateInProgress() const
{
    return m_pPaneChooser->isCloudProfileUpdateInProgress();
}

void UIMachineManagerWidget::openGroupNameEditor()
{
    m_pPaneChooser->openGroupNameEditor();
}

void UIMachineManagerWidget::disbandGroup()
{
    m_pPaneChooser->disbandGroup();
}

void UIMachineManagerWidget::removeMachine()
{
    m_pPaneChooser->removeMachine();
}

void UIMachineManagerWidget::moveMachineToGroup(const QString &strName /* = QString() */)
{
    m_pPaneChooser->moveMachineToGroup(strName);
}

QStringList UIMachineManagerWidget::possibleGroupsForMachineToMove(const QUuid &uId)
{
    return m_pPaneChooser->possibleGroupsForMachineToMove(uId);
}

QStringList UIMachineManagerWidget::possibleGroupsForGroupToMove(const QString &strFullName)
{
    return m_pPaneChooser->possibleGroupsForGroupToMove(strFullName);
}

void UIMachineManagerWidget::refreshMachine()
{
    m_pPaneChooser->refreshMachine();
}

void UIMachineManagerWidget::sortGroup()
{
    m_pPaneChooser->sortGroup();
}

void UIMachineManagerWidget::setMachineSearchWidgetVisibility(bool fVisible)
{
    m_pPaneChooser->setMachineSearchWidgetVisibility(fVisible);
}

UIToolPaneMachine *UIMachineManagerWidget::toolPane() const
{
    return m_pPaneTools;
}

UIToolType UIMachineManagerWidget::menuToolType() const
{
    return m_pMenuTools ? m_pMenuTools->toolsType() : UIToolType_Invalid;
}

void UIMachineManagerWidget::setMenuToolType(UIToolType enmType)
{
    m_pMenuTools->setToolsType(enmType);
}

UIToolType UIMachineManagerWidget::toolType() const
{
    return m_pPaneTools ? m_pPaneTools->currentTool() : UIToolType_Invalid;
}

bool UIMachineManagerWidget::isToolOpened(UIToolType enmType) const
{
    return m_pPaneTools ? m_pPaneTools->isToolOpened(enmType) : false;
}

void UIMachineManagerWidget::switchToolTo(UIToolType enmType)
{
    /* Open corresponding tool: */
    m_pPaneTools->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();
}

void UIMachineManagerWidget::closeTool(UIToolType enmType)
{
    m_pPaneTools->closeTool(enmType);
}

void UIMachineManagerWidget::switchToVMActivityPane(const QUuid &uMachineId)
{
    AssertPtrReturnVoid(m_pPaneChooser);
    AssertPtrReturnVoid(m_pMenuTools);
    m_pPaneChooser->setCurrentMachine(uMachineId);
    m_pMenuTools->setToolsType(UIToolType_VMActivity);
}

bool UIMachineManagerWidget::isCurrentStateItemSelected() const
{
    return m_pPaneTools->isCurrentStateItemSelected();
}

QUuid UIMachineManagerWidget::currentSnapshotId()
{
    return m_pPaneTools->currentSnapshotId();
}

QString UIMachineManagerWidget::currentHelpKeyword() const
{
    QString strHelpKeyword;
    if (isMachineItemSelected())
        strHelpKeyword = m_pPaneTools->currentHelpKeyword();
    return strHelpKeyword;
}

void UIMachineManagerWidget::sltRetranslateUI()
{
    /* Make sure chosen item fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIMachineManagerWidget::sltHandleCommitData()
{
    // WORKAROUND:
    // This will be fixed proper way during session management cleanup for Qt6.
    // But for now we will just cleanup connections which is Ok anyway.
    cleanupConnections();
}

void UIMachineManagerWidget::sltHandleMachineStateChange(const QUuid &uId)
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

void UIMachineManagerWidget::sltHandleSettingsExpertModeChange()
{
    /* Update tools restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        updateToolsMenu(pItem);
}

void UIMachineManagerWidget::sltHandleSplitterMove()
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
                    this, &UIMachineManagerWidget::sltHandleSplitterSettingsSave);
        }
    }
    /* [Re]start timer finally: */
    m_pSplitterSettingsSaveTimer->start();
}

void UIMachineManagerWidget::sltHandleSplitterSettingsSave()
{
    const QList<int> splitterSizes = m_pSplitter->sizes();
    LogRel2(("GUI: UIMachineManagerWidget: Saving splitter as: Size=%d,%d\n",
             splitterSizes.at(0), splitterSizes.at(1)));
    gEDataManager->setSelectorWindowSplitterHints(splitterSizes);
}

void UIMachineManagerWidget::sltHandleChooserPaneIndexChange()
{
    /* Let the parent know: */
    emit sigChooserPaneIndexChange();

    /* Update tools restrictions for currently selected item: */
    UIVirtualMachineItem *pItem = currentItem();
    if (pItem)
        updateToolsMenu(pItem);

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

void UIMachineManagerWidget::sltHandleChooserPaneSelectionInvalidated()
{
    recacheCurrentMachineItemInformation(true /* fDontRaiseErrorPane */);
}

void UIMachineManagerWidget::sltHandleCloudMachineStateChange(const QUuid &uId)
{
    /* Acquire current item: */
    UIVirtualMachineItem *pItem = currentItem();
    const bool fCurrentItemIsOk = isItemAccessible(pItem);

    /* If current item is Ok: */
    if (fCurrentItemIsOk)
    {
        /* If Error-pane is chosen currently => switch to tool currently chosen in Tools-menu: */
        if (m_pPaneTools->currentTool() == UIToolType_Error)
            switchToolTo(m_pMenuTools->toolsType());

        /* If we still have same item selected: */
        if (pItem && pItem->id() == uId)
        {
            /* Propagate current items to update the Details-pane: */
            m_pPaneTools->setItems(currentItems());
        }
    }
    else
    {
        /* Make sure Error pane raised: */
        if (m_pPaneTools->currentTool() != UIToolType_Error)
            m_pPaneTools->openTool(UIToolType_Error);

        /* If we still have same item selected: */
        if (pItem && pItem->id() == uId)
        {
            /* Propagate current items to update the Details-pane (in any case): */
            m_pPaneTools->setItems(currentItems());
            /* Propagate last access error to update the Error-pane (if machine selected but inaccessible): */
            m_pPaneTools->setErrorDetails(pItem->accessError());
        }
    }

    /* Pass the signal further: */
    emit sigCloudMachineStateChange(uId);
}

void UIMachineManagerWidget::sltHandleToolMenuRequested(const QPoint &position, UIVirtualMachineItem *pItem)
{
    /* Update tools menu beforehand: */
    UITools *pMenu = pItem ? m_pMenuTools : 0;
    AssertPtrReturnVoid(pMenu);
    if (pItem)
        updateToolsMenu(pItem);

    /* Compose popup-menu geometry first of all: */
    QRect ourGeo = QRect(position, pMenu->minimumSizeHint());
    /* Adjust location only to properly fit into available geometry space: */
    const QRect availableGeo = gpDesktop->availableGeometry(position);
    ourGeo = gpDesktop->normalizeGeometry(ourGeo, availableGeo, false /* resize? */);

    /* Move, resize and show: */
    pMenu->move(ourGeo.topLeft());
    pMenu->show();
    // WORKAROUND:
    // Don't want even to think why, but for Qt::Popup resize to
    // smaller size often being ignored until it is actually shown.
    pMenu->resize(ourGeo.size());
}

void UIMachineManagerWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Load settings: */
    loadSettings();

    /* Translate UI: */
    sltRetranslateUI();
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIMachineManagerWidget::sltRetranslateUI);

    /* Make sure current Chooser-pane index fetched: */
    sltHandleChooserPaneIndexChange();
}

void UIMachineManagerWidget::prepareWidgets()
{
    /* Create main-layout: */
    QHBoxLayout *pLayoutMain = new QHBoxLayout(this);
    if (pLayoutMain)
    {
        /* Configure layout: */
        pLayoutMain->setSpacing(0);
        pLayoutMain->setContentsMargins(0, 0, 0, 0);

        /* Create splitter: */
        m_pSplitter = new QISplitter;
        if (m_pSplitter)
        {
            /* Create Chooser-pane: */
            m_pPaneChooser = new UIChooser(this, actionPool());
            if (m_pPaneChooser)
            {
                /* Add into splitter: */
                m_pSplitter->addWidget(m_pPaneChooser);
            }

            /* Create right widget: */
            QWidget *pWidgetRight = new QWidget;
            if (pWidgetRight)
            {
                /* Create right-layout: */
                QVBoxLayout *pLayoutRight = new QVBoxLayout(pWidgetRight);
                if(pLayoutRight)
                {
                    /* Configure layout: */
                    pLayoutRight->setSpacing(0);
                    pLayoutRight->setContentsMargins(0, 0, 0, 0);

                    /* Create Tools-pane: */
                    m_pPaneTools = new UIToolPaneMachine(actionPool());
                    if (m_pPaneTools)
                    {
                        /// @todo make sure it's used properly
                        m_pPaneTools->setActive(true);

                        /* Add into layout: */
                        pLayoutRight->addWidget(m_pPaneTools, 1);
                    }
                }

                /* Add into splitter: */
                m_pSplitter->addWidget(pWidgetRight);
            }

            /* Set the initial distribution. The right site is bigger. */
            m_pSplitter->setStretchFactor(0, 2);
            m_pSplitter->setStretchFactor(1, 3);

            /* Add into layout: */
            pLayoutMain->addWidget(m_pSplitter);
        }

        /* Create Tools-menu: */
        m_pMenuTools = new UITools(this, UIToolClass_Machine, actionPool());
    }

    /* Create notification-center: */
    UINotificationCenter::create(this);

    /* Bring the VM list to the focus: */
    m_pPaneChooser->setFocus();
}

void UIMachineManagerWidget::prepareConnections()
{
    /* UICommon connections: */
    connect(&uiCommon(), &UICommon::sigAskToCommitData,
            this, &UIMachineManagerWidget::sltHandleCommitData);

    /* Global COM event handlers: */
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange,
            this, &UIMachineManagerWidget::sltHandleMachineStateChange);
    connect(gEDataManager, &UIExtraDataManager::sigSettingsExpertModeChange,
            this, &UIMachineManagerWidget::sltHandleSettingsExpertModeChange);

    /* Splitter connections: */
    connect(m_pSplitter, &QISplitter::splitterMoved,
            this, &UIMachineManagerWidget::sltHandleSplitterMove);

    /* Chooser-pane connections: */
    connect(m_pPaneChooser, &UIChooser::sigSelectionChanged,
            this, &UIMachineManagerWidget::sltHandleChooserPaneIndexChange);
    connect(m_pPaneChooser, &UIChooser::sigSelectionInvalidated,
            this, &UIMachineManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    connect(m_pPaneChooser, &UIChooser::sigToolMenuRequested,
            this, &UIMachineManagerWidget::sltHandleToolMenuRequested);
    connect(m_pPaneChooser, &UIChooser::sigCloudMachineStateChange,
            this, &UIMachineManagerWidget::sltHandleCloudMachineStateChange);
    connect(m_pPaneChooser, &UIChooser::sigToggleStarted,
            m_pPaneTools, &UIToolPaneMachine::sigToggleStarted);
    connect(m_pPaneChooser, &UIChooser::sigToggleFinished,
            m_pPaneTools, &UIToolPaneMachine::sigToggleFinished);

    /* Tools-pane connections: */
    connect(m_pPaneTools, &UIToolPaneMachine::sigLinkClicked,
            this, &UIMachineManagerWidget::sigMachineSettingsLinkClicked);

    /* Tools-menu connections: */
    connect(m_pMenuTools, &UITools::sigSelectionChanged,
            this, &UIMachineManagerWidget::sltHandleToolsMenuIndexChange);
}

void UIMachineManagerWidget::loadSettings()
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
        LogRel2(("GUI: UIMachineManagerWidget: Restoring splitter to: Size=%d,%d\n",
                 sizes.at(0), sizes.at(1)));
        m_pSplitter->setSizes(sizes);
    }

    /* Open tools last chosen in Tools-menu: */
    switchToolTo(m_pMenuTools->toolsType());
}

void UIMachineManagerWidget::cleanupConnections()
{
    /* Chooser-pane connections: */
    disconnect(m_pPaneChooser, &UIChooser::sigSelectionChanged,
               this, &UIMachineManagerWidget::sltHandleChooserPaneIndexChange);
    disconnect(m_pPaneChooser, &UIChooser::sigSelectionInvalidated,
               this, &UIMachineManagerWidget::sltHandleChooserPaneSelectionInvalidated);
    disconnect(m_pPaneChooser, &UIChooser::sigToolMenuRequested,
               this, &UIMachineManagerWidget::sltHandleToolMenuRequested);
    disconnect(m_pPaneChooser, &UIChooser::sigCloudMachineStateChange,
               this, &UIMachineManagerWidget::sltHandleCloudMachineStateChange);
    disconnect(m_pPaneChooser, &UIChooser::sigToggleStarted,
               m_pPaneTools, &UIToolPaneMachine::sigToggleStarted);
    disconnect(m_pPaneChooser, &UIChooser::sigToggleFinished,
               m_pPaneTools, &UIToolPaneMachine::sigToggleFinished);

    /* Tools-pane connections: */
    disconnect(m_pPaneTools, &UIToolPaneMachine::sigLinkClicked,
               this, &UIMachineManagerWidget::sigMachineSettingsLinkClicked);

    /* Tools-menu connections: */
    disconnect(m_pMenuTools, &UITools::sigSelectionChanged,
               this, &UIMachineManagerWidget::sltHandleToolsMenuIndexChange);
}

void UIMachineManagerWidget::cleanupWidgets()
{
    UINotificationCenter::destroy();
}

void UIMachineManagerWidget::cleanup()
{
    /* Ask sub-dialogs to commit data: */
    sltHandleCommitData();

    /* Cleanup everything: */
    cleanupWidgets();
}

void UIMachineManagerWidget::recacheCurrentMachineItemInformation(bool fDontRaiseErrorPane /* = false */)
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
        /* If Error-pane is chosen currently => switch to tool currently chosen in Tools-menu: */
        if (m_pPaneTools->currentTool() == UIToolType_Error)
            switchToolTo(m_pMenuTools->toolsType());

        /* Propagate current items to the Tools pane: */
        m_pPaneTools->setItems(currentItems());
    }
    /* Otherwise if we were not asked separately to calm down: */
    else if (!fDontRaiseErrorPane)
    {
        /* Make sure Error pane raised: */
        if (m_pPaneTools->currentTool() != UIToolType_Error)
            m_pPaneTools->openTool(UIToolType_Error);

        /* Propagate last access error to the Error-pane: */
        if (pItem)
            m_pPaneTools->setErrorDetails(pItem->accessError());
    }
}

void UIMachineManagerWidget::updateToolsMenu(UIVirtualMachineItem *pItem)
{
    /* Get current item state: */
    const bool fCurrentItemIsOk = isItemAccessible(pItem);

    /* Update machine tools restrictions: */
    QSet<UIToolType> restrictedTypes;
    const bool fExpertMode = gEDataManager->isSettingsInExpertMode();
    if (!fExpertMode)
        restrictedTypes << UIToolType_FileManager;
    if (pItem && pItem->itemType() != UIVirtualMachineItemType_Local)
        restrictedTypes << UIToolType_Snapshots
                        << UIToolType_Logs
                        << UIToolType_FileManager;
    if (restrictedTypes.contains(m_pMenuTools->toolsType()))
        m_pMenuTools->setToolsType(UIToolType_Details);
    const QList restrictions(restrictedTypes.begin(), restrictedTypes.end());
    m_pMenuTools->setRestrictedToolTypes(restrictions);
    /* Update machine menu items availability: */
    m_pMenuTools->setItemsEnabled(fCurrentItemIsOk);

    /* Take restrictions into account, closing all restricted tools: */
    foreach (const UIToolType &enmRestrictedType, restrictedTypes)
        m_pPaneTools->closeTool(enmRestrictedType);
}
