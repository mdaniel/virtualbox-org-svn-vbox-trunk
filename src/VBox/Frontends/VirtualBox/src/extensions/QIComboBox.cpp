/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: QIComboBox class implementation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
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
# include <QHBoxLayout>
# include <QLineEdit>

/* GUI includes: */
# include "QIComboBox.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QAccessibleWidget extension used as an accessibility interface for QIComboBox. */
class QIAccessibilityInterfaceForQIComboBox : public QAccessibleWidget
{
public:

    /** Returns an accessibility interface for passed @a strClassname and @a pObject. */
    static QAccessibleInterface *pFactory(const QString &strClassname, QObject *pObject)
    {
        /* Creating QIComboBox accessibility interface: */
        if (pObject && strClassname == QLatin1String("QIComboBox"))
            return new QIAccessibilityInterfaceForQIComboBox(qobject_cast<QWidget*>(pObject));

        /* Null by default: */
        return 0;
    }

    /** Constructs an accessibility interface passing @a pWidget to the base-class. */
    QIAccessibilityInterfaceForQIComboBox(QWidget *pWidget)
        : QAccessibleWidget(pWidget, QAccessible::ToolBar)
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

    /** Returns corresponding QIComboBox. */
    QIComboBox *combo() const { return qobject_cast<QIComboBox*>(widget()); }
};


/*********************************************************************************************************************************
*   Class QIAccessibilityInterfaceForQIComboBox implementation.                                                                  *
*********************************************************************************************************************************/

int QIAccessibilityInterfaceForQIComboBox::childCount() const
{
    /* Make sure combo still alive: */
    AssertPtrReturn(combo(), 0);

    /* Return the number of children: */
    return combo()->subElementCount();
}

QAccessibleInterface *QIAccessibilityInterfaceForQIComboBox::child(int iIndex) const
{
    /* Make sure combo still alive: */
    AssertPtrReturn(combo(), 0);
    /* Make sure index is valid: */
    AssertReturn(iIndex >= 0 && iIndex < childCount(), 0);

    /* Return the child with the passed iIndex: */
    return QAccessible::queryAccessibleInterface(combo()->subElement(iIndex));
}

int QIAccessibilityInterfaceForQIComboBox::indexOfChild(const QAccessibleInterface *pChild) const
{
    /* Search for corresponding child: */
    for (int i = 0; i < childCount(); ++i)
        if (child(i) == pChild)
            return i;

    /* -1 by default: */
    return -1;
}

QString QIAccessibilityInterfaceForQIComboBox::text(QAccessible::Text /* enmTextRole */) const
{
    /* Return empty string: */
    return QString();
}


/*********************************************************************************************************************************
*   Class QIComboBox implementation.                                                                                             *
*********************************************************************************************************************************/

QIComboBox::QIComboBox(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pComboBox(0)
{
    /* Prepare all: */
    prepare();
}

int QIComboBox::subElementCount() const
{
    /* Depending on 'editable' property: */
    return !isEditable() ? (int)SubElement_Max : (int)SubElementEditable_Max;
}

QWidget *QIComboBox::subElement(int iIndex) const
{
    /* Make sure index is inside the bounds: */
    AssertReturn(iIndex >= 0 && iIndex < subElementCount(), 0);

    /* For 'non-editable' case: */
    if (!isEditable())
    {
        switch (iIndex)
        {
            case SubElement_Selector: return m_pComboBox;
            default: break;
        }
    }
    /* For 'editable' case: */
    else
    {
        switch (iIndex)
        {
            case SubElementEditable_Editor: return lineEdit();
            case SubElementEditable_Selector: return m_pComboBox;
            default: break;
        }
    }

    /* Null otherwise: */
    return 0;
}

QLineEdit *QIComboBox::lineEdit() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, 0);
    return m_pComboBox->lineEdit();
}

QComboBox *QIComboBox::comboBox() const
{
    return m_pComboBox;
}

QAbstractItemView *QIComboBox::view() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, 0);
    return m_pComboBox->view();
}

QSize QIComboBox::iconSize() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QSize());
    return m_pComboBox->iconSize();
}

QComboBox::InsertPolicy QIComboBox::insertPolicy() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QComboBox::NoInsert);
    return m_pComboBox->insertPolicy();
}

bool QIComboBox::isEditable() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, false);
    return m_pComboBox->isEditable();
}

int QIComboBox::count() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, 0);
    return m_pComboBox->count();
}

int QIComboBox::currentIndex() const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, -1);
    return m_pComboBox->currentIndex();
}

void QIComboBox::insertItem(int iIndex, const QString &strText, const QVariant &userData /* = QVariant() */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    return m_pComboBox->insertItem(iIndex, strText, userData);
}

void QIComboBox::removeItem(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    return m_pComboBox->removeItem(iIndex);
}

QVariant QIComboBox::itemData(int iIndex, int iRole /* = Qt::UserRole */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QVariant());
    return m_pComboBox->itemData(iIndex, iRole);
}

QIcon QIComboBox::itemIcon(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QIcon());
    return m_pComboBox->itemIcon(iIndex);
}

QString QIComboBox::itemText(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturn(m_pComboBox, QString());
    return m_pComboBox->itemText(iIndex);
}

void QIComboBox::setIconSize(const QSize &size) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setIconSize(size);
}

void QIComboBox::setInsertPolicy(QComboBox::InsertPolicy policy) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setInsertPolicy(policy);
}

void QIComboBox::setEditable(bool fEditable) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setEditable(fEditable);
}

void QIComboBox::setCurrentIndex(int iIndex) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setCurrentIndex(iIndex);
}

void QIComboBox::setItemData(int iIndex, const QVariant &value, int iRole /* = Qt::UserRole */) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemData(iIndex, value, iRole);
}

void QIComboBox::setItemIcon(int iIndex, const QIcon &icon) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemIcon(iIndex, icon);
}

void QIComboBox::setItemText(int iIndex, const QString &strText) const
{
    /* Redirect to combo-box: */
    AssertPtrReturnVoid(m_pComboBox);
    m_pComboBox->setItemText(iIndex, strText);
}

void QIComboBox::prepare()
{
    /* Install QIComboBox accessibility interface factory: */
    QAccessible::installFactory(QIAccessibilityInterfaceForQIComboBox::pFactory);

    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);

        /* Create combo-box: */
        m_pComboBox = new QComboBox;
        AssertPtrReturnVoid(m_pComboBox);
        {
            /* Configure combo-box: */
            connect(m_pComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
                    this, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::activated));
            connect(m_pComboBox, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
                    this, static_cast<void(QIComboBox::*)(const QString &)>(&QIComboBox::activated));
            connect(m_pComboBox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, static_cast<void(QIComboBox::*)(int)>(&QIComboBox::currentIndexChanged));
            connect(m_pComboBox, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
                    this, static_cast<void(QIComboBox::*)(const QString &)>(&QIComboBox::currentIndexChanged));
            connect(m_pComboBox, &QComboBox::currentTextChanged, this, &QIComboBox::currentTextChanged);
            connect(m_pComboBox, &QComboBox::editTextChanged, this, &QIComboBox::editTextChanged);
            connect(m_pComboBox, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::highlighted),
                    this, static_cast<void(QIComboBox::*)(const QString &)>(&QIComboBox::highlighted));
            connect(m_pComboBox, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::highlighted),
                    this, static_cast<void(QIComboBox::*)(const QString &)>(&QIComboBox::highlighted));
            /* Add combo-box into layout: */
            pLayout->addWidget(m_pComboBox);
        }
    }
}

