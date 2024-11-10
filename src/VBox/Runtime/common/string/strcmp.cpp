/* $Id$ */
/** @file
 * IPRT - CRT Strings, strcmp().
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
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
#include "internal/iprt.h"
#include <iprt/string.h>


/**
 * Compares two strings.
 *
 * @returns -1, 0 or 1.
 * @param   psz1    Left side string.
 * @param   psz2    Right side stirng.
 */
#ifdef IPRT_NO_CRT
# undef strcmp
int RT_NOCRT(strcmp)(const char *psz1, const char *psz2)
#else
int strcmp(const char *psz1, const char *psz2)
#endif
{
    for (;;)
    {
        char const ch1 = *psz1++;
        char const ch2 = *psz2++;
        if (ch1 == ch2)
        {
            if (ch1 != 0)
            { /* likely*/ }
            else
                return 0;
        }
        else
            return ch1 < ch2 ? -1 : 1;
    }
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strcmp);

