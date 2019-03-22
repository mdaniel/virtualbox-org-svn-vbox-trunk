/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QDateTime>
# include <QDir>
# include <QVBoxLayout>
# if defined(RT_OS_SOLARIS)
#  include <QFontDatabase>
# endif
# include <QPainter>
# include <QScrollBar>
# include <QTextBlock>

/* GUI includes: */
# include "UIVMLogPage.h"
# include "UIVMLogViewerTextEdit.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogPage::UIVMLogPage(QWidget *pParent /* = 0 */, int tabIndex /*= -1 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pMainLayout(0)
    , m_pTextEdit(0)
    , m_tabIndex(tabIndex)
    , m_iSelectedBookmarkIndex(-1)
    , m_bFiltered(false)
    , m_iFilteredLineCount(-1)
    , m_iUnfilteredLineCount(-1)
    , m_bShowLineNumbers(true)
    , m_bWrapLines(false)
    , m_iFontSizeInPoints(9)
{
    prepare();
}

UIVMLogPage::~UIVMLogPage()
{
    cleanup();
}

int UIVMLogPage::defaultLogPageWidth() const
{
    if (!m_pTextEdit)
        return 0;

    /* Compute a width for 132 characters plus scrollbar and frame width: */
    int iDefaultWidth = m_pTextEdit->fontMetrics().width(QChar('x')) * 132 +
                        m_pTextEdit->verticalScrollBar()->width() +
                        m_pTextEdit->frameWidth() * 2;

    return iDefaultWidth;
}


void UIVMLogPage::prepare()
{
    prepareWidgets();
    retranslateUi();
}

void UIVMLogPage::prepareWidgets()
{
    m_pMainLayout = new QHBoxLayout();
    setLayout(m_pMainLayout);
    m_pMainLayout->setSpacing(0);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

    m_pTextEdit = new UIVMLogViewerTextEdit(this);
    m_pMainLayout->addWidget(m_pTextEdit);

    connect(qobject_cast<UIVMLogViewerTextEdit*>(m_pTextEdit), &UIVMLogViewerTextEdit::sigAddBookmark,
            this, &UIVMLogPage::sltAddBookmark);
    connect(qobject_cast<UIVMLogViewerTextEdit*>(m_pTextEdit), &UIVMLogViewerTextEdit::sigDeleteBookmark,
            this, &UIVMLogPage::sltDeleteBookmark);
}

QPlainTextEdit *UIVMLogPage::textEdit()
{
    return m_pTextEdit;
}

QTextDocument* UIVMLogPage::document()
{
    if (!m_pTextEdit)
        return 0;
    return m_pTextEdit->document();
}

void UIVMLogPage::setTabIndex(int index)
{
    m_tabIndex = index;
}

int UIVMLogPage::tabIndex()  const
{
    return m_tabIndex;
}

void UIVMLogPage::retranslateUi()
{
}

void UIVMLogPage::cleanup()
{
}

void UIVMLogPage::setLogString(const QString &strLog)
{
    m_strLog = strLog;
}

const QString& UIVMLogPage::logString() const
{
    return m_strLog;
}

void UIVMLogPage::setFileName(const QString &strFileName)
{
    m_strFileName = strFileName;
}

const QString& UIVMLogPage::fileName() const
{
    return m_strFileName;
}

void UIVMLogPage::setTextEditText(const QString &strText)
{
    if (!m_pTextEdit)
        return;

    m_pTextEdit->setPlainText(strText);
    /* Move the cursor position to end: */
    QTextCursor cursor = m_pTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    m_pTextEdit->setTextCursor(cursor);
    repaint();
}

void UIVMLogPage::setTextEditTextAsHtml(const QString &strText)
{
    if (!m_pTextEdit)
        return;
    m_pTextEdit->appendHtml(strText);
    repaint();
}

void UIVMLogPage::markForError()
{
    if (!m_pTextEdit)
        return;
    m_pTextEdit->setWrapLines(true);
}

void UIVMLogPage::setScrollBarMarkingsVector(const QVector<float> &vector)
{
    if (!m_pTextEdit)
        return;
    m_pTextEdit->setScrollBarMarkingsVector(vector);
    repaint();
}

void UIVMLogPage::clearScrollBarMarkingsVector()
{
    if (!m_pTextEdit)
        return;
    m_pTextEdit->clearScrollBarMarkingsVector();
    repaint();
}

void UIVMLogPage::documentUndo()
{
    if (!m_pTextEdit)
        return;
    if (m_pTextEdit->document())
        m_pTextEdit->document()->undo();
}


void UIVMLogPage::deleteBookmark(int index)
{
    if (m_bookmarkVector.size() <= index)
         return;
    m_bookmarkVector.remove(index, 1);
    updateTextEditBookmarkLineSet();
}

void UIVMLogPage::deleteBookmark(LogBookmark bookmark)
{
    int index = -1;
    for (int i = 0; i < m_bookmarkVector.size(); ++i)
    {
        if (m_bookmarkVector.at(i).first == bookmark.first)
        {
            index = i;
            break;
        }
    }
    if (index != -1)
        deleteBookmark(index);
}


void UIVMLogPage::deleteAllBookmarks()
{
    m_bookmarkVector.clear();
    updateTextEditBookmarkLineSet();
}

void UIVMLogPage::scrollToBookmark(int bookmarkIndex)
{
    if (!m_pTextEdit)
        return;
    if (bookmarkIndex >= m_bookmarkVector.size())
        return;

    int lineNumber = m_bookmarkVector.at(bookmarkIndex).first;
    m_pTextEdit->scrollToLine(lineNumber);
}

const QVector<LogBookmark>& UIVMLogPage::bookmarkVector() const
{
    return m_bookmarkVector;
}

void UIVMLogPage::showEvent(QShowEvent *pEvent)
{
    if (m_pTextEdit)
        m_pTextEdit->setFocus();
    applySettings();
    QWidget::showEvent(pEvent);
}

void UIVMLogPage::sltAddBookmark(LogBookmark bookmark)
{
    m_bookmarkVector.push_back(bookmark);
    updateTextEditBookmarkLineSet();
    emit sigBookmarksUpdated();
}

void UIVMLogPage::sltDeleteBookmark(LogBookmark bookmark)
{
    deleteBookmark(bookmark);
    updateTextEditBookmarkLineSet();
    emit sigBookmarksUpdated();
}

void UIVMLogPage::updateTextEditBookmarkLineSet()
{
    if (!m_pTextEdit)
        return;
    QSet<int> bookmarkLinesSet;
    for (int i = 0; i < m_bookmarkVector.size(); ++i)
    {
        bookmarkLinesSet.insert(m_bookmarkVector.at(i).first);
    }
    m_pTextEdit->setBookmarkLineSet(bookmarkLinesSet);
}

bool UIVMLogPage::isFiltered() const
{
    return m_bFiltered;
}

void UIVMLogPage::setFiltered(bool filtered)
{
    if (m_bFiltered == filtered)
        return;
    m_bFiltered = filtered;
    if (m_pTextEdit)
    {
        m_pTextEdit->setShownTextIsFiltered(m_bFiltered);
        m_pTextEdit->update();
    }
    emit sigLogPageFilteredChanged(m_bFiltered);
}

void UIVMLogPage::setShowLineNumbers(bool bShowLineNumbers)
{
    if (m_bShowLineNumbers == bShowLineNumbers)
        return;
    m_bShowLineNumbers = bShowLineNumbers;
}

void UIVMLogPage::setWrapLines(bool bWrapLines)
{
    if (m_bWrapLines == bWrapLines)
        return;
    m_bWrapLines = bWrapLines;
    applySettings();
}

void UIVMLogPage::setFilterParameters(const QSet<QString> &filterTermSet, int filterOperationType,
                                      int iFilteredLineCount, int iUnfilteredLineCount)
{
    m_filterTermSet = filterTermSet;
    m_filterOperationType = filterOperationType;
    m_iFilteredLineCount = iFilteredLineCount;
    m_iUnfilteredLineCount = iUnfilteredLineCount;
}

void UIVMLogPage::setFontSizeInPoints(int fontSize)
{
    if (m_iFontSizeInPoints == fontSize)
        return;
    m_iFontSizeInPoints = fontSize;
    applySettings();
}

int UIVMLogPage::fontSizeInPoints() const
{
    return m_iFontSizeInPoints;
}

int  UIVMLogPage::filteredLineCount() const
{
    return m_iFilteredLineCount;
}

int  UIVMLogPage::unfilteredLineCount() const
{
    return m_iUnfilteredLineCount;
}

bool UIVMLogPage::shouldFilterBeApplied(const QSet<QString> &filterTermSet, int filterOperationType) const
{
    /* If filter terms set is different reapply the filter. */
    if (filterTermSet != m_filterTermSet)
        return true;

    /* If filter operation type set is different reapply the filter. */
    if (filterOperationType != m_filterOperationType)
        return true;
    return false;
}

void UIVMLogPage::applySettings()
{
    if (!isVisible())
        return;
    if (!m_pTextEdit)
        return;
    if (m_bWrapLines != m_pTextEdit->wrapLines())
        m_pTextEdit->setWrapLines(m_bWrapLines);

    if (m_bShowLineNumbers != m_pTextEdit->showLineNumbers())
        m_pTextEdit->setShowLineNumbers(m_bShowLineNumbers);

    if (m_iFontSizeInPoints != m_pTextEdit->fontSizeInPoints())
        m_pTextEdit->setFontSizeInPoints(m_iFontSizeInPoints);

    update();

}
