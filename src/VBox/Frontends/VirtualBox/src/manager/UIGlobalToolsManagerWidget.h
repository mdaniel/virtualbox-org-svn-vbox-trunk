/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalToolsManagerWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIGlobalToolsManagerWidget_h
#define FEQT_INCLUDED_SRC_manager_UIGlobalToolsManagerWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIActionPool;
class UIChooser;
class UIMachineManagerWidget;
class UIToolPaneGlobal;
class UIToolPaneMachine;
class UITools;
class UIVirtualBoxManagerAdvancedWidget;

/** QWidget extension used as Global Tools Manager Widget instance. */
class UIGlobalToolsManagerWidget : public QWidget
{
    Q_OBJECT;

signals:

    /** @name Tools pane stuff.
      * @{ */
        /** Notifies about Tool type change. */
        void sigToolTypeChange();
    /** @} */

public:

    /** Constructs Global Tools Manager widget passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool reference.  */
    UIGlobalToolsManagerWidget(UIVirtualBoxManagerAdvancedWidget *pParent, UIActionPool *pActionPool);
    /** Destructs Global Tools Manager widget. */
    virtual ~UIGlobalToolsManagerWidget() RT_OVERRIDE;

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const { return m_pActionPool; }
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Returns tool-pane instance. */
        UIToolPaneGlobal *toolPane() const;
        /** Returns Machine Manager reference. */
        UIMachineManagerWidget *machineManager() const;

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

    /** @name Help browser stuff.
      * @{ */
        /** Returns the current help key word. */
        QString currentHelpKeyword() const;
    /** @} */

private slots:

    /** @name General stuff.
      * @{ */
        /** Handles request to commit data. */
        void sltHandleCommitData();
    /** @} */

    /** @name COM event handling stuff.
      * @{ */
        /** Handles signal about settings expert mode change. */
        void sltHandleSettingsExpertModeChange();
    /** @} */

    /** @name Chooser pane stuff.
      * @{ */
        /** Handles state change for cloud profile with certain @a strProviderShortName and @a strProfileName. */
        void sltHandleCloudProfileStateChange(const QString &strProviderShortName,
                                              const QString &strProfileName);
    /** @} */

    /** @name Tools pane stuff.
      * @{ */
        /** Handles signal about Tools-menu index change.
          * @param  enmType  Brings current tool type. */
        void sltHandleToolsMenuIndexChange(UIToolType enmType);

        /** Handles signal requesting switch to Activities tool. */
        void sltSwitchToActivitiesTool();
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
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Tools stuff.
      * @{ */
        /** Returns tool-menu instance. */
        UITools *toolMenu() const;
        /** Returns Machine Manager's Chooser-pane reference. */
        UIChooser *chooser() const;
        /** Returns Machine Manager's Tool-pane instance. */
        UIToolPaneMachine *toolPaneMachine() const;

        /** Updates tools menu. */
        void updateToolsMenu();
    /** @} */

    /** Holds the action-pool instance. */
    UIActionPool *m_pActionPool;

    /** Holds the Tools-menu instance. */
    UITools          *m_pMenu;
    /** Holds the Tools-pane instance. */
    UIToolPaneGlobal *m_pPane;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIGlobalToolsManagerWidget_h */
