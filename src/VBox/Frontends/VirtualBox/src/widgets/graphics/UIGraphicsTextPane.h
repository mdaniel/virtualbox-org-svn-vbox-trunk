/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsTextPane class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsTextPane_h
#define FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsTextPane_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "UITextTable.h"

/* Forward declarations: */
class QTextLayout;


/** QIGraphicsWidget reimplementation to draw QTextLayout content. */
class UIGraphicsTextPane : public QIGraphicsWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about size-hint changes. */
    void sigGeometryChanged();

    /** Notifies listeners about anchor clicked. */
    void sigAnchorClicked(const QString &strAnchor);

public:

    /** Graphics text-pane constructor. */
    UIGraphicsTextPane(QIGraphicsWidget *pParent, QPaintDevice *pPaintDevice);
    /** Graphics text-pane destructor. */
    ~UIGraphicsTextPane();

    /** Returns whether contained text is empty. */
    bool isEmpty() const { return m_text.isEmpty(); }
    /** Returns contained text. */
    UITextTable &text() { return m_text; }
    /** Defines contained text. */
    void setText(const UITextTable &text);

    /** Defines whether passed @a strAnchorRole is @a fRestricted. */
    void setAnchorRoleRestricted(const QString &strAnchorRole, bool fRestricted);

private:

    /** Update text-layout. */
    void updateTextLayout(bool fFull = false);

    /** Notifies listeners about size-hint changes. */
    void updateGeometry();
    /** Returns the size-hint to constrain the content. */
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    /** This event handler is delivered after the widget has been resized. */
    void resizeEvent(QGraphicsSceneResizeEvent *pEvent);

    /** This event handler called when mouse hovers widget. */
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent);
    /** This event handler called when mouse leaves widget. */
    void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent);
    /** Summarize two hover-event handlers above. */
    void handleHoverEvent(QGraphicsSceneHoverEvent *pEvent);
    /** Update hover stuff. */
    void updateHoverStuff();

    /** This event handler called when mouse press widget. */
    void mousePressEvent(QGraphicsSceneMouseEvent *pEvent);

    /** Paints the contents in local coordinates. */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

    /** Builds new text-layout. */
    static QTextLayout* buildTextLayout(const QFont &font, QPaintDevice *pPaintDevice,
                                        const QString &strText, int iWidth, int &iHeight,
                                        const QString &strHoveredAnchor);

    /** Search for hovered anchor in passed text-layout @a list. */
    static QString searchForHoveredAnchor(QPaintDevice *pPaintDevice, const QList<QTextLayout*> &list, const QPoint &mousePosition);

    /** Paint-device to scale to. */
    QPaintDevice *m_pPaintDevice;

    /** Margin. */
    const int m_iMargin;
    /** Spacing. */
    const int m_iSpacing;
    /** Minimum text-column width: */
    const int m_iMinimumTextColumnWidth;

    /** Minimum size-hint invalidation flag. */
    mutable bool m_fMinimumSizeHintInvalidated;
    /** Minimum size-hint cache. */
    mutable QSizeF m_minimumSizeHint;
    /** Minimum text-width. */
    int m_iMinimumTextWidth;
    /** Minimum text-height. */
    int m_iMinimumTextHeight;

    /** Contained text. */
    UITextTable m_text;
    /** Left text-layout list. */
    QList<QTextLayout*> m_leftList;
    /** Right text-layout list. */
    QList<QTextLayout*> m_rightList;

    /** Holds whether anchor can be hovered. */
    bool m_fAnchorCanBeHovered;
    /** Holds restricted anchor roles. */
    QSet<QString> m_restrictedAnchorRoles;
    /** Holds currently hovered anchor. */
    QString m_strHoveredAnchor;
};

#endif /* !FEQT_INCLUDED_SRC_widgets_graphics_UIGraphicsTextPane_h */
