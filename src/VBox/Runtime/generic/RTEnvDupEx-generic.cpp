/* $Id$ */
/** @file
 * IPRT - Environment, RTEnvDupEx, generic.
 */

/*
 * Copyright (C) 2010-2024 Oracle and/or its affiliates.
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
#include <iprt/env.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/string.h>



RTDECL(char *) RTEnvDupEx(RTENV Env, const char *pszVar)
{
    /*
     * Try with a small buffer.  This helps avoid allocating a heap buffer for
     * variables that doesn't exist.
     */
    char szSmall[256];
    int rc = RTEnvGetEx(Env, pszVar, szSmall, sizeof(szSmall), NULL);
    if (RT_SUCCESS(rc))
        return RTStrDup(szSmall);
    if (rc != VERR_BUFFER_OVERFLOW)
        return NULL;

    /*
     * It's a big bugger.
     */
    size_t cbBuf = _1K;
    do
    {
        char *pszBuf = RTStrAlloc(cbBuf);
        AssertBreak(pszBuf);

        rc = RTEnvGetEx(Env, pszVar, pszBuf, cbBuf, NULL);
        if (RT_SUCCESS(rc))
            return pszBuf;

        RTStrFree(pszBuf);

        /* If overflow double the buffer. */
        if (rc != VERR_BUFFER_OVERFLOW)
            break;
        cbBuf *= 2;
    } while (cbBuf < _64M);

    return NULL;
}
RT_EXPORT_SYMBOL(RTEnvDupEx);

