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

#ifndef ___UIGuestControlFileManagerSettingsPanel_h___
#define ___UIGuestControlFileManagerSettingsPanel_h___

/* GUI includes: */
#include "UIGuestControlFileManagerPanel.h"

/* Forward declarations: */
class QCheckBox;
class QSpinBox;
class QLabel;
class QIToolButton;
class UIGuestControlFileManagerSettings;

/** UIVMLogViewerPanel extension providing GUI to manage logviewer settings. */
class UIGuestControlFileManagerSettingsPanel : public UIGuestControlFileManagerPanel
{
    Q_OBJECT;

public:

    UIGuestControlFileManagerSettingsPanel(UIGuestControlFileManager *pManagerWidget,
                                           QWidget *pParent, UIGuestControlFileManagerSettings *pFileManagerSettings);
    virtual QString panelName() const /* override */;

signals:

    void sigListDirectoriesFirstChanged();

protected:

    virtual void prepareWidgets() /* override */;
    virtual void prepareConnections() /* override */;

    /** Handles the translation event. */
    void retranslateUi();

private slots:

    void sltListDirectoryCheckBoxToogled(bool fChecked);

private:

    QCheckBox                         *m_pListDirectoriesOnTopCheckBox;
    QCheckBox                         *m_pDeleteConfirmationCheckBox;
    UIGuestControlFileManagerSettings *m_pFileManagerSettings;
};

#endif /* !___UIGuestControlFileManagerSettingsPanel_h___ */
