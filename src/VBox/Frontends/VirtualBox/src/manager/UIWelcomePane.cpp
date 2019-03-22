/* $Id$ */
/** @file
 * VBox Qt GUI - UIWelcomePane class implementation.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
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
# include <QHBoxLayout>
# include <QLabel>
# include <QStyle>
# include <QVBoxLayout>

/* GUI includes */
# include "QIWithRetranslateUI.h"
# include "UIIconPool.h"
# include "UIWelcomePane.h"

/* Forward declarations: */
class QEvent;
class QHBoxLayout;
class QString;
class QResizeEvent;
class QVBoxLayout;

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Wrappable QLabel extension for tools pane of the desktop widget.
  * The main idea behind this stuff is to allow dynamically calculate
  * [minimum] size hint for changeable one-the-fly widget width.
  * That's a "white unicorn" task for QLabel which never worked since
  * the beginning, because out-of-the-box version just uses static
  * hints calculation which is very stupid taking into account
  * QLayout "eats it raw" and tries to be dynamical on it's basis. */
class UIWrappableLabel : public QLabel
{
    Q_OBJECT;

public:

    /** Constructs wrappable label passing @a pParent to the base-class. */
    UIWrappableLabel(QWidget *pParent = 0);

protected:

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Returns whether the widget's preferred height depends on its width. */
    virtual bool hasHeightForWidth() const /* override */;

    /** Holds the minimum widget size. */
    virtual QSize minimumSizeHint() const /* override */;

    /** Holds the preferred widget size. */
    virtual QSize sizeHint() const /* override */;
};


/*********************************************************************************************************************************
*   Class UIWrappableLabel implementation.                                                                                       *
*********************************************************************************************************************************/

UIWrappableLabel::UIWrappableLabel(QWidget *pParent /* = 0 */)
    : QLabel(pParent)
{
}

void UIWrappableLabel::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QLabel::resizeEvent(pEvent);

    // WORKAROUND:
    // That's not cheap procedure but we need it to
    // make sure geometry is updated after width changed.
    if (minimumWidth() > 0)
        updateGeometry();
}

bool UIWrappableLabel::hasHeightForWidth() const
{
    // WORKAROUND:
    // No need to panic, we do it ourselves in resizeEvent() and
    // this 'false' here to prevent automatic layout fighting for it.
    return   minimumWidth() > 0
           ? false
           : QLabel::hasHeightForWidth();
}

QSize UIWrappableLabel::minimumSizeHint() const /* override */
{
    // WORKAROUND:
    // We should calculate hint height on the basis of width,
    // keeping the hint width equal to minimum we have set.
    return   minimumWidth() > 0
           ? QSize(minimumWidth(), heightForWidth(width()))
           : QLabel::minimumSizeHint();
}

QSize UIWrappableLabel::sizeHint() const /* override */
{
    // WORKAROUND:
    // Keep widget always minimal.
    return minimumSizeHint();
}


/*********************************************************************************************************************************
*   Class UIWelcomePane implementation.                                                                                          *
*********************************************************************************************************************************/

UIWelcomePane::UIWelcomePane(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pLabelText(0)
    , m_pLabelIcon(0)
{
    /* Prepare: */
    prepare();
}

bool UIWelcomePane::event(QEvent *pEvent)
{
    /* Handle known event types: */
    switch (pEvent->type())
    {
        case QEvent::Show:
        case QEvent::ScreenChangeInternal:
        {
            /* Update pixmap: */
            updatePixmap();
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QIWithRetranslateUI<QWidget>::event(pEvent);
}

void UIWelcomePane::retranslateUi()
{
    /* Translate welcome text: */
    m_pLabelText->setText(tr("<h3>Welcome to VirtualBox!</h3>"
                             "<p>The left part of application window contains global tools and "
                             "lists all virtual machines and virtual machine groups on your computer. "
                             "You can import, add and create new VMs using corresponding toolbar buttons. "
                             "You can popup a tools of currently selected element using corresponding element button.</p>"
                             "<p>You can press the <b>%1</b> key to get instant help, or visit "
                             "<a href=https://www.virtualbox.org>www.virtualbox.org</a> "
                             "for more information and latest news.</p>")
                             .arg(QKeySequence(QKeySequence::HelpContents).toString(QKeySequence::NativeText)));
}

void UIWelcomePane::prepare()
{
    /* Prepare default welcome icon: */
    m_icon = UIIconPool::iconSet(":/tools_banner_global_200px.png");

    /* Create main layout: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    if (pMainLayout)
    {
        /* Create welcome layout: */
        QHBoxLayout *pLayoutWelcome = new QHBoxLayout;
        if (pLayoutWelcome)
        {
            /* Configure layout: */
            const int iL = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 2;
            pLayoutWelcome->setContentsMargins(iL, 0, 0, 0);

            /* Create welcome text label: */
            m_pLabelText = new UIWrappableLabel;
            if (m_pLabelText)
            {
                /* Configure label: */
                m_pLabelText->setWordWrap(true);
                m_pLabelText->setMinimumWidth(160); /// @todo make dynamic
                m_pLabelText->setAlignment(Qt::AlignLeading | Qt::AlignTop);
                m_pLabelText->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

                /* Add into layout: */
                pLayoutWelcome->addWidget(m_pLabelText);
            }

            /* Create welcome picture label: */
            m_pLabelIcon = new QLabel;
            if (m_pLabelIcon)
            {
                /* Configure label: */
                m_pLabelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

                /* Add into layout: */
                pLayoutWelcome->addWidget(m_pLabelIcon);
                pLayoutWelcome->setAlignment(m_pLabelIcon, Qt::AlignHCenter | Qt::AlignTop);
            }

            /* Add into layout: */
            pMainLayout->addLayout(pLayoutWelcome);
        }

        /* Add stretch: */
        pMainLayout->addStretch();
    }

    /* Translate finally: */
    retranslateUi();
    /* Update pixmap: */
    updatePixmap();
}

void UIWelcomePane::updatePixmap()
{
    /* Assign corresponding icon: */
    if (!m_icon.isNull())
    {
        const QList<QSize> sizes = m_icon.availableSizes();
        const QSize firstOne = sizes.isEmpty() ? QSize(200, 200) : sizes.first();
        m_pLabelIcon->setPixmap(m_icon.pixmap(window()->windowHandle(),
                                                       QSize(firstOne.width(),
                                                             firstOne.height())));
    }
}


#include "UIWelcomePane.moc"
