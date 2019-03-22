/* $Id$ */
/** @file
 * VBox Qt GUI - UITabBar class declaration.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UITabBar_h___
#define ___UITabBar_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QIcon>
#include <QString>
#include <QUuid>
#include <QWidget>

/* Forward declarations: */
class QAction;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QHBoxLayout;
class QIcon;
class QPaintEvent;
class QString;
class QUuid;
class QWidget;
class UITabBarItem;


/** Our own skinnable implementation of tab-bar.
  * The idea is to make tab-bar analog which looks more interesting
  * on various platforms, allows for various skins, and tiny adjustments. */
class UITabBar : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies about tab with @a uuid requested closing. */
    void sigTabRequestForClosing(const QUuid &uuid);
    /** Notifies about tab with @a uuid set to current. */
    void sigCurrentTabChanged(const QUuid &uuid);

public:

    /** Alignment types. */
    enum Alignment { Align_Left, Align_Right };

    /** Constructs tab-bar passing @a pParent to the base-class. */
    UITabBar(Alignment enmAlignment, QWidget *pParent = 0);

    /** Adds new tab for passed @a pAction. @returns unique tab ID. */
    QUuid addTab(const QAction *pAction);

    /** Removes tab with passed @a uuid. */
    bool removeTab(const QUuid &uuid);

    /** Makes tab with passed @a uuid current. */
    bool setCurrent(const QUuid &uuid);

    /** Return tab-bar order ID list. */
    QList<QUuid> tabOrder() const;

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

    /** Handles drag-enter @a pEvent. */
    virtual void dragEnterEvent(QDragEnterEvent *pEvent) /* override */;
    /** Handles drag-move @a pEvent. */
    virtual void dragMoveEvent(QDragMoveEvent *pEvent) /* override */;
    /** Handles drag-leave @a pEvent. */
    virtual void dragLeaveEvent(QDragLeaveEvent *pEvent) /* override */;
    /** Handles drop @a pEvent. */
    virtual void dropEvent(QDropEvent *pEvent) /* override */;

private slots:

    /** Handles request to make @a pItem current. */
    void sltHandleMakeChildCurrent(UITabBarItem *pItem);

    /** Handles request to close @a pItem. */
    void sltHandleChildClose(UITabBarItem *pItem);

    /** Handles drag object destruction. */
    void sltHandleDragObjectDestroy();

private:

    /** Prepares all. */
    void prepare();

    /** Updates children styles. */
    void updateChildrenStyles();

    /** @name Contents: Common
      * @{ */
        /** Holds the alignment. */
        Alignment m_enmAlignment;
    /** @} */

    /** @name Contents: Widgets
      * @{ */
        /** Holds the main layout instance. */
        QHBoxLayout *m_pLayoutMain;
        /** Holds the tab layout instance. */
        QHBoxLayout *m_pLayoutTab;

        /** Holds the current item reference. */
        UITabBarItem *m_pCurrentItem;

        /** Holds the array of items instances. */
        QList<UITabBarItem*> m_aItems;
    /** @} */

    /** @name Contents: Order
      * @{ */
        /** Holds the token-item to drop dragged-item nearby. */
        UITabBarItem *m_pItemToken;

        /** Holds whether the dragged-item should be dropped <b>after</b> the token-item. */
        bool  m_fDropAfterTokenItem;
    /** @} */
};

#endif /* !___UITabBar_h___ */

