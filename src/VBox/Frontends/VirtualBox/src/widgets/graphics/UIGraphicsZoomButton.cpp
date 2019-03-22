/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsZoomButton class definition.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QStateMachine>
# include <QSignalTransition>
# include <QPropertyAnimation>
# include <QPainter>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UIGraphicsZoomButton.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIGraphicsZoomButton::UIGraphicsZoomButton(QIGraphicsWidget *pParent, const QIcon &icon, int iDirection)
    : UIGraphicsButton(pParent, icon)
    , m_iIndent(4)
    , m_iDirection(iDirection)
    , m_iAnimationDuration(200)
    , m_pStateMachine(0)
    , m_pForwardAnimation(0)
    , m_pBackwardAnimation(0)
    , m_fStateDefault(true)
{
    /* Setup: */
    setAcceptHoverEvents(true);

    /* Create state machine: */
    m_pStateMachine = new QStateMachine(this);

    /* Create 'default' state: */
    QState *pStateDefault = new QState(m_pStateMachine);
    pStateDefault->assignProperty(this, "stateDefault", true);

    /* Create 'zoomed' state: */
    QState *pStateZoomed = new QState(m_pStateMachine);
    pStateZoomed->assignProperty(this, "stateDefault", false);

    /* Initial state is 'default': */
    m_pStateMachine->setInitialState(pStateDefault);

    /* Zoom animation: */
    m_pForwardAnimation = new QPropertyAnimation(this, "geometry", this);
    m_pForwardAnimation->setDuration(m_iAnimationDuration);

    /* Unzoom animation: */
    m_pBackwardAnimation = new QPropertyAnimation(this, "geometry", this);
    m_pBackwardAnimation->setDuration(m_iAnimationDuration);

    /* Add state transitions: */
    QSignalTransition *pDefaultToZoomed = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateZoomed);
    pDefaultToZoomed->addAnimation(m_pForwardAnimation);

    QSignalTransition *pZoomedToDefault = pStateZoomed->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
    pZoomedToDefault->addAnimation(m_pBackwardAnimation);

    /* Start state-machine: */
    m_pStateMachine->start();
}

int UIGraphicsZoomButton::indent() const
{
    return m_iIndent;
}

void UIGraphicsZoomButton::setIndent(int iIndent)
{
    m_iIndent = iIndent;
}

void UIGraphicsZoomButton::updateAnimation()
{
    QRectF oldRect = geometry();
    QRectF newRect = oldRect;
    if (m_iDirection & UIGraphicsZoomDirection_Top)
        newRect.setTop(newRect.top() - m_iIndent);
    if (m_iDirection & UIGraphicsZoomDirection_Bottom)
        newRect.setBottom(newRect.bottom() + m_iIndent);
    if (m_iDirection & UIGraphicsZoomDirection_Left)
        newRect.setLeft(newRect.left() - m_iIndent);
    if (m_iDirection & UIGraphicsZoomDirection_Right)
        newRect.setRight(newRect.right() + m_iIndent);
    if (!(m_iDirection & UIGraphicsZoomDirection_Left) &&
        !(m_iDirection & UIGraphicsZoomDirection_Right))
    {
        newRect.setLeft(newRect.left() - m_iIndent / 2);
        newRect.setRight(newRect.right() + m_iIndent / 2);
    }
    if (!(m_iDirection & UIGraphicsZoomDirection_Top) &&
        !(m_iDirection & UIGraphicsZoomDirection_Bottom))
    {
        newRect.setTop(newRect.top() - m_iIndent / 2);
        newRect.setBottom(newRect.bottom() + m_iIndent / 2);
    }
    m_pForwardAnimation->setStartValue(oldRect);
    m_pForwardAnimation->setEndValue(newRect);
    m_pBackwardAnimation->setStartValue(newRect);
    m_pBackwardAnimation->setEndValue(oldRect);
}

QVariant UIGraphicsZoomButton::data(int iKey) const
{
    /* Known key? */
    switch (iKey)
    {
        case GraphicsButton_Margin: return 1;
        default: break;
    }
    /* Call to base-class: */
    return UIGraphicsButton::data(iKey);
}

void UIGraphicsZoomButton::hoverEnterEvent(QGraphicsSceneHoverEvent*)
{
    emit sigHoverEnter();
}

void UIGraphicsZoomButton::hoverLeaveEvent(QGraphicsSceneHoverEvent*)
{
    emit sigHoverLeave();
}

void UIGraphicsZoomButton::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget*)
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    int iMargin = data(GraphicsButton_Margin).toInt();
    QRect paintRect = pOption->rect;
    paintRect.setTopLeft(paintRect.topLeft() + QPoint(iMargin, iMargin));
    paintRect.setBottomRight(paintRect.bottomRight() - QPoint(iMargin, iMargin));
    QIcon icon = data(GraphicsButton_Icon).value<QIcon>();
    QSize iconSize = data(GraphicsButton_IconSize).toSize();

    /* Make painter beauty: */
    pPainter->setRenderHint(QPainter::SmoothPixmapTransform);

    /* Draw pixmap: */
    pPainter->drawPixmap(/* Pixmap rectangle: */
                         paintRect,
                         /* Pixmap size: */
                         icon.pixmap(iconSize));

    /* Restore painter: */
    pPainter->restore();
}

bool UIGraphicsZoomButton::isAnimationRunning() const
{
    return m_pForwardAnimation->state() == QAbstractAnimation::Running ||
           m_pBackwardAnimation->state() == QAbstractAnimation::Running;
}

bool UIGraphicsZoomButton::stateDefault() const
{
    return m_fStateDefault;
}

void UIGraphicsZoomButton::setStateDefault(bool fStateDefault)
{
    m_fStateDefault = fStateDefault;
}

