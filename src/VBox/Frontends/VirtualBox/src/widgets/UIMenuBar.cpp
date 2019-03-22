/* $Id$ */
/** @file
 * VBox Qt GUI - UIMenuBar class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QPainter>
# include <QPaintEvent>
# include <QPixmapCache>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIImageTools.h"
# include "UIMenuBar.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMenuBar::UIMenuBar(QWidget *pParent /* = 0 */)
    : QMenuBar(pParent)
    , m_fShowBetaLabel(false)
{
    /* Check for beta versions: */
    if (vboxGlobal().isBeta())
        m_fShowBetaLabel = true;
}

void UIMenuBar::paintEvent(QPaintEvent *pEvent)
{
    /* Call to base-class: */
    QMenuBar::paintEvent(pEvent);

    /* Draw BETA label if necessary: */
    if (m_fShowBetaLabel)
    {
        QPixmap betaLabel;
        const QString key("vbox:betaLabel");
        if (!QPixmapCache::find(key, betaLabel))
        {
            betaLabel = ::betaLabel();
            QPixmapCache::insert(key, betaLabel);
        }
        QSize s = size();
        QPainter painter(this);
        painter.setClipRect(pEvent->rect());
        painter.drawPixmap(s.width() - betaLabel.width() - 10, (height() - betaLabel.height()) / 2, betaLabel);
    }
}
