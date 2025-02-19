/* $Id$ */
/** @file
 * VBox Qt GUI - UITools class declaration.
 */

/*
 * Copyright (C) 2012-2024 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_manager_tools_UITools_h
#define FEQT_INCLUDED_SRC_manager_tools_UITools_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI icludes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QVBoxLayout;
class UIActionPool;
class UIToolsModel;
class UIToolsView;

/** QWidget extension used as VM Tools-pane. */
class UITools : public QWidget
{
    Q_OBJECT;

signals:

    /** @name General stuff.
      * @{ */
        /** Notifies listeners about selection changed.
          * @param  enmType  Brings current tool type. */
        void sigSelectionChanged(UIToolType enmType);
    /** @} */

public:

    /** Constructs Tools-pane passing @a pParent to the base-class.
      * @param  enmClass     Brings the tools class, it will be fixed one.
      * @param  pActionPool  Brings the action-pool reference.
      * @param  theFlags     Brings the widget flags. */
    UITools(QWidget *pParent,
            UIToolClass enmClass,
            UIActionPool *pActionPool,
            Qt::WindowFlags theFlags = Qt::Popup);
    /** Destructs Tools-pane. */
    virtual ~UITools();

    /** @name General stuff.
      * @{ */
        /** Defines current tools @a enmType. */
        void setToolsType(UIToolType enmType);
        /** Returns current tools type for the @a enmClass specified. */
        UIToolType toolsType(UIToolClass enmClass) const;

        /** Defines whether tool items @a fEnabled. */
        void setItemsEnabled(bool fEnabled);
        /** Returns whether tool items enabled. */
        bool isItemsEnabled() const;

        /** Defines restructed tool @a types for the @a enmClass specified. */
        void setRestrictedToolTypes(UIToolClass enmClass, const QList<UIToolType> &types);
    /** @} */

private:

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares contents. */
        void prepareContents();
        /** Prepares model. */
        void prepareModel();
        /** Prepares view. */
        void prepareView();
        /** Prepares connections. */
        void prepareConnections();
        /** Inits model. */
        void initModel();

        /** Cleanups connections. */
        void cleanupConnections();
        /** Cleanups view. */
        void cleanupView();
        /** Cleanups model. */
        void cleanupModel();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the tools class. */
        const UIToolClass  m_enmClass;

        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;

        /** Holds whether tools represented as popup. */
        const bool  m_fPopup;

        /** Holds the main layout instance. */
        QVBoxLayout  *m_pMainLayout;
        /** Holds the Tools-model instance. */
        UIToolsModel *m_pToolsModel;
        /** Holds the Tools-view instance. */
        UIToolsView  *m_pToolsView;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_tools_UITools_h */
