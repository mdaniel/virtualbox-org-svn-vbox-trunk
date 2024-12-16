/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineManagerWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIMachineManagerWidget_h
#define FEQT_INCLUDED_SRC_manager_UIMachineManagerWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QUuid>
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QTimer;
class QISplitter;
class UIActionPool;
class UIChooser;
class UIToolPaneGlobal;
class UIToolPaneMachine;
class UITools;
class UIVirtualMachineItem;

/** QWidget extension used as Machine Manager Widget instance. */
class UIMachineManagerWidget : public QWidget
{
    Q_OBJECT;

    /** Possible selection types. */
    enum SelectionType
    {
        SelectionType_Invalid,
        SelectionType_SingleLocalGroupItem,
        SelectionType_SingleCloudGroupItem,
        SelectionType_FirstIsLocalMachineItem,
        SelectionType_FirstIsCloudMachineItem
    };

signals:

    /** @name Chooser pane stuff.
      * @{ */
        /** Notifies about Chooser-pane index change. */
        void sigChooserPaneIndexChange();
        /** Notifies about Chooser-pane selection change. */
        void sigChooserPaneSelectionChange();

        /** Notifies about state change for cloud machine with certain @a uId. */
        void sigCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Notifies about Tool type change. */
        void sigToolTypeChange();
    /** @} */

    /** @name Tools / Details pane stuff.
      * @{ */
        /** Notifies aboud Details-pane link clicked. */
        void sigMachineSettingsLinkClicked(const QString &strCategory, const QString &strControl, const QUuid &uId);
    /** @} */

public:

    /** Constructs Virtual Machine Manager widget passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool reference. */
    UIMachineManagerWidget(UIToolPaneGlobal *pParent, UIActionPool *pActionPool);
    /** Destructs Virtual Machine Manager widget. */
    virtual ~UIMachineManagerWidget() RT_OVERRIDE;

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const { return m_pActionPool; }

        /** Makes manager active. */
        void setActive(bool fActive);
    /** @} */

    /** @name Chooser pane stuff.
      * @{ */
        /** Returns Chooser-pane instance. */
        UIChooser *chooser() const;

        /** Returns current-item. */
        UIVirtualMachineItem *currentItem() const;
        /** Returns a list of current-items. */
        QList<UIVirtualMachineItem*> currentItems() const;

        /** Returns whether passed @a pItem accessible, by default it's the current one. */
        bool isItemAccessible(UIVirtualMachineItem *pItem = 0) const;

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

        /** Returns current selection type. */
        SelectionType selectionType() const;

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
        /** Returns Tool-pane instance. */
        UIToolPaneMachine *toolPane() const;

        /** Returns menu tool type. */
        UIToolType menuToolType() const;
        /** Defines menu tool @a enmType. */
        void setMenuToolType(UIToolType enmType);

        /** Returns pane tool type. */
        UIToolType toolType() const;
        /** Returns whether pane has tool of passed @a enmType. */
        bool isToolOpened(UIToolType enmType) const;
        /** Switches pane to passed tool @a enmType. */
        void switchToolTo(UIToolType enmType);
        /** Closes pane tool of passed @a enmType. */
        void closeTool(UIToolType enmType);
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

private slots:

    /** @name Event handling stuff.
      * @{ */
        /** Handles translation event. */
        void sltRetranslateUI();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Handles request to commit data. */
        void sltHandleCommitData();
    /** @} */

    /** @name COM event handling stuff.
      * @{ */
        /** Handles CVirtualBox event about state change for machine with @a uId. */
        void sltHandleMachineStateChange(const QUuid &uId);

        /** Handles signal about settings expert mode change. */
        void sltHandleSettingsExpertModeChange();
    /** @} */

    /** @name Splitter stuff.
      * @{ */
        /** Handles signal about splitter move. */
        void sltHandleSplitterMove();
        /** Handles request to save splitter settings. */
        void sltHandleSplitterSettingsSave();
    /** @} */

    /** @name Chooser pane stuff.
      * @{ */
        /** Handles signal about Chooser-pane index change. */
        void sltHandleChooserPaneIndexChange();

        /** Handles signal about Chooser-pane selection invalidated. */
        void sltHandleChooserPaneSelectionInvalidated();

        /** Handles state change for cloud machine with certain @a uId. */
        void sltHandleCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Handles tool popup-menu request. */
        void sltHandleToolMenuRequested(const QPoint &position, UIVirtualMachineItem *pItem);

        /** Handles signal about Tools-menu index change.
          * @param  enmType  Brings current tool type. */
        void sltHandleToolsMenuIndexChange(UIToolType enmType) { switchToolTo(enmType); }

        /** Switches to VM Activity pane of machine with @a uMachineId. */
        void sltSwitchToVMActivityPane(const QUuid &uMachineId);
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

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups widgets. */
        void cleanupWidgets();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Recaches current machine item information.
          * @param  fDontRaiseErrorPane  Brings whether we should not raise error-pane. */
        void recacheCurrentMachineItemInformation(bool fDontRaiseErrorPane = false);

        /** Updates tools menu for @a pItem specified. */
        void updateToolsMenu(UIVirtualMachineItem *pItem);
    /** @} */

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the central splitter instance. */
    QISplitter        *m_pSplitter;
    /** Holds the Chooser-pane instance. */
    UIChooser         *m_pPaneChooser;
    /** Holds the Tools-pane instance. */
    UIToolPaneMachine *m_pPaneTools;
    /** Holds the Tools-menu instance. */
    UITools           *m_pMenuTools;

    /** Holds the last selection type. */
    SelectionType  m_enmSelectionType;
    /** Holds whether the last selected item was accessible. */
    bool           m_fSelectedMachineItemAccessible;

    /** Holds the splitter settings save timer. */
    QTimer *m_pSplitterSettingsSaveTimer;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIMachineManagerWidget_h */
