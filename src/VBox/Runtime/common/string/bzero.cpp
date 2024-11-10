/* $Id$ */
/** @file
 * IPRT - CRT Strings, bzero().
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
 * Fill a memory block with zeros.
 *
 * @returns pvDst.
 * @param   pvDst   Pointer to the block.
 * @param   cb      The size of the block.
 */
#undef bzero
void *RT_NOCRT(bzero)(void *pvDst, size_t cb)
{
    union
    {
        uint8_t  *pu8;
        size_t   *puz;
        void     *pvDst;
        uintptr_t uPtr;
    } u;
    u.pvDst = pvDst;

    /* Align the attack. */
    if (cb > sizeof(size_t) * 2 && (u.uPtr % sizeof(size_t)))
    {
        size_t c = sizeof(size_t) - (u.uPtr % sizeof(size_t));
        cb -= c;
        while (c-- > 0)
            *u.pu8++ = 0;
    }

    /* size_t sized moves. */
    size_t c = cb / sizeof(size_t);
    while (c-- > 0)
        *u.puz++ = 0;

    /* Remaining byte moves. */
    c = cb % sizeof(size_t);
    while (c-- > 0)
        *u.pu8++ = 0;

    return pvDst;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(bzero);
