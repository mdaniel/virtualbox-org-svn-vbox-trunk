/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMDiskPage class implementation.
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
#include <QButtonGroup>
#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "QIToolButton.h"
#include "UIIconPool.h"
#include "UIGuestOSType.h"
#include "UIMediaComboBox.h"
#include "UIMediumSelector.h"
#include "UIMediumSizeEditor.h"
#include "UIGlobalSession.h"
#include "UIWizardNewVMDiskPage.h"
#include "UIWizardDiskEditors.h"
#include "UIWizardNewVM.h"

/* COM includes: */
#include "CSystemProperties.h"

QUuid UIWizardNewVMDiskCommon::getWithFileOpenDialog(const QString &strOSTypeID,
                                                     const QString &strMachineFolder,
                                                     QWidget *pCaller, UIActionPool *pActionPool)
{
    QUuid uMediumId;
    int returnCode = UIMediumSelector::openMediumSelectorDialog(pCaller, UIMediumDeviceType_HardDisk,
                                                         QUuid() /* current medium id */,
                                                         uMediumId,
                                                         strMachineFolder,
                                                         QString() /* strMachineName */,
                                                         strOSTypeID,
                                                         false /* don't show/enable the create action: */,
                                                         QUuid() /* Machinie Id */, pActionPool);
    if (returnCode != static_cast<int>(UIMediumSelector::ReturnCode_Accepted))
        return QUuid();
    return uMediumId;
}

UIWizardNewVMDiskPage::UIWizardNewVMDiskPage(UIActionPool *pActionPool, const QString strHelpKeyword /* = QString() */)
    : UINativeWizardPage(strHelpKeyword)
    , m_pMediumSizeEditorLabel(0)
    , m_pMediumSizeEditor(0)
    , m_fVDIFormatFound(false)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(gpGlobalSession->virtualBox().GetSystemProperties().GetInfoVDSize())
    , m_pActionPool(pActionPool)
{
    prepare();
}

void UIWizardNewVMDiskPage::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    pMainLayout->addWidget(createDiskWidgets());

    pMainLayout->addStretch();
    createConnections();
}

QWidget *UIWizardNewVMDiskPage::createNewDiskWidgets()
{
    QWidget *pWidget = new QWidget;
    if (pWidget)
    {
        QVBoxLayout *pLayout = new QVBoxLayout(pWidget);
        if (pLayout)
        {
            pLayout->setContentsMargins(0, 0, 0, 0);

            /* Prepare size layout: */
            QGridLayout *pSizeLayout = new QGridLayout;
            if (pSizeLayout)
            {
                pSizeLayout->setContentsMargins(0, 0, 0, 0);

                /* Prepare Hard disk size label: */
                m_pMediumSizeEditorLabel = new QLabel(pWidget);
                if (m_pMediumSizeEditorLabel)
                {
                    m_pMediumSizeEditorLabel->setAlignment(Qt::AlignRight);
                    m_pMediumSizeEditorLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                    pSizeLayout->addWidget(m_pMediumSizeEditorLabel, 0, 0, Qt::AlignBottom);
                }
                /* Prepare Hard disk size editor: */
                m_pMediumSizeEditor = new UIMediumSizeEditor(pWidget);
                if (m_pMediumSizeEditor)
                {
                    m_pMediumSizeEditorLabel->setBuddy(m_pMediumSizeEditor);
                    pSizeLayout->addWidget(m_pMediumSizeEditor, 0, 1, 2, 1);
                }
                pLayout->addLayout(pSizeLayout);
            }
        }
    }
    return pWidget;
}

void UIWizardNewVMDiskPage::createConnections()
{
    if (m_pMediumSizeEditor)
        connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMDiskPage::sltHandleSizeEditorChange);
}

void UIWizardNewVMDiskPage::sltRetranslateUI()
{
    setTitle(UIWizardNewVM::tr("Specify virtual hard disk"));

    if (m_pMediumSizeEditorLabel)
        m_pMediumSizeEditorLabel->setText(UIWizardNewVM::tr("D&isk Size"));
}

void UIWizardNewVMDiskPage::initializePage()
{
    sltRetranslateUI();

    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturnVoid(pWizard);

    LONG64 iRecommendedSize = 0;

    if (!m_userModifiedParameters.contains("SelectedDiskSource"))
    {
        iRecommendedSize = gpGlobalSession->guestOSTypeManager().getRecommendedHDD(pWizard->guestOSTypeId());
        if (iRecommendedSize != 0)
        {
            pWizard->setDiskSource(SelectedDiskSource_New);
            pWizard->setEmptyDiskRecommended(false);
        }
        else
        {
            pWizard->setDiskSource(SelectedDiskSource_Empty);
            pWizard->setEmptyDiskRecommended(true);
        }
    }

    if (!m_fVDIFormatFound)
    {
        /* We do not have any UI elements for HDD format selection since we default to VDI in case of guided wizard mode: */
        CSystemProperties properties = gpGlobalSession->virtualBox().GetSystemProperties();
        const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
        foreach (const CMediumFormat &format, formats)
        {
            if (format.GetName() == "VDI")
            {
                pWizard->setMediumFormat(format);
                m_fVDIFormatFound = true;
            }
        }
        if (!m_fVDIFormatFound)
            AssertMsgFailed(("No medium format corresponding to VDI could be found!"));
    }
    QString strDefaultExtension =  UIWizardDiskEditors::defaultExtension(pWizard->mediumFormat(), KDeviceType_HardDisk);

    /* We set the medium name and path according to machine name/path and do not allow user change these in the guided mode: */
    QString strDefaultName = pWizard->machineFileName().isEmpty() ? QString("NewVirtualDisk1") : pWizard->machineFileName();
    const QString &strMachineFolder = pWizard->machineFolder();
    QString strMediumPath =
        UIWizardDiskEditors::constructMediumFilePath(UIWizardDiskEditors::appendExtension(strDefaultName,
                                                                                          strDefaultExtension), strMachineFolder);
    pWizard->setMediumPath(strMediumPath);

    /* Set the recommended disk size if user has already not done so: */
    if (m_pMediumSizeEditor && !m_userModifiedParameters.contains("MediumSize"))
    {
        m_pMediumSizeEditor->blockSignals(true);
        m_pMediumSizeEditor->setMediumSize(iRecommendedSize);
        m_pMediumSizeEditor->blockSignals(false);
        pWizard->setMediumSize(iRecommendedSize);
    }

    /* Initialize medium variant parameter of the wizard (only if user has not touched the checkbox yet): */
    if (!m_userModifiedParameters.contains("MediumVariant"))
    {
        pWizard->setMediumVariant((qulonglong)KMediumVariant_Standard);
    }
}

bool UIWizardNewVMDiskPage::isComplete() const
{
    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    AssertReturn(pWizard, false);

    const qulonglong uSize = pWizard->mediumSize();
    if (pWizard->diskSource() == SelectedDiskSource_New)
        return uSize >= m_uMediumSizeMin && uSize <= m_uMediumSizeMax;

    return true;
}

void UIWizardNewVMDiskPage::sltHandleSizeEditorChange(qulonglong uSize)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setMediumSize(uSize);
    m_userModifiedParameters << "MediumSize";
    emit completeChanged();
}

QWidget *UIWizardNewVMDiskPage::createDiskWidgets()
{
    QWidget *pDiskContainer = new QWidget;
    QGridLayout *pDiskLayout = new QGridLayout(pDiskContainer);
    pDiskLayout->setContentsMargins(0, 0, 0, 0);
    pDiskLayout->addWidget(createNewDiskWidgets(), 1, 1, 3, 2);
    return pDiskContainer;
}
