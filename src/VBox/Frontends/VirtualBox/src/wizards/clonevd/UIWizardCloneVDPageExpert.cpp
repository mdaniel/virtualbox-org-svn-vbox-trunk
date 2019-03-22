/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVDPageExpert class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
# include <QButtonGroup>
# include <QCheckBox>
# include <QGridLayout>
# include <QGroupBox>
# include <QHBoxLayout>
# include <QLineEdit>
# include <QRadioButton>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIToolButton.h"
# include "UIConverter.h"
# include "UIIconPool.h"
# include "UIMediaComboBox.h"
# include "UIMessageCenter.h"
# include "UIWizardCloneVD.h"
# include "UIWizardCloneVDPageExpert.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIWizardCloneVDPageExpert::UIWizardCloneVDPageExpert(const CMedium &comSourceVirtualDisk, KDeviceType enmDeviceType)
{
    /* Create widgets: */
    QGridLayout *pMainLayout = new QGridLayout(this);
    {
        m_pSourceDiskCnt = new QGroupBox(this);
        {
            m_pSourceDiskCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pSourceDiskCntLayout = new QHBoxLayout(m_pSourceDiskCnt);
            {
                m_pSourceDiskSelector = new UIMediaComboBox(m_pSourceDiskCnt);
                {
                    m_pSourceDiskSelector->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                    m_pSourceDiskSelector->setType(UIMediumDefs::mediumTypeToLocal(enmDeviceType));
                    m_pSourceDiskSelector->setCurrentItem(comSourceVirtualDisk.GetId());
                    m_pSourceDiskSelector->repopulate();
                }
                m_pSourceDiskOpenButton = new QIToolButton(m_pSourceDiskCnt);
                {
                    m_pSourceDiskOpenButton->setAutoRaise(true);
                    m_pSourceDiskOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_disabled_16px.png"));
                }
                pSourceDiskCntLayout->addWidget(m_pSourceDiskSelector);
                pSourceDiskCntLayout->addWidget(m_pSourceDiskOpenButton);
            }
        }
        m_pDestinationCnt = new QGroupBox(this);
        {
            m_pDestinationCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pLocationCntLayout = new QHBoxLayout(m_pDestinationCnt);
            {
                m_pDestinationDiskEditor = new QLineEdit(m_pDestinationCnt);
                m_pDestinationDiskOpenButton = new QIToolButton(m_pDestinationCnt);
                {
                    m_pDestinationDiskOpenButton->setAutoRaise(true);
                    m_pDestinationDiskOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", "select_file_disabled_16px.png"));
                }
            }
            pLocationCntLayout->addWidget(m_pDestinationDiskEditor);
            pLocationCntLayout->addWidget(m_pDestinationDiskOpenButton);
        }
        m_pFormatCnt = new QGroupBox(this);
        {
            m_pFormatCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pFormatCntLayout = new QVBoxLayout(m_pFormatCnt);
            {
                m_pFormatButtonGroup = new QButtonGroup(m_pFormatCnt);
                {
                    /* Enumerate medium formats in special order: */
                    CSystemProperties properties = vboxGlobal().virtualBox().GetSystemProperties();
                    const QVector<CMediumFormat> &formats = properties.GetMediumFormats();
                    QMap<QString, CMediumFormat> vdi, preferred, others;
                    foreach (const CMediumFormat &format, formats)
                    {
                        /* VDI goes first: */
                        if (format.GetName() == "VDI")
                            vdi[format.GetId()] = format;
                        else
                        {
                            const QVector<KMediumFormatCapabilities> &capabilities = format.GetCapabilities();
                            /* Then goes preferred: */
                            if (capabilities.contains(KMediumFormatCapabilities_Preferred))
                                preferred[format.GetId()] = format;
                            /* Then others: */
                            else
                                others[format.GetId()] = format;
                        }
                    }

                    /* Create buttons for VDI, preferred and others: */
                    foreach (const QString &strId, vdi.keys())
                        addFormatButton(this, pFormatCntLayout, enmDeviceType, vdi.value(strId), true);
                    foreach (const QString &strId, preferred.keys())
                        addFormatButton(this, pFormatCntLayout, enmDeviceType, preferred.value(strId), true);
                    foreach (const QString &strId, others.keys())
                        addFormatButton(this, pFormatCntLayout, enmDeviceType, others.value(strId));

                    if (!m_pFormatButtonGroup->buttons().isEmpty())
                    {
                        m_pFormatButtonGroup->button(0)->click();
                        m_pFormatButtonGroup->button(0)->setFocus();
                    }
                }
            }
        }
        m_pVariantCnt = new QGroupBox(this);
        {
            m_pVariantCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QVBoxLayout *pVariantCntLayout = new QVBoxLayout(m_pVariantCnt);
            {
                m_pVariantButtonGroup = new QButtonGroup(m_pVariantCnt);
                {
                    m_pDynamicalButton = new QRadioButton(m_pVariantCnt);
                    if (enmDeviceType == KDeviceType_HardDisk)
                    {
                        m_pDynamicalButton->click();
                        m_pDynamicalButton->setFocus();
                    }
                    m_pFixedButton = new QRadioButton(m_pVariantCnt);
                    if (   enmDeviceType == KDeviceType_DVD
                        || enmDeviceType == KDeviceType_Floppy)
                    {
                        m_pFixedButton->click();
                        m_pFixedButton->setFocus();
                    }
                    m_pVariantButtonGroup->addButton(m_pDynamicalButton, 0);
                    m_pVariantButtonGroup->addButton(m_pFixedButton, 1);
                }
                m_pSplitBox = new QCheckBox(m_pVariantCnt);
                pVariantCntLayout->addWidget(m_pDynamicalButton);
                pVariantCntLayout->addWidget(m_pFixedButton);
                pVariantCntLayout->addWidget(m_pSplitBox);
            }
        }
        pMainLayout->addWidget(m_pSourceDiskCnt, 0, 0, 1, 2);
        pMainLayout->addWidget(m_pDestinationCnt, 1, 0, 1, 2);
        pMainLayout->addWidget(m_pFormatCnt, 2, 0, Qt::AlignTop);
        pMainLayout->addWidget(m_pVariantCnt, 2, 1, Qt::AlignTop);
    }

    /* Setup connections: */
    connect(m_pSourceDiskSelector, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::currentIndexChanged),
            this, &UIWizardCloneVDPageExpert::sltHandleSourceDiskChange);
    connect(m_pSourceDiskOpenButton, &QIToolButton::clicked,
            this, &UIWizardCloneVDPageExpert::sltHandleOpenSourceDiskClick);
    connect(m_pFormatButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, &UIWizardCloneVDPageExpert::sltMediumFormatChanged);
    connect(m_pVariantButtonGroup, static_cast<void(QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked),
            this, &UIWizardCloneVDPageExpert::completeChanged);
    connect(m_pSplitBox, &QCheckBox::stateChanged,
            this, &UIWizardCloneVDPageExpert::completeChanged);
    connect(m_pDestinationDiskEditor, &QLineEdit::textChanged,
            this, &UIWizardCloneVDPageExpert::completeChanged);
    connect(m_pDestinationDiskOpenButton, &QIToolButton::clicked,
            this, &UIWizardCloneVDPageExpert::sltSelectLocationButtonClicked);

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    qRegisterMetaType<CMediumFormat>();
    /* Register fields: */
    registerField("sourceVirtualDisk", this, "sourceVirtualDisk");
    registerField("mediumFormat", this, "mediumFormat");
    registerField("mediumVariant", this, "mediumVariant");
    registerField("mediumPath", this, "mediumPath");
    registerField("mediumSize", this, "mediumSize");
}

void UIWizardCloneVDPageExpert::sltHandleSourceDiskChange()
{
    /* Get source virtual-disk file-information: */
    QFileInfo sourceFileInfo(sourceVirtualDisk().GetLocation());
    /* Get default path for virtual-disk copy: */
    m_strDefaultPath = sourceFileInfo.absolutePath();
    /* Compose name for virtual-disk copy: */
    QString strMediumName = UIWizardCloneVD::tr("%1_copy", "copied virtual disk image name").arg(sourceFileInfo.baseName());
    /* Set text to location editor: */
    m_pDestinationDiskEditor->setText(strMediumName);

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardCloneVDPageExpert::sltHandleOpenSourceDiskClick()
{
    /* Call to base-class: */
    onHandleOpenSourceDiskClick();

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardCloneVDPageExpert::sltMediumFormatChanged()
{
    /* Get medium format: */
    CMediumFormat mf = mediumFormat();
    if (mf.isNull())
    {
        AssertMsgFailed(("No medium format set!"));
        return;
    }

    /* Enable/disable widgets: */
    ULONG uCapabilities = 0;
    QVector<KMediumFormatCapabilities> capabilities;
    capabilities = mf.GetCapabilities();
    for (int i = 0; i < capabilities.size(); i++)
        uCapabilities |= capabilities[i];

    bool fIsCreateDynamicPossible = uCapabilities & KMediumFormatCapabilities_CreateDynamic;
    bool fIsCreateFixedPossible = uCapabilities & KMediumFormatCapabilities_CreateFixed;
    bool fIsCreateSplitPossible = uCapabilities & KMediumFormatCapabilities_CreateSplit2G;
    m_pDynamicalButton->setEnabled(fIsCreateDynamicPossible);
    m_pFixedButton->setEnabled(fIsCreateFixedPossible);
    m_pSplitBox->setEnabled(fIsCreateSplitPossible);

    /* Compose virtual-disk extension: */
    acquireExtensions(mf, static_cast<UIWizardCloneVD*>(wizardImp())->sourceVirtualDiskDeviceType(),
                      m_aAllowedExtensions, m_strDefaultExtension);

    /* Broadcast complete-change: */
    emit completeChanged();
}

void UIWizardCloneVDPageExpert::sltSelectLocationButtonClicked()
{
    /* Call to base-class: */
    onSelectLocationButtonClicked();
}

void UIWizardCloneVDPageExpert::retranslateUi()
{
    /* Translate widgets: */
    m_pSourceDiskCnt->setTitle(UIWizardCloneVD::tr("Disk image to &copy"));
    m_pSourceDiskOpenButton->setToolTip(UIWizardCloneVD::tr("Choose a virtual disk image file to copy..."));
    m_pDestinationCnt->setTitle(UIWizardCloneVD::tr("&New disk image to create"));
    m_pDestinationDiskOpenButton->setToolTip(UIWizardCloneVD::tr("Choose a location for new virtual disk image file..."));
    m_pFormatCnt->setTitle(UIWizardCloneVD::tr("Disk image file &type"));
    QList<QAbstractButton*> buttons = m_pFormatButtonGroup->buttons();
    for (int i = 0; i < buttons.size(); ++i)
    {
        QAbstractButton *pButton = buttons[i];
        UIMediumFormat enmFormat = gpConverter->fromInternalString<UIMediumFormat>(m_formatNames[m_pFormatButtonGroup->id(pButton)]);
        pButton->setText(gpConverter->toString(enmFormat));
    }
    m_pVariantCnt->setTitle(UIWizardCloneVD::tr("Storage on physical hard disk"));
    m_pDynamicalButton->setText(UIWizardCloneVD::tr("&Dynamically allocated"));
    m_pFixedButton->setText(UIWizardCloneVD::tr("&Fixed size"));
    m_pSplitBox->setText(UIWizardCloneVD::tr("&Split into files of less than 2GB"));
}

void UIWizardCloneVDPageExpert::initializePage()
{
    /* Translate page: */
    retranslateUi();

    sltHandleSourceDiskChange();
    sltMediumFormatChanged();
}

bool UIWizardCloneVDPageExpert::isComplete() const
{
    /* Check what source virtual-disk feats the rules,
     * medium format/variant is correct,
     * current name is not empty: */
    return !sourceVirtualDisk().isNull() &&
           !mediumFormat().isNull() &&
           mediumVariant() != (qulonglong)KMediumVariant_Max &&
           !m_pDestinationDiskEditor->text().trimmed().isEmpty();
}

bool UIWizardCloneVDPageExpert::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Make sure such virtual-disk doesn't exists: */
    QString strMediumPath(mediumPath());
    fResult = !QFileInfo(strMediumPath).exists();
    if (!fResult)
        msgCenter().cannotOverwriteHardDiskStorage(strMediumPath, this);

    /* Try to copy virtual-disk: */
    if (fResult)
        fResult = qobject_cast<UIWizardCloneVD*>(wizard())->copyVirtualDisk();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

