/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIVMLogViewerDialog_h___
#define ___UIVMLogViewerDialog_h___

/* Qt includes: */
#include <QMap>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "QIDialog.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class UIVMLogViewerDialog;

/* Type definitions: */
typedef QMap<QString, UIVMLogViewerDialog*> VMLogViewerMap;


/** QIManagerDialog extension providing GUI with the dialog to control media related functionality. */
class UIVMLogViewerDialog : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:
    UIVMLogViewerDialog(QWidget *pParent, const CMachine &machine);
    ~UIVMLogViewerDialog();

    /** Static method to create/show VM Log Viewer by passing @a pParent to QWidget base-class constructor.
     * @param  machine  Specifies the machine for which VM Log-Viewer is requested. */
    static void showLogViewerFor(QWidget* parent, const CMachine &machine);

private:

    static void showLogViewerDialog(UIVMLogViewerDialog *logViewerDialog);
    void retranslateUi();
    void prepare();
    void cleanup();

    QPushButton      *m_pCloseButton;
    QDialogButtonBox *m_pButtonBox;
    QVBoxLayout      *m_pMainLayout;

    /** Holds the machine instance. */
    CMachine m_machine;

    /** Holds the list of all VM Log Viewers. */
    static VMLogViewerMap m_viewers;
};

#endif /* !___UIVMLogViewerDialog_h___ */

