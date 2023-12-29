/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewer class implementation.
 */

/*
 * Copyright (C) 2010-2023 Oracle and/or its affiliates.
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
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIIconPool.h"
#include "UIPaneContainer.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Other VBox includes: */
#include <iprt/assert.h>

UIPaneContainer::UIPaneContainer(QWidget *pParent, EmbedTo enmEmbedTo /* = EmbedTo_Stack */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedTo(enmEmbedTo)
    , m_pTabWidget(0)
{
    prepare();
    retranslateUi();
}

void UIPaneContainer::retranslateUi()
{
}

void UIPaneContainer::prepare()
{
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    AssertReturnVoid(pLayout);
    pLayout->setContentsMargins(0, 0, 0, 0);

    /* Add the tab widget: */
    m_pTabWidget = new QTabWidget;
    AssertReturnVoid(m_pTabWidget);
    connect(m_pTabWidget, &QTabWidget::currentChanged, this, &UIPaneContainer::sigCurrentTabChanged);
    pLayout->addWidget(m_pTabWidget);
}

void UIPaneContainer::sltHide()
{
    hide();
    emit sigHidden();
}

void UIPaneContainer::insertTab(int iIndex, QWidget *pPage, const QString &strLabel /* = QString() */)
{
    if (m_pTabWidget)
        m_pTabWidget->insertTab(iIndex, pPage, strLabel);
}

void UIPaneContainer::setTabText(int iIndex, const QString &strText)
{
    if (!m_pTabWidget || iIndex < 0 || iIndex >= m_pTabWidget->count())
        return;
    m_pTabWidget->setTabText(iIndex, strText);
}

void UIPaneContainer::setCurrentIndex(int iIndex)
{
    if (!m_pTabWidget || iIndex >= m_pTabWidget->count() || iIndex < 0)
        return;
    m_pTabWidget->setCurrentIndex(iIndex);
}

int UIPaneContainer::currentIndex() const
{
    if (!m_pTabWidget)
        return -1;
    return m_pTabWidget->currentIndex();
}
