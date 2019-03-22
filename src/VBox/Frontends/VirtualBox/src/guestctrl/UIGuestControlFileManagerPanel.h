/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlFileManagerPanel_h___
#define ___UIGuestControlFileManagerPanel_h___

/* Qt includes: */
#include <QWidget>
#include <QKeySequence>
/* GUI includes: */
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QHBoxLayout;
class QPlainTextEdit;
class QTextDocument;
class QIToolButton;
class UIGuestControlFileManager;

/** QWidget extension acting as the base class for UIVMLogViewerXXXPanel widgets. */
class UIGuestControlFileManagerPanel : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIGuestControlFileManagerPanel(UIGuestControlFileManager *pManagerWidget, QWidget *pParent);
    void setCloseButtonShortCut(QKeySequence shortCut);
    virtual QString panelName() const = 0;

protected:

    virtual void prepare();
    virtual void prepareWidgets();
    virtual void prepareConnections();

    /* Access functions for children classes. */
    QHBoxLayout*               mainLayout();

    /** Handles the translation event. */
    void retranslateUi();

    /** Handles Qt @a pEvent, used for keyboard processing. */
    bool eventFilter(QObject *pObject, QEvent *pEvent);
    /** Handles the Qt show @a pEvent. */
    void showEvent(QShowEvent *pEvent);
    /** Handles the Qt hide @a pEvent. */
    void hideEvent(QHideEvent *pEvent);

    /* Return the unmodified log. */
    const QString* logString() const;

private:

    /** Holds the reference to VM Log-Viewer this panel belongs to. */
    QHBoxLayout         *m_pMainLayout;
    QIToolButton        *m_pCloseButton;
    UIGuestControlFileManager *m_pFileManager;
};

#endif /* !___UIGuestControlFileManagerPanel!_h___ */
