/* $Id$ */
/** @file
 * VBox Qt GUI - UIChooserItemGlobal class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemGlobal_h
#define FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemGlobal_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UIChooserItem.h"

/* Forward declarations: */
class QMimeData;
class QPainter;
class QStyleOptionGraphicsItem;
class UIGraphicsToolBar;
class UIGraphicsZoomButton;

/** UIChooserItem extension implementing global item. */
class UIChooserItemGlobal : public UIChooserItem
{
    Q_OBJECT;

public:

    /** RTTI item type. */
    enum { Type = UIChooserItemType_Global };

    /** Constructs possible @a fFavorite item with specified @a iPosition, passing @a pParent to the base-class. */
    UIChooserItemGlobal(UIChooserItem *pParent, bool fFavorite, int iPosition = -1);
    /** Constructs possible @a fFavorite temporary item with specified @a iPosition as a @a pCopyFrom, passing @a pParent to the base-class. */
    UIChooserItemGlobal(UIChooserItem *pParent, bool fFavorite, UIChooserItemGlobal *pCopyFrom, int iPosition = -1);
    /** Destructs global item. */
    virtual ~UIChooserItemGlobal() /* override */;

    /** @name Item stuff.
      * @{ */
        /** Defines whether item is @a fFavorite. */
        virtual void setFavorite(bool fFavorite) /* override */;

        /** Returns whether passed @a position belongs to tool button area. */
        bool isToolButtonArea(const QPoint &position, int iMarginMultiplier = 1) const;
        /** Returns whether passed @a position belongs to pin button area. */
        bool isPinButtonArea(const QPoint &position, int iMarginMultiplier = 1) const;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Defines height @a iHint. */
        void setHeightHint(int iHint);
        /** Returns height hint. */
        int heightHint() const;
    /** @} */

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;

        /** Handles show @a pEvent. */
        virtual void showEvent(QShowEvent *pEvent) /* override */;

        /** Handles resize @a pEvent. */
        virtual void resizeEvent(QGraphicsSceneResizeEvent *pEvent) /* override */;

        /** Handles mouse press @a pEvent. */
        virtual void mousePressEvent(QGraphicsSceneMouseEvent *pEvent) /* override */;

        /** Performs painting using passed @a pPainter, @a pOptions and optionally specified @a pWidget. */
        virtual void paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *pWidget = 0) /* override */;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Returns RTTI item type. */
        virtual int type() const /* override */ { return Type; }

        /** Starts item editing. */
        virtual void startEditing() /* override */;

        /** Updates item tool-tip. */
        virtual void updateToolTip() /* override */;

        /** Returns item name. */
        virtual QString name() const /* override */;
        /** Returns item description. */
        virtual QString description() const /* override */;
        /** Returns item full-name. */
        virtual QString fullName() const /* override */;
        /** Returns item definition. */
        virtual QString definition() const /* override */;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds possible @a fFavorite child @a pItem to certain @a iPosition. */
        virtual void addItem(UIChooserItem *pItem, bool fFavorite, int iPosition) /* override */;
        /** Removes child @a pItem. */
        virtual void removeItem(UIChooserItem *pItem) /* override */;

        /** Replaces children @a items of certain @a enmType. */
        virtual void setItems(const QList<UIChooserItem*> &items, UIChooserItemType enmType) /* override */;
        /** Returns children items of certain @a enmType. */
        virtual QList<UIChooserItem*> items(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;
        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIChooserItemType enmType = UIChooserItemType_Any) const /* override */;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIChooserItemType enmType = UIChooserItemType_Any) /* override */;

        /** Updates all children items with specified @a uId. */
        virtual void updateAllItems(const QUuid &uId) /* override */;
        /** Removes all children items with specified @a uId. */
        virtual void removeAllItems(const QUuid &uId) /* override */;

        /** Searches for a first child item answering to specified @a strSearchTag and @a iItemSearchFlags. */
        virtual UIChooserItem *searchForItem(const QString &strSearchTag, int iItemSearchFlags) /* override */;

        /** Searches for a first machine child item. */
        virtual UIChooserItem *firstMachineItem() /* override */;

        /** Sorts children items. */
        virtual void sortItems() /* override */;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        virtual void updateLayout() /* override */;

        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const /* override */;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const /* override */;

        /** Returns size-hint.
          * @param  enmWhich    Brings size-hint type.
          * @param  constraint  Brings size constraint. */
        virtual QSizeF sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint = QSizeF()) const /* override */;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns pixmap item representation. */
        virtual QPixmap toPixmap() /* override */;

        /** Returns whether item drop is allowed.
          * @param  pEvent    Brings information about drop event.
          * @param  enmPlace  Brings the place of drag token to the drop moment. */
        virtual bool isDropAllowed(QGraphicsSceneDragDropEvent *pEvent, DragToken where) const /* override */;
        /** Processes item drop.
          * @param  pEvent    Brings information about drop event.
          * @param  pFromWho  Brings the item according to which we choose drop position.
          * @param  enmPlace  Brings the place of drag token to the drop moment (according to item mentioned above). */
        virtual void processDrop(QGraphicsSceneDragDropEvent *pEvent, UIChooserItem *pFromWho, DragToken where) /* override */;
        /** Reset drag token. */
        virtual void resetDragToken() /* override */;

        /** Returns D&D mime data. */
        virtual QMimeData *createMimeData() /* override */;
    /** @} */

private slots:

    /** @name Item stuff.
      * @{ */
        /** Handles top-level window remaps. */
        void sltHandleWindowRemapped();
    /** @} */

private:

    /** Data field types. */
    enum GlobalItemData
    {
        /* Layout hints: */
        GlobalItemData_Margin,
        GlobalItemData_Spacing,
        GlobalItemData_ButtonMargin,
    };

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
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
        /** Updates pixmaps. */
        void updatePixmaps();
        /** Updates pixmap. */
        void updatePixmap();
        /** Updates tool pixmap. */
        void updateToolPixmap();
        /** Updates pin pixmap. */
        void updatePinPixmap();
        /** Updates minimum name width. */
        void updateMinimumNameWidth();
        /** Updates maximum name width. */
        void updateMaximumNameWidth();
        /** Updates visible name. */
        void updateVisibleName();
    /** @} */

    /** @name Painting stuff.
      * @{ */
        /** Paints background using specified @a pPainter and certain @a rectangle. */
        void paintBackground(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints frame using specified @a pPainter and certain @a rect. */
        void paintFrame(QPainter *pPainter, const QRect &rectangle) const;
        /** Paints global info using specified @a pPainter and certain @a pOptions. */
        void paintGlobalInfo(QPainter *pPainter, const QRect &rectangle) const;
    /** @} */

    /** @name Item stuff.
      * @{ */
        /** Holds initial item position. */
        const int  m_iPosition;

        /** Holds item minimum default lightness. */
        int  m_iDefaultLightnessMin;
        /** Holds item maximum default lightness. */
        int  m_iDefaultLightnessMax;
        /** Holds item minimum hover lightness. */
        int  m_iHoverLightnessMin;
        /** Holds item maximum hover lightness. */
        int  m_iHoverLightnessMax;
        /** Holds item minimum highlight lightness. */
        int  m_iHighlightLightnessMin;
        /** Holds item maximum highlight lightness. */
        int  m_iHighlightLightnessMax;

        /** Holds item pixmap. */
        QPixmap  m_pixmap;
        /** Holds item tool pixmap. */
        QPixmap  m_toolPixmap;
        /** Holds item pin pixmap. */
        QPixmap  m_pinPixmap;

        /** Holds item name. */
        QString  m_strName;
        /** Holds item description. */
        QString  m_strDescription;
        /** Holds item visible name. */
        QString  m_strVisibleName;

        /** Holds item name font. */
        QFont  m_nameFont;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Holds pixmap size. */
        QSize  m_pixmapSize;
        /** Holds tool pixmap size. */
        QSize  m_toolPixmapSize;
        /** Holds pin pixmap size. */
        QSize  m_pinPixmapSize;
        /** Holds visible name size. */
        QSize  m_visibleNameSize;

        /** Holds minimum name width. */
        int  m_iMinimumNameWidth;
        /** Holds maximum name width. */
        int  m_iMaximumNameWidth;

        /** Holds the height hint. */
        int  m_iHeightHint;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_manager_chooser_UIChooserItemGlobal_h */
