/* $Id$ */
/** @file
 * VBox Qt GUI - UIMedium class implementation.
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
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
# include <QDir>
/* GUI includes: */
# include "UIMedium.h"
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIMessageCenter.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
/* COM includes: */
# include "CMachine.h"
# include "CSnapshot.h"
#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

QString UIMedium::m_sstrNullID = QUuid().toString().remove('{').remove('}');
QString UIMedium::m_sstrTable = QString("<table>%1</table>");
QString UIMedium::m_sstrRow = QString("<tr><td>%1</td></tr>");

UIMedium::UIMedium()
    : m_type(UIMediumType_Invalid)
    , m_medium(CMedium())
    , m_state(KMediumState_NotCreated)
    , m_enmMediumType(KMediumType_Max)
    , m_enmMediumVariant(KMediumVariant_Max)
{
    refresh();
}

UIMedium::UIMedium(const CMedium &medium, UIMediumType type)
    : m_type(type)
    , m_medium(medium)
    , m_state(KMediumState_NotCreated)
    , m_enmMediumType(KMediumType_Max)
    , m_enmMediumVariant(KMediumVariant_Max)
{
    refresh();
}

UIMedium::UIMedium(const CMedium &medium, UIMediumType type, KMediumState state)
    : m_type(type)
    , m_medium(medium)
    , m_state(state)
    , m_enmMediumType(KMediumType_Max)
    , m_enmMediumVariant(KMediumVariant_Max)
{
    refresh();
}

UIMedium::UIMedium(const UIMedium &other)
{
    *this = other;
}

UIMedium& UIMedium::operator=(const UIMedium &other)
{
    m_type = other.type();

    m_medium = other.medium();

    m_state = other.state();
    m_result = other.result();
    m_strLastAccessError = other.lastAccessError();

    m_strId = other.id();
    m_strRootId = other.rootID();
    m_strParentId = other.parentID();

    m_strKey = other.key();

    m_strName = other.name();
    m_strLocation = other.location();
    m_strDescription = other.description();

    m_uSize = other.sizeInBytes();
    m_uLogicalSize = other.logicalSizeInBytes();
    m_strSize = other.size();
    m_strLogicalSize = other.logicalSize();

    m_enmMediumType = other.mediumType();
    m_enmMediumVariant = other.mediumVariant();

    m_strHardDiskType = other.hardDiskType();
    m_strHardDiskFormat = other.hardDiskFormat();
    m_strStorageDetails = other.storageDetails();
    m_strEncryptionPasswordID = other.encryptionPasswordID();

    m_strUsage = other.usage();
    m_strToolTip = other.tip();
    m_machineIds = other.machineIds();
    m_curStateMachineIds = other.curStateMachineIds();

    m_noDiffs = other.cache();

    m_fHidden = other.m_fHidden;
    m_fUsedByHiddenMachinesOnly = other.m_fUsedByHiddenMachinesOnly;
    m_fReadOnly = other.isReadOnly();
    m_fUsedInSnapshots = other.isUsedInSnapshots();
    m_fHostDrive = other.isHostDrive();
    m_fEncrypted = other.isEncrypted();

    return *this;
}

void UIMedium::blockAndQueryState()
{
    /* Ignore for NULL medium: */
    if (m_medium.isNull())
        return;

    /* Acquire actual medium state: */
    m_state = m_medium.RefreshState();

    /* Save the result to distinguish between
     * inaccessible and e.g. uninitialized objects: */
    m_result = COMResult(m_medium);
    if (!m_result.isOk())
    {
        m_state = KMediumState_Inaccessible;
        m_strLastAccessError = QString();
    }
    else
        m_strLastAccessError = m_medium.GetLastAccessError();

    /* Refresh finally: */
    refresh();
}

void UIMedium::refresh()
{
    /* Reset ID parameters: */
    m_strId = nullID();
    m_strRootId = nullID();
    m_strParentId = nullID();

    /* Reset cache parameters: */
    //m_strKey = nullID();

    /* Reset name/location/description/size parameters: */
    m_strName = VBoxGlobal::tr("Empty", "medium");
    m_strLocation = m_strSize = m_strLogicalSize = QString("--");
    m_strDescription = QString();
    m_uSize = m_uLogicalSize = 0;

    /* Reset medium type & variant parameter: */
    m_enmMediumType = KMediumType_Max;
    m_enmMediumVariant = KMediumVariant_Max;

    /* Reset hard drive related parameters: */
    m_strHardDiskType = QString();
    m_strHardDiskFormat = QString();
    m_strStorageDetails = QString();
    m_strEncryptionPasswordID = QString();

    /* Reset data parameters: */
    m_strUsage = QString();
    m_strToolTip = QString();
    m_machineIds.clear();
    m_curStateMachineIds.clear();

    /* Reset m_noDiffs: */
    m_noDiffs.isSet = false;

    /* Reset flags: */
    m_fHidden = false;
    m_fUsedByHiddenMachinesOnly = false;
    m_fReadOnly = false;
    m_fUsedInSnapshots = false;
    m_fHostDrive = false;
    m_fEncrypted = false;

    /* For non NULL medium: */
    if (!m_medium.isNull())
    {
        /* Refresh medium ID: */
        m_strId = normalizedID(m_medium.GetId());
        /* Refresh root medium ID: */
        m_strRootId = m_strId;

        /* Init medium key if necessary: */
        if (m_strKey.isNull())
            m_strKey = m_strId;

        /* Check whether this is host-drive medium: */
        m_fHostDrive = m_medium.GetHostDrive();

        /* Refresh medium name: */
        if (!m_fHostDrive)
            m_strName = m_medium.GetName();
        else if (m_medium.GetDescription().isEmpty())
            m_strName = VBoxGlobal::tr("Host Drive '%1'", "medium").arg(QDir::toNativeSeparators(m_medium.GetLocation()));
        else
            m_strName = VBoxGlobal::tr("Host Drive %1 (%2)", "medium").arg(m_medium.GetDescription(), m_medium.GetName());
        /* Refresh medium location/description: */
        if (!m_fHostDrive)
        {
            m_strLocation = QDir::toNativeSeparators(m_medium.GetLocation());
            m_strDescription = m_medium.GetDescription();
        }

        /* Refresh medium size and logical size: */
        if (!m_fHostDrive)
        {
            /* Only for created and accessible mediums: */
            if (m_state != KMediumState_Inaccessible && m_state != KMediumState_NotCreated)
            {
                m_uSize = m_medium.GetSize();
                m_strSize = vboxGlobal().formatSize(m_uSize);
                if (m_type == UIMediumType_HardDisk)
                {
                    m_uLogicalSize = m_medium.GetLogicalSize();
                    m_strLogicalSize = vboxGlobal().formatSize(m_uLogicalSize);
                }
                else
                {
                    m_uLogicalSize = m_uSize;
                    m_strLogicalSize = m_strSize;
                }
            }
        }

        /* Refresh medium type & variant: */
        m_enmMediumType = m_medium.GetType();
        qlonglong iMediumVariant = 0;
        foreach (const KMediumVariant &enmVariant, m_medium.GetVariant())
            iMediumVariant |= enmVariant;
        m_enmMediumVariant = (KMediumVariant)iMediumVariant;

        /* For hard drive medium: */
        if (m_type == UIMediumType_HardDisk)
        {
            /* Refresh hard drive disk type: */
            m_strHardDiskType = mediumTypeToString(m_medium);
            /* Refresh hard drive format: */
            m_strHardDiskFormat = m_medium.GetFormat();

            /* Refresh hard drive storage details: */
            m_strStorageDetails = gpConverter->toString(m_enmMediumVariant);

            /* Check whether this is read-only hard drive: */
            m_fReadOnly = m_medium.GetReadOnly();

            /* Refresh parent hard drive ID: */
            CMedium parentMedium = m_medium.GetParent();
            if (!parentMedium.isNull())
                m_strParentId = normalizedID(parentMedium.GetId());

            /* Only for created and accessible mediums: */
            if (m_state != KMediumState_Inaccessible && m_state != KMediumState_NotCreated)
            {
                /* Refresh root hard drive ID: */
                while (!parentMedium.isNull())
                {
                    m_strRootId = normalizedID(parentMedium.GetId());
                    parentMedium = parentMedium.GetParent();
                }

                /* Refresh encryption attributes: */
                if (m_strRootId != m_strId)
                {
                    m_strEncryptionPasswordID = root().encryptionPasswordID();
                    m_fEncrypted = root().isEncrypted();
                }
                else
                {
                    QString strCipher;
                    CMedium medium(m_medium);
                    const QString strEncryptionPasswordID = medium.GetEncryptionSettings(strCipher);
                    if (medium.isOk())
                    {
                        m_strEncryptionPasswordID = strEncryptionPasswordID;
                        m_fEncrypted = true;
                    }
                }
            }
        }

        /* Check whether this is hidden medium: */
        QString strHints = m_medium.GetProperty("Special/GUI/Hints");
        if (!strHints.isEmpty())
        {
            QStringList hints(strHints.split(','));
            if (hints.contains("Hide", Qt::CaseInsensitive))
                m_fHidden = true;
        }

        /* Refresh usage data: */
        m_curStateMachineIds.clear();
        m_machineIds = m_medium.GetMachineIds().toList();
        if (m_machineIds.size() > 0)
        {
            /* Get CVirtualBox object: */
            CVirtualBox vbox = vboxGlobal().virtualBox();

            /* By default we assuming that this medium is attached
             * to 'hidden' machines only, if at least one machine present: */
            m_fUsedByHiddenMachinesOnly = true;

            /* Prepare machine usage: */
            QString strMachineUsage;
            /* Walk through all the machines this medium attached to: */
            foreach (const QString &strMachineID, m_machineIds)
            {
                /* Look for the corresponding machine: */
                CMachine machine = vbox.FindMachine(strMachineID);

                /* UIMedium object can wrap newly created CMedium object
                 * which belongs to not yet registered machine, like while creating VM clone.
                 * We can skip such a machines in usage string. */
                if (machine.isNull())
                {
                    /* Since we can't precisely check 'hidden' status for that machine in such case,
                     * we have to assume that medium attached not only to 'hidden' machines: */
                    m_fUsedByHiddenMachinesOnly = false;
                    continue;
                }

                /* Finally we can precisely check if current machine is 'hidden': */
                if (gEDataManager->showMachineInSelectorChooser(strMachineID))
                    m_fUsedByHiddenMachinesOnly = false;

                /* Prepare snapshot usage: */
                QString strSnapshotUsage;
                /* Walk through all the snapshots this medium attached to: */
                foreach (const QString &strSnapshotID, m_medium.GetSnapshotIds(strMachineID))
                {
                    if (strSnapshotID == strMachineID)
                    {
                        /* The medium is attached to the machine in the current
                         * state, we don't distinguish this for now by always
                         * giving the VM name in front of snapshot names. */
                        m_curStateMachineIds.push_back(strSnapshotID);
                        continue;
                    }

                    /* Look for the corresponding snapshot: */
                    CSnapshot snapshot = machine.FindSnapshot(strSnapshotID);

                    /* Snapshot can be NULL while takeSnaphot is in progress: */
                    if (snapshot.isNull())
                        continue;

                    /* Refresh snapshot usage flag: */
                    m_fUsedInSnapshots = true;

                    /* Append snapshot usage: */
                    if (!strSnapshotUsage.isNull())
                        strSnapshotUsage += ", ";
                    strSnapshotUsage += snapshot.GetName();
                }

                /* Append machine usage: */
                if (!strMachineUsage.isNull())
                    strMachineUsage += ", ";
                strMachineUsage += machine.GetName();

                /* Append snapshot usage: */
                if (!strSnapshotUsage.isNull())
                    strMachineUsage += QString(" (%2)").arg(strSnapshotUsage);
            }

            /* Append machine usage: */
            if (!strMachineUsage.isEmpty())
                m_strUsage += strMachineUsage;
        }

        /* Refresh tool-tip: */
        m_strToolTip = m_sstrRow.arg(QString("<p style=white-space:pre><b>%1</b></p>").arg(m_fHostDrive ? m_strName : m_strLocation));
        if (m_type == UIMediumType_HardDisk)
        {
            m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<p style=white-space:pre>Type (Format):  %1 (%2)</p>", "medium")
                                                         .arg(m_strHardDiskType).arg(m_strHardDiskFormat));
        }
        m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<p>Attached to:  %1</p>", "image")
                                                     .arg(m_strUsage.isNull() ? VBoxGlobal::tr("<i>Not Attached</i>", "image") : m_strUsage));
        switch (m_state)
        {
            case KMediumState_NotCreated:
            {
                m_strToolTip += m_sstrRow.arg(VBoxGlobal::tr("<i>Checking accessibility...</i>", "medium"));
                break;
            }
            case KMediumState_Inaccessible:
            {
                if (m_result.isOk())
                {
                    /* Not Accessible: */
                    m_strToolTip += m_sstrRow.arg("<hr>") + m_sstrRow.arg(VBoxGlobal::highlight(m_strLastAccessError, true /* aToolTip */));
                }
                else
                {
                    /* Accessibility check (eg GetState()) itself failed: */
                    m_strToolTip += m_sstrRow.arg("<hr>") + m_sstrRow.arg(VBoxGlobal::tr("Failed to check accessibility of disk image files.", "medium")) +
                                    m_sstrRow.arg(UIMessageCenter::formatErrorInfo(m_result) + ".");
                }
                break;
            }
            default:
                break;
        }
    }
}

void UIMedium::updateParentID()
{
    m_strParentId = nullID();
    if (m_type == UIMediumType_HardDisk)
    {
        CMedium parentMedium = m_medium.GetParent();
        if (!parentMedium.isNull())
            m_strParentId = normalizedID(parentMedium.GetId());
    }
}

QString UIMedium::toolTip(bool fNoDiffs /* = false */, bool fCheckRO /* = false */, bool fNullAllowed /* = false */) const
{
    QString strTip;

    if (m_medium.isNull())
    {
        strTip = fNullAllowed ? m_sstrRow.arg(VBoxGlobal::tr("<b>No disk image file selected</b>", "medium")) +
                                m_sstrRow.arg(VBoxGlobal::tr("You can also change this while the machine is running.")) :
                                m_sstrRow.arg(VBoxGlobal::tr("<b>No disk image files available</b>", "medium")) +
                                m_sstrRow.arg(VBoxGlobal::tr("You can create or add disk image files in the virtual machine settings."));
    }
    else
    {
        unconst(this)->checkNoDiffs(fNoDiffs);

        strTip = fNoDiffs ? m_noDiffs.toolTip : m_strToolTip;

        if (fCheckRO && m_fReadOnly)
            strTip += m_sstrRow.arg("<hr>") +
                      m_sstrRow.arg(VBoxGlobal::tr("Attaching this hard disk will be performed indirectly using "
                                                   "a newly created differencing hard disk.", "medium"));
    }

    return m_sstrTable.arg(strTip);
}

QPixmap UIMedium::icon(bool fNoDiffs /* = false */, bool fCheckRO /* = false */) const
{
    QPixmap pixmap;

    if (state(fNoDiffs) == KMediumState_Inaccessible)
        pixmap = result(fNoDiffs).isOk() ? vboxGlobal().warningIcon() : vboxGlobal().errorIcon();

    if (fCheckRO && m_fReadOnly)
    {
        QIcon icon = UIIconPool::iconSet(":/hd_new_16px.png");
        pixmap = VBoxGlobal::joinPixmaps(pixmap, icon.pixmap(icon.availableSizes().first()));
    }

    return pixmap;
}

QString UIMedium::details(bool fNoDiffs /* = false */,
                          bool fPredictDiff /* = false */,
                          bool fUseHTML /* = false */) const
{
    /// @todo the below check is rough; if m_medium becomes uninitialized, any
    // of getters called afterwards will also fail. The same relates to the
    // root hard drive object (that will be the hard drive itself in case of
    // non-differencing disks). However, this check was added to fix a
    // particular use case: when the hard drive is a differencing hard drive and
    // it happens to be discarded (and uninitialized) after this method is
    // called but before we read all its properties (yes, it's possible!), the
    // root object will be null and calling methods on it will assert in the
    // debug builds. This check seems to be enough as a quick solution (fresh
    // hard drive attachments will be re-read by a machine state change signal
    // after the discard operation is finished, so the user will eventually see
    // correct data), but in order to solve the problem properly we need to use
    // exceptions everywhere (or check the result after every method call). See
    // @bugref{2149}.

    if (m_medium.isNull() || m_fHostDrive)
        return m_strName;

    if (!m_medium.isOk())
        return QString();

    QString strDetails, strText;

    /* Note: root accessible only if medium enumerated: */
    UIMedium rootMedium = root();
    KMediumState eState = m_state;

    if (m_type == UIMediumType_HardDisk)
    {
        if (fNoDiffs)
        {
            bool isDiff = (!fPredictDiff && parentID() != nullID()) || (fPredictDiff && m_fReadOnly);

            strDetails = isDiff && fUseHTML ?
                QString("<i>%1</i>, ").arg(rootMedium.m_strHardDiskType) :
                QString("%1, ").arg(rootMedium.m_strHardDiskType);

            eState = this->state(true /* fNoDiffs */);

            if (rootMedium.m_state == KMediumState_NotCreated)
                eState = KMediumState_NotCreated;
        }
        else
        {
            strDetails = QString("%1, ").arg(rootMedium.m_strHardDiskType);
        }

        /* Add encryption status: */
        if (m_fEncrypted)
            strDetails += QString("%1, ").arg(VBoxGlobal::tr("Encrypted", "medium"));
    }

    /// @todo prepend the details with the warning/error icon when not accessible

    switch (eState)
    {
        case KMediumState_NotCreated:
            strText = VBoxGlobal::tr("Checking...", "medium");
            strDetails += fUseHTML ? QString("<i>%1</i>").arg(strText) : strText;
            break;
        case KMediumState_Inaccessible:
            strText = VBoxGlobal::tr("Inaccessible", "medium");
            strDetails += fUseHTML ? QString("<b>%1</b>").arg(strText) : strText;
            break;
        default:
            strDetails += m_type == UIMediumType_HardDisk ? rootMedium.m_strLogicalSize : rootMedium.m_strSize;
            break;
    }

    strDetails = fUseHTML ?
        QString("%1 (<nobr>%2</nobr>)").arg(VBoxGlobal::locationForHTML(rootMedium.m_strName), strDetails) :
        QString("%1 (%2)").arg(VBoxGlobal::locationForHTML(rootMedium.m_strName), strDetails);

    return strDetails;
}

/* static */
QString UIMedium::nullID()
{
    return m_sstrNullID;
}

/* static */
QString UIMedium::normalizedID(const QString &strID)
{
    /* Handle wrong UUID (null/empty or invalid format): */
    if (QUuid(strID).toString().remove('{').remove('}') != strID)
        return nullID();
    return strID;
}

/* static */
bool UIMedium::isMediumAttachedToHiddenMachinesOnly(const UIMedium &medium)
{
    /* Iterate till the root: */
    UIMedium mediumIterator = medium;
    do
    {
        /* Ignore medium if its hidden
         * or attached to hidden machines only: */
        if (mediumIterator.isHidden())
            return true;
        /* Move iterator to parent: */
        mediumIterator = mediumIterator.parent();
    }
    while (!mediumIterator.isNull());
    /* False by default: */
    return false;
}

UIMedium UIMedium::root() const
{
    /* Redirect call to VBoxGlobal: */
    return vboxGlobal().medium(m_strRootId);
}

UIMedium UIMedium::parent() const
{
    /* Redirect call to VBoxGlobal: */
    return vboxGlobal().medium(m_strParentId);
}

void UIMedium::checkNoDiffs(bool fNoDiffs)
{
    if (!fNoDiffs || m_noDiffs.isSet)
        return;

    m_noDiffs.toolTip = QString();

    m_noDiffs.state = m_state;
    for (UIMedium parentMedium = parent(); !parentMedium.isNull(); parentMedium = parentMedium.parent())
    {
        if (parentMedium.m_state == KMediumState_Inaccessible)
        {
            m_noDiffs.state = parentMedium.m_state;

            if (m_noDiffs.toolTip.isNull())
                m_noDiffs.toolTip = m_sstrRow.arg(VBoxGlobal::tr("Some of the files in this hard disk chain "
                                                                 "are inaccessible. Please use the Virtual Medium "
                                                                 "Manager to inspect these files.", "medium"));

            if (!parentMedium.m_result.isOk())
            {
                m_noDiffs.result = parentMedium.m_result;
                break;
            }
        }
    }

    if (parentID() != nullID() && !m_fReadOnly)
    {
        m_noDiffs.toolTip = root().tip() +
                            m_sstrRow.arg("<hr>") +
                            m_sstrRow.arg(VBoxGlobal::tr("This base hard disk is indirectly attached using "
                                                         "the following differencing hard disk:", "medium")) +
                            m_strToolTip + m_noDiffs.toolTip;
    }

    if (m_noDiffs.toolTip.isNull())
        m_noDiffs.toolTip = m_strToolTip;

    m_noDiffs.isSet = true;
}

/* static */
QString UIMedium::mediumTypeToString(const CMedium &comMedium)
{
    if (!comMedium.GetParent().isNull())
    {
        Assert(comMedium.GetType() == KMediumType_Normal);
        return QApplication::translate("VBoxGlobal", "Differencing", "MediumType");
    }
    return gpConverter->toString(comMedium.GetType());
}

