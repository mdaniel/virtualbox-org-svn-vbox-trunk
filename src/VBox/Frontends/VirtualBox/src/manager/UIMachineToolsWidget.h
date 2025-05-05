/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineToolsWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIMachineToolsWidget_h
#define FEQT_INCLUDED_SRC_manager_UIMachineToolsWidget_h
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
class UIToolPane;
class UITools;
class UIVirtualMachineItem;

/** QWidget extension used as Machine Tools Widget instance. */
class UIMachineToolsWidget : public QWidget
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
        /** Notifies about Chooser-pane selection change. */
        void sigChooserPaneSelectionChange();
        /** Notifies about Chooser-pane selection class change.
          * @note Every selection class change caused by selection change.
          *       But not every selection change causes class change. */
        void sigChooserPaneSelectionClassChange();

        /** Notifies about state change for cloud machine with certain @a uId. */
        void sigCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Notifies about required tool menu update for the @a pItem specified. */
        void sigToolMenuUpdate(UIVirtualMachineItem *pItem);

        /** Notifies about Tool type change. */
        void sigToolTypeChange();
    /** @} */

public:

    /** Constructs Machine Tools Widget passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool reference. */
    UIMachineToolsWidget(UIToolPane *pParent, UIActionPool *pActionPool);

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

        /** Returns whether single local group is selected. */
        bool isSingleLocalGroupSelected() const;
        /** Returns whether single cloud provider group is selected. */
        bool isSingleCloudProviderGroupSelected() const;
        /** Returns whether single cloud profile group is selected. */
        bool isSingleCloudProfileGroupSelected() const;

        /** Returns current selection type. */
        SelectionType selectionType() const;
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Returns tool-menu instance. */
        UITools *toolMenu() const;
        /** Returns tool-pane instance. */
        UIToolPane *toolPane() const;

        /** Returns menu tool type for the @a enmClass specified. */
        UIToolType menuToolType(UIToolClass enmClass) const;
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
        /** Handles signal about Chooser-pane selection change. */
        void sltHandleChooserPaneSelectionChange();

        /** Handles signal about Chooser-pane selection invalidated. */
        void sltHandleChooserPaneSelectionInvalidated();

        /** Handles state change for cloud machine with certain @a uId. */
        void sltHandleCloudMachineStateChange(const QUuid &uId);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Handles request for tool menu update for the @a pItem specified. */
        void sltHandleToolMenuUpdate(UIVirtualMachineItem *pItem);

        /** Handles signal about Tools-menu index change.
          * @param  enmType  Brings current tool type. */
        void sltHandleToolsMenuIndexChange(UIToolType enmType);
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
    /** @} */

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const { return m_pActionPool; }
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Recaches current machine item information.
          * @param  fDontRaiseErrorPane  Brings whether we should not raise error-pane. */
        void recacheCurrentMachineItemInformation(bool fDontRaiseErrorPane = false);
    /** @} */

    /** Holds the parent reference. */
    UIToolPane   *m_pParent;
    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the tools-menu instance. */
    UITools    *m_pMenu;
    /** Holds the central splitter instance. */
    QISplitter *m_pSplitter;
    /** Holds the Chooser-pane instance. */
    UIChooser  *m_pPaneChooser;
    /** Holds the Tools-pane instance. */
    UIToolPane *m_pPaneTools;

    /** Holds the last selection type. */
    SelectionType  m_enmSelectionType;
    /** Holds whether the last selected item was accessible. */
    bool           m_fSelectedMachineItemAccessible;
    /** Holds whether the last selected item was started. */
    bool           m_fSelectedMachineItemStarted;

    /** Holds the splitter settings save timer. */
    QTimer *m_pSplitterSettingsSaveTimer;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIMachineToolsWidget_h */
