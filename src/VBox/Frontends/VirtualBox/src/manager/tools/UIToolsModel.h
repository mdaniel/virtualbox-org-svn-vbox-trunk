/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsModel class declaration.
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

#ifndef ___UIToolsModel_h___
#define ___UIToolsModel_h___

/* Qt includes: */
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QTransform>

/* GUI includes: */
#include "UIToolsItem.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declaration: */
class QGraphicsItem;
class QGraphicsScene;
class QGraphicsSceneContextMenuEvent;
class QMenu;
class QPaintDevice;
class QTimer;
class UIActionPool;
class UITools;
class UIToolsHandlerMouse;
class UIToolsHandlerKeyboard;

/** QObject extension used as VM Tools-pane model: */
class UIToolsModel : public QObject
{
    Q_OBJECT;

signals:

    /** @name Selection stuff.
      * @{ */
        /** Notifies about selection changed. */
        void sigSelectionChanged();
        /** Notifies about focus changed. */
        void sigFocusChanged();

        /** Notifies about group expanding started. */
        void sigExpandingStarted();
        /** Notifies about group expanding finished. */
        void sigExpandingFinished();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Notifies about item minimum width @a iHint changed. */
        void sigItemMinimumWidthHintChanged(int iHint);
        /** Notifies about item minimum height @a iHint changed. */
        void sigItemMinimumHeightHintChanged(int iHint);
    /** @} */

public:

    /** Constructs Tools-model passing @a pParent to the base-class. */
    UIToolsModel(UITools *pParent);
    /** Destructs Tools-model. */
    virtual ~UIToolsModel() /* override */;

    /** @name General stuff.
      * @{ */
        /** Inits model. */
        void init();
        /** Deinits model. */
        void deinit();

        /** Returns the Tools reference. */
        UITools *tools() const;
        /** Returns the action-pool reference. */
        UIActionPool *actionPool() const;
        /** Returns the scene reference. */
        QGraphicsScene *scene() const;
        /** Returns the paint device reference. */
        QPaintDevice *paintDevice() const;

        /** Returns item at @a position, taking into account possible @a deviceTransform. */
        QGraphicsItem *itemAt(const QPointF &position, const QTransform &deviceTransform = QTransform()) const;

        /** Defines current tools @a enmClass. */
        void setToolsClass(UIToolClass enmClass);
        /** Returns current tools class. */
        UIToolClass toolsClass() const;

        /** Defines current tools @a enmType. */
        void setToolsType(UIToolType enmType);
        /** Returns current tools type. */
        UIToolType toolsType() const;

        /** Defines whether certain @a enmClass of tools is @a fEnabled.*/
        void setToolsEnabled(UIToolClass enmClass, bool fEnabled);
        /** Returns whether certain class of tools is enabled.*/
        bool areToolsEnabled(UIToolClass enmClass) const;

        /** Closes parent. */
        void closeParent();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the item list. */
        QList<UIToolsItem*> items() const;
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Defines current @a pItem. */
        void setCurrentItem(UIToolsItem *pItem);
        /** Returns current item. */
        UIToolsItem *currentItem() const;

        /** Defines focus @a pItem. */
        void setFocusItem(UIToolsItem *pItem);
        /** Returns focus item. */
        UIToolsItem *focusItem() const;

        /** Makes sure some item is selected. */
        void makeSureSomeItemIsSelected();
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Returns navigation item list. */
        const QList<UIToolsItem*> &navigationList() const;
        /** Removes @a pItem from navigation list. */
        void removeFromNavigationList(UIToolsItem *pItem);
        /** Updates navigation list. */
        void updateNavigation();
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates layout. */
        void updateLayout();
    /** @} */

public slots:

    /** @name General stuff.
      * @{ */
        /** Handles Tools-view resize. */
        void sltHandleViewResized();
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Handles minimum width hint change. */
        void sltItemMinimumWidthHintChanged();
        /** Handles minimum height hint change. */
        void sltItemMinimumHeightHintChanged();
    /** @} */

protected:

    /** @name Event handling stuff.
      * @{ */
        /** Preprocesses Qt @a pEvent for passed @a pObject. */
        virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;
    /** @} */

private slots:

    /** @name Selection stuff.
      * @{ */
        /** Handles focus item destruction. */
        void sltFocusItemDestroyed();
    /** @} */

private:

    /** Data field types. */
    enum ToolsModelData
    {
        /* Layout hints: */
        ToolsModelData_Margin,
        ToolsModelData_Spacing,
    };

    /** @name Prepare/Cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares scene. */
        void prepareScene();
        /** Prepares items. */
        void prepareItems();
        /** Prepares handlers. */
        void prepareHandlers();
        /** Prepares connections. */
        void prepareConnections();
        /** Loads last selected items. */
        void loadLastSelectedItems();

        /** Saves last selected items. */
        void saveLastSelectedItems();
        /** Cleanups connections. */
        void cleanupHandlers();
        /** Cleanups items. */
        void cleanupItems();
        /** Cleanups scene. */
        void cleanupScene();
        /** Cleanups all. */
        void cleanup();
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Returns abstractly stored data value for certain @a iKey. */
        QVariant data(int iKey) const;

        /** Returns @a enmType string representation.
          * @todo move to UIConverter one day .. */
        static QString typeToString(UIToolType enmType);
        /** Returns @a strType converted to corresponding UIToolType.
          * @todo move to UIConverter one day .. */
        static UIToolType typeFromString(const QString &strType);

        /** Returns whether passed @a enmType is of passed @a enmClass. */
        static bool isTypeOfClass(UIToolType enmType, UIToolClass enmClass);
    /** @} */

    /** @name General stuff.
      * @{ */
        /** Holds the Tools reference. */
        UITools *m_pTools;

        /** Holds the scene reference. */
        QGraphicsScene *m_pScene;

        /** Holds the mouse handler instance. */
        UIToolsHandlerMouse    *m_pMouseHandler;
        /** Holds the keyboard handler instance. */
        UIToolsHandlerKeyboard *m_pKeyboardHandler;

        /** Holds current tools class. */
        UIToolClass  m_enmCurrentClass;

        /** Holds whether tools of particular class are enabled. */
        QMap<UIToolClass, bool>  m_statesToolsEnabled;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Holds the root stack. */
        QList<UIToolsItem*>  m_items;
    /** @} */

    /** @name Selection stuff.
      * @{ */
        /** Holds the selected item reference. */
        QPointer<UIToolsItem> m_pCurrentItem;
        /** Holds the focus item reference. */
        QPointer<UIToolsItem> m_pFocusItem;
    /** @} */

    /** @name Navigation stuff.
      * @{ */
        /** Holds the navigation list. */
        QList<UIToolsItem*>  m_navigationList;

        /** Holds the last chosen navigation item of global class. */
        QPointer<UIToolsItem> m_pLastItemGlobal;
        /** Holds the last chosen navigation item of machine class. */
        QPointer<UIToolsItem> m_pLastItemMachine;
    /** @} */
};

#endif /* !___UIToolsModel_h___ */
