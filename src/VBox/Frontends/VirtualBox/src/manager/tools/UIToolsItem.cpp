/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsItem class definition.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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
# include <QAccessibleObject>
# include <QApplication>
# include <QGraphicsScene>
# include <QPainter>
# include <QPropertyAnimation>
# include <QSignalTransition>
# include <QStateMachine>
# include <QStyle>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UITools.h"
# include "UIToolsItem.h"
# include "UIToolsModel.h"
# include "UIToolsView.h"
# include "UIVirtualBoxManager.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleObject extension used as an accessibility interface for Tools-view items. */
class UIAccessibilityInterfaceForUIToolsItem : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating Tools-view accessibility interface: */
        if (pObject && strClassname == QLatin1String("UIToolsItem"))
            return new UIAccessibilityInterfaceForUIToolsItem(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    UIAccessibilityInterfaceForUIToolsItem(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Return the parent: */
        return QAccessible::queryAccessibleInterface(item()->model()->tools()->view());
    }

    /** Returns the number of children. */
    virtual int childCount() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Zero: */
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int) const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Null: */
        return 0;
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const /* override */
    {
        /* Search for corresponding child: */
        for (int i = 0; i < childCount(); ++i)
            if (child(i) == pChild)
                return i;

        /* -1 by default: */
        return -1;
    }

    /** Returns the rect. */
    virtual QRect rect() const /* override */
    {
        /* Now goes the mapping: */
        const QSize   itemSize         = item()->size().toSize();
        const QPointF itemPosInScene   = item()->mapToScene(QPointF(0, 0));
        const QPoint  itemPosInView    = item()->model()->tools()->view()->mapFromScene(itemPosInScene);
        const QPoint  itemPosInScreen  = item()->model()->tools()->view()->mapToGlobal(itemPosInView);
        const QRect   itemRectInScreen = QRect(itemPosInScreen, itemSize);
        return itemRectInScreen;
    }

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QString());

        switch (enmTextRole)
        {
            case QAccessible::Name:        return item()->name();
            /// @todo handle!
            //case QAccessible::Description: return item()->description();
            default: break;
        }

        /* Null-string by default: */
        return QString();
    }

    /** Returns the role. */
    virtual QAccessible::Role role() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QAccessible::NoRole);

        /* ListItem by default: */
        return QAccessible::ListItem;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const /* override */
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QAccessible::State());

        /* Compose the state: */
        QAccessible::State state;
        state.focusable = true;
        state.selectable = true;

        /* Compose the state of current item: */
        if (item() && item() == item()->model()->currentItem())
        {
            state.active = true;
            state.focused = true;
            state.selected = true;
        }

        /* Return the state: */
        return state;
    }

private:

    /** Returns corresponding Tools-view item. */
    UIToolsItem *item() const { return qobject_cast<UIToolsItem*>(object()); }
};


/*********************************************************************************************************************************
*   Class UIToolsItem implementation.                                                                                            *
*********************************************************************************************************************************/

UIToolsItem::UIToolsItem(QGraphicsScene *pScene,
                         UIToolsClass enmClass, UIToolsType enmType,
                         const QIcon &icon, const QString &strName)
    : m_pScene(pScene)
    , m_enmClass(enmClass)
    , m_enmType(enmType)
    , m_icon(icon)
    , m_strName(strName)
    , m_fHovered(false)
    , m_pHoveringMachine(0)
    , m_pHoveringAnimationForward(0)
    , m_pHoveringAnimationBackward(0)
    , m_iAnimationDuration(400)
    , m_iDefaultValue(100)
    , m_iHoveredValue(90)
    , m_iAnimatedValue(m_iDefaultValue)
    , m_iHoverLightnessMin(0)
    , m_iHoverLightnessMax(0)
    , m_iHighlightLightnessMin(0)
    , m_iHighlightLightnessMax(0)
    , m_iPreviousMinimumWidthHint(0)
    , m_iPreviousMinimumHeightHint(0)
    , m_iMaximumNameWidth(0)
{
    /* Prepare: */
    prepare();
}

UIToolsItem::~UIToolsItem()
{
    /* Cleanup: */
    cleanup();
}

UIToolsModel *UIToolsItem::model() const
{
    UIToolsModel *pModel = qobject_cast<UIToolsModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

void UIToolsItem::reconfigure(UIToolsClass enmClass, UIToolsType enmType,
                              const QIcon &icon, const QString &strName)
{
    /* If class is changed: */
    if (m_enmClass != enmClass)
    {
        /* Update linked values: */
        m_enmClass = enmClass;
    }

    /* If type is changed: */
    if (m_enmType != enmType)
    {
        /* Update linked values: */
        m_enmType = enmType;
    }

    /* Update linked values: */
    m_icon = icon;
    updatePixmap();

    /* If name is changed: */
    if (m_strName != strName)
    {
        /* Update linked values: */
        m_strName = strName;
        updateMinimumNameSize();
        updateVisibleName();
    }
}

UIToolsClass UIToolsItem::itemClass() const
{
    return m_enmClass;
}

UIToolsType UIToolsItem::itemType() const
{
    return m_enmType;
}

const QIcon &UIToolsItem::icon() const
{
    return m_icon;
}

const QString &UIToolsItem::name() const
{
    return m_strName;
}

void UIToolsItem::setHovered(bool fHovered)
{
    m_fHovered = fHovered;
    if (m_fHovered)
        emit sigHoverEnter();
    else
        emit sigHoverLeave();
}

bool UIToolsItem::isHovered() const
{
    return m_fHovered;
}

void UIToolsItem::updateGeometry()
{
    /* Call to base-class: */
    QIGraphicsWidget::updateGeometry();

    /* We should notify Tools-model if minimum-width-hint was changed: */
    const int iMinimumWidthHint = minimumWidthHint();
    if (m_iPreviousMinimumWidthHint != iMinimumWidthHint)
    {
        /* Save new minimum-width-hint, notify listener: */
        m_iPreviousMinimumWidthHint = iMinimumWidthHint;
        emit sigMinimumWidthHintChanged(m_iPreviousMinimumWidthHint);
    }
    /* We should notify Tools-model if minimum-height-hint was changed: */
    const int iMinimumHeightHint = minimumHeightHint();
    if (m_iPreviousMinimumHeightHint != iMinimumHeightHint)
    {
        /* Save new minimum-height-hint, notify listener: */
        m_iPreviousMinimumHeightHint = iMinimumHeightHint;
        emit sigMinimumHeightHintChanged(m_iPreviousMinimumHeightHint);
    }
}

int UIToolsItem::minimumWidthHint() const
{
    /* Prepare variables: */
    const int iMargin = data(ToolsItemData_Margin).toInt();
    const int iSpacing = data(ToolsItemData_Spacing).toInt();

    /* Calculating proposed width: */
    int iProposedWidth = 0;

    /* Two margins: */
    iProposedWidth += 2 * iMargin;
    /* And Tools-item content to take into account: */
    int iToolsItemWidth = m_pixmapSize.width() +
                          iSpacing +
                          m_minimumNameSize.width();
    iProposedWidth += iToolsItemWidth;

    /* Return result: */
    return iProposedWidth;
}

int UIToolsItem::minimumHeightHint() const
{
    /* Prepare variables: */
    const int iMargin = data(ToolsItemData_Margin).toInt();

    /* Calculating proposed height: */
    int iProposedHeight = 0;

    /* Two margins: */
    iProposedHeight += 2 * iMargin;
    /* And Tools-item content to take into account: */
    int iToolsItemHeight = qMax(m_pixmapSize.height(),
                                m_minimumNameSize.height());
    iProposedHeight += iToolsItemHeight;

    /* Return result: */
    return iProposedHeight;
}

QSizeF UIToolsItem::sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint /* = QSizeF() */) const
{
    /* If Qt::MinimumSize requested: */
    if (enmWhich == Qt::MinimumSize)
        return QSizeF(minimumWidthHint(), minimumHeightHint());
    /* Else call to base-class: */
    return QIGraphicsWidget::sizeHint(enmWhich, constraint);
}

void UIToolsItem::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::showEvent(pEvent);

    /* Update pixmap: */
    updatePixmap();
}

void UIToolsItem::resizeEvent(QGraphicsSceneResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIGraphicsWidget::resizeEvent(pEvent);

    /* What is the new geometry? */
    const QRectF newGeometry = geometry();

    /* Should we update visible name? */
    if (previousGeometry().width() != newGeometry.width())
        updateMaximumNameWidth();

    /* Remember the new geometry: */
    setPreviousGeometry(newGeometry);
}

void UIToolsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
        update();
    }
}

void UIToolsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_fHovered)
    {
        m_fHovered = false;
        emit sigHoverLeave();
        update();
    }
}

void UIToolsItem::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget * /* pWidget = 0 */)
{
    /* Setup: */
    pPainter->setRenderHint(QPainter::Antialiasing);

    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
    /* Paint frame: */
    paintFrame(pPainter, rectangle);
    /* Paint tool info: */
    paintToolInfo(pPainter, rectangle);
}

void UIToolsItem::sltHandleWindowRemapped()
{
    /* Update pixmap: */
    updatePixmap();
}

void UIToolsItem::prepare()
{
    /* Add item to the scene: */
    AssertMsg(m_pScene, ("Incorrect scene passed!"));
    m_pScene->addItem(this);

    /* Install Tools-view item accessibility interface factory: */
    QAccessible::installFactory(UIAccessibilityInterfaceForUIToolsItem::pFactory);

    /* Prepare color tones: */
#ifdef VBOX_WS_MAC
    m_iHighlightLightnessMin = 105;
    m_iHighlightLightnessMax = 115;
    m_iHoverLightnessMin = 110;
    m_iHoverLightnessMax = 120;
#else /* VBOX_WS_MAC */
    m_iHighlightLightnessMin = 120;
    m_iHighlightLightnessMax = 160;
    m_iHoverLightnessMin = 155;
    m_iHoverLightnessMax = 175;
#endif /* !VBOX_WS_MAC */

    /* Prepare fonts: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);

    /* Configure item options: */
    setOwnedByLayout(false);
    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    /* Prepare hover animation: */
    prepareHoverAnimation();
    /* Prepare connections: */
    prepareConnections();

    /* Init: */
    updatePixmap();
    updateMinimumNameSize();
    updateVisibleName();
}

void UIToolsItem::prepareHoverAnimation()
{
    /* Create hovering animation machine: */
    m_pHoveringMachine = new QStateMachine(this);
    if (m_pHoveringMachine)
    {
        /* Create 'default' state: */
        QState *pStateDefault = new QState(m_pHoveringMachine);
        /* Create 'hovered' state: */
        QState *pStateHovered = new QState(m_pHoveringMachine);

        /* Configure 'default' state: */
        if (pStateDefault)
        {
            /* When we entering default state => we assigning animatedValue to m_iDefaultValue: */
            pStateDefault->assignProperty(this, "animatedValue", m_iDefaultValue);

            /* Add state transitions: */
            QSignalTransition *pDefaultToHovered = pStateDefault->addTransition(this, SIGNAL(sigHoverEnter()), pStateHovered);
            if (pDefaultToHovered)
            {
                /* Create forward animation: */
                m_pHoveringAnimationForward = new QPropertyAnimation(this, "animatedValue", this);
                if (m_pHoveringAnimationForward)
                {
                    m_pHoveringAnimationForward->setDuration(m_iAnimationDuration);
                    m_pHoveringAnimationForward->setStartValue(m_iDefaultValue);
                    m_pHoveringAnimationForward->setEndValue(m_iHoveredValue);

                    /* Add to transition: */
                    pDefaultToHovered->addAnimation(m_pHoveringAnimationForward);
                }
            }
        }

        /* Configure 'hovered' state: */
        if (pStateHovered)
        {
            /* When we entering hovered state => we assigning animatedValue to m_iHoveredValue: */
            pStateHovered->assignProperty(this, "animatedValue", m_iHoveredValue);

            /* Add state transitions: */
            QSignalTransition *pHoveredToDefault = pStateHovered->addTransition(this, SIGNAL(sigHoverLeave()), pStateDefault);
            if (pHoveredToDefault)
            {
                /* Create backward animation: */
                m_pHoveringAnimationBackward = new QPropertyAnimation(this, "animatedValue", this);
                if (m_pHoveringAnimationBackward)
                {
                    m_pHoveringAnimationBackward->setDuration(m_iAnimationDuration);
                    m_pHoveringAnimationBackward->setStartValue(m_iHoveredValue);
                    m_pHoveringAnimationBackward->setEndValue(m_iDefaultValue);

                    /* Add to transition: */
                    pHoveredToDefault->addAnimation(m_pHoveringAnimationBackward);
                }
            }
        }

        /* Initial state is 'default': */
        m_pHoveringMachine->setInitialState(pStateDefault);
        /* Start state-machine: */
        m_pHoveringMachine->start();
    }
}

void UIToolsItem::prepareConnections()
{
    /* This => model connections: */
    connect(this, &UIToolsItem::sigMinimumWidthHintChanged,
            model(), &UIToolsModel::sltItemMinimumWidthHintChanged);
    connect(this, &UIToolsItem::sigMinimumHeightHintChanged,
            model(), &UIToolsModel::sltItemMinimumHeightHintChanged);

    /* Manager => this connections: */
    connect(gpManager, &UIVirtualBoxManager::sigWindowRemapped,
            this, &UIToolsItem::sltHandleWindowRemapped);
}

void UIToolsItem::cleanup()
{
    /* If that item is focused: */
    if (model()->focusItem() == this)
    {
        /* Unset the focus item: */
        model()->setFocusItem(0);
    }
    /* If that item is current: */
    if (model()->currentItem() == this)
    {
        /* Unset the current item: */
        model()->setCurrentItem(0);
    }
    /* If that item is in navigation list: */
    if (model()->navigationList().contains(this))
    {
        /* Remove item from the navigation list: */
        model()->removeFromNavigationList(this);
    }
}

QVariant UIToolsItem::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ToolsItemData_Margin:  return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 3 * 2;
        case ToolsItemData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;

        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIToolsItem::updatePixmap()
{
    /* Prepare variables: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);

    /* Prepare new pixmap size: */
    const QSize pixmapSize = QSize(iIconMetric, iIconMetric);
    const QPixmap pixmap = m_icon.pixmap(gpManager->windowHandle(), pixmapSize);
    /* Update linked values: */
    if (m_pixmapSize != pixmapSize)
    {
        m_pixmapSize = pixmapSize;
        updateMaximumNameWidth();
        updateGeometry();
    }
    if (m_pixmap.toImage() != pixmap.toImage())
    {
        m_pixmap = pixmap;
        update();
    }
}

void UIToolsItem::updateMinimumNameSize()
{
    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new minimum name size: */
    const QFontMetrics fm(m_nameFont, pPaintDevice);
    const int iWidthOf15Letters = textWidthMonospace(m_nameFont, pPaintDevice, 15);
    const QString strNameCompressedTo15Letters = compressText(m_nameFont, pPaintDevice, m_strName, iWidthOf15Letters);
    const QSize minimumNameSize = QSize(fm.width(strNameCompressedTo15Letters), fm.height());

    /* Update linked values: */
    if (m_minimumNameSize != minimumNameSize)
    {
        m_minimumNameSize = minimumNameSize;
        updateGeometry();
    }
}

void UIToolsItem::updateMaximumNameWidth()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsItemData_Margin).toInt();
    const int iSpacing = data(ToolsItemData_Spacing).toInt();

    /* Calculate new maximum name width: */
    int iMaximumNameWidth = (int)geometry().width();
    iMaximumNameWidth -= iMargin; /* left margin */
    iMaximumNameWidth -= m_pixmapSize.width(); /* pixmap width */
    iMaximumNameWidth -= iSpacing; /* spacing between pixmap and name(s) */
    iMaximumNameWidth -= iMargin; /* right margin */

    /* Update linked values: */
    if (m_iMaximumNameWidth != iMaximumNameWidth)
    {
        m_iMaximumNameWidth = iMaximumNameWidth;
        updateVisibleName();
    }
}

void UIToolsItem::updateVisibleName()
{
    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new visible name: */
    const QString strVisibleName = compressText(m_nameFont, pPaintDevice, m_strName, m_iMaximumNameWidth);

    /* Update linked values: */
    if (m_strVisibleName != strVisibleName)
    {
        m_strVisibleName = strVisibleName;
        update();
    }
}

/* static */
int UIToolsItem::textWidthMonospace(const QFont &font, QPaintDevice *pPaintDevice, int iCount)
{
    /* Return text width, based on font-metrics: */
    QFontMetrics fm(font, pPaintDevice);
    QString strString;
    strString.fill('_', iCount);
    return fm.width(strString);
}

/* static */
QString UIToolsItem::compressText(const QFont &font, QPaintDevice *pPaintDevice, QString strText, int iWidth)
{
    /* Check if passed text is empty: */
    if (strText.isEmpty())
        return strText;

    /* Check if passed text already fits maximum width: */
    QFontMetrics fm(font, pPaintDevice);
    if (fm.width(strText) <= iWidth)
        return strText;

    /* Truncate otherwise: */
    QString strEllipsis = QString("...");
    int iEllipsisWidth = fm.width(strEllipsis + " ");
    while (!strText.isEmpty() && fm.width(strText) + iEllipsisWidth > iWidth)
        strText.truncate(strText.size() - 1);
    return strText + strEllipsis;
}

void UIToolsItem::paintBackground(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const QPalette pal = palette();

    /* Selection background: */
    if (model()->currentItem() == this)
    {
        /* Prepare color: */
        const QColor backgroundColor = isEnabled()
                                     ? pal.color(QPalette::Active, QPalette::Highlight)
                                     : pal.color(QPalette::Disabled, QPalette::Midlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iHighlightLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iHighlightLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);
    }
    /* Hovering background: */
    else if (isHovered())
    {
        /* Prepare color: */
        const QColor backgroundColor = isEnabled()
                                     ? pal.color(QPalette::Active, QPalette::Highlight)
                                     : pal.color(QPalette::Disabled, QPalette::Midlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iHoverLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iHoverLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);
    }
    /* Default background: */
    else
    {
        /* Prepare color: */
        const QColor backgroundColor = isEnabled()
                                     ? pal.color(QPalette::Active, QPalette::Mid)
                                     : pal.color(QPalette::Disabled, QPalette::Midlight);
        /* Draw gradient: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
        bgGrad.setColorAt(0, backgroundColor.lighter(m_iHoverLightnessMax));
        bgGrad.setColorAt(1, backgroundColor.lighter(m_iHoverLightnessMin));
        pPainter->fillRect(rectangle, bgGrad);
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIToolsItem::paintFrame(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare colors: */
    const QPalette pal = palette();
    QColor strokeColor;

    /* Selection frame: */
    if (model()->currentItem() == this)
        strokeColor = pal.color(QPalette::Active, QPalette::Mid).darker(110);
    /* Default frame: */
    else
        strokeColor = pal.color(QPalette::Active, QPalette::Midlight).darker(110);

    /* Assign pen: */
    pPainter->setPen(strokeColor);

    /* Draw frame: */
    pPainter->drawLine(rectangle.topLeft(), rectangle.topRight() + QPoint(1, 0));
    pPainter->drawLine(rectangle.bottomLeft() + QPoint(0, 1), rectangle.bottomRight() + QPoint(1, 1));

    /* Restore painter: */
    pPainter->restore();
}

void UIToolsItem::paintToolInfo(QPainter *pPainter, const QRect &rectangle) const
{
    /* Prepare variables: */
    const int iFullHeight = rectangle.height();
    const int iMargin = data(ToolsItemData_Margin).toInt();
    const int iSpacing = data(ToolsItemData_Spacing).toInt();
    const QPalette pal = palette();

    /* Selected item foreground: */
    if (model()->currentItem() == this)
    {
        const QColor textColor = isEnabled()
                               ? pal.color(QPalette::Active, QPalette::HighlightedText)
                               : pal.color(QPalette::Disabled, QPalette::Text);
        pPainter->setPen(textColor);
    }
    /* Hovered item foreground: */
    else if (isHovered())
    {
        /* Prepare color: */
        QColor defaultHighlighted = pal.color(QPalette::Active, QPalette::Highlight);
        QColor hoveredHighlighted = defaultHighlighted.lighter(m_iHoverLightnessMax);
        QColor textColor;
        if (hoveredHighlighted.value() - hoveredHighlighted.saturation() > 0)
            textColor = isEnabled()
                      ? pal.color(QPalette::Active, QPalette::Text)
                      : pal.color(QPalette::Disabled, QPalette::Text);
        else
            textColor = isEnabled()
                      ? pal.color(QPalette::Active, QPalette::HighlightedText)
                      : pal.color(QPalette::Disabled, QPalette::Text);
        pPainter->setPen(textColor);
    }
    /* Default item foreground: */
    else
    {
        const QColor textColor = isEnabled()
                               ? pal.color(QPalette::Active, QPalette::Text)
                               : pal.color(QPalette::Disabled, QPalette::Text);
        pPainter->setPen(textColor);
    }

    /* Paint left column: */
    {
        /* Prepare variables: */
        int iPixmapX = iMargin;
        int iPixmapY = (iFullHeight - m_pixmap.height() / m_pixmap.devicePixelRatio()) / 2;
        /* Paint pixmap: */
        paintPixmap(/* Painter: */
                    pPainter,
                    /* Point to paint in: */
                    QPoint(iPixmapX, iPixmapY),
                    /* Pixmap to paint: */
                    m_pixmap);
    }

    /* Paint right column: */
    {
        /* Prepare variables: */
        int iNameX = iMargin + m_pixmapSize.width() + iSpacing;
        int iNameY = (iFullHeight - m_minimumNameSize.height()) / 2;
        /* Paint name: */
        paintText(/* Painter: */
                  pPainter,
                  /* Point to paint in: */
                  QPoint(iNameX, iNameY),
                  /* Font to paint text: */
                  m_nameFont,
                  /* Paint device: */
                  model()->paintDevice(),
                  /* Text to paint: */
                  m_strVisibleName);
    }
}

/* static */
void UIToolsItem::paintPixmap(QPainter *pPainter, const QPoint &point,
                              const QPixmap &pixmap)
{
    /* Draw pixmap: */
    pPainter->drawPixmap(point, pixmap);
}

/* static */
void UIToolsItem::paintText(QPainter *pPainter, QPoint point,
                            const QFont &font, QPaintDevice *pPaintDevice,
                            const QString &strText)
{
    /* Save painter: */
    pPainter->save();

    /* Assign font: */
    pPainter->setFont(font);

    /* Calculate ascent: */
    QFontMetrics fm(font, pPaintDevice);
    point += QPoint(0, fm.ascent());

    /* Draw text: */
    pPainter->drawText(point, strText);

    /* Restore painter: */
    pPainter->restore();
}
