/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsItem class declaration.
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

#ifndef FEQT_INCLUDED_SRC_manager_details_UIDetailsItem_h
#define FEQT_INCLUDED_SRC_manager_details_UIDetailsItem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes */
#include <QUuid>

/* GUI includes: */
#include "QIGraphicsWidget.h"
#include "QIWithRetranslateUI.h"

/* Forward declaration: */
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;
class UIDetailsElement;
class UIDetailsGroup;
class UIDetailsModel;
class UIDetailsSet;


/** UIDetailsItem types. */
enum UIDetailsItemType
{
    UIDetailsItemType_Any     = QGraphicsItem::UserType,
    UIDetailsItemType_Group,
    UIDetailsItemType_Set,
    UIDetailsItemType_Element,
    UIDetailsItemType_Preview
};


/** QIGraphicsWidget extension used as interface
  * for graphics details model/view architecture. */
class UIDetailsItem : public QIWithRetranslateUI4<QIGraphicsWidget>
{
    Q_OBJECT;

signals:

    /** @name Item stuff.
      * @{ */
        /** Notifies listeners about step build should be started.
          * @param  uStepId    Brings the step ID.
          * @param  iStepNumber  Brings the step number. */
        void sigBuildStep(const QUuid &uStepId, int iStepNumber);
        /** Notifies listeners about step build complete. */
        void sigBuildDone();
    /** @} */

public:

    /** Constructs item passing @a pParent to the base-class. */
    UIDetailsItem(UIDetailsItem *pParent);

    /** @name Item stuff.
      * @{ */
        /** Returns parent  reference. */
        UIDetailsItem *parentItem() const { return m_pParent; }

        /** Casts item to group one. */
        UIDetailsGroup *toGroup();
        /** Casts item to set one. */
        UIDetailsSet *toSet();
        /** Casts item to element one. */
        UIDetailsElement *toElement();

        /** Returns model reference. */
        UIDetailsModel *model() const;

        /** Returns the description of the item. */
        virtual QString description() const = 0;
    /** @} */

    /** @name Children stuff.
      * @{ */
        /** Adds child @a pItem. */
        virtual void addItem(UIDetailsItem *pItem) = 0;
        /** Removes child @a pItem. */
        virtual void removeItem(UIDetailsItem *pItem) = 0;

        /** Returns children items of certain @a enmType. */
        virtual QList<UIDetailsItem*> items(UIDetailsItemType enmType = UIDetailsItemType_Any) const = 0;
        /** Returns whether there are children items of certain @a enmType. */
        virtual bool hasItems(UIDetailsItemType enmType = UIDetailsItemType_Any) const = 0;
        /** Clears children items of certain @a enmType. */
        virtual void clearItems(UIDetailsItemType enmType = UIDetailsItemType_Any) = 0;
    /** @} */

    /** @name Layout stuff.
      * @{ */
        /** Updates geometry. */
        virtual void updateGeometry() /* override */;

        /** Updates layout. */
        virtual void updateLayout() = 0;

        /** Returns minimum width-hint. */
        virtual int minimumWidthHint() const = 0;
        /** Returns minimum height-hint. */
        virtual int minimumHeightHint() const = 0;

        /** Returns size-hint.
          * @param  enmWhich    Brings size-hint type.
          * @param  constraint  Brings size constraint. */
        virtual QSizeF sizeHint(Qt::SizeHint enmWhich, const QSizeF &constraint = QSizeF()) const /* override */;
    /** @} */

protected slots:

    /** @name Item stuff.
      * @{ */
        /** Handles request about starting step build.
          * @param  uStepId    Brings the step ID.
          * @param  iStepNumber  Brings the step number. */
    /** @} */
    virtual void sltBuildStep(const QUuid &uStepId, int iStepNumber);

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */ {}
    /** @} */

private:

    /** Holds the parent item reference. */
    UIDetailsItem *m_pParent;
};


/** QObject extension used to prepare details steps. */
class UIPrepareStep : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies listeners about step preparing is complete.
      * @param  strStepId    Brings the step ID.
      * @param  iStepNumber  Brings the step number. */
    void sigStepDone(const QUuid& aStepId, int iStepNumber);

public:

    /** Constructs step preparing object passing @a pParent to the base-class.
      * @param  pBuildObject  Brings the build object reference.
      * @param  uStepId       Brings the step ID.
      * @param  iStepNumber   Brings the step number.*/
    UIPrepareStep(QObject *pParent, QObject *pBuildObject, const QUuid &uStepId, int iStepNumber);

private slots:

    /** Handles step prepare completion. */
    void sltStepDone();

private:

    /** Holds the step ID. */
    QUuid  m_uStepId;
    /** Holds the step number. */
    int    m_iStepNumber;
};


#endif /* !FEQT_INCLUDED_SRC_manager_details_UIDetailsItem_h */
