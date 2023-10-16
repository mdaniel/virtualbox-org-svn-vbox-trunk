/* $Id$ */
/** @file
 * VBox Qt GUI - UIUpdateSettingsEditor class declaration.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>

/* GUI includes: */
#include "UIEditor.h"
#include "UIUpdateDefs.h"

/* Forward declarations: */
class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;

/** UIEditor sub-class used as a update settings editor. */
class SHARED_LIBRARY_STUFF UIUpdateSettingsEditor : public UIEditor
{
    Q_OBJECT;

public:

    /** Constructs editor passing @a pParent to the base-class. */
    UIUpdateSettingsEditor(QWidget *pParent = 0);

    /** Defines editor @a guiValue. */
    void setValue(const VBoxUpdateData &guiValue);
    /** Returns editor value. */
    VBoxUpdateData value() const;

protected:

    /** Filters out contents.
      * @param  fExpertMode  Brings whether settings expert mode is requested.
      * @param  strFilter    Brings the filter description should correspond to. */
    virtual void filterOut(bool fExpertMode, const QString &strFilter) RT_OVERRIDE;

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles whether update is @a fEnabled. */
    void sltHandleUpdateToggle(bool fEnabled);
    /** Handles update period change. */
    void sltHandleUpdatePeriodChange();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Returns whether check is enabled. */
    bool isCheckEnabled() const;
    /** Returns update period. */
    UpdatePeriodType updatePeriod() const;
    /** Returns update channel. */
    KUpdateChannel updateChannel() const;

    /** Fetches passed value. */
    void fetchValue();

    /** Holds the value to be set. */
    VBoxUpdateData  m_guiValue;

    /** @name Widgets
     * @{ */
        /** Holds the update check-box instance. */
        QCheckBox *m_pCheckBox;
        /** Holds the update settings widget instance. */
        QWidget   *m_pWidgetUpdateSettings;
        /** Holds the update period label instance. */
        QLabel    *m_pLabelUpdatePeriod;
        /** Holds the update period combo instance. */
        QComboBox *m_pComboUpdatePeriod;
        /** Holds the update date label instance. */
        QLabel    *m_pLabelUpdateDate;
        /** Holds the update date field instance. */
        QLabel    *m_pFieldUpdateDate;
        /** Holds the update filter label instance. */
        QLabel    *m_pLabelUpdateFilter;

        /** Holds the radio button group instance. */
        QButtonGroup                           *m_pRadioButtonGroup;
        /** Holds the radio button map instance. */
        QMap<KUpdateChannel, QAbstractButton*>  m_mapRadioButtons;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UIUpdateSettingsEditor_h */
