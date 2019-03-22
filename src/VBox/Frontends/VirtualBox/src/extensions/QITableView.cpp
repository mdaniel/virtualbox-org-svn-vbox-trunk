/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QITableView class implementation.
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
# include <QAccessibleWidget>

/* GUI includes: */
# include "QIStyledItemDelegate.h"
# include "QITableView.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleObject extension used as an accessibility interface for QITableViewCell. */
class QIAccessibilityInterfaceForQITableViewCell : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableViewCell accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableViewCell"))
            return new QIAccessibilityInterfaceForQITableViewCell(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITableViewCell(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const /* override */;

    /** Returns the number of children. */
    virtual int childCount() const /* override */ { return 0; }
    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int /* iIndex */) const /* override */ { return 0; }
    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface * /* pChild */) const /* override */ { return -1; }

    /** Returns the rect. */
    virtual QRect rect() const /* override */;
    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */;

    /** Returns the role. */
    virtual QAccessible::Role role() const /* override */;
    /** Returns the state. */
    virtual QAccessible::State state() const /* override */;

private:

    /** Returns corresponding QITableViewCell. */
    QITableViewCell *cell() const { return qobject_cast<QITableViewCell*>(object()); }
};


/** QAccessibleObject extension used as an accessibility interface for QITableViewRow. */
class QIAccessibilityInterfaceForQITableViewRow : public QAccessibleObject
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableViewRow accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableViewRow"))
            return new QIAccessibilityInterfaceForQITableViewRow(pObject);

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pObject to the base-class. */
    QIAccessibilityInterfaceForQITableViewRow(QObject *pObject)
        : QAccessibleObject(pObject)
    {}

    /** Returns the parent. */
    virtual QAccessibleInterface *parent() const /* override */;

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const /* override */;
    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const /* override */;

    /** Returns the rect. */
    virtual QRect rect() const /* override */;
    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */;

    /** Returns the role. */
    virtual QAccessible::Role role() const /* override */;
    /** Returns the state. */
    virtual QAccessible::State state() const /* override */;

private:

    /** Returns corresponding QITableViewRow. */
    QITableViewRow *row() const { return qobject_cast<QITableViewRow*>(object()); }
};


/** QAccessibleWidget extension used as an accessibility interface for QITableView. */
class QIAccessibilityInterfaceForQITableView : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QITableView accessibility interface: */
        if (pObject && strClassname == QLatin1String("QITableView"))
            return new QIAccessibilityInterfaceForQITableView(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQITableView(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::List)
    {}

    /** Returns the number of children. */
    virtual int childCount() const /* override */;
    /** Returns the child with the passed @a iIndex. */
    virtual QAccessibleInterface *child(int iIndex) const /* override */;
    /** Returns the index of the passed @a pChild. */
    virtual int indexOfChild(const QAccessibleInterface *pChild) const /* override */;

    /** Returns a text for the passed @a enmTextRole. */
    virtual QString text(QAccessible::Text enmTextRole) const /* override */;

private:

    /** Returns corresponding QITableView. */
    QITableView *table() const { return qobject_cast<QITableView*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QIAccessibilityInterfaceForQITableViewCell implementation.                                                             *
*********************************************************************************************************************************/

QAccessibleInterface *QIAccessibilityInterfaceForQITableViewCell::parent() const
{
    /* Make sure cell still alive: */
    AssertPtrReturn(cell(), 0);

    /* Return the parent: */
    return QAccessible::queryAccessibleInterface(cell()->row());
}

QRect QIAccessibilityInterfaceForQITableViewCell::rect() const
{
    /* Make sure cell still alive: */
    AssertPtrReturn(cell(), QRect());
    AssertPtrReturn(cell()->row(), QRect());
    AssertPtrReturn(cell()->row()->table(), QRect());

    /* Calculate local item coordinates: */
    const int iIndexInParent = parent()->indexOfChild(this);
    const int iParentIndexInParent = parent()->parent()->indexOfChild(parent());
    const int iX = cell()->row()->table()->columnViewportPosition(iIndexInParent);
    const int iY = cell()->row()->table()->rowViewportPosition(iParentIndexInParent);
    const int iWidth = cell()->row()->table()->columnWidth(iIndexInParent);
    const int iHeight = cell()->row()->table()->rowHeight(iParentIndexInParent);

    /* Map local item coordinates to global: */
    const QPoint itemPosInScreen = cell()->row()->table()->viewport()->mapToGlobal(QPoint(iX, iY));

    /* Return item rectangle: */
    return QRect(itemPosInScreen, QSize(iWidth, iHeight));
}

QString QIAccessibilityInterfaceForQITableViewCell::text(QAccessible::Text enmTextRole) const
{
    /* Make sure cell still alive: */
    AssertPtrReturn(cell(), QString());

    /* Return a text for the passed enmTextRole: */
    switch (enmTextRole)
    {
        case QAccessible::Name: return cell()->text();
        default: break;
    }

    /* Null-string by default: */
    return QString();
}

QAccessible::Role QIAccessibilityInterfaceForQITableViewCell::role() const
{
    /* Cell by default: */
    return QAccessible::Cell;
}

QAccessible::State QIAccessibilityInterfaceForQITableViewCell::state() const
{
    /* Make sure cell still alive: */
    AssertPtrReturn(cell(), QAccessible::State());

    /* Empty state by default: */
    return QAccessible::State();
}


/*********************************************************************************************************************************
*   Class QIAccessibilityInterfaceForQITableViewRow implementation.                                                              *
*********************************************************************************************************************************/

QAccessibleInterface *QIAccessibilityInterfaceForQITableViewRow::parent() const
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), 0);

    /* Return the parent: */
    return QAccessible::queryAccessibleInterface(row()->table());
}

int QIAccessibilityInterfaceForQITableViewRow::childCount() const
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), 0);

    /* Return the number of children: */
    return row()->childCount();
}

QAccessibleInterface *QIAccessibilityInterfaceForQITableViewRow::child(int iIndex) const /* override */
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), 0);
    /* Make sure index is valid: */
    AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

    /* Return the child with the passed iIndex: */
    return QAccessible::queryAccessibleInterface(row()->childItem(iIndex));
}

int QIAccessibilityInterfaceForQITableViewRow::indexOfChild(const QAccessibleInterface *pChild) const /* override */
{
    /* Search for corresponding child: */
    for (int i = 0; i < childCount(); ++i)
        if (child(i) == pChild)
            return i;

    /* -1 by default: */
    return -1;
}

QRect QIAccessibilityInterfaceForQITableViewRow::rect() const
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), QRect());
    AssertPtrReturn(row()->table(), QRect());

    /* Calculate local item coordinates: */
    const int iIndexInParent = parent()->indexOfChild(this);
    const int iX = row()->table()->columnViewportPosition(0);
    const int iY = row()->table()->rowViewportPosition(iIndexInParent);
    int iWidth = 0;
    int iHeight = 0;
    for (int i = 0; i < childCount(); ++i)
        iWidth += row()->table()->columnWidth(i);
    iHeight += row()->table()->rowHeight(iIndexInParent);

    /* Map local item coordinates to global: */
    const QPoint itemPosInScreen = row()->table()->viewport()->mapToGlobal(QPoint(iX, iY));

    /* Return item rectangle: */
    return QRect(itemPosInScreen, QSize(iWidth, iHeight));
}

QString QIAccessibilityInterfaceForQITableViewRow::text(QAccessible::Text enmTextRole) const
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), QString());

    /* Return a text for the passed enmTextRole: */
    switch (enmTextRole)
    {
        case QAccessible::Name: return childCount() > 0 && child(0) ? child(0)->text(enmTextRole) : QString();
        default: break;
    }

    /* Null-string by default: */
    return QString();
}

QAccessible::Role QIAccessibilityInterfaceForQITableViewRow::role() const
{
    /* Row by default: */
    return QAccessible::Row;
}

QAccessible::State QIAccessibilityInterfaceForQITableViewRow::state() const
{
    /* Make sure row still alive: */
    AssertPtrReturn(row(), QAccessible::State());

    /* Empty state by default: */
    return QAccessible::State();
}


/*********************************************************************************************************************************
*   Class QIAccessibilityInterfaceForQITableView implementation.                                                                 *
*********************************************************************************************************************************/

int QIAccessibilityInterfaceForQITableView::childCount() const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), 0);

    /* Return the number of children: */
    return table()->childCount();
}

QAccessibleInterface *QIAccessibilityInterfaceForQITableView::child(int iIndex) const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), 0);
    /* Make sure index is valid: */
    AssertReturn(iIndex >= 0, 0);
    if (iIndex >= childCount())
    {
        // WORKAROUND:
        // Normally I would assert here, but Qt5 accessibility code has
        // a hard-coded architecture for a table-views which we do not like
        // but have to live with and this architecture enumerates cells
        // including header column and row, so Qt5 can try to address
        // our interface with index which surely out of bounds by our laws.
        // So let's assume that's exactly such case and try to enumerate
        // table cells including header column and row.
        // printf("Invalid index: %d\n", iIndex);

        // Split delimeter is overall column count, including vertical header:
        const int iColumnCount = table()->model()->columnCount() + 1 /* v_header */;
        // Real index is zero-based, incoming is 1-based:
        const int iRealIndex = iIndex - 1;
        // Real row index, excluding horizontal header:
        const int iRealRowIndex = iRealIndex / iColumnCount - 1 /* h_header */;
        // printf("Actual row index: %d\n", iRealRowIndex);

        // Return what we found:
        return iRealRowIndex >= 0 && iRealRowIndex < childCount() ?
               QAccessible::queryAccessibleInterface(table()->childItem(iRealRowIndex)) : 0;
    }

    /* Return the child with the passed iIndex: */
    return QAccessible::queryAccessibleInterface(table()->childItem(iIndex));
}

int QIAccessibilityInterfaceForQITableView::indexOfChild(const QAccessibleInterface *pChild) const
{
    /* Search for corresponding child: */
    for (int i = 0; i < childCount(); ++i)
        if (child(i) == pChild)
            return i;

    /* -1 by default: */
    return -1;
}

QString QIAccessibilityInterfaceForQITableView::text(QAccessible::Text /* enmTextRole */) const
{
    /* Make sure table still alive: */
    AssertPtrReturn(table(), QString());

    /* Return table whats-this: */
    return table()->whatsThis();
}


/*********************************************************************************************************************************
*   Class QITableView implementation.                                                                                            *
*********************************************************************************************************************************/

QITableView::QITableView(QWidget *pParent)
    : QTableView(pParent)
{
    /* Prepare: */
    prepare();
}

QITableView::~QITableView()
{
    /* Cleanup: */
    cleanup();
}

void QITableView::makeSureEditorDataCommitted()
{
    /* Do we have current editor at all? */
    QObject *pEditorObject = m_editors.value(currentIndex());
    if (pEditorObject && pEditorObject->isWidgetType())
    {
        /* Cast the editor to widget type: */
        QWidget *pEditor = qobject_cast<QWidget*>(pEditorObject);
        AssertPtrReturnVoid(pEditor);
        {
            /* Commit the editor data and closes it: */
            commitData(pEditor);
            closeEditor(pEditor, QAbstractItemDelegate::SubmitModelCache);
        }
    }
}

void QITableView::sltEditorCreated(QWidget *pEditor, const QModelIndex &index)
{
    /* Connect created editor to the table and store it: */
    connect(pEditor, &QWidget::destroyed, this, &QITableView::sltEditorDestroyed);
    m_editors[index] = pEditor;
}

void QITableView::sltEditorDestroyed(QObject *pEditor)
{
    /* Clear destroyed editor from the table: */
    const QModelIndex index = m_editors.key(pEditor);
    AssertReturnVoid(index.isValid());
    m_editors.remove(index);
}

void QITableView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    /* Notify listeners about index changed: */
    emit sigCurrentChanged(current, previous);
    /* Call to base-class: */
    QTableView::currentChanged(current, previous);
}

void QITableView::prepare()
{
    /* Install QITableViewCell accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableViewCell::pFactory);
    /* Install QITableViewRow accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableViewRow::pFactory);
    /* Install QITableView accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQITableView::pFactory);

    /* Delete old delegate: */
    delete itemDelegate();
    /* Create new delegate: */
    QIStyledItemDelegate *pStyledItemDelegate = new QIStyledItemDelegate(this);
    AssertPtrReturnVoid(pStyledItemDelegate);
    {
        /* Assign newly created delegate to the table: */
        setItemDelegate(pStyledItemDelegate);
        /* Connect newly created delegate to the table: */
        connect(pStyledItemDelegate, &QIStyledItemDelegate::sigEditorCreated,
                this, &QITableView::sltEditorCreated);
    }
}

void QITableView::cleanup()
{
    /* Disconnect all the editors prematurelly: */
    foreach (QObject *pEditor, m_editors.values())
        disconnect(pEditor, 0, this, 0);
}
