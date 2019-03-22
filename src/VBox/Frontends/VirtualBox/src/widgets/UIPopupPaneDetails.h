/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupPaneDetails class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIPopupPaneDetails_h___
#define ___UIPopupPaneDetails_h___

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QTextEdit;
class UIAnimation;

/** QWidget extension providing GUI with popup-pane details-pane prototype class. */
class SHARED_LIBRARY_STUFF UIPopupPaneDetails : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QSize collapsedSizeHint READ collapsedSizeHint);
    Q_PROPERTY(QSize expandedSizeHint READ expandedSizeHint);
    Q_PROPERTY(QSize minimumSizeHint READ minimumSizeHint WRITE setMinimumSizeHint);

signals:

    /** Notifies about focus enter. */
    void sigFocusEnter();
    /** Notifies about focus enter. */
    void sigFocusLeave();

    /** Notifies about size-hint change. */
    void sigSizeHintChanged();

public:

    /** Constructs details-pane passing @a pParent to the base-class.
      * @param  strText   Brings the details text.
      * @param  fFcoused  Brings whether the pane is focused. */
    UIPopupPaneDetails(QWidget *pParent, const QString &strText, bool fFocused);

    /** Defines the details @a strText. */
    void setText(const QString &strText);

    /** Returns the details minimum size-hint. */
    QSize minimumSizeHint() const;
    /** Defines the details @a minimumSizeHint. */
    void setMinimumSizeHint(const QSize &minimumSizeHint);
    /** Lays the content out. */
    void layoutContent();

    /** Returns the collapsed size-hint. */
    QSize collapsedSizeHint() const { return m_collapsedSizeHint; }
    /** Returns the expanded size-hint. */
    QSize expandedSizeHint() const { return m_expandedSizeHint; }

public slots:

    /** Handles proposal for @a iWidth. */
    void sltHandleProposalForWidth(int iWidth);
    /** Handles proposal for @a iHeight. */
    void sltHandleProposalForHeight(int iHeight);

    /** Handles focus enter. */
    void sltFocusEnter();
    /** Handles focus leave. */
    void sltFocusLeave();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares content. */
    void prepareContent();
    /** Prepares animations. */
    void prepareAnimation();

    /** Updates size-hint. */
    void updateSizeHint();
    /** Updates visibility. */
    void updateVisibility();

    /** Adjusts @a font. */
    static QFont tuneFont(QFont font);

    /** Holds the layout margin. */
    const int m_iLayoutMargin;
    /** Holds the layout spacing. */
    const int m_iLayoutSpacing;

    /** Holds the text-editor size-hint. */
    QSize m_textEditSizeHint;
    /** Holds the collapsed size-hint. */
    QSize m_collapsedSizeHint;
    /** Holds the expanded size-hint. */
    QSize m_expandedSizeHint;
    /** Holds the minimum size-hint. */
    QSize m_minimumSizeHint;

    /** Holds the text. */
    QString m_strText;

    /** Holds the text-editor instance. */
    QTextEdit *m_pTextEdit;

    /** Holds the desired textr-editor width. */
    int m_iDesiredTextEditWidth;
    /** Holds the maximum pane height. */
    int m_iMaximumPaneHeight;
    /** Holds the desired textr-editor height. */
    int m_iMaximumTextEditHeight;
    /** Holds the text content margin. */
    int m_iTextContentMargin;

    /** Holds whether details-pane is focused. */
    bool m_fFocused;

    /** Holds the animation instance. */
    UIAnimation *m_pAnimation;
};

#endif /* !___UIPopupPaneDetails_h___ */

