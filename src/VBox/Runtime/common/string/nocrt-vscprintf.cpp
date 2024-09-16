/* $Id$ */
/** @file
 * IPRT - No-CRT Strings, vscprintf().
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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
#define IPRT_NO_CRT_FOR_3RD_PARTY
#include "internal/iprt.h"
#include <iprt/nocrt/stdio.h>
#include <iprt/nocrt/limits.h>
#include <iprt/string.h>


/**
 * @callback_method_impl{FNRTSTROUTPUT, Dummy that only return cbChars.}
 */
static DECLCALLBACK(size_t) rtStrDummyOutputter(void *pvArg, const char *pachChars, size_t cbChars)
{
    RT_NOREF(pvArg, pachChars);
    return cbChars;
}


#undef vscprintf
int RT_NOCRT(vscprintf)(const char *pszFormat, va_list va)
{
    size_t cchOutput = RTStrFormatV(rtStrDummyOutputter, NULL, NULL, NULL, pszFormat, va);
    return cchOutput < (size_t)INT_MAX ? (int)cchOutput : INT_MAX - 1;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(vscprintf);

