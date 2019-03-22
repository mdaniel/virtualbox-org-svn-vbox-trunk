/* $Id$ */
/** @file
 * VBox Qt GUI - UINameAndSystemEditor class implementation.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QComboBox>
# include <QGridLayout>
# include <QLabel>
# include <QVBoxLayout>

/* GUI includes: */
# include "QILineEdit.h"
# include "VBoxGlobal.h"
# include "UIFilePathSelector.h"
# include "UINameAndSystemEditor.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Defines the VM OS type ID. */
enum
{
    TypeID = Qt::UserRole + 1
};


UINameAndSystemEditor::UINameAndSystemEditor(QWidget *pParent, bool fChooseLocation /* = false */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_fChooseLocation(fChooseLocation)
    , m_fSupportsHWVirtEx(false)
    , m_fSupportsLongMode(false)
    , m_pLabelFamily(0)
    , m_pLabelType(0)
    , m_pIconType(0)
    , m_pNameLabel(0)
    , m_pPathLabel(0)
    , m_pNameLineEdit(0)
    , m_pPathSelector(0)
    , m_pComboFamily(0)
    , m_pComboType(0)
{
    /* Prepare: */
    prepare();
}

QString UINameAndSystemEditor::name() const
{
    if (!m_pNameLineEdit)
        return QString();
    return m_pNameLineEdit->text();
}

QString UINameAndSystemEditor::path() const
{
    if (!m_pPathSelector)
        return vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
    return m_pPathSelector->path();
}

void UINameAndSystemEditor::setName(const QString &strName)
{
    if (!m_pNameLineEdit)
        return;
    m_pNameLineEdit->setText(strName);
}

CGuestOSType UINameAndSystemEditor::type() const
{
    return m_enmType;
}

void UINameAndSystemEditor::setType(const CGuestOSType &enmType)
{
    // WORKAROUND:
    // We're getting here with a NULL enmType when creating new VMs.
    // Very annoying, so just workarounded for now.
    /** @todo find out the reason and way to fix that.. */
    if (enmType.isNull())
        return;

    /* Initialize variables: */
    const QString strFamilyId = enmType.GetFamilyId();
    const QString strTypeId = enmType.GetId();

    /* Get/check family index: */
    const int iFamilyIndex = m_pComboFamily->findData(strFamilyId, TypeID);
    AssertMsg(iFamilyIndex != -1, ("Invalid family ID: '%s'", strFamilyId.toLatin1().constData()));
    if (iFamilyIndex != -1)
        m_pComboFamily->setCurrentIndex(iFamilyIndex);

    /* Get/check type index: */
    const int iTypeIndex = m_pComboType->findData(strTypeId, TypeID);
    AssertMsg(iTypeIndex != -1, ("Invalid type ID: '%s'", strTypeId.toLatin1().constData()));
    if (iTypeIndex != -1)
        m_pComboType->setCurrentIndex(iTypeIndex);
}

void UINameAndSystemEditor::retranslateUi()
{
    m_pLabelFamily->setText(tr("&Type:"));
    m_pLabelType->setText(tr("&Version:"));
    m_pNameLabel->setText(tr("Name:"));
    if (m_pPathLabel)
        m_pPathLabel->setText(tr("Path:"));

    m_pComboFamily->setWhatsThis(tr("Selects the operating system family that "
                                    "you plan to install into this virtual machine."));
    m_pComboType->setWhatsThis(tr("Selects the operating system type that "
                                  "you plan to install into this virtual machine "
                                  "(called a guest operating system)."));
}

void UINameAndSystemEditor::sltFamilyChanged(int iIndex)
{
    /* Lock the signals of m_pComboType to prevent it's reaction on clearing: */
    m_pComboType->blockSignals(true);
    m_pComboType->clear();

    /* Acquire family ID: */
    const QString strFamilyId = m_pComboFamily->itemData(iIndex, TypeID).toString();

    /* Populate combo-box with OS types related to currently selected family id: */
    foreach (const CGuestOSType &comType, vboxGlobal().vmGuestOSTypeList(strFamilyId))
    {
        /* Skip 64bit OS types if hardware virtualization or long mode is not supported: */
        if (comType.GetIs64Bit() && (!m_fSupportsHWVirtEx || !m_fSupportsLongMode))
            continue;
        const int iIndex = m_pComboType->count();
        m_pComboType->insertItem(iIndex, comType.GetDescription());
        m_pComboType->setItemData(iIndex, comType.GetId(), TypeID);
    }

    /* Select the most recently chosen item: */
    if (m_currentIds.contains(strFamilyId))
    {
        const QString strTypeId = m_currentIds.value(strFamilyId);
        const int iTypeIndex = m_pComboType->findData(strTypeId, TypeID);
        if (iTypeIndex != -1)
            m_pComboType->setCurrentIndex(iTypeIndex);
    }
    /* Or select Windows 7 item for Windows family as default: */
    else if (strFamilyId == "Windows")
    {
        QString strDefaultID = "Windows7";
        if (ARCH_BITS == 64 && m_fSupportsHWVirtEx && m_fSupportsLongMode)
            strDefaultID += "_64";
        const int iIndexWin7 = m_pComboType->findData(strDefaultID, TypeID);
        if (iIndexWin7 != -1)
            m_pComboType->setCurrentIndex(iIndexWin7);
    }
    /* Or select Oracle Linux item for Linux family as default: */
    else if (strFamilyId == "Linux")
    {
        QString strDefaultID = "Oracle";
        if (ARCH_BITS == 64 && m_fSupportsHWVirtEx && m_fSupportsLongMode)
            strDefaultID += "_64";
        const int iIndexUbuntu = m_pComboType->findData(strDefaultID, TypeID);
        if (iIndexUbuntu != -1)
            m_pComboType->setCurrentIndex(iIndexUbuntu);
    }
    /* Else simply select the first one present: */
    else
        m_pComboType->setCurrentIndex(0);

    /* Update all the stuff: */
    sltTypeChanged(m_pComboType->currentIndex());

    /* Unlock the signals of m_pComboType: */
    m_pComboType->blockSignals(false);
}

void UINameAndSystemEditor::sltTypeChanged(int iIndex)
{
    /* Acquire type/family IDs: */
    const QString strTypeId = m_pComboType->itemData(iIndex, TypeID).toString();
    const QString strFamilyId = m_pComboFamily->itemData(m_pComboFamily->currentIndex(), TypeID).toString();

    /* Save the new selected OS type: */
    m_enmType = vboxGlobal().vmGuestOSType(strTypeId, strFamilyId);

    /* Update selected type pixmap: */
    m_pIconType->setPixmap(vboxGlobal().vmGuestOSTypePixmapDefault(strTypeId));

    /* Save the most recently used item: */
    m_currentIds[strFamilyId] = strTypeId;

    /* Notifies listeners about OS type change: */
    emit sigOsTypeChanged();
}

void UINameAndSystemEditor::prepare()
{
    /* Prepare this: */
    prepareThis();
    /* Prepare widgets: */
    prepareWidgets();
    /* Prepare connections: */
    prepareConnections();
    /* Apply language settings: */
    retranslateUi();
}

void UINameAndSystemEditor::prepareThis()
{
    /* Check if host supports (AMD-V or VT-x) and long mode: */
    CHost host = vboxGlobal().host();
    m_fSupportsHWVirtEx = host.GetProcessorFeature(KProcessorFeature_HWVirtEx);
    m_fSupportsLongMode = host.GetProcessorFeature(KProcessorFeature_LongMode);
}

void UINameAndSystemEditor::prepareWidgets()
{
    /* Create main-layout: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    if (pMainLayout)
    {
        pMainLayout->setContentsMargins(0, 0, 0, 0);

        int iRow = 0;

        /* Create name label: */
        m_pNameLabel = new QLabel;
        if (m_pNameLabel)
        {
            m_pNameLabel->setAlignment(Qt::AlignRight);
            m_pNameLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

            /* Add into layout: */
            pMainLayout->addWidget(m_pNameLabel, iRow, 0, 1, 1);
        }
        /* Create name editor: */
        m_pNameLineEdit = new QILineEdit;
        if (m_pNameLineEdit)
        {
            /* Add into layout: */
            pMainLayout->addWidget(m_pNameLineEdit, iRow, 1, 1, 2);
        }

        ++iRow;

        if (m_fChooseLocation)
        {
            /* Create path label: */
            m_pPathLabel = new QLabel;
            if (m_pPathLabel)
            {
                m_pPathLabel->setAlignment(Qt::AlignRight);
                m_pPathLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

                /* Add into layout: */
                pMainLayout->addWidget(m_pPathLabel, iRow, 0, 1, 1);
            }
            /* Create path selector: */
            m_pPathSelector = new UIFilePathSelector;
            if (m_pPathSelector)
            {
                QString strDefaultMachineFolder = vboxGlobal().virtualBox().GetSystemProperties().GetDefaultMachineFolder();
                m_pPathSelector->setPath(strDefaultMachineFolder);
                m_pPathSelector->setDefaultPath(strDefaultMachineFolder);

                /* Add into layout: */
                pMainLayout->addWidget(m_pPathSelector, iRow, 1, 1, 2);
            }

            ++iRow;
        }

        /* Create VM OS family label: */
        m_pLabelFamily = new QLabel;
        if (m_pLabelFamily)
        {
            m_pLabelFamily->setAlignment(Qt::AlignRight);
            m_pLabelFamily->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelFamily, iRow, 0);
        }

        int iIconRow = iRow;

        /* Create VM OS family combo: */
        m_pComboFamily = new QComboBox;
        if (m_pComboFamily)
        {
            m_pComboFamily->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pLabelFamily->setBuddy(m_pComboFamily);

            /* Add into layout: */
            pMainLayout->addWidget(m_pComboFamily, iRow, 1);
        }

        ++iRow;

        /* Create VM OS type label: */
        m_pLabelType = new QLabel;
        if (m_pLabelType)
        {
            m_pLabelType->setAlignment(Qt::AlignRight);
            m_pLabelType->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

            /* Add into layout: */
            pMainLayout->addWidget(m_pLabelType, iRow, 0);
        }
        /* Create VM OS type combo: */
        m_pComboType = new QComboBox;
        if (m_pComboType)
        {
            m_pComboType->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            m_pLabelType->setBuddy(m_pComboType);

            /* Add into layout: */
            pMainLayout->addWidget(m_pComboType, iRow, 1);
        }

        ++iRow;

        /* Create sub-layout: */
        QVBoxLayout *pLayoutIcon = new QVBoxLayout;
        if (pLayoutIcon)
        {
            /* Create VM OS type icon: */
            m_pIconType = new QLabel;
            if (m_pIconType)
            {
                m_pIconType->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

                /* Add into layout: */
                pLayoutIcon->addWidget(m_pIconType);
            }

            /* Add stretch to sub-layout: */
            pLayoutIcon->addStretch();

            /* Add into layout: */
            pMainLayout->addLayout(pLayoutIcon, iIconRow, 2, 2, 1);
        }
    }

    /* Initialize VM OS family combo
     * after all widgets were created: */
    prepareFamilyCombo();
}

void UINameAndSystemEditor::prepareFamilyCombo()
{
    /* Populate VM OS family combo: */
    const QList<QString> &familyIDs = vboxGlobal().vmGuestOSFamilyIDs();
    for (int i = 0; i < familyIDs.size(); ++i)
    {
        const QString &strFamilyId = familyIDs.at(i);
        m_pComboFamily->insertItem(i, vboxGlobal().vmGuestOSFamilyDescription(strFamilyId));
        m_pComboFamily->setItemData(i, strFamilyId, TypeID);
    }

    /* Choose the 1st item to be the current: */
    m_pComboFamily->setCurrentIndex(0);
    /* And update the linked widgets accordingly: */
    sltFamilyChanged(m_pComboFamily->currentIndex());
}

void UINameAndSystemEditor::prepareConnections()
{
    /* Prepare connections: */
    connect(m_pNameLineEdit, &QILineEdit::textChanged,
            this, &UINameAndSystemEditor::sigNameChanged);
    if (m_pPathSelector)
        connect(m_pPathSelector, &UIFilePathSelector::pathChanged,
                this, &UINameAndSystemEditor::sigPathChanged);
    connect(m_pComboFamily, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UINameAndSystemEditor::sltFamilyChanged);
    connect(m_pComboType, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &UINameAndSystemEditor::sltTypeChanged);
}

void UINameAndSystemEditor::setNameFieldValidator(const QString &strValidator)
{
    if (!m_pNameLineEdit)
        return;
    m_pNameLineEdit->setValidator(new QRegExpValidator(QRegExp(strValidator), this));
}
