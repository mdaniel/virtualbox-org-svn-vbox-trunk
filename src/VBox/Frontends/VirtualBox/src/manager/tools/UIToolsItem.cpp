/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsItem class definition.
 */

/*
 * Copyright (C) 2012-2024 Oracle and/or its affiliates.
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
#include <QAccessibleObject>
#include <QApplication>
#include <QGraphicsScene>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSignalTransition>
#include <QStateMachine>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QToolTip>
#include <QWindow>

/* GUI includes: */
#include "UICommon.h"
#include "UIImageTools.h"
#include "UITools.h"
#include "UIToolsItem.h"
#include "UIToolsModel.h"
#include "UIToolsView.h"
#include "UIVirtualBoxManager.h"


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
    virtual QAccessibleInterface *parent() const RT_OVERRIDE
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Return the parent: */
        return QAccessible::queryAccessibleInterface(item()->model()->tools()->view());
    }

    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Zero: */
        return 0;
    }

    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int) const RT_OVERRIDE
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), 0);

        /* Null: */
        return 0;
    }

    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const RT_OVERRIDE
    {
        /* Search for corresponding child: */
        for (int i = 0; i < childCount(); ++i)
            if (child(i) == pChild)
                return i;

        /* -1 by default: */
        return -1;
    }

    /** Returns the rect. */
    virtual QRect rect() const RT_OVERRIDE
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
    virtual QString text(QAccessible::Text enmTextRole) const RT_OVERRIDE
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
    virtual QAccessible::Role role() const RT_OVERRIDE
    {
        /* Make sure item still alive: */
        AssertPtrReturn(item(), QAccessible::NoRole);

        /* ListItem by default: */
        return QAccessible::ListItem;
    }

    /** Returns the state. */
    virtual QAccessible::State state() const RT_OVERRIDE
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

UIToolsItem::UIToolsItem(QGraphicsScene *pScene, const QIcon &icon,
                         UIToolClass enmClass, UIToolType enmType)
    : m_pScene(pScene)
    , m_icon(icon)
    , m_enmClass(enmClass)
    , m_enmType(enmType)
    , m_fHovered(false)
    , m_pHoveringMachine(0)
    , m_pHoveringAnimationForward(0)
    , m_pHoveringAnimationBackward(0)
    , m_iAnimationDuration(400)
    , m_iDefaultValue(0)
    , m_iHoveredValue(100)
    , m_iAnimatedValue(m_iDefaultValue)
    , m_iDefaultLightnessStart(0)
    , m_iDefaultLightnessFinal(0)
    , m_iHoverLightnessStart(0)
    , m_iHoverLightnessFinal(0)
    , m_iHighlightLightnessStart(0)
    , m_iHighlightLightnessFinal(0)
    , m_iPreviousMinimumWidthHint(0)
    , m_iPreviousMinimumHeightHint(0)
{
    prepare();
}

UIToolsItem::~UIToolsItem()
{
    cleanup();
}

UIToolsModel *UIToolsItem::model() const
{
    UIToolsModel *pModel = qobject_cast<UIToolsModel*>(QIGraphicsWidget::scene()->parent());
    AssertMsg(pModel, ("Incorrect graphics scene parent set!"));
    return pModel;
}

void UIToolsItem::setName(const QString &strName)
{
    /* If name changed: */
    if (m_strName != strName)
    {
        /* Update linked values: */
        m_strName = strName;
        updateNameSize();
    }
}

void UIToolsItem::setEnabled(bool fEnabled)
{
    /* Call to base-class: */
    QIGraphicsWidget::setEnabled(fEnabled);

    /* Update linked values: */
    updatePixmap();
}

void UIToolsItem::setHovered(bool fHovered)
{
    m_fHovered = fHovered;
    if (m_fHovered)
        emit sigHoverEnter();
    else
        emit sigHoverLeave();
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

    /* Add 2 margins by default: */
    iProposedWidth += 2 * iMargin;
#ifdef VBOX_WS_MAC
    /* Additional 3 margins for widget mode: */
    if (!model()->tools()->isPopup())
        iProposedWidth += 3 * iMargin;
#else
    /* Additional 1 margin for widget mode: */
    if (!model()->tools()->isPopup())
        iProposedWidth += iMargin;
#endif

    /* Add pixmap size by default: */
    iProposedWidth += m_pixmapSize.width();

    /* Add text size for popup mode or
     * if it's requested for widget mode: */
    if (   model()->tools()->isPopup()
        || model()->showItemNames())
    {
        iProposedWidth += m_nameSize.width();

        /* Add 1 spacing by default: */
        iProposedWidth += iSpacing;
        /* Additional 1 spacing for widget mode: */
        if (!model()->tools()->isPopup())
            iProposedWidth += iSpacing;
    }

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
                                m_nameSize.height());
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

void UIToolsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *)
{
    if (!m_fHovered)
    {
        m_fHovered = true;
        emit sigHoverEnter();
        update();

        /* Show tooltip at the right of item for widget mode: */
        if (   !model()->tools()->isPopup()
            && !model()->showItemNames())
        {
            const QPointF posAtScene = mapToScene(rect().topRight() + QPoint(3, -3));
            const QPoint posAtScreen = model()->tools()->mapToGlobal(posAtScene.toPoint());
            QToolTip::showText(posAtScreen, name());
        }
    }
}

void UIToolsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_fHovered)
    {
        m_fHovered = false;
        emit sigHoverLeave();
        update();

        /* Hide tooltip for good: */
        if (!model()->tools()->isPopup())
            QToolTip::hideText();
    }
}

void UIToolsItem::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget * /* pWidget = 0 */)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
    /* Paint frame for popup only: */
    if (model()->tools()->isPopup())
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
#if defined(VBOX_WS_MAC)
    m_iDefaultLightnessStart = 120;
    m_iDefaultLightnessFinal = 110;
    m_iHoverLightnessStart = 125;
    m_iHoverLightnessFinal = 115;
    m_iHighlightLightnessStart = 115;
    m_iHighlightLightnessFinal = 105;
#elif defined(VBOX_WS_WIN)
    m_iDefaultLightnessStart = 120;
    m_iDefaultLightnessFinal = 110;
    m_iHoverLightnessStart = 220;
    m_iHoverLightnessFinal = 210;
    m_iHighlightLightnessStart = 190;
    m_iHighlightLightnessFinal = 180;
#else /* !VBOX_WS_MAC && !VBOX_WS_WIN */
    m_iDefaultLightnessStart = 110;
    m_iDefaultLightnessFinal = 100;
    m_iHoverLightnessStart = 125;
    m_iHoverLightnessFinal = 115;
    m_iHighlightLightnessStart = 110;
    m_iHighlightLightnessFinal = 100;
#endif /* !VBOX_WS_MAC && !VBOX_WS_WIN */

    /* Prepare fonts: */
    m_nameFont = font();
    m_nameFont.setWeight(QFont::Bold);

    /* Configure item options: */
    setOwnedByLayout(false);
    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    /* Prepare hover animation for popup mode only: */
    if (model()->tools()->isPopup())
        prepareHoverAnimation();
    /* Prepare connections: */
    prepareConnections();

    /* Init: */
    updatePixmap();
    updateNameSize();
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
        case ToolsItemData_Margin: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 3 * 2;
        case ToolsItemData_Spacing: return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 2;
        case ToolsItemData_Padding: return 4;

        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIToolsItem::updatePixmap()
{
    /* Prepare variables: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) * 1.5;

    /* Prepare new pixmap size: */
    const QSize pixmapSize = QSize(iIconMetric, iIconMetric);
    const qreal fDevicePixelRatio = gpManager->windowHandle() ? gpManager->windowHandle()->devicePixelRatio() : 1;
    const QPixmap pixmap = m_icon.pixmap(pixmapSize, fDevicePixelRatio, isEnabled() ? QIcon::Normal : QIcon::Disabled);
    /* Update linked values: */
    if (m_pixmapSize != pixmapSize)
    {
        m_pixmapSize = pixmapSize;
        updateGeometry();
    }
    if (m_pixmap.toImage() != pixmap.toImage())
    {
        m_pixmap = pixmap;
        update();
    }
}

void UIToolsItem::updateNameSize()
{
    /* Prepare variables: */
    QPaintDevice *pPaintDevice = model()->paintDevice();

    /* Calculate new name size: */
    const QFontMetrics fm(m_nameFont, pPaintDevice);
    const QSize nameSize = QSize(fm.horizontalAdvance(m_strName), fm.height());

    /* Update linked values: */
    if (m_nameSize != nameSize)
    {
        m_nameSize = nameSize;
        updateGeometry();
    }
}

void UIToolsItem::paintBackground(QPainter *pPainter, const QRect &rectangle) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const QPalette pal = QApplication::palette();

    /* For popup: */
    if (model()->tools()->isPopup())
    {
        /* Selection background: */
        if (model()->currentItem() == this)
        {
            /* Prepare color: */
            const QColor backgroundColor = isEnabled()
                                         ? pal.color(QPalette::Active, QPalette::Highlight)
                                         : pal.color(QPalette::Disabled, QPalette::Window);
            /* Draw gradient: */
            QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
            bgGrad.setColorAt(0, backgroundColor.lighter(m_iHighlightLightnessStart));
            bgGrad.setColorAt(1, backgroundColor.lighter(m_iHighlightLightnessFinal));
            pPainter->fillRect(rectangle, bgGrad);

            if (isEnabled() && isHovered())
            {
                /* Prepare color: */
                QColor animationColor1 = QColor(Qt::white);
                QColor animationColor2 = QColor(Qt::white);
#ifdef VBOX_WS_MAC
                animationColor1.setAlpha(90);
#else
                animationColor1.setAlpha(30);
#endif
                animationColor2.setAlpha(0);
                /* Draw hovering animated gradient: */
                QRect animatedRect = rectangle;
                animatedRect.setWidth(animatedRect.height());
                const int iLength = 2 * animatedRect.width() + rectangle.width();
                const int iShift = - animatedRect.width() + iLength * animatedValue() / 100;
                animatedRect.moveLeft(iShift);
                QLinearGradient bgAnimatedGrad(animatedRect.topLeft(), animatedRect.bottomRight());
                bgAnimatedGrad.setColorAt(0,   animationColor2);
                bgAnimatedGrad.setColorAt(0.1, animationColor2);
                bgAnimatedGrad.setColorAt(0.5, animationColor1);
                bgAnimatedGrad.setColorAt(0.9, animationColor2);
                bgAnimatedGrad.setColorAt(1,   animationColor2);
                pPainter->fillRect(rectangle, bgAnimatedGrad);
            }
        }
        /* Hovering background: */
        else if (isHovered())
        {
            /* Prepare color: */
            const QColor backgroundColor = isEnabled()
                                         ? pal.color(QPalette::Active, QPalette::Highlight)
                                         : pal.color(QPalette::Disabled, QPalette::Window);
            /* Draw gradient: */
            QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
            bgGrad.setColorAt(0, backgroundColor.lighter(m_iHoverLightnessStart));
            bgGrad.setColorAt(1, backgroundColor.lighter(m_iHoverLightnessFinal));
            pPainter->fillRect(rectangle, bgGrad);

            if (isEnabled())
            {
                /* Prepare color: */
                QColor animationColor1 = QColor(Qt::white);
                QColor animationColor2 = QColor(Qt::white);
#ifdef VBOX_WS_MAC
                animationColor1.setAlpha(120);
#else
                animationColor1.setAlpha(50);
#endif
                animationColor2.setAlpha(0);
                /* Draw hovering animated gradient: */
                QRect animatedRect = rectangle;
                animatedRect.setWidth(animatedRect.height());
                const int iLength = 2 * animatedRect.width() + rectangle.width();
                const int iShift = - animatedRect.width() + iLength * animatedValue() / 100;
                animatedRect.moveLeft(iShift);
                QLinearGradient bgAnimatedGrad(animatedRect.topLeft(), animatedRect.bottomRight());
                bgAnimatedGrad.setColorAt(0,   animationColor2);
                bgAnimatedGrad.setColorAt(0.1, animationColor2);
                bgAnimatedGrad.setColorAt(0.5, animationColor1);
                bgAnimatedGrad.setColorAt(0.9, animationColor2);
                bgAnimatedGrad.setColorAt(1,   animationColor2);
                pPainter->fillRect(rectangle, bgAnimatedGrad);
            }
        }
        /* Default background: */
        else
        {
            /* Prepare color: */
            const QColor backgroundColor = isEnabled()
                                         ? pal.color(QPalette::Active, QPalette::Window)
                                         : pal.color(QPalette::Disabled, QPalette::Window);
            /* Draw gradient: */
            QLinearGradient bgGrad(rectangle.topLeft(), rectangle.bottomLeft());
            bgGrad.setColorAt(0, backgroundColor.lighter(m_iDefaultLightnessStart));
            bgGrad.setColorAt(1, backgroundColor.lighter(m_iDefaultLightnessFinal));
            pPainter->fillRect(rectangle, bgGrad);
        }
    }
    /* For widget: */
    else
    {
        /* Selection background: */
        if (model()->currentItem() == this)
        {
            /* Acquire token color: */
            const QColor highlightColor = isEnabled()
                                        ? pal.color(QPalette::Active, QPalette::Highlight)
                                        : pal.color(QPalette::Disabled, QPalette::Highlight);
            const QColor highlightColor1 = uiCommon().isInDarkMode()
                                         ? highlightColor.lighter(m_iHighlightLightnessStart + 20)
                                         : highlightColor.darker(m_iHighlightLightnessStart + 20);
            const QColor highlightColor2 = uiCommon().isInDarkMode()
                                         ? highlightColor.lighter(m_iHighlightLightnessFinal + 20)
                                         : highlightColor.darker(m_iHighlightLightnessFinal + 20);

            /* Prepare token sub-rect: */
            QRect tokenRect(rectangle.topLeft() + QPoint(0, 4),
                            QSize(5, rectangle.height() - 8));

            /* Draw gradient token: */
            QLinearGradient hlGrad(tokenRect.topLeft(), tokenRect.bottomLeft());
            hlGrad.setColorAt(0, highlightColor1);
            hlGrad.setColorAt(1, highlightColor2);
            pPainter->fillRect(tokenRect, hlGrad);
        }

        /* Hovering background for widget: */
        if (isHovered())
        {
            /* Prepare variables: */
            const int iMargin = data(ToolsItemData_Margin).toInt();
            const int iPadding = data(ToolsItemData_Padding).toInt();

            /* Configure painter: */
            pPainter->setRenderHint(QPainter::Antialiasing, true);
            /* Acquire background color: */
#ifdef VBOX_WS_MAC
            const QColor backgroundColor = pal.color(QPalette::Active, QPalette::Window);
#else /* !VBOX_WS_MAC */
            const QColor windowColor = pal.color(QPalette::Active, QPalette::Window);
            const QColor accentColor = pal.color(QPalette::Active, QPalette::Accent);
            const int iRed = iShift30(windowColor.red(), accentColor.red());
            const int iGreen = iShift30(windowColor.green(), accentColor.green());
            const int iBlue = iShift30(windowColor.blue(), accentColor.blue());
            const QColor backgroundColor = QColor(qRgb(iRed, iGreen, iBlue));
#endif /* !VBOX_WS_MAC */

            /* Prepare icon sub-rect: */
            QRect subRect;
            subRect.setHeight(m_pixmap.height() / m_pixmap.devicePixelRatio() + iPadding * 2);
            subRect.setWidth(subRect.height());
#ifdef VBOX_WS_MAC
            subRect.moveTopLeft(rectangle.topLeft() + QPoint(2.5 * iMargin - iPadding, iMargin - iPadding));
#else
            subRect.moveTopLeft(rectangle.topLeft() + QPoint(1.5 * iMargin - iPadding, iMargin - iPadding));
#endif

            /* Paint icon frame: */
            QPainterPath painterPath;
            painterPath.addRoundedRect(subRect, iPadding, iPadding);
#ifdef VBOX_WS_MAC
            const QColor backgroundColor1 = uiCommon().isInDarkMode()
                                          ? backgroundColor.lighter(220)
                                          : backgroundColor.darker(140);
#else /* !VBOX_WS_MAC */
            const QColor backgroundColor1 = uiCommon().isInDarkMode()
                                          ? backgroundColor.lighter(140)
                                          : backgroundColor.darker(120);
#endif /* !VBOX_WS_MAC */
            pPainter->setPen(QPen(backgroundColor1, 2, Qt::SolidLine, Qt::RoundCap));
            pPainter->drawPath(QPainterPathStroker().createStroke(painterPath));

            /* Fill icon body: */
            pPainter->setClipPath(painterPath);
#ifdef VBOX_WS_MAC
            const QColor backgroundColor2 = uiCommon().isInDarkMode()
                                          ? backgroundColor.lighter(160)
                                          : backgroundColor.darker(120);
#else /* !VBOX_WS_MAC */
            const QColor backgroundColor2 = uiCommon().isInDarkMode()
                                          ? backgroundColor.lighter(105)
                                          : backgroundColor.darker(105);
#endif /* !VBOX_WS_MAC */
            pPainter->fillRect(subRect, backgroundColor2);
        }
    }

    /* Restore painter: */
    pPainter->restore();
}

void UIToolsItem::paintFrame(QPainter *pPainter, const QRect &rectangle) const
{
    /* Don't paint frame for disabled items: */
    if (!isEnabled())
        return;

    /* Save painter: */
    pPainter->save();

    /* Prepare colors: */
    const QPalette pal = QApplication::palette();
    QColor strokeColor;

    /* Selection frame: */
    if (model()->currentItem() == this)
        strokeColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(m_iHighlightLightnessStart - 40);
    /* Hovering frame: */
    else if (isHovered())
        strokeColor = pal.color(QPalette::Active, QPalette::Highlight).lighter(m_iHoverLightnessStart - 40);
    /* Default frame: */
    else
        strokeColor = pal.color(QPalette::Active, QPalette::Window).lighter(m_iDefaultLightnessStart);

    /* Create/assign pen: */
    QPen pen(strokeColor);
    pen.setWidth(0);
    pPainter->setPen(pen);

    /* Draw borders: */
    pPainter->drawLine(rectangle.topLeft(),    rectangle.topRight());
    pPainter->drawLine(rectangle.bottomLeft(), rectangle.bottomRight());
    pPainter->drawLine(rectangle.topLeft(),    rectangle.bottomLeft());
    pPainter->drawLine(rectangle.topRight(),   rectangle.bottomRight());

    /* Restore painter: */
    pPainter->restore();
}

void UIToolsItem::paintToolInfo(QPainter *pPainter, const QRect &rectangle) const
{
    /* Prepare variables: */
    const int iFullHeight = rectangle.height();
    const int iMargin = data(ToolsItemData_Margin).toInt();
    const int iSpacing = data(ToolsItemData_Spacing).toInt();
    const QPalette pal = QApplication::palette();

    /* Selected or hovered item foreground for popup mode: */
    if (   model()->tools()->isPopup()
        && (model()->currentItem() == this || isHovered()))
    {
        /* Get background color: */
        const QColor highlight = pal.color(QPalette::Active, QPalette::Highlight);
        const QColor background = model()->currentItem() == this
                                ? highlight.lighter(m_iHighlightLightnessStart)
                                : highlight.lighter(m_iHoverLightnessStart);

        /* Gather foreground color for background one: */
        const QColor foreground = suitableForegroundColor(pal, background);
        pPainter->setPen(foreground);
    }
    /* Default item foreground: */
    else
    {
        const QColor foreground = isEnabled()
                                ? pal.color(QPalette::Active, QPalette::Text)
                                : pal.color(QPalette::Disabled, QPalette::Text);
        pPainter->setPen(foreground);
    }

    /* Paint left column: */
    {
        /* Prepare variables: */
#ifdef VBOX_WS_MAC
        const int iPixmapX = model()->tools()->isPopup() ? iMargin : 2.5 * iMargin;
#else
        const int iPixmapX = model()->tools()->isPopup() ? iMargin : 1.5 * iMargin;
#endif
        const int iPixmapY = (iFullHeight - m_pixmap.height() / m_pixmap.devicePixelRatio()) / 2;
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
#ifdef VBOX_WS_MAC
        const int iNameX = model()->tools()->isPopup() ? iMargin + m_pixmapSize.width() + iSpacing
                                                       : 2.5 * iMargin + m_pixmapSize.width() + 2 * iSpacing;
#else
        const int iNameX = model()->tools()->isPopup() ? iMargin + m_pixmapSize.width() + iSpacing
                                                       : 1.5 * iMargin + m_pixmapSize.width() + 2 * iSpacing;
#endif
        const int iNameY = (iFullHeight - m_nameSize.height()) / 2;
        /* Paint name (always for popup mode, if requested otherwise): */
        if (   model()->tools()->isPopup()
            || model()->showItemNames())
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iNameX, iNameY),
                      /* Font to paint text: */
                      m_nameFont,
                      /* Paint device: */
                      model()->paintDevice(),
                      /* Text to paint: */
                      m_strName);
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

#ifndef VBOX_WS_MAC
/* static */
int UIToolsItem::iShift30(int i1, int i2)
{
    const int iMin = qMin(i1, i2);
    const int iMax = qMax(i1, i2);
    const int iDiff = iMax - iMin;
    const int iDiff10 = iDiff * 0.3;

    int iResult = 0;
    if (i1 > i2)
        iResult = i1 - iDiff10;
    else
        iResult = i1 + iDiff10;

    return qMin(255, iResult);
}
#endif /* !VBOX_WS_MAC */
