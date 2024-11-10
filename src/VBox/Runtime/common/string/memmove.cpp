/* $Id$ */
/** @file
 * IPRT - CRT Strings, memmove().
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
 * Copies a memory.
 *
 * @returns pvDst.
 * @param   pvDst       Pointer to the target block.
 * @param   pvSrc       Pointer to the source block.
 * @param   cb          The size of the block.
 */
#ifdef IPRT_NO_CRT
# undef memmove
void *RT_NOCRT(memmove)(void *pvDst, const void *pvSrc, size_t cb)
#elif RT_MSC_PREREQ(RT_MSC_VER_VS2005)
_CRT_INSECURE_DEPRECATE_MEMORY(memmove_s) void * __cdecl
memmove(__out_bcount_full_opt(_Size) void *pvDst, __in_bcount_opt(_Size) const void *pvSrc, __in size_t cb)
#else
void *memmove(void *pvDst, const void *pvSrc, size_t cb)
#endif
{
    union
    {
        uint8_t volatile  *pu8;
        size_t volatile   *puz;
        void              *pv;
        uintptr_t          uPtr;
    } uTrg;
    uTrg.pv = pvDst;

    union
    {
        uint8_t const volatile  *pu8;
        size_t const volatile   *puz;
        void const              *pv;
        uintptr_t                uPtr;
    } uSrc;
    uSrc.pv = pvSrc;

    if (uTrg.uPtr < uSrc.uPtr || uTrg.uPtr >= uSrc.uPtr + cb)
    {
        /*
         * Copy forwards.
         */
        /* size word moves. */
        size_t c = cb / sizeof(size_t);
        while (c-- > 0)
            *uTrg.puz++ = *uSrc.puz++;

        /* Remaining byte moves. */
        c = cb % sizeof(*uTrg.puz);
        while (c-- > 0)
            *uTrg.pu8++ = *uSrc.pu8++;
    }
    else if (uTrg.uPtr != uSrc.uPtr)
    {
        /*
         * Copy backwards.
         */
        uTrg.uPtr += cb;
        uSrc.uPtr += cb;

        /* Byte stuff. */
        size_t c = cb % sizeof(*uTrg.puz);
        while (c-- > 0)
            *--uTrg.pu8 = *--uSrc.pu8;

        /* size word moves. */
        c = cb / sizeof(size_t);
        while (c-- > 0)
            *--uTrg.puz = *--uSrc.puz;
    }

    return pvDst;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(memmove);

