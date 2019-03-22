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

#ifndef ___UIFileManagerOperationsPanel_h___
#define ___UIFileManagerOperationsPanel_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
# include <QUuid>

/* GUI includes: */
#include "UIGuestControlDefs.h"
#include "UIFileManagerPanel.h"

/* Forward declarations: */
class CProgress;
class QScrollArea;
class QSpacerItem;
class QVBoxLayout;

class UIFileOperationModel;
class UIFileOperationProgressWidget;
class UIFileManager;


/** UIVMLogViewerPanel extension hosting a QListWidget which in turn has a special QWidget extension
  * to manage multiple CProgress instances. This is particulary used in monitoring file operations. */
class UIFileManagerOperationsPanel : public UIFileManagerPanel
{
    Q_OBJECT;

signals:

    void sigFileOperationComplete(QUuid progressId);
    void sigFileOperationFail(QString strErrorString, FileManagerLogType eLogType);

public:

    UIFileManagerOperationsPanel(UIFileManager *pManagerWidget, QWidget *pParent);
    virtual QString panelName() const /* override */;
    void addNewProgress(const CProgress &comProgress);

protected:

    /** @name Preparation specific functions.
      * @{ */
        virtual void prepareWidgets() /* override */;
        virtual void prepareConnections() /* override */;
    /** @} */

    /** Handles the translation event. */
    virtual void retranslateUi() /* override */;
    virtual void contextMenuEvent(QContextMenuEvent *pEvent) /* override */;

private slots:

    void sltRemoveFinished();
    void sltRemoveAll();
    void sltRemoveSelected();

    void sltHandleWidgetFocusIn(QWidget *pWidget);
    void sltHandleWidgetFocusOut(QWidget *pWidget);
    void sltScrollToBottom(int iMin, int iMax);

private:

    /** @name Member variables.
      * @{ */
        QScrollArea    *m_pScrollArea;
        QWidget        *m_pContainerWidget;
        QVBoxLayout    *m_pContainerLayout;
        QSpacerItem    *m_pContainerSpaceItem;
        QWidget        *m_pWidgetInFocus;
        QSet<QWidget*>  m_widgetSet;
    /** @} */



};

#endif /* !___UIFileManagerOperationsPanel_h___ */
