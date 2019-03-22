/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsButton class definition.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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
# include <QPainter>
# include <QGraphicsSceneMouseEvent>

/* GUI includes: */
# include "UIGraphicsButton.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGraphicsButton::UIGraphicsButton(QIGraphicsWidget *pParent, const QIcon &icon)
    : QIGraphicsWidget(pParent)
    , m_icon(icon)
    , m_buttonType(UIGraphicsButtonType_Iconified)
    , m_fParentSelected(false)
{
    /* Refresh finally: */
    refresh();
}

UIGraphicsButton::UIGraphicsButton(QIGraphicsWidget *pParent, UIGraphicsButtonType buttonType)
    : QIGraphicsWidget(pParent)
    , m_buttonType(buttonType)
    , m_fParentSelected(false)
{
    /* Refresh finally: */
    refresh();
}

void UIGraphicsButton::setParentSelected(bool fParentSelected)
{
    if (m_fParentSelected == fParentSelected)
        return;
    m_fParentSelected = fParentSelected;
    update();
}

QVariant UIGraphicsButton::data(int iKey) const
{
    switch (iKey)
    {
        case GraphicsButton_Margin: return 0;
        case GraphicsButton_IconSize: return m_icon.isNull() ? QSize(16, 16) : m_icon.availableSizes().first();
        case GraphicsButton_Icon: return m_icon;
        default: break;
    }
    return QVariant();
}

QSizeF UIGraphicsButton::sizeHint(Qt::SizeHint which, const QSizeF &constraint /* = QSizeF() */) const
{
    /* Calculations for minimum size: */
    if (which == Qt::MinimumSize)
    {
        /* Variables: */
        int iMargin = data(GraphicsButton_Margin).toInt();
        QSize iconSize = data(GraphicsButton_IconSize).toSize();
        /* Calculations: */
        int iWidth = 2 * iMargin + iconSize.width();
        int iHeight = 2 * iMargin + iconSize.height();
        return QSize(iWidth, iHeight);
    }
    /* Call to base-class: */
    return QIGraphicsWidget::sizeHint(which, constraint);
}

void UIGraphicsButton::paint(QPainter *pPainter, const QStyleOptionGraphicsItem* /* pOption */, QWidget* /* pWidget = 0 */)
{
    /* Prepare variables: */
    int iMargin = data(GraphicsButton_Margin).toInt();
    QIcon icon = data(GraphicsButton_Icon).value<QIcon>();
    QSize iconSize = data(GraphicsButton_IconSize).toSize();

    /* Which type button has: */
    switch (m_buttonType)
    {
        case UIGraphicsButtonType_Iconified:
        {
            /* Just draw the pixmap: */
            pPainter->drawPixmap(QRect(QPoint(iMargin, iMargin), iconSize), icon.pixmap(iconSize));
            break;
        }
        case UIGraphicsButtonType_DirectArrow:
        {
            /* Prepare variables: */
            QPalette pal = palette();
            QColor buttonColor = pal.color(m_fParentSelected ? QPalette::HighlightedText : QPalette::Mid);
#ifdef VBOX_WS_MAC
            /* Mac is using only light standard highlight colors, keeping highlight-text color always black.
             * User can choose a darker (non-standard) highlight color but it will be his visibility problem.
             * I think using highlight-text color (black) for arrow-buttons is too ugly,
             * so the corresponding color will be received from the highlight color: */
            if (m_fParentSelected)
                buttonColor = pal.color(QPalette::Highlight).darker(150);
#endif /* VBOX_WS_MAC */

            /* Setup: */
            pPainter->setRenderHint(QPainter::Antialiasing);
            QPen pen = pPainter->pen();
            pen.setColor(buttonColor);
            pen.setWidth(2);
            pen.setCapStyle(Qt::RoundCap);

            /* Draw path: */
            QPainterPath circlePath;
            circlePath.moveTo(iMargin, iMargin);
            circlePath.lineTo(iMargin + iconSize.width() / 2, iMargin);
            circlePath.arcTo(QRectF(circlePath.currentPosition(), iconSize).translated(-iconSize.width() / 2, 0), 90, -180);
            circlePath.lineTo(iMargin, iMargin + iconSize.height());
            circlePath.closeSubpath();
            pPainter->strokePath(circlePath, pen);

            /* Draw triangle: */
            QPainterPath linePath;
            linePath.moveTo(iMargin + 5, iMargin + 5);
            linePath.lineTo(iMargin + iconSize.height() - 5, iMargin + iconSize.width() / 2);
            linePath.lineTo(iMargin + 5, iMargin + iconSize.width() - 5);
            pPainter->strokePath(linePath, pen);
            break;
        }
        case UIGraphicsButtonType_RoundArrow:
        {
            /* Prepare variables: */
            QPalette pal = palette();
            QColor buttonColor = pal.color(m_fParentSelected ? QPalette::HighlightedText : QPalette::Mid);
#ifdef VBOX_WS_MAC
            /* Mac is using only light standard highlight colors, keeping highlight-text color always black.
             * User can choose a darker (non-standard) highlight color but it will be his visibility problem.
             * I think using highlight-text color (black) for arrow-buttons is too ugly,
             * so the corresponding color will be received from the highlight color: */
            if (m_fParentSelected)
                buttonColor = pal.color(QPalette::Highlight).darker(150);
#endif /* VBOX_WS_MAC */

            /* Setup: */
            pPainter->setRenderHint(QPainter::Antialiasing);
            QPen pen = pPainter->pen();
            pen.setColor(buttonColor);
            pen.setWidth(2);
            pen.setCapStyle(Qt::RoundCap);

            /* Draw circle: */
            QPainterPath circlePath;
            circlePath.moveTo(iMargin, iMargin);
            circlePath.addEllipse(QRectF(circlePath.currentPosition(), iconSize));
            pPainter->strokePath(circlePath, pen);

            /* Draw triangle: */
            QPainterPath linePath;
            linePath.moveTo(iMargin + 5, iMargin + 5);
            linePath.lineTo(iMargin + iconSize.height() - 5, iMargin + iconSize.width() / 2);
            linePath.lineTo(iMargin + 5, iMargin + iconSize.width() - 5);
            pPainter->strokePath(linePath, pen);
            break;
        }
    }
}

void UIGraphicsButton::mousePressEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Accepting this event allows to get release-event: */
    pEvent->accept();
}

void UIGraphicsButton::mouseReleaseEvent(QGraphicsSceneMouseEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::mouseReleaseEvent(pEvent);
    /* Notify listeners about button click: */
    emit sigButtonClicked();
}

void UIGraphicsButton::refresh()
{
    /* Refresh geometry: */
    updateGeometry();
    /* Resize to minimum size: */
    resize(minimumSizeHint());
}

