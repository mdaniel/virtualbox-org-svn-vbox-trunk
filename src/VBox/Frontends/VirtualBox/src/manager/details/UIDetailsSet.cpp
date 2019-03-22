/* $Id$ */
/** @file
 * VBox Qt GUI - UIDetailsSet class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QPainter>
# include <QStyle>
# include <QStyleOptionGraphicsItem>

/* GUI includes: */
# include "UIDetailsElements.h"
# include "UIDetailsModel.h"
# include "UIDetailsSet.h"
# include "UIVirtualBoxEventHandler.h"
# include "UIVirtualMachineItem.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIDetailsSet::UIDetailsSet(UIDetailsItem *pParent)
    : UIDetailsItem(pParent)
    , m_pMachineItem(0)
    , m_fFullSet(true)
    , m_fHasDetails(false)
    , m_configurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_pBuildStep(0)
    , m_iLastStepNumber(-1)
{
    /* Add set to the parent group: */
    parentItem()->addItem(this);

    /* Prepare set: */
    prepareSet();

    /* Prepare connections: */
    prepareConnections();
}

UIDetailsSet::~UIDetailsSet()
{
    /* Cleanup items: */
    clearItems();

    /* Remove set from the parent group: */
    parentItem()->removeItem(this);
}

void UIDetailsSet::buildSet(UIVirtualMachineItem *pMachineItem, bool fFullSet, const QMap<DetailsElementType, bool> &settings)
{
    /* Remember passed arguments: */
    m_pMachineItem = pMachineItem;
    m_machine = m_pMachineItem->machine();
    m_fHasDetails = m_pMachineItem->hasDetails();
    m_fFullSet = fFullSet;
    m_settings = settings;

    /* Cleanup superfluous items: */
    if (!m_fFullSet || !m_fHasDetails)
    {
        int iFirstItem = m_fHasDetails ? DetailsElementType_Display : DetailsElementType_General;
        int iLastItem = DetailsElementType_Description;
        bool fCleanupPerformed = false;
        for (int i = iFirstItem; i <= iLastItem; ++i)
            if (m_elements.contains(i))
            {
                delete m_elements[i];
                fCleanupPerformed = true;
            }
        if (fCleanupPerformed)
            updateGeometry();
    }

    /* Make sure we have details: */
    if (!m_fHasDetails)
    {
        /* Reset last-step number: */
        m_iLastStepNumber = -1;
        /* Notify parent group we are built: */
        emit sigBuildDone();
        return;
    }

    /* Choose last-step number: */
    m_iLastStepNumber = m_fFullSet ? DetailsElementType_Description : DetailsElementType_Preview;

    /* Fetch USB controller restrictions: */
    const CUSBDeviceFilters &filters = m_machine.GetUSBDeviceFilters();
    if (filters.isNull() || !m_machine.GetUSBProxyAvailable())
        m_settings.remove(DetailsElementType_USB);

    /* Start building set: */
    rebuildSet();
}

void UIDetailsSet::sltBuildStep(const QUuid &uStepId, int iStepNumber)
{
    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Is step id valid? */
    if (uStepId != m_uSetId)
        return;

    /* Step number feats the bounds: */
    if (iStepNumber >= 0 && iStepNumber <= m_iLastStepNumber)
    {
        /* Load details settings: */
        DetailsElementType enmElementType = (DetailsElementType)iStepNumber;
        /* Should the element be visible? */
        bool fVisible = m_settings.contains(enmElementType);
        /* Should the element be opened? */
        bool fOpen = fVisible && m_settings[enmElementType];

        /* Check if element is present already: */
        UIDetailsElement *pElement = element(enmElementType);
        if (pElement && fOpen)
            pElement->open(false);
        /* Create element if necessary: */
        bool fJustCreated = false;
        if (!pElement)
        {
            fJustCreated = true;
            pElement = createElement(enmElementType, fOpen);
        }

        /* Show element if necessary: */
        if (fVisible && !pElement->isVisible())
        {
            /* Show the element: */
            pElement->show();
            /* Recursively update size-hint: */
            pElement->updateGeometry();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Hide element if necessary: */
        else if (!fVisible && pElement->isVisible())
        {
            /* Hide the element: */
            pElement->hide();
            /* Recursively update size-hint: */
            updateGeometry();
            /* Update layout: */
            model()->updateLayout();
        }
        /* Update model if necessary: */
        else if (fJustCreated)
            model()->updateLayout();

        /* For visible element: */
        if (pElement->isVisible())
        {
            /* Create next build-step: */
            m_pBuildStep = new UIPrepareStep(this, pElement, uStepId, iStepNumber + 1);

            /* Build element: */
            pElement->updateAppearance();
        }
        /* For invisible element: */
        else
        {
            /* Just build next step: */
            sltBuildStep(uStepId, iStepNumber + 1);
        }
    }
    /* Step number out of bounds: */
    else
    {
        /* Update model: */
        model()->updateLayout();
        /* Repaint all the items: */
        foreach (UIDetailsItem *pItem, items())
            pItem->update();
        /* Notify listener about build done: */
        emit sigBuildDone();
    }
}

void UIDetailsSet::paint(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions, QWidget *)
{
    /* Paint background: */
    paintBackground(pPainter, pOptions);
}

QString UIDetailsSet::description() const
{
    return tr("Contains the details of virtual machine '%1'").arg(m_pMachineItem->name());
}

void UIDetailsSet::addItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Element:
        {
            UIDetailsElement *pElement = pItem->toElement();
            DetailsElementType type = pElement->elementType();
            AssertMsg(!m_elements.contains(type), ("Element already added!"));
            m_elements.insert(type, pItem);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

void UIDetailsSet::removeItem(UIDetailsItem *pItem)
{
    switch (pItem->type())
    {
        case UIDetailsItemType_Element:
        {
            UIDetailsElement *pElement = pItem->toElement();
            DetailsElementType type = pElement->elementType();
            AssertMsg(m_elements.contains(type), ("Element do not present (type = %d)!", (int)type));
            m_elements.remove(type);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

QList<UIDetailsItem*> UIDetailsSet::items(UIDetailsItemType enmType /* = UIDetailsItemType_Element */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Element: return m_elements.values();
        case UIDetailsItemType_Any: return items(UIDetailsItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return QList<UIDetailsItem*>();
}

bool UIDetailsSet::hasItems(UIDetailsItemType enmType /* = UIDetailsItemType_Element */) const
{
    switch (enmType)
    {
        case UIDetailsItemType_Element: return !m_elements.isEmpty();
        case UIDetailsItemType_Any: return hasItems(UIDetailsItemType_Element);
        default: AssertMsgFailed(("Invalid item type!")); break;
    }
    return false;
}

void UIDetailsSet::clearItems(UIDetailsItemType enmType /* = UIDetailsItemType_Element */)
{
    switch (enmType)
    {
        case UIDetailsItemType_Element:
        {
            foreach (int iKey, m_elements.keys())
                delete m_elements[iKey];
            AssertMsg(m_elements.isEmpty(), ("Set items cleanup failed!"));
            break;
        }
        case UIDetailsItemType_Any:
        {
            clearItems(UIDetailsItemType_Element);
            break;
        }
        default:
        {
            AssertMsgFailed(("Invalid item type!"));
            break;
        }
    }
}

UIDetailsElement *UIDetailsSet::element(DetailsElementType enmElementType) const
{
    UIDetailsItem *pItem = m_elements.value(enmElementType, 0);
    if (pItem)
        return pItem->toElement();
    return 0;
}

void UIDetailsSet::updateLayout()
{
    /* Prepare variables: */
    const int iMargin = data(SetData_Margin).toInt();
    const int iSpacing = data(SetData_Spacing).toInt();
    const int iMaximumWidth = geometry().width();
    const UIDetailsElement *pPreviewElement = element(DetailsElementType_Preview);
    const int iPreviewWidth = pPreviewElement ? pPreviewElement->minimumWidthHint() : 0;
    const int iPreviewHeight = pPreviewElement ? pPreviewElement->minimumHeightHint() : 0;
    int iVerticalIndent = iMargin;

    /* Calculate Preview group elements: */
    QList<DetailsElementType> inGroup;
    QList<DetailsElementType> outGroup;
    int iAdditionalGroupHeight = 0;
    int iAdditionalPreviewHeight = 0;
    enumerateLayoutItems(inGroup, outGroup, iAdditionalGroupHeight, iAdditionalPreviewHeight);

    /* Layout all the elements: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        const DetailsElementType enmElementType = pElement->elementType();
        switch (enmElementType)
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
            case DetailsElementType_Description:
            {
                /* Move element: */
                pElement->setPos(0, iVerticalIndent);

                /* Calculate required width: */
                int iWidth = iMaximumWidth;
                if (inGroup.contains(enmElementType))
                    iWidth -= (iSpacing + iPreviewWidth);
                /* Resize element to required width (separately from height): */
                if (pElement->geometry().width() != iWidth)
                    pElement->resize(iWidth, pElement->geometry().height());

                /* Calculate required height: */
                int iHeight = pElement->minimumHeightHint();
                if (   !inGroup.isEmpty()
                    && inGroup.last() == enmElementType)
                {
                    if (!pElement->isAnimationRunning() && !pElement->isClosed())
                        iHeight += iAdditionalGroupHeight;
                    else
                        iVerticalIndent += iAdditionalGroupHeight;
                }
                /* Resize element to required height (separately from width): */
                if (pElement->geometry().height() != iHeight)
                    pElement->resize(pElement->geometry().width(), iHeight);

                /* Layout element content: */
                pItem->updateLayout();
                /* Advance indent: */
                iVerticalIndent += (iHeight + iSpacing);

                break;
            }
            case DetailsElementType_Preview:
            {
                /* Move element: */
                pElement->setPos(iMaximumWidth - iPreviewWidth, iMargin);

                /* Calculate required size: */
                int iWidth = iPreviewWidth;
                int iHeight = iPreviewHeight;
                if (!pElement->isAnimationRunning() && !pElement->isClosed())
                    iHeight += iAdditionalPreviewHeight;
                /* Resize element to required size: */
                pElement->resize(iWidth, iHeight);

                /* Layout element content: */
                pItem->updateLayout();

                break;
            }
            default: AssertFailed(); break; /* Shut up, MSC! */
        }
    }
}

int UIDetailsSet::minimumWidthHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    const int iSpacing = data(SetData_Spacing).toInt();
    int iMinimumWidthHint = 0;

    /* Take into account all the elements: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        switch (pElement->elementType())
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
            case DetailsElementType_Description:
            {
                iMinimumWidthHint = qMax(iMinimumWidthHint, pItem->minimumWidthHint());
                break;
            }
            case DetailsElementType_Preview:
            {
                UIDetailsItem *pGeneralItem = element(DetailsElementType_General);
                UIDetailsItem *pSystemItem = element(DetailsElementType_System);
                int iGeneralElementWidth = pGeneralItem ? pGeneralItem->minimumWidthHint() : 0;
                int iSystemElementWidth = pSystemItem ? pSystemItem->minimumWidthHint() : 0;
                int iFirstColumnWidth = qMax(iGeneralElementWidth, iSystemElementWidth);
                iMinimumWidthHint = qMax(iMinimumWidthHint, iFirstColumnWidth + iSpacing + pItem->minimumWidthHint());
                break;
            }
            default: AssertFailed(); break; /* Shut up, MSC! */
        }
    }

    /* Return result: */
    return iMinimumWidthHint;
}

int UIDetailsSet::minimumHeightHint() const
{
    /* Zero if has no details: */
    if (!hasDetails())
        return 0;

    /* Prepare variables: */
    const int iMargin = data(SetData_Margin).toInt();
    const int iSpacing = data(SetData_Spacing).toInt();

    /* Calculate Preview group elements: */
    QList<DetailsElementType> inGroup;
    QList<DetailsElementType> outGroup;
    int iAdditionalGroupHeight = 0;
    int iAdditionalPreviewHeight = 0;
    enumerateLayoutItems(inGroup, outGroup, iAdditionalGroupHeight, iAdditionalPreviewHeight);

    /* Take into account all the elements: */
    int iMinimumHeightHintInGroup = 0;
    int iMinimumHeightHintOutGroup = 0;
    int iMinimumHeightHintPreview = 0;
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* For each particular element: */
        UIDetailsElement *pElement = pItem->toElement();
        const DetailsElementType enmElementType = pElement->elementType();
        switch (enmElementType)
        {
            case DetailsElementType_General:
            case DetailsElementType_System:
            case DetailsElementType_Display:
            case DetailsElementType_Storage:
            case DetailsElementType_Audio:
            case DetailsElementType_Network:
            case DetailsElementType_Serial:
            case DetailsElementType_USB:
            case DetailsElementType_SF:
            case DetailsElementType_UI:
            case DetailsElementType_Description:
            {
                if (inGroup.contains(enmElementType))
                {
                    iMinimumHeightHintInGroup += (pItem->minimumHeightHint() + iSpacing);
                    if (inGroup.last() == enmElementType)
                        iMinimumHeightHintInGroup += iAdditionalGroupHeight;
                }
                else if (outGroup.contains(enmElementType))
                    iMinimumHeightHintOutGroup += (pItem->minimumHeightHint() + iSpacing);
                break;
            }
            case DetailsElementType_Preview:
            {
                iMinimumHeightHintPreview = pItem->minimumHeightHint() + iAdditionalPreviewHeight;
                break;
            }
            default: AssertFailed(); break; /* Shut up, MSC! */
        }
    }

    /* Minus last spacing: */
    iMinimumHeightHintInGroup -= iSpacing;
    iMinimumHeightHintOutGroup -= iSpacing;

    /* Calculate minimum height hint: */
    int iMinimumHeightHint = qMax(iMinimumHeightHintInGroup, iMinimumHeightHintPreview);

    /* Spacing if necessary: */
    if (!inGroup.isEmpty() && !outGroup.isEmpty())
        iMinimumHeightHint += iSpacing;

    /* Spacing if necessary: */
    if (!outGroup.isEmpty())
        iMinimumHeightHint += iMinimumHeightHintOutGroup;

    /* And two margins finally: */
    iMinimumHeightHint += 2 * iMargin;

    /* Return result: */
    return iMinimumHeightHint;
}

void UIDetailsSet::sltMachineStateChange(const QUuid &uId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != uId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::sltMachineAttributesChange(const QUuid &uId)
{
    /* Is this our VM changed? */
    if (m_machine.GetId() != uId)
        return;

    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::sltUpdateAppearance()
{
    /* Update appearance: */
    rebuildSet();
}

void UIDetailsSet::prepareSet()
{
    /* Setup size-policy: */
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void UIDetailsSet::prepareConnections()
{
    /* Global-events connections: */
    connect(gVBoxEvents, SIGNAL(sigMachineStateChange(QUuid, KMachineState)), this, SLOT(sltMachineStateChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigMachineDataChange(QUuid)), this, SLOT(sltMachineAttributesChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigSessionStateChange(QUuid, KSessionState)), this, SLOT(sltMachineAttributesChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotTake(QUuid, QUuid)), this, SLOT(sltMachineAttributesChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotDelete(QUuid, QUuid)), this, SLOT(sltMachineAttributesChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotChange(QUuid, QUuid)), this, SLOT(sltMachineAttributesChange(QUuid)));
    connect(gVBoxEvents, SIGNAL(sigSnapshotRestore(QUuid, QUuid)), this, SLOT(sltMachineAttributesChange(QUuid)));

    /* Meidum-enumeration connections: */
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationStarted()), this, SLOT(sltUpdateAppearance()));
    connect(&vboxGlobal(), SIGNAL(sigMediumEnumerationFinished()), this, SLOT(sltUpdateAppearance()));
}

QVariant UIDetailsSet::data(int iKey) const
{
    /* Provide other members with required data: */
    switch (iKey)
    {
        /* Layout hints: */
        case SetData_Margin: return 1;
        case SetData_Spacing: return 1;
        /* Default: */
        default: break;
    }
    return QVariant();
}

void UIDetailsSet::rebuildSet()
{
    /* Make sure we have details: */
    if (!m_fHasDetails)
        return;

    /* Recache properties: */
    m_configurationAccessLevel = m_pMachineItem->configurationAccessLevel();

    /* Cleanup build-step: */
    delete m_pBuildStep;
    m_pBuildStep = 0;

    /* Generate new set-id: */
    m_uSetId = QUuid::createUuid();

    /* Request to build first step: */
    emit sigBuildStep(m_uSetId, DetailsElementType_General);
}

UIDetailsElement *UIDetailsSet::createElement(DetailsElementType enmElementType, bool fOpen)
{
    /* Element factory: */
    switch (enmElementType)
    {
        case DetailsElementType_General:     return new UIDetailsElementGeneral(this, fOpen);
        case DetailsElementType_System:      return new UIDetailsElementSystem(this, fOpen);
        case DetailsElementType_Preview:     return new UIDetailsElementPreview(this, fOpen);
        case DetailsElementType_Display:     return new UIDetailsElementDisplay(this, fOpen);
        case DetailsElementType_Storage:     return new UIDetailsElementStorage(this, fOpen);
        case DetailsElementType_Audio:       return new UIDetailsElementAudio(this, fOpen);
        case DetailsElementType_Network:     return new UIDetailsElementNetwork(this, fOpen);
        case DetailsElementType_Serial:      return new UIDetailsElementSerial(this, fOpen);
        case DetailsElementType_USB:         return new UIDetailsElementUSB(this, fOpen);
        case DetailsElementType_SF:          return new UIDetailsElementSF(this, fOpen);
        case DetailsElementType_UI:          return new UIDetailsElementUI(this, fOpen);
        case DetailsElementType_Description: return new UIDetailsElementDescription(this, fOpen);
        default:                             AssertFailed(); break; /* Shut up, MSC! */
    }
    return 0;
}

void UIDetailsSet::enumerateLayoutItems(QList<DetailsElementType> &inGroup,
                                        QList<DetailsElementType> &outGroup,
                                        int &iAdditionalGroupHeight,
                                        int &iAdditionalPreviewHeight) const
{
    /* Prepare variables: */
    const int iSpacing = data(SetData_Spacing).toInt();
    const UIDetailsElement *pPreviewElement = element(DetailsElementType_Preview);
    const bool fPreviewVisible = pPreviewElement && pPreviewElement->isVisible();
    const int iPreviewHeight = fPreviewVisible ? pPreviewElement->minimumHeightHint() : 0;
    int iGroupHeight = 0;

    /* Enumerate all the items: */
    foreach (UIDetailsItem *pItem, items())
    {
        /* Skip hidden: */
        if (!pItem->isVisible())
            continue;

        /* Acquire element and its type: */
        const UIDetailsElement *pElement = pItem->toElement();
        const DetailsElementType enmElementType = pElement->elementType();

        /* Skip Preview: */
        if (enmElementType == DetailsElementType_Preview)
            continue;

        /* Acquire element height: */
        const int iElementHeight = pElement->minimumHeightHint();

        /* Advance cumulative height if necessary: */
        if (   fPreviewVisible
            && outGroup.isEmpty()
            && (   iGroupHeight == 0
                || iGroupHeight + iElementHeight / 2 < iPreviewHeight))
        {
            iGroupHeight += iElementHeight;
            iGroupHeight += iSpacing;
            inGroup << enmElementType;
        }
        else
            outGroup << enmElementType;
    }
    /* Minus last spacing: */
    iGroupHeight -= iSpacing;

    /* Calculate additional height: */
    if (iPreviewHeight > iGroupHeight)
        iAdditionalGroupHeight = iPreviewHeight - iGroupHeight;
    else
        iAdditionalPreviewHeight = iGroupHeight - iPreviewHeight;
}

void UIDetailsSet::paintBackground(QPainter *pPainter, const QStyleOptionGraphicsItem *pOptions) const
{
    /* Save painter: */
    pPainter->save();

    /* Prepare variables: */
    const QRect optionRect = pOptions->rect;

    /* Paint default background: */
    const QColor defaultColor = palette().color(QPalette::Active, QPalette::Midlight).darker(110);
    pPainter->fillRect(optionRect, defaultColor);

    /* Restore painter: */
    pPainter->restore();
}
