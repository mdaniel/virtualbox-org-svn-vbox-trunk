/* $Id$ */
/** @file
 * VBox Qt GUI - UINetworkCustomer class implementation.
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

/* GUI includes: */
#include "UINetworkCustomer.h"
#include "UINetworkRequestManager.h"


UINetworkCustomer::UINetworkCustomer()
{
}

UINetworkCustomer::~UINetworkCustomer()
{
    emit sigBeingDestroyed(this);
}

void UINetworkCustomer::createNetworkRequest(UINetworkRequestType enmType,
                                             const QList<QUrl> urls,
                                             const QString &strTarget /* = QString() */,
                                             const UserDictionary requestHeaders /* = UserDictionary() */)
{
    m_uId = gNetworkManager->createNetworkRequest(enmType, urls, strTarget, requestHeaders, this);
}

void UINetworkCustomer::cancelNetworkRequest()
{
    gNetworkManager->cancelNetworkRequest(m_uId);
}
