/* $Id$ */
/** @file
 * VBox Qt GUI - UIAnimationFramework class declaration.
 */

/*
 * Copyright (C) 2013-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIAnimationFramework_h___
#define ___UIAnimationFramework_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declaration: */
class QPropertyAnimation;
class QState;
class QStateMachine;


/** QObject subclass used as animation factory. */
class SHARED_LIBRARY_STUFF UIAnimation : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listener about 'Start' state entered. */
    void sigStateEnteredStart();
    /** Notifies listener about 'Final' state entered. */
    void sigStateEnteredFinal();

public:

    /** Installs property animation.
      * @param  pTarget                    Brings the object being animated.
      * @param  pszPropertyName            Brings the name of property being animated.
      * @param  pszValuePropertyNameStart  Brings the name of the property holding 'start' value.
      * @param  pszValuePropertyNameFinal  Brings the name of the property holding 'final' value.
      * @param  pszSignalForward           Brings the signal to start forward animation.
      * @param  pszSignalReverse           Brings the signal to start reverse animation.
      * @param  fReverse                   Brings whether the animation should be inverted.
      * @param  iAnimationDuration         Brings the animation duration. */
    static UIAnimation *installPropertyAnimation(QWidget *pTarget, const char *pszPropertyName,
                                                 const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                                 const char *pszSignalForward, const char *pszSignalReverse,
                                                 bool fReverse = false, int iAnimationDuration = 300);

    /** Updates the animation, fetching new initial values. */
    void update();

private:

    /** Constructs animation. Doesn't mean to be used directly.
      * @param  pParent                    Brings the object animation being applied to.
      * @param  pszPropertyName            Brings the name of property being animated.
      * @param  pszValuePropertyNameStart  Brings the name of the property holding 'start' value.
      * @param  pszValuePropertyNameFinal  Brings the name of the property holding 'final' value.
      * @param  pszSignalForward           Brings the signal to start forward animation.
      * @param  pszSignalReverse           Brings the signal to start reverse animation.
      * @param  fReverse                   Brings whether the animation should be inverted.
      * @param  iAnimationDuration         Brings the animation duration. */
    UIAnimation(QWidget *pParent, const char *pszPropertyName,
                const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                const char *pszSignalForward, const char *pszSignalReverse,
                bool fReverse, int iAnimationDuration);

    /** Prepares all. */
    void prepare();

    /** Holds the name of property being animated. */
    const char *m_pszPropertyName;

    /** Holds the name of the property holding 'start' value. */
    const char *m_pszValuePropertyNameStart;
    /** Holds the name of the property holding 'final' value. */
    const char *m_pszValuePropertyNameFinal;

    /** Holds the signal to start forward animation. */
    const char *m_pszSignalForward;
    /** Holds the signal to start reverse animation. */
    const char *m_pszSignalReverse;

    /** Holds whether the animation should be inverted. */
    bool m_fReverse;

    /** Holds the animation duration. */
    int m_iAnimationDuration;

    /** Holds the animation machine instance. */
    QStateMachine      *m_pAnimationMachine;
    /** Holds the instance of the animation 'Start' state. */
    QState             *m_pStateStart;
    /** Holds the instance of the animation 'Final' state. */
    QState             *m_pStateFinal;
    /** Holds the instance of the 'Forward' animation. */
    QPropertyAnimation *m_pForwardAnimation;
    /** Holds the instance of the 'Reverse' animation. */
    QPropertyAnimation *m_pReverseAnimation;
};


/** QObject subclass used as animation loop factory. */
class SHARED_LIBRARY_STUFF UIAnimationLoop : public QObject
{
    Q_OBJECT;

public:

    /** Installs property animation.
      * @param  pTarget                    Brings the object being animated.
      * @param  pszPropertyName            Brings the name of property being animated.
      * @param  pszValuePropertyNameStart  Brings the name of the property holding 'start' value.
      * @param  pszValuePropertyNameFinal  Brings the name of the property holding 'final' value.
      * @param  iAnimationDuration         Brings the animation duration. */
    static UIAnimationLoop *installAnimationLoop(QWidget *pTarget, const char *pszPropertyName,
                                                 const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                                                 int iAnimationDuration = 300);

    /** Updates the animation, fetching new initial values. */
    void update();

    /** Starts the loop. */
    void start();
    /** Stops the loop. */
    void stop();

private:

    /** Constructs animation loop. Doesn't mean to be used directly.
      * @param  pParent                    Brings the object animation being applied to.
      * @param  pszPropertyName            Brings the name of property being animated.
      * @param  pszValuePropertyNameStart  Brings the name of the property holding 'start' value.
      * @param  pszValuePropertyNameFinal  Brings the name of the property holding 'final' value.
      * @param  iAnimationDuration         Brings the animation duration. */
    UIAnimationLoop(QWidget *pParent, const char *pszPropertyName,
                    const char *pszValuePropertyNameStart, const char *pszValuePropertyNameFinal,
                    int iAnimationDuration);

    /** Prepares all. */
    void prepare();

    /** Holds the name of property being animated. */
    const char *m_pszPropertyName;

    /** Holds the name of the property holding 'start' value. */
    const char *m_pszValuePropertyNameStart;
    /** Holds the name of the property holding 'final' value. */
    const char *m_pszValuePropertyNameFinal;

    /** Holds the animation duration. */
    int m_iAnimationDuration;

    /** Holds the instance of the animation. */
    QPropertyAnimation *m_pAnimation;
};


#endif /* !___UIAnimationFramework_h___ */

