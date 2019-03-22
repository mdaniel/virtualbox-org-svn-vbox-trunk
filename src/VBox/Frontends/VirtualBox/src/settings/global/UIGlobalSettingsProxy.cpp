/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsProxy class implementation.
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
# include <QButtonGroup>
# include <QRegExpValidator>

/* GUI includes: */
# include "QIWidgetValidator.h"
# include "UIGlobalSettingsProxy.h"
# include "UIExtraDataManager.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** Global settings: Proxy page data structure. */
struct UIDataSettingsGlobalProxy
{
    /** Constructs data. */
    UIDataSettingsGlobalProxy()
        : m_enmProxyMode(KProxyMode_System)
        , m_strProxyHost(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSettingsGlobalProxy &other) const
    {
        return true
               && (m_enmProxyMode == other.m_enmProxyMode)
               && (m_strProxyHost == other.m_strProxyHost)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSettingsGlobalProxy &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSettingsGlobalProxy &other) const { return !equal(other); }

    /** Holds the proxy mode. */
    KProxyMode  m_enmProxyMode;
    /** Holds the proxy host. */
    QString     m_strProxyHost;
};


UIGlobalSettingsProxy::UIGlobalSettingsProxy()
    : m_pCache(0)
{
    /* Prepare: */
    prepare();
}

UIGlobalSettingsProxy::~UIGlobalSettingsProxy()
{
    /* Cleanup: */
    cleanup();
}

void UIGlobalSettingsProxy::loadToCacheFrom(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Clear cache initially: */
    m_pCache->clear();

    /* Prepare old proxy data: */
    UIDataSettingsGlobalProxy oldProxyData;

    /* Gather old proxy data: */
    oldProxyData.m_enmProxyMode = m_properties.GetProxyMode();
    oldProxyData.m_strProxyHost = m_properties.GetProxyURL();

    /* Cache old proxy data: */
    m_pCache->cacheInitialData(oldProxyData);

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

void UIGlobalSettingsProxy::getFromCache()
{
    /* Get old proxy data from the cache: */
    const UIDataSettingsGlobalProxy &oldProxyData = m_pCache->base();

    /* Load old proxy data from the cache: */
    switch (oldProxyData.m_enmProxyMode)
    {
        case KProxyMode_System:  m_pRadioProxyAuto->setChecked(true); break;
        case KProxyMode_NoProxy: m_pRadioProxyDisabled->setChecked(true); break;
        case KProxyMode_Manual:  m_pRadioProxyEnabled->setChecked(true); break;
        case KProxyMode_Max:     break; /* (compiler warnings) */
    }
    m_pHostEditor->setText(oldProxyData.m_strProxyHost);
    sltHandleProxyToggle();

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsProxy::putToCache()
{
    /* Prepare new proxy data: */
    UIDataSettingsGlobalProxy newProxyData = m_pCache->base();

    /* Gather new proxy data: */
    newProxyData.m_enmProxyMode = m_pRadioProxyEnabled->isChecked()  ? KProxyMode_Manual
                                : m_pRadioProxyDisabled->isChecked() ? KProxyMode_NoProxy : KProxyMode_System;
    newProxyData.m_strProxyHost = m_pHostEditor->text();

    /* Cache new proxy data: */
    m_pCache->cacheCurrentData(newProxyData);
}

void UIGlobalSettingsProxy::saveFromCacheTo(QVariant &data)
{
    /* Fetch data to properties: */
    UISettingsPageGlobal::fetchData(data);

    /* Update proxy data and failing state: */
    setFailed(!saveProxyData());

    /* Upload properties to data: */
    UISettingsPageGlobal::uploadData(data);
}

bool UIGlobalSettingsProxy::validate(QList<UIValidationMessage> &messages)
{
    /* Pass if proxy is disabled: */
    if (!m_pRadioProxyEnabled->isChecked())
        return true;

    /* Pass by default: */
    bool fPass = true;

    /* Prepare message: */
    UIValidationMessage message;

    /* Check for host value: */
    if (m_pHostEditor->text().trimmed().isEmpty())
    {
        message.second << tr("No proxy host is currently specified.");
        fPass = false;
    }

    /* Check for password presence: */
    if (!QUrl(m_pHostEditor->text().trimmed()).password().isEmpty())
    {
        message.second << tr("You have provided a proxy password. "
                             "Please be aware that the password will be saved in plain text. "
                             "You may wish to configure a system-wide proxy instead and not "
                             "store application-specific settings.");
        fPass = true;
    }

    /* Serialize message: */
    if (!message.second.isEmpty())
        messages << message;

    /* Return result: */
    return fPass;
}

void UIGlobalSettingsProxy::retranslateUi()
{
    /* Translate uic generated strings: */
    Ui::UIGlobalSettingsProxy::retranslateUi(this);
}

void UIGlobalSettingsProxy::sltHandleProxyToggle()
{
    /* Update widgets availability: */
    m_pContainerProxy->setEnabled(m_pRadioProxyEnabled->isChecked());

    /* Revalidate: */
    revalidate();
}

void UIGlobalSettingsProxy::prepare()
{
    /* Apply UI decorations: */
    Ui::UIGlobalSettingsProxy::setupUi(this);

    /* Prepare cache: */
    m_pCache = new UISettingsCacheGlobalProxy;
    AssertPtrReturnVoid(m_pCache);

    /* Layout created in the .ui file. */
    {
        /* Create button-group: */
        QButtonGroup *pButtonGroup = new QButtonGroup(this);
        AssertPtrReturnVoid(pButtonGroup);
        {
            /* Configure button-group: */
            pButtonGroup->addButton(m_pRadioProxyAuto);
            pButtonGroup->addButton(m_pRadioProxyDisabled);
            pButtonGroup->addButton(m_pRadioProxyEnabled);
            connect(pButtonGroup, SIGNAL(buttonClicked(QAbstractButton *)), this, SLOT(sltHandleProxyToggle()));
        }

        /* Host editor created in the .ui file. */
        AssertPtrReturnVoid(m_pHostEditor);
        {
            /* Configure editor: */
            m_pHostEditor->setValidator(new QRegExpValidator(QRegExp("\\S+"), m_pHostEditor));
            connect(m_pHostEditor, SIGNAL(textEdited(const QString &)), this, SLOT(revalidate()));
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UIGlobalSettingsProxy::cleanup()
{
    /* Cleanup cache: */
    delete m_pCache;
    m_pCache = 0;
}

bool UIGlobalSettingsProxy::saveProxyData()
{
    /* Prepare result: */
    bool fSuccess = true;
    /* Save proxy settings from the cache: */
    if (fSuccess && m_pCache->wasChanged())
    {
        /* Get old proxy data from the cache: */
        //const UIDataSettingsGlobalProxy &oldProxyData = m_pCache->base();
        /* Get new proxy data from the cache: */
        const UIDataSettingsGlobalProxy &newProxyData = m_pCache->data();

        /* Save new proxy data from the cache: */
        m_properties.SetProxyMode(newProxyData.m_enmProxyMode);
        fSuccess &= m_properties.isOk();
        m_properties.SetProxyURL(newProxyData.m_strProxyHost);
        fSuccess &= m_properties.isOk();

        /* Drop the old extra data setting if still around: */
        if (fSuccess && !gEDataManager->proxySettings().isEmpty())
            gEDataManager->setProxySettings(QString());
    }
    /* Return result: */
    return fSuccess;
}

