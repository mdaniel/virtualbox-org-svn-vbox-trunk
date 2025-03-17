/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVMHardwarePage class implementation.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "QIRichTextLabel.h"
#include "UIBaseMemoryEditor.h"
#include "UIWizardDiskEditors.h"
#include "UIGlobalSession.h"
#include "UIGuestOSType.h"
#include "UIMediumSizeEditor.h"
#include "UITranslationEventListener.h"
#include "UIVirtualCPUEditor.h"
#include "UIWizardNewVM.h"
#include "UIWizardNewVMHardwarePage.h"

/* COM includes: */
#include "CSystemProperties.h"

UIWizardNewVMHardwarePage::UIWizardNewVMHardwarePage(const QString strHelpKeyword /* = QString() */)
    : UINativeWizardPage(strHelpKeyword)
    , m_pLabel(0)
    , m_pBaseMemoryEditor(0)
    , m_pVirtualCPUEditor(0)
    , m_pEFICheckBox(0)
    , m_pMediumSizeEditor(0)
    , m_fVDIFormatFound(false)
    , m_uMediumSizeMin(_4M)
    , m_uMediumSizeMax(gpGlobalSession->virtualBox().GetSystemProperties().GetInfoVDSize())
{
    prepare();
    qRegisterMetaType<CMedium>();
}

void UIWizardNewVMHardwarePage::prepare()
{
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);

    m_pLabel = new QIRichTextLabel(this);
    pMainLayout->addWidget(m_pLabel);
    QWidget *pHardwareWidgetContainer = new QWidget(this);
    AssertReturnVoid(pHardwareWidgetContainer);

    QGridLayout *pContainerLayout = new QGridLayout(pHardwareWidgetContainer);
    pContainerLayout->setContentsMargins(0, 0, 0, 0);

    m_pBaseMemoryEditor = new UIBaseMemoryEditor;
    m_pVirtualCPUEditor = new UIVirtualCPUEditor;
    m_pMediumSizeEditor = new UIMediumSizeEditor(this, true /* with editor label */);
    m_pEFICheckBox      = new QCheckBox;
    pContainerLayout->addWidget(m_pBaseMemoryEditor, 0, 0, 1, 4);
    pContainerLayout->addWidget(m_pVirtualCPUEditor, 1, 0, 1, 4);
    pContainerLayout->addWidget(m_pMediumSizeEditor, 2, 0, 1, 4);
    pContainerLayout->addWidget(m_pEFICheckBox, 3, 0, 1, 1);

    pMainLayout->addWidget(pHardwareWidgetContainer);

    pMainLayout->addStretch();
    createConnections();
}

void UIWizardNewVMHardwarePage::createConnections()
{
    if (m_pBaseMemoryEditor)
        connect(m_pBaseMemoryEditor, &UIBaseMemoryEditor::sigValueChanged,
                this, &UIWizardNewVMHardwarePage::sltMemorySizeChanged);
    if (m_pVirtualCPUEditor)
        connect(m_pVirtualCPUEditor, &UIVirtualCPUEditor::sigValueChanged,
                this, &UIWizardNewVMHardwarePage::sltCPUCountChanged);
    if (m_pEFICheckBox)
        connect(m_pEFICheckBox, &QCheckBox::toggled,
                this, &UIWizardNewVMHardwarePage::sltEFIEnabledChanged);
    if (m_pMediumSizeEditor)
        connect(m_pMediumSizeEditor, &UIMediumSizeEditor::sigSizeChanged,
                this, &UIWizardNewVMHardwarePage::sltHandleSizeEditorChange);

    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIWizardNewVMHardwarePage::sltRetranslateUI);
}

void UIWizardNewVMHardwarePage::sltRetranslateUI()
{
    setTitle(UIWizardNewVM::tr("Specify virtual hardware"));

    if (m_pLabel)
        m_pLabel->setText(UIWizardNewVM::tr("Specify the VM's hardware. Resources allocated to the VM will not be available to the host when the VM is running."));
    updateMinimumLayoutHint();
}

void UIWizardNewVMHardwarePage::initializePage()
{
    sltRetranslateUI();

    UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
    const QString &strTypeId = pWizard->guestOSTypeId();

    if (m_pBaseMemoryEditor && !m_userModifiedParameters.contains("MemorySize"))
    {
        m_pBaseMemoryEditor->blockSignals(true);
        ULONG recommendedRam = gpGlobalSession->guestOSTypeManager().getRecommendedRAM(strTypeId);
        m_pBaseMemoryEditor->setValue(recommendedRam);
        pWizard->setMemorySize(recommendedRam);
        m_pBaseMemoryEditor->blockSignals(false);
    }
    if (m_pVirtualCPUEditor && !m_userModifiedParameters.contains("CPUCount"))
    {
        m_pVirtualCPUEditor->blockSignals(true);
        ULONG recommendedCPUs = gpGlobalSession->guestOSTypeManager().getRecommendedCPUCount(strTypeId);
        m_pVirtualCPUEditor->setValue(recommendedCPUs);
        pWizard->setCPUCount(recommendedCPUs);
        m_pVirtualCPUEditor->blockSignals(false);
    }
    if (m_pEFICheckBox && !m_userModifiedParameters.contains("EFIEnabled"))
    {
        m_pEFICheckBox->blockSignals(true);
        KFirmwareType fwType = gpGlobalSession->guestOSTypeManager().getRecommendedFirmware(strTypeId);
        m_pEFICheckBox->setChecked(fwType != KFirmwareType_BIOS);
        pWizard->setEFIEnabled(fwType != KFirmwareType_BIOS);
        m_pEFICheckBox->blockSignals(false);
    }

    initializeVirtualHardDiskParameters();
}

void UIWizardNewVMHardwarePage::initializeVirtualHardDiskParameters()
{
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

    pWizard->setMediumVariant((qulonglong)KMediumVariant_Standard);
}

bool UIWizardNewVMHardwarePage::isComplete() const
{
   UIWizardNewVM *pWizard = wizardWindow<UIWizardNewVM>();
   AssertReturn(pWizard, false);

   const qulonglong uSize = pWizard->mediumSize();
   if (pWizard->diskSource() == SelectedDiskSource_New)
       return uSize >= m_uMediumSizeMin && uSize <= m_uMediumSizeMax;
   return true;
}

void UIWizardNewVMHardwarePage::sltMemorySizeChanged(int iValue)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setMemorySize(iValue);
    m_userModifiedParameters << "MemorySize";
}

void UIWizardNewVMHardwarePage::sltCPUCountChanged(int iCount)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setCPUCount(iCount);
    m_userModifiedParameters << "CPUCount";
}

void UIWizardNewVMHardwarePage::sltEFIEnabledChanged(bool fEnabled)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setEFIEnabled(fEnabled);
    m_userModifiedParameters << "EFIEnabled";
}

void UIWizardNewVMHardwarePage::sltHandleSizeEditorChange(qulonglong uSize)
{
    AssertReturnVoid(wizardWindow<UIWizardNewVM>());
    wizardWindow<UIWizardNewVM>()->setMediumSize(uSize);
    m_userModifiedParameters << "MediumSize";
    emit completeChanged();
}

void UIWizardNewVMHardwarePage::updateMinimumLayoutHint()
{
    /* These editors have own labels, but we want them to be properly layouted according to each other: */
    int iMinimumLayoutHint = 0;
    if (m_pBaseMemoryEditor && !m_pBaseMemoryEditor->isHidden())
        iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pBaseMemoryEditor->minimumLabelHorizontalHint());
    if (m_pVirtualCPUEditor && !m_pVirtualCPUEditor->isHidden())
        iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pVirtualCPUEditor->minimumLabelHorizontalHint());
    if (m_pMediumSizeEditor && !m_pMediumSizeEditor->isHidden())
        iMinimumLayoutHint = qMax(iMinimumLayoutHint, m_pMediumSizeEditor->minimumLabelHorizontalHint());
    if (m_pBaseMemoryEditor)
        m_pBaseMemoryEditor->setMinimumLayoutIndent(iMinimumLayoutHint);
    if (m_pVirtualCPUEditor)
        m_pVirtualCPUEditor->setMinimumLayoutIndent(iMinimumLayoutHint);
    if (m_pMediumSizeEditor)
        m_pMediumSizeEditor->setMinimumLayoutIndent(iMinimumLayoutHint);
}
