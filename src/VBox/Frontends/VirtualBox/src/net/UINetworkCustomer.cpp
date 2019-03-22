/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class implementation.
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
# include <QUrl>

/* Local includes: */
# include "UINetworkCustomer.h"
# include "UINetworkManager.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UINetworkCustomer::UINetworkCustomer(QObject *pParent /* = 0 */, bool fForceCall /* = true */)
    : QObject(pParent)
    , m_fForceCall(fForceCall)
{
}

void UINetworkCustomer::createNetworkRequest(UINetworkRequestType enmType, const QList<QUrl> urls,
                                             const UserDictionary requestHeaders /* = UserDictionary() */)
{
    gNetworkManager->createNetworkRequest(enmType, urls, requestHeaders, this);
}

