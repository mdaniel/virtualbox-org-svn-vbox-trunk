/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardCloneVD class implementation.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIMedium.h"
#include "UIWizardCloneVD.h"
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVDPageBasic2.h"
#include "UIWizardCloneVDPageBasic3.h"
#include "UIWizardCloneVDPageExpert.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"

/* COM includes: */
#include "CMediumFormat.h"


UIWizardCloneVD::UIWizardCloneVD(QWidget *pParent, const CMedium &comSourceVirtualDisk)
    : UIWizard(pParent, WizardType_CloneVD)
    , m_comSourceVirtualDisk(comSourceVirtualDisk)
    , m_enmSourceVirtualDiskDeviceType(m_comSourceVirtualDisk.GetDeviceType())
{
#ifndef VBOX_WS_MAC
    /* Assign watermark: */
    assignWatermark(":/wizard_new_harddisk.png");
#else /* VBOX_WS_MAC */
    /* Assign background image: */
    assignBackground(":/wizard_new_harddisk_bg.png");
#endif /* VBOX_WS_MAC */
}

bool UIWizardCloneVD::copyVirtualDisk()
{
    /* Gather attributes: */

    const CMediumFormat comMediumFormat = field("mediumFormat").value<CMediumFormat>();
    const qulonglong uVariant = field("mediumVariant").toULongLong();
    const QString strMediumPath = field("mediumPath").toString();
    const qulonglong uSize = field("mediumSize").toULongLong();
    /* Check attributes: */
    AssertReturn(!strMediumPath.isNull(), false);
    AssertReturn(uSize > 0, false);

    /* Get VBox object: */
    CVirtualBox comVBox = vboxGlobal().virtualBox();

    /* Create new virtual disk image: */
    CMedium comVirtualDisk = comVBox.CreateMedium(comMediumFormat.GetName(), strMediumPath, KAccessMode_ReadWrite, m_enmSourceVirtualDiskDeviceType);
    if (!comVBox.isOk())
    {
        msgCenter().cannotCreateMediumStorage(comVBox, strMediumPath, this);
        return false;
    }

    /* Compose medium-variant: */
    QVector<KMediumVariant> variants(sizeof(qulonglong) * 8);
    for (int i = 0; i < variants.size(); ++i)
    {
        qulonglong temp = uVariant;
        temp &= Q_UINT64_C(1) << i;
        variants[i] = (KMediumVariant)temp;
    }

    /* Copy source image to new one: */
    CProgress comProgress = m_comSourceVirtualDisk.CloneTo(comVirtualDisk, variants, CMedium());
    if (!m_comSourceVirtualDisk.isOk())
    {
        msgCenter().cannotCreateMediumStorage(m_comSourceVirtualDisk, strMediumPath, this);
        return false;
    }

    /* Show creation progress: */
    msgCenter().showModalProgressDialog(comProgress, windowTitle(), ":/progress_media_create_90px.png", this);
    if (comProgress.GetCanceled())
        return false;
    if (!comProgress.isOk() || comProgress.GetResultCode() != 0)
    {
        msgCenter().cannotCreateMediumStorage(comProgress, strMediumPath, this);
        return false;
    }

    /* Save created image as target one: */
    m_comTargetVirtualDisk = comVirtualDisk;

    /* Make sure we register the medium to VBox: */
    vboxGlobal().createMedium(UIMedium(m_comTargetVirtualDisk, UIMediumDeviceType_HardDisk, KMediumState_Created));

    return true;
}

void UIWizardCloneVD::retranslateUi()
{
    /* Call to base-class: */
    UIWizard::retranslateUi();

    /* Translate wizard: */
    setWindowTitle(tr("Copy Virtual Disk Image"));
    setButtonText(QWizard::FinishButton, tr("Copy"));
}

void UIWizardCloneVD::prepare()
{
    /* Create corresponding pages: */
    switch (mode())
    {
        case WizardMode_Basic:
        {
            setPage(Page1, new UIWizardCloneVDPageBasic1(m_enmSourceVirtualDiskDeviceType));
            setPage(Page2, new UIWizardCloneVDPageBasic2(m_enmSourceVirtualDiskDeviceType));
            setPage(Page3, new UIWizardCloneVDPageBasic3);
            break;
        }
        case WizardMode_Expert:
        {
            setPage(PageExpert, new UIWizardCloneVDPageExpert(m_enmSourceVirtualDiskDeviceType));
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid mode: %d", mode()));
            break;
        }
    }
    /* Call to base-class: */
    UIWizard::prepare();
}
