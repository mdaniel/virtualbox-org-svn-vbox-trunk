/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QILabel class declaration.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This class is based on the original QLabel implementation.
 */

#ifndef ___QILabel_h___
#define ___QILabel_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QLabel>

/* GUI includes: */
#include "UILibraryDefs.h"

/** QLabel subclass extending it with advanced functionality. */
class SHARED_LIBRARY_STUFF QILabel : public QLabel
{
    Q_OBJECT;

public:

    /** Constructs label passing @a pParent and @a enmFlags to the base-class. */
    QILabel(QWidget *pParent = 0, Qt::WindowFlags enmFlags = 0);
    /** Constructs label passing @a strText, @a pParent and @a enmFlags to the base-class. */
    QILabel(const QString &strText, QWidget *pParent = 0, Qt::WindowFlags enmFlags = 0);

    /** Returns whether label full-size focusing selection is enabled. */
    bool fullSizeSelection() const { return m_fFullSizeSelection; }
    /** Defines whether label full-size focusing selection is @a fEnabled. */
    void setFullSizeSelection(bool fEnabled);

    /** Defines whether label should use size-hint based on passed @a iWidthHint. */
    void useSizeHintForWidth(int iWidthHint) const;
    /** Returns size-hint. */
    QSize sizeHint() const;
    /** Returns minimum size-hint. */
    QSize minimumSizeHint() const;

    /** Returns text. */
    QString text() const { return m_strText; }

public slots:

    /** Clears text. */
    void clear();
    /** Defines text. */
    void setText(const QString &strText);
    /** Copies text into clipboard. */
    void copy();

protected:

    /** Handles resize @a pEvent. */
    void resizeEvent(QResizeEvent *pEvent);

    /** Handles mouse-press @a pEvent. */
    void mousePressEvent(QMouseEvent *pEvent);
    /** Handles mouse-release @a pEvent. */
    void mouseReleaseEvent(QMouseEvent *pEvent);
    /** Handles mouse-move @a pEvent. */
    void mouseMoveEvent(QMouseEvent *pEvent);

    /** Handles context-menu @a pEvent. */
    void contextMenuEvent(QContextMenuEvent *pEvent);

    /** Handles focus-in @a pEvent. */
    void focusInEvent(QFocusEvent *pEvent);
    /** Handles focus-out @a pEvent. */
    void focusOutEvent(QFocusEvent *pEvent);

    /** Handles paint @a pEvent. */
    void paintEvent(QPaintEvent *pEvent);

private:

    /** Performs initialization. */
    void init();

    /** Updates size-hint. */
    void updateSizeHint() const;
    /** Defines full-text. */
    void setFullText(const QString &strText);
    /** Updates text. */
    void updateText();
    /** Compresses passed @a strText. */
    QString compressText(const QString &strText) const;
    /** Returns text without HTML tags. */
    static QString removeHtmlTags(const QString &strText);
    /** Converts passed @a strType to text-elide mode flag. */
    static Qt::TextElideMode toTextElideMode(const QString& strType);

    /** Holds the text. */
    QString m_strText;

    /** Holds whether label full-size focusing selection is enabled. */
    bool m_fFullSizeSelection;
    /** Holds whether we started D&D. */
    bool m_fStartDragging;

    /** Holds whether the size-hint is valid. */
    mutable bool  m_fHintValid;
    /** Holds the width-hint. */
    mutable int   m_iWidthHint;
    /** Holds the size-hint. */
    mutable QSize m_ownSizeHint;

    /** Holds the Copy action instance. */
    QAction *m_pCopyAction;

    /** Holds text-copy reg-exp. */
    static const QRegExp s_regExpCopy;
    /** Holds text-elide reg-exp. */
    static QRegExp       s_regExpElide;
};

#endif /* !___QILabel_h___ */
