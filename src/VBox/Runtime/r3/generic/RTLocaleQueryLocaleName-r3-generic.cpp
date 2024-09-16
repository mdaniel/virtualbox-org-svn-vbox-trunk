/* $Id$ */
/** @file
 * IPRT - RTLocaleQueryLocaleName, ring-3 generic.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <locale.h>

#include <iprt/locale.h>
#include "internal/iprt.h"

#include <iprt/errcore.h>
#include <iprt/string.h>
#ifdef RT_OS_SOLARIS
#include <iprt/path.h>
#endif /* RT_OS_SOLARIS */



RTDECL(int) RTLocaleQueryLocaleName(char *pszName, size_t cbName)
{
    const char *pszLocale = setlocale(LC_ALL, NULL);
    if (pszLocale)
    {
#ifdef RT_OS_SOLARIS /* Solaris can return a locale starting with a slash ('/'), e.g. /en_GB.UTF-8/C/C/C/C/C */
        if (RTPATH_IS_SLASH(*pszLocale))
            pszLocale++;
#endif /* RT_OS_SOLARIS */
        return RTStrCopy(pszName, cbName, pszLocale);
    }
    return VERR_NOT_AVAILABLE;
}

