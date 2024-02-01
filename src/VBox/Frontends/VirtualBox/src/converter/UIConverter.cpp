/* $Id$ */
/** @file
 * VBox Qt GUI - UIConverter implementation.
 */

/*
 * Copyright (C) 2012-2023 Oracle and/or its affiliates.
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
#include "UIConverter.h"

/* Other VBox includes: */
#include "iprt/assert.h"


/* static */
UIConverter* UIConverter::s_pInstance = 0;

/* static */
void UIConverter::create()
{
    AssertReturnVoid(!s_pInstance);
    new UIConverter;
}

/* static */
void UIConverter::destroy()
{
    AssertPtrReturnVoid(s_pInstance);
    delete s_pInstance;
}
