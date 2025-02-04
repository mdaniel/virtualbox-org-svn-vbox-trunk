/* $Id$ */
/** @file
 * VBox Qt GUI - UIToolsModel class implementation.
 */

/*
 * Copyright (C) 2012-2024 Oracle and/or its affiliates.
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
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QScrollBar>
#include <QTimer>

/* GUI includes: */
#include "QIMessageBox.h"
#include "UIActionPoolManager.h"
#include "UIIconPool.h"
#include "UILoggingDefs.h"
#include "UITools.h"
#include "UIToolsModel.h"
#include "UITranslationEventListener.h"
#include "UIExtraDataDefs.h"
#include "UIExtraDataManager.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIVirtualBoxManagerWidget.h"
#include "UIVirtualBoxEventHandler.h"

/* COM includes: */
#include "CExtPack.h"
#include "CExtPackManager.h"

/* Qt includes: */
#include <QParallelAnimationGroup>

/* Type defs: */
typedef QSet<QString> UIStringSet;


UIToolsModel::UIToolsModel(UIToolClass enmClass, UITools *pParent)
    : QObject(pParent)
    , m_enmClass(enmClass)
    , m_pTools(pParent)
    , m_pScene(0)
    , m_fItemsEnabled(true)
    , m_fShowItemNames(false)
{
    prepare();
}

UIToolsModel::~UIToolsModel()
{
    cleanup();
}

void UIToolsModel::init()
{
    /* Update linked values: */
    updateLayout();
    updateNavigation();
    sltItemMinimumWidthHintChanged();
    sltItemMinimumHeightHintChanged();

    /* Load settings: */
    loadSettings();
}

UITools *UIToolsModel::tools() const
{
    return m_pTools;
}

UIActionPool *UIToolsModel::actionPool() const
{
    return tools() ? tools()->actionPool() : 0;
}

QGraphicsScene *UIToolsModel::scene() const
{
    return m_pScene;
}

QPaintDevice *UIToolsModel::paintDevice() const
{
    if (scene() && !scene()->views().isEmpty())
        return scene()->views().first();
    return 0;
}

QGraphicsItem *UIToolsModel::itemAt(const QPointF &position, const QTransform &deviceTransform /* = QTransform() */) const
{
    return scene() ? scene()->itemAt(position, deviceTransform) : 0;
}

void UIToolsModel::setToolsType(UIToolType enmType)
{
    if (!currentItem() || currentItem()->itemType() != enmType)
    {
        foreach (UIToolsItem *pItem, items())
            if (pItem->itemType() == enmType)
            {
                setCurrentItem(pItem);
                break;
            }
    }
}

UIToolType UIToolsModel::toolsType() const
{
    return currentItem() ? currentItem()->itemType() : UIToolType_Invalid;
}

void UIToolsModel::setItemsEnabled(bool fEnabled)
{
    if (m_fItemsEnabled != fEnabled)
    {
        m_fItemsEnabled = fEnabled;
        foreach (UIToolsItem *pItem, items())
            pItem->setEnabled(m_fItemsEnabled);
    }
}

bool UIToolsModel::isItemsEnabled() const
{
    return m_fItemsEnabled;
}

void UIToolsModel::setRestrictedToolTypes(const QList<UIToolType> &types)
{
    if (m_restrictedToolTypes != types)
    {
        m_restrictedToolTypes = types;
        foreach (UIToolsItem *pItem, items())
            pItem->setVisible(!m_restrictedToolTypes.contains(pItem->itemType()));
        updateLayout();
        updateNavigation();
        sltItemMinimumWidthHintChanged();
        sltItemMinimumHeightHintChanged();
    }
}

QList<UIToolType> UIToolsModel::restrictedToolTypes() const
{
    return m_restrictedToolTypes;
}

void UIToolsModel::close()
{
    emit sigClose();
}

void UIToolsModel::setCurrentItem(UIToolsItem *pItem)
{
    /* Is there something changed? */
    if (m_pCurrentItem == pItem)
        return;

    /* Remember old current-item: */
    UIToolsItem *pOldCurrentItem = m_pCurrentItem;

    /* If there is item: */
    if (pItem)
    {
        /* Set this item as current: */
        m_pCurrentItem = pItem;

        /* Load last tool types: */
        UIToolType enmTypeGlobal, enmTypeMachine;
        loadLastToolTypes(enmTypeGlobal, enmTypeMachine);

        /* Depending on tool class: */
        switch (m_enmClass)
        {
            case UIToolClass_Global: enmTypeGlobal = m_pCurrentItem->itemType(); break;
            case UIToolClass_Machine: enmTypeMachine = m_pCurrentItem->itemType(); break;
            default: break;
        }

        /* Save selected items data: */
        const QList<UIToolType> currentTypes = QList<UIToolType>() << enmTypeGlobal << enmTypeMachine;
        LogRel2(("GUI: UIToolsModel: Saving tool items as: Global=%d, Machine=%d\n",
                 (int)enmTypeGlobal, (int)enmTypeMachine));
        gEDataManager->setToolsPaneLastItemsChosen(currentTypes);
    }
    /* Otherwise reset current item: */
    else
        m_pCurrentItem = 0;

    /* Update old item (if any): */
    if (pOldCurrentItem)
        pOldCurrentItem->update();
    /* Update new item (if any): */
    if (m_pCurrentItem)
        m_pCurrentItem->update();

    /* Notify about selection change: */
    emit sigSelectionChanged(toolsType());

    /* Move focus to current-item: */
    setFocusItem(currentItem());

    /* Adjust corrresponding actions finally: */
    const UIToolType enmType = currentItem() ? currentItem()->itemType() : UIToolType_Welcome;
    QMap<UIToolType, UIAction*> actions;
    actions[UIToolType_Welcome] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_WelcomeScreen);
    actions[UIToolType_Extensions] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_ExtensionPackManager);
    actions[UIToolType_Media] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_VirtualMediaManager);
    actions[UIToolType_Network] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_NetworkManager);
    actions[UIToolType_Cloud] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_CloudProfileManager);
    actions[UIToolType_Activities] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_VMActivityOverview);
#ifdef VBOX_GUI_WITH_ADVANCED_WIDGETS
    actions[UIToolType_Machines] = actionPool()->action(UIActionIndexMN_M_File_M_Tools_T_MachineManager);
#endif
    if (actions.contains(enmType))
        actions.value(enmType)->setChecked(true);
}

UIToolsItem *UIToolsModel::currentItem() const
{
    return m_pCurrentItem;
}

void UIToolsModel::setFocusItem(UIToolsItem *pItem)
{
    /* Always make sure real focus unset: */
    scene()->setFocusItem(0);

    /* Is there something changed? */
    if (m_pFocusItem == pItem)
        return;

    /* Remember old focus-item: */
    UIToolsItem *pOldFocusItem = m_pFocusItem;

    /* If there is item: */
    if (pItem)
    {
        /* Set this item to focus if navigation list contains it: */
        if (navigationList().contains(pItem))
            m_pFocusItem = pItem;
        /* Otherwise it's error: */
        else
            AssertMsgFailed(("Passed item is not in navigation list!"));
    }
    /* Otherwise reset focus item: */
    else
        m_pFocusItem = 0;

    /* Disconnect old focus-item (if any): */
    if (pOldFocusItem)
        disconnect(pOldFocusItem, &UIToolsItem::destroyed, this, &UIToolsModel::sltFocusItemDestroyed);
    /* Connect new focus-item (if any): */
    if (m_pFocusItem)
        connect(m_pFocusItem.data(), &UIToolsItem::destroyed, this, &UIToolsModel::sltFocusItemDestroyed);

    /* Notify about focus change: */
    emit sigFocusChanged();
}

UIToolsItem *UIToolsModel::focusItem() const
{
    return m_pFocusItem;
}

const QList<UIToolsItem*> &UIToolsModel::navigationList() const
{
    return m_navigationList;
}

void UIToolsModel::removeFromNavigationList(UIToolsItem *pItem)
{
    AssertMsg(pItem, ("Passed item is invalid!"));
    m_navigationList.removeAll(pItem);
}

void UIToolsModel::updateNavigation()
{
    /* Clear list initially: */
    m_navigationList.clear();

    /* Enumerate the children: */
    foreach (UIToolsItem *pItem, items())
        if (pItem->isVisible())
            m_navigationList << pItem;
}

QList<UIToolsItem*> UIToolsModel::items() const
{
    return m_items;
}

UIToolsItem *UIToolsModel::item(UIToolType enmType) const
{
    foreach (UIToolsItem *pItem, items())
        if (pItem->itemType() == enmType)
            return pItem;
    return 0;
}

bool UIToolsModel::showItemNames() const
{
    return m_fShowItemNames;
}

bool UIToolsModel::isAtLeastOneItemHovered() const
{
    foreach (UIToolsItem *pItem, items())
        if (pItem->isHovered())
            return true;
    return false;
}

void UIToolsModel::updateLayout()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();
    const int iMajorSpacing = data(ToolsModelData_MajorSpacing).toInt();
    const QSize viewportSize = scene()->views()[0]->viewport()->size();
    const int iViewportWidth = viewportSize.width();
    int iVerticalIndent = iMargin;

    /* Init last item type: */
    UIToolType enmLastType = UIToolType_Invalid;

    /* Layout the children: */
    foreach (UIToolsItem *pItem, items())
    {
        /* Make sure item visible: */
        if (!pItem->isVisible())
            continue;

        /* In widget mode we should add spacing after Welcome item: */
        if (   !tools()->isPopup()
            && enmLastType == UIToolType_Welcome)
            iVerticalIndent += iMajorSpacing;

        /* Set item position: */
        pItem->setPos(iMargin, iVerticalIndent);
        /* Set root-item size: */
        pItem->resize(iViewportWidth, pItem->minimumHeightHint());
        /* Make sure item is shown: */
        pItem->show();
        /* Advance vertical indent: */
        iVerticalIndent += (pItem->minimumHeightHint() + iSpacing);

        /* Remember last item type: */
        enmLastType = pItem->itemType();
    }
}

void UIToolsModel::sltItemMinimumWidthHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();

    /* Calculate maximum horizontal width: */
    int iMinimumWidthHint = 0;
    iMinimumWidthHint += 2 * iMargin;
    foreach (UIToolsItem *pItem, items())
        iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());

    /* Notify listeners: */
    emit sigItemMinimumWidthHintChanged(iMinimumWidthHint);
}

void UIToolsModel::sltItemMinimumHeightHintChanged()
{
    /* Prepare variables: */
    const int iMargin = data(ToolsModelData_Margin).toInt();
    const int iSpacing = data(ToolsModelData_Spacing).toInt();

    /* Calculate summary vertical height: */
    int iMinimumHeightHint = 0;
    iMinimumHeightHint += 2 * iMargin;
    foreach (UIToolsItem *pItem, items())
        if (pItem->isVisible())
            iMinimumHeightHint += (pItem->minimumHeightHint() + iSpacing);
    iMinimumHeightHint -= iSpacing;

    /* Notify listeners: */
    emit sigItemMinimumHeightHintChanged(iMinimumHeightHint);
}

bool UIToolsModel::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Process only scene events: */
    if (pWatched != scene())
        return QObject::eventFilter(pWatched, pEvent);

    /* Process only item focused by model: */
    if (scene()->focusItem())
        return QObject::eventFilter(pWatched, pEvent);

    /* Do not handle disabled items: */
    if (!currentItem()->isEnabled())
        return QObject::eventFilter(pWatched, pEvent);

    /* Checking event-type: */
    switch (pEvent->type())
    {
        /* Mouse handler: */
        case QEvent::GraphicsSceneMouseRelease:
        {
            /* Acquire event: */
            QGraphicsSceneMouseEvent *pMouseEvent = static_cast<QGraphicsSceneMouseEvent*>(pEvent);
            /* Get item under mouse cursor: */
            QPointF scenePos = pMouseEvent->scenePos();
            if (QGraphicsItem *pItemUnderMouse = itemAt(scenePos))
            {
                /* Which button it was? */
                switch (pMouseEvent->button())
                {
                    /* Both buttons: */
                    case Qt::LeftButton:
                    case Qt::RightButton:
                    {
                        /* Which item we just clicked? */
                        UIToolsItem *pClickedItem = qgraphicsitem_cast<UIToolsItem*>(pItemUnderMouse);
                        if (pClickedItem)
                        {
                            /* Do we have extra-button? */
                            if (pClickedItem->hasExtraButton())
                            {
                                /* Check if clicked place is within extra-button geometry: */
                                const QPointF itemPos = pClickedItem->mapFromParent(scenePos);
                                if (pClickedItem->extraButtonRect().contains(itemPos.toPoint()))
                                {
                                    /* Handle known button types: */
                                    switch (pClickedItem->itemType())
                                    {
                                        case UIToolType_Welcome:
                                        {
                                            /* Toggle the button: */
                                            m_fShowItemNames = !m_fShowItemNames;
                                            /* Update geometry for all the items: */
                                            foreach (UIToolsItem *pItem, m_items)
                                                pItem->updateGeometry();
                                            /* Recalculate layout: */
                                            updateLayout();
                                            break;
                                        }
                                        default:
                                            break;
                                    }
                                }
                            }

                            /* Make clicked item the current one: */
                            if (pClickedItem->isEnabled())
                            {
                                setCurrentItem(pClickedItem);
                                /* Close the widget in popup mode only: */
                                if (tools()->isPopup())
                                    close();
                                return true;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            break;
        }
        default:
            break;
    }

    /* Call to base-class: */
    return QObject::eventFilter(pWatched, pEvent);
}

void UIToolsModel::sltHandleItemHoverChange()
{
    /* Just update all the items: */
    foreach (UIToolsItem *pItem, items())
        pItem->update();
}

void UIToolsModel::sltFocusItemDestroyed()
{
    AssertMsgFailed(("Focus item destroyed!"));
}

void UIToolsModel::sltRetranslateUI()
{
    foreach (UIToolsItem *pItem, m_items)
    {
        switch (pItem->itemType())
        {
            case UIToolType_Welcome:     pItem->setName(tr("Welcome")); break;
            case UIToolType_Extensions:  pItem->setName(tr("Extensions")); break;
            case UIToolType_Media:       pItem->setName(tr("Media")); break;
            case UIToolType_Network:     pItem->setName(tr("Network")); break;
            case UIToolType_Cloud:       pItem->setName(tr("Cloud")); break;
            case UIToolType_Activities:  pItem->setName(tr("Activities")); break;
#ifdef VBOX_GUI_WITH_ADVANCED_WIDGETS
            case UIToolType_Machines:    pItem->setName(tr("Machines")); break;
#endif
            case UIToolType_Details:     pItem->setName(tr("Details")); break;
            case UIToolType_Snapshots:   pItem->setName(tr("Snapshots")); break;
            case UIToolType_Logs:        pItem->setName(tr("Logs")); break;
            case UIToolType_VMActivity:  pItem->setName(tr("Activity")); break;
            case UIToolType_FileManager: pItem->setName(tr("File Manager")); break;
            default: break;
        }
    }
}

void UIToolsModel::prepare()
{
    /* Prepare everything: */
    prepareScene();
    prepareItems();
    prepareConnections();

    /* Apply language settings: */
    sltRetranslateUI();
}

void UIToolsModel::prepareScene()
{
    m_pScene = new QGraphicsScene(this);
    if (m_pScene)
        m_pScene->installEventFilter(this);
}

void UIToolsModel::prepareItems()
{
    /* Depending on tool class: */
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            /* Welcome: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/welcome_screen_24px.png",
                                                                    ":/welcome_screen_24px.png"),
                                       UIToolClass_Global, UIToolType_Welcome,
                                       !tools()->isPopup() /* extra-button */);

            /* Extensions: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/extension_pack_manager_24px.png",
                                                                    ":/extension_pack_manager_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Extensions);

            /* Media: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/media_manager_24px.png",
                                                                    ":/media_manager_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Media);

            /* Network: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/host_iface_manager_24px.png",
                                                                    ":/host_iface_manager_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Network);

            /* Cloud: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/cloud_profile_manager_24px.png",
                                                                    ":/cloud_profile_manager_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Cloud);

            /* Activities: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/resources_monitor_24px.png",
                                                                    ":/resources_monitor_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Activities);

#ifdef VBOX_GUI_WITH_ADVANCED_WIDGETS
            /* Machines: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/machine_details_manager_24px.png",
                                                                    ":/machine_details_manager_disabled_24px.png"),
                                       UIToolClass_Global, UIToolType_Machines);
#endif

            break;
        }
        case UIToolClass_Machine:
        {
            /* Details: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/machine_details_manager_24px.png",
                                                                    ":/machine_details_manager_disabled_24px.png"),
                                       UIToolClass_Machine, UIToolType_Details);

            /* Snapshots: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/snapshot_manager_24px.png",
                                                                    ":/snapshot_manager_disabled_24px.png"),
                                       UIToolClass_Machine, UIToolType_Snapshots);

            /* Logs: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/vm_show_logs_24px.png",
                                                                    ":/vm_show_logs_disabled_24px.png"),
                                       UIToolClass_Machine, UIToolType_Logs);

            /* Activity: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/performance_monitor_24px.png",
                                                                    ":/performance_monitor_disabled_24px.png"),
                                       UIToolClass_Machine, UIToolType_VMActivity);

            /* File Manager: */
            m_items << new UIToolsItem(scene(), UIIconPool::iconSet(":/file_manager_24px.png",
                                                                    ":/file_manager_disabled_24px.png"),
                                       UIToolClass_Machine, UIToolType_FileManager);

            break;
        }
        default:
            break;
    }
}

void UIToolsModel::prepareConnections()
{
    /* Translation stuff: */
    connect(&translationEventListener(), &UITranslationEventListener::sigRetranslateUI,
            this, &UIToolsModel::sltRetranslateUI);

    /* Connect item hover listeners: */
    foreach (UIToolsItem *pItem, m_items)
    {
        connect(pItem, &UIToolsItem::sigHoverEnter,
                this, &UIToolsModel::sltHandleItemHoverChange);
        connect(pItem, &UIToolsItem::sigHoverLeave,
                this, &UIToolsModel::sltHandleItemHoverChange);
    }
}

void UIToolsModel::loadSettings()
{
    /* Load last tool types: */
    UIToolType enmTypeGlobal, enmTypeMachine;
    loadLastToolTypes(enmTypeGlobal, enmTypeMachine);

    /* Depending on tool class: */
    UIToolsItem *pCurrentItem = 0;
    switch (m_enmClass)
    {
        case UIToolClass_Global:
        {
            foreach (UIToolsItem *pItem, items())
                if (pItem->itemType() == enmTypeGlobal)
                    pCurrentItem = pItem;
            if (!pCurrentItem)
                pCurrentItem = item(UIToolType_Welcome);
            break;
        }
        case UIToolClass_Machine:
        {
            foreach (UIToolsItem *pItem, items())
                if (pItem->itemType() == enmTypeMachine)
                    pCurrentItem = pItem;
            if (!pCurrentItem)
                pCurrentItem = item(UIToolType_Details);
            break;
        }
        default:
            break;
    }
    setCurrentItem(pCurrentItem);
}

/* static */
void UIToolsModel::loadLastToolTypes(UIToolType &enmTypeGlobal, UIToolType &enmTypeMachine)
{
    /* Load selected items data: */
    const QList<UIToolType> data = gEDataManager->toolsPaneLastItemsChosen();
    enmTypeGlobal = data.value(0);
    if (!UIToolStuff::isTypeOfClass(enmTypeGlobal, UIToolClass_Global))
        enmTypeGlobal = UIToolType_Welcome;
    enmTypeMachine = data.value(1);
    if (!UIToolStuff::isTypeOfClass(enmTypeMachine, UIToolClass_Machine))
        enmTypeMachine = UIToolType_Details;
    LogRel2(("GUI: UIToolsModel: Restoring tool items as: Global=%d, Machine=%d\n",
             (int)enmTypeGlobal, (int)enmTypeMachine));
}

void UIToolsModel::cleanupItems()
{
    foreach (UIToolsItem *pItem, m_items)
        delete pItem;
    m_items.clear();
}

void UIToolsModel::cleanupScene()
{
    delete m_pScene;
    m_pScene = 0;
}

void UIToolsModel::cleanup()
{
    /* Cleanup items: */
    cleanupItems();
    /* Cleanup scene: */
    cleanupScene();
}

QVariant UIToolsModel::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case ToolsModelData_Margin: return 0;
        case ToolsModelData_Spacing: return 1;
        case ToolsModelData_MajorSpacing: return 0;

        /* Default: */
        default: break;
    }
    return QVariant();
}
