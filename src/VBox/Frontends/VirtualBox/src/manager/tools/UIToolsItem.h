/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsItem class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_tools_UIToolsItem_h
#define FEQT_INCLUDED_SRC_manager_tools_UIToolsItem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QPixmap>
#include <QString>

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "UITools.h"

/* Forward declaration: */
class QGraphicsScene;
class QGraphicsSceneHoverEvent;
class UIToolsModel;

/** QIGraphicsWidget extension used as interface
  * for graphics Tools-model/view architecture. */
class UIToolsItem : public QIGraphicsWidget
{
    Q_OBJECT;

signals:

    /** @name Layout stuff.
      * @{ */
        /** Notifies listeners about minimum width @a iHint change. */
        void sigMinimumWidthHintChanged(int iHint);
        /** Notifies listeners about minimum height @a iHint change. */
        void sigMinimumHeightHintChanged(int iHint);
    /** @} */

public:

    /** Hiding reasons. */
    enum HidingReason
    {
        HidingReason_Null       = 0,
        HidingReason_Restricted = RT_BIT(0),
    };

    /** Constructs item on the basis of passed arguments.
      * @param  pScene        Brings the scene reference to add item to.
      * @param  icon          Brings the item icon.
      * @param  enmType       Brings the item type. */
    UIToolsItem(QGraphicsScene *pScene, const QIcon &icon, UIToolType enmType);
    /** Destructs item. */
    virtual ~UIToolsItem() RT_OVERRIDE;

    /** @name Item stuff.
      * @{ */
        /** Returns model reference. */
        UIToolsModel *model() const;

        /** Returns item icon. */
        QIcon icon() const { return m_icon; }

        /** Returns item name. */
        QString name() const { return m_strName; }
        /** Defines item @a strName. */
        void setName(const QString &strName);

        /** Returns item class. */
        UIToolClass itemClass() const { return m_enmClass; }
        /** Returns item type. */
        UIToolType itemType() const { return m_enmType; }

        /** Defines whether item is @a fEnabled. */
        void setEnabled(bool fEnabled);

        /** Defines whether item is @a fHidden by the @a enmReason. */
        void setHiddenByReason(bool fHidden, HidingReason enmReason);
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates geometry. */
        virtual void updateGeometry() RT_OVERRIDE;

        /** Returns minimum width-hint. */
        int minimumWidthHint() const;
        /** Returns minimum height-hint. */
        int minimumHeightHint() const;

        /** Returns size-hint.
          * @param  enmWhich    Brings size-hint type.
          * @param  constraint  Brings size constraint. */
        virtual QSizeF sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint = QSizeF()) const RT_OVERRIDE;
    /** @} */

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) RT_OVERRIDE;

        /** Handles hover enter @a event. */
        virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *pEvent) RT_OVERRIDE;
        /** Handles hover leave @a event. */
        virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *pEvent) RT_OVERRIDE;

        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) RT_OVERRIDE;
    /** @} */

private slots:

    /** @name Item stuff.
      * @{ */
        /** Handles top-level window remaps. */
        void sltHandleWindowRemapped();
    /** @} */

private:

    /** Data field types. */
    enum ToolsItemData
    {
        /* Layout hints: */
        ToolsItemData_Margin,
        ToolsItemData_Spacing,
        ToolsItemData_Padding,
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares connections. */
        void prepareConnections();

        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates pixmap. */
        void updatePixmap();
        /** Updates name size. */
        void updateNameSize();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter.
          * @param  rectangle  Brings the rectangle to fill with background. */
        void paintBackground(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints tool info using using passed @a pPainter.
          * @param  rectangle  Brings the rectangle to limit painting with. */
        void paintToolInfo(QPainter *pPainter, const QRect &rectangle) const;

        /** Paints @a pixmap using passed @a pPainter.
          * @param  pOptions  Brings the options set with painting data. */
        static void paintPixmap(QPainter *pPainter, const QPoint &point, const QPixmap &pixmap);

        /** Paints @a strText using passed @a pPainter.
          * @param  point         Brings upper-left corner pixmap should be mapped to.
          * @param  font          Brings the text font.
          * @param  pPaintDevice  Brings the paint-device reference to initilize painting from. */
        static void paintText(QPainter *pPainter, QPoint point,
                              const QFont &font, QPaintDevice *pPaintDevice,
                              const QString &strText);
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds the item parent. */
        QGraphicsScene *m_pScene;
        /** Holds the item icon. */
        QIcon           m_icon;
        /** Holds the item name. */
        QString         m_strName;
        /** Holds the item class. */
        UIToolClass     m_enmClass;
        /** Holds the item type. */
        UIToolType      m_enmType;

        /** Holds the item pixmap. */
        QPixmap  m_pixmap;

        /** Holds the hiding reason. */
        HidingReason  m_enmReason;

        /** Holds whether item is hovered. */
        bool  m_fHovered;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds previous minimum width hint. */
        int  m_iPreviousMinimumWidthHint;
        /** Holds previous minimum height hint. */
        int  m_iPreviousMinimumHeightHint;

        /** Holds the pixmap size. */
        QSize  m_pixmapSize;
        /** Holds the name size. */
        QSize  m_nameSize;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_tools_UIToolsItem_h */
