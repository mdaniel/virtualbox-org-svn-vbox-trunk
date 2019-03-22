/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlConsole class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QVBoxLayout>
# include <QApplication>
# include <QTextBlock>

/* GUI includes: */
# include "UIGuestControlConsole.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

UIGuestControlConsole::UIGuestControlConsole(QWidget* parent /* = 0 */)
    :QPlainTextEdit(parent)
    , m_strGreet("Welcome to 'Guest Control Console'. Type 'help' for help\n")
    , m_strPrompt("$>")
    , m_uCommandHistoryIndex(0)
{
    /* Configure this: */
    setWordWrapMode(QTextOption::NoWrap);
    reset();
}

void UIGuestControlConsole::reset()
{
    clear();
    startNextLine();
    insertPlainText(m_strGreet);
    startNextLine();
}

void UIGuestControlConsole::startNextLine()
{
    moveCursor(QTextCursor::End);
    insertPlainText(m_strPrompt);
    moveCursor(QTextCursor::End);
}


void UIGuestControlConsole::putOutput(const QString &strOutput)
{
    if (strOutput.isNull() || strOutput.length() <= 0)
        return;

    bool newLineNeeded = getCommandString().isEmpty();

    QString strOwn("\n");
    strOwn.append(strOutput);
    moveCursor(QTextCursor::End);
    insertPlainText(strOwn);
    moveCursor(QTextCursor::End);

    if (newLineNeeded)
    {
        insertPlainText("\n");
        startNextLine();
    }
}

void UIGuestControlConsole::keyPressEvent(QKeyEvent *pEvent)
{

    switch (pEvent->key()) {
        case Qt::Key_PageUp:
        case Qt::Key_Up:
        {
            replaceLineContent(getPreviousCommandFromHistory(getCommandString()));
            break;
        }
        case Qt::Key_PageDown:
        case Qt::Key_Down:
        {
            replaceLineContent(getNextCommandFromHistory(getCommandString()));
            break;
        }
        case Qt::Key_Backspace:
        {
            QTextCursor cursor = textCursor();
            if (cursor.positionInBlock() > m_strPrompt.length())
                cursor.deletePreviousChar();
            break;
        }
        case Qt::Key_Left:
        case Qt::Key_Right:
        {
            if (textCursor().positionInBlock() > m_strPrompt.length())
                QPlainTextEdit::keyPressEvent(pEvent);
            break;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
        {
            QString strCommand(getCommandString());
            if (!strCommand.isEmpty())
            {
                emit commandEntered(strCommand);
                if (!m_tCommandHistory.contains(strCommand))
                    m_tCommandHistory.push_back(strCommand);
                m_uCommandHistoryIndex = m_tCommandHistory.size()-1;
                moveCursor(QTextCursor::End);
                QPlainTextEdit::keyPressEvent(pEvent);
                startNextLine();
            }
            break;
        }
        case Qt::Key_Home:
        {
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, m_strPrompt.length());
            setTextCursor(cursor);
            break;
        }
        default:
            QPlainTextEdit::keyPressEvent(pEvent);
            break;
    }
}

void UIGuestControlConsole::mousePressEvent(QMouseEvent *pEvent)
{
    // Q_UNUSED(pEvent);
    // setFocus();
    QPlainTextEdit::mousePressEvent(pEvent);
}

void UIGuestControlConsole::mouseDoubleClickEvent(QMouseEvent *pEvent)
{
    //Q_UNUSED(pEvent);
    QPlainTextEdit::mouseDoubleClickEvent(pEvent);
}

void UIGuestControlConsole::contextMenuEvent(QContextMenuEvent *pEvent)
{
    //Q_UNUSED(pEvent);
    QPlainTextEdit::contextMenuEvent(pEvent);
}

QString UIGuestControlConsole::getCommandString()
{
    QTextDocument* pDocument = document();
    if (!pDocument)
        return QString();
    QTextBlock block = pDocument->lastBlock();//findBlockByLineNumber(pDocument->lineCount()-1);
    if (!block.isValid())
        return QString();
    QString lineStr = block.text();
    if (lineStr.isNull() || lineStr.length() <= 1)
        return QString();
    /* Remove m_strPrompt from the line string: */
    return (lineStr.right(lineStr.length()-m_strPrompt.length()));
}

void UIGuestControlConsole::replaceLineContent(const QString &stringNewContent)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    QString newString(m_strPrompt);
    newString.append(stringNewContent);
    insertPlainText(newString);
    moveCursor(QTextCursor::End);
}

QString UIGuestControlConsole::getNextCommandFromHistory(const QString &originalString /* = QString() */)
{
    if (m_tCommandHistory.empty())
        return originalString;

    if (m_uCommandHistoryIndex == (unsigned)(m_tCommandHistory.size() - 1))
        m_uCommandHistoryIndex = 0;
    else
        ++m_uCommandHistoryIndex;

    return m_tCommandHistory.at(m_uCommandHistoryIndex);
}


QString UIGuestControlConsole::getPreviousCommandFromHistory(const QString &originalString /* = QString() */)
{
    if (m_tCommandHistory.empty())
        return originalString;
    if (m_uCommandHistoryIndex == 0)
        m_uCommandHistoryIndex = m_tCommandHistory.size() - 1;
    else
        --m_uCommandHistoryIndex;

    return m_tCommandHistory.at(m_uCommandHistoryIndex);
}
