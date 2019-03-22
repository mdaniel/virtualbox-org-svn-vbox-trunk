/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITabWidget class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "QITabWidget.h"


QITabWidget::QITabWidget(QWidget *pParent /* = 0 */)
    : QTabWidget(pParent)
{
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // I don't know why, but for some languages there is
    // ElideRight the default on Mac OS X. Fix this.
    setElideMode(Qt::ElideNone);
#endif
}
