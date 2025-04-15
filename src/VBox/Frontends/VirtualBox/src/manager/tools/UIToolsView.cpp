/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsView class implementation.
 */

/*
 * Copyright (C) 2012-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QAccessibleWidget>
#include <QApplication>

/* GUI includes: */
#include "UICommon.h"
#include "UITools.h"
#include "UIToolsItem.h"
#include "UIToolsModel.h"
#include "UIToolsView.h"
#include "UITranslationEventListener.h"

/* Other VBox includes: */
#include <iprt/assert.h>


/** QAccessibleWidget extension used as an accessibility interface for Tools-view. */
class UIAccessibilityInterfaceForUIToolsView : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating Tools-view accessibility interface: */
        if (pObject && strClassname == QLatin1String("UIToolsView"))
            return new UIAccessibilityInterfaceForUIToolsView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    UIAccessibilityInterfaceForUIToolsView(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
    {}

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), 0);

        /* Return the number of children: */
        return view()->model()->items().size();
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const RT_OVERRIDE
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), 0);
        /* Make sure index is valid: */
        AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

        /* Return the child with the passed iIndex: */
        return QAccessible::queryAccessibleInterface(view()->model()->items().at(iIndex));
    }

    /** Returns the index of passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), -1);
        /* Make sure child is valid: */
        AssertReturn(pChild, -1);

        /* Return the index of passed model child: */
        return view()->model()->items().indexOf(qobject_cast<UIToolsItem*>(pChild->object()));
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
    {
        /* Make sure view still alive: */
        AssertPtrReturn(view(), QString());

        /* Return view tool-tip: */
        Q_UNUSED(enmTextRole);
        return view()->toolTip();
    }

private:

    /** Returns corresponding Tools-view. */
    UIToolsView *view() const { return qobject_cast<UIToolsView*>(widget()); }
};


UIToolsView::UIToolsView(QWidget *pParent, UIToolsModel *pModel)
    : QIGraphicsView(pParent)
    , m_pModel(pModel)
    , m_iMinimumWidthHint(0)
    , m_iMinimumHeightHint(0)
{
    prepare();
}

UIToolsView::~UIToolsView()
{
    cleanup();
}

QSize UIToolsView::minimumSizeHint() const
{
    return QSize(2 * frameWidth() + m_iMinimumWidthHint,
                 2 * frameWidth() + m_iMinimumHeightHint);
}

QSize UIToolsView::sizeHint() const
{
    return minimumSizeHint();
}

void UIToolsView::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsView::resizeEvent(pEvent);

    /* Update model's layout: */
    model()->updateLayout();
}

void UIToolsView::sltRetranslateUI()
{
    setWhatsThis(tr("Contains a list of VirtualBox tools."));
}

void UIToolsView::sltMinimumWidthHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumWidthHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumWidthHint = iHint;

    /* Update geometry & scene-rect: */
    updateGeometry();
    updateSceneRect();
}

void UIToolsView::sltMinimumHeightHintChanged(int iHint)
{
    /* Is there something changed? */
    if (m_iMinimumHeightHint == iHint)
        return;

    /* Remember new value: */
    m_iMinimumHeightHint = iHint;

    /* Update geometry & scene-rect: */
    updateGeometry();
    updateSceneRect();
}

void UIToolsView::prepare()
{
    /* Install Tools-view accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUIToolsView::pFactory);

    /* Prepare everything: */
    prepareThis();
    preparePalette();
    prepareConnections();

    /* Update scene-rect: */
    updateSceneRect();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIToolsView::prepareThis()
{
    /* Exchange information with model: */
    setScene(model()->scene());
    model()->setView(this);

    /* Set minimum size-hint policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    /* Setup frame: */
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    /* Setup scroll-bars policy: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void UIToolsView::preparePalette()
{
    /* Setup palette: */
    QPalette pal = qApp->palette();

    /* We are just taking the [in]active Window colors and
     * making them a bit darker/lighter according to theme: */
    QColor backgroundColorActive = pal.color(QPalette::Active, QPalette::Window);
    QColor backgroundColorInactive = pal.color(QPalette::Inactive, QPalette::Window);
    backgroundColorActive = uiCommon().isInDarkMode()
                          ? backgroundColorActive.lighter(120)
                          : backgroundColorActive.darker(108);
    backgroundColorInactive = uiCommon().isInDarkMode()
                            ? backgroundColorInactive.lighter(120)
                            : backgroundColorInactive.darker(108);
    pal.setColor(QPalette::Active, QPalette::Base, backgroundColorActive);
    pal.setColor(QPalette::Inactive, QPalette::Base, backgroundColorInactive);

    /* Assing changed palette: */
    setPalette(pal);
#ifdef VBOX_WS_WIN
    // WORKAROUND:
    // New Windows Modern look&feel style have different palettes for view
    // and viewport, so we are assigning viewport palette as well.
    viewport()->setPalette(pal);
#endif
}

void UIToolsView::prepareConnections()
{
    /* Translation signal: */
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIToolsView::sltRetranslateUI);

    /* Model connections: */
    connect(model(), &UIToolsModel::sigItemMinimumWidthHintChanged,
            this, &UIToolsView::sltMinimumWidthHintChanged);
    connect(model(), &UIToolsModel::sigItemMinimumHeightHintChanged,
            this, &UIToolsView::sltMinimumHeightHintChanged);
}

void UIToolsView::cleanupConnections()
{
    /* Model connections: */
    disconnect(model(), &UIToolsModel::sigItemMinimumWidthHintChanged,
               this, &UIToolsView::sltMinimumWidthHintChanged);
    disconnect(model(), &UIToolsModel::sigItemMinimumHeightHintChanged,
               this, &UIToolsView::sltMinimumHeightHintChanged);
}

void UIToolsView::cleanup()
{
    /* Cleanup everything: */
    cleanupConnections();
}

void UIToolsView::updateSceneRect()
{
    setSceneRect(0, 0, m_iMinimumWidthHint, m_iMinimumHeightHint);
}
