/* $Id$ */
/** @file
 * VBox Qt GUI - UISlidingAnimation class declaration.
 */

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISlidingAnimation_h___
#define ___UISlidingAnimation_h___

/* Qt includes: */
#include <QWidget>

/* Forward declarations: */
class QLabel;
class QRect;
class QWidget;
class UIAnimation;


/** Sliding direction. */
enum SlidingDirection
{
    SlidingDirection_Forward,
    SlidingDirection_Reverse
};


/** QWidget extension which renders a sliding animation
  * while transiting from one widget to another. */
class UISlidingAnimation : public QWidget
{
    Q_OBJECT;
    Q_PROPERTY(QRect widgetGeometry READ widgetGeometry WRITE setWidgetGeometry);
    Q_PROPERTY(QRect startWidgetGeometry READ startWidgetGeometry);
    Q_PROPERTY(QRect finalWidgetGeometry READ finalWidgetGeometry);

signals:

    /** Commands to move animation in forward direction. */
    void sigForward();
    /** Commands to move animation in reverse direction. */
    void sigReverse();

    /** Notifies listeners about animation in specified @a enmDirection is complete. */
    void sigAnimationComplete(SlidingDirection enmDirection);

public:

    /** Constructs sliding animation passing @a pParent to the base-class.
      * @param  enmOrientation  Brings the widget orientation.
      * @param  fReverse        Brings whether the animation should be initially reversed. */
    UISlidingAnimation(Qt::Orientation enmOrientation, bool fReverse, QWidget *pParent = 0);

    /** Defines @a pWidget1 and @a pWidget2. */
    void setWidgets(QWidget *pWidget1, QWidget *pWidget2);

    /** Animates cached pWidget1 and pWidget2 in passed @a enmDirection. */
    void animate(SlidingDirection enmDirection);

private slots:

    /** Handles entering for 'Start' state. */
    void sltHandleStateEnteredStart() { emit sigAnimationComplete(SlidingDirection_Reverse); }
    /** Handles entering for 'Final' state. */
    void sltHandleStateEnteredFinal() { emit sigAnimationComplete(SlidingDirection_Forward); }

private:

    /** Prepares all. */
    void prepare();

    /** Defines sub-window geometry. */
    void setWidgetGeometry(const QRect &rect);
    /** Returns sub-window geometry. */
    QRect widgetGeometry() const;
    /** Returns sub-window start-geometry. */
    QRect startWidgetGeometry() const { return m_startWidgetGeometry; }
    /** Returns sub-window final-geometry. */
    QRect finalWidgetGeometry() const { return m_finalWidgetGeometry; }

    /** Holds the widget orientation. */
    Qt::Orientation  m_enmOrientation;
    /** Holds whether the animation should be initially reversed. */
    bool             m_fReverse;
    /** Holds the animation instance. */
    UIAnimation     *m_pAnimation;
    /** Holds sub-window start-geometry. */
    QRect            m_startWidgetGeometry;
    /** Holds sub-window final-geometry. */
    QRect            m_finalWidgetGeometry;

    /** Holds the sliding widget instance. */
    QWidget     *m_pWidget;
    /** Holds the 1st label instance. */
    QLabel      *m_pLabel1;
    /** Holds the 2nd label instance. */
    QLabel      *m_pLabel2;

    /** Holds the 1st rendered-widget reference. */
    QWidget     *m_pWidget1;
    /** Holds the 2nd rendered-widget reference. */
    QWidget     *m_pWidget2;
};

#endif /* !___UISlidingAnimation_h___ */
