/* $Id$ */
/** @file
 * VBox Qt GUI - UIManagementToolsWidget class implementation.
 */

/*
 * Copyright (C) 2006-2025 Oracle and/or its affiliates.
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
#include <QVBoxLayout>

/* GUI includes: */
#include "UIManagementToolsWidget.h"
#include "UIToolPane.h"

/* Other VBox includes: */
#include "iprt/assert.h"


UIManagementToolsWidget::UIManagementToolsWidget(QWidget *pParent, UIActionPool *pActionPool)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pPane(0)
{
    prepare();
}

UIToolPane *UIManagementToolsWidget::toolPane() const
{
    return m_pPane;
}

UIToolType UIManagementToolsWidget::toolType() const
{
    AssertPtrReturn(toolPane(), UIToolType_Invalid);
    return toolPane()->currentTool();
}

bool UIManagementToolsWidget::isToolOpened(UIToolType enmType) const
{
    /* Sanity check: */
    AssertReturn(enmType != UIToolType_Invalid, false);
    /* Make sure new tool type is of Management class: */
    AssertReturn(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Management), false);

    AssertPtrReturn(toolPane(), false);
    return toolPane()->isToolOpened(enmType);
}

void UIManagementToolsWidget::switchToolTo(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Management class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Management));

    /* Open corresponding tool: */
    AssertPtrReturnVoid(toolPane());
    toolPane()->openTool(enmType);

    /* Let the parent know: */
    emit sigToolTypeChange();
}

void UIManagementToolsWidget::closeTool(UIToolType enmType)
{
    /* Sanity check: */
    AssertReturnVoid(enmType != UIToolType_Invalid);
    /* Make sure new tool type is of Management class: */
    AssertReturnVoid(UIToolStuff::isTypeOfClass(enmType, UIToolClass_Management));

    AssertPtrReturnVoid(toolPane());
    toolPane()->closeTool(enmType);
}

QString UIManagementToolsWidget::currentHelpKeyword() const
{
    AssertPtrReturn(toolPane(), QString());
    return toolPane()->currentHelpKeyword();
}

void UIManagementToolsWidget::prepare()
{
    /* Prepare everything: */
    prepareWidgets();
}

void UIManagementToolsWidget::prepareWidgets()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);

        /* Create tool-pane: */
        m_pPane = new UIToolPane(this, UIToolClass_Management, actionPool());
        if (toolPane())
        {
            /* Add into layout: */
            pLayout->addWidget(toolPane());
        }
    }
}
