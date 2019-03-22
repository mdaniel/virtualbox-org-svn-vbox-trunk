/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediaComboBox class implementation.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
# include <QAbstractItemView>
# include <QDir>
# include <QFileInfo>

/* GUI includes: */
# include "UIMediaComboBox.h"
# include "UIMedium.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMediaComboBox::UIMediaComboBox(QWidget *pParent)
    : QComboBox(pParent)
    , m_enmMediaType(UIMediumDeviceType_Invalid)
    , m_uMachineId(QUuid())
    , m_uLastItemId(QUuid())
{
    /* Prepare: */
    prepare();
}

void UIMediaComboBox::refresh()
{
    /* Clearing lists: */
    clear(), m_media.clear();

    /* Use the medium creation handler to add all the items:  */
    foreach (const QUuid &uMediumId, vboxGlobal().mediumIDs())
        sltHandleMediumCreated(uMediumId);

    /* If at least one real medium present, remove null medium: */
    if (count() > 1 && m_enmMediaType == UIMediumDeviceType_HardDisk)
    {
        removeItem(0);
        m_media.erase(m_media.begin());
    }

    /* Notify listeners about active item changed. */
    emit activated(currentIndex());
}

void UIMediaComboBox::repopulate()
{
    if (!vboxGlobal().isMediumEnumerationInProgress())
        vboxGlobal().startMediumEnumeration();
    else
        refresh();
}

void UIMediaComboBox::setCurrentItem(const QUuid &aItemId)
{
    m_uLastItemId = aItemId;

    int iIndex;
    // WORKAROUND:
    // Note that the media combo-box may be not populated here yet,
    // so we don't assert..
    if (findMediaIndex(aItemId, iIndex))
    {
        QComboBox::setCurrentIndex(iIndex);
        emit activated(iIndex);
    }
}

QUuid UIMediaComboBox::id(int iIndex /* = -1 */) const
{
    AssertReturn(iIndex == -1 ||
                 (iIndex >= 0 && iIndex < m_media.size()),
                  QUuid());

    if (iIndex == -1)
        iIndex = currentIndex();
    return iIndex == -1 ? QUuid() : m_media.at(iIndex).id;
}

QString UIMediaComboBox::location(int iIndex /* = -1 */) const
{
    AssertReturn(iIndex == -1 ||
                 (iIndex >= 0 && iIndex < m_media.size()),
                  QString());

    if (iIndex == -1)
        iIndex = currentIndex();
    return iIndex == -1 ? QString() : m_media.at(iIndex).location;
}

void UIMediaComboBox::sltHandleMediumCreated(const QUuid &aMediumId)
{
    /* Search for corresponding medium: */
    UIMedium guiMedium = vboxGlobal().medium(aMediumId);

    /* Ignore media (and their children) which are
     * marked as hidden or attached to hidden machines only: */
    if (UIMedium::isMediumAttachedToHiddenMachinesOnly(guiMedium))
        return;

    /* Add only 1. NULL medium and 2. media of required type: */
    if (!guiMedium.isNull() && guiMedium.type() != m_enmMediaType)
        return;

    /* Ignore all diffs: */
    if (guiMedium.type() == UIMediumDeviceType_HardDisk && guiMedium.parentID() != UIMedium::nullID())
        return;

    /* Append medium into combo-box: */
    appendItem(guiMedium);

    /* Activate the required item if any: */
    if (guiMedium.id() == m_uLastItemId)
        setCurrentItem(guiMedium.id());
    /* Select last added item if there is no item selected: */
    else if (currentText().isEmpty())
        QComboBox::setCurrentIndex(count() - 1);
}

void UIMediaComboBox::sltHandleMediumEnumerated(const QUuid &aMediumId)
{
    /* Search for corresponding medium: */
    UIMedium guiMedium = vboxGlobal().medium(aMediumId);

    /* Add only 1. NULL medium and 2. media of required type: */
    if (!guiMedium.isNull() && guiMedium.type() != m_enmMediaType)
        return;

    /* Search for corresponding item index: */
    int iIndex;
    if (!findMediaIndex(guiMedium.id(), iIndex))
        return;

    /* Replace medium in combo-box: */
    replaceItem(iIndex, guiMedium);

    /* Ensure the parent dialog handles the change of the selected item's data: */
    emit activated(currentIndex());
}

void UIMediaComboBox::sltHandleMediumDeleted(const QUuid &aMediumId)
{
    /* Search for corresponding item index: */
    int iIndex;
    if (!findMediaIndex(aMediumId, iIndex))
        return;

    /* Replace medium from combo-box: */
    removeItem(iIndex);
    m_media.erase(m_media.begin() + iIndex);

    /* If no real medium left, add the NULL medium: */
    if (count() == 0)
        sltHandleMediumCreated(UIMedium::nullID());

    /* Ensure the parent dialog handles the change of the selected item: */
    emit activated(currentIndex());
}

void UIMediaComboBox::sltHandleMediumEnumerationStart()
{
    refresh();
}

void UIMediaComboBox::sltHandleComboActivated(int iIndex)
{
    AssertReturnVoid(iIndex >= 0 && iIndex < m_media.size());

    m_uLastItemId = m_media.at(iIndex).id;

    updateToolTip(iIndex);
}

void UIMediaComboBox::sltHandleComboHovered(const QModelIndex &index)
{
    /* Set the combo-box item's tooltip: */
    const int iIndex = index.row();
    view()->viewport()->setToolTip(QString());
    view()->viewport()->setToolTip(m_media.at(iIndex).toolTip);
}

void UIMediaComboBox::prepare()
{
    /* Setup the elide mode: */
    view()->setTextElideMode(Qt::ElideRight);
    QSizePolicy sp1(QSizePolicy::Ignored, QSizePolicy::Fixed, QSizePolicy::ComboBox);
    sp1.setHorizontalStretch(2);
    setSizePolicy(sp1);

    /* Setup medium-processing handlers: */
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumCreated,
            this, &UIMediaComboBox::sltHandleMediumCreated);
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumDeleted,
            this, &UIMediaComboBox::sltHandleMediumDeleted);

    /* Setup medium-enumeration handlers: */
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumEnumerationStarted,
            this, &UIMediaComboBox::sltHandleMediumEnumerationStart);
    connect(&vboxGlobal(), &VBoxGlobal::sigMediumEnumerated,
            this, &UIMediaComboBox::sltHandleMediumEnumerated);

    /* Setup other connections: */
    connect(this, static_cast<void(UIMediaComboBox::*)(int)>(&UIMediaComboBox::activated),
            this, &UIMediaComboBox::sltHandleComboActivated);
    connect(view(), &QAbstractItemView::entered,
            this, &UIMediaComboBox::sltHandleComboHovered);
}

void UIMediaComboBox::updateToolTip(int iIndex)
{
    /* Set the combo-box tooltip: */
    setToolTip(QString());
    if (iIndex >= 0 && iIndex < m_media.size())
        setToolTip(m_media.at(iIndex).toolTip);
}

void UIMediaComboBox::appendItem(const UIMedium &guiMedium)
{
    m_media.append(Medium(guiMedium.id(), guiMedium.location(),
                          guiMedium.toolTipCheckRO(true, false)));

    insertItem(count(), guiMedium.iconCheckRO(true), guiMedium.details(true));
}

void UIMediaComboBox::replaceItem(int iIndex, const UIMedium &guiMedium)
{
    AssertReturnVoid(iIndex >= 0 && iIndex < m_media.size());

    m_media[iIndex].id = guiMedium.id();
    m_media[iIndex].location = guiMedium.location();
    m_media[iIndex].toolTip = guiMedium.toolTipCheckRO(true, false);

    setItemText(iIndex, guiMedium.details(true));
    setItemIcon(iIndex, guiMedium.iconCheckRO(true));

    if (iIndex == currentIndex())
        updateToolTip(iIndex);
}

bool UIMediaComboBox::findMediaIndex(const QUuid &aId, int &iIndex)
{
    iIndex = 0;

    for (; iIndex < m_media.size(); ++ iIndex)
        if (m_media.at(iIndex).id == aId)
            break;

    return iIndex < m_media.size();
}
