/* $Id$ */
/** @file
 * VBox Qt GUI - UIMotherboardFeaturesEditor class implementation.
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

/* Qt includes: */
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

/* GUI includes: */
#include "UIIconPool.h"
#include "UIMessageCenter.h"
#include "UIMotherboardFeaturesEditor.h"


UIMotherboardFeaturesEditor::UIMotherboardFeaturesEditor(QWidget *pParent /* = 0 */)
    : UIEditor(pParent, true /* show in basic mode? */)
    , m_fWasInExpertMode(true)
    , m_fEnableIoApic(false)
    , m_fEnableUtcTime(false)
    , m_fEnableEfi(false)
    , m_fEnableSecureBoot(false)
    , m_pLabel(0)
    , m_pCheckBoxEnableIoApic(0)
    , m_pCheckBoxEnableUtcTime(0)
    , m_pCheckBoxEnableEfi(0)
    , m_pCheckBoxEnableSecureBoot(0)
    , m_pPushButtonResetSecureBoot(0)
{
    prepare();
}

void UIMotherboardFeaturesEditor::setEnableIoApic(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableIoApic != fOn)
    {
        m_fEnableIoApic = fOn;
        if (m_pCheckBoxEnableIoApic)
            m_pCheckBoxEnableIoApic->setCheckState(m_fEnableIoApic ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIMotherboardFeaturesEditor::isEnabledIoApic() const
{
    return   m_pCheckBoxEnableIoApic
           ? m_pCheckBoxEnableIoApic->checkState() == Qt::Checked
           : m_fEnableIoApic;
}

void UIMotherboardFeaturesEditor::setEnableUtcTime(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableUtcTime != fOn)
    {
        m_fEnableUtcTime = fOn;
        if (m_pCheckBoxEnableUtcTime)
            m_pCheckBoxEnableUtcTime->setCheckState(m_fEnableUtcTime ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIMotherboardFeaturesEditor::isEnabledUtcTime() const
{
    return   m_pCheckBoxEnableUtcTime
           ? m_pCheckBoxEnableUtcTime->checkState() == Qt::Checked
           : m_fEnableUtcTime;
}

void UIMotherboardFeaturesEditor::setEnableEfi(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableEfi != fOn)
    {
        m_fEnableEfi = fOn;
        if (m_pCheckBoxEnableEfi)
            m_pCheckBoxEnableEfi->setCheckState(m_fEnableEfi ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIMotherboardFeaturesEditor::isEnabledEfi() const
{
    return   m_pCheckBoxEnableEfi
           ? m_pCheckBoxEnableEfi->checkState() == Qt::Checked
           : m_fEnableEfi;
}

void UIMotherboardFeaturesEditor::setEnableSecureBoot(bool fOn)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fEnableSecureBoot != fOn)
    {
        m_fEnableSecureBoot = fOn;
        if (m_pCheckBoxEnableSecureBoot)
            m_pCheckBoxEnableSecureBoot->setCheckState(m_fEnableSecureBoot ? Qt::Checked : Qt::Unchecked);
    }
}

bool UIMotherboardFeaturesEditor::isEnabledSecureBoot() const
{
    return   m_pCheckBoxEnableSecureBoot
           ? m_pCheckBoxEnableSecureBoot->checkState() == Qt::Checked
           : m_fEnableSecureBoot;
}

bool UIMotherboardFeaturesEditor::isResetSecureBoot() const
{
    return   m_pPushButtonResetSecureBoot
           ? m_pPushButtonResetSecureBoot->property("clicked_once").toBool()
           : false;
}

int UIMotherboardFeaturesEditor::minimumLabelHorizontalHint() const
{
    return m_pLabel ? m_pLabel->minimumSizeHint().width() : 0;
}

void UIMotherboardFeaturesEditor::setMinimumLayoutIndent(int iIndent)
{
    if (m_pLayout)
        m_pLayout->setColumnMinimumWidth(0, iIndent);
}

void UIMotherboardFeaturesEditor::retranslateUi()
{
    if (m_pLabel)
        m_pLabel->setText(tr("Extended Features:"));
    if (m_pCheckBoxEnableIoApic)
    {
        m_pCheckBoxEnableIoApic->setText(tr("Enable &I/O APIC"));
        m_pCheckBoxEnableIoApic->setToolTip(tr("When checked, the virtual machine will support the Input Output APIC (I/O APIC), "
                                               "which may slightly decrease performance. Note: don't disable this feature "
                                               "after having installed a Windows guest operating system!"));
    }
    if (m_pCheckBoxEnableUtcTime)
    {
        m_pCheckBoxEnableUtcTime->setText(tr("Enable Hardware Clock in &UTC Time"));
        m_pCheckBoxEnableUtcTime->setToolTip(tr("When checked, the RTC device will report the time in UTC, otherwise in local "
                                                "(host) time. Unix usually expects the hardware clock to be set to UTC."));
    }
    if (m_pCheckBoxEnableEfi)
    {
        m_pCheckBoxEnableEfi->setText(tr("Enable &EFI (special OSes only)"));
        m_pCheckBoxEnableEfi->setToolTip(tr("When checked, the guest will support the Extended Firmware Interface (EFI), "
                                            "which is required to boot certain guest OSes. Non-EFI aware OSes will not be able "
                                            "to boot if this option is activated."));
    }
    if (m_pCheckBoxEnableSecureBoot)
    {
        m_pCheckBoxEnableSecureBoot->setText(tr("Enable &Secure Boot"));
        m_pCheckBoxEnableSecureBoot->setToolTip(tr("When checked, the secure boot emulation will be enabled."));
    }
    if (m_pPushButtonResetSecureBoot)
    {
        m_pPushButtonResetSecureBoot->setText(tr("&Reset Keys to Default"));
        m_pPushButtonResetSecureBoot->setToolTip(tr("Resets secure boot keys to default."));
    }
}

void UIMotherboardFeaturesEditor::handleFilterChange()
{
    /* Layout reinsertion is too expensive, do only if necessary: */
    if (m_fWasInExpertMode == m_fInExpertMode)
        return;
    m_fWasInExpertMode = m_fInExpertMode;

    /* Update visibility first of all (helps avoid flicker): */
    if (m_pCheckBoxEnableIoApic)
        m_pCheckBoxEnableIoApic->setVisible(m_fInExpertMode);
    if (m_pCheckBoxEnableUtcTime)
        m_pCheckBoxEnableUtcTime->setVisible(m_fInExpertMode);
    if (m_pCheckBoxEnableSecureBoot)
        m_pCheckBoxEnableSecureBoot->setVisible(m_fInExpertMode);
    if (m_pPushButtonResetSecureBoot)
        m_pPushButtonResetSecureBoot->setVisible(m_fInExpertMode);

    /* It's a grid-layout, we'll have to dance a little: */
    if (m_pLayout)
    {
        /* Remove the stuff from the layout first (backward direction): */
        m_pLayout->removeWidget(m_pPushButtonResetSecureBoot);
        m_pLayout->removeWidget(m_pCheckBoxEnableSecureBoot);
        m_pLayout->removeWidget(m_pCheckBoxEnableEfi);
        m_pLayout->removeWidget(m_pCheckBoxEnableUtcTime);
        m_pLayout->removeWidget(m_pCheckBoxEnableIoApic);

        /* Add it again: */
        if (m_fInExpertMode)
        {
            m_pLayout->addWidget(m_pCheckBoxEnableIoApic, 0, 1);
            m_pLayout->addWidget(m_pCheckBoxEnableUtcTime, 1, 1);
            m_pLayout->addWidget(m_pCheckBoxEnableEfi, 2, 1);
            m_pLayout->addWidget(m_pCheckBoxEnableSecureBoot, 3, 1);
            m_pLayout->addWidget(m_pPushButtonResetSecureBoot, 4, 1);
        }
        /* Special place for EFI: */
        else
            m_pLayout->addWidget(m_pCheckBoxEnableEfi, 0, 1);
    }
}

void UIMotherboardFeaturesEditor::sltHandleEnableEfiToggling()
{
    /* Acquire actual feature state: */
    const bool fOn = m_pCheckBoxEnableEfi
                   ? m_pCheckBoxEnableEfi->isChecked()
                   : false;

    /* Update corresponding controls: */
    if (m_pCheckBoxEnableSecureBoot)
        m_pCheckBoxEnableSecureBoot->setEnabled(fOn);

    /* Notify listeners: */
    emit sigChangedEfi();
    sltHandleEnableSecureBootToggling();
}

void UIMotherboardFeaturesEditor::sltHandleEnableSecureBootToggling()
{
    /* Acquire actual feature state: */
    const bool fOn =    m_pCheckBoxEnableEfi
                     && m_pCheckBoxEnableSecureBoot
                     && m_pPushButtonResetSecureBoot
                   ?    m_pCheckBoxEnableEfi->isChecked()
                     && m_pCheckBoxEnableSecureBoot->isChecked()
                     && !m_pPushButtonResetSecureBoot->property("clicked_once").toBool()
                   : false;

    /* Update corresponding controls: */
    if (m_pPushButtonResetSecureBoot)
        m_pPushButtonResetSecureBoot->setEnabled(fOn);

    /* Notify listeners: */
    emit sigChangedSecureBoot();
}

void UIMotherboardFeaturesEditor::sltResetSecureBoot()
{
    if (!m_pPushButtonResetSecureBoot->property("clicked_once").toBool())
    {
        if (msgCenter().confirmRestoringDefaultKeys())
        {
            m_pPushButtonResetSecureBoot->setProperty("clicked_once", true);
            sltHandleEnableSecureBootToggling();
        }
    }
}

void UIMotherboardFeaturesEditor::prepare()
{
    /* Prepare main layout: */
    m_pLayout = new QGridLayout(this);
    if (m_pLayout)
    {
        m_pLayout->setContentsMargins(0, 0, 0, 0);
        m_pLayout->setColumnStretch(1, 1);

        /* Prepare label: */
        m_pLabel = new QLabel(this);
        if (m_pLabel)
        {
            m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_pLayout->addWidget(m_pLabel, 0, 0);
        }
        /* Prepare 'enable IO APIC' check-box: */
        m_pCheckBoxEnableIoApic = new QCheckBox(this);
        if (m_pCheckBoxEnableIoApic)
        {
            connect(m_pCheckBoxEnableIoApic, &QCheckBox::stateChanged,
                    this, &UIMotherboardFeaturesEditor::sigChangedIoApic);
            m_pLayout->addWidget(m_pCheckBoxEnableIoApic, 0, 1);
        }
        /* Prepare 'enable UTC time' check-box: */
        m_pCheckBoxEnableUtcTime = new QCheckBox(this);
        if (m_pCheckBoxEnableUtcTime)
        {
            connect(m_pCheckBoxEnableUtcTime, &QCheckBox::stateChanged,
                    this, &UIMotherboardFeaturesEditor::sigChangedUtcTime);
            m_pLayout->addWidget(m_pCheckBoxEnableUtcTime, 1, 1);
        }
        /* Prepare 'enable EFI' check-box: */
        m_pCheckBoxEnableEfi = new QCheckBox(this);
        if (m_pCheckBoxEnableEfi)
        {
            connect(m_pCheckBoxEnableEfi, &QCheckBox::stateChanged,
                    this, &UIMotherboardFeaturesEditor::sltHandleEnableEfiToggling);
            m_pLayout->addWidget(m_pCheckBoxEnableEfi, 2, 1);
        }
        /* Prepare 'enable secure boot' check-box: */
        m_pCheckBoxEnableSecureBoot = new QCheckBox(this);
        if (m_pCheckBoxEnableSecureBoot)
        {
            connect(m_pCheckBoxEnableSecureBoot, &QCheckBox::stateChanged,
                    this, &UIMotherboardFeaturesEditor::sltHandleEnableSecureBootToggling);
            m_pLayout->addWidget(m_pCheckBoxEnableSecureBoot, 3, 1);
        }
        /* Prepare 'reset secure boot' tool-button: */
        m_pPushButtonResetSecureBoot = new QPushButton(this);
        if (m_pPushButtonResetSecureBoot)
        {
            m_pPushButtonResetSecureBoot->setIcon(UIIconPool::iconSet(":/refresh_16px.png"));
            connect(m_pPushButtonResetSecureBoot, &QPushButton::clicked,
                    this, &UIMotherboardFeaturesEditor::sltResetSecureBoot);
            m_pLayout->addWidget(m_pPushButtonResetSecureBoot, 4, 1);
        }
    }

    /* Fetch states: */
    sltHandleEnableEfiToggling();
    sltHandleEnableSecureBootToggling();

    /* Apply language settings: */
    retranslateUi();
}
