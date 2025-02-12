/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolPaneGlobal class implementation.
 */

/*
 * Copyright (C) 2017-2024 Oracle and/or its affiliates.
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
#include <QApplication>
#include <QStackedLayout>
#include <QUuid>
#ifndef VBOX_WS_MAC
# include <QStyle>
#endif

/* GUI includes */
#include "UICommon.h"
#include "UICloudProfileManager.h"
#include "UIExtensionPackManager.h"
#include "UIHomePane.h"
#include "UIMachineToolsManagerWidget.h"
#include "UIMediumManager.h"
#include "UINetworkManager.h"
#include "UIToolPaneGlobal.h"
#include "UIVMActivityOverviewWidget.h"

/* Other VBox includes: */
#include <iprt/assert.h>


UIToolPaneGlobal::UIToolPaneGlobal(UIActionPool *pActionPool, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_pActionPool(pActionPool)
    , m_pLayout(0)
    , m_pPaneHome(0)
    , m_pPaneMachines(0)
    , m_pPaneExtensions(0)
    , m_pPaneMedia(0)
    , m_pPaneNetwork(0)
    , m_pPaneCloud(0)
    , m_pPaneActivities(0)
    , m_fActive(false)
{
    prepare();
}

UIToolPaneGlobal::~UIToolPaneGlobal()
{
    cleanup();
}

void UIToolPaneGlobal::setActive(bool fActive)
{
    /* Save activity: */
    if (m_fActive != fActive)
    {
        m_fActive = fActive;

        /* Handle token change: */
        handleTokenChange();
    }
}

UIToolType UIToolPaneGlobal::currentTool() const
{
    return   m_pLayout && m_pLayout->currentWidget()
           ? m_pLayout->currentWidget()->property("ToolType").value<UIToolType>()
           : UIToolType_Invalid;
}

bool UIToolPaneGlobal::isToolOpened(UIToolType enmType) const
{
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
            return true;
    return false;
}

void UIToolPaneGlobal::openTool(UIToolType enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type exists: */
    if (iActualIndex != -1)
    {
        /* Activate corresponding index: */
        m_pLayout->setCurrentIndex(iActualIndex);
    }
    /* Otherwise: */
    else
    {
        /* Create, remember, append corresponding stacked widget: */
        switch (enmType)
        {
            case UIToolType_Home:
            {
                /* Create Home pane: */
                m_pPaneHome = new UIHomePane;
                AssertPtrReturnVoid(m_pPaneHome);
                {
                    /* Configure pane: */
                    m_pPaneHome->setProperty("ToolType", QVariant::fromValue(UIToolType_Home));

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneHome);
                    m_pLayout->setCurrentWidget(m_pPaneHome);
                }
                break;
            }
            case UIToolType_Machines:
            {
                /* Create Machine Tool Manager: */
                m_pPaneMachines = new UIMachineToolsManagerWidget(this, m_pActionPool);
                AssertPtrReturnVoid(m_pPaneMachines);
                {
                    /* Configure pane: */
                    m_pPaneMachines->setProperty("ToolType", QVariant::fromValue(UIToolType_Machines));
                    /// @todo connect!
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneMachines->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneMachines);
                    m_pLayout->setCurrentWidget(m_pPaneMachines);
                }
                break;
            }
            case UIToolType_Extensions:
            {
                /* Create Extension Pack Manager: */
                m_pPaneExtensions = new UIExtensionPackManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneExtensions);
                {
                    /* Configure pane: */
                    m_pPaneExtensions->setProperty("ToolType", QVariant::fromValue(UIToolType_Extensions));
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneExtensions->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneExtensions);
                    m_pLayout->setCurrentWidget(m_pPaneExtensions);
                }
                break;
            }
            case UIToolType_Media:
            {
                /* Create Virtual Media Manager: */
                m_pPaneMedia = new UIMediumManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneMedia);
                {
                    /* Configure pane: */
                    m_pPaneMedia->setProperty("ToolType", QVariant::fromValue(UIToolType_Media));
                    connect(m_pPaneMedia, &UIMediumManagerWidget::sigCreateMedium,
                            this, &UIToolPaneGlobal::sigCreateMedium);
                    connect(m_pPaneMedia, &UIMediumManagerWidget::sigCopyMedium,
                            this, &UIToolPaneGlobal::sigCopyMedium);
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneMedia->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneMedia);
                    m_pLayout->setCurrentWidget(m_pPaneMedia);
                }
                break;
            }
            case UIToolType_Network:
            {
                /* Create Network Manager: */
                m_pPaneNetwork = new UINetworkManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneNetwork);
                {
                    /* Configure pane: */
                    m_pPaneNetwork->setProperty("ToolType", QVariant::fromValue(UIToolType_Network));
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneNetwork->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneNetwork);
                    m_pLayout->setCurrentWidget(m_pPaneNetwork);
                }
                break;
            }
            case UIToolType_Cloud:
            {
                /* Create Cloud Profile Manager: */
                m_pPaneCloud = new UICloudProfileManagerWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneCloud);
                {
                    /* Configure pane: */
                    m_pPaneCloud->setProperty("ToolType", QVariant::fromValue(UIToolType_Cloud));
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneCloud->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneCloud);
                    m_pLayout->setCurrentWidget(m_pPaneCloud);
                }
                break;
            }
            case UIToolType_Activities:
            {
                /* Create VM Activity Overview: */
                m_pPaneActivities = new UIVMActivityOverviewWidget(EmbedTo_Stack, m_pActionPool, false /* show toolbar */);
                AssertPtrReturnVoid(m_pPaneActivities);
                {
                    /* Configure pane: */
                    m_pPaneActivities->setProperty("ToolType", QVariant::fromValue(UIToolType_Activities));
                    connect(m_pPaneActivities, &UIVMActivityOverviewWidget::sigSwitchToMachineActivityPane,
                            this, &UIToolPaneGlobal::sigSwitchToMachineActivityPane);
                    m_pPaneActivities->setCloudMachineItems(m_cloudItems);
#ifndef VBOX_WS_MAC
                    const int iMargin = qApp->style()->pixelMetric(QStyle::PM_LayoutLeftMargin) / 4;
                    m_pPaneActivities->setContentsMargins(iMargin, 0, iMargin, 0);
#endif

                    /* Add into layout: */
                    m_pLayout->addWidget(m_pPaneActivities);
                    m_pLayout->setCurrentWidget(m_pPaneActivities);
                }
                break;
            }
            default:
                AssertFailedReturnVoid();
        }
    }

    /* Handle token change: */
    handleTokenChange();
}

void UIToolPaneGlobal::closeTool(UIToolType enmType)
{
    /* Search through the stacked widgets: */
    int iActualIndex = -1;
    for (int iIndex = 0; iIndex < m_pLayout->count(); ++iIndex)
        if (m_pLayout->widget(iIndex)->property("ToolType").value<UIToolType>() == enmType)
            iActualIndex = iIndex;

    /* If widget with such type doesn't exist: */
    if (iActualIndex != -1)
    {
        /* Forget corresponding widget: */
        switch (enmType)
        {
            case UIToolType_Home:       m_pPaneHome = 0; break;
            case UIToolType_Machines:   m_pPaneMachines = 0; break;
            case UIToolType_Extensions: m_pPaneExtensions = 0; break;
            case UIToolType_Media:      m_pPaneMedia = 0; break;
            case UIToolType_Network:    m_pPaneNetwork = 0; break;
            case UIToolType_Cloud:      m_pPaneCloud = 0; break;
            case UIToolType_Activities: m_pPaneActivities = 0; break;
            default: break;
        }
        /* Delete corresponding widget: */
        QWidget *pWidget = m_pLayout->widget(iActualIndex);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }

    /* Handle token change: */
    handleTokenChange();
}

QString UIToolPaneGlobal::currentHelpKeyword() const
{
    QWidget *pCurrentToolWidget = 0;
    switch (currentTool())
    {
        case UIToolType_Home:
            pCurrentToolWidget = m_pPaneHome;
            break;
        case UIToolType_Machines:
            pCurrentToolWidget = m_pPaneMachines;
            break;
        case UIToolType_Extensions:
            pCurrentToolWidget = m_pPaneExtensions;
            break;
        case UIToolType_Media:
            pCurrentToolWidget = m_pPaneMedia;
            break;
        case UIToolType_Network:
            pCurrentToolWidget = m_pPaneNetwork;
            break;
        case UIToolType_Cloud:
            pCurrentToolWidget = m_pPaneCloud;
            break;
        case UIToolType_Activities:
            pCurrentToolWidget = m_pPaneActivities;
            break;
        default:
            break;
    }
    return uiCommon().helpKeyword(pCurrentToolWidget);
}

void UIToolPaneGlobal::setCloudMachineItems(const QList<UIVirtualMachineItemCloud*> &cloudItems)
{
    /* Cache passed value: */
    m_cloudItems = cloudItems;

    /* Update activity overview pane if it is open: */
    if (isToolOpened(UIToolType_Activities))
    {
        AssertPtrReturnVoid(m_pPaneActivities);
        m_pPaneActivities->setCloudMachineItems(m_cloudItems);
    }
}

UIMachineToolsManagerWidget *UIToolPaneGlobal::machineToolManager() const
{
    return m_pPaneMachines;
}

void UIToolPaneGlobal::prepare()
{
    /* Create stacked-layout: */
    m_pLayout = new QStackedLayout(this);

    /* Create welcome pane: */
    openTool(UIToolType_Home);
    /* Create machines pane: */
    openTool(UIToolType_Machines);
}

void UIToolPaneGlobal::cleanup()
{
    /* Remove all widgets prematurelly: */
    while (m_pLayout->count())
    {
        QWidget *pWidget = m_pLayout->widget(0);
        m_pLayout->removeWidget(pWidget);
        delete pWidget;
    }
}

void UIToolPaneGlobal::handleTokenChange()
{
    /* Determine whether resource monitor is currently active tool: */
    if (m_pPaneActivities)
        m_pPaneActivities->setIsCurrentTool(m_fActive && currentTool() == UIToolType_Activities);
}
