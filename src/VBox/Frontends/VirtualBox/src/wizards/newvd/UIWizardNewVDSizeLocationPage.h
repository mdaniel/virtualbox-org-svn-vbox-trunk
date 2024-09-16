/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardNewVDSizeLocationPage class declaration.
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

#ifndef FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDSizeLocationPage_h
#define FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDSizeLocationPage_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QSet>

/* GUI includes: */
#include "UINativeWizardPage.h"

/* Forward declarations: */
class UIMediumSizeAndPathGroupBox;

class SHARED_LIBRARY_STUFF UIWizardNewVDSizeLocationPage : public UINativeWizardPage
{
    Q_OBJECT;

public:

    UIWizardNewVDSizeLocationPage(qulonglong uDiskMinimumSize);

private slots:

    void sltSelectLocationButtonClicked();
    void sltMediumSizeChanged(qulonglong uSize);
    void sltMediumPathChanged(const QString &strPath);
    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

private:

    virtual void initializePage() RT_OVERRIDE RT_FINAL;
    virtual bool isComplete() const RT_OVERRIDE RT_FINAL;
    virtual bool validatePage() RT_OVERRIDE RT_FINAL;
    void prepare();

    UIMediumSizeAndPathGroupBox *m_pMediumSizePathGroup;
    qulonglong m_uMediumSizeMin;
    qulonglong m_uMediumSizeMax;
    QSet<QString> m_userModifiedParameters;
};


#endif /* !FEQT_INCLUDED_SRC_wizards_newvd_UIWizardNewVDSizeLocationPage_h */
