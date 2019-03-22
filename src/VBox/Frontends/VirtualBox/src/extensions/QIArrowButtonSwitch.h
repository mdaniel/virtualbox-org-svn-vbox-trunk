/* $Id$ */
/** @file
 * VBox Qt GUI - QIArrowButtonSwitch class declaration.
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

#ifndef ___QIArrowButtonSwitch_h___
#define ___QIArrowButtonSwitch_h___

/* Qt includes: */
#include <QIcon>

/* GUI includes: */
#include "QIRichToolButton.h"
#include "UILibraryDefs.h"

/** QIRichToolButton extension
  * representing arrow tool-button with text-label,
  * can be used as collaps/expand switch in various places. */
class SHARED_LIBRARY_STUFF QIArrowButtonSwitch : public QIRichToolButton
{
    Q_OBJECT;

public:

    /** Constructs button passing @a pParent to the base-class. */
    QIArrowButtonSwitch(QWidget *pParent = 0);

    /** Defines the @a iconCollapsed and the @a iconExpanded. */
    void setIcons(const QIcon &iconCollapsed, const QIcon &iconExpanded);

    /** Defines whether the button is @a fExpanded. */
    void setExpanded(bool fExpanded);
    /** Returns whether the button is expanded. */
    bool isExpanded() const { return m_fExpanded; }

protected slots:

    /** Handles button-click. */
    virtual void sltButtonClicked();

protected:

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Updates icon according button-state. */
    void updateIcon() { setIcon(m_fExpanded ? m_iconExpanded : m_iconCollapsed); }

    /** Holds whether the button is expanded. */
    bool m_fExpanded;

    /** Holds the icon for collapsed button. */
    QIcon m_iconCollapsed;
    /** Holds the icon for expanded button. */
    QIcon m_iconExpanded;
};

#endif /* !___QIArrowButtonSwitch_h___ */

