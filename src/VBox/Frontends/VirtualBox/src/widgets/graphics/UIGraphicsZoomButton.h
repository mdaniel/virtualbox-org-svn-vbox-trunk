/* $Id$ */
/** @file
 * VBox Qt GUI - UIGraphicsZoomButton class declaration.
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

#ifndef __UIGraphicsZoomButton_h__
#define __UIGraphicsZoomButton_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIGraphicsButton.h"

/* Other VBox includes: */
#include "iprt/assert.h"

/* Forward declarations: */
class QStateMachine;
class QPropertyAnimation;

/* Zoom direction: */
enum UIGraphicsZoomDirection
{
    UIGraphicsZoomDirection_Top    = RT_BIT(0),
    UIGraphicsZoomDirection_Bottom = RT_BIT(1),
    UIGraphicsZoomDirection_Left   = RT_BIT(2),
    UIGraphicsZoomDirection_Right  = RT_BIT(3)
};

/* Zoom graphics-button representation: */
class UIGraphicsZoomButton : public UIGraphicsButton
{
    Q_OBJECT;
    Q_PROPERTY(bool stateDefault READ stateDefault WRITE setStateDefault);

signals:

    /* Notify listeners about hover events: */
    void sigHoverEnter();
    void sigHoverLeave();

public:

    /* Constructor: */
    UIGraphicsZoomButton(QIGraphicsWidget *pParent, const QIcon &icon, int iDirection);

    /* API: Zoom stuff: */
    int indent() const;
    void setIndent(int iIndent);

    /* API: Animation stuff: */
    void updateAnimation();

protected:

    /* Data provider: */
    QVariant data(int iKey) const;

    /* Handler: Mouse hover: */
    void hoverEnterEvent(QGraphicsSceneHoverEvent *pEvent);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent);

    /* Paint stuff: */
    void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOption, QWidget *pWidget = 0);

private:

    /* Animation stuff: */
    bool isAnimationRunning() const;

    /* Property stuff: */
    bool stateDefault() const;
    void setStateDefault(bool fStateDefault);

    /* Variables: */
    int m_iIndent;
    int m_iDirection;
    int m_iAnimationDuration;
    QStateMachine *m_pStateMachine;
    QPropertyAnimation *m_pForwardAnimation;
    QPropertyAnimation *m_pBackwardAnimation;
    bool m_fStateDefault;
};

#endif /* __UIGraphicsZoomButton_h__ */

