/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class declaration.
 */

/*
 * Copyright (C) 2008-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_settings_editors_UINameAndSystemEditor_h
#define FEQT_INCLUDED_SRC_settings_editors_UINameAndSystemEditor_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIEditor.h"
#include "UIGuestOSType.h"

/* COM includes: */
#include "COMEnums.h"
#include "CGuestOSType.h"

/* Forward declarations: */
class QComboBox;
class QGridLayout;
class QLabel;
class QILineEdit;
class QString;
class UIFilePathSelector;
class UIMarkableLineEdit;

/** UIEditor sub-class providing complex editor for basic VM parameters. */
class SHARED_LIBRARY_STUFF UINameAndSystemEditor : public UIEditor
{
    Q_OBJECT;
    Q_PROPERTY(QString name READ name WRITE setName);

signals:

    /** Notifies listeners about VM name change. */
    void sigNameChanged(const QString &strNewName);

    /** Notifies listeners about VM path change. */
    void sigPathChanged(const QString &strPath);

    /** Notifies listeners about VM image change. */
    void sigImageChanged(const QString &strImage);

    /** Notifies listeners about edition change. */
    void sigEditionChanged(ulong selectedEditionIndex);

    /** Notifies listeners about VM OS family change. */
    void sigOSFamilyChanged(const QString &strFamilyId);
    /** Notifies listeners about VM OS type change. */
    void sigOsTypeChanged();

public:

    /** Constructs editor passing @a pParent to the base-class.
     * @param  fChooseName    Controls whether we should propose to choose name.
     * @param  fChoosePath    Controls whether we should propose to choose path.
     * @param  fChooseImage   Controls whether we should propose to choose image.
     * @param  fChooseEdition Controls whether we should propose to choose edition.
     * @param  fChooseType    Controls whether we should propose to choose type. */
    UINameAndSystemEditor(QWidget *pParent,
                          bool fChooseName = true,
                          bool fChoosePath = false,
                          bool fChooseImage = false,
                          bool fChooseEdition = false,
                          bool fChooseType = true);

    /** Defines minimum layout @a iIndent. */
    void setMinimumLayoutIndent(int iIndent);

    /** Defines whether VM name stuff is @a fEnabled. */
    void setNameStuffEnabled(bool fEnabled);
    /** Defines whether VM path stuff is @a fEnabled. */
    void setPathStuffEnabled(bool fEnabled);
    /** Defines whether VM OS type stuff is @a fEnabled. */
    void setOSTypeStuffEnabled(bool fEnabled);

    /** Defines whether edition selector is @a fEnabled. */
    void setEditionSelectorEnabled(bool fEnabled);
    /** Returns whether edition selector is enabled. */
    bool isEditionsSelectorEmpty() const;

    /** Defines the VM @a strName. */
    void setName(const QString &strName);
    /** Returns the VM name. */
    QString name() const;

    /** Defines the VM @a strPath. */
    void setPath(const QString &strPath);
    /** Returns path string selected by the user. */
    QString path() const;

    /** Sets image path. */
    void setISOImagePath(const QString &strPath);
    /** Returns image string selected by the user. */
    QString ISOImagePath() const;

    /* strTypeId should be one of the type ids defined in Global.cpp and returned by IGuestOSType::getId(). */
    bool setGuestOSTypeByTypeId(const QString &strTypeId);
    /** Returns the VM OS family ID. */
    QString familyId() const;
    /** Returns the VM OS type ID. */
    QString typeId() const;

    /** Passes the @p fError to QILineEdit::mark(bool) effectively marking it for error. */
    void markNameEditor(bool fError);
    /** Passes the @p fError and @a strErrorMessage to UIFilePathSelector::mark(bool)
      *  effectively changing the background color and error-text. */
    void markImageEditor(bool fError, const QString &strErrorMessage);

    /** @p names and @p indices are parallel array storing edition names and their indices, respectively.*/
    void setEditionNameAndIndices(const QVector<QString> &names, const QVector<ulong> &ids);

    /** Returns 1st column width. */
    int firstColumnWidth() const;

protected:

    /** Handles translation event. */
    virtual void retranslateUi() RT_OVERRIDE;

private slots:

    /** Handles VM OS edition @a iIndex change. */
    void sltSelectedEditionsChanged(int);

    /** Handles VM OS family @a iIndex change. */
    void sltFamilyChanged(int index);
    /** Handles VM OS @a strSubtype change. */
    void sltSubtypeChanged(const QString &strSubtype);
    /** Handles VM OS type @a iIndex change. */
    void sltTypeChanged(int iIndex);

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Prepares connections. */
    void prepareConnections();

    /** Returns selected editions index. */
    ulong selectedEditionIndex() const;

    /** Prepares VM OS family combo. */
    void prepareFamilyCombo();
    /** Pupulates VM OS type combo.
      * @param  types  Brings the list of type pairs. */
    void populateTypeCombo(const QList<QPair<QString, QString> > &typeList);

    /** Selects preferred type. */
    void selectPreferredType();

    /** @name Arguments
     * @{ */
        /** Holds whether we should propose to choose a name. */
        bool  m_fChooseName;
        /** Holds whether we should propose to choose a path. */
        bool  m_fChoosePath;
        /** Holds whether we should propose to choose an image. */
        bool  m_fChooseImage;
        /** Holds whether we should propose to choose an edition. */
        bool  m_fChooseEdition;
        /** Holds whether we should propose to choose a type. */
        bool  m_fChooseType;
    /** @} */

    /** @name Values
     * @{ */
        /** Holds the VM OS family ID. */
        QString  m_strFamilyId;
        /** Holds the VM OS subtype. */
        QString  m_strSubtype;
        /** Holds the VM OS type ID. */
        QString  m_strTypeId;

        /** Holds the currently chosen OS type IDs on per-family basis. */
        QMap<QString, QString>  m_currentIds;
    /** @} */

    /** @name Widgets
     * @{ */
        /** Holds the main layout instance. */
        QGridLayout *m_pLayout;

        /** Holds the VM name label instance. */
        QLabel             *m_pLabelName;
        /** Holds the VM name editor instance. */
        UIMarkableLineEdit *m_pEditorName;

        /** Holds the VM path label instance. */
        QLabel             *m_pLabelPath;
        /** Holds the VM path editor instance. */
        UIFilePathSelector *m_pSelectorPath;

        /** Holds the ISO image label instance. */
        QLabel             *m_pLabelImage;
        /** Holds the file selector for ISO image. */
        UIFilePathSelector *m_pSelectorImage;

        /** Holds the edition label instance. */
        QLabel    *m_pLabelEdition;
        /** Holds the VM OS edition combo (currently only Windows ISO have this). */
        QComboBox *m_pComboEdition;

        /** Holds the VM OS family label instance. */
        QLabel    *m_pLabelFamily;
        /** Holds the VM OS family combo instance. */
        QComboBox *m_pComboFamily;
        /** Holds the VM OS subtype label instance. */
        QLabel    *m_pLabelSubtype;
        /** Holds the VM OS subtype combo instance. */
        QComboBox *m_pComboSubtype;
        /** Holds the VM OS type label instance. */
        QLabel    *m_pLabelType;
        /** Holds the VM OS type combo instance. */
        QComboBox *m_pComboType;
        /** Holds the VM OS type icon instance. */
        QLabel    *m_pIconType;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_editors_UINameAndSystemEditor_h */
