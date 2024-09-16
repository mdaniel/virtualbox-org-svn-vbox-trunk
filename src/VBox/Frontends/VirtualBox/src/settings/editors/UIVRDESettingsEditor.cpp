/* $Id$ */
/** @file
 * VBox Qt GUI - UIVRDESettingsEditor class implementation.
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

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

/* GUI includes: */
#include "UIConverter.h"
#include "UIVRDESettingsEditor.h"


UIVRDESettingsEditor::UIVRDESettingsEditor(QWidget *pParent /* = 0 */)
    : UIEditor(pParent)
    , m_fFeatureEnabled(false)
    , m_enmAuthType(KAuthType_Max)
    , m_fMultipleConnectionsAllowed(false)
    , m_pCheckboxFeature(0)
    , m_pWidgetSettings(0)
    , m_pLabelPort(0)
    , m_pEditorPort(0)
    , m_pLabelAuthMethod(0)
    , m_pComboAuthType(0)
    , m_pLabelTimeout(0)
    , m_pEditorTimeout(0)
    , m_pLabelOptions(0)
    , m_pCheckboxMultipleConnections(0)
{
    prepare();
}

void UIVRDESettingsEditor::setFeatureEnabled(bool fEnabled)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fFeatureEnabled != fEnabled)
    {
        m_fFeatureEnabled = fEnabled;
        if (m_pCheckboxFeature)
        {
            m_pCheckboxFeature->setChecked(m_fFeatureEnabled);
            sltHandleFeatureToggled(m_pCheckboxFeature->isChecked());
        }
    }
}

bool UIVRDESettingsEditor::isFeatureEnabled() const
{
    return m_pCheckboxFeature ? m_pCheckboxFeature->isChecked() : m_fFeatureEnabled;
}

void UIVRDESettingsEditor::setVRDEOptionsAvailable(bool fAvailable)
{
    if (m_pLabelOptions)
        m_pLabelOptions->setEnabled(fAvailable);
    if (m_pCheckboxMultipleConnections)
        m_pCheckboxMultipleConnections->setEnabled(fAvailable);
}

void UIVRDESettingsEditor::setPort(const QString &strPort)
{
    /* Update cached value and
     * line-edit if value has changed: */
    if (m_strPort != strPort)
    {
        m_strPort = strPort;
        if (m_pEditorPort)
            m_pEditorPort->setText(m_strPort);
    }
}

QString UIVRDESettingsEditor::port() const
{
    return m_pEditorPort ? m_pEditorPort->text() : m_strPort;
}

void UIVRDESettingsEditor::setAuthType(const KAuthType &enmType)
{
    /* Update cached value and
     * combo if value has changed: */
    if (m_enmAuthType != enmType)
    {
        m_enmAuthType = enmType;
        repopulateComboAuthType();
    }
}

KAuthType UIVRDESettingsEditor::authType() const
{
    return m_pComboAuthType ? m_pComboAuthType->currentData().value<KAuthType>() : m_enmAuthType;
}

void UIVRDESettingsEditor::setTimeout(const QString &strTimeout)
{
    /* Update cached value and
     * line-edit if value has changed: */
    if (m_strTimeout != strTimeout)
    {
        m_strTimeout = strTimeout;
        if (m_pEditorTimeout)
            m_pEditorTimeout->setText(m_strTimeout);
    }
}

QString UIVRDESettingsEditor::timeout() const
{
    return m_pEditorTimeout ? m_pEditorTimeout->text() : m_strTimeout;
}

void UIVRDESettingsEditor::setMultipleConnectionsAllowed(bool fAllowed)
{
    /* Update cached value and
     * check-box if value has changed: */
    if (m_fMultipleConnectionsAllowed != fAllowed)
    {
        m_fMultipleConnectionsAllowed = fAllowed;
        if (m_pCheckboxMultipleConnections)
            m_pCheckboxMultipleConnections->setChecked(m_fMultipleConnectionsAllowed);
    }
}

bool UIVRDESettingsEditor::isMultipleConnectionsAllowed() const
{
    return m_pCheckboxMultipleConnections ? m_pCheckboxMultipleConnections->isChecked() : m_fMultipleConnectionsAllowed;
}

void UIVRDESettingsEditor::sltRetranslateUI()
{
    if (m_pCheckboxFeature)
    {
        m_pCheckboxFeature->setText(tr("&Enable Server"));
        m_pCheckboxFeature->setToolTip(tr("When checked, the VM will act as a Remote Desktop Protocol (RDP) server, allowing "
                                          "remote clients to connect and operate the VM (when it is running) using a standard "
                                          "RDP client."));
    }

    if (m_pLabelPort)
        m_pLabelPort->setText(tr("Server &Port:"));
    if (m_pEditorPort)
        m_pEditorPort->setToolTip(tr("Holds the VRDP Server port number. You may specify 0 (zero), to select port 3389, the "
                                     "standard port for RDP."));

    if (m_pLabelAuthMethod)
        m_pLabelAuthMethod->setText(tr("Authentication &Method:"));
    if (m_pComboAuthType)
    {
        for (int iIndex = 0; iIndex < m_pComboAuthType->count(); ++iIndex)
        {
            const KAuthType enmType = m_pComboAuthType->itemData(iIndex).value<KAuthType>();
            m_pComboAuthType->setItemText(iIndex, gpConverter->toString(enmType));
        }
        m_pComboAuthType->setToolTip(tr("Selects the VRDP authentication method."));
    }

    if (m_pLabelTimeout)
        m_pLabelTimeout->setText(tr("Authentication &Timeout:"));
    if (m_pEditorTimeout)
        m_pEditorTimeout->setToolTip(tr("Holds the timeout for guest authentication, in milliseconds."));

    if (m_pLabelOptions)
        m_pLabelOptions->setText(tr("Extended Features:"));
    if (m_pCheckboxMultipleConnections)
    {
        m_pCheckboxMultipleConnections->setText(tr("&Allow Multiple Connections"));
        m_pCheckboxMultipleConnections->setToolTip(tr("When checked, multiple simultaneous connections to the VM are "
                                                      "permitted."));
    }
}

void UIVRDESettingsEditor::sltHandleFeatureToggled(bool fEnabled)
{
    /* Update widget availability: */
    if (m_pWidgetSettings)
        m_pWidgetSettings->setEnabled(fEnabled);

    /* Notify listeners: */
    emit sigChanged();
}

void UIVRDESettingsEditor::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIVRDESettingsEditor::prepareWidgets()
{
    /* Prepare main layout: */
    QGridLayout *pLayout = new QGridLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setColumnStretch(1, 1);

        /* Prepare 'feature' check-box: */
        m_pCheckboxFeature = new QCheckBox(this);
        if (m_pCheckboxFeature)
            pLayout->addWidget(m_pCheckboxFeature, 0, 0, 1, 2);

        /* Prepare 20-px shifting spacer: */
        QSpacerItem *pSpacerItem = new QSpacerItem(20, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        if (pSpacerItem)
            pLayout->addItem(pSpacerItem, 1, 0);

        /* Prepare 'settings' widget: */
        m_pWidgetSettings = new QWidget(this);
        if (m_pWidgetSettings)
        {
            /* Prepare 'settings' layout: */
            QGridLayout *pLayoutRemoteDisplaySettings = new QGridLayout(m_pWidgetSettings);
            if (pLayoutRemoteDisplaySettings)
            {
                pLayoutRemoteDisplaySettings->setContentsMargins(0, 0, 0, 0);
                pLayoutRemoteDisplaySettings->setColumnStretch(1, 1);

                /* Prepare 'port' label: */
                m_pLabelPort = new QLabel(m_pWidgetSettings);
                if (m_pLabelPort)
                {
                    m_pLabelPort->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutRemoteDisplaySettings->addWidget(m_pLabelPort, 0, 0);
                }
                /* Prepare 'port' editor: */
                m_pEditorPort = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorPort)
                {
                    if (m_pLabelPort)
                        m_pLabelPort->setBuddy(m_pEditorPort);
                    m_pEditorPort->setValidator(new QRegularExpressionValidator(
                        QRegularExpression("(([0-9]{1,5}(\\-[0-9]{1,5}){0,1}),)*([0-9]{1,5}(\\-[0-9]{1,5}){0,1})"), this));

                    pLayoutRemoteDisplaySettings->addWidget(m_pEditorPort, 0, 1, 1, 2);
                }

                /* Prepare 'auth type' label: */
                m_pLabelAuthMethod = new QLabel(m_pWidgetSettings);
                if (m_pLabelAuthMethod)
                {
                    m_pLabelAuthMethod->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutRemoteDisplaySettings->addWidget(m_pLabelAuthMethod, 1, 0);
                }
                /* Prepare 'auth type' combo: */
                m_pComboAuthType = new QComboBox(m_pWidgetSettings);
                if (m_pComboAuthType)
                {
                    if (m_pLabelAuthMethod)
                        m_pLabelAuthMethod->setBuddy(m_pComboAuthType);
                    m_pComboAuthType->setSizeAdjustPolicy(QComboBox::AdjustToContents);

                    pLayoutRemoteDisplaySettings->addWidget(m_pComboAuthType, 1, 1, 1, 2);
                }

                /* Prepare 'timeout' label: */
                m_pLabelTimeout = new QLabel(m_pWidgetSettings);
                if (m_pLabelTimeout)
                {
                    m_pLabelTimeout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutRemoteDisplaySettings->addWidget(m_pLabelTimeout, 2, 0);
                }
                /* Prepare 'timeout' editor: */
                m_pEditorTimeout = new QLineEdit(m_pWidgetSettings);
                if (m_pEditorTimeout)
                {
                    if (m_pLabelTimeout)
                        m_pLabelTimeout->setBuddy(m_pEditorTimeout);
                    m_pEditorTimeout->setValidator(new QIntValidator(this));

                    pLayoutRemoteDisplaySettings->addWidget(m_pEditorTimeout, 2, 1, 1, 2);
                }

                /* Prepare 'options' label: */
                m_pLabelOptions = new QLabel(m_pWidgetSettings);
                if (m_pLabelOptions)
                {
                    m_pLabelOptions->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    pLayoutRemoteDisplaySettings->addWidget(m_pLabelOptions, 3, 0);
                }
                /* Prepare 'multiple connections' check-box: */
                m_pCheckboxMultipleConnections = new QCheckBox(m_pWidgetSettings);
                if (m_pCheckboxMultipleConnections)
                    pLayoutRemoteDisplaySettings->addWidget(m_pCheckboxMultipleConnections, 3, 1);
            }

            pLayout->addWidget(m_pWidgetSettings, 1, 1, 1, 2);
        }
    }

    /* Update widget availability: */
    if (m_pCheckboxFeature)
        sltHandleFeatureToggled(m_pCheckboxFeature->isChecked());
}

void UIVRDESettingsEditor::prepareConnections()
{
    if (m_pCheckboxFeature)
        connect(m_pCheckboxFeature, &QCheckBox::toggled, this, &UIVRDESettingsEditor::sltHandleFeatureToggled);
    if (m_pEditorPort)
        connect(m_pEditorPort, &QLineEdit::textChanged, this, &UIVRDESettingsEditor::sigChanged);
    if (m_pEditorTimeout)
        connect(m_pEditorTimeout, &QLineEdit::textChanged, this, &UIVRDESettingsEditor::sigChanged);
}

void UIVRDESettingsEditor::repopulateComboAuthType()
{
    if (m_pComboAuthType)
    {
        /* Clear combo first of all: */
        m_pComboAuthType->clear();

        /// @todo get supported auth types (API not implemented), not hardcoded!
        QVector<KAuthType> authTypes = QVector<KAuthType>() << KAuthType_Null
                                                            << KAuthType_External
                                                            << KAuthType_Guest;

        /* Take into account currently cached value: */
        if (!authTypes.contains(m_enmAuthType))
            authTypes.prepend(m_enmAuthType);

        /* Populate combo finally: */
        foreach (const KAuthType &enmType, authTypes)
            m_pComboAuthType->addItem(gpConverter->toString(enmType), QVariant::fromValue(enmType));

        /* Look for proper index to choose: */
        const int iIndex = m_pComboAuthType->findData(QVariant::fromValue(m_enmAuthType));
        if (iIndex != -1)
            m_pComboAuthType->setCurrentIndex(iIndex);
    }
}
