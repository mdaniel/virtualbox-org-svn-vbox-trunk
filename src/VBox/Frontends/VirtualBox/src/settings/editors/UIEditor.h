/* $Id$ */
/** @file
 * VBox Qt GUI - UIEditor class declaration.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QTabWidget;

/** QWidget sub-class used as editor interface. */
class SHARED_LIBRARY_STUFF UIEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about visibility changes. */
    void sigVisibilityChange(bool fVisible);

public:

    /** Constructs editor to be injected into @a pTabWidget. */
    UIEditor(QTabWidget *pTabWidget);
    /** Constructs editor passing @a pParent to the base-class.
      * @param  fShowInBasicMode  Brings whether widget should be shown in basic mode. */
    UIEditor(QWidget *pParent = 0, bool fShowInBasicMode = false);

    /** Adds @a pEditor into list of sub-editor. */
    void addEditor(UIEditor *pEditor);

    /** Filters out contents.
      * @param  fExpertMode  Brings whether editor is in expert mode.
      * @param  strFilter    Brings the filter description should match to. */
    virtual void filterOut(bool fExpertMode, const QString &strFilter);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE {}

    /** Returns editor description which could be used to filter it in. */
    virtual QStringList description() const;

    /** Holds whether widget should be shown in basic mode. */
    bool  m_fShowInBasicMode;
    /** Holds whether editor is in expert mode. */
    bool  m_fInExpertMode;

    /** Holds the parent tab-widget if any. */
    QTabWidget *m_pTabWidget;

    /** Holds the list of sub-editors. */
    QList<UIEditor*> m_editors;
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIEditor_h */
