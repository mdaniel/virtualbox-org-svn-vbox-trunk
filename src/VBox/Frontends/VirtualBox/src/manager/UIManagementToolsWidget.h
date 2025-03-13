/* $Id$ */
/** @file
 * VBox Qt GUI - UIManagementToolsWidget class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_UIManagementToolsWidget_h
#define FEQT_INCLUDED_SRC_manager_UIManagementToolsWidget_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIActionPool;
class UIToolPane;

/** QWidget extension used as Management Tools Widget instance. */
class UIManagementToolsWidget : public QWidget
{
    Q_OBJECT;

signals:

    /** @name Tools pane stuff.
      * @{ */
        /** Notifies about Tool type change. */
        void sigToolTypeChange();
    /** @} */

public:

    /** Constructs Management Tools Widget passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool reference. */
    UIManagementToolsWidget(QWidget *pParent, UIActionPool *pActionPool);

    /** @name Tools pane stuff.
      * @{ */
        /** Returns tool-pane instance. */
        UIToolPane *toolPane() const;

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

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares widgets. */
        void prepareWidgets();
    /** @} */

    /** @name Common stuff.
      * @{ */
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const { return m_pActionPool; }
    /** @} */

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;

    /** Holds the tool-pane instance. */
    UIToolPane *m_pPane;
};

#endif /* !FEQT_INCLUDED_SRC_manager_UIManagementToolsWidget_h */
