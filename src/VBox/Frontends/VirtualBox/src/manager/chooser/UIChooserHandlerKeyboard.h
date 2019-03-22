/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserHandlerKeyboard class declaration.
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

#ifndef __UIChooserHandlerKeyboard_h__
#define __UIChooserHandlerKeyboard_h__
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QObject>
#include <QMap>

/* Forward declarations: */
class UIChooserModel;
class QKeyEvent;

/* Keyboard event type: */
enum UIKeyboardEventType
{
    UIKeyboardEventType_Press,
    UIKeyboardEventType_Release
};

/* Item shift direction: */
enum UIItemShiftDirection
{
    UIItemShiftDirection_Up,
    UIItemShiftDirection_Down
};

/* Item shift size: */
enum UIItemShiftSize
{
    UIItemShiftSize_Item,
    UIItemShiftSize_Full
};

/* Keyboard handler for graphics selector: */
class UIChooserHandlerKeyboard : public QObject
{
    Q_OBJECT;

public:

    /* Constructor: */
    UIChooserHandlerKeyboard(UIChooserModel *pParent);

    /* API: Model keyboard-event handler delegate: */
    bool handle(QKeyEvent *pEvent, UIKeyboardEventType type) const;

private:

    /* API: Model wrapper: */
    UIChooserModel* model() const;

    /* Helpers: Model keyboard-event handler delegates: */
    bool handleKeyPress(QKeyEvent *pEvent) const;
    bool handleKeyRelease(QKeyEvent *pEvent) const;

    /* Helper: Item shift delegate: */
    void shift(UIItemShiftDirection direction, UIItemShiftSize size) const;

    /* Variables: */
    UIChooserModel *m_pModel;
    QMap<int, UIItemShiftSize> m_shiftMap;
};

#endif /* __UIChooserHandlerKeyboard_h__ */

