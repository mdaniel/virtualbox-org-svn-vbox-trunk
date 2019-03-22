/* $Id$ */
/** @file
 * VBox Qt GUI - UIWizardExportAppPageBasic1 class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h
#define FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QUuid>

/* GUI includes: */
#include "UIWizardPage.h"

/* Forward declarations: */
class QListWidget;
class QIRichTextLabel;


/** UIWizardPageBase extension for 1st page of the Export Appliance wizard. */
class UIWizardExportAppPage1 : public UIWizardPageBase
{
protected:

    /** Constructs 1st page base. */
    UIWizardExportAppPage1();

    /** Populates VM selector items on the basis of passed @a selectedVMNames. */
    void populateVMSelectorItems(const QStringList &selectedVMNames);

    /** Returns a list of selected machine names. */
    QStringList machineNames() const;
    /** Returns a list of selected machine IDs. */
    QList<QUuid> machineIDs() const;

    /** Holds the VM selector instance. */
    QListWidget *m_pVMSelector;
};


/** UIWizardPage extension for 1st page of the Export Appliance wizard, extends UIWizardExportAppPage1 as well. */
class UIWizardExportAppPageBasic1 : public UIWizardPage, public UIWizardExportAppPage1
{
    Q_OBJECT;
    Q_PROPERTY(QStringList machineNames READ machineNames);
    Q_PROPERTY(QList<QUuid> machineIDs READ machineIDs);

public:

    /** Constructs 1st basic page.
      * @param  selectedVMNames  Brings the list of selected VM names. */
    UIWizardExportAppPageBasic1(const QStringList &selectedVMNames);

private:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Performs page initialization. */
    virtual void initializePage() /* override */;

    /** Returns whether page is complete. */
    virtual bool isComplete() const /* override */;

    /** Performs page validation. */
    virtual bool validatePage() /* override */;

private:

    /** Holds the label instance. */
    QIRichTextLabel *m_pLabel;
};

#endif /* !FEQT_INCLUDED_SRC_wizards_exportappliance_UIWizardExportAppPageBasic1_h */
