/* $Id$ */
/** @file
 * VBox Qt GUI - UIVirtualBoxWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIVirtualBoxWidget_h
#define FEQT_INCLUDED_SRC_manager_UIVirtualBoxWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QIToolBar;
class UIActionPool;
class UIChooser;
class UIGlobalToolsWidget;
class UIMachineToolsWidget;
class UIToolPane;
class UIVirtualBoxManager;
class UIVirtualMachineItem;

/** QWidget extension used as VirtualBox Manager Widget instance. */
class UIVirtualBoxWidget : public QWidget
{
    Q_OBJECT;

signals:

    /** @name Chooser pane stuff.
      * @{ */
        /** Notifies about Chooser-pane index change. */
        void sigChooserPaneIndexChange();
        /** Notifies about Chooser-pane group saving change. */
        void sigGroupSavingStateChanged();
        /** Notifies about Chooser-pane cloud update change. */
        void sigCloudUpdateStateChanged();

        /** Notifies about state change for cloud machine with certain @a uId. */
        void sigCloudMachineStateChange(const QUuid &uId);

        /** Notify listeners about start or show request. */
        void sigStartOrShowRequest();
        /** Notifies listeners about machine search widget visibility changed to @a fVisible. */
        void sigMachineSearchWidgetVisibilityChanged(bool fVisible);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Notifies about Global Tool type change. */
        void sigToolTypeChangeGlobal();
        /** Notifies about Machine Tool type change. */
        void sigToolTypeChangeMachine();
    /** @} */

    /** @name Tools / Media pane stuff.
      * @{ */
        /** Notifies listeners about creation procedure was requested. */
        void sigCreateMedium();
        /** Notifies listeners about copy procedure was requested for medium with specified @a uMediumId. */
        void sigCopyMedium(const QUuid &uMediumId);
    /** @} */

    /** @name Tools / Details pane stuff.
      * @{ */
        /** Notifies aboud Details-pane link clicked. */
        void sigMachineSettingsLinkClicked(const QString &strCategory, const QString &strControl, const QUuid &uId);
    /** @} */

    /** @name Tools / Snapshots pane stuff.
      * @{ */
        /** Notifies listeners about current Snapshots pane item change. */
        void sigCurrentSnapshotItemChange();
    /** @} */

    /** @name Tools / Generic pane stuff.
      * @{ */
        /** Notifies listeners about request to detach pane with tool type @p enmToolType. */
        void sigDetachToolPane(UIToolType enmToolType);
    /** @} */

public:

    /** Constructs VirtualBox Widget. */
    UIVirtualBoxWidget(UIVirtualBoxManager *pParent);
    /** Destructs VirtualBox Widget. */
    virtual ~UIVirtualBoxWidget() RT_OVERRIDE;

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool instance. */
        UIActionPool *actionPool() const { return m_pActionPool; }
    /** @} */

    /** @name Tool-bar stuff.
      * @{ */
        /** Updates tool-bar menu buttons. */
        void updateToolBarMenuButtons(bool fSeparateMenuSection);
    /** @} */

    /** @name Chooser pane stuff.
      * @{ */
        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether group item is selected. */
        bool isGroupItemSelected() const;
        /** Returns whether machine item is selected. */
        bool isMachineItemSelected() const;
        /** Returns whether local machine item is selected. */
        bool isLocalMachineItemSelected() const;
        /** Returns whether cloud machine item is selected. */
        bool isCloudMachineItemSelected() const;

        /** Returns whether single group is selected. */
        bool isSingleGroupSelected() const;
        /** Returns whether single local group is selected. */
        bool isSingleLocalGroupSelected() const;
        /** Returns whether single cloud provider group is selected. */
        bool isSingleCloudProviderGroupSelected() const;
        /** Returns whether single cloud profile group is selected. */
        bool isSingleCloudProfileGroupSelected() const;
        /** Returns whether all items of one group are selected. */
        bool isAllItemsOfOneGroupSelected() const;

        /** Returns full name of currently selected group. */
        QString fullGroupName() const;

        /** Returns whether group saving is in progress. */
        bool isGroupSavingInProgress() const;
        /** Returns whether at least one cloud profile currently being updated. */
        bool isCloudProfileUpdateInProgress() const;

        /** Opens group name editor. */
        void openGroupNameEditor();
        /** Disbands group. */
        void disbandGroup();
        /** Removes machine. */
        void removeMachine();
        /** Moves machine to a group with certain @a strName. */
        void moveMachineToGroup(const QString &strName = QString());
        /** Returns possible groups for machine with passed @a uId to move to. */
        QStringList possibleGroupsForMachineToMove(const QUuid &uId);
        /** Returns possible groups for group with passed @a strFullName to move to. */
        QStringList possibleGroupsForGroupToMove(const QString &strFullName);
        /** Refreshes machine. */
        void refreshMachine();
        /** Sorts group. */
        void sortGroup();
        /** Toggle machine search widget to be @a fVisible. */
        void setMachineSearchWidgetVisibility(bool fVisible);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Defines Global tools @a enmType and @a fMakeSureItsVisible if requested. */
        void setToolsTypeGlobal(UIToolType enmType, bool fMakeSureItsVisible = false);
        /** Returns Global tools type. */
        UIToolType toolsTypeGlobal() const;

        /** Defines Machine tools @a enmType. */
        void setToolsTypeMachine(UIToolType enmType);
        /** Returns Machine tools type. */
        UIToolType toolsTypeMachine() const;

        /** Returns a type of curent Global tool. */
        UIToolType currentGlobalTool() const;
        /** Returns a type of curent Machine tool. */
        UIToolType currentMachineTool() const;
        /** Returns whether Global tool of passed @a enmType is opened. */
        bool isGlobalToolOpened(UIToolType enmType) const;
        /** Returns whether Machine tool of passed @a enmType is opened. */
        bool isMachineToolOpened(UIToolType enmType) const;
        /** Closes Global tool of passed @a enmType. */
        void closeGlobalTool(UIToolType enmType);
        /** Closes Machine tool of passed @a enmType. */
        void closeMachineTool(UIToolType enmType);
    /** @} */

    /** @name Tools / Snapshot pane stuff.
      * @{ */
        /** Returns whether current-state item of Snapshot pane is selected. */
        bool isCurrentStateItemSelected() const;

        /** Returns currently selected snapshot ID if any. */
        QUuid currentSnapshotId();
    /** @} */

    /** @name Help browser stuff.
      * @{ */
        /** Returns the current help key word. */
        QString currentHelpKeyword() const;
    /** @} */

public slots:

    /** @name Tool-bar stuff.
      * @{ */
        /** Handles tool-bar context-menu request for passed @a position. */
        void sltHandleToolBarContextMenuRequest(const QPoint &position);
    /** @} */

private slots:

    /** @name General stuff.
      * @{ */
        /** Handles request to commit data. */
        void sltHandleCommitData();
    /** @} */

    /** @name Tool-bar stuff.
      * @{ */
        /** Updates tool-bar. */
        void sltUpdateToolbar();
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads settings. */
        void loadSettings();

        /** Updates toolbar. */
        void updateToolbar();

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Returns Global Tools Widget instance. */
        UIGlobalToolsWidget *globalToolsWidget() const;
        /** Returns Global Tool Pane reference. */
        UIToolPane *globalToolPane() const;

        /** Returns Machine Tools Widget reference. */
        UIMachineToolsWidget *machineToolsWidget() const;
        /** Returns Machine Tool Pane reference. */
        UIToolPane *machineToolPane() const;
        /** Returns Machine Chooser Pane reference. */
        UIChooser *chooser() const;
    /** @} */

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the main toolbar instance. */
    QIToolBar *m_pToolBar;

    /** Holds the Global Tools Widget instance. */
    UIGlobalToolsWidget *m_pGlobalToolsWidget;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIVirtualBoxWidget_h */
