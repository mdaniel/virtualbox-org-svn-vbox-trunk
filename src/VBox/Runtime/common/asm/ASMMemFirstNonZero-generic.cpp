/* $Id$ */
/** @file
 * IPRT - ASMMemZeroPage - generic C implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
#include <iprt/asm-mem.h>
#include "internal/iprt.h"


RTDECL(void RT_FAR *) ASMMemFirstNonZero(void const RT_FAR *pv, size_t cb) RT_NOTHROW_DEF
{
    uint8_t const *pb = (uint8_t const RT_FAR *)pv;

    /*
     * If we've got a large buffer to search, do it in larger units.
     */
    if (cb >= sizeof(size_t) * 2)
    {
        /* Align the pointer: */
        while ((uintptr_t)pb & (sizeof(size_t) - 1))
        {
            if (RT_LIKELY(*pb == 0))
            { /* likely */ }
            else
                return (void RT_FAR *)pb;
            cb--;
            pb++;
        }

        /* Scan in size_t sized words: */
        while (   cb >= sizeof(size_t)
               && *(size_t *)pb == 0)
        {
            pb += sizeof(size_t);
            cb -= sizeof(size_t);
        }
    }

    /*
     * Search byte by byte.
     */
    for (; cb; cb--, pb++)
        if (RT_LIKELY(*pb == 0))
        { /* likely */ }
        else
            return (void RT_FAR *)pb;

    return NULL;
}

