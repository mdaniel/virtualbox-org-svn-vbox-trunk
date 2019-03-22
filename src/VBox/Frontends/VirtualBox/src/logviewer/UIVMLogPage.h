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

#ifndef ___UIVMLogPage_h___
#define ___UIVMLogPage_h___

/* Qt includes: */
#include <QWidget>
/* #include <QMap> */
#include <QPair>

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"

/* Forward declarations: */
class QITabWidget;
class QPlainTextEdit;
class QHBoxLayout;
class UIToolBar;
class UIVMLogViewerBookmarksPanel;
class UIVMLogViewerFilterPanel;
class UIVMLogViewerPanel;
class UIVMLogViewerSearchPanel;

/* Type definitions: */
/** first is line number, second is block text */
typedef QPair<int, QString> LogBookmark;

class UIVMLogPage  : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    UIVMLogPage(QWidget *pParent = 0, int tabIndex = -1);
    /** Destructs the VM Log-Viewer. */
    ~UIVMLogPage();
    /** Returns the width of the current log page. return 0 if there is no current log page: */
    int defaultLogPageWidth() const;

    QPlainTextEdit *textEdit();
    QTextDocument  *document();

    void setTabIndex(int index);
    int tabIndex()  const;

    /* Only to be called when log file is re-read. */
    void setLogString(const QString &strLog);
    const QString& logString() const;

    void setFileName(const QString &strFileName);
    const QString& fileName() const;

    /** Ses plaintextEdit's text. Note that the text we
        show currently might be different than
        m_strLog. For example during filtering. */
    void setTextEdit(const QString &strText);

    /* Marks the plain text edit When we dont have a log content. */
    void markForError();

    void setScrollBarMarkingsVector(const QVector<float> &vector);
    void clearScrollBarMarkingsVector();

    /* Undos the changes done to textDocument */
    void documentUndo();

protected:

private slots:


private:
    void prepare();
    void prepareWidgets();
    void cleanup();
    void retranslateUi();

    QHBoxLayout    *m_pMainLayout;
    QPlainTextEdit *m_pPlainTextEdit;
    /** Stores the log file (unmodified) content. */
    QString         m_strLog;
    /** Stores full path and name of the log file. */
    QString         m_strFileName;
    /** This is the index of the tab containing this widget in UIVMLogViewerWidget. */
    int             m_tabIndex;

    QVector<LogBookmark> m_bookmarkMap;

};

#endif /* !___UIVMLogPage_h___ */
