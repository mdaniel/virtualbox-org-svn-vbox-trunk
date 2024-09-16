/* $Id$ */
/** @file
 * IPRT - CRT Strings, strstr().
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
 * Find the location of @a pszSubStr in @a pszString.
 *
 * @returns Pointer to first occurence of the substring in @a pszString, NULL if
 *          not found.
 * @param   pszString   Zero terminated string to search.
 * @param   pszSubStr   The substring to search for.
 */
#undef strstr
char *RT_NOCRT(strstr)(const char *pszString, const char *pszSubStr)
{
    char const  ch0Sub = *pszSubStr;
    if (ch0Sub != '\0')
    {
        pszString = strchr(pszString, ch0Sub);
        if (pszString)
        {
            size_t const cchSubStr = strlen(pszSubStr);
            do
            {
                if (strncmp(pszString, pszSubStr, cchSubStr) == 0)
                    return (char *)pszString;
                if (ch0Sub)
                    pszString = strchr(pszString + 1, ch0Sub);
                else
                    break;
            } while (pszString != NULL);
        }
        return NULL;
    }
    return (char *)pszString;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strstr);

