/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsItem class definition.
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
#include <QAccessibleObject>
#include <QApplication>
#include <QGraphicsScene>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>
#include <QToolTip>
#include <QWindow>

/* GUI includes: */
#include "UICommon.h"
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
        return QAccessible::queryAccessibleInterface(item()->model()->view());
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
        const QPoint  itemPosInView    = item()->model()->view()->mapFromScene(itemPosInScene);
        const QPoint  itemPosInScreen  = item()->model()->view()->mapToGlobal(itemPosInView);
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
        if (item() && item() == item()->model()->currentItem(item()->itemClass()))
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

UIToolsItem::UIToolsItem(QGraphicsScene *pScene, const QIcon &icon, UIToolType enmType)
    : m_pScene(pScene)
    , m_icon(icon)
    , m_enmClass(UIToolStuff::castTypeToClass(enmType))
    , m_enmType(enmType)
    , m_enmReason(HidingReason_Null)
    , m_fHovered(false)
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

void UIToolsItem::setHiddenByReason(bool fHidden, HidingReason enmReason)
{
    if (fHidden && !(m_enmReason & enmReason))
        m_enmReason = (HidingReason)(m_enmReason | enmReason);
    else if (!fHidden && (m_enmReason & enmReason))
        m_enmReason = (HidingReason)(m_enmReason ^ enmReason);
    setVisible(m_enmReason == HidingReason_Null);
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
    /* Additional 2 margins: */
    iProposedWidth += 2 * iMargin;
#else
    /* Additional 1 margin: */
    iProposedWidth += iMargin;
#endif

    /* Add pixmap size by default: */
    iProposedWidth += m_pixmapSize.width();

    /* Take into account label size for non-Aux tools
     * 1. if text labels requested or
     * 2. machine item selected: */
    const bool fCondition1 = m_enmClass == UIToolClass_Global && model()->showItemNames();
    const bool fCondition2 = m_enmClass == UIToolClass_Machine && (   model()->showItemNames()
                                                                   || model()->currentItem(itemClass()) == this);
    if (fCondition1 || fCondition2)
    {
        iProposedWidth += m_nameSize.width();

        /* Add 1 spacing by default: */
        iProposedWidth += iSpacing;
        /* Additional 1 spacing: */
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

        /* Show tooltip for all tools
         * 0. for Aux unconditionally
         * 1. for Global if text labels hidden
         * 2. For Machine if text labels hidden and item isn't selected: */
        const bool fCondition0 = m_enmClass == UIToolClass_Aux;
        const bool fCondition1 = m_enmClass == UIToolClass_Global && !model()->showItemNames();
        const bool fCondition2 = m_enmClass == UIToolClass_Machine && !model()->showItemNames()
                                                                   && model()->currentItem(itemClass()) != this;
        if (fCondition0 || fCondition1 || fCondition2)
        {
            const QPointF posAtScene = mapToScene(rect().topRight() + QPoint(3, -3));
            const QPoint posAtScreen = model()->view()->parentWidget()->mapToGlobal(posAtScene.toPoint());
            QToolTip::showText(posAtScreen, name());
        }
    }

    update();
}

void UIToolsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *)
{
    if (m_fHovered)
    {
        m_fHovered = false;
        update();

        /* Hide tooltip for good: */
        QToolTip::hideText();
    }
}

void UIToolsItem::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget * /* pWidget = 0 */)
{
    /* Acquire rectangle: */
    const QRect rectangle = pOptions->rect;

    /* Paint background: */
    paintBackground(pPainter, rectangle);
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

    /* Configure item options: */
    setOwnedByLayout(false);
    setAcceptHoverEvents(true);
    setFocusPolicy(Qt::NoFocus);
    setFlag(QGraphicsItem::ItemIsSelectable, false);

    /* Prepare connections: */
    prepareConnections();

    /* Init: */
    updatePixmap();
    updateNameSize();
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
    /* If that item is current: */
    if (model()->currentItem(itemClass()) == this)
    {
        /* Unset the current item: */
        model()->setCurrentItem(0);
    }
}

QVariant UIToolsItem::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ToolsItemData_Margin:
        {
            const int iHint = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
            return iHint / 3 * 2;
        }
        case ToolsItemData_Spacing:
        {
            const int iHint = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
            return m_enmClass == UIToolClass_Machine ? iHint / 4 : iHint / 2;
        }
        case ToolsItemData_Padding:
        {
            return 4;
        }

        /* Font hints: */
        case Qt::FontRole:
        {
            /* Init font: */
            QFont fnt = font();
            fnt.setWeight(QFont::Bold);

            /* Make font a bit smaller: */
            fnt.setPointSize(fnt.pointSize() - 1);

            /* Return font: */
            return fnt;
        }

        /* Default: */
        default:
            break;
    }
    return QVariant();
}

void UIToolsItem::updatePixmap()
{
    /* Make icons a bit smaller: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);

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
    /* Calculate new name size: */
    const QFontMetrics fm(data(Qt::FontRole).value<QFont>(), model()->paintDevice());
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

    /* Selection background: */
    if (model()->currentItem(itemClass()) == this)
    {
        /* Acquire background color: */
        const QColor selectionColor = uiCommon().isInDarkMode()
                                    ? pal.color(QPalette::Active, QPalette::Button).lighter(150)
                                    : pal.color(QPalette::Active, QPalette::Button).darker(150);
        QColor selectionColor1 = selectionColor;
        QColor selectionColor2 = selectionColor;
        selectionColor1.setAlpha(100);
        selectionColor2.setAlpha(110);

        /* Acquire token color: */
        const QColor highlightColor = isEnabled()
                                    ? pal.color(QPalette::Active, QPalette::Highlight)
                                    : pal.color(QPalette::Disabled, QPalette::Highlight);
        const QColor highlightColor1 = uiCommon().isInDarkMode()
                                     ? highlightColor.lighter(160)
                                     : highlightColor.darker(160);
        const QColor highlightColor2 = uiCommon().isInDarkMode()
                                     ? highlightColor.lighter(140)
                                     : highlightColor.darker(140);

        /* Draw gradient background: */
        QLinearGradient bgGrad(rectangle.topLeft(), rectangle.topRight());
        bgGrad.setColorAt(0, selectionColor1);
        bgGrad.setColorAt(1, selectionColor2);
        pPainter->fillRect(rectangle, bgGrad);

        /* Depending on item class: */
        switch (itemClass())
        {
            case UIToolClass_Global:
            {
                /* Draw gradient token: */
                QRect tokenRect(rectangle.topLeft(), QSize(3, rectangle.height()));
                QLinearGradient tkGrad(tokenRect.topLeft(), tokenRect.bottomLeft());
                tkGrad.setColorAt(0, highlightColor1);
                tkGrad.setColorAt(1, highlightColor2);
                pPainter->fillRect(tokenRect, tkGrad);
                break;
            }
            case UIToolClass_Machine:
            {
                /* Draw gradient token: */
                QRect tokenRect(rectangle.bottomLeft() - QPoint(0, 2), QSize(rectangle.width(), 2));
                QLinearGradient hlGrad(tokenRect.bottomLeft(), tokenRect.bottomRight());
                hlGrad.setColorAt(0, highlightColor1);
                hlGrad.setColorAt(1, highlightColor2);
                pPainter->fillRect(tokenRect, hlGrad);
                break;
            }
            default:
                break;
        }
    }

    /* Hovering background for widget: */
    else if (m_fHovered)
    {
        /* Prepare variables: */
        const int iMargin = data(ToolsItemData_Margin).toInt();
        const int iPadding = data(ToolsItemData_Padding).toInt();

        /* Configure painter: */
        pPainter->setRenderHint(QPainter::Antialiasing, true);
        /* Acquire background color: */
        const QColor backgroundColor = model() && model()->view()
                                     ? model()->view()->palette().color(QPalette::Active, QPalette::Base)
                                     : pal.color(QPalette::Active, QPalette::Window);

        /* Prepare button sub-rect: */
        QRect subRect;
        subRect.setHeight(m_pixmap.height() / m_pixmap.devicePixelRatio() + iPadding * 2);
        subRect.setWidth(subRect.height());
#ifdef VBOX_WS_MAC
        subRect.moveTopLeft(rectangle.topLeft() + QPoint(2 * iMargin - iPadding, iMargin - iPadding));
#else
        subRect.moveTopLeft(rectangle.topLeft() + QPoint(1.5 * iMargin - iPadding, iMargin - iPadding));
#endif

        /* Prepare mouse cursor position: */
        const QPoint  cursorPosInView = model()->view()->mapFromGlobal(QCursor::pos());
        const QPointF cursorPosInScene = model()->view()->mapToScene(cursorPosInView);
        const QPointF cursorPosInItem = mapFromScene(cursorPosInScene);

        /* Paint button finally: */
        paintRoundedButton(pPainter, subRect, cursorPosInItem, backgroundColor, iPadding);
    }

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

    /* Default item foreground: */
    const QColor foreground = isEnabled()
                            ? pal.color(QPalette::Active, QPalette::Text)
                            : pal.color(QPalette::Disabled, QPalette::Text);
    pPainter->setPen(foreground);

    /* Paint left column: */
    {
        /* Prepare variables: */
#ifdef VBOX_WS_MAC
        int iPixmapX = 2 * iMargin;
#else
        int iPixmapX = 1.5 * iMargin;
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
    if (m_enmClass != UIToolClass_Aux)
    {
        /* Prepare variables: */
#ifdef VBOX_WS_MAC
        int iNameX = 2 * iMargin + m_pixmapSize.width() + 2 * iSpacing;
#else
        int iNameX = 1.5 * iMargin + m_pixmapSize.width() + 2 * iSpacing;
#endif
        const int iNameY = (iFullHeight - m_nameSize.height()) / 2;

        /* Paint name for non-Aux tools
         * 1. if text labels requested or
         * 2. machine item selected: */
        const bool fCondition1 = m_enmClass == UIToolClass_Global && model()->showItemNames();
        const bool fCondition2 = m_enmClass == UIToolClass_Machine && (   model()->showItemNames()
                                                                       || model()->currentItem(itemClass()) == this);
        if (fCondition1 || fCondition2)
            paintText(/* Painter: */
                      pPainter,
                      /* Point to paint in: */
                      QPoint(iNameX, iNameY),
                      /* Font to paint text: */
                      data(Qt::FontRole).value<QFont>(),
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
    QPainterPath textPath;
    textPath.addText(0, 0, font, strText);
    textPath.translate(point);
    pPainter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    pPainter->setPen(QPen(uiCommon().isInDarkMode() ? Qt::black : Qt::white, 2, Qt::SolidLine, Qt::RoundCap));
    pPainter->drawPath(QPainterPathStroker().createStroke(textPath));
    pPainter->setBrush(uiCommon().isInDarkMode() ? Qt::white: Qt::black);
    pPainter->setPen(Qt::NoPen);
    pPainter->drawPath(textPath);

    /* Restore painter: */
    pPainter->restore();
}

/* static */
void UIToolsItem::paintRoundedButton(QPainter *pPainter,
                                     const QRect &rectangle,
                                     const QPointF &cursorPosition,
                                     const QColor &color,
                                     int iPadding)
{
    /* Save painter: */
    pPainter->save();

    /* Paint icon frame: */
    QPainterPath painterPath;
    painterPath.addRoundedRect(rectangle, iPadding, iPadding);
    const QColor frameColor = uiCommon().isInDarkMode()
                            ? color.lighter(220)
                            : color.darker(120);
    pPainter->setPen(QPen(frameColor, 2, Qt::SolidLine, Qt::RoundCap));
    pPainter->drawPath(QPainterPathStroker().createStroke(painterPath));

    /* Fill icon body: */
    pPainter->setClipPath(painterPath);
    const QColor backgroundColor = uiCommon().isInDarkMode()
                                  ? color.lighter(180)
                                  : color.darker(105);
    pPainter->fillRect(rectangle, backgroundColor);

    /* Paint active background: */
    QRadialGradient grad(rectangle.center(), rectangle.width(), cursorPosition);
    const QColor color1 = uiCommon().isInDarkMode()
                        ? backgroundColor.lighter(180)
                        : backgroundColor.lighter(120);
    const QColor color2 = uiCommon().isInDarkMode()
                        ? backgroundColor.darker(180)
                        : backgroundColor.darker(120);
    grad.setColorAt(0, color1);
    grad.setColorAt(1, color2);
    pPainter->fillRect(rectangle, grad);

    /* Restore painter: */
    pPainter->restore();
}
