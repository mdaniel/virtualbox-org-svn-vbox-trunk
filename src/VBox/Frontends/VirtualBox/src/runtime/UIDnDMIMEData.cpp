/* $Id$ */
/** @file
 * VBox Qt GUI - UIDnDMIMEData class implementation.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
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
# include <QFileInfo>
# include <QMimeData>
# include <QStringList>
# include <QUrl>

/* GUI includes: */
# include "UIDnDMIMEData.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>
#include <iprt/errcore.h>


UIDnDMIMEData::UIDnDMIMEData(UIDnDHandler *pDnDHandler,
                             QStringList lstFormats, Qt::DropAction defAction, Qt::DropActions actions)
    : m_pDnDHandler(pDnDHandler)
    , m_lstFormats(lstFormats)
    , m_defAction(defAction)
    , m_curAction(Qt::IgnoreAction)
    , m_actions(actions)
    , m_enmState(Dragging)
{
    LogFlowThisFuncEnter();
#ifdef DEBUG
    LogFlowFunc(("Number of formats: %d\n", m_lstFormats.size()));
    for (int i = 0; i < m_lstFormats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, m_lstFormats.at(i).toUtf8().constData()));
#endif
}

QStringList UIDnDMIMEData::formats(void) const
{
    LogFlowFuncEnter();
#ifdef DEBUG
    for (int i = 0; i < m_lstFormats.size(); i++)
        LogFlowFunc(("\tFormat %d: %s\n", i, m_lstFormats.at(i).toUtf8().constData()));
#endif
    return m_lstFormats;
}

bool UIDnDMIMEData::hasFormat(const QString &strMIMEType) const
{
    RT_NOREF(strMIMEType);

    bool fRc;
#ifdef RT_OS_DARWIN
    fRc = m_lstFormats.contains(strMIMEType);
#else
    fRc = m_curAction != Qt::IgnoreAction;
#endif

    LogFlowFunc(("%s: %RTbool (QtMimeData: %RTbool, curAction=0x%x)\n",
                 strMIMEType.toStdString().c_str(), fRc, QMimeData::hasFormat(strMIMEType), m_curAction));

    return fRc;
}

/**
 * Called by Qt's drag'n drop operation (QDrag) for retrieving the actual drag'n drop
 * data in case of a successful drag'n drop operation.
 *
 * @param strMIMEType           MIME type string.
 * @param vaType                Variant containing the actual data based on the MIME type.
 *
 * @return QVariant
 */
QVariant UIDnDMIMEData::retrieveData(const QString &strMIMEType, QVariant::Type vaType) const
{
    LogFlowFunc(("state=%RU32, curAction=0x%x, defAction=0x%x, mimeType=%s, type=%d (%s)\n",
                 m_enmState, m_curAction, m_defAction, strMIMEType.toStdString().c_str(), vaType, QVariant::typeToName(vaType)));

    int rc = VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    /*
     * On Windows this function will be called several times by Qt's
     * OLE-specific internals to figure out which data formats we have
     * to offer. So just assume we can drop data here for a start.
     */
#elif defined(RT_OS_DARWIN)
# ifndef VBOX_WITH_DRAG_AND_DROP_PROMISES
    /*
     * Without VBOX_WITH_DRAG_AND_DROP_PROMISES being set in VBox *and* in our (patched) Qt
     * libraries there's no reliable way to get this working on OS X. So just deny any dropping.
     */
    rc = VERR_NOT_IMPLEMENTED;

    /* Let the user know. */
    LogRel(("DnD: Drag and drop support for OS X is not available in this version\n"));
# endif /* VBOX_WITH_DRAG_AND_DROP_PROMISES */
#else /* !RT_OS_DARWIN */
    /*
     * On Linux/Solaris our state gets updated if the drop target has been
     * changed. So query the current status if we're at the moment are able
     * to drop something on the current target.
     */
    if (m_curAction == Qt::IgnoreAction)
    {
        LogFlowFunc(("Current drop action is 0x%x, so can't drop yet\n", m_curAction));
        rc = VERR_NOT_FOUND;
    }
#endif

    if (RT_SUCCESS(rc))
    {
        /* Silently ignore internal Qt types / converters. */
        if (!strMIMEType.compare("application/x-qt-mime-type-name", Qt::CaseInsensitive))
        {
            rc = VERR_NOT_FOUND;
        }
        /* Do we support the requested MIME type? */
        else if (!m_lstFormats.contains(strMIMEType))
        {
            LogRel(("DnD: Unsupported MIME type '%s'\n", strMIMEType.toStdString().c_str()));
            rc = VERR_NOT_SUPPORTED;
        }
#ifndef RT_OS_DARWIN /* On OS X QVariant::Invalid can happen for drag and drop "promises" for "lazy requests".  */
        /* Check supported variant types. */
        else if (!(
                   /* Plain text. */
                      vaType == QVariant::String
                   /* Binary data. */
                   || vaType == QVariant::ByteArray
                   /* URI list. */
                   || vaType == QVariant::List
                   || vaType == QVariant::StringList))
        {
            LogRel(("DnD: Unsupported data type '%s'\n", QVariant::typeToName(vaType)));
            rc = VERR_NOT_SUPPORTED;
        }
#endif
    }

    LogRel3(("DnD: Retrieved data state is %ld (action=0x%x), rc=%Rrc\n", m_enmState, m_curAction, rc));

    if (RT_SUCCESS(rc))
    {
        QVariant vaData;
        rc = emit sigGetData(Qt::CopyAction, strMIMEType, vaType, vaData);
        if (RT_SUCCESS(rc))
        {
            LogRel3(("DnD: Returning data for MIME type=%s, variant type=%s, rc=%Rrc\n",
                     strMIMEType.toStdString().c_str(), QVariant::typeToName(vaData.type()), rc));

            return vaData;
        }
    }
    else if (rc == VERR_NOT_FOUND) /* Silently skip internal entries. */
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        LogRel2(("DnD: Retrieving data failed with %Rrc\n", rc));

    return QVariant(QVariant::Invalid);
}

/* static */
QVariant::Type UIDnDMIMEData::getVariantType(const QString &strMIMEType)
{
    QVariant::Type vaType;

    if (   !strMIMEType.compare("text/html")
        || !strMIMEType.compare("text/plain;charset=utf-8")
        || !strMIMEType.compare("text/plain;charset=utf-16")
        || !strMIMEType.compare("text/plain")
        || !strMIMEType.compare("text/richtext")
        || !strMIMEType.compare("UTF8_STRING")
        || !strMIMEType.compare("TEXT")
        || !strMIMEType.compare("STRING"))
    {
        vaType = QVariant::String;
    }
    else if (!strMIMEType.compare("text/uri-list", Qt::CaseInsensitive))
        vaType = QVariant::List;
    else
        vaType = QVariant::Invalid;

    LogFlowFunc(("strMIMEType=%s -> vaType=%s\n", qPrintable(strMIMEType), QVariant::typeToName(vaType)));
    return vaType;
}

/* static */
int UIDnDMIMEData::getDataAsVariant(const QVector<uint8_t> &vecData,
                                    const QString          &strMIMEType,
                                          QVariant::Type    vaType,
                                          QVariant         &vaData)
{
    RT_NOREF(strMIMEType);
    LogFlowFunc(("vecDataSize=%d, strMIMEType=%s vaType=%s\n",
                 vecData.size(), qPrintable(strMIMEType), QVariant::typeToName(vaType)));

    int rc = VINF_SUCCESS;

    switch (vaType)
    {
        case QVariant::String:
        {
            vaData = QVariant::fromValue(QString(reinterpret_cast<const char *>(vecData.constData())));
            Assert(vaData.type() == QVariant::String);
            break;
        }

        case QVariant::ByteArray:
        {
            QByteArray ba(reinterpret_cast<const char*>(vecData.constData()), vecData.size());

            vaData = QVariant::fromValue(ba);
            Assert(vaData.type() == QVariant::ByteArray);
            break;
        }

        /* See: https://developer.apple.com/library/ios/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html */
        case QVariant::List: /* Used on OS X for representing URI lists. */
        {
            QString strData = QString(reinterpret_cast<const char*>(vecData.constData()));
            QStringList lstString = strData.split("\r\n", QString::SkipEmptyParts);

            QVariantList lstVariant;

            Q_FOREACH(const QString& strCur, lstString)
            {
                QVariant vaURL = QVariant::fromValue(QUrl(strCur));
                Assert(vaURL.type() == QVariant::Url);
                lstVariant.append(vaURL);
            }

            vaData = QVariant::fromValue(lstVariant);
            Assert(vaData.type() == QVariant::List);
            break;
        }

        case QVariant::StringList:
        {
            QString strData = QString(reinterpret_cast<const char*>(vecData.constData()));
            QStringList lstString = strData.split("\r\n", QString::SkipEmptyParts);

            LogFlowFunc(("\tStringList has %d entries\n", lstString.size()));
# ifdef DEBUG
            Q_FOREACH(const QString& strCur, lstString)
                LogFlowFunc(("\t\tString: %s\n", qPrintable(strCur)));
# endif
            vaData = QVariant::fromValue(lstString);
            Assert(vaData.type() == QVariant::StringList);
            break;
        }

        default:
        {
            LogRel2(("DnD: Converting data (%d bytes) from guest to variant type '%s' not supported\n",
                     vecData.size(), QVariant::typeToName(vaType) ? QVariant::typeToName(vaType) : "<Invalid>"));

            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Issued by the QDrag object as soon as the current drop action has changed.
 *
 * @param dropAction            New drop action to use.
 */
void UIDnDMIMEData::sltDropActionChanged(Qt::DropAction dropAction)
{
    LogFlowFunc(("dropAction=0x%x\n", dropAction));
    m_curAction = dropAction;
}

